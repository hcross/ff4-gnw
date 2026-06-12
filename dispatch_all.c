#include "dispatch_all.h"

const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT] = {
    { 0x00808e, AfterBattle_c },  /* field */
    { 0x0080a0, FieldMain_c },  /* field */
    { 0x00834e, InitMapRAM_c },  /* field */
    { 0x008690, LoadTitleGfx_c },  /* field */
    { 0x008dfc, WaitKeyUp_c },  /* field */
    { 0x008e05, WaitKeyDown_c },  /* field */
    { 0x008e57, TfrWaterLavaGfx_c },  /* field */
    { 0x008f34, TfrLavaGfx_c },  /* field */
    { 0x0099fb, GiveGil_c },  /* field */
    { 0x009ae9, GetTreasureTiles_c },  /* field */
    { 0x009b5b, GetTreasurePtr_c },  /* field */
    { 0x00a03e, BoardChoco_c },  /* field */
    { 0x00aa58, CheckTilePass_c },  /* field */
    { 0x00ab13, ClearPlayerNPCMap_c },  /* field */
    { 0x00af24, CloseYesNoWindow_c },  /* field */
    { 0x00b09c, ScrollItemListDown_c },  /* field */
    { 0x00b143, TfrBGGfx_c },  /* field */
    { 0x00b6f1, InitDlgIRQ_c },  /* field */
    { 0x00b8c9, _15b8c9_c },  /* field */
    { 0x00bb6a, _15bb6a_c },  /* field */
    { 0x00bfe3, _00bfe3_c },  /* field */
    { 0x00c11f, ReloadNPCs_c },  /* field */
    { 0x00c144, _15c144_c },  /* field */
    { 0x00c23d, _15c23d_c },  /* field */
    { 0x00c59a, AfterCutscene_c },  /* field */
    { 0x00c8bc, Special_2d_c },  /* field */
    { 0x00cb05, _00cb05_c },  /* field */
    { 0x00cb5f, TfrBGAnimGfx_c },  /* field */
    { 0x00cb72, _00cb72_c },  /* field */
    { 0x00cfc4, Special_1d_c },  /* field */
    { 0x00cfd0, Special_1c_c },  /* field */
    { 0x00d342, Special_0d_c },  /* field */
    { 0x00d831, Special_1e_c },  /* field */
    { 0x00dbbe, IncBrightness_c },  /* field */
    { 0x00dbd2, LoadOverworldIntro_c },  /* field */
    { 0x00de1b, _00de1b_c },  /* field */
    { 0x00df53, _00df53_c },  /* field */
    { 0x00e35b, WaitVblankEvent_c },  /* field */
    { 0x00e613, EventCmd_d8_c },  /* field */
    { 0x00e7d3, EventCmd_d7_c },  /* field */
    { 0x00eb47, EventCmd_e0_c },  /* field */
    { 0x00ec06, EventCmd_e6_c },  /* field */
    { 0x00ec3e, EventCmd_dd_c },  /* field */
    { 0x00ed1d, SetCurrGil_c },  /* field */
    { 0x00ee1c, EventCmd_d0_c },  /* field */
    { 0x00ee25, EventCmd_d1_c },  /* field */
    { 0x00ee35, TfrInvertPal_c },  /* field */
    { 0x00f58e, _14f58e_c },  /* field */
    { 0x00f626, GilWindowTiles3_c },  /* field */
    { 0x00f63e, GilWindowTiles4_c },  /* field */
    { 0x00f6d6, DlgTilesTop_c },  /* field */
    { 0x00f796, MapTitleTilesTop_c },  /* field */
    { 0x00f922, _00f922_c },  /* field */
    { 0x00fa16, LavaAnimPal_c },  /* field */
    { 0x00fb1e, WipeScanlineTbl_c },  /* field */
    { 0x00fb93, TfrBG2Tilemap_c },  /* field */
    { 0x00ffe0, Vectors_c },  /* field */
    { 0x038009, ExecBattle_c },  /* battle */
    { 0x03805f, DrawMP_c },  /* battle */
    { 0x038085, ExecBtlGfx_c },  /* battle */
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
    { 0x03fe36, AutoBattle_0003_c },  /* battle */
    { 0x048000, InitSound_ext_c },  /* sound */
    { 0x048003, ExecSound_ext_c },  /* sound */
    { 0x0485e1, PlayGameSfx_c },  /* sound */
    { 0x04861e, ExecInterrupt_c },  /* sound */
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
};

uint32_t ff4_dispatch_hits = 0;
uint32_t ff4_dispatch_misses = 0;
uint32_t ff4_miss_per_bank[256] = {0};

int ff4_dispatch_try(Snes *snes, uint32_t pc) {
    int lo = 0, hi = FF4_DISPATCH_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        uint32_t e = ff4_dispatch_table[mid].pc;
        if (e == pc) {
            ff4_dispatch_hits++;
            ff4_dispatch_table[mid].fn(snes);
            return 1;
        }
        if (e < pc) lo = mid + 1; else hi = mid - 1;
    }
    ff4_dispatch_misses++;
    ff4_miss_per_bank[(pc >> 16) & 0xff]++;
    return 0;
}
