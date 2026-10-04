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
//   Center or Outside alignment.
//
// InDesign fills an outline under the nonzero rule (`f`, also where contours overlap, as
// Create Outlines leaves them for decorative fonts). Every clip here is built to mean the same
// under both rules, since InDesign's PDF port does not keep the rule a clip asks for: the fill
// itself is the outline's nonzero region, which a nonzero clip of the outline reads exactly;
// the outside of the fill is built from the outline when both rules agree on it, and otherwise
// from the region resolved into contours that do not overlap (IseRegionCore.h).
//
// What it draws (IseMiterCore.h):
//   Center   the outline stroked at the weight with bevel joins; each exterior corner's miter
//            join filled; each interior corner's miter join filled, clipped to the item's
//            fill, so an interior miter's part outside the fill is not drawn. A corner is
//            interior when its join points into the fill (nonzero), so a corner one contour
//            makes inside another's fill is interior and stays inside it.
//   Outside  the outline filled again (InDesign fills an offset of it, which breaks on some
//            outlines), then stroked at twice the weight with bevel joins, clipped to outside
//            the fill (ise_miter::OutsideClip); each exterior corner's miter join filled.
//            Interior miters are dropped. Where the fill rules disagree, the stroke and its
//            corners follow the resolved region's boundary, so contours inside the fill show
//            no stroke (InDesign strokes every contour).
//   Joins are filled polygons in the stroke's paint and overprint (ise_miter::MiterJoin), not
//   restrokes: the PDF port's rounding bent short restrokes enough for renderers to disagree.
//   Past the miter limit, an exterior corner is not beveled at its vertex as InDesign does:
//   its miter is filled whole when its point reaches no farther than the corner's shorter edge
//   is long, so a shape's sharp corners keep their points (owner decision, 2026-10-04; a first
//   version cut every point at the limit distance). Otherwise the miter is cut where that edge
//   ends, but never closer than the limit distance (ise_miter::PointReach, CutMiter).
//   Interior miters keep the limit.
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
#include "IseRegionCore.h"

#include <algorithm>
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

void EmitPolygon(IGraphicsPort* port, const std::vector<ise_miter::Pt>& polygon)
{
    const std::vector<ise_miter::Pt> pts = ise_miter::Counterclockwise(polygon);
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

/** InDesign's highest miter limit: the longest point kept, in half widths. */
const double kLongestMiter = 500;

bool PastLimit(const ise_miter::Stub& s, double limit) { return !s.interior && s.ratio > limit; }

/** Fills the miter joins of the exterior or interior corners: whole within the limit; past
    it, an exterior point whole or cut (ise_miter::PointReach), an interior one beveled as the
    renderer would (the bevel stroke has drawn it). */
void FillJoins(IGraphicsPort* port, const std::vector<ise_miter::Stub>& stubs, bool interior, double h, double limit)
{
    bool any = false;
    port->newpath();
    for (const ise_miter::Stub& s : stubs) {
        if (s.interior != interior || (interior && s.ratio > limit)) continue;
        EmitPolygon(port, ise_miter::JoinPolygon(s, h, PastLimit(s, limit) ? ise_miter::PointReach(s, h, limit, kLongestMiter) : s.ratio * h));
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
    // What is stroked and where its corners are: the outline, or for an Outside stroke on an
    // outline whose fill rules disagree (overlapping contours, same-direction holes), the
    // boundary of its resolved region, rebuilt from the outline's own segments. An Outside
    // stroke shows only that boundary; stroking the contours inside the fill as well missed
    // the joins where two contours share a corner (each corner's probe landed in the other
    // contour's fill, so both read as interior; German Beauty, 2026-10-04).
    bool resolved = false;
    std::vector<ise_miter::Contour> stroked;
    if (outside && !ise_miter::FillRulesAgree(ise_miter::Flatten(contours, 0.001 * h, 1e-9), 0.01 * h)) {
        stroked = ise_region::NonzeroRegion(contours, std::min(0.001 * h, 0.0005));
        if (stroked.empty()) return false;
        resolved = true;
    }
    const std::vector<ise_miter::Contour>& path = resolved ? stroked : contours;
    ise_miter::Options options;
    options.halfWidth = h;
    const std::vector<ise_miter::Stub> stubs = ise_miter::Corners(path, options, !outside);
    bool anyInterior = false;
    for (const ise_miter::Stub& s : stubs) anyInterior = anyInterior || s.interior;
    double reach = limit;   // the longest miter drawn, in half widths
    for (const ise_miter::Stub& s : stubs)
        if (PastLimit(s, limit)) reach = std::max(reach, ise_miter::PointReach(s, h, limit, kLongestMiter) / h);
    // Overprint is set explicitly: IRenderingObject::Install does not carry it, and the PDF
    // port wrote the plugin's strokes and fills without it (measured 2026-10-04).
    bool16 overprint = kFalse, overprintFill = kFalse;
    if (attrs->GetOverprintStroke(overprint, style) != kSuccess ||
        attrs->GetOverprintFill(overprintFill, style) != kSuccess) return false;
    std::vector<ise_miter::Contour> outsideClip;
    if (outside) {
        // The frame lies past the longest miter drawn.
        outsideClip = ise_miter::OutsideClip(path, (reach + 2) * h + 10, 0.001 * h, 0.01 * h, resolved);
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
                if (overprintFill) port->setoverprint(kTrue);
                // Not IPathPageItem::Fill: for an Outside stroke that fills the offset path too.
                // Nonzero, as InDesign fills.
                port->newpath();
                EmitContours(port, contours);
                port->fill();
            }
        }
    }

    AutoGSave save(port);
    paint->Install(port, style, IRenderingObject::kInstallStroke);
    if (overprint) port->setoverprint(kTrue);   // the stroke and its joins
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
    EmitContours(port, path);
    port->stroke();
    FillJoins(port, stubs, false, h, limit);
    if (!outside && anyInterior) {
        // Clipped to the recorded outline, not with IPathPageItem::Clip: InDesign's PDF port
        // wrote that clip without its path (`W n`), and on a translated item dropped the
        // clipped stubs altogether, leaving notches at concave corners (measured 2026-10-04).
        // The fill is the outline's nonzero region, which this nonzero clip reads exactly, also
        // where contours overlap or a hole is wound the fill's way.
        AutoGSave clipScope(port);
        port->newpath();
        EmitContours(port, contours);
        port->clip();
        port->newpath();
        FillJoins(port, stubs, true, h, limit);
    }
    return true;
}

} // namespace ise_page_stroke
