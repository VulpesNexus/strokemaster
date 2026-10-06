//========================================================================================
// The layout of a striped stroke style, measured: the style's own stroker draws a straight line
// (IPathStroker::StrokePathForUI, which strokes any path) into a port that only records what is
// drawn (StripeRecorder), and each stripe is read off as the band it paints across the line.
// A custom striped style keeps its stripes where no public interface reads them (only its
// stroker does), and drawn along the word's path by that stroker, its stripes left the letters
// and crossed in the counters (the owner's check, 2026-10-06); with the layout, custom stripes
// are filled as regions by distance from the fill, as the built-in styles are.
//
// Calibrated on the built-in Thick - Thin style, whose layout is known (BuiltInStripes): the side
// of the line its thick stripe falls on is the side away from the object.
//========================================================================================
#pragma once

#include "IGraphicsPort.h"
#include "IPathGeometry.h"
#include "IPathStroker.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

namespace ise_stripe_probe {

/** A graphics port that draws nothing: it records, in its own space, each stroked or filled
    band that crosses the vertical line x = probeX, as an interval of y. Anything it cannot read
    (an AGM path, an image, text) marks the recording unreadable. */
class StripeRecorder : public IGraphicsPort
{
public:
    explicit StripeRecorder(double probeX) : fProbeX(probeX) {}

    std::vector<std::pair<double, double>> bands;   // y intervals painted across x = probeX
    bool unreadable = false;

    // IPMUnknown: a stack object, not a boss; it offers no other interface.
    IPMUnknown* QueryInterface(PMIID iid) const override { return iid == IID_IGRAPHICSPORT ? const_cast<StripeRecorder*>(this) : nil; }
    void AddRef() const override {}
    void Release() const override {}

