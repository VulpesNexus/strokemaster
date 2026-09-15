// PROPOSED region-prep-2: real-core closed-region replacement for the marked
// analytic fixture only. RegionNative.h carries the validated integration:
// parser-checked source admission, the closed-form fixture oracle, and the
// owned-output SDK backend. Arbitrary artwork is refused before staging.
#include "SPBasic.h"
#include "SPInterf.h"
#include "SPAccess.h"
#include "AIScriptMessage.h"
#include "AIUnicodeString.h"
#include "RegionNative.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace region_probe {
constexpr char build[] = "region-prep-2";

struct Event {
    unsigned long long sequence = 0;
    char stage[64]{}, detail[112]{};
    ASErr error = kNoErr;
};
struct Telemetry {
    unsigned long long sequence = 0;
    std::array<Event, 32> events{};
};
// This pointer exists only during a synchronous PluginMain invocation. Persistent
// telemetry stores copied text, never pointers into a reloadable DLL.
Telemetry* currentTelemetry = nullptr;
struct TelemetryScope {
    Telemetry* previous;
    explicit TelemetryScope(Telemetry* value) : previous(currentTelemetry) { currentTelemetry = value; }
    ~TelemetryScope() { currentTelemetry = previous; }
};
template<size_t N> void Label(char (&out)[N], const char* text) noexcept {
    size_t i = 0;
    if (text) for (; i + 1 < N && text[i]; ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        out[i] = c >= 32 && c < 127 && c != '"' && c != '\\' ? text[i] : '_';
    }
    out[i] = '\0';
}
void Trace(const char* stage, ASErr error = kNoErr, const char* detail = "") noexcept {
    Event event{};
    Label(event.stage, stage); Label(event.detail, detail);
    event.error = error;
    if (currentTelemetry) {
        event.sequence = ++currentTelemetry->sequence;
        currentTelemetry->events[(event.sequence - 1) % currentTelemetry->events.size()] = event;
    }
}

// Each invocation owns its acquisitions. No suite pointer survives unload.
template<class T> class Suite {
    SPBasicSuite* basic_;
    const char* name_;
    ai::int32 version_;
    const T* value_ = nullptr;
public:
    ASErr error;
    Suite(SPBasicSuite* basic, const char* name, ai::int32 version)
        : basic_(basic), name_(name), version_(version), error(kBadParameterErr) {
        const void* acquiredPointer = nullptr;
        if (basic_) error = basic_->AcquireSuite(name_, version_, &acquiredPointer);
        if (!error) {
            value_ = static_cast<const T*>(acquiredPointer);
            if (!value_) {
                basic_->ReleaseSuite(name_, version_);
                error = kBadParameterErr;
            }
        }
        Trace("suite.acquire", error, name_);
    }
    ~Suite() { if (value_) Trace("suite.release", basic_->ReleaseSuite(name_, version_), name_); }
    Suite(const Suite&) = delete;
    Suite& operator=(const Suite&) = delete;
    const T* operator->() const { return value_; }
    const T* operator&() const { return value_; }
};

struct State {
    AILiveEffectHandle effect = nullptr;
    unsigned long long fixtures = 0;
    Metrics metrics{};
    unsigned activeDepth = 0;
    bool pendingFree = false;
    Telemetry telemetry{};
};

