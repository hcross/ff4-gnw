#include "snes/snes.h"

extern void SetNpcMapCell_c(Snes *snes);

/* SetPlayerNPCMap ($00:AADA -- annotated $AAD8, off-by-2; callers `jsr $AADA`
 * at $00:A9E1/A9F4, field context m=8, x=16, D=$0600, DB=$00). Marks the
 * cell the player is stepping into as occupied by the player ($AE=$FF) in
 * the NPC map, unless it is a bridge tile with a mismatching z-level.
 *
 *   lda $ab / asl / tay                         ; x=16: Y = B:A
 *   lda $06a1,y / and #$04 / beq @set           ; not a bridge tile
 *   lda $06a1,y / and #$03 / and $d2 / beq @rts
 *   @set: lda $ab / tay                         ; Y = B:A again
 *         lda $1706 / clc / adc $AB0B,y / sta $0c   ; XMoveTbl (ROM)
 *         lda $1707 / clc / adc $AB10,y / sta $0e   ; YMoveTbl (ROM)
 *         lda #$ff / sta $ae / jsr $C347 (SetNpcMapCell_c, dispatched L2)
 *   @rts: rts
 *
 * Operand addresses come from the ROM bytes (the annotation's tables sit at
 * $AB09/$AB0E, 2 bytes early like the routine). $ab/$d2/$0c/$0e/$ae are
 * direct-page ([D+n]). Rewritten 2026-10-10 on re-key (ADR-009): the former
 * "delegate" forced db=$AA and called run_emulated_func($AA:00D8) -- an
 * address with no code; only the dead key kept it from ever running. */

static uint8_t dp8(Snes *snes, uint8_t off) {
    return snes_read(snes, (uint16_t)(snes->cpu->dp + off));
}

static void set_al(Cpu *cpu, uint8_t v) {
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | v);
    cpu->z = (v == 0);
    cpu->n = (v & 0x80) != 0;
}

/* clc / adc abs,y (8-bit): returns the sum, sets NZVC like the 65816. */
static uint8_t adc8(Cpu *cpu, uint8_t a, uint8_t m) {
    uint16_t sum = (uint16_t)(a + m);
    uint8_t r = (uint8_t)sum;
    cpu->c = sum > 0xFF;
    cpu->v = (~(a ^ m) & (a ^ r) & 0x80) != 0;
    set_al(cpu, r);
    return r;
}

void SetPlayerNPCMap_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    uint32_t db = (uint32_t)cpu->db << 16;

    uint8_t dir = dp8(snes, 0xAB);
    cpu->c = (dir & 0x80) != 0;                                   /* asl */
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | (uint8_t)(dir << 1));
    cpu->y = cpu->a;                                              /* tay */
    uint8_t tile = snes_read(snes, (db + 0x06A1 + cpu->y) & 0xFFFFFF);
    if (tile & 0x04) {                                            /* bridge */
        uint8_t m = (uint8_t)(tile & 0x03 & dp8(snes, 0xD2));
        set_al(cpu, m);
        if (m == 0) return;
    }
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | dir);                 /* lda $ab */
    cpu->y = cpu->a;                                              /* tay */
    uint8_t px = adc8(cpu, snes_read(snes, db | 0x1706),
                      snes_read(snes, (db + 0xAB0B + cpu->y) & 0xFFFFFF));
    snes_write(snes, (uint16_t)(cpu->dp + 0x0C), px);
    uint8_t py = adc8(cpu, snes_read(snes, db | 0x1707),
                      snes_read(snes, (db + 0xAB10 + cpu->y) & 0xFFFFFF));
    snes_write(snes, (uint16_t)(cpu->dp + 0x0E), py);
    set_al(cpu, 0xFF);                                            /* lda #$ff */
    snes_write(snes, (uint16_t)(cpu->dp + 0xAE), 0xFF);           /* sta $ae */
    SetNpcMapCell_c(snes);                                        /* jsr $C347 */
}

// PITFALLS: 1 (direct-page operands, D=$0600), off-by-2 entry AND tables,
//           16-bit tay keeps B (twice)
// HELPERS: SetNpcMapCell_c (D00C347, the dispatched target of jsr $C347)
// SPIKE_EXTRA_SRC: ../../ff4-gnw/field/NpcMapCells.c
// CONTRACT:
//   inputs_reg:  a=2, x=2, y=none
//   inputs_ram:  0x06AB=1, 0x06A1..06AA=tiles, 0x06D2=1, 0x1706=1, 0x1707=1
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x00
//   entry_flags: z=auto, n=auto
// SPIKE_COMPARE: region
// REVERSED_FUNCTION: field::SetPlayerNPCMap ($00:AADA)
