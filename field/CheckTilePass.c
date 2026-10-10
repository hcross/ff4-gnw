#include "snes/snes.h"

extern void run_emulated_func(Snes *snes, uint32_t addr);

/* CheckTilePass ($00:AA5A -- annotated $AA58, off-by-2; four callers
 * `jsr $AA5A` at $00:A987/A9A2/A9BD/A9D8, field context m=8, x=16,
 * D=$0600, DB=$00). A = 1 if the player may step onto the tile, else 0.
 *
 *   lda $b1 / beq @+ / lda #0 / rts         ; event running -> blocked
 *   jsr CheckNPCBlock ($AA96) / cmp #0 / bne @pass
 *   lda $0709 / asl / tay                   ; x=16: Y = B:A
 *   lda $a1 / and #$04 / beq @plain         ; bridge tile?
 *   lda $06a1,y / and #$03 / and $d2 / beq @bridge_ok / lda #0 / rts
 *   @bridge_ok: lda $06a1,y / and #$04 / beq @pass / lda #0 / rts
 *   @plain: lda $06a1,y / and $0a / beq @pass / lda #0 / rts
 *   @pass: lda #1 / rts
 *
 * $b1/$a1/$d2/$0a are DIRECT-PAGE operands ([D+n]); $0709/$06a1,y are
 * absolute (DB-relative). Rewritten 2026-10-10 when the entry was re-keyed
 * from the dead $00:AA58 (ADR-009): the old body read D=0 addresses, turned
 * `and $0a` into the constant `& 0x0A`, truncated the 16-bit tay, and called
 * a no-op CheckNPCBlock_emu -- it had never run in game. CheckNPCBlock is
 * delegated to the asm with the firmware's run_emulated_func. */

static uint8_t dp8(Snes *snes, uint8_t off) {
    return snes_read(snes, (uint16_t)(snes->cpu->dp + off));
}

static void set_al(Cpu *cpu, uint8_t v) {
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | v);
    cpu->z = (v == 0);
    cpu->n = (v & 0x80) != 0;
}

void CheckTilePass_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    uint32_t db = (uint32_t)cpu->db << 16;

    if (dp8(snes, 0xB1) != 0) {          /* lda $b1 / beq */
        set_al(cpu, 0x00);               /* lda #$00 */
        return;
    }

    run_emulated_func(snes, 0x00AA96);   /* jsr CheckNPCBlock */
    uint8_t npc = (uint8_t)cpu->a;
    cpu->c = true;                       /* cmp #$00: A >= 0 */
    if (npc != 0) {                      /* bne @aa91 */
        set_al(cpu, 0x01);
        return;
    }

    uint8_t dir = snes_read(snes, db | 0x0709);       /* lda $0709 */
    cpu->c = (dir & 0x80) != 0;                        /* asl */
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | (uint8_t)(dir << 1));
    cpu->y = cpu->a;                                   /* tay (x=16) */
    uint8_t tile = snes_read(snes, (db + 0x06A1 + cpu->y) & 0xFFFFFF);

    uint8_t blocked;
    if (dp8(snes, 0xA1) & 0x04) {                      /* bridge tile */
        if ((tile & 0x03) & dp8(snes, 0xD2))
            blocked = 1;                               /* z-level matches */
        else
            blocked = (tile & 0x04) != 0;
    } else {
        blocked = (tile & dp8(snes, 0x0A)) != 0;       /* and $0a (memory) */
    }
    set_al(cpu, blocked ? 0x00 : 0x01);
}

// PITFALLS: 1 (direct-page operands, D=$0600), off-by-2 entry ($AA5A),
//           `and $0a` is a memory operand, 16-bit tay keeps B
// HELPERS: run_emulated_func(CheckNPCBlock @ $00:AA96)
// SPIKE NOTE: $06B1 (event-running gate) is left at its baseline value --
// fuzzing it sends 255/256 trials down the trivial early exit and starves the
// tile paths (a reinjected `& 0x0A` mutant was caught 4/300000 vs 576/50000).
// CONTRACT:
//   inputs_reg:  a=2, x=none, y=none
//   inputs_ram:  0x0709=1, 0x06A1..06B0=tiles, 0x06D2=1, 0x060A=1
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x00
//   entry_flags: z=auto, n=auto
// SPIKE_COMPARE: region
// REVERSED_FUNCTION: field::CheckTilePass ($00:AA5A)
