// Pure tests of plugin/source/IseCrossFrameCore.h: traversal brackets, fill records, the
// stroke-withholding rule, and the content that cancels it. No Adobe types are involved.
#include "../../plugin/source/IseCrossFrameCore.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace ise_cross_frame;
static int checks = 0;
static void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

namespace {
int kPortA, kPortB, kRun1, kRun2, kRun3;

Glyph Square(double x, double y, double s)
{
    Glyph g;
    const double pts[4][2] = {{x, y}, {x + s, y}, {x + s, y + s}, {x, y + s}};
    for (int i = 0; i < 4; ++i) {
        Seg seg{i == 0 ? Seg::Move : Seg::Line, {pts[i][0], pts[i][1], 0, 0, 0, 0}};
        g.segs.push_back(seg);
        g.box.Add(pts[i][0], pts[i][1]);
    }
    g.segs.push_back(Seg{Seg::Close, {0, 0, 0, 0, 0, 0}});
    return g;
}

FillRecord Fill(const void* run, const void* port, std::uint32_t story, std::uint32_t frame, std::uint32_t key, std::vector<Glyph> glyphs)
{
    FillRecord r;
    r.run = run; r.port = port; r.story = story; r.frame = frame; r.parcelKey = key;
    r.glyphs = std::move(glyphs);
    for (const auto& g : r.glyphs) r.box.Add(g.box);
    return r;
}

StrokeContext Stroke(const void* port, std::uint32_t story, std::uint32_t frame, std::uint32_t key, Box box, std::vector<std::uint32_t> own = {})
{
    StrokeContext s;
    s.port = port; s.story = story; s.frame = frame; s.parcelKey = key; s.strokeBox = box; s.own = std::move(own);
    return s;
}

Box B(double x0, double y0, double x1, double y1) { Box b; b.Add(x0, y0); b.Add(x1, y1); return b; }
}

