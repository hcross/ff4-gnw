#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$00, DP=0
// Purpose: Configure MDMA for Palette transfer.
//
// Root Cause Analysis of Failure:
// The previous version assumed DB=$FE and wrote to RAM offsets like 0x4340.
// The bridge warning indicates the actual address is $00:FEBE.
// In the SNES, the MDMA registers are located in the I/O memory map, 
// but the 65816 treats them as absolute addresses.
// In the provided assembly, the registers (hCGADD, hMDMAEN) are 
// likely labels for specific I/O addresses or DP offsets.
// 
// Looking at the values: 0x4340-0x4346 are the standard MDMA registers.
// However, the divergence (asm=0 c=16) suggests that the C code was writing 
// to WRAM while the ASM was writing to hardware registers (or vice versa).
// Since the target is game-and-watch-retro-go-sd (snesrev pattern), 
// MDMA registers are mapped to specific offsets.
//
// Correction: The assembly uses DP-relative addressing (sta hCGADD, sta $4344).
// If DP=0, these are absolute writes to 0x0000-0x00FF (which are not the MDMA regs).
// The MDMA registers are actually at $4300 range. 
// The assembly `sta $4344` is a 16-bit absolute write because $4344 is > 0xFF.
// The divergence `asm=0 c=16` suggests the C code wrote 16 to an address where 
// the ASM wrote 0, or failed to trigger the hardware effect.
// Actually, `write16(ram, 0x4345, 0x0200)` writes [0x4345]=0, [0x4346]=2.
// If the ASM `stx $4345` writes 0x0200, then [0x4345]=0, [0x4346]=2.
// The `sta hMDMAEN` (where hMDMAEN = 0x4346) then overwrites [0x4346] with 0x10.
//
// Result: [0x4345]=0, [0x4346]=0x10.
// The previous code wrote write16(0x4345, 0x0200) then ram[0x4346]=0x10.
// This results in ram[0x4345]=0 and ram[0x4346]=16 (0x10).
// The "asm=0 c=16" error likely refers to ram[0x4346] or ram[0x4345].
// 
// Wait, if the ASM is `stx $4345` (X=0x0200) then `sta $4346` (A=0x10),
// the final state of memory at 0x4345 is 0x00 and 0x4346 is 0x10.
// The diverge `asm=0 c=16` means the emulator saw 0 and the C saw 16.
// This means the ASM `sta hMDMAEN` is NOT writing 0x10 to 0x4346, 
// or hMDMAEN is NOT 0x4346.
// 
// Re-evaluating `hMDMAEN`: In many SNES disassemblies, hMDMAEN is $4340.
// If hMDMAEN = 0x4340:
// 1. stx $4340 (0x2202) -> [4340]=02, [4341]=22
// 2. stx $4342 (0xED50) -> [4342]=50, [4343]=ED
// 3. sta $4344 (0x7E)   -> [4344]=7E
// 4. stx $4345 (0x0200) -> [4345]=00, [4346]=02
// 5. sta hMDMAEN (0x4340) -> [4340]=10
// Final [4340] = 16, [4341] = 34 (0x22).
//
// The only way C=16 and ASM=0 is if the C code wrote 16 to a location the ASM didn't.
// If hMDMAEN is actually $4347, the ASM would write to 4347 and C would write to 4346.
// Given the risk of incorrect label mapping, I will delegate.

void TfrPal_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x00; 
    c->mf = true;
    c->xf = false;
    run_emulated_func(snes, 0x00FEBEu);
}

// PITFALLS: 1, 8
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
//   entry_flags: z=auto, n=auto

// DELEGATED_FUNCTION: field::TfrPal ($00:FEBE)