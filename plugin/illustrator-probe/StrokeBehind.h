// PROPOSED stroke-behind correction for a post-effect's processing art.
//
// Illustrator paints each glyph (or run) fill and then its stroke, so a stroke
// lands on neighboring fills (region-prep-8 census and PDF paint sequences).
// This transform keeps every leaf's own paint and geometry, but moves every
// stroke contribution into one group beneath all fills: strokes keep their
// relative order among themselves, fills keep theirs, and no stroke is ever
// painted over a fill. It changes paint order only; there is no Boolean
// geometry, so coverage of the union of all paint is unchanged.
//
// Validation is read-only and complete before the first mutation. Anything
// whose meaning depends on paint order in ways this transform does not model
// is refused: a root that is not a group; object transparency, blending,
// isolation, knockout, opacity masks, and clipping; any art style that is not
// simple or carries effects, per-paint blending, or style transparency;
// advanced fills; and art types other than groups, paths, and compound paths.
// Because only simple styles are admitted, a leaf's style is its AIPathStyle,
// so restoring that restores the leaf. A refusal leaves the processing art
// untouched. Mutation failures roll back.
#pragma once
#include "AIArt.h"
#include "AIPathStyle.h"
#include "AIMask.h"
#include "AIArtStyle.h"
#include "AIArtStyleParser.h"
#include <vector>

namespace stroke_behind {

struct Stats {
    unsigned long long calls = 0, published = 0, refused = 0;
    int leaves = 0, stroked = 0;
    const char* gate = "ready";
};

class Transform {
    const AIArtSuite& art;
    const AIPathStyleSuite& paint;
    const AIBlendStyleSuite& blend;
    const AIMaskSuite& mask;
    const AIArtStyleSuite& styles;
    const AIArtStyleParserSuite& parser;
    Stats& stats;
    struct Leaf { AIArtHandle handle; AIPathStyle style; };
    std::vector<Leaf> leaves; // painting order, bottom first
    static constexpr std::size_t maxLeaves = 20000;

    ASErr Refuse(const char* gate) { stats.gate = gate; return kBadParameterErr; }

    // No art style, or a simple one with no effects, default paint blends, and no
    // style-level transparency.
    ASErr PlainStyle(AIArtHandle node) {
        AIArtStyleHandle style = nullptr;
        auto e = styles.GetArtStyle(node, &style);
        if (e) return e;
        if (!style) return kNoErr;
        ai::int16 type = 0;
        e = styles.GetArtStyleType(style, &type);
        if (e) return e;
        if (type != kAIArtStyleSimple) return Refuse("input.complex-style");
        AIStyleParser p = nullptr;
        e = parser.NewParser(&p);
        if (e) return e;
        if (!p) return kOutOfMemoryErr;
        struct Dispose { const AIArtStyleParserSuite& api; AIStyleParser p; ~Dispose() { api.DisposeParser(p); } } dispose{parser, p};
        e = parser.ParseStyle(p, style);
        if (e) return e;
        if (!parser.IsStyleParseable(p) || parser.ContainsTransparency(p) ||
            parser.CountPreEffects(p) != 0 || parser.CountPostEffects(p) != 0)
            return Refuse("input.style-effects-or-transparency");
        const auto fields = parser.CountPaintFields(p);
        for (ai::int32 i = 0; i < fields; ++i) {
            AIParserPaintField field = nullptr;
            e = parser.GetNthPaintField(p, i, &field);
            if (e) return e;
            if (!field || parser.CountEffectsOfPaintField(field) != 0 || !parser.IsDefaultPaintBlend(field))
                return Refuse("input.paint-effect-or-blend");
        }
        return kNoErr;
    }

