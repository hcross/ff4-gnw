#include "snes/snes.h"

void LoadTitleGfx_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    uint8_t *ram = snes->ram;

    // Set up TfrVRAM parameters in WRAM (DB=$7E)
    write16(ram, 0x47, 0x0000);   // destination VRAM address
    write16(ram, 0x45, 0x2000);   // transfer size
    ram[0x3C] = 0x08;             // source bank (TitleGfx at $08:C000)
    write16(ram, 0x3D, 0xC000);   // source offset low word

    // Set CPU state for TfrVRAM call (battle module convention)
    cpu->db = 0x7E;
    cpu->dp = 0;
    cpu->mf = true;   // A 8-bit
    cpu->xf = false;  // X/Y 16-bit

    // Delegate to TfrVRAM via emulator (avoids unresolved stub)
    run_emulated_func(snes, 0x03CA85);
}
// PITFALLS: 1 (DB set to $7E before calling TfrVRAM), 8 (mode flags
//          explicitly set to field/battle module convention)
// HELPERS: run_emulated_func(snes, 0x03CA85) — calls TfrVRAM directly
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none (side effect: VRAM transfer)
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: none
// REVERSED_FUNCTION: field::LoadTitleGfx ($00:8690)