# ff4-gnw — Final Fantasy IV native C port for Game & Watch

The **compiled overlay** that runs Final Fantasy IV on the Nintendo Game & Watch
(STM32H7B0VBT6). Wired into
[sylverb/game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd)
as a Homebrew overlay.

> **New to reverse engineering, 65816 assembly, or SNES hardware?** Read
> the umbrella repo's primer first —
> [`ff4/docs/primer/`](https://github.com/hcross/ff4/tree/main/docs/primer).
> For what's specific to *this* repository:
> [`docs/dispatch-integration.md`](docs/dispatch-integration.md) (how a
> validated routine ends up running here),
> [`docs/firmware-and-hardware.md`](docs/firmware-and-hardware.md) (the
> target device's constraints), and
> [`docs/build-and-flash-explained.md`](docs/build-and-flash-explained.md)
> (what the commands below are actually doing).

## Role in the ecosystem

```
ff4-port (workshop)          ff4-gnw (firmware overlay)
  upstream/ 65816 asm   →→   battle/, field/, …   ← validated C translations
  translator/ LLM pipeline        dispatch_all.c   ← dispatch table
  desktop/ wram_diff      →→   ff4_helpers.c       ← hardware glue & emu stubs
  port/ candidates              main.c             ← G&W wiring
                                snes/              ← LakeSnes, G&W-patched
```

**[ff4-port](https://github.com/hcross/ff4-port)** is the workshop: it holds
the 65816 disassembly, the LLM translation pipeline, and the A/B oracle
(`wram_diff`) that validates every function before it lands here.

**This repo** is the delivery end: all files here compile directly into the
G&W firmware. Nothing in this repo is a generation candidate or an
unreviewed draft.

### What lives here and why

| Directory / file | Nature | G&W-specific? |
|---|---|---|
| `battle/`, `field/`, `menu/`, `cutscene/`, `sound/` | Validated C translations of FF4 routines (current count: see [`DISPATCH_REGISTRY.md`](https://github.com/hcross/ff4/blob/main/DISPATCH_REGISTRY.md) in the umbrella repo — generated, changes as routines are ported) | No — pure 65816 logic, no hardware dependency |
| `dispatch_all.{c,h}` | Binary-search dispatch table; `ff4_dispatch_try` is called on every JSR/JSL. Also holds the per-slot variant gate array (`ff4_dispatch_gate[]`, see next row) | No — generic dispatch mechanism |
| `rom_ident.{c,h}`, `rom_profiles.{c,h}` | ROM identity + per-variant dispatch profiles: full-file CRC32 at `ff4_init` selects the profile that gates patch-invalidated dispatches back to the interpreter; unknown ROMs are refused on device (`FF4_REQUIRE_KNOWN_ROM`). `rom_profiles.{c,h}` are **generated** by the umbrella repo's `registry/patch_impact.py` — do not hand-edit. See [`docs/dispatch-integration.md`](docs/dispatch-integration.md) and ADR-008 in `ff4-port` | No — generic mechanism (the refusal UI lives in the retro-go-sd scaffold) |
| `ff4_helpers.c` | `*_emu` stubs for routines not yet translated; the few already translated ones delegate to their `_c` body | Partially — `update_ctrl_field_emu` reconstructs G&W input; most stubs are generic weak no-ops |
| `main.c` | `ff4_init`, `ff4_step`, `ff4_blit_to_lcd` (BGR888→RGB565) | **Yes** — G&W LCD wiring |
| `snes/` | LakeSnes SNES core, G&W-patched: static allocation under `-DFF4_PORT_STATIC_SNES`, ROM read XIP from extflash, dispatch hook in `cpu.c` JSR/JSL cases | **Yes** — G&W patches behind `#ifdef FF4_PORT_STATIC_SNES` |
| `ff4_redefines` | `objcopy --redefine-syms` map prefixing 24 SNES-core globals with `ff4__` to avoid collisions with smw/zelda3 overlays | **Yes** — retro-go-sd coexistence |
| `gen_dispatch.py` | Regenerates `dispatch_all.{c,h}` from `<mod>/*.c` + ff4-port validation logs | Tool |

## Status

- Boots FF4 through the Square Enix splash to the title screen on real
  hardware (G&W Mario, 64 MB extflash mod, JTAG).
- **Routines dispatched, by module and maturity level: see
  [`DISPATCH_REGISTRY.md`](https://github.com/hcross/ff4/blob/main/DISPATCH_REGISTRY.md)**
  in the umbrella repo — generated from the registry, not hand-counted
  here, so it never drifts out of date the way a static number in this
  README would (some files contain multiple entry points, which is part
  of why a quick manual count is easy to get wrong — see the umbrella
  repo's [`docs/primer/00-glossary.md`](https://github.com/hcross/ff4/blob/main/docs/primer/00-glossary.md#project-specific-terms)
  for what "dispatched" and the maturity levels mean).
- Sound is live: the SPC700/DSP run in LakeSnes' real APU, and the
  historical `ExecSound_ext` no-op stub was removed (`7420465`,
  2026-07-14 — music and SFX play, proven via the desktop harness's
  `--audio-crc` channel).
- Device savestates: 4 pause-menu slots, streamed and TAMP-compressed —
  see [`docs/firmware-and-hardware.md`](docs/firmware-and-hardware.md),
  "Savestates on the device".
- Known translation-patch variants are playable: the ROM is identified by
  CRC32 at `ff4_init` and the matching dispatch profile is armed. First
  variant: J2e EN v3.21, desktop-validated (device bench pending). See
  `rom_ident.{c,h}` in the table above.

## How to build and flash

> What each flag below actually does, and why a power cycle is needed
> after flashing: [`docs/build-and-flash-explained.md`](docs/build-and-flash-explained.md).

Cloned as `external/ff4` of `hcross/game-and-watch-retro-go-sd`
(branch `feat/ff4-port-scaffold`).

```bash
# Validated flag set for a G&W Mario with 64 MB extflash
make INTFLASH_BANK=2 FF4_AUTOBOOT=1 SD_CARD=0 EXTFLASH_SIZE_MB=4 \
     CHECK_DIRTY_SUBMODULE=0 -j4 all
make INTFLASH_BANK=2 FF4_AUTOBOOT=1 SD_CARD=0 EXTFLASH_SIZE_MB=4 \
     CHECK_DIRTY_SUBMODULE=0 flash

gnwmanager monitor   # observe "FF4 live: host=… dispatch=H/T"
```

- `INTFLASH_BANK=2` — app linked at `0x08100000`; bootloader at `0x08000000`.
- `SD_CARD=0` — ROM baked into FrogFS (no SD card mod required). With
  `SD_CARD=1` the `flash` recipe is a silent no-op.
- After `flash`, a power-cycle is needed for a real boot through the bootloader;
  `make flash` ends with `start` + `disable-debug` which bypasses it.

The ROM (`CRC32 CAA15E97`, FF4 JP 1.1) goes at
`sd_content/roms/homebrew/ff4.sfc`. It is gitignored and is wiped by
`make clean` — keep a copy outside `sd_content/`. Known translation-patch
variant images (e.g. J2e EN v3.21 as `ff4-j2e.sfc`, built per
`ff4-port/patches/README.md`) are staged the same way and selected by the
scaffold's persisted **Language** pause-menu option (applied at the next
launch); an image with an unknown CRC32 is refused at boot under
`-DFF4_REQUIRE_KNOWN_ROM`.

`-DFF4_AUTOBOOT=1` activates the diagnostic harness in
`Core/Src/porting/ff4/main_ff4.c`: the device autoboots straight into FF4 and
emits five greppable UART markers (`FF4_DIAG_PPU`, `FF4_DIAG_APU`,
`FF4_DIAG_PCHIST`, `FF4_DIAG_MISS`, `FF4_DIAG_ALIVE`).

## Upstream

- LakeSnes: [elzo-d/LakeSnes](https://github.com/elzo-d/LakeSnes)
- 65816 disassembly: [everything8215/ff4](https://github.com/everything8215/ff4)
- Translation pipeline: [hcross/ff4-port](https://github.com/hcross/ff4-port)
- Firmware: [sylverb/game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd)
