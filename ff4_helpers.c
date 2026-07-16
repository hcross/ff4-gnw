#include "ff4_helpers.h"
#include "dispatch_all.h"  /* Mult8_c, Mult16_c, Div16_c, RandXA_c, Rand99_c */
#ifdef DEBUG_UPDATECTRL
#include <stdio.h>
#endif

uint16_t read16(const uint8_t *ram, int addr) { return (uint16_t)ram[addr] | ((uint16_t)ram[addr + 1] << 8); }
void write16(uint8_t *ram, int addr, uint16_t v) { ram[addr]=(uint8_t)(v&0xff); ram[addr+1]=(uint8_t)((v>>8)&0xff); }
__attribute__((weak)) void run_emulated_func(Snes *snes, uint32_t addr) { (void)snes; (void)addr; }
__attribute__((weak)) void CheckNPCBlock_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ClearBit_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ClearNPCMap_emu(Snes *snes) { (void)snes; }
void ExecBtlGfx_emu(Snes *snes) { run_emulated_func(snes, 0x038085u); } /* F1: was no-op weak stub */
void ExecBtlGfx_ext_emu(Snes *snes) { run_emulated_func(snes, 0x028003u); } /* F10: was no-op weak stub */
void ExecDMA_emu(Snes *snes) { run_emulated_func(snes, 0x008B38u); } /* F1: was no-op weak stub */
/* InitDMA_emu: delegates to InitDMA_c (sets DMA0 BBAD=$18 etc. via the bus).
 * Was a no-op weak stub → callers (e.g. CloseYesNoWindow) got an unprogrammed
 * DMA B-bus address. */
void InitDMA_emu(Snes *snes) { InitDMA_c(snes); }
__attribute__((weak)) void InitHWRegs_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void InitInterrupts_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void LoadMap_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void LoadNPCGfx_emu(Snes *snes) { (void)snes; }
void Rand99_emu(Snes *snes) { Rand99_c(snes); }        /* F1: was no-op weak stub */
__attribute__((weak)) void RandAITarget_emu(Snes *snes) { (void)snes; }
void RandXA_emu(Snes *snes) { RandXA_c(snes); }        /* F1: was no-op weak stub */
__attribute__((weak)) void SetMagicStatus2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void SkipAITurn_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void SleepParalyzeEffect_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void Tfr3bppGfx_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void TfrVRAM_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void TfrWaterTiles_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void UpdatePlayerSpeed_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00cd72_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00d7f6_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00df93_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00dfc4_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00e013_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00e075_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _00f167_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _13e058_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _13e58b_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void _13eb60_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void add_msg2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void add_msg3_emu(Snes *snes) { (void)snes; }
/* apply_speed_mod_emu: delegates to ApplySpeedMod_c ($03:9FD8, already dispatched). */
void apply_speed_mod_emu(Snes *snes) { ApplySpeedMod_c(snes); }
__attribute__((weak)) void board_whale_emu(Snes *snes) { (void)snes; }
void calc_dmg_emu(Snes *snes) { CalcDmg_c(snes); } /* F1-adjacent: was no-op stub */
__attribute__((weak)) void check_battle_list_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_player_move_world_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_strong_elem_emu(Snes *snes) { (void)snes; }
/* check_timer_emu: delegates to CheckTimer_c ($03:9788, already dispatched). */
void check_timer_emu(Snes *snes) { CheckTimer_c(snes); }
__attribute__((weak)) void check_weak_elem_emu(Snes *snes) { (void)snes; }
void clear_text_emu(Snes *snes) { run_emulated_func(snes, 0x14FD06u); } /* menu display bug: was no-op weak stub */
void div16_emu(Snes *snes) { Div16_c(snes); }           /* F1: was no-op weak stub */
__attribute__((weak)) void do_fight_cmd_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void do_magic_attack_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void do_multi_attack_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void draw_pos_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void draw_solar_system_sprite_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void exec_sound_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void fade_out_song_slow_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void get_monster_with_status_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void get_next_event_byte_emu(Snes *snes) { (void)snes; }
/* get_tile_prop_emu removed 2026-07-11: its sole caller (UpdateLocalTiles_c)
 * now calls the native GetTileProps_c ($00:9FC2) directly. The weak no-op it
 * used to be here silently disabled the tile lookups on device. */
