#include "ff4_helpers.h"
#include <stdio.h>

uint16_t read16(const uint8_t *ram, int addr) {
    return (uint16_t)ram[addr] | ((uint16_t)ram[addr + 1] << 8);
}

void write16(uint8_t *ram, int addr, uint16_t v) {
    ram[addr]     = (uint8_t)(v & 0xff);
    ram[addr + 1] = (uint8_t)((v >> 8) & 0xff);
}

/* Run an emulated 65816 routine at the given 24-bit address. Some
 * battle/*.c bodies call this directly (instead of going through a
 * named *_emu helper) to delegate a sub-routine they couldn't
 * translate. Stubbed for now — same caveats as the *_emu stubs
 * below. */
__attribute__((weak)) void run_emulated_func(Snes *snes, uint32_t addr) {
    (void)snes; (void)addr;
}

/* No-op stubs for untranslated 65816 routines. The battle
 * subsystem still runs but loses any side effect these would
 * have produced — visible as wrong damage numbers, missing AI
 * cues, etc. Replace one by one when their .c body lands. */
__attribute__((weak)) void add_msg2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void add_msg3_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void apply_speed_mod_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void calc_dmg_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_battle_list_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_strong_elem_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_timer_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void check_weak_elem_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ClearBit_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void div16_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void do_fight_cmd_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void do_magic_attack_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void do_multi_attack_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ExecBtlGfx_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void ExecBtlGfx_ext_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void get_monster_with_status_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void get_timer_ptr_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void init_battle_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void magic_dmg_effect_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void mult16_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void mult8_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void no_self_target_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void rand_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void rand_summon_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void Rand99_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void RandAITarget_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void RandXA_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void remove_target_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void select_obj_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_magic_status_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_magic_status2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void set_timer_dur_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void SetMagicStatus2_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void SkipAITurn_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void SleepParalyzeEffect_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_character_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_monster_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void target_monster_type_emu(Snes *snes) { (void)snes; }
__attribute__((weak)) void update_equip_emu(Snes *snes) { (void)snes; }
