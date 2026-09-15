// Bounded generated-fixture integration. External SDK declarations only.
#pragma once
#include "RegionSdkOutput.h"
#include "RegionFixture.h"
#include "AIArtStyle.h"
#include "AIArtStyleParser.h"
#include "AIDictionary.h"
#include "AIDocument.h"
#include "AIMask.h"
namespace region_probe {
using namespace strokemaster::topology_probe;
struct Metrics {
    unsigned long long goCalls=0,mutations=0,failedCalls=0;
    ASErr lastGoError=0;
    const char* gate="ready";
    int inputType=0,outputComponents=0,outputHoles=0;
    double maxVertexDeviation=-1;
};
inline constexpr char fixtureKey[]="StrokeMasterRegionFixtureVersion";
struct Native {
    const AIArtSuite& art;const AIPathSuite& path;const AIPathStyleSuite& paint;
    const AIArtStyleSuite& styles;const AIArtStyleParserSuite& parser;
    const AIDictionarySuite& dict;const AILiveEffectSuite& live;
    const AIDocumentSuite& doc;const AIBlendStyleSuite& blend;
    Metrics& metrics;
    ASErr Reject(const char* gate){metrics.gate=gate;return kBadParameterErr;}
    struct Parser {
        const AIArtStyleParserSuite& api;AIStyleParser value=nullptr;
        ~Parser(){if(value)api.DisposeParser(value);}
        ASErr Close(){if(!value)return kNoErr;auto p=value;value=nullptr;return api.DisposeParser(p);}
    };
    ASErr PlainObject(AIArtHandle object) {
        ai::int32 attributes=0;
        auto e=art.GetArtUserAttr(object,kArtIsClipMask,&attributes);if(e)return e;
        if(attributes&kArtIsClipMask)return Reject("source.clip-mask");
        if(blend.GetOpacity(object)!=1||blend.GetBlendingMode(object)!=kAINormalBlendingMode||blend.GetIsolated(object))
            return Reject("source.transparency");
        const auto knockout=blend.GetKnockout(object);
        if(knockout!=kAIKnockoutOff&&knockout!=kAIKnockoutInherit)return Reject("source.knockout");
        return kNoErr;
    }
    ASErr InspectStyle(AIArtHandle object,AILiveEffectHandle effect,bool wrapper) {
        auto e=PlainObject(object);if(e)return e;
        AIArtStyleHandle style=nullptr;e=styles.GetArtStyle(object,&style);if(e)return e;
        if(!style)return Reject("source.style-missing");
        if(!wrapper){
            ai::int16 type=0;e=styles.GetArtStyleType(style,&type);if(e)return e;
            if(type!=kAIArtStyleSimple)return Reject("source.complex-appearance");
        }
        Parser p{parser};e=parser.NewParser(&p.value);if(e)return e;if(!p.value)return kOutOfMemoryErr;
        e=parser.ParseStyle(p.value,style);if(e)return e;
        if(!parser.IsStyleParseable(p.value)||parser.ContainsTransparency(p.value))return Reject("source.style-unparseable-or-transparent");
        if(parser.CountPreEffects(p.value)!=0||parser.CountPostEffects(p.value)!=(wrapper?1:0))
            return Reject("source.extra-effects");
        const auto paints=parser.CountPaintFields(p.value);
        if(paints!=(wrapper?0:2))return Reject("source.paint-count");
        if(wrapper){
            AIParserLiveEffect field=nullptr;AILiveEffectHandle actual=nullptr;
            e=parser.GetNthPostEffect(p.value,0,&field);if(e)return e;if(!field)return kBadParameterErr;
            e=parser.GetLiveEffectHandle(field,&actual);if(e)return e;
            if(actual!=effect)return Reject("source.effect-identity");
        }else{
            int fills=0,strokes=0;
            for(ai::int32 i=0;i<paints;++i){
                AIParserPaintField field=nullptr;e=parser.GetNthPaintField(p.value,i,&field);if(e)return e;
                if(!field||parser.CountEffectsOfPaintField(field)!=0||!parser.IsDefaultPaintBlend(field))
                    return Reject("source.paint-effect-or-blend");
                fills+=parser.IsFill(field)?1:0;strokes+=parser.IsStroke(field)?1:0;
            }
            if(fills!=1||strokes!=1)return Reject("source.fill-stroke-count");
        }
        return p.Close();
    }
    ASErr ReadContour(AIArtHandle object,ai::int16 expected,NativeContour& result) {
        short type=0;auto e=art.GetArtType(object,&type);if(e)return e;
        if(type!=kPathArt)return Reject("source.non-path-contour");
        e=PlainObject(object);if(e)return e;
        ai::int16 count=0;AIBoolean closed=false;
        e=path.GetPathSegmentCount(object,&count);if(e)return e;
        e=path.GetPathClosed(object,&closed);if(e)return e;
        if(count!=expected||!closed)return Reject("source.contour-count-or-closure");
        std::array<AIPathSegment,4> segments{};e=path.GetPathSegments(object,0,count,segments.data());if(e)return e;
        result.closed=true;
        for(int i=0;i<count;++i){auto s=segments[i];result.segments.push_back({{s.p.h,s.p.v},{s.in.h,s.in.v},{s.out.h,s.out.v}});}
        return kNoErr;
    }
    ASErr ReadSource(AILiveEffectGoMessage& message,Source& source) {
        if(!message.srcArt||!message.art||message.srcArt==message.art)return Reject("source.input-alias");
        ai::int32 version=0;
        if(!message.parameters)return Reject("source.parameters-missing");
        auto e=dict.GetIntegerEntry(message.parameters,dict.Key(fixtureKey),&version);if(e)return e;
        if(version!=1)return Reject("source.fixture-version");
        short type=0;e=art.GetArtType(message.srcArt,&type);if(e)return e;
        if(type!=kGroupArt)return Reject("source.not-wrapper");
        e=InspectStyle(message.srcArt,message.effect,true);if(e)return e;
        AIArtHandle compound=nullptr,sibling=nullptr,shell=nullptr,hole=nullptr;
        e=art.GetArtFirstChild(message.srcArt,&compound);if(e)return e;if(!compound)return Reject("source.empty-wrapper");
        e=art.GetArtSibling(compound,&sibling);if(e)return e;if(sibling)return Reject("source.wrapper-children");
        e=art.GetArtType(compound,&type);if(e)return e;if(type!=kCompoundPathArt)return Reject("source.not-compound");
        e=InspectStyle(compound,message.effect,false);if(e)return e;
        e=art.GetArtFirstChild(compound,&shell);if(e)return e;if(!shell)return Reject("source.no-shell");
        e=art.GetArtSibling(shell,&hole);if(e)return e;if(!hole)return Reject("source.no-hole");
        e=art.GetArtSibling(hole,&sibling);if(e)return e;if(sibling)return Reject("source.excess-contours");
        // Ordinal roles are written by Create below and described by fixture v1.
        // No winding inference or selection-based conversion is performed.
        e=ReadContour(shell,4,source.shell);if(e)return e;
        source.holes.emplace_back();e=ReadContour(hole,3,source.holes[0]);if(e)return e;
        AIPathStyle style{};AIBoolean advanced=false;e=paint.GetPathStyle(compound,&style,&advanced);if(e)return e;
        if(advanced||style.clip||!style.evenodd)return Reject("source.fill-rule-or-advanced");
        auto& p=source.paint;p.fill=style.fillPaint;p.stroke=style.strokePaint;
        p.grayOnly=style.fill.color.kind==kGrayColor&&style.stroke.color.kind==kGrayColor;
        p.singleStroke=p.plainAppearance=p.opaque=true;p.miterJoin=style.stroke.join==kAIMiterJoin;
        p.dashed=style.stroke.dash.length!=0;p.overprint=style.fill.overprint||style.stroke.overprint;
        p.fillGray=style.fill.color.c.g.gray;p.strokeGray=style.stroke.color.c.g.gray;
        p.width=style.stroke.width;p.miterLimit=style.stroke.miterLimit;
        source.generatedDiagnostic=source.explicitRoles=true;
        return kNoErr;
    }
    ASErr Process(AILiveEffectGoMessage& message) {
        metrics.gate="source.validate";metrics.outputComponents=metrics.outputHoles=0;metrics.maxVertexDeviation=-1;
        short type=0;auto e=art.GetArtType(message.art,&type);if(e)return e;metrics.inputType=type;
        Source source;e=ReadSource(message,source);if(e)return e;
        CoreKernel kernel;FixtureAdmission admission;
        RegionSdkOutput output(art,path,paint,message.art,message.srcArt);
        const auto result=Run(source,kernel,admission,output);
        metrics.maxVertexDeviation=admission.deviation;
        if(!result.committed){
            if(result.error==Error::rollbackFailed)return Reject("output.rollback-failed");
            if(result.error==Error::kernelRejected)return Reject("kernel.refused");
            if(result.error==Error::outputRejected)return Reject("oracle.refused");
            if(output.error){metrics.gate=output.gate;return output.error;}
            return Reject("source-or-output.refused");
        }
        metrics.outputComponents=metrics.outputHoles=1;metrics.gate="published";return kNoErr;
    }
    ASErr AddRing(AIArtHandle compound,const Ring& ring) {
        AIArtHandle child=nullptr;auto e=art.NewArt(kPathArt,kPlaceInsideOnBottom,compound,&child);if(e)return e;
        if(!child)return kOutOfMemoryErr;
        std::array<AIPathSegment,4> points{};
        for(std::size_t i=0;i<ring.size();++i){points[i].p.h=ring[i].x;points[i].p.v=ring[i].y;points[i].in=points[i].out=points[i].p;points[i].corner=true;}
        const auto count=static_cast<ai::int16>(ring.size());
        e=path.SetPathSegmentCount(child,count);if(e)return e;
        e=path.SetPathSegments(child,0,count,points.data());if(e)return e;
        return path.SetPathClosed(child,true);
    }
    ASErr Create(bool effect) {
        AIDocumentHandle document=nullptr;auto e=doc.GetDocument(&document);if(e)return e;
        if(!document)return Reject("create.no-document");
        AILiveEffectHandle handle=nullptr;e=live.GetLiveEffectHandleByName(effectIdentity,&handle);if(e)return e;
        if(!handle)return Reject("create.effect-missing");
        AIArtHandle root=nullptr;AILiveEffectParameters parameters=nullptr;
        struct Cleanup {
            const AIArtSuite& art;const AIDictionarySuite& dict;AIArtHandle& root;AILiveEffectParameters& parameters;
            ~Cleanup(){if(root)art.DisposeArt(root);if(parameters)dict.Release(parameters);}
        } cleanup{art,dict,root,parameters};
        e=art.NewArt(kGroupArt,kPlaceAboveAll,nullptr,&root);if(e)return e;if(!root)return kOutOfMemoryErr;
        AIArtHandle compound=nullptr;e=art.NewArt(kCompoundPathArt,kPlaceInsideOnBottom,root,&compound);if(e)return e;
        if(!compound)return kOutOfMemoryErr;
        const auto shape=FixtureRegion();e=AddRing(compound,shape.shell);if(e)return e;
        e=AddRing(compound,shape.holes[0]);if(e)return e;
        AIPathStyle style{};style.Init();style.fillPaint=style.strokePaint=true;style.evenodd=true;
        style.fill.color.kind=style.stroke.color.kind=kGrayColor;style.fill.color.c.g.gray=style.stroke.color.c.g.gray=.75;
        style.stroke.width=.6;style.stroke.join=kAIMiterJoin;style.stroke.miterLimit=4;
        e=paint.SetPathStyleForceSimple(compound,&style);if(e)return e;
        if(effect){
            // Construct an explicitly empty wrapper Appearance, then append the
            // post-effect. Never replace the compound's fill or stroke node.
            Parser p{parser};e=parser.NewParser(&p.value);if(e)return e;if(!p.value)return kOutOfMemoryErr;
            AIArtStyleHandle empty=nullptr,merged=nullptr;e=parser.CreateNewStyle(p.value,&empty);if(e)return e;
            e=p.Close();if(e)return e;if(!empty)return Reject("create.empty-style");
            e=live.CreateLiveEffectParameters(&parameters);if(e)return e;if(!parameters)return kOutOfMemoryErr;
            e=dict.SetIntegerEntry(parameters,dict.Key(fixtureKey),1);if(e)return e;
            e=live.NewArtStyleByMergingLiveEffect(empty,handle,parameters,regionMergeAction,&merged);if(e)return e;
            if(!merged)return Reject("create.merge-null");
            e=styles.SetArtStyle(root,merged);if(e)return e;
        }
        root=nullptr;metrics.gate=effect?"created.effect":"created.control";return kNoErr;
    }
};
} // namespace region_probe
