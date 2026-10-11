"""Safety checks for the panel's editable layout and real rendered icon bounds."""
import re
import unittest
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]

class PanelLayout(unittest.TestCase):
    def test_widgets_remain_visible_and_separate_when_resized(self):
        resource = (ROOT / 'plugin/ui/IseUI.fr').read_text()
        panel = resource.split('resource IseUIPanelWidget(', 1)[1]
        entries = re.findall(r'(IseUICheckBoxWidget|IseUIButtonWidget|StaticTextWidget|SeparatorWidget)\((\w+),[^\n]*?, (kBind[^,]*), Frame\(([^)]+)\)', panel)
        widgets = [(kind, key, bind, [int(v.strip()) for v in frame.split(',')]) for kind, key, bind, frame in entries]
        self.assertEqual(len({key for _, key, _, _ in widgets}), len(widgets))
        self.assertEqual(sum(kind == 'IseUICheckBoxWidget' for kind, *_ in widgets), 15)
        self.assertIn('kIsResizable', resource)
        for width, height in [(270, 418), (350, 500), (500, 800)]:
            boxes = []
            for kind, key, bind, raw in widgets:
                x1, y1, x2, y2 = raw
                if 'kBindRight' in bind:
                    x2 += width - 270
                    if 'kBindLeft' not in bind:
                        x1 += width - 270
                if 'kBindBottom' in bind:
                    y2 += height - 418
                    if 'kBindTop' not in bind:
                        y1 += height - 418
                self.assertTrue(0 <= x1 < x2 <= width, key)
                self.assertTrue(0 <= y1 < y2 <= height, key)
                if kind == 'IseUICheckBoxWidget':
                    boxes.append((key, x1, y1, x2, y2))
            for i, a in enumerate(boxes):
                for b in boxes[i + 1:]:
                    self.assertFalse(a[1] < b[3] and b[1] < a[3] and a[2] < b[4] and b[2] < a[4], (a[0], b[0]))

    def test_separators_do_not_cross_controls(self):
        resource = (ROOT / 'plugin/ui/IseUI.fr').read_text()
        lines = resource.split('resource IseUIPanelWidget(', 1)[1].splitlines()
        separators, controls = [], []
        for line in lines:
            match = re.search(r'Frame\((\d+), (\d+), (\d+), (\d+)\)', line)
            if not match:
                continue
            frame = tuple(map(int, match.groups()))
            if 'SeparatorWidget(' in line:
                separators.append(frame)
            elif 'Widget(' in line:
                controls.append(frame)
        self.assertEqual(len(separators), 3)
        for separator in separators:
            for control in controls:
                self.assertFalse(separator[1] < control[3] and control[1] < separator[3])

    def test_rendered_icons_have_measured_native_padding(self):
        for variant in ('N', 'D'):
            for size, inset in ((23, 3), (46, 6)):
                with Image.open(ROOT / f'plugin/ui/icons/StrokeMaster_{variant}_{size}.png') as image:
                    self.assertEqual(image.size, (size, size))
                    self.assertEqual(image.getchannel('A').getbbox(), (inset, inset, size - inset, size - inset))

if __name__ == '__main__':
    unittest.main()
