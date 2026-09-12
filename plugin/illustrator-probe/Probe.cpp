// Original diagnostic implementation. PROPOSED: never loaded in Illustrator.
// SDK declarations are external dependencies, not vendored sources.
#include "SPBasic.h"
#include "SPInterf.h"
#include "SPAccess.h"
#include "AIArt.h"
#include "AIPath.h"
#include "AIPathStyle.h"
#include "AILiveEffect.h"
#include "AIArtStyle.h"
#include "AIDictionary.h"
#include "AIEntry.h"
#include "AIFilter.h"
#include "AIDocument.h"
#include "AIScriptMessage.h"
#include "AIUnicodeString.h"
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <new>
#ifdef PROBE_TRACE
#include <Windows.h>
#endif

namespace {
constexpr const char* kIdentity = "org.strokemaster.proposed.line-replacement.v1";
constexpr const char* kPluginName = "StrokeMasterProposedProbe";

template<size_t N> constexpr std::array<unsigned char, N> Pascal(const char (&text)[N]) {
    static_assert(N > 0 && N <= 256, "Pascal payload must fit one byte");
    std::array<unsigned char, N> result{};
    result[0] = static_cast<unsigned char>(N - 1);
    for (size_t i = 1; i < N; ++i) result[i] = static_cast<unsigned char>(text[i - 1]);
    return result;
}

void Trace(const char* stage, ASErr error = kNoErr) noexcept {
#ifdef PROBE_TRACE
    char line[160]{};
    std::snprintf(line, sizeof(line), "StrokeMaster PROPOSED: %s error=%ld\n", stage, static_cast<long>(error));
    OutputDebugStringA(line);
#else
    (void)stage;
    (void)error;
#endif
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
    }
    ~Suite() { if (value_) Trace("release-suite", basic_->ReleaseSuite(name_, version_)); }
    Suite(const Suite&) = delete;
    Suite& operator=(const Suite&) = delete;
    const T* operator->() const { return value_; }
};

// Allocated through PICA, with only opaque host handles across binary reloads.
struct State {
    AILiveEffectHandle effect;
    AIFilterHandle command;
    unsigned long long goCalls = 0, mutations = 0, effectFixtures = 0, controlFixtures = 0;
    int strokeOrigin = 0; // 0 missing, 1 transformed instance, 2 instance, 3 parameters
    ASErr lastGoError = kNoErr;
};

#define REQUIRE(call) do { const ASErr checked = (call); if (checked) return checked; } while (false)

ASErr Startup(SPMessageData& message) {
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIFilterSuite> filter(message.basic, kAIFilterSuite, kAIFilterVersion);
    REQUIRE(live.error);
    REQUIRE(filter.error);
    if (!message.globals) {
        void* storage = nullptr;
        REQUIRE(message.basic->AllocateBlock(sizeof(State), &storage));
        if (!storage) return kOutOfMemoryErr;
        message.globals = new (storage) State{};
    }
    auto& state = *static_cast<State*>(message.globals);
    if (!state.effect) {
        AILiveEffectData data{};
        char title[] = "StrokeMaster PROPOSED line replacement";
        data.self = message.self;
        data.name = kIdentity;
        data.title = title;
        data.majorVersion = 1;
        data.minorVersion = 0;
        data.prefersAsInput = kPathInputArt | kGroupInputArt;
        data.styleFilterFlags = kStrokeFilter;
        REQUIRE(live->AddLiveEffect(&data, &state.effect));
        if (!state.effect) return kBadParameterErr;
    }
    if (!state.command) {
        PlatformAddFilterData menu{};
        auto category = Pascal("StrokeMaster PROPOSED");
        auto title = Pascal("Create diagnostic line");
        char name[] = "StrokeMaster PROPOSED create line";
        menu.category = category.data();
        menu.title = title.data();
        REQUIRE(filter->AddFilter(message.self, name, &menu, 0, &state.command));
    }
    return kNoErr;
}

AIPathSegment Segment(AIReal x, AIReal y) {
    AIPathSegment result{};
    result.p.h = x;
    result.p.v = y;
    result.in = result.out = result.p;
    result.corner = true;
    return result;
}

