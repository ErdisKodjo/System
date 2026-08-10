/*
 * vfs/properties.c — System properties.
 *
 * Android stores system-wide configuration in /dev/__properties__ — a
 * shared-memory area holding a flat array of (key, value) records. Apps
 * read properties via __system_property_get() (or the higher-level
 * SystemProperties.getString() Java API), and the init process writes
 * them at boot. Properties prefixed "ro." are read-only and persist
 * across reboots; "persist." properties survive reboots by being written
 * to /data/property/.
 *
 * This module emulates that interface in-process: a global key→value
 * map, get/set helpers, and on-disk persistence of "persist." keys.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROP_NAME_MAX   92
#define PROP_VALUE_MAX  128
#define PROP_TABLE_MAX  1024

struct prop_entry {
    int   in_use;
    char  name[PROP_NAME_MAX];
    char  value[PROP_VALUE_MAX];
};

static struct prop_entry g_props[PROP_TABLE_MAX];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_persist_dir[256] = "/var/lib/afros-androsandbox/property";

static int find_slot_locked(const char *name) {
    for (size_t i = 0; i < PROP_TABLE_MAX; i++) {
        if (g_props[i].in_use && strcmp(g_props[i].name, name) == 0)
            return (int)i;
    }
    return -1;
}

static int alloc_slot_locked(const char *name) {
    for (size_t i = 0; i < PROP_TABLE_MAX; i++) {
        if (!g_props[i].in_use) {
            g_props[i].in_use = 1;
            snprintf(g_props[i].name, sizeof(g_props[i].name), "%s", name);
            g_props[i].value[0] = 0;
            return (int)i;
        }
    }
    return -1;
}

/* Persist the value of a "persist." property to disk. */
static void persist_to_disk_locked(const char *name, const char *value) {
    char path[300];
    FILE *f;
    mkdir(g_persist_dir, 0755);
    /* Sanitize the name (replace '.' with '_'). */
    char safe[PROP_NAME_MAX];
    snprintf(safe, sizeof(safe), "%s", name);
    for (char *p = safe; *p; p++) if (*p == '.') *p = '_';
    snprintf(path, sizeof(path), "%s/%s", g_persist_dir, safe);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", value);
        fclose(f);
    }
}

static void load_persisted_locked(void) {
    /* Walk the persist dir; each file's name (with '_' -> '.') is the
     * property name and its contents are the value. In the sandbox we
     * skip this on first boot — defaults are seeded by seed_defaults_locked. */
}

/* Seed a handful of standard ro.build.* properties. */
static void seed_defaults_locked(void) {
    static const char *kDefaults[][2] = {
        {"ro.build.version.release", "14"},
        {"ro.build.version.sdk",     "34"},
        {"ro.build.version.incremental", "afros-1"},
        {"ro.product.model",         "AfriOS Sandbox"},
        {"ro.product.manufacturer",  "AfriOS"},
        {"ro.product.brand",         "afros"},
        {"ro.product.name",          "afros_androsandbox"},
        {"ro.product.cpu.abi",       "arm64-v8a"},
        {"ro.product.cpu.abilist",   "arm64-v8a,armeabi-v7a,armeabi"},
        {"ro.build.fingerprint",     "afros/afros/afros:14/afros-1/eng"},
        {"ro.hardware",              "afros"},
        {"ro.bootmode",              "normal"},
        {"ro.debuggable",            "1"},
        {"ro.secure",                "0"},
        {"ro.treble.enabled",        "false"},
        {"ro.zygote",                "zygote64_32"},
        {NULL, NULL},
    };
    for (size_t i = 0; kDefaults[i][0]; i++) {
        int idx = find_slot_locked(kDefaults[i][0]);
        if (idx < 0) idx = alloc_slot_locked(kDefaults[i][0]);
        if (idx >= 0) {
            snprintf(g_props[idx].value, sizeof(g_props[idx].value),
                     "%s", kDefaults[i][1]);
        }
    }
}

int PropertyGet(const char *name, char *value, size_t value_max) {
    int idx;
    if (!name || !value || value_max == 0) return -EINVAL;
    pthread_mutex_lock(&g_lock);
    if (g_props[0].in_use == 0 && g_props[1].in_use == 0) {
        /* Lazy init: seed defaults on first call. */
        seed_defaults_locked();
        load_persisted_locked();
    }
    idx = find_slot_locked(name);
    if (idx < 0) {
        pthread_mutex_unlock(&g_lock);
        value[0] = 0;
        return 0;
    }
    snprintf(value, value_max, "%s", g_props[idx].value);
    pthread_mutex_unlock(&g_lock);
    return (int)strlen(value);
}

int PropertySet(const char *name, const char *value) {
    int idx;
    if (!name || !value) return -EINVAL;
    if (strlen(name) >= PROP_NAME_MAX) return -EINVAL;
    if (strlen(value) >= PROP_VALUE_MAX) return -EINVAL;
    pthread_mutex_lock(&g_lock);
    if (g_props[0].in_use == 0) seed_defaults_locked();
    /* "ro." properties are immutable once set. */
    if (strncmp(name, "ro.", 3) == 0) {
        idx = find_slot_locked(name);
        if (idx >= 0 && g_props[idx].value[0] != 0) {
            pthread_mutex_unlock(&g_lock);
            return -EACCES;
        }
    }
    idx = find_slot_locked(name);
    if (idx < 0) idx = alloc_slot_locked(name);
    if (idx < 0) { pthread_mutex_unlock(&g_lock); return -ENOMEM; }
    snprintf(g_props[idx].value, sizeof(g_props[idx].value), "%s", value);
    if (strncmp(name, "persist.", 8) == 0) {
        persist_to_disk_locked(name, value);
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}

size_t PropertyList(char (*names)[PROP_NAME_MAX], size_t max) {
    size_t n = 0;
    pthread_mutex_lock(&g_lock);
    if (g_props[0].in_use == 0) seed_defaults_locked();
    for (size_t i = 0; i < PROP_TABLE_MAX && n < max; i++) {
        if (g_props[i].in_use) {
            snprintf(names[n], PROP_NAME_MAX, "%s", g_props[i].name);
            n++;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return n;
}

void PropertySetPersistDir(const char *dir) {
    pthread_mutex_lock(&g_lock);
    if (dir) snprintf(g_persist_dir, sizeof(g_persist_dir), "%s", dir);
    pthread_mutex_unlock(&g_lock);
}
