#include "uart.h"
#include "imxrt.h"

// Startup configures: CCM_CSCDR1 |= UART_CLK_SEL (OSC, 24 MHz), PODF = 0.
#define UART1_CLOCK_HZ  24000000UL

void uart1_init(uint32_t baud)
{
    // Enable LPUART6 clock gate (CCM_CCGR3 bits [7:6])
    CCM_CCGR3 |= CCM_CCGR3_LPUART6(CCM_CCGR_ON);

    // Pin mux — ALT2 routes GPIO_AD_B0_02/03 to LPUART6_TX/RX
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_02 = 2;   // TX = Teensy pin 1
    IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_02 = IOMUXC_PAD_SRE | IOMUXC_PAD_DSE(3) | IOMUXC_PAD_SPEED(3);
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_03 = 2;   // RX = Teensy pin 0
    IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B0_03 = IOMUXC_PAD_DSE(7) | IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | IOMUXC_PAD_PUS(3) | IOMUXC_PAD_HYS;
    IOMUXC_LPUART6_RX_SELECT_INPUT = 1;         // daisy: GPIO_AD_B0_03 ALT2

    // Software reset — clears all registers to defaults
    LPUART6_GLOBAL = LPUART_GLOBAL_RST;
    LPUART6_GLOBAL = 0;

    // Baud rate: OSR=25 (26× oversampling), SBR = clock / (26 × baud)
    // Example: 24 MHz / (26 × 115200) = 8 → 115 384 bps (+0.16 %)
    uint32_t sbr = UART1_CLOCK_HZ / (26u * baud);
    LPUART6_BAUD = LPUART_BAUD_OSR(25) | LPUART_BAUD_SBR(sbr);

    // Enable transmitter and receiver
    LPUART6_CTRL = LPUART_CTRL_TE | LPUART_CTRL_RE;
}

void uart1_putc(char c)
{
    while (!(LPUART6_STAT & LPUART_STAT_TDRE));
    LPUART6_DATA = (uint8_t)c;
}

void uart1_puts(const char *s)
{
    while (*s)
        uart1_putc(*s++);
}

int uart1_getc(void)
{
    while (!(LPUART6_STAT & LPUART_STAT_RDRF));
    return (int)(LPUART6_DATA & 0xFF);
}

int uart1_rxready(void)
{
    return (LPUART6_STAT & LPUART_STAT_RDRF) ? 1 : 0;
}
