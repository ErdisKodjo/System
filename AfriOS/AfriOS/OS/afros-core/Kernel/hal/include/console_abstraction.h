#ifndef CONSOLE_ABSTRACTION_H
#define CONSOLE_ABSTRACTION_H

#include "afros_types.h"

/**
 * @file console_abstraction.h
 * @brief Early boot console / UART abstraction for AfriOS (PL011 / 16550 / SBI console /
 *        board-specific USART). Used before any higher-level driver (drivers/) is
 *        registered, hence kept separate from device_abstraction.h.
 */

typedef struct {
    afros_status_t (*init)(uint32_t baud_rate);
    afros_status_t (*putc)(char c);
    afros_status_t (*puts)(const char *s);
    afros_status_t (*getc)(char *c);       // non-blocking; AFROS_ERROR_TIMEOUT if no data
    bool           (*has_input)(void);
} console_ops_t;

extern console_ops_t arch_console_ops;

#endif // CONSOLE_ABSTRACTION_H
