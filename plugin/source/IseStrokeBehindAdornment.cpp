//========================================================================================
// Route W: native Outside text strokes drawn beneath every fill of their frame.
// EXPERIMENTAL; see IseStrokeBehindCore.h for the mechanism and its pointer-safety rules.
//
// Two global text adornments:
//   IseStrokeBehindAdornment  A pass before Text. For each eligible run (Outside stroke,
//                             nonzero weight, stroke paint present), strokes the run's glyph
//                             outlines at twice the weight with the run's own stroke
//                             rendering object, join, and miter limit, then records the
//                             run's stroke paint and sets it to [None] in the run's wax.
//                             A mitered run is stroked with bevel joins, and only its
//                             exterior corners get their miter joins back, filled, so no
//                             interior miter tip can leave a thin glyph (IseMiterCore.h). A mitered
//                             Center run stays native; only its join is muted to bevel.
//   IseStrokeRestoreAdornment Foreground pass. Restores each recorded run; the stroke
//                             adornment restores any run left muted before deciding. A
//                             Center run's miter joins are filled here first: the exterior
//                             ones as they are, the interior ones clipped to the run's fill,
//                             so only an interior miter's part outside the letter is lost.
// Native text then draws the muted runs as fill only, in the Text pass, above every stroke
// of the frame. Center strokes keep native drawing and order, joins aside.
//
// In a frame whose transform is not a similarity (sheared, or scaled unevenly as a kept
// percentage), InDesign strokes text in page space, at an even width: its PDF writes the
// frame's transform into the text matrix and the line width in page units (measured
// 2026-10-04). A path stroked in the run's own coordinates would follow the frame's shear and
// vary in width with direction (up to about 6% at 20 degrees). Such runs are drawn in page
// space instead (PageSpace): the outline placed through the frame's transform, the port's
// transform undone by as much, the width scaled by the transform's area factor (1 for a
// shear), and the corners found on the placed outline. Only lines the frame itself does not
// turn (horizontal text) are. Vertical runs are left to InDesign entirely (Draw). The
// document's attributes are never changed, so without this plugin the same document draws
// natively. The stroke is written as paths (the glyph fills stay native text), so exported
// PDF keeps its searchable text and fonts.
//
// Not yet established by host evidence: that the Text pass honors the muted paint on every
// output path (screen, PDF, print, EPS); that no other consumer reads or copies the wax
// between the passes (saving, a background export snapshot); that the Foreground pass
// visits every muted run (if not, the next draw restores it first); and strikethrough
// ordering relative to the moved strokes.
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "IDataBase.h"
#include "IFontInstance.h"
#include "IFontMgr.h"
#include "IGlobalTextAdornment.h"
#include "IGraphicsPort.h"
#include "IOutlineInfo.h"
#include "IPMFont.h"
#include "IParcelShape.h"
#include "IRenderingObject.h"
#include "ISwatchUtils.h"
#include "ITextOptions.h"
#include "IParcel.h"
#include "IWaxGlyphs.h"
#include "IWaxIterator.h"
#include "IWaxLine.h"
#include "IWaxRenderData.h"
#include "IWaxRun.h"
#include "IWaxStrand.h"

// General includes:
#include "AutoGSave.h"
#include "CPMUnknown.h"
#include "GraphicTypes.h"
#include "GraphicsData.h"
#include "PMMatrix.h"
#include "PMPoint.h"
#include "TextDrawPriority.h"
#include "Utils.h"

// Project includes:
#include "IseID.h"
#include "IseStrokeBehindCore.h"
#include "IseMiterCore.h"
#include "IseRegionCore.h"

#ifndef ISE_ROUTE_X
// Route W alone (build-plugin.ps1 -RouteW) reads no switches file: the defaults route X starts
// from, Close holes off (it needs the region code route X links). It did not build since the
// switches came in (the second review of 2026-11-01).
namespace ise_cross_frame {
inline bool TextTipsEnabled() { return true; }
inline bool CloseHolesEnabled() { return false; }
}
#endif
#ifdef ISE_ROUTE_X
#include "IseCrossFrame.h"
#endif

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

using namespace ise_stroke_behind;

// A fixed debugger diagnostic, as in the refusal adornment: no dialogs, files, or paths.
const char kIseStrokeBehindLoaded[] =
    "ISE_STROKE_BEHIND route=W experimental outside-strokes-beneath-frame-fills\n";

/** Writes a glyph outline into a graphics port, through a matrix and an offset.
    The run's glyphs are accumulated into one path and stroked once, so the transform is
    applied to the coordinates rather than concatenated into the port: restoring the port
    after each glyph would discard the segments just emitted. The offset carries the run's
    position within its wax line and is applied after the glyph matrix. */
class TransformedOutline : public IOutlineInfo
{
public:
    /** With a recorder, the same placed outline is also kept for the miter correction; the
        port receives exactly the same path either way. */
    explicit TransformedOutline(IGraphicsPort* port, ise_miter::Recorder* recorder = nullptr)
        : fPort(port), fRecorder(recorder), fX(0), fY(0) {}
    void SetMatrix(const PMMatrix& m) { fMatrix = m; }
    void SetOffset(const PMReal& x, const PMReal& y) { fX = x; fY = y; }
    virtual void BeginPath() {}
    virtual void EndPath() {}
    virtual void moveto(const PMReal& x, const PMReal& y)
    {
        PMReal a = x, b = y; Place(&a, &b); if (fPort) fPort->moveto(a, b);
        if (fRecorder) fRecorder->MoveTo(P(a, b));
    }
    virtual void lineto(const PMReal& x, const PMReal& y)
    {
        PMReal a = x, b = y; Place(&a, &b); if (fPort) fPort->lineto(a, b);
        if (fRecorder) fRecorder->LineTo(P(a, b));
    }
    virtual void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2,
                         const PMReal& x3, const PMReal& y3)
    {
        PMReal ax = x1, ay = y1, bx = x2, by = y2, cx = x3, cy = y3;
        Place(&ax, &ay); Place(&bx, &by); Place(&cx, &cy);
        if (fPort) fPort->curveto(ax, ay, bx, by, cx, cy);
        if (fRecorder) fRecorder->CurveTo(P(ax, ay), P(bx, by), P(cx, cy));
    }
    virtual void closepath() { if (fPort) fPort->closepath(); if (fRecorder) fRecorder->Close(); }
