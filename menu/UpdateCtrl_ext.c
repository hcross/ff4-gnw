#include "snes/snes.h"

// This routine is a simple jump wrapper that redirects execution to UpdateCtrl.
// It preserves all registers and flags.
void UpdateCtrl_ext_c(Snes *snes) {
    update_ctrl_emu(snes);
}

// PITFALLS: None.
// HELPERS: update_ctrl_emu(snes) — delegates UpdateCtrl @ $FD:D0
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0037, db=0x7E
//   entry_flags: z=auto, n=auto
// ENTRY_STATE: entry_mode corrected 2026-10-10 from the observed game entry (registry/ENTRY_STATE.md).
// REVERSED_FUNCTION: menu::UpdateCtrl_ext ($FD:03)