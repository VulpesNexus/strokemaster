"""Bounded embedded-TrueType contour census; no font bytes or outlines are written.

Supports direct page text, explicit WinAnsiEncoding, and printable ASCII text.
Forms, composite PDF fonts, and unsupported text-show operations fail explicitly.
Counts alone cannot prove geometry identity or diagnose Appearance expansion.
"""
import argparse
from collections import Counter
from io import BytesIO
import hashlib
import json
from pathlib import Path

import fontTools
from fontTools.pens.recordingPen import DecomposingRecordingPen, DecomposingRecordingPointPen
from fontTools.ttLib import TTFont
import pikepdf


def contour_point_statistics(glyph_set, name):
    """Count explicit glyf points (on- and off-curve), not curve segments or implied points."""
    pen = DecomposingRecordingPointPen(glyph_set, skipMissingComponents=False)
    glyph_set[name].drawPoints(pen)
    histogram, two_point = Counter(), Counter()
    current = None
    for operation, args, _ in pen.value:
        if operation == 'beginPath':
            if current is not None:
                raise ValueError('nested point contour')
            current = []
        elif operation == 'addPoint':
            if current is None:
                raise ValueError('point outside contour')
            if args[1] == 'move':
                raise ValueError('open point contour')
            current.append((tuple(args[0]), args[1]))
        elif operation == 'endPath':
            if current is None or not current:
                raise ValueError('empty or unbalanced point contour')
            histogram[len(current)] += 1
            if len(current) == 2:
                two_point['coincident' if current[0][0] == current[1][0] else 'distinct_coordinates'] += 1
                two_point['both_on_curve' if all(kind is not None for _, kind in current) else 'has_off_curve'] += 1
            current = None
        else:
            raise ValueError('unsupported decomposed point operation: '+operation)
    if current is not None:
        raise ValueError('unclosed point contour')
    return {'explicit_points_per_contour': dict(sorted(histogram.items())),
            'single_point_contours': histogram[1], 'two_point_contours': histogram[2],
            'two_point_details': dict(sorted(two_point.items()))}


def aggregate_points(rows):
    histogram, details = Counter(), Counter()
    for row in rows:
        histogram.update(row['point_statistics']['explicit_points_per_contour'])
        details.update(row['point_statistics']['two_point_details'])
    return {'explicit_points_per_contour': dict(sorted(histogram.items())),
            'single_point_contours': histogram[1], 'two_point_contours': histogram[2],
            'two_point_details': dict(sorted(details.items()))}


def glyph_counts(font, text):
    cmap = font.getBestCmap()
    if cmap is None:
        raise ValueError('embedded font has no supported Unicode cmap')
    glyph_set = font.getGlyphSet()
    rows = []
    for character in text:
        if not 32 <= ord(character) <= 126:
            raise ValueError('only printable ASCII WinAnsi text is supported')
        name = cmap.get(ord(character))
        if name is None:
            raise ValueError('character missing from embedded font cmap')
        pen = DecomposingRecordingPen(glyph_set, skipMissingComponents=False)
        glyph_set[name].draw(pen)
        operations = Counter(operation for operation, _ in pen.value)
        if operations['endPath'] or operations['moveTo'] != operations['closePath']:
            raise ValueError('decomposed glyph contains open or unbalanced contours')
        point_statistics = contour_point_statistics(glyph_set, name)
        if sum(point_statistics['explicit_points_per_contour'].values()) != operations['moveTo']:
            raise ValueError('point and segment contour counts disagree')
        rows.append({'character': character, 'glyph_name': name, 'point_statistics': point_statistics,
                     'contours': operations['moveTo'], 'operations': dict(sorted(operations.items()))})
    return rows


def decode_show(operands, operator):
    if operator == 'Tj':
        strings = [operands[0]]
    elif operator == 'TJ':
        strings = [item for item in operands[0] if isinstance(item, pikepdf.String)]
    else:
        raise ValueError('unsupported text-show operator: '+operator)
    raw = b''.join(bytes(value) for value in strings)
    if any(value < 32 or value > 126 for value in raw):
        raise ValueError('only printable ASCII WinAnsi text is supported')
    return raw.decode('ascii')


