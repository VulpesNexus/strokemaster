// PROPOSED region-prep-14: real-core closed-region replacement for the marked
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
#include "NativeMiter.h"
#include <algorithm>
#include "AIMatchingArt.h"
#include "AIMdMemory.h"
#include "AIPaintStyle.h"
#include "AIATEPaint.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace region_probe {
constexpr char build[] = "region-prep-14";

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

// Result of the last "native-fix" bridge command (NativeFix): the owner's miter rulings and
// the Outside rule for overlapping contours, written as native attributes.
struct NativeFixStats {
    const char* gate = "ready";
    int paths = 0, pathLimits = 0, reordered = 0, frames = 0, chars = 0, charLimits = 0, mapped = 0;
    int keptBeveled = 0, cutMitered = 0, corners = 0, dashes = 0;
};

struct State {
    AILiveEffectHandle effect = nullptr;
    AppearanceStats appearance{};
    AILiveEffectHandle census = nullptr;
    AILiveEffectHandle strokeBehind = nullptr;
    stroke_behind::Stats strokeBehindStats{};
    AILiveEffectHandle textBehind = nullptr;
    text_behind::Stats textBehindStats{};
    NativeFixStats nativeFix{};
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
// The selected text frames, or every text frame of the document when none is selected.
ASErr SelectedOrAllFrames(const AIMatchingArtSuite& matching, AIArtHandle*** matches, ai::int32* count) {
    AIMatchingArtSpec selected(kTextFrameArt, kArtSelected, kArtSelected);
    auto err = matching.GetMatchingArt(&selected, 1, matches, count);
    if (err || *count > 0) return err;
    AIMatchingArtSpec all(kTextFrameArt, 0, 0);
    return matching.GetMatchingArt(&all, 1, matches, count);
}

// The native stroke-behind for text, two forms (2026-10-15): "stroke-below" adds
// an object-level stroke beneath the characters (uniform strokes, any fills);
// "fill-above" adds an object-level black fill above the characters, which paints
// every glyph fill over every character stroke (uniform fills, any strokes). Both
// are ordinary Appearance data: no plugin draws them and text edits follow.
enum class AppearanceKind { StrokeBelow, FillAbove };

ASErr AppearanceStrokeBelow(SPMessageData& message, State& state, AppearanceKind kind = AppearanceKind::StrokeBelow) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    for (ASErr e : {matching.error, memory.error, styles.error, parser.error}) if (e) { stats.gate = "suites"; return e; }
    // The selected text frames, or every text frame when none is selected.
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = SelectedOrAllFrames(*matching.operator&(), &matches, &count);
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
        AIParserPaintField field = nullptr;
        if (kind == AppearanceKind::StrokeBelow) {
            AIStrokeStyle stroke = base.stroke;
            stroke.color.kind = kGrayColor;
            stroke.color.c.g.gray = static_cast<AIReal>(0.4);
            stroke.width = 8;
            stroke.join = kAIMiterJoin;
            stroke.miterLimit = 4;
            err = parser->NewPaintFieldStroke(&stroke, nullptr, &field);
        } else {
            AIFillStyle fill = base.fill;
            fill.color.kind = kGrayColor;
            fill.color.c.g.gray = static_cast<AIReal>(1);
            err = parser->NewPaintFieldFill(&fill, false, nullptr, &field);
        }
        if (err || !field) { stats.gate = "new-field"; return err ? err : kOutOfMemoryErr; }
        err = parser->InsertNthPaintField(p, kind == AppearanceKind::StrokeBelow ? -1 : 0, field);
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
        // The inserted fill is the first field; the contents must sit below it.
        const bool placed = kind == AppearanceKind::StrokeBelow
            ? stats.contentsAfter >= 0 && stats.contentsAfter <= stats.fieldsAfter - 1
            : stats.contentsAfter >= 1;
        if (stats.fieldsAfter != stats.fieldsBefore + 1 || parser->ContainsEffects(q) || !placed) {
            stats.gate = "readback-mismatch"; return kBadParameterErr;
        }
        ++stats.styled;
    }
    stats.gate = "styled";
    return kNoErr;
}

