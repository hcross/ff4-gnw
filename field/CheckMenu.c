#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$81, DP=$0600
// This routine checks for the X button press to trigger the menu.
// It handles menu state transitions, specific map-based menu behaviors (tent/cabin),
// and triggers the corresponding menu events.
//
// DIRECT PAGE: the field main loop runs with D=$0600 (confirmed at runtime:
// CheckMenu entry dp=$0600). The asm uses direct-page addressing for $D5/$50/
// $02/$B1/$AB/$C0, i.e. those land at D+$nn = $06nn. The earlier port assumed
// DP=0 and read $00D5 (always 0) → it took the "player control disabled" early
// return every frame, so the X button was never tested and the menu never
// opened in native mode (it worked interpreted, where the asm honours D=$0600).
// Fix: honour the runtime DP for every zero-page access via ZP().
// The $1xxx/$0Fxx/$16xx accesses are absolute (DB=$81, but <$2000 → WRAM mirror),
// so they stay direct ram[] indices.
void CheckMenu_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    uint16_t dp = snes->cpu->dp;            // field D=$0600
#define ZP(off) ram[(uint16_t)(dp + (off))]

    if (ZP(0xD5) == 0) {                     // lda $d5 / bne @81f9
        return;
    }

    if (ZP(0x50) != 0) {                     // lda $50 / beq @8200
        return;
    }

    // Check if X button is pressed (JOY_X = 0x40)
    if ((ZP(0x02) & 0x40) == 0) {            // lda $02 / and #JOY_X / bne @8209
        return;
    }

    ZP(0x50) = 0x01;                         // sta $50

    uint8_t target_val;
    if (ram[0x1700] == 0x03) {              // lda $1700 / cmp #$03 / bne @821e
        target_val = (ram[0x0FDB] & 0x30) | 0x80;
    } else {
        if (ram[0x1704] == 0) {              // lda $1704 / bne @8228
            target_val = 0x40;               // lda #$40
        } else {
            target_val = 0x00;               // lda #$00
        }
    }
    ram[0x1A04] = target_val;               // sta $1a04

    fade_out_menu_emu(snes);                 // jsr FadeOutMenu
    main_menu_emu(snes);                     // jsl MainMenu_ext

    if (ram[0x1700] == 0x03) {              // lda $1700 / cmp #$03 / bne @8241
        write16(ram, 0x0CDD, read16(ram, 0x16AA)); // ldx $16aa / stx $0cdd
    }

    fade_in_menu_emu(snes);                  // jsr FadeInMenu

    uint8_t event = ram[0x1A03];
    if (event == 0) {                       // lda $1a03 / beq @828f
        return;
    }

    ZP(0xB1) = 0x01;                         // sta $b1
    ZP(0xAB) = 0;                            // stz $ab

    uint8_t exec_val;
    if (event < 0x03) {                     // cmp #$03 / bcs @825f
        exec_val = (uint8_t)(event + 0x76); // clc / adc #$76 (Pitfall 7)
    } else if (event == 0x03) {
        exec_val = 0x87;
    } else if (event == 0x04) {
        exec_val = 0x86;
    } else if (event == 0x05) {
        exec_val = 0xFB;
    } else if (event == 0x06) {
        ZP(0xC0) = 0x01;                     // sta $c0
        // jmp @828a skips ExecEvent
        ZP(0xB1) = 0;                        // stz $b1
        play_map_song_emu(snes);            // jsr PlayMapSong
        return;
    } else {
        exec_val = 0xDB;
    }

    snes->cpu->a = exec_val;                // lda #val / jmp @8287
    exec_event_emu(snes);                    // jsr ExecEvent

    ZP(0xB1) = 0;                            // stz $b1
    play_map_song_emu(snes);                 // jsr PlayMapSong

#undef ZP
}

// PITFALLS: 7 (ADC 8-bit truncation), 1 (DB=$81), DP=$0600 (zero-page = D+$nn)
// HELPERS: fade_out_menu_emu, main_menu_emu, fade_in_menu_emu,
//          exec_event_emu, play_map_song_emu
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x06D5=1, 0x0650=1, 0x0602=1, 0x1700=1, 0x1704=1, 0x0FDB=1, 0x1A03=1, 0x16AA=2
//   output_ram:  0x1A04=1
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x81
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::CheckMenu ($00:81F4)
