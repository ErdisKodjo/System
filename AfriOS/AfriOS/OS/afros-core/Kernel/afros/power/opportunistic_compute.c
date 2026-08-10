#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file opportunistic_compute.c
 * @brief Calcul opportuniste : exploite les fenêtres d'énergie solaire
 *        ou de charge CPU faible pour exécuter des tâches différées.
 *
 * Le calcul opportuniste est la pierre angulaire de l'efficacité
 * énergétique d'AfriOS en zones rurales :
 *   - Quand `afros_hal_ops.get_power_source()` signale une source
 *     solaire ET que la batterie est pleine (>90%), on déclenche les
 *     tâches BATCH gelées : backups, indexation, ML inference.
 *   - Inversement, dès qu'un capteur signale un seuil critique, on
 *     suspend tout calcul non essentiel dans la minute.
 *   - Un budget CPU par tâche est alloué dynamiquement : si la fenêtre
 *     solaire dure 2h, on répartit 2h × nb_cœurs × 80% entre les
 *     tâches BATCH en attente (20% gardé en réserve pour le système).
 */

#define OC_MAX_BATCH_TASKS     32
#define OC_BATTERY_FULL_THRESH 90
#define OC_CPU_BUDGET_RESERVE  20   /* 20% réservé au système */
#define OC_WINDOW_MINUTES_MIN  5    /* fenêtre minimum rentable */

typedef struct {
    uint32_t task_id;
    uint32_t cpu_budget_ms;     /* budget CPU alloué pour la fenêtre */
    uint32_t cpu_used_ms;       /* CPU consommé */
    bool     running;
} oc_batch_t;

static oc_batch_t g_batch[OC_MAX_BATCH_TASKS];
static uint32_t g_batch_count = 0;
static bool g_window_open = false;

/**
 * Enregistre une tâche BATCH pour exécution opportuniste. La tâche ne
 * s'exécutera que lorsqu'une fenêtre d'énergie favorable s'ouvre.
 */
afros_status_t oc_register_batch(uint32_t task_id) {
    if (g_batch_count >= OC_MAX_BATCH_TASKS) return AFROS_ERROR_NO_MEMORY;
    g_batch[g_batch_count].task_id = task_id;
    g_batch[g_batch_count].cpu_budget_ms = 0;
    g_batch[g_batch_count].cpu_used_ms = 0;
    g_batch[g_batch_count].running = false;
    g_batch_count++;
    kprintf("[OC] Tâche BATCH %u enregistrée pour calcul opportuniste.\n", task_id);
    return AFROS_SUCCESS;
}

/**
 * Évalue si une fenêtre de calcul opportuniste peut s'ouvrir. Les
 * critères sont :
 *   - Source d'énergie solaire active.
 *   - Batterie > 90% (pour absorber les fluctuations nuageuses).
 *   - Au moins une tâche BATCH en attente.
 *   - Charge CPU système < 30% (pour ne pas gêner les tâches user).
 *
 * Si la fenêtre s'ouvre, alloue un budget CPU par tâche BATCH
 * proportionnel à la durée estimée de la fenêtre solaire (basée sur
 * l'heure locale — TODO: brancher sur `afros_hal_ops` ou un RTC).
 */
void oc_evaluate_window(void) {
    afros_power_source_t src;
    uint32_t bat;

    if (afros_hal_ops.get_power_source(&src) != AFROS_SUCCESS) return;
    if (afros_hal_ops.get_battery_level(&bat) != AFROS_SUCCESS) return;

    bool can_open = (src == AFROS_POWER_SOURCE_SOLAR) &&
                    (bat >= OC_BATTERY_FULL_THRESH) &&
                    (g_batch_count > 0);

    if (can_open && !g_window_open) {
        g_window_open = true;
        /* estimer la durée de la fenêtre : TODO via position solaire,
         * pour l'instant on suppose 60 minutes. */
        uint32_t window_minutes = 60;
        if (window_minutes < OC_WINDOW_MINUTES_MIN) return;

        uint32_t total_cpu_ms = window_minutes * 60 * 1000 *
                                (100 - OC_CPU_BUDGET_RESERVE) / 100;
        uint32_t per_task = total_cpu_ms / g_batch_count;

        for (uint32_t i = 0; i < g_batch_count; i++) {
            g_batch[i].cpu_budget_ms = per_task;
            g_batch[i].cpu_used_ms = 0;
            g_batch[i].running = true;
        }

        kprintf("[OC] Fenêtre de calcul opportuniste OUVERTE (%u min, %u ms/task).\n",
                window_minutes, per_task);
    } else if (!can_open && g_window_open) {
        /* fermeture de la fenêtre */
        g_window_open = false;
        for (uint32_t i = 0; i < g_batch_count; i++) {
            if (g_batch[i].running) {
                kprintf("[OC] Suspension tâche %u (consommé %u/%u ms).\n",
                        g_batch[i].task_id, g_batch[i].cpu_used_ms,
                        g_batch[i].cpu_budget_ms);
                g_batch[i].running = false;
            }
        }
        kprintf("[OC] Fenêtre de calcul opportuniste FERMÉE.\n");
    }
}

/**
 * Tick appelé à chaque tranche de temps CPU. Si une fenêtre est ouverte,
 * dépile les tâches BATCH éligibles et les exécute jusqu'à épuisement
 * du budget alloué.
 *
 * @return task_id à exécuter, ou 0xFFFFFFFF si aucune tâche BATCH
 *         n'est éligible.
 */
uint32_t oc_pick_batch_task(void) {
    if (!g_window_open) return 0xFFFFFFFF;

    for (uint32_t i = 0; i < g_batch_count; i++) {
        if (g_batch[i].running && g_batch[i].cpu_used_ms < g_batch[i].cpu_budget_ms) {
            return g_batch[i].task_id;
        }
    }
    /* tous les budgets épuisés : fermer la fenêtre */
    if (g_window_open) {
        g_window_open = false;
        kprintf("[OC] Tous les budgets BATCH épuisés, fenêtre fermée.\n");
    }
    return 0xFFFFFFFF;
}

/**
 * Comptabilise le temps CPU consommé par une tâche BATCH. Appelée après
 * chaque tranche.
 */
void oc_account_cpu(uint32_t task_id, uint32_t cpu_ms) {
    for (uint32_t i = 0; i < g_batch_count; i++) {
        if (g_batch[i].task_id == task_id) {
            g_batch[i].cpu_used_ms += cpu_ms;
            if (g_batch[i].cpu_used_ms >= g_batch[i].cpu_budget_ms) {
                g_batch[i].running = false;
                kprintf("[OC] Tâche %u a consommé tout son budget (%u ms).\n",
                        task_id, g_batch[i].cpu_budget_ms);
            }
            return;
        }
    }
}
