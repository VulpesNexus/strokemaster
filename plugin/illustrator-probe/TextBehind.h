// PROPOSED stroke-behind for live text that stays text in every export.
//
// Effects that declare path input receive live text as glyph outlines, so the
// result, and every PDF exported from it, carries outlines instead of text
// (claim 8). Host effects that keep their source art (Drop Shadow, Transform,
// Outer Glow) export the text as text (2026-10-15 control). This transform
// therefore declares text-frame input and returns, for each text frame:
//
//   group
//     a copy of the frame, its character strokes cleared (top: live text, fills)
//     a stroke-only outline copy of the frame             (bottom: paths)
//
// The outline copy keeps every glyph's own stroke paint, so mixed per-character
// strokes are kept, and no stroke paints over any fill. The document's source
// art is never touched: the user's text stays editable, and the effect reruns
// on every edit. Only an explicit Type > Create Outlines makes the text paths.
//
// Validation is read-only and complete before the first mutation. Accepted
// input: a text frame, or groups whose leaves are text frames, with no clipping,
// transparency, blending, knockout, opacity mask, or art style other than a
// simple one with no effects. Anything else is refused and passes through.
// Mutation failures roll back, and only a failed rollback reaches the host.
#pragma once
#include "AIArt.h"
#include "AIPathStyle.h"
#include "AIMask.h"
#include "AIArtStyle.h"
#include "AIArtStyleParser.h"
#include "AITextFrame.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace text_behind {

struct Stats {
    unsigned long long calls = 0, published = 0, refused = 0;
    int rootType = -1, frames = 0, outlineLeaves = 0, stroked = 0, mode = 0;
    const char* gate = "ready";
    char census[384]{}; // per frame: T(characters,font size,stroke on)
};

// Research modes, from the effect parameter StrokeMasterTextMode.
enum Mode { kFull = 0, kCensusOnly = 1, kOutlineOnly = 2, kClearOnly = 3 };

struct Suites {
    const AIArtSuite& art;
    const AIPathStyleSuite& paint;
    const AIBlendStyleSuite& blend;
    const AIMaskSuite& mask;
    const AIArtStyleSuite& styles;
    const AIArtStyleParserSuite& parser;
    const AITextFrameSuite& text;
    const ATE::TextRangeSuite& range;
    const ATE::CharFeaturesSuite& features;
};

class Transform {
    const Suites& s;
    Stats& stats;
    std::vector<AIArtHandle> frames; // bottom first
    static constexpr std::size_t maxNodes = 4096;
    std::size_t nodes = 0;

    ASErr Refuse(const char* gate) { stats.gate = gate; return kBadParameterErr; }

    // No art style, or a simple one with no effects, default paint blends, and
    // no style-level transparency.
    ASErr PlainStyle(AIArtHandle node) {
        AIArtStyleHandle style = nullptr;
        auto e = s.styles.GetArtStyle(node, &style);
        if (e) return e;
        if (!style) return kNoErr;
        ai::int16 type = 0;
        e = s.styles.GetArtStyleType(style, &type);
        if (e) return e;
        if (type != kAIArtStyleSimple) return Refuse("input.complex-style");
        AIStyleParser p = nullptr;
        e = s.parser.NewParser(&p);
        if (e) return e;
        if (!p) return kOutOfMemoryErr;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{s.parser, p};
        e = s.parser.ParseStyle(p, style);
        if (e) return e;
        if (!s.parser.IsStyleParseable(p) || s.parser.ContainsTransparency(p) ||
            s.parser.CountPreEffects(p) != 0 || s.parser.CountPostEffects(p) != 0)
            return Refuse("input.style-effects-or-transparency");
        return kNoErr;
    }

    ASErr Plain(AIArtHandle node) {
        ai::int32 attributes = 0;
        auto e = s.art.GetArtUserAttr(node, kArtIsClipMask, &attributes);
        if (e) return e;
        if (attributes & kArtIsClipMask) return Refuse("input.clip-mask");
        if (s.blend.GetOpacity(node) != 1 || s.blend.GetBlendingMode(node) != kAINormalBlendingMode ||
            s.blend.GetIsolated(node))
            return Refuse("input.transparency");
        const auto knockout = s.blend.GetKnockout(node);
        if (knockout != kAIKnockoutOff && knockout != kAIKnockoutInherit) return Refuse("input.knockout");
        AIMaskRef opacityMask = nullptr;
        e = s.mask.GetMask(node, &opacityMask);
        if (e) return e;
        if (opacityMask) { s.mask.Release(opacityMask); return Refuse("input.opacity-mask"); }
        return PlainStyle(node);
    }

