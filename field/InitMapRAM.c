#include "snes/snes.h"

// Entry mode: A 8-bit (mf=1), X 16-bit (xf=0), DB=0, DP=0 per the original
// CONTRACT -- WRONG, same bug class as CheckMenu_c/_00883d_c/_00885e_c
// (see ff4-gnw/CONVENTIONS.md's field-module DP note). FIX (2026-07-06):
// all the STZ/LDA/STA $nn opcodes below are ZERO-PAGE (direct-page)
// addressing, i.e. D+$nn, not literal absolute $00nn -- confirmed by a
// byte-level WRAM diff between the dispatched (buggy) and interpreted
// (correct) passes at the exact frame InitMapRAM first runs during a
// map-transition repro: native wrote $0050-$0057/$0066-$0069/etc (D=0
// assumed) while the real asm wrote $0650-$0657/$0666-$0669/etc (D=$0600,
// the field module's actual running direct page at this call site) --
// found while investigating "exit a building, land in the sea, stuck"
// (see MemPalace wing=ff4-gnw room=obstacles-and-solutions, "[BUG OUVERT]
// Sortie de bâtiment"). Read the caller's real D via cpu->dp, exactly like
// CheckMenu_c's ZP() macro, instead of assuming 0.
void InitMapRAM_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    Cpu *cpu = snes->cpu;
    uint16_t dp = cpu->dp;
#define ZP(off) ram[(uint16_t)(dp + (off))]

    // lda #$80 / sta hINIDISP ; stz hHDMAEN ; stz hNMITIMEN — these are MMIO
    // registers, NOT WRAM. The original port hallucinated WRAM offsets
    // $00/$01/$02 (comments literally said "Placeholder"), which left HDMA/NMI
    // unconfigured (→ corrupted Mode-7 graphics + broken scroll) and clobbered
    // zero-page scratch. Route them through the bus, like InitHWRegs_c does.
    snes_write(snes, 0x2100, 0x80); // hINIDISP  — force blank
    snes_write(snes, 0x420C, 0x00); // hHDMAEN   — disable HDMA channels
    snes_write(snes, 0x4200, 0x00); // hNMITIMEN — disable NMI/IRQ/auto-joypad

    cpu->i = true;    // sei (disable interrupts)

    reset_sprites_emu(snes); // jsr ResetSprites

    // Clear animation and map state variables (zero-page, D-relative)
    ZP(0x7A) = 0;    // animation frame counter
    ZP(0x94) = 0;
    ZP(0xEB) = 0;
    ZP(0xE9) = 0;
    ZP(0xEB) = 0;    // Duplicate in asm
    ZP(0xEC) = 0;
    ZP(0xED) = 0;
    ZP(0xEA) = 0;    // show map name
    ZP(0xE7) = 0;
    ZP(0xE8) = 0;
    ZP(0xD4) = 0;
    ZP(0xAB) = 0;
    ZP(0xCF) = 0;
    ZP(0xDA) = 0;
    ZP(0xC9) = 0;
    ZP(0xC4) = 0;
    ZP(0xC1) = 0;

    // lda #1 / sta sequence
    ZP(0x54) = 1;
    ZP(0x55) = 1;
    ZP(0x50) = 1;
    ZP(0x51) = 1;
    ZP(0x52) = 1;
    ZP(0x53) = 1;
    ZP(0x56) = 1;
    ZP(0x57) = 1;

    // Clear Mode 7 related variables
    ZP(0x66) = 0;
    ZP(0x67) = 0;
    ZP(0x68) = 0;
    ZP(0x69) = 0;

    // lda #$10 / sta $ad (mode 7 zoom level)
    ZP(0xAD) = 0x10;

    // ldx #0 / stx $06fb (mode 7 rotation angle) -- ABSOLUTE addressing
    // (opcode $8E, 3 bytes), not zero-page -- always $06FB regardless of D.
    // X is 16-bit (xf=0), so this writes two bytes (little endian).
    write16(ram, 0x06FB, 0);
}

// PITFALLS: DP (direct-page) bug -- see the FIX comment above the function.
// HELPERS: reset_sprites_emu(snes) - delegates ResetSprites @ 8980
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  none
//   output_ram:  0x06FB=2
//   entry_mode:  mf=true, xf=false, dp=cpu->dp (NOT 0x0 -- see FIX comment), db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::InitMapRAM ($83:4E)