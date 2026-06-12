/* Helpers expected by the LLM-generated battle/*.c bodies.
 * read16/write16 were declared `static inline` in the LakeSnes
 * prompt but never emitted in the .c files; provide them here.
 * *_emu helpers represent calls to not-yet-translated 65816
 * routines — for now they are no-op stubs so the link succeeds.
 * A correct implementation would dispatch back to LakeSnes via
 * run_emulated_func(snes, asm_address) — Phase 5.7. */
#pragma once
#include <stdint.h>
#include "snes/snes.h"

uint16_t read16(const uint8_t *ram, int addr);
void     write16(uint8_t *ram, int addr, uint16_t v);

void add_msg2_emu(Snes *snes);
void add_msg3_emu(Snes *snes);
void apply_speed_mod_emu(Snes *snes);
void calc_dmg_emu(Snes *snes);
void check_battle_list_emu(Snes *snes);
void check_strong_elem_emu(Snes *snes);
void check_timer_emu(Snes *snes);
void check_weak_elem_emu(Snes *snes);
void ClearBit_emu(Snes *snes);
void div16_emu(Snes *snes);
void do_fight_cmd_emu(Snes *snes);
void do_magic_attack_emu(Snes *snes);
void do_multi_attack_emu(Snes *snes);
void ExecBtlGfx_emu(Snes *snes);
void ExecBtlGfx_ext_emu(Snes *snes);
void get_monster_with_status_emu(Snes *snes);
void get_timer_ptr_emu(Snes *snes);
void init_battle_emu(Snes *snes);
void magic_dmg_effect_emu(Snes *snes);
void mult16_emu(Snes *snes);
void mult8_emu(Snes *snes);
void no_self_target_emu(Snes *snes);
void rand_emu(Snes *snes);
void rand_summon_emu(Snes *snes);
void Rand99_emu(Snes *snes);
void RandAITarget_emu(Snes *snes);
void RandXA_emu(Snes *snes);
void remove_target_emu(Snes *snes);
void select_obj_emu(Snes *snes);
void set_magic_status_emu(Snes *snes);
void set_magic_status2_emu(Snes *snes);
void set_timer_dur_emu(Snes *snes);
void SetMagicStatus2_emu(Snes *snes);
void SkipAITurn_emu(Snes *snes);
void SleepParalyzeEffect_emu(Snes *snes);
void target_character_emu(Snes *snes);
void target_monster_emu(Snes *snes);
void target_monster_type_emu(Snes *snes);
void update_equip_emu(Snes *snes);
