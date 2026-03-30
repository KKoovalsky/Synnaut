#pragma once

#include <stdint.h>

// Polling LPUART driver for Teensy 4.1.
//
// Clock source: 24 MHz OSC, set by startup.c via CCM_CSCDR1_UART_CLK_SEL.
// Baud rate: OSR=25 (26× oversampling), SBR = 24 000 000 / (26 × baud).
// No interrupts, no DMA, no FIFO.
//
// Usage:
//   uart_serial_init(&uart_serial1, 921600);
//   uart_serial_puts(&uart_serial1, "hello\r\n");

#ifdef __cplusplus
extern "C" {
#endif

// Opaque instance type — use the pre-defined instances below.
typedef struct uart_serial_s uart_serial_t;

// Pre-defined instances (Teensy 4.1 default pin assignments):
//   uart_serial1 — LPUART6, TX pin  1, RX pin  0
//   uart_serial2 — LPUART4, TX pin  8, RX pin  7
//   uart_serial3 — LPUART2, TX pin 14, RX pin 15
//   uart_serial4 — LPUART3, TX pin 17, RX pin 16
//   uart_serial5 — LPUART8, TX pin 20, RX pin 21
//   uart_serial6 — LPUART1, TX pin 24, RX pin 25
//   uart_serial7 — LPUART7, TX pin 29, RX pin 28
//   uart_serial8 — LPUART5, TX pin 35, RX pin 34  (Teensy 4.1 only)
extern const uart_serial_t uart_serial1;
extern const uart_serial_t uart_serial2;
extern const uart_serial_t uart_serial3;
extern const uart_serial_t uart_serial4;
extern const uart_serial_t uart_serial5;
extern const uart_serial_t uart_serial6;
extern const uart_serial_t uart_serial7;
extern const uart_serial_t uart_serial8;

// Initialise the UART instance at the requested baud rate.
// Call once before any other uart_serial_* function on this instance.
void uart_serial_init(const uart_serial_t *s, uint32_t baud);

// Block until TX is ready, then send one byte.
void uart_serial_putc(const uart_serial_t *s, char c);

// Send a null-terminated string.
void uart_serial_puts(const uart_serial_t *s, const char *str);

// Block until a byte is received and return it.
int uart_serial_getc(const uart_serial_t *s);

// Non-blocking RX check. Returns 1 if a byte is waiting, 0 otherwise.
int uart_serial_rxready(const uart_serial_t *s);

#ifdef __cplusplus
}
#endif
