"""Run the bounded prototype tests and save reproducible analytic measurements."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import subprocess
import time
import unittest

import test_topology_region as cases
import test_topology_retention as retention
import test_topology_core as core


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    started = time.perf_counter()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(cases.TopologyRegionTests)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(retention.TopologyRetentionTests))
    suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(core.TopologyCoreTests))
    test_result = unittest.TextTestRunner(verbosity=1).run(suite)
    elapsed = time.perf_counter()-started
    rectangle = cases.request([cases.wkt(cases.rect(0,0,4,3))])
    clipping = cases.request([cases.wkt([(0,0),(8,-1),(8,1),(0,0)])],
                             [cases.wkt(cases.rect(-1,-2,9,2))],limit=20)
    thin = cases.request([cases.wkt(cases.rect(0,0,10,.0001))],h=.1)
    numbers = (16,32,64,128,256)
    queries = [clipping,thin] + [dict(rectangle,join="round",circle_points=n) for n in numbers]
    measured = cases.batch(queries)
    clip = measured[0]
    if not all(r.get("ok") for r in measured):
        raise RuntimeError(measured)
    max_excursion = max(max(-1-x,x-9,-2-y,y-2,0)
                        for p in clip["geometry"] for x,y in p[0])
    for r in measured:
        r.pop("geometry")
        r.pop("probes")
    round_errors = [{"circle_points":n,"area":r["area"],
                     "area_error_from_disk_dilation":26+math.pi-r["area"],
                     "analytic_chord_sagitta":1-math.cos(math.pi/n)}
                    for n,r in zip(numbers,measured[2:])]
    a,h,d = 7.5,1.5,.3
    x = a*math.tan(math.pi/12)
    mouth = d*math.tan(math.pi/12)
    closed_queries = [
        {"source":[cases.wkt(cases.rect(-7.5,-12,7.5,d),[(0,0),(x,-a),(-x,-a),(0,0)])],
         "h":h,"limit":4,"compare_boundary":True},
        {"source":[cases.wkt([(-5,-8),(5,-8),(5,d),(mouth,d),(0,0),(-mouth,d),(-5,d),(-5,-8)])],
         "h":h,"limit":4,"compare_boundary":True}]
    closed_results = cases.batch(closed_queries)
    closed_comparisons = []
    for req,r in zip(closed_queries,closed_results):
        if not r.get("ok"):
            raise RuntimeError(r)
        closed_comparisons.append({"request":req,"result":r,
            "filled_top":max(y for p in r["geometry"] for x,y in p[0]),
            "fill_plus_symmetric_boundary_top":max(y for p in r["symmetric_geometry"] for x,y in p[0])})
    timed_requests = [rectangle]*100
    started = time.perf_counter()
    timed_results = cases.batch(timed_requests)
    batch_seconds = time.perf_counter()-started
    if not all(r.get("ok") for r in timed_results):
        raise RuntimeError("Timed batch contains rejected request")
    repo = Path(__file__).resolve().parents[2]
    paths = ["tools/topology_region.cpp","tools/build_topology.py",
             "tools/topology_region_core.h","tests/geometry/test_topology_region.py",
             "tests/geometry/measure_topology_region.py","tests/geometry/test_topology_retention.py",
             "tests/geometry/test_topology_core.py","tests/geometry/topology_core_smoke.cpp",
             "tests/geometry/build_core_api.py","tests/geometry/TOPOLOGY_CORE_API.md",
             "tests/geometry/topology_api_equivalence.json","tests/geometry/topology_union_regression.json",
             "tests/geometry/topology_freeze_rejected_6e7d8faf.json"]
    hashes = {p:hashlib.sha256((repo/p).read_bytes()).hexdigest() for p in paths}
    equivalence=json.loads((repo/"tests/geometry/topology_api_equivalence.json").read_text(encoding="utf-8"))
    if any(hashes[p] != digest for p,digest in equivalence["after_source_sha256"].items()):
        raise RuntimeError("Historical API replay does not match current core sources")
    build_log = (cases.EXE.parent/"compile.log").read_text(encoding="utf-8",errors="replace")
    core_exe=cases.EXE.parent/"core_api/topology_core_smoke.exe"
    core_result=json.loads(subprocess.run([str(core_exe)],capture_output=True,text=True,check=True).stdout)
    report = {
        "status":"PASS" if test_result.wasSuccessful() else "FAIL",
        "scope":"Explicit-role linear filled-region buffer, optional external envelope, and symmetric boundary comparison; no host or font proof",
        "boost_version":clip["boost_version"],"overlay_mode":clip["overlay_mode"],
        "source_hashes_sha256":hashes,
        "executable_sha256":hashlib.sha256(cases.EXE.read_bytes()).hexdigest(),
        "core_api_executable_sha256":hashlib.sha256(core_exe.read_bytes()).hexdigest(),
        "core_api_smoke":core_result,
        "review_regressions":{"count":7,"status":"included in this run; pre-union repair awaits independent re-review"},
        "input_union_regression":json.loads((repo/"tests/geometry/topology_union_regression.json").read_text(encoding="utf-8")),
        "cli_api_equivalence":equivalence,
        "compiler_warning_count":sum(": warning " in line for line in build_log.splitlines()),
        "tests":test_result.testsRun,"failures":len(test_result.failures),
        "errors":len(test_result.errors),"skips":len(test_result.skipped),
        "successful_requests_checked":cases.TopologyRegionTests.requests_checked,
        "max_normalized_analytic_area_error":cases.TopologyRegionTests.max_normalized_analytic_error,
        "test_wall_seconds":elapsed,
        "clip_request":clipping,"clip_measurement":measured[0],
        "clip_independent_rectangle_vertex_excursion":max_excursion,
        "thin_stem_request":thin,"thin_stem_measurement":measured[1],
        "round_convergence":round_errors,
        "closed_region_comparisons":closed_comparisons,
        "timing":{"requests":100,"process_count":1,"wall_seconds":batch_seconds,
                  "includes":"process startup, JSON I/O, validation, buffer, and overlays; four-vertex rectangle only"},
        "limitations":["Native W geometry and fill semantics unverified; positive filled buffer is the closed-region proposal",
                       "No envelope semantics for open unfilled paths","Numerical budgets are not exact containment proof",
                       "No native or extreme-font pass","reviewer identified P1/P2; fixes await independent re-review"],
    }
    args.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"Evidence written: {args.output}")
    return 0 if test_result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
