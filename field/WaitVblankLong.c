#include "snes/snes.h"

// ADR-003 delegate: routine contains a spin-wait loop on a memory-mapped
// register/variable (ram[0x7D]) that is updated by an asynchronous
// interrupt (VBlank). A C translation using a 'while' loop will hang 
// the parity harness because the harness executes the C function 
// atomically without stepping the emulator's interrupt logic.
//
// Classifier reasons: Infinite loop in C translation (detected by validator).
void WaitVblankLong_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x00;
    c->mf = true;
    c->xf = false;
    
    // The routine is a synchronization primitive. To maintain parity,
    // it must be executed by the emulator to allow the VBlank interrupt
    // to fire and modify ram[0x7D].
    run_emulated_func(snes, 0x00912Du);
}

// DELEGATED_FUNCTION: field::WaitVblankLong ($00:912D)