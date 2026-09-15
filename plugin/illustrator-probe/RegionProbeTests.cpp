// Registration-only target tests. No Illustrator or real SDK implementation.
#include "RegionProbe.cpp"
#include <cstdlib>
#include <iostream>
namespace {
SPBasicSuite basic{};AILiveEffectSuite live{};
int checks=0,acquired=0,released=0,registered=0,failAt=0,calls=0;
bool nestedShutdown=false;
constexpr ASErr failure=-1234;
void Check(bool x,const char* s){++checks;if(!x){std::cerr<<"FAIL: "<<s<<'\n';std::exit(1);}}
ASErr Step(){return ++calls==failAt?failure:kNoErr;}
void Setup(){
    acquired=released=registered=failAt=calls=0;nestedShutdown=false;
    basic.AcquireSuite=[](const char* name,ai::int32 version,const void** out)->SPErr{
        *out=nullptr;auto e=Step();if(e)return e;
        Check(std::strcmp(name,kAILiveEffectSuite)==0&&version==kAILiveEffectVersion,"exact live-effect suite gate");
        ++acquired;*out=&live;return kNoErr;
    };
    basic.ReleaseSuite=[](const char*,ai::int32)->SPErr{++released;return kNoErr;};
    // AllocateBlock, FreeBlock, and every art suite are deliberately unavailable.
    live.CountLiveEffects=[](ai::int32* n)->AIErr{auto e=Step();if(!e)*n=registered;return e;};
    live.GetNthLiveEffect=[](ai::int32 n,AILiveEffectHandle* h)->AIErr{
        auto e=Step();Check(n==0,"bounded registered index");if(!e)*h=reinterpret_cast<AILiveEffectHandle>(1);return e;
    };
    live.GetLiveEffectName=[](AILiveEffectHandle,const char** name)->AIErr{
        auto e=Step();if(!e)*name=strokemaster::topology_probe::effectIdentity;return e;
    };
    live.GetStyleFilterFlags=[](AILiveEffectHandle,ai::int32* flags)->AIErr{
        auto e=Step();if(!e)*flags=kPostEffectFilter;return e;
    };
    live.AddLiveEffect=[](AILiveEffectData* d,AILiveEffectHandle* h)->AIErr{
        auto e=Step();if(e)return e;
        Check(d->styleFilterFlags==kPostEffectFilter&&d->prefersAsInput==kGroupInputArt,"whole-object post-effect registration");
        Check(std::strcmp(d->name,strokemaster::topology_probe::effectIdentity)==0,"distinct region effect identity");
        if(nestedShutdown){
            SPMessageData stop{};stop.basic=&basic;
            char caller[]=kSPInterfaceCaller,selector[]=kSPInterfaceShutdownSelector;
            Check(PluginMain(caller,selector,&stop)==kNoErr&&!stop.globals,"nested shutdown needs no persistent allocation");
        }
        ++registered;*h=reinterpret_cast<AILiveEffectHandle>(1);return kNoErr;
    };
    live.GetLiveEffectHandleByName=[](const char* name,AILiveEffectHandle* h)->AIErr{
        auto e=Step();Check(std::strcmp(name,strokemaster::topology_probe::effectIdentity)==0,"status queries exact separate effect");
        if(!e)*h=registered?reinterpret_cast<AILiveEffectHandle>(1):nullptr;return e;
    };
}
}
int main(){
    for(int fault=0;fault<=3;++fault){
        Setup();failAt=fault;SPMessageData m{};m.basic=&basic;
        char caller[]=kSPInterfaceCaller,start[]=kSPInterfaceStartupSelector,stop[]=kSPInterfaceShutdownSelector;
        Check(PluginMain(caller,start,&m)==(fault?failure:kNoErr),"registration propagates SDK failure");
        Check(!m.globals&&acquired==released,"no persistent heap state and balanced suites");
        Check(PluginMain(caller,stop,&m)==kNoErr&&!m.globals,"shutdown without owned heap");
    }
    Setup();{
        nestedShutdown=true;SPMessageData m{};m.basic=&basic;
        char caller[]=kSPInterfaceCaller,start[]=kSPInterfaceStartupSelector;
        Check(PluginMain(caller,start,&m)==kNoErr,"startup completes through nested SDK shutdown");
        Check(PluginMain(caller,start,&m)==kNoErr&&registered==1,"no duplicate registration");
        char payload[768]{};
        Check(region_probe::Payload(m,"status",true,payload,sizeof(payload))==kNoErr,"read-only bridge payload");
        Check(std::strstr(payload,"\"plugin\":\"StrokeMasterRegionProbe\"")&&std::strstr(payload,"\"geometryEnabled\":false")&&
            std::strstr(payload,"\"kernelAdmitted\":false")&&std::strstr(payload,"\"nativeGeometryExecuted\":false"),"explicit registration-only identity and admission");
        char pid[64]{};std::snprintf(pid,sizeof(pid),"\"processId\":%lu",static_cast<unsigned long>(GetCurrentProcessId()));
        Check(std::strstr(payload,pid)!=nullptr,"native process attribution");
        const int before=acquired;
        for(const char* selector:{"create","unexpected"}){
            Check(region_probe::Payload(m,selector,false,payload,sizeof(payload))==kNoErr&&std::strstr(payload,"\"ok\":false"),"mutating and unknown bridge actions refused");
        }
        Check(acquired==before,"refused bridge action acquires no art suites");
        Check(region_probe::Payload(m,"status",true,payload,8)==kBadParameterErr,"truncated JSON refused");
        AILiveEffectGoMessage go{};go.d.basic=&basic;go.art=reinterpret_cast<AIArtHandle>(1);go.srcArt=reinterpret_cast<AIArtHandle>(2);
        char goCaller[]=kCallerAILiveEffect,selector[]=kSelectorAIGoLiveEffect;
        Check(PluginMain(goCaller,selector,&go)==kBadParameterErr&&go.art==reinterpret_cast<AIArtHandle>(1),"registration-only Go never touches input");
        Check(acquired==released,"all suites released after protocol calls");
        m.globals=reinterpret_cast<void*>(1);
        Check(PluginMain(caller,start,&m)==kBadParameterErr,"unknown persistent layout rejected before dereference");
    }
    std::cout<<"PASS: "<<checks<<" registration/bridge assertions; no art suites, heap allocation, or host execution.\n";
}
