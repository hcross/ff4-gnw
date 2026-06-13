#include "snes/snes.h"

// ADR-003 delegate: cannot determine the absolute address of EventDlg1HPtrs
// (EventDlg1Ptrs + 512) without the full disassembly. The lda f:…,x
// instruction embeds a 24‑bit ROM address that is not recoverable from the
// snippet alone. Delegating the entire routine avoids guessing the table
// location.
void GetDlgPtr1H_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    cpu->db = 0x7E;
    cpu->dp = 0;
    cpu->mf = true;
    cpu->xf = false;
    run_emulated_func(snes, 0xB41B);
}

// PITFALLS: none (delegate)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  0x0772=2, 0xDD=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::GetDlgPtr1H ($00:B41B)