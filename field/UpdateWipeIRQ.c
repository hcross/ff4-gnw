#include "snes/snes.h"

// ADR-003 delegate: routine reads from ROM table WipeScanlineTbl whose
// address is not provided in the prompt; delegating to emulator to avoid
// guessing. The classifier originally returned "translate" but the missing
// table address makes a faithful C translation impossible without the
// exact ROM offset.
void UpdateWipeIRQ_c(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x7E;   // field module WRAM (lda $7f, $79, $b1, $80)
    c->mf = true;   // 8-bit A (lda/asl/adc all 8-bit)
    c->xf = true;   // 8-bit X/Y (sty hVTIMEL writes only low byte,
                    // avoiding unintended VTIMEH write)
    // The routine starts with lda $7f; bne. The emulator will execute
    // lda and set Z/N from the RAM value, so no pre-set needed.
    run_emulated_func(snes, 0x91CAu);
}
// DELEGATED_FUNCTION: field::UpdateWipeIRQ ($91:CA)