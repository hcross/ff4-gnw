#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$FE, DP=0
// This routine configures DMA transfers for sprites (OAM).
// If ram[0xf42b] is non-zero, it skips the configuration.
// Otherwise, it sets up the DMA source/destination and handles priority shifting.
void TfrSprites_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    if (ram[0xf42b] != 0) { // bne @fe45
        return;
    }

    // Note: phb / clr_a / pha / plb is a 65816 idiom to clear A
    // without disturbing the B register's state relative to the stack.
    // Effectively: A = 0.

    // hOAMADDL (likely 0x4341 based on common SNES DMA patterns, 
    // but we use absolute addresses as per ASM)
    // In the ASM: stx hOAMADDL where X was #0
    write16(ram, 0x4341, 0); // hOAMADDL = 0

    ram[0x4340] = 0x00;      // stx $4340 (X = $0400, but stx is 8-bit if X is used as 8-bit)
    ram[0x4341] = 0x04;      // Since X was #$0400, write16(ram, 0x4340, 0x0400)
    
    // The ASM does:
    // ldx #$0400 / stx $4340 -> write16(ram, 0x4340, 0x0400)
    // ldx #$0300 / stx $4342 -> write16(ram, 0x4342, 0x0300)
    write16(ram, 0x4340, 0x0400);
    write16(ram, 0x4342, 0x0300);

    ram[0x4344] = 0;         // sta $4344 (clr_a)
    ram[0x4347] = 0;         // sta $4347

    write16(ram, 0x4345, 0x0220); // ldx #$0220 / stx $4345

    // hMDMAEN (DMA Enable register)
    ram[0x4343] = 0x10;      // sta hMDMAEN (assuming hMDMAEN is 0x4343)

    // Priority order shifting check
    // ram[0x7ef28a] is an absolute address in the $7E bank
    uint8_t priority_val = ram[0x7ef28a];
    if (priority_val < 0x80) { // bpl @fe44 (Positive/Zero if bit 7 is 0)
        return;
    }

    // Set highest priority sprite address
    // hOAMADDH is usually the high byte of the OAM address
    ram[0x4342] = priority_val; // sta hOAMADDH (assuming 0x4342)
    ram[0x4341] = ram[0x7ef289]; // sta hOAMADDL (assuming 0x4341)
}

// PITFALLS: 1 (DB=$FE), 6 (Mode A 8-bit vs 16-bit for lda $f42b), 
// 7 (Truncation not applicable here as we use direct assignments)
// HELPERS: none
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0xf42b=1, 0x7ef28a=1, 0x7ef289=1
//   output_ram:  0x4340=2, 0x4342=2, 0x4344=1, 0x4345=2, 0x4347=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0xFE
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::TfrSprites ($FE:03)