//========================================================================================
//
//  Repaints eligible stroked text fills after a frame's text pass. This prototype does not
//  repaint unstroked victim runs and does not replace native stroke generation.
//
//  Why this works (docs/STROKE_MECHANISMS.md, M1 to M4): for Outside alignment InDesign paints,
//  per wax run, a centered stroke of twice the weight and then that same run's fill. Runs are
//  visited in story order, so a later run's stroke lands on an earlier run's fill: the next
//  line's, or the next formatting run's. Draw passes, however, are frame-wide: for each pass
//  used by the frame's active adornments, every wax run in the frame is visited (SDK,
//  TextDrawPriority.h, lines 36 to 42). An adornment in the foreground pass therefore runs
//  after all of the frame's native text, and repainting each stroked run's fill there puts
//  eligible fills above the frame's native strokes. Other fill runs are not included.
//
//  Deliberate limits of this prototype:
//    - Only runs that paint both a fill and a stroke, with the stroke aligned Outside, are
//      repainted. A Center stroke is painted over its own fill by definition, so it is left
//      alone.
//    - The fill is installed from the run's own rendering object, so color, tint, gradient
//      and overprint state are requested from InDesign. This does not prove identical
//      compositing: antialiased edges and overlapping differently colored runs need tests.
//    - Fills are drawn from glyph outlines rather than by showing text again, so exported PDF
//      does not gain a second copy of searchable text.
//    - Frames are independent. Stacking order between frames is InDesign's and is untouched.
//
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

// Standard includes, for the diagnostic trace below:
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Project includes:
#include "IseID.h"