private:
    static ise_miter::Pt P(const PMReal& x, const PMReal& y) { return {ToDouble(x), ToDouble(y)}; }
    void Place(PMReal* x, PMReal* y) const { fMatrix.Transform(x, y); *x = *x + fX; *y = *y + fY; }
    IGraphicsPort* fPort;
    ise_miter::Recorder* fRecorder;
    PMMatrix fMatrix;
    PMReal fX, fY;
};

/** A run's glyph outlines, placed as route W draws them, written to a port, a recorder, or both. */
void WriteRun(TransformedOutline* writer, const IWaxRun* run, const IWaxGlyphs* glyphs, IFontInstance* instance)
{
    const int32 count = glyphs->GetGlyphCount();
    const Text::GlyphID* ids = glyphs->PeekGlyphArray();
    if (count <= 0 || ids == nil) return;
    writer->SetOffset(run->GetXPosition(), run->GetYPosition());
    const bool16 hasMatrix = glyphs->HasGlyphMatrix();
    for (int32 i = 0; i < count; ++i) {
        if (hasMatrix) {
            PMMatrix m;
            PMPoint pen(0, 0);
            if (glyphs->GetGlyphMatrix(i, &m, &pen)) {
                writer->SetMatrix(m);
                instance->GetOutlineInfo(writer, ids[i], pen.X(), pen.Y());
                continue;
            }
        }
        const PMMatrix where = glyphs->GetGlyphDrawPosition(i);
        writer->SetMatrix(PMMatrix());
        instance->GetOutlineInfo(writer, ids[i], where.GetXTranslation(), where.GetYTranslation());
    }
}

/** The linear part of a run's line-to-pasteboard transform when it is not a similarity (it
    shears, or scales unevenly), and the factor a stroke width is scaled by there: the square
    root of its area factor, which is 1 for a shear and, as the transform nears a similarity,
    its scale. False for a similarity (rotation, flip, even scale), whose runs are drawn as
    before, in their own coordinates, for a singular transform, and for a line its frame turns
    or scales (OnlyMoves: vertical text, not measured). */
bool PageSpace(const IWaxRun* run, PMMatrix* linear, double* widthScale)
{
    const IWaxLine* line = run ? run->GetWaxLine() : nil;
    if (line == nil) return false;
    const PMMatrix own = line->GetTransform();
    if (!OnlyMoves(ToDouble(own[0]), ToDouble(own[1]), ToDouble(own[2]), ToDouble(own[3]))) return false;
    const PMMatrix m = line->GetToPasteboardMatrix();
    if (!PageSpaceStroke(ToDouble(m[0]), ToDouble(m[1]), ToDouble(m[2]), ToDouble(m[3]), widthScale)) return false;
    *linear = PMMatrix(m[0], m[1], m[2], m[3], 0, 0);
    return true;
}

/** Contours placed through a linear transform. */
std::vector<ise_miter::Contour> Placed(const std::vector<ise_miter::Contour>& contours, const PMMatrix& linear)
{
    auto at = [&linear](ise_miter::Pt p) {
        PMReal x = p.x, y = p.y;
        linear.Transform(&x, &y);
        return ise_miter::Pt{ToDouble(x), ToDouble(y)};
    };
    std::vector<ise_miter::Contour> out = contours;
    for (ise_miter::Contour& c : out) {
        c.start = at(c.start);
        for (ise_miter::Segment& s : c.segments) {
            s.p = at(s.p);
            if (s.curve) { s.c1 = at(s.c1); s.c2 = at(s.c2); }
        }
    }
    return out;
}

void EmitPath(IGraphicsPort* port, const std::vector<ise_miter::Contour>& contours)
{
    for (const ise_miter::Contour& c : contours) {
        port->moveto(c.start.x, c.start.y);
        for (const ise_miter::Segment& s : c.segments) {
            if (s.curve) port->curveto(s.c1.x, s.c1.y, s.c2.x, s.c2.y, s.p.x, s.p.y);
            else port->lineto(s.p.x, s.p.y);
        }
        port->closepath();
    }
}

/** Fills the miter joins (ise_miter::MiterJoin) of the exterior or interior corners within the
    run's miter limit, for half width h; a corner past it stays beveled, as the native stroke
    draws it. Filled rather than restroked along the corner: InDesign's PDF port rounds
    coordinates to a thousandth of a point, which bent short restrokes beside tiny segments
    enough for renderers to disagree on the join (2026-10-04). Returns whether any was.
    `tips`: Trim glyph tips, on live text as on shapes (the owner, 2026-10-31): a glyph tip's miter
    within the limit is cut flush (TipReach), and past it, with `cutPast` (Outside and Center runs),
    cut flush at the stroke's reach instead of beveled, whatever the length of the edge beside it,
    as StrokeSideJoins draws a shape's (the bevel, made at the letter's point, left the tip
    wedged close to it: the owner's V5, 2026-10-31). */
bool FillJoins(IGraphicsPort* port, const std::vector<ise_miter::Stub>& stubs, bool interior, double h, double limit,
               bool tips = false, bool cutPast = false)
{
    bool any = false;
    port->newpath();
    for (const ise_miter::Stub& s : stubs) {
        if (s.interior != interior) continue;
        const bool tip = tips && !interior && ise_miter::TrimmedTip(s, h);
        double reach = s.ratio * h;
        if (s.ratio > limit) {
            if (!(tip && cutPast)) continue;
            reach = h;
        } else if (tips && !interior && ise_miter::NearTip(s, h)) {
            reach = std::min(reach, ise_miter::TipReach(s, h));
        }
        const std::vector<ise_miter::Pt> pts = ise_miter::Counterclockwise(ise_miter::JoinPolygon(s, h, reach));
        port->moveto(pts[0].x, pts[0].y);
        for (size_t i = 1; i < pts.size(); ++i) port->lineto(pts[i].x, pts[i].y);
        port->closepath();
        any = true;
    }
    if (any) port->fill(); else port->newpath();
    return any;
}

/** After the Text pass, for a Center run whose join was muted to bevel: give its corners their
    miter joins back, the exterior ones as they are and the interior ones clipped to the run's
    fill, so only the part of an interior miter outside the letter is not drawn. Joins are
    fills in the stroke's paint and overprint. */
