"""Record an InDesign victim-census run made with route W installed, without Adobe.

CLI: RUN BASELINE OUTPUT --plugin-sha SHA [--pdf RUN/pdf/routew.pdf]

RUN is a fresh tools/jsx/probe-victim-census.jsx folder made with route W loaded; BASELINE is
the recorded no-plugin folder made by the same probe. The record holds:

- route W native against its own split reference, inside the independent fill mask
  (the pass test: zero fill-core pixels changed in every frame);
- the no-plugin native against its split reference, the same measurement without the plugin;
- the no-plugin native against route W native (what the plugin changed);
- the size of every edge difference against the reference (max channel difference, pixels
  over 64 and over 128), so an anti-aliasing ring is not reported as a pass or a defect by name;
- for the re-exported PDF: live text and fonts, each frame's paint events in painting order,
  strokes drawn after a frame's text (only strikethrough bars are expected there), line
  widths, and the MuPDF render against the reference inside the fill core.

Frames are matched by the centers of their paint events; a page-sized rectangle is the page
background and belongs to no frame. MuPDF renders the PDF; one renderer is not every renderer.
"""
import argparse
import hashlib
import json
import sys
import tempfile
from pathlib import Path

import numpy
import pymupdf
from PIL import Image
from scipy import ndimage

sys.path.insert(0, str(Path(__file__).resolve().parent))
import compare_renders  # noqa: E402

PPI = 600
ERODE = 6
EDGE_PAD_PT = 6


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare(a, b, manifest, mask):
    with tempfile.TemporaryDirectory() as folder:
        stem = str(Path(folder) / 'c')
        compare_renders.main(str(a), str(b), str(manifest), stem, PPI, str(mask))
        report = json.loads(Path(stem + '.json').read_text(encoding='utf-8'))
    keep = ('key', 'changed_px', 'max_channel_diff', 'interior_px', 'interior_changed_px', 'edge_changed_px')
    return {'page_changed_px': report['page_changed_px'],
            'frames': [{k: f.get(k) for k in keep} for f in report['frames']]}


def gray(path):
    return numpy.asarray(Image.open(path).convert('L')).astype(int)


def edge_magnitude(run, frames):
    native, reference = gray(run / 'native.png'), gray(run / 'reference.png')
    k = PPI / 72
    out = []
    for f in frames:
        y1, x1, y2, x2 = f['bounds']
        sl = (slice(int((y1 - EDGE_PAD_PT) * k), int((y2 + EDGE_PAD_PT) * k)),
              slice(int((x1 - EDGE_PAD_PT) * k), int((x2 + EDGE_PAD_PT) * k)))
        d = numpy.abs(native[sl] - reference[sl])
        signed = native[sl] - reference[sl]
        out.append({'key': f['key'], 'diff_px': int((d > 0).sum()), 'max_diff': int(d.max()),
                    'over_64': int((d > 64).sum()), 'over_128': int((d > 128).sum()),
                    'net_signed_sum': int(signed.sum())})
    return out


