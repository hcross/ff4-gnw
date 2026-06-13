#include "snes/snes.h"

// ADR-003: Routine contains table indexing (RedWingsIntroPos) and a loop 
// with a missing increment in the provided snippet, leading to 
// linker errors when attempting to reference ROM symbols directly 
// in C. To ensure 100% parity and avoid symbol resolution failures, 
// this routine is delegated to the emulator.

static void DrawRedWings_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x00; // As per the provided address $DB:71
    c->mf = true; // A 8-bit
    c->xf = false; // X/Y 16-bit
    
    // The routine does not start with a conditional branch based on 
    // entry registers, so Z/N flags are not explicitly set here.
    
    run_emulated_func(snes, 0x000000DB71u); // Corrected to use the full PC24
}

// For the parity harness, we provide the function with the required signature.
void DrawRedWings_c(Snes *snes) {
    DrawRedWings_emu(snes);
}

// PITFALLS: 11 (Data-only symbols like RedWingsIntroPos cannot be 
// easily linked in the spike harness without specific ROM offset headers),
// 12 (Ensuring correct address for the harness).
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
//   entry_flags: z=auto, n=auto

// DELEGATED_FUNCTION: field::DrawRedWings ($DB:71)