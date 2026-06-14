/* FF4 SPC700 mailbox responder — Phase C MVP (synchronous echo).
 *
 * Background. On a real SNES, the SPC700 boot IPL replies $BBAA at $2140-$2141
 * once it's ready, and the FF4 audio engine (sound.asm) uses a streaming
 * upload protocol where each byte sent must be echoed by the SPC in $2140
 * before the next byte is sent. See the InitSound and PlaySong routines in
 * upstream/sound/sound.asm:42-330 — the pattern is:
 *
 *     sta hAPUIO1          ; payload byte
 *     lda counter
 *     sta hAPUIO0          ; counter byte
 * @poll: cmp hAPUIO0       ; wait until SPC echoes counter
 *     bne @poll
 *
 * On G&W the SPC is stubbed (init_sound_emu / exec_sound_emu are weak no-ops)
 * to avoid running a SPC700 emulator on a chip that already struggles with
 * the main CPU. As a result outPorts stays at zero forever and the CPU spins
 * inside the @poll loop indefinitely. This responder unblocks that loop by
 * mirroring inPorts -> outPorts at the moment of the write. The bne sees the
 * match on the very next opcode and falls through.
 *
 * No audio is produced. The goal is to let the FF4 main CPU progress past
 * upload/handshake/upload-song phases so that gameplay code runs, and the
 * savestate becomes a usable shortcut. Audio synthesis (Phase E) is a
 * separate, larger chantier.
 *
 * Limits known up-front:
 *   - Some FF4 audio sub-routines may read back specific status bytes from
 *     the SPC after sending a command (status, ready, mute flags). For those
 *     a plain echo of the last write is unlikely to match. We'll extend the
 *     state machine as such mismatches surface during testing (Phase C+).
 */

#include "spc4_responder.h"
#include "snes/snes.h"
#include "snes/apu.h"

void ff4_snes_spc4_on_inport_write(Snes *snes, uint8_t port_index,
                                   uint8_t val) {
    /* Synchronous echo. The SPC IPL responds at $2140-$2141 with $AA $BB on
     * boot, but here the FF4 main CPU has already passed that point in any
     * savestate we'd care about, so a plain echo is a safe approximation
     * until proven otherwise. */
    snes->apu->outPorts[port_index & 0x3] = val;
}