// Only the explicit ordinary-filter command edits the document tree.
// Selection is ignored. The command never closes, saves, or creates a document.
ASErr Diagnostic(SPMessageData& message, const State& state, bool withEffect = true) {
    Suite<AIDocumentSuite> doc(message.basic, kAIDocumentSuite, kAIDocumentVersion);
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(message.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIPathStyleSuite> paint(message.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    REQUIRE(doc.error); REQUIRE(art.error); REQUIRE(path.error); REQUIRE(paint.error);
    REQUIRE(styles.error); REQUIRE(live.error); REQUIRE(dict.error);
    AIDocumentHandle document = nullptr;
    REQUIRE(doc->GetDocument(&document));
    if (!document || !state.effect) return kBadParameterErr;
    AIArtHandle fixture = nullptr;
    REQUIRE(art->NewArt(kPathArt, kPlaceAboveAll, nullptr, &fixture));
    if (!fixture) return kBadParameterErr;
    AILiveEffectParameters parameters = nullptr;
    // Cleanup guards outlive all dependent operations and precede suite release.
    struct Cleanup {
        const AIArtSuite* art;
        const AIDictionarySuite* dict;
        AIArtHandle& fixture;
        AILiveEffectParameters& parameters;
        ~Cleanup() {
            if (fixture) Trace("cleanup-fixture", art->DisposeArt(fixture));
            if (parameters) dict->Release(parameters);
        }
    } cleanup{art.operator->(), dict.operator->(), fixture, parameters};
    const AIReal y = withEffect ? 100 : 140;
    const AIPathSegment line[] = {Segment(100, y), Segment(220, y)};
    REQUIRE(path->SetPathSegmentCount(fixture, 2));
    REQUIRE(path->SetPathSegments(fixture, 0, 2, line));
    REQUIRE(path->SetPathClosed(fixture, false));
    AIPathStyle style{};
    style.Init();
    style.strokePaint = true;
    style.stroke.width = 8;
    style.stroke.cap = kAIButtCap;
    style.stroke.join = kAIMiterJoin;
    style.stroke.miterLimit = 4;
    style.stroke.color.kind = kGrayColor;
    style.stroke.color.c.g.gray = 0.75;
    REQUIRE(paint->SetPathStyleForceSimple(fixture, &style));
    if (!withEffect) { fixture = nullptr; return kNoErr; }
    AIArtStyleHandle original = nullptr, replacement = nullptr;
    REQUIRE(styles->GetArtStyle(fixture, &original));
    if (!original) return kBadParameterErr;
    REQUIRE(live->CreateLiveEffectParameters(&parameters));
    if (!parameters) return kBadParameterErr;
    REQUIRE(live->NewArtStyleByMergingLiveEffect(original, state.effect, parameters,
        kReplaceStrokeWithLiveEffect, &replacement));
    if (!replacement) return kBadParameterErr;
    REQUIRE(styles->SetArtStyle(fixture, replacement));
    // Styles are host-managed handles. The dictionary's initial ref is released.
    fixture = nullptr;
    Trace("diagnostic-created");
    return kNoErr;
}

bool SamePoint(const AIRealPoint& a, const AIRealPoint& b) {
    return a.h == b.h && a.v == b.v;
}

ASErr Go(AILiveEffectGoMessage& message, int* strokeOrigin = nullptr) {
    if (strokeOrigin) *strokeOrigin = 0;
    Suite<AIArtSuite> art(message.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(message.d.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIPathStyleSuite> paint(message.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIDictionarySuite> dict(message.d.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIEntrySuite> entry(message.d.basic, kAIEntrySuite, kAIEntryVersion);
    REQUIRE(art.error); REQUIRE(path.error); REQUIRE(paint.error); REQUIRE(dict.error); REQUIRE(entry.error);
    AIArtHandle leaf = message.art;
    // Reject unexpected ownership aliasing rather than touching editable source.
    if (!leaf || leaf == message.srcArt) return kBadParameterErr;
    bool foundPath = false;
    for (int depth = 0; depth < 5; ++depth) {
        if (!leaf || leaf == message.srcArt) return kBadParameterErr;
        short type = kUnknownArt;
        REQUIRE(art->GetArtType(leaf, &type));
        ai::int32 attributes = 0;
        REQUIRE(art->GetArtUserAttr(leaf, kArtIsClipMask, &attributes));
        if (attributes & kArtIsClipMask) return kBadParameterErr;
        if (type == kPathArt) { foundPath = true; break; }
        if (type != kGroupArt) return kBadParameterErr;
        AIArtHandle child = nullptr, sibling = nullptr;
        REQUIRE(art->GetArtFirstChild(leaf, &child));
        if (!child) return kBadParameterErr;
        REQUIRE(art->GetArtSibling(child, &sibling));
        if (sibling) return kBadParameterErr;
        leaf = child;
    }
    if (!foundPath) return kBadParameterErr;
    ai::int16 count = 0;
    AIBoolean closed = false;
    REQUIRE(path->GetPathSegmentCount(leaf, &count));
    REQUIRE(path->GetPathClosed(leaf, &closed));
    if (count != 2 || closed) return kBadParameterErr;
    AIPathSegment original[2]{};
    REQUIRE(path->GetPathSegments(leaf, 0, 2, original));
    for (const auto& segment : original) {
        if (!std::isfinite(segment.p.h) || !std::isfinite(segment.p.v) ||
            std::abs(segment.p.h) > 1e6 || std::abs(segment.p.v) > 1e6 ||
            !SamePoint(segment.p, segment.in) || !SamePoint(segment.p, segment.out))
            return kBadParameterErr;
    }
    AIPathStyle originalStyle{};
    AIBoolean advancedFill = false;
    REQUIRE(paint->GetPathStyle(leaf, &originalStyle, &advancedFill));
    if (advancedFill || originalStyle.clip || originalStyle.fillPaint) return kBadParameterErr;

    AIStrokeStyle stroke{};
    const char* origin = nullptr;
    // ToStrokeStyle consumes the ref returned by Get, including on type failure.
    const auto readStroke = [&](ConstAIDictionaryRef source, const char* keyName) -> ASErr {
        AIEntryRef value = dict->Get(source, dict->Key(keyName));
        if (!value) return kBadParameterErr;
        return entry->ToStrokeStyle(value, &stroke);
    };
    const auto has = [&](ConstAIDictionaryRef source, const char* key) {
        return source && dict->IsKnown(source, dict->Key(key));
    };
    if (has(message.instanceInfo, kTransformedStrokeStyleKey)) {
        REQUIRE(readStroke(message.instanceInfo, kTransformedStrokeStyleKey));
        origin = "stroke-transformed-instance";
        if (strokeOrigin) *strokeOrigin = 1;
    } else if (has(message.instanceInfo, kStrokeStyleKey)) {
        REQUIRE(readStroke(message.instanceInfo, kStrokeStyleKey));
        origin = "stroke-instance";
        if (strokeOrigin) *strokeOrigin = 2;
    } else if (has(message.parameters, kStrokeStyleKey)) {
        REQUIRE(readStroke(message.parameters, kStrokeStyleKey));
        origin = "stroke-parameters";
        if (strokeOrigin) *strokeOrigin = 3;
    } else {
        Trace("missing-stroke-style");
        return kBadParameterErr;
    }
    if (!std::isfinite(stroke.width) || stroke.width <= 0 || stroke.width > 1000 ||
        stroke.cap != kAIButtCap || stroke.dash.length != 0 ||
        stroke.color.kind != kGrayColor || !std::isfinite(stroke.color.c.g.gray) ||
        stroke.color.c.g.gray < 0 || stroke.color.c.g.gray > 1)
        return kBadParameterErr;
    const double dx = original[1].p.h - original[0].p.h;
    const double dy = original[1].p.v - original[0].p.v;
    const double length = std::hypot(dx, dy);
    if (!std::isfinite(length) || length < 1e-3) return kBadParameterErr;
    const double nx = -dy / length * stroke.width * 0.5;
    const double ny = dx / length * stroke.width * 0.5;
    const auto& a = original[0].p;
    const auto& b = original[1].p;
    const AIPathSegment rectangle[] = {
        Segment(a.h + nx, a.v + ny), Segment(b.h + nx, b.v + ny),
        Segment(b.h - nx, b.v - ny), Segment(a.h - nx, a.v - ny)
    };
    AIPathStyle output{};
    output.Init();
    output.fillPaint = true;
    output.fill.color = stroke.color;
    output.fill.overprint = stroke.overprint;
    ASErr result = path->SetPathSegmentCount(leaf, 4);
    if (!result) result = path->SetPathSegments(leaf, 0, 4, rectangle);
    if (!result) result = path->SetPathClosed(leaf, true);
    if (!result) result = paint->SetPathStyle(leaf, &output);
    if (result) {
        // Restore the same processing leaf; never delete input or mutate parents.
        const ASErr countError = path->SetPathSegmentCount(leaf, 2);
        const ASErr segmentError = path->SetPathSegments(leaf, 0, 2, original);
        const ASErr closedError = path->SetPathClosed(leaf, closed);
        const ASErr styleError = paint->SetPathStyle(leaf, &originalStyle);
        Trace("rollback-count", countError); Trace("rollback-segments", segmentError);
        Trace("rollback-closed", closedError); Trace("rollback-style", styleError);
        // Persistent host failure cannot be repaired here; return failure explicitly.
        return result;
    }
    Trace(origin);
    Trace("go-replaced-line");
    return kNoErr;
}

ASErr CountedGo(AILiveEffectGoMessage& message, State& state) {
    ++state.goCalls;
    state.lastGoError = Go(message, &state.strokeOrigin);
    if (!state.lastGoError) ++state.mutations;
    return state.lastGoError;
}

// Shared checked protocol core; callable by mock tests without constructing SDK strings.
ASErr Bridge(SPMessageData& message, State& state, const char* selector,
             const char* input, char* output, size_t capacity) {
    ASErr actionError = kBadParameterErr;
    if (std::strcmp(selector, "status") == 0 && input[0] == '\0') actionError = kNoErr;
    else if (std::strcmp(selector, "create") == 0 &&
             (std::strcmp(input, "effect") == 0 || std::strcmp(input, "control") == 0)) {
        const bool effect = std::strcmp(input, "effect") == 0;
        actionError = Diagnostic(message, state, effect);
        if (!actionError) {
            if (effect) ++state.effectFixtures;
            else ++state.controlFixtures;
        }
    }
    const int length = std::snprintf(output, capacity,
        "{\"schema\":1,\"status\":\"PROPOSED\",\"plugin\":\"%s\",\"ok\":%s,\"error\":%ld,"
        "\"goCalls\":%llu,\"mutations\":%llu,\"strokeOrigin\":%d,\"lastGoError\":%ld,"
        "\"effectFixtures\":%llu,\"controlFixtures\":%llu}",
        kPluginName, actionError ? "false" : "true", static_cast<long>(actionError),
        state.goCalls, state.mutations, state.strokeOrigin, static_cast<long>(state.lastGoError),
        state.effectFixtures, state.controlFixtures);
    return length < 0 || static_cast<size_t>(length) >= capacity ? kBadParameterErr : kNoErr;
}

ASErr ScriptMessage(AIScriptMessage& message, State& state, const char* selector) {
    Suite<AIUnicodeStringSuite> unicode(message.d.basic, kAIUnicodeStringSuite, kAIUnicodeStringVersion);
    REQUIRE(unicode.error);
    // Work directly with borrowed message strings; no global Unicode suite pointer
    // or copied SDK implementation is necessary. Host owns their outer lifetime.
    char input[33]{};
    const auto length = unicode->Length(message.inParam);
    if (length > 32) return kBadParameterErr;
    for (ai::UnicodeString::size_type i = 0; i < length; ++i) {
        ai::UnicodeString::UTF32TextChar ch = 0;
        REQUIRE(unicode->GetChar(message.inParam, i, ch));
        if (ch == 0 || ch > 127) return kBadParameterErr;
        input[i] = static_cast<char>(ch);
    }
    char payload[512]{};
    REQUIRE(Bridge(message.d, state, selector, input, payload, sizeof(payload)));
    REQUIRE(unicode->Destroy(message.outParam));
    return unicode->Initialize(message.outParam, payload,
        static_cast<ai::UnicodeString::offset_type>(std::strlen(payload)), kAIUTF8CharacterEncoding);
}
} // namespace

extern "C" ASAPI ASErr PluginMain(char* caller, char* selector, void* raw) {
    if (!caller || !selector || !raw) return kBadParameterErr;
    auto& message = *static_cast<SPMessageData*>(raw);
    if (!message.basic) return kBadParameterErr;
    try {
        if (std::strcmp(caller, kSPInterfaceCaller) == 0) {
            if (std::strcmp(selector, kSPInterfaceStartupSelector) == 0) return Startup(message);
            if (std::strcmp(selector, kSPInterfaceShutdownSelector) == 0) {
                if (message.globals) {
                    REQUIRE(message.basic->FreeBlock(message.globals));
                    message.globals = nullptr;
                }
                return kNoErr;
            }
        }
        // No virtual tables, cached suites, or callbacks need fixing across reload.
        if (std::strcmp(caller, kSPAccessCaller) == 0) return kNoErr;
        auto* state = static_cast<State*>(message.globals);
        if (!state) return kNoErr;
        if (std::strcmp(caller, kCallerAIScriptMessage) == 0)
            return ScriptMessage(*static_cast<AIScriptMessage*>(raw), *state, selector);
        if (std::strcmp(caller, kCallerAIFilter) == 0) {
            const auto* command = static_cast<AIFilterMessage*>(raw);
            if (command->filter != state->command) return kBadParameterErr;
            if (std::strcmp(selector, kSelectorAIGetFilterParameters) == 0) return kNoErr;
            if (std::strcmp(selector, kSelectorAIGoFilter) == 0) {
                const ASErr result = Diagnostic(message, *state);
                if (!result) ++state->effectFixtures;
                return result;
            }
        }
        if (std::strcmp(caller, kCallerAILiveEffect) == 0) {
            if (std::strcmp(selector, kSelectorAIGoLiveEffect) == 0) {
                auto& go = *static_cast<AILiveEffectGoMessage*>(raw);
                if (go.effect != state->effect) return kBadParameterErr;
                const ASErr result = CountedGo(go, *state);
                Trace("go-return", result);
                return result;
            }
            if (std::strcmp(selector, kSelectorAIEditLiveEffectParameters) == 0) return kNoErr;
        }
        return kNoErr;
    } catch (...) {
        Trace("unexpected-exception", kBadParameterErr);
        return kBadParameterErr;
    }
}
