
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "snes.h"
#include "cpu.h"
#include "apu.h"
#include "spc.h"
#include "dma.h"
#include "ppu.h"
#include "cart.h"
#include "input.h"
#include "statehandler.h"

// ADR-006: the apu debt is tracked exactly, as an integer numerator of apu
// cycles against these denominators (apu cycles per master cycle = num/den;
// the historical floating-point accumulator survives only as the serialized
// state view, converted in snes_handleState).
static const int64_t apuCycNum = 32040 * 32;
static const int64_t apuCycDenNtsc = 1364 * 262 * 60;
static const int64_t apuCycDenPal = 1364 * 312 * 50;

static void snes_catchupApu(Snes* snes);
static void snes_doAutoJoypad(Snes* snes);
static uint8_t snes_readReg(Snes* snes, uint16_t adr);
static void snes_writeReg(Snes* snes, uint16_t adr, uint8_t val);
static uint8_t snes_rread(Snes* snes, uint32_t adr); // wrapped by read, to set open bus
static int snes_getAccessTime(Snes* snes, uint32_t adr);

#ifdef FF4_PORT_STATIC_SNES
/* G&W port: place the 128 KB Snes (mostly its ram[0x20000]) in
 * the .overlay_ff4 BSS section. The MCU heap is only 85 KB; malloc
 * of sizeof(Snes) would OOM via _sbrk. The static lives in RAM_EMU
 * (740 KB) which is reset on every overlay switch. */
static Snes _ff4_snes_storage;
#endif

Snes* snes_init(void) {
#ifdef FF4_PORT_STATIC_SNES
  Snes* snes = &_ff4_snes_storage;
#else
  Snes* snes = malloc(sizeof(Snes));
#endif
  snes->cpu = cpu_init(snes, snes_cpuRead, snes_cpuWrite, snes_cpuIdle);
  snes->apu = apu_init(snes);
  snes->dma = dma_init(snes);
  snes->ppu = ppu_init(snes);
  snes->cart = cart_init(snes);
  snes->input1 = input_init(snes);
  snes->input2 = input_init(snes);
  snes->palTiming = false;
  return snes;
}

void snes_free(Snes* snes) {
  cpu_free(snes->cpu);
  apu_free(snes->apu);
  dma_free(snes->dma);
  ppu_free(snes->ppu);
  cart_free(snes->cart);
  input_free(snes->input1);
  input_free(snes->input2);
#ifndef FF4_PORT_STATIC_SNES
  free(snes);
#endif
}

void snes_reset(Snes* snes, bool hard) {
  cpu_reset(snes->cpu, hard);
  apu_reset(snes->apu);
  dma_reset(snes->dma);
  ppu_reset(snes->ppu);
  input_reset(snes->input1);
  input_reset(snes->input2);
  cart_reset(snes->cart);
  if(hard) memset(snes->ram, 0, sizeof(snes->ram));
  snes->ramAdr = 0;
  snes->hPos = 0;
  snes->vPos = 0;
  snes->frames = 0;
  snes->cycles = 0;
  snes->syncCycle = 0;
  snes->apuCatchupCycles = 0.0;
  snes->apuPendingNum = 0;
  snes->ticksToEvent = 0;
  snes->irqCondInterior = false;
  snes->hIrqEnabled = false;
  snes->vIrqEnabled = false;
  snes->nmiEnabled = false;
  snes->hTimer = 0x1ff;
  snes->vTimer = 0x1ff;
  snes->inNmi = false;
  snes->irqCondition = false;
  snes->inIrq = false;
  snes->inVblank = false;
  memset(snes->portAutoRead, 0, sizeof(snes->portAutoRead));
  snes->autoJoyRead = false;
  snes->autoJoyTimer = 0;
  snes->ppuLatch = false;
  snes->multiplyA = 0xff;
  snes->multiplyResult = 0xfe01;
  snes->divideA = 0xffff;
  snes->divideResult = 0x101;
  snes->fastMem = false;
  snes->openBus = 0;
}

