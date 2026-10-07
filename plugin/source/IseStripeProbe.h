//========================================================================================
// What a stroke style's own stroker paints, measured: it draws a straight line into a port that
// only records what is drawn (RecordingPort), and the drawing is read off.
//
// Stripes (StripeRecorder): each stripe is read off as the band it paints across the line. A
// custom striped style keeps its stripes where no public interface reads them (only its stroker
// does), and drawn along the word's path by that stroker, its stripes left the letters and
// crossed in the counters (the owner's check, 2026-10-06); with the layout, custom stripes are
// filled as regions by distance from the fill, as the built-in styles are. Calibrated on the
// built-in Thick - Thin style, whose layout is known (BuiltInStripes): the side of the line its
// thick stripe falls on is the side away from the object.
//
// Marks (MarkRecorder): every paint, with its path, width, cap, join, and dash array, in the
// line's space, for laying a pattern along a path by runs between its real corners
// (ise_pattern, IsePatternCore.h).
//========================================================================================
#pragma once

#include "IGraphicsPort.h"
#include "IPathGeometry.h"
#include "IPathStroker.h"

#include "IsePatternCore.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

namespace ise_stripe_probe {

using ise_miter::Contour;
using ise_miter::Pt;
using ise_miter::Segment;

/** A graphics port that draws nothing: it keeps the current path in its own space (curves kept)
    and the stroke state, and hands each fill and stroke to Painted. Anything it cannot read (an
    AGM path, an image, text, a shading, a dash array without its values) marks the recording
    unreadable; a clip marks it clipped. Colors, transparency, and soft masks are ignored: the
    strokers it records paint every mark in the paint they are given (a stroke's gap color is
    painted by its caller). */
class RecordingPort : public IGraphicsPort
{
public:
    bool unreadable = false;

    // IPMUnknown: a stack object, not a boss; it offers no other interface.
    IPMUnknown* QueryInterface(PMIID iid) const override { return iid == IID_IGRAPHICSPORT ? const_cast<RecordingPort*>(this) : nil; }
    void AddRef() const override {}
    void Release() const override {}

