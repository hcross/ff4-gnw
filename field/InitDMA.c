#include "snes/snes.h"

// Routine initializes the DMA controller registers.
// It clears the MDMA enable flag, sets the DMA control register,
// and clears the DMA address register.
void InitDMA_c(Snes *snes) {
    // MMIO registers (DB=$00), not WRAM. Two bugs in the original port: it wrote
    // ram[$43xx] (WRAM $7E:43xx) AND mis-mapped hMDMAEN to $4300 (it is $420B).
    // No transfer here — just DMA register init. Route through the bus.
    snes_write(snes, 0x420B, 0x00); // stz hMDMAEN — disable DMA
    snes_write(snes, 0x4301, 0x18); // sta $4301  — DMA0 B-bus addr ($2118)
    snes_write(snes, 0x4304, 0x00); // stz $4304  — DMA0 source bank
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