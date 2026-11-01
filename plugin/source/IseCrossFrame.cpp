//========================================================================================
// Route X: route W's correction across the frames of one story. EXPERIMENTAL.
// The rule and its bookkeeping are in IseCrossFrameCore.h; this file is the host side:
//
//   - a draw event handler (a kDrawEventService provider) that brackets each traversal
//     (spread draw or partial draw) and reports content drawn during it: every page item's
//     shape, and frame fills and strokes;
//   - the parcel a text pass is drawing, resolved to its frame, story, and transparency
//     state when route W's adornments are asked about it;
//   - fill recording, fill confirmation, and the even-odd clips that withhold a stroke from
//     fills of the same story the traversal has already painted.
//
// The research build (ISE_ROUTE_X_RESEARCH) also reads a control file and writes a trace,
// both named by environment variables of the InDesign process and absent otherwise:
//   ISE_XF_CONTROL  lines of key=value: mode=native|routew|story|overlap, trace=0|1,
//                   suppress_shape=<uid>, suppress_text=<uid>, defer_probe=0|1,
//                   miter=0|1 (route W's miter correction; on by default),
//                   screen=0|1 (screen snapshots and overlap invalidation; on by default),
//                   shapes=0|1 (page-item strokes, IsePageItemStroke.cpp; on by default),
//                   xp=0|1 (page-item strokes on items with object-level transparency; on by default),
//                   probe=native (research: claim each stroked page item's event and draw
//                   InDesign's own stroke from it, unclipped; off by default),
//                   word=0|1 (patterns along one outline per word, IsePageItemStroke.cpp's
//                   PrepareWord; on by default), overlaps, inside, whole_points,
//                   pattern_points, glyph_tips, effect_corners, open_paths, shape_miter=0|1 (the
//                   page-item switches of IseSettings.h; on by default)
//   ISE_XF_TRACE    the trace file, appended to
// Without the control file, the research build follows the StrokeMaster switches as the
// production build does: the file the StrokeMaster panel writes (IseSettings.h), read again at
// the start of each spread drawn when it has changed. With every switch at its default, the
// mode is Story.
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "IDataBase.h"
#include "IBoolData.h"
#include "ICallback.h"
#include "IFrameList.h"
#include "ILayoutUIUtils.h"
#include "IViewPortAttributes.h"
#include "IPDFLibraryUtilsPublic.h"
#include "IWaxIterator.h"
#include "IWaxStrand.h"
#include "IDrwEvtDispatcher.h"
#include "IDrwEvtHandler.h"
#include "IFontInstance.h"
#include "IFontMgr.h"
#include "IGeometry.h"
#include "IGraphicAttributeUtils.h"
#include "IGraphicStyleDescriptor.h"
#include "IGraphicsPort.h"
#include "IHierarchy.h"
#include "IPathGeometry.h"
#include "IOutlineInfo.h"
#include "IPMFont.h"
#include "IParcel.h"
#include "IParcelList.h"
#include "IParcelShape.h"
#include "IShape.h"
#include "ISwatchUtils.h"
#include "ITextFrameColumn.h"
#include "IWaxGlyphs.h"
#include "IWaxLine.h"
#include "IWaxRenderData.h"
#include "IWaxRun.h"
#include "IXPManager.h"
#include "IXPUtils.h"
#ifdef ISE_ROUTE_X_RESEARCH
#include "IObjectModel.h"
#endif

// General includes:
#include "AutoGSave.h"
#include "CPMUnknown.h"
#include "CServiceProvider.h"
#include "CObserver.h"
#include "CIdleTask.h"
#include <unordered_map>
#include "IChangeManager.h"
#include "CResponder.h"
#include "ISubject.h"
#include "IDocument.h"
#include "IDocumentSignalData.h"
#include "ISignalMgr.h"
#include "DocumentID.h"
#include "GenericID.h"
#include "GraphicStylesID.h"
#include "XPID.h"
#include "TextID.h"
#include "LayerID.h"
#include "DocumentContextID.h"
#include "GraphicsData.h"
#include "GraphicsID.h"
#include "PMMatrix.h"
#include "PersistUtils.h"
#include "TransformTypes.h"
#include "TransformUtils.h"
#include "Utils.h"

// Project includes:
#include "IseID.h"
#include "IseCrossFrame.h"
#include "IsePageItemStroke.h"
#include "IseSettings.h"
#include "IseMiterCore.h"
#include "IseRegionCore.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

namespace ise_cross_frame {
namespace {

// ---------------------------------------------------------------- control and trace

struct Control {
    Mode mode = Mode::RouteW;
    bool trace = false;
    std::uint32_t suppressShape = 0, suppressText = 0;
    bool deferProbe = false;
    bool screen = true;   // Research builds can disable the screen path to compare the old behavior.
    bool miter = true;   // route W's miter correction (IseMiterCore.h)
    bool textTips = true;   // ... with glyph tips trimmed on live text too (the owner, 2026-10-31)
    bool closeHoles = false;   // route W fills the holes a run's stroke encloses (the owner, 2026-10-31)
    bool shapes = true;  // page-item strokes (IsePageItemStroke.cpp)
    bool shapesAlone = false;   // page-item strokes also when text is left to InDesign (the switches)
    ise_page_stroke::Coverage coverage;   // what page-item strokes may take over
};

/** The control the StrokeMaster switches ask for (IseSettings.h). */
Control FromSettings(const ise_settings::Settings& s)
{
    Control c;
    c.mode = !s.enabled || !s.textBehind ? Mode::Native : !s.textStory ? Mode::RouteW : s.textStories ? Mode::Overlap : Mode::Story;
    c.miter = s.enabled && s.textBehind && s.textMiter;
    c.textTips = s.enabled && s.glyphTips;
    c.closeHoles = s.enabled && s.textBehind && s.closeHoles;
    c.shapes = s.enabled && s.shapes;
    c.shapesAlone = true;
    c.coverage.transparency = s.transparency;
    c.coverage.wordOutline = s.wordOutline;
    c.coverage.wholePoints = s.wholePoints;
    c.coverage.shapeMiter = s.shapeMiter;
    c.coverage.glyphTips = s.glyphTips;
    c.coverage.effectCorners = s.effectCorners;
    c.coverage.patternPoints = s.patternPoints;
    c.coverage.overlaps = s.overlaps;
    c.coverage.inside = s.inside;
    c.coverage.openPaths = s.openPaths;
    // Close holes on shapes and outlined text too (the owner's retest notes, 2026-11-01).
    c.coverage.closeHoles = s.enabled && s.closeHoles;
    return c;
}

/** Reads the switches into `c` when their file has changed since `stamp` (or has gone). */
void ReloadSettings(Control& c, FILETIME& stamp, bool& seen)
{
    FILETIME now = {0, 0};
    const bool exists = ise_settings::Stamp(&now);
    if (seen && (exists ? CompareFileTime(&now, &stamp) == 0 : stamp.dwLowDateTime == 0 && stamp.dwHighDateTime == 0)) return;
    ise_settings::Settings s;
    // A file that exists but cannot be read now (held by another program) keeps the switches in
    // force and is read again at the next draw; it gave the defaults until it changed again (the
    // second review of 2026-11-01).
    if (!ise_settings::Load(&s) && exists && seen) return;
    c = FromSettings(s);
    stamp = exists ? now : FILETIME{0, 0};
    seen = true;
}

#ifdef ISE_ROUTE_X_RESEARCH
// Each drawing thread reads the control file for itself; the trace file is shared, guarded.
Control& Ctl() { thread_local Control c; return c; }

thread_local FILETIME gControlStamp = {0, 0};
thread_local bool gControlSeen = false;
FILE* gTrace = nullptr;
std::mutex gTraceLock;

thread_local FILETIME gSettingsStamp = {0, 0};
thread_local bool gSettingsSeen = false;

void ReloadControl()
{
    const wchar_t* path = _wgetenv(L"ISE_XF_CONTROL");
    if (path == nullptr || *path == 0) { ReloadSettings(Ctl(), gSettingsStamp, gSettingsSeen); return; }
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attrs)) return;
    if (gControlSeen && CompareFileTime(&attrs.ftLastWriteTime, &gControlStamp) == 0) return;
    FILE* f = _wfopen(path, L"rb");
    if (f == nullptr) return;
    Control c;
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        char key[64] = {0}, value[128] = {0};
        if (std::sscanf(line, " %63[^= ] = %127s", key, value) != 2) continue;
        const std::string k(key), v(value);
        if (k == "mode") {
            c.mode = v == "native" ? Mode::Native : v == "story" ? Mode::Story : v == "overlap" ? Mode::Overlap : Mode::RouteW;
        } else if (k == "trace") {
            c.trace = v == "1";
        } else if (k == "suppress_shape") {
            c.suppressShape = static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10));
        } else if (k == "suppress_text") {
            c.suppressText = static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10));
        } else if (k == "defer_probe") {
            c.deferProbe = v == "1";
        } else if (k == "screen") {
            c.screen = v != "0";
        } else if (k == "miter") {
            c.miter = v != "0";
        } else if (k == "shapes") {
            c.shapes = v != "0";
        } else if (k == "xp") {
            c.coverage.transparency = v != "0";
        } else if (k == "probe") {
            c.coverage.probeNative = v == "native";
        } else if (k == "word") {
            c.coverage.wordOutline = v != "0";
        } else if (k == "overlaps") {
            c.coverage.overlaps = v != "0";
        } else if (k == "inside") {
            c.coverage.inside = v != "0";
        } else if (k == "whole_points") {
            c.coverage.wholePoints = v != "0";
        } else if (k == "shape_miter") {
            c.coverage.shapeMiter = v != "0";
        } else if (k == "pattern_points") {
            c.coverage.patternPoints = v != "0";
        } else if (k == "glyph_tips") {
            c.coverage.glyphTips = v != "0";
            c.textTips = c.coverage.glyphTips;
        } else if (k == "close_holes") {
            c.closeHoles = v == "1";
            c.coverage.closeHoles = c.closeHoles;
        } else if (k == "effect_corners") {
            c.coverage.effectCorners = v != "0";
        } else if (k == "open_paths") {
            c.coverage.openPaths = v != "0";
        }
    }
    std::fclose(f);
    Ctl() = c;
    gControlStamp = attrs.ftLastWriteTime;
    gControlSeen = true;
}

