// Teensy 4.1 – blink LED and echo received bytes over UART1 (pins 0/1, 115200 8N1)

#include "imxrt.h"
#include "uart.h"

// LED: Teensy pin 13 → pad GPIO_B0_03 → GPIO2_IO03 / GPIO7_IO03
static constexpr uint32_t LED_BIT = 1u << 3;

static void delay_ms(uint32_t ms)
{
    // F_CPU cycles per ms, two instructions per iteration ≈ 1 cycle each at -O2
    volatile uint32_t ticks = (F_CPU / 1000u / 2u) * ms;
    while (ticks) { ticks -= 1; }
}

extern "C" int main()
{
    // LED setup (GPIO_B0_03 / GPIO7 fast mirror)
    CCM_CCGR0 |= CCM_CCGR0_GPIO2(CCM_CCGR_ON);
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 5;
    GPIO7_GDIR |= LED_BIT;

    // UART1 setup
    uart1_init(921600);
    uart1_puts("NautSyn booted\r\n");

    while (true) {
        GPIO7_DR_TOGGLE = LED_BIT;
        delay_ms(500);

        if (uart1_rxready())
            uart1_putc((char)uart1_getc());
    }
}
