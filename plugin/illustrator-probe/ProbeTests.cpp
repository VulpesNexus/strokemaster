// Original mock tests. No Adobe executable or suite implementation is loaded.
#include "Probe.cpp"
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct FakeArt {
    short type = kPathArt;
    ai::int16 count = 2;
    AIBoolean closed = false;
    AIPathStyle style{};
    std::array<AIPathSegment, 4> segments{};
    AIArtHandle child = nullptr, sibling = nullptr;
    ai::int32 attributes = 0;
};
AIArtHandle Handle(FakeArt& art) { return reinterpret_cast<AIArtHandle>(&art); }
FakeArt& Fake(AIArtHandle art) { return *reinterpret_cast<FakeArt*>(art); }
AIArtSuite artMock{};
AIPathSuite pathMock{};
AIPathStyleSuite paintMock{};
AIDictionarySuite dictMock{};
AIEntrySuite entryMock{};
AILiveEffectSuite liveMock{};
AIFilterSuite filterMock{};
AIArtStyleSuite stylesMock{};
AIDocumentSuite docMock{};
SPBasicSuite basicMock{};
std::vector<std::string> acquired, released;
int acquireCalls = 0, failAcquire = 0, mutations = 0, failMutation = 0;
int entryReferences = 0, dictionaryReferences = 0, disposals = 0;
int liveRegistrations = 0, commandRegistrations = 0, allocations = 0;
bool strokeAvailable = true, failAttach = false, failConversion = false;
bool advancedFillMock = false;
bool failStrokeEntry = false, failStrokeSet = false, persistedStroke = false, requirePersistedStroke = false;
bool normalizeFixtureStroke = false;
int mergeCalls = 0;
AIStrokeStyle capturedStroke{};
AIStrokeStyle incoming{};
FakeArt created;
constexpr ASErr failure = -1234;
std::function<void(const char*)> sdkHook;
bool failFree = false;
int freeCalls = 0;
State freedState{};

