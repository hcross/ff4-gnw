#include "snes/snes.h"

// ADR-003 delegate: routine contains ROM table lookups (XMoveTbl, YMoveTbl)
// whose exact addresses are not provided in the provided asm fragment.
// To maintain parity and avoid guessing ROM offsets, this is delegated to the emulator.
void SetPlayerNPCMap_c(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0xAA;
    c->mf = true;   // A 8-bit
    c->xf = false;  // X/Y 16-bit
    
    // The routine starts with 'lda $ab', so no entry flags (Z/N) are required
    // to be set by the caller for the first instruction.
    
    run_emulated_func(snes, 0xAA00D8u);
}

// DELEGATED_FUNCTION: field::SetPlayerNPCMap ($AA:D8)