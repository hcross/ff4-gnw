#include "snes/snes.h"

// ADR-003 delegate: routine uses absolute indexed reads from data tables
// (VehicleSongTbl, WorldSongTbl) whose ROM addresses are not available in
// this translation unit.  Delegating to the emulator preserves correctness.
void PlayMapSong_c(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x8D;          // routine resides in bank $8D, DB must match
    c->mf = true;          // A 8-bit
    c->xf = false;         // X/Y 16-bit
    // Flags are set by the first lda $1704 inside the routine; no need to preset.
    run_emulated_func(snes, 0x8D5D);
}

// PITFALLS: none (delegated)
// HELPERS: run_emulated_func (interpreter)
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x1704=1, 0x1700=1, 0x0FE2=1
//   output_ram:  0x1E01=1, 0x1E00=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x8D
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::PlayMapSong ($8D:5D)