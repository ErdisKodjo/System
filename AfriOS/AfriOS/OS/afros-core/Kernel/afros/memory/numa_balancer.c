#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file numa_balancer.c
 * @brief Équilibrage NUMA pour serveurs multi-socket.
 *
 * Sur les serveurs ARM multi-socket (typique des centres de données
 * africains émergents qui utilisent des SoCs Ampere Altra ou des
 * clusters de Raspberry Pi 5 avec topologie NUMA émulée via PCIe), la
 * localité mémoire NUMA est cruciale : un accès cross-socket coûte
 * 2 à 4x plus cher qu'un accès local.
 *
 * Ce module :
 *   - Découvre la topologie NUMA en interrogeant le HAL (`get_info` sur
 *     chaque CPU pour récupérer `cluster_id` qui sert de node NUMA).
 *   - Maintient un compteur de fautes de page par (task, node) pour
 *     détecter les accès distants.
 *   - Migre les pages mémoire d'une tâche vers le node où elle s'exécute
 *     le plus souvent ("task home node").
 *   - Avertit l'ordonnanceur quand une tâche a sa majorité de pages
 *     sur un node distant (pour déclencher une migration de tâche
 *     plutôt que de pages — moins coûteux).
 */

#define NB_MAX_NODES            8
#define NB_MAX_TASKS            128
#define NB_MIGRATION_THRESHOLD  70   /* % de pages distantes → migration */

typedef struct {
    uint32_t task_id;
    uint32_t home_node;              /* node où la tâche s'exécute le + souvent */
    uint32_t pages_local[NB_MAX_NODES];
    uint32_t pages_remote[NB_MAX_NODES];
    uint32_t total_page_faults;
    uint32_t migrations_triggered;
} nb_task_stats_t;

static nb_task_stats_t g_tasks[NB_MAX_TASKS];
static uint32_t g_task_count = 0;
static uint32_t g_node_count = 0;
static bool g_initialized = false;

/**
 * Initialise le balancer NUMA en découvrant la topologie via le HAL.
 * Pour chaque cœur, on récupère son `cluster_id` qui sert d'identifiant
 * de node NUMA. Le nombre de nodes distincts est calculé à la volée.
 */
void numa_balancer_init(uint32_t cpu_count) {
    bool node_seen[NB_MAX_NODES] = {false};
    g_node_count = 0;

    for (uint32_t i = 0; i < cpu_count && i < 64; i++) {
        afros_cpu_info_t info;
        if (arch_cpu_ops.get_info(i, &info) == AFROS_SUCCESS) {
            if (info.cluster_id < NB_MAX_NODES && !node_seen[info.cluster_id]) {
                node_seen[info.cluster_id] = true;
                g_node_count++;
            }
        }
    }

    g_task_count = 0;
    g_initialized = true;
    kprintf("[NUMA] Topologie : %u node(s) distinct(s) sur %u cœurs.\n",
            g_node_count, cpu_count);
}

/**
 * Enregistre une tâche auprès du balancer NUMA. La tâche est initialement
 * assignée au node 0 ; son "home node" sera recalculé après quelques
 * milliers de fautes de page.
 */
afros_status_t numa_balancer_register_task(uint32_t task_id, uint32_t home_node) {
    if (g_task_count >= NB_MAX_TASKS) return AFROS_ERROR_NO_MEMORY;
    if (home_node >= g_node_count) home_node = 0;

    g_tasks[g_task_count].task_id = task_id;
    g_tasks[g_task_count].home_node = home_node;
    g_tasks[g_task_count].total_page_faults = 0;
    g_tasks[g_task_count].migrations_triggered = 0;
    for (uint32_t i = 0; i < NB_MAX_NODES; i++) {
        g_tasks[g_task_count].pages_local[i] = 0;
        g_tasks[g_task_count].pages_remote[i] = 0;
    }
    g_task_count++;
    return AFROS_SUCCESS;
}

/**
 * Enregistre une faute de page pour une tâche. Si la page est sur le
 * node local de la tâche, on incrémente `pages_local` ; sinon on
 * incmente `pages_remote`. Au-delà d'un seuil, on déclenche une
 * migration de pages (ou une migration de tâche si la majorité est
 * sur un autre node).
 */
void numa_balancer_account_fault(uint32_t task_id, uint32_t faulting_cpu,
                                  uint32_t page_node) {
    nb_task_stats_t *t = NULL;
    for (uint32_t i = 0; i < g_task_count; i++) {
        if (g_tasks[i].task_id == task_id) {
            t = &g_tasks[i];
            break;
        }
    }
    if (!t) return;

    afros_cpu_info_t cpu_info;
    uint32_t cpu_node = 0;
    if (arch_cpu_ops.get_info(faulting_cpu, &cpu_info) == AFROS_SUCCESS) {
        cpu_node = cpu_info.cluster_id;
    }

    if (cpu_node == page_node) {
        t->pages_local[cpu_node]++;
    } else {
        t->pages_remote[page_node]++;
    }
    t->total_page_faults++;

    /* évaluation périodique toutes les 1024 fautes */
    if ((t->total_page_faults & 0x3FF) != 0) return;

    uint32_t total = t->total_page_faults;
    uint32_t remote_total = 0;
    for (uint32_t i = 0; i < g_node_count; i++) {
        if (i != t->home_node) {
            remote_total += t->pages_remote[i];
        }
    }
    uint32_t remote_pct = (remote_total * 100) / (total > 0 ? total : 1);

    if (remote_pct > NB_MIGRATION_THRESHOLD) {
        /* identifier le node distant dominant */
        uint32_t best_node = t->home_node;
        uint32_t best_pages = 0;
        for (uint32_t i = 0; i < g_node_count; i++) {
            if (i != t->home_node && t->pages_remote[i] > best_pages) {
                best_pages = t->pages_remote[i];
                best_node = i;
            }
        }

        if (best_node != t->home_node) {
            kprintf("[NUMA] Tâche %u : %u%% pages distantes → migration home node %u→%u.\n",
                    task_id, remote_pct, t->home_node, best_node);
            t->home_node = best_node;
            t->migrations_triggered++;
            /* reset compteurs après migration */
            for (uint32_t i = 0; i < NB_MAX_NODES; i++) {
                t->pages_local[i] = 0;
                t->pages_remote[i] = 0;
            }
            t->total_page_faults = 0;
        }
    }
}

/**
 * Retourne le "home node" d'une tâche. Utilisé par l'ordonnanceur pour
 * placer la tâche sur le bon cœur (politique NUMA-aware scheduling).
 */
uint32_t numa_balancer_get_home_node(uint32_t task_id) {
    for (uint32_t i = 0; i < g_task_count; i++) {
        if (g_tasks[i].task_id == task_id) {
            return g_tasks[i].home_node;
        }
    }
    return 0;
}
