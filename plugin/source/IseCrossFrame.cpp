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
//                   suppress_shape=<uid>, suppress_text=<uid>, defer_probe=0|1
//   ISE_XF_TRACE    the trace file, appended to
// Without the control file the research build behaves as route W. The production candidate
// has neither: its mode is Story.
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "IDataBase.h"
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
#include "IPDFLibraryUtilsPublic.h"
#include "IViewPortAttributes.h"
#endif

// General includes:
#include "AutoGSave.h"
#include "CPMUnknown.h"
#include "CServiceProvider.h"
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

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace ise_cross_frame {
namespace {

// ---------------------------------------------------------------- control and trace

struct Control {
    Mode mode = Mode::RouteW;
    bool trace = false;
    std::uint32_t suppressShape = 0, suppressText = 0;
    bool deferProbe = false;
};

#ifdef ISE_ROUTE_X_RESEARCH
// Each drawing thread reads the control file for itself; the trace file is shared, guarded.
Control& Ctl() { thread_local Control c; return c; }

thread_local FILETIME gControlStamp = {0, 0};
thread_local bool gControlSeen = false;
FILE* gTrace = nullptr;
std::mutex gTraceLock;

void ReloadControl()
{
    const wchar_t* path = _wgetenv(L"ISE_XF_CONTROL");
    if (path == nullptr || *path == 0) return;
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
Control& Ctl() { static Control c = [] { Control x; x.mode = Mode::Story; return x; }(); return c; }
void ReloadControl() {}
bool Tracing() { return false; }
void Trace(const char*, ...) {}
void FlushTrace() {}
#endif

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

void NoteParcel(const IParcelShape* parcelShape, char tag, int32 flags)
{
    if (!CrossFrameMode() && !Tracing()) return;
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

void RecordPendingFill(GraphicsData* gd, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db)
{
    if (!CrossFrameMode() || gd == nil || run == nil || data == nil || glyphs == nil || db == nil) return;
    Tracker& t = ThreadTracker();
    if (!t.Active() || !SameParcel(run) || gParcel.transparent) return;
    if (Ctl().mode == Mode::Story && gParcel.parcels < 2) return;   // a one-parcel story has no other frame
    const UID none = Utils<ISwatchUtils>()->GetNoneSwatchUID(db);
    const IWaxRenderData::RenderMethod method = data->GetRenderMethod(db, none);
    if (method != IWaxRenderData::kFillOnly && method != IWaxRenderData::kFillStroke) return;
    if (data->GetColorUID(kFalse) == none || data->GetOverprint(kFalse)) return;
    const PMMatrix toPb = run->GetWaxLine()->GetToPasteboardMatrix();
    FillRecord rec;
    rec.run = run;
    rec.port = gd->GetGraphicsPort();
    rec.story = gParcel.story;
    rec.frame = gParcel.frame;
    rec.parcelKey = KeyValue(gParcel.key);
    if (glyphs->HasGlyphMatrix()) {
        // Rotated, scaled, or pathed glyphs: build the outlines now.
        OutlineCollector collect(&toPb);
        if (!CollectOutlines(run, data, glyphs, &collect)) return;
        rec.glyphs = std::move(collect.Glyphs());
        for (const auto& g : rec.glyphs) rec.box.Add(g.box);
    } else {
        const int32 count = glyphs->GetGlyphCount();
        const Text::GlyphID* ids = glyphs->PeekGlyphArray();
        if (count <= 0 || ids == nil) return;
        std::shared_ptr<RunSource> src = std::make_shared<RunSource>();
        IWaxRenderData* mutableData = const_cast<IWaxRenderData*>(data);
        src->font.reset(mutableData->QueryFont());
        if (src->font == nil) return;
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
    const Box b = rec.box;
    const bool lazy = !rec.resolved;
    const bool kept = t.Pending(std::move(rec));
    Trace("F pending run=%p kept=%d lazy=%d frame=%u box=%.2f,%.2f,%.2f,%.2f", run, kept ? 1 : 0,
          lazy ? 1 : 0, gParcel.frame, b.x0, b.y0, b.x1, b.y1);
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
    Tracker& t = ThreadTracker();
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

namespace {

using namespace ise_cross_frame;

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
    if (parcelShape != nil && parcel != nil) {
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
    }
    virtual void UnRegister(IDrwEvtDispatcher* d)
    {
        for (const auto& e : kEvents) d->UnRegisterHandler(e.id, this);
    }
    virtual bool16 HandleDrawEvent(ClassID eventID, void* eventData);
};

CREATE_PMINTERFACE(IseXfDrawEvents, kIseXfDrawEventHandlerImpl)

bool16 IseXfDrawEvents::HandleDrawEvent(ClassID eventID, void* eventData)
{
    DrawEventData* ded = static_cast<DrawEventData*>(eventData);
    if (ded == nullptr) return kFalse;
    Tracker& t = ThreadTracker();
    const std::uint32_t uid = ded->changedBy ? ::GetUID(ded->changedBy).Get() : 0;
    const void* port = ded->gd ? ded->gd->GetGraphicsPort() : nullptr;
    const bool opens = eventID == kBeginSpreadMessage || eventID == kBeforePartialSpreadDrawMessage;
    const bool closes = eventID == kEndSpreadMessage || eventID == kAfterPartialSpreadDrawMessage;
    if (opens) { ReloadControl(); t.Begin(); }
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
    if (closes) { t.End(); FlushTrace(); }
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
