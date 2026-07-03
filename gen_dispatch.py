#!/usr/bin/env python3
"""gen_dispatch — find dispatch candidates NOT yet in dispatch_all.c. Read-only.

HISTORY / WHY THIS NO LONGER WRITES dispatch_all.{h,c}: this script used to
regenerate both files from scratch. It emitted a binary-search dispatcher
and knew nothing of the ~200 lines of hand-maintained oracle machinery
(cycle charging, trace/filter/miss hooks, calibration, the FF4_AUTOBOOT
miss ring) or the per-entry EXCL/RETIRED reasoning and rich domain comments
that had accumulated in the committed file since. Running it for real
(2026-07-03, verified during the W0-1/W1-4 acceleration work) silently
destroyed all of that and dropped the entry count from 206 to 201 — the
exact landmine a Sonnet-class agent following this file's own former
header comment ("Re-run gen_dispatch.py whenever a translator run
finishes") would step on. dispatch_all.c is HAND-MAINTAINED; see its own
top-of-file comment and workflows/WF-DECOMP.md for how to add an entry.

What this script still does, safely: scan RUNS + the ff4-gnw module
directories for routines with a valid `void <Name>_c(Snes*)` signature and
a resolvable address, then diff that candidate set against the PCs already
present in the live dispatch_all.c. It reports what's NEW — a routine that
compiles and could be added — plus which `*_emu` helpers a candidate would
need that aren't defined anywhere yet. It never writes dispatch_all.{h,c}.

Usage:
    python gen_dispatch.py [--json]

Exit code: 0 if no new candidates, 1 if there are some (usable as a
"new work available" signal in a session bootstrap).
"""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

# ff4-gnw lives at <umbrella>/ff4-gnw, ff4-port at <umbrella>/ff4-port —
# siblings under the umbrella repo since the submodule move (BACKLOG.md §1).
# FF4_PORT_DIR overrides for a non-standard checkout layout.
THIS = Path(__file__).resolve().parent
FF4_PORT = Path(os.environ.get("FF4_PORT_DIR", str(THIS.parent / "ff4-port")))
BRIDGE   = FF4_PORT / "ca65-bridge/.venv/bin/ca65-bridge"
UPSTREAM = FF4_PORT / "upstream"
DISPATCH_ALL_C = THIS / "dispatch_all.c"

# Bank per module (session-restart-guide).
MODULES = {
    'battle':   0x03,
    'btlgfx':   0x02,
    'menu':     0x01,
    'field':    0x00,
    'sound':    0x04,
    'cutscene': 0x13,
}

# Routines we intentionally KEEP out of the C dispatch so LakeSnes runs the
# original 65816 asm via its interpreter instead. These are routines whose C
# wrappers only call a `*_emu()` helper, but that helper is a weak no-op stub
# in ff4_helpers.c (ADR-001 voie B). For routines whose side effects are
# entirely contained in the C body that is harmless, but for routines that
# DRIVE the SPC700 mailbox handshake (InitSound, ExecSound) the no-op breaks
# the audio CPU's boot sequence and the main CPU spins forever waiting for
# an ack that never comes (proved on hardware 2026-06-13 via the autoboot
# diagnostic harness — PC parked at 04:8232, out=AABB constant, inPorts
# never advances past 01).
#
# Until RunEmulatedFunc is wired on G&W, leave SPC-driving routines out of
# the dispatch so LakeSnes' real APU emulation runs them end-to-end.
SKIPPED_ROUTINES = {
    'InitSound_ext',
    'ExecSound_ext',
}

# Routines deliberately removed from the dispatch for reasons OTHER than the
# SPC handshake above — kept out of the "new candidate" report so it doesn't
# repeatedly re-surface known, intentional exclusions as if they were unseen
# work. See DISPATCH_REGISTRY.md's "Routines deliberately kept in the
# interpreter" note and the RETIRED row for the reasons.
DELIBERATELY_EXCLUDED = {
    'ExecBtlGfx':  'RETIRED — BLOCKING multi-frame animation (Wait* chain), hangs '
                    'run_emulated_func\'s synchronous single-frame execution',
    'ExecCmd':     'tail-jump via jml [$0080], not a JSR/JSL — cannot be dispatched',
    'TimerDur_0b': 'ROM bank $0F access, not ported to C',
    'TimerDur_03': 'ROM bank $0F access, not ported to C',
    'TimerDur_07': 'non-standard signature (Snes*, uint16_t x)',
    'Cmd_0f':      'do_magic_attack_emu is a no-op weak stub — damage silently swallowed',
    'Cmd_0e':      'do_magic_attack_emu is a no-op weak stub — damage silently swallowed',
    'Cmd_0c':      'do_multi_attack_emu is a no-op weak stub — damage silently swallowed',
    'Cmd_08':      'do_fight_cmd_emu is a no-op weak stub — damage silently swallowed',
    'Cmd_01':      'do_fight_cmd_emu is a no-op weak stub — damage silently swallowed',
}

