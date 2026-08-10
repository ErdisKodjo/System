#include "console_abstraction.h"

/**
 * @file console_port.c
 * @brief Port ARM64 : UART PL011 (console série d'amorçage).
 *
 * Implémentation freestanding : accès MMIO direct aux registres PL011, aucune
 * dépendance libc. Convient à QEMU virt (base 0x9000000)
 * ainsi qu'aux SoC ARM64 réels exposant un PL011 — surcharger PL011_BASE si
 * l'adresse diffère (ex. 0x09000000 sur certains SoC, 0xFFC02000 sur d'autres).
 */

#ifndef PL011_BASE
/** Base MMIO du PL011 — QEMU « virt » par défaut. Surcharger via -DPL011_BASE=0x... */
#define PL011_BASE 0x9000000UL
#endif

#define PL011_DR   (PL011_BASE + 0x00UL)  /* Data Register (write = TX, read = RX) */
#define PL011_RSR  (PL011_BASE + 0x04UL)  /* Receive Status Register */
#define PL011_FR   (PL011_BASE + 0x18UL)  /* Flag Register */
#define PL011_ILPR (PL011_BASE + 0x20UL)
#define PL011_IBRD (PL011_BASE + 0x24UL)  /* Integer Baud Rate Divisor */
#define PL011_FBRD (PL011_BASE + 0x28UL)  /* Fractional Baud Rate Divisor */
#define PL011_LCRH (PL011_BASE + 0x2CUL)  /* Line Control (H) */
#define PL011_CR   (PL011_BASE + 0x30UL)  /* Control Register */
#define PL011_IFLS (PL011_BASE + 0x34UL)
#define PL011_IMSC (PL011_BASE + 0x38UL)
#define PL011_ICR  (PL011_BASE + 0x44UL)  /* Interrupt Clear Register */

#define FR_TXFF    (1u << 5)              /* Transmit FIFO full */
#define FR_RXFE    (1u << 4)              /* Receive FIFO empty */
#define FR_BUSY    (1u << 3)

#define LCRH_FEN   (1u << 4)              /* FIFO Enable */
#define LCRH_WLEN8 (3u << 5)              /* 8 bits par caractère */

#define CR_UARTEN  (1u << 0)              /* UART Enable */
#define CR_TXE     (1u << 8)              /* Transmit Enable */
#define CR_RXE     (1u << 9)              /* Receive Enable */

/** Horloge de référence du PL011 — QEMU virt utilise 24 MHz par défaut. */
#ifndef PL011_UARTCLK
#define PL011_UARTCLK 24000000UL
#endif

static inline void mmio_write(uint32_t addr, uint32_t val) {
    *(volatile uint32_t *)(uintptr_t)addr = val;
}

static inline uint32_t mmio_read(uint32_t addr) {
    return *(volatile uint32_t *)(uintptr_t)addr;
}

/** Émet un octet sur la ligne en attendant que le TX FIFO ait de la place. */
static void uart_send_byte(uint8_t c) {
    while (mmio_read(PL011_FR) & FR_TXFF) {
        /* spin-wait : TX FIFO plein. */
    }
    mmio_write(PL011_DR, (uint32_t)c);
}

static afros_status_t console_init_impl(uint32_t baud_rate) {
    if (baud_rate == 0) baud_rate = 115200;

    /* Diviseur baud : baud_div = UARTCLK / (16 * baud), split en entier/fractionnel.
     * Arrondi au plus proche (formule PL011 TRM). */
    uint32_t baud_divisor = ((PL011_UARTCLK * 4u) + (baud_rate * 2u)) / (baud_rate * 16u);
    uint32_t ibrd = baud_divisor >> 6;
    uint32_t fbrd = baud_divisor & 0x3Fu;

    mmio_write(PL011_CR, 0);                          /* désactiver le temps de la config */
    mmio_write(PL011_ICR, 0x7FFu);                    /* clear toutes les IRQ pendantes */
    mmio_write(PL011_IBRD, ibrd);
    mmio_write(PL011_FBRD, fbrd);
    mmio_write(PL011_LCRH, LCRH_WLEN8 | LCRH_FEN);    /* 8N1 + FIFO */
    mmio_write(PL011_IMSC, 0);                        /* pas d'IRQ (mode polling) */
    mmio_write(PL011_CR, CR_UARTEN | CR_TXE | CR_RXE);
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    /* Traduire \n en \r\n pour les terminaux série classiques. */
    if (c == '\n') {
        uart_send_byte('\r');
    }
    uart_send_byte((uint8_t)c);
    return AFROS_SUCCESS;
}

static afros_status_t console_puts_impl(const char *s) {
    if (!s) return AFROS_ERROR_INVALID_PARAM;
    while (*s) {
        console_putc_impl(*s++);
    }
    return AFROS_SUCCESS;
}

static afros_status_t console_getc_impl(char *c) {
    if (!c) return AFROS_ERROR_INVALID_PARAM;
    if (mmio_read(PL011_FR) & FR_RXFE) {
        return AFROS_ERROR_TIMEOUT;  /* RX FIFO vide */
    }
    *c = (char)(mmio_read(PL011_DR) & 0xFFu);
    return AFROS_SUCCESS;
}

static bool console_has_input_impl(void) {
    return (mmio_read(PL011_FR) & FR_RXFE) == 0;
}

console_ops_t arch_console_ops = {
    .init = console_init_impl,
    .putc = console_putc_impl,
    .puts = console_puts_impl,
    .getc = console_getc_impl,
    .has_input = console_has_input_impl
};
