// SDK-free conversion/transaction tests. Reference kernel is a test double;
// these tests do not admit Euler's kernel or any native output implementation.
#include "TopologyAdapter.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace strokemaster::topology_probe;
namespace {
int checks = 0;
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
bool Near(double a, double b) { return std::abs(a - b) <= 1e-10; }
NativeContour Contour(std::initializer_list<Point> points) {
    NativeContour c; c.closed = true;
    for (auto p : points) c.segments.push_back({p,p,p});
    return c;
}
Source Fixture() {
    Source s; s.generatedDiagnostic = s.explicitRoles = true;
    s.shell = Contour({{0,0},{10,0},{10,10},{0,10}});
    // Left wall is only 0.2 pt thick. Hole has a genuine acute right tip.
    s.holes.push_back(Contour({{0.2,4},{2.2,5},{0.2,6}}));
    auto& p = s.paint;
    p.fill = p.stroke = p.grayOnly = p.singleStroke = p.plainAppearance = p.opaque = true;
    p.miterJoin = true;
    p.fillGray = p.strokeGray = 0.75; p.width = 0.2; p.miterLimit = 4;
    return s;
}
Region Analytic() {
    // Independent shifted-line intersections: h=0.1, triangle side normals
    // have length sqrt(5). Left hole side x=.2 moves right by h; the other
    // sides move inward normally. Outer square's miter corners move by h.
    const double h=0.1, q=h*std::sqrt(5.0);
    Region r;
    r.shell={{-h,-h},{10+h,-h},{10+h,10+h},{-h,10+h}};
    r.holes.push_back({{0.2+h,4+(h+q)/2},{2.2-q,5},{0.2+h,6-(h+q)/2}});
    return r;
}
Source SpikeFixture() {
    auto s=Fixture();
    s.holes[0]=Contour({{.2,5},{2.2,4},{2.2,6}});
    s.paint.width=.6;
    return s;
}
Region SpikeAnalytic() {
    const double h=.3,q=h*std::sqrt(5.0);
    Region r;
    r.shell={{-h,-h},{10+h,-h},{10+h,10+h},{-h,10+h}};
    r.holes.push_back({{.2+q,5},{2.2-h,4+(h+q)/2},{2.2-h,6-(h+q)/2}});
    return r;
}
double Area(const Ring& r) {
    double a=0;
    for(std::size_t i=0;i<r.size();++i){auto p=r[i],q=r[(i+1)%r.size()];a+=p.x*q.y-p.y*q.x;}
    return std::abs(a)/2;
}
bool SameRing(const Ring& a,const Ring& b) {
    if(a.size()!=b.size()) return false;
    for(std::size_t start=0;start<b.size();++start) for(int direction : {-1,1}) {
        bool equal=true;
        for(std::size_t i=0;i<a.size();++i) {
            auto j=(int(start)+direction*int(i)+int(b.size()))%int(b.size());
            equal=equal&&Near(a[i].x,b[j].x)&&Near(a[i].y,b[j].y);
        }
        if(equal) return true;
    }
    return false;
}
struct Oracle : Admission {
    int calls=0;
    bool spike=false;
    bool Check(const Request& request,const Region& actual) override {
        ++calls;
        Request known; if(Convert(spike?SpikeFixture():Fixture(),known)!=Error::none) return false;
        if(!Near(request.halfWidth,spike?.3:.1)||request.source.holes.size()!=1||
            !SameRing(request.source.shell,known.source.shell)||
            !SameRing(request.source.holes[0],known.source.holes[0])) return false;
        auto expected=spike?SpikeAnalytic():Analytic();
        return actual.holes.size()==1&&SameRing(actual.shell,expected.shell)&&SameRing(actual.holes[0],expected.holes[0]);
    }
};
#ifdef PROBE_REAL_CORE_TEST
Ring WithoutExactCollinear(Ring ring) {
    bool changed=true;
    while(changed&&ring.size()>3) {
        changed=false;
        for(std::size_t i=0;i<ring.size();++i) {
            auto a=ring[(i+ring.size()-1)%ring.size()],b=ring[i],c=ring[(i+1)%ring.size()];
            const double cross=(b.x-a.x)*(c.y-b.y)-(b.y-a.y)*(c.x-b.x);
            const double forward=(b.x-a.x)*(c.x-b.x)+(b.y-a.y)*(c.y-b.y);
            if(cross==0&&forward>0) {ring.erase(ring.begin()+i);changed=true;break;}
        }
    }
    return ring;
}
double VertexDelta(Ring a,Ring b) {
    a=WithoutExactCollinear(std::move(a));b=WithoutExactCollinear(std::move(b));
    if(a.size()!=b.size()) return std::numeric_limits<double>::infinity();
    double best=std::numeric_limits<double>::infinity();
    for(std::size_t start=0;start<b.size();++start) for(int direction : {-1,1}) {
        double delta=0;
        for(std::size_t i=0;i<a.size();++i) {
            auto j=(int(start)+direction*int(i)+int(b.size()))%int(b.size());
            delta=std::max(delta,std::hypot(a[i].x-b[j].x,a[i].y-b[j].y));
        }
        best=std::min(best,delta);
    }
    return best;
}
struct NumericalOracle : Admission {
    bool spike=false;
    double maxDelta=0;
    // The fixture's S=10 pt. Core LengthTolerance is normalized by S.
    static constexpr double budget=10*core::LengthTolerance;
    bool Check(const Request& request,const Region& actual) override {
        Request known;if(Convert(spike?SpikeFixture():Fixture(),known)!=Error::none) return false;
        if(request.halfWidth!=known.halfWidth||request.source.holes.size()!=1||
            !SameRing(request.source.shell,known.source.shell)||!SameRing(request.source.holes[0],known.source.holes[0])||
            actual.holes.size()!=1) return false;
        const auto expected=spike?SpikeAnalytic():Analytic();
        maxDelta=std::max(VertexDelta(actual.shell,expected.shell),VertexDelta(actual.holes[0],expected.holes[0]));
        return maxDelta<=budget;
    }
};
#endif
struct ReferenceKernel : Kernel {
    int calls=0, fault=0;
    bool Evaluate(const Request&,Region& result) override {
        ++calls;
        if(fault==1) return false;
        if(fault==2) throw std::runtime_error("injected kernel exception");
        result=Analytic();
        if(fault==3) result.shell[0].x=std::numeric_limits<double>::infinity();
        if(fault==4) result.holes.clear(); // Lost hole must not be admitted.
        if(fault==5) result.shell[0].x+=1e-6; // Local loss: area-only checks could conceal it.
        if(fault==6) result.holes[0][0].x-=.31; // Hole crosses expanded thin outer edge.
        return true;
    }
};
int coreFault=0;
core::Result MockCore(const core::Request& request) noexcept {
    Check(request.source.size()==1&&request.source[0].outer.size()==5&&request.source[0].holes.size()==1&&
        request.source[0].holes[0].size()==4,"real core header receives explicit shell/hole closures");
    Check(request.source[0].outer.front().x==request.source[0].outer.back().x&&
        request.source[0].outer.front().y==request.source[0].outer.back().y&&
        request.h==.1&&request.limit==4&&request.join==core::Join::Miter,
        "core request maps half-width and miter parameters without unit or orientation guesses");
    core::Result result;
    if(coreFault>=1&&coreFault<=3){
        result.status=coreFault==1?core::Status::Refused:coreFault==2?core::Status::ResourceFailure:core::Status::InternalFailure;
        return result;
    }
    result.status=core::Status::Ok;
    auto expected=Analytic();core::Polygon p;
    p.outer=CloseForCore(expected.shell);p.holes.push_back(CloseForCore(expected.holes[0]));
    result.geometry.push_back(p);
    if(coreFault==4) result.status=core::Status::Refused; // Violates non-Ok empty-output contract.
    if(coreFault==5) result.geometry[0].outer.pop_back();
    if(coreFault==6) result.geometry.push_back(p);
    if(coreFault==7) result.geometry[0].holes.push_back(p.holes[0]);
    if(coreFault==8) result.geometry[0].outer[1].x=std::numeric_limits<double>::quiet_NaN();
    if(coreFault==9) result.message.fill('x'); // Always bound diagnostics locally.
    return result;
}
struct Sink : OwnedOutput {
    int fault=0, stages=0, commits=0, rollbacks=0, owned=0, published=0;
    bool Stage(const Output& output) override {
        ++stages; ++owned; // Root is owned before any fallible child operation.
        if(fault==1) return false;
        if(fault==2) throw std::runtime_error("root exists, child failed");
        Check(output.region.holes.size()==1&&output.gray==.75,"paint and explicit hole reach sink");
        ++owned;
        return true;
    }
    bool Commit() override {
        ++commits;
        if(fault==3) return false;
        if(fault==4) throw std::runtime_error("publish failed while output remains owned");
        published=owned;owned=0;return true;
    }
    bool Rollback() noexcept override {++rollbacks;if(fault==5)return false;owned=0;return true;}
};
}
int main() {
    {
        auto a=Analytic();const double q=.1*std::sqrt(5.0),leg=2-.1-q;
        Check(Near(Area(a.shell),104.04),"independent analytic outer miter area");
        Check(Near(Area(a.holes[0]),leg*leg/2),"independent analytic triangular-hole area");
        Check(Near(a.holes[0][0].x-a.shell[0].x,.4),"thin wall retains and expands to 0.4 pt");
        Check(a.holes[0][1].x<2.2,"hole acute tip moves inward, not beyond surrounding envelope");
    }
    {
        auto source=Fixture();auto before=source.shell.segments;
        ReferenceKernel k;Oracle oracle;Sink sink;
        auto result=Run(source,k,oracle,sink);
        Check(result.committed&&k.calls==1&&oracle.calls==1&&sink.published==2,"validated reference contour transaction commits");
        Check(sink.rollbacks==0&&source.shell.segments.size()==before.size()&&Same(source.shell.segments[0].anchor,before[0].anchor),"source unchanged; no success rollback");
        Request request;Convert(source,request);auto reversed=Analytic();
        std::reverse(reversed.shell.begin(),reversed.shell.end());
        std::reverse(reversed.holes[0].begin(),reversed.holes[0].end());
        Check(oracle.Check(request,reversed),"explicit roles survive ring direction changes");
    }
    for(int fault=0;fault<13;++fault){
        auto source=Fixture();ReferenceKernel k;Oracle oracle;Sink sink;
        if(fault==0)source.generatedDiagnostic=false;
        if(fault==1)source.explicitRoles=false;
        if(fault==2)source.paint.strokeGray=.2;
        if(fault==3)source.paint.opaque=false;
        if(fault==4)source.paint.singleStroke=false;
        if(fault==5)source.paint.brush=true;
        if(fault==6)source.paint.dashed=true;
        if(fault==7)source.paint.plainAppearance=false;
        if(fault==8)source.shell.closed=false;
        if(fault==9)source.holes[0].segments[0].out.x+=.01;
        if(fault==10)source.paint.width=std::numeric_limits<double>::quiet_NaN();
        if(fault==11)source.paint.overprint=true;
        if(fault==12)source.paint.miterJoin=false;
        auto result=Run(source,k,oracle,sink);
        Check(!result.committed&&k.calls==0&&sink.stages==0&&sink.rollbacks==0,"unsupported source rejected before kernel and owned art");
    }
    for(int fault=1;fault<=6;++fault){
        ReferenceKernel k;k.fault=fault;Oracle oracle;Sink sink;
        auto result=Run(Fixture(),k,oracle,sink);
        Check(!result.committed&&sink.stages==0&&sink.commits==0&&sink.rollbacks==0,"invalid/unverified output never touches sink");
    }
    for(int fault=1;fault<=4;++fault){
        ReferenceKernel k;Oracle oracle;Sink sink;sink.fault=fault;
        auto result=Run(Fixture(),k,oracle,sink);
        Check(!result.committed&&sink.owned==0&&sink.published==0&&sink.rollbacks==1,"partial stage/commit failure rolls back all owned art");
    }
    {
        struct BrokenCleanup : Sink {
            bool Stage(const Output&) override {++stages;++owned;return false;}
            bool Rollback() noexcept override {++rollbacks;return false;}
        } sink;
        ReferenceKernel k;Oracle oracle;auto result=Run(Fixture(),k,oracle,sink);
        Check(result.error==Error::rollbackFailed&&result.originalError==Error::stageFailed&&!result.committed,"cleanup failure remains explicit, never certified success");
    }
    for(coreFault=0;coreFault<=9;++coreFault){
        CoreKernel kernel(&MockCore);Oracle oracle;Sink sink;
        auto result=Run(Fixture(),kernel,oracle,sink);
        if(coreFault==0||coreFault==9){
            Check(result.committed&&sink.published==2&&kernel.lastStatus==core::Status::Ok,
                "actual API types round-trip through adapter before analytic admission");
        }else{
            Check(!result.committed&&sink.stages==0&&sink.rollbacks==0,"non-Ok or malformed core result never reaches output sink");
        }
        Check(kernel.contractViolation==(coreFault==4),"non-Ok geometry contract violation is identified");
        Check(kernel.message.back()=='\0',"bounded native diagnostic message is always terminated");
    }
    Check(CloseForCore({}).empty(),"empty ring conversion cannot access front");
#ifdef PROBE_REAL_CORE_TEST
    for(bool spike : {false,true}) {
        const auto fixture=spike?SpikeFixture():Fixture();
        CoreKernel kernel; NumericalOracle oracle; oracle.spike=spike; Sink sink;
        auto result=Run(fixture,kernel,oracle,sink);
        if(!result.committed) {
            std::cerr<<"core result="<<int(result.error)<<" status="<<int(kernel.lastStatus)<<" spike="<<spike<<" "<<kernel.message.data()<<'\n';
            Request debugRequest;Convert(fixture,debugRequest);Region debugRegion;
            if(kernel.Evaluate(debugRequest,debugRegion)) {
                std::cerr.precision(17);
                for(auto p:debugRegion.shell) std::cerr<<"shell "<<p.x<<","<<p.y<<'\n';
                for(const auto& ring:debugRegion.holes) for(auto p:ring) std::cerr<<"hole "<<p.x<<","<<p.y<<'\n';
            }
        }
        Check(result.committed&&sink.published==2,"frozen real core passes independent full-region vertex oracle before mock publication");
        std::cout.precision(17);
        std::cout<<"ORACLE: spike="<<spike<<" maxVertexDistance="<<oracle.maxDelta<<" budget="<<NumericalOracle::budget<<'\n';
        Request converted; Check(Convert(fixture,converted)==Error::none,"fixture converts for separate extent control");
        core::Request request; core::Polygon polygon;
        polygon.outer=CloseForCore(converted.source.shell);
        polygon.holes.push_back(CloseForCore(converted.source.holes[0]));
        request.source.push_back(polygon);request.h=converted.halfWidth;request.limit=4;request.compare_boundary=true;
        const auto compared=core::process(request);
        Check(compared.status==core::Status::Ok&&!compared.symmetric_geometry.empty(),"real core provides conventional boundary comparison");
        auto minimumX=[](const core::Region& region) {
            double x=std::numeric_limits<double>::infinity();
            for(const auto& p:region) for(auto v:p.outer) x=std::min(x,v.x);
            return x;
        };
        const double fullMin=minimumX(compared.geometry),boundaryMin=minimumX(compared.symmetric_geometry);
        // Shifted lines at the left acute tip: x=.2-h*sqrt(5).
        // Miter ratio sqrt(5) is below limit 4, so this is not limit fallback.
        const double expectedBoundary=spike?.2-.3*std::sqrt(5.0):-.1;
        Check(std::abs(fullMin+request.h)<=NumericalOracle::budget&&std::abs(boundaryMin-expectedBoundary)<=NumericalOracle::budget,
            "independent minimum extent matches both geometry paths within declared core distance budget");
        Check(spike?fullMin-boundaryMin>.17:Near(fullMin,boundaryMin),"bad triangle forces escape while old triangle is benign");
        if(spike) {
            const double inradius=2/(1+std::sqrt(5.0));
            Check(request.h<inradius&&compared.geometry[0].holes.size()==1,"triangular hole survives the inward offset");
            std::cout.precision(17);
            std::cout<<"SPIKE: boundaryMinX="<<boundaryMin<<" fullRegionMinX="<<fullMin<<" escape="<<fullMin-boundaryMin<<'\n';
        }
        auto altered=spike?SpikeAnalytic():Analytic();altered.holes.clear();
        Check(!oracle.Check(converted,altered),"numerical admission still rejects a missing hole");
        altered=spike?SpikeAnalytic():Analytic();altered.shell[0].x+=.001;
        Check(!oracle.Check(converted,altered),"numerical admission rejects a measured local defect above budget");
    }
#endif
    std::cout<<"PASS: "<<checks<<" conversion/transaction and oracle assertions.\n"
        <<"NOT ADMITTED: candidate testing does not clear independent algorithm review or SDK/native ownership gates.\n";
}
