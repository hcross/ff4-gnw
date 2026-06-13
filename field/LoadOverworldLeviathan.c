#include "snes/snes.h"

// LoadOverworldLeviathan — sets up overworld Leviathan scene:
//   writes initial VRAM/PPU parameters, then chains through
//   InitHWRegs, LoadOverworld, LoadWhirlpoolPal, ResetSprites,
//   and DrawWhirlpool.
void LoadOverworldLeviathan_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    uint8_t *ram = snes->ram;

    // Inherited mode: A=8-bit, X/Y=16-bit (field module convention)
    cpu->mf = true;
    cpu->xf = false;
    cpu->dp = 0;
    cpu->db = 0x7E;               // Pitfall 1: DB must be $7E for WRAM

    // ldx #$90a8 / stx $1706
    cpu->x = 0x90A8;
    write16(ram, 0x1706, 0x90A8);

    // lda #$10 / sta $2c
    cpu->a = 0x10;
    ram[0x2C] = 0x10;

    // lda #$58 / sta $2e
    cpu->a = 0x58;
    ram[0x2E] = 0x58;

    // jsl InitHWRegs — A=0x58, X=0x90A8
    // (bridge could not resolve init_hw_regs_emu; use run_emulated_func)
    cpu->z = false;               // Pitfall 2: flags reflect A before call
    cpu->n = false;
    run_emulated_func(snes, 0x0082CB);

    // stz $1700
    ram[0x1700] = 0;

    // lda #$07 / sta $1704
    cpu->a = 0x07;
    ram[0x1704] = 0x07;

    // lda #$01 / sta $1728
    cpu->a = 0x01;
    ram[0x1728] = 0x01;

    // jsr LoadOverworld — A=0x01, X=0x90A8
    cpu->z = false;
    cpu->n = false;
    load_overworld_emu(snes);

    // lda #$10 / sta $ad
    cpu->a = 0x10;
    ram[0xAD] = 0x10;

    // jsr LoadWhirlpoolPal — A=0x10, X=0x90A8
    cpu->z = false;
    cpu->n = false;
    load_whirlpool_pal_emu(snes);

    // jsr ResetSprites — A/X as left by LoadWhirlpoolPal
    reset_sprites_emu(snes);

    // jsr DrawWhirlpool — A/X as left by ResetSprites
    draw_whirlpool_emu(snes);

    // rts
}

// PITFALLS: 1 (DB=$7E for WRAM), 2 (Z/N set before emulated calls),
//           6 (mode A 8-bit assumed)
// HELPERS: run_emulated_func (for InitHWRegs, bridge missing stub),
//          load_overworld_emu, load_whirlpool_pal_emu,
//          reset_sprites_emu, draw_whirlpool_emu
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none (multiple side effects; CUSTOM_SPIKE: yes)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::LoadOverworldLeviathan ($00:D263)