void snes_handleState(Snes* snes, StateHandler* sh) {
  sh_handleBools(sh,
    &snes->palTiming, &snes->hIrqEnabled, &snes->vIrqEnabled, &snes->nmiEnabled, &snes->inNmi, &snes->irqCondition,
    &snes->inIrq, &snes->inVblank, &snes->autoJoyRead, &snes->ppuLatch, &snes->fastMem, NULL
  );
  sh_handleBytes(sh, &snes->multiplyA, &snes->openBus, NULL);
  sh_handleWords(sh,
    &snes->hPos, &snes->vPos, &snes->hTimer, &snes->vTimer,
    &snes->portAutoRead[0], &snes->portAutoRead[1], &snes->portAutoRead[2], &snes->portAutoRead[3],
    &snes->autoJoyTimer, &snes->multiplyResult, &snes->divideA, &snes->divideResult, NULL
  );
  sh_handleInts(sh, &snes->ramAdr, &snes->frames, NULL);
  sh_handleLongLongs(sh, &snes->cycles, &snes->syncCycle, NULL);
  // ADR-006: .lss files keep the historical double view of the apu debt so
  // states stay loadable both ways; the live counter is the exact integer
  // numerator. palTiming was already restored by the bools above.
  {
    const int64_t den = snes->palTiming ? apuCycDenPal : apuCycDenNtsc;
    if(sh->saving) snes->apuCatchupCycles = (double)snes->apuPendingNum / (double)den;
    sh_handleDoubles(sh, &snes->apuCatchupCycles, NULL);
    if(!sh->saving) {
      const double num = snes->apuCatchupCycles * (double)den;
      snes->apuPendingNum = (int64_t)(num >= 0.0 ? num + 0.5 : num - 0.5);
    }
  }
  snes->ticksToEvent = 0; // downcounter cache is never trusted across a state load
  sh_handleByteArray(sh, snes->ram, 0x20000);
  // components
  cpu_handleState(snes->cpu, sh);
  dma_handleState(snes->dma, sh);
  ppu_handleState(snes->ppu, sh);
  apu_handleState(snes->apu, sh);
  input_handleState(snes->input1, sh);
  input_handleState(snes->input2, sh);
  cart_handleState(snes->cart, sh);
}

#ifdef FF4_PORT_STATIC_SNES
/* G&W port: pet the watchdog every N opcodes so LakeSnes's interpreter
 * (still single-stepping each 65816 instruction on a 280 MHz Cortex-M7)
 * does not trip the ~237 ms WWDG before a frame completes. Defined as
 * an empty weak symbol so the host can override with a real callback
 * (main_ff4.c implements it as wdog_refresh()). */
void __attribute__((weak)) ff4_port_wdog_refresh(void) {}
#define WDOG_PET_EVERY 4096
#endif

void snes_runFrame(Snes* snes) {
  // TODO: improve handling of dma's that take up entire vblank / frame
#ifdef FF4_PORT_STATIC_SNES
  unsigned pet_counter = 0;
#endif
  // run until we are starting a new frame (leaving vblank)
  while(snes->inVblank) {
    cpu_runOpcode(snes->cpu);
#ifdef FF4_PORT_STATIC_SNES
    if((++pet_counter & (WDOG_PET_EVERY - 1)) == 0) ff4_port_wdog_refresh();
#endif
  }
  // then run until we are at vblank, or we end up at next frame (DMA caused vblank to be skipped)
  uint32_t frame = snes->frames;
  while(!snes->inVblank && frame == snes->frames) {
    cpu_runOpcode(snes->cpu);
#ifdef FF4_PORT_STATIC_SNES
    if((++pet_counter & (WDOG_PET_EVERY - 1)) == 0) ff4_port_wdog_refresh();
#endif
  }
  snes_catchupApu(snes); // catch up the apu after running
}

// Like snes_runFrame but bounded by a CPU-opcode budget. Returns true if the
// frame completed, false if max_ops was hit first (a stall/hang). Used by the
// desktop A/B oracle (ADR 0001 / M3) so a native-side combat hang surfaces as a
// bounded divergence at a known frame instead of freezing the host. Device
// build never calls it; behaviour there is unchanged.
bool snes_runFrameBounded(Snes* snes, uint64_t max_ops) {
  uint64_t ops = 0;
  while(snes->inVblank) {
    cpu_runOpcode(snes->cpu);
    if(++ops >= max_ops) return false;
  }
  uint32_t frame = snes->frames;
  while(!snes->inVblank && frame == snes->frames) {
    cpu_runOpcode(snes->cpu);
    if(++ops >= max_ops) return false;
  }
  snes_catchupApu(snes);
  return true;
}

