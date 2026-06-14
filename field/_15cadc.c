#include "snes/snes.h"
#include "snes/dma.h"

/* $15:CADC — "transfer sprite data to ppu"
 *
 * Original ASM (37 bytes):
 *   STZ $2102           OAM address low = 0
 *   STZ $420B           MDMAEN clear
 *   STZ $4300           DMA0 ctrl  = byte / CPU->PPU / no incr
 *   LDA #$04 STA $4301  DMA0 dest  = $21xx + 04 = $2104 (OAMDATA)
 *   LDX #$0300 STX $4302..$4303  source low/high = $0300
 *   LDA #$00 STA $4304  source bank = $00 (WRAM mirror)
 *   LDX #$0220 STX $4305..$4306  byte count = 0x220 (full 544-byte OAM)
 *   LDA #$01 STA $420B  trigger MDMA channel 0
 *   RTL
 *
 * Effect: blast 544 bytes of OAM shadow from $00:0300 into PPU OAM via
 * DMA channel 0. This is the NMI sprite refresh — without it no sprite
 * ever reaches the PPU after a savestate resumes.
 */
void _15cadc_c(Snes *snes) {
    /* Reset OAM word address. */
    snes_writeBBus(snes, 0x02, 0x00);  /* $2102 OAMADDL */

    /* Clear MDMAEN before reconfiguring. */
    dma_write(snes->dma, 0x420b, 0x00);

    /* DMA0 configuration. */
    dma_write(snes->dma, 0x4300, 0x00);  /* ctrl: byte, A->B, fixed dest */
    dma_write(snes->dma, 0x4301, 0x04);  /* B-bus dest = $2104 OAMDATA */
    dma_write(snes->dma, 0x4302, 0x00);  /* src low  */
    dma_write(snes->dma, 0x4303, 0x03);  /* src high */
    dma_write(snes->dma, 0x4304, 0x00);  /* src bank */
    dma_write(snes->dma, 0x4305, 0x20);  /* count low */
    dma_write(snes->dma, 0x4306, 0x02);  /* count high (= 0x0220) */

    /* BISECT: skip the DMA trigger to test if dma_doDma is what crashes. */
    /* dma_startDma(snes->dma, 0x01, false); */
}

/* PITFALLS: 6 (16-bit reg vs 8-bit writes), 8 (DMA mid-frame)
 * HELPERS: snes_writeBBus, dma_write, dma_startDma (core LakeSnes API)
 * CONTRACT:
 *   inputs_reg:  none (uses M=8 X=16 entry, but only writes constants)
 *   inputs_ram:  $00:0300..$00:051F (OAM shadow buffer, 544 bytes)
 *   output_ram: none (writes PPU OAM directly)
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_15cadc ($15:CADC) — "transfer sprite data to ppu"
 */