// All suites Native borrows are acquired here and outlive the operation call.
template<class Op> ASErr WithNative(SPMessageData& message, State& state, const char* context, Op operation) {
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(message.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIPathStyleSuite> paint(message.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDocumentSuite> doc(message.basic, kAIDocumentSuite, kAIDocumentVersion);
    Suite<AIBlendStyleSuite> blend(message.basic, kAIBlendStyleSuite, kAIBlendStyleVersion);
    if (art.error) { Trace(context, art.error, "art"); return art.error; }
    if (path.error) { Trace(context, path.error, "path"); return path.error; }
    if (paint.error) { Trace(context, paint.error, "paint"); return paint.error; }
    if (styles.error) { Trace(context, styles.error, "styles"); return styles.error; }
    if (parser.error) { Trace(context, parser.error, "parser"); return parser.error; }
    if (dict.error) { Trace(context, dict.error, "dictionary"); return dict.error; }
    if (live.error) { Trace(context, live.error, "live"); return live.error; }
    if (doc.error) { Trace(context, doc.error, "document"); return doc.error; }
    if (blend.error) { Trace(context, blend.error, "blend"); return blend.error; }
    Native native{*art.operator&(), *path.operator&(), *paint.operator&(), *styles.operator&(),
                  *parser.operator&(), *dict.operator&(), *live.operator&(), *doc.operator&(),
                  *blend.operator&(), state.metrics};
    return operation(native);
}

ASErr Register(SPMessageData& message, State& state) {
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    if (live.error) return live.error;
    ai::int32 count = 0;
    auto err = live->CountLiveEffects(&count);
    if (err) return err;
    if (count < 0 || count > 8192) return kBadParameterErr;
    // Enumeration differentiates absent identity from a failed lookup operation.
    for (ai::int32 i = 0; i < count; ++i) {
        AILiveEffectHandle handle = nullptr;
        const char* name = nullptr;
        err = live->GetNthLiveEffect(i, &handle);
        if (err) return err;
        if (!handle) return kBadParameterErr;
        err = live->GetLiveEffectName(handle, &name);
        if (err) return err;
        if (name && std::strcmp(name, effectIdentity) == 0) {
            ai::int32 flags = 0;
            err = live->GetStyleFilterFlags(handle, &flags);
            if (err) return err;
            if (flags != regionFilterType) return kBadParameterErr;
            state.effect = handle;
            return kNoErr;
        }
    }
    AILiveEffectData data{};
    char title[] = "StrokeMaster PROPOSED closed region";
    data.self = message.self;
    data.name = effectIdentity;
    data.title = title;
    data.majorVersion = 1;
    data.minorVersion = 0;
    data.prefersAsInput = kGroupInputArt;
    data.styleFilterFlags = regionFilterType;
    AILiveEffectHandle handle = nullptr;
    err = live->AddLiveEffect(&data, &handle);
    if (err) return err;
    if (!handle) return kBadParameterErr;
    state.effect = handle;
    Trace("register.effect");
    return kNoErr;
}

ASErr Diagnostics(const State& state, char* output, size_t capacity) {
    int n = std::snprintf(output, capacity,
        "{\"schema\":2,\"plugin\":\"%s\",\"build\":\"%s\",\"processId\":%lu,\"effectRegistered\":%s,"
        "\"geometryEnabled\":true,\"goCalls\":%llu,\"mutations\":%llu,\"failedCalls\":%llu,"
        "\"lastGoError\":%d,\"gate\":\"%s\",\"maxVertexDeviation\":%.17g,\"fixtures\":%llu,"
        "\"sequence\":%llu,\"events\":[",
        regionPluginName, build, static_cast<unsigned long>(GetCurrentProcessId()),
        state.effect ? "true" : "false", state.metrics.goCalls, state.metrics.mutations,
        state.metrics.failedCalls, state.metrics.lastGoError, state.metrics.gate,
        state.metrics.maxVertexDeviation, state.fixtures, state.telemetry.sequence);
    if (n < 0 || static_cast<size_t>(n) >= capacity) return kBadParameterErr;
    size_t used = static_cast<size_t>(n);
    const auto first = state.telemetry.sequence > 32 ? state.telemetry.sequence - 31 : 1;
    for (auto seq = first; seq <= state.telemetry.sequence; ++seq) {
        const auto& e = state.telemetry.events[(seq - 1) % 32];
        n = std::snprintf(output + used, capacity - used,
            "%s{\"sequence\":%llu,\"stage\":\"%s\",\"detail\":\"%s\",\"error\":%ld}",
            seq == first ? "" : ",", e.sequence, e.stage, e.detail, static_cast<long>(e.error));
        if (n < 0 || static_cast<size_t>(n) >= capacity - used) return kBadParameterErr;
        used += static_cast<size_t>(n);
    }
    return std::snprintf(output + used, capacity - used, "]}") == 2 && capacity - used > 2 ? kNoErr : kBadParameterErr;
}

ASErr Payload(SPMessageData& message, State& state, const char* selector, bool emptyInput,
              ASErr actionError, char* output, size_t capacity) {
    const bool read = emptyInput && (std::strcmp(selector, "status") == 0 || std::strcmp(selector, "diagnostics") == 0);
    if (read) {
        Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
        if (live.error) return live.error;
        AILiveEffectHandle handle = nullptr;
        const auto err = live->GetLiveEffectHandleByName(effectIdentity, &handle);
        if (err) return err;
        if (handle && !state.effect) state.effect = handle;
        actionError = kNoErr;
    }
    const int n = std::snprintf(output, capacity,
        "{\"schema\":1,\"plugin\":\"%s\",\"build\":\"%s\",\"processId\":%lu,\"ok\":%s,"
        "\"effectRegistered\":%s,\"effect\":\"%s\",\"attachment\":\"POST_OBJECT_APPEND\","
        "\"geometryEnabled\":true,\"kernelAdmitted\":%s,\"goCalls\":%llu,\"mutations\":%llu,"
        "\"lastGoError\":%d,\"gate\":\"%s\",\"maxVertexDeviation\":%.17g,\"fixtures\":%llu,"
        "\"supportedGate\":\"MARKED_ANALYTIC_FIXTURE_ONLY\"}",
        regionPluginName, build, static_cast<unsigned long>(GetCurrentProcessId()),
        actionError ? "false" : "true", state.effect ? "true" : "false", effectIdentity,
        state.metrics.mutations > 0 ? "true" : "false",
        state.metrics.goCalls, state.metrics.mutations, state.metrics.lastGoError,
        state.metrics.gate, state.metrics.maxVertexDeviation, state.fixtures);
    return n < 0 || static_cast<size_t>(n) >= capacity ? kBadParameterErr : kNoErr;
}

ASErr Script(AIScriptMessage& message, State& state, const char* selector) {
    Suite<AIUnicodeStringSuite> unicode(message.d.basic, kAIUnicodeStringSuite, kAIUnicodeStringVersion);
    if (unicode.error) return unicode.error;
    char input[17]{};
    const auto length = unicode->Length(message.inParam);
    if (length > 16) return kBadParameterErr;
    for (ai::UnicodeString::size_type i = 0; i < length; ++i) {
        ai::UnicodeString::UTF32TextChar ch = 0;
        if (auto err = unicode->GetChar(message.inParam, i, ch)) return err;
        if (ch == 0 || ch > 127) return kBadParameterErr;
        input[i] = static_cast<char>(ch);
    }
    ASErr actionError = kBadParameterErr;
    if (std::strcmp(selector, "diagnostics") == 0 && input[0] == '\0') {
        char payload[16384]{};
        const auto err = Diagnostics(state, payload, sizeof(payload));
        if (err) return err;
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, payload,
            static_cast<ai::UnicodeString::offset_type>(std::strlen(payload)), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "status") == 0 && input[0] == '\0') actionError = kNoErr;
    else if (std::strcmp(selector, "create") == 0 &&
             (std::strcmp(input, "region") == 0 || std::strcmp(input, "control") == 0)) {
        const bool withEffect = std::strcmp(input, "region") == 0;
        actionError = WithNative(message.d, state, "create", [withEffect](Native& native) {
            return native.Create(withEffect);
        });
        if (!actionError) ++state.fixtures;
    }
    char payload[1024]{};
    auto err = Payload(message.d, state, selector, input[0] == '\0', actionError, payload, sizeof(payload));
    if (err) return err;
    if (actionError) Trace("bridge.action-failed", actionError, selector);
    if (auto err2 = unicode->Destroy(message.outParam)) return err2;
    return unicode->Initialize(message.outParam, payload,
        static_cast<ai::UnicodeString::offset_type>(std::strlen(payload)), kAIUTF8CharacterEncoding);
}
} // namespace region_probe

