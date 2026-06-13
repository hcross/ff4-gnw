#include "snes/snes.h"

// ADR-003 delegate: routine contains a spin-lock on memory ($7E) 
// that depends on hardware/interrupt emulation (Vblank) to clear.
// The previous C translation produced an infinite loop in the 
// parity harness because the harness does not emulate the 
// asynchronous hardware state change of the Vblank flag.
static void WaitVblankShort_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x00;
    c->mf = true;
    c->xf = false;
    
    // PC is $00:913C per ca65-bridge
    run_emulated_func(snes, 0x00913Cu);
}

// DELEGATED_FUNCTION: field::WaitVblankShort ($00:913C)