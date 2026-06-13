#include "snes/snes.h"

// LoadMapStack @ $D3:6D — loads map-stack entry indexed by $172C,
// dispatches to _00f167 with a derived argument, then triggers
// sound/music side-effects and clears a few flags.
void LoadMapStack_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    ram[0xCA] = 0;                              // stz $ca

    uint16_t idx = read16(ram, 0x172C);         // ldx $172c (16-bit X)
    uint8_t  val = ram[(0x172E + idx) & 0xFFFF]; // lda $172e,x

    uint8_t push_value;
    if (val >= 0xFB) {                          // cmp #$fb / bcs @d39e
        // branch path: val >= 0xFB
        push_value = (uint8_t)(val - 0xFB);     // sec / sbc #$fb
        ram[0x1706] = ram[(0x172F + idx) & 0xFFFF]; // lda $172f,x / sta $1706
        ram[0x1707] = ram[(0x1730 + idx) & 0xFFFF]; // lda $1730,x / sta $1707
        // $1702 and $1705 are NOT modified in this path
    } else {
        // fall-through: val < 0xFB
        ram[0x1702] = val;                      // sta $1702
        push_value = 3;                         // lda #$03 / pha
        uint8_t b = ram[(0x172F + idx) & 0xFFFF];
        ram[0x1706] = b & 0x3F;                 // and #$3f / sta $1706
        ram[0x1707] = ram[(0x1730 + idx) & 0xFFFF]; // lda $1730,x / sta $1707
        ram[0x1705] = (b & 0xC0) >> 6;          // and #$c0 / lsr6 / sta $1705
    }

    // Common tail (@d3ae) — pla restores A from the stack
    snes->cpu->a = push_value;
    snes->cpu->z = (push_value == 0);
    snes->cpu->n = (push_value & 0x80) != 0;
    _00f167_emu(snes);                          // jsr _00f167

    ram[0x1E05] = 0;                            // stz $1e05
    ExecSound_ext_emu(snes);                    // jsl ExecSound_ext
    play_map_song_emu(snes);                    // jsr PlayMapSong
    ram[0xD6] = 0;                              // stz $d6
}

// PITFALLS:
//   1 (DB=$7E required for absolute addressing — inherited, documented in contract)
//   3 (CMP/BCS inversion: bcs taken when A >= mem, so C body uses `if (val >= 0xFB)`)
//   6 (mode A 8-bit — all lda/sta are byte operations)
//   7 (8-bit truncation on sbc: cast to uint8_t after subtraction)
//   8 (inherited mode: mf=true, xf=false assumed from field module convention)
// HELPERS:
//   read16 (little-endian 16-bit accessor)
//   _00f167_emu(snes)   — delegates _00f167 @ $D3:F167
//   ExecSound_ext_emu(snes) — delegates ExecSound_ext @ $00:8003 (jsl)
//   play_map_song_emu(snes)  — delegates PlayMapSong @ $D3:8D5D
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x172C=2
//   output_ram:  0xCA=1, 0x1702=1, 0x1705=1, 0x1706=1, 0x1707=1, 0x1E05=1, 0xD6=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::LoadMapStack ($D3:6D)