extern "C" ASAPI ASErr PluginMain(char* caller, char* selector, void* raw) {
    if (!caller || !selector || !raw) return kBadParameterErr;
    auto& message = *static_cast<SPMessageData*>(raw);
    if (!message.basic) return kBadParameterErr;
    try {
        if (std::strcmp(caller, kSPInterfaceCaller) == 0) {
            if (std::strcmp(selector, kSPInterfaceStartupSelector) == 0) {
                // Refuse unknown persistent state instead of interpreting
                // another build's globals layout.
                if (message.globals) return kBadParameterErr;
                static region_probe::State startupState{};
                region_probe::TelemetryScope telemetry(&startupState.telemetry);
                const auto err = region_probe::Register(message, startupState);
                if (err) return err;
                // Persisted globals are allocated only after registration succeeds;
                // a nested shutdown during registration finds no owned heap.
                if (!message.globals) {
                    void* storage = nullptr;
                    if (auto err2 = message.basic->AllocateBlock(sizeof(region_probe::State), &storage)) return err2;
                    if (!storage) return kOutOfMemoryErr;
                    message.globals = new (storage) region_probe::State{};
                    auto* state = static_cast<region_probe::State*>(message.globals);
                    state->effect = startupState.effect;
                    region_probe::Trace("globals.allocated");
                }
                return kNoErr;
            }
            if (std::strcmp(selector, kSPInterfaceShutdownSelector) == 0) {
                auto* state = static_cast<region_probe::State*>(message.globals);
                if (!state) return kNoErr;
                if (state->activeDepth) {
                    state->pendingFree = true;
                    message.globals = nullptr;
                    return kNoErr;
                }
                message.globals = nullptr;
                return message.basic->FreeBlock(state);
            }
            return kNoErr;
        }
        if (std::strcmp(caller, kSPAccessCaller) == 0) return kNoErr;
        auto* state = static_cast<region_probe::State*>(message.globals);
        if (!state) return kNoErr;
        ++state->activeDepth;
        const auto finish = [&](ASErr result) {
            if (--state->activeDepth == 0 && state->pendingFree) {
                state->pendingFree = false;
                message.globals = nullptr;
                const auto freed = message.basic->FreeBlock(state);
                if (freed) message.globals = state;
                return freed;
            }
            return result;
        };
        if (std::strcmp(caller, kCallerAIScriptMessage) == 0) {
            region_probe::TelemetryScope telemetry(&state->telemetry);
            return finish(region_probe::Script(*static_cast<AIScriptMessage*>(raw), *state, selector));
        }
        if (std::strcmp(caller, kCallerAILiveEffect) == 0) {
            if (std::strcmp(selector, kSelectorAIGoLiveEffect) == 0) {
                auto& go = *static_cast<AILiveEffectGoMessage*>(raw);
                if (go.effect != state->effect) return finish(kBadParameterErr);
                region_probe::TelemetryScope telemetry(&state->telemetry);
                auto& metrics = state->metrics;
                ++metrics.goCalls;
                metrics.gate = "ready";
                ASErr err = kBadParameterErr;
                if (!go.art) metrics.gate = "input.null";
                else if (!go.srcArt) metrics.gate = "source.null";
                else if (go.art == go.srcArt) metrics.gate = "input.source-alias";
                else err = region_probe::WithNative(go.d, *state, "go", [&go](region_probe::Native& native) {
                    return native.Process(go);
                });
                if (err) metrics.lastGoError = static_cast<int>(err);
                else ++metrics.mutations;
                metrics.failedCalls = metrics.goCalls - metrics.mutations;
                region_probe::Trace("callback.result", err, metrics.gate);
                return finish(err);
            }
            return finish(kNoErr);
        }
        return finish(kNoErr);
    } catch (...) {
        return kBadParameterErr;
    }
}
