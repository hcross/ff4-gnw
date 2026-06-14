#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$FF, DP=0
// Logic:
//   Loads the BG1 tilemap identifier from $06F9 into A, then calls
//   DecodeSubTilemap to process that specific map.
void DecodeBG1Tilemap_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // lda $06f9
    uint8_t tilemap_id = ram[0x06F9];
    snes->cpu->a = tilemap_id;
    
    // Update flags Z and N based on the load for the sub-routine
    snes->cpu->z = (tilemap_id == 0);
    snes->cpu->n = (tilemap_id & 0x80) != 0;

    // jsr DecodeSubTilemap
    decode_sub_tilemap_emu(snes);
}

// PITFALLS: None (straightforward sequence)
// HELPERS: decode_sub_tilemap_emu(snes) — delegates DecodeSubTilemap @ $FF:B0 (est)
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x06F9=1
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0xFF
//   entry_flags: z=auto, n=auto

// REVERSED_FUNCTION: field::DecodeBG1Tilemap ($FF:AB)