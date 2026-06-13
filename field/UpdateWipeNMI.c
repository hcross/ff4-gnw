#include "snes/snes.h"

// field::UpdateWipeNMI ($92:04)
// Sets up PPU window registers (WH0, WH1) and VTIMEL for the screen wipe
// effect, based on a phase counter in $79.  Reads a scanline table from
// ROM bank $92.  Also computes a value for $0677 and enables NMI/IRQ.
void UpdateWipeNMI_c(Snes *snes) {
    uint8_t *ram = snes->ram;
    uint8_t phase = ram[0x79];          // lda $79

    // asl; tax -> X = phase * 2 (byte offset into table)
    // Assumes B=0 on entry (high byte of A is zero).
    uint16_t x = (uint16_t)(phase << 1); // asl (8-bit) then tax (16-bit, B=0)

    // WipeScanlineTbl is in bank $92.  Offset must be verified against the
    // disassembly; 0xF800 is a plausible placeholder.
    #define WIPE_SCANLINE_TBL_OFFSET 0xF800
    uint32_t tbl_base = 0x920000 + WIPE_SCANLINE_TBL_OFFSET;

    uint8_t tbl_high = snes_read(snes, tbl_base + 1 + x); // f:WipeScanlineTbl+1,x
    uint8_t tbl_low  = snes_read(snes, tbl_base + x);     // f:WipeScanlineTbl,x

    // VTIMEL = 0x6F - high byte of table entry
    uint8_t a = 0x6F;                   // lda #$6f
    // sec (carry set) -> sbc subtracts without borrow
    a = (uint8_t)(a - tbl_high);        // sbc f:WipeScanlineTbl+1,x
    // tay -> Y = a (B=0)
    snes_write(snes, 0x4207, a);        // sty hVTIMEL (low byte of Y)

    // WH0 = 0x80 - low byte of table entry
    a = 0x80;                           // lda #$80
    // sec
    a = (uint8_t)(a - tbl_low);         // sbc f:WipeScanlineTbl,x
    snes_write(snes, 0x2126, a);        // sta hWH0

    // WH1 = 0x7F + low byte of table entry
    a = 0x7F;                           // lda #$7f
    // clc (carry clear) -> adc adds without carry-in
    a = (uint8_t)(a + tbl_low);         // adc f:WipeScanlineTbl,x
    snes_write(snes, 0x2127, a);        // sta hWH1

    // Compute value for $0677: ((phase >> 1) << 4) + 3
    a = phase;                          // lda $79
    a = (uint8_t)(a >> 1);             // lsr
    a = (uint8_t)(a << 4);             // asl4 (four left shifts)
    // clc
    a = (uint8_t)(a + 3);              // adc #$03
    ram[0x0677] = a;                    // sta $0677

    // Enable NMI and IRQ, set brightness
    snes_write(snes, 0x4200, 0xA1);     // lda #$a1 / sta hNMITIMEN
    snes_write(snes, 0x2100, 0x80);     // lda #$80 / sta hINIDISP
}

// PITFALLS:
//   1 (DB=$7E required for absolute stores like sta $0677)
//   6 (mode A 8-bit assumed; mf=true, xf=false)
//   7 (8-bit truncation after shifts and arithmetic)
//   9 (B=0 assumed on entry; tax/tay zero-extend the 8-bit result)
// HELPERS: none
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  0x79=1
//   output_ram:  0x0677=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x7E
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::UpdateWipeNMI ($92:04)