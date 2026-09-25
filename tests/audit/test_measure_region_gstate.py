"""ExtGState admission in tools/measure_region_pdf.py; no PDF or Adobe host needed."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import measure_region_pdf as m  # noqa: E402


def gs(**entries):
    return {'/' + k: v for k, v in entries.items()}


class ExtGStateAdmission(unittest.TestCase):
    def test_illustrator_default_state_is_admitted_with_recorded_hints(self):
        # The exact dictionary Illustrator 30.7.0 writes (region-prep-7 PDFs).
        state = {}
        m.apply_ext_gstate(gs(AIS=False, BM='/Normal', CA=1.0, OP=False, OPM=1, SA=True,
                              SMask='/None', Type='/ExtGState', ca=1.0, op=False), state)
        self.assertEqual(state['render_hints'], ('OPM-inert', 'SA'))

    def test_overprint_mode_with_overprint_is_refused(self):
        for key in ('OP', 'op'):
            with self.assertRaises(m.Unsupported):
                m.apply_ext_gstate(gs(OPM=1, **{key: True}), {})

    def test_overprint_itself_is_refused(self):
        with self.assertRaises(m.Unsupported):
            m.apply_ext_gstate(gs(OP=True, OPM=0), {})

    def test_transparency_blend_and_soft_mask_stay_refused(self):
        for entries in (gs(CA=0.5), gs(ca=0.5), gs(BM='/Multiply'), gs(SMask='/Luminosity'), gs(AIS=True)):
            with self.assertRaises(m.Unsupported):
                m.apply_ext_gstate(entries, {})

    def test_unknown_key_is_refused(self):
        with self.assertRaises(m.Unsupported):
            m.apply_ext_gstate(gs(TR='/Identity'), {})

    def test_plain_state_records_no_hints(self):
        state = {}
        m.apply_ext_gstate(gs(OP=False, op=False, OPM=0, SA=False), state)
        self.assertNotIn('render_hints', state)


if __name__ == '__main__':
    unittest.main()
