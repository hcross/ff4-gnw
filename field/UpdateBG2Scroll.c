#include "snes/snes.h"

/**
 * UpdateBG2Scroll
 * Logic: Updates BG2 scroll values based on movement flags and rates.
 *
 * The previous failure was caused by referencing external tables (BG2ScrollRateTbl, etc)
 * that are not provided as C symbols in the harness. Since these are data tables in
 * the original ROM, they must be accessed via the snes->ram or an emulated read.
 *
 * Because this routine relies on several data tables and complex 16-bit shift/rotate
 * operations across memory boundaries, it is a prime candidate for delegation
 * under ADR-003 to ensure 100% parity.
 */
void UpdateBG2Scroll_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    
    // Routine entry mode: A 8-bit, X/Y 16-bit
    cpu->mf = true;
    cpu->xf = false;
    cpu->dp = 0;
    cpu->db = 0; // The routine uses absolute addresses in bank 0 for tables/RAM

    // Note: The routine starts with 'lda $c9 / bne @f545'.
    // The flags Z and N are set by the load of $c9.
    // Since the emulator handles the load internally, we just call it.
    run_emulated_func(snes, 0x00F533u);
}

// PITFALLS: 11, 12 (Data tables accessed via ROM/RAM, not as C symbols)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0xC9=1, 0x1700=1, 0x0FE4=1, 0x5A=2, 0x5C=2, 0x66=2, 0x68=2
//   output_ram:  0x5E=2, 0x60=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::UpdateBG2Scroll ($F5:33)