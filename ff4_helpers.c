#include "ff4_helpers.h"
#include "dispatch_all.h"  /* Mult8_c, Mult16_c, Div16_c, RandXA_c, Rand99_c */

uint16_t read16(const uint8_t *ram, int addr) { return (uint16_t)ram[addr] | ((uint16_t)ram[addr + 1] << 8); }
void write16(uint8_t *ram, int addr, uint16_t v) { ram[addr]=(uint8_t)(v&0xff); ram[addr+1]=(uint8_t)((v>>8)&0xff); }
__attribute__((weak)) void run_emulated_func(Snes *snes, uint32_t addr) { (void)snes; (void)addr; }
__attribute__((weak)) void CheckNPCBlock_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ClearBit_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ClearNPCMap_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ExecBtlGfx_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ExecBtlGfx_ext_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ExecDMA_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void InitDMA_emu(Snes *snes) { (void)snes; }
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
__attribute__((weak)) void apply_speed_mod_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void board_whale_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void calc_dmg_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_battle_list_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_player_move_world_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_strong_elem_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_timer_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_weak_elem_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void clear_text_emu(Snes *snes) { (void)snes; }
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
__attribute__((weak)) void get_tile_prop_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void get_timer_ptr_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void give_item_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void init_battle_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void init_char_prop_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void init_ctrl_emu(Snes *snes) { (void)snes; }
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
__attribute__((weak)) void reset_sprites_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void select_obj_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_magic_status2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_magic_status_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_timer_dur_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_character_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_monster_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_monster_type_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void tfr_lava_gfx_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void tfr_water_gfx_emu(Snes *snes) { (void)snes; }
/* update_ctrl_field_emu / update_ctrl_emu — real implementations.
 *
 * The original FF4 NMI handler (\$00:9450) calls
 *   JSL \$018010 (UpdateCtrlField_ext)
 *     -> JSR \$805e (UpdateCtrlField)
 *       -> stores 0 at f:\$000140 (disable multi-controller update)
 *       -> JSL \$14FD03 (UpdateCtrl_ext) -> JMP UpdateCtrl (\$14:FD12)
 *
 * UpdateCtrl reads \$4218/\$4219 (auto-joypad result for ctrl 1) and
 * \$421A/\$421B (ctrl 2), runs each byte through the user-configurable
 * button-mapping table, and ultimately stores the mapped 16-bit result
 * at zero-page bytes \$00..\$03 (\$00..\$01 = ctrl 1, \$02..\$03 = ctrl 2).
 *
 * On G&W the LakeSnes auto-joypad has already populated
 * \$portAutoRead[0] with the 16-bit ctrl 1 word during the emulated
 * VBlank. We side-step the full mapping pass (the default identity
 * mapping is sufficient to advance the title screen) and just stream
 * the raw button bits into the controller mirrors. The byte order
 * matches the title-screen reader (\$15:CA1D):
 *
 *   ctrl[\$02] = low byte (\$4218):  A X L R 0 0 0 0
 *   ctrl[\$03] = high byte (\$4219): B Y Sel Start Up Down Left Right
 *
 * Ctrl 1 (\$00/\$01) and ctrl 2 (\$02/\$03) get the same bits — the
 * G&W has a single joypad and the title only checks one side.
 *
 * DIRECT PAGE: the real UpdateCtrl (\$14:FD12) does `PLD` to restore the
 * CALLER's direct-page register *before* its final `STA \$00` / `STA \$02`,
 * so the mirror lands at (D + \$00..\$03). FF4's NMI and title both run with
 * D=\$0600, and the title reads the A bit via `LDA \$02` at \$00:8705 — i.e.
 * \$0602, not absolute \$0002. Writing absolute \$00..\$03 dropped the input
 * one direct page too low, so the title never saw A and stayed on the logo.
 * Honour D (= snes->cpu->dp at dispatch entry, the NMI's DP at the JSL).
 *
 * f:\$000140 mirror of the disable-multi-controller flag is an absolute
 * long store in the asm (DP-independent), so it stays at \$0140. */
void update_ctrl_field_emu(Snes *snes) {
    if (snes == 0) return;

    /* Cycle budget: the real UpdateCtrl ($14:FD12) consumes 7022 MC more than
     * our stub.  Measured empirically via cycle-counter watchpoints at two
     * independent synchronisation points (PC=$02:AC0B and $02:BB3A), both
     * showing Δ=7022 MC between the interpreter and dispatch passes. */
    snes_runCycles(snes, 7022);

    /* The real UpdateCtrl ($14:FD12) reads the auto-joypad hardware registers
     * and stores the mapped 16-bit button word at DP_restored+$00..$03.
     * DP_restored is the caller's direct-page value (restored by PLD inside
     * the routine from the stack at SP+4:SP+5 at dispatch time).
     *
     * In oracle runs (no actual input) all writes are 0x0000, and the target
     * range happened to already be 0 in the savestate — the DP reconstruction
     * is a no-op for the oracle.  On device with real input, we must write to
     * the caller's DP (not the current dp=0x0037).
     *
     * Reconstruct caller's D from stack: after JSL pushed PBR+PCH+PCL (3 bytes),
     * and the caller did PHD before JSL (2 bytes), caller's D sits at SP+4:SP+5.
     */
    uint16_t sp = snes->cpu->sp;
    uint16_t d = (uint16_t)(snes->ram[(uint16_t)(sp + 4)]
                           | ((uint16_t)snes->ram[(uint16_t)(sp + 5)] << 8));

    uint16_t port1 = snes->input1 ? snes->portAutoRead[0] : 0;
    uint8_t  lo = (uint8_t)(port1 & 0xFF);
    uint8_t  hi = (uint8_t)((port1 >> 8) & 0xFF);
    snes->ram[(uint16_t)(d + 0x00)] = lo;
    snes->ram[(uint16_t)(d + 0x01)] = hi;
    snes->ram[(uint16_t)(d + 0x02)] = lo;
    snes->ram[(uint16_t)(d + 0x03)] = hi;

    /* f:\$000140 is an absolute long store — bank \$00 WRAM, DP-independent. */
    snes->ram[0x0140] = 0x00;
}

void update_ctrl_emu(Snes *snes) {
    /* Same job — UpdateCtrl_ext is the inner call. Idempotent if called
     * twice in the same frame. */
    update_ctrl_field_emu(snes);
}
__attribute__((weak)) void update_equip_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void update_scroll_regs_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void update_window_color_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void update_zoom_pal_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_frame_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_vblank_event_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_vblank_long_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void wait_vblank_short_emu(Snes *snes) { (void)snes; }
