#include "snes/snes.h"

// ADR-003 delegate: routine depends on UpdateCtrlBattle_ext, 
// which the bridge cannot resolve to a translated helper.
// Since the C translation fails to compile due to the missing 
// declaration of the delegated routine, we must delegate the 
// entire function to the emulator to maintain parity and buildability.
void UpdateCtrl_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x01;      // Corrected DB based on bridge report $01:FDD0
    c->mf = true;      // Battle/Menu convention: A 8-bit
    c->xf = false;     // X/Y 16-bit
    
    // No registers are read at entry before the first branch (lda $f44a),
    // so we do not need to manually set c->a or flags here.
    
    run_emulated_func(snes, 0x01FDD0u);
}

DELEGATED_FUNCTION: menu::UpdateCtrl ($01:FDD0)