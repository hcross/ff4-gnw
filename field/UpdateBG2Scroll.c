#include "snes/snes.h"

/*
 * Helpers for 16-bit DP/WRAM access.
 * All DP-relative reads/writes pass (dp + offset) so the caller's D register
 * is honoured (the field engine runs with D=$0600, not D=$0000).
 * Absolute addresses ($1700, $0FE4) bypass dp.
 */
static inline uint16_t r16(const uint8_t *ram, uint16_t addr) {
    return (uint16_t)(ram[addr] | ((uint16_t)ram[(uint16_t)(addr + 1)] << 8));
}
static inline void w16(uint8_t *ram, uint16_t addr, uint16_t v) {
    ram[addr]     = (uint8_t)(v & 0xFF);
    ram[addr + 1] = (uint8_t)(v >> 8);
}

/*
 * ROM-constant BG2 scroll tables (bank $00):
 *   $F5F9: speed mask per bits[7:6]      —  db $00, $07, $01, $00
 *   $F5FD: h-scroll delta per bits[5:4]  —  dw 0, -1, 0, +1  (16-bit LE)
 *   $F605: v-scroll delta per bits[5:4]  —  dw +1, 0, -1, 0  (16-bit LE)
 *
 * The 65816 computes byte offset Y = (fe4 & 0x30) >> 3 for 16-bit tables.
 * In C we fold to index (fe4 & 0x30) >> 4.
 */
static const uint8_t k_speed_mask[4] = { 0x00, 0x07, 0x01, 0x00 };
static const int16_t k_h_delta[4]    = { 0, -1, 0, 1 };
static const int16_t k_v_delta[4]    = { 1, 0, -1, 0 };

/* Shared body entered after the guard BNE ($F535 onward).
 *
 * guard_val: the value tested by BNE — 0 = proceed, non-zero = early RTS.
 *
 * IMPORTANT: CPU mode flags and DP are NOT overridden here.  The caller
 * runs with D=$0600 (field engine context); all DP-relative accesses must
 * go through dp, not hardcoded $00xx offsets.  We only override mf/xf/db
 * when (and only when) the body actually does 16-bit arithmetic — i.e. after
 * all guard checks pass — and we restore them (via SEP/REP) only when the
 * actual code path does so (continuous scroll path ends with SEP #$20). */
static void update_bg2_scroll_body(Snes *snes, uint8_t guard_val) {
    if (guard_val) return;   /* BNE taken: caller dp/mf unchanged */

    uint8_t  *ram = snes->ram;
    uint16_t  dp  = snes->cpu->dp;   /* D register — $0600 in field context */

    /* Absolute addresses (no dp offset): $1700 and $0FE4 */
    if (ram[0x1700] != 0x03) return;

    uint8_t fe4 = ram[0x0FE4];
    if (!(fe4 & 0xC0)) return;   /* AND #$C0 / BNE proceed */

    /* All paths below modify WRAM and do 16-bit arithmetic.
     * Set the mode flags the routine sets with REP #$20 / SEP #$20. */
    snes->cpu->mf = false;   /* REP #$20 in effect for 16-bit ADC/STA/LDA */
    snes->cpu->xf = false;   /* X/Y remain 16-bit throughout */
    snes->cpu->db = 0;

    if (fe4 & 0x06) {
        /* Parallax path ($F593+) — does 16-bit LDX/STX, 8-bit shifts ------ */
        snes->cpu->mf = true;   /* no REP #$20 in the parallax path */
        uint8_t scale = fe4 & 0xC0;

        if (fe4 & 0x04) {
            /* h-parallax: $5E = $5A, scale by bits[7:6] */
            w16(ram, (uint16_t)(dp + 0x5E), r16(ram, (uint16_t)(dp + 0x5A)));
            if (scale != 0x80) {
                uint16_t v = r16(ram, (uint16_t)(dp + 0x5E));
                w16(ram, (uint16_t)(dp + 0x5E),
                    (scale == 0x40) ? (uint16_t)(v >> 1) : (uint16_t)(v << 1));
            }
        }

        if ((fe4 & 0x06) == 0x04) {
            w16(ram, (uint16_t)(dp + 0x60), 0);
            return;
        }

        if (fe4 & 0x02) {
            w16(ram, (uint16_t)(dp + 0x60), r16(ram, (uint16_t)(dp + 0x5C)));
            if (scale != 0x80) {
                uint16_t v = r16(ram, (uint16_t)(dp + 0x60));
                w16(ram, (uint16_t)(dp + 0x60),
                    (scale == 0x40) ? (uint16_t)(v >> 1) : (uint16_t)(v << 1));
            }
            if ((fe4 & 0x06) == 0x02) {
                w16(ram, (uint16_t)(dp + 0x5E), 0);
            }
        }
        return;
    }

    /* Continuous scroll path ($F54D+) — REP #$20 in effect for 16-bit ADC */
    uint8_t speed_idx = (fe4 & 0xC0) >> 6;
    if (!(ram[(uint16_t)(dp + 0x7A)] & k_speed_mask[speed_idx])) {
        uint8_t dir_idx = (fe4 & 0x30) >> 4;
        w16(ram, (uint16_t)(dp + 0x66),
            (uint16_t)(r16(ram, (uint16_t)(dp + 0x66)) + (uint16_t)k_h_delta[dir_idx]));
        w16(ram, (uint16_t)(dp + 0x68),
            (uint16_t)(r16(ram, (uint16_t)(dp + 0x68)) + (uint16_t)k_v_delta[dir_idx]));
    }
    w16(ram, (uint16_t)(dp + 0x5E),
        (uint16_t)(r16(ram, (uint16_t)(dp + 0x5A)) + r16(ram, (uint16_t)(dp + 0x66))));
    w16(ram, (uint16_t)(dp + 0x60),
        (uint16_t)(r16(ram, (uint16_t)(dp + 0x5C)) + r16(ram, (uint16_t)(dp + 0x68))));

    /* Matches $F58B: LDA #0 / SEP #$20 before RTS in the continuous path */
    snes->cpu->mf = true;
    snes->cpu->a  = (snes->cpu->a & 0xFF00); /* low byte = 0 */
}

