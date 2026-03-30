/* NautSyn — Teensy 4.1 (IMXRT1062) platform startup
 *
 * Derived from the PJRC Teensyduino core (MIT License).
 * https://github.com/PaulStoffregen/cores
 *
 * Stripped of: Arduino runtime (USB, Serial, Wire, analogRead/Write,
 * tempmon, millis/SysTick), PSRAM auto-detect.
 * Retained:  FCB/IVT/boot-data, ResetHandler, configure_cache,
 *            usb_pll_start, set_arm_clock hook, RAM vector table,
 *            weak startup hooks, newlib syscall stubs.
 */

#include "imxrt.h"
#include <stdint.h>
#include <sys/stat.h>

// ---------------------------------------------------------------------------
// Linker symbols
// ---------------------------------------------------------------------------
extern unsigned long _stextload, _stext, _etext;
extern unsigned long _sdataload, _sdata, _edata;
extern unsigned long _sbss, _ebss;
extern unsigned long _flexram_bank_config;
extern unsigned long _estack;
extern unsigned long _heap_start, _heap_end;
extern unsigned long _flashimagelen;

// ---------------------------------------------------------------------------
// Convenience attribute: keep in XIP flash (must be usable before ITCM copy)
// ---------------------------------------------------------------------------
#define FLASHMEM __attribute__((section(".flashmem")))

// ---------------------------------------------------------------------------
// RAM vector table  (aligned to 1 KB as required by SCB_VTOR)
// Placed in .vectorsram which the linker puts in DTCM via the .data copy.
// ---------------------------------------------------------------------------
#define NVIC_NUM_INTERRUPTS 160

__attribute__((used, aligned(1024), section(".vectorsram")))
void (* volatile _VectorsRam[NVIC_NUM_INTERRUPTS + 16])(void);

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void ResetHandler(void);
static void ResetHandler2(void) __attribute__((noreturn));
static void memory_copy(uint32_t *dest, const uint32_t *src, uint32_t *dest_end);
static void memory_clear(uint32_t *dest, uint32_t *dest_end);
void configure_cache(void);
void usb_pll_start(void);
void default_isr(void);
uint32_t set_arm_clock(uint32_t frequency); // clockspeed.c

// ---------------------------------------------------------------------------
// Weak startup hooks
// Defaults are FLASHMEM no-ops callable before ITCM is populated.
// Override by defining the non-default name in any translation unit.
//
//   early_hook  — right after FlexRAM init, before memory copies
//   middle_hook — after clocks, cache, vector table; before constructors
//   late_hook   — after constructors, just before main()
// ---------------------------------------------------------------------------
FLASHMEM void startup_default_early_hook(void)  {}
FLASHMEM void startup_default_middle_hook(void) {}
FLASHMEM void startup_default_late_hook(void)   {}

void startup_early_hook(void)
    __attribute__((weak, alias("startup_default_early_hook")));
void startup_middle_hook(void)
    __attribute__((weak, alias("startup_default_middle_hook")));
void startup_late_hook(void)
    __attribute__((weak, alias("startup_default_late_hook")));

// Debugger breakpoint target — set a hardware BP here to stop at entry.
extern void startup_debug_reset(void) __attribute__((noinline));
FLASHMEM void startup_debug_reset(void) { __asm__ volatile("nop"); }

