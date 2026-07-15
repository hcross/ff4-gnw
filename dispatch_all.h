/* HAND-MAINTAINED dispatch table for FF4 native-C bodies. Do NOT run
 * gen_dispatch.py expecting it to regenerate this file — it no longer
 * writes here (2026-07-03: doing so used to silently destroy the oracle
 * machinery in dispatch_all.c and drop the linear-scan table down to a
 * broken binary-search one). gen_dispatch.py is now read-only: it reports
 * candidate routines not yet in the table (`python gen_dispatch.py`). To
 * add an entry, hand-edit this file and dispatch_all.c per
 * workflows/WF-DECOMP.md. */
#pragma once
#include <stdint.h>
#include "snes/snes.h"

void AfterBattle_c(Snes *snes);
void FieldMain_c(Snes *snes);
void CalcObjScreenPos_c(Snes *snes);
void DrawNpcs_c(Snes *snes);
void GetTileProps_c(Snes *snes);
void ResetAllSprites_c(Snes *snes);
void SetNpcMapPtr_c(Snes *snes);
void ClearNpcMapCell_c(Snes *snes);
void SetNpcMapCell_c(Snes *snes);
void CheckMenu_c(Snes *snes);
void UpdatePlayerSpeed_c(Snes *snes);
void InitMapRAM_c(Snes *snes);
void _00883d_c(Snes *snes);
void _00885e_c(Snes *snes);
void CheckTilePass_c(Snes *snes);
void SetPlayerNPCMap_c(Snes *snes);
void ClearPlayerNPCMap_c(Snes *snes);
void CheckVehicleBlock_c(Snes *snes);
void CalcVehicleSpritePos_c(Snes *snes);
void PlayerSpriteTiles_c(Snes *snes);
void UpdateWhalePal_c(Snes *snes);
void TfrBGAnimGfx_c(Snes *snes);
void InitCharProp_ext_c(Snes *snes);
void Vectors_c(Snes *snes);
void UpdateCtrlField_ext_c(Snes *snes);
void TfrVRAM_c(Snes *snes);
void FadeIn_c(Snes *snes);
void LoadBattleSpeedPosText_c(Snes *snes);
void ExecBattle_c(Snes *snes);
void DrawMP_c(Snes *snes);
void ExecBtlGfx_c(Snes *snes);
void InitHWRegs_c(Snes *snes);
void RandXA_c(Snes *snes);
void Mult16_c(Snes *snes);
void Mult8_c(Snes *snes);
void Div16_c(Snes *snes);
void Add16_c(Snes *snes);
void Sub16_c(Snes *snes);
void CalcDmg_c(Snes *snes);
void RandMonster_c(Snes *snes);
void Rand99_c(Snes *snes);
void AddMsg1_c(Snes *snes);
void AddMsg2_c(Snes *snes);
void AddMsg3_c(Snes *snes);
void CheckFanfare_c(Snes *snes);
void CheckBattleList_c(Snes *snes);
void CheckWinAnim_c(Snes *snes);
void InitCharRows_c(Snes *snes);
void GetPendingAction_c(Snes *snes);
void CheckTimer_c(Snes *snes);
void InitAction_c(Snes *snes);
void TimerDur_00_c(Snes *snes);
void TimerDur_02_c(Snes *snes);
void TimerDur_0b_c(Snes *snes);
void TimerDur_03_c(Snes *snes);
void TimerDur_08_c(Snes *snes);
void TimerDur_0a_c(Snes *snes);
void ApplySpeedMod_c(Snes *snes);
/* TimerDur_00/02/0b/03/08/0a removed — see dispatch_all.c */
/* ExecCmd_c removed from dispatch — tail-jump routine, see dispatch_all.c comment at 0x03b0ff */
void Cmd_21_c(Snes *snes);
void AITarget_1e_c(Snes *snes);
void AITarget_1f_c(Snes *snes);
void AITarget_20_c(Snes *snes);
void AITarget_21_c(Snes *snes);
void AITarget_22_c(Snes *snes);
void AITarget_23_c(Snes *snes);
void AITarget_24_c(Snes *snes);
void AITarget_25_c(Snes *snes);
void AITarget_29_c(Snes *snes);
void AICond_02_c(Snes *snes);
void AICond_05_c(Snes *snes);
void AICond_06_c(Snes *snes);
void AICond_09_c(Snes *snes);
void AICond_0b_c(Snes *snes);
void AICondTarget_25_c(Snes *snes);
void AICondTarget_26_c(Snes *snes);
void AICondTarget_27_c(Snes *snes);
void AnyTarget_c(Snes *snes);
void AICondTarget_23_c(Snes *snes);
void CalcHits_c(Snes *snes);
void MagicDmgEffect_c(Snes *snes);
void MagicEffect_04_c(Snes *snes);
void MagicEffect_08_c(Snes *snes);
void MagicEffect_0d_c(Snes *snes);
void MagicEffect_0e_c(Snes *snes);
void MagicEffect_13_c(Snes *snes);
void MagicEffect_15_c(Snes *snes);
void MagicEffect_16_c(Snes *snes);
void MagicEffect_20_c(Snes *snes);
void MagicEffect_21_c(Snes *snes);
void MagicEffect_22_c(Snes *snes);
void MagicEffect_24_c(Snes *snes);
void MagicEffect_2c_c(Snes *snes);
void MagicEffect_28_c(Snes *snes);
void MagicEffect_2a_c(Snes *snes);
void MagicEffect_2b_c(Snes *snes);
void MagicEffect_2f_c(Snes *snes);
void MagicEffect_32_c(Snes *snes);
void RemoveTarget_c(Snes *snes);
void CheckStrongElem_c(Snes *snes);
void CheckWeakElem_c(Snes *snes);
void Cmd_22_c(Snes *snes);
void TwinFailed_c(Snes *snes);
/* Cmd_0f/0e/0c/08/01 removed — see dispatch_all.c */
void TfrSprites_c(Snes *snes);
void PlayGameSfx_c(Snes *snes);
void ExecInterrupt_c(Snes *snes);
void LoadTitleGfx_c(Snes *snes);
void TfrTitleCrystalTiles_c(Snes *snes);
void CheckBattle_c(Snes *snes);
void WaitVblankEvent_c(Snes *snes);
void FindEventTerminator_c(Snes *snes);
void EventCmd_d8_c(Snes *snes);
void EventCmd_d7_c(Snes *snes);
void EventCmd_e0_c(Snes *snes);
void EventCmd_e6_c(Snes *snes);
void EventCmd_dd_c(Snes *snes);
void SetCurrGil_c(Snes *snes);
void EventCmd_d0_c(Snes *snes);
void EventCmd_d1_c(Snes *snes);
void TfrInvertPal_c(Snes *snes);
void _00bfe3_c(Snes *snes);
void ReloadNPCs_c(Snes *snes);
void LoadTheEndGfx_c(Snes *snes);
void _13db10_c(Snes *snes);
void _13db23_c(Snes *snes);
void _13e07d_c(Snes *snes);
void GetEarthSpritePos_c(Snes *snes);
void InitStars_c(Snes *snes);
void GetOtherPlanetTile_c(Snes *snes);
void Mult16_1_c(Snes *snes);
void NewLine_c(Snes *snes);
void _13eb60_c(Snes *snes);
void _13ebb8_c(Snes *snes);
void _13ef4c_c(Snes *snes);
void AutoBattle_0003_c(Snes *snes);
void _14f58e_c(Snes *snes);
void GilWindowTiles3_c(Snes *snes);
void GilWindowTiles4_c(Snes *snes);
void DlgTilesTop_c(Snes *snes);
void MapTitleTilesTop_c(Snes *snes);
void MapTitleTilesBtm_c(Snes *snes);
void LavaAnimPal_c(Snes *snes);
void WipeScanlineTbl_c(Snes *snes);
void InitCtrl_ext2_c(Snes *snes);
void UpdateCtrl_ext_c(Snes *snes);
void ClearText_ext_c(Snes *snes);
void UpdateWindowColor_ext_c(Snes *snes);
void UpdateScrollRegs_ext_c(Snes *snes);
void InitWorld_c(Snes *snes);
void InitInterrupts_c(Snes *snes);
void InitDMA_c(Snes *snes);
void PlayMapSong_c(Snes *snes);
void WaitKeyUp_c(Snes *snes);
void WaitKeyDown_c(Snes *snes);
void UpdateWaterLavaAnim_c(Snes *snes);
void TfrWaterLavaGfx_c(Snes *snes);
void TfrLavaGfx_c(Snes *snes);
void UpdateMode7Regs_c(Snes *snes);
void UpdateWipeIRQ_c(Snes *snes);
void UpdateWipeNMI_c(Snes *snes);
void LoadPlayerGfxWorld_c(Snes *snes);
void LoadPlayerGfxSub_c(Snes *snes);
void GiveGil_c(Snes *snes);
void GetTreasureTiles_c(Snes *snes);
void GetTreasurePtr_c(Snes *snes);
void CloseYesNoWindow_c(Snes *snes);
void ScrollItemListDown_c(Snes *snes);
void TfrBGGfx_c(Snes *snes);
void _15b3dc_c(Snes *snes);
void GetDlgPtr1H_c(Snes *snes);
void InitDlgIRQ_c(Snes *snes);
void _15b8c9_c(Snes *snes);
void _15bb6a_c(Snes *snes);
void _15c144_c(Snes *snes);
void _15c163_c(Snes *snes);
void _15c23d_c(Snes *snes);
void Pow10Hi_c(Snes *snes);
void _15ca5e_c(Snes *snes);
void _15ca85_c(Snes *snes);
void _15cadc_c(Snes *snes);
void AfterCutscene_c(Snes *snes);
void Special_2d_c(Snes *snes);
void _00cb05_c(Snes *snes);
void _00cb72_c(Snes *snes);
void Special_1d_c(Snes *snes);
void Special_1c_c(Snes *snes);
void LoadOverworldLeviathan_c(Snes *snes);
void Special_0d_c(Snes *snes);
void LoadMapStack_c(Snes *snes);
void DrawDestroyedDamcyan_c(Snes *snes);
void Special_1e_c(Snes *snes);
void Special_06_c(Snes *snes);
void DrawRedWings_c(Snes *snes);
void IncBrightness_c(Snes *snes);
void LoadOverworldIntro_c(Snes *snes);
void _00de1b_c(Snes *snes);
void _00df53_c(Snes *snes);
void UpdateBG2Scroll_c(Snes *snes);
void UpdateBG2ScrollSkip_c(Snes *snes);
void BackAttackYOffset_s_c(Snes *snes);
void BackAttackYOffset_l_c(Snes *snes);
void Mult8_btlgfx_c(Snes *snes);
void HardMult_btlgfx_c(Snes *snes);
void TfrBG2MenuTile_c(Snes *snes);
void IncrTextPtr_c(Snes *snes);
void CheckSpriteVisible_c(Snes *snes);
void UpdateMonsterAnim_c(Snes *snes);
void DrawMonsterSprite_c(Snes *snes);
void InitMonsterAnim_c(Snes *snes);
void BuildOAMEntries_c(Snes *snes);
void _00f922_c(Snes *snes);
void TfrBG2Tilemap_c(Snes *snes);
void DecodeBG1Tilemap_c(Snes *snes);
void UpdateLocalTiles_c(Snes *snes);
void BoardChoco_c(Snes *snes);

