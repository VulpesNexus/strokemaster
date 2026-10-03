"""Measure the 42 miter host controls in InDesign's exported PDF.

Usage:
    python tools/measure_miter_controls.py RUN_DIR --pdftoppm EXE --gs EXE --work ASCII_DIR
        [--out JSON] [--dpi 1200]

RUN_DIR is a build/miter-controls-<ms>/ folder written by tools/jsx/probe-miter-host-controls.jsx.
ASCII_DIR receives a copy of the PDF and the renders, because Poppler and Ghostscript are
ANSI programs that cannot open the workspace path.

Three independent checks per control page:
  1. Operators: the PDF's stroke parameters (content stream and ExtGState) and path vertices
     against the requested specimen.
  2. Renders: MuPDF, Poppler, Ghostscript, and InDesign's own PNG, measured as anti-aliased
     coverage area and 50%-coverage bounds in specimen points.
  3. Expectation: an analytic area and bounds derived from the PDF stroking rules (ISO 32000-2,
     8.5.3.2: a join whose miter ratio exceeds the limit is beveled). The expectation is a
     hand derivation, not a stroker; near U-turns carry an O(eps^2) approximation.

Nothing here launches Adobe. Child processes run without a console window.
"""
import argparse
import hashlib
import json
import math
import re
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HALF = 1.5            # half of the requested 3 pt stroke
ORIGIN = (35.0, 50.0)  # specimen origin in PDF page coordinates (y up), page 100 x 100 pt
PAGE = 100.0
JOIN_CODES = {'miter': 0, 'round': 1, 'bevel': 2}
CAP_CODES = {'butt': 0, 'round': 1, 'projecting': 2}
AREA_TOLERANCE = 0.5     # pt^2, fixed before measuring; about 140 px at 1200 dpi
BOUNDS_TOLERANCE = 0.12  # pt, two pixels at 1200 dpi
NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


def expected_ink(case):
    """Analytic ink area (pt^2) and bounds [x0, x1, y0, y1] in specimen points."""
    kind, join, cap, r = case['kind'], case['join'], case['cap'], HALF
    disc = math.pi * r * r
    # Outline length of each end: butt 2r, round half circle, projecting 2r plus r on each side.
    end = {'butt': 2 * r, 'round': math.pi * r, 'projecting': 4 * r}
    if kind == 'segment':
        extra = {'butt': 0.0, 'round': disc, 'projecting': 4 * r * r}[cap]
        reach = 0.0 if cap == 'butt' else r
        return {'area': 30 * 2 * r + extra, 'bounds': [-reach, 30 + reach, -r, r],
                'perimeter': 60 + 2 * end[cap], 'rule': 'two caps on one 30 pt segment'}
    if kind == 'exact-uturn':
        # Both legs coincide. The 180-degree join has an infinite miter ratio, so a miter join is
        # beveled, and a bevel across a full reversal adds nothing; a round join adds a half disc.
        # The two caps lie on top of each other at x = 0.
        join_extra = disc / 2 if join == 'round' else 0.0
        cap_extra = {'butt': 0.0, 'round': disc / 2, 'projecting': 2 * r * r}[cap]
        return {'area': 60 * r + join_extra + cap_extra,
                'bounds': [0.0 if cap == 'butt' else -r, 30 + (r if join == 'round' else 0.0), -r, r],
                'perimeter': 60 + end[cap] + (end['round'] if join == 'round' else 2 * r),
                'rule': 'coincident legs; reversal join beveled unless round; coincident caps'}
    if kind == 'closed-two-anchor':
        # Closed 0 -> 30 -> 0: a reversal join at each end, no caps.
        extra = disc if join == 'round' else 0.0
        reach = r if join == 'round' else 0.0
        return {'area': 60 * r + extra, 'bounds': [-reach, 30 + reach, -r, r],
                'perimeter': 60 + 2 * (end['round'] if join == 'round' else 2 * r),
                'rule': 'closed reversal; joins at both ends, caps do not apply'}
    if kind == 'right-angle':
        # Legs (0,-20)-(0,0) and (0,0)-(20,0), butt caps; miter ratio sqrt(2) is within limit 4.
        union = 2 * 20 * 2 * r - r * r
        extra = {'miter': r * r, 'bevel': r * r / 2, 'round': disc / 4}[join]
        return {'area': union + extra, 'bounds': [-r, 20.0, -20.0, r],
                'perimeter': 4 * (20 + r) - 2 * r + {'miter': 2 * r, 'bevel': r * math.sqrt(2), 'round': math.pi * r / 2}[join],
                'rule': 'two 20 pt legs; outer corner filled by the join'}
    if kind == 'near-uturn':
        # (0,0) -> (30,0) -> (0,eps). The miter ratio is about 60/|eps|, far above the limit, so
        # miter and bevel joins both bevel, and the bevel triangle is O(|eps|) small. The return
        # strip leaves the first one as a sliver of area 15|eps|. Neglected terms are below 1e-3.
        eps = case['epsilon']
        join_extra = disc / 2 if join == 'round' else 0.0
        return {'area': 60 * r + 15 * abs(eps) + join_extra,
                'bounds': [0.0, 30 + (r if join == 'round' else 0.0), -r + min(eps, 0.0), r + max(eps, 0.0)],
                'perimeter': 60 + 2 * r + (end['round'] if join == 'round' else 2 * r),
                'rule': 'near reversal beveled unless round; sliver 15|eps|; O(eps^2) neglected'}
    raise ValueError('unknown control kind ' + kind)


