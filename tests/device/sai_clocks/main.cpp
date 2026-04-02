// SAI clock verification test
//
// Cycles through all four supported sample rates — 44100, 48000, 176400, 192000 Hz —
// running each for ~5 seconds.  Before each transition the expected frequencies for
// every SAI signal are printed over UART so you know exactly what to look for on a
// scope or logic analyser.
//
// Pin assignment (SAI1 master mode):
//   pin 23  MCLK   — master clock output to PCM5122 SCKI
//   pin 21  BCLK   — bit clock (= 64 × Fs = 32 bits × 2 channels × Fs)
//   pin 20  LRCLK  — left/right frame sync (= Fs)
//   pin  7  DATA   — I2S TX data, carries a sine tone at Fs/48
//
// Connect your scope probes to these pins with GND on Teensy GND (pins 0/1 row).
// UART output is on pin 1 (TX) at 921600 baud.
//
// NOTE: pin 7 is also uart_serial2 RX — do not connect uart_serial2 during this test.
//       pin 20/21 are uart_serial5 TX/RX — do not connect uart_serial5 either.
//       Use uart_serial1 (pins 0/1) for the Serial output shown here.

#include "imxrt.h"
#include "uart_serial.h"
#include "sai.h"
#include <FreeRTOS.h>
#include <task.h>

static constexpr uint32_t LED_BIT = 1u << 3;

// ── 48-sample sine table (32-bit signed, full-scale) ─────────────────────────
//
// Generated as: round(sin(2π × i/48) × 2147483647)  for i = 0..47
//
// Tone frequency = sample_rate / 48:
//   44100 Hz  →  918.75 Hz  ≈  919 Hz
//   48000 Hz  → 1000.00 Hz  = 1000 Hz  (exactly)
//  176400 Hz  → 3675.00 Hz  = 3675 Hz  (exactly)
//  192000 Hz  → 4000.00 Hz  = 4000 Hz  (exactly)
//
// Same mono sine is sent to both left and right channels.
static const int32_t k_sine48[48] = {
           0,  280067329,  555570233,  820130814,
  1073741823, 1307612352, 1518500249, 1703710534,
  1859775393, 1983705984, 2073741399, 2128521022,
  2147483647, 2128521022, 2073741399, 1983705984,
  1859775393, 1703710534, 1518500249, 1307612352,
  1073741823,  820130814,  555570233,  280067329,
           0, -280067329, -555570233, -820130814,
 -1073741823,-1307612352,-1518500249,-1703710534,
 -1859775393,-1983705984,-2073741399,-2128521022,
 -2147483648,-2128521022,-2073741399,-1983705984,
 -1859775393,-1703710534,-1518500249,-1307612352,
 -1073741823, -820130814, -555570233, -280067329,
};

// ── UART helper ───────────────────────────────────────────────────────────────

static void print(const char *s)
{
    uart_serial_puts(&uart_serial1, s);
}

// ── Per-rate scope guide ──────────────────────────────────────────────────────

struct rate_info {
    sai_config_t cfg;
    const char  *banner;   // printed as a section header
    const char  *mclk;     // expected MCLK frequency string
    const char  *bclk;     // expected BCLK frequency string
    const char  *lrclk;    // expected LRCLK (= Fs) frequency string
    const char  *tone;     // expected DATA tone frequency string
};

static const rate_info k_rates[] = {
    {
        { 44100, SAI_MODE_MASTER },
        "44100 Hz",
        "22.579 MHz  (= 512 x 44100)",
        " 2.822 MHz  (= 64 x 44100)",
        "44.100 kHz",
        "~919 Hz  (= 44100 / 48)",
    },
    {
        { 48000, SAI_MODE_MASTER },
        "48000 Hz",
        "24.576 MHz  (= 512 x 48000)",
        " 3.072 MHz  (= 64 x 48000)",
        "48.000 kHz",
        "1000 Hz  (= 48000 / 48, exact)",
    },
    {
        { 176400, SAI_MODE_MASTER },
        "176400 Hz",
        "45.158 MHz  (= 256 x 176400)",
        "11.290 MHz  (= 64 x 176400)",
        "176.400 kHz",
        "3675 Hz  (= 176400 / 48, exact)",
    },
    {
        { 192000, SAI_MODE_MASTER },
        "192000 Hz",
        "49.152 MHz  (= 256 x 192000)",
        "12.288 MHz  (= 64 x 192000)",
        "192.000 kHz",
        "4000 Hz  (= 192000 / 48, exact)",
    },
};

static void print_scope_guide(const rate_info &r)
{
    print("\r\n");
    print("════════════════════════════════════════\r\n");
    print("  Sample rate: ");
    print(r.banner);
    print("\r\n");
    print("════════════════════════════════════════\r\n");
    print("  Probe   Pin   Expected\r\n");
    print("  ------  ----  --------------------------------\r\n");
    print("  MCLK    23    "); print(r.mclk);  print("\r\n");
    print("  BCLK    21    "); print(r.bclk);  print("\r\n");
    print("  LRCLK   20    "); print(r.lrclk); print("\r\n");
    print("  DATA     7    sine tone at "); print(r.tone); print("\r\n");
    print("\r\n");
    print("  LRCLK duty cycle: 50 %\r\n");
    print("  BCLK edges per LRCLK half-period: 32\r\n");
    print("  DATA: I2S framed, MSB first, 32-bit words\r\n");
    print("        LRCLK low = left word, LRCLK high = right word\r\n");
    print("\r\n");
    print("  Running for 5 seconds...\r\n");
}

// ── Tasks ─────────────────────────────────────────────────────────────────────

static void task_blink(void *)
{
    for (;;) {
        GPIO7_DR_TOGGLE = LED_BIT;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_sai(void *)
{
    print("\r\n");
    print("NautSyn SAI clock test\r\n");
    print("======================\r\n");
    print("Cycling through all four sample rates, 5 s each.\r\n");
    print("Connect scope probes before the first rate starts.\r\n");
    print("UART: pin 1 (TX), 921600 baud.\r\n");

    uint32_t phase = 0;

    for (;;) {
        const rate_info &r = k_rates[phase % 4];

        print_scope_guide(r);

        sai1_init(&r.cfg);

        TickType_t start = xTaskGetTickCount();
        uint32_t   i     = 0;

        while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(5000)) {
            uint32_t s = (uint32_t)k_sine48[i % 48];
            sai1_write(s, s);
            ++i;
        }

        print("  Done.\r\n");
        ++phase;
    }
}

// ── FreeRTOS hooks ────────────────────────────────────────────────────────────

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char *)
{
    __asm__ volatile("bkpt #0");
    for (;;) __asm__ volatile("wfi");
}

extern "C" void vApplicationMallocFailedHook(void)
{
    __asm__ volatile("bkpt #0");
    for (;;) __asm__ volatile("wfi");
}

// ── Entry point ───────────────────────────────────────────────────────────────

extern "C" int main()
{
    // LED: Teensy pin 13 → GPIO_B0_03 → GPIO7 fast mirror
    CCM_CCGR0 |= CCM_CCGR0_GPIO2(CCM_CCGR_ON);
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 5;
    GPIO7_GDIR |= LED_BIT;

    uart_serial_init(&uart_serial1, 921600);

    xTaskCreate(task_blink, "blink", 256,     nullptr, 1, nullptr);
    xTaskCreate(task_sai,   "sai",   512,     nullptr, 1, nullptr);

    vTaskStartScheduler();
    for (;;) __asm__ volatile("wfi");
}
