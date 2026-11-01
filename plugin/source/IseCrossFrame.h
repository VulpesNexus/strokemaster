//========================================================================================
// Route X glue: what route W's adornments call to extend their correction across the frames
// of a story. See IseCrossFrameCore.h for the rule and IseCrossFrame.cpp for the host side.
//========================================================================================
#pragma once
#include "IseCrossFrameCore.h"
#include "IseMiterCore.h"
#include "IseRegionCore.h"
#include <cstdint>
#include <vector>

class GraphicsData;
class IDataBase;
class IGraphicsPort;
class IParcelShape;
class IWaxGlyphs;
class IWaxRenderData;
class IWaxRun;
class PMReal;

namespace ise_cross_frame {

Mode CurrentMode();
// Route W's own stroke pass runs (false only in the research build's Native mode).
bool RouteWEnabled();
// Route W's miter correction runs (false only when the research build's control says miter=0).
bool MiterCorrectionEnabled();
// Trim glyph tips on live text: a tip's miter cut flush where route W fills the joins.
bool TextTipsEnabled();
// Close holes: route W fills the silhouette of a run's stroke beneath it (ise_region::Silhouette).
bool CloseHolesEnabled();
// Close holes across frames: the outlines, in `run`'s line coordinates, of glyphs other frames
// painted earlier in this traversal within `box` (line coordinates), retaining each stroke's
// reach, join and limit. Paint and tint must match. `reach` is the current stroke's reach.
// Screen drawing reads earlier visible frames from a
// parcel-scoped layout snapshot, since their item caches may replay without adornment visits.
std::vector<ise_region::StrokeField> NeighborOutlines(GraphicsData* gd, const IWaxRun* run, double x0, double y0, double x1, double y1,
                                                 double reach, std::uint32_t paint);
// Refuse shared paint over unsupported or covered text in the actual output bounds.
bool SharedHoleAreaSafe(GraphicsData* gd, const IWaxRun* run, double x0, double y0, double x1, double y1,
                       double sx0, double sy0, double sx1, double sy1);
// A parcel is about to be drawn in the pass named by tag ('W' stroke pass, 'R' restore pass).
void NoteParcel(const IParcelShape* parcel, char tag, int32 flags);
// Release the screen snapshot's font references when the parcel pass ends.
void EndParcel();
// Stroke pass, every run: remember a fill this parcel is about to paint.
void RecordPendingFill(GraphicsData* gd, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db);
// Restore pass, every run: the run's fill has been painted.
void ConfirmFill(GraphicsData* gd, const IWaxRun* run);
// Stroke pass, eligible run, inside route W's gsave before the stroke: clips out the glyphs
// of earlier-painted fills this stroke must not cover. Returns how many it withheld.
int ClipWithheld(GraphicsData* gd, IGraphicsPort* port, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db);
// Research trace of one adornment visit.
void TraceRun(char tag, GraphicsData* gd, const IWaxRun* run, const IWaxRenderData* data, const IWaxGlyphs* glyphs, IDataBase* db, const char* what);

} // namespace ise_cross_frame
