"""Locate InDesign's miter spikes: stroke ink outside the one-sided outward offset of the fill.

Usage:
    python tools/measure_miter_envelope.py PDF MANIFEST --out JSON [--dpi 1200] [--tolerance 0.01]

PDF is a suite B export (tests/indesign-stroke/B-miter-spikes) made with route W or route X
loaded. Route W draws each run's stroke as one vector path, the run's glyph outlines placed on
the page, stroked at twice the Outside weight beneath the text, with InDesign's own join and
miter limit. That path is the input here; the measured ink is the PDF's own rendering.

For each text frame whose stroke is mitered:
  envelope   the frozen topology core's one-sided outward offset of the filled outlines by the
             Outside weight, with miters only at convex exterior corners (an above-limit corner
             is cut flat, which contains PDF's bevel, so the envelope holds every legitimate
             exterior join);
  symmetric  the core's symmetric stroke of the same outlines: what InDesign draws, in Boost's
             above-limit semantics;
  ink        stroke-colored coverage in a MuPDF render of the PDF.

Ink outside the envelope (beyond a fringe band of FRINGE_PX device pixels) is an interior miter
escaping the intended stroke: a spike. Predicted spikes are symmetric minus envelope. The
record says how much measured spike ink the prediction explains, per frame and per connected
spike. Nothing here launches Adobe; the core runs without a console window.
"""
import argparse
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import curve_region as cr  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'tests/geometry/topology_build/topology_region.exe'
FRINGE_PX = 2           # device pixels beyond the envelope treated as anti-aliasing fringe
MARGIN_PT = 30.0        # render margin around each frame
NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)


def _affine(m, n):
    a, b, c, d, e, f = m
    A, B, C, D, E, F = n
    return [a * A + b * C, a * B + b * D, c * A + d * C, c * B + d * D, e * A + f * C + E, e * B + f * D + F]


def stroke_paths(operations):
    """Stroked paths with their state: [{'width', 'limit', 'join', 'gray', 'contours', 'open'}],
    each subpath (start, [('L', p) | ('C', c1, c2, p)]) in page coordinates; 'contours' are the
    closed subpaths (a glyph outline), 'open' the others (route W's miter stubs)."""
    state = {'ctm': [1, 0, 0, 1, 0, 0], 'w': 1.0, 'M': 10.0, 'j': 0, 'K': None}
    stack, contours, opened, start, segs, cur, out = [], [], [], None, [], None, []

    def P(x, y):
        m = state['ctm']
        return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])

    def close(explicit=False):
        nonlocal start, segs
        if start is not None and segs:
            (contours if explicit else opened).append((start, segs))
        start, segs = None, []

    for op, a in operations:
        if op == 'q':
            stack.append(dict(state))
        elif op == 'Q':
            state = stack.pop()
        elif op == 'cm':
            state['ctm'] = _affine([float(v) for v in a], state['ctm'])
        elif op == 'w':
            state['w'] = float(a[0])
        elif op == 'M':
            state['M'] = float(a[0])
        elif op == 'j':
            state['j'] = int(a[0])
        elif op == 'K':
            state['K'] = [float(v) for v in a]
        elif op == 'm':
            close()
            start = cur = P(float(a[0]), float(a[1]))
        elif op == 'l':
            cur = P(float(a[0]), float(a[1]))
            segs.append(('L', cur))
        elif op == 'c':
            p = [P(float(a[i]), float(a[i + 1])) for i in (0, 2, 4)]
            segs.append(('C', p[0], p[1], p[2]))
            cur = p[2]
        elif op == 'h':
            close(True)
        elif op in ('S', 's'):
            close(op == 's')
            out.append({'width': state['w'] * math.sqrt(abs(state['ctm'][0] * state['ctm'][3] - state['ctm'][1] * state['ctm'][2])),
                        'limit': state['M'], 'join': state['j'], 'gray': state['K'], 'contours': contours, 'open': opened})
            contours, opened = [], []
        elif op in ('f', 'F', 'f*', 'B', 'B*', 'b', 'b*', 'n'):
            close()
            contours, opened = [], []
    return out


def isolated_stroke(events, page_size):
    """A one-page PDF (opened in MuPDF) drawing only these strokes, each with its own width,
    limit, and join, in page coordinates."""
    import pymupdf
    if isinstance(events, dict):
        events = [events]
    parts = []
    for ev in events:
        parts.append('0 G %r w %r M %d j 0 J' % (ev['width'], ev['limit'], ev['join']))
        for subpaths, closed in ((ev.get('contours', []), True), (ev.get('open', []), False)):
            for s, segs in subpaths:
                parts.append('%r %r m' % s)
                for g in segs:
                    parts.append(('%r %r l' % g[1]) if g[0] == 'L' else ('%r %r %r %r %r %r c' % (g[1] + g[2] + g[3])))
                if closed:
                    parts.append('h')
        parts.append('S')
    doc = pymupdf.open()
    page = doc.new_page(width=page_size[0], height=page_size[1])
    xref = doc.get_new_xref()
    doc.update_object(xref, '<<>>')
    doc.update_stream(xref, ('\n'.join(parts)).encode('ascii'))
    doc.xref_set_key(page.xref, 'Contents', '%d 0 R' % xref)
    return doc


