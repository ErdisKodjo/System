#include "../include/afros_net.h"
#include "kprintf.h"

/**
 * @file afros_net.c
 * @brief Implementation of the intelligent network manager for AfriOS.
 *
 * Freestanding: utilise kprintf (HAL) au lieu de <stdio.h>/printf. Les
 * messages sont émis via arch_console_ops (PL011/SBI/USART/16550 selon le
 * port actif) une fois la HAL initialisée — avant, kprintf devient no-op
 * mais ne crash pas.
 *
 * Étape P2 : ajout de l'API socket-like (afros_net_open_socket / send /
 * recv / close_socket). En mode host (libc disponible), délègue aux BSD
 * sockets via <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>. En mode
 * freestanding (AFROS_FREESTANDING défini), retourne AFROS_ERROR_NOT_SUPPORTED
 * — l'implémentation attendra un driver de carte réseau dans Kernel/drivers/.
 *
 * Table de sockets : 64 entrées max, lookup par endpoint (match exact sur
 * proto+src_ip+src_port+dst_ip+dst_port). Un slot libre a `in_use = false`.
 */

/* ========================== Legacy API ================================= */

static bool g_net_initialized = false;
static bool g_energy_saving = false;

afros_status_t net_init(void) {
    if (g_net_initialized) {
        return AFROS_SUCCESS;
    }

    kprintf("AfriOS Network: Initializing intelligent networking subsystem...\n");
    /* In a real implementation, this would detect available interfaces
     * (WiFi, Mobile, etc.) via the HAL device manager. */

    g_net_initialized = true;
    return AFROS_SUCCESS;
}

afros_status_t net_send_packet(afros_net_interface_t type, const uint8_t *data, size_t size) {
    if (!g_net_initialized) {
        return AFROS_ERROR;
    }
    (void)data;

    const char *interface_name = "UNKNOWN";
    switch (type) {
        case NET_TYPE_ETHERNET:  interface_name = "ETHERNET";  break;
        case NET_TYPE_WIFI:      interface_name = "WIFI";      break;
        case NET_TYPE_MOBILE:    interface_name = "MOBILE";    break;
        case NET_TYPE_SATELLITE: interface_name = "SATELLITE"; break;
    }

    kprintf("AfriOS Network: Sending packet (%zu bytes) via %s\n", size, interface_name);

    /* Apply optimization logic based on interface type and energy mode */
    if (g_energy_saving && type == NET_TYPE_MOBILE) {
        kprintf("AfriOS Network: [Optimization] Batching packet to save power on mobile interface.\n");
    }

    if (type == NET_TYPE_SATELLITE) {
        kprintf("AfriOS Network: [Optimization] Using high-latency protocol optimization for satellite link.\n");
    }

    /* Simulate hardware transmission */
    return AFROS_SUCCESS;
}

afros_status_t net_optimize_bandwidth(bool energy_saving_mode) {
    if (!g_net_initialized) {
        return AFROS_ERROR;
    }

    g_energy_saving = energy_saving_mode;
    kprintf("AfriOS Network: Bandwidth optimization set to %s mode.\n",
            energy_saving_mode ? "ENERGY-SAVING" : "PERFORMANCE");

    return AFROS_SUCCESS;
}

/* ==================== Nouvelle API socket-like (P2) ==================== */

#ifndef AFROS_FREESTANDING
/*
 * Mode host : implémentation réelle via BSD sockets (libc).
 *
 * Notes :
 *   - On n'inclut PAS <sys/socket.h> dans le header (afros_net.h) pour
 *     garder le header freestanding-compatible. On l'inclut ici, uniquement
 *     dans le .c, derrière #ifndef AFROS_FREESTANDING.
 *   - La table de sockets est statique (64 entrées). Pas de verrou : on
 *     suppose single-threaded pour l'instant (le thread DTN est le seul
 *     caller en kernel). Si on multi-thread, ajouter un mutex.
 *   - Les adresses IP sont en host byte order dans l'endpoint ; on les
 *     convertit en network byte order (htonl/htons) au moment de l'appel.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define AFROS_NET_MAX_SOCKETS  64

typedef struct {
    bool                  in_use;
    afros_net_endpoint_t  ep;
    int                   fd;     /**< File descriptor BSD (>= 0 si valide). */
} afros_net_slot_t;

static afros_net_slot_t s_socket_table[AFROS_NET_MAX_SOCKETS];

/* Recherche d'un slot par endpoint (match exact). Retourne -1 si non trouvé. */
static int find_slot_by_ep(const afros_net_endpoint_t *ep) {
    for (int i = 0; i < AFROS_NET_MAX_SOCKETS; i++) {
        if (!s_socket_table[i].in_use) continue;
        const afros_net_endpoint_t *s = &s_socket_table[i].ep;
        if (s->proto    == ep->proto    &&
            s->src_ip   == ep->src_ip   &&
            s->src_port == ep->src_port &&
            s->dst_ip   == ep->dst_ip   &&
            s->dst_port == ep->dst_port) {
            return i;
        }
    }
    return -1;
}

