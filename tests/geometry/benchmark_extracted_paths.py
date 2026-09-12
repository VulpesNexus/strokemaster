"""Provisional local-join statistics on extracted contours, without host operations.

This does not identify glyphs, recover centerlines from expanded Appearance,
resolve holes, or compare rendered strokes. The caller supplies actual half-width.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import stroke_outline as outline


def fingerprint(path, canonical=False):
    points = tuple(tuple(p[k][i] for k in ('a', 'l', 'r') for i in (0, 1)) for p in path['pts'])
    if canonical and points:
        reversed_points = tuple((p[0], p[1], p[4], p[5], p[2], p[3]) for p in reversed(points))
        def start_at_minimum(seq):
            smallest = min(seq)
            return min(seq[i:]+seq[:i] for i, p in enumerate(seq) if p == smallest)
        if path.get('closed'):
            points = min(start_at_minimum(points), start_at_minimum(reversed_points))
        else:
            points = min(points, reversed_points)
    data = json.dumps([path.get('closed'), points], separators=(',', ':'), allow_nan=False)
    return hashlib.sha256(data.encode()).hexdigest()


def benchmark(source, half_width):
    if not math.isfinite(half_width) or half_width <= 0:
        raise ValueError('half-width must be finite and positive')
    raw = source.read_bytes()
    document = json.loads(raw.decode('utf-8-sig'))
    paths = document['paths']
    ordered, canonical = defaultdict(list), defaultdict(list)
    for path in paths:
        ordered[fingerprint(path)].append(path)
        canonical[fingerprint(path, True)].append(path)
    unique = [group[0] for group in canonical.values()]
    counts = Counter()
    failures = Counter()
    failure_paths = set()
    failure_anchor_counts = Counter()
    failure_examples = []
    ratios, clip_axial, native_axial, clip_radial, native_radial = [], [], [], [], []
    plane_error = line_error = 0.0
    cap = half_width*math.sqrt(2)
    for path in unique:
        if not path.get('closed'):
            counts['open_contours_excluded'] += 1
            continue
        flat = outline.flatten(path['pts'])
        counts['input_anchors'] += len(path['pts'])
        counts['flattened_vertices'] += len(flat)
        for i, (x, y, corner) in enumerate(flat):
            if not corner:
                counts['smooth_vertices_excluded'] += 1
                continue
            counts['corner_candidates'] += 1
            prev, nxt = flat[(i-1) % len(flat)], flat[(i+1) % len(flat)]
            u = outline.unit((x-prev[0], y-prev[1]))
            w = outline.unit((nxt[0]-x, nxt[1]-y))
            if u is None or w is None:
                counts['zero_or_tiny_adjacent_chords_excluded'] += 1
                continue
            try:
                clip = outline.join_polygon((x, y), u, w, half_width, 'clip')
                native = outline.join_polygon((x, y), u, w, half_width, 'miter', limit=4)
            except ValueError as error:
                failures[str(error)] += 1
                failure_paths.add(path.get('index'))
                failure_anchor_counts[len(path['pts'])] += 1
                if len(failure_examples) < 5:
                    failure_examples.append({'path_index': path.get('index'), 'flat_index': i,
                                             'vertex': [x, y], 'error': str(error)})
                continue
            if clip is None:
                counts['straight_chord_pairs_excluded'] += 1
                continue
            counts['valid_local_joins'] += 1
            is_clip = len(clip) == 5
            is_bevel = len(native) == 3
            counts['clip_flat' if is_clip else 'clip_retained_miter'] += 1
            counts['native_bevel' if is_bevel else 'native_miter'] += 1
            counts[('flat_clip' if is_clip else 'retained_clip') +
                   ('_vs_native_bevel' if is_bevel else '_vs_native_miter')] += 1
            # Independent measurements on the returned vertices in a local frame.
            bx, by = u[0]-w[0], u[1]-w[1]
            length = math.hypot(bx, by)
            bx, by = bx/length, by/length
            def extents(poly):
                axial = max((p[0]-x)*bx+(p[1]-y)*by for p in poly)
                radial = max(math.hypot(p[0]-x, p[1]-y) for p in poly)
                return axial, radial
            ca, cr = extents(clip)
            na, nr = extents(native)
            clip_axial.append(ca); clip_radial.append(cr)
            native_axial.append(na); native_radial.append(nr)
            ratios.append(2/math.hypot(u[0]+w[0], u[1]+w[1]))
            if ca < na-1e-10:
                counts['axial_shorter_than_native'] += 1
            elif ca > na+1e-10:
                counts['axial_longer_than_native'] += 1
            else:
                counts['axial_same_as_native_within_1e_10'] += 1
            if cr > cap+1e-10:
                counts['clip_radial_exceeds_axial_cap'] += 1
            if is_clip:
                side = 1 if u[0]*w[1]-u[1]*w[0] < 0 else -1
                for p, d in ((clip[2], u), (clip[3], w)):
                    px, py = p[0]-x, p[1]-y
                    plane_error = max(plane_error, abs(px*bx+py*by-cap))
                    line_error = max(line_error, abs(px*(-side*d[1])+py*(side*d[0])-half_width))
    def summary(values):
        values = sorted(values)
        return {'min': values[0], 'median': values[len(values)//2],
                'p95_nearest_rank': values[math.ceil(0.95*len(values))-1], 'max': values[-1]} if values else None
    return {
        'status': 'PROVISIONAL_JOIN_ONLY',
        'source': str(source), 'source_sha256': hashlib.sha256(raw).hexdigest(), 'source_bytes': len(raw),
        'extractor_metadata': {k: v for k, v in document.items() if k != 'paths'},
        'half_width': half_width, 'full_width': 2*half_width, 'clip_ratio': math.sqrt(2),
        'axial_cap': cap, 'native_model_limit': 4,
        'width_provenance': 'Takeover supervisor PDF census measured w=3, j=0, M=4; PDF not reparsed by this benchmark',
        'raw_contours': len(paths), 'ordered_geometry_fingerprints': len(ordered),
        'cyclic_and_reversal_geometry_fingerprints': len(canonical),
        'canonical_multiplicity_histogram': dict(sorted(Counter(map(len, canonical.values())).items())),
        'closed_histogram': dict(Counter(str(p.get('closed')) for p in unique)),
        'stroke_width_histogram': dict(Counter(str(p.get('strokeWidth')) for p in unique)),
        'area_sign_histogram': dict(Counter('negative' if p.get('area', 0)<0 else
                                          'positive' if p.get('area', 0)>0 else 'zero' for p in unique)),
        'counts': dict(sorted(counts.items())), 'explicit_join_failures': dict(failures),
        'contours_with_join_failures': len(failure_paths),
        'failed_joins_by_contour_anchor_count': dict(sorted(failure_anchor_counts.items())),
        'failure_examples_first_five': failure_examples,
        'unclipped_miter_ratio': summary(ratios),
        'clip_axial_extent': summary(clip_axial), 'native_axial_extent': summary(native_axial),
        'clip_radial_extent': summary(clip_radial), 'native_radial_extent': summary(native_radial),
        'max_flat_plane_residual': plane_error, 'max_offset_line_residual': line_error,
        'limitations': [
            'Exact coordinate fingerprints ignore index, bounds, area, and style; no rounding or translation deduplication.',
            'Cyclic starts and traversal reversal swap incoming/outgoing handles for fingerprinting.',
            'Corner tags use anchor handles; measured joins use flattened chords (24 fixed subdivisions).',
            'All joins are hypothetical strokes around extracted contours, which may already contain expanded Appearance.',
            'No whole glyph, hole topology, visible area, raster, or actual host stroke comparison is established.',
            'Area sign is descriptive only; it does not identify holes without containment and fill rules.'
        ]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--half-width', type=float, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = json.dumps(benchmark(args.source, args.half_width), indent=2, allow_nan=False)+'\n'
    if args.output:
        args.output.write_text(result, encoding='utf-8')
    else:
        print(result, end='')


if __name__ == '__main__':
    main()
