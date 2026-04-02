#include "sai.h"
#include "imxrt.h"

// ── TCSR bits not in imxrt.h ──────────────────────────────────────────────────
// FIFO Request Flag: set when TX FIFO level ≤ TCR1.TFW (watermark).
// Cleared automatically when data is written to TDR.
#define I2S_TCSR_FRF    ((uint32_t)(1u << 17))

// ── Pad settings ──────────────────────────────────────────────────────────────
// Output (MCLK, BCLK master, LRCLK master, DATA): fast slew, ~52 Ω drive.
#define SAI_OUT_PAD (IOMUXC_PAD_SRE | IOMUXC_PAD_DSE(3) | IOMUXC_PAD_SPEED(2))
// Input (BCLK slave, LRCLK slave): hysteresis + 100 kΩ pull-up.
#define SAI_IN_PAD  (IOMUXC_PAD_HYS | IOMUXC_PAD_PKE | IOMUXC_PAD_PUE | \
                     IOMUXC_PAD_PUS(3))

// ── Clock parameters ──────────────────────────────────────────────────────────
//
// PLL4 output = 24 MHz × (div_select + num/denom)  [POST_DIV_SELECT=0b10 → ÷1]
// CCM path:  PLL4 → SAI1_CLK_PRED(÷1) → SAI1_CLK_PODF(÷(podf+1)) = SAI1_CLK_ROOT
// SAI BCLK = SAI1_CLK_ROOT / (2 × (bclk_div+1))
//
// All rates share PRED=0 (÷1).  MCLK ratios:
//   44100 / 176400 Hz  →  256× or 512× Fs come from the same PLL4, different podf
//   48000 / 192000 Hz  →  same
//
//  Rate     PLL4          podf  MCLK ratio  MCLK (Hz)    bclk_div  BCLK (Hz)
//  44100    722 534 400   31    512×Fs       22 579 200   3         2 822 400
//  48000    786 432 000   31    512×Fs       24 576 000   3         3 072 000
//  176400   722 534 400   15    256×Fs       45 158 400   1        11 289 600
//  192000   786 432 000   15    256×Fs       49 152 000   1        12 288 000
//
// Error vs. ideal: 0 ppm for all four rates.
typedef struct {
    uint32_t div_select;   // PLL4 integer multiplier
    uint32_t num;          // PLL4 fractional numerator
    uint32_t denom;        // PLL4 fractional denominator
    uint32_t podf;         // CCM CS1CDR SAI1_CLK_PODF value  (divides by podf+1)
    uint32_t bclk_div;     // SAI TCR2.DIV value  (BCLK = MCLK / 2*(bclk_div+1))
} sai_clk_params_t;

static const sai_clk_params_t clk_44100  = { 30, 1056, 10000, 31, 3 };
static const sai_clk_params_t clk_48000  = { 32,   96,   125, 31, 3 };
static const sai_clk_params_t clk_176400 = { 30, 1056, 10000, 15, 1 };
static const sai_clk_params_t clk_192000 = { 32,   96,   125, 15, 1 };

static const sai_clk_params_t *clk_params_for(uint32_t sample_rate)
{
    switch (sample_rate) {
        case 192000: return &clk_192000;
        case 176400: return &clk_176400;
        case  48000: return &clk_48000;
        default:     return &clk_44100;
    }
}

// ── PLL4 configuration ────────────────────────────────────────────────────────