def frame_for(contours, frames, page_height):
    xs = [p[0] for s, segs in contours for p in [s] + [g[-1] for g in segs]]
    ys = [p[1] for s, segs in contours for p in [s] + [g[-1] for g in segs]]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    for f in frames:
        y0, x0, y1, x1 = f['bounds']
        if x0 <= cx <= x1 and page_height - y1 <= cy <= page_height - y0:
            return f
    return None


def core(polygons, h, limit):
    req = {'source': [cr.wkt(p) for p in polygons], 'h': h, 'limit': limit, 'compare_boundary': True}
    proc = subprocess.run([str(EXE)], input=json.dumps(req) + '\n', text=True, capture_output=True,
                          timeout=120, check=True, creationflags=NO_WINDOW)
    if proc.stderr:
        raise RuntimeError('core emitted stderr')
    return json.loads(proc.stdout.splitlines()[0])


def raster(polygons, box, scale, shape):
    """Polygons [[ring, ...], ...] in page points (y up) to a mask over box (x0, y0, x1, y1)."""
    img = Image.new('L', (shape[1], shape[0]), 0)
    d = ImageDraw.Draw(img)
    for poly in polygons:
        for k, ring in enumerate(poly):
            pts = [((x - box[0]) * scale, (box[3] - y) * scale) for x, y in ring]
            d.polygon(pts, fill=255 if k == 0 else 0)
    return np.asarray(img) > 127


def dilate(mask, r):
    from scipy import ndimage
    return ndimage.binary_dilation(mask, iterations=r)


def components(ink, box, scale, min_pt2=0.01):
    from scipy import ndimage
    lab, n = ndimage.label(ink > 0.05)
    out = []
    for i in range(1, n + 1):
        sel = lab == i
        area = float(ink[sel].sum() / scale ** 2)
        if area < min_pt2:
            continue
        rows, cols = np.nonzero(sel)
        out.append({'area_pt2': area, 'box_pt': [box[0] + cols.min() / scale, box[3] - (rows.max() + 1) / scale,
                                                 box[0] + (cols.max() + 1) / scale, box[3] - rows.min() / scale],
                    'pixels': int(sel.sum())})
    return sorted(out, key=lambda c: c['box_pt'][0])


