"""Census of real glyph outlines through tools/curve_region.py and the topology core.

CLI: glyph_region_census.py FONT [FONT ...] --output census.json [--h 40] [--limit 4]
     [--tolerance 0.5] [--codepoints 33-126,161-255]

For each font file (read locally; no outline, name, or path is written to the output)
and each character, the glyph is drawn through a fontTools pen, quadratic segments are
raised to cubics exactly, contours are flattened and classified, and the core offsets the
result by h font units with the given miter limit. The output keeps only counts,
refusal reasons, and the core's own numeric checks per font, identified by the hash of
its file: fonts are not redistributed, so this is a local measurement recipe and its
aggregate result, not a portable fixture.
"""
import argparse
import collections
import hashlib
import json
import sys
from pathlib import Path

from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import curve_region as cr  # noqa: E402

TOPOLOGY_EXE = Path(__file__).resolve().parents[1] / 'tests/geometry/topology_build/topology_region.exe'


class ContourPen(BasePen):
    """Collects closed contours as (start, [('L', p) | ('C', c1, c2, p)])."""

    def __init__(self, glyphset):
        super().__init__(glyphset)
        self.contours, self.start, self.segments = [], None, []

    def _moveTo(self, p):
        self.start, self.segments = p, []

    def _lineTo(self, p):
        self.segments.append(('L', p))

    def _curveToOne(self, p1, p2, p3):
        self.segments.append(('C', p1, p2, p3))

    def _qCurveToOne(self, p1, p2):
        p0 = self._getCurrentPoint()
        c1 = (p0[0] + 2 / 3 * (p1[0] - p0[0]), p0[1] + 2 / 3 * (p1[1] - p0[1]))
        c2 = (p2[0] + 2 / 3 * (p1[0] - p2[0]), p2[1] + 2 / 3 * (p1[1] - p2[1]))
        self.segments.append(('C', c1, c2, p2))

    def _closePath(self):
        if self.start is not None and self.segments:
            self.contours.append((self.start, self.segments))
        self.start, self.segments = None, []

    _endPath = _closePath


def census(font_path, chars, h, limit, tolerance, run_batch):
    font = TTFont(font_path, lazy=True)
    glyphset = font.getGlyphSet()
    cmap = font.getBestCmap()
    outcome = collections.Counter()
    reasons = collections.Counter()
    refused = collections.defaultdict(list)  # reason class -> code points
    worst_lost = 0.0
    requests, labels = [], []
    for ch in chars:
        name = cmap.get(ord(ch))
        if name is None:
            outcome['no-glyph'] += 1
            continue
        pen = ContourPen(glyphset)
        glyphset[name].draw(pen)
        if not pen.contours:
            outcome['empty'] += 1
            continue
        try:
            rings = [cr.flatten_contour(start, segments, tolerance) for start, segments in pen.contours]
            polygons = cr.classify(rings)
        except cr.Refused as error:
            outcome['adapter-refused'] += 1
            reasons[str(error)] += 1
            refused['adapter'].append(ord(ch))
            continue
        requests.append(cr.request(polygons, h, limit))
        labels.append(ch)
    for ch, result in zip(labels, run_batch(requests) if requests else []):
        if result.get('ok'):
            outcome['core-ok'] += 1
            worst_lost = max(worst_lost, float(result.get('lost_fill_area_normalized', 0)))
        else:
            outcome['core-refused'] += 1
            reason = str(result.get('error', result.get('message', 'refused')))
            reasons[reason.split(';')[0][:120]] += 1
            refused['core'].append(ord(ch))
    return {'font_sha256': hashlib.sha256(Path(font_path).read_bytes()).hexdigest(),
            'units_per_em': font['head'].unitsPerEm, 'characters': len(chars),
            'outcome': dict(sorted(outcome.items())), 'refusal_reasons': dict(reasons.most_common()),
            'refused_codepoints': {k: sorted(v) for k, v in sorted(refused.items())},
            'worst_lost_fill_area_normalized': worst_lost}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fonts', type=Path, nargs='+')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--h', type=float, default=40)
    parser.add_argument('--limit', type=float, default=4)
    parser.add_argument('--tolerance', type=float, default=0.5)
    parser.add_argument('--codepoints', default='33-126',
                        help='comma-separated decimal code points or ranges, e.g. 33-126,161-255')
    args = parser.parse_args(argv)
    chars = []
    for part in args.codepoints.split(','):
        lo, _, hi = part.partition('-')
        chars.extend(chr(c) for c in range(int(lo), int(hi or lo) + 1))
    args.chars = ''.join(chars)
    if args.output.exists():
        raise SystemExit('refusing to overwrite the output')
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tests/geometry'))
    from test_topology_region import batch  # the core's reviewed CLI driver
    report = {'schema': 1, 'h_font_units': args.h, 'limit': args.limit, 'flatten_tolerance_font_units': args.tolerance,
              'topology_exe_sha256': hashlib.sha256(TOPOLOGY_EXE.read_bytes()).hexdigest(),
              'fonts': [census(f, args.chars, args.h, args.limit, args.tolerance, batch) for f in args.fonts]}
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps(report, indent=1))


if __name__ == '__main__':
    main()