bool Tracing() { return Ctl().trace; }

void Trace(const char* fmt, ...)
{
    if (!Tracing()) return;
    std::lock_guard<std::mutex> hold(gTraceLock);
    if (gTrace == nullptr) {
        const wchar_t* path = _wgetenv(L"ISE_XF_TRACE");
        if (path == nullptr || *path == 0) return;
        gTrace = _wfopen(path, L"ab");
        if (gTrace == nullptr) return;
    }
    std::fprintf(gTrace, "T%lu ", static_cast<unsigned long>(GetCurrentThreadId()));
    va_list args;
    va_start(args, fmt);
    std::vfprintf(gTrace, fmt, args);
    va_end(args);
    std::fputc('\n', gTrace);
}

void FlushTrace() { std::lock_guard<std::mutex> hold(gTraceLock); if (gTrace) std::fflush(gTrace); }

const char* ClassName(IPMUnknown* item)
{
    if (item == nil) return "-";
    InterfacePtr<IObjectModel> om(GetExecutionContextSession(), IID_IOBJECTMODEL);
    const char* name = om ? om->GetIDName(::GetClass(item)) : nullptr;
    return name ? name : "?";
}

const char* PortKind(GraphicsData* gd)
{
    if (gd == nil) return "nogd";
    IViewPortAttributes* vpa = gd->GetViewPortAttributes();
    if (vpa == nil) return "novpa";
    if (Utils<IPDFLibraryUtilsPublic>()->IsPDFExportPort(vpa)) return "pdf";
    if (vpa->GetViewPortIsPrintingPort()) return "print";
    return "screen";
}
#else
// Each drawing thread keeps its own copy of the switches and reads them again when their file
// changes, at the start of each spread it draws.
Control& Ctl()
{
    // The switches as their file has them, from the thread's first use; read again when the file
    // changes, at the start of each spread drawn (ReloadControl).
    thread_local Control c = [] {
        ise_settings::Settings s;
        ise_settings::Load(&s);
        return FromSettings(s);
    }();
    return c;
}
thread_local FILETIME gSettingsStamp = {0, 0};
thread_local bool gSettingsSeen = false;
void ReloadControl() { ReloadSettings(Ctl(), gSettingsStamp, gSettingsSeen); }
bool Tracing() { return false; }
void Trace(const char*, ...) {}
void FlushTrace() {}
#endif

ise_page_stroke::Coverage PageStrokeCoverage()
{
    ise_page_stroke::Coverage c = Ctl().coverage;
    if (Tracing()) c.trace = &Trace;
    return c;
}

// ---------------------------------------------------------------- the parcel being drawn

struct ParcelInfo {
    const void* shape = nullptr;
    bool ok = false;
    ParcelKey key;
    std::uint32_t frame = 0, story = 0;
    int32 parcels = 0;
    bool transparent = false;
    std::vector<std::uint32_t> own;   // the frame column and every ancestor
};

thread_local ParcelInfo gParcel;

void ResetScreen();

std::uint32_t KeyValue(const ParcelKey& key)
{
    static_assert(sizeof(ParcelKey) == sizeof(std::uint32_t), "ParcelKey is one 32-bit value");
    std::uint32_t v = 0;
    std::memcpy(&v, &key, sizeof v);
    return v;
}

bool SameParcel(const IWaxRun* run)
{
    if (!gParcel.ok || run == nil) return false;
    const IWaxLine* line = run->GetWaxLine();
    return line != nil && line->GetParcelKey() == gParcel.key;
}

// ---------------------------------------------------------------- glyph outlines

// Writes a run's glyph outlines as they are drawn: glyph matrix, then the run's offset in its
// wax line (route W's TransformedOutline), then an optional line-to-pasteboard matrix.
class OutlineCollector : public IOutlineInfo
{
public:
    OutlineCollector(const PMMatrix* toPasteboard) : fToPb(toPasteboard), fX(0), fY(0) {}
    void SetMatrix(const PMMatrix& m) { fMatrix = m; }
    void SetOffset(const PMReal& x, const PMReal& y) { fX = x; fY = y; }
    void StartGlyph() { fGlyphs.emplace_back(); }
    virtual void BeginPath() {}
    virtual void EndPath() {}
    virtual void moveto(const PMReal& x, const PMReal& y) { Emit(Seg::Move, &x, &y, 1); }
    virtual void lineto(const PMReal& x, const PMReal& y) { Emit(Seg::Line, &x, &y, 1); }
    virtual void curveto(const PMReal& x1, const PMReal& y1, const PMReal& x2, const PMReal& y2, const PMReal& x3, const PMReal& y3)
    {
        const PMReal xs[3] = {x1, x2, x3}, ys[3] = {y1, y2, y3};
        Emit(Seg::Curve, xs, ys, 3);
    }
    virtual void closepath() { if (!fGlyphs.empty()) { Seg s; s.op = Seg::Close; std::memset(s.p, 0, sizeof s.p); fGlyphs.back().segs.push_back(s); } }
    std::vector<Glyph>& Glyphs() { return fGlyphs; }
    Box All() const { Box b; for (const auto& g : fGlyphs) b.Add(g.box); return b; }
private:
    void Emit(Seg::Op op, const PMReal* xs, const PMReal* ys, int n)
    {
        if (fGlyphs.empty()) fGlyphs.emplace_back();
        Seg s; s.op = op; std::memset(s.p, 0, sizeof s.p);
        for (int i = 0; i < n; ++i) {
            PMReal x = xs[i], y = ys[i];
            fMatrix.Transform(&x, &y);
            x = x + fX; y = y + fY;
            if (fToPb) fToPb->Transform(&x, &y);
            s.p[2 * i] = ToDouble(x); s.p[2 * i + 1] = ToDouble(y);
            fGlyphs.back().box.Add(s.p[2 * i], s.p[2 * i + 1]);
        }
        fGlyphs.back().segs.push_back(s);
    }
    const PMMatrix* fToPb;
    PMMatrix fMatrix;
    PMReal fX, fY;
    std::vector<Glyph> fGlyphs;
};

bool CollectOutlines(const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, OutlineCollector* out)
{
    const int32 count = glyphs->GetGlyphCount();
    const Text::GlyphID* ids = glyphs->PeekGlyphArray();
    if (count <= 0 || ids == nil) return false;
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    InterfacePtr<IPMFont> font(mutableData->QueryFont());
    InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
    if (font == nil || fontMgr == nil) return false;
    InterfacePtr<IFontInstance> instance(fontMgr->QueryFontInstance(font, mutableData->GetFontMatrix()));
    if (instance == nil) return false;
    out->SetOffset(run->GetXPosition(), run->GetYPosition());
    const bool16 hasMatrix = glyphs->HasGlyphMatrix();
    for (int32 i = 0; i < count; ++i) {
        out->StartGlyph();
        if (hasMatrix) {
            PMMatrix m;
            PMPoint pen(0, 0);
            if (glyphs->GetGlyphMatrix(i, &m, &pen)) {
                out->SetMatrix(m);
                instance->GetOutlineInfo(out, ids[i], pen.X(), pen.Y());
                continue;
            }
        }
        const PMMatrix where = glyphs->GetGlyphDrawPosition(i);
        out->SetMatrix(PMMatrix());
        instance->GetOutlineInfo(out, ids[i], where.GetXTranslation(), where.GetYTranslation());
    }
    auto& g = out->Glyphs();
    g.erase(std::remove_if(g.begin(), g.end(), [](const Glyph& x) { return x.segs.empty(); }), g.end());
    return !g.empty();
}

// What a recorded fill needs to build its outlines later, on the same drawing thread in the
// same traversal: the font (a counted reference, released when the record is resolved or
// dropped), the font matrix, the glyphs and their draw positions, the run's offset in its
// wax line, and the line's pasteboard matrix. Runs with glyph matrices resolve at once.
struct RunSource {
    InterfacePtr<IPMFont> font;
    PMMatrix fontMatrix;
    std::vector<Text::GlyphID> ids;
    std::vector<PMReal> x, y;
    PMReal runX = 0, runY = 0;
    PMMatrix toPb;
};

bool BuildOutlines(const RunSource& src, OutlineCollector* out)
{
    InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
    if (src.font == nil || fontMgr == nil) return false;
    InterfacePtr<IFontInstance> instance(fontMgr->QueryFontInstance(src.font, src.fontMatrix));
    if (instance == nil) return false;
    out->SetOffset(src.runX, src.runY);
    out->SetMatrix(PMMatrix());
    for (size_t i = 0; i < src.ids.size(); ++i) {
        out->StartGlyph();
        instance->GetOutlineInfo(out, src.ids[i], src.x[i], src.y[i]);
    }
    auto& g = out->Glyphs();
    g.erase(std::remove_if(g.begin(), g.end(), [](const Glyph& x) { return x.segs.empty(); }), g.end());
    return !g.empty();
}

// Builds an unresolved record's glyphs and exact box (the core's resolver).
void Resolve(FillRecord& r)
{
    std::shared_ptr<RunSource> src = std::static_pointer_cast<RunSource>(r.source);
    r.glyphs.clear();
    r.box = Box();
    if (src) {
        OutlineCollector c(&src->toPb);
        if (BuildOutlines(*src, &c)) {
            r.glyphs = std::move(c.Glyphs());
            for (const auto& g : r.glyphs) r.box.Add(g.box);
        }
    }
    r.source.reset();
}

bool CrossFrameMode() { const Mode m = Ctl().mode; return m == Mode::Story || m == Mode::Overlap; }

} // namespace

// ---------------------------------------------------------------- the glue API

Mode CurrentMode() { return Ctl().mode; }

bool RouteWEnabled() { return Ctl().mode != Mode::Native; }

bool MiterCorrectionEnabled() { return Ctl().miter; }

bool TextTipsEnabled() { return Ctl().textTips; }

bool CloseHolesEnabled() { return Ctl().closeHoles; }

