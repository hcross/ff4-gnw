// FF4 native C port — Phase 5.4 proof-of-life.
//
// Drives the LakeSnes shadow-execution core. At this stage the goal is
// only to confirm that LakeSnes initialises and runs frames on the
// STM32H7B0 with a real ROM loaded — no PPU/LCD wiring, no hybrid
// native-C/asm dispatcher yet. The 88 translated battle/ routines are
// already compiled into the overlay, just not called yet.

#include <stddef.h>
#include <stdbool.h>
#ifndef FF4_REQUIRE_KNOWN_ROM
#include <stdio.h>  /* unknown-ROM warning on the tolerant (desktop) path */
#endif
#include "snes/snes.h"
#include "rom_ident.h"
#include "dispatch_all.h"

Snes *ff4_snes = NULL;

bool ff4_init(const uint8_t *rom_bytes, int rom_length) {
    /* Identify the image and arm the matching dispatch profile BEFORE
     * anything touches the emulator (translation-patch ADR, rom_ident.h).
     * Policy on an unknown image differs per target:
     *   - device (FF4_REQUIRE_KNOWN_ROM, set by the scaffold build): refuse;
     *     the scaffold renders the refusal screen from the ident accessors;
     *   - desktop: run it, but with the native dispatch fully disabled --
     *     the pure interpreter is slow but correct on any lineage. */
    ff4_rom_ident_t ident = ff4_rom_identify(rom_bytes, rom_length);
    if (ident == FF4_ROM_UNKNOWN) {
#ifdef FF4_REQUIRE_KNOWN_ROM
        return false;
#else
        fprintf(stderr,
                "ff4: unknown ROM (crc32 %08X) -- native dispatch disabled, "
                "running pure interpreter\n",
                (unsigned)ff4_rom_ident_crc32());
        ff4_dispatch_enabled = 0;
#endif
    }
    ff4_snes = snes_init();
    if (ff4_snes == NULL) {
        return false;
    }
    if (!snes_loadRom(ff4_snes, rom_bytes, rom_length)) {
        snes_free(ff4_snes);
        ff4_snes = NULL;
        return false;
    }
    return true;
}

void ff4_step(void) {
    if (ff4_snes != NULL) {
        snes_runFrame(ff4_snes);
    }
}

void ff4_set_button(int player, int button, bool pressed) {
    if (ff4_snes != NULL) {
        snes_setButtonState(ff4_snes, player, button, pressed);
    }
}

void ff4_shutdown(void) {
    if (ff4_snes != NULL) {
        snes_free(ff4_snes);
        ff4_snes = NULL;
    }
}

#include <stdint.h>
void ff4_get_state(uint32_t *frames_out, uint64_t *cycles_out) {
    if (ff4_snes == NULL) {
        if (frames_out) *frames_out = 0;
        if (cycles_out) *cycles_out = 0;
        return;
    }
    if (frames_out) *frames_out = ff4_snes->frames;
    if (cycles_out) *cycles_out = ff4_snes->cycles;
}

/* Blit the LakeSnes PPU frame to a G&W LCD framebuffer.
 *
 * The PPU writes into ppu->pixelBuffer one 4-byte pixel per SNES
 * column ([X, B, G, R] when pixelOutputFormat = BGRX), 256 columns x
 * 224 rows -- see PPU_PIXELBUF_STRIDE/PPU_PIXELBUF_XPITCH in ppu.h
 * (the stock hires-pair layout is halved in the FF4 static build).
 * We centre that into the 320x240 RGB565 LCD framebuffer with a
 * 32-pixel left/right margin and an 8-row top/bottom margin. */
#include "snes/ppu.h"
#define GW_LCD_W 320
#define GW_LCD_H 240
#define SNES_W   256
#define SNES_H   224
#define X_OFFSET ((GW_LCD_W - SNES_W) / 2)   /* 32 */
#define Y_OFFSET ((GW_LCD_H - SNES_H) / 2)   /* 8  */

void ff4_blit_to_lcd(uint16_t *lcd_fb) {
    if (ff4_snes == NULL || ff4_snes->ppu == NULL) return;
    const uint8_t *src = ff4_snes->ppu->pixelBuffer;
    /* R11: the PPU output stage now stores native little-endian RGB565
     * (the exact value this loop used to compute per pixel from the
     * 8-bit BGRX cells), so the blit is a plain row copy. Measured at
     * ~15% of the walking frame as a per-pixel conversion (LR-sampling,
     * 2026-07-12). */
    for (int y = 0; y < SNES_H; y++) {
        memcpy(lcd_fb + (y + Y_OFFSET) * GW_LCD_W + X_OFFSET,
               src + y * PPU_PIXELBUF_STRIDE,
               SNES_W * PPU_PIXELBUF_XPITCH);
    }
}
