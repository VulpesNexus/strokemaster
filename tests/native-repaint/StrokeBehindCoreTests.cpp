// Pure tests of plugin/source/IseStrokeBehindCore.h: eligibility and the muted-run
// ledger. No Adobe types, host, rendering, or ABI are involved.
#include "../../plugin/source/IseStrokeBehindCore.h"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ise_stroke_behind;
static int checks = 0;
static void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

namespace {
struct Wax { std::uint32_t stroke; };
Fingerprint Print(std::uint32_t fill, int glyphs, std::int64_t x)
{
    Fingerprint f; f.fillPaint = fill; f.widthMicro = 8000000; f.glyphCount = glyphs; f.xMicro = x; f.yMicro = 0;
    return f;
}
// Every write through a stored pointer is logged, so tests can prove which pointers
// were dereferenced.
std::vector<std::pair<void*, std::uint32_t>> writes;
void Set(void* data, std::uint32_t paint) { writes.push_back({data, paint}); static_cast<Wax*>(data)->stroke = paint; }
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

        const int parcelA = 0, parcelB = 0;
        const void* A = &parcelA; const void* B = &parcelB;
        {   // One ordinary draw: per-run restore plus the end-of-draw fallback.
            Ledger l; writes.clear();
            Wax r1{11}, r2{12};
            Check(!l.Mute(&r1, &r1, Print(1, 2, 0), 11, A), "no mute outside an open draw");
            Check(l.BeginDraw(A), "draw opens");
            Check(l.Mute(&r1, &r1, Print(1, 2, 0), 11, A) && l.Mute(&r2, &r2, Print(1, 2, 5), 12, A), "two mutes");
            Check(!l.Mute(&r1, &r1, Print(1, 2, 0), 11, A), "a run is muted once per draw");
            Check(l.IsMutedNow(&r1) && l.IsMutedNow(&r2), "both muted now");
            Check(!l.Restore(&r1, &r2, Print(1, 2, 0), [](std::uint32_t) {}), "restore refuses another pointer");
            Check(!l.Restore(&r1, &r1, Print(9, 2, 0), [](std::uint32_t) {}), "restore refuses another fingerprint");
            std::uint32_t got = 0;
            Check(l.Restore(&r1, &r1, Print(1, 2, 0), [&](std::uint32_t p) { got = p; }) && got == 11, "per-run restore");
            Check(l.EndDraw(A, Set) == 1 && writes.size() == 1 && writes[0].first == &r2 && r2.stroke == 12,
                  "fallback restores only what is left of this draw");
            Check(l.Size() == 0 && l.OpenDraws() == 0, "draw closed, nothing left");
        }
        {   // A frame anchored in text is drawn inside its host's Text pass.
            Ledger l; writes.clear();
            Wax a1{21}, b1{31};
            l.BeginDraw(A); l.Mute(&a1, &a1, Print(1, 1, 0), 21, A);
            l.BeginDraw(B); l.Mute(&b1, &b1, Print(2, 1, 0), 31, B);
            Check(l.EndDraw(B, Set) == 1 && writes.back().first == &b1, "inner draw restores its own run");
            Check(l.IsMutedNow(&a1), "outer run still muted, its draw still open");
            Check(l.Restore(&a1, &a1, Print(1, 1, 0), [](std::uint32_t) {}), "outer run restored by its own pass");
            Check(l.EndDraw(A, Set) == 0 && writes.size() == 1, "nothing else written");
        }
        {   // An interrupted draw: the record is healed through the host's current pointer.
            Ledger l; writes.clear();
            Wax r1{41};
            l.BeginDraw(A); l.Mute(&r1, &r1, Print(1, 3, 0), 41, A); r1.stroke = 0; // muted, never restored
            l.BeginDraw(A); // the same frame drawn again: the earlier draw is abandoned
            Check(!l.IsMutedNow(&r1), "the abandoned mute is not part of the open draw");
            std::uint32_t got = 0;
            Check(l.Heal(&r1, Print(1, 3, 0), [&](std::uint32_t p) { got = p; r1.stroke = p; }) && got == 41 && l.healed == 1,
                  "healed with the recorded paint");
            Check(l.EndDraw(A, Set) == 0 && writes.empty(), "no stored pointer from the abandoned draw was written");
        }
        {   // An interrupted draw followed by a recomposition: the address now holds other wax.
            Ledger l; writes.clear();
            Wax r1{51};
            l.BeginDraw(A); l.Mute(&r1, &r1, Print(1, 3, 0), 51, A);
            l.BeginDraw(A);
            bool applied = false;
            Check(!l.Heal(&r1, Print(1, 4, 0), [&](std::uint32_t) { applied = true; }) && !applied && l.staleDropped == 1,
                  "a stale record is dropped, not applied");
            Check(l.Size() == 0, "stale record gone");
            Check(l.EndDraw(A, Set) == 0 && writes.empty(), "nothing written");
        }
        {   // A stale record is never written by another parcel's fallback either.
            Ledger l; writes.clear();
            Wax r1{61}, r2{62};
            l.BeginDraw(A); l.Mute(&r1, &r1, Print(1, 1, 0), 61, A);
            l.BeginDraw(A); // abandon
            l.BeginDraw(B); l.Mute(&r2, &r2, Print(1, 1, 0), 62, B);
            Check(l.EndDraw(B, Set) == 1 && writes.size() == 1 && writes[0].first == &r2, "only B's own record");
            Check(l.EndDraw(A, Set) == 0 && writes.size() == 1, "A's abandoned record untouched");
            Check(l.Size() == 1, "abandoned record waits for a heal or a drop");
        }
        {   // Heal refuses records of an open draw: those belong to the per-run restore.
            Ledger l;
            Wax r1{71};
            l.BeginDraw(A); l.Mute(&r1, &r1, Print(1, 1, 0), 71, A);
            Check(!l.Heal(&r1, Print(1, 1, 0), [](std::uint32_t) {}), "no heal inside the open draw");
        }
        {   // Caps.
            Ledger l;
            for (std::size_t i = 0; i < Ledger::kMaxOpen; ++i) Check(l.BeginDraw(reinterpret_cast<const void*>(i + 1)), "open");
            Check(!l.BeginDraw(reinterpret_cast<const void*>(Ledger::kMaxOpen + 7)) && l.overflow == 1, "open cap");
        }
        std::cout << "PASS: " << checks << " stroke-behind core checks; no host execution.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
