"""Measure one bounded polygon fixture from effect/control PDFs, without Adobe.

CLI: effect.pdf control.pdf --output result.json --origin X Y --scale S
Origin is the fixture's (0,0) in PDF user-space points, with y increasing upward;
S is positive PDF points per fixture unit. Per-file origin/scale overrides exist.
The transform is supplied, never fitted from geometry. Selected pages only.

Supports an isolated opaque neutral-gray compound polygon, filled for the
effect and filled or filled+stroked for the control. Curves, clipping, XObjects,
text, images, annotations, transparency, custom color spaces, page rotation,
and unrecognized operators are refused. No whole-document or renderer proof.
Uses PyMuPDF vectors and a pikepdf operator census; no topology core imports.
References: https://pymupdf.readthedocs.io/en/latest/page.html#get-drawings
https://pikepdf.readthedocs.io/en/latest/api/filters.html
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path

import pikepdf
import pymupdf


class Unsupported(ValueError):
    """The PDF or supplied transform exceeds the measurement contract."""


def require(condition, message):
    if not condition:
        raise Unsupported(message)


def analytic_fixture():
    h = .3
    shift = h * math.sqrt(5)
    apex = .2 + shift
    delta = (h + shift) / 2
    bad = .2 - shift
    excursion = -.3 - bad
    outer = [(-.3, -.3), (10.3, -.3), (10.3, 10.3), (-.3, 10.3)]
    hole = [(apex, 5), (1.9, 4 + delta), (1.9, 6 - delta)]
    control_outer = outer + [(-.3, 5 + excursion / 2), (bad, 5),
                             (-.3, 5 - excursion / 2)]
    # Remove the ordinary closing upper-left -> lower-left edge in favor of
    # its two remaining segments and the measured escaping triangle.
    desired_area = 10.6**2 - (1.9 - apex) * (2 - 2 * delta) / 2
    return dict(source=[[(0, 0), (10, 0), (10, 10), (0, 10)],
                        [(.2, 5), (2.2, 4), (2.2, 6)]],
                effect=[outer, hole], control=[control_outer, hole],
                effect_area=desired_area, control_area=desired_area + excursion**2 / 2,
                escaping_triangle_area=excursion**2 / 2, control_min_x=bad,
                effect_min_x=-.3, excursion=excursion,
                escaping_probe=((bad - .3) / 2, 5), h=h, limit=4)


def signed_area(ring):
    ox, oy = ring[0]
    return math.fsum((a[0]-ox)*(b[1]-oy)-(b[0]-ox)*(a[1]-oy)
                     for a, b in zip(ring, ring[1:]+ring[:1])) / 2


def inside_ring(point, ring):
    x, y = point
    inside = False
    for a, b in zip(ring, ring[1:]+ring[:1]):
        if (a[1] > y) != (b[1] > y) and x < a[0]+(y-a[1])*(b[0]-a[0])/(b[1]-a[1]):
            inside = not inside
    return inside


def membership(point, rings, even_odd):
    hits = [r for r in rings if inside_ring(point, r)]
    return bool(len(hits) % 2) if even_odd else sum(1 if signed_area(r)>0 else -1 for r in hits) != 0


def cross(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])


def distance_to_segment(p, a, b):
    dx, dy = b[0]-a[0], b[1]-a[1]
    d = dx*dx+dy*dy
    t = max(0, min(1, ((p[0]-a[0])*dx+(p[1]-a[1])*dy)/d)) if d else 0
    return math.hypot(p[0]-a[0]-t*dx, p[1]-a[1]-t*dy)


def segments_touch(a, b, c, d):
    if cross(a,b,c)*cross(a,b,d) < 0 and cross(c,d,a)*cross(c,d,b) < 0:
        return True
    return min(distance_to_segment(a,c,d), distance_to_segment(b,c,d),
               distance_to_segment(c,a,b), distance_to_segment(d,a,b)) <= 1e-10


def validate_rings(rings):
    require(len(rings) == 2, "require exactly one outer ring and one hole")
    for ring in rings:
        require(3 <= len(ring) <= 256, "unsupported ring vertex count")
        require(all(math.isfinite(v) for p in ring for v in p), "nonfinite path coordinate")
        require(abs(signed_area(ring)) > 1e-10, "degenerate ring")
        edges = list(zip(ring, ring[1:]+ring[:1]))
        for i, (a,b) in enumerate(edges):
            require(math.dist(a,b) > 1e-10, "zero-length edge")
            for j, (c,d) in enumerate(edges):
                if j > i+1 and not (i == 0 and j == len(edges)-1):
                    require(not segments_touch(a,b,c,d), "self-intersecting or touching ring")
    rings = sorted(rings, key=lambda r: -abs(signed_area(r)))
    require(all(inside_ring(v,rings[0]) for v in rings[1]), "hole must lie strictly inside outer ring")
    for a,b in zip(rings[0],rings[0][1:]+rings[0][:1]):
        for c,d in zip(rings[1],rings[1][1:]+rings[1][:1]):
            require(not segments_touch(a,b,c,d), "hole touches outer ring")
    return rings


def simplify(ring, tolerance):
    ring = list(ring)
    changed = True
    while changed and len(ring) > 3:
        changed = False
        for i in range(len(ring)):
            if distance_to_segment(ring[i],ring[i-1],ring[(i+1)%len(ring)]) <= tolerance:
                ring.pop(i)
                changed = True
                break
    return ring


def ring_error(actual, expected, tolerance):
    actual = simplify(actual, tolerance)
    if len(actual) != len(expected):
        return None
    return min(max(math.dist(a, b) for a,b in zip(actual, trial[k:]+trial[:k]))
               for trial in (expected,list(reversed(expected))) for k in range(len(expected)))


def gray(values):
    values = tuple(float(v) for v in values)
    require(all(math.isfinite(v) and 0 <= v <= 1 for v in values), "invalid color component")
    require(len(values) in (1,3) and max(values)-min(values) <= 1e-9,
            "only DeviceGray or neutral DeviceRGB colors are supported")
    return values[0]


def operator_census(path, page_number):
    """Strict allowlist prevents invisible/unsupported paint from being dropped."""
    with pikepdf.open(path) as pdf:
        require(not pdf.is_encrypted, "encrypted PDF unsupported")
        require(not pdf.check_pdf_syntax(), "PDF syntax warnings unsupported")
        require(0 <= page_number < len(pdf.pages), "page index out of range")
        require('/OCProperties' not in pdf.Root, "optional content unsupported")
        page = pdf.pages[page_number]
        require(not page.get('/Annots', []), "annotations unsupported")
        require('/Group' not in page, "page transparency group unsupported")
        require(float(page.get('/UserUnit',1)) == 1, "nondefault UserUnit unsupported")
        require(int(page.get('/Rotate',0)) == 0, "rotated page unsupported")
        resources = page.get('/Resources',pikepdf.Dictionary())
        spaces = resources.get('/ColorSpace',pikepdf.Dictionary())
        require(not any(k in spaces for k in ('/DefaultGray','/DefaultRGB','/DefaultCMYK')),
                "default color-space substitution unsupported")
        state = dict(fill=0., stroke=0., fill_space='/DeviceGray', stroke_space='/DeviceGray',
                     width=1., join=0, cap=0, limit=10., ctm_scale=1.)
        stack, records, operators = [], [], {}
        active, subpaths, first, last, closed = False, [], None, None, False
        def finish_subpath():
            if first is not None:
                subpaths.append(closed or last == first)
        instructions = pikepdf.parse_content_stream(page)
        require(len(instructions) <= 10000, "operator budget exceeded")
        for operands, operator in instructions:
            op = str(operator)
            operators[op] = operators.get(op,0)+1
            arity = {'q':0,'Q':0,'cm':6,'g':1,'G':1,'rg':3,'RG':3,'cs':1,'CS':1,
                     'w':1,'j':1,'J':1,'M':1,'d':2,'gs':1,'m':2,'l':2,'h':0,'re':4,
                     'f':0,'F':0,'f*':0,'B':0,'B*':0,'b':0,'b*':0,'S':0,'s':0,'n':0}
            require(op in arity or op in ('sc','scn','SC','SCN'), "unsupported PDF operator: "+op)
            if op in arity:
                require(len(operands)==arity[op], "wrong operator arity: "+op)
            if op in ('m','l','re'):
                require(all(math.isfinite(float(v)) for v in operands), "nonfinite PDF path operand")
            if op == 'm':
                finish_subpath()
                first = last = tuple(float(v) for v in operands)
                closed = False
                active = True
            elif op == 'l':
                require(active and not closed, "line outside open subpath")
                last = tuple(float(v) for v in operands)
            elif op == 'h':
                require(first is not None, "close without subpath")
                closed = True
            elif op == 're':
                finish_subpath()
                subpaths.append(True)
                first = last = None
                closed = False
                active = True
            elif op in ('f','F','f*','B','B*','b','b*','S','s','n'):
                require(active, "empty path paint/discard unsupported")
                if op in ('b','b*','s'):
                    closed = True
                finish_subpath()
                require(op != 'n', "discarded path unsupported")
                stroke = op in ('B','B*','b','b*','S','s')
                fill = op not in ('S','s')
                require(not stroke or all(subpaths), "open stroked subpath unsupported")
                require(len(subpaths)==2, "paint requires two subpaths")
                records.append(dict(state, type='fs' if fill and stroke else 'f' if fill else 's',
                                    even_odd=op.endswith('*')))
                active, subpaths, first, last, closed = False, [], None, None, False
            else:
                require(not active, "graphics-state change inside path unsupported")
                if op == 'q':
                    require(len(stack)<64, "graphics-state depth exceeded")
                    stack.append(copy.copy(state))
                elif op == 'Q':
                    require(stack, "unbalanced Q")
                    state = stack.pop()
                elif op == 'cm':
                    a,b,c,d,e,f = map(float,operands)
                    require(all(math.isfinite(v) for v in (a,b,c,d,e,f)) and a>0 and a==d and b==c==0,
                            "only positive uniform scale/translation cm supported")
                    state['ctm_scale'] *= a
                elif op in ('g','G','rg','RG'):
                    key = 'fill' if op[0].islower() else 'stroke'
                    state[key] = gray(operands)
                    state[key+'_space'] = '/DeviceGray' if len(operands)==1 else '/DeviceRGB'
                elif op in ('cs','CS'):
                    key = 'fill' if op=='cs' else 'stroke'
                    require(str(operands[0]) in ('/DeviceGray','/DeviceRGB'), "unsupported color space")
                    state[key+'_space'] = str(operands[0])
                elif op in ('sc','scn','SC','SCN'):
                    key = 'fill' if op[0].islower() else 'stroke'
                    require(len(operands)==(1 if state[key+'_space']=='/DeviceGray' else 3), "color arity mismatch")
                    state[key] = gray(operands)
                elif op in ('w','j','J','M'):
                    key = {'w':'width','j':'join','J':'cap','M':'limit'}[op]
                    state[key] = float(operands[0])
                    require(math.isfinite(state[key]), "nonfinite stroke state")
                elif op == 'd':
                    require(len(operands[0])==0 and float(operands[1])==0, "dashed stroke unsupported")
                elif op == 'gs':
                    ext = resources.get('/ExtGState',{}).get(operands[0])
                    require(ext is not None, "missing ExtGState")
                    apply_ext_gstate(dict(ext.items()), state)
        require(not stack and not active, "unbalanced graphics state or unfinished path")
        if len(records)==2 and records[0]['type']=='f' and records[1]['type']=='s':
            # Illustrator 30.7.0 exports a filled and stroked compound as one
            # fill (f*) followed by one stroke (S) of the same subpaths, not as
            # B*. measure_pdf verifies the two share geometry before merging.
            merged = dict(records[1], type='fs', fill=records[0]['fill'],
                          fill_space=records[0]['fill_space'], even_odd=records[0]['even_odd'],
                          split_paint=True)
            return merged, operators
        require(len(records)==1, "require exactly one compound paint; split/extra paint unsupported")
        return records[0], operators


def apply_ext_gstate(ext, state):
    """Apply one ExtGState dictionary, refusing anything that changes paint.

    Illustrator writes /OPM 1 and /SA true into every graphics state. Overprint
    mode has no effect while /OP and /op are both false (ISO 32000-1, 8.6.7),
    and stroke adjustment is a scan-conversion hint that does not change the
    vector geometry measured here. Both are accepted only in that form and are
    recorded as a sorted tuple in state['render_hints'].
    """
    overprint = any(bool(ext.get(key, False)) for key in ('/OP', '/op'))
    for key, value in ext.items():
        if key == '/Type':
            require(str(value)=='/ExtGState', "invalid graphics-state type")
        elif key in ('/LW','/LJ','/LC','/ML'):
            state[{'/LW':'width','/LJ':'join','/LC':'cap','/ML':'limit'}[key]] = float(value)
        elif key in ('/CA','/ca'):
            require(float(value)==1, "transparency unsupported")
        elif key in ('/OP','/op','/AIS'):
            require(not bool(value), "overprint/alpha-is-shape unsupported")
        elif key == '/SA':
            if bool(value):
                state['render_hints'] = tuple(sorted(set(state.get('render_hints', ())) | {'SA'}))
        elif key == '/OPM':
            require(float(value)==0 or not overprint, "overprint mode with overprint unsupported")
            if float(value) != 0:
                state['render_hints'] = tuple(sorted(set(state.get('render_hints', ())) | {'OPM-inert'}))
        elif key == '/BM':
            require(str(value)=='/Normal', "blend mode unsupported")
        elif key == '/SMask':
            require(str(value)=='/None', "soft mask unsupported")
        else:
            raise Unsupported('unsupported ExtGState key: '+key)


def extract_rings(drawing, matrix, origin, scale):
    rings, current = [], []
    def point(p):
        q = pymupdf.Point(p) * matrix
        return ((q.x-origin[0])/scale, (q.y-origin[1])/scale)
    def flush():
        nonlocal current
        if current:
            if math.dist(current[0],current[-1]) <= 1e-10:
                current.pop()
            rings.append(current)
            current = []
    for item in drawing['items']:
        require(item[0] in ('l','re','qu'), 'unsupported PyMuPDF path item: '+item[0])
        if item[0] == 'l':
            a,b = point(item[1]),point(item[2])
            if current and math.dist(current[-1],a)>1e-10:
                flush()
            if not current:
                current = [a]
            current.append(b)
        else:
            flush()
            if item[0] == 're':
                r = item[1]
                # PyMuPDF rectangle orientation +1 corresponds to CCW in PDF.
                current = [point(p) for p in (r.bl,r.br,r.tr,r.tl)]
                if item[2] < 0:
                    current.reverse()
            else:
                q = item[1]
                current = [point(p) for p in (q.ul,q.ll,q.lr,q.ur)]
            flush()
    flush()
    return validate_rings(rings)


def measure_pdf(path, role, origin, scale, gray_value=.5, tolerance=1e-4, page_number=0):
    require(role in ('effect','control'), "invalid role")
    require(len(origin)==2 and all(math.isfinite(v) for v in origin), "invalid explicit origin")
    require(math.isfinite(scale) and scale>0, "scale must be positive and finite")
    require(math.isfinite(tolerance) and 0<tolerance<=1e-3, "tolerance must be in (0, 0.001] fixture units")
    require(math.isfinite(gray_value) and 0<=gray_value<=1, "gray must be in [0,1]")
    path = Path(path)
    require(path.stat().st_size <= 20_000_000, "PDF byte budget exceeded")
    state, operators = operator_census(path,page_number)
    fixture = analytic_fixture()
    with pymupdf.open(path) as doc:
        require(not doc.is_repaired, "repaired PDF unsupported")
        page = doc[page_number]
        require(page.rotation==0, "rotated page unsupported")
        drawings = page.get_drawings(extended=True)
        matrix = ~page.transformation_matrix  # MuPDF top-left/y-down -> PDF y-up.
        if state.get('split_paint'):
            require(role=='control', "split fill/stroke paint is accepted for the control only")
        if state.get('split_paint') and len(drawings)==1:
            # PyMuPDF itself joins a fill and a stroke of identical subpaths
            # into one 'fs' drawing; the operator census above proves the split.
            require(drawings[0]['type']=='fs', "split paint must extract as one fill+stroke drawing")
        elif state.get('split_paint'):
            require(len(drawings)==2 and [d['type'] for d in drawings]==['f','s'],
                    "split paint needs exactly one fill then one stroke drawing")
            fill_rings = extract_rings(drawings[0],matrix,origin,scale)
            stroke_rings = extract_rings(drawings[1],matrix,origin,scale)
            require(len(fill_rings)==len(stroke_rings)==2 and
                    all(e is not None and e<=tolerance for e in
                        (ring_error(a,b,tolerance) for a,b in zip(fill_rings,stroke_rings))),
                    "split fill and stroke do not share geometry")
            drawing = dict(drawings[0], type='fs', color=drawings[1].get('color'),
                           width=drawings[1].get('width'), lineJoin=drawings[1].get('lineJoin'),
                           stroke_opacity=drawings[1].get('stroke_opacity'))
            drawings = [drawing]
        require(len(drawings)==1 and drawings[0]['type'] in ('f','fs'), "unsupported clipping, group, or paint census")
        drawing = drawings[0]
        require(drawing['type']==state['type'], "operator/vector paint disagreement")
        require(drawing['even_odd']==state['even_odd'], "operator/vector fill-rule disagreement")
        require(not drawing.get('layer'), "layered paint unsupported")
        require(len(drawing['items'])<=512, "path item budget exceeded")
        require(drawing.get('fill_opacity')==1, "nonopaque fill")
        require(abs(state['fill']-gray_value)<=1e-9, "unexpected fill gray")
        require(drawing.get('fill') is not None and max(abs(v-gray_value) for v in drawing['fill']) <= 2e-4,
                "unexpected extracted fill color")
        rings = extract_rings(drawing,matrix,origin,scale)
        pdf_bounds = [pymupdf.Point(origin[0]+x*scale,origin[1]+y*scale)*page.transformation_matrix
                      for x,y in [(-.5,-.3),(10.3,10.3)]]
        require(all(page.rect.contains(p) for p in pdf_bounds), "fixture or escaping triangle falls outside page crop")
        # PyMuPDF path coordinates have float32 precision. Refuse placements
        # too coarse for the selected tolerance instead of silently widening it.
        magnitude = max(1.,*(abs(v) for p in pdf_bounds for v in (p.x,p.y)))
        require(magnitude*2**-23/scale <= tolerance/4, "PDF placement exceeds vector precision budget")
        stroked = state['type']=='fs'
        require(role=='control' or not stroked, "effect must be an unstroked filled region")
        if stroked:
            require(abs(state['stroke']-gray_value)<=1e-9, "fill/stroke grays differ")
            require(drawing.get('stroke_opacity')==1 and drawing.get('color') is not None and
                    max(abs(v-gray_value) for v in drawing['color'])<=2e-4, "unexpected extracted stroke color")
            width = state['width']*state['ctm_scale']/scale
            require(abs(width-.6)<=tolerance and abs(drawing['width']/scale-.6)<=tolerance,
                    "control stroke must have h=0.3 in fixture units")
            require(state['join']==0 and state['limit']==4 and drawing['lineJoin']==0,
                    "control requires miter join and limit 4")
            require(state['cap'] in (0,1,2), "invalid line cap")
        expected = fixture['source'] if stroked else fixture[role]
        errors = [ring_error(a,e,tolerance) for a,e in zip(rings,expected)]
        geometry_matches = all(e is not None and e<=tolerance for e in errors)
        hole_center = tuple(sum(p[i] for p in expected[1])/3 for i in (0,1))
        hole_empty = not membership(hole_center,rings,state['even_odd'])
        actual_area = abs(signed_area(rings[0]))
        if state['even_odd'] or signed_area(rings[0])*signed_area(rings[1]) < 0:
            actual_area -= abs(signed_area(rings[1]))
        # A stroked PDF stores the centerline, not the stroke silhouette. Only
        # reconstruct the bounded analytic control after validating that input.
        reconstruction_valid = stroked and geometry_matches and hole_empty
        painted_rings = fixture['control'] if reconstruction_valid else rings
        painted_even_odd = True if reconstruction_valid else state['even_odd']
        area = fixture['control_area'] if reconstruction_valid else actual_area
        area_error = abs(area-fixture[role+'_area'])
        probe = fixture['escaping_probe']
        escape = membership(probe,painted_rings,painted_even_odd)
        ordinary_corner = membership((-.25,-.25),painted_rings,painted_even_odd)
        corrected_hole_center = tuple(sum(p[i] for p in fixture['effect'][1])/3 for i in (0,1))
        corrected_hole_empty = not membership(corrected_hole_center,painted_rings,painted_even_odd)
        checks = dict(path_matches=geometry_matches, fill_rule_preserves_hole=hole_empty,
                      area_matches=area_error<=50*tolerance,
                      escaping_probe_matches=escape==(role=='control'),
                      ordinary_outer_corner_preserved=ordinary_corner,
                      corrected_hole_empty=corrected_hole_empty)
        return dict(status='PASS' if all(checks.values()) else 'FAIL', checks=checks,
                    pdf_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), page=page_number+1,
                    origin_pdf_points=list(origin), points_per_fixture_unit=scale,
                    coordinate_tolerance=tolerance, area_tolerance=50*tolerance,
                    gray=gray_value, operators=operators, paint_type=state['type'],
                    rings=rings, even_odd=state['even_odd'], vertex_errors=errors,
                    extracted_fill_area=actual_area, painted_area=area, area_error=area_error,
                    painted_min_x=min(x for r in painted_rings for x,y in r),
                    escaping_probe=list(probe), escaping_probe_inside=escape,
                    area_evidence='analytic stroke reconstruction from validated centerline and operators'
                    if reconstruction_valid else 'shoelace area of extracted filled polygon vertices',
                    silhouette_explicit_in_pdf=not stroked,
                    split_fill_stroke_paint=bool(state.get('split_paint')),
                    render_hints=list(state.get('render_hints',())))


def measure_pair(effect, control, origin, scale, *, effect_origin=None, control_origin=None,
                 effect_scale=None, control_scale=None, gray_value=.5, tolerance=1e-4,
                 effect_page=0, control_page=0):
    report = dict(schema=1, status='REFUSED', pymupdf_version=pymupdf.VersionBind,
                  pikepdf_version=pikepdf.__version__, expected=analytic_fixture(),
                  limitations=['Explicit transform required; no automatic registration or native placement proof.',
                               'Isolated linear compound fixture only; unsupported content is refused.',
                               'Stroke reconstruction is PDF-operator evidence, not measured raster silhouette.',
                               'MuPDF/Poppler renderer disagreement and curved-font behavior remain unresolved.'])
    for role,path,xy,s,page in [('effect',effect,effect_origin,effect_scale,effect_page),
                              ('control',control,control_origin,control_scale,control_page)]:
        try:
            report[role] = measure_pdf(path,role,origin if xy is None else xy,
                                       scale if s is None else s,gray_value,tolerance,page)
        except (Unsupported,OSError,ValueError,RuntimeError,pikepdf.PdfError) as error:
            report[role] = dict(status='REFUSED',reason=str(error))
    statuses = [report[r]['status'] for r in ('effect','control')]
    report['status'] = 'REFUSED' if 'REFUSED' in statuses else 'PASS' if statuses==['PASS','PASS'] else 'FAIL'
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('effect',type=Path)
    parser.add_argument('control',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--origin',type=float,nargs=2,required=True,metavar=('PDF_X','PDF_Y'))
    parser.add_argument('--scale',type=float,required=True,help='positive PDF points per fixture unit')
    for role in ('effect','control'):
        parser.add_argument('--'+role+'-origin',type=float,nargs=2)
        parser.add_argument('--'+role+'-scale',type=float)
        parser.add_argument('--'+role+'-page',type=int,default=1,help='one-based page (default 1)')
    parser.add_argument('--gray',type=float,default=.5)
    parser.add_argument('--tolerance',type=float,default=1e-4,help='fixture-coordinate tolerance, at most 0.001')
    args = parser.parse_args(argv)
    require(args.output.resolve() not in (args.effect.resolve(),args.control.resolve()), "output must not overwrite input PDF")
    report = measure_pair(args.effect,args.control,args.origin,args.scale,
                          effect_origin=args.effect_origin,control_origin=args.control_origin,
                          effect_scale=args.effect_scale,control_scale=args.control_scale,
                          effect_page=args.effect_page-1,control_page=args.control_page-1,
                          gray_value=args.gray,tolerance=args.tolerance)
    args.output.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print(report['status'])
    return {'PASS':0,'FAIL':1,'REFUSED':2}[report['status']]


if __name__ == '__main__':
    raise SystemExit(main())
