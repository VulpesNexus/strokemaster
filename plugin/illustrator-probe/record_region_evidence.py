"""Writes the portable region host record from raw local evidence.

Reads the pipe-delimited probe results and measurer reports under
build/host-census/<run folder>/ and writes tests/audit/results/<name>.json with
no paths, document names, or machine identity. The raw files stay local.

Usage: record_region_evidence.py <run folder> <output name> <host version> <host build> [scope]
"""
import collections
import hashlib
import json
import sys
from pathlib import Path

import pikepdf
import pymupdf

root = Path(__file__).resolve().parents[2]
folder = root / 'build/host-census' / sys.argv[1]
output = root / 'tests/audit/results' / (sys.argv[2] + '.json')
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()


def probe(name):
    text = (folder / name).read_text(encoding='utf-8')
    status, *fields = text.split('|')
    record = {'status': status}
    for item in fields:
        key, _, value = item.partition('=')
        if key == 'diagnostics':
            continue  # the event ring is local detail; its counters are repeated in status fields
        record[key] = json.loads(value) if value[:1] in '"-0123456789tf' and value not in ('true', 'false') and _is_json(value) else value
    return record


def _is_json(value):
    try:
        json.loads(value)
        return True
    except ValueError:
        return False


def measurement(name):
    report = json.loads((folder / name).read_text(encoding='utf-8'))
    keep = ('status', 'checks', 'pdf_sha256', 'coordinate_tolerance', 'area_tolerance', 'gray', 'paint_type',
            'painted_min_x', 'painted_area', 'area_error', 'vertex_errors', 'split_fill_stroke_paint', 'render_hints',
            'escaping_probe_inside', 'area_evidence', 'silhouette_explicit_in_pdf', 'operators')
    return {'status': report['status'],
            **{role: {k: report[role][k] for k in keep if k in report[role]} for role in ('effect', 'control')}}


# Paint-relevant PDF operators, in stream order, with paths collapsed to one
# token: enough to read fill/stroke order without copying any glyph outline.
PAINT = {'BT', 'ET', 'Tj', 'TJ', 'Tr', 'f', 'f*', 'F', 'S', 's', 'B', 'B*', 'b', 'b*', 'W', 'W*', 'n', 'q', 'Q'}


def pdf_summary(path):
    pdf = pikepdf.open(path)
    sequence, counts = [], collections.Counter()
    for operands, operator in pikepdf.parse_content_stream(pdf.pages[0]):
        op = str(operator)
        counts[op] += 1
        if op in ('m', 'l', 'c', 'v', 'y', 're', 'h'):
            if not sequence or sequence[-1] != 'path':
                sequence.append('path')
        elif op == 'Tr':
            sequence.append('Tr' + str(operands[0]))
        elif op in PAINT:
            sequence.append(op)
    with pymupdf.open(path) as doc:
        page = doc[0]
        text_chars = len(page.get_text().strip())
        fonts = len(page.get_fonts())
        drawings = [d['type'] for d in page.get_drawings()]
    return {'sha256': digest(path), 'bytes': path.stat().st_size, 'extractedTextChars': text_chars, 'fonts': fonts,
            'drawings': drawings, 'operatorCounts': dict(sorted(counts.items())), 'paintSequence': sequence}


binary = root / 'plugin/illustrator-probe/build/region-native/StrokeMasterRegionProbe.aip'
artifact = json.loads((root / 'plugin/illustrator-probe/build/region-native/artifact.json').read_text(encoding='utf-8'))
record = {
    'schema': 1,
    'host': {'application': 'Adobe Illustrator', 'version': sys.argv[3], 'build': sys.argv[4], 'platform': 'Windows x64'},
    'plugin': {'name': artifact['plugin'], 'build': artifact['build'], 'sha256': digest(binary),
               'bytes': binary.stat().st_size, 'coreHeaderSha256': artifact['coreHeaderSha256'],
               'coreCppSha256': artifact['coreCppSha256']},
    'scope': sys.argv[5] if len(sys.argv) > 5 else
             'Marked analytic closed-region fixture only (square shell, one triangular hole, 0.6 pt equal-gray miter '
             'stroke, limit 4). Not arbitrary artwork, curves, text, or live style synchronization.',
    'probes': {},
    'measurements': {},
    'pdfs': {},
}
for path in sorted(folder.glob('*-result.txt')):
    record['probes'][path.name[:-len('-result.txt')]] = probe(path.name)
# The plugin identity comes from the current build output, so refuse to label a
# run that reported a different build.
for name, result in record['probes'].items():
    reported = result.get('build')
    if reported is not None and reported != artifact['build']:
        raise SystemExit(f'{name} ran build {reported}, but the current artifact is {artifact["build"]}')
for path in sorted(folder.glob('*-measure.json')):
    record['measurements'][path.name[:-len('-measure.json')]] = measurement(path.name)
for path in sorted(folder.glob('*.pdf')):
    record['pdfs'][path.name] = pdf_summary(path)
output.write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n', encoding='utf-8', newline='\n')
print(output.relative_to(root))
