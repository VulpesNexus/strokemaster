// PROPOSED host-independent scaffold. No SDK calls or plugin registration.
// Full-region output is allowed only for generated equal-gray fill/stroke art.
#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>
#include "../../tools/topology_region_core.h"

namespace strokemaster::topology_probe {
inline constexpr char effectIdentity[] = "org.strokemaster.proposed.closed-region.v1";
struct Point { double x = 0, y = 0; };
inline bool Same(Point a, Point b) { return a.x == b.x && a.y == b.y; }
struct Segment { Point anchor, in, out; };
struct NativeContour { bool closed = false; std::vector<Segment> segments; };
// Roles are structural. Ring vertices exclude a repeated closing endpoint.
using Ring = std::vector<Point>;
struct Region { Ring shell; std::vector<Ring> holes; };
struct Paint {
    bool fill = false, stroke = false, grayOnly = false;
    bool singleStroke = false, plainAppearance = false, opaque = false, miterJoin = false;
    bool brush = false, dashed = false, overprint = false;
    double fillGray = 0, strokeGray = 0, width = 0, miterLimit = 0;
};
struct Source {
    bool generatedDiagnostic = false, explicitRoles = false;
    NativeContour shell;
    std::vector<NativeContour> holes;
    Paint paint;
};
struct Request { Region source; double halfWidth = 0, miterLimit = 0, gray = 0; };
struct Output { Region region; double gray = 0; }; // Filled region; never a stroke band.
enum class Error {
    none, unsupportedSource, unsupportedPaint, invalidContour,
    kernelRejected, outputRejected, stageFailed, commitFailed, rollbackFailed
};
struct Result { Error error = Error::none; Error originalError = Error::none; bool committed = false; };

inline bool FinitePoint(Point p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) <= 10000 && std::abs(p.y) <= 10000;
}
inline bool BoundedRing(const Ring& ring) {
    if (ring.size() < 3 || ring.size() > 128) return false;
    double twiceArea = 0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        Point a = ring[i], b = ring[(i + 1) % ring.size()];
        if (!FinitePoint(a) || Same(a, b)) return false;
        // This is an extent/degeneracy gate, not a polygon-topology validator.
        twiceArea += (a.x - ring[0].x) * (b.y - ring[0].y) - (a.y - ring[0].y) * (b.x - ring[0].x);
    }
    return std::isfinite(twiceArea) && std::abs(twiceArea) > 1e-12;
}
inline bool BoundedRegion(const Region& r) {
    if (!BoundedRing(r.shell) || r.holes.size() > 1) return false;
    for (const auto& hole : r.holes) if (!BoundedRing(hole)) return false;
    return true;
}
inline bool ConvertContour(const NativeContour& native, Ring& out) {
    if (!native.closed || native.segments.size() < 3 || native.segments.size() > 128) return false;
    Ring temporary;
    temporary.reserve(native.segments.size());
    for (const auto& s : native.segments) {
        if (!Same(s.anchor, s.in) || !Same(s.anchor, s.out)) return false;
        temporary.push_back(s.anchor);
    }
    if (!BoundedRing(temporary)) return false;
    out = std::move(temporary);
    return true;
}
inline Error Convert(const Source& source, Request& out) {
    if (!source.generatedDiagnostic || !source.explicitRoles || source.holes.size() > 1)
        return Error::unsupportedSource;
    const auto& p = source.paint;
    if (!p.fill || !p.stroke || !p.grayOnly || !p.singleStroke || !p.plainAppearance || !p.opaque || !p.miterJoin ||
        p.brush || p.dashed || p.overprint || !std::isfinite(p.fillGray) ||
        p.fillGray < 0 || p.fillGray > 1 || p.strokeGray != p.fillGray ||
        !std::isfinite(p.width) || p.width <= 0 || p.width > 1000 ||
        !std::isfinite(p.miterLimit) || p.miterLimit < 1 || p.miterLimit > 100)
        return Error::unsupportedPaint;
    Request temporary;
    if (!ConvertContour(source.shell, temporary.source.shell)) return Error::invalidContour;
    for (const auto& hole : source.holes) {
        Ring converted;
        if (!ConvertContour(hole, converted)) return Error::invalidContour;
        temporary.source.holes.push_back(std::move(converted));
    }
    temporary.gray = p.fillGray; temporary.halfWidth = p.width / 2; temporary.miterLimit = p.miterLimit;
    out = std::move(temporary);
    return Error::none;
}