// Sets every character's stroke join of every text frame in the document: "bevel",
// "round", or a miter limit (miter join). A native character attribute, so text
// edits keep it with no plugin. Research command for the glyph-tip rule.
ASErr CharacterJoin(SPMessageData& message, State& state, const char* value) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    for (ASErr e : {matching.error, memory.error, text.error, range.error, features.error})
        if (e) { stats.gate = "suites"; return e; }
    ATE::LineJoinType join = ATE::kMiterJoin;
    double limit = 0;
    if (std::strcmp(value, "bevel") == 0) join = ATE::kBevelJoin;
    else if (std::strcmp(value, "round") == 0) join = ATE::kRoundJoin;
    else {
        limit = std::atof(value);
        if (!(limit >= 1 && limit <= 500)) { stats.gate = "bad-limit"; return kBadParameterErr; }
    }
    // The selected text frames, or every text frame when none is selected.
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = SelectedOrAllFrames(*matching.operator&(), &matches, &count);
    if (err) { stats.gate = "match"; return err; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    stats.frames = count;
    if (count <= 0 || count > 64) { stats.gate = count <= 0 ? "no-text-frame" : "too-many-frames"; return kBadParameterErr; }
    for (ai::int32 i = 0; i < count; ++i) {
        TextRangeRef r = nullptr;
        err = text->GetATETextRange((*matches)[i], &r);
        if (err || !r) { stats.gate = "range"; return err ? err : kBadParameterErr; }
        ATE::CharFeaturesRef f = nullptr;
        ATE::ATEErr ate = features->Initialize(&f);
        if (!ate && f) ate = features->SetLineJoin(f, join);
        if (!ate && f && join == ATE::kMiterJoin) ate = features->SetMiterLimit(f, static_cast<ATETextDOM::Float>(limit));
        if (!ate && f) ate = range->ReplaceOrAddLocalCharFeatures(r, f);
        if (f) features->Release(f);
        range->Release(r);
        if (ate || !f) { stats.gate = "set-join"; return kBadParameterErr; }
        ++stats.styled;
    }
    stats.gate = "joined";
    return kNoErr;
}

// Whether a partial style's solid color is the same across the whole range: its kind
// and every component of that kind assigned. Gradients and patterns are never uniform here.
bool UniformSolid(const AIColor& color, const AIColorMap& map) {
    if (!map.kind) return false;
    switch (color.kind) {
    case kGrayColor: return map.c.g.gray;
    case kFourColor: return map.c.f.cyan && map.c.f.magenta && map.c.f.yellow && map.c.f.black;
    case kThreeColor: return map.c.rgb.red && map.c.rgb.green && map.c.rgb.blue;
    case kCustomColor: return map.c.c.color && map.c.c.tint;
    default: return false;
    }
}

