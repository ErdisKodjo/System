#include "console_abstraction.h"
#include <stdio.h>

/**
 * @file console_port.c
 * @brief Port RISC-V : console via SBI Legacy Console (ecall) en mode superviseur,
 *        repli UART 16550-compatible si le firmware SBI n'expose pas le service.
 */

static afros_status_t console_init_impl(uint32_t baud_rate) {
    printf("[UART] RISC-V : SBI console détectée (pas de baud rate à programmer) ; "
           "repli 16550 à %u bauds sinon.\n", baud_rate);
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    putchar(c); // simulerait sbi_console_putchar() ou écriture UART 16550
    return AFROS_SUCCESS;
}

static afros_status_t console_puts_impl(const char *s) {
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    fputs(s, stdout);
    return AFROS_SUCCESS;
}

static afros_status_t console_getc_impl(char *c) {
    (void)c;
    return AFROS_ERROR_TIMEOUT; // simulerait sbi_console_getchar() == -1
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
