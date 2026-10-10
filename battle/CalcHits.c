#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$7E, DP=0
// All inputs/outputs in WRAM (no register I/O — convention battle):
//   in : ram[$38FB] base_hits, ram[$38FA] hit_rate
//   out: ram[$38FD] = number of hits
void CalcHits_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    ram[0x38FD] = 0;                // stz $38fd
    uint8_t base = ram[0x38FB];     // lda $38fb
    if (base == 0) {                // beq @c99e -- A.low = 0 (m=8, B kept), Z=1
        snes->cpu->a &= 0xFF00;
        snes->cpu->z = true;
        snes->cpu->n = false;
        return;
    }
    uint8_t rate = ram[0x38FA];
    bool carry = snes->cpu->c;
    for (uint8_t y = base; y > 0; y--) { // tay / dey / bne loop
        // FIX (2026-07-06, KNOWN_FINDINGS.md F1): Rand99_emu is `void` --
        // the real result lives in snes->cpu->a per SNES calling
        // convention, NOT in a (nonexistent) C return value. The previous
        // `uint8_t r = Rand99_emu(snes);` compiled only because this file
        // doesn't include ff4_helpers.h (implicit-int declaration, matches
        // the device build) and captured whatever garbage happened to be
        // in the return-value slot -- root cause of "every physical attack
        // misses" (a near-constant/wrong r almost always compared as >=
        // rate). Call, then read cpu->a explicitly.
        Rand99_emu(snes);                // jsr Rand99 (delegated)
        uint8_t r = (uint8_t)snes->cpu->a;
        carry = (r >= rate);             // cmp $38fa
        if (r < rate) {                  // cmp $38fa / bcs (inverted!)
            ram[0x38FD]++;               // inc $38fd
        }
    }
    // Exit state: the loop ends on dey -> Y=0, Z=1, N=0; C from the last cmp
    // (dey/inc leave C alone); A/X as the last Rand99 left them. RandXA's
    // `shorti` truncates the 16-bit `tay` count to its low byte on the first
    // call, so the C loop's 8-bit count matches the asm.
    snes->cpu->y = 0;
    snes->cpu->z = true;
    snes->cpu->n = false;
    snes->cpu->c = carry;
}

// PITFALLS: 3 (CMP/BCS inversion: bcs branches when A>=mem, so we
// enter the body when A<mem)
// HELPERS: Rand99_emu(snes) — delegates Rand99 @ $03:858B
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x38FB=1, 0x38FA=1
//   output_ram:  0x38FD=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// STRICT: full-state equivalent (FF4_SPIKE_STRICT=gate) once _emu helpers mirror the firmware (2026-10-10).
// REVERSED_FUNCTION: battle::CalcHits ($C9:87)