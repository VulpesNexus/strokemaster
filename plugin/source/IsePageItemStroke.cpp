//========================================================================================
// Page-item strokes: miter corrections, offset fills, and Inside strokes for shapes, frames,
// and outlined text. EXPERIMENTAL.
//
// CGraphicFrameShape asks the draw-event dispatcher before stroking a page item
// (kBeginFrameStrokeDrawMessage); a handler that reports the event handled makes InDesign skip
// its own stroke, and the stroke adornments around it, for that item. This route draws the
// stroke itself at that moment, in the item's inner coordinates, so drawing order is kept, and
// a transform kept on the item (a scale kept as a percentage, shear) applies to the stroke as
// it does to InDesign's: InDesign strokes a page item in its own space.
//
// What InDesign draws (measured on InDesign 21.6's PDF, docs/COVERAGE_CENSUS_2026-10-04.md):
//   Center   the path stroked; its miters escape thin parts, and past the limit points are
//            beveled.
//   Outside  the path offset outward by half the weight, filled and stroked at the weight. The
//            offset breaks on outlines (overlapping or reversed contours, corner effects),
//            showing the stroke through the letters and holes in the fill; its miters spike.
//   Inside   the path offset inward by half the weight, filled and stroked. Where a part is
//            thinner than the stroke the offset drops it, so the part is neither filled nor
//            stroked, and the stroke spills outside thin parts.
// The offset's corners on the stroke's side follow the join: extended to a point for a miter
// within the limit, an arc for a round join, beveled otherwise (ise_miter::OffsetBevel).
//
// What is taken over, and how:
//   Solid strokes, closed paths:
//     Center, Miter       the outline stroked at the weight with bevel joins; each exterior
//                         corner's miter join filled (past the limit, a shape's own point kept
//                         whole, ise_miter::WholePoint, and any other corner beveled as
//                         InDesign bevels it); each interior corner's miter filled, clipped to
//                         the fill. Center round and bevel joins have nothing to correct and
//                         stay InDesign's.
//     Outside, any join   the outline filled at the fill event instead of InDesign's outward
//                         offset (DrawFill, as for Inside below; the offset flooded a script
//                         letter's opening, 2026-10-05), then stroked at twice the weight,
//                         clipped to outside the fill
//                         (ise_miter::OutsideClip), with the join's corners: a miter's
//                         exterior corners filled as for Center, past the limit beveled as
//                         InDesign's offset bevels them (OffsetBevel); a round join's are the
//                         round stroke's own; a bevel join's exterior corners get the beveled
//                         offset's stroke (OffsetBevel). Interior miters are dropped.
//     Inside, any join    the outline filled at the fill event instead of InDesign's inward
//                         offset (DrawFill: InDesign skips its own fill, kBeginFrameFillDrawMessage),
//                         in the fill's place, beneath any content; then the stroke at twice
//                         the weight, clipped to the fill; the interior corners get the inward
//                         offset's join: a miter within the limit filled whole, past it or for a
//                         bevel join the beveled offset's stroke, a round join the round stroke's
//                         own. InDesign's inward offset fill spilled past curved outlines and
//                         corner effects (up to 1,700 pt2 on a line of outlined text in the
//                         census), which a stroke clipped to the fill could not cover; it is
//                         never drawn now, so curves, corner effects, and frames with content
//                         are covered (2026-10-05). With corner effects, the tiny holes their
//                         loops leave in an overlapping outline's region are not stroked.
//   Solid strokes, open paths:
//     Center, Miter       the path stroked at the weight with its caps and bevel joins, each
//                         corner's miter filled as InDesign draws it within the limit, and past
//                         it kept whole as on closed shapes; the corners are not told apart by
//                         the fill the path closes. Without arrowheads (a claimed stroke draws
//                         none). Outside and Inside on an open path stay InDesign's: it offsets
//                         the path to one side by its direction, which on outlined text puts
//                         the stroke inside many letters.
//   Other stroke types (dashes, dots, stripes, any stroke style): InDesign's own stroke
//     (IPathPageItem::Stroke) drawn from here under a clip (ise_region::StrokeClip): within a
//     little more than the stroke's reach of the outline, plus each corner's join, and for
//     Outside outside the fill, for Inside within it. That removes escaping tips and the
//     stroke shown through the letters while keeping InDesign's pattern; Outside and Inside
//     are filled at the fill event. Past the limit, InDesign bevels the pattern; the whole
//     points (below) are filled solid from the vertex over it, without the pattern (the
//     owner's choice, 2026-10-05; from the vertex since 2026-10-06).
//     Outside patterns follow one outline per word instead (the owner's choice, 2026-10-05;
//     PrepareWord, DrawWord): InDesign strokes each contour's offset, so where letters touch
//     or overlap their patterns cross. A path stroker (dashes, dots, hashes, waves, diamonds)
//     draws its pattern, corner adjustment included, along the word's united offset, its
//     creases rounded (ise_region::WordPath), over the gap color painted beneath; what a dash
//     stroker's band leaves where letters meet is filled solid (ise_region::Uncovered). Stripes, which
//     the stripe stroker draws along a page item's path only, are filled as regions by
//     distance from the fill (ise_region::DistanceBands), the gap color's beneath them: into
//     the notches, without seams against the fill, and with the whole points, each stripe
//     following the point (the owner's check, 2026-10-06). Custom stripes, whose layout no
//     public interface reads, are drawn along the outline by their stroker's StrokePathForUI;
//     a stroker that refuses the outline keeps the clip above. A pattern its stroker fits to
//     corners (a corner adjustment) is laid by runs between the outline's real corners
//     (ise_pattern): its adjustment fitted the pattern between every two anchors, which ran
//     dashes solid along outlined script (the owner's check, 2026-10-06).
//     Center and Inside patterns follow one outline as well where contours meet (Center
//     stripes: contours within the weight of one another or overlapping; Center path strokers
//     and Inside: overlapping contours), across the outline and within it (2026-10-06):
//     InDesign's stripes and dashes crossed there. An Inside path stroker's path is the
//     outline shrunk, which drops the parts thinner than the stroke; those are filled solid.
//
// Where an Outside or Inside outline's fill rules disagree (overlapping contours, holes wound
// the fill's way), the boundary of the region it fills is stroked instead of its contours
// (ise_region::NonzeroRegion), so the edges of contours inside the fill show no stroke
// (InDesign strokes every contour). Every clip is built to mean the same under both fill
// rules, since InDesign's PDF port does not keep the rule a clip asks for: the fill itself is
// the outline's nonzero region, which a nonzero clip of the outline reads exactly; the
// outside of the fill is built from the outline when both rules agree on it, and otherwise
// from the resolved region; both are checked exactly (ise_region::ClipsOutside).
//
// The fill is drawn at the fill event, not again at the stroke: InDesign strokes a frame after
// drawing its content, so a refill at the stroke would paint the frame's fill over its text or
// image. Both events analyze the item alike (Analyze), so they agree on taking it.
//
// Whole points (the owner's rule, 2026-10-05): past the miter limit, a corner between two
// straight edges, the shorter at least the stroke's reach long and the point at most five times
// that edge, is a shape's own point and is drawn all the way (ise_miter::WholePoint). Every
// other corner past the limit is beveled as InDesign bevels it: the tapered tips of script and
// blackletter glyphs, which a point at the limit distance turned into spikes, and the needles
// of decorative fonts. A glyph tip (ise_miter::TrimmedTip: sharper than 60 degrees with a curve
// on either side or an edge shorter than the stroke's reach, or sharper than 84 degrees with
// both) is trimmed (TipReach): beside an edge shorter than the reach, its stroke ends flush,
// cut at the reach along the tip, within the limit and past it; otherwise its miter reaches
// past the stroke's edge no farther than that edge. A glyph tip's whole miter stuck out as a
// spike (Segoe Script's W, Great Vibes' g; the owner asked for them trimmed, 2026-10-05, and
// flush, 2026-10-06). Blunter corners, such as serif feet, keep their whole miters.
//
// Where an Outside or Inside stroke follows the resolved region, the region's boundary has
// corners the outline does not: where contours cross, and where a contour's loop (corner
// effects on small details make many) pinches off. Past the miter limit, such a corner keeps
// the stroke's bevel, as InDesign's stroke of the crossing contours shows no point there; only
// corners of the outline itself keep whole points (inset corners on outlined text threw 16 pt
// blocks from loop pinches, 2026-10-04). Likewise the corners a corner effect adds: the outline
// with corner effects replaces each corner of the drawn path with the effect's shape, whose
// corners lie off the path's anchors; past the limit they keep the stroke's bevel, as InDesign
// draws them, and only corners at the path's own anchors keep whole points (fancy, inset, and
// inverse rounded corners threw 16 pt points from the acute corners of outlined text,
// 2026-10-05). Within the limit, such a corner's miter reaches past the stroke's edge no
// farther than the corner's shorter edge (ise_miter::EffectMiterReach): the fancy and inset
// effects cut corners into zigzags of segments under half a point, whose whole miters stuck out
// 7 to 10 pt (InDesign's own stroke draws them; the owner asked for them trimmed, 2026-10-05).
//
// Transparency: object-level opacity, blending, and effects are kept (InDesign draws the whole
// item, the plugin's stroke included, inside them); under the object's opacity or blending, or
// a group's around it, a whole point keeps to the painted bounds that bound InDesign's group
// (PointBounds). The stroke's and the
// fill's own opacity and blend mode are applied here, since a claimed stroke or fill is drawn
// without them: the stroke as one transparency group (BlendedStroke), so its overlapping parts
// do not show through one another as InDesign's per-contour strokes did, the fill with them set
// (OwnBlending, 2026-10-06). Effects on the stroke or the fill (a drop shadow, a feather, a
// glow) are not taken over: InDesign applies them around its own stroke and fill
// (CGraphicFrameShape's adornments), and claiming the stroke draws without them.
//
// Never taken over: an item filled even-odd (InDesign's even-odd attribute), a stroke
// without paint or weight, a singular transform, an open path other than a solid Center Miter
// stroke without arrowheads, effects on the stroke or the fill, and any transparency with the
// transparency switch off.
//
// Joins are filled polygons in the stroke's paint and overprint (ise_miter::JoinPolygon), not
// restrokes: the PDF port's rounding bent short restrokes enough for renderers to disagree.
//
// What a stroke needs besides its outline (the resolved region, the corners, the clips) is
// kept for the next draw of the same outline and stroke (PreparedCache): on one line of
// outlined Kerberos Fang it takes seconds, and the screen redraws an item many times.
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "IDataBase.h"
#include "IGraphicAttributeUtils.h"
#include "IGraphicStyleDescriptor.h"
#include "IGraphicsPort.h"
#include "IHierarchy.h"
#include "IOutlineInfo.h"
#include "IPathGeometry.h"
#include "IPathPageItem.h"
#include "IPathStroker.h"
#include "IRenderingObject.h"
#include "IShape.h"
#include "ISwatchUtils.h"
#include "IXPAttributeSuite.h"
#include "IXPAttributeUtils.h"
#include "IXPManager.h"
#include "IXPUtils.h"

// General includes:
#include "AutoGSave.h"
#include "GraphicTypes.h"
#include "GraphicsData.h"
#include "InstStrokeFillID.h"
#include "OffsetPathID.h"
#include "PMPathPoint.h"
#include "ScotchRulesID.h"
#include "PMMatrix.h"
#include "TransformTypes.h"
#include "TransformUtils.h"
#include "Utils.h"

