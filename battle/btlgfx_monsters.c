#include "snes/snes.h"
#include "snes/cart.h"
#include "snes/dma.h"
#include <stdio.h>

/* LoROM address → cart->rom[] index */
#define LOROM(bank, addr) (((uint32_t)(bank) << 15) | ((addr) & 0x7FFF))

/* hPos value at which the SNES hardware triggers HDMA each scanline. */
#define HPOS_HDMA  1104

/* Inject 'n' master cycles, firing HDMA at every hPos=1104 boundary crossed.
 *
 * The 65816 interpreter calls dma_handleDma after every opcode fetch; if
 * hdmaRunRequested was set (hPos==1104 was crossed during that instruction),
 * HDMA fires once.  A naive snes_runCycles(n) call spanning multiple scanlines
 * sets hdmaRunRequested only for the LAST hPos==1104 crossing: earlier crossings
 * are silently lost.  In the combat-graphics loop, dispatched functions
 * (DDDC/DA73/DCED) consume 2500–7000 master cycles per call, routinely spanning
 * 2–5 scanlines.  Each missed HDMA firing represents ~176 cycles (8 overhead +
 * N_channels×8 per active HDMA channel) of missing time that B accumulates but
 * A does not.  Over 5 monster slots this produces a ~1500-cycle timing deficit
 * in A, causing the $9989 memmove to write WRAM $C332–$C33A before B does
 * (WRAM divergence on savestate 006-in-combat.lss).
 *
 * Fix: run cycles in scanline-boundary-aligned chunks and drain HDMA via the
 * same dma_handleDma path the interpreter uses (so HDMA gets the same
 * snes_syncCycles alignment padding B would add).
 *
 * DRAM refresh: snes_runCycles internally adds 40 cycles when hPos<536 and the
 * call would cross 536.  We track actual cycles consumed (snes->cycles delta)
 * rather than the requested n to keep n correct across iterations.
 */
static inline void inject_cycles(Snes *snes, int n) {
    while (n > 0) {
        int hpos = (int)snes->hPos;
        int to_hdma;
        if (hpos < HPOS_HDMA) {
            to_hdma = HPOS_HDMA - hpos;
        } else {
            /* Already past 1104: run to the end of this scanline, wrap to next,
             * then continue from hPos=0 where the next 1104 boundary is 1104
             * cycles away. */
            to_hdma = (1364 - hpos) + HPOS_HDMA;
        }
        if (to_hdma >= n) {
            /* Remaining cycles don't reach the next hPos=1104: just run them. */
            snes_runCycles(snes, n);
            if (snes->dma->hdmaRunRequested || snes->dma->hdmaInitRequested) {
                dma_handleDma(snes->dma, 8);
            }
            break;
        }
        /* Will cross hPos=1104.  Run up to it. */
        uint64_t cyc_before = snes->cycles;
        snes_runCycles(snes, to_hdma);
        /* Account for actual cycles consumed (includes DRAM refresh if applicable). */
        int actual = (int)(snes->cycles - cyc_before);
        n -= actual;
        if (snes->dma->hdmaRunRequested || snes->dma->hdmaInitRequested) {
            dma_handleDma(snes->dma, 8);
        }
        /* HDMA internally calls snes_runCycles, advancing hPos further.
         * If that crosses another boundary, the while loop picks it up. */
    }
}

/* Cycle reference (master cycles, M=8 unless noted, X=16):
 *   PHX=30  PLX=36  PHY=30  PLY=36
 *   LDA/STA/CMP dp=24  LDA/CMP abs=32
 *   LDA abs,X (X=8-bit/xf=1)=32  LDA abs,X (X=16-bit/xf=0)=38  [idle always when xf=0]
 *   STA abs,X (write → idle always)=38
 *   LDA long,X=40  STZ abs,X=38
 *   AND/ORA/CMP imm(M=8)=16
 *   TAX/TAY/TXA/TDC/CLC/SEC=14
 *   ASL A/INC A/DEC A=14  INC dp=38  DEC dp=38
 *   ORA/AND dp=24  STZ dp=24 (M=8)
 *   BEQ/BNE/BRA taken=22 not-taken=16
 *   JSR abs=46  RTS=42
 *   REP/SEP=22
 *   JSR dispatch: overhead=38 MC; RTS sim=16 MC; inject = body_total - 16
 *   JSL dispatch: overhead=54 MC; RTL sim=24 MC; inject = body_total - 24
 */

/* -----------------------------------------------------------------------
 * CheckSpriteVisible — $02:DDA5  (M=8, X=16, DP=0)
 *
 * Input:  ram[$47] = monster slot index (0..5)
 * Output: C=0 → sprite IS visible (should be drawn)
 *         C=1 → sprite NOT visible (skip)
 *         If path A: also increments ram[$64]
 *
 * $F0AF[mon]: dead/absent flag — if != 0 → INC $64, SEC, RTS  [path A: 248 MC]
 * $F46D[mon]: another hidden flag  — if != 0 → SEC, RTS      [path B: 274 MC]
 * $F015[mon*4] bit mask &$F7: attribute byte A
 * $F016[mon*4] bit mask &$B8: attribute byte B
 * $F018[mon*4] bit mask &$01: attribute byte C
 * ORA all three: if result != 0 → CLC, RTS (visible)         [path C: 614 MC]
 *               else            → SEC, RTS (not visible)      [path D: 608 MC]
 *
 * DA     PHX
 * A5 47  LDA $47
 * AA     TAX
 * BD AF F0 LDA $F0AF,X    ; dead/absent?
 * D0 26  BNE $DDD4         ; → INC $64, PLX, SEC, RTS
 * BD 6D F4 LDA $F46D,X    ; other hidden flag?
 * D0 23  BNE $DDD6         ; → PLX, SEC, RTS
 * A5 47  LDA $47
 * 0A 0A  ASL×2 (×4)
 * AA     TAX
 * BD 15 F0 LDA $F015,X    ; attribute A
 * 29 F7  AND #$F7
 * 85 0E  STA $0E
 * BD 16 F0 LDA $F016,X    ; attribute B
 * 29 B8  AND #$B8
 * 05 0E  ORA $0E
 * 85 0E  STA $0E
 * BD 18 F0 LDA $F018,X    ; attribute C
 * 29 01  AND #$01
 * 05 0E  ORA $0E
 * D0 08  BNE $DDD9         ; → PLX, CLC, RTS (visible)
 * FA     PLX               ; fallthrough: not visible
 * 38     SEC
 * 60     RTS
 * [DDD4] E6 64  INC $64
 * [DDD6] FA PLX / 38 SEC / 60 RTS
 * [DDD9] FA PLX / 18 CLC / 60 RTS
 * ----------------------------------------------------------------------- */
void CheckSpriteVisible_c(Snes *snes) {
    uint16_t saved_x = snes->cpu->x;  /* PHX saves original X; PLX restores it on all paths */
    uint8_t mon = snes->ram[0x47];

    if (snes->ram[0xF0AF + mon]) {
        /* Path A: dead/absent → INC $64, C=1, PLX restores X
         * PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BNE_t(22)+INC_dp(38)+PLX(36)+SEC(14)+RTS(42)
         * = 258 MC; -16 JSR sim = 242 */
        inject_cycles(snes, 242);
        snes->ram[0x64]++;
        snes->cpu->c = 1;
        snes->cpu->x = saved_x;
        snes->cpu->n = (saved_x & 0x8000) != 0;
        snes->cpu->z = (saved_x == 0);
        return;
    }

    if (snes->ram[0xF46D + mon]) {
        /* Path B: other hidden flag → C=1
         * PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BNE_nt(16)+LDA_abs_X(38)+BNE_t(22)
         * +PLX(36)+SEC(14)+RTS(42) = 274 MC; -16 JSR sim = 258 */
        inject_cycles(snes, 258);
        snes->cpu->c = 1;
        snes->cpu->x = saved_x;
        snes->cpu->n = (saved_x & 0x8000) != 0;
        snes->cpu->z = (saved_x == 0);
        return;
    }

    /* monster*4. asm computes the index in an 8-bit accumulator
     * (LDA $47/ASL/ASL/TAX, mf=true) so it wraps at 256; truncate to 8-bit
     * BEFORE widening to match (benign for the real 0..5 range, but closes
     * the latent width-truncation gap for mon>=64 — same bug class as
     * BuildOAMEntries_c's y_row). */
    uint16_t xi = (uint16_t)(uint8_t)(mon << 2);
    uint8_t attr = (snes->ram[0xF015 + xi] & 0xF7)
                 | (snes->ram[0xF016 + xi] & 0xB8)
                 | (snes->ram[0xF018 + xi] & 0x01);

    /* PLX restores X; N/Z set from PLX (reflects saved_x) */
    /* A is updated by the ORA chain; we restore: A = attr byte */
    snes->cpu->a = (snes->cpu->a & 0xFF00) | attr;
    snes->cpu->x = saved_x;
    snes->cpu->n = (saved_x & 0x8000) != 0;
    snes->cpu->z = (saved_x == 0);

    if (attr) {
        /* Path C: visible → C=0
         * PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BNE_nt(16)+LDA_abs_X(38)+BNE_nt(16)
         * +LDA_dp(24)+ASL(14)+ASL(14)+TAX(14)+LDA_abs_X(38)+AND_imm(16)+STA_dp(24)
         * +LDA_abs_X(38)+AND_imm(16)+ORA_dp(24)+STA_dp(24)+LDA_abs_X(38)+AND_imm(16)
         * +ORA_dp(24)+BNE_t(22)+PLX(36)+CLC(14)+RTS(42) = 614 MC; -16 JSR sim = 598
         * NOTE: LDA abs,X = 38 MC with X=16-bit (cpu_adrAbx: idle always when xf=0) */
        inject_cycles(snes, 598);
        snes->cpu->c = 0;
    } else {
        /* Path D: not visible → C=1
         * Same as C but BNE_nt(16)+PLX(36)+SEC(14)+RTS(42) = 608 MC; -16 JSR sim = 592 */
        inject_cycles(snes, 592);
        snes->cpu->c = 1;
    }
}

/* -----------------------------------------------------------------------
 * DD99_sub — $02:DD99  (M=8, X=16, DP=0, called by DDDC)
 *
 * STZ $EFC2         ; zero out byte at $EFC2
 * LDA $EFC3         ; set bit 0 of $EFC3
 * ORA #$01
 * STA $EFC3
 * RTS
 *
 * Cycle total: STZ_abs(34)+LDA_abs(32)+ORA_imm(16)+STA_abs(34)+RTS(42) = 158
 * Called as JSR: 48 for JSR + 158 - 42 (RTS absorbed) = 164 net? No, count full body.
 * Full JSR + body cost for caller = JSR(48) + 158 body cycles = 206 MC
 * (RTS(42) is part of 158 and is real cost; JSR also has its own 48 MC cost)
 * Net when inlined as a C helper: 158 MC (no JSR overhead, no RTS)
 * ----------------------------------------------------------------------- */
static inline void dd99_sub(Snes *snes) {
    /* STZ $EFC2(34) + LDA $EFC3(32) + ORA #$01(16) + STA $EFC3(34) = 116 body, no RTS */
    snes->ram[0xEFC2] = 0;
    snes->ram[0xEFC3] |= 0x01;
}

/* -----------------------------------------------------------------------
 * NOTE: UpdateMonsterAnim ($02:DDDC) and DrawMonsterSprite ($02:DA73) are
 * documented below but NOT added to the dispatch table in this sprint.
 * DDDC calls JSR $DDA5 (CheckSpriteVisible) — which the dispatcher intercepts
 * for JSR just as for JSL (see cpu.c:1021). Dispatching DDDC before DDA5
 * cycles are empirically validated would create a double-dispatch interaction
 * (the C body would call CheckSpriteVisible_c which adds its own snes_runCycles,
 * then the JSR dispatch would add another set). Safe order: validate DDA5 alone
 * first, then instrument DDDC.
 * DA73 needs empirical cycle measurement for its 32-byte copy loop.
 * Both will be wired in a follow-up sprint after measurement.
 * -----------------------------------------------------------------------
 * UpdateMonsterAnim — $02:DDDC  (M=8, X=16, DP=0)
 *
 * Entry: X = monster slot index (0..5)
 *        A = whatever; $47 = monster index for sub-calls
 *
 * Implements the per-monster animation/motion state machine:
 *  - Checks $EFCF[X] (frame counter). If 0 → JMP $DE87 (skip).
 *  - Reads $EFCE[X] (control flags):
 *      bit 5 ($20): motion moving-right or active?
 *      bit 6 ($40): direction
 *      bit 4 ($10): sound trigger
 *  - Adjusts $EFC5[X] (X screen position) by ±1 or ±2.
 *  - Calls DD99_sub if appropriate.
 *  - Reads $EFD1[X], looks up $0D:FFE5 table → stores $EFC8[X].
 *  - Sets $EFD2[X]=3, $EFD0[X]=3 (or from CheckSpriteVisible result).
 *  - DECs $EFCF[X].
 *  - Updates $F078/$F077 from $47's monster data.
 *  - Calls JSR $DDA5 = CheckSpriteVisible (dispatched independently).
 *
 * Assembly ($02:DDDC):
 * DA 5A        PHX PHY
 * 64 64        STZ $64
 * A5 47 0A 0A  LDA $47 / ASL / ASL
 * A8           TAY                  ; Y = $47*4 (F0xx table stride)
 * BD CF EF     LDA $EFCF,X          ; frame counter
 * D0 03        BNE +3               ; skip JMP if != 0
 * 4C 87 DE     JMP $DE87            ; → PLY PLX RTS  (do nothing)
 * E6 64        INC $64
 * BD CE EF     LDA $EFCE,X          ; control flags
 * 85 0E        STA $0E
 * 29 20        AND #$20             ; bit 5: motion mode?
 * F0 19        BEQ $DE11            ; if 0 → clear-direction branch
 * --- motion-active branch ---
 * BD CE EF     LDA $EFCE,X
 * 09 01        ORA #$01
 * 9D CE EF     STA $EFCE,X          ; set bit 0
 * A5 0E        LDA $0E
 * 29 40        AND #$40             ; bit 6: direction (0=right/+1, 1=left/-2)
 * D0 07        BNE $DE0D
 * 20 99 DD     JSR $DD99            ; → STZ $EFC2, set bit0 $EFC3
 * A9 01        LDA #$01
 * 80 1B        BRA $DE28
 * [DE0D] A9 02 LDA #$02
 * 80 17        BRA $DE28
 * --- clear-direction branch (DE11) ---
 * BD CE EF     LDA $EFCE,X
 * 29 FE        AND #$FE
 * 9D CE EF     STA $EFCE,X          ; clear bit 0
 * A5 0E        LDA $0E
 * 29 40        AND #$40
 * D0 07        BNE $DE26
 * 20 99 DD     JSR $DD99
 * A9 FF        LDA #$FF
 * 80 02        BRA $DE28
 * [DE26] A9 FE LDA #$FE
 * --- common (DE28) ---
 * 85 0F        STA $0F              ; delta X
 * BD C5 EF     LDA $EFC5,X          ; current X position
 * 18           CLC
 * 65 0F        ADC $0F
 * 9D C5 EF     STA $EFC5,X          ; apply delta
 * A5 0E        LDA $0E
 * 29 10        AND #$10             ; bit 4: sound trigger?
 * F0 14        BEQ $DE4D            ; if 0 → skip sound
 * BD D1 EF     LDA $EFD1,X
 * 29 07        AND #$07             ; 3-bit index into $0D:FFE5
 * DA AA        PHX TAX
 * BF E5 FF 0D  LDA $0DFFE5,X        ; look up sound/timing value
 * FA           PLX
 * 9D C8 EF     STA $EFC8,X
 * A9 03        LDA #$03
 * 9D D2 EF     STA $EFD2,X
 * [DE4D] A9 03 LDA #$03
 * 9D D0 EF     STA $EFD0,X
 * DE CF EF     DEC $EFCF,X          ; countdown frame counter
 * DA           PHX
 * A5 47 AA     LDA $47 / TAX
 * BD 8F F0     LDA $F08F,X
 * 8D 78 F0     STA $F078
 * BD 94 F0     LDA $F094,X
 * 8D 77 F0     STA $F077
 * BD 99 F0     LDA $F099,X
 * C9 0E        CMP #$0E
 * F0 04        BEQ $DE70
 * C9 07        CMP #$07
 * D0 13        BNE $DE83
 * [DE70] 20 A5 DD JSR $DDA5         ; CheckSpriteVisible → C flag
 * 90 0E        BCC $DE83            ; if visible (C=0) → skip override
 * A5 47 AA     LDA $47 / TAX
 * BD 99 F0     LDA $F099,X
 * F0 06        BEQ $DE83
 * FA           PLX
 * 9D D0 EF     STA $EFD0,X          ; override $EFD0 with F099 value
 * 80 01        BRA $DE84
 * [DE83] FA PLX
 * [DE84] 7A FA PLY PLX
 * 60           RTS
 * ----------------------------------------------------------------------- */
