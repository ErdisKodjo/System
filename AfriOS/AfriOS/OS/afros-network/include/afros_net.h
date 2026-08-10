#ifndef AFROS_NET_H
#define AFROS_NET_H

/*
 * En-tête HAL fourni transitivement par la cible CMake `afros-hal`
 * (target_link_libraries afros-network PUBLIC afros-hal). En cas de build
 * partiel sans la cible, le CMakeLists de afros-network repli sur
 * ${CMAKE_SOURCE_DIR}/afros-core/Kernel/hal/include comme include direct.
 *
 * On inclut ici uniquement afros_types.h (types de base) plutôt que afros_hal.h
 * (qui amènerait cpu_abstraction, memory_abstraction, … inutiles au réseau).
 */
#include "afros_types.h"

/**
 * @file afros_net.h
 * @brief Gestionnaire réseau intelligent pour AfriOS.
 *
 * Évolution (étape P2) : la couche afros-network n'exposait historiquement
 * que `net_init` / `net_send_packet` / `net_optimize_bandwidth` — un stub
 * sans envoi réel. La couche DTN (`intermittent_net.c`) ne pouvait donc pas
 * rejouer les paquets en attente, faute de fonction d'envoi synchrone.
 *
 * On ajoute une API socket-like (endpoint + send/recv + open/close) qui,
 * en mode host (libc <sys/socket.h>), délègue aux BSD sockets. En mode
 * freestanding (kernel bare-metal), retourne AFROS_ERROR_NOT_SUPPORTED —
 * l'implémentation réelle attendra un driver de carte réseau dans
 * Kernel/drivers/.
 *
 * Modèle :
 *   - `afros_net_open_socket(ep, &sock_id)` : ouvre un socket BSD, l'associe
 *     à l'endpoint `ep` dans une table interne (64 slots max).
 *   - `afros_net_send(ep, data, len)` : cherche dans la table un socket
 *     ouvert pour cet endpoint (match exact proto+dst+src), et envoie via
 *     send(). Si l'endpoint n'a pas de socket ouvert, retourne
 *     AFROS_ERROR_INVALID_PARAM.
 *   - `afros_net_recv(ep, buf, &len, timeout_ms)` : idem via recv() avec
 *     SO_RCVTIMEO.
 *   - `afros_net_close_socket(sock_id)` : ferme le socket et libère le slot.
 *
 * Coexistence : l'ancienne API (net_init/net_send_packet/net_optimize_bandwidth)
 * reste disponible pour les consumers existants. La nouvelle API ne casse
 * rien.
 */

/* ====================== Ancienne API (legacy) =========================== */

typedef enum {
    NET_TYPE_ETHERNET,
    NET_TYPE_WIFI,
    NET_TYPE_MOBILE,
    NET_TYPE_SATELLITE
} afros_net_interface_t;

afros_status_t net_init(void);
afros_status_t net_send_packet(afros_net_interface_t type, const uint8_t *data, size_t size);
afros_status_t net_optimize_bandwidth(bool energy_saving_mode);

/* ====================== Nouvelle API socket-like (P2) ================== */

/**
 * @brief Protocole de transport utilisé pour un endpoint.
 *
 * En mode host, l'implémentation mappe ces valeurs sur SOCK_STREAM/IPPROTO_TCP,
 * SOCK_DGRAM/IPPROTO_UDP, ou SOCK_RAW. En freestanding, AFROS_ERROR_NOT_SUPPORTED.
 */
typedef enum {
    AFROS_NET_PROTO_TCP = 0,   /**< Connecté, fiable, ordonné.                */
    AFROS_NET_PROTO_UDP = 1,   /**< Non-connecté, best-effort.                 */
    AFROS_NET_PROTO_RAW = 2    /**< Paquets bruts (nécessite root/cap_net_raw).*/
} afros_net_proto_t;

/**
 * @brief Endpoint réseau (quadriplet src/dst + protocole).
 *
 * Représentation en host byte order — l'implémentation fait la conversion
 * vers network byte order (htonl/htons) au moment de l'appel socket BSD.
 * Utiliser AFROS_NET_PROTO_TCP pour une connexion persistante (le socket
 * reste ouvert entre send/recv successifs tant que afros_net_close_socket
 * n'est pas appelé).
 *
 * L'endpoint sert de clé de lookup dans la table de sockets interne :
 * deux appels à afros_net_send avec le même endpoint utiliseront le même
 * socket sous-jacent, sans coût de reconnexion.
 */
typedef struct {
    uint32_t           src_ip;     /**< IPv4 source  (0.0.0.0 = any).          */
    uint16_t           src_port;   /**< Port source  (0 = auto-assign).        */
    uint32_t           dst_ip;     /**< IPv4 destination.                       */
    uint16_t           dst_port;   /**< Port destination.                       */
    afros_net_proto_t  proto;      /**< TCP / UDP / RAW.                        */
} afros_net_endpoint_t;

