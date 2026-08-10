#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>

#include "../include/orchestrator.h"
#include "../include/runtime_manager.h"
#include "../include/loader.h"

/**
 * @file monitoring.c
 * @brief Background monitor for the orchestrator: per-runtime CPU and
 *        memory stats, fault counters, and a watchdog that kills any
 *        runtime which stops heartbeating.
 *
 * The monitor thread wakes up every MONITOR_INTERVAL_MS milliseconds,
 * samples each registered runtime, increments fault counters when a
 * runtime's pid has exited unexpectedly, and runs the watchdog check.
 */

#define MONITOR_INTERVAL_MS 1000
#define WATCHDOG_TIMEOUT_MS 5000
#define MAX_MONITORED       32

struct monitored_rt {
    int              in_use;
    runtime_handle_t handle;
    pid_t            pid;
    uint64_t         last_heartbeat_ms;
    uint64_t         cpu_time_ms;
    uint64_t         mem_used_bytes;
    uint32_t         faults;
    int              alive;
};

static struct monitored_rt g_rt[MAX_MONITORED];
static pthread_t           g_thread;
static pthread_mutex_t     g_lock = PTHREAD_MUTEX_INITIALIZER;
static int                 g_running = 0;
static uint64_t            g_start_ms;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static struct monitored_rt *slot_find_locked(runtime_handle_t rt)
{
    for (int i = 0; i < MAX_MONITORED; i++)
        if (g_rt[i].in_use && g_rt[i].handle == rt)
            return &g_rt[i];
    return NULL;
}

static struct monitored_rt *slot_alloc_locked(runtime_handle_t rt, pid_t pid)
{
    struct monitored_rt *s = slot_find_locked(rt);
    if (s) {
        s->pid = pid;
        return s;
    }
    for (int i = 0; i < MAX_MONITORED; i++)
        if (!g_rt[i].in_use) {
            memset(&g_rt[i], 0, sizeof(g_rt[i]));
            g_rt[i].in_use = 1;
            g_rt[i].handle = rt;
            g_rt[i].pid    = pid;
            g_rt[i].last_heartbeat_ms = now_ms();
            g_rt[i].alive  = 1;
            return &g_rt[i];
        }
    return NULL;
}

/* Read /proc/<pid>/statm to get RSS in bytes. Returns 0 on failure. */
static uint64_t sample_rss(pid_t pid)
{
    char path[64];
    char buf[256];
    FILE *fp;
    unsigned long sz = 0, rss_pages = 0;
    snprintf(path, sizeof(path), "/proc/%lu/statm", (unsigned long)pid);
    fp = fopen(path, "r");
    if (!fp) return 0;
    if (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%lu %lu", &sz, &rss_pages);
    }
    fclose(fp);
    return (uint64_t)rss_pages * 4096; /* page size = 4 KiB on most Linux */
}

/* Read /proc/<pid>/stat to get utime+stime in clock ticks. */
static uint64_t sample_cpu_ms(pid_t pid)
{
    char path[64];
    char buf[1024];
    FILE *fp;
    unsigned long utime = 0, stime = 0;
    long clk = sysconf(_SC_CLK_TCK);
    snprintf(path, sizeof(path), "/proc/%lu/stat", (unsigned long)pid);
    fp = fopen(path, "r");
    if (!fp) return 0;
    if (fgets(buf, sizeof(buf), fp)) {
        /* The fields we want are after the comm field (in parens). */
        char *p = strrchr(buf, ')');
        if (p) {
            /* Skip " state ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime ..." */
            unsigned long minflt, cminflt, majflt, cmajflt;
            sscanf(p + 2,
                   "%*c %*d %*d %*d %*d %*d %*u %lu %lu %lu %lu %lu %lu",
                   &minflt, &cminflt, &majflt, &cmajflt, &utime, &stime);
            (void)minflt; (void)cminflt; (void)majflt; (void)cmajflt;
        }
    }
    fclose(fp);
    if (clk <= 0) clk = 100;
    return ((uint64_t)utime + (uint64_t)stime) * 1000ULL / (uint64_t)clk;
}

/* ------------------------------------------------------------------ */
/* Monitor loop                                                       */
/* ------------------------------------------------------------------ */

