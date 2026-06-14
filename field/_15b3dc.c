#include "snes/snes.h"

/* $15:B3DC — "draw chocobo sprite"
 *
 * Temporary stub: original routine is ~200+ bytes and composes OAM entries
 * for the chocobo sprite. Stubbed to RTL-equivalent (no-op) so the NMI loop
 * progresses past the chocobo path; no chocobo will be drawn on the screen
 * but every other sprite remains untouched (it only reaches here when a
 * chocobo is on the map).
 *
 * TODO: full port — depends on $15:BE47 sub-routine and accesses
 * $170F/$1701/$1704/$1705/$1710/$1711, player position $AD/$AB, and
 * frame counter $7A.
 */
void _15b3dc_c(Snes *snes) {
    (void)snes;
}

/* PITFALLS: 9 (stub — game logic not reproduced, chocobos invisible)
 * HELPERS: none
 * CONTRACT:
 *   inputs_reg:  none (stub)
 *   inputs_ram:  none (stub)
 *   output_ram:  none (stub)
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_15b3dc ($15:B3DC) — "draw chocobo sprite" (STUB)
 */
