#include "snes/snes.h"

// UpdateLocalTiles: reads the base tile coordinates from $1706 (x) and
// $1707 (y), then queries GetTileProp for the five surrounding tiles
// (above, left, current, right, below). Stores the 16-bit tile property
// in $a3/$a9/$a1/$a5/$a7 and the 8-bit property in $070c/$070f/$070b/$070d/$070e.
void UpdateLocalTiles_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    uint8_t base_x = ram[0x1706];
    uint8_t base_y = ram[0x1707];

    // tile above: (base_x, base_y - 1)
    ram[0x1A] = base_x;
    ram[0x1B] = base_y - 1;
    get_tile_prop_emu(snes);
    write16(ram, 0xA3, read16(ram, 0x1E));
    ram[0x070C] = ram[0x06];

    // tile left: (base_x - 1, base_y)
    ram[0x1B] = base_y;          // inc $1b (restore y)
    ram[0x1A] = base_x - 1;      // dec $1a
    get_tile_prop_emu(snes);
    write16(ram, 0xA9, read16(ram, 0x1E));
    ram[0x070F] = ram[0x06];

    // current tile: (base_x, base_y)
    ram[0x1A] = base_x;          // inc $1a (restore x)
    get_tile_prop_emu(snes);
    write16(ram, 0xA1, read16(ram, 0x1E));
    ram[0x070B] = ram[0x06];

    // tile right: (base_x + 1, base_y)
    ram[0x1A] = base_x + 1;      // inc $1a
    get_tile_prop_emu(snes);
    write16(ram, 0xA5, read16(ram, 0x1E));
    ram[0x070D] = ram[0x06];

    // tile below: (base_x, base_y + 1)
    ram[0x1B] = base_y + 1;      // inc $1b
    ram[0x1A] = base_x;          // dec $1a (restore x)
    get_tile_prop_emu(snes);
    write16(ram, 0xA7, read16(ram, 0x1E));
    ram[0x070E] = ram[0x06];
}

// PITFALLS:
//   1  – DB=$7E required for GetTileProp (emu wrapper must set it).
//   8  – Mode heritage: assumed mf=true (A 8-bit), xf=false (X/Y 16-bit)
//        for the field module; no explicit shorta/longa in this routine.
// HELPERS:
//   get_tile_prop_emu(snes)  – delegates GetTileProp @ $9F:C0
//   read16 / write16         – little-endian 16-bit accessors over ram[]
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x1706=1, 0x1707=1
//   output_ram:  0xA3=2, 0xA9=2, 0xA1=2, 0xA5=2, 0xA7=2,
//                0x070C=1, 0x070F=1, 0x070B=1, 0x070D=1, 0x070E=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::UpdateLocalTiles ($9F:6C)