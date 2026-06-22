#include "snes/snes.h"

void Mult16_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    uint16_t multiplicand = read16(ram, 0x393D);
    uint16_t multiplier   = read16(ram, 0x393F);

    uint16_t reg_x = 0x0010;       /* ldx #$0010 */
    uint16_t m393f = multiplier;
    uint16_t m3943 = 0;            /* stz $3943 */
    uint16_t m3941 = 0;            /* stz $3941 */
    uint8_t  carry = snes->cpu->c;

    int n_ones = 0, n_zeros = 0;

    do {
        /* ror $393f — shift multiplier right, incoming carry enters MSB */
        uint8_t next_carry = m393f & 1;
        m393f = (m393f >> 1) | (carry ? 0x8000 : 0);
        carry = next_carry;

        if (carry) {
            /* clc / lda $393d / adc $3943 / sta $3943 */
            uint32_t sum = (uint32_t)multiplicand + m3943;
            m3943  = (uint16_t)(sum & 0xFFFF);
            carry  = (sum > 0xFFFF) ? 1 : 0;
            n_ones++;
        } else {
            carry = 0;   /* bcc taken: carry is cleared */
            n_zeros++;
        }

        /* ror $3943 */
        next_carry = m3943 & 1;
        m3943 = (m3943 >> 1) | (carry ? 0x8000 : 0);
        carry = next_carry;

        /* ror $3941 */
        next_carry = m3941 & 1;
        m3941 = (m3941 >> 1) | (carry ? 0x8000 : 0);
        carry = next_carry;

        reg_x--;
    } while (reg_x != 0);

    write16(ram, 0x3941, m3941);
    write16(ram, 0x3943, m3943);
    write16(ram, 0x393F, m393f);   /* ROR consumed $393F — write mutated value back */

    snes_runCycles(snes, 27 + n_zeros * 32 + n_ones * 48 - 1);

    /* shorta0 at end: mf=true, A=0 (tdc clears A), X=0 (tax) */
    snes->cpu->x  = 0;
    snes->cpu->mf = true;
    snes->cpu->a  = 0;
    snes->cpu->z  = true;   /* last dex result = 0 */
    snes->cpu->n  = false;
    snes->cpu->c  = carry;
}

// PITFALLS: 1 (DB=$7E required for correct RAM access), 9 (high byte of A preserved in B hidden register)
// HELPERS: read16/write16 for 16-bit little-endian access
// CONTRACT:
//   inputs_reg:  cpu->c (initial carry for first ROR)
//   inputs_ram:  $393D=2 (multiplicand), $393F=2 (multiplier — mutated to 0)
//   output_ram:  $3941=2 (low word), $3943=2 (high word), $393F=2 (0 after shift)
//   entry_mode:  mf=false, xf=false, dp=0x0, db=0x7E
//   entry_flags: c=initial carry consumed by first ROR
// REVERSED_FUNCTION: battle::Mult16 ($03:83B9)