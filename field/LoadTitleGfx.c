#include "snes/snes.h"
#include "ff4_helpers.h"
#include "dispatch_all.h"

/* field::LoadTitleGfx ($00:8690) - load title screen bg graphics
 *
 * Original ASM (10 instructions):
 *   LDX #$0000 / STX $47    VRAM destination = $0000
 *   LDX #$2000 / STX $45    transfer size = 0x2000 bytes
 *   LDA #$08   / STA $3C    source bank = $08 (TitleGfx)
 *   LDX #$C000 / STX $3D    source offset = $C000 (TitleGfx low/high)
 *   JSL $15CA85             call "transfer data to vram"
 *   RTS
 *
 * Just a parameter setup + JSL to $15:CA85. We have that target ported
 * in field/_15ca85.c (direct VMDATAL/VMDATAH stream, no DMA engine), so
 * call it directly here. Avoids run_emulated_func entirely - the G&W
 * port doesn't implement that path and the original cascade output
 * targeted a wrong address ($03CA85 instead of $15CA85). */
void LoadTitleGfx_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    /* VRAM destination = $0000. */
    write16(ram, 0x47, 0x0000);
    /* Transfer size = 0x2000 bytes (one full 4 KB VRAM page x 2). */
    write16(ram, 0x45, 0x2000);
    /* Source bank = $08, source offset = $C000 (TitleGfx in upstream). */
    ram[0x3C] = 0x08;
    write16(ram, 0x3D, 0xC000);

    /* Delegate to the in-tree port of $15:CA85. */
    _15ca85_c(snes);
}

/* PITFALLS: none (pure parameter shuffle + dispatch).
 * HELPERS: write16, _15ca85_c
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  none
 *   output_ram:  $0045/$0047/$003C/$003D (TfrVRAM params),
 *                + PPU VRAM as side effect
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::LoadTitleGfx ($00:8690)
 */
