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
// Illustrator hands a post-effect on a styled group one processing child per
// paint field plus the group contents, which sit at the style's group-contents
// position (host census, region-prep-5: None fill, contents, None stroke). The
// contents child is replaced; every other child must be demonstrably unpainted
// and is removed with it. Source handles are used solely as alias guards. No
// source geometry or source Appearance is modified.
class RegionSdkOutput final : public OwnedOutput {
    const AIArtSuite& art;
    const AIPathSuite& path;
    const AIPathStyleSuite& paint;
    AIArtHandle processing, source, sourceChild = nullptr, original = nullptr, staged = nullptr;
    std::array<AIArtHandle,2> looseChildren{};
    // Unpainted paint-field renderings beside the contents; removed at commit.
    std::array<AIArtHandle,3> inert{};
    ai::int32 contentsIndex;
    bool ready = false, published = false;
    bool Check(AIErr value, const char* at) noexcept {
        if (!value) return true;
        error = value; gate = at; return false;
    }
    bool Reject(const char* at) noexcept { error=kBadParameterErr;gate=at;return false; }
    // True only if every leaf under node is a path or compound with neither
    // fill nor stroke paint. Anything else, including an empty group, is not
    // a demonstrated None rendering.
    bool Unpainted(AIArtHandle node,int depth) {
        if(depth>4) return Reject("processing.inert-too-deep");
        short type=kUnknownArt;
        if(!Check(art.GetArtType(node,&type),"inert.type")) return false;
        if(type==kPathArt||type==kCompoundPathArt) {
            AIPathStyle style{};AIBoolean advanced=false;
            if(!Check(paint.GetPathStyle(node,&style,&advanced),"inert.paint")) return false;
            if(advanced||style.fillPaint||style.strokePaint) return Reject("processing.inert-painted");
            if(type==kPathArt) return true;
        } else if(type!=kGroupArt) return Reject("processing.inert-type");
        AIArtHandle child=nullptr;
        if(!Check(art.GetArtFirstChild(node,&child),"inert.child")) return false;
        if(!child) return type==kCompoundPathArt?true:Reject("processing.inert-empty");
        for(int count=0;child;++count) {
            if(count>=64) return Reject("processing.inert-too-large");
            if(!Unpainted(child,depth+1)) return false;
            if(!Check(art.GetArtSibling(child,&child),"inert.sibling")) return false;
        }
        return true;
    }
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
        // Readback is compared within readbackTolerance, not bit for bit:
        // host evidence (region-prep-6) showed Illustrator returns different
        // values than were written. Both deltas are reported either way.
        for(std::size_t i=0;i<ring.size();++i) {
            const auto& s=actual[i];
            const double anchor=std::fmax(std::fabs(double(s.p.h)-double(segments[i].p.h)),
                                          std::fabs(double(s.p.v)-double(segments[i].p.v)));
            const double handle=std::fmax(std::fmax(std::fabs(double(s.in.h)-double(s.p.h)),std::fabs(double(s.in.v)-double(s.p.v))),
                                          std::fmax(std::fabs(double(s.out.h)-double(s.p.h)),std::fabs(double(s.out.v)-double(s.p.v))));
            if(!(anchor<=maxAnchorDelta)) maxAnchorDelta=anchor;
            if(!(handle<=maxHandleDelta)) maxHandleDelta=handle;
        }
        if(!(maxAnchorDelta<=readbackTolerance)||!(maxHandleDelta<=readbackTolerance))
            return Reject("verify.geometry-mismatch");
        return true;
    }
public:
    AIErr error=kNoErr;
    const char* gate="not-started";
    // Largest readback deviations over every written vertex, in points.
    double maxAnchorDelta=0,maxHandleDelta=0;
    static constexpr double readbackTolerance=1e-4;
    // DisposeArt has no documented partial-failure atomicity. If commit fails,
    // report that processing-input preservation is unverified, never successful.
    bool commitAttempted=false;
    RegionSdkOutput(const AIArtSuite& a,const AIPathSuite& p,const AIPathStyleSuite& s,
                    AIArtHandle input,AIArtHandle editableSource,ai::int32 groupContentsPosition=0)
        : art(a),path(p),paint(s),processing(input),source(editableSource),contentsIndex(groupContentsPosition) {}
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
        if(contentsIndex<0||contentsIndex>static_cast<ai::int32>(inert.size())) return Reject("processing.contents-index");
        AIArtHandle child=nullptr;
        if(!Check(art.GetArtFirstChild(processing,&child),"processing.child")) return false;
        std::size_t inertCount=0;
        for(ai::int32 index=0;child;++index) {
            if(child==source||child==sourceChild||child==processing) return Reject("processing.child-alias");
            if(index==contentsIndex) original=child;
            else if(inertCount==inert.size()) return Reject("processing.too-many-children");
            else inert[inertCount++]=child;
            if(!Check(art.GetArtSibling(child,&child),"processing.sibling")) return false;
        }
        if(!original) return Reject("processing.contents-missing");
        for(std::size_t i=0;i<inertCount;++i) if(!Unpainted(inert[i],0)) return false;
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
        // Unpainted renderings go first: a failure here has changed nothing
        // visible, and rollback still removes the staged compound.
        for(auto& child:inert) if(child) {
            if(!Check(art.DisposeArt(child),"commit.dispose-inert-child")) return false;
            child=nullptr;
        }
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
