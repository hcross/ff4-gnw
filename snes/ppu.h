
#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Ppu Ppu;

#include "snes.h"
#include "statehandler.h"

typedef struct BgLayer {
  uint16_t hScroll;
  uint16_t vScroll;
  bool tilemapWider;
  bool tilemapHigher;
  uint16_t tilemapAdr;
  uint16_t tileAdr;
  bool bigTiles;
  bool mosaicEnabled;
} BgLayer;

typedef struct Layer {
  bool mainScreenEnabled;
  bool subScreenEnabled;
  bool mainScreenWindowed;
  bool subScreenWindowed;
} Layer;

typedef struct WindowLayer {
  bool window1enabled;
  bool window2enabled;
  bool window1inversed;
  bool window2inversed;
  uint8_t maskLogic;
} WindowLayer;

struct Ppu {
  Snes* snes;
  // vram access
  uint16_t vram[0x8000];
  // derived, never serialized: bumped on every vram write, reset and state
  // load, so the line-renderer decoded-tile-row cache can invalidate on any
  // vram change (R2b)
  uint32_t vramGen;
  uint16_t vramPointer;
  bool vramIncrementOnHigh;
  uint16_t vramIncrement;
  uint8_t vramRemapMode;
  uint16_t vramReadBuffer;
  // cgram access
  uint16_t cgram[0x100];
  // derived, never serialized: bumped on every cgram write (and forced on
  // state load) so line-renderer palette caches can key on it (R2a)
  uint32_t cgramGen;
  uint8_t cgramPointer;
  bool cgramSecondWrite;
  uint8_t cgramBuffer;
  // oam access
  uint16_t oam[0x100];
  uint8_t highOam[0x20];
  uint8_t oamAdr;
  uint8_t oamAdrWritten;
  bool oamInHigh;
  bool oamInHighWritten;
  bool oamSecondWrite;
  uint8_t oamBuffer;
  // object/sprites
  bool objPriority;
  uint16_t objTileAdr1;
  uint16_t objTileAdr2;
  uint8_t objSize;
  uint8_t objPixelBuffer[256]; // line buffers
  uint8_t objPriorityBuffer[256];
  bool timeOver;
  bool rangeOver;
  bool objInterlace;
  // background layers
  BgLayer bgLayer[4];
  uint8_t scrollPrev;
  uint8_t scrollPrev2;
  uint8_t mosaicSize;
  uint8_t mosaicStartLine;
  // layers
  Layer layer[5];
  // mode 7
  int16_t m7matrix[8]; // a, b, c, d, x, y, h, v
  uint8_t m7prev;
  bool m7largeField;
  bool m7charFill;
  bool m7xFlip;
  bool m7yFlip;
  bool m7extBg;
  // mode 7 internal
  int32_t m7startX;
  int32_t m7startY;
  // windows
  WindowLayer windowLayer[6];
  uint8_t window1left;
  uint8_t window1right;
  uint8_t window2left;
  uint8_t window2right;
  // color math
  uint8_t clipMode;
  uint8_t preventMathMode;
  bool addSubscreen;
  bool subtractColor;
  bool halfColor;
  bool mathEnabled[6];
  uint8_t fixedColorR;
  uint8_t fixedColorG;
  uint8_t fixedColorB;
  // settings
  bool forcedBlank;
  uint8_t brightness;
  uint8_t mode;
  bool bg3priority;
  bool evenFrame;
  bool pseudoHires;
  bool overscan;
  bool frameOverscan; // if we are overscanning this frame (determined at 0,225)
  bool interlace;
  bool frameInterlace; // if we are interlacing this frame (determined at start vblank)
  bool directColor;
  // latching
  uint16_t hCount;
  uint16_t vCount;
  bool hCountSecond;
  bool vCountSecond;
  bool countersLatched;
  uint8_t ppu1openBus;
  uint8_t ppu2openBus;
  // R4 dirty-frame render skip: signature of all render inputs, sampled at
  // frame start; if unchanged from the last rendered frame (and no raster/
  // HDMA writes could have altered pixels mid-frame), the whole frame's
  // pixel production is skipped and pixelBuffer keeps the last frame. All
  // derived, never serialized.
  uint32_t skipSig, skipVramGen, skipCgramGen;
  bool skipHaveBaseline;   // a frame has been rendered since reset/load
  bool skipRasterWrite;    // a ppu_write landed on a visible line this frame
  uint8_t skipMode;        // 0=full render, 1=whole-frame skip, 2=palette-only (R5)
  // R5 palette-only skip: when ONLY cgram changed since the last complete
  // stored render (geometry identical), decode+compose are reused from the
  // line store and only the output stage re-runs. Derived, never serialized.
  uint32_t psSig, psVramGen;  // signature/vram gen the line store was built at
  bool psValid;               // a complete line store exists for psSig
  bool psStoring;             // this full render is storing every line
  // pixel buffer (xbgr)
#ifdef FF4_PORT_STATIC_SNES
  /* G&W port: single buffer instead of even/odd double buffer, and
   * shrunk to 224 lines (SNES native height with overscan disabled).
   * Width halved to 256 single 4-byte pixels per row: the stock layout
   * stores a hires PAIR (8 bytes) per SNES column, but outside modes
   * 5/6/pseudo-hires both halves carry the same color and every reader
   * in this port (ff4_blit_to_lcd; ppu_putPixels is dead here) only
   * ever reads the left half. FF4 never uses hires, so the right half
   * was 229 KB of dead writes in the overlay's tightest RAM region.
   * Pair this with ppu_handleFrameStart forcing evenFrame=true (no
   * row+239 offset) and ppu_runLine bounded by
   * `frameOverscan ? 239 : 224` — FF4 sets frameOverscan=false so 224
   * rows are sufficient. Indexing goes through PPU_PIXELBUF_STRIDE /
   * PPU_PIXELBUF_XPITCH (ppu.c, main.c) — keep them in sync. */
  uint8_t pixelBuffer[256 * 4 * 224] __attribute__((aligned(4))); // R10c: word-store output path
#else
  // times 2 for even and odd frame
  uint8_t pixelBuffer[512 * 4 * 239 * 2] __attribute__((aligned(4))); // R10c
#endif
  uint8_t pixelOutputFormat;
};

