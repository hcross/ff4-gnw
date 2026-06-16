#include "snes/snes.h"
#include "ff4_helpers.h"
#include "dispatch_all.h"

/* field::_00883d ($00:883D) - load title screen bg graphics
 *
 * Original ASM (~10 instructions):
 *   LDX #$0000 / STX $47    VRAM dest 1 = $0000
 *   LDX #$2800 / STX $45    size = 0x2800 bytes
 *   LDA #$08   / STA $3C    source bank = $08
 *   LDX #$C000 / STX $3D    source offset = $C000
 *   JSL $15CA85             transfer first half
 *   LDX #$4000 / STX $47    VRAM dest 2 = $4000
 *   JSL $15CA85             transfer second half (continues from $3D)
 *   RTS
 *
 * Called from title screen init at $00:8634 (JSR $883D). This is THE
 * routine that fills the title screen VRAM with the FF4 logo + crystal
 * + copyright glyph tiles. Without it, the PPU has brightness=15 but
 * renders empty backdrop and the screen looks black.
 *
 * Two back-to-back transfers via _15ca85_c, only the destination
 * differs ($0000 then $4000). $3D auto-advances inside _15ca85_c via
 * the source-pointer reads, but here the original ASM does NOT
 * advance $3D between the two calls - the second transfer pulls bytes
 * from $08:C000 + 0 again. Mirror that exactly: reset $3D to $C000
 * before the second call. (Wait - actually re-reading the asm: $3D
 * is NOT touched between calls, so _15ca85 must be consuming from
 * the original $3D each time. But _15ca85 doesn't update $3D either,
 * so both calls start from the SAME source. That's the intended
 * behavior for double-banking a single 10 KB tile sheet across two
 * VRAM pages.) */
void _00883d_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    /* The transfer params ($3C..$48) are DIRECT-PAGE locations: the original
     * `STA $3C` / `STX $47` resolve against the 65816 direct-page register D.
     * ShowTitle runs with D=$0600, so they live at $063C.. — NOT absolute zero
     * page (the CONTRACT's `dp=0x0` was a wrong RE assumption). _15ca85_c reads
     * them DP-relative, so we must write them the same way or it streams from
     * a stale source bank ($00 instead of $08). Honour D. */
    uint16_t d = snes->cpu->dp;
    #define DPW(off, v) write16(ram, (uint16_t)(d + (off)), (v))

    /* Common params: source bank $08, size $2800 bytes. */
    ram[(uint16_t)(d + 0x3C)] = 0x08;
    DPW(0x3D, 0xC000);
    DPW(0x45, 0x2800);

    /* First half - dest $0000. */
    DPW(0x47, 0x0000);
    _15ca85_c(snes);

    /* Second half - dest $4000. */
    DPW(0x47, 0x4000);
    _15ca85_c(snes);
    #undef DPW
}

/* PITFALLS: 6 (16-bit src/dst/size words), 12 (ROM source via _15ca85)
 * HELPERS: write16, _15ca85_c
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  none
 *   output_ram:  $3C..$48 (transfer params), VRAM as side effect
 *   entry_mode:  mf=true, xf=false, dp=0x600, db=0x00
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_00883d ($00:883D) - "load title screen bg graphics"
 */
