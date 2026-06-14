#include "snes/snes.h"

/* $15:CA85 - "transfer data to vram"
 *
 * Original ASM (44 bytes):
 *   LDA #$80 / STA $2115     VMAINC = $80 (auto-incr after high byte write)
 *   STZ $420B                MDMAEN clear
 *   LDA #$01 / STA $4300     DMA0 ctrl = mode 1 (2 bytes -> dest, dest+1)
 *   LDA #$18 / STA $4301     DMA0 dest = $2118 (VMDATAL)
 *   LDA $3C  / STA $4304     DMA0 src bank = byte at $3C
 *   LDX $47  / STX $2116     VMADDR = word at $47 (VRAM destination)
 *   LDX $3D  / STX $4302     DMA0 src low/high = word at $3D
 *   LDX $45  / STX $4305     DMA0 count = word at $45
 *   LDA #$01 / STA $420B     trigger MDMA channel 0
 *   RTL
 *
 * Effect: copy <count> bytes from <src_bank>:<src_addr> to VRAM at <vram_dest>,
 * via DMA channel 0 in word-pair mode. VMAINC=$80 means VMADD auto-increments
 * after each high-byte ($2119) write, so consecutive 2-byte transfers fill
 * VRAM linearly starting at VMADDR.
 *
 * G&W port: bypass dma_startDma (the cycle-sync path crashes post-savestate
 * for the same reason _15cadc had to avoid it). Stream the source bytes
 * directly via snes_writeBBus to $2118 / $2119 in alternation. Source is
 * read via snes_read so ROM accesses work transparently.
 */
void _15ca85_c(Snes *snes) {
    uint8_t *ram = snes->ram;

    /* VMAINC = $80: VMADD auto-increments by 1 after each $2119 write. */
    snes_writeBBus(snes, 0x15, 0x80);

    /* Set VRAM destination from zero-page word at $47. */
    uint16_t vram_dest = (uint16_t)ram[0x47] | ((uint16_t)ram[0x48] << 8);
    snes_writeBBus(snes, 0x16, (uint8_t)(vram_dest & 0xFF));
    snes_writeBBus(snes, 0x17, (uint8_t)((vram_dest >> 8) & 0xFF));

    /* Source address: bank at $3C, addr at $3D/$3E. */
    uint8_t  src_bank = ram[0x3C];
    uint16_t src_addr = (uint16_t)ram[0x3D] | ((uint16_t)ram[0x3E] << 8);
    uint32_t src = ((uint32_t)src_bank << 16) | src_addr;

    /* Size in bytes from zero-page word at $45. */
    uint16_t size = (uint16_t)ram[0x45] | ((uint16_t)ram[0x46] << 8);

    /* Stream bytes alternating low/high to VMDATAL/VMDATAH. */
    for (uint16_t i = 0; i < size; i += 2) {
        uint8_t lo = snes_read(snes, (src + i) & 0xFFFFFF);
        uint8_t hi = snes_read(snes, (src + i + 1) & 0xFFFFFF);
        snes_writeBBus(snes, 0x18, lo);  /* $2118 VMDATAL */
        snes_writeBBus(snes, 0x19, hi);  /* $2119 VMDATAH (triggers VMADD++) */
    }
}

/* PITFALLS: 6 (16-bit zero page reads via paired bytes), 8 (DMA timing),
 *           12 (ROM read via snes_read works for any bank)
 * HELPERS: snes_writeBBus, snes_read
 * CONTRACT:
 *   inputs_reg:  none
 *   inputs_ram:  $3C ($00:003C, src bank),
 *                $3D/$3E ($00:003D, src addr 16-bit),
 *                $45/$46 ($00:0045, count 16-bit),
 *                $47/$48 ($00:0047, VRAM dest 16-bit)
 *   output_ram:  none (PPU VRAM via $2118/$2119)
 *   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
 *   entry_flags: z=auto, n=auto
 * REVERSED_FUNCTION: field::_15ca85 ($15:CA85) - "transfer data to vram"
 */
