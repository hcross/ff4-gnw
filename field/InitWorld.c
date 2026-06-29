#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$00, DP=0
// Logic:
//   1. Clear floating windows and reset battle flags.
//   2. If not returning from battle (ram[0x85] == 0), set player to face down.
//   3. Initialize hardware registers: BG mode, sprite/BG1 enable, disable color math.
//   4. Play map music if ram[0xB1] is 0.
//   5. Transfer world graphics and invert palette.
void InitWorld_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    remove_float_emu(snes);        // jsr RemoveFloat

    ram[0xD1] = 0;                 // stz $d1
    
    if (ram[0x85] == 0) {          // lda $85 / bne @85b9
        ram[0x1705] = 0x02;        // sta $1705 (face down)
    }

    ram[0x85] = 0;                 // stz $85 (disable battle)
    
    // PPU registers ($21xx) are MMIO (DB=$00), not WRAM. The original port
    // wrote $7E:21xx (no effect: BG mode / screen-enable / color-math never
    // set → wrong video mode on the overworld). Route through the bus.
    snes_write(snes, 0x2105, 0x07);   // hBGMODE — mode 7
    snes_write(snes, 0x212C, 0x11);   // TM — enable sprites + BG1
    snes_write(snes, 0x2130, 0x00);   // CGWSEL
    snes_write(snes, 0x2131, 0x00);   // CGADSUB

    if (ram[0xB1] == 0) {          // lda $b1 / bne @85d2
        play_map_song_emu(snes);   // jsr PlayMapSong
    }

    tfr_world_gfx_emu(snes);       // jsl TfrWorldGfx
    invert_pal_emu(snes);          // jsl InvertPal
}

// PITFALLS: None (Straightforward sequential flow with simple branches)
// HELPERS: 
//   remove_float_emu(snes)   - delegates RemoveFloat @ 85db
//   play_map_song_emu(snes)  - delegates PlayMapSong @ 8d5d
//   tfr_world_gfx_emu(snes)   - delegates TfrWorldGfx @ b181
//   invert_pal_emu(snes)     - delegates InvertPal @ c6d5

// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x85=1, 0xB1=1
//   output_ram:  0x85=1, 0xD1=1, 0x1705=1, 0x212C=1, 0x2130=1, 0x2131=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::InitWorld ($85:00AB)