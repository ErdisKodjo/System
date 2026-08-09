#include "console_abstraction.h"
#include <stdio.h>

/**
 * @file console_port.c
 * @brief Port MCU : USART périphérique (ex. USART1 sur STM32), registres directs.
 */

static afros_status_t console_init_impl(uint32_t baud_rate) {
    printf("[UART] MCU : USART->BRR calculé pour %u bauds (horloge APB connue au build).\n", baud_rate);
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    putchar(c); // simulerait l'attente USART->SR.TXE puis écriture USART->DR
    return AFROS_SUCCESS;
}

static afros_status_t console_puts_impl(const char *s) {
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    fputs(s, stdout);
    return AFROS_SUCCESS;
}

static afros_status_t console_getc_impl(char *c) {
    (void)c;
    return AFROS_ERROR_TIMEOUT; // simulerait USART->SR.RXNE == 0
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
