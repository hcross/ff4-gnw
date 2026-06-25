#include "snes/snes.h"

// Entry mode: inherited from caller (BattleMain uses mf=1/xf=0/db=$7E/dp=0).
// The contract "mf=0" was incorrect — the ASM at $03:8085 does not change mf.
// WaitVblank ($02:851E) uses INC/LDA on $1811 in 8-bit mode; forcing mf=0 here
// causes LDA $1811 to read 2 bytes (including $1812, the NMI re-entry guard),
// which prevents WaitVblank from ever exiting.
// DB=$7E is the only register that must be set; mf/xf are left as-is.
void ExecBtlGfx_c(Snes *snes) {
    snes->cpu->db = 0x7E;   // Data bank for WRAM (mf/xf inherited from caller)
    ExecBtlGfx_ext_emu(snes);  // jsl ExecBtlGfx_ext
}

// PITFALLS: 1 (DB must be $7E), 2 (mf must NOT be forced to 0 — WaitVblank is 8-bit)
// HELPERS: ExecBtlGfx_ext_emu(snes) — delegates ExecBtlGfx_ext @ $02:8003
// CONTRACT:
//   inputs_reg:  a=mode (8-bit), x=none, y=none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=inherited(1), xf=inherited, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: battle::ExecBtlGfx ($80:85)