// Project includes:
#include "IsePageItemStroke.h"
#include "IseMiterCore.h"
#include "IsePatternCore.h"
#include "IseRegionCore.h"
#include "IseStripeProbe.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace ise_page_stroke {
namespace {

using ise_miter::Contour;
using ise_miter::Pt;
using ise_miter::Stub;

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

/** Writes contours to the port's path; an open contour is left open (a fill or clip closes it). */
void EmitContours(IGraphicsPort* port, const std::vector<Contour>& contours)
{
    for (const Contour& c : contours) {
        port->moveto(c.start.x, c.start.y);
        for (const ise_miter::Segment& s : c.segments) {
            if (s.curve) port->curveto(s.c1.x, s.c1.y, s.c2.x, s.c2.y, s.p.x, s.p.y);
            else port->lineto(s.p.x, s.p.y);
        }
        if (c.closed) port->closepath();
    }
}

void EmitPolygon(IGraphicsPort* port, const std::vector<Pt>& polygon)
{
    const std::vector<Pt> pts = ise_miter::Counterclockwise(polygon);
    port->moveto(pts[0].x, pts[0].y);
    for (size_t i = 1; i < pts.size(); ++i) port->lineto(pts[i].x, pts[i].y);
    port->closepath();
}

/** Fills polygons together in the current paint (nonzero, all wound alike, so they add up). */
void FillPolygons(IGraphicsPort* port, const std::vector<std::vector<Pt>>& polygons)
{
    port->newpath();
    bool any = false;
    for (const std::vector<Pt>& p : polygons) {
        if (p.size() < 3) continue;
        EmitPolygon(port, p);
        any = true;
    }
    if (any) port->fill(); else port->newpath();
}

/** The item's transform is one a stroke can be drawn through: finite and not singular. */
bool Invertible(IPMUnknown* item)
{
    const PMMatrix m = ::InnerToPasteboardMatrix(item);
    const double a = ToDouble(m[0]), b = ToDouble(m[1]), c = ToDouble(m[2]), d = ToDouble(m[3]);
    const double det = a * d - b * c;
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c) && std::isfinite(d) && std::fabs(det) > 1e-6;
}

/** Reads one transparency attribute of the item. An item carries a stroke or fill attribute
    only once it is set: an item with object-level opacity alone has none of them, and reading
    them through the descriptor fails (measured 2026-10-05), so an absent attribute takes its
    hard-coded default, which is opaque, Normal, and no effect. False when a present attribute
    cannot be read. */
bool ReadXP(IXPAttributeUtils* xp, IGraphicStyleDescriptor* style, IXPAttributeSuite::AttributeType t,
            IXPAttributeSuite::AttributeValue& v, const Coverage& coverage, UID uid)
{
    const ClassID cls = xp->GetAttributeClassID(t);
    InterfacePtr<IPMUnknown> attr(cls == kInvalidClass ? nil : style->QueryAttribute(cls));
    const ErrorCode rc = cls == kInvalidClass ? kFailure
                         : attr != nil         ? xp->GetAttributeValue(t, v, attr.get())
                                               : xp->GetAttributeDefaultValue(t, v);
    if (coverage.trace) {
        IXPAttributeSuite::AttributeValue d;
        const ErrorCode rcDesc = xp->GetAttributeValue(t, d, style);
        coverage.trace("X item=%u attr=%d class=0x%x present=%d rc=%d desc_rc=%d type=%d int=%d real=%.3f bool=%d",
                       uid.Get(), static_cast<int>(t), cls.Get(), attr != nil ? 1 : 0, static_cast<int>(rc),
                       static_cast<int>(rcDesc), static_cast<int>(v.GetDataType()), v.GetInt32Data(),
                       ToDouble(v.GetRealData()), v.GetBooleanData() ? 1 : 0);
    }
    return rc == kSuccess;
}

/** A stroke's or a fill's own opacity (0 to 100) and blend mode, not the object's. */
struct OwnBlending {
    double strokeOpacity = 100, fillOpacity = 100;
    PMBlendingMode strokeBlend = kPMBlendNormal, fillBlend = kPMBlendNormal;
    bool Stroke() const { return strokeOpacity < 100 || strokeBlend != kPMBlendNormal; }
    bool Fill() const { return fillOpacity < 100 || fillBlend != kPMBlendNormal; }
};

/** Reads the stroke's and the fill's own opacity and blend mode, which the plugin applies as
    InDesign applies them around its own stroke and fill (a claimed stroke or fill is drawn
    without them: the census showed a 50% stroke opaque, and a refill opaque over a 50% fill).
    False when the stroke or the fill has effects of its own (a drop shadow, a feather, a glow,
    an inner shadow, bevel and emboss, satin, or a directional or gradient feather), which stay
    InDesign's, or when an attribute cannot be read. */
bool OwnTransparency(IGraphicStyleDescriptor* style, const Coverage& coverage, UID uid, OwnBlending* out)
{
    Utils<IXPAttributeUtils> xp;
    if (!xp) return false;
    typedef IXPAttributeSuite S;
    const S::AttributeType shadow[] = {S::kStrokeDropShadowMode, S::kFillDropShadowMode};
    const S::AttributeType feather[] = {S::kStrokeFeatherMode, S::kFillFeatherMode};
    const S::AttributeType applied[] = {
        S::kStrokeInnerShadowApply, S::kStrokeOuterGlowApply, S::kStrokeInnerGlowApply, S::kStrokeBevelEmbossApply,
        S::kStrokeSatinApply, S::kStrokeDirectionalFeatherApply, S::kStrokeGradientFeatherApply,
        S::kFillInnerShadowApply, S::kFillOuterGlowApply, S::kFillInnerGlowApply, S::kFillBevelEmbossApply,
        S::kFillSatinApply, S::kFillDirectionalFeatherApply, S::kFillGradientFeatherApply};
    S::AttributeValue v;
    if (!ReadXP(xp, style, S::kStrokeBlendingOpacity, v, coverage, uid) || v.GetDataType() != S::kDataTypeReal) return false;
    out->strokeOpacity = ToDouble(v.GetReal());
    if (!ReadXP(xp, style, S::kFillBlendingOpacity, v, coverage, uid) || v.GetDataType() != S::kDataTypeReal) return false;
    out->fillOpacity = ToDouble(v.GetReal());
    if (!ReadXP(xp, style, S::kStrokeBlendingBlendMode, v, coverage, uid) || v.GetDataType() != S::kDataTypePMBlendingMode) return false;
    out->strokeBlend = v.GetPMBlendingMode();
    if (!ReadXP(xp, style, S::kFillBlendingBlendMode, v, coverage, uid) || v.GetDataType() != S::kDataTypePMBlendingMode) return false;
    out->fillBlend = v.GetPMBlendingMode();
    for (S::AttributeType t : shadow)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypePMDropShadowMode ||
            v.GetPMDropShadowMode() != kDSMNone) return false;
    for (S::AttributeType t : feather)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypePMVignetteMode ||
            v.GetPMVignetteMode() != kVTMNone) return false;
    for (S::AttributeType t : applied)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypeBoolean || v.GetBoolean()) return false;
    return true;
}

/** Whether the object itself has opacity, a blend mode, or a knockout or isolated group: InDesign
    then draws the whole item, the plugin's stroke included, inside a transparency group bounded
    by the item's painted bounds, which a whole point past the miter limit outreaches
    (PointBounds). An object's effects alone do not bound it so: a drop shadow's point was drawn
    whole (census 10, 2026-10-06). True also when an attribute cannot be read. */
bool ObjectBlending(IGraphicStyleDescriptor* style, const Coverage& coverage, UID uid)
{
    Utils<IXPAttributeUtils> xp;
    if (!xp) return true;
    typedef IXPAttributeSuite S;
    S::AttributeValue v;
    if (!ReadXP(xp, style, S::kBSOpacity, v, coverage, uid) || v.GetDataType() != S::kDataTypeReal || v.GetReal() < 100) return true;
    if (!ReadXP(xp, style, S::kBSBlendMode, v, coverage, uid) || v.GetDataType() != S::kDataTypePMBlendingMode ||
        v.GetPMBlendingMode() != kPMBlendNormal) return true;
    for (S::AttributeType t : {S::kBSKnockoutGroup, S::kBSIsolationGroup})
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypeBoolean || v.GetBoolean()) return true;
    return false;
}

/** Where a whole point may reach when the object has opacity or blending (ObjectBlending): within
    the item's painted bounds, in pasteboard coordinates, which bound InDesign's transparency
    group. A point that reached past them was cut flat along the bounds' edge in the
    live-transparency PDFs (High Quality Print, PDF/X-4) and whole in the flattened PDF/X-1a
    (2026-10-06); within them it is shortened along its own axis instead (Fit). */
struct PointBounds {
    bool on = false;
    double box[4] = {0, 0, 0, 0};   // left, top, right, bottom (pasteboard)
    double m[6] = {1, 0, 0, 1, 0, 0};   // inner to pasteboard

    /** Narrows the bounds to `r` as well (a group around the item bounds it too). */
    void Within(const PMRect& r)
    {
        const double b[4] = {ToDouble(r.Left()), ToDouble(r.Top()), ToDouble(r.Right()), ToDouble(r.Bottom())};
        if (!on) { std::copy(b, b + 4, box); on = true; return; }
        box[0] = std::max(box[0], b[0]); box[1] = std::max(box[1], b[1]);
        box[2] = std::min(box[2], b[2]); box[3] = std::min(box[3], b[3]);
    }

    bool Inside(const std::vector<Pt>& polygon) const
    {
        const double margin = 0.01;
        for (const Pt& p : polygon) {
            const double x = m[0] * p.x + m[2] * p.y + m[4], y = m[1] * p.x + m[3] * p.y + m[5];
            if (x < box[0] + margin || x > box[2] - margin || y < box[1] + margin || y > box[3] - margin) return false;
        }
        return true;
    }
    /** The longest reach up to `reach` at which the corner's miter (ise_miter::CutMiter) lies
        within the bounds; `reach` when the bounds are off or it does. */
    double Fit(const Stub& s, double h, double reach) const
    {
        if (!on || Inside(ise_miter::CutMiter(s, h, reach))) return reach;
        double lo = 0, hi = reach;
        for (int k = 0; k < 40; ++k) {
            const double mid = (lo + hi) / 2;
            if (Inside(ise_miter::CutMiter(s, h, mid))) lo = mid; else hi = mid;
        }
        return lo;
    }
};

/** Whether the item has corner effects (on any corner, simple or live). True also when an
    attribute cannot be read. */
bool CornerEffects(IGraphicAttributeUtils* attrs, IGraphicStyleDescriptor* style)
{
    const IGraphicAttributeUtils::CornerType corners[] = {
        IGraphicAttributeUtils::kSimpleCorner, IGraphicAttributeUtils::kLiveTopLeftCorner,
        IGraphicAttributeUtils::kLiveTopRightCorner, IGraphicAttributeUtils::kLiveBottomLeftCorner,
        IGraphicAttributeUtils::kLiveBottomRightCorner};
    for (IGraphicAttributeUtils::CornerType c : corners) {
        ClassID impl = kInvalidClass;
        if (attrs->GetCornerImplementation(impl, style, c) != kSuccess) return true;
        if (impl != kInvalidClass) return true;
    }
    return false;
}

/** InDesign's highest miter limit: the longest point kept, in half widths. */
const double kLongestMiter = 500;

bool PastLimit(const Stub& s, double limit) { return s.ratio > limit; }

enum class Align { Center, Outside, Inside };

