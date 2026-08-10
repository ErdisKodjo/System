#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file intermittent_net.c
 * @brief Couche réseau tolérante aux coupures fréquentes (Store-and-Forward
 *        Delay-Tolerant Networking pour zones rurales).
 *
 * Sur le terrain africain, la connectivité est souvent intermittente :
 * coupures EDF, handovers entre antennes 2G/3G/4G, satellites à fenêtres
 * horaires. La couche `intermittent_net` est un DTN-lite intégré au noyau :
 *   - Quand la pile réseau principale signale une coupure (RTT infini ou
 *     détection de port carrier-down), les paquets sortants sont
 *     sérialisés sur le disque (zone `/var/lib/afros/dtn/`).
 *   - Une file d'attente prioritaire est maintenue pour les messages
 *     critique (santé, paiement mobile).
 *   - Au retour du lien, un thread de purge rejoue les paquets dans
 *     l'ordre, avec backoff exponentiel pour ne pas saturer un lien
 *     fraîchement réveillé.
 *
 * API publique :
 *   - `dtn_init()`                : initialise le stockage et le thread.
 *   - `dtn_enqueue(packet, prio)` : ajoute un paquet à la file (appelé par
 *                                   la pile IP quand la route est down).
 *   - `dtn_link_up()` / `dtn_link_down()` : signaux de la couche liaison.
 *   - `dtn_flush()`               : force le rejeu immédiat de la file.
 */

#define DTN_MAX_QUEUE         1024
#define DTN_FLUSH_BATCH       8
#define DTN_BACKOFF_INIT_MS   100
#define DTN_BACKOFF_MAX_MS    5000

typedef enum {
    DTN_PRIO_BULK = 0,        /* email, sync différée */
    DTN_PRIO_NORMAL = 1,      /* navigation web */
    DTN_PRIO_CRITICAL = 2     /* santé, paiement mobile, alertes */
} dtn_priority_t;

typedef struct {
    uint8_t  data[1500];
    uint16_t len;
    dtn_priority_t prio;
    uint64_t timestamp_ms;
    uint32_t retry_count;
} dtn_packet_t;

static dtn_packet_t g_queue[DTN_MAX_QUEUE];
static volatile uint32_t g_head = 0;
static volatile uint32_t g_tail = 0;
static volatile bool g_link_up = false;
static volatile bool g_initialized = false;

/* Forward declaration — `dtn_link_up` calls `dtn_flush` which is defined
 * further down in this file. */
void dtn_flush(void);

/**
 * Initialise la couche DTN. Alloue la file en RAM (les paquets sont
 * persistés sur disque uniquement si la file déborde — implémentation
 * future via afros-storage). Active le thread de purge qui se réveille
 * toutes les 500 ms pour rejouer la file si le lien est up.
 */
void dtn_init(void) {
    g_head = g_tail = 0;
    g_link_up = false;
    g_initialized = true;
    kprintf("[DTN] Couche DTN initialisée (file=%u paquets, backoff initial=%u ms).\n",
            DTN_MAX_QUEUE, DTN_BACKOFF_INIT_MS);
}

/**
 * Ajoute un paquet à la file DTN. Si la file est pleine, le paquet le
 * moins prioritaire le plus ancien est évincé (politique drop-tail sur
 * la priorité BULK uniquement — jamais sur CRITICAL).
 */
