
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ppu.h"
#include "snes.h"
#include "statehandler.h"

// array for layer definitions per mode:
//   0-7: mode 0-7; 8: mode 1 + l3prio; 9: mode 7 + extbg

//   0-3; layers 1-4; 4: sprites; 5: nonexistent
static const int layersPerMode[10][12] = {
  {4, 0, 1, 4, 0, 1, 4, 2, 3, 4, 2, 3},
  {4, 0, 1, 4, 0, 1, 4, 2, 4, 2, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5},
  {4, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5, 5},
  {2, 4, 0, 1, 4, 0, 1, 4, 4, 2, 5, 5},
  {4, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5, 5}
};

static const int prioritysPerMode[10][12] = {
  {3, 1, 1, 2, 0, 0, 1, 1, 1, 0, 0, 0},
  {3, 1, 1, 2, 0, 0, 1, 1, 0, 0, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5},
  {3, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5, 5},
  {1, 3, 1, 1, 2, 0, 0, 1, 0, 0, 5, 5},
  {3, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5, 5}
};

static const int layerCountPerMode[10] = {
  12, 10, 8, 8, 8, 8, 6, 5, 10, 7
};

static const int bitDepthsPerMode[10][4] = {
  {2, 2, 2, 2},
  {4, 4, 2, 5},
  {4, 4, 5, 5},
  {8, 4, 5, 5},
  {8, 2, 5, 5},
  {4, 2, 5, 5},
  {4, 5, 5, 5},
  {8, 5, 5, 5},
  {4, 4, 2, 5},
  {8, 7, 5, 5}
};

static const int spriteSizes[8][2] = {
  {8, 16}, {8, 32}, {8, 64}, {16, 32},
  {16, 64}, {32, 64}, {16, 32}, {16, 32}
};

static void ppu_handlePixel(Ppu* ppu, int x, int y);
static int ppu_getPixel(Ppu* ppu, int x, int y, bool sub, int* r, int* g, int* b);
static uint16_t ppu_getOffsetValue(Ppu* ppu, int col, int row);
static int ppu_getPixelForBgLayer(Ppu* ppu, int x, int y, int layer, bool priority);
static void ppu_handleOPT(Ppu* ppu, int layer, int* lx, int* ly);
static void ppu_calculateMode7Starts(Ppu* ppu, int y);
static int ppu_getPixelForMode7(Ppu* ppu, int x, int layer, bool priority);
static bool ppu_getWindowState(Ppu* ppu, int layer, int x);
static void ppu_evaluateSprites(Ppu* ppu, int line);
static uint16_t ppu_getVramRemap(Ppu* ppu);

#ifdef FF4_PORT_STATIC_SNES
/* G&W port: sizeof(Ppu) ≈ 1 MB on this build (pixel layer buffers
 * dominate). The 85 KB MCU heap cannot hold it; place the struct in
 * overlay_ff4 BSS (RAM_EMU, 740 KB) instead. */
static Ppu _ff4_ppu_storage;
#endif

Ppu* ppu_init(Snes* snes) {
#ifdef FF4_PORT_STATIC_SNES
  Ppu* ppu = &_ff4_ppu_storage;
#else
  Ppu* ppu = malloc(sizeof(Ppu));
#endif
  ppu->snes = snes;
  ppu_setPixelOutputFormat(ppu, ppu_pixelOutputFormatBGRX);
  return ppu;
}

void ppu_free(Ppu* ppu) {
#ifndef FF4_PORT_STATIC_SNES
  free(ppu);
#else
  (void)ppu;
#endif
}

void ppu_reset(Ppu* ppu) {
  memset(ppu->vram, 0, sizeof(ppu->vram));
  ppu->vramGen++; // invalidate the decoded-tile-row cache (R2b)
  ppu->vramPointer = 0;
  ppu->vramIncrementOnHigh = false;
  ppu->vramIncrement = 1;
  ppu->vramRemapMode = 0;
  ppu->vramReadBuffer = 0;
  memset(ppu->cgram, 0, sizeof(ppu->cgram));
  ppu->cgramGen++; // invalidate palette caches keyed on cgram content
  ppu->skipHaveBaseline = false; ppu->skipRasterWrite = false; ppu->skipMode = 0; // R4
  ppu->psValid = false; ppu->psStoring = false; // R5
  ppu->cgramPointer = 0;
  ppu->cgramSecondWrite = false;
  ppu->cgramBuffer = 0;
  memset(ppu->oam, 0, sizeof(ppu->oam));
  memset(ppu->highOam, 0, sizeof(ppu->highOam));
  ppu->oamAdr = 0;
  ppu->oamAdrWritten = 0;
  ppu->oamInHigh = false;
  ppu->oamInHighWritten = false;
  ppu->oamSecondWrite = false;
  ppu->oamBuffer = 0;
  ppu->objPriority = false;
  ppu->objTileAdr1 = 0;
  ppu->objTileAdr2 = 0;
  ppu->objSize = 0;
  memset(ppu->objPixelBuffer, 0, sizeof(ppu->objPixelBuffer));
  memset(ppu->objPriorityBuffer, 0, sizeof(ppu->objPriorityBuffer));
  ppu->timeOver = false;
  ppu->rangeOver = false;
  ppu->objInterlace = false;
  for(int i = 0; i < 4; i++) {
    ppu->bgLayer[i].hScroll = 0;
    ppu->bgLayer[i].vScroll = 0;
    ppu->bgLayer[i].tilemapWider = false;
    ppu->bgLayer[i].tilemapHigher = false;
    ppu->bgLayer[i].tilemapAdr = 0;
    ppu->bgLayer[i].tileAdr = 0;
    ppu->bgLayer[i].bigTiles = false;
    ppu->bgLayer[i].mosaicEnabled = false;
  }
  ppu->scrollPrev = 0;
  ppu->scrollPrev2 = 0;
  ppu->mosaicSize = 1;
  ppu->mosaicStartLine = 1;
  for(int i = 0; i < 5; i++) {
    ppu->layer[i].mainScreenEnabled = false;
    ppu->layer[i].subScreenEnabled = false;
    ppu->layer[i].mainScreenWindowed = false;
    ppu->layer[i].subScreenWindowed = false;
  }
  memset(ppu->m7matrix, 0, sizeof(ppu->m7matrix));
  ppu->m7prev = 0;
  ppu->m7largeField = false;
  ppu->m7charFill = false;
  ppu->m7xFlip = false;
  ppu->m7yFlip = false;
  ppu->m7extBg = false;
  ppu->m7startX = 0;
  ppu->m7startY = 0;
  for(int i = 0; i < 6; i++) {
    ppu->windowLayer[i].window1enabled = false;
    ppu->windowLayer[i].window2enabled = false;
    ppu->windowLayer[i].window1inversed = false;
    ppu->windowLayer[i].window2inversed = false;
    ppu->windowLayer[i].maskLogic = 0;
  }
  ppu->window1left = 0;
  ppu->window1right = 0;
  ppu->window2left = 0;
  ppu->window2right = 0;
  ppu->clipMode = 0;
  ppu->preventMathMode = 0;
  ppu->addSubscreen = false;
  ppu->subtractColor = false;
  ppu->halfColor = false;
  memset(ppu->mathEnabled, 0, sizeof(ppu->mathEnabled));
  ppu->fixedColorR = 0;
  ppu->fixedColorG = 0;
  ppu->fixedColorB = 0;
  ppu->forcedBlank = true;
  ppu->brightness = 0;
  ppu->mode = 0;
  ppu->bg3priority = false;
#ifdef FF4_PORT_STATIC_SNES
  /* G&W port: halved pixelBuffer requires evenFrame=true always
   * (see ppu_handleFrameStart). Start true; never toggled. */
  ppu->evenFrame = true;
#else
  ppu->evenFrame = false;
#endif
  ppu->pseudoHires = false;
  ppu->overscan = false;
  ppu->frameOverscan = false;
  ppu->interlace = false;
  ppu->frameInterlace = false;
  ppu->directColor = false;
  ppu->hCount = 0;
  ppu->vCount = 0;
  ppu->hCountSecond = false;
  ppu->vCountSecond = false;
  ppu->countersLatched = false;
  ppu->ppu1openBus = 0;
  ppu->ppu2openBus = 0;
  memset(ppu->pixelBuffer, 0, sizeof(ppu->pixelBuffer));
}

void ppu_handleState(Ppu* ppu, StateHandler* sh) {
  sh_handleBools(sh,
    &ppu->vramIncrementOnHigh, &ppu->cgramSecondWrite, &ppu->oamInHigh, &ppu->oamInHighWritten, &ppu->oamSecondWrite,
    &ppu->objPriority, &ppu->timeOver, &ppu->rangeOver, &ppu->objInterlace, &ppu->m7largeField, &ppu->m7charFill,
    &ppu->m7xFlip, &ppu->m7yFlip, &ppu->m7extBg, &ppu->addSubscreen, &ppu->subtractColor, &ppu->halfColor,
    &ppu->mathEnabled[0], &ppu->mathEnabled[1], &ppu->mathEnabled[2], &ppu->mathEnabled[3], &ppu->mathEnabled[4],
    &ppu->mathEnabled[5], &ppu->forcedBlank, &ppu->bg3priority, &ppu->evenFrame, &ppu->pseudoHires, &ppu->overscan,
    &ppu->frameOverscan, &ppu->interlace, &ppu->frameInterlace, &ppu->directColor, &ppu->hCountSecond, &ppu->vCountSecond,
    &ppu->countersLatched, NULL
  );
  sh_handleBytes(sh,
    &ppu->vramRemapMode, &ppu->cgramPointer, &ppu->cgramBuffer, &ppu->oamAdr, &ppu->oamAdrWritten, &ppu->oamBuffer,
    &ppu->objSize, &ppu->scrollPrev, &ppu->scrollPrev2, &ppu->mosaicSize, &ppu->mosaicStartLine, &ppu->m7prev,
    &ppu->window1left, &ppu->window1right, &ppu->window2left, &ppu->window2right, &ppu->clipMode, &ppu->preventMathMode,
    &ppu->fixedColorR, &ppu->fixedColorG, &ppu->fixedColorB, &ppu->brightness, &ppu->mode,
    &ppu->ppu1openBus, &ppu->ppu2openBus, NULL
  );
  sh_handleWords(sh,
    &ppu->vramPointer, &ppu->vramIncrement, &ppu->vramReadBuffer, &ppu->objTileAdr1, &ppu->objTileAdr2,
    &ppu->hCount, &ppu->vCount, NULL
  );
  sh_handleWordsS(sh,
    &ppu->m7matrix[0], &ppu->m7matrix[1], &ppu->m7matrix[2], &ppu->m7matrix[3], &ppu->m7matrix[4], &ppu->m7matrix[5],
    &ppu->m7matrix[6], &ppu->m7matrix[7], NULL
  );
  sh_handleIntsS(sh, &ppu->m7startX, &ppu->m7startY, NULL);
  for(int i = 0; i < 4; i++) {
    sh_handleBools(sh,
      &ppu->bgLayer[i].tilemapWider, &ppu->bgLayer[i].tilemapHigher, &ppu->bgLayer[i].bigTiles,
      &ppu->bgLayer[i].mosaicEnabled, NULL
    );
    sh_handleWords(sh,
      &ppu->bgLayer[i].hScroll, &ppu->bgLayer[i].vScroll, &ppu->bgLayer[i].tilemapAdr, &ppu->bgLayer[i].tileAdr, NULL
    );
  }
  for(int i = 0; i < 5; i++) {
    sh_handleBools(sh,
      &ppu->layer[i].mainScreenEnabled, &ppu->layer[i].subScreenEnabled, &ppu->layer[i].mainScreenWindowed,
      &ppu->layer[i].subScreenWindowed, NULL
    );
  }
  for(int i = 0; i < 6; i++) {
    sh_handleBools(sh,
      &ppu->windowLayer[i].window1enabled, &ppu->windowLayer[i].window1inversed, &ppu->windowLayer[i].window2enabled,
      &ppu->windowLayer[i].window2inversed, NULL
    );
    sh_handleBytes(sh, &ppu->windowLayer[i].maskLogic, NULL);
  }
  sh_handleWordArray(sh, ppu->vram, 0x8000);
  sh_handleWordArray(sh, ppu->cgram, 0x100);
  sh_handleWordArray(sh, ppu->oam, 0x100);
  sh_handleByteArray(sh, ppu->highOam, 0x20);
  sh_handleByteArray(sh, ppu->objPixelBuffer, 256);
  sh_handleByteArray(sh, ppu->objPriorityBuffer, 256);
  if(!sh->saving) { ppu->cgramGen++; ppu->vramGen++; ppu->skipHaveBaseline = false; ppu->psValid = false; ppu->psStoring = false; } // loaded gfx: invalidate caches + force a render
}

bool ppu_checkOverscan(Ppu* ppu) {
  // called at (0,225)
  ppu->frameOverscan = ppu->overscan; // set if we have a overscan-frame
  return ppu->frameOverscan;
}

