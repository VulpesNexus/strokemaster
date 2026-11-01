//========================================================================================
// Cross-frame core: SDK-free bookkeeping for route X, the cross-frame extension of route W.
//
// Route W draws each Outside stroke of a frame before that frame's fills, inside one frame's
// adornment passes. A later frame of the same story is drawn in a later page-item draw, after
// the earlier frame's fills are already painted, so its strokes land on them (E00). Nothing
// drawn later can go beneath something already painted. Route X therefore does not move the
// later stroke; it withholds it where it would cover a fill the same traversal has already
// painted for the same story:
//
//   1. While a frame's runs are visited before its Text pass, every fill-bearing run of a
//      story with more than one parcel is recorded as pending: a generous box around it in
//      pasteboard coordinates, what is needed to rebuild its glyph outlines later, its story
//      (frame list), its frame and parcel, and the port. The outlines themselves are built
//      only when a stroke's box first meets that box (most recorded fills are never needed).
//   2. The Foreground visit of that run confirms it: its fill has been painted.
//   3. When route W strokes a later run of the same story, each confirmed glyph whose box
//      meets the stroke's box is excluded from the stroke by an even-odd clip, unless other
//      content drawn since that glyph was painted meets the glyph (a page item stacked in
//      between, or the later frame's own frame fill): then the stroke is left to cover it,
//      as InDesign draws it, because that content is above the glyph in any order.
//
// For output, only observed paint counts: a frame that was hidden, nonprinting, clipped out of the update
// area, or not drawn for any other reason records nothing and withholds nothing. Records live
// for one traversal (a spread draw, or a partial draw), bracketed by draw events, and belong
// to the port they were drawn into; outside a bracket nothing is recorded.
//
// The glue uses a separate tracker for screen snapshots of earlier visible layout content,
// since cached items can replay without fresh paint callbacks. Output traversal records stay
// isolated from those snapshots.
//
// No host pointer stored here is ever dereferenced: runs and ports are compared only.
// This header runs under the pure test shim.
//========================================================================================
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace ise_cross_frame {

// What the research build does. The production candidate is fixed at Story.
enum class Mode { Native, RouteW, Story, Overlap };

struct Box {
    double x0 = 0, y0 = 0, x1 = -1, y1 = -1;   // empty until a point is added
    bool Empty() const { return x1 < x0 || y1 < y0; }
    void Add(double x, double y)
    {
        if (Empty()) { x0 = x1 = x; y0 = y1 = y; return; }
        x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
    }
    void Add(const Box& b) { if (!b.Empty()) { Add(b.x0, b.y0); Add(b.x1, b.y1); } }
    Box Grown(double d) const { Box b = *this; if (!Empty()) { b.x0 -= d; b.y0 -= d; b.x1 += d; b.y1 += d; } return b; }
    bool Meets(const Box& o) const
    {
        return !Empty() && !o.Empty() && x0 <= o.x1 && o.x0 <= x1 && y0 <= o.y1 && o.y0 <= y1;
    }
};

struct FrameBounds {
    std::uint32_t frame = 0, story = 0;
    Box box;
};

// Only frames whose painted bounds meet can depend on one another. Sweep by x so a spread
// of disjoint frames does not pay for every possible pair during invalidation.
inline std::vector<std::uint32_t> RelatedFrames(std::vector<FrameBounds> frames, bool overlapDomain)
{
    frames.erase(std::remove_if(frames.begin(), frames.end(), [](const FrameBounds& f) {
        return f.frame == 0 || f.box.Empty();
    }), frames.end());
    std::sort(frames.begin(), frames.end(), [](const FrameBounds& a, const FrameBounds& b) { return a.box.x0 < b.box.x0; });
    std::vector<bool> related(frames.size(), false);
    // Dense layouts must not allocate one entry per pair or stall a redraw indefinitely.
    // On exhaustion, extra cache invalidations are safe; omitted dependencies are not.
    constexpr std::size_t maxComparisons = 1000000;
    std::size_t comparisons = 0;
    bool exhausted = false;
    for (std::size_t i = 0; i < frames.size() && !exhausted; ++i) {
        const FrameBounds& a = frames[i];
        for (std::size_t j = i + 1; j < frames.size() && frames[j].box.x0 <= a.box.x1; ++j) {
            if (++comparisons > maxComparisons) { exhausted = true; break; }
            const FrameBounds& b = frames[j];
            if (a.frame == b.frame || (!overlapDomain && (a.story == 0 || a.story != b.story)) || !a.box.Meets(b.box)) continue;
            related[i] = related[j] = true;
        }
    }
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < frames.size(); ++i)
        if (related[i] || exhausted) out.push_back(frames[i].frame);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Every drawn frame may gain a neighbor after an edit, even if it never overlapped before.
// Keep identities only; callers prune deleted UIDs and discard the document on close.
inline void RememberScreenFrames(std::vector<std::uint32_t>& history, const std::vector<FrameBounds>& frames)
{
    for (const auto& frame : frames) if (frame.frame) history.push_back(frame.frame);
    std::sort(history.begin(), history.end());
    history.erase(std::unique(history.begin(), history.end()), history.end());
}

// One path operator of a glyph outline, in pasteboard coordinates.
struct Seg {
    enum Op : std::uint8_t { Move, Line, Curve, Close } op;
    double p[6];
};

struct Glyph {
    std::vector<Seg> segs;
    Box box;
};

// One run's fill, as painted. Until resolved, box is a generous bound of the run and glyphs is
// empty; source holds what the glue needs to build the outlines (opaque here).
struct FillRecord {
    const void* run = nullptr;
    const void* port = nullptr;
    std::uint32_t story = 0;        // the frame list's UID
    std::uint32_t frame = 0;        // the text frame column's UID
    std::uint32_t parcelKey = 0;
    std::uint64_t paintedSeq = 0;   // 0 while pending
    std::vector<Glyph> glyphs;
    Box box;
    bool resolved = true;
    std::shared_ptr<void> source;
    // The run's stroke, for Close holes across frames (Neighbors): how far it reaches beyond the
    // glyphs (0: none route W draws) and its paint's UID.
    double reach = 0;
    std::uint32_t strokePaint = 0;
};

// Content drawn during the traversal that may lie above an earlier fill: a page item's
// shape (with the items it belongs to) or a frame fill.
struct ContentEvent {
    std::uint64_t seq = 0;
    std::uint32_t item = 0;
    bool frameFill = false;
    const void* port = nullptr;
    Box box;
};

// Who is stroking now.
struct StrokeContext {
    const void* port = nullptr;
    std::uint32_t story = 0;
    std::uint32_t frame = 0;
    std::uint32_t parcelKey = 0;
    std::vector<std::uint32_t> own;   // the stroking frame and its ancestors
    Box strokeBox;                         // the stroke's ink, pasteboard
};

struct NeighborStats {
    std::size_t pending = 0, otherPort = 0, otherStory = 0, ownParcel = 0, stroke = 0, bounds = 0, covered = 0;
};

class Tracker {
public:
    static constexpr std::size_t kMaxRecords = 65536;
    static constexpr std::size_t kMaxEvents = 65536;
    // Deeper than any real draw nests (a partial draw inside a spread draw, a placed page inside a
    // frame): ends were missed, and the traversal starts over (the review of 2026-11-01).
    static constexpr int kMaxDepth = 8;

    std::uint64_t Next() { return ++seq_; }
    std::uint64_t Seq() const { return seq_; }

    // Traversal brackets. Records survive nested brackets (a partial draw inside a spread
    // draw) and are dropped when the outermost bracket closes, or when a new outermost
    // bracket opens (an end that never came).
    // `spread`: a spread draw, the outermost bracket. One opening on the port of the spread draw
    // still open is the next one, its end missed: the bookkeeping starts over (a single missed end
    // left the depth at 1 for good, and the records with it: the second review of 2026-11-01).
    void Begin(bool spread = false, const void* port = nullptr)
    {
        if (depth_ >= kMaxDepth || (spread && depth_ > 0 && port != nullptr && port == spreadPort_)) { depth_ = 0; ++resets; }
        if (depth_ == 0) { Clear(); spreadPort_ = nullptr; }
        if (spread && spreadPort_ == nullptr) spreadPort_ = port;
        ++depth_;
    }
    void End()
    {
        if (depth_ > 0) --depth_;
        if (depth_ == 0) { Clear(); spreadPort_ = nullptr; }
    }
    bool Active() const { return depth_ > 0; }
    int Depth() const { return depth_; }

    // Step 1. Replaces an earlier pending or painted record of the same run in this
    // traversal (a run can be drawn more than once, in tiles).
    bool Pending(FillRecord rec)
    {
        if (!Active() || (rec.resolved ? rec.glyphs.empty() : rec.box.Empty())) return false;
        Drop(rec.run);
        // Full: refused. The records of the traversal under way stay (starting over dropped them,
        // and a missed end is recovered in Begin: the second review of 2026-11-01).
        if (records_.size() >= kMaxRecords) { ++overflow; return false; }
        rec.paintedSeq = 0;
        records_.push_back(std::move(rec));
        return true;
    }

    // Step 2.
    bool Painted(const void* run, const void* port)
    {
        for (auto& r : records_) {
            if (r.run == run && r.port == port && r.paintedSeq == 0) { r.paintedSeq = Next(); ++painted_; return true; }
        }
        return false;
    }

    void Content(ContentEvent e)
    {
        if (!Active() || e.box.Empty()) return;
        if (events_.size() >= kMaxEvents) { ++overflow; return; }
        e.seq = Next();
        events_.push_back(e);
    }

    // Step 3. The glyphs a stroke must not cover, in the order they were painted.
    // overlapDomain: any story (the research Overlap mode), not only the stroke's own.
    // resolve(FillRecord&) builds an unresolved record's glyphs and exact box; it is called at
    // most once per record, and only for a record whose generous box meets the stroke.
    template <class Resolve>
    std::vector<const Glyph*> Withheld(const StrokeContext& s, bool overlapDomain, std::size_t* skippedCovered, Resolve resolve)
    {
        std::vector<const Glyph*> out;
        if (!Active() || s.strokeBox.Empty()) return out;
        for (auto& r : records_) {
            if (r.paintedSeq == 0 || r.port != s.port) continue;
            if (!overlapDomain && (r.story == 0 || r.story != s.story)) continue;
            if (r.frame == s.frame && r.parcelKey == s.parcelKey) continue;   // this parcel: route W's
            if (!r.box.Meets(s.strokeBox)) continue;
            if (!r.resolved) {
                r.resolved = true;
                resolve(r);
                ++resolvedRecords;
                if (!r.box.Meets(s.strokeBox)) continue;
            }
            for (const auto& g : r.glyphs) {
                if (!g.box.Meets(s.strokeBox)) continue;
                if (CoveredSince(g.box, r.paintedSeq, s)) { if (skippedCovered) ++*skippedCovered; continue; }
                out.push_back(&g);
            }
        }
        return out;
    }
    std::vector<const Glyph*> Withheld(const StrokeContext& s, bool overlapDomain, std::size_t* skippedCovered = nullptr)
    {
        return Withheld(s, overlapDomain, skippedCovered, [](FillRecord&) {});
    }

    // Close holes across frames (the owner's retest BB1, 2026-11-01): the glyphs of other parcels
    // painted earlier in this traversal near the stroking frame (as Withheld), whose runs' strokes
    // reach as far in the same paint, so that the holes their strokes and the frame's close off
    // together can be filled.
    template <class Resolve>
    std::vector<const Glyph*> Neighbors(const StrokeContext& s, bool overlapDomain, double reach, std::uint32_t paint, Resolve resolve, NeighborStats* stats = nullptr)
    {
        std::vector<const Glyph*> out;
        if (!Active() || s.strokeBox.Empty() || !(reach > 0)) return out;
        for (auto& r : records_) {
            if (r.paintedSeq == 0) { if (stats) ++stats->pending; continue; }
            if (r.port != s.port) { if (stats) ++stats->otherPort; continue; }
            if (!overlapDomain && (r.story == 0 || r.story != s.story)) { if (stats) ++stats->otherStory; continue; }
            if (r.frame == s.frame && r.parcelKey == s.parcelKey) { if (stats) ++stats->ownParcel; continue; }
            if (r.strokePaint != paint || std::fabs(r.reach - reach) > 1e-6) { if (stats) ++stats->stroke; continue; }
            if (!r.box.Meets(s.strokeBox)) { if (stats) ++stats->bounds; continue; }
            if (!r.resolved) {
                r.resolved = true;
                resolve(r);
                ++resolvedRecords;
                if (!r.box.Meets(s.strokeBox)) continue;
            }
            for (const auto& g : r.glyphs) {
                if (!g.box.Meets(s.strokeBox)) continue;
                if (CoveredSince(g.box, r.paintedSeq, s)) { if (stats) ++stats->covered; continue; }
                out.push_back(&g);
            }
        }
        return out;
    }
    std::size_t resolvedRecords = 0;

    std::size_t Records() const { return records_.size(); }
    std::size_t PaintedRecords() const { return painted_; }   // asked at every draw event: kept, not counted
    std::size_t Events() const { return events_.size(); }
    std::size_t overflow = 0;
    std::size_t resets = 0;   // traversals started over because their ends were missed
private:
    const void* spreadPort_ = nullptr;   // the port of the spread draw open, if any
public:

private:
    // Content drawn after the glyph was painted, into the same port, that meets it. The
    // stroking frame's own shape and ancestors are exempt (they only bracket its drawing),
    // except its frame fill, which is painted before its text.
    bool CoveredSince(const Box& glyph, std::uint64_t paintedSeq, const StrokeContext& s) const
    {
        for (const auto& e : events_) {
            if (e.seq <= paintedSeq || e.port != s.port || !e.box.Meets(glyph)) continue;
            if (!e.frameFill && std::find(s.own.begin(), s.own.end(), e.item) != s.own.end()) continue;
            return true;
        }
        return false;
    }
    void Drop(const void* run)
    {
        records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const FillRecord& r) {
            if (r.run != run) return false;
            if (r.paintedSeq != 0) --painted_;
            return true;
        }), records_.end());
    }
    void Clear() { records_.clear(); events_.clear(); painted_ = 0; }

    std::uint64_t seq_ = 0;
    int depth_ = 0;
    std::size_t painted_ = 0;
    std::vector<FillRecord> records_;
    std::vector<ContentEvent> events_;
};

inline Tracker& ThreadTracker()
{
    thread_local Tracker tracker;
    return tracker;
}

} // namespace ise_cross_frame
