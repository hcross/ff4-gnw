#include "snes/snes.h"

/**
 * Logic:
 * Calculates vehicle sprite position based on zoom level.
 * Due to heavy dependence on external tables (f:_14f380, f:_14f000, f:_14f3a1, f:_14f1c0)
 * and specific 65816-mode flag behavior across multiple branches, this routine is 
 * delegated to the emulator to ensure parity and avoid table definition errors.
 */
void CalcVehicleSpritePos_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    
    // Set Data Bank to $BE (Field module)
    cpu->db = 0xBE;
    cpu->dp = 0;
    
    // Inherited mode from field module: A 8-bit, X/Y 16-bit
    cpu->mf = true;
    cpu->xf = false;

    // The routine is complex, accesses multiple external memory tables,
    // and uses specific shift/rotate sequences. Delegating to LakeSnes.
    run_emulated_func(snes, 0xBE47u);
}

// ADR-003 delegate: routine too complex for direct translation
// (classifier reasons: instr_count > 50, external table dependencies, 
// complex flag/register state across multiple branches)
// HELPERS: run_emulated_func(snes, 0xBE47u)
// CONTRACT:
//   inputs_reg: a=none, x=none, y=none
//   inputs_ram: 0xAD=1, 0x0C=1, 0x0D=1, 0x0E=1, 0x0F=1, 0x5A=1, 0x5C=1, 0x1706=1, 0x1707=1
//   output_ram: 0x0C=1, 0x0D=1, 0x0E=1, 0x0F=1, 0x0A=1, 0xD7=1
//   entry_mode: mf=true, xf=false, dp=0x0, db=0xBE
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::CalcVehicleSpritePos ($BE:47)