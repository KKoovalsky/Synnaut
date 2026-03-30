// Teensy 4.1 – blink built-in LED (pin 13 = GPIO_B0_03 / GPIO7 bit 3)
//
// startup.c already sets IOMUXC_GPR_GPR27 = 0xFFFFFFFF, routing all
// GPIO2 pads through the fast-GPIO7 alias.  We use GPIO7_DR_TOGGLE for
// the actual toggle and enable the GPIO2 clock gate before touching any
// GPIO2/7 register.

#include "imxrt.h"

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
    // Enable GPIO2 clock gate (bits [31:30])
    CCM_CCGR0 |= CCM_CCGR0_GPIO2(CCM_CCGR_ON);

    // Mux pad GPIO_B0_03 to ALT5 = GPIO2_IO03
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 5;

    // Set pin as output (via fast-GPIO7 mirror)
    GPIO7_GDIR |= LED_BIT;

    while (true) {
        GPIO7_DR_TOGGLE = LED_BIT;
        delay_ms(500);
    }
}
