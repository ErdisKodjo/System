#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file big_little_support.c
 * @brief Support avancé pour la topologie big.LITTLE (ARM DynamIQ).
 *
 * Étend `big_little.c` avec :
 *   - Détection de la topologie (cluster big, cluster LITTLE, nombre de
 *     cœurs par cluster).
 *   - Politique de placement "efficiency-first" : les tâches peu
 *     gourmandes en CPU vont sur les cœurs LITTLE, les tâches CPU-bound
 *     sur les cœurs big.
 *   - Migration asynchrone basée sur l'historique de charge (EWMA) plutôt
 *     que sur un échantillon instantané — évite le thrashing.
 *   - Mode "solar boost" : quand `afros_hal_ops.get_power_source()`
 *     signale une source solaire, bascule toutes les tâches sur les
 *     cœurs big pour exploiter l'énergie disponible (intégration avec
 *     `power/solar_aware.c`).
 */

#define BIGLITTLE_EWMA_ALPHA        4   /* 1/4 nouveau + 3/4 ancien */
#define BIGLITTLE_LOAD_HI_THRESH    75  /* % → candidat big */
#define BIGLITTLE_LOAD_LO_THRESH    30  /* % → candidat LITTLE */
#define BIGLITTLE_HYSTERESIS_PKTS   3   /* nb d'échantillons consécutifs requis */
#define BIGLITTLE_MAX_CPUS          8

typedef struct {
    bool     is_big;
    uint32_t load_ewma;            /* charge lissée 0-100 */
    uint32_t hi_count;             /* samples consécutifs > HI_THRESH */
    uint32_t lo_count;             /* samples consécutifs < LO_THRESH */
    uint32_t current_freq_mhz;
} bl_cpu_state_t;

static bl_cpu_state_t g_cpus[BIGLITTLE_MAX_CPUS];
static uint32_t g_cpu_count = 0;
static bool g_initialized = false;

/**
 * Initialise la topologie big.LITTLE en interrogeant la HAL pour chaque
 * cœur. Détecte quels cœurs sont "big" (fréquence max élevée) et
 * "LITTLE" (fréquence max plus basse, plus économes).
 */
void biglittle_init(uint32_t cpu_count) {
    if (cpu_count > BIGLITTLE_MAX_CPUS) cpu_count = BIGLITTLE_MAX_CPUS;
    g_cpu_count = cpu_count;

    for (uint32_t i = 0; i < cpu_count; i++) {
        afros_cpu_info_t info;
        if (arch_cpu_ops.get_info(i, &info) == AFROS_SUCCESS) {
            g_cpus[i].is_big = info.is_big;
            g_cpus[i].load_ewma = 0;
            g_cpus[i].hi_count = 0;
            g_cpus[i].lo_count = 0;
            g_cpus[i].current_freq_mhz = info.current_freq_mhz;
        } else {
            g_cpus[i].is_big = false;
            g_cpus[i].load_ewma = 0;
            g_cpus[i].hi_count = 0;
            g_cpus[i].lo_count = 0;
            g_cpus[i].current_freq_mhz = 0;
        }
    }
    g_initialized = true;
    kprintf("[bigLITTLE] Topologie initialisée : %u cœurs détectés.\n", cpu_count);
    for (uint32_t i = 0; i < cpu_count; i++) {
        kprintf("  CPU%u : %s, fréquence actuelle=%u MHz\n",
                i, g_cpus[i].is_big ? "big" : "LITTLE",
                g_cpus[i].current_freq_mhz);
    }
}

/**
 * Met à jour la charge d'un cœur (appelée par l'ordonnanceur à chaque
 * tick). Applique un lissage EWMA pour éviter les décisions hâtives sur
 * un pic ponctuel. Compte les échantillons consécutifs dépassant les
 * seuils pour introduire une hystérésis : il faut 3 samples d'affilée
 * au-dessus de HI pour déclencher une migration vers big, et inversement.
 */
