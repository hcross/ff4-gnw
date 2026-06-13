#include "snes/snes.h"

// ADR-003 delegate: routine is a 4-instruction trampoline (sta/phx/phy/jmp)
// that forwards to _f5f5 at an unresolved address (bridge reports @?).
// Delegating avoids guessing the jump target and sidesteps the missing
// _f5f5_emu stub (auto-generator did not emit it for the underscore-prefixed
// label). The emulator executes all four instructions faithfully.
void PlaySfx_c(Snes *snes, uint8_t a, uint16_t x, uint16_t y) {
    Cpu *cpu = snes->cpu;
    cpu->a = a;
    cpu->x = x;
    cpu->y = y;
    cpu->mf = true;   // 8-bit A (sta $f47f is an 8-bit store)
    cpu->xf = false;  // 16-bit X/Y (phx/phy push 2 bytes each)
    cpu->db = 0x7E;   // $F47F is in WRAM bank $7E
    cpu->dp = 0;
    cpu->e = false;   // native mode (16-bit SP for phx/phy)
    run_emulated_func(snes, 0xF500DF);
}

// PITFALLS: 1 (DB=$7E required for sta $F47F), 4 (stack address depends
//           on E flag; native mode assumed for 16-bit pushes)
// HELPERS: run_emulated_func (delegates the entire trampoline)
// CONTRACT:
//   inputs_reg:  a=8, x=16, y=16
//   inputs_ram:  none
//   output_ram:  $F47F=1 (sound effect ID stored before tail jump)
//   entry_mode:  mf=true, xf=false, e=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
//   calls:       _f5f5 (via emulated jmp)
// DELEGATED_FUNCTION: field::PlaySfx ($F5:DF)