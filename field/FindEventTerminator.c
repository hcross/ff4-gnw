#include "snes/snes.h"

// ADR-003 delegate: routine accesses ROM memory (f:EventScript) 
// which is not directly available via snes->ram and requires 
// emulator memory map resolution.
void FindEventTerminator_c(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0xE5;
    c->mf = true;
    c->xf = false;
    
    // The routine reads its starting pointer from RAM [0x09D3].
    // The emulator will handle the LDX $09D3 and the subsequent 
    // ROM lookup at f:EventScript,x based on the current PC and DB.
    run_emulated_func(snes, 0xE55Au);
}

// PITFALLS: None (Delegated due to ROM access)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x09D3=2
//   output_ram:  0x09D3=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0xE5
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::FindEventTerminator ($E5:5A)