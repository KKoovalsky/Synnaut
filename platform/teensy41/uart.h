#pragma once

#include <stdint.h>

// Hardware UART driver for Teensy 4.1 — LPUART6, Serial1 (pin 0 RX / pin 1 TX).
//
// Clock source: 24 MHz OSC, configured by startup.c via CCM_CSCDR1_UART_CLK_SEL.
// Polling only — no interrupts, no DMA, no FIFO.

#ifdef __cplusplus
extern "C" {
#endif

// Initialise LPUART6 at the requested baud rate.
// Call once before any other uart1_* function.
void uart1_init(uint32_t baud);

// Block until the TX holding register is empty, then send one byte.
void uart1_putc(char c);

// Send a null-terminated string.
void uart1_puts(const char *s);

// Block until a byte is received and return it.
int uart1_getc(void);

// Non-blocking RX check.  Returns 1 if a byte is waiting, 0 otherwise.
int uart1_rxready(void);

#ifdef __cplusplus
}
#endif
