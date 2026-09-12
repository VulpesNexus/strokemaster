# Bounded native-caller API

Status: implemented and tested as a standalone C++ caller. The preceding snapshot was rejected because input union could erase a hole before its precision check; the pre-union repair awaits independent review, and native integration remains blocked. This note supersedes the earlier prototype's vertex-only containment and absolute-budget interpretation of protected retention. The native adapter owns art lifetime and publication. This API does not prove curved-font support or production host behavior.

## Caller contract

Include [*topology_region_core.h*](../../tools/topology_region_core.h). Compile [*topology_region.cpp*](../../tools/topology_region.cpp) as a separate translation unit with `ISE_TOPOLOGY_CORE_ONLY`, using C++17 and the existing Boost headers. That build excludes JSON/property-tree parsing, WKT parsing calls, and `main`. The public header contains only standard C++ value types. Use the same toolchain and CRT as the caller; this is not a stable binary DLL interface.

`ise::topology::Request` contains `source` (a vector of polygons), `h`, `limit`, and `join` (default `Join::Miter`). A polygon has `outer` and `holes`, each represented by arrays of `Point {x, y}`. Every ring must repeat its first coordinate at the end. The fields establish shell/hole roles independently of orientation. Coordinates and positive h share one length unit; limit is dimensionless. Optional fields are `envelope`, `protected_regions`, `expected`, `compare_boundary`, `probes`, and `circle_points`. The minimal native experiment needs only source, h, and limit.

Call `Result result = ise::topology::process(request)`. This function is `noexcept` for C++ exceptions. `Status::Ok` carries caller-owned original-coordinate `geometry` and numeric `diagnostics`. Other statuses are `Refused`, `ResourceFailure`, and `InternalFailure`; every failure has empty geometry, comparison geometry, and probes, with a NUL-terminated message of at most 511 bytes. Input stays unchanged and no result points alias input storage. Request construction/allocation remains the caller's responsibility. Neither header nor implementation owns Adobe handles, changes style, performs I/O in the core-only build, or publishes art.

Diagnostics report source/result area in squared coordinate units; fields ending in `_normalized` use coordinates divided by S = max(source bounding-box width, height, h). LengthTolerance is relative to S, and AreaTolerance is relative to S².

The initial adapter should use generated, valid, closed polygons and an analytic triangular hole with uniform equal gray fill/stroke. Convert to one declared coordinate system, apply the effective h/limit, call the core, and create replacement art only after `Status::Ok` and caller validation. On refusal, preserve the original art. Do not infer permission to operate a host from this API note.

Input remains bounded to **256 polygons and 20,000 vertices per polygon list**, counting closures and holes. **No Bézier decoding, flattening, raw fill-rule resolution, or curved-font support exists.** Keep the established near-cusp, scale, and degeneracy refusals. Above-limit Boost miters still differ from native bevel fallback. No Adobe or font source is redistributed.

## Review findings and corrections

- **P1 reproduced before repair:** source [0, 1]², h = 0.1, envelope [−1, 2]² minus [0.49995, 0.50005] × [0.4995, 0.5005], with that removed rectangle also protected, returned success and excluded (0.5, 0.5). Its complete 10⁻⁷-area loss fit the absolute budget, and residual vertices lay on the retained boundary. The corrected path refuses with `source versus envelope: residual interior exceeds distance band`.
- **Interior check:** a nonempty residual must pass the area/vertex checks and leave no polygonal remainder outside a round polygonal distance band of the target. This includes residual interiors instead of merely sampling boundary vertices. Construction and diagnostics still use Boost and remain numerical acceptance checks, not exact arithmetic certification.
- **Witness retention:** each protected input polygon independently must retain positive area and at least 0.999999 of its area, with residual area no greater than its own area × 10⁻⁶. Witnesses are not unioned before that check. A large witness cannot subsidize losing a smaller witness, and the global 2 × 10⁻⁷ area budget does not authorize complete or material partial witness loss. Separate regressions exercise complete and half-witness removal outside the original source, so source containment cannot mask the protected-retention check.
- **P2 reproduced before repair:** the large envelope [−100000, 100000]² with hole [1.0001, 1.2] × [0.2, 0.8] returned area 1.3800725 instead of 1.38006 and x maximum 1.10005 instead of 1.1. The corrected guard reads the actual Boost rescale multiplier and refuses its 0.02 normalized grid, which exceeds half the 2 × 10⁻⁶ distance budget. The result must also pass containment against the un-clipped offset candidate. A containing, hole-free axis-aligned rectangle uses an exact bounding-box shortcut instead of a needless coarse intersection. This admits the former loose rectangle controls without exempting envelopes with holes.

