
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "apu.h"
#include "snes.h"
#include "spc.h"
#include "dsp.h"
#include "statehandler.h"

static const uint8_t bootRom[0x40] = {
  0xcd, 0xef, 0xbd, 0xe8, 0x00, 0xc6, 0x1d, 0xd0, 0xfc, 0x8f, 0xaa, 0xf4, 0x8f, 0xbb, 0xf5, 0x78,
  0xcc, 0xf4, 0xd0, 0xfb, 0x2f, 0x19, 0xeb, 0xf4, 0xd0, 0xfc, 0x7e, 0xf4, 0xd0, 0x0b, 0xe4, 0xf5,
  0xcb, 0xf4, 0xd7, 0x00, 0xfc, 0xd0, 0xf3, 0xab, 0x01, 0x10, 0xef, 0x7e, 0xf4, 0x10, 0xeb, 0xba,
  0xf6, 0xda, 0x00, 0xba, 0xf4, 0xc4, 0xf4, 0xdd, 0x5d, 0xd0, 0xdb, 0x1f, 0x00, 0x00, 0xc0, 0xff
};

static void apu_cycle(Apu* apu);
static void apu_syncTimers(Apu* apu);

Apu* apu_init(Snes* snes) {
  Apu* apu = malloc(sizeof(Apu));
  apu->snes = snes;
  apu->spc = spc_init(apu, apu_spcRead, apu_spcWrite, apu_spcIdle);
  apu->dsp = dsp_init(apu);
  return apu;
}

void apu_free(Apu* apu) {
  spc_free(apu->spc);
  dsp_free(apu->dsp);
  free(apu);
}

void apu_reset(Apu* apu) {
  // TODO: hard reset for apu
  spc_reset(apu->spc, true);
  dsp_reset(apu->dsp);
  memset(apu->ram, 0, sizeof(apu->ram));
  apu->dspAdr = 0;
  apu->romReadable = true;
  apu->cycles = 0;
  memset(apu->inPorts, 0, sizeof(apu->inPorts));
  memset(apu->outPorts, 0, sizeof(apu->outPorts));
  apu->timersLastSync = 0;   // FF4 A1 (apu->cycles was just reset)
  for(int i = 0; i < 3; i++) {
    apu->timer[i].cycles = 0;
    apu->timer[i].divider = 0;
    apu->timer[i].target = 0;
    apu->timer[i].counter = 0;
    apu->timer[i].enabled = false;
  }
}

void apu_handleState(Apu* apu, StateHandler* sh) {
  apu_syncTimers(apu);   // FF4 A1: saves serialize the exact materialized state
  sh_handleBools(sh, &apu->romReadable, NULL);
  sh_handleBytes(sh,
    &apu->dspAdr, &apu->inPorts[0], &apu->inPorts[1], &apu->inPorts[2], &apu->inPorts[3], &apu->inPorts[4],
    &apu->inPorts[5], &apu->outPorts[0], &apu->outPorts[1], &apu->outPorts[2], &apu->outPorts[3], NULL
  );
  sh_handleInts(sh, &apu->cycles, NULL);
  for(int i = 0; i < 3; i++) {
    sh_handleBools(sh, &apu->timer[i].enabled, NULL);
    sh_handleBytes(sh, &apu->timer[i].cycles, &apu->timer[i].divider, &apu->timer[i].target, &apu->timer[i].counter, NULL);
  }
  sh_handleByteArray(sh, apu->ram, 0x10000);
  apu->timersLastSync = apu->cycles;   // FF4 A1: loads resync the lazy clock
  // components
  spc_handleState(apu->spc, sh);
  dsp_handleState(apu->dsp, sh);
}

int apu_runCycles(Apu* apu, int wantedCycles) {
  int runCycles = 0;
  uint32_t startCycles = apu->cycles;
  while(runCycles < wantedCycles) {
    spc_runOpcode(apu->spc);
    runCycles += (uint32_t) (apu->cycles - startCycles);
    startCycles = apu->cycles;
  }
  return runCycles;
}

/* FF4 A1: lazy timers. The old apu_cycle ran the 3-timer countdown
 * bookkeeping on EVERY SPC cycle (~17k times/frame) for counters the
 * game reads a handful of times per frame ($FD-FF). The per-cycle loop
 * was, per timer: on countdown==0, reload (16/128) and -- if enabled --
 * divider++ with an 8-BIT EQUALITY test against target (so a target
 * written below the current divider only fires after the divider wraps
 * through 256), reset-and-count on hit, counter masked to 4 bits.
 * apu_syncTimers reproduces that loop in closed form over any elapsed
 * span; it is called before every observable access (counter reads,
 * $F1 enable writes, $FA-FC target writes) and around savestates, so
 * the serialized format and every read value are exactly those the
 * per-cycle loop would have produced. */