bool RemoveInteriorMiters();   // below, with CorrectMiter

void DrawCenterStubs(GraphicsData* gd, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs,
                     IDataBase* db, int32 join)
{
    IGraphicsPort* port = gd ? gd->GetGraphicsPort() : nil;
    if (port == nil) return;
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    InterfacePtr<IPMFont> font(mutableData->QueryFont());
    InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
    if (font == nil || fontMgr == nil) return;
    InterfacePtr<IFontInstance> instance(fontMgr->QueryFontInstance(font, mutableData->GetFontMatrix()));
    InterfacePtr<IRenderingObject> stroke(db, mutableData->GetColorUID(kTrue), UseDefaultIID());
    if (instance == nil || stroke == nil) return;
    ise_miter::Recorder recorder;
    TransformedOutline writer(nullptr, &recorder);
    WriteRun(&writer, run, glyphs, instance);
    // In a sheared frame, the corners are found and filled in page space, where InDesign's
    // native Center stroke is even (PageSpace).
    PMMatrix linear;
    double widthScale = 1;
    const bool pageSpace = PageSpace(run, &linear, &widthScale);
    const std::vector<ise_miter::Contour> contours = pageSpace ? Placed(recorder.Contours(), linear) : recorder.Contours();
    ise_miter::Options options;
    options.halfWidth = ToDouble(mutableData->GetOutlineWidth()) / 2 * widthScale;
    const std::vector<ise_miter::Stub> stubs = ise_miter::Corners(contours, options, true);
    if (stubs.empty()) return;
    bool anyInterior = false, anyExterior = false;
    for (const ise_miter::Stub& s : stubs) (s.interior ? anyInterior : anyExterior) = true;
    if (join != kPMMiterJoin) return;
    AutoGSave save(port);
    if (pageSpace) port->concat(linear.Inverse());
    stroke->Install(port, mutableData, IRenderingObject::kInstallStroke);
    if (mutableData->GetOverprint(kTrue)) port->setoverprint(kTrue);
    const double limit = ToDouble(mutableData->GetMiterLimit());
    // Past the limit, a glyph tip is cut flush, not left to the bevel: on a letter that tapers to
    // a hairline, the bevel still reads as a point (the owner's S11, 2026-10-31).
    if (anyExterior) FillJoins(port, stubs, false, options.halfWidth, limit, ise_cross_frame::TextTipsEnabled(), true);
    if (anyInterior) {
        AutoGSave clipScope(port);
        if (RemoveInteriorMiters()) {
            port->newpath();
            EmitPath(port, contours);
            port->clip();
        }
        FillJoins(port, stubs, true, options.halfWidth, limit);
    }
}

struct HoleBounds { double x0, y0, x1, y1; GraphicsData* gd; const IWaxRun* run; };
bool HolesWithin(const std::vector<ise_miter::Contour>& contours, const HoleBounds& bounds, const PMMatrix& toLine)
{
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    auto inside = [&](ise_miter::Pt p) {
        PMReal x = p.x, y = p.y; toLine.Transform(&x, &y);
        x0 = std::min(x0, ToDouble(x)); y0 = std::min(y0, ToDouble(y));
        x1 = std::max(x1, ToDouble(x)); y1 = std::max(y1, ToDouble(y));
        return ToDouble(x) >= bounds.x0 && ToDouble(y) >= bounds.y0 && ToDouble(x) <= bounds.x1 && ToDouble(y) <= bounds.y1;
    };
    for (const auto& c : contours) {
        if (!inside(c.start)) return false;
        for (const auto& s : c.segments)
            if (!inside(s.p) || (s.curve && (!inside(s.c1) || !inside(s.c2)))) return false;
    }
    // Test the actual fill footprint, not the larger neighbor-search margin.
    // Control-point bounds contain each entire Bezier, including fitted overshoot.
#ifdef ISE_ROUTE_X
    return !contours.empty() && ise_cross_frame::SharedHoleAreaSafe(bounds.gd, bounds.run, x0, y0, x1, y1,
                                                                  bounds.x0, bounds.y0, bounds.x1, bounds.y1);
#else
    return true;
#endif
}

/** Close holes (the owner, 2026-10-31: "close all holes"): the silhouette of a stroke that
    reaches `reach` beyond `contours` (glyph outlines in the run's line coordinates;
    ise_region::Silhouette), filled beneath the stroke in the port's current paint, so every hole
    its outline encloses is filled: a letter's counters and the gaps that the strokes of touching
    letters close off. In a sheared frame, in page space, as the stroke is drawn. The holes are
    those the stroke's own `join` and miter `limit` enclose. */
void FillHoles(IGraphicsPort* port, const IWaxRun* run, const std::vector<ise_miter::Contour>& lineContours, double reach,
               int32 join, double limit, bool hollow, bool holesOnly = false, const HoleBounds* bounds = nullptr)
{
#ifndef ISE_ROUTE_X
    (void)port; (void)run; (void)lineContours; (void)reach; (void)join; (void)limit; (void)hollow; (void)holesOnly;
    return;
#else
    if (port == nil || !(reach > 0) || lineContours.empty()) return;
    PMMatrix linear;
    double widthScale = 1;
    const bool pageSpace = PageSpace(run, &linear, &widthScale);
    const std::vector<ise_miter::Contour> contours = pageSpace ? Placed(lineContours, linear) : lineContours;
    // Short of the stroke's edge by twice the tolerance, so the fitted curves never show past it.
    const double tol = 0.01;
    bool ok = false;
    const ise_region::BandJoin joined = join == kPMMiterJoin   ? ise_region::BandJoin::Miter
                                      : join == kPMRoundJoin ? ise_region::BandJoin::Round
                                                             : ise_region::BandJoin::Bevel;
    const std::vector<ise_miter::Contour> shape =
        ise_region::Silhouette(contours, std::max(0.0, reach * widthScale - 2 * tol), tol, &ok, joined, limit, holesOnly);
    if (!ok || shape.empty()) return;
    if (bounds && !HolesWithin(shape, *bounds, pageSpace ? linear.Inverse() : PMMatrix())) return;
    AutoGSave save(port);
    if (pageSpace) port->concat(linear.Inverse());
    // `hollow`: a run without a fill; its letters stay hollow, and only the holes are filled (the
    // silhouette took the letters' bodies too: the second review of 2026-11-01). The clip is the
    // outside of the letters' filled region, the same under either fill rule (OutsideClip).
    if (hollow) {
        const std::vector<ise_miter::Contour> region = ise_region::NonzeroRegion(contours, tol);
        const std::vector<ise_miter::Contour> outside =
            region.empty() ? region : ise_miter::OutsideClip(region, 2 * reach * widthScale + 10, tol, tol, true, false);
        if (outside.empty()) return;
        port->newpath();
        EmitPath(port, outside);
        port->clip();
    }
    port->newpath();
    EmitPath(port, shape);
    port->fill();
#endif
}

