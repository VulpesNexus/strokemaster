// PROPOSED region-prep-17: real-core closed-region replacement for the marked
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
#include "NativeRegion.h"
#include <algorithm>
#include "AIMatchingArt.h"
#include "AIMdMemory.h"
#include "AIPaintStyle.h"
#include "AIATEPaint.h"
#include "AIBeautifulStrokes.h"
#include "AIEntry.h"
#include "AIArray.h"
#include "AITimer.h"
#include "AIUndo.h"
#include "../source/IseRegionCore.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

namespace region_probe {
constexpr char build[] = "region-prep-17";

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
    int fillFirst = 0;   // native-route: frames given per-character stroke-behind (FillFirst off)
};

// Result of the last "native-fix" bridge command (NativeFix): the owner's miter rulings and
// the Outside rule for overlapping contours, written as native attributes.
struct NativeFixStats {
    const char* gate = "ready";
    int paths = 0, pathLimits = 0, reordered = 0, frames = 0, chars = 0, charLimits = 0, mapped = 0;
    int keptBeveled = 0, cutMitered = 0, corners = 0, dashes = 0;
    int pathfinder = 0;    // Inside strokes on overlapping contours given Pathfinder Add
    int dashRefused = 0;   // dashed strokes Illustrator's dash alignment does not accept
    int released = 0;      // re-fix: items the user had changed by hand, unmarked and left alone
    int failed = 0;        // items whose fix failed (the others were still fixed)
};

// The automatic re-fix (2026-10-16): art the native fix has written to is marked in its
// dictionary, and a timer re-runs the fix on marked art whose time stamp has changed since,
// appended to the edit's own undo step. Plain data, so State stays a raw block.
struct RefixState {
    AITimerHandle timer = nullptr;
    AIDocumentHandle document = nullptr;
    size_t global = 0;                 // the document's global stamp at the last scan
    struct Seen { AIArtHandle art; size_t stamp; };
    Seen seen[4096] = {};
    int count = 0;
    unsigned long long scans = 0, runs = 0, items = 0, deferred = 0, released = 0, overflow = 0;
    bool waiting = false;              // a changed text frame waits for typing to end
    int lastError = 0;
};

