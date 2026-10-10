#include "snes/snes.h"

extern void ClearNpcMapCell_c(Snes *snes);

/* ClearPlayerNPCMap ($00:AB15 -- annotated $AB13, off-by-2; sole caller
 * `jsr $AB15` at $00:A967, field context m=8, x=16, D=$0600, DB=$00).
 * Clears the player's cell in the NPC map unless standing on a bridge tile
 * whose z-level doesn't match.
 *
 *   lda $a1 / and #$04 / beq @clear          ; not a bridge tile
 *   lda $a1 / and #$03 / and $d2 / beq @rts  ; bridge, z-level mismatch
 *   @clear: lda $1706 / sta $0c / lda $1707 / sta $0e
 *           jsr $C2FF (ClearNPCMap = ClearNpcMapCell_c, dispatched L2)
 *   @rts:   rts
 *
 * $a1/$d2/$0c/$0e are DIRECT-PAGE operands ([D+n]); $1706/$1707 absolute.
 * Rewritten 2026-10-10 when re-keyed from the dead $00:AB13 (ADR-009): the
 * old body used D=0 addresses and called a firmware no-op ClearNPCMap_emu. */

static uint8_t dp8(Snes *snes, uint8_t off) {
    return snes_read(snes, (uint16_t)(snes->cpu->dp + off));
}

static void set_al(Cpu *cpu, uint8_t v) {
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | v);
    cpu->z = (v == 0);
    cpu->n = (v & 0x80) != 0;
}

void ClearPlayerNPCMap_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    uint32_t db = (uint32_t)cpu->db << 16;

    uint8_t a1 = dp8(snes, 0xA1);
    if (a1 & 0x04) {                                   /* bridge tile */
        uint8_t m = (uint8_t)(a1 & 0x03 & dp8(snes, 0xD2));
        set_al(cpu, m);
        if (m == 0) return;                            /* beq @ab2e */
    }
    uint8_t x = snes_read(snes, db | 0x1706);
    snes_write(snes, (uint16_t)(cpu->dp + 0x0C), x);
    uint8_t y = snes_read(snes, db | 0x1707);
    snes_write(snes, (uint16_t)(cpu->dp + 0x0E), y);
    set_al(cpu, y);                                    /* lda $1707 */
    ClearNpcMapCell_c(snes);                           /* jsr $C2FF */
}

// PITFALLS: 1 (direct-page operands, D=$0600), off-by-2 entry ($AB15)
// HELPERS: ClearNpcMapCell_c (D00C2FF, the dispatched target of jsr $C2FF)
// SPIKE_EXTRA_SRC: ../../ff4-gnw/field/NpcMapCells.c
// CONTRACT:
//   inputs_reg:  a=2, x=none, y=none
//   inputs_ram:  0x06A1=1, 0x06D2=1, 0x1706=1, 0x1707=1
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x00
//   entry_flags: z=auto, n=auto
// SPIKE_COMPARE: region
// REVERSED_FUNCTION: field::ClearPlayerNPCMap ($00:AB15)