void ppu_handleVblank(Ppu* ppu) {
  if(ppu->psStoring) { ppu->psValid = true; ppu->psStoring = false; }
  // called either right after ppu_checkOverscan at (0,225), or at (0,240)
  if(!ppu->forcedBlank) {
    ppu->oamAdr = ppu->oamAdrWritten;
    ppu->oamInHigh = ppu->oamInHighWritten;
    ppu->oamSecondWrite = false;
  }
  ppu->frameInterlace = ppu->interlace; // set if we have a interlaced frame
#ifdef FF4_PORT_STATIC_SNES
  /* G&W port: pixelBuffer is halved to single-frame (rows 0..238 only,
   * no row 239..477 slot). Interlace mode would index past the end,
   * so disable it unconditionally here. */
  ppu->frameInterlace = false;
#endif
}

// R4: FNV-1a fold of a memory range into a running hash.
static uint32_t ppu_fnv(uint32_t h, const void *p, unsigned n) {
  const uint8_t *b = (const uint8_t *)p;
  for(unsigned i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
  return h;
}

// R4: signature of everything the pixel render reads. Excludes the volatile
// per-line scratch (objPixelBuffer/objPriorityBuffer, rebuilt each line) and
// the frame-parity latch (evenFrame/frameInterlace). vram/cgram are summed by
// their gen counters, not rehashed here. Over-inclusion only costs a missed
// skip (never a wrong one); the risk to guard is UNDER-inclusion, caught by
// the per-frame framebuffer-CRC validation over animated fixtures.
static uint32_t ppu_computeRenderSig(Ppu* ppu) {
  uint32_t h = 2166136261u;
  h = ppu_fnv(h, ppu->oam, sizeof(ppu->oam) + sizeof(ppu->highOam)); // oam+highOam adjacent
  h = ppu_fnv(h, &ppu->objPriority, sizeof(uint8_t));
  h = ppu_fnv(h, &ppu->objTileAdr1, sizeof(ppu->objTileAdr1));
  h = ppu_fnv(h, &ppu->objTileAdr2, sizeof(ppu->objTileAdr2));
  h = ppu_fnv(h, &ppu->objSize, sizeof(ppu->objSize));
  h = ppu_fnv(h, &ppu->objInterlace, sizeof(uint8_t));
  h = ppu_fnv(h, ppu->bgLayer, sizeof(ppu->bgLayer));
  h = ppu_fnv(h, &ppu->mosaicSize, sizeof(ppu->mosaicSize));
  h = ppu_fnv(h, &ppu->mosaicStartLine, sizeof(ppu->mosaicStartLine)); // ly base for mosaic
  h = ppu_fnv(h, ppu->layer, sizeof(ppu->layer));
  h = ppu_fnv(h, ppu->m7matrix, sizeof(ppu->m7matrix));
  h = ppu_fnv(h, &ppu->m7largeField, 5); // m7largeField/charFill/xFlip/yFlip/extBg bools
  h = ppu_fnv(h, &ppu->m7startX, sizeof(ppu->m7startX) + sizeof(ppu->m7startY));
  h = ppu_fnv(h, ppu->windowLayer, sizeof(ppu->windowLayer));
  h = ppu_fnv(h, &ppu->window1left, 4); // window1left/right, window2left/right
  h = ppu_fnv(h, &ppu->clipMode, sizeof(ppu->clipMode));
  h = ppu_fnv(h, &ppu->preventMathMode, sizeof(ppu->preventMathMode));
  h = ppu_fnv(h, &ppu->addSubscreen, 3); // addSubscreen/subtractColor/halfColor
  h = ppu_fnv(h, ppu->mathEnabled, sizeof(ppu->mathEnabled));
  h = ppu_fnv(h, &ppu->fixedColorR, 3);
  h = ppu_fnv(h, &ppu->forcedBlank, 1);
  h = ppu_fnv(h, &ppu->brightness, sizeof(ppu->brightness));
  h = ppu_fnv(h, &ppu->mode, sizeof(ppu->mode));
  h = ppu_fnv(h, &ppu->bg3priority, 1);
  h = ppu_fnv(h, &ppu->pseudoHires, 1);
  h = ppu_fnv(h, &ppu->overscan, 1);
  h = ppu_fnv(h, &ppu->frameOverscan, 1);
  h = ppu_fnv(h, &ppu->interlace, 1);
  h = ppu_fnv(h, &ppu->directColor, 1);
  return h;
}

void ppu_handleFrameStart(Ppu* ppu) {
  // called at (0, 0)
  ppu->mosaicStartLine = 1;
  ppu->rangeOver = false;
  ppu->timeOver = false;
#ifdef FF4_PORT_STATIC_SNES
  /* G&W port: keep evenFrame=true forever so pixel writes land at
   * row = y-1 (0..238) rather than y-1+239 (239..477) which would
   * overrun the halved pixelBuffer. */
  ppu->evenFrame = true;
#else
  ppu->evenFrame = !ppu->evenFrame;
#endif
  // R4/R5 skip decision, gated on geometry stability (no visible-line
  // ppu_write last frame, no HDMA armed). mode 1 = whole-frame identical;
  // mode 2 = geometry identical to the line store but cgram moved (replay
  // only the output stage).
  {
    const uint32_t sig = ppu_computeRenderSig(ppu);
    const bool prevFrameRaster = ppu->skipRasterWrite;
    ppu->skipRasterWrite = false;
    const bool geomStable = !prevFrameRaster && !snes_anyHdmaActive(ppu->snes);
    ppu->skipMode = 0;
    if(geomStable && ppu->skipHaveBaseline
       && sig == ppu->skipSig
       && ppu->vramGen == ppu->skipVramGen
       && ppu->cgramGen == ppu->skipCgramGen) {
      ppu->skipMode = 1;
      ppu->psStoring = false;
    } else if(geomStable && ppu->psValid
              && sig == ppu->psSig
              && ppu->vramGen == ppu->psVramGen) {
      ppu->skipMode = 2;
      ppu->psStoring = false;
      ppu->skipSig = sig;
      ppu->skipVramGen = ppu->vramGen;
      ppu->skipCgramGen = ppu->cgramGen;
      ppu->skipHaveBaseline = true;
    } else {
      ppu->skipSig = sig;
      ppu->skipVramGen = ppu->vramGen;
      ppu->skipCgramGen = ppu->cgramGen;
      ppu->skipHaveBaseline = true;
      ppu->psStoring = true;
      ppu->psValid = false;
      ppu->psSig = sig;
      ppu->psVramGen = ppu->vramGen;
    }
  }
}

/* Per-scanline BG fetch cache. ppu_getPixelForBgLayer is called once per
 * pixel per layer, but the tilemap word and the bitplane words it reads from
 * VRAM are identical across the 8 horizontal pixels of a tile span (only the
 * column bit-extraction varies). Memoise the two/four VRAM reads keyed on the
 * exact masked address they target. Valid only within one ppu_runLine call:
 * VRAM is stable for the whole scanline (the CPU runs between lines, not
 * mid-line), and the key is the real address read, so a hit always returns the
 * same bytes the uncached path would. Reset (sentinel 0xFFFF > 0x7FFF max) at
 * the top of every scanline. Cuts ~7/8 of BG-layer VRAM fetches in-game. */
static uint16_t s_bgTilemapAdr[4];
static uint16_t s_bgTile[4];
static uint16_t s_bgPlaneAdr[4];
static uint16_t s_bgPlane[4][4];

static inline void ppu_resetBgCache(void) {
  for(int i = 0; i < 4; i++) { s_bgTilemapAdr[i] = 0xFFFF; s_bgPlaneAdr[i] = 0xFFFF; }
}

int ff4_ppu_render_enabled = 1;

/* ==================== M1: per-line compositor ====================
 *
 * Line renderer for the hot modes (0/1/3, no OPT/hires/mosaic/interlace):
 * replaces the 256x ppu_handlePixel -> ppu_getPixel(main[+sub]) ->
 * layer-priority-loop -> function-call-per-pixel-per-layer pipeline with
 * three per-line stages:
 *   1. decode each active BG layer's full line into index+priority arrays
 *      (exact ppu_getPixelForBgLayer address/extraction math);
 *   2. compose the main (and, when color math needs it, sub) screen line
 *      by PAINTING layer entries in REVERSE priority order (lowest first,
 *      opaque pixels overwrite) -- same final pixel as the original
 *      first-opaque-wins walk, but array ops instead of calls;
 *   3. resolve CGRAM/direct color, clip window, color math and brightness
 *      per pixel from the composed lines, reproducing ppu_handlePixel's
 *      arithmetic exactly, and write the (halved) pixelBuffer.
 * Every excluded configuration (mode 7's own path, modes 2/4/6 OPT,
 * 5/6 hires, active mosaic, interlace, pseudo-hires) falls back to the
 * untouched per-pixel path below.
 *
 * Motivation: measured on the G&W (D6 probe, title screen, 2026-07-09)
 * the render step costs 186 ms of a 254 ms frame. The per-pixel pipeline's
 * cost is calls + re-derived addressing per pixel per layer per screen;
 * this compositor does that work once per line per layer. */

/* On the G&W, pin the compositor's per-line scratch into zero-wait-state
 * DTCM: the ff4 overlay's .bss lives in AXI SRAM behind a 16 KB D-cache
 * that the frame loop (VRAM + WRAM + pixelBuffer traffic) keeps evicting,
 * so these hot intermediates otherwise pay bus latency per access. All of
 * them are fully written before being read on every rendered line, so the
 * NOLOAD (non-zeroed) placement is safe. Desktop builds keep plain BSS. */
#ifdef STM32H7B0xx
#define FF4_LR_SCRATCH __attribute__((section(".ff4_dtcm")))
#else
#define FF4_LR_SCRATCH
#endif
static FF4_LR_SCRATCH uint16_t s_lrVal[4][256];     // decoded BG line: CGRAM index (0 = transparent)
static FF4_LR_SCRATCH uint8_t  s_lrPrio[4][256];    // decoded BG line: tile priority bit
static FF4_LR_SCRATCH uint8_t  s_lrHasPrio[4][2];   // any opaque pixel at prio 0/1 on this line?
static FF4_LR_SCRATCH bool     s_lrSpritesAny;      // any sprite pixel on this line?
static FF4_LR_SCRATCH uint16_t s_lrPix[2][256];     // composed pixel index   [0]=main [1]=sub
static FF4_LR_SCRATCH uint8_t  s_lrLayer[2][256];   // composed layer id (5 = backdrop, 6 = no-math sprite)
static FF4_LR_SCRATCH uint8_t  s_lrWin[6][256];     // window membership per window-layer, this line
// R2a: final palette — cgram color with display brightness pre-applied, in
// pixel-buffer byte order (b, g, r). Serves every no-math, no-clip,
// non-direct-color pixel of the output stage with three byte copies.
// Keyed on (cgramGen, brightness); s_lrPal3Valid sits in zeroed BSS so a
// cold overlay start always rebuilds.
static FF4_LR_SCRATCH uint8_t  s_lrPal3[256][3];
static bool     s_lrPal3Valid;
static uint32_t s_lrPal3Gen;
static uint8_t  s_lrPal3Bright;

// R5 palette-only skip line store (see ppu.h). ~143 KB of overlay BSS.
#define PS_LINES 224
static uint8_t s_psPix[PS_LINES][256];
static uint8_t s_psSub[PS_LINES][256];
static uint8_t s_psFlg[PS_LINES][128];
#define PS_F_CLIP 1u
#define PS_F_MATH 2u
#define PS_F_SUBV 4u

/* R8: brightness LUT cache. Was rebuilt (32 muls + divides) per line by
 * both ppu_lrRunLine and ppu_lrPaletteLine -- 224x per frame for a value
 * that only changes on a $2100 write. The valid flag sits in zeroed BSS
 * (NOT in the NOLOAD DTCM section) so a cold overlay start always
 * rebuilds, same convention as s_lrPal3Valid. */
static FF4_LR_SCRATCH uint8_t s_brightLut[32];
static bool    s_brightValid;
static uint8_t s_brightFor;
static const uint8_t* ppu_lrBright(Ppu* ppu) {
  if(!s_brightValid || s_brightFor != ppu->brightness) {
    for(int c = 0; c < 32; c++)
      s_brightLut[c] = (uint8_t)((((c << 3) | (c >> 2)) * ppu->brightness) / 15);
    s_brightValid = true;
    s_brightFor = ppu->brightness;
  }
  return s_brightLut;
}

static void ppu_lrRefreshPal3(Ppu* ppu, const uint8_t bright[32]) {
  if(s_lrPal3Valid && s_lrPal3Gen == ppu->cgramGen && s_lrPal3Bright == ppu->brightness)
    return;
  for(int c = 0; c < 256; c++) {
    const uint16_t col = ppu->cgram[c];
    s_lrPal3[c][0] = bright[(col >> 10) & 0x1f];
    s_lrPal3[c][1] = bright[(col >> 5) & 0x1f];
    s_lrPal3[c][2] = bright[col & 0x1f];
  }
  s_lrPal3Valid = true;
  s_lrPal3Gen = ppu->cgramGen;
  s_lrPal3Bright = ppu->brightness;
}

// R2b: decoded-tile-row cache. Keyed on (planeAdr, bitDepth) — the VRAM
// word address of a tile's row and its bit depth fully determine the eight
// dechunked palette-relative pixel values, BEFORE hFlip and palette offset
// (both applied per use). raw[k] = the pixel built from bitplane column k
// (k = 0..7), so a use reads raw[hFlip ? px : 7-px]. Invalidated wholesale
// by ppu->vramGen: measured 0 VRAM writes/frame on the title and ~5 frames
// in 6 on field, so hit rate is ~100% on the scenes this targets. Direct-
// mapped, 4096 slots; a collision or a gen miss just re-decodes (still
// byte-identical). File-scope singleton — FF4_PORT_STATIC_SNES contract,
// like the E2 access maps.
// 2048 slots (was 4096): halved 2026-07-10 to make room for the R5 line
// store alongside the FF4_LOAD_SAVESTATE boot path (the combination was
// 2,676 bytes over the RAM_EMU budget; this frees 32 KB). Collisions only
// cost a re-decode -- correctness is invariant -- and the measured impact
// on the field/title D6 is within noise (see the release table).
#define LR_TRC_SLOTS 2048
static uint8_t  s_trcRaw[LR_TRC_SLOTS][8];
static uint32_t s_trcKey[LR_TRC_SLOTS];   // (planeAdr << 4) | bitDepth
static uint32_t s_trcGen[LR_TRC_SLOTS];

static void ppu_lrDecodeBgLine(Ppu* ppu, int layer, int y) {
  // Same address/extraction math as ppu_getPixelForBgLayer, restricted to
  // modes 0/1/3 (wideTiles == bigTiles, same tileBits/highBit both axes),
  // no mosaic (ly is line-constant).
  const bool bigTiles = ppu->bgLayer[layer].bigTiles;
  const int tileBits = bigTiles ? 4 : 3;
  const int tileHighBit = bigTiles ? 0x200 : 0x100;
  const int bitDepth = bitDepthsPerMode[ppu->mode][layer];
  const int hScroll = ppu->bgLayer[layer].hScroll;
  // mosaic: the source row is the block's base row -- same expression as
  // the legacy path (ppu_handlePixel), applied BEFORE the scroll add so the
  // signed-% behaviour matches exactly
  int yEff = y;
  const int mosSize = (ppu->bgLayer[layer].mosaicEnabled && ppu->mosaicSize > 1)
    ? ppu->mosaicSize : 1;
  if(mosSize > 1) yEff -= (yEff - ppu->mosaicStartLine) % mosSize;
  const int ly = (yEff + ppu->bgLayer[layer].vScroll) & 0x3ff;
  int sx = 0;
  while(sx < 256) {
    const int lx = (sx + hScroll) & 0x3ff;
    int spanLen = 8 - (lx & 7);           // span ends at the tile boundary
    if(spanLen > 256 - sx) spanLen = 256 - sx;
    uint16_t tilemapAdr = ppu->bgLayer[layer].tilemapAdr + (((ly >> tileBits) & 0x1f) << 5 | ((lx >> tileBits) & 0x1f));
    if((lx & tileHighBit) && ppu->bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
    if((ly & tileHighBit) && ppu->bgLayer[layer].tilemapHigher) tilemapAdr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
    const uint16_t tile = ppu->vram[tilemapAdr & 0x7fff];
    const uint8_t prio = (tile & 0x2000) ? 1 : 0;
    int paletteNum = (tile & 0x1c00) >> 10;
    const int row = (tile & 0x8000) ? 7 - (ly & 0x7) : (ly & 0x7);
    int tileNum = tile & 0x3ff;
    if(bigTiles) {
      if(((bool) (lx & 8)) ^ ((bool) (tile & 0x4000))) tileNum += 1;
      if(((bool) (ly & 8)) ^ ((bool) (tile & 0x8000))) tileNum += 0x10;
    }
    int paletteSize = 4;
    if(bitDepth > 2) paletteSize = 16;
    if(bitDepth > 4) paletteSize = 256;
    if(ppu->mode == 0) paletteNum += 8 * layer;
    const int paletteBase = paletteSize * paletteNum;
    const uint16_t planeAdr = (ppu->bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + row) & 0x7fff;
    // R2b: fetch (or build) this row's eight dechunked pixels in column
    // order (raw[k] from bitplane column k), before hFlip and palette
    const uint32_t trcKey = ((uint32_t)planeAdr << 4) | (uint32_t)bitDepth;
    const uint32_t trcSlot = planeAdr & (LR_TRC_SLOTS - 1);
    uint8_t *raw = s_trcRaw[trcSlot];
    if(s_trcGen[trcSlot] != ppu->vramGen || s_trcKey[trcSlot] != trcKey) {
      const uint16_t plane1 = ppu->vram[planeAdr];
      uint16_t plane2 = 0, plane3 = 0, plane4 = 0;
      if(bitDepth > 2) plane2 = ppu->vram[(planeAdr + 8) & 0x7fff];
      if(bitDepth > 4) {
        plane3 = ppu->vram[(planeAdr + 16) & 0x7fff];
        plane4 = ppu->vram[(planeAdr + 24) & 0x7fff];
      }
      for(int col = 0; col < 8; col++) {
        int pixel = (plane1 >> col) & 1;
        pixel |= ((plane1 >> (8 + col)) & 1) << 1;
        if(bitDepth > 2) {
          pixel |= ((plane2 >> col) & 1) << 2;
          pixel |= ((plane2 >> (8 + col)) & 1) << 3;
        }
        if(bitDepth > 4) {
          pixel |= ((plane3 >> col) & 1) << 4;
          pixel |= ((plane3 >> (8 + col)) & 1) << 5;
          pixel |= ((plane4 >> col) & 1) << 6;
          pixel |= ((plane4 >> (8 + col)) & 1) << 7;
        }
        raw[col] = (uint8_t)pixel;
      }
      s_trcGen[trcSlot] = ppu->vramGen;
      s_trcKey[trcSlot] = trcKey;
    }
    const bool hFlip = (tile & 0x4000) != 0;
    uint16_t *ov = &s_lrVal[layer][sx];
    uint8_t  *op = &s_lrPrio[layer][sx];
    int hasPrio = 0;
    if(hFlip) {
      for(int i = 0; i < spanLen; i++) {
        const int pixel = raw[(lx + i) & 0x7];
        ov[i] = pixel == 0 ? 0 : (uint16_t)(paletteBase + pixel);
        op[i] = (uint8_t)prio;
        hasPrio |= pixel;
      }
    } else {
      for(int i = 0; i < spanLen; i++) {
        const int pixel = raw[7 - ((lx + i) & 0x7)];
        ov[i] = pixel == 0 ? 0 : (uint16_t)(paletteBase + pixel);
        op[i] = (uint8_t)prio;
        hasPrio |= pixel;
      }
    }
    if(hasPrio) s_lrHasPrio[layer][prio] = 1;
    sx += spanLen;
  }
  if(mosSize > 1) {
    // horizontal mosaic: every pixel shows its block's base pixel. The
    // legacy path fetches (x - x%size) + hScroll; the loop above decoded
    // exactly those base positions, so replication is a straight copy
    // (ascending x reads an already-final base slot).
    uint16_t *val = s_lrVal[layer];
    uint8_t  *prio = s_lrPrio[layer];
    for(int x = 0; x < 256; x++) {
      const int bx = x - (x % mosSize);
      val[x] = val[bx];
      prio[x] = prio[bx];
    }
  }
}

/* R7/R8: mode-7 line decode. Exact ppu_getPixelForMode7 math, with the
 * per-line m7startX/m7startY already computed by ppu_calculateMode7Starts
 * (which also owns the VERTICAL mosaic adjustment, keyed on layer 0's
 * mosaicEnabled exactly like the legacy path).
 *
 * ppu_m7DecodeLineU8 writes the full 8-bit mode-7 pixel per x. Two
 * variants:
 *  - HOT (R8): the census-verified FF4 worldmap shape -- m7matrix[2] == 0
 *    (no rotation => the whole line samples ONE map row), no xFlip, no
 *    largeField, no mosaic. The affine walk degenerates to an incremental
 *    accX (no per-pixel multiply), the tilemap/char row bases hoist out of
 *    the loop, the tilemap fetch is memoised on the tile column (~1/8
 *    fetches at 1:1 zoom), and both VRAM fetches are explicit little-endian
 *    byte loads (tilemap = low byte, char = high byte of the VRAM word)
 *    instead of 16-bit load + mask/shift.
 *  - GENERAL: per-pixel legacy math (rotation, flips, largeField bounds,
 *    mosaic), kept literally except for a tilemap-cell memo, which is
 *    address-keyed and therefore exact.
 * Byte-identical by construction: the hot variant computes the same
 * (m7startX + m0*x) >> 8 value by accumulation (integer, exact, no
 * overflow: |m0| <= 32768, x <= 255). */
static void ppu_m7DecodeLineU8(Ppu* ppu, uint8_t dst[256]) {
#ifdef FF4_M7_CENSUS
  {
    static uint32_t n, spr;
    int sa = 0; for(int i = 0; i < 256; i++) if(ppu->objPixelBuffer[i]) { sa = 1; break; }
    if(sa) spr++;
    if((++n % 5000) == 1)
      fprintf(stderr, "M7CENSUS n=%u spr=%u lf=%d cf=%d xf=%d yf=%d m0=%d m1=%d m2=%d m3=%d mos=%d/%d math=%d%d%d%d%d%d clip=%d dc=%d win0=%d%d mainEn=%d subEn=%d bright=%d extbg=%d\n",
        n, spr, ppu->m7largeField, ppu->m7charFill, ppu->m7xFlip, ppu->m7yFlip,
        (int16_t)ppu->m7matrix[0], (int16_t)ppu->m7matrix[1], (int16_t)ppu->m7matrix[2], (int16_t)ppu->m7matrix[3],
        ppu->bgLayer[0].mosaicEnabled, ppu->mosaicSize,
        ppu->mathEnabled[0], ppu->mathEnabled[1], ppu->mathEnabled[2], ppu->mathEnabled[3], ppu->mathEnabled[4], ppu->mathEnabled[5],
        ppu->clipMode, ppu->directColor,
        ppu->windowLayer[0].window1enabled, ppu->windowLayer[0].window2enabled,
        ppu->layer[0].mainScreenEnabled, ppu->layer[0].subScreenEnabled, ppu->brightness, ppu->m7extBg);
  }
#endif
  const int32_t m0 = (int16_t)ppu->m7matrix[0];
  const int32_t m2 = (int16_t)ppu->m7matrix[2];
  const int mosSize = (ppu->bgLayer[0].mosaicEnabled && ppu->mosaicSize > 1)
    ? ppu->mosaicSize : 1;
  if(m2 == 0 && !ppu->m7xFlip && !ppu->m7largeField && mosSize == 1) {
    // HOT: one source row for the whole line (little-endian byte views).
    const int yPos = (ppu->m7startY >> 8) & 0x3ff;
    const uint8_t *tmapRow = (const uint8_t*)(ppu->vram + ((yPos >> 3) << 7));
    const uint8_t *charHi  = (const uint8_t*)ppu->vram + 1;
    const int charRowOff = (yPos & 7) << 3;
    const uint8_t *charRow = charHi;      // set on the first tile fetch
    int32_t accX = ppu->m7startX;
    int lastTx = -1;
    for(int x = 0; x < 256; x++) {
      const int xPos = (accX >> 8) & 0x3ff;
      const int tx = xPos >> 3;
      if(tx != lastTx) {
        lastTx = tx;
        charRow = charHi + ((((int)tmapRow[tx << 1] << 6) + charRowOff) << 1);
      }
      dst[x] = charRow[(xPos & 7) << 1];
      accX += m0;
    }
    return;
  }
  // GENERAL: legacy per-pixel math, tilemap fetch memoised by cell address.
  int lastTa = -1;
  int charAdr = 0;
  for(int x = 0; x < 256; x++) {
    int lx = x;
    if(mosSize > 1) lx -= lx % mosSize;
    const uint8_t rx = ppu->m7xFlip ? 255 - lx : lx;
    int xPos = (ppu->m7startX + m0 * rx) >> 8;
    int yPos = (ppu->m7startY + m2 * rx) >> 8;
    bool outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;
    xPos &= 0x3ff;
    yPos &= 0x3ff;
    if(!ppu->m7largeField) outsideMap = false;
    int ca;
    if(outsideMap) {
      if(!ppu->m7charFill) { dst[x] = 0; continue; }
      ca = 0;                                  // legacy: tile forced to 0
    } else {
      const int ta = (yPos >> 3) * 128 + (xPos >> 3);
      if(ta != lastTa) { lastTa = ta; charAdr = (ppu->vram[ta] & 0xff) << 6; }
      ca = charAdr;
    }
    dst[x] = (uint8_t)(ppu->vram[ca + ((yPos & 7) << 3) + (xPos & 7)] >> 8);
  }
}

/* Line scratch for the mode-7 u8 decode (see FF4_LR_SCRATCH above). */
static FF4_LR_SCRATCH uint8_t s_m7Line[256];
static FF4_LR_SCRATCH uint8_t s_m7Win[256];   // R8b: sprite-layer window mask

/* layer 0 = the mode-7 BG: full 8-bit pixel, painted at the single priority
 * slot the mode-7 rows of layersPerMode give it. layer 1 = EXTBG (actMode
 * 9): the pixel's bit 7 is its priority, low 7 bits the color index -- the
 * legacy helper returns 0 on a priority mismatch, which maps here to
 * storing (val = low 7 bits, prio = bit 7) and letting the compose pass
 * match on prio: a 0x80 pixel becomes val 0 at prio 1 = transparent, same
 * outcome. */
static void ppu_lrDecodeM7Line(Ppu* ppu, int layer, int y) {
  (void)y;   // vertical position is baked into m7startX/Y
  ppu_m7DecodeLineU8(ppu, s_m7Line);
  uint16_t *val = s_lrVal[layer];
  uint8_t  *prio = s_lrPrio[layer];
  if(layer == 1) {
    uint8_t hasPrio0 = 0, hasPrio1 = 0;
    for(int x = 0; x < 256; x++) {
      const uint8_t pixel = s_m7Line[x];
      const uint8_t p = (uint8_t)(pixel >> 7);
      const uint16_t v = pixel & 0x7f;
      val[x] = v;
      prio[x] = p;
      if(v) { if(p) hasPrio1 = 1; else hasPrio0 = 1; }
    }
    if(hasPrio0) s_lrHasPrio[layer][0] = 1;
    if(hasPrio1) s_lrHasPrio[layer][1] = 1;
  } else {
    uint32_t acc = 0;
    for(int x = 0; x < 256; x++) {
      const uint8_t pixel = s_m7Line[x];
      val[x] = pixel;
      acc |= pixel;
    }
    memset(prio, 0, 256);
    if(acc) s_lrHasPrio[layer][0] = 1;
  }
}

/* R8: apply layer-l's main-screen window mask to a decoded u8 line --
 * masked pixels drop to index 0 (backdrop; the fused output then writes
 * s_lrPal3[0] = the cgram[0] color, exactly what the compose pass yields
 * for a window-masked pixel). Same membership evaluation and span
 * structure as the s_lrWin build in ppu_lrRunLine: membership is
 * piecewise-constant between window edges. FF4 worldmap concretely: an
 * inverted window1 [1,254] on layer 0 masking only columns 0 and 255
 * (the classic mode-7 edge-artifact hide). */
/* R8b: build layer-l's window membership as a 0/1 mask line (same span
 * evaluation as ppu_lrWinMaskU8 / the s_lrWin build; 1 = masked off the
 * main screen). Used for the sprite layer in the fused mode-7 path,
 * where masked sprite pixels must simply not paint. */
static void ppu_lrWinSpanMask(Ppu* ppu, int l, uint8_t *mask) {
  if(!ppu->windowLayer[l].window1enabled && !ppu->windowLayer[l].window2enabled) {
    memset(mask, 0, 256);
    return;
  }
  int winBp[6] = {0, ppu->window1left, ppu->window1right + 1,
                  ppu->window2left, ppu->window2right + 1, 256};
  for(int i = 1; i < 6; i++) {
    int v = winBp[i], j = i;
    while(j > 0 && winBp[j-1] > v) { winBp[j] = winBp[j-1]; j--; }
    winBp[j] = v;
  }
  for(int i = 0; i < 5; i++) {
    const int s = winBp[i];
    int e = winBp[i + 1];
    if(e > 256) e = 256;
    if(s >= e) continue;
    memset(&mask[s], ppu_getWindowState(ppu, l, s) ? 1 : 0, e - s);
  }
}

static void ppu_lrWinMaskU8(Ppu* ppu, int l, uint8_t *buf) {
  if(!ppu->windowLayer[l].window1enabled && !ppu->windowLayer[l].window2enabled) return;
  int winBp[6] = {0, ppu->window1left, ppu->window1right + 1,
                  ppu->window2left, ppu->window2right + 1, 256};
  for(int i = 1; i < 6; i++) {           // insertion sort, 6 entries
    int v = winBp[i], j = i;
    while(j > 0 && winBp[j-1] > v) { winBp[j] = winBp[j-1]; j--; }
    winBp[j] = v;
  }
  for(int i = 0; i < 5; i++) {
    const int s = winBp[i];
    int e = winBp[i + 1];
    if(e > 256) e = 256;
    if(s >= e) continue;
    if(ppu_getWindowState(ppu, l, s)) memset(&buf[s], 0, e - s);
  }
}

static void ppu_lrComposeLine(Ppu* ppu, int actMode, bool sub, const bool *bgDecoded) {
  uint16_t *outPix   = s_lrPix[sub ? 1 : 0];
  uint8_t  *outLayer = s_lrLayer[sub ? 1 : 0];
  memset(outPix, 0, 256 * sizeof(uint16_t));
  memset(outLayer, 5, 256);                       // backdrop
  for(int i = layerCountPerMode[actMode] - 1; i >= 0; i--) {
    const int curLayer = layersPerMode[actMode][i];
    const int curPriority = prioritysPerMode[actMode][i];
    const bool enabled = sub ? ppu->layer[curLayer].subScreenEnabled
                             : ppu->layer[curLayer].mainScreenEnabled;
    if(!enabled) continue;
    const bool windowed = sub ? ppu->layer[curLayer].subScreenWindowed
                              : ppu->layer[curLayer].mainScreenWindowed;
    const uint8_t *win = windowed ? s_lrWin[curLayer] : NULL;   // win[x] -> masked off
    if(curLayer < 4) {
      if(!bgDecoded[curLayer]) continue;
      if(!s_lrHasPrio[curLayer][curPriority]) continue;   // no opaque pixel at this prio
      const uint16_t *val  = s_lrVal[curLayer];
      const uint8_t  *prio = s_lrPrio[curLayer];
      if(win) {
        for(int x = 0; x < 256; x++) {
          if(win[x] || prio[x] != curPriority) continue;
          const uint16_t v = val[x];
          if(v) { outPix[x] = v; outLayer[x] = (uint8_t)curLayer; }
        }
      } else {
        for(int x = 0; x < 256; x++) {
          if(prio[x] != curPriority) continue;
          const uint16_t v = val[x];
          if(v) { outPix[x] = v; outLayer[x] = (uint8_t)curLayer; }
        }
      }
    } else {
      if(!s_lrSpritesAny) continue;                       // sprite-free line
      const uint8_t *obp = ppu->objPriorityBuffer;
      const uint8_t *obx = ppu->objPixelBuffer;
      if(win) {
        for(int x = 0; x < 256; x++) {
          if(win[x] || obp[x] != curPriority) continue;
          const uint8_t v = obx[x];
          // sprites with palette color < 0xc0 are exempt from color math (id 6)
          if(v) { outPix[x] = v; outLayer[x] = (v < 0xc0) ? 6 : 4; }
        }
      } else {
        for(int x = 0; x < 256; x++) {
          if(obp[x] != curPriority) continue;
          const uint8_t v = obx[x];
          if(v) { outPix[x] = v; outLayer[x] = (v < 0xc0) ? 6 : 4; }
        }
      }
    }
  }
}

static void ppu_lrRunLine(Ppu* ppu, int y) {
  // R7: per-line mode-7 starts, BEFORE the row clamp and the forced-blank
  // bail so the m7startX/Y state evolves exactly as on the legacy path
  // (which computes them on blanked lines AND on the clamped vblank-edge
  // line 225, before its per-pixel F9 clamp) -- they are savestate-persisted
  // and hashed into the R4/R5 render signature.
  if(ppu->mode == 7) ppu_calculateMode7Starts(ppu, y);
  const int row = y - 1;                          // evenFrame path (see ppu_lrFastPathOk)
#ifdef FF4_PORT_STATIC_SNES
  if(row < 0 || row >= 224) return;               // same clamp as ppu_handlePixel (F9)
#endif
  uint8_t *out = &ppu->pixelBuffer[row * PPU_PIXELBUF_STRIDE];
  if(ppu->forcedBlank) {                          // original writes a zero line
    ppu->psStoring = false;
    memset(out, 0, 256 * PPU_PIXELBUF_XPITCH);
    return;
  }
  const int actMode = (ppu->mode == 1 && ppu->bg3priority) ? 8
                    : (ppu->mode == 7 && ppu->m7extBg)     ? 9 : ppu->mode;

  s_lrSpritesAny = false;
  for(int x = 0; x < 256; x++) {
    if(ppu->objPixelBuffer[x]) { s_lrSpritesAny = true; break; }
  }

  /* R8: fused decode->output for the dominant mode-7 line. Census
   * (2026-07-11, 008/011/012): FF4's worldmap runs mode 7 with no color
   * math, clip mode 0, no direct color, layer 0 unwindowed on the main
   * screen, no mosaic -- and ~80% of mode-7 lines carry no sprite pixel.
   * For that exact shape the generic pipeline collapses: the composed
   * line is layer-0-over-backdrop, and since backdrop resolves to
   * cgram[0] = s_lrPal3[0], the output bytes are s_lrPal3[pixel] for
   * every pixel, transparent or not. So decode straight into the output
   * (and into the R5 line store when it is recording: flags are all 0
   * here by construction, same as the generic whole-line fast path).
   * Every other mode-7 line falls through to the generic stages below. */
  // sprLine: this line has sprite pixels AND the sprite layer is visible.
  // (spritesAny with layer 4 main-screen-disabled paints nothing -- the
  // plain no-sprite loop is then exact.)
  const bool sprLine7 = s_lrSpritesAny && ppu->layer[4].mainScreenEnabled;
  if(actMode == 7
     && !(ppu->mathEnabled[0] || ppu->mathEnabled[1] || ppu->mathEnabled[2]
       || ppu->mathEnabled[3] || ppu->mathEnabled[4] || ppu->mathEnabled[5])
     && ppu->clipMode == 0 && !ppu->directColor
     && ppu->layer[0].mainScreenEnabled) {
    ppu_lrRefreshPal3(ppu, ppu_lrBright(ppu));
    const bool psStore7 = ppu->psStoring;
    uint8_t *buf = psStore7 ? s_psPix[row] : s_m7Line;  // decode into the R5 store directly
    ppu_m7DecodeLineU8(ppu, buf);
    if(ppu->layer[0].mainScreenWindowed) ppu_lrWinMaskU8(ppu, 0, buf);
    uint8_t *px = out + ppu->pixelOutputFormat;
    if(!sprLine7) {
      for(int x = 0; x < 256; x++) {
        const uint8_t *c3 = s_lrPal3[buf[x]];
        px[0] = c3[0]; px[1] = c3[1]; px[2] = c3[2];
        px += PPU_PIXELBUF_XPITCH;
      }
    } else {
      // R8b: sprite lines fused too. actMode 7's paint order is
      // spr(prio0) < BG < spr(prio1) < spr(prio2) < spr(prio3), and the
      // sprite buffers hold ONE (pixel, priority) pair per x, so the
      // five compose passes collapse to a single select: an opaque,
      // un-window-masked sprite pixel wins unless it has priority 0 AND
      // the BG pixel is opaque. The composed index feeds the same pal3
      // lookup (and the R5 store) that the compose+output stages would
      // produce -- math, clip and direct color are gated off, so layer
      // ids are inert. (FF4 windows the sprite layer exactly like the
      // BG: the [1,254]-inverted edge mask -- hence the mask, not a
      // gate: a gate here sent every sprite line back to the generic
      // path.)
      const uint8_t *obx = ppu->objPixelBuffer;
      const uint8_t *obp = ppu->objPriorityBuffer;
      const bool objWin = ppu->layer[4].mainScreenWindowed;
      if(objWin) ppu_lrWinSpanMask(ppu, 4, s_m7Win);
      for(int x = 0; x < 256; x++) {
        uint8_t pix = buf[x];
        const uint8_t ov = obx[x];
        if(ov && (!objWin || !s_m7Win[x]) && (obp[x] != 0 || pix == 0)) pix = ov;
        if(psStore7) buf[x] = pix;          // R5 store records the composed index
        const uint8_t *c3 = s_lrPal3[pix];
        px[0] = c3[0]; px[1] = c3[1]; px[2] = c3[2];
        px += PPU_PIXELBUF_XPITCH;
      }
    }
    if(psStore7) memset(s_psFlg[row], 0, 128);
#ifdef FF4_M7_CENSUS
    { static uint32_t f; if((++f % 10000) == 1) fprintf(stderr, "R8FUSED %u\n", f); }
#endif
    return;
  }
#ifdef FF4_M7_CENSUS
  if(actMode == 7) { static uint32_t g; if((++g % 2000) == 1) fprintf(stderr,
    "R8GENERAL %u spr=%d mathAny=%d clip=%d dc=%d mainEn=%d mainWin=%d win1=%d l=%d r=%d inv=%d\n", g,
    (int)s_lrSpritesAny,
    (int)(ppu->mathEnabled[0]||ppu->mathEnabled[1]||ppu->mathEnabled[2]||ppu->mathEnabled[3]||ppu->mathEnabled[4]||ppu->mathEnabled[5]),
    ppu->clipMode, (int)ppu->directColor, (int)ppu->layer[0].mainScreenEnabled,
    (int)ppu->layer[0].mainScreenWindowed, (int)ppu->windowLayer[0].window1enabled,
    ppu->window1left, ppu->window1right, (int)ppu->windowLayer[0].window1inversed); }
#endif

  // window membership per window-layer (cheap when both windows disabled).
  // Membership is piecewise-constant in x: whatever the enable/invert/mask
  // combination, it can only change value at a window edge. Evaluate
  // ppu_getWindowState once per span and memset, instead of once per pixel
  // (measured at 34% of the whole render on the M7, R1).
  int winBp[6] = {0, ppu->window1left, ppu->window1right + 1,
                  ppu->window2left, ppu->window2right + 1, 256};
  for(int i = 1; i < 6; i++) {           // insertion sort, 6 entries
    int v = winBp[i], j = i;
    while(j > 0 && winBp[j-1] > v) { winBp[j] = winBp[j-1]; j--; }
    winBp[j] = v;
  }
  for(int l = 0; l < 6; l++) {
    if(!ppu->windowLayer[l].window1enabled && !ppu->windowLayer[l].window2enabled) {
      memset(s_lrWin[l], 0, 256);
    } else {
      for(int i = 0; i < 5; i++) {
        const int s = winBp[i];
        int e = winBp[i + 1];
        if(e > 256) e = 256;
        if(s >= e) continue;             // duplicate/out-of-range breakpoint
        memset(&s_lrWin[l][s], ppu_getWindowState(ppu, l, s) ? 1 : 0, e - s);
      }
    }
  }

  // decode each BG layer used by either screen
  bool bgDecoded[4] = {false, false, false, false};
  for(int i = 0; i < layerCountPerMode[actMode]; i++) {
    const int l = layersPerMode[actMode][i];
    if(l >= 4 || bgDecoded[l]) continue;
    if(ppu->layer[l].mainScreenEnabled || ppu->layer[l].subScreenEnabled) {
      s_lrHasPrio[l][0] = s_lrHasPrio[l][1] = 0;
      if(ppu->mode == 7) ppu_lrDecodeM7Line(ppu, l, y);
      else ppu_lrDecodeBgLine(ppu, l, y);
      bgDecoded[l] = true;
    }
  }
  // brightness LUT: cached across lines/frames (R8), rebuilt on change only
  const uint8_t *bright = ppu_lrBright(ppu);

  ppu_lrComposeLine(ppu, actMode, false, bgDecoded);
  const bool mathAny = ppu->mathEnabled[0] || ppu->mathEnabled[1] || ppu->mathEnabled[2]
                    || ppu->mathEnabled[3] || ppu->mathEnabled[4] || ppu->mathEnabled[5];
  const bool needSub = ppu->addSubscreen && mathAny;
  if(needSub) ppu_lrComposeLine(ppu, actMode, true, bgDecoded);

  // R2a: final palette cache (now via the shared helper, reused by R5)
  ppu_lrRefreshPal3(ppu, bright);
  // direct color can only engage on a mode with an 8bpp layer
  bool dcPossible = false;
  if(ppu->directColor) {
    for(int i = 0; i < layerCountPerMode[actMode]; i++) {
      const int l = layersPerMode[actMode][i];
      if(l < 4 && bitDepthsPerMode[actMode][l] == 8) { dcPossible = true; break; }
    }
  }

  if(ppu->psStoring && dcPossible) ppu->psStoring = false;
  const bool psStore = ppu->psStoring;

  if(!mathAny && ppu->clipMode == 0 && !dcPossible) {
    uint8_t *psp = psStore ? s_psPix[row] : NULL;
    for(int x = 0; x < 256; x++) {
      const uint8_t pi = (uint8_t)s_lrPix[0][x];
      if(psp) psp[x] = pi;
      const uint8_t* c3 = s_lrPal3[pi];
      uint8_t *px = out + x * PPU_PIXELBUF_XPITCH + ppu->pixelOutputFormat;
      px[0] = c3[0];
      px[1] = c3[1];
      px[2] = c3[2];
    }
    if(psStore) memset(s_psFlg[row], 0, 128);
    return;
  }

  // (measured on device: a per-pixel palette shortcut inside this loop
  // COSTS 4 ms/frame on math-heavy lines — the duplicated tests outweigh
  // the savings; only the whole-line fast path above survives)
  uint8_t psFlagPend = 0;
  for(int x = 0; x < 256; x++) {
    const int pixel = s_lrPix[0][x];
    const int layer = s_lrLayer[0][x];
    int r, g, b;
    if(ppu->directColor && layer < 4 && bitDepthsPerMode[actMode][layer] == 8) {
      r = ((pixel & 0x7) << 2) | ((pixel & 0x100) >> 7);
      g = ((pixel & 0x38) >> 1) | ((pixel & 0x200) >> 8);
      b = ((pixel & 0xc0) >> 3) | ((pixel & 0x400) >> 8);
    } else {
      const uint16_t color = ppu->cgram[pixel & 0xff];
      r = color & 0x1f;
      g = (color >> 5) & 0x1f;
      b = (color >> 10) & 0x1f;
    }
    const bool colorWindowState = s_lrWin[5][x] != 0;
    const bool psClipped = ppu->clipMode == 3 ||
       (ppu->clipMode == 2 && colorWindowState) ||
       (ppu->clipMode == 1 && !colorWindowState);
    if(psClipped) {
      r = 0; g = 0; b = 0;
    }
    int secondLayer = 5;
    const bool mathEnabled = layer < 6 && ppu->mathEnabled[layer] && !(
      ppu->preventMathMode == 3 ||
      (ppu->preventMathMode == 2 && colorWindowState) ||
      (ppu->preventMathMode == 1 && !colorWindowState)
    );
    if(psStore) {
      uint8_t f = (psClipped ? PS_F_CLIP : 0) | (mathEnabled ? PS_F_MATH : 0);
      uint8_t sp = 0;
      if(mathEnabled && ppu->addSubscreen) {
        const int sly = s_lrLayer[1][x];
        if(((sly == 6) ? 4 : sly) != 5) { f |= PS_F_SUBV; sp = (uint8_t)s_lrPix[1][x]; }
      }
      s_psPix[row][x] = (uint8_t)pixel;
      s_psSub[row][x] = sp;
      if((x & 1) == 0) psFlagPend = f;
      else s_psFlg[row][x >> 1] = (uint8_t)(psFlagPend | (f << 4));
    }
    int r2 = 0, g2 = 0, b2 = 0;
    if(mathEnabled && ppu->addSubscreen) {
      const int spix = s_lrPix[1][x];
      const int slayer = s_lrLayer[1][x];
      secondLayer = (slayer == 6) ? 4 : slayer;   // math only tests != 5
      if(ppu->directColor && slayer < 4 && bitDepthsPerMode[actMode][slayer] == 8) {
        r2 = ((spix & 0x7) << 2) | ((spix & 0x100) >> 7);
        g2 = ((spix & 0x38) >> 1) | ((spix & 0x200) >> 8);
        b2 = ((spix & 0xc0) >> 3) | ((spix & 0x400) >> 8);
      } else {
        const uint16_t c2 = ppu->cgram[spix & 0xff];
        r2 = c2 & 0x1f;
        g2 = (c2 >> 5) & 0x1f;
        b2 = (c2 >> 10) & 0x1f;
      }
    }
    if(mathEnabled) {
      if(ppu->subtractColor) {
        r -= (ppu->addSubscreen && secondLayer != 5) ? r2 : ppu->fixedColorR;
        g -= (ppu->addSubscreen && secondLayer != 5) ? g2 : ppu->fixedColorG;
        b -= (ppu->addSubscreen && secondLayer != 5) ? b2 : ppu->fixedColorB;
      } else {
        r += (ppu->addSubscreen && secondLayer != 5) ? r2 : ppu->fixedColorR;
        g += (ppu->addSubscreen && secondLayer != 5) ? g2 : ppu->fixedColorG;
        b += (ppu->addSubscreen && secondLayer != 5) ? b2 : ppu->fixedColorB;
      }
      if(ppu->halfColor && (secondLayer != 5 || !ppu->addSubscreen)) {
        r >>= 1; g >>= 1; b >>= 1;
      }
      if(r > 31) r = 31;
      if(g > 31) g = 31;
      if(b > 31) b = 31;
      if(r < 0) r = 0;
      if(g < 0) g = 0;
      if(b < 0) b = 0;
    }
    uint8_t *px = out + x * PPU_PIXELBUF_XPITCH + ppu->pixelOutputFormat;
    px[0] = bright[b];
    px[1] = bright[g];
    px[2] = bright[r];
  }
}

static void ppu_lrPaletteLine(Ppu* ppu, int y) {
  const int row = y - 1;
  if(row < 0 || row >= PS_LINES) return;
  uint8_t *out = &ppu->pixelBuffer[row * PPU_PIXELBUF_STRIDE];
  const uint8_t *bright = ppu_lrBright(ppu);
  ppu_lrRefreshPal3(ppu, bright);
  const uint8_t *pixL = s_psPix[row];
  const uint8_t *subL = s_psSub[row];
  const uint8_t *flgL = s_psFlg[row];
  for(int x = 0; x < 256; x++) {
    const uint8_t f = (uint8_t)((flgL[x >> 1] >> ((x & 1) ? 4 : 0)) & 0x7);
    uint8_t *px = out + x * PPU_PIXELBUF_XPITCH + ppu->pixelOutputFormat;
    if(f == 0) {
      const uint8_t *c3 = s_lrPal3[pixL[x]];
      px[0] = c3[0]; px[1] = c3[1]; px[2] = c3[2];
      continue;
    }
    int r, g, b;
    if(f & PS_F_CLIP) { r = 0; g = 0; b = 0; }
    else {
      const uint16_t color = ppu->cgram[pixL[x]];
      r = color & 0x1f; g = (color >> 5) & 0x1f; b = (color >> 10) & 0x1f;
    }
    if(f & PS_F_MATH) {
      const bool subv = (f & PS_F_SUBV) != 0;
      int r2 = 0, g2 = 0, b2 = 0;
      if(subv) {
        const uint16_t c2 = ppu->cgram[subL[x]];
        r2 = c2 & 0x1f; g2 = (c2 >> 5) & 0x1f; b2 = (c2 >> 10) & 0x1f;
      }
      if(ppu->subtractColor) {
        r -= subv ? r2 : ppu->fixedColorR;
        g -= subv ? g2 : ppu->fixedColorG;
        b -= subv ? b2 : ppu->fixedColorB;
      } else {
        r += subv ? r2 : ppu->fixedColorR;
        g += subv ? g2 : ppu->fixedColorG;
        b += subv ? b2 : ppu->fixedColorB;
      }
      if(ppu->halfColor && (subv || !ppu->addSubscreen)) { r >>= 1; g >>= 1; b >>= 1; }
      if(r > 31) r = 31; if(g > 31) g = 31; if(b > 31) b = 31;
      if(r < 0) r = 0; if(g < 0) g = 0; if(b < 0) b = 0;
    }
    px[0] = bright[b]; px[1] = bright[g]; px[2] = bright[r];
  }
}

static bool ppu_lrFastPathOk(Ppu* ppu) {
  // mode 7 handled since 2026-07-11 (R7): the airship/worldmap scenes used
  // to be the last legacy-renderer scenes -- measured at ~53% of the M7
  // frame in ppu_runLine (D6 + PC-sampling profile, 2026-07-11).
  if(ppu->mode != 0 && ppu->mode != 1 && ppu->mode != 3 && ppu->mode != 7) return false;
  // EXTBG (mode 7 + $2133 bit 6) stays on the legacy path: the lr decoder
  // implements it (layer 1 branch of ppu_lrDecodeM7Line, actMode 9) but NO
  // fixture exercises it (instrumented sweep 2026-07-11: zero actMode-9
  // lines across 008/011/012), so it ships unreachable rather than
  // unvalidated. Lift this gate the day a scene provides golden coverage.
  if(ppu->mode == 7 && ppu->m7extBg) return false;
  if(ppu->pseudoHires || ppu->interlace) return false;
  // mosaic is line-renderer-friendly (base row + horizontal block
  // replication in the decoder) and handled since 2026-07-10 -- it used to
  // force these scenes (battle-entry pixelation, teleports) onto the legacy
  // per-pixel path, which is why the transitions crawled on device.
#ifndef FF4_PORT_STATIC_SNES
  if(!ppu->evenFrame) return false;               // odd-frame row offset not handled
#endif
  return true;
}

void ppu_runLine(Ppu* ppu, int line) {
  // called for lines 1-224/239
  // evaluate sprites
  memset(ppu->objPixelBuffer, 0, sizeof(ppu->objPixelBuffer));
  if(!ppu->forcedBlank) ppu_evaluateSprites(ppu, line - 1);
  /* Frameskip (see ppu.h): sprite evaluation above stays -- its
   * range/time-over flags are game-visible via $213E -- but everything
   * below only feeds pixelBuffer, so a skipped frame bails out here. */
  if(!ff4_ppu_render_enabled) { ppu->psStoring = false; return; }
  if(ppu->skipMode == 1) return;
  if(ppu->skipMode == 2) { ppu_lrPaletteLine(ppu, line); return; }
  if(ppu_lrFastPathOk(ppu)) {
    ppu_lrRunLine(ppu, line);
    return;
  }
  ppu->psStoring = false;   // legacy-path lines are not stored
  // actual line
  if(ppu->mode == 7) ppu_calculateMode7Starts(ppu, line);
  ppu_resetBgCache();
  for(int x = 0; x < 256; x++) {
    ppu_handlePixel(ppu, x, line);
  }
}

void ppu_setPixelOutputFormat(Ppu* ppu, int pixelOutputFormat) {
  ppu->pixelOutputFormat = pixelOutputFormat;
}

static void ppu_handlePixel(Ppu* ppu, int x, int y) {
  int r = 0, r2 = 0;
  int g = 0, g2 = 0;
  int b = 0, b2 = 0;
  if(!ppu->forcedBlank) {
    int mainLayer = ppu_getPixel(ppu, x, y, false, &r, &g, &b);
    bool colorWindowState = ppu_getWindowState(ppu, 5, x);
    if(
      ppu->clipMode == 3 ||
      (ppu->clipMode == 2 && colorWindowState) ||
      (ppu->clipMode == 1 && !colorWindowState)
    ) {
      r = 0;
      g = 0;
      b = 0;
    }
    int secondLayer = 5; // backdrop
    bool mathEnabled = mainLayer < 6 && ppu->mathEnabled[mainLayer] && !(
      ppu->preventMathMode == 3 ||
      (ppu->preventMathMode == 2 && colorWindowState) ||
      (ppu->preventMathMode == 1 && !colorWindowState)
    );
    if((mathEnabled && ppu->addSubscreen) || ppu->pseudoHires || ppu->mode == 5 || ppu->mode == 6) {
      secondLayer = ppu_getPixel(ppu, x, y, true, &r2, &g2, &b2);
    }
    // TODO: subscreen pixels can be clipped to black as well
    // TODO: math for subscreen pixels (add/sub sub to main)
    if(mathEnabled) {
      if(ppu->subtractColor) {
        r -= (ppu->addSubscreen && secondLayer != 5) ? r2 : ppu->fixedColorR;
        g -= (ppu->addSubscreen && secondLayer != 5) ? g2 : ppu->fixedColorG;
        b -= (ppu->addSubscreen && secondLayer != 5) ? b2 : ppu->fixedColorB;
      } else {
        r += (ppu->addSubscreen && secondLayer != 5) ? r2 : ppu->fixedColorR;
        g += (ppu->addSubscreen && secondLayer != 5) ? g2 : ppu->fixedColorG;
        b += (ppu->addSubscreen && secondLayer != 5) ? b2 : ppu->fixedColorB;
      }
      if(ppu->halfColor && (secondLayer != 5 || !ppu->addSubscreen)) {
        r >>= 1;
        g >>= 1;
        b >>= 1;
      }
      if(r > 31) r = 31;
      if(g > 31) g = 31;
      if(b > 31) b = 31;
      if(r < 0) r = 0;
      if(g < 0) g = 0;
      if(b < 0) b = 0;
    }
    if(!(ppu->pseudoHires || ppu->mode == 5 || ppu->mode == 6)) {
      r2 = r; g2 = g; b2 = b;
    }
  }
  int row = (y - 1) + (ppu->evenFrame ? 0 : 239);
#ifdef FF4_PORT_STATIC_SNES
  /* The G&W pixelBuffer holds only 224 rows. A vblank-edge race can
   * call ppu_runLine for line 225 (-> row 224), one row past the end, which
   * corrupts the adjacent Snes (observed clobbering snes->dma -> crash in
   * dma_handleDma when the next DMA fires). Clamp to the buffer; non-visible
   * lines must not be rendered anyway. */
  if (row < 0 || row >= 224) return;
#endif
  ppu->pixelBuffer[row * PPU_PIXELBUF_STRIDE + x * PPU_PIXELBUF_XPITCH + 0 + ppu->pixelOutputFormat] = ((b2 << 3) | (b2 >> 2)) * ppu->brightness / 15;
  ppu->pixelBuffer[row * PPU_PIXELBUF_STRIDE + x * PPU_PIXELBUF_XPITCH + 1 + ppu->pixelOutputFormat] = ((g2 << 3) | (g2 >> 2)) * ppu->brightness / 15;
  ppu->pixelBuffer[row * PPU_PIXELBUF_STRIDE + x * PPU_PIXELBUF_XPITCH + 2 + ppu->pixelOutputFormat] = ((r2 << 3) | (r2 >> 2)) * ppu->brightness / 15;
#ifndef FF4_PORT_STATIC_SNES
  /* Right half of the hires pair -- dropped in the FF4 static build:
   * outside hires it duplicates (b2,g2,r2) above, no reader ever
   * consumed it, and it doubled the buffer (see pixelBuffer in ppu.h). */
  ppu->pixelBuffer[row * 2048 + x * 8 + 4 + ppu->pixelOutputFormat] = ((b << 3) | (b >> 2)) * ppu->brightness / 15;
  ppu->pixelBuffer[row * 2048 + x * 8 + 5 + ppu->pixelOutputFormat] = ((g << 3) | (g >> 2)) * ppu->brightness / 15;
  ppu->pixelBuffer[row * 2048 + x * 8 + 6 + ppu->pixelOutputFormat] = ((r << 3) | (r >> 2)) * ppu->brightness / 15;
#endif
}

static int ppu_getPixel(Ppu* ppu, int x, int y, bool sub, int* r, int* g, int* b) {
  // figure out which color is on this location on main- or subscreen, sets it in r, g, b
  // returns which layer it is: 0-3 for bg layer, 4 or 6 for sprites (depending on palette), 5 for backdrop
  int actMode = ppu->mode == 1 && ppu->bg3priority ? 8 : ppu->mode;
  actMode = ppu->mode == 7 && ppu->m7extBg ? 9 : actMode;
  int layer = 5;
  int pixel = 0;
  for(int i = 0; i < layerCountPerMode[actMode]; i++) {
    int curLayer = layersPerMode[actMode][i];
    int curPriority = prioritysPerMode[actMode][i];
    bool layerActive = false;
    if(!sub) {
      layerActive = ppu->layer[curLayer].mainScreenEnabled && (
        !ppu->layer[curLayer].mainScreenWindowed || !ppu_getWindowState(ppu, curLayer, x)
      );
    } else {
      layerActive = ppu->layer[curLayer].subScreenEnabled && (
        !ppu->layer[curLayer].subScreenWindowed || !ppu_getWindowState(ppu, curLayer, x)
      );
    }
    if(layerActive) {
      if(curLayer < 4) {
        // bg layer
        int lx = x;
        int ly = y;
        if(ppu->bgLayer[curLayer].mosaicEnabled && ppu->mosaicSize > 1) {
          lx -= lx % ppu->mosaicSize;
          ly -= (ly - ppu->mosaicStartLine) % ppu->mosaicSize;
        }
        if(ppu->mode == 7) {
          pixel = ppu_getPixelForMode7(ppu, lx, curLayer, curPriority);
        } else {
          lx += ppu->bgLayer[curLayer].hScroll;
          if(ppu->mode == 5 || ppu->mode == 6) {
            lx *= 2;
            lx += (sub || ppu->bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            if(ppu->interlace) {
              ly *= 2;
              ly += (ppu->evenFrame || ppu->bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            }
          }
          ly += ppu->bgLayer[curLayer].vScroll;
          if(ppu->mode == 2 || ppu->mode == 4 || ppu->mode == 6) {
            ppu_handleOPT(ppu, curLayer, &lx, &ly);
          }
          pixel = ppu_getPixelForBgLayer(
            ppu, lx & 0x3ff, ly & 0x3ff,
            curLayer, curPriority
          );
        }
      } else {
        // get a pixel from the sprite buffer
        pixel = 0;
        if(ppu->objPriorityBuffer[x] == curPriority) pixel = ppu->objPixelBuffer[x];
      }
    }
    if(pixel > 0) {
      layer = curLayer;
      break;
    }
  }
  if(ppu->directColor && layer < 4 && bitDepthsPerMode[actMode][layer] == 8) {
    *r = ((pixel & 0x7) << 2) | ((pixel & 0x100) >> 7);
    *g = ((pixel & 0x38) >> 1) | ((pixel & 0x200) >> 8);
    *b = ((pixel & 0xc0) >> 3) | ((pixel & 0x400) >> 8);
  } else {
    uint16_t color = ppu->cgram[pixel & 0xff];
    *r = color & 0x1f;
    *g = (color >> 5) & 0x1f;
    *b = (color >> 10) & 0x1f;
  }
  if(layer == 4 && pixel < 0xc0) layer = 6; // sprites with palette color < 0xc0
  return layer;
}

static void ppu_handleOPT(Ppu* ppu, int layer, int* lx, int* ly) {
  int x = *lx;
  int y = *ly;
  int column = 0;
  if(ppu->mode == 6) {
    column = ((x - (x & 0xf)) - ((ppu->bgLayer[layer].hScroll * 2) & 0xfff0)) >> 4;
  } else {
    column = ((x - (x & 0x7)) - (ppu->bgLayer[layer].hScroll & 0xfff8)) >> 3;
  }
  if(column > 0) {
    // fetch offset values from layer 3 tilemap
    int valid = layer == 0 ? 0x2000 : 0x4000;
    uint16_t hOffset = ppu_getOffsetValue(ppu, column - 1, 0);
    uint16_t vOffset = 0;
    if(ppu->mode == 4) {
      if(hOffset & 0x8000) {
        vOffset = hOffset;
        hOffset = 0;
      }
    } else {
      vOffset = ppu_getOffsetValue(ppu, column - 1, 1);
    }
    if(ppu->mode == 6) {
      // TODO: not sure if correct
      if(hOffset & valid) *lx = (((hOffset & 0x3f8) + (column * 8)) * 2) | (x & 0xf);
    } else {
      if(hOffset & valid) *lx = ((hOffset & 0x3f8) + (column * 8)) | (x & 0x7);
    }
    // TODO: not sure if correct for interlace
    if(vOffset & valid) *ly = (vOffset & 0x3ff) + (y - ppu->bgLayer[layer].vScroll);
  }
}

static uint16_t ppu_getOffsetValue(Ppu* ppu, int col, int row) {
  int x = col * 8 + ppu->bgLayer[2].hScroll;
  int y = row * 8 + ppu->bgLayer[2].vScroll;
  int tileBits = ppu->bgLayer[2].bigTiles ? 4 : 3;
  int tileHighBit = ppu->bgLayer[2].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = ppu->bgLayer[2].tilemapAdr + (((y >> tileBits) & 0x1f) << 5 | ((x >> tileBits) & 0x1f));
  if((x & tileHighBit) && ppu->bgLayer[2].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBit) && ppu->bgLayer[2].tilemapHigher) tilemapAdr += ppu->bgLayer[2].tilemapWider ? 0x800 : 0x400;
  return ppu->vram[tilemapAdr & 0x7fff];
}

static int ppu_getPixelForBgLayer(Ppu* ppu, int x, int y, int layer, bool priority) {
  // figure out address of tilemap word and read it
  bool wideTiles = ppu->bgLayer[layer].bigTiles || ppu->mode == 5 || ppu->mode == 6;
  int tileBitsX = wideTiles ? 4 : 3;
  int tileHighBitX = wideTiles ? 0x200 : 0x100;
  int tileBitsY = ppu->bgLayer[layer].bigTiles ? 4 : 3;
  int tileHighBitY = ppu->bgLayer[layer].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = ppu->bgLayer[layer].tilemapAdr + (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));
  if((x & tileHighBitX) && ppu->bgLayer[layer].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBitY) && ppu->bgLayer[layer].tilemapHigher) tilemapAdr += ppu->bgLayer[layer].tilemapWider ? 0x800 : 0x400;
  uint16_t tmAdr = tilemapAdr & 0x7fff;
  uint16_t tile;
  if(s_bgTilemapAdr[layer] == tmAdr) {
    tile = s_bgTile[layer];
  } else {
    tile = ppu->vram[tmAdr];
    s_bgTilemapAdr[layer] = tmAdr;
    s_bgTile[layer] = tile;
  }
  // check priority, get palette
  if(((bool) (tile & 0x2000)) != priority) return 0; // wrong priority
  int paletteNum = (tile & 0x1c00) >> 10;
  // figure out position within tile
  int row = (tile & 0x8000) ? 7 - (y & 0x7) : (y & 0x7);
  int col = (tile & 0x4000) ? (x & 0x7) : 7 - (x & 0x7);
  int tileNum = tile & 0x3ff;
  if(wideTiles) {
    // if unflipped right half of tile, or flipped left half of tile
    if(((bool) (x & 8)) ^ ((bool) (tile & 0x4000))) tileNum += 1;
  }
  if(ppu->bgLayer[layer].bigTiles) {
    // if unflipped bottom half of tile, or flipped upper half of tile
    if(((bool) (y & 8)) ^ ((bool) (tile & 0x8000))) tileNum += 0x10;
  }
  // read tiledata, ajust palette for mode 0
  int bitDepth = bitDepthsPerMode[ppu->mode][layer];
  if(ppu->mode == 0) paletteNum += 8 * layer;
  int paletteSize = 4;
  // The plane words (8 horizontal pixels each) are constant across a tile
  // span; fetch them once per (layer, tileNum, row) and memoise — keyed on
  // the masked plane-1 address (planes 2-4 are at fixed +8/+16/+24 offsets
  // derived from the same base, so the same key covers them). Only the per-
  // pixel bit-extraction below runs every pixel.
  uint16_t planeAdr = (ppu->bgLayer[layer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth) + row) & 0x7fff;
  uint16_t plane1, plane2 = 0, plane3 = 0, plane4 = 0;
  if(s_bgPlaneAdr[layer] == planeAdr) {
    plane1 = s_bgPlane[layer][0]; plane2 = s_bgPlane[layer][1];
    plane3 = s_bgPlane[layer][2]; plane4 = s_bgPlane[layer][3];
  } else {
    plane1 = ppu->vram[planeAdr];
    if(bitDepth > 2) plane2 = ppu->vram[(planeAdr + 8) & 0x7fff];
    if(bitDepth > 4) {
      plane3 = ppu->vram[(planeAdr + 16) & 0x7fff];
      plane4 = ppu->vram[(planeAdr + 24) & 0x7fff];
    }
    s_bgPlaneAdr[layer] = planeAdr;
    s_bgPlane[layer][0] = plane1; s_bgPlane[layer][1] = plane2;
    s_bgPlane[layer][2] = plane3; s_bgPlane[layer][3] = plane4;
  }
  // plane 1 (always)
  int pixel = (plane1 >> col) & 1;
  pixel |= ((plane1 >> (8 + col)) & 1) << 1;
  // plane 2 (for 4bpp, 8bpp)
  if(bitDepth > 2) {
    paletteSize = 16;
    pixel |= ((plane2 >> col) & 1) << 2;
    pixel |= ((plane2 >> (8 + col)) & 1) << 3;
  }
  // plane 3 & 4 (for 8bpp)
  if(bitDepth > 4) {
    paletteSize = 256;
    pixel |= ((plane3 >> col) & 1) << 4;
    pixel |= ((plane3 >> (8 + col)) & 1) << 5;
    pixel |= ((plane4 >> col) & 1) << 6;
    pixel |= ((plane4 >> (8 + col)) & 1) << 7;
  }
  // return cgram index, or 0 if transparent, palette number in bits 10-8 for 8-color layers
  return pixel == 0 ? 0 : paletteSize * paletteNum + pixel;
}

static void ppu_calculateMode7Starts(Ppu* ppu, int y) {
  // expand 13-bit values to signed values
  int hScroll = ((int16_t) (ppu->m7matrix[6] << 3)) >> 3;
  int vScroll = ((int16_t) (ppu->m7matrix[7] << 3)) >> 3;
  int xCenter = ((int16_t) (ppu->m7matrix[4] << 3)) >> 3;
  int yCenter = ((int16_t) (ppu->m7matrix[5] << 3)) >> 3;
  // do calculation
  int clippedH = hScroll - xCenter;
  int clippedV = vScroll - yCenter;
  clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
  clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);
  if(ppu->bgLayer[0].mosaicEnabled && ppu->mosaicSize > 1) {
    y -= (y - ppu->mosaicStartLine) % ppu->mosaicSize;
  }
  uint8_t ry = ppu->m7yFlip ? 255 - y : y;
  ppu->m7startX = (
    ((ppu->m7matrix[0] * clippedH) & ~63) +
    ((ppu->m7matrix[1] * ry) & ~63) +
    ((ppu->m7matrix[1] * clippedV) & ~63) +
    (xCenter << 8)
  );
  ppu->m7startY = (
    ((ppu->m7matrix[2] * clippedH) & ~63) +
    ((ppu->m7matrix[3] * ry) & ~63) +
    ((ppu->m7matrix[3] * clippedV) & ~63) +
    (yCenter << 8)
  );
}

