#!/usr/bin/env python3
"""Regenerate dispatch_all.{h,c} from hcross/ff4-port translator runs.

Usage: python gen_dispatch.py
"""
import json
import re
import subprocess
from pathlib import Path

FF4_PORT = Path.home() / "devel/perso/retrogaming/ff4-port"
BRIDGE   = FF4_PORT / "ca65-bridge/.venv/bin/ca65-bridge"
UPSTREAM = FF4_PORT / "upstream"

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

# Validation files we know exist (extend as we re-run modules).
RUNS = {
    'battle':   FF4_PORT / 'translator/runs/qwen3_validation_after_retry.jsonl',
    'cutscene': FF4_PORT / 'translator/runs/cutscene_validation_after_retry.jsonl',
    'sound':    FF4_PORT / 'translator/runs/sound_validation.jsonl',
    'field':    FF4_PORT / 'translator/runs/field_validation_after_retry.jsonl',
}

def has_standard_sig(name, mod):
    f = Path(mod) / f"{name}.c"
    if not f.is_file():
        return False
    text = f.read_text()
    return bool(re.search(rf'^(?:static\s+)?(?:inline\s+)?void {re.escape(name)}_c\s*\(\s*Snes\s*\*\s*\w+\s*\)\s*\{{', text, re.MULTILINE))