    void Describe(AIArtHandle frame) {
        int characters = -1, strokeOn = -1;
        double fontSize = -1;
        TextRangeRef range = nullptr;
        if (!s.text.GetATETextRange(frame, &range) && range) {
            ATETextDOM::Int32 size = 0;
            if (!s.range.GetSize(range, &size)) characters = size;
            ATE::CharFeaturesRef features = nullptr;
            if (!s.range.GetUniqueCharFeatures(range, &features) && features) {
                ATE::ATEBool8 assigned = false;
                ATETextDOM::Float value = 0;
                if (!s.features.GetFontSize(features, &assigned, &value)) fontSize = assigned ? value : -2;
                ATE::ATEBool8 on = false;
                assigned = false;
                if (!s.features.GetStroke(features, &assigned, &on)) strokeOn = assigned ? (on ? 1 : 0) : -2;
                s.features.Release(features);
            }
            s.range.Release(range);
        }
        const auto used = std::strlen(stats.census);
        if (used + 40 < sizeof(stats.census))
            std::snprintf(stats.census + used, sizeof(stats.census) - used, "T(%d,%g,%d)", characters, fontSize, strokeOn);
    }

    ASErr Collect(AIArtHandle node, int depth) {
        if (depth > 8) return Refuse("input.too-deep");
        if (++nodes > maxNodes) return Refuse("input.too-large");
        short type = kUnknownArt;
        auto e = s.art.GetArtType(node, &type);
        if (e) return e;
        e = Plain(node);
        if (e) return e;
        if (type == kTextFrameArt) { Describe(node); frames.push_back(node); return kNoErr; }
        if (type != kGroupArt) return Refuse("input.art-type");
        std::vector<AIArtHandle> children;
        AIArtHandle child = nullptr;
        e = s.art.GetArtFirstChild(node, &child);
        if (e) return e;
        while (child) {
            if (children.size() >= maxNodes) return Refuse("input.too-large");
            children.push_back(child);
            e = s.art.GetArtSibling(child, &child);
            if (e) return e;
        }
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            e = Collect(*it, depth + 1);
            if (e) return e;
        }
        return kNoErr;
    }

    // Leaves of the outline copy, bottom first. Paths and compounds carry paint;
    // groups are walked.
    ASErr OutlineLeaves(AIArtHandle node, int depth, std::vector<AIArtHandle>& out) {
        if (depth > 8) return Refuse("outline.too-deep");
        if (out.size() >= 20000) return Refuse("outline.too-large");
        short type = kUnknownArt;
        auto e = s.art.GetArtType(node, &type);
        if (e) return e;
        if (type == kPathArt || type == kCompoundPathArt) { out.push_back(node); return kNoErr; }
        if (type != kGroupArt) return Refuse("outline.art-type");
        AIArtHandle child = nullptr;
        e = s.art.GetArtFirstChild(node, &child);
        if (e) return e;
        std::vector<AIArtHandle> children;
        while (child) { children.push_back(child); e = s.art.GetArtSibling(child, &child); if (e) return e; }
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            e = OutlineLeaves(*it, depth + 1, out);
            if (e) return e;
        }
        return kNoErr;
    }

    // The outline copy keeps only its strokes: fills off, unstroked leaves removed.
    // Returns the number of stroked leaves; the copy is disposed by the caller on error.
    ASErr StrokeOnly(AIArtHandle outline, int& stroked) {
        std::vector<AIArtHandle> leaves;
        auto e = OutlineLeaves(outline, 0, leaves);
        if (e) return e;
        stats.outlineLeaves += static_cast<int>(leaves.size());
        stroked = 0;
        for (AIArtHandle leaf : leaves) {
            AIPathStyle style{};
            AIBoolean advanced = false;
            e = s.paint.GetPathStyle(leaf, &style, &advanced);
            if (e) return e;
            if (advanced) return Refuse("outline.advanced-fill");
            if (!style.strokePaint) {
                e = s.art.DisposeArt(leaf);
                if (e) return e;
                continue;
            }
            style.fillPaint = false;
            e = s.paint.SetPathStyle(leaf, &style);
            if (e) return e;
            AIPathStyle actual{};
            e = s.paint.GetPathStyle(leaf, &actual, &advanced);
            if (e) return e;
            if (actual.fillPaint || !actual.strokePaint) return Refuse("output.paint-readback");
            ++stroked;
        }
        return kNoErr;
    }

    // Character strokes off across the frame's whole text range, merged into the existing
    // overrides: SetLocalCharFeatures would replace them all (size, font, leading).
    ASErr ClearTextStrokes(AIArtHandle frame) {
        TextRangeRef range = nullptr;
        auto e = s.text.GetATETextRange(frame, &range);
        if (e) return e;
        if (!range) return Refuse("output.text-range");
        ATE::CharFeaturesRef features = nullptr;
        ATE::ATEErr ate = s.features.Initialize(&features);
        if (!ate && features) ate = s.features.SetStroke(features, false);
        if (!ate && features) ate = s.range.ReplaceOrAddLocalCharFeatures(range, features);
        if (features) s.features.Release(features);
        s.range.Release(range);
        if (ate || !features) return Refuse("output.text-stroke");
        return kNoErr;
    }

