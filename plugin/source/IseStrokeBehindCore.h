//========================================================================================
// Route W core: SDK-free decisions and bookkeeping for the stroke-behind experiment.
//
// InDesign paints each wax run's Outside stroke and then that run's fill, run after run
// (PDF evidence: Tr 1 then Tr 0 per run), so a later run's stroke lands on an earlier
// run's fill. Adornment passes are frame-wide (TextDrawPriority.h): every run of a frame
// is visited in an earlier pass before any run is visited in the Text pass, where native
// text draws. Route W therefore:
//   1. in a pass before Text, strokes each eligible run's glyph outlines at twice the
//      stroke weight with the run's own stroke paint, then records the run's stroke paint
//      and sets it to None in the run's wax;
//   2. the Text pass draws that run's fill only, above every stroke of the frame;
//   3. in the Foreground pass, gives the run its stroke paint back.
// The document's attributes are never changed, only the composed wax between passes.
//
// Independence from the host's call protocol. The SDK does not promise when GetCouldDraw
// is asked or which passes Start/EndOfParcelDraw reach, so nothing here depends on them.
// Both adornments are asked about every run (the glue returns kFalse from
// GetCheckCouldDraw) and decide inside Draw. Recovery is a single rule, applied at the
// start of both passes: a run that is recorded, is the same wax (pointer and
// fingerprint), and currently carries no stroke paint gets its recorded paint back
// through the host's current pointer. If a Foreground visit never comes, the next draw's
// first visit restores the run before deciding anything. A stored pointer is never
// dereferenced: it is only compared.
//
// This header runs under the pure test shim.
//========================================================================================
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

namespace ise_stroke_behind {

// Mirrors IWaxRenderData::RenderMethod.
enum class Method { None, StrokeOnly, FillOnly, FillStroke };
// The text stroke alignments this code distinguishes.
enum class Alignment { Center, Outside, Other };

// Only Outside strokes move. A Center stroke is painted over its own fill by definition.
// A run whose stroke is None, zero, or non-finite is left to native drawing.
inline bool Eligible(Alignment alignment, double width, Method method)
{
    if (alignment != Alignment::Outside) return false;
    if (!(width > 0) || width > 10000) return false;
    return method == Method::FillStroke || method == Method::StrokeOnly;
}

// What identifies a run's wax between two passes. Addresses alone are not enough,
// because wax freed by a recomposition can be reallocated at the same address.
struct Fingerprint {
    std::uint32_t fillPaint = 0;
    std::int64_t widthMicro = 0;
    std::int32_t glyphCount = 0;
    std::int64_t xMicro = 0, yMicro = 0;
    bool operator==(const Fingerprint& o) const
    {
        return fillPaint == o.fillPaint && widthMicro == o.widthMicro && glyphCount == o.glyphCount &&
               xMicro == o.xMicro && yMicro == o.yMicro;
    }
};

class Ledger {
    struct Record {
        const void* run;
        const void* data;
        Fingerprint print;
        std::uint32_t strokePaint;
    };
    std::vector<Record> records;

    std::size_t Find(const void* run) const
    {
        for (std::size_t i = 0; i < records.size(); ++i) if (records[i].run == run) return i;
        return records.size();
    }
    void Erase(std::size_t i) { records.erase(records.begin() + static_cast<std::ptrdiff_t>(i)); }

public:
    static constexpr std::size_t kMaxRecords = 65536;
    std::size_t recovered = 0, staleDropped = 0, overflow = 0;

    // The run's recorded paint is given back (apply, through the host's current pointer)
    // only when the run is the recorded wax and currently has no stroke paint: then this
    // run is still muted by route W. A record for other wax at the same address is
    // dropped; one whose run already has a stroke paint (restored, or recomposed with a
    // new stroke) is dropped without writing anything.
    template <class Apply>
    bool Recover(const void* run, const void* data, const Fingerprint& print, bool strokeIsNone, Apply apply)
    {
        const std::size_t i = Find(run);
        if (i == records.size()) return false;
        if (records[i].data != data || !(records[i].print == print) || !strokeIsNone) {
            Erase(i); ++staleDropped;
            return false;
        }
        apply(records[i].strokePaint);
        Erase(i);
        ++recovered;
        return true;
    }

    // Records a mute. The caller must mute only when this returns true.
    bool Mute(const void* run, const void* data, const Fingerprint& print, std::uint32_t strokePaint)
    {
        const std::size_t i = Find(run);
        if (i != records.size()) { Erase(i); ++staleDropped; }
        if (records.size() >= kMaxRecords) { ++overflow; return false; }
        records.push_back({run, data, print, strokePaint});
        return true;
    }

    bool IsRecorded(const void* run) const { return Find(run) != records.size(); }
    std::size_t Size() const { return records.size(); }
};

inline Ledger& ThreadLedger()
{
    thread_local Ledger ledger;
    return ledger;
}

// A Center stroke stays native, but its miter join is muted to bevel for the Text pass and
// given back after it (route W's miter correction). The same bookkeeping applies: the
// recorded value is the run's join, and "muted" means the run's join is currently bevel.
inline bool CenterMiterEligible(Alignment alignment, double width, Method method)
{
    if (alignment != Alignment::Center) return false;
    if (!(width > 0) || width > 10000) return false;
    return method == Method::FillStroke || method == Method::StrokeOnly;
}

inline Ledger& ThreadJoinLedger()
{
    thread_local Ledger ledger;
    return ledger;
}

// Whether a run whose line maps to the pasteboard through the linear part [a b; c d] (x' = a x
// + c y, y' = b x + d y) is stroked in page space: when the map is not a similarity (it shears,
// or scales unevenly), since InDesign strokes such text in page space, at an even width. Sets
// the factor the stroke width is scaled by there: the square root of the map's area factor, 1
// for a shear, and the scale itself as the map nears a similarity, where the run is stroked in
// its own coordinates as before. False for a similarity or a singular map.
inline bool PageSpaceStroke(double a, double b, double c, double d, double* widthScale)
{
    const double det = a * d - b * c, x = a * a + b * b, y = c * c + d * d;
    if (!std::isfinite(det) || !std::isfinite(x) || !std::isfinite(y) || std::fabs(det) <= 1e-9) return false;
    const double scale = x > y ? x : y;
    if (std::fabs(x - y) <= 1e-6 * scale && std::fabs(a * c + b * d) <= 1e-6 * scale) return false;
    *widthScale = std::sqrt(std::fabs(det));
    return true;
}

// Whether a line's own transform within its frame, with linear part [a b; c d], only moves it:
// the lines of horizontal text. Page-space stroking (PageSpaceStroke) is limited to those, the
// lines the census measured; a line turned within its frame (vertical text) keeps the stroke in
// its own coordinates, as before.
inline bool OnlyMoves(double a, double b, double c, double d)
{
    const double e = 1e-9;
    return std::fabs(a - 1) <= e && std::fabs(b) <= e && std::fabs(c) <= e && std::fabs(d - 1) <= e;
}

} // namespace ise_stroke_behind
