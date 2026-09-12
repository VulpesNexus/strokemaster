"""Independent review's witness-loss and input-overlay precision regressions."""
import itertools
import unittest
from test_topology_region import batch, request, rect, wkt


class TopologyRetentionTests(unittest.TestCase):
    @staticmethod
    def decomposed_envelope(extent):
        # Preserve the reported short decimal spelling for the WKT parser.
        def polygon(box):
            return "POLYGON((" + ",".join(f"{x} {y}" for x,y in rect(*box)) + "))"
        return [polygon((-extent,-extent,1.01,extent)),
                polygon((1.025,-extent,extent,extent)),
                polygon((1.01,-extent,1.025,.51)),
                polygon((1.01,.525,1.025,extent))]

    def test_coarse_decomposed_envelope_refused_before_union(self):
        queries = [request([wkt(rect(0,0,1,1))],list(parts),h=.1,
                           probes=[[1.0175,.5175]])
                   for parts in itertools.permutations(self.decomposed_envelope(100000))]
        for i,result in enumerate(batch(queries)):
            with self.subTest(order=i):
                self.assertIn("envelope union: overlay grid=",result.get("error",""))
                self.assertNotIn("geometry",result)

    def test_tight_decomposed_envelope_preserves_analytic_hole(self):
        queries = [request([wkt(rect(0,0,1,1))],list(parts),h=.1,
                           probes=[[1.0175,.5175],[1.005,.5175]])
                   for parts in itertools.permutations(self.decomposed_envelope(2))]
        for i,result in enumerate(batch(queries)):
            with self.subTest(order=i):
                self.assertTrue(result.get("ok"),result)
                self.assertEqual(result["holes"],1)
                self.assertEqual(result["probes"],[False,True])
                self.assertAlmostEqual(result["area"],1.2**2-.015**2,delta=2e-7)
                self.assertLessEqual(max(x for p in result["geometry"] for x,y in p[0]),1.1+2e-6)

    def test_coarse_expected_input_refused_before_union(self):
        query = request([wkt(rect(0,0,1,1))],h=.1,
                        expected=self.decomposed_envelope(100000))
        result = batch([query])[0]
        self.assertIn("expected union: overlay grid=",result.get("error",""))
        self.assertNotIn("geometry",result)

    def test_protected_witness_outside_source_cannot_disappear(self):
        witness = rect(-.05005,.4995,-.04995,.5005)
        query = request([wkt(rect(0,0,1,1))],
                        [wkt(rect(-1,-1,2,2),witness)],h=.1,
                        protected=[wkt(witness)])
        result = batch([query])[0]
        self.assertIn("protected witness must retain",result.get("error",""))
        self.assertNotIn("geometry",result)

    def test_partial_protected_loss_is_not_subsidized_by_absolute_budget(self):
        witness = rect(-.05005,.4995,-.04995,.5005)
        removed_half = rect(-.05005,.4995,-.05,.5005)
        query = request([wkt(rect(0,0,1,1))],
                        [wkt(rect(-1,-1,2,2),removed_half)],h=.1,
                        protected=[wkt(witness)])
        result = batch([query])[0]
        self.assertIn("protected witness must retain",result.get("error",""))
        self.assertNotIn("geometry",result)

    def test_volta_complete_protected_loss_is_refused(self):
        missing = rect(.49995,.4995,.50005,.5005)
        query = request([wkt(rect(0,0,1,1))],
                        [wkt(rect(-1,-1,2,2),missing)],h=.1,
                        protected=[wkt(missing)],probes=[[.5,.5]])
        result = batch([query])[0]
        self.assertIn("error",result)
        self.assertNotIn("geometry",result)

    def test_volta_remote_envelope_precision_is_refused(self):
        query = request([wkt(rect(0,0,1,1))],
                        [wkt(rect(-100000,-100000,100000,100000),
                             rect(1.0001,.2,1.2,.8))],h=.1)
        result = batch([query])[0]
        self.assertIn("error",result)
        self.assertNotIn("geometry",result)


if __name__ == "__main__":
    unittest.main(verbosity=2)
