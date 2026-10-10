#include "snes/snes.h"

// 16-bit addition with carry propagation: $395A = $3956 + $3958, $395C = carry
// Entry mode: A 16-bit (mf=0), X 16-bit (xf=0), DB=$7E, DP=0
// All operands and results in WRAM
void Add16_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    uint16_t a = read16(ram, 0x3956);  // lda $3956
    uint16_t b = read16(ram, 0x3958);  // lda $3958

    // Perform 16-bit addition with carry
    uint32_t sum = (uint32_t)a + (uint32_t)b;
    uint16_t result_lo = (uint16_t)(sum & 0xFFFF);
    uint16_t carry_hi = (uint16_t)((sum >> 16) & 0xFFFF);

    write16(ram, 0x395A, result_lo);   // sta $395a
    write16(ram, 0x395C, carry_hi);    // sta $395c (carry stored as 16-bit)

    // adc #0 on 0 + carry cannot carry or overflow; shorta0 = tdc / sep #$20.
    snes->cpu->c  = false;
    snes->cpu->v  = false;
    snes->cpu->a  = snes->cpu->dp;
    snes->cpu->z  = (snes->cpu->dp == 0);
    snes->cpu->n  = (snes->cpu->dp & 0x8000) != 0;
    snes->cpu->mf = true;
}

// PITFALLS: 6 (mode A is 16-bit due to `longa`), 7 (arithmetic truncation
// not an issue here since we're doing full 16-bit math)
// HELPERS: read16, write16 (standard 16-bit accessors)
// CONTRACT:
//   inputs_ram:  0x3956=2, 0x3958=2
//   output_ram:  0x395A=2, 0x395C=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// STRICT: full-state equivalent (FF4_SPIKE_STRICT=gate), 2026-10-10.
// ENTRY_STATE: entry_mode corrected 2026-10-10 from the observed game entry (registry/ENTRY_STATE.md).
// REVERSED_FUNCTION: battle::Add16 ($84:E3)