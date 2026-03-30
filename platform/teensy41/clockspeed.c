/* NautSyn — Teensy 4.1 (IMXRT1062) ARM clock configuration
 *
 * Derived from the PJRC Teensyduino core (MIT License).
 * https://github.com/PaulStoffregen/cores
 *
 * Stripped of: printf debug output, wiring.h dependency
 *              (scale_cpu_cycles_to_microseconds removed).
 */

#include "imxrt.h"
#include <stdint.h>

// Actual CPU and bus frequencies after set_arm_clock().
// Peripheral drivers (LPUART, SAI, etc.) can read these to compute dividers.
volatile uint32_t F_CPU_ACTUAL = 396000000;
volatile uint32_t F_BUS_ACTUAL = 132000000;

#define OVERCLOCK_STEPSIZE  28000000
#define OVERCLOCK_MAX_VOLT  1575

uint32_t set_arm_clock(uint32_t frequency)
{
    uint32_t cbcdr = CCM_CBCDR;
    uint32_t cbcmr = CCM_CBCMR;
    uint32_t dcdc  = DCDC_REG3;

    // Compute required core voltage
    uint32_t voltage = 1150; // default 1.15 V
    if (frequency > 528000000) {
        voltage = 1250; // 1.25 V
#if defined(OVERCLOCK_STEPSIZE) && defined(OVERCLOCK_MAX_VOLT)
        if (frequency > 600000000) {
            voltage += ((frequency - 600000000) / OVERCLOCK_STEPSIZE) * 25;
            if (voltage > OVERCLOCK_MAX_VOLT) voltage = OVERCLOCK_MAX_VOLT;
        }
#endif
    } else if (frequency <= 24000000) {
        voltage = 950; // 0.95 V
    }

    // Raise voltage before increasing frequency
    CCM_CCGR6 |= CCM_CCGR6_DCDC(CCM_CCGR_ON);
    if ((dcdc & DCDC_REG3_TRG_MASK) < DCDC_REG3_TRG((voltage - 800) / 25)) {
        dcdc &= ~DCDC_REG3_TRG_MASK;
        dcdc |= DCDC_REG3_TRG((voltage - 800) / 25);
        DCDC_REG3 = dcdc;
        while (!(DCDC_REG0 & DCDC_REG0_STS_DC_OK)) ; // wait for voltage to settle
    }

    // Switch the core to an alternate clock source so it is safe to
    // reprogram the ARM PLL (PLL1).
    if (!(cbcdr & CCM_CBCDR_PERIPH_CLK_SEL)) {
        const uint32_t need1s = CCM_ANALOG_PLL_USB1_ENABLE |
                                CCM_ANALOG_PLL_USB1_POWER  |
                                CCM_ANALOG_PLL_USB1_LOCK   |
                                CCM_ANALOG_PLL_USB1_EN_USB_CLKS;
        uint32_t sel, div;
        if ((CCM_ANALOG_PLL_USB1 & need1s) == need1s) {
            sel = 0; div = 3; // PLL3/6 = 120 MHz — IPG stays in spec
        } else {
            sel = 1; div = 0; // 24 MHz crystal
        }
        if ((cbcdr & CCM_CBCDR_PERIPH_CLK2_PODF_MASK) !=
                CCM_CBCDR_PERIPH_CLK2_PODF(div)) {
            cbcdr &= ~CCM_CBCDR_PERIPH_CLK2_PODF_MASK;
            cbcdr |= CCM_CBCDR_PERIPH_CLK2_PODF(div);
            CCM_CBCDR = cbcdr;
        }
        if ((cbcmr & CCM_CBCMR_PERIPH_CLK2_SEL_MASK) !=
                CCM_CBCMR_PERIPH_CLK2_SEL(sel)) {
            cbcmr &= ~CCM_CBCMR_PERIPH_CLK2_SEL_MASK;
            cbcmr |= CCM_CBCMR_PERIPH_CLK2_SEL(sel);
            CCM_CBCMR = cbcmr;
            while (CCM_CDHIPR & CCM_CDHIPR_PERIPH2_CLK_SEL_BUSY) ;
        }
        cbcdr |= CCM_CBCDR_PERIPH_CLK_SEL;
        CCM_CBCDR = cbcdr;
        while (CCM_CDHIPR & CCM_CDHIPR_PERIPH_CLK_SEL_BUSY) ;
    }

    // Compute ARM PLL multiplier and AHB/ARM dividers.
    // PLL1 VCO range: 648–1296 MHz (DIV_SELECT 54–108, step = 12 MHz).
    uint32_t div_arm = 1;
    uint32_t div_ahb = 1;
    while (frequency * div_arm * div_ahb < 648000000) {
        if (div_arm < 8) {
            div_arm++;
        } else if (div_ahb < 5) {
            div_ahb++;
            div_arm = 1;
        } else {
            break;
        }
    }
    uint32_t mult = (frequency * div_arm * div_ahb + 6000000) / 12000000;
    if (mult > 108) mult = 108;
    if (mult < 54)  mult = 54;
    frequency = mult * 12000000 / div_arm / div_ahb;

    // Reprogram ARM PLL if needed
    const uint32_t arm_pll_mask = CCM_ANALOG_PLL_ARM_LOCK        |
                                  CCM_ANALOG_PLL_ARM_BYPASS       |
                                  CCM_ANALOG_PLL_ARM_ENABLE       |
                                  CCM_ANALOG_PLL_ARM_POWERDOWN    |
                                  CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK;
    if ((CCM_ANALOG_PLL_ARM & arm_pll_mask) !=
            (CCM_ANALOG_PLL_ARM_LOCK | CCM_ANALOG_PLL_ARM_ENABLE |
             CCM_ANALOG_PLL_ARM_DIV_SELECT(mult))) {
        CCM_ANALOG_PLL_ARM = CCM_ANALOG_PLL_ARM_POWERDOWN;
        CCM_ANALOG_PLL_ARM = CCM_ANALOG_PLL_ARM_ENABLE |
                             CCM_ANALOG_PLL_ARM_DIV_SELECT(mult);
        while (!(CCM_ANALOG_PLL_ARM & CCM_ANALOG_PLL_ARM_LOCK)) ;
    }

    if ((CCM_CACRR & CCM_CACRR_ARM_PODF_MASK) != (div_arm - 1)) {
        CCM_CACRR = CCM_CACRR_ARM_PODF(div_arm - 1);
        while (CCM_CDHIPR & CCM_CDHIPR_ARM_PODF_BUSY) ;
    }

    if ((cbcdr & CCM_CBCDR_AHB_PODF_MASK) != CCM_CBCDR_AHB_PODF(div_ahb - 1)) {
        cbcdr &= ~CCM_CBCDR_AHB_PODF_MASK;
        cbcdr |= CCM_CBCDR_AHB_PODF(div_ahb - 1);
        CCM_CBCDR = cbcdr;
        while (CCM_CDHIPR & CCM_CDHIPR_AHB_PODF_BUSY) ;
    }

    uint32_t div_ipg = (frequency + 149999999) / 150000000;
    if (div_ipg > 4) div_ipg = 4;
    if ((cbcdr & CCM_CBCDR_IPG_PODF_MASK) != CCM_CBCDR_IPG_PODF(div_ipg - 1)) {
        cbcdr &= ~CCM_CBCDR_IPG_PODF_MASK;
        cbcdr |= CCM_CBCDR_IPG_PODF(div_ipg - 1);
        CCM_CBCDR = cbcdr;
    }

    // Switch back to ARM PLL
    CCM_CBCDR &= ~CCM_CBCDR_PERIPH_CLK_SEL;
    while (CCM_CDHIPR & CCM_CDHIPR_PERIPH_CLK_SEL_BUSY) ;

    F_CPU_ACTUAL = frequency;
    F_BUS_ACTUAL = frequency / div_ipg;

    // Lower voltage after reducing frequency
    if ((dcdc & DCDC_REG3_TRG_MASK) > DCDC_REG3_TRG((voltage - 800) / 25)) {
        dcdc &= ~DCDC_REG3_TRG_MASK;
        dcdc |= DCDC_REG3_TRG((voltage - 800) / 25);
        DCDC_REG3 = dcdc;
        while (!(DCDC_REG0 & DCDC_REG0_STS_DC_OK)) ;
    }

    return frequency;
}
