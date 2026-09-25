"""Pure tests for route X's evidence tools: the per-case verdicts and the PDF structure check in
tools/certify_routex.py, its document plugin-state scan, and the trace condenser in
tools/routex_trace.py. Synthetic records and files; no Adobe."""
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2] / 'tools'
sys.path.insert(0, str(TOOLS))
import certify_routex as x  # noqa: E402
import routex_trace  # noqa: E402


class Verdicts(unittest.TestCase):
    def test_corrected_needs_zero_where_native_was_damaged(self):
        self.assertEqual(x.judge('corrected', 11214, 4198, 0), 'PASS')
        self.assertEqual(x.judge('corrected', 11214, 4198, 12), 'FAILED')
        self.assertEqual(x.judge('corrected', 0, 0, 0), 'FAILED', 'a case that is not damaged natively proves nothing')

    def test_unchanged_control_and_center_must_equal_route_w_exactly(self):
        for expectation in ('unchanged', 'control', 'center'):
            self.assertEqual(x.judge(expectation, 2758, 2758, 2758), 'PASS')
            self.assertEqual(x.judge(expectation, 2758, 2758, 0), 'FAILED', 'fixing an independent frame is a failure too')
            self.assertEqual(x.judge(expectation, 2758, 2758, 2759), 'FAILED')

    def test_partial_lies_strictly_between(self):
        self.assertEqual(x.judge('partial', 11118, 4296, 219), 'PASS')
        self.assertEqual(x.judge('partial', 11118, 4296, 4296), 'FAILED')
        self.assertEqual(x.judge('partial', 11118, 4296, 5000), 'FAILED')

    def test_missing_measurements_are_untested(self):
        self.assertEqual(x.judge('corrected', 1, None, 0), 'UNTESTED')
        self.assertEqual(x.judge('corrected', 1, 1, None), 'UNTESTED')

    def test_every_census_case_has_a_known_expectation(self):
        manifest = json.loads((TOOLS.parent / 'tests/certification/corpus/cert-X.manifest.json').read_text(encoding='utf-8'))
        keys = {f['key'] for f in manifest['frames']}
        self.assertEqual(keys, set(x.XCASES), 'the census and the expectations must name the same cases')
        for relation, expectation, why in x.XCASES.values():
            self.assertIn(expectation, ('corrected', 'unchanged', 'partial', 'control', 'center'))
            self.assertTrue(why)


class Structure(unittest.TestCase):
    def rec(self, **kw):
        s = {'text_chars': 192, 'text_render_modes': {'0': 1}, 'images': 0,
             'fonts': {'ABCDEF+MinionPro-Regular': {'embedded': True}}, 'ops': {'BT': 72, 'Tj': 72, 'S': 63, 'W': 34, 'gs': 43}}
        s.update(kw)
        return {'structure': s}

    def test_clip_and_state_only(self):
        a = self.rec()
        b = self.rec(fonts={'QWERTY+MinionPro-Regular': {'embedded': True}}, ops={'BT': 72, 'Tj': 72, 'S': 63, 'W': 42, 'gs': 50})
        r = x.structure_check(a, b)
        self.assertTrue(all(r['same_as_route_w'].values()), 'subset tags are random and ignored')
        self.assertEqual(r['changed_operators'], ['W', 'gs'])
        self.assertTrue(r['only_clip_and_state_changed'])

    def test_text_or_paint_changes_are_reported(self):
        r = x.structure_check(self.rec(), self.rec(text_chars=190, ops={'BT': 70, 'Tj': 72, 'S': 63, 'W': 34, 'gs': 43}))
        self.assertFalse(r['same_as_route_w']['text_chars'])
        self.assertFalse(r['only_clip_and_state_changed'])


class StateScan(unittest.TestCase):
    def test_finds_the_plugin_in_a_document_or_an_idml_entry(self):
        with tempfile.TemporaryDirectory() as d:
            clean, marked, idml_clean, idml_marked = (Path(d) / n for n in ('a.indd', 'b.indd', 'c.idml', 'd.idml'))
            clean.write_bytes(b'\x00' * 64 + b'MinionPro')
            marked.write_bytes(b'\x00' * 8 + bytes.fromhex('00A20500') + b'\x00' * 8)
            with zipfile.ZipFile(idml_clean, 'w') as z:
                z.writestr('designmap.xml', '<Document/>')
            with zipfile.ZipFile(idml_marked, 'w') as z:
                z.writestr('Stories/Story_u1.xml', '<Story plugin="InDesignStrokeEngine"/>')
            out = Path(d) / 'scan.json'
            self.assertEqual(x.state_scan(out, [str(clean), str(idml_clean)]), 0)
            self.assertEqual(x.state_scan(out, [str(clean), str(marked), str(idml_marked)]), 1)
            rows = {r['file']: r['hits'] for r in json.loads(out.read_text(encoding='utf-8'))['files']}
            self.assertEqual(rows['a.indd'], [])
            self.assertEqual(rows['b.indd'], ['00a20500'])
            self.assertTrue(any('InDesignStrokeEngine' in h for h in rows['d.idml']))


