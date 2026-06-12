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
    return bool(re.search(rf'^void {re.escape(name)}_c\(Snes\s*\*\s*snes\)\s*\{{', text, re.MULTILINE))

def get_address(name):
    try:
        out = subprocess.check_output(
            [str(BRIDGE), '--root', str(UPSTREAM), 'get-asm', name],
            text=True, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        return None
    m = re.search(r'address_hint:\s*([0-9a-fA-F]+)', out)
    return int(m.group(1), 16) if m else None

entries = []
for mod, bank in MODULES.items():
    run = RUNS.get(mod)
    if run is None or not run.is_file():
        continue
    for line in open(run):
        if not line.strip(): continue
        rec = json.loads(line)
        if rec.get('status') != 'pass':
            continue
        name = Path(rec['file']).stem
        if not has_standard_sig(name, mod):
            continue
        addr = get_address(name)
        if addr is None:
            continue
        pc = (bank << 16) | addr
        entries.append((pc, name, mod))

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
    c.write('    return 0;\n')
    c.write('}\n')

by_mod = {}
for _, _, m in entries:
    by_mod[m] = by_mod.get(m, 0) + 1
for m, n in sorted(by_mod.items()):
    print(f'  {m}: {n}')
