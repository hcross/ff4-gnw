# How a validated routine ends up running on the device

> If "dispatch" is an unfamiliar term, read the umbrella repo's glossary
> and mechanism definition first:
> [`ff4/docs/primer/00-glossary.md`](https://github.com/hcross/ff4/blob/main/docs/primer/00-glossary.md)
> and [`AGENTS.md` §A.2](https://github.com/hcross/ff4/blob/main/AGENTS.md).
> This page covers only what's specific to *this* repository's side of
> the mechanism.

## What lands here, and when

A C routine only reaches this repository once it has been proven
equivalent to the original assembly in [`ff4-port`](https://github.com/hcross/ff4-port)
— see that repo's
[`docs/workflow/translation-cascade.md`](https://github.com/hcross/ff4-port/blob/main/docs/workflow/translation-cascade.md)
and
[`docs/workflow/validation-oracle.md`](https://github.com/hcross/ff4-port/blob/main/docs/workflow/validation-oracle.md).
Nothing here is a generation candidate or an unreviewed draft — that
distinction is the whole reason this project is split across two
repositories in the first place (see the main [README](../README.md)'s
"Role in the ecosystem").

## The dispatch table, concretely

`dispatch_all.c` is a table mapping original SNES addresses to the C
functions that replace them. At runtime, every `JSR`/`JSL` the interpreter
is about to execute is checked against this table first
(`ff4_dispatch_try`, hooked into LakeSnes's `cpu.c`): if the target address
has a validated C function, that function runs instead of interpreting the
original assembly; otherwise, the interpreter proceeds exactly as if
dispatch didn't exist. This is why the port can be *partially* complete
and still fully playable — routines without a C replacement simply run
the slow way, correctly.

**A limit worth knowing early**: `JML` (a different jump instruction,
opcode `$DC`) is deliberately *not* intercepted — see
[`AGENTS.md`'s "Known dispatch limits"](https://github.com/hcross/ff4/blob/main/AGENTS.md)
for why, and why some routines that end in a tail jump must stay
interpreted for exactly that reason.

## Variant ROMs — the per-slot gate array

The dispatch table is an equivalence proof against **one** ROM image
(vanilla JP 1.1). When a known translation-patch variant is loaded
instead, `rom_ident.c` CRCs the image once at `ff4_init` and arms
`ff4_dispatch_gate[]`: a gated slot falls through to the interpreter —
which reads the patched bytes and is therefore always correct — and is
counted in `ff4_dispatch_gated`, separately from the miss counters. The
per-variant profiles (`rom_profiles.{c,h}`) are **generated** by the
umbrella repo's `registry/patch_impact.py`; never hand-edit them. For an
unknown image, the device build refuses to boot
(`FF4_REQUIRE_KNOWN_ROM`) and the desktop harness warns and runs
interpreter-only. Full decision record:
[ADR-008 in `ff4-port`](https://github.com/hcross/ff4-port/blob/main/docs/adr/adr-008-translation-patches-crc-profiles.md).

## Regenerating the table

`gen_dispatch.py` used to regenerate `dispatch_all.{c,h}` automatically
from `<module>/*.c` files. That is no longer how it works: the file
carries hand-written, per-entry comments and oracle-specific tooling that
an automatic regeneration would destroy, so `gen_dispatch.py` now runs in
**read-only report mode** — it tells you which validated routines aren't
yet dispatched, it doesn't write anything. Adding a routine to the
dispatch table is a manual, reviewed edit.

## Where the routines themselves live

`battle/`, `field/`, `menu/`, `cutscene/`, `sound/` — one C file per
ported routine (or small group), organized by which part of the original
game they belong to. `ff4_helpers.c` holds the `_emu` delegation stubs
(see [ADR-003 in `ff4-port`](https://github.com/hcross/ff4-port/blob/main/docs/adr/adr-003-classification.md))
for routines not yet translated, plus a few genuinely
Game & Watch-specific reimplementations (like input handling) that had no
direct equivalent to translate.

## Going further

For the mechanism's full definition and its documented limits, see
[`AGENTS.md` §A.2](https://github.com/hcross/ff4/blob/main/AGENTS.md) in
the umbrella repo — this page deliberately does not restate it, to avoid
the two copies drifting apart.
