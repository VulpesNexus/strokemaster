"""Independent analytic region experiments, not a polygon offset implementation.

Only fixed rectangles, triangular holes, and a specified open notch are modeled.
No production join kernel, font, host, or Boolean library supplies the oracle.
Boundary points belong to the closed filled region; hole interiors are open.
"""
import math
import unittest


def in_box(point, box):
    x, y = point
    left, bottom, right, top = box
    return left <= x <= right and bottom <= y <= top


def box_distance(point, box):
    x, y = point
    left, bottom, right, top = box
    return math.hypot(max(left-x, 0, x-right), max(bottom-y, 0, y-top))


def expanded_box(box, radius):
    left, bottom, right, top = box
    return (left-radius, bottom-radius, right+radius, top+radius)


def triangle_hole_clearance(point, depth, half_angle):
    """Minimum inward distance to the three support lines of the downward triangle.

    Triangle: (0,0), (+/-depth*tan(half_angle), -depth).
    Positive clearance means inside. For an inside point this is distance to fill.
    """
    x, y = point
    s, c = math.sin(half_angle), math.cos(half_angle)
    return min(-c*x-s*y, c*x-s*y, y+depth)


def in_round_region(point, box, depth, half_angle, radius):
    """Exact Euclidean dilation for a box with this strictly contained triangular hole."""
    return (box_distance(point, box) <= radius and
            triangle_hole_clearance(point, depth, half_angle) <= radius)


def in_miter_box_region(point, box, depth, half_angle, radius):
    """Analytic candidate for this family: expanded box minus eroded hole interior.

    This definition is not offered as a generic mitered polygon dilation algorithm.
    """
    return (in_box(point, expanded_box(box, radius)) and
            triangle_hole_clearance(point, depth, half_angle) <= radius)