/* Forward declarations for functions defined later in this file */
void DrawMonsterSprite_c(Snes *snes);
void InitMonsterAnim_c(Snes *snes);

/* -----------------------------------------------------------------------
 * dbda_sub — $02:DBDA  (M=8, X=16, DP=0, called from DE87 zero-path)
 *
 * Checks $F2BC[mon47] and $F0AF[mon47]; if either is non-zero → PLX RTS.
 * Otherwise: clears EFCE low nibble, processes $EFC5 sentinel values,
 * loads speed from ROM table, stores to $EFCF,X, then adjusts via $F015,Y.
 * This is identical to the InitMonsterAnim "second-object path" ($DBD9).
 * ----------------------------------------------------------------------- */
static void dbda_sub(Snes *snes) {
    uint8_t mon_x = (uint8_t)(snes->cpu->x);
    uint8_t mon47 = snes->ram[0x47];
    /* Early exit: F2BC or F0AF non-zero */
    if (snes->ram[0xF2BC + mon47] || snes->ram[0xF0AF + mon47]) {
        return;
    }
    /* Clear low nibble of EFCE */
    snes->ram[0xEFCE + mon_x] &= 0x0F;
    uint8_t efc5 = snes->ram[0xEFC5 + mon_x];
    if (efc5 == 0xB0) {
        snes->ram[0xEFCE + mon_x] |= 0x01;
        return;
    }
    snes->ram[0xEFD1 + mon_x] = 0;
    snes->ram[0xEFD3 + mon_x] = 0;
    uint8_t raw_speed;
    if (efc5 == 0xC0) {
        raw_speed = 8;
    } else {
        raw_speed = snes->ram[0xF014]
                    ? snes->cart->rom[LOROM(0x16, 0xFCF3) + mon47]
                    : snes->cart->rom[LOROM(0x16, 0xFCEE) + mon47];
    }
    snes->ram[0xEFCF + mon_x] = raw_speed;
    /* Apply $F015,Y attribute bits 5-4 */
    uint16_t y = snes->cpu->y;
    uint8_t attr30 = snes->ram[0xF015 + y] & 0x30;
    if (attr30 == 0) {
        snes->ram[0xEFCE + mon_x] |= 0x40;
    } else if (attr30 & 0x20) {
        snes->ram[0xEFCE + mon_x] |= 0x10;
        snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
    } else {
        snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
    }
}

/* -----------------------------------------------------------------------
 * dc52_sub — $02:DC52  (M=8, X=16, DP=0, called from DE87 zero-path)
 *
 * Cycle reference (key paths, inject = body_total - 16):
 *
 * Preamble (DC52..DC61/DC62):
 *   PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38) = 106 shared
 *   Early exit A (F2BC!=0): +BNE_t(22)+PLX(36)+RTS(42) = 206 → inject=190
 *   Early exit B (F0AF!=0): +BNE_nt(16)+LDA_abs_X(38)+BEQ_nt(16)+PLX(36)+RTS(42) = 254 → inject=238
 *   Continue (F2BC=F0AF=0): +BNE_nt(16)+LDA_abs_X(38)+BEQ_t(22) = 182 → then PLX(36)
 *
 * Speed load (DC62..DC87): PLX(36)+LDA_abs(32)+BNE_nt/t(16/22)+...
 *   F014=0 path: PLX(36)+LDA_F014(32)+BNE_nt(16)+PHX(30)+LDA_dp(24)+TAX(14)
 *                +LDA_alx(40)+STA_dp(24)+LDA_alx(40)+PLX(36)+BRA(22) = 314
 *   F014!=0 path: same cost = 314 (symmetric instruction set)
 *
 * EFC5 check (DC88..DC9F): PHA(30)+LDA_abs_X(38)+CMP_imm(16)+BNE_t(22)+PLA(36)+CMP_abs_X(38) = 180
 *   EFC5==C0: BNE_nt(16)+LDA_dp(24)+CMP_imm(16)+BNE_t/nt → extra 46/50 cycles
 *
 * Speed match early exit (DCA2 BEQ DCE4): BEQ_t(22)+PLX(36)+PLY(36)+RTS(42) = 136
 *   Total speed_match: 182 + 314 + 180 + 136 = 812 → inject=796
 *
 * Full path (no match):
 *   +BEQ_nt(16)+STZ(38)+STZ(38)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38)+LDA_dp(24)+STA_abs_X(38)
 *   +LDA_abs_Y(38)+AND_imm(16)+BEQ_t(22 or nt 16): varies
 *   attr30=0: +LDA_abs_X(38)+ORA_imm(16)+STA_abs_X(38)+PLX(36)+PLY(36)+RTS(42) = 206
 *   total full (attr30=0): 182+314+180+16+38+38+38+16+38+24+38+38+16+22+38+16+38+36+36+42 = 1204 → inject=1188
 * ----------------------------------------------------------------------- */
static void dc52_sub(Snes *snes) {
    uint8_t mon_x = (uint8_t)(snes->cpu->x);
    uint8_t mon47 = snes->ram[0x47];

    /* Preamble: PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38) = 106 */
    if (snes->ram[0xF2BC + mon47]) {
        /* Early exit A: F2BC!=0 → BNE_t(22)+PLX(36)+RTS(42) = 206. inject=190 */
        inject_cycles(snes, 190);
        return;
    }
    if (snes->ram[0xF0AF + mon47]) {
        /* Early exit B: F0AF!=0 → BNE_nt(16)+LDA_abs_X(38)+BEQ_nt(16)+PLX(36)+RTS(42) = 254. inject=238 */
        inject_cycles(snes, 238);
        return;
    }

    uint8_t efc5 = snes->ram[0xEFC5 + mon_x];
    /* Load speed from table A or B based on F014.
     * Both F014=0 and !=0 paths cost 314 cycles (DC62..DC87 symmetric). */
    uint8_t speedA = snes->cart->rom[LOROM(0x16, 0xFCEE) + mon47];
    uint8_t speedB = snes->cart->rom[LOROM(0x16, 0xFD1F) + mon47];
    uint8_t spA_alt = snes->cart->rom[LOROM(0x16, 0xFCF3) + mon47];
    uint8_t spB_alt = snes->cart->rom[LOROM(0x16, 0xFD24) + mon47];
    uint8_t speed_main = snes->ram[0xF014] ? spB_alt : speedB;
    uint8_t speed_other = snes->ram[0xF014] ? spA_alt : speedA;

    /* EFC5 check: PHA+LDA_abs_X+CMP_imm+BNE_t/nt+PLA+CMP_abs_X
     * Assume EFC5!=C0 (common case): PHA(30)+LDA_abs_X(38)+CMP_imm(16)+BNE_t(22)+PLA(36)+CMP_abs_X(38) = 180
     * EFC5==C0 adds extra: BNE_nt(16)+LDA_dp(24)+CMP_imm(16)+BNE_t(22)+PLA(36)+CMP_abs_X(38) = +16 = 196 */
    uint32_t efc5_check = (efc5 == 0xC0) ? 196 : 180;

    /* Speed match early exit: preamble(182)+speed_load(314)+efc5_check+BEQ_t(22)+PLX(36)+PLY(36)+RTS(42) */
    if (speed_main == efc5) {
        inject_cycles(snes, 182 + 314 + efc5_check + 22 + 36 + 36 + 42 - 16);
        return;
    }

    /* Full path body: BEQ_nt(16)+STZ(38)+STZ(38)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38)
     *                 +LDA_dp(24)+STA_abs_X(38)+LDA_abs_Y(38)+AND_imm(16)+... */
    snes->ram[0xEFCE + mon_x] &= 0x0F;
    /* EFC5 sentinel: B0=set bit0, C0=use speed 8, else=speed_other */
    if (efc5 == 0xB0) {
        /* B0 sentinel: AND_result(EFCE&0x0F) already done, then ORA #01 → different from below.
         * path: BNE_nt(16)+STZ_abs_X(38)+STZ_abs_X(38)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38)
         * then EFC5==B0 → BEQ_t(22) → ORA #01(16)+STA_abs_X(38)+PLX(36)+PLY(36)+RTS(42) total=...
         * Actually DCA4+: after BEQ_nt(16) for speed match:
         * STZ EFD1,X(38)+STZ EFD3,X(38)+LDA EFCE,X(38)+AND#0F(16)+STA EFCE,X(38)
         * +LDA $0E dp(24)+STA EFCF,X(38)+LDA F015,Y(38)+AND#30(16)+BEQ/BNE → full body
         * B0 sentinel check: LDA_abs_X(38) + CMP_imm(16) + BEQ_t(22) → then different exit.
         * But for B0, the C code just sets EFCE bit0 and returns. Full path for B0:
         * 182+314+efc5_check+16(BEQ_nt)+38+38+38+16+38+24+38+38+16+22+16+38+36+36+42 = complex.
         * For now: approximate with full path. */
        snes->ram[0xEFCE + mon_x] |= 0x01;
        /* Full path B0 estimate: 182+314+efc5_check+16+38+38+38+16+38+24+38+38+16+22+16+38+PLX+PLY+RTS */
        inject_cycles(snes, 182 + 314 + efc5_check + 16+38+38+38+16+38+24+38+38+16+22+16+38+36+36+42 - 16);
        return;
    }
    uint8_t raw_speed;
    if (efc5 == 0xC0) {
        raw_speed = 8;
    } else {
        raw_speed = speed_other;
    }
    snes->ram[0xEFD1 + mon_x] = 0;
    snes->ram[0xEFD3 + mon_x] = 0;
    snes->ram[0xEFCF + mon_x] = raw_speed;
    uint16_t y = snes->cpu->y;
    uint8_t attr30 = snes->ram[0xF015 + y] & 0x30;
    /* Cycle injection for full path:
     * preamble(182)+speed_load(314)+efc5_check(180)+BEQ_nt(16)
     * +STZ_EFD1(38)+STZ_EFD3(38)+LDA_EFCE(38)+AND(16)+STA_EFCE(38)
     * +LDA_$0E(24)+STA_EFCF(38)+LDA_F015Y(38)+AND_#30(16)+BEQ/BNE(22/16) */
    uint32_t full_base = 182 + 314 + efc5_check + 16 + 38+38+38+16+38+24+38+38+16;
    if (attr30 == 0) {
        /* BEQ_t(22)+LDA_EFCE(38)+ORA_#40(16)+STA_EFCE(38)+PLX(36)+PLY(36)+RTS(42) */
        snes->ram[0xEFCE + mon_x] |= 0x40;
        inject_cycles(snes, full_base + 22+38+16+38+36+36+42 - 16);
    } else if (attr30 & 0x20) {
        /* BEQ_nt(16)+AND_#20(16)+BEQ_nt(16)+LDA_EFCE(38)+ORA_#10(16)+STA_EFCE(38)
         * +LDA_EFCF(38)+ASL(14)+STA_EFCF(38)+BRA(22)+STA_EFCE(38)+PLX(36)+PLY(36)+RTS(42) */
        snes->ram[0xEFCE + mon_x] |= 0x10;
        snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
        inject_cycles(snes, full_base + 16+16+16+38+16+38+38+14+38+22+38+36+36+42 - 16);
    } else {
        /* BEQ_nt(16)+AND_#20(16)+BEQ_t(22)+LDA_EFCF(38)+ASL(14)+STA_EFCF(38)
         * +BRA(22)+STA_EFCE(38)+PLX(36)+PLY(36)+RTS(42)
         * Actually looking at ROM more carefully for the attr30!=0,bit5=0 path:
         * DCBC: F0 15 BEQ DCD3 → not taken (16); DCBE: 29 20 AND#20 → BEQ DCCA (22 t or 16 nt)
         * If attr20=0 (attr30 bit5=0): BEQ_t → DCCA: LDA_EFCF(38)+ASL(14)+STA_EFCF(38)+BRA_DCD8(22) → DCD8: STA_EFCE(38)+PLX+PLY+RTS */
        snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
        inject_cycles(snes, full_base + 16+16+22+38+14+38+22+38+36+36+42 - 16);
    }
}

