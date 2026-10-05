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
//                         corner's miter join filled (a point past the limit kept whole up to
//                         the corner's shorter edge, ise_miter::PointReach); each interior
//                         corner's miter filled, clipped to the fill. Center round and bevel
//                         joins have nothing to correct and stay InDesign's.
//     Outside, any join   the outline filled again (the item's fill, gradients included), then
//                         stroked at twice the weight, clipped to outside the fill
//                         (ise_miter::OutsideClip), with the join's corners: a miter's
//                         exterior corners filled as for Center; a round join's are the round
//                         stroke's own; a bevel join's exterior corners get the beveled offset's
//                         stroke (OffsetBevel). Interior miters are dropped.
//     Inside, any join    the outline filled again, then stroked at twice the weight, clipped
//                         to the fill; the interior corners get the inward offset's join: a
//                         miter within the limit filled whole, past it or for a bevel join the
//                         beveled offset's stroke, a round join the round stroke's own. Only
//                         for outlines of straight segments without corner effects, in an item
//                         without content: on curves and corner effects InDesign's own inward
//                         offset fill spills past the outline (up to 1,700 pt2 on a line of
//                         outlined text in the census), which a stroke clipped to the fill
//                         no longer covers, and which the plugin cannot take back.
//   Open paths stay InDesign's.
//   Other stroke types (dashes, dots, stripes, any stroke style): InDesign's own stroke
//     (IPathPageItem::Stroke) drawn from here under a clip (ise_region::StrokeClip): within a
//     little more than the stroke's reach of the outline, plus each corner's join, and for
//     Outside outside the fill, for Inside within it. That removes escaping tips and the
//     stroke shown through the letters while keeping InDesign's pattern; Outside and Inside
//     are refilled first. Points past the limit stay as InDesign draws them.
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
// A frame with content (text, a placed graphic, pasted items: any child) is not refilled:
// InDesign strokes a frame after drawing its content, so a refill at the stroke would paint
// the frame's fill over its text or image. A frame's own outline is a plain shape, whose
// offset fill does not break.
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
// item, the plugin's stroke included, inside them). Stroke-level and fill-level transparency
// and effects are not taken over: InDesign applies them around its own stroke and fill
// (CGraphicFrameShape's adornments), and claiming the stroke draws without them (the census
// showed a 50% stroke opaque, a stroke's drop shadow lost, and a refill opaque over a 50% fill).
//
// Never taken over: an item filled even-odd (InDesign's even-odd attribute), a stroke
// without paint or weight, a singular transform, an open path.
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
#include "PMMatrix.h"
#include "TransformUtils.h"
#include "Utils.h"

// Project includes:
#include "IsePageItemStroke.h"
#include "IseMiterCore.h"
#include "IseRegionCore.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
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

/** Whether the stroke or the fill has transparency or effects of its own (not the object's):
    opacity, a blend mode, a drop shadow, a feather, a glow, an inner shadow, bevel and emboss,
    satin, a directional or gradient feather. True also when an attribute cannot be read. */
