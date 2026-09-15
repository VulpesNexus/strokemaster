// PROPOSED registration-only region target. Geometry admission is disabled.
// No persistent heap state, startup art creation, or link to the candidate core.
#include "SPBasic.h"
#include "SPInterf.h"
#include "SPAccess.h"
#include "AIScriptMessage.h"
#include "AIUnicodeString.h"
#include "RegionSdkOutput.h"
#include <Windows.h>
#include <cstring>
#include <cstdio>
namespace region_probe {
using namespace strokemaster::topology_probe;
constexpr char build[]="region-prep-1";
template<class T> struct Suite {
    SPBasicSuite* basic;const char* name;ai::int32 version;const T* value=nullptr;ASErr error=kBadParameterErr;
    Suite(SPBasicSuite* b,const char* n,ai::int32 v):basic(b),name(n),version(v){
        const void* p=nullptr;error=basic->AcquireSuite(name,version,&p);
        if(!error){value=static_cast<const T*>(p);if(!value){basic->ReleaseSuite(name,version);error=kBadParameterErr;}}
    }
    ~Suite(){if(value)basic->ReleaseSuite(name,version);}
    Suite(const Suite&)=delete;Suite& operator=(const Suite&)=delete;
};
ASErr Register(SPMessageData& message){
    Suite<AILiveEffectSuite> live(message.basic,kAILiveEffectSuite,kAILiveEffectVersion);
    if(live.error)return live.error;
    ai::int32 count=0;auto err=live.value->CountLiveEffects(&count);if(err)return err;
    if(count<0||count>8192)return kBadParameterErr;
    // Enumeration differentiates absent identity from a failed lookup operation.
    for(ai::int32 i=0;i<count;++i){
        AILiveEffectHandle handle=nullptr;const char* name=nullptr;
        err=live.value->GetNthLiveEffect(i,&handle);if(err)return err;
        if(!handle)return kBadParameterErr;
        err=live.value->GetLiveEffectName(handle,&name);if(err)return err;
        if(name&&std::strcmp(name,effectIdentity)==0){
            ai::int32 flags=0;err=live.value->GetStyleFilterFlags(handle,&flags);
            return err?err:flags==regionFilterType?kNoErr:kBadParameterErr;
        }
    }
    AILiveEffectData data{};char title[]="StrokeMaster PROPOSED closed region";
    data.self=message.self;data.name=effectIdentity;data.title=title;
    data.majorVersion=1;data.minorVersion=0;data.prefersAsInput=kGroupInputArt;
    data.styleFilterFlags=regionFilterType;
    AILiveEffectHandle handle=nullptr;
    err=live.value->AddLiveEffect(&data,&handle);return err?err:handle?kNoErr:kBadParameterErr;
}
ASErr Payload(SPMessageData& message,const char* selector,bool emptyInput,char* output,size_t capacity){
    const bool read=emptyInput&&(std::strcmp(selector,"status")==0||std::strcmp(selector,"diagnostics")==0);
    bool registered=false;
    if(read){
        Suite<AILiveEffectSuite> live(message.basic,kAILiveEffectSuite,kAILiveEffectVersion);
        if(live.error)return live.error;
        AILiveEffectHandle handle=nullptr;
        const auto err=live.value->GetLiveEffectHandleByName(effectIdentity,&handle);if(err)return err;
        registered=handle!=nullptr;
    }
    const int n=std::snprintf(output,capacity,
        "{\"schema\":1,\"plugin\":\"%s\",\"build\":\"%s\",\"processId\":%lu,\"ok\":%s,"
        "\"effectRegistered\":%s,\"effect\":\"%s\",\"attachment\":\"POST_OBJECT_APPEND\","
        "\"kernelAdmitted\":false,\"geometryEnabled\":false,\"nativeGeometryExecuted\":false,"
        "\"gate\":\"KERNEL_AND_SOURCE_ADMISSION_PENDING\"}",
        regionPluginName,build,static_cast<unsigned long>(GetCurrentProcessId()),read?"true":"false",
        registered?"true":"false",effectIdentity);
    return n<0||static_cast<size_t>(n)>=capacity?kBadParameterErr:kNoErr;
}
ASErr Script(AIScriptMessage& message,const char* selector){
    Suite<AIUnicodeStringSuite> unicode(message.d.basic,kAIUnicodeStringSuite,kAIUnicodeStringVersion);
    if(unicode.error)return unicode.error;
    char payload[768]{};
    auto err=Payload(message.d,selector,unicode.value->Length(message.inParam)==0,payload,sizeof(payload));if(err)return err;
    err=unicode.value->Destroy(message.outParam);if(err)return err;
    return unicode.value->Initialize(message.outParam,payload,
        static_cast<ai::UnicodeString::offset_type>(std::strlen(payload)),kAIUTF8CharacterEncoding);
}
} // namespace region_probe
extern "C" ASAPI ASErr PluginMain(char* caller,char* selector,void* raw){
    if(!caller||!selector||!raw)return kBadParameterErr;
    auto& message=*static_cast<SPMessageData*>(raw);if(!message.basic)return kBadParameterErr;
    // This registration-only target never allocates or frees message.globals.
    // Reject unexpected persisted state instead of interpreting another build's layout.
    if(message.globals)return kBadParameterErr;
    try{
        if(std::strcmp(caller,kSPInterfaceCaller)==0){
            if(std::strcmp(selector,kSPInterfaceStartupSelector)==0)return region_probe::Register(message);
            return kNoErr;
        }
        if(std::strcmp(caller,kSPAccessCaller)==0)return kNoErr;
        if(std::strcmp(caller,kCallerAIScriptMessage)==0)return region_probe::Script(*static_cast<AIScriptMessage*>(raw),selector);
        if(std::strcmp(caller,kCallerAILiveEffect)==0)return kBadParameterErr;
        return kNoErr;
    }catch(...){return kBadParameterErr;}
}
