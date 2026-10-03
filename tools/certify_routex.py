"""Route X cross-frame census: per-case verdicts from measured session records.

The sessions are planned, run, and measured by tools/certify_routew.py (source key X, the
cross-frame census in tests/certification/corpus/cert-X.indd). This tool joins a native control,
a route W baseline, and a route X candidate, case by case and output by output, and states what
each case is expected to do under route X's rule:

  corrected     one story's stroke over the same story's fill, drawn in another frame or column:
                route X must reach 0 damaged fill-core pixels where native drawing does not
  unchanged     InDesign's order is kept on purpose (independent stories, content stacked in
                between, a translucent upstream frame): route X must equal route W exactly
  partial       corrected except where other content drawn in between (an unrelated item, the
                frame's own border) lies over a glyph: route X must lie strictly between 0 and
                route W
  control       no damage natively, or none drawn: route X must equal route W
  center        Center strokes are left to InDesign: route X must equal route W

Usage:
  certify_routex.py summarize --native N.json --routew W.json --candidate K.json OUTPUT
      [--overlap O.json] [--reference R.json] [--edit E.json] [--label TEXT]
  certify_routex.py summarize --census X2 --candidate K.json OUTPUT [--native N.json]
      [--routew W.json]   the extension census (tests/certification/corpus/cert-X2.indd); a
      corrected case with no native session is PASS_NO_CONTROL, never PASS
  certify_routex.py state-scan OUTPUT FILE...    documents saved with the plugin: does any
      INDD, or any IDML entry, carry the plugin's name or its ID prefix (0x5A200, either
      byte order)? A model plugin may write document data; this one must not.
"""
import argparse
import json
import re
from pathlib import Path

# key: (relation, expectation, why)
XCASES = {
    'XA0': ('independent', 'unchanged', 'two stories; the upper frame\'s stroke over the lower frame\'s fill is page-item stacking'),
    'XA1': ('independent', 'unchanged', 'two stories; stroked upper frame over an unstroked lower frame'),
    'XA2': ('independent', 'control', 'the stroked frame is below: no damage natively'),
    'XA3': ('independent', 'unchanged', 'two stories in touching frames: the XB0 geometry without the thread'),
    'XA4': ('independent', 'unchanged', 'two stories on two layers'),
    'XA5': ('independent', 'unchanged', 'a headline halo over another frame\'s text: the same geometry as XA0, drawn on purpose'),
    'XB0': ('same story', 'corrected', 'threaded continuation, downstream frame in front (E00)'),
    'XB1': ('same story', 'corrected', 'threaded, upstream frame in front'),
    'XB2': ('same story', 'corrected', 'threaded frames that overlap'),
    'XB3': ('same story', 'partial', "an unrelated rectangle stacked between the frames covers the upstream second line, which keeps InDesign's order; the first line is corrected"),
    'XB4': ('same story', 'corrected', 'downstream frame fill that does not reach the upstream glyphs'),
    'XB5': ('same story', 'partial', 'bordered frames: glyphs the upstream border crosses keep InDesign\'s order'),
    'XB6': ('same story', 'corrected', 'two columns of one frame (two parcels)'),
    'XB7': ('same story', 'corrected', 'threaded pair on two layers'),
    'XD0': ('same story', 'corrected', 'threaded pair in a group'),
    'XD1': ('same story', 'unchanged', 'translucent upstream frame: its fill is not opaque, so it is not recorded'),
    'XD2': ('same story', 'control', 'upstream frame on a hidden layer: nothing drawn beneath the stroke'),
    'XD3': ('same story', 'control', 'table rows: cell text does not overlap'),
    'XN0': ('single frame', 'control', 'stroked text over a rectangle'),
    'XN1': ('single frame', 'control', 'a rectangle over stroked text'),
    'XN2': ('same story', 'center', 'Center strokes'),
    'XN3': ('same story', 'control', 'nonprinting upstream frame: absent from exports'),
}
# The cross-frame extension census (tools/jsx/cert-build-xframe2.jsx, 2026-10-03): the cases the
# cross-frame research left untested, and neighbors of them. Every one is one story.
XECASES = {
    'XE0': ('same story', 'corrected', 'threaded pair on the parent page, drawn on the document page'),
    'XE1': ('same story', 'corrected', 'path text threaded into a frame below it'),
    'XE2': ('same story', 'corrected', 'threaded anchored frames: InDesign 21.6 refuses to thread them by script, so the case is not built'),
    'XE3': ('same story', 'corrected', 'threaded pair rotated 30 degrees as a unit'),
    'XE4': ('same story', 'corrected', 'three frames threaded in a chain'),
    'XE5': ('same story', 'corrected', 'downstream frame scaled 125% in its matrix'),
    'XE6': ('same story', 'corrected', 'an invisible rectangle stacked between the frames draws nothing in between'),
    'XE7': ('same story', 'corrected', 'threaded pair in a group nested in another group'),
    'XE8': ('same story', 'partial', 'an unfilled rectangle with a 1 pt border stacked between the frames: glyphs under the border keep InDesign\'s order'),
}
CENSUS = {'X': XCASES, 'X2': XECASES}
OUTPUTS = ('hqp.pdf', 'x4.pdf', 'x1a.pdf', 'png600.png', 'eps.eps', 'pshigh.ps')
STRUCTURE_KEYS = ('text_chars', 'text_render_modes', 'images', 'fonts')


