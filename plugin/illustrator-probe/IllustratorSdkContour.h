// Thin SDK boundary for the shared contour geometry. The conversion itself is
// exercised without Adobe headers in IllustratorContourTests.cpp.
#pragma once
#include "IllustratorContour.h"
#include "AIPath.h"

namespace illustrator_contour {

inline bool ConvertSdk(const AIPathSegment* segments, std::size_t count, bool closed,
                       Contour* result) {
    if (!segments || count < (closed ? 3u : 2u) || count > 32767u) return false;
    std::vector<Node> nodes;
    nodes.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const AIPathSegment& s = segments[i];
        nodes.push_back({{s.p.h, s.p.v}, {s.in.h, s.in.v}, {s.out.h, s.out.v}});
    }
    return Convert(nodes, closed, result);
}

} // namespace illustrator_contour
