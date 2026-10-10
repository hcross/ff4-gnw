#include "snes/snes.h"

/* UpdatePlayerSpeed ($00:8302) -- movement speed for the current vehicle.
 *
 *   lda $1704            ; vehicle id (absolute, DB-relative: DB=$00 -> WRAM)
 *   tax                  ; x=16-bit: X = B:A -- the high byte B comes along
 *   lda PlayerSpeedTbl,x ; ROM table at $00:830C (DB-relative, absolute,X)
 *   sta $ac              ; DIRECT PAGE: [D+$AC] -- D=field_dp ($0600) at both
 *   rts                  ; callers ($00:8067, $00:8092), i.e. WRAM $06AC
 *
 * Entry: m=8, x=16, D=$0600, DB=$00 (field init, after `pld`/`plb`).
 *
 * Rewritten 2026-10-10: the LLM port read the table from WRAM $00C0 (it is
 * ROM data), wrote $00AC instead of [D+$AC], declared db=0x83 (the routine
 * address's high byte, a recurring LLM error) and left A/X/NZ unset. Its
 * spike failed 1951/2000 once rerun at the registry PC (strict audit). */
void UpdatePlayerSpeed_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    uint32_t db = (uint32_t)cpu->db << 16;

    uint8_t id = snes_read(snes, db | 0x1704);                  /* lda $1704 */
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | id);
    cpu->x = cpu->a;                                            /* tax (x=16) */

    uint8_t speed = snes_read(snes, (db + 0x830C + cpu->x) & 0xFFFFFF);
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | speed);             /* lda tbl,x */
    cpu->z = (speed == 0);
    cpu->n = (speed & 0x80) != 0;

    snes_write(snes, (uint16_t)(cpu->dp + 0xAC), speed);        /* sta $ac */
}

// PITFALLS: 1 (sta $ac is [D+$AC], D=$0600), 2 (16-bit tax keeps B),
//           ROM table, not WRAM
// HELPERS: none
// CONTRACT:
//   inputs_reg:  a=2(B fuzzed: the 16-bit tax uses it), x=none, y=none
//   inputs_ram:  0x1704=1
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x00
//   entry_flags: z=auto, n=auto
// SPIKE_COMPARE: region
// STRICT: full-state equivalent (FF4_SPIKE_STRICT=gate), 2026-10-10.
// REVERSED_FUNCTION: field::UpdatePlayerSpeed ($00:8302)