    void Setup() override {}
    void newpath() override { fPath.clear(); }
    void moveto(const PMReal& x, const PMReal& y) override { Begin(At(x, y)); }
    void rmoveto(const PMReal& x, const PMReal& y) override { Begin(Last() + Delta(x, y)); }
    void lineto(const PMReal& x, const PMReal& y) override
    {
        Segment s;
        s.p = At(x, y);
        Extend(s);
    }
    void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2, const PMReal& x3, const PMReal& y3) override
    {
        Segment s;
        s.curve = true;
        s.c1 = At(x1, y1); s.c2 = At(x2, y2); s.p = At(x3, y3);
        Extend(s);
    }
    void curvetov(const PMReal& x2, const PMReal& y2, const PMReal& x3, const PMReal& y3) override
    {
        Segment s;
        s.curve = true;
        s.c1 = Last(); s.c2 = At(x2, y2); s.p = At(x3, y3);
        Extend(s);
    }
    void closepath() override { if (!fPath.empty()) fPath.back().closed = true; }
    // gsave keeps the current path too, as PostScript's does.
    void gsave() override { fStack.push_back({fState, fPath}); }
    void grestore() override
    {
        if (fStack.empty()) return;
        fState = fStack.back().first;
        fPath = fStack.back().second;
        fStack.pop_back();
    }
    void translate(const PMReal& x, const PMReal& y) override { Concat({1, 0, 0, 1, ToDouble(x), ToDouble(y)}); }
    void scale(const PMReal& x, const PMReal& y) override { Concat({ToDouble(x), 0, 0, ToDouble(y), 0, 0}); }
    void rotate(const PMReal& theta) override
    {
        const double a = ToDouble(theta) * 3.14159265358979323846 / 180, c = std::cos(a), s = std::sin(a);
        Concat({c, s, -s, c, 0, 0});
    }
    void concat(const PMMatrix& m) override
    {
        Concat({ToDouble(m[0]), ToDouble(m[1]), ToDouble(m[2]), ToDouble(m[3]), ToDouble(m[4]), ToDouble(m[5])});
    }
    void setlinewidth(const PMReal& width) override { fState.width = ToDouble(width); }
    void setdash(int32 numDash, float* dashes, float phase) override
    {
        if (numDash > 0 && dashes == nullptr) unreadable = true;
        fState.dash.clear();
        for (int32 i = 0; i < numDash && dashes != nullptr; ++i) fState.dash.push_back(dashes[i]);
        fState.phase = phase;
    }
    void setblendingmode(PMBlendingMode) override {}
    void starttransparencygroup(const PMRect&, AGMColorSpace*, bool16, bool16) override {}
    void endtransparencygroup() override {}
    void BeginGroup(const PMString, const_AIMDictionaryContainer*) override {}
    void EndGroup(const PMString) override {}
    void setopacity(const PMReal&, bool16) override {}
    void setlinecap(int32 cap) override { fState.cap = cap; }
    void setlinejoin(int32 join) override { fState.join = join; }
    void setmiterlimit(const PMReal&) override {}
    void setstrokeadjust(bool16) override {}
    void setgray(const PMReal&) override {}
    void setrgbcolor(const PMReal&, const PMReal&, const PMReal&) override {}
    void setcmykcolor(const PMReal&, const PMReal&, const PMReal&, const PMReal&) override {}
    void setlabcolor(const PMReal&, const PMReal&, const PMReal&) override {}
    void setcolorspace(AGMColorSpace*) override {}
    void setcolorvalues(const float*) override {}
    void setgradient(AGMGradient*, const PMMatrix&, const PMReal&, const PMReal&, const PMReal&, const PMReal&, const PMReal&,
                     const PMReal&) override {}
    void SetColorServer(AGMPaint*, bool16, const PMMatrix&) override {}
    void SetAlphaServer(AGMPaint*, bool16, const PMMatrix&) override {}
    void setoverprint(bool32) override {}
    void fill() override { Paint(false, false); }
    void eofill() override { Paint(false, true); }
    void stroke() override { Paint(true, false); }
    void image(_t_AGMImageRecord*, const PMMatrix&, int32) override { unreadable = true; }
    void image(_t_AGMImageRecord*, AGMColorSpace*, const PMMatrix&, int32) override { unreadable = true; }
    void image(AGMImage*, const PMMatrix&, int32) override { unreadable = true; }
    void show(const PMReal&, const PMReal&, uint32, const textchar*, TextGraphicsFlags, bool16, bool16) override { unreadable = true; }
    void xshow(const PMReal&, const PMReal&, uint32, const int32*, const Fixed*, const Fixed*, TextGraphicsFlags, bool16, bool16, uint32,
               const textchar*, K2Vector<OneOTFeature>*) override { unreadable = true; }
    void xshow(const PMReal&, const PMReal&, uint32, const int32*, const float*, const float*, TextGraphicsFlags, bool16, bool16, uint32,
               const textchar*, K2Vector<OneOTFeature>*) override { unreadable = true; }
    void ctshow(IFontInstance*, PMRealGlyphPoint*, uint32, TextGraphicsFlags, bool16) override { unreadable = true; }
    void selectfont(IPMFont*, const PMMatrix&) override {}
    void selectfont(IPMFont*, const PMReal&) override {}
    // Clips are ignored, and mark the recording: what a stroker clips would be misread.
    void clip() override { fClipped = true; }
    void eoclip() override { fClipped = true; }
    void strokepath_clip() override { fClipped = true; }
    void rectclip(const PMReal&, const PMReal&, const PMReal&, const PMReal&) override { fClipped = true; }
    void rectclip(const PMRect&) override { fClipped = true; }
    // rectfill paints a rectangle of its own and leaves the current path as it was.
    void rectfill(const PMReal& x, const PMReal& y, const PMReal& w, const PMReal& h) override
    {
        std::vector<Contour> kept;
        kept.swap(fPath);
        RectPath(ToDouble(x), ToDouble(y), ToDouble(w), ToDouble(h));
        Paint(false, false);
        fPath.swap(kept);
    }
    void rectfill(const PMRect& r) override { rectfill(r.Left(), r.Top(), r.Width(), r.Height()); }
    void rectstroke(const PMReal&, const PMReal&, const PMReal&, const PMReal&) override { unreadable = true; }
    void rectstroke(const PMRect&) override { unreadable = true; }
    void rectpath(const PMReal& x, const PMReal& y, const PMReal& w, const PMReal& h) override
    {
        RectPath(ToDouble(x), ToDouble(y), ToDouble(w), ToDouble(h));
    }
    void rectpath(const PMRect& r) override { rectpath(r.Left(), r.Top(), r.Width(), r.Height()); }
    void shadingfill(AGMPaint*) override { unreadable = true; }
    void AddComment(int32, void*, int32) override {}
    IDrawMgr* PortDrawMgr() override { return nil; }
    void BeginAuxInfoContext(GPortAuxInfoParam*) override {}
    void EndAuxInfoContext(GPortAuxInfoParam*) override {}
    void show(const CCTTextList&, int32, const PMMatrix*) const override { const_cast<RecordingPort*>(this)->unreadable = true; }
    void SetRenderingIntent(RenderingIntent) override {}
    void SetOverPrintMode(bool32) override {}
    void FillPath(AGMPath*) override { unreadable = true; }
    void StrokePath(AGMPath*) override { unreadable = true; }
    void ClipPath(AGMPath*) override { fClipped = true; }