/* get_timer_ptr_emu: executes GetTimerPtr ($03:8569) in the interpreter.
 * Computes $3598:$3599 = $3530:$3531 + A (timer-array base + offset). */
void get_timer_ptr_emu(Snes *snes) { run_emulated_func(snes, 0x038569u); }
__attribute__((weak)) void give_item_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void init_battle_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void init_char_prop_emu(Snes *snes) { (void)snes; }
#define FF4_HELPERS_LOROM(bank, addr) (((uint32_t)(bank) << 15) | ((addr) & 0x7FFF))
/* init_ctrl_emu: InitCtrl ($14:FDD9) -- copies the 24-byte default
 * button-remap table from ROM ($14:FE86) into WRAM at $1A05 (used by
 * ReadCtrl/UpdateCtrl's map_ctrl_word, see update_ctrl_field_emu below) and
 * again at $1A1D (the "custom" slot, initially identical until the player
 * edits it via the Config menu), then copies the control-scheme selector
 * byte $16A9 into $1A64. FIX (2026-07-06): was a no-op weak stub, silently
 * leaving the remap table zeroed -- harmless for the old raw-bit-copy
 * update_ctrl_field_emu (which never consulted the table), but a hard
 * dependency once that routine got a real, table-driven translation: with
 * the table all zero, map_ctrl_word ORs in nothing for any button, so no
 * input registers at all (menu doesn't even open). Table load only, no
 * cycle injection (InitCtrl runs once at boot, off any hot path). */
void init_ctrl_emu(Snes *snes) {
    if (snes == 0) return;
    for (int i = 0; i < 24; i++) {
        uint8_t b = snes->cart->rom[FF4_HELPERS_LOROM(0x14, 0xFE86) + i];
        snes->ram[0x1A05 + i] = b;
        snes->ram[0x1A1D + i] = b;
    }
    snes->ram[0x1A64] = snes->ram[0x16A9];
}
__attribute__((weak)) void init_sound_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void load_overworld_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void magic_dmg_effect_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void move_player_emu(Snes *snes) { (void)snes; }
void mult16_emu(Snes *snes) { Mult16_c(snes); }         /* F1: was no-op weak stub */
void mult8_emu(Snes *snes)  { Mult8_c(snes);  }         /* F1: was no-op weak stub */
__attribute__((weak)) void no_self_target_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void play_map_song_emu(Snes *snes) { (void)snes; }
void rand_emu(Snes *snes) {                              /* F1: Rand($03:8593) = RandXA(0,255) */
    snes->cpu->x = 0;
    snes->cpu->a = 0xFF;
    RandXA_c(snes);
}
__attribute__((weak)) void rand_summon_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void remove_target_emu(Snes *snes) { (void)snes; }
/* reset_sprites_emu: ResetSprites ($00:9177), called by InitMapRAM_c on
 * every map load/transition. Sets all 128 OAM sprite entries' Y coordinate
 * to $F0 (off-screen, the standard "hide sprite" sentinel: WRAM $0301,X
 * stride 4, 128 entries -> $0301-$04FD) then zeroes the OAM high-table
 * mirror ($0500-$051F, 32 bytes). FIX (2026-07-06): was a no-op weak stub
 * (part of the InitMapRAM_c investigation into the "exit a building,
 * land in the sea, stuck" bug -- see field/InitMapRAM.c). Straight-line
 * WRAM writes only, no branches/state to get wrong. */
void reset_sprites_emu(Snes *snes) {
    if (snes == 0) return;
    for (int i = 0; i < 0x200; i += 4) snes->ram[0x0301 + i] = 0xF0;
    for (int i = 0; i < 0x20; i++) snes->ram[0x0500 + i] = 0x00;
}
/* select_obj_emu: executes SelectObj ($03:8489) in the interpreter.
 * Computes $A6 (char/monster property ptr) and $3530:$3531 (timer-array ptr)
 * from entity index in A; calls Mult8_c ($03:83E0) internally. */
