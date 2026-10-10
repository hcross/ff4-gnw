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
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x00
//   entry_flags: z=auto, n=auto
// ENTRY_STATE: entry_mode corrected 2026-10-10 from the observed game entry (registry/ENTRY_STATE.md).
// REVERSED_FUNCTION: menu::UpdateCtrlField_ext ($80:10)