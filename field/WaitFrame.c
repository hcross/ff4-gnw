#include "snes/snes.h"

// ADR-003 delegate: routine contains a call to ExecBtlGfx_ext which 
// was not resolved by the bridge, and the routine is primarily 
// a register-preservation wrapper for that call.
void WaitFrame_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x00;
    c->mf = true;
    c->xf = false;
    
    // The routine sets A = 0x0F before calling ExecBtlGfx_ext.
    // To ensure the emulator starts with the correct state for 
    // the internal JSL, we set the accumulator here.
    c->a = 0x0F;
    
    run_emulated_func(snes, 0x008513u);
}

// DELEGATED_FUNCTION: field::WaitFrame ($00:8513)