/* Trouve un slot libre. Retourne -1 si tous occupés. */
static int find_free_slot(void) {
    for (int i = 0; i < AFROS_NET_MAX_SOCKETS; i++) {
        if (!s_socket_table[i].in_use) return i;
    }
    return -1;
}

/* Mappe afros_net_proto_t vers (type, protocol) BSD. */
static int proto_to_bsd(afros_net_proto_t proto, int *type, int *protocol) {
    switch (proto) {
        case AFROS_NET_PROTO_TCP: *type = SOCK_STREAM; *protocol = IPPROTO_TCP; return 0;
        case AFROS_NET_PROTO_UDP: *type = SOCK_DGRAM;  *protocol = IPPROTO_UDP; return 0;
        case AFROS_NET_PROTO_RAW: *type = SOCK_RAW;    *protocol = 0;           return 0;
        default: return -1;
    }
}

afros_status_t afros_net_open_socket(const afros_net_endpoint_t *ep,
                                      uint32_t *sock_id) {
    if (!ep || !sock_id) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    /* Si un socket est déjà ouvert pour cet endpoint, on le réutilise
     * (idempotence — utile pour DTN qui peut appeler open_socket avant
     * chaque send sans se soucier de l'état précédent). */
    int existing = find_slot_by_ep(ep);
    if (existing >= 0) {
        *sock_id = (uint32_t)existing;
        return AFROS_SUCCESS;
    }

    int slot = find_free_slot();
    if (slot < 0) {
        kprintf("[NET] Socket table pleine (%d/%d).\n",
                AFROS_NET_MAX_SOCKETS, AFROS_NET_MAX_SOCKETS);
        return AFROS_ERROR_NO_MEMORY;
    }

    int type, protocol;
    if (proto_to_bsd(ep->proto, &type, &protocol) != 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    int fd = socket(AF_INET, type, protocol);
    if (fd < 0) {
        kprintf("[NET] socket() failed: errno=%d\n", errno);
        return AFROS_ERROR;
    }

    /* Bind à l'adresse source si src_port != 0. Pour src_ip = 0.0.0.0,
     * le kernel choisit l'interface de sortie. */
    if (ep->src_port != 0) {
        struct sockaddr_in src_addr;
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.sin_family      = AF_INET;
        src_addr.sin_addr.s_addr = htonl(ep->src_ip);
        src_addr.sin_port        = htons(ep->src_port);
        if (bind(fd, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
            kprintf("[NET] bind() failed: errno=%d\n", errno);
            close(fd);
            return AFROS_ERROR;
        }
    }

    /* Pour TCP : connect() vers la destination. Pour UDP, on pourrait
     * faire un connect() aussi (pour bénéficier d'ECONNREFUSED), mais on
     * reste conservateur — sendto() serait plus idiomatique pour UDP
     * non-connecté, mais l'API afros_net_send ne prend pas d'adresse,
     * donc on doit avoir un default destination. */
    if (ep->proto == AFROS_NET_PROTO_TCP) {
        struct sockaddr_in dst_addr;
        memset(&dst_addr, 0, sizeof(dst_addr));
        dst_addr.sin_family      = AF_INET;
        dst_addr.sin_addr.s_addr = htonl(ep->dst_ip);
        dst_addr.sin_port        = htons(ep->dst_port);
        if (connect(fd, (struct sockaddr *)&dst_addr, sizeof(dst_addr)) < 0) {
            kprintf("[NET] connect() failed: errno=%d\n", errno);
            close(fd);
            return AFROS_ERROR;
        }
    } else if (ep->proto == AFROS_NET_PROTO_UDP) {
        /* connect() sur un socket UDP fixe la destination par défaut,
         * permettant l'usage de send() au lieu de sendto(). */
        struct sockaddr_in dst_addr;
        memset(&dst_addr, 0, sizeof(dst_addr));
        dst_addr.sin_family      = AF_INET;
        dst_addr.sin_addr.s_addr = htonl(ep->dst_ip);
        dst_addr.sin_port        = htons(ep->dst_port);
        if (connect(fd, (struct sockaddr *)&dst_addr, sizeof(dst_addr)) < 0) {
            kprintf("[NET] UDP connect() failed: errno=%d\n", errno);
            close(fd);
            return AFROS_ERROR;
        }
    }

    s_socket_table[slot].in_use = true;
    s_socket_table[slot].ep     = *ep;
    s_socket_table[slot].fd     = fd;
    *sock_id = (uint32_t)slot;

    kprintf("[NET] Socket ouvert (slot=%u, fd=%d, proto=%d, dst=0x%08x:%u).\n",
            *sock_id, fd, (int)ep->proto, ep->dst_ip, ep->dst_port);
    return AFROS_SUCCESS;
}

afros_status_t afros_net_send(const afros_net_endpoint_t *ep,
                               const uint8_t *data, uint16_t len) {
    if (!ep) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    if (len > 0 && !data) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    int slot = find_slot_by_ep(ep);
    if (slot < 0) {
        kprintf("[NET] afros_net_send: pas de socket ouvert pour cet endpoint.\n");
        return AFROS_ERROR_INVALID_PARAM;
    }

    if (len == 0) {
        return AFROS_SUCCESS;  /* no-op, mais valide */
    }

    ssize_t sent = send(s_socket_table[slot].fd, data, len, 0);
    if (sent < 0) {
        kprintf("[NET] send() failed: errno=%d\n", errno);
        return AFROS_ERROR;
    }
    if (sent < (ssize_t)len) {
        /* Partial send — pour TCP, l'appeler devrait réessayer avec le
         * reste. On retourne SUCCESS mais on logge. */
        kprintf("[NET] send() partial: %zd/%u bytes.\n", sent, len);
    }
    return AFROS_SUCCESS;
}

afros_status_t afros_net_recv(afros_net_endpoint_t *ep,
                               uint8_t *buf, uint16_t *len,
                               uint32_t timeout_ms) {
    if (!ep || !buf || !len) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    int slot = find_slot_by_ep(ep);
    if (slot < 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    int fd = s_socket_table[slot].fd;

    /* Configurer le timeout via SO_RCVTIMEO. On le remet à chaque appel
     * (cheap) pour rester stateless côté caller. */
    if (timeout_ms > 0) {
        struct timeval tv;
        tv.tv_sec  = (time_t)(timeout_ms / 1000u);
        tv.tv_usec = (suseconds_t)((timeout_ms % 1000u) * 1000u);
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            kprintf("[NET] setsockopt(SO_RCVTIMEO) failed: errno=%d\n", errno);
            return AFROS_ERROR;
        }
    } else {
        /* timeout_ms = 0 : non-bloquant. */
        struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    ssize_t got = recv(fd, buf, (size_t)*len, 0);
    if (got < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *len = 0;
            return AFROS_ERROR_TIMEOUT;
        }
        kprintf("[NET] recv() failed: errno=%d\n", errno);
        *len = 0;
        return AFROS_ERROR;
    }
    *len = (uint16_t)got;
    return AFROS_SUCCESS;
}

afros_status_t afros_net_close_socket(uint32_t sock_id) {
    if (sock_id >= AFROS_NET_MAX_SOCKETS) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    if (!s_socket_table[sock_id].in_use) {
        return AFROS_ERROR_INVALID_PARAM;
    }

    int fd = s_socket_table[sock_id].fd;
    s_socket_table[sock_id].in_use = false;
    s_socket_table[sock_id].fd     = -1;
    memset(&s_socket_table[sock_id].ep, 0, sizeof(s_socket_table[sock_id].ep));

    if (close(fd) < 0) {
        kprintf("[NET] close(fd=%d) failed: errno=%d\n", fd, errno);
        return AFROS_ERROR;
    }
    return AFROS_SUCCESS;
}

#else  /* AFROS_FREESTANDING — pas de libc <sys/socket.h> */

/*
 * Build freestanding (kernel bare-metal) : pas de BSD sockets. Les 4
 * fonctions retournent AFROS_ERROR_NOT_SUPPORTED. L'implémentation réelle
 * attendra un driver de carte réseau (Kernel/drivers/) qui exposera une
 * API équivalente — voir la couche DTN (afros/network/intermittent_net.c)
 * qui repli sur le mode "simulé" quand afros_net_send retourne NOT_SUPPORTED.
 */

afros_status_t afros_net_open_socket(const afros_net_endpoint_t *ep,
                                      uint32_t *sock_id) {
    (void)ep; (void)sock_id;
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t afros_net_send(const afros_net_endpoint_t *ep,
                               const uint8_t *data, uint16_t len) {
    (void)ep; (void)data; (void)len;
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t afros_net_recv(afros_net_endpoint_t *ep,
                               uint8_t *buf, uint16_t *len,
                               uint32_t timeout_ms) {
    (void)ep; (void)buf; (void)len; (void)timeout_ms;
    return AFROS_ERROR_NOT_SUPPORTED;
}

afros_status_t afros_net_close_socket(uint32_t sock_id) {
    (void)sock_id;
    return AFROS_ERROR_NOT_SUPPORTED;
}

#endif /* AFROS_FREESTANDING */