static int ppu_getPixelForMode7(Ppu* ppu, int x, int layer, bool priority) {
  uint8_t rx = ppu->m7xFlip ? 255 - x : x;
  int xPos = (ppu->m7startX + ppu->m7matrix[0] * rx) >> 8;
  int yPos = (ppu->m7startY + ppu->m7matrix[2] * rx) >> 8;
  bool outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;
  xPos &= 0x3ff;
  yPos &= 0x3ff;
  if(!ppu->m7largeField) outsideMap = false;
  uint8_t tile = outsideMap ? 0 : ppu->vram[(yPos >> 3) * 128 + (xPos >> 3)] & 0xff;
  uint8_t pixel = outsideMap && !ppu->m7charFill ? 0 : ppu->vram[tile * 64 + (yPos & 7) * 8 + (xPos & 7)] >> 8;
  if(layer == 1) {
    if(((bool) (pixel & 0x80)) != priority) return 0;
    return pixel & 0x7f;
  }
  return pixel;
}

static bool ppu_getWindowState(Ppu* ppu, int layer, int x) {
  if(!ppu->windowLayer[layer].window1enabled && !ppu->windowLayer[layer].window2enabled) {
    return false;
  }
  if(ppu->windowLayer[layer].window1enabled && !ppu->windowLayer[layer].window2enabled) {
    bool test = x >= ppu->window1left && x <= ppu->window1right;
    return ppu->windowLayer[layer].window1inversed ? !test : test;
  }
  if(!ppu->windowLayer[layer].window1enabled && ppu->windowLayer[layer].window2enabled) {
    bool test = x >= ppu->window2left && x <= ppu->window2right;
    return ppu->windowLayer[layer].window2inversed ? !test : test;
  }
  bool test1 = x >= ppu->window1left && x <= ppu->window1right;
  bool test2 = x >= ppu->window2left && x <= ppu->window2right;
  if(ppu->windowLayer[layer].window1inversed) test1 = !test1;
  if(ppu->windowLayer[layer].window2inversed) test2 = !test2;
  switch(ppu->windowLayer[layer].maskLogic) {
    case 0: return test1 || test2;
    case 1: return test1 && test2;
    case 2: return test1 != test2;
    case 3: return test1 == test2;
  }
  return false;
}

