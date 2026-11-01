// PROPOSED region-prep-51: real-core closed-region replacement for the marked
// analytic fixture only. RegionNative.h carries the validated integration:
// parser-checked source admission, the closed-form fixture oracle, and the
// owned-output SDK backend. Arbitrary artwork is refused before staging.
#include "SPBasic.h"
#include "SPInterf.h"
#include "SPAccess.h"
#include "SPBlocks.h"
#include "AIScriptMessage.h"
#include "AIUnicodeString.h"
#include "RegionNative.h"
#include "StrokeBehind.h"
#include "TextBehind.h"
#include "NativeMiter.h"
#include "NativeRegion.h"
#include "IllustratorSettings.h"
#include <algorithm>
#include <functional>
#include "AIMatchingArt.h"
#include "AIMdMemory.h"
#include "AIPaintStyle.h"
#include "AIATEPaint.h"
#include "AICustomColor.h"
#include "AIBeautifulStrokes.h"
#include "AIEntry.h"
#include "AIArray.h"
#include "AITimer.h"
#include "AIUndo.h"
#include "AIGroup.h"
#include "AIDocumentList.h"
#include "../source/IseRegionCore.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

extern "C" { AIUnicodeStringSuite* sAIUnicodeString = nullptr; SPBlocksSuite* sSPBlocks = nullptr; }

// The two suites ai::UnicodeString reads, acquired the first time a Unicode string is needed (the
// undo text, the panel) and released at shutdown; false when either is unavailable.
inline bool EnsureUnicode(SPBasicSuite* basic) {
    if (!basic) return false;
    if (!sSPBlocks) {
        const void* blocks = nullptr;
        if (!basic->AcquireSuite(kSPBlocksSuite, kSPBlocksSuiteVersion, &blocks) && blocks)
            sSPBlocks = static_cast<SPBlocksSuite*>(const_cast<void*>(blocks));
    }
    if (sSPBlocks && !sAIUnicodeString) {
        const void* suite = nullptr;
        if (!basic->AcquireSuite(kAIUnicodeStringSuite, kAIUnicodeStringSuiteVersion, &suite) && suite)
            sAIUnicodeString = static_cast<AIUnicodeStringSuite*>(const_cast<void*>(suite));
    }
    return sSPBlocks && sAIUnicodeString;
}

