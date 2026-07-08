// FF4 native C port — Phase 5.4 proof-of-life.
//
// Drives the LakeSnes shadow-execution core. At this stage the goal is
// only to confirm that LakeSnes initialises and runs frames on the
// STM32H7B0 with a real ROM loaded — no PPU/LCD wiring, no hybrid
// native-C/asm dispatcher yet. The 88 translated battle/ routines are
// already compiled into the overlay, just not called yet.

#include <stddef.h>
#include <stdbool.h>
#include "snes/snes.h"

Snes *ff4_snes = NULL;

bool ff4_init(const uint8_t *rom_bytes, int rom_length) {
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
    /* ppu_handlePixel writes one 4-byte chunk per SNES column in this
     * build (the stock "right" hires sub-pixel is dropped -- FF4 never
     * uses hires and it always duplicated the left one). With
     * pixelOutputFormat = BGRX (= 1), the in-memory order is
     * [X, B, G, R] inside each chunk. */
    for (int y = 0; y < SNES_H; y++) {
        const uint8_t *row = src + y * PPU_PIXELBUF_STRIDE;
        uint16_t *dst = lcd_fb + (y + Y_OFFSET) * GW_LCD_W + X_OFFSET;
        for (int x = 0; x < SNES_W; x++) {
            uint8_t b = row[x * PPU_PIXELBUF_XPITCH + 1];
            uint8_t g = row[x * PPU_PIXELBUF_XPITCH + 2];
            uint8_t r = row[x * PPU_PIXELBUF_XPITCH + 3];
            dst[x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
    }
}
