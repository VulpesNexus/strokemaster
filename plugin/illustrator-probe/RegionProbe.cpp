// PROPOSED region-prep-13: real-core closed-region replacement for the marked
// analytic fixture only. RegionNative.h carries the validated integration:
// parser-checked source admission, the closed-form fixture oracle, and the
// owned-output SDK backend. Arbitrary artwork is refused before staging.
#include "SPBasic.h"
#include "SPInterf.h"
#include "SPAccess.h"
#include "AIScriptMessage.h"
#include "AIUnicodeString.h"
#include "RegionNative.h"
#include "StrokeBehind.h"
#include "TextBehind.h"
#include "AIMatchingArt.h"
#include "AIMdMemory.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace region_probe {
constexpr char build[] = "region-prep-13";

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

// Second, census-only effect. It declares path and compound input but not
// text, as Adobe's Twirl sample does, so the host decides what a live text
// source becomes before Go. Go records the processing tree and passes it
// through unchanged; it never edits art.
constexpr char censusIdentity[] = "org.strokemaster.proposed.input-census.v1";
constexpr ai::int32 censusInput = kGroupInputArt | kPathInputArt | kCompoundPathInputArt;

struct CensusStats {
    unsigned long long calls = 0;
    int nodes = 0, groups = 0, paths = 0, compounds = 0, textFrames = 0, other = 0;
    int fillOnly = 0, strokeOnly = 0, fillAndStroke = 0, unpainted = 0;
    bool truncated = false;
    char tree[512]{};
};

// Third effect: the stroke-behind paint-order correction in StrokeBehind.h,
// with the same input declaration as the census so live text arrives as glyph
// outlines while the source stays editable.
constexpr char strokeBehindIdentity[] = "org.strokemaster.proposed.stroke-behind.v1";

// Fourth effect: stroke-behind that keeps live text as text (TextBehind.h). It
// declares text-frame input, so the host hands it the text frame itself.
constexpr char textBehindIdentity[] = "org.strokemaster.proposed.stroke-behind-text.v1";
constexpr ai::int32 textBehindInput = kGroupInputArt | kTextFrameInputArt;

// Result of the last "appearance" bridge command: a native, effect-free object-level
// stroke added beneath the contents of every text frame in the document.
struct AppearanceStats {
    const char* gate = "ready";
    int frames = 0, styled = 0, fieldsBefore = -1, fieldsAfter = -1, contentsBefore = -1, contentsAfter = -1;
};

