#include "snes/snes.h"

void InitCharProp_ext_c(Snes *snes) {
    init_char_prop_emu(snes);
}
// PITFALLS: none
// HELPERS: init_char_prop_emu(snes) — delegates InitCharProp @ $FF:94EE
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::InitCharProp_ext ($FF:BC)