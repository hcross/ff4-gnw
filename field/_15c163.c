#include "snes/snes.h"
#include "snes/dma.h"

/* $15:C163 - "update mode 7 zoom hdma table"
 *
 * Builds a small HDMA control table in WRAM at $7F:5A00..$7F:5A06 that
 * drives Mode 7 matrix elements M7B ($211C) and M7C ($211D) over the
 * 224 visible scanlines, then configures DMA channels 2/3 to pull from
 * that table on each scanline. Used for the zoom in/out effect on the
 * world map (and the title screen, which presents the crystal over a
 * Mode 7 backdrop with $1700==3 set at boot).
 *
 * Original logic:
 *   if (map_id != 3) RTL
 *   M7B = 0,  M7C = 0
 *   table[0] = table[3] = $F0       (224 scanlines for each segment)
 *   vehicle 6 (big whale): v = ((player_y - 16) & 0xFE) + 0x22
 *   else                 : v = (player_y - 16) << 1
 *   table[2] = table[5] = v          (zoom scale per segment)
 *   table[1] = table[4] = $00 / $E0  (segment splits)
 *   table[6] = $80                   (HDMA terminator)
 *   $420C = 0                        (disable HDMA temporarily)
 *   configure DMA chans 2 and 3 to feed M7B / M7D from table
 *   RTL
 *
 * G&W port notes:
 *   - PPU writes (M7B/M7C) go via snes_writeBBus - safe and immediate.
 *   - WRAM writes to $7F:5A00 area land in snes->ram[0x15A00] (bank $7F
 *     starts at offset 0x10000 in the 128 KB RAM array).
 *   - HDMA register configuration ($4340-$4357) goes via dma_write,
 *     which only updates the channel struct without triggering the
 *     snes_syncCycles cascade that crashes post-savestate.
 *   - The $420C=0 step (disable HDMA) is replicated by clearing
 *     hdmaActive directly on every channel; we skip dma_startDma to
 *     avoid the catchup. HDMA re-enable happens elsewhere in the FF4
 *     boot sequence, not here.
 */
void _15c163_c(Snes *snes) {
    uint8_t map_id = snes->ram[0x1700];
    if (map_id != 3) {
        return;  /* early-out for title-pre-init / battle / dialog */
    }

    /* Reset mode 7 matrix B and C (off-diagonal - disables rotation/shear).
     * Two writes per register because M7x latches the low byte then the
     * high byte on alternating writes. */
    snes_writeBBus(snes, 0x1C, 0x00);
    snes_writeBBus(snes, 0x1C, 0x00);
    snes_writeBBus(snes, 0x1D, 0x00);
    snes_writeBBus(snes, 0x1D, 0x00);

    /* 224-line segments at both halves of the HDMA table. */
    snes->ram[0x15A00] = 0xF0;
    snes->ram[0x15A03] = 0xF0;

    /* Zoom scale computed from player Y position ($AD) and vehicle ($1704).
     * Vehicle 6 == big whale uses a different bias. */
    uint8_t vehicle = snes->ram[0x1704];
    uint8_t pos_a   = snes->ram[0xAD];
    uint8_t v;
    if (vehicle == 0x06) {
        v = (uint8_t)(((pos_a - 0x10) & 0xFE) + 0x22);
    } else {
        v = (uint8_t)((pos_a - 0x10) << 1);
    }
    snes->ram[0x15A02] = v;
    snes->ram[0x15A05] = v;
    snes->ram[0x15A01] = 0x00;
    snes->ram[0x15A04] = 0xE0;

    /* HDMA terminator. */
    snes->ram[0x15A06] = 0x80;

    /* Disable HDMA - direct hdmaActive clear (skip dma_startDma cycle sync). */
    for (int i = 0; i < 8; i++) {
        snes->dma->channel[i].hdmaActive = false;
    }

    /* DMA channel 2 - feeds $211B (M7A top-left). Mode $42: 2 bytes per
     * write, fixed B-bus, indirect. */
    dma_write(snes->dma, 0x4340, 0x42);
    dma_write(snes->dma, 0x4341, 0x1B);
    dma_write(snes->dma, 0x4342, 0x00);
    dma_write(snes->dma, 0x4343, 0x5A);
    dma_write(snes->dma, 0x4344, 0x7F);
    dma_write(snes->dma, 0x4347, 0x7F);

    /* DMA channel 3 - feeds $211E (M7D bottom-right). Same source table. */
    dma_write(snes->dma, 0x4350, 0x42);
    dma_write(snes->dma, 0x4351, 0x1E);
    dma_write(snes->dma, 0x4352, 0x00);
    dma_write(snes->dma, 0x4353, 0x5A);
    dma_write(snes->dma, 0x4354, 0x7F);
    dma_write(snes->dma, 0x4357, 0x7F);
}

/* PITFALLS: 6 (matrix latch pair writes), 8 (DMA setup without trigger),
 *           12 (WRAM bank $7F → snes->ram[0x10000+offs])
 * HELPERS: snes_writeBBus (B-bus M7B/M7C), dma_write (chan register
 *          updates), direct hdmaActive clear (HDMA disable)
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  $00:1700, $00:1704, $00:00AD
 *   output_ram:  $7F:5A00..$7F:5A06 (HDMA table), DMA chan 2 + 3 regs
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_15c163 ($15:C163) - "update mode 7 zoom hdma table"
 */
