"""Condense a route X research trace (ISE_XF_TRACE, research build) into draw-order facts.

The trace is written by plugin/source/IseCrossFrame.cpp. Lines, each optionally prefixed with a
thread tag "T<id> ":
  E <seq> <event> depth=<d> item=<uid> class=<name> flags=<hex> port=<ptr> kind=<screen|pdf|print>
  P <W|R|w> parcel=<ptr> ok=<0|1> frame=<column uid> story=<frame list uid> key=<k> parcels=<n> xp=<0|1> ...
  R <W|R> <what> run=<ptr> port=<ptr> ...           an adornment visit (W stroke pass, R restore pass)
  F pending|painted run=<ptr> ...                    a fill recorded, or confirmed painted
  K run=<ptr> withheld=<n> covered=<n> box=...       a stroke's knockout decision
  C shape|frame-fill|frame-stroke item=<uid> box=... content drawn during the traversal
  D defer-probe glyphs=<n> ...                       the deferred-emission probe
  S suppress-shape|suppress-text item=<uid>          a suppression probe

Usage: routex_trace.py TRACE [--names ids.tsv] [--start BYTE] [--end BYTE] [--out JSON]
"""
import argparse
import json
import re
from collections import Counter, defaultdict
from pathlib import Path


def parse(text, names):
    traversals, cur = [], None
    frame = None
    for raw in text.splitlines():
        line = re.sub(r'^T\d+ ', '', raw)
        m = re.match(r'E (\d+) (\w+) depth=(\d+) item=(\d+) class=\S+ flags=(\w+) port=(\w+) kind=(\w+)', line)
        if m:
            seq, ev, depth, item, flags, port, kind = int(m[1]), m[2], int(m[3]), int(m[4]), m[5], m[6], m[7]
            if ev in ('BeginSpread', 'BeginPartial') or (cur is None and ev not in ('EndSpread', 'EndPartial')):
                cur = {'kind': kind, 'flags': flags, 'bracket': ev if ev in ('BeginSpread', 'BeginPartial') else 'none',
                       'ports': set(), 'events': Counter(), 'frame_order': [], 'passes': defaultdict(list),
                       'knockouts': defaultdict(lambda: [0, 0, 0]), 'fills_recorded': 0, 'fills_painted': 0,
                       'interleaved_frames': 0, 'defer_probe_glyphs': None, 'suppressed': []}
                traversals.append(cur)
            cur['ports'].add(port)
            cur['events'][ev] += 1
            if ev in ('EndSpread', 'EndPartial'):
                cur = None
            continue
        if cur is None:
            continue
        m = re.match(r'P ([WRw]) parcel=\S+ ok=\d frame=(\d+) story=(\d+)', line)
        if m:
            frame = int(m[2])
            if not cur['frame_order'] or cur['frame_order'][-1][0] != frame:
                cur['frame_order'].append((frame, int(m[3])))
            continue
        m = re.match(r'R ([WR]) \w+ run=\S+ port=(\w+)', line)
        if m and frame is not None:
            cur['passes'][frame].append(m[1])
            cur['ports'].add(m[2])
            continue
        if line.startswith('F pending'):
            cur['fills_recorded'] += 1
        elif line.startswith('F painted'):
            cur['fills_painted'] += 1
        m = re.match(r'K run=\S+ withheld=(\d+) covered=(\d+)', line)
        if m and frame is not None:
            k = cur['knockouts'][frame]
            k[0] += 1; k[1] += int(m[1]); k[2] += int(m[2])
        m = re.match(r'D defer-probe glyphs=(\d+)', line)
        if m:
            cur['defer_probe_glyphs'] = int(m[1])
        m = re.match(r'S (\S+) item=(\d+)', line)
        if m:
            cur['suppressed'].append({'probe': m[1], 'item': int(m[2])})
    out = []
    for t in traversals:
        order = [f for f, _ in t['frame_order']]
        # A frame whose passes are split by another frame's would appear twice in the order.
        seen, interleaved = set(), 0
        for f in order:
            if f in seen:
                interleaved += 1
            seen.add(f)
        label = lambda f: names.get(f - 2, str(f))
        out.append({
            'kind': t['kind'], 'flags': t['flags'], 'bracket': t['bracket'], 'distinct_ports': len(t['ports']),
            'events': dict(t['events']), 'frames_drawn': len(seen),
            'frames_reentered_after_another_frame': interleaved,
            'pass_sequences': sorted(Counter(''.join(p) for p in t['passes'].values()).items()),
            'fills_recorded': t['fills_recorded'], 'fills_painted': t['fills_painted'],
            'knockouts': {label(f): {'strokes': v[0], 'withheld_glyphs': v[1], 'covered_glyphs': v[2]} for f, v in t['knockouts'].items() if v[1] or v[2]},
            'defer_probe_glyphs': t['defer_probe_glyphs'], 'suppressed': t['suppressed'],
        })
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('trace', type=Path)
    ap.add_argument('--names', type=Path)
    ap.add_argument('--start', type=int, default=0)
    ap.add_argument('--end', type=int, default=None)
    ap.add_argument('--out', type=Path)
    a = ap.parse_args(argv)
    data = a.trace.read_bytes()[a.start:a.end].decode('utf-8', 'replace')
    names = {}
    if a.names:
        for line in a.names.read_text(encoding='utf-8').splitlines():
            parts = line.split('\t')
            if len(parts) >= 2 and parts[0].isdigit():
                names[int(parts[0])] = parts[1]
    result = parse(data, names)
    text = json.dumps(result, indent=1)
    if a.out:
        a.out.write_bytes((text + '\n').encode('utf-8'))
    else:
        print(text)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
