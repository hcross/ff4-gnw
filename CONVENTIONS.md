# ff4-gnw — verified hardware-state conventions

Facts about the 65816 register state each module runs under, gathered from
CONTRACT `entry_mode` lines, fix commits, and MemPalace obstacle drawers —
**not** a restatement of what the translator assumed, which has been wrong
more than once (see the DP section below). Read this before trusting a
routine's CONTRACT `dp=`/`db=` fields at face value.

## Data Bank (DB) — selects which 64 KB bank `sta $nnnn` (absolute) targets

| Module | DB convention | Confidence | Source |
|--------|--------------|------------|--------|
| `battle` | `$7E` (WRAM) | established | Pitfall 1 (`prompts/reverser_system.md`); 0 counter-examples in ~93 battle CONTRACTs |
| `field`, `cutscene`, `sound` | presumed `$7E` ("a priori OK") for most routines, but **NOT individually verified** | low-medium | MemPalace `wing=ff4-gnw room=obstacles-and-solutions`, "[CLASSE DE BUG] MMIO écrit en WRAM" (2026-06-29): explicitly lists `_13ddd6, _13eb60, _13ebb8, ExecInterrupt, InitCharRows, PlayGameSfx, PlaySystemSfx` as "DB=$7E (a priori OK, non corrigés)" — an *assumption*, not a confirmed fact |

`registry/classify_flags.py`'s `DMA_TRIGGER`/`SPC_MAILBOX` flags mark routines
that touch these address ranges in their asm — a non-`false` flag on a
routine outside `battle` means its DB assumption has not been individually
verified. `D04861E ExecInterrupt_c` was demoted L2→L1 in this pass on
exactly this ground (its own stated purpose — driving the SPC700 mailbox —
contradicts a DB=$7E reading of `sta hAPUIO0`, which would silently write
WRAM instead; not yet oracle-confirmed).

## Direct Page (D) — where `sta $nn` (single-byte operand) actually lands: `$(D+nn)`, never `$00nn`

**This is a distinct register from DB and a distinct bug class** (Pitfall
14 in `prompts/reverser_system.md`, and the `snes-re` plugin's own
"pitfall #1" — two different documents number their #1 pitfall for two
different registers; do not conflate them).

| Module | D value | Confidence | Source |
|--------|---------|------------|--------|
| `battle` | `$0000` | established | Universal CONTRACT consensus: 0/93 battle files declare a non-zero `dp=`; 3/93 use dynamic `cpu->dp` addressing (no counter-evidence found) |
| `field` | **NOT uniform — verify per routine** | mixed, evidenced | `D=$0600` confirmed at runtime for the NMI/FieldMain-loop context (AGENTS.md golden rule 1; `CheckMenu_c` fix, printf-verified `dp=0600`). But 98/114 field CONTRACTs still declare `dp=0x0` and do not use `cpu->dp` — some of those are plausibly genuine D=0 contexts (e.g. boot-time loaders called before FieldMain sets D), others may be the SAME uncaught bug class as `CheckMenu_c`/`_00883d_c`/`_00885e_c` (all three explicitly document "the CONTRACT's `dp=0x0` was a wrong RE assumption" after being fixed) |
| `menu` (the interpreted/delegated ReadCtrl-UpdateCtrl logic, NOT the 8 dispatched wrappers below) | **`$0100`**, confirmed via the linker (`ff4-en.lnk` `menu_dp` symbol) | high, but not a dispatch-layer bug | MemPalace obstacle drawer "[CHANTIER INPUT/MENU] Diagnostic complet + spec ReadCtrl" (2026-06-30): "menu_dp = $0100 (linker ff4-en.lnk)". Relevant to the `update_ctrl_emu`/`update_ctrl_field_emu` stub reimplementation (see below), not to the dispatched `menu` module files |
| `cutscene`, `sound` | unverified | low | 1/20 (cutscene) and 0/6 (sound) files use dynamic `cpu->dp`; no CONTRACT in either module declares a non-zero `dp=`. No specific evidence either way — flagged here as an open question, not investigated in this pass |

`registry/classify_flags.py`'s `DP_SENSITIVE` flag mechanically surfaces
`field`/`menu` routines that read OR write low WRAM (`ram[N]`/`read16`/
`write16(ram, N, ...)` for `N < 0x1000`) without ever computing the
address relative to `cpu->dp`. **This is a "verify before trusting"
proxy, not a confirmed bug list** — some flagged routines may genuinely
run at D=0. Priority: the ~110 flagged `field` files not already fixed.

**Corrected 2026-07-03 (self-caught overstatement — read this before citing
the menu row above from memory):** an earlier pass of this document
claimed "0/8 menu-module files use dynamic `cpu->dp`, all declare
`dp=0x0` — same shape as the confirmed `CheckMenu_c` bug" and treated it
as a likely contributing cause of the input/menu chantier. Verified
directly: **all 8 dispatched `menu` files are thin one-line redirectors**
(`void X_ext_c(Snes *snes) { x_emu(snes); }`) with **zero** `ram[]`/
`read16`/`write16` calls of their own — `dp=0x0` is harmless for them
because they never touch memory; there is no DP-addressing bug possible
in a routine that reads and writes nothing. `registry/classify_flags.py`
confirms this: `DP_SENSITIVE` never fires for any of the 8, even after
fixing the detector to also catch read-only access (see the tool's own
commit history). The real `menu_dp=$0100` logic lives one layer down, in
the routines these wrappers delegate to (`update_ctrl_emu`,
`update_ctrl_field_emu`, ...) — currently simplified stubs (no edge
detection/auto-repeat, per the ReadCtrl spec drawer), not DP-broken
dispatched C. **Lesson for future sessions: an aggregate CONTRACT pattern
(N/M files share a trait) is not evidence of a bug until you check
whether the flagged files actually touch the memory in question at all.**

## Do not repeat these mistakes

- Never declare `dp=0x0` in a CONTRACT because it's the default — state the
  entry D you actually traced from the caller, or mark it explicitly
  unverified.
- A CONTRACT's `dp=`/`db=` fields are **not proof** of the real entry
  state — they are the translator's *claim*, and that claim has been wrong
  for at least 3 confirmed routines (`CheckMenu_c`, `_00883d_c`,
  `_00885e_c`). Check whether a flagged routine actually touches memory
  before assuming a DP bug — see the corrected `menu` finding above.
- `generate_spike.py` now runs both the asm and the C side at the
  CONTRACT's declared `entry_mode: dp=`/`db=` (previously hardcoded to
  `dp=0`/`db=0x7E` regardless of the CONTRACT — the structural blindness
  that let `CheckMenu_c` pass its spike while reading the wrong address).
  This closes the harness gap, but it is **not a substitute for verifying
  the declared `dp=` itself is correct**: a routine whose CONTRACT still
  wrongly claims `dp=0x0` when it isn't (some fraction of the ~110
  `DP_SENSITIVE`-flagged `field` files) now gets tested at that wrong
  value on BOTH sides — consistently wrong, so the spike still passes.
  The fix only pays off once the declared entry state is fixed to match
  reality; verifying that per routine remains open work.