/** Close holes on one run: its own glyphs' silhouette (FillHoles). A hole closed only by another
    run's stroke stays open: FrameHoles takes the frame's runs together where it can. */
void FillSilhouette(IGraphicsPort* port, const IWaxRun* run, const IWaxGlyphs* glyphs, IFontInstance* instance, double reach,
                    int32 join, double limit, bool hollow = false)
{
    ise_miter::Recorder recorder;
    TransformedOutline writer(nullptr, &recorder);
    WriteRun(&writer, run, glyphs, instance);
    FillHoles(port, run, recorder.Contours(), reach, join, limit, hollow);
}

Alignment AlignmentOf(const IWaxRenderData* data);   // below
Method MethodOf(const IWaxRenderData* data, IDataBase* db);

/** The frame (parcel) whose stroke pass is being drawn, between StartOfParcelDraw and
    EndOfParcelDraw, and whether its holes were filled in this pass (FrameHoles). */
struct HolesPass {
    bool open = false;
    GraphicsData* gd = nullptr;   // the pass's, for the frames drawn before (NeighborOutlines)
    ParcelKey key;
    bool done = false;    // filled, for the whole frame, by its first run that strokes
    bool mixed = false;   // its runs' strokes differ: each run fills its own (FillSilhouette)
};
thread_local HolesPass tHoles;

/** A frame's stroke pass begins (GetIsActive, StartOfParcelDraw): its holes are not filled yet. */
void OpenHolesPass(GraphicsData* gd, const IParcelShape* parcel)
{
    tHoles = HolesPass();
    tHoles.gd = gd;
    InterfacePtr<IParcel> p(const_cast<IParcelShape*>(parcel), UseDefaultIID());
    if (p != nil) { tHoles.key = p->GetParcelKey(); tHoles.open = true; }
}

/** Close holes across a frame's lines (the owner's retest notes, 2026-11-01: on live text the
    gaps between two lines' words stayed open, where Illustrator closes them): the first run of the
    frame that strokes fills, beneath every stroke of the frame, the silhouette of all its runs
    whose strokes Close holes takes, when they are all alike (paint, overprint, weight, alignment,
    join, limit, and with or without a fill). True when the frame's holes are taken care of (now,
    or earlier in this pass); false when the run should fill its own (FillSilhouette): outside a
    frame's pass, when the strokes differ, or for a frame too large to take at once. Holes are
    closed across frames with matching strokes through NeighborOutlines. The fill is
    withheld only where the first run's stroke is from fills of the story painted earlier
    (ClipWithheld, applied by the caller). */
