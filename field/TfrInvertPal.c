#include "snes/snes.h"

// Purpose: Sets up DMA parameters to invert palette colors by transferring 
// a block of memory ($BDB0) to the palette RAM area via the DMA engine.
// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$EE (Field/System), DP=0
void TfrInvertPal_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    (void)ram;
    // CGRAM (palette) DMA: 256 bytes from $00:0BDB → CGDATA, CGADD=0. The
    // original port wrote the DMA params to WRAM ($7E:43xx) and never
    // transferred. Replicate manually via $2122 (CGDATA) — DMA-from-C doesn't
    // flush on the isolated harness (F6); $2122 writes take effect immediately.
    snes_writeBBus(snes, 0x21, 0x00);   // $2121 CGADD = 0
    uint32_t src = 0x000BDB;            // source ($4304=0 bank, $4302=$0BDB)
    for (int i = 0; i < 0x100; i++)     // size $4305 = 0x0100 bytes (128 colours)
        snes_writeBBus(snes, 0x22, snes_read(snes, src++));  // $2122 CGDATA
}

// PITFALLS: 1 (DB=$EE required for these memory accesses)
// HELPERS: read16/write16 — little-endian 16-bit accessors over ram[]
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  0x420B=1, 0x2121=1, 0x4300=1, 0x4301=1, 0x4302=2, 0x4304=1, 0x4305=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0xEE
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::TfrInvertPal ($EE:35)