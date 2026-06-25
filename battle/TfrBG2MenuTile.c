#include "snes/snes.h"
#include "dispatch_all.h"

/* TfrBG2MenuTile — $02:90A0 (13 opcodes, M=8, X=16, DP=0)
 *
 * Transfers one BG2 menu tile graphic from ROM WindowGfx into VRAM
 * via DMA channel 5.  Called from LoadBG2MenuGfx's 37-iteration loop.
 *
 * A8           PHX                  ; save loop index (tile slot)
 * 85 26        STA $26              ; $26 = tile_id (Mult8 multiplicand)
 * A9 10        LDA #$10             ; 16 bytes per tile
 * 85 28        STA $28              ; $28 = 16 (Mult8 multiplier)
 * 20 60 85     JSR Mult8            ; $2A:$2B = tile_id × 16
 * C2 20        REP #$20             ; M=16
 * A5 2A        LDA $2A              ; product_lo  ← RACE TARGET (see below)
 * 18           CLC
 * 69 xx xx     ADC #.loword(WindowGfx)
 * AA           TAX                  ; X = src addr in ROM
 * A9 10 00     LDA #$0010           ; 16 bytes
 * 85 00        STA $00              ; DMA size
 * E2 30        SEP #$30             ; M=8, X=8 (but X restored by PLX)
 * A9 0A        LDA #^WindowGfx      ; bank $0A
 * 20 75 86     JSR TfrVRAM5         ; DMA ch5: src=X:A, dest=Y (VRAM), size=$00
 * FA           PLX                  ; restore loop index
 * 60           RTS
 *
 * RACE CONDITION (f11-f12 artefact):
 *   snes_runCycles inside Mult8_btlgfx_c can set cpu->nmiWanted = true (VBlank
 *   edge).  On the next cpu_runOpcode after Mult8 returns, cpu_checkInt fires
 *   and BattleNMI executes — which calls PeriodicMenuUpdate → LoadMenuTfrData →
 *   Mult8, clobbering $2A with (menu_index × 6) before "LDA $2A" above runs.
 *   The DMA then transfers from the wrong ROM address → catastrophic stripes.
 *
 * FIX: dispatch TfrBG2MenuTile as a C function so that the call to
 *   Mult8_btlgfx_c and the subsequent read of $2A are performed atomically
 *   within C code.  cpu_runOpcode is never called between these two steps,
 *   so nmiWanted cannot be promoted to intWanted and no interrupt can fire
 *   before $2A has been consumed. */

/* WindowGfx is at SNES ROM address $0A:F000 (from ff4-jp1.map). */
#define WINDOW_GFX_BANK  0x0Au
#define WINDOW_GFX_LO    0xF000u

