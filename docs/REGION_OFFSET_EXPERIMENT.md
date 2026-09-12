# Filled-region outward-offset experiment

**Decision:** advance a topology-aware filled-region construction, with an explicit surrounding outer-envelope constraint and preserved ordinary exterior miters. A one-sided local offset alone is insufficient. Euclidean dilation guarantees the relevant containment but rounds exterior miters; selected exterior miter additions can retain them, provided their geometry is compatible with that envelope. This is analytic research, not a production offset implementation or host proof.

## Acceptance criterion and terms

The original *W.jpg* objective remains unchanged: inner spikes must never pass the surrounding outer stroke envelope. The √2 local clip is not that guarantee. The [previous synthetic figure](../tests/geometry/local_vs_global_envelope.png) supplies the negative control.

Let F be the closed filled region after resolving containment, holes, overlaps, and the fill rule. Let h be the intended outward distance: h = 1.5 pt for the measured 3 pt centered stroke. This does not imply that a separately specified outside-aligned stroke should have its width halved. Let O be a defined surrounding reference region and E its intended outer stroke envelope, including allowed ordinary exterior miters. For the closed-hole examples O is the parent rectangle with the hole filled; for the open-notch example its surrounding rectangle is explicitly supplied. No rule deriving O for arbitrary open W-like notches, or replacing O by a convex hull, is asserted here.

## What region dilation proves

Euclidean dilation is Dₕ(F) = {p : distance(p, F) ≤ h}. If F ⊆ O, then Dₕ(F) ⊆ Dₕ(O): every radius-h disk centered in F is also centered in O. In particular, if O has top y = d, the dilation cannot pass y = d + h. This proof is independent of contour traversal direction. Convexity must be classified relative to filled material: a convex triangular hole corner is a reflex corner of F.

For the downward triangular hole H with apex (0, 0), depth a, and half-angle α, write s = sin α and c = cos α. Its eroded interior consists of points satisfying both ±cx + sy < −h and y > −a + h. The surviving apex moves **down** to y = −h/s, into the hole, rather than generating the symmetric stroke’s upward tip +h/s. Empty eroded holes disappear; retaining inverted offset loops would violate this set definition.

A constructive positive result for this family is R = E_rect minus the eroded hole interior, where E_rect expands each parent-rectangle side by h. Then F ⊆ R ⊆ E_rect, and all four ordinary exterior right-angle miters remain. At h = 1.5, d = 0.3, α = 15°, and a = 7.5, the symmetric limit-4 tip reaches y = 5.795555 and the local √2 clip reaches y = 2.121320; both exceed E_rect’s top 1.8. R admits neither excursion. This demonstrates that region construction can meet both requirements for a specified family; it does not generalize the rectangle formula to arbitrary concave polygons.

## Counterexamples that constrain the architecture

- **Round dilation loses ordinary miters.** The expanded rectangle’s right-angle tip is √2h from its original corner, outside Dₕ(F). Analytic shifted support lines also preserve admitted exterior miters at 30°, 60°, 90°, and 120°, while the Euclidean radius-h dilation rounds them. “Dilation” therefore needs a declared join policy.
- **Masking an existing symmetric stroke is ineffective.** For a symmetric stroke region S, F ∪ (S minus F) = F ∪ S. The unwanted tip lies outside F, so an outside-only mask retains it.
- **One-sided shifted lines still need topology resolution.** An open V notch with vertex (0, 0) and mouth (±d tan α, d) has inward-to-notch offset lines meeting at y = h/s. In the narrow example that intersection lies beyond the finite notch edges and above d + h. Connecting offset lines without resolving intersections and disappearing notches is not a valid region dilation.
- **Mitering arbitrary decomposition pieces creates new spikes.** Split [0, 2]² along y = x. Outward-shifting the lower/right triangle’s support lines by h produces the vertex (−(1 + √2)h, −h), outside the square’s intended miter envelope [−h, 2 + h]². Unioning independently mitered convex pieces is therefore not equivalent to offsetting the original boundary; internal seams must not receive miter joins.
- **Topology changes are required.** A 6-by-8 right-triangular hole vanishes at its exact inradius 2. Two differently sized rectangular holes vanish at different distances. Two filled rectangles separated by a gap 2h meet under dilation. Keeping the original loop inventory cannot represent these outcomes.

## Proposed contract for the next experiment

Require F ⊆ R ⊆ E. Preserve the ordinary miter geometry of protected exterior convex joins wherever that geometry lies within E. Resolve concave intersections, shrinking counters, disappearing holes, and component mergers as region operations. Do not construct inward symmetric miter patches and then merely mask them against F. Zero-width contours, unresolved cusps, and ambiguous topology remain explicit unsupported cases.

One testable set specification is R = (Dₕ(F) ∪ J) ∩ E, where J contains only the permitted exterior miter patches of the actual filled boundary. If F ⊆ E and every protected patch is within E, this guarantees fill retention and envelope containment while retaining those patches. It does not prescribe an implementation, establish how to derive E for every glyph, or promise that separate joins remain visibly distinct after overlap. A round envelope that excludes an ordinary miter is incompatible with preserving that miter; the intended E must include the exterior joins the original objective protects. This is an engineering constraint, not a new user-choice request.

## Executed evidence and limits

*tests/geometry/test_outward_region.py* contains only independent analytic rectangle, triangle, and notch models; it imports no production join code, font tooling, host API, or Boolean library. **13 tests pass, zero failures, errors, or skips**, including the positive hole construction, the five counterexample classes above, and reflection/similarity controls. Finite membership probes supplement the stated set proofs; they do not establish a generic polygon offset algorithm. Run:

```powershell
python -B tests/geometry/test_outward_region.py
```

The existing font-provenance discrepancy, renderer disagreement, and 179 cusp/unresolved-turn failures remain outside this experiment. No font pass, production implementation, Adobe operation, or change to the local join policy is claimed. Only this document and the new analytic test file were written for this stage.
