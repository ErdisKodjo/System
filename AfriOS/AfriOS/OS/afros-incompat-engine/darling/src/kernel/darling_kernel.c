/**
 * @file darling_kernel.c
 * @brief Emulates macOS kernel APIs: task_self, mach_port_t, mach_msg.
 *
 * AfriOS does not have a Mach microkernel; this module provides a
 * userspace approximation of the host-side APIs that Darling needs.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Port table                                                          */
/* ------------------------------------------------------------------ */

#define AFROS_MACH_MAX_PORTS 1024

typedef struct {
    uint32_t    id;
    void       *owner;
    int         refcount;
    bool        in_use;
} mach_port_slot_t;

static mach_port_slot_t g_ports[AFROS_MACH_MAX_PORTS];
static pthread_mutex_t  g_port_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t         g_next_port_id = 0x1000;

/* Well-known pseudo-port ids.                                        */
#define AFROS_MACH_TASK_SELF_PORT       0x00000001u
#define AFROS_MACH_HOST_SELF_PORT       0x00000002u
#define AFROS_MACH_BOOTSTRAP_PORT       0x00000003u

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

static bool g_kernel_inited = false;

static void ensure_init(void) {
    if (g_kernel_inited) return;
    pthread_mutex_lock(&g_port_lock);
    if (!g_kernel_inited) {
        g_ports[0].id       = AFROS_MACH_TASK_SELF_PORT;
        g_ports[0].owner    = NULL;
        g_ports[0].refcount = 1;
        g_ports[0].in_use   = true;

        g_ports[1].id       = AFROS_MACH_HOST_SELF_PORT;
        g_ports[1].owner    = NULL;
        g_ports[1].refcount = 1;
        g_ports[1].in_use   = true;

        g_ports[2].id       = AFROS_MACH_BOOTSTRAP_PORT;
        g_ports[2].owner    = NULL;
        g_ports[2].refcount = 1;
        g_ports[2].in_use   = true;
        g_kernel_inited = true;
    }
    pthread_mutex_unlock(&g_port_lock);
}

/* ------------------------------------------------------------------ */
/* Port allocation                                                     */
/* ------------------------------------------------------------------ */

