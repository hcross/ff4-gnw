#include "snes/snes.h"
#include "ff4_helpers.h"

/* field::_00885e ($00:885E) - transfer 1 row of crystal (4 tiles)
 *
 * Original ASM:
 *   LDA #$80 / STA $2115         VMAINC = $80 (auto-incr on high write)
 *   LDX $47  / STX $2116         VMADDR = word at $47
 *   LDX $3D
 *   loop @886A:
 *     LDA $08:0000,X / STA $2118   src low to VMDATAL
 *     LDA $08:0001,X / STA $2119   src high to VMDATAH (triggers VMADD++)
 *     INX INX
 *     TXA / AND #$07 / BNE loop    loop until low 3 bits of X are 0
 *   REP #$20
 *   LDA $3D / CLC / ADC #$0008 / STA $3D  advance $3D by 8
 *   ...
 *
 * Effect: copy 8 bytes (= 1 row of 4 tiles, 2 bytes each) from ROM at
 * $08:<X> to VRAM starting at <VMADDR>, then advance the WRAM source
 * pointer ($3D) by 8 so the next call picks up where this one left off.
 *
 * Used by title screen init at $00:8693 and $00:86B0 to stream the
 * crystal tilemap entries one row at a time. */
void _00885e_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    /* VMAINC = $80: VMADDR auto-increments after each $2119 write. */
    snes_writeBBus(snes, 0x15, 0x80);

    /* Set VRAM destination from zero page $47/$48. */
    uint16_t vram_dest = (uint16_t)ram[0x47] | ((uint16_t)ram[0x48] << 8);
    snes_writeBBus(snes, 0x16, (uint8_t)(vram_dest & 0xFF));
    snes_writeBBus(snes, 0x17, (uint8_t)((vram_dest >> 8) & 0xFF));

    /* Source = bank $08, offset $3D. Loop transfers 8 bytes (4 tile-rows,
     * each row being one VRAM word = 2 bytes). */
    uint16_t src_off = (uint16_t)ram[0x3D] | ((uint16_t)ram[0x3E] << 8);
    uint32_t src_base = (0x08u << 16) | src_off;
    for (int i = 0; i < 4; i++) {
        uint8_t lo = snes_read(snes, (src_base + 2 * i) & 0xFFFFFF);
        uint8_t hi = snes_read(snes, (src_base + 2 * i + 1) & 0xFFFFFF);
        snes_writeBBus(snes, 0x18, lo);
        snes_writeBBus(snes, 0x19, hi);
    }

    /* Advance source pointer by 8 (the original does a 16-bit ADC). */
    uint16_t new_off = src_off + 8;
    write16(ram, 0x3D, new_off);
}

/* PITFALLS: 6 (16-bit add via ADC #$0008), 12 (ROM read via snes_read)
 * HELPERS: snes_writeBBus, snes_read, write16
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  $3D/$3E (src offset), $47/$48 (VRAM dest)
 *   output_ram:  $3D/$3E (advanced by 8), VRAM as side effect
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_00885e ($00:885E) - "transfer 1 row of crystal"
 */
