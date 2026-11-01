// region-prep-14 target tests. No Illustrator or real SDK implementation.
// The real core, adapter conversion, admission oracle, and SDK output backend
// carry their own suites; these tests cover registration, globals lifetime,
// bridge payloads, and bounded Go refusals against mocked suites.
#include "RegionProbe.cpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
SPBasicSuite basic{};AILiveEffectSuite live{};AIArtSuite art{};AIPathSuite path{};
AIPathStyleSuite paint{};AIArtStyleSuite styles{};AIArtStyleParserSuite parser{};
AIDictionarySuite dict{};AIDocumentSuite doc{};AIBlendStyleSuite blend{};AIMaskSuite maskSuite{};
int checks=0,acquired=0,released=0,registered=0,allocated=0,freed=0,failAt=0,calls=0;
int copyFailure=0,outlineFailure=0,styleFailure=0,disposedCopies=0,outlinedCopies=0;
void Check(bool x,const char* s){++checks;if(!x){std::cerr<<"FAIL: "<<s<<'\n';std::exit(1);}}
ASErr Step(){return ++calls==failAt?-1234:kNoErr;}
void Setup(){
    acquired=released=registered=allocated=freed=failAt=calls=0;
    basic.AcquireSuite=[](const char* name,ai::int32,const void** out)->SPErr{
        *out=nullptr;auto e=Step();if(e)return e;++acquired;
        if(std::strcmp(name,kAILiveEffectSuite)==0)*out=&live;
        else if(std::strcmp(name,kAIArtSuite)==0)*out=&art;
        else if(std::strcmp(name,kAIPathSuite)==0)*out=&path;
        else if(std::strcmp(name,kAIPathStyleSuite)==0)*out=&paint;
        else if(std::strcmp(name,kAIArtStyleSuite)==0)*out=&styles;
        else if(std::strcmp(name,kAIArtStyleParserSuite)==0)*out=&parser;
        else if(std::strcmp(name,kAIDictionarySuite)==0)*out=&dict;
        else if(std::strcmp(name,kAIDocumentSuite)==0)*out=&doc;
        else if(std::strcmp(name,kAIBlendStyleSuite)==0)*out=&blend;
        else if(std::strcmp(name,kAIMaskSuite)==0)*out=&maskSuite;
        else {--acquired;return -1235;} // a refused suite is never acquired
        return kNoErr;
    };
    basic.ReleaseSuite=[](const char*,ai::int32)->SPErr{++released;return kNoErr;};
    basic.AllocateBlock=[](size_t size,void** out)->SPErr{
        auto e=Step();if(e)return e;++allocated;
        *out=std::malloc(size);return *out?kNoErr:-1236;
    };
    basic.FreeBlock=[](void* p)->SPErr{++freed;std::free(p);return kNoErr;};
    live.CountLiveEffects=[](ai::int32* n)->AIErr{auto e=Step();if(!e)*n=registered;return e;};
    live.GetNthLiveEffect=[](ai::int32 n,AILiveEffectHandle* h)->AIErr{
        auto e=Step();Check(n>=0&&n<registered,"bounded registered index");
        if(!e)*h=reinterpret_cast<AILiveEffectHandle>(static_cast<std::intptr_t>(n+1));return e;
    };
    live.GetLiveEffectName=[](AILiveEffectHandle h,const char** name)->AIErr{
        auto e=Step();
        if(!e)*name=h==reinterpret_cast<AILiveEffectHandle>(1)?strokemaster::topology_probe::effectIdentity
                   :h==reinterpret_cast<AILiveEffectHandle>(2)?region_probe::censusIdentity
                                                              :region_probe::strokeBehindIdentity;
        return e;
    };
    live.GetStyleFilterFlags=[](AILiveEffectHandle,ai::int32* flags)->AIErr{
        auto e=Step();if(!e)*flags=strokemaster::topology_probe::regionFilterType;return e;
    };
    live.AddLiveEffect=[](AILiveEffectData* d,AILiveEffectHandle* h)->AIErr{
        auto e=Step();if(e)return e;
        Check(d->styleFilterFlags==strokemaster::topology_probe::regionFilterType,"post-effect registration");
        if(registered==0){
            Check(std::strcmp(d->name,strokemaster::topology_probe::effectIdentity)==0&&
                  d->prefersAsInput==kGroupInputArt,"region effect registered first, group input only");
        }else if(registered==1){
            Check(std::strcmp(d->name,region_probe::censusIdentity)==0&&d->prefersAsInput==region_probe::censusInput&&
                  !(d->prefersAsInput&kTextFrameInputArt),"census effect second, path/compound/group input, no text");
        }else if(registered==2){
            Check(std::strcmp(d->name,region_probe::strokeBehindIdentity)==0&&
                  d->prefersAsInput==region_probe::censusInput,"stroke-behind third, same input as the census");
        }else if(registered==4){
            Check(std::strcmp(d->name,region_probe::closeHolesIdentity)==0&&d->prefersAsInput==region_probe::censusInput,
                  "Close holes fifth, the census's input (text as outlines)");
        }else if(registered==5){
            Check(std::strcmp(d->name,region_probe::flatTipsIdentity)==0&&d->prefersAsInput==region_probe::censusInput,
                  "flat tips sixth, the census's input (text as outlines)");
        }else{
            Check(std::strcmp(d->name,region_probe::textBehindIdentity)==0&&
                  (d->prefersAsInput&kTextFrameInputArt)&&!(d->prefersAsInput&kPathInputArt),
                  "text-kept stroke-behind fourth, text-frame input and no path input");
        }
        ++registered;*h=reinterpret_cast<AILiveEffectHandle>(static_cast<std::intptr_t>(registered));return kNoErr;
    };
    live.GetLiveEffectHandleByName=[](const char* name,AILiveEffectHandle* h)->AIErr{
        auto e=Step();Check(std::strcmp(name,strokemaster::topology_probe::effectIdentity)==0,"status queries exact separate effect");
        if(!e)*h=registered?reinterpret_cast<AILiveEffectHandle>(1):nullptr;return e;
    };
    art.GetArtType=[](AIArtHandle,short*)->AIErr{return kNoErr;};
    art.GetArtFirstChild=[](AIArtHandle,AIArtHandle* child)->AIErr{*child=nullptr;return kNoErr;};
}
SPMessageData Startup(){
    SPMessageData m{};m.basic=&basic;
    char caller[]=kSPInterfaceCaller,start[]=kSPInterfaceStartupSelector;
    Check(PluginMain(caller,start,&m)==kNoErr,"startup completes");
    Check(m.globals!=nullptr,"startup persists globals");
    return m;
}
}
namespace ise { namespace topology {
// The tests link RegionProbe.cpp, whose default kernel references the real core
// symbol; this mock satisfies the linker without Boost or the reviewed core.
Result process(Request const&) noexcept { Result r{};r.status=Status::Refused;return r; }
}}
int main(){
    // Never the user's switches: a settings file of the tests' own (IllustratorSettings.h).
    { wchar_t tmp[MAX_PATH] = {}; GetTempPathW(MAX_PATH, tmp); wcscat_s(tmp, L"strokemaster-mock-settings.txt"); DeleteFileW(tmp); SetEnvironmentVariableW(L"STROKEMASTER_SETTINGS", tmp); }

    {
        AITextFrameSuite text{};
        AIPaintStyleSuite paintStyle{};
        ATE::TextRangeSuite ranges{};
        ATE::CharFeaturesSuite features{};
        ATE::TextFrameSuite frames{};
        art.DuplicateArt=[](AIArtHandle source,ai::int16 order,AIArtHandle prep,AIArtHandle* out)->AIErr{
            Check(source==reinterpret_cast<AIArtHandle>(101)&&prep==source&&order==kPlaceAbove,
                  "outline copy uses the source frame placement");
            *out=copyFailure?nullptr:reinterpret_cast<AIArtHandle>(102);
            return copyFailure?-456:kNoErr;
        };
        art.DisposeArt=[](AIArtHandle a)->AIErr{
            Check(a==reinterpret_cast<AIArtHandle>(102),"outline helper never disposes live source");
            ++disposedCopies; return kNoErr;
        };
        text.CreateOutline=[](AIArtHandle a,AIArtHandle* out)->AIErr{
            Check(a==reinterpret_cast<AIArtHandle>(102),"only owned copy is outlined");
            ++outlinedCopies;
            *out=outlineFailure?nullptr:reinterpret_cast<AIArtHandle>(103);
            return outlineFailure?-457:kNoErr;
        };
        styles.SetArtStyle=[](AIArtHandle a,AIArtStyleHandle style)->AIErr{
            Check(a==reinterpret_cast<AIArtHandle>(102)&&!style,"only copy loses object appearance");
            return styleFailure?-458:kNoErr;
        };
        region_probe::FixSuites s{art,path,paint,paintStyle,styles,parser,text,ranges,features,frames};
        AIArtHandle outline=reinterpret_cast<AIArtHandle>(999);
        copyFailure=1;
        Check(region_probe::CopyOutline(s,reinterpret_cast<AIArtHandle>(101),&outline)==-456&&
              !outline&&disposedCopies==0&&outlinedCopies==0,"copy failure leaves source intact");
        copyFailure=0; styleFailure=1;
        Check(region_probe::CopyOutline(s,reinterpret_cast<AIArtHandle>(101),&outline)==-458&&
              !outline&&disposedCopies==1&&outlinedCopies==0,"style failure releases copy before outlining");
        styleFailure=0; outlineFailure=1;
        Check(region_probe::CopyOutline(s,reinterpret_cast<AIArtHandle>(101),&outline)==-457&&
              !outline&&disposedCopies==2&&outlinedCopies==1,"outline failure releases its copy");
        outlineFailure=0;
        Check(region_probe::CopyOutline(s,reinterpret_cast<AIArtHandle>(101),&outline)==kNoErr&&
              outline==reinterpret_cast<AIArtHandle>(103)&&disposedCopies==3&&outlinedCopies==2,
              "success releases copy and returns the owned outline");
    }
    {
        std::array<AIPathSegment,4> square{};
        const double x[4]={0,10,10,0},y[4]={0,0,10,10};
        for(int i=0;i<4;++i){
            square[i].p.h=x[i];square[i].p.v=y[i];
            square[i].in=square[i].out=square[i].p;
        }
        ise_miter::Contour converted;
        Check(illustrator_contour::ConvertSdk(square.data(),square.size(),true,&converted)&&
              converted.closed&&converted.segments.size()==4,
              "SDK square converts with an explicit closing edge");
        square[3].out.h=-2;square[3].out.v=8;
        square[0].in.h=-2;square[0].in.v=2;
        Check(illustrator_contour::ConvertSdk(square.data(),square.size(),true,&converted)&&
              converted.segments.back().curve&&converted.segments.back().c1.x==-2&&
              converted.segments.back().c2.y==2,"SDK closing cubic keeps handles");
        square[1].out.h=std::numeric_limits<double>::quiet_NaN();
        Check(!illustrator_contour::ConvertSdk(square.data(),square.size(),true,&converted),
              "SDK nonfinite handle refused");
    }
    {
        Setup();
        live.GetLiveEffectHandleByName=[](const char* name,AILiveEffectHandle* out)->AIErr{
            Check(std::strcmp(name,"Adobe Outline Type")==0,"text pass uses the native outline effect");
            *out=reinterpret_cast<AILiveEffectHandle>(50);return kNoErr;
        };
        live.CreateLiveEffectParameters=[](AILiveEffectParameters* out)->AIErr{
            *out=reinterpret_cast<AILiveEffectParameters>(51);return kNoErr;
        };
        dict.Key=[](const char*)->AIDictKey{return reinterpret_cast<AIDictKey>(52);};
        dict.SetBooleanEntry=[](AIDictionaryRef,AIDictKey,AIBoolean)->AIErr{return kNoErr;};
        dict.SetIntegerEntry=[](AIDictionaryRef,AIDictKey,ai::int32)->AIErr{return kNoErr;};
        dict.Release=[](AIDictionaryRef)->ai::int32{return 0;};
        parser.CountEffectsOfPaintField=[](AIParserPaintField)->ai::int32{return 0;};
        parser.NewParserLiveEffect=[](AILiveEffectHandle effect,AILiveEffectParameters,AIParserLiveEffect* out)->AIErr{
            Check(effect==reinterpret_cast<AILiveEffectHandle>(50),"outline handle reaches the paint pass");
            *out=reinterpret_cast<AIParserLiveEffect>(53);return kNoErr;
        };
        parser.InsertNthEffectOfPaintField=[](AIStyleParser,AIParserPaintField,ai::int32 at,AIParserLiveEffect)->AIErr{
            Check(at==0,"text outlining precedes hole processing");return kNoErr;
        };
        Check(region_probe::AddOutlinePass(&basic,parser,dict,nullptr,reinterpret_cast<AIParserPaintField>(54))==kNoErr,
              "whole text paint pass accepts native outlining");
        live.GetLiveEffectHandleByName=[](const char*,AILiveEffectHandle*)->AIErr{return -456;};
        Check(region_probe::AddOutlinePass(&basic,parser,dict,nullptr,reinterpret_cast<AIParserPaintField>(54))==kBadParameterErr,
              "missing native outline effect is refused");
    }
    for(int fault=0;fault<=2;++fault){
        Setup();failAt=fault;SPMessageData m{};m.basic=&basic;
        char caller[]=kSPInterfaceCaller,start[]=kSPInterfaceStartupSelector;
        Check(PluginMain(caller,start,&m)==(fault?-1234:kNoErr),"registration propagates SDK failure");
        if(fault)Check(!m.globals,"failed startup owns no globals");
        char stop[]=kSPInterfaceShutdownSelector;
        Check(PluginMain(caller,stop,&m)==kNoErr,"shutdown after fault is clean");
    }
    Setup();{
        auto m=Startup();
        Check(registered==6&&m.globals!=nullptr,"all six effects registered and globals allocated");
        auto* state=static_cast<region_probe::State*>(m.globals);
        Check(state->effect==reinterpret_cast<AILiveEffectHandle>(1)&&
              state->census==reinterpret_cast<AILiveEffectHandle>(2)&&
              state->strokeBehind==reinterpret_cast<AILiveEffectHandle>(3)&&
              state->textBehind==reinterpret_cast<AILiveEffectHandle>(4),"persisted effect handles");
        char payload[4096]{};
        Check(region_probe::Payload(m,*state,"status",true,kNoErr,payload,sizeof(payload))==kNoErr,"status payload");
        Check(std::strstr(payload,"\"plugin\":\"StrokeMasterRegionProbe\"")&&
              std::strstr(payload,"\"geometryEnabled\":true")&&
              std::strstr(payload,"\"kernelAdmitted\":false")&&
              std::strstr(payload,"\"supportedGate\":\"MARKED_ANALYTIC_FIXTURE_ONLY\""),
              "geometry-enabled identity with bounded gate");
        char pid[64]{};std::snprintf(pid,sizeof(pid),"\"processId\":%lu",static_cast<unsigned long>(GetCurrentProcessId()));
        Check(std::strstr(payload,pid)!=nullptr,"native process attribution");
        Check(region_probe::Payload(m,*state,"unexpected",false,kBadParameterErr,payload,sizeof(payload))==kNoErr&&
              std::strstr(payload,"\"ok\":false"),"unknown bridge action refused");
        Check(region_probe::Payload(m,*state,"status",true,kNoErr,payload,8)==kBadParameterErr,"truncated JSON refused");
        // Go with an unknown effect handle is refused before any callback work.
        AILiveEffectGoMessage go{};go.d.basic=&basic;go.d.globals=m.globals;
        go.art=reinterpret_cast<AIArtHandle>(1);go.srcArt=reinterpret_cast<AIArtHandle>(2);
        char goCaller[]=kCallerAILiveEffect,selector[]=kSelectorAIGoLiveEffect;
        go.effect=reinterpret_cast<AILiveEffectHandle>(9);
        Check(PluginMain(goCaller,selector,&go)==kBadParameterErr,"unknown effect handle refused");
        Check(state->metrics.goCalls==0,"mismatched handle never reaches the callback");
        // Null processing input is refused without acquiring any suite.
        const int suitesBefore=acquired;
        go.effect=state->effect;go.art=nullptr;
        Check(PluginMain(goCaller,selector,&go)==kNoErr,"null input refused as a silent pass-through");
        Check(state->metrics.goCalls==1&&state->metrics.mutations==0&&
              std::strcmp(state->metrics.gate,"input.null")==0,"null-input refusal recorded at its gate");
        Check(acquired==suitesBefore,"null-input refusal acquires no suites");
        // Alias and provenance refusals.
        go.art=reinterpret_cast<AIArtHandle>(1);go.srcArt=go.art;
        Check(PluginMain(goCaller,selector,&go)==kNoErr,"source alias refused as a silent pass-through");
        Check(std::strcmp(state->metrics.gate,"input.source-alias")==0,"alias refusal recorded at its gate");
        go.srcArt=reinterpret_cast<AIArtHandle>(2);go.parameters=nullptr;
        Check(PluginMain(goCaller,selector,&go)==kNoErr,"missing marker refused as a silent pass-through");
        Check(std::strcmp(state->metrics.gate,"source.parameters-missing")==0,"provenance refusal recorded at its gate");
        Check(state->metrics.goCalls==3&&state->metrics.mutations==0&&
              state->metrics.failedCalls==3,"refusals counted, never mutated");
        Check(state->metrics.passThroughs==3&&state->metrics.lastGoError==kBadParameterErr,
              "each refusal is a recorded pass-through, not a host error");
        Check(region_probe::Payload(m,*state,"status",true,kNoErr,payload,sizeof(payload))==kNoErr&&
              std::strstr(payload,"\"passThroughs\":3")&&std::strstr(payload,"\"wrapperPaintFields\":-1")&&
              std::strstr(payload,"\"wrapperContainsPaint\":-1")&&std::strstr(payload,"\"wrapperBase\":0")&&
              std::strstr(payload,"\"groupContentsPosition\":-1")&&
              std::strstr(payload,"\"processingCensus\":\"0\""),"status reports pass-throughs, unread wrapper census, and the last processing census");
        // The census effect never errors and never mutates; it only records.
        go.effect=state->census;go.art=nullptr;
        Check(PluginMain(goCaller,selector,&go)==kNoErr,"census Go with no art passes through");
        go.art=reinterpret_cast<AIArtHandle>(1);
        Check(PluginMain(goCaller,selector,&go)==kNoErr,"census Go records and passes through");
        Check(state->inputCensus.calls==2&&state->inputCensus.nodes==1&&state->inputCensus.other==1&&
              std::strcmp(state->inputCensus.tree,"0")==0,"census walked the single mocked node");
        Check(state->metrics.goCalls==3&&state->metrics.mutations==0,"census calls are not region calls");
        Check(region_probe::Payload(m,*state,"status",true,kNoErr,payload,sizeof(payload))==kNoErr&&
              std::strstr(payload,"\"census\":{\"registered\":true,\"calls\":2,\"nodes\":1"),
              "status reports the census");
        // The appearance command refuses cleanly when a suite it needs is unavailable
        // (the mock host offers no matching-art suite) and reports where it stopped.
        Check(region_probe::AppearanceStrokeBelow(m,*state)!=kNoErr&&
              std::strcmp(state->appearance.gate,"suites")==0&&state->appearance.styled==0,
              "appearance refuses without its suites and styles nothing");
        Check(region_probe::StoryFill(m,*state)!=kNoErr&&
              std::strcmp(state->appearance.gate,"story.suites")==0&&state->appearance.styled==0,
              "story fill refuses without suites before writing any style");
        Check(region_probe::Payload(m,*state,"status",true,kNoErr,payload,sizeof(payload))==kNoErr&&
              std::strstr(payload,"\"appearance\":{\"gate\":\"story.suites\",\"frames\":0,\"styled\":0"),
              "status reports the appearance gate");
        Check(region_probe::StoryFill(m,*state,true)!=kNoErr&&std::strcmp(state->appearance.gate,"story.suites")==0,
              "the separate-stories opt-in refuses without suites too");
        // The panel's switches (IllustratorSettings.h): InDesign's key=0|1 format, defaults as InDesign's
        // panel where the correction is the same, the opt-ins and Trim glyph tips off.
        {
            ai_settings::Settings d;
            Check(d.enabled&&d.textBehind&&d.textStory&&!d.textStories&&d.textMiter&&d.wholePoints&&d.glyphTips&&
                  d.overlaps&&d.patternPoints&&!d.outsideDashes&&d.followEdits,"switch defaults");
            ai_settings::Settings s;
            ai_settings::Parse("text_stories=1\r\nglyph_tips=0\nbogus=1\nfollow_edits=x\noutside_dashes=1",s);
            Check(s.textStories&&!s.glyphTips&&s.outsideDashes&&s.followEdits,"parse: known keys set, unknown and malformed skipped");
            ai_settings::Settings r;
            ai_settings::Parse(ai_settings::Format(s).c_str(),r);
            Check(r==s,"format and parse round-trip");
            Check(ai_settings::Format(d).find("text_stories=0\n")!=std::string::npos,"one key=0|1 line per switch");
            ai_settings::Settings b;
            ai_settings::Parse("\xEF\xBB\xBF" "enabled = 0\r\n  glyph_tips=\t0",b);
            Check(!b.enabled&&!b.glyphTips,"a BOM and spaces around '=' are read, as InDesign's reader");
            wchar_t over[MAX_PATH]={};
            Check(GetEnvironmentVariableW(L"STROKEMASTER_SETTINGS",over,MAX_PATH)>0&&ai_settings::Path()==over,
                  "the tests use a settings file of their own");
        }
        // The text route's decision (2026-10-30): one stroke below first, so a recolor of the
        // fills stays drawn without the plugin; then one fill above; then FillFirst off.
        {
            region_probe::CharPaint c;
            Check(region_probe::ChooseRoute(c)==region_probe::kRouteNone,"no characters: no route");
            Check(region_probe::GroupOwnsTextFields(region_probe::kRouteStroke),"stroke route supplies group hole and tip fields");
            Check(!region_probe::GroupRouteSupportsFields(region_probe::kRouteFill,true,false)&&
                  !region_probe::GroupRouteSupportsFields(region_probe::kRouteFill,false,true),"mixed stroke group refuses incompatible child correction fields");
            Check(region_probe::GroupRouteSupportsFields(region_probe::kRouteFill,false,false)&&
                  region_probe::GroupRouteSupportsFields(region_probe::kRouteStroke,true,true),"compatible group routes remain admitted");
            Check(!region_probe::GroupOwnsTextFields(region_probe::kRouteFill)&&
                  !region_probe::GroupOwnsTextFields(region_probe::kRouteFillFirst)&&
                  !region_probe::GroupOwnsTextFields(region_probe::kRouteNone),"other routes retain per-frame hole and tip fields");
            c.any=true;c.noStroke=false;c.oneStroke=true;c.oneFill=true;
            Check(region_probe::ChooseRoute(c)==region_probe::kRouteStroke,"one stroke and one fill: the stroke below");
            c.oneStroke=false;
            Check(region_probe::ChooseRoute(c)==region_probe::kRouteFill,"mixed strokes, one fill: the fill above");
            c.oneFill=false;
            Check(region_probe::ChooseRoute(c)==region_probe::kRouteFillFirst,"both mixed: FillFirst off");
            c.noStroke=true;
            Check(region_probe::ChooseRoute(c)==region_probe::kRouteNone,"no stroke anywhere: no route");
            AIColor red{},red2{},gray{};
            red.kind=red2.kind=kThreeColor;
            red.c.rgb.red=red2.c.rgb.red=0.8f;red.c.rgb.green=red2.c.rgb.green=0.1f;red.c.rgb.blue=red2.c.rgb.blue=0.1f;
            gray.kind=kGrayColor;gray.c.g.gray=0.8f;
            Check(region_probe::SameSolid(red,red2)&&!region_probe::SameSolid(red,gray),"solid colors compare by kind and components");
            red2.c.rgb.blue=0.2f;
            Check(!region_probe::SameSolid(red,red2),"one component apart is another color");
            AIStrokeStyle s1{},s2{};
            s1.color=s2.color=red;s1.width=s2.width=7;s1.join=s2.join=kAIMiterJoin;s1.miterLimit=s2.miterLimit=4;
            Check(region_probe::SameStroke(s1,s2),"identical strokes compare equal");
            s2.dash.length=2;s2.dash.array[0]=3;s2.dash.array[1]=2;
            Check(!region_probe::SameStroke(s1,s2),"a dashed stroke differs from a solid one");
            s2.dash.length=0;s2.width=8;
            Check(!region_probe::SameStroke(s1,s2),"another weight is another stroke");
        }
        // Stroke-behind refusals pass through; the transform itself has its own suite.
        go.effect=state->strokeBehind;go.art=nullptr;
        Check(PluginMain(goCaller,selector,&go)==kNoErr,"stroke-behind with no art passes through");
        Check(state->strokeBehindStats.calls==1&&state->strokeBehindStats.refused==1&&
              std::strcmp(state->strokeBehindStats.gate,"input.null")==0,"stroke-behind refusal recorded");
        Check(region_probe::Payload(m,*state,"status",true,kNoErr,payload,sizeof(payload))==kNoErr&&
              std::strstr(payload,"\"strokeBehind\":{\"registered\":true,\"calls\":1,\"published\":0,\"refused\":1"),
              "status reports stroke-behind");
        char stop2[]=kSPInterfaceShutdownSelector,iface[]=kSPInterfaceCaller;
        Check(PluginMain(iface,stop2,&m)==kNoErr&&m.globals==nullptr,"shutdown frees globals");
        Check(allocated==1&&freed==1,"exactly one owned heap block");
        Check(acquired==released,"all suites released");
        SPMessageData foreign{};foreign.basic=&basic;foreign.globals=reinterpret_cast<void*>(1);
        char start2[]=kSPInterfaceStartupSelector;
        Check(PluginMain(iface,start2,&foreign)==kBadParameterErr,"unknown persistent layout rejected before dereference");
    }
    std::cout<<"PASS: "<<checks<<" region registration/bridge/refusal assertions; no host execution.\n";
}
