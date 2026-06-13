#include "snes/snes.h"

// This label is a data table containing powers of 10 (or related constants).
// It contains no executable instructions.
static const uint16_t Pow10Hi[8] = {
    0x0098, 0x000F, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
void Pow10Hi_c(Snes *snes) {
    // No operation – this label is a data table only.
}

// PITFALLS: 11 (Data-only label treated as code), 12 (Exact bank/offset for reversed function)
// HELPERS: none
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::Pow10Hi ($C3:7F)