// ---------------------------------------------------------------------------
// Flash Configuration Block (FCB) — 0x60000000, 512 bytes
// Correct for the W25Q64JV (8 MB, Quad SPI, 133 MHz) on Teensy 4.1.
// Source: PJRC bootdata.c (MIT License)
// ---------------------------------------------------------------------------
__attribute__((section(".flashconfig"), used))
const uint32_t FlexSPI_NOR_Config[128] = {
    // 448-byte common FlexSPI configuration block
    // RT1060 RM §8.6.3.1 / MCU_Flashloader_Reference_Manual §8.2.1
    0x42464346,  // [0x000] Tag "FCFB"
    0x56010000,  // [0x004] Version 1.0
    0,           // [0x008] reserved
    0x00030301,  // [0x00C] columnAddrWidth=0, dataSetup=3, dataHold=3, readSampleClkSrc=1

    0x00000000,  // [0x010] waitTimeCfgCommands / deviceModeCfgEnable
    0,           // [0x014] deviceModeSeq
    0,           // [0x018] deviceModeArg
    0x00000000,  // [0x01C] configCmdEnable

    0, 0, 0, 0,  // [0x020] configCmdSeqs
    0, 0, 0, 0,  // [0x030] cfgCmdArgs

    0x00000000,  // [0x040] controllerMiscOption
    0x00080401,  // [0x044] serialClkFreq=8(133MHz), sflashPadType=4(Quad), deviceType=1(NOR)
    0, 0,        // [0x048] reserved

    0x00800000,  // [0x050] sflashA1Size = 8 MB (Teensy 4.1 / W25Q64JV)
    0,           // [0x054] sflashA2Size
    0,           // [0x058] sflashB1Size
    0,           // [0x05C] sflashB2Size

    0, 0, 0, 0,  // [0x060] pad overrides
    0, 0, 0, 0,  // [0x070] timeout / interval / dataValidTime / busyBit

    // Lookup Table (LUT)
    0x0A1804EB,  // lookupTable[0]  — Fast Read Quad (1-4-4)
    0x26043206,  // lookupTable[1]
    0, 0,
    0x24040405,  // lookupTable[4]  — Read Status
    0, 0, 0,
    0, 0, 0, 0,
    0x00000406,  // lookupTable[12] — Write Enable
    0, 0, 0,
    0, 0, 0, 0,
    0x08180420,  // lookupTable[20] — Erase Sector
    0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0x081804D8,  // lookupTable[32] — Page Program
    0, 0, 0,
    0x08180402,  // lookupTable[36]
    0x00002004,
    0, 0,
    0, 0, 0, 0,
    0x00000460,  // lookupTable[44] — Chip Erase
    0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,

    // 64-byte Serial NOR configuration block (§8.6.3.2)
    256,         // [0x1C0] pageSize
    4096,        // [0x1C4] sectorSize
    1,           // [0x1C8] ipCmdSerialClkFreq
    0,           // [0x1CC] reserved
    0x00010000,  // [0x1D0] blockSize (64 KB)
    0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

// ---------------------------------------------------------------------------
// Boot data + HAB CSF placeholder + Image Vector Table
// IVT must land at 0x60001000 (XIP offset per RT1062 boot ROM).
// ---------------------------------------------------------------------------
__attribute__((section(".bootdata"), used))
const uint32_t BootData[3] = {
    0x60000000,               // image start
    (uint32_t)&_flashimagelen,// image length (linker symbol)
    0,                        // plugin flag
};

__attribute__((section(".csf"), used))
const uint32_t hab_csf[768]; // HAB Code Signing File placeholder

__attribute__((section(".ivt"), used))
const uint32_t ImageVectorTable[8] = {
    0x432000D1,                  // header: tag=0xD1, length=0x2000, version=0x43
    (uint32_t)&ResetHandler,     // entry point
    0,                           // reserved
    0,                           // DCD — not used
    (uint32_t)BootData,          // absolute address of boot data
    (uint32_t)ImageVectorTable,  // self-reference
    (uint32_t)hab_csf,           // CSF (unsigned: placeholder)
    0,                           // reserved
};

// ---------------------------------------------------------------------------
// Reset Handler — phase 1 (runs in XIP flash, no stack available yet)
//
// ORDER: configure FlexRAM FIRST (GPR17/16/14), THEN set SP.
// Boot ROM leaves all FlexRAM as OCRAM; DTCM at 0x20000000 does not exist
// until GPR16 bit-2 (FLEXRAM_BANK_CFG_SEL) is set.  Setting SP to a DTCM
// address before that write would make the first stack push hit unmapped mem.
//
// The volatile register writes compile to plain str instructions with no
// stack access, making them safe to use in a naked function body.
// Pattern follows PJRC's tested implementation.
// ---------------------------------------------------------------------------
__attribute__((section(".startup"), naked))
void ResetHandler(void)
{
    IOMUXC_GPR_GPR17 = (uint32_t)&_flexram_bank_config;
    IOMUXC_GPR_GPR16 = 0x00200007; // enable FlexRAM, use GPR17 config
    IOMUXC_GPR_GPR14 = 0x00AA0000; // OCRAM bank config
    __asm__ volatile("mov sp, %0" :: "r"((uint32_t)&_estack) : "memory");
    ResetHandler2();
    __builtin_unreachable();
}

// ---------------------------------------------------------------------------
// Reset Handler — phase 2 (C, valid stack, FlexRAM configured)
// ---------------------------------------------------------------------------
__attribute__((section(".startup"), noinline, noreturn))
static void ResetHandler2(void)
{
    unsigned int i;

    __asm__ volatile("dsb" ::: "memory");

    // Must be FLASHMEM — ITCM not yet copied.
    startup_early_hook();

    // Use bandgap-based bias currents for best analog performance (RM §12.4)
    PMU_MISC0_SET = 1 << 3;

    // Configure PLL PFD fractional dividers.
    // SYS PLL (528 MHz): PFD3=297, PFD2=396, PFD1=594, PFD0=352 MHz
    // USB PLL (480 MHz): PFD3=454, PFD2=508, PFD1=664, PFD0=720 MHz
    CCM_ANALOG_PFD_528 = 0x2018101B;
    CCM_ANALOG_PFD_480 = 0x13110D0C;

    // Copy .text.itcm from flash load address to ITCM run address
    memory_copy(&_stext, &_stextload, &_etext);
    // Copy .data from flash load address to DTCM run address
    memory_copy(&_sdata, &_sdataload, &_edata);
    // Zero .bss in DTCM
    memory_clear(&_sbss, &_ebss);

    // Enable FPU: CP10 and CP11 full access
    SCB_CPACR = 0x00F00000;
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    // Populate RAM vector table with the default ISR, then redirect VTOR.
    // Doing this before any peripheral init ensures no unhandled interrupt
    // silently falls through.
    for (i = 0; i < NVIC_NUM_INTERRUPTS + 16; i++)
        _VectorsRam[i] = default_isr;
    for (i = 0; i < NVIC_NUM_INTERRUPTS; i++)
        NVIC_SET_PRIORITY(i, 128);
    SCB_VTOR = (uint32_t)_VectorsRam;

    // Enable MemManage, BusFault, UsageFault separate exception handlers
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA | SCB_SHCSR_BUSFAULTENA |
                 SCB_SHCSR_USGFAULTENA;

    // PIT and GPT timers: clock from 24 MHz crystal (independent of ARM speed)
    CCM_CSCMR1 = (CCM_CSCMR1 & ~CCM_CSCMR1_PERCLK_PODF(0x3F)) |
                 CCM_CSCMR1_PERCLK_CLK_SEL;
    // LPUART: clock from 24 MHz crystal (works regardless of PLL3 state)
    CCM_CSCDR1 = (CCM_CSCDR1 & ~CCM_CSCDR1_UART_CLK_PODF(0x3F)) |
                 CCM_CSCDR1_UART_CLK_SEL;

    // Route all GPIO to fast GPIO6-9 shadow registers (single-cycle access)
    IOMUXC_GPR_GPR26 = 0xFFFFFFFF;
    IOMUXC_GPR_GPR27 = 0xFFFFFFFF;
    IOMUXC_GPR_GPR28 = 0xFFFFFFFF;
    IOMUXC_GPR_GPR29 = 0xFFFFFFFF;

    configure_cache();

    // usb_pll_start must precede set_arm_clock: the clock-switching logic in
    // set_arm_clock uses PLL3 (480 MHz) as an intermediate source while
    // reprogramming the ARM PLL.
    usb_pll_start();
    set_arm_clock(F_CPU);

    // Undo PIT timer usage left by the boot ROM
    CCM_CCGR1 |= CCM_CCGR1_PIT(CCM_CCGR_ON);
    PIT_MCR = 0;
    PIT_TCTRL0 = 0;
    PIT_TCTRL1 = 0;
    PIT_TCTRL2 = 0;
    PIT_TCTRL3 = 0;

    startup_middle_hook();

    // C++ global constructors
    extern void (*__init_array_start[])(void);
    extern void (*__init_array_end[])(void);
    for (void (**f)(void) = __init_array_start; f < __init_array_end; f++)
        (*f)();

    startup_debug_reset(); // set a hardware BP here to catch startup faults
    startup_late_hook();

    extern int main(void);
    main();
    while (1) __asm__ volatile("wfi");
}

// ---------------------------------------------------------------------------
// configure_cache — MPU regions + I-cache + D-cache
// Source: PJRC startup.c (MIT License)
// ---------------------------------------------------------------------------
#define NOEXEC      SCB_MPU_RASR_XN
#define READONLY    SCB_MPU_RASR_AP(7)
#define READWRITE   SCB_MPU_RASR_AP(3)
#define NOACCESS    SCB_MPU_RASR_AP(0)
#define MEM_CACHE_WT   (SCB_MPU_RASR_TEX(0) | SCB_MPU_RASR_C)
#define MEM_CACHE_WB   (SCB_MPU_RASR_TEX(0) | SCB_MPU_RASR_C | SCB_MPU_RASR_B)
#define MEM_CACHE_WBWA (SCB_MPU_RASR_TEX(1) | SCB_MPU_RASR_C | SCB_MPU_RASR_B)
#define MEM_NOCACHE    (SCB_MPU_RASR_TEX(1))
#define DEV_NOCACHE    (SCB_MPU_RASR_TEX(2))
#define SIZE_32B   (SCB_MPU_RASR_SIZE(4)  | SCB_MPU_RASR_ENABLE)
#define SIZE_128K  (SCB_MPU_RASR_SIZE(16) | SCB_MPU_RASR_ENABLE)
#define SIZE_512K  (SCB_MPU_RASR_SIZE(18) | SCB_MPU_RASR_ENABLE)
#define SIZE_1M    (SCB_MPU_RASR_SIZE(19) | SCB_MPU_RASR_ENABLE)
#define SIZE_16M   (SCB_MPU_RASR_SIZE(23) | SCB_MPU_RASR_ENABLE)
#define SIZE_32M   (SCB_MPU_RASR_SIZE(24) | SCB_MPU_RASR_ENABLE)
#define SIZE_64M   (SCB_MPU_RASR_SIZE(25) | SCB_MPU_RASR_ENABLE)
#define SIZE_1G    (SCB_MPU_RASR_SIZE(29) | SCB_MPU_RASR_ENABLE)
#define SIZE_4G    (SCB_MPU_RASR_SIZE(31) | SCB_MPU_RASR_ENABLE)
#define REGION(n)  (SCB_MPU_RBAR_REGION(n) | SCB_MPU_RBAR_VALID)

FLASHMEM void configure_cache(void)
{
    SCB_MPU_CTRL = 0; // disable MPU before reprogramming

    uint32_t i = 0;
    // Region 0: deny-all catch-all (prevents speculative reads into void)
    SCB_MPU_RBAR = 0x00000000 | REGION(i++);
    SCB_MPU_RASR = SCB_MPU_RASR_TEX(0) | NOACCESS | NOEXEC | SIZE_4G;

    // Region 1: ITCM — no-cache (TCM is already single-cycle; cache is wasted)
    SCB_MPU_RBAR = 0x00000000 | REGION(i++);
    SCB_MPU_RASR = MEM_NOCACHE | READONLY | SIZE_512K;

    // Region 2: NULL-pointer trap (32 bytes at 0x0)
    SCB_MPU_RBAR = 0x00000000 | REGION(i++);
    SCB_MPU_RASR = DEV_NOCACHE | NOACCESS | SIZE_32B;

    // Region 3: Boot ROM
    SCB_MPU_RBAR = 0x00200000 | REGION(i++);
    SCB_MPU_RASR = MEM_CACHE_WT | READONLY | SIZE_128K;

    // Region 4: DTCM — no-cache (TCM), no execute
    SCB_MPU_RBAR = 0x20000000 | REGION(i++);
    SCB_MPU_RASR = MEM_NOCACHE | READWRITE | NOEXEC | SIZE_512K;

    // Region 5: stack-overflow trap (32 bytes just above .bss)
    SCB_MPU_RBAR = ((uint32_t)&_ebss) | REGION(i++);
    SCB_MPU_RASR = SCB_MPU_RASR_TEX(0) | NOACCESS | NOEXEC | SIZE_32B;

    // Region 6: OCRAM (AXI bus) — write-back write-allocate cache, no execute
    SCB_MPU_RBAR = 0x20200000 | REGION(i++);
    SCB_MPU_RASR = MEM_CACHE_WBWA | READWRITE | NOEXEC | SIZE_1M;

    // Region 7: Peripherals — device (strongly ordered), no execute
    SCB_MPU_RBAR = 0x40000000 | REGION(i++);
    SCB_MPU_RASR = DEV_NOCACHE | READWRITE | NOEXEC | SIZE_64M;

    // Region 8: QSPI flash (FlexSPI1) — write-back write-allocate, read-only
    SCB_MPU_RBAR = 0x60000000 | REGION(i++);
    SCB_MPU_RASR = MEM_CACHE_WBWA | READONLY | SIZE_16M;

    // Region 9: FlexSPI2 (PSRAM / external flash) — WBWA, no execute
    SCB_MPU_RBAR = 0x70000000 | REGION(i++);
    SCB_MPU_RASR = MEM_CACHE_WBWA | READWRITE | NOEXEC | SIZE_32M;

    __asm__ volatile("nop\nnop\nnop\nnop\nnop" ::: "memory"); // let writes settle
    SCB_MPU_CTRL = SCB_MPU_CTRL_ENABLE;

    // Enable instruction and data caches (ARM DDI0403E §B3.2.3)
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    SCB_CACHE_ICIALLU = 0; // invalidate I-cache
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    SCB_CCR |= (SCB_CCR_IC | SCB_CCR_DC);
}

// ---------------------------------------------------------------------------
// usb_pll_start — bring PLL3 (480 MHz) to a known good state
//
// PLL3 is not just for USB: its PFD outputs (PFD0=720, PFD1=664, PFD2=508,
// PFD3=454 MHz) feed the peripheral clock muxes for FlexSPI, LPI2C, SAI, etc.
// set_arm_clock also uses PLL3/6 (=120 MHz) as a safe intermediate clock
// source while reconfiguring the ARM PLL.
//
// Source: PJRC startup.c (MIT License), printf calls removed.
// Reference: NXP EB790
// ---------------------------------------------------------------------------
FLASHMEM void usb_pll_start(void)
{
    while (1) {
        uint32_t n = CCM_ANALOG_PLL_USB1;
        if (n & CCM_ANALOG_PLL_USB1_DIV_SELECT) {
            // Should never be in 528 MHz mode — reset and retry
            CCM_ANALOG_PLL_USB1_CLR = 0xC000;
            CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_BYPASS;
            CCM_ANALOG_PLL_USB1_CLR = CCM_ANALOG_PLL_USB1_POWER |
                                      CCM_ANALOG_PLL_USB1_DIV_SELECT |
                                      CCM_ANALOG_PLL_USB1_ENABLE |
                                      CCM_ANALOG_PLL_USB1_EN_USB_CLKS;
            continue;
        }
        if (!(n & CCM_ANALOG_PLL_USB1_ENABLE)) {
            CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_ENABLE;
            continue;
        }
        if (!(n & CCM_ANALOG_PLL_USB1_POWER)) {
            CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_POWER;
            continue;
        }
        if (!(n & CCM_ANALOG_PLL_USB1_LOCK)) {
            continue; // wait for PLL lock
        }
        if (n & CCM_ANALOG_PLL_USB1_BYPASS) {
            CCM_ANALOG_PLL_USB1_CLR = CCM_ANALOG_PLL_USB1_BYPASS;
            continue;
        }
        if (!(n & CCM_ANALOG_PLL_USB1_EN_USB_CLKS)) {
            CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_EN_USB_CLKS;
            continue;
        }
        return; // PLL3 running and stable
    }
}

// ---------------------------------------------------------------------------
// default_isr — fallback for any unregistered interrupt or fault
// Disables IRQ and halts.  A debugger can inspect IPSR to identify the cause.
// ---------------------------------------------------------------------------
void default_isr(void)
{
    __disable_irq();
    while (1) __asm__ volatile("wfi");
}

// ---------------------------------------------------------------------------
// memory_copy / memory_clear — section-copy helpers
// Placed in .startup so they run from flash before ITCM is populated.
// Source: PJRC startup.c (MIT License), asm versions for reliability.
// ---------------------------------------------------------------------------
__attribute__((section(".startup"), noinline))
static void memory_copy(uint32_t *dest, const uint32_t *src, uint32_t *dest_end)
{
    __asm__ volatile(
        "   cmp     %[end], %[dest]     \n"
        "   beq.n   2f                  \n"
        "1: ldr.w   r3, [%[src]], #4   \n"
        "   str.w   r3, [%[dest]], #4  \n"
        "   cmp     %[end], %[dest]     \n"
        "   bhi.n   1b                  \n"
        "2:                             \n"
        : [dest] "+r"(dest), [src] "+r"(src)
        : [end] "r"(dest_end)
        : "r3", "memory"
    );
}

__attribute__((section(".startup"), noinline))
static void memory_clear(uint32_t *dest, uint32_t *dest_end)
{
    __asm__ volatile(
        "   ldr     r3, =0              \n"
        "1: str.w   r3, [%[dest]], #4  \n"
        "   cmp     %[end], %[dest]     \n"
        "   bhi.n   1b                  \n"
        : [dest] "+r"(dest)
        : [end] "r"(dest_end)
        : "r3", "memory"
    );
}

// ---------------------------------------------------------------------------
// Newlib syscall stubs
// ---------------------------------------------------------------------------
char *__brkval = (char *)&_heap_start;

__attribute__((weak))
void *_sbrk(int incr)
{
    char *prev = __brkval;
    if (incr != 0) {
        if (prev + incr > (char *)&_heap_end)
            return (void *)-1;
        __brkval = prev + incr;
    }
    return prev;
}

__attribute__((weak))
int _read(int file __attribute__((unused)),
          char *ptr __attribute__((unused)),
          int len __attribute__((unused)))
{ return 0; }

__attribute__((weak))
int _close(int fd __attribute__((unused)))
{ return -1; }

__attribute__((weak))
int _fstat(int fd __attribute__((unused)), struct stat *st)
{ st->st_mode = S_IFCHR; return 0; }

__attribute__((weak))
int _isatty(int fd __attribute__((unused)))
{ return 1; }

__attribute__((weak))
int _lseek(int fd __attribute__((unused)),
           long long offset __attribute__((unused)),
           int whence __attribute__((unused)))
{ return -1; }

__attribute__((weak))
void _exit(int status __attribute__((unused)))
{ while (1) __asm__ volatile("wfi"); }

__attribute__((weak))
void __cxa_pure_virtual(void)
{ while (1) __asm__ volatile("wfi"); }

__attribute__((weak))
int __cxa_guard_acquire(char *g)
{ return !(*g); }

__attribute__((weak))
void __cxa_guard_release(char *g)
{ *g = 1; }

__attribute__((weak))
void abort(void)
{ while (1) __asm__ volatile("wfi"); }