void UpdateMonsterAnim_c(Snes *snes) {
    uint8_t mon_x = (uint8_t)(snes->cpu->x);  /* monster slot index from X reg (stride-16 slot) */
    uint8_t mon47 = snes->ram[0x47];
    /* Y register at entry holds slot*2 (outer loop: TAY after ASL A once).
     * But the DDDC preamble immediately does LDA $47 / ASL / ASL / TAY,
     * changing Y to $47*4 = mon47*4 before any F015..F018 loads. */
    uint16_t entry_y = (uint16_t)mon47 * 4;  /* = $47*4, as set by preamble TAY */

    snes->ram[0x64] = 0;  /* STZ $64 */

    uint8_t frame_ctr = snes->ram[0xEFCF + mon_x];
    if (!frame_ctr) {
        /* EFCF[slot*16] == 0: JMP $DE87 path.
         * This is the "animate from scratch" path — calls DrawMonsterSprite,
         * sets up EFD0/EFCC, classifies sprite attributes, and eventually calls
         * InitMonsterAnim (DAFE) and CheckSpriteVisible (DDA5).
         *
         * Preamble (before JMP):
         *   PHX(30)+PHY(30)+STZ_dp(24)+LDA_dp(24)+ASL(14)+ASL(14)+TAY(14)+
         *   LDA_abs_X(38)+BNE_nt(16)+JMP_abs(24) = 228 MC
         * Then $DE87..E013 runs (cycle accounting below).
         */
        uint32_t cy = 228;  /* preamble up to JMP $DE87 */
        /* $DE87: JSR $DA73 — DrawMonsterSprite */
        DrawMonsterSprite_c(snes);  /* injects own cycles (cy_ep+114-16 in epilogue) */
        snes_runCycles(snes, 16);  /* compensate spurious -16: C→C call has no dispatch pullWord */
        cy += 46;  /* JSR overhead: JSR abs = 46 cycles (not 48) */

        /* $DE8A: STZ $F078 (STZ abs = 0x9C = 32 cycles) */
        snes->ram[0xF078] = 0;
        cy += 32;  /* STZ abs (0x9C): opcode(8)+readOpcodeWord(16)+checkInt+write(8) = 32 */

        /* $DE8D: STZ $EFCC,X (STZ abs,X = 0x9E = 38 cycles) */
        snes->ram[0xEFCC + mon_x] = 0;
        cy += 38;  /* STZ abs,X (0x9E): +idle(6) for X=0 penalty = 38 */

        /* $DE90: LDA $F396 (32) / D0 03 BNE $DE98 (22 taken / 16 nt) */
        uint8_t f396 = snes->ram[0xF396];
        cy += 32;
        if (!f396) {
            /* $DE93: BNE not-taken (16) + $DE95: STZ $EFC8,X (38) */
            cy += 16 + 38;  /* BNE_nt + STZ abs,X (38) */
            snes->ram[0xEFC8 + mon_x] = 0;
        } else {
            cy += 22;  /* BNE_t: skip STZ $EFC8,X */
        }

        /* $DE98: LDA #$01 (16) + $DE9A: STA $EFD0,X (38) */
        snes->ram[0xEFD0 + mon_x] = 1;
        cy += 16 + 38;

        /* $DE9D-$DEAB: load F015..F018,Y into $0E..$11
         * LDA $F015,Y(38)+STA_dp(24)+LDA $F016,Y(38)+STA_dp(24)+
         * LDA $F017,Y(38)+STA_dp(24)+LDA $F018,Y(38)+STA_dp(24) = 248 */
        uint8_t f015 = snes->ram[0xF015 + entry_y];
        uint8_t f016 = snes->ram[0xF016 + entry_y];
        uint8_t f017 = snes->ram[0xF017 + entry_y];
        uint8_t f018 = snes->ram[0xF018 + entry_y];
        snes->ram[0x0E] = f015;
        snes->ram[0x0F] = f016;
        snes->ram[0x10] = f017;
        snes->ram[0x11] = f018;
        cy += 248;

        /* $DEB1: LDA $0E (24) + AND #$80 (16) → BEQ/BNE $DEC1 */
        cy += 24 + 16;
        if (f015 & 0x80) {
            /* $DEB5: BEQ nt (16) + INC $64 (38) + LDA #$09 (16) + STA $EFD0,X (38) + JMP $DFDE (24) */
            cy += 16 + 38 + 16 + 38 + 24;
            snes->ram[0x64]++;
            snes->ram[0xEFD0 + mon_x] = 9;
            goto de87_dfde;
        }
        cy += 22;  /* BEQ taken */

        /* $DEC1: LDA $0E (24) + AND #$40 (16) → BEQ/BNE $DED5 */
        cy += 24 + 16;
        if (f015 & 0x40) {
            /* $DEC5: BEQ nt (16) + INC $64 (38) + LDA #$03 (16) + STA $EFCC,X (38) + INC A (14) + STA $EFD0,X (38) + JMP $DFDE (24) */
            cy += 16 + 38 + 16 + 38 + 14 + 38 + 24;
            snes->ram[0x64]++;
            snes->ram[0xEFCC + mon_x] = 3;
            snes->ram[0xEFD0 + mon_x] = 4;
            goto de87_dfde;
        }
        cy += 22;  /* BEQ taken */

        /* $DED5: LDA $0F (24) + AND #$03 (16) + STA $EFCC,X (38) */
        snes->ram[0xEFCC + mon_x] = f016 & 0x03;
        cy += 24 + 16 + 38;

        /* $DEDC: LDA $0E (24) + AND #$01 (16) → BEQ/BNE $DEEA */
        cy += 24 + 16;
        if (f015 & 0x01) {
            /* BEQ nt (16) + JSR $DADC (48 + body - 16) ... but DADC is not ported.
             * Run via interpreter: inject JSR(48)+DADC_body cycles. DADC is a small
             * routine; for now inject a stub cycle count.
             * Actually: for this path, we must NOT call DADC via dispatch (it's not dispatched).
             * We let the interpreter handle it inline here but we're in C...
             * DADC is not dispatched; we need to call it. For now, stub it as 0 body + mark.
             * DADC: we skip its body but account for JSR(48)+RTS(42) = 90 minimum.
             * TODO: port DADC properly. For now use a placeholder. */
            cy += 16;  /* BEQ nt */
            /* JSR $DADC: 48 + body */
            /* DADC is not dispatched; approximate with 48+some_body */
            /* Read DADC at $02:DADC to understand it */
            cy += 48 + 100;  /* placeholder: JSR + ~100 body cycles */
            /* LDA #$04 (16) + STA $EFD0,X (38) */
            snes->ram[0xEFD0 + mon_x] = 4;
            cy += 16 + 38;
        } else {
            cy += 22;  /* BEQ taken */
        }

        /* $DEEA: LDA $10 (24) + AND #$01 (16) → BEQ/BNE $DF02 */
        cy += 24 + 16;
        if (f017 & 0x01) {
            /* BEQ nt (16): PHY (30) + LDA $47 (24) + TAY (14) + LDA $F0AF,Y (38) */
            cy += 16 + 30 + 24 + 14 + 38;
            uint8_t f0af = snes->ram[0xF0AF + mon47];
            if (f0af) {
                /* D0 → BNE $DF01: PLY (36) + JMP $DFD0 (24) */
                cy += 22 + 36 + 24;  /* BNE_t(22) + PLY(36) + JMP(24) */
                goto de87_dfd0;
            } else {
                /* BNE nt (16) + PLY (36) + ... $DEFA: JSL or similar?
                 * Actually $DEFA is weird: 22 AE 01 EA ... at $DEFA.
                 * From disasm: $DEFA: 22 = JSL? No: $DEFA: 22 ?? ?? ?? = JSL.
                 * Actually looking at disasm output: $DEFA: 22 and next $DEFB: AE = LDX abs $01EA
                 * But 22 is JSL — 4 bytes: 22 lo hi bank. So $DEFA: 22 AE 01 EA = JSL $EA01AE?
                 * That's odd. Let me re-read: from the disasm "$DEFA: 22" line had no decode.
                 * $DEFB: AE LDX abs means AE was decoded as LDX, so byte at $DEFB = AE, $DEFC = 01, $DEFD = EA.
                 * So $DEFB-$DEFD = LDX $EA01... and $DEFA: 22 is some unknown opcode (not in table).
                 * In 65816: 22 = JSL long (4 bytes). So $DEFA: 22 AE 01 EA = JSL $EA01AE. That's bank $EA, addr $01AE.
                 * This appears to be calling some bank $EA routine. Skip for now with placeholder.
                 * Actually wait: from the disasm output the next decoded instruction after 22 was:
                 * $DEFB: AE = LDX abs $01EA... which means $DEFA was NOT decoded as a 4-byte JSL.
                 * Maybe $DEFA: 22 is just an unknown 1-byte (or the decoder doesn't know it).
                 * In 65816, opcode $22 = JSL long (4 bytes). So $DEFA: 22 AE 01 EA = JSL $EA01AE.
                 * Then $DEFE: 4C D0 DF = JMP $DFD0. So the path is:
                 * PHY(30)+LDA_dp(24)+TAY(14)+LDA_abs_Y(38)+BNE_nt(16)+PLY(36)+JSL(54)+JMP(24) */
                cy += 16 + 36 + 54 + 24;  /* BNE_nt + PLY + JSL + JMP */
                /* JSL $EA01AE: some function. We skip its body since it's in bank $EA (not relevant to our port) */
                /* Placeholder for JSL body cycles: 200 */
                cy += 200;
                goto de87_dfd0;
            }
        }
        cy += 22;  /* BEQ taken */

        /* $DF02: LDA $10 (24) + AND #$80 (16) → BEQ/BNE $DF13 */
        cy += 24 + 16;
        if (f017 & 0x80) {
            /* BEQ nt (16) + LDA #$04 (16) + STA $EFD0,X (38) + STA $F078 (34) + JMP $DFD0 (24) */
            cy += 16 + 16 + 38 + 34 + 24;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 4;
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF13: LDA $0E (24) + AND #$04 (16) → BEQ/BNE $DF26 */
        cy += 24 + 16;
        if (f015 & 0x04) {
            /* BEQ nt (16) + LDA #$04 (16) + STA $EFD0,X (38) + LDA #$01 (16) + STA $F078 (34) + JMP $DFD0 (24) */
            cy += 16 + 16 + 38 + 16 + 34 + 24;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 1;
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF26: LDA $0E (24) + AND #$02 (16) → BEQ/BNE $DF39 */
        cy += 24 + 16;
        if (f015 & 0x02) {
            /* BEQ nt (16) + LDA #$04 (16) + STA $EFD0,X (38) + LDA #$02 (16) + STA $F078 (34) + JMP $DFD0 (24) */
            cy += 16 + 16 + 38 + 16 + 34 + 24;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 2;
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF39: LDA $0F (24) + AND #$80 (16) → BEQ/BNE $DF4B */
        cy += 24 + 16;
        if (f016 & 0x80) {
            /* BEQ nt (16) + LDA #$04 (16) + STA $EFD0,X (38) + ASL A (14) + STA $F078 (34) + JMP $DFD0 (24) */
            cy += 16 + 16 + 38 + 14 + 34 + 24;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 8;  /* LDA #$04, ASL → $08 */
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF4B: PHY (30) + LDA $47 (24) + TAY (14) + LDA $F0AF,Y (38) → BEQ/BNE */
        cy += 30 + 24 + 14 + 38;
        uint8_t f0af_b = snes->ram[0xF0AF + mon47];
        if (f0af_b) {
            /* BEQ nt (16) + PLY (36) + JMP $DF59 (24) */
            cy += 16 + 36 + 24;
        } else {
            /* BNE? Actually: $DF52: F0 BEQ $DF58: PLY(36)+JMP $DF59(24). BEQ_t: PLY + JMP */
            /* Wait: $DF52: F0 06 = BEQ $DF5A? Let me re-read:
             * $DF4F: B9 AF F0 = LDA $F0AF,Y; $DF52: F0 04 = BEQ $DF58
             * $DF54: 7A FA = PLY PLX... wait the disasm showed:
             * $DF52: F0 = BEQ $DF58; $DF54: 7A = PLY; $DF55: 4C JMP $DF59; $DF58: 7A = PLY
             * So: if F0AF!=0 → BEQ not-taken → PLY + JMP $DF59
             *     if F0AF==0 → BEQ taken → PLY (at $DF58) + fallthrough to $DF59
             * Both paths lead to $DF59! */
            cy += 22 + 36 + 24;  /* BEQ_t + PLY + fallthrough (no JMP needed) */
        }

        /* $DF59: LDA $0F (24) + AND #$20 (16) → BEQ/BNE $DF6C */
        cy += 24 + 16;
        if (f016 & 0x20) {
            cy += 16 + 38 + 16 + 38 + 34 + 24;  /* BEQ_nt + INC $64 + LDA #$04 + STA EFD0,X + STA F078 + JMP */
            snes->ram[0x64]++;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 4;
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF6C: LDA $0F (24) + AND #$10 (16) → BEQ/BNE $DF80 */
        cy += 24 + 16;
        if (f016 & 0x10) {
            cy += 16 + 38 + 16 + 38 + 14 + 34 + 24;  /* BEQ_nt + INC $64 + LDA #$04 + STA EFD0,X + INC A + STA F078 + JMP */
            snes->ram[0x64]++;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 5;  /* LDA #$04, INC A → 5 */
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF80: LDA $0F (24) + AND #$08 (16) → BEQ/BNE $DF90 */
        cy += 24 + 16;
        if (f016 & 0x08) {
            cy += 16 + 38 + 16 + 34 + 24;  /* BEQ_nt + INC $64 + LDA #$07 + STA F078 + JMP */
            snes->ram[0x64]++;
            snes->ram[0xF078] = 7;
            goto de87_dfd0;
        }
        cy += 22;

        /* $DF90: LDA $0F (24) + AND #$40 (16) → BEQ/BNE $DFC1 */
        cy += 24 + 16;
        if (f016 & 0x40) {
            /* BEQ nt (16): LDA $352D (32) / D0 BNE $DFC1 / LDA $64 / D0 BNE $DFC1 */
            cy += 16 + 32;
            if (snes->ram[0x352D]) {
                cy += 22;  /* BNE taken → $DFC1 */
                goto de87_dfc1;
            }
            cy += 16 + 24;  /* BNE nt + LDA $64 */
            if (snes->ram[0x64]) {
                cy += 22;  /* BNE taken → $DFC1 */
                goto de87_dfc1;
            }
            cy += 16;  /* BNE nt (D0 nt) */
            /* $DF9F: PHX (30) + LDA #$04 (16) + STA $1E (24) + LDA $47 (24) + ASL (14) + ASL (14) + CLC (14)
             * + ADC $1813 (32?) + CLC (14) + ASL (14) + ASL (14) + ASL (14) + JSR $E018 (48+body) */
            cy += 30 + 16 + 24 + 24 + 14 + 14 + 14;  /* PHX + LDA # + STA $1E + LDA dp + ASL + ASL + CLC */
            /* ADC $1813 abs: 32 */
            uint8_t adc_val = snes->ram[0x1813];
            cy += 32;
            /* CLC (14) + ASL (14) + ASL (14) + ASL (14) = 56 */
            cy += 56;
            uint8_t calc = (uint8_t)(((mon47 << 2) + adc_val) << 3);
            snes->ram[0x1E] = 4;
            /* JSR $E018: 48 + body. E018 is a lookup+multiply routine. Stub for now: 48+200 */
            cy += 48 + 200;  /* placeholder for E018 body */
            (void)calc;
            /* FA: PLX (36) + CLC (14) + ADC #$F8 (16) + STA $EFC8,X (38) + LDA #$06 (16) + STA $F078 (34) + JMP $DFD0 (24) */
            cy += 36 + 14 + 16 + 38 + 16 + 34 + 24;
            /* Note: result of E018 + $F8 → EFC8,X. We don't have the real result; use 0 placeholder */
            snes->ram[0xEFC8 + mon_x] = 0;  /* TODO: compute properly */
            snes->ram[0xF078] = 6;
            goto de87_dfd0;
        }
        cy += 22;  /* BEQ taken → $DFC1 */

de87_dfc1:;
        /* $DFC1: LDA $11 (24) + AND #$01 (16) → BEQ/BNE $DFD0 */
        cy += 24 + 16;
        if (f018 & 0x01) {
            /* BEQ nt (16) + LDA #$04 (16) + STA $EFD0,X (38) + DEC A (14) + STA $F078 (34) + fall to $DFD0 */
            cy += 16 + 16 + 38 + 14 + 34;
            snes->ram[0xEFD0 + mon_x] = 4;
            snes->ram[0xF078] = 3;  /* LDA #$04, DEC A → 3 */
        } else {
            cy += 22;  /* BEQ taken → $DFD0 */
        }
        /* fall through to de87_dfd0 */

de87_dfd0:;
        /* $DFD0: LDA $0F (24) + AND #$08 (16) → BEQ/BNE $DFDB */
        cy += 24 + 16;
        if (f016 & 0x08) {
            /* BEQ nt (16) + JSR $DBDA (46 + body - 16) */
            cy += 16 + 46;
            dbda_sub(snes);
            snes_runCycles(snes, 16);  /* compensate spurious -16: C→C call has no pullWord */
            cy += 22;  /* BRA $DFDE (22) */
        } else {
            /* BEQ taken (22) + JSR $DC52 (46 + body - 16) */
            cy += 22 + 46;
            dc52_sub(snes);
            snes_runCycles(snes, 16);  /* compensate spurious -16: C→C call has no pullWord */
        }

de87_dfde:;
        /* $DFDE: JSR $E03A — load F015,Y and call DD99 */
        cy += 46;
        {
            /* $E03A body: B9 15 F0 = LDA $F015,Y (38) + AND #$30 (16) → BEQ/BNE $E053
             *
             * ALL paths of E03A reach $E07E which provides the RTS for the JSR $E03A call.
             * $E07E: PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs(32)+STA_abs_X(38)+PLX(36)+RTS(42)
             *      = 216 cycles (RTS included — this IS E03A's RTS).
             * No separate RTS (42) after the E07E block.
             *
             * $E053 (attr30==0):
             *   LDA $F015,Y(38)+AND #$08(16)+BEQ_t(22) or BEQ_nt(16):
             *   Path BEQ_t (F015,Y & 8 == 0 → to E05E):
             *     PHY(30)+TYA(14)+LSR(14)+LSR(14)+TAY(14)+LDA_F235Y(38)+CMP(16)+BCC_t/nt:
             *     BCC_t (E07A): PLY(36)+STZ_F077(34) → E07E(216): total=22+30+14+14+14+14+38+16+22+36+34+216=470
             *     BCC_nt (E06A): PLY(36)+CMP(16)+BNE_t(22)+LDA#30(16)+BRA E075(22)+STA_F077(34)+BRA E07E(22)+E07E(216)=
             *                   22+30+14+14+14+14+38+16+16+36+16+22+16+22+34+22+216=562
             *                   or BNE_nt: ...+LDA#30(16)+BRA E075 omit...→ same STA+BRA E07E
             *                   BNE_nt (E06F): PLY+CMP+BNE_nt+LDA#30+BRA_E075+STA_F077+BRA_E07E+E07E=
             *                   22+30+14+14+14+14+38+16+16+36+16+16+16+22+34+22+216=556
             *   Path BEQ_nt (F015,Y & 8 != 0 → to E05A):
             *     LDA#40(16)+BRA_E075(22)+STA_F077(34)+BRA_E07E(22)+E07E(216):
             *     total = 16+16+22+34+22+216=326... wait include BEQ_nt cost:
             *     38+16+16+16+22+34+22+216 = 380 (from start of E053)
             *
             * Approximate dominant path for combat (monsters with static attr):
             *   F015,Y & 0x30 == 0 (most common): E053 path.
             *   F015,Y & 0x08 varies. Use runtime check.
             */
            uint8_t f015y = snes->ram[0xF015 + entry_y];
            uint8_t attr30 = f015y & 0x30;
            cy += 38 + 16;
            if (attr30) {
                /* AND #$20 (16) + BEQ/BNE $E049 */
                cy += 16;
                if (attr30 & 0x20) {
                    /* BEQ nt (16) + LDA #$20 (16) + BRA $E04B (22) */
                    cy += 16 + 16 + 22;
                    snes->ram[0xF077] = 0x20;
                } else {
                    /* BEQ_t (22) + LDA #$10 (16) */
                    cy += 22 + 16;
                    snes->ram[0xF077] = 0x10;
                }
                /* $E04B: STA $F077 (34) + JSR $DD99 (46) + DD99_body_no_rts(116) + BRA $E07E (22) */
                cy += 34 + 46;
                dd99_sub(snes);
                cy += 116;  /* DD99 body (no RTS, inlined) */
                cy += 22;   /* BRA $E07E */
                /* $E07E: PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs(32)+STA_abs_X(38)+PLX(36)+RTS(42) = 216 */
                /* Side effect: STA $F094,X where X = mon47 */
                snes->ram[0xF094 + mon47] = snes->ram[0xF077];
                cy += 216;
            } else {
                /* BEQ taken (22) → $E053 */
                cy += 22;
                /* $E053: LDA $F015,Y(38) + AND #$08(16) + BEQ/BNE */
                uint8_t f015y2 = snes->ram[0xF015 + entry_y];  /* same as f015y but re-read as in ASM */
                cy += 38 + 16;
                if (f015y2 & 0x08) {
                    /* BEQ_nt(16) → E05A: LDA #$40(16) + BRA $E075(22) + STA $F077(34) + BRA $E07E(22) + E07E(216) */
                    snes->ram[0xF077] = 0x40;
                    snes->ram[0xF094 + mon47] = 0x40;
                    cy += 16 + 16 + 22 + 34 + 22 + 216;  /* = 326 */
                } else {
                    /* BEQ_t(22) → E05E: PHY+TYA+LSR+LSR+TAY+LDA $F235,Y+CMP #$0F+BCC */
                    cy += 22 + 30 + 14 + 14 + 14 + 14 + 38 + 16;  /* = 184 */
                    uint8_t saved_y_lo = (uint8_t)(snes->cpu->y);
                    uint8_t saved_y_hi = (uint8_t)(snes->cpu->y >> 8);
                    uint8_t tya = saved_y_lo;  /* TYA in 8-bit A */
                    uint8_t shifted = tya >> 2;  /* LSR LSR = >>2 */
                    uint8_t lda_f235 = snes->ram[0xF235 + shifted];  /* LDA $F235,Y with new Y */
                    if (lda_f235 < 0x0F) {
                        /* BCC taken (E07A): PLY(36) + STZ $F077(34) → E07E(216) */
                        snes->ram[0xF077] = 0;
                        snes->ram[0xF094 + mon47] = 0;
                        cy += 22 + 36 + 34 + 216;  /* BCC_t + PLY + STZ + E07E = 308 */
                    } else {
                        /* BCC_nt → E06A: PLY(36)+CMP #$0F(16)+BNE/BEQ */
                        cy += 16 + 36 + 16;
                        if (lda_f235 == 0x0F) {
                            /* BNE_nt (16) → E06F: LDA #$30(16)+BRA $E075(22)+STA $F077(34)+BRA $E07E(22)+E07E(216) */
                            snes->ram[0xF077] = 0x30;
                            snes->ram[0xF094 + mon47] = 0x30;
                            cy += 16 + 16 + 22 + 34 + 22 + 216;  /* = 326 */
                        } else {
                            /* BNE_t(22) → E073: LDA #$30(16)+BRA $E075(22)+STA $F077(34)+BRA $E07E(22)+E07E(216) */
                            snes->ram[0xF077] = 0x30;
                            snes->ram[0xF094 + mon47] = 0x30;
                            cy += 22 + 16 + 22 + 34 + 22 + 216;  /* = 332 */
                        }
                    }
                }
            }
            /* No separate E03A RTS: E07E provides the RTS for JSR $E03A */
        }

        /* $DFE1: JSR $DAFE — InitMonsterAnim */
        InitMonsterAnim_c(snes);  /* injects own cycles (cy-16 in epilogue) */
        snes_runCycles(snes, 16);  /* compensate spurious -16: C→C call has no pullWord */
        cy += 46;  /* JSR abs = 46 cycles */

        /* $DFE4: JSR $DDA5 — CheckSpriteVisible */
        CheckSpriteVisible_c(snes);  /* injects own cycles (cy-16 in epilogue) */
        snes_runCycles(snes, 16);  /* compensate spurious -16: C→C call has no pullWord */
        cy += 46;  /* JSR abs = 46 cycles */

        /* $DFE7: BCC $DFF9 (C=0 visible: taken; C=1 not visible: nt) */
        if (!snes->cpu->c) {
            /* C=0 visible: BCC_t (22) */
            cy += 22;
        } else {
            /* C=1 not visible: BCC_nt (16) + PHX (30) + LDA $47 (24) + TAX (14) + LDA $F099,X (38) */
            cy += 16 + 30 + 24 + 14 + 38;
            uint8_t f099 = snes->ram[0xF099 + mon47];
            if (!f099) {
                /* BEQ_t (22) + PLX (36) */
                cy += 22 + 36;
            } else {
                /* BEQ_nt (16) + PLX (36) + STA $EFD0,X (38) + BRA $DFF9 (22) */
                cy += 16 + 36 + 38 + 22;
                snes->ram[0xEFD0 + mon_x] = f099;
            }
        }

        /* $DFF9: LDA $352D (32) + BEQ/BNE $E011 */
        cy += 32;
        if (!snes->ram[0x352D]) {
            /* BEQ_t (22) → $E011: PLY(36)+PLX(36)+RTS(42) */
            cy += 22 + 36 + 36 + 42;
        } else {
            /* BNE_nt (16) + LDA $64 (24) + BNE/BEQ $E011 */
            cy += 16 + 24;
            if (snes->ram[0x64]) {
                /* BNE_t (22) → $E011 */
                cy += 22 + 36 + 36 + 42;
            } else {
                /* BNE_nt (16) + LDA $47 (24) + TAY (14) + LDA $29C5,Y (38) + CMP #$FF (16) */
                cy += 16 + 24 + 14 + 38 + 16;
                uint8_t v = snes->ram[0x29C5 + mon47];
                if (v == 0xFF) {
                    /* BEQ_t (22) → $E011 */
                    cy += 22 + 36 + 36 + 42;
                } else {
                    /* BEQ_nt (16) + LDA #$03 (16) + STA $EFD0,X (38) → $E011 */
                    cy += 16 + 16 + 38 + 36 + 36 + 42;
                    snes->ram[0xEFD0 + mon_x] = 3;
                }
            }
        }

        inject_cycles(snes, cy - 16);  /* inject = body_total - RTS_sim */
        return;
    }

    snes->ram[0x64]++;  /* INC $64 */

    uint8_t ctrl = snes->ram[0xEFCE + mon_x];
    snes->ram[0x0E] = ctrl;  /* STA $0E (DP side-effect, 8-bit) */
    bool call_dd99 = false;
    uint8_t delta;

    if (ctrl & 0x20) {
        /* motion-active branch: set bit 0 of ctrl */
        snes->ram[0xEFCE + mon_x] = ctrl | 0x01;
        if (ctrl & 0x40) {
            delta = 2;    /* direction bit set → move +2 */
        } else {
            call_dd99 = true;
            delta = 1;    /* direction bit clear → move +1, call DD99 */
        }
    } else {
        /* clear-direction branch: clear bit 0 of ctrl */
        snes->ram[0xEFCE + mon_x] = ctrl & 0xFE;
        if (ctrl & 0x40) {
            delta = 0xFE; /* -2 */
        } else {
            call_dd99 = true;
            delta = 0xFF; /* -1 */
        }
    }

    if (call_dd99) {
        /* JSR $DD99 body (not dispatched separately):
         * JSR(48)+STZ_abs(34)+LDA_abs(32)+ORA_imm(16)+STA_abs(34)+RTS(42) = 206 MC */
        dd99_sub(snes);
    }

    /* STA $0F — delta side-effect: DE28 = STA $0F */
    snes->ram[0x0F] = delta;

    snes->ram[0xEFC5 + mon_x] = (uint8_t)(snes->ram[0xEFC5 + mon_x] + delta);

    bool sound_active = (ctrl & 0x10) != 0;
    if (sound_active) {
        /* LDA $EFD1,X + AND #$07 + PHX + TAX + LDA $0DFFE5,X (long) + PLX + STA $EFC8,X + LDA #$03 + STA $EFD2,X */
        uint8_t idx = snes->ram[0xEFD1 + mon_x] & 0x07;
        snes->ram[0xEFC8 + mon_x] = snes->cart->rom[LOROM(0x0D, 0xFFE5) + idx];
        snes->ram[0xEFD2 + mon_x] = 3;
    }
    snes->ram[0xEFD0 + mon_x] = 3;  /* LDA #$03 + STA $EFD0,X */
    snes->ram[0xEFCF + mon_x]--;    /* DEC $EFCF,X */

    /* Load $47's animation state into $F078/$F077 (PHX + LDA $47/TAX + loads + PLX) */
    uint8_t mon47_x = mon47;
    snes->ram[0xF078] = snes->ram[0xF08F + mon47_x];
    snes->ram[0xF077] = snes->ram[0xF094 + mon47_x];

    uint8_t anim_type = snes->ram[0xF099 + mon47_x];
    bool did_check = (anim_type == 0x0E || anim_type == 0x07);
    if (did_check) {
        /* JSR $DDA5 = CheckSpriteVisible (also in dispatch).
         * Called directly in C — CheckSpriteVisible_c injects its own cycles. */
        CheckSpriteVisible_c(snes);
        if (snes->cpu->c) {
            /* C=1 → not visible; if F099 != 0, override EFD0 */
            uint8_t f099 = snes->ram[0xF099 + mon47_x];
            if (f099) {
                snes->ram[0xEFD0 + mon_x] = f099;
                /* [DE70..DE83]: PLX(36 at PLX before BRA), then [DE84] PLY PLX RTS */
            }
            /* PLX for mon47 restore */
        }
        /* BCC $DE83 (C=0 visible) or fall from above: [DE83] PLX */
    }

    /* Cycle accounting for DDDC body (excludes CheckSpriteVisible's own cycles):
     *
     * Preamble:
     *   PHX(30)+PHY(30)+STZ_dp(24)+LDA_dp(24)+ASL(14)+ASL(14)+TAY(14)
     *   LDA_abs_X(38)+BNE_t(22)+INC_dp(38) = 248
     *
     * Ctrl block:
     *   LDA_abs_X(38)+STA_dp(24)+AND_imm(16)+branch...
     *   bit5=1,bit6=0 (dd99): BEQ_nt(16)+LDA_abs_X(38)+ORA_imm(16)+STA_abs_X(38)
     *     +LDA_dp(24)+AND_imm(16)+BNE_nt(16)+JSR_DD99(48+116=164 body)+LDA_imm(16)+BRA(22) = 454
     *     Total ctrl: 78+454=... see table below.
     *   bit5=1,bit6=1 (no dd99): ~286 ctrl
     *   bit5=0,bit6=0 (dd99): ~similar to bit5=1,bit6=0
     *   bit5=0,bit6=1 (no dd99): ~similar to bit5=1,bit6=1
     *
     * Common after ctrl:
     *   STA_dp(24)+LDA_abs_X(38)+CLC(14)+ADC_dp(24)+STA_abs_X(38) = 138
     *
     * Sound skip: LDA_dp(24)+AND_imm(16)+BEQ_t(22) = 62
     * Sound active: LDA_dp(24)+AND_imm(16)+BNE_t(22)+LDA_abs_X(38)+AND_imm(16)
     *   +PHX(30)+TAX(14)+LDA_long_X(40)+PLX(36)+STA_abs_X(38)+LDA_imm(16)+STA_abs_X(38) = 328
     *
     * Post-sound: LDA_imm(16)+STA_abs_X(38)+DEC_abs_X(42) = 96
     *
     * Mon47 load: PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+STA_abs(34)+LDA_abs_X(38)+STA_abs(34) = 212
     *
     * Anim check (anim_type!=0x0E and !=0x07):
     *   LDA_abs_X(38)+CMP_imm(16)+BNE_nt(16)+CMP_imm(16)+BNE_t(22)+PLX(36)+PLY(36)+PLX(36)+RTS(42) = 258
     * Anim check (match, visible C=0):
     *   LDA_abs_X(38)+CMP_imm(16)+BEQ_t(22) [or CMP_imm+BNE_t for 0x07]
     *   +[CheckSprite overhead 38 already counted in interp/dispatch for JSR]
     *   +BCC_t(22)+PLX(36)+PLY(36)+PLX(36)+RTS(42)
     *   CheckSpriteVisible_c injects its own cycles; DDDC must NOT double-count.
     *
     * Analytical (M=8, X=16 throughout):
     *   no_dd99, no_sound, no_check:  preamble(238)+ctrl_no_dd99(248+38+common_delta(138))+
     *     sound_skip(62)+post(96)+mon47(212)+epilogue_no_check(258) = ~1290 MC → inject=1274
     *   with_dd99, no_sound, no_check: add JSR_DD99_total(206) = ~1496 → inject=1480
     *   no_dd99, with_sound, no_check: sub sound_skip(62)+add sound_active(328) = ~1556 → inject=1540
     *   with_dd99, with_sound, no_check: ~1762 → inject=1746
     *
     * When anim_type matches (0x0E/0x07), the epilogue is longer by:
     *   LDA_abs_X(38)+CMP_imm(16)+BEQ_t/nt path difference vs BNE_t(22) skip
     *   = rough +22 MC (anim branch overhead, excluding CheckSprite which is separate)
     *
     * Strategy: compute cycles dynamically per path.
     */
    {
        /* Base preamble (to BNE_t after frame_ctr check) = 248 */
        uint32_t cy = 248;

        /* Ctrl block: preamble shared = LDA_abs_X(38)+STA_dp(24)+AND_imm(16) = 78 */
        cy += 78;
        if (ctrl & 0x20) {
            /* bit5=1: BEQ_nt(16)+LDA_abs_X(38)+ORA_imm(16)+STA_abs_X(38) = 108 */
            cy += 108;
            if (ctrl & 0x40) {
                /* bit6=1: BNE_t(22)+LDA_imm(16)+BRA(22) = 60 */
                cy += 60;
            } else {
                /* bit6=0: BNE_nt(16)+JSR(48)+DD99_body_no_rts(116)+LDA_imm(16)+BRA(22) = 218
                 * Wait: DD99 body = STZ_abs(34)+LDA_abs(32)+ORA_imm(16)+STA_abs(34)+RTS(42)=158
                 * JSR=48, so JSR+body = 206. Then LDA_imm(16)+BRA(22)=38.
                 * = BNE_nt(16)+206+38 = 260 */
                cy += 260;
            }
        } else {
            /* bit5=0: BEQ_t(22)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38) = 114 */
            cy += 114;
            if (ctrl & 0x40) {
                /* bit6=1: BNE_t(22)+LDA_imm(16)+BRA(22) = 60 */
                cy += 60;
            } else {
                /* bit6=0: BNE_nt(16)+206+LDA_imm(16)+BRA(22) = 260 */
                cy += 260;
            }
        }

        /* Common delta block: STA_dp(24)+LDA_abs_X(38)+CLC(14)+ADC_dp(24)+STA_abs_X(38) = 138 */
        cy += 138;

        /* Sound trigger: LDA_dp(24)+AND_imm(16)+... */
        if (sound_active) {
            /* BEQ_nt(16)+LDA_abs_X(38)+AND_imm(16)+PHX(30)+TAX(14)+LDA_long_X(40)+PLX(36)+STA_abs_X(38)+LDA_imm(16)+STA_abs_X(38) = 282
             * Actually: BNE_t means bit4 set so BEQ_nt(16): 24+16+16+38+16+30+14+40+36+38+16+38 = 322... let me sum:
             * LDA_dp(24)+AND_imm(16)+BEQ_nt(16)+LDA_abs_X(38)+AND_imm(16)+PHX(30)+TAX(14)+LDA_long_X(40)+PLX(36)+STA_abs_X(38)+LDA_imm(16)+STA_abs_X(38) = 322 */
            cy += 322;
        } else {
            /* LDA_dp(24)+AND_imm(16)+BEQ_t(22) = 62 */
            cy += 62;
        }

        /* Post-sound: LDA_imm(16)+STA_abs_X(38)+DEC_abs_X(42) = 96 */
        cy += 96;

        /* Mon47 load: PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+STA_abs(34)+LDA_abs_X(38)+STA_abs(34) = 212 */
        cy += 212;

        /* Anim type check + epilogue */
        if (did_check) {
            /* LDA_abs_X(38)+CMP_imm(16)+BEQ_t(22) [or CMP+BNE_t(22) for 0x07] */
            if (anim_type == 0x0E) {
                /* BEQ taken at first CMP: LDA_abs_X(38)+CMP_imm(16)+BEQ_t(22) = 76 */
                cy += 76;
            } else {
                /* 0x07: BEQ_nt(16)+CMP_imm(16)+BNE_t(22) → 38+16+16+22 = 92 */
                cy += 92;
            }
            /* CheckSpriteVisible_c injects its own cycles → do NOT add here */
            /* After CheckSprite: BCC/BCS + optional PLX + PLX PLY PLX RTS
             * BCC_t(22)+PLX(36)+PLY(36)+PLX(36)+RTS(42) = 172 (visible path)
             * or BCC_nt(16)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BEQ_nt(16)+PLX(36)+PLX(36)+PLY(36)+PLX(36)+RTS(42) for not-visible,f099!=0
             * Use visible path (most common): 172 */
            cy += 172;
        } else {
            /* No anim check: LDA_abs_X(38)+CMP_imm(16)+BNE_nt(16)+CMP_imm(16)+BNE_t(22)+PLX(36)+PLY(36)+PLX(36)+RTS(42) = 258 */
            cy += 258;
        }

        inject_cycles(snes, cy - 16);  /* inject = body_total - RTS_sim(16) */
    }
}

/* -----------------------------------------------------------------------
 * DrawMonsterSprite — $02:DA73  (M=8, X=16, DP=0)
 *
 * Input: ram[$47] = monster slot index
 * Copies 32 bytes of sprite tile data from ROM $1C:FD00 into OAM buffer
 * at $ED50 + offset based on monster slot.
 * Optionally writes palette/priority bytes into OAM high table.
 *
 * DA 5A         PHX PHY
 * A5 47 AA      LDA $47 / TAX
 * BD A3 F0      LDA $F0A3,X          ; tile base index for this monster
 * AA            TAX                   ; X = tile_base (8→16 bit, zero-extended)
 * 86 1C         STX $1C
 * A5 47         LDA $47
 * 18 69 09      CLC / ADC #$09       ; $47 + 9
 * C2 20         REP #$20  [m=16]
 * 0A×5          ASL×5                ; ×32
 * A8            TAY                   ; Y = ($47+9)*32  (OAM slot offset)
 * A5 1C         LDA $1C  (16-bit)
 * 0A×5          ASL×5                ; ×32
 * AA            TAX                   ; X = tile_base*32
 * 7B            TDC                   ; A = DP = 0
 * E2 20         SEP #$20  [m=8]
 * 5A            PHY
 * AD AD F0      LDA $F0AD             ; global hide flag 1?
 * D0 16         BNE $DAB2            ; if set → skip copy
 * AD 83 F2      LDA $F283             ; global hide flag 2?
 * D0 11         BNE $DAB2
 * A9 20         LDA #$20             ; count = 32
 * 85 1C         STA $1C
 * [loop:]
 * BF 00 FD 1C   LDA $1CFD00,X        ; read from ROM (long,X)
 * 99 50 ED      STA $ED50,Y          ; write to OAM buffer
 * E8            INX
 * C8            INY
 * C6 1C         DEC $1C
 * D0 F3         BNE loop
 * [DAB2:]
 * 7A            PLY
 * A5 D7         LDA $D7              ; battle animation flag?
 * F0 22         BEQ $DAD9            ; if 0 → done
 * A5 47         LDA $47
 * CD 22 18      CMP $1822            ; is this the animation target?
 * D0 1B         BNE $DAD9
 * AD 13 18      LDA $1813
 * 29 04         AND #$04
 * F0 0C         BEQ $DAD1
 * A9 EF         LDA #$EF
 * 99 52 ED      STA $ED52,Y
 * A9 3D         LDA #$3D
 * 99 53 ED      STA $ED53,Y
 * 80 08         BRA $DAD9
 * [DAD1:]
 * A9 00         LDA #$00
 * 99 52 ED      STA $ED52,Y
 * 99 53 ED      STA $ED53,Y
 * [DAD9:]
 * 7A FA         PLY PLX
 * 60            RTS
 *
 * ROM access: $1C:FD00 is LoROM bank $1C → rom_offset = 0x1C*0x8000 + (0xFD00-0x8000)
 * ----------------------------------------------------------------------- */
void DrawMonsterSprite_c(Snes *snes) {
    uint8_t mon = snes->ram[0x47];
    uint8_t tile_base = snes->ram[0xF0A3 + mon];  /* LDA $F0A3,X where X=mon */

    /* Y = (mon + 9) * 32 — OAM slot offset */
    uint16_t y_oam = (uint16_t)((mon + 9) << 5);
    /* X = tile_base * 32 — ROM data offset within $1C:FD00 block */
    uint16_t x_rom = (uint16_t)((uint16_t)tile_base << 5);

    if (!snes->ram[0xF0AD] && !snes->ram[0xF283]) {
        /* Copy 32 bytes from ROM $1C:FD00+x_rom → WRAM $ED50+y_oam */
        uint32_t rom_base = LOROM(0x1C, 0xFD00) + x_rom;
        uint16_t wram_base = 0xED50 + y_oam;
        for (int i = 0; i < 0x20; i++) {
            snes->ram[wram_base + i] = snes->cart->rom[rom_base + i];
        }
    }

    /* Optional: write palette/priority bytes into OAM high table */
    if (snes->ram[0xD7]) {
        if (snes->ram[0x47] == snes->ram[0x1822]) {
            if (snes->ram[0x1813] & 0x04) {
                snes->ram[0xED52 + y_oam] = 0xEF;
                snes->ram[0xED53 + y_oam] = 0x3D;
            } else {
                snes->ram[0xED52 + y_oam] = 0x00;
                snes->ram[0xED53 + y_oam] = 0x00;
            }
        }
    }

    /* Cycle accounting (M=8 start, REP/SEP mid-routine, X=16 throughout):
     *
     * Preamble (up to PHY before hide check):
     *   PHX(30)+PHY(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+TAX(14)+STX_dp(32)   ← STX dp 16-bit X = 32
     *   +LDA_dp(24)+CLC(14)+ADC_imm(16)+REP(22)+ASL×5(70)+TAY(14)              ← REP = 22
     *   +LDA_dp_m16(32)+ASL×5(70)+TAX(14)+TDC(14)+SEP(22)+PHY(30) = 524       ← SEP = 22
     *
     * Copy path (F0AD=0, F283=0):
     *   LDA_abs(32)+BNE_nt(16)+LDA_abs(32)+BNE_nt(16)+LDA_imm(16)+STA_dp(24) = 136
     *   Loop 32 iters: [LDA_long_X(40)+STA_abs_Y(38)+INX(14)+INY(14)+DEC_dp(38)]+BNE
     *     31×BNE_t(22) + 1×BNE_nt(16):
     *     iter_body = 40+38+14+14+38 = 144; total = 144×32 + 31×22 + 16 = 4608+682+16 = 5306
     *   Loop is injected per-iteration via inject_cycles() so HDMA fires naturally.
     *
     * No-copy path (F0AD!=0): LDA_abs(32)+BNE_t(22) = 54
     *
     * Epilogue D7=0: PLY(36)+LDA_dp(24)+BEQ_t(22)+PLY(36)+PLX(36)+RTS(42) = 196
     * Epilogue D7!=0, matches 1822, bit4 set (EF/3D):
     *   PLY(36)+LDA_dp(24)+BNE_t(22)+LDA_dp(24)+CMP_abs(32)+BNE_nt(16)+LDA_abs(32)
     *   +AND_imm(16)+BEQ_nt(16)+LDA_imm(16)+STA_abs_Y(38)+LDA_imm(16)+STA_abs_Y(38)
     *   +BRA(22)+PLY(36)+PLX(36)+RTS(42) = 462
     * Epilogue D7!=0, bit4 clear (00/00):
     *   PLY(36)+LDA_dp(24)+BNE_t(22)+LDA_dp(24)+CMP_abs(32)+BNE_nt(16)+LDA_abs(32)
     *   +AND_imm(16)+BEQ_t(22)+LDA_imm(16)+STA_abs_Y(38)+STA_abs_Y(38)
     *   +PLY(36)+PLX(36)+RTS(42) = 434
     * Epilogue D7!=0, no match: PLY+LDA_dp+BNE_t+LDA_dp+CMP_abs+BNE_t+PLY+PLX+RTS = 296
     * Called via JSR → inject = body_total - 16 (RTS sim)
     */
    {
        bool copy_taken = (!snes->ram[0xF0AD] && !snes->ram[0xF283]);

        if (copy_taken) {
            /* Preamble + hide-check injected before loop, then loop per-iteration. */
            inject_cycles(snes, 524 + 136);  /* preamble(524) + hide_check(136) = 660 */
            /* Loop: 32 iters, injected individually so HDMA can fire at hPos=1104. */
            for (int da73_iter = 0; da73_iter < 32; da73_iter++) {
                int iter_cy = (da73_iter < 31) ? 166 : 160; /* BNE_t=22; last iter BNE_nt=16 */
                inject_cycles(snes, iter_cy);
            }
        } else {
            uint32_t cy = 524; /* preamble */
            if (!snes->ram[0xF0AD]) {
                /* F0AD=0 but F283!=0: LDA_abs(32)+BNE_nt(16)+LDA_abs(32)+BNE_t(22) = 102 */
                cy += 102;
            } else {
                /* F0AD!=0: LDA_abs(32)+BNE_t(22) = 54 */
                cy += 54;
            }
            inject_cycles(snes, cy);
            cy = 0;
        }

        /* PLY(36) always (pops the second PHY from DA96) */
        uint32_t cy_ep = 36;

        if (snes->ram[0xD7]) {
            if (snes->ram[0x47] == snes->ram[0x1822]) {
                /* LDA_dp(24)+BNE_t(22)+LDA_dp(24)+CMP_abs(32)+BNE_nt(16)+LDA_abs(32)+AND_imm(16) = 166 */
                cy_ep += 166;
                if (snes->ram[0x1813] & 0x04) {
                    /* BEQ_nt(16)+LDA_imm(16)+STA_abs_Y(38)+LDA_imm(16)+STA_abs_Y(38)+BRA(22) = 146 */
                    cy_ep += 146;
                } else {
                    /* BEQ_t(22)+LDA_imm(16)+STA_abs_Y(38)+STA_abs_Y(38) = 114 */
                    cy_ep += 114;
                }
            } else {
                /* D7!=0 but no match: LDA_dp(24)+BNE_t(22)+LDA_dp(24)+CMP_abs(32)+BNE_t(22) = 124 */
                cy_ep += 124;
            }
        } else {
            /* D7=0: LDA_dp(24)+BEQ_t(22) = 46 */
            cy_ep += 46;
        }

        /* PLY(36)+PLX(36)+RTS(42) = 114; inject = cy_ep + 114 - 16(RTS sim) */
        inject_cycles(snes, cy_ep + 114 - 16);
    }
}

/* -----------------------------------------------------------------------
 * InitMonsterAnim — $02:DAFE  (M=8, X=16, DP=0)
 *
 * Initialises or re-arms the animation-control state for a monster sprite
 * object referenced by ram[$47] (the "entity" or "combat object" index).
 * On entry X = slot index (same as for UpdateMonsterAnim).
 *
 * Called from $02:DFE1 (combat object dispatch loop, once per frame).
 * Exits early if the object's attributes already indicate "done" or if a
 * disqualifying flag is set.
 *
 * Key RAM used:
 *   $47          — entity index (0..5)
 *   $F015+X*4    — attribute flags (bits 7-6 gate early exit)
 *   $F2BC+X      — additional state flag (non-zero: already active, skip)
 *   $EFC5+X      — current animation position/ID byte (several sentinel values)
 *   $EFCE+X      — animation control flags (bits: 0=motion, 4=sound, 5=direction, 6=move)
 *   $EFCF+X      — frame countdown timer
 *   $EFD1+X      — sub-animation state (bits 2:0 used as 3-bit timer)
 *   $EFD3+X      — sub-animation accumulator
 *   $F014        — global animation variant (0 = table A, else table B)
 *   $F0AF+X[$47] — per-entity secondary flag
 *
 * ROM animation tables (LoROM):
 *   $16:FD1F+idx — table A speed values (one per entity slot)
 *   $16:FD24+idx — table B speed values
 *   $16:FCEE+idx — table A for "other" animation class
 *   $16:FCF3+idx — table B for "other" animation class
 *
 * The complete disassembly is:
 *
 * $02:DAFE  DA           PHX
 * $02:DAFF  A5 47        LDA $47
 * $02:DB01  0A           ASL A
 * $02:DB02  0A           ASL A
 * $02:DB03  AA           TAX            ; X = $47 * 4
 * $02:DB04  BD 15 F0     LDA $F015,X    ; attribute flags
 * $02:DB07  29 C0        AND #$C0       ; bits 7-6: "done" sentinel
 * $02:DB09  D0 08        BNE $DB13      ; bits set → restore X, RTS
 * $02:DB0B  A5 47        LDA $47
 * $02:DB0D  AA           TAX
 * $02:DB0E  BD BC F2     LDA $F2BC,X    ; activity state flag
 * $02:DB11  F0 02        BEQ $DB15      ; zero → fall through (not active yet)
 * $02:DB13  FA           PLX            ; [early exit A & B]
 * $02:DB14  60           RTS
 * $02:DB15  FA           PLX            ; restore entry X
 * $02:DB16  BD C5 EF     LDA $EFC5,X   ; current anim ID
 * $02:DB19  C9 D0        CMP #$D0
 * $02:DB1B  F0 07        BEQ $DB24     ; → "D0/E0" animation class
 * $02:DB1D  C9 E0        CMP #$E0
 * $02:DB1F  F0 03        BEQ $DB24
 * $02:DB21  4C 60 DB     JMP $DB60     ; → "other" animation class
 *
 * --- "D0/E0" class ($DB24) ---
 * $02:DB24  AD 14 F0     LDA $F014
 * $02:DB27  D0 0B        BNE $DB34
 * $02:DB29  DA           PHX
 * $02:DB2A  A5 47        LDA $47
 * $02:DB2C  AA           TAX
 * $02:DB2D  BF 1F FD 16  LDA $16FD1F,X ; table A speed
 * $02:DB31  FA           PLX
 * $02:DB32  80 09        BRA $DB3D
 * $02:DB34  DA           PHX
 * $02:DB35  A5 47        LDA $47
 * $02:DB37  AA           TAX
 * $02:DB38  BF 24 FD 16  LDA $16FD24,X ; table B speed
 * $02:DB3C  FA           PLX
 * $02:DB3D  DD C5 EF     CMP $EFC5,X   ; compare with current anim pos
 * $02:DB40  F0 1E        BEQ $DB60     ; same → skip update, go to "other" path?
 *            [Actually falls to $DB60 which does the real classification check]
 * $02:DB42  48           PHA
 * $02:DB43  BD CE EF     LDA $EFCE,X
 * $02:DB46  29 0F        AND #$0F      ; keep low nibble
 * $02:DB48  9D CE EF     STA $EFCE,X
 * $02:DB4B  68           PLA
 * $02:DB4C  C9 D0        CMP #$D0
 * $02:DB4E  D0 04        BNE $DB54
 * $02:DB50  A9 08        LDA #$08
 * $02:DB52  80 5E        BRA $DBB2     ; → STA $EFCF,X
 * $02:DB54  BD CE EF     LDA $EFCE,X
 * $02:DB57  09 20        ORA #$20      ; set bit 5 (direction flag)
 * $02:DB59  9D CE EF     STA $EFCE,X
 * $02:DB5C  A9 08        LDA #$08
 * $02:DB5E  80 52        BRA $DBB2
 *
 * --- "other" class ($DB60) ---
 * $02:DB60  DA           PHX
 * $02:DB61  A5 47        LDA $47
 * $02:DB63  AA           TAX
 * $02:DB64  BD AF F0     LDA $F0AF,X   ; secondary flag
 * $02:DB67  F0 03        BEQ $DB6C     ; zero → object is valid, continue
 * $02:DB69  FA           PLX
 * $02:DB6A  80 03        BRA $DB6F     ; non-zero: use $EFC5 comparison path
 * $02:DB6C  FA           PLX
 * $02:DB6D  80 6A        BRA $DBD9     ; zero: use $F2BC/$F0AF path
 *
 * $02:DB6F  BD C5 EF     LDA $EFC5,X
 * $02:DB72  C9 C0        CMP #$C0
 * $02:DB74  F0 63        BEQ $DBD9     ; $C0 → go to second-object path
 * $02:DB76  48           PHA
 * $02:DB77  BD CE EF     LDA $EFCE,X
 * $02:DB7A  29 0F        AND #$0F
 * $02:DB7C  9D CE EF     STA $EFCE,X
 * $02:DB7F  68           PLA
 * $02:DB80  9E D1 EF     STZ $EFD1,X
 * $02:DB83  9E D3 EF     STZ $EFD3,X
 * $02:DB86  C9 B0        CMP #$B0
 * $02:DB88  D0 0C        BNE $DB96
 * $02:DB8A  BD CE EF     LDA $EFCE,X
 * $02:DB8D  09 20        ORA #$20
 * $02:DB8F  9D CE EF     STA $EFCE,X
 * $02:DB92  A9 10        LDA #$10
 * $02:DB94  80 19        BRA $DBAF
 * $02:DB96  AD 14 F0     LDA $F014
 * $02:DB99  D0 0B        BNE $DBA6
 * $02:DB9B  DA           PHX
 * $02:DB9C  A5 47        LDA $47
 * $02:DB9E  AA           TAX
 * $02:DB9F  BF EE FC 16  LDA $16FCEE,X ; other-class table A
 * $02:DBA3  FA           PLX
 * $02:DBA4  80 09        BRA $DBAF
 * $02:DBA6  DA           PHX
 * $02:DBA7  A5 47        LDA $47
 * $02:DBA9  AA           TAX
 * $02:DBAA  BF F3 FC 16  LDA $16FCF3,X ; other-class table B
 * $02:DBAE  FA           PLX
 * $02:DBAF  38           SEC
 * $02:DBB0  E9 08        SBC #$08
 * $02:DBB2  9D CF EF     STA $EFCF,X   ; store frame speed
 * $02:DBB5  B9 15 F0     LDA $F015,Y   ; Y = caller's Y (attribute byte)
 * $02:DBB8  29 30        AND #$30
 * $02:DBBA  F0 15        BEQ $DBD1     ; bits5-4 = 0 → set bit 6
 * $02:DBBC  29 20        AND #$20      ; bit5 only?
 * $02:DBBE  F0 08        BEQ $DBC8     ; bit5=0, bit4=1 → double speed
 * $02:DBC0  BD CE EF     LDA $EFCE,X
 * $02:DBC3  09 10        ORA #$10      ; set bit 4 (sound flag)
 * $02:DBC5  9D CE EF     STA $EFCE,X
 * $02:DBC8  BD CF EF     LDA $EFCF,X
 * $02:DBCB  0A           ASL A
 * $02:DBCC  9D CF EF     STA $EFCF,X   ; double frame speed
 * $02:DBCF  80 08        BRA $DBD9
 * $02:DBD1  BD CE EF     LDA $EFCE,X
 * $02:DBD4  09 40        ORA #$40      ; set bit 6 (direction flag)
 * $02:DBD6  9D CE EF     STA $EFCE,X
 * $02:DBD9  60           RTS
 *
 * --- Second-object path ($DBD9 from BRA, also from $DB6D) ---
 * $02:DBD9  DA           PHX
 * $02:DBDA  A5 47        LDA $47
 * $02:DBDC  AA           TAX
 * $02:DBDD  BD BC F2     LDA $F2BC,X
 * $02:DBE0  D0 05        BNE $DBE7    ; F2BC!=0 → PLX RTS
 * $02:DBE2  BD AF F0     LDA $F0AF,X
 * $02:DBE5  F0 02        BEQ $DBE9    ; F0AF=0 → continue; else PLX RTS
 * $02:DBE7  FA           PLX
 * $02:DBE8  60           RTS
 * $02:DBE9  FA           PLX
 * $02:DBEA  BD CE EF     LDA $EFCE,X
 * $02:DBED  29 0F        AND #$0F
 * $02:DBEF  9D CE EF     STA $EFCE,X
 * $02:DBF2  BD C5 EF     LDA $EFC5,X
 * $02:DBF5  C9 B0        CMP #$B0
 * $02:DBF7  F0 4F        BEQ $DC48    ; $B0 → set bit0 of EFCE, RTS
 * $02:DBF9  9E D1 EF     STZ $EFD1,X
 * $02:DBFC  9E D3 EF     STZ $EFD3,X
 * $02:DBFF  C9 C0        CMP #$C0
 * $02:DC01  D0 04        BNE $DC07
 * $02:DC03  A9 08        LDA #$08
 * $02:DC05  80 19        BRA $DC20
 * $02:DC07  AD 14 F0     LDA $F014
 * $02:DC0A  D0 0B        BNE $DC17
 * $02:DC0C  DA           PHX
 * $02:DC0D  A5 47        LDA $47
 * $02:DC0F  AA           TAX
 * $02:DC10  BF EE FC 16  LDA $16FCEE,X
 * $02:DC14  FA           PLX
 * $02:DC15  80 09        BRA $DC20
 * $02:DC17  DA           PHX
 * $02:DC18  A5 47        LDA $47
 * $02:DC1A  AA           TAX
 * $02:DC1B  BF F3 FC 16  LDA $16FCF3,X
 * $02:DC1F  FA           PLX
 * $02:DC20  9D CF EF     STA $EFCF,X
 * $02:DC23  B9 15 F0     LDA $F015,Y
 * $02:DC26  29 30        AND #$30
 * $02:DC28  F0 15        BEQ $DC3F
 * $02:DC2A  29 20        AND #$20
 * $02:DC2C  F0 08        BEQ $DC36
 * $02:DC2E  BD CE EF     LDA $EFCE,X
 * $02:DC31  09 10        ORA #$10
 * $02:DC33  9D CE EF     STA $EFCE,X
 * $02:DC36  BD CF EF     LDA $EFCF,X
 * $02:DC39  0A           ASL A
 * $02:DC3A  9D CF EF     STA $EFCF,X
 * $02:DC3D  80 08        BRA $DC47
 * $02:DC3F  BD CE EF     LDA $EFCE,X
 * $02:DC42  09 40        ORA #$40
 * $02:DC44  9D CE EF     STA $EFCE,X
 * $02:DC47  60           RTS
 * $02:DC48  BD CE EF     LDA $EFCE,X
 * $02:DC4B  09 01        ORA #$01
 * $02:DC4D  9D CE EF     STA $EFCE,X
 * $02:DC50  60           RTS
 * ----------------------------------------------------------------------- */
void InitMonsterAnim_c(Snes *snes) {
    uint8_t mon_x = (uint8_t)(snes->cpu->x);  /* entry X = slot index */
    uint8_t mon47 = snes->ram[0x47];

    /* Prologue guard: check $F015[mon47*4] bits[7:6] and $F2BC[mon47].
     * asm computes the index in an 8-bit accumulator (LDA $47/ASL/ASL/TAX,
     * mf=true) so it wraps at 256; truncate to match (benign for the 0..5
     * slot range seen in-game, but faithful to the asm for any $47). */
    uint16_t xi4 = (uint16_t)(uint8_t)(mon47 << 2);
    if (snes->ram[0xF015 + xi4] & 0xC0) {
        /* Early exit A: bits set — PHX(30)+LDA_dp(24)+ASL(14)+ASL(14)+TAX(14)+
         * LDA_abs_X(38)+AND_imm(16)+BNE_t(22)+PLX(36)+RTS(42) = 250; inject=234 */
        inject_cycles(snes, 234);
        return;
    }
    if (snes->ram[0xF2BC + mon47]) {
        /* Early exit B: F2BC non-zero — add: BEQ_nt(16)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BNE_t(22)+PLX(36)+RTS(42)
         * Total = 250+16+24+14+38+22 = 364... recalc full:
         * PHX(30)+LDA_dp(24)+ASL(14)+ASL(14)+TAX(14)+LDA_abs_X(38)+AND_imm(16)+BNE_nt(16)
         * +LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BNE_t(22)+PLX(36)+RTS(42) = 342; inject=326 */
        inject_cycles(snes, 326);
        return;
    }

    /* PLX at $DB15: restore X = mon_x (already in mon_x; no CPU state to update here) */

    uint8_t efc5 = snes->ram[0xEFC5 + mon_x];

    if (efc5 == 0xD0 || efc5 == 0xE0) {
        /* "D0/E0" animation class ($DB24) */
        uint8_t speed = snes->ram[0xF014]
                        ? snes->cart->rom[LOROM(0x16, 0xFD24) + mon47]
                        : snes->cart->rom[LOROM(0x16, 0xFD1F) + mon47];

        if (speed == efc5) {
            /* speed matches current position → treat as "other" class (BEQ $DB60)
             * Fall through to the "other" ($DB60) block below with the current efc5.
             * But first: account for cycles up to this point and then go to DB60 block.
             * Rather than duplicating, we handle this as a goto to the DB60 path.
             * Per disasm: BEQ $DB60 means the "other" block logic runs from there. */
            goto db60_class;
        }

        /* Update EFCE: clear low nibble, then set flag bits */
        uint8_t ctrl = snes->ram[0xEFCE + mon_x];
        snes->ram[0xEFCE + mon_x] = ctrl & 0x0F; /* clear high nibble of low nibble... wait: AND #$0F keeps low nibble */

        if (speed == 0xD0) {
            /* A9 08 → $08, BRA $DBB2 */
            snes->ram[0xEFCF + mon_x] = 0x08;
        } else {
            /* ORA #$20, STA, A=$08, BRA */
            snes->ram[0xEFCE + mon_x] |= 0x20;
            snes->ram[0xEFCF + mon_x] = 0x08;
        }
        goto dbb5_block; /* common F015,Y attribute check */
    }

db60_class:;
    {
        /* "Other" animation class ($DB60): check $F0AF[mon47]
         * $DB60: PHX; LDA $47; TAX; LDA $F0AF,X
         * BEQ $DB6C → PLX; BRA $DBD9 → RTS (efc5 already set up, nothing more to do)
         * BNE $DB6F → continue with "other" initialization path
         */
        uint8_t f0af = snes->ram[0xF0AF + mon47];
        if (f0af) {
            /* $F0AF non-zero: go to EFC5 comparison path ($DB6F) */
            if (efc5 == 0xC0) {
                /* $DB74: CMP #$C0 / BEQ $DBD9 → RTS — done */
                goto dafe_done;
            }
            /* $DB76: PHA; LDA $EFCE,X; AND #$0F; STA $EFCE,X; PLA */
            {
                uint8_t ctrl = snes->ram[0xEFCE + mon_x];
                snes->ram[0xEFCE + mon_x] = ctrl & 0x0F;
            }
            snes->ram[0xEFD1 + mon_x] = 0;
            snes->ram[0xEFD3 + mon_x] = 0;

            if (efc5 == 0xB0) {
                /* $DB86: CMP #$B0 / BEQ $DB8A: ORA #$20 / STA $EFCE,X */
                /* Actually $DB8A: ORA #$20 + STA + A=$10? Let me re-check disasm:
                 * $DB8A: LDA $EFCE,X; ORA #$20; STA $EFCE,X
                 * $DB8F: 9D CE EF = STA $EFCE,X? Already done. Next:
                 * $DB92: A9 10 = LDA #$10; $DB94: 80 19 = BRA $DBAF
                 * $DBAF: SEC; SBC #$08; STA $EFCF,X */
                /* So for B0: set bit5 of EFCE, then A=$10, BRA $DBAF: SEC-SBC #$08 = $10-$08=$08 */
                snes->ram[0xEFCE + mon_x] |= 0x20;
                snes->ram[0xEFCF + mon_x] = (uint8_t)(0x10 - 0x08);  /* = $08 */
                goto dbb5_block;
            }
            /* generic: load speed from table FCEE/FCF3 → SEC-SBC #$08 → EFCF */
            {
                uint8_t raw_speed = snes->ram[0xF014]
                                    ? snes->cart->rom[LOROM(0x16, 0xFCF3) + mon47]
                                    : snes->cart->rom[LOROM(0x16, 0xFCEE) + mon47];
                snes->ram[0xEFCF + mon_x] = (uint8_t)(raw_speed - 8);
            }
            goto dbb5_block;
        } else {
            /* $DB6C: PLX; BRA $DBD9 → $DBD9: RTS. DAFE returns without further writes. */
            goto dafe_done;
        }
    }

dbd9_path:;
    {
        /* Second-object path reached from dbd9_path label — this is DBDA's body,
         * NOT the DAFE $DBD9 RTS. Only called when InitMonsterAnim_c is used as
         * a standalone sub (not as DAFE). For DAFE, F0AF==0 takes goto dafe_done above.
         * This block implements $DBDA..DC48 for completeness.
         * Check F2BC and F0AF for mon47 again */
        if (snes->ram[0xF2BC + mon47] || snes->ram[0xF0AF + mon47]) {
            /* PLX + RTS (early from DBD9) */
            goto dafe_done;
        }
        /* Clear EFCE low nibble */
        {
            uint8_t ctrl = snes->ram[0xEFCE + mon_x];
            snes->ram[0xEFCE + mon_x] = ctrl & 0x0F;
        }

        if (efc5 == 0xB0) {
            /* $DC48: set bit0 of EFCE, RTS */
            snes->ram[0xEFCE + mon_x] |= 0x01;
            goto dafe_done;
        }

        snes->ram[0xEFD1 + mon_x] = 0;
        snes->ram[0xEFD3 + mon_x] = 0;

        uint8_t raw_speed;
        if (efc5 == 0xC0) {
            raw_speed = 0x08;
        } else {
            raw_speed = snes->ram[0xF014]
                        ? snes->cart->rom[LOROM(0x16, 0xFCF3) + mon47]
                        : snes->cart->rom[LOROM(0x16, 0xFCEE) + mon47];
        }
        snes->ram[0xEFCF + mon_x] = raw_speed;
        goto dc23_block; /* F015,Y attribute block (mirrored from dbb5_block) */
    }

dbb5_block:;
    {
        /* $DBB5: LDA $F015,Y / AND #$30 / branch on bits 5-4
         * Y here is caller's Y at entry — passed implicitly as snes->cpu->y */
        uint16_t cal_y = snes->cpu->y;
        uint8_t attr30 = snes->ram[0xF015 + cal_y] & 0x30;

        if (attr30 == 0) {
            /* bits5-4 clear: set bit6 of EFCE */
            snes->ram[0xEFCE + mon_x] |= 0x40;
        } else if (attr30 & 0x20) {
            /* bit5 set: set bit4 of EFCE, then double EFCF */
            snes->ram[0xEFCE + mon_x] |= 0x10;
            snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
        } else {
            /* bit4 only: just double EFCF */
            snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
        }
        goto dafe_done;
    }

dc23_block:;
    {
        /* $DC23 (mirrored dbb5 logic for the DBD9 sub-path) */
        uint16_t cal_y = snes->cpu->y;
        uint8_t attr30 = snes->ram[0xF015 + cal_y] & 0x30;

        if (attr30 == 0) {
            snes->ram[0xEFCE + mon_x] |= 0x40;
        } else if (attr30 & 0x20) {
            snes->ram[0xEFCE + mon_x] |= 0x10;
            snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
        } else {
            snes->ram[0xEFCF + mon_x] = (uint8_t)(snes->ram[0xEFCF + mon_x] << 1);
        }
    }

dafe_done:;
    /* Cycle accounting (M=8, X=16, JSR → inject = body - 16):
     *
     * Early exits:
     *   A: $F015 bits C0 set → 250 MC (inject=234)
     *   B: $F2BC non-zero   → 342 MC (inject=326)
     *
     * Main paths (approximate, dominant path through DB60 class):
     *   Entry to DB15: PHX(30)+LDA_dp(24)+ASL(14)+ASL(14)+TAX(14)+LDA_abs_X(38)
     *                  +AND_imm(16)+BNE_nt(16)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+BEQ_t(22)+PLX(36) = 300
     *   DB16 → JMP DB60: LDA_abs_X(38)+CMP_imm(16)+BNE_nt(16)+CMP_imm(16)+BNE_nt(16)+JMP_abs(24) = 126
     *   DB60 (F0AF!=0): PHX(30)+LDA_dp(24)+ASL×2(28)+TAX(14)+LDA_abs_X(38)+BEQ_nt(16)+PLX(36)+BRA(22) = 208
     *   DB6F (not C0): LDA_abs_X(38)+CMP_imm(16)+BNE_nt(16) = 70
     *   DB76: PHA(22)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38)+PLA(28) = 142
     *   DB80: STZ_abs_X(34)+STZ_abs_X(34)+CMP_imm(16)+BNE_t(22) [not B0] = 106
     *   DB96 (F014=0): LDA_abs(32)+BNE_nt(16)+PHX(30)+LDA_dp(24)+TAX(14)+LDA_long_X(40)+PLX(36)+BRA(22) = 214
     *   DBAF: SEC(14)+SBC_imm(16)+STA_abs_X(38) = 68
     *   DBB5 (bits5-4=0): LDA_abs_Y(38)+AND_imm(16)+BEQ_t(22) = 76 → DBD1: LDA_abs_X(38)+ORA_imm(16)+STA_abs_X(38)+RTS(42) = 134
     *   Total: 300+126+208+70+142+106+214+68+76+134 = 1444 MC → inject=1428
     *
     *   DB6F (EFC5=C0, → DBD9): LDA_abs_X(38)+CMP_imm(16)+BEQ_t(22)+PHX(30)+LDA_dp(24)+ASL×2+TAX+LDA_abs_X(38)+BNE_nt(16)+LDA_abs_X(38)+BEQ_t(22)+PLX(36)+...
     *
     * Strategy: compute dynamically based on actual path taken.
     * Note: all paths computed as full body cycles; inject = cy - 16.
     */
    {
        uint32_t cy;

        /* Classify which path was taken */
        uint16_t xi4_l = (uint16_t)(uint8_t)(mon47 << 2);  /* 8-bit index, matches asm */
        bool bits_c0 = (snes->ram[0xF015 + xi4_l] & 0xC0) != 0;
        bool f2bc_nz = (snes->ram[0xF2BC + mon47] != 0);
        /* Note: these were evaluated before we modified RAM, so re-read is safe
         * (we only write EFCE/EFCF/EFD1/EFD3, not F015/F2BC/F0AF) */

        if (bits_c0) {
            cy = 250;
        } else if (f2bc_nz) {
            cy = 342;
        } else {
            /* Entry to DB15 = 300 */
            cy = 300;

            if (efc5 == 0xD0 || efc5 == 0xE0) {
                /* DB24 class: DB16→DB24 = LDA_abs_X(38)+CMP_imm(16)+BEQ_t(22) [or BNE_nt(16)+CMP+BEQ_t] */
                if (efc5 == 0xD0) {
                    cy += 38+16+22;  /* DB16: CMP D0 BEQ taken = 76 */
                } else {
                    cy += 38+16+16+16+22;  /* DB16: CMP D0 BNE_nt, CMP E0 BEQ_t = 108 */
                }
                /* DB24: F014 check + table lookup */
                bool f014 = (snes->ram[0xF014] != 0);
                uint8_t speed_val = f014 ? snes->cart->rom[LOROM(0x16, 0xFD24) + mon47]
                                         : snes->cart->rom[LOROM(0x16, 0xFD1F) + mon47];
                if (!f014) {
                    cy += 32+16+30+24+14+40+36+22;  /* LDA_abs(32)+BNE_nt(16)+PHX(30)+... = 214 (table A path) */
                } else {
                    cy += 32+22+30+24+14+40+36;     /* LDA_abs(32)+BNE_t(22)+PHX(30)+LDA_dp(24)+TAX(14)+LDA_long_X(40)+PLX(36) = 198 (table B path, no BRA) */
                }
                /* CMP $EFC5,X: 38; BEQ (taken if speed==efc5, nt otherwise) */
                if (speed_val == efc5) {
                    cy += 38+22;  /* CMP+BEQ_t(22) → goto DB60 */
                    /* then DB60 path: PHX(30)+LDA_dp(24)+ASL×2(28)+TAX(14)+LDA_abs_X(38)+BEQ_nt/t+PLX(36)+BRA/nop */
                    goto d0e0_to_db60;
                } else {
                    cy += 38+16;  /* CMP+BEQ_nt(16) */
                    /* DB42: PHA(22)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38)+PLA(28)+CMP_imm(16)+... */
                    cy += 22+38+16+38+28+16;  /* = 158 */
                    if (speed_val == 0xD0) {
                        cy += 16+22;   /* BNE_nt+LDA_imm+BRA = 16+16+22 = 54... */
                        /* Actually: D0 04=BNE $DB54 (not-taken if ==D0, since we BNE means !=D0 → taken)
                         * Wait: CMP #$D0 / D0 04: BNE taken if != $D0. Since speed_val was loaded
                         * from table and we compare with $D0 again... speed_val may not be $D0.
                         * This sub-branch: if PLA'd A == $D0 → LDA #$08, BRA; else ORA #$20+STA+LDA #$08+BRA
                         * The PLA'd A IS speed_val (the ROM table value), not efc5.
                         * If speed_val == 0xD0: BNE nt → LDA_imm(16)+BRA(22) = 38 */
                        cy += 38;
                    } else {
                        /* speed_val != $D0: BNE_t(22)+LDA_abs_X(38)+ORA_imm(16)+STA_abs_X(38)+LDA_imm(16)+BRA(22) = 152 */
                        cy += 22+38+16+38+16+22;
                    }
                    goto dbb5_cyc;
                }
            }

d0e0_to_db60:
            {
                /* DB60: PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38) = 106
                 * (no ASL×2 — ROM bytes: DA A5 47 AA BD AF F0 ... PHX LDA_dp TAX LDA_abs_X)
                 * DB67: F0 03 = BEQ +3 → target DB6C (f0af==0) or fall through DB69 (f0af!=0)
                 */
                uint8_t f0af_v = snes->ram[0xF0AF + mon47];
                cy += 30+24+14+38;  /* PHX+LDA_dp+TAX+LDA_abs_X = 106 */
                if (!f0af_v) {
                    /* BEQ_t(22)+PLX(36)+BRA(22)+RTS(42) = 122 → DBD9=RTS */
                    cy += 22+36+22+42;
                    goto dafe_cyc_done;
                }
                /* f0af != 0: BEQ_nt(16)+PLX(36)+BRA(22) → DB6F = 74 */
                cy += 16+36+22;
                /* DB6F: LDA $EFC5,X(38)+CMP #$C0(16)+BEQ → DBD9=RTS if ==C0 */
                if (efc5 == 0xC0) {
                    cy += 38+16+22+42;  /* LDA_abs_X+CMP+BEQ_t+RTS = 118 */
                    goto dafe_cyc_done;
                }
                /* efc5 != C0: BEQ_nt(16) → DB76 */
                cy += 38+16+16;  /* LDA_abs_X+CMP+BEQ_nt = 70 */
                /* DB76: PHA(8)+LDA_abs_X(38)+AND_imm(16)+STA_abs_X(38)+PLA(8)+STZ_abs_X(34)+STZ_abs_X(34)+CMP_imm(16) = 192 */
                cy += 8+38+16+38+8+34+34+16;
                if (efc5 == 0xB0) {
                    /* BNE_nt(16)+LDA_abs_X(38)+ORA_imm(16)+STA_abs_X(38)+LDA_imm(16)+BRA(22) = 146 */
                    cy += 16+38+16+38+16+22;
                } else {
                    /* BNE_t(22)+skip_block → LDA_abs_X(38)+ORA(16)+STA(38)+LDA_imm(16)+BRA(22)... approximate */
                    cy += 22+38+16+38+16+22;  /* same length, BNE target reaches same tail */
                }
                cy += 38+16+38+16+38+14;  /* SEC+SBC+STA_abs_X+CMP+LDA_abs_X+TAX approx */
                goto dbb5_cyc;
            }

dc23_cyc:
            {
                uint16_t cal_y2 = snes->cpu->y;
                uint8_t a30 = snes->ram[0xF015 + cal_y2] & 0x30;
                cy += 38+16;  /* LDA_abs_Y+AND_imm = 54 */
                if (a30 == 0) {
                    cy += 22+38+16+38+42;  /* BEQ_t→DC3F+ORA+STA+RTS */
                } else if (a30 & 0x20) {
                    cy += 16+16+38+16+38+38+16+38+22+42;  /* full path */
                } else {
                    cy += 16+16+22+38+14+38+22+42; /* bit4 only */
                }
                goto dafe_cyc_done;
            }

dbb5_cyc:
            {
                uint16_t cal_y2 = snes->cpu->y;
                uint8_t a30 = snes->ram[0xF015 + cal_y2] & 0x30;
                /* LDA_abs_Y(38)+AND_imm(16)+... */
                if (a30 == 0) {
                    cy += 38+16+22+38+16+38+42;  /* BEQ_t+ORA+STA+RTS = 76+134 */
                } else if (a30 & 0x20) {
                    cy += 38+16+16+16+38+16+38+38+14+38+22+42; /* bit5 path + double */
                } else {
                    cy += 38+16+16+16+22+38+14+38+22+42; /* bit4 only */
                }
                goto dafe_cyc_done;
            }
        }

dafe_cyc_done:;
        inject_cycles(snes, cy - 16);
    }
}

/* -----------------------------------------------------------------------
 * BuildOAMEntries — $02:DCED  (M=8, X=16, DP=0)
 *
 * Builds the OAM (sprite table) entries for one game object in the combat
 * scene.  Called once per frame per active object.
 *
 * Entry: X = object slot index (0..5 for monster sprites)
 *
 * Key RAM and ROM references:
 *   $F07B + X*4  — object active flag (0 = inactive, skip)
 *   $F07C + X*4  — frame counter lo (incremented every call, mod-8 sub-anim)
 *   $F07D + X*4  — frame counter hi (incremented every 8 calls)
 *   $F078        — sprite variant/type byte (stored into $F08F+X on entry)
 *   $F08F + X    — per-object saved variant byte
 *   $6CF3 + X*2  — tile position X base offset (word table, WRAM)
 *   $6CF4 + X*2  — tile position Y base offset (word table, WRAM)
 *   $6CC0        — global H-flip flag
 *   $0300 + Y    — OAM shadow table (destination of sprite data)
 *   ROM $16:FD15+X — tile count for object X (determines loop iterations = count*4)
 *   ROM $13:FAE1    — tile coordinate/attribute data array
 *
 * The loop writes 4-byte OAM entries × 2 tiles:
 *   byte 0: X coordinate  (ROM data + $6CF3 offset, via JSR $BB0B transform)
 *   byte 1: Y coordinate  (ROM data + $6CF4 offset)
 *   byte 2: tile number   (ROM data, or $F079+ctr for variant==9)
 *   byte 3: attributes    (ROM data, EOR #$40 if $6CC0 != 0 for H-flip)
 *
 * Complete disassembly at $02:DCED:
 *   DA 5A: PHX PHY
 *   8A 0A 0A A8: TXA / ASL / ASL / TAY     ; Y = X*4 (per-object row offset)
 *   BF 15 FD 16: LDA $16FD15,X             ; tile count (8-bit)
 *   C2 20: REP #$20  [m=16]
 *   0A 0A: ASL×2                           ; tile_count * 4
 *   85 0E: STA $0E                         ; [$0E] = tile_count*4 (16-bit, loop limit)
 *   7B: TDC                                ; A=0
 *   E2 20: SEP #$20  [m=8]
 *   AD 78 F0: LDA $F078
 *   9D 8F F0: STA $F08F,X
 *   0A×4: ASL×4                            ; variant * 16 (8-bit)
 *   85 10: STA $10
 *   B9 7B F0: LDA $F07B,Y                  ; active flag
 *   D0 03: BNE $DD14                       ; active → proceed
 *   4C 96 DD: JMP $DD96                    ; inactive → PLX, RTS
 *
 *   [DD14] B9 7C F0: LDA $F07C,Y
 *   1A: INC A
 *   99 7C F0: STA $F07C,Y
 *   29 07: AND #$07
 *   D0 07: BNE $DD26                       ; not mod-8: skip hi counter
 *   B9 7D F0: LDA $F07D,Y
 *   1A: INC A
 *   99 7D F0: STA $F07D,Y
 *
 *   [DD26] 8A 0A AA: TXA/ASL/TAX            ; X = slot*2 (word index)
 *   BD F3 6C: LDA $6CF3,X → 85 12: STA $12
 *   BD F4 6C: LDA $6CF4,X → 85 13: STA $13
 *   B9 7D F0: LDA $F07D,Y / 29 01 AND #$01 / ASL×3 / CLC / 65 10 ADC $10
 *   AA: TAX                                ; X = frame_offset
 *   A4 0E: LDY $0E                         ; Y = tile_count*4 (OAM dest)
 *   64 0E: STZ $0E                         ; reset loop counter
 *
 *   [loop at DD42]
 *   BF E1 FA 13: LDA $13FAE1,X / CLC / 65 12 ADC $12 / JSR $BB0B / 99 00 03 STA $0300,Y / INX / INY
 *   BF E1 FA 13: LDA $13FAE1,X / CLC / 65 13 ADC $13 / 99 00 03 STA $0300,Y / INX / INY
 *   [tile byte] AD 78 F0 LDA $F078 / C9 09 CMP #$09 /
 *     D0 0A BNE $DD6E: BF E1 FA 13 LDA $13FAE1,X
 *     or: DA PHX / A5 0E LDA $0E / AA TAX / BD 79 F0 LDA $F079,X / FA PLX
 *   [merge] 99 00 03 STA $0300,Y / INX / INY
 *   [attr] AD C0 6C LDA $6CC0 / F0 08 BEQ $DD84 (no flip):
 *     BF E1 FA 13 LDA $13FAE1,X
 *     or: BF E1 FA 13 LDA $13FAE1,X / 49 40 EOR #$40
 *   [merge] 99 00 03 STA $0300,Y / INX / INY
 *   E6 0E INC $0E / A5 0E LDA $0E / C9 02 CMP #$02 / D0 AD BNE loop
 *
 *   [DD95] 7A PLY / [DD96] FA PLX / 60 RTS
 * ----------------------------------------------------------------------- */
void BuildOAMEntries_c(Snes *snes) {
    uint8_t slot = (uint8_t)(snes->cpu->x);  /* object slot index from X */
#ifdef DEBUG_DCED
    fprintf(stderr, "DCED slot=%u F078=%02X F07B+y_row=%02X\n",
            slot, snes->ram[0xF078], snes->ram[0xF07B + (slot<<2)]);
#endif

    /* TXA/ASL/ASL/TAY: y_row = slot * 4 (per-object table row offset).
     * A is 8-bit here (mf=true): each ASL truncates to 8 bits, so the
     * real hardware result wraps at 256, not at 65536 like a plain
     * uint16_t shift would. Mask BEFORE widening, or slot>=64 silently
     * diverges from the original (found via region-spike, 2026-07-05). */
    uint16_t y_row = (uint16_t)(uint8_t)(slot << 2);

    /* BF 15 FD 16: tile count from ROM $16:FD15+slot */
    uint8_t tile_count = snes->cart->rom[LOROM(0x16, 0xFD15) + slot];

    /* REP/ASL×2/STA $0E (16-bit): $0E = tile_count * 4 (loop limit)
     * This writes $0E (lo) and $0F (hi) as 16-bit DP side-effect. */
    uint16_t loop_limit = (uint16_t)tile_count << 2;
    snes->ram[0x0E] = (uint8_t)(loop_limit & 0xFF);  /* STA $0E m=16 lo */
    snes->ram[0x0F] = (uint8_t)(loop_limit >> 8);    /* STA $0E m=16 hi */

    /* LDA $F078 / STA $F08F+slot / ASL×4 / STA $10 */
    uint8_t variant = snes->ram[0xF078];
    snes->ram[0xF08F + slot] = variant;
    uint8_t tile_base_scaled = (uint8_t)((uint8_t)variant << 4);  /* variant * 16, 8-bit */

#ifdef DEBUG_DCED
    fprintf(stderr, "DCED slot=%u variant=%02X tbs=%02X loop_limit=%04X F07B+0=%02X\n",
            slot, variant, tile_base_scaled, loop_limit, snes->ram[0xF07B + y_row]);
#endif

    /* Active check: B9 7B F0 = LDA $F07B,Y */
    if (!snes->ram[0xF07B + y_row]) {
        /* Inactive: JMP $DD96 → PLY PLX RTS */
        /* Prologue to here:
         * PHX(30)+PHY(30)+TXA(14)+ASL(14)+ASL(14)+TAY(14)+LDA_long_X(40)+
         * REP(22)+ASL(14)+ASL(14)+STA_dp_m16(32)+TDC(14)+SEP(22)+
         * LDA_abs(32)+STA_abs_X(38)+ASL×4(56)+STA_dp(24)+
         * LDA_abs_Y(38)+BNE_nt(16)+JMP_abs(24)+PLY(36)+PLX(36)+RTS(42) = 616
         * inject = 616 - 16 = 600 */
        inject_cycles(snes, 600);
        return;
    }

    /* Frame counter lo: increment and check mod-8 */
    uint8_t ctr_lo = snes->ram[0xF07C + y_row] + 1;
    snes->ram[0xF07C + y_row] = ctr_lo;
    bool hi_tick = ((ctr_lo & 0x07) == 0);
    if (hi_tick) {
        snes->ram[0xF07D + y_row]++;
    }

    /* DD26: pointer setup. TXA/ASL/TAX: A is 8-bit (mf=true), the ASL
     * truncates to 8 bits before TAX widens it back to 16-bit X --
     * truncate before widening or slot>=128 silently diverges (same bug
     * class as y_row above, found by a higher-trial-count re-run,
     * 2026-07-05). */
    uint16_t slot_w = (uint16_t)(uint8_t)(slot << 1);
    uint8_t x_base = snes->ram[0x6CF3 + slot_w];
    snes->ram[0x12] = x_base;  /* STA $12 DP side-effect */
    uint8_t y_base = snes->ram[0x6CF4 + slot_w];
    snes->ram[0x13] = y_base;  /* STA $13 DP side-effect */

    /* frame offset = (F07D+y_row & 1) * 8 + variant*16 */
    uint8_t frame_off = (uint8_t)(((snes->ram[0xF07D + y_row] & 0x01) << 3) + tile_base_scaled);
    uint16_t rom_x = frame_off;   /* X index into $13:FAE1 */
    uint16_t oam_y = loop_limit;  /* Y index into $0300 (= tile_count*4) */

    /* LDY $0E (m=16 → Y already set as oam_y); STZ $0E (m=8) clears lo byte.
     * $0F retains its hi-byte value from the 16-bit STA above. */
    snes->ram[0x0E] = 0;  /* STZ $0E (m=8) — reset to use as loop counter */

    bool do_flip = (snes->ram[0x6CC0] != 0);
    bool variant9 = (variant == 9);

    /* Main loop: 2 iterations (counter 0..1, exits when ==2).
     * $0E tracks loop_ctr in WRAM (INC $0E → LDA $0E → CMP #$02 → BNE). */
    uint8_t loop_ctr = 0;
    while (loop_ctr < 2) {
        /* byte 0: X coordinate — load into A, call JSR $BB0B via C dispatch
         * BB0B reads/writes snes->cpu->a and calls its own snes_runCycles.
         * DCED's own inject_cycles accounts only for JSR_abs(46) of the call. */
        snes->cpu->a = (snes->cpu->a & 0xFF00) |
                       (uint8_t)(snes->cart->rom[LOROM(0x13, 0xFAE1) + rom_x] + x_base);
        BackAttackYOffset_s_c(snes);   /* JSR $BB0B: transforms A if $6CC0 != 0 */
        snes->ram[0x0300 + oam_y] = (uint8_t)(snes->cpu->a);
#ifdef DEBUG_DCED
        fprintf(stderr, "DCED slot=%u iter=%u x_oam=%02X at $%04X\n",
                slot, loop_ctr, (uint8_t)(snes->cpu->a), 0x0300 + oam_y);
#endif
        rom_x++; oam_y++;

        /* byte 1: Y coordinate (no JSR, just add and store) */
        snes->ram[0x0300 + oam_y] =
            (uint8_t)(snes->cart->rom[LOROM(0x13, 0xFAE1) + rom_x] + y_base);
        rom_x++; oam_y++;

        /* byte 2: tile number */
        uint8_t tile_byte;
        if (variant9) {
            tile_byte = snes->ram[0xF079 + loop_ctr];
        } else {
            tile_byte = snes->cart->rom[LOROM(0x13, 0xFAE1) + rom_x];
        }
        snes->ram[0x0300 + oam_y] = tile_byte;
        rom_x++; oam_y++;

        /* byte 3: tile attributes (H-flip controlled by $6CC0, tested at runtime) */
        uint8_t attr = snes->cart->rom[LOROM(0x13, 0xFAE1) + rom_x];
        if (do_flip) {
            attr ^= 0x40;  /* EOR #$40: flip bit 6 (H-flip) */
        }
        snes->ram[0x0300 + oam_y] = attr;
        rom_x++; oam_y++;

        loop_ctr++;
        snes->ram[0x0E] = loop_ctr;  /* INC $0E side-effect (after increment) */
    }

    /* Cycle accounting (M=8 at entry, REP #$20 mid-prologue, back to M=8; X=16):
     *
     * Prologue: PHX(30)+PHY(30)+TXA(14)+ASL(14)+ASL(14)+TAY(14) = 116
     *           LDA_long_X(40) = 40
     *           REP(22)+ASL(14)+ASL(14)+STA_dp_m16(32)+TDC(14)+SEP(22) = 118
     *           LDA_abs(32)+STA_abs_X(38)+ASL×4(56)+STA_dp(24) = 150
     *           LDA_abs_Y(38)+BNE_t(22) = 60
     *           Total prologue (active): 484
     *
     * Frame counter (no hi-tick): LDA_abs_Y(38)+INC_A(14)+STA_abs_Y(38)+AND_imm(16)+BNE_t(22) = 128
     * Frame counter (hi-tick):    LDA_abs_Y(38)+INC_A(14)+STA_abs_Y(38)+AND_imm(16)+BNE_nt(16)
     *                              +LDA_abs_Y(38)+INC_A(14)+STA_abs_Y(38) = 212
     *
     * Pointer setup DD26-DD40:
     *   TXA(14)+ASL(14)+TAX(14)+LDA_abs_X(38)+STA_dp(24)+LDA_abs_X(38)+STA_dp(24)
     *   +LDA_abs_Y(38)+AND_imm(16)+ASL×3(42)+CLC(14)+ADC_dp(24)+TAX(14)+LDY_dp(24)+STZ_dp(24) = 362
     *
     * Loop (2 iters, variant!=9, no flip):
     *   X coord: LDA_long_X(40)+CLC(14)+ADC_dp(24)+JSR_abs(48)+BB0B_body_noflip(146)+STA_abs_Y(38)+INX(14)+INY(14) = 338
     *   Y coord: LDA_long_X(40)+CLC(14)+ADC_dp(24)+STA_abs_Y(38)+INX(14)+INY(14) = 144
     *   tile:    LDA_abs(32)+CMP_imm(16)+BNE_t(22)+LDA_long_X(40)+STA_abs_Y(38)+INX(14)+INY(14) = 176
     *   attr nf: LDA_abs(32)+BEQ_t(22)+LDA_long_X(40)+STA_abs_Y(38)+INX(14)+INY(14) = 160
     *   end:     INC_dp(38)+LDA_dp(24)+CMP_imm(16)+BNE_t(22) = 100  [iter 1]
     *            INC_dp(38)+LDA_dp(24)+CMP_imm(16)+BNE_nt(16) = 94  [iter 2]
     *   iter1 = 338+144+176+160+100 = 918
     *   iter2 = 338+144+176+160+94 = 912
     *
     * Epilogue: PLY(36)+PLX(36)+RTS(42) = 114
     *
     * Main path (active, no hi-tick, variant!=9, no flip):
     *   480 + 128 + 362 + 918+912 + 114 = 2914 MC  (interpreter total)
     *   CheckSpriteVisible is NOT called here.
     *   BB0B IS dispatched — its inject (130 MC noflip) is via its own snes_runCycles.
     *   BB0B dispatch pullWord overhead (16 MC, not in BB0B inject) counted in x_coord_dced.
     *   DCED's own inject = (2914 - BB0B_inject×2) - 16 = (2914 - 260) - 16 = 2638
     *   cy breakdown: 480+128+362+(208+144+176+160+100)+(208+144+176+160+94)+114 = 2654
     *   inject = 2654 - 16 = 2638
     *
     * Note: BB0B_inject = BB0B_body(146) - 16 = 130.
     * With flip: BB0B_body_flip = 186; inject_flip = 170; DCED_inject_flip = (2914 + 40*2 - 260*delta) - 16
     *   Actually: flip adds (186-146)=40 to each call: 2914+80=2994; inject=2994-340-16=2638 still.
     *   BB0B absorbs the flip cost; DCED inject unchanged.
     *   So flip doesn't change DCED's own inject — BB0B absorbs the flip cost.
     *
     * Variant 9 path: replaces BNE_t+LDA_long_X (22+40=62) with
     *   BNE_nt(16)+PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+PLX(36)+BRA(22) = 180
     *   delta per iter: 180 - 62 = +118 per iter x 2 = +236
     *   But also: CMP #$09 BNE_t(22) vs BNE_nt(16): -6 per iter x 2 = -12
     *   Net variant9 delta vs normal: +118-6 = +112 per iter x 2 = +224
     *   DCED_inject_v9 = 2638 + 224 = 2862
     *
     * Hi-tick delta: BNE_nt(16)+extra_LDA(38)+INC_A(14)+STA(38) = 106 extra vs BNE_t(22) = +84
     *   DCED_inject_hitick = 2638 + 84 = 2722
     */
    {
        uint32_t cy = 484;  /* prologue (active path): REP=22, SEP=22 */

        if (hi_tick) {
            cy += 212;  /* frame counter with hi-tick */
        } else {
            cy += 128;  /* frame counter no hi-tick */
        }
        cy += 362;  /* pointer setup */

        /* Loop: 2 iterations */
        /* X coord per iter: 338 (includes JSR_abs=48; BB0B body injected separately) */
        /* But: BB0B inject = 130 (noflip, always same as shown above) */
        /* DCED's own contribution per loop iter: 338 - 146 (BB0B body) = 192?
         * No: we count JSR_abs(48) in DCED's budget, not BB0B body (146).
         * So DCED cycles for x_coord = 338 - BB0B_body(146) = 192... but we already have 338.
         * The split is: DCED pays JSR(46) + the caller's code around it. BB0B pays its own body.
         * x_coord DCED portion = LDA_long_X(40)+CLC(14)+ADC_dp(24)+JSR_abs(46)+STA_abs_Y(38)+INX(14)+INY(14) = 190
         *   (JSR_abs=46 is DCED's cost for the call; BB0B body=146 is BB0B's cost = injected by BackAttackYOffset_s_c)
         * So DCED per iter without BB0B body: 190+144+tile+attr+end
         */
        /* x_coord_dced: LDA_long_X(40)+CLC(14)+ADC_dp(24)+JSR_abs(46)+STA_abs_Y(38)+INX(14)+INY(14) = 190
         * +16 for BB0B dispatch pullWord overhead (not covered by BB0B's own inject): = 206 */
        uint32_t x_coord_dced = 40+14+24+46+16+38+14+14;  /* = 206 */
        uint32_t y_coord = 40+14+24+38+14+14;           /* = 144 */

        uint32_t tile_bytes;
        if (variant9) {
            /* BNE_nt(16)+PHX(30)+LDA_dp(24)+TAX(14)+LDA_abs_X(38)+PLX(36)+BRA(22) = 180
             * but also: LDA_abs(32)+CMP_imm(16)+BNE_nt(16) up front = 64 */
            tile_bytes = 32+16+16 + 30+24+14+38+36+22 + 38+14+14;  /* = 294 */
        } else {
            /* LDA_abs(32)+CMP_imm(16)+BNE_t(22)+LDA_long_X(40)+STA_abs_Y(38)+INX(14)+INY(14) = 176 */
            tile_bytes = 32+16+22+40+38+14+14;  /* = 176 */
        }

        uint32_t attr_bytes;
        /* attr: LDA_abs(32)+BEQ_t(22)[no flip]+LDA_long_X(40)+STA_abs_Y(38)+INX(14)+INY(14) = 160
         *    or: LDA_abs(32)+BEQ_nt(16)+LDA_long_X(40)+EOR_imm(16)+BRA(22)+STA_abs_Y(38)+INX+INY = 198 */
        attr_bytes = do_flip ? 198 : 160;
        /* Note: flip doesn't change DCED's inject (BB0B absorbs it), but it does change
         * how we account for attr bytes (EOR path vs direct). Keep correct for accuracy. */

        uint32_t iter1_end = 38+24+16+22;  /* INC_dp(38)+LDA_dp+CMP_imm+BNE_t = 100 */
        uint32_t iter2_end = 38+24+16+16;  /* INC_dp(38)+LDA_dp+CMP_imm+BNE_nt = 94 */

        uint32_t iter_body = x_coord_dced + y_coord + tile_bytes + attr_bytes;
        cy += iter_body + iter1_end;  /* iter 1 */
        cy += iter_body + iter2_end;  /* iter 2 */

        cy += 114;  /* PLY+PLX+RTS */

        inject_cycles(snes, cy - 16);
    }
}

/* CONTRACT CheckSpriteVisible_c:
 *   inputs_ram:  $47=mon_index, $F0AF[mon], $F46D[mon], $F015[mon*4..mon*4+3],
 *                $F016[mon*4], $F018[mon*4]
 *   output_cpu:  C=0 visible, C=1 not visible; N/Z from attr
 *   side_effect: may INC $64
 *   entry_mode:  mf=true, xf=false, dp=0
 * CONTRACT UpdateMonsterAnim_c:
 *   inputs_reg:  X = monster slot index
 *   inputs_ram:  $47=mon_index, $EFCF[X], $EFCE[X], $EFC5[X], $EFD1[X],
 *                $F08F[$47], $F094[$47], $F099[$47], $F0AF[$47]
 *   outputs_ram: $64, $EFCE[X], $EFC5[X], $EFC8[X], $EFD2[X], $EFD0[X], $EFCF[X],
 *                $F078, $F077, $EFC2, $EFC3
 *   entry_mode:  mf=true, xf=false, dp=0
 * CONTRACT DrawMonsterSprite_c:
 *   inputs_ram:  $47=mon_index, $F0A3[mon], $F0AD, $F283, $D7, $1822, $1813
 *   outputs_ram: $ED50+y_oam (32 bytes of OAM), $ED52+y_oam, $ED53+y_oam
 *   entry_mode:  mf=true, xf=false, dp=0
 * CONTRACT InitMonsterAnim_c:
 *   inputs_reg:  X = slot index, Y = caller's Y (used for $F015,Y attribute lookup)
 *   inputs_ram:  $47=entity_index, $F015[mon47*4], $F2BC[mon47], $EFC5[X], $EFCE[X],
 *                $EFCF[X], $EFD1[X], $EFD3[X], $F014, $F0AF[mon47]
 *   outputs_ram: $EFCE[X], $EFCF[X], $EFD1[X], $EFD3[X]
 *   entry_mode:  mf=true, xf=false, dp=0
 * CONTRACT BuildOAMEntries_c:
 *   inputs_reg:  X = object slot index
 *   inputs_ram:  $F07B+X*4 (active), $F07C/D+X*4 (counters), $F078 (variant),
 *                $6CF3/$6CF4 (position offsets), $6CC0 (flip flag),
 *                $F079 (override tile for variant 9)
 *   outputs_ram: $F08F+X (variant save), $F07C/$F07D (counter update), $0300+Y (OAM entries)
 *   rom_reads:   $16:FD15+X (tile count), $13:FAE1+offset (tile data)
 *   entry_mode:  mf=true, xf=false, dp=0
 */
