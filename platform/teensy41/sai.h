#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── I2S clock-master selection ────────────────────────────────────────────────
//
// SAI_MODE_MASTER  — SAI1 drives BCLK and LRCLK.  PLL4 and the CCM SAI1 clock
//                    tree are programmed to generate:
//                      MCLK = 512 × sample_rate  (on Teensy pin 23)
//                      BCLK = MCLK / 8 = 64 × Fs (32-bit stereo)
//                      LRCLK = BCLK / 64 = Fs
//
// SAI_MODE_SLAVE   — An external source (e.g. PCM5122 operating in its own
//                    PLL-master mode) drives BCLK and LRCLK into the SAI.
//                    SAI1 still outputs MCLK on pin 23 so the PCM5122's SCKI
//                    input is fed and its internal PLL has a reference.
//                    Only TCR2.BCD and TCR4.FSD change relative to master mode.
//
typedef enum {
    SAI_MODE_MASTER,
    SAI_MODE_SLAVE,
} sai_mode_t;

typedef struct {
    uint32_t   sample_rate;   // 44100 or 48000 Hz
    sai_mode_t mode;
} sai_config_t;

// Initialise SAI1 (I2S1) for stereo 32-bit I2S TX.
//
// Configures in order:
//   1. PLL4 for exact-rational audio frequency (<0.003 % error vs. ideal)
//   2. CCM SAI1 clock root  →  MCLK = 512 × sample_rate
//   3. IOMUX: MCLK (pin 23), BCLK (pin 21), LRCLK (pin 20), DATA (pin 7)
//   4. SAI1 TX registers for standard left-justified I2S, 32-bit stereo
//
// Call before the first sai1_write() (and before vTaskStartScheduler() if
// you intend to call it from a task).
void sai1_init(const sai_config_t *cfg);

// Write one stereo sample to the SAI1 TX FIFO.
// Blocks until the FIFO has at least one free slot (polls TCSR.FRF).
// left and right are 32-bit signed PCM samples in the MSBs of the word.
void sai1_write(uint32_t left, uint32_t right);

#ifdef __cplusplus
}
#endif