bool FrameHoles(IGraphicsPort* port, const IWaxRun* run, const IWaxRenderData* data, IDataBase* db, double reach, bool hollow)
{
#ifndef ISE_ROUTE_X
    (void)port; (void)run; (void)data; (void)db; (void)reach; (void)hollow;
    return false;
#else
    const IWaxLine* line = run ? run->GetWaxLine() : nil;
    if (line == nil) return false;
    const ParcelKey key = line->GetParcelKey();
    // Within the frame's pass (GetIsActive, StartOfParcelDraw): its first stroking run drawn fills
    // the frame. Without it, as InDesign's first screen draw of a document had it (the owner's
    // retest CC1, 2026-11-01: a toggle was still needed), the frame's first stroking run in text
    // order fills the frame, and every other run its own line (the caller's FillSilhouette).
    // STROKEMASTER_HOLES_UNBRACKETED (tests only): as if no pass were open, as on screen.
    static const bool unbracketed = [] { const wchar_t* v = _wgetenv(L"STROKEMASTER_HOLES_UNBRACKETED"); return v != nullptr && *v == L'1'; }();
    const bool bracketed = !unbracketed && tHoles.open && key == tHoles.key;
    if (bracketed && tHoles.mixed) return false;
    if (bracketed && tHoles.done) return true;
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    const UID paint = mutableData->GetColorUID(kTrue), none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    const bool16 overprint = mutableData->GetOverprint(kTrue);
    const double width = ToDouble(mutableData->GetOutlineWidth()), limit = ToDouble(mutableData->GetMiterLimit());
    const int32 join = mutableData->GetOutlineJoin();
    const Alignment alignment = AlignmentOf(data);
    InterfacePtr<IWaxStrand> strand(line->QueryWaxStrand());
    if (strand == nil) return false;
    std::unique_ptr<const IWaxIterator> it(strand->NewReadOnlyWaxIterator());
    if (it == nullptr) return false;
    // The frame's first line: its lines are consecutive in the story.
    const int kMaxLines = 400;
    const IWaxLine* first = it->GetFirstWaxLine(line->TextOrigin());
    if (first == nil) return false;
    TextIndex start = first->TextOrigin();
    for (int n = 0; n < kMaxLines; ++n) {
        const IWaxLine* back = it->GetPreviousWaxLine();
        if (back == nil || !(back->GetParcelKey() == key)) break;
        start = back->TextOrigin();
    }
    if (!bracketed) {
        // Told apart by where they start in the story, not by pointer: the run being drawn and
        // the one its line hands out need not be the same object.
        const TextIndex here = line->TextOrigin(run);
        bool found = false, first = false;
        int scanned = 0;
        for (const IWaxLine* l = it->GetFirstWaxLine(start); l != nil && l->GetParcelKey() == key && !found && ++scanned <= kMaxLines;
             l = it->GetNextWaxLine()) {
            for (int32 i = 0, n = l->GetRunCount(); i < n; ++i) {
                InterfacePtr<IWaxRun> r(l->QueryRun(i));
                InterfacePtr<IWaxRenderData> d(r, IID_IWAXRENDERDATA);
                if (r == nil || d == nil || r->IsVerticalWritingMode()) continue;
                const Alignment a = AlignmentOf(d);
                const double w = ToDouble(d->GetOutlineWidth());
                const Method m = MethodOf(d, db);
                if (CenterMiterEligible(a, w, m) || Eligible(a, w, m)) { found = true; first = l->TextOrigin(r) == here; break; }
            }
        }
        if (!first) return false;
    }
    InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
    if (fontMgr == nil) return false;
    const PMMatrix toLine = line->GetToPasteboardMatrix().Inverse();
    std::vector<ise_miter::Contour> all;
    size_t segments = 0;
    auto mix = [&] { if (bracketed) tHoles.mixed = true; return false; };
    int lines = 0;
    for (const IWaxLine* l = it->GetFirstWaxLine(start); l != nil && l->GetParcelKey() == key; l = it->GetNextWaxLine()) {
        if (++lines > kMaxLines) return mix();
        const PMMatrix toPb = l->GetToPasteboardMatrix();
        for (int32 i = 0, n = l->GetRunCount(); i < n; ++i) {
            InterfacePtr<IWaxRun> r(l->QueryRun(i));
            InterfacePtr<IWaxRenderData> d(r, IID_IWAXRENDERDATA);
            InterfacePtr<IWaxGlyphs> g(r, IID_IWAXGLYPHS);
            if (r == nil || d == nil || g == nil) return mix();
            if (r->IsVerticalWritingMode()) continue;   // left to InDesign (Draw)
            const Alignment a = AlignmentOf(d);
            const double w = ToDouble(d->GetOutlineWidth());
            const Method m = MethodOf(d, db);
            if (!CenterMiterEligible(a, w, m) && !Eligible(a, w, m)) continue;   // no stroke this route draws
            // Its stroke as the first run's: anything else, and each run fills its own.
            if (d->GetColorUID(kTrue) != paint || d->GetOverprint(kTrue) != overprint || a != alignment ||
                std::fabs(ToDouble(d->GetTint(kTrue)) - ToDouble(mutableData->GetTint(kTrue))) > 1e-6 ||
                std::fabs(w - width) > 1e-6 || d->GetOutlineJoin() != join || std::fabs(ToDouble(d->GetMiterLimit()) - limit) > 1e-6 ||
                (d->GetColorUID(kFalse) == none) != hollow)
                return mix();
            InterfacePtr<IPMFont> font(d->QueryFont());
            InterfacePtr<IFontInstance> instance(font ? fontMgr->QueryFontInstance(font, d->GetFontMatrix()) : nil);
            if (instance == nil) return mix();
            ise_miter::Recorder recorder;
            TransformedOutline writer(nullptr, &recorder);
            WriteRun(&writer, r, g, instance);
            std::vector<ise_miter::Contour> contours = recorder.Contours();
            // Another line's outline in this run's line coordinates, through the pasteboard.
            if (l != line) {
                auto at = [&](ise_miter::Pt q) {
                    PMReal x = q.x, y = q.y;
                    toPb.Transform(&x, &y);
                    toLine.Transform(&x, &y);
                    return ise_miter::Pt{ToDouble(x), ToDouble(y)};
                };
                for (ise_miter::Contour& c : contours) {
                    c.start = at(c.start);
                    for (ise_miter::Segment& s : c.segments) {
                        s.p = at(s.p);
                        if (s.curve) { s.c1 = at(s.c1); s.c2 = at(s.c2); }
                    }
                }
            }
            for (const ise_miter::Contour& c : contours) segments += c.segments.size();
            if (segments > 200000) return mix();
            all.insert(all.end(), contours.begin(), contours.end());
        }
    }
    if (bracketed) tHoles.done = true;
    FillHoles(port, run, all, reach, join, limit, hollow);
    // Holes closed together with other frames' strokes (the owner's retest BB1, 2026-11-01): those
    // frames' glyphs painted earlier in this traversal, with strokes alike, are taken with the
    // frame's, and only the holes are filled, so nothing is painted over a glyph already drawn.
    if (!all.empty()) {
        double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
        for (const ise_miter::Contour& c : all) {
            x0 = std::min(x0, c.start.x); x1 = std::max(x1, c.start.x); y0 = std::min(y0, c.start.y); y1 = std::max(y1, c.start.y);
            for (const ise_miter::Segment& s : c.segments) {
                x0 = std::min(x0, s.p.x); x1 = std::max(x1, s.p.x); y0 = std::min(y0, s.p.y); y1 = std::max(y1, s.p.y);
            }
        }
        const double margin = 2 * reach * std::max(1.0, limit) + 1;
        std::vector<ise_region::StrokeField> nearby = ise_cross_frame::NeighborOutlines(tHoles.gd, run, x0 - margin, y0 - margin, x1 + margin,
                                                                                  y1 + margin, reach, paint.Get());
        if (!nearby.empty() && !overprint) {
            // SharedHolesSafe refuses unsupported/foreign earlier text under the output.
            // Refuse output outside the checked area geometrically; adding a graphics
            // port clip here changes antialiased edge compositing in PDF.
            const HoleBounds sharedBounds{x0 - margin, y0 - margin, x1 + margin, y1 + margin, tHoles.gd, run};
            const auto joined = join == kPMMiterJoin ? ise_region::BandJoin::Miter :
                                join == kPMRoundJoin ? ise_region::BandJoin::Round : ise_region::BandJoin::Bevel;
            bool uniform = true;
            for (const auto& f : nearby) if (std::fabs(f.grow - reach) > 1e-6 || f.join != joined ||
                                             std::fabs(f.limit - limit) > 1e-6) uniform = false;
            if (uniform) {
                auto combined = all;
                for (const auto& f : nearby) combined.insert(combined.end(), f.contours.begin(), f.contours.end());
                FillHoles(port, run, combined, reach, join, limit, hollow, true, &sharedBounds);
            } else if (!hollow) {
                nearby.push_back({all, reach, joined, limit});
                PMMatrix linear;
                double widthScale = 1;
                const bool pageSpace = PageSpace(run, &linear, &widthScale);
                for (auto& f : nearby) {
                    if (pageSpace) f.contours = Placed(f.contours, linear);
                    f.grow = std::max(0.0, f.grow * widthScale - 0.02);
                }
                bool ok = false;
                const auto holes = ise_region::SharedStrokeHoles(nearby, 0.01, &ok);
                if (ok && !holes.empty() && HolesWithin(holes, sharedBounds, pageSpace ? linear.Inverse() : PMMatrix())) {
                    AutoGSave save(port);
                    if (pageSpace) port->concat(linear.Inverse());
                    port->newpath(); EmitPath(port, holes); port->fill();
                }
            }
        }
    }
    return true;
#endif
}

