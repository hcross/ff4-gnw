#include "snes/snes.h"

// Logic:
// Transfers lava graphics (Water tiles) in two passes.
// First pass: Sets VRAM address based on WaterShiftX table index from ram[0x7C].
// Second pass: Sets VRAM address with a 0x40 offset to the same table value.
// Increments the animation frame counter at $7C.
void TfrLavaGfx_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // $2115/$2116/$2117 are MMIO PPU regs (not WRAM) → bus. And WaterShiftX is a
    // ROM table at $8F:90AA (DB=$8F), not WRAM — the original port read
    // ram[$8F00+x] ($7E:8F00). The actual transfer is done by TfrWaterTiles
    // (delegated to the interpreter), so no manual loop here — only the register
    // setup needs fixing.
    snes_writeBBus(snes, 0x15, 0x80);   // $2115 VMAIN

    uint8_t shift_idx = (uint8_t)((ram[0x7C] >> 1) & 0x0F);
    uint8_t shift_val = snes_read(snes, 0x8F90AA + shift_idx); // WaterShiftX,x (ROM)
    snes_writeBBus(snes, 0x16, shift_val);  // $2116 VMADDL
    snes_writeBBus(snes, 0x17, 0x38);       // $2117 VMADDH
    TfrWaterTiles_emu(snes);                // jsr TfrWaterTiles (does the copy)

    // Second pass: same index, +$40 offset
    shift_idx = (uint8_t)((ram[0x7C] >> 1) & 0x0F);
    uint8_t offset_val = (uint8_t)(snes_read(snes, 0x8F90AA + shift_idx) + 0x40);
    snes_writeBBus(snes, 0x16, offset_val); // $2116 VMADDL
    snes_writeBBus(snes, 0x17, 0x38);       // $2117 VMADDH
    TfrWaterTiles_emu(snes);

    ram[0x7C]++;                            // inc $7c (animation frame)
}

// PITFALLS: 7 (8-bit truncation for addition)
// HELPERS: TfrWaterTiles_emu(snes) — delegates TfrWaterTiles @ 8E0E
//
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x7C=1, 0x8F00=16
//   output_ram:  0x7C=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x8F
//   entry_flags: z=auto, n=auto

// REVERSED_FUNCTION: field::TfrLavaGfx ($8F:34)