"""Resolve the near-U-turn sliver of the miter host controls at 1000 px per point.

Usage:
    python tools/measure_miter_sliver.py RUN_DIR --pdftoppm EXE --gs EXE --work ASCII_DIR
        [--out JSON] [--scale 1000]

A near U-turn (0,0) -> (30,0) -> (0,eps) returns along a strip tilted by eps/30. Its outer
edge leaves the first strip as a wedge eps tall at x = 0 and 0 at x = 30, of area 15|eps|
pt^2, on the side of eps; the other edge stays inside. At 1200 dpi that wedge is under a pixel.

Each renderer draws two 32.1 x 1 pt strips per page, along y = +1.5 and y = -1.5, at SCALE
pixels per point, for the 12 near U-turns and for the exact U-turn with the same join as a
control on the same pixel grid. Subtracting the control cancels the renderer's edge bias. The
column profile of the difference is fitted over 1 <= x <= 29 against eps(30 - x)/30.

Renderers: MuPDF, Poppler (pdftoppm crop), Ghostscript (fixed media and PageOffset). InDesign's
PNG export cannot reach this resolution. Child processes run without a console window.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
ORIGIN = (35.0, 50.0)
PAGE = 100.0
X_RANGE = (-0.5, 31.6)    # specimen points; covers a round join's reach to 31.5
STRIP = 1.0               # strip height in points, centered on the nominal edge
FIT = (1.0, 29.0)         # columns used for the profile fit, away from both ends
AREA_TOLERANCE = 0.005    # pt^2, fixed before measuring: 5,000 px at 1000 px/pt
PROFILE_TOLERANCE = 0.1   # relative, on the fitted wedge height at x = 0
FRAMING_TOLERANCE = 0.01  # pt, on the control's ink thickness inside a strip (nominally 0.5)
CONTROL = {'miter': 'MC01', 'bevel': 'MC15', 'round': 'MC29'}
NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


def strip_box(side):
    """Strip [x0, x1, y0, y1] in specimen points for the edge on `side` (+1 top, -1 bottom)."""
    edge = 1.5 * side
    return [X_RANGE[0], X_RANGE[1], edge - STRIP / 2, edge + STRIP / 2]


def expected_sliver(eps, side):
    """Wedge area outside the first strip on `side`.

    Neglected: the return strip's ends are square to its own direction, so their corners sit
    1.5 eps/30 pt off x = 0 and x = 30, and the bevel adds a triangle of that width. Inside a
    strip these terms stay below 1e-3 pt^2, a fifth of the area tolerance."""
    return 15 * abs(eps) if eps * side > 0 else 0.0


def analyze(near, control, scale, box):
    """Difference of two gray strips (near minus control): area and column-profile fit."""
    if near.shape != control.shape:
        raise ValueError('strip shapes differ: %r %r' % (near.shape, control.shape))
    cov_near = 1.0 - near.astype(np.float64) / 255.0
    cov_ctrl = 1.0 - control.astype(np.float64) / 255.0
    columns = (cov_near - cov_ctrl).sum(axis=0) / scale    # points of extra ink per column
    x = box[0] + (np.arange(near.shape[1]) + 0.5) / scale
    keep = (x >= FIT[0]) & (x <= FIT[1])
    slope, intercept = np.polyfit(x[keep], columns[keep], 1)
    return {'area': float(columns.sum() / scale), 'fitHeightAtX0': float(intercept),
            'fitHeightAtX30': float(intercept + 30 * slope),
            'fitRms': float(np.sqrt(np.mean((columns[keep] - (intercept + slope * x[keep])) ** 2))),
            'controlInkThickness': float(cov_ctrl.sum(axis=0)[keep].mean() / scale)}


def judge(result, eps, side):
    want = expected_sliver(eps, side)
    # The control's first strip covers half of every strip; anything else means the crop missed
    # the edge, and a difference of two blank strips would otherwise read as "no sliver".
    framed = abs(result['controlInkThickness'] - STRIP / 2) <= FRAMING_TOLERANCE
    if not framed:
        return {'expectedArea': want, 'areaError': None, 'framed': False, 'pass': False}
    ok = abs(result['area'] - want) <= AREA_TOLERANCE
    if want:
        ok = ok and abs(result['fitHeightAtX0'] - abs(eps)) <= PROFILE_TOLERANCE * abs(eps) \
            and abs(result['fitHeightAtX30']) <= PROFILE_TOLERANCE * abs(eps)
    return {'expectedArea': want, 'areaError': result['area'] - want, 'framed': True, 'pass': bool(ok)}


def _gray(path):
    return np.asarray(Image.open(path).convert('L'))


def render_mupdf(pdf, page, box, scale, out):
    import pymupdf
    doc = pymupdf.open(str(pdf))
    # MuPDF page space has y down from the top of the page.
    clip = pymupdf.Rect(ORIGIN[0] + box[0], PAGE - (ORIGIN[1] + box[3]),
                        ORIGIN[0] + box[1], PAGE - (ORIGIN[1] + box[2]))
    pix = doc[page - 1].get_pixmap(matrix=pymupdf.Matrix(scale, scale), clip=clip,
                                   colorspace=pymupdf.csGRAY, alpha=False)
    pix.save(str(out))
    return _gray(out)


def render_poppler(exe, pdf, page, box, scale, out):
    x = round((ORIGIN[0] + box[0]) * scale)
    y = round((PAGE - (ORIGIN[1] + box[3])) * scale)
    w = round((box[1] - box[0]) * scale)
    h = round((box[3] - box[2]) * scale)
    stem = out.with_suffix('')
    subprocess.run([exe, '-f', str(page), '-l', str(page), '-r', str(72 * scale), '-x', str(x), '-y', str(y),
                    '-W', str(w), '-H', str(h), '-gray', '-png', '-singlefile', str(pdf), str(stem)],
                   check=True, capture_output=True, creationflags=NO_WINDOW)
    return _gray(stem.with_suffix('.png'))


def render_ghostscript(exe, pdf, page, box, scale, out):
    w = round((box[1] - box[0]) * scale)
    h = round((box[3] - box[2]) * scale)
    # Ghostscript 9.23 with fixed media moved a PDF page down for a positive y PageOffset (measured
    # 2026-10-03: +51 frames page y 51..52), the opposite of x. The framing guard in judge()
    # catches a strip placed anywhere else.
    offset = '<</PageOffset [%r %r]>> setpagedevice' % (-(ORIGIN[0] + box[0]), ORIGIN[1] + box[2])
    subprocess.run([exe, '-q', '-dNOPAUSE', '-dBATCH', '-dSAFER', '-sDEVICE=pnggray', '-r%d' % (72 * scale),
                    '-g%dx%d' % (w, h), '-dFIXEDMEDIA', '-dFirstPage=%d' % page, '-dLastPage=%d' % page,
                    '-dGraphicsAlphaBits=4', '-sOutputFile=' + str(out), '-c', offset, '-f', str(pdf)],
                   check=True, capture_output=True, creationflags=NO_WINDOW)
    return _gray(out)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('run', type=Path)
    ap.add_argument('--pdftoppm', required=True)
    ap.add_argument('--gs', required=True)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--scale', type=int, default=1000)
    a = ap.parse_args(argv)

    record = json.loads((a.run / 'controls.json').read_text(encoding='utf-8'))
    cases = {c['requested']['id']: dict(c['requested'], page=c['page']) for c in record['cases']}
    if record.get('status') != 'HOST_RECORDED' or len(cases) != 42:
        raise SystemExit('run is not a passing 42-control host record')
    for join, cid in CONTROL.items():
        c = cases[cid]
        if c['kind'] != 'exact-uturn' or c['join'] != join or c['cap'] != 'butt':
            raise SystemExit('control %s is not the butt exact U-turn for %s' % (cid, join))
    if a.work.exists():
        raise SystemExit('work directory must be new')
    a.work.mkdir(parents=True)
    pdf = a.work / 'controls.pdf'
    shutil.copyfile(a.run / 'controls.pdf', pdf)

    renderers = {'mupdf': lambda p, b, o: render_mupdf(pdf, p, b, a.scale, o),
                 'poppler': lambda p, b, o: render_poppler(a.pdftoppm, pdf, p, b, a.scale, o),
                 'ghostscript': lambda p, b, o: render_ghostscript(a.gs, pdf, p, b, a.scale, o)}
    strips = {}

    def strip(name, cid, side):
        key = (name, cid, side)
        if key not in strips:
            out = a.work / ('%s-%s-%s.png' % (name, cid, 'top' if side > 0 else 'bottom'))
            strips[key] = renderers[name](cases[cid]['page'], strip_box(side), out)
        return strips[key]

    rows = []
    near = [c for c in cases.values() if c['kind'] == 'near-uturn']
    for c in sorted(near, key=lambda c: c['id']):
        row = {'id': c['id'], 'page': c['page'], 'join': c['join'], 'epsilon': c['epsilon'],
               'control': CONTROL[c['join']], 'strips': {}}
        for side in (1, -1):
            label = 'top' if side > 0 else 'bottom'
            row['strips'][label] = {}
            for name in renderers:
                r = analyze(strip(name, c['id'], side), strip(name, CONTROL[c['join']], side), a.scale, strip_box(side))
                r.update(judge(r, c['epsilon'], side))
                row['strips'][label][name] = r
        row['pass'] = all(r['pass'] for s in row['strips'].values() for r in s.values())
        rows.append(row)
        for key in [k for k in strips if k[1] == c['id']]:
            del strips[key]

    summary = {'cases': len(rows), 'allPass': sum(r['pass'] for r in rows),
               'perRenderer': {n: sum(all(r['strips'][s][n]['pass'] for s in r['strips']) for r in rows)
                               for n in renderers}}
    result = {'schema': 1, 'status': 'SLIVER_RESOLVED' if summary['allPass'] == len(rows) else 'SLIVER_MISMATCH',
              'host': record.get('hostVersion'), 'run': a.run.name, 'pixelsPerPoint': a.scale,
              'tolerances': {'areaPt2': AREA_TOLERANCE, 'profileRelative': PROFILE_TOLERANCE, 'fitX': FIT},
              'renderers': {'mupdf': __import__('pymupdf').VersionBind,
                            'poppler': subprocess.run([a.pdftoppm, '-v'], capture_output=True, text=True,
                                                      creationflags=NO_WINDOW).stderr.strip().splitlines()[0],
                            'ghostscript': subprocess.run([a.gs, '--version'], capture_output=True, text=True,
                                                          creationflags=NO_WINDOW).stdout.strip()},
              'hashes': {'controls.pdf': hashlib.sha256(pdf.read_bytes()).hexdigest(),
                         'tools/measure_miter_sliver.py': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
              'summary': summary, 'cases': rows,
              'limitations': ['Strips cover 1 pt around each nominal edge; ink farther away is not measured here.',
                              'InDesign\'s own raster is not included: its PNG export cannot reach this resolution.']}
    if a.out:
        a.out.write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(summary))
    return 0 if result['status'] == 'SLIVER_RESOLVED' else 1


if __name__ == '__main__':
    sys.exit(main())
