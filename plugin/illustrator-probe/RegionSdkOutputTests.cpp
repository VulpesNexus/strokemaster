// Compile against external SDK declarations; every suite below is a test double.
#include "RegionSdkOutput.h"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace strokemaster::topology_probe;
namespace {
struct Art {
    short type=kPathArt;bool alive=true;int parent=-1;
    std::vector<int> children;
    std::vector<AIPathSegment> segments;
    AIBoolean closed=false;AIPathStyle style{};
};
std::array<Art,16> pool;
int nextArt=4,calls=0,failAt=0,checks=0,oldDisposals=0;
bool failCleanup=false,partialCreate=false;
constexpr AIErr failure=-1234;
AIArtSuite art{};AIPathSuite path{};AIPathStyleSuite paint{};
void Check(bool value,const char* why){++checks;if(!value){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
AIArtHandle H(int i){return reinterpret_cast<AIArtHandle>(&pool[i]);}
int Index(AIArtHandle h){return int(reinterpret_cast<Art*>(h)-pool.data());}
Art& A(AIArtHandle h){auto& a=pool[Index(h)];Check(a.alive,"no use of disposed art");return a;}
AIErr Step(){return ++calls==failAt?failure:kNoErr;}
void Delete(int i){for(auto child:pool[i].children)if(pool[child].alive)Delete(child);pool[i].alive=false;}
void Setup(){
    pool={};nextArt=4;calls=failAt=oldDisposals=0;failCleanup=partialCreate=false;
    pool[0].type=pool[2].type=kGroupArt;pool[0].children={1};pool[2].children={3};
    pool[1].parent=0;pool[3].parent=2;
    art.GetArtType=[](AIArtHandle h,short* out)->AIErr{auto e=Step();if(!e)*out=A(h).type;return e;};
    art.GetArtFirstChild=[](AIArtHandle h,AIArtHandle* out)->AIErr{
        auto e=Step();if(!e){*out=nullptr;for(auto i:A(h).children)if(pool[i].alive){*out=H(i);break;}}return e;
    };
    art.GetArtSibling=[](AIArtHandle h,AIArtHandle* out)->AIErr{
        auto e=Step();if(!e){*out=nullptr;bool found=false;for(auto i:pool[A(h).parent].children){
            if(found&&pool[i].alive){*out=H(i);break;}if(i==Index(h))found=true;}}return e;
    };
    art.GetArtParent=[](AIArtHandle h,AIArtHandle* out)->AIErr{auto e=Step();if(!e)*out=H(A(h).parent);return e;};
    art.NewArt=[](ai::int16 type,ai::int16 order,AIArtHandle parent,AIArtHandle* out)->AIErr{
        auto e=Step();*out=nullptr;if(e&&!partialCreate)return e;
        Check(parent!=H(2)&&parent!=H(3),"new art never placed under editable source");
        int i=nextArt++;pool[i].type=type;pool[i].parent=Index(parent);*out=H(i);
        auto& list=A(parent).children;
        if(order==kPlaceInsideOnTop)list.insert(list.begin(),i);else list.push_back(i);
        return e;
    };
    art.DisposeArt=[](AIArtHandle h)->AIErr{
        Check(h!=H(0)&&h!=H(2)&&h!=H(3),"never dispose processing root or editable source");
        auto e=Step();if(e||(failCleanup&&Index(h)>=4))return failure;
        if(h==H(1))++oldDisposals;Delete(Index(h));return kNoErr;
    };
    path.SetPathSegmentCount=[](AIArtHandle h,ai::int16 n)->AIErr{auto e=Step();if(!e)A(h).segments.resize(n);return e;};
    path.SetPathSegments=[](AIArtHandle h,ai::int16 start,ai::int16 n,const AIPathSegment* v)->AIErr{
        auto e=Step();if(!e)for(int i=0;i<n;++i)A(h).segments[start+i]=v[i];return e;
    };
    path.SetPathClosed=[](AIArtHandle h,AIBoolean c)->AIErr{auto e=Step();if(!e)A(h).closed=c;return e;};
    path.GetPathSegmentCount=[](AIArtHandle h,ai::int16* n)->AIErr{auto e=Step();if(!e)*n=ai::int16(A(h).segments.size());return e;};
    path.GetPathClosed=[](AIArtHandle h,AIBoolean* c)->AIErr{auto e=Step();if(!e)*c=A(h).closed;return e;};
    path.GetPathSegments=[](AIArtHandle h,ai::int16 start,ai::int16 n,AIPathSegment* v)->AIErr{
        auto e=Step();if(!e)for(int i=0;i<n;++i)v[i]=A(h).segments[start+i];return e;
    };
    paint.SetPathStyleForceSimple=[](AIArtHandle h,const AIPathStyle* s)->AIErr{auto e=Step();if(!e)A(h).style=*s;return e;};
    paint.GetPathStyle=[](AIArtHandle h,AIPathStyle* s,AIBoolean* adv)->AIErr{
        auto e=Step();if(!e){*s=A(h).style;*adv=false;}return e;
    };
}
Output Specimen(){
    const double h=.3,q=h*std::sqrt(5.0);
    return {{{{-h,-h},{10+h,-h},{10+h,10+h},{-h,10+h}},
        {{{.2+q,5},{2.2-h,4+(h+q)/2},{2.2-h,6-(h+q)/2}}}},.75};
}
void SourceIntact(){Check(pool[2].alive&&pool[3].alive&&pool[2].children==std::vector<int>{3}&&
    pool[3].segments.empty(),"editable source remains unchanged");}
void Clean(){for(int i=4;i<nextArt;++i)Check(!pool[i].alive,"all owned staged art reclaimed");SourceIntact();}
}
int main(){
    static_assert(regionFilterType==kPostEffectFilter&&regionMergeAction==kAppendLiveEffectToStyle,
        "reserved whole-object attachment contract; not registered by these tests");
    Setup();int stageCalls=0;
    {
        RegionSdkOutput sink(art,path,paint,H(0),H(2));
        Check(sink.Stage(Specimen()),"SDK compound stage succeeds");stageCalls=calls;
        Check(pool[1].alive&&oldDisposals==0,"input child survives all validation");
        Check(pool[4].type==kCompoundPathArt&&pool[4].style.evenodd&&pool[4].style.fillPaint&&
            !pool[4].style.strokePaint&&pool[4].children.size()==2,"explicit shell/hole become filled even-odd compound");
        Check(sink.Commit()&&sink.commitAttempted&&oldDisposals==1&&pool[4].alive,"single final deletion publishes staged compound");
        Check(sink.Rollback()&&pool[4].alive,"published output not deleted by rollback");SourceIntact();
    }
    for(bool partial : {false,true})for(int fault=1;fault<=stageCalls+1;++fault){
        Setup();failAt=fault;partialCreate=partial;
        RegionSdkOutput sink(art,path,paint,H(0),H(2));
        bool staged=sink.Stage(Specimen());
        Check(!staged||!sink.Commit(),"injected SDK stage/commit error propagates");
        Check(sink.Rollback(),"SDK error cleans owned staging");
        Check(pool[1].alive&&oldDisposals==0,"mock atomic failure leaves input child intact");Clean();
    }
    Setup();{
        RegionSdkOutput sink(art,path,paint,H(0),H(2));
        Check(sink.Stage(Specimen()),"stage for cleanup failure");failCleanup=true;
        Check(!sink.Rollback()&&pool[4].alive,"cleanup failure explicit with retained owned handle");
        failCleanup=false;Check(sink.Rollback(),"cleanup retry succeeds");Clean();
    }
    Setup();{
        RegionSdkOutput sink(art,path,paint,H(2),H(2));
        Check(!sink.Stage(Specimen())&&calls==0,"source alias refused before SDK access");SourceIntact();
    }
    Setup();{
        pool[0].children={3};RegionSdkOutput sink(art,path,paint,H(0),H(2));
        Check(!sink.Stage(Specimen())&&nextArt==4,"editable compound child alias refused before creation");SourceIntact();
    }
    std::cout<<"PASS: "<<checks<<" SDK-mock assertions; "<<2*(stageCalls+1)<<" injected stage/commit failure runs.\n"
        <<"PROPOSED: no plugin registration, real core binding, host load, or observed native compound output.\n";
}