// The native route (2026-10-15): stroke-behind written as ordinary Appearance data,
// so text edits are followed with or without the plugin. For each selected text frame
// (every frame when none is selected), from the characters' paint across the frame:
//   no stroke anywhere     -> "none", left alone
//   one fill everywhere    -> an object-level fill with that fill above Characters
//                             (character strokes, mixed or not, are kept as typed)
//   one stroke everywhere  -> an object-level stroke with that stroke below Characters,
//                             and the characters' strokes cleared
//   both mixed             -> "mixed": not expressible natively; left alone
// A frame whose art style already carries effects or more paint fields than Characters
// alone is refused ("styled"), so nothing the user built is rebuilt. Counts go to the
// appearance stats: styled = frames changed; fieldsBefore/After count fills/strokes added.
ASErr NativeRoute(SPMessageData& message, State& state) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    stats.fieldsBefore = stats.fieldsAfter = stats.contentsBefore = stats.contentsAfter = 0;
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    Suite<AIATEPaintSuite> atePaint(message.basic, kAIATEPaintSuite, kAIATEPaintVersion);
    for (ASErr e : {matching.error, memory.error, styles.error, parser.error, text.error, range.error, features.error, atePaint.error})
        if (e) { stats.gate = "suites"; return e; }
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = SelectedOrAllFrames(*matching.operator&(), &matches, &count);
    if (err) { stats.gate = "match"; return err; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    stats.frames = count;
    if (count <= 0 || count > 4096) { stats.gate = count <= 0 ? "no-text-frame" : "too-many-frames"; return kBadParameterErr; }
    int none = 0, mixed = 0, refused = 0;
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle frame = (*matches)[i];
        // The characters' paint across the whole frame; the map says what is uniform.
        AIPathStyle paint{};
        AIPathStyleMap map{};
        {
            TextRangeRef r = nullptr;
            err = text->GetATETextRange(frame, &r);
            if (err || !r) { stats.gate = "range"; return err ? err : kBadParameterErr; }
            ATE::CharFeaturesRef unique = nullptr;
            ATE::ATEErr ate = range->GetUniqueCharFeatures(r, &unique);
            if (!ate && unique) err = atePaint->GetAIPathStyleAndMap(unique, &paint, &map);
            if (unique) features->Release(unique);
            range->Release(r);
            if (ate || !unique || err) { stats.gate = "paint"; return err ? err : kBadParameterErr; }
        }
        const bool noStroke = map.strokePaint && !paint.strokePaint;
        const bool oneFill = map.fillPaint && paint.fillPaint && map.fill.overprint &&
                             UniformSolid(paint.fill.color, map.fill.color);
        const bool oneStroke = map.strokePaint && paint.strokePaint && map.stroke.overprint && map.stroke.width &&
                               map.stroke.join && map.stroke.miterLimit && map.stroke.cap && map.stroke.dash.length &&
                               UniformSolid(paint.stroke.color, map.stroke.color);
        if (noStroke) { ++none; continue; }
        if (!oneFill && !oneStroke) { ++mixed; continue; }
        AIArtStyleHandle style = nullptr;
        err = styles->GetArtStyle(frame, &style);
        if (err) { stats.gate = "get-style"; return err; }
        AIStyleParser p = nullptr;
        err = parser->NewParser(&p);
        if (err || !p) { stats.gate = "parser"; return err ? err : kOutOfMemoryErr; }
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } }
            dispose{*parser.operator&(), p};
        if (style) { err = parser->ParseStyle(p, style); if (err) { stats.gate = "parse"; return err; } }
        // Only Characters alone (an unstyled text object) is rebuilt.
        if (parser->ContainsEffects(p) || parser->CountPaintFields(p) > 2 || parser->ContainsTransparency(p)) { ++refused; continue; }
        AIParserPaintField field = nullptr;
        if (oneFill) err = parser->NewPaintFieldFill(&paint.fill, paint.evenodd, nullptr, &field);
        else err = parser->NewPaintFieldStroke(&paint.stroke, nullptr, &field);
        if (err || !field) { stats.gate = "new-field"; return err ? err : kOutOfMemoryErr; }
        err = parser->InsertNthPaintField(p, oneFill ? 0 : -1, field);
        if (err) { parser->DisposePaintField(field); stats.gate = "insert"; return err; }
        AIArtStyleHandle created = nullptr;
        err = parser->CreateNewStyle(p, &created);
        if (err || !created) { stats.gate = "create-style"; return err ? err : kBadParameterErr; }
        // Stroke below: the characters' own strokes are cleared first, so a failure leaves
        // either the frame untouched or both steps done.
        auto setStroke = [&](bool on) -> bool {
            TextRangeRef r = nullptr;
            if (text->GetATETextRange(frame, &r) || !r) return false;
            ATE::CharFeaturesRef f = nullptr;
            ATE::ATEErr ate = features->Initialize(&f);
            if (!ate && f) ate = features->SetStroke(f, on);
            if (!ate && f) ate = range->ReplaceOrAddLocalCharFeatures(r, f);
            if (f) features->Release(f);
            range->Release(r);
            return !ate && f;
        };
        if (!oneFill && !setStroke(false)) { stats.gate = "clear-stroke"; return kBadParameterErr; }
        err = styles->SetArtStyle(frame, created);
        if (err) {
            if (!oneFill) setStroke(true);   // the stroke was uniform, so turning it back on restores it
            stats.gate = "set-style";
            return err;
        }
        if (!oneFill) ++stats.fieldsAfter; else ++stats.fieldsBefore;
        ++stats.styled;
    }
    stats.contentsBefore = none;
    stats.contentsAfter = mixed * 1000 + refused;
    stats.gate = mixed || refused ? "partial" : "native";
    return kNoErr;
}

// ---- native-fix: the owner's rulings as native attributes (2026-10-15) ----------------
//
// For each selected path or compound path with a mitered stroke: one miter limit chosen by
// native_miter::ChooseLimit from its exterior corners (whole points kept, glyph tips cut),
// and, for an Outside stroke on a compound path of two or more contours, its fill moved
// above its stroke in the Appearance (the fill then hides the stroke of contour edges
// inside the filled region, the owner's Outside rule). For each selected text frame: the
// native route (NativeRoute) and one miter limit per character, chosen from that glyph's
// outline. Everything written is ordinary document data and follows edits without the
// plugin; an edit can leave a new corner with a limit chosen for the old outline.

struct FixSuites {
    const AIArtSuite& art;
    const AIPathSuite& path;
    const AIPathStyleSuite& paint;
    const AIPaintStyleSuite& paintStyle;
    const AIArtStyleSuite& styles;
    const AIArtStyleParserSuite& parser;
    const AITextFrameSuite& text;
    const ATE::TextRangeSuite& range;
    const ATE::CharFeaturesSuite& features;
    const ATE::TextFrameSuite& textFrame;
};