def get_address(name):
    try:
        out = subprocess.check_output(
            [str(BRIDGE), '--root', str(UPSTREAM), 'get-asm', name],
            text=True, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        return None
    m = re.search(r'address_hint:\s*([0-9a-fA-F]+)', out)
    return int(m.group(1), 16) if m else None


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
    if not _ROUTINE_BANK: pass
    return _ROUTINE_BANK.get(name, fallback_bank)

entries = []
seen_names = set()
for mod in MODULES:
    _index_module_routines(mod)
for mod, bank in MODULES.items():
    # 1. Read PASS routines from the validation JSONL (legacy path).
    run = RUNS.get(mod)
    if run is not None and run.is_file():
        for line in open(run):
            if not line.strip(): continue
            rec = json.loads(line)
            if rec.get('status') != 'pass':
                continue
            name = Path(rec['file']).stem
            if name in seen_names: continue
            if name in SKIPPED_ROUTINES: continue
            if not has_standard_sig(name, mod):
                continue
            addr = get_address(name)
            if addr is None:
                continue
            pc = (get_bank(name, bank) << 16) | addr
            entries.append((pc, name, mod))
            seen_names.add(name)
    # 2. Scan ff4-gnw/<mod>/*.c — include anything with a valid signature.
    mod_dir = Path(mod)
    if mod_dir.is_dir():
        for cfile in sorted(mod_dir.glob('*.c')):
            name = cfile.stem
            if name in seen_names: continue
            if name in SKIPPED_ROUTINES: continue
            if not has_standard_sig(name, mod):
                continue
            addr = get_address(name)
            if addr is None:
                continue
            pc = (get_bank(name, bank) << 16) | addr
            entries.append((pc, name, mod))
            seen_names.add(name)

entries.sort(key=lambda e: e[0])
print(f'{len(entries)} dispatch entries across modules')

with open('dispatch_all.h', 'w') as h:
    h.write('/* Auto-generated dispatch table for FF4 native-C bodies.\n')
    h.write(' * Re-run gen_dispatch.py whenever a translator run finishes. */\n')
    h.write('#pragma once\n#include <stdint.h>\n#include "snes/snes.h"\n\n')
    for _, n, _ in entries:
        h.write(f'void {n}_c(Snes *snes);\n')
    h.write(f'\ntypedef struct {{ uint32_t pc; void (*fn)(Snes *snes); }} ff4_dispatch_entry_t;\n')
    h.write(f'#define FF4_DISPATCH_COUNT {len(entries)}\n')
    h.write('extern const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT];\n')
    h.write('int ff4_dispatch_try(Snes *snes, uint32_t pc);\n')
    h.write('extern uint32_t ff4_dispatch_hits;\n')
    h.write('extern uint32_t ff4_dispatch_misses;\n')

with open('dispatch_all.c', 'w') as c:
    c.write('#include "dispatch_all.h"\n\n')
    c.write('const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT] = {\n')
    for pc, n, mod in entries:
        c.write(f'    {{ 0x{pc:06x}, {n}_c }},  /* {mod} */\n')
    c.write('};\n\n')
    c.write('uint32_t ff4_dispatch_hits = 0;\n')
    c.write('uint32_t ff4_dispatch_misses = 0;\n')
    c.write('uint32_t ff4_miss_per_bank[256] = {0};\n\n')
    c.write('#ifdef FF4_AUTOBOOT\n')
    c.write('uint32_t g_diag_miss_ring[8] = {0};\n')
    c.write('static uint8_t g_diag_miss_ring_head = 0;\n')
    c.write('#endif\n\n')
    c.write('int ff4_dispatch_try(Snes *snes, uint32_t pc) {\n')
    c.write('    int lo = 0, hi = FF4_DISPATCH_COUNT - 1;\n')
    c.write('    while (lo <= hi) {\n')
    c.write('        int mid = (lo + hi) >> 1;\n')
    c.write('        uint32_t e = ff4_dispatch_table[mid].pc;\n')
    c.write('        if (e == pc) {\n')
    c.write('            ff4_dispatch_hits++;\n')
    c.write('            ff4_dispatch_table[mid].fn(snes);\n')
    c.write('            return 1;\n')
    c.write('        }\n')
    c.write('        if (e < pc) lo = mid + 1; else hi = mid - 1;\n')
    c.write('    }\n')
    c.write('    ff4_dispatch_misses++;\n')
    c.write('    ff4_miss_per_bank[(pc >> 16) & 0xff]++;\n')
    c.write('#ifdef FF4_AUTOBOOT\n')
    c.write('    {   /* record unique miss PCs in a small ring (dedup against existing) */\n')
    c.write('        int already = 0;\n')
    c.write('        for (int i = 0; i < 8; i++) {\n')
    c.write('            if (g_diag_miss_ring[i] == pc) { already = 1; break; }\n')
    c.write('        }\n')
    c.write('        if (!already) {\n')
    c.write('            g_diag_miss_ring[g_diag_miss_ring_head] = pc;\n')
    c.write('            g_diag_miss_ring_head = (g_diag_miss_ring_head + 1) & 7;\n')
    c.write('        }\n')
    c.write('    }\n')
    c.write('#endif\n')
    c.write('    return 0;\n')
    c.write('}\n')

by_mod = {}
for _, _, m in entries:
    by_mod[m] = by_mod.get(m, 0) + 1
for m, n in sorted(by_mod.items()):
    print(f'  {m}: {n}')


# ---------------------------------------------------------------------
# Auto-stubs for *_emu helpers
# ---------------------------------------------------------------------
# Each newly-dispatched routine often calls 1..N *_emu() helpers (the
# delegate pattern, or sub-routines inside a translated body). When the
# *_emu is not yet declared in ff4_helpers.c, the build breaks with
# "undefined reference". To keep iterations autonomous, scan every
# dispatched .c, diff against the existing helpers, and emit the
# missing ones as weak no-op stubs at the tail of dispatch_all.c.
#
# A stronger definition (e.g. a real RunEmulatedFunc-backed delegate or
# a future native-C reimplementation) can later override these weak
# stubs without any change to this generator.

import collections

existing_emu = set()
try:
    with open('ff4_helpers.c') as f:
        for line in f:
            m = re.search(r'\bvoid\s+(\w+_emu)\s*\(', line)
            if m: existing_emu.add(m.group(1))
except FileNotFoundError:
    pass

needed_emu = collections.OrderedDict()  # name -> first <mod>/<routine> needing it
for pc, n, mod in entries:
    cfile = Path(mod) / f"{n}.c"
    if not cfile.is_file():
        continue
    text = cfile.read_text()
    # Strip C comments so commented-out *_emu mentions don't trigger stubs.
    no_block = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    no_line  = re.sub(r"//[^\n]*", "", no_block)
    # Also drop string literals (rare but cheap insurance).
    no_str   = re.sub(r'"[^"\n]*"', '""', no_line)
    for m in re.finditer(r"\b(\w+_emu)\s*\(", no_str):
        emu = m.group(1)
        # Skip the function being defined itself, in case the body
        # happens to contain a self-reference inside a defensive guard.
        if emu == f"{n}_emu":
            continue
        if emu in existing_emu:
            continue
        if emu not in needed_emu:
            needed_emu[emu] = f"{mod}/{n}.c"

if needed_emu:
    with open('dispatch_all.c', 'a') as c:
        c.write('\n\n/* ---------------------------------------------------------\n')
        c.write(' * Auto-stubs (generated by gen_dispatch.py)\n')
        c.write(' *\n')
        c.write(' * Weak no-op definitions for *_emu helpers referenced by\n')
        c.write(' * the dispatched .c files above but not yet declared in\n')
        c.write(' * ff4_helpers.c. A stronger definition elsewhere (e.g. a\n')
        c.write(' * real RunEmulatedFunc-backed delegate) will override the\n')
        c.write(' * weak symbol automatically.\n')
        c.write(' * --------------------------------------------------------- */\n\n')
        for emu, used_by in needed_emu.items():
            c.write(f'__attribute__((weak)) void {emu}(Snes *snes) '
                    f'{{ (void)snes; }}  /* first needed by {used_by} */\n')
    print(f'  auto-stubs: {len(needed_emu)} weak *_emu definitions emitted')
else:
    print('  auto-stubs: none needed (all *_emu helpers already in ff4_helpers.c)')
