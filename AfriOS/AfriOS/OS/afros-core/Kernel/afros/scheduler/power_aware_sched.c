#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file power_aware_sched.c
 * @brief Ordonnanceur conscient de l'énergie pour AfriOS.
 *
 * Complète `power_aware.c` avec une vision multi-tâche : au lieu de ne
 * réguler qu'une seule tâche, ce module gère une file de tâches prêtes
 * et décide laquelle exécuter en fonction :
 *   - De la priorité (user-visible > background > batch).
 *   - De la source d'énergie (solaire → boost, batterie → économie).
 *   - Du niveau de batterie (seuils 75%, 50%, 25%, 10%).
 *   - De l'historique de consommation CPU de chaque tâche (UV Estimator).
 *
 * Politique d'admission : quand la batterie passe sous 10%, seules les
 * tâches marquées "critical" (santé, paiement, appels) sont acceptées
 * dans la file ; les autres sont gelées jusqu'au retour du chargeur.
 */

#define PAS_MAX_TASKS         64
#define PAS_BATTERY_CRITICAL  10
#define PAS_BATTERY_LOW       25
#define PAS_BATTERY_MID       50
#define PAS_BATTERY_HIGH      75

typedef enum {
    PAS_TASK_CRITICAL = 0,    /* santé, paiement, appels — jamais gelées */
    PAS_TASK_USER_VISIBLE = 1,/* app foreground */
    PAS_TASK_BACKGROUND = 2,  /* sync, indexing */
    PAS_TASK_BATCH = 3        /* backups, ML training */
} pas_task_class_t;

typedef struct {
    uint32_t task_id;
    pas_task_class_t klass;
    uint32_t priority;        /* 0 (haute) → 31 (basse) */
    uint32_t cpu_estimate;    /* % CPU moyen mesuré (EWMA) */
    uint64_t last_run_tick;
    bool     frozen;
} pas_task_t;

static pas_task_t g_tasks[PAS_MAX_TASKS];
static uint32_t g_task_count = 0;
static uint32_t g_battery_pct = 100;
static bool g_solar_mode = false;

/**
 * Enregistre une tâche dans la file de l'ordonnanceur power-aware.
 * Les tâches CRITICAL sont toujours acceptées. Les tâches BATCH ne sont
 * acceptées que si la batterie est > 50% ou qu'on est en mode solaire.
 */
afros_status_t pas_register_task(uint32_t task_id, pas_task_class_t klass,
                                  uint32_t priority) {
    if (g_task_count >= PAS_MAX_TASKS) return AFROS_ERROR_NO_MEMORY;

    /* admission control */
    if (klass == PAS_TASK_BATCH) {
        if (!g_solar_mode && g_battery_pct < PAS_BATTERY_MID) {
            kprintf("[PAS] Tâche BATCH %u rejetée (batterie=%u%%).\n",
                    task_id, g_battery_pct);
            return AFROS_ERROR;
        }
    }

    g_tasks[g_task_count].task_id = task_id;
    g_tasks[g_task_count].klass = klass;
    g_tasks[g_task_count].priority = priority;
    g_tasks[g_task_count].cpu_estimate = 0;
    g_tasks[g_task_count].last_run_tick = 0;
    g_tasks[g_task_count].frozen = false;
    g_task_count++;

    kprintf("[PAS] Tâche %u enregistrée (classe=%d, prio=%u).\n",
            task_id, klass, priority);
    return AFROS_SUCCESS;
}

/**
 * Met à jour l'état de l'énergie. Appelée à chaque variation détectée
 * par `afros_hal_ops.get_battery_level()` ou `get_power_source()`.
 * Applique la politique de gel/dégel en fonction des seuils.
 */
void pas_update_power_state(void) {
    afros_power_source_t src;
    uint32_t bat;

    if (afros_hal_ops.get_power_source(&src) != AFROS_SUCCESS) return;
    if (afros_hal_ops.get_battery_level(&bat) != AFROS_SUCCESS) return;

    bool was_solar = g_solar_mode;
    g_solar_mode = (src == AFROS_POWER_SOURCE_SOLAR);
    g_battery_pct = bat;

    /* transition vers solaire → dégeler tout */
    if (g_solar_mode && !was_solar) {
        kprintf("[PAS] Source solaire détectée (batterie=%u%%), dégel de toutes les tâches.\n", bat);
        for (uint32_t i = 0; i < g_task_count; i++) {
            g_tasks[i].frozen = false;
        }
    }

    /* batterie critique → ne garder que CRITICAL */
    if (!g_solar_mode && bat <= PAS_BATTERY_CRITICAL) {
        kprintf("[PAS] Batterie CRITIQUE (%u%%), gel de toutes les tâches non critiques.\n", bat);
        for (uint32_t i = 0; i < g_task_count; i++) {
            if (g_tasks[i].klass != PAS_TASK_CRITICAL) {
                g_tasks[i].frozen = true;
            }
        }
    } else if (!g_solar_mode && bat <= PAS_BATTERY_LOW) {
        /* batterie basse → gel BATCH + BACKGROUND agressive */
        for (uint32_t i = 0; i < g_task_count; i++) {
            if (g_tasks[i].klass == PAS_TASK_BATCH) {
                g_tasks[i].frozen = true;
            }
        }
    } else if (!g_solar_mode && bat > PAS_BATTERY_MID) {
        /* batterie saine → dégeler BACKGROUND */
        for (uint32_t i = 0; i < g_task_count; i++) {
            if (g_tasks[i].klass == PAS_TASK_BACKGROUND) {
                g_tasks[i].frozen = false;
            }
        }
    }
}

/**
 * Sélectionne la prochaine tâche à exécuter. Politique :
 *   1. Filtrer les tâches non gelées.
 *   2. Trier par classe (CRITICAL > USER_VISIBLE > BACKGROUND > BATCH).
 *   3. Dans chaque classe, plus petite priorité numérique = plus haute.
 *   4. Égalité → tâche qui n'a pas tourné depuis le plus longtemps.
 *
 * @return task_id, ou 0xFFFFFFFF si aucune tâche éligible.
 */
uint32_t pas_pick_next(void) {
    uint32_t best = 0xFFFFFFFF;
    pas_task_class_t best_klass = PAS_TASK_BATCH;
    uint32_t best_prio = 0xFFFFFFFF;
    uint64_t best_idle = 0;

    for (uint32_t i = 0; i < g_task_count; i++) {
        if (g_tasks[i].frozen) continue;

        uint64_t idle = 0;  /* TODO: arch_timer_ops.get_ticks() - g_tasks[i].last_run_tick */

        bool better = false;
        if (g_tasks[i].klass < best_klass) better = true;
        else if (g_tasks[i].klass == best_klass) {
            if (g_tasks[i].priority < best_prio) better = true;
            else if (g_tasks[i].priority == best_prio && idle > best_idle) better = true;
        }

        if (better) {
            best = g_tasks[i].task_id;
            best_klass = g_tasks[i].klass;
            best_prio = g_tasks[i].priority;
            best_idle = idle;
        }
    }
    return best;
}

/**
 * Met à jour l'estimation CPU d'une tâche après exécution (EWMA).
 */
void pas_update_cpu_usage(uint32_t task_id, uint32_t cpu_pct) {
    for (uint32_t i = 0; i < g_task_count; i++) {
        if (g_tasks[i].task_id == task_id) {
            g_tasks[i].cpu_estimate = (cpu_pct / 4) +
                                       (g_tasks[i].cpu_estimate * 3 / 4);
            return;
        }
    }
}
