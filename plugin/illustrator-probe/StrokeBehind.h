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
// is refused: transparency, blending, isolation, knockout, clipping, advanced
// fills, and art types other than groups, paths, and compound paths. A
// refusal leaves the processing art untouched. Mutation failures roll back.
#pragma once
#include "AIArt.h"
#include "AIPathStyle.h"
#include "AIMask.h"
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
    Stats& stats;
    struct Leaf { AIArtHandle handle; AIPathStyle style; };
    std::vector<Leaf> leaves; // painting order, bottom first
    static constexpr std::size_t maxLeaves = 20000;

    ASErr Refuse(const char* gate) { stats.gate = gate; return kBadParameterErr; }

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
    Transform(const AIArtSuite& a, const AIPathStyleSuite& p, const AIBlendStyleSuite& b, Stats& s)
        : art(a), paint(p), blend(b), stats(s) {}

    // Returns kNoErr after publishing, or an error with processing art restored.
    ASErr Apply(AIArtHandle root) {
        stats.gate = "validate";
        leaves.clear();
        auto e = Collect(root, 0);
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