// Event-batched replacement for the historical per-tick snes_runCycle loop.
// The old code paid the full check chain (irq condition, positional events,
// autojoy timer, wrap tests) on every 2-master-cycle tick — ~357k times per
// frame, measured as ~67% of the frame on the G&W's Cortex-M7. Ticks that owe
// event work only exist at a handful of hPos values (0, 16, 512, 1104, the
// h-irq point, end of line); everything between is a straight run that can be
// applied in bulk. Semantics are tick-exact: event order, irq edge detection
// and the apu catchup accumulation are all preserved bit-for-bit (validated
// byte-identical on PPM+WRAM goldens and per-frame framebuffer CRCs).
void snes_runCycles(Snes* snes, int cycles) {
  if(snes->hPos + cycles >= 536 && snes->hPos < 536) {
    // if we go past 536, add 40 cycles for dram refersh
    cycles += 40;
  }
  int ticks = (cycles + 1) >> 1; // old loop was for(i=0;i<cycles;i+=2): ceil
  if(ticks <= 0) return;
  const int64_t apuNumPerTick = 2 * apuCycNum; // same numerator NTSC/PAL
  // fast path: the whole run stays inside the current segment — no tick owes
  // event work, every update is linear, O(1) per call. ticksToEvent is
  // maintained by the segment loop below and zeroed by anything that could
  // change the event set outside it (timer/irq register writes, reset,
  // state load), which forces the next call through the full machinery.
  if(ticks <= snes->ticksToEvent) {
    snes->ticksToEvent -= ticks;
    snes->irqCondition = snes->irqCondInterior;
    snes->apuPendingNum += apuNumPerTick * ticks;
    snes->cycles += 2 * (uint64_t)ticks;
    if(snes->autoJoyTimer > 0) {
      int dec = 2 * ticks;
      snes->autoJoyTimer =
        (snes->autoJoyTimer > dec) ? (uint16_t)(snes->autoJoyTimer - dec) : 0;
    }
    snes->hPos += 2 * ticks;
    return;
  }
  while(ticks > 0) {
    const int h = snes->hPos;
    const int v = snes->vPos;
    // ---- work owed by the tick that starts at (h, v) ----
    // irq edge check first, then positional events: same order as before
    bool condition = (
      (snes->vIrqEnabled || snes->hIrqEnabled) &&
      (v == snes->vTimer || !snes->vIrqEnabled) &&
      (h == snes->hTimer * 4 || !snes->hIrqEnabled)
    );
    if(!snes->irqCondition && condition) {
      snes->inIrq = true;
      cpu_setIrq(snes->cpu, true);
    }
    snes->irqCondition = condition;
    if(h == 0) {
      // end of hblank, do most vPos-tests
      bool startingVblank = false;
      if(v == 0) {
        // end of vblank
        snes->inVblank = false;
        snes->inNmi = false;
        ppu_handleFrameStart(snes->ppu);
      } else if(v == 225) {
        // ask the ppu if we start vblank now or at vPos 240 (overscan)
        startingVblank = !ppu_checkOverscan(snes->ppu);
      } else if(v == 240){
        // if we are not yet in vblank, we had an overscan frame, set startingVblank
        if(!snes->inVblank) startingVblank = true;
      }
      if(startingVblank) {
        // if we are starting vblank
        ppu_handleVblank(snes->ppu);
        snes->inVblank = true;
        snes->inNmi = true;
        if(snes->autoJoyRead) {
          // TODO: this starts a little after start of vblank
          snes->autoJoyTimer = 4224;
          snes_doAutoJoypad(snes);
        }
        if(snes->nmiEnabled) {
          cpu_nmi(snes->cpu);
        }
      }
    } else if(h == 16) {
      if(v == 0) snes->dma->hdmaInitRequested = true;
    } else if(h == 512) {
      // render the line halfway of the screen for better compatibility
      if(!snes->inVblank && v > 0) ppu_runLine(snes->ppu, v);
    } else if(h == 1104) {
      if(!snes->inVblank) snes->dma->hdmaRunRequested = true;
    }
    // ---- how far can we run before another tick owes event work? ----
    // evenFrame/frameInterlace/inVblank/timers only change inside the event
    // handlers above or in cpu-driven register writes, never mid-call, so
    // the line length and the event set are constant within this segment.
    int lineEnd;
    if(!snes->palTiming) {
      // line 240 of odd frame with no interlace is 4 cycles shorter
      lineEnd = (v == 240 && !snes->ppu->evenFrame && !snes->ppu->frameInterlace)
        ? 1360 : 1364;
    } else {
      // line 311 of odd frame with interlace is 4 cycles longer
      lineEnd = (v == 311 && !snes->ppu->evenFrame && snes->ppu->frameInterlace)
        ? 1368 : 1364;
    }
    int nextEvent = lineEnd;
    bool active = !snes->inVblank; // read AFTER the h==0 handler above
    if(h < 16 && v == 0) nextEvent = 16;
    else if(h < 512 && active && v > 0) nextEvent = 512;
    else if(h < 1104 && active) nextEvent = 1104;
    if(snes->hIrqEnabled) {
      int ht = snes->hTimer * 4;
      if(ht > h && ht < nextEvent) nextEvent = ht;
    }
    int n = (nextEvent - h) >> 1;
    if(n > ticks) n = ticks;
    if(n < 1) n = 1; // corrupt-hPos guard: march like the old code did
    // per-tick irq condition anywhere strictly inside this segment: hPos
    // cannot equal hTimer*4 there (the segment stops at it), so the h-term
    // only passes when the h-irq is disabled. No interior rising edge is
    // possible: this being true implies the boundary condition already was.
    const bool irqInterior = (
      (snes->vIrqEnabled || snes->hIrqEnabled) &&
      (v == snes->vTimer || !snes->vIrqEnabled) &&
      !snes->hIrqEnabled
    );
    // ---- bulk-apply the n ticks ----
    snes->apuPendingNum += apuNumPerTick * n; // exact integer debt (ADR-006)
    snes->cycles += 2 * (uint64_t)n;
    if(snes->autoJoyTimer > 0) {
      int dec = 2 * n;
      snes->autoJoyTimer =
        (snes->autoJoyTimer > dec) ? (uint16_t)(snes->autoJoyTimer - dec) : 0;
    }
    if(n > 1) {
      // irqCondition must end as the per-tick check would have left it
      snes->irqCondition = irqInterior;
    }
    snes->irqCondInterior = irqInterior;
    int newH = h + 2 * n;
    if(newH == lineEnd) {
      snes->ticksToEvent = 0; // hPos 0 owes event work next tick
      snes->hPos = 0;
      snes->vPos++;
      if(!snes->palTiming) {
        // even interlace frame is 263 lines
        if((snes->vPos == 262 && (!snes->ppu->frameInterlace || !snes->ppu->evenFrame)) || snes->vPos == 263) {
          snes->vPos = 0;
          snes->frames++;
        }
      } else {
        // even interlace frame is 313 lines
        if((snes->vPos == 312 && (!snes->ppu->frameInterlace || !snes->ppu->evenFrame)) || snes->vPos == 313) {
          snes->vPos = 0;
          snes->frames++;
        }
      }
    } else {
      // ticks runnable from the new position before the next owed-work tick.
      // Positional events are owed by the tick that STARTS at the boundary,
      // so landing exactly on one is fine (next call goes slow-path). The
      // line wrap is owed by the tick that ENDS at lineEnd — that tick must
      // run in the slow path, so it never counts as fast-path budget.
      int rem = (nextEvent - newH) >> 1;
      if(nextEvent == lineEnd && rem > 0) rem -= 1;
      snes->ticksToEvent = rem;
      snes->hPos = (uint16_t)newH;
    }
    ticks -= n;
  }
}

