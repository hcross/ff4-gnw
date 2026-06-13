#include "snes/snes.h"

// Transfers 8 bytes from WRAM $7F:5800+X to VRAM via $2119.
// The asm is unrolled; a loop produces identical PPU side-effects.
// Entry: X = 16-bit offset into the table at $7F:5800.
// Mode: A 8-bit (mf=1), X/Y 16-bit (xf=0) — inferred from byte-sized
//       loads and the 16-bit index.
void TfrWaterTiles_c(Snes *snes, uint16_t x) {
    uint32_t base = 0x15800u + x;            // $7F:5800 in WRAM flat offset
    for (int i = 0; i < 8; i++) {
        uint8_t b = snes->ram[base + i];
        snes_write(snes, 0x2119, b);         // PPU VRAM data write (auto-increment)
    }
}

// PITFALLS: 6 (mode A 8-bit assumed — confirmed by byte-sized loads)
// HELPERS:  none
// CONTRACT:
//   inputs_reg:  x=16
//   inputs_ram:  none
//   output_ram:  none (writes to PPU $2119)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::TfrWaterTiles ($00:8E0E)