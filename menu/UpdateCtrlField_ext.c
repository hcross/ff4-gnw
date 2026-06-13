#include "snes/snes.h"

// menu::UpdateCtrlField_ext ($80:10) — thin wrapper that delegates to
// UpdateCtrlField and returns via RTL. No register or flag manipulation
// at this level; the callee handles all logic.
void UpdateCtrlField_ext_c(Snes *snes) {
    update_ctrl_field_emu(snes);
}

// PITFALLS: none
// HELPERS: update_ctrl_field_emu(snes) — delegates UpdateCtrlField @ $80:5E
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=auto, xf=auto, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: menu::UpdateCtrlField_ext ($80:10)