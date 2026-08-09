/**
 * @file network_stack.h
 * @brief Stack réseau complète pour AfriOS
 * 
 * Implémente une stack réseau native incluant :
 * - Ethernet, VLAN, bonding
 * - IPv4/IPv6 dual-stack
 * - TCP/UDP/ICMP/ICMPv6
 * - DNS, DHCP, NTP clients
 * - Firewall stateful avec règles avancées
 * - QoS et traffic shaping
 * - VPN (WireGuard, IPsec)
 */

#ifndef AFROS_NETWORK_STACK_H
#define AFROS_NETWORK_STACK_H

#include <stdint.h>
#include <stdbool.h>
#include "afros_types.h"

// ============================================================================
// CONSTANTES ET LIMITES
// ============================================================================

#define MAX_INTERFACES 16
#define MAX_IP_ADDRESSES 8
#define MAX_ROUTES 256
#define MAX_FIREWALL_RULES 1024
#define MAX_SOCKETS 65535
#define MAX_DNS_SERVERS 4
#define MTU_DEFAULT 1500
#define MTU_JUMBO 9000

// ============================================================================
// ADRESSAGE ET CONFIGURATION IP
// ============================================================================

typedef enum {
    IP_FAMILY_V4 = 4,
    IP_FAMILY_V6 = 6
} ip_family_t;

typedef struct {
    union {
        uint8_t v4[4];
        uint8_t v6[16];
    } addr;
    ip_family_t family;
} ip_address_t;

typedef struct {
    ip_address_t address;
    ip_address_t netmask;
    ip_address_t broadcast;
    ip_address_t gateway;
    bool dhcp_enabled;
    bool configured;
} ip_config_t;

typedef enum {
    IF_STATE_DOWN = 0,
    IF_STATE_UP,
    IF_STATE_RUNNING,
    IF_STATE_ERROR
} interface_state_t;

typedef enum {
    IF_TYPE_ETHERNET = 0,
    IF_TYPE_WIFI,
    IF_TYPE_LOOPBACK,
    IF_TYPE_TUNNEL,
    IF_TYPE_BRIDGE,
    IF_TYPE_VLAN,
    IF_TYPE_BOND
} interface_type_t;

typedef struct {
    char name[32];
    interface_type_t type;
    interface_state_t state;
    uint8_t mac_addr[6];
    uint32_t mtu;
    uint32_t speed_mbps;
    bool duplex_full;
    ip_config_t ipv4_config;
    ip_config_t ipv6_config;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
    void* driver_data;
} network_interface_t;

// ============================================================================
// ROUTAGE
// ============================================================================

typedef enum {
    ROUTE_TYPE_UNICAST = 0,
    ROUTE_TYPE_LOCAL,
    ROUTE_TYPE_BROADCAST,
    ROUTE_TYPE_MULTICAST,
    ROUTE_TYPE_BLACKHOLE
} route_type_t;

typedef struct {
    ip_address_t destination;
    ip_address_t netmask;
    ip_address_t gateway;
    char interface[32];
    uint32_t metric;
    route_type_t type;
    bool active;
} routing_entry_t;

// ============================================================================
// SOCKETS ET TRANSPORT
// ============================================================================

typedef enum {
    SOCKET_STREAM = 0,      // TCP
    SOCKET_DGRAM,           // UDP
    SOCKET_RAW,             // Raw IP
    SOCKET_SEQPACKET        // SCTP-like
} socket_type_t;

typedef enum {
    SOCKET_STATE_CLOSED = 0,
    SOCKET_STATE_LISTEN,
    SOCKET_STATE_SYN_SENT,
    SOCKET_STATE_SYN_RECV,
    SOCKET_STATE_ESTABLISHED,
    SOCKET_STATE_FIN_WAIT_1,
    SOCKET_STATE_FIN_WAIT_2,
    SOCKET_STATE_CLOSE_WAIT,
    SOCKET_STATE_CLOSING,
    SOCKET_STATE_LAST_ACK,
    SOCKET_STATE_TIME_WAIT
} socket_state_t;

typedef struct {
    ip_address_t local_ip;
    ip_address_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    socket_type_t type;
    socket_state_t state;
    uint32_t recv_buffer_size;
    uint32_t send_buffer_size;
    uint32_t recv_available;
    uint32_t send_available;
    uint64_t options;       // SO_* flags
} socket_info_t;

// ============================================================================
// FIREWALL STATEFUL
// ============================================================================

typedef enum {
    FW_ACTION_ALLOW = 0,
    FW_ACTION_DENY,
    FW_ACTION_REJECT,
    FW_ACTION_LOG,
    FW_ACTION_RATE_LIMIT
} firewall_action_t;

typedef enum {
    FW_DIRECTION_INBOUND = 0,
    FW_DIRECTION_OUTBOUND,
    FW_DIRECTION_FORWARD
} firewall_direction_t;

