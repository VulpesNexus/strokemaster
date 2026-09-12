// Standalone native-caller control: this translation unit includes no Boost,
// JSON, WKT, or Adobe headers. Allocation injection exists only in this test.
#include "../../tools/topology_region_core.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>

static bool fail_allocation = false;
void* operator new(std::size_t size) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace t = ise::topology;
static int checks = 0;
void check(bool value, char const* name) {
    ++checks;
    if (!value) { std::cerr << "FAILED: " << name << '\n'; std::exit(1); }
}
t::Ring rectangle(double x0,double y0,double x1,double y1) {
    return {{x0,y0},{x1,y0},{x1,y1},{x0,y1},{x0,y0}};
}
t::Request simple() {
    t::Request request;
    request.source.push_back({rectangle(0,0,4,3),{}});
    request.h = 1;
    return request;
}
void refused(t::Result const& r, char const* label) {
    check(r.status == t::Status::Refused,label);
    check(r.geometry.empty() && r.symmetric_geometry.empty() && r.probes.empty(),"refusal has no partial output");
    check(r.message[0] != '\0' && r.message.back() == '\0',"bounded refusal message");
}
int main() {
    static_assert(noexcept(t::process(std::declval<t::Request const&>())),"noexcept API");
    auto request = simple();
    auto result = t::process(request);
    check(result.status == t::Status::Ok,"rectangle success");
    check(std::abs(result.diagnostics.area-30) < 1e-10,"analytic area");
    check(result.geometry.size() == 1 && result.geometry[0].holes.empty(),"array topology");
    check(result.message[0] == '\0',"no failure message on success");
    check(request.source[0].outer.size() == 5 && request.source[0].outer[0].x == 0,"const input preserved");
    double saved = result.geometry[0].outer[0].x;
    request.source[0].outer[0].x = 900;
    check(result.geometry[0].outer[0].x == saved,"result owns its coordinates");

    auto invalid = simple();
    invalid.h = -1;
    auto bad = t::process(invalid);
    refused(bad,"negative h refused");
    auto repeated = t::process(invalid);
    check(std::strcmp(bad.message.data(),repeated.message.data()) == 0,"deterministic refusal");
    check(t::process(simple()).status == t::Status::Ok,"success after refusal");

    invalid = simple();
    invalid.compare_boundary = true;
    invalid.probes.push_back({std::numeric_limits<double>::infinity(),0});
    refused(t::process(invalid),"late invalid probe clears already-computed geometry");
    invalid = simple();
    invalid.source[0].outer.pop_back();
    refused(t::process(invalid),"closure required");
    invalid = simple();
    invalid.source[0].outer.assign(t::MaxVerticesPerList+1,t::Point{});
    refused(t::process(invalid),"vertex budget");
    invalid = simple();
    invalid.source.assign(t::MaxPolygonsPerList+1,invalid.source[0]);
    refused(t::process(invalid),"polygon budget");

    auto oom_request = simple();
    fail_allocation = true;
    auto oom = t::process(oom_request);
    fail_allocation = false;
    check(oom.status == t::Status::ResourceFailure,"allocation failure translated");
    check(oom.geometry.empty() && oom.symmetric_geometry.empty() && oom.probes.empty(),"allocation failure has no partial output");
    check(t::process(oom_request).status == t::Status::Ok,"success after allocation failure");

    t::Request p1;
    p1.source.push_back({rectangle(0,0,1,1),{}});
    p1.h = .1;
    auto hole = rectangle(.49995,.4995,.50005,.5005);
    p1.envelope = t::Region{{rectangle(-1,-1,2,2),{hole}}};
    p1.protected_regions = t::Region{{hole,{}}};
    refused(t::process(p1),"Volta P1 through array API");
    p1.protected_regions.reset();
    p1.envelope = t::Region{{rectangle(-100000,-100000,100000,100000),{rectangle(1.0001,.2,1.2,.8)}}};
    refused(t::process(p1),"Volta P2 through array API");

    auto decomposed = [](double extent) {
        return t::Region{{rectangle(-extent,-extent,1.01,extent),{}},
                         {rectangle(1.025,-extent,extent,extent),{}},
                         {rectangle(1.01,-extent,1.025,.51),{}},
                         {rectangle(1.01,.525,1.025,extent),{}}};
    };
    p1.envelope = decomposed(100000);
    refused(t::process(p1),"coarse input union refused through array API");
    p1.envelope = decomposed(2);
    p1.probes = {{1.0175,.5175}};
    auto tight = t::process(p1);
    check(tight.status == t::Status::Ok,"tight input union accepted");
    check(tight.diagnostics.holes == 1,"tight union preserves hole");
    check(std::abs(tight.diagnostics.area-1.439775) <= 2e-7,"tight union analytic area");
    check(tight.probes.size()==1 && !tight.probes[0],"tight union excludes hole center");

    t::Request glyph;
    double x = 7.5*std::tan(std::acos(-1.0)/12);
    glyph.source.push_back({rectangle(-7.5,-12,7.5,.3),{{{0,0},{x,-7.5},{-x,-7.5},{0,0}}}});
    glyph.h = 1.5;
    glyph.compare_boundary = true;
    glyph.probes.push_back({0,3});
    auto controlled = t::process(glyph);
    check(controlled.status == t::Status::Ok,"triangle hole through array API");
    double top = -100, symmetric_top = -100;
    for (auto const& p : controlled.geometry) for (auto const& v : p.outer) top = (std::max)(top,v.y);
    for (auto const& p : controlled.symmetric_geometry) for (auto const& v : p.outer) symmetric_top = (std::max)(symmetric_top,v.y);
    check(std::abs(top-1.8) < 1e-8,"filled top");
    check(std::abs(symmetric_top-1.5/std::sin(std::acos(-1.0)/12)) < 1e-6,"symmetric excess");
    check(controlled.probes.size()==1 && !controlled.probes[0],"protected diagnostic point outside filled result");
    std::cout << "{\"ok\":true,\"checks\":" << checks << "}\n";
}
