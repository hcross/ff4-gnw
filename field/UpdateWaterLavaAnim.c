#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=$8E, DP=0
// This routine determines which environmental animation update to run based on 
// the current map type stored in ram[0x1700].
// 0x00 -> UpdateWaterAnim
// 0x01 -> UpdateLavaAnim
// Other -> No operation
void UpdateWaterLavaAnim_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    uint8_t map_type = ram[0x1700]; // lda $1700

    if (map_type != 0) {             // bne @8e4f
        if (map_type == 0x01) {      // cmp #$01 / bne @8e56
            update_lava_anim_emu(snes); // jmp UpdateLavaAnim
        }
        return;                     // @8e56: rts
    }
    update_water_anim_emu(snes);    // jmp UpdateWaterAnim
}

// PITFALLS: None. (Straightforward conditional logic, no flag residue dependencies
// on entry since the first instruction is an LDA).
// HELPERS: update_water_anim_emu(snes) — delegates UpdateWaterAnim @ $8E:8FA1
//          update_lava_anim_emu(snes) — delegates UpdateLavaAnim @ $8E:8EE3
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram: 0x1700=1
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x8E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::UpdateWaterLavaAnim ($8E:0047)