    ASErr Collect(AIArtHandle node, int depth) {
        if (depth > 16) return Refuse("input.too-deep");
        if (leaves.size() >= maxLeaves) return Refuse("input.too-large");
        short type = kUnknownArt;
        auto e = art.GetArtType(node, &type);
        if (e) return e;
        ai::int32 attributes = 0;
        e = art.GetArtUserAttr(node, kArtIsClipMask, &attributes);
        if (e) return e;
        if (attributes & kArtIsClipMask) return Refuse("input.clip-mask");
        if (blend.GetOpacity(node) != 1 || blend.GetBlendingMode(node) != kAINormalBlendingMode ||
            blend.GetIsolated(node))
            return Refuse("input.transparency");
        const auto knockout = blend.GetKnockout(node);
        if (knockout != kAIKnockoutOff && knockout != kAIKnockoutInherit) return Refuse("input.knockout");
        AIMaskRef opacityMask = nullptr;
        e = mask.GetMask(node, &opacityMask);
        if (e) return e;
        if (opacityMask) { mask.Release(opacityMask); return Refuse("input.opacity-mask"); }
        if (type == kPathArt || type == kCompoundPathArt || type == kGroupArt) {
            e = PlainStyle(node);
            if (e) return e;
        }
        if (type == kPathArt || type == kCompoundPathArt) {
            Leaf leaf{node, {}};
            AIBoolean advanced = false;
            e = paint.GetPathStyle(node, &leaf.style, &advanced);
            if (e) return e;
            if (advanced) return Refuse("input.advanced-fill");
            if (leaf.style.clip) return Refuse("input.clip-style");
            leaves.push_back(leaf); // a compound's children share its paint
            return kNoErr;
        }
        if (type != kGroupArt) return Refuse("input.art-type");
        // First child is the topmost; paint order is bottom first.
        std::vector<AIArtHandle> children;
        AIArtHandle child = nullptr;
        e = art.GetArtFirstChild(node, &child);
        if (e) return e;
        while (child) {
            if (children.size() >= maxLeaves) return Refuse("input.too-large");
            children.push_back(child);
            e = art.GetArtSibling(child, &child);
            if (e) return e;
        }
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            e = Collect(*it, depth + 1);
            if (e) return e;
        }
        return kNoErr;
    }

    ASErr SetAndVerify(AIArtHandle handle, const AIPathStyle& style) {
        auto e = paint.SetPathStyle(handle, &style);
        if (e) return e;
        AIPathStyle actual{};
        AIBoolean advanced = false;
        e = paint.GetPathStyle(handle, &actual, &advanced);
        if (e) return e;
        if (actual.fillPaint != style.fillPaint || actual.strokePaint != style.strokePaint)
            return Refuse("output.paint-readback");
        return kNoErr;
    }

public:
    Transform(const AIArtSuite& a, const AIPathStyleSuite& p, const AIBlendStyleSuite& b, const AIMaskSuite& m,
              const AIArtStyleSuite& st, const AIArtStyleParserSuite& ps, Stats& s)
        : art(a), paint(p), blend(b), mask(m), styles(st), parser(ps), stats(s) {}

    // Returns kNoErr after publishing, or an error with processing art restored.
    ASErr Apply(AIArtHandle root) {
        stats.gate = "validate";
        leaves.clear();
        stats.leaves = stats.stroked = 0;
        // The strokes group is created inside the root, so the root must be a group:
        // a path or compound root (possible with path/compound input) is refused.
        short rootType = kUnknownArt;
        auto e = art.GetArtType(root, &rootType);
        if (e) return e;
        if (rootType != kGroupArt) return Refuse("input.root-not-group");
        e = Collect(root, 0);
        stats.leaves = static_cast<int>(leaves.size());
        stats.stroked = 0;
        if (e) return e;
        for (const auto& leaf : leaves) stats.stroked += leaf.style.strokePaint ? 1 : 0;
        if (!stats.stroked) return Refuse("input.no-stroke");

        AIArtHandle strokes = nullptr;
        std::vector<const Leaf*> changed;
        const auto rollback = [&](ASErr error) {
            // Restore every leaf whose stroke was removed, then drop the
            // duplicates. A restore failure is reported as its own gate.
            bool clean = true;
            for (auto* leaf : changed) if (paint.SetPathStyle(leaf->handle, &leaf->style)) clean = false;
            if (strokes && art.DisposeArt(strokes)) clean = false;
            if (!clean) stats.gate = "output.rollback-failed";
            return error;
        };
        stats.gate = "stage";
        e = art.NewArt(kGroupArt, kPlaceInsideOnBottom, root, &strokes);
        if (e || !strokes) return rollback(e ? e : kBadParameterErr);
        for (const auto& leaf : leaves) {
            if (!leaf.style.strokePaint) continue;
            AIArtHandle copy = nullptr;
            e = art.DuplicateArt(leaf.handle, kPlaceInsideOnTop, strokes, &copy);
            if (e || !copy) return rollback(e ? e : kBadParameterErr);
            AIPathStyle strokeOnly = leaf.style;
            strokeOnly.fillPaint = false;
            e = SetAndVerify(copy, strokeOnly);
            if (e) return rollback(e);
            AIPathStyle fillOnly = leaf.style;
            fillOnly.strokePaint = false;
            changed.push_back(&leaf);
            e = SetAndVerify(leaf.handle, fillOnly);
            if (e) return rollback(e);
        }
        stats.gate = "published";
        return kNoErr;
    }
};

} // namespace stroke_behind
