//========================================================================================
// Route X glue: what route W's adornments call to extend their correction across the frames
// of a story. See IseCrossFrameCore.h for the rule and IseCrossFrame.cpp for the host side.
//========================================================================================
#pragma once
#include "IseCrossFrameCore.h"

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
// A parcel is about to be drawn in the pass named by tag ('W' stroke pass, 'R' restore pass).
void NoteParcel(const IParcelShape* parcel, char tag, int32 flags);
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
