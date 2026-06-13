#include "snes/snes.h"

// field::UpdateWhalePal ($C3:BD) — updates two palette entries ($0EC7-$0EC8)
// for the big whale based on a cycling index derived from $7A.
// Only runs when $1704 == 6 (big whale map).
//
// Entry mode: A 8-bit (mf=true), X/Y 16-bit (xf=false), DB=$7E, DP=0.
// No register inputs; reads ram[$1704] and ram[$7A].
// Writes ram[$0EC7] and ram[$0EC8] from a far ROM table at $C3:WhalePal.
void UpdateWhalePal_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // Check map id
    if (ram[0x1704] != 0x06) {
        return;  // bne @c3d9
    }

    // Compute table index: (($7A >> 2) & 0x0E) → even values 0..14
    uint8_t a = ram[0x7A];
    a >>= 2;          // lsr2 (two logical shifts)
    a &= 0x0E;
    uint16_t x = a;   // tax (8-bit A → 16-bit X, high byte zeroed)

    // Base address of WhalePal in ROM (bank $C3).  Offset assumed $DA;
    // adjust if the disassembly places it elsewhere.
    const uint32_t WHALE_PAL_ADDR = 0xC300DA;

    ram[0x0EC7] = snes_read(snes, WHALE_PAL_ADDR + x);
    ram[0x0EC8] = snes_read(snes, WHALE_PAL_ADDR + x + 1);
}

// PITFALLS: none (simple routine, no flag-dependent branches, no mode ambiguity)
// HELPERS: snes_read (provided by harness) to access ROM table
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x1704=1, 0x7A=1
//   output_ram:  0x0EC7=1, 0x0EC8=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::UpdateWhalePal ($C3:BD)