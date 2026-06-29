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

    snes_write(snes, 0x4301, 0x18);  // sta $4301  — DMA0 B-bus addr ($2118 VMDATA)
    snes_write(snes, 0x2116, 0x00);  // stx hVMADDL — VRAM address = $0000
    snes_write(snes, 0x2117, 0x00);

    for (uint16_t y = 0; y != 0x0180; y++) {   // 384 tiles → VRAM $0000-$17FF
        // Burst 1: 16 bytes
        snes_write(snes, 0x2115, 0x80);  // hVMAINC — increment after high byte
        snes_write(snes, 0x4300, 0x01);  // DMA0 DMAP — 2 regs write twice ($2118/9)
        snes_write(snes, 0x4305, 0x10);  // DMA0 DAS size lo = 0x0010
        snes_write(snes, 0x4306, 0x00);
        snes_write(snes, 0x420B, 0x01);  // hMDMAEN — trigger channel 0

        snes_write(snes, 0x420B, 0x00);  // stz hMDMAEN
        snes_write(snes, 0x2115, 0x00);  // stz hVMAINC
        snes_write(snes, 0x4300, 0x00);  // stz DMAP

        // Burst 2: 8 bytes
        snes_write(snes, 0x4305, 0x08);  // DAS size = 0x0008 (high byte still 0)
        snes_write(snes, 0x4306, 0x00);
        snes_write(snes, 0x420B, 0x01);  // trigger channel 0
    }
    // NOTE: this faithful register translation targets the real MMIO (vs the
    // original WRAM-offset hallucination), but a DMA triggered from dispatched C
    // does not flush on the isolated desktop harness (dma_startDma only sets
    // dmaState; the transfer needs CPU cycles to run — F6 class). On device,
    // where cycles flow continuously, it may run; unverified. The desktop host
    // therefore interprets this routine (host_exclude_divergent). A device-safe
    // native port likely needs an F6-style manual VRAM loop.
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