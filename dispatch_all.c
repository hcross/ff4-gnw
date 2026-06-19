#include "dispatch_all.h"

/* Stub for ExecSound_ext ($04:8003): the title routine ShowTitle ($00:85FA)
 * calls `jsl ExecSound_ext` as its 8th instruction; the real sound engine
 * (jsr ExecSound) spins waiting for an SPC handshake that never completes on
 * G&W, so ShowTitle never returns and never loads the logo tilemap. No-op
 * here so the dispatch JSL hook simulates RTL and ShowTitle proceeds. This
 * silences all music/SFX (acceptable for now); the proper fix is a working
 * SPC responder. TODO: move into gen_dispatch.py instead of editing the
 * generated table by hand. */
static void ExecSound_ext_stub(Snes *snes) { (void)snes; }

const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT] = {
    { 0x00808e, AfterBattle_c },  /* field */
    { 0x0080a0, FieldMain_c },  /* field */
    { 0x0081f4, CheckMenu_c },  /* field */
    { 0x008302, UpdatePlayerSpeed_c },  /* field */
    { 0x00834e, InitMapRAM_c },  /* field */
    { 0x00883d, _00883d_c },  /* field */
    { 0x00885e, _00885e_c },  /* field */
    { 0x00aa58, CheckTilePass_c },  /* field */
    { 0x00aad8, SetPlayerNPCMap_c },  /* field */
    { 0x00ab13, ClearPlayerNPCMap_c },  /* field */
    { 0x00ac7d, CheckVehicleBlock_c },  /* field */
    { 0x00be47, CalcVehicleSpritePos_c },  /* field */
    { 0x00c0c4, PlayerSpriteTiles_c },  /* field */
    { 0x00c3bd, UpdateWhalePal_c },  /* field */
    { 0x00cb5f, TfrBGAnimGfx_c },  /* field */
    { 0x00ffbc, InitCharProp_ext_c },  /* field */
    { 0x00ffe0, Vectors_c },  /* field */
    { 0x018010, UpdateCtrlField_ext_c },  /* menu */
    { 0x01ca85, TfrVRAM_c },  /* field */
    { 0x01d718, FadeIn_c },  /* field */
    { 0x01dfd2, LoadBattleSpeedPosText_c },  /* menu */
    { 0x038009, ExecBattle_c },  /* battle */
    { 0x03805f, DrawMP_c },  /* battle */
    { 0x038085, ExecBtlGfx_c },  /* battle */
    { 0x0382cb, InitHWRegs_c },  /* field */
    { 0x0383b9, Mult16_c },  /* battle */
    { 0x038407, Div16_c },  /* battle */
    { 0x0384e3, Add16_c },  /* battle */
    { 0x0384fc, Sub16_c },  /* battle */
    { 0x038579, RandMonster_c },  /* battle */
    { 0x03858b, Rand99_c },  /* battle */
    { 0x03859b, AddMsg1_c },  /* battle */
    { 0x0385a6, AddMsg2_c },  /* battle */
    { 0x0385b1, AddMsg3_c },  /* battle */
    { 0x0387d8, CheckFanfare_c },  /* battle */
    { 0x0387e4, CheckBattleList_c },  /* battle */
    { 0x038803, CheckWinAnim_c },  /* battle */
    { 0x0395ce, InitCharRows_c },  /* battle */
    { 0x039741, GetPendingAction_c },  /* battle */
    { 0x039788, CheckTimer_c },  /* battle */
    { 0x0397b3, InitAction_c },  /* battle */
    { 0x039e65, TimerDur_00_c },  /* battle */
    { 0x039e71, TimerDur_02_c },  /* battle */
    { 0x039e85, TimerDur_0b_c },  /* battle */
    { 0x039e99, TimerDur_03_c },  /* battle */
    { 0x039f1c, TimerDur_08_c },  /* battle */
    { 0x039f75, TimerDur_0a_c },  /* battle */
    { 0x039fd8, ApplySpeedMod_c },  /* battle */
    { 0x03b0ff, ExecCmd_c },  /* battle */
    { 0x03b33f, Cmd_21_c },  /* battle */
    { 0x03ba04, AITarget_1e_c },  /* battle */
    { 0x03ba67, AITarget_1f_c },  /* battle */
    { 0x03ba74, AITarget_20_c },  /* battle */
    { 0x03ba81, AITarget_21_c },  /* battle */
    { 0x03ba8e, AITarget_22_c },  /* battle */
    { 0x03baef, AITarget_23_c },  /* battle */
    { 0x03bafe, AITarget_24_c },  /* battle */
    { 0x03bb0d, AITarget_25_c },  /* battle */
    { 0x03bb6b, AITarget_29_c },  /* battle */
    { 0x03bda9, AICond_02_c },  /* battle */
    { 0x03be1e, AICond_05_c },  /* battle */
    { 0x03be31, AICond_06_c },  /* battle */
    { 0x03bee4, AICond_09_c },  /* battle */
    { 0x03bf05, AICond_0b_c },  /* battle */
    { 0x03bff6, AICondTarget_25_c },  /* battle */
    { 0x03c03b, AICondTarget_26_c },  /* battle */
    { 0x03c042, AICondTarget_27_c },  /* battle */
    { 0x03c049, AnyTarget_c },  /* battle */
    { 0x03c06c, AICondTarget_23_c },  /* battle */
    { 0x03c987, CalcHits_c },  /* battle */
    { 0x03d378, MagicDmgEffect_c },  /* battle */
    { 0x03d466, MagicEffect_04_c },  /* battle */
    { 0x03d613, MagicEffect_08_c },  /* battle */
    { 0x03d83f, MagicEffect_0d_c },  /* battle */
    { 0x03d863, MagicEffect_0e_c },  /* battle */
    { 0x03d972, MagicEffect_13_c },  /* battle */
    { 0x03d9ec, MagicEffect_15_c },  /* battle */
    { 0x03da0c, MagicEffect_16_c },  /* battle */
    { 0x03dc9b, MagicEffect_20_c },  /* battle */
    { 0x03dcbb, MagicEffect_21_c },  /* battle */
    { 0x03dcd6, MagicEffect_22_c },  /* battle */
    { 0x03dd06, MagicEffect_24_c },  /* battle */
    { 0x03dd65, MagicEffect_2c_c },  /* battle */
    { 0x03dd95, MagicEffect_28_c },  /* battle */
    { 0x03ddb1, MagicEffect_2a_c },  /* battle */
    { 0x03ddc9, MagicEffect_2b_c },  /* battle */
    { 0x03dddd, MagicEffect_2f_c },  /* battle */
    { 0x03dfd2, MagicEffect_32_c },  /* battle */
    { 0x03e030, RemoveTarget_c },  /* battle */
    { 0x03e100, CheckStrongElem_c },  /* battle */
    { 0x03e133, CheckWeakElem_c },  /* battle */
    { 0x03e43b, Cmd_22_c },  /* battle */
    { 0x03e4d9, TwinFailed_c },  /* battle */
    { 0x03e699, Cmd_0f_c },  /* battle */
    { 0x03e6b7, Cmd_0e_c },  /* battle */
    { 0x03e839, Cmd_0c_c },  /* battle */
    { 0x03e903, Cmd_08_c },  /* battle */
    { 0x03eba2, Cmd_01_c },  /* battle */
    { 0x03fe03, TfrSprites_c },  /* field */
    { 0x048004, ExecSound_ext_stub },  /* sound: stub to unblock title (SPC wait); jsl target per ROM bytes at $00:860D */
    { 0x0485e1, PlayGameSfx_c },  /* sound */
    { 0x04861e, ExecInterrupt_c },  /* sound */
    { 0x088690, LoadTitleGfx_c },  /* field */
    { 0x08885e, TfrTitleCrystalTiles_c },  /* field */
    { 0x0e8b3c, CheckBattle_c },  /* field */
    { 0x12e35b, WaitVblankEvent_c },  /* field */
    { 0x12e55a, FindEventTerminator_c },  /* field */
    { 0x12e613, EventCmd_d8_c },  /* field */
    { 0x12e7d3, EventCmd_d7_c },  /* field */
    { 0x12eb47, EventCmd_e0_c },  /* field */
    { 0x12ec06, EventCmd_e6_c },  /* field */
    { 0x12ec3e, EventCmd_dd_c },  /* field */
    { 0x12ed1d, SetCurrGil_c },  /* field */
    { 0x12ee1c, EventCmd_d0_c },  /* field */
    { 0x12ee25, EventCmd_d1_c },  /* field */
    { 0x12ee35, TfrInvertPal_c },  /* field */
    { 0x13bfe3, _00bfe3_c },  /* field */
    { 0x13c11f, ReloadNPCs_c },  /* field */
    { 0x13d730, LoadTheEndGfx_c },  /* cutscene */
    { 0x13db10, _13db10_c },  /* cutscene */
    { 0x13db23, _13db23_c },  /* cutscene */
    { 0x13e07d, _13e07d_c },  /* cutscene */
    { 0x13e139, GetEarthSpritePos_c },  /* cutscene */
    { 0x13e247, InitStars_c },  /* cutscene */
    { 0x13e2dc, GetOtherPlanetTile_c },  /* cutscene */
    { 0x13e512, Mult16_1_c },  /* cutscene */
    { 0x13eb24, NewLine_c },  /* cutscene */
    { 0x13eb60, _13eb60_c },  /* cutscene */
    { 0x13ebb8, _13ebb8_c },  /* cutscene */
    { 0x13ef4c, _13ef4c_c },  /* cutscene */
    { 0x13fe36, AutoBattle_0003_c },  /* battle */
    { 0x14f58e, _14f58e_c },  /* field */
    { 0x14f626, GilWindowTiles3_c },  /* field */
    { 0x14f63e, GilWindowTiles4_c },  /* field */
    { 0x14f6d6, DlgTilesTop_c },  /* field */
    { 0x14f796, MapTitleTilesTop_c },  /* field */
    { 0x14f7b6, MapTitleTilesBtm_c },  /* field */
    { 0x14fa16, LavaAnimPal_c },  /* field */
    { 0x14fb1e, WipeScanlineTbl_c },  /* field */
    { 0x14fd00, InitCtrl_ext2_c },  /* menu */
    { 0x14fd03, UpdateCtrl_ext_c },  /* menu */
    { 0x14fd06, ClearText_ext_c },  /* menu */
    { 0x14fd09, UpdateWindowColor_ext_c },  /* menu */
    { 0x14fd0c, UpdateScrollRegs_ext_c },  /* menu */
    { 0x1585ab, InitWorld_c },  /* field */
    { 0x1589ed, InitInterrupts_c },  /* field */
    { 0x158b2a, InitDMA_c },  /* field */
    { 0x158d5d, PlayMapSong_c },  /* field */
    { 0x158dfc, WaitKeyUp_c },  /* field */
    { 0x158e05, WaitKeyDown_c },  /* field */
    { 0x158e47, UpdateWaterLavaAnim_c },  /* field */
    { 0x158e57, TfrWaterLavaGfx_c },  /* field */
    { 0x158f34, TfrLavaGfx_c },  /* field */
    { 0x159104, UpdateMode7Regs_c },  /* field */
    { 0x1591ca, UpdateWipeIRQ_c },  /* field */
    { 0x159204, UpdateWipeNMI_c },  /* field */
    { 0x159792, LoadPlayerGfxWorld_c },  /* field */
    { 0x1597a2, LoadPlayerGfxSub_c },  /* field */
    { 0x1599fb, GiveGil_c },  /* field */
    { 0x159ae9, GetTreasureTiles_c },  /* field */
    { 0x159b5b, GetTreasurePtr_c },  /* field */
    { 0x15af24, CloseYesNoWindow_c },  /* field */
    { 0x15b09c, ScrollItemListDown_c },  /* field */
    { 0x15b143, TfrBGGfx_c },  /* field */
    { 0x15b3dc, _15b3dc_c },  /* field */
    { 0x15b41b, GetDlgPtr1H_c },  /* field */
    { 0x15b6f1, InitDlgIRQ_c },  /* field */
    { 0x15b8c9, _15b8c9_c },  /* field */
    { 0x15bb6a, _15bb6a_c },  /* field */
    { 0x15c144, _15c144_c },  /* field */
    { 0x15c163, _15c163_c },  /* field */
    { 0x15c23d, _15c23d_c },  /* field */
    { 0x15c37f, Pow10Hi_c },  /* field */
    { 0x15ca5e, _15ca5e_c },  /* field */
    { 0x15ca85, _15ca85_c },  /* field */
    { 0x15cadc, _15cadc_c },  /* field */
    { 0x16c59a, AfterCutscene_c },  /* field */
    { 0x16c8bc, Special_2d_c },  /* field */
    { 0x16cb05, _00cb05_c },  /* field */
    { 0x16cb72, _00cb72_c },  /* field */
    { 0x16cfc4, Special_1d_c },  /* field */
    { 0x16cfd0, Special_1c_c },  /* field */
    { 0x16d263, LoadOverworldLeviathan_c },  /* field */
    { 0x16d342, Special_0d_c },  /* field */
    { 0x16d36d, LoadMapStack_c },  /* field */
    { 0x16d758, DrawDestroyedDamcyan_c },  /* field */
    { 0x16d831, Special_1e_c },  /* field */
    { 0x16d9d6, Special_06_c },  /* field */
    { 0x16db71, DrawRedWings_c },  /* field */
    { 0x16dbbe, IncBrightness_c },  /* field */
    { 0x16dbd2, LoadOverworldIntro_c },  /* field */
    { 0x16de1b, _00de1b_c },  /* field */
    { 0x16df53, _00df53_c },  /* field */
    { 0x16f533, UpdateBG2Scroll_c },  /* field */
    { 0x16f922, _00f922_c },  /* field */
    { 0x16fb93, TfrBG2Tilemap_c },  /* field */
    { 0x16ffab, DecodeBG1Tilemap_c },  /* field */
    { 0x1e9f6c, UpdateLocalTiles_c },  /* field */
    { 0x1ea03e, BoardChoco_c },  /* field */
};

