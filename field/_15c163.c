#include "snes/snes.h"

/* $15:C163 — "update mode 7 zoom hdma table"
 *
 * Full original is ~70 instructions that compose an HDMA table at
 * $7F:5A00 driving Mode 7 zoom (matrix B,C registers $211C/$211D)
 * during world-map exploration, with sub-paths for the big whale
 * vehicle. The very first instructions are an early-out:
 *
 *   LDA $1700      ; map id (0 = title, 1 = sub-map, 3 = world map)
 *   CMP #$03
 *   BNE $C16B      ; → RTL when not on a world map
 *   RTL
 *
 * On the title / new-game / menu / battle / sub-map / dialog screens
 * $1700 != 3, so the early-out fires and the routine is effectively a
 * no-op. The G&W port leans on that: while the title screen is loaded
 * the early-out is enough to unblock the NMI handler at $00:92DB,
 * which lets it reach $00:92E3 (STZ $7E) and release the main wait-
 * vblank loop. The full Mode 7 path stays unported for now — entering
 * the world map would re-surface this routine as a hot miss.
 */
void _15c163_c(Snes *snes) {
    uint8_t map_id = snes->ram[0x1700];
    if (map_id != 3) {
        return;  /* early-out matches the original BNE+RTL path */
    }
    /* TODO: port the Mode 7 world-map HDMA table setup
     * ($00:C16B..$00:C1F? in upstream/notes/ff4j-sfc.asm). For now leave
     * the world map degraded rather than block the NMI handler. */
}

/* PITFALLS: 9 (partial port — world-map zoom won't work until full body
 *           is ported), 12 (WRAM offset)
 * HELPERS: none
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  $00:1700 (map id)
 *   output_ram:  none (world-map path unported)
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_15c163 ($15:C163) — "update mode 7 zoom hdma table"
 */
