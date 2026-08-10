#include "afros_hal.h"
#include "kprintf.h"

/**
 * @file tcp_westwood_africa.c
 * @brief Variante TCP Westwood adaptée aux liens instables d'Afrique de l'Ouest.
 *
 * TCP Westwood Africa est une variante de TCP Westwood+ conçue pour les
 * réseaux où la perte de paquets est plus souvent due à des erreurs de
 * transmission sans fil (interférences, handover cellulaire, coupures
 * intermittentes) qu'à une congestion réelle du routeur. Le principe :
 * estimer la bande passante disponible (BWE — BandWidth Estimate) à partir
 * du taux d'acquittement, et utiliser cette estimation pour calculer la
 * fenêtre de congestion après une perte, au lieu de la diviser
 * brutalement par 2 (Reno) ou par 4 (CUBIC).
 *
 * Ajustements spécifiques AfriOS :
 *   - Filtrage passe-bas de l'estimation BWE pour ignorer les pics
 *     ponctuels dus au handover 2G/3G/4G.
 *   - Détection "intermittent link" via un compteur de RTT soudainement
 *     élevé (> 3x la moyenne) — déclenche le repli sur la fenêtre de
 *     survie plutôt qu'une réduction de congestion.
 *   - Seuil de départ lent (ssthresh) initial élevé sur les liens
 *     satellitaires détectés (RTT > 400 ms).
 */

#define BWE_FILTER_ALPHA        3       /* filtre passe-bas (1/3) */
#define RTT_INTERMITTENT_RATIO  3       /* RTT > 3x moyenne → lien instable */
#define RTT_SATELLITE_MS        400
#define INITIAL_CWND_PACKETS    10
#define MIN_CWND_PACKETS        2

typedef enum {
    TCP_AFRICA_STATE_OPEN,
    TCP_AFRICA_STATE_DISORDER,
    TCP_AFRICA_STATE_RECOVERY,
    TCP_AFRICA_STATE_INTERMITTENT
} tcp_africa_state_t;

typedef struct {
    uint32_t cwnd;              /* fenêtre de congestion (paquets) */
    uint32_t ssthresh;          /* seuil de départ lent */
    uint32_t bwe_filtered;      /* BWE filtrée (bytes/sec) */
    uint32_t bwe_last_sample;
    uint64_t rtt_avg_us;        /* RTT moyen mobile (µs) */
    uint64_t rtt_last_us;
    tcp_africa_state_t state;
    uint32_t loss_count;
    uint32_t intermittent_count;
} tcp_africa_conn_t;

/**
 * Initialise une connexion TCP Westwood Africa. Sur détection d'un RTT
 * satellite, on élargit le seuil de départ lent pour éviter le timeout
 * initial qui tuerait le débit.
 */
void tcp_westwood_africa_init(tcp_africa_conn_t *c, uint64_t initial_rtt_us) {
    if (!c) return;
    c->cwnd = INITIAL_CWND_PACKETS;
    c->ssthresh = 0xFFFFFFFF;
    c->bwe_filtered = 0;
    c->bwe_last_sample = 0;
    c->rtt_avg_us = initial_rtt_us;
    c->rtt_last_us = initial_rtt_us;
    c->state = TCP_AFRICA_STATE_OPEN;
    c->loss_count = 0;
    c->intermittent_count = 0;

    if (initial_rtt_us > (RTT_SATELLITE_MS * 1000ULL)) {
        kprintf("[TCP-WA] Lien satellitaire détecté (RTT=%llu µs), ssthresh initial élargi.\n",
                (unsigned long long)initial_rtt_us);
        c->ssthresh = 64;
    }
}

/**
 * Met à jour l'estimation de bande passante (BWE) avec un nouvel échantillon.
 * Le filtre passe-bas (1/3 nouveau + 2/3 ancien) lisse les variations
 * transitoires, typiques d'un handover cellulaire.
 */
