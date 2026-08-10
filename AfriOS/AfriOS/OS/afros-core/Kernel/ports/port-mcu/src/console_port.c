#include "console_abstraction.h"

/**
 * @file console_port.c
 * @brief Port MCU : USART mémoire-mappé (style STM32) en accès registre direct.
 *
 * Implémentation freestanding : aucun libc, polling sur les bits d'état.
 *
 * IMPORTANT : ajuster USART1_BASE à l'USART réellement utilisé sur la cible.
 * - STM32F1 : USART1 @ 0x40011000 (APB2)
 * - STM32F4 : USART1 @ 0x40011000 (APB2)
 * - STM32L4 : USART1 @ 0x40013800 (APB2)
 * - STM32G4 : USART1 @ 0x40013800
 * Les offsetsregistre (SR/DR/BRR/CR1) sont ceux de la famille F1/F4. Les
 * familles plus récentes (L4/G4/H7) utilisent ISR/TDR/RDR/CR1 à la place ;
 * adapter les #define ci-dessous dans ce cas.
 */

#ifndef USART1_BASE
/** @note ajuster à la cible MCU réelle — voir commentaires ci-dessus. */
#define USART1_BASE 0x40011000UL
#endif

/* Offsets registres — convention STM32F1/F4 (SR=+0, DR=+4, BRR=+8, CR1=+0xC). */
#define USART_SR   (USART1_BASE + 0x00UL)  /* Status Register (TDR @ +0x04 sur F1/F4) */
#define USART_DR   (USART1_BASE + 0x04UL)  /* Data Register */
#define USART_BRR  (USART1_BASE + 0x08UL)  /* Baud Rate (diviseur fixe sur 16 bits) */
#define USART_CR1  (USART1_BASE + 0x0CUL)  /* Control Register 1 */

#define SR_TXE     (1u << 7)               /* Transmit Data Register Empty */
#define SR_TC      (1u << 6)               /* Transmission Complete */
#define SR_RXNE    (1u << 5)               /* Read Data Register Not Empty */

#define CR1_UE     (1u << 13)              /* USART Enable */
#define CR1_TE     (1u << 3)               /* Transmitter Enable */
#define CR1_RE     (1u << 2)               /* Receiver Enable */

/**
 * Horloge de l'USART — sur STM32F4 USART1 est sur APB2 (PCLK2, 84 MHz max).
 * Surcharger via -DUSART1_PCLK=... à la configuration de l'arbre d'horloge.
 */
#ifndef USART1_PCLK
#define USART1_PCLK 84000000UL
#endif

static inline void mmio_write(uint32_t addr, uint32_t val) {
    *(volatile uint32_t *)(uintptr_t)addr = val;
}

static inline uint32_t mmio_read(uint32_t addr) {
    return *(volatile uint32_t *)(uintptr_t)addr;
}

/** Attend SR.TXE puis écrit l'octet dans DR. */
static void uart_send_byte(uint8_t c) {
    while ((mmio_read(USART_SR) & SR_TXE) == 0) {
        /* spin-wait : TDR plein. */
    }
    mmio_write(USART_DR, (uint32_t)c);
}

static afros_status_t console_init_impl(uint32_t baud_rate) {
    if (baud_rate == 0) baud_rate = 115200;

    /* Désactiver l'USART pendant la configuration. */
    uint32_t cr1 = mmio_read(USART_CR1);
    cr1 &= ~CR1_UE;
    mmio_write(USART_CR1, cr1);

    /* Diviseur baud : BRR = PCLK / baud (entier). */
    uint32_t brr = (uint32_t)(USART1_PCLK / baud_rate);
    mmio_write(USART_BRR, brr);

    /* Activer TX + RX + USART. Format 8N1 par défaut (M=0, PCE=0). */
    cr1 |= CR1_TE | CR1_RE | CR1_UE;
    mmio_write(USART_CR1, cr1);

    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    /* Convertir \n en \r\n pour les terminaux série classiques. */
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
    if ((mmio_read(USART_SR) & SR_RXNE) == 0) {
        return AFROS_ERROR_TIMEOUT;  /* rien à lire */
    }
    *c = (char)(mmio_read(USART_DR) & 0xFFu);
    return AFROS_SUCCESS;
}

static bool console_has_input_impl(void) {
    return (mmio_read(USART_SR) & SR_RXNE) != 0;
}

console_ops_t arch_console_ops = {
    .init = console_init_impl,
    .putc = console_putc_impl,
    .puts = console_puts_impl,
    .getc = console_getc_impl,
    .has_input = console_has_input_impl
};