void NoteParcel(const IParcelShape* parcelShape, char tag, int32 flags)
{
    if (!CrossFrameMode() && !Tracing()) return;
    if (tag == 'W' || tag == 'w') ResetScreen();
    if (parcelShape == gParcel.shape && gParcel.ok) { Trace("P %c same frame=%u", tag, gParcel.frame); return; }
    ParcelInfo info;
    info.shape = parcelShape;
    InterfacePtr<IParcel> parcel(const_cast<IParcelShape*>(parcelShape), UseDefaultIID());
    if (parcel != nil) {
        info.key = parcel->GetParcelKey();
        info.frame = parcel->GetFrameUID().Get();
        InterfacePtr<IParcelList> list(parcel->QueryParcelList());
        if (list != nil) {
            info.parcels = list->GetParcelCount();
            info.story = list->GetFrameListRef().GetUID().Get();
        }
        InterfacePtr<ITextFrameColumn> column(parcel->QueryFrame());
        IDataBase* db = column ? ::GetDataBase(column) : nil;
        InterfacePtr<IHierarchy> node(column, UseDefaultIID());
        InterfacePtr<IShape> firstShape;
        while (node != nil) {
            info.own.push_back(::GetUID(node).Get());
            if (firstShape == nil) firstShape.reset(InterfacePtr<IShape>(node, UseDefaultIID()).forget());
            node.reset(node->QueryParent());
        }
        if (db != nil && firstShape != nil) {
            InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
            info.transparent = xp != nil && xp->ItemHasTransparencyInParentHierarchy(firstShape) != 0;
        }
        info.ok = info.frame != 0 && info.story != 0;
    }
    gParcel = info;
    Trace("P %c parcel=%p ok=%d frame=%u story=%u key=%u parcels=%d xp=%d depth=%u flags=0x%x",
          tag, parcelShape, info.ok ? 1 : 0, info.frame, info.story, KeyValue(info.key), info.parcels,
          info.transparent ? 1 : 0, static_cast<unsigned>(info.own.size()), static_cast<unsigned>(flags));
}