/** The joins filled at the corners on the stroke's side, beyond a stroke of half width h along
    the outline with bevel joins: see the file comment. Center and Outside strokes take the
    exterior corners, Inside strokes the interior ones; Center's interior corners are filled by
    the caller, clipped to the fill. `crossing` (empty, or one flag per stub) marks the corners
    the drawn shape does not have (the resolved region's, or a corner effect's), which keep the
    stroke's bevel past the limit. `effect` (empty, or one flag per stub) marks the corners a
    corner effect adds, whose miters within the limit are trimmed (EffectMiterReach), as are
    those of glyph tips (TrimmedTip). The switches turn whole points and the glyph-tip trim off.
    A whole point stays within `bounds` (PointBounds). */
std::vector<std::vector<Pt>> StrokeSideJoins(const std::vector<Stub>& stubs, const std::vector<char>& crossing,
                                             const std::vector<char>& effect, Align align, int32 join, double h, double limit,
                                             const Coverage& coverage, const PointBounds& bounds)
{
    std::vector<std::vector<Pt>> out;
    const bool wantInterior = align == Align::Inside;
    for (size_t i = 0; i < stubs.size(); ++i) {
        const Stub& s = stubs[i];
        if (s.interior != wantInterior) continue;
        if (join == kPMRoundJoin) continue;   // the round stroke's own join is the offset's
        if (i < crossing.size() && crossing[i] && PastLimit(s, limit)) continue;   // the stroke's bevel
        const bool tip = coverage.glyphTips && ise_miter::TrimmedTip(s, h);
        if (join == kPMMiterJoin && !PastLimit(s, limit)) {
            double reach = s.ratio * h;
            if (i < effect.size() && effect[i]) reach = ise_miter::EffectMiterReach(s, h);
            if (tip) reach = std::min(reach, ise_miter::TipReach(s, h));
            out.push_back(ise_miter::JoinPolygon(s, h, reach));
        } else if (join == kPMMiterJoin && align != Align::Inside && coverage.wholePoints && ise_miter::WholePoint(s, h) &&
                   bounds.Fit(s, h, ise_miter::WholePointReach(s, h, kLongestMiter)) > 0) {
            // Past the limit, a shape's own point: kept whole (the owner's whole points), within
            // the bounds of the object's transparency group; none left there, the bevel below.
            out.push_back(ise_miter::JoinPolygon(s, h, bounds.Fit(s, h, ise_miter::WholePointReach(s, h, kLongestMiter))));
        } else if (join == kPMMiterJoin && align == Align::Outside && tip && s.edge < h) {
            // Past the limit, a glyph tip: cut flush, as within it (TipReach), not beveled.
            out.push_back(ise_miter::JoinPolygon(s, h, h));
        } else if (join == kPMMiterJoin && align == Align::Center) {
            continue;   // past the limit, any other corner: the stroke's bevel, as InDesign's
        } else {
            // The beveled offset's stroke, with the outline's own stroke beside the corner
            // (JoinPolygon cut at the vertex: its bands and bevel) so no seam shows between them.
            out.push_back(ise_miter::JoinPolygon(s, h, 0));
            out.push_back(ise_miter::OffsetBevel(s, h, limit, join == kPMMiterJoin));
        }
    }
    return out;
}

/** The whole points a patterned stroke (Center or Outside, Miter) keeps past the limit: the
    whole miter from the vertex (ise_miter::CutMiter), filled solid over the pattern, so the
    pattern ends where each side meets the point. Drawn from the bevel the pattern draws
    there, it left a dash cut short and a gap below the point (the owner's check, 2026-10-06).
    The corners as in StrokeSideJoins, and within `bounds` likewise. */
std::vector<std::vector<Pt>> PatternPoints(const std::vector<Stub>& stubs, const std::vector<char>& crossing, Align align,
                                           int32 join, double h, double limit, const Coverage& coverage, const PointBounds& bounds)
{
    std::vector<std::vector<Pt>> out;
    if (join != kPMMiterJoin || align == Align::Inside || !coverage.wholePoints || !coverage.patternPoints) return out;
    for (size_t i = 0; i < stubs.size(); ++i) {
        const Stub& s = stubs[i];
        if (s.interior || !PastLimit(s, limit) || !ise_miter::WholePoint(s, h)) continue;
        if (i < crossing.size() && crossing[i]) continue;
        out.push_back(ise_miter::CutMiter(s, h, bounds.Fit(s, h, ise_miter::WholePointReach(s, h, kLongestMiter))));
    }
    return out;
}

/** The outline filled in the item's fill paint, gradients included, nonzero as InDesign
    fills, in place of InDesign's offset fill (DrawFill), which broke on curved outlines and
    corner effects; with the fill's own opacity and blend mode. Not IPathPageItem::Fill: for an
    Outside or Inside stroke that fills the offset path. */
void Refill(IGraphicsPort* port, IDataBase* db, IGraphicStyleDescriptor* style, UID none, const std::vector<Contour>& contours,
            bool16 overprintFill, const OwnBlending& blending)
{
    UID fillPaint = kInvalidUID;
    if (Utils<IGraphicAttributeUtils>()->GetFillRenderingUID(fillPaint, style) != kSuccess || fillPaint == kInvalidUID ||
        fillPaint == none) return;
    InterfacePtr<IRenderingObject> fill(db, fillPaint, UseDefaultIID());
    if (fill == nil) return;
    AutoGSave refill(port);
    fill->Install(port, style, IRenderingObject::kInstallFill);
    if (overprintFill) port->setoverprint(kTrue);
    if (blending.Fill()) {
        // The fill's own opacity and blend mode: one fill, so nothing in it overlaps.
        port->setopacity(blending.fillOpacity / 100, kFalse);
        port->setblendingmode(blending.fillBlend);
    }
    port->newpath();
    EmitContours(port, contours);
    port->fill();
}

/** Everything a stroke draws from besides its outline and paint, prepared once per outline and
    stroke (Prepare). `ok` false: the route refuses the stroke (kept, so the refusal is not
    worked out again at every draw). */
struct Prepared {
    bool ok = false;
    bool resolved = false;
    std::vector<Contour> stroked;       // the resolved region's boundary, when resolved
    std::vector<Stub> stubs;
    std::vector<char> crossing;         // per stub: a corner the drawn shape lacks (resolution, corner effect)
    std::vector<char> effect;           // per stub, with corner effects: a corner the effect adds
    std::vector<Contour> outsideClip;   // Outside, solid
    std::vector<Contour> strokeClip;    // other stroke types
    // Patterns along one outline per word (PrepareWord; Center and Inside where contours meet):
    std::vector<Contour> word;          // the outline's united offset at half the weight, for a path stroker
    std::vector<Contour> notches;       // what a dash stroker's band along it leaves (ise_region::Uncovered)
    ise_pattern::Laid laid;             // a fitted pattern laid along it by runs between real corners
    std::vector<std::vector<Contour>> bands;   // stripes: each stripe's region (ise_region::DistanceBands)
    std::vector<Contour> gapBand;       // stripes with a gap color: the region it paints, beneath them
};

/** A 64-bit FNV-1a hash of what Prepare reads. */
class Hash {
public:
    void Add(const void* data, size_t n)
    {
        const unsigned char* b = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < n; ++i) { fH ^= b[i]; fH *= 1099511628211ull; }
    }
    void Add(double v) { if (v == 0) v = 0; Add(&v, sizeof v); }   // -0 hashes as 0
    void Add(int32 v) { Add(&v, sizeof v); }
    void Add(Pt p) { Add(p.x); Add(p.y); }
    std::uint64_t Value() const { return fH; }
private:
    std::uint64_t fH = 14695981039346656037ull;
};

/** Prepared strokes by key, the most recently used first. Shared by the drawing threads (screen,
    background export). Bounded by entries and by the segments they hold, so that a document of
    large outlines does not keep them all. */
class PreparedCache {
public:
    std::shared_ptr<const Prepared> Find(std::uint64_t key)
    {
        std::lock_guard<std::mutex> hold(fLock);
        for (auto it = fEntries.begin(); it != fEntries.end(); ++it) {
            if (it->key != key) continue;
            fEntries.splice(fEntries.begin(), fEntries, it);
            return fEntries.front().value;
        }
        return nullptr;
    }
    void Keep(std::uint64_t key, std::shared_ptr<const Prepared> value, size_t size)
    {
        std::lock_guard<std::mutex> hold(fLock);
        for (auto it = fEntries.begin(); it != fEntries.end(); ++it) {
            if (it->key != key) continue;   // prepared meanwhile by another drawing thread
            fSize -= it->size;
            fEntries.erase(it);
            break;
        }
        fEntries.push_front({key, std::move(value), size});
        fSize += size;
        while (fEntries.size() > 1 && (fEntries.size() > kEntries || fSize > kSegments)) {
            fSize -= fEntries.back().size;
            fEntries.pop_back();
        }
    }
private:
    struct Entry { std::uint64_t key; std::shared_ptr<const Prepared> value; size_t size; };
    static const size_t kEntries = 256, kSegments = 2000000;
    std::mutex fLock;
    std::list<Entry> fEntries;
    size_t fSize = 0;
};

PreparedCache& Cache() { static PreparedCache cache; return cache; }

size_t Segments(const std::vector<Contour>& contours)
{
    size_t n = 0;
    for (const Contour& c : contours) n += c.segments.size() + 1;
    return n;
}

/** Set while InDesign's own stroke is drawn from here (StrokeNatively), which may dispatch the
    stroke event again. */
thread_local bool tInsideStroke = false;

/** Sets tInsideStroke for a scope, and clears it however the scope is left. */
class InsideStroke {
public:
    InsideStroke() { tInsideStroke = true; }
    ~InsideStroke() { tInsideStroke = false; }
    InsideStroke(const InsideStroke&) = delete;
    InsideStroke& operator=(const InsideStroke&) = delete;
};

/** Draws InDesign's own stroke for the item, under the clip (nonzero; empty: none). False when
    called from within that same drawing (IPathPageItem::Stroke dispatching the event again). */
bool StrokeNatively(IGraphicsPort* port, IPathPageItem* pathItem, IGraphicStyleDescriptor* style, IRenderingObject* paint,
                    const std::vector<Contour>& clip, double weight, int32 join, double limit, int32 cap, bool16 overprint)
{
    if (tInsideStroke) return false;
    InsideStroke inside;
    {
        AutoGSave save(port);
        if (!clip.empty()) {
            port->newpath();
            EmitContours(port, clip);
            port->clip();
        }
        port->newpath();
        paint->Install(port, style, IRenderingObject::kInstallStroke);
        if (overprint) port->setoverprint(kTrue);
        port->setlinewidth(weight);
        port->setlinejoin(join);
        port->setmiterlimit(limit);
        port->setlinecap(cap);
        pathItem->Stroke(port);
    }
    return true;
}

