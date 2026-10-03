"""Bounded positive filled-region versus symmetric-boundary comparison on embedded fonts.

No source font, glyph coordinates, or result geometry is written. Unsupported or
ambiguous topology remains a per-glyph refusal. Native-host semantics are not measured.
"""
import argparse
from collections import Counter
from io import BytesIO
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

import pikepdf
from fontTools.agl import UV2AGL
from fontTools.cffLib import CFFFontSet
from fontTools.ttLib import TTFont

from glyph_region_census import ContourPen
import curve_region as cr

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'tests/geometry/topology_build/topology_region.exe'


class StrictContourPen(ContourPen):
    def _endPath(self):
        raise ValueError('open glyph contour is not a filled-region input')


def load_font(font_object):
    if str(font_object.get('/Encoding')) != '/WinAnsiEncoding':
        raise ValueError('only explicit WinAnsiEncoding is admitted')
    descriptor = font_object.get('/FontDescriptor', {})
    if '/FontFile2' in descriptor:
        data = descriptor.FontFile2.read_bytes()
        font = TTFont(BytesIO(data))
        cmap = font.getBestCmap()
        if cmap is None:
            raise ValueError('missing embedded cmap')
        upem = font['head'].unitsPerEm
        return font.getGlyphSet(), lambda c: cmap.get(ord(c)), 1/upem, data, 'TrueType'
    if '/FontFile3' in descriptor and str(descriptor.FontFile3.get('/Subtype')) == '/Type1C':
        data = descriptor.FontFile3.read_bytes()
        cff = CFFFontSet()
        cff.decompile(BytesIO(data), None)
        top = cff.topDictIndex[0]
        matrix = list(top.FontMatrix)
        if len(matrix) != 6 or matrix[1:3] != [0, 0] or matrix[4:] != [0, 0] or matrix[0] != matrix[3] or matrix[0] <= 0:
            raise ValueError('only positive isotropic zero-origin FontMatrix is admitted')
        return top.CharStrings, lambda c: UV2AGL.get(ord(c)), matrix[0], data, 'Type1C'
    raise ValueError('unsupported embedded font program')


def compare(pdf_path, text, size_pt, half_width_pt, tolerance_pt, limit):
    if not all(math.isfinite(x) and x > 0 for x in (size_pt, half_width_pt, tolerance_pt, limit)):
        raise ValueError('positive finite calibration required')
    report = {'schema':1, 'status':'BOUNDED_OFFLINE_REGION_COMPARISON', 'nativeExecuted':False,
        'pdf_sha256':hashlib.sha256(pdf_path.read_bytes()).hexdigest(),
        'core_sha256':hashlib.sha256(EXE.read_bytes()).hexdigest(),
        'size_pt':size_pt, 'half_width_pt':half_width_pt, 'tolerance_pt':tolerance_pt, 'limit':limit,
        'fonts':[], 'limitations':['Sizes and widths are explicit caller calibration, not inferred from Tf alone.',
        'Glyphs are processed independently; no story, run, or neighboring-glyph union is authorized.',
        'Core joins are Boost miter semantics, not certified Adobe miter/bevel semantics.',
        'Fill-rule resolution, cusp behavior, and native font mapping remain separate gates.']}
    with pikepdf.open(pdf_path) as pdf:
        if len(pdf.pages) != 1:
            raise ValueError('bounded reader requires exactly one direct page')
        for _, obj in pdf.pages[0].Resources.Font.items():
            glyphset, glyph_name, unit, data, kind = load_font(obj)
            scale = size_pt*unit
            h, tolerance = half_width_pt/scale, tolerance_pt/scale
            rows, requests = [], []
            for ch in sorted(set(text)):
                row = {'character':ch, 'status':'NOT_MEASURED'}
                rows.append(row)
                name = glyph_name(ch)
                if not name or name not in glyphset:
                    row.update(status='MAPPING_REFUSED', reason='missing glyph in embedded subset')
                    continue
                pen = StrictContourPen(glyphset)
                try:
                    glyphset[name].draw(pen)
                except ValueError as exc:
                    row.update(status='OUTLINE_REFUSED', reason=str(exc))
                    continue
                row['contours'] = len(pen.contours)
                if not pen.contours:
                    row.update(status='EMPTY_GLYPH_NO_REGION')
                    continue
                try:
                    rings = [cr.flatten_contour(a, segments, tolerance) for a, segments in pen.contours]
                    polygons = cr.classify(rings)
                    # No external envelope: compare the two proposed stroke semantics directly.
                    req = {'source':[cr.wkt(p) for p in polygons], 'h':h, 'limit':limit, 'compare_boundary':True}
                except (cr.Refused, ValueError) as exc:
                    row.update(status='ADAPTER_REFUSED', reason=str(exc))
                    continue
                row['request_index'] = len(requests)
                requests.append(req)
            if requests:
                proc = subprocess.run([str(EXE)],input=''.join(json.dumps(r)+'\n' for r in requests),
                    text=True,capture_output=True,timeout=60,check=True,
                    creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
                if proc.stderr:
                    raise RuntimeError('core emitted stderr')
                results=[json.loads(line) for line in proc.stdout.splitlines()]
                if len(results) != len(requests):
                    raise RuntimeError('incomplete core response census')
                for row in rows:
                    if 'request_index' not in row:
                        continue
                    result=results[row.pop('request_index')]
                    if not result.get('ok'):
                        row.update(status='CORE_REFUSED', reason=result.get('error','core refusal'))
                        continue
                    region=result['geometry']; symmetric=result['symmetric_geometry']
                    def bounds(polygons):
                        ps=[p for poly in polygons for ring in poly for p in ring]
                        return [min(p[0] for p in ps)*scale,min(p[1] for p in ps)*scale,
                            max(p[0] for p in ps)*scale,max(p[1] for p in ps)*scale]
                    row.update(status='CORE_OK',fill_loss_normalized=result['lost_fill_area_normalized'],
                        symmetric_excess_area_pt2=result['symmetric_excess_area_normalized']*(result['scale']*scale)**2,
                        region_bounds_pt=bounds(region),symmetric_bounds_pt=bounds(symmetric),
                        region_holes=result['holes'])
            report['fonts'].append({'font_sha256':hashlib.sha256(data).hexdigest(), 'kind':kind,
                'font_unit_to_pt':scale, 'h_font_units':h,'tolerance_font_units':tolerance,
                'outcomes':dict(Counter(r['status'] for r in rows)), 'glyphs':rows})
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pdf',type=Path,required=True)
    p.add_argument('--text',required=True)
    p.add_argument('--size-pt',type=float,required=True)
    p.add_argument('--half-width-pt',type=float,required=True)
    p.add_argument('--tolerance-pt',type=float,default=0.01)
    p.add_argument('--limit',type=float,default=4)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if a.output.exists():
        raise SystemExit('refusing existing evidence output')
    result=compare(a.pdf,a.text,a.size_pt,a.half_width_pt,a.tolerance_pt,a.limit)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps([f['outcomes'] for f in result['fonts']]))

if __name__=='__main__':
    main()