namespace {

Box FromRect(const PMRect& r)
{
    Box b;
    b.Add(ToDouble(r.Left()), ToDouble(r.Top()));
    b.Add(ToDouble(r.Right()), ToDouble(r.Bottom()));
    return b;
}

// A frame's own fill or stroke paint, as it will be drawn: nothing when the frame has none.
// InDesign sends the frame fill and frame stroke events for every frame, painted or not.
// A fill covers the path's box. A stroke covers its bounding box, except on an unrotated
// rectangle, where it covers only four strips around the edge, one weight either side of the
// path, whatever its alignment: a bordered frame does not cover the text inside it.
std::vector<Box> FramePaintBoxes(IPMUnknown* item, bool stroke)
{
    std::vector<Box> out;
    InterfacePtr<IGraphicStyleDescriptor> style(item, UseDefaultIID());
    InterfacePtr<IGeometry> geometry(item, UseDefaultIID());
    IDataBase* db = item ? ::GetDataBase(item) : nil;
    if (style == nil || geometry == nil || db == nil) return out;
    UID paint = kInvalidUID;
    const ErrorCode got = stroke ? Utils<IGraphicAttributeUtils>()->GetStrokeRenderingUID(paint, style)
                                 : Utils<IGraphicAttributeUtils>()->GetFillRenderingUID(paint, style);
    if (got != kSuccess || paint == kInvalidUID || paint == Utils<ISwatchUtils>()->GetNoneSwatchUID(db)) return out;
    const PMMatrix toPb = ::InnerToPasteboardMatrix(item);
    if (!stroke) { out.push_back(FromRect(geometry->GetPathBoundingBox(toPb))); return out; }
    PMReal weight = 0;
    Utils<IGraphicAttributeUtils>()->GetStrokeWeight(weight, style);
    if (weight <= 0) return out;
    InterfacePtr<IPathGeometry> path(item, UseDefaultIID());
    bool rectangle = path != nil && path->GetNumPaths() == 1 && path->GetNumSegments(0) == 4 &&
                     std::fabs(ToDouble(toPb[1])) < 1e-9 && std::fabs(ToDouble(toPb[2])) < 1e-9;
    for (int32 i = 0; rectangle && i < 4; ++i) {
        PMPathPointList points;
        rectangle = path->GetNthSegment(0, i, points) == kLine;
    }
    if (!rectangle) { out.push_back(FromRect(geometry->GetStrokeBoundingBox(toPb))); return out; }
    const double w = ToDouble(weight) * std::max(std::fabs(ToDouble(toPb[0])), std::fabs(ToDouble(toPb[3])));
    const Box p = FromRect(geometry->GetPathBoundingBox(toPb)), o = p.Grown(w), in = p.Grown(-w);
    Box top, bottom, left, right;
    top.Add(o.x0, o.y0); top.Add(o.x1, in.y0);
    bottom.Add(o.x0, in.y1); bottom.Add(o.x1, o.y1);
    left.Add(o.x0, o.y0); left.Add(in.x0, o.y1);
    right.Add(in.x1, o.y0); right.Add(o.x1, o.y1);
    out = {top, bottom, left, right};
    return out;
}

// A spline that paints nothing of its own when its shape draws: no fill, no stroke, no children
// (a placed graphic or path text is a child), no text column, and no transparency. Its frame
// fill and stroke events already report nothing; counting its geometry as content would stop
// the correction under an invisible rectangle or an empty frame stacked between two frames of
// a story (cert-X2 XE6, measured 2026-10-03). Anything this cannot establish keeps its box.
bool PaintsNothing(IPMUnknown* item)
{
    if (item == nil) return false;
    InterfacePtr<IParcelShape> parcelShape(item, UseDefaultIID());
    if (parcelShape != nil) return false;
    InterfacePtr<IGraphicStyleDescriptor> style(item, UseDefaultIID());
    InterfacePtr<IHierarchy> hierarchy(item, UseDefaultIID());
    InterfacePtr<IShape> shape(item, UseDefaultIID());
    IDataBase* db = ::GetDataBase(item);
    if (style == nil || hierarchy == nil || shape == nil || db == nil) return false;
    if (hierarchy->GetChildCount() != 0) return false;
    InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
    if (xp == nil || xp->ItemHasTransparency(shape) != 0) return false;
    const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    UID fill = kInvalidUID, stroke = kInvalidUID;
    if (Utils<IGraphicAttributeUtils>()->GetFillRenderingUID(fill, style) != kSuccess) return false;
    if (Utils<IGraphicAttributeUtils>()->GetStrokeRenderingUID(stroke, style) != kSuccess) return false;
    const bool fillNone = fill == kInvalidUID || fill == none;
    bool strokeNone = stroke == kInvalidUID || stroke == none;
    if (!strokeNone) {
        PMReal weight = 0;
        Utils<IGraphicAttributeUtils>()->GetStrokeWeight(weight, style);
        strokeNone = weight <= 0;
    }
    return fillNone && strokeNone;
}

Box Placed(const PMRect& r, const PMMatrix& m1, const PMMatrix* m2 = nullptr)
{
    Box b;
    const double xs[2] = {ToDouble(r.Left()), ToDouble(r.Right())}, ys[2] = {ToDouble(r.Top()), ToDouble(r.Bottom())};
    for (double x0 : xs) for (double y0 : ys) {
        PMReal x = x0, y = y0;
        m1.Transform(&x, &y);
        if (m2) m2->Transform(&x, &y);
        b.Add(ToDouble(x), ToDouble(y));
    }
    return b;
}

// Native glyph ink excludes our deliberately generous repaint adornment bounds. Those
// bounds must not turn an unrelated text frame into an intervening painted occluder.
bool NativeTextBox(IPMUnknown* item, Box& box)
{
    InterfacePtr<ITextFrameColumn> column(item, UseDefaultIID());
    InterfacePtr<IFrameList> list(column ? column->QueryFrameList() : nil);
    InterfacePtr<IWaxStrand> strand(list, UseDefaultIID());
    if (column == nil || list == nil || strand == nil || list->GetFrameIndex(::GetUID(column)) < 0) return false;
    const TextIndex start = list->GetFrameStart(::GetUID(column));
    const int32 span = list->GetFrameSpan(::GetUID(column));
    if (start < 0 || span < 0) return false;
    std::unique_ptr<const IWaxIterator> iterator(strand->NewReadOnlyWaxIterator());
    if (!iterator) return false;
    Box ink;
    int lines = 0;
    for (const IWaxLine* line = iterator->GetFirstWaxLine(start); line && line->TextOrigin() < start + span;
         line = iterator->GetNextWaxLine()) {
        if (++lines > 400) return false;
        for (int32 i=0; i<line->GetRunCount(); ++i) {
            InterfacePtr<IWaxRun> run(line->QueryRun(i));
            InterfacePtr<IWaxGlyphs> glyphs(run, UseDefaultIID());
            if (run == nil || glyphs == nil) continue;
            const PMRect bounds = glyphs->GetStrokeBoundingBox();
            const double x = ToDouble(run->GetXPosition()), y = ToDouble(run->GetYPosition());
            const PMRect shifted(ToDouble(bounds.Left())+x, ToDouble(bounds.Top())+y,
                                 ToDouble(bounds.Right())+x, ToDouble(bounds.Bottom())+y);
            ink.Add(Placed(shifted, line->GetToPasteboardMatrix()));
        }
    }
    box.Add(ink);
    return true;
}

// Where a page item that starts drawing can paint, on the pasteboard. The same answer on every
// thread and output: IShape::GetPaintedBBox is not (on the main thread it adds about 9 pt, for
// screen adornments, and 1 pt on a background export thread), so it is used only for items
// with transparency effects, which can paint beyond their geometry.
//   a text column: the ink of its composed text (IParcelShape, parcel coordinates)
//   any item: its path with its stroke
Box ContentBox(IPMUnknown* item)
{
    Box b;
    if (item == nil) return b;
    InterfacePtr<IParcelShape> parcelShape(item, UseDefaultIID());
    InterfacePtr<IParcel> parcel(item, UseDefaultIID());
    // Preserve the original parcel ink contract whenever Close holes adds no adornment.
    if (parcelShape != nil && parcel != nil && (!Ctl().closeHoles || !NativeTextBox(item, b))) {
        PMRect ink;
        parcelShape->GetInkBounds(&ink);
        const PMMatrix toFrame = parcel->GetToFrameMatrix(), toPb = ::InnerToPasteboardMatrix(item);
        b.Add(Placed(ink, toFrame, &toPb));
    }
    InterfacePtr<IGeometry> geometry(item, UseDefaultIID());
    if (geometry != nil) b.Add(FromRect(geometry->GetStrokeBoundingBox(::InnerToPasteboardMatrix(item))));
    InterfacePtr<IShape> shape(item, UseDefaultIID());
    IDataBase* db = ::GetDataBase(item);
    if (shape != nil && db != nil) {
        InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
        if (b.Empty() || (xp != nil && xp->ItemHasTransparency(shape) != 0)) {
            b.Add(FromRect(shape->GetPaintedBBox(PMMatrix(), Transform::PasteboardCoordinates())));
        }
    }
    return b.Grown(b.Empty() ? 0 : 1.0);
}

bool ReadFill(FillRecord& rec, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs,
              IDataBase* db, const void* port, std::uint32_t frame, std::uint32_t story, const ParcelKey& key)
{
    const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    const IWaxRenderData::RenderMethod method = data->GetRenderMethod(db, none);
    if (method != IWaxRenderData::kFillOnly && method != IWaxRenderData::kFillStroke) return false;
    if (data->GetColorUID(kFalse) == none || data->GetOverprint(kFalse)) return false;
    const PMMatrix toPb = run->GetWaxLine()->GetToPasteboardMatrix();
    rec.run = run;
    rec.port = port;
    rec.story = story;
    rec.frame = frame;
    rec.parcelKey = KeyValue(key);
    {
        // The run's stroke, for Close holes across frames (NeighborOutlines).
        IWaxRenderData* stroked = const_cast<IWaxRenderData*>(data);
        const double width = ToDouble(stroked->GetOutlineWidth());
        const int32 align = stroked->GetStrokeAlignment();
        const bool strokes = method == IWaxRenderData::kFillStroke && stroked->GetColorUID(kTrue) != none && width > 0;
        rec.reach = !strokes ? 0 : align == kStrokeAlignOutside ? width : align == kStrokeAlignCenter ? width / 2 : 0;
        rec.strokePaint = strokes ? stroked->GetColorUID(kTrue).Get() : 0;
    }
    if (glyphs->HasGlyphMatrix()) {
        // Rotated, scaled, or pathed glyphs: build the outlines now.
        OutlineCollector collect(&toPb);
        if (!CollectOutlines(run, data, glyphs, &collect)) return false;
        rec.glyphs = std::move(collect.Glyphs());
        for (const auto& g : rec.glyphs) rec.box.Add(g.box);
    } else {
        const int32 count = glyphs->GetGlyphCount();
        const Text::GlyphID* ids = glyphs->PeekGlyphArray();
        if (count <= 0 || ids == nil) return false;
        std::shared_ptr<RunSource> src = std::make_shared<RunSource>();
        IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
        src->font.reset(mutableData->QueryFont());
        if (src->font == nil) return false;
        src->fontMatrix = mutableData->GetFontMatrix();
        src->ids.assign(ids, ids + count);
        src->runX = run->GetXPosition();
        src->runY = run->GetYPosition();
        src->toPb = toPb;
        // A generous box: each glyph origin widened by one and a half em up and down, one em
        // left and two right, which holds any glyph of an ordinary font at that size.
        const PMMatrix& fm = src->fontMatrix;
        const double em = std::max(std::max(std::fabs(ToDouble(fm[0])), std::fabs(ToDouble(fm[1]))),
                                   std::max(std::fabs(ToDouble(fm[2])), std::fabs(ToDouble(fm[3]))));
        Box line;
        for (int32 i = 0; i < count; ++i) {
            const PMMatrix where = glyphs->GetGlyphDrawPosition(i);
            src->x.push_back(where.GetXTranslation());
            src->y.push_back(where.GetYTranslation());
            const double gx = ToDouble(src->runX + where.GetXTranslation()), gy = ToDouble(src->runY + where.GetYTranslation());
            line.Add(gx - em, gy - 1.5 * em);
            line.Add(gx + 2 * em, gy + 1.5 * em);
        }
        const double cx[4] = {line.x0, line.x1, line.x1, line.x0}, cy[4] = {line.y0, line.y0, line.y1, line.y1};
        for (int i = 0; i < 4; ++i) { PMReal x = cx[i], y = cy[i]; toPb.Transform(&x, &y); rec.box.Add(ToDouble(x), ToDouble(y)); }
        rec.resolved = false;
        rec.source = src;
    }
    return true;
}

// Screen item caches can replay earlier text without visiting its adornments. This snapshot
// uses visible layout order and read-only wax instead of treating cache replays as fresh paint.
// It lives for one parcel pass, holds no wax pointers, and never feeds print or PDF drawing.
thread_local Tracker gScreen;
thread_local bool gScreenReady = false;
thread_local bool gScreenRefused = false;
thread_local const void* gScreenPort = nullptr;

struct ScreenDependency {
    const void* database = nullptr;
    std::uint32_t spread = 0;
    std::vector<std::uint32_t> frames;
    std::unordered_map<std::uint32_t, bool> visibility;
    bool refreshPending = false;
};
thread_local std::vector<ScreenDependency> gScreenDependencies;

ScreenDependency& ScreenState(const void* database, std::uint32_t spread)
{
    for (auto& entry : gScreenDependencies)
        if (entry.database == database && entry.spread == spread) return entry;
    // Only opaque identities and unique UIDs survive a draw. Document close removes its
    // history. Do not evict a spread while its graphics caches can still contain old ink.
    gScreenDependencies.push_back({database, spread, {}});
    return gScreenDependencies.back();
}

std::vector<std::uint32_t>& ScreenDependencies(const void* database, std::uint32_t spread)
{
    return ScreenState(database, spread).frames;
}

bool ScreenPort(GraphicsData* gd);

void CheckScreenVisibility(DrawEventData* event)
{
    if (event == nullptr || event->changedBy == nil || !ScreenPort(event->gd) || !CrossFrameMode()) return;
    IDataBase* db = ::GetDataBase(event->changedBy);
    if (db == nil) return;
    const auto spread = ::GetUID(event->changedBy).Get();
    bool changed = false, pending = false;
    ScreenDependency* current = nullptr;
    for (auto& entry : gScreenDependencies) {
        if (entry.database != db) continue;
        pending = pending || entry.refreshPending;
        if (entry.spread != spread) continue;
        current = &entry;
        for (auto it = entry.visibility.begin(); it != entry.visibility.end();) {
            if (!db->IsValidUID(UID(it->first))) { it = entry.visibility.erase(it); continue; }
            InterfacePtr<IBoolData> visible(db, UID(it->first), IID_IVISIBLE);
            const bool state = visible == nil || visible->GetBool();
            if (state != it->second) { it->second = state; changed = true; }
            ++it;
        }
    }
    if (!changed || current == nullptr || pending) return;
    InterfacePtr<IIdleTask> task(db, db->GetRootUID(), IID_ISESCREENIDLE);
    if (task != nil) {
        current->refreshPending = true;
        task->InstallTask(0);
        Trace("H screen-visibility queued spread=%u", spread);
    }
}

void ResetScreen()
{
    if (gScreen.Active()) gScreen.End();
    gScreenReady = false;
    gScreenRefused = false;
    gScreenPort = nullptr;
}

bool ScreenPort(GraphicsData* gd)
{
    IViewPortAttributes* attrs = gd ? gd->GetViewPortAttributes() : nil;
    return Ctl().screen && gd != nil && gd->GetView() != nil && attrs != nil && !attrs->GetViewPortIsPrintingPort() && !Utils<IPDFLibraryUtilsPublic>()->IsPDFExportPort(attrs);
}

void WatchScreenSubject(IPMUnknown* item, const PMIID& protocol)
{
    if (item == nil) return;
    IDataBase* db = ::GetDataBase(item);
    if (db == nil) return;
    InterfacePtr<IObserver> observer(db, db->GetRootUID(), IID_ISESCREENOBSERVER);
    InterfacePtr<ISubject> subject(item, UseDefaultIID());
    if (observer != nil && subject != nil &&
        !subject->IsAttached(ISubject::kBothAttachments, observer, protocol, IID_ISESCREENOBSERVER))
        subject->AttachObserver(ISubject::kBothAttachments, observer, protocol, IID_ISESCREENOBSERVER);
}

class ScreenOrder : public ICallback
{
public:
    ScreenOrder(GraphicsData* gd, Tracker* tracker, std::uint32_t stop = 0, Box box = Box())
        : fGd(gd), fTracker(tracker), fStop(stop), fBox(box) {}
    virtual IDrawMgr* GetCallbackDrawManager() { return fGd->GetDrawManager(); }
    virtual void ExecuteCallback(const PMMatrix*, IPMUnknown* item, int32)
    {
        if (found || truncated || item == nil) return;
        if (++fItems > Tracker::kMaxEvents) { truncated = true; return; }
        // IterateShapeDrawOrder skips hidden layers but still visits individually hidden
        // items. A hidden group also hides its descendants, without changing their flags.
        InterfacePtr<IHierarchy> visibleNode(item, UseDefaultIID());
        while (visibleNode != nil) {
            InterfacePtr<IBoolData> visible(visibleNode, IID_IVISIBLE);
            if (visible != nil && fTracker != nullptr)
                ScreenState(::GetDataBase(visibleNode), visibleNode->GetSpreadUID().Get())
                    .visibility[::GetUID(visibleNode).Get()] = visible->GetBool();
            if (visible != nil && !visible->GetBool()) return;
            visibleNode.reset(visibleNode->QueryParent());
        }
        const std::uint32_t uid = ::GetUID(item).Get();
        InterfacePtr<ITextFrameColumn> column(item, UseDefaultIID());
        InterfacePtr<IFrameList> list(column ? column->QueryFrameList() : nil);
        // Multi-column containers can expose the same interface, but only their actual
        // columns occur in the frame list. Counting a container would record its own text twice.
        if (column != nil && (list == nil || list->GetFrameIndex(UID(uid)) < 0)) column.reset();
        if (column != nil) {
            InterfacePtr<ISubject> story(::GetDataBase(column), list->GetTextModelUID(), UseDefaultIID());
            if (fTracker != nullptr) WatchScreenSubject(story, IID_ITEXTMODEL);
            const Box box = ContentBox(item);
            bounds.push_back({uid, column->GetFrameListUID().Get(), box});
        }
        if (fTracker == nullptr) return;
        // Frame paint and intervening artwork still block correction. Ancestor shape boxes
        // are exempted by CoveredSince, but their own fills and strokes are not.
        auto content = [&](const Box& box, bool framePaint) {
            ContentEvent e;
            e.item = uid; e.port = fGd->GetGraphicsPort(); e.box = box; e.frameFill = framePaint;
            fTracker->Content(e);
        };
        if (!PaintsNothing(item)) content(ContentBox(item), false);
        for (const Box& box : FramePaintBoxes(item, false)) content(box, true);
        for (const Box& box : FramePaintBoxes(item, true)) content(box, true);
        if (uid == fStop) { found = true; return; }
        if (column == nil || (Ctl().mode == Mode::Story && column->GetFrameListUID().Get() != gParcel.story)) return;
        if (!fBox.Empty() && !ContentBox(item).Meets(fBox)) return;
        IDataBase* db = ::GetDataBase(column);
        InterfacePtr<IShape> shape(item, UseDefaultIID());
        InterfacePtr<IXPManager> xp(Utils<IXPUtils>()->QueryXPManager(db));
        if (shape == nil || xp == nil || xp->ItemHasTransparencyInParentHierarchy(shape)) return;
        // Wax belongs to the frame-list strand, not the text-model boss.
        InterfacePtr<IWaxStrand> strand(list, UseDefaultIID());
        if (list == nil || strand == nil) return;
        const TextIndex start = list->GetFrameStart(::GetUID(column));
        const int32 span = list->GetFrameSpan(::GetUID(column));
        if (start < 0 || span <= 0) return;
        std::unique_ptr<const IWaxIterator> it(strand->NewReadOnlyWaxIterator());
        if (!it) return;
        int lines = 0;
        for (const IWaxLine* line = it->GetFirstWaxLine(start); line && line->TextOrigin() < start + span; line = it->GetNextWaxLine()) {
            if (++lines > 400) { truncated = true; return; }
            for (int32 i = 0, n = line->GetRunCount(); i < n; ++i) {
                InterfacePtr<IWaxRun> run(line->QueryRun(i));
                InterfacePtr<IWaxRenderData> data(run, UseDefaultIID());
                InterfacePtr<IWaxGlyphs> glyphs(run, UseDefaultIID());
                if (run == nil || data == nil || glyphs == nil || run->IsVerticalWritingMode()) continue;
                FillRecord rec;
                if (!ReadFill(rec, run, data, glyphs, db, fGd->GetGraphicsPort(), uid,
                              column->GetFrameListUID().Get(), line->GetParcelKey())) continue;
                // Iterator-owned wax can be reused for the next line. A sequential identity is
                // enough for this resolved or self-contained snapshot; it is never dereferenced.
                rec.run = reinterpret_cast<const void*>(++fRuns);
                const void* identity = rec.run;
                if (fTracker->Pending(std::move(rec))) fTracker->Painted(identity, fGd->GetGraphicsPort());
            }
        }
    }
    std::vector<FrameBounds> bounds;
    bool found = false, truncated = false;
private:
    GraphicsData* fGd;
    Tracker* fTracker;
    std::uint32_t fStop;
    Box fBox;
    std::size_t fItems = 0;
    std::uintptr_t fRuns = 0;
};

Tracker* ScreenRecords(GraphicsData* gd, const IWaxRun* run)
{
    if (!ScreenPort(gd) || !CrossFrameMode() || run == nil || !SameParcel(run) || gParcel.transparent ||
        (Ctl().mode == Mode::Story && gParcel.parcels < 2)) return nullptr;
    if (gScreenReady && gScreenPort == gd->GetGraphicsPort()) return &gScreen;
    if (gScreenRefused && gScreenPort == gd->GetGraphicsPort()) return nullptr;
    ResetScreen();
    gScreen.Begin();
    const std::size_t overflow = gScreen.overflow;
    gScreenPort = gd->GetGraphicsPort();
    IDataBase* db = run->GetDataBase();
    if (db == nil) { ResetScreen(); return nullptr; }
    // A canceled close/revert can leave a live document detached. The next
    // snapshot restores the idempotent subscriptions, once per parcel pass.
    InterfacePtr<IObserver> observer(db, db->GetRootUID(), IID_ISESCREENOBSERVER);
    if (observer != nil) observer->AutoAttach();
    InterfacePtr<IHierarchy> frame(db, UID(gParcel.frame), UseDefaultIID());
    InterfacePtr<IShape> spread(db, frame ? frame->GetSpreadUID() : kInvalidUID, UseDefaultIID());
    ScreenOrder order(gd, &gScreen, gParcel.frame, ContentBox(frame).Grown(1));
    if (spread != nil) spread->IterateShapeDrawOrder(&order, IShape::kSkipHiddenLayers | IShape::kSkipGuideLayers | IShape::kSkipMasterPageItems);
    if (spread != nil) {
        auto& dependencies = ScreenDependencies(db, ::GetUID(spread).Get());
        RememberScreenFrames(dependencies, order.bounds);
        RememberScreenFrames(dependencies, {{gParcel.frame, gParcel.story, Box()}});
    }
    if (!order.found || order.truncated || gScreen.overflow != overflow) {
        Trace("H screen-refused frame=%u found=%d truncated=%d", gParcel.frame, order.found ? 1 : 0, order.truncated ? 1 : 0);
        // Refusal is cached for this parcel/port too: retrying a bounded spread
        // walk for every glyph would multiply the fallback cost. EndParcel and
        // model invalidation clear both successful and refused snapshots.
        gScreen.End();
        gScreenRefused = true;
        return nullptr;
    }
    gScreenReady = true;
    Trace("H screen-snapshot frame=%u records=%u port=%p", gParcel.frame, static_cast<unsigned>(gScreen.Records()), gScreenPort);
    return &gScreen;
}

// Invalidation events precede drawing and already carry its layout context. Extend those
// invals to overlapping text only; no document scan or cache invalidation occurs per run.
void InvalidateScreenNeighbors(InvalEventData* event)
{
    if (event == nullptr || !ScreenPort(event->gd) || event->gd->GetView() == nil || !CrossFrameMode()) return;
    thread_local bool busy = false;
    if (busy) return;
    struct Guard { bool& b; Guard(bool& v) : b(v) { b = true; } ~Guard() { b = false; } } guard(busy);
    InterfacePtr<IShape> spread(event->changedBy, UseDefaultIID());
    if (spread == nil) return;
    ScreenOrder order(event->gd, nullptr);
    spread->IterateShapeDrawOrder(&order, IShape::kSkipHiddenLayers | IShape::kSkipGuideLayers | IShape::kSkipMasterPageItems);
    if (order.truncated) {
        // An incomplete scan must not replace the previous overlap participants. Clear the
        // spread cache conservatively; the next complete scan can narrow its dependencies.
        ResetScreen();
        Utils<ILayoutUIUtils>()->InvalidateShapeGraphixCache(spread);
        spread->Inval(event->gd, event->reasonForInval);
        Trace("H screen-inval truncated: spread cache invalidated");
        FlushTrace();
        return;
    }
    const auto related = RelatedFrames(order.bounds, Ctl().mode == Mode::Overlap);
    IDataBase* db = ::GetDataBase(spread);
    const UID spreadUID = ::GetUID(spread);
    auto& previous = ScreenDependencies(db, spreadUID.Get());
    std::vector<std::uint32_t> affected = related;
    affected.insert(affected.end(), previous.begin(), previous.end());
    std::sort(affected.begin(), affected.end());
    affected.erase(std::unique(affected.begin(), affected.end()), affected.end());
    RememberScreenFrames(previous, order.bounds);
    ResetScreen();
    std::vector<std::uint32_t> invalidated;
    for (std::uint32_t uid : affected) {
        if (!db->IsValidUID(UID(uid))) continue;
        InterfacePtr<IHierarchy> node(db, UID(uid), UseDefaultIID());
        // A column can be inside a cached multi-column frame or group. Invalidate that
        // chain too, since clearing the leaf alone can still replay its parent's old ink.
        while (node != nil && ::GetUID(node) != spreadUID) {
            const std::uint32_t parentUID = ::GetUID(node).Get();
            if (std::find(invalidated.begin(), invalidated.end(), parentUID) == invalidated.end()) {
                invalidated.push_back(parentUID);
                InterfacePtr<IShape> shape(node, UseDefaultIID());
                if (shape != nil) {
                    Utils<ILayoutUIUtils>()->InvalidateShapeGraphixCache(shape);
                    shape->Inval(event->gd, event->reasonForInval);
                }
            }
            node.reset(node->QueryParent());
        }
    }
    Trace("H screen-inval spread=%u frames=%u", spreadUID.Get(), static_cast<unsigned>(affected.size()));
    FlushTrace();
}


} // namespace