/** Works out what the stroke draws from (Prepared): see the file comment. */
void Prepare(const std::vector<Contour>& contours, const std::vector<Pt>* anchors, Align align, int32 join, bool solid, double h,
             double limit, bool centerWord, const Coverage& coverage, Prepared* out)
{
    const bool center = align == Align::Center, outside = align == Align::Outside, inside = align == Align::Inside;
    // The region is rebuilt within 0.0005 pt; the rules and the clips are compared on coarser
    // flattenings (their slivers are discounted), and the clip InDesign's own stroke is drawn
    // under lies 0.1 h past its reach, so it is flattened coarser still.
    const double regionTol = std::min(0.001 * h, 0.0005), checkTol = std::max(regionTol, 0.002 * h);
    // What is stroked and where its corners are: the outline, or for an Outside or Inside
    // stroke on an outline whose fill rules disagree, the boundary of its resolved region,
    // rebuilt from the outline's own segments. Such a stroke shows only that boundary;
    // stroking the contours inside the fill as well missed the joins where two contours share a
    // corner (each corner's probe landed in the other contour's fill, so both read as interior;
    // German Beauty, 2026-10-04).
    // A Center pattern too, when it will follow one outline (`centerWord`: PrepareWord, the word
    // outline and overlaps switches on); a Center solid stroke keeps InDesign's drawing of
    // overlapping contours, and so does any Center stroke with those switches off.
    if ((!center || centerWord) && !ise_region::FillRulesAgree(contours, checkTol)) {
        if (!coverage.overlaps) return;   // switched off: InDesign strokes every contour
        out->stroked = ise_region::NonzeroRegion(contours, regionTol);
        if (out->stroked.empty()) return;
        out->resolved = true;
        if (inside && anchors != nullptr) {
            // Corner effects: the tiny holes their loops leave inside the fill are not stroked
            // (ise_miter::SmallHoles); the fill keeps them as InDesign's does.
            const std::vector<char> tiny = ise_miter::SmallHoles(out->stroked, h, regionTol);
            std::vector<Contour> kept;
            for (size_t i = 0; i < out->stroked.size(); ++i)
                if (!tiny[i]) kept.push_back(out->stroked[i]);
            out->stroked.swap(kept);
        }
    }
    const std::vector<Contour>& path = out->resolved ? out->stroked : contours;
    ise_miter::Options options;
    options.halfWidth = h;
    out->stubs = ise_miter::Corners(path, options, !outside);
    // An open path (Center only): every corner's miter is drawn as InDesign draws it, whether
    // it points into the fill the path closes or not (see the file comment).
    bool open = false;
    for (const Contour& c : contours) open = open || !c.closed;
    if (open) for (Stub& s : out->stubs) s.interior = false;
    if (out->resolved) out->crossing = ise_miter::CornersNotOnOutline(out->stubs, contours, options, limit);
    if (anchors != nullptr) {   // corner effects: the corners they add (see the file comment)
        const std::vector<char> made = ise_miter::CornersNotAtAnchors(out->stubs, *anchors);
        out->effect = made;
        if (out->crossing.empty()) out->crossing = made;
        else for (size_t i = 0; i < made.size(); ++i) out->crossing[i] = out->crossing[i] || made[i];
    }
    // The clip for InDesign's own stroke (other stroke types): its reach of the outline
    // (StrokeClip), when the outline is small enough for that to take well under a second;
    // Clipper2's offset of the 5,725 contours of a line of outlined Kerberos Fang took 8 s.
    // Otherwise only the fill bounds it: Outside, outside it (the exact clip below), which
    // removes the stroke shown through the letters (InDesign's dashes spiked by under 0.2 pt2 in
    // the census); Inside, within it; Center has nothing else to clip and stays InDesign's.
    const double clipTol = std::max(regionTol, 0.01 * h);
    size_t points = 0;
    if (!solid) for (const ise_miter::Ring& r : ise_miter::Flatten(path, clipTol, 1e-9)) points += r.points.size();
    const bool band = !solid && points <= 10000;
    if (!solid && band) {
        const double margin = 0.1 * h;
        std::vector<std::vector<Pt>> joins;
        for (const Stub& s : out->stubs) {
            if (s.interior != inside) continue;
            joins.push_back(ise_miter::CutMiter(s, h + margin, std::min(s.ratio, std::max(limit, 1.5)) * (h + margin)));
        }
        const ise_region::FillMode mode = center ? ise_region::FillMode::Add : outside ? ise_region::FillMode::Remove
                                                                                   : ise_region::FillMode::Within;
        out->strokeClip = ise_region::StrokeClip(path, h, margin, joins, contours, mode, clipTol);
        out->ok = !out->strokeClip.empty();
        return;
    }
    if (!solid && center) return;
    if (!solid && inside) {
        out->strokeClip = contours;   // the fill: a nonzero clip of the outline reads it exactly
        out->ok = true;
        return;
    }
    if (outside) {
        double reach = std::max(limit, 2.0);   // the longest join drawn, in half widths
        for (const Stub& s : out->stubs)
            if (!s.interior && PastLimit(s, limit) && coverage.wholePoints && ise_miter::WholePoint(s, h)) reach = std::max(reach, ise_miter::WholePointReach(s, h, kLongestMiter) / h);
        // The frame lies past the longest join drawn.
        out->outsideClip = ise_miter::OutsideClip(path, (reach + 3) * h + 10, 0.001 * h, 0.01 * h, out->resolved, false);
        if (out->outsideClip.empty() || !ise_region::ClipsOutside(out->outsideClip, contours, checkTol)) return;
        if (!solid) out->strokeClip = out->outsideClip;
    }
    out->ok = true;
}

/** The stripes of InDesign's built-in striped stroke styles, as fractions of the weight [from,
    to], measured from the stroke's edge away from the object (read from InDesign 21.6's PDF of
    each style, 2026-10-05: InDesign draws each stripe as a stroke of its own). Empty for any
    other stroker, custom striped styles included. */
std::vector<std::pair<double, double>> BuiltInStripes(ClassID c)
{
    if (c == kThinThinPathStrokerBoss) return {{0, 1.0 / 6}, {5.0 / 6, 1}};
    if (c == kThinThickPathStrokerBoss) return {{0, 1.0 / 6}, {0.5, 1}};
    if (c == kThickThinPathStrokerBoss) return {{0, 0.5}, {5.0 / 6, 1}};
    if (c == kThickThickPathStrokerBoss) return {{0, 1.0 / 3}, {2.0 / 3, 1}};
    if (c == kThinThickThinPathStrokerBoss) return {{0, 1.0 / 7}, {2.5 / 7, 4.5 / 7}, {6.0 / 7, 1}};
    if (c == kThickThinThickPathStrokerBoss) return {{0, 2.0 / 7}, {3.0 / 7, 4.0 / 7}, {5.0 / 7, 1}};
    if (c == kTriplePathStrokerBoss) return {{0, 1.0 / 6}, {5.0 / 12, 7.0 / 12}, {5.0 / 6, 1}};
    return {};
}

/** A stroke type's own class: for a custom style (kCustomPathStrokerBoss), its document style's
    class (custom dashes, dots, or stripes), as ITableLineType reads it. */
ClassID StrokeClass(IDataBase* db, ClassID stroker, UID uid)
{
    if (stroker == kCustomPathStrokerBoss && uid != kInvalidUID) return db->GetClass(uid);
    return stroker;
}

/** The stripe strokers: built-in, custom, and the tables' own. */
bool StripeStroker(ClassID c)
{
    return !BuiltInStripes(c).empty() || c == kCustomStripedPathStrokerBoss || c == kTableStripePathStrokerBoss;
}

/** The dash strokers (dashes, not dots): their bands leave notches where letters meet, which are
    filled solid (ise_region::Uncovered). */
bool DashStroker(ClassID c)
{
    return c == kDashedPathStrokerBoss || c == kCannedDash4x4PathStrokerBoss || c == kCannedDash3x2PathStrokerBoss ||
           c == kCustomDashedPathStrokerBoss;
}

/** The pattern strokers drawn along one outline per word (IPathStroker::StrokePath follows any
    path given to it, gap colors and corner adjustment included, measured for the dash strokers
    on InDesign 21.6, 2026-10-05): every stroke type but solid and stripes. The stripe stroker
    refuses a path that is not a page item's (StrokePath fails), so built-in stripes are drawn
    here (DistanceBands); a custom style's stripes, whose layout no public interface reads, are
    drawn by its stroker's StrokePathForUI, which strokes any path. A stroker that fails is
    remembered (Refused) and keeps InDesign's own stroke under the clip. */
bool PatternStroker(ClassID c)
{
    return c != kSolidPathStrokerBoss && c != kInvalidClass && !StripeStroker(c);
}

/** The pattern strokers whose StrokePath failed on a word's outline, for this session. */
class Refusals {
public:
    bool Has(ClassID c)
    {
        std::lock_guard<std::mutex> hold(fLock);
        return std::find(fClasses.begin(), fClasses.end(), c) != fClasses.end();
    }
    void Add(ClassID c)
    {
        std::lock_guard<std::mutex> hold(fLock);
        if (std::find(fClasses.begin(), fClasses.end(), c) == fClasses.end()) fClasses.push_back(c);
    }
private:
    std::mutex fLock;
    std::vector<ClassID> fClasses;
};

Refusals& Refused() { static Refusals r; return r; }

/** A path geometry holding contours, in a temporary geometry boss of InDesign's offset plugin.
    A point is smooth where its direction points line up within 2 degrees (InDesign's offset
    turns its curves' joins by a fraction of a degree), which a dash stroker's corner adjustment
    reads. The adjustment still fits its pattern between every two anchors of an outline, so on
    outlined script it runs dashes together solid, as InDesign's own stroke does (2026-10-06). */
InterfacePtr<IPathGeometry> TempGeometry(const std::vector<Contour>& contours)
{
    InterfacePtr<IPathGeometry> g(static_cast<IPathGeometry*>(::CreateObject(kOffsetPathTempPathGeometry, IID_IPATHGEOMETRY)));
    if (g == nil) return g;
    g->RemoveAllPaths();
    auto P = [](Pt p) { return PMPoint(p.x, p.y); };
    for (const Contour& c : contours) {
        const std::vector<ise_miter::Segment> segs = c.closed ? ise_miter::Closed(c, 1e-9) : ise_miter::Stroked(c, 1e-9);
        if (segs.empty()) continue;
        // Anchors: the start, then each segment's end (the last, for a closed contour, is the start).
        std::vector<Pt> anchor{c.start}, left{c.start}, right;
        for (const ise_miter::Segment& sg : segs) {
            right.push_back(sg.curve ? sg.c1 : anchor.back());
            anchor.push_back(sg.p);
            left.push_back(sg.curve ? sg.c2 : sg.p);
        }
        right.push_back(anchor.back());
        size_t n = anchor.size();
        if (c.closed) { left[0] = left[n - 1]; --n; }
        const int32 path = g->AddNewPath(static_cast<int32>(n));
        for (size_t i = 0; i < n; ++i) {
            const Pt a = anchor[i], l = left[i], r = right[i];
            const Pt dl = a - l, dr = r - a;
            const double ll = ise_miter::Len(dl), lr = ise_miter::Len(dr);
            const bool smooth = ll > 1e-9 && lr > 1e-9 && std::fabs(ise_miter::Cross(dl, dr)) <= 0.0349 * ll * lr && ise_miter::Dot(dl, dr) > 0;
            g->AddPoint(path, PMPathPoint(smooth ? kCS : kCK, P(l), P(a), P(r)));
        }
        if (c.closed) g->ClosePath(path); else g->OpenPath(path);
    }
    return g;
}

/** The stripes a striped stroker paints, measured (ise_stripe_probe): across a straight line
    1000 pt long at a weight of 100, read at its middle, as [from, to] fractions of the weight
    from the side y < 0. Empty when the stroker draws nothing it can read, or clips. */