def load(path):
    return json.loads(Path(path).read_text(encoding='utf-8')) if path else None


def per_case(record, output, census='X'):
    if not record or census not in record.get('sources', {}):
        return None, None
    o = record['sources'][census]['outputs'].get(output)
    return ({f['key']: f['damage_px'] for f in o['fill_core']['frames']}, o) if o else (None, None)


def judge(expectation, native, routew, candidate):
    if candidate is None:
        return 'UNTESTED'
    if expectation == 'corrected' and native is None and routew is None:
        # A census run without native or route W sessions: zero damage is necessary, but no
        # native session shows that the case exercises the defect.
        return 'PASS_NO_CONTROL' if candidate == 0 else 'FAILED'
    if routew is None:
        return 'UNTESTED'
    if expectation == 'corrected':
        return 'PASS' if candidate == 0 and (native or 0) > 0 else 'FAILED'
    if expectation == 'partial':
        return 'PASS' if 0 < candidate < routew else ('PASS' if candidate == 0 else 'FAILED')
    return 'PASS' if candidate == routew else 'FAILED'


def structure_check(routew_out, candidate_out):
    """PDF structure the candidate must keep: the same text, text modes, images, and fonts as
    route W (fonts compared without their random subset tags), and paint operators that differ
    only in clipping and graphics state."""
    if not routew_out or 'structure' not in routew_out:
        return None
    a, b = routew_out['structure'], candidate_out['structure']
    strip = lambda s: re.sub(r'[A-Z]{6}\+', '', json.dumps(s, sort_keys=True))
    same = {k: strip(a.get(k)) == strip(b.get(k)) for k in STRUCTURE_KEYS}
    ops_a, ops_b = a.get('ops', {}), b.get('ops', {})
    changed_ops = sorted(k for k in set(ops_a) | set(ops_b) if ops_a.get(k) != ops_b.get(k))
    return {'same_as_route_w': same, 'changed_operators': changed_ops,
            'only_clip_and_state_changed': set(changed_ops) <= {'W', 'n', 'gs', 'q', 'Q', 're', 'm', 'l', 'c', 'h', 'cm'},
            'text_chars': b.get('text_chars'), 'images': b.get('images')}