bool StrokeOrFillTransparency(IGraphicStyleDescriptor* style, const Coverage& coverage, UID uid)
{
    Utils<IXPAttributeUtils> xp;
    if (!xp) return true;
    typedef IXPAttributeSuite S;
    const S::AttributeType opacity[] = {S::kStrokeBlendingOpacity, S::kFillBlendingOpacity};
    const S::AttributeType blend[] = {S::kStrokeBlendingBlendMode, S::kFillBlendingBlendMode};
    const S::AttributeType shadow[] = {S::kStrokeDropShadowMode, S::kFillDropShadowMode};
    const S::AttributeType feather[] = {S::kStrokeFeatherMode, S::kFillFeatherMode};
    const S::AttributeType applied[] = {
        S::kStrokeInnerShadowApply, S::kStrokeOuterGlowApply, S::kStrokeInnerGlowApply, S::kStrokeBevelEmbossApply,
        S::kStrokeSatinApply, S::kStrokeDirectionalFeatherApply, S::kStrokeGradientFeatherApply,
        S::kFillInnerShadowApply, S::kFillOuterGlowApply, S::kFillInnerGlowApply, S::kFillBevelEmbossApply,
        S::kFillSatinApply, S::kFillDirectionalFeatherApply, S::kFillGradientFeatherApply};
    S::AttributeValue v;
    for (S::AttributeType t : opacity)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypeReal || v.GetReal() < 100) return true;
    for (S::AttributeType t : blend)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypePMBlendingMode ||
            v.GetPMBlendingMode() != kPMBlendNormal) return true;
    for (S::AttributeType t : shadow)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypePMDropShadowMode ||
            v.GetPMDropShadowMode() != kDSMNone) return true;
    for (S::AttributeType t : feather)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypePMVignetteMode ||
            v.GetPMVignetteMode() != kVTMNone) return true;
    for (S::AttributeType t : applied)
        if (!ReadXP(xp, style, t, v, coverage, uid) || v.GetDataType() != S::kDataTypeBoolean || v.GetBoolean()) return true;
    return false;
}

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
    corner effect adds, whose miters within the limit are trimmed (EffectMiterReach). */
std::vector<std::vector<Pt>> StrokeSideJoins(const std::vector<Stub>& stubs, const std::vector<char>& crossing,
                                             const std::vector<char>& effect, Align align, int32 join, double h, double limit)
{
    std::vector<std::vector<Pt>> out;
    const bool wantInterior = align == Align::Inside;
    for (size_t i = 0; i < stubs.size(); ++i) {
        const Stub& s = stubs[i];
        if (s.interior != wantInterior) continue;
        if (join == kPMRoundJoin) continue;   // the round stroke's own join is the offset's
        if (i < crossing.size() && crossing[i] && PastLimit(s, limit)) continue;   // the stroke's bevel
        if (join == kPMMiterJoin && !PastLimit(s, limit)) {
            const bool trimmed = i < effect.size() && effect[i];
            out.push_back(ise_miter::JoinPolygon(s, h, trimmed ? ise_miter::EffectMiterReach(s, h) : s.ratio * h));
        } else if (join == kPMMiterJoin && align != Align::Inside) {
            // Past the limit: the point kept whole or cut (the owner's whole points).
            out.push_back(ise_miter::JoinPolygon(s, h, ise_miter::PointReach(s, h, limit, kLongestMiter)));
        } else {
            // The beveled offset's stroke, with the outline's own stroke beside the corner
            // (JoinPolygon cut at the vertex: its bands and bevel) so no seam shows between them.
            out.push_back(ise_miter::JoinPolygon(s, h, 0));
            out.push_back(ise_miter::OffsetBevel(s, h, limit, join == kPMMiterJoin));
        }
    }
    return out;
}

/** The outline filled again in the item's fill paint, gradients included, nonzero as InDesign
    fills: identical pixels where InDesign's offset fill is whole, and the fill where it broke.
    Not IPathPageItem::Fill: for an Outside or Inside stroke that fills the offset path. */
