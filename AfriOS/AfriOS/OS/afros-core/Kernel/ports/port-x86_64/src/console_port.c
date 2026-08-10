#include "console_abstraction.h"

/**
 * @file console_port.c
 * @brief x86_64 Port: VGA Text Mode + UART 16550 (COM1, I/O port 0x3F8)
 */

#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define UART_COM1_PORT 0x3F8
#define UART_THR 0
#define UART_RBR 0
#define UART_IER 1
#define UART_FCR 2
#define UART_LCR 3
#define UART_MCR 4
#define UART_LSR 5

static size_t vga_row = 0;
static size_t vga_col = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
}

static void uart_send_byte(uint8_t byte) {
    while ((inb(UART_COM1_PORT + UART_LSR) & 0x20) == 0);
    outb(UART_COM1_PORT + UART_THR, byte);
}

static void vga_putchar(char c) {
    uint16_t *vga = (uint16_t *)VGA_MEMORY;
    uint16_t color = (0x07 << 8) | 0x07; 
    
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
    } else {
        vga[vga_row * VGA_WIDTH + vga_col] = color | (unsigned char)c;
        vga_col++;
    }
    
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
    }
    
    if (vga_row >= VGA_HEIGHT) {
        for (size_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga[i] = vga[i + VGA_WIDTH];
        }
        for (size_t i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga[i] = color | ' ';
        }
        vga_row = VGA_HEIGHT - 1;
    }
}

static afros_status_t console_init_impl(uint32_t baud_rate) {
    uint16_t divisor = 115200 / baud_rate;
    
    outb(UART_COM1_PORT + UART_LCR, 0x80);
    outb(UART_COM1_PORT + 0, divisor & 0xFF);
    outb(UART_COM1_PORT + 1, (divisor >> 8) & 0xFF);
    outb(UART_COM1_PORT + UART_LCR, 0x03);
    outb(UART_COM1_PORT + UART_FCR, 0xC7);
    outb(UART_COM1_PORT + UART_MCR, 0x0B);
    outb(UART_COM1_PORT + UART_IER, 0x00);
    
    vga_row = 0;
    vga_col = 0;
    
    return AFROS_SUCCESS;
}

static afros_status_t console_putc_impl(char c) {
    vga_putchar(c);
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
    if ((inb(UART_COM1_PORT + UART_LSR) & 0x01) == 0) {
        return AFROS_ERROR_TIMEOUT;
    }
    *c = (char)inb(UART_COM1_PORT + UART_RBR);
    return AFROS_SUCCESS;
}

static bool console_has_input_impl(void) {
    return (inb(UART_COM1_PORT + UART_LSR) & 0x01) != 0;
}

console_ops_t arch_console_ops = {
    .init = console_init_impl,
    .putc = console_putc_impl,
    .puts = console_puts_impl,
    .getc = console_getc_impl,
    .has_input = console_has_input_impl
};