/* Bank $16 entry ($16:F533 — dispatch D16F533, untouched/separate from the
 * bank-$00 finding below): loads ram[dp + $C9] as the guard value. This is a
 * genuinely different, valid ROM location (own instruction stream in its own
 * LoROM bank) and is not affected by the bank-$00 disassembly error. */
void UpdateBG2Scroll_c(Snes *snes) {
    update_bg2_scroll_body(snes, snes->ram[(uint16_t)(snes->cpu->dp + 0xC9)]);
}

/* Bank $00's SOLE real entry point, dispatched at $00:F535 (D00F535).
 *
 * FINDING (2026-07-05, see MemPalace wing=ff4-gnw room=obstacles-and-solutions
 * for the full writeup): the disassembly (upstream/notes/ff4j-sfc.asm) had a
 * 2-byte offset error here. It modeled two entry points -- a "full" one at
 * $00:F533 that loads $C9, and a "skip" one at $00:F535 that assumes the
 * caller pre-loaded the guard into A. Direct ROM-byte inspection disproves
 * this: $00:F533 is not even an instruction boundary (it's the operand byte
 * of the PRECEDING routine's `LDX $43` at $00:F532; $00:F534 is that
 * routine's RTS). The real instruction at $00:F535 is `LDA $C9` -- there is
 * only ONE entry point in this bank, and it always loads its own guard byte.
 * The dead $00:F533 dispatch entry was retired (see dispatch_all.c and
 * registry D00F533).
 *
 * This function keeps the name UpdateBG2ScrollSkip_c (rather than being
 * merged into UpdateBG2Scroll_c above) because the registry's `name` field
 * for dispatch D00F535 is not renameable through the sanctioned
 * registry_promote.py write path -- see AGENTS.md on why
 * dispatch_state.jsonl is never hand-edited. Despite the now-inaccurate
 * "Skip" name, the body below is correct: it reads $C9 itself, exactly like
 * UpdateBG2Scroll_c does, because that's what the real $00:F535 asm does. */
void UpdateBG2ScrollSkip_c(Snes *snes) {
    update_bg2_scroll_body(snes, snes->ram[(uint16_t)(snes->cpu->dp + 0xC9)]);
}

// CONTRACT:
//   inputs_ram:  dp+$C9=1, $1700=1(abs), $0FE4=1(abs),
//                dp+$5A=2, dp+$5C=2, dp+$66=2, dp+$68=2, dp+$7A=1
//   output_ram:  dp+$5E=2, dp+$60=2 (plus dp+$66/dp+$68 in continuous path)
//   entry_mode:  mf=any, xf=any, dp=caller's D, db=any
//   NOTE: dp is caller-supplied ($0600 in field engine). All DP accesses
//         use (dp + offset) to honour the caller's Direct Page register.
// REVERSED_FUNCTION: field::UpdateBG2Scroll ($16:F533); UpdateBG2ScrollSkip_c
//   is the real, sole bank-$00 entry despite the disassembly's stale
//   $00:F533/$00:F535 two-entry model -- see the finding above.
