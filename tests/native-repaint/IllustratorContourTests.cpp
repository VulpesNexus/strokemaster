#include "../../plugin/illustrator-probe/IllustratorContour.h"
#include "../../plugin/source/IseRegionCore.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using illustrator_contour::Node;
using ise_miter::Contour;

static int checks = 0;
static void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::printf("FAIL: %s\n", message); std::exit(1); }
}

int main() {
    Contour contour;
    const std::vector<Node> square = {
        {{0,0},{0,0},{0,0}}, {{10,0},{10,0},{10,0}},
        {{10,10},{10,10},{10,10}}, {{0,10},{0,10},{0,10}}};
    Check(illustrator_contour::Convert(square, true, &contour), "closed square converts");
    Check(contour.closed && contour.segments.size() == 4, "closing edge is explicit");
    Check(!contour.segments.back().curve && contour.segments.back().p.x == 0 &&
          contour.segments.back().p.y == 0, "closing line returns to first anchor");
    auto region = ise_region::NonzeroRegion({contour}, .01);
    Check(region.size() == 1, "shared region core accepts converted square");
    Check(ise_miter::ExteriorCorners({contour}, ise_miter::Options{}).size() == 4,
          "shared miter core finds four square corners");

    auto curved = square;
    curved[3].out = {-2,8};
    curved[0].in = {-2,2};
    Check(illustrator_contour::Convert(curved, true, &contour), "curved closing edge converts");
    Check(contour.segments.size() == 4 && contour.segments.back().curve &&
          contour.segments.back().c1.x == -2 && contour.segments.back().c2.y == 2,
          "closing cubic keeps both handles");
    Check(!ise_region::NonzeroRegion({contour}, .01).empty(),
          "shared region core accepts converted cubic");

    Check(illustrator_contour::Convert(square, false, &contour) && !contour.closed &&
          contour.segments.size() == 3, "open path has no closing edge");
    const auto saved = contour;
    auto invalid = square;
    invalid[1].out.x = std::numeric_limits<double>::quiet_NaN();
    Check(!illustrator_contour::Convert(invalid, true, &contour), "nonfinite handle refused");
    Check(contour.segments.size() == saved.segments.size() && !contour.closed,
          "refusal does not change output");
    invalid = square;
    invalid[1].anchor.x = 1000001;
    Check(!illustrator_contour::Convert(invalid, true, &contour), "out of range anchor refused");
    Check(!illustrator_contour::Convert({square[0], square[1]}, true, &contour),
          "closed path needs three anchors");
    Check(!illustrator_contour::Convert(square, true, nullptr), "null output refused");
    std::printf("illustrator contour: %d checks passed\n", checks);
}