struct State {
    AILiveEffectHandle effect = nullptr;
    AppearanceStats appearance{};
    AILiveEffectHandle census = nullptr;
    AILiveEffectHandle strokeBehind = nullptr;
    stroke_behind::Stats strokeBehindStats{};
    AILiveEffectHandle textBehind = nullptr;
    text_behind::Stats textBehindStats{};
    CensusStats inputCensus{};
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

ASErr RegisterOne(SPMessageData& message, const AILiveEffectSuite& live, const char* identity, char* title,
                  ai::int32 input, AILiveEffectHandle& out) {
    ai::int32 count = 0;
    auto err = live.CountLiveEffects(&count);
    if (err) return err;
    if (count < 0 || count > 8192) return kBadParameterErr;
    // Enumeration differentiates absent identity from a failed lookup operation.
    for (ai::int32 i = 0; i < count; ++i) {
        AILiveEffectHandle handle = nullptr;
        const char* name = nullptr;
        err = live.GetNthLiveEffect(i, &handle);
        if (err) return err;
        if (!handle) return kBadParameterErr;
        err = live.GetLiveEffectName(handle, &name);
        if (err) return err;
        if (name && std::strcmp(name, identity) == 0) {
            ai::int32 flags = 0;
            err = live.GetStyleFilterFlags(handle, &flags);
            if (err) return err;
            if (flags != regionFilterType) return kBadParameterErr;
            out = handle;
            return kNoErr;
        }
    }
    AILiveEffectData data{};
    data.self = message.self;
    data.name = identity;
    data.title = title;
    data.majorVersion = 1;
    data.minorVersion = 0;
    data.prefersAsInput = input;
    data.styleFilterFlags = regionFilterType;
    AILiveEffectHandle handle = nullptr;
    err = live.AddLiveEffect(&data, &handle);
    if (err) return err;
    if (!handle) return kBadParameterErr;
    out = handle;
    Trace("register.effect", kNoErr, identity);
    return kNoErr;
}

ASErr Register(SPMessageData& message, State& state) {
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    if (live.error) return live.error;
    char title[] = "StrokeMaster PROPOSED closed region";
    auto err = RegisterOne(message, *live.operator&(), effectIdentity, title, kGroupInputArt, state.effect);
    if (err) return err;
    char censusTitle[] = "StrokeMaster PROPOSED input census";
    err = RegisterOne(message, *live.operator&(), censusIdentity, censusTitle, censusInput, state.census);
    if (err) return err;
    char strokeBehindTitle[] = "StrokeMaster PROPOSED stroke behind";
    err = RegisterOne(message, *live.operator&(), strokeBehindIdentity, strokeBehindTitle, censusInput,
                      state.strokeBehind);
    if (err) return err;
    char textBehindTitle[] = "StrokeMaster PROPOSED stroke behind, text kept";
    return RegisterOne(message, *live.operator&(), textBehindIdentity, textBehindTitle, textBehindInput,
                       state.textBehind);
}

ASErr TextBehindGo(AILiveEffectGoMessage& go, State& state) {
    auto& stats = state.textBehindStats;
    ++stats.calls;
    Suite<AIArtSuite> art(go.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathStyleSuite> paint(go.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIBlendStyleSuite> blend(go.d.basic, kAIBlendStyleSuite, kAIBlendStyleVersion);
    Suite<AIMaskSuite> mask(go.d.basic, kAIMaskSuite, kAIMaskVersion);
    Suite<AIArtStyleSuite> styles(go.d.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(go.d.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AITextFrameSuite> text(go.d.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(go.d.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(go.d.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    ASErr err = kNoErr;
    for (ASErr e : {art.error, paint.error, blend.error, mask.error, styles.error, parser.error, text.error,
                    range.error, features.error}) if (!err && e) err = e;
    if (err) stats.gate = "suites";
    if (!err && !go.art) { stats.gate = "input.null"; err = kBadParameterErr; }
    if (!err) {
        text_behind::Suites suites{*art.operator&(), *paint.operator&(), *blend.operator&(), *mask.operator&(),
                                   *styles.operator&(), *parser.operator&(), *text.operator&(),
                                   *range.operator&(), *features.operator&()};
        text_behind::Transform transform(suites, stats);
        AIArtHandle result = go.art;
        int mode = text_behind::kFull;
        Suite<AIDictionarySuite> dict(go.d.basic, kAIDictionarySuite, kAIDictionaryVersion);
        if (!dict.error && go.parameters) {
            ai::int32 value = 0;
            if (!dict->GetIntegerEntry(go.parameters, dict->Key("StrokeMasterTextMode"), &value) &&
                value >= text_behind::kFull && value <= text_behind::kClearOnly)
                mode = value;
        }
        err = transform.Apply(result, mode);
        if (!err) go.art = result;
    }
    Trace("text-behind.result", err, stats.gate);
    if (!err) { ++stats.published; return kNoErr; }
    ++stats.refused;
    return std::strcmp(stats.gate, "output.rollback-failed") == 0 ? err : kNoErr;
}

ASErr StrokeBehindGo(AILiveEffectGoMessage& go, State& state) {
    auto& stats = state.strokeBehindStats;
    ++stats.calls;
    Suite<AIArtSuite> art(go.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathStyleSuite> paint(go.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIBlendStyleSuite> blend(go.d.basic, kAIBlendStyleSuite, kAIBlendStyleVersion);
    Suite<AIMaskSuite> mask(go.d.basic, kAIMaskSuite, kAIMaskVersion);
    Suite<AIArtStyleSuite> styles(go.d.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(go.d.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    ASErr err = kNoErr;
    for (ASErr e : {art.error, paint.error, blend.error, mask.error, styles.error, parser.error}) if (!err && e) err = e;
    if (err) stats.gate = "suites";
    if (!err && !go.art) { stats.gate = "input.null"; err = kBadParameterErr; }
    if (!err) {
        stroke_behind::Transform transform(*art.operator&(), *paint.operator&(), *blend.operator&(), *mask.operator&(),
                                           *styles.operator&(), *parser.operator&(), stats);
        err = transform.Apply(go.art);
    }
    Trace("stroke-behind.result", err, stats.gate);
    if (!err) { ++stats.published; return kNoErr; }
    ++stats.refused;
    // A refusal leaves the processing copy as it was, so it passes through.
    // Only a failed rollback, where the copy may be partly changed, is
    // reported to the host.
    return std::strcmp(stats.gate, "output.rollback-failed") == 0 ? err : kNoErr;
}

// Walks the processing art handed to the census effect. Grammar as in
// RegionNative's census, but with aggregate counts that are never truncated
// (within 4,096 nodes and depth 8) and a tree string that may be.
struct CensusWalker {
    const AIArtSuite& art;
    const AIPathStyleSuite& paint;
    CensusStats& stats;
    void Append(const char* text) {
        const std::size_t used = std::strlen(stats.tree);
        const int n = std::snprintf(stats.tree + used, sizeof(stats.tree) - used, "%s", text);
        if (n < 0 || static_cast<std::size_t>(n) >= sizeof(stats.tree) - used) stats.truncated = true;
    }
    ASErr Node(AIArtHandle node, int depth) {
        if (++stats.nodes > 4096 || depth > 8) { stats.truncated = true; return kNoErr; }
        short type = 0;
        auto e = art.GetArtType(node, &type);
        if (e) return e;
        char text[24]{};
        std::snprintf(text, sizeof(text), "%d", static_cast<int>(type));
        Append(text);
        if (type == kGroupArt) ++stats.groups;
        else if (type == kPathArt) ++stats.paths;
        else if (type == kCompoundPathArt) ++stats.compounds;
        else if (type == kTextFrameArt) ++stats.textFrames;
        else ++stats.other;
        if (type == kPathArt || type == kCompoundPathArt) {
            AIPathStyle style{};
            AIBoolean advanced = false;
            e = paint.GetPathStyle(node, &style, &advanced);
            if (e) return e;
            std::snprintf(text, sizeof(text), "[%d,%d]", style.fillPaint ? 1 : 0, style.strokePaint ? 1 : 0);
            Append(text);
            if (type == kPathArt || style.fillPaint || style.strokePaint) {
                if (style.fillPaint && style.strokePaint) ++stats.fillAndStroke;
                else if (style.fillPaint) ++stats.fillOnly;
                else if (style.strokePaint) ++stats.strokeOnly;
                else ++stats.unpainted;
            }
            if (type == kPathArt) return kNoErr;
        }
        if (type != kGroupArt && type != kCompoundPathArt) return kNoErr;
        AIArtHandle child = nullptr;
        e = art.GetArtFirstChild(node, &child);
        if (e) return e;
        if (!child) return kNoErr;
        // Children of a compound share its paint; they are counted as nodes only.
        const bool compound = type == kCompoundPathArt;
        Append("(");
        for (int count = 0; child; ++count) {
            if (count) Append(",");
            if (compound) {
                if (++stats.nodes > 4096) { stats.truncated = true; break; }
                Append("2");
            } else {
                e = Node(child, depth + 1);
                if (e) return e;
            }
            e = art.GetArtSibling(child, &child);
            if (e) return e;
        }
        Append(")");
        return kNoErr;
    }
};

ASErr CensusGo(AILiveEffectGoMessage& go, State& state) {
    Suite<AIArtSuite> art(go.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathStyleSuite> paint(go.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    if (art.error) return art.error;
    if (paint.error) return paint.error;
    const auto calls = state.inputCensus.calls + 1;
    state.inputCensus = CensusStats{};
    state.inputCensus.calls = calls;
    CensusWalker walker{*art.operator&(), *paint.operator&(), state.inputCensus};
    const auto err = go.art ? walker.Node(go.art, 0) : kBadParameterErr;
    Trace("census.result", err, state.inputCensus.truncated ? "truncated" : "complete");
    // The census never edits art: whatever it found, the processing copy is
    // the result, so no host alert is raised.
    return kNoErr;
}

// Adds one object-level stroke paint field (0.4 gray, 8 pt, miter join, limit 4) at the
// bottom of every text frame's art style, below the frame's contents, so the frame's
// characters paint above it. This writes an ordinary native Appearance, the manual
// "stroke below Characters" technique; no live effect is involved. Diagnostic only: it
// acts on every text frame of the current document, which the probe creates itself; a
// failure part-way leaves earlier frames styled, and each call adds another stroke.
ASErr AppearanceStrokeBelow(SPMessageData& message, State& state) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    for (ASErr e : {matching.error, memory.error, styles.error, parser.error}) if (e) { stats.gate = "suites"; return e; }
    AIMatchingArtSpec spec(kTextFrameArt, 0, 0);
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = matching->GetMatchingArt(&spec, 1, &matches, &count);
    if (err) { stats.gate = "match"; return err; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    stats.frames = count;
    if (count <= 0 || count > 64) { stats.gate = count <= 0 ? "no-text-frame" : "too-many-frames"; return kBadParameterErr; }
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle frame = (*matches)[i];
        AIArtStyleHandle style = nullptr;
        err = styles->GetArtStyle(frame, &style);
        if (err) { stats.gate = "get-style"; return err; }
        AIStyleParser p = nullptr;
        err = parser->NewParser(&p);
        if (err || !p) { stats.gate = "parser"; return err ? err : kOutOfMemoryErr; }
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } }
            dispose{*parser.operator&(), p};
        if (style) {
            err = parser->ParseStyle(p, style);
            if (err) { stats.gate = "parse"; return err; }
        }
        stats.fieldsBefore = parser->CountPaintFields(p);
        stats.contentsBefore = parser->GetGroupContentsPosition(p);
        // Refuse before any change: a style that already carries effects would be
        // rebuilt without its meaning being modeled here.
        if (parser->ContainsEffects(p)) { stats.gate = "has-effects"; return kBadParameterErr; }
        AIPathStyle base{};
        base.Init();
        AIStrokeStyle stroke = base.stroke;
        stroke.color.kind = kGrayColor;
        stroke.color.c.g.gray = static_cast<AIReal>(0.4);
        stroke.width = 8;
        stroke.join = kAIMiterJoin;
        stroke.miterLimit = 4;
        AIParserPaintField field = nullptr;
        err = parser->NewPaintFieldStroke(&stroke, nullptr, &field);
        if (err || !field) { stats.gate = "new-stroke"; return err ? err : kOutOfMemoryErr; }
        err = parser->InsertNthPaintField(p, -1, field);
        if (err) { parser->DisposePaintField(field); stats.gate = "insert"; return err; }
        AIArtStyleHandle created = nullptr;
        err = parser->CreateNewStyle(p, &created);
        if (err || !created) { stats.gate = "create-style"; return err ? err : kBadParameterErr; }
        err = styles->SetArtStyle(frame, created);
        if (err) { stats.gate = "set-style"; return err; }
        // Read back what the frame now carries.
        AIArtStyleHandle now = nullptr;
        err = styles->GetArtStyle(frame, &now);
        if (err || !now) { stats.gate = "readback"; return err ? err : kBadParameterErr; }
        AIStyleParser q = nullptr;
        err = parser->NewParser(&q);
        if (err || !q) { stats.gate = "readback-parser"; return err ? err : kOutOfMemoryErr; }
        Dispose disposeQ{*parser.operator&(), q};
        err = parser->ParseStyle(q, now);
        if (err) { stats.gate = "readback-parse"; return err; }
        stats.fieldsAfter = parser->CountPaintFields(q);
        stats.contentsAfter = parser->GetGroupContentsPosition(q);
        // The inserted stroke is the last paint field; it must sit at or below the
        // contents position (positions run top to bottom), or it would paint over them.
        if (stats.fieldsAfter != stats.fieldsBefore + 1 || parser->ContainsEffects(q) ||
            stats.contentsAfter < 0 || stats.contentsAfter > stats.fieldsAfter - 1) {
            stats.gate = "readback-mismatch"; return kBadParameterErr;
        }
        ++stats.styled;
    }
    stats.gate = "styled";
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
        "\"passThroughs\":%llu,\"wrapperPaintFields\":%d,\"wrapperContainsPaint\":%d,"
        "\"wrapperBase\":%d,\"groupContentsPosition\":%d,\"processingCensus\":\"%s\","
        "\"readbackAnchorDelta\":%.9g,\"readbackHandleDelta\":%.9g,"
        "\"census\":{\"registered\":%s,\"calls\":%llu,\"nodes\":%d,\"groups\":%d,\"paths\":%d,"
        "\"compounds\":%d,\"textFrames\":%d,\"other\":%d,\"fillOnly\":%d,\"strokeOnly\":%d,"
        "\"fillAndStroke\":%d,\"unpainted\":%d,\"truncated\":%s,\"tree\":\"%s\"},"
        "\"strokeBehind\":{\"registered\":%s,\"calls\":%llu,\"published\":%llu,\"refused\":%llu,"
        "\"leaves\":%d,\"stroked\":%d,\"gate\":\"%s\"},"
        "\"textBehind\":{\"registered\":%s,\"calls\":%llu,\"published\":%llu,\"refused\":%llu,"
        "\"rootType\":%d,\"frames\":%d,\"outlineLeaves\":%d,\"stroked\":%d,\"mode\":%d,\"census\":\"%s\",\"gate\":\"%s\"},"
        "\"appearance\":{\"gate\":\"%s\",\"frames\":%d,\"styled\":%d,\"fieldsBefore\":%d,\"fieldsAfter\":%d,"
        "\"contentsBefore\":%d,\"contentsAfter\":%d},"
        "\"supportedGate\":\"MARKED_ANALYTIC_FIXTURE_ONLY\"}",
        regionPluginName, build, static_cast<unsigned long>(GetCurrentProcessId()),
        actionError ? "false" : "true", state.effect ? "true" : "false", effectIdentity,
        state.metrics.mutations > 0 ? "true" : "false",
        state.metrics.goCalls, state.metrics.mutations, state.metrics.lastGoError,
        state.metrics.gate, state.metrics.maxVertexDeviation, state.fixtures,
        state.metrics.passThroughs, state.metrics.wrapperPaintFields, state.metrics.wrapperContainsPaint,
        state.metrics.wrapperBase, state.metrics.groupContentsPosition, state.metrics.processingCensus,
        state.metrics.readbackAnchorDelta, state.metrics.readbackHandleDelta,
        state.census ? "true" : "false", state.inputCensus.calls, state.inputCensus.nodes, state.inputCensus.groups,
        state.inputCensus.paths, state.inputCensus.compounds, state.inputCensus.textFrames, state.inputCensus.other,
        state.inputCensus.fillOnly, state.inputCensus.strokeOnly, state.inputCensus.fillAndStroke,
        state.inputCensus.unpainted, state.inputCensus.truncated ? "true" : "false", state.inputCensus.tree,
        state.strokeBehind ? "true" : "false", state.strokeBehindStats.calls, state.strokeBehindStats.published,
        state.strokeBehindStats.refused, state.strokeBehindStats.leaves, state.strokeBehindStats.stroked,
        state.strokeBehindStats.gate,
        state.textBehind ? "true" : "false", state.textBehindStats.calls, state.textBehindStats.published,
        state.textBehindStats.refused, state.textBehindStats.rootType, state.textBehindStats.frames,
        state.textBehindStats.outlineLeaves, state.textBehindStats.stroked, state.textBehindStats.mode,
        state.textBehindStats.census, state.textBehindStats.gate,
        state.appearance.gate, state.appearance.frames, state.appearance.styled, state.appearance.fieldsBefore,
        state.appearance.fieldsAfter, state.appearance.contentsBefore, state.appearance.contentsAfter);
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
    else if (std::strcmp(selector, "appearance") == 0 && std::strcmp(input, "stroke-below") == 0)
        actionError = AppearanceStrokeBelow(message.d, state);
    else if (std::strcmp(selector, "create") == 0 &&
             (std::strcmp(input, "region") == 0 || std::strcmp(input, "control") == 0)) {
        const bool withEffect = std::strcmp(input, "region") == 0;
        actionError = WithNative(message.d, state, "create", [withEffect](Native& native) {
            return native.Create(withEffect);
        });
        if (!actionError) ++state.fixtures;
    }
    char payload[4096]{};
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
                    state->census = startupState.census;
                    state->strokeBehind = startupState.strokeBehind;
                    state->textBehind = startupState.textBehind;
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
                if (state->strokeBehind && go.effect == state->strokeBehind) {
                    region_probe::TelemetryScope telemetry(&state->telemetry);
                    return finish(region_probe::StrokeBehindGo(go, *state));
                }
                if (state->textBehind && go.effect == state->textBehind) {
                    region_probe::TelemetryScope telemetry(&state->telemetry);
                    return finish(region_probe::TextBehindGo(go, *state));
                }
                if (state->census && go.effect == state->census) {
                    region_probe::TelemetryScope telemetry(&state->telemetry);
                    return finish(region_probe::CensusGo(go, *state));
                }
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
                // A refusal leaves the processing copy untouched, so it passes
                // through as the effect's identity result (the SDK asks effects
                // to pass through art they cannot handle). Returning the error
                // made Illustrator raise a modal alert on every redraw. Only a
                // failed rollback, where the copy may be partly written, is
                // still reported to the host.
                if (err && std::strcmp(metrics.gate, "output.rollback-failed") != 0) {
                    ++metrics.passThroughs;
                    return finish(kNoErr);
                }
                return finish(err);
            }
            return finish(kNoErr);
        }
        return finish(kNoErr);
    } catch (...) {
        return kBadParameterErr;
    }
}
