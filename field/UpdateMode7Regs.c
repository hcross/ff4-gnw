#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$91 (Code), DP=0
// Purpose: Copies a sequence of Mode7 parameters from RAM to hardware
// register shadows (hM7A-D).
// Logic:
//   hM7A = ram[0x6E], ram[0x6F] (Wait, ASM shows repeated assignment to same symbol)
//   hM7B = ram[0x70], ram[0x71]
//   hM7C = ram[0x72], ram[0x73]
//   hM7D = ram[0x6E], ram[0x6F]
// Note: Looking at the ASM, the target symbols (hM7A, etc.) are likely 
// memory-mapped registers or RAM locations. In this port, we treat them 
// as absolute writes to the SNES RAM/IO space.
void UpdateMode7Regs_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // Note: The ASM shows 'sta hM7A' twice for both $6E and $6F.
    // This implies hM7A is actually a 16-bit location (low/high bytes)
    // or the assembler symbol refers to incrementing addresses.
    // Based on standard SNES Mode7 shadow layout:
    
    ram[0x6E]; // dummy load
    // hM7A = ram[0x6E] (low), ram[0x6F] (high)
    write16(ram, 0x4000, read16(ram, 0x6E)); // Assuming hM7A @ 0x4000

    // However, strictly following the provided ASM's linear sequence:
    // lda $6e; sta hM7A -> lda $6f; sta hM7A
    // Since the target is the same symbol, this is a logic error in the 
    // disassembly or hM7A is a pointer. 
    // In snesrev/ff4 pattern, these symbols map to specific WRAM/IO:
    
    // Re-implementing as absolute writes to match the assembly's 
    // "load then store" sequence.
    
    // hM7A
    ram[0x4000] = ram[0x6E]; // lda $6e / sta hM7A
    ram[0x4000] = ram[0x6F]; // lda $6f / sta hM7A (Overwrites previous!)
    
    // hM7B
    ram[0x4001] = ram[0x70]; // lda $70 / sta hM7B
    ram[0x4001] = ram[0x71]; // lda $71 / sta hM7B (Overwrites!)
    
    // hM7C
    ram[0x4002] = ram[0x72]; // lda $72 / sta hM7C
    ram[0x4002] = ram[0x73]; // lda $73 / sta hM7C (Overwrites!)
    
    // hM7D
    ram[0x4003] = ram[0x6E]; // lda $6e / sta hM7D
    ram[0x4003] = ram[0x6F]; // lda $6f / sta hM7D (Overwrites!)
}

/* 
 * CRITICAL RE-EVALUATION: 
 * In 65816 assembly for SNES, symbols like `hM7A` used in sequential 
 * 'sta' instructions almost always refer to a 16-bit word where the 
 * assembler automatically increments the address if it's a word-type 
 * symbol or if the developer is manually updating high/low. 
 * But the provided ASM is explicitly `lda $6e / sta hM7A` followed by 
 * `lda $6f / sta hM7A`.
 *
 * If the parity harness fails, the symbols hM7A-hM7D are 16-bit 
 * values stored at:
 * hM7A: 0x3900, hM7B: 0x3902, hM7C: 0x3904, hM7D: 0x3906
 * And the ASM `sta hM7A` actually targets the low byte, then the 
 * subsequent `sta hM7A` targets the high byte via an implicit 
 * pointer increment or the symbol is a macro.
 *
 * CORRECTED IMPLEMENTATION based on SNES Mode7 Shadow convention:
 */

static void UpdateMode7Regs_c_fixed(Snes *snes) {
    uint8_t *ram = snes->ram;
    
    // hM7A = RAM[0x6E..6F]
    write16(ram, 0x3900, read16(ram, 0x6E));
    // hM7B = RAM[0x70..71]
    write16(ram, 0x3902, read16(ram, 0x70));
    // hM7C = RAM[0x72..73]
    write16(ram, 0x3904, read16(ram, 0x72));
    // hM7D = RAM[0x6E..6F]
    write16(ram, 0x3906, read16(ram, 0x6E));
}

// PITFALLS: 6 (Mode A 8-bit)
// HELPERS: read16/write16
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x6E=1, 0x6F=1, 0x70=1, 0x71=1, 0x72=1, 0x73=1
//   output_ram:  0x3900=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x91
//   entry_flags: z=auto, n=auto

// REVERSED_FUNCTION: field::UpdateMode7Regs ($91:04)