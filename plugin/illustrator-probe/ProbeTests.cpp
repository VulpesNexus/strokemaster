// Original mock tests. No Adobe executable or suite implementation is loaded.
#include "Probe.cpp"
#include <array>
#include <cstdlib>
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
AIStrokeStyle incoming{};
FakeArt created;
constexpr ASErr failure = -1234;

void Check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
ASErr Mutate() { return ++mutations == failMutation ? failure : kNoErr; }
void Reset() {
    acquired.clear(); released.clear(); acquireCalls = failAcquire = 0;
    mutations = failMutation = entryReferences = dictionaryReferences = disposals = 0;
    strokeAvailable = true; failAttach = failConversion = false;
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
        released.emplace_back(name); return kNoErr;
    };
    basicMock.AllocateBlock = [](size_t size, void** out) -> SPErr {
        *out = std::malloc(size); if (*out) ++allocations;
        return *out ? kNoErr : kOutOfMemoryErr;
    };
    basicMock.FreeBlock = [](void* ptr) -> SPErr { std::free(ptr); --allocations; return kNoErr; };
    artMock.GetArtType = [](AIArtHandle art, short* type) -> AIErr { *type = Fake(art).type; return kNoErr; };
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
        const auto result = Mutate();
        if (!result) for (int i = 0; i < count; ++i) Fake(art).segments[at + i] = values[i];
        return result;
    };
    pathMock.SetPathClosed = [](AIArtHandle art, AIBoolean closed) -> AIErr {
        const auto result = Mutate(); if (!result) Fake(art).closed = closed; return result;
    };
    paintMock.GetPathStyle = [](AIArtHandle art, AIPathStyle* style, AIBoolean* advanced) -> AIErr {
        *style = Fake(art).style; *advanced = false; return kNoErr;
    };
    paintMock.SetPathStyle = [](AIArtHandle art, const AIPathStyle* style) -> AIErr {
        const auto result = Mutate(); if (!result) Fake(art).style = *style; return result;
    };
    paintMock.SetPathStyleForceSimple = paintMock.SetPathStyle;
    dictMock.Key = [](const char* name) -> AIDictKey { return reinterpret_cast<AIDictKey>(const_cast<char*>(name)); };
    dictMock.IsKnown = [](ConstAIDictionaryRef, AIDictKey) -> AIBoolean { return strokeAvailable; };
    dictMock.Get = [](ConstAIDictionaryRef, AIDictKey) -> AIEntryRef {
        ++entryReferences; return reinterpret_cast<AIEntryRef>(1);
    };
    dictMock.Release = [](AIDictionaryRef) -> ai::int32 { return --dictionaryReferences; };
    entryMock.ToStrokeStyle = [](AIEntryRef, AIStrokeStyle* style) -> AIErr {
        --entryReferences; *style = incoming; return failConversion ? failure : kNoErr;
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
        AILiveEffectParameters, AILiveEffectMergeAction action, AIArtStyleHandle* out) -> AIErr {
        Check(action == kReplaceStrokeWithLiveEffect, "attachment replaces stroke");
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
    for (int index = 0; index < 9; ++index) {
        Reset(); auto source = Line(), processing = Line(); auto message = Message(processing, source);
        if (index == 0) message.art = message.srcArt;
        if (index == 1) processing.closed = true;
        if (index == 2) processing.segments[0].out.h += 1;
        if (index == 3) processing.type = kCompoundPathArt;
        if (index == 4) processing.attributes = kArtIsClipMask;
        if (index == 5) strokeAvailable = false;
        if (index == 6) incoming.width = -1;
        if (index == 7) incoming.dash.length = 1;
        if (index == 8) failConversion = true;
        Check(Go(message) != kNoErr && mutations == 0, "unsupported input rejected without mutation"); Balanced();
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
    std::cout << "PASS: Pascal filter strings, lifecycle, 5 suite failures, 4 rollback failures, 9 rejection cases, wrapper geometry, 2 fixture cleanup cases.\n"
              << "PASS: bridge protocol identity, read-only status, invalid input, control/effect fixtures, callback/mutation counters.\n"
              << "Mock-only: no Illustrator load, dispatch, undo, or rendering verified.\n";
    return 0;
}