namespace region_probe {
// The panel's switches (IllustratorSettings.h): read from the settings file at load, and again
// whenever the panel or the bridge's "settings" command changes them.
inline ai_settings::Settings& Switches() { static ai_settings::Settings s = ai_settings::Load(); return s; }
constexpr char build[] = "region-prep-51";

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

// Fifth effect: Close holes (the owner, 2026-10-31: "close all holes"). On a fill of the
// Appearance panel's, below the strokes, it turns the art (text as glyph outlines) into the
// silhouette of a stroke that reaches kCloseHolesReach beyond it, every hole dropped
// (ise_region::Silhouette), which the fill paints in the stroke's color. Illustrator keeps the
// result in the document, so it still shows without the plugin; it follows edits with it.
constexpr char closeHolesIdentity[] = "org.strokemaster.close-holes.v1";
// Sixth effect: flat glyph tips, the opt-in (the owner, 2026-11-01): on a fill below the strokes,
// the region an outward stroke paints around the art with InDesign's tip geometry
// (ise_region::TippedOutline), in the stroke's color. Like Close holes, its last result stays in
// the document without the plugin, and follows edits only with it.
constexpr char flatTipsIdentity[] = "org.strokemaster.flat-tips.v1";
constexpr char kCloseHolesReach[] = "StrokeMasterCloseHolesReach";
// The stroke's join (an AILineJoin) and miter limit, with which the holes it encloses are found;
// without them, as written before 2026-11-01, the bevel's.
constexpr char kCloseHolesJoin[] = "StrokeMasterCloseHolesJoin";
constexpr char kCloseHolesLimit[] = "StrokeMasterCloseHolesLimit";
// The effect's revision: Illustrator keeps an effect's last result in the document and computes it
// again only when its parameters change, so a fill made by an older build is rewritten (and so
// computed again) when this differs. 2: holes found by the stroke's own join, past its edge.
// 3: paths and compound paths too, holes alone beneath a dashed stroke, unfilled letters hollow.
// 4: outline the entire text paint pass before finding holes, including separate stories.
constexpr char kCloseHolesRevisionKey[] = "StrokeMasterCloseHolesRevision";
constexpr ai::int32 kCloseHolesRevision = 4;
// 1: a dashed stroke, beneath which only the holes are filled, so its gaps stay open.
constexpr char kCloseHolesPattern[] = "StrokeMasterCloseHolesPattern";
// 1: nothing fills the letters or the shape, which stay hollow; only what lies outside them is filled.
constexpr char kCloseHolesHollow[] = "StrokeMasterCloseHolesHollow";
AILiveEffectHandle gCloseHoles = nullptr;   // State::closeHoles, for the fix
AILiveEffectHandle gFlatTips = nullptr;     // State::flatTips, for the fix
char gPathJudged[160] = "";                 // the last path stroke FixPath judged (status, for tests)

// Result of the last "appearance" bridge command: a native, effect-free object-level
// stroke added beneath the contents of every text frame in the document.
struct AppearanceStats {
    const char* gate = "ready";
    int frames = 0, styled = 0, fieldsBefore = -1, fieldsAfter = -1, contentsBefore = -1, contentsAfter = -1;
    int fillFirst = 0;   // native-route: frames given per-character stroke-behind (FillFirst off)
    int objectStrokes = 0;   // native-route: frames whose own strokes were moved below their fills
};

// Result of the last "native-fix" bridge command (NativeFix): the owner's miter rulings and
// the Outside rule for overlapping contours, written as native attributes.
struct NativeFixStats {
    const char* gate = "ready";
    int paths = 0, pathLimits = 0, reordered = 0, frames = 0, chars = 0, charLimits = 0, mapped = 0;
    int keptBeveled = 0, cutMitered = 0, corners = 0, dashes = 0;
    int pathfinder = 0;    // Inside or dashed strokes on overlapping contours given Pathfinder Add
    int dashRefused = 0;   // dashed strokes Illustrator's dash alignment does not accept
    int released = 0;      // re-fix: items the user had changed by hand, unmarked and left alone
    int offsetSynced = 0;  // Insets drawn as two fills whose shrunk fill was shown or hidden again
    int rerouted = 0;      // re-fix: routed frames or story groups whose route followed an edit
    int routeReleased = 0; // re-fix: route fields the user had changed, left to them
    int failed = 0;        // items whose fix failed (the others were still fixed)
    int fieldLimits = 0;   // Appearance strokes besides a path's own given a limit
    int objectStrokes = 0; // text objects whose own strokes were moved below their fills
    int realigned = 0;     // dashed Outside opt-ins undone for the alignment the user set since
    int holes = 0;         // Close holes' fills added, updated, or taken off
    int tips = 0;          // glyph tips' fills (TipFields) added, updated, or taken off
    int carried = 0;       // re-fix: a limit set on unstroked characters carried to the strokes below
    int orderRestored = 0; // text objects' own strokes put back above (Strokes behind off)
};

// The automatic re-fix (2026-10-16): art the native fix has written to is marked in its
// dictionary, and a timer re-runs the fix on marked art whose time stamp has changed since,
// appended to the edit's own undo step. Plain data, so State stays a raw block.
struct RefixState {
    AITimerHandle timer = nullptr;
    AIDocumentHandle document = nullptr;
    size_t global = 0;                 // the document's global stamp at the last scan
    struct Seen { AIArtHandle art; size_t stamp; };
    // Sorted by handle after each scan. 4,096 entries left every later item of a large document
    // unfollowed, first seen at every scan (the review of 2026-11-01).
    static constexpr int kSeen = 65536;
    Seen seen[kSeen] = {};
    int count = 0;
    unsigned long long scans = 0, runs = 0, items = 0, deferred = 0, released = 0, overflow = 0;
    unsigned long long rerouted = 0, routeReleased = 0;
    unsigned long long dashed = 0;     // paths given the dashed Outside opt-in (RefixDashes)
    unsigned long long automatic = 0;  // new or edited art corrected automatically (AutoCorrect)
    unsigned long long skipped = 0;    // edits too many at once to correct automatically
    unsigned long long redraws = 0;    // RedrawDocument calls after a correction from the timer
    unsigned long long threaded = 0;   // threaded text grouped across its frames automatically
    unsigned long long opened = 0;     // items corrected as their document opened (the opt-in)
    // Diagnosis of the owner's session (retests S4, S5, S7, S9, 2026-10-31, automatic correction
    // seen doing nothing): scans that found another document than the last, and scans that waited
    // while text had the focus.
    unsigned long long docSwitches = 0, typingWaits = 0;
    unsigned long long reapplied = 0;  // corrected art brought to switches changed since (Reapply)
    unsigned long long regrouped = 0;  // corrected frames of threaded text grouped once edited
    // Each open document's last scan and the switches its corrections follow, so switching
    // between documents is no opening, and each follows a switch changed while another was active
    // (the owner, 2026-10-31: the active document at once, the others when they come back).
    struct DocMemo { AIDocumentHandle doc; size_t global; ai_settings::Settings applied; bool adopt; };
    DocMemo docs[32] = {};
    int docCount = 0;
    ai_settings::Settings applied{};   // the switches the active document's corrections follow
    bool opening = false;              // the active document has just opened (Also documents as they open)
    bool paused = false;               // automatic correction was off or paused at the last tick
    bool waiting = false;              // a changed text frame waits for typing to end
    int lastError = 0;
};

struct AlignStats {
    const char* gate = "ready";
    int native = 0, profiled = 0, doubled = 0, typeOutside = 0, restored = 0, refused = 0, offset = 0;
    char reasons[208] = {};
};

// ---- Correct Selection: the panel's button and the bridge's "correct" (2026-10-30) ----------------
// What strokemaster-fix-story.jsx, strokemaster-fix-selection.jsx, and the opt-in scripts do,
// in one undoable step and gated by the panel's switches (IllustratorSettings.h):
//   1. Across a story's threaded frames: each complete story whose frames are all selected and
//      share a parent is grouped, as the story script does it: items between its frames that
//      draw nothing move below the last frame; visible artwork keeps its place and splits the
//      story into runs, each run of two or more grouped and routed (story-route, "part" when
//      split); a run the route refuses is taken apart again, its frames in their order.
//      Across separate stories (Across other text objects in the panel): complete stories with no visible artwork
//      between them go into one group (story-route "separate") first.
//   2. Strokes behind other letters' fills: every other selected text frame, native-route.
//   3. native-fix on everything corrected (miter limits, overlapping contours, dashes), as the
//      switches allow.
//   4. The opt-in: dashed Outside strokes (native-align dashes). Strokes Illustrator cannot align
//      are the panel menu's Align commands instead: Illustrator keeps no alignment the plugin can
//      read on an open path (host test align1), so there is nothing for a switch to follow.
// The selection is left on what was corrected.
struct CorrectStats {
    const char* gate = "ready";
    int stories = 0, runs = 0, moved = 0, split = 0, frames = 0, paths = 0, aligned = 0, dashes = 0, refused = 0;
    int unrouted = 0, dissolved = 0, undashed = 0;   // taken off because their switch is off
    const char* refusal = "";   // the story route's gate for the last run it refused
};

struct State {
    AILiveEffectHandle effect = nullptr;
    AppearanceStats appearance{};
    AILiveEffectHandle census = nullptr;
    AILiveEffectHandle strokeBehind = nullptr;
    stroke_behind::Stats strokeBehindStats{};
    AILiveEffectHandle textBehind = nullptr;
    text_behind::Stats textBehindStats{};
    AILiveEffectHandle closeHoles = nullptr;
    AILiveEffectHandle flatTips = nullptr;
    struct { unsigned long long calls = 0, published = 0, passed = 0; const char* gate = "ready"; } flatTipsStats;
    struct { unsigned long long calls = 0, published = 0, passed = 0, contours = 0, rings = 0; const char* gate = "ready"; } closeHolesStats;
    NativeFixStats nativeFix{};
    AlignStats align{};
    RefixState refix{};
    CorrectStats correct{};
    bool correctPending = false;   // the panel's Correct Selection, run at the next timer message
    bool refixRefreshOnly = false; // the timer's re-fix only refreshes its stamps after a correction
    bool refixPaused = false;      // the bridge's "refix off" (the timer keeps running Correct Selection)
    int alignPending = 0;          // the panel menu's Align Selection Outside (1), Inside (2), Restore (3)
    // Research: the Stroke panel's Align Stroke buttons, as the panel sets them (on the selection),
    // which a script cannot (SetStrokeAlignmentOnSelection refuses there): Outside (1), Inside (2),
    // Center (3), run at the next timer message; the answer kept for the status.
    int panelAlignPending = 0;
    int panelAlignResult = -1;
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
    char closeHolesTitle[] = "StrokeMaster Close Holes";
    if (!RegisterOne(message, *live.operator&(), closeHolesIdentity, closeHolesTitle, censusInput, state.closeHoles))
        gCloseHoles = state.closeHoles;
    char flatTipsTitle[] = "StrokeMaster Flat Tips";
    if (!RegisterOne(message, *live.operator&(), flatTipsIdentity, flatTipsTitle, censusInput, state.flatTips))
        gFlatTips = state.flatTips;
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

// Close holes' effect (see closeHolesIdentity): the art's outlines (glyphs, paths, compound paths,
// in groups) replaced by their silhouette at the reach the parameters give. Art it cannot read
// passes through, as the SDK asks of effects.
void CollectOutlines(const AIArtSuite& art, const AIPathSuite& path, AIArtHandle a, std::vector<ise_miter::Contour>& out,
                     AIArtHandle* firstPath = nullptr, int depth = 0) {
    if (!a || depth > 16) return;
    short type = kUnknownArt;
    if (art.GetArtType(a, &type)) return;
    if (type == kPathArt) {
        if (firstPath && !*firstPath) *firstPath = a;
        ai::int16 count = 0;
        AIBoolean closed = false;
        if (path.GetPathSegmentCount(a, &count) || count < 3 || count > 32000 || path.GetPathClosed(a, &closed)) return;
        std::vector<AIPathSegment> segs(static_cast<size_t>(count));
        ise_miter::Contour c;
        if (!path.GetPathSegments(a, 0, count, segs.data()) && illustrator_contour::ConvertSdk(segs.data(), segs.size(), true, &c))
            out.push_back(std::move(c));
        return;
    }
    if (type == kGroupArt || type == kCompoundPathArt) {
        AIArtHandle child = nullptr;
        for (art.GetArtFirstChild(a, &child); child;) {
            CollectOutlines(art, path, child, out, firstPath, depth + 1);
            if (art.GetArtSibling(child, &child)) return;
        }
    }
}

// An effect's result: `shape` as a compound path in place of the art, painted as the art it
// replaces is (`source`'s paint, the field's). False, with `gate` saying why, when it cannot be
// written; the art then passes through.
bool PublishShape(AILiveEffectGoMessage& go, const AIArtSuite& art, const AIPathSuite& path, const AIPathStyleSuite& paint,
                  const std::vector<ise_miter::Contour>& shape, AIArtHandle source, const char*& gate) {
    AIArtHandle compound = nullptr;
    if (art.NewArt(kCompoundPathArt, kPlaceAbove, go.art, &compound) || !compound) { gate = "new-art"; return false; }
    auto point = [](const ise_miter::Pt& q) { AIRealPoint r; r.h = static_cast<AIReal>(q.x); r.v = static_cast<AIReal>(q.y); return r; };
    bool written = true;
    for (const ise_miter::Contour& c : shape) {
        std::vector<AIPathSegment> segs;
        AIPathSegment first{};
        first.p = first.in = first.out = point(c.start);
        first.corner = true;
        segs.push_back(first);
        for (const ise_miter::Segment& s : c.segments) {
            if (s.curve) segs.back().out = point(s.c1);
            AIPathSegment next{};
            next.p = next.in = next.out = point(s.p);
            next.corner = true;
            if (s.curve) next.in = point(s.c2);
            segs.push_back(next);
        }
        // The closing segment ends where the contour starts: its handle is the first point's.
        if (segs.size() > 1 && std::fabs(segs.back().p.h - segs.front().p.h) < 1e-6 && std::fabs(segs.back().p.v - segs.front().p.v) < 1e-6) {
            segs.front().in = segs.back().in;
            segs.pop_back();
        }
        if (segs.size() < 3 || segs.size() > 32000) continue;
        AIArtHandle ring = nullptr;
        if (art.NewArt(kPathArt, kPlaceInsideOnTop, compound, &ring) || !ring ||
            path.SetPathSegmentCount(ring, static_cast<ai::int16>(segs.size())) ||
            path.SetPathSegments(ring, 0, static_cast<ai::int16>(segs.size()), segs.data()) || path.SetPathClosed(ring, true)) {
            written = false;
            break;
        }
    }
    // Painted as the art it replaces is (the fill's paint): new art is otherwise black (the first
    // host test, 2026-10-31).
    AIPathStyle style{};
    AIBoolean advanced = false;
    if (written && source && !paint.GetPathStyle(source, &style, &advanced)) {
        style.strokePaint = false;
        written = paint.SetPathStyle(compound, &style) == kNoErr;
    }
    if (!written) { art.DisposeArt(compound); gate = "write"; return false; }
    art.DisposeArt(go.art);
    go.art = compound;
    return true;
}


ASErr CloseHolesGo(AILiveEffectGoMessage& go, State& state) {
    auto& st = state.closeHolesStats;
    ++st.calls;
    Suite<AIArtSuite> art(go.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(go.d.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIDictionarySuite> dict(go.d.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIPathStyleSuite> paint(go.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    if (art.error || path.error || dict.error || paint.error || !go.art) { st.gate = "suites"; ++st.passed; return kNoErr; }
    AIReal reach = 0;
    if (!go.parameters || dict->GetRealEntry(go.parameters, dict->Key(kCloseHolesReach), &reach) || !(reach > 0)) {
        st.gate = "parameters"; ++st.passed; return kNoErr;
    }
    ai::int32 joinValue = kAIBevelJoin, pattern = 0, hollow = 0;
    AIReal limit = 0;
    dict->GetIntegerEntry(go.parameters, dict->Key(kCloseHolesPattern), &pattern);
    dict->GetIntegerEntry(go.parameters, dict->Key(kCloseHolesHollow), &hollow);
    dict->GetIntegerEntry(go.parameters, dict->Key(kCloseHolesJoin), &joinValue);
    dict->GetRealEntry(go.parameters, dict->Key(kCloseHolesLimit), &limit);
    const ise_region::BandJoin join = joinValue == kAIMiterJoin  ? ise_region::BandJoin::Miter
                                            : joinValue == kAIRoundJoin ? ise_region::BandJoin::Round
                                                                        : ise_region::BandJoin::Bevel;
    std::vector<ise_miter::Contour> outlines;
    AIArtHandle source = nullptr;
    CollectOutlines(*art.operator&(), *path.operator&(), go.art, outlines, &source);
    st.contours = outlines.size();
    if (outlines.empty()) { short type = kUnknownArt; art->GetArtType(go.art, &type); st.gate = type == kTextFrameArt ? "input.text" : "input.empty"; }
    // Short of the stroke's edge by twice the tolerance, so the fitted curves never show past it.
    const double tol = 0.01;
    bool ok = false;
    const std::vector<ise_miter::Contour> shape =
        outlines.empty() ? std::vector<ise_miter::Contour>()
                         : ise_region::Silhouette(outlines, (std::max)(0.0, static_cast<double>(reach) - 2 * tol), tol, &ok, join, limit,
                                                  pattern != 0, hollow != 0);
    if (!ok || shape.empty()) { if (!outlines.empty()) st.gate = "silhouette"; ++st.passed; return kNoErr; }
    st.rings = shape.size();
    const char* gate = "published";
    if (PublishShape(go, *art.operator&(), *path.operator&(), *paint.operator&(), shape, source, gate)) ++st.published;
    else ++st.passed;
    st.gate = gate;
    return kNoErr;
}

// The flat tips' effect (see flatTipsIdentity): parameters "ofst" (the stroke's reach) and "mlim"
// (its miter limit), as the native tip fills carry them (TipFields).
ASErr FlatTipsGo(AILiveEffectGoMessage& go, State& state) {
    auto& st = state.flatTipsStats;
    ++st.calls;
    Suite<AIArtSuite> art(go.d.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIPathSuite> path(go.d.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIDictionarySuite> dict(go.d.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIPathStyleSuite> paint(go.d.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    if (art.error || path.error || dict.error || paint.error || !go.art) { st.gate = "suites"; ++st.passed; return kNoErr; }
    AIReal reach = 0, limit = 4;
    if (!go.parameters || dict->GetRealEntry(go.parameters, dict->Key("ofst"), &reach) || !(reach > 0)) {
        st.gate = "parameters"; ++st.passed; return kNoErr;
    }
    dict->GetRealEntry(go.parameters, dict->Key("mlim"), &limit);
    std::vector<ise_miter::Contour> outlines;
    AIArtHandle source = nullptr;
    CollectOutlines(*art.operator&(), *path.operator&(), go.art, outlines, &source);
    if (outlines.empty()) { st.gate = "input.empty"; ++st.passed; return kNoErr; }
    bool ok = false;
    const std::vector<ise_miter::Contour> shape = ise_region::TippedOutline(outlines, reach, (std::max)(1.0, static_cast<double>(limit)), 0.01, &ok);
    if (!ok || shape.empty()) { st.gate = "outline"; ++st.passed; return kNoErr; }
    const char* gate = "published";
    if (PublishShape(go, *art.operator&(), *path.operator&(), *paint.operator&(), shape, source, gate)) ++st.published;
    else ++st.passed;
    st.gate = gate;
    return kNoErr;
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

// Whether a paint field is a stroke that paints: visible, a color, and a width.
bool PaintingStroke(const AIArtStyleParserSuite& parser, AIParserPaintField f) {
    if (!parser.IsStroke(f)) return false;
    AIBoolean visible = true;
    AIStrokeStyle stroke{};
    AIArtStylePaintData data;
    return !parser.GetPaintFieldVisible(f, &visible) && visible && !parser.GetStroke(f, &stroke, &data) &&
           stroke.color.kind != kNoneColor && stroke.width > 0;
}

// Whether a paint field is Close holes' own fill: a fill carrying the effect.
bool HoleField(const AIArtStyleParserSuite& parser, AIParserPaintField f) {
    if (!parser.IsFill(f)) return false;
    for (ai::int32 k = 0, n = parser.CountEffectsOfPaintField(f); k < n; ++k) {
        AIParserLiveEffect fx = nullptr;
        const char* name = nullptr;
        ai::int32 major = 0, minor = 0;
        if (!parser.GetNthEffectOfPaintField(f, k, &fx) && fx && !parser.GetLiveEffectNameAndVersion(fx, &name, &major, &minor) &&
            name && std::strcmp(name, closeHolesIdentity) == 0)
            return true;
    }
    return false;
}

// A style's paint fields, top first, one letter each: F a fill, S a stroke that paints, s one that
// does not, and C where Characters (or a group's Contents) sits.
std::string FieldOrder(const AIArtStyleParserSuite& parser, AIStyleParser p) {
    std::string out;
    const ai::int32 contents = parser.GetGroupContentsPosition(p), n = parser.CountPaintFields(p);
    for (ai::int32 i = 0; i < n; ++i) {
        if (i == contents) out += 'C';
        AIParserPaintField f = nullptr;
        if (parser.GetNthPaintField(p, i, &f) || !f) { out += '?'; continue; }
        out += parser.IsFill(f) ? 'F' : (parser.IsStroke(f) ? (PaintingStroke(parser, f) ? 'S' : 's') : '?');
    }
    if (contents >= n) out += 'C';
    return out;
}

std::string FieldOrderOf(const AIArtStyleSuite& styles, const AIArtStyleParserSuite& parser, AIArtHandle art) {
    AIArtStyleHandle style = nullptr;
    AIStyleParser p = nullptr;
    if (styles.GetArtStyle(art, &style) || !style || parser.NewParser(&p) || !p) return {};
    std::string out = parser.ParseStyle(p, style) ? std::string() : FieldOrder(parser, p);
    parser.DisposeParser(p);
    return out;
}

// Whether an item's style has a stroke that paints, anywhere in it (the Appearance panel's
// strokes included, which the item's own path style does not report).
bool HasPaintingStroke(const AIArtStyleSuite& styles, const AIArtStyleParserSuite& parser, AIArtHandle art) {
    return FieldOrderOf(styles, parser, art).find('S') != std::string::npos;
}

// The field order before the fix first moved a text object's strokes (StrokesBelowFills), so
// Correct Selection with Strokes behind off can put them back (RestoreFieldOrder).
constexpr char kRecordOrderBefore[] = "StrokeMasterOrderBefore";

// Text with paint of its own (the Appearance panel's Add New Stroke on a type object): its strokes
// that paint moved below Characters and below every fill, in their own order, so they paint
// beneath the letters' fills as the stroke route puts the characters' own strokes (the owner's
// retest Q1, 2026-10-31: a stroke added above Characters, or above a fill of the object's own, was
// left on top, and nothing was corrected). Fills, Characters, and strokes that paint nothing keep
// their places. `changed`: the style was rewritten.
ASErr StrokesBelowFills(const AIArtSuite& art, const AIDictionarySuite& dict, const AIArtStyleSuite& styles,
                        const AIArtStyleParserSuite& parser, AIArtHandle item, bool& changed) {
    changed = false;
    AIArtStyleHandle style = nullptr;
    auto e = styles.GetArtStyle(item, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    if ((e = parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{parser, p};
    if ((e = parser.ParseStyle(p, style)) != kNoErr) return e;
    const ai::int32 contents = parser.GetGroupContentsPosition(p), all = parser.CountPaintFields(p);
    if (contents < 0) return kNoErr;
    // Close holes' fill stays last, beneath the strokes: the fields considered stop above it.
    ai::int32 tail = 0;
    {
        AIParserPaintField last = nullptr;
        if (all > 0 && !parser.GetNthPaintField(p, all - 1, &last) && last && HoleField(parser, last)) tail = 1;
    }
    const ai::int32 n = all - tail;
    std::vector<AIParserPaintField> move;
    ai::int32 above = 0;
    bool inPlace = true;
    for (ai::int32 i = 0; i < n; ++i) {
        AIParserPaintField f = nullptr;
        if (parser.GetNthPaintField(p, i, &f) || !f || !PaintingStroke(parser, f)) continue;
        if (i < contents) ++above;
        move.push_back(f);
    }
    const ai::int32 k = static_cast<ai::int32>(move.size());
    if (k == 0) return kNoErr;
    // Already in place: the strokes that paint are the last fields, under Characters.
    if (contents > n - k) inPlace = false;
    for (ai::int32 j = 0; j < k && inPlace; ++j) {
        AIParserPaintField f = nullptr;
        if (parser.GetNthPaintField(p, n - k + j, &f) || f != move[static_cast<size_t>(j)]) inPlace = false;
    }
    if (inPlace) return kNoErr;
    // Without Close holes' fill, which RestoreFieldOrder leaves last: with it in the record, the
    // two orders never matched and the order was never put back (the review of 2026-11-01).
    std::string before = FieldOrder(parser, p);
    if (tail && !before.empty() && before.back() == 'F') before.pop_back();
    for (AIParserPaintField f : move) if ((e = parser.RemovePaintField(p, f, false)) != kNoErr) return e;
    for (AIParserPaintField f : move)
        if ((e = parser.InsertNthPaintField(p, tail ? parser.CountPaintFields(p) - tail : -1, f)) != kNoErr) return e;
    if ((e = parser.MoveGroupContentsPosition(p, contents - above)) != kNoErr) return e;
    if (parser.GetGroupContentsPosition(p) != contents - above) return kBadParameterErr;
    AIArtStyleHandle created = nullptr;
    if ((e = parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    if ((e = styles.SetArtStyle(item, created)) != kNoErr) return e;
    changed = true;
    // The order the user made, kept from the first move on.
    AIDictionaryRef d = nullptr;
    if (!art.GetDictionary(item, &d) && d) {
        const char* had = nullptr;
        if (dict.GetStringEntry(d, dict.Key(kRecordOrderBefore), &had) || !had || !*had)
            dict.SetStringEntry(d, dict.Key(kRecordOrderBefore), before.c_str());
        dict.Release(d);
    }
    return kNoErr;
}

// StrokesBelowFills undone (Correct Selection, Strokes behind off): the fields back in the order
// recorded before the first move, when the item still has the same fields; the record goes.
ASErr RestoreFieldOrder(const AIArtSuite& art, const AIDictionarySuite& dict, const AIArtStyleSuite& styles,
                        const AIArtStyleParserSuite& parser, AIArtHandle item, bool& changed) {
    changed = false;
    if (!art.HasDictionary(item)) return kNoErr;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return kNoErr;
    struct ReleaseDict { const AIDictionarySuite& api; AIDictionaryRef d; ~ReleaseDict() { api.Release(d); } } releaseDict{dict, d};
    const char* text = nullptr;
    if (dict.GetStringEntry(d, dict.Key(kRecordOrderBefore), &text) || !text || !*text) return kNoErr;
    const std::string recorded = text;
    // Deleted once restored, or once it no longer fits the fields; kept when a step fails, so the
    // order can still be put back (the second review of 2026-11-01).
    AIArtStyleHandle style = nullptr;
    auto e = styles.GetArtStyle(item, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    if ((e = parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{parser, p};
    if ((e = parser.ParseStyle(p, style)) != kNoErr) return e;
    // Close holes' fill, added since and last, stays last.
    const ai::int32 n = parser.CountPaintFields(p);
    AIParserPaintField hole = nullptr;
    if (n > 0 && !parser.GetNthPaintField(p, n - 1, &hole) && hole && !HoleField(parser, hole)) hole = nullptr;
    std::string now = FieldOrder(parser, p);
    if (hole) now.pop_back();
    // A record made before 2026-11-01 kept the hole's fill: dropped the same way.
    std::string before = recorded;
    // A record from before 2026-11-01 kept Close holes' fill, which may be gone since.
    if (before.size() == now.size() + 1 && before.back() == 'F') before.pop_back();
    // The same letters in another order, or nothing is done: the fields were changed since.
    std::string a = before, b = now;
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    if (a != b || now == before) { dict.DeleteEntry(d, dict.Key(kRecordOrderBefore)); return kNoErr; }
    // Each letter of the recorded order takes the next field of its kind, in the order they are now.
    std::map<char, std::vector<AIParserPaintField>> byKind;
    for (ai::int32 i = 0, at = 0; i < n - (hole ? 1 : 0); ++i) {
        AIParserPaintField f = nullptr;
        if (parser.GetNthPaintField(p, i, &f) || !f) return kNoErr;
        if (now[static_cast<size_t>(at)] == 'C') ++at;
        byKind[now[static_cast<size_t>(at++)]].push_back(f);
    }
    std::vector<AIParserPaintField> order;
    ai::int32 contents = 0;
    std::map<char, size_t> next;
    for (char c : before) {
        if (c == 'C') { contents = static_cast<ai::int32>(order.size()); continue; }
        order.push_back(byKind[c][next[c]++]);
    }
    if (hole) order.push_back(hole);
    for (AIParserPaintField f : order) if ((e = parser.RemovePaintField(p, f, false)) != kNoErr) return e;
    for (AIParserPaintField f : order) if ((e = parser.InsertNthPaintField(p, -1, f)) != kNoErr) return e;
    if ((e = parser.MoveGroupContentsPosition(p, contents)) != kNoErr) return e;
    AIArtStyleHandle created = nullptr;
    if ((e = parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    if ((e = styles.SetArtStyle(item, created)) != kNoErr) return e;
    dict.DeleteEntry(d, dict.Key(kRecordOrderBefore));
    changed = true;
    return kNoErr;
}

// Two solid colors the same: one kind, every component equal (a spot color by its handle
// and tint). Gradients and patterns never compare equal here.
bool SameSolid(const AIColor& a, const AIColor& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
    case kGrayColor: return a.c.g.gray == b.c.g.gray;
    case kFourColor: return a.c.f.cyan == b.c.f.cyan && a.c.f.magenta == b.c.f.magenta &&
                            a.c.f.yellow == b.c.f.yellow && a.c.f.black == b.c.f.black;
    case kThreeColor: return a.c.rgb.red == b.c.rgb.red && a.c.rgb.green == b.c.rgb.green && a.c.rgb.blue == b.c.rgb.blue;
    case kCustomColor: return a.c.c.color == b.c.c.color && a.c.c.tint == b.c.c.tint;
    default: return false;
    }
}

// Two strokes the route takes for one: color, overprint, weight, cap, join, and dashes. The
// miter limit is left out: native-fix gives each character its own, and sets the route's
// stroke's afterward (review 2026-10-30: per-character limits had kept fixed frames off the
// stroke route).
bool SameStroke(const AIStrokeStyle& a, const AIStrokeStyle& b) {
    if (!SameSolid(a.color, b.color) || !a.overprint != !b.overprint || a.width != b.width || a.cap != b.cap ||
        a.join != b.join || a.dash.length != b.dash.length ||
        (a.dash.length && a.dash.offset != b.dash.offset))
        return false;
    for (ai::int16 i = 0; i < a.dash.length && i < kMaxDashComponents; ++i)
        if (a.dash.array[i] != b.dash.array[i]) return false;
    return true;
}

// ---- The text route as one decision (2026-10-30) ---------------------------------------------
// Stroke-behind written as ordinary Appearance data, on a text frame or on a group holding one
// complete story, from the characters' paint across every frame it covers:
//   no stroke anywhere     -> nothing to do
//   one stroke everywhere  -> an object-level stroke below Characters (or the group's Contents),
//                             and the characters' strokes cleared: the fills keep their own
//                             colors, so a later recolor is drawn with or without the plugin
//                             (the owner's ruling: uniform text strokes stay with Illustrator's
//                             own Appearance stroke)
//   one fill everywhere    -> an object-level fill above Characters (or Contents); the character
//                             strokes are kept as typed. A later recolor of the characters is
//                             hidden by that fill until the re-fix follows it
//   both mixed             -> every character's stroke painted before its own fill (ATE FillFirst
//                             off): a later character's stroke can still reach an earlier one's
//                             fill; refused on a story group
// Until 2026-10-30 one fill came before one stroke; the recolor check of that day measured the
// fill route hiding a recolor (688 pt2 drawn in the old color) and the stroke route following it.
// The art styled keeps the route and the signature of the field it wrote, so the re-fix can tell
// a recolor of the characters (followed) from a change to that field (the user's: released).
constexpr char kRouteKind[] = "StrokeMasterRoute";
constexpr char kRouteSig[] = "StrokeMasterRouteSig";
// The story route's group, in builds before region-prep-22 the only record.
constexpr char kStoryMark[] = "StrokeMasterStory";
enum RouteKind { kRouteNone = 0, kRouteFill = 1, kRouteStroke = 2, kRouteFillFirst = 3 };

// Only this route installs group-level hole and tip fields. Other routes must
// retain each frame's fields even though their paint order belongs to the group.
inline bool GroupOwnsTextFields(RouteKind route) { return route == kRouteStroke; }

// A fill above group Contents also paints over child hole/tip fields. Keep
// independently corrected frames when those fields cannot move to one stroke.
inline bool GroupRouteSupportsFields(RouteKind route, bool holes, bool miter) {
    return route != kRouteFill || !(holes || miter);
}

struct RouteSuites {
    SPBasicSuite* basic;
    const AIArtSuite& art;
    const AIArtStyleSuite& styles;
    const AIArtStyleParserSuite& parser;
    const AITextFrameSuite& text;
    const ATE::TextRangeSuite& range;
    const ATE::CharFeaturesSuite& features;
    const AIATEPaintSuite& atePaint;
    const AIDictionarySuite& dict;
};

// A solid color as text, the same across sessions (a spot color by its tint and definition).
std::string ColorSig(SPBasicSuite* basic, const AIColor& c) {
    char b[200];
    switch (c.kind) {
    case kGrayColor: std::snprintf(b, sizeof b, "g%.6g", static_cast<double>(c.c.g.gray)); break;
    case kFourColor:
        std::snprintf(b, sizeof b, "f%.6g,%.6g,%.6g,%.6g", static_cast<double>(c.c.f.cyan), static_cast<double>(c.c.f.magenta),
                      static_cast<double>(c.c.f.yellow), static_cast<double>(c.c.f.black));
        break;
    case kThreeColor:
        std::snprintf(b, sizeof b, "r%.6g,%.6g,%.6g", static_cast<double>(c.c.rgb.red), static_cast<double>(c.c.rgb.green),
                      static_cast<double>(c.c.rgb.blue));
        break;
    case kCustomColor: {
        // By its definition, not its name: ai::UnicodeString is not linked into this plugin.
        Suite<AICustomColorSuite> custom(basic, kAICustomColorSuite, kAICustomColorVersion);
        AICustomColor def{};
        if (custom.error || !c.c.c.color || custom->GetCustomColor(c.c.c.color, &def)) return "s?";
        std::snprintf(b, sizeof b, "s%.6g:%d:%d:%.6g,%.6g,%.6g,%.6g", static_cast<double>(c.c.c.tint), static_cast<int>(def.kind),
                      static_cast<int>(def.flag), static_cast<double>(def.c.f.cyan), static_cast<double>(def.c.f.magenta),
                      static_cast<double>(def.c.f.yellow), static_cast<double>(def.c.f.black));
        break;
    }
    default: return "x";
    }
    return b;
}
std::string FillSig(SPBasicSuite* basic, const AIFillStyle& f) { return "F" + ColorSig(basic, f.color) + (f.overprint ? "o" : ""); }
// The miter limit is left out: native-fix sets it on the route's stroke.
std::string StrokeSig(SPBasicSuite* basic, const AIStrokeStyle& s) {
    char w[40];
    std::snprintf(w, sizeof w, "w%.6g", static_cast<double>(s.width));
    return "S" + ColorSig(basic, s.color) + (s.overprint ? "o" : "") + w;
}

// The characters' paint across a set of frames. A frame without characters is skipped; a frame
// with no stroke at all breaks "one stroke" (the object stroke would reach it).
struct CharPaint {
    bool any = false, noStroke = true, oneFill = true, oneStroke = true, haveFill = false, haveStroke = false;
    AIFillStyle fill{};
    AIStrokeStyle stroke{};
};
ASErr ReadCharPaint(const RouteSuites& s, const std::vector<AIArtHandle>& frames, CharPaint& out) {
    out = CharPaint{};
    for (AIArtHandle frame : frames) {
        TextRangeRef r = nullptr;
        if (s.text.GetATETextRange(frame, &r) || !r) return kBadParameterErr;
        struct RangeRelease { const ATE::TextRangeSuite& api; TextRangeRef r; ~RangeRelease() { api.Release(r); } } release{s.range, r};
        ATETextDOM::Int32 size = 0;
        if (s.range.GetSize(r, &size)) return kBadParameterErr;
        if (size <= 0) continue;
        ATE::CharFeaturesRef f = nullptr;
        AIPathStyle ps{};
        AIPathStyleMap map{};
        const auto ate = s.range.GetUniqueCharFeatures(r, &f);
        const auto pe = !ate && f ? s.atePaint.GetAIPathStyleAndMap(f, &ps, &map) : kBadParameterErr;
        if (f) s.features.Release(f);
        if (pe) return pe;
        out.any = true;
        const bool frameNoStroke = map.strokePaint && !ps.strokePaint;
        if (!frameNoStroke) out.noStroke = false;
        const bool frameFill = map.fillPaint && ps.fillPaint && map.fill.overprint && UniformSolid(ps.fill.color, map.fill.color);
        const bool frameStroke = map.strokePaint && ps.strokePaint && map.stroke.overprint && map.stroke.width &&
                                 map.stroke.join && map.stroke.cap && map.stroke.dash.length &&
                                 UniformSolid(ps.stroke.color, map.stroke.color);
        if (!frameFill || (out.haveFill && (!SameSolid(ps.fill.color, out.fill.color) || !ps.fill.overprint != !out.fill.overprint)))
            out.oneFill = false;
        else if (!out.haveFill) { out.fill = ps.fill; out.haveFill = true; }
        if (!frameStroke || (out.haveStroke && !SameStroke(ps.stroke, out.stroke))) out.oneStroke = false;
        else if (!out.haveStroke) { out.stroke = ps.stroke; out.haveStroke = true; }
    }
    out.oneFill = out.oneFill && out.haveFill;
    out.oneStroke = out.oneStroke && out.haveStroke;
    return kNoErr;
}

RouteKind ChooseRoute(const CharPaint& c) {
    if (!c.any || c.noStroke) return kRouteNone;
    if (c.oneStroke) return kRouteStroke;
    if (c.oneFill) return kRouteFill;
    return kRouteFillFirst;
}

// One character attribute over every frame: the stroke on or off, or FillFirst.
bool SetFrameChars(const RouteSuites& s, AIArtHandle frame, bool fillFirst, bool on) {
    TextRangeRef r = nullptr;
    if (s.text.GetATETextRange(frame, &r) || !r) return false;
    ATE::CharFeaturesRef f = nullptr;
    ATE::ATEErr ate = s.features.Initialize(&f);
    if (!ate && f) ate = fillFirst ? s.features.SetFillFirst(f, on) : s.features.SetStroke(f, on);
    if (!ate && f) ate = s.range.ReplaceOrAddLocalCharFeatures(r, f);
    if (f) s.features.Release(f);
    s.range.Release(r);
    return !ate && f;
}

// FillFirst turned off by Reroute under a stroke route (some characters restroked), to be turned
// back on by Unroute; without the mark, Unroute leaves it as the user had it (the second review of
// 2026-11-01: turning it on for every stroke route overrode the user's own setting).
constexpr char kFillFirstOff[] = "StrokeMasterFillFirstOff";
void MarkFillFirstOff(const RouteSuites& s, AIArtHandle target, bool on) {
    AIDictionaryRef d = nullptr;
    if (s.art.GetDictionary(target, &d) || !d) return;
    if (on) s.dict.SetBooleanEntry(d, s.dict.Key(kFillFirstOff), true);
    else s.dict.DeleteEntry(d, s.dict.Key(kFillFirstOff));
    s.dict.Release(d);
}
bool FillFirstOffMarked(const RouteSuites& s, AIArtHandle target) {
    if (!s.art.HasDictionary(target)) return false;
    AIDictionaryRef d = nullptr;
    if (s.art.GetDictionary(target, &d) || !d) return false;
    AIBoolean v = false;
    const bool marked = !s.dict.GetBooleanEntry(d, s.dict.Key(kFillFirstOff), &v) && v;
    s.dict.Release(d);
    return marked;
}

void WriteRouteRecord(const RouteSuites& s, AIArtHandle target, RouteKind kind, const std::string& sig) {
    AIDictionaryRef d = nullptr;
    if (s.art.GetDictionary(target, &d) || !d) return;
    if (kind == kRouteNone) { s.dict.DeleteEntry(d, s.dict.Key(kRouteKind)); s.dict.DeleteEntry(d, s.dict.Key(kRouteSig)); }
    else {
        s.dict.SetIntegerEntry(d, s.dict.Key(kRouteKind), kind);
        s.dict.SetStringEntry(d, s.dict.Key(kRouteSig), sig.c_str());
    }
    s.dict.DeleteEntry(d, s.dict.Key(kStoryMark));
    s.dict.Release(d);
}

RouteKind ReadRouteRecord(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle target, std::string* sig = nullptr) {
    if (!art.HasDictionary(target)) return kRouteNone;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(target, &d) || !d) return kRouteNone;
    ai::int32 v = 0;
    RouteKind kind = kRouteNone;
    if (!dict.GetIntegerEntry(d, dict.Key(kRouteKind), &v) && v >= kRouteFill && v <= kRouteFillFirst) kind = static_cast<RouteKind>(v);
    else if (!dict.GetIntegerEntry(d, dict.Key(kStoryMark), &v) && (v == 1 || v == 2)) kind = static_cast<RouteKind>(v);
    if (sig) {
        const char* text = nullptr;
        *sig = !dict.GetStringEntry(d, dict.Key(kRouteSig), &text) && text ? text : "";
    }
    dict.Release(d);
    return kind;
}

// A story group's stroke lifted from its frames' own strokes (LiftFrameStrokes) rather than
// made from their characters' is recorded with this after its signature, so that taking it off
// puts it back on the frames.
constexpr char kLiftedSig[] = "|frames";
std::string BaseSig(const std::string& sig) {
    const size_t at = sig.find(kLiftedSig);
    return at == std::string::npos ? sig : sig.substr(0, at);
}

// The field the route wrote, found by its side of Contents and its signature; nullptr when the
// user has changed or removed it.
AIParserPaintField FindRouteField(const RouteSuites& s, AIStyleParser p, RouteKind kind, const std::string& sig) {
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p), fields = s.parser.CountPaintFields(p);
    for (ai::int32 i = 0; i < fields; ++i) {
        AIParserPaintField f = nullptr;
        if (s.parser.GetNthPaintField(p, i, &f) || !f) continue;
        AIArtStylePaintData data;
        if (kind == kRouteFill && i < contents && s.parser.IsFill(f)) {
            AIFillStyle fill{};
            if (!s.parser.GetFill(f, &fill, &data) && FillSig(s.basic, fill) == sig) return f;
        } else if (kind == kRouteStroke && i >= contents && s.parser.IsStroke(f)) {
            AIStrokeStyle stroke{};
            if (!s.parser.GetStroke(f, &stroke, &data) && StrokeSig(s.basic, stroke) == BaseSig(sig)) return f;
        }
    }
    return nullptr;
}

// Predict the paint after removing individual routes, without mutating them.
// Stroke routes have disabled native strokes, so read their verified owned field.
ASErr ReadGroupingPaint(const RouteSuites& s, const std::vector<AIArtHandle>& frames, CharPaint& out) {
    out = CharPaint{};
    for (AIArtHandle frame : frames) {
        CharPaint one;
        if (ReadCharPaint(s, {frame}, one)) return kBadParameterErr;
        if (!one.any) continue;
        std::string sig;
        if (one.noStroke && ReadRouteRecord(s.art, s.dict, frame, &sig) == kRouteStroke) {
            AIArtStyleHandle style = nullptr;
            AIStyleParser parser = nullptr;
            if (s.styles.GetArtStyle(frame, &style) || !style || s.parser.NewParser(&parser) || !parser) return kBadParameterErr;
            struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, parser};
            if (s.parser.ParseStyle(parser, style)) return kBadParameterErr;
            AIParserPaintField field = FindRouteField(s, parser, kRouteStroke, sig);
            AIArtStylePaintData data;
            if (!field || s.parser.GetStroke(field, &one.stroke, &data)) return kBadParameterErr;
            one.noStroke = false;
            one.oneStroke = one.haveStroke = true;
        }
        out.any = true;
        out.noStroke = out.noStroke && one.noStroke;
        if (!one.oneFill || (out.haveFill && (!SameSolid(one.fill.color, out.fill.color) || !one.fill.overprint != !out.fill.overprint)))
            out.oneFill = false;
        else if (!out.haveFill) { out.fill = one.fill; out.haveFill = true; }
        if (!one.oneStroke || (out.haveStroke && !SameStroke(one.stroke, out.stroke))) out.oneStroke = false;
        else if (!out.haveStroke) { out.stroke = one.stroke; out.haveStroke = true; }
    }
    out.oneFill = out.oneFill && out.haveFill;
    out.oneStroke = out.oneStroke && out.haveStroke;
    return kNoErr;
}

// The route's own paint pass drawn as paths: Illustrator's Effect > Path > Outline Object
// ("Adobe Outline Type") at the head of the route's field, with the parameters the menu command
// writes plus a tag. Illustrator writes every paint pass of type as a copy of the text, so
// without it a PDF reads each character twice; with it the route's pass is paths, Characters
// stay live text, and a PDF extracts the text once (measured 2026-10-30: 48 of 48 where the
// route alone gave 96 and Illustrator's own uncorrected export 72). The effect is Illustrator's,
// so it follows edits with or without the plugin.
constexpr char kRoutePassKey[] = "StrokeMasterRoutePass";
ASErr AddOutlinePass(SPBasicSuite* basic, const AIArtStyleParserSuite& parser, const AIDictionarySuite& dict,
                     AIStyleParser p, AIParserPaintField field) {
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    if (live.error) return live.error;
    AILiveEffectHandle outline = nullptr;
    if (live->GetLiveEffectHandleByName("Adobe Outline Type", &outline) || !outline) return kBadParameterErr;
    for (ai::int32 k = 0, n = parser.CountEffectsOfPaintField(field); k < n; ++k) {
        AIParserLiveEffect fx = nullptr;
        AILiveEffectParameters params = nullptr;
        ai::int32 tag = 0;
        if (!parser.GetNthEffectOfPaintField(field, k, &fx) && fx && !parser.GetLiveEffectParams(fx, &params) && params &&
            !dict.GetIntegerEntry(params, dict.Key(kRoutePassKey), &tag) && tag == 1)
            return kNoErr;   // already there
    }
    AILiveEffectParameters params = nullptr;
    if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
    dict.SetBooleanEntry(params, dict.Key("-DefaultApplyEffectsKey"), false);
    dict.SetIntegerEntry(params, dict.Key(kRoutePassKey), 1);
    AIParserLiveEffect fx = nullptr;
    auto e = parser.NewParserLiveEffect(outline, params, &fx);
    dict.Release(params);
    if (e || !fx) return e ? e : kOutOfMemoryErr;
    if ((e = parser.InsertNthEffectOfPaintField(p, field, 0, fx)) != kNoErr) parser.DisposeParserLiveEffect(fx);
    return e;
}

ASErr AddOutlinePass(const RouteSuites& s, AIStyleParser p, AIParserPaintField field) {
    return AddOutlinePass(s.basic, s.parser, s.dict, p, field);
}

// Writes `kind` on `target` (a frame, or a story group) from the parsed style `p`, which must
// not yet hold a route field. The Contents position is set explicitly: a style built from a
// group's empty style had none, so the group's children were not drawn at all, only its paint on
// their outlines (every stroke lost under the story fill of region-prep-21, found 2026-10-30).
// Stroke below: the characters' strokes are cleared first, and put back if anything after fails.
ASErr ApplyRoute(const RouteSuites& s, AIArtHandle target, const std::vector<AIArtHandle>& frames, AIStyleParser p,
                 RouteKind kind, const CharPaint& c) {
    if (kind == kRouteNone) { WriteRouteRecord(s, target, kRouteNone, ""); return kNoErr; }
    if (kind == kRouteFillFirst) {
        for (AIArtHandle f : frames) if (!SetFrameChars(s, f, true, false)) return kBadParameterErr;
        WriteRouteRecord(s, target, kRouteFillFirst, "");
        return kNoErr;
    }
    const bool fill = kind == kRouteFill;
    ai::int32 before = s.parser.GetGroupContentsPosition(p);
    const ai::int32 fields = s.parser.CountPaintFields(p);
    if (before > fields) before = fields;
    if (before < 0) before = 0;
    AIParserPaintField field = nullptr;
    auto e = fill ? s.parser.NewPaintFieldFill(&c.fill, false, nullptr, &field) : s.parser.NewPaintFieldStroke(&c.stroke, nullptr, &field);
    if (e || !field) return e ? e : kOutOfMemoryErr;
    if ((e = s.parser.InsertNthPaintField(p, fill ? 0 : -1, field)) != kNoErr) { s.parser.DisposePaintField(field); return e; }
    // Without it the route still corrects the drawing; only the PDF's text reads twice.
    if (AddOutlinePass(s, p, field) != kNoErr) Trace("route.outline-pass", kBadParameterErr, "");
    const ai::int32 contents = fill ? before + 1 : before;
    if ((e = s.parser.MoveGroupContentsPosition(p, contents)) != kNoErr) return e;
    if (s.parser.GetGroupContentsPosition(p) != contents) return kBadParameterErr;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    size_t cleared = 0;
    if (!fill)
        for (; cleared < frames.size(); ++cleared)
            if (!SetFrameChars(s, frames[cleared], false, false)) break;
    if ((!fill && cleared != frames.size()) || (e = s.styles.SetArtStyle(target, created)) != kNoErr) {
        // The stroke was one stroke across every frame, so turning it back on restores it.
        for (size_t i = 0; i < cleared; ++i) SetFrameChars(s, frames[i], false, true);
        return e ? e : kBadParameterErr;
    }
    WriteRouteRecord(s, target, kind, fill ? FillSig(s.basic, c.fill) : StrokeSig(s.basic, c.stroke));
    return kNoErr;
}

// The frames a route target covers: the frame itself, or a story group's direct text frames.
std::vector<AIArtHandle> RouteFrames(const AIArtSuite& art, AIArtHandle target) {
    std::vector<AIArtHandle> out;
    short type = kUnknownArt;
    if (art.GetArtType(target, &type)) return out;
    if (type == kTextFrameArt) { out.push_back(target); return out; }
    AIArtHandle child = nullptr;
    for (art.GetArtFirstChild(target, &child); child; ) {
        short t = kUnknownArt;
        if (!art.GetArtType(child, &t) && t == kTextFrameArt) out.push_back(child);
        if (art.GetArtSibling(child, &child)) break;
    }
    return out;
}

// Whether a frame belongs to a story group the route styled (its paint is the group's).
bool InRoutedStory(const RouteSuites& s, AIArtHandle frame) {
    AIArtHandle parent = nullptr;
    short type = kUnknownArt;
    return !s.art.GetArtParent(frame, &parent) && parent && !s.art.GetArtType(parent, &type) && type == kGroupArt &&
           ReadRouteRecord(s.art, s.dict, parent) != kRouteNone;
}

// The re-fix of a routed frame or story group after an edit. A recolor of the characters is
// followed: the route's fill takes the new color, or gives way to the stroke route or FillFirst
// when the fills no longer agree; a stroke the user puts back on some characters of a stroke
// route gets FillFirst off. A route field the user changed or removed is theirs: the record is
// dropped and nothing else is touched. `released` counts that; `changed` any rewrite.
ASErr Reroute(const RouteSuites& s, AIArtHandle target, bool& changed, bool& released) {
    changed = released = false;
    std::string sig;
    const RouteKind kind = ReadRouteRecord(s.art, s.dict, target, &sig);
    if (kind == kRouteNone) return kNoErr;
    const auto frames = RouteFrames(s.art, target);
    short type = kUnknownArt;
    if (frames.empty() || s.art.GetArtType(target, &type)) return kNoErr;
    const bool story = type == kGroupArt;
    CharPaint c;
    auto e = ReadCharPaint(s, frames, c);
    if (e) return e;
    // Text all cut away (for one): the route stays for what is pasted or typed back.
    if (!c.any) return kNoErr;
    AIArtStyleHandle style = nullptr;
    AIStyleParser p = nullptr;
    if ((e = s.styles.GetArtStyle(target, &style)) != kNoErr || (e = s.parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if (style && (e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    if (kind == kRouteFill || kind == kRouteStroke) {
        AIParserPaintField field = FindRouteField(s, p, kind, sig);
        // Before region-prep-22 a story group kept no signature: its field is taken as it is.
        if (!field && sig.empty() && story) {
            for (ai::int32 i = 0, n = s.parser.CountPaintFields(p); i < n && !field; ++i) {
                AIParserPaintField f = nullptr;
                if (!s.parser.GetNthPaintField(p, i, &f) && f && (kind == kRouteFill ? s.parser.IsFill(f) : s.parser.IsStroke(f))) field = f;
            }
        }
        if (!field) { WriteRouteRecord(s, target, kRouteNone, ""); released = true; return kNoErr; }
        if (kind == kRouteStroke) {
            if (c.noStroke) return kNoErr;                       // as the route left it
            if (!c.oneStroke) {                                   // some characters restroked
                for (AIArtHandle f : frames) SetFrameChars(s, f, true, false);
                MarkFillFirstOff(s, target, true);
                changed = true;
                return kNoErr;
            }
            // Every character restroked alike: the user's new stroke takes the route's place.
        } else if (c.oneFill && FillSig(s.basic, c.fill) == sig) return kNoErr;
        // Contents moved back up by one when the field removed was above it, whether or not
        // RemovePaintField does that itself.
        ai::int32 index = -1;
        for (ai::int32 i = 0, n = s.parser.CountPaintFields(p); i < n && index < 0; ++i) {
            AIParserPaintField f = nullptr;
            if (!s.parser.GetNthPaintField(p, i, &f) && f == field) index = i;
        }
        const ai::int32 contents = s.parser.GetGroupContentsPosition(p);
        if ((e = s.parser.RemovePaintField(p, field, true)) != kNoErr) return e;
        if (index >= 0 && (e = s.parser.MoveGroupContentsPosition(p, index < contents ? contents - 1 : contents)) != kNoErr) return e;
    }
    RouteKind next = ChooseRoute(c);
    if (story && next == kRouteFillFirst) {
        // The story's fills and strokes no longer agree: each character's stroke before its own
        // fill, recorded on each frame so that Unroute puts FillFirst back (the review of
        // 2026-11-01), and the group's paint taken off.
        for (AIArtHandle f : frames) if (SetFrameChars(s, f, true, false)) WriteRouteRecord(s, f, kRouteFillFirst, "");
        next = kRouteNone;
    }
    if (kind == kRouteFillFirst && next == kRouteFillFirst) return kNoErr;
    // Leaving the FillFirst route (fills recolored alike, or every stroke removed): FillFirst back on
    // before the next route, or none, which would leave it off with no record (the second review
    // of 2026-11-01). Not when the route stays for a style that is the user's (below).
    const bool userStyled = s.parser.ContainsEffects(p) || s.parser.CountPaintFields(p) > 2 || s.parser.ContainsTransparency(p);
    if (kind == kRouteFillFirst && !(userStyled && (next == kRouteFill || next == kRouteStroke)))
        for (AIArtHandle f : frames) SetFrameChars(s, f, true, true);
    // A frame left on FillFirst because its style was the user's (effects, more paint, opacity)
    // stays so: a route would rebuild that style (review 2026-10-30).
    if (kind == kRouteFillFirst && (next == kRouteFill || next == kRouteStroke) &&
        (s.parser.ContainsEffects(p) || s.parser.CountPaintFields(p) > 2 || s.parser.ContainsTransparency(p)))
        return kNoErr;
    if (next == kRouteNone || next == kRouteFillFirst) {
        // The route's field is gone from the parsed style: write it back without one (no style
        // at all when nothing is left, so a group never keeps a style without its Contents).
        if (kind != kRouteFillFirst) {
            AIArtStyleHandle created = nullptr;
            if (s.parser.CountPaintFields(p) > 0 &&
                ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created)) return e ? e : kBadParameterErr;
            if ((e = s.styles.SetArtStyle(target, created)) != kNoErr) return e;
        }
        if ((e = ApplyRoute(s, target, frames, p, next, c)) != kNoErr) return e;
        changed = true;
        return kNoErr;
    }
    if ((e = ApplyRoute(s, target, frames, p, next, c)) != kNoErr) return e;
    changed = true;
    return kNoErr;
}

// A route taken off (Correct Selection with its switch off, 2026-10-30): the route's field and
// its outline pass removed, the characters' strokes back on for the stroke route, FillFirst back
// on for that route, and the record dropped. A field the user has changed since is theirs and
// stays, with the characters as they are; only the record goes. `removed`: something changed.
ASErr Unroute(const RouteSuites& s, AIArtHandle target, bool& removed) {
    removed = false;
    std::string sig;
    const RouteKind kind = ReadRouteRecord(s.art, s.dict, target, &sig);
    if (kind == kRouteNone) return kNoErr;
    const auto frames = RouteFrames(s.art, target);
    if (kind == kRouteFillFirst) {
        for (AIArtHandle f : frames) if (!SetFrameChars(s, f, true, true)) return kBadParameterErr;
        WriteRouteRecord(s, target, kRouteNone, "");
        removed = true;
        return kNoErr;
    }
    AIArtStyleHandle style = nullptr;
    AIStyleParser p = nullptr;
    ASErr e = kNoErr;
    if ((e = s.styles.GetArtStyle(target, &style)) != kNoErr || (e = s.parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if (style && (e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    AIParserPaintField field = FindRouteField(s, p, kind, sig);
    if (!field) { WriteRouteRecord(s, target, kRouteNone, ""); return kNoErr; }
    ai::int32 index = -1;
    for (ai::int32 i = 0, n = s.parser.CountPaintFields(p); i < n && index < 0; ++i) {
        AIParserPaintField f = nullptr;
        if (!s.parser.GetNthPaintField(p, i, &f) && f == field) index = i;
    }
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p);
    // A stroke lifted from the frames' own is read before its field is removed: removing it
    // disposes of it (doDelete), and it was read after that until 2026-11-01 (a use after free
    // the review found; a switch turned off could crash Illustrator).
    const bool lifted = kind == kRouteStroke && sig.find(kLiftedSig) != std::string::npos;
    AIStrokeStyle stroke{};
    AIArtStylePaintData data;
    const bool haveStroke = lifted && !s.parser.GetStroke(field, &stroke, &data);
    if (lifted && !haveStroke) return kBadParameterErr;   // not removed: the frames would get nothing back
    if ((e = s.parser.RemovePaintField(p, field, true)) != kNoErr) return e;
    field = nullptr;
    if (index >= 0 && (e = s.parser.MoveGroupContentsPosition(p, index < contents ? contents - 1 : contents)) != kNoErr) return e;
    // No style at all when nothing is left, so a group never keeps a style without its Contents.
    AIArtStyleHandle created = nullptr;
    if (s.parser.CountPaintFields(p) > 0 && ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created)) return e ? e : kBadParameterErr;
    if ((e = s.styles.SetArtStyle(target, created)) != kNoErr) return e;
    if (lifted) {
        // Lifted from the frames' own strokes: one each again, beneath each frame's Characters.
        if (haveStroke)
            for (AIArtHandle f : frames) {
                AIArtStyleHandle fs = nullptr;
                AIStyleParser fp = nullptr;
                if (s.styles.GetArtStyle(f, &fs) || s.parser.NewParser(&fp) || !fp) continue;
                struct DisposeFrame { const AIArtStyleParserSuite& api; AIStyleParser p; ~DisposeFrame() { api.DisposeParser(p); } } disposeFrame{s.parser, fp};
                if (fs && s.parser.ParseStyle(fp, fs)) continue;
                // Its stroke that paints nothing (LiftFrameStrokes left it so) takes the stroke back;
                // a frame without one gets a new field.
                AIParserPaintField back = nullptr;
                for (ai::int32 k = 0, n = s.parser.CountPaintFields(fp); k < n && !back; ++k) {
                    AIParserPaintField fk = nullptr;
                    if (!s.parser.GetNthPaintField(fp, k, &fk) && fk && s.parser.IsStroke(fk) && !PaintingStroke(s.parser, fk)) back = fk;
                }
                if (back) {
                    AIStrokeStyle had{};
                    AIArtStylePaintData hadData;
                    if (s.parser.GetStroke(back, &had, &hadData) || s.parser.SetStroke(back, &stroke, &hadData)) continue;
                } else {
                    const ai::int32 at = s.parser.GetGroupContentsPosition(fp);
                    if (s.parser.NewPaintFieldStroke(&stroke, nullptr, &back) || !back) continue;
                    if (s.parser.InsertNthPaintField(fp, -1, back)) { s.parser.DisposePaintField(back); continue; }
                    if (at >= 0) s.parser.MoveGroupContentsPosition(fp, at);
                }
                AIArtStyleHandle made = nullptr;
                if (!s.parser.CreateNewStyle(fp, &made) && made) s.styles.SetArtStyle(f, made);
            }
    } else if (kind == kRouteStroke) {
        // The characters' strokes back on, and FillFirst too when Reroute turned it off (the review
        // of 2026-11-01; only then: the second review).
        const bool fillFirstOff = FillFirstOffMarked(s, target);
        for (AIArtHandle f : frames) { SetFrameChars(s, f, false, true); if (fillFirstOff) SetFrameChars(s, f, true, true); }
        MarkFillFirstOff(s, target, false);
    }
    WriteRouteRecord(s, target, kRouteNone, "");
    removed = true;
    return kNoErr;
}

// Threaded text whose frames carry their stroke in the Appearance panel, not on their characters
// (the owner's retest S4, 2026-10-31: neither automatic correction nor Correct Selection grouped
// it, the story route reading only the characters' strokes): when every frame has one stroke that
// paints, all alike, it is lifted onto the story group below its Contents, with the route's
// Outline Object pass, and taken off the frames, whose fills stay. Recorded as the stroke route,
// its signature marked lifted (kLiftedSig). `p` is the group's parsed (empty) style.
ASErr LiftFrameStrokes(const RouteSuites& s, AIArtHandle group, const std::vector<AIArtHandle>& frames, AIStyleParser p) {
    AIStrokeStyle first{};
    bool have = false;
    for (AIArtHandle f : frames) {
        AIArtStyleHandle fs = nullptr;
        AIStyleParser fp = nullptr;
        if (s.styles.GetArtStyle(f, &fs) || !fs || s.parser.NewParser(&fp) || !fp) return kBadParameterErr;
        struct DisposeFrame { const AIArtStyleParserSuite& api; AIStyleParser p; ~DisposeFrame() { api.DisposeParser(p); } } disposeFrame{s.parser, fp};
        if (s.parser.ParseStyle(fp, fs)) return kBadParameterErr;
        int count = 0;
        AIStrokeStyle mine{};
        for (ai::int32 i = 0, n = s.parser.CountPaintFields(fp); i < n; ++i) {
            AIParserPaintField fld = nullptr;
            AIArtStylePaintData data;
            if (s.parser.GetNthPaintField(fp, i, &fld) || !fld || !PaintingStroke(s.parser, fld)) continue;
            if (s.parser.CountEffectsOfPaintField(fld) > 0 || s.parser.GetStroke(fld, &mine, &data)) return kBadParameterErr;
            ++count;
        }
        if (count != 1 || (have && !SameStroke(first, mine))) return kBadParameterErr;
        if (!have) { first = mine; have = true; }
    }
    if (!have) return kBadParameterErr;
    ai::int32 before = s.parser.GetGroupContentsPosition(p);
    const ai::int32 fields = s.parser.CountPaintFields(p);
    if (before < 0 || before > fields) before = fields;
    AIParserPaintField field = nullptr;
    auto e = s.parser.NewPaintFieldStroke(&first, nullptr, &field);
    if (e || !field) return e ? e : kOutOfMemoryErr;
    if ((e = s.parser.InsertNthPaintField(p, -1, field)) != kNoErr) { s.parser.DisposePaintField(field); return e; }
    if (AddOutlinePass(s, p, field) != kNoErr) Trace("lift.outline-pass", kBadParameterErr, "");
    if ((e = s.parser.MoveGroupContentsPosition(p, before)) != kNoErr) return e;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    if ((e = s.styles.SetArtStyle(group, created)) != kNoErr) return e;
    // Off the frames: each frame's stroke that paints set to paint nothing. A type object keeps
    // the fill and stroke it was made with; removing that stroke left it in place (the first host
    // test, 2026-10-31).
    for (AIArtHandle f : frames) {
        AIArtStyleHandle fs = nullptr;
        AIStyleParser fp = nullptr;
        if (s.styles.GetArtStyle(f, &fs) || !fs || s.parser.NewParser(&fp) || !fp) continue;
        struct DisposeFrame { const AIArtStyleParserSuite& api; AIStyleParser p; ~DisposeFrame() { api.DisposeParser(p); } } disposeFrame{s.parser, fp};
        if (s.parser.ParseStyle(fp, fs)) continue;
        bool changed = false;
        for (ai::int32 i = 0, n = s.parser.CountPaintFields(fp); i < n; ++i) {
            AIParserPaintField fld = nullptr;
            AIStrokeStyle stroke{};
            AIArtStylePaintData data;
            if (s.parser.GetNthPaintField(fp, i, &fld) || !fld || !PaintingStroke(s.parser, fld) || s.parser.GetStroke(fld, &stroke, &data)) continue;
            stroke.color.kind = kNoneColor;
            if (!s.parser.SetStroke(fld, &stroke, &data)) changed = true;
        }
        AIArtStyleHandle made = nullptr;
        if (changed && !s.parser.CreateNewStyle(fp, &made) && made) s.styles.SetArtStyle(f, made);
    }
    WriteRouteRecord(s, group, kRouteStroke, StrokeSig(s.basic, first) + kLiftedSig);
    return kNoErr;
}

// The story route (research, region-prep-21/22): the route above on an explicitly selected group
// that holds one complete linked story, so every frame's strokes paint beneath every frame's
// fills, across frames. The group must hold only that story's text frames, with no paint,
// effect, or transparency of its own, and its frames no object-level paint or effects of their
// own; grouping and ordering are the caller's (strokemaster-fix-story.jsx). Fills and strokes
// both mixed are refused ("story.mixed"): native-route's per-frame FillFirst is the answer there.
// `separate` (input "separate"): the opt-in of InDesign's "Across separate stories" switch: the
// group may hold several stories, each complete, so one story's strokes also paint beneath
// another's fills where their frames overlap (a deliberate halo over another frame is lost,
// which is why it is never the default).
// `part` (input "part"): the group holds an adjacent run of one story's frames, not all of them,
// because visible artwork lies between the runs (strokemaster-fix-story.jsx splits the story
// there). Each run is corrected within itself; across the artwork the host's stacking stays,
// as InDesign's plugin keeps its order where such content covers a letter.
ASErr StoryFill(SPMessageData& message, State& state, bool separate = false, bool part = false, AIArtHandle target = nullptr) {
    auto& st = state.appearance;
    st = AppearanceStats{};
    st.fieldsBefore = st.fieldsAfter = st.contentsBefore = st.contentsAfter = 0;
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> ranges(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    Suite<AIATEPaintSuite> paint(message.basic, kAIATEPaintSuite, kAIATEPaintVersion);
    Suite<AIBlendStyleSuite> blend(message.basic, kAIBlendStyleSuite, kAIBlendStyleVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    for (ASErr e : {art.error,matching.error,memory.error,styles.error,parser.error,text.error,
                    ranges.error,features.error,paint.error,blend.error,dict.error})
        if (e) { st.gate = "story.suites"; return e; }
    const RouteSuites s{message.basic, *art.operator&(), *styles.operator&(), *parser.operator&(), *text.operator&(),
                        *ranges.operator&(), *features.operator&(), *paint.operator&(), *dict.operator&()};
    struct Handles {
        const AIMdMemorySuite& api; AIArtHandle** p = nullptr;
        ~Handles() { if (p) api.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(p)); }
    } selected{*memory.operator&()}, all{*memory.operator&()};
    // The one selected group, or the group Correct Selection names (`target`), which does not
    // depend on the selection's state between its steps.
    ai::int32 n = 0;
    ASErr e = kNoErr;
    AIArtHandle group = target;
    if (!group) {
        AIMatchingArtSpec groupSpec(kGroupArt,kArtSelected,kArtSelected);
        e = matching->GetMatchingArt(&groupSpec,1,&selected.p,&n);
        if (e || n != 1) { st.gate = "story.one-group"; return e ? e : kBadParameterErr; }
        group = (*selected.p)[0];
    }
    if (blend->GetOpacity(group) != 1 || blend->GetBlendingMode(group) != kAINormalBlendingMode) {
        st.gate = "story.transparency"; return kBadParameterErr;
    }
    AIArtStyleHandle style = nullptr;
    AIStyleParser p = nullptr;
    if ((e = styles->GetArtStyle(group,&style)) || (e = parser->NewParser(&p)) || !p) {
        st.gate = "story.parser"; return e ? e : kBadParameterErr;
    }
    struct ParserRelease { const AIArtStyleParserSuite& api; AIStyleParser p; ~ParserRelease(){api.DisposeParser(p);} }
        releaseParser{*parser.operator&(),p};
    if (style && (e = parser->ParseStyle(p,style))) { st.gate = "story.parse"; return e; }
    if (parser->ContainsPaint(p) || parser->ContainsEffects(p) || parser->ContainsTransparency(p)) {
        st.gate = "story.existing-style"; return kBadParameterErr;
    }
    AIArtHandle child = nullptr;
    if ((e = art->GetArtFirstChild(group,&child))) return e;
    ai::int32 story = -1;
    std::set<ai::int32> stories;
    std::vector<AIArtHandle> frames;
    for (; child; ) {
        short type = kUnknownArt;
        ai::int32 index = -1;
        if (art->GetArtType(child,&type) || type != kTextFrameArt ||
            text->GetStoryIndex(child,&index) || index < 0 || (!separate && story >= 0 && index != story)) {
            st.gate = "story.children"; return kBadParameterErr;
        }
        story = index;
        stories.insert(index);
        if (++st.frames > 64 || blend->GetOpacity(child) != 1 ||
            blend->GetBlendingMode(child) != kAINormalBlendingMode) {
            st.gate = "story.frame-limit-or-transparency"; return kBadParameterErr;
        }
        // A frame with object-level paint or effects of its own is the user's construction.
        AIArtStyleHandle fs = nullptr;
        AIStyleParser fp = nullptr;
        if (styles->GetArtStyle(child,&fs) || parser->NewParser(&fp) || !fp) { st.gate = "story.parser"; return kBadParameterErr; }
        ParserRelease releaseFrame{*parser.operator&(),fp};
        if (fs && parser->ParseStyle(fp,fs)) { st.gate = "story.parse"; return kBadParameterErr; }
        if (parser->ContainsEffects(fp) || parser->CountPaintFields(fp) > 2 || parser->ContainsTransparency(fp)) {
            st.gate = "story.frame-style"; return kBadParameterErr;
        }
        frames.push_back(child);
        if ((e = art->GetArtSibling(child,&child))) return e;
    }
    if (st.frames < 2) { st.gate = "story.too-few-frames"; return kBadParameterErr; }
    if (separate && stories.size() < 2) { st.gate = "story.one-story"; return kBadParameterErr; }
    AIMatchingArtSpec frameSpec(kTextFrameArt,0,0);
    if ((e = matching->GetMatchingArt(&frameSpec,1,&all.p,&n))) return e;
    int total = 0;
    for (ai::int32 i = 0; i < n; ++i) {
        ai::int32 index = -1;
        if (!text->GetStoryIndex((*all.p)[i],&index) && stories.count(index)) ++total;
    }
    if (!part && total != st.frames) { st.gate = "story.incomplete"; return kBadParameterErr; }
    CharPaint c;
    if (ReadCharPaint(s, frames, c)) { st.gate = "story.paint"; return kBadParameterErr; }
    const RouteKind kind = ChooseRoute(c);
    if (kind == kRouteNone) {
        // The frames' own strokes, lifted onto the group (LiftFrameStrokes), by the opt-in.
        if (!Switches().appearanceStrokes) { st.gate = "story.appearance-off"; return kBadParameterErr; }
        if (LiftFrameStrokes(s, group, frames, p)) { st.gate = "story.no-stroke"; return kBadParameterErr; }
        for (AIArtHandle f : frames) WriteRouteRecord(s, f, kRouteNone, "");
        st.styled = 1;
        st.fieldsAfter = 1;
        st.gate = "story.lifted";
        return kNoErr;
    }
    if (kind == kRouteFillFirst) { st.gate = "story.mixed"; return kBadParameterErr; }
    if (!GroupRouteSupportsFields(kind, Switches().closeHoles, Switches().textMiter)) {
        st.gate = "story.mixed-stroke-fields";
        return kBadParameterErr;
    }
    st.contentsBefore = parser->GetGroupContentsPosition(p);
    if ((e = ApplyRoute(s, group, frames, p, kind, c)) != kNoErr) { st.gate = "story.set-style"; return e; }
    // The frames' own route records (a FillFirst from native-route, for one) give way to the
    // group's, so the re-fix never routes a frame on top of its group.
    for (AIArtHandle f : frames) WriteRouteRecord(s, f, kRouteNone, "");
    st.contentsAfter = parser->GetGroupContentsPosition(p);
    st.styled = 1;
    if (kind == kRouteFill) { st.fieldsBefore = 1; st.gate = "story.filled"; }
    else { st.fieldsAfter = 1; st.gate = "story.stroked"; }
    return kNoErr;
}

// native-route: the route on each selected text frame (every frame when none is selected). A
// frame whose art style already carries effects or more paint fields than Characters alone is
// not rebuilt, so nothing the user built is changed; it gets FillFirst off unless a fill above
// Characters already covers every stroke. A frame in a routed story group is left to its group.
// Counts go to the appearance stats: styled = frames given an object-level paint; fieldsBefore =
// fills above, fieldsAfter = strokes below; fillFirst = frames given FillFirst off.
ASErr NativeRoute(SPMessageData& message, State& state, const std::vector<AIArtHandle>* only = nullptr) {
    auto& stats = state.appearance;
    stats = AppearanceStats{};
    stats.fieldsBefore = stats.fieldsAfter = stats.contentsBefore = stats.contentsAfter = 0;
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    Suite<AIATEPaintSuite> atePaint(message.basic, kAIATEPaintSuite, kAIATEPaintVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    for (ASErr e : {art.error, matching.error, memory.error, styles.error, parser.error, text.error, range.error, features.error,
                    atePaint.error, dict.error})
        if (e) { stats.gate = "suites"; return e; }
    const RouteSuites s{message.basic, *art.operator&(), *styles.operator&(), *parser.operator&(), *text.operator&(),
                        *range.operator&(), *features.operator&(), *atePaint.operator&(), *dict.operator&()};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    auto err = SelectedOrAllFrames(*matching.operator&(), &matches, &count);
    if (err) { stats.gate = "match"; return err; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    // An explicit list (Correct Selection, automatic correction) is taken whatever is selected:
    // with a path alone selected, the selection's frame count was 0 and the list was dropped.
    if (only) count = static_cast<ai::int32>(only->size());
    stats.frames = count;
    if (count <= 0) { stats.gate = "no-text-frame"; return kNoErr; }
    if (count > 4096) { stats.gate = "too-many-frames"; return kBadParameterErr; }
    int none = 0, mixed = 0, refused = 0;
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle frame = only ? (*only)[static_cast<size_t>(i)] : (*matches)[i];
        if (InRoutedStory(s, frame)) continue;
        // The object's own strokes (the Appearance panel's) below its fills and Characters, by the
        // opt-in.
        if (Switches().appearanceStrokes) {
            bool moved = false;
            const ASErr e = StrokesBelowFills(s.art, s.dict, s.styles, s.parser, frame, moved);
            if (e) Trace("route.object-strokes", e, "");
            else if (moved) ++stats.objectStrokes;
        }
        // Already routed: followed as the re-fix would (a recolor since, for one).
        if (ReadRouteRecord(s.art, s.dict, frame) != kRouteNone) {
            bool changed = false, released = false;
            if ((err = Reroute(s, frame, changed, released)) != kNoErr) { stats.gate = "reroute"; return err; }
            continue;
        }
        CharPaint c;
        const std::vector<AIArtHandle> one{frame};
        if (ReadCharPaint(s, one, c)) { stats.gate = "paint"; return kBadParameterErr; }
        const RouteKind kind = ChooseRoute(c);
        if (kind == kRouteNone) { ++none; continue; }
        if (kind == kRouteFillFirst) {
            ++mixed;
            if (ApplyRoute(s, frame, one, nullptr, kind, c)) { stats.gate = "fill-first"; return kBadParameterErr; }
            ++stats.fillFirst;
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
            if (!fillAbove) {
                if (!SetFrameChars(s, frame, true, false)) { stats.gate = "fill-first"; return kBadParameterErr; }
                // Recorded, so that Unroute turns FillFirst back on (the review of 2026-11-01).
                WriteRouteRecord(s, frame, kRouteFillFirst, "");
                ++stats.fillFirst;
            }
            continue;
        }
        if ((err = ApplyRoute(s, frame, one, p, kind, c)) != kNoErr) { stats.gate = "set-style"; return err; }
        if (kind == kRouteStroke) ++stats.fieldsAfter; else ++stats.fieldsBefore;
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
ASErr AlignDashes(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, bool& changed, bool& refused, bool want = true) {
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
        const bool already = !got && current && !!bs->GetDashAdjustment(current, nullptr) == want;
        if (current) dict->Release(current);
        if (got) return kNoErr;
        if (already) return kNoErr;
        if (!accept && want) { refused = true; return kNoErr; }
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
        if (!e && inner) e = bs->SetDashAdjustment(inner, want);
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
    if (on != want || !(after.width > 0) || after.dash.length <= 0) { Trace("fix.dash-readback", kBadParameterErr, ""); refused = true; return kNoErr; }
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

// Whether a parsed style's effect is the one named. By name, not by handle: in a document saved
// as PDF and then edited (ai-native-align, 2026-10-16), a parsed Offset Path effect reported a
// handle other than GetLiveEffectHandleByName's, which made restore refuse.
bool EffectNamed(const AIArtStyleParserSuite& parser, AIParserLiveEffect fx, const char* want) {
    const char* name = nullptr;
    ai::int32 major = 0, minor = 0;
    return fx && !parser.GetLiveEffectNameAndVersion(fx, &name, &major, &minor) && name && std::strcmp(name, want) == 0;
}

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
            ai::int32 command = -1, tag = 0;
            AILiveEffectParameters fxParams = nullptr;
            if (s.parser.GetNthPreEffect(p, i, &fx) || !EffectNamed(s.parser, fx, "Adobe Pathfinder") ||
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
// Set on what the opt-in for dashed Outside strokes made ("dashes"), so Correct Selection can
// take it off again when that switch is off.
constexpr char kAlignDashes[] = "StrokeMasterAlignDashes";

struct AlignRecord { std::string as; int side = 0; double weight = 0; bool dashes = false; };

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
    AIBoolean byDashes = false;
    if (!dict->GetBooleanEntry(d, dict->Key(kAlignDashes), &byDashes)) r.dashes = byDashes;
    dict->Release(d);
    return r;
}

bool WriteAlignRecord(SPBasicSuite* basic, const AIArtSuite& art, AIArtHandle item, const AlignRecord& r) {
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (dict.error) return false;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return false;
    if (r.as.empty()) {
        for (const char* k : {kAlignAs, kAlignSide, kAlignWeight, kAlignDashes}) dict->DeleteEntry(d, dict->Key(k));
    } else {
        dict->SetStringEntry(d, dict->Key(kAlignAs), r.as.c_str());
        dict->SetIntegerEntry(d, dict->Key(kAlignSide), r.side);
        dict->SetRealEntry(d, dict->Key(kAlignWeight), static_cast<AIReal>(r.weight));
        if (r.dashes) dict->SetBooleanEntry(d, dict->Key(kAlignDashes), true);
        else dict->DeleteEntry(d, dict->Key(kAlignDashes));
    }
    dict->Release(d);
    return ReadAlignRecord(basic, art, item).as == r.as;
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
    bool dashesByFix = false;   // ... and the fix turned the option on (not the user)
    bool reordered = false;     // the fix moved the fill above an Outside stroke (overlapping contours)
    std::set<long> charLimits;  // every limit a frame's text runs carry after the fix, in thousandths
    // The user's own limit, the one the rulings are judged against (within it a glyph tip is
    // trimmed, past it only a whole point is kept): the limit before the fix first changed it.
    // Without it, a later fix would take the limit it wrote for the user's and judge again.
    double userLimit = 0;       // a path's (0: unknown)
    double charUserLimit = 0;   // a frame's, when one limit ran through it (0: each character's own)
    // Strokes of the Appearance panel's (a text object's own, the route's, a path's besides its
    // own stroke): every limit the fix left on a stroke that paints, in thousandths, and the
    // user's own limit before the fix first changed one. A limit set there by hand is the user's
    // (the owner's retest Q4, 2026-10-31: the route stroke's limit was set back at once).
    std::set<long> fieldLimits;
    double fieldUser = 0;
    std::string order;          // the field order the fix left (FieldOrder)
    bool operator==(const FixRecord& o) const {
        return hasLimit == o.hasLimit && (!hasLimit || std::fabs(limit - o.limit) <= 1e-4) && pathfinder == o.pathfinder &&
               dashes == o.dashes && dashesByFix == o.dashesByFix && reordered == o.reordered && charLimits == o.charLimits && std::fabs(userLimit - o.userLimit) <= 1e-4 &&
               std::fabs(charUserLimit - o.charUserLimit) <= 1e-4 && fieldLimits == o.fieldLimits &&
               std::fabs(fieldUser - o.fieldUser) <= 1e-4 && order == o.order;
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
// Correct Selection's pass: a switch that is off also takes its correction off the art it
// corrects, as InDesign draws nothing for a switch that is off (the owner's hand test 4,
// 2026-10-30). The re-fix of an edit never does; a switch changed does it through Reapply.
bool gReconcile = false;
// Correction the user did not ask for (AutoCorrect, Reapply): art the user took back stays theirs,
// and art changed by hand where StrokeMaster did not see it (another machine, or while Correct
// automatically was off) is released rather than corrected again, as the re-fix does. Only Correct
// Selection pressed by the user takes such art back (the review of 2026-11-01).
bool gAutomatic = false;
struct Automatic { bool was = gAutomatic; Automatic() { gAutomatic = true; } ~Automatic() { gAutomatic = was; } };
// Correct Selection without Across other text objects' grouping of the stories selected together:
// Reapply's pass over a whole document, which would ask to group every story in it at once.
bool gNoSeparate = false;

// Illustrator's own default miter limit. Art corrected for the first time is judged against at
// least it: a lower limit there was inherited from art StrokeMaster had corrected (new text takes
// the last character attributes, new art the last appearance), not set by the user, and it kept
// Trim glyph tips from making any difference (the owner's retest S6, 2026-10-31).
constexpr double kDefaultLimit = 4;
inline double FirstRuling(double limit) { return limit < kDefaultLimit ? kDefaultLimit : limit; }

// Defined with the text fixes below.
ASErr FixFieldStrokes(SPBasicSuite* basic, const FixSuites& s, AIArtHandle item, const std::vector<ise_miter::Contour>& contours,
                      double scale, bool text, bool charsFill, const AIStrokeStyle* own, NativeFixStats& st,
                      const FixRecord* prior, FixRecord& rec, double user = 0);

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
    // Strokes added in the Appearance panel besides the path's own: their limits too (the owner's
    // retest Q1, 2026-10-31: a triangle's second stroke kept 4, its point cut).
    auto fields = [&]() -> ASErr {
        std::vector<ise_miter::Contour> cs;
        if (!basic || !Contours(s, art, cs) || cs.empty()) return kNoErr;
        AIPathStyle now{};
        AIBoolean adv = false;
        const bool own = !s.paint.GetPathStyle(art, &now, &adv) && now.strokePaint && now.stroke.width > 0;
        return FixFieldStrokes(basic, s, art, cs, 1, false, false, own ? &now.stroke : nullptr, st, prior, rec);
    };
    if (!style.strokePaint || !(style.stroke.width > 0)) return fields();
    // Inside drawn as two fills (native-align): its stroke is hidden, and Offset Path carries the
    // join and limit; nothing here applies.
    if (basic && ReadAlignRecord(basic, s.art, art).as == "offset") return kNoErr;
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
    // Inside on overlapping contours (any join): united first, by Illustrator's Pathfinder. So
    // are dashed Center and Outside strokes there (2026-10-30), as InDesign's plugin lays a
    // pattern along one outline of the filled shape; a solid Center stroke keeps its inner
    // contour edges (the owner's ruling).
    const bool dashed = style.stroke.dash.length > 0;
    const bool unite = Switches().overlaps && !style.evenodd && style.fillPaint &&
                       (align == kAIStrokeAlignmentInside ||
                        (dashed && (align == kAIStrokeAlignmentCenter || align == kAIStrokeAlignmentOutside))) &&
                       native_region::InternalEdges(contours, 0.01, style.stroke.width / 2);
    if (style.stroke.join == kAIMiterJoin && !skipLimit) {
        // The miter's reach: half the width for Center, the whole width for Outside and Inside.
        const double h = style.stroke.width * reachScale;
        // Judged against the user's own limit while the limit is still the one the fix wrote.
        double ruling = style.stroke.miterLimit;
        if (prior && prior->hasLimit && prior->userLimit > 0 && std::fabs(style.stroke.miterLimit - prior->limit) <= 1e-3)
            ruling = prior->userLimit;
        else if (!prior || !prior->hasLimit)
            ruling = FirstRuling(ruling);
        rec.userLimit = ruling;
        // Center: interior miters over the fill are visible. Inside: clipped to the fill, so none
        // escapes. Outside: no interior miter on its side. United: the corners actually drawn.
        const auto choice = native_miter::ChooseLimitForOutline(unite ? UnitedOutline(contours) : contours, h,
                                                                ruling, Switches().shapeMiter && drawn != kAIStrokeAlignmentOutside,
                                                                underFill, align == kAIStrokeAlignmentInside,
                                                                Switches().wholePoints, Switches().glyphTips);
        Tally(st, choice);
        std::snprintf(gPathJudged, sizeof gPathJudged, "own w%.2f lim%.3f ruling%.3f h%.2f skip%d choice%.3f kept%d cut%d", static_cast<double>(style.stroke.width),
                      static_cast<double>(style.stroke.miterLimit), ruling, h, skipLimit ? 1 : 0, choice.limit, choice.kept, choice.cut);
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
    if (Switches().wholePoints && Switches().patternPoints && basic && style.stroke.dash.length > 0) {
        bool aligned = false, refusedDash = false;
        e = AlignDashes(basic, s, art, aligned, refusedDash);
        if (e) return e;
        if (aligned) { ++st.dashes; rec.dashesByFix = true; }
        if (refusedDash) ++st.dashRefused;
        rec.dashes = DashesOn(basic, art);
    } else if (gReconcile && basic && rec.dashesByFix && style.stroke.dash.length > 0) {
        // The switch off: the option the fix turned on goes off again (one the user set stays).
        bool cleared = false, refusedDash = false;
        e = AlignDashes(basic, s, art, cleared, refusedDash, false);
        if (e) return e;
        rec.dashes = DashesOn(basic, art);
        rec.dashesByFix = rec.dashes;
    }
    if (Switches().overlaps && align == kAIStrokeAlignmentOutside && contours.size() >= 2) {
        bool changed = false;
        e = FillAboveStroke(s, art, changed);
        if (e) return e;
        if (changed) { ++st.reordered; rec.reordered = true; }
    } else if (gReconcile && rec.reordered && !Switches().overlaps) {
        if ((e = StrokeAboveFill(s, art)) != kNoErr) return e;
        rec.reordered = false;
    }
    return fields();
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
// characters paint above, so interior miters inside the fill are hidden. `user`: the user's own
// limit, recorded at the first fix (0: the stroke's limit as it is). Judged against it, not the
// limit a previous fix wrote, so a correction can raise the limit again (Trim glyph tips turned
// off: the owner's retest R17, 2026-10-30).
// Whether a stroke field is the route's own (its Outline Object pass carries the route's tag).
bool RouteField(const AIArtStyleParserSuite& parser, const AIDictionarySuite* dict, AIParserPaintField field) {
    if (!dict) return false;
    for (ai::int32 k = 0, n = parser.CountEffectsOfPaintField(field); k < n; ++k) {
        AIParserLiveEffect fx = nullptr;
        AILiveEffectParameters params = nullptr;
        ai::int32 tag = 0;
        if (!parser.GetNthEffectOfPaintField(field, k, &fx) && fx && !parser.GetLiveEffectParams(fx, &params) && params &&
            !dict->GetIntegerEntry(params, dict->Key(kRoutePassKey), &tag) && tag == 1)
            return true;
    }
    return false;
}

// The Appearance panel's strokes on an item (a text object's own and the route's, or a path's
// besides its own stroke, `own`): one miter limit each, from the outline `contours` at the
// stroke's half width. A stroke under a painting fill (or, on text, under Characters that fill)
// has its interior miters hidden. Each is judged against the user's own limit: the route's
// against `user` (the characters', 0: its own), any other against the one recorded at the first
// fix (`prior`) while it still carries a limit the fix wrote. `rec` returns every limit left on a
// stroke that paints, and the user's. Live text keeps no whole point past the limit (`wholePoints`
// false), as InDesign (MustKeep).
ASErr FixFieldStrokes(SPBasicSuite* basic, const FixSuites& s, AIArtHandle item, const std::vector<ise_miter::Contour>& contours,
                      double scale, bool text, bool charsFill, const AIStrokeStyle* own, NativeFixStats& st,
                      const FixRecord* prior, FixRecord& rec, double user) {
    rec.fieldLimits.clear();
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(item, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    e = s.parser.ParseStyle(p, style);
    if (e) return e;
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    const AIDictionarySuite* d = dict.error ? nullptr : dict.operator&();
    // The path's own stroke (its path style's), corrected by FixPath with its alignment: the first
    // stroke field of its width and limit. Illustrator's focus stroke is not it: the parser's and
    // the path style's differed on the owner's triangle with a second stroke (2026-10-31).
    bool ownSeen = own == nullptr;
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p);
    const ai::int32 fields = s.parser.CountPaintFields(p);
    bool changed = false, fillAbove = false;
    // Positions run top to bottom, and `contents` counts the fields above Characters.
    for (ai::int32 i = 0; i < fields; ++i) {
        if (text && i == contents && charsFill) fillAbove = true;
        AIParserPaintField field = nullptr;
        if (s.parser.GetNthPaintField(p, i, &field) || !field) continue;
        if (PaintingFill(s.parser, field)) { fillAbove = true; continue; }
        if (!PaintingStroke(s.parser, field)) continue;   // a hidden copy (native-align) among them
        AIStrokeStyle stroke{};
        AIArtStylePaintData data;
        if (s.parser.GetStroke(field, &stroke, &data)) continue;
        const bool isOwn = !ownSeen && std::fabs(stroke.width - own->width) <= 1e-4 &&
                           std::fabs(stroke.miterLimit - own->miterLimit) <= 1e-4;
        if (isOwn) ownSeen = true;
        if (isOwn || stroke.join != kAIMiterJoin) {
            rec.fieldLimits.insert(std::lround(static_cast<double>(stroke.miterLimit) * 1000));
            continue;
        }
        const bool route = text && RouteField(s.parser, d, field);
        const long current = std::lround(static_cast<double>(stroke.miterLimit) * 1000);
        double ruling = stroke.miterLimit;
        if (route && user > 0) ruling = user;
        else if (!route && prior && prior->fieldUser > 0 && prior->fieldLimits.count(current)) ruling = prior->fieldUser;
        else if (!route && !(prior && prior->fieldUser > 0)) ruling = FirstRuling(ruling);
        if (!route && !(rec.fieldUser > 0)) rec.fieldUser = ruling;
        const double h = stroke.width / 2 * scale;
        const auto choice = text ? native_miter::ChooseLimitForOutline(contours, h, ruling, true, fillAbove, false, false,
                                                                       Switches().glyphTips)
                                 : native_miter::ChooseLimitForOutline(contours, h, ruling, Switches().shapeMiter, false, false,
                                                                       Switches().wholePoints, Switches().glyphTips);
        Tally(st, choice);
        std::snprintf(gPathJudged, sizeof gPathJudged, "field w%.2f lim%.3f ruling%.3f h%.2f choice%.3f kept%d cut%d", static_cast<double>(stroke.width),
                      static_cast<double>(stroke.miterLimit), ruling, h, choice.limit, choice.kept, choice.cut);
        if (std::fabs(choice.limit - stroke.miterLimit) > 1e-6) {
            stroke.miterLimit = static_cast<AIReal>(choice.limit);
            if (!s.parser.SetStroke(field, &stroke, &data)) {
                rec.fieldLimits.insert(std::lround(choice.limit * 1000));
                changed = true;
                ++st.fieldLimits;
                continue;
            }
        }
        rec.fieldLimits.insert(current);
    }
    if (!changed) return kNoErr;
    AIArtStyleHandle created = nullptr;
    e = s.parser.CreateNewStyle(p, &created);
    if (e || !created) return e ? e : kBadParameterErr;
    e = s.styles.SetArtStyle(item, created);
    if (!e) ++st.pathLimits;
    return e;
}

// A text frame's strokes of the Appearance panel (its own and the route's): see FixFieldStrokes.
ASErr FixObjectStrokes(SPBasicSuite* basic, const FixSuites& s, AIArtHandle frame, const std::vector<ise_miter::Contour>& glyphs,
                       double scale, bool charsFill, NativeFixStats& st, const FixRecord* prior, FixRecord& rec, double user = 0) {
    return FixFieldStrokes(basic, s, frame, glyphs, scale, true, charsFill, nullptr, st, prior, rec, user);
}

// CreateOutline can split a live threaded story even though it keeps the source frame.
// Outline an owned deep copy so the caller's story and frame ranges stay intact.
ASErr CopyOutline(const FixSuites& s, AIArtHandle frame, AIArtHandle* outline) {
    *outline = nullptr;
    AIArtHandle copy = nullptr;
    auto e = s.art.DuplicateArt(frame, kPlaceAbove, frame, &copy);
    struct DisposeCopy { const AIArtSuite& api; AIArtHandle a; ~DisposeCopy() { if (a) api.DisposeArt(a); } } dispose{s.art, copy};
    if (e || !copy) return e ? e : kBadParameterErr;
    // Measure the characters, not extra object-level paint or effects. Outlining a
    // styled linked copy can also invalidate its source story's saved native data.
    e = s.styles.SetArtStyle(copy, nullptr);
    if (e) return e;
    return s.text.CreateOutline(copy, outline);
}

// One limit per character, from its glyph outline. Glyph outlines come from CreateOutline of
// the frame (first child = last glyph); characters that draw no glyph are skipped. When the
// counts disagree (ligatures, for one), each character is judged against the whole frame.
ASErr FixCharacters(SPBasicSuite* basic, const FixSuites& s, AIArtHandle frame, NativeFixStats& st, FixRecord& rec,
                    const FixRecord* prior = nullptr, double* userLimitOut = nullptr) {
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
    e = CopyOutline(s, frame, &outline);
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
        if (!s.range.GetUniqueCharFeatures(whole, &u) && u && !s.features.GetMiterLimit(u, &assigned, &l) && assigned) frameUser = FirstRuling(l);
        if (u) s.features.Release(u);
    }
    if (userLimitOut) *userLimitOut = frameUser;
    // The route stroke's: the frame's, or else the one limit its characters carry. The stroke route
    // leaves the characters unstroked, so the fix never lowers theirs, and a frame corrected before
    // its record kept the frame's limit (before region-prep-30) still gets back to it.
    double routeUser = frameUser;
    if (!(routeUser > 0)) {
        ATE::CharFeaturesRef u = nullptr;
        ATE::ATEBool8 assigned = false, stroked = false, on = false;
        ATETextDOM::Float l = 0;
        if (!s.range.GetUniqueCharFeatures(whole, &u) && u && !s.features.GetMiterLimit(u, &assigned, &l) && assigned &&
            !(!s.features.GetStroke(u, &stroked, &on) && stroked && on))
            routeUser = prior ? l : FirstRuling(l);
        if (u) s.features.Release(u);
    }
    // Whether the characters paint a fill (assumed when they differ).
    bool charsFill = true;
    {
        ATE::CharFeaturesRef u = nullptr;
        ATE::ATEBool8 assigned = false, on = true;
        if (!s.range.GetUniqueCharFeatures(whole, &u) && u && !s.features.GetFill(u, &assigned, &on) && assigned) charsFill = on;
        if (u) s.features.Release(u);
    }
    e = FixObjectStrokes(basic, s, frame, allGlyphs, scale, charsFill, st, prior, rec, routeUser);
    if (e) return e;
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
        else if (!prior)
            limit = static_cast<ATETextDOM::Float>(FirstRuling(limit));
        if (on && width > 0 && join == ATE::kMiterJoin) {
            const double h = width / 2 * scale;
            if (mapped) {
                // Its own fill paints over its stroke when FillFirst is off: an interior miter
                // that stays inside the glyph is hidden.
                const auto choice = native_miter::ChooseLimitForOutline(c.contours, h, limit, true, hidden || !fillFirst, false, false,
                                                                        Switches().glyphTips);
                Tally(st, choice);
                chosen = choice.limit;
            } else {
                const auto key = std::make_pair(static_cast<double>(width), static_cast<double>(limit));
                for (const auto& fl : frameLimits) if (fl.first == key) chosen = fl.second;
                if (chosen < 0) {
                    // Against the whole frame, a miter inside a neighbour's fill is not hidden by
                    // FillFirst (only a glyph's own fill paints over its stroke): frame fill only.
                    const auto choice = native_miter::ChooseLimitForOutline(allGlyphs, h, limit, true, hidden, false, false, Switches().glyphTips);
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

// A story group's stroke below its Contents (the story route): one miter limit from every glyph
// of every frame, as FixObjectStrokes gives a frame's own object stroke.
ASErr FixStoryStroke(SPBasicSuite* basic, const FixSuites& s, AIArtHandle group, NativeFixStats& st, double user = 0) {
    std::vector<ise_miter::Contour> all;
    double scale = 0;
    AIArtHandle child = nullptr;
    for (s.art.GetArtFirstChild(group, &child); child; ) {
        short type = kUnknownArt;
        if (!s.art.GetArtType(child, &type) && type == kTextFrameArt) {
            if (scale == 0) scale = FrameScale(s, child);
            AIArtHandle outline = nullptr;
            const auto e = CopyOutline(s, child, &outline);
            struct DisposeArt { const AIArtSuite& api; AIArtHandle a; ~DisposeArt() { if (a) api.DisposeArt(a); } } dispose{s.art, outline};
            if (!e && outline) {
                AIArtHandle glyph = nullptr;
                for (s.art.GetArtFirstChild(outline, &glyph); glyph; ) {
                    std::vector<ise_miter::Contour> cs;
                    if (Contours(s, glyph, cs)) for (auto& c : cs) all.push_back(std::move(c));
                    if (s.art.GetArtSibling(glyph, &glyph)) break;
                }
            }
        }
        if (s.art.GetArtSibling(child, &child)) break;
    }
    if (all.empty()) return kNoErr;
    FixRecord unused;
    return FixObjectStrokes(basic, s, group, all, scale > 0 ? scale : 1, true, st, nullptr, unused, user);
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
constexpr char kRecordDashesByFix[] = "StrokeMasterDashesByFix";
constexpr char kRecordReordered[] = "StrokeMasterReordered";
constexpr char kRecordFieldLimits[] = "StrokeMasterFieldLimits";
constexpr char kRecordFieldUser[] = "StrokeMasterFieldUser";
constexpr char kRecordOrder[] = "StrokeMasterOrder";
// Art whose correction the user changed by hand (a limit, the fix's dash alignment or Pathfinder
// Add taken off, the route's field changed): automatic correction leaves it alone from then on,
// and Correct Selection takes it back (Mark clears this).
constexpr char kReleasedMark[] = "StrokeMasterReleased";
// A story group's route stroke limits as the fix left them, to tell a limit set by hand on the
// group (the second review of 2026-11-01: it was reset at the next tick).
constexpr char kGroupLimits[] = "StrokeMasterGroupLimits";

void Mark(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle item, const FixRecord& rec) {
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return;
    dict.SetIntegerEntry(d, dict.Key(kNativeMark), 1);
    dict.DeleteEntry(d, dict.Key(kReleasedMark));
    if (rec.hasLimit) dict.SetRealEntry(d, dict.Key(kRecordLimit), static_cast<AIReal>(rec.limit));
    else dict.DeleteEntry(d, dict.Key(kRecordLimit));
    dict.SetBooleanEntry(d, dict.Key(kRecordPathfinder), rec.pathfinder);
    dict.SetBooleanEntry(d, dict.Key(kRecordDashes), rec.dashes);
    dict.SetBooleanEntry(d, dict.Key(kRecordDashesByFix), rec.dashesByFix);
    dict.SetBooleanEntry(d, dict.Key(kRecordReordered), rec.reordered);
    std::string limits;
    for (long v : rec.charLimits) { if (!limits.empty()) limits += ","; limits += std::to_string(v); }
    dict.SetStringEntry(d, dict.Key(kRecordCharLimits), limits.c_str());
    limits.clear();
    for (long v : rec.fieldLimits) { if (!limits.empty()) limits += ","; limits += std::to_string(v); }
    dict.SetStringEntry(d, dict.Key(kRecordFieldLimits), limits.c_str());
    if (rec.fieldUser > 0) dict.SetRealEntry(d, dict.Key(kRecordFieldUser), static_cast<AIReal>(rec.fieldUser));
    else dict.DeleteEntry(d, dict.Key(kRecordFieldUser));
    dict.SetStringEntry(d, dict.Key(kRecordOrder), rec.order.c_str());
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
                          kRecordCharUserLimit, kRecordDashesByFix, kRecordReordered, kRecordFieldLimits, kRecordFieldUser,
                          kRecordOrder, kRecordOrderBefore})
        dict.DeleteEntry(d, dict.Key(k));
    dict.Release(d);
}

void MarkReleased(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle item) {
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return;
    dict.SetIntegerEntry(d, dict.Key(kReleasedMark), 1);
    dict.Release(d);
}

bool Released(const AIArtSuite& art, const AIDictionarySuite& dict, AIArtHandle item) {
    if (!art.HasDictionary(item)) return false;
    AIDictionaryRef d = nullptr;
    if (art.GetDictionary(item, &d) || !d) return false;
    ai::int32 v = 0;
    const bool released = !dict.GetIntegerEntry(d, dict.Key(kReleasedMark), &v) && v == 1;
    dict.Release(d);
    return released;
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
        if (!dict.GetRealEntry(d, dict.Key(kRecordFieldUser), &user)) rec->fieldUser = user;
        AIBoolean b = false;
        if (!dict.GetBooleanEntry(d, dict.Key(kRecordPathfinder), &b)) rec->pathfinder = b;
        if (!dict.GetBooleanEntry(d, dict.Key(kRecordDashes), &b)) rec->dashes = b;
        if (!dict.GetBooleanEntry(d, dict.Key(kRecordDashesByFix), &b)) rec->dashesByFix = b;
        if (!dict.GetBooleanEntry(d, dict.Key(kRecordReordered), &b)) rec->reordered = b;
        const char* text = nullptr;
        if (!dict.GetStringEntry(d, dict.Key(kRecordCharLimits), &text) && text)
            for (const char* q = text; *q;) { char* end = nullptr; const long n = std::strtol(q, &end, 10); if (end == q) break; rec->charLimits.insert(n); q = *end == ',' ? end + 1 : end; }
        text = nullptr;
        if (!dict.GetStringEntry(d, dict.Key(kRecordFieldLimits), &text) && text)
            for (const char* q = text; *q;) { char* end = nullptr; const long n = std::strtol(q, &end, 10); if (end == q) break; rec->fieldLimits.insert(n); q = *end == ',' ? end + 1 : end; }
        text = nullptr;
        if (!dict.GetStringEntry(d, dict.Key(kRecordOrder), &text) && text) rec->order = text;
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
bool Overridden(SPBasicSuite* basic, const FixSuites& s, AIArtHandle item, const FixRecord& rec, bool rerouted = false) {
    short type = kUnknownArt;
    if (s.art.GetArtType(item, &type)) return false;
    // A limit on a stroke of the Appearance panel the fix did not leave (none recorded before
    // region-prep-32), or the same fields put in another order (a stroke moved back above). A
    // stroke added or removed is an edit the fix follows. Whether a stroke paints does not count.
    if (!rec.fieldLimits.empty() || !rec.order.empty()) {
        Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        if (!s.styles.GetArtStyle(item, &style) && style && !s.parser.NewParser(&p) && p) {
            struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
            if (!s.parser.ParseStyle(p, style)) {
                if (!rec.fieldLimits.empty())
                    for (ai::int32 i = 0, n = s.parser.CountPaintFields(p); i < n; ++i) {
                        AIParserPaintField f = nullptr;
                        AIStrokeStyle stroke{};
                        AIArtStylePaintData data;
                        // The route's field just rewritten by the re-fix (a recolor) is not the user's limit.
                        if (!s.parser.GetNthPaintField(p, i, &f) && f && PaintingStroke(s.parser, f) && !s.parser.GetStroke(f, &stroke, &data) &&
                            !(rerouted && RouteField(s.parser, dict.error ? nullptr : dict.operator&(), f)) &&
                            stroke.join == kAIMiterJoin && !rec.fieldLimits.count(std::lround(static_cast<double>(stroke.miterLimit) * 1000)))
                            return true;
                    }
                auto kinds = [](std::string o) { for (char& c : o) if (c == 'S') c = 's'; return o; };
                const std::string now = FieldOrder(s.parser, p);
                if (!rec.order.empty() && now.size() == rec.order.size() && kinds(now) != kinds(rec.order)) {
                    std::string a = kinds(now), b = kinds(rec.order);
                    std::sort(a.begin(), a.end());
                    std::sort(b.begin(), b.end());
                    if (a == b) return true;
                }
            }
        }
    }
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
    // The path's own stroke, by its recorded limit when the limits of the Appearance panel's strokes
    // were not recorded (before region-prep-32), and only when it is the one stroke: with several,
    // the stroke Illustrator takes for the path's own follows the Appearance panel's selection (the
    // owner's triangle with a second stroke was taken for changed by hand, 2026-10-31).
    if (rec.fieldLimits.empty() && rec.hasLimit && style.stroke.join == kAIMiterJoin &&
        std::fabs(style.stroke.miterLimit - rec.limit) > 1e-3) {
        const std::string order = FieldOrderOf(s.styles, s.parser, item);
        if (std::count(order.begin(), order.end(), 'S') + std::count(order.begin(), order.end(), 's') <= 1) return true;
    }
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
                    AILiveEffectParameters fxParams = nullptr;
                    ai::int32 tag = 0;
                    if (!s.parser.GetNthPreEffect(p, i, &fx) && EffectNamed(s.parser, fx, "Adobe Pathfinder") &&
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

// No miter spikes off, at Correct Selection: the limit of a route's strokes below Characters (a
// frame's, or a story group's below its Contents) back to the user's own.
ASErr SetRouteStrokeLimit(const FixSuites& s, AIArtHandle target, double limit) {
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(target, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    if ((e = s.parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p), fields = s.parser.CountPaintFields(p);
    bool changed = false;
    for (ai::int32 i = contents; contents >= 0 && i < fields; ++i) {
        AIParserPaintField field = nullptr;
        AIStrokeStyle stroke{};
        AIArtStylePaintData data;
        if (s.parser.GetNthPaintField(p, i, &field) || !field || !s.parser.IsStroke(field) || s.parser.GetStroke(field, &stroke, &data))
            continue;
        if (stroke.join != kAIMiterJoin || std::fabs(stroke.miterLimit - limit) <= 1e-4) continue;
        stroke.miterLimit = static_cast<AIReal>(limit);
        if (!s.parser.SetStroke(field, &stroke, &data)) changed = true;
    }
    if (!changed) return kNoErr;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    return s.styles.SetArtStyle(target, created);
}

// No miter spikes off, at Correct Selection: a frame's characters back to the one limit that ran
// through the frame before the fix (FixRecord::charUserLimit), and its route stroke with them.
// Characters that had limits of their own are left as the fix left them: what each had is not
// known.
ASErr RestoreCharLimits(const FixSuites& s, AIArtHandle frame, double limit) {
    TextRangeRef whole = nullptr;
    auto e = s.text.GetATETextRange(frame, &whole);
    if (e || !whole) return e ? e : kBadParameterErr;
    ATE::CharFeaturesRef f = nullptr;
    ATE::ATEErr ate = s.features.Initialize(&f);
    if (!ate && f) ate = s.features.SetMiterLimit(f, static_cast<ATETextDOM::Float>(limit));
    if (!ate && f) ate = s.range.ReplaceOrAddLocalCharFeatures(whole, f);
    if (f) s.features.Release(f);
    s.range.Release(whole);
    if (ate) return kBadParameterErr;
    return SetRouteStrokeLimit(s, frame, limit);
}

// Defined with native-align below.
ASErr SyncOffset(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, double weight, bool& changed);

// The native fix on a list of paths, compound paths, and text frames. One item's failure is
// counted and traced, and the others are still fixed. `refix`: the automatic re-fix, which first
// leaves alone (and unmarks) items whose managed attributes the user has changed. `done` returns
// the items fixed (re-fix bookkeeping).
// Close holes' fill (the owner, 2026-10-31): last in the style of a text frame or a story group,
// beneath its strokes, in the color of the widest stroke that paints below Characters (its own,
// or the route's), its effect reaching as far as that stroke (half its width, the stroke on type
// being Center, times the frame's scale), with that stroke's join and miter limit. On a path or a
// compound path (the owner's retest notes, 2026-11-01: outlined text was left out), last beneath
// all its fields, its widest stroke reaching by `align` (Outside its whole width, Center half,
// Inside nothing to close). Holes alone beneath a dashed stroke; letters nothing fills kept
// hollow. Taken off when `want` is false or no stroke paints there. `changed`: the style was
// rewritten.
bool TextUnfilled(const FixSuites& s, AIArtHandle target);   // below

// Whether any of a group's own paths and compound paths is filled (outlined letters whose stroke
// is the group's). False for anything but a group.
bool ChildrenFilled(const FixSuites& s, AIArtHandle group) {
    short type = kUnknownArt;
    if (s.art.GetArtType(group, &type) || type != kGroupArt) return false;
    for (AIArtHandle child = nullptr; !(child ? s.art.GetArtSibling(child, &child) : s.art.GetArtFirstChild(group, &child)) && child;) {
        short t = kUnknownArt;
        AIPathStyle style{};
        AIBoolean advanced = false;
        if (!s.art.GetArtType(child, &t) && (t == kPathArt || t == kCompoundPathArt) && !s.paint.GetPathStyle(child, &style, &advanced) &&
            style.fillPaint && style.fill.color.kind != kNoneColor)
            return true;
    }
    return false;
}
// The stroke Close holes fills for when it is not one of the target's own fields: a group of
// outlined letters' widest (GroupHoleStroke).
struct HoleStroke {
    AIStrokeStyle stroke{};
    double reach = 0;
    bool hollow = false;
};
ASErr CloseHolesField(SPBasicSuite* basic, const FixSuites& s, AIArtHandle target, bool want, double scale, bool& changed,
                      ai::uint32 align = kAIStrokeAlignmentCenter, const HoleStroke* given = nullptr) {
    changed = false;
    if (!gCloseHoles) return kNoErr;
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (live.error || dict.error) return kNoErr;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(target, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    if ((e = s.parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p), n = s.parser.CountPaintFields(p);
    // A path or a compound path (whose Contents, when it has one, is its paths' own paint).
    short targetType = kUnknownArt;
    s.art.GetArtType(target, &targetType);
    const bool shape = targetType == kPathArt || targetType == kCompoundPathArt ||
                       (targetType == kGroupArt && ReadRouteRecord(s.art, *dict.operator&(), target) == kRouteNone);
    if (!shape && contents < 0) return kNoErr;
    if (shape && align == kAIStrokeAlignmentInside) want = false;
    AIParserPaintField hole = nullptr;
    ai::int32 holeAt = -1;
    AIStrokeStyle widest{};
    bool have = false, filled = false;
    for (ai::int32 i = 0; i < n; ++i) {
        AIParserPaintField f = nullptr;
        if (s.parser.GetNthPaintField(p, i, &f) || !f) continue;
        if (HoleField(s.parser, f)) { hole = f; holeAt = i; continue; }
        AIStrokeStyle stroke{};
        AIFillStyle fill{};
        AIArtStylePaintData data;
        AIBoolean visible = true;
        s.parser.GetPaintFieldVisible(f, &visible);
        if (s.parser.IsFill(f) && visible && !s.parser.GetFill(f, &fill, &data) && fill.color.kind != kNoneColor) filled = true;
        if ((shape || i >= contents) && PaintingStroke(s.parser, f) && !s.parser.GetStroke(f, &stroke, &data) && (!have || stroke.width > widest.width)) {
            widest = stroke;
            have = true;
        }
    }
    double reach = have ? (shape && align == kAIStrokeAlignmentOutside ? widest.width : widest.width / 2) * scale : 0;
    if (given) {
        widest = given->stroke;
        reach = given->reach;
        have = reach > 0;
    }
    const ai::int32 pattern = have && widest.dash.length > 0 ? 1 : 0;
    if (!filled && shape) filled = ChildrenFilled(s, target);
    const ai::int32 hollow = given ? (given->hollow ? 1 : 0) : (!filled && (shape || TextUnfilled(s, target)) ? 1 : 0);
    if (want && have && hole && holeAt == n - 1) {
        AIFillStyle fill{};
        AIArtStylePaintData data;
        AIReal had = -1, hadLimit = -1;
        ai::int32 hadJoin = -1, hadRevision = 0, hadPattern = -1, hadHollow = -1;
        for (ai::int32 k = 0, m = s.parser.CountEffectsOfPaintField(hole); k < m; ++k) {
            AIParserLiveEffect fx = nullptr;
            AILiveEffectParameters params = nullptr;
            if (!s.parser.GetNthEffectOfPaintField(hole, k, &fx) && fx && !s.parser.GetLiveEffectParams(fx, &params) && params) {
                dict->GetRealEntry(params, dict->Key(kCloseHolesReach), &had);
                dict->GetIntegerEntry(params, dict->Key(kCloseHolesJoin), &hadJoin);
                dict->GetRealEntry(params, dict->Key(kCloseHolesLimit), &hadLimit);
                dict->GetIntegerEntry(params, dict->Key(kCloseHolesRevisionKey), &hadRevision);
                dict->GetIntegerEntry(params, dict->Key(kCloseHolesPattern), &hadPattern);
                dict->GetIntegerEntry(params, dict->Key(kCloseHolesHollow), &hadHollow);
            }
        }
        if (!s.parser.GetFill(hole, &fill, &data) && !fill.overprint == !widest.overprint &&
            (SameSolid(fill.color, widest.color) || std::memcmp(&fill.color, &widest.color, sizeof fill.color) == 0) &&
            std::fabs(had - reach) <= 1e-4 && hadJoin == static_cast<ai::int32>(widest.join) &&
            std::fabs(hadLimit - widest.miterLimit) <= 1e-4 && hadRevision == kCloseHolesRevision && hadPattern == pattern &&
            hadHollow == hollow)
            return kNoErr;   // as it should be
    }
    if (!hole && !(want && have)) return kNoErr;
    ai::int32 at = contents;
    if (hole) {
        if ((e = s.parser.RemovePaintField(p, hole, true)) != kNoErr) return e;
        if (holeAt < contents) --at;
    }
    if (want && have) {
        AIFillStyle fill{};
        fill.color = widest.color;
        fill.overprint = widest.overprint;
        AIParserPaintField field = nullptr;
        if ((e = s.parser.NewPaintFieldFill(&fill, false, nullptr, &field)) != kNoErr || !field) return e ? e : kOutOfMemoryErr;
        if ((e = s.parser.InsertNthPaintField(p, -1, field)) != kNoErr) { s.parser.DisposePaintField(field); return e; }
        // A group accepted as effect input may still contain live text. Outline this fill's
        // entire pass first, so CloseHolesGo receives all stories together as glyph paths.
        if (!shape && (e = AddOutlinePass(basic, s.parser, *dict.operator&(), p, field)) != kNoErr) return e;
        AILiveEffectParameters params = nullptr;
        if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
        dict->SetRealEntry(params, dict->Key(kCloseHolesReach), static_cast<AIReal>(reach));
        dict->SetIntegerEntry(params, dict->Key(kCloseHolesJoin), static_cast<ai::int32>(widest.join));
        dict->SetRealEntry(params, dict->Key(kCloseHolesLimit), widest.miterLimit);
        dict->SetIntegerEntry(params, dict->Key(kCloseHolesRevisionKey), kCloseHolesRevision);
        dict->SetIntegerEntry(params, dict->Key(kCloseHolesPattern), pattern);
        dict->SetIntegerEntry(params, dict->Key(kCloseHolesHollow), hollow);
        AIParserLiveEffect fx = nullptr;
        e = s.parser.NewParserLiveEffect(gCloseHoles, params, &fx);
        dict->Release(params);
        if (e || !fx) return e ? e : kOutOfMemoryErr;
        if ((e = s.parser.InsertNthEffectOfPaintField(p, field, shape ? 0 : -1, fx)) != kNoErr) { s.parser.DisposeParserLiveEffect(fx); return e; }
    }
    if (contents >= 0 && (e = s.parser.MoveGroupContentsPosition(p, at)) != kNoErr) return e;
    AIArtStyleHandle created = nullptr;
    if (s.parser.CountPaintFields(p) > 0 && ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created)) return e ? e : kBadParameterErr;
    if ((e = s.styles.SetArtStyle(target, created)) != kNoErr) return e;
    changed = true;
    return kNoErr;
}

// A group of outlined letters (Create Outlines makes one path or compound path per letter): when
// two or more of its own paths and compound paths are stroked, the widest of their strokes, its
// reach by its alignment (Inside: none), and whether none of them is filled. Close holes then goes
// on the group, beneath all its letters, so that the gaps their strokes close off between them are
// filled too (the owner's retest notes, 2026-11-01: each letter only filled its own counters).
bool GroupHoleStroke(SPBasicSuite* basic, const FixSuites& s, AIArtHandle group, HoleStroke* out) {
    int stroked = 0;
    bool filled = false;
    for (AIArtHandle child = nullptr; !(child ? s.art.GetArtSibling(child, &child) : s.art.GetArtFirstChild(group, &child)) && child;) {
        short t = kUnknownArt;
        if (s.art.GetArtType(child, &t) || (t != kPathArt && t != kCompoundPathArt)) continue;
        AIPathStyle style{};
        AIBoolean advanced = false;
        if (s.paint.GetPathStyle(child, &style, &advanced)) continue;
        if (style.fillPaint && style.fill.color.kind != kNoneColor) filled = true;
        if (!style.strokePaint || style.stroke.color.kind == kNoneColor || !(style.stroke.width > 0)) continue;
        ++stroked;
        const ai::uint32 align = ReadAlignment(basic, s, child);
        const double reach = align == kAIStrokeAlignmentInside ? 0 : align == kAIStrokeAlignmentOutside ? style.stroke.width : style.stroke.width / 2;
        if (reach > out->reach) { out->reach = reach; out->stroke = style.stroke; }
    }
    out->hollow = !filled;
    return stroked >= 2 && out->reach > 0;
}

// Whether a text frame's characters (a story group's first frame's) are all unfilled: hollow
// letters, which Close holes leaves hollow.
bool TextUnfilled(const FixSuites& s, AIArtHandle target) {
    short type = kUnknownArt;
    AIArtHandle frame = target;
    if (s.art.GetArtType(target, &type)) return false;
    if (type == kGroupArt) {
        frame = nullptr;
        for (AIArtHandle child = nullptr; !s.art.GetArtFirstChild(target, &child) && child;) {
            short t = kUnknownArt;
            if (!s.art.GetArtType(child, &t) && t == kTextFrameArt) frame = child;
            break;
        }
        if (!frame) return false;
    } else if (type != kTextFrameArt) {
        return false;
    }
    TextRangeRef whole = nullptr;
    if (s.text.GetATETextRange(frame, &whole) || !whole) return false;
    ATE::CharFeaturesRef u = nullptr;
    ATE::ATEBool8 assigned = false, on = true;
    const bool unfilled = !s.range.GetUniqueCharFeatures(whole, &u) && u && !s.features.GetFill(u, &assigned, &on) && assigned && !on;
    if (u) s.features.Release(u);
    s.range.Release(whole);
    return unfilled;
}

// Glyph tips drawn natively (the owner, 2026-11-01: InDesign cuts a sharp glyph tip flush at the
// stroke's reach, which no native Illustrator construction does; the closest, chosen by the owner,
// reaches as far with a rounded end). Fills of the stroke's color just below Characters (a story
// group's Contents), each Illustrator's own Effect > Path > Offset Path of the letters by the
// stroke's reach, outward only, so they draw no interior join and need no miter limit against
// spikes (the story's one route limit beveled every letter's tips: the owner's AA3):
//   - Trim glyph tips on: one mitered at the user's limit, at most 2 (corners under 60 degrees
//     sharper are tips), and one round, which carries every tip to the stroke's reach, rounded.
//   - Trim glyph tips off: one mitered at the user's limit, which gives each letter back the
//     whole miters a lower limit chosen against spikes took.
// Only with No miter spikes in live text on (off, the stroke keeps the user's limit and its
// spikes, as Illustrator draws them), a solid stroke of a flat color, and letters that are
// filled (the fills cover the stroke's inner half these do not draw). They follow every edit
// and draw the same without the plugin. The Offset Path effect carries kTipFillKey: 1 mitered,
// 2 round.
constexpr char kTipFillKey[] = "StrokeMasterTipFill";
const char* gTipGate = "ready";   // where TipFields last stopped (status, for tests)

// A tip fill of the fix's own, its kind (kTipFillKey), or 0.
ai::int32 TipFieldKind(const AIArtStyleParserSuite& parser, const AIDictionarySuite& dict, AIParserPaintField f, AIReal* ofst = nullptr,
                       AIReal* mlim = nullptr) {
    if (!parser.IsFill(f)) return 0;
    for (ai::int32 k = 0, n = parser.CountEffectsOfPaintField(f); k < n; ++k) {
        AIParserLiveEffect fx = nullptr;
        AILiveEffectParameters params = nullptr;
        ai::int32 kind = 0;
        if (!parser.GetNthEffectOfPaintField(f, k, &fx) && fx && !parser.GetLiveEffectParams(fx, &params) && params &&
            !dict.GetIntegerEntry(params, dict.Key(kTipFillKey), &kind) && kind > 0) {
            if (ofst) dict.GetRealEntry(params, dict.Key("ofst"), ofst);
            if (mlim) dict.GetRealEntry(params, dict.Key("mlim"), mlim);
            return kind;
        }
    }
    return 0;
}

// The tip fills of a text frame or a story group (`group`: its Contents hold the frames, so the
// letters go through Outline Type first, as the route's own stroke does). `userLimit`: the miter
// limit the user gave the text (0: unknown, Illustrator's own 4 for type). `changed`: the style
// was rewritten.
ASErr TipFields(SPBasicSuite* basic, const FixSuites& s, AIArtHandle target, bool want, bool group, double scale, double userLimit,
                bool& changed, bool shape = false, bool letters = false) {
    changed = false;
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (live.error || dict.error) return kNoErr;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(target, &style);
    if (e || !style) return e;
    AIStyleParser p = nullptr;
    if ((e = s.parser.NewParser(&p)) != kNoErr || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    const ai::int32 contents = s.parser.GetGroupContentsPosition(p), n = s.parser.CountPaintFields(p);
    if (contents < 0 && !shape) return kNoErr;
    std::vector<AIParserPaintField> had;
    std::vector<std::pair<ai::int32, std::pair<AIReal, AIReal>>> hadKinds;
    AIStrokeStyle widest{};
    AIFillStyle hadFill{};
    bool have = false, filled = false;
    for (ai::int32 i = 0; i < n; ++i) {
        AIParserPaintField f = nullptr;
        if (s.parser.GetNthPaintField(p, i, &f) || !f) continue;
        AIReal o = 0, m = 0;
        if (const ai::int32 kind = TipFieldKind(s.parser, *dict.operator&(), f, &o, &m)) {
            had.push_back(f);
            hadKinds.push_back({kind, {o, m}});
            AIArtStylePaintData data;
            s.parser.GetFill(f, &hadFill, &data);
            continue;
        }
        if (HoleField(s.parser, f)) continue;
        AIStrokeStyle stroke{};
        AIFillStyle fill{};
        AIArtStylePaintData data;
        AIBoolean visible = true;
        s.parser.GetPaintFieldVisible(f, &visible);
        if (s.parser.IsFill(f) && visible && !s.parser.GetFill(f, &fill, &data) && fill.color.kind != kNoneColor) filled = true;
        if ((shape || i >= contents) && PaintingStroke(s.parser, f) && !s.parser.GetStroke(f, &stroke, &data) &&
            (!have || stroke.width > widest.width)) {
            widest = stroke;
            have = true;
        }
    }
    // A group's letters are filled by their own paths (outlined text whose stroke is the group's).
    if (!filled && shape) filled = ChildrenFilled(s, target);
    if (want && (!have || widest.dash.length > 0 || widest.color.kind == kGradient || widest.color.kind == kPattern ||
                 (!filled && (shape || TextUnfilled(s, target))))) {
        gTipGate = !have ? "no-stroke" : widest.dash.length > 0 ? "dashed" : !filled ? "unfilled" : "paint";
        want = false;
    }
    const AIReal reach = have ? static_cast<AIReal>(widest.width / 2 * scale) : 0;
    // The user's limit: the text's, or a shape's own stroke's.
    const AIReal limit = static_cast<AIReal>(userLimit > 1 ? userLimit : (have && widest.miterLimit > 1 ? widest.miterLimit : 4));
    std::vector<std::pair<ai::int32, std::pair<AIReal, AIReal>>> wanted;
    // Flat tips, the opt-in: one fill of StrokeMaster's own effect (kind 3) in place of the two.
    const bool flat = Switches().glyphTips && Switches().flatTips && gFlatTips;
    // A shape: the limit its outer corners want, judged without its interior spikes (the mitered
    // fill is outward only), as InDesign judges a shape's corners: whole points kept past the limit,
    // glyph tips trimmed within it. A thin triangle whose interior spike lowered the stroke's one
    // limit got its whole point back so (the owner's question with CC3, 2026-11-01). Outlined
    // letters (`letters`) get the round fill too with Trim glyph tips; another shape gets the
    // mitered fill only where it adds to its stroke.
    if (want && reach > 0 && shape) {
        std::vector<ise_miter::Contour> cs;
        short targetType = kUnknownArt;
        s.art.GetArtType(target, &targetType);
        if (targetType == kGroupArt) {
            for (AIArtHandle child = nullptr; !(child ? s.art.GetArtSibling(child, &child) : s.art.GetArtFirstChild(target, &child)) && child;) {
                short t = kUnknownArt;
                if (!s.art.GetArtType(child, &t) && (t == kPathArt || t == kCompoundPathArt)) Contours(s, child, cs);
            }
        } else {
            Contours(s, target, cs);
        }
        const double outer = cs.empty() ? limit
                                        : native_miter::ChooseLimitForOutline(cs, reach, limit, false, false, false, Switches().wholePoints,
                                                                               Switches().glyphTips).limit;
        if (letters || outer > widest.miterLimit + 1e-6) wanted.push_back({1, {reach, static_cast<AIReal>(outer)}});
        else gTipGate = cs.empty() ? "shape.no-contours" : "shape.nothing-to-add";
        if (letters && Switches().glyphTips) wanted.push_back({2, {reach, 0}});
    } else if (want && reach > 0 && flat) {
        wanted.push_back({3, {reach, limit}});
    } else if (want && reach > 0) {
        wanted.push_back({1, {reach, Switches().glyphTips ? (std::min)(limit, static_cast<AIReal>(2)) : limit}});
        if (Switches().glyphTips) wanted.push_back({2, {reach, 0}});
    }
    // As they should be: the same fills, offsets, limits, and paint.
    if (wanted.size() == hadKinds.size()) {
        bool same = !had.empty() || wanted.empty();
        for (size_t k = 0; k < wanted.size() && same; ++k)
            same = wanted[k].first == hadKinds[k].first && std::fabs(wanted[k].second.first - hadKinds[k].second.first) <= 1e-4 &&
                   (wanted[k].first == 2 || std::fabs(wanted[k].second.second - hadKinds[k].second.second) <= 1e-4);
        if (same && !had.empty())
            same = !hadFill.overprint == !widest.overprint &&
                   (SameSolid(hadFill.color, widest.color) || std::memcmp(&hadFill.color, &widest.color, sizeof hadFill.color) == 0);
        if (same) return kNoErr;
    }
    AILiveEffectHandle offset = nullptr, outline = nullptr;
    if (!wanted.empty() && (live->GetLiveEffectHandleByName("Adobe Offset Path", &offset) || !offset ||
                            (group && (live->GetLiveEffectHandleByName("Adobe Outline Type", &outline) || !outline))))
        return kBadParameterErr;
    ai::int32 at = contents;
    for (AIParserPaintField f : had) {
        ai::int32 index = -1;
        for (ai::int32 i = 0, m = s.parser.CountPaintFields(p); i < m; ++i) {
            AIParserPaintField g = nullptr;
            if (!s.parser.GetNthPaintField(p, i, &g) && g == f) index = i;
        }
        if ((e = s.parser.RemovePaintField(p, f, true)) != kNoErr) return e;
        if (index >= 0 && index < at) --at;
    }
    // Just below Characters (Contents): above the strokes, which stay the last fields. On a shape,
    // last, beneath its fills (which cover the offsets' inner part) and above Close holes' fill.
    ai::int32 slot = at;
    if (shape) {
        slot = s.parser.CountPaintFields(p);
        AIParserPaintField last = nullptr;
        if (slot > 0 && !s.parser.GetNthPaintField(p, slot - 1, &last) && last && HoleField(s.parser, last)) --slot;
    }
    for (const auto& w : wanted) {
        AIFillStyle band{};
        band.color = widest.color;
        band.overprint = widest.overprint;
        AIParserPaintField field = nullptr;
        if ((e = s.parser.NewPaintFieldFill(&band, false, nullptr, &field)) != kNoErr || !field) return e ? e : kOutOfMemoryErr;
        if ((e = s.parser.InsertNthPaintField(p, slot, field)) != kNoErr) { s.parser.DisposePaintField(field); return e; }
        ++slot;
        ai::int32 position = 0;
        if (w.first == 3) {
            AILiveEffectParameters params = nullptr;
            if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
            dict->SetRealEntry(params, dict->Key("ofst"), w.second.first);
            dict->SetRealEntry(params, dict->Key("mlim"), w.second.second);
            dict->SetIntegerEntry(params, dict->Key(kTipFillKey), 3);
            AIParserLiveEffect fx = nullptr;
            e = s.parser.NewParserLiveEffect(gFlatTips, params, &fx);
            dict->Release(params);
            if (e || !fx) return e ? e : kOutOfMemoryErr;
            if ((e = s.parser.InsertNthEffectOfPaintField(p, field, 0, fx)) != kNoErr) { s.parser.DisposeParserLiveEffect(fx); return e; }
            continue;
        }
        if (group) {
            AILiveEffectParameters params = nullptr;
            if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
            dict->SetBooleanEntry(params, dict->Key("-DefaultApplyEffectsKey"), false);
            AIParserLiveEffect fx = nullptr;
            e = s.parser.NewParserLiveEffect(outline, params, &fx);
            dict->Release(params);
            if (e || !fx) return e ? e : kOutOfMemoryErr;
            if ((e = s.parser.InsertNthEffectOfPaintField(p, field, position++, fx)) != kNoErr) { s.parser.DisposeParserLiveEffect(fx); return e; }
        }
        AILiveEffectParameters params = nullptr;
        if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
        dict->SetRealEntry(params, dict->Key("ofst"), w.second.first);
        dict->SetIntegerEntry(params, dict->Key("jntp"), w.first == 1 ? 2 : 0);
        dict->SetRealEntry(params, dict->Key("mlim"), w.first == 1 ? w.second.second : static_cast<AIReal>(4));
        dict->SetIntegerEntry(params, dict->Key(kTipFillKey), w.first);
        AIParserLiveEffect fx = nullptr;
        e = s.parser.NewParserLiveEffect(offset, params, &fx);
        dict->Release(params);
        if (e || !fx) return e ? e : kOutOfMemoryErr;
        if ((e = s.parser.InsertNthEffectOfPaintField(p, field, position, fx)) != kNoErr) { s.parser.DisposeParserLiveEffect(fx); return e; }
    }
    if (contents >= 0 && (e = s.parser.MoveGroupContentsPosition(p, at)) != kNoErr) return e;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    if ((e = s.styles.SetArtStyle(target, created)) != kNoErr) return e;
    changed = true;
    return kNoErr;
}

// Defined with native-align below.
bool Realign(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, ai::uint32 wanted);

// A frame of threaded text judged against the character limits every frame of its story recorded:
// text flowing between frames carries a sibling's limits, which are no hand change (the re-fix
// released such frames: the third review of 2026-11-01). Leaves `rec` as it is for a lone frame.
void StoryLimits(SPBasicSuite* basic, const FixSuites& s, AIArtHandle frame, FixRecord& rec) {
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIMatchingArtSuite> matching(basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    ai::int32 index = -1;
    if (dict.error || matching.error || memory.error || rec.charLimits.empty() || s.text.GetStoryIndex(frame, &index) || index < 0) return;
    AIMatchingArtSpec spec(kTextFrameArt, 0, 0);
    AIArtHandle** every = nullptr;
    ai::int32 n = 0;
    if (matching->GetMatchingArt(&spec, 1, &every, &n) || !every) return;
    std::set<long> all;
    int frames = 0;
    for (ai::int32 i = 0; i < n; ++i) {
        ai::int32 k = -1;
        if (s.text.GetStoryIndex((*every)[i], &k) || k != index) continue;
        ++frames;
        FixRecord r;
        if (Marked(s.art, *dict.operator&(), (*every)[i], &r)) all.insert(r.charLimits.begin(), r.charLimits.end());
    }
    memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(every));
    if (frames > 1) rec.charLimits.insert(all.begin(), all.end());
}

ASErr FixItems(SPBasicSuite* basic, const FixSuites& s, const std::vector<AIArtHandle>& items, NativeFixStats& st,
               bool refix = false, std::vector<AIArtHandle>* done = nullptr) {
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (dict.error) { st.gate = "suites"; return dict.error; }
    std::vector<ai::uint32> aligns(items.size(), kAIStrokeAlignmentCenter);
    for (size_t i = 0; i < items.size(); ++i) {
        short type = kUnknownArt;
        if (s.art.GetArtType(items[i], &type) || type == kTextFrameArt || type == kGroupArt) continue;
        aligns[i] = ReadAlignment(basic, s, items[i]);
    }
    int failed = 0;
    ASErr first = kNoErr;
    std::set<AIArtHandle> holeGroups;   // groups of outlined letters whose Close holes this pass took
    for (size_t i = 0; i < items.size(); ++i) {
        short type = kUnknownArt;
        if (s.art.GetArtType(items[i], &type)) { ++failed; continue; }
        // Taken back by the user: left alone unless the user presses Correct Selection (gAutomatic),
        // a story group too, before its route follows an edit (a released group was rerouted: the
        // third review of 2026-11-01).
        if ((refix || gAutomatic) && Released(s.art, *dict.operator&(), items[i])) {
            if (done) done->push_back(items[i]);
            continue;
        }
        // An Inside drawn as two fills (native-align): the shrunk fill shown or hidden for the outline
        // as it is now.
        {
            const AlignRecord ar = ReadAlignRecord(basic, s.art, items[i]);
            if ((ar.as == "offset" || ar.as == "type-offset") && ar.weight > 0) {
                bool synced = false;
                if (!SyncOffset(basic, s, items[i], ar.weight, synced) && synced) ++st.offsetSynced;
            }
        }
        // A routed frame or story group: its route follows the edit first (a recolor, for one).
        bool rerouted = false;
        if (refix && Switches().textBehind && ReadRouteRecord(s.art, *dict.operator&(), items[i]) != kRouteNone) {
            Suite<AIATEPaintSuite> atePaint(basic, kAIATEPaintSuite, kAIATEPaintVersion);
            if (!atePaint.error) {
                const RouteSuites rs{basic, s.art, s.styles, s.parser, s.text, s.range, s.features, *atePaint.operator&(),
                                     *dict.operator&()};
                bool changed = false, released = false;
                ASErr e = kNoErr;
                // A frame of a routed story group: its paint is the group's, and its own record
                // a leftover.
                if (type == kTextFrameArt && InRoutedStory(rs, items[i])) WriteRouteRecord(rs, items[i], kRouteNone, "");
                else e = Reroute(rs, items[i], changed, released);
                if (e) { ++failed; if (!first) first = e; Trace("fix.reroute", e, ""); if (type == kGroupArt) continue; }
                if (changed) { ++st.rerouted; rerouted = true; }
                if (released) { ++st.routeReleased; MarkReleased(s.art, *dict.operator&(), items[i]); }
            }
        }
        if (type == kGroupArt) {
            const bool strokeRoute = ReadRouteRecord(s.art, *dict.operator&(), items[i]) == kRouteStroke;
            // Its route stroke's limits, as the fix left them: changed by hand, the group is the
            // user's, as a marked item is; only Correct Selection takes it back.
            auto limitsOf = [&](AIArtHandle g) {
                std::string out;
                AIArtStyleHandle gs = nullptr;
                AIStyleParser gp = nullptr;
                if (s.styles.GetArtStyle(g, &gs) || !gs || s.parser.NewParser(&gp) || !gp) return out;
                struct DisposeGroup { const AIArtStyleParserSuite& api; AIStyleParser p; ~DisposeGroup() { api.DisposeParser(p); } } disposeGroup{s.parser, gp};
                if (s.parser.ParseStyle(gp, gs)) return out;
                for (ai::int32 k = std::max<ai::int32>(0, s.parser.GetGroupContentsPosition(gp)), n = s.parser.CountPaintFields(gp); k < n; ++k) {
                    AIParserPaintField f = nullptr;
                    AIStrokeStyle sk{};
                    AIArtStylePaintData dk;
                    if (!s.parser.GetNthPaintField(gp, k, &f) && f && s.parser.IsStroke(f) && !s.parser.GetStroke(f, &sk, &dk) && sk.join == kAIMiterJoin)
                        out += std::to_string(std::lround(static_cast<double>(sk.miterLimit) * 1000)) + ",";
                }
                return out;
            };
            AIDictionaryRef gd = nullptr;
            const bool hasDict = !s.art.GetDictionary(items[i], &gd) && gd;
            struct ReleaseGroupDict { const AIDictionarySuite& api; AIDictionaryRef d; ~ReleaseGroupDict() { if (d) api.Release(d); } } releaseGroupDict{*dict.operator&(), hasDict ? gd : nullptr};
            if (refix || gAutomatic) {
                const char* had = nullptr;
                const bool handSet = strokeRoute && !rerouted && hasDict && !dict->GetStringEntry(gd, dict->Key(kGroupLimits), &had) && had &&
                                     std::string(had) != limitsOf(items[i]);
                if (handSet) { MarkReleased(s.art, *dict.operator&(), items[i]); ++st.released; }
                if (handSet || Released(s.art, *dict.operator&(), items[i])) { if (done) done->push_back(items[i]); continue; }
            } else if (hasDict) {
                dict->DeleteEntry(gd, dict->Key(kReleasedMark));   // Correct Selection takes it back
            }
            // The story's own limit, when its frames recorded the same one.
            double limit = -1;
            AIArtHandle child = nullptr;
            for (s.art.GetArtFirstChild(items[i], &child); strokeRoute && child && limit != 0;) {
                FixRecord r;
                if (Marked(s.art, *dict.operator&(), child, &r)) {
                    if (!(r.charUserLimit > 0) || (limit > 0 && std::fabs(limit - r.charUserLimit) > 1e-4)) limit = 0;
                    else limit = r.charUserLimit;
                }
                if (s.art.GetArtSibling(child, &child)) break;
            }
            if (Switches().textMiter && strokeRoute) {
                const ASErr e = FixStoryStroke(basic, s, items[i], st, limit > 0 ? limit : 0);
                if (e) { ++failed; if (!first) first = e; Trace("fix.story-stroke", e, ""); continue; }
            } else if (gReconcile && strokeRoute) {
                if (limit > 0) {
                    const ASErr e = SetRouteStrokeLimit(s, items[i], limit);
                    if (e) { ++failed; if (!first) first = e; Trace("fix.story-limit", e, ""); continue; }
                }
            }
            if (hasDict && strokeRoute) dict->SetStringEntry(gd, dict->Key(kGroupLimits), limitsOf(items[i]).c_str());
            // Close holes on the story group, beneath its stroke.
            {
                double groupScale = 1;
                AIArtHandle lead = nullptr;
                short ft = kUnknownArt;
                if (!s.art.GetArtFirstChild(items[i], &lead) && lead && !s.art.GetArtType(lead, &ft) && ft == kTextFrameArt) groupScale = FrameScale(s, lead);
                const bool want = Switches().closeHoles && Switches().textBehind && strokeRoute;
                bool holes = false;
                if ((want || gReconcile) && !CloseHolesField(basic, s, items[i], want, groupScale, holes) && holes) ++st.holes;
                // The glyph tips' fills (TipFields), with the story's own limit.
                const bool tips = Switches().textBehind && Switches().textMiter && strokeRoute;
                bool tipped = false;
                if ((tips || gReconcile) && !TipFields(basic, s, items[i], tips, true, groupScale, limit > 0 ? limit : 0, tipped) && tipped)
                    ++st.tips;
            }
            if (done) done->push_back(items[i]);
            continue;
        }
        FixRecord prior;
        const bool hasPrior = Marked(s.art, *dict.operator&(), items[i], &prior);
        if (refix && !hasPrior) { if (done) done->push_back(items[i]); continue; }   // routed only
        // A dashed Outside stroke the opt-in drew as Center at twice the weight under its fill, since
        // set Inside or Outside in the Stroke panel: the alignment is the user's, given to the stroke
        // as it was (the owner's retest P-dash, 2026-10-31: it went to the doubled stroke, so Inside
        // hid it). Taken first: the new alignment also drops the dash alignment, which is no change
        // by hand. The fix and the re-fix's dashed opt-in then judge it again.
        bool realigned = false;
        if (type != kTextFrameArt && aligns[i] != kAIStrokeAlignmentCenter && ReadAlignRecord(basic, s.art, items[i]).as == "doubled" &&
            Realign(basic, s, items[i], aligns[i])) {
            ++st.realigned;
            realigned = true;
            aligns[i] = ReadAlignment(basic, s, items[i]);
        }
        FixRecord judged = prior;
        if (refix && hasPrior && type == kTextFrameArt) StoryLimits(basic, s, items[i], judged);
        if (refix && hasPrior && !realigned && Overridden(basic, s, items[i], judged, rerouted)) {
            // A limit set in the Stroke panel on text whose characters are unstroked lands on the
            // characters, where it draws nothing: it is carried to the strokes below Characters
            // (the route's, or the object's own), which then are the user's (the owner's retest
            // S3, 2026-10-31: "doesn't change the result visually, but value does remain").
            if (type == kTextFrameArt) {
                std::set<long> added;
                for (long v : RunLimits(basic, s, items[i])) if (!prior.charLimits.count(v)) added.insert(v);
                TextRangeRef whole = nullptr;
                ATE::CharFeaturesRef u = nullptr;
                ATE::ATEBool8 assigned = false, on = false;
                bool unstroked = false;
                if (!s.text.GetATETextRange(items[i], &whole) && whole) {
                    if (!s.range.GetUniqueCharFeatures(whole, &u) && u) unstroked = !s.features.GetStroke(u, &assigned, &on) && !on;
                    if (u) s.features.Release(u);
                    s.range.Release(whole);
                }
                if (added.size() == 1 && unstroked && !SetRouteStrokeLimit(s, items[i], *added.begin() / 1000.0)) ++st.carried;
            }
            Unmark(s.art, *dict.operator&(), items[i]);
            MarkReleased(s.art, *dict.operator&(), items[i]);
            ++st.released;
            continue;
        }
        FixRecord rec = hasPrior ? prior : FixRecord{};
        ASErr err = kNoErr;
        if (type == kTextFrameArt) {
            ++st.frames;
            // The object's own strokes below its fills (the edit added one, for one), or back where
            // the user had them when Strokes behind or its Appearance opt-in is off at Correct Selection.
            if (Switches().textBehind && Switches().appearanceStrokes) {
                bool moved = false;
                if (!StrokesBelowFills(s.art, *dict.operator&(), s.styles, s.parser, items[i], moved) && moved) ++st.objectStrokes;
            } else if (gReconcile) {
                bool restored = false;
                if (!RestoreFieldOrder(s.art, *dict.operator&(), s.styles, s.parser, items[i], restored) && restored) ++st.orderRestored;
            }
            // Close holes, beneath the strokes; a frame of a routed story group has its group's.
            {
                AIArtHandle up = nullptr;
                short upType = kUnknownArt;
                const bool inStory = !s.art.GetArtParent(items[i], &up) && up && !s.art.GetArtType(up, &upType) && upType == kGroupArt &&
                                     GroupOwnsTextFields(ReadRouteRecord(s.art, *dict.operator&(), up));
                const bool want = Switches().closeHoles && Switches().textBehind && !inStory;
                bool holes = false;
                if ((want || gReconcile || inStory) && !CloseHolesField(basic, s, items[i], want, FrameScale(s, items[i]), holes) && holes) ++st.holes;
            }
            double userLimit = 0;
            if (Switches().textMiter) err = FixCharacters(basic, s, items[i], st, rec, hasPrior ? &prior : nullptr, &userLimit);
            else if (hasPrior) {
                userLimit = prior.charUserLimit;   // kept while the switch is off
                if (gReconcile && userLimit > 0) err = RestoreCharLimits(s, items[i], userLimit);
            }
            rec.charUserLimit = userLimit;
            const auto limits = RunLimits(basic, s, items[i]);
            if (!limits.empty()) rec.charLimits = limits;
            // The glyph tips' fills (TipFields); a frame of a routed story group has its group's.
            if (!err) {
                AIArtHandle up = nullptr;
                short upType = kUnknownArt;
                const bool inStory = !s.art.GetArtParent(items[i], &up) && up && !s.art.GetArtType(up, &upType) && upType == kGroupArt &&
                                     GroupOwnsTextFields(ReadRouteRecord(s.art, *dict.operator&(), up));
                const bool tips = Switches().textBehind && Switches().textMiter && !inStory;
                bool tipped = false;
                if ((tips || gReconcile || inStory) &&
                    !TipFields(basic, s, items[i], tips, false, FrameScale(s, items[i]), userLimit, tipped) && tipped)
                    ++st.tips;
            }
        } else {
            ++st.paths;
            err = FixPath(basic, s, items[i], aligns[i], st, hasPrior ? &prior : nullptr, rec);
            // Close holes on paths and compound paths too (outlined text), last beneath their fields;
            // on a group of outlined letters, on the group (GroupHoleStroke), each letter's own taken off.
            if (!err) {
                const bool want = Switches().closeHoles;
                AIArtHandle up = nullptr;
                short upType = kUnknownArt;
                HoleStroke group;
                const bool letters = !s.art.GetArtParent(items[i], &up) && up && !s.art.GetArtType(up, &upType) && upType == kGroupArt &&
                                     ReadRouteRecord(s.art, *dict.operator&(), up) == kRouteNone && GroupHoleStroke(basic, s, up, &group);
                // Outlined text whose stroke is the group's own (the Appearance panel's, carried by
                // Create Outlines): Close holes and the tips on the group (the owner's retest BB5).
                const bool grouped = !letters && up && upType == kGroupArt && ReadRouteRecord(s.art, *dict.operator&(), up) == kRouteNone &&
                                     HasPaintingStroke(s.styles, s.parser, up);
                bool holes = false;
                if ((want || gReconcile) &&
                    !CloseHolesField(basic, s, items[i], want && !letters && !grouped, 1, holes, ReadAlignment(basic, s, items[i])) && holes)
                    ++st.holes;
                const bool firstOfGroup = (letters || grouped) && holeGroups.insert(up).second;
                if (letters && firstOfGroup && (want || gReconcile) &&
                    !CloseHolesField(basic, s, up, want, 1, holes, kAIStrokeAlignmentCenter, &group) && holes)
                    ++st.holes;
                if (grouped && firstOfGroup && (want || gReconcile) && !CloseHolesField(basic, s, up, want, 1, holes) && holes)
                    ++st.holes;
                // Compound paths and groups of stroked paths are treated as outlined text
                // for rounded tips. Illustrator provides no reliable original-text identity;
                // ordinary compound shapes and grouped paths share this classification.
                const bool tips = Switches().shapeMiter;
                const bool outlinedLetter = type == kCompoundPathArt || letters;
                const double user = rec.fieldUser > 0 ? rec.fieldUser : rec.userLimit;
                bool tipped = false;
                if ((tips || gReconcile) &&
                    !TipFields(basic, s, items[i], tips && !grouped, false, 1, user, tipped, true, outlinedLetter) && tipped)
                    ++st.tips;
                if (grouped && firstOfGroup && (tips || gReconcile) && !TipFields(basic, s, up, tips, false, 1, 0, tipped, true, true) && tipped)
                    ++st.tips;
            }
        }
        if (err) {
            ++failed;
            if (!first) first = err;
            Trace(type == kTextFrameArt ? "fix.characters" : "fix.path", err, "");
        }
        rec.order = FieldOrderOf(s.styles, s.parser, items[i]);
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
    // Selected story groups on the stroke route get their stroke's limit too.
    {
        Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
        AIMatchingArtSpec groupSpec(kGroupArt, kArtSelected, kArtSelected);
        AIArtHandle** groups = nullptr;
        ai::int32 n = 0;
        if (!dict.error && !matching->GetMatchingArt(&groupSpec, 1, &groups, &n) && groups) {
            for (ai::int32 i = 0; i < n; ++i)
                if (ReadRouteRecord(*art.operator&(), *dict.operator&(), (*groups)[i]) == kRouteStroke) items.push_back((*groups)[i]);
            memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(groups));
        }
    }
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        short ptype = kUnknownArt;
        if (!art->GetArtParent(a, &parent) && parent && !art->GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        items.push_back(a);
    }
    return FixItems(message.basic, s, items, st);
}

// Defined with native-align below.
size_t RefixDashes(SPBasicSuite* basic, const FixSuites& s, const std::vector<AIArtHandle>& fixed);

// Defined below: the panel's Correct Selection.
ASErr CorrectSelection(SPMessageData& message, State& state);

// Automatic correction (region-prep-31): what Correct Selection does, on art never corrected that
// is new or was just edited, in the undo step of that edit, or on everything in a document as it
// opens (region-prep-32, the opt-in). Left alone: art the user took back (Released), locked,
// hidden, or clipping art, art inside anything but groups and compound paths (a blend, an
// envelope, a symbol), art with no stroke that paints (the Appearance panel's strokes count), and
// more than `cap` such items at once (Correct Selection is there for those). Threaded text is
// corrected across its frames as Correct Selection does it, its frames grouped (the owner,
// 2026-10-31), with the selection put back after. Returns the items corrected.
// Before an automatic pass: each marked item whose managed attributes differ from its record was
// changed by hand where the timer did not see it (another machine, or while Correct automatically
// was off) and is released, so the pass leaves it alone (the review of 2026-11-01). Checked once,
// before the pass: within it, an item fixed twice (its strokes moved below its fills, then fixed
// again) would differ from its record by the pass's own change.
// A frame of threaded text is judged against the character limits every frame of its story
// recorded: text flowing between frames carries a sibling's limits, which are not a hand change
// (the second review of 2026-11-01). Returns the items the pass may correct: not released,
// locked, or hidden; Correct Selection also reorders, routes, and groups what it is given, so a
// released item is kept out of it altogether (the second review).
std::vector<AIArtHandle> ReleaseOverridden(SPBasicSuite* basic, const FixSuites& s, const std::vector<AIArtHandle>& items) {
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIMatchingArtSuite> matching(basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    if (dict.error || matching.error || memory.error) return items;
    std::map<ai::int32, std::vector<AIArtHandle>> stories;
    {
        AIMatchingArtSpec spec(kTextFrameArt, 0, 0);
        AIArtHandle** every = nullptr;
        ai::int32 n = 0;
        if (!matching->GetMatchingArt(&spec, 1, &every, &n) && every) {
            for (ai::int32 i = 0; i < n; ++i) {
                ai::int32 index = -1;
                if (!s.text.GetStoryIndex((*every)[i], &index) && index >= 0) stories[index].push_back((*every)[i]);
            }
            memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(every));
        }
    }
    std::map<ai::int32, std::set<long>> storyLimits;
    for (const auto& kv : stories) {
        if (kv.second.size() < 2) continue;
        auto& all = storyLimits[kv.first];
        for (AIArtHandle f : kv.second) {
            FixRecord r;
            if (Marked(s.art, *dict.operator&(), f, &r)) all.insert(r.charLimits.begin(), r.charLimits.end());
        }
    }
    std::vector<AIArtHandle> keep;
    for (AIArtHandle a : items) {
        if (!s.art.ValidArt(a, true)) continue;
        ai::int32 attr = 0;
        if (!s.art.GetArtUserAttr(a, kArtLocked | kArtHidden, &attr) && attr) continue;
        FixRecord prior;
        short type = kUnknownArt;
        if (!s.art.GetArtType(a, &type) && type != kGroupArt && Marked(s.art, *dict.operator&(), a, &prior)) {
            ai::int32 index = -1;
            if (type == kTextFrameArt && !prior.charLimits.empty() && !s.text.GetStoryIndex(a, &index) && storyLimits.count(index))
                prior.charLimits = storyLimits[index];
            if (Overridden(basic, s, a, prior)) {
                Unmark(s.art, *dict.operator&(), a);
                MarkReleased(s.art, *dict.operator&(), a);
            }
        }
        if (Released(s.art, *dict.operator&(), a)) continue;
        keep.push_back(a);
    }
    return keep;
}

size_t AutoCorrect(SPMessageData& message, State& state, const FixSuites& s, const std::vector<AIArtHandle>& fresh,
                   size_t cap = 500) {
    const Automatic automatic;
    ReleaseOverridden(message.basic, s, fresh);   // released, the loop below leaves them out
    const auto& sw = Switches();
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIATEPaintSuite> atePaint(message.basic, kAIATEPaintSuite, kAIATEPaintVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    if (dict.error || atePaint.error || matching.error || memory.error) return 0;
    const RouteSuites rs{message.basic, s.art, s.styles, s.parser, s.text, s.range, s.features, *atePaint.operator&(),
                         *dict.operator&()};
    std::vector<AIArtHandle> frames, paths;
    for (AIArtHandle a : fresh) {
        if (Released(s.art, *dict.operator&(), a)) continue;
        ai::int32 attr = 0;
        if (!s.art.GetArtUserAttr(a, kArtLocked | kArtHidden | kArtIsClipMask, &attr) && attr) continue;
        bool plain = true;
        for (AIArtHandle up = nullptr, at = a; plain && !s.art.GetArtParent(at, &up) && up; at = up) {
            short ptype = kUnknownArt;
            plain = !s.art.GetArtType(up, &ptype) && (ptype == kGroupArt || ptype == kCompoundPathArt);
        }
        if (!plain) continue;
        short type = kUnknownArt;
        if (s.art.GetArtType(a, &type)) continue;
        const bool fieldStroke = HasPaintingStroke(s.styles, s.parser, a);
        if (type == kTextFrameArt) {
            CharPaint c;
            if (ReadCharPaint(rs, {a}, c) || !c.any || (c.noStroke && !fieldStroke)) continue;
            frames.push_back(a);
        } else if (type == kPathArt || type == kCompoundPathArt) {
            AIPathStyle style{};
            AIBoolean advanced = false;
            if (s.paint.GetPathStyle(a, &style, &advanced) || ((!style.strokePaint || !(style.stroke.width > 0)) && !fieldStroke)) continue;
            paths.push_back(a);
        }
    }
    if (frames.size() + paths.size() > cap) { ++state.refix.skipped; return 0; }
    if (frames.empty() && paths.empty()) return 0;
    size_t made = 0;
    // Threaded text: every frame of each story with more than one, through Correct Selection.
    if (sw.textBehind && sw.textStory) {
        std::map<ai::int32, std::vector<AIArtHandle>> stories;
        {
            AIMatchingArtSpec spec(kTextFrameArt, 0, 0);
            AIArtHandle** every = nullptr;
            ai::int32 n = 0;
            if (!matching->GetMatchingArt(&spec, 1, &every, &n) && every) {
                for (ai::int32 i = 0; i < n; ++i) {
                    ai::int32 index = -1;
                    if (!s.text.GetStoryIndex((*every)[i], &index) && index >= 0) stories[index].push_back((*every)[i]);
                }
                memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(every));
            }
        }
        std::vector<AIArtHandle> threaded, rest;
        for (AIArtHandle f : frames) {
            ai::int32 index = -1;
            const bool many = !s.text.GetStoryIndex(f, &index) && index >= 0 && stories[index].size() > 1;
            if (!many) { rest.push_back(f); continue; }
            for (AIArtHandle g : stories[index])
                if (std::find(threaded.begin(), threaded.end(), g) == threaded.end()) threaded.push_back(g);
        }
        // Every frame the stories add, checked for hand changes and kept out when released,
        // locked, or hidden (only the new art was: the second review of 2026-11-01).
        threaded = ReleaseOverridden(message.basic, s, threaded);
        if (!threaded.empty()) {
            std::vector<AIArtHandle> selection;
            AIArtHandle** chosen = nullptr;
            ai::int32 n = 0;
            if (!matching->GetSelectedArt(&chosen, &n) && chosen) {
                for (ai::int32 i = 0; i < n; ++i) selection.push_back((*chosen)[i]);
                memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(chosen));
            }
            matching->DeselectAll();
            for (AIArtHandle g : threaded) s.art.SetArtUserAttr(g, kArtSelected, kArtSelected);
            if (!CorrectSelection(message, state)) { made += threaded.size(); state.refix.threaded += static_cast<unsigned long long>(state.correct.runs); }
            matching->DeselectAll();
            for (AIArtHandle a : selection) if (s.art.ValidArt(a, true)) s.art.SetArtUserAttr(a, kArtSelected, kArtSelected);
            frames = rest;
        }
    }
    if (sw.textBehind && !frames.empty()) NativeRoute(message, state, &frames);
    std::vector<AIArtHandle> all = frames;
    all.insert(all.end(), paths.begin(), paths.end());
    if (all.empty()) return made;
    NativeFixStats st{};
    FixItems(message.basic, s, all, st);
    state.refix.dashed += RefixDashes(message.basic, s, paths);
    return made + all.size();
}

// The switches compared for Reapply: those that say what a correction writes.
ai_settings::Settings Corrections(ai_settings::Settings s) {
    s.enabled = s.followEdits = s.openDocuments = true;
    return s;
}

// The active document's corrected art brought to the switches as they are now, through Correct
// Selection, after a switch changed (the panel, Restore Defaults, the bridge) or as the document
// opens with Also documents as they open (the owner, 2026-10-31: "active document at once"; S7,
// S9, and T5 had expected it of a move or of opening). Art the user took back stays theirs;
// threaded text goes whole, each story with all its frames. `opening`: what was never corrected
// is corrected too (AutoCorrect). Returns the items corrected.
size_t Reapply(SPMessageData& message, State& state, const FixSuites& s, bool opening) {
    const Automatic automatic;
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    if (dict.error || matching.error || memory.error) return 0;
    AIMatchingArtSpec specs[4] = {AIMatchingArtSpec(kPathArt, 0, 0), AIMatchingArtSpec(kCompoundPathArt, 0, 0),
                                  AIMatchingArtSpec(kTextFrameArt, 0, 0), AIMatchingArtSpec(kGroupArt, 0, 0)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    if (matching->GetMatchingArt(specs, 4, &matches, &count) || !matches) return 0;
    std::vector<AIArtHandle> items, fresh, frames;
    std::set<ai::int32> stories;
    // Art made since the last scan: changed after it (the seen table holds only corrected art, so
    // "not in it" took every never-corrected object for new: the third review of 2026-11-01).
    const auto& rs0 = state.refix;
    auto madeSinceScan = [&](AIArtHandle a) {
        size_t v = 0;
        return rs0.count > 0 && !s.art.GetArtTimeStamp(a, kAITimeStampMaxFromArtAndChildren, &v) && v > rs0.global;
    };
    auto add = [&](AIArtHandle a) { if (std::find(items.begin(), items.end(), a) == items.end()) items.push_back(a); };
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        short type = kUnknownArt, ptype = kUnknownArt;
        if (s.art.GetArtType(a, &type)) continue;
        if (!s.art.GetArtParent(a, &parent) && parent && !s.art.GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        ai::int32 attr = 0;
        if (!s.art.GetArtUserAttr(a, kArtLocked | kArtHidden, &attr) && attr) continue;
        if (type == kGroupArt) {
            if (ReadRouteRecord(s.art, *dict.operator&(), a) != kRouteNone) for (AIArtHandle f : RouteFrames(s.art, a)) add(f);
            continue;
        }
        if (type == kTextFrameArt) frames.push_back(a);
        if (Marked(s.art, *dict.operator&(), a)) {
            add(a);
            ai::int32 index = -1;
            if (type == kTextFrameArt && !s.text.GetStoryIndex(a, &index) && index >= 0) stories.insert(index);
        } else if (opening || madeSinceScan(a)) {
            // Opening: all of it; otherwise art made since the last scan, which the scan that
            // follows this pass would no longer take for new (the second review of 2026-11-01).
            fresh.push_back(a);
        }
    }
    memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches));
    for (AIArtHandle f : frames) {
        ai::int32 index = -1;
        if (!s.text.GetStoryIndex(f, &index) && stories.count(index)) add(f);
    }
    size_t made = 0;
    // Hand changes first (AutoCorrect, below, widens new art to stories with marked frames), and
    // released, locked, or hidden items kept out (the second review of 2026-11-01).
    items = ReleaseOverridden(message.basic, s, items);
    if (!fresh.empty()) made += AutoCorrect(message, state, s, fresh, 20000);
    if (items.empty()) return made;
    if (items.size() > 20000) { ++state.refix.skipped; return made; }
    std::vector<AIArtHandle> selection;
    AIArtHandle** chosen = nullptr;
    ai::int32 n = 0;
    if (!matching->GetSelectedArt(&chosen, &n) && chosen) {
        for (ai::int32 i = 0; i < n; ++i) selection.push_back((*chosen)[i]);
        memory->MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(chosen));
    }
    auto correct = [&](const std::vector<AIArtHandle>& list) {
        matching->DeselectAll();
        for (AIArtHandle a : list) if (s.art.ValidArt(a, true)) s.art.SetArtUserAttr(a, kArtSelected, kArtSelected);
        if (!CorrectSelection(message, state)) made += list.size();
    };
    // Across other text objects: each set of stories whose frames overlap (visible bounds, strokes
    // included), not yet in a story group, corrected on its own, so it is grouped as Correct
    // Selection groups the stories selected together; the switch turned off, then on again, left
    // them apart (the owner's retest, 2026-10-31).
    const auto& sw = Switches();
    if (sw.textBehind && sw.textStory && sw.textStories) {
        std::map<ai::int32, std::vector<AIArtHandle>> byStory;
        std::map<ai::int32, AIRealRect> box;
        for (AIArtHandle a : items) {
            short type = kUnknownArt;
            AIArtHandle up = nullptr;
            short upType = kUnknownArt;
            ai::int32 index = -1;
            AIRealRect r{};
            if (s.art.GetArtType(a, &type) || type != kTextFrameArt) continue;
            if (!s.art.GetArtParent(a, &up) && up && !s.art.GetArtType(up, &upType) && upType == kGroupArt &&
                ReadRouteRecord(s.art, *dict.operator&(), up) != kRouteNone) continue;
            if (s.text.GetStoryIndex(a, &index) || index < 0 || s.art.GetArtBounds(a, &r)) continue;
            auto& b = box.emplace(index, r).first->second;
            b.left = (std::min)(b.left, r.left); b.right = (std::max)(b.right, r.right);
            b.bottom = (std::min)(b.bottom, r.bottom); b.top = (std::max)(b.top, r.top);
            byStory[index].push_back(a);
        }
        std::vector<ai::int32> ids;
        for (const auto& kv : byStory) ids.push_back(kv.first);
        std::map<ai::int32, ai::int32> root;
        for (ai::int32 id : ids) root[id] = id;
        auto find = [&](ai::int32 id) { while (root[id] != id) id = root[id] = root[root[id]]; return id; };
        auto overlap = [](const AIRealRect& a, const AIRealRect& b) {
            return (std::min)(a.right, b.right) > (std::max)(a.left, b.left) && (std::min)(a.top, b.top) > (std::max)(a.bottom, b.bottom);
        };
        for (size_t i = 0; i < ids.size(); ++i)
            for (size_t j = i + 1; j < ids.size(); ++j)
                if (overlap(box[ids[i]], box[ids[j]])) root[find(ids[i])] = find(ids[j]);
        std::map<ai::int32, std::vector<ai::int32>> sets;
        for (ai::int32 id : ids) sets[find(id)].push_back(id);
        std::set<AIArtHandle> done;
        for (const auto& kv : sets) {
            if (kv.second.size() < 2) continue;
            std::vector<AIArtHandle> list;
            for (ai::int32 id : kv.second) for (AIArtHandle f : byStory[id]) { list.push_back(f); done.insert(f); }
            correct(list);
        }
        if (!done.empty()) {
            std::vector<AIArtHandle> rest;
            for (AIArtHandle a : items) if (!done.count(a)) rest.push_back(a);
            items.swap(rest);
        }
    }
    {
        struct NoSeparate { NoSeparate() { gNoSeparate = true; } ~NoSeparate() { gNoSeparate = false; } } noSeparate;
        if (!items.empty()) correct(items);
    }
    matching->DeselectAll();
    for (AIArtHandle a : selection) if (s.art.ValidArt(a, true)) s.art.SetArtUserAttr(a, kArtSelected, kArtSelected);
    return made;
}

// The timer's re-fix: marked art whose stamp changed since it was last seen is fixed again, in
// the undo step of the edit that changed it (appended). Art seen for the first time is taken as
// fixed. Text frames wait while the user types (a story has the text focus), so a re-fix never
// lands inside a typing session; nothing is done while redo steps wait (it would discard them).
// An item whose managed attributes the user changed by hand is unmarked and left alone.
ASErr Refix(SPMessageData& message, State& state) {
    auto& r = state.refix;
    // StrokeMaster switch or Follow edits off: edits are not followed, and the table is dropped,
    // so on its return nothing edited meanwhile is taken for due (it is first seen again). So
    // while paused by the bridge's "refix off".
    auto memo = [&](AIDocumentHandle d) -> RefixState::DocMemo* {
        for (int i = 0; i < r.docCount; ++i) if (r.docs[i].doc == d) return &r.docs[i];
        return nullptr;
    };
    // The active document's state into its memo (the oldest dropped when they are all taken).
    auto keep = [&]() {
        if (!r.document) return;
        RefixState::DocMemo* m = memo(r.document);
        if (!m) {
            if (r.docCount == 32) { std::copy(r.docs + 1, r.docs + 32, r.docs); --r.docCount; }
            m = &r.docs[r.docCount++];
        }
        *m = {r.document, r.global, r.applied, false};
    };
    // The memos are kept, each document's next scan its first sight, and its corrections taken
    // as following the switches as they are then: not an opening, and no switch change.
    if (!Switches().enabled || !Switches().followEdits || state.refixPaused) {
        keep();
        // Only a document already following the switches adopts them; one with a switch change
        // still due keeps it for its return (it was dropped: the review of 2026-11-01).
        for (int i = 0; i < r.docCount; ++i) {
            r.docs[i].global = 0;
            if (Corrections(r.docs[i].applied) == Corrections(Switches())) r.docs[i].adopt = true;
        }
        r.document = nullptr; r.count = 0; r.opening = false; r.paused = true;
        return kNoErr;
    }
    // Back from a pause: the documents that adopted the switches take them as they are now, once;
    // a switch changed later still reaches them when they are visited (adopt was kept, so every
    // later change was taken as applied: the second review of 2026-11-01).
    if (r.paused) {
        r.paused = false;
        for (int i = 0; i < r.docCount; ++i)
            if (r.docs[i].adopt) { r.docs[i].applied = Switches(); r.docs[i].adopt = false; }
    }
    Suite<AIDocumentSuite> document(message.basic, kAIDocumentSuite, kAIDocumentVersion);
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    if (document.error || art.error) return kNoErr;
    AIDocumentHandle doc = nullptr;
    if (document->GetDocument(&doc) || !doc) { keep(); r.document = nullptr; r.count = 0; r.opening = false; return kNoErr; }
    const size_t global = art->GetGlobalTimeStamp();
    AIBoolean typing = false;
    if (document->HasTextFocus(&typing)) typing = false;
    if (doc != r.document) {
        // Documents closed since drop out, before keep() might evict an open one's memo.
        Suite<AIDocumentListSuite> list(message.basic, kAIDocumentListSuite, kAIDocumentListVersion);
        // The document left is kept only when still open (a closed one's memo was added back: the
        // second review of 2026-11-01).
        bool leftOpen = true;
        if (!list.error) {
            ai::int32 open = 0;
            if (list->Count(&open)) open = 0;
            int kept = 0;
            for (int i = 0; i < r.docCount; ++i) {
                bool still = false;
                for (ai::int32 j = 0; j < open && !still; ++j) { AIDocumentHandle d = nullptr; still = !list->GetNthDocument(&d, j) && d == r.docs[i].doc; }
                if (still) r.docs[kept++] = r.docs[i];
            }
            r.docCount = kept;
            leftOpen = false;
            for (ai::int32 j = 0; j < open && !leftOpen; ++j) { AIDocumentHandle d = nullptr; leftOpen = !list->GetNthDocument(&d, j) && d == r.document; }
        }
        if (leftOpen) keep();
        r.count = 0; r.waiting = false; ++r.docSwitches;
        if (const RefixState::DocMemo* m = memo(doc)) {
            r.global = m->global;
            r.applied = m->adopt ? Switches() : m->applied;
            r.opening = false;
        } else {
            // First seen: opened (or open as the plugin loaded).
            r.global = 0;
            r.applied = Switches();
            r.opening = Switches().openDocuments;
        }
        r.document = doc;
    }
    // While the user types, only the story being typed changes: nothing is scanned until the
    // typing ends (the text focus goes), then the changed frames are fixed.
    if (typing) {
        ++r.typingWaits;
        if (global != r.global) { r.waiting = true; ++r.deferred; }
        // Correct Selection pressed while typing: its changes are seen as edits once the typing
        // ends, re-fixed in that edit's undo step, rather than the flag swallowing every edit made
        // meanwhile (the review of 2026-11-01).
        state.refixRefreshOnly = false;
        return kNoErr;
    }
    const bool switched = !(Corrections(r.applied) == Corrections(Switches()));
    // Nothing changed: a refresh-only flag left by a Correct Selection that changed nothing goes too
    // (it swallowed the next edit: the second review of 2026-11-01).
    if (global == r.global && !r.waiting && !switched && !r.opening) { state.refixRefreshOnly = false; return kNoErr; }
    Suite<AIUndoSuite> undo(message.basic, kAIUndoSuite, kAIUndoVersion);
    if (undo.error) return kNoErr;
    // While redo steps wait (the user is undoing), every state restored was fixed when it was made,
    // and a change now would discard the redo steps: the stamps are refreshed, nothing is fixed.
    bool undoing = false;
    {
        ai::int32 past = 0, future = 0;
        undoing = !undo->CountTransactions(&past, &future) && future > 0;
    }
    // A switch change waiting for the redo steps to go, and nothing else changed: no rescan at
    // every tick meanwhile (the second review of 2026-11-01).
    if (switched && undoing && global == r.global && !r.waiting && !r.opening) return kNoErr;
    if (state.refixRefreshOnly) { undoing = true; state.refixRefreshOnly = false; }
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
    // A switch changed, or the document has just opened with the opt-in: its corrected art follows
    // the switches, as an undo step of its own; then followed from here, as on first sight.
    if ((switched || r.opening) && !undoing) {
        undo->SetKind(kAIStandardUndoContext);
        if (EnsureUnicode(message.basic))
            undo->SetUndoTextUS(ai::UnicodeString("Undo StrokeMaster Switches"), ai::UnicodeString("Redo StrokeMaster Switches"));
        const size_t made = Reapply(message, state, s, r.opening);
        (r.opening ? r.opened : r.reapplied) += made;
        r.opening = false;
        r.applied = Switches();
        document->RedrawDocument();
        ++r.redraws;
        r.count = 0;
        r.waiting = false;
        r.global = art->GetGlobalTimeStamp();
        return kNoErr;
    }
    AIMatchingArtSpec specs[4] = {AIMatchingArtSpec(kPathArt, 0, 0), AIMatchingArtSpec(kCompoundPathArt, 0, 0),
                                  AIMatchingArtSpec(kTextFrameArt, 0, 0), AIMatchingArtSpec(kGroupArt, 0, 0)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    if (matching->GetMatchingArt(specs, 4, &matches, &count) || !matches) { r.global = global; return kNoErr; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    auto find = [&](AIArtHandle a) -> const RefixState::Seen* {
        const auto end = r.seen + r.count;
        const auto it = std::lower_bound(r.seen, end, a, [](const RefixState::Seen& x, AIArtHandle k) { return std::less<AIArtHandle>()(x.art, k); });
        return it != end && it->art == a ? it : nullptr;
    };
    auto stampOf = [&](AIArtHandle a) {
        size_t v = 0;
        art->GetArtTimeStamp(a, kAITimeStampMaxFromArtAndChildren, &v);
        return v;
    };
    // The table is rebuilt from this scan: art no longer in the document drops out.
    std::vector<RefixState::Seen> next;
    std::vector<AIArtHandle> due, fresh, regroup;
    auto addDue = [&](AIArtHandle a) { if (std::find(due.begin(), due.end(), a) == due.end()) due.push_back(a); };
    // Frames per story, read once a scan needs them.
    std::map<ai::int32, int> storyFrames;
    bool storiesRead = false;
    auto threaded = [&](AIArtHandle a) {
        if (!storiesRead) {
            storiesRead = true;
            for (ai::int32 i = 0; i < count; ++i) {
                short t2 = kUnknownArt;
                ai::int32 index = -1;
                if (!art->GetArtType((*matches)[i], &t2) && t2 == kTextFrameArt && !text->GetStoryIndex((*matches)[i], &index) && index >= 0)
                    ++storyFrames[index];
            }
        }
        ai::int32 index = -1;
        return !text->GetStoryIndex(a, &index) && index >= 0 && storyFrames[index] > 1;
    };
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i], parent = nullptr;
        short ptype = kUnknownArt;
        if (!art->GetArtParent(a, &parent) && parent && !art->GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        // A frame of a routed story group is followed for its group: an edit of its characters
        // does not move the group's own stamp (the story's recolor went unseen, 2026-10-30).
        short type = kUnknownArt;
        const bool storyFrame = !art->GetArtType(a, &type) && type == kTextFrameArt && parent && ptype == kGroupArt &&
                                ReadRouteRecord(*art.operator&(), *dict.operator&(), parent) != kRouteNone;
        const bool marked = Marked(*art.operator&(), *dict.operator&(), a);
        if (!marked && !storyFrame && ReadRouteRecord(*art.operator&(), *dict.operator&(), a) == kRouteNone) {
            // Never corrected: new art, or art edited since the last scan, is corrected
            // automatically, as InDesign corrects whatever it draws (the owner, 2026-10-30). The
            // first scan of a document sees nothing as new, so opening one changes nothing in it
            // (Also documents as they open is Reapply's).
            if (!undoing && type != kGroupArt && r.global != 0 && stampOf(a) > r.global) fresh.push_back(a);
            continue;
        }
        const size_t stamp = stampOf(a);
        const RefixState::Seen* seen = find(a);
        // First sight (just fixed, opened, pasted, duplicated): taken as fixed, and followed from
        // here, so opening a document changes nothing in it.
        if (!seen || seen->stamp == stamp || undoing) { next.push_back({a, stamp}); continue; }
        if (storyFrame) {
            addDue(parent);
            if (!marked) { next.push_back({a, stamp}); continue; }
        }
        // A frame of threaded text corrected on its own (before it was threaded, or before
        // threading was grouped): grouped with its story's other frames, as Correct Selection
        // does it (the owner's retest T1, 2026-10-31: only the button grouped them).
        if (!storyFrame && type == kTextFrameArt && Switches().textBehind && Switches().textStory && threaded(a)) {
            regroup.push_back(a);
            next.push_back({a, stamp});
            continue;
        }
        addDue(a);
    }
    if (!due.empty() || !fresh.empty() || !regroup.empty()) undo->SetKind(kAIAppendUndoContext);
    if (!fresh.empty()) {
        const size_t made = AutoCorrect(message, state, s, fresh);
        r.automatic += made;
        if (made) for (AIArtHandle a : fresh) next.push_back({a, stampOf(a)});
    }
    if (!regroup.empty()) {
        const size_t made = AutoCorrect(message, state, s, regroup);
        r.regrouped += made;
        for (auto& x : next) if (art->ValidArt(x.art, true)) x.stamp = stampOf(x.art);
    }
    // AutoCorrect may have taken apart and disposed of a story group that is due (a frame threaded
    // onto its story): only art that still exists is fixed (the review of 2026-11-01).
    due.erase(std::remove_if(due.begin(), due.end(), [&](AIArtHandle a) { return !art->ValidArt(a, true); }), due.end());
    if (!due.empty()) {
        NativeFixStats st{};
        std::vector<AIArtHandle> fixed;
        const ASErr err = FixItems(message.basic, s, due, st, true, &fixed);
        r.lastError = static_cast<int>(err);
        ++r.runs;
        r.items += fixed.size();
        r.released += static_cast<unsigned long long>(st.released);
        r.rerouted += static_cast<unsigned long long>(st.rerouted);
        r.routeReleased += static_cast<unsigned long long>(st.routeReleased);
        // A corrected dashed path the user has just set Outside (the Stroke panel) gets the opt-in
        // for dashed Outside strokes, as Correct Selection gives it, in the same undo step:
        // Illustrator's own Outside dashes leave the corners empty (the owner's retest,
        // 2026-10-30: Inside, corrected, then Outside). The fix then judges it again.
        r.dashed += RefixDashes(message.basic, s, fixed);
        // Every due item is followed from its new stamp: fixed, released (no longer marked: it
        // drops out next scan), or failed (looked at again on its next change).
        for (AIArtHandle a : due)
            if (std::none_of(next.begin(), next.end(), [&](const RefixState::Seen& x) { return x.art == a; })) next.push_back({a, stampOf(a)});
        // A story group's frames are followed from their stamps after the group's re-fix too.
        for (auto& x : next) x.stamp = stampOf(x.art);
    }
    // Illustrator redraws after a plugin's own command, not after a timer: an automatic
    // correction showed only once the object was moved (the owner's retest Q1, 2026-10-31).
    if (!due.empty() || !fresh.empty() || !regroup.empty()) { document->RedrawDocument(); ++r.redraws; }
    r.count = 0;
    for (const auto& e : next) { if (r.count >= RefixState::kSeen) break; r.seen[r.count++] = e; }
    if (next.size() > static_cast<size_t>(RefixState::kSeen)) ++r.overflow;
    std::sort(r.seen, r.seen + r.count, [](const RefixState::Seen& x, const RefixState::Seen& y) { return std::less<AIArtHandle>()(x.art, y.art); });
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

// Research (2026-10-30): one live effect, by name, at the head of the route's own paint field
// on each selected routed frame and story group (input "outline-object", "outline-stroke",
// "outline-both", or an
// effect name of at most 16 characters; "-" removes the ones (and the route's default pass)
// this command added). The aim: a route pass written as paths (Outline Object) instead of a
// second copy of the text, so a PDF's text is extracted once. Counts: styled = targets changed.
ASErr RoutePass(SPMessageData& message, State& state, const char* name) {
    auto& st = state.appearance;
    st = AppearanceStats{};
    st.fieldsBefore = st.fieldsAfter = st.contentsBefore = st.contentsAfter = 0;
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AIDictionarySuite> dict(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    Suite<AIATEPaintSuite> atePaint(message.basic, kAIATEPaintSuite, kAIATEPaintVersion);
    for (ASErr e : {art.error, matching.error, memory.error, styles.error, parser.error, dict.error, live.error, text.error,
                    range.error, features.error, atePaint.error})
        if (e) { st.gate = "pass.suites"; return e; }
    const bool remove = std::strcmp(name, "-") == 0;
    // Bridge input is at most 16 characters: short names for Illustrator's own effects.
    // "outline-both": Outline Object, then Outline Stroke, so the pass is filled shapes Illustrator
    // computed (a viewer then strokes nothing itself).
    std::vector<const char*> names;
    if (std::strcmp(name, "outline-object") == 0) names = {"Adobe Outline Type"};
    else if (std::strcmp(name, "outline-stroke") == 0) names = {"Adobe Outline Stroke"};
    else if (std::strcmp(name, "outline-both") == 0) names = {"Adobe Outline Type", "Adobe Outline Stroke"};
    else if (!remove) names = {name};
    std::vector<AILiveEffectHandle> effects;
    for (const char* n : names) {
        AILiveEffectHandle h = nullptr;
        if (live->GetLiveEffectHandleByName(n, &h) || !h) { st.gate = "pass.no-effect"; return kBadParameterErr; }
        effects.push_back(h);
    }
    AIMatchingArtSpec specs[2] = {AIMatchingArtSpec(kTextFrameArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kGroupArt, kArtSelected, kArtSelected)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    if (matching->GetMatchingArt(specs, 2, &matches, &count)) { st.gate = "pass.match"; return kBadParameterErr; }
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtHandle a = (*matches)[i];
        std::string sig;
        const RouteKind kind = ReadRouteRecord(*art.operator&(), *dict.operator&(), a, &sig);
        if (kind != kRouteFill && kind != kRouteStroke) continue;
        ++st.frames;
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        if (styles->GetArtStyle(a, &style) || !style || parser->NewParser(&p) || !p) continue;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{*parser.operator&(), p};
        if (parser->ParseStyle(p, style)) continue;
        const RouteSuites rs{message.basic, *art.operator&(), *styles.operator&(), *parser.operator&(), *text.operator&(),
                             *range.operator&(), *features.operator&(), *atePaint.operator&(), *dict.operator&()};
        AIParserPaintField field = FindRouteField(rs, p, kind, sig);
        if (!field) { ++st.contentsBefore; continue; }
        bool changed = false;
        for (ai::int32 k = parser->CountEffectsOfPaintField(field) - 1; k >= 0; --k) {
            AIParserLiveEffect fx = nullptr;
            AILiveEffectParameters fxParams = nullptr;
            ai::int32 tag = 0;
            ai::int32 pass = 0;
            if (!parser->GetNthEffectOfPaintField(field, k, &fx) && fx && !parser->GetLiveEffectParams(fx, &fxParams) && fxParams &&
                ((!dict->GetIntegerEntry(fxParams, dict->Key(kOwnedKey), &tag) && tag == 1) ||
                 (!dict->GetIntegerEntry(fxParams, dict->Key(kRoutePassKey), &pass) && pass == 1)) &&
                !parser->RemoveEffectOfPaintField(p, field, fx, true))
                changed = true;
        }
        for (size_t k = 0; k < effects.size(); ++k) {
            AILiveEffectParameters params = nullptr;
            if (live->CreateLiveEffectParameters(&params) || !params) break;
            // As Effect > Path > Outline Object writes it (read back 2026-10-30), plus the tag.
            dict->SetBooleanEntry(params, dict->Key("-DefaultApplyEffectsKey"), false);
            dict->SetIntegerEntry(params, dict->Key(kOwnedKey), 1);
            AIParserLiveEffect fx = nullptr;
            const auto e = parser->NewParserLiveEffect(effects[k], params, &fx);
            dict->Release(params);
            if (e || !fx) break;
            if (parser->InsertNthEffectOfPaintField(p, field, static_cast<ai::int32>(k), fx)) { parser->DisposeParserLiveEffect(fx); break; }
            changed = true;
        }
        if (!changed) continue;
        AIArtStyleHandle created = nullptr;
        if (parser->CreateNewStyle(p, &created) || !created || styles->SetArtStyle(a, created)) continue;
        ++st.styled;
    }
    st.gate = remove ? "pass.removed" : "pass.added";
    return kNoErr;
}

// Research (2026-10-30): every live effect Illustrator has, "name|title" per line.
ASErr EffectsList(SPMessageData& message, std::string& out) {
    out.clear();
    Suite<AILiveEffectSuite> live(message.basic, kAILiveEffectSuite, kAILiveEffectVersion);
    if (live.error) return live.error;
    ai::int32 n = 0;
    if (auto e = live->CountLiveEffects(&n)) return e;
    for (ai::int32 i = 0; i < n; ++i) {
        AILiveEffectHandle h = nullptr;
        const char* name = nullptr;
        const char* title = nullptr;
        if (live->GetNthLiveEffect(i, &h) || !h) continue;
        live->GetLiveEffectName(h, &name);
        live->GetLiveEffectTitle(h, &title);
        out += name ? name : "?"; out += "|"; out += title ? title : "?"; out += "\n";
    }
    return kNoErr;
}

// `parent`: the selected item's group instead (a story group's route stroke), when it has one.
ASErr StyleDump(SPMessageData& message, std::string& out, bool group = false) {
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
    if (group) {
        AIArtHandle up = nullptr;
        short utype = kUnknownArt;
        if (!art->GetArtParent(target, &up) && up && !art->GetArtType(up, &utype) && utype == kGroupArt) target = up;
    }
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
        AIBoolean visible = true;
        parser->GetPaintFieldVisible(f, &visible);
        if (parser->IsStroke(f)) {
            AIStrokeStyle stroke{};
            AIArtStylePaintData data;
            if (!parser->GetStroke(f, &stroke, &data)) {
                // The color's kind (kNoneColor paints nothing) and whether the field is shown.
                std::snprintf(buf, sizeof buf, "w %.3g join %d limit %.4g color %d vis %d ", static_cast<double>(stroke.width),
                              static_cast<int>(stroke.join), static_cast<double>(stroke.miterLimit),
                              static_cast<int>(stroke.color.kind), visible ? 1 : 0);
                out += buf;
            }
        } else if (parser->IsFill(f)) {
            AIFillStyle fill{};
            AIArtStylePaintData data;
            if (!parser->GetFill(f, &fill, &data)) {
                std::snprintf(buf, sizeof buf, "color %d vis %d ", static_cast<int>(fill.color.kind), visible ? 1 : 0);
                out += buf;
            }
        }
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
    // What StrokeMaster has recorded on the item (tests and diagnosis).
    {
        FixRecord rec;
        const bool marked = Marked(*art.operator&(), *dict.operator&(), target, &rec);
        auto set = [](const std::set<long>& v) { std::string o; for (long x : v) { if (!o.empty()) o += ','; o += std::to_string(x); } return o; };
        std::string before;
        AIDictionaryRef d = nullptr;
        if (art->HasDictionary(target) && !art->GetDictionary(target, &d) && d) {
            const char* text = nullptr;
            if (!dict->GetStringEntry(d, dict->Key(kRecordOrderBefore), &text) && text) before = text;
            dict->Release(d);
        }
        char rb[256];
        std::snprintf(rb, sizeof rb, "RECORD marked %d released %d route %d limit %.4g user %.4g fieldUser %.4g ", marked ? 1 : 0,
                      Released(*art.operator&(), *dict.operator&(), target) ? 1 : 0,
                      static_cast<int>(ReadRouteRecord(*art.operator&(), *dict.operator&(), target)), rec.hasLimit ? rec.limit : -1,
                      rec.userLimit, rec.fieldUser);
        out += rb;
        out += "fieldLimits [" + set(rec.fieldLimits) + "] charLimits [" + set(rec.charLimits) + "] order '" + rec.order +
               "' before '" + before + "'; ";
    }
    return kNoErr;
}

// Tests (bridge "field-limit", input the limit): every stroke that paints in the selected items'
// Appearance set to that miter limit, as a hand sets it in the Stroke panel with one targeted.
ASErr FieldLimit(SPMessageData& message, const char* input) {
    const double limit = std::atof(input);
    if (!(limit >= 1 && limit <= 500)) return kBadParameterErr;
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    for (ASErr e : {styles.error, parser.error, matching.error, memory.error}) if (e) return e;
    AIMatchingArtSpec specs[3] = {AIMatchingArtSpec(kPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kCompoundPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kTextFrameArt, kArtSelected, kArtSelected)};
    AIArtHandle** matches = nullptr;
    ai::int32 count = 0;
    if (matching->GetMatchingArt(specs, 3, &matches, &count) || !matches) return kBadParameterErr;
    struct Release {
        const AIMdMemorySuite& memory; AIArtHandle** matches;
        ~Release() { if (matches) memory.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(matches)); }
    } release{*memory.operator&(), matches};
    for (ai::int32 i = 0; i < count; ++i) {
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        if (styles->GetArtStyle((*matches)[i], &style) || !style || parser->NewParser(&p) || !p) continue;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{*parser.operator&(), p};
        if (parser->ParseStyle(p, style)) continue;
        for (ai::int32 k = 0, n = parser->CountPaintFields(p); k < n; ++k) {
            AIParserPaintField f = nullptr;
            AIStrokeStyle stroke{};
            AIArtStylePaintData data;
            if (parser->GetNthPaintField(p, k, &f) || !f || !PaintingStroke(*parser.operator&(), f) || parser->GetStroke(f, &stroke, &data)) continue;
            stroke.miterLimit = static_cast<AIReal>(limit);
            parser->SetStroke(f, &stroke, &data);
        }
        AIArtStyleHandle created = nullptr;
        if (!parser->CreateNewStyle(p, &created) && created) styles->SetArtStyle((*matches)[i], created);
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
//       - a compound path with an open contour (Illustrator drops alignment for all its contours):
//         Outside, Center at twice the weight under its fill; Inside, two fills (AddOffsetFills)
//         where each open contour is open by no more than the weight and BandRefusal allows;
//       - type (Illustrator stores but never draws a type object's alignment): Outside, with one
//         stroke, an object-level stroke of twice the weight below Characters, the characters'
//         strokes cleared, and with mixed strokes, every character's weight doubled and its
//         stroke painted before its own fill (FillFirst off); Inside, with one fill and one
//         stroke, two fills above Characters (AddOffsetFills).
//   "dashes"  the opt-in for dashed Outside strokes, which Illustrator cannot fit to corners:
//       Center at twice the weight under the fill, then Align Dashes to Corners.
//   "restore"  what native-align made of the selected items, put back as it was.
// The Center-under-fill forms need a simple style (one painting fill, one stroke): the fill hides
// the inner half. What each item became is kept in its dictionary (AlignRecord).
struct NativeAlignStats {
    int native = 0, profiled = 0, doubled = 0, typeOutside = 0, restored = 0, refused = 0, offset = 0;
    std::string reasons;
};

// Inside drawn with two fills (2026-10-16), where Illustrator draws no Inside alignment (a
// compound path with an open contour, type): a fill of the stroke's color beneath, and the item's
// own fill on top, shrunk by the stroke weight with Illustrator's live Effect > Path > Offset Path
// (offset -w, the stroke's join and miter limit; parameters as Illustrator writes them). What shows
// between them is the Inside band, and both follow every edit of the outline. The stroke itself is
// kept in the style, hidden, so restore can bring it back as it was. Offset Path joins: 0 round,
// 1 bevel, 2 miter.
int OffsetJoin(AILineJoin join) { return join == kAIRoundJoin ? 0 : (join == kAIBevelJoin ? 1 : 2); }

// Why a stroke cannot be drawn as the band of two fills, or nullptr. The band is one flat fill in
// the stroke's color, so dashes, a gradient or pattern along the stroke, effects of the stroke's
// own (Roughen, Zig Zag), a brush, a width profile, and arrowheads would all be lost: refused.
// `art` null (type, whose character strokes carry none of these, its object-level effects refused
// before) checks the stroke style only.
const char* BandRefusal(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, const AIStrokeStyle& stroke) {
    if (stroke.dash.length > 0) return "inside-dashed";
    if (stroke.color.kind == kGradient || stroke.color.kind == kPattern) return "inside-stroke-paint";
    if (!art) return nullptr;
    {
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        if (s.styles.GetArtStyle(art, &style) || !style || s.parser.NewParser(&p) || !p) return "inside-stroke-params";
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
        if (s.parser.ParseStyle(p, style)) return "inside-stroke-params";
        for (ai::int32 i = 0; i < s.parser.CountPaintFields(p); ++i) {
            AIParserPaintField f = nullptr;
            if (!s.parser.GetNthPaintField(p, i, &f) && f && s.parser.IsStroke(f) && s.parser.CountEffectsOfPaintField(f) > 0)
                return "inside-stroke-effects";
        }
    }
    Suite<AIBeautifulStrokesSuite> bs(basic, kAIBeautifulStrokesSuite, kAIBeautifulStrokesVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (bs.error || dict.error) return "inside-stroke-params";
    AIDictionaryRef params = nullptr;
    AIBoolean brush = false;
    const AIErr got = bs->GetArtStrokeParams(art, params, nullptr, &brush, nullptr, nullptr);
    struct Release { const AIDictionarySuite& d; AIDictionaryRef p; ~Release() { if (p) d.Release(p); } } release{*dict.operator&(), params};
    if (got) return "inside-stroke-params";
    if (brush) return "inside-brush";
    // The getters give their defaults (constant width, no arrowheads) on a dictionary without them.
    if (!params) return nullptr;
    if (bs->GetWidthSource(params) != kConstantWidth) return "inside-width-profile";
    AIPatternHandle begin = nullptr, end = nullptr;
    AIReal beginScale = 0, endScale = 0;
    ArrowTipAlignment tip{};
    AIBoolean mixedBegin = false, mixedEnd = false;
    if (bs->GetArrowheadOptions(params, &begin, &end, &beginScale, &endScale, &tip, &mixedBegin, &mixedEnd)) return "inside-stroke-params";
    if (begin || end) return "inside-arrowheads";
    return nullptr;
}

// The two fills, inserted at the top of the style (above Characters for type).
ASErr AddOffsetFills(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, const AIFillStyle& fill, bool evenodd,
                     const AIStrokeStyle& stroke) {
    Suite<AILiveEffectSuite> live(basic, kAILiveEffectSuite, kAILiveEffectVersion);
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (live.error || dict.error) return kBadParameterErr;
    AILiveEffectHandle offset = nullptr;
    if (live->GetLiveEffectHandleByName("Adobe Offset Path", &offset) || !offset) return kBadParameterErr;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e ? e : kBadParameterErr;
    AILiveEffectParameters params = nullptr;
    if (live->CreateLiveEffectParameters(&params) || !params) return kOutOfMemoryErr;
    struct ReleaseParams { const AIDictionarySuite& d; AILiveEffectParameters p; ~ReleaseParams() { d.Release(p); } }
        releaseParams{*dict.operator&(), params};
    dict->SetRealEntry(params, dict->Key("ofst"), static_cast<AIReal>(-stroke.width));
    dict->SetIntegerEntry(params, dict->Key("jntp"), OffsetJoin(stroke.join));
    dict->SetRealEntry(params, dict->Key("mlim"), stroke.miterLimit);
    dict->SetIntegerEntry(params, dict->Key(kOwnedKey), 1);   // the fix's own: a user's Offset Path fills are left alone
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    AIFillStyle band{};
    band.color = stroke.color;
    band.overprint = stroke.overprint;
    AIParserPaintField under = nullptr, top = nullptr;
    if ((e = s.parser.NewPaintFieldFill(&band, evenodd, nullptr, &under)) != kNoErr || !under) return e ? e : kOutOfMemoryErr;
    if ((e = s.parser.InsertNthPaintField(p, 0, under)) != kNoErr) { s.parser.DisposePaintField(under); return e; }
    if ((e = s.parser.NewPaintFieldFill(&fill, evenodd, nullptr, &top)) != kNoErr || !top) return e ? e : kOutOfMemoryErr;
    AIParserLiveEffect fx = nullptr;
    if ((e = s.parser.NewParserLiveEffect(offset, params, &fx)) != kNoErr || !fx) { s.parser.DisposePaintField(top); return e ? e : kOutOfMemoryErr; }
    if ((e = s.parser.InsertNthPaintField(p, 0, top)) != kNoErr) { s.parser.DisposeParserLiveEffect(fx); s.parser.DisposePaintField(top); return e; }
    if ((e = s.parser.InsertNthEffectOfPaintField(p, top, 0, fx)) != kNoErr) { s.parser.DisposeParserLiveEffect(fx); return e; }
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    return s.styles.SetArtStyle(art, created);
}

// Shows or hides the first stroke field that is `fromVisible` (on a fresh parse, as the SDK asks).
ASErr SetStrokeFieldVisible(const FixSuites& s, AIArtHandle art, bool fromVisible, bool visible, bool lastOne = false) {
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e ? e : kBadParameterErr;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    AIParserPaintField target = nullptr;
    const ai::int32 n = s.parser.CountPaintFields(p);
    for (ai::int32 k = 0; k < n; ++k) {
        const ai::int32 i = lastOne ? n - 1 - k : k;
        AIParserPaintField f = nullptr;
        AIBoolean vis = true;
        if (s.parser.GetNthPaintField(p, i, &f) || !f || !s.parser.IsStroke(f) || s.parser.GetPaintFieldVisible(f, &vis)) continue;
        if ((vis != 0) == fromVisible) { target = f; break; }
    }
    if (!target) return kBadParameterErr;
    if ((e = s.parser.SetParserPaintFieldVisible(target, visible)) != kNoErr) return e;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    return s.styles.SetArtStyle(art, created);
}

// The fix's own shrunk fill in a parsed style: a fill whose one effect is Adobe Offset Path tagged
// kOwnedKey, with `under` the plain fill right below it (the band) when there is one, and `inset`
// the effect's own offset (its magnitude: the weight as the effect has it now, after any Scale
// Strokes & Effects or a change in the Appearance panel).
bool FindOwnOffsetFill(SPBasicSuite* basic, const FixSuites& s, AIStyleParser p, AIParserPaintField& top,
                       AIParserPaintField* under, double* inset) {
    top = nullptr;
    if (under) *under = nullptr;
    Suite<AIDictionarySuite> dict(basic, kAIDictionarySuite, kAIDictionaryVersion);
    if (dict.error) return false;
    const ai::int32 n = s.parser.CountPaintFields(p);
    for (ai::int32 i = 0; i < n; ++i) {
        AIParserPaintField f = nullptr;
        AIParserLiveEffect fx = nullptr;
        AILiveEffectParameters params = nullptr;   // the style's own: read only
        ai::int32 tag = 0;
        if (s.parser.GetNthPaintField(p, i, &f) || !f || !s.parser.IsFill(f) || s.parser.CountEffectsOfPaintField(f) != 1 ||
            s.parser.GetNthEffectOfPaintField(f, 0, &fx) || !EffectNamed(s.parser, fx, "Adobe Offset Path") ||
            s.parser.GetLiveEffectParams(fx, &params) || !params || dict->GetIntegerEntry(params, dict->Key(kOwnedKey), &tag) || tag != 1)
            continue;
        top = f;
        if (inset) {
            AIReal o = 0;
            *inset = dict->GetRealEntry(params, dict->Key("ofst"), &o) ? 0 : std::fabs(o);
        }
        AIParserPaintField b = nullptr;
        if (under && i + 1 < n && !s.parser.GetNthPaintField(p, i + 1, &b) && b && s.parser.IsFill(b) && s.parser.CountEffectsOfPaintField(b) == 0)
            *under = b;
        return true;
    }
    return false;
}

// Shows or hides the fix's shrunk fill; `changed` says whether it had to change. Hidden when the
// inset leaves nothing (native_region::InsetEmpty), since Offset Path then gives the shape back
// unshrunk and would cover the band.
ASErr SetOffsetFillVisible(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, bool visible, bool& changed) {
    changed = false;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e ? e : kBadParameterErr;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    AIParserPaintField f = nullptr;
    AIBoolean vis = true;
    if (!FindOwnOffsetFill(basic, s, p, f, nullptr, nullptr) || s.parser.GetPaintFieldVisible(f, &vis)) return kBadParameterErr;
    if ((vis != 0) == visible) return kNoErr;
    if ((e = s.parser.SetParserPaintFieldVisible(f, visible)) != kNoErr) return e;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    if ((e = s.styles.SetArtStyle(art, created)) != kNoErr) return e;
    changed = true;
    return kNoErr;
}

// The outline an Inside drawn as two fills follows: a path's contours, or type's glyph outlines;
// false when any part cannot be read.
bool OffsetOutline(const FixSuites& s, AIArtHandle art, std::vector<ise_miter::Contour>& out) {
    out.clear();
    short type = kUnknownArt;
    if (s.art.GetArtType(art, &type)) return false;
    if (type != kTextFrameArt) return Contours(s, art, out);
    AIArtHandle outline = nullptr;
    if (CopyOutline(s, art, &outline) || !outline) return false;
    struct DisposeArt { const AIArtSuite& api; AIArtHandle a; ~DisposeArt() { if (a) api.DisposeArt(a); } } dispose{s.art, outline};
    AIArtHandle child = nullptr;
    if (s.art.GetArtFirstChild(outline, &child)) return false;
    while (child) {
        if (!Contours(s, child, out)) return false;
        if (s.art.GetArtSibling(child, &child)) return false;
    }
    return !out.empty();
}

// Keeps an Inside drawn as two fills right after an edit: the shrunk fill shown where the inset
// leaves something, hidden where it leaves nothing. The inset is the effect's own; for type it is
// in the frame's own space, as the characters' weight is, and the outline in page space.
ASErr SyncOffset(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, double /*recorded*/, bool& changed) {
    changed = false;
    double inset = 0;
    {
        AIArtStyleHandle style = nullptr;
        AIStyleParser p = nullptr;
        if (s.styles.GetArtStyle(art, &style) || !style || s.parser.NewParser(&p) || !p) return kNoErr;
        AIParserPaintField f = nullptr;
        const bool found = !s.parser.ParseStyle(p, style) && FindOwnOffsetFill(basic, s, p, f, nullptr, &inset);
        s.parser.DisposeParser(p);
        if (!found || !(inset > 0)) return kNoErr;
    }
    std::vector<ise_miter::Contour> contours;
    if (!OffsetOutline(s, art, contours)) return kNoErr;
    short type = kUnknownArt;
    if (!s.art.GetArtType(art, &type) && type == kTextFrameArt) inset *= FrameScale(s, art);
    return SetOffsetFillVisible(basic, s, art, !native_region::InsetEmpty(contours, inset), changed);
}

// Removes the two fills AddOffsetFills made (the fix's tagged shrunk fill and the band right below
// it). `found` false leaves the style as it is.
ASErr RemoveOffsetFills(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, bool& found) {
    found = false;
    AIArtStyleHandle style = nullptr;
    auto e = s.styles.GetArtStyle(art, &style);
    if (e || !style) return e ? e : kBadParameterErr;
    AIStyleParser p = nullptr;
    e = s.parser.NewParser(&p);
    if (e || !p) return e ? e : kOutOfMemoryErr;
    struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
    if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
    AIParserPaintField top = nullptr, under = nullptr;
    if (!FindOwnOffsetFill(basic, s, p, top, &under, nullptr) || !under) return kNoErr;
    if ((e = s.parser.RemovePaintField(p, top, true)) != kNoErr) return e;
    if ((e = s.parser.RemovePaintField(p, under, true)) != kNoErr) return e;
    found = true;
    AIArtStyleHandle created = nullptr;
    if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
    return s.styles.SetArtStyle(art, created);
}

ASErr NativeAlignItem(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, const char* mode, NativeAlignStats& st);

// The doubled stroke of the dashed Outside opt-in put back, then the alignment the user has set.
bool Realign(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, ai::uint32 wanted) {
    NativeAlignStats st;
    if (NativeAlignItem(basic, s, art, "restore", st) || st.refused) return false;
    Suite<AIPaintStyleSuite> paintStyle(basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
    // The restore leaves it Outside.
    if (!paintStyle.error && wanted != kAIStrokeAlignmentOutside) paintStyle->SetStrokeAlignment(art, wanted);
    return true;
}

size_t RefixDashes(SPBasicSuite* basic, const FixSuites& s, const std::vector<AIArtHandle>& fixed) {
    const auto& sw = Switches();
    if (!(sw.wholePoints && sw.patternPoints && sw.outsideDashes)) return 0;
    std::vector<AIArtHandle> doubled;
    for (AIArtHandle a : fixed) {
        short type = kUnknownArt;
        if (s.art.GetArtType(a, &type) || (type != kPathArt && type != kCompoundPathArt)) continue;
        if (ReadAlignRecord(basic, s.art, a).dashes) continue;   // the opt-in's already
        NativeAlignStats st;
        if (NativeAlignItem(basic, s, a, "dashes", st)) continue;   // a no-op unless dashed and Outside
        if (ReadAlignRecord(basic, s.art, a).dashes) doubled.push_back(a);
    }
    if (!doubled.empty()) {
        NativeFixStats again{};
        FixItems(basic, s, doubled, again);
    }
    return doubled.size();
}

ASErr NativeAlignItem(SPBasicSuite* basic, const FixSuites& s, AIArtHandle art, const char* mode, NativeAlignStats& st) {
    auto refuse = [&](const char* why) {
        ++st.refused;
        const std::string token = std::string(why) + ' ';
        if (st.reasons.size() + token.size() <= 200 && (' ' + st.reasons).find(' ' + token) == std::string::npos) st.reasons += token;
        return kNoErr;
    };
    Suite<AIPaintStyleSuite> paintStyle(basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
    if (paintStyle.error) return paintStyle.error;
    short type = kUnknownArt;
    if (s.art.GetArtType(art, &type)) return refuse("type?");
    const bool outside = std::strcmp(mode, "outside") == 0, inside = std::strcmp(mode, "inside") == 0;
    // "rollback": restore after a failure part-way, which goes on where a step had not run yet.
    const bool rollback = std::strcmp(mode, "rollback") == 0;
    const bool dashes = std::strcmp(mode, "dashes") == 0, restore = rollback || std::strcmp(mode, "restore") == 0;
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
        if (!WriteAlignRecord(basic, s.art, art, AlignRecord{"doubled", fillWasOnTop ? 3 : 1, w})) return rollback(kBadParameterErr);
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
        } else if (ar.as == "offset" || ar.as == "type-offset") {
            bool found = false;
            auto e = RemoveOffsetFills(basic, s, art, found);
            if (e) return e;
            // Changed since (the shrunk fill or the band removed or reordered): left as it is.
            if (!found && !rollback) return refuse("restore-fills-changed");
            if (ar.as == "offset") {
                if ((e = SetStrokeFieldVisible(s, art, false, true)) != kNoErr) return refuse("restore-hidden-stroke");
            } else {
                // The hidden stroke field back onto the characters, then removed.
                AIArtStyleHandle style = nullptr;
                AIStyleParser p = nullptr;
                if ((e = s.styles.GetArtStyle(art, &style)) != kNoErr || !style || s.parser.NewParser(&p) || !p) return e ? e : kBadParameterErr;
                struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
                if ((e = s.parser.ParseStyle(p, style)) != kNoErr) return e;
                AIParserPaintField hidden = nullptr;
                AIStrokeStyle stroke{};
                AIArtStylePaintData data;
                for (ai::int32 k = s.parser.CountPaintFields(p) - 1; k >= 0 && !hidden; --k) {
                    AIParserPaintField f = nullptr;
                    AIBoolean vis = true;
                    if (!s.parser.GetNthPaintField(p, k, &f) && f && s.parser.IsStroke(f) && !s.parser.GetPaintFieldVisible(f, &vis) && !vis &&
                        !s.parser.GetStroke(f, &stroke, &data))
                        hidden = f;
                }
                if (!hidden) return refuse("restore-hidden-stroke");
                Suite<AIATEPaintSuite> atePaint(basic, kAIATEPaintSuite, kAIATEPaintVersion);
                if (atePaint.error) return atePaint.error;
                AIPathStyle ps{};
                ps.Init();
                ps.strokePaint = true;
                ps.stroke = stroke;
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
                if ((e = s.parser.RemovePaintField(p, hidden, true)) != kNoErr) return e;
                AIArtStyleHandle created = nullptr;
                if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
                if ((e = s.styles.SetArtStyle(art, created)) != kNoErr) return e;
            }
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
    // A dashed Outside stroke the opt-in drew as Center at twice the weight: Illustrator grays the
    // Stroke panel's Inside and Outside for it (its dashes are aligned to corners, which Illustrator
    // does on Center strokes only), so Align Selection Inside is the way to set it Inside (the
    // owner's V4, 2026-10-31). Outside it is already.
    if (ar.as == "doubled" && ar.dashes && (inside || outside)) {
        if (outside) return kNoErr;
        return Realign(basic, s, art, kAIStrokeAlignmentInside) ? kNoErr : refuse("realign");
    }
    if (!ar.as.empty()) return refuse("already-aligned");   // restore first

    if (type == kTextFrameArt) {
        if (dashes) return kNoErr;
        if (!outside && !inside) return kNoErr;
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
        if (inside) {
            // Two fills above Characters, which need one fill and one stroke across the frame.
            const bool oneFill = map.fillPaint && paint.fillPaint && map.fill.overprint && UniformSolid(paint.fill.color, map.fill.color);
            if (!oneStroke || !oneFill) return refuse("type-inside-needs-one-fill-one-stroke");
            if (s.parser.ContainsEffects(p) || s.parser.CountPaintFields(p) > 2 || s.parser.ContainsTransparency(p)) return refuse("type-styled");
            const AIStrokeStyle stroke = paint.stroke;
            if (const char* why = BandRefusal(basic, s, nullptr, stroke)) return refuse(why);
            // The stroke kept below Characters, hidden, for restore; the characters' strokes cleared.
            AIParserPaintField keep = nullptr;
            if ((e = s.parser.NewPaintFieldStroke(&stroke, nullptr, &keep)) != kNoErr || !keep) return e ? e : kOutOfMemoryErr;
            if ((e = s.parser.InsertNthPaintField(p, -1, keep)) != kNoErr) { s.parser.DisposePaintField(keep); return e; }
            AIArtStyleHandle created = nullptr;
            if ((e = s.parser.CreateNewStyle(p, &created)) != kNoErr || !created) return e ? e : kBadParameterErr;
            if ((e = s.styles.SetArtStyle(art, created)) != kNoErr) return e;
            // The copy (the last field) taken off again if it cannot be hidden or recorded.
            auto dropCopy = [&](ASErr err) {
                AIArtStyleHandle st1 = nullptr;
                AIStyleParser q = nullptr;
                if (!s.styles.GetArtStyle(art, &st1) && st1 && !s.parser.NewParser(&q) && q) {
                    AIParserPaintField last = nullptr;
                    AIArtStyleHandle made = nullptr;
                    const ai::int32 n = !s.parser.ParseStyle(q, st1) ? s.parser.CountPaintFields(q) : 0;
                    if (n > 0 && !s.parser.GetNthPaintField(q, n - 1, &last) && last && s.parser.IsStroke(last) &&
                        !s.parser.RemovePaintField(q, last, true) && !s.parser.CreateNewStyle(q, &made) && made)
                        s.styles.SetArtStyle(art, made);
                    s.parser.DisposeParser(q);
                }
                return err;
            };
            if ((e = SetStrokeFieldVisible(s, art, true, false, true)) != kNoErr) return dropCopy(e);
            if (!WriteAlignRecord(basic, s.art, art, AlignRecord{"type-offset", 2, stroke.width})) {
                SetStrokeFieldVisible(s, art, false, true, true);
                dropCopy(kNoErr);
                return refuse("record");
            }
            auto fail = [&](ASErr err) { NativeAlignStats ignored; NativeAlignItem(basic, s, art, "rollback", ignored); return err; };
            {
                TextRangeRef r = nullptr;
                if (s.text.GetATETextRange(art, &r) || !r) return fail(kBadParameterErr);
                ATE::CharFeaturesRef f = nullptr;
                ATE::ATEErr ate = s.features.Initialize(&f);
                if (!ate && f) ate = s.features.SetStroke(f, false);
                if (!ate && f) ate = s.range.ReplaceOrAddLocalCharFeatures(r, f);
                if (f) s.features.Release(f);
                s.range.Release(r);
                if (ate) return fail(kBadParameterErr);
            }
            if ((e = AddOffsetFills(basic, s, art, paint.fill, paint.evenodd, stroke)) != kNoErr) return fail(e);
            bool synced = false;
            if ((e = SyncOffset(basic, s, art, stroke.width, synced)) != kNoErr) return fail(e);
            ++st.offset;
            return kNoErr;
        }
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
            if (!WriteAlignRecord(basic, s.art, art, AlignRecord{"type-outside", 1, w})) Trace("align.record", kBadParameterErr, "type-outside");
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
        // All or nothing: Restore halves every stroked run, so a run left at its weight would be
        // halved (its failures were ignored: the review of 2026-11-01).
        auto setWidth = [&](const std::pair<ATETextDOM::Int32, ATETextDOM::Int32>& span, double width) {
            TextRangeRef r = nullptr;
            if (s.range.Clone(whole, &r) || !r) return false;
            ATE::CharFeaturesRef f = nullptr;
            bool ok = !s.range.SetStart(r, span.first) && !s.range.SetEnd(r, span.second) && !s.features.Initialize(&f) && f &&
                      !s.features.SetLineWidth(f, static_cast<ATETextDOM::Float>(width)) && !s.range.ReplaceOrAddLocalCharFeatures(r, f);
            if (f) s.features.Release(f);
            s.range.Release(r);
            return ok;
        };
        size_t doubledRuns = 0;
        while (doubledRuns < runsOut.size() && setWidth(runsOut[doubledRuns].first, runsOut[doubledRuns].second)) ++doubledRuns;
        if (doubledRuns < runsOut.size()) {
            for (size_t k = 0; k < doubledRuns; ++k) setWidth(runsOut[k].first, runsOut[k].second / 2);
            s.range.Release(whole);
            return refuse("type-chars-width");
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
        if (!WriteAlignRecord(basic, s.art, art, AlignRecord{"type-chars", 1, fillFirstWasOn ? 1.0 : 0.0})) Trace("align.record", kBadParameterErr, "type-chars");
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
        const ASErr e = doubleUnderFill(true);
        AlignRecord made = ReadAlignRecord(basic, s.art, art);
        // Doubled and recorded for the dashes opt-in even when aligning its dashes then failed, so the
        // switch turned off still takes it back (the second review of 2026-11-01).
        if (made.as == "doubled") { made.dashes = true; WriteAlignRecord(basic, s.art, art, made); }
        return e;
    }
    if (!(outside || inside)) return refuse("mode");
    if (!anyOpen) {
        const ai::uint32 want = outside ? kAIStrokeAlignmentOutside : kAIStrokeAlignmentInside;
        if (paintStyle->SetStrokeAlignment(art, want) || ReadAlignment(basic, s, art) != want) return refuse("native-refused");
        ++st.native;
        // A dashed stroke set Outside takes the dashed Outside opt-in, as one set Outside in the
        // Stroke panel does through the re-fix (the owner's W2, 2026-10-31: Align Selection Outside
        // after Inside lost its whole-point dashes).
        const auto& sw = Switches();
        if (outside && style.stroke.dash.length > 0 && sw.wholePoints && sw.patternPoints && sw.outsideDashes)
            return NativeAlignItem(basic, s, art, "dashes", st);
        return kNoErr;
    }
    if (type == kCompoundPathArt) {
        if (inside) {
            if (!SimpleFillAndStroke(s, art)) return refuse("needs-one-fill-one-stroke");
            AIArtStyleHandle st0 = nullptr;
            AIStyleParser p = nullptr;
            AIFillStyle fill{};
            AIStrokeStyle stroke{};
            bool haveFill = false, haveStroke = false;
            if (!s.styles.GetArtStyle(art, &st0) && st0 && !s.parser.NewParser(&p) && p) {
                if (!s.parser.ParseStyle(p, st0))
                    for (ai::int32 k = 0; k < s.parser.CountPaintFields(p); ++k) {
                        AIParserPaintField f = nullptr;
                        AIArtStylePaintData data;
                        if (s.parser.GetNthPaintField(p, k, &f) || !f) continue;
                        if (s.parser.IsFill(f) && !s.parser.GetFill(f, &fill, &data)) haveFill = true;
                        else if (s.parser.IsStroke(f) && !s.parser.GetStroke(f, &stroke, &data)) haveStroke = true;
                    }
                s.parser.DisposeParser(p);
            }
            if (!haveFill || !haveStroke) return refuse("needs-one-fill-one-stroke");
            if (const char* why = BandRefusal(basic, s, art, stroke)) return refuse(why);
            // An open contour's fill ends along its closing, where no stroke runs, and the band would
            // run there too (measured: 237.6 of a 240 pt2 strip along a 60 pt closing). Allowed only
            // where each closing is no longer than the weight, a contour left open by a hair; a longer
            // one is refused, and joining it gives Illustrator's own Inside.
            for (const auto& c : contours)
                if (!c.closed && !c.segments.empty() && ise_miter::Len(c.segments.back().p - c.start) > stroke.width + 1e-6)
                    return refuse("inside-open-closing");
            // The record first, so a failure part-way can always be rolled back.
            if (!WriteAlignRecord(basic, s.art, art, AlignRecord{"offset", 2, stroke.width})) return refuse("record");
            auto e = SetStrokeFieldVisible(s, art, true, false);
            if (e) { WriteAlignRecord(basic, s.art, art, AlignRecord{}); return e; }
            bool synced = false;
            if ((e = AddOffsetFills(basic, s, art, fill, style.evenodd, stroke)) != kNoErr ||
                (e = SyncOffset(basic, s, art, stroke.width, synced)) != kNoErr) {
                NativeAlignStats ignored;
                NativeAlignItem(basic, s, art, "rollback", ignored);
                return e;
            }
            ++st.offset;
            return kNoErr;
        }
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

namespace correct_detail {
// Whether an item draws nothing at all: hidden, an unpainted path, a compound path of those, an
// empty text frame, or a group of those (not a clip group).
bool Invisible(SPBasicSuite* basic, const AIArtSuite& art, const AIPathStyleSuite& paint, const AITextFrameSuite& text,
               const ATE::TextRangeSuite& range, AIArtHandle a, int depth = 0) {
    if (depth > 16) return false;
    ai::int32 attr = 0;
    if (!art.GetArtUserAttr(a, kArtHidden, &attr) && (attr & kArtHidden)) return true;
    short type = kUnknownArt;
    if (art.GetArtType(a, &type)) return false;
    if (type == kPathArt) {
        AIPathStyle st{};
        AIBoolean advanced = false;
        AIBoolean clip = false;
        if (art.GetArtUserAttr(a, kArtIsClipMask, &attr) == kNoErr && (attr & kArtIsClipMask)) clip = true;
        return !clip && !paint.GetPathStyle(a, &st, &advanced) && !st.fillPaint && !st.strokePaint;
    }
    if (type == kTextFrameArt) {
        TextRangeRef r = nullptr;
        ATETextDOM::Int32 size = 1;
        if (!text.GetATETextRange(a, &r) && r) { range.GetSize(r, &size); range.Release(r); }
        return size == 0;
    }
    if (type == kGroupArt || type == kCompoundPathArt) {
        AIBoolean clipped = false;
        if (type == kGroupArt) {
            Suite<AIGroupSuite> group(basic, kAIGroupSuite, kAIGroupVersion);
            if (group.error || group->GetGroupClipped(a, &clipped) || clipped) return false;
        }
        AIArtHandle child = nullptr;
        for (art.GetArtFirstChild(a, &child); child;) {
            if (!Invisible(basic, art, paint, text, range, child, depth + 1)) return false;
            if (art.GetArtSibling(child, &child)) return false;
        }
        return true;
    }
    return false;
}

void SelectOnly(const AIMatchingArtSuite& matching, const AIArtSuite& art, const std::vector<AIArtHandle>& items) {
    matching.DeselectAll();
    for (AIArtHandle a : items) art.SetArtUserAttr(a, kArtSelected, kArtSelected);
}
} // namespace correct_detail

ASErr CorrectSelection(SPMessageData& message, State& state) {
    auto& c = state.correct;
    c = CorrectStats{};
    const auto& sw = Switches();
    if (!sw.enabled) { c.gate = "correct.off"; return kNoErr; }
    // Switches that are off take their corrections off (FixPath, FixItems, and below).
    struct Reconcile { Reconcile() { gReconcile = true; } ~Reconcile() { gReconcile = false; } } reconcile;
    Suite<AIArtSuite> art(message.basic, kAIArtSuite, kAIArtVersion);
    Suite<AIMatchingArtSuite> matching(message.basic, kAIMatchingArtSuite, kAIMatchingArtVersion);
    Suite<AIMdMemorySuite> memory(message.basic, kAIMdMemorySuite, kAIMdMemorySuiteVersion);
    Suite<AITextFrameSuite> text(message.basic, kAITextFrameSuite, kAITextFrameVersion);
    Suite<ATE::TextRangeSuite> range(message.basic, kTextRangeSuite, kTextRangeSuiteVersion);
    Suite<AIPathStyleSuite> paint(message.basic, kAIPathStyleSuite, kAIPathStyleVersion);
    Suite<AIPathSuite> path(message.basic, kAIPathSuite, kAIPathVersion);
    Suite<AIArtStyleSuite> styles(message.basic, kAIArtStyleSuite, kAIArtStyleVersion);
    Suite<AIArtStyleParserSuite> parser(message.basic, kAIArtStyleParserSuite, kAIArtStyleParserVersion);
    Suite<ATE::CharFeaturesSuite> features(message.basic, kCharFeaturesSuite, kCharFeaturesSuiteVersion);
    Suite<AIATEPaintSuite> atePaint(message.basic, kAIATEPaintSuite, kAIATEPaintVersion);
    Suite<AIDictionarySuite> dictionary(message.basic, kAIDictionarySuite, kAIDictionaryVersion);
    Suite<AIPaintStyleSuite> paintStyle(message.basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
    Suite<ATE::TextFrameSuite> textFrame(message.basic, kTextFrameSuite, kTextFrameSuiteVersion);
    for (ASErr e : {art.error, matching.error, memory.error, text.error, range.error, paint.error, path.error, styles.error,
                    parser.error, features.error, atePaint.error, dictionary.error, paintStyle.error, textFrame.error})
        if (e) { c.gate = "correct.suites"; return e; }
    const AIArtSuite& A = *art.operator&();
    const RouteSuites rs{message.basic, A, *styles.operator&(), *parser.operator&(), *text.operator&(), *range.operator&(),
                         *features.operator&(), *atePaint.operator&(), *dictionary.operator&()};
    const FixSuites fs{A, *path.operator&(), *paint.operator&(), *paintStyle.operator&(), *styles.operator&(),
                       *parser.operator&(), *text.operator&(), *range.operator&(), *features.operator&(), *textFrame.operator&()};
    const AIMatchingArtSuite& M = *matching.operator&();
    struct Handles {
        const AIMdMemorySuite& api; AIArtHandle** p = nullptr;
        ~Handles() { if (p) api.MdMemoryDisposeHandle(reinterpret_cast<AIMdMemoryHandle>(p)); }
    } selected{*memory.operator&()}, every{*memory.operator&()};
    AIMatchingArtSpec specs[3] = {AIMatchingArtSpec(kTextFrameArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kPathArt, kArtSelected, kArtSelected),
                                  AIMatchingArtSpec(kCompoundPathArt, kArtSelected, kArtSelected)};
    ai::int32 n = 0;
    if (M.GetMatchingArt(specs, 3, &selected.p, &n) || n <= 0) { c.gate = "correct.no-selection"; return kNoErr; }
    std::vector<AIArtHandle> frames, paths;
    for (ai::int32 i = 0; i < n; ++i) {
        AIArtHandle a = (*selected.p)[i], parent = nullptr;
        short type = kUnknownArt, ptype = kUnknownArt;
        if (A.GetArtType(a, &type)) continue;
        if (!A.GetArtParent(a, &parent) && parent && !A.GetArtType(parent, &ptype) && ptype == kCompoundPathArt) continue;
        (type == kTextFrameArt ? frames : paths).push_back(a);
    }
    // Every frame of each story in the document, to tell a complete story from a part.
    std::map<ai::int32, int> storyTotal;
    {
        AIMatchingArtSpec all(kTextFrameArt, 0, 0);
        ai::int32 m = 0;
        if (!M.GetMatchingArt(&all, 1, &every.p, &m))
            for (ai::int32 i = 0; i < m; ++i) {
                ai::int32 index = -1;
                if (!text->GetStoryIndex((*every.p)[i], &index) && index >= 0) ++storyTotal[index];
            }
    }
    std::map<ai::int32, std::vector<AIArtHandle>> byStory;
    std::vector<AIArtHandle> routedGroups;
    // The children of a parent, top first.
    auto children = [&](AIArtHandle parent) {
        std::vector<AIArtHandle> out;
        AIArtHandle ch = nullptr;
        for (A.GetArtFirstChild(parent, &ch); ch;) { out.push_back(ch); if (A.GetArtSibling(ch, &ch)) break; }
        return out;
    };
    {
        std::vector<AIArtHandle> rest;
        for (AIArtHandle f : frames) {
            AIArtHandle parent = nullptr;
            short ptype = kUnknownArt;
            if (!A.GetArtParent(f, &parent) && parent && !A.GetArtType(parent, &ptype) && ptype == kGroupArt &&
                ReadRouteRecord(A, *dictionary.operator&(), parent) != kRouteNone) {
                if (std::find(routedGroups.begin(), routedGroups.end(), parent) == routedGroups.end()) routedGroups.push_back(parent);
                continue;
            }
            rest.push_back(f);
        }
        // A story group the switches would no longer make (Strokes behind or Across a story's
        // threaded frames off, or several stories with Across separate stories off): its route
        // taken off and the group taken apart, its frames back where it was, in their order, and
        // corrected below as the switches say.
        // Also one whose story now has a frame outside it among the selection (a frame threaded on
        // since, for one): taken apart and grouped again with it.
        std::vector<AIArtHandle> kept;
        for (AIArtHandle g : routedGroups) {
            std::set<ai::int32> ids;
            for (AIArtHandle f : RouteFrames(A, g)) { ai::int32 index = -1; if (!text->GetStoryIndex(f, &index)) ids.insert(index); }
            // Grown: a frame of its story outside it would join it, that is, shares its parent with
            // nothing visible between. A frame that visible artwork keeps apart is a run of its own,
            // and the group stays as it is (it was taken apart and rebuilt on every correction,
            // losing its own opacity or name: the review of 2026-11-01).
            bool grown = false;
            AIArtHandle gParent = nullptr;
            A.GetArtParent(g, &gParent);
            const auto sibs = gParent ? children(gParent) : std::vector<AIArtHandle>{};
            const auto at = [&](AIArtHandle x) { return static_cast<int>(std::find(sibs.begin(), sibs.end(), x) - sibs.begin()); };
            for (AIArtHandle f : rest) {
                ai::int32 index = -1;
                if (grown || text->GetStoryIndex(f, &index) || !ids.count(index)) continue;
                const int gi = at(g), fi = at(f);
                if (gi >= static_cast<int>(sibs.size()) || fi >= static_cast<int>(sibs.size())) continue;   // another parent
                bool visible = false;
                for (int k = (std::min)(gi, fi) + 1; k < (std::max)(gi, fi) && !visible; ++k)
                    visible = !correct_detail::Invisible(message.basic, A, *paint.operator&(), *text.operator&(), *range.operator&(), sibs[k]);
                grown = !visible;
            }
            // One whose stroke was lifted from its frames' own goes with its opt-in.
            std::string sig;
            const RouteKind existingRoute = ReadRouteRecord(A, *dictionary.operator&(), g, &sig);
            const bool lifted = sig.find(kLiftedSig) != std::string::npos;
            const bool keep = sw.textBehind && sw.textStory && (ids.size() < 2 || sw.textStories) && !grown &&
                              (!lifted || sw.appearanceStrokes);
            if (keep && !GroupRouteSupportsFields(existingRoute, sw.closeHoles, sw.textMiter)) {
                // Preserve the existing group, including owner opacity, blend mode,
                // name, and paint. It is excluded from this correction pass entirely.
                ++c.refused;
                c.refusal = "correct.existing-mixed-group";
                continue;
            }
            bool removed = false;
            if (keep || Unroute(rs, g, removed) != kNoErr) { kept.push_back(g); continue; }
            const auto kids = children(g);
            for (AIArtHandle ch : kids) A.ReorderArt(ch, kPlaceAbove, g);
            AIArtHandle left = nullptr;
            if (!A.GetArtFirstChild(g, &left) && !left) A.DisposeArt(g);
            ++c.dissolved;
            for (AIArtHandle ch : kids) { short t = kUnknownArt; if (!A.GetArtType(ch, &t) && t == kTextFrameArt) rest.push_back(ch); }
        }
        routedGroups = kept;
        frames = rest;
    }
    for (AIArtHandle f : frames) {
        ai::int32 index = -1;
        if (!text->GetStoryIndex(f, &index) && index >= 0) byStory[index].push_back(f);
    }
    std::vector<AIArtHandle> lone, groups;
    // Groups one set of frames that share a parent, top to bottom; true when a run was routed.
    auto groupStories = [&](const std::vector<AIArtHandle>& set, bool separate) -> bool {
        AIArtHandle parent = nullptr;
        if (A.GetArtParent(set[0], &parent) || !parent) return false;
        for (AIArtHandle f : set) { AIArtHandle p2 = nullptr; if (A.GetArtParent(f, &p2) || p2 != parent) return false; }
        std::set<AIArtHandle> chosen(set.begin(), set.end());
        auto sibs = children(parent);
        int first = -1, last = -1;
        for (int i = 0; i < static_cast<int>(sibs.size()); ++i) if (chosen.count(sibs[i])) { if (first < 0) first = i; last = i; }
        if (first < 0) return false;
        std::vector<std::vector<AIArtHandle>> runs(1);
        std::vector<AIArtHandle> hidden;
        int visible = 0;
        for (int i = first; i <= last; ++i) {
            if (chosen.count(sibs[i])) { runs.back().push_back(sibs[i]); continue; }
            if (correct_detail::Invisible(message.basic, A, *paint.operator&(), *text.operator&(), *range.operator&(), sibs[i])) {
                hidden.push_back(sibs[i]); continue;
            }
            ++visible;
            if (!runs.back().empty()) runs.emplace_back();
        }
        if (visible && separate) return false;   // separate stories need nothing visible between them
        bool hiddenMoved = false;
        bool any = false;
        for (auto& run : runs) {
            if (run.size() < 2) { for (AIArtHandle f : run) lone.push_back(f); continue; }
            CharPaint anticipated;
            const ASErr paintError = ReadGroupingPaint(rs, run, anticipated);
            if (paintError || !GroupRouteSupportsFields(ChooseRoute(anticipated), sw.closeHoles, sw.textMiter)) {
                ++c.refused;
                c.refusal = paintError ? "correct.paint-preflight" : "story.mixed-stroke-fields";
                if (separate) return any;
                for (AIArtHandle f : run) lone.push_back(f);
                continue;
            }
            if (!hiddenMoved) {
                for (auto it = hidden.rbegin(); it != hidden.rend(); ++it)
                    if (!A.ReorderArt(*it, kPlaceBelow, sibs[last])) ++c.moved;
                hiddenMoved = true;
            }
            // Routes of their own (corrected frame by frame before) give way to the group's, and
            // so do their own Close holes fills, which the group's fill refused as the frames'
            // own construction (story.frame-style): Across other text objects turned off, then on,
            // left the stories apart with Close holes on (the owner's V1, 2026-10-31).
            bool prepared = true;
            for (AIArtHandle f : run) {
                bool removed = false, holes = false, tips = false;
                if (Unroute(rs, f, removed) || CloseHolesField(message.basic, fs, f, false, 1, holes) ||
                    TipFields(message.basic, fs, f, false, false, 1, 0, tips)) {
                    prepared = false;
                    break;
                }
            }
            // Owned tip fills carry effects too. They must move to the group along with Close
            // holes, or StoryFill mistakes a previously corrected frame for user-built art.
            if (!prepared) {
                ++c.refused;
                c.refusal = "correct.prepare";
                for (AIArtHandle f : run) lone.push_back(f);
                continue;
            }
            AIArtHandle group = nullptr;
            if (A.NewArt(kGroupArt, kPlaceAbove, run[0], &group) || !group) { for (AIArtHandle f : run) lone.push_back(f); continue; }
            bool moved = true;
            for (AIArtHandle f : run) if (A.ReorderArt(f, kPlaceInsideOnBottom, group)) moved = false;
            correct_detail::SelectOnly(M, A, {group});
            const ASErr e = moved ? StoryFill(message, state, separate, visible > 0, group) : kBadParameterErr;
            if (e) {
                // Taken apart again, the frames in their order above where the group was.
                for (AIArtHandle f : run) A.ReorderArt(f, kPlaceAbove, group);
                AIArtHandle left = nullptr;
                if (!A.GetArtFirstChild(group, &left) && !left) A.DisposeArt(group);
                else { c.refusal = "correct.rollback-incomplete"; c.gate = "correct.rollback-incomplete"; return any; }
                ++c.refused;
                c.refusal = moved ? state.appearance.gate : "correct.group-move";
                if (!separate) for (AIArtHandle f : run) lone.push_back(f);
                if (separate) return any;
                continue;
            }
            groups.push_back(group);
            ++c.runs;
            any = true;
        }
        if (visible) ++c.split;
        return any;
    };
    std::vector<ai::int32> complete;
    // Stories selected whole, one-frame stories too: those take part in Across separate stories
    // (two one-frame stories were never grouped, hand test 14, 2026-10-30).
    for (auto& [index, list] : byStory) {
        if (sw.textStory && storyTotal[index] == static_cast<int>(list.size())) complete.push_back(index);
        else for (AIArtHandle f : list) lone.push_back(f);
    }
    bool separateDone = false;
    if (sw.textBehind && sw.textStory && sw.textStories && complete.size() >= 2 && !gNoSeparate) {
        std::vector<AIArtHandle> set;
        for (ai::int32 index : complete) for (AIArtHandle f : byStory[index]) set.push_back(f);
        const size_t before = lone.size();
        separateDone = groupStories(set, true);
        if (separateDone) c.stories = static_cast<int>(complete.size());
        else lone.resize(before);
    }
    if (!separateDone)
        for (ai::int32 index : complete) {
            if (!sw.textBehind || byStory[index].size() < 2) { for (AIArtHandle f : byStory[index]) lone.push_back(f); continue; }
            if (groupStories(byStory[index], false)) ++c.stories;
            else for (AIArtHandle f : byStory[index]) {
                bool inGroup = false;
                AIArtHandle p = nullptr;
                if (!A.GetArtParent(f, &p)) for (AIArtHandle g : groups) inGroup = inGroup || p == g;
                if (!inGroup && std::find(lone.begin(), lone.end(), f) == lone.end()) lone.push_back(f);
            }
        }
    c.frames = static_cast<int>(lone.size());
    c.paths = static_cast<int>(paths.size());
    bool partial = false;
    if (sw.textBehind && !lone.empty()) {
        correct_detail::SelectOnly(M, A, lone);
        partial |= NativeRoute(message, state, &lone) != kNoErr;
    } else if (!sw.textBehind) {
        for (AIArtHandle f : lone) {
            bool removed = false;
            if (Unroute(rs, f, removed)) partial = true;
            else if (removed) ++c.unrouted;
        }
    }
    // Dashed Outside strokes off: what the opt-in made of the selected paths put back, before
    // the native fix judges them again.
    const bool outsideDashes = sw.wholePoints && sw.patternPoints && sw.outsideDashes;   // with its parents, as the panel grays it
    if (!outsideDashes && !paths.empty()) {
        std::vector<AIArtHandle> optIn;
        for (AIArtHandle a : paths) if (ReadAlignRecord(message.basic, A, a).dashes) optIn.push_back(a);
        if (!optIn.empty()) {
            NativeAlignStats st;
            correct_detail::SelectOnly(M, A, optIn);
            if (NativeAlign(message, "restore", st)) partial = true;
            c.undashed = st.restored;
        }
    }
    // native-fix on everything corrected: the frames (story frames for their character limits),
    // the story groups (their stroke's limit), and the paths.
    std::vector<AIArtHandle> all = lone;
    groups.insert(groups.end(), routedGroups.begin(), routedGroups.end());
    for (AIArtHandle g : groups) { all.push_back(g); for (AIArtHandle ch : children(g)) all.push_back(ch); }
    all.insert(all.end(), paths.begin(), paths.end());
    if (!all.empty()) {
        correct_detail::SelectOnly(M, A, all);
        partial |= NativeFix(message, state) != kNoErr;
    }
    if (!paths.empty() && outsideDashes) {
        NativeAlignStats st;
        if (outsideDashes) {
            correct_detail::SelectOnly(M, A, paths);
            if (!NativeAlign(message, "dashes", st)) ++c.dashes;
        }
        correct_detail::SelectOnly(M, A, paths);
        NativeFix(message, state);
    }
    correct_detail::SelectOnly(M, A, all);
    // A group that could not be taken apart again keeps its gate (it read "corrected": the review
    // of 2026-11-01).
    if (std::strcmp(c.gate, "correct.rollback-incomplete") != 0) c.gate = partial ? "corrected-partial" : "corrected";
    return kNoErr;
}

std::string CorrectJson(const State& state) {
    const auto& c = state.correct;
    char b[768];
    std::snprintf(b, sizeof b,
                  "{\"gate\":\"%s\",\"stories\":%d,\"runs\":%d,\"moved\":%d,\"split\":%d,\"frames\":%d,\"paths\":%d,"
                  "\"aligned\":%d,\"dashes\":%d,\"refused\":%d,\"refusal\":\"%s\",\"unrouted\":%d,\"dissolved\":%d,"
                  "\"undashed\":%d,\"build\":\"%s\"}",
                  c.gate, c.stories, c.runs, c.moved, c.split, c.frames, c.paths, c.aligned, c.dashes, c.refused, c.refusal,
                  c.unrouted, c.dissolved, c.undashed, build);
    return b;
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
        "\"dashRefused\":%d,\"pathfinder\":%d,\"failed\":%d,\"offsetSynced\":%d,\"fieldLimits\":%d,\"orderRestored\":%d,\"objectStrokes\":%d,\"realigned\":%d,\"holes\":%d,\"tips\":%d,\"tipGate\":\"%s\",\"pathJudged\":\"%s\",\"carried\":%d},"
        "\"align\":{\"gate\":\"%s\",\"native\":%d,\"profiled\":%d,\"doubled\":%d,\"typeOutside\":%d,"
        "\"restored\":%d,\"refused\":%d,\"offset\":%d,\"reasons\":\"%s\"},"
        "\"refix\":{\"timer\":%s,\"scans\":%llu,\"runs\":%llu,\"items\":%llu,\"deferred\":%llu,\"lastError\":%d,"
        "\"released\":%llu,\"overflow\":%llu,\"rerouted\":%llu,\"routeReleased\":%llu,\"dashed\":%llu,\"automatic\":%llu,\"skipped\":%llu,\"redraws\":%llu,\"threaded\":%llu,\"opened\":%llu,\"docSwitches\":%llu,\"typingWaits\":%llu,\"reapplied\":%llu,\"regrouped\":%llu,"
        "\"undoPast\":%d,\"undoFuture\":%d},"
        "\"appearance\":{\"gate\":\"%s\",\"frames\":%d,\"styled\":%d,\"fieldsBefore\":%d,\"fieldsAfter\":%d,"
        "\"contentsBefore\":%d,\"contentsAfter\":%d,\"fillFirst\":%d,\"objectStrokes\":%d},"
        "\"closeHoles\":{\"registered\":%s,\"calls\":%llu,\"published\":%llu,\"passed\":%llu,\"contours\":%llu,\"rings\":%llu,\"gate\":\"%s\"},"
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
        state.nativeFix.dashRefused, state.nativeFix.pathfinder, state.nativeFix.failed, state.nativeFix.offsetSynced,
        state.nativeFix.fieldLimits, state.nativeFix.orderRestored, state.nativeFix.objectStrokes, state.nativeFix.realigned, state.nativeFix.holes, state.nativeFix.tips, gTipGate, gPathJudged, state.nativeFix.carried,
        state.align.gate, state.align.native, state.align.profiled, state.align.doubled, state.align.typeOutside,
        state.align.restored, state.align.refused, state.align.offset, state.align.reasons,
        state.refix.timer ? "true" : "false", state.refix.scans, state.refix.runs, state.refix.items,
        state.refix.deferred, state.refix.lastError, state.refix.released, state.refix.overflow, state.refix.rerouted,
        state.refix.routeReleased, state.refix.dashed, state.refix.automatic, state.refix.skipped, state.refix.redraws, state.refix.threaded, state.refix.opened, state.refix.docSwitches, state.refix.typingWaits, state.refix.reapplied, state.refix.regrouped, undoPast, undoFuture,
        state.appearance.gate, state.appearance.frames, state.appearance.styled, state.appearance.fieldsBefore,
        state.appearance.fieldsAfter, state.appearance.contentsBefore, state.appearance.contentsAfter,
        state.appearance.fillFirst, state.appearance.objectStrokes,
        state.closeHoles ? "true" : "false", state.closeHolesStats.calls, state.closeHolesStats.published, state.closeHolesStats.passed,
        state.closeHolesStats.contours, state.closeHolesStats.rings, state.closeHolesStats.gate);
    return n < 0 || static_cast<size_t>(n) >= capacity ? kBadParameterErr : kNoErr;
}

} // namespace region_probe
namespace sm_panel { std::string Status(); bool Press(const char* what); bool Shot(); void Refresh(); }
namespace region_probe {

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
    if (std::strcmp(selector, "panel-align") == 0) {
        // Inside, center, or outside only, and only while StrokeMaster is on, as the panel's own
        // commands (any other input queued Outside: the review of 2026-11-01).
        const int which = std::strcmp(input, "inside") == 0 ? 2 : std::strcmp(input, "center") == 0 ? 3 : std::strcmp(input, "outside") == 0 ? 1 : 0;
        if (which && Switches().enabled) state.panelAlignPending = which;
        const std::string text = (which && Switches().enabled ? "queued " + std::to_string(which) : std::string("refused")) +
                                 " last " + std::to_string(state.panelAlignResult);
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "correct") == 0 && input[0] == '\0') {
        CorrectSelection(message.d, state);
        const std::string text = CorrectJson(state);
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    // The panel's switches: "?" reads them, "defaults" restores them, "key=0|1" sets one; each
    // change is saved to the settings file the panel uses.
    if (std::strcmp(selector, "settings") == 0) {
        auto& sw = Switches();
        // "ok", "unchanged", or "unsaved": in force, but the file could not be written (it read
        // "unchanged": the review of 2026-11-01).
        const char* answer = "ok";
        if (std::strcmp(input, "defaults") == 0) { sw = ai_settings::Settings{}; if (!ai_settings::Save(sw)) answer = "unsaved"; }
        else if (std::strcmp(input, "?") != 0) {
            const ai_settings::Settings before = sw;
            ai_settings::Parse(input, sw);
            answer = sw == before ? "unchanged" : ai_settings::Save(sw) ? "ok" : "unsaved";
        }
        sm_panel::Refresh();
        const std::string text = std::string(answer) + "\n" + ai_settings::Format(sw);
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    // The panel, for tests: "ui-status" says what exists; "ui-press" sends a control BM_CLICK
    // (a scripted press through the control's own click handling, not a hand's click).
    if (std::strcmp(selector, "ui-status") == 0 || std::strcmp(selector, "ui-press") == 0) {
        std::string text;
        if (std::strcmp(selector, "ui-press") == 0) text = sm_panel::Press(input) ? "pressed " : "refused ";
        if (std::strcmp(selector, "ui-press") == 0 && std::strcmp(input, "shot") == 0) text = sm_panel::Shot() ? "shot " : "no-shot ";
        text += sm_panel::Status();
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "effects-list") == 0) {
        std::string text;
        if (EffectsList(message.d, text)) text = "effects-list failed";
        if (auto err2 = unicode->Destroy(message.outParam)) return err2;
        return unicode->Initialize(message.outParam, text.c_str(),
            static_cast<ai::UnicodeString::offset_type>(text.size()), kAIUTF8CharacterEncoding);
    }
    if (std::strcmp(selector, "style-dump") == 0) {
        std::string text;
        const auto err = StyleDump(message.d, text, std::strcmp(input, "parent") == 0);
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
    else if (std::strcmp(selector, "field-limit") == 0 && input[0] != '\0')
        actionError = FieldLimit(message.d, input);
    else if (std::strcmp(selector, "appearance") == 0 && std::strncmp(input, "miter:", 6) == 0)
        actionError = CharacterJoin(message.d, state, input + 6);
    else if (std::strcmp(selector, "appearance") == 0 && std::strncmp(input, "fill-first:", 11) == 0)
        actionError = FillFirst(message.d, state, input + 11);
    else if (std::strcmp(selector, "dash-align") == 0)
        actionError = DashAlign(message.d, state, input);
    else if (std::strcmp(selector, "refix") == 0 && (std::strcmp(input, "on") == 0 || std::strcmp(input, "off") == 0)) {
        // Pauses or resumes the automatic re-fix (tests; a user switch later).
        state.refixPaused = std::strcmp(input, "off") == 0;
        actionError = state.refix.timer ? kNoErr : kBadParameterErr;
    }
    else if (std::strcmp(selector, "route-pass") == 0 && input[0] != '\0')
        actionError = RoutePass(message.d, state, input);
    else if (std::strcmp(selector, "native-route") == 0 && input[0] == '\0')
        actionError = NativeRoute(message.d, state);
    else if ((std::strcmp(selector, "story-fill") == 0 || std::strcmp(selector, "story-route") == 0) &&
             (input[0] == '\0' || std::strcmp(input, "separate") == 0 || std::strcmp(input, "part") == 0))
        actionError = StoryFill(message.d, state, std::strcmp(input, "separate") == 0, std::strcmp(input, "part") == 0);
    else if (std::strcmp(selector, "native-align") == 0) {
        NativeAlignStats st;
        actionError = NativeAlign(message.d, input, st);
        auto& a = state.align;
        a.gate = actionError ? "partial" : "aligned";
        a.native = st.native; a.profiled = st.profiled; a.doubled = st.doubled; a.typeOutside = st.typeOutside;
        a.restored = st.restored; a.refused = st.refused; a.offset = st.offset;
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

#include "StrokeMasterPanel.h"

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
                    state->closeHoles = startupState.closeHoles;
                    state->flatTips = startupState.flatTips;
                    state->refix.timer = startupState.refix.timer;
                    region_probe::Trace("globals.allocated");
                    // The StrokeMaster panel (Window > StrokeMaster); a host without panels
                    // (the mock host) simply has none.
                    sm_panel::Startup(message.basic, message.self, &state->correctPending, &state->alignPending,
                                      state->refix.timer != nullptr);
                }
                return kNoErr;
            }
            if (std::strcmp(selector, kSPInterfaceShutdownSelector) == 0) {
                sm_panel::Shutdown();
                if (sAIUnicodeString) {
                    message.basic->ReleaseSuite(kAIUnicodeStringSuite, kAIUnicodeStringSuiteVersion);
                    sAIUnicodeString = nullptr;
                }
                if (sSPBlocks) {
                    message.basic->ReleaseSuite(kSPBlocksSuite, kSPBlocksSuiteVersion);
                    sSPBlocks = nullptr;
                }
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
        if (std::strcmp(caller, kCallerAIMenu) == 0) {
            auto& menu = *static_cast<AIMenuMessage*>(raw);
            if (sm_panel::OwnsMenuItem(menu.menuItem)) {
                if (std::strcmp(selector, kSelectorAIGoMenuItem) == 0) sm_panel::ToggleShown();
                else if (std::strcmp(selector, kSelectorAIUpdateMenuItem) == 0) sm_panel::UpdateMenu();
            }
            return finish(kNoErr);
        }
        if (std::strcmp(caller, kCallerAITimer) == 0 && std::strcmp(selector, kSelectorAIGoTimer) == 0) {
            auto& tm = *static_cast<AITimerMessage*>(raw);
            if (!state->refix.timer || tm.timer != state->refix.timer) return finish(kNoErr);
            region_probe::TelemetryScope telemetry(&state->telemetry);
            if (state->correctPending) {
                state->correctPending = false;
                region_probe::Suite<AIUndoSuite> undo(tm.d.basic, kAIUndoSuite, kAIUndoVersion);
                if (!undo.error) {
                    undo->SetKind(kAIStandardUndoContext);
                    if (EnsureUnicode(tm.d.basic)) undo->SetUndoTextUS(ai::UnicodeString("Undo Correct Strokes"), ai::UnicodeString("Redo Correct Strokes"));
                }
                region_probe::CorrectSelection(tm.d, *state);
                state->refixRefreshOnly = true;
                // Illustrator redraws after a plugin's own menu command, not after a timer: a correction
                // showed only once the object moved, so each press showed the one before it (the
                // owner's retests Q1 and P17, 2026-10-31: Trim glyph tips seemed to act when turned off).
                region_probe::Suite<AIDocumentSuite> document(tm.d.basic, kAIDocumentSuite, kAIDocumentVersion);
                if (!document.error) document->RedrawDocument();
            }
            if (state->alignPending) {
                // The panel menu's Align commands, as strokemaster-align-selection.jsx runs them.
                const char* mode = state->alignPending == 1 ? "outside" : (state->alignPending == 2 ? "inside" : "restore");
                state->alignPending = 0;
                region_probe::Suite<AIUndoSuite> undo(tm.d.basic, kAIUndoSuite, kAIUndoVersion);
                if (!undo.error) {
                    undo->SetKind(kAIStandardUndoContext);
                    if (EnsureUnicode(tm.d.basic)) undo->SetUndoTextUS(ai::UnicodeString("Undo Align Strokes"), ai::UnicodeString("Redo Align Strokes"));
                }
                region_probe::NativeAlignStats st;
                if (!region_probe::NativeAlign(tm.d, mode, st) && std::strcmp(mode, "restore") != 0) region_probe::NativeFix(tm.d, *state);
                state->refixRefreshOnly = true;
                region_probe::Suite<AIDocumentSuite> document(tm.d.basic, kAIDocumentSuite, kAIDocumentVersion);
                if (!document.error) document->RedrawDocument();
            }
            if (state->panelAlignPending) {
                const int which = state->panelAlignPending;
                state->panelAlignPending = 0;
                region_probe::Suite<AIPaintStyleSuite> paintStyle(tm.d.basic, kAIPaintStyleSuite, kAIPaintStyleVersion);
                if (!paintStyle.error) {
                    region_probe::Suite<AIUndoSuite> undo(tm.d.basic, kAIUndoSuite, kAIUndoVersion);
                    if (!undo.error) undo->SetKind(kAIStandardUndoContext);
                    state->panelAlignResult = static_cast<int>(paintStyle->SetStrokeAlignmentOnSelection(
                        which == 2 ? kAIStrokeAlignmentInside : (which == 3 ? kAIStrokeAlignmentCenter : kAIStrokeAlignmentOutside)));
                }
                return finish(kNoErr);   // the re-fix sees the change at the next message, as after a click
            }
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
                if (state->closeHoles && go.effect == state->closeHoles) {
                    region_probe::TelemetryScope telemetry(&state->telemetry);
                    return finish(region_probe::CloseHolesGo(go, *state));
                }
                if (state->flatTips && go.effect == state->flatTips) {
                    region_probe::TelemetryScope telemetry(&state->telemetry);
                    return finish(region_probe::FlatTipsGo(go, *state));
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
