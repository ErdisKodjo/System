#include "console_abstraction.h"
#include <stdio.h>

/**
 * @file console_port.c
 * @brief Port x86_64 : UART 16550 (COM1, port I/O 0x3F8 en legacy, MMIO sur serveur).
 */

static afros_status_t console_init_impl(uint32_t baud_rate) {
    printf("[UART] 16550 (COM1, 0x3F8) : diviseur programmé pour %u bauds.\n", baud_rate);
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    putchar(c); // simulerait un out sur le registre THR (0x3F8)
    return AFROS_SUCCESS;
}

static afros_status_t console_puts_impl(const char *s) {
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    fputs(s, stdout);
    return AFROS_SUCCESS;
}

static afros_status_t console_getc_impl(char *c) {
    (void)c;
    return AFROS_ERROR_TIMEOUT; // simulerait LSR.DR == 0
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
