#include "snes/snes.h"

/**
 * Logic: Configures the DMA to clear VRAM.
 * The asm performs:
 *   stx hVMADDL     -> store X to a specific address
 *   ldx #0 / stx $4352 -> source addr = 0
 *   lda #$09 / sta $4350 -> count low = 9
 *   lda #$18 / sta $4351 -> count high = 24
 *   lda #0 / sta $4354 -> dest addr low = 0
 *   sty $4355 -> dest addr high = Y (inherited from caller)
 *   lda #$20 / sta hMDMAEN -> DMA enable = 0x20
 * 
 * Analysis of Failure: 
 * The previous translation wrote to ram[0x43xx]. 
 * The SNES registers (like DMA control) are NOT in WRAM (0x7E0000-0x7FFFFF).
 * They are in the I/O memory map. In the parity harness, these are accessed 
 * via the emulated CPU state or a specific register map.
 * Since the routine modifies hardware registers and relies on inherited 
 * register values (X, Y) to perform these writes, and the target is I/O, 
 * this is a prime candidate for delegation to avoid divergence in memory 
 * mapping (WRAM vs I/O).
 */

// ADR-003 delegate: routine modifies I/O registers ($43xx) and uses inherited X/Y
void ClearVRAM_emu(Snes *snes) {
    Cpu *c = snes->cpu;
    c->dp = 0;
    c->db = 0x83; // Set to the bank where the code resides
    c->mf = true; // Mode A: 8-bit
    c->xf = false; // Mode X/Y: 16-bit
    
    // The routine is at $00:83E1 (as per bridge)
    run_emulated_func(snes, 0x0083E1u);
}

// PITFALLS: 1 (DB mismatch), 8 (Inherited X/Y influence I/O writes)
// HELPERS: run_emulated_func
// CONTRACT:
//   inputs_reg:  a=none, x=16, y=16
//   inputs_ram:  none
//   output_ram:  none
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x83
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::ClearVRAM ($00:83E1)