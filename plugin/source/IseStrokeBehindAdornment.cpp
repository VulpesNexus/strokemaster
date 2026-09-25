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
//   IseStrokeRestoreAdornment Foreground pass. Restores each recorded run; the stroke
//                             adornment restores any run left muted before deciding.
// Native text then draws the muted runs as fill only, in the Text pass, above every stroke
// of the frame. Center strokes and everything else are left to native drawing. The
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
#include "IWaxGlyphs.h"
#include "IWaxRenderData.h"
#include "IWaxRun.h"

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

#include <cmath>

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
    explicit TransformedOutline(IGraphicsPort* port) : fPort(port), fX(0), fY(0) {}
    void SetMatrix(const PMMatrix& m) { fMatrix = m; }
    void SetOffset(const PMReal& x, const PMReal& y) { fX = x; fY = y; }
    virtual void BeginPath() {}
    virtual void EndPath() {}
    virtual void moveto(const PMReal& x, const PMReal& y) { PMReal a = x, b = y; Place(&a, &b); fPort->moveto(a, b); }
    virtual void lineto(const PMReal& x, const PMReal& y) { PMReal a = x, b = y; Place(&a, &b); fPort->lineto(a, b); }
    virtual void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2,
                         const PMReal& x3, const PMReal& y3)
    {
        PMReal ax = x1, ay = y1, bx = x2, by = y2, cx = x3, cy = y3;
        Place(&ax, &ay); Place(&bx, &by); Place(&cx, &cy);
        fPort->curveto(ax, ay, bx, by, cx, cy);
    }
    virtual void closepath() { fPort->closepath(); }
private:
    void Place(PMReal* x, PMReal* y) const { fMatrix.Transform(x, y); *x = *x + fX; *y = *y + fY; }
    IGraphicsPort* fPort;
    PMMatrix fMatrix;
    PMReal fX, fY;
};

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
    virtual bool16 GetIsActive(const IParcelShape* parcel, const ITextOptions*, int32) { return parcel != nil; }
    virtual bool16 GetCheckCouldDraw() { return kFalse; }
    virtual bool16 GetCouldDraw(const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) { return kTrue; }
    /** The moved stroke occupies the ink the run's native Outside stroke already claims. */
    virtual bool16 GetHasInkBounds() { return kFalse; }
    virtual void GetInkBounds(PMRect*, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) {}
    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void Draw(GraphicsData* gd, int32 flags, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs);
};

CREATE_PMINTERFACE(IseStrokeBehindAdornment, kIseStrokeBehindAdornmentImpl)

void IseStrokeBehindAdornment::Draw(GraphicsData* gd, int32, const IWaxRun* run, const IWaxRenderData* data,
                                    const IWaxGlyphs* glyphs)
{
    if (gd == nil || run == nil || data == nil || glyphs == nil) return;
    IDataBase* db = run->GetDataBase();
    if (db == nil) return;
    Recover(run, data, glyphs, db);
    if (!Eligible(AlignmentOf(data), ToDouble(data->GetOutlineWidth()), MethodOf(data, db))) return;
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
        stroke->Install(port, mutableData, IRenderingObject::kInstallStroke);
        // Outside alignment is drawn by InDesign as a centered stroke of twice the weight
        // beneath the fill; reproduce that geometry explicitly.
        port->setlinewidth(mutableData->GetOutlineWidth() * 2);
        port->setlinejoin(mutableData->GetOutlineJoin());
        port->setmiterlimit(mutableData->GetMiterLimit());
        TransformedOutline writer(port);
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
    virtual bool16 GetIsActive(const IParcelShape* parcel, const ITextOptions*, int32) { return parcel != nil; }
    virtual bool16 GetCheckCouldDraw() { return kFalse; }
    virtual bool16 GetCouldDraw(const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) { return kTrue; }
    virtual bool16 GetHasInkBounds() { return kFalse; }
    virtual void GetInkBounds(PMRect*, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) {}
    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void Draw(GraphicsData*, int32, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs)
    {
        if (run == nil || data == nil || glyphs == nil) return;
        IDataBase* db = run->GetDataBase();
        if (db == nil) return;
        Recover(run, data, glyphs, db);
    }
};

CREATE_PMINTERFACE(IseStrokeRestoreAdornment, kIseStrokeRestoreAdornmentImpl)
