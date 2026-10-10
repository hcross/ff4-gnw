#include "snes/snes.h"

// 16-bit unsigned division: ram[$3945] / ram[$3947] → ram[$3949] (quotient), ram[$394B] (remainder)
// Entry mode: A 16-bit (mf=0), X/Y 16-bit (xf=0), DB=$7E, DP=0
// Exit state (strict): A=D, m=8, X=0/C=0/V on the loop path; see body.
void Div16_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    Cpu *cpu = snes->cpu;
    write16(ram, 0x3949, 0);  // stz $3949 (longa: 16-bit)
    write16(ram, 0x394B, 0);  // stz $394b
    uint16_t dividend = read16(ram, 0x3945);
    uint16_t divisor  = read16(ram, 0x3947);
    if (dividend != 0 && divisor != 0) {  // beq @843f (x2)
        uint16_t remainder = 0;
        uint16_t quotient  = 0;
        bool v = cpu->v;
        for (int i = 0; i < 16; i++) {
            // rol $3945 / rol $394b: the carry rotated into $3945 is the bit
            // rotated out of $3949, always 0 within 16 iterations (clc first).
            uint16_t carry = (dividend >> 15) & 1;
            dividend <<= 1;
            remainder = (uint16_t)((remainder << 1) | carry);  // bit 16 lost (sec follows)
            // sec / lda $394b / sbc $3947 / sta $394b
            uint16_t diff = (uint16_t)(remainder - divisor);
            v = ((remainder ^ divisor) & (remainder ^ diff) & 0x8000) != 0;
            if (remainder >= divisor) {          // bcs @8439
                remainder = diff;
                quotient = (uint16_t)((quotient << 1) | 1);
            } else {
                // lda $394b / adc $3947 (C=0) / sta $394b / clc -- restores remainder
                uint16_t sum = (uint16_t)(diff + divisor);
                v = (~(diff ^ divisor) & (diff ^ sum) & 0x8000) != 0;
                quotient <<= 1;
            }
        }
        write16(ram, 0x3945, dividend);  // fully rotated out: 0
        write16(ram, 0x3949, quotient);
        write16(ram, 0x394B, remainder);
        cpu->x = 0;      // ldx #$0010 ... dex/bne
        cpu->c = false;  // last rol $3949 shifts out a 0
        cpu->v = v;
    }
    // @843f: shorta0 = tdc / sep #$20 -- A = D (16-bit transfer), m=8
    cpu->a  = cpu->dp;
    cpu->z  = (cpu->dp == 0);
    cpu->n  = (cpu->dp & 0x8000) != 0;
    cpu->mf = true;
}
// PITFALLS: 6 (mode A 16-bit required), 7 (arithmetic truncation not
// relevant here since all ops are 16-bit and explicit)
// HELPERS: none
// CONTRACT:
//   inputs_ram:  0x3945=2, 0x3947=2
//   output_ram:  0x3949=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
// STRICT: full-state equivalent (FF4_SPIKE_STRICT=gate) -- remainder $394B,
//   rotated dividend $3945, A=D, X=0, NZVC and m=8 at exit all match the asm.
//   entry_flags: z=auto, n=auto
// ENTRY_STATE: entry_mode corrected 2026-10-10 from the observed game entry (registry/ENTRY_STATE.md).
// REVERSED_FUNCTION: battle::Div16 ($84:07)