void Refill(IGraphicsPort* port, IDataBase* db, IGraphicStyleDescriptor* style, UID none, const std::vector<Contour>& contours,
            bool16 overprintFill)
{
    UID fillPaint = kInvalidUID;
    if (Utils<IGraphicAttributeUtils>()->GetFillRenderingUID(fillPaint, style) != kSuccess || fillPaint == kInvalidUID ||
        fillPaint == none) return;
    InterfacePtr<IRenderingObject> fill(db, fillPaint, UseDefaultIID());
    if (fill == nil) return;
    AutoGSave refill(port);
    fill->Install(port, style, IRenderingObject::kInstallFill);
    if (overprintFill) port->setoverprint(kTrue);
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
             double limit, Prepared* out)
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
    if (!center && !ise_region::FillRulesAgree(contours, checkTol)) {
        out->stroked = ise_region::NonzeroRegion(contours, regionTol);
        if (out->stroked.empty()) return;
        out->resolved = true;
    }
    const std::vector<Contour>& path = out->resolved ? out->stroked : contours;
    ise_miter::Options options;
    options.halfWidth = h;
    out->stubs = ise_miter::Corners(path, options, !outside);
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
            if (!s.interior && PastLimit(s, limit)) reach = std::max(reach, ise_miter::PointReach(s, h, limit, kLongestMiter) / h);
        // The frame lies past the longest join drawn.
        out->outsideClip = ise_miter::OutsideClip(path, (reach + 3) * h + 10, 0.001 * h, 0.01 * h, out->resolved, false);
        if (out->outsideClip.empty() || !ise_region::ClipsOutside(out->outsideClip, contours, checkTol)) return;
        if (!solid) out->strokeClip = out->outsideClip;
    }
    out->ok = true;
}

} // namespace