typedef enum {
    FW_PROTOCOL_ANY = 0,
    FW_PROTOCOL_ICMP,
    FW_PROTOCOL_TCP,
    FW_PROTOCOL_UDP,
    FW_PROTOCOL_ICMPV6
} firewall_protocol_t;

typedef struct {
    uint32_t rule_id;
    char name[64];
    firewall_direction_t direction;
    firewall_protocol_t protocol;
    ip_address_t src_ip;
    ip_address_t src_mask;
    ip_address_t dst_ip;
    ip_address_t dst_mask;
    uint16_t src_port_start;
    uint16_t src_port_end;
    uint16_t dst_port_start;
    uint16_t dst_port_end;
    firewall_action_t action;
    uint32_t rate_limit_pps;      // packets per second
    uint64_t log_mask;            // quels événements logger
    bool enabled;
    bool stateful;                // suivre les connexions établies
} firewall_rule_t;

typedef struct {
    uint64_t total_allowed;
    uint64_t total_denied;
    uint64_t total_rejected;
    uint64_t rate_limited;
    uint64_t stateful_tracked;
} firewall_stats_t;

// ============================================================================
// DHCP CLIENT
// ============================================================================

typedef enum {
    DHCP_STATE_INIT = 0,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING,
    DHCP_STATE_REBINDING,
    DHCP_STATE_FAILED
} dhcp_state_t;

typedef struct {
    dhcp_state_t state;
    ip_address_t offered_ip;
    ip_address_t server_ip;
    uint32_t lease_time;
    uint32_t renewal_time;
    uint32_t rebind_time;
    uint64_t lease_start;
    ip_address_t dns_servers[MAX_DNS_SERVERS];
    size_t dns_count;
    char domain_name[128];
} dhcp_client_t;

// ============================================================================
// DNS CLIENT
// ============================================================================

typedef enum {
    DNS_RECORD_A = 1,
    DNS_RECORD_NS = 2,
    DNS_RECORD_CNAME = 5,
    DNS_RECORD_SOA = 6,
    DNS_RECORD_PTR = 12,
    DNS_RECORD_MX = 15,
    DNS_RECORD_TXT = 16,
    DNS_RECORD_AAAA = 28,
    DNS_RECORD_SRV = 33
} dns_record_type_t;

typedef struct {
    char hostname[256];
    ip_address_t addresses[8];
    size_t address_count;
    uint32_t ttl;
    uint64_t cached_at;
    bool valid;
} dns_cache_entry_t;

typedef struct {
    ip_address_t servers[MAX_DNS_SERVERS];
    size_t server_count;
    dns_cache_entry_t* cache;
    size_t cache_size;
    uint32_t timeout_ms;
    uint32_t retries;
} dns_client_t;

// ============================================================================
// QoS ET TRAFFIC SHAPING
// ============================================================================

typedef enum {
    QOS_CLASS_BEST_EFFORT = 0,
    QOS_CLASS_BACKGROUND,
    QOS_CLASS_STANDARD,
    QOS_CLASS_PRIORITY,
    QOS_CLASS_INTERACTIVE,
    QOS_CLASS_REALTIME
} qos_class_t;

typedef struct {
    qos_class_t class_id;
    char name[32];
    uint32_t min_bandwidth_bps;
    uint32_t max_bandwidth_bps;
    uint32_t priority;            // 0-7 (plus haut = plus prioritaire)
    uint32_t latency_target_us;   // latence cible en microsecondes
    bool enabled;
} qos_policy_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t total_packets;
    uint32_t current_rate_bps;
    uint32_t peak_rate_bps;
    uint32_t dropped_packets;
    uint32_t delayed_packets;
} qos_stats_t;

// ============================================================================
// VPN SUPPORT (WireGuard/IPsec)
// ============================================================================

typedef enum {
    VPN_TYPE_WIREGUARD = 0,
    VPN_TYPE_IPSEC_IKEV2,
    VPN_TYPE_OPENVPN_COMPAT
} vpn_type_t;

typedef struct {
    vpn_type_t type;
    char name[32];
    char endpoint[256];
    uint16_t port;
    uint8_t public_key[32];
    uint8_t private_key[32];
    ip_address_t local_tunnel_ip;
    ip_address_t remote_tunnel_ip;
    ip_address_t allowed_ips[16];
    size_t allowed_ips_count;
    uint32_t keepalive_interval;
    bool enabled;
    bool connected;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} vpn_config_t;

// ============================================================================
// STACK RÉSEAU PRINCIPALE
// ============================================================================