def frame_of(rect, frames, page):
    if (rect[2] - rect[0]) * (rect[3] - rect[1]) > 0.9 * page.width * page.height:
        return None
    cx, cy = (rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2
    for f in frames:
        y1, x1, y2, x2 = f['bounds']
        if x1 - 15 <= cx <= x2 + 15 and y1 - 15 <= cy <= y2 + 15:
            return f['key']
    return '?'


def frame_order(events):
    """events: [(kind, width)] in painting order for one frame, kind 'stroke' or 'fill'.
    Returns strokes drawn before the first fill and strokes drawn after it."""
    first_fill = next((i for i, (kind, _) in enumerate(events) if kind == 'fill'), len(events))
    before = [w for kind, w in events[:first_fill] if kind == 'stroke']
    after = [w for kind, w in events[first_fill:] if kind == 'stroke']
    return before, after


def pdf_record(pdf, run, frames):
    doc = pymupdf.open(pdf)
    page = doc[0]
    events = []
    for d in page.get_drawings():
        kind = {'s': 'stroke', 'f': 'fill', 'fs': 'fill+stroke'}[d['type']]
        events.append((d['seqno'], kind, d.get('width'), frame_of(d['rect'], frames, page.rect)))
    for s in page.get_texttrace():
        kind = {0: 'fill', 1: 'stroke', 2: 'fill+stroke', 3: 'invisible'}[s['type']]
        events.append((s['seqno'], 'text-' + kind, s.get('linewidth'), frame_of(s['bbox'], frames, page.rect)))
    events.sort(key=lambda e: e[0])
    per_frame = {}
    for _, kind, width, key in events:
        if key is None:
            continue
        simple = 'stroke' if kind in ('stroke', 'text-stroke') else 'fill'
        per_frame.setdefault(key, []).append((simple, None if width is None else round(width, 3), kind))
    order = []
    for f in frames:
        seq = per_frame.get(f['key'], [])
        before, after = frame_order([(k, w) for k, w, _ in seq])
        order.append({'key': f['key'], 'events': [k for _, _, k in seq],
                      'strokes_before_first_fill': before, 'strokes_after_first_fill': after})
    pix = page.get_pixmap(dpi=PPI, colorspace=pymupdf.csGRAY, alpha=False)
    render = numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width).astype(int)
    reference = gray(run / 'reference.png')
    h = min(render.shape[0], reference.shape[0])
    mask = numpy.asarray(Image.open(run / 'mask.png').convert('L'))[:h] >= 254
    core = ndimage.binary_erosion(mask, iterations=ERODE)
    diff = numpy.abs(render[:h] - reference[:h])
    fonts = sorted({f[3].split('+', 1)[-1] for f in page.get_fonts()})
    return {'sha256': sha(pdf), 'text': page.get_text().split(), 'fonts': fonts,
            'unknown_frame_events': sum(1 for e in events if e[3] == '?'),
            'frame_order': order,
            'render_vs_reference': {'renderer': 'MuPDF ' + pymupdf.VersionBind, 'dpi': PPI, 'erode_px': ERODE,
                                    'core_px': int(core.sum()), 'core_over_64': int((core & (diff > 64)).sum()),
                                    'core_max_diff': int(diff[core].max())}}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--plugin-sha', required=True)
    parser.add_argument('--pdf', type=Path)
    args = parser.parse_args(argv)
    host = json.loads((args.run / 'host.json').read_text(encoding='utf-8'))
    base_host = json.loads((args.baseline / 'host.json').read_text(encoding='utf-8'))
    if not (host.get('passed') and base_host.get('passed')):
        raise SystemExit('a census collection did not pass')
    frames = json.loads((args.run / 'manifest.json').read_text(encoding='utf-8'))['frames']
    routew = compare(args.run / 'native.png', args.run / 'reference.png', args.run / 'manifest.json', args.run / 'mask.png')
    record = {
        'schema': 1, 'host': 'InDesign', 'version': host['version'], 'plugin': 'route W',
        'plugin_sha256': args.plugin_sha, 'ppi': PPI,
        'probe': 'tools/jsx/probe-victim-census.jsx', 'collection_passed': True,
        'routew_native_vs_reference': routew,
        'noplugin_native_vs_reference': compare(args.baseline / 'native.png', args.baseline / 'reference.png',
                                                args.baseline / 'manifest.json', args.baseline / 'mask.png'),
        'noplugin_native_vs_routew_native': compare(args.baseline / 'native.png', args.run / 'native.png',
                                                    args.run / 'manifest.json', args.run / 'mask.png'),
        'routew_edge_magnitude': edge_magnitude(args.run, frames),
        'fill_core_zero_all_frames': all(f['interior_changed_px'] == 0 for f in routew['frames']),
        'artifact_sha256': {name: sha(args.run / name) for name in
                            ('fixture.indd', 'manifest.json', 'mask.png', 'native.png', 'reference.png')},
        'baseline_artifact_sha256': {name: sha(args.baseline / name) for name in ('native.png', 'reference.png', 'mask.png')},
    }
    if args.pdf:
        record['pdf'] = pdf_record(args.pdf, args.run, frames)
    args.output.write_bytes((json.dumps(record, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    print('fill core zero in all frames:', record['fill_core_zero_all_frames'])
    if args.pdf:
        r = record['pdf']['render_vs_reference']
        print('pdf fonts', record['pdf']['fonts'], 'core over 64:', r['core_over_64'])


if __name__ == '__main__':
    main()