// The core owns topology validation. Admission still requires an independent
// oracle; compiling this provisional binding does not approve the kernel.
struct Kernel {
    virtual ~Kernel() = default;
    virtual bool Evaluate(const Request&, Region&) = 0;
};
namespace core = ise::topology;
using CoreProcess = core::Result (*)(const core::Request&) noexcept;
inline core::Ring CloseForCore(const Ring& ring) {
    if(!BoundedRing(ring)) return {};
    core::Ring closed;
    closed.reserve(ring.size()+1);
    for(auto p:ring) closed.push_back({p.x,p.y});
    closed.push_back(closed.front()); // Called only after BoundedRing.
    return closed;
}
inline bool OpenFromCore(const core::Ring& closed, Ring& out) {
    if(closed.size()<4||closed.size()>129) return false;
    if(closed.front().x!=closed.back().x||closed.front().y!=closed.back().y) return false;
    Ring temporary;
    temporary.reserve(closed.size()-1);
    for(std::size_t i=0;i+1<closed.size();++i) temporary.push_back({closed[i].x,closed[i].y});
    if(!BoundedRing(temporary)) return false;
    out=std::move(temporary);return true;
}
struct CoreKernel final : Kernel {
    CoreProcess call;
    core::Status lastStatus = core::Status::Refused;
    bool contractViolation = false;
    std::array<char,512> message{};
    explicit CoreKernel(CoreProcess process = &core::process) : call(process) {}
    bool Evaluate(const Request& input, Region& out) override {
        lastStatus=core::Status::Refused;contractViolation=false;message.fill(0);
        if(!call||!BoundedRegion(input.source)) return false;
        core::Request request;
        core::Polygon polygon;
        polygon.outer=CloseForCore(input.source.shell);
        for(const auto& hole:input.source.holes) polygon.holes.push_back(CloseForCore(hole));
        request.source.push_back(std::move(polygon));
        request.h=input.halfWidth;request.limit=input.miterLimit;request.join=core::Join::Miter;
        auto result=call(request);
        lastStatus=result.status;message=result.message;message.back()='\0';
        if(result.status!=core::Status::Ok) {
            contractViolation=!result.geometry.empty()||!result.symmetric_geometry.empty()||!result.probes.empty()||
                (result.status!=core::Status::Refused&&result.status!=core::Status::ResourceFailure&&result.status!=core::Status::InternalFailure);
            return false;
        }
        if(result.geometry.size()!=1||result.geometry[0].holes.size()>1) return false;
        Region converted;
        if(!OpenFromCore(result.geometry[0].outer,converted.shell)) return false;
        for(const auto& hole:result.geometry[0].holes){
            Ring ring;if(!OpenFromCore(hole,ring)) return false;converted.holes.push_back(std::move(ring));
        }
        out=std::move(converted);return true;
    }
};
// A separate oracle is mandatory before admission, including the triangular-hole
// thin-edge benchmark. Numeric bounds alone must never authorize SDK mutation.
struct Admission {
    virtual ~Admission() = default;
    virtual bool Check(const Request&, const Region&) = 0;
};
// The future SDK implementation may touch only newly created diagnostic art.
// Stage must record every new handle before later operations can throw. Commit
// transfers ownership only on true. Rollback must cover partial Stage/Commit.
// Constructing this facade creates no art. No existing source handle is supplied.
struct OwnedOutput {
    virtual ~OwnedOutput() = default;
    virtual bool Stage(const Output&) = 0;
    virtual bool Commit() = 0;
    virtual bool Rollback() noexcept = 0;
};
inline Result Run(const Source& source, Kernel& kernel, Admission& admission, OwnedOutput& sink) {
    Request request;
    Error error = Convert(source, request);
    if (error != Error::none) return {error, error, false};
    Output output;
    output.gray = request.gray;
    try {
        if (!kernel.Evaluate(request, output.region)) return {Error::kernelRejected, Error::kernelRejected, false};
        if (!BoundedRegion(output.region) || !admission.Check(request, output.region))
            return {Error::outputRejected, Error::outputRejected, false};
    } catch (...) { return {Error::kernelRejected, Error::kernelRejected, false}; }
    // No sink operation occurs until computation, conversion, and admission pass.
    error = Error::stageFailed;
    try {
        if (sink.Stage(output)) {
            error = Error::commitFailed;
            if (sink.Commit()) return {Error::none, Error::none, true};
        }
    } catch (...) { /* Roll back all owned output, including partial construction. */ }
    if (!sink.Rollback()) return {Error::rollbackFailed, error, false};
    return {error, error, false};
}
} // namespace strokemaster::topology_probe
