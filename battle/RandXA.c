#include "snes/snes.h"

/* RandXA — random number in range [X..A] (8-bit).
 *
 * ASM @ $03:8379.  Called ~30 times in the battle engine.
 *
 * Convention (8-bit A at entry, X width toggled internally):
 *   X  = lower bound  (8-bit on entry; asm does `shorti` immediately)
 *   A  = upper bound  (8-bit)
 *   $96  = low bound stash
 *   $97  = auto-incrementing index into the RNG table at $1900
 *   $1900..19FF = 256-byte random lookup table (seeded at battle init)
 *
 * Result: A = random value in [$96 .. original_A], via modular reduction.
 *         $97 is post-incremented (wraps 8-bit).
 */
/* m=8 throughout: every A write (lda/adc) replaces only the low byte; the
 * high byte B must survive, since a caller's 16-bit-index `tax` after the
 * trailing `longi` copies the full B:A into X. */
#define SET_AL(v) (snes->cpu->a = (uint16_t)((snes->cpu->a & 0xFF00) | (uint8_t)(v)))
/* Early-out compares all hit equality: Z=1, C=1, N=0, V untouched. The
 * trailing `longi` (rep #$10) leaves NZVC alone, so these reach the caller. */
#define SET_FLAGS_CMP_EQ() (snes->cpu->z = true, snes->cpu->c = true, snes->cpu->n = false)

void RandXA_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    /* Simulate shorti at entry: truncate X and Y to 8-bit */
    snes->cpu->x &= 0xFF;
    snes->cpu->y &= 0xFF;

    uint8_t lo = (uint8_t)snes->cpu->x;
    uint8_t hi = (uint8_t)(snes->cpu->a & 0xFF);

    ram[0x96] = lo;                                  /* stx $96 */

    /* cpx #$ff / bne @8383 / bra @83b6 — if lo == 0xFF, skip */
    if (lo == 0xFF) {
        snes_runCycles(snes, 20);
        SET_FLAGS_CMP_EQ();                          /* cpx #$ff: Z=1 C=1 N=0 */
        SET_AL(hi);
        snes->cpu->xf = false;
        return;
    }

    /* cmp #$00 / beq @83b6 — if hi == 0, skip */
    if (hi == 0x00) {
        snes_runCycles(snes, 23);
        SET_FLAGS_CMP_EQ();                          /* cmp #$00 */
        SET_AL(hi);
        snes->cpu->xf = false;
        return;
    }

    /* cmp $96 / beq @83b6 — if hi == lo, skip (return hi unchanged) */
    if (hi == lo) {
        snes_runCycles(snes, 28);
        SET_FLAGS_CMP_EQ();                          /* cmp $96 */
        SET_AL(hi);
        snes->cpu->xf = false;
        return;
    }

    uint8_t idx = ram[0x97];                         /* ldx $97 */
    snes->cpu->x = idx;                              /* update register X state */

    uint8_t range = hi - lo;                         /* sec / sbc $96 (8-bit) */
    snes->cpu->v = ((hi ^ lo) & (hi ^ range) & 0x80) != 0;

    /* cmp #$ff / bne @8399 — if range == 0xFF (full range), just return table value */
    if (range == 0xFF) {
        snes_runCycles(snes, 46);
        SET_AL(ram[0x1900 + idx]);            /* lda $1900,x / bra @83b6 */
        snes->cpu->c = true;                         /* cmp #$ff matched */
        snes->cpu->z = (ram[0x1900 + idx] == 0);
        snes->cpu->n = (ram[0x1900 + idx] & 0x80) != 0;
        snes->cpu->xf = false;
        return;
    }

    /* @8399: range = range + 1 (inc) */
    uint8_t divisor = range + 1;                     /* inc */
    ram[0x3947] = divisor;                           /* sta $3947 */
    ram[0x3948] = 0;                                 /* stz $3948 */

    uint8_t rng_val = ram[0x1900 + idx];             /* lda $1900,x */
    /* tax / stx $3945 — store as 8-bit dividend under shorti (leaves $3946 unchanged) */
    snes->cpu->x = rng_val;                          /* tax */
    ram[0x3945] = rng_val;                           /* stx $3945 (8-bit absolute store) */

    /* longi / jsr Div16 — 16-bit division: $3945/$3947 → $3949 quot, $394B rem */
    snes->cpu->xf = false;
    Div16_c(snes);

    /* shorti / clc / lda $394b / adc $96 — result = remainder + lo */
    snes->cpu->x &= 0xFF;
    snes->cpu->y &= 0xFF;

    uint8_t remainder = ram[0x394B];                 /* lda $394b (8-bit after shorti) */
    uint16_t sum = (uint16_t)remainder + lo;         /* clc / adc $96 */
    uint8_t result = (uint8_t)sum;
    snes->cpu->c = sum > 0xFF;
    snes->cpu->v = (~(remainder ^ lo) & (remainder ^ result) & 0x80) != 0;

    ram[0x97] = idx + 1;                             /* inc $97 (wraps 8-bit) */
    snes->cpu->z = (ram[0x97] == 0);                 /* inc sets N/Z last */
    snes->cpu->n = (ram[0x97] & 0x80) != 0;

    snes_runCycles(snes, 79);
    SET_AL(result);
    snes->cpu->xf = false;
}

// PITFALLS: 2 (shorti/longi mode switches inside the routine)
// SPIKE_EXTRA_SRC: ../../ff4-gnw/battle/Div16.c
// HELPERS: Div16_c(snes) — called directly (strong body, not emu stub)
// CONTRACT:
//   inputs_reg:  a=2(upper_bound; B fuzzed, must survive), x=1(lower_bound)
//   inputs_ram:  0x97=1, 0x1900..19FF=RNG_table
//   output_reg:  a=1(random_result)
//   output_ram:  0x97=1(incremented)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// STRICT: full-state equivalent (FF4_SPIKE_STRICT=gate): B preserved, NZVC per path.
// REVERSED_FUNCTION: battle::RandXA ($03:8379)
