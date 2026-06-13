#include "snes/snes.h"

// ADR-003 delegate: routine contains a jump to an anonymous label (_d9e5).
// Since the previous translation attempt using run_emulated_func for the 
// jump target caused ram_diverge, the most reliable approach to ensure 
// full parity (including precise CPU state/flag transitions at the jump 
// boundary) is to delegate the entire routine to the emulator.
void Special_06_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    cpu->dp = 0;
    cpu->db = 0x00;
    cpu->mf = true;
    cpu->xf = false;

    // Target: field::Special_06 ($00:D9D6)
    run_emulated_func(snes, 0x00D9D6u);
}

// PITFALLS: 1 (DB=0x00 based on bridge report $00:D9D6)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  0x91=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
//   entry_flags: z=auto, n=auto

// DELEGATED_FUNCTION: field::Special_06 ($00:D9D6)