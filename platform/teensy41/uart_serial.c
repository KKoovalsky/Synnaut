#include "uart_serial.h"
#include "imxrt.h"
#include <stddef.h>

#define UART_CLOCK_HZ  24000000UL   // OSC, configured by startup.c

// Pad settings reused for all TX and RX pads.
#define TX_PAD_VAL  (IOMUXC_PAD_SRE | IOMUXC_PAD_DSE(3) | IOMUXC_PAD_SPEED(3))
#define RX_PAD_VAL  (IOMUXC_PAD_DSE(7) | IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | \
                     IOMUXC_PAD_PUS(3) | IOMUXC_PAD_HYS)

struct uart_serial_s {
    volatile IMXRT_LPUART_t *regs;
    volatile uint32_t       *ccgr;
    uint32_t                 ccgr_mask;
    volatile uint32_t       *tx_mux;
    uint32_t                 tx_mux_val;
    volatile uint32_t       *tx_pad;
    volatile uint32_t       *rx_mux;
    uint32_t                 rx_mux_val;
    volatile uint32_t       *rx_pad;
    volatile uint32_t       *rx_sel;    // NULL = no daisy chain select
    uint32_t                 rx_sel_val;
    volatile uint32_t       *tx_sel;    // NULL = no daisy chain select
    uint32_t                 tx_sel_val;
};

// ── Instance definitions ──────────────────────────────────────────────────────