// Every contour of a path or compound path (closed or open), converted; false on any
// malformed or oversized path.
bool Contours(const FixSuites& s, AIArtHandle art, std::vector<ise_miter::Contour>& out, int depth = 0) {
    if (depth > 8) return false;
    short type = kUnknownArt;
    if (s.art.GetArtType(art, &type)) return false;
    if (type == kPathArt) {
        ai::int16 count = 0;
        AIBoolean closed = false;
        if (s.path.GetPathSegmentCount(art, &count) || s.path.GetPathClosed(art, &closed)) return false;
        if (count < 2 || count > 32000) return count < 2;   // a lone point has no corner
        std::vector<AIPathSegment> segs(static_cast<size_t>(count));
        if (s.path.GetPathSegments(art, 0, count, segs.data())) return false;
        ise_miter::Contour c;
        if (!illustrator_contour::ConvertSdk(segs.data(), segs.size(), closed != 0, &c)) return false;
        out.push_back(std::move(c));
        return true;
    }
    if (type == kCompoundPathArt || type == kGroupArt) {
        AIArtHandle child = nullptr;
        if (s.art.GetArtFirstChild(art, &child)) return false;
        while (child) {
            if (!Contours(s, child, out, depth + 1)) return false;
            if (s.art.GetArtSibling(child, &child)) return false;
        }
        return true;
    }
    return false;
}

void Tally(NativeFixStats& st, const native_miter::Choice& c) {
    st.keptBeveled += c.keptBeveled; st.cutMitered += c.cutMitered; st.corners += c.kept + c.cut;
}

// The fill field moved above the stroke field of a simple path style (both present).
ASErr FillAboveStroke(const FixSuites& s, AIArtHandle art, bool& changed) {
    changed = false;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    e = s.parser.ParseStyle(p, style);
    if (e) return e;
    // Only one fill and one stroke, and no effect on the fill (the stroke's own Stroke Offset
    // effect, which an Outside stroke is, is expected). Object-level effects, a drop shadow or
    // Round Corners, do not depend on the fields' order and are kept.
    if (s.parser.CountPaintFields(p) != 2) return kNoErr;
    AIParserPaintField first = nullptr, second = nullptr;
    if (s.parser.GetNthPaintField(p, 0, &first) || s.parser.GetNthPaintField(p, 1, &second)) return kNoErr;
    // Index 0 paints on top. Already fill on top: nothing to do.
    if (s.parser.IsFill(first) || !s.parser.IsFill(second) || s.parser.CountEffectsOfPaintField(second) != 0) return kNoErr;
    e = s.parser.RemovePaintField(p, second, false);
    if (e) return e;
    e = s.parser.InsertNthPaintField(p, 0, second);
    if (e) { if (s.parser.InsertNthPaintField(p, -1, second)) s.parser.DisposePaintField(second); return e; }
    AIArtStyleHandle created = nullptr;
    e = s.parser.CreateNewStyle(p, &created);
    if (e || !created) return e ? e : kBadParameterErr;
    e = s.styles.SetArtStyle(art, created);
    if (!e) changed = true;
    return e;
}

// A path's stroke alignment from its art style. Inside and Outside strokes are the live
// effect "Adobe Stroke Offset" on the stroke field, in place of its own stroking
// (AIArtStyleParser GetPaintLiveEffectInfo), with the boolean StrokeOffsetInside; no effect
// means Center. (AIPaintStyleSuite reads alignment only from the selection, and ignores a
// selection set by SetArtUserAttr.) The parameters belong to the style: releasing them
// crashed Illustrator (access violation, 2026-10-15).
ai::uint32 ReadAlignment(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art) {
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (live.error || dict.error) return kAIStrokeAlignmentCenter;
    AIArtStyleHandle style = nullptr;
    if (s.styles.GetArtStyle(art, &style) || !style) return kAIStrokeAlignmentCenter;
    AIStyleParser p = nullptr;
    if (s.parser.NewParser(&p) || !p) return kAIStrokeAlignmentCenter;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if (s.parser.ParseStyle(p, style)) return kAIStrokeAlignmentCenter;
    AIParserPaintField stroke = nullptr;
    if (s.parser.GetFocusStroke(p, &stroke) || !stroke) return kAIStrokeAlignmentCenter;
    AILiveEffectHandle handle = nullptr;
    AILiveEffectParameters params = nullptr;
    if (s.parser.GetPaintLiveEffectInfo(stroke, &handle, &params) || !handle) return kAIStrokeAlignmentCenter;
    const char* name = nullptr;
    if (live->GetLiveEffectName(handle, &name) || !name || std::strcmp(name, "Adobe Stroke Offset") != 0)
        return kAIStrokeAlignmentCenter;
    AIBoolean inside = false;
    if (params) dict->GetBooleanEntry(params, dict->Key("StrokeOffsetInside"), &inside);
    return inside ? kAIStrokeAlignmentInside : kAIStrokeAlignmentOutside;
}