def _affine(m, n):
    """Compose PDF matrices: apply m, then n."""
    a, b, c, d, e, f = m
    A, B, C, D, E, F = n
    return [a * A + b * C, a * B + b * D, c * A + d * C, c * B + d * D,
            e * A + f * C + E, e * B + f * D + F]


def _apply(m, x, y):
    return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])


def stroke_events(operations, ext_gstates):
    """Stroked paths in page coordinates with the graphics state that painted them.

    operations: iterable of (operator name, list of float-or-name operands).
    ext_gstates: {name: {key: value}} from the page's /ExtGState resources.
    """
    state = {'ctm': [1, 0, 0, 1, 0, 0], 'w': 1.0, 'J': 0, 'j': 0, 'M': 10.0, 'dash': None}
    stack, subpaths, current, events = [], [], None, []
    for op, args in operations:
        if op == 'q':
            stack.append(dict(state))
        elif op == 'Q':
            state = stack.pop()
        elif op == 'cm':
            state['ctm'] = _affine([float(v) for v in args], state['ctm'])
        elif op == 'w':
            state['w'] = float(args[0])
        elif op == 'J':
            state['J'] = int(args[0])
        elif op == 'j':
            state['j'] = int(args[0])
        elif op == 'M':
            state['M'] = float(args[0])
        elif op == 'd':
            state['dash'] = [list(args[0]), float(args[1])]
        elif op == 'gs':
            g = ext_gstates.get(str(args[0]), {})
            for key, slot in (('LW', 'w'), ('LC', 'J'), ('LJ', 'j'), ('ML', 'M')):
                if key in g:
                    state[slot] = g[key]
            if 'D' in g:
                state['dash'] = g['D']
        elif op == 'm':
            current = {'points': [_apply(state['ctm'], float(args[0]), float(args[1]))], 'closed': False, 'curves': 0}
            subpaths.append(current)
        elif op == 'l':
            current['points'].append(_apply(state['ctm'], float(args[0]), float(args[1])))
        elif op in ('c', 'v', 'y'):
            current['points'].append(_apply(state['ctm'], float(args[-2]), float(args[-1])))
            current['curves'] += 1
        elif op == 'h':
            current['closed'] = True
        elif op == 're':
            x, y, w, h = (float(v) for v in args)
            corners = [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
            subpaths.append({'points': [_apply(state['ctm'], *p) for p in corners], 'closed': True, 'curves': 0})
        elif op in ('S', 's', 'B', 'B*', 'b', 'b*', 'f', 'F', 'f*', 'n'):
            if op in ('s', 'b', 'b*') and subpaths:
                subpaths[-1]['closed'] = True
            if op in ('S', 's', 'B', 'B*', 'b', 'b*'):
                m = state['ctm']
                events.append({'operator': op, 'width': state['w'], 'cap': state['J'], 'join': state['j'],
                               'miterLimit': state['M'], 'dash': state['dash'],
                               'ctmScale': math.sqrt(abs(m[0] * m[3] - m[1] * m[2])),
                               'subpaths': [{'points': [list(p) for p in s['points']], 'closed': s['closed'],
                                             'curves': s['curves']} for s in subpaths]})
            subpaths, current = [], None
    return events


def compare_operators(case, events):
    """Requested stroke versus the single stroke event on the page."""
    out = {'strokeEvents': len(events)}
    if len(events) != 1 or len(events[0]['subpaths']) != 1:
        out['match'] = False
        out['reason'] = 'expected one stroke of one subpath'
        return out
    ev, sub = events[0], events[0]['subpaths'][0]
    want = [[ORIGIN[0] + x, ORIGIN[1] + y] for x, y in case['points']]
    written = sub['points']
    # InDesign writes a closed contour with an explicit return to its first vertex, then h.
    closing_written = bool(sub['closed'] and len(written) == len(want) + 1 and
                           max(abs(written[-1][k] - written[0][k]) for k in (0, 1)) < 1e-9)
    vertices = written[:-1] if closing_written else written
    deviation = (max(abs(a[k] - b[k]) for a, b in zip(vertices, want) for k in (0, 1))
                 if len(vertices) == len(want) else None)
    out.update({'width': ev['width'] * ev['ctmScale'], 'miterLimit': ev['miterLimit'], 'joinCode': ev['join'],
                'capCode': ev['cap'], 'dash': ev['dash'], 'closed': sub['closed'], 'curves': sub['curves'],
                'closingSegmentWritten': closing_written, 'vertexDeviation': deviation})
    out['match'] = bool(deviation is not None and deviation <= 1e-6 and sub['closed'] == case['closed'] and
                        sub['curves'] == 0 and abs(out['width'] - case['width']) < 1e-9 and
                        abs(ev['miterLimit'] - case['limit']) < 1e-9 and ev['join'] == JOIN_CODES[case['join']] and
                        ev['cap'] == CAP_CODES[case['cap']] and ev['dash'] in (None, [[], 0.0]))
    return out


def measure_coverage(gray, dpi):
    """Coverage area (pt^2) and 50%-coverage bounds in specimen points of a gray page render."""
    a = np.asarray(gray, dtype=np.float64)
    if a.shape[0] != a.shape[1] or abs(a.shape[0] - PAGE * dpi / 72) > 1:
        raise ValueError('unexpected render size %r' % (a.shape,))
    s = 72.0 / dpi
    coverage = 1.0 - a / 255.0
    rows, cols = np.nonzero(coverage >= 0.5)
    if not len(rows):
        return {'area': float(coverage.sum() * s * s), 'bounds': None}
    top_y = PAGE - ORIGIN[1]  # specimen y = PAGE - row*s - ORIGIN[1]
    return {'area': float(coverage.sum() * s * s),
            'bounds': [float(cols.min() * s - ORIGIN[0]), float((cols.max() + 1) * s - ORIGIN[0]),
                       float(top_y - (rows.max() + 1) * s), float(top_y - rows.min() * s)]}


def judge(measured, expected):
    if measured['bounds'] is None:
        return {'areaError': None, 'boundsError': None, 'pass': False}
    area_error = measured['area'] - expected['area']
    bounds_error = max(abs(m - e) for m, e in zip(measured['bounds'], expected['bounds']))
    return {'areaError': area_error, 'boundsError': bounds_error,
            'pass': abs(area_error) <= AREA_TOLERANCE and bounds_error <= BOUNDS_TOLERANCE}


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _numbered(folder, prefix):
    files = [p for p in Path(folder).glob(prefix + '*.png') if re.search(r'(\d+)\.png$', p.name)]
    return sorted(files, key=lambda p: int(re.search(r'(\d+)\.png$', p.name).group(1)))


def render_all(pdf, work, dpi, pdftoppm, gs):
    import pymupdf as fitz
    renders, versions = {}, {'mupdf': fitz.VersionBind}
    doc = fitz.open(str(pdf))
    renders['mupdf'] = [Image.frombytes('L', (p.width, p.height), p.samples)
                        for p in (page.get_pixmap(dpi=dpi, colorspace=fitz.csGRAY, alpha=False) for page in doc)]
    out = work / 'poppler'
    out.mkdir()
    subprocess.run([pdftoppm, '-r', str(dpi), '-gray', '-png', str(pdf), str(out / 'p')],
                   check=True, capture_output=True, creationflags=NO_WINDOW)
    renders['poppler'] = [Image.open(p).convert('L') for p in _numbered(out, 'p-')]
    versions['poppler'] = subprocess.run([pdftoppm, '-v'], capture_output=True, text=True,
                                         creationflags=NO_WINDOW).stderr.strip().splitlines()[0]
    out = work / 'ghostscript'
    out.mkdir()
    subprocess.run([gs, '-q', '-dNOPAUSE', '-dBATCH', '-dSAFER', '-sDEVICE=pnggray', '-r%d' % dpi,
                    '-dGraphicsAlphaBits=4', '-sOutputFile=' + str(out / 'g-%02d.png'), str(pdf)],
                   check=True, capture_output=True, creationflags=NO_WINDOW)
    renders['ghostscript'] = [Image.open(p).convert('L') for p in _numbered(out, 'g-')]
    versions['ghostscript'] = subprocess.run([gs, '--version'], capture_output=True, text=True,
                                             creationflags=NO_WINDOW).stdout.strip()
    return renders, versions


def indesign_pngs(run, count):
    # InDesign names page 1 controls.png and page N controlsN.png (N = 2..count).
    names = ['controls.png'] + ['controls%d.png' % n for n in range(2, count + 1)]
    return [Image.open(run / n).convert('L') for n in names]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('run', type=Path)
    ap.add_argument('--pdftoppm', required=True)
    ap.add_argument('--gs', required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--dpi', type=int, default=1200)
    a = ap.parse_args(argv)
    import pikepdf

    record = json.loads((a.run / 'controls.json').read_text(encoding='utf-8'))
    cases = [c['requested'] for c in record['cases']]
    if record.get('status') != 'HOST_RECORDED' or len(cases) != 42:
        raise SystemExit('run is not a passing 42-control host record')
    if a.work.exists():
        raise SystemExit('work directory must be new')
    a.work.mkdir(parents=True)
    pdf = a.work / 'controls.pdf'
    shutil.copyfile(a.run / 'controls.pdf', pdf)

    doc = pikepdf.open(str(pdf))
    if len(doc.pages) != len(cases):
        raise SystemExit('page count %d does not match %d controls' % (len(doc.pages), len(cases)))
    renders, versions = render_all(pdf, a.work, a.dpi, a.pdftoppm, a.gs)
    renders['indesign'] = indesign_pngs(a.run, len(cases))
    for name, images in renders.items():
        if len(images) != len(cases):
            raise SystemExit('%s produced %d pages' % (name, len(images)))

    rows = []
    for i, case in enumerate(cases):
        page = doc.pages[i]
        ext = {}
        for name, g in (page.Resources.get('/ExtGState') or {}).items():
            ext[str(name)] = {}
            for k, v in g.items():
                key = str(k)[1:]
                if key in ('LW', 'ML'):
                    ext[str(name)][key] = float(v)
                elif key in ('LC', 'LJ'):
                    ext[str(name)][key] = int(v)
                elif key == 'D':
                    ext[str(name)][key] = [[float(x) for x in v[0]], float(v[1])]
            ext[str(name)]['other'] = sorted(str(k) for k in g.keys())
        ops = []
        for operands, op in pikepdf.parse_content_stream(page):
            op = str(op)
            if op in ('BDC', 'EMC', 'BMC', 'RG', 'rg', 'G', 'g', 'K', 'k', 'CS', 'cs', 'SC', 'SCN', 'sc', 'scn'):
                continue
            if op == 'd':
                ops.append((op, [[float(x) for x in operands[0]], float(operands[1])]))
            elif op == 'gs':
                ops.append((op, [str(operands[0])]))
            else:
                ops.append((op, [float(x) for x in operands]))
        events = stroke_events(ops, ext)
        expected = expected_ink(case)
        row = {'id': case['id'], 'page': i + 1, 'kind': case['kind'], 'join': case['join'], 'cap': case['cap'],
               'epsilon': case['epsilon'], 'operators': compare_operators(case, events), 'expected': expected,
               'renders': {}}
        for name, images in renders.items():
            m = measure_coverage(images[i], a.dpi)
            m.update(judge(m, expected))
            row['renders'][name] = m
        row['pass'] = row['operators']['match'] and all(r['pass'] for r in row['renders'].values())
        rows.append(row)

    # Diagnostic, not a verdict: a rasterizer that grows or thins every edge by a constant offset
    # changes the area by offset x outline length. Estimate each renderer's offset from the plain
    # butt segment alone (MC00), then report what that offset leaves unexplained elsewhere.
    base = next(r for r in rows if r['kind'] == 'segment' and r['cap'] == 'butt' and r['join'] == 'miter')
    offsets, residual_max = {}, {}
    for n in renders:
        offsets[n] = base['renders'][n]['areaError'] / base['expected']['perimeter']
        for r in rows:
            r['renders'][n]['edgeBiasResidual'] = r['renders'][n]['areaError'] - offsets[n] * r['expected']['perimeter']
        residual_max[n] = max(abs(r['renders'][n]['edgeBiasResidual']) for r in rows)
    summary = {'cases': len(rows), 'operatorsMatch': sum(r['operators']['match'] for r in rows),
               'allPass': sum(r['pass'] for r in rows),
               'perRenderer': {n: sum(r['renders'][n]['pass'] for r in rows) for n in renders},
               'edgeOffsetPt': offsets, 'maxAbsEdgeBiasResidualPt2': residual_max}
    result = {'schema': 1, 'status': 'PDF_MEASURED' if summary['allPass'] == len(rows) else 'PDF_MEASURED_WITH_FAILURES',
              'host': record.get('hostVersion'), 'run': a.run.name, 'dpi': a.dpi,
              'tolerances': {'areaPt2': AREA_TOLERANCE, 'boundsPt': BOUNDS_TOLERANCE},
              'renderers': versions | {'indesign': 'InDesign PNG export, 1200 ppi, anti-aliased'},
              'hashes': {'controls.pdf': _sha256(pdf), 'controls.json': _sha256(a.run / 'controls.json'),
                         'tools/measure_miter_controls.py': _sha256(__file__)},
              'summary': summary, 'cases': rows,
              'limitations': ['Analytic expectations are hand derivations from the PDF stroking rules, not a stroker.',
                              'Coverage area depends on each renderer\'s anti-aliasing; the tolerance was fixed before measuring.',
                              'Bounds compare 50%-coverage pixels, so a sub-pixel feature narrower than a pixel is not resolved.']}
    text = json.dumps(result, indent=1)
    if a.out:
        a.out.write_text(text + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(summary))
    return 0 if result['status'] == 'PDF_MEASURED' else 1


if __name__ == '__main__':
    sys.exit(main())