/** Whether route W draws this run's miter joins itself (always, outside the research build): for
    No miter spikes in live text, or for Trim glyph tips alone, which trims tips only there (it did
    nothing on live text with No miter spikes off: the review of 2026-11-01). */
bool CorrectMiter(int32 join)
{
    if (join != kPMMiterJoin) return false;
#ifdef ISE_ROUTE_X
    return ise_cross_frame::MiterCorrectionEnabled() || ise_cross_frame::TextTipsEnabled();
#else
    return true;
#endif
}

/** Whether the interior miters are removed (No miter spikes in live text); otherwise, drawn whole as
    InDesign draws them, when the joins are drawn for Trim glyph tips alone. */
bool RemoveInteriorMiters()
{
#ifdef ISE_ROUTE_X
    return ise_cross_frame::MiterCorrectionEnabled();
#else
    return true;
#endif
}

Alignment AlignmentOf(const IWaxRenderData* data)
{
    switch (data->GetStrokeAlignment()) {
    case kStrokeAlignCenter: return Alignment::Center;
    case kStrokeAlignOutside: return Alignment::Outside;
    default: return Alignment::Other;
    }
}

Method MethodOf(const IWaxRenderData* data, IDataBase* db)
{
    switch (data->GetRenderMethod(db, Utils<ISwatchUtils>()->GetNoneSwatchUID(db))) {
    case IWaxRenderData::kStrokeOnly: return Method::StrokeOnly;
    case IWaxRenderData::kFillOnly: return Method::FillOnly;
    case IWaxRenderData::kFillStroke: return Method::FillStroke;
    default: return Method::None;
    }
}

std::int64_t Micro(const PMReal& value)
{
    const double v = ToDouble(value) * 1e6;
    return std::isfinite(v) ? static_cast<std::int64_t>(std::llround(v)) : 0;
}

Fingerprint PrintOf(const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs)
{
    Fingerprint f;
    f.fillPaint = data->GetColorUID(kFalse).Get();
    f.widthMicro = Micro(data->GetOutlineWidth());
    f.glyphCount = glyphs ? glyphs->GetGlyphCount() : 0;
    f.xMicro = Micro(run->GetXPosition());
    f.yMicro = Micro(run->GetYPosition());
    return f;
}

// Gives a run back its stroke paint if route W left it muted (the Foreground visit of an
// earlier draw did not happen, or this is that visit), through the host's current pointer.
void Recover(const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db)
{
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    const bool strokeIsNone = data->GetColorUID(kTrue) == Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    ThreadLedger().Recover(run, data, PrintOf(run, data, glyphs), strokeIsNone,
                           [&](std::uint32_t paint) { mutableData->SetColorUID(kTrue, UID(paint)); });
}

// Gives a Center run back its miter join if route W left it muted to bevel. With gd (the
// pass after Text), its miter joins are filled first; without (a later draw's first visit),
// the join is only given back.
void RecoverJoin(const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db, GraphicsData* gd)
{
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    const bool muted = data->GetOutlineJoin() == kPMBevelJoin;
    ThreadJoinLedger().Recover(run, data, PrintOf(run, data, glyphs), muted, [&](std::uint32_t join) {
        if (gd != nil) DrawCenterStubs(gd, run, data, glyphs, db, static_cast<int32>(join));
        mutableData->SetOutlineJoin(static_cast<int32>(join));
    });
}

} // namespace

/** Pass before Text: draws eligible runs' strokes and mutes them for the Text pass.
    Asked about every run (GetCheckCouldDraw is kFalse) and decides in Draw, because the
    SDK promises nothing about when GetCouldDraw is asked. */
class IseStrokeBehindAdornment : public CPMUnknown<IGlobalTextAdornment>
{
public:
    explicit IseStrokeBehindAdornment(IPMUnknown* boss) : CPMUnknown<IGlobalTextAdornment>(boss)
    {
        OutputDebugStringA(kIseStrokeBehindLoaded);
    }
    /** Background + 0.95 truncates to a pass of its own (DrawPriority::GetPass), after the
        frame's Background pass (shading, underlines) and before its Text pass. */
    virtual Text::DrawPriority GetDrawPriority() { return Text::DrawPriority(Text::kTAPassPriBackground + 0.95); }
    virtual bool16 GetCheckIsActive() { return kTrue; }
#ifdef ISE_ROUTE_X
    /** Route X also learns here which parcel (frame, story) the pass is about to draw, and Close
        holes' pass opens here too: a screen draw that never called StartOfParcelDraw left each
        line to fill its own (the owner's retest BB1, 2026-11-01). */
    virtual bool16 GetIsActive(const IParcelShape* parcel, const ITextOptions*, int32 flags)
    {
        if (parcel != nil) ise_cross_frame::NoteParcel(parcel, 'W', flags);
        OpenHolesPass(nullptr, parcel);
        return parcel != nil;
    }
#else
    virtual bool16 GetIsActive(const IParcelShape* parcel, const ITextOptions*, int32) { return parcel != nil; }
#endif
    virtual bool16 GetCheckCouldDraw() { return kFalse; }
    virtual bool16 GetCouldDraw(const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) { return kTrue; }
    /** Cross-frame holes can extend beyond the run's native stroke bounds on screen. */
    virtual bool16 GetHasInkBounds() { return kTrue; }
    virtual void GetInkBounds(PMRect* bounds, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs)
    {
#ifdef ISE_ROUTE_X
        if (!ise_cross_frame::CloseHolesEnabled() || run == nil || data == nil || glyphs == nil || run->IsVerticalWritingMode()) return;
        const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(run->GetDataBase());
        if (data->GetColorUID(kTrue) == none) return;
        const double width = ToDouble(data->GetOutlineWidth());
        if (!(width > 0)) return;
        const double margin = 2 * width * std::max(1.0, ToDouble(data->GetMiterLimit())) + 1;
        const PMRect ink = glyphs->GetStrokeBoundingBox();
        const double x = ToDouble(run->GetXPosition()), y = ToDouble(run->GetYPosition());
        *bounds = PMRect(ToDouble(ink.Left()) + x - margin, ToDouble(ink.Top()) + y - margin,
                         ToDouble(ink.Right()) + x + margin, ToDouble(ink.Bottom()) + y + margin);
#else
        (void)bounds; (void)run; (void)data; (void)glyphs;
#endif
    }
#ifdef ISE_ROUTE_X
    virtual void StartOfParcelDraw(GraphicsData* gd, int32 flags, const IParcelShape* parcel)
    {
        if (parcel != nil) ise_cross_frame::NoteParcel(parcel, 'w', flags);
        // The frame's stroke pass begins: its holes are not filled yet (FrameHoles).
        OpenHolesPass(gd, parcel);
    }
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) { tHoles = HolesPass(); ise_cross_frame::EndParcel(); }
#else
    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
