#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$8B, DP=0
// Logic: Evaluates battle triggers for different map types.
// Since the routine heavily relies on external data tables (RNGTbl, BattleRates, etc.)
// that are not mapped to the simple WRAM array, we use the LakeSnes emulator for
// data access consistency and the complex branch logic.
// 
// ADR-003: Delegating this routine because it accesses multiple ROM tables 
// (OverworldBattleRate, RNGTbl, etc.) that are not provided as C arrays in the harness.
void CheckBattle_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    
    // Setup CPU state to match battle/field module entry conventions
    cpu->dp = 0;
    cpu->db = 0x8B;
    cpu->mf = true;   // A 8-bit
    cpu->xf = false;  // X/Y 16-bit
    
    // The routine starts with 'lda $1704', so no specific register inputs 
    // are required for the first branch.
    
    run_emulated_func(snes, 0x8B3Cu);
}

// PITFALLS: 1 (DB set to 0x8B), 8 (Inherited mf=true)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x1704=1, 0xA2=1, 0xC0=1, 0xAB=1, 0xD5=1, 0x1700=1, 0x1707=1, 0x1706=1, 0x17EF=1, 0x1702=1, 0x1701=1
//   output_ram:  0x85=1
//   entry_mode:  mf=true, xf=false, dp=0, db=0x8B
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::CheckBattle ($8B:3C)