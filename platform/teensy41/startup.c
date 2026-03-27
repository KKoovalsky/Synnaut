// NautSyn — Teensy 4.1 (IMXRT1062) startup
//
// Responsible for:
//   1. FlexSPI NOR flash configuration block   (.flashconfig)
//   2. NXP Image Vector Table + boot data      (.ivt / .bootdata)
//   3. ResetHandler — FlexRAM config, SP, memory init, FPU, constructors, main

#include <stdint.h>

// ---------------------------------------------------------------------------
// Linker symbols (defined in teensy41.ld)
// ---------------------------------------------------------------------------
extern uint32_t _stextload;       // ITCM load address in flash
extern uint32_t _stext, _etext;   // ITCM run address range
extern uint32_t _sdataload;       // .data load address in flash
extern uint32_t _sdata, _edata;   // .data run address range
extern uint32_t _sbss,  _ebss;    // .bss range (must be zeroed)
extern uint32_t _flexram_bank_config; // computed by linker, written to GPR17
extern uint32_t _estack;          // top of DTCM stack
extern uint32_t _flashimagelen;   // total flash image size (for boot data)

// ---------------------------------------------------------------------------
// Register addresses
// ---------------------------------------------------------------------------
#define IOMUXC_GPR_GPR14  (*(volatile uint32_t *)0x400AC038u)
#define IOMUXC_GPR_GPR16  (*(volatile uint32_t *)0x400AC040u)
#define IOMUXC_GPR_GPR17  (*(volatile uint32_t *)0x400AC044u)
#define SCB_CPACR         (*(volatile uint32_t *)0xE000ED88u)

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void ResetHandler(void);
void ResetHandler2(void);

// ---------------------------------------------------------------------------
// FlexSPI NOR Flash Configuration Block (FCB)
// IMXRT1060 RM §8.6.3 / MCU_Flashloader_Reference_Manual §8.2.1
// 512 bytes placed at the very start of flash (0x60000000).
// ---------------------------------------------------------------------------
__attribute__((section(".flashconfig"), used))
const uint32_t FlexSPI_NOR_Config[128] = {
    // --- Common FlexSPI configuration block (448 bytes) ---
    0x42464346,     // [0x000] Tag "FCFB"
    0x56010000,     // [0x004] Version 1.0
    0,              // [0x008] reserved
    0x00030301,     // [0x00C] columnAddrWidth=0, dataSetup=3, dataHold=3, readSampleClkSrc=1

    0x00000000,     // [0x010] waitTimeCfgCommands / deviceModeCfgEnable
    0,              // [0x014] deviceModeSeq
    0,              // [0x018] deviceModeArg
    0x00000000,     // [0x01C] configCmdEnable

    0, 0, 0, 0,     // [0x020] configCmdSeqs
    0, 0, 0, 0,     // [0x030] cfgCmdArgs

    0x00000000,     // [0x040] controllerMiscOption
    0x00080401,     // [0x044] lutCustomSeqEnable=0, serialClkFreq=8(133MHz), sflashPadType=4(Quad), deviceType=1(NOR)
    0, 0,           // [0x048] reserved

    0x00800000,     // [0x050] sflashA1Size = 8 MB (Teensy 4.1)
    0,              // [0x054] sflashA2Size
    0,              // [0x058] sflashB1Size
    0,              // [0x05C] sflashB2Size

    0, 0, 0, 0,     // [0x060] pad overrides
    0, 0, 0, 0,     // [0x070] timeout / interval / dataValidTime / busyBit

    // Lookup Table (LUT) — read, read status, write enable, erase, program
    0x0A1804EB,     // [0x080] LUT[0]  Fast Read Quad
    0x26043206,     // [0x081] LUT[1]
    0, 0,           // [0x082-3]
    0x24040405,     // [0x084] LUT[4]  Read Status
    0, 0, 0,
    0, 0, 0, 0,     // [0x088-B]
    0x00000406,     // [0x08C] LUT[12] Write Enable
    0, 0, 0,
    0, 0, 0, 0,     // [0x090-3]
    0, 0, 0, 0,     // [0x094-7]
    0x08180420,     // [0x098] LUT[20] Erase Sector
    0, 0, 0,
    0, 0, 0, 0,     // [0x09C-F]
    0, 0, 0, 0,     // [0x0A0-3]  (LUT[24-27])
    0, 0, 0, 0,     // [0x0A4-7]  (LUT[28-31])
    0x081804D8,     // [0x0A8] LUT[32] Page Program
    0, 0, 0,
    0x08180402,     // [0x0AC] LUT[36]
    0x00002004,
    0, 0,
    0, 0, 0, 0,     // [0x0B0-3]
    0x00000460,     // [0x0B4] LUT[44] Chip Erase
    0, 0, 0,
    0, 0, 0, 0,     // [0x0B8-B]
    0, 0, 0, 0,     // [0x0BC-F]

    // --- Serial NOR configuration block (64 bytes) ---
    256,            // [0x1C0] pageSize
    4096,           // [0x1C4] sectorSize
    1,              // [0x1C8] ipCmdSerialClkFreq
    0,              // [0x1CC] reserved
    0x00010000,     // [0x1D0] blockSize
    0, 0, 0,        // reserved
    0, 0, 0, 0,     // reserved
    0, 0, 0, 0,     // reserved
};