def summarize(args):
    native, routew, cand = load(args.native), load(args.routew), load(args.candidate)
    overlap, reference, edit = load(args.overlap), load(args.reference), load(args.edit)
    rows, verdicts = [], {}
    census = args.census
    for key, (relation, expectation, why) in CENSUS[census].items():
        row = {'key': key, 'relation': relation, 'expectation': expectation, 'why': why, 'outputs': {}}
        for output in OUTPUTS:
            n, _ = per_case(native, output, census)
            w, wo = per_case(routew, output, census)
            c, co = per_case(cand, output, census)
            if c is None or key not in c:
                continue
            entry = {'native_damage_px': n.get(key) if n else None, 'route_w_damage_px': w.get(key) if w else None,
                     'route_x_damage_px': c.get(key), 'route_x_sha256': co['sha256'], 'renderer': co.get('renderer')}
            if overlap:
                ov, _ = per_case(overlap, output, census)
                entry['overlap_mode_damage_px'] = ov.get(key) if ov else None
            entry['verdict'] = judge(expectation, entry['native_damage_px'], entry['route_w_damage_px'], entry['route_x_damage_px'])
            row['outputs'][output] = entry
        verdicts_here = [e['verdict'] for e in row['outputs'].values()]
        if 'FAILED' in verdicts_here:
            row['verdict'] = 'FAILED'
        elif verdicts_here and all(v == 'PASS' for v in verdicts_here):
            row['verdict'] = 'PASS'
        elif verdicts_here and all(v in ('PASS', 'PASS_NO_CONTROL') for v in verdicts_here):
            row['verdict'] = 'PASS_NO_CONTROL'
        else:
            row['verdict'] = 'UNTESTED'
        if reference:
            ro = reference['sources'][census]['outputs']
            edges = {}
            for output in OUTPUTS:
                e = ro.get(output, {}).get('edge_vs_split_reference')
                if e:
                    fr = next((f for f in e['frames'] if f['key'] == key), None)
                    if fr:
                        edges[output] = {k: fr[k] for k in ('max_diff', 'px_over_64', 'over_64_beyond_band', 'over_16_by_edge_distance_px')}
            row['edge_vs_story_reference'] = edges
        verdicts[row['verdict']] = verdicts.get(row['verdict'], 0) + 1
        rows.append(row)
    structure = {}
    for output in ('hqp.pdf', 'x4.pdf', 'x1a.pdf'):
        _, wo = per_case(routew, output, census)
        _, co = per_case(cand, output, census)
        if co:
            structure[output] = structure_check(wo, co)
    doc = {'schema': 1, 'label': args.label,
           'census': census,
           'sessions': {'native': native.get('label') if native else None, 'route_w': routew.get('label') if routew else None,
                        'route_x': cand.get('label'),
                        'overlap': overlap.get('label') if overlap else None, 'reference': reference.get('label') if reference else None},
           'corpus_sha256': cand.get('corpus_sha256', {}).get('cert-%s.indd' % census),
           'verdict_counts': verdicts, 'cases': rows, 'pdf_structure_vs_route_w': structure}
    if edit:
        doc['editing'] = {'identities': {k: v for k, v in edit['editing']['identities'].items() if 'ED4' in k},
                          'stages': {k: {kk: v[kk] for kk in ('undo_history', 'undo_steps', 'redo_steps', 'log', 'errors')}
                                     for k, v in edit['editing']['stages'].items() if k.startswith('ED4')}}
    Path(args.output).write_bytes((json.dumps(doc, indent=1, ensure_ascii=False) + '\n').encode('utf-8'))
    print('cases', len(rows), 'verdicts', verdicts)
    for r in rows:
        if r['verdict'] != 'PASS':
            print(' ', r['key'], r['verdict'], {o: (e['native_damage_px'], e['route_w_damage_px'], e['route_x_damage_px']) for o, e in r['outputs'].items()})
    return 0 if 'FAILED' not in verdicts else 1


PLUGIN_MARKS = [b'InDesignStrokeEngine', b'InDesign Stroke Engine', bytes.fromhex('0005A200'), bytes.fromhex('00A20500')]
IDML_TEXT_MARKS = ['InDesignStrokeEngine', 'InDesign Stroke Engine', '5A200', '5a200', '368128']   # 368128 = 0x5A200


def state_scan(output, files):
    import hashlib
    import zipfile
    rows = []
    for f in files:
        path = Path(f)
        data = path.read_bytes()
        row = {'file': path.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(), 'hits': []}
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as z:
                for name in z.namelist():
                    text = z.read(name).decode('utf-8', 'replace')
                    row['hits'] += ['%s: %s' % (name, m) for m in IDML_TEXT_MARKS if m in text]
        else:
            row['hits'] += [m.hex() for m in PLUGIN_MARKS if m in data]
        rows.append(row)
        print(path.name, 'clean' if not row['hits'] else row['hits'])
    Path(output).write_bytes((json.dumps({'schema': 1, 'marks': [m.hex() for m in PLUGIN_MARKS], 'idml_marks': IDML_TEXT_MARKS, 'files': rows}, indent=1) + '\n').encode('utf-8'))
    return 1 if any(r['hits'] for r in rows) else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('summarize')
    s.add_argument('output', type=Path)
    s.add_argument('--census', choices=sorted(CENSUS), default='X')
    s.add_argument('--native')
    s.add_argument('--routew')
    s.add_argument('--candidate', required=True)
    s.add_argument('--overlap')
    s.add_argument('--reference')
    s.add_argument('--edit')
    s.add_argument('--label', default='')
    t = sub.add_parser('state-scan')
    t.add_argument('output', type=Path)
    t.add_argument('files', nargs='+')
    args = parser.parse_args(argv)
    if args.cmd == 'state-scan':
        return state_scan(args.output, args.files)
    if args.census == 'X' and not (args.native and args.routew):
        parser.error('the cross-frame census needs --native and --routew')
    return summarize(args)


if __name__ == '__main__':
    raise SystemExit(main())