    bool Clipped() const { return fClipped; }

protected:
    struct State {
        double m[6] = {1, 0, 0, 1, 0, 0};   // a b c d tx ty: x' = a x + c y + tx, y' = b x + d y + ty
        double width = 1;
        int cap = 0, join = 0;
        std::vector<double> dash;
        double phase = 0;
    };

    /** A fill (stroke false; even-odd or nonzero) or a stroke of the current path (fPath), in
        the state fState. */
    virtual void Painted(bool stroke, bool evenOdd) = 0;

    /** How the matrix scales lengths (the square root of its determinant's size). */
    double Scale() const { return std::sqrt(std::fabs(fState.m[0] * fState.m[3] - fState.m[1] * fState.m[2])); }

    /** Whether the matrix keeps shapes (no shear, the same scale both ways): a stroke's pen
        stays round, and its width and dashes scale by Scale(). */
    bool Similar() const
    {
        const double* m = fState.m;
        const double a = m[0] * m[0] + m[1] * m[1], d = m[2] * m[2] + m[3] * m[3], cross = m[0] * m[2] + m[1] * m[3];
        return std::fabs(a - d) <= 1e-9 * std::max(a, d) + 1e-12 && std::fabs(cross) <= 1e-9 * std::max(a, d) + 1e-12;
    }

    State fState;
    std::vector<Contour> fPath;

private:
    void Paint(bool stroke, bool evenOdd)
    {
        Painted(stroke, evenOdd);
        fPath.clear();
    }
    Pt At(const PMReal& x, const PMReal& y) const
    {
        const double px = ToDouble(x), py = ToDouble(y);
        const double* m = fState.m;
        return {m[0] * px + m[2] * py + m[4], m[1] * px + m[3] * py + m[5]};
    }
    Pt Delta(const PMReal& x, const PMReal& y) const
    {
        const double px = ToDouble(x), py = ToDouble(y);
        const double* m = fState.m;
        return {m[0] * px + m[2] * py, m[1] * px + m[3] * py};
    }
    /** The current point: after closepath, the closed subpath's start. */
    Pt Last() const
    {
        if (fPath.empty()) return {0, 0};
        const Contour& c = fPath.back();
        return c.closed || c.segments.empty() ? c.start : c.segments.back().p;
    }
    /** A new subpath at p; a moveto right after another replaces it, as it paints nothing. */
    void Begin(Pt p)
    {
        if (!fPath.empty() && fPath.back().segments.empty() && !fPath.back().closed) fPath.pop_back();
        Contour c;
        c.closed = false;
        c.start = p;
        fPath.push_back(c);
    }
    void Extend(const Segment& s)
    {
        // A segment without a current point starts a subpath at its end, as the recorder did.
        if (fPath.empty()) { Begin(s.p); return; }
        if (fPath.back().closed) Begin(Last());   // after closepath, a new subpath from its start
        fPath.back().segments.push_back(s);
    }
    void RectPath(double x, double y, double w, double h)
    {
        const double* m = fState.m;
        auto map = [m](double px, double py) { return Pt{m[0] * px + m[2] * py + m[4], m[1] * px + m[3] * py + m[5]}; };
        Contour c;
        c.start = map(x, y);
        for (const Pt& p : {Pt{x + w, y}, Pt{x + w, y + h}, Pt{x, y + h}}) {
            Segment s;
            s.p = map(p.x, p.y);
            c.segments.push_back(s);
        }
        c.closed = true;
        fPath.push_back(c);
    }
    void Concat(const double n[6])
    {
        // PostScript's concat: the new matrix applies first, then the current one.
        const double* m = fState.m;
        const double r[6] = {n[0] * m[0] + n[1] * m[2], n[0] * m[1] + n[1] * m[3], n[2] * m[0] + n[3] * m[2],
                             n[2] * m[1] + n[3] * m[3], n[4] * m[0] + n[5] * m[2] + m[4], n[4] * m[1] + n[5] * m[3] + m[5]};
        std::copy(r, r + 6, fState.m);
    }
    void Concat(std::initializer_list<double> n) { std::vector<double> v(n); Concat(v.data()); }

    std::vector<std::pair<State, std::vector<Contour>>> fStack;
    bool fClipped = false;
};