// ---------------------------------------------------------------------------
// Boot data — referenced by the IVT
// ---------------------------------------------------------------------------
__attribute__((section(".bootdata"), used))
const uint32_t BootData[3] = {
    0x60000000,                 // image start address
    (uint32_t)&_flashimagelen,  // image length
    0,                          // plugin flag
};

// HAB Code Signing File placeholder (required even when not signed)
__attribute__((section(".csf"), used))
const uint32_t hab_csf[768];

// ---------------------------------------------------------------------------
// Image Vector Table — boot ROM reads this at 0x60001000 to find our entry
// ---------------------------------------------------------------------------
__attribute__((section(".ivt"), used))
const uint32_t ImageVectorTable[8] = {
    0x432000D1,                     // header (tag=0xD1, length=0x2000, version=0x43)
    (uint32_t)&ResetHandler,        // program entry point
    0,                              // reserved
    0,                              // DCD (device config data) — unused
    (uint32_t)BootData,             // absolute address of boot data
    (uint32_t)ImageVectorTable,     // absolute address of IVT itself
    (uint32_t)hab_csf,             // command sequence file
    0,                              // reserved
};

// ---------------------------------------------------------------------------
// Reset Handler — runs in flash, before ITCM is populated
// ---------------------------------------------------------------------------

// Set the stack pointer and branch to phase 2.
// Clang requires naked functions to contain only inline asm.
__attribute__((section(".startup"), naked))
void ResetHandler(void)
{
    __asm__ volatile(
        "ldr r0, =_estack   \n"
        "mov sp, r0          \n"
        "b   ResetHandler2   \n"
    );
}

// Phase 2 runs in flash with a valid stack.
// FlexRAM must be configured here, before the ITCM/DTCM memory copies.
__attribute__((section(".startup"), noinline, noreturn))
void ResetHandler2(void)
{
    IOMUXC_GPR_GPR17 = (uint32_t)&_flexram_bank_config; // ITCM/DTCM bank split
    IOMUXC_GPR_GPR16 = 0x00200007; // enable FlexRAM, use GPR17 config
    IOMUXC_GPR_GPR14 = 0x00AA0000; // OCRAM bank config

    // Copy .text.itcm: stored in flash (_stextload), runs from ITCM (_stext)
    {
        uint32_t *src = &_stextload, *dst = &_stext;
        while (dst < &_etext) *dst++ = *src++;
    }

    // Copy .data: stored in flash (_sdataload), runs from DTCM (_sdata)
    {
        uint32_t *src = &_sdataload, *dst = &_sdata;
        while (dst < &_edata) *dst++ = *src++;
    }

    // Zero .bss in DTCM
    {
        uint32_t *dst = &_sbss;
        while (dst < &_ebss) *dst++ = 0;
    }

    // Enable FPU: set CP10 and CP11 to full access in CPACR
    SCB_CPACR = 0x00F00000;
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    // Call C++ static constructors
    extern void (*__init_array_start[])(void);
    extern void (*__init_array_end[])(void);
    for (void (**f)(void) = __init_array_start; f < __init_array_end; ++f)
        (*f)();

    extern int main(void);
    main();

    // main() must not return on bare metal; sleep if it does
    while (1) __asm__ volatile("wfi");
}
