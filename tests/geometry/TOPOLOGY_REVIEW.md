# Independent topology review log

Review date: September 13, 2026 (UTC+07:00). **Status: APPROVED for the bounded standalone polygon and value-API experiment at snapshot `f92abc24f98fed3e57b8c46ce4923f2c3a8556acb162f55836aa33576a2f659d`.** The original protected-retention failure, original remote-envelope example, and multipart-envelope bypass are closed by the inspected correction and independent reruns below. No remaining actionable blocker was found in that reviewed scope. This decision is not production, native-host, curve, or universal numerical certification. Earlier rejected snapshots and failures remain recorded as historical evidence.

This log records independent source inspection and standalone executable results. It performs no Adobe host operation and establishes no native, font, curve, or production support. Evidence links are repository-relative; generated executables and earlier local snapshots remain ignored build artifacts and may be absent from another checkout. Hashes below identify the actual reviewed bytes; they are not cross-checkout text invariants.

## Reviewed identities

| September 13 review stage | Artifact | Raw SHA-256 |
| --- | --- | --- |
| Original implementation | Geometry source | `7ca944cc672692b7dc6a1a8309078627f4777941ec551a63d02aabcabfc1a6d9` |
| Original implementation | CLI executable | `4c9270e6c16bc980306b971cea353db5ed061690e0a0d57fe906945c8cfa6961` |
| Original and refactored test corpus | *test_topology_region.py* | `79bf3f82e6d321cbfcf6abbc5bb6a4336ebd4c02ae4b9a07547d53a5a20f5c66` |
| Refactored implementation | *topology_region.cpp* | `f4460f15bb624b6fe0e4383380ca30871172339e8e9d234f5fbdfde9583038ce` |
| Refactored implementation | *topology_region_core.h* | `97f01eb213e30463bd6161ee2341e3725d284182fadff13c2f2092dc0d17ffa7` |
| Refactored implementation | CLI executable | `77b0330ca1a173cafa26dde76fe2466ba18509010c51984d94951afd24969350` |
| Refactored implementation | Core API smoke executable | `6c4e499650ab2b7b7d823ee3903dcd6af69d928b3cd1ef7de64d707b0e8d9378` |

The earlier freeze's declared snapshot identifier is `7058e8ec92490f9e8d409e47db4b49b109de29bcf895b30c17bce540b7176981`. The rejected [freeze](topology_freeze_rejected_6e7d8faf.json) declares snapshot `6e7d8faf5a519cca33f9869ab90d2dc82904a68b9edbb727f4cfc681ff3a9170`; the freeze file itself has raw SHA-256 `765d2a01b6ef904829b43ae9fe24cec4e31139311d936e07e31cca70553cc999`. These are different identifiers. Every listed source and local build hash matched at the final read-only verification. This log is subsequent review evidence and was not part of that freeze.

## Original findings and observed corrections

**P1 — complete protected witness loss; original example now refused.** Source F is [0, 1]², h = 0.1, and envelope E is [−1, 2]² with the rectangle [0.49995, 0.50005] × [0.4995, 0.5005] removed. That removed rectangle is also supplied as a protected witness. The original executable returned success, excluded its center (0.5, 0.5), and reported normalized fill loss `1.0000000000005664e-07`. The whole witness fit the absolute area budget, and its residual vertices lay on the hole boundary, defeating the vertices-only distance check. Original source lines 130–138 and 216–218 implemented the relevant checks.

The refactored [containment implementation](../../tools/topology_region.cpp) at reviewed lines 162–181 checks the residual interior against a distance band. Reviewed lines 184–197 additionally require each original witness independently to retain positive area and at least 0.999999 of its area, with relative residual at most 10⁻⁶. The exact original example is refused. Separate regressions place a witness outside F but inside the offset candidate and remove either all or half of it; both are refused, exercising witness retention without relying on source containment to reject first. These observations close the reproduced P1 cases, not every possible numerical retention failure.

**P2 — remote envelope enlarges the clipped result; original example now refused.** Source F is [0, 1]², h = 0.1, and E is [−100000, 100000]² with [1.0001, 1.2] × [0.2, 0.8] removed. The original executable returned area `1.3800725` instead of analytic `1.38006` and maximum x `1.10005` instead of candidate maximum `1.1`, while reporting zero outside-envelope and fill-loss residuals. Source normalization did not bound the rescale grid of a later operation involving the distant envelope. Original source lines 207–211 performed the relevant intersection and checks.

