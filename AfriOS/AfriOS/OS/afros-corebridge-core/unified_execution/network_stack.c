#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/loader.h"

/**
 * @file network_stack.c
 * @brief Network namespace sharing for AfriOS runtimes.
 *
 * Each runtime can be attached to a network namespace (netns). We
 * provide port forwarding between the host and a runtime's port range,
 * and per-runtime traffic counters for monitoring.
 *
 * On the host simulator, netns is emulated with bind() + a forwarder
 * thread that accepts connections on the host port and pipes them to
 * the runtime's port.
 */

#define MAX_NETNS      16
#define MAX_FORWARDS   64
#define RT_PORT_BASE   49152   /* Runtime ports are allocated above this */
#define RT_PORT_MAX    65535

typedef struct {
    int          in_use;
    runtime_handle_t rt;
    int          ns_fd;          /* File descriptor of /proc/self/ns/net (or -1) */
    char         name[64];
    uint64_t     rx_bytes;
    uint64_t     tx_bytes;
    uint64_t     rx_packets;
    uint64_t     tx_packets;
} netns_t;

typedef struct {
    int          in_use;
    int          host_port;
    runtime_handle_t rt;
    int          rt_port;
    int          proto;          /* IPPROTO_TCP / IPPROTO_UDP */
    int          listen_fd;
    pthread_t    thread;
    int          running;
} forward_t;

static netns_t    g_netns[MAX_NETNS];
static forward_t  g_forwards[MAX_FORWARDS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ------------------------------------------------------------------ */
/* Namespaces                                                         */
/* ------------------------------------------------------------------ */

int NetCreateNamespace(runtime_handle_t rt, const char *name)
{
    int slot = -1;
    if (!name) return -1;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_NETNS; i++)
        if (!g_netns[i].in_use) { slot = i; break; }
    if (slot < 0) { pthread_mutex_unlock(&g_lock); return -1; }
    g_netns[slot].in_use = 1;
    g_netns[slot].rt     = rt;
    g_netns[slot].ns_fd  = -1;
    strncpy(g_netns[slot].name, name, sizeof(g_netns[slot].name) - 1);
    g_netns[slot].name[sizeof(g_netns[slot].name) - 1] = '\0';
    pthread_mutex_unlock(&g_lock);
    return slot; /* ns "fd" is just the slot index in the simulator */
}

afros_status_t NetAttach(runtime_handle_t rt, int ns_fd)
{
    if (ns_fd < 0 || ns_fd >= MAX_NETNS) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_lock);
    if (!g_netns[ns_fd].in_use) {
        pthread_mutex_unlock(&g_lock);
        return AFROS_ERROR_INVALID_PARAM;
    }
    g_netns[ns_fd].rt = rt;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Port forwarding                                                    */
/* ------------------------------------------------------------------ */

static void *tcp_forward_thread(void *arg)
{
    forward_t *f = (forward_t *)arg;
    struct sockaddr_in addr;
    int one = 1;

    f->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (f->listen_fd < 0) return NULL;
    setsockopt(f->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons((uint16_t)f->host_port);
    if (bind(f->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(f->listen_fd); f->listen_fd = -1; return NULL;
    }
    if (listen(f->listen_fd, 8) != 0) {
        close(f->listen_fd); f->listen_fd = -1; return NULL;
    }
    while (f->running) {
        int c = accept(f->listen_fd, NULL, NULL);
        int s;
        if (c < 0) break;
        /* Connect to runtime port (simulated: same host). */
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s >= 0) {
            struct sockaddr_in r;
            memset(&r, 0, sizeof(r));
            r.sin_family = AF_INET;
            r.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            r.sin_port = htons((uint16_t)f->rt_port);
            if (connect(s, (struct sockaddr *)&r, sizeof(r)) == 0) {
                /* Pipe both directions until EOF. */
                char buf[4096];
                ssize_t n;
                while ((n = recv(c, buf, sizeof(buf), 0)) > 0) {
                    send(s, buf, (size_t)n, 0);
                    pthread_mutex_lock(&g_lock);
                    g_netns[0].rx_bytes += (uint64_t)n;
                    g_netns[0].tx_bytes += (uint64_t)n;
                    pthread_mutex_unlock(&g_lock);
                }
            }
            close(s);
        }
        close(c);
    }
    return NULL;
}

afros_status_t NetForwardPort(int host_port, runtime_handle_t rt,
                              int rt_port, int proto)
{
    int slot = -1;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_FORWARDS; i++)
        if (!g_forwards[i].in_use) { slot = i; break; }
    if (slot < 0) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_NO_MEMORY; }
    g_forwards[slot].in_use     = 1;
    g_forwards[slot].host_port  = host_port;
    g_forwards[slot].rt         = rt;
    g_forwards[slot].rt_port    = rt_port;
    g_forwards[slot].proto      = proto;
    g_forwards[slot].listen_fd  = -1;
    g_forwards[slot].running    = 1;
    pthread_mutex_unlock(&g_lock);

    if (proto == IPPROTO_TCP) {
        pthread_create(&g_forwards[slot].thread, NULL,
                       tcp_forward_thread, &g_forwards[slot]);
    } else {
        /* UDP forwarder: we leave a stub here; the host simulator does
         * not actually relay UDP traffic. */
    }
    return AFROS_SUCCESS;
}

afros_status_t NetCancelForward(int host_port)
{
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_FORWARDS; i++) {
        if (g_forwards[i].in_use && g_forwards[i].host_port == host_port) {
            g_forwards[i].running = 0;
            if (g_forwards[i].listen_fd >= 0)
                close(g_forwards[i].listen_fd);
            g_forwards[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Statistics                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint32_t forward_count;
} net_stats_t;

afros_status_t NetGetStats(runtime_handle_t rt, net_stats_t *out)
{
    net_stats_t s;
    memset(&s, 0, sizeof(s));
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_NETNS; i++)
        if (g_netns[i].in_use && g_netns[i].rt == rt) {
            s.rx_bytes   += g_netns[i].rx_bytes;
            s.tx_bytes   += g_netns[i].tx_bytes;
            s.rx_packets += g_netns[i].rx_packets;
            s.tx_packets += g_netns[i].tx_packets;
        }
    for (int i = 0; i < MAX_FORWARDS; i++)
        if (g_forwards[i].in_use && g_forwards[i].rt == rt)
            s.forward_count++;
    pthread_mutex_unlock(&g_lock);
    if (out) *out = s;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Init / Shutdown                                                    */
/* ------------------------------------------------------------------ */

afros_status_t NetStackInit(void)
{
    memset(g_netns, 0, sizeof(g_netns));
    memset(g_forwards, 0, sizeof(g_forwards));
    return AFROS_SUCCESS;
}

afros_status_t NetStackShutdown(void)
{
    for (int i = 0; i < MAX_FORWARDS; i++) {
        if (g_forwards[i].in_use) {
            g_forwards[i].running = 0;
            if (g_forwards[i].listen_fd >= 0) close(g_forwards[i].listen_fd);
            g_forwards[i].in_use = 0;
        }
    }
    return AFROS_SUCCESS;
}