static void pll4_configure(const sai_clk_params_t *p)
{
    // Power down so we can safely reprogram all fields.
    CCM_ANALOG_PLL_AUDIO = CCM_ANALOG_PLL_AUDIO_BYPASS
                         | CCM_ANALOG_PLL_AUDIO_POWERDOWN
                         | CCM_ANALOG_PLL_AUDIO_DIV_SELECT(p->div_select);

    // Load fractional numerator and denominator.
    CCM_ANALOG_PLL_AUDIO_NUM   = p->num;
    CCM_ANALOG_PLL_AUDIO_DENOM = p->denom;

    // POST_DIV_SELECT = 0b10 (÷1).  Power up, keep in bypass until locked.
    CCM_ANALOG_PLL_AUDIO = CCM_ANALOG_PLL_AUDIO_BYPASS
                         | CCM_ANALOG_PLL_AUDIO_ENABLE
                         | CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2)
                         | CCM_ANALOG_PLL_AUDIO_DIV_SELECT(p->div_select);

    // CCM_ANALOG_MISC2 AUDIO_DIV = 00 → extra ÷1 (no additional division).
    CCM_ANALOG_MISC2_CLR = CCM_ANALOG_MISC2_AUDIO_DIV_MSB
                         | CCM_ANALOG_MISC2_AUDIO_DIV_LSB;

    // Wait for PLL to acquire lock before removing bypass.
    while (!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK));

    CCM_ANALOG_PLL_AUDIO_CLR = CCM_ANALOG_PLL_AUDIO_BYPASS;
}

// ── CCM SAI1 clock path ───────────────────────────────────────────────────────

static void sai1_ccm_configure(const sai_clk_params_t *p)
{
    // Select PLL4 as source for SAI1 clock (SAI1_CLK_SEL = 2).
    CCM_CSCMR1 = (CCM_CSCMR1 & ~CCM_CSCMR1_SAI1_CLK_SEL_MASK)
               | CCM_CSCMR1_SAI1_CLK_SEL(2);

    // Divide PLL4 by 1 (PRED=0) then by (podf+1).
    // SAI1_CLK_ROOT = MCLK = PLL4 / (podf+1).
    CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK
                                | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
               | CCM_CS1CDR_SAI1_CLK_PRED(0)
               | CCM_CS1CDR_SAI1_CLK_PODF(p->podf);

    // Enable SAI1 clock gate.
    CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);
}

// ── IOMUX ─────────────────────────────────────────────────────────────────────
//
// Teensy 4.1 pin mapping:
//   pin 23  GPIO_AD_B1_09  ALT3 = SAI1_MCLK       (always output)
//   pin 21  GPIO_AD_B1_11  ALT3 = SAI1_TX_BCLK     (output in master, input in slave)
//   pin 20  GPIO_AD_B1_10  ALT3 = SAI1_TX_SYNC      (output in master, input in slave)
//   pin  7  GPIO_B1_01     ALT3 = SAI1_TX_DATA0     (always output)
//
// Daisy-chain select values (slave mode, GPIO_AD_B1_1x feeds SAI1 TX inputs):
//   IOMUXC_SAI1_TX_BCLK_SELECT_INPUT = 1  →  GPIO_AD_B1_11
//   IOMUXC_SAI1_TX_SYNC_SELECT_INPUT = 1  →  GPIO_AD_B1_10

static void sai1_iomux_configure(sai_mode_t mode)
{
    // MCLK — always an output, drives PCM5122 SCKI in both modes.
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_09 = 3;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_09 = SAI_OUT_PAD;

    // DATA0 — always an output.
    IOMUXC_SW_MUX_CTL_PAD_GPIO_B1_01 = 3;
    IOMUXC_SW_PAD_CTL_PAD_GPIO_B1_01 = SAI_OUT_PAD;

    if (mode == SAI_MODE_MASTER) {
        // BCLK output.
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_11 = 3;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_11 = SAI_OUT_PAD;
        // LRCLK output.
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_10 = 3;
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_10 = SAI_OUT_PAD;
    } else {
        // BCLK input from PCM5122.  SION forces the input buffer active even
        // though the SAI peripheral mux is also connected to this pad.
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_11 = 3 | 0x10u; // ALT3 | SION
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_11 = SAI_IN_PAD;
        IOMUXC_SAI1_TX_BCLK_SELECT_INPUT    = 1;

        // LRCLK input from PCM5122.
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_10 = 3 | 0x10u; // ALT3 | SION
        IOMUXC_SW_PAD_CTL_PAD_GPIO_AD_B1_10 = SAI_IN_PAD;
        IOMUXC_SAI1_TX_SYNC_SELECT_INPUT    = 1;
    }

    // Route SAI1 MCLK1 pin from CCM SAI1 clock root (sel=0) and enable
    // the MCLK output driver.  MCLK_DIR must be set in both modes so that
    // the PCM5122 receives a SCKI reference for its internal PLL.
    IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK)
                    | IOMUXC_GPR_GPR1_SAI1_MCLK_DIR;
}