/**
 * @brief Ouvre un socket associé à un endpoint.
 *
 * En mode host :
 *   - Alloue un slot dans la table interne (64 sockets max).
 *   - appelle socket(AF_INET, SOCK_STREAM/SOCK_DGRAM/SOCK_RAW, 0).
 *   - Pour TCP : connect() vers (dst_ip, dst_port). bind() au (src_ip,
 *     src_port) si src_port != 0.
 *   - Pour UDP : bind() au (src_ip, src_port) si non-nul, pas de connect().
 *
 * @param[in]  ep       Endpoint (proto, src/dst IP/port). Doit être non-NULL.
 * @param[out] sock_id  Receives the socket id (0..63). Stable until
 *                      afros_net_close_socket() is called.
 *
 * @retval AFROS_SUCCESS              Socket ouvert.
 * @retval AFROS_ERROR_INVALID_PARAM  ep ou sock_id NULL.
 * @retval AFROS_ERROR_NO_MEMORY      Table de sockets pleine (64/64).
 * @retval AFROS_ERROR                Erreur libc (socket/connect/bind a échoué).
 * @retval AFROS_ERROR_NOT_SUPPORTED  Build freestanding (pas de libc <sys/socket.h>).
 */
afros_status_t afros_net_open_socket(const afros_net_endpoint_t *ep,
                                      uint32_t *sock_id);

/**
 * @brief Envoie des données sur le socket associé à un endpoint.
 *
 * Cherche dans la table interne un socket ouvert pour cet endpoint
 * (match exact proto+src_ip+src_port+dst_ip+dst_port). Si trouvé, appelle
 * send(sock_fd, data, len, 0). Sinon, retourne AFROS_ERROR_INVALID_PARAM
 * (le caller doit appeler afros_net_open_socket en premier).
 *
 * @param[in] ep    Endpoint identifiant le socket. Doit être non-NULL.
 * @param[in] data  Buffer à envoyer. Doit être non-NULL si len > 0.
 * @param[in] len   Nombre d'octets à envoyer. Peut être 0 (no-op).
 *
 * @retval AFROS_SUCCESS              len octets envoyés.
 * @retval AFROS_ERROR_INVALID_PARAM  ep NULL, data NULL avec len>0, ou pas
 *                                    de socket ouvert pour cet endpoint.
 * @retval AFROS_ERROR                send() a échoué (connexion fermée ?).
 * @retval AFROS_ERROR_NOT_SUPPORTED  Build freestanding.
 */
afros_status_t afros_net_send(const afros_net_endpoint_t *ep,
                               const uint8_t *data, uint16_t len);

/**
 * @brief Reçoit des données depuis le socket associé à un endpoint.
 *
 * En mode host, configure SO_RCVTIMEO avant le recv() pour ne pas bloquer
 * indéfiniment. Retourne AFROS_ERROR_TIMEOUT si rien n'est arrivé dans le
 * délai imparti.
 *
 * @param[in]     ep          Endpoint identifiant le socket. Non-NULL.
 * @param[out]    buf         Buffer de réception. Doit être non-NULL.
 * @param[in,out] len         In: capacité de buf (max 65535). Out: octets
 *                            effectivement reçus.
 * @param[in]     timeout_ms  Délai max en ms. 0 = non-bloquant.
 *
 * @retval AFROS_SUCCESS              len octets reçus (len mis à jour).
 * @retval AFROS_ERROR_INVALID_PARAM  ep NULL, buf NULL, len NULL, ou pas
 *                                    de socket ouvert pour cet endpoint.
 * @retval AFROS_ERROR_TIMEOUT        Rien reçu dans le délai (len = 0).
 * @retval AFROS_ERROR                recv() a échoué.
 * @retval AFROS_ERROR_NOT_SUPPORTED  Build freestanding.
 */
afros_status_t afros_net_recv(afros_net_endpoint_t *ep,
                               uint8_t *buf, uint16_t *len,
                               uint32_t timeout_ms);

/**
 * @brief Ferme un socket ouvert.
 *
 * @param[in] sock_id  Socket id retourné par afros_net_open_socket.
 *
 * @retval AFROS_SUCCESS              Socket fermé, slot libéré.
 * @retval AFROS_ERROR_INVALID_PARAM  sock_id invalide ou déjà fermé.
 * @retval AFROS_ERROR_NOT_SUPPORTED  Build freestanding.
 */
afros_status_t afros_net_close_socket(uint32_t sock_id);

#endif /* AFROS_NET_H */
