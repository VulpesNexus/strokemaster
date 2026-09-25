//========================================================================================
// Route W core: SDK-free decisions and bookkeeping for the stroke-behind experiment.
//
// InDesign paints each wax run's Outside stroke and then that run's fill, run after run
// (PDF evidence: Tr 1 then Tr 0 per run), so a later run's stroke lands on an earlier
// run's fill. Adornment passes are frame-wide (TextDrawPriority.h): every run of a frame
// is visited in the Background pass before any run is visited in the Text pass, where
// native text draws. Route W therefore:
//   1. Background pass: strokes each eligible run's glyph outlines at twice the stroke
//      weight with the run's own stroke paint, then records the run's stroke paint and
//      sets it to None for the rest of this draw;
//   2. Text pass: native text draws that run's fill only, above every stroke of the frame;
//   3. Foreground pass: restores the recorded stroke paint.
// The document is never changed: only the composed wax, and only between two passes.
//
// This header holds what can be decided without Adobe types, so it runs under the pure
// test shim: which runs are eligible, and the ledger of muted runs, kept per thread (one
// draw runs on one thread; export may run on another with its own copy of the document).
//
// Draws are keyed by parcel, because a frame anchored in text is drawn in the middle of
// its host frame's Text pass, and by a serial, because a draw can be interrupted.
// Pointer safety:
//  - The end-of-draw fallback restores only the ending parcel's records from this draw,
//    through their stored pointers; a parcel's wax cannot be recomposed mid-draw.
//  - A per-run restore writes through the host's current pointer, and only when it is the
//    recorded pointer with the recorded fingerprint.
//  - A record left by an interrupted draw is never dereferenced. It is healed only when
//    the host hands that run over again, through the host's current pointer, and only if
//    the fingerprint still matches; otherwise it is dropped as stale.
//========================================================================================
#pragma once
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
        void* data;
        Fingerprint print;
        std::uint32_t strokePaint;
        const void* parcel;
        std::uint64_t serial;
    };
    struct Open { const void* parcel; std::uint64_t serial; };
    std::vector<Record> records;
    std::vector<Open> open;
    std::uint64_t serial = 0;

    std::size_t Find(const void* run) const
    {
        for (std::size_t i = 0; i < records.size(); ++i) if (records[i].run == run) return i;
        return records.size();
    }
    std::uint64_t OpenSerial(const void* parcel) const
    {
        for (const auto& o : open) if (o.parcel == parcel) return o.serial;
        return 0;
    }
    bool IsOpen(const Record& r) const { return r.serial != 0 && OpenSerial(r.parcel) == r.serial; }
    void Erase(std::size_t i) { records.erase(records.begin() + static_cast<std::ptrdiff_t>(i)); }

public:
    static constexpr std::size_t kMaxRecords = 65536, kMaxOpen = 256;
    std::size_t staleDropped = 0, healed = 0, overflow = 0;

    // A parcel's Background pass starts. An unfinished earlier draw of the same parcel is
    // abandoned: its records become stale and will only ever be healed or dropped.
    bool BeginDraw(const void* parcel)
    {
        for (auto& o : open) if (o.parcel == parcel) { o.serial = ++serial; return true; }
        if (open.size() >= kMaxOpen) { ++overflow; return false; }
        open.push_back({parcel, ++serial});
        return true;
    }

    // If this run is still muted from an interrupted draw, give its stroke paint back
    // through the host's current pointer (apply) and forget the record. A record whose
    // fingerprint no longer matches is dropped without being applied.
    template <class Apply> bool Heal(const void* run, const Fingerprint& print, Apply apply)
    {
        const std::size_t i = Find(run);
        if (i == records.size() || IsOpen(records[i])) return false;
        if (!(records[i].print == print)) { Erase(i); ++staleDropped; return false; }
        apply(records[i].strokePaint);
        Erase(i);
        ++healed;
        return true;
    }

    // Records a mute made in this parcel's open draw. The caller must not mute a run this
    // returns false for.
    bool Mute(const void* run, void* data, const Fingerprint& print, std::uint32_t strokePaint, const void* parcel)
    {
        const std::uint64_t s = OpenSerial(parcel);
        if (!s) return false;
        const std::size_t i = Find(run);
        if (i != records.size()) {
            if (IsOpen(records[i])) return false; // already muted in an open draw
            Erase(i); ++staleDropped;
        }
        if (records.size() >= kMaxRecords) { ++overflow; return false; }
        records.push_back({run, data, print, strokePaint, parcel, s});
        return true;
    }

    bool IsMutedNow(const void* run) const
    {
        const std::size_t i = Find(run);
        return i != records.size() && IsOpen(records[i]);
    }

    // Restores one run muted in an open draw, through the host's current pointer, only
    // when it is the recorded pointer with the recorded fingerprint.
    template <class Apply> bool Restore(const void* run, const void* data, const Fingerprint& print, Apply apply)
    {
        const std::size_t i = Find(run);
        if (i == records.size() || !IsOpen(records[i])) return false;
        if (records[i].data != data || !(records[i].print == print)) return false;
        apply(records[i].strokePaint);
        Erase(i);
        return true;
    }

    // A parcel's Foreground pass ends: restore whatever of this draw is still muted,
    // through the stored pointers, and close the draw.
    template <class Setter> std::size_t EndDraw(const void* parcel, Setter set)
    {
        const std::uint64_t s = OpenSerial(parcel);
        std::size_t n = 0;
        if (s) {
            for (std::size_t i = 0; i < records.size();) {
                if (records[i].parcel != parcel || records[i].serial != s) { ++i; continue; }
                set(records[i].data, records[i].strokePaint);
                Erase(i);
                ++n;
            }
        }
        for (std::size_t i = 0; i < open.size(); ++i)
            if (open[i].parcel == parcel) { open.erase(open.begin() + static_cast<std::ptrdiff_t>(i)); break; }
        return n;
    }

    std::size_t Size() const { return records.size(); }
    std::size_t OpenDraws() const { return open.size(); }
};

inline Ledger& ThreadLedger()
{
    thread_local Ledger ledger;
    return ledger;
}

} // namespace ise_stroke_behind
