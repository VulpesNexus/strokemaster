"""Record route W after screen drawing: a document drawn on screen, exported live and saved as a copy.

CLI: RUN SCREEN OUTPUT --gs GSWIN64C --work ASCII_DIR

RUN is the route W victim-census folder (its fixture.indd, split reference, fill mask, and the
census exports in pdf/ and eps/). SCREEN holds what tools/jsx/ise-export-open.jsx wrote from
that fixture while it was open in a window after the owner's screen check (live.pdf, live.eps,
live-600ppi.png, and the saved copy live.indd), plus what tools/jsx/ise-reexport.jsx wrote from
the copy (copy.pdf, copy.eps, copy-600ppi.png).

Route W mutes a stroke in one adornment pass and restores it in a later one. A screen draw
stopped between the two could leave the stroke muted in memory, where an export or a save would
find it. Each export is compared with the census export made before any screen drawing: the PNG
pixel for pixel, the PDF by its text and each frame's painting order, and the EPS by its
Ghostscript render. Each is also checked against the split reference inside the fill core.
"""
import argparse
import shutil
import json
import sys
from pathlib import Path

import numpy
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import record_routew_census as census  # noqa: E402
import record_routew_print as printing  # noqa: E402

PPI = 600


def rgb(path):
    return numpy.asarray(Image.open(path).convert('RGB')).astype(int)


def png_record(png, run):
    a, b = rgb(png), rgb(run / 'pdf/routew-600ppi.png')
    if a.shape != b.shape:
        raise SystemExit('%s and the census PNG differ in size' % png.name)
    return {'sha256': census.sha(png), 'px_differing_from_census_export': int((numpy.abs(a - b).max(axis=2) > 0).sum()),
            'fill_core_vs_reference': printing.victims(png, run, PPI)}


def pdf_record(pdf, run, frames, base):
    r = census.pdf_record(pdf, run, frames)
    return {'sha256': r['sha256'], 'text_same_as_census': r['text'] == base['text'], 'fonts': r['fonts'],
            'frames_ordered_unlike_census': [f['key'] for f, g in zip(r['frame_order'], base['frame_order']) if f != g],
            'strokes_before_first_fill': sum(len(f['strokes_before_first_fill']) for f in r['frame_order']),
            'strokes_after_first_fill': {f['key']: f['strokes_after_first_fill'] for f in r['frame_order'] if f['strokes_after_first_fill']},
            'render_vs_reference': r['render_vs_reference']}


def eps_record(eps, name, base_png, args):
    png = printing.render(args.gs, args.work, eps, name, PPI, True)
    a, b = rgb(png), rgb(base_png)
    return {'px_differing_from_census_eps': int((numpy.abs(a - b).max(axis=2) > 0).sum()) if a.shape == b.shape else None,
            'fill_core_vs_reference': printing.victims(png, args.run, PPI)}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('screen', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--gs', required=True)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args(argv)
    if not str(args.work).isascii():
        raise SystemExit('--work must be an ASCII path')
    frames = json.loads((args.run / 'manifest.json').read_text(encoding='utf-8'))['frames']
    base = census.pdf_record(args.run / 'pdf/routew.pdf', args.run, frames)
    base_eps = printing.render(args.gs, args.work, args.run / 'eps/routew.eps', 'census-eps', PPI, True)
    exports = {}
    for stem in ('live', 'copy'):
        exports[stem] = {'png': png_record(args.screen / (stem + '-600ppi.png'), args.run),
                         'pdf': pdf_record(args.screen / (stem + '.pdf'), args.run, frames, base),
                         'eps': eps_record(args.screen / (stem + '.eps'), stem + '-eps', base_eps, args)}
    record = {'schema': 1, 'host': 'InDesign', 'version': '21.5.1.73', 'plugin': 'route W',
              'plugin_sha256': 'f31dd2c9c7a4f7bd94ea9c9c1c48b19ba33fec76f2c3332730d1d82d7c4f4215',
              'screen_check': 'owner, by eye: VC00 at 400% zoom showed the first line whole; then scrolled and zoomed to interrupt redraws',
              'census_fixture_sha256': census.sha(args.run / 'fixture.indd'),
              'live': 'exported from the open window by tools/jsx/ise-export-open.jsx; document unmodified before and after',
              'copy': 'saveACopy of the open document, reopened without a window and re-exported by tools/jsx/ise-reexport.jsx',
              'exports': exports}
    args.output.write_bytes((json.dumps(record, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    for stem, e in exports.items():
        print(stem, 'png px off census', e['png']['px_differing_from_census_export'],
              '| pdf frames reordered', e['pdf']['frames_ordered_unlike_census'], 'text same', e['pdf']['text_same_as_census'],
              '| eps px off census', e['eps']['px_differing_from_census_eps'],
              '| core over 64 png/pdf/eps', sum(f['core_over_64'] for f in e['png']['fill_core_vs_reference']['frames']),
              e['pdf']['render_vs_reference']['core_over_64'], sum(f['core_over_64'] for f in e['eps']['fill_core_vs_reference']['frames']))
    shutil.rmtree(args.work, ignore_errors=True)


if __name__ == '__main__':
    main()