static void apu_syncTimers(Apu* apu) {
  const uint32_t elapsed = apu->cycles - apu->timersLastSync;
  if(elapsed == 0) return;
  apu->timersLastSync = apu->cycles;
  for(int i = 0; i < 3; i++) {
    Timer* t = &apu->timer[i];
    const uint32_t period = (i == 2) ? 16 : 128;
    const uint32_t c = t->cycles;
    // boundary hits: first when the countdown reaches 0 (c+1 cycles in),
    // then every `period` -- identical to the per-cycle reload pattern
    const uint32_t ticks = (elapsed + period - 1 - c) / period;
    t->cycles = (uint8_t)((c + period - (elapsed % period)) % period);
    if(ticks == 0 || !t->enabled) continue;
    // 8-bit divider marches to an equality with target (wrap included)
    const uint32_t tb = t->target;
    const uint32_t Tp = tb ? tb : 256;
    const uint32_t k1 = ((tb - (uint32_t)t->divider - 1) & 0xff) + 1;
    if(ticks < k1) {
      t->divider = (uint8_t)(t->divider + ticks);
    } else {
      const uint32_t fires = 1 + (ticks - k1) / Tp;
      t->divider = (uint8_t)((ticks - k1) % Tp);
      t->counter = (uint8_t)((t->counter + fires) & 0xf);
    }
  }
}

static void apu_cycle(Apu* apu) {
  if((apu->cycles & 0x1f) == 0) {
    // every 32 cycles
    dsp_cycle(apu->dsp);
  }
  apu->cycles++;   // timers are materialized lazily (apu_syncTimers)
}

uint8_t apu_read(Apu* apu, uint16_t adr) {
  switch(adr) {
    case 0xf0:
    case 0xf1:
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      return 0;
    }
    case 0xf2: {
      return apu->dspAdr;
    }
    case 0xf3: {
      return dsp_read(apu->dsp, apu->dspAdr & 0x7f);
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7:
    case 0xf8:
    case 0xf9: {
      return apu->inPorts[adr - 0xf4];
    }
    case 0xfd:
    case 0xfe:
    case 0xff: {
      apu_syncTimers(apu);   // FF4 A1: materialize before the read
      uint8_t ret = apu->timer[adr - 0xfd].counter;
      apu->timer[adr - 0xfd].counter = 0;
      return ret;
    }
  }
  if(apu->romReadable && adr >= 0xffc0) {
    return bootRom[adr - 0xffc0];
  }
  return apu->ram[adr];
}

void apu_write(Apu* apu, uint16_t adr, uint8_t val) {
  switch(adr) {
    case 0xf0: {
      break; // test register
    }
    case 0xf1: {
      apu_syncTimers(apu);   // FF4 A1: materialize before reconfiguring
      for(int i = 0; i < 3; i++) {
        if(!apu->timer[i].enabled && (val & (1 << i))) {
          apu->timer[i].divider = 0;
          apu->timer[i].counter = 0;
        }
        apu->timer[i].enabled = val & (1 << i);
      }
      if(val & 0x10) {
        apu->inPorts[0] = 0;
        apu->inPorts[1] = 0;
      }
      if(val & 0x20) {
        apu->inPorts[2] = 0;
        apu->inPorts[3] = 0;
      }
      apu->romReadable = val & 0x80;
      break;
    }
    case 0xf2: {
      apu->dspAdr = val;
      break;
    }
    case 0xf3: {
      if(apu->dspAdr < 0x80) dsp_write(apu->dsp, apu->dspAdr, val);
      break;
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7: {
      apu->outPorts[adr - 0xf4] = val;
      break;
    }
    case 0xf8:
    case 0xf9: {
      apu->inPorts[adr - 0xf4] = val;
      break;
    }
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      apu_syncTimers(apu);   // FF4 A1: the equality-march depends on the old target
      apu->timer[adr - 0xfa].target = val;
      break;
    }
  }
  apu->ram[adr] = val;
}

uint8_t apu_spcRead(void* mem, uint16_t adr) {
  Apu* apu = (Apu*) mem;
  apu_cycle(apu);
  return apu_read(apu, adr);
}

void apu_spcWrite(void* mem, uint16_t adr, uint8_t val) {
  Apu* apu = (Apu*) mem;
  apu_cycle(apu);
  apu_write(apu, adr, val);
}

void apu_spcIdle(void* mem, bool waiting) {
  Apu* apu = (Apu*) mem;
  apu_cycle(apu);
}