#endif
    virtual void Draw(GraphicsData* gd, int32 flags, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs);
};

CREATE_PMINTERFACE(IseStrokeBehindAdornment, kIseStrokeBehindAdornmentImpl)

void IseStrokeBehindAdornment::Draw(GraphicsData* gd, int32, const IWaxRun* run, const IWaxRenderData* data,
                                    const IWaxGlyphs* glyphs)
{
    if (gd == nil || run == nil || data == nil || glyphs == nil) return;
    IDataBase* db = run->GetDataBase();
    if (db == nil) return;
    tHoles.gd = gd;   // Close holes across frames asks the traversal through it (FrameHoles)
    Recover(run, data, glyphs, db);
    RecoverJoin(run, data, glyphs, db, nil);
    // Vertical text stays InDesign's: its glyphs are placed and turned by the run in ways the
    // outline written here does not follow, and its strokes landed beside and across the
    // glyphs (the owner's vertical frame, 2026-10-06).
    if (run->IsVerticalWritingMode()) return;
#ifdef ISE_ROUTE_X
    ise_cross_frame::TraceRun('W', gd, run, data, glyphs, db, "visit");
    // Every fill this parcel is about to paint, stroked or not (route X; a no-op otherwise).
    ise_cross_frame::RecordPendingFill(gd, run, data, glyphs, db);
    if (!ise_cross_frame::RouteWEnabled()) return;
#endif
    const Alignment alignment = AlignmentOf(data);
    const double width = ToDouble(data->GetOutlineWidth());
    if (CenterMiterEligible(alignment, width, MethodOf(data, db))) {
        // A mitered Center stroke stays native and in InDesign's order; only its join is muted
        // to bevel for the Text pass, and its miter joins are filled after it (RecoverJoin).
        const int32 join = data->GetOutlineJoin();
        // Close holes: the silhouette of the stroke's outer half, beneath it, in its paint.
        if (ise_cross_frame::CloseHolesEnabled()) {
            IGraphicsPort* port = gd->GetGraphicsPort();
            IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
            InterfacePtr<IPMFont> font(mutableData->QueryFont());
            InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
            InterfacePtr<IFontInstance> instance(font && fontMgr ? fontMgr->QueryFontInstance(font, mutableData->GetFontMatrix()) : nil);
            InterfacePtr<IRenderingObject> paint(db, mutableData->GetColorUID(kTrue), UseDefaultIID());
            if (port != nil && instance != nil && paint != nil) {
                AutoGSave save(port);
#ifdef ISE_ROUTE_X
                ise_cross_frame::ClipWithheld(gd, port, run, data, glyphs, db);
#endif
                paint->Install(port, mutableData, IRenderingObject::kInstallStroke);
                if (mutableData->GetOverprint(kTrue)) port->setoverprint(kTrue);
                const bool hollow = mutableData->GetColorUID(kFalse) == Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
                if (!FrameHoles(port, run, data, db, width / 2, hollow))
                    FillSilhouette(port, run, glyphs, instance, width / 2, join, ToDouble(mutableData->GetMiterLimit()), hollow);
            }
        }
        if (CorrectMiter(join) && ThreadJoinLedger().Mute(run, data, PrintOf(run, data, glyphs), static_cast<std::uint32_t>(join))) {
            const_cast<IWaxRenderData*>(data)->SetOutlineJoin(kPMBevelJoin);
        }
        return;
    }
    if (!Eligible(alignment, width, MethodOf(data, db))) return;
    IGraphicsPort* port = gd->GetGraphicsPort();
    const int32 count = glyphs->GetGlyphCount();
    const Text::GlyphID* ids = glyphs->PeekGlyphArray();
    if (port == nil || count <= 0 || ids == nil) return;
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    InterfacePtr<IPMFont> font(mutableData->QueryFont());
    InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
    if (font == nil || fontMgr == nil) return;
    InterfacePtr<IFontInstance> instance(fontMgr->QueryFontInstance(font, mutableData->GetFontMatrix()));
    const UID strokePaint = mutableData->GetColorUID(kTrue);
    InterfacePtr<IRenderingObject> stroke(db, strokePaint, UseDefaultIID());
    const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    if (instance == nil || stroke == nil || none == kInvalidUID) return;
    // Record before drawing: if the record cannot be kept, draw nothing and leave the run
    // to native drawing, rather than draw a stroke that native text would draw again.
    if (!ThreadLedger().Mute(run, data, PrintOf(run, data, glyphs), strokePaint.Get())) return;
    {
        AutoGSave save(port);
#ifdef ISE_ROUTE_X
        // Withhold the stroke from fills of the same story painted earlier in this traversal.
        ise_cross_frame::ClipWithheld(gd, port, run, data, glyphs, db);
#endif
        stroke->Install(port, mutableData, IRenderingObject::kInstallStroke);
        // Install does not carry the run's stroke overprint, and the PDF port wrote this stroke
        // without it (measured 2026-10-04): it is set explicitly, for the stroke and its joins.
        if (mutableData->GetOverprint(kTrue)) port->setoverprint(kTrue);
        // Close holes: the silhouette of the whole Outside stroke, beneath it, in its paint.
        if (ise_cross_frame::CloseHolesEnabled() && !FrameHoles(port, run, data, db, width, mutableData->GetColorUID(kFalse) == none))
            FillSilhouette(port, run, glyphs, instance, width, mutableData->GetOutlineJoin(), ToDouble(mutableData->GetMiterLimit()),
                           mutableData->GetColorUID(kFalse) == none);
        // Outside alignment is drawn by InDesign as a centered stroke of twice the weight
        // beneath the fill; reproduce that geometry explicitly. A mitered run is stroked with
        // bevel joins first, and its exterior corners get their miters back below (see
        // IseMiterCore.h): the interior miters, whose tips can leave a thin glyph, are not drawn.
        const int32 join = mutableData->GetOutlineJoin();
        const bool correctMiter = CorrectMiter(join);
        PMMatrix linear;
        double widthScale = 1;
        if (PageSpace(run, &linear, &widthScale)) {
            // A sheared frame: the run's outline placed in page space and stroked there, at an
            // even width, as InDesign strokes it (PageSpace).
            ise_miter::Recorder recorder;
            TransformedOutline writer(nullptr, &recorder);
            WriteRun(&writer, run, glyphs, instance);
            const std::vector<ise_miter::Contour> contours = Placed(recorder.Contours(), linear);
            const double h = ToDouble(mutableData->GetOutlineWidth()) * widthScale;
            port->concat(linear.Inverse());
            port->setlinewidth(2 * h);
            port->setlinejoin(correctMiter ? kPMBevelJoin : join);
            port->setmiterlimit(mutableData->GetMiterLimit());
            port->newpath();
            EmitPath(port, contours);
            port->stroke();
            if (correctMiter) {
                ise_miter::Options options;
                options.halfWidth = h;
                const std::vector<ise_miter::Stub> stubs = ise_miter::Corners(contours, options, !RemoveInteriorMiters());
                if (!stubs.empty()) {
                    FillJoins(port, stubs, false, h, ToDouble(mutableData->GetMiterLimit()), ise_cross_frame::TextTipsEnabled(),
                              alignment == Alignment::Outside);
                    if (!RemoveInteriorMiters()) FillJoins(port, stubs, true, h, ToDouble(mutableData->GetMiterLimit()));
                }
            }
        } else {
            port->setlinewidth(mutableData->GetOutlineWidth() * 2);
            port->setlinejoin(correctMiter ? kPMBevelJoin : join);
            port->setmiterlimit(mutableData->GetMiterLimit());
            ise_miter::Recorder recorder;
            TransformedOutline writer(port, correctMiter ? &recorder : nullptr);
            writer.SetOffset(run->GetXPosition(), run->GetYPosition());
            const bool16 hasMatrix = glyphs->HasGlyphMatrix();
            port->newpath();
            for (int32 i = 0; i < count; ++i) {
                if (hasMatrix) {
                    PMMatrix m;
                    PMPoint pen(0, 0);
                    if (glyphs->GetGlyphMatrix(i, &m, &pen)) {
                        writer.SetMatrix(m);
                        instance->GetOutlineInfo(&writer, ids[i], pen.X(), pen.Y());
                        continue;
                    }
                }
                const PMMatrix where = glyphs->GetGlyphDrawPosition(i);
                writer.SetMatrix(PMMatrix());
                instance->GetOutlineInfo(&writer, ids[i], where.GetXTranslation(), where.GetYTranslation());
            }
            port->stroke();
            if (correctMiter) {
                ise_miter::Options options;
                options.halfWidth = ToDouble(mutableData->GetOutlineWidth());
                const std::vector<ise_miter::Stub> stubs = ise_miter::Corners(recorder.Contours(), options, !RemoveInteriorMiters());
                if (!stubs.empty()) {
                    // The exterior corners' miter joins, within the run's limit as the native stroke
                    // draws them, filled in the stroke's paint and overprint; the interior ones too,
                    // whole, when the joins are drawn for Trim glyph tips alone.
                    FillJoins(port, stubs, false, options.halfWidth, ToDouble(mutableData->GetMiterLimit()),
                              ise_cross_frame::TextTipsEnabled(), alignment == Alignment::Outside);
                    if (!RemoveInteriorMiters()) FillJoins(port, stubs, true, options.halfWidth, ToDouble(mutableData->GetMiterLimit()));
                }
            }
        }
    }
    mutableData->SetColorUID(kTrue, none);
}

