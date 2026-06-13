#include "snes/snes.h"

// Data-only label: def_ptxt macro expands to a text command sequence
// (set position + string). The caller reads these bytes via indexed
// addressing; no executable instructions exist at this address.
static const uint8_t LoadBattleSpeedPosText_data[16] = {
    0x02, 20, 1,                     // set text position (x=20, y=1)
    'B', 'a', 't', 't', 'l', 'e',   // "Battle Speed"
    ' ', 'S', 'p', 'e', 'e', 'd',
    0x00                             // null terminator
};

void LoadBattleSpeedPosText_c(Snes *snes) {
    (void)snes;
    // Stub: the label is pure rodata; the dispatcher hands it back
    // to the asm interpreter when the caller needs the bytes.
}

// PITFALLS: 11 (data-only label, stub function), 12 (REVERSED_FUNCTION
//           address must match the label, not the stub)
// HELPERS:  none
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=auto, xf=auto, dp=0x0, db=0xDF
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: menu::LoadBattleSpeedPosText ($DF:D2)