void snes_syncCycles(Snes* snes, bool start, int syncCycles) {
  if(start) {
    snes->syncCycle = snes->cycles;
    int count = syncCycles - (snes->cycles % syncCycles);
    snes_runCycles(snes, count);
  } else {
    int count = syncCycles - ((snes->cycles - snes->syncCycle) % syncCycles);
    snes_runCycles(snes, count);
  }
}

static void snes_catchupApu(Snes* snes) {
  // ADR-006: exact integer debt. C division truncates toward zero, mirroring
  // the historical (int) cast of the double (the debt can go negative when
  // apu_runCycles overruns, exactly as before).
  const int64_t den = snes->palTiming ? apuCycDenPal : apuCycDenNtsc;
  int catchupCycles = (int)(snes->apuPendingNum / den);
  int ranCycles = apu_runCycles(snes->apu, catchupCycles);
  snes->apuPendingNum -= (int64_t)ranCycles * den;
}

static void snes_doAutoJoypad(Snes* snes) {
  memset(snes->portAutoRead, 0, sizeof(snes->portAutoRead));
  // latch controllers
  input_latch(snes->input1, true);
  input_latch(snes->input2, true);
  input_latch(snes->input1, false);
  input_latch(snes->input2, false);
  for(int i = 0; i < 16; i++) {
    uint8_t val = input_read(snes->input1);
    snes->portAutoRead[0] |= ((val & 1) << (15 - i));
    snes->portAutoRead[2] |= (((val >> 1) & 1) << (15 - i));
    val = input_read(snes->input2);
    snes->portAutoRead[1] |= ((val & 1) << (15 - i));
    snes->portAutoRead[3] |= (((val >> 1) & 1) << (15 - i));
  }
}

