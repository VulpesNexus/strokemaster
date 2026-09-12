"""Separate fill coverage from a halo that survives erosion."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('compare', ROOT / 'tools/compare_renders.py')
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class RenderMaskTests(unittest.TestCase):
    def test_halo_changes_are_not_fill_changes(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            a = np.full((60, 60, 3), 255, dtype=np.uint8)
            a[4:36, 4:36] = 90  # Wide gray stroke halo.
            a[14:26, 14:26] = 10  # Actual fill.
            b = a.copy()
            b[7:12, 7:12] = 120  # 25 changed halo pixels well inside ink.
            mask = np.zeros((60, 60), dtype=np.uint8)
            mask[14:26, 14:26] = 255
            Image.fromarray(a).save(base / 'a.png')
            Image.fromarray(b).save(base / 'b.png')
            Image.fromarray(mask).save(base / 'mask.png')
            manifest = base / 'frames.json'
            manifest.write_text(json.dumps({'frames': [{'name': 'halo', 'bounds': [0, 0, 60, 60]}]}))
            with contextlib.redirect_stdout(io.StringIO()):
                compare.main(base / 'a.png', base / 'b.png', manifest, str(base / 'ink'), 72)
                compare.main(base / 'a.png', base / 'b.png', manifest, str(base / 'fill'), 72, base / 'mask.png')
            ink = json.loads((base / 'ink.json').read_text())
            fill = json.loads((base / 'fill.json').read_text())
            self.assertEqual(ink['mask_kind'], 'estimated_nonbackground_ink')
            self.assertEqual(ink['frames'][0]['interior_changed_px'], 25)
            self.assertEqual(fill['mask_kind'], 'external_fill_only')
            self.assertEqual(fill['frames'][0]['interior_changed_px'], 0)
            self.assertEqual(fill['page_changed_px'], 25)
            self.assertFalse(fill['fill_compositing_verified'])

    def test_no_silent_uneroded_fallback(self):
        previous = compare.ndimage
        try:
            compare.ndimage = None
            with self.assertRaisesRegex(RuntimeError, 'refusing'):
                compare.interior_mask(np.ones((8, 8), dtype=bool), 1)
        finally:
            compare.ndimage = previous

    def test_empty_mask_has_no_interior(self):
        self.assertEqual(compare.interior_mask(np.zeros((8, 8), dtype=bool), 1).sum(), 0)

    def test_off_page_negative_crop_is_empty(self):
        image = np.zeros((60, 60, 3))
        self.assertEqual(compare.crop(image, [-40, -40, -20, -20], 12, 12, 1).size, 0)
        self.assertEqual(compare.crop(image, [90, 90, 100, 100], 12, 12, 1).size, 0)

    def test_invalid_bounds_are_refused(self):
        image = np.zeros((60, 60, 3))
        for bounds in ([10, 10, 0, 0], [0, 0, float('inf'), 20]):
            with self.assertRaises(ValueError):
                compare.crop(image, bounds, 12, 12, 1)

    def test_transparent_render_and_mask_are_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            filename = Path(tmp) / 'transparent.png'
            Image.new('RGBA', (4, 4), (255, 255, 255, 0)).save(filename)
            for mode in ('RGB', 'L'):
                with self.assertRaisesRegex(ValueError, 'transparent'):
                    compare.opaque_image(filename, mode)

    def test_opaque_alpha_image_is_supported(self):
        with tempfile.TemporaryDirectory() as tmp:
            filename = Path(tmp) / 'opaque.png'
            Image.new('RGBA', (4, 4), (255, 255, 255, 255)).save(filename)
            self.assertTrue(np.all(compare.opaque_image(filename, 'RGB') == 255))


if __name__ == '__main__':
    unittest.main()
