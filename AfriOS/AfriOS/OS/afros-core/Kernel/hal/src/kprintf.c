/*
 * kprintf.c — freestanding formatted output for the AfriOS kernel.
 *
 * Two operating modes:
 *   - AFROS_FREESTANDING defined : walk the format string by hand and emit
 *     each character through the active port's arch_console_ops.putc. No libc
 *     call (no libc formatted-output helper), no heap.
 *   - AFROS_FREESTANDING undefined (host simulator build) : delegate to the
 *     libc varargs formatter so the existing afros-kernel-sim still prints to
 *     stdout without real UART/VGA hardware.
 *
 * Conversions supported: %s %d %i %u %x %X %c %% and the `l` length modifier.
 */

#include "kprintf.h"
#include "console_abstraction.h"
#include <stdarg.h>

#ifndef AFROS_FREESTANDING
/* Host simulator: rely on the C library so the test/simulator executable can
 * still produce visible output. */
#include <stdio.h>

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

#else /* AFROS_FREESTANDING — real kernel build, no libc. */
/** Emit a NUL-terminated string through the active console port. */
static void kprint_str(const char *s) {
    if (!s) s = "(null)";
    if (arch_console_ops.puts) {
        arch_console_ops.puts(s);
        return;
    }
    if (arch_console_ops.putc) {
        while (*s) arch_console_ops.putc(*s++);
    }
}

/** Emit an unsigned integer in the requested base. */
static void kprint_uint(unsigned long val, unsigned base, int upper) {
    char buf[32];
    int i = (int)sizeof(buf);
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    buf[--i] = '\0';
    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0 && i > 0) {
            buf[--i] = digits[val % base];
            val /= base;
        }
    }
    kprint_str(&buf[i]);
}

/** Emit a signed integer (handles negative values explicitly). */
static void kprint_int(long val) {
    unsigned long u;
    if (val < 0) {
        if (arch_console_ops.putc) arch_console_ops.putc('-');
        u = (unsigned long)(-(val + 1)) + 1UL; /* avoid overflow on LONG_MIN */
    } else {
        u = (unsigned long)val;
    }
    kprint_uint(u, 10, 0);
}

int kprintf(const char *fmt, ...) {
    if (!fmt) return 0;
    va_list ap;
    va_start(ap, fmt);
    int count = 0;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            if (arch_console_ops.putc) arch_console_ops.putc(*p);
            count++;
            continue;
        }

        p++; /* skip '%' */
        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }

        switch (*p) {
            case 's':
                kprint_str(va_arg(ap, const char *));
                break;
            case 'd':
            case 'i':
                if (is_long) kprint_int(va_arg(ap, long));
                else         kprint_int((long)va_arg(ap, int));
                break;
            case 'u':
                if (is_long) kprint_uint(va_arg(ap, unsigned long), 10, 0);
                else         kprint_uint((unsigned long)va_arg(ap, unsigned int), 10, 0);
                break;
            case 'x':
                if (is_long) kprint_uint(va_arg(ap, unsigned long), 16, 0);
                else         kprint_uint((unsigned long)va_arg(ap, unsigned int), 16, 0);
                break;
            case 'X':
                if (is_long) kprint_uint(va_arg(ap, unsigned long), 16, 1);
                else         kprint_uint((unsigned long)va_arg(ap, unsigned int), 16, 1);
                break;
            case 'c':
                if (arch_console_ops.putc)
                    arch_console_ops.putc((char)va_arg(ap, int));
                break;
            case '%':
                if (arch_console_ops.putc) arch_console_ops.putc('%');
                break;
            case '\0':
                p--; /* end of fmt — back up so the for-loop's ++ doesn't skip */
                break;
            default:
                /* unknown specifier — emit verbatim so bugs are visible */
                if (arch_console_ops.putc) arch_console_ops.putc('%');
                if (arch_console_ops.putc) arch_console_ops.putc(*p);
                break;
        }
        count++;
    }

    va_end(ap);
    return count;
}

#endif /* AFROS_FREESTANDING */
