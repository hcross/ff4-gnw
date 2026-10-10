#include "snes/snes.h"

/* TfrInvertPal ($00:EE37 -- annotated $EE35, off-by-2; sole caller
 * `jsr $EE37` in FieldNMI @ $00:92D1, entered with m=8, x=16, D=$0600, DB=$00).
 *
 *   stz $420b ; stz $2121            ; CGADD = 0
 *   lda #$02 / sta $4300             ; DMAP0: A->B, mode 2 (one reg, twice)
 *   lda #$22 / sta $4301             ; BBAD0 = $2122 (CGDATA)
 *   lda #$00 / sta $4304             ; A1B0 = $00
 *   ldx #$0bdb / stx $4302           ; A1T0 = $0BDB (palette buffer)
 *   ldx #$0100 / stx $4305           ; DAS0 = 256 bytes
 *   lda #$01 / sta $420b             ; run channel 0
 *
 * The transfer is replicated as 256 CGDATA writes (DMA from C does not flush
 * on the harness, F6, and a DMA from a savestate-resumed device hard-faults,
 * F3); the channel-0 registers are then set to the values a completed DMA
 * leaves (A1T0 advanced by the count, DAS0 = 0) so the post-call bus state
 * matches the asm -- writing $43xx only stores registers, it never triggers.
 *
 * History: keyed at $12:EE35 ($FF padding) until 2026-10-10, so this body had
 * never run in game (F16, ADR-009); also wrote CGADD/DMA regs as WRAM. */
void TfrInvertPal_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    snes_writeBBus(snes, 0x21, 0x00);                    /* $2121 CGADD = 0 */
    for (uint32_t src = 0x000BDB; src < 0x000CDB; src++)
        snes_writeBBus(snes, 0x22, snes_read(snes, src)); /* $2122 CGDATA */
    snes_write(snes, 0x4300, 0x02);
    snes_write(snes, 0x4301, 0x22);
    snes_write(snes, 0x4304, 0x00);
    snes_write(snes, 0x4302, 0xDB);                       /* A1T0 = $0CDB */
    snes_write(snes, 0x4303, 0x0C);
    snes_write(snes, 0x4305, 0x00);                       /* DAS0 = 0 */
    snes_write(snes, 0x4306, 0x00);
    cpu->a = (uint16_t)((cpu->a & 0xFF00) | 0x01);       /* lda #$01 */
    cpu->x = 0x0100;                                      /* ldx #$0100 */
    cpu->z = false;
    cpu->n = false;
}

// PITFALLS: off-by-2 entry ($EE37), MMIO via the bus (never ram[]), DB=$00 not $EE
// HELPERS: none
// CONTRACT:
//   inputs_reg:  a=2, x=none, y=none
//   inputs_ram:  0x0BDB..0CDA=palette_buffer
//   output_ram:  none
//   mmio_effects: $2121, $420B, $4300, $4301, $4302, $4304, $4305
//   dma:         manual-loop
//   entry_mode:  mf=true, xf=false, dp=0x0600, db=0x00
//   entry_flags: z=auto, n=auto
// SPIKE_COMPARE: region
// REVERSED_FUNCTION: field::TfrInvertPal ($00:EE37)
