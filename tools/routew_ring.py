"""Characterize route W's edge ring: vector geometry first, then rasterization.

CLI: ROUTEW_PDF ROUTEW_DIR REFERENCE_DIR MANIFEST OUTPUT [--keys VC02 VC11 VC12 VC13]

Route W strokes the font's outlines through InDesign's graphics port; InDesign's own text stroke
strokes the same outlines inside its text renderer. The two can differ in geometry (other
outline points, flattening, offset) or only in how each is rasterized.

1. Geometry. In route W's PDF the stroke of each line is a path and the fill is text. The
   embedded font program (CFF) gives each glyph's outline; placed at the fill glyph's origin and
   size, it is the outline the fill draws. Every on-curve point of route W's stroke path is
   matched to the nearest on-curve point of those outlines. Identical geometry puts every point
   within the PDF's coordinate precision (0.01 pt).
2. Stroke parameters: width, join, cap, and miter limit of route W's paths.
3. Rasterization. For each resolution present in both folders (InDesign PNG exports, and MuPDF
   renders of the PDFs), route W against the split reference: differing pixels, how far they
   lie from an edge in device pixels, and the largest difference. A geometric offset would grow
   with resolution in device pixels; a rasterization difference stays within a pixel.
"""
import argparse
import io
import json
import sys
from pathlib import Path

import numpy
import pikepdf
import pymupdf
from fontTools.cffLib import CFFFontSet
from fontTools.pens.recordingPen import RecordingPen

sys.path.insert(0, str(Path(__file__).resolve().parent))
import routew_measure as m  # noqa: E402


def embedded_cff(pdf_path):
    fonts = {}
    with pikepdf.open(pdf_path) as pdf:
        for obj in pdf.objects:
            if isinstance(obj, pikepdf.Dictionary) and obj.get('/Type') == '/FontDescriptor' and '/FontFile3' in obj:
                data = obj.FontFile3.read_bytes()
                cff = CFFFontSet()
                cff.decompile(io.BytesIO(data), None)
                fonts[str(obj.FontName)[1:]] = cff[cff.fontNames[0]]
    return fonts


def outline_points(top, glyph_name):
    cs = top.CharStrings[glyph_name]
    pen = RecordingPen()
    cs.draw(pen)
    pts = []
    for op, args in pen.value:
        if op in ('moveTo', 'lineTo'):
            pts.append(args[0])
        elif op in ('curveTo', 'qCurveTo'):
            pts.append(args[-1])
    return pts


def geometry(pdf_path, manifest_frames, keys):
    doc = pymupdf.open(pdf_path)
    page = doc[0]
    fonts = embedded_cff(pdf_path)
    trace = [s for s in page.get_texttrace() if s['type'] == 0]
    paths = [d for d in page.get_drawings() if d['type'] == 's']
    frames = {f['key']: f for f in manifest_frames if f['key'] in keys}
    out = {}
    for key, f in frames.items():
        y1, x1, y2, x2 = f['bounds']
        inside = lambda r: x1 - 15 <= (r[0] + r[2]) / 2 <= x2 + 15 and y1 - 15 <= (r[1] + r[3]) / 2 <= y2 + 15
        glyph_pts = []
        for span in trace:
            if not inside(span['bbox']):
                continue
            font = next((v for k, v in fonts.items() if k.split('+')[-1] == span['font'].split('+')[-1]), None)
            if font is None:
                continue
            charset = font.charset
            a, b, c, d_, e, f_ = span.get('matrix', (1, 0, 0, 1, 0, 0)) if 'matrix' in span else (1, 0, 0, 1, 0, 0)
            size = span['size']
            for ch in span['chars']:
                gid, origin = ch[1], ch[2]
                name = charset[gid] if gid < len(charset) else chr(ch[0])
                for px, py in outline_points(font, name):
                    glyph_pts.append((origin[0] + px * size / 1000.0, origin[1] - py * size / 1000.0))
        stroke_pts, params = [], set()
        for dpath in paths:
            if not inside(dpath['rect']):
                continue
            params.add((round(dpath['width'], 3), dpath.get('lineJoin'), str(dpath.get('lineCap')), dpath.get('miterLimit')))
            for it in dpath['items']:
                if it[0] == 'l':
                    stroke_pts += [(it[1].x, it[1].y), (it[2].x, it[2].y)]
                elif it[0] == 'c':
                    stroke_pts += [(it[1].x, it[1].y), (it[4].x, it[4].y)]
        if not glyph_pts or not stroke_pts:
            out[key] = {'error': 'no glyphs or no stroke paths found', 'glyph_points': len(glyph_pts), 'stroke_points': len(stroke_pts)}
            continue
        G = numpy.array(glyph_pts)
        S = numpy.unique(numpy.round(numpy.array(stroke_pts), 4), axis=0)
        dist = numpy.sqrt(((S[:, None, :] - G[None, :, :]) ** 2).sum(axis=2)).min(axis=1)
        out[key] = {'stroke_points': int(len(S)), 'outline_points': int(len(G)),
                    'nearest_outline_point_pt': {'max': round(float(dist.max()), 5), 'median': round(float(numpy.median(dist)), 5),
                                                 'p99': round(float(numpy.percentile(dist, 99)), 5)},
                    'stroke_params': sorted([list(p) for p in params])}
    return out


