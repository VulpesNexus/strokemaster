//========================================================================================
// Page-item strokes: miter corrections for shapes, frames, and outlined text. EXPERIMENTAL.
//
// CGraphicFrameShape asks the draw-event dispatcher before stroking a page item
// (kBeginFrameStrokeDrawMessage); a handler that reports the event handled makes InDesign skip
// its own stroke, and the stroke adornments around it, for that item. This route draws the
// stroke itself at that moment, in the item's inner coordinates, so drawing order is kept.
//
// It takes over only strokes it can draw exactly; everything else stays InDesign's:
//   closed paths only; a solid stroke type (no dashes or stripes); no corner effects; no
//   transparency on the item; an unscaled item (rotation and translation only); a Miter join;
//   Center or Outside alignment; wherever a clip is needed (Outside, or Center with interior
//   corners), an outline whose nonzero and even-odd regions agree, since InDesign's PDF port
//   does not keep the rule a clip asks for; no stroke overprint when a corner needs a cut.
//
// What it draws (IseMiterCore.h):
//   Center   the outline stroked at the weight with bevel joins; each exterior corner restroked
//            with the miter; each interior corner restroked with the miter, clipped to the
//            item's fill, so only an interior miter's part outside escapes.
//   Outside  the outline filled again (InDesign fills an offset of it, which breaks on some
//            outlines), then stroked at twice the weight with bevel joins, clipped to outside
//            the fill (ise_miter::OutsideClip); each exterior corner restroked with the miter.
//            Interior miters are dropped.
//   Past the miter limit, an exterior corner is not beveled at its vertex as InDesign does:
//   its miter is cut at the limit distance along its tip and filled (ise_miter::CutMiter), so
//   a point is kept up to the limit and the shape changes continuously as a corner sharpens
//   (owner decision, 2026-10-03).
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "IDataBase.h"
#include "IGradientFill.h"
#include "IGraphicAttributeUtils.h"
#include "IGraphicStyleDescriptor.h"
#include "IGraphicsPort.h"
#include "IOutlineInfo.h"
#include "IPathGeometry.h"
#include "IPathPageItem.h"
#include "IRenderingObject.h"
#include "IShape.h"
#include "ISwatchUtils.h"
#include "IXPManager.h"
#include "IXPUtils.h"

// General includes:
#include "AutoGSave.h"
#include "GraphicTypes.h"
#include "GraphicsData.h"
#include "InstStrokeFillID.h"
#include "PMMatrix.h"
#include "TransformUtils.h"
#include "Utils.h"

// Project includes:
#include "IsePageItemStroke.h"
#include "IseMiterCore.h"

#include <cmath>
#include <vector>

