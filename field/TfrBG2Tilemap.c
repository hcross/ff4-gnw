#include "snes/snes.h"

/* TfrBG2Tilemap ($16:FB93) — two DMA passes to VRAM.
 * $2116/$2117 are PPU registers (bus B), $420B/$4302-$4306 are DMA registers.
 * Must go through snes_write() — direct ram[] writes miss the hardware registers. */
void TfrBG2Tilemap_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    uint16_t src;

    src = read16(ram, 0x99);                    /* ldx $99 — VRAM dest addr */
    snes_write(snes, 0x002116, (uint8_t)(src & 0xFF));
    snes_write(snes, 0x002117, (uint8_t)((src >> 8) & 0xFF));
    snes_write(snes, 0x00420B, 0);              /* stz $420B — disarm before re-arm */
    snes_write(snes, 0x004302, 0xDB);           /* ldx #$0ADB — DMA ch0 src lo */
    snes_write(snes, 0x004303, 0x0A);           /* DMA ch0 src hi */
    snes_write(snes, 0x004305, 0x40);           /* ldx #$0040 — DMA ch0 size lo */
    snes_write(snes, 0x004306, 0x00);           /* DMA ch0 size hi */
    ExecDMA_emu(snes);                          /* jsr ExecDMA — sta $420B=1 → DMA go */

    src = read16(ram, 0x9D);
    snes_write(snes, 0x002116, (uint8_t)(src & 0xFF));
    snes_write(snes, 0x002117, (uint8_t)((src >> 8) & 0xFF));
    snes_write(snes, 0x00420B, 0);
    snes_write(snes, 0x004302, 0x1B);           /* ldx #$0B1B — DMA ch0 src lo */
    snes_write(snes, 0x004303, 0x0B);
    snes_write(snes, 0x004305, 0x40);
    snes_write(snes, 0x004306, 0x00);
    ExecDMA_emu(snes);
}

// PITFALLS: 1 (DB=$7E assumed for WRAM sources $99 and $9D).
// HELPERS: ExecDMA_emu(snes) — delegates ExecDMA @ $8B36
//
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x99=2, 0x9D=2
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::TfrBG2Tilemap ($FB:93)