def measure(pdf_path, manifest_path, dpi, tolerance):
    import pikepdf
    import pymupdf
    manifest = json.loads(Path(manifest_path).read_text(encoding='utf-8'))
    page_h = manifest['page'][1]
    pdf = pikepdf.open(str(pdf_path))
    ops = []
    for operands, op in pikepdf.parse_content_stream(pdf.pages[0]):
        o = str(op)
        if o in ('BDC', 'BMC', 'EMC', 'BT', 'ET', 'Tf', 'Tm', 'TJ', 'Tj', 'gs', 'k', 'rg', 'g', 'cs', 'CS', 'sc', 'scn', 'SC', 'SCN', 'W', 're'):
            if o == 're':
                ops.append(('n', []))   # a rectangle here is a clip or a fill, never a run's stroke
            continue
        ops.append((o, [x for x in operands]))
    doc = pymupdf.open(str(pdf_path))
    page = doc[0]
    scale = dpi / 72.0
    rows = []
    # A frame's strokes, in drawing order: the line's outline (closed contours), and with route
    # W's miter correction, a second stroke of exterior-corner stubs (open subpaths).
    groups, order = {}, []
    for ev in stroke_paths(ops):
        sub = ev['contours'] or ev['open']
        if not sub:
            continue
        f = frame_for(sub, manifest['frames'], page_h)
        if f is None or not str(f.get('variant', '')).startswith('text'):
            continue
        if f['name'] not in groups:
            groups[f['name']] = (f, [])
            order.append(f['name'])
        groups[f['name']][1].append(ev)
    for name in order:
        f, events = groups[name]
        outlines = [e for e in events if e['contours']]
        if not outlines or not any(e['join'] == 0 for e in events):
            continue   # not mitered
        ev = outlines[0]
        h = ev['width'] / 2
        limit = max(e['limit'] for e in events if e['join'] == 0)
        row = {'frame': f['name'], 'variant': f['variant'], 'limit': limit, 'stroke_width_pt': ev['width'],
               'outside_weight_pt': h, 'stroke_gray': ev['gray'],
               'strokes': [{'join': e['join'], 'closed_subpaths': len(e['contours']), 'open_subpaths': len(e['open'])} for e in events]}
        try:
            rings = [cr.flatten_contour(s, segs, tolerance) for s, segs in ev['contours']]
            polygons = cr.classify(rings)
        except (cr.Refused, ValueError) as e:
            row.update(status='ADAPTER_REFUSED', reason=str(e))
            rows.append(row)
            continue
        res = core(polygons, h, limit)
        if not res.get('ok'):
            row.update(status='CORE_REFUSED', reason=res.get('error'))
            rows.append(row)
            continue
        y0, x0, y1, x1 = f['bounds']
        # Clamped to the page: MuPDF clamps a pixmap clip to the page, so an unclamped box
        # shifts the render against the envelope raster (a 6 pt shift for a frame at x = 24).
        page_w = manifest['page'][0]
        box = (max(0.0, x0 - MARGIN_PT), max(0.0, page_h - y1 - MARGIN_PT),
               min(page_w, x1 + MARGIN_PT), min(page_h, page_h - y0 + MARGIN_PT))
        # The run's stroke alone: its exact path, width, limit, and join, black on white, so
        # neighboring frames and the text drawn over it do not enter the measurement.
        alone = isolated_stroke(events, manifest['page'])
        apage = alone[0]
        clip = pymupdf.Rect(box[0], page_h - box[3], box[2], page_h - box[1])
        pix = apage.get_pixmap(matrix=pymupdf.Matrix(scale, scale), clip=clip, colorspace=pymupdf.csGRAY, alpha=False)
        v = np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width).astype(np.float64)
        backdrop = 255.0
        ink = 1.0 - v / 255.0
        env = raster(res['geometry'], box, scale, v.shape)
        sym = raster(res['symmetric_geometry'], box, scale, v.shape)
        outside_env = ~dilate(env, FRINGE_PX)
        predicted = sym & outside_env
        spike = ink * outside_env
        explained = ink * predicted
        unexplained = ink * (outside_env & ~dilate(sym, FRINGE_PX))
        spikes = components(spike, box, scale)
        # Name each spike's glyph: the run's shells in reading order are its characters.
        text = ''.join(l['contents'] for l in f.get('lines', [])).replace(' ', '')
        shells = sorted(((min(p[0] for p in sh), max(p[0] for p in sh)) for sh, _ in polygons), key=lambda r: r[0])
        for c in spikes:
            cx = (c['box_pt'][0] + c['box_pt'][2]) / 2
            near = min(range(len(shells)), key=lambda i: 0 if shells[i][0] <= cx <= shells[i][1]
                       else min(abs(cx - shells[i][0]), abs(cx - shells[i][1])))
            c['glyph'] = text[near] if len(text) == len(shells) else None
        row.update(status='MEASURED', backdrop_level=backdrop,
                   ink_outside_envelope_pt2=float(spike.sum() / scale ** 2),
                   predicted_spike_pt2=float(predicted.sum() / scale ** 2),
                   ink_in_predicted_pt2=float(explained.sum() / scale ** 2),
                   ink_outside_both_pt2=float(unexplained.sum() / scale ** 2),
                   core_symmetric_excess_area=res.get('symmetric_excess_area_normalized', 0) * res.get('scale', 1) ** 2,
                   spikes=spikes)
        rows.append(row)
    return {'schema': 1, 'status': 'MEASURED', 'pdf': Path(pdf_path).name,
            'pdf_sha256': hashlib.sha256(Path(pdf_path).read_bytes()).hexdigest(),
            'core_sha256': hashlib.sha256(EXE.read_bytes()).hexdigest(),
            'tool_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'dpi': dpi, 'flatten_tolerance_pt': tolerance, 'fringe_px': FRINGE_PX, 'frames': rows,
            'limitations': ['The envelope and the symmetric stroke use Boost buffer semantics on flattened outlines; above-limit corners are cut flat, not beveled.',
                            'Ink is MuPDF coverage of the PDF route W wrote; InDesign\'s own raster is not measured here.',
                            'A spike narrower than the fringe band is not counted.']}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('pdf', type=Path)
    ap.add_argument('manifest', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--dpi', type=int, default=1200)
    ap.add_argument('--tolerance', type=float, default=0.01)
    a = ap.parse_args(argv)
    result = measure(a.pdf, a.manifest, a.dpi, a.tolerance)
    if a.out:
        a.out.write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8', newline='\n')
    for r in result['frames']:
        if r['status'] == 'MEASURED':
            print('%-34s limit %-5g spike ink %7.3f pt2 | predicted %7.3f | ink in predicted %7.3f | outside both %6.3f | spikes %d'
                  % (r['frame'][:34], r['limit'], r['ink_outside_envelope_pt2'], r['predicted_spike_pt2'],
                     r['ink_in_predicted_pt2'], r['ink_outside_both_pt2'], len(r['spikes'])))
        else:
            print('%-34s %s %s' % (r['frame'][:34], r['status'], r.get('reason', '')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