/* Row stride and per-SNES-column pitch of pixelBuffer (see the field's
 * comment). The FF4 static build stores one 4-byte pixel per column;
 * stock LakeSnes stores a hires pair of two. */
#ifdef FF4_PORT_STATIC_SNES
#define PPU_PIXELBUF_STRIDE 1024
#define PPU_PIXELBUF_XPITCH 4
#else
#define PPU_PIXELBUF_STRIDE 2048
#define PPU_PIXELBUF_XPITCH 8
#endif

enum { ppu_pixelOutputFormatXBGR = 0, ppu_pixelOutputFormatBGRX = 1 };

/* Frameskip hook (see ppu_runLine): when 0, sprite evaluation still runs
 * (the range/time-over flags read back at $213E are game-visible state)
 * but the per-pixel render loop is skipped -- pixelBuffer simply keeps
 * the last rendered frame. Emulation-visible state is unaffected by
 * construction: the pixel loop only writes pixelBuffer. Default 1. */
extern int ff4_ppu_render_enabled;

Ppu* ppu_init(Snes* snes);
void ppu_free(Ppu* ppu);
void ppu_reset(Ppu* ppu);
void ppu_handleState(Ppu* ppu, StateHandler* sh);
bool ppu_checkOverscan(Ppu* ppu);
void ppu_handleVblank(Ppu* ppu);
void ppu_handleFrameStart(Ppu* ppu);
void ppu_runLine(Ppu* ppu, int line);
uint8_t ppu_read(Ppu* ppu, uint8_t adr);
void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val);
void ppu_putPixels(Ppu* ppu, uint8_t* pixels);
void ppu_setPixelOutputFormat(Ppu* ppu, int pixelOutputFormat);

#endif
