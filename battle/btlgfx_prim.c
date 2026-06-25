#include "snes/snes.h"
#include "snes/dma.h"

/* BackAttackYOffset_s — $02:BB0B (9 opcodes, M=8)
 *
 * Adjusts a Y sprite offset for back-attack mirroring (small amount).
 * Input:  A = Y offset (8-bit)
 * Output: A = A unchanged if ram[$6CC0]==0; else A = (~A) - 8
 *
 * 48           PHA
 * AD C0 6C     LDA $6CC0       ; back-attack flag (absolute)
 * F0 07        BEQ +7          ; no back attack → PLA ; RTS
 * 68           PLA             ; restore A
 * 49 FF        EOR #$FF        ; bitwise NOT
 * 38           SEC
 * E9 08        SBC #$08        ; A = ~A - 8
 * 60           RTS
 * 68           PLA             ; BEQ target
 * 60           RTS
 */
void BackAttackYOffset_s_c(Snes *snes) {
    uint8_t a = (uint8_t)(snes->cpu->a);
    if (snes->ram[0x6CC0]) {
        /* PHA(22)+LDA(32)+BEQ_nt(16)+PLA(28)+EOR(16)+SEC(14)+SBC(16)+RTS(42)=186, -16 pull = 170 */
        snes_runCycles(snes, 170);
        uint8_t na = (uint8_t)(~a);
        uint8_t res = (uint8_t)(na - 8);
        snes->cpu->a  = (snes->cpu->a & 0xFF00) | res;
        snes->cpu->c  = (na >= 8);
        snes->cpu->v  = (((na ^ 8) & (na ^ res)) & 0x80) != 0;
        snes->cpu->n  = (res & 0x80) != 0;
        snes->cpu->z  = (res == 0);
    } else {
        /* PHA(22)+LDA(32)+BEQ_t(22)+PLA(28)+RTS(42)=146, -16 pull = 130 */
        snes_runCycles(snes, 130);
        snes->cpu->n = (a & 0x80) != 0;
        snes->cpu->z = (a == 0);
    }
}

/* BackAttackYOffset_l — $02:BB1A (8 opcodes, M=8)
 *
 * Same as _s but subtracts $10 instead of $08.
 *
 * 48           PHA
 * AD C0 6C     LDA $6CC0
 * F0 07        BEQ +7
 * 68           PLA
 * 49 FF        EOR #$FF
 * 38           SEC
 * E9 10        SBC #$10
 * 60           RTS
 * 68           PLA
 * 60           RTS
 */
void BackAttackYOffset_l_c(Snes *snes) {
    uint8_t a = (uint8_t)(snes->cpu->a);
    if (snes->ram[0x6CC0]) {
        /* identical structure to BB0B: 170 MC */
        snes_runCycles(snes, 170);
        uint8_t na = (uint8_t)(~a);
        uint8_t res = (uint8_t)(na - 0x10);
        snes->cpu->a  = (snes->cpu->a & 0xFF00) | res;
        snes->cpu->c  = (na >= 0x10);
        snes->cpu->v  = (((na ^ 0x10) & (na ^ res)) & 0x80) != 0;
        snes->cpu->n  = (res & 0x80) != 0;
        snes->cpu->z  = (res == 0);
    } else {
        /* identical structure to BB0B: 130 MC */
        snes_runCycles(snes, 130);
        snes->cpu->n = (a & 0x80) != 0;
        snes->cpu->z = (a == 0);
    }
}

/* Mult8_btlgfx — $02:8560 (11 opcodes, M=8, X=16)
 *
 * 8-bit shift-add multiply (combat graphics context).
 * Inputs:  ram[$26] = multiplicand,  ram[$28] = multiplier (DP=0)
 * Outputs: ram[$2A] = product_lo,    ram[$2B] = product_hi
 *
 * DA           PHX
 * A2 08 00     LDX #$0008      ; 8 iterations (16-bit X, xf=0)
 * 64 2A        STZ $2A         ; zero result lo
 * 64 2B        STZ $2B         ; zero partial-sum
 * [loop:]
 * 66 28        ROR $28         ; shift multiplier right, bit→carry
 * 90 07        BCC +7          ; if bit=0, skip add
 * A5 26        LDA $26         ; A = multiplicand
 * 18           CLC
 * 65 2B        ADC $2B
 * 85 2B        STA $2B
 * [bcc_target:]
 * 66 2B        ROR $2B         ; shift partial-sum right, carry→result
 * 66 2A        ROR $2A         ; shift result right
 * CA           DEX
 * D0 EE        BNE loop
 * FA           PLX
 * 60           RTS
 */