void EndParcel() { ResetScreen(); }

void RecordPendingFill(GraphicsData* gd, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db)
{
    if (!CrossFrameMode() || gd == nil || run == nil || data == nil || glyphs == nil || db == nil) return;
    Tracker& t = ThreadTracker();
    if (!t.Active() || !SameParcel(run) || gParcel.transparent) {
        Trace("F unavailable active=%d same=%d xp=%d", t.Active() ? 1 : 0, SameParcel(run) ? 1 : 0, gParcel.transparent ? 1 : 0);
        return;
    }
    if (Ctl().mode == Mode::Story && gParcel.parcels < 2) return;   // a one-parcel story has no other frame
    FillRecord rec;
    if (!ReadFill(rec, run, data, glyphs, db, gd->GetGraphicsPort(), gParcel.frame, gParcel.story, gParcel.key)) return;
    const Box b = rec.box;
    const bool lazy = !rec.resolved;
    const bool kept = t.Pending(std::move(rec));
    Trace("F pending run=%p kept=%d lazy=%d frame=%u box=%.2f,%.2f,%.2f,%.2f", run, kept ? 1 : 0,
          lazy ? 1 : 0, gParcel.frame, b.x0, b.y0, b.x1, b.y1);
}

std::vector<ise_miter::Contour> NeighborOutlines(GraphicsData* gd, const IWaxRun* run, double x0, double y0, double x1, double y1,
                                                 double reach, std::uint32_t paint)
{
    std::vector<ise_miter::Contour> out;
    if (!CrossFrameMode() || gd == nil || run == nil || run->GetWaxLine() == nil) return out;
    Tracker* screen = ScreenRecords(gd, run);
    Tracker& t = screen ? *screen : ThreadTracker();
    if (!t.Active() || t.PaintedRecords() == 0 || !SameParcel(run)) {
        Trace("H unavailable active=%d painted=%u same=%d port=%p", t.Active() ? 1 : 0,
              static_cast<unsigned>(t.PaintedRecords()), SameParcel(run) ? 1 : 0, gd->GetGraphicsPort());
        return out;
    }
    const PMMatrix toPb = run->GetWaxLine()->GetToPasteboardMatrix();
    StrokeContext s;
    s.port = gd->GetGraphicsPort();
    s.story = gParcel.story;
    s.frame = gParcel.frame;
    s.parcelKey = KeyValue(gParcel.key);
    s.own = gParcel.own;
    const double cx[4] = {x0, x1, x1, x0}, cy[4] = {y0, y0, y1, y1};
    for (int i = 0; i < 4; ++i) { PMReal x = cx[i], y = cy[i]; toPb.Transform(&x, &y); s.strokeBox.Add(ToDouble(x), ToDouble(y)); }
    NeighborStats stats;
    const std::vector<const Glyph*> nearby = t.Neighbors(s, Ctl().mode == Mode::Overlap, reach, paint, Resolve, Tracing() ? &stats : nullptr);
    const PMMatrix toLine = toPb.Inverse();
    for (const Glyph* g : nearby) {
        for (const Seg& seg : g->segs) {
            PMReal q[6];
            for (int i = 0; i < 6; i += 2) { PMReal x = seg.p[i], y = seg.p[i + 1]; toLine.Transform(&x, &y); q[i] = x; q[i + 1] = y; }
            if (seg.op == Seg::Move) { out.emplace_back(); out.back().start = {ToDouble(q[0]), ToDouble(q[1])}; continue; }
            if (out.empty() || seg.op == Seg::Close) continue;
            ise_miter::Segment s2;
            if (seg.op == Seg::Curve) {
                s2.curve = true;
                s2.c1 = {ToDouble(q[0]), ToDouble(q[1])};
                s2.c2 = {ToDouble(q[2]), ToDouble(q[3])};
                s2.p = {ToDouble(q[4]), ToDouble(q[5])};
            } else {
                s2.p = {ToDouble(q[0]), ToDouble(q[1])};
            }
            out.back().segments.push_back(s2);
        }
    }
    Trace("H neighbors run=%p source=%s glyphs=%u contours=%u records=%u pending=%u ports=%u stories=%u own=%u stroke=%u bounds=%u covered=%u",
          run, screen ? "layout" : "paint", static_cast<unsigned>(nearby.size()), static_cast<unsigned>(out.size()),
          static_cast<unsigned>(t.Records()), static_cast<unsigned>(stats.pending), static_cast<unsigned>(stats.otherPort),
          static_cast<unsigned>(stats.otherStory), static_cast<unsigned>(stats.ownParcel), static_cast<unsigned>(stats.stroke),
          static_cast<unsigned>(stats.bounds), static_cast<unsigned>(stats.covered));
    return out;
}