void select_obj_emu(Snes *snes) { run_emulated_func(snes, 0x038489u); }
__attribute__((weak)) void set_magic_status2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_magic_status_emu(Snes *snes) { (void)snes; }
/* set_timer_dur_emu: SetTimerDur ($03:9FCF) — LDY $AB (16-bit), clamp negative→0, STY $D4. */
void set_timer_dur_emu(Snes *snes) {
    uint16_t y = read16(snes->ram, 0xAB);
    if (y & 0x8000) y = 0;
    write16(snes->ram, 0xD4, y);
}
__attribute__((weak)) void target_character_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_monster_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_monster_type_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void tfr_lava_gfx_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void tfr_water_gfx_emu(Snes *snes) { (void)snes; }
/* ReadCtrl ($14:FD90), one call per raw joypad byte (slot 0..3 matches the
 * real asm's X=4..7). Produces an edge+auto-repeat filtered "active" byte
 * (menu_dp $0104-$0107) plus a persisted "prev-raw" byte (menu_dp
 * $0108-$010B) and a repeat-delay counter (menu_dp $010C-$010F) — these are
 * FIXED scratch addresses, not relative to any caller's direct page, exactly
 * as the real routine keeps them at $01xx regardless of who calls UpdateCtrl.
 *
 * Algorithm (ground truth: upstream/notes/ff4j-sfc.asm 14/FDB4-FDD8):
 *   raw = 0, or raw changed since last call -> reset: prev=active=raw,
 *     repeat delay = 0x18 (24 frames before first auto-repeat)
 *   raw unchanged and nonzero (held) -> decrement repeat counter; hits 0 ->
 *     re-fire (active=raw, counter reloaded to 0x03, i.e. fast repeat) else
 *     active=0 (suppressed while waiting out the delay)
 * Returns the "active" byte (edge pulse, or repeat pulse, or 0). */
static uint8_t read_ctrl_byte(Snes *snes, uint8_t raw, int slot) {
    uint16_t active_addr = (uint16_t)(0x0104 + slot);
    uint16_t prev_addr   = (uint16_t)(0x0108 + slot);
    uint16_t rep_addr    = (uint16_t)(0x010C + slot);
    uint8_t prev = snes->ram[prev_addr];
    uint8_t active;

    if (raw == 0 || raw != prev) {
        snes->ram[prev_addr] = raw;
        active = raw;
        snes->ram[rep_addr] = 0x18;
    } else {
        uint8_t rep = (uint8_t)(snes->ram[rep_addr] - 1);
        snes->ram[rep_addr] = rep;
        if (rep == 0) {
            snes->ram[rep_addr] = 0x03;
            active = prev;
        } else {
            active = 0;
        }
    }
    snes->ram[active_addr] = active;
    return active;
}

/* Runs a 16-bit raw/active controller word through the user's button-remap
 * table (12 meaningful bits: B Y Sel Start Up Down Left Right A X L R, MSB
 * first — the low nibble of the low byte is always-zero hardware padding
 * and is correctly never tested). The table lives in WRAM at $1A05 (default,
 * written by InitCtrl from ROM) or $1A05+24 (custom remap), selected by
 * ram[$1A64] bit 0 — read live, never hardcoded, so a player's custom
 * control config (menu Config screen) is honoured automatically. */
static uint16_t map_ctrl_word(Snes *snes, uint16_t word, uint16_t table_off) {
    uint16_t mapped = 0;
    uint16_t shift = word;
    for (int i = 0; i < 12; i++) {
        bool carry = (shift & 0x8000) != 0;
        shift = (uint16_t)(shift << 1);
        if (carry) {
            uint16_t addr = (uint16_t)(0x1A05 + table_off + i * 2);
            mapped |= (uint16_t)(snes->ram[addr] | ((uint16_t)snes->ram[addr + 1] << 8));
        }
    }
    return mapped;
}

