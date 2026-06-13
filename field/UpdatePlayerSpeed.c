#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$83 (program bank), DP=0
// Logic:
//   Read vehicle ID from $1704.
//   Use ID as index into PlayerSpeedTbl.
//   Store result in $AC.
void UpdatePlayerSpeed_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // lda $1704 (vehicle id)
    uint8_t vehicle_id = ram[0x1704];

    // tax / lda PlayerSpeedTbl,x
    // PlayerSpeedTbl is at $83:00C0 (based on disassembly mapping)
    // Mode A is 8-bit, so it's a byte load from table.
    uint8_t speed = ram[0x00C0 + vehicle_id];

    // sta $ac
    ram[0xAC] = speed;
}

// PITFALLS: None (Simple linear load/store)
// HELPERS: None
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x1704=1, 0x00C0=1 (table)
//   output_ram:  0xAC=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x83
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::UpdatePlayerSpeed ($83:02)