#include "snes/snes.h"

// field::TfrSprites ($FE:03) — refresh OAM from the $0300 shadow buffer.
//
// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$FE, DP=0.
// Skips if ram[$F42B] != 0. Otherwise transfers the 544-byte OAM shadow at
// $00:0300 to PPU OAM, then optionally applies the sprite priority rotation.
//
// This is the SAME OAM blast as field::_15cadc ($CA:DC). Like that routine, the
// transfer is a manual $2104 (OAMDATA) loop rather than the original's DMA-
// channel-0 setup + MDMAEN trigger: the LakeSnes DMA path (dma_startDma ->
// snes_syncCycles) is incompatible with savestate-resumed APU state and
// hardfaults on device (see desktop/KNOWN_FINDINGS.md F3). The PPU auto-
// increments the OAM word address on each $2104 write, so the loop is
// equivalent to a DMA mode-0 transfer over 0x220 bytes.
//
// NOTE: the previous port wrote the DMA params into snes->ram[0x4340..0x4347]
// (WRAM $7E:4340) — those are CPU I/O DMA registers, NOT WRAM — so it both
// corrupted WRAM and never ran the transfer; it also read ram[0x7ef28a], a
// 128 KB-out-of-bounds index ($7E:F28A is WRAM offset 0xF28A). Both fixed here
// (desktop oracle finding F6).
void TfrSprites_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    if (ram[0xF42B] != 0) {  // bne @fe45 — caller asked to skip the refresh
        /* Cycle budget for the early-exit path: JMP($FE03→body) + LDA_abs(32) +
         * BNE_taken(22) + RTL(50) − RTL_dispatch_sim(24) ≈ 104 MC.  No
         * empirical measurement yet; use 104 as an analytic estimate. */
        snes_runCycles(snes, 104);
        return;
    }

    /* Cycle budget for the full OAM-transfer path: 5146 MC measured empirically
     * at two independent synchronisation points (TRACE 14FD03 and 02BB1A) by
     * comparing passe-A-dispatch vs passe-A-interp-TfrSprites runs. */
    snes_runCycles(snes, 5146);

    // Reset the OAM word address, then stream the 544-byte shadow to OAMDATA.
    snes_writeBBus(snes, 0x02, 0x00);  // $2102 OAMADDL = 0
    snes_writeBBus(snes, 0x03, 0x00);  // $2103 OAMADDH = 0
    const uint8_t *src = &ram[0x0300];
    for (int i = 0; i < 0x0220; i++) {
        snes_writeBBus(snes, 0x04, src[i]);  // $2104 OAMDATA (auto-increments)
    }

    // Priority-order shifting: if the high mirror byte has bit 7 set, repoint
    // the OAM address at the rotated sprite ($7E:F289/F28A hold OAMADDL/H).
    uint8_t pr_hi = ram[0xF28A];
    if (pr_hi < 0x80) {  // bpl @fe44
        return;
    }
    snes_writeBBus(snes, 0x03, pr_hi);        // $2103 OAMADDH
    snes_writeBBus(snes, 0x02, ram[0xF289]);  // $2102 OAMADDL
}

// PITFALLS: 1 (DB=$FE), 6 (A 8-bit vs X 16-bit), 8 (DMA mid-frame -> manual loop)
// HELPERS: snes_writeBBus (core LakeSnes API)
// CONTRACT:
//   inputs_reg:  none (writes constants / shadow bytes)
//   inputs_ram:  $00:0300..$00:051F (OAM shadow, 544 B); $7E:F42B (skip flag);
//                $7E:F289/F28A (priority rotation OAM address)
//   output_ram:  none (writes PPU OAM directly via $2104/$2102/$2103)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0xFE
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::TfrSprites ($FE:03)