# Validation files we know exist (extend as we re-run modules).
RUNS = {
    'battle':   FF4_PORT / 'translator/runs/qwen3_validation_after_retry.jsonl',
    'cutscene': FF4_PORT / 'translator/runs/cutscene_validation_after_retry.jsonl',
    'sound':    FF4_PORT / 'translator/runs/sound_validation.jsonl',
    'field':    FF4_PORT / 'translator/runs/field_validation_after_retry.jsonl',
}


def has_standard_sig(name, mod):
    f = THIS / mod / f"{name}.c"
    if not f.is_file():
        return False
    text = f.read_text()
    return bool(re.search(rf'^(?:static\s+)?(?:inline\s+)?void {re.escape(name)}_c\s*\(\s*Snes\s*\*\s*\w+\s*\)\s*\{{', text, re.MULTILINE))


_NAKED_ADDR_RE = re.compile(r'^_([0-9a-fA-F]{2})([0-9a-fA-F]{4})$')


def get_address(name):
    try:
        out = subprocess.check_output(
            [str(BRIDGE), '--root', str(UPSTREAM), 'get-asm', name],
            text=True, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        out = ''
    m = re.search(r'address_hint:\s*([0-9a-fA-F]+)', out)
    if m: return int(m.group(1), 16)
    # Fallback: routine named after its bank+offset (e.g. _15cadc → bank 15, addr CADC).
    # Used when the upstream asm has no symbolic label but the routine is
    # known by its hardware address (e.g. discovered via dispatch miss ring).
    m = _NAKED_ADDR_RE.match(name)
    if m: return int(m.group(2), 16)
    return None


# Segment → bank, parsed from ff4-en.lnk once.
_LNK_FILE = UPSTREAM / 'ff4-en.lnk'
_SEGMENT_BANK = {}
def _load_segment_banks():
    if _SEGMENT_BANK: return
    text = _LNK_FILE.read_text()
    for m in re.finditer(r'(\w+):\s*load\s*=\s*bank_([0-9a-fA-F]+)', text):
        _SEGMENT_BANK[m.group(1)] = int(m.group(2), 16)


# Cache routine → bank derived by walking each module's .asm tree.
_ROUTINE_BANK = {}
def _index_module_routines(mod):
    """Walk upstream/<mod>/*.asm; track current .segment as we encounter labels."""
    _load_segment_banks()
    mod_dir = UPSTREAM / mod
    if not mod_dir.is_dir(): return
    for asm in mod_dir.rglob('*.asm'):
        cur_seg = None
        for line in asm.read_text().splitlines():
            m = re.match(r'\s*\.segment\s+"([^"]+)"', line)
            if m:
                cur_seg = m.group(1)
                continue
            lbl = re.match(r'^([A-Za-z_][A-Za-z0-9_]*):', line)
            if lbl and cur_seg and cur_seg in _SEGMENT_BANK:
                _ROUTINE_BANK.setdefault(lbl.group(1), _SEGMENT_BANK[cur_seg])


def get_bank(name, fallback_bank):
    if name in _ROUTINE_BANK:
        return _ROUTINE_BANK[name]
    # Naked _<bank><addr> pattern: extract the bank from the name itself.
    m = _NAKED_ADDR_RE.match(name)
    if m: return int(m.group(1), 16)
    return fallback_bank


def collect_candidates():
    """Every routine with a valid signature and resolvable address, scanned
    from the validation JSONLs and the ff4-gnw module directories."""
    entries = []
    seen_names = set()
    for mod in MODULES:
        _index_module_routines(mod)
    for mod, bank in MODULES.items():
        run = RUNS.get(mod)
        if run is not None and run.is_file():
            for line in open(run):
                if not line.strip(): continue
                rec = json.loads(line)
                if rec.get('status') != 'pass':
                    continue
                name = Path(rec['file']).stem
                if name in seen_names or name in SKIPPED_ROUTINES: continue
                if not has_standard_sig(name, mod): continue
                addr = get_address(name)
                if addr is None: continue
                pc = (get_bank(name, bank) << 16) | addr
                entries.append((pc, name, mod))
                seen_names.add(name)
        mod_dir = THIS / mod
        if mod_dir.is_dir():
            for cfile in sorted(mod_dir.glob('*.c')):
                name = cfile.stem
                if name in seen_names or name in SKIPPED_ROUTINES: continue
                if not has_standard_sig(name, mod): continue
                addr = get_address(name)
                if addr is None: continue
                pc = (get_bank(name, bank) << 16) | addr
                entries.append((pc, name, mod))
                seen_names.add(name)
    entries.sort(key=lambda e: e[0])
    return entries


_LIVE_ENTRY_RE = re.compile(r"\{\s*0x([0-9A-Fa-f]{6})\s*,\s*(\w+)\s*\}")


def live_dispatch_table():
    """(pcs, fn_names) already present in the committed dispatch_all.c. None
    (with a warning) if the file can't be read — never treated as 'no
    entries exist', which would make every candidate look new."""
    if not DISPATCH_ALL_C.is_file():
        sys.stderr.write(f"warning: {DISPATCH_ALL_C} not found — cannot diff against "
                          "the live table\n")
        return None, None
    text = DISPATCH_ALL_C.read_text()
    matches = _LIVE_ENTRY_RE.findall(text)
    pcs = {int(pc, 16) for pc, _ in matches}
    # Function names, not routine names: strip the _c suffix to match `name`
    # from collect_candidates().
    fn_names = {fn[:-2] if fn.endswith('_c') else fn for _, fn in matches}
    return pcs, fn_names


def needed_emu_for(name, mod, existing_emu):
    cfile = THIS / mod / f"{name}.c"
    if not cfile.is_file():
        return []
    text = cfile.read_text()
    no_block = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    no_line = re.sub(r"//[^\n]*", "", no_block)
    no_str = re.sub(r'"[^"\n]*"', '""', no_line)
    out = []
    for m in re.finditer(r"\b(\w+_emu)\s*\(", no_str):
        emu = m.group(1)
        if emu == f"{name}_emu" or emu in existing_emu or emu in out:
            continue
        out.append(emu)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args(argv)

    candidates = collect_candidates()
    live_pcs, live_names = live_dispatch_table()
    if live_pcs is None:
        return 2

    # A candidate whose NAME is already dispatched (just under a PC this
    # script's own bank-fallback heuristic guessed wrong — segment lookup in
    # upstream/*.asm can miss a routine, e.g. one logically domain-tagged
    # "btlgfx" but physically filed under ff4-gnw/battle/) is not new work;
    # it is a heuristic mismatch worth noting separately, never a silent
    # false "new candidate".
    bank_mismatches = [(pc, n, m) for pc, n, m in candidates
                       if pc not in live_pcs and n in live_names]
    new_candidates = [(pc, n, m) for pc, n, m in candidates
                      if pc not in live_pcs and n not in live_names
                      and n not in DELIBERATELY_EXCLUDED]
    excluded_hits = [(n, m) for pc, n, m in candidates
                     if pc not in live_pcs and n not in live_names
                     and n in DELIBERATELY_EXCLUDED]

    existing_emu = set()
    helpers_c = THIS / 'ff4_helpers.c'
    if helpers_c.is_file():
        for line in helpers_c.read_text().splitlines():
            m = re.search(r'\bvoid\s+(\w+_emu)\s*\(', line)
            if m: existing_emu.add(m.group(1))

    report = []
    for pc, name, mod in new_candidates:
        report.append({
            "pc": f"{pc:06X}",
            "name": name,
            "module": mod,
            "needed_emu": needed_emu_for(name, mod, existing_emu),
        })

    if args.json:
        print(json.dumps({"candidates": len(candidates), "live": len(live_pcs),
                           "new": report,
                           "deliberately_excluded_hits": [
                               {"name": n, "module": m, "reason": DELIBERATELY_EXCLUDED[n]}
                               for n, m in excluded_hits],
                           "bank_heuristic_mismatches": [
                               {"guessed_pc": f"{pc:06X}", "name": n, "module": m}
                               for pc, n, m in bank_mismatches]},
                          indent=2))
    else:
        print(f"{len(candidates)} candidate routines scanned, {len(live_pcs)} already "
              f"in dispatch_all.c, {len(new_candidates)} new "
              f"({len(excluded_hits)} deliberately-excluded, "
              f"{len(bank_mismatches)} bank-heuristic-mismatch — not shown as new)")
        for r in report:
            emu_note = f" (needs: {', '.join(r['needed_emu'])})" if r["needed_emu"] else ""
            print(f"  ${r['pc']}  {r['name']}  [{r['module']}]{emu_note}")
        if bank_mismatches:
            print("Bank-heuristic mismatches (already dispatched under a different "
                  "bank than this script's segment lookup guessed — not new work, "
                  "informational only):")
            for pc, n, m in bank_mismatches:
                print(f"  guessed ${pc:06X}  {n}  [{m}]")
        if not new_candidates:
            print("Nothing new. To add a routine, follow workflows/WF-DECOMP.md and "
                  "hand-edit dispatch_all.{h,c} — this script never writes them.")

    return 1 if new_candidates else 0


if __name__ == "__main__":
    sys.exit(main())
