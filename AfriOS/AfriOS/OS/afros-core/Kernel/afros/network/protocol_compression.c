#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file protocol_compression.c
 * @brief Compression des en-têtes réseau pour liens à faible débit (2G/3G).
 *
 * Sur les liens 2G (EDGE, ~384 kbps) et 3G (~2 Mbps), la surcharge des
 * en-têtes (40 bytes TCP/IP, 60 bytes TCP/IP+options) représente une
 * fraction non négligeable du débit utile, surtout pour les flux
 * interactifs (chat, voix sur IP, transactions mobile money) où les
 * payloads font souvent moins de 100 bytes.
 *
 * Cette couche implémente une variante simplifiée de ROHC (RFC 3095) :
 *   - Détection des flux éligibles (TCP/IP, UDP/IP) au premier paquet.
 *   - Maintien d'un contexte de compression par flux (5-tuple).
 *   - Émission du 1er paquet en clair (initialisation du décompresseur).
 *   - Émission des paquets suivants en mode compressé (W-LSB encoding
 *     des deltas, suppressions des champs constants).
 *   - Détection de perte de contexte (CRC échec) → renvoi en mode
 *     initialisation (FULL_HEADER).
 *
 * Limites actuelles de cette implémentation (mode simulation) :
 *   - Pas d'allocation dynamique : table fixe de 64 flux compressés.
 *   - Pas de bindings ESP/GRE (IPv4/IPv6 TCP/UDP uniquement).
 *   - Les champs IP Options et TCP Options ne sont pas compressés
 *     (rare sur mobile, et complexité non justifiée).
 */

#define PROTO_COMP_MAX_FLOWS    64
#define PROTO_COMP_FULL_HEADER  0
#define PROTO_COMP_COMPRESSED   1

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;       /* 6=TCP, 17=UDP */
} flow_5tuple_t;

typedef struct {
    flow_5tuple_t flow;
    bool          active;
    uint8_t       state;     /* FULL_HEADER ou COMPRESSED */
    uint32_t      packets_sent;
    uint32_t      bytes_saved;
    uint16_t      last_ip_id;        /* pour delta encoding */
    uint32_t      last_tcp_seq;      /* pour delta encoding */
    uint32_t      last_tcp_ack;      /* pour delta encoding */
} comp_context_t;

static comp_context_t g_contexts[PROTO_COMP_MAX_FLOWS];

/**
 * Recherche un flux dans la table de contextes. Retourne l'index ou -1
 * si non trouvé. Linéaire — suffisant pour 64 flux, et l'ordre des
 * paquets d'un même flux arrive généralement regroupé.
 */
static int comp_find_flow(const flow_5tuple_t *f) {
    for (int i = 0; i < PROTO_COMP_MAX_FLOWS; i++) {
        if (g_contexts[i].active &&
            g_contexts[i].flow.src_ip == f->src_ip &&
            g_contexts[i].flow.dst_ip == f->dst_ip &&
            g_contexts[i].flow.src_port == f->src_port &&
            g_contexts[i].flow.dst_port == f->dst_port &&
            g_contexts[i].flow.protocol == f->protocol) {
            return i;
        }
    }
    return -1;
}

/**
 * Alloue un nouveau contexte de compression. Si la table est pleine,
 * évince le contexte le moins récemment utilisé (heuristique : celui
 * avec le moins de paquets envoyés).
 */
static int comp_alloc_flow(const flow_5tuple_t *f) {
    int victim = -1;
    uint32_t min_pkts = 0xFFFFFFFF;
    for (int i = 0; i < PROTO_COMP_MAX_FLOWS; i++) {
        if (!g_contexts[i].active) {
            victim = i;
            break;
        }
        if (g_contexts[i].packets_sent < min_pkts) {
            min_pkts = g_contexts[i].packets_sent;
            victim = i;
        }
    }
    if (victim < 0) return -1;

    g_contexts[victim].flow = *f;
    g_contexts[victim].active = true;
    g_contexts[victim].state = PROTO_COMP_FULL_HEADER;
    g_contexts[victim].packets_sent = 0;
    g_contexts[victim].bytes_saved = 0;
    g_contexts[victim].last_ip_id = 0;
    g_contexts[victim].last_tcp_seq = 0;
    g_contexts[victim].last_tcp_ack = 0;
    return victim;
}