bool DrawStroke(GraphicsData* gd, IPMUnknown* item, const Coverage& coverage)
{
    IGraphicsPort* port = gd ? gd->GetGraphicsPort() : nil;
    if (port == nil || item == nil || tInsideStroke) return false;
    InterfacePtr<IPathPageItem> pathItem(item, UseDefaultIID());
    InterfacePtr<IPathGeometry> geometry(item, UseDefaultIID());
    InterfacePtr<IGraphicStyleDescriptor> style(item, UseDefaultIID());
    InterfacePtr<IShape> shape(item, UseDefaultIID());
    IDataBase* db = ::GetDataBase(item);
    if (pathItem == nil || geometry == nil || style == nil || shape == nil || db == nil) return false;

    // The stroke's attributes.
    const int32 paths = geometry->GetNumPaths();
    if (paths <= 0) return false;
    bool anyOpen = false;
    for (int32 i = 0; i < paths; ++i) anyOpen = anyOpen || !geometry->IsPathClosed(i);
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
    if (!(weight > 0) || weight > 10000 || !(limit >= 1)) return false;
    if (join != kPMMiterJoin && join != kPMRoundJoin && join != kPMBevelJoin) return false;
    Align align;
    switch (alignment) {
    case kStrokeAlignCenter: align = Align::Center; break;
    case kStrokeAlignOutside: align = Align::Outside; break;
    case kStrokeAlignInside: align = Align::Inside; break;
    default: return false;
    }
    ClassID stroker = kInvalidClass;
    UID strokerUID = kInvalidUID;
    if (attrs->GetLineImplementation(stroker, strokerUID, style) != kSuccess) return false;
    const bool solid = stroker == kSolidPathStrokerBoss;
    bool16 evenOdd = kFalse;
    if (attrs->GetEvenOdd(evenOdd, style) == kSuccess && evenOdd) return false;   // filled even-odd
    InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
    if (xp == nil) return false;
    if (xp->ItemHasTransparency(shape) != 0 && (!coverage.transparency || StrokeOrFillTransparency(style, coverage, ::GetUID(item)))) return false;
    if (!Invertible(item)) return false;
    InterfacePtr<IHierarchy> hierarchy(item, UseDefaultIID());
    const bool refill = hierarchy == nil || hierarchy->GetChildCount() == 0;   // no content drawn yet beneath the stroke
    InterfacePtr<IRenderingObject> paint(db, strokePaint, UseDefaultIID());
    if (paint == nil) return false;
    // Overprint is set explicitly: IRenderingObject::Install does not carry it, and the PDF
    // port wrote the plugin's strokes and fills without it (measured 2026-10-04).
    bool16 overprint = kFalse, overprintFill = kFalse;
    if (attrs->GetOverprintStroke(overprint, style) != kSuccess ||
        attrs->GetOverprintFill(overprintFill, style) != kSuccess) return false;

    if (coverage.probeNative) return StrokeNatively(port, pathItem, style, paint, {}, weight, join, limit, cap, overprint);

    // Which strokes this route draws.
    if (anyOpen) return false;
    if (align == Align::Center && join != kPMMiterJoin) return false;      // nothing to correct
    if (align == Align::Inside && (!refill || CornerEffects(attrs, style))) return false;

    ise_miter::Recorder recorder;
    Recording writer(&recorder);
    pathItem->GetOutlineInfo(&writer);
    std::vector<Contour> contours = recorder.Contours();
    if (contours.empty()) return false;
    for (Contour& c : contours) c.closed = true;
    // With corner effects, the anchors of the path the user drew (inner coordinates, as the
    // outline): corners off them are the effect's.
    std::vector<Pt> anchors;
    const bool effects = CornerEffects(attrs, style);
    if (effects)
        for (int32 i = 0; i < paths; ++i)
            for (int32 j = 0, n = geometry->GetNumPoints(i); j < n; ++j) {
                const PMPoint& a = geometry->GetNthPoint(i, j).GetAnchorPoint();
                anchors.push_back({ToDouble(a.X()), ToDouble(a.Y())});
            }
    if (align == Align::Inside) {
        for (const Contour& c : contours)
            for (const ise_miter::Segment& sg : c.segments)
                if (sg.curve) return false;   // straight segments only (see the file comment)
    }

    const bool center = align == Align::Center, outside = align == Align::Outside, inside = align == Align::Inside;
    const double h = center ? weight / 2 : weight;   // half the drawn width
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
    std::shared_ptr<const Prepared> prepared = Cache().Find(key.Value());
    if (!prepared) {
        std::shared_ptr<Prepared> made = std::make_shared<Prepared>();
        Prepare(contours, effects ? &anchors : nullptr, align, join, solid, h, limit, made.get());
        Cache().Keep(key.Value(), made, Segments(contours) + Segments(made->stroked) + Segments(made->outsideClip) +
                                        Segments(made->strokeClip) + made->stubs.size());
        prepared = made;
    }
    if (!prepared->ok) return false;
    const std::vector<Contour>& path = prepared->resolved ? prepared->stroked : contours;
    const std::vector<Stub>& stubs = prepared->stubs;

    if (!solid) {
        // InDesign's own stroke, clipped to where a stroke of this reach can paint: escaping
        // tips and stroke inside the letters are cut away, the pattern is InDesign's.
        if (!center && refill) Refill(port, db, style, none, contours, overprintFill);
        return StrokeNatively(port, pathItem, style, paint, prepared->strokeClip, weight, join, limit, cap, overprint);
    }

    // Outside and Inside: InDesign fills the offset path, not the outline (see the file comment),
    // so the outline is filled again first; the stroke covers the offset fill's band.
    if (!center && refill) Refill(port, db, style, none, contours, overprintFill);

    AutoGSave save(port);
    paint->Install(port, style, IRenderingObject::kInstallStroke);
    if (overprint) port->setoverprint(kTrue);   // the stroke and its joins
    port->setlinewidth(2 * h);
    port->setlinecap(cap);
    port->setmiterlimit(limit);
    if (outside || inside) {
        // Outside: clipped to outside the fill, beyond every join. Inside: to the fill, a
        // nonzero clip of the outline, which reads it exactly also where contours overlap.
        // Not IPathPageItem::Clip: InDesign's PDF port wrote that clip without its path (`W n`).
        port->newpath();
        EmitContours(port, outside ? prepared->outsideClip : contours);
        port->clip();
    }
    port->setlinejoin(join == kPMRoundJoin ? kPMRoundJoin : kPMBevelJoin);
    port->newpath();
    EmitContours(port, path);
    port->stroke();
    FillPolygons(port, StrokeSideJoins(stubs, prepared->crossing, prepared->effect, align, join, h, limit));
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