namespace ise_page_stroke {
namespace {

/** Records a page item's outline (corner effects included) in its inner coordinates. */
class Recording : public IOutlineInfo
{
public:
    explicit Recording(ise_miter::Recorder* r) : fRecorder(r) {}
    virtual void BeginPath() {}
    virtual void EndPath() {}
    virtual void moveto(const PMReal& x, const PMReal& y) { fRecorder->MoveTo({ToDouble(x), ToDouble(y)}); }
    virtual void lineto(const PMReal& x, const PMReal& y) { fRecorder->LineTo({ToDouble(x), ToDouble(y)}); }
    virtual void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2,
                         const PMReal& x3, const PMReal& y3)
    {
        fRecorder->CurveTo({ToDouble(x1), ToDouble(y1)}, {ToDouble(x2), ToDouble(y2)}, {ToDouble(x3), ToDouble(y3)});
    }
    virtual void closepath() { fRecorder->Close(); }
private:
    ise_miter::Recorder* fRecorder;
};

void EmitContours(IGraphicsPort* port, const std::vector<ise_miter::Contour>& contours)
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

void EmitStub(IGraphicsPort* port, const ise_miter::Stub& s)
{
    port->moveto(s.a.x, s.a.y);
    if (s.inCurve) port->curveto(s.in1.x, s.in1.y, s.in2.x, s.in2.y, s.v.x, s.v.y);
    else port->lineto(s.v.x, s.v.y);
    if (s.outCurve) port->curveto(s.out1.x, s.out1.y, s.out2.x, s.out2.y, s.b.x, s.b.y);
    else port->lineto(s.b.x, s.b.y);
}

void EmitPolygon(IGraphicsPort* port, const std::vector<ise_miter::Pt>& pts)
{
    port->moveto(pts[0].x, pts[0].y);
    for (size_t i = 1; i < pts.size(); ++i) port->lineto(pts[i].x, pts[i].y);
    port->closepath();
}

/** Rotation and translation only: a stroke weight then means the same in inner coordinates. */
bool Unscaled(IPMUnknown* item)
{
    const PMMatrix m = ::InnerToPasteboardMatrix(item);
    const double a = ToDouble(m[0]), b = ToDouble(m[1]), c = ToDouble(m[2]), d = ToDouble(m[3]);
    return std::fabs(a * a + b * b - 1) < 1e-6 && std::fabs(c * c + d * d - 1) < 1e-6 && std::fabs(a * c + b * d) < 1e-6;
}

bool PastLimit(const ise_miter::Stub& s, double limit) { return !s.interior && s.ratio > limit; }

/** Restrokes stubs with the miter join; exterior miters past the limit are instead filled,
    cut at the limit distance (ise_miter::CutMiter). */
void StrokeStubs(IGraphicsPort* port, const std::vector<ise_miter::Stub>& stubs, bool interior, double h, double limit)
{
    port->setlinejoin(kPMMiterJoin);
    port->setlinecap(kPMButtCap);
    port->setmiterlimit(limit);
    bool any = false;
    port->newpath();
    for (const ise_miter::Stub& s : stubs) {
        if (s.interior != interior || PastLimit(s, limit)) continue;
        EmitStub(port, s);
        any = true;
    }
    if (any) port->stroke(); else port->newpath();
    if (interior) return;
    any = false;
    for (const ise_miter::Stub& s : stubs) {
        if (!PastLimit(s, limit)) continue;
        EmitPolygon(port, ise_miter::CutMiter(s, h, limit * h));
        any = true;
    }
    if (any) port->fill(); else port->newpath();
}

} // namespace

