"""Writes the portable region host record from raw local evidence.

Reads the pipe-delimited probe results and measurer reports under
build/host-census/<run folder>/ and writes tests/audit/results/<name>.json with
no paths, document names, or machine identity. The raw files stay local.

Usage: record_region_evidence.py <run folder> <output name> <host version> <host build>
"""
import hashlib
import json
import sys
from pathlib import Path

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


binary = root / 'plugin/illustrator-probe/build/region-native/StrokeMasterRegionProbe.aip'
artifact = json.loads((root / 'plugin/illustrator-probe/build/region-native/artifact.json').read_text(encoding='utf-8'))
record = {
    'schema': 1,
    'host': {'application': 'Adobe Illustrator', 'version': sys.argv[3], 'build': sys.argv[4], 'platform': 'Windows x64'},
    'plugin': {'name': artifact['plugin'], 'build': artifact['build'], 'sha256': digest(binary),
               'bytes': binary.stat().st_size, 'coreHeaderSha256': artifact['coreHeaderSha256'],
               'coreCppSha256': artifact['coreCppSha256']},
    'scope': 'Marked analytic closed-region fixture only (square shell, one triangular hole, 0.6 pt equal-gray miter '
             'stroke, limit 4). Not arbitrary artwork, curves, text, or live style synchronization.',
    'probes': {},
    'measurements': {},
}
for path in sorted(folder.glob('*-result.txt')):
    record['probes'][path.name[:-len('-result.txt')]] = probe(path.name)
for path in sorted(folder.glob('*-measure.json')):
    record['measurements'][path.name[:-len('-measure.json')]] = measurement(path.name)
output.write_text(json.dumps(record, indent=2, ensure_ascii=False) + '\n', encoding='utf-8', newline='\n')
print(output.relative_to(root))
