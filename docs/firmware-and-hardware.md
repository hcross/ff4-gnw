# The target hardware, and why it constrains the code the way it does

> Assumes the SNES-hardware background from
> [`ff4/docs/primer/01-snes-hardware-101.md`](https://github.com/hcross/ff4/blob/main/docs/primer/01-snes-hardware-101.md).
> This page is about the *destination* hardware, not the SNES.

## What a "Nintendo Game & Watch" is, here

The device this project targets is the modern (2020) Nintendo Game &
Watch handheld — physically nothing like the original 1980s Game & Watch
line it's named after. Inside, it's built around an **STM32H7B0VBT6**, a
single-chip microcontroller: one ARM Cortex-M7 CPU core, a fixed, small
amount of internal RAM, and external flash memory for storage. This is
much closer to what you'd find in embedded/IoT hardware than to a phone
or a desktop computer — there is no operating system managing memory for
you, no dynamic process model, no virtual memory.

## Why that matters for this specific port

- **No assumption of a large heap.** The SNES emulator core (LakeSnes,
  see [ADR-002 in `ff4-port`](https://github.com/hcross/ff4-port/blob/main/docs/adr/adr-002-lakesnes-upstream.md))
  normally allocates its state dynamically. On this device, nearly
  everything is **statically allocated** instead
  (`-DFF4_PORT_STATIC_SNES`) — single, fixed-size instances, sized once
  at compile time, because the firmware's general-purpose heap is small
  (~85 KB) and shared with the retro-go frontend. Two footnotes, both
  learned the hard way (2026-07-14, commit `a411391`): the
  controller-input component is **two** instances, one per SNES
  controller port — a naive single static aliased pad 2 onto pad 1,
  caught by framebuffer-CRC bisect; and the one deliberate exception is
  the APU's 64 KB of SPC RAM, which fits no static region (the overlay's
  slack is ~15 KB, DTCM is full, AHB is uncached) and stays on the heap
  — freeing it is an overlay-budget decision, parked on branch
  `fix/static-snes-components`.
- **ROM doesn't fit in RAM.** The original game ROM is read directly from
  external flash memory (XIP — execute/read in place) rather than loaded
  into RAM first, because the on-device RAM budget has no room to spare
  for a full copy of it.
- **The CPU itself is comparatively slow and in-order.** Unlike a modern
  desktop CPU (out-of-order, deeply pipelined, large caches), the
  Cortex-M7 here is in-order with slower external memory access. This is
  precisely why desktop validation timing numbers (see `ff4-port`'s
  [`docs/workflow/validation-oracle.md`](https://github.com/hcross/ff4-port/blob/main/docs/workflow/validation-oracle.md))
  cannot be used to predict on-device performance — only a real device
  run can answer "is this fast enough." By 2026-07-14 this lesson had
  bitten three times, hardening it into a rule: desktop profiling is
  disqualified even for *ranking* device cost buckets (e.g. the compose
  stage weighs ~10× less on the M7 than on the host, relative to its
  neighbors). Desktop runs remain valid for **finding** code paths and
  for byte-exactness proofs; only an on-device measurement ranks or
  sizes a cost.

## `retro-go-sd` — the firmware this project plugs into

This project doesn't write its own bootloader, filesystem, or hardware
drivers. It's built as a **homebrew overlay** for
[`sylverb/game-and-watch-retro-go-sd`](https://github.com/sylverb/game-and-watch-retro-go-sd),
an existing open-source firmware that already handles booting, storage,
and other emulator cores sharing the same device. This repository's job is
to provide `ff4_init`/`ff4_step`/`ff4_blit_to_lcd` and friends (`main.c`)
— a small, well-defined interface retro-go-sd calls into, plus a symbol
naming scheme (`ff4_redefines`, via `objcopy --redefine-syms`) so this
port's copy of the SNES core doesn't collide with other homebrew cores
(`smw`, `zelda3`) also linked into the same firmware image.

The scaffold also owns the **Language** pause-menu entry that switches
between the vanilla JP ROM and a translation-variant image
(confirmation dialog, then an automatic reset into the other language —
scaffold branch `feat/ff4-port-scaffold`, commit `cb67a933`). One
fork-specific trap it had to route around, worth knowing before adding
any new per-app setting: the generic `odroid_settings_int32_get/set`
API is a **no-op stub** in this fork — only the dedicated
`persistent_config_t` fields actually persist — so a setting stored
through it "works" in the menu and silently resets at every boot. The
language choice therefore lives in a dedicated one-byte LittleFS file
(`/ff4_lang`) owned by the FF4 app.

## Savestates on the device

The desktop harness can hold a whole ~270 KB savestate in one buffer;
the device cannot (see the heap point above). Device savestates
therefore **stream**: LakeSnes's state serializer
(`snes/statehandler.c`) exposes a byte-level **write hook** (added
for the live-device state exfiltration pipeline — see
[`ff4-port/FIXTURES.md`](https://github.com/hcross/ff4-port/blob/main/FIXTURES.md))
and, since commit `200212e`, a symmetric **read hook**. Every consumer
funnels through `sh_readByte`, so an installed hook can feed a load
byte-by-byte from a file through a small (512-byte) window instead of
requiring the whole state contiguous in RAM.

The pause-menu save/load integration built on these hooks lives in the
retro-go-sd scaffold branch (local commits `34ede4ac..a3792a60`, not
yet upstream), not in this repository. Two of its constraints are worth
knowing on the FF4 side, because they shaped the hook design:

- **Savestates are TAMP-compressed on the device filesystem** (~4:1 —
  273 KB → 67.7 KB measured, retro-go-sd `ce6da19d`), which is what
  lets all four pause-menu slots fit the 768 KB internal LittleFS.
- **The save runs two passes so the compressed length is known before
  the header is written** (retro-go-sd `258fd02b`), instead of seeking
  back to patch the header afterwards: a post-stream `fseek` rewrite on
  a LittleFS file rewrites its whole CTZ block chain and roughly
  doubles its on-disk footprint — the "no more free space" trap.

Since the Language option landed (2026-07-15), the slots are also
**namespaced per language**: the scaffold points the app descriptor's
`romPath` at the active language's ROM file, and every derived
savestate/screenshot path follows. Handler-side path rewriting does not
work — the slot UI derives its existence checks and previews from
`romPath`, so suffixed saves become invisible to it (scaffold commit
`cb67a933`).

A state saved from the pause menu loads **byte-identical** in the
desktop harness once extracted — the extraction recipe (and the second
device→desktop fixture pipeline it enables) is documented in
[`ff4-port/FIXTURES.md`](https://github.com/hcross/ff4-port/blob/main/FIXTURES.md).

## Continuing from here

- What the build/flash commands in the [README](../README.md) are actually
  doing: [`build-and-flash-explained.md`](build-and-flash-explained.md).
- How a routine gets from "validated in `ff4-port`" to "compiled into this
  firmware": [`dispatch-integration.md`](dispatch-integration.md).
