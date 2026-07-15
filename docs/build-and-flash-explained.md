# What the build & flash commands are actually doing

> The [README](../README.md) keeps the exact commands to copy-paste; this
> page explains what each flag means and why it's there. Background on
> the target hardware: [`firmware-and-hardware.md`](firmware-and-hardware.md).

The build/flash sequence from the README:

```bash
make INTFLASH_BANK=2 FF4_AUTOBOOT=1 SD_CARD=0 EXTFLASH_SIZE_MB=4 \
     CHECK_DIRTY_SUBMODULE=0 -j4 all
make INTFLASH_BANK=2 FF4_AUTOBOOT=1 SD_CARD=0 EXTFLASH_SIZE_MB=4 \
     CHECK_DIRTY_SUBMODULE=0 flash
```

## Why each flag is there

- **`INTFLASH_BANK=2`** — the device's internal flash is split into two
  banks so a bootloader can live in one (`0x08000000`) while the
  application lives in the other (`0x08100000`). This flag tells the build
  to link the app for bank 2, alongside the existing bootloader rather
  than replacing it — the same pattern used by the other homebrew cores
  sharing this firmware.
- **`SD_CARD=0`** — this device supports loading ROMs from an SD card, but
  this project instead bakes the ROM directly into the firmware's
  filesystem image (FrogFS) at build time, so no SD card modification is
  needed to test. Setting `SD_CARD=1` switches to the SD-card path, in
  which case the `flash` step becomes a no-op (there's nothing new to
  flash — the ROM would be read from the card at runtime instead).
- **`EXTFLASH_SIZE_MB=4`** — declares how much external flash is available
  on the specific device unit being targeted, so the build can lay out
  the firmware image correctly. This must match the actual hardware
  modification present on the device (see the README's mention of a
  "64 MB extflash mod" for the units this project has been validated on)
  — a mismatch here doesn't fail loudly, it produces a broken image.
  `4` is enough for the single vanilla-ROM build; the **dual-language**
  build (vanilla + the 2 MiB J2e variant image both baked into FrogFS —
  a 3.65 MB FrogFS image as bench-measured on 2026-07-15, and the
  bench-validated translation-variant configuration) needs
  `EXTFLASH_SIZE_MB=8`: at `4` the FrogFS reserve overflows and the
  build fails hard. To keep a 4 MB build while variant
  images sit in `sd_content/`, exclude them from baking via
  `sd_content/.frogfsignore` (one fnmatch pattern per line, relative to
  `roms/`; the scaffold ships the J2e exclusion as a commented example
  — scaffold commit `39b08f34`).
- **`CHECK_DIRTY_SUBMODULE=0`** — the retro-go-sd build normally refuses to
  build if any of its submodules have uncommitted changes, as a safety
  check for its own release process. This project's development flow
  regularly has local changes in flight, so the check is disabled here.
- **`FF4_AUTOBOOT=1`** — activates a diagnostic mode
  (`Core/Src/porting/ff4/main_ff4.c`) that boots straight into FF4 instead
  of the firmware's normal game-selection menu, and emits UART markers
  (`FF4_DIAG_PPU`, `FF4_DIAG_APU`, `FF4_DIAG_PCHIST`, `FF4_DIAG_MISS`,
  `FF4_DIAG_ALIVE`) that can be grepped from a serial monitor — useful for
  automated or remote testing where clicking through a menu isn't
  practical.

## Why a power cycle is needed after flashing

`make flash` ends by starting the newly-flashed firmware directly and
disabling the debugger, which **bypasses the bootloader's normal startup
path**. To confirm the firmware actually boots the way it will for a real
user (via the bootloader, not via a debug tool forcing it to start), the
device needs a full power cycle after flashing — a debug-triggered start
and a real cold boot are not proven to be the same thing until you've
checked both.

## Continuing from here

Once flashed, `gnwmanager monitor` lets you observe the diagnostic markers
live. For deeper on-device debugging (crash decoding, fault registers,
GDB), see the `gnw-hardware:debug` tooling referenced in the umbrella
[`AGENTS.md`](https://github.com/hcross/ff4/blob/main/AGENTS.md)'s tools
section, and the umbrella's
[`workflows/WF-RELEASE.md`](https://github.com/hcross/ff4/blob/main/workflows/WF-RELEASE.md)
for the full release/test procedure.
