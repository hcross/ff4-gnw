# ff4-gnw — Final Fantasy IV native C port for Game & Watch

A LakeSnes-based native-C port of Final Fantasy IV for the Nintendo
Game & Watch (STM32H7B0VBT6), wired into the
[sylverb/game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd)
firmware as a Homebrew overlay.

## What's in the box

| Directory     | Contents                                                            |
|---------------|---------------------------------------------------------------------|
| `snes/`       | LakeSnes SNES core, patched for G&W. Snes/Ppu/cart structures are statically allocated under `-DFF4_PORT_STATIC_SNES` to fit the 85 KB MCU heap; the ROM is read XIP from extflash instead of being duplicated. |
| `battle/`     | 88 native-C translations of the FF4 `battle` module routines, produced by [hcross/ff4-port](https://github.com/hcross/ff4-port) (qwen3-coder via Ollama Cloud, parity-validated against the original ROM). |
| `main.c`      | Port entry: `ff4_init`, `ff4_step`, `ff4_blit_to_lcd` (BGR888→RGB565), `ff4_get_state`, `ff4_shutdown`. |
| `ff4_redefines` | `objcopy --redefine-syms` mapping that prefixes the 24 SNES-core globals (`apu_*`, `cart_*`, `cpu_*`, `ppu_*`, `snes_*`, `spc_*`) with `ff4__` to avoid colliding with `external/smw/` when both overlays coexist in the retro-go binary. |

## Status

- Boots Final Fantasy IV through the Square Enix splash to the title
  screen on real hardware (G&W Mario via JTAG, 64 MB extflash mod).
- ~3–12 FPS on the pure 65816 interpreter, no audio yet, no save state.
- The 88 translated routines are linked into the overlay but **not yet
  called** — the dispatch hook from `cpu_runOpcode` to the C body is
  the next phase.

## How to use

Cloned as `external/ff4` of `sylverb/game-and-watch-retro-go-sd`
through the FF4 scaffold PR:
[sylverb/game-and-watch-retro-go-sd#82](https://github.com/sylverb/game-and-watch-retro-go-sd/pull/82).

The user provides their FF4 ROM at
`sd_content/roms/homebrew/ff4.sfc` and builds with
`SD_CARD=0 INTFLASH_BANK=1 EXTFLASH_SIZE_MB=64`.

## Upstream

LakeSnes is from [elzo-d/LakeSnes](https://github.com/elzo-d/LakeSnes).
The translations come from [hcross/ff4-port](https://github.com/hcross/ff4-port).

The G&W-specific patches inside `snes/` live behind
`#ifdef FF4_PORT_STATIC_SNES` so the desktop build path of LakeSnes
stays untouched and the diff against upstream stays surgical.
