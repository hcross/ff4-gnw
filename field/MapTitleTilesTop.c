#include "snes/snes.h"

// This label is a data table only.
// It contains a sequence of tile indices for the top part of the map title.
static const uint8_t MapTitleTilesTop[32] = {
    0x16, 0x20, 0x17, 0x20, 0x17, 0x20, 0x17, 0x20,
    0x17, 0x20, 0x17, 0x20, 0x17, 0x20, 0x17, 0x20,
    0x17, 0x20, 0x17, 0x20, 0x17, 0x20, 0x17, 0x20,
    0x17, 0x20, 0x17, 0x20, 0x17, 0x20, 0x18, 0x20
};
void MapTitleTilesTop_c(Snes *snes) {
    // No operation – this label is a data table only.
}

// PITFALLS: 11 (Data-only label treated as routine stub to satisfy harness)
// HELPERS: none
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto

// REVERSED_FUNCTION: field::MapTitleTilesTop ($F7:96)