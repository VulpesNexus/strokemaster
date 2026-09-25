"""Record route W on InDesign's print drawing path (EPS export, print to PostScript).

CLI: OUTPUT --gs GSWIN64C --work ASCII_DIR --n-noplugin DIR --n-routew DIR --victims-run DIR --victims-eps FILE

EPS and PostScript are written by InDesign's print drawing, not by PDF export, so route W's
correction there is a separate question. Both are rendered by Ghostscript at the given
resolution. Ghostscript cannot open a path containing this repository's en dash, so each file
is copied (a DOS EPS reduced to its PostScript section) into WORK, which must be ASCII.

- Suite N (transparency, blend modes, gradients, overprint, effects): the no-plugin EPS and
  PostScript kept beside the committed export (git-ignored: InDesign stamps the user name into
  their DSC header) against route W's, per frame with tools/compare_renders.py. N has almost no
  damage to correct, so this is the no-harm half.
- The victim census fixture: route W's EPS against the census's split reference and fill mask
  (600 ppi): fill-core pixels more than 64 levels off, per frame. This is the correction half.

Neither the EPS nor the PostScript files belong in the repository.
"""
import argparse
import contextlib
import io
import json
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy
from PIL import Image
from scipy import ndimage

sys.path.insert(0, str(Path(__file__).resolve().parent))
import compare_renders  # noqa: E402

N_MANIFEST = Path(__file__).resolve().parents[1] / 'tests/indesign-stroke/N-transparency/actual/N-transparency.manifest.json'


def render(gs, work, source, name, dpi, eps):
    work.mkdir(parents=True, exist_ok=True)
    data = Path(source).read_bytes()
    if data[:4] == b'\xc5\xd0\xd3\xc6':  # DOS EPS: offset and length of the PostScript section
        start, length = struct.unpack('<II', data[4:12])
        data = data[start:start + length]
    (work / (name + '.ps')).write_bytes(data)
    args = [gs, '-q', '-dSAFER', '-dBATCH', '-dNOPAUSE', '-sDEVICE=png16m', '-r%d' % dpi,
            '-dTextAlphaBits=4', '-dGraphicsAlphaBits=4'] + (['-dEPSCrop'] if eps else [])
    subprocess.run(args + ['-o', name + '.png', name + '.ps'], cwd=work, check=True)
    return work / (name + '.png')


def compare(a, b):
    with tempfile.TemporaryDirectory() as folder, contextlib.redirect_stdout(io.StringIO()):
        compare_renders.main(str(a), str(b), str(N_MANIFEST), folder + '/c', 300.0, None)
        report = json.loads(Path(folder + '/c.json').read_text(encoding='utf-8'))
    return {'page_changed_px': report['page_changed_px'],
            'frames': [{'name': f['name'], 'changed_px': f.get('changed_px'), 'max_channel_diff': f.get('max_channel_diff')}
                       for f in report['frames']]}


def victims(png, run, dpi):
    rendered = numpy.asarray(Image.open(png).convert('L')).astype(int)
    reference = numpy.asarray(Image.open(run / 'reference.png').convert('L')).astype(int)
    mask = numpy.asarray(Image.open(run / 'mask.png').convert('L')) >= 254
    if rendered.shape != reference.shape:
        raise SystemExit('EPS render and reference differ in size')
    core = ndimage.binary_erosion(mask, iterations=6)
    diff = numpy.abs(rendered - reference)
    k = dpi / 72
    frames = []
    for f in json.loads((run / 'manifest.json').read_text(encoding='utf-8'))['frames']:
        y1, x1, y2, x2 = f['bounds']
        sl = (slice(int(y1 * k), int(y2 * k)), slice(int(x1 * k), int(x2 * k)))
        c = core[sl]
        frames.append({'key': f['key'], 'core_px': int(c.sum()), 'core_over_64': int((c & (diff[sl] > 64)).sum()),
                       'core_max_diff': int(diff[sl][c].max()) if c.any() else 0})
    return {'dpi': dpi, 'erode_px': 6, 'frames': frames}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--gs', required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--n-noplugin', type=Path, required=True)
    parser.add_argument('--n-routew', type=Path, required=True)
    parser.add_argument('--victims-run', type=Path, required=True)
    parser.add_argument('--victims-eps', type=Path, required=True)
    args = parser.parse_args(argv)
    if not str(args.work).isascii():
        raise SystemExit('--work must be an ASCII path')
    version = subprocess.run([args.gs, '--version'], capture_output=True, text=True, check=True).stdout.strip()
    n = {}
    for kind in ('eps', 'ps'):
        a = render(args.gs, args.work, args.n_noplugin / ('N-transparency.' + kind), 'n-noplugin-' + kind, 300, kind == 'eps')
        b = render(args.gs, args.work, args.n_routew / ('N-transparency.' + kind), 'n-routew-' + kind, 300, kind == 'eps')
        n[kind] = compare(a, b)
    v = victims(render(args.gs, args.work, args.victims_eps, 'victims-routew-eps', 600, True), args.victims_run, 600)
    record = {'schema': 1, 'host': 'InDesign', 'version': '21.5.1.73', 'plugin': 'route W',
              'plugin_sha256': 'f31dd2c9c7a4f7bd94ea9c9c1c48b19ba33fec76f2c3332730d1d82d7c4f4215',
              'renderer': 'Ghostscript ' + version,
              'suite_n_noplugin_vs_routew': n, 'victims_eps_vs_reference': v}
    args.output.write_bytes((json.dumps(record, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    for kind in n:
        print('N', kind, 'page changed', n[kind]['page_changed_px'])
    print('victims EPS core over 64:', sum(f['core_over_64'] for f in v['frames']))
    shutil.rmtree(args.work, ignore_errors=True)


if __name__ == '__main__':
    main()