/** Records, in its own space, each stroked or filled band that crosses the vertical line
    x = probeX, as an interval of y. */
class StripeRecorder : public RecordingPort
{
public:
    explicit StripeRecorder(double probeX) : fProbeX(probeX) {}

    std::vector<std::pair<double, double>> bands;   // y intervals painted across x = probeX

protected:
    void Painted(bool stroke, bool) override
    {
        if (stroke) StrokeBands(); else FillBands();
    }

private:
    /** The current path flattened: each subpath's points (16 steps a curve, as before). */
    std::vector<std::vector<Pt>> Polylines() const
    {
        std::vector<std::vector<Pt>> out;
        for (const Contour& c : fPath) {
            std::vector<Pt> pts{c.start};
            Pt from = c.start;
            for (const Segment& s : c.segments) {
                if (s.curve) {
                    for (int k = 1; k <= 16; ++k) {
                        const double t = k / 16.0, r = 1 - t;
                        pts.push_back(from * (r * r * r) + s.c1 * (3 * r * r * t) + s.c2 * (3 * r * t * t) + s.p * (t * t * t));
                    }
                } else {
                    pts.push_back(s.p);
                }
                from = s.p;
            }
            if (c.closed) pts.push_back(c.start);
            out.push_back(pts);
        }
        return out;
    }
    // Where the path's segments cross x = probeX: their y values, sorted.
    std::vector<double> Crossings() const
    {
        std::vector<double> ys;
        for (const std::vector<Pt>& sub : Polylines())
            for (size_t i = 1; i < sub.size(); ++i) {
                const Pt a = sub[i - 1], b = sub[i];
                if ((a.x <= fProbeX) == (b.x <= fProbeX) || a.x == b.x) continue;
                ys.push_back(a.y + (fProbeX - a.x) * (b.y - a.y) / (b.x - a.x));
            }
        std::sort(ys.begin(), ys.end());
        return ys;
    }
    void FillBands()
    {
        // Even pairs of crossings bound the filled spans (exact for the simple bands stripes are).
        const std::vector<double> ys = Crossings();
        if (ys.size() % 2 != 0) { unreadable = true; return; }
        for (size_t i = 0; i + 1 < ys.size(); i += 2) bands.push_back({ys[i], ys[i + 1]});
    }
    void StrokeBands()
    {
        if (!fState.dash.empty()) { unreadable = true; return; }
        // A stroked line crossing x = probeX paints width / 2 either side of it, the width
        // scaled as the matrix scales across the line (the line is nearly horizontal).
        const double* m = fState.m;
        const double across = std::sqrt(m[2] * m[2] + m[3] * m[3]);
        for (const std::vector<Pt>& sub : Polylines())
            for (size_t i = 1; i < sub.size(); ++i) {
                const Pt a = sub[i - 1], b = sub[i];
                if ((a.x <= fProbeX) == (b.x <= fProbeX) || a.x == b.x) continue;
                const double y = a.y + (fProbeX - a.x) * (b.y - a.y) / (b.x - a.x), half = fState.width * across / 2;
                bands.push_back({y - half, y + half});
            }
    }

    double fProbeX;
};

/** Records every paint as a mark (ise_pattern::Mark): its path in the recorder's own space, and
    for a stroke its width, cap, join, and dash array scaled into that space. An even-odd fill,
    and a stroke through a matrix that does not keep shapes (its pen an ellipse), cannot be laid
    as recorded, and mark the recording unreadable. */
class MarkRecorder : public RecordingPort
{
public:
    std::vector<ise_pattern::Mark> marks;

protected:
    void Painted(bool stroke, bool evenOdd) override
    {
        if (fPath.empty()) return;
        if (evenOdd || (stroke && !Similar())) { unreadable = true; return; }
        ise_pattern::Mark m;
        m.stroke = stroke;
        const double scale = Scale();
        m.width = fState.width * scale;
        m.cap = fState.cap;
        m.join = fState.join;
        for (double d : fState.dash) m.dash.push_back(d * scale);
        m.phase = fState.phase * scale;
        m.path = fPath;
        marks.push_back(std::move(m));
    }
};

/** The union of the recorded bands, as sorted, disjoint intervals. */
inline std::vector<std::pair<double, double>> Merged(std::vector<std::pair<double, double>> bands)
{
    std::sort(bands.begin(), bands.end());
    std::vector<std::pair<double, double>> out;
    for (const auto& b : bands) {
        if (!(b.second > b.first)) continue;
        if (!out.empty() && b.first <= out.back().second + 1e-9) out.back().second = std::max(out.back().second, b.second);
        else out.push_back(b);
    }
    return out;
}

} // namespace ise_stripe_probe
