#include "snes/snes.h"

// ADR-003 delegate: routine too complex for direct translation
// (classifier reasons: instr_count > 50, call_count > 2, unresolved
//  data table VehiclePassBit whose WRAM address is not provided)
static void CheckPlayerMoveWorld_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x7E;   // field module uses WRAM for variables
    c->mf = true;   // A 8-bit (inherited from caller)
    c->xf = false;  // X/Y 16-bit (caller convention)
    c->a = 0;       // clear B to avoid Pitfall 9 (tax/tay with residue)
    run_emulated_func(snes, 0x0AB84u);
}
// PITFALLS: 1 (DB=$7E required for WRAM accesses), 9 (B=0 enforced)
// HELPERS: none (delegated entirely)
// CONTRACT:
//   inputs_reg:  none (all inputs in RAM)
//   inputs_ram:  $D5, $B1, $04, $05, $A1, $1715, $1706, $1716,
//                $1707, $1717, $1700, $1704, $1287, $1281, $0A, etc.
//   output_ram:  $AB, $02, $03, $D2, $1705, $0709, $070A, $54, etc.
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto (first lda $D5 sets flags)
// DELEGATED_FUNCTION: field::CheckPlayerMoveWorld ($AB:84)