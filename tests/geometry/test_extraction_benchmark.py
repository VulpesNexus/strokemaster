"""Controls for exact contour deduplication and provisional benchmark accounting."""
import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

from benchmark_extracted_paths import benchmark, fingerprint


class ExtractionBenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.path = {'index': 1, 'closed': True, 'strokeWidth': 0, 'area': 0.5,
                     'pts': [{'a': [0, 0], 'l': [-1, 0], 'r': [0, 1]},
                             {'a': [1, 0], 'l': [1, 0], 'r': [1, 0]},
                             {'a': [0, 1], 'l': [0, 1], 'r': [0, 1]}]}

    def test_fingerprint_cyclic_start_and_reversal_swap_handles(self):
        rotated = copy.deepcopy(self.path)
        rotated['pts'] = rotated['pts'][1:]+rotated['pts'][:1]
        reversed_path = copy.deepcopy(self.path)
        reversed_path['pts'] = [{'a': p['a'], 'l': p['r'], 'r': p['l']}
                                for p in reversed(reversed_path['pts'])]
        for path in (rotated, reversed_path):
            self.assertNotEqual(fingerprint(path), fingerprint(self.path))
            self.assertEqual(fingerprint(path, True), fingerprint(self.path, True))
        # Reversing without swapping handles changes the curves.
        reversed_path['pts'] = list(reversed(self.path['pts']))
        self.assertNotEqual(fingerprint(reversed_path, True), fingerprint(self.path, True))

    def test_fingerprint_excludes_metadata_but_preserves_location_and_handles(self):
        path = copy.deepcopy(self.path)
        path.update(index=999, strokeWidth=3, area=42, gb=[9, 8, 7, 6])
        self.assertEqual(fingerprint(path, True), fingerprint(self.path, True))
        for point in path['pts']:
            for key in ('a', 'l', 'r'):
                point[key][0] += 1
        self.assertNotEqual(fingerprint(path, True), fingerprint(self.path, True))
        path = copy.deepcopy(self.path)
        path['pts'][0]['r'][1] += 0.000001
        self.assertNotEqual(fingerprint(path, True), fingerprint(self.path, True))
        path = copy.deepcopy(self.path)
        path['closed'] = False
        self.assertNotEqual(fingerprint(path, True), fingerprint(self.path, True))

    def test_deduplicated_triangle_counts(self):
        path = copy.deepcopy(self.path)
        for point in path['pts']:
            point['l'], point['r'] = list(point['a']), list(point['a'])
        duplicate = copy.deepcopy(path)
        duplicate['index'] = 2
        source = Path('synthetic.json')
        raw = json.dumps({'paths': [path, duplicate]}).encode()
        with patch.object(Path, 'read_bytes', return_value=raw):
            result = benchmark(source, 1.5)
        self.assertEqual(result['raw_contours'], 2)
        self.assertEqual(result['cyclic_and_reversal_geometry_fingerprints'], 1)
        self.assertEqual(result['counts']['input_anchors'], 3)
        self.assertEqual(result['counts']['valid_local_joins'], 3)
        self.assertEqual(result['counts']['clip_flat'], 2)
        self.assertEqual(result['counts']['native_miter'], 3)
        self.assertEqual(result['explicit_join_failures'], {})


if __name__ == '__main__':
    unittest.main()
