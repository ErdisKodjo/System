/*
 * console_port.c — Host-mock console implementation.
 *
 * Userspace-safe backing for arch_console_ops using libc stdio:
 *   - putc   -> fputc(c, stdout)
 *   - puts   -> fputs(s, stdout)
 *   - getc   -> fgetc(stdin), non-blocking via select(0 timeout)
 *   - has_input -> select() on STDIN with 0 timeout
 *   - init   -> no-op (stdout/stdin are already wired by libc at _start)
 *
 * The contract (console_abstraction.h) requires getc() to be non-blocking
 * and to return AFROS_ERROR_TIMEOUT when no input is available. We achieve
 * that with a zero-timeout select() on fd 0 (STDIN) so the test runner
 * can call arch_console_ops.getc() without ever hanging.
 */
#include "console_abstraction.h"
#include "port_host_mock.h"

#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>

static afros_status_t console_init_impl(uint32_t baud_rate) {
    /* No-op on host — libc already wires stdout/stdin. The baud_rate
     * parameter is meaningful for a real UART (16550 / PL011) but has
     * no equivalent on a userspace terminal. */
    (void)baud_rate;
    /* Line-buffer stdout so the test runner's "[PASS]/[FAIL]" lines
     * appear immediately rather than being deferred to exit. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    if (fputc((unsigned char)c, stdout) == EOF) {
        return AFROS_ERROR_IO;
    }
    return AFROS_SUCCESS;
}

static afros_status_t console_puts_impl(const char *s) {
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    if (fputs(s, stdout) == EOF) {
        return AFROS_ERROR_IO;
    }
    /* Match the behaviour of every real UART port: puts() does not
     * implicitly append '\n' — the caller is responsible for that. */
    return AFROS_SUCCESS;
}

static afros_status_t console_getc_impl(char *c) {
    if (!c) return AFROS_ERROR_INVALID_PARAM;

    /* Non-blocking probe on STDIN via select() with a 0 timeout. */
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    if (rc < 0) {
        if (errno == EINTR) return AFROS_ERROR_TIMEOUT;
        return AFROS_ERROR_IO;
    }
    if (rc == 0 || !FD_ISSET(STDIN_FILENO, &rfds)) {
        return AFROS_ERROR_TIMEOUT;
    }

    int ch = fgetc(stdin);
    if (ch == EOF) {
        return AFROS_ERROR_TIMEOUT;
    }
    *c = (char)ch;
    return AFROS_SUCCESS;
}

static bool console_has_input_impl(void) {
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int rc = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    return (rc > 0) && FD_ISSET(STDIN_FILENO, &rfds);
}

console_ops_t arch_console_ops = {
    .init      = console_init_impl,
    .putc      = console_putc_impl,
    .puts      = console_puts_impl,
    .getc      = console_getc_impl,
    .has_input = console_has_input_impl
};
