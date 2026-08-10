#include "console_abstraction.h"

/**
 * @file console_port.c
 * @brief Port RISC-V : console via SBI (Supervisor Binary Interface).
 *
 * Implémentation freestanding : appelle directement l'EID legacy SBI
 * `sbi_console_putchar` (a7=1) / `sbi_console_getchar` (a7=2) via `ecall`.
 * Convient à tout firmwire OpenSBI / RustSBI exposant l'extension console
 * legacy (cas par défaut sur QEMU virt RISC-V). Le repli 16550 mentionné dans
 * l'ancienne version simulée n'est pas câblé ici — si le firmware SBI n'expose
 * pas le service, aucun caractère n'est émis (et getc renvoie TIMEOUT).
 */

/**
 * @brief SBI legacy console putchar (EID 0x01).
 * @returns 0 sur succès, code d'erreur négatif sinon.
 */
static inline long sbi_console_putchar(unsigned char c) {
#if defined(__riscv) && (__riscv_xlen == 32 || __riscv_xlen == 64)
    register unsigned long a0 asm("a0") = (unsigned long)c;
    register unsigned long a7 asm("a7") = 0x01UL; /* sbi_console_putchar */
    asm volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return (long)a0;
#else
    /* Compilation croisée absente : stub pour permettre la syntaxe-check sur
     * une autre architecture. À l'exécution le caractère est perdu. */
    (void)c;
    return 0;
#endif
}

/**
 * @brief SBI legacy console getchar (EID 0x02).
 * @returns caractère lu [0..255], ou -1 si aucun caractère disponible.
 */
static inline long sbi_console_getchar(void) {
#if defined(__riscv) && (__riscv_xlen == 32 || __riscv_xlen == 64)
    register unsigned long a0 asm("a0");
    register unsigned long a7 asm("a7") = 0x02UL; /* sbi_console_getchar */
    asm volatile ("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return (long)a0;
#else
    return -1; /* stub : pas d'entrée disponible. */
#endif
}

static afros_status_t console_init_impl(uint32_t baud_rate) {
    /* SBI gère la console lui-même : aucun baud rate à programmer côté noyau.
     * Le paramètre est conservé pour respecter le contrat console_ops_t.init. */
    (void)baud_rate;
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    /* Convertir \n en \r\n pour les terminaux série classiques. */
    if (c == '\n') {
        sbi_console_putchar('\r');
    }
    sbi_console_putchar((unsigned char)c);
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
    long ch = sbi_console_getchar();
    if (ch < 0) {
        return AFROS_ERROR_TIMEOUT;  /* pas de caractère disponible */
    }
    *c = (char)ch;
    return AFROS_SUCCESS;
}

static bool console_has_input_impl(void) {
    /* SBI legacy getchar est destructeur (pas de peek) : on ne peut pas
     * répondre exactement sans consommer le caractère. On renvoie true pour
     * laisser getc() être appelé et décider ; le coût est un ecall supplémentaire
     * uniquement dans le chemin polling-input. */
    return true;
}

console_ops_t arch_console_ops = {
    .init = console_init_impl,
    .putc = console_putc_impl,
    .puts = console_puts_impl,
    .getc = console_getc_impl,
    .has_input = console_has_input_impl
};
