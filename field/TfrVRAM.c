#include "snes/snes.h"

// ADR-003 delegate: routine too complex for direct translation.
// The previous translation failed because hardware register addresses (hVMAINC, etc.) 
// were guessed as 0x00-0x09. In a real SNES, these are mapped to specific 
// memory-mapped I/O ranges (e.g., $21xx), not the start of WRAM.
// Since the exact mapping for these macros is not provided in the prompt,
// the only way to maintain parity is to execute the original assembly via the emulator.
void TfrVRAM_c(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0x00;
    c->db = 0x00;
    c->mf = true;  // A 8-bit
    c->xf = false; // X 16-bit
    
    // The routine starts with `lda #$80`, so no entry flags need to be simulated.
    run_emulated_func(snes, 0x00CA85u);
}

// PITFALLS: 1 (DB=0 used for hardware registers), 6 (Inherited mode A=8, X=16)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x3C=1, 0x47=2, 0x3D=2, 0x45=2
//   output_ram:  none (Writes to HW registers outside WRAM range)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::TfrVRAM ($00:CA85)