uint8_t snes_readBBus(Snes* snes, uint8_t adr) {
  if(adr < 0x40) {
    return ppu_read(snes->ppu, adr);
  }
  if(adr < 0x80) {
    snes_catchupApu(snes); // catch up the apu before reading
    return snes->apu->outPorts[adr & 0x3];
  }
  if(adr == 0x80) {
    uint8_t ret = snes->ram[snes->ramAdr++];
    snes->ramAdr &= 0x1ffff;
    return ret;
  }
  return snes->openBus;
}

void snes_writeBBus(Snes* snes, uint8_t adr, uint8_t val) {
  if(adr < 0x40) {
    ppu_write(snes->ppu, adr, val);
    return;
  }
  if(adr < 0x80) {
    snes_catchupApu(snes); // catch up the apu before writing
    snes->apu->inPorts[adr & 0x3] = val;
#ifdef FF4_SPC4_RESPONDER
    /* MVP synchronous mailbox responder: the SPC700 is stubbed on G&W,
     * so polling loops in FF4 audio routines (PlaySong, InitSound, etc.)
     * — which write a counter to $2140 then spin on `cmp $2140 / bne` —
     * never see the SPC's expected echo and the CPU spins forever.
     * Mirror inPorts → outPorts synchronously at write time so the bne
     * loop exits on the very next opcode. No audio generated; this only
     * unblocks the FF4 audio-engine state machine so the main CPU can
     * proceed past upload/handshake phases. */
    extern void ff4_snes_spc4_on_inport_write(Snes *snes, uint8_t port,
                                              uint8_t val);
    ff4_snes_spc4_on_inport_write(snes, adr & 0x3, val);
#endif
    return;
  }
  switch(adr) {
    case 0x80: {
      snes->ram[snes->ramAdr++] = val;
      snes->ramAdr &= 0x1ffff;
      break;
    }
    case 0x81: {
      snes->ramAdr = (snes->ramAdr & 0x1ff00) | val;
      break;
    }
    case 0x82: {
      snes->ramAdr = (snes->ramAdr & 0x100ff) | (val << 8);
      break;
    }
    case 0x83: {
      snes->ramAdr = (snes->ramAdr & 0x0ffff) | ((val & 1) << 16);
      break;
    }
  }
}