class JobLogPaths(unittest.TestCase):
    def test_error_messages_lose_the_repository_location(self):
        import certify_routew as w
        both = 'source not found: %s/build/cert/K2/out/ED/ED1-edited.indd and %s\\tools' % (w.fwd(w.REPO), w.REPO)
        cleaned = w._no_paths(both)
        self.assertEqual(cleaned, 'source not found: <repo>/build/cert/K2/out/ED/ED1-edited.indd and <repo>\\tools')
        self.assertIsNone(w._no_paths(None))
        self.assertEqual(w._no_paths(12), 12)

    def test_job_log_scrubs_what_it_records(self):
        import certify_routew as w
        with tempfile.TemporaryDirectory() as d:
            rec = {'id': 'ED1.fill', 'error': 'source not found: %s/x.indd' % w.fwd(w.REPO), 'prep': [], 'ms': 1,
                   'outputs': [{'file': 'a.pdf', 'kind': 'pdf', 'ms': 1, 'bytes': 0, 'error': 'cannot open %s/a.pdf' % w.fwd(w.REPO), 'log': None}]}
            (Path(d) / 'results.jsonl').write_text(json.dumps(rec) + '\n', encoding='utf-8')
            logged = json.dumps(w.job_log(Path(d)))
            self.assertNotIn(w.fwd(w.REPO), logged)
            self.assertIn('<repo>/x.indd', logged)


class Trace(unittest.TestCase):
    TRACE = '\n'.join([
        'T7 E 1 BeginSpread depth=1 item=209 class=? flags=0x200 port=P1 kind=pdf',
        'T7 E 2 BeginShape depth=1 item=539 class=? flags=0x200 port=P1 kind=pdf',
        'T7 P W parcel=A ok=1 frame=541 story=520 key=541 parcels=2 xp=0 depth=5 flags=0x200',
        'T7 R W visit run=R1 port=P1 kind=pdf',
        'T7 F pending run=R1 kept=1 lazy=1 frame=541 box=0,0,1,1',
        'T7 R R restored run=R1 port=P1 kind=pdf',
        'T7 F painted run=R1',
        'T7 E 3 BeginShape depth=1 item=562 class=? flags=0x200 port=P1 kind=pdf',
        'T7 P W parcel=B ok=1 frame=564 story=520 key=564 parcels=2 xp=0 depth=5 flags=0x200',
        'T7 R W visit run=R2 port=P1 kind=pdf',
        'T7 K run=R2 withheld=2 covered=1 box=0,0,1,1',
        'T7 R R restored run=R2 port=P1 kind=pdf',
        'T7 E 4 EndSpread depth=1 item=209 class=? flags=0x200 port=P1 kind=pdf',
        'E 5 BeginShape depth=0 item=563 class=? flags=0x1200 port=P2 kind=screen',
        'P W parcel=C ok=1 frame=565 story=521 key=565 parcels=2 xp=0 depth=5 flags=0x1200',
    ])

    def test_condenses_traversals(self):
        t = routex_trace.parse(self.TRACE, {539: 'XB0a', 562: 'XB0b'})
        self.assertEqual(len(t), 2)
        first, second = t
        self.assertEqual((first['kind'], first['bracket'], first['distinct_ports']), ('pdf', 'BeginSpread', 1))
        self.assertEqual(first['frames_drawn'], 2)
        self.assertEqual(first['frames_reentered_after_another_frame'], 0)
        self.assertEqual(first['pass_sequences'], [('WR', 2)])
        self.assertEqual((first['fills_recorded'], first['fills_painted']), (1, 1))
        self.assertEqual(first['knockouts'], {'XB0b': {'strokes': 1, 'withheld_glyphs': 2, 'covered_glyphs': 1}})
        self.assertEqual(second['bracket'], 'none', 'a single-item export draws with no spread bracket')

    def test_a_frame_resumed_after_another_is_counted(self):
        text = '\n'.join([
            'E 1 BeginSpread depth=1 item=1 class=? flags=0x0 port=P kind=pdf',
            'P W parcel=A ok=1 frame=10 story=5 key=10 parcels=2 xp=0 depth=5 flags=0x0',
            'P W parcel=B ok=1 frame=20 story=5 key=20 parcels=2 xp=0 depth=5 flags=0x0',
            'P R parcel=A ok=1 frame=10 story=5 key=10 parcels=2 xp=0 depth=5 flags=0x0',
            'E 2 EndSpread depth=1 item=1 class=? flags=0x0 port=P kind=pdf',
        ])
        self.assertEqual(routex_trace.parse(text, {})[0]['frames_reentered_after_another_frame'], 1)


if __name__ == '__main__':
    unittest.main()