PNG_PPI = {'png72': 72, 'png150': 150, 'png300': 300, 'png600': 600, 'png300noaa': 300, 'png600noaa': 600}
PDF_DPI = [150, 300, 600, 1200, 2400]


def _row(a, b, frame, ppi):
    e = m.edge_metrics(a, b, [frame], ppi)['frames'][0]
    return {k: e[k] for k in ('px_diff', 'px_over_16', 'px_over_64', 'max_diff', 'over_16_by_edge_distance_px', 'over_64_beyond_band', 'net_signed_sum')}


def raster(routew_dir, reference_dir, manifest_frames, keys):
    """InDesign's PNG exports at each resolution, and MuPDF renders of the High Quality Print PDFs
    from 150 to 2400 dpi (each frame rendered alone, so high resolutions stay small)."""
    rows = {}
    frames = [f for f in manifest_frames if f['key'] in keys]
    for stem, ppi in PNG_PPI.items():
        out, ref = Path(routew_dir) / (stem + '.png'), Path(reference_dir) / (stem + '.png')
        if out.exists() and ref.exists():
            (a, b), _ = m.align(m.gray(out), m.gray(ref))
            rows['InDesign ' + stem] = {'ppi': ppi, 'frames': {f['key']: _row(a, b, f, ppi) for f in frames}}
    out, ref = Path(routew_dir) / 'hqp.pdf', Path(reference_dir) / 'hqp.pdf'
    pa, pb = pymupdf.open(str(out))[0], pymupdf.open(str(ref))[0]
    for dpi in PDF_DPI:
        per = {}
        for f in frames:
            y1, x1, y2, x2 = f['bounds']
            clip = pymupdf.Rect(x1 - m.PAD_PT, y1 - m.PAD_PT, x2 + m.PAD_PT, y2 + m.PAD_PT)
            imgs = []
            for pg in (pa, pb):
                pix = pg.get_pixmap(dpi=dpi, colorspace=pymupdf.csGRAY, clip=clip)
                imgs.append(numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width))
            local = dict(f, bounds=[m.PAD_PT, m.PAD_PT, y2 - y1 + m.PAD_PT, x2 - x1 + m.PAD_PT])
            (a, b), _ = m.align(*imgs)
            per[f['key']] = _row(a, b, local, dpi)
        rows['MuPDF hqp.pdf @%d' % dpi] = {'ppi': dpi, 'frames': per}
    for label, r in rows.items():
        print(label, {k: (v['px_diff'], v['max_diff'], v['over_16_by_edge_distance_px']) for k, v in r['frames'].items()}, flush=True)
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('routew_pdf')
    parser.add_argument('routew_dir')
    parser.add_argument('reference_dir')
    parser.add_argument('manifest')
    parser.add_argument('output', type=Path)
    parser.add_argument('--keys', nargs='+', default=['VC02', 'VC11', 'VC12', 'VC13'])
    args = parser.parse_args(argv)
    frames = json.loads(Path(args.manifest).read_text(encoding='utf-8'))['frames']
    rec = {'schema': 1, 'keys': args.keys, 'geometry': geometry(args.routew_pdf, frames, args.keys),
           'raster': raster(args.routew_dir, args.reference_dir, frames, args.keys)}
    print(json.dumps(rec['geometry'], indent=1))
    args.output.write_bytes((json.dumps(rec, indent=1) + '\n').encode('utf-8'))


if __name__ == '__main__':
    main()