void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
ASErr Mutate() { return ++mutations == failMutation ? failure : kNoErr; }
void Reset() {
    acquired.clear(); released.clear(); acquireCalls = failAcquire = 0;
    mutations = failMutation = entryReferences = dictionaryReferences = disposals = 0;
    strokeAvailable = true; failAttach = failConversion = false;
    advancedFillMock = false;
    failStrokeEntry = failStrokeSet = persistedStroke = requirePersistedStroke = false; mergeCalls = 0;
    normalizeFixtureStroke = false;
    sdkHook = {}; failFree = false;
    incoming.Init(); incoming.width = 8; incoming.cap = kAIButtCap;
    incoming.color.kind = kGrayColor; incoming.color.c.g.gray = 0.75;
}
FakeArt Line() {
    FakeArt art;
    art.style.Init(); art.style.strokePaint = true; art.style.stroke = incoming;
    art.segments[0] = Segment(100, 100); art.segments[1] = Segment(220, 100);
    return art;
}
void Balanced() {
    Check(acquired.size() == released.size(), "all acquired suites released");
    for (size_t i = 0; i < acquired.size(); ++i)
        Check(acquired[i] == released[released.size() - 1 - i], "reverse suite release order");
    Check(entryReferences == 0, "entry conversion consumes exactly one reference");
    Check(dictionaryReferences == 0, "owned parameter dictionary released");
}
AILiveEffectGoMessage Message(FakeArt& processing, FakeArt& source) {
    AILiveEffectGoMessage result{};
    result.d.basic = &basicMock;
    result.art = Handle(processing); result.srcArt = Handle(source);
    result.parameters = reinterpret_cast<AILiveEffectParameters>(1);
    return result;
}
void Setup() {
    basicMock.AcquireSuite = [](const char* name, ai::int32, const void** out) -> SPErr {
        if (sdkHook) sdkHook("acquire");
        *out = nullptr;
        if (++acquireCalls == failAcquire) return failure;
        if (std::strcmp(name, kAIArtSuite) == 0) *out = &artMock;
        else if (std::strcmp(name, kAIPathSuite) == 0) *out = &pathMock;
        else if (std::strcmp(name, kAIPathStyleSuite) == 0) *out = &paintMock;
        else if (std::strcmp(name, kAIDictionarySuite) == 0) *out = &dictMock;
        else if (std::strcmp(name, kAIEntrySuite) == 0) *out = &entryMock;
        else if (std::strcmp(name, kAILiveEffectSuite) == 0) *out = &liveMock;
        else if (std::strcmp(name, kAIFilterSuite) == 0) *out = &filterMock;
        else if (std::strcmp(name, kAIArtStyleSuite) == 0) *out = &stylesMock;
        else if (std::strcmp(name, kAIDocumentSuite) == 0) *out = &docMock;
        else return failure;
        acquired.emplace_back(name); return kNoErr;
    };
    basicMock.ReleaseSuite = [](const char* name, ai::int32) -> SPErr {
        if (sdkHook) sdkHook("release");
        released.emplace_back(name); return kNoErr;
    };
    basicMock.AllocateBlock = [](size_t size, void** out) -> SPErr {
        *out = std::malloc(size); if (*out) ++allocations;
        return *out ? kNoErr : kOutOfMemoryErr;
    };
    basicMock.FreeBlock = [](void* ptr) -> SPErr {
        ++freeCalls;
        Check(currentTelemetry != &static_cast<State*>(ptr)->telemetry, "free occurs after telemetry unwinds");
        if (sdkHook) sdkHook("free");
        if (failFree) return failure;
        freedState = *static_cast<State*>(ptr);
        std::memset(ptr, 0xdd, sizeof(State)); // Deterministic poison before release.
        std::free(ptr); --allocations; return kNoErr;
    };
    artMock.GetArtType = [](AIArtHandle art, short* type) -> AIErr {
        if (sdkHook) sdkHook("read-art");
        *type = Fake(art).type; return kNoErr;
    };
    artMock.GetArtUserAttr = [](AIArtHandle art, ai::int32 which, ai::int32* attr) -> AIErr {
        *attr = Fake(art).attributes & which; return kNoErr;
    };
    artMock.GetArtFirstChild = [](AIArtHandle art, AIArtHandle* child) -> AIErr { *child = Fake(art).child; return kNoErr; };
    artMock.GetArtSibling = [](AIArtHandle art, AIArtHandle* sibling) -> AIErr { *sibling = Fake(art).sibling; return kNoErr; };
    artMock.NewArt = [](ai::int16, ai::int16, AIArtHandle, AIArtHandle* out) -> AIErr {
        created = Line(); *out = Handle(created); return kNoErr;
    };
    artMock.DisposeArt = [](AIArtHandle art) -> AIErr {
        Check(art == Handle(created), "only owned fixture is disposed"); ++disposals; return kNoErr;
    };
    pathMock.GetPathSegmentCount = [](AIArtHandle art, ai::int16* count) -> AIErr { *count = Fake(art).count; return kNoErr; };
    pathMock.GetPathClosed = [](AIArtHandle art, AIBoolean* closed) -> AIErr { *closed = Fake(art).closed; return kNoErr; };
    pathMock.GetPathSegments = [](AIArtHandle art, ai::int16 at, ai::int16 count, AIPathSegment* out) -> AIErr {
        for (int i = 0; i < count; ++i) out[i] = Fake(art).segments[at + i]; return kNoErr;
    };
    pathMock.SetPathSegmentCount = [](AIArtHandle art, ai::int16 count) -> AIErr {
        const auto result = Mutate(); if (!result) Fake(art).count = count; return result;
    };
    pathMock.SetPathSegments = [](AIArtHandle art, ai::int16 at, ai::int16 count, const AIPathSegment* values) -> AIErr {
        if (sdkHook) sdkHook("write-art");
        const auto result = Mutate();
        if (!result) for (int i = 0; i < count; ++i) Fake(art).segments[at + i] = values[i];
        return result;
    };
    pathMock.SetPathClosed = [](AIArtHandle art, AIBoolean closed) -> AIErr {
        const auto result = Mutate(); if (!result) Fake(art).closed = closed; return result;
    };
    paintMock.GetPathStyle = [](AIArtHandle art, AIPathStyle* style, AIBoolean* advanced) -> AIErr {
        *style = Fake(art).style; *advanced = advancedFillMock; return kNoErr;
    };
    paintMock.SetPathStyle = [](AIArtHandle art, const AIPathStyle* style) -> AIErr {
        const auto result = Mutate(); if (!result) Fake(art).style = *style; return result;
    };
    paintMock.SetPathStyleForceSimple = [](AIArtHandle art, const AIPathStyle* style) -> AIErr {
        auto applied = *style;
        if (normalizeFixtureStroke) applied.stroke.width = 12;
        return paintMock.SetPathStyle(art, &applied);
    };
    dictMock.Key = [](const char* name) -> AIDictKey { return reinterpret_cast<AIDictKey>(const_cast<char*>(name)); };
    dictMock.IsKnown = [](ConstAIDictionaryRef source, AIDictKey key) -> AIBoolean {
        if (requirePersistedStroke) return source == reinterpret_cast<ConstAIDictionaryRef>(1) && persistedStroke &&
            std::strcmp(reinterpret_cast<const char*>(key), kStrokeStyleKey) == 0;
        return strokeAvailable;
    };
    dictMock.Get = [](ConstAIDictionaryRef, AIDictKey) -> AIEntryRef {
        ++entryReferences; return reinterpret_cast<AIEntryRef>(1);
    };
    dictMock.Release = [](AIDictionaryRef) -> ai::int32 { return --dictionaryReferences; };
    entryMock.FromStrokeStyle = [](const AIStrokeStyle* stroke) -> AIEntryRef {
        if (failStrokeEntry) return nullptr;
        capturedStroke = *stroke; ++entryReferences; return reinterpret_cast<AIEntryRef>(2);
    };
    dictMock.Set = [](AIDictionaryRef destination, AIDictKey key, AIEntryRef value) -> AIErr {
        Check(destination == reinterpret_cast<AIDictionaryRef>(1), "stroke stored only in owned parameters");
        Check(std::strcmp(reinterpret_cast<const char*>(key), kStrokeStyleKey) == 0, "documented persistent stroke key");
        Check(value == reinterpret_cast<AIEntryRef>(2), "Set receives typed captured stroke entry");
        --entryReferences; // SDK Set consumes caller reference, including on error.
        if (failStrokeSet) return failure;
        persistedStroke = true; return kNoErr;
    };
    entryMock.ToStrokeStyle = [](AIEntryRef, AIStrokeStyle* style) -> AIErr {
        --entryReferences; *style = requirePersistedStroke ? capturedStroke : incoming;
        return failConversion ? failure : kNoErr;
    };
    liveMock.AddLiveEffect = [](AILiveEffectData* data, AILiveEffectHandle* out) -> AIErr {
        Check(data->styleFilterFlags == kStrokeFilter, "registration is stroke replacement only");
        ++liveRegistrations; *out = reinterpret_cast<AILiveEffectHandle>(1); return kNoErr;
    };
    filterMock.AddFilter = [](SPPluginRef, char*, PlatformAddFilterData* menu, ai::int32, AIFilterHandle* out) -> AIErr {
        constexpr char category[] = "StrokeMaster PROPOSED";
        constexpr char title[] = "Create diagnostic line";
        Check(menu && menu->category && menu->title, "filter strings exist");
        // Check one-byte lengths before any payload reads. Never use strlen on Pascal strings.
        Check(menu->category[0] == sizeof(category) - 1, "Pascal category length matches payload");
        Check(menu->title[0] == sizeof(title) - 1, "Pascal title length matches payload");
        Check(std::memcmp(menu->category + 1, category, sizeof(category) - 1) == 0, "Pascal category payload");
        Check(std::memcmp(menu->title + 1, title, sizeof(title) - 1) == 0, "Pascal title payload");
        ++commandRegistrations; *out = reinterpret_cast<AIFilterHandle>(2); return kNoErr;
    };
    liveMock.CreateLiveEffectParameters = [](AILiveEffectParameters* out) -> AIErr {
        ++dictionaryReferences; *out = reinterpret_cast<AILiveEffectParameters>(1); return kNoErr;
    };
    liveMock.NewArtStyleByMergingLiveEffect = [](AIArtStyleHandle, AILiveEffectHandle,
        AILiveEffectParameters parameters, AILiveEffectMergeAction action, AIArtStyleHandle* out) -> AIErr {
        Check(action == kReplaceStrokeWithLiveEffect, "attachment replaces stroke");
        Check(parameters == reinterpret_cast<AILiveEffectParameters>(1) && persistedStroke,
            "merge receives captured stroke parameters, no implicit population assumed");
        Check(capturedStroke.width == created.style.stroke.width && capturedStroke.cap == created.style.stroke.cap &&
            capturedStroke.color.kind == created.style.stroke.color.kind && capturedStroke.color.c.g.gray == created.style.stroke.color.c.g.gray,
            "parameter captures exact native style before merge");
        ++mergeCalls;
        *out = reinterpret_cast<AIArtStyleHandle>(2); return kNoErr;
    };
    stylesMock.GetArtStyle = [](AIArtHandle, AIArtStyleHandle* out) -> AIErr {
        *out = reinterpret_cast<AIArtStyleHandle>(1); return kNoErr;
    };
    stylesMock.SetArtStyle = [](AIArtHandle art, AIArtStyleHandle) -> AIErr {
        Check(art == Handle(created), "attachment touches only owned fixture"); return failAttach ? failure : kNoErr;
    };
    docMock.GetDocument = [](AIDocumentHandle* out) -> AIErr {
        *out = reinterpret_cast<AIDocumentHandle>(1); return kNoErr;
    };
}
} // namespace