uint32_t ff4_dispatch_hits = 0;
uint32_t ff4_dispatch_misses = 0;
uint32_t ff4_miss_per_bank[256] = {0};

/* Runtime A/B toggle (desktop validation host, ADR 0001). Default 1 = native
 * dispatch active, exactly as the device runs. The desktop oracle sets it to 0
 * to fall back to pure interpretation (ground truth) on the same binary/state.
 * The device never clears it, so on-device behaviour is unchanged. */
int ff4_dispatch_enabled = 1;

/* Optional per-hit trace hook (desktop A/B oracle, ADR 0001 / M3). NULL on the
 * device, so the only on-device cost is one predictably-not-taken NULL check.
 * The oracle installs a callback to record which original PC was dispatched on
 * each hit, so the first diverging frame can be attributed to a concrete hook. */
void (*ff4_dispatch_trace)(uint32_t pc) = 0;

#ifdef FF4_AUTOBOOT
uint32_t g_diag_miss_ring[8] = {0};
static uint8_t g_diag_miss_ring_head = 0;
#endif

int ff4_dispatch_try(Snes *snes, uint32_t pc) {
    if (!ff4_dispatch_enabled) return 0;  /* pure-interpreter side of the A/B */
    /* Linear scan (binary search was unreliable: gen_dispatch.py sorts by the
     * original pc, then rewrites banks, leaving the table unsorted). */
    for (int i = 0; i < FF4_DISPATCH_COUNT; i++) {
        if (ff4_dispatch_table[i].pc == pc) {
            ff4_dispatch_hits++;
            if (ff4_dispatch_trace) ff4_dispatch_trace(pc);
            ff4_dispatch_table[i].fn(snes);
            return 1;
        }
    }
    ff4_dispatch_misses++;
    ff4_miss_per_bank[(pc >> 16) & 0xff]++;
#ifdef FF4_AUTOBOOT
    {   /* record unique miss PCs in a small ring (dedup against existing) */
        int already = 0;
        for (int i = 0; i < 8; i++) {
            if (g_diag_miss_ring[i] == pc) { already = 1; break; }
        }
        if (!already) {
            g_diag_miss_ring[g_diag_miss_ring_head] = pc;
            g_diag_miss_ring_head = (g_diag_miss_ring_head + 1) & 7;
        }
    }
#endif
    return 0;
}


