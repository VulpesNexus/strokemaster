"""Run the compiled value-API caller without JSON or WKT in the core build."""
import json
from pathlib import Path
import subprocess
import unittest


class TopologyCoreTests(unittest.TestCase):
    def test_native_array_api_smoke(self):
        exe=Path(__file__).parent/"topology_build/core_api/topology_core_smoke.exe"
        self.assertTrue(exe.is_file(),"Build tests/geometry/build_core_api.py first")
        result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30,check=True)
        self.assertEqual(result.stderr,"")
        data=json.loads(result.stdout)
        self.assertTrue(data["ok"])
        self.assertGreaterEqual(data["checks"],30)


if __name__=="__main__":
    unittest.main(verbosity=2)