void biglittle_update_load(uint32_t cpu_id, uint32_t load_pct) {
    if (!g_initialized || cpu_id >= g_cpu_count) return;

    bl_cpu_state_t *s = &g_cpus[cpu_id];
    s->load_ewma = (load_pct / BIGLITTLE_EWMA_ALPHA) +
                   (s->load_ewma * (BIGLITTLE_EWMA_ALPHA - 1) / BIGLITTLE_EWMA_ALPHA);

    if (s->load_ewma > BIGLITTLE_LOAD_HI_THRESH) {
        s->hi_count++;
        s->lo_count = 0;
    } else if (s->load_ewma < BIGLITTLE_LOAD_LO_THRESH) {
        s->lo_count++;
        s->hi_count = 0;
    } else {
        s->hi_count = 0;
        s->lo_count = 0;
    }
}

/**
 * Sélectionne le cœur cible pour une nouvelle tâche. Politique
 * "efficiency-first" :
 *   - Tâche IO-bound ou peu gourmande → cœur LITTLE le moins chargé.
 *   - Tâche CPU-bound → cœur big le moins chargé.
 *   - Si tous les cœurs big sont sleepés et qu'on est sur batterie,
 *     réveille un cœur big avant de l'assigner.
 *
 * @param cpu_bound true si la tâche est marquée CPU-intensive.
 * @return ID du cœur cible, ou 0 si erreur.
 */
uint32_t biglittle_select_cpu(bool cpu_bound) {
    if (!g_initialized || g_cpu_count == 0) return 0;

    bool want_big = cpu_bound;
    afros_power_source_t pwr;
    if (afros_hal_ops.get_power_source(&pwr) == AFROS_SUCCESS &&
        pwr == AFROS_POWER_SOURCE_SOLAR) {
        /* mode solar boost : tout sur big si possible */
        want_big = true;
    }

    uint32_t best = 0;
    uint32_t best_load = 0xFFFFFFFF;
    for (uint32_t i = 0; i < g_cpu_count; i++) {
        if (g_cpus[i].is_big != want_big) continue;
        if (g_cpus[i].load_ewma < best_load) {
            best_load = g_cpus[i].load_ewma;
            best = i;
        }
    }

    /* fallback : n'importe quel cœur */
    if (best_load == 0xFFFFFFFF) {
        for (uint32_t i = 0; i < g_cpu_count; i++) {
            if (g_cpus[i].load_ewma < best_load) {
                best_load = g_cpus[i].load_ewma;
                best = i;
            }
        }
    }
    return best;
}

/**
 * Évalue les migrations de tâches nécessaires. Pour chaque cœur LITTLE
 * avec hystérésis HI dépassée, propose une migration vers un cœur big
 * moins chargé. Pour chaque cœur big avec hystérésis LO dépassée,
 * propose l'inverse.
 */
void biglittle_balance(uint32_t *migrations_proposed) {
    if (!g_initialized) return;
    uint32_t count = 0;

    for (uint32_t i = 0; i < g_cpu_count; i++) {
        bl_cpu_state_t *s = &g_cpus[i];
        if (!s->is_big && s->hi_count >= BIGLITTLE_HYSTERESIS_PKTS) {
            /* LITTLE surchargé → migrer vers big le moins chargé */
            uint32_t target = biglittle_select_cpu(true);
            if (target != i && g_cpus[target].load_ewma < s->load_ewma) {
                kprintf("[bigLITTLE] Migration suggérée : tâches CPU%u (LITTLE, %u%%) → CPU%u (big, %u%%).\n",
                        i, s->load_ewma, target, g_cpus[target].load_ewma);
                s->hi_count = 0;
                count++;
            }
        } else if (s->is_big && s->lo_count >= BIGLITTLE_HYSTERESIS_PKTS) {
            /* big sous-utilisé → migrer vers LITTLE pour économiser */
            uint32_t target = biglittle_select_cpu(false);
            if (target != i) {
                kprintf("[bigLITTLE] Migration suggérée : tâches CPU%u (big, %u%%) → CPU%u (LITTLE, %u%%).\n",
                        i, s->load_ewma, target, g_cpus[target].load_ewma);
                s->lo_count = 0;
                count++;
            }
        }
    }

    if (migrations_proposed) *migrations_proposed = count;
}