ASErr FixPath(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, ai::uint32 align, NativeFixStats& st) {
    AIPathStyle style{};
    AIBoolean advanced = false;
    auto e = s.paint.GetPathStyle(art, &style, &advanced);
    if (e) return e;
    if (!style.strokePaint || style.stroke.join != kAIMiterJoin || !(style.stroke.width > 0)) return kNoErr;
    std::vector<ise_miter::Contour> contours;
    if (!Contours(s, art, contours) || contours.empty()) return kNoErr;
    // The miter's reach: half the width for Center, the whole width for Outside and Inside.
    const double h = align == kAIStrokeAlignmentCenter ? style.stroke.width / 2 : style.stroke.width;
    // Center: interior miters over the fill are visible. Inside: clipped to the fill, so none
    // escapes. Outside: no interior miter on its side.
    const auto choice = native_miter::ChooseLimitForOutline(contours, h, style.stroke.miterLimit,
                                                            align != kAIStrokeAlignmentOutside, false,
                                                            align == kAIStrokeAlignmentInside);
    Tally(st, choice);
    if (std::fabs(choice.limit - style.stroke.miterLimit) > 1e-6) {
        style.stroke.miterLimit = static_cast<AIReal>(choice.limit);
        AIPathStyle partial = style;
        e = s.paint.SetPathStyle(art, &partial);
        if (e) return e;
        ++st.pathLimits;
        // SetPathStyle must not drop an Inside or Outside stroke's Stroke Offset effect.
        if (basic && ReadAlignment(basic, s, art) != align) {
            Suite<AIPaintStyleSuite> paintStyle(basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
            if (!paintStyle.error) paintStyle->SetStrokeAlignment(art, align);
            if (ReadAlignment(basic, s, art) != align) { Trace("fix.alignment-lost", kBadParameterErr, ""); return kBadParameterErr; }
        }
    }
    // Dashes are left as they are. Writing Illustrator's "align dashes to corners" option as
    // Beautiful Strokes parameters on a Center stroke field made its stroke vanish
    // (2026-10-15); a dashed point lost in a gap stays a gap until that is understood.
    if (align == kAIStrokeAlignmentOutside && contours.size() >= 2) {
        bool changed = false;
        e = FillAboveStroke(s, art, changed);
        if (e) return e;
        if (changed) ++st.reordered;
    }
    return kNoErr;
}

// Characters that draw no glyph: spaces, breaks, and invisible formatting.
bool IsSpace(ATETextDOM::Unicode c) {
    return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == 3 || c == 0xA0 || c == 0xAD ||
           (c >= 0x2000 && c <= 0x200F) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F ||
           c == 0x2060 || c == 0x3000 || c == 0xFEFF;
}

// Whether the fill paints above the characters' strokes: a fill field above Characters (the
// fill-above route).
bool FillAboveCharacters(const FixSuites& s, AIArtHandle frame) {
    AIArtStyleHandle style = nullptr;
    if (s.styles.GetArtStyle(frame, &style) || !style) return false;
    AIStyleParser p = nullptr;
    if (s.parser.NewParser(&p) || !p) return false;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if (s.parser.ParseStyle(p, style)) return false;
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p);
    for (ai::int32 i = 0; i < contents; ++i) {
        AIParserPaintField field = nullptr;
        if (!s.parser.GetNthPaintField(p, i, &field) && field && s.parser.IsFill(field)) return true;
    }
    return false;
}

// The frame's scale (the square root of its matrix's determinant): outlines are in page
// space, a character's stroke width in its own.
double FrameScale(const FixSuites& s, AIArtHandle frame) {
    TextFrameRef tf = nullptr;
    if (s.text.GetATETextFrame(frame, &tf) || !tf) return 1;
    ATETextDOM::FloatMatrix m{};
    const bool ok = !s.textFrame.GetMatrix(tf, &m);
    s.textFrame.Release(tf);
    const double det = std::fabs(static_cast<double>(m.a) * m.d - static_cast<double>(m.b) * m.c);
    return ok && det > 1e-12 && std::isfinite(det) ? std::sqrt(det) : 1;
}

