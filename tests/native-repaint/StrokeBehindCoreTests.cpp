// Pure tests of plugin/source/IseStrokeBehindCore.h: eligibility and the muted-run
// ledger. No Adobe types, host, rendering, or ABI are involved.
#include "../../plugin/source/IseStrokeBehindCore.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ise_stroke_behind;
static int checks = 0;
static void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

namespace {
const std::uint32_t kNone = 1;
struct Wax { std::uint32_t stroke; };
Fingerprint Print(std::uint32_t fill, int glyphs, std::int64_t x)
{
    Fingerprint f; f.fillPaint = fill; f.widthMicro = 8000000; f.glyphCount = glyphs; f.xMicro = x; f.yMicro = 0;
    return f;
}
// One visit of either pass, as the glue performs it: recover through the current pointer.
bool Visit(Ledger& l, Wax& w, const Fingerprint& p)
{
    return l.Recover(&w, &w, p, w.stroke == kNone, [&](std::uint32_t paint) { w.stroke = paint; });
}
// The stroke pass after recovery: mute an eligible run.
void MuteRun(Ledger& l, Wax& w, const Fingerprint& p)
{
    Check(l.Mute(&w, &w, p, w.stroke), "mute recorded");
    w.stroke = kNone;
}
}

int main()
{
    try {
        // Eligibility.
        Check(Eligible(Alignment::Outside, 8, Method::FillStroke), "Outside fill+stroke moves");
        Check(Eligible(Alignment::Outside, 8, Method::StrokeOnly), "Outside stroke-only moves");
        Check(!Eligible(Alignment::Center, 8, Method::FillStroke), "Center stroke stays native");
        Check(!Eligible(Alignment::Other, 8, Method::FillStroke), "unknown alignment stays native");
        Check(!Eligible(Alignment::Outside, 0, Method::FillStroke), "zero width stays native");
        Check(!Eligible(Alignment::Outside, -1, Method::FillStroke), "negative width stays native");
        Check(!Eligible(Alignment::Outside, std::numeric_limits<double>::quiet_NaN(), Method::FillStroke), "NaN width stays native");
        Check(!Eligible(Alignment::Outside, 1e6, Method::FillStroke), "absurd width stays native");
        Check(!Eligible(Alignment::Outside, 8, Method::FillOnly), "no stroke, nothing to move");
        Check(!Eligible(Alignment::Outside, 8, Method::None), "nothing painted, nothing to move");

        {   // An ordinary draw: stroke pass mutes, Foreground pass restores.
            Ledger l; Wax r{11};
            Check(!Visit(l, r, Print(1, 2, 0)), "nothing to recover before the first mute");
            MuteRun(l, r, Print(1, 2, 0));
            Check(r.stroke == kNone && l.IsRecorded(&r), "muted for the Text pass");
            Check(Visit(l, r, Print(1, 2, 0)) && r.stroke == 11 && l.Size() == 0, "Foreground restores and forgets");
        }
        {   // The Foreground visit never came: the next draw's first visit restores first.
            Ledger l; Wax r{12};
            MuteRun(l, r, Print(1, 2, 0));
            Check(Visit(l, r, Print(1, 2, 0)) && r.stroke == 12 && l.recovered == 1, "next draw recovers the paint");
            MuteRun(l, r, Print(1, 2, 0));
            Check(Visit(l, r, Print(1, 2, 0)) && r.stroke == 12, "and the cycle continues normally");
        }
        {   // Recomposed after an interrupted draw, with a new stroke color, same address and
            // fingerprint: the record must not overwrite the new paint.
            Ledger l; Wax r{13};
            MuteRun(l, r, Print(1, 2, 0));
            r.stroke = 99; // fresh wax carrying the user's new stroke
            Check(!Visit(l, r, Print(1, 2, 0)) && r.stroke == 99 && l.staleDropped == 1 && l.Size() == 0,
                  "a run that already has a stroke is never written");
        }
        {   // Other wax at the same run address: a different render-data pointer or print.
            Ledger l; Wax r{14}, other{kNone};
            MuteRun(l, r, Print(1, 2, 0));
            bool applied = false;
            Check(!l.Recover(&r, &other, Print(1, 2, 0), true, [&](std::uint32_t) { applied = true; }) && !applied,
                  "another render-data pointer is not the recorded wax");
            MuteRun(l, r, Print(1, 2, 0));
            Check(!l.Recover(&r, &r, Print(1, 3, 0), true, [&](std::uint32_t) { applied = true; }) && !applied,
                  "another fingerprint is not the recorded wax");
            Check(l.Size() == 0 && l.staleDropped == 2, "both stale records dropped");
        }
        {   // A new mute replaces an older record for the same run.
            Ledger l; Wax r{15};
            Check(l.Mute(&r, &r, Print(1, 2, 0), 15) && l.Mute(&r, &r, Print(1, 2, 0), 16) && l.Size() == 1,
                  "one record per run");
        }
        {   // The record cap: beyond it, runs are left to native drawing.
            Ledger l;
            static int keys[Ledger::kMaxRecords + 1];
            for (std::size_t i = 0; i < Ledger::kMaxRecords; ++i)
                Check(l.Mute(&keys[i], &keys[i], Print(1, 1, 0), 2), "under the cap");
            Check(!l.Mute(&keys[Ledger::kMaxRecords], &keys[Ledger::kMaxRecords], Print(1, 1, 0), 2) && l.overflow == 1,
                  "the cap refuses, so the caller must not mute");
        }
        {   // Center strokes: only their join is muted (miter to bevel) and given back.
            Check(CenterMiterEligible(Alignment::Center, 8, Method::FillStroke), "Center fill+stroke is corrected");
            Check(CenterMiterEligible(Alignment::Center, 8, Method::StrokeOnly), "Center stroke-only is corrected");
            Check(!CenterMiterEligible(Alignment::Outside, 8, Method::FillStroke), "Outside takes the stroke pass instead");
            Check(!CenterMiterEligible(Alignment::Center, 0, Method::FillStroke), "zero-width Center stays native");
            Check(!CenterMiterEligible(Alignment::Center, 8, Method::FillOnly), "no stroke, no join to mute");
            const std::uint32_t kMiter = 0, kBevel = 2;
            Ledger joins;
            int run = 0, data = 0;
            std::uint32_t join = kMiter;
            auto recover = [&](const Fingerprint& p) {
                return joins.Recover(&run, &data, p, join == kBevel, [&](std::uint32_t j) { join = j; });
            };
            Check(joins.Mute(&run, &data, Print(1, 2, 0), join), "join mute recorded");
            join = kBevel;
            Check(recover(Print(1, 2, 0)) && join == kMiter && joins.Size() == 0, "the miter join comes back after the Text pass");
            Check(joins.Mute(&run, &data, Print(1, 2, 0), join), "muted again on the next draw");
            join = kBevel;
            Check(!recover(Print(1, 3, 0)) && join == kBevel && joins.staleDropped == 1, "other wax at the same address is not touched");
            Check(joins.Mute(&run, &data, Print(1, 2, 0), kMiter), "muted again");
            join = kMiter;   // recomposed with a miter join already: nothing to give back
            Check(!recover(Print(1, 2, 0)) && join == kMiter && joins.Size() == 0, "a join no longer muted is left alone");
            Check(&ThreadJoinLedger() != &ThreadLedger(), "joins and paints keep separate ledgers");
        }
        std::cout << "PASS: " << checks << " stroke-behind core checks; no host execution.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