def source_font_census(paths, texts):
    """Read local fonts without recording their filenames, paths, or binary contents."""
    by_hash = {}
    for path in paths:
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest in by_hash:
            by_hash[digest]['identical_input_files'] += 1
            continue
        with TTFont(BytesIO(data)) as font:
            if 'glyf' not in font:
                raise ValueError('source comparison requires glyf-based TrueType')
            row = {'font_sha256': digest, 'font_bytes': len(data), 'identical_input_files': 1,
                   'family': font['name'].getDebugName(1),
                   'version': font['name'].getDebugName(5), 'units_per_em': font['head'].unitsPerEm,
                   'text_runs': []}
            for text in sorted(set(texts)):
                glyphs = glyph_counts(font, text)
                row['text_runs'].append({'text': text, 'glyphs': glyphs, 'point_statistics': aggregate_points(glyphs),
                                        'decomposed_contours': sum(g['contours'] for g in glyphs)})
            by_hash[digest] = row
    return list(by_hash.values())


def census(pdf_path, extraction_path, source_fonts=()):
    fonts, shows = {}, []
    with pikepdf.open(pdf_path) as pdf:
        for page_number, page in enumerate(pdf.pages, 1):
            resources = page.Resources
            font_resources = resources.get('/Font', {})
            state = {'font': None, 'size': None, 'render_mode': 0, 'stroke_width': 1,
                     'join': 0, 'miter_limit': 10}
            stack = []
            for instruction in pikepdf.parse_content_stream(page):
                op, args = str(instruction.operator), instruction.operands
                if op == 'Do':
                    raise ValueError('XObject invocation unsupported; refusing incomplete census')
                if op == 'gs':
                    # An ExtGState may override the font or stroke parameters.
                    ext = resources.get('/ExtGState', {}).get(str(args[0]))
                    if ext is None:
                        raise ValueError('missing ExtGState resource')
                    if '/Font' in ext:
                        raise ValueError('ExtGState font override unsupported')
                    for key, target in (('/LW', 'stroke_width'), ('/LJ', 'join'), ('/ML', 'miter_limit')):
                        if key in ext:
                            state[target] = float(ext[key])
                elif op == 'q':
                    stack.append(state.copy())
                elif op == 'Q':
                    if not stack:
                        raise ValueError('unbalanced graphics state')
                    state = stack.pop()
                elif op == 'Tf':
                    state['font'], state['size'] = str(args[0]), float(args[1])
                elif op in ('Tr', 'w', 'j', 'M'):
                    state[{'Tr': 'render_mode', 'w': 'stroke_width', 'j': 'join', 'M': 'miter_limit'}[op]] = float(args[0])
                elif op in ("'", '"'):
                    raise ValueError('text-show quote operators unsupported')
                elif op in ('Tj', 'TJ'):
                    resource = state['font']
                    if resource is None or resource not in font_resources:
                        raise ValueError('text show without a known selected font')
                    font_object = font_resources[resource]
                    if str(font_object.get('/Subtype')) != '/TrueType' or str(font_object.get('/Encoding')) != '/WinAnsiEncoding':
                        raise ValueError('requires simple TrueType with explicit WinAnsiEncoding')
                    descriptor = font_object.get('/FontDescriptor', {})
                    if '/FontFile2' not in descriptor:
                        raise ValueError('font has no embedded TrueType FontFile2 stream')
                    data = descriptor['/FontFile2'].read_bytes()
                    digest = hashlib.sha256(data).hexdigest()
                    text = decode_show(args, op)
                    with TTFont(BytesIO(data)) as font:
                        if 'glyf' not in font:
                            raise ValueError('embedded font is not a glyf-based TrueType font')
                        rows = glyph_counts(font, text)
                        fonts[digest] = {'base_font': str(font_object.get('/BaseFont')),
                                         'pdf_subtype': str(font_object.get('/Subtype')),
                                         'pdf_encoding': str(font_object.get('/Encoding')),
                                         'font_descriptor_flags': int(descriptor.get('/Flags', 0)),
                                         'units_per_em': font['head'].unitsPerEm,
                                         'embedded_font_bytes': len(data), 'embedded_font_sha256': digest}
                    shows.append({'page': page_number, 'operator': op, 'text': text,
                                  'font_sha256': digest, 'pdf_state': state.copy(),
                                  'glyphs': rows, 'point_statistics': aggregate_points(rows), 'decomposed_contours': sum(row['contours'] for row in rows)})
            if stack:
                raise ValueError('unbalanced graphics state at page end')
    if not shows:
        raise ValueError('no supported text shows found')
    extraction_raw = extraction_path.read_bytes()
    extraction = json.loads(extraction_raw.decode('utf-8-sig'))
    paths = extraction['paths']
    # Ordered exact coordinate identity, excluding index and descriptive metadata.
    fingerprints = {hashlib.sha256(json.dumps([p.get('closed'), p['pts']], sort_keys=True,
                    separators=(',', ':'), allow_nan=False).encode()).hexdigest() for p in paths}
    comparisons = []
    for text, digest in sorted({(show['text'], show['font_sha256']) for show in shows}):
        occurrences = [show for show in shows if show['text'] == text and show['font_sha256'] == digest]
        total = occurrences[0]['decomposed_contours']
        comparisons.append({'text': text, 'font_sha256': digest,
                            'text_show_occurrences': len(occurrences),
                            'embedded_contours_per_text_run': total,
                            'extracted_contours': len(paths),
                            'embedded_minus_extracted_per_run': total-len(paths),
                            'count_identity': total == len(paths),
                            'point_statistics_per_run': occurrences[0]['point_statistics'],
                            'hypothetical_remove_all_two_point_contours': {
                                'remaining_embedded_contours': total-occurrences[0]['point_statistics']['two_point_contours'],
                                'remaining_minus_extracted': total-occurrences[0]['point_statistics']['two_point_contours']-len(paths),
                                'is_actual_removal_mapping': False}})
    return {'status': 'RESEARCH_COUNTS_ONLY', 'fontTools_version': fontTools.__version__,
            'pikepdf_version': pikepdf.__version__,
            'pdf_path': str(pdf_path), 'pdf_sha256': hashlib.sha256(pdf_path.read_bytes()).hexdigest(),
            'extraction_path': str(extraction_path),
            'extraction_sha256': hashlib.sha256(extraction_raw).hexdigest(),
            'extracted_contours': len(paths), 'extracted_ordered_unique_geometry': len(fingerprints),
            'extracted_anchors': sum(len(p['pts']) for p in paths),
            'extracted_anchor_count_histogram': dict(sorted(Counter(len(p['pts']) for p in paths).items())),
            'extracted_two_anchor_contours': sum(len(p['pts']) == 2 for p in paths),
            'fonts': list(fonts.values()), 'text_shows': shows, 'per_run_comparisons': comparisons,
            'source_fonts_without_paths': source_font_census(source_fonts, [show['text'] for show in shows]),
            'contours_summed_over_paint_occurrences': sum(show['decomposed_contours'] for show in shows),
            'limitations': [
                'Repeated text paint occurrences are reported separately; their contours are not treated as unique glyph geometry.',
                'Per-run comparisons assume the extraction corresponds to one specified run; no spatial correspondence is established.',
                'Nonmatching counts reject exact contour-count identity, not geometric region equivalence.',
                'Explicit TrueType point counts include off-curve controls; Illustrator anchor counts describe another representation.',
                'Subtracting two-point contours is a hypothetical count calculation, not an observed removal correspondence.',
                'A two-point closed contour with distinct on-curve points has zero fill area but may contribute stroked geometry.',
                'Bounds, units per em, contour counts, and matching names do not establish source-font identity.',
                'Subset quantization, contour cleanup, curve conversion, source font differences, or Appearance remain untested explanations.',
                'Text transforms, glyph placement, and contour coordinates are not compared; optional source-font checks cover metadata and counts only.',
                'Stroke values are raw PDF graphics state, not device-space effective widths under arbitrary transforms.',
                'Embedded font bytes are read in memory and hashed; no font program or outline coordinates are exported.'
            ]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pdf', type=Path, required=True)
    parser.add_argument('--extraction', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--source-font', type=Path, action='append', default=[],
                        help='optional local TrueType comparison; path is omitted from report')
    args = parser.parse_args()
    result = json.dumps(census(args.pdf, args.extraction, args.source_font), indent=2, allow_nan=False)+'\n'
    if args.output:
        args.output.write_text(result, encoding='utf-8')
    else:
        print(result, end='')


if __name__ == '__main__':
    main()
