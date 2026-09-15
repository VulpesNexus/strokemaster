// Bounded diagnostic geometry and admission, independent of Adobe handles.
#pragma once
#include "TopologyAdapter.h"
#include <algorithm>
#include <limits>
namespace strokemaster::topology_probe {
inline Region FixtureRegion() {
    return {{{0,0},{10,0},{10,10},{0,10}},{{{.2,5},{2.2,4},{2.2,6}}}};
}
inline Region FixtureExpected() {
    const double h=.3,q=h*std::sqrt(5.0);
    return {{{-h,-h},{10+h,-h},{10+h,10+h},{-h,10+h}},
        {{{.2+q,5},{2.2-h,4+(h+q)/2},{2.2-h,6-(h+q)/2}}}};
}
inline Ring ExactCorners(Ring ring) {
    bool changed=true;
    while(changed&&ring.size()>3) {
        changed=false;
        for(std::size_t i=0;i<ring.size();++i) {
            auto a=ring[(i+ring.size()-1)%ring.size()],b=ring[i],c=ring[(i+1)%ring.size()];
            if((b.x-a.x)*(c.y-b.y)==(b.y-a.y)*(c.x-b.x)&&
                (b.x-a.x)*(c.x-b.x)+(b.y-a.y)*(c.y-b.y)>0) {
                ring.erase(ring.begin()+i);changed=true;break;
            }
        }
    }
    return ring;
}
inline double RingDeviation(Ring actual,Ring expected) {
    actual=ExactCorners(std::move(actual));expected=ExactCorners(std::move(expected));
    if(actual.size()!=expected.size())return std::numeric_limits<double>::infinity();
    double best=std::numeric_limits<double>::infinity();
    for(std::size_t start=0;start<expected.size();++start)for(int direction:{-1,1}) {
        double worst=0;
        for(std::size_t i=0;i<actual.size();++i) {
            auto j=(int(start)+direction*int(i)+int(expected.size()))%int(expected.size());
            worst=std::max(worst,std::hypot(actual[i].x-expected[j].x,actual[i].y-expected[j].y));
        }
        best=std::min(best,worst);
    }
    return best;
}
struct FixtureAdmission final:Admission {
    double deviation=0;
    static constexpr double tolerance=10*core::LengthTolerance;
    bool Check(const Request& request,const Region& output) override {
        const auto source=FixtureRegion();
        if(request.halfWidth!=.3||request.miterLimit!=4||request.source.holes.size()!=1||
            RingDeviation(request.source.shell,source.shell)>1e-10||
            RingDeviation(request.source.holes[0],source.holes[0])>1e-10||output.holes.size()!=1)return false;
        const auto expected=FixtureExpected();
        deviation=std::max(RingDeviation(output.shell,expected.shell),RingDeviation(output.holes[0],expected.holes[0]));
        return deviation<=tolerance;
    }
};
} // namespace strokemaster::topology_probe
