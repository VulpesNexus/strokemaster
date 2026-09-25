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
        std::cout << "cross-frame core: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
}
