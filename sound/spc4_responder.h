#ifndef FF4_SPC4_RESPONDER_H
#define FF4_SPC4_RESPONDER_H

#include <stdint.h>

struct Snes;

/* Called from snes_writeBBus() at every write to $2140-$2143 (B-bus 0x40-0x43)
 * when FF4_SPC4_RESPONDER is defined. Phase C-MVP: synchronous echo of the
 * byte the CPU just wrote to inPorts, mirrored into outPorts so the SPC IPL
 * handshake + FF4 audio upload polling loops exit immediately. No real
 * audio synthesis. */
void ff4_snes_spc4_on_inport_write(struct Snes *snes, uint8_t port_index,
                                   uint8_t val);

#endif