static uint8_t snes_readReg(Snes* snes, uint16_t adr) {
  switch(adr) {
    case 0x4210: {
      uint8_t val = 0x2; // CPU version (4 bit)
      val |= snes->inNmi << 7;
      snes->inNmi = false;
      return val | (snes->openBus & 0x70);
    }
    case 0x4211: {
      uint8_t val = snes->inIrq << 7;
      snes->inIrq = false;
      cpu_setIrq(snes->cpu, false);
      return val | (snes->openBus & 0x7f);
    }
    case 0x4212: {
      uint8_t val = (snes->autoJoyTimer > 0);
      val |= (snes->hPos < 4 || snes->hPos >= 1096) << 6;
      val |= snes->inVblank << 7;
      return val | (snes->openBus & 0x3e);
    }
    case 0x4213: {
      return snes->ppuLatch << 7; // IO-port
    }
    case 0x4214: {
      return snes->divideResult & 0xff;
    }
    case 0x4215: {
      return snes->divideResult >> 8;
    }
    case 0x4216: {
      return snes->multiplyResult & 0xff;
    }
    case 0x4217: {
      return snes->multiplyResult >> 8;
    }
    case 0x4218:
    case 0x421a:
    case 0x421c:
    case 0x421e: {
      return snes->portAutoRead[(adr - 0x4218) / 2] & 0xff;
    }
    case 0x4219:
    case 0x421b:
    case 0x421d:
    case 0x421f: {
      return snes->portAutoRead[(adr - 0x4219) / 2] >> 8;
    }
    default: {
      return snes->openBus;
    }
  }
}

static void snes_writeReg(Snes* snes, uint16_t adr, uint8_t val) {
  switch(adr) {
    case 0x4200: {
      snes->autoJoyRead = val & 0x1;
      if(!snes->autoJoyRead) snes->autoJoyTimer = 0;
      snes->hIrqEnabled = val & 0x10;
      snes->vIrqEnabled = val & 0x20;
      if(!snes->hIrqEnabled && !snes->vIrqEnabled) {
        snes->inIrq = false;
        cpu_setIrq(snes->cpu, false);
      }
      // if nmi is enabled while inNmi is still set, immediately generate nmi
      if(!snes->nmiEnabled && (val & 0x80) && snes->inNmi) {
        cpu_nmi(snes->cpu);
      }
      snes->nmiEnabled = val & 0x80;
      snes->ticksToEvent = 0; // irq enables changed: rebuild the event set
      break;
    }
    case 0x4201: {
      if(!(val & 0x80) && snes->ppuLatch) {
        // latch the ppu
        ppu_read(snes->ppu, 0x37);
      }
      snes->ppuLatch = val & 0x80;
      break;
    }
    case 0x4202: {
      snes->multiplyA = val;
      break;
    }
    case 0x4203: {
      snes->multiplyResult = snes->multiplyA * val;
      break;
    }
    case 0x4204: {
      snes->divideA = (snes->divideA & 0xff00) | val;
      break;
    }
    case 0x4205: {
      snes->divideA = (snes->divideA & 0x00ff) | (val << 8);
      break;
    }
    case 0x4206: {
      if(val == 0) {
        snes->divideResult = 0xffff;
        snes->multiplyResult = snes->divideA;
      } else {
        snes->divideResult = snes->divideA / val;
        snes->multiplyResult = snes->divideA % val;
      }
      break;
    }
    case 0x4207: {
      snes->hTimer = (snes->hTimer & 0x100) | val;
      snes->ticksToEvent = 0; // h-irq point moved: rebuild the event set
      break;
    }
    case 0x4208: {
      snes->hTimer = (snes->hTimer & 0x0ff) | ((val & 1) << 8);
      snes->ticksToEvent = 0;
      break;
    }
    case 0x4209: {
      snes->vTimer = (snes->vTimer & 0x100) | val;
      snes->ticksToEvent = 0; // v-irq line moved: interior condition stale
      break;
    }
    case 0x420a: {
      snes->vTimer = (snes->vTimer & 0x0ff) | ((val & 1) << 8);
      snes->ticksToEvent = 0;
      break;
    }
    case 0x420b: {
      dma_startDma(snes->dma, val, false);
      break;
    }
    case 0x420c: {
      dma_startDma(snes->dma, val, true);
      break;
    }
    case 0x420d: {
      snes->fastMem = val & 0x1;
      break;
    }
    default: {
      break;
    }
  }
}