void Mult8_btlgfx_c(Snes *snes) {
    /* Save inputs BEFORE snes_runCycles — a VBlank NMI firing inside that call
     * can re-enter Mult8_btlgfx_c (via PeriodicMenuUpdate→LoadMenuTfrData→Mult8)
     * and clobber $26/$28 in WRAM and cpu->c. */
    uint8_t multiplicand = snes->ram[0x26];
    uint8_t mult         = snes->ram[0x28];
    uint8_t carry_in     = snes->cpu->c;  /* save before NMI can clobber it */

    /* Cycle-accurate accounting: mirrors the 8-iteration ROR-based loop.
     * PHX(30)+LDX_imm(24)+STZ_2A(24)+STZ_2B(24) = 102 preamble
     * Per iter (bit=0, BCC taken): ROR_28(38)+BCC_t(22)+ROR_2B(38)+ROR_2A(38)+DEX(14)+BNE(22/16)
     * Per iter (bit=1, BCC not taken): ROR_28(38)+BCC_nt(16)+LDA(24)+CLC(14)+ADC(24)+STA(24)+ROR_2B(38)+ROR_2A(38)+DEX(14)+BNE(22/16)
     * PLX(36)+RTS(42) = 78 epilog.  Subtract 16 MC already consumed by dispatch pullWord. */
    int cyc = 102 + 78;
    {
        uint8_t tmp = mult;
        for (int i = 0; i < 8; i++) {
            cyc += 38; /* ROR $28 */
            if (tmp & 1) {
                cyc += 16 + 24 + 14 + 24 + 24; /* BCC_nt + LDA + CLC + ADC + STA */
            } else {
                cyc += 22; /* BCC taken */
            }
            cyc += 38 + 38 + 14; /* ROR $2B + ROR $2A + DEX */
            cyc += (i < 7) ? 22 : 16; /* BNE taken / not taken on last iter */
            tmp >>= 1;
        }
    }
    snes_runCycles(snes, cyc - 16);
    /* After this point $26/$28/cpu->c may be clobbered by an NMI re-entry. */

    /* Full shift-and-add simulation matching $02:8560 exactly:
     *   STZ $2A; STZ $2B; LDX #8
     *   loop: ROR $28; BCC nc; CLC; ADC $26→$2B; nc: ROR $2B; ROR $2A; DEX; BNE
     *
     * The carry flowing into each ROR $28 comes from the PREVIOUS ROR $2A,
     * NOT from $28's own bits. Simulating ROR $28 in isolation (as a plain
     * 8×ROR loop) produces the wrong $28 side-effect and a wrong carry flag.
     * The only correct approach is to simulate the full loop so the carry
     * chain is faithful. */
    uint8_t c     = carry_in;
    uint8_t m     = mult;
    uint8_t p_lo  = 0;   /* $2A — STZ $2A */
    uint8_t p_hi  = 0;   /* $2B — STZ $2B */

    for (int i = 0; i < 8; i++) {
        /* ROR $28: bit0 of m → carry; incoming carry → bit7 of m */
        uint8_t bit28 = m & 1;
        m  = (m >> 1) | (c << 7);
        c  = bit28;  /* carry = old bit0($28) */

        /* BCC: if carry (old bit0($28)) add multiplicand to p_hi.
         * CLC before ADC means carry-out = plain overflow bit. */
        if (c) {
            uint16_t sum = (uint16_t)p_hi + (uint16_t)multiplicand;
            p_hi = (uint8_t)(sum & 0xFF);
            c    = (uint8_t)(sum >> 8);
        }
        /* else BCC taken: c already 0, no add */

        /* ROR $2B */
        uint8_t bit_hi = p_hi & 1;
        p_hi = (p_hi >> 1) | (c << 7);
        c    = bit_hi;

        /* ROR $2A — carry from ROR $2B feeds next iter's ROR $28 */
        uint8_t bit_lo = p_lo & 1;
        p_lo = (p_lo >> 1) | (c << 7);
        c    = bit_lo;
    }

    snes->ram[0x28] = m;
    snes->ram[0x2A] = p_lo;
    snes->ram[0x2B] = p_hi;
    snes->cpu->c = c;
    /* PLX sets N/Z from the restored (original) X value. */
    snes->cpu->n = (snes->cpu->x & 0x8000) != 0;
    snes->cpu->z = (snes->cpu->x == 0);
}