/* update_ctrl_field_emu / update_ctrl_emu — real implementations.
 *
 * FIX (2026-07-06): full-fidelity translation of UpdateCtrl ($14:FD12) +
 * its ReadCtrl callee ($14:FD90), replacing an earlier raw-bit-copy
 * shortcut. That shortcut was good enough to advance the title screen (a
 * simple "is A held" check) but broke MENU NAVIGATION: MainMenu_ext (never
 * dispatched, always interpreted — see point 3 below) expects an
 * edge-filtered, auto-repeating "active" word, not a raw held-bit level,
 * at the controller-1 mirror. With the shortcut, a held button stayed
 * "active" forever (no edge), so the menu's cursor never moved — confirmed
 * by reproducing on fixture ff4-port/fixtures/011-worldmap-entry.lss: the
 * menu opens (once interpreted per point 3) but the cursor never leaves
 * the first entry regardless of Down presses. See MemPalace
 * wing=ff4-gnw room=obstacles-and-solutions, "[CHANTIER INPUT/MENU]
 * Diagnostic complet + spec ReadCtrl" (2026-06-30) for the original
 * diagnosis this implements.
 *
 * The original FF4 NMI handler ($00:9450) calls
 *   JSL $018010 (UpdateCtrlField_ext)
 *     -> JSR $805e (UpdateCtrlField)
 *       -> stores 0 at f:$000140 (disable multi-controller update)
 *       -> JSL $14FD03 (UpdateCtrl_ext) -> JMP UpdateCtrl ($14:FD12)
 *
 * UpdateCtrl runs ReadCtrl once per raw joypad byte ($4218/$4219 = ctrl 1
 * low/high, $421A/$421B = ctrl 2 low/high — always 0 on a single-joypad
 * G&W, no multitap), then maps TWO different 16-bit words through the
 * remap table:
 *   - the byte0/1 ACTIVE word (edge+repeat filtered)   -> "ctrl1 mapped"
 *     -> caller_D+$00/$01 -- what MENU NAVIGATION reads (single-fire +
 *     auto-repeat cursor movement).
 *   - the byte0/1 PREV-RAW word (persists while held)  -> "ctrl2 mapped"
 *     -> caller_D+$02/$03 -- what CheckMenu_c's "is X held" check and
 *     field movement read (a continuous, not edge-triggered, level).
 * (Ctrl2's own hardware bytes $421A/$421B are read and processed into
 * ReadCtrl slots 2/3 for fidelity, but — matching the real asm — their
 * result is never read back by UpdateCtrl itself on this single-joypad
 * target.)
 *
 * Multi-controller branch ($16B8 AND $0140 BEQ) skipped: our stub already
 * forces ram[0x0140]=0 every call (see below), which is the real routine's
 * steady state for a single joypad — always takes the "combine both
 * controllers" ($FDB4) path. ReadCtrl's own auto-joypad-busy wait loop
 * ($4212 bit0) is skipped: snes->portAutoRead is already valid by the time
 * this dispatch fires, matching the prior stub's own approach.
 *
 * DIRECT PAGE: the real UpdateCtrl does `PLD` to restore the CALLER's
 * direct-page register *before* its final stores, so the ctrl1/ctrl2
 * mapped mirrors land at (caller_D + $00..$03) — NOT at the internal
 * menu_dp ($0100) used for the $0104-$010F ReadCtrl scratch above, which
 * stays fixed regardless of caller.
 *
 * FIX (2026-07-06): caller_D = snes->cpu->dp, read directly — NOT
 * reconstructed from a fixed stack offset as an earlier version of this
 * stub did. Our dispatch hook fires in place of the callee's entire body,
 * before any of its own PHD/PLD executes, so cpu->dp at that instant is
 * exactly whatever direct page the caller had when it issued the JSR/JSL —
 * no reconstruction needed. The stack-offset version was calibrated only
 * for the NMI -> UpdateCtrlField_ext -> UpdateCtrl_ext chain (caller_D was
 * always $0600 there); it silently produced a garbage address (observed:
 * $82C2) for MainMenu_ext's own direct calls to UpdateCtrl during menu
 * navigation (caller_D=$0100 there, confirmed via cpu->dp instrumentation)
 * — the mapped controller word was computed correctly but stored to the
 * wrong place, so the menu never saw it. Verified cpu->dp matches the old
 * stack reconstruction exactly on the NMI/field path ($0600 both ways) and
 * gives the correct value on the MainMenu_ext path where the stack hack
 * didn't.
 *
 * SELF-HEAL (2026-07-06, same day as the fix above): every savestate
 * captured before this fix was saved under a no-op InitCtrl_ext2_c, so its
 * $1A05-$1A1C button-remap table is baked into WRAM as 24 zero bytes —
 * loading any such savestate (all 11 catalogued fixtures, any interactive
 * SDL save slot from before today) now maps every button to nothing at
 * all, since map_ctrl_word ORs in a real table entry per bit and an
 * all-zero table ORs in nothing regardless of what's pressed. Confirmed by
 * Hoani after rebuilding the SDL build and loading the worldmap fixture:
 * neither field movement nor any menu could be triggered anymore — the
 * fix above is correct but this table-staleness issue makes it
 * unusable in practice without a recapture. Rather than require every
 * existing savestate to be recaptured, detect an all-zero table here and
 * populate it on the spot (init_ctrl_emu is idempotent and cheap — a
 * 24-byte ROM copy, not a hot-path cost). The real default table always
 * has several nonzero bytes (verified against ROM), so this check never
 * false-triggers on a legitimately initialized table. */