struct AlignStats {
    const char* gate = "ready";
    int native = 0, profiled = 0, doubled = 0, typeOutside = 0, restored = 0, refused = 0;
    char reasons[208] = {};
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
    AlignStats align{};
    RefixState refix{};
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
    err = RegisterOne(message, *live.operator&(), textBehindIdentity, textBehindTitle, textBehindInput,
                      state.textBehind);
    if (err) return err;
    // Twice a second, in an undoable context (the re-fix is appended to the edit's step).
    Suite<AITimerSuite> timer(message.basic, kAITimerSuite, kAITimerVersion);
    if (timer.error) return kNoErr;   // no re-fix, everything else still works
    if (timer->AddTimerWithOptions(message.self, "StrokeMaster native re-fix", kTicksPerSecond / 2,
                                   kAITimerOptionUndoableContext, &state.refix.timer))
        state.refix.timer = nullptr;
    return kNoErr;
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
// The selected text frames; every text frame only when nothing at all is selected (a selection
// of paths alone names no frame: count 0).
ASErr SelectedOrAllFrames(const AIMatchingArtSuite& matching, AIArtHandle*** matches, ai::int32* count) {
    AIMatchingArtSpec selected(kTextFrameArt, kArtSelected, kArtSelected);
    auto err = matching.GetMatchingArt(&selected, 1, matches, count);
    if (err || *count > 0) return err;
    if (matching.IsSomeArtSelected()) { *count = 0; return kNoErr; }
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

// Whether a paint field is a fill that paints: a visible color with a normal, opaque blend.
bool PaintingFill(const AIArtStyleParserSuite& parser, AIParserPaintField f) {
    if (!parser.IsFill(f)) return false;
    AIBoolean visible = true;
    AIFillStyle fill{};
    AIArtStylePaintData data;
    return !parser.GetPaintFieldVisible(f, &visible) && visible && !parser.GetFill(f, &fill, &data) &&
           fill.color.kind != kNoneColor && parser.IsNormalOpaquePaintBlend(f);
}

// The native route (2026-10-15): stroke-behind written as ordinary Appearance data,
// so text edits are followed with or without the plugin. For each selected text frame
// (every frame when none is selected), from the characters' paint across the frame:
//   no stroke anywhere     -> "none", left alone
//   one fill everywhere    -> an object-level fill with that fill above Characters
//                             (character strokes, mixed or not, are kept as typed)
//   one stroke everywhere  -> an object-level stroke with that stroke below Characters,
//                             and the characters' strokes cleared
//   both mixed             -> every character's stroke painted before its own fill (ATE
//                             FillFirst off, a native character attribute): no stroke paints
//                             over its own glyph's fill, but a later character's stroke can
//                             still reach an earlier one's fill (2026-10-16)
// A frame whose art style already carries effects or more paint fields than Characters
// alone is not rebuilt, so nothing the user built is changed; it gets FillFirst off too.
// Counts go to the appearance stats: styled = frames given an object-level paint;
// fieldsBefore/After count fills/strokes added; fillFirst = frames given FillFirst off.
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
    if (count <= 0) { stats.gate = "no-text-frame"; return kNoErr; }
    if (count > 4096) { stats.gate = "too-many-frames"; return kBadParameterErr; }
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
        // Per-character stroke-behind, for the frames no object-level paint serves.
        auto fillFirstOff = [&]() -> ASErr {
            TextRangeRef r = nullptr;
            if (text->GetATETextRange(frame, &r) || !r) return kBadParameterErr;
            ATE::CharFeaturesRef f = nullptr;
            ATE::ATEErr ate = features->Initialize(&f);
            if (!ate && f) ate = features->SetFillFirst(f, false);
            if (!ate && f) ate = range->ReplaceOrAddLocalCharFeatures(r, f);
            if (f) features->Release(f);
            range->Release(r);
            if (ate || !f) return kBadParameterErr;
            ++stats.fillFirst;
            return kNoErr;
        };
        if (!oneFill && !oneStroke) {
            ++mixed;
            if (fillFirstOff()) { stats.gate = "fill-first"; return kBadParameterErr; }
            continue;
        }
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
        if (parser->ContainsEffects(p) || parser->CountPaintFields(p) > 2 || parser->ContainsTransparency(p)) {
            ++refused;
            // A fill already above Characters covers every stroke; otherwise FillFirst off.
            bool fillAbove = false;
            for (ai::int32 k = 0, top = parser->GetGroupContentsPosition(p); k < top; ++k) {
                AIParserPaintField f = nullptr;
                if (!parser->GetNthPaintField(p, k, &f) && f && PaintingFill(*parser.operator&(), f)) fillAbove = true;
            }
            if (!fillAbove && fillFirstOff()) { stats.gate = "fill-first"; return kBadParameterErr; }
            continue;
        }
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
    stats.gate = "native";
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

// Illustrator's own "Align dashes to corners and path ends" on a dashed stroke, as the Stroke
// panel stores it (read back 2026-10-16): the stroke field's live-effect parameters carry a
// Beautiful Strokes dictionary with BSDashAdj on, beside a StrokeStyle entry, and no live effect
// for a Center stroke. Dashes then fit between the path's corner anchors and ends (a smooth
// anchor does not anchor them), so a dashed point is drawn whole. Inside and Outside strokes are
// a live effect of their own (Adobe Stroke Offset); Beautiful Strokes declines them
// (GetArtStrokeParams canAccept), so they are counted, not changed.
ASErr AlignDashes(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, bool& changed, bool& refused) {
    changed = refused = false;
    Suite<AIBeautifulStrokesSuite> bs(basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIEntrySuite> entry(basic, kAIEntrySuite, kAIEntryVersion);
    if (bs.error || live.error || dict.error || entry.error) return kNoErr;
    {
        AIDictionaryRef current = nullptr;
        AIBoolean accept = false;
        const auto got = bs->GetArtStrokeParams(art, current, nullptr, nullptr, nullptr, &accept);
        const bool already = !got && current && bs->GetDashAdjustment(current, nullptr);
        if (current) dict->Release(current);
        if (got) return kNoErr;
        if (already) return kNoErr;
        if (!accept) { refused = true; return kNoErr; }
    }
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e;
    AILiveEffectParameters mine = nullptr;
    {
        AIStyleParser p = nullptr;
        e = s.parser.NewParser(&p);
        if (e || !p) return e ? e : kOutOfMemoryErr;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
        e = s.parser.ParseStyle(p, style);
        if (e) return e;
        AIParserPaintField stroke = nullptr;
        if (s.parser.GetFocusStroke(p, &stroke) || !stroke) return kNoErr;
        AILiveEffectHandle handle = nullptr;
        AILiveEffectParameters params = nullptr;
        // An Inside or Outside stroke (the live effect Adobe Stroke Offset) ignores the option even
        // when it is written into the effect's own parameters (census, 2026-10-16).
        if (s.parser.GetPaintLiveEffectInfo(stroke, &handle, &params) || handle) { refused = true; return kNoErr; }
        AIStrokeStyle strokeStyle{};
        AIArtStylePaintData data;
        if (s.parser.GetStroke(stroke, &strokeStyle, &data)) return kNoErr;
        // The field's parameters belong to the style: they are cloned, never released.
        if (params ? dict->Clone(params, &mine) : live->CreateLiveEffectParameters(&mine)) return kNoErr;
        if (!mine) return kNoErr;
        struct ReleaseMine { const AIDictionarySuite& d; AILiveEffectParameters& m; ~ReleaseMine() { if (m) d.Release(m); } }
            releaseMine{*dict.operator&(), mine};
        AIDictionaryRef inner = nullptr;
        e = bs->CreateParamsDict(mine, inner);
        if (!e && inner) e = bs->SetDashAdjustment(inner, true);
        if (inner) dict->Release(inner);
        if (e) return kNoErr;
        if (!dict->IsKnown(mine, dict->Key("StrokeStyle"))) {
            AIEntryRef se = entry->FromStrokeStyle(&strokeStyle);
            if (!se || dict->Set(mine, dict->Key("StrokeStyle"), se)) return kNoErr;
        }
        e = s.parser.SetPaintLiveEffectInfo(stroke, nullptr, mine);
        if (e) return e;
        AIArtStyleHandle created = nullptr;
        e = s.parser.CreateNewStyle(p, &created);
        if (e || !created) return e ? e : kBadParameterErr;
        e = s.styles.SetArtStyle(art, created);
        if (e) return e;
    }
    // Read back: the option must be on, and the stroke still a plain stroke.
    AIDictionaryRef now = nullptr;
    AIBoolean accept = false;
    AIStrokeStyle after{};
    const auto got = bs->GetArtStrokeParams(art, now, &after, nullptr, nullptr, &accept);
    const bool on = !got && now && bs->GetDashAdjustment(now, nullptr);
    if (now) dict->Release(now);
    // Not taken: counted as refused, and the rest of the fix goes on.
    if (!on || !(after.width > 0) || after.dash.length <= 0) { Trace("fix.dash-readback", kBadParameterErr, ""); refused = true; return kNoErr; }
    changed = true;
    return kNoErr;
}

// Illustrator's own Effect > Pathfinder > Add as a pre-effect of the path's style, with the
// parameters Illustrator writes for it (read back 2026-10-16): the contours are united before
// they are painted, so an Inside stroke follows only the filled region's boundary, the owner's
// rule, natively and live (Adobe's effect, present in every Illustrator). It goes after the
// user's own pre-effects, so they act on the contours as before and the stroke follows the
// region they fill. The fix's own effect carries kOwnedKey in its parameters: a Pathfinder Add the
// user put there is never removed, and serves as the union when present (nothing is added then).
// `want` false removes the fix's own (the re-fix, when the path no longer needs it). `owned` says
// whether the fix's own is there on return.
constexpr char kOwnedKey[] = "StrokeMasterNativeFix";

ASErr SetPathfinder(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, bool want, bool& owned, bool& changed) {
    changed = owned = false;
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (live.error || dict.error) return kNoErr;
    AILiveEffectHandle pathfinder = nullptr;
    if (live->GetLiveEffectHandleByName("Adobe Pathfinder", &pathfinder) || !pathfinder) return kNoErr;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e;
    AILiveEffectParameters params = nullptr;
    if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
    struct ReleaseParams { const AIDictionarySuite& d; AILiveEffectParameters p; ~ReleaseParams() { d.Release(p); } }
        releaseParams{*dict.operator&(), params};
    {
        AIStyleParser p = nullptr;
        e = s.parser.NewParser(&p);
        if (e || !p) return e ? e : kOutOfMemoryErr;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
        e = s.parser.ParseStyle(p, style);
        if (e) return e;
        // The style's parameters belong to it and are only read here.
        AIParserLiveEffect own = nullptr;
        bool users = false;
        for (ai::int32 i = 0; i < s.parser.CountPreEffects(p); ++i) {
            AIParserLiveEffect fx = nullptr;
            AILiveEffectHandle h = nullptr;
            ai::int32 command = -1, tag = 0;
            AILiveEffectParameters fxParams = nullptr;
            if (s.parser.GetNthPreEffect(p, i, &fx) || !fx || s.parser.GetLiveEffectHandle(fx, &h) || h != pathfinder ||
                s.parser.GetLiveEffectParams(fx, &fxParams) || !fxParams ||
                dict->GetIntegerEntry(fxParams, dict->Key("Command"), &command) || command != 0)
                continue;
            if (!dict->GetIntegerEntry(fxParams, dict->Key(kOwnedKey), &tag) && tag == 1) own = fx;
            else users = true;
        }
        owned = own != nullptr;
        if (want ? (owned || users) : !owned) return kNoErr;
        if (!want) {
            e = s.parser.RemovePreEffect(p, own, true);
            if (e) return e;
        } else {
            const auto key = [&](const char* k) { return dict->Key(k); };
            dict->SetIntegerEntry(params, key("Command"), 0);   // Add
            dict->SetRealEntry(params, key("Mix"), static_cast<AIReal>(0.5));
            dict->SetRealEntry(params, key("Precision"), 10);
            dict->SetBooleanEntry(params, key("ExtractUnpainted"), true);
            dict->SetBooleanEntry(params, key("RemovePoints"), false);
            dict->SetBooleanEntry(params, key("ConvertCustom"), true);
            dict->SetBooleanEntry(params, key("TrapConvertCustom"), false);
            dict->SetBooleanEntry(params, key("TrapReverse"), false);
            dict->SetRealEntry(params, key("TrapThickness"), static_cast<AIReal>(0.25));
            dict->SetRealEntry(params, key("TrapTint"), static_cast<AIReal>(0.4));
            dict->SetRealEntry(params, key("TrapMaxTint"), 1);
            dict->SetRealEntry(params, key("TrapAspect"), 1);
            dict->SetRealEntry(params, key("TrapTintTolerance"), static_cast<AIReal>(0.05));
            dict->SetBooleanEntry(params, key("-DefaultApplyEffectsKey"), false);
            dict->SetIntegerEntry(params, key(kOwnedKey), 1);
            // DisplayString (the Appearance panel's label) is left for Illustrator to fill in.
            AIParserLiveEffect fx = nullptr;
            e = s.parser.NewParserLiveEffect(pathfinder, params, &fx);
            if (e || !fx) return e ? e : kOutOfMemoryErr;
            e = s.parser.InsertNthPreEffect(p, s.parser.CountPreEffects(p), fx);
            if (e) { s.parser.DisposeParserLiveEffect(fx); return e; }
        }
        AIArtStyleHandle created = nullptr;
        e = s.parser.CreateNewStyle(p, &created);
        if (e || !created) return e ? e : kBadParameterErr;
        e = s.styles.SetArtStyle(art, created);
        if (e) return e;
    }
    owned = want;
    changed = true;
    return kNoErr;
}

// Whether a path's stroke has Illustrator's dash alignment on.
bool DashesOn(SPBasicSuite* basic, AIArtHandle art) {
    Suite<AIBeautifulStrokesSuite> bs(basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (bs.error || dict.error) return false;
    AIDictionaryRef params = nullptr;
    const auto got = bs->GetArtStrokeParams(art, params, nullptr, nullptr, nullptr, nullptr);
    const bool on = !got && params && bs->GetDashAdjustment(params, nullptr);
    if (params) dict->Release(params);
    return on;
}

// What native-align made of an item where Illustrator draws no stroke alignment (2026-10-16),
// kept in its dictionary: how (kAlignAs), the side a one-sided profile stands for (kAlignSide), and
// the weight to restore (kAlignWeight).
constexpr char kAlignAs[] = "StrokeMasterAlignAs";         // "profile", "doubled", "type-outside", "type-chars"
constexpr char kAlignSide[] = "StrokeMasterAlignSide";     // 1 Outside, 2 Inside
constexpr char kAlignWeight[] = "StrokeMasterAlignWeight";

struct AlignRecord { std::string as; int side = 0; double weight = 0; };

AlignRecord ReadAlignRecord(SPBasicSuite* basic, const AIArtSuite& art, AIArtHandle item) {
    AlignRecord r;
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (dict.error || !art.HasDictionary(item)) return r;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return r;
    const char* as = nullptr;
    if (!dict->GetStringEntry(d, dict->Key(kAlignAs), &as) && as) r.as = as;
    ai::int32 side = 0;
    if (!dict->GetIntegerEntry(d, dict->Key(kAlignSide), &side)) r.side = side;
    AIReal w = 0;
    if (!dict->GetRealEntry(d, dict->Key(kAlignWeight), &w)) r.weight = w;
    dict->Release(d);
    return r;
}

void WriteAlignRecord(SPBasicSuite* basic, const AIArtSuite& art, AIArtHandle item, const AlignRecord& r) {
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (dict.error) return;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return;
    if (r.as.empty()) {
        for (const char* k : {kAlignAs, kAlignSide, kAlignWeight}) dict->DeleteEntry(d, dict->Key(k));
    } else {
        dict->SetStringEntry(d, dict->Key(kAlignAs), r.as.c_str());
        dict->SetIntegerEntry(d, dict->Key(kAlignSide), r.side);
        dict->SetRealEntry(d, dict->Key(kAlignWeight), static_cast<AIReal>(r.weight));
    }
    dict->Release(d);
}

// Whether a path's style paints its fill above its stroke: two fields, a painting fill first.
bool FillOnTop(const FixSuites& s, AIArtHandle art) {
    AIArtStyleHandle style = nullptr;
    if (s.styles.GetArtStyle(art, &style) || !style) return false;
    AIStyleParser p = nullptr;
    if (s.parser.NewParser(&p) || !p) return false;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if (s.parser.ParseStyle(p, style) || s.parser.CountPaintFields(p) != 2) return false;
    AIParserPaintField first = nullptr, second = nullptr;
    return !s.parser.GetNthPaintField(p, 0, &first) && first && PaintingFill(s.parser, first) &&
           !s.parser.GetNthPaintField(p, 1, &second) && second && s.parser.IsStroke(second);
}

// What a fill (two fields, a painting fill and a stroke) allows: whether the style is that simple.
bool SimpleFillAndStroke(const FixSuites& s, AIArtHandle art) {
    AIArtStyleHandle style = nullptr;
    if (s.styles.GetArtStyle(art, &style) || !style) return false;
    AIStyleParser p = nullptr;
    if (s.parser.NewParser(&p) || !p) return false;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if (s.parser.ParseStyle(p, style) || s.parser.CountPaintFields(p) != 2 || s.parser.ContainsTransparency(p)) return false;
    int fills = 0, strokes = 0;
    for (ai::int32 i = 0; i < 2; ++i) {
        AIParserPaintField f = nullptr;
        if (s.parser.GetNthPaintField(p, i, &f) || !f) return false;
        // A fill with an effect of its own is not moved (FillAboveStroke): refused here, first.
        if (PaintingFill(s.parser, f) && s.parser.CountEffectsOfPaintField(f) == 0) ++fills;
        else if (s.parser.IsStroke(f)) ++strokes;
    }
    return fills == 1 && strokes == 1;
}

// The stroke field moved back above the fill (FillAboveStroke undone).
ASErr StrokeAboveFill(const FixSuites& s, AIArtHandle art) {
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    e = s.parser.ParseStyle(p, style);
    if (e || s.parser.CountPaintFields(p) != 2) return e;
    AIParserPaintField first = nullptr, second = nullptr;
    if (s.parser.GetNthPaintField(p, 0, &first) || s.parser.GetNthPaintField(p, 1, &second)) return kNoErr;
    if (!s.parser.IsFill(first) || !s.parser.IsStroke(second)) return kNoErr;
    e = s.parser.RemovePaintField(p, second, false);
    if (e) return e;
    e = s.parser.InsertNthPaintField(p, 0, second);
    if (e) { if (s.parser.InsertNthPaintField(p, -1, second)) s.parser.DisposePaintField(second); return e; }
    AIArtStyleHandle created = nullptr;
    e = s.parser.CreateNewStyle(p, &created);
    if (e || !created) return e ? e : kBadParameterErr;
    return s.styles.SetArtStyle(art, created);
}

// What the fix leaves on an item, kept in its dictionary beside the mark: the automatic re-fix
// compares it with what the item carries before fixing again, and a difference is the user's own
// change (a limit set by hand, Pathfinder or dash alignment removed), which it respects.
struct FixRecord {
    bool hasLimit = false;
    double limit = 0;           // a path's miter limit after the fix
    bool pathfinder = false;    // the fix's own Pathfinder Add present after the fix
    bool dashes = false;        // dashes aligned after the fix
    std::set<long> charLimits;  // every limit a frame's text runs carry after the fix, in thousandths
    // The user's own limit, the one the rulings are judged against (within it a glyph tip is
    // trimmed, past it only a whole point is kept): the limit before the fix first changed it.
    // Without it, a later fix would take the limit it wrote for the user's and judge again.
    double userLimit = 0;       // a path's (0: unknown)
    double charUserLimit = 0;   // a frame's, when one limit ran through it (0: each character's own)
    bool operator==(const FixRecord& o) const {
        return hasLimit == o.hasLimit && (!hasLimit || std::fabs(limit - o.limit) <= 1e-4) && pathfinder == o.pathfinder &&
               dashes == o.dashes && charLimits == o.charLimits && std::fabs(userLimit - o.userLimit) <= 1e-4 &&
               std::fabs(charUserLimit - o.charUserLimit) <= 1e-4;
    }
};

// The region an Inside stroke follows once Pathfinder Add has united the contours, for the miter
// limit: NonzeroRegion rebuilds it from the contours' own segments, curves included.
std::vector<ise_miter::Contour> UnitedOutline(const std::vector<ise_miter::Contour>& contours) {
    auto out = ise_region::NonzeroRegion(contours, 0.01);
    return out.empty() ? contours : out;
}

// The native fix on one path or compound path. The miter limit is written first (SetPathStyle,
// which rebuilds the plain style), then what lives in the style's parser data: Pathfinder Add,
// dash alignment, and the Outside paint order. `prior` is the item's record from an earlier fix,
// if any; `rec` returns this one.
ASErr FixPath(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, ai::uint32 align, NativeFixStats& st,
              const FixRecord* prior, FixRecord& rec) {
    // Kept as it was where a step does not run (no stroke, an unreadable outline), so the fix's
    // own Pathfinder Add stays known as its own.
    rec = prior ? *prior : FixRecord{};
    rec.charLimits.clear();
    AIPathStyle style{};
    AIBoolean advanced = false;
    auto e = s.paint.GetPathStyle(art, &style, &advanced);
    if (e) return e;
    if (!style.strokePaint || !(style.stroke.width > 0)) return kNoErr;
    std::vector<ise_miter::Contour> contours;
    if (!Contours(s, art, contours) || contours.empty()) return kNoErr;
    // How the stroke is drawn, as against the alignment Illustrator holds: a Center stroke under
    // its fill shows its outer half only (Outside at half the weight: native-align's doubled
    // stroke, or the user's own order), and a one-sided profile (native-align on an open path)
    // lies on its recorded side at the full weight.
    double reachScale = align == kAIStrokeAlignmentCenter ? 0.5 : 1;
    ai::uint32 drawn = align;
    // A Center stroke under its fill still has its inner half: its interior miters are hidden
    // while they stay in the fill, and cut where a thin part lets one out (as text under a fill).
    bool underFill = false;
    bool skipLimit = false;
    if (basic && align == kAIStrokeAlignmentCenter) {
        const AlignRecord ar = ReadAlignRecord(basic, s.art, art);
        bool profiled = false;
        if (ar.as == "profile") {
            // Only while the one-sided profile is still there (the user may have reset it).
            Suite<AIBeautifulStrokesSuite> bs(basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
            Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
            if (!bs.error && !dict.error) {
                AIDictionaryRef params = nullptr;
                const auto got = bs->GetArtStrokeParams(art, params, nullptr, nullptr, nullptr, nullptr);
                profiled = !got && params && bs->GetWidthSource(params) == kUserProfile;
                if (params) dict->Release(params);
            }
        }
        if (profiled && ar.side == 1) { drawn = kAIStrokeAlignmentOutside; reachScale = 1; }
        // An Inside profile draws only the joins on its inner side, which the corner rules here
        // do not single out: its limit is left as it is.
        else if (profiled && ar.side == 2) skipLimit = true;
        // Under an even-odd fill an interior miter can point into a hole and show.
        else if (FillOnTop(s, art) && !style.evenodd) underFill = true;
    }
    // Inside on overlapping contours (any join): united first, by Illustrator's Pathfinder.
    const bool unite = align == kAIStrokeAlignmentInside && !style.evenodd && style.fillPaint &&
                       native_region::InternalEdges(contours, 0.01, style.stroke.width / 2);
    if (style.stroke.join == kAIMiterJoin && !skipLimit) {
        // The miter's reach: half the width for Center, the whole width for Outside and Inside.
        const double h = style.stroke.width * reachScale;
        // Judged against the user's own limit while the limit is still the one the fix wrote.
        double ruling = style.stroke.miterLimit;
        if (prior && prior->hasLimit && prior->userLimit > 0 && std::fabs(style.stroke.miterLimit - prior->limit) <= 1e-3)
            ruling = prior->userLimit;
        rec.userLimit = ruling;
        // Center: interior miters over the fill are visible. Inside: clipped to the fill, so none
        // escapes. Outside: no interior miter on its side. United: the corners actually drawn.
        const auto choice = native_miter::ChooseLimitForOutline(unite ? UnitedOutline(contours) : contours, h,
                                                                ruling, drawn != kAIStrokeAlignmentOutside,
                                                                underFill, align == kAIStrokeAlignmentInside);
        Tally(st, choice);
        rec.hasLimit = true;
        rec.limit = style.stroke.miterLimit;
        if (std::fabs(choice.limit - style.stroke.miterLimit) > 1e-6) {
            style.stroke.miterLimit = static_cast<AIReal>(choice.limit);
            AIPathStyle partial = style;
            e = s.paint.SetPathStyle(art, &partial);
            if (e) return e;
            ++st.pathLimits;
            rec.limit = choice.limit;
            // SetPathStyle must not drop an Inside or Outside stroke's Stroke Offset effect.
            if (basic && ReadAlignment(basic, s, art) != align) {
                Suite<AIPaintStyleSuite> paintStyle(basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
                if (!paintStyle.error) paintStyle->SetStrokeAlignment(art, align);
                if (ReadAlignment(basic, s, art) != align) { Trace("fix.alignment-lost", kBadParameterErr, ""); return kBadParameterErr; }
            }
        }
    }
    // Pathfinder Add where it is needed; one the fix added earlier comes off when it no longer is.
    if (basic && (unite || rec.pathfinder)) {
        bool owned = false, changed = false;
        e = SetPathfinder(basic, s, art, unite, owned, changed);
        if (e) return e;
        if (changed && unite) ++st.pathfinder;
        rec.pathfinder = owned;
    }
    // Dashed: dashes fitted between corners and ends (any join).
    if (basic && style.stroke.dash.length > 0) {
        bool aligned = false, refusedDash = false;
        e = AlignDashes(basic, s, art, aligned, refusedDash);
        if (e) return e;
        if (aligned) ++st.dashes;
        if (refusedDash) ++st.dashRefused;
        rec.dashes = DashesOn(basic, art);
    }
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

// Whether the fill paints above the characters' strokes: a painting fill field above Characters
// (the fill-above route).
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
        if (!s.parser.GetNthPaintField(p, i, &field) && field && PaintingFill(s.parser, field)) return true;
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
ASErr FixCharacters(const FixSuites& s, AIArtHandle frame, NativeFixStats& st, const FixRecord* prior = nullptr,
                    double* userLimitOut = nullptr) {
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
    if (e || !outline) return kNoErr;   // nothing drawn (spaces only, overset): nothing to fix
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
    // The user's own limit for the rulings: the frame's, recorded at the first fix when one limit
    // ran through the frame, for every character still carrying a limit the fix left (typing
    // copies one); otherwise the character's own.
    double frameUser = prior ? prior->charUserLimit : 0;
    if (!(frameUser > 0) && !prior) {
        ATE::CharFeaturesRef u = nullptr;
        ATE::ATEBool8 assigned = false;
        ATETextDOM::Float l = 0;
        if (!s.range.GetUniqueCharFeatures(whole, &u) && u && !s.features.GetMiterLimit(u, &assigned, &l) && assigned) frameUser = l;
        if (u) s.features.Release(u);
    }
    if (userLimitOut) *userLimitOut = frameUser;
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
        ATE::ATEBool8 fillFirst = true;
        const ATE::ATEErr got2 = s.range.GetUniqueCharFeatures(r, &f);
        if (f) {
            if (!got2) {
                if (s.features.GetFillFirst(f, &assigned, &fillFirst) || !assigned) fillFirst = true;
                s.features.GetStroke(f, &assigned, &on);
                s.features.GetLineWidth(f, &assigned, &width);
                s.features.GetMiterLimit(f, &assigned, &limit);
                s.features.GetLineJoin(f, &assigned, &join);
            }
            s.features.Release(f);
        }
        double chosen = -1;
        const ATETextDOM::Float current = limit;
        if (frameUser > 0 && (!prior || prior->charLimits.count(std::lround(static_cast<double>(limit) * 1000))))
            limit = static_cast<ATETextDOM::Float>(frameUser);
        if (on && width > 0 && join == ATE::kMiterJoin) {
            const double h = width / 2 * scale;
            if (mapped) {
                // Its own fill paints over its stroke when FillFirst is off: an interior miter
                // that stays inside the glyph is hidden.
                const auto choice = native_miter::ChooseLimitForOutline(c.contours, h, limit, true, hidden || !fillFirst);
                Tally(st, choice);
                chosen = choice.limit;
            } else {
                const auto key = std::make_pair(static_cast<double>(width), static_cast<double>(limit));
                for (const auto& fl : frameLimits) if (fl.first == key) chosen = fl.second;
                if (chosen < 0) {
                    // Against the whole frame, a miter inside a neighbour's fill is not hidden by
                    // FillFirst (only a glyph's own fill paints over its stroke): frame fill only.
                    const auto choice = native_miter::ChooseLimitForOutline(allGlyphs, h, limit, true, hidden);
                    Tally(st, choice);
                    chosen = choice.limit;
                    frameLimits.push_back({key, chosen});
                }
            }
        }
        if (chosen > 0 && std::fabs(chosen - current) > 1e-4) {
            if (setLimit(r, chosen)) ++st.charLimits;
        }
        s.range.Release(r);
    }
    return kNoErr;
}

// The art dictionary keys: the mark (art the native fix has written to, which the automatic re-fix
// follows) and the record of what the fix left (FixRecord).
constexpr char kNativeMark[] = "StrokeMasterNativeFix";
constexpr char kRecordLimit[] = "StrokeMasterLimit";
constexpr char kRecordPathfinder[] = "StrokeMasterPathfinder";
constexpr char kRecordDashes[] = "StrokeMasterDashes";
constexpr char kRecordCharLimits[] = "StrokeMasterCharLimits";
constexpr char kRecordUserLimit[] = "StrokeMasterUserLimit";
constexpr char kRecordCharUserLimit[] = "StrokeMasterCharUserLimit";

void Mark(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle item, const FixRecord& rec) {
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return;
    dict.SetIntegerEntry(d, dict.Key(kNativeMark), 1);
    if (rec.hasLimit) dict.SetRealEntry(d, dict.Key(kRecordLimit), static_cast<AIReal>(rec.limit));
    else dict.DeleteEntry(d, dict.Key(kRecordLimit));
    dict.SetBooleanEntry(d, dict.Key(kRecordPathfinder), rec.pathfinder);
    dict.SetBooleanEntry(d, dict.Key(kRecordDashes), rec.dashes);
    std::string limits;
    for (long v : rec.charLimits) { if (!limits.empty()) limits += ","; limits += std::to_string(v); }
    dict.SetStringEntry(d, dict.Key(kRecordCharLimits), limits.c_str());
    if (rec.userLimit > 0) dict.SetRealEntry(d, dict.Key(kRecordUserLimit), static_cast<AIReal>(rec.userLimit));
    else dict.DeleteEntry(d, dict.Key(kRecordUserLimit));
    if (rec.charUserLimit > 0) dict.SetRealEntry(d, dict.Key(kRecordCharUserLimit), static_cast<AIReal>(rec.charUserLimit));
    else dict.DeleteEntry(d, dict.Key(kRecordCharUserLimit));
    dict.Release(d);
}

void Unmark(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle item) {
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return;
    for (const char* k : {kNativeMark, kRecordLimit, kRecordPathfinder, kRecordDashes, kRecordCharLimits, kRecordUserLimit,
                          kRecordCharUserLimit})
        dict.DeleteEntry(d, dict.Key(k));
    dict.Release(d);
}

// Whether the item is marked, and its record.
bool Marked(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle item, FixRecord* rec = nullptr) {
    if (!art.HasDictionary(item)) return false;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return false;
    ai::int32 v = 0;
    const bool marked = !dict.GetIntegerEntry(d, dict.Key(kNativeMark), &v) && v == 1;
    if (marked && rec) {
        *rec = FixRecord{};
        AIReal limit = 0;
        if (!dict.GetRealEntry(d, dict.Key(kRecordLimit), &limit)) { rec->hasLimit = true; rec->limit = limit; }
        AIReal user = 0;
        if (!dict.GetRealEntry(d, dict.Key(kRecordUserLimit), &user)) rec->userLimit = user;
        if (!dict.GetRealEntry(d, dict.Key(kRecordCharUserLimit), &user)) rec->charUserLimit = user;
        AIBoolean b = false;
        if (!dict.GetBooleanEntry(d, dict.Key(kRecordPathfinder), &b)) rec->pathfinder = b;
        if (!dict.GetBooleanEntry(d, dict.Key(kRecordDashes), &b)) rec->dashes = b;
        const char* text = nullptr;
        if (!dict.GetStringEntry(d, dict.Key(kRecordCharLimits), &text) && text)
            for (const char* q = text; *q;) { char* end = nullptr; const long n = std::strtol(q, &end, 10); if (end == q) break; rec->charLimits.insert(n); q = *end == ',' ? end + 1 : end; }
    }
    dict.Release(d);
    return marked;
}

// Every miter limit a frame's text runs carry, in thousandths of the single-precision value
// (spaces and unstroked runs too, so typing anywhere copies a recorded value).
std::set<long> RunLimits(SPBasicSuite* basic, const FixSuites& s, AIArtHandle frame) {
    std::set<long> out;
    Suite<ATE::TextRunsIteratorSuite> iter(basic, kTextRunsIteratorSuite, kTextRunsIteratorSuiteVersion);
    if (iter.error) return out;
    TextRangeRef whole = nullptr;
    if (s.text.GetATETextRange(frame, &whole) || !whole) return out;
    ATE::TextRunsIteratorRef runs = nullptr;
    if (!s.range.GetTextRunsIterator(whole, &runs) && runs) {
        for (ATE::ATEBool8 done = false; !iter->IsDone(runs, &done) && !done; iter->Next(runs)) {
            TextRangeRef run = nullptr;
            if (iter->Item(runs, &run) || !run) break;
            ATE::CharFeaturesRef f = nullptr;
            if (!s.range.GetUniqueCharFeatures(run, &f) && f) {
                ATE::ATEBool8 assigned = false;
                ATETextDOM::Float limit = 0;
                if (!s.features.GetMiterLimit(f, &assigned, &limit)) out.insert(std::lround(static_cast<double>(limit) * 1000));
            }
            if (f) s.features.Release(f);
            s.range.Release(run);
        }
        iter->Release(runs);
    }
    s.range.Release(whole);
    return out;
}

// Whether the user has changed what the fix manages since it ran: a path's miter limit, the fix's
// Pathfinder Add or dash alignment taken off, or a character limit the fix did not leave.
bool Overridden(SPBasicSuite* basic, const FixSuites& s, AIArtHandle item, const FixRecord& rec) {
    short type = kUnknownArt;
    if (s.art.GetArtType(item, &type)) return false;
    if (type == kTextFrameArt) {
        // Every run's limit must be one the fix left: typing copies a neighbour's, a font, size,
        // or stroke change keeps it; a limit set by hand is new.
        if (rec.charLimits.empty()) return false;
        for (long v : RunLimits(basic, s, item)) if (!rec.charLimits.count(v)) return true;
        return false;
    }
    AIPathStyle style{};
    AIBoolean advanced = false;
    if (s.paint.GetPathStyle(item, &style, &advanced)) return false;
    if (rec.hasLimit && style.stroke.join == kAIMiterJoin && std::fabs(style.stroke.miterLimit - rec.limit) > 1e-3) return true;
    if (rec.pathfinder) {
        // The fix's own Pathfinder Add taken off.
        Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
        Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
        AILiveEffectHandle pathfinder = nullptr;
        AIArtStyleHandle st = nullptr;
        AIStyleParser p = nullptr;
        bool present = false;
        if (!live.error && !dict.error && !live->GetLiveEffectHandleByName("Adobe Pathfinder", &pathfinder) && pathfinder &&
            !s.styles.GetArtStyle(item, &st) && st && !s.parser.NewParser(&p) && p) {
            if (!s.parser.ParseStyle(p, st))
                for (ai::int32 i = 0; i < s.parser.CountPreEffects(p); ++i) {
                    AIParserLiveEffect fx = nullptr;
                    AILiveEffectHandle h = nullptr;
                    AILiveEffectParameters fxParams = nullptr;
                    ai::int32 tag = 0;
                    if (!s.parser.GetNthPreEffect(p, i, &fx) && fx && !s.parser.GetLiveEffectHandle(fx, &h) && h == pathfinder &&
                        !s.parser.GetLiveEffectParams(fx, &fxParams) && fxParams &&
                        !dict->GetIntegerEntry(fxParams, dict->Key(kOwnedKey), &tag) && tag == 1)
                        present = true;
                }
            s.parser.DisposeParser(p);
            if (!present) return true;
        }
    }
    // The fix's dash alignment turned off.
    if (rec.dashes && style.stroke.dash.length > 0 && !DashesOn(basic, item)) return true;
    return false;
}

// The native fix on a list of paths, compound paths, and text frames. One item's failure is
// counted and traced, and the others are still fixed. `refix`: the automatic re-fix, which first
// leaves alone (and unmarks) items whose managed attributes the user has changed. `done` returns
// the items fixed (re-fix bookkeeping).
ASErr FixItems(SPBasicSuite* basic, const FixSuites& s, const std::vector<AIArtHandle>& items, NativeFixStats& st,
               bool refix = false, std::vector<AIArtHandle>* done = nullptr) {
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (dict.error) { st.gate = "suites"; return dict.error; }
    std::vector<ai::uint32> aligns(items.size(), kAIStrokeAlignmentCenter);
    for (size_t i = 0; i < items.size(); ++i) {
        short type = kUnknownArt;
        if (s.art.GetArtType(items[i], &type) || type == kTextFrameArt) continue;
        aligns[i] = ReadAlignment(basic, s, items[i]);
    }
    int failed = 0;
    ASErr first = kNoErr;
    for (size_t i = 0; i < items.size(); ++i) {
        short type = kUnknownArt;
        if (s.art.GetArtType(items[i], &type)) { ++failed; continue; }
        FixRecord prior;
        const bool hasPrior = Marked(s.art, *dict.operator&(), items[i], &prior);
        if (refix && hasPrior && Overridden(basic, s, items[i], prior)) {
            Unmark(s.art, *dict.operator&(), items[i]);
            ++st.released;
            continue;
        }
        FixRecord rec = hasPrior ? prior : FixRecord{};
        ASErr err = kNoErr;
        if (type == kTextFrameArt) {
            ++st.frames;
            double userLimit = 0;
            err = FixCharacters(s, items[i], st, hasPrior ? &prior : nullptr, &userLimit);
            rec.charUserLimit = userLimit;
            const auto limits = RunLimits(basic, s, items[i]);
            if (!limits.empty()) rec.charLimits = limits;
        } else {
            ++st.paths;
            err = FixPath(basic, s, items[i], aligns[i], st, hasPrior ? &prior : nullptr, rec);
        }
        if (err) {
            ++failed;
            if (!first) first = err;
            Trace(type == kTextFrameArt ? "fix.characters" : "fix.path", err, "");
        }
        // Marked even after a failure part-way, with what was done, so a limit the fix wrote is
        // not later taken for the user's; an unchanged record is not written again.
        if (!hasPrior || !(rec == prior)) Mark(s.art, *dict.operator&(), items[i], rec);
        if (!err && done) done->push_back(items[i]);
    }
    st.failed = failed;
    st.gate = failed ? "partial" : "fixed";
    return failed ? first : kNoErr;
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
    return FixItems(message.basic, s, items, st);
}

// The timer's re-fix: marked art whose stamp changed since it was last seen is fixed again, in
// the undo step of the edit that changed it (appended). Art seen for the first time is taken as
// fixed. Text frames wait while the user types (a story has the text focus), so a re-fix never
// lands inside a typing session; nothing is done while redo steps wait (it would discard them).
// An item whose managed attributes the user changed by hand is unmarked and left alone.
ASErr Refix(SPMessageData& message, State& state) {
    auto& r = state.refix;
    Suite<AIDocumentSuite> document(message.basic, kAIDocumentSuite, kAIDocumentVersion);
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    if (document.error || art.error) return kNoErr;
    AIDocumentHandle doc = nullptr;
    if (document->GetDocument(&doc) || !doc) { r.document = nullptr; r.count = 0; return kNoErr; }
    const size_t global = art->GetGlobalTimeStamp();
    AIBoolean typing = false;
    if (document->HasTextFocus(&typing)) typing = false;
    if (doc != r.document) { r.document = doc; r.count = 0; r.waiting = false; r.global = 0; }
    // While the user types, only the story being typed changes: nothing is scanned until the
    // typing ends (the text focus goes), then the changed frames are fixed.
    if (typing) { if (global != r.global) { r.waiting = true; ++r.deferred; } return kNoErr; }
    if (global == r.global && !r.waiting) return kNoErr;
    Suite<AIUndoSuite> undo(message.basic, kAIUndoSuite, kAIUndoVersion);
    if (undo.error) return kNoErr;
    // While redo steps wait (the user is undoing), every state restored was fixed when it was made,
    // and a change now would discard the redo steps: the stamps are refreshed, nothing is fixed.
    bool undoing = false;
    {
        ai::int32 past = 0, future = 0;
        undoing = !undo->CountTransactions(&past, &future) && future > 0;
    }
    ++r.scans;
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
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    for (ASErr e : {path.error, paint.error, paintStyle.error, styles.error, parser.error, text.error, range.error,
                    features.error, textFrame.error, matching.error, memory.error, dict.error})
        if (e) { r.lastError = static_cast<int>(e); return kNoErr; }
    FixSuites s{*art.operator&(), *path.operator&(), *paint.operator&(), *paintStyle.operator&(), *styles.operator&(),
                *parser.operator&(), *text.operator&(), *range.operator&(), *features.operator&(), *textFrame.operator&()};
    AIMatchingArtSpec specs[3] = {AIMatchingArtSpec(kPathArt, 0, 0), AIMatchingArtSpec(kCompoundPathArt, 0, 0),
                                  AIMatchingArtSpec(kTextFrameArt, 0, 0)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    if (matching->GetMatchingArt(specs, 3, &matches, &count) || !matches) { r.global = global; return kNoErr; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    auto find = [&](AIArtHandle a) -> const RefixState::Seen* {
        for (int i = 0; i < r.count; ++i) if (r.seen[i].art == a) return &r.seen[i];
        return nullptr;
    };
    auto stampOf = [&](AIArtHandle a) {
        size_t v = 0;
        art->GetArtTimeStamp(a, kAITimeStampMaxFromArtAndChildren, &v);
        return v;
    };
    // The table is rebuilt from this scan: art no longer in the document drops out.
    std::vector<RefixState::Seen> next;
    std::vector<AIArtHandle> due;
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        short ptype = kUnknownArt;
        if (!art->GetArtParent(a, &parent) && parent && !art->GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        if (!Marked(*art.operator&(), *dict.operator&(), a)) continue;
        const size_t stamp = stampOf(a);
        const RefixState::Seen* seen = find(a);
        // First sight (just fixed, opened, pasted, duplicated): taken as fixed, and followed from
        // here, so opening a document changes nothing in it.
        if (!seen || seen->stamp == stamp || undoing) { next.push_back({a, stamp}); continue; }
        due.push_back(a);
    }
    if (!due.empty()) {
        undo->SetKind(kAIAppendUndoContext);
        NativeFixStats st{};
        std::vector<AIArtHandle> fixed;
        const ASErr err = FixItems(message.basic, s, due, st, true, &fixed);
        r.lastError = static_cast<int>(err);
        ++r.runs;
        r.items += fixed.size();
        r.released += static_cast<unsigned long long>(st.released);
        // Every due item is followed from its new stamp: fixed, released (no longer marked: it
        // drops out next scan), or failed (looked at again on its next change).
        for (AIArtHandle a : due) next.push_back({a, stampOf(a)});
    }
    r.count = 0;
    for (const auto& e : next) { if (r.count >= 4096) break; r.seen[r.count++] = e; }
    if (next.size() > 4096) ++r.overflow;
    r.waiting = false;
    if (undoing) ++r.deferred;
    r.global = art->GetGlobalTimeStamp();
    return kNoErr;
}

// Research (2026-10-16): ATE's per-character FillFirst ("fill-first:0" paints each character's
// stroke before its fill, "fill-first:1" restores the default) on the selected text frames, or
// every frame when none is selected; "fill-first:?" reads it back (contentsAfter: 10 when
// assigned, plus 1 when the fill comes first; -1 when mixed).
ASErr FillFirst(SPMessageData& message, State& state, const char* value) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    for (ASErr e : {matching.error, memory.error, text.error, range.error, features.error})
        if (e) { stats.gate = "suites"; return e; }
    const bool read = std::strcmp(value, "?") == 0;
    if (!read && std::strcmp(value, "0") != 0 && std::strcmp(value, "1") != 0) { stats.gate = "bad-value"; return kBadParameterErr; }
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
    for (ai::int32 i = 0; i < count; ++i) {
        TextRangeRef r = nullptr;
        err = text->GetATETextRange((*matches)[i], &r);
        if (err || !r) { stats.gate = "range"; return err ? err : kBadParameterErr; }
        ATE::CharFeaturesRef f = nullptr;
        ATE::ATEErr ate = ATE::kNoError;
        if (read) {
            ate = range->GetUniqueCharFeatures(r, &f);
            ATE::ATEBool8 assigned = false, first = true;
            if (!ate && f) ate = features->GetFillFirst(f, &assigned, &first);
            stats.contentsAfter = assigned ? 10 + (first ? 1 : 0) : -1;
        } else {
            ate = features->Initialize(&f);
            if (!ate && f) ate = features->SetFillFirst(f, value[0] == '1');
            if (!ate && f) ate = range->ReplaceOrAddLocalCharFeatures(r, f);
        }
        if (f) features->Release(f);
        range->Release(r);
        if (ate || !f) { stats.gate = "fill-first"; return kBadParameterErr; }
        ++stats.styled;
    }
    stats.gate = read ? "read" : "set";
    return kNoErr;
}

// Research (2026-10-16): Illustrator's own "Align dashes to corners and path ends" through the
// call the Stroke panel makes, SetCurrentPathStyle with advanced stroke parameters, on the
// current selection ("dash-align" "on" or "off"); "?" only reads. It takes the SDK StrokeFilter
// sample's form: the selection's own style and map with the dash length flagged; with an empty
// map, or the parameters nested in an effect dictionary, the call changed nothing. Its result,
// read back with style-dump, is what AlignDashes writes per path. fieldsBefore and fieldsAfter:
// the selection's dash adjustment before and after (1 on, 0 off, -1 mixed); styled: selected
// paths that can accept the parameters (GetArtStrokeParams canAccept).
ASErr DashAlign(SPMessageData& message, State& state, const char* value) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    Suite<AIPathStyleSuite> paint(message.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIBeautifulStrokesSuite> bs(message.basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    for (ASErr e : {paint.error, bs.error, dict.error, matching.error, memory.error})
        if (e) { stats.gate = "suites"; return e; }
    const bool read = std::strcmp(value, "?") == 0;
    const bool on = std::strcmp(value, "on") == 0;
    if (!read && !on && std::strcmp(value, "off") != 0) { stats.gate = "bad-value"; return kBadParameterErr; }
    auto current = [&](int* adjust) -> ASErr {
        AIPathStyle style{};
        AIPathStyleMap map{};
        AIDictionaryRef adv = nullptr;
        AIBoolean advFill = false;
        auto e = paint->GetCurrentPathStyle(&style, &map, &adv, &advFill);
        if (e) return e;
        *adjust = 0;
        if (adv) {
            AIBoolean mixed = false;
            AIDictionaryRef inner = nullptr;
            bs->GetParamsDict(adv, inner);
            const AIBoolean got = bs->GetDashAdjustment(inner ? inner : adv, &mixed);
            *adjust = mixed ? -1 : (got ? 1 : 0);
            if (inner) dict->Release(inner);
            dict->Release(adv);
        }
        return kNoErr;
    };
    int before = 0;
    auto err = current(&before);
    if (err) { stats.gate = "get-current"; return err; }
    stats.fieldsBefore = before;
    {
        AIMatchingArtSpec specs[2] = {AIMatchingArtSpec(kPathArt, kArtSelected, kArtSelected),
                                      AIMatchingArtSpec(kCompoundPathArt, kArtSelected, kArtSelected)};
        AIArtHandle** matches = nullptr;
        ai::int32 count = 0;
        if (!matching->GetMatchingArt(specs, 2, &matches, &count) && matches) {
            for (ai::int32 i = 0; i < count; ++i) {
                AIDictionaryRef p = nullptr;
                AIBoolean accept = false;
                if (!bs->GetArtStrokeParams((*matches)[i], p, nullptr, nullptr, nullptr, &accept) && accept) ++stats.styled;
                if (p) dict->Release(p);
            }
            memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches));
        }
        stats.frames = count;
    }
    if (!read) {
        AIPathStyle style{};
        AIPathStyleMap map{};
        AIDictionaryRef adv = nullptr;
        err = paint->GetCurrentPathStyle(&style, &map, &adv, nullptr);
        map.stroke.dash.length = true;
        if (!err && !adv) err = bs->CreateParamsDict(nullptr, adv);
        if (err || !adv) { if (adv) dict->Release(adv); stats.gate = "dict"; return err ? err : kOutOfMemoryErr; }
        err = bs->SetDashAdjustment(adv, on);
        if (!err) err = paint->SetCurrentPathStyle(&style, &map, adv);
        dict->Release(adv);
        if (err) { stats.gate = "set-current"; return err; }
    }
    int after = 0;
    err = current(&after);
    if (err) { stats.gate = "get-after"; return err; }
    stats.fieldsAfter = after;
    stats.gate = read ? "read" : "set";
    return kNoErr;
}

// Research (2026-10-16): the art style of the first selected path or compound path as text, for
// learning what Illustrator writes (Effect > Pathfinder > Add, Align Dashes to Corners): its
// pre- and post-effects and paint fields, each with its live effect's name and parameters.
struct Dumper {
    const AIDictionarySuite& dict;
    const AIDictionaryIteratorSuite& iter;
    std::string& out;
    void Dict(ConstAIDictionaryRef d, int depth) {
        if (!d || depth > 4) { out += "{}"; return; }
        AIDictionaryIterator it = nullptr;
        if (dict.Begin(d, &it) || !it) { out += "{?}"; return; }
        out += "{";
        for (int n = 0; !iter.AtEnd(it) && n < 64; iter.Next(it), ++n) {
            const AIDictKey key = iter.GetKey(it);
            const char* name = dict.GetKeyString(key);
            out += name ? name : "?";
            out += "=";
            AIEntryType type = UnknownType;
            dict.GetEntryType(d, key, &type);
            char buf[64]{};
            if (type == BooleanType) { AIBoolean b = false; dict.GetBooleanEntry(d, key, &b); out += b ? "T" : "F"; }
            else if (type == IntegerType) { ai::int32 v = 0; dict.GetIntegerEntry(d, key, &v); std::snprintf(buf, sizeof buf, "i%d", v); out += buf; }
            else if (type == RealType) { AIReal v = 0; dict.GetRealEntry(d, key, &v); std::snprintf(buf, sizeof buf, "r%g", static_cast<double>(v)); out += buf; }
            else if (type == StringType) { const char* s = nullptr; dict.GetStringEntry(d, key, &s); out += "'"; out += s ? s : ""; out += "'"; }
            else if (type == DictType) { AIDictionaryRef sub = nullptr; dict.GetDictEntry(d, key, &sub); Dict(sub, depth + 1); if (sub) dict.Release(sub); }
            else { std::snprintf(buf, sizeof buf, "<type %d>", static_cast<int>(type)); out += buf; }
            out += " ";
        }
        out += "}";
        iter.Release(it);
    }
};

ASErr StyleDump(SPMessageData& message, std::string& out) {
    out.clear();
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIDictionaryIteratorSuite> iter(message.basic, kAIDictionaryIteratorSuite, kAIDictionaryIteratorVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    for (ASErr e : {styles.error, parser.error, dict.error, iter.error, matching.error, memory.error, art.error})
        if (e) return e;
    AIMatchingArtSpec specs[3] = {AIMatchingArtSpec(kPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kCompoundPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kTextFrameArt, kArtSelected, kArtSelected)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = matching->GetMatchingArt(specs, 3, &matches, &count);
    if (err) return err;
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    AIArtHandle target = nullptr;
    for (ai::int32 i = 0; i < count && !target; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        short ptype = kUnknownArt;
        if (!art->GetArtParent(a, &parent) && parent && !art->GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        target = a;
    }
    if (!target) return kBadParameterErr;
    short type = kUnknownArt;
    art->GetArtType(target, &type);
    char buf[96]{};
    std::snprintf(buf, sizeof buf, "type %d; ", static_cast<int>(type));
    out += buf;
    AIArtStyleHandle style = nullptr;
    if (styles->GetArtStyle(target, &style) || !style) { out += "no style"; return kNoErr; }
    AIStyleParser p = nullptr;
    if (parser->NewParser(&p) || !p) return kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{*parser.operator&(), p};
    if ((err = parser->ParseStyle(p, style)) != kNoErr) return err;
    Dumper dump{*dict.operator&(), *iter.operator&(), out};
    auto effect = [&](AIParserLiveEffect e) {
        const char* name = nullptr;
        ai::int32 major = 0, minor = 0;
        parser->GetLiveEffectNameAndVersion(e, &name, &major, &minor);
        AILiveEffectParameters params = nullptr;
        parser->GetLiveEffectParams(e, &params);
        out += "'"; out += name ? name : "?"; out += "' ";
        dump.Dict(params, 0);
        out += "; ";
    };
    std::snprintf(buf, sizeof buf, "pre %d post %d fields %d contents %d; ", parser->CountPreEffects(p),
                  parser->CountPostEffects(p), parser->CountPaintFields(p), parser->GetGroupContentsPosition(p));
    out += buf;
    for (ai::int32 i = 0; i < parser->CountPreEffects(p); ++i) {
        AIParserLiveEffect e = nullptr;
        if (!parser->GetNthPreEffect(p, i, &e) && e) { out += "PRE "; effect(e); }
    }
    for (ai::int32 i = 0; i < parser->CountPostEffects(p); ++i) {
        AIParserLiveEffect e = nullptr;
        if (!parser->GetNthPostEffect(p, i, &e) && e) { out += "POST "; effect(e); }
    }
    for (ai::int32 i = 0; i < parser->CountPaintFields(p); ++i) {
        AIParserPaintField f = nullptr;
        if (parser->GetNthPaintField(p, i, &f) || !f) continue;
        out += parser->IsFill(f) ? "FILL " : (parser->IsStroke(f) ? "STROKE " : "FIELD ");
        AILiveEffectHandle h = nullptr;
        AILiveEffectParameters params = nullptr;
        parser->GetPaintLiveEffectInfo(f, &h, &params);
        const char* name = nullptr;
        ai::int32 major = 0, minor = 0;
        if (h) parser->GetPaintLiveEffectNameAndVersion(f, &name, &major, &minor);
        out += "paint '"; out += name ? name : "-"; out += "' ";
        dump.Dict(params, 0);
        for (ai::int32 k = 0; k < parser->CountEffectsOfPaintField(f); ++k) {
            AIParserLiveEffect e = nullptr;
            if (!parser->GetNthEffectOfPaintField(f, k, &e) && e) { out += " EFFECT "; effect(e); }
        }
        out += "; ";
    }
    return kNoErr;
}

// Research and tests: every character's miter limit in the selected text frames (every frame
// when none is selected), comma-separated, frames separated by ';'. Scripting reads NaN.
// Research (2026-10-16): stroke alignment per target. For each selected item whose parent is not
// selected (a group, not its children), any art type: "center", "inside", or "outside" calls
// AIPaintStyleSuite::SetStrokeAlignment and reports its error; "?" only reads. Then the alignment
// read back from the style (ReadAlignment) and the style's paint fields. "add-stroke" (selector
// align-survey, input "stroke") first adds an object-level 8 pt stroke below everything, as the
// Appearance panel's Add New Stroke does on a group or a type object ("stroke-above": above
// everything, Characters included).
ASErr AlignSurvey(SPMessageData& message, const char* input, std::string& out) {
    out.clear();
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
        if (e) return e;
    FixSuites s{*art.operator&(), *path.operator&(), *paint.operator&(), *paintStyle.operator&(), *styles.operator&(),
                *parser.operator&(), *text.operator&(), *range.operator&(), *features.operator&(), *textFrame.operator&()};
    ai::uint32 align = 99;
    if (std::strcmp(input, "center") == 0) align = kAIStrokeAlignmentCenter;
    else if (std::strcmp(input, "inside") == 0) align = kAIStrokeAlignmentInside;
    else if (std::strcmp(input, "outside") == 0) align = kAIStrokeAlignmentOutside;
    // "stroke" or "stroke-above", optionally ":<weight>" (8 pt by default).
    const bool above = std::strncmp(input, "stroke-above", 12) == 0;
    const bool addStroke = above || std::strncmp(input, "stroke", 6) == 0;
    double addWeight = 8;
    if (const char* colon = std::strchr(input, ':')) addWeight = std::atof(colon + 1);
    if (addStroke && !(addWeight > 0 && addWeight <= 1000)) return kBadParameterErr;
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = matching->GetSelectedArt(&matches, &count);
    if (err) return err;
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    char buf[160]{};
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        ai::int32 attr = 0;
        short ptype = kUnknownArt;
        AIBoolean layerGroup = false;
        if (!art->IsArtLayerGroup(a, &layerGroup) && layerGroup) continue;   // a layer, not an item
        if (!art->GetArtParent(a, &parent) && parent && !art->GetArtType(parent, &ptype)) {
            // Skip children of a selected parent (layers' groups excepted).
            AIBoolean isLayerGroup = false;
            art->IsArtLayerGroup(parent, &isLayerGroup);
            if (!isLayerGroup && !art->GetArtUserAttr(parent, kArtSelected, &attr) && (attr & kArtSelected)) continue;
        }
        short type = kUnknownArt;
        art->GetArtType(a, &type);
        std::snprintf(buf, sizeof buf, "%stype %d", out.empty() ? "" : "; ", static_cast<int>(type));
        out += buf;
        if (addStroke) {
            AIArtStyleHandle style = nullptr;
            AIStyleParser p = nullptr;
            ASErr e = styles->GetArtStyle(a, &style);
            if (!e && !parser->NewParser(&p) && p) {
                if (style) e = parser->ParseStyle(p, style);
                AIPathStyle base{};
                base.Init();
                AIStrokeStyle stroke = base.stroke;
                stroke.color.kind = kGrayColor;
                stroke.color.c.g.gray = static_cast<AIReal>(0.6);
                stroke.width = static_cast<AIReal>(addWeight);
                stroke.join = kAIMiterJoin;
                stroke.miterLimit = 4;
                AIParserPaintField field = nullptr;
                if (!e) e = parser->NewPaintFieldStroke(&stroke, nullptr, &field);
                if (!e && field) e = parser->InsertNthPaintField(p, above ? 0 : -1, field);
                AIArtStyleHandle created = nullptr;
                if (!e) e = parser->CreateNewStyle(p, &created);
                if (!e && created) e = styles->SetArtStyle(a, created);
                parser->DisposeParser(p);
            }
            std::snprintf(buf, sizeof buf, " add-stroke %d", static_cast<int>(e));
            out += buf;
        }
        if (align != 99) {
            const ASErr e = paintStyle->SetStrokeAlignment(a, align);
            std::snprintf(buf, sizeof buf, " set %d", static_cast<int>(e));
            out += buf;
        }
        std::snprintf(buf, sizeof buf, " read %u", static_cast<unsigned>(ReadAlignment(message.basic, s, a)));
        out += buf;
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        if (!styles->GetArtStyle(a, &style) && style && !parser->NewParser(&p) && p) {
            if (!parser->ParseStyle(p, style)) {
                std::snprintf(buf, sizeof buf, " fields %d contents %d", parser->CountPaintFields(p), parser->GetGroupContentsPosition(p));
                out += buf;
            }
            parser->DisposeParser(p);
        }
    }
    return kNoErr;
}

// A constant uncentered width profile on every stroke field of an item's style: `left` and `right`
// are the side widths as Beautiful Strokes takes them (sides seen along the path's direction),
// written as the Width tool writes a profile, beside the field's StrokeStyle. With one side 0,
// the stroke lies on the other side only.
ASErr SetSideProfile(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, double left, double right, int& fields,
                     bool clear = false) {
    fields = 0;
    Suite<AIBeautifulStrokesSuite> bs(basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIEntrySuite> entry(basic, kAIEntrySuite, kAIEntryVersion);
    Suite<AIArraySuite> array(basic, kAIArraySuite, kAIArrayVersion);
    for (ASErr e : {bs.error, live.error, dict.error, entry.error, array.error}) if (e) return e;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e ? e : kBadParameterErr;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    e = s.parser.ParseStyle(p, style);
    if (e) return e;
    auto realArray = [&](double a, double b) -> AIArrayRef {
        AIArrayRef arr = nullptr;
        if (array->CreateArray(&arr) || !arr) return nullptr;
        array->AppendEntry(arr, entry->FromReal(static_cast<AIReal>(a)));
        array->AppendEntry(arr, entry->FromReal(static_cast<AIReal>(b)));
        return arr;
    };
    for (ai::int32 i = 0; i < s.parser.CountPaintFields(p); ++i) {
        AIParserPaintField field = nullptr;
        if (s.parser.GetNthPaintField(p, i, &field) || !field || !s.parser.IsStroke(field)) continue;
        AILiveEffectHandle handle = nullptr;
        AILiveEffectParameters params = nullptr;
        if (s.parser.GetPaintLiveEffectInfo(field, &handle, &params) || handle) continue;   // brushes, Inside/Outside
        AIStrokeStyle strokeStyle{};
        AIArtStylePaintData data;
        if (s.parser.GetStroke(field, &strokeStyle, &data)) continue;
        AILiveEffectParameters mine = nullptr;
        if (params ? dict->Clone(params, &mine) : live->CreateLiveEffectParameters(&mine)) continue;
        if (!mine) continue;
        AIDictionaryRef inner = nullptr;
        e = bs->CreateParamsDict(mine, inner);
        AIArrayRef ts = realArray(0, 1), ls = realArray(left, left), rs = realArray(right, right);
        if (!e && (!inner || (!clear && !(ts && ls && rs)))) e = kOutOfMemoryErr;   // nothing would be written
        if (!e) e = bs->SetWidthSource(inner, clear ? kConstantWidth : kUserProfile);
        if (!e && !clear) e = bs->SetUncenteredWidthProfile(inner, ts, ls, rs);
        for (AIArrayRef a : {ts, ls, rs}) if (a) array->Release(a);
        if (inner) dict->Release(inner);
        if (!e && !dict->IsKnown(mine, dict->Key("StrokeStyle"))) {
            AIEntryRef se = entry->FromStrokeStyle(&strokeStyle);
            if (se) e = dict->Set(mine, dict->Key("StrokeStyle"), se);
        }
        if (!e) e = s.parser.SetPaintLiveEffectInfo(field, nullptr, mine);
        dict->Release(mine);
        if (e) return e;
        ++fields;
    }
    if (!fields) return kNoErr;
    AIArtStyleHandle created = nullptr;
    e = s.parser.CreateNewStyle(p, &created);
    if (e || !created) return e ? e : kBadParameterErr;
    return s.styles.SetArtStyle(art, created);
}

// Research (2026-10-16): "width-side" with "<left>,<right>" (for example "0,1") puts that constant
// uncentered width profile on the selected items (whose parent is not selected; any type), to learn
// whether a one-sided profile stands in for Inside and Outside where Illustrator draws no alignment.
ASErr WidthSide(SPMessageData& message, const char* input, std::string& out) {
    out.clear();
    double left = -1, right = -1;
    if (std::sscanf(input, "%lf,%lf", &left, &right) != 2 || left < 0 || right < 0 || left > 2 || right > 2) return kBadParameterErr;
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
        if (e) return e;
    FixSuites s{*art.operator&(), *path.operator&(), *paint.operator&(), *paintStyle.operator&(), *styles.operator&(),
                *parser.operator&(), *text.operator&(), *range.operator&(), *features.operator&(), *textFrame.operator&()};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = matching->GetSelectedArt(&matches, &count);
    if (err) return err;
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    char buf[96]{};
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        AIBoolean layerGroup = false;
        if (!art->IsArtLayerGroup(a, &layerGroup) && layerGroup) continue;
        ai::int32 attr = 0;
        AIBoolean parentLayer = false;
        if (!art->GetArtParent(a, &parent) && parent && !art->IsArtLayerGroup(parent, &parentLayer) && !parentLayer &&
            !art->GetArtUserAttr(parent, kArtSelected, &attr) && (attr & kArtSelected)) continue;
        short type = kUnknownArt;
        art->GetArtType(a, &type);
        int fields = 0;
        const ASErr e = SetSideProfile(message.basic, s, a, left, right, fields);
        std::snprintf(buf, sizeof buf, "%stype %d err %d fields %d", out.empty() ? "" : "; ", static_cast<int>(type),
                      static_cast<int>(e), fields);
        out += buf;
    }
    return kNoErr;
}

// native-align (2026-10-16), the opt-in for strokes Illustrator cannot align, on the selected items
// (whose parent is not selected):
//   "outside", "inside"  where Illustrator draws the alignment (closed paths, compound paths of
//       closed contours, groups with their own stroke), its own SetStrokeAlignment; otherwise:
//       - an open path: a one-sided width profile of the full weight on that side (the side the
//         path's implicit closing encloses is Inside), so the Stroke panel keeps the weight;
//       - a compound path with an open contour (Illustrator drops alignment for all its contours),
//         Outside only: Center at twice the weight under its fill;
//       - type (Illustrator stores but never draws a type object's alignment), Outside only: one
//         stroke, an object-level stroke of twice the weight below Characters, the characters'
//         strokes cleared; mixed strokes, every character's weight doubled and its stroke painted
//         before its own fill (FillFirst off).
//     Inside on type and on a compound path with an open contour has no faithful native form
//     and is refused (a one-sided profile there measured 1,333 pt2 off Illustrator's own Inside).
//   "dashes"  the opt-in for dashed Outside strokes, which Illustrator cannot fit to corners:
//       Center at twice the weight under the fill, then Align Dashes to Corners.
//   "restore"  what native-align made of the selected items, put back as it was.
// The Center-under-fill forms need a simple style (one painting fill, one stroke): the fill hides
// the inner half. What each item became is kept in its dictionary (AlignRecord).
struct NativeAlignStats {
    int native = 0, profiled = 0, doubled = 0, typeOutside = 0, restored = 0, refused = 0;
    std::string reasons;
};

ASErr NativeAlignItem(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, const char* mode, NativeAlignStats& st) {
    auto refuse = [&](const char* why) { ++st.refused; if (st.reasons.size() < 200) { st.reasons += why; st.reasons += ' '; } return kNoErr; };
    Suite<AIPaintStyleSuite> paintStyle(basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
    if (paintStyle.error) return paintStyle.error;
    short type = kUnknownArt;
    if (s.art.GetArtType(art, &type)) return refuse("type?");
    const bool outside = std::strcmp(mode, "outside") == 0, inside = std::strcmp(mode, "inside") == 0;
    const bool dashes = std::strcmp(mode, "dashes") == 0, restore = std::strcmp(mode, "restore") == 0;
    AlignRecord ar = ReadAlignRecord(basic, s.art, art);

    // Center at twice the weight under the fill (from Outside or Center at weight w).
    // Side in a "doubled" record: 1, the stroke was above the fill before (the fill was moved up);
    // 3, the fill was above it already.
    auto doubleUnderFill = [&](bool alignDashes) -> ASErr {
        // Checked before any change: one painting fill with no effect of its own, one stroke.
        if (!SimpleFillAndStroke(s, art)) return refuse("needs-one-fill-one-stroke");
        AIPathStyle style{};
        AIBoolean adv = false;
        if (s.paint.GetPathStyle(art, &style, &adv) || !style.strokePaint || !(style.stroke.width > 0)) return refuse("no-stroke");
        const double w = style.stroke.width;
        const ai::uint32 wasAlign = ReadAlignment(basic, s, art);
        const bool fillWasOnTop = FillOnTop(s, art);
        // Rolled back if a later step fails, so the item is never left half rewritten.
        auto rollback = [&](ASErr e) {
            AIPathStyle back{};
            AIBoolean a2 = false;
            if (!s.paint.GetPathStyle(art, &back, &a2)) { back.stroke.width = static_cast<AIReal>(w); AIPathStyle partial = back; s.paint.SetPathStyle(art, &partial); }
            if (!fillWasOnTop && FillOnTop(s, art)) StrokeAboveFill(s, art);
            paintStyle->SetStrokeAlignment(art, wasAlign);
            Trace("align.rollback", e, "");
            return e;
        };
        if (paintStyle->SetStrokeAlignment(art, kAIStrokeAlignmentCenter)) return refuse("center");
        if (s.paint.GetPathStyle(art, &style, &adv)) return rollback(kBadParameterErr);
        style.stroke.width = static_cast<AIReal>(2 * w);
        AIPathStyle partial = style;
        auto e = s.paint.SetPathStyle(art, &partial);
        if (e) return rollback(e);
        bool changed = false;
        e = FillAboveStroke(s, art, changed);
        if (e) return rollback(e);
        if (!FillOnTop(s, art)) return rollback(kBadParameterErr);
        WriteAlignRecord(basic, s.art, art, AlignRecord{"doubled", fillWasOnTop ? 3 : 1, w});
        ++st.doubled;
        if (alignDashes && style.stroke.dash.length > 0) {
            bool aligned = false, refusedDash = false;
            e = AlignDashes(basic, s, art, aligned, refusedDash);
            if (e) return e;   // the doubled stroke stands, recorded; its dashes stay as they were
        }
        return kNoErr;
    };

    if (restore) {
        if (ar.as.empty()) return kNoErr;
        if (ar.as == "profile") {
            int fields = 0;
            auto e = SetSideProfile(basic, s, art, 1, 1, fields, true);
            if (e) return e;
        } else if (ar.as == "doubled") {
            // The stroke back above the fill only when it was there before.
            ASErr e = kNoErr;
            if (ar.side != 3 && (e = StrokeAboveFill(s, art)) != kNoErr) return e;
            AIPathStyle style{};
            AIBoolean adv = false;
            // Half the weight it has now, so a weight the user changed since is kept.
            if (!s.paint.GetPathStyle(art, &style, &adv) && style.stroke.width > 0) {
                style.stroke.width = static_cast<AIReal>(style.stroke.width / 2);
                AIPathStyle partial = style;
                e = s.paint.SetPathStyle(art, &partial);
                if (e) return e;
            }
            paintStyle->SetStrokeAlignment(art, kAIStrokeAlignmentOutside);   // refused on an open contour: Center
        } else if (ar.as == "type-outside") {
            // The bottom stroke field back onto the characters at half its weight.
            AIArtStyleHandle style = nullptr;
            AIStyleParser p = nullptr;
            auto e = s.styles.GetArtStyle(art, &style);
            if (e || !style || s.parser.NewParser(&p) || !p) return e ? e : kBadParameterErr;
            struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
            if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
            // The stroke field native-align added: below Characters, at twice the recorded weight.
            const ai::int32 n = s.parser.CountPaintFields(p), contents = s.parser.GetGroupContentsPosition(p);
            AIParserPaintField last = nullptr;
            AIStrokeStyle stroke{};
            AIArtStylePaintData data;
            for (ai::int32 k = n - 1; k >= contents && k >= 0 && !last; --k) {
                AIParserPaintField fk = nullptr;
                AIStrokeStyle sk{};
                AIArtStylePaintData dk;
                if (!s.parser.GetNthPaintField(p, k, &fk) && fk && s.parser.IsStroke(fk) && !s.parser.GetStroke(fk, &sk, &dk) &&
                    std::fabs(sk.width - 2 * ar.weight) <= 1e-3 * (std::max)(1.0, ar.weight)) {
                    last = fk; stroke = sk; data = dk;
                }
            }
            if (!last) return refuse("restore-field-changed");
            stroke.width = static_cast<AIReal>(stroke.width / 2);   // half its weight now
            Suite<AIATEPaintSuite> atePaint(basic, kAIATEPaintSuite, kAIATEPaintVersion);
            if (atePaint.error) return atePaint.error;
            AIPathStyle ps{};
            ps.Init();
            ps.strokePaint = true;
            ps.stroke = stroke;
            // Every part of the stroke given: the color's kind and components, width, join, cap,
            // limit, overprint, and dashes (a map naming the kind alone gave no stroke).
            AIPathStyleMap map{};
            map.strokePaint = true;
            std::memset(&map.stroke, 1, sizeof map.stroke);
            TextRangeRef r = nullptr;
            if (s.text.GetATETextRange(art, &r) || !r) return kBadParameterErr;
            ATE::CharFeaturesRef f = nullptr;
            ATE::ATEErr ate = s.features.Initialize(&f);
            if (!ate && f) e = atePaint->GetCharFeatures(&ps, &map, f);
            if (!ate && f && !e) ate = s.features.SetStroke(f, true);
            if (!ate && f && !e) ate = s.range.ReplaceOrAddLocalCharFeatures(r, f);
            if (f) s.features.Release(f);
            s.range.Release(r);
            if (ate || e) return e ? e : kBadParameterErr;
            if ((e = s.parser.RemovePaintField(p, last, true)) != kNoErr) return e;
            AIArtStyleHandle created = nullptr;
            if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
            if ((e = s.styles.SetArtStyle(art, created)) != kNoErr) return e;
        } else if (ar.as == "type-chars") {
            // Every character's stroke weight halved, and FillFirst back on when it was on before.
            Suite<ATE::TextRunsIteratorSuite> iter(basic, kTextRunsIteratorSuite, kTextRunsIteratorSuiteVersion);
            if (iter.error) return iter.error;
            TextRangeRef whole = nullptr;
            if (s.text.GetATETextRange(art, &whole) || !whole) return kBadParameterErr;
            std::vector<std::pair<std::pair<ATETextDOM::Int32, ATETextDOM::Int32>, double>> runsOut;
            ATE::TextRunsIteratorRef runs = nullptr;
            if (!s.range.GetTextRunsIterator(whole, &runs) && runs) {
                for (ATE::ATEBool8 done = false; !iter->IsDone(runs, &done) && !done; iter->Next(runs)) {
                    TextRangeRef run = nullptr;
                    if (iter->Item(runs, &run) || !run) break;
                    ATE::CharFeaturesRef f = nullptr;
                    ATETextDOM::Int32 a = 0, b = 0;
                    s.range.GetStart(run, &a);
                    s.range.GetEnd(run, &b);
                    if (!s.range.GetUniqueCharFeatures(run, &f) && f) {
                        ATE::ATEBool8 assigned = false, on = false;
                        ATETextDOM::Float wdt = 0;
                        s.features.GetStroke(f, &assigned, &on);
                        if (on && !s.features.GetLineWidth(f, &assigned, &wdt) && wdt > 0) runsOut.push_back({{a, b}, wdt / 2});
                    }
                    if (f) s.features.Release(f);
                    s.range.Release(run);
                }
                iter->Release(runs);
            }
            for (const auto& ro : runsOut) {
                TextRangeRef r = nullptr;
                if (s.range.Clone(whole, &r) || !r) continue;
                ATE::CharFeaturesRef f = nullptr;
                if (!s.range.SetStart(r, ro.first.first) && !s.range.SetEnd(r, ro.first.second) && !s.features.Initialize(&f) && f &&
                    !s.features.SetLineWidth(f, static_cast<ATETextDOM::Float>(ro.second)))
                    s.range.ReplaceOrAddLocalCharFeatures(r, f);
                if (f) s.features.Release(f);
                s.range.Release(r);
            }
            if (ar.weight == 1) {
                ATE::CharFeaturesRef f = nullptr;
                if (!s.features.Initialize(&f) && f && !s.features.SetFillFirst(f, true)) s.range.ReplaceOrAddLocalCharFeatures(whole, f);
                if (f) s.features.Release(f);
            }
            s.range.Release(whole);
        }
        WriteAlignRecord(basic, s.art, art, AlignRecord{});
        ++st.restored;
        return kNoErr;
    }
    if (!ar.as.empty()) return refuse("already-aligned");   // restore first

    if (type == kTextFrameArt) {
        if (dashes) return kNoErr;
        if (inside) return refuse("type-inside");
        if (!outside) return kNoErr;
        // The characters' paint across the frame.
        Suite<AIATEPaintSuite> atePaint(basic, kAIATEPaintSuite, kAIATEPaintVersion);
        if (atePaint.error) return atePaint.error;
        AIPathStyle paint{};
        AIPathStyleMap map{};
        {
            TextRangeRef r = nullptr;
            if (s.text.GetATETextRange(art, &r) || !r) return kBadParameterErr;
            ATE::CharFeaturesRef unique = nullptr;
            ATE::ATEErr ate = s.range.GetUniqueCharFeatures(r, &unique);
            ASErr e = kNoErr;
            if (!ate && unique) e = atePaint->GetAIPathStyleAndMap(unique, &paint, &map);
            if (unique) s.features.Release(unique);
            s.range.Release(r);
            if (ate || !unique || e) return e ? e : kBadParameterErr;
        }
        if (map.strokePaint && !paint.strokePaint) return refuse("type-no-stroke");
        const bool oneStroke = map.strokePaint && paint.strokePaint && map.stroke.overprint && map.stroke.width &&
                               map.stroke.join && map.stroke.miterLimit && map.stroke.cap && map.stroke.dash.length &&
                               UniformSolid(paint.stroke.color, map.stroke.color);
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        auto e = s.styles.GetArtStyle(art, &style);
        if (e || s.parser.NewParser(&p) || !p) return e ? e : kOutOfMemoryErr;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
        if (style && (e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
        if (oneStroke) {
            if (s.parser.ContainsEffects(p) || s.parser.CountPaintFields(p) > 2 || s.parser.ContainsTransparency(p)) return refuse("type-styled");
            AIStrokeStyle stroke = paint.stroke;
            const double w = stroke.width;
            stroke.width = static_cast<AIReal>(2 * w);
            AIParserPaintField field = nullptr;
            if ((e = s.parser.NewPaintFieldStroke(&stroke, nullptr, &field)) != kNoErr || !field) return e ? e : kOutOfMemoryErr;
            if ((e = s.parser.InsertNthPaintField(p, -1, field)) != kNoErr) { s.parser.DisposePaintField(field); return e; }
            AIArtStyleHandle created = nullptr;
            if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
            TextRangeRef r = nullptr;
            if (s.text.GetATETextRange(art, &r) || !r) return kBadParameterErr;
            ATE::CharFeaturesRef f = nullptr;
            ATE::ATEErr ate = s.features.Initialize(&f);
            if (!ate && f) ate = s.features.SetStroke(f, false);
            if (!ate && f) ate = s.range.ReplaceOrAddLocalCharFeatures(r, f);
            if (f) s.features.Release(f);
            s.range.Release(r);
            if (ate) return kBadParameterErr;
            if ((e = s.styles.SetArtStyle(art, created)) != kNoErr) {
                // The stroke was one throughout, so turning it back on restores it.
                TextRangeRef r2 = nullptr;
                if (!s.text.GetATETextRange(art, &r2) && r2) {
                    ATE::CharFeaturesRef f2 = nullptr;
                    if (!s.features.Initialize(&f2) && f2 && !s.features.SetStroke(f2, true)) s.range.ReplaceOrAddLocalCharFeatures(r2, f2);
                    if (f2) s.features.Release(f2);
                    s.range.Release(r2);
                }
                return e;
            }
            WriteAlignRecord(basic, s.art, art, AlignRecord{"type-outside", 1, w});
            ++st.typeOutside;
            return kNoErr;
        }
        // Mixed strokes: every stroked run's weight doubled, each character's stroke before its fill.
        Suite<ATE::TextRunsIteratorSuite> iter(basic, kTextRunsIteratorSuite, kTextRunsIteratorSuiteVersion);
        if (iter.error) return iter.error;
        TextRangeRef whole = nullptr;
        if (s.text.GetATETextRange(art, &whole) || !whole) return kBadParameterErr;
        std::vector<std::pair<std::pair<ATETextDOM::Int32, ATETextDOM::Int32>, double>> runsOut;
        ATE::TextRunsIteratorRef runs = nullptr;
        if (!s.range.GetTextRunsIterator(whole, &runs) && runs) {
            for (ATE::ATEBool8 done = false; !iter->IsDone(runs, &done) && !done; iter->Next(runs)) {
                TextRangeRef run = nullptr;
                if (iter->Item(runs, &run) || !run) break;
                ATE::CharFeaturesRef f = nullptr;
                ATETextDOM::Int32 a = 0, b = 0;
                s.range.GetStart(run, &a);
                s.range.GetEnd(run, &b);
                if (!s.range.GetUniqueCharFeatures(run, &f) && f) {
                    ATE::ATEBool8 assigned = false, on = false;
                    ATETextDOM::Float wdt = 0;
                    s.features.GetStroke(f, &assigned, &on);
                    if (on && !s.features.GetLineWidth(f, &assigned, &wdt) && wdt > 0) runsOut.push_back({{a, b}, wdt * 2});
                }
                if (f) s.features.Release(f);
                s.range.Release(run);
            }
            iter->Release(runs);
        }
        for (const auto& ro : runsOut) {
            TextRangeRef r = nullptr;
            if (s.range.Clone(whole, &r) || !r) continue;
            ATE::CharFeaturesRef f = nullptr;
            if (!s.range.SetStart(r, ro.first.first) && !s.range.SetEnd(r, ro.first.second) && !s.features.Initialize(&f) && f &&
                !s.features.SetLineWidth(f, static_cast<ATETextDOM::Float>(ro.second)))
                s.range.ReplaceOrAddLocalCharFeatures(r, f);
            if (f) s.features.Release(f);
            s.range.Release(r);
        }
        // FillFirst before: on everywhere (the default) is recorded, to be put back on restore.
        bool fillFirstWasOn = false;
        {
            ATE::CharFeaturesRef u = nullptr;
            ATE::ATEBool8 assigned = false, ff = false;
            if (!s.range.GetUniqueCharFeatures(whole, &u) && u && !s.features.GetFillFirst(u, &assigned, &ff)) fillFirstWasOn = assigned && ff;
            if (u) s.features.Release(u);
        }
        {
            ATE::CharFeaturesRef f = nullptr;
            if (!s.features.Initialize(&f) && f && !s.features.SetFillFirst(f, false)) s.range.ReplaceOrAddLocalCharFeatures(whole, f);
            if (f) s.features.Release(f);
        }
        s.range.Release(whole);
        WriteAlignRecord(basic, s.art, art, AlignRecord{"type-chars", 1, fillFirstWasOn ? 1.0 : 0.0});
        ++st.typeOutside;
        return kNoErr;
    }
    if (type != kPathArt && type != kCompoundPathArt) {
        if (dashes || !(outside || inside)) return kNoErr;
        // A group with its own stroke takes Illustrator's own alignment.
        if (paintStyle->SetStrokeAlignment(art, outside ? kAIStrokeAlignmentOutside : kAIStrokeAlignmentInside)) return refuse("not-a-path");
        if (ReadAlignment(basic, s, art) != (outside ? kAIStrokeAlignmentOutside : kAIStrokeAlignmentInside)) return refuse("not-a-path");
        ++st.native;
        return kNoErr;
    }
    AIPathStyle style{};
    AIBoolean adv = false;
    if (s.paint.GetPathStyle(art, &style, &adv) || !style.strokePaint || !(style.stroke.width > 0)) return refuse("no-stroke");
    std::vector<ise_miter::Contour> contours;
    if (!Contours(s, art, contours) || contours.empty()) return refuse("outline");
    bool anyOpen = false;
    for (const auto& c : contours) anyOpen = anyOpen || !c.closed;
    if (dashes) {
        if (style.stroke.dash.length <= 0 || ReadAlignment(basic, s, art) != kAIStrokeAlignmentOutside) return kNoErr;
        return doubleUnderFill(true);
    }
    if (!(outside || inside)) return refuse("mode");
    if (!anyOpen) {
        const ai::uint32 want = outside ? kAIStrokeAlignmentOutside : kAIStrokeAlignmentInside;
        if (paintStyle->SetStrokeAlignment(art, want) || ReadAlignment(basic, s, art) != want) return refuse("native-refused");
        ++st.native;
        return kNoErr;
    }
    if (type == kCompoundPathArt) {
        if (inside) return refuse("compound-open-inside");
        return doubleUnderFill(false);
    }
    // An open path: the side its implicit closing encloses is Inside. Beautiful Strokes sides are
    // left and right of travel; with y up, a counterclockwise closing (positive area) encloses its
    // left side.
    double area = 0;
    for (const auto& r : ise_miter::Flatten(contours, 0.01, 1e-9))
        for (size_t i = 0; i < r.points.size(); ++i) {
            const auto a = r.points[i], b = r.points[(i + 1) % r.points.size()];
            area += a.x * b.y - b.x * a.y;
        }
    area /= 2;
    if (std::fabs(area) < 1e-3 * style.stroke.width * style.stroke.width) return refuse("open-no-side");
    {
        // A width profile of the user's own is not replaced.
        Suite<AIBeautifulStrokesSuite> bs(basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
        Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
        if (bs.error || dict.error) return kBadParameterErr;
        AIDictionaryRef params = nullptr;
        const auto got = bs->GetArtStrokeParams(art, params, nullptr, nullptr, nullptr, nullptr);
        const bool profiled = !got && params && bs->GetWidthSource(params) == kUserProfile;
        if (params) dict->Release(params);
        if (profiled) return refuse("own-width-profile");
    }
    const bool leftIsInside = area > 0;
    const bool left = outside ? !leftIsInside : leftIsInside;
    // The empty side gets a thousandth of the weight: exactly 0 drew a star at the joins.
    int fields = 0;
    auto e = SetSideProfile(basic, s, art, left ? 1 : 0.001, left ? 0.001 : 1, fields);
    if (e) return e;
    if (!fields) return refuse("profile");
    WriteAlignRecord(basic, s.art, art, AlignRecord{"profile", outside ? 1 : 2, style.stroke.width});
    ++st.profiled;
    return kNoErr;
}

ASErr NativeAlign(SPMessageData& message, const char* mode, NativeAlignStats& st) {
    st = NativeAlignStats{};
    if (std::strcmp(mode, "outside") != 0 && std::strcmp(mode, "inside") != 0 && std::strcmp(mode, "dashes") != 0 &&
        std::strcmp(mode, "restore") != 0)
        return kBadParameterErr;
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
        if (e) return e;
    FixSuites s{*art.operator&(), *path.operator&(), *paint.operator&(), *paintStyle.operator&(), *styles.operator&(),
                *parser.operator&(), *text.operator&(), *range.operator&(), *features.operator&(), *textFrame.operator&()};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = matching->GetSelectedArt(&matches, &count);
    if (err) return err;
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    ASErr first = kNoErr;
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        AIBoolean layerGroup = false, parentLayer = false;
        ai::int32 attr = 0;
        if (!art->IsArtLayerGroup(a, &layerGroup) && layerGroup) continue;
        if (!art->GetArtParent(a, &parent) && parent && !art->IsArtLayerGroup(parent, &parentLayer) && !parentLayer &&
            !art->GetArtUserAttr(parent, kArtSelected, &attr) && (attr & kArtSelected)) continue;
        const ASErr e = NativeAlignItem(message.basic, s, a, mode, st);
        if (e) { ++st.refused; st.reasons += "error "; if (!first) first = e; Trace("align.item", e, mode); }
    }
    return first;
}

ASErr CharLimits(SPMessageData& message, std::string& out) {
    out.clear();
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    for (ASErr e : {matching.error, memory.error, text.error, range.error, features.error}) if (e) return e;
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = SelectedOrAllFrames(*matching.operator&(), &matches, &count);
    if (err) return err;
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    for (ai::int32 i = 0; i < count && i < 64; ++i) {
        TextRangeRef whole = nullptr;
        if (text->GetATETextRange((*matches)[i], &whole) || !whole) continue;
        ATETextDOM::Int32 start = 0, size = 0;
        range->GetStart(whole, &start);
        range->GetSize(whole, &size);
        if (i) out += ";";
        for (ATETextDOM::Int32 k = 0; k < size && k < 4096; ++k) {
            TextRangeRef one = nullptr;
            if (range->Clone(whole, &one) || !one) break;
            char buf[32] = "?";
            if (!range->SetStart(one, start + k) && !range->SetEnd(one, start + k + 1)) {
                ATE::CharFeaturesRef f = nullptr;
                if (!range->GetUniqueCharFeatures(one, &f) && f) {
                    ATE::ATEBool8 assigned = false;
                    ATETextDOM::Float limit = 0;
                    if (!features->GetMiterLimit(f, &assigned, &limit)) std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(limit));
                }
                if (f) features->Release(f);
            }
            range->Release(one);
            if (k) out += ",";
            out += buf;
        }
        range->Release(whole);
    }
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
    ai::int32 undoPast = -1, undoFuture = -1;
    {
        Suite<AIUndoSuite> undo(message.basic, kAIUndoSuite, kAIUndoVersion);
        if (!undo.error && undo->CountTransactions(&undoPast, &undoFuture)) undoPast = undoFuture = -1;
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
        "\"charLimits\":%d,\"mapped\":%d,\"corners\":%d,\"keptBeveled\":%d,\"cutMitered\":%d,\"dashes\":%d,"
        "\"dashRefused\":%d,\"pathfinder\":%d,\"failed\":%d},"
        "\"align\":{\"gate\":\"%s\",\"native\":%d,\"profiled\":%d,\"doubled\":%d,\"typeOutside\":%d,"
        "\"restored\":%d,\"refused\":%d,\"reasons\":\"%s\"},"
        "\"refix\":{\"timer\":%s,\"scans\":%llu,\"runs\":%llu,\"items\":%llu,\"deferred\":%llu,\"lastError\":%d,"
        "\"released\":%llu,\"overflow\":%llu,"
        "\"undoPast\":%d,\"undoFuture\":%d},"
        "\"appearance\":{\"gate\":\"%s\",\"frames\":%d,\"styled\":%d,\"fieldsBefore\":%d,\"fieldsAfter\":%d,"
        "\"contentsBefore\":%d,\"contentsAfter\":%d,\"fillFirst\":%d},"
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
        state.nativeFix.dashRefused, state.nativeFix.pathfinder, state.nativeFix.failed,
        state.align.gate, state.align.native, state.align.profiled, state.align.doubled, state.align.typeOutside,
        state.align.restored, state.align.refused, state.align.reasons,
        state.refix.timer ? "true" : "false", state.refix.scans, state.refix.runs, state.refix.items,
        state.refix.deferred, state.refix.lastError, state.refix.released, state.refix.overflow, undoPast, undoFuture,
        state.appearance.gate, state.appearance.frames, state.appearance.styled, state.appearance.fieldsBefore,
        state.appearance.fieldsAfter, state.appearance.contentsBefore, state.appearance.contentsAfter,
        state.appearance.fillFirst);
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
    if (std::strcmp(selector, "width-side") == 0) {
        std::string text;
        if (WidthSide(message.d, input, text)) text = "width-side failed";
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "align-survey") == 0) {
        std::string text;
        if (AlignSurvey(message.d, input, text)) text = "align-survey failed";
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "char-limits") == 0) {
        std::string text;
        if (CharLimits(message.d, text)) text = "char-limits failed";
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "style-dump") == 0) {
        std::string text;
        const auto err = StyleDump(message.d, text);
        if (err) text = "style-dump failed";
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "status") == 0 && input[0] == '\0') actionError = kNoErr;
    else if (std::strcmp(selector, "appearance") == 0 && std::strcmp(input, "stroke-below") == 0)
        actionError = AppearanceStrokeBelow(message.d, state);
    else if (std::strcmp(selector, "appearance") == 0 && std::strcmp(input, "fill-above") == 0)
        actionError = AppearanceStrokeBelow(message.d, state, AppearanceKind::FillAbove);
    else if (std::strcmp(selector, "appearance") == 0 && std::strncmp(input, "miter:", 6) == 0)
        actionError = CharacterJoin(message.d, state, input + 6);
    else if (std::strcmp(selector, "appearance") == 0 && std::strncmp(input, "fill-first:", 11) == 0)
        actionError = FillFirst(message.d, state, input + 11);
    else if (std::strcmp(selector, "dash-align") == 0)
        actionError = DashAlign(message.d, state, input);
    else if (std::strcmp(selector, "refix") == 0 && (std::strcmp(input, "on") == 0 || std::strcmp(input, "off") == 0)) {
        // Pauses or resumes the automatic re-fix (tests; a user switch later).
        Suite<AITimerSuite> timer(message.d.basic, kAITimerSuite, kAITimerVersion);
        actionError = timer.error || !state.refix.timer ? kBadParameterErr
                    : timer->SetTimerActive(state.refix.timer, std::strcmp(input, "on") == 0);
    }
    else if (std::strcmp(selector, "native-route") == 0 && input[0] == '\0')
        actionError = NativeRoute(message.d, state);
    else if (std::strcmp(selector, "native-align") == 0) {
        NativeAlignStats st;
        actionError = NativeAlign(message.d, input, st);
        auto& a = state.align;
        a.gate = actionError ? "partial" : "aligned";
        a.native = st.native; a.profiled = st.profiled; a.doubled = st.doubled; a.typeOutside = st.typeOutside;
        a.restored = st.restored; a.refused = st.refused;
        std::snprintf(a.reasons, sizeof a.reasons, "%s", st.reasons.c_str());
    }
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
    char payload[8192]{};
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
                    state->refix.timer = startupState.refix.timer;
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
        if (std::strcmp(caller, kCallerAITimer) == 0 && std::strcmp(selector, kSelectorAIGoTimer) == 0) {
            auto& tm = *static_cast<AITimerMessage*>(raw);
            if (!state->refix.timer || tm.timer != state->refix.timer) return finish(kNoErr);
            region_probe::TelemetryScope telemetry(&state->telemetry);
            return finish(region_probe::Refix(tm.d, *state));
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
