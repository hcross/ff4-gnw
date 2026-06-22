#include "snes/snes.h"

// 8-bit multiplication (16-bit shift-and-add result)
// Inputs:  ram[$DF] = multiplicand, ram[$E1] = multiplier (low bytes of 16-bit words)
// Outputs: ram[$E3] = low 16 bits of product, ram[$394D] = high 16 bits
// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$7E, DP=0
void Mult8_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // stz $e0
    // stz $e2
    // En entrée, l'accumulateur est en 8-bit, donc stz est 8-bit.
    ram[0xE0] = 0;
    ram[0xE2] = 0;

    uint16_t multiplicand = read16(ram, 0xDF); // $df..$e0
    uint16_t multiplier = read16(ram, 0xE1);   // $e1..$e2
    uint16_t m_e3 = 0;                         // stz $e3 (low 16 bits of product)
    uint16_t m_394d = 0;                       // stz $394d (high 16 bits of product)

    uint16_t reg_a = snes->cpu->a;
    uint16_t reg_x = 0x0010;                   // ldx #$0010
    uint8_t carry = snes->cpu->c;

    int n_ones = 0;
    int n_zeros = 0;

    do {
        // ror $e1
        uint8_t next_carry = multiplier & 1;
        multiplier = (multiplier >> 1) | (carry ? 0x8000 : 0);
        carry = next_carry;

        if (carry) { // bcc @83fb (branch if carry clear)
            // clc / lda $df / adc $394d / sta $394d
            uint32_t sum = (uint32_t)multiplicand + m_394d;
            reg_a = (uint16_t)(sum & 0xFFFF);
            m_394d = reg_a;
            carry = (sum > 0xFFFF) ? 1 : 0;
            n_ones++;
        } else {
            carry = 0; // bcc pris, carry est garanti à 0 pour @83fb
            n_zeros++;
        }

        // ror $394d
        next_carry = m_394d & 1;
        m_394d = (m_394d >> 1) | (carry ? 0x8000 : 0);
        carry = next_carry;

        // ror $e3
        next_carry = m_e3 & 1;
        m_e3 = (m_e3 >> 1) | (carry ? 0x8000 : 0);
        carry = next_carry;

        // dex / bne @83ee
        reg_x--;
    } while (reg_x != 0);

    write16(ram, 0xE1, multiplier);
    write16(ram, 0xE3, m_e3);
    write16(ram, 0x394D, m_394d);

    int loop_cycles = n_zeros * 30 + n_ones * 45 - 1;
    snes_runCycles(snes, 32 + loop_cycles);

    // Sortie: shorta0 (A 8-bit, X=0, Y inchangé, A cleared to 0 by tdc)
    snes->cpu->x = 0;
    snes->cpu->mf = true;
    snes->cpu->a = 0;

    // Les flags Z/N à la sortie reflètent le résultat du dernier `dex` (X=0)
    snes->cpu->z = true;
    snes->cpu->n = false;
    snes->cpu->c = carry;
}

// PITFALLS: 1 (DB=$7E required for correct RAM access), 9 (high byte of A preserved in B hidden register)
// HELPERS: read16/write16 for 16-bit little-endian access
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0xDF=2, 0xE1=2
//   output_ram:  0xE3=2, 0x394D=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: battle::Mult8 ($03:83E0)
