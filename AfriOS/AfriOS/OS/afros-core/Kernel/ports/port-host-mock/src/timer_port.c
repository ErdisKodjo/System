/*
 * timer_port.c — Host-mock timer operations.
 *
 * Backed by POSIX clock_gettime(CLOCK_MONOTONIC) + timer_create(SIGEV_THREAD):
 *   - init              -> no-op (returns SUCCESS; honours tick_hz but stores
 *                          it for get_frequency_hz to echo back, matching the
 *                          x86_64 contract)
 *   - get_ticks         -> clock_gettime(CLOCK_MONOTONIC) raw nanoseconds
 *   - get_frequency_hz  -> returns the host-mock timer frequency (1 GHz, i.e.
 *                          the nanosecond resolution of CLOCK_MONOTONIC)
 *   - set_oneshot       -> timer_create + timer_settime(TIME_ABSTIME off),
 *                          one-shot. The callback fires on a dedicated
 *                          SIGEV_THREAD worker thread.
 *   - set_periodic      -> same as set_oneshot but with the it_interval set.
 *   - cancel            -> timer_delete (disarms and frees the OS timer).
 *   - busy_wait_us      -> nanosleep.
 *
 * The HAL test runner (hal_test_runner.c) accepts SUCCESS or NOT_SUPPORTED
 * for set_oneshot/set_periodic. We implement them for real so the host-mock
 * gives meaningful coverage of timer_create + SIGEV_THREAD (not just a stub
 * that always returns NOT_SUPPORTED like the x86_64 port does).
 */
#include "timer_abstraction.h"
#include "port_host_mock.h"

#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

/* Frequency reported by get_frequency_hz(). CLOCK_MONOTONIC raw gives us
 * nanosecond resolution -> 1 GHz. The test runner asserts (hz != 0). */
#define HOST_MOCK_TIMER_HZ  AFROS_HOST_MOCK_TIMER_FREQUENCY_HZ

/* The single active timer — the contract has timer_cancel() take no
 * argument, so we can only have one outstanding oneshot/periodic at a
 * time (matches x86_64's single-channel PIT model). */
static timer_t   s_active_timer    = NULL;
static int       s_timer_armed     = 0;
static uint32_t  s_configured_hz   = HOST_MOCK_TIMER_HZ;

/* SIGEV_THREAD callback bridge: the OS dispatches on a helper thread,
 * we forward to the user callback + ctx captured at set_oneshot time. */
static afros_timer_callback_t s_user_cb  = NULL;
static void                   *s_user_ctx = NULL;

static void timer_dispatch_cb(union sigval sv) {
    (void)sv;
    if (s_user_cb) {
        s_user_cb(s_user_ctx);
    }
}

static afros_status_t timer_init_impl(uint32_t tick_hz) {
    /* No PIT to program on host — just record the requested frequency.
     * If the caller passes 0, keep the default (1 GHz). */
    if (tick_hz != 0) {
        s_configured_hz = tick_hz;
    } else {
        s_configured_hz = HOST_MOCK_TIMER_HZ;
    }
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_ticks_impl(uint64_t *tick_count) {
    if (!tick_count) return AFROS_ERROR_INVALID_PARAM;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return AFROS_ERROR_IO;
    }
    /* Convert to raw ticks (nanoseconds) — matches HOST_MOCK_TIMER_HZ=1e9. */
    *tick_count = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    return AFROS_SUCCESS;
}

static afros_status_t timer_get_frequency_hz_impl(uint32_t *freq_hz) {
    if (!freq_hz) return AFROS_ERROR_INVALID_PARAM;
    *freq_hz = s_configured_hz;
    return AFROS_SUCCESS;
}

static afros_status_t timer_arm_impl(uint64_t period_ticks,
                                     afros_timer_callback_t cb,
                                     void *ctx,
                                     int periodic) {
    /* Cancel any previously-armed timer to mirror the single-channel
     * model: set_oneshot then set_periodic replaces, not stacks. */
    if (s_timer_armed && s_active_timer != NULL) {
        timer_delete(s_active_timer);
        s_active_timer = NULL;
        s_timer_armed = 0;
    }

    s_user_cb  = cb;
    s_user_ctx = ctx;

    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify          = SIGEV_THREAD;
    sev.sigev_notify_function = timer_dispatch_cb;
    sev.sigev_value.sival_ptr = NULL;

    if (timer_create(CLOCK_MONOTONIC, &sev, &s_active_timer) != 0) {
        s_active_timer = NULL;
        /* EAGAIN means we ran out of OS timer slots — degrade gracefully
         * to NOT_SUPPORTED so the test runner skips rather than fails. */
        if (errno == EAGAIN || errno == ENOMEM) return AFROS_ERROR_NOT_SUPPORTED;
        return AFROS_ERROR;
    }

    /* period_ticks is in units of (1 / get_frequency_hz()) seconds.
     * Convert to seconds + nanoseconds for struct itimerspec. */
    uint64_t ns;
    if (s_configured_hz == 0) {
        ns = period_ticks; /* defensive: treat as ns */
    } else {
        ns = (uint64_t)((double)period_ticks * 1e9 / (double)s_configured_hz);
    }
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    its.it_value.tv_sec  = (time_t)(ns / 1000000000ull);
    its.it_value.tv_nsec = (long)(ns % 1000000000ull);
    if (periodic) {
        its.it_interval = its.it_value;
    } else {
        its.it_interval.tv_sec  = 0;
        its.it_interval.tv_nsec = 0;
    }

    if (timer_settime(s_active_timer, 0, &its, NULL) != 0) {
        timer_delete(s_active_timer);
        s_active_timer = NULL;
        return AFROS_ERROR;
    }

    s_timer_armed = 1;
    return AFROS_SUCCESS;
}

static afros_status_t timer_set_oneshot_impl(uint64_t delay_ticks,
                                             afros_timer_callback_t cb,
                                             void *ctx) {
    return timer_arm_impl(delay_ticks, cb, ctx, 0 /* periodic */);
}

static afros_status_t timer_set_periodic_impl(uint64_t period_ticks,
                                              afros_timer_callback_t cb,
                                              void *ctx) {
    return timer_arm_impl(period_ticks, cb, ctx, 1 /* periodic */);
}

static afros_status_t timer_cancel_impl(void) {
    if (s_timer_armed && s_active_timer != NULL) {
        timer_delete(s_active_timer);
        s_active_timer = NULL;
        s_timer_armed  = 0;
    }
    s_user_cb  = NULL;
    s_user_ctx = NULL;
    return AFROS_SUCCESS;
}

static afros_status_t timer_busy_wait_us_impl(uint32_t microseconds) {
    /* Use nanosleep rather than a busy loop — on a CI runner a busy
     * loop steals CPU from the parallel syntax-check workers. The
     * contract just says "wait at least N microseconds". */
    struct timespec req;
    req.tv_sec  = (time_t)(microseconds / 1000000u);
    req.tv_nsec = (long)((microseconds % 1000000u) * 1000u);
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        /* Retry on EINTR — keep waiting until the full interval elapses. */
    }
    return AFROS_SUCCESS;
}

timer_ops_t arch_timer_ops = {
    .init            = timer_init_impl,
    .get_ticks       = timer_get_ticks_impl,
    .get_frequency_hz= timer_get_frequency_hz_impl,
    .set_oneshot     = timer_set_oneshot_impl,
    .set_periodic    = timer_set_periodic_impl,
    .cancel          = timer_cancel_impl,
    .busy_wait_us    = timer_busy_wait_us_impl
};