void ConfirmFill(GraphicsData* gd, const IWaxRun* run)
{
    if (!CrossFrameMode() || gd == nil || run == nil) return;
    const bool ok = ThreadTracker().Painted(run, gd->GetGraphicsPort());
    if (ok) Trace("F painted run=%p", run);
}

int ClipWithheld(GraphicsData* gd, IGraphicsPort* port, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase*)
{
    if (!CrossFrameMode() || gd == nil || port == nil) return 0;
    Tracker* screen = ScreenRecords(gd, run);
    Tracker& t = screen ? *screen : ThreadTracker();
    if (!t.Active() || t.PaintedRecords() == 0) return 0;
    if (!SameParcel(run)) { Trace("K run=%p parcel-mismatch", run); return 0; }
    OutlineCollector local(nullptr);
    if (!CollectOutlines(run, data, glyphs, &local)) return 0;
    IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
    const double width = ToDouble(mutableData->GetOutlineWidth());
    const double miter = std::max(1.0, ToDouble(mutableData->GetMiterLimit()));
    const Box lineBox = local.All().Grown(width * miter + 1.0);   // the stroke's ink, in line space
    const PMMatrix toPb = run->GetWaxLine()->GetToPasteboardMatrix();
    StrokeContext s;
    s.port = gd->GetGraphicsPort();
    s.story = gParcel.story;
    s.frame = gParcel.frame;
    s.parcelKey = KeyValue(gParcel.key);
    s.own = gParcel.own;
    const double cx[4] = {lineBox.x0, lineBox.x1, lineBox.x1, lineBox.x0}, cy[4] = {lineBox.y0, lineBox.y0, lineBox.y1, lineBox.y1};
    for (int i = 0; i < 4; ++i) { PMReal x = cx[i], y = cy[i]; toPb.Transform(&x, &y); s.strokeBox.Add(ToDouble(x), ToDouble(y)); }
    size_t covered = 0;
    const std::vector<const Glyph*> withheld = t.Withheld(s, Ctl().mode == Mode::Overlap, &covered, Resolve);
    Trace("K run=%p withheld=%u covered=%u box=%.2f,%.2f,%.2f,%.2f", run, static_cast<unsigned>(withheld.size()),
          static_cast<unsigned>(covered), s.strokeBox.x0, s.strokeBox.y0, s.strokeBox.x1, s.strokeBox.y1);
    if (withheld.empty()) return 0;
    const PMMatrix toLine = toPb.Inverse();
    const Box big = lineBox.Grown(1.0);
    for (const Glyph* g : withheld) {
        // The glyph resolved to its filled region, and the clip to its outside built so that it
        // means the same under either fill rule (ise_miter::OutsideClip): InDesign's PDF port writes
        // an even-odd clip as a nonzero one, and a glyph with overlapping contours stayed inside an
        // even-odd clip where its contours overlap (the review of 2026-11-01). The frame reaches
        // past the stroke's whole box. When the region cannot be made, the even-odd clip below.
        std::vector<ise_miter::Contour> contours;
        for (const Seg& seg : g->segs) {
            PMReal q[6];
            for (int i = 0; i < 6; i += 2) { PMReal x = seg.p[i], y = seg.p[i + 1]; toLine.Transform(&x, &y); q[i] = x; q[i + 1] = y; }
            if (seg.op == Seg::Move) { contours.emplace_back(); contours.back().start = {ToDouble(q[0]), ToDouble(q[1])}; continue; }
            if (contours.empty() || seg.op == Seg::Close) continue;
            ise_miter::Segment s2;
            if (seg.op == Seg::Curve) {
                s2.curve = true;
                s2.c1 = {ToDouble(q[0]), ToDouble(q[1])};
                s2.c2 = {ToDouble(q[2]), ToDouble(q[3])};
                s2.p = {ToDouble(q[4]), ToDouble(q[5])};
            } else {
                s2.p = {ToDouble(q[0]), ToDouble(q[1])};
            }
            contours.back().segments.push_back(s2);
        }
        const std::vector<ise_miter::Contour> region = contours.empty() ? contours : ise_region::NonzeroRegion(contours, 0.01);
        std::vector<ise_miter::Contour> clip;
        if (!region.empty()) {
            double gx0 = 1e300, gy0 = 1e300, gx1 = -1e300, gy1 = -1e300;
            for (const ise_miter::Contour& c : region) {
                gx0 = std::min(gx0, c.start.x); gx1 = std::max(gx1, c.start.x); gy0 = std::min(gy0, c.start.y); gy1 = std::max(gy1, c.start.y);
                for (const ise_miter::Segment& s2 : c.segments) {
                    gx0 = std::min(gx0, s2.p.x); gx1 = std::max(gx1, s2.p.x); gy0 = std::min(gy0, s2.p.y); gy1 = std::max(gy1, s2.p.y);
                }
            }
            const double reach = std::max(std::max(gx0 - big.x0, big.x1 - gx1), std::max(gy0 - big.y0, big.y1 - gy1));
            clip = ise_miter::OutsideClip(region, 2 * std::max(reach, 0.0) + 10, 0.01, 0.01, true, false);
            // Checked exactly, as OutsideClip's contract asks without its probes; a clip that fails
            // falls back to the even-odd one below (the second review of 2026-11-01).
            if (!clip.empty() && !ise_region::ClipsOutside(clip, contours, 0.01)) clip.clear();
        }
        if (!clip.empty()) {
            port->newpath();
            for (const ise_miter::Contour& c : clip) {
                port->moveto(c.start.x, c.start.y);
                for (const ise_miter::Segment& s2 : c.segments) {
                    if (s2.curve) port->curveto(s2.c1.x, s2.c1.y, s2.c2.x, s2.c2.y, s2.p.x, s2.p.y);
                    else port->lineto(s2.p.x, s2.p.y);
                }
                port->closepath();
            }
            port->clip();
            port->newpath();
            continue;
        }
        port->newpath();
        port->rectpath(big.x0, big.y0, big.x1 - big.x0, big.y1 - big.y0);
        for (const Seg& seg : g->segs) {
            PMReal p[6];
            for (int i = 0; i < 6; i += 2) { PMReal x = seg.p[i], y = seg.p[i + 1]; toLine.Transform(&x, &y); p[i] = x; p[i + 1] = y; }
            switch (seg.op) {
            case Seg::Move: port->moveto(p[0], p[1]); break;
            case Seg::Line: port->lineto(p[0], p[1]); break;
            case Seg::Curve: port->curveto(p[0], p[1], p[2], p[3], p[4], p[5]); break;
            case Seg::Close: port->closepath(); break;
            }
        }
        port->eoclip();
        port->newpath();
    }
    return static_cast<int>(withheld.size());
}