The refactor reads the actual Boost overlay rescale multiplier, rejects a grid larger than half the normalized distance budget, and checks the result against the offset candidate. The exact original example is refused. Reviewed lines 149–157, 239, 261–262, and 269 contain these changes. This closes the single-polygon reproducer; the multipart bypass below prevents closing the broader P2 concern.

## Historical multipart P2: envelope union bypasses the precision guard

**Confidence: high for this exact executable and input. Status: CLOSED in the final reviewed snapshot below; OPEN in the historical `6e7d8faf…` snapshot.** Reviewed source line 118 calls `bg::union_` without the new precision guard. At reviewed line 238, envelope parts are unioned before the source/envelope guard at line 239. A coarse union can erase the intended hole and return a valid rectangle. The subsequent `rectangular_cover` shortcut then accepts that altered envelope, so later checks cannot detect the lost input constraint.

Exact input: source [0, 1]², h = 0.1, default miter join and limit 4. Supply the following four rectangles as separate envelope polygons, in this order. Each tuple is (xmin, ymin, xmax, ymax).

```text
left:   (-100000, -100000, 1.01,    100000)
right:  (1.025,   -100000, 100000,  100000)
bottom: (-100000, -100000, 100000,  0.51)
top:    (-100000, 0.525,   100000,  100000)
probe:  (1.0175, 0.5175)
```

The exact union is the large square minus [1.01, 1.025] × [0.51, 0.525]. The intended hole has area 0.000225, and its center is 0.0075 from the true envelope. Source normalization gives S = 1. The hole lies entirely within the ordinary offset candidate [−0.1, 1.1]², whose area is 1.44. Therefore the analytic clipped area is 1.439775, and the probe must be outside the result. The hole's area and center distance both exceed the declared numerical budgets.

Repeated execution produced identical parsed responses: `ok: true`, area `1.4400000000000004`, zero holes, `probes: [true]`, and zero `outside_area_normalized`, `lost_fill_area_normalized`, and `clipped_area_normalized`. The same result was reproduced after the final freeze was supplied and its executable hash was verified. This is an actual accepted violation of the supplied envelope, not an inference from the absence of a test.

The following JSON request is the portable reproducer passed to the existing CLI; it creates no files and invokes no host:

```json
{
  "source": ["POLYGON((0 0,1 0,1 1,0 1,0 0))"],
  "envelope": [
    "POLYGON((-100000 -100000,1.01 -100000,1.01 100000,-100000 100000,-100000 -100000))",
    "POLYGON((1.025 -100000,100000 -100000,100000 100000,1.025 100000,1.025 -100000))",
    "POLYGON((-100000 -100000,100000 -100000,100000 0.51,-100000 0.51,-100000 -100000))",
    "POLYGON((-100000 0.525,100000 0.525,100000 100000,-100000 100000,-100000 0.525))"
  ],
  "h": 0.1,
  "probes": [[1.0175, 0.5175]]
}
```

Correction requested at that review: apply an appropriate precision check before union can change input topology, audit every union call for the same ordering problem, and add this multipart regression. Refusal without geometry is acceptable for this deliberately remote envelope. A valid geometry result must satisfy the independently known hole and area constraints. The final inspection and executed results below establish closure separately.

## Independent verification and remaining limits

The independently executed refactored suite comprised 34 tests: the 29 original tests, four [retention regressions](test_topology_retention.py), and one [core caller test](test_topology_core.py). All passed in 50.121 seconds, with 139 successful request checks and maximum normalized analytic area error `1.1520001885756415e-07`. Source, header, test, CLI executable, and core caller executable hashes were unchanged across that run. The complete suite's separately reported 73 passes are not presented as a second independent execution by this review. The [223-response replay record](topology_api_equivalence.json) reports zero differences across API extraction; that historical replay was inspected, not independently repeated.

No additional actionable explicit-role or C++ exception-flow defect was found in the inspected snapshot. Shell/hole roles are supplied explicitly; orientation correction does not establish a raw nonzero or even-odd contour resolver. Invalid-role, open-ring, reversal, and degenerate-input refusal tests pass. The value API returns owned geometry, leaves the request unchanged, and translates caught failures into empty outputs; the caller test covers late failure, allocation failure, and recovery. These finite results do not prove host art ownership, Windows structured-exception recovery, resource-security bounds, or all numerical inputs.