void update_ctrl_field_emu(Snes *snes) {
    if (snes == 0) return;

    bool table_uninitialized = true;
    for (int i = 0; i < 24; i++) {
        if (snes->ram[0x1A05 + i] != 0) { table_uninitialized = false; break; }
    }
    if (table_uninitialized) init_ctrl_emu(snes);

    /* Cycle budget: the real UpdateCtrl ($14:FD12) consumes 7022 MC more than
     * our stub.  Measured empirically via cycle-counter watchpoints at two
     * independent synchronisation points (PC=$02:AC0B and $02:BB3A), both
     * showing Δ=7022 MC between the interpreter and dispatch passes. */
    snes_runCycles(snes, 7022);

    uint16_t d = snes->cpu->dp;

    uint16_t port1 = snes->input1 ? snes->portAutoRead[0] : 0;
    uint16_t port2 = snes->input2 ? snes->portAutoRead[1] : 0;
    uint8_t b4218 = (uint8_t)(port1 & 0xFF);
    uint8_t b4219 = (uint8_t)((port1 >> 8) & 0xFF);
    uint8_t b421A = (uint8_t)(port2 & 0xFF);
    uint8_t b421B = (uint8_t)((port2 >> 8) & 0xFF);

    read_ctrl_byte(snes, (uint8_t)(b4218 | b421A), 0);
    read_ctrl_byte(snes, (uint8_t)(b4219 | b421B), 1);
    read_ctrl_byte(snes, b421A, 2);
    read_ctrl_byte(snes, b421B, 3);

    snes->ram[0x01EC] = 0x00;   /* STZ $EC (menu_dp+$EC) -- fidelity, no known reader */

    uint16_t table_off = (uint16_t)((snes->ram[0x1A64] & 0x01) ? 24 : 0);

    uint16_t active_word  = (uint16_t)(snes->ram[0x0104] | ((uint16_t)snes->ram[0x0105] << 8));
    uint16_t ctrl1_mapped = map_ctrl_word(snes, active_word, table_off);

    uint16_t prevraw_word = (uint16_t)(snes->ram[0x0108] | ((uint16_t)snes->ram[0x0109] << 8));
    uint16_t ctrl2_mapped = map_ctrl_word(snes, prevraw_word, table_off);

#ifdef DEBUG_UPDATECTRL
    printf("[updatectrl] d=%04X port1=%04X active=%04X prevraw=%04X ctrl1m=%04X ctrl2m=%04X\n",
           d, port1, active_word, prevraw_word, ctrl1_mapped, ctrl2_mapped);
#endif
    snes->ram[(uint16_t)(d + 0x00)] = (uint8_t)(ctrl1_mapped & 0xFF);
    snes->ram[(uint16_t)(d + 0x01)] = (uint8_t)(ctrl1_mapped >> 8);
    snes->ram[(uint16_t)(d + 0x02)] = (uint8_t)(ctrl2_mapped & 0xFF);
    snes->ram[(uint16_t)(d + 0x03)] = (uint8_t)(ctrl2_mapped >> 8);

    /* f:$000140 is an absolute long store — bank $00 WRAM, DP-independent. */
    snes->ram[0x0140] = 0x00;
}

void update_ctrl_emu(Snes *snes) {
    /* Same job — UpdateCtrl_ext is the inner call. Idempotent if called
     * twice in the same frame. */
    update_ctrl_field_emu(snes);
}
__attribute__((weak)) void update_equip_emu(Snes *snes) { (void)snes; }
void update_scroll_regs_emu(Snes *snes) { run_emulated_func(snes, 0x14FD0Cu); } /* menu display bug: was no-op weak stub */
void update_window_color_emu(Snes *snes) { run_emulated_func(snes, 0x14FD09u); } /* menu display bug: was no-op weak stub */
__attribute__((weak)) void update_zoom_pal_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_frame_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_vblank_event_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_vblank_long_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_vblank_short_emu(Snes *snes) { (void)snes; }
