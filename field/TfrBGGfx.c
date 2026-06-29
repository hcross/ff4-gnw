#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$B1 (ROM), DP=0
// Logic:
// This routine performs a high-speed DMA transfer of BG graphics to VRAM.
// It iterates 384 times (@b151), each loop performing two MDMA bursts:
// 1. A burst with a 0x10 (16 byte) increment.
// 2. A burst with a 0x08 (8 byte) increment.
// This pattern suggests it is filling VRAM tiles by alternating offsets.
void TfrBGGfx_c(Snes *snes) {
    // $4300-$4307 are CPU DMA-channel-0 I/O registers, and hVMADDL/hVMAINC/
    // hMDMAEN are PPU/CPU registers ($2116/$2115/$420B) — all MMIO, reached
    // through the bus, NOT the WRAM array. The original port hallucinated WRAM
    // offsets (comments said "assuming 0x43xx"), so the DMA was never programmed
    // nor triggered (the $420B MDMAEN write that fires the transfer went to WRAM)
    // → BG tiles never reached VRAM → garbled tilemap. The DMA *source* ($4302-4,
    // A1T) is set by the caller and intentionally left intact here.

    // Manual VRAM transfer — same approach as TfrSprites_c (F6). A DMA triggered
    // from dispatched C does NOT flush on the isolated harness (dma_startDma only
    // sets dmaState; the transfer needs CPU cycles), so replicate the two-burst
    // DMA with direct $2118/$2119 (VMDATA) writes, which take effect immediately.
    // The source gfx is pre-loaded into DMA channel 0's A-bus address by the
    // caller (ROM, bank $B1); the original port wrote the DMA params into WRAM
    // ($7E:43xx) and never transferred anything → garbled tilemap.
    DmaChannel *ch = &snes->dma->channel[0];
    uint32_t src = ((uint32_t)ch->aBank << 16) | ch->aAdr;

    snes_writeBBus(snes, 0x16, 0x00);   // $2116 VMADDL = $0000
    snes_writeBBus(snes, 0x17, 0x00);   // $2117 VMADDH

    for (int t = 0; t < 0x180; t++) {   // 384 3bpp tiles (24 B each) → VRAM $0000+
        // Burst 1: 16 B as words — DMAP=1 ($2118 then $2119), VMAIN=$80 (inc after $2119)
        snes_writeBBus(snes, 0x15, 0x80);
        for (int i = 0; i < 16; i++)
            snes_writeBBus(snes, (uint8_t)(0x18 + (i & 1)), snes_read(snes, src++));
        // Burst 2: 8 B low-byte only — DMAP=0 ($2118), VMAIN=$00 (inc after $2118)
        snes_writeBBus(snes, 0x15, 0x00);
        for (int i = 0; i < 8; i++)
            snes_writeBBus(snes, 0x18, snes_read(snes, src++));
    }

    // Advance channel 0's A-bus address as the real transfers would have.
    ch->aAdr  = (uint16_t)(src & 0xFFFF);
    ch->aBank = (uint8_t)((src >> 16) & 0xFF);
}

// PITFALLS: 1 (DB=$B1, though these are IO writes to $43xx range), 
// 6 (Sized mode: A is 8-bit, Y is 8-bit for the loop counter).
// HELPERS: none
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  0x4300=1, 0x4301=1, 0x4305=1, 0x4307=1 (volatile IO)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0xB1
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::TfrBGGfx ($B1:43)