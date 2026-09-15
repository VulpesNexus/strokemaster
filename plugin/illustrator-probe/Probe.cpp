// Original diagnostic implementation. PROPOSED: replacement not host-verified.
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
#if defined(PROBE_TRACE) || defined(WIN_ENV)
#include <Windows.h>
#endif

namespace {
constexpr const char* kIdentity = "org.strokemaster.proposed.line-replacement.v1";
constexpr const char* kPluginName = "StrokeMasterProposedProbe";
constexpr const char* kProbeBuild = "native-prep-5";

struct Event {
    unsigned long long sequence = 0;
    char stage[64]{}, detail[112]{};
    ASErr error = kNoErr;
    ai::int32 version = 0;
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

template<size_t N> constexpr std::array<unsigned char, N> Pascal(const char (&text)[N]) {
    static_assert(N > 0 && N <= 256, "Pascal payload must fit one byte");
    std::array<unsigned char, N> result{};
    result[0] = static_cast<unsigned char>(N - 1);
    for (size_t i = 1; i < N; ++i) result[i] = static_cast<unsigned char>(text[i - 1]);
    return result;
}

void Trace(const char* stage, ASErr error = kNoErr, const char* detail = "", ai::int32 version = 0) noexcept {
    Event event{};
    Label(event.stage, stage); Label(event.detail, detail);
    event.error = error; event.version = version;
    if (currentTelemetry) {
        event.sequence = ++currentTelemetry->sequence;
        currentTelemetry->events[(event.sequence - 1) % currentTelemetry->events.size()] = event;
    }
#ifdef PROBE_TRACE
    char line[384]{};
    std::snprintf(line, sizeof(line), "{\"probe\":\"StrokeMasterProposedProbe\",\"build\":\"%s\",\"sequence\":%llu,\"stage\":\"%s\",\"detail\":\"%s\",\"version\":%ld,\"error\":%ld}\n",
        kProbeBuild, event.sequence, event.stage, event.detail, static_cast<long>(version), static_cast<long>(error));
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
        Trace("suite.acquire", error, name_, version_);
    }
    ~Suite() { if (value_) Trace("suite.release", basic_->ReleaseSuite(name_, version_), name_, version_); }
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
    Telemetry telemetry{};
    unsigned long long acceptedInputs = 0;
};

// Synchronous nested entry lifetime, independent of the persisted State layout.
// PICA writes each message's globals back on return (SPInterf.h:138-144).
// Retire every matching active frame, so an outer message cannot restore a freed
// pointer. Keep storage alive through its last callback, suite release, and trace.
struct Invocation;
Invocation* currentInvocation = nullptr;
struct Invocation {
    SPMessageData& message;
    Invocation* previous;
    State* state;
    bool retired = false;
    bool blocked = false;
    explicit Invocation(SPMessageData& value)
        : message(value), previous(currentInvocation), state(static_cast<State*>(value.globals)) {
        for (auto* frame = previous; frame; frame = frame->previous) {
            if (frame->retired && frame->message.self == message.self) {
                blocked = true;
                state = nullptr; // Do not dereference a stale globals pointer.
                message.globals = nullptr;
                break;
            }
        }
        currentInvocation = this;
    }
    ~Invocation() { currentInvocation = previous; }
    void Retire() noexcept {
        retired = true;
        for (auto* frame = this; frame; frame = frame->previous) {
            if (frame->state == state && frame->message.self == message.self) {
                frame->retired = true;
                frame->message.globals = nullptr;
            }
        }
    }
    ASErr Finish(ASErr result) {
        if (!retired || !state) return result;
        message.globals = nullptr;
        for (auto* frame = previous; frame; frame = frame->previous)
            if (frame->state == state) return result;
        // The telemetry scope and all SDK suite guards have already unwound.
        // Leave this retired frame visible during FreeBlock in case it reenters.
        const ASErr freed = message.basic->FreeBlock(state);
        if (freed) {
            message.globals = state; // Retain ownership for a later shutdown retry.
            return freed;
        }
        state = nullptr;
        return result;
    }
};

#define REQUIRE(call) do { const ASErr checked = (call); if (checked) { Trace("operation.failed", checked, #call); return checked; } } while (false)

ASErr Startup(SPMessageData& message) {
    if (!message.globals) {
        void* storage = nullptr;
        REQUIRE(message.basic->AllocateBlock(sizeof(State), &storage));
        if (!storage) return kOutOfMemoryErr;
        message.globals = new (storage) State{};
    }
    if (currentInvocation && &currentInvocation->message == &message)
        currentInvocation->state = static_cast<State*>(message.globals);
    auto& state = *static_cast<State*>(message.globals);
    TelemetryScope telemetry(&state.telemetry);
    Trace("INITIALIZES");
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIFilterSuite> filter(message.basic, kAIFilterSuite, kAIFilterVersion);
    REQUIRE(live.error);
    REQUIRE(filter.error);
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
        Trace("register.effect");
    }
    if (!state.command) {
        PlatformAddFilterData menu{};
        auto category = Pascal("StrokeMaster PROPOSED");
        auto title = Pascal("Create diagnostic line");
        char name[] = "StrokeMaster PROPOSED create line";
        menu.category = category.data();
        menu.title = title.data();
        REQUIRE(filter->AddFilter(message.self, name, &menu, 0, &state.command));
        if (!state.command) return kBadParameterErr;
        Trace("register.command");
    }
    Trace("REGISTERS");
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
    // Capture the actual simple native stroke before replacing its style node.
    // The merge API takes our parameter dictionary; it does not promise to fill it.
    AIPathStyle capturedStyle{};
    AIBoolean capturedAdvanced = false;
    REQUIRE(paint->GetPathStyle(fixture, &capturedStyle, &capturedAdvanced));
    if (capturedAdvanced || capturedStyle.fillPaint || capturedStyle.clip || !capturedStyle.strokePaint) {
        Trace("attach.reject", kBadParameterErr, "source.not-simple-stroke");
        return kBadParameterErr;
    }
    Suite<AIEntrySuite> entry(message.basic, kAIEntrySuite, kAIEntryVersion);
    REQUIRE(entry.error);
    AIArtStyleHandle original = nullptr, replacement = nullptr;
    REQUIRE(styles->GetArtStyle(fixture, &original));
    if (!original) return kBadParameterErr;
    REQUIRE(live->CreateLiveEffectParameters(&parameters));
    if (!parameters) return kBadParameterErr;
    const auto strokeKey = dict->Key(kStrokeStyleKey);
    if (!strokeKey) return kBadParameterErr;
    const auto strokeEntry = entry->FromStrokeStyle(&capturedStyle.stroke);
    if (!strokeEntry) {
        Trace("attach.reject", kOutOfMemoryErr, "stroke.entry-allocation");
        return kOutOfMemoryErr;
    }
    // Set consumes FromStrokeStyle's initial reference, including on error.
    // StrokeStyle is a documented, non-temporary key. No extra entry release.
    REQUIRE(dict->Set(parameters, strokeKey, strokeEntry));
    Trace("attach.stroke-parameter", kNoErr, kStrokeStyleKey);
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

ASErr Reject(const char* gate) {
    Trace("go.reject", kBadParameterErr, gate);
    return kBadParameterErr;
}

ASErr Go(AILiveEffectGoMessage& message, int* strokeOrigin = nullptr, bool* accepted = nullptr) {
    if (strokeOrigin) *strokeOrigin = 0;
    if (accepted) *accepted = false;
    Suite<AIArtSuite> art(message.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(message.d.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIPathStyleSuite> paint(message.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIDictionarySuite> dict(message.d.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIEntrySuite> entry(message.d.basic, kAIEntrySuite, kAIEntryVersion);
    REQUIRE(art.error); REQUIRE(path.error); REQUIRE(paint.error); REQUIRE(dict.error); REQUIRE(entry.error);
    AIArtHandle leaf = message.art;
    // Reject unexpected ownership aliasing rather than touching editable source.
    if (!leaf) return Reject("input.null");
    if (leaf == message.srcArt) return Reject("input.source-alias");
    bool foundPath = false;
    for (int depth = 0; depth < 5; ++depth) {
        if (!leaf) return Reject("leaf.null");
        if (leaf == message.srcArt) return Reject("leaf.source-alias");
        short type = kUnknownArt;
        REQUIRE(art->GetArtType(leaf, &type));
        ai::int32 attributes = 0;
        REQUIRE(art->GetArtUserAttr(leaf, kArtIsClipMask, &attributes));
        char shape[112]{};
        std::snprintf(shape, sizeof(shape), "depth=%d type=%d clip=%d", depth, int(type), int(!!(attributes & kArtIsClipMask)));
        Trace("input.shape", kNoErr, shape);
        if (attributes & kArtIsClipMask) return Reject("leaf.clip-mask");
        if (type == kPathArt) { foundPath = true; break; }
        if (type != kGroupArt) return Reject("leaf.unsupported-type");
        AIArtHandle child = nullptr, sibling = nullptr;
        REQUIRE(art->GetArtFirstChild(leaf, &child));
        if (!child) return Reject("group.empty");
        REQUIRE(art->GetArtSibling(child, &sibling));
        if (sibling) return Reject("group.multiple-children");
        leaf = child;
    }
    if (!foundPath) return Reject("group.depth-limit");
    ai::int16 count = 0;
    AIBoolean closed = false;
    REQUIRE(path->GetPathSegmentCount(leaf, &count));
    REQUIRE(path->GetPathClosed(leaf, &closed));
    char facts[112]{};
    std::snprintf(facts, sizeof(facts), "count=%d closed=%d", int(count), int(closed));
    Trace("input.path", kNoErr, facts);
    if (count != 2) return Reject("path.segment-count");
    if (closed) return Reject("path.closed");
    AIPathSegment original[2]{};
    REQUIRE(path->GetPathSegments(leaf, 0, 2, original));
    for (const auto& segment : original) {
        if (!std::isfinite(segment.p.h) || !std::isfinite(segment.p.v) ||
            std::abs(segment.p.h) > 1e6 || std::abs(segment.p.v) > 1e6)
            return Reject("path.coordinate-range");
        if (!SamePoint(segment.p, segment.in) || !SamePoint(segment.p, segment.out))
            return Reject("path.curve-handles");
    }
    AIPathStyle originalStyle{};
    AIBoolean advancedFill = false;
    REQUIRE(paint->GetPathStyle(leaf, &originalStyle, &advancedFill));
    std::snprintf(facts, sizeof(facts), "advanced=%d clip=%d fill=%d stroke=%d", int(advancedFill), int(originalStyle.clip), int(originalStyle.fillPaint), int(originalStyle.strokePaint));
    Trace("input.style", kNoErr, facts);
    if (advancedFill) return Reject("style.advanced-fill");
    if (originalStyle.clip) return Reject("style.clip");
    if (originalStyle.fillPaint) return Reject("style.fill-painted");

    AIStrokeStyle stroke{};
    const char* origin = nullptr;
    // ToStrokeStyle consumes the ref returned by Get, including on type failure.
    const auto readStroke = [&](ConstAIDictionaryRef source, const char* keyName) -> ASErr {
        AIEntryRef value = dict->Get(source, dict->Key(keyName));
        if (!value) return Reject("stroke.entry-null");
        const auto result = entry->ToStrokeStyle(value, &stroke);
        Trace("stroke.convert", result, keyName);
        return result;
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
        std::snprintf(facts, sizeof(facts), "instance=%d parameters=%d", int(message.instanceInfo != nullptr), int(message.parameters != nullptr));
        Trace("missing-stroke-style", kBadParameterErr, facts);
        return Reject("stroke.keys-missing");
    }
    std::snprintf(facts, sizeof(facts), "width=%.9g cap=%d dash=%d colorKind=%d", double(stroke.width), int(stroke.cap), int(stroke.dash.length), int(stroke.color.kind));
    Trace("input.stroke", kNoErr, facts);
    if (!std::isfinite(stroke.width) || stroke.width <= 0 || stroke.width > 1000) return Reject("stroke.width");
    if (stroke.cap != kAIButtCap) return Reject("stroke.cap");
    if (stroke.dash.length != 0) return Reject("stroke.dash");
    if (stroke.color.kind != kGrayColor) return Reject("stroke.color-kind");
    if (!std::isfinite(stroke.color.c.g.gray) || stroke.color.c.g.gray < 0 || stroke.color.c.g.gray > 1) return Reject("stroke.gray-range");
    const double dx = original[1].p.h - original[0].p.h;
    const double dy = original[1].p.v - original[0].p.v;
    const double length = std::hypot(dx, dy);
    if (!std::isfinite(length) || length < 1e-3) return Reject("path.length");
    if (accepted) *accepted = true;
    Trace("TEST_ART_REACHES_CALLBACK", kNoErr, origin);
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
    Trace("CALLBACK_EXECUTES");
    bool accepted = false;
    state.lastGoError = Go(message, &state.strokeOrigin, &accepted);
    if (accepted) ++state.acceptedInputs;
    if (!state.lastGoError) ++state.mutations;
    Trace("callback.result", state.lastGoError);
    return state.lastGoError;
}

ASErr Diagnostics(const State& state, char* output, size_t capacity) {
    int n = std::snprintf(output, capacity,
        "{\"schema\":2,\"plugin\":\"%s\",\"build\":\"%s\",\"processId\":%lu,\"effectRegistered\":%s,\"commandRegistered\":%s,\"acceptedInputs\":%llu,\"sequence\":%llu,\"events\":[",
        kPluginName, kProbeBuild, static_cast<unsigned long>(GetCurrentProcessId()), state.effect ? "true" : "false", state.command ? "true" : "false", state.acceptedInputs, state.telemetry.sequence);
    if (n < 0 || static_cast<size_t>(n) >= capacity) return kBadParameterErr;
    size_t used = static_cast<size_t>(n);
    const auto first = state.telemetry.sequence > 32 ? state.telemetry.sequence - 31 : 1;
    for (auto seq = first; seq <= state.telemetry.sequence; ++seq) {
        const auto& e = state.telemetry.events[(seq - 1) % 32];
        n = std::snprintf(output + used, capacity - used,
            "%s{\"sequence\":%llu,\"stage\":\"%s\",\"detail\":\"%s\",\"version\":%ld,\"error\":%ld}",
            seq == first ? "" : ",", e.sequence, e.stage, e.detail, static_cast<long>(e.version), static_cast<long>(e.error));
        if (n < 0 || static_cast<size_t>(n) >= capacity - used) return kBadParameterErr;
        used += static_cast<size_t>(n);
    }
    return std::snprintf(output + used, capacity - used, "]}") == 2 && capacity - used > 2 ? kNoErr : kBadParameterErr;
}

// Shared checked protocol core; callable by mock tests without constructing SDK strings.
ASErr Bridge(SPMessageData& message, State& state, const char* selector,
             const char* input, char* output, size_t capacity) {
    ASErr actionError = kBadParameterErr;
    if (std::strcmp(selector, "diagnostics") == 0 && input[0] == '\0') return Diagnostics(state, output, capacity);
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
    char payload[16384]{};
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
    Invocation invocation(message);
    const bool shutdown = std::strcmp(caller, kSPInterfaceCaller) == 0 &&
        std::strcmp(selector, kSPInterfaceShutdownSelector) == 0;
    if (invocation.blocked) return shutdown ? kNoErr : kBadParameterErr;
    const auto dispatch = [&]() -> ASErr {
    TelemetryScope telemetry(invocation.state ? &invocation.state->telemetry : nullptr);
    try {
        if (std::strcmp(caller, kSPInterfaceCaller) == 0) {
            if (std::strcmp(selector, kSPInterfaceStartupSelector) == 0) return Startup(message);
            if (std::strcmp(selector, kSPInterfaceShutdownSelector) == 0) {
                Trace("shutdown.enter");
                invocation.Retire();
                Trace("shutdown.return"); // Plugin shutdown is not proof of host exit.
                return kNoErr;
            }
        }
        // No virtual tables, cached suites, or callbacks need fixing across reload.
        if (std::strcmp(caller, kSPAccessCaller) == 0) { Trace("access", kNoErr, selector); return kNoErr; }
        auto* state = static_cast<State*>(message.globals);
        if (!state) return kNoErr;
        if (std::strcmp(caller, kCallerAIScriptMessage) == 0) {
            Trace("bridge", kNoErr, selector);
            return ScriptMessage(*static_cast<AIScriptMessage*>(raw), *state, selector);
        }
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
    };
    return invocation.Finish(dispatch());
}