// Serial1 — LPUART6, TX pin 1 (GPIO_AD_B0_02), RX pin 0 (GPIO_AD_B0_03)
const uart_serial_t uart_serial1 = {
    .regs       = &IMXRT_LPUART6,
    .ccgr       = &CCM_CCGR3,
    .ccgr_mask  = CCM_CCGR3_LPUART6(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_02, .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_02,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_03, .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_03,
    .rx_sel     = &IOMUXC_LPUART6_RX_SELECT_INPUT,      .rx_sel_val = 1,
    .tx_sel     = &IOMUXC_LPUART6_TX_SELECT_INPUT,      .tx_sel_val = 1,
};

// Serial2 — LPUART4, TX pin 8 (GPIO_B1_00), RX pin 7 (GPIO_B1_01)
const uart_serial_t uart_serial2 = {
    .regs       = &IMXRT_LPUART4,
    .ccgr       = &CCM_CCGR1,
    .ccgr_mask  = CCM_CCGR1_LPUART4(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_00,   .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_00,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01,   .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_01,
    .rx_sel     = &IOMUXC_LPUART4_RX_SELECT_INPUT,      .rx_sel_val = 2,
    .tx_sel     = &IOMUXC_LPUART4_TX_SELECT_INPUT,      .tx_sel_val = 2,
};

// Serial3 — LPUART2, TX pin 14 (GPIO_AD_B1_02), RX pin 15 (GPIO_AD_B1_03)
const uart_serial_t uart_serial3 = {
    .regs       = &IMXRT_LPUART2,
    .ccgr       = &CCM_CCGR0,
    .ccgr_mask  = CCM_CCGR0_LPUART2(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_02, .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_02,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_03, .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_03,
    .rx_sel     = &IOMUXC_LPUART2_RX_SELECT_INPUT,       .rx_sel_val = 1,
    .tx_sel     = &IOMUXC_LPUART2_TX_SELECT_INPUT,       .tx_sel_val = 1,
};

// Serial4 — LPUART3, TX pin 17 (GPIO_AD_B1_06), RX pin 16 (GPIO_AD_B1_07)
const uart_serial_t uart_serial4 = {
    .regs       = &IMXRT_LPUART3,
    .ccgr       = &CCM_CCGR0,
    .ccgr_mask  = CCM_CCGR0_LPUART3(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_06, .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_06,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_07, .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_07,
    .rx_sel     = &IOMUXC_LPUART3_RX_SELECT_INPUT,       .rx_sel_val = 0,
    .tx_sel     = &IOMUXC_LPUART3_TX_SELECT_INPUT,       .tx_sel_val = 0,
};

// Serial5 — LPUART8, TX pin 20 (GPIO_AD_B1_10), RX pin 21 (GPIO_AD_B1_11)
const uart_serial_t uart_serial5 = {
    .regs       = &IMXRT_LPUART8,
    .ccgr       = &CCM_CCGR6,
    .ccgr_mask  = CCM_CCGR6_LPUART8(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_10, .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_10,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_11, .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_11,
    .rx_sel     = &IOMUXC_LPUART8_RX_SELECT_INPUT,       .rx_sel_val = 1,
    .tx_sel     = &IOMUXC_LPUART8_TX_SELECT_INPUT,       .tx_sel_val = 1,
};

// Serial6 — LPUART1, TX pin 24 (GPIO_AD_B0_12), RX pin 25 (GPIO_AD_B0_13)
// No daisy-chain select: these are the only pads routed to LPUART1.
const uart_serial_t uart_serial6 = {
    .regs       = &IMXRT_LPUART1,
    .ccgr       = &CCM_CCGR5,
    .ccgr_mask  = CCM_CCGR5_LPUART1(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12, .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_12,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13, .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_13,
    .rx_sel     = NULL,                                   .rx_sel_val = 0,
    .tx_sel     = NULL,                                   .tx_sel_val = 0,
};

// Serial7 — LPUART7, TX pin 29 (GPIO_EMC_31), RX pin 28 (GPIO_EMC_32)
const uart_serial_t uart_serial7 = {
    .regs       = &IMXRT_LPUART7,
    .ccgr       = &CCM_CCGR5,
    .ccgr_mask  = CCM_CCGR5_LPUART7(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_31,   .tx_mux_val = 2,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_31,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_32,   .rx_mux_val = 2,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_EMC_32,
    .rx_sel     = &IOMUXC_LPUART7_RX_SELECT_INPUT,       .rx_sel_val = 1,
    .tx_sel     = &IOMUXC_LPUART7_TX_SELECT_INPUT,       .tx_sel_val = 1,
};

// Serial8 — LPUART5, TX pin 35 (GPIO_B1_12), RX pin 34 (GPIO_B1_13)  [Teensy 4.1 only]
const uart_serial_t uart_serial8 = {
    .regs       = &IMXRT_LPUART5,
    .ccgr       = &CCM_CCGR3,
    .ccgr_mask  = CCM_CCGR3_LPUART5(CCM_CCGR_ON),
    .tx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_12,    .tx_mux_val = 1,
    .tx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_12,
    .rx_mux     = &IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_13,    .rx_mux_val = 1,
    .rx_pad     = &IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_13,
    .rx_sel     = &IOMUXC_LPUART5_RX_SELECT_INPUT,       .rx_sel_val = 1,
    .tx_sel     = &IOMUXC_LPUART5_TX_SELECT_INPUT,       .tx_sel_val = 1,
};

// ── Driver implementation ─────────────────────────────────────────────────────

void uart_serial_init(const uart_serial_t *s, uint32_t baud)
{
    // Clock gate
    *s->ccgr |= s->ccgr_mask;

    // Pin mux
    *s->tx_mux = s->tx_mux_val;
    *s->tx_pad = TX_PAD_VAL;
    *s->rx_mux = s->rx_mux_val;
    *s->rx_pad = RX_PAD_VAL;
    if (s->rx_sel) *s->rx_sel = s->rx_sel_val;
    if (s->tx_sel) *s->tx_sel = s->tx_sel_val;

    // Software reset — clears all registers to defaults
    s->regs->GLOBAL = LPUART_GLOBAL_RST;
    s->regs->GLOBAL = 0;

    // Baud rate: OSR=25 (26× oversampling), SBR = clock / (26 × baud)
    // Example: 24 MHz / (26 × 921600) ≈ 1 → 923 077 bps (+0.16 %)
    uint32_t sbr = UART_CLOCK_HZ / (26u * baud);
    s->regs->BAUD = LPUART_BAUD_OSR(25) | LPUART_BAUD_SBR(sbr);

    // Enable transmitter and receiver
    s->regs->CTRL = LPUART_CTRL_TE | LPUART_CTRL_RE;
}

void uart_serial_putc(const uart_serial_t *s, char c)
{
    while (!(s->regs->STAT & LPUART_STAT_TDRE));
    s->regs->DATA = (uint8_t)c;
}

void uart_serial_puts(const uart_serial_t *s, const char *str)
{
    while (*str)
        uart_serial_putc(s, *str++);
}

int uart_serial_getc(const uart_serial_t *s)
{
    while (!(s->regs->STAT & LPUART_STAT_RDRF));
    return (int)(s->regs->DATA & 0xFF);
}

int uart_serial_rxready(const uart_serial_t *s)
{
    uint32_t stat = s->regs->STAT;
    // OR (overrun), NF (noise), FE (framing), PF (parity) are W1C.
    // OR in particular halts reception until cleared — if left unhandled the
    // UART stops receiving entirely.  Discard the corrupted byte and clear all
    // error flags so the receiver re-enables for the next incoming byte.
    // TODO: propagate error flags to the caller instead of silently discarding
    //       the byte (e.g. return a negative error code or expose a separate
    //       uart_serial_errors() query so the application can log or count them).
    if (stat & (LPUART_STAT_OR | LPUART_STAT_NF | LPUART_STAT_FE | LPUART_STAT_PF)) {
        s->regs->STAT = stat;      // W1C: write 1s back to clear the set flags
        (void)s->regs->DATA;       // drain the error byte, clears RDRF
        return 0;
    }
    return (stat & LPUART_STAT_RDRF) ? 1 : 0;
}
