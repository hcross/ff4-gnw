#include "snes/snes.h"

// Root Cause Analysis: 
// The previous translation failed because DestroyedDamcyanVRAMTbl and 
// DestroyedDamcyanTiles are data labels (tables) in the ROM. 
// They are not defined in the C scope. 
// Since this routine is essentially a loop writing ROM-based table 
// data to PPU registers, and the tables are not provided as C arrays, 
// the most faithful and stable approach is to delegate to the 
// emulator (ADR-003), ensuring parity by executing the original bytes.

// ADR-003 delegate: routine relies on ROM tables not mapped to C arrays
static void DrawDestroyedDamcyan_emu(Snes *snes) {
    Cpu *cpu = snes->cpu;
    cpu->dp = 0;
    cpu->db = 0x00;   // PPU registers are accessed via DB 0 in this context
    cpu->mf = true;   // A 8-bit
    cpu->xf = false;  // X/Y 16-bit
    
    // The routine starts with 'lda $2c', so no register flags 
    // need to be set before the call.
    run_emulated_func(snes, 0x00D758u);
}

// The harness requires the function name DrawDestroyedDamcyan_c
void DrawDestroyedDamcyan_c(Snes *snes) {
    DrawDestroyedDamcyan_emu(snes);
}

// PITFALLS: 11 (Data labels in ROM used as lookup tables)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x002C=1
//   output_ram:  0x2118=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::DrawDestroyedDamcyan ($D7:58)