// A text frame's object-level strokes below its Characters (the stroke-below route): one
// miter limit for the frame, from every glyph outline, set on each such stroke field. The
// characters paint above, so interior miters inside the fill are hidden.
ASErr FixObjectStrokes(const FixSuites& s, AIArtHandle frame, const std::vector<ise_miter::Contour>& glyphs,
                       double scale, NativeFixStats& st) {
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(frame, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    e = s.parser.ParseStyle(p, style);
    if (e) return e;
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p);
    const ai::int32 fields = s.parser.CountPaintFields(p);
    bool changed = false;
    // Positions run top to bottom, and `contents` counts the fields above Characters: the
    // fields from index `contents` on paint below it.
    for (ai::int32 i = contents; contents >= 0 && i < fields; ++i) {
        AIParserPaintField field = nullptr;
        if (s.parser.GetNthPaintField(p, i, &field) || !field || !s.parser.IsStroke(field)) continue;
        AIStrokeStyle stroke{};
        AIArtStylePaintData data;
        if (s.parser.GetStroke(field, &stroke, &data)) continue;
        if (stroke.join != kAIMiterJoin || !(stroke.width > 0)) continue;
        const auto choice = native_miter::ChooseLimitForOutline(glyphs, stroke.width / 2 * scale, stroke.miterLimit, true, true);
        Tally(st, choice);
        if (std::fabs(choice.limit - stroke.miterLimit) <= 1e-6) continue;
        stroke.miterLimit = static_cast<AIReal>(choice.limit);
        if (s.parser.SetStroke(field, &stroke, &data)) continue;
        changed = true;
    }
    if (!changed) return kNoErr;
    AIArtStyleHandle created = nullptr;
    e = s.parser.CreateNewStyle(p, &created);
    if (e || !created) return e ? e : kBadParameterErr;
    e = s.styles.SetArtStyle(frame, created);
    if (!e) ++st.pathLimits;
    return e;
}