int main()
{
    try {
        // Boxes.
        Check(Box().Empty(), "a new box is empty");
        Check(!Box().Meets(B(0, 0, 1, 1)), "an empty box meets nothing");
        Check(B(0, 0, 10, 10).Meets(B(10, 10, 20, 20)), "touching boxes meet");
        Check(!B(0, 0, 10, 10).Meets(B(10.5, 0, 20, 10)), "separate boxes do not meet");
        Check(B(0, 0, 10, 10).Grown(2).Meets(B(11, 11, 12, 12)), "growing reaches a neighbor");

        // Screen cache dependencies respect story scope and exclude isolated frames.
        {
            const std::vector<FrameBounds> frames = {{1, 7, B(0, 0, 10, 10)}, {2, 7, B(5, 5, 15, 15)},
                                                      {3, 8, B(12, 12, 20, 20)}, {4, 7, B(100, 100, 110, 110)}};
            Check(RelatedFrames(frames, false) == std::vector<std::uint32_t>({1, 2}), "screen: same-story overlaps only");
            Check(RelatedFrames(frames, true) == std::vector<std::uint32_t>({1, 2, 3}), "screen: separate stories opt in");
            auto moved = frames;
            moved[1].box = B(50, 50, 60, 60);
            Check(RelatedFrames(moved, true).empty(), "screen: moved-apart frames have no new dependency");
            Check(RelatedFrames({{1, 7, Box()}, {2, 7, B(0, 0, 10, 10)}}, true).empty(), "screen: empty bounds have no dependency");
        }

        // A dense spread has bounded storage/work, with conservative invalidation if the
        // comparison budget is exceeded. Duplicate columns never duplicate the result.
        {
            std::vector<FrameBounds> dense;
            for (std::uint32_t uid = 1; uid <= 1500; ++uid)
                dense.push_back({uid, uid, B(0, 0, 10, 10)});
            dense.push_back({1501, 9, B(100, 100, 110, 110)});
            auto affected = RelatedFrames(dense, true);
            Check(affected.size() == 1501 && affected.front() == 1 && affected.back() == 1501,
                  "screen: exhausted comparison budget conservatively keeps every frame");
            Check(RelatedFrames({{1, 7, B(0, 0, 10, 10)}, {1, 7, B(0, 0, 10, 10)}}, true).empty(),
                  "screen: duplicate entries alone create no dependency");
            Check(RelatedFrames({{0, 7, B(0, 0, 10, 10)}, {2, 7, B(0, 0, 10, 10)}}, true).empty(),
                  "screen: invalid frame identities create no dependency");
        }

        // Ends that never came: a bracket stack deeper than any real draw starts over, and the
        // records of the lost traversal go (the review of 2026-11-01).
        {
            Tracker t;
            t.Begin();
            Check(t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)})), "kept in the first traversal");
            for (int i = 1; i < Tracker::kMaxDepth; ++i) t.Begin();
            Check(t.Records() == 1 && t.Depth() == Tracker::kMaxDepth, "nested brackets keep the records");
            t.Begin();
            Check(t.Records() == 0 && t.Depth() == 1 && t.resets == 1, "one more starts over");
            t.End();
            Check(!t.Active(), "and closes normally");
        }

        // One missed end: the next spread draw on the same port starts over; a partial draw inside
        // a spread draw keeps its records (the second review of 2026-11-01).
        {
            Tracker t;
            t.Begin(true, &kPortA);
            Check(t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)})), "kept in the spread draw");
            t.Begin(false, &kPortA);
            Check(t.Records() == 1 && t.Depth() == 2, "a partial draw inside keeps them");
            t.End();
            t.Begin(true, &kPortA);
            Check(t.Records() == 0 && t.Depth() == 1, "a spread draw on the same port, its end missed: started over");
            t.End();
            Check(!t.Active(), "and closed");
            t.Begin(true, &kPortA);
            t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            t.Begin(true, &kPortB);
            Check(t.Records() == 1 && t.Depth() == 2, "a spread draw on another port nests");
        }

        // Nothing is recorded outside a traversal.
        {
            Tracker t;
            Check(!t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)})), "no record outside a bracket");
            Check(t.Records() == 0, "none kept");
        }

        // A painted fill of the same story, from another frame, withholds a stroke that meets it.
        {
            Tracker t;
            t.Begin();
            Check(t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10), Square(50, 0, 10)})), "pending kept");
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).empty(), "a pending fill withholds nothing");
            Check(t.Painted(&kRun1, &kPortA), "confirmed");
            Check(!t.Painted(&kRun1, &kPortA), "confirmed once");
            auto w = t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false);
            Check(w.size() == 1 && w[0]->box.x0 == 0, "only the glyph the stroke meets");
            Check(t.Withheld(Stroke(&kPortB, 7, 200, 2, B(5, 5, 20, 20)), false).empty(), "another port is another drawing");
            Check(t.Withheld(Stroke(&kPortA, 8, 200, 2, B(5, 5, 20, 20)), false).empty(), "another story is left alone");
            Check(t.Withheld(Stroke(&kPortA, 8, 200, 2, B(5, 5, 20, 20)), true).size() == 1, "overlap mode takes any story");
            Check(t.Withheld(Stroke(&kPortA, 7, 100, 1, B(5, 5, 20, 20)), false).empty(), "the same parcel is route W's own");
            Check(t.Withheld(Stroke(&kPortA, 7, 100, 3, B(5, 5, 20, 20)), false).size() == 1, "another column of the same frame counts");
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(100, 100, 120, 120)), false).empty(), "a stroke elsewhere is left alone");
            Check(t.Withheld(Stroke(&kPortA, 0, 200, 2, B(5, 5, 20, 20)), false).empty(), "an unknown story matches nothing");
            t.End();
            Check(t.Records() == 0 && t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).empty(), "the traversal's end drops everything");
        }

        // Content drawn since the fill was painted, over the glyph, cancels withholding.
        {
            Tracker t;
            t.Begin();
            ContentEvent before; before.item = 55; before.port = &kPortA; before.box = B(0, 0, 10, 10);
            t.Content(before);
            t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            t.Painted(&kRun1, &kPortA);
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).size() == 1, "content drawn before the fill does not cancel");
            ContentEvent own; own.item = 200; own.port = &kPortA; own.box = B(0, 0, 30, 30);
            t.Content(own);
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20), {200, 300}), false).size() == 1, "the stroking frame's own shape does not cancel");
            ContentEvent ownFill = own; ownFill.frameFill = true;
            t.Content(ownFill);
            std::size_t covered = 0;
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20), {200, 300}), false, &covered).empty() && covered == 1,
                  "the stroking frame's own frame fill cancels");
        }
        {
            Tracker t;
            t.Begin();
            t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            t.Painted(&kRun1, &kPortA);
            ContentEvent between; between.item = 55; between.port = &kPortA; between.box = B(2, 2, 4, 4);
            t.Content(between);
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).empty(), "an item stacked in between over the glyph cancels");
            ContentEvent away = between; away.box = B(40, 40, 50, 50);
            Tracker u;
            u.Begin();
            u.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            u.Painted(&kRun1, &kPortA);
            u.Content(away);
            Check(u.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).size() == 1, "an item elsewhere does not cancel");
            ContentEvent otherPort = between; otherPort.port = &kPortB;
            Tracker v;
            v.Begin();
            v.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            v.Painted(&kRun1, &kPortA);
            v.Content(otherPort);
            Check(v.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).size() == 1, "content in another port does not cancel");
        }

        // Brackets: nested draws keep the outer traversal's records; a Begin with no End
        // before it starts clean.
        {
            Tracker t;
            t.Begin();
            t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            t.Painted(&kRun1, &kPortA);
            t.Begin();
            Check(t.Depth() == 2 && t.PaintedRecords() == 1, "a nested bracket keeps the records");
            t.End();
            Check(t.PaintedRecords() == 1, "closing the nested bracket keeps them");
            t.End();
            Check(!t.Active() && t.Records() == 0, "closing the outer bracket drops them");
            t.End();
            Check(t.Depth() == 0, "an unmatched end does not go negative");
        }

        // A run drawn again in the same traversal (tiles) replaces its record, pending again.
        {
            Tracker t;
            t.Begin();
            t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            t.Painted(&kRun1, &kPortA);
            t.Pending(Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)}));
            Check(t.Records() == 1 && t.PaintedRecords() == 0, "a redraw replaces the record");
            Check(t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false).empty(), "and is pending until painted again");
            Check(!t.Painted(&kRun2, &kPortA), "an unknown run confirms nothing");
            Check(!t.Painted(&kRun1, &kPortB), "a run confirms only in its own port");
            Check(!t.Pending(Fill(&kRun3, &kPortA, 7, 100, 1, {})), "a fill with no glyphs is not recorded");
        }
        // Lazy records: outlines are built only for a record whose generous box meets a stroke,
        // once, and the exact glyphs then decide.
        {
            Tracker t;
            t.Begin();
            auto lazy = [&](const void* run, Box approx) {
                FillRecord r = Fill(run, &kPortA, 7, 100, 1, {});
                r.resolved = false; r.box = approx; r.source = std::make_shared<int>(42);
                return r;
            };
            Check(t.Pending(lazy(&kRun1, B(-5, -5, 60, 15))), "an unresolved record with a box is kept");
            Check(t.Pending(lazy(&kRun2, B(200, 200, 260, 215))), "a second, far away");
            Check(!t.Pending(lazy(&kRun3, Box())), "an unresolved record needs a box");
            t.Painted(&kRun1, &kPortA); t.Painted(&kRun2, &kPortA);
            int calls = 0;
            auto resolve = [&](FillRecord& r) {
                ++calls;
                Check(r.source && *std::static_pointer_cast<int>(r.source) == 42, "the source reaches the resolver");
                r.glyphs = {Square(0, 0, 10), Square(50, 0, 10)};
                r.box = Box(); for (const auto& g : r.glyphs) r.box.Add(g.box);
            };
            auto w = t.Withheld(Stroke(&kPortA, 7, 200, 2, B(5, 5, 20, 20)), false, nullptr, resolve);
            Check(calls == 1 && w.size() == 1, "only the record the stroke meets is resolved, and its exact glyph decides");
            w = t.Withheld(Stroke(&kPortA, 7, 200, 2, B(52, 2, 55, 8)), false, nullptr, resolve);
            Check(calls == 1 && w.size() == 1 && w[0]->box.x0 == 50, "a resolved record is not resolved again");
            w = t.Withheld(Stroke(&kPortA, 7, 200, 2, B(30, 2, 40, 8)), false, nullptr, resolve);
            Check(w.empty(), "inside the generous box but between the exact glyphs: nothing withheld");
            Check(t.resolvedRecords == 1, "one record resolved in all");
        }
        // Neighbor diagnostics distinguish missing paint, separate ports, and occluding art.
        {
            Tracker t;
            t.Begin();
            auto record = Fill(&kRun1, &kPortA, 7, 100, 1, {Square(0, 0, 10)});
            record.reach = 3; record.strokePaint = 9;
            t.Pending(record);
            const auto context = Stroke(&kPortA, 7, 200, 2, B(-3, -3, 13, 13));
            NeighborStats pending;
            Check(t.Neighbors(context, false, 3, 9, [](FillRecord&) {}, &pending).empty() && pending.pending == 1,
                  "neighbors: a pending fill is diagnosed");
            t.Painted(&kRun1, &kPortA);
            Check(t.Neighbors(context, false, 3, 9, [](FillRecord&) {}).size() == 1, "neighbors: matching painted fill is read");
            NeighborStats ports;
            Check(t.Neighbors(Stroke(&kPortB, 7, 200, 2, context.strokeBox), false, 3, 9, [](FillRecord&) {}, &ports).empty() &&
                  ports.otherPort == 1, "neighbors: a different output port stays isolated");
            Check(t.Neighbors(Stroke(&kPortA, 8, 200, 2, context.strokeBox), false, 3, 9, [](FillRecord&) {}).empty(),
                  "neighbors: another story needs the opt-in");
            Check(t.Neighbors(Stroke(&kPortA, 8, 200, 2, context.strokeBox), true, 3, 9, [](FillRecord&) {}).size() == 1,
                  "neighbors: the separate-story opt-in reads it");
            ContentEvent e; e.item = 55; e.port = &kPortA; e.box = B(0, 0, 10, 10); t.Content(e);
            NeighborStats covered;
            Check(t.Neighbors(context, false, 3, 9, [](FillRecord&) {}, &covered).empty() && covered.covered == 1,
                  "neighbors: intervening artwork is diagnosed and preserved");
        }
        {
            std::vector<std::uint32_t> history;
            RememberScreenFrames(history, {{11, 1, B(0,0,1,1)}, {12, 2, B(100,100,101,101)}});
            Check(history == std::vector<std::uint32_t>({11,12}), "unpaired frames can gain a first neighbor after an edit");
            RememberScreenFrames(history, {{12, 2, B(0,0,1,1)}, {0, 0, Box()}, {13, 3, Box()}});
            Check(history == std::vector<std::uint32_t>({11,12,13}), "history deduplicates identities and retains empty frames that may gain ink");
            for (std::uint32_t i=14; i<114; ++i) RememberScreenFrames(history, {{i, i, B(0,0,1,1)}});
            Check(history.front()==11 && history.back()==113, "history does not evict still-cached frames after 64 entries");
        }
        std::cout << "cross-frame core: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
}