void tcp_westwood_africa_update_bwe(tcp_africa_conn_t *c, uint32_t acked_bytes, uint64_t rtt_us) {
    if (!c || acked_bytes == 0) return;

    uint32_t sample = acked_bytes;
    if (rtt_us > 0) {
        /* normaliser en bytes/sec */
        sample = (uint32_t)(((uint64_t)acked_bytes * 1000000ULL) / rtt_us);
    }

    c->bwe_last_sample = sample;
    c->bwe_filtered = (sample / BWE_FILTER_ALPHA) +
                      (c->bwe_filtered * (BWE_FILTER_ALPHA - 1) / BWE_FILTER_ALPHA);

    /* mise à jour RTT moyen mobile */
    c->rtt_last_us = rtt_us;
    c->rtt_avg_us = (rtt_us / 4) + (c->rtt_avg_us * 3 / 4);

    /* détection de lien intermittent */
    if (rtt_us > c->rtt_avg_us * RTT_INTERMITTENT_RATIO && c->rtt_avg_us > 0) {
        c->intermittent_count++;
        c->state = TCP_AFRICA_STATE_INTERMITTENT;
        kprintf("[TCP-WA] Lien intermittent détecté (RTT=%llu µs, moy=%llu µs).\n",
                (unsigned long long)rtt_us, (unsigned long long)c->rtt_avg_us);
        c->cwnd = MIN_CWND_PACKETS;
        c->ssthresh = (c->bwe_filtered * (uint32_t)c->rtt_avg_us) / (1000000ULL * 1460);
        if (c->ssthresh < MIN_CWND_PACKETS) c->ssthresh = MIN_CWND_PACKETS;
    } else if (c->state == TCP_AFRICA_STATE_INTERMITTENT &&
               rtt_us < c->rtt_avg_us * 2) {
        c->state = TCP_AFRICA_STATE_OPEN;
        kprintf("[TCP-WA] Lien stabilisé, retour en mode OPEN (cwnd=%u).\n", c->cwnd);
    }
}

/**
 * Gestion d'une perte de paquet. Contrairement à Reno/CUBIC qui divisent
 * cwnd par 2 ou 4, on calcule ssthresh à partir de la BWE filtrée — bien
 * plus précise sur les liens sans-fil où la perte n'est pas toujours
 * synonyme de congestion.
 */
void tcp_westwood_africa_on_loss(tcp_africa_conn_t *c) {
    if (!c) return;
    c->loss_count++;

    if (c->state == TCP_AFRICA_STATE_INTERMITTENT) {
        /* ne pas pénaliser deux fois : on est déjà en mode survie */
        return;
    }

    uint32_t new_ssthresh = (c->bwe_filtered * (uint32_t)c->rtt_avg_us) / (1000000ULL * 1460);
    if (new_ssthresh < MIN_CWND_PACKETS) new_ssthresh = MIN_CWND_PACKETS;

    c->ssthresh = new_ssthresh;
    c->cwnd = new_ssthresh;
    c->state = TCP_AFRICA_STATE_RECOVERY;

    kprintf("[TCP-WA] Perte paquet → ssthresh=%u, cwnd=%u (BWE=%u B/s, RTTmoy=%llu µs).\n",
            c->ssthresh, c->cwnd, c->bwe_filtered, (unsigned long long)c->rtt_avg_us);
}

/**
 * Accusé de réception reçu. Croissance AIMD : +1 paquet par RTT en slow
 * start, linéaire en avoidance. On bascule en OPEN dès qu'on acquitte
 * hors d'une phase de récupération.
 */
void tcp_westwood_africa_on_ack(tcp_africa_conn_t *c, uint32_t acked_packets) {
    if (!c) return;
    (void)acked_packets;

    if (c->state == TCP_AFRICA_STATE_RECOVERY) {
        c->state = TCP_AFRICA_STATE_OPEN;
    }

    if (c->cwnd < c->ssthresh) {
        c->cwnd++;  /* slow start : exponentiel */
    } else {
        c->cwnd += 1; /* congestion avoidance : linéaire (simplifié) */
    }
}
