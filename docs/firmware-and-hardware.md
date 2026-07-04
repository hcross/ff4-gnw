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
  normally allocates its state dynamically. On this device, everything is
  **statically allocated** instead (`-DFF4_PORT_STATIC_SNES`) — a single,
  fixed-size instance, sized once at compile time, because there's no
  general-purpose heap to assume is big enough.
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
  run can answer "is this fast enough."

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

## Continuing from here

- What the build/flash commands in the [README](../README.md) are actually
  doing: [`build-and-flash-explained.md`](build-and-flash-explained.md).
- How a routine gets from "validated in `ff4-port`" to "compiled into this
  firmware": [`dispatch-integration.md`](dispatch-integration.md).