/* HardMult_btlgfx — $02:85D2 (10 opcodes, M=8, X=16, DP=0)
 *
 * Hardware 8×8 multiply via SNES MPY registers.
 * Inputs:  ram[$1C] = multiplicand A,  ram[$1E] = multiplier B (DP=0)
 * Output:  ram[$20]:ram[$21] = 16-bit product (STX $20 with X=16-bit)
 *
 * DA           PHX
 * A5 1C        LDA $1C
 * 8F 02 42 00  STA $004202     ; MPYA
 * A5 1E        LDA $1E
 * 8F 03 42 00  STA $004203     ; MPYB — triggers multiply
 * 8B           PHB
 * 7B           TDC             ; A = dp = 0
 * 48           PHA
 * AB           PLB             ; db = 0 (to access $004216 without bank prefix)
 * AE 16 42     LDX $4216       ; read RDMPYL:RDMPYH (16-bit result)
 * 86 20        STX $20
 * AB           PLB             ; restore original db
 * FA           PLX
 * 60           RTS
 *
 * The hardware multiply is instantaneous in emulation; no cycle accounting needed.
 */
void HardMult_btlgfx_c(Snes *snes) {
    /* Save inputs before snes_runCycles — same NMI-clobber hazard as Mult8_btlgfx_c. */
    uint8_t mc = snes->ram[0x1C];
    uint8_t mp = snes->ram[0x1E];
    /* PHX(30)+LDA(24)+STA_4202(38)+LDA(24)+STA_4203(38)+PHB(22)+TDC(14)+PHA(22)
     * +PLB(28)+LDX_4216(36)+STX_20(32)+PLB(28)+PLX(36)+RTS(42)=414, -16 pull = 398 */
    snes_runCycles(snes, 398);
    uint16_t product = (uint16_t)mc * (uint16_t)mp;
    snes->ram[0x20] = (uint8_t)(product & 0xFF);
    snes->ram[0x21] = (uint8_t)(product >> 8);
    snes->cpu->a = snes->cpu->dp;                       /* TDC: dp=0 → A=0 */
    snes->cpu->n = (snes->cpu->x & 0x8000) != 0;       /* PLX sets N/Z */
    snes->cpu->z = (snes->cpu->x == 0);
}

/* IncrTextPtr — $02:A491 (4 opcodes, X=16, DP=0)
 *
 * Increments the 16-bit text pointer at DP+$30.
 *
 * A6 30        LDX $30         ; X = ram[$30:$31]
 * E8           INX
 * 86 30        STX $30
 * 60           RTS
 */
void IncrTextPtr_c(Snes *snes) {
    /* LDX_dp(32)+INX(14)+STX_dp(32)+RTS(42)=120, -16 pull = 104 */
    snes_runCycles(snes, 104);
    /* Fire HDMA (matching interpreter behaviour). */
    if (snes->dma->hdmaRunRequested || snes->dma->hdmaInitRequested)
        dma_handleDma(snes->dma, 8);
    uint16_t x = (uint16_t)(snes->ram[0x30] | ((uint16_t)snes->ram[0x31] << 8));
    x++;
    snes->ram[0x30] = (uint8_t)(x & 0xFF);
    snes->ram[0x31] = (uint8_t)(x >> 8);
    snes->cpu->x = x;
    snes->cpu->n = (x & 0x8000) != 0;
    snes->cpu->z = (x == 0);
}

// CONTRACT BackAttackYOffset_s_c:
//   inputs_reg:  a=y_offset (8-bit, M=8)
//   inputs_ram:  $6CC0=1 (back-attack flag, absolute)
//   output_reg:  a=modified_y_offset
//   entry_mode:  mf=true, dp=any
// CONTRACT BackAttackYOffset_l_c: same as _s but offset=$10
// CONTRACT Mult8_btlgfx_c:
//   inputs_ram:  $26=1, $28=1 (DP=0)
//   output_ram:  $2A=1 (product_lo), $2B=1 (product_hi)
//   entry_mode:  mf=true, xf=false, dp=0
// CONTRACT HardMult_btlgfx_c:
//   inputs_ram:  $1C=1, $1E=1 (DP=0)
//   output_ram:  $20=1 (product_lo), $21=1 (product_hi)
//   entry_mode:  mf=true, xf=false, dp=0
// CONTRACT IncrTextPtr_c:
//   inputs_ram:  $30=2 (DP=0, 16-bit text pointer)
//   output_ram:  $30=2
//   entry_mode:  xf=false, dp=0