bool DrawStroke(GraphicsData* gd, IPMUnknown* item)
{
    IGraphicsPort* port = gd ? gd->GetGraphicsPort() : nil;
    if (port == nil || item == nil) return false;
    InterfacePtr<IPathPageItem> pathItem(item, UseDefaultIID());
    InterfacePtr<IPathGeometry> geometry(item, UseDefaultIID());
    InterfacePtr<IGraphicStyleDescriptor> style(item, UseDefaultIID());
    InterfacePtr<IShape> shape(item, UseDefaultIID());
    IDataBase* db = ::GetDataBase(item);
    if (pathItem == nil || geometry == nil || style == nil || shape == nil || db == nil) return false;

    // Which strokes this route draws.
    const int32 paths = geometry->GetNumPaths();
    if (paths <= 0) return false;
    for (int32 i = 0; i < paths; ++i) if (!geometry->IsPathClosed(i)) return false;
    Utils<IGraphicAttributeUtils> attrs;
    const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    UID strokePaint = kInvalidUID;
    if (attrs->GetStrokeRenderingUID(strokePaint, style) != kSuccess || strokePaint == kInvalidUID ||
        strokePaint == none) return false;
    PMReal weightValue = 0, limitValue = 0;
    int32 join = 0, alignment = 0, cap = 0;
    if (attrs->GetStrokeWeight(weightValue, style) != kSuccess || attrs->GetJoinType(join, style) != kSuccess ||
        attrs->GetStrokeAlignment(alignment, style) != kSuccess || attrs->GetMiterLimit(limitValue, style) != kSuccess ||
        attrs->GetLineCap(cap, style) != kSuccess) return false;
    const double weight = ToDouble(weightValue), limit = ToDouble(limitValue);
    if (!(weight > 0) || weight > 10000 || !(limit >= 1) || join != kPMMiterJoin) return false;
    if (alignment != kStrokeAlignCenter && alignment != kStrokeAlignOutside) return false;
    ClassID stroker = kInvalidClass;
    UID strokerUID = kInvalidUID;
    if (attrs->GetLineImplementation(stroker, strokerUID, style) != kSuccess || stroker != kSolidPathStrokerBoss) return false;
    ClassID corner = kInvalidClass;
    if (attrs->GetCornerImplementation(corner, style) == kSuccess && corner != kInvalidClass) return false;
    InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
    if (xp == nil || xp->ItemHasTransparency(shape) != 0) return false;
    if (!Unscaled(item)) return false;
    InterfacePtr<IRenderingObject> paint(db, strokePaint, UseDefaultIID());
    if (paint == nil) return false;

    ise_miter::Recorder recorder;
    Recording writer(&recorder);
    pathItem->GetOutlineInfo(&writer);
    const std::vector<ise_miter::Contour>& contours = recorder.Contours();
    if (contours.empty()) return false;
    const bool outside = alignment == kStrokeAlignOutside;
    const double h = outside ? weight : weight / 2;   // half the drawn width
    ise_miter::Options options;
    options.halfWidth = h;
    const std::vector<ise_miter::Stub> stubs = ise_miter::Corners(contours, options, !outside);
    bool anyInterior = false;
    for (const ise_miter::Stub& s : stubs) anyInterior = anyInterior || s.interior;
    // Every clip drawn here (outside the fill; the fill itself for interior stubs) must mean
    // the same under both fill rules: the PDF port does not keep the rule a clip asks for.
    if ((outside || anyInterior) &&
        !ise_miter::FillRulesAgree(ise_miter::Flatten(contours, 0.001 * h, 1e-9), 0.01 * h)) return false;
    // A cut miter is filled in the stroke's paint; PDF keeps a separate overprint for fills,
    // so an overprinting stroke stays InDesign's when any corner needs a cut.
    bool cuts = false;
    for (const ise_miter::Stub& s : stubs) cuts = cuts || PastLimit(s, limit);
    bool16 overprint = kFalse;
    if (cuts && (attrs->GetOverprintStroke(overprint, style) != kSuccess || overprint)) return false;
    std::vector<ise_miter::Contour> outsideClip;
    if (outside) {
        // The frame lies past the longest miter the stroke can draw: limit * h, cut there.
        outsideClip = ise_miter::OutsideClip(contours, (limit + 2) * h + 10, 0.001 * h, 0.01 * h);
        if (outsideClip.empty()) return false;
    }

    // Outside: InDesign fills the outline offset by half the weight, not the outline, and its
    // offset breaks on outlines with overlapping or reversed contours (Create Outlines), leaving
    // holes inside the item (measured on suite B, 2026-10-03). The stroke here is drawn outside
    // the outline itself and covers the offset fill's band, so the outline is filled again first
    // in the item's own paint: identical pixels where the offset fill is whole
    // (transparency is excluded above). Gradient fills are left as InDesign drew them.
    if (outside) {
        UID fillPaint = kInvalidUID;
        if (attrs->GetFillRenderingUID(fillPaint, style) == kSuccess && fillPaint != kInvalidUID && fillPaint != none) {
            InterfacePtr<IGradientFill> gradient(db, fillPaint, UseDefaultIID());
            InterfacePtr<IRenderingObject> fill(db, fillPaint, UseDefaultIID());
            if (gradient == nil && fill != nil) {
                AutoGSave refill(port);
                fill->Install(port, style, IRenderingObject::kInstallFill);
                // Not IPathPageItem::Fill: for an Outside stroke that fills the offset path too.
                // Both fill rules agree on this outline (checked above), so nonzero is exact.
                port->newpath();
                EmitContours(port, contours);
                port->fill();
            }
        }
    }

    AutoGSave save(port);
    paint->Install(port, style, IRenderingObject::kInstallStroke);
    port->setlinewidth(2 * h);
    port->setlinecap(cap);
    if (outside) {
        // Clip to outside the fill, beyond every miter; the same under either fill rule.
        port->newpath();
        EmitContours(port, outsideClip);
        port->clip();
    }
    port->setlinejoin(kPMBevelJoin);
    port->setmiterlimit(limit);
    port->newpath();
    EmitContours(port, contours);
    port->stroke();
    StrokeStubs(port, stubs, false, h, limit);
    if (!outside && anyInterior) {
        AutoGSave clipScope(port);
        port->newpath();
        pathItem->Clip(port);   // the item's own fill
        port->newpath();
        StrokeStubs(port, stubs, true, h, limit);
    }
    return true;
}

} // namespace ise_page_stroke
