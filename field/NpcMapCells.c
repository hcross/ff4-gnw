#include "snes/snes.h"

/* Field NPC-map cell writers + index computation — the $00:C2FF/$C347/$C357
 * cluster (~32 combined calls/frame on the field map, the second-hottest
 * interpreted group after CalcObjScreenPos).
 *
 * The NPC occupation map lives at $7F:4C00 (WRAM), indexed by
 * idx = y*32 + x. $00:C357 computes that index into $3D/$3E (16-bit,
 * little-endian) from the DP tile coords $0E (y) and $0C (x); the two
 * writers call it and then store through the long-indexed address.
 *
 * ROM-bytes-are-truth: the reference disassembly annotates the index
 * routine at $00:C355; the real entry is $00:C357 ($C355/56 = FA 60, the
 * PLX/RTS tail of $C347). Third off-by-2 of this class (D00F535, D00BDB2).
 *
 * DP is the caller's D (=$0600 in the field engine, probe-verified for all
 * three entries, mf=1 xf=0). $3D/$3E are real outputs of ALL three entries
 * (the writers update them via the shared computation).
 *
 * Exact asm semantics kept: the low index byte is ((y & 7) << 5) + x with
 * an 8-BIT add — a carry out of bit 7 is dropped, not propagated to $3E
 * (harmless in-game: the callers bound x/y to < $20 first, but the spike
 * fuzzes the full byte range, so match the truncation exactly). */

static uint16_t npc_map_index(Snes *snes) {
    uint8_t *ram = snes->ram;
    const uint16_t dp = snes->cpu->dp;
    const uint8_t y = ram[(uint16_t)(dp + 0x0E)];   // LDA $0E
    const uint8_t hi = (uint8_t)(y >> 3);            // 3x LSR $3E
    const uint8_t lo = (uint8_t)((uint8_t)((y & 7) << 5)
                       + ram[(uint16_t)(dp + 0x0C)]); // 3x ROR $3D; ADC $0C (8-bit)
    ram[(uint16_t)(dp + 0x3E)] = hi;
    ram[(uint16_t)(dp + 0x3D)] = lo;
    return (uint16_t)((hi << 8) | lo);
}

/* $00:C357 — compute the NPC-map index into $3D/$3E. */
void SetNpcMapPtr_c(Snes *snes) {
    npc_map_index(snes);
}

/* $00:C2FF — clear the NPC-map cell at the current tile ($7F:4C00+idx = 0). */
void ClearNpcMapCell_c(Snes *snes) {
    const uint16_t idx = npc_map_index(snes);
    snes->ram[0x14C00 + idx] = 0x00;                 // STA $7F4C00,X (A=0)
}

/* $00:C347 — mark the NPC-map cell with the current object ($AE | $80). */
void SetNpcMapCell_c(Snes *snes) {
    const uint16_t dp = snes->cpu->dp;
    const uint16_t idx = npc_map_index(snes);
    snes->ram[0x14C00 + idx] =
        (uint8_t)(snes->ram[(uint16_t)(dp + 0xAE)] | 0x80);
}

// PITFALLS: entries $C2FF/$C347/$C357 verified against ROM bytes (annotation
//   says $C355 — off-by-2); DP caller-relative; 8-bit index add truncates;
//   $3D/$3E written by all three entries; map store is long-indexed (bank
//   $7F), DB-independent.
// HELPERS: none
// REVERSED_FUNCTION: field::NpcMapCells ($00:C2FF, $00:C347, $00:C357)
