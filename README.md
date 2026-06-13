# ff4-gnw — Final Fantasy IV native C port for Game & Watch

A LakeSnes-based native-C port of Final Fantasy IV for the Nintendo
Game & Watch (STM32H7B0VBT6), wired into the
[sylverb/game-and-watch-retro-go-sd](https://github.com/sylverb/game-and-watch-retro-go-sd)
firmware as a Homebrew overlay.

## What's in the box

| Directory          | Contents                                                            |
|--------------------|---------------------------------------------------------------------|
| `snes/`            | LakeSnes SNES core, patched for G&W. `Snes` / `Ppu` / `Cart` structures are statically allocated under `-DFF4_PORT_STATIC_SNES` to fit the 85 KB MCU heap; the ROM is read XIP from extflash instead of being duplicated. The dispatch hook lives in the JSR (0x20) / JSL (0x22) opcode cases of `cpu.c`, guarded by `-DFF4_PORT_STATIC_SNES`. |
| `battle/`, `field/`, `menu/`, `cutscene/`, `sound/` | 168 native-C translations of FF4 module routines, produced by the [hcross/ff4-port](https://github.com/hcross/ff4-port) pipeline (cascade gemma4 ↦ gpt-oss critic ↦ deepseek-v4-pro, with verbatim-error multi-turn refinement and a 100-trial fuzz parity oracle). |
| `main.c`           | Port entry: `ff4_init`, `ff4_step`, `ff4_blit_to_lcd` (BGR888→RGB565), `ff4_get_state`, `ff4_shutdown`. Pixel-buffer reader writes `snes->ppu->pixelBuffer` directly. |
| `dispatch_all.{c,h}` | Auto-generated dispatch table (`ff4_dispatch_try`, `ff4_dispatch_hits`, `ff4_dispatch_misses`, `ff4_miss_per_bank[256]`). Sorted binary search; weak no-op stubs auto-emitted for any `*_emu()` helper a dispatched body calls but that's not yet defined in `ff4_helpers.c`. |
| `ff4_helpers.c`    | Manual + auto-stubbed `*_emu` helpers. Each is a `__attribute__((weak))` no-op until a real implementation (e.g. a future G&W `RunEmulatedFunc`) overrides it. |
| `gen_dispatch.py`  | Walks `<mod>/*.c` plus the `ff4-port/translator/runs/*_validation*.jsonl` PASS sets, parses the ld65 link config to derive each routine's bank, sorts by PC, regenerates `dispatch_all.{c,h}`, and appends weak `*_emu` auto-stubs for any new symbol referenced by a dispatched body. |
| `ff4_redefines`    | `objcopy --redefine-syms` mapping prefixing the 24 SNES-core globals (`apu_*`, `cart_*`, `cpu_*`, `ppu_*`, `snes_*`, `spc_*`) with `ff4__` to avoid colliding with `external/smw/` and `external/zelda3/` when those overlays coexist in the retro-go binary. |

## Status

- Boots FF4 through the Square Enix splash to the title screen on
  real hardware (G&W Mario via JTAG, 64 MB extflash mod).
- **168 routines dispatched.** 26 % dispatch hit rate at host frame
  175 (`dispatch=142/546`). Each native-C body eliminates its
  sub-JSRs from the miss denominator, so adding a routine on the hot
  path mechanically lifts the ratio.
- SPC700 mailbox handshake is left to LakeSnes' real APU emulator;
  `InitSound_ext` and `ExecSound_ext` are in the `gen_dispatch.py`
  skip list to keep the bank-0x40 handshake intact (see commit
  17823b7).
- Wall-clock ~5 fps in the boot/title path (mostly interpreter time
  on the un-translated routines; native-C bodies take roughly the
  same wall-clock as their interpreted equivalent on this MCU but
  reduce the dispatch miss count, which is the proxy metric the
  pipeline tracks).
- No audio output yet, no save-state loading yet. Both are on the
  ff4-port roadmap.

## How to use

Cloned as `external/ff4` of `hcross/game-and-watch-retro-go-sd`
(branch `feat/ff4-port-scaffold`). Build with `SD_CARD=0` (no SD
card mod required) and `EXTFLASH_SIZE_MB=4`:

```bash
make flash SD_CARD=0 FF4_AUTOBOOT=1 EXTFLASH_SIZE_MB=4 -j8
gnwmanager monitor   # observe `FF4 live: host=... dispatch=H/T`
```

`-DFF4_AUTOBOOT=1` activates a built-in diagnostic harness in
`Core/Src/porting/ff4/main_ff4.c`: the device autoboots straight
into FF4 (skips the retro-go launcher) and emits five greppable
diagnostic markers on the serial UART (`FF4_DIAG_PPU`,
`FF4_DIAG_APU`, `FF4_DIAG_PCHIST`, `FF4_DIAG_MISS`,
`FF4_DIAG_ALIVE`) so downstream tooling can assert on boot health
and hit-rate without any human in the loop.

The user provides their FF4 JP 1.1 ROM (CRC32 `CAA15E97`) at
`sd_content/roms/homebrew/ff4.sfc`.

## Upstream

LakeSnes is from [elzo-d/LakeSnes](https://github.com/elzo-d/LakeSnes).
The translations come from [hcross/ff4-port](https://github.com/hcross/ff4-port).

The G&W-specific patches inside `snes/` live behind
`#ifdef FF4_PORT_STATIC_SNES` so the desktop build path of LakeSnes
stays untouched and the diff against upstream stays surgical.