int main() {
    Setup(); Reset();
    {
        constexpr auto empty = Pascal("");
        constexpr auto text = Pascal("abc");
        static_assert(empty.size() == 1 && empty[0] == 0, "empty Pascal extent");
        static_assert(text.size() == 4 && text[0] == 3 && text[3] == 'c', "Pascal exact extent");
        // Compile-time constructor bounds are exercised above; actual filter payloads
        // are validated by AddFilter before reading their exact known extents.
    }
    {
        SPMessageData message{}; message.basic = &basicMock;
        Check(Startup(message) == kNoErr, "startup"); Balanced();
        Reset(); Check(Startup(message) == kNoErr, "idempotent startup"); Balanced();
        Check(liveRegistrations == 1 && commandRegistrations == 1, "no duplicate registrations");
        Check(mutations == 0 && disposals == 0, "startup does not edit art");
        char caller[] = kSPInterfaceCaller, selector[] = kSPInterfaceShutdownSelector;
        Check(PluginMain(caller, selector, &message) == kNoErr && !message.globals && allocations == 0, "shutdown frees POD state");
    }
    for (int index = 1; index <= 5; ++index) {
        Reset(); auto source = Line(), processing = Line(); auto message = Message(processing, source);
        failAcquire = index;
        Check(Go(message) == failure && mutations == 0, "partial acquisition failure cannot mutate art"); Balanced();
    }
    for (int index = 1; index <= 4; ++index) {
        Reset(); auto source = Line(), processing = Line(); auto message = Message(processing, source);
        failMutation = index;
        Check(Go(message) == failure, "mutation error is propagated");
        Check(processing.count == 2 && !processing.closed && processing.style.strokePaint && !processing.style.fillPaint,
            "rollback restores count, closure, and paint");
        Check(SamePoint(processing.segments[0].p, source.segments[0].p) &&
              SamePoint(processing.segments[1].p, source.segments[1].p), "rollback restores geometry");
        Check(source.count == 2 && disposals == 0, "Go never deletes or changes source"); Balanced();
    }
    const char* rejection[] = {"input.source-alias", "path.closed", "path.curve-handles",
        "leaf.unsupported-type", "leaf.clip-mask", "stroke.keys-missing", "stroke.width",
        "stroke.dash", "stroke.convert", "style.fill-painted", "style.advanced-fill", "style.clip"};
    for (int index = 0; index < 12; ++index) {
        Reset(); auto source = Line(), processing = Line(); auto message = Message(processing, source);
        State state{}; TelemetryScope scope(&state.telemetry);
        if (index == 0) message.art = message.srcArt;
        if (index == 1) processing.closed = true;
        if (index == 2) processing.segments[0].out.h += 1;
        if (index == 3) processing.type = kCompoundPathArt;
        if (index == 4) processing.attributes = kArtIsClipMask;
        if (index == 5) strokeAvailable = false;
        if (index == 6) incoming.width = -1;
        if (index == 7) incoming.dash.length = 1;
        if (index == 8) failConversion = true;
        if (index == 9) processing.style.fillPaint = true;
        if (index == 10) advancedFillMock = true;
        if (index == 11) processing.style.clip = true;
        Check(CountedGo(message, state) != kNoErr && mutations == 0, "unsupported input rejected without mutation");
        char payload[16384]{};
        Check(Diagnostics(state, payload, sizeof(payload)) == kNoErr && std::strstr(payload, rejection[index]),
            "failed callback identifies its rejecting gate after suite cleanup");
        Check(state.acceptedInputs == 0 && state.mutations == 0 && source.count == 2,
            "rejection telemetry never accepts or mutates source"); Balanced();
    }
    Reset(); {
        auto source = Line(), processing = Line(), group = Line(); group.type = kGroupArt; group.child = Handle(processing);
        auto message = Message(group, source); const auto root = message.art;
        Check(Go(message) == kNoErr, "single-child processing wrapper accepted");
        Check(message.art == root && source.count == 2 && processing.count == 4 && processing.closed,
            "root and source preserved; processing leaf replaced");
        Check(processing.style.fillPaint && !processing.style.strokePaint && processing.style.fill.color.c.g.gray == 0.75,
            "output is filled stroke color, no second stroke");
        Check(processing.segments[0].p.v == 104 && processing.segments[2].p.v == 96,
            "8 point butt rectangle has exact expected sides"); Balanced();
    }
    for (int index = 0; index < 2; ++index) {
        Reset(); SPMessageData message{}; message.basic = &basicMock;
        State state{reinterpret_cast<AILiveEffectHandle>(1), nullptr}; failAttach = index != 0;
        Check(Diagnostic(message, state) == (failAttach ? failure : kNoErr), "diagnostic attachment result");
        Check(disposals == (failAttach ? 1 : 0), "fixture retained only on success"); Balanced();
    }
    for (int index = 0; index < 4; ++index) {
        Reset(); SPMessageData message{}; message.basic = &basicMock;
        State state{reinterpret_cast<AILiveEffectHandle>(1), nullptr};
        if (index == 0) failAcquire = 8; // Entry suite is needed only for the effect attachment.
        if (index == 1) failStrokeEntry = true;
        if (index == 2) failStrokeSet = true;
        if (index == 3) advancedFillMock = true;
        Check(Diagnostic(message, state) != kNoErr && disposals == 1 && mergeCalls == 0,
            "attachment parameter failure disposes owned fixture before any merge"); Balanced();
    }
    Reset(); {
        SPMessageData header{}; header.basic = &basicMock;
        State state{reinterpret_cast<AILiveEffectHandle>(1), nullptr};
        normalizeFixtureStroke = true;
        Check(Diagnostic(header, state) == kNoErr && capturedStroke.width == 12 && mergeCalls == 1,
            "attachment stores host-read stroke, not the intended hard-coded width"); Balanced();
        acquired.clear(); released.clear();
        mutations = 0; incoming.width = 99; requirePersistedStroke = true;
        auto source = Line(), processing = Line(); processing.style.Init();
        auto message = Message(processing, source);
        message.instanceInfo = reinterpret_cast<AILiveEffectParameters>(2); // Both instance keys absent.
        Check(CountedGo(message, state) == kNoErr && state.strokeOrigin == 3 && state.mutations == 1,
            "Go uses captured parameters when instance keys and processing stroke are absent");
        Check(processing.segments[0].p.v == 106 && processing.segments[2].p.v == 94 && source.count == 2,
            "exact captured native width drives output while editable source stays intact"); Balanced();
        acquired.clear(); released.clear();
        processing = Line(); processing.style.Init(); persistedStroke = false;
        mutations = 0;
        Check(CountedGo(message, state) == kBadParameterErr && mutations == 0 && state.strokeOrigin == 0,
            "missing persisted key still rejects; no source or processing-style fallback"); Balanced();
    }
    Reset(); {
        State state{reinterpret_cast<AILiveEffectHandle>(1), nullptr};
        SPMessageData header{}; header.basic = &basicMock;
        char payload[512]{};
        Check(Bridge(header, state, "status", "", payload, sizeof(payload)) == kNoErr && mutations == 0 && acquireCalls == 0,
            "status protocol has no art or suite actions");
        Check(std::strstr(payload, "\"plugin\":\"StrokeMasterProposedProbe\"") &&
              std::strstr(payload, "\"goCalls\":0") && std::strstr(payload, "\"ok\":true"), "status identity and counters");
        Check(Bridge(header, state, "create", "wrong", payload, sizeof(payload)) == kNoErr &&
              std::strstr(payload, "\"ok\":false") && mutations == 0, "invalid protocol input cannot create art");
        Check(Bridge(header, state, "create", "control", payload, sizeof(payload)) == kNoErr && state.controlFixtures == 1 &&
              state.effectFixtures == 0 && state.goCalls == 0 && dictionaryReferences == 0, "ordinary control has no effect callback");
        Check(created.segments[0].p.v == 140 && created.style.strokePaint, "control is native line at separate position");
        Balanced(); Reset();
        Check(Bridge(header, state, "create", "effect", payload, sizeof(payload)) == kNoErr && state.effectFixtures == 1,
            "effect fixture is explicitly requested"); Balanced(); Reset();
        auto source = Line(), processing = Line(); auto message = Message(processing, source);
        Check(CountedGo(message, state) == kNoErr && state.goCalls == 1 && state.mutations == 1 && state.strokeOrigin == 3,
            "callback and successful mutation counters are distinct and capture style origin"); Balanced(); Reset();
        incoming.width = -1; processing = Line();
        Check(CountedGo(message, state) != kNoErr && state.goCalls == 2 && state.mutations == 1 && state.lastGoError != kNoErr,
            "failed callback increments only call counter"); Balanced();
    }
    Reset(); {
        SPMessageData header{}; header.basic = &basicMock;
        failAcquire = 1;
        Check(Startup(header) == failure && header.globals, "startup failure keeps diagnostic state for shutdown");
        auto& state = *static_cast<State*>(header.globals);
        char payload[16384]{};
        char processField[64]{};
        std::snprintf(processField, sizeof(processField), "\"processId\":%lu,", static_cast<unsigned long>(GetCurrentProcessId()));
        Check(Diagnostics(state, payload, sizeof(payload)) == kNoErr &&
            std::strstr(payload, "suite.acquire") && std::strstr(payload, "AI Live Effect Suite") &&
            std::strstr(payload, "\"error\":-1234"), "startup failure identifies exact suite and error");
        Check(std::strstr(payload, "\"schema\":2,") && std::strstr(payload, processField),
            "diagnostics schema 2 reports actual mock-executable process ID, not stored or caller-supplied identity");
        Balanced(); Reset();
        Check(Startup(header) == kNoErr, "retry partial startup uses same diagnostic state"); Balanced();
        {
            TelemetryScope scope(&state.telemetry);
            for (int i = 0; i < 40; ++i) Trace("ring", kNoErr, "quoted\"\\label");
            Check(Diagnostics(state, payload, sizeof(payload)) == kNoErr &&
                std::strstr(payload, "quoted__label"), "bounded diagnostic labels remain JSON-safe");
            size_t events = 0;
            for (const char* p = payload; (p = std::strstr(p, "\"stage\":")); ++p) ++events;
            Check(events == 32, "telemetry retains only the latest 32 events");
            Check(Diagnostics(state, payload, 8) != kNoErr, "truncated telemetry rejected");
        }
        char caller[] = kSPInterfaceCaller, selector[] = kSPInterfaceShutdownSelector;
        Check(PluginMain(caller, selector, &header) == kNoErr && !header.globals && !currentTelemetry,
            "shutdown does not log through freed telemetry storage");
    }
    // Enter through PluginMain; the shutdown is injected by an SDK function
    // while CountedGo still holds State& and Go still has suite guards alive.
    for (int scenario = 0; scenario < 7; ++scenario) {
        Reset();
        SPMessageData startup{}; startup.basic = &basicMock;
        char interfaceCaller[] = kSPInterfaceCaller, startSelector[] = kSPInterfaceStartupSelector;
        char stopSelector[] = kSPInterfaceShutdownSelector;
        char goCaller[] = kCallerAILiveEffect, goSelector[] = kSelectorAIGoLiveEffect;
        Check(PluginMain(interfaceCaller, startSelector, &startup) == kNoErr, "nested test startup");
        Balanced(); acquired.clear(); released.clear();
        auto* liveState = static_cast<State*>(startup.globals);
        auto source = Line(), processing = Line();
        auto go = Message(processing, source); go.d.globals = liveState; go.effect = liveState->effect;
        const int freesBefore = freeCalls;
        bool injected = false, insideNested = false, freeReentry = false;
        const char* injection = scenario == 0 ? "acquire" : scenario == 1 ? "write-art" :
            scenario == 2 ? "release" : "read-art";
        if (scenario == 3) incoming.width = -1;
        sdkHook = [&](const char* stage) {
            if (std::strcmp(stage, "free") == 0) {
                Check(injected && !currentTelemetry, "all callback telemetry scopes unwind before final free");
                // PICA may still have the old globals until the outer call returns.
                SPMessageData stale{}; stale.basic = &basicMock; stale.globals = liveState;
                Check(PluginMain(interfaceCaller, stopSelector, &stale) == kNoErr && !stale.globals,
                    "reentrant duplicate shutdown during FreeBlock is harmless");
                stale.globals = liveState;
                Check(PluginMain(interfaceCaller, startSelector, &stale) == kBadParameterErr && !stale.globals,
                    "retiring epoch refuses restart even with stale globals");
                freeReentry = true;
                return;
            }
            if (injected || std::strcmp(stage, injection) != 0) return;
            if (scenario == 4 && !insideNested) {
                insideNested = true;
                auto nestedProcessing = Line(); auto nestedGo = Message(nestedProcessing, source);
                nestedGo.d.globals = liveState; nestedGo.effect = liveState->effect;
                Check(PluginMain(goCaller, goSelector, &nestedGo) == kNoErr && !nestedGo.d.globals,
                    "inner Go completes after its nested shutdown");
                Check(allocations == 1 && freeCalls == freesBefore,
                    "inner completion cannot release state pinned by outer Go");
                return;
            }
            injected = true;
            SPMessageData stop{}; stop.basic = &basicMock; stop.globals = liveState;
            Check(PluginMain(interfaceCaller, stopSelector, &stop) == kNoErr && !stop.globals,
                "nested shutdown retires its globals");
            Check(!go.d.globals && allocations == 1 && freeCalls == freesBefore,
                "outer globals cleared immediately but referenced state remains allocated");
            Check(currentTelemetry == &liveState->telemetry,
                "nested telemetry restores a still-live outer state");
            auto refused = Message(processing, source); refused.d.globals = liveState; refused.effect = liveState->effect;
            const int before = acquireCalls;
            Check(PluginMain(goCaller, goSelector, &refused) == kBadParameterErr && !refused.d.globals && acquireCalls == before,
                "new callback refused during retirement without acquiring SDK suites");
            if (scenario == 5) throw 7; // Outer catch must still unpin once.
            if (scenario == 6) failFree = true;
        };
        const auto expected = scenario == 3 || scenario == 5 ? kBadParameterErr : scenario == 6 ? failure : kNoErr;
        Check(PluginMain(goCaller, goSelector, &go) == expected, "outer completion result after nested shutdown");
        Check(injected && freeReentry && !currentTelemetry && !currentInvocation && freeCalls == freesBefore + 1,
            "exactly one final free attempt after outer callback and suite cleanup");
        if (scenario == 6) {
            Check(go.d.globals == liveState && allocations == 1, "failed free retains ownership for retry");
            sdkHook = {}; failFree = false;
            Check(PluginMain(interfaceCaller, stopSelector, &go.d) == kNoErr && !go.d.globals,
                "later shutdown releases retained block");
        }
        Check(!go.d.globals && allocations == 0, "no freed globals restored on outer return");
        if (scenario != 5) {
            Check(freedState.goCalls == (scenario == 4 ? 2u : 1u) &&
                freedState.mutations == (scenario == 3 ? 0u : scenario == 4 ? 2u : 1u),
                "CountedGo completed its state writes before release");
        }
        Check(source.count == 2 && !source.closed && disposals == 0, "nested shutdown preserves editable source");
        // Nested Go release order is per invocation, not globally reversed.
        if (scenario != 4) Balanced();
        else Check(acquired.size() == released.size() && entryReferences == 0, "nested suite acquisitions balanced");
        sdkHook = {};
    }
    for (const char* trigger : {"acquire", "release"}) {
        Reset();
        SPMessageData start{}; start.basic = &basicMock;
        char caller[] = kSPInterfaceCaller, startSelector[] = kSPInterfaceStartupSelector;
        char stopSelector[] = kSPInterfaceShutdownSelector;
        bool injected = false;
        const int freesBefore = freeCalls;
        sdkHook = [&](const char* stage) {
            if (injected || std::strcmp(stage, trigger) != 0) return;
            injected = true;
            SPMessageData stop{}; stop.basic = &basicMock; stop.globals = start.globals;
            Check(stop.globals && PluginMain(caller, stopSelector, &stop) == kNoErr,
                "new startup state is pinned before its first SDK suite call");
            Check(!start.globals && !stop.globals && allocations == 1 && freeCalls == freesBefore,
                "nested startup shutdown keeps local references alive until startup unwinds");
        };
        Check(PluginMain(caller, startSelector, &start) == kNoErr && injected && !start.globals &&
            allocations == 0 && freeCalls == freesBefore + 1 && !currentTelemetry && !currentInvocation,
            "startup callback and suite cleanup complete before final release");
        Check(mutations == 0 && disposals == 0, "retired startup never touches art");
        Balanced(); sdkHook = {};
    }
    std::cout << "PASS: 9 nested shutdown scenarios through SDK calls, nested Go/startup, rejected reentry, exception unwind, free failure/retry.\n"
              << "PASS: Pascal filter strings, lifecycle, 5 suite failures, 4 rollback failures, 12 named rejection cases, wrapper geometry, 2 fixture cleanup cases.\n"
              << "PASS: bridge protocol identity, read-only status, invalid input, control/effect fixtures, callback/mutation counters.\n"
              << "PASS: captured native stroke parameter, 4 attachment failure cleanups, absent-instance Go and missing-key rejection.\n"
              << "Mock-only: no Illustrator load, dispatch, undo, or rendering verified.\n";
    return 0;
}
