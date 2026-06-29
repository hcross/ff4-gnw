#include "snes/snes.h"

// Logic:
//   Hides the "Yes/No" window by performing a DMA transfer of 
//   YesNoTilesHide tiles into VRAM.
//   1. Forces VRAM access/disables screen ($2115 = 0x80).
//   2. Sets up DMA source (YesNoTilesHide ROM address) and size (0x10).
//   3. Sets VRAM destination address via $2116 (loaded from WRAM $3D).
void CloseYesNoWindow_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    // $2115/$2116 and $43xx are MMIO (not WRAM) → bus. The DMA *trigger* is
    // delegated to ExecDMA (interpreter, where cycles flow), so once the channel
    // is programmed through the bus the transfer runs — no manual loop. The
    // original port wrote everything to WRAM and left the source as "Placeholder"
    // $0000 → nothing transferred (yes/no window never hidden).
    snes_writeBBus(snes, 0x15, 0x80);   // $2115 VMAIN

    InitDMA_emu(snes);                  // DMA0 BBAD=$18 ($2118 VMDATA) etc.

    snes_write(snes, 0x4300, 0x01);     // DMA0 DMAP — word writes ($2118/$2119)

    uint16_t vram_addr = read16(ram, 0x3D);   // ldx $3d / stx $2116 (VRAM dest)
    snes_writeBBus(snes, 0x16, (uint8_t)(vram_addr & 0xFF));
    snes_writeBBus(snes, 0x17, (uint8_t)(vram_addr >> 8));

    snes_write(snes, 0x4302, 0xC6);     // A1T = loword(YesNoTilesHide) = $F6C6
    snes_write(snes, 0x4303, 0xF6);
    snes_write(snes, 0x4304, 0x15);     // A1B = bankbyte(YesNoTilesHide) = $15
    snes_write(snes, 0x4305, 0x10);     // DAS size = $0010
    snes_write(snes, 0x4306, 0x00);

    ExecDMA_emu(snes);                  // triggers channel 0 in the interpreter
}

// PITFALLS: 6 (Mode A 8-bit vs 16-bit), 8 (Inherited mf=true, xf=false)
// HELPERS: InitDMA_emu(snes), ExecDMA_emu(snes), read16, write16
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x3D=2
//   output_ram:  0x2115=1, 0x2116=2, 0x4300=1, 0x4302=2, 0x4304=1, 0x4305=2
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::CloseYesNoWindow ($AF:24)