class OutwardRegionTests(unittest.TestCase):
    def setUp(self):
        self.h = 1.5
        self.d = 0.3
        self.alpha = math.pi/12
        self.depth = 5*self.h
        self.box = (-5*self.h, -8*self.h, 5*self.h, self.d)

    def test_original_hole_spike_violates_outer_envelope(self):
        s = math.sin(self.alpha)
        ordinary_tip = (0, self.h/s)
        local_clip_tip = (0, math.sqrt(2)*self.h)
        outer_top = self.d+self.h
        self.assertLess(1/s, 4)  # This is a retained ordinary limit-4 miter.
        self.assertAlmostEqual(ordinary_tip[1], 5.79555495773441)
        self.assertAlmostEqual(local_clip_tip[1]-outer_top, 0.3213203435596428)
        for point in (ordinary_tip, local_clip_tip):
            self.assertFalse(in_box(point, expanded_box(self.box, self.h)))
            self.assertFalse(in_round_region(point, self.box, self.depth, self.alpha, self.h))
            self.assertFalse(in_miter_box_region(point, self.box, self.depth, self.alpha, self.h))

    def test_triangle_is_strictly_inside_parent_and_erosion_moves_into_hole(self):
        a, s = self.depth, math.sin(self.alpha)
        for x, y in ((0, 0), (-a*math.tan(self.alpha), -a), (a*math.tan(self.alpha), -a)):
            left, bottom, right, top = self.box
            self.assertTrue(left < x < right and bottom < y < top)
        eroded_apex_y = -self.h/s
        eroded_base_y = -a+self.h
        self.assertLess(eroded_base_y, eroded_apex_y)
        self.assertAlmostEqual(triangle_hole_clearance((0, eroded_apex_y), a, self.alpha), self.h)
        middle = (0, (eroded_apex_y+eroded_base_y)/2)
        self.assertGreater(triangle_hole_clearance(middle, a, self.alpha), self.h)
        self.assertFalse(in_miter_box_region(middle, self.box, a, self.alpha, self.h))
        self.assertTrue(in_miter_box_region((0, 0), self.box, a, self.alpha, self.h))

    def test_round_dilation_does_not_preserve_ordinary_exterior_miter(self):
        # Upper right rectangle corner, retained by the ordinary limit-4 join.
        point = (self.box[2]+self.h, self.box[3]+self.h)
        self.assertAlmostEqual(box_distance(point, self.box), math.sqrt(2)*self.h)
        self.assertFalse(in_round_region(point, self.box, self.depth, self.alpha, self.h))
        self.assertTrue(in_miter_box_region(point, self.box, self.depth, self.alpha, self.h))
        self.assertTrue(in_round_region((self.box[2], self.box[3]+self.h),
                                       self.box, self.depth, self.alpha, self.h))

    def test_convex_exterior_miters_at_other_angles_are_admissible(self):
        # Convex upward triangle: +/-c*x+s*y<=0 and y>=-depth.
        # Its miter offset shifts each support line outward by h.
        for angle in (30, 60, 90, 120):
            alpha = math.radians(angle)/2
            s, c = math.sin(alpha), math.cos(alpha)
            tip = (0, self.h/s)
            self.assertLessEqual(1/s, 4)
            self.assertAlmostEqual(c*tip[0]+s*tip[1], self.h)
            self.assertAlmostEqual(-c*tip[0]+s*tip[1], self.h)
            self.assertGreaterEqual(tip[1], -self.depth-self.h)
            self.assertGreater(tip[1], self.h)  # Euclidean dilation would round it.
            self.assertGreater(s*(tip[1]+0.01), self.h)

    def test_global_candidate_preserves_fill_and_outer_miters_without_hole_spike(self):
        envelope = expanded_box(self.box, self.h)
        for x in (envelope[0], envelope[2]):
            for y in (envelope[1], envelope[3]):
                self.assertTrue(in_miter_box_region((x, y), self.box, self.depth, self.alpha, self.h))
        # Finite membership probes supplement, not prove, the set-inclusion derivation.
        original_fill_samples = 0
        for ix in range(-22, 23):
            for iy in range(-30, 11):
                p = (ix*0.5, iy*0.5)
                inside = in_miter_box_region(p, self.box, self.depth, self.alpha, self.h)
                if inside:
                    self.assertTrue(in_box(p, envelope))
                if in_box(p, self.box) and triangle_hole_clearance(p, self.depth, self.alpha) <= 0:
                    original_fill_samples += 1
                    self.assertTrue(inside)
                if in_round_region(p, self.box, self.depth, self.alpha, self.h):
                    self.assertTrue(inside)
        self.assertGreater(original_fill_samples, 100)

    def test_outside_only_mask_of_symmetric_stroke_keeps_the_bad_tip(self):
        # F union (S minus F) = F union S. An outside-only mask cannot repair the union.
        for in_fill in (False, True):
            for in_stroke in (False, True):
                self.assertEqual(in_fill or (in_stroke and not in_fill), in_fill or in_stroke)
        p = (0, self.h/math.sin(self.alpha))
        in_fill = in_box(p, self.box) and triangle_hole_clearance(p, self.depth, self.alpha) <= 0
        in_symmetric_miter_patch = True  # p is the analytic tip of that patch.
        self.assertFalse(in_fill)
        self.assertTrue(in_symmetric_miter_patch and not in_fill)
        self.assertGreater(p[1], self.d+self.h)

    def test_one_sided_open_notch_lines_need_global_intersection_resolution(self):
        # Parent top y=d, notch vertex (0,0), mouth (+/-d*tan(alpha), d).
        # Shift both notch sides into the complement by h: +/-c*x-s*y=-h.
        s, c = math.sin(self.alpha), math.cos(self.alpha)
        naive_reflex_vertex = (0, self.h/s)
        self.assertAlmostEqual(c*naive_reflex_vertex[0]-s*naive_reflex_vertex[1], -self.h)
        self.assertAlmostEqual(-c*naive_reflex_vertex[0]-s*naive_reflex_vertex[1], -self.h)
        offset_intersection_parameter = self.h*c/s
        original_finite_edge_length = self.d/c
        self.assertGreater(offset_intersection_parameter, original_finite_edge_length)
        self.assertGreater(naive_reflex_vertex[1], self.d+self.h)
        # A correct radius-h region dilation cannot exceed max(F.y)+h.
        self.assertFalse(in_box(naive_reflex_vertex, expanded_box(self.box, self.h)))

    def test_mitering_convex_decomposition_can_create_artificial_seam_spikes(self):
        # Square [0,2]^2 split along y=x. Lower/right triangle is
        # y>=0, x<=2, y-x<=0. Shift its three normalized support lines by h.
        h = 1.0
        point = (-h*(1+math.sqrt(2)), -h)
        x, y = point
        self.assertGreaterEqual(y, -h)
        self.assertLessEqual(x, 2+h)
        self.assertAlmostEqual((y-x)/math.sqrt(2), h)
        self.assertFalse(in_box(point, (-h, -h, 2+h, 2+h)))
        # Thus unioning per-triangle miter offsets is not the square's miter offset.

    def test_hole_collapse_is_topology_not_an_inverted_contour(self):
        # Right triangle x>=0,y>=0,4x+3y<=24 has exact inradius 2 at (2,2).
        def hole_clearance(p):
            x, y = p
            return min(x, y, (24-4*x-3*y)/5)
        self.assertEqual(hole_clearance((2, 2)), 2)
        self.assertGreater(hole_clearance((2, 2)), 1.9)
        self.assertFalse(hole_clearance((2, 2)) > 2)
        for h in (2, 2.1, 3):
            # Surviving hole interior would require x>h, y>h, 4x+3y<24-5h.
            self.assertGreaterEqual(7*h, 24-5*h)

    def test_two_holes_shrink_and_disappear_independently(self):
        holes = [(-5, -2, -1, 2), (1, -1, 3, 1)]
        def eroded_hole_interior(p, box, h):
            left, bottom, right, top = box
            return left+h < p[0] < right-h and bottom+h < p[1] < top-h
        for h in (0, 0.5, 1, 1.5):
            self.assertTrue(eroded_hole_interior((-3, 0), holes[0], h))
        self.assertFalse(eroded_hole_interior((-3, 0), holes[0], 2))
        self.assertTrue(eroded_hole_interior((2, 0), holes[1], 0.5))
        self.assertFalse(eroded_hole_interior((2, 0), holes[1], 1))
        for h in (0, 0.5, 1, 2):
            self.assertFalse(any(eroded_hole_interior((0, 0), box, h) for box in holes))

    def test_disconnected_components_can_merge(self):
        left, right = (-3, -1, -1, 1), (1, -1, 3, 1)
        distance = min(box_distance((0, 0), left), box_distance((0, 0), right))
        self.assertEqual(distance, 1)
        self.assertFalse(distance <= 0.99)
        self.assertTrue(distance <= 1)

    def test_declared_envelope_and_protected_miter_must_be_compatible(self):
        corner = (self.box[2]+self.h, self.box[3]+self.h)
        in_miter_envelope = in_box(corner, expanded_box(self.box, self.h))
        in_round_envelope = box_distance(corner, self.box) <= self.h
        self.assertTrue(in_miter_envelope)
        self.assertFalse(in_round_envelope)
        # Intersection with a round envelope would discard this requested miter.
        self.assertFalse(in_miter_envelope and in_round_envelope)

    def test_similarity_and_reflection_preserve_the_clearance_counterexample(self):
        base_tip = (0, self.h*math.sqrt(2))
        base_excess = self.h*math.sqrt(2)-(self.d+self.h)
        for scale in (1e-6, 1, 1e6):
            for reflect in (-1, 1):
                for angle in (0, 0.7, 2.2):
                    co, si = math.cos(angle), math.sin(angle)
                    def transform(p):
                        x, y = reflect*p[0], p[1]
                        return (scale*(co*x-si*y+3), scale*(si*x+co*y-2))
                    tip = transform(base_tip)
                    top = transform((0, self.d+self.h))
                    normal = (-si, co)
                    excess = (tip[0]-top[0])*normal[0]+(tip[1]-top[1])*normal[1]
                    self.assertAlmostEqual(excess/scale, base_excess, places=12)
                    self.assertGreater(excess, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
