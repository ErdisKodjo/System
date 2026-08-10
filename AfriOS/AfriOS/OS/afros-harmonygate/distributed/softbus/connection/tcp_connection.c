/**
 * @file tcp_connection.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus TCP transport.
 *
 * Wraps a POSIX socket in a uniform SoftBus "connection" abstraction. The
 * sandbox can use the loopback interface for in-process testing; on real
 * hardware the caller passes a routable peer address.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>

#define AFROS_TCP_TIMEOUT_MS  5000
#define AFROS_TCP_MAX_CONN    8

/** SoftBus TCP connection handle. */
typedef struct {
    int      fd;            /**< -1 when free. */
    char     peer_addr[64];
    uint16_t peer_port;
    bool     in_use;
} afros_tcp_conn_t;

static struct {
    afros_tcp_conn_t  conns[AFROS_TCP_MAX_CONN];
    pthread_mutex_t   lock;
} g_tcp = { .lock = PTHREAD_MUTEX_INITIALIZER };

static afros_tcp_conn_t *alloc_locked(void)
{
    for (uint32_t i = 0; i < AFROS_TCP_MAX_CONN; ++i) {
        if (!g_tcp.conns[i].in_use) {
            memset(&g_tcp.conns[i], 0, sizeof(g_tcp.conns[i]));
            g_tcp.conns[i].fd = -1;
            g_tcp.conns[i].in_use = true;
            return &g_tcp.conns[i];
        }
    }
    return NULL;
}

/** Set SO_RCVTIMEO / SO_SNDTIMEO on the socket. */
static void apply_timeouts(int fd, uint32_t timeout_ms)
{
    struct timeval tv = {
        .tv_sec  = timeout_ms / 1000U,
        .tv_usec = (timeout_ms % 1000U) * 1000U,
    };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/**
 * @brief Open a TCP connection to @p addr:@p port.
 * @param addr  IPv4 dotted-decimal or hostname.
 * @param port  TCP port.
 * @return A non-negative connection id, or -AFROS_ERROR_* on failure.
 */
int32_t TcpConnect(const char *addr, uint16_t port)
{
    if (addr == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -AFROS_ERROR;
    }
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(port);
    if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
        close(fd);
        return -AFROS_ERROR_INVALID_PARAM;
    }
    apply_timeouts(fd, AFROS_TCP_TIMEOUT_MS);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd);
        return -AFROS_ERROR_TIMEOUT;
    }
    pthread_mutex_lock(&g_tcp.lock);
    afros_tcp_conn_t *c = alloc_locked();
    if (c == NULL) {
        pthread_mutex_unlock(&g_tcp.lock);
        close(fd);
        return -AFROS_ERROR_NO_MEMORY;
    }
    c->fd   = fd;
    strncpy(c->peer_addr, addr, sizeof(c->peer_addr) - 1);
    c->peer_port = port;
    int32_t id = (int32_t)(c - g_tcp.conns);
    pthread_mutex_unlock(&g_tcp.lock);
    return id;
}

static afros_tcp_conn_t *get_locked(int32_t id)
{
    if (id < 0 || id >= AFROS_TCP_MAX_CONN) {
        return NULL;
    }
    if (!g_tcp.conns[id].in_use || g_tcp.conns[id].fd < 0) {
        return NULL;
    }
    return &g_tcp.conns[id];
}

/**
 * @brief Send up to @p len bytes on a connection.
 * @return Number of bytes sent, or -AFROS_ERROR_* on failure.
 */
int32_t TcpSend(int32_t id, const uint8_t *buf, uint32_t len)
{
    if (buf == NULL && len > 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_tcp.lock);
    afros_tcp_conn_t *c = get_locked(id);
    int fd = (c != NULL) ? c->fd : -1;
    pthread_mutex_unlock(&g_tcp.lock);
    if (fd < 0) {
        return -AFROS_ERROR;
    }
    uint32_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) {
            return (sent > 0) ? (int32_t)sent : -AFROS_ERROR_TIMEOUT;
        }
        sent += (uint32_t)n;
    }
    return (int32_t)sent;
}

/**
 * @brief Receive up to @p cap bytes on a connection.
 * @return Number of bytes read (0 on EOF), or -AFROS_ERROR_* on failure.
 */
int32_t TcpRecv(int32_t id, uint8_t *buf, uint32_t cap)
{
    if (buf == NULL || cap == 0) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_tcp.lock);
    afros_tcp_conn_t *c = get_locked(id);
    int fd = (c != NULL) ? c->fd : -1;
    pthread_mutex_unlock(&g_tcp.lock);
    if (fd < 0) {
        return -AFROS_ERROR;
    }
    ssize_t n = recv(fd, buf, cap, 0);
    if (n < 0) {
        return -AFROS_ERROR_TIMEOUT;
    }
    return (int32_t)n;
}

/**
 * @brief Close a connection and release its slot.
 */
int32_t TcpClose(int32_t id)
{
    pthread_mutex_lock(&g_tcp.lock);
    afros_tcp_conn_t *c = get_locked(id);
    if (c == NULL) {
        pthread_mutex_unlock(&g_tcp.lock);
        return AFROS_ERROR;
    }
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
    c->in_use = false;
    pthread_mutex_unlock(&g_tcp.lock);
    return AFROS_SUCCESS;
}

/** @brief Format a connection's peer address for diagnostics. */
int32_t TcpGetPeer(int32_t id, char *out, uint32_t cap, uint16_t *port_out)
{
    if (out == NULL || cap == 0) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_tcp.lock);
    afros_tcp_conn_t *c = get_locked(id);
    if (c == NULL) {
        pthread_mutex_unlock(&g_tcp.lock);
        return AFROS_ERROR;
    }
    strncpy(out, c->peer_addr, cap - 1);
    out[cap - 1] = '\0';
    if (port_out != NULL) {
        *port_out = c->peer_port;
    }
    pthread_mutex_unlock(&g_tcp.lock);
    return AFROS_SUCCESS;
}