std::vector<std::pair<double, double>> MeasureStripes(const IPathStroker* stroker)
{
    std::vector<std::pair<double, double>> out;
    if (stroker == nil) return out;
    Contour line;
    line.start = {0, 0};
    line.closed = false;
    ise_miter::Segment s;
    s.p = {1000, 0};
    line.segments.push_back(s);
    InterfacePtr<IPathGeometry> path = TempGeometry({line});
    if (path == nil) return out;
    ise_stripe_probe::StripeRecorder port(500);
    if (stroker->StrokePathForUI(path, &port, 100) != kSuccess || port.unreadable || port.Clipped()) return out;
    for (const auto& b : ise_stripe_probe::Merged(port.bands)) {
        const double from = std::max(0.0, (b.first + 50) / 100), to = std::min(1.0, (b.second + 50) / 100);
        if (to - from > 1e-3) out.push_back({from, to});
    }
    return out;
}

/** What a path stroker paints along a straight line from (0, 0) to (length, 0), stroked as the
    item's stroke is (its style, weight, cap, join, and limit; the line already in the port, as
    DrawWord gives it): every paint as a mark (ise_stripe_probe::MarkRecorder). False in `ok`
    when the stroker fails, or paints what the recorder cannot read, or clips. */
std::vector<ise_pattern::Mark> RecordPath(IPathStroker* stroker, IGraphicStyleDescriptor* style, double weight, int32 cap, int32 join,
                                          double limit, const Contour& contour, bool* ok)
{
    *ok = false;
    InterfacePtr<IPathGeometry> path = TempGeometry({contour});
    if (stroker == nil || path == nil) return {};
    ise_stripe_probe::MarkRecorder port;
    port.setlinewidth(weight);
    port.setlinecap(cap);
    port.setlinejoin(join);
    port.setmiterlimit(limit);
    port.newpath();
    port.moveto(contour.start.x, contour.start.y);
    for (const ise_miter::Segment& sg : contour.segments) {
        if (sg.curve) port.curveto(sg.c1.x, sg.c1.y, sg.c2.x, sg.c2.y, sg.p.x, sg.p.y);
        else port.lineto(sg.p.x, sg.p.y);
    }
    if (contour.closed) port.closepath();
    if (stroker->StrokePath(style, path, &port) != kSuccess || port.unreadable || port.Clipped()) return {};
    *ok = true;
    return port.marks;
}

std::vector<ise_pattern::Mark> RecordLine(IPathStroker* stroker, IGraphicStyleDescriptor* style, double weight, int32 cap, int32 join,
                                          double limit, double length, bool* ok)
{
    Contour line;
    line.start = {0, 0};
    line.closed = false;
    ise_miter::Segment s;
    s.p = {length, 0};
    line.segments.push_back(s);
    return RecordPath(stroker, style, weight, cap, join, limit, line, ok);
}

/** A hash of what marks paint along a line (ise_pattern::Signature, with each mark's kind,
    width, cap, join, and points, to a ten-thousandth), for the prepared stroke's key: a pattern
    laid by runs depends on it. */