// One limit per character, from its glyph outline. Glyph outlines come from CreateOutline of
// the frame (first child = last glyph); characters that draw no glyph are skipped. When the
// counts disagree (ligatures, for one), each character is judged against the whole frame.
ASErr FixCharacters(const FixSuites& s, AIArtHandle frame, NativeFixStats& st) {
    TextRangeRef whole = nullptr;
    auto e = s.text.GetATETextRange(frame, &whole);
    if (e || !whole) return e ? e : kBadParameterErr;
    struct ReleaseRange { const ATE::TextRangeSuite& api; TextRangeRef r; ~ReleaseRange() { if (r) api.Release(r); } } releaseWhole{s.range, whole};
    ATETextDOM::Int32 start = 0, size = 0;
    if (s.range.GetStart(whole, &start) || s.range.GetSize(whole, &size) || size <= 0 || size > 100000) return kNoErr;
    std::vector<ATETextDOM::Unicode> chars(static_cast<size_t>(size) + 1, 0);
    ATETextDOM::Int32 got = 0;
    if (s.range.GetContents_AsUnicode(whole, chars.data(), size + 1, &got)) return kNoErr;
    // Per character (a surrogate pair is one character of two units): its outline later.
    struct Ch { int index; int length; std::vector<ise_miter::Contour> contours; };
    std::vector<Ch> drawn;
    for (ATETextDOM::Int32 i = 0; i < got && i < size; ++i) {
        const ATETextDOM::Unicode c = chars[static_cast<size_t>(i)];
        const bool pair = c >= 0xD800 && c <= 0xDBFF && i + 1 < got;
        if (!IsSpace(c)) drawn.push_back({i, pair ? 2 : 1, {}});
        if (pair) ++i;
    }
    AIArtHandle outline = nullptr;
    e = s.text.CreateOutline(frame, &outline);
    struct DisposeArt { const AIArtSuite& api; AIArtHandle a; ~DisposeArt() { if (a) api.DisposeArt(a); } } disposeOutline{s.art, outline};
    if (e || !outline) return e ? e : kBadParameterErr;
    std::vector<AIArtHandle> glyphs;
    {
        AIArtHandle child = nullptr;
        if (s.art.GetArtFirstChild(outline, &child)) return kNoErr;
        while (child) { glyphs.push_back(child); if (s.art.GetArtSibling(child, &child)) return kNoErr; }
        std::reverse(glyphs.begin(), glyphs.end());   // first child is the last glyph
    }
    // A ligature or a character drawn as several glyphs makes the counts differ: then every
    // character is judged against the whole frame's outline.
    const bool mapped = glyphs.size() == drawn.size();
    if (mapped) ++st.mapped;
    std::vector<ise_miter::Contour> allGlyphs;
    for (size_t g = 0; g < glyphs.size(); ++g) {
        std::vector<ise_miter::Contour> cs;
        if (!Contours(s, glyphs[g], cs)) continue;
        if (mapped) drawn[g].contours = cs;
        for (auto& c : cs) allGlyphs.push_back(std::move(c));
    }
    const double scale = FrameScale(s, frame);
    e = FixObjectStrokes(s, frame, allGlyphs, scale, st);
    if (e) return e;
    // With a fill above the characters, an interior miter inside the fill is hidden.
    const bool hidden = FillAboveCharacters(s, frame);
    auto charRange = [&](const Ch& c, TextRangeRef* out) -> bool {
        if (s.range.Clone(whole, out) || !*out) { *out = nullptr; return false; }
        if (s.range.SetStart(*out, start + c.index) || s.range.SetEnd(*out, start + c.index + c.length)) {
            s.range.Release(*out); *out = nullptr; return false;
        }
        return true;
    };
    auto setLimit = [&](TextRangeRef r, double limit) -> bool {
        ATE::CharFeaturesRef f = nullptr;
        ATE::ATEErr ate = s.features.Initialize(&f);
        if (!ate && f) ate = s.features.SetMiterLimit(f, static_cast<ATETextDOM::Float>(limit));
        if (!ate && f) ate = s.range.ReplaceOrAddLocalCharFeatures(r, f);
        if (f) s.features.Release(f);
        return !ate;
    };
    // Frame-level limits, one per stroke width and limit, when the outlines are not mapped.
    std::vector<std::pair<std::pair<double, double>, double>> frameLimits;
    for (const auto& c : drawn) {
        TextRangeRef r = nullptr;
        if (!charRange(c, &r)) continue;
        ++st.chars;
        ATE::CharFeaturesRef f = nullptr;
        ATE::ATEBool8 assigned = false, on = false;
        ATE::LineJoinType join = ATE::kMiterJoin;
        ATETextDOM::Float width = 0, limit = 4;
        const ATE::ATEErr got2 = s.range.GetUniqueCharFeatures(r, &f);
        if (f) {
            if (!got2) {
                s.features.GetStroke(f, &assigned, &on);
                s.features.GetLineWidth(f, &assigned, &width);
                s.features.GetMiterLimit(f, &assigned, &limit);
                s.features.GetLineJoin(f, &assigned, &join);
            }
            s.features.Release(f);
        }
        double chosen = -1;
        if (on && width > 0 && join == ATE::kMiterJoin) {
            const double h = width / 2 * scale;
            if (mapped) {
                const auto choice = native_miter::ChooseLimitForOutline(c.contours, h, limit, true, hidden);
                Tally(st, choice);
                chosen = choice.limit;
            } else {
                const auto key = std::make_pair(static_cast<double>(width), static_cast<double>(limit));
                for (const auto& fl : frameLimits) if (fl.first == key) chosen = fl.second;
                if (chosen < 0) {
                    const auto choice = native_miter::ChooseLimitForOutline(allGlyphs, h, limit, true, hidden);
                    Tally(st, choice);
                    chosen = choice.limit;
                    frameLimits.push_back({key, chosen});
                }
            }
        }
        if (chosen > 0 && std::fabs(chosen - limit) > 1e-4) {
            if (setLimit(r, chosen)) ++st.charLimits;
        }
        s.range.Release(r);
    }
    return kNoErr;
}

