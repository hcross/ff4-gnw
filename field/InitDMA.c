#include "snes/snes.h"

// Routine initializes the DMA controller registers.
// It clears the MDMA enable flag, sets the DMA control register,
// and clears the DMA address register.
void InitDMA_c(Snes *snes) {
    // Note: $4300 range is SNES hardware register space (CGRAM/DMA/etc).
    // In the snesrev/LakeSnes pattern, these are accessed via the 
    // snes->ram mapping or specific hardware register offsets.
    // According to standard SNES mapping:
    // hMDMAEN corresponds to $4300
    uint8_t *ram = snes->ram;

    ram[0x4300] = 0; // stz hMDMAEN
    ram[0x4301] = 0x18; // lda #$18 / sta $4301
    ram[0x4304] = 0; // stz $4304
}

// PITFALLS: None. Simple register writes.
// HELPERS: None.
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  0x4300=1, 0x4301=1, 0x4304=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::InitDMA ($8B:2A)