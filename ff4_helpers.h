#pragma once
#include <stdint.h>
#include "snes/snes.h"

uint16_t read16(const uint8_t *ram, int addr);
void     write16(uint8_t *ram, int addr, uint16_t v);
void     run_emulated_func(struct Snes *snes, uint32_t addr);

void ClearBit_emu(Snes *snes);
void ExecBtlGfx_emu(Snes *snes);
void ExecBtlGfx_ext_emu(Snes *snes);
void Rand99_emu(Snes *snes);
void RandAITarget_emu(Snes *snes);
void RandXA_emu(Snes *snes);
void SetMagicStatus2_emu(Snes *snes);
void SkipAITurn_emu(Snes *snes);
void SleepParalyzeEffect_emu(Snes *snes);
void _13e058_emu(Snes *snes);
void _13e58b_emu(Snes *snes);
void _13eb60_emu(Snes *snes);
void add_msg2_emu(Snes *snes);
void add_msg3_emu(Snes *snes);
void apply_speed_mod_emu(Snes *snes);
void calc_dmg_emu(Snes *snes);
void check_battle_list_emu(Snes *snes);
void check_strong_elem_emu(Snes *snes);
void check_timer_emu(Snes *snes);
void check_weak_elem_emu(Snes *snes);
void div16_emu(Snes *snes);
void do_fight_cmd_emu(Snes *snes);
void do_magic_attack_emu(Snes *snes);
void do_multi_attack_emu(Snes *snes);
void draw_solar_system_sprite_emu(Snes *snes);
void exec_sound_emu(Snes *snes);
void get_monster_with_status_emu(Snes *snes);
void get_timer_ptr_emu(Snes *snes);
void init_battle_emu(Snes *snes);
void init_sound_emu(Snes *snes);
void magic_dmg_effect_emu(Snes *snes);
void mult16_emu(Snes *snes);
void mult8_emu(Snes *snes);
void no_self_target_emu(Snes *snes);
void rand_emu(Snes *snes);
void rand_summon_emu(Snes *snes);
void remove_target_emu(Snes *snes);
void select_obj_emu(Snes *snes);
void set_magic_status2_emu(Snes *snes);
void set_magic_status_emu(Snes *snes);
void set_timer_dur_emu(Snes *snes);
void target_character_emu(Snes *snes);
void target_monster_emu(Snes *snes);
void target_monster_type_emu(Snes *snes);
void update_equip_emu(Snes *snes);