typedef struct {
    // Interfaces
    network_interface_t interfaces[MAX_INTERFACES];
    size_t interface_count;
    
    // Routage
    routing_entry_t routes[MAX_ROUTES];
    size_t route_count;
    
    // Sockets
    void* sockets[MAX_SOCKETS];   // Pointeurs vers structures internes
    size_t active_sockets;
    
    // Firewall
    firewall_rule_t firewall_rules[MAX_FIREWALL_RULES];
    size_t firewall_rule_count;
    firewall_stats_t firewall_stats;
    bool firewall_enabled;
    
    // DHCP
    dhcp_client_t dhcp_clients[MAX_INTERFACES];
    
    // DNS
    dns_client_t dns_client;
    
    // QoS
    qos_policy_t qos_policies[8];
    qos_stats_t qos_stats[8];
    
    // VPN
    vpn_config_t vpn_configs[4];
    
    // État global
    bool initialized;
    bool ipv4_enabled;
    bool ipv6_enabled;
    bool forwarding_enabled;
    uint32_t default_ttl;
} network_stack_t;

// ============================================================================
// API PUBLIQUE
// ============================================================================

/**
 * @brief Initialise la stack réseau
 * @param config Configuration initiale
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_stack_init(const network_stack_t* config);

/**
 * @brief Crée une interface réseau
 * @param name Nom de l'interface
 * @param type Type d'interface
 * @param mac_addr Adresse MAC
 * @return Index de l'interface ou erreur
 */
int network_create_interface(const char* name, interface_type_t type, 
                             const uint8_t* mac_addr);

/**
 * @brief Configure une adresse IP sur une interface
 * @param if_name Nom de l'interface
 * @param config Configuration IP
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_configure_ip(const char* if_name, 
                                    const ip_config_t* config);

/**
 * @brief Active/désactive une interface
 * @param if_name Nom de l'interface
 * @param up true pour activer, false pour désactiver
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_set_interface_state(const char* if_name, bool up);

/**
 * @brief Ajoute une entrée de routage
 * @param route Entrée de routage à ajouter
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_add_route(const routing_entry_t* route);

/**
 * @brief Crée un socket
 * @param family Famille IP (4 ou 6)
 * @param type Type de socket
 * @param protocol Protocole (0 par défaut)
 * @return FD du socket ou erreur
 */
int network_socket(ip_family_t family, socket_type_t type, int protocol);

/**
 * @brief Connecte un socket
 * @param fd Descripteur du socket
 * @param remote_ip IP distante
 * @param remote_port Port distant
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_connect(int fd, const ip_address_t* remote_ip, 
                               uint16_t remote_port);

/**
 * @brief Écoute sur un socket
 * @param fd Descripteur du socket
 * @param backlog Taille de la file d'attente
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_listen(int fd, int backlog);

/**
 * @brief Accepte une connexion entrante
 * @param fd Socket d'écoute
 * @param client_ip IP du client (sortie)
 * @param client_port Port du client (sortie)
 * @return FD du nouveau socket ou erreur
 */
int network_accept(int fd, ip_address_t* client_ip, uint16_t* client_port);

/**
 * @brief Envoie des données sur un socket
 * @param fd Descripteur du socket
 * @param buffer Données à envoyer
 * @param size Taille des données
 * @return Nombre d'octets envoyés ou erreur
 */
ssize_t network_send(int fd, const void* buffer, size_t size);

/**
 * @brief Reçoit des données d'un socket
 * @param fd Descripteur du socket
 * @param buffer Buffer de réception
 * @param size Taille du buffer
 * @return Nombre d'octets reçus ou erreur
 */
ssize_t network_recv(int fd, void* buffer, size_t size);

/**
 * @brief Ferme un socket
 * @param fd Descripteur du socket
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_close(int fd);

/**
 * @brief Ajoute une règle de firewall
 * @param rule Règle à ajouter
 * @return ID de la règle ou erreur
 */
int network_add_firewall_rule(const firewall_rule_t* rule);

/**
 * @brief Active/désactive le firewall
 * @param enabled true pour activer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_set_firewall_enabled(bool enabled);

/**
 * @brief Démarre le client DHCP sur une interface
 * @param if_name Nom de l'interface
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_dhcp_start(const char* if_name);

/**
 * @brief Résout un nom de domaine
 * @param hostname Nom à résoudre
 * @param result Résultat (adresse IP)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_dns_resolve(const char* hostname, 
                                   ip_address_t* result);

/**
 * @brief Configure une politique QoS
 * @param policy Politique QoS
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_configure_qos(const qos_policy_t* policy);

/**
 * @brief Configure un tunnel VPN
 * @param config Configuration VPN
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_configure_vpn(const vpn_config_t* config);

/**
 * @brief Active/désactive un tunnel VPN
 * @param vpn_name Nom du VPN
 * @param enabled true pour activer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_set_vpn_state(const char* vpn_name, bool enabled);

/**
 * @brief Récupère les statistiques d'une interface
 * @param if_name Nom de l'interface
 * @param stats Structure de statistiques (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_get_interface_stats(const char* if_name,
                                           network_interface_t* stats);

/**
 * @brief Récupère les statistiques du firewall
 * @param stats Structure de statistiques (sortie)
 */
void network_get_firewall_stats(firewall_stats_t* stats);

/**
 * @brief Nettoie la stack réseau
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t network_stack_shutdown(void);

#endif // AFROS_NETWORK_STACK_H