typedef struct { uint32_t pc; void (*fn)(Snes *snes); } ff4_dispatch_entry_t;
#define FF4_DISPATCH_COUNT 208
extern const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT];
int ff4_dispatch_try(Snes *snes, uint32_t pc);
extern uint32_t ff4_dispatch_hits;
extern uint32_t ff4_dispatch_misses;
extern int ff4_dispatch_enabled;  /* 1=native dispatch (device default), 0=pure interpreter */
extern void (*ff4_dispatch_trace)(uint32_t pc);      /* NULL on device; A/B oracle per-hit trace hook */
extern int  (*ff4_dispatch_filter)(uint32_t pc);     /* NULL on device; return 0 to force interpretation of a hook */
extern void (*ff4_dispatch_miss_trace)(uint32_t pc); /* NULL on device; miss profiler hook */

/* Per-slot correctness gate (ROM-variant dispatch profiles, translation-patch
 * ADR; armed by rom_ident.c at ff4_init). A set slot means the loaded ROM
 * variant rewrote this routine's original asm: the native body (proven
 * against vanilla only) must not run, the call falls through to the
 * interpreter. Distinct from ff4_dispatch_filter, which stays owned by the
 * desktop harness (oracle --exclude/--only) and composes with the gate. */
extern uint8_t ff4_dispatch_gate[FF4_DISPATCH_COUNT];
extern uint32_t ff4_dispatch_gated;   /* gated fall-throughs (NOT misses)    */
void ff4_dispatch_gate_clear(void);
int  ff4_dispatch_gate_pc(uint32_t pc); /* 1 = found & gated, 0 = unknown pc */