/** Appends one diagnostic line to a trace file.

    A plugin that loads but draws nothing cannot be diagnosed from its output, because every
    gate that could reject a run leaves the same result: no change at all. Each gate therefore
    records both that it was reached and the values it decided on.

    The directory is read from the environment at run time, so no path belonging to one machine
    is written into this source. The line count is capped because InDesign calls these methods
    once per wax run per pass. The counter lives as long as the process, not as long as one
    export, so a session that exports twice records only the first export: delete the file and
    restart InDesign to trace a later one.
*/
static void IseTrace(const char* fmt, ...)
{
    static int32 lines = 0;
    if (lines >= 4000) {
        return;
    }
    ++lines;

    // Several variables are tried, so that one of them being unset cannot look like silence.
    const char* dir = getenv("TEMP");
    if (dir == nil) {
        dir = getenv("TMP");
    }
    if (dir == nil) {
        dir = getenv("USERPROFILE");
    }
    if (dir == nil) {
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s\\ise-trace.log", dir);
    FILE* f = fopen(path, "a");
    if (f == nil) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

/** Writes a glyph outline into a graphics port, through a matrix and an offset.

    The obvious way to honor a glyph's transform is the SDK's own idiom: concatenate the matrix
    into the port, emit the outline through GPortOutlineInfo, and restore. That cannot be used
    here. This adornment accumulates every glyph of a run into a single path and fills once, so
    that overlapping glyphs cannot composite twice along their shared edges, and the current
    path belongs to the graphics state: restoring after each glyph would discard the segments
    just emitted for it. Transforming the coordinates instead leaves the port's state alone.

    The offset is applied after the matrix, not before it. It carries the run's position within
    its wax line, which is a plain translation of the finished glyph, whereas the matrix
    describes the glyph within its run. Folding the offset in first would let a glyph matrix
    scale or skew the run's position.
*/
class IseTransformedOutline : public IOutlineInfo
{
public:
    IseTransformedOutline(IGraphicsPort* gPort) : fPort(gPort), fOffsetX(0), fOffsetY(0) {}

    /** The transform applied to every coordinate until it is changed again. */
    void SetMatrix(const PMMatrix& matrix) { fMatrix = matrix; }

    /** The translation added after the transform; the run's position on its line. */
    void SetOffset(const PMReal& x, const PMReal& y) { fOffsetX = x; fOffsetY = y; }

    virtual void BeginPath() {}
    virtual void EndPath() {}

    virtual void moveto(const PMReal& x, const PMReal& y)
    {
        PMReal tx = x, ty = y;
        Place(&tx, &ty);
        fPort->moveto(tx, ty);
    }

    virtual void lineto(const PMReal& x, const PMReal& y)
    {
        PMReal tx = x, ty = y;
        Place(&tx, &ty);
        fPort->lineto(tx, ty);
    }

    virtual void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2, const PMReal& x3, const PMReal& y3)
    {
        PMReal ax = x1, ay = y1, bx = x2, by = y2, cx = x3, cy = y3;
        Place(&ax, &ay);
        Place(&bx, &by);
        Place(&cx, &cy);
        fPort->curveto(ax, ay, bx, by, cx, cy);
    }

    virtual void closepath() { fPort->closepath(); }

private:
    void Place(PMReal* x, PMReal* y) const
    {
        fMatrix.Transform(x, y);
        *x = *x + fOffsetX;
        *y = *y + fOffsetY;
    }

    IGraphicsPort* fPort;
    PMMatrix fMatrix;
    PMReal fOffsetX;
    PMReal fOffsetY;
};

/** Writes one line as the library loads.

    This is the control for the trace itself. Static initializers run when the DLL is mapped,
    and the DLL is known to be mapped, so this line must appear. If it appears and the boss's
    constructor line does not, then the class description table is registering nothing and the
    fault is in the resources rather than in the drawing code. If neither appears, the trace
    mechanism is at fault and proves nothing either way.
*/
static struct IseLoadTrace
{
    IseLoadTrace() { IseTrace("library loaded"); }
} gIseLoadTrace;

/** Draws, in the foreground pass, the fill of every wax run that carries an Outside stroke.
    @see IGlobalTextAdornment
*/
class IseFillRepaintAdornment : public CPMUnknown<IGlobalTextAdornment>
{
public:
    IseFillRepaintAdornment(IPMUnknown* boss) : CPMUnknown<IGlobalTextAdornment>(boss)
    {
        // Reaching this line proves the class description table registered the boss and that
        // InDesign instantiated it.
        IseTrace("ctor: boss instantiated");
    }

    /** After the text pass, and after the adornments that decorate text, so fills land on top. */
    virtual Text::DrawPriority GetDrawPriority()
    {
        return Text::DrawPriority(Text::kTAPassPriForeground + 0.90);
    }

    virtual bool16 GetCheckIsActive() { return kTrue; }
    virtual bool16 GetIsActive(const IParcelShape* parcelShape, const ITextOptions* textOptions, int32 iShapeFlags);

    /** Asked per run before Draw, so frames without stroked text cost nothing. */
    virtual bool16 GetCheckCouldDraw() { return kTrue; }
    virtual bool16 GetCouldDraw(const IWaxRun* waxRun, const IWaxRenderData* renderData, const IWaxGlyphs* waxGlyphs);

    /** The repainted fill lies inside the ink the run already claims. */
    virtual bool16 GetHasInkBounds() { return kFalse; }
    virtual void GetInkBounds(PMRect*, const IWaxRun*, const IWaxRenderData*, const IWaxGlyphs*) {}

    virtual void Draw(GraphicsData* gd, int32 iShapeFlags, const IWaxRun* waxRun, const IWaxRenderData* renderData, const IWaxGlyphs* waxGlyphs);

    virtual void StartOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}
    virtual void EndOfParcelDraw(GraphicsData*, int32, const IParcelShape*) {}

private:
    /** The database the wax run lives in, or nil. */
    static IDataBase* GetRunDataBase(const IWaxRun* waxRun);

    /** True when this run paints an Outside stroke under its fill, which is the case this
        adornment corrects. */
    static bool16 WantsRepaint(const IWaxRun* waxRun, const IWaxRenderData* renderData);
};

CREATE_PMINTERFACE(IseFillRepaintAdornment, kIseFillRepaintAdornmentImpl)

IDataBase* IseFillRepaintAdornment::GetRunDataBase(const IWaxRun* waxRun)
{
    // IWaxRun hands out its database directly (IWaxRun.h, line 168). Asking a wax run for
    // IPMPersist instead returns nil, because the wax is not a persistent UID-based object,
    // and so does the free GetDataBase helper, which is built on the same persistence query.
    return waxRun == nil ? nil : waxRun->GetDataBase();
}

bool16 IseFillRepaintAdornment::WantsRepaint(const IWaxRun* waxRun, const IWaxRenderData* renderData)
{
    if (renderData == nil) {
        return kFalse;
    }

    IDataBase* db = IseFillRepaintAdornment::GetRunDataBase(waxRun);

    // Center-aligned strokes are painted over their own fill by definition; leave them alone.
    if (renderData->GetStrokeAlignment() != kStrokeAlignOutside) {
        return kFalse;
    }
    if (renderData->GetOutlineWidth() <= 0) {
        return kFalse;
    }
    // Prototype restriction: repaint only runs carrying both paints. An unstroked victim
    // can also suffer overlap and is currently missed (the takeover V0 regression).
    // Without a database the render method cannot be resolved; use the checks above.
    if (db != nil && renderData->GetRenderMethod(db) != IWaxRenderData::kFillStroke) {
        return kFalse;
    }
    return kTrue;
}

