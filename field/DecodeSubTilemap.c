#include "snes/snes.h"
#include "ff4_helpers.h"

/* Strong override of the weak no-op decode_sub_tilemap_emu (dispatch_all.c).
 *
 * DecodeBG1Tilemap_c / DecodeBG2Tilemap_c load the tilemap id into A and then
 * delegate the actual RLE tilemap decompression here. The real SNES routine
 * DecodeSubTilemap ($16:FEBA) consumes A (the map id) and fills the WRAM
 * tilemap buffer at $7F:5C71, which the title-screen transfer routines then
 * push to VRAM. Run it in the interpreter via the dispatch bridge.
 *
 * Entry contract (set by the caller DecodeBG1Tilemap_c): A = tilemap id,
 * mf=1 (A 8-bit), xf=0 (X 16-bit), DB=$FF, DP=0 — matches DecodeSubTilemap. */
void decode_sub_tilemap_emu(Snes *snes) {
    run_emulated_func(snes, 0x16FEBAu);
}