void TraceRun(char tag, GraphicsData* gd, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db, const char* what)
{
#ifdef ISE_ROUTE_X_RESEARCH
    if (!Tracing() || run == nil || data == nil || db == nil) return;
    const IWaxLine* line = run->GetWaxLine();
    const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    PMMatrix m = line ? line->GetToPasteboardMatrix() : PMMatrix();
    Trace("R %c %s run=%p port=%p kind=%s same=%d method=%d align=%d w=%.3f fill=%u stroke=%u none=%u op=%d glyphs=%d x=%.3f y=%.3f lineY=%.3f pb=[%.4f %.4f %.4f %.4f %.3f %.3f]",
          tag, what, run, gd ? gd->GetGraphicsPort() : nullptr, PortKind(gd), SameParcel(run) ? 1 : 0,
          static_cast<int>(data->GetRenderMethod(db, none)), static_cast<int>(data->GetStrokeAlignment()),
          ToDouble(data->GetOutlineWidth()), data->GetColorUID(kFalse).Get(), data->GetColorUID(kTrue).Get(), none.Get(),
          data->GetOverprint(kFalse) ? 1 : 0, glyphs ? glyphs->GetGlyphCount() : -1,
          ToDouble(run->GetXPosition()), ToDouble(run->GetYPosition()), line ? ToDouble(line->GetYPosition()) : 0.0,
          ToDouble(m[0]), ToDouble(m[1]), ToDouble(m[2]), ToDouble(m[3]), ToDouble(m[4]), ToDouble(m[5]));
#else
    (void)tag; (void)gd; (void)run; (void)data; (void)glyphs; (void)db; (void)what;
#endif
}

} // namespace ise_cross_frame

// ---------------------------------------------------------------- draw events

using namespace ise_cross_frame;

// Model edits do not necessarily emit spread invalidation draw events. Lazy document
// notifications run after the transaction, including undo and redo, outside painting.
class IseScreenObserver : public CObserver
{
public:
    explicit IseScreenObserver(IPMUnknown* boss) : CObserver(boss, IID_ISESCREENOBSERVER) {}
    void AutoAttach() override { Attach(true); }
    void AutoDetach() override
    {
        InterfacePtr<IIdleTask> task(this, IID_ISESCREENIDLE);
        if (task != nil) task->UninstallTask();
        // Subjects include stories, not just the document itself.
        if (GetChangeManager()) GetChangeManager()->RemoveObserver(::GetUIDRef(this), IID_ISESCREENOBSERVER);
        const auto db = ::GetDataBase(this);
        gScreenDependencies.erase(std::remove_if(gScreenDependencies.begin(), gScreenDependencies.end(),
            [db](const ScreenDependency& entry) { return entry.database == db; }), gScreenDependencies.end());
        ResetScreen();
    }
    void Update(const ClassID& change, ISubject*, const PMIID& protocol, void*) override
    {
        if (fBusy || protocol == IID_ISESCREENOBSERVER) return;
        Trace("H screen-notify change=%u protocol=%u", change.Get(), protocol.Get());
        // Some legacy subjects send only regular Change notifications. Forward them to
        // our private lazy protocol so cache invalidation runs after the transaction.
        InterfacePtr<ISubject> doc(this, UseDefaultIID());
        if (doc != nil) doc->ModelChange(change, IID_ISESCREENOBSERVER);
    }
    void LazyUpdate(ISubject*, const PMIID& protocol, const LazyNotificationData*) override
    {
        if (fBusy) return;
        ReloadControl();
        const bool active = Ctl().screen && CrossFrameMode();
        struct Guard { bool& busy; Guard(bool& b) : busy(b) { busy = true; } ~Guard() { busy = false; } } guard(fBusy);
        IDataBase* db = ::GetDataBase(this);
        std::unordered_set<std::uint32_t> invalidated;
        // Retain every drawn frame, including frames that can gain their first neighbor.
        // Prune deleted identities before attempting to load a database object.
        for (auto& entry : gScreenDependencies) {
            if (entry.database != db) continue;
            entry.frames.erase(std::remove_if(entry.frames.begin(), entry.frames.end(),
                [db](std::uint32_t uid) { return !db->IsValidUID(UID(uid)); }), entry.frames.end());
            for (auto uid : entry.frames) {
                InterfacePtr<IHierarchy> node(db, UID(uid), UseDefaultIID());
                while (node != nil && ::GetUID(node).Get() != entry.spread) {
                    const auto id = ::GetUID(node).Get();
                    if (!invalidated.insert(id).second) break;
                    InterfacePtr<IShape> shape(node, UseDefaultIID());
                    if (shape != nil) Utils<ILayoutUIUtils>()->InvalidateShapeGraphixCache(shape);
                    node.reset(node->QueryParent());
                }
            }
        }
        if (!active) {
            InterfacePtr<IIdleTask> task(this, IID_ISESCREENIDLE);
            if (task != nil) task->UninstallTask();
            gScreenDependencies.erase(std::remove_if(gScreenDependencies.begin(), gScreenDependencies.end(),
                [db](const ScreenDependency& entry) { return entry.database == db; }), gScreenDependencies.end());
        }
        if (invalidated.empty()) return;
        ResetScreen();
        InterfacePtr<IDocument> doc(this, UseDefaultIID());
        if (doc != nil) Utils<ILayoutUIUtils>()->InvalidateViews(doc);
        Trace("H screen-edit protocol=%u caches=%u", protocol.Get(), static_cast<unsigned>(invalidated.size()));
        FlushTrace();
    }
private:
    void Attach(bool attach)
    {
        InterfacePtr<ISubject> subject(this, UseDefaultIID());
        if (subject == nil) return;
        const PMIID protocols[] = { IID_IHIERARCHY_DOCUMENT, IID_ITRANSFORM_DOCUMENT,
            IID_IGEOMETRY_DOCUMENT, IID_IPATHGEOMETRY_DOCUMENT,
            IID_IGRAPHICSTYLE_OVERRIDES_DOCUMENT, IID_IGRAPHICSTYLE_REFERENCE_DOCUMENT,
            IID_IXPATTRIBUTES_OVERRIDES_DOCUMENT, IID_ITEXTMODEL_DOCUMENT,
            IID_ILAYERLIST, IID_IHIERARCHY_VISIBILITY, IID_ISESCREENOBSERVER };
        for (const auto& protocol : protocols) {
            const auto type = ISubject::kLazyAttachment;
            const bool attached = subject->IsAttached(type, this, protocol, IID_ISESCREENOBSERVER);
            if (attach && !attached) subject->AttachObserver(type, this, protocol, IID_ISESCREENOBSERVER);
            if (!attach && attached) subject->DetachObserver(type, this, protocol, IID_ISESCREENOBSERVER);
        }
    }
    bool fBusy = false;
};
CREATE_PMINTERFACE(IseScreenObserver, kIseScreenObserverImpl)

// Visibility changes can bypass model notifications. One queued task refreshes
// their dependent frames after painting, never by executing a document command.
class IseScreenIdle : public CIdleTask
{
public:
    explicit IseScreenIdle(IPMUnknown* boss) : CIdleTask(boss) {}
    uint32 RunTask(uint32, IdleTimer*) override
    {
        InterfacePtr<IIdleTask> keepAlive(this, IID_ISESCREENIDLE);
        UninstallTask();
        const auto db = ::GetDataBase(this);
        for (auto& entry : gScreenDependencies)
            if (entry.database == db) entry.refreshPending = false;
        InterfacePtr<IObserver> observer(this, IID_ISESCREENOBSERVER);
        if (observer != nil) observer->LazyUpdate(nil, IID_ISESCREENIDLE, nil);
        return 0;
    }
    const char* TaskName() override { return "StrokeMaster screen visibility refresh"; }
};
CREATE_PMINTERFACE(IseScreenIdle, kIseScreenIdleImpl)

class IseScreenDocResponder : public CResponder
{
public:
    explicit IseScreenDocResponder(IPMUnknown* boss) : CResponder(boss) {}
    void Respond(ISignalMgr* signal) override
    {
        InterfacePtr<IDocumentSignalData> data(signal, UseDefaultIID());
        if (data == nil) return;
        InterfacePtr<IObserver> observer(data->GetDocument(), IID_ISESCREENOBSERVER);
        if (observer == nil) return;
        if (signal->GetServiceID() == kBeforeCloseDocSignalResponderService ||
            signal->GetServiceID() == kBeforeRevertDocSignalResponderService) observer->AutoDetach();
        else observer->AutoAttach();
    }
};
CREATE_PMINTERFACE(IseScreenDocResponder, kIseScreenDocResponderImpl)

class IseScreenDocService : public CServiceProvider
{
public:
    explicit IseScreenDocService(IPMUnknown* boss) : CServiceProvider(boss) {}
    ServiceID GetServiceID() override { return kAfterNewDocSignalResponderService; }
    bool16 HasMultipleIDs() const override { return kTrue; }
    void GetServiceIDs(K2Vector<ServiceID>& ids) override
    {
        ids.push_back(kAfterNewDocSignalResponderService);
        ids.push_back(kDuringOpenDocSignalResponderService);
        ids.push_back(kBeforeCloseDocSignalResponderService);
        ids.push_back(kBeforeRevertDocSignalResponderService);
        ids.push_back(kAfterRevertDocSignalResponderService);
    }
    bool16 IsDefaultServiceProvider() override { return kFalse; }
    InstancePerX GetInstantiationPolicy() override { return kInstancePerSession; }
    void GetName(PMString* name) override { name->SetKey("IseScreenDocService"); }
};
CREATE_PMINTERFACE(IseScreenDocService, kIseScreenDocServiceImpl)

