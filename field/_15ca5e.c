#include "snes/snes.h"

/* $15:CA5E — "transfer palettes to ppu"
 *
 * Original ASM (39 bytes):
 *   STZ $420B             MDMAEN clear
 *   STZ $2121             CGADD = 0 (write CGRAM from color index 0)
 *   LDA #$02 STA $4300    DMA0 ctrl  = word-step, A->B (CGDATA pair pattern)
 *   LDA #$22 STA $4301    DMA0 dest  = $2122 (CGDATA)
 *   LDA #$00 STA $4304    DMA0 src bank = $00 (WRAM mirror)
 *   LDX #$0CDB STX $4302  src low/high = $0CDB
 *   LDX #$0200 STX $4305  byte count = 0x200 (256 palette entries)
 *   LDA #$01 STA $420B    trigger MDMA channel 0
 *   RTL
 *
 * Effect: blast 512 bytes (256 RGB555 entries) of palette shadow from
 * $00:0CDB into PPU CGRAM via DMA channel 0 mode 2. On a real SNES the
 * DMA engine handles the address auto-increment on the CGDATA side
 * (writes alternate low byte / high byte of each color word as it
 * scans 256 entries).
 *
 * Direct port: bypass the LakeSnes DMA engine — `dma_startDma` triggers
 * `snes_syncCycles` which is incompatible with savestate-resumed APU
 * state and silently Hardfaulted on previous attempts (same crash mode
 * observed with _15cadc). Two B-bus writes per color, CGADD auto-increments.
 */
void _15ca5e_c(Snes *snes) {
    /* CGADD = 0 — reset the CGRAM write index to entry 0. */
    snes_writeBBus(snes, 0x21, 0x00);  /* $2121 CGADD */

    /* Stream 0x200 bytes from WRAM $00:0CDB to CGDATA. Each pair of
     * bytes is one RGB555 color; CGDATA auto-toggles low/high and
     * auto-increments the index after the high byte write. */
    const uint8_t *src = &snes->ram[0x0CDB];
    for (int i = 0; i < 0x0200; i++) {
        snes_writeBBus(snes, 0x22, src[i]);  /* $2122 CGDATA */
    }
}

/* PITFALLS: 6 (16-bit/8-bit write split), 8 (DMA mid-frame)
 * HELPERS: snes_writeBBus (B-bus 0x21 = CGADD, 0x22 = CGDATA)
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  $00:0CDB..$00:0EDA (palette shadow, 512 bytes)
 *   output_ram:  none (writes PPU CGRAM)
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_15ca5e ($15:CA5E) — "transfer palettes to ppu"
 */