static void *monitor_loop(void *arg)
{
    (void)arg;
    while (g_running) {
        uint64_t now = now_ms();
        pthread_mutex_lock(&g_lock);
        for (int i = 0; i < MAX_MONITORED; i++) {
            struct monitored_rt *s = &g_rt[i];
            if (!s->in_use) continue;
            if (s->pid <= 0) continue;
            /* Sample CPU and memory. */
            s->cpu_time_ms     = sample_cpu_ms(s->pid);
            s->mem_used_bytes  = sample_rss(s->pid);
            /* Check liveness: kill(pid, 0) returns 0 if the pid exists. */
            if (kill(s->pid, 0) != 0) {
                s->alive = 0;
                s->faults++;
            } else {
                s->alive = 1;
                s->last_heartbeat_ms = now;
            }
        }
        pthread_mutex_unlock(&g_lock);
        usleep(MONITOR_INTERVAL_MS * 1000);
    }
    return NULL;
}

static void *watchdog_loop(void *arg)
{
    (void)arg;
    while (g_running) {
        uint64_t now = now_ms();
        pthread_mutex_lock(&g_lock);
        for (int i = 0; i < MAX_MONITORED; i++) {
            struct monitored_rt *s = &g_rt[i];
            if (!s->in_use || s->pid <= 0) continue;
            if (now - s->last_heartbeat_ms > WATCHDOG_TIMEOUT_MS) {
                /* Watchdog trip: kill the runtime. */
                kill(s->pid, SIGKILL);
                s->faults++;
                s->alive = 0;
            }
        }
        pthread_mutex_unlock(&g_lock);
        usleep(MONITOR_INTERVAL_MS * 1000);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

afros_status_t MonitorStart(void)
{
    if (g_running) return AFROS_SUCCESS;
    g_start_ms = now_ms();
    g_running  = 1;
    if (pthread_create(&g_thread, NULL, monitor_loop, NULL) != 0) {
        g_running = 0;
        return AFROS_ERROR;
    }
    return AFROS_SUCCESS;
}

afros_status_t MonitorStop(void)
{
    if (!g_running) return AFROS_SUCCESS;
    g_running = 0;
    pthread_join(g_thread, NULL);
    return AFROS_SUCCESS;
}

afros_status_t MonitorRegister(runtime_handle_t rt, pid_t pid)
{
    pthread_mutex_lock(&g_lock);
    if (!slot_alloc_locked(rt, pid)) {
        pthread_mutex_unlock(&g_lock);
        return AFROS_ERROR_NO_MEMORY;
    }
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t MonitorUnregister(runtime_handle_t rt)
{
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_MONITORED; i++)
        if (g_rt[i].in_use && g_rt[i].handle == rt)
            g_rt[i].in_use = 0;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t MonitorHeartbeat(runtime_handle_t rt)
{
    struct monitored_rt *s;
    pthread_mutex_lock(&g_lock);
    s = slot_find_locked(rt);
    if (!s) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    s->last_heartbeat_ms = now_ms();
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t MonitorGetStats(runtime_handle_t rt, uint64_t *cpu_ms,
                               uint64_t *mem_bytes, uint32_t *faults,
                               int *alive)
{
    struct monitored_rt *s;
    pthread_mutex_lock(&g_lock);
    s = slot_find_locked(rt);
    if (!s) { pthread_mutex_unlock(&g_lock); return AFROS_ERROR_INVALID_PARAM; }
    if (cpu_ms)     *cpu_ms     = s->cpu_time_ms;
    if (mem_bytes)  *mem_bytes  = s->mem_used_bytes;
    if (faults)     *faults     = s->faults;
    if (alive)      *alive      = s->alive;
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}

afros_status_t MonitorWatchdog(void)
{
    /* One-shot watchdog pass: kill any runtime that has timed out. */
    uint64_t now = now_ms();
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < MAX_MONITORED; i++) {
        struct monitored_rt *s = &g_rt[i];
        if (!s->in_use || s->pid <= 0) continue;
        if (now - s->last_heartbeat_ms > WATCHDOG_TIMEOUT_MS) {
            kill(s->pid, SIGKILL);
            s->faults++;
            s->alive = 0;
        }
    }
    pthread_mutex_unlock(&g_lock);
    return AFROS_SUCCESS;
}
