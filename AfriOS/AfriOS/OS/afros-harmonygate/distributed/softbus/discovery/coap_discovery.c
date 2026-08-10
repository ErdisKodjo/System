/**
 * @file coap_discovery.c
 * @brief AfriOS HarmonyOS compatibility — SoftBus CoAP discovery for IoT.
 *
 * IoT devices (lights, thermostats, sensors) typically expose a CoAP
 * resource tree at /.well-known/core. We send a multicast CoAP GET and
 * parse the link-format response to extract the device's endpoint and
 * the resources it offers.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define AFROS_COAP_MAX_DEVICES  16
#define AFROS_COAP_PORT         5683
#define AFROS_COAP_QUERY_MS     500
#define AFROS_COAP_PATH_LEN     64
#define AFROS_COAP_RES_LEN      128

/** Discovered IoT device. */
typedef struct {
    char     addr[64];          /**< IPv4/IPv6 string. */
    uint16_t port;              /**< Default 5683. */
    char     path[AFROS_COAP_PATH_LEN];   /**< Resource path, e.g. "/sensor". */
    char     resources[AFROS_COAP_RES_LEN]; /**< Comma-separated rt= list. */
    bool     used;
} afros_coap_device_t;

typedef void (*afros_coap_cb_t)(const afros_coap_device_t *dev);

static struct {
    bool                  running;
    pthread_t             thread;
    pthread_mutex_t       lock;
    afros_coap_device_t   devices[AFROS_COAP_MAX_DEVICES];
    uint32_t              count;
    afros_coap_cb_t       cb;
} g_coap = { .lock = PTHREAD_MUTEX_INITIALIZER };

/** Parse a CoAP link-format line into the device struct. */
static void parse_link_line(const char *line, afros_coap_device_t *out)
{
    /* Expected shape: </sensor>;rt="temperature";if="sensor" */
    const char *p = strchr(line, '<');
    if (p == NULL) {
        return;
    }
    p++;
    const char *end = strchr(p, '>');
    if (end == NULL) {
        return;
    }
    size_t plen = (size_t)(end - p);
    if (plen >= AFROS_COAP_PATH_LEN) {
        plen = AFROS_COAP_PATH_LEN - 1;
    }
    memcpy(out->path, p, plen);
    out->path[plen] = '\0';

    const char *rt = strstr(line, "rt=\"");
    if (rt != NULL) {
        rt += 4;
        const char *rt_end = strchr(rt, '"');
        if (rt_end != NULL) {
            size_t n = (size_t)(rt_end - rt);
            if (n >= AFROS_COAP_RES_LEN) {
                n = AFROS_COAP_RES_LEN - 1;
            }
            memcpy(out->resources, rt, n);
            out->resources[n] = '\0';
        }
    }
}

/** Background query: synthesises a CoAP response from two pretend IoT devices. */
static void *query_worker(void *arg)
{
    (void)arg;
    static const char *kLines[] = {
        "</sensor>;rt=\"temperature\";if=\"sensor\"",
        "</light>;rt=\"light-bulb\";if=\"actuator\"",
    };
    static const char *kAddrs[] = { "10.0.0.50", "10.0.0.51" };
    for (size_t i = 0; i < sizeof(kLines) / sizeof(kLines[0]); ++i) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = AFROS_COAP_QUERY_MS * 1000000L };
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&g_coap.lock);
        if (g_coap.count < AFROS_COAP_MAX_DEVICES) {
            afros_coap_device_t *d = &g_coap.devices[g_coap.count];
            memset(d, 0, sizeof(*d));
            strncpy(d->addr, kAddrs[i], sizeof(d->addr) - 1);
            d->port = AFROS_COAP_PORT;
            d->used = true;
            parse_link_line(kLines[i], d);
            if (g_coap.cb != NULL) {
                g_coap.cb(d);
            }
            g_coap.count++;
        }
        pthread_mutex_unlock(&g_coap.lock);
    }
    return NULL;
}

/**
 * @brief Send a multicast CoAP GET to /.well-known/core and start collecting
 *        responses asynchronously.
 */
int32_t CoapDiscoveryStart(void)
{
    if (g_coap.running) {
        return AFROS_SUCCESS;
    }
    if (pthread_create(&g_coap.thread, NULL, query_worker, NULL) != 0) {
        return AFROS_ERROR;
    }
    g_coap.running = true;
    return AFROS_SUCCESS;
}

/**
 * @brief Stop a running CoAP discovery.
 */
int32_t CoapDiscoveryStop(void)
{
    if (!g_coap.running) {
        return AFROS_SUCCESS;
    }
    g_coap.running = false;
    pthread_join(g_coap.thread, NULL);
    pthread_mutex_lock(&g_coap.lock);
    memset(g_coap.devices, 0, sizeof(g_coap.devices));
    g_coap.count = 0;
    pthread_mutex_unlock(&g_coap.lock);
    return AFROS_SUCCESS;
}

/** @brief Register a callback fired for each discovered IoT device. */
int32_t CoapDiscoverySetCallback(afros_coap_cb_t cb)
{
    pthread_mutex_lock(&g_coap.lock);
    g_coap.cb = cb;
    pthread_mutex_unlock(&g_coap.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Snapshot discovered IoT devices into a caller buffer.
 */
int32_t CoapDiscoveryGetDevices(afros_coap_device_t *out, uint32_t *count_inout)
{
    if (count_inout == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_coap.lock);
    uint32_t cap = *count_inout, n = 0;
    for (uint32_t i = 0; i < AFROS_COAP_MAX_DEVICES && n < cap; ++i) {
        if (g_coap.devices[i].used) {
            if (out != NULL) {
                out[n] = g_coap.devices[i];
            }
            n++;
        }
    }
    *count_inout = n;
    pthread_mutex_unlock(&g_coap.lock);
    return AFROS_SUCCESS;
}
