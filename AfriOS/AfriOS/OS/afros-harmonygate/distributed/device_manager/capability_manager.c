/**
 * @file capability_manager.c
 * @brief AfriOS HarmonyOS compatibility — per-device capability tracking.
 *
 * Each distributed device publishes a bitmask of capabilities (camera,
 * microphone, sensor array, storage, GPS, display, speaker, etc.). Other
 * subsystems query CapabilityQuery() before requesting hardware sharing.
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>

#define AFROS_CAP_MAX_DEVICES 32
#define AFROS_CAP_NAME_LEN    32

/** Capability bit definitions (HarmonyOS DeviceCapability subset). */
typedef enum {
    AFROS_CAP_NONE       = 0,
    AFROS_CAP_CAMERA     = 1u << 0,
    AFROS_CAP_MICROPHONE = 1u << 1,
    AFROS_CAP_SPEAKER    = 1u << 2,
    AFROS_CAP_DISPLAY    = 1u << 3,
    AFROS_CAP_GPS        = 1u << 4,
    AFROS_CAP_SENSOR     = 1u << 5,
    AFROS_CAP_STORAGE    = 1u << 6,
    AFROS_CAP_NETWORK    = 1u << 7,
    AFROS_CAP_BLUETOOTH  = 1u << 8,
    AFROS_CAP_WIFI       = 1u << 9,
    AFROS_CAP_INPUT      = 1u << 10,
} afros_capability_bit_t;

typedef struct {
    char     device_id[64];
    char     friendly_name[AFROS_CAP_NAME_LEN];
    uint32_t capability_mask;
    bool     valid;
} afros_cap_record_t;

static struct {
    pthread_mutex_t     lock;
    afros_cap_record_t  records[AFROS_CAP_MAX_DEVICES];
    uint32_t            count;
} g_cap = { .lock = PTHREAD_MUTEX_INITIALIZER };

static afros_cap_record_t *find_locked(const char *device_id)
{
    for (uint32_t i = 0; i < AFROS_CAP_MAX_DEVICES; ++i) {
        if (g_cap.records[i].valid &&
            strncmp(g_cap.records[i].device_id, device_id, 63) == 0) {
            return &g_cap.records[i];
        }
    }
    return NULL;
}

/**
 * @brief Query the capability mask of a known device.
 * @param device_id  Target device identifier.
 * @param out_mask   Receives the capability bitmask (may be NULL).
 * @return AFROS_SUCCESS, AFROS_ERROR_INVALID_PARAM, or AFROS_ERROR (unknown device).
 */
int32_t CapabilityQuery(const char *device_id, uint32_t *out_mask)
{
    if (device_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_cap.lock);
    afros_cap_record_t *r = find_locked(device_id);
    int32_t rc;
    if (r == NULL) {
        rc = AFROS_ERROR;
    } else {
        if (out_mask != NULL) {
            *out_mask = r->capability_mask;
        }
        rc = AFROS_SUCCESS;
    }
    pthread_mutex_unlock(&g_cap.lock);
    return rc;
}

/**
 * @brief Register or update a device's capability mask.
 * @param device_id   Target device identifier.
 * @param friendly    Human-readable name (may be NULL).
 * @param cap_mask    Capability bitmask (see afros_capability_bit_t).
 * @return AFROS_SUCCESS or AFROS_ERROR_NO_MEMORY when the table is full.
 */
int32_t CapabilityRegister(const char *device_id,
                           const char *friendly,
                           uint32_t    cap_mask)
{
    if (device_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_cap.lock);
    afros_cap_record_t *r = find_locked(device_id);
    if (r == NULL) {
        for (uint32_t i = 0; i < AFROS_CAP_MAX_DEVICES; ++i) {
            if (!g_cap.records[i].valid) {
                r = &g_cap.records[i];
                memset(r, 0, sizeof(*r));
                r->valid = true;
                strncpy(r->device_id, device_id, 63);
                g_cap.count++;
                break;
            }
        }
        if (r == NULL) {
            pthread_mutex_unlock(&g_cap.lock);
            return AFROS_ERROR_NO_MEMORY;
        }
    }
    r->capability_mask = cap_mask;
    if (friendly != NULL) {
        strncpy(r->friendly_name, friendly, AFROS_CAP_NAME_LEN - 1);
    }
    pthread_mutex_unlock(&g_cap.lock);
    return AFROS_SUCCESS;
}

/**
 * @brief Test whether a device exposes a specific capability.
 * @return 1 if yes, 0 if no, -AFROS_ERROR on unknown device.
 */
int32_t CapabilityHas(const char *device_id, uint32_t cap_bit)
{
    uint32_t mask = 0;
    int32_t rc = CapabilityQuery(device_id, &mask);
    if (rc != AFROS_SUCCESS) {
        return -rc;
    }
    return (mask & cap_bit) ? 1 : 0;
}

/**
 * @brief Remove a device from the capability table.
 */
int32_t CapabilityUnregister(const char *device_id)
{
    if (device_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    pthread_mutex_lock(&g_cap.lock);
    afros_cap_record_t *r = find_locked(device_id);
    if (r != NULL) {
        r->valid = false;
        r->capability_mask = 0;
        if (g_cap.count > 0) {
            g_cap.count--;
        }
    }
    pthread_mutex_unlock(&g_cap.lock);
    return AFROS_SUCCESS;
}

/** @brief Number of devices currently registered. */
uint32_t CapabilityCount(void)
{
    pthread_mutex_lock(&g_cap.lock);
    uint32_t n = g_cap.count;
    pthread_mutex_unlock(&g_cap.lock);
    return n;
}
