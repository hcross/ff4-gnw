#include "snes/snes.h"
#include "dispatch_all.h"

/* TfrVRAM ($CA85) and _15ca85 ($15:CA85) are the SAME "transfer data to VRAM"
 * routine, reached via different LoROM bank encodings (the dispatch table
 * carries both $01:CA85 -> TfrVRAM_c and $15:CA85 -> _15ca85_c). _15ca85_c is
 * the working direct C port: it reads the source/dest/size params from
 * zero-page ($3C/$3D src bank+addr, $47 VRAM dest, $45 size) and streams to
 * VRAM via snes_writeBBus ($2116/$2118/$2119).
 *
 * The title screen reaches THIS entry via `jsl TfrVRAM` (title_jp.asm) to load
 * the "FINAL FANTASY IV" logo tilemap from $08:E800 to VRAM $1880 (and the
 * second half to $3080). The previous body delegated to
 * run_emulated_func(0x00CA85) — the wrong bank ($00 instead of the linked
 * $15) — so the logo tilemap never transferred and BG1/BG2 stayed on the
 * $00DF clear fill. Delegate to the proven port instead. */
void TfrVRAM_c(Snes *snes) {
    _15ca85_c(snes);
}

// CONTRACT:
//   inputs_ram:  0x3C=1, 0x3D=2 (src bank:addr), 0x47=2 (VRAM dest), 0x45=2 (size)
//   output:      PPU VRAM (via snes_writeBBus)
// REVERSED_FUNCTION: field::TfrVRAM ($CA85) == field::_15ca85 ($15:CA85)