static uint8_t snes_rread(Snes* snes, uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if(bank == 0x7e || bank == 0x7f) {
    return snes->ram[((bank & 1) << 16) | adr]; // ram
  }
  if(bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if(adr < 0x2000) {
      return snes->ram[adr]; // ram mirror
    }
    if(adr >= 0x2100 && adr < 0x2200) {
      return snes_readBBus(snes, adr & 0xff); // B-bus
    }
    if(adr == 0x4016) {
      return input_read(snes->input1) | (snes->openBus & 0xfc);
    }
    if(adr == 0x4017) {
      return input_read(snes->input2) | (snes->openBus & 0xe0) | 0x1c;
    }
    if(adr >= 0x4200 && adr < 0x4220) {
      return snes_readReg(snes, adr); // internal registers
    }
    if(adr >= 0x4300 && adr < 0x4380) {
      return dma_read(snes->dma, adr); // dma registers
    }
  }
  // read from cart
  return cart_read(snes->cart, bank, adr);
}

/* WRAM write watchpoint hook (desktop diagnostics only; NULL on device). */
void (*snes_wram_write_hook)(uint32_t wram_off, uint8_t val, void *ctx) = NULL;
void *snes_wram_write_hook_ctx = NULL;

void snes_write(Snes* snes, uint32_t adr, uint8_t val) {
  snes->openBus = val;
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if(bank == 0x7e || bank == 0x7f) {
    uint32_t wram_idx = ((bank & 1) << 16) | adr;
    if(snes_wram_write_hook) snes_wram_write_hook(wram_idx, val, snes_wram_write_hook_ctx);
    snes->ram[wram_idx] = val; // ram
  }
  if(bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if(adr < 0x2000) {
      if(snes_wram_write_hook) snes_wram_write_hook(adr, val, snes_wram_write_hook_ctx);
      snes->ram[adr] = val; // ram mirror
    }
    if(adr >= 0x2100 && adr < 0x2200) {
      snes_writeBBus(snes, adr & 0xff, val); // B-bus
    }
    if(adr == 0x4016) {
      input_latch(snes->input1, val & 1); // input latch
      input_latch(snes->input2, val & 1);
    }
    if(adr >= 0x4200 && adr < 0x4220) {
      snes_writeReg(snes, adr, val); // internal registers
    }
    if(adr >= 0x4300 && adr < 0x4380) {
      dma_write(snes->dma, adr, val); // dma registers
    }
  }
  // write to cart
  cart_write(snes->cart, bank, adr, val);
}

static int snes_getAccessTime(Snes* snes, uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if((bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) && adr < 0x8000) {
    // 00-3f,80-bf:0-7fff
    if(adr < 0x2000 || adr >= 0x6000) return 8; // 0-1fff, 6000-7fff
    if(adr < 0x4000 || adr >= 0x4200) return 6; // 2000-3fff, 4200-5fff
    return 12; // 4000-41ff
  }
  // 40-7f,co-ff:0000-ffff, 00-3f,80-bf:8000-ffff
  return (snes->fastMem && bank >= 0x80) ? 6 : 8; // depends on setting in banks 80+
}

uint8_t snes_read(Snes* snes, uint32_t adr) {
  uint8_t val = snes_rread(snes, adr);
  snes->openBus = val;
  return val;
}

void snes_cpuIdle(void* mem, bool waiting) {
  Snes* snes = (Snes*) mem;
  dma_handleDma(snes->dma, 6);
  snes_runCycles(snes, 6);
}

uint8_t snes_cpuRead(void* mem, uint32_t adr) {
  Snes* snes = (Snes*) mem;
  int cycles = snes_getAccessTime(snes, adr);
  dma_handleDma(snes->dma, cycles);
  snes_runCycles(snes, cycles);
  return snes_read(snes, adr);
}

void snes_cpuWrite(void* mem, uint32_t adr, uint8_t val) {
  Snes* snes = (Snes*) mem;
  int cycles = snes_getAccessTime(snes, adr);
  dma_handleDma(snes->dma, cycles);
  snes_runCycles(snes, cycles);
  snes_write(snes, adr, val);
}

// debugging

void snes_runCpuCycle(Snes* snes) {
  cpu_runOpcode(snes->cpu);
}

void snes_runSpcCycle(Snes* snes) {
  // TODO: apu catchup is not aware of this, SPC runs extra cycle(s)
  spc_runOpcode(snes->apu->spc);
}