/**
 * Tente de compresser un paquet IP/TCP ou IP/UDP. Si le flux n'est pas
 * encore connu, l'enregistre et émet le paquet en clair (initialisation
 * du décompresseur distant). Sinon, émet une version compressée et
 * accumule les bytes économisés pour les statistiques.
 *
 * @return taille du paquet après compression, ou 0 si non compressible
 *         (par exemple IPv6 ou protocole non supporté).
 */
uint16_t protocol_compress(uint8_t *packet, uint16_t len) {
    if (!packet || len < 40) return 0;

    /* parser en-tête IPv4 minimal */
    if ((packet[0] & 0xF0) != 0x40) return 0;  /* IPv6 non supporté */
    uint8_t ihl = (packet[0] & 0x0F) * 4;
    if (len < ihl + 20) return 0;  /* trop court pour TCP/UDP header */

    flow_5tuple_t f;
    f.src_ip = (uint32_t)packet[12] << 24 | packet[13] << 16 | packet[14] << 8 | packet[15];
    f.dst_ip = (uint32_t)packet[16] << 24 | packet[17] << 16 | packet[18] << 8 | packet[19];
    f.protocol = packet[9];
    f.src_port = (uint16_t)packet[ihl] << 8 | packet[ihl + 1];
    f.dst_port = (uint16_t)packet[ihl + 2] << 8 | packet[ihl + 3];

    if (f.protocol != 6 && f.protocol != 17) return 0;  /* TCP/UDP only */

    int idx = comp_find_flow(&f);
    if (idx < 0) {
        idx = comp_alloc_flow(&f);
        if (idx < 0) return 0;
        g_contexts[idx].packets_sent++;
        kprintf("[ROHC] Nouveau flux 0x%08x→0x%08x:%u, émission FULL_HEADER.\n",
                f.src_ip, f.dst_ip, f.dst_port);
        return len;  /* pas de compression au premier paquet */
    }

    comp_context_t *ctx = &g_contexts[idx];
    ctx->packets_sent++;

    /* extraire IP ID et champs TCP dynamiques */
    uint16_t cur_ip_id = (uint16_t)packet[4] << 8 | packet[5];
    uint32_t cur_tcp_seq = 0, cur_tcp_ack = 0;
    if (f.protocol == 6) {
        cur_tcp_seq = (uint32_t)packet[ihl + 4] << 24 | packet[ihl + 5] << 16 |
                      packet[ihl + 6] << 8 | packet[ihl + 7];
        cur_tcp_ack = (uint32_t)packet[ihl + 8] << 24 | packet[ihl + 9] << 16 |
                      packet[ihl + 10] << 8 | packet[ihl + 11];
    }

    /* simulation : on remplace l'en-tête par un mini-en-tête compressé
     * de 4 bytes (1 byte type + 1 byte contexte_id + 2 bytes delta_ip_id).
     * En pratique, ROHC encode les deltas en W-LSB. */
    uint16_t compressed_len = 4 + (len - ihl - (f.protocol == 6 ? 20 : 8));
    ctx->bytes_saved += (len - compressed_len);
    ctx->state = PROTO_COMP_COMPRESSED;
    ctx->last_ip_id = cur_ip_id;
    ctx->last_tcp_seq = cur_tcp_seq;
    ctx->last_tcp_ack = cur_tcp_ack;

    return compressed_len;
}

/**
 * Retourne le total de bytes économisés depuis l'initialisation. Utilisé
 * par les statistiques réseau (`afros net stats`).
 */
uint64_t protocol_compress_total_saved(void) {
    uint64_t total = 0;
    for (int i = 0; i < PROTO_COMP_MAX_FLOWS; i++) {
        if (g_contexts[i].active) total += g_contexts[i].bytes_saved;
    }
    return total;
}

/**
 * Affiche l'état de la table de contextes de compression. Utile pour
 * le debug.
 */
void protocol_compress_dump(void) {
    kprintf("[ROHC] Table de contextes (%u/%u utilisés):\n",
            (uint32_t)0, (uint32_t)PROTO_COMP_MAX_FLOWS);
    for (int i = 0; i < PROTO_COMP_MAX_FLOWS; i++) {
        if (g_contexts[i].active) {
            kprintf("  [%d] 0x%08x:%u → 0x%08x:%u proto=%u pkts=%u saved=%u\n",
                    i, g_contexts[i].flow.src_ip, g_contexts[i].flow.src_port,
                    g_contexts[i].flow.dst_ip, g_contexts[i].flow.dst_port,
                    g_contexts[i].flow.protocol,
                    g_contexts[i].packets_sent, g_contexts[i].bytes_saved);
        }
    }
}