void TfrBG2MenuTile_c(Snes *snes) {
    /* PHX (30 MC, X=16-bit): save tile-slot loop counter. */
    uint16_t saved_x = snes->cpu->x;

    /* STA $26 (24) + LDA #$10 (16) + STA $28 (24) = 64 MC
     * Preamble total with PHX: 94 MC; minus 16 MC dispatch pullWord = 78. */
    uint8_t tile_id = (uint8_t)(snes->cpu->a);
    snes->ram[0x26] = tile_id;
    snes->ram[0x28] = 0x10;
    snes_runCycles(snes, 78);

    /* Mult8_btlgfx_c internally does snes_runCycles(snes, cyc - 16), subtracting
     * 16 MC for its own dispatch overhead.  Since we call it directly (not via
     * dispatch), re-add those 16 MC to keep cycle accounting accurate. */
    snes_runCycles(snes, 16);

    /* Direct call — no cpu_runOpcode between here and the read of $2A below.
     * nmiWanted may be set to true inside snes_runCycles, but the NMI handler
     * only runs at the top of cpu_runOpcode (via cpu_doInterrupt), never inside
     * pure C code.  The sequence Mult8→read($2A) is therefore atomic. */
    Mult8_btlgfx_c(snes);

    /* Atomic read of $2A/$2B — $2A cannot have been clobbered yet. */
    uint8_t prod_lo = snes->ram[0x2A];
    uint8_t prod_hi = snes->ram[0x2B];

    /* REP #$20 (24) + LDA dp16 (24) + CLC (14) + ADC #imm16 (24) + TAX (14)
     * + LDA #imm16 (24) + STA dp16 (24) + SEP #$30 (24) + LDA #imm8 (16) = 188 MC */
    uint16_t product  = (uint16_t)prod_lo | ((uint16_t)prod_hi << 8);
    uint16_t src_addr = (uint16_t)(product + WINDOW_GFX_LO);
    snes->cpu->x    = src_addr;
    snes->ram[0x00] = 0x10;
    snes->ram[0x01] = 0x00;
    snes->cpu->a    = WINDOW_GFX_BANK;
    snes_runCycles(snes, 188);

    /* TfrVRAM5 ($02:8675) inlined via snes_write — no cpu_runOpcode, no NMI risk.
     *
     * The original TfrVRAM5 assembly:
     *   PHB, PHA, CLR_A, PHA, PLB, PLA   (save/restore DB; we skip this as we
     *   write registers directly)
     *   STY hVMADDL   → $2116/$2117 = Y (VRAM destination)
     *   STX $4352     → DMA ch5 source addr (lo/hi)
     *   STA $4354     → DMA ch5 source bank
     *   LDA #$01      → DMA ch5 mode (CPU→PPU, auto-increment B-bus)
     *   STA $4350
     *   LDA #<hVMDATAL → $18 (B-bus register = hVMDATAL at $2118)
     *   STA $4351
     *   LDX $00       → size from DP ($00)
     *   STX $4355
     *   LDA #$20      → ch5 bitmask
     *   STA hMDMAEN   → triggers DMA ch5
     *   PLB, RTS
     *
     * dma_startDma sets dmaState=1; the actual transfer runs on the next
     * dma_handleDma call (during the following cpu_runOpcode cycle). */
    uint16_t vram_dest = snes->cpu->y;
    snes_write(snes, 0x002116, (uint8_t)(vram_dest & 0xFF));
    snes_write(snes, 0x002117, (uint8_t)(vram_dest >> 8));
    snes_write(snes, 0x004352, (uint8_t)(src_addr & 0xFF));
    snes_write(snes, 0x004353, (uint8_t)(src_addr >> 8));
    snes_write(snes, 0x004354, WINDOW_GFX_BANK);
    snes_write(snes, 0x004350, 0x01u);
    snes_write(snes, 0x004351, 0x18u);
    {
        uint16_t sz = (uint16_t)snes->ram[0x00] | ((uint16_t)snes->ram[0x01] << 8);
        snes_write(snes, 0x004355, (uint8_t)(sz & 0xFF));
        snes_write(snes, 0x004356, (uint8_t)(sz >> 8));
    }
    snes_write(snes, 0x00420B, 0x20u);

    /* JSR TfrVRAM5 overhead (46) + TfrVRAM5 body (~502) + PLB/RTS (~70) ≈ 618 MC.
     * Approximation; G&W port is not cycle-accurate to the master-clock. */
    snes_runCycles(snes, 618);

    /* PLX (36 MC): restore loop index. */
    snes->cpu->x = saved_x;
    snes_runCycles(snes, 36);

    /* RTS: simulated by the dispatch hook (cpu_pullWord + 1). */
}

// CONTRACT TfrBG2MenuTile_c:
//   inputs_reg:  a=tile_id (8-bit, M=8), y=vram_dest (VRAM address word, set by LoadBG2MenuGfx: ldy $04)
//   inputs_ram:  none (tile_id comes from A, not RAM)
//   output_ram:  DMA ch5 configured and armed (transfer fires on next CPU cycle)
//   entry_mode:  mf=true, xf=false, dp=0
//
// RACE FIX: Mult8_btlgfx_c called directly — $2A read atomically before any
// cpu_runOpcode can trigger the NMI handler and clobber $2A.
// REVERSED_FUNCTION: btlgfx::TfrBG2MenuTile ($02:90A0)