std::uint64_t MarksKey(const std::vector<ise_pattern::Mark>& marks, double length, double eps)
{
    std::uint64_t h = 14695981039346656037ull;
    auto add = [&h](double v) {
        if (v == 0) v = 0;
        const unsigned char* b = reinterpret_cast<const unsigned char*>(&v);
        for (size_t i = 0; i < sizeof v; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    };
    for (const ise_pattern::Mark& m : marks) {
        add(m.stroke ? 1 : 0); add(m.width); add(m.cap); add(m.join);
        for (const auto& sp : ise_pattern::Signature({m}, length, eps, 1e300)) { add(sp.first); add(sp.second); }
        for (const Contour& c : m.path) {
            add(c.closed ? 1 : 0);
            add(std::round(c.start.x * 1e4)); add(std::round(c.start.y * 1e4));
            for (const ise_miter::Segment& sg : c.segments) {
                if (sg.curve) { add(std::round(sg.c1.x * 1e4)); add(std::round(sg.c1.y * 1e4)); add(std::round(sg.c2.x * 1e4)); add(std::round(sg.c2.y * 1e4)); }
                add(std::round(sg.p.x * 1e4)); add(std::round(sg.p.y * 1e4));
            }
        }
    }
    return h;
}

/** Which way the measured stripes face (MeasureStripes): +1 when the side y < 0 is the edge
    away from the object, as BuiltInStripes counts, -1 when it is the other edge, 0 when the
    built-in Thick - Thin style does not measure as its known layout either way. Measured once. */
int StripeOrientation()
{
    static const int orientation = [] {
        InterfacePtr<IPathStroker> thickThin(static_cast<IPathStroker*>(::CreateObject(kThickThinPathStrokerBoss, IID_IPATHSTROKER)));
        const std::vector<std::pair<double, double>> measured = MeasureStripes(thickThin.get()), known = BuiltInStripes(kThickThinPathStrokerBoss);
        if (measured.size() != known.size()) return 0;
        auto same = [&](bool flip) {
            for (size_t i = 0; i < known.size(); ++i) {
                const auto& k = flip ? known[known.size() - 1 - i] : known[i];
                const double from = flip ? 1 - k.second : k.first, to = flip ? 1 - k.first : k.second;
                if (std::fabs(measured[i].first - from) > 0.01 || std::fabs(measured[i].second - to) > 0.01) return false;
            }
            return true;
        };
        return same(false) ? 1 : same(true) ? -1 : 0;
    }();
    return orientation;
}

/** A custom striped style's stripes, as BuiltInStripes gives them, measured from its own
    stroker (MeasureStripes, StripeOrientation); empty when they cannot be measured. */
std::vector<std::pair<double, double>> CustomStripes(IDataBase* db, UID uid, const Coverage& coverage)
{
    std::vector<std::pair<double, double>> out;
    const int orientation = StripeOrientation();
    InterfacePtr<IPathStroker> stroker(db, uid, IID_IPATHSTROKER);
    const std::vector<std::pair<double, double>> measured = orientation != 0 ? MeasureStripes(stroker.get())
                                                                              : std::vector<std::pair<double, double>>();
    if (coverage.trace) coverage.trace("P custom stripes orientation=%d measured=%d", orientation, static_cast<int>(measured.size()));
    if (orientation > 0) return measured;
    for (auto it = measured.rbegin(); it != measured.rend(); ++it) out.push_back({1 - it->second, 1 - it->first});
    return out;
}

/** The corners whose miters an Outside or Center stroke's outward bands keep
    (ise_region::BandCorner), as StrokeSideJoins treats them at the stroke's reach h past the
    outline (the weight for Outside, half of it for Center): within the limit the whole miter,
    an effect's corner trimmed to its shorter edge, a glyph tip trimmed (TipReach; beside a short
    edge cut flush, which the bands' squared offsets are already); past the limit a shape's own
    point kept whole when the switches keep whole points on patterned strokes, within `bounds`,
    every other corner squared off at each band's distance, as close to InDesign's bevel as
    bands allow. */
std::vector<ise_region::BandCorner> BandCorners(const Prepared& p, double h, double limit, const Coverage& coverage,
                                                const PointBounds& bounds)
{
    std::vector<ise_region::BandCorner> out;
    for (size_t i = 0; i < p.stubs.size(); ++i) {
        const Stub& s = p.stubs[i];
        if (s.interior) continue;
        ise_region::BandCorner c;
        c.stub = s;
        const bool tip = coverage.glyphTips && ise_miter::TrimmedTip(s, h);
        if (!PastLimit(s, limit)) {
            if (tip && s.edge < h) continue;   // flush: the squared offset
            if ((i < p.effect.size() && p.effect[i]) || tip) { c.cap = 1; c.extra = s.edge; }
            else c.cap = s.ratio;
        } else {
            if ((i < p.crossing.size() && p.crossing[i]) || !coverage.wholePoints || !coverage.patternPoints ||
                !ise_miter::WholePoint(s, h)) continue;
            c.cap = bounds.Fit(s, h, std::min(s.ratio, kLongestMiter) * h) / h;
            if (!(c.cap > 0)) continue;   // nothing left within the bounds: squared off
        }
        out.push_back(c);
    }
    return out;
}

/** One outline per word for an Outside patterned stroke (the owner's choice, 2026-10-05):
    InDesign strokes each contour's offset, so where letters touch or overlap their patterns
    cross. For a path stroker (dashes, dots, hashes, waves), the word's path at half the weight
    (ise_region::WordPath: the united offset, its creases rounded), along which the stroker
    draws (DrawWord), with its gap color painted beneath; for dashes, also what that band
    leaves unpainted where letters meet (ise_region::Uncovered). InDesign's own offset
    (IOffsetPath) left creases where the pattern spiked and restarted, two anchors at a corner
    where dots doubled, and a slightly different offset from the notches', which left crescent
    gaps between them (the owner's check, 2026-10-06). For stripes, each
    stripe's region by distance from the fill, and the gap color's (ise_region::DistanceBands):
    the stripes' own offsets, stroked, left the notches white, seams against the fill, and a
    stray island where InDesign's offset folded (the owner's check, 2026-10-06), and now carry
    the whole points. Leaves `out` without them when an offset fails, and the stroke is then
    drawn as for other stroke types.
    A pattern its stroker fits to corners (a corner adjustment: Fitted) is laid along the path
    by runs between its real corners (`record`: what the stroker paints along a straight line
    of a length; ise_pattern::LayContour), not by the stroker along the whole path: its
    adjustment fitted the pattern between every two anchors, which ran dashes solid along
    outlined script (the owner's check of the corner adjustment study, 2026-10-06). A stroker
    that sets a mark of its own at each corner (White Diamond) draws along the path itself,
    since no run can lay that mark (LayContour).
    Center and Inside patterns follow one outline too, where their contours' strokes meet: a
    Center stroke where contours lie within its half width of one another (ContoursInteract),
    an Inside stroke where contours overlap (`resolved`). InDesign strokes each contour, so
    where letters touch and shapes overlap their stripes and dashes crossed (census rows 11, 49,
    and 50, 2026-10-06). The path is the united outline itself for Center, shrunk by half the
    weight for Inside; stripes lie at their distance across the outline (Center) or within it
    (Inside), counted, as for Outside, from the stroke's outer edge (InDesign 21.6's Inside
    Thick - Thin keeps its thick stripe at the shape's edge). Otherwise `out` is left without
    them, and InDesign's own pattern is drawn under the clip. */
void PrepareWord(const std::vector<Contour>& contours, Align align, bool resolved, bool pathStroker, bool dashes, bool stripedPath,
                 const std::vector<std::pair<double, double>>& stripes,
                 bool gap, double weight, int32 join, double limit, const Coverage& coverage, const PointBounds& bounds,
                 const std::function<std::vector<ise_pattern::Mark>(double, bool*)>* record, Prepared* out)
{
    // Within 0.005 pt, and under a fiftieth of the weight for hairlines.
    const double tol = std::min(0.005, weight / 50);
    const ise_region::BandJoin bandJoin = join == kPMRoundJoin ? ise_region::BandJoin::Round
                                          : join == kPMBevelJoin ? ise_region::BandJoin::Bevel
                                                                 : ise_region::BandJoin::Miter;
    const bool outside = align == Align::Outside, center = align == Align::Center, inside = align == Align::Inside;
    // Center: a path stroker only where contours overlap (the united outline of contours that
    // merely come near one another is those contours, whose patterns still cross); stripes also
    // where contours' strokes meet, as the bands by distance join there. Inside: where contours
    // overlap.
    if (center && !resolved && (pathStroker || !ise_region::ContoursInteract(contours, weight / 2, std::max(tol, 0.01)))) return;
    if (inside && !resolved) return;
    // Bounded as the band clips are: the paths and bands offset the outline several times.
    size_t points = 0;
    for (const ise_miter::Ring& r : ise_miter::Flatten(contours, tol, 1e-9)) points += r.points.size();
    if (points > 40000) return;
    // The stroke's outer edge, by signed distance from the outline (outside positive), and the
    // half width a corner's miter is drawn at.
    const double outer = outside ? weight : center ? weight / 2 : 0, h = center ? weight / 2 : weight;
    if (pathStroker) {
        // Creases are rounded only where contours' strokes meet (ise_region::ContoursInteract),
        // and never for stripes, which keep their distance from the fill: along a rounded crease
        // custom stripes left the letters (the owner's check, 2026-10-06).
        const bool creases = outside && !stripedPath && ise_region::ContoursInteract(contours, weight / 2, std::max(tol, 0.01));
        // Fitted within 0.02 pt: the stroker's corner adjustment fits its pattern between every
        // two anchors, so the path keeps as few as it can.
        const double pathTol = std::min(0.02, weight / 50);
        bool ok = false;
        std::vector<Contour> word = ise_region::WordPath(contours, outer - weight / 2, bandJoin, limit, creases, pathTol, &ok);
        if (!ok || word.empty()) return;
        if (dashes && outside) {
            std::vector<Contour> notches = ise_region::Uncovered(contours, word, weight / 2, std::min(0.1, weight / 20), tol, &ok);
            if (!ok) return;
            out->notches.swap(notches);
        } else if (inside) {
            // The parts thinner than the stroke, which the inward path does not reach, filled
            // solid as a solid Inside stroke fills them (ise_region::UncoveredWithin).
            std::vector<Contour> thin = ise_region::UncoveredWithin(contours, weight / 2, std::min(0.1, weight / 20), tol, &ok);
            if (!ok) return;
            out->notches.swap(thin);
        }
        size_t runs = 0;
        for (const Contour& c : word) runs += c.segments.size();
        if (record != nullptr && runs <= 20000) {
            // Runs of the same length (a shape's equal sides) are recorded once.
            std::map<double, std::vector<ise_pattern::Mark>> recorded;
            bool readable = true;
            auto line = [&](double length) {
                auto it = recorded.find(length);
                if (it != recorded.end()) return it->second;
                bool ok = false;
                std::vector<ise_pattern::Mark> marks = (*record)(length, &ok);
                if (!ok) {
                    readable = false;
                    if (coverage.trace) coverage.trace("P pattern run unread length=%.4f", length);
                }
                recorded[length] = marks;
                return marks;
            };
            ise_pattern::Laid laid;
            bool layable = true;
            std::pair<Pt, double> stray;
            for (const Contour& c : word) layable = layable && ise_pattern::LayContour(c, line, 1e-4, 30, &laid, weight, &stray);
            if (!layable && coverage.trace)
                coverage.trace("P pattern corner mark at %.4f %.4f on a side of %.4f: drawn along the path", stray.first.x, stray.first.y, stray.second);
            if (readable && layable) out->laid = std::move(laid);   // otherwise the stroker draws along the path
        }
        out->word.swap(word);
        return;
    }
    if (stripes.empty()) return;
    // Signed distances from the outline, outermost stripe last; the gap color's region from the
    // middle of the innermost stripe (or the stroke's inner edge) to the middle of the outermost
    // (or its outer edge), so its edges lie under stripes.
    std::vector<std::pair<double, double>> bands;
    for (auto it = stripes.rbegin(); it != stripes.rend(); ++it) bands.push_back({outer - it->second * weight, outer - it->first * weight});
    std::sort(bands.begin(), bands.end());
    const size_t count = bands.size();
    const double inner = outer - weight;
    if (gap) {
        const double from = bands.front().first > inner ? inner : (bands.front().first + bands.front().second) / 2;
        const double to = bands.back().second < outer ? outer : (bands.back().first + bands.back().second) / 2;
        bands.push_back({from, to});
    }
    const std::vector<ise_region::BandCorner> corners = bandJoin == ise_region::BandJoin::Miter && outer > 0
                                                           ? BandCorners(*out, h, limit, coverage, bounds)
                                                           : std::vector<ise_region::BandCorner>();
    bool ok = false;
    // Only an Outside stroke's innermost band meets the fill drawn beneath it, so only it is inset.
    std::vector<std::vector<Contour>> regions =
        ise_region::DistanceBands(contours, bands, bandJoin, corners, outside ? 3 * tol : 0, tol, &ok, limit);
    if (!ok) return;
    if (gap) { out->gapBand.swap(regions.back()); regions.pop_back(); }
    regions.resize(count);
    out->bands.swap(regions);
}

/** Everything the route decides for an item from its attributes and outline (Analyze); the
    fill and the stroke events analyze the same item alike, so they always agree on whether the
    route takes it. */
struct Plan {
    InterfacePtr<IPathPageItem> pathItem;
    InterfacePtr<IGraphicStyleDescriptor> style;
    InterfacePtr<IRenderingObject> paint;
    IDataBase* db = nil;
    UID none = kInvalidUID;
    double weight = 0, limit = 0, h = 0;
    int32 join = 0, cap = 0;
    Align align = Align::Center;
    bool solid = false, hasFill = false;
    ClassID stroker = kInvalidClass;   // the stroke type's path stroker, and its document style
    UID strokerUID = kInvalidUID;
    ClassID strokeClass = kInvalidClass;   // a custom style's own class (StrokeClass)
    bool pathStroker = false;              // one outline per word, drawn by the stroker (PatternStroker)
    bool forUI = false;                    // custom stripes: drawn by IPathStroker::StrokePathForUI
    std::uint64_t marksKey = 0;            // what a path stroker paints along a line (MarksKey)
    std::vector<std::pair<double, double>> stripes;   // built-in stripes (BuiltInStripes), when drawn here
    InterfacePtr<IRenderingObject> gap;                // the gap color, when there is one
    bool16 overprintGap = kFalse;
    bool16 overprint = kFalse, overprintFill = kFalse;
    OwnBlending blending;                  // the stroke's and the fill's own opacity and blend mode
    PointBounds pointBounds;               // with object-level transparency: where whole points may reach
    std::vector<Contour> contours;
    std::shared_ptr<const Prepared> prepared;
};

/** Whether the item draws an arrowhead at either end. True also when the attribute cannot be
    read. */
bool Arrowheads(IGraphicAttributeUtils* attrs, IGraphicStyleDescriptor* style)
{
    ClassID start = kInvalidClass, end = kInvalidClass;
    if (attrs->GetLineEndStartImplementation(start, style) != kSuccess ||
        attrs->GetLineEndEndImplementation(end, style) != kSuccess) return true;
    return start != kInvalidClass || end != kInvalidClass;
}

/** Fills `plan` and returns true when this route draws the item's stroke; see the file comment
    for what it takes over. With `probeNative`, the caller only needs the stroke's attributes. */
bool Analyze(IPMUnknown* item, const Coverage& coverage, Plan* plan)
{
    plan->pathItem = InterfacePtr<IPathPageItem>(item, UseDefaultIID());
    InterfacePtr<IPathGeometry> geometry(item, UseDefaultIID());
    plan->style = InterfacePtr<IGraphicStyleDescriptor>(item, UseDefaultIID());
    InterfacePtr<IShape> shape(item, UseDefaultIID());
    IDataBase* db = ::GetDataBase(item);
    IGraphicStyleDescriptor* style = plan->style.get();
    if (plan->pathItem == nil || geometry == nil || style == nil || shape == nil || db == nil) return false;
    plan->db = db;

    // The stroke's attributes.
    const int32 paths = geometry->GetNumPaths();
    if (paths <= 0) return false;
    bool anyOpen = false;
    for (int32 i = 0; i < paths; ++i) anyOpen = anyOpen || !geometry->IsPathClosed(i);
    Utils<IGraphicAttributeUtils> attrs;
    plan->none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    UID strokePaint = kInvalidUID, fillPaint = kInvalidUID;
    if (attrs->GetStrokeRenderingUID(strokePaint, style) != kSuccess || strokePaint == kInvalidUID ||
        strokePaint == plan->none) return false;
    plan->hasFill = attrs->GetFillRenderingUID(fillPaint, style) == kSuccess && fillPaint != kInvalidUID && fillPaint != plan->none;
    PMReal weightValue = 0, limitValue = 0;
    int32 alignment = 0;
    if (attrs->GetStrokeWeight(weightValue, style) != kSuccess || attrs->GetJoinType(plan->join, style) != kSuccess ||
        attrs->GetStrokeAlignment(alignment, style) != kSuccess || attrs->GetMiterLimit(limitValue, style) != kSuccess ||
        attrs->GetLineCap(plan->cap, style) != kSuccess) return false;
    plan->weight = ToDouble(weightValue);
    plan->limit = ToDouble(limitValue);
    const double weight = plan->weight, limit = plan->limit;
    const int32 join = plan->join;
    if (!(weight > 0) || weight > 10000 || !(limit >= 1)) return false;
    if (join != kPMMiterJoin && join != kPMRoundJoin && join != kPMBevelJoin) return false;
    switch (alignment) {
    case kStrokeAlignCenter: plan->align = Align::Center; break;
    case kStrokeAlignOutside: plan->align = Align::Outside; break;
    case kStrokeAlignInside: plan->align = Align::Inside; break;
    default: return false;
    }
    const Align align = plan->align;
    ClassID stroker = kInvalidClass;
    UID strokerUID = kInvalidUID;
    if (attrs->GetLineImplementation(stroker, strokerUID, style) != kSuccess) return false;
    plan->stroker = stroker;
    plan->strokerUID = strokerUID;
    plan->solid = stroker == kSolidPathStrokerBoss;
    const bool solid = plan->solid;
    plan->strokeClass = StrokeClass(db, stroker, strokerUID);
    const ClassID strokeClass = plan->strokeClass;
    if (!solid && coverage.wordOutline && (align == Align::Outside || coverage.overlaps)) {
        // One outline per word (PrepareWord): pattern strokers, and stripes, each with its gap
        // color; for Center and Inside, where contours meet (the overlaps switch).
        plan->stripes = strokeClass == kCustomStripedPathStrokerBoss ? CustomStripes(db, strokerUID, coverage) : BuiltInStripes(strokeClass);
        plan->forUI = strokeClass == kCustomStripedPathStrokerBoss && plan->stripes.empty();
        plan->pathStroker = (PatternStroker(strokeClass) || plan->forUI) && !Refused().Has(strokeClass);
        UID gap = kInvalidUID;
        if ((plan->pathStroker || !plan->stripes.empty()) && attrs->GetGapRenderingUID(gap, style) == kSuccess &&
            gap != kInvalidUID && gap != plan->none) {
            plan->gap = InterfacePtr<IRenderingObject>(db, gap, UseDefaultIID());
            if (plan->gap == nil || attrs->GetOverprintGap(plan->overprintGap, style) != kSuccess) return false;
        }
    }
    const bool pathStroker = plan->pathStroker, dashes = pathStroker && DashStroker(strokeClass);
    if (coverage.trace && !solid)
        coverage.trace("P stroker class=0x%x own class=0x%x document style=%d path stroker=%d dashes=%d stripes=%d gap=%d",
                       static_cast<unsigned>(stroker.Get()), static_cast<unsigned>(strokeClass.Get()), strokerUID != kInvalidUID ? 1 : 0,
                       pathStroker ? 1 : 0, dashes ? 1 : 0, static_cast<int>(plan->stripes.size()), plan->gap != nil ? 1 : 0);
    bool16 evenOdd = kFalse;
    if (attrs->GetEvenOdd(evenOdd, style) == kSuccess && evenOdd) return false;   // filled even-odd
    InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
    if (xp == nil) return false;
    if (xp->ItemHasTransparency(shape) != 0) {
        // Opacity and blending are taken over: the object's applies around the whole item, the
        // stroke's and the fill's are applied here (OwnBlending); effects on the stroke or the
        // fill stay InDesign's.
        if (!coverage.transparency || !OwnTransparency(style, coverage, ::GetUID(item), &plan->blending)) return false;
        if (!coverage.probeNative && ObjectBlending(style, coverage, ::GetUID(item)))
            plan->pointBounds.Within(shape->GetPaintedBBox(PMMatrix(), Transform::PasteboardCoordinates()));
    }
    // A group around the item with opacity or blending bounds its whole points as well (its own
    // transparency group), though the item reports no transparency of its own.
    if (!coverage.probeNative && xp->ItemHasTransparencyInParentHierarchy(shape) != 0) {
        InterfacePtr<IHierarchy> node(item, UseDefaultIID());
        InterfacePtr<IHierarchy> parent(node ? node->QueryParent() : nil);
        for (int depth = 0; parent != nil && depth < 64; ++depth) {
            InterfacePtr<IGraphicStyleDescriptor> parentStyle(parent, UseDefaultIID());
            InterfacePtr<IShape> parentShape(parent, UseDefaultIID());
            // Past the page items (a layer, the spread), or at an inline's anchor: past it lies
            // the text frame, whose painted bounds need not hold an anchored object hanging out
            // of it, so a group beyond does not bound the item's points.
            if (parentStyle == nil || parentShape == nil) break;
            if (ObjectBlending(parentStyle, coverage, ::GetUID(parent)))
                plan->pointBounds.Within(parentShape->GetPaintedBBox(PMMatrix(), Transform::PasteboardCoordinates()));
            InterfacePtr<IHierarchy> up(parent->QueryParent());
            parent = up;
        }
    }
    if (plan->pointBounds.on) {
        const PMMatrix m = ::InnerToPasteboardMatrix(item);
        for (int k = 0; k < 6; ++k) plan->pointBounds.m[k] = ToDouble(m[k]);
    }
    if (!Invertible(item)) return false;
    plan->paint = InterfacePtr<IRenderingObject>(db, strokePaint, UseDefaultIID());
    if (plan->paint == nil) return false;
    // Overprint is set explicitly: IRenderingObject::Install does not carry it, and the PDF
    // port wrote the plugin's strokes and fills without it (measured 2026-10-04).
    if (attrs->GetOverprintStroke(plan->overprint, style) != kSuccess ||
        attrs->GetOverprintFill(plan->overprintFill, style) != kSuccess) return false;
    if (coverage.probeNative) return true;

    // Which strokes this route draws.
    if (align == Align::Center && join != kPMMiterJoin) return false;      // nothing to correct
    // Open paths: Center, Miter, solid, without arrowheads (see the file comment).
    if (anyOpen && (!coverage.openPaths || align != Align::Center || !solid || Arrowheads(attrs, style))) return false;
    if (align == Align::Inside && !coverage.inside) return false;

    // A path stroker's pattern: what it paints along a straight line, part of what is prepared
    // (MarksKey), as a pattern laid by runs between the path's real corners (PrepareWord)
    // depends on it. One recording at every analysis; whether the stroker fits its pattern to a
    // path's length (a corner adjustment), only when preparing.
    InterfacePtr<IPathStroker> patternStroker;
    std::vector<ise_pattern::Mark> shortLine;
    bool lineRead = false;
    // Two lengths no common pattern divides alike, so that a fitted pattern differs.
    const double lineA = 41.37 * weight, lineB = 57.71 * weight, markEps = 1e-4;
    if (pathStroker && !plan->forUI) {
        if (strokerUID != kInvalidUID) patternStroker = InterfacePtr<IPathStroker>(db, strokerUID, IID_IPATHSTROKER);
        if (patternStroker == nil) patternStroker = InterfacePtr<IPathStroker>(static_cast<IPathStroker*>(::CreateObject(stroker, IID_IPATHSTROKER)));
        shortLine = RecordLine(patternStroker.get(), style, weight, plan->cap, join, limit, lineA, &lineRead);
        if (lineRead) plan->marksKey = MarksKey(shortLine, lineA, markEps);
        else if (coverage.trace) coverage.trace("P pattern line unread");
    }

    ise_miter::Recorder recorder(anyOpen);
    Recording writer(&recorder);
    plan->pathItem->GetOutlineInfo(&writer);
    plan->contours = recorder.Contours();
    std::vector<Contour>& contours = plan->contours;
    if (contours.empty()) return false;
    if (!anyOpen) for (Contour& c : contours) c.closed = true;
    // With corner effects, the anchors of the path the user drew (inner coordinates, as the
    // outline): corners off them are the effect's.
    std::vector<Pt> anchors;
    const bool effects = coverage.effectCorners && CornerEffects(attrs, style);
    if (effects)
        for (int32 i = 0; i < paths; ++i)
            for (int32 j = 0, n = geometry->GetNumPoints(i); j < n; ++j) {
                const PMPoint& a = geometry->GetNthPoint(i, j).GetAnchorPoint();
                anchors.push_back({ToDouble(a.X()), ToDouble(a.Y())});
            }

    plan->h = align == Align::Center ? weight / 2 : weight;   // half the drawn width
    const double h = plan->h;
    Hash key;
    for (const Contour& c : contours) {
        key.Add(c.start);
        key.Add(static_cast<int32>(c.closed));
        for (const ise_miter::Segment& sg : c.segments) {
            key.Add(static_cast<int32>(sg.curve));
            if (sg.curve) { key.Add(sg.c1); key.Add(sg.c2); }
            key.Add(sg.p);
        }
        key.Add(static_cast<int32>(-1));
    }
    key.Add(h); key.Add(limit); key.Add(static_cast<int32>(align)); key.Add(join); key.Add(static_cast<int32>(solid));
    key.Add(static_cast<int32>(effects));
    for (const Pt& a : anchors) key.Add(a);
    key.Add(static_cast<int32>(pathStroker) | static_cast<int32>(dashes) << 1 | static_cast<int32>(plan->gap != nil) << 2 |
            static_cast<int32>(plan->forUI) << 3 | static_cast<int32>(lineRead) << 4);
    if (lineRead) key.Add(&plan->marksKey, sizeof plan->marksKey);
    key.Add(static_cast<int32>(coverage.wholePoints) | static_cast<int32>(coverage.overlaps) << 1 |
            static_cast<int32>(coverage.glyphTips) << 2 | static_cast<int32>(coverage.patternPoints) << 3 |
            static_cast<int32>(coverage.wordOutline) << 4);
    for (const auto& st : plan->stripes) { key.Add(st.first); key.Add(st.second); }
    // The point bounds, which only the stripes' bands carry (BandCorners); relative to the item,
    // so that moving it keeps what is prepared.
    const bool bandBounds = plan->pointBounds.on && !plan->stripes.empty() && !pathStroker;
    key.Add(static_cast<int32>(bandBounds));
    if (bandBounds) {
        const PointBounds& pb = plan->pointBounds;
        for (int k = 0; k < 4; ++k) key.Add(pb.box[k] - pb.m[4 + k % 2]);
        for (int k = 0; k < 4; ++k) key.Add(pb.m[k]);
    }
    const bool centerWord = align == Align::Center && !solid && coverage.wordOutline && coverage.overlaps &&
                            (pathStroker || !plan->stripes.empty());
    std::shared_ptr<const Prepared> prepared = Cache().Find(key.Value());
    // A run is measured on a square whose sides are as long (ise_pattern::RunSquare).
    const std::function<std::vector<ise_pattern::Mark>(double, bool*)> record = [&](double length, bool* ok) {
        return RecordPath(patternStroker.get(), style, weight, plan->cap, join, limit, ise_pattern::RunSquare(length), ok);
    };
    if (!prepared) {
        std::shared_ptr<Prepared> made = std::make_shared<Prepared>();
        Prepare(contours, effects ? &anchors : nullptr, align, join, solid, h, limit, centerWord, coverage, made.get());
        // A pattern its stroker fits to a path's length is laid by runs (ise_pattern::Fitted, on
        // a second line), when every mark can be laid so (ise_pattern::Layable: not a wave).
        bool layRuns = false;
        if (made->ok && lineRead && !shortLine.empty()) {
            bool longRead = false;
            const std::vector<ise_pattern::Mark> longLine = RecordLine(patternStroker.get(), style, weight, plan->cap, join, limit, lineB, &longRead);
            layRuns = longRead && ise_pattern::Layable(shortLine, markEps, 3 * weight) && ise_pattern::Fitted(shortLine, lineA, longLine, lineB, markEps);
            if (coverage.trace)
                coverage.trace("P pattern marks=%d long line read=%d layable=%d laid by runs=%d", static_cast<int>(shortLine.size()), longRead ? 1 : 0,
                               ise_pattern::Layable(shortLine, markEps, 3 * weight) ? 1 : 0, layRuns ? 1 : 0);
        }
        if (made->ok && (pathStroker || !plan->stripes.empty()))
            PrepareWord(made->resolved ? made->stroked : contours, align, made->resolved, pathStroker, dashes, plan->forUI, plan->stripes,
                        plan->gap != nil, weight, join,
                        limit, coverage, plan->pointBounds, layRuns ? &record : nullptr, made.get());
        size_t words = Segments(made->word) + Segments(made->notches) + Segments(made->gapBand);
        for (const auto& band : made->bands) words += Segments(band);
        for (const auto& st : made->laid.strokes) words += Segments(st.pieces);
        for (const auto& fill : made->laid.fills) words += Segments(fill);
        Cache().Keep(key.Value(), made, Segments(contours) + Segments(made->stroked) + Segments(made->outsideClip) +
                                        Segments(made->strokeClip) + made->stubs.size() + words);
        prepared = made;
    }
    plan->prepared = prepared;
    return prepared->ok;
}

/** The stroke's own opacity and blend mode (OwnBlending), applied to everything the stroke draws
    at once, as InDesign applies them to its single stroke: an isolated transparency group
    composited with them, so that the parts of the stroke that overlap (the stroke and its
    joins, a pattern's pieces and its points) do not show through one another, as they did in
    InDesign's own drawing of overlapping contours (census rows 60, 61, and 75, 2026-10-06).
    Nothing without them. */
class BlendedStroke {
public:
    BlendedStroke(IGraphicsPort* port, IDataBase* db, const OwnBlending& blending, const PMRect& bounds, bool overprint) : fPort(port)
    {
        if (!blending.Stroke()) return;
        InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
        if (xp == nil) return;
        AGMColorSpace* space = xp->GetDocumentBlendingSpace();
        port->gsave();
        port->setopacity(blending.strokeOpacity / 100, kFalse);
        port->setblendingmode(blending.strokeBlend);
        // Isolated, unless the stroke overprints: overprint needs the backdrop within the group.
        port->starttransparencygroup(bounds, space, overprint ? kFalse : kTrue, kFalse);
        if (space != nullptr) xp->ReleaseBlendingSpace(space);
        fOpen = true;
    }
    ~BlendedStroke()
    {
        if (!fOpen) return;
        fPort->endtransparencygroup();
        fPort->grestore();
    }
    BlendedStroke(const BlendedStroke&) = delete;
    BlendedStroke& operator=(const BlendedStroke&) = delete;
private:
    IGraphicsPort* fPort;
    bool fOpen = false;
};

/** Everything a stroke can paint, in the item's inner coordinates: the outline's bounds grown by
    the longest join or point drawn and the weight. */
PMRect StrokeBounds(const std::vector<Contour>& contours, const std::vector<Stub>& stubs, double weight, double h, double limit)
{
    double reach = std::max(limit, 1.0) * h;
    for (const Stub& s : stubs) reach = std::max(reach, ise_miter::WholePointReach(s, h, kLongestMiter));
    const double grow = reach + 2 * weight + 1;
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    auto add = [&](Pt p) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); };
    for (const Contour& c : contours) {
        add(c.start);
        for (const ise_miter::Segment& sg : c.segments) { if (sg.curve) { add(sg.c1); add(sg.c2); } add(sg.p); }
    }
    if (x0 > x1) return PMRect(0, 0, 0, 0);
    return PMRect(x0 - grow, y0 - grow, x1 + grow, y1 + grow);
}