public:
    Transform(const Suites& suites, Stats& st) : s(suites), stats(st) {}

    // root is in/out: a text frame root is replaced by the group that holds it.
    ASErr Apply(AIArtHandle& root, int mode = kFull) {
        stats.mode = mode;
        stats.census[0] = 0;
        stats.gate = "validate";
        stats.frames = stats.outlineLeaves = stats.stroked = 0;
        frames.clear();
        nodes = 0;
        short rootType = kUnknownArt;
        auto e = s.art.GetArtType(root, &rootType);
        if (e) return e;
        stats.rootType = rootType;
        e = Collect(root, 0);
        stats.frames = static_cast<int>(frames.size());
        if (e) return e;
        if (frames.empty()) return Refuse("input.no-text");
        if (mode == kCensusOnly) return Refuse("mode.census-only");
        if (mode == kClearOnly) {
            // Research only: it edits the frames themselves, so a failure after the
            // first frame leaves changed art and is reported as such.
            for (std::size_t i = 0; i < frames.size(); ++i) {
                e = ClearTextStrokes(frames[i]);
                if (e) { if (i) stats.gate = "output.rollback-failed"; return e; }
            }
            stats.gate = "published";
            return kNoErr;
        }

        // Staging never touches an original frame: each one gets a group above it
        // holding its stroke-only outline and a duplicate whose strokes are
        // cleared. A failure disposes the groups and leaves the input as it was.
        // Only when every frame is staged are the originals disposed (commit).
        struct Staged { AIArtHandle frame, group; };
        std::vector<Staged> staged;
        const auto rollback = [&](ASErr error) {
            bool clean = true;
            for (auto it = staged.rbegin(); it != staged.rend(); ++it)
                if (s.art.DisposeArt(it->group)) clean = false;
            if (!clean) stats.gate = "output.rollback-failed";
            return error;
        };
        stats.gate = "stage";
        for (AIArtHandle frame : frames) {
            AIArtHandle group = nullptr;
            e = s.art.NewArt(kGroupArt, kPlaceAbove, frame, &group);
            if (e || !group) return rollback(e ? e : kBadParameterErr);
            staged.push_back({frame, group});
            AIArtHandle outline = nullptr;
            e = s.text.CreateOutline(frame, &outline);
            if (e || !outline) return rollback(e ? e : Refuse("outline.none"));
            e = s.art.ReorderArt(outline, kPlaceInsideOnTop, group);
            if (e) {
                // The outline still sits outside the group, so the group's disposal
                // would not take it along.
                const bool kept = s.art.DisposeArt(outline) != kNoErr;
                rollback(e);
                if (kept) stats.gate = "output.rollback-failed";
                return e;
            }
            int stroked = 0;
            e = StrokeOnly(outline, stroked);
            if (e) return rollback(e);
            stats.stroked += stroked;
            AIArtHandle copy = nullptr;
            e = s.art.DuplicateArt(frame, kPlaceInsideOnTop, group, &copy);
            if (e || !copy) return rollback(e ? e : kBadParameterErr);
            if (mode == kOutlineOnly) continue;
            e = ClearTextStrokes(copy);
            if (e) return rollback(e);
        }
        // Nothing stroked: the result would equal the input, so drop the groups.
        if (!stats.stroked) return rollback(Refuse("input.no-stroke"));
        stats.gate = "commit";
        for (const auto& item : staged) {
            if (s.art.DisposeArt(item.frame)) {
                // Earlier originals are gone; the output mixes staged and original frames.
                stats.gate = "output.rollback-failed";
                return kBadParameterErr;
            }
        }
        if (rootType == kTextFrameArt) root = staged.front().group;
        stats.gate = "published";
        return kNoErr;
    }
};

} // namespace text_behind
