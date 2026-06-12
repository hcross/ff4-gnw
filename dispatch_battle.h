/* Auto-generated dispatch table — FF4 battle PASS routines.
 * Source: hcross/ff4-port qwen3_validation_after_retry.jsonl */
#pragma once
#include <stdint.h>
#include "snes/snes.h"

void ExecBattle_c(Snes *snes);
void DrawMP_c(Snes *snes);
void ExecBtlGfx_c(Snes *snes);
void Mult16_c(Snes *snes);
void Div16_c(Snes *snes);
void Add16_c(Snes *snes);
void Sub16_c(Snes *snes);
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
void ExecCmd_c(Snes *snes);
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
void Cmd_0f_c(Snes *snes);
void Cmd_0e_c(Snes *snes);
void Cmd_0c_c(Snes *snes);
void Cmd_08_c(Snes *snes);
void Cmd_01_c(Snes *snes);
void AutoBattle_0003_c(Snes *snes);

typedef struct { uint32_t pc; void (*fn)(Snes *snes); } ff4_dispatch_entry_t;
#define FF4_DISPATCH_COUNT 77
extern const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT];

/* Returns true and runs the C body if pc hits; false otherwise. */
int ff4_dispatch_try(Snes *snes, uint32_t pc);
