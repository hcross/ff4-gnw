#include "snes/snes.h"

// ADR-003 delegate: routine uses absolute memory access $7F4C00,x
// which is outside the snes->ram (WRAM) range and requires
// the emulator's full memory map to resolve correctly.
// (classifier reasons: outside WRAM access, implicit function call)
void CheckNPCMap1_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    
    // The routine uses Direct Page relative addressing ($0C, $0E)
    // and relies on the state of the DP register.
    // The Battle/Field module convention typically uses DP=0.
    c->dp = 0; 
    c->db = 0xC3;
    c->mf = true;   // A is 8-bit
    c->xf = false;  // X/Y are 16-bit
    
    // The routine does not start with a conditional branch based on 
    // inherited flags, so no Z/N setup is required here.
    
    run_emulated_func(snes, 0xC30B);
}

// DELEGATED_FUNCTION: field::CheckNPCMap1 ($C3:0B)