/* ---------------------------------------------------------
 * Auto-stubs (generated by gen_dispatch.py)
 *
 * Weak no-op definitions for *_emu helpers referenced by
 * the dispatched .c files above but not yet declared in
 * ff4_helpers.c. A stronger definition elsewhere (e.g. a
 * real RunEmulatedFunc-backed delegate) will override the
 * weak symbol automatically.
 * --------------------------------------------------------- */

__attribute__((weak)) void fade_out_menu_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void main_menu_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void fade_in_menu_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void exec_event_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void fadein_emu(Snes *snes) { (void)snes; }  /* first needed by field/FadeIn.c */
__attribute__((weak)) void remove_float_emu(Snes *snes) { (void)snes; }  /* first needed by field/InitWorld.c */
__attribute__((weak)) void tfr_world_gfx_emu(Snes *snes) { (void)snes; }  /* first needed by field/InitWorld.c */
__attribute__((weak)) void invert_pal_emu(Snes *snes) { (void)snes; }  /* first needed by field/InitWorld.c */
__attribute__((weak)) void update_lava_anim_emu(Snes *snes) { (void)snes; }  /* first needed by field/UpdateWaterLavaAnim.c */
__attribute__((weak)) void update_water_anim_emu(Snes *snes) { (void)snes; }  /* first needed by field/UpdateWaterLavaAnim.c */
__attribute__((weak)) void load_whirlpool_pal_emu(Snes *snes) { (void)snes; }  /* first needed by field/LoadOverworldLeviathan.c */
__attribute__((weak)) void draw_whirlpool_emu(Snes *snes) { (void)snes; }  /* first needed by field/LoadOverworldLeviathan.c */
__attribute__((weak)) void ExecSound_ext_emu(Snes *snes) { (void)snes; }  /* first needed by field/LoadMapStack.c */
__attribute__((weak)) void decode_sub_tilemap_emu(Snes *snes) { (void)snes; }  /* first needed by field/DecodeBG1Tilemap.c */
