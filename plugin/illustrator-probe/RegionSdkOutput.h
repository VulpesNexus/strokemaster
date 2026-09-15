// PROPOSED SDK output backend. No registration, core binding, or host execution.
#pragma once
#include "AIArt.h"
#include "AIPath.h"
#include "AIPathStyle.h"
#include "AILiveEffect.h"
#include "TopologyAdapter.h"

namespace strokemaster::topology_probe {
inline constexpr char regionPluginName[] = "StrokeMasterRegionProbe";
inline constexpr AIStyleFilterFlags regionFilterType = kPostEffectFilter;
inline constexpr AILiveEffectMergeAction regionMergeAction = kAppendLiveEffectToStyle;

// Caller acquires these suites per invocation; this object borrows them only.
// It accepts a processing group with exactly one child. Source handles are used
// solely as alias guards. No source geometry or source Appearance is modified.
class RegionSdkOutput final : public OwnedOutput {
    const AIArtSuite& art;
    const AIPathSuite& path;
    const AIPathStyleSuite& paint;
    AIArtHandle processing, source, sourceChild = nullptr, original = nullptr, staged = nullptr;
    std::array<AIArtHandle,2> looseChildren{};
    bool ready = false, published = false;
    bool Check(AIErr value, const char* at) noexcept {
        if (!value) return true;
        error = value; gate = at; return false;
    }
    bool Reject(const char* at) noexcept { error=kBadParameterErr;gate=at;return false; }
    bool Append(const Ring& ring, std::size_t index) {
        AIArtHandle child=nullptr;
        const auto created=art.NewArt(kPathArt,kPlaceInsideOnBottom,staged,&child);
        if(child==source||child==sourceChild||child==processing||child==original||child==staged)
            return Reject("new-path.alias");
        looseChildren[index]=child; // Own returned handle before any next SDK call.
        if(!Check(created,"new-path")||!child) return Reject("new-path.failed");
        AIArtHandle parent=nullptr;
        if(!Check(art.GetArtParent(child,&parent),"path.parent")) return false;
        if(parent!=staged) return Reject("path.parent-mismatch");
        looseChildren[index]=nullptr; // Compound now owns its confirmed child.
        std::array<AIPathSegment,128> segments{};
        for(std::size_t i=0;i<ring.size();++i) {
            segments[i].p.h=ring[i].x;segments[i].p.v=ring[i].y;
            segments[i].in=segments[i].out=segments[i].p;segments[i].corner=true;
        }
        const auto count=static_cast<ai::int16>(ring.size());
        if(!Check(path.SetPathSegmentCount(child,count),"path.count")||
           !Check(path.SetPathSegments(child,0,count,segments.data()),"path.segments")||
           !Check(path.SetPathClosed(child,true),"path.close")) return false;
        ai::int16 actualCount=0;AIBoolean closed=false;
        std::array<AIPathSegment,128> actual{};
        if(!Check(path.GetPathSegmentCount(child,&actualCount),"verify.count")||actualCount!=count)
            return Reject("verify.count-mismatch");
        if(!Check(path.GetPathClosed(child,&closed),"verify.closed")||!closed)
            return Reject("verify.not-closed");
        if(!Check(path.GetPathSegments(child,0,count,actual.data()),"verify.segments")) return false;
        for(std::size_t i=0;i<ring.size();++i) {
            const auto& s=actual[i];
            if(s.p.h!=segments[i].p.h||s.p.v!=segments[i].p.v||
               s.in.h!=s.p.h||s.in.v!=s.p.v||s.out.h!=s.p.h||s.out.v!=s.p.v)
                return Reject("verify.geometry-mismatch");
        }
        return true;
    }
public:
    AIErr error=kNoErr;
    const char* gate="not-started";
    // DisposeArt has no documented partial-failure atomicity. If commit fails,
    // report that processing-input preservation is unverified, never successful.
    bool commitAttempted=false;
    RegionSdkOutput(const AIArtSuite& a,const AIPathSuite& p,const AIPathStyleSuite& s,
                    AIArtHandle input,AIArtHandle editableSource)
        : art(a),path(p),paint(s),processing(input),source(editableSource) {}
    bool Stage(const Output& output) override {
        if(staged||ready||published||!source||!processing||processing==source) return Reject("processing.alias-or-reuse");
        if(!BoundedRegion(output.region)||output.region.holes.size()!=1||
           !std::isfinite(output.gray)||output.gray<0||output.gray>1) return Reject("output.bounds");
        short type=kUnknownArt;
        if(!Check(art.GetArtType(source,&type),"source.type")||type!=kGroupArt)
            return Reject("source.not-group");
        if(!Check(art.GetArtFirstChild(source,&sourceChild),"source.child")||!sourceChild||sourceChild==processing)
            return Reject("source.child-alias");
        if(!Check(art.GetArtType(processing,&type),"processing.type")||type!=kGroupArt)
            return Reject("processing.not-group");
        if(!Check(art.GetArtFirstChild(processing,&original),"processing.child")||!original||original==source||original==sourceChild)
            return Reject("processing.child-alias");
        AIArtHandle sibling=nullptr;
        if(!Check(art.GetArtSibling(original,&sibling),"processing.sibling")||sibling)
            return Reject("processing.multiple-children");
        AIArtHandle created=nullptr;
        const auto status=art.NewArt(kCompoundPathArt,kPlaceInsideOnTop,processing,&created);
        if(created==processing||created==source||created==sourceChild||created==original) return Reject("new-compound.alias");
        staged=created;
        if(!Check(status,"new-compound")||!staged) return Reject("new-compound.failed");
        AIArtHandle parent=nullptr;
        if(!Check(art.GetArtParent(staged,&parent),"compound.parent")||parent!=processing)
            return Reject("compound.parent-mismatch");
        if(!Append(output.region.shell,0)||!Append(output.region.holes[0],1)) return false;
        AIPathStyle style{};style.Init();
        style.fillPaint=true;style.fill.color.kind=kGrayColor;
        style.fill.color.c.g.gray=output.gray;style.evenodd=true;
        if(!Check(paint.SetPathStyleForceSimple(staged,&style),"compound.paint")) return false;
        AIPathStyle actual{};AIBoolean advanced=false;
        if(!Check(paint.GetPathStyle(staged,&actual,&advanced),"verify.paint")) return false;
        if(advanced||!actual.fillPaint||actual.strokePaint||actual.clip||!actual.evenodd||actual.fill.overprint||
           actual.fill.color.kind!=kGrayColor||actual.fill.color.c.g.gray!=style.fill.color.c.g.gray)
            return Reject("verify.paint-mismatch");
        ready=true;gate="staged";return true;
    }
    bool Commit() override {
        if(!ready||published) return Reject("commit.not-ready");
        commitAttempted=true;
        if(!Check(art.DisposeArt(original),"commit.dispose-input-child")) return false;
        // Final fallible operation succeeded. No SDK calls after publication.
        original=nullptr;staged=nullptr;ready=false;published=true;gate="published";return true;
    }
    bool Rollback() noexcept override {
        bool ok=true;
        // A child whose parent could not be verified may still belong to staged.
        // Dispose it first, then the compound; never dispose any input/source.
        for(auto& child:looseChildren) if(child) {
            if(art.DisposeArt(child)) ok=false;else child=nullptr;
        }
        // Do not dispose the parent after a failed child disposal: retain handles
        // for a reported cleanup failure rather than risk a stale child retry.
        if(!ok) return false;
        if(staged) {if(art.DisposeArt(staged)) ok=false;else staged=nullptr;}
        ready=false;return ok;
    }
};
} // namespace strokemaster::topology_probe
