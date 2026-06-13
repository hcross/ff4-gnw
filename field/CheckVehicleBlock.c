#include "snes/snes.h"

// ADR-003 delegate: routine uses ROM tables (XMoveTbl, YMoveTbl) not defined in the C environment.
// To maintain parity and avoid "undeclared identifier" errors, the logic is delegated 
// to the emulator which has access to the original ROM byte-stream.
void CheckVehicleBlock_c(Snes *snes) {
    Cpu *cpu = snes->cpu;
    
    // Setup environment for field module convention
    cpu->dp = 0;
    cpu->db = 0x00; // ROM bank for field code
    cpu->mf = true; // A is 8-bit
    cpu->xf = false; // X/Y are 16-bit
    
    // The routine reads $0709 (direction) at entry, so we ensure
    // the CPU state is consistent. The emulator will handle the 
    // ROM table lookups and RAM writes to $0C, $0E, and $0A.
    run_emulated_func(snes, 0x00AC7Du);
}

// PITFALLS: None (Delegated to avoid ROM table definition errors)
// HELPERS: run_emulated_func(snes, addr)
// CONTRACT:
//   inputs_reg:  a=none, x=none, y=none
//   inputs_ram:  0x0709=1, 0x1706=1, 0x1707=1, 0x1704=1, 
//                0x1719=1, 0x171A=1, 0x1715=1, 0x1713=1, 0x1714=1,
//                0x171D=1, 0x171E=1, 0x1721=1, 0x1722=1, 0x1725=1, 0x1726=1
//   output_ram:  0x0A=1
//   entry_mode:  mf=true, xf=false, dp=0x0, db=0x00
//   entry_flags: z=auto, n=auto
// DELEGATED_FUNCTION: field::CheckVehicleBlock ($00:AC7D)