The fixes were tested before the API extraction. All 29 original tests passed, plus four new retention regressions. A 223-request capture of the fixed CLI was then replayed after extraction; all parsed JSON responses, including refusal messages and coordinates, matched exactly. The compile/link test calls only the array API and checks deterministic refusal, ownership, late failure after geometry construction, allocation failure, recovery, both reported counterexamples, and the triangular-hole excess. Its compiler include trace contains no JSON/property-tree headers.

## Review and reproduction

The subsequent input-union counterexample uses the unit source square, h = 0.1, and four rectangles forming a large envelope with an excluded [1.01, 1.025] × [0.51, 0.525] square. The archived binary falsely accepted 16 of the 24 rectangle orders, reporting area 1.4400000000000004, zero holes, and the hole center included, instead of analytic area 1.439775. The literal left/right/bottom/top order refused at a later check in this run; the recorded failure depends on input order. See the exact requests and outputs in the [union regression evidence](topology_union_regression.json).

`unite` now checks the actual operand-pair rescale grid before each union, covering source, envelope, and expected-input lists. The separate symmetric-comparison union also checks its operands before execution. There is no retrospective exemption based on an already-unioned rectangular result. All 24 coarse orders refuse with `envelope union: overlay grid=0.02 exceeds normalized 9.9999999999999995e-07`. Tightening the rectangle extent to [−2, 2] preserves the analytic hole in all 24 orders, with maximum measured area error 4.440892098500626 × 10⁻¹⁶. The expected-input route also has a refusal regression. These guards bound the measured overlay grid; they do not prove arbitrary feature retention below that grid or certify Boost's internal buffer operations.

The public array API is unchanged. Its direct caller now executes 43 checks, including coarse decomposition refusal and tight decomposition retention. The earlier 223-request pre-extraction capture was replayed against this repaired source; the replay record identifies the exact compared source hashes. The [rejected manifest](topology_freeze_rejected_6e7d8faf.json) is retained unchanged. Its matching source, evidence, and binaries are archived locally under *topology_build/rejected_6e7d8faf/*; the new freeze explicitly supersedes it.

The independent reviewer should review `containment`, `protected_retention`, `overlay_precision`, `rectangular_cover`, and `process_impl`'s result-versus-candidate check. The API extraction preserves their fixed geometry path. Remaining concerns are the numerical backend and precision budgets, unsupported curves/raw topology, and native art ownership outside this core. A passing direct caller does not clear the native integration gate by itself.

The [evidence](topology_evidence.json) and [freeze](topology_freeze.json) include the new header, API tests, retention regressions, this contract, and the core-only executable hash. The prior freeze is superseded; local pre-fix evidence/source and the pre-extraction response capture are retained under ignored *topology_build/* for review. The [historical replay record](topology_api_equivalence.json) pins both source versions and the capture hash. Measurement verifies that record's current-source hashes; it does not repeat the historical comparison or require the local capture to rerun the tests. The oracle graphic continues to show the triangle-hole excess and the notch agreement control.

Executed from the repository:

```powershell
./.venv/Scripts/python.exe -B tests/geometry/build_core_api.py --boost-root '<sdk-root>/Adobe InDesign 2026 SDK/external/dva/third_party/boost_libraries' --msvc-root 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.42.34433' --windows-sdk 'C:/Program Files (x86)/Windows Kits/10'
./.venv/Scripts/python.exe -B -m unittest discover -s tests/geometry -p 'test_*.py' -q
./.venv/Scripts/python.exe -B tests/geometry/measure_topology_region.py --output tests/geometry/topology_evidence.json
./.venv/Scripts/python.exe -B tests/geometry/freeze_topology.py
./.venv/Scripts/python.exe -B tests/geometry/freeze_topology.py --check
```