static void ppu_evaluateSprites(Ppu* ppu, int line) {
  // TODO: rectangular sprites, wierdness with sprites at -256
  uint8_t index = ppu->objPriority ? (ppu->oamAdr & 0xfe) : 0;
  int spritesFound = 0;
  int tilesFound = 0;
  uint8_t foundSprites[32] = {};
  // iterate over oam to find sprites in range
  for(int i = 0; i < 128; i++) {
    uint8_t y = ppu->oam[index] >> 8;
    // check if the sprite is on this line and get the sprite size
    uint8_t row = line - y;
    int spriteSize = spriteSizes[ppu->objSize][(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int spriteHeight = ppu->objInterlace ? spriteSize / 2 : spriteSize;
    if(row < spriteHeight) {
      // in y-range, get the x location, using the high bit as well
      int x = ppu->oam[index] & 0xff;
      x |= ((ppu->highOam[index >> 3] >> (index & 7)) & 1) << 8;
      if(x > 255) x -= 512;
      // if in x-range, record
      if(x > -spriteSize) {
        // break if we found 32 sprites already
        spritesFound++;
        if(spritesFound > 32) {
          ppu->rangeOver = true;
          spritesFound = 32;
          break;
        }
        foundSprites[spritesFound - 1] = index;
      }
    }
    index += 2;
  }
  // iterate over found sprites backwards to fetch max 34 tile slivers
  for(int i = spritesFound; i > 0; i--) {
    index = foundSprites[i - 1];
    uint8_t y = ppu->oam[index] >> 8;
    uint8_t row = line - y;
    int spriteSize = spriteSizes[ppu->objSize][(ppu->highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int x = ppu->oam[index] & 0xff;
    x |= ((ppu->highOam[index >> 3] >> (index & 7)) & 1) << 8;
    if(x > 255) x -= 512;
    if(x > -spriteSize) {
      // update row according to obj-interlace
      if(ppu->objInterlace) row = row * 2 + (ppu->evenFrame ? 0 : 1);
      // get some data for the sprite and y-flip row if needed
      int tile = ppu->oam[index + 1] & 0xff;
      int palette = (ppu->oam[index + 1] & 0xe00) >> 9;
      bool hFlipped = ppu->oam[index + 1] & 0x4000;
      if(ppu->oam[index + 1] & 0x8000) row = spriteSize - 1 - row;
      // fetch all tiles in x-range
      for(int col = 0; col < spriteSize; col += 8) {
        if(col + x > -8 && col + x < 256) {
          // break if we found > 34 8*1 slivers already
          tilesFound++;
          if(tilesFound > 34) {
            ppu->timeOver = true;
            break;
          }
          // figure out which tile this uses, looping within 16x16 pages, and get it's data
          int usedCol = hFlipped ? spriteSize - 1 - col : col;
          uint8_t usedTile = (((tile >> 4) + (row / 8)) << 4) | (((tile & 0xf) + (usedCol / 8)) & 0xf);
          uint16_t objAdr = (ppu->oam[index + 1] & 0x100) ? ppu->objTileAdr2 : ppu->objTileAdr1;
          uint16_t plane1 = ppu->vram[(objAdr + usedTile * 16 + (row & 0x7)) & 0x7fff];
          uint16_t plane2 = ppu->vram[(objAdr + usedTile * 16 + 8 + (row & 0x7)) & 0x7fff];
          // go over each pixel
          for(int px = 0; px < 8; px++) {
            int shift = hFlipped ? px : 7 - px;
            int pixel = (plane1 >> shift) & 1;
            pixel |= ((plane1 >> (8 + shift)) & 1) << 1;
            pixel |= ((plane2 >> shift) & 1) << 2;
            pixel |= ((plane2 >> (8 + shift)) & 1) << 3;
            // draw it in the buffer if there is a pixel here
            int screenCol = col + x + px;
            if(pixel > 0 && screenCol >= 0 && screenCol < 256) {
              ppu->objPixelBuffer[screenCol] = 0x80 + 16 * palette + pixel;
              ppu->objPriorityBuffer[screenCol] = (ppu->oam[index + 1] & 0x3000) >> 12;
            }
          }
        }
      }
      if(tilesFound > 34) break; // break out of sprite-loop if max tiles found
    }
  }
}

static uint16_t ppu_getVramRemap(Ppu* ppu) {
  uint16_t adr = ppu->vramPointer;
  switch(ppu->vramRemapMode) {
    case 0: return adr;
    case 1: return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
    case 2: return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
    case 3: return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
  }
  return adr;
}

uint8_t ppu_read(Ppu* ppu, uint8_t adr) {
  switch(adr) {
    case 0x04: case 0x14: case 0x24:
    case 0x05: case 0x15: case 0x25:
    case 0x06: case 0x16: case 0x26:
    case 0x08: case 0x18: case 0x28:
    case 0x09: case 0x19: case 0x29:
    case 0x0a: case 0x1a: case 0x2a: {
      return ppu->ppu1openBus;
    }
    case 0x34:
    case 0x35:
    case 0x36: {
      int result = ppu->m7matrix[0] * (ppu->m7matrix[1] >> 8);
      ppu->ppu1openBus = (result >> (8 * (adr - 0x34))) & 0xff;
      return ppu->ppu1openBus;
    }
    case 0x37: {
      // TODO: only when ppulatch is set
      ppu->hCount = ppu->snes->hPos / 4;
      ppu->vCount = ppu->snes->vPos;
      ppu->countersLatched = true;
      return ppu->snes->openBus;
    }
    case 0x38: {
      uint8_t ret = 0;
      if(ppu->oamInHigh) {
        ret = ppu->highOam[((ppu->oamAdr & 0xf) << 1) | ppu->oamSecondWrite];
        if(ppu->oamSecondWrite) {
          ppu->oamAdr++;
          if(ppu->oamAdr == 0) ppu->oamInHigh = false;
        }
      } else {
        if(!ppu->oamSecondWrite) {
          ret = ppu->oam[ppu->oamAdr] & 0xff;
        } else {
          ret = ppu->oam[ppu->oamAdr++] >> 8;
          if(ppu->oamAdr == 0) ppu->oamInHigh = true;
        }
      }
      ppu->oamSecondWrite = !ppu->oamSecondWrite;
      ppu->ppu1openBus = ret;
      return ret;
    }
    case 0x39: {
      uint16_t val = ppu->vramReadBuffer;
      if(!ppu->vramIncrementOnHigh) {
        ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
        ppu->vramPointer += ppu->vramIncrement;
      }
      ppu->ppu1openBus = val & 0xff;
      return val & 0xff;
    }
    case 0x3a: {
      uint16_t val = ppu->vramReadBuffer;
      if(ppu->vramIncrementOnHigh) {
        ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
        ppu->vramPointer += ppu->vramIncrement;
      }
      ppu->ppu1openBus = val >> 8;
      return val >> 8;
    }
    case 0x3b: {
      uint8_t ret = 0;
      if(!ppu->cgramSecondWrite) {
        ret = ppu->cgram[ppu->cgramPointer] & 0xff;
      } else {
        ret = ((ppu->cgram[ppu->cgramPointer++] >> 8) & 0x7f) | (ppu->ppu2openBus & 0x80);
      }
      ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
      ppu->ppu2openBus = ret;
      return ret;
    }
    case 0x3c: {
      uint8_t val = 0;
      if(ppu->hCountSecond) {
        val = ((ppu->hCount >> 8) & 1) | (ppu->ppu2openBus & 0xfe);
      } else {
        val = ppu->hCount & 0xff;
      }
      ppu->hCountSecond = !ppu->hCountSecond;
      ppu->ppu2openBus = val;
      return val;
    }
    case 0x3d: {
      uint8_t val = 0;
      if(ppu->vCountSecond) {
        val = ((ppu->vCount >> 8) & 1) | (ppu->ppu2openBus & 0xfe);
      } else {
        val = ppu->vCount & 0xff;
      }
      ppu->vCountSecond = !ppu->vCountSecond;
      ppu->ppu2openBus = val;
      return val;
    }
    case 0x3e: {
      uint8_t val = 0x1; // ppu1 version (4 bit)
      val |= ppu->ppu1openBus & 0x10;
      val |= ppu->rangeOver << 6;
      val |= ppu->timeOver << 7;
      ppu->ppu1openBus = val;
      return val;
    }
    case 0x3f: {
      uint8_t val = 0x3; // ppu2 version (4 bit)
      val |= ppu->snes->palTiming << 4; // ntsc/pal
      val |= ppu->ppu2openBus & 0x20;
      val |= ppu->countersLatched << 6;
      val |= ppu->evenFrame << 7;
      ppu->countersLatched = false; // TODO: only when ppulatch is set
      ppu->hCountSecond = false;
      ppu->vCountSecond = false;
      ppu->ppu2openBus = val;
      return val;
    }
    default: {
      return ppu->snes->openBus;
    }
  }
}

void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val) {
  // R4: a PPU write on a visible scanline can change pixels mid-frame
  // (raster effect); such a frame is not summarized by its start-of-frame
  // signature, so bar the next frame from skipping against it.
  { const uint16_t vp = ppu->snes->vPos;
    if(vp >= 1 && vp <= 224) {
      ppu->skipRasterWrite = true;
      if(ppu->skipMode) { ppu->skipMode = 0; ppu->psValid = false; }
      ppu->psStoring = false;
    } }
  switch(adr) {
    case 0x00: {
      // TODO: oam address reset when written on first line of vblank, (and when forced blank is disabled?)
      ppu->brightness = val & 0xf;
      ppu->forcedBlank = val & 0x80;
      break;
    }
    case 0x01: {
      ppu->objSize = val >> 5;
      ppu->objTileAdr1 = (val & 7) << 13;
      ppu->objTileAdr2 = ppu->objTileAdr1 + (((val & 0x18) + 8) << 9);
      break;
    }
    case 0x02: {
      ppu->oamAdr = val;
      ppu->oamAdrWritten = ppu->oamAdr;
      ppu->oamInHigh = ppu->oamInHighWritten;
      ppu->oamSecondWrite = false;
      break;
    }
    case 0x03: {
      ppu->objPriority = val & 0x80;
      ppu->oamInHigh = val & 1;
      ppu->oamInHighWritten = ppu->oamInHigh;
      ppu->oamAdr = ppu->oamAdrWritten;
      ppu->oamSecondWrite = false;
      break;
    }
    case 0x04: {
      if(ppu->oamInHigh) {
        ppu->highOam[((ppu->oamAdr & 0xf) << 1) | ppu->oamSecondWrite] = val;
        if(ppu->oamSecondWrite) {
          ppu->oamAdr++;
          if(ppu->oamAdr == 0) ppu->oamInHigh = false;
        }
      } else {
        if(!ppu->oamSecondWrite) {
          ppu->oamBuffer = val;
        } else {
          ppu->oam[ppu->oamAdr++] = (val << 8) | ppu->oamBuffer;
          if(ppu->oamAdr == 0) ppu->oamInHigh = true;
        }
      }
      ppu->oamSecondWrite = !ppu->oamSecondWrite;
      break;
    }
    case 0x05: {
      ppu->mode = val & 0x7;
      ppu->bg3priority = val & 0x8;
      ppu->bgLayer[0].bigTiles = val & 0x10;
      ppu->bgLayer[1].bigTiles = val & 0x20;
      ppu->bgLayer[2].bigTiles = val & 0x40;
      ppu->bgLayer[3].bigTiles = val & 0x80;
      break;
    }
    case 0x06: {
      // TODO: mosaic line reset specifics
      ppu->bgLayer[0].mosaicEnabled = val & 0x1;
      ppu->bgLayer[1].mosaicEnabled = val & 0x2;
      ppu->bgLayer[2].mosaicEnabled = val & 0x4;
      ppu->bgLayer[3].mosaicEnabled = val & 0x8;
      ppu->mosaicSize = (val >> 4) + 1;
      ppu->mosaicStartLine = ppu->snes->vPos;
      break;
    }
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a: {
      ppu->bgLayer[adr - 7].tilemapWider = val & 0x1;
      ppu->bgLayer[adr - 7].tilemapHigher = val & 0x2;
      ppu->bgLayer[adr - 7].tilemapAdr = (val & 0xfc) << 8;
      break;
    }
    case 0x0b: {
      ppu->bgLayer[0].tileAdr = (val & 0xf) << 12;
      ppu->bgLayer[1].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0c: {
      ppu->bgLayer[2].tileAdr = (val & 0xf) << 12;
      ppu->bgLayer[3].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0d: {
      ppu->m7matrix[6] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      // fallthrough to normal layer BG-HOFS
    }
    case 0x0f:
    case 0x11:
    case 0x13: {
      ppu->bgLayer[(adr - 0xd) / 2].hScroll = ((val << 8) | (ppu->scrollPrev & 0xf8) | (ppu->scrollPrev2 & 0x7)) & 0x3ff;
      ppu->scrollPrev = val;
      ppu->scrollPrev2 = val;
      break;
    }
    case 0x0e: {
      ppu->m7matrix[7] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      // fallthrough to normal layer BG-VOFS
    }
    case 0x10:
    case 0x12:
    case 0x14: {
      ppu->bgLayer[(adr - 0xe) / 2].vScroll = ((val << 8) | ppu->scrollPrev) & 0x3ff;
      ppu->scrollPrev = val;
      break;
    }
    case 0x15: {
      if((val & 3) == 0) {
        ppu->vramIncrement = 1;
      } else if((val & 3) == 1) {
        ppu->vramIncrement = 32;
      } else {
        ppu->vramIncrement = 128;
      }
      ppu->vramRemapMode = (val & 0xc) >> 2;
      ppu->vramIncrementOnHigh = val & 0x80;
      break;
    }
    case 0x16: {
      ppu->vramPointer = (ppu->vramPointer & 0xff00) | val;
      ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
      break;
    }
    case 0x17: {
      ppu->vramPointer = (ppu->vramPointer & 0x00ff) | (val << 8);
      ppu->vramReadBuffer = ppu->vram[ppu_getVramRemap(ppu) & 0x7fff];
      break;
    }
    case 0x18: {
      // TODO: vram access during rendering (also cgram and oam)
      uint16_t vramAdr = ppu_getVramRemap(ppu);
      ppu->vram[vramAdr & 0x7fff] = (ppu->vram[vramAdr & 0x7fff] & 0xff00) | val;
      ppu->vramGen++;
      if(!ppu->vramIncrementOnHigh) ppu->vramPointer += ppu->vramIncrement;
      break;
    }
    case 0x19: {
      uint16_t vramAdr = ppu_getVramRemap(ppu);
      ppu->vram[vramAdr & 0x7fff] = (ppu->vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);
      ppu->vramGen++;
      if(ppu->vramIncrementOnHigh) ppu->vramPointer += ppu->vramIncrement;
      break;
    }
    case 0x1a: {
      ppu->m7largeField = val & 0x80;
      ppu->m7charFill = val & 0x40;
      ppu->m7yFlip = val & 0x2;
      ppu->m7xFlip = val & 0x1;
      break;
    }
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e: {
      ppu->m7matrix[adr - 0x1b] = (val << 8) | ppu->m7prev;
      ppu->m7prev = val;
      break;
    }
    case 0x1f:
    case 0x20: {
      ppu->m7matrix[adr - 0x1b] = ((val << 8) | ppu->m7prev) & 0x1fff;
      ppu->m7prev = val;
      break;
    }
    case 0x21: {
      ppu->cgramPointer = val;
      ppu->cgramSecondWrite = false;
      break;
    }
    case 0x22: {
      if(!ppu->cgramSecondWrite) {
        ppu->cgramBuffer = val;
      } else {
        ppu->cgram[ppu->cgramPointer++] = (val << 8) | ppu->cgramBuffer;
        ppu->cgramGen++;
      }
      ppu->cgramSecondWrite = !ppu->cgramSecondWrite;
      break;
    }
    case 0x23:
    case 0x24:
    case 0x25: {
      ppu->windowLayer[(adr - 0x23) * 2].window1inversed = val & 0x1;
      ppu->windowLayer[(adr - 0x23) * 2].window1enabled = val & 0x2;
      ppu->windowLayer[(adr - 0x23) * 2].window2inversed = val & 0x4;
      ppu->windowLayer[(adr - 0x23) * 2].window2enabled = val & 0x8;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window1inversed = val & 0x10;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window1enabled = val & 0x20;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window2inversed = val & 0x40;
      ppu->windowLayer[(adr - 0x23) * 2 + 1].window2enabled = val & 0x80;
      break;
    }
    case 0x26: {
      ppu->window1left = val;
      break;
    }
    case 0x27: {
      ppu->window1right = val;
      break;
    }
    case 0x28: {
      ppu->window2left = val;
      break;
    }
    case 0x29: {
      ppu->window2right = val;
      break;
    }
    case 0x2a: {
      ppu->windowLayer[0].maskLogic = val & 0x3;
      ppu->windowLayer[1].maskLogic = (val >> 2) & 0x3;
      ppu->windowLayer[2].maskLogic = (val >> 4) & 0x3;
      ppu->windowLayer[3].maskLogic = (val >> 6) & 0x3;
      break;
    }
    case 0x2b: {
      ppu->windowLayer[4].maskLogic = val & 0x3;
      ppu->windowLayer[5].maskLogic = (val >> 2) & 0x3;
      break;
    }
    case 0x2c: {
      ppu->layer[0].mainScreenEnabled = val & 0x1;
      ppu->layer[1].mainScreenEnabled = val & 0x2;
      ppu->layer[2].mainScreenEnabled = val & 0x4;
      ppu->layer[3].mainScreenEnabled = val & 0x8;
      ppu->layer[4].mainScreenEnabled = val & 0x10;
      break;
    }
    case 0x2d: {
      ppu->layer[0].subScreenEnabled = val & 0x1;
      ppu->layer[1].subScreenEnabled = val & 0x2;
      ppu->layer[2].subScreenEnabled = val & 0x4;
      ppu->layer[3].subScreenEnabled = val & 0x8;
      ppu->layer[4].subScreenEnabled = val & 0x10;
      break;
    }
    case 0x2e: {
      ppu->layer[0].mainScreenWindowed = val & 0x1;
      ppu->layer[1].mainScreenWindowed = val & 0x2;
      ppu->layer[2].mainScreenWindowed = val & 0x4;
      ppu->layer[3].mainScreenWindowed = val & 0x8;
      ppu->layer[4].mainScreenWindowed = val & 0x10;
      break;
    }
    case 0x2f: {
      ppu->layer[0].subScreenWindowed = val & 0x1;
      ppu->layer[1].subScreenWindowed = val & 0x2;
      ppu->layer[2].subScreenWindowed = val & 0x4;
      ppu->layer[3].subScreenWindowed = val & 0x8;
      ppu->layer[4].subScreenWindowed = val & 0x10;
      break;
    }
    case 0x30: {
      ppu->directColor = val & 0x1;
      ppu->addSubscreen = val & 0x2;
      ppu->preventMathMode = (val & 0x30) >> 4;
      ppu->clipMode = (val & 0xc0) >> 6;
      break;
    }
    case 0x31: {
      ppu->subtractColor = val & 0x80;
      ppu->halfColor = val & 0x40;
      for(int i = 0; i < 6; i++) {
        ppu->mathEnabled[i] = val & (1 << i);
      }
      break;
    }
    case 0x32: {
      if(val & 0x80) ppu->fixedColorB = val & 0x1f;
      if(val & 0x40) ppu->fixedColorG = val & 0x1f;
      if(val & 0x20) ppu->fixedColorR = val & 0x1f;
      break;
    }
    case 0x33: {
      ppu->interlace = val & 0x1;
      ppu->objInterlace = val & 0x2;
      ppu->overscan = val & 0x4;
      ppu->pseudoHires = val & 0x8;
      ppu->m7extBg = val & 0x40;
      break;
    }
    default: {
      break;
    }
  }
}

void ppu_putPixels(Ppu* ppu, uint8_t* pixels) {
#ifdef FF4_PORT_STATIC_SNES
  /* Dead in the FF4 port: no caller (device and desktop both read the
   * frame through ff4_blit_to_lcd), and the stock 2048-stride / row+239
   * indexing below no longer matches the shrunk pixelBuffer layout
   * (see ppu.h). Kept as a stub so the upstream API surface stays. */
  (void)ppu; (void)pixels;
#else
  for(int y = 0; y < (ppu->frameOverscan ? 239 : 224); y++) {
    int dest = y * 2 + (ppu->frameOverscan ? 2 : 16);
    int y1 = y, y2 = y + 239;
    if(!ppu->frameInterlace) {
      y1 = y + (ppu->evenFrame ? 0 : 239);
      y2 = y1;
    }
    memcpy(pixels + (dest * 2048), &ppu->pixelBuffer[y1 * 2048], 2048);
    memcpy(pixels + ((dest + 1) * 2048), &ppu->pixelBuffer[y2 * 2048], 2048);
  }
  // clear top 2 lines, and following 14 and last 16 lines if not overscanning
  memset(pixels, 0, 2048 * 2);
  if(!ppu->frameOverscan) {
    memset(pixels + (2 * 2048), 0, 2048 * 14);
    memset(pixels + (464 * 2048), 0, 2048 * 16);
  }
#endif
}
