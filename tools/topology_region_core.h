#ifndef ISE_TOPOLOGY_REGION_CORE_H
#define ISE_TOPOLOGY_REGION_CORE_H

// C++17 value API for bounded, straight-sided filled regions. No Adobe handles,
// Boost types, JSON, WKT, I/O, or mutable global state in this interface.
// Compile topology_region.cpp with ISE_TOPOLOGY_CORE_ONLY for native callers.
// Link in the same C++ toolchain/CRT as the caller; this is not a stable DLL ABI.
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace ise { namespace topology {

inline constexpr std::size_t MaxPolygonsPerList = 256;
inline constexpr std::size_t MaxVerticesPerList = 20000;
inline constexpr double AreaTolerance = 2e-7;
inline constexpr double LengthTolerance = 2e-6;
inline constexpr double ProtectedLossFraction = 1e-6;

struct Point { double x = 0; double y = 0; };
using Ring = std::vector<Point>;
struct Polygon {
    Ring outer;
    std::vector<Ring> holes;
};
using Region = std::vector<Polygon>;
enum class Join { Miter, Round };
enum class Status { Ok, Refused, ResourceFailure, InternalFailure };

struct Request {
    Region source;
    double h = 0;                 // Positive outward distance, in coordinate units.
    double limit = 4;             // Dimensionless ratio to h.
    Join join = Join::Miter;
    int circle_points = 64;       // Round-join diagnostic refinement.
    std::optional<Region> envelope;
    std::optional<Region> protected_regions;
    std::optional<Region> expected; // Optional analytic comparison, not an input fixup.
    bool compare_boundary = false;
    std::vector<Point> probes;
};

struct Diagnostics {
    int boost_version = 0;
    bool rescaled_overlay = false;
    bool explicit_envelope = false;
    double scale = 0;
    double source_area = 0;
    double area = 0;
    double outside_area_normalized = 0;
    double lost_fill_area_normalized = 0;
    double clipped_area_normalized = 0;
    double expected_error_normalized = -1;
    double symmetric_excess_area_normalized = 0;
    std::size_t components = 0;
    std::size_t holes = 0;
};

struct Result {
    Status status = Status::Refused;
    std::array<char,512> message{}; // NUL-terminated; at most 511 diagnostic bytes.
    Region geometry;              // Caller-owned values in the original coordinates.
    Region symmetric_geometry;    // Populated only for the requested comparison.
    std::vector<bool> probes;
    Diagnostics diagnostics;
};

// Rings must explicitly repeat their first vertex. Roles come from outer/holes,
// not orientation. Limits include closure and hole vertices. No curve decoding,
// fill-rule inference, or art lifetime management is performed. Source is const.
// Every non-Ok result has empty geometry, comparisons, and probes; C++ exceptions
// are translated to status/message. The caller handles allocation of Request,
// validates its host eligibility/style/transform, and publishes only an Ok result.
Result process(Request const& request) noexcept;

}} // namespace ise::topology
#endif
