// Bounded filled-region core and optional research CLI. No Adobe host APIs.
#include "topology_region_core.h"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/version.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#ifndef ISE_TOPOLOGY_CORE_ONLY
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#endif

namespace api = ise::topology;
namespace bg = boost::geometry;
namespace {
using Point = bg::model::d2::point_xy<double>;
using Polygon = bg::model::polygon<Point>;
using Region = bg::model::multi_polygon<Polygon>;
using Lines = bg::model::multi_linestring<bg::model::linestring<Point>>;
constexpr double area_tolerance = api::AreaTolerance;
constexpr double length_tolerance = api::LengthTolerance;
struct UnsimplifiedDistance : bg::strategy::buffer::distance_symmetric<double> {
    explicit UnsimplifiedDistance(double h) : distance_symmetric(h) {}
    double simplify_distance() const { return 0; }
};
struct Refusal : std::runtime_error { using std::runtime_error::runtime_error; };
void require(bool condition, std::string const& message) {
    if (!condition) throw Refusal(message);
}
struct Frame {
    double x, y, scale;
    Point local(Point const& p) const {
        return Point((p.x() - x) / scale, (p.y() - y) / scale);
    }
};
template<class Function> void rings(Polygon& p, Function f) {
    f(p.outer());
    for (auto& r : p.inners()) f(r);
}
Region read_parts(api::Region const& list, std::string const& key) {
    require(!list.empty() && list.size() <= api::MaxPolygonsPerList, key + ": require 1..256 polygons");
    Region parts;
    std::size_t vertices = 0;
    for (auto const& item : list) {
        Polygon p;
        auto convert = [&](api::Ring const& input, auto& output) {
            require(input.size() <= api::MaxVerticesPerList - vertices, key + ": vertex budget exceeded");
            vertices += input.size();
            require(input.size() >= 4, key + ": ring needs three vertices and closure");
            for (auto const& v : input)
                require(std::isfinite(v.x) && std::isfinite(v.y) && std::abs(v.x) <= 1e100 && std::abs(v.y) <= 1e100,
                        key + ": coordinates must be finite and at most 1e100");
            require(input.front().x == input.back().x && input.front().y == input.back().y,
                    key + ": explicit ring closure required");
            for (auto const& v : input) output.emplace_back(v.x,v.y);
        };
        convert(item.outer, p.outer());
        for (auto const& hole : item.holes) {
            p.inners().emplace_back();
            convert(hole, p.inners().back());
        }
        parts.push_back(std::move(p));
    }
    return parts;
}
void validate(Region const& r, std::string const& label) {
    std::string reason;
    require(!r.empty() && std::isfinite(bg::area(r)) && bg::area(r) > 0,
            label + ": empty or nonpositive region");
    bool valid = bg::is_valid(r, reason);
    require(valid, label + ": invalid topology: " + reason);
}
void normalize(Region& parts, Frame const& frame, std::string const& label) {
    for (auto& p : parts) {
        rings(p, [&](auto& r) {
            for (auto& v : r) {
                double ulp = std::max(std::abs(std::nextafter(v.x(), INFINITY) - v.x()),
                                      std::abs(std::nextafter(v.y(), INFINITY) - v.y()));
                require(ulp / frame.scale <= 1e-10, label + ": coordinate resolution too coarse");
                v = frame.local(v);
                require(std::isfinite(v.x()) && std::isfinite(v.y()) &&
                        std::abs(v.x()) <= 1e6 && std::abs(v.y()) <= 1e6,
                        label + ": extent exceeds normalized 1e6");
            }
            for (std::size_t i = 1; i < r.size(); ++i)
                require(bg::distance(r[i-1], r[i]) >= 1e-9,
                        label + ": repeated anchor or edge below normalized 1e-9");
            for (std::size_t i = 0; i + 1 < r.size(); ++i) {
                Point a = r[(i + r.size() - 2) % (r.size() - 1)], b = r[i], c = r[i+1];
                double ux = b.x()-a.x(), uy = b.y()-a.y();
                double vx = c.x()-b.x(), vy = c.y()-b.y();
                double sine = std::abs(ux*vy-uy*vx)/(std::hypot(ux,uy)*std::hypot(vx,vy));
                require(ux*vx+uy*vy >= 0 || sine >= 1e-7,
                        label + ": cusp or near reversal below sine 1e-7 unsupported");
            }
        });
        // Roles are explicit WKT shell/holes. Reversing a ring changes no role.
        bg::correct(p);
        std::string reason;
        bool valid = bg::is_valid(p, reason);
        require(valid, label + ": invalid polygon: " + reason);
        require(bg::area(p) >= 1e-12, label + ": polygon area below normalized 1e-12");
    }
}
void overlay_precision(Region const& a, Region const& b, std::string const& label);
Region unite(Region const& parts, std::string const& label) {
    Region result;
    for (auto const& p : parts) {
        if (result.empty()) result.push_back(p);
        else {
            Region next;
            // Guard the original operands: the union may otherwise erase a hole
            // before any downstream containment or precision check can see it.
            Region operand; operand.push_back(p);
            overlay_precision(result, operand, label + " union");
            bg::union_(result, p, next);
            result = std::move(next);
        }
    }
    validate(result, "union");
    return result;
}
Region difference(Region const& a, Region const& b) {
    Region r;
    bg::difference(a, b, r);
    require(std::isfinite(bg::area(r)), "nonfinite difference area");
    return r;
}
double residual(Region const& a, Region const& b) {
    return std::abs(bg::area(difference(a, b)));
}
bool rectangular_cover(Region const& a, Region const& b) {
    if (b.size() != 1 || !b[0].inners().empty()) return false;
    bg::model::box<Point> box, other;
    bg::envelope(b, box);
    bg::envelope(a, other);
    auto const& ring = b[0].outer();
    for (std::size_t i = 1; i < ring.size(); ++i) {
        Point const& p = ring[i-1]; Point const& q = ring[i];
        bool vertical = p.x() == q.x() && (p.x() == box.min_corner().x() || p.x() == box.max_corner().x());
        bool horizontal = p.y() == q.y() && (p.y() == box.min_corner().y() || p.y() == box.max_corner().y());
        if (!vertical && !horizontal) return false;
    }
    return other.min_corner().x() >= box.min_corner().x() && other.max_corner().x() <= box.max_corner().x()
        && other.min_corner().y() >= box.min_corner().y() && other.max_corner().y() <= box.max_corner().y();
}
void overlay_precision(Region const& a, Region const& b, std::string const& label) {
#ifdef BOOST_GEOMETRY_USE_RESCALING
    using Policy = bg::rescale_overlay_policy_type<Region, Region>::type;
    auto policy = bg::get_rescale_policy<Policy>(a, b);
    double grid = 1.0 / policy.m_multiplier;
    std::ostringstream diagnostic;
    diagnostic << std::setprecision(17) << label << ": overlay grid=" << grid
               << " exceeds normalized " << length_tolerance / 2;
    require(std::isfinite(grid) && grid <= length_tolerance / 2, diagnostic.str());
#else
    require(false, label + ": bounded overlay requires rescaling");
#endif
}
void containment(Region const& a, Region const& b, std::string const& label) {
    if (rectangular_cover(a, b)) return;
    Region outside = difference(a, b);
    std::ostringstream diagnostic;
    diagnostic << std::setprecision(17) << label << ": area containment failed; normalized residual="
               << std::abs(bg::area(outside));
    require(std::abs(bg::area(outside)) <= area_tolerance, diagnostic.str());
    // Area alone can miss a narrow, long excursion. Inspect residual vertices too.
    for (auto const& p : outside) for (auto const& v : p.outer())
        require(bg::distance(v, b) <= length_tolerance, label + ": distance containment failed");
    if (!outside.empty()) {
        // Check the residual's filled interior, not only its boundary vertices.
        // An inscribed round buffer is conservative relative to the distance band.
        overlay_precision(outside, b, label);
        Region band;
        bg::buffer(b, band, UnsimplifiedDistance(length_tolerance),
                   bg::strategy::buffer::side_straight(), bg::strategy::buffer::join_round(64),
                   bg::strategy::buffer::end_flat(), bg::strategy::buffer::point_circle(64));
        validate(band, label + " distance band");
        require(difference(outside, band).empty(), label + ": residual interior exceeds distance band");
    }
}
void protected_retention(Region const& witnesses, Region const& target, std::string const& label) {
    for (auto const& witness : witnesses) {
        Region one; one.push_back(witness);
        if (rectangular_cover(one, target)) continue;
        overlay_precision(one, target, label);
        Region retained;
        bg::intersection(one, target, retained);
        double original_area = bg::area(one), retained_area = bg::area(retained);
        // Each witness must retain area independently; a large witness cannot
        // subsidize complete or partial loss of a small one.
        require(retained_area > 0 && retained_area >= original_area * (1 - api::ProtectedLossFraction)
                && residual(one, target) <= original_area * api::ProtectedLossFraction,
                label + ": protected witness must retain at least 0.999999 of its area");
        containment(one, target, label);
    }
}

api::Region export_geometry(Region const& region, Frame const& frame) {
    api::Region out;
    for (auto const& p : region) {
        api::Polygon converted;
        auto copy = [&](auto const& ring) {
            api::Ring values;
            for (auto const& v : ring) values.push_back({v.x()*frame.scale+frame.x,v.y()*frame.scale+frame.y});
            return values;
        };
        converted.outer = copy(p.outer());
        for (auto const& hole : p.inners()) converted.holes.push_back(copy(hole));
        out.push_back(std::move(converted));
    }
    return out;
}
api::Result process_impl(api::Request const& request) {
    double h = request.h, limit = request.limit;
    int n = request.circle_points;
    require(std::isfinite(h) && h > 0 && h <= 1e100, "h must be finite in (0, 1e100]");
    require(std::isfinite(limit) && limit >= 1 && limit <= 100, "limit must be finite in [1,100]");
    require(n >= 16 && n <= 4096 && n % 4 == 0, "circle_points must be a multiple of 4 in [16,4096]");
    require(request.join == api::Join::Miter || request.join == api::Join::Round, "join must be miter or round");
    std::string join = request.join == api::Join::Miter ? "miter" : "round";
    Region parts = read_parts(request.source, "source"), envelope_parts;
    bool explicit_envelope = bool(request.envelope);
    if (explicit_envelope) envelope_parts = read_parts(*request.envelope, "envelope");
    bg::model::box<Point> bounds;
    // A distant, loose envelope must not reduce the source's numeric resolution.
    bg::envelope(parts, bounds);
    double dx = bounds.max_corner().x() - bounds.min_corner().x();
    double dy = bounds.max_corner().y() - bounds.min_corner().y();
    Frame frame{bounds.min_corner().x(), bounds.min_corner().y(), std::max({dx, dy, h})};
    require(h / frame.scale >= 1e-7, "h below normalized 1e-7 precision envelope");
    normalize(parts, frame, "source");
    Region source = unite(parts, "source"), envelope;
    if (explicit_envelope) {
        normalize(envelope_parts, frame, "envelope");
        envelope = unite(envelope_parts, "envelope");
        if (!rectangular_cover(source, envelope)) overlay_precision(source, envelope, "source/envelope");
        containment(source, envelope, "source versus envelope");
    }
    // The stock distance strategy simplifies at h/1000, which can erase a stem.
    UnsimplifiedDistance distance(h / frame.scale);
    bg::strategy::buffer::side_straight side;
    bg::strategy::buffer::end_flat end;
    bg::strategy::buffer::point_circle circle(n);
    Region candidate;
    if (join == "miter") {
        bg::strategy::buffer::join_miter joins(limit);
        bg::buffer(source, candidate, distance, side, joins, end, circle);
    } else {
        bg::strategy::buffer::join_round joins(n);
        bg::buffer(source, candidate, distance, side, joins, end, circle);
    }
    validate(candidate, "offset");
    containment(source, candidate, "source versus offset");
    Region result;
    if (explicit_envelope) {
        if (rectangular_cover(candidate, envelope)) result = candidate;
        else {
            overlay_precision(candidate, envelope, "candidate/envelope");
            bg::intersection(candidate, envelope, result);
        }
    }
    else result = envelope = candidate;
    validate(result, "result");
    containment(source, result, "fill retention");
    containment(result, envelope, "outer envelope");
    containment(result, candidate, "result versus candidate");
    if (request.protected_regions) {
        Region protected_parts = read_parts(*request.protected_regions, "protected");
        normalize(protected_parts, frame, "protected");
        protected_retention(protected_parts, candidate, "protected region versus candidate");
        protected_retention(protected_parts, envelope, "protected region versus envelope");
        protected_retention(protected_parts, result, "protected region retention");
    }
    Region symmetric;
    bool compare_boundary = request.compare_boundary;
    if (compare_boundary) {
        Lines boundaries;
        for (auto p : source) rings(p, [&](auto const& r) {
            bg::model::linestring<Point> line(r.begin(), r.end());
            // Repeat the first edge so its initial corner is an interior join.
            // Flat terminal caps add no new region on that already covered edge.
            line.push_back(r[1]);
            boundaries.push_back(std::move(line));
        });
        Region stroke;
        if (join == "miter") {
            bg::strategy::buffer::join_miter joins(limit);
            bg::buffer(boundaries, stroke, distance, side, joins, end, circle);
        } else {
            bg::strategy::buffer::join_round joins(n);
            bg::buffer(boundaries, stroke, distance, side, joins, end, circle);
        }
        validate(stroke, "symmetric boundary stroke");
        overlay_precision(source, stroke, "symmetric comparison union");
        bg::union_(source, stroke, symmetric);
        validate(symmetric, "fill plus symmetric boundary stroke");
    }
    double expected_error = -1;
    if (request.expected) {
        Region expected = read_parts(*request.expected, "expected");
        normalize(expected, frame, "expected");
        expected = unite(expected, "expected");
        expected_error = residual(result, expected) + residual(expected, result);
    }

    api::Result response;
    auto& d = response.diagnostics;
    d.boost_version = BOOST_VERSION;
#ifndef BOOST_GEOMETRY_NO_ROBUSTNESS
    d.rescaled_overlay = true;
#endif
    d.explicit_envelope = explicit_envelope;
    d.scale = frame.scale;
    d.source_area = bg::area(source) * frame.scale * frame.scale;
    d.area = bg::area(result) * frame.scale * frame.scale;
    d.outside_area_normalized = residual(result, envelope);
    d.lost_fill_area_normalized = residual(source, result);
    d.clipped_area_normalized = residual(candidate, result);
    d.expected_error_normalized = expected_error;
    d.components = result.size();
    for (auto const& p : result) d.holes += p.inners().size();
    response.geometry = export_geometry(result, frame);
    if (compare_boundary) {
        d.symmetric_excess_area_normalized = residual(symmetric, candidate);
        response.symmetric_geometry = export_geometry(symmetric, frame);
    }
    for (auto const& point : request.probes) {
        require(std::isfinite(point.x) && std::isfinite(point.y), "probe coordinates must be finite");
        response.probes.push_back(bg::covered_by(frame.local(Point(point.x,point.y)), result));
    }
    response.status = api::Status::Ok;
    return response;
}
api::Result failure(api::Status status, char const* message) noexcept {
    api::Result result;
    result.status = status;
    std::size_t count = (std::min)(std::strlen(message), result.message.size()-1);
    std::memcpy(result.message.data(), message, count);
    result.message[count] = '\0';
    return result;
}
} // implementation details

