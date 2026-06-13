#include "snes/snes.h"

// ADR-003 delegate: WaitSpecial contains a blocking VBlank-wait loop
// (jsr WaitVblankLong) that cannot be safely inlined into a C loop
// without the full frame-advance machinery of the emulator.  Running
// the original asm via run_emulated_func lets the harness handle the
// VBlank wait correctly.
void WaitSpecial_c(Snes *snes, uint16_t arg_x) {
    snes->cpu->x = arg_x;
    snes->cpu->db = 0x7E;
    snes->cpu->dp = 0;
    snes->cpu->mf = true;
    snes->cpu->xf = false;
    run_emulated_func(snes, 0xE1DE);
}

// PITFALLS: none (delegate)
// HELPERS: none
// CONTRACT:
//   inputs_reg:  x=16
//   inputs_ram:  none
//   output_ram:  0x89=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: none
// DELEGATED_FUNCTION: field::WaitSpecial ($00:E1DE)