The [design](../../TOPOLOGY_GEOMETRY_DESIGN.md), [API contract](TOPOLOGY_CORE_API.md), and [measurement record](topology_evidence.json) remain supporting engineering records, not substitutes for the counterexample. Their mutable current contents may later describe another revision; the hashes above define this review's scope. The standalone review neither redistributes SDK source nor changes external dependency packaging. No native, curved-font, or full-rendering approval follows from these tests.

## Final bounded approval, September 13, 2026

The final [freeze](topology_freeze.json) declares snapshot `f92abc24f98fed3e57b8c46ce4923f2c3a8556acb162f55836aa33576a2f659d`; its file has raw SHA-256 `6709e9a7d612544c20b498c186279d78974551c220d98907e09105033bfa30b7`. All pinned source and local build hashes matched before and after independent test execution. The review log and independent fuzz test are subsequent evidence, not members of that declared snapshot.

| Final reviewed artifact | Raw SHA-256 |
| --- | --- |
| Geometry source | `3bab6ab77ef58acd9aade359b6f58c5cd7a29cb61f653419d9de1d5979559c42` |
| Public header, unchanged | `97f01eb213e30463bd6161ee2341e3725d284182fadff13c2f2092dc0d17ffa7` |
| CLI executable | `07acb688e8aec59949aeb986a1be3810082dbd983e73dd5d85e0a5cd1905b676` |
| Core API smoke executable | `9f48ae6b2f7d9f15bd38b1f99f87ea8cee5a51a65a9c85e0b20de3cc49f02e1f` |
| Independent rectangle fuzz test | `4d876899abd51e70872b4d74df3ad437568e01cb758719ea51c5ad574676f494` |

At final source lines 121–123, `unite` checks the rescale policy on each pair of operands before invoking union. Source, envelope, and expected-region assembly all use this guarded function. The separate symmetric-comparison union has its own guard at lines 302–303. Inspection found no unguarded union call remaining. The earlier interior-containment, per-witness retention, and result-versus-candidate checks remain present.

The independent rerun used the exact wide bottom/top bars printed above, which overlap the left/right bars. The committed multipart regression uses narrow bottom/top bars spanning only x = 1.01 to 1.025. Both represent the same mathematical envelope, but their numerical operation sequences differ; their evidence is not conflated. All 24 permutations of the exact wide-bar remote case are now refused with an envelope-union precision diagnostic and no geometry. All 24 corresponding tight controls with extent 2 succeed, retain one hole, exclude the center probe, and have maximum absolute area error `4.440892098500626e-16` from analytic 1.439775. The committed narrow-bar permutation and expected-region regressions also pass. The [retained union regression record](topology_union_regression.json) separately preserves the earlier-versus-corrected measurements.

Independent geometry discovery found 78 tests: 72 passed and six optional font-provenance tests skipped under the system Python environment; the run took 47.677 seconds. Those six tests were then explicitly rerun in the repository environment and all passed in 0.105 seconds. Thus every discovered test was executed successfully across the two stated environments; this is not presented as one skip-free invocation. The standalone core caller independently reports 43 passing checks. The original topology corpus still records 139 successful requests and maximum normalized analytic area error `1.1520001885756415e-07`.

The independent [rectangle-union fuzz test](test_topology_rect_fuzz.py), seed 20260913, passes both concave sanity controls and 80 integer-grid union cases with h = 0.125 or 0.5 and miter limit 4. All 80 were accepted and validated; zero were refused. Exact rational sweep-area and independent ray-membership checks cover 6,415 points strictly away from expected boundaries, with maximum normalized area error `1.925604206959827e-08`. Executable identity remained unchanged during the run. These cases exercise orthogonal positive buffering; their union-dilation oracle is not generalized to arbitrary nonorthogonal geometry.

**Decision: APPROVED within the declared standalone scope.** The reproduced P1 and both P2 failures are closed for the final identities above. Numerical acceptance remains bounded and uses the same construction library for some diagnostics; the finite corpus does not prove a universal bound. Raw winding resolution, curves, host art ownership, automatic style synchronization, missing-plugin behavior, and production performance remain outside this approval. No core, host, SDK, or retained execution record was modified by this review. A later source or executable change requires checking its relevance to this decision rather than inheriting approval solely from a passing historical freeze.