api::Result api::process(api::Request const& request) noexcept {
    try { return process_impl(request); }
    catch (Refusal const& e) { return failure(Status::Refused, e.what()); }
    catch (std::bad_alloc const&) { return failure(Status::ResourceFailure, "geometry allocation failed"); }
    catch (std::exception const& e) { return failure(Status::InternalFailure, e.what()); }
    catch (...) { return failure(Status::InternalFailure, "unexpected geometry exception"); }
}

#ifndef ISE_TOPOLOGY_CORE_ONLY
namespace pt = boost::property_tree;
namespace {
api::Region read_cli_parts(pt::ptree const& request, std::string const& key) {
    auto const& list = request.get_child(key);
    require(!list.empty() && list.size() <= api::MaxPolygonsPerList, key + ": require 1..256 polygons");
    api::Region parts;
    for (auto const& item : list) {
        require(item.first.empty(), key + ": require array");
        Polygon p;
        bg::read_wkt(item.second.get_value<std::string>(), p);
        api::Polygon converted;
        auto copy = [](auto const& ring) {
            api::Ring values;
            for (auto const& v : ring) values.push_back({v.x(),v.y()});
            return values;
        };
        converted.outer = copy(p.outer());
        for (auto const& hole : p.inners()) converted.holes.push_back(copy(hole));
        parts.push_back(std::move(converted));
    }
    return parts;
}
void emit_geometry(std::ostream& out, api::Region const& region) {
    out << '[';
    bool first_polygon = true;
    for (auto const& p : region) {
        if (!first_polygon) out << ',';
        first_polygon = false;
        out << '[';
        bool first_ring = true;
        auto emit = [&](api::Ring const& r) {
            if (!first_ring) out << ',';
            first_ring = false;
            out << '[';
            for (std::size_t i = 0; i < r.size(); ++i) {
                if (i) out << ',';
                out << '[' << r[i].x << ',' << r[i].y << ']';
            }
            out << ']';
        };
        emit(p.outer);
        for (auto const& hole : p.holes) emit(hole);
        out << ']';
    }
    out << ']';
}
std::string execute(pt::ptree const& input) {
    api::Request request;
    request.h = input.get<double>("h");
    request.limit = input.get<double>("limit",4.0);
    request.circle_points = input.get<int>("circle_points",64);
    auto join = input.get<std::string>("join","miter");
    require(join == "miter" || join == "round", "join must be miter or round");
    request.join = join == "miter" ? api::Join::Miter : api::Join::Round;
    require(input.get<std::string>("fill_rule","explicit_roles") == "explicit_roles",
            "unresolved winding/even-odd paths unsupported; supply explicit shells and holes");
    request.source = read_cli_parts(input,"source");
    if (input.get_child_optional("envelope")) request.envelope = read_cli_parts(input,"envelope");
    if (input.get_child_optional("protected")) request.protected_regions = read_cli_parts(input,"protected");
    if (input.get_child_optional("expected")) request.expected = read_cli_parts(input,"expected");
    request.compare_boundary = input.get<bool>("compare_boundary",false);
    if (input.get_child_optional("probes")) for (auto const& item : input.get_child("probes")) {
        require(item.second.size() == 2, "probe must have two coordinates");
        auto it = item.second.begin();
        double x = it++->second.get_value<double>(), y = it->second.get_value<double>();
        request.probes.push_back({x,y});
    }
    auto response = api::process(request);
    if (response.status != api::Status::Ok) throw std::runtime_error(response.message.data());
    auto const& d = response.diagnostics;
    std::ostringstream out;
    out << std::setprecision(17) << "{\"ok\":true,\"boost_version\":" << d.boost_version
        << ",\"overlay_mode\":\"" << (d.rescaled_overlay ? "default_rescale" : "floating_no_rescale") << "\""
        << ",\"envelope_source\":\"" << (d.explicit_envelope ? "explicit" : "filled_region_buffer") << "\""
        << ",\"scale\":" << d.scale << ",\"source_area\":" << d.source_area
        << ",\"area\":" << d.area
        << ",\"outside_area_normalized\":" << d.outside_area_normalized
        << ",\"lost_fill_area_normalized\":" << d.lost_fill_area_normalized
        << ",\"clipped_area_normalized\":" << d.clipped_area_normalized
        << ",\"expected_error_normalized\":" << d.expected_error_normalized
        << ",\"components\":" << d.components << ",\"holes\":" << d.holes << ",\"geometry\":";
    emit_geometry(out, response.geometry);
    if (request.compare_boundary) {
        out << ",\"symmetric_excess_area_normalized\":" << d.symmetric_excess_area_normalized
            << ",\"symmetric_geometry\":";
        emit_geometry(out, response.symmetric_geometry);
    }
    out << ",\"probes\":[";
    for (std::size_t i = 0; i < response.probes.size(); ++i) {
        if (i) out << ',';
        out << (response.probes[i] ? "true" : "false");
    }
    out << "]}";
    return out.str();
}
} // CLI helpers
int main() {
    std::string line;
    while (std::getline(std::cin,line)) {
        try {
            require(line.size() <= 4000000, "request byte budget exceeded");
            pt::ptree request;
            std::istringstream input(line);
            pt::read_json(input,request);
            std::cout << execute(request) << '\n';
        } catch (std::exception const& e) {
            pt::ptree error;
            error.put("error",e.what());
            std::ostringstream out;
            pt::write_json(out,error,false);
            std::cout << out.str();
        }
    }
}
#endif // ISE_TOPOLOGY_CORE_ONLY
