#include "snes/snes.h"

// ADR-003 delegate: routine reads from ROM table AnimSineTbl whose
// absolute address is not provided in the translation context.
// Without the table base, a faithful C translation is impossible;
// the classifier should have delegated.  We emit a thin wrapper
// that runs the original asm under the emulator.
static void CalcSine_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;          // direct page zero (field convention)
    c->db = 0x7E;       // data bank = WRAM (absolute $26/$2b)
    c->mf = true;       // A 8-bit (inherited from caller)
    c->xf = false;      // X/Y 16-bit (inherited from caller)
    // The caller has already placed the angle index in A and set
    // the multiplicand in $27.  The routine starts with `tax` and
    // then `lda f:AnimSineTbl,x` – we let the interpreter handle
    // the table read and the rest of the logic.
    run_emulated_func(snes, 0xE79A);
    // On return, $2b holds the 8-bit product and the carry flag
    // reflects the sign outcome (C=1 for negative, C=0 for positive).
    // The caller reads those directly from CPU state / RAM.
}

// PITFALLS: none (delegated)
// HELPERS: run_emulated_func (interpreter)
// CONTRACT:
//   inputs_reg:  a=<index>, x=don't care, y=don't care
//   inputs_ram:  0x27=<multiplicand>
//   output_ram:  0x2B=<product>
//   output_flags: c=<sign indicator>
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::CalcSine ($E7:9A)