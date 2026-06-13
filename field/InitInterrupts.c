#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$89, DP=0
// Logic:
// This routine initializes the NMI and IRQ jump vectors.
// It writes a JMP instruction (opcode $5C) followed by the 
// 16-bit address of the field-specific handlers.
void InitInterrupts_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // JmpNMI vector setup
    // JmpNMI is typically a constant address in the vector table
    // Based on the asm, we write the opcode $5C (JML) and the address of FieldNMI
    ram[0x0000] = 0x5C; // JmpNMI (opcode)
    write16(ram, 0x0001, 0x89ED); // FieldNMI address (placeholder - see REVERSED_FUNCTION)
    ram[0x0003] = 0;    // stz JmpNMI+3

    // JmpIRQ vector setup
    ram[0x0004] = 0x5C; // JmpIRQ (opcode)
    write16(ram, 0x0005, 0x89ED); // FieldIRQ address (placeholder)
    ram[0x0007] = 0;    // stz JmpIRQ+3
}

/* 
 * NOTE: The ASM refers to labels JmpNMI, JmpIRQ, FieldNMI, and FieldIRQ.
 * In a real system, these are fixed addresses. The C implementation 
 * must target the specific memory locations defined by those labels.
 * Assuming standard SNES vector layout for this module's specific logic.
 */

// PITFALLS: None relevant for this direct memory assignment.
// HELPERS: none
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  0x0000=1 // Vector table writes
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x89
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::InitInterrupts ($89:00ED)