/** Draws a patterned stroke along one outline per word (PrepareWord). Stripes: each stripe's
    region filled, over the gap color's (an Inside stroke's within the fill). A path stroker: its
    pattern (laid by runs, or drawn by the stroker) along the word's path, under the stroke's
    clip, over the gap color's band, with the notches its dashes leave or the thin parts an
    Inside path misses; then the whole points past the limit as for other patterned strokes.
    False when the stroker fails, which is then remembered (Refused). */
bool DrawWord(IGraphicsPort* port, const Plan& plan, const Prepared& prepared, const std::vector<std::vector<Pt>>& points,
              const Coverage& coverage)
{
    if (!prepared.bands.empty()) {
        AutoGSave save(port);
        if (plan.align == Align::Inside) {
            // Within the fill: an Inside stroke's bands, built from the region without the tiny
            // holes a corner effect leaves (Prepare), keep off those holes as the fill does.
            port->newpath();
            EmitContours(port, plan.contours);
            port->clip();
        }
        if (plan.gap != nil && !prepared.gapBand.empty()) {
            AutoGSave gapScope(port);
            plan.gap->Install(port, plan.style.get(), IRenderingObject::kInstallGap);
            if (plan.overprintGap) port->setoverprint(kTrue);
            port->newpath();
            EmitContours(port, prepared.gapBand);
            port->fill();
        }
        plan.paint->Install(port, plan.style.get(), IRenderingObject::kInstallStroke);
        if (plan.overprint) port->setoverprint(kTrue);
        port->newpath();
        for (const std::vector<Contour>& band : prepared.bands) EmitContours(port, band);
        port->fill();
        return true;
    }
    InterfacePtr<IPathStroker> stroker;
    InterfacePtr<IPathGeometry> path;
    const bool laid = !prepared.laid.Empty();
    if (!laid) {
        if (plan.strokerUID != kInvalidUID) stroker = InterfacePtr<IPathStroker>(plan.db, plan.strokerUID, IID_IPATHSTROKER);
        if (stroker == nil) stroker = InterfacePtr<IPathStroker>(static_cast<IPathStroker*>(::CreateObject(plan.stroker, IID_IPATHSTROKER)));
        path = TempGeometry(prepared.word);
        if (stroker == nil || path == nil) { Refused().Add(plan.strokeClass); return false; }
    }
    InsideStroke inside;
    {
        AutoGSave save(port);
        const std::vector<Contour>& clip = prepared.outsideClip.empty() ? prepared.strokeClip : prepared.outsideClip;
        if (!clip.empty()) {
            port->newpath();
            EmitContours(port, clip);
            port->clip();
        }
        port->setlinejoin(plan.join);
        port->setmiterlimit(plan.limit);
        port->setlinecap(plan.cap);
        // Undashed: the gap color's band is solid, and the laid pattern carries its own dashes.
        port->setdash(0, nullptr, 0);
        if (plan.gap != nil) {
            // The gap color: InDesign paints it beneath a pattern, which the stroker does not.
            AutoGSave gapScope(port);
            plan.gap->Install(port, plan.style.get(), IRenderingObject::kInstallGap);
            if (plan.overprintGap) port->setoverprint(kTrue);
            port->setlinewidth(plan.weight);
            port->newpath();
            EmitContours(port, prepared.word);
            port->stroke();
        }
        plan.paint->Install(port, plan.style.get(), IRenderingObject::kInstallStroke);
        if (plan.overprint) port->setoverprint(kTrue);
        if (laid) {
            // The pattern laid by runs (PrepareWord): each stroke's pieces with its width, cap,
            // and join, undashed, then each mark the stroker fills.
            for (const ise_pattern::Laid::Stroke& st : prepared.laid.strokes) {
                if (st.pieces.empty()) continue;
                port->setlinewidth(st.width);
                port->setlinecap(st.cap);
                port->setlinejoin(st.join);
                port->newpath();
                EmitContours(port, st.pieces);
                port->stroke();
            }
            for (const std::vector<Contour>& fill : prepared.laid.fills) {
                port->newpath();
                EmitContours(port, fill);
                port->fill();
            }
        }
        // The stroker strokes the path already in the port, with the port's paint and width.
        port->setlinewidth(plan.weight);
        port->newpath();
        if (!laid) EmitContours(port, prepared.word);
        const ErrorCode rc = laid ? kSuccess : plan.forUI ? stroker->StrokePathForUI(path, port, plan.weight)
                                                          : stroker->StrokePath(plan.style.get(), path, port);
        if (rc != kSuccess) {
            if (coverage.trace) coverage.trace("P stroker refused class=0x%x", static_cast<unsigned>(plan.strokeClass.Get()));
            Refused().Add(plan.strokeClass);
            return false;
        }
        if (!prepared.notches.empty()) {
            port->newpath();
            EmitContours(port, prepared.notches);
            port->fill();
        }
    }
    if (!points.empty()) {
        // Unclipped, as for other patterned strokes: a whole point reaches past the band clip.
        AutoGSave save(port);
        plan.paint->Install(port, plan.style.get(), IRenderingObject::kInstallStroke);
        if (plan.overprint) port->setoverprint(kTrue);
        FillPolygons(port, points);
    }
    return true;
}

} // namespace

