#include "../../plugin/illustrator-probe/IllustratorContour.h"
#include "../../plugin/illustrator-probe/NativeMiter.h"
#include "../../plugin/illustrator-probe/NativeRegion.h"
#include "../../plugin/source/IseRegionCore.h"
#include <algorithm>
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

    // native_miter::ChooseLimit: the owner's rulings as one native limit.
    {
        // The census thorn: a 10-degree tip between straight 20 pt sides on a 30 pt square
        // base. Outside 4 (h = 4): the tip (ratio 11.47) is a whole point, so the limit
        // rises just past it; the base's corners keep their miters.
        const double half = 5 * 3.14159265358979323846 / 180, side = 20, bw = side * std::sin(half),
                     by = side * std::cos(half);
        std::vector<Node> thorn = {
            {{0, 0}, {0, 0}, {0, 0}}, {{bw, by}, {bw, by}, {bw, by}}, {{15, by}, {15, by}, {15, by}},
            {{15, by + 30}, {15, by + 30}, {15, by + 30}}, {{-15, by + 30}, {-15, by + 30}, {-15, by + 30}},
            {{-15, by}, {-15, by}, {-15, by}}, {{-bw, by}, {-bw, by}, {-bw, by}}};
        Check(illustrator_contour::Convert(thorn, true, &contour), "thorn converts");
        ise_miter::Options o; o.halfWidth = 4;
        const auto stubs = ise_miter::ExteriorCorners({contour}, o);
        const auto choice = native_miter::ChooseLimit(stubs, 4, 4);
        Check(choice.limit > 11.47 && choice.limit < 11.5, "thorn: limit just past the whole point");
        Check(choice.keptBeveled == 0 && choice.cutMitered == 0, "thorn: every ruling met");

        // Synthetic corners: a glyph tip within the limit (ratio 3, a curve beside it) is cut,
        // a serif corner (ratio 1.41, straight 1 pt edges) keeps its miter: the limit lies
        // between them.
        ise_miter::Stub tip; tip.ratio = 3; tip.inCurve = true; tip.edge = 10;
        ise_miter::Stub serif; serif.ratio = 1.41; serif.edge = 1;
        auto c2 = native_miter::ChooseLimit({tip, serif}, 4, 4);
        Check(c2.limit > 1.41 && c2.limit < 3 && c2.keptBeveled == 0 && c2.cutMitered == 0, "tip cut, serif kept");

        // Already satisfied: the limit stays.
        ise_miter::Stub square90; square90.ratio = 1.414; square90.edge = 20;
        auto c3 = native_miter::ChooseLimit({square90}, 4, 4);
        Check(c3.limit == 4 && c3.kept == 1 && c3.cut == 0, "a satisfied limit is kept");

        // A conflict: a straight-edged corner of ratio 3 (kept) and a glyph tip of ratio 2.5
        // (cut) cannot both be met; one corner is wrong, and the limit nearest 4 wins.
        ise_miter::Stub straight3; straight3.ratio = 3; straight3.edge = 20;
        ise_miter::Stub tip25; tip25.ratio = 2.5; tip25.outCurve = true; tip25.edge = 10;
        auto c4 = native_miter::ChooseLimit({straight3, tip25}, 4, 4);
        Check(c4.keptBeveled + c4.cutMitered == 1 && c4.limit >= 3, "conflict: one corner wrong, nearest limit");

        // A corner past the limit that is not a whole point (a curve beside it) stays beveled.
        ise_miter::Stub needle; needle.ratio = 30; needle.inCurve = true; needle.edge = 50;
        auto c5 = native_miter::ChooseLimit({needle, serif}, 4, 4);
        Check(c5.limit < 30 && c5.cutMitered == 0, "a curved needle past the limit stays beveled");

        // A v whose crotch miter runs through its thin bottom: the crotch (50, 25) turns 136
        // degrees (ratio 2.69) into a bottom 25 deep. At h 20 its miter (53.8) reaches past
        // the far side's band (25 + 20) and is cut; at h 5 (13.5 < 30) it stays inside and,
        // hidden under a fill above, constrains nothing.
        std::vector<Node> vee = {
            {{0, 100}, {0, 100}, {0, 100}}, {{40, 0}, {40, 0}, {40, 0}}, {{60, 0}, {60, 0}, {60, 0}},
            {{100, 100}, {100, 100}, {100, 100}}, {{80, 100}, {80, 100}, {80, 100}},
            {{50, 25}, {50, 25}, {50, 25}}, {{20, 100}, {20, 100}, {20, 100}}};
        Check(illustrator_contour::Convert(vee, true, &contour), "v converts");
        auto c6 = native_miter::ChooseLimitForOutline({contour}, 20, 4, true, true);
        Check(c6.cut >= 1 && c6.cutMitered == 0 && c6.keptBeveled == 0 && c6.limit < 2.69,
              "v: the escaping crotch miter is cut, nothing kept is beveled");
        auto c7 = native_miter::ChooseLimitForOutline({contour}, 5, 4, true, true);
        Check(c7.limit == 4 && c7.cutMitered == 0, "v: a crotch miter inside the fill leaves the limit");
        auto c8 = native_miter::ChooseLimitForOutline({contour}, 20, 4, false);
        Check(c8.limit == 4, "v, Outside: interior joins are not drawn, the limit stays");

        // A whole point and an escaping crotch sharper than the current limit (independent review,
        // 2026-10-15): the thorn's tip (ratio 11.47) would raise the limit past a narrow v's crotch
        // (ratio 8.45, 15 deep; at h 4 its miter, 33.8, runs out the bottom), whose spike the cut
        // was meant to remove. Both cannot hold, so one ruling is reported unmet, not silently lost.
        std::vector<Node> narrow = {
            {{200, 100}, {200, 100}, {200, 100}}, {{245, 0}, {245, 0}, {245, 0}}, {{255, 0}, {255, 0}, {255, 0}},
            {{300, 100}, {300, 100}, {300, 100}}, {{260, 100}, {260, 100}, {260, 100}},
            {{250, 15}, {250, 15}, {250, 15}}, {{240, 100}, {240, 100}, {240, 100}}};
        ise_miter::Contour narrowV, thornC;
        Check(illustrator_contour::Convert(narrow, true, &narrowV) && illustrator_contour::Convert(thorn, true, &thornC),
              "narrow v and thorn convert");
        auto c9 = native_miter::ChooseLimitForOutline({thornC, narrowV}, 4, 4, true);
        Check(c9.keptBeveled + c9.cutMitered == 1, "whole point vs escaping crotch: the conflict is counted");
        auto c10 = native_miter::ChooseLimitForOutline({narrowV}, 4, 4, true, false, true);
        Check(c10.cutMitered == 0 && c10.limit == 4, "Inside (clipped): the crotch does not escape, nothing changes");

        // No usable half width: the current limit, clamped to Illustrator's range.
        Check(native_miter::ChooseLimit({tip}, 0, 4).limit == 4, "no half width: limit unchanged");
        Check(native_miter::ChooseLimit({}, 4, 900).limit == native_miter::kMaxLimit, "limit clamped to 500");
    }

    // native_region::InternalEdges: which paths need Pathfinder Add under an Inside stroke.
    {
        auto box = [](double x0, double y0, double x1, double y1, bool reversed) {
            std::vector<Node> n = {{{x0, y0}, {x0, y0}, {x0, y0}}, {{x1, y0}, {x1, y0}, {x1, y0}},
                                   {{x1, y1}, {x1, y1}, {x1, y1}}, {{x0, y1}, {x0, y1}, {x0, y1}}};
            if (reversed) std::reverse(n.begin(), n.end());
            Contour c;
            illustrator_contour::Convert(n, true, &c);
            return c;
        };
        // The census squares: 80 pt, overlapping by 40 each way; 80 pt of each inside the other.
        const auto a = box(0, 0, 80, 80, false), b = box(40, 40, 120, 120, false);
        Check(native_region::InternalEdges({a, b}, .01, 2), "overlapping squares have inner edges");
        Check(native_region::InternalEdges({a, b}, .01, 159) && !native_region::InternalEdges({a, b}, .01, 161), "inner edges: 160 pt");
        // A counter wound the other way is a hole: boundary, no inner edge (a letter's outline).
        Check(!native_region::InternalEdges({box(0, 0, 100, 100, false), box(30, 30, 70, 70, true)}, .01, 2),
              "a counter is boundary");
        // Apart, or touching only at a corner: no inner edges.
        Check(!native_region::InternalEdges({a, box(200, 0, 280, 80, false)}, .01, 2), "disjoint squares");
        Check(!native_region::InternalEdges({a, box(80, 80, 160, 160, false)}, .01, 2), "corner-touching squares");
        // A square inside another, wound the same way: its edges are all inside the fill.
        Check(native_region::InternalEdges({box(0, 0, 100, 100, false), box(30, 30, 70, 70, false)}, .01, 2),
              "a same-wound inner square is inside the fill");
        Check(!native_region::InternalEdges({a}, .01, 2), "one contour has none");
    }
    std::printf("illustrator contour: %d checks passed\n", checks);
}