afros_status_t darling_kernel_port_allocate(uint32_t *port_out) {
    if (!port_out) return AFROS_ERROR_INVALID_PARAM;
    ensure_init();
    pthread_mutex_lock(&g_port_lock);
    for (int i = 0; i < AFROS_MACH_MAX_PORTS; i++) {
        if (!g_ports[i].in_use) {
            g_ports[i].id       = ++g_next_port_id;
            g_ports[i].owner    = (void *)pthread_self();
            g_ports[i].refcount = 1;
            g_ports[i].in_use   = true;
            *port_out = g_ports[i].id;
            pthread_mutex_unlock(&g_port_lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_port_lock);
    return AFROS_ERROR_NO_MEMORY;
}

afros_status_t darling_kernel_port_deallocate(uint32_t port) {
    ensure_init();
    pthread_mutex_lock(&g_port_lock);
    for (int i = 0; i < AFROS_MACH_MAX_PORTS; i++) {
        if (g_ports[i].in_use && g_ports[i].id == port) {
            if (--g_ports[i].refcount <= 0) {
                g_ports[i].in_use = false;
                g_ports[i].owner  = NULL;
            }
            pthread_mutex_unlock(&g_port_lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_port_lock);
    return AFROS_ERROR;
}

afros_status_t darling_kernel_port_mod_refs(uint32_t port, int delta) {
    ensure_init();
    pthread_mutex_lock(&g_port_lock);
    for (int i = 0; i < AFROS_MACH_MAX_PORTS; i++) {
        if (g_ports[i].in_use && g_ports[i].id == port) {
            g_ports[i].refcount += delta;
            if (g_ports[i].refcount <= 0) {
                g_ports[i].in_use = false;
                g_ports[i].owner  = NULL;
            }
            pthread_mutex_unlock(&g_port_lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_port_lock);
    return AFROS_ERROR;
}

/* ------------------------------------------------------------------ */
/* task_self                                                           */
/* ------------------------------------------------------------------ */

afros_status_t darling_kernel_task_self(uint32_t *port_out) {
    if (!port_out) return AFROS_ERROR_INVALID_PARAM;
    ensure_init();
    *port_out = AFROS_MACH_TASK_SELF_PORT;
    return AFROS_SUCCESS;
}

afros_status_t darling_kernel_host_self(uint32_t *port_out) {
    if (!port_out) return AFROS_ERROR_INVALID_PARAM;
    ensure_init();
    *port_out = AFROS_MACH_HOST_SELF_PORT;
    return AFROS_SUCCESS;
}

afros_status_t darling_kernel_bootstrap_port(uint32_t *port_out) {
    if (!port_out) return AFROS_ERROR_INVALID_PARAM;
    ensure_init();
    *port_out = AFROS_MACH_BOOTSTRAP_PORT;
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* mach_msg                                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t    msgh_bits;
    uint32_t    msgh_size;
    uint32_t    msgh_remote_port;
    uint32_t    msgh_local_port;
    uint32_t    msgh_id;
} mach_msg_header_t;

afros_status_t darling_kernel_mach_msg(void *msg, size_t len) {
    if (!msg || len < sizeof(mach_msg_header_t)) {
        return AFROS_ERROR_INVALID_PARAM;
    }
    mach_msg_header_t *hdr = (mach_msg_header_t *)msg;
    (void)hdr;
    /* Real Mach would dispatch the message to the destination port. */
    /* AfriOS performs no IPC; the call is accepted as a no-op.       */
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* task/thread info                                                    */
/* ------------------------------------------------------------------ */

afros_status_t darling_kernel_task_info(uint32_t task_port,
                                        int flavor,
                                        void *info_out, size_t *len) {
    (void)task_port; (void)flavor;
    if (!info_out || !len) return AFROS_ERROR_INVALID_PARAM;
    /* Caller asked for info we don't track; zero the buffer.        */
    memset(info_out, 0, *len);
    return AFROS_SUCCESS;
}

afros_status_t darling_kernel_thread_self(uint32_t *port_out) {
    return darling_kernel_port_allocate(port_out);
}

/* ------------------------------------------------------------------ */
/* vm_allocate / vm_deallocate stubs                                   */
/* ------------------------------------------------------------------ */

afros_status_t darling_kernel_vm_allocate(uint32_t task_port,
                                          void **addr, size_t size) {
    (void)task_port;
    if (!addr || !size) return AFROS_ERROR_INVALID_PARAM;
    *addr = calloc(1, size);
    return *addr ? AFROS_SUCCESS : AFROS_ERROR_NO_MEMORY;
}

afros_status_t darling_kernel_vm_deallocate(uint32_t task_port,
                                            void *addr, size_t size) {
    (void)task_port; (void)size;
    if (!addr) return AFROS_ERROR_INVALID_PARAM;
    free(addr);
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Bootstrap registration (look-up by service name)                    */
/* ------------------------------------------------------------------ */

typedef struct bootstrap_entry_s {
    char        name[64];
    uint32_t    port;
    struct bootstrap_entry_s *next;
} bootstrap_entry_t;

static bootstrap_entry_t *g_bootstrap_head = NULL;

afros_status_t darling_kernel_bootstrap_register(const char *name,
                                                 uint32_t port) {
    if (!name) return AFROS_ERROR_INVALID_PARAM;
    bootstrap_entry_t *e = (bootstrap_entry_t *)calloc(1, sizeof *e);
    if (!e) return AFROS_ERROR_NO_MEMORY;
    strncpy(e->name, name, sizeof e->name - 1);
    e->port = port;
    pthread_mutex_lock(&g_port_lock);
    e->next = g_bootstrap_head;
    g_bootstrap_head = e;
    pthread_mutex_unlock(&g_port_lock);
    return AFROS_SUCCESS;
}

afros_status_t darling_kernel_bootstrap_look_up(const char *name,
                                                uint32_t *port_out) {
    if (!name || !port_out) return AFROS_ERROR_INVALID_PARAM;
    pthread_mutex_lock(&g_port_lock);
    for (bootstrap_entry_t *e = g_bootstrap_head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            *port_out = e->port;
            pthread_mutex_unlock(&g_port_lock);
            return AFROS_SUCCESS;
        }
    }
    pthread_mutex_unlock(&g_port_lock);
    return AFROS_ERROR;
}
