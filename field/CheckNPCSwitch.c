#include "snes/snes.h"

// field::CheckNPCSwitch ($00:C3EF)
// Reads a bit from an NPC switch table in ROM, indexed by the input
// accumulator split into table index (bits 3-7) and bit position (bits 0-2).
// The table base is $00:12E0; an optional +$20 offset is applied when
// ram[$0FE5] has bit7 set or ram[$1701] == 0.
// Returns the selected bit (0 or 1) in A.
void CheckNPCSwitch_c(Snes *snes, uint8_t a) {
    uint8_t *ram = snes->ram;
    Cpu *cpu = snes->cpu;

    uint8_t low3 = a & 0x07;
    ram[0x07] = low3;                 // sta $07
    uint8_t high = a >> 3;            // lsr3 (three lsr in 8-bit mode)
    ram[0x3D] = high;                 // sta $3d

    // Condition: if $0FE5 negative OR $1701 == 0, add $20 to table index
    uint8_t val0fe5 = ram[0x0FE5];
    uint8_t val1701 = ram[0x1701];
    if ((val0fe5 & 0x80) || val1701 == 0) {
        high += 0x20;                 // clc / adc #$20
        ram[0x3D] = high;             // sta $3d
    }

    ram[0x3E] = 0;                    // stz $3e (high byte of X = 0)

    uint8_t y = low3;                 // tay (Y = bit position, 8-bit value)
    uint16_t x = high;                // ldx $3d (X = table index, 16-bit, hi=0)
    uint8_t byteVal = snes_read(snes, 0x0012E0 + x); // lda f:$0012e0,x

    // The asm loop shifts byteVal right y times, then one more lsr,
    // moving bit y into carry.  Equivalent to (byteVal >> y) & 1.
    uint8_t result = (byteVal >> y) & 1;

    // Final: lda #0 / adc #0  → A = carry (0 or 1), flags updated
    cpu->a = result;
    cpu->z = (result == 0);
    cpu->n = false;                    // result is 0 or 1, never negative
    cpu->c = false;                    // adc #0 with A=0 cannot produce carry out

    // Y is preserved (phy/ply); we leave cpu->y unchanged.
}

// PITFALLS: 1 (DB=$7E required for direct-page/absolute RAM accesses),
//           6 (mode A 8-bit assumed; X/Y 16-bit inherited from caller)
// HELPERS:  snes_read (ROM access at $00:12E0+X)
// CONTRACT:
//   inputs_reg:  a=8, x=none, y=none
//   inputs_ram:  0x0FE5=1, 0x1701=1
//   output_ram:  0x07=1, 0x3D=1, 0x3E=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto, c=auto
// REVERSED_FUNCTION: field::CheckNPCSwitch ($00:C3EF)