namespace {

struct EventName { ClassID id; const char* name; };

#ifdef ISE_ROUTE_X_RESEARCH
const EventName kEvents[] = {
    {kBeforeFirstSpreadDrawMessage, "FirstSpread"}, {kAfterLastSpreadDrawMessage, "LastSpread"},
    {kBeginSpreadMessage, "BeginSpread"}, {kEndSpreadMessage, "EndSpread"},
    {kBeforePartialSpreadDrawMessage, "BeginPartial"}, {kAfterPartialSpreadDrawMessage, "EndPartial"},
    {kBeginMasterSpreadMessage, "BeginMaster"}, {kEndMasterSpreadMessage, "EndMaster"},
    {kBeforeContentMessage, "BeforeContent"}, {kAfterContentMessage, "AfterContent"},
    {kBeginLayerMessage, "BeginLayer"}, {kEndLayerMessage, "EndLayer"},
    {kBeginPageMessage, "BeginPage"}, {kEndPageMessage, "EndPage"},
    {kBeginGroupMessage, "BeginGroup"}, {kEndGroupMessage, "EndGroup"},
    {kBeginShapeMessage, "BeginShape"}, {kDrawShapeMessage, "DrawShape"}, {kEndShapeMessage, "EndShape"},
    {kBeforeFrameBackground, "FrameBackground"}, {kBeforeFrameText, "FrameText"},
    {kBeforeFrameForeground, "FrameForeground"}, {kAfterFrameForeground, "AfterFrameForeground"},
    {kBeginFrameFillDrawMessage, "BeginFrameFill"}, {kEndFrameFillDrawMessage, "EndFrameFill"},
    {kBeginFrameStrokeDrawMessage, "BeginFrameStroke"}, {kEndFrameStrokeDrawMessage, "EndFrameStroke"},
    {kBeginFrameHierarchyDrawMessage, "BeginFrameHierarchy"}, {kEndFrameHierarchyDrawMessage, "EndFrameHierarchy"},
    {kBeforeFlattenerShowPage, "BeforeFlattener"}, {kAfterFlattenerShowPage, "AfterFlattener"},
};
#else
const EventName kEvents[] = {
    {kBeginSpreadMessage, "BeginSpread"}, {kEndSpreadMessage, "EndSpread"},
    {kBeforePartialSpreadDrawMessage, "BeginPartial"}, {kAfterPartialSpreadDrawMessage, "EndPartial"},
    {kBeginShapeMessage, "BeginShape"},
    {kBeginFrameFillDrawMessage, "BeginFrameFill"}, {kBeginFrameStrokeDrawMessage, "BeginFrameStroke"},
};
#endif


#ifdef ISE_ROUTE_X_RESEARCH
// Draws every painted record's glyphs, filled magenta, from a broader callback, placed by the
// callback item's inner-to-pasteboard matrix: evidence of what that scope can still emit.
void DeferProbe(GraphicsData* gd, IPMUnknown* item)
{
    IGraphicsPort* port = gd ? gd->GetGraphicsPort() : nil;
    if (port == nil || item == nil) return;
    const PMMatrix toPb = ::InnerToPasteboardMatrix(item);
    const PMMatrix toInner = toPb.Inverse();
    std::vector<const Glyph*> all;
    StrokeContext s;
    s.port = port;
    s.strokeBox.Add(-1e7, -1e7); s.strokeBox.Add(1e7, 1e7);
    s.frame = 0xFFFFFFFFu;
    all = ThreadTracker().Withheld(s, true, nullptr, Resolve);
    AutoGSave save(port);
    port->setrgbcolor(1, 0, 1);
    port->newpath();
    for (const Glyph* g : all) {
        for (const Seg& seg : g->segs) {
            PMReal p[6];
            for (int i = 0; i < 6; i += 2) { PMReal x = seg.p[i], y = seg.p[i + 1]; toInner.Transform(&x, &y); p[i] = x; p[i + 1] = y; }
            switch (seg.op) {
            case Seg::Move: port->moveto(p[0], p[1]); break;
            case Seg::Line: port->lineto(p[0], p[1]); break;
            case Seg::Curve: port->curveto(p[0], p[1], p[2], p[3], p[4], p[5]); break;
            case Seg::Close: port->closepath(); break;
            }
        }
    }
    port->fill();
    Trace("D defer-probe glyphs=%u item=%s", static_cast<unsigned>(all.size()), ClassName(item));
}
#endif

} // namespace

class IseXfDrawEvents : public CPMUnknown<IDrwEvtHandler>
{
public:
    explicit IseXfDrawEvents(IPMUnknown* boss) : CPMUnknown<IDrwEvtHandler>(boss) {}
    virtual void Register(IDrwEvtDispatcher* d)
    {
        for (const auto& e : kEvents) d->RegisterHandler(e.id, this, kDEHMediumPriority);
        d->RegisterHandler(kAfterSpreadInvalMessage, this, kDEHPostProcess);
    }
    virtual void UnRegister(IDrwEvtDispatcher* d)
    {
        for (const auto& e : kEvents) d->UnRegisterHandler(e.id, this);
        d->UnRegisterHandler(kAfterSpreadInvalMessage, this);
    }
    virtual bool16 HandleDrawEvent(ClassID eventID, void* eventData);
};

CREATE_PMINTERFACE(IseXfDrawEvents, kIseXfDrawEventHandlerImpl)

bool16 IseXfDrawEvents::HandleDrawEvent(ClassID eventID, void* eventData)
{
    if (eventID == kAfterSpreadInvalMessage) {
        ReloadControl();
        InvalidateScreenNeighbors(static_cast<InvalEventData*>(eventData));
        return kFalse;
    }
    DrawEventData* ded = static_cast<DrawEventData*>(eventData);
    if (ded == nullptr) return kFalse;
    Tracker& t = ThreadTracker();
    const std::uint32_t uid = ded->changedBy ? ::GetUID(ded->changedBy).Get() : 0;
    const void* port = ded->gd ? ded->gd->GetGraphicsPort() : nullptr;
    const bool opens = eventID == kBeginSpreadMessage || eventID == kBeforePartialSpreadDrawMessage;
    const bool closes = eventID == kEndSpreadMessage || eventID == kAfterPartialSpreadDrawMessage;
    // A new outermost traversal: the frame last noted is noted again (a frame threaded or made
    // translucent since kept its old details: the second review of 2026-11-01). A spread draw that
    // opens on the port of one never closed starts the bookkeeping over (Tracker::Begin).
    if (opens) {
        ReloadControl();
        CheckScreenVisibility(ded);
        ResetScreen();
        if (!t.Active()) gParcel = ParcelInfo();
        t.Begin(eventID == kBeginSpreadMessage, port);
    }
    bool16 handled = kFalse;
#ifdef ISE_ROUTE_X_RESEARCH
    if (Tracing()) {
        const char* name = "?";
        for (const auto& e : kEvents) if (e.id == eventID) { name = e.name; break; }
        Trace("E %llu %s depth=%d item=%u class=%s flags=0x%x port=%p kind=%s", static_cast<unsigned long long>(t.Next()),
              name, t.Depth(), uid, ClassName(ded->changedBy), static_cast<unsigned>(ded->flags), port, PortKind(ded->gd));
    }
    if (eventID == kDrawShapeMessage && Ctl().suppressShape != 0 && uid == Ctl().suppressShape) {
        Trace("S suppress-shape item=%u", uid);
        handled = kTrue;
    }
    if (eventID == kBeforeFrameText && Ctl().suppressText != 0 && uid == Ctl().suppressText) {
        Trace("S suppress-text item=%u", uid);
        handled = kTrue;
    }
    if (eventID == kAfterContentMessage && Ctl().deferProbe) DeferProbe(ded->gd, ded->changedBy);
#endif
    // Content drawn before any fill is painted cannot cover one: nothing is kept until then.
    if (CrossFrameMode() && t.Active() && t.PaintedRecords() > 0) {
        if (eventID == kBeginShapeMessage || eventID == kBeginFrameFillDrawMessage || eventID == kBeginFrameStrokeDrawMessage) {
            const std::vector<Box> boxes = eventID == kBeginShapeMessage
                ? (PaintsNothing(ded->changedBy) ? std::vector<Box>{} : std::vector<Box>{ContentBox(ded->changedBy)})
                : FramePaintBoxes(ded->changedBy, eventID == kBeginFrameStrokeDrawMessage);
            for (const Box& b : boxes) {
                ContentEvent e;
                e.item = uid;
                e.frameFill = eventID != kBeginShapeMessage;
                e.port = port;
                e.box = b;
                t.Content(e);   // an empty box is not kept
                Trace("C %s item=%u box=%.2f,%.2f,%.2f,%.2f", eventID == kBeginShapeMessage ? "shape" : eventID == kBeginFrameFillDrawMessage ? "frame-fill" : "frame-stroke",
                      uid, b.x0, b.y0, b.x1, b.y1);
            }
        }
    }
    // Page-item strokes: drawn here, in the item's inner coordinates, when this route can draw
    // them exactly; claiming the event makes InDesign skip its own stroke (CGraphicFrameShape).
    if (eventID == kBeginFrameFillDrawMessage && !handled && (RouteWEnabled() || Ctl().shapesAlone) && Ctl().shapes &&
        ise_page_stroke::DrawFill(ded->gd, ded->changedBy, PageStrokeCoverage())) {
        Trace("P page-item fill item=%u", uid);
        handled = kTrue;
    }
    if (eventID == kBeginFrameStrokeDrawMessage && !handled && (RouteWEnabled() || Ctl().shapesAlone) && Ctl().shapes &&
        ise_page_stroke::DrawStroke(ded->gd, ded->changedBy, PageStrokeCoverage())) {
        Trace("P page-item stroke item=%u", uid);
        handled = kTrue;
    }
    if (closes) { ResetScreen(); t.End(); FlushTrace(); }
    return handled;
}

/** Provides the draw event handler as a session-wide kDrawEventService. */
class IseXfDrawEventService : public CServiceProvider
{
public:
    explicit IseXfDrawEventService(IPMUnknown* boss) : CServiceProvider(boss) {}
    virtual ServiceID GetServiceID() { return kDrawEventService; }
    virtual bool16 IsDefaultServiceProvider() { return kFalse; }
    virtual InstancePerX GetInstantiationPolicy() { return IK2ServiceProvider::kInstancePerSession; }
    virtual void GetName(PMString* name) { name->SetKey("IseXfDrawEventService"); }
    /** Instantiated on every drawing thread, background PDF export included. Its state is
        per thread (ThreadTracker), so threads never share records. */
    virtual IPlugIn::ThreadingPolicy GetThreadingPolicy() const { return IPlugIn::kMultipleThreads; }
};

CREATE_PMINTERFACE(IseXfDrawEventService, kIseXfDrawEventServiceImpl)
