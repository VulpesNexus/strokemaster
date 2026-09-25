"""Record route W across the saved InDesign fixture suites, without Adobe.

CLI: SURVEY OUTPUT

SURVEY is the folder written by re-exporting each suite's saved document with
tools/jsx/ise-reexport.jsx while route W was installed (one subfolder per suite, as listed in
SUITES), after tools/stroke_probe.py ran in each measured subfolder. Baselines and oracles are
the committed exports under tests/indesign-stroke:

- hidden fill (stroke_probe, MuPDF 720 dpi): core fill pixels covered by stroke, as exported
  without the plugin, with route W, and with the frame group's strokes moved first (the
  strokes-first oracle), per frame;
- miter spikes (suite B): stroke_probe's spike count and reach, without the plugin and with it;
- the strokes-first oracle PDFs of suites A and C (tools/make_expected.py) against the route W
  PDFs, both rendered by MuPDF at 300 dpi: pixels off by more than 128 levels per frame;
- live text: fill-mode text spans of every route W PDF against the committed export, and the
  stroke-mode text spans route W left native.
"""
import argparse
import collections
import json
from pathlib import Path

import numpy
import pymupdf

ROOT = Path(__file__).resolve().parents[1]
FIX = ROOT / 'tests' / 'indesign-stroke'
# suite folder in SURVEY, stem, committed folder of the no-plugin export, probe measured
SUITES = [('A', 'A-minimal', 'A-adjacent-line-overlap/actual-before-plugin', True),
          ('B', 'B-miter', 'B-miter-spikes/actual-before-plugin', True),
          ('C', 'C-attribute-sweep', 'C-run-fragmentation/actual-before-plugin', True),
          ('N', 'N-transparency', 'N-transparency/actual-noplugin', True),
          ('S', 'S-split', 'S-split/actual-noplugin', True)]
ORACLES = [('A', 'A-minimal', 'A-adjacent-line-overlap'), ('C', 'C-attribute-sweep', 'C-run-fragmentation')]
TEXT = [('D', 'D-contours', 'D-create-outlines/actual'), ('D', 'D-faithful', 'D-create-outlines/actual'),
        ('D', 'D-hook', 'D-create-outlines/actual'), ('D', 'D-outlines', 'D-create-outlines/actual'),
        ('T', 'T-livesync', 'T-livesync/actual'), ('corpus', 'fixture-plugin', 'corpus/actual'),
        ('A', 'A-minimal', 'A-adjacent-line-overlap/actual-before-plugin'),
        ('C', 'C-attribute-sweep', 'C-run-fragmentation/actual-before-plugin'),
        ('N', 'N-transparency', 'N-transparency/actual-noplugin'), ('S', 'S-split', 'S-split/actual-noplugin')]
SPIKE_KEYS = ('spike_px_beyond_weight', 'max_stroke_reach_pt')


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def hidden(survey):
    out = []
    for suite, stem, base, _ in SUITES:
        before = load(FIX / base / (stem + '.probe.json'))['frames']
        after = load(survey / suite / (stem + '.probe.json'))['frames']
        if [f['name'] for f in before] != [f['name'] for f in after]:
            raise SystemExit(suite + ': probe frames differ')
        frames = []
        for b, a in zip(before, after):
            rec = {'name': b['name']}
            if b.get('hidden_fill_px') is not None:
                rec.update(noplugin=b['hidden_fill_px'], routew=a.get('hidden_fill_px'),
                           oracle=b.get('hidden_fill_px_strokes_first'),
                           fill_core_noplugin=b.get('fill_core_px'), fill_core_routew=a.get('fill_core_px'))
            if any(k in b for k in SPIKE_KEYS):
                rec.update({k + '_noplugin': b.get(k) for k in SPIKE_KEYS})
                rec.update({k + '_routew': a.get(k) for k in SPIKE_KEYS})
            if len(rec) > 1:
                frames.append(rec)
        measured = [f for f in frames if 'noplugin' in f]
        out.append({'suite': suite, 'frames': frames,
                    'hidden_total': {k: sum(f[k] or 0 for f in measured) for k in ('noplugin', 'routew', 'oracle')}})
    return out


def render(path, dpi=300):
    with pymupdf.open(path) as doc:
        pix = doc[0].get_pixmap(dpi=dpi, colorspace=pymupdf.csRGB, alpha=False)
        return numpy.frombuffer(pix.samples, numpy.uint8).reshape(pix.height, pix.width, 3).astype(int)


def oracle(survey):
    out = []
    for suite, stem, folder in ORACLES:
        expected = render(FIX / folder / 'expected' / (stem + '-expected.pdf'))
        routew = render(survey / suite / (stem + '.pdf'))
        noplugin = render(FIX / folder / 'actual-before-plugin' / (stem + '.pdf'))
        k = 300 / 72
        frames = []
        for f in load(FIX / folder / 'actual' / (stem + '.manifest.json'))['frames']:
            y1, x1, y2, x2 = f['bounds']
            sl = (slice(int(y1 * k), int(y2 * k)), slice(int(x1 * k), int(x2 * k)))
            dr = numpy.abs(routew[sl] - expected[sl]).max(axis=2)
            dn = numpy.abs(noplugin[sl] - expected[sl]).max(axis=2)
            frames.append({'name': f['name'], 'noplugin_over_128': int((dn > 128).sum()),
                           'routew_over_128': int((dr > 128).sum()), 'routew_max': int(dr.max())})
        out.append({'suite': suite, 'dpi': 300, 'frames': frames})
    return out


def spans(path, kind):
    with pymupdf.open(path) as doc:
        return collections.Counter(''.join(chr(c[0]) for c in s['chars'])
                                   for s in doc[0].get_texttrace() if s['type'] == kind)


def text(survey):
    out = []
    for suite, stem, folder in TEXT:
        mine = survey / suite / (stem + '.pdf')
        out.append({'document': stem, 'fill_text_identical': spans(mine, 0) == spans(FIX / folder / (stem + '.pdf'), 0),
                    'fill_text_spans': sum(spans(mine, 0).values()),
                    'native_stroke_text_spans_left': sum(spans(mine, 1).values())})
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('survey', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args(argv)
    record = {'schema': 1, 'host': 'InDesign', 'version': '21.5.1.73', 'plugin': 'route W',
              'plugin_sha256': 'f31dd2c9c7a4f7bd94ea9c9c1c48b19ba33fec76f2c3332730d1d82d7c4f4215',
              'renderer': 'MuPDF ' + pymupdf.VersionBind,
              'hidden_fill': hidden(args.survey), 'strokes_first_oracle': oracle(args.survey), 'live_text': text(args.survey)}
    args.output.write_bytes((json.dumps(record, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    for s in record['hidden_fill']:
        print(s['suite'], s['hidden_total'])
    for s in record['strokes_first_oracle']:
        print(s['suite'], 'frames off oracle >128: no plugin', sum(1 for f in s['frames'] if f['noplugin_over_128']),
              'route W', sum(1 for f in s['frames'] if f['routew_over_128']))
    print('fill text identical in all:', all(t['fill_text_identical'] for t in record['live_text']))


if __name__ == '__main__':
    main()