bool DrawFill(GraphicsData* gd, IPMUnknown* item, const Coverage& coverage)
{
    IGraphicsPort* port = gd ? gd->GetGraphicsPort() : nil;
    if (port == nil || item == nil || tInsideStroke || coverage.probeNative) return false;
    Plan plan;
    if (!Analyze(item, coverage, &plan) || plan.align == Align::Center || !plan.hasFill) return false;
    // InDesign fills an Outside- or Inside-stroked item's offset outline, which breaks on curved
    // outlines, loops, and corner effects (spilling past them, or flooding a letter's opening);
    // the outline itself is filled instead, in the fill's place in the drawing order, beneath
    // any content (see the file comment).
    Refill(port, plan.db, plan.style.get(), plan.none, plan.contours, plan.overprintFill, plan.blending);
    return true;
}

bool DrawStroke(GraphicsData* gd, IPMUnknown* item, const Coverage& coverage)
{
    IGraphicsPort* port = gd ? gd->GetGraphicsPort() : nil;
    if (port == nil || item == nil || tInsideStroke) return false;
    Plan plan;
    const bool taken = Analyze(item, coverage, &plan);
    if (coverage.probeNative)
        return taken && StrokeNatively(port, plan.pathItem.get(), plan.style.get(), plan.paint.get(), {}, plan.weight, plan.join,
                                       plan.limit, plan.cap, plan.overprint);
    if (!taken) return false;
    const BlendedStroke blended(port, plan.db, plan.blending,
                                StrokeBounds(plan.contours, plan.prepared->stubs, plan.weight, plan.h, plan.limit),
                                plan.overprint || plan.overprintGap);
    IGraphicStyleDescriptor* style = plan.style.get();
    IRenderingObject* paint = plan.paint.get();
    const Align align = plan.align;
    const int32 join = plan.join, cap = plan.cap;
    const double weight = plan.weight, limit = plan.limit, h = plan.h;
    const std::vector<Contour>& contours = plan.contours;
    const Prepared& prepared = *plan.prepared;
    const bool center = align == Align::Center, outside = align == Align::Outside, inside = align == Align::Inside;
    const std::vector<Contour>& path = prepared.resolved ? prepared.stroked : contours;
    const std::vector<Stub>& stubs = prepared.stubs;
    // Outside and Inside: the outline was filled at the fill event (DrawFill), in place of
    // InDesign's offset fill.

    if (!plan.solid && !prepared.bands.empty()) return DrawWord(port, plan, prepared, {}, coverage);   // whole points in the bands
    if (!plan.solid && !prepared.word.empty() &&
        DrawWord(port, plan, prepared, PatternPoints(stubs, prepared.crossing, align, join, h, limit, coverage, plan.pointBounds), coverage))
        return true;   // a refused stroker keeps InDesign's stroke below
    if (!plan.solid) {
        // InDesign's own stroke, clipped to where a stroke of this reach can paint: escaping
        // tips and stroke inside the letters are cut away, the pattern is InDesign's. Then the
        // whole points past the limit, solid, beyond InDesign's bevel.
        if (!StrokeNatively(port, plan.pathItem.get(), style, paint, prepared.strokeClip, weight, join, limit, cap, plan.overprint))
            return false;
        const std::vector<std::vector<Pt>> points = PatternPoints(stubs, prepared.crossing, align, join, h, limit, coverage, plan.pointBounds);
        if (!points.empty()) {
            AutoGSave save(port);
            paint->Install(port, style, IRenderingObject::kInstallStroke);
            if (plan.overprint) port->setoverprint(kTrue);
            FillPolygons(port, points);
        }
        return true;
    }

    AutoGSave save(port);
    paint->Install(port, style, IRenderingObject::kInstallStroke);
    if (plan.overprint) port->setoverprint(kTrue);   // the stroke and its joins
    port->setlinewidth(2 * h);
    port->setlinecap(cap);
    port->setmiterlimit(limit);
    if (outside || inside) {
        // Outside: clipped to outside the fill, beyond every join. Inside: to the fill, a
        // nonzero clip of the outline, which reads it exactly also where contours overlap.
        // Not IPathPageItem::Clip: InDesign's PDF port wrote that clip without its path (`W n`).
        port->newpath();
        EmitContours(port, outside ? prepared.outsideClip : contours);
        port->clip();
    }
    port->setlinejoin(join == kPMRoundJoin ? kPMRoundJoin : kPMBevelJoin);
    port->newpath();
    EmitContours(port, path);
    port->stroke();
    FillPolygons(port, StrokeSideJoins(stubs, prepared.crossing, prepared.effect, align, join, h, limit, coverage, plan.pointBounds));
    if (center) {
        // Each interior corner's miter, within the limit, clipped to the fill: clipped to the
        // recorded outline, not with IPathPageItem::Clip, which InDesign's PDF port wrote
        // without its path and on a translated item dropped with the stubs (2026-10-04).
        std::vector<std::vector<Pt>> interior;
        for (const Stub& s : stubs)
            if (s.interior && !PastLimit(s, limit)) interior.push_back(ise_miter::JoinPolygon(s, h, s.ratio * h));
        if (!interior.empty()) {
            AutoGSave clipScope(port);
            port->newpath();
            EmitContours(port, contours);
            port->clip();
            FillPolygons(port, interior);
        }
    }
    return true;
}

} // namespace ise_page_stroke
