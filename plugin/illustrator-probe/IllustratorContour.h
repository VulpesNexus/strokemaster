// Convert Illustrator's anchor/in/out path representation to the shared
// host-independent contour representation. No Adobe types enter this layer.
#pragma once

#include "../source/IseMiterCore.h"
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace illustrator_contour {

using ise_miter::Contour;
using ise_miter::Pt;
using ise_miter::Segment;

struct Node {
    Pt anchor, in, out;
};

inline bool Finite(Pt p) { return std::isfinite(p.x) && std::isfinite(p.y); }
inline bool Same(Pt a, Pt b) { return a.x == b.x && a.y == b.y; }

// A closed Illustrator path has a final segment from its last anchor to the
// first. Keep it explicit so its incoming and outgoing handles are retained.
// Reject malformed or unbounded data before any geometric processing.
inline bool Convert(const std::vector<Node>& nodes, bool closed, Contour* result) {
    if (!result || nodes.size() < (closed ? 3u : 2u) || nodes.size() > 32767u) return false;
    for (const Node& n : nodes) {
        if (!Finite(n.anchor) || !Finite(n.in) || !Finite(n.out)) return false;
        for (Pt p : {n.anchor, n.in, n.out})
            if (std::fabs(p.x) > 1000000 || std::fabs(p.y) > 1000000) return false;
    }
    Contour converted;
    converted.start = nodes.front().anchor;
    converted.closed = closed;
    const std::size_t edges = nodes.size() - 1 + (closed ? 1 : 0);
    converted.segments.reserve(edges);
    for (std::size_t i = 0; i < edges; ++i) {
        const Node& from = nodes[i];
        const Node& to = nodes[(i + 1) % nodes.size()];
        Segment s;
        s.p = to.anchor;
        s.c1 = from.out;
        s.c2 = to.in;
        s.curve = (!Same(s.c1, from.anchor) || !Same(s.c2, to.anchor)) &&
                  !ise_miter::StraightCubic(from.anchor, s.c1, s.c2, to.anchor);
        converted.segments.push_back(s);
    }
    *result = std::move(converted);
    return true;
}

} // namespace illustrator_contour