bool16 IseFillRepaintAdornment::GetIsActive(const IParcelShape* parcelShape, const ITextOptions* /*textOptions*/, int32 /*iShapeFlags*/)
{
    // Active for every parcel; per-run filtering happens in GetCouldDraw. Screen, print and
    // export all reach this adornment, which is what makes the correction consistent.
    return parcelShape != nil;
}

bool16 IseFillRepaintAdornment::GetCouldDraw(const IWaxRun* waxRun, const IWaxRenderData* renderData, const IWaxGlyphs* waxGlyphs)
{
    return waxGlyphs != nil && IseFillRepaintAdornment::WantsRepaint(waxRun, renderData);
}

void IseFillRepaintAdornment::Draw(GraphicsData* gd, int32 /*iShapeFlags*/, const IWaxRun* waxRun, const IWaxRenderData* renderData, const IWaxGlyphs* waxGlyphs)
{
    if (gd == nil || waxGlyphs == nil || !IseFillRepaintAdornment::WantsRepaint(waxRun, renderData)) {
        return;
    }
    IGraphicsPort* gPort = gd->GetGraphicsPort();
    if (gPort == nil) {
        return;
    }
    const int32 glyphCount = waxGlyphs->GetGlyphCount();
    const Text::GlyphID* glyphs = waxGlyphs->PeekGlyphArray();
    if (glyphCount <= 0 || glyphs == nil) {
        return;
    }

    IWaxRenderData* data = const_cast<IWaxRenderData*>(renderData);
    InterfacePtr<IPMFont> font(data->QueryFont());
    if (font == nil) {
        return;
    }
    InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
    if (fontMgr == nil) {
        return;
    }
    InterfacePtr<IFontInstance> fontInstance(fontMgr->QueryFontInstance(font, data->GetFontMatrix()));
    if (fontInstance == nil) {
        return;
    }

    IDataBase* db = IseFillRepaintAdornment::GetRunDataBase(waxRun);
    if (db == nil) {
        return;
    }
    InterfacePtr<IRenderingObject> fillColor(db, data->GetColorUID(kFalse), UseDefaultIID());
    if (fillColor == nil) {
        return;
    }

    AutoGSave autoGSave(gPort);
    // Install the run's own fill, so color, tint, gradient and overprint match the fill
    // InDesign already painted and the repaint is invisible.
    fillColor->Install(gPort, data, IRenderingObject::kInstallFill);

    // Collect the run's glyph outlines at their drawn positions into one path, so overlapping
    // glyphs cannot double-composite along their shared edges.
    //
    // Two positions have to be combined, and missing the second is what made this adornment
    // repaint only the first run of every line. IWaxGlyphs places glyphs relative to the start
    // of their own run (IWaxGlyphs.h, lines 39 to 42), so every run's first glyph reports an
    // x of zero. IWaxRun places the run relative to its wax line (IWaxRun.h, lines 98 to 114).
    // Measured on the fixture: of 200 runs, 145 sat at a non-zero position on their line and
    // every one of them reported its first glyph at zero.
    //
    // Where a run carries glyph matrices, GetGlyphMatrix describes the glyph completely within
    // that run, incorporating scale, skew and rotation (IWaxGlyphs.h, lines 285 to 289). Most
    // roman text has none, so the plain draw position is the ordinary path.
    const bool16 hasGlyphMatrix = waxGlyphs->HasGlyphMatrix();
    IseTransformedOutline writer(gPort);
    writer.SetOffset(waxRun->GetXPosition(), waxRun->GetYPosition());
    int32 transformed = 0;

    gPort->newpath();
    for (int32 i = 0; i < glyphCount; ++i) {
        if (hasGlyphMatrix) {
            PMMatrix glyphMatrix;
            PMPoint penCorrection(0, 0);
            if (waxGlyphs->GetGlyphMatrix(i, &glyphMatrix, &penCorrection)) {
                writer.SetMatrix(glyphMatrix);
                fontInstance->GetOutlineInfo(&writer, glyphs[i], penCorrection.X(), penCorrection.Y());
                ++transformed;
                continue;
            }
        }
        const PMMatrix where = waxGlyphs->GetGlyphDrawPosition(i);
        writer.SetMatrix(PMMatrix());
        fontInstance->GetOutlineInfo(&writer, glyphs[i], where.GetXTranslation(), where.GetYTranslation());
    }
    gPort->fill();
    IseTrace("Draw: filled %d glyphs, %d through a glyph matrix, runX=%.2f runY=%.2f",
             glyphCount, transformed,
             ToDouble(waxRun->GetXPosition()), ToDouble(waxRun->GetYPosition()));
}
