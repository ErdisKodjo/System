/**
 * @file versioning.c
 * @brief AfriOS HarmonyOS compatibility — vector-clock versioning for DDS.
 *
 * Each distributed-data key carries a vector clock with one entry per device
 * that has written it. SyncEngine compares clocks to detect conflicts:
 *   - ClockA < ClockB  → B dominates A (no conflict, take B).
 *   - ClockA > ClockB  → A dominates B (no conflict, take A).
 *   - Equal            → identical versions.
 *   - Otherwise        → concurrent (conflict).
 */

#include "afros_harmony.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define AFROS_VC_MAX_NODES 16

/** Single (node_id, count) pair. */
typedef struct {
    char     node_id[32];
    uint64_t count;
} afros_vc_entry_t;

/** Vector clock: list of (node_id, count) entries. */
typedef struct {
    afros_vc_entry_t entries[AFROS_VC_MAX_NODES];
    uint32_t         size;
} afros_vc_t;

/** Comparison result. */
typedef enum {
    AFROS_VC_EQUAL       = 0,
    AFROS_VC_BEFORE      = -1,  /**< Strictly less than (older). */
    AFROS_VC_AFTER       =  1,  /**< Strictly greater than (newer). */
    AFROS_VC_CONCURRENT  =  2,  /**< Neither dominates the other. */
} afros_vc_cmp_t;

/**
 * @brief Initialise a fresh vector clock for a node.
 */
int32_t VersionNew(afros_vc_t *vc, const char *local_node_id)
{
    if (vc == NULL || local_node_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    memset(vc, 0, sizeof(*vc));
    strncpy(vc->entries[0].node_id, local_node_id,
            sizeof(vc->entries[0].node_id) - 1);
    vc->entries[0].count = 1;
    vc->size = 1;
    return AFROS_SUCCESS;
}

/** Look up the count for a node in a clock (0 if absent). */
static uint64_t vc_lookup(const afros_vc_t *vc, const char *node_id)
{
    for (uint32_t i = 0; i < vc->size; ++i) {
        if (strncmp(vc->entries[i].node_id, node_id,
                    sizeof(vc->entries[0].node_id)) == 0) {
            return vc->entries[i].count;
        }
    }
    return 0;
}

/**
 * @brief Compare two vector clocks.
 * @return One of AFROS_VC_EQUAL / BEFORE / AFTER / CONCURRENT.
 */
int32_t VersionCompare(const afros_vc_t *a, const afros_vc_t *b)
{
    if (a == NULL || b == NULL) {
        return AFROS_VC_CONCURRENT;
    }
    bool a_greater = false;
    bool b_greater = false;
    for (uint32_t i = 0; i < a->size; ++i) {
        uint64_t bv = vc_lookup(b, a->entries[i].node_id);
        if (a->entries[i].count > bv) {
            a_greater = true;
        } else if (a->entries[i].count < bv) {
            b_greater = true;
        }
    }
    for (uint32_t i = 0; i < b->size; ++i) {
        uint64_t av = vc_lookup(a, b->entries[i].node_id);
        if (b->entries[i].count > av) {
            b_greater = true;
        } else if (b->entries[i].count < av) {
            a_greater = true;
        }
    }
    if (!a_greater && !b_greater) {
        return AFROS_VC_EQUAL;
    }
    if (a_greater && !b_greater) {
        return AFROS_VC_AFTER;
    }
    if (!a_greater && b_greater) {
        return AFROS_VC_BEFORE;
    }
    return AFROS_VC_CONCURRENT;
}

/** Ensure a node entry exists in the clock; returns its index. */
static uint32_t vc_ensure(afros_vc_t *vc, const char *node_id)
{
    for (uint32_t i = 0; i < vc->size; ++i) {
        if (strncmp(vc->entries[i].node_id, node_id,
                    sizeof(vc->entries[0].node_id)) == 0) {
            return i;
        }
    }
    if (vc->size >= AFROS_VC_MAX_NODES) {
        return AFROS_VC_MAX_NODES; /* Overflow — caller must handle. */
    }
    uint32_t idx = vc->size++;
    strncpy(vc->entries[idx].node_id, node_id,
            sizeof(vc->entries[0].node_id) - 1);
    vc->entries[idx].count = 0;
    return idx;
}

/**
 * @brief Merge two vector clocks (component-wise max) into @p out.
 * @return AFROS_SUCCESS or AFROS_ERROR_NO_MEMORY on overflow.
 */
int32_t VersionMerge(const afros_vc_t *a, const afros_vc_t *b, afros_vc_t *out)
{
    if (a == NULL || b == NULL || out == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    memset(out, 0, sizeof(*out));
    for (uint32_t i = 0; i < a->size; ++i) {
        if (out->size >= AFROS_VC_MAX_NODES) {
            return AFROS_ERROR_NO_MEMORY;
        }
        strncpy(out->entries[out->size].node_id, a->entries[i].node_id,
                sizeof(out->entries[0].node_id) - 1);
        out->entries[out->size].count = a->entries[i].count;
        out->size++;
    }
    for (uint32_t i = 0; i < b->size; ++i) {
        uint32_t idx = vc_ensure(out, b->entries[i].node_id);
        if (idx >= AFROS_VC_MAX_NODES) {
            return AFROS_ERROR_NO_MEMORY;
        }
        if (b->entries[i].count > out->entries[idx].count) {
            out->entries[idx].count = b->entries[i].count;
        }
    }
    return AFROS_SUCCESS;
}

/**
 * @brief Increment the local component of a vector clock.
 */
int32_t VersionIncrement(afros_vc_t *vc, const char *node_id)
{
    if (vc == NULL || node_id == NULL) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    uint32_t idx = vc_ensure(vc, node_id);
    if (idx >= AFROS_VC_MAX_NODES) {
        return AFROS_ERROR_NO_MEMORY;
    }
    vc->entries[idx].count += 1;
    return AFROS_SUCCESS;
}

/**
 * @brief Serialise a vector clock into a flat byte buffer.
 * @return Bytes written, or -AFROS_ERROR on overflow.
 */
int32_t VersionSerialize(const afros_vc_t *vc, uint8_t *buf, uint32_t cap)
{
    if (vc == NULL || buf == NULL) {
        return -AFROS_ERROR_INVALID_PARAM;
    }
    uint32_t off = 0;
    if (cap < 4) {
        return -AFROS_ERROR_NO_MEMORY;
    }
    buf[off++] = (uint8_t)(vc->size & 0xFF);
    buf[off++] = (uint8_t)((vc->size >> 8) & 0xFF);
    buf[off++] = (uint8_t)((vc->size >> 16) & 0xFF);
    buf[off++] = (uint8_t)((vc->size >> 24) & 0xFF);
    for (uint32_t i = 0; i < vc->size; ++i) {
        if (off + 32 + 8 > cap) {
            return -AFROS_ERROR_NO_MEMORY;
        }
        memcpy(buf + off, vc->entries[i].node_id, 32); off += 32;
        uint64_t v = vc->entries[i].count;
        for (int b = 0; b < 8; ++b) {
            buf[off++] = (uint8_t)((v >> (b * 8)) & 0xFF);
        }
    }
    return (int32_t)off;
}