    void Setup() override {}
    void newpath() override { fPath.clear(); }
    void moveto(const PMReal& x, const PMReal& y) override { fPath.push_back({At(x, y)}); }
    void rmoveto(const PMReal& x, const PMReal& y) override
    {
        const Point last = Last();
        fPath.push_back({Add(last, Delta(x, y))});
    }
    void lineto(const PMReal& x, const PMReal& y) override { Extend(At(x, y)); }
    void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2, const PMReal& x3, const PMReal& y3) override
    {
        Curve(At(x1, y1), At(x2, y2), At(x3, y3));
    }
    void curvetov(const PMReal& x2, const PMReal& y2, const PMReal& x3, const PMReal& y3) override
    {
        Curve(Last(), At(x2, y2), At(x3, y3));
    }
    void closepath() override
    {
        if (!fPath.empty() && !fPath.back().empty()) fPath.back().push_back(fPath.back().front());
    }
    void gsave() override { fStack.push_back(fState); }
    void grestore() override
    {
        if (fStack.empty()) return;
        fState = fStack.back();
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
    void setdash(int32 numDash, float*, float) override { fState.dashed = numDash > 0; }
    void setblendingmode(PMBlendingMode) override {}
    void starttransparencygroup(const PMRect&, AGMColorSpace*, bool16, bool16) override {}
    void endtransparencygroup() override {}
    void BeginGroup(const PMString, const_AIMDictionaryContainer*) override {}
    void EndGroup(const PMString) override {}
    void setopacity(const PMReal&, bool16) override {}
    void setlinecap(int32) override {}
    void setlinejoin(int32) override {}
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
    void fill() override { FillBands(); fPath.clear(); }
    void eofill() override { FillBands(); fPath.clear(); }
    void stroke() override { StrokeBands(); fPath.clear(); }
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
    // Clips are ignored: a stroker that clipped its stripes would be misread, and is caught by
    // the calibration only if the built-in stroker clips too, so a clip marks the recording.
    void clip() override { fClipped = true; }
    void eoclip() override { fClipped = true; }
    void strokepath_clip() override { fClipped = true; }
    void rectclip(const PMReal&, const PMReal&, const PMReal&, const PMReal&) override { fClipped = true; }
    void rectclip(const PMRect&) override { fClipped = true; }
    void rectfill(const PMReal& x, const PMReal& y, const PMReal& w, const PMReal& h) override
    {
        RectPath(ToDouble(x), ToDouble(y), ToDouble(w), ToDouble(h));
        FillBands();
        fPath.clear();
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
    void show(const CCTTextList&, int32, const PMMatrix*) const override { const_cast<StripeRecorder*>(this)->unreadable = true; }
    void SetRenderingIntent(RenderingIntent) override {}
    void SetOverPrintMode(bool32) override {}
    void FillPath(AGMPath*) override { unreadable = true; }
    void StrokePath(AGMPath*) override { unreadable = true; }
    void ClipPath(AGMPath*) override { fClipped = true; }

    bool Clipped() const { return fClipped; }

private:
    struct Point { double x, y; };
    struct State {
        double m[6] = {1, 0, 0, 1, 0, 0};   // a b c d tx ty: x' = a x + c y + tx, y' = b x + d y + ty
        double width = 1;
        bool dashed = false;
    };

    Point At(const PMReal& x, const PMReal& y) const
    {
        const double px = ToDouble(x), py = ToDouble(y);
        const double* m = fState.m;
        return {m[0] * px + m[2] * py + m[4], m[1] * px + m[3] * py + m[5]};
    }
    Point Delta(const PMReal& x, const PMReal& y) const
    {
        const double px = ToDouble(x), py = ToDouble(y);
        const double* m = fState.m;
        return {m[0] * px + m[2] * py, m[1] * px + m[3] * py};
    }
    static Point Add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
    Point Last() const { return fPath.empty() || fPath.back().empty() ? Point{0, 0} : fPath.back().back(); }
    void Extend(Point p)
    {
        if (fPath.empty()) fPath.push_back({});
        fPath.back().push_back(p);
    }
    void Curve(Point c1, Point c2, Point p)
    {
        const Point p0 = Last();
        for (int k = 1; k <= 16; ++k) {
            const double t = k / 16.0, s = 1 - t;
            Extend({s * s * s * p0.x + 3 * s * s * t * c1.x + 3 * s * t * t * c2.x + t * t * t * p.x,
                    s * s * s * p0.y + 3 * s * s * t * c1.y + 3 * s * t * t * c2.y + t * t * t * p.y});
        }
    }
    void RectPath(double x, double y, double w, double h)
    {
        fPath.push_back({});
        for (const Point& p : {Point{x, y}, Point{x + w, y}, Point{x + w, y + h}, Point{x, y + h}, Point{x, y}}) {
            const double* m = fState.m;
            fPath.back().push_back({m[0] * p.x + m[2] * p.y + m[4], m[1] * p.x + m[3] * p.y + m[5]});
        }
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
    // Where the path's segments cross x = probeX: their y values, sorted.
    std::vector<double> Crossings() const
    {
        std::vector<double> ys;
        for (const std::vector<Point>& sub : fPath)
            for (size_t i = 1; i < sub.size(); ++i) {
                const Point a = sub[i - 1], b = sub[i];
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
        if (fState.dashed) { unreadable = true; return; }
        // A stroked line crossing x = probeX paints width / 2 either side of it, the width
        // scaled as the matrix scales across the line (the line is nearly horizontal).
        const double* m = fState.m;
        const double across = std::sqrt(m[2] * m[2] + m[3] * m[3]);
        for (const std::vector<Point>& sub : fPath)
            for (size_t i = 1; i < sub.size(); ++i) {
                const Point a = sub[i - 1], b = sub[i];
                if ((a.x <= fProbeX) == (b.x <= fProbeX) || a.x == b.x) continue;
                const double y = a.y + (fProbeX - a.x) * (b.y - a.y) / (b.x - a.x), half = fState.width * across / 2;
                bands.push_back({y - half, y + half});
            }
    }

    double fProbeX;
    State fState;
    std::vector<State> fStack;
    std::vector<std::vector<Point>> fPath;
    bool fClipped = false;
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
