#include "snes/snes.h"

// ADR-003 delegate: routine calls WaitVBlank, which polls PPU status
// and would spin forever in a spike harness. The whole function is
// therefore delegated to the asm interpreter.
void FadeIn_c(Snes *snes) {
    fadein_emu(snes);
}

// PITFALLS: none
// HELPERS: fadein_emu(snes) — delegates the original FadeIn @ $00:D718
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x3303=1 (initial value, set by caller)
//   output_ram:  0x3303=1, 0x9A=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::FadeIn ($00:D718)