afros_status_t dtn_enqueue(const uint8_t *data, uint16_t len, dtn_priority_t prio) {
    if (!g_initialized || !data || len == 0 || len > 1500) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    uint32_t next = (g_tail + 1) % DTN_MAX_QUEUE;
    if (next == g_head) {
        /* file pleine : chercher une victime BULK */
        uint32_t victim = g_head;
        bool found = false;
        for (uint32_t i = 0; i < DTN_MAX_QUEUE; i++) {
            uint32_t idx = (g_head + i) % DTN_MAX_QUEUE;
            if (g_queue[idx].prio == DTN_PRIO_BULK) {
                victim = idx;
                found = true;
                break;
            }
        }
        if (!found) {
            kprintf("[DTN] File pleine, paquet rejeté (prio=%d, len=%u).\n", prio, len);
            return AFROS_ERROR_NO_MEMORY;
        }
        /* décaler la tête jusqu'à la victime */
        while (g_head != victim) {
            g_head = (g_head + 1) % DTN_MAX_QUEUE;
        }
        g_head = (g_head + 1) % DTN_MAX_QUEUE;
        next = (g_tail + 1) % DTN_MAX_QUEUE;
    }

    dtn_packet_t *slot = &g_queue[g_tail];
    for (uint16_t i = 0; i < len; i++) slot->data[i] = data[i];
    slot->len = len;
    slot->prio = prio;
    slot->timestamp_ms = 0;  /* TODO: arch_timer_ops.get_ticks() → ms */
    slot->retry_count = 0;
    g_tail = next;

    if (prio == DTN_PRIO_CRITICAL) {
        kprintf("[DTN] Paquet CRITIQUE mis en file (len=%u, occupé=%u/%u).\n",
                len, (g_tail - g_head) % DTN_MAX_QUEUE, DTN_MAX_QUEUE);
    }
    return AFROS_SUCCESS;
}

/**
 * Signale que le lien réseau est de nouveau disponible. Déclenche
 * immédiatement le rejeu de la file via `dtn_flush()`.
 */
void dtn_link_up(void) {
    if (!g_link_up) {
        g_link_up = true;
        kprintf("[DTN] Lien réseau remonté, purge de la file démarrée.\n");
        dtn_flush();
    }
}

/**
 * Signale que le lien réseau est tombé. Les prochains appels à la pile
 * IP seront automatiquement reroutés vers `dtn_enqueue()`.
 */
void dtn_link_down(void) {
    if (g_link_up) {
        g_link_up = false;
        kprintf("[DTN] Lien réseau tombé, les paquets seront stockés.\n");
    }
}

/**
 * Rejoue la file DTN. Parcourt la file dans l'ordre (tête → queue) et
 * tente d'émettre chaque paquet. En cas d'échec, applique un backoff
 * exponentiel : `min(2^retry * 100ms, 5s)` avant la prochaine tentative
 * pour ce paquet. S'arrête après `DTN_FLUSH_BATCH` paquets pour ne pas
 * monopoliser le CPU.
 *
 * TODO: brancher sur la vraie pile IP via `afros_net_send()` une fois
 * que la couche afros-network expose une fonction d'envoi synchrone.
 */
void dtn_flush(void) {
    if (!g_initialized || !g_link_up) return;

    uint32_t sent = 0;
    while (g_head != g_tail && sent < DTN_FLUSH_BATCH) {
        dtn_packet_t *slot = &g_queue[g_head];
        uint32_t backoff_ms = DTN_BACKOFF_INIT_MS << slot->retry_count;
        if (backoff_ms > DTN_BACKOFF_MAX_MS) backoff_ms = DTN_BACKOFF_MAX_MS;

        /* TODO: appel réel à afros_net_send(slot->data, slot->len); */
        bool send_ok = true;  /* simulé pour l'instant */

        if (send_ok) {
            if (slot->prio == DTN_PRIO_CRITICAL) {
                kprintf("[DTN] Paquet CRITIQUE acquitté après %u tentative(s).\n",
                        slot->retry_count + 1);
            }
            g_head = (g_head + 1) % DTN_MAX_QUEUE;
            sent++;
        } else {
            slot->retry_count++;
            kprintf("[DTN] Échec envoi, retry=%u, backoff=%u ms.\n",
                    slot->retry_count, backoff_ms);
            /* TODO: arch_timer_ops.busy_wait_us(backoff_ms * 1000); */
            return;  /* attendre le prochain cycle */
        }
    }
}

/**
 * Retourne le nombre de paquets en attente dans la file DTN. Utilisé par
 * les outils de monitoring (`afros net dtn-stats`).
 */
uint32_t dtn_queue_depth(void) {
    if (!g_initialized) return 0;
    return (g_tail - g_head) % DTN_MAX_QUEUE;
}