/** Foreground pass: gives every muted run its stroke paint back. Asked about every run
    and decides in Draw, like the stroke adornment. */
class IseStrokeRestoreAdornment : public CPMUnknown<IGlobalTextAdornment>
{
public:
    explicit IseStrokeRestoreAdornment(IPMUnknown* boss) : CPMUnknown<IGlobalTextAdornment>(boss) {}
    /** After everything else in the Foreground pass. */
    virtual Text::DrawPriority GetDrawPriority() { return Text::DrawPriority(Text::kTAPassPriForeground + 0.99); }
    virtual bool16 GetCheckIsActive() { return kTrue; }
#ifdef ISE_ROUTE_X
    virtual bool16 GetIsActive(const IParcelShape* parcel, const ITextOptions*, int32 flags)
    {
        if (parcel != nil) ise_cross_frame::NoteParcel(parcel, 'R', flags);
        return parcel != nil;
    }
#else
    virtual bool16 GetIsActive(const IParcelShape* parcel, const ITextOptions*, int32) { return parcel != nil; }
#endif
    virtual bool16 GetCheckCouldDraw() { return kFalse; }
    virtual bool16 GetCouldDraw(const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) { return kTrue; }
    virtual bool16 GetHasInkBounds() { return kFalse; }
    virtual void GetInkBounds(PMRect*, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) {}
    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void Draw(GraphicsData* gd, int32, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs)
    {
        if (run == nil || data == nil || glyphs == nil) return;
        IDataBase* db = run->GetDataBase();
        if (db == nil) return;
        Recover(run, data, glyphs, db);
        // A Center run's miter joins are filled here, after the Text pass, then its join returns.
        RecoverJoin(run, data, glyphs, db, gd);
#ifdef ISE_ROUTE_X
        // The Text pass has run: this run's fill is painted (route X; a no-op otherwise).
        ise_cross_frame::ConfirmFill(gd, run);
        ise_cross_frame::TraceRun('R', gd, run, data, glyphs, db, "restored");
#endif
    }
};

CREATE_PMINTERFACE(IseStrokeRestoreAdornment, kIseStrokeRestoreAdornmentImpl)