ASErr NativeFix(SPMessageData& message, State& state) {
    auto& st = state.nativeFix;
    st = NativeFixStats{};
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(message.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIPathStyleSuite> paint(message.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIPaintStyleSuite> paintStyle(message.basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    Suite<ATE::TextFrameSuite> textFrame(message.basic, kTextFrameSuite, kTextFrameSuiteVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    for (ASErr e : {art.error, path.error, paint.error, paintStyle.error, styles.error, parser.error, text.error,
                    range.error, features.error, textFrame.error, matching.error, memory.error})
        if (e) { st.gate = "suites"; return e; }
    FixSuites s{*art.operator&(), *path.operator&(), *paint.operator&(), *paintStyle.operator&(), *styles.operator&(),
                *parser.operator&(), *text.operator&(), *range.operator&(), *features.operator&(), *textFrame.operator&()};
    AIMatchingArtSpec specs[3] = {AIMatchingArtSpec(kPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kCompoundPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kTextFrameArt, kArtSelected, kArtSelected)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = matching->GetMatchingArt(specs, 3, &matches, &count);
    if (err) { st.gate = "match"; return err; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    if (count <= 0 || count > 100000) { st.gate = "no-selection"; return kBadParameterErr; }
    std::vector<AIArtHandle> items;
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        short ptype = kUnknownArt;
        if (!art->GetArtParent(a, &parent) && parent && !art->GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        items.push_back(a);
    }
    std::vector<ai::uint32> aligns(items.size(), kAIStrokeAlignmentCenter);
    for (size_t i = 0; i < items.size(); ++i) {
        short type = kUnknownArt;
        if (art->GetArtType(items[i], &type) || type == kTextFrameArt) continue;
        aligns[i] = ReadAlignment(message.basic, s, items[i]);
    }
    for (size_t i = 0; i < items.size(); ++i) {
        short type = kUnknownArt;
        if (art->GetArtType(items[i], &type)) continue;
        if (type == kTextFrameArt) {
            ++st.frames;
            err = FixCharacters(s, items[i], st);
            if (err) { st.gate = "characters"; return err; }
        } else {
            ++st.paths;
            err = FixPath(message.basic, s, items[i], aligns[i], st);
            if (err) { st.gate = "path"; return err; }
        }
    }
    st.gate = "fixed";
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
        "\"nativeFix\":{\"gate\":\"%s\",\"paths\":%d,\"pathLimits\":%d,\"reordered\":%d,\"frames\":%d,\"chars\":%d,"
        "\"charLimits\":%d,\"mapped\":%d,\"corners\":%d,\"keptBeveled\":%d,\"cutMitered\":%d,\"dashes\":%d},"
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
        state.nativeFix.gate, state.nativeFix.paths, state.nativeFix.pathLimits, state.nativeFix.reordered,
        state.nativeFix.frames, state.nativeFix.chars, state.nativeFix.charLimits, state.nativeFix.mapped,
        state.nativeFix.corners, state.nativeFix.keptBeveled, state.nativeFix.cutMitered, state.nativeFix.dashes,
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
    else if (std::strcmp(selector, "appearance") == 0 && std::strcmp(input, "fill-above") == 0)
        actionError = AppearanceStrokeBelow(message.d, state, AppearanceKind::FillAbove);
    else if (std::strcmp(selector, "appearance") == 0 && std::strncmp(input, "miter:", 6) == 0)
        actionError = CharacterJoin(message.d, state, input + 6);
    else if (std::strcmp(selector, "native-route") == 0 && input[0] == '\0')
        actionError = NativeRoute(message.d, state);
    else if (std::strcmp(selector, "native-fix") == 0 && input[0] == '\0')
        actionError = NativeFix(message.d, state);
    else if (std::strcmp(selector, "stroke-align") == 0) {
        // Research and census command: the stroke alignment of the selected art, which
        // scripting does not expose (AIPaintStyleSuite).
        // SetStrokeAlignmentOnSelection refuses (kBadParameterErr) when called from a
        // script, so each selected path or compound path is set on its own.
        Suite<AIPaintStyleSuite> paintStyle(message.d.basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
        Suite<AIMatchingArtSuite> matching(message.d.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
        Suite<AIMdMemorySuite> memory(message.d.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
        ai::uint32 align = 99;
        if (std::strcmp(input, "center") == 0) align = kAIStrokeAlignmentCenter;
        else if (std::strcmp(input, "inside") == 0) align = kAIStrokeAlignmentInside;
        else if (std::strcmp(input, "outside") == 0) align = kAIStrokeAlignmentOutside;
        if (paintStyle.error || matching.error || memory.error) actionError = kBadParameterErr;
        else if (align == 99) actionError = kBadParameterErr;
        else {
            AIMatchingArtSpec specs[2] = {AIMatchingArtSpec(kPathArt, kArtSelected, kArtSelected),
                                          AIMatchingArtSpec(kCompoundPathArt, kArtSelected, kArtSelected)};
            AIArtHandle** matches = nullptr;
            ai::int32 count = 0;
            actionError = matching->GetMatchingArt(specs, 2, &matches, &count);
            if (!actionError && count <= 0) actionError = kBadParameterErr;
            Suite<AIArtSuite> artS(message.d.basic, kAIArtSuite, kAIArtVersion);
            for (ai::int32 i = 0; !actionError && i < count; ++i) {
                AIArtHandle parent = nullptr;
                short ptype = kUnknownArt;
                if (!artS.error && !artS->GetArtParent((*matches)[i], &parent) && parent &&
                    !artS->GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
                actionError = paintStyle->SetStrokeAlignment((*matches)[i], align);
            }
            if (matches) memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches));
            state.appearance = AppearanceStats{};
            state.appearance.gate = actionError ? "align-failed" : "aligned";
            state.appearance.frames = count;
        }
    }
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
