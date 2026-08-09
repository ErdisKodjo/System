#include "console_abstraction.h"
#include <stdio.h>

/**
 * @file console_port.c
 * @brief Port ARM64 : UART PL011 (console série d'amorçage).
 */

static afros_status_t console_init_impl(uint32_t baud_rate) {
    printf("[UART] PL011 : initialisation à %u bauds (UARTIBRD/UARTFBRD).\n", baud_rate);
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    putchar(c); // simulerait un write sur UARTDR
    return AFROS_SUCCESS;
}

static afros_status_t console_puts_impl(const char *s) {
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    fputs(s, stdout);
    return AFROS_SUCCESS;
}

static afros_status_t console_getc_impl(char *c) {
    (void)c;
    return AFROS_ERROR_TIMEOUT; // simulerait UARTFR.RXFE (RX FIFO vide)
}

static bool console_has_input_impl(void) {
    return false;
}

console_ops_t arch_console_ops = {
    .init = console_init_impl,
    .putc = console_putc_impl,
    .puts = console_puts_impl,
    .getc = console_getc_impl,
    .has_input = console_has_input_impl
};
