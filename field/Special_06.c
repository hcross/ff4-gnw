// ADR-003 delegate: tail-call target _d9e5 is a local label not exported
// to the symbol table; ca65-bridge cannot resolve it to a global address.
// Rather than guess the 24-bit target, delegate the entire routine.
// (classifier reasons: unresolved local jmp target)
#include "snes/snes.h"

void Special_06_c(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x7E;
    c->mf = true;      // A 8-bit (lda #imm in 8-bit context)
    c->xf = false;     // X/Y 16-bit (field module default)
    // No input registers needed — routine loads its own constants.
    // No flag pre-set needed — first instruction is lda, not a branch.
    run_emulated_func(snes, 0xD9D9D6u);
}

// PITFALLS: none (delegate)
// HELPERS: run_emulated_func (interpreter)
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  0x91=1 (written by the emulated lda/sta sequence)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::Special_06 ($D9:D6)