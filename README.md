# ff4-gnw — Final Fantasy IV native C port for Game & Watch

The **compiled overlay** that runs Final Fantasy IV on the Nintendo Game & Watch
(STM32H7B0VBT6). Wired into
[sylverb/game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd)
as a Homebrew overlay.

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
| `battle/`, `field/`, `menu/`, `cutscene/`, `sound/` | 240 validated C translations of FF4 routines | No — pure 65816 logic, no hardware dependency |
| `dispatch_all.{c,h}` | Binary-search dispatch table; `ff4_dispatch_try` is called on every JSR/JSL | No — generic dispatch mechanism |
| `ff4_helpers.c` | `*_emu` stubs for routines not yet translated; the few already translated ones delegate to their `_c` body | Partially — `update_ctrl_field_emu` reconstructs G&W input; most stubs are generic weak no-ops |
| `main.c` | `ff4_init`, `ff4_step`, `ff4_blit_to_lcd` (BGR888→RGB565) | **Yes** — G&W LCD wiring |
| `snes/` | LakeSnes SNES core, G&W-patched: static allocation under `-DFF4_PORT_STATIC_SNES`, ROM read XIP from extflash, dispatch hook in `cpu.c` JSR/JSL cases | **Yes** — G&W patches behind `#ifdef FF4_PORT_STATIC_SNES` |
| `ff4_redefines` | `objcopy --redefine-syms` map prefixing 24 SNES-core globals with `ff4__` to avoid collisions with smw/zelda3 overlays | **Yes** — retro-go-sd coexistence |
| `gen_dispatch.py` | Regenerates `dispatch_all.{c,h}` from `<mod>/*.c` + ff4-port validation logs | Tool |

## Status

- Boots FF4 through the Square Enix splash to the title screen on real
  hardware (G&W Mario, 64 MB extflash mod, JTAG).
- **213 routines dispatched** — battle 92, field 114, menu 8, cutscene 20,
  sound 6 (some files contain multiple entry points).
- SPC700 mailbox handshake left to LakeSnes' real APU; `InitSound_ext` and
  `ExecSound_ext` are in the `gen_dispatch.py` skip list (see commit 17823b7).
- No audio output, no save-state loading yet.

## How to build and flash

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
`make clean` — keep a copy outside `sd_content/`.

`-DFF4_AUTOBOOT=1` activates the diagnostic harness in
`Core/Src/porting/ff4/main_ff4.c`: the device autoboots straight into FF4 and
emits five greppable UART markers (`FF4_DIAG_PPU`, `FF4_DIAG_APU`,
`FF4_DIAG_PCHIST`, `FF4_DIAG_MISS`, `FF4_DIAG_ALIVE`).

## Upstream

- LakeSnes: [elzo-d/LakeSnes](https://github.com/elzo-d/LakeSnes)
- 65816 disassembly: [everything8215/ff4](https://github.com/everything8215/ff4)
- Translation pipeline: [hcross/ff4-port](https://github.com/hcross/ff4-port)
- Firmware: [sylverb/game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd)