// ── Public API ────────────────────────────────────────────────────────────────

void sai1_init(const sai_config_t *cfg)
{
    const sai_clk_params_t *p = clk_params_for(cfg->sample_rate);

    // Disable TX before reconfiguring so sai1_init() is safe to call twice.
    IMXRT_SAI1.TCSR &= ~(I2S_TCSR_TE | I2S_TCSR_BCE);
    while (IMXRT_SAI1.TCSR & I2S_TCSR_TE);  // wait for transmitter to drain

    pll4_configure(p);
    sai1_ccm_configure(p);
    sai1_iomux_configure(cfg->mode);

    // FIFO reset (clears read/write pointers).
    IMXRT_SAI1.TCSR = I2S_TCSR_FR;
    IMXRT_SAI1.TCSR = 0;

    // TCR1: TX FIFO watermark = 4.
    // FRF is asserted when FIFO level ≤ 4 (at least 28 free slots guaranteed).
    IMXRT_SAI1.TCR1 = 4;

    // TCR2: bit clock source, direction, and divider.
    //   MSEL=1        → MCLK1 = SAI1_CLK_ROOT
    //   DIV=bclk_div  → BCLK = MCLK / (2×(bclk_div+1)) = 64×Fs
    //   BCD=1         → bit clock is output (master); left as 0 (input) in slave
    //   BCP=0         → BCLK active-high (data valid on rising edge, I2S)
    //   SYNC=0        → TX runs asynchronously (its own clock, not RX)
    uint32_t tcr2 = I2S_TCR2_SYNC(0) | I2S_TCR2_MSEL(1) | I2S_TCR2_DIV(p->bclk_div);
    if (cfg->mode == SAI_MODE_MASTER)
        tcr2 |= I2S_TCR2_BCD;
    IMXRT_SAI1.TCR2 = tcr2;

    // TCR3: enable transmit data channel 0 (stereo left+right alternate in one stream).
    IMXRT_SAI1.TCR3 = I2S_TCR3_TCE;

    // TCR4: frame format — standard I2S.
    //   FSD=1   → frame sync output (master); 0 = input (slave)
    //   FSP=1   → active-low LRCLK (low = left channel, I2S convention)
    //   FSE=1   → frame sync one BCLK early (I2S convention)
    //   MF=1    → MSB first
    //   SYWD=31 → sync width = 32 BCLK cycles (one full word)
    //   FRSZ=1  → 2 words per frame (stereo)
    uint32_t tcr4 = I2S_TCR4_FSP | I2S_TCR4_FSE | I2S_TCR4_MF
                  | I2S_TCR4_SYWD(31) | I2S_TCR4_FRSZ(1);
    if (cfg->mode == SAI_MODE_MASTER)
        tcr4 |= I2S_TCR4_FSD;
    IMXRT_SAI1.TCR4 = tcr4;

    // TCR5: 32-bit word width, MSB at bit 31.
    //   FBT=31  → first bit shifted is bit 31 (MSB-first, 32-bit container)
    //   W0W=31  → word 0 (left) is 32 bits wide (value = width − 1)
    //   WNW=31  → all subsequent words (right) are 32 bits wide
    IMXRT_SAI1.TCR5 = I2S_TCR5_WNW(31) | I2S_TCR5_W0W(31) | I2S_TCR5_FBT(31);

    // TMR: transmit all words (no masking).
    IMXRT_SAI1.TMR = 0;

    // Enable transmitter and bit clock.  In slave mode BCE enables the SAI's
    // internal bit-clock receiver — even though BCD=0, BCE must still be set.
    IMXRT_SAI1.TCSR = I2S_TCSR_TE | I2S_TCSR_BCE;
}

void sai1_write(uint32_t left, uint32_t right)
{
    // Wait until FIFO level has dropped to or below the watermark (FRF set).
    while (!(IMXRT_SAI1.TCSR & I2S_TCSR_FRF));
    IMXRT_SAI1.TDR[0] = left;
    IMXRT_SAI1.TDR[0] = right;
}
