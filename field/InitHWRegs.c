#include "snes/snes.h"

// field::InitHWRegs ($82:CB) — initialise SNES PPU and CPU hardware
// registers to a known state (mode 1, forced blank, zero scroll,
// disabled windows, no DMA, etc.).  All values are immediate; no
// inputs from RAM or registers.
void InitHWRegs_c(Snes *snes) {
    // Data bank = 0 (lda #0 / pha / plb)
    snes->cpu->db = 0;

    // NMITIMEN = 0 (disable NMI, IRQ, joypad auto-read)
    snes_write(snes, 0x4200, 0);

    // Direct page = 0 (ldx #0 / phx / pld)
    snes->cpu->dp = 0;

    // INIDISP: force blank + brightness 0
    snes_write(snes, 0x2100, 0x80);

    // BGMODE = 9 (mode 1, BG3 high priority)
    snes_write(snes, 0x2105, 0x09);

    // OAM address = 0 (stx hOAMADDL with X=0)
    snes_write(snes, 0x2102, 0);
    snes_write(snes, 0x2103, 0);

    // OBJSEL = 0 (txa → A=0)
    snes_write(snes, 0x2101, 0);

    // BG1/2 tile base addresses
    snes_write(snes, 0x210B, 0x22);   // BG12NBA
    snes_write(snes, 0x210C, 0x55);   // BG34NBA

    // BG tilemap addresses
    snes_write(snes, 0x2107, 0x63);   // BG1SC
    snes_write(snes, 0x2108, 0x59);   // BG2SC
    snes_write(snes, 0x2109, 0x73);   // BG3SC
    snes_write(snes, 0x210A, 0x73);   // BG4SC

    // VRAM address increment mode
    snes_write(snes, 0x2115, 0x80);   // VMAINC

    // clr_ax (tdc / tax) with DP=0 → A=0, X=0
    // Now write zero to all scroll, window, and screen-designation regs

    snes_write(snes, 0x2106, 0);      // MOSAIC

    // BG scroll offsets (each is a double-write: low then high byte)
    snes_write(snes, 0x210D, 0);      // BG1HOFS low
    snes_write(snes, 0x210D, 0);      // BG1HOFS high
    snes_write(snes, 0x210E, 0);      // BG1VOFS low
    snes_write(snes, 0x210E, 0);      // BG1VOFS high
    snes_write(snes, 0x210F, 0);      // BG2HOFS low
    snes_write(snes, 0x210F, 0);      // BG2HOFS high
    snes_write(snes, 0x2110, 0);      // BG2VOFS low
    snes_write(snes, 0x2110, 0);      // BG2VOFS high
    snes_write(snes, 0x2111, 0);      // BG3HOFS low
    snes_write(snes, 0x2111, 0);      // BG3HOFS high
    snes_write(snes, 0x2112, 0);      // BG3VOFS low
    snes_write(snes, 0x2112, 0);      // BG3VOFS high
    snes_write(snes, 0x2113, 0);      // BG4HOFS low
    snes_write(snes, 0x2113, 0);      // BG4HOFS high
    snes_write(snes, 0x2114, 0);      // BG4VOFS low
    snes_write(snes, 0x2114, 0);      // BG4VOFS high

    // Window selection
    snes_write(snes, 0x2123, 0);      // W12SEL
    snes_write(snes, 0x2124, 0);      // W34SEL
    snes_write(snes, 0x2125, 0);      // WOBJSEL

    // Window position
    snes_write(snes, 0x2126, 0);      // WH0
    snes_write(snes, 0x2127, 0);      // WH1
    snes_write(snes, 0x2128, 0);      // WH2
    snes_write(snes, 0x2129, 0);      // WH3

    // Window BG logic (stx hWBGLOG writes 0 to $212A and $212B)
    snes_write(snes, 0x212A, 0);
    snes_write(snes, 0x212B, 0);

    // Main/sub screen designations
    snes_write(snes, 0x212C, 0);      // TM
    snes_write(snes, 0x212D, 0);      // TS
    snes_write(snes, 0x212E, 0);      // TMW
    snes_write(snes, 0x212F, 0);      // TSW

    // DMA enable
    snes_write(snes, 0x420B, 0);      // MDMAEN
    snes_write(snes, 0x420C, 0);      // HDMAEN

    // Color math
    snes_write(snes, 0x2131, 0);      // CGADSUB
    snes_write(snes, 0x2133, 0);      // SETINI
    snes_write(snes, 0x2130, 0);      // CGSWSEL

    // Restore data bank to WRAM ($7E)
    snes->cpu->db = 0x7E;
}

// PITFALLS: none
// HELPERS: none
// CONTRACT:
//   inputs_reg:  none
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x0
//   entry_flags: z=auto, n=auto
// REVERSED_FUNCTION: field::InitHWRegs ($82:CB)