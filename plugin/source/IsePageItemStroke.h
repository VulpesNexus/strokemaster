//========================================================================================
// Page-item strokes (shapes, frames, outlined text): the miter correction route W gives text,
// for the strokes InDesign draws on page items. See IsePageItemStroke.cpp.
//========================================================================================
#pragma once

class GraphicsData;
class IPMUnknown;

namespace ise_page_stroke {

// What the route may take over; the production build uses the defaults. The research build
// reads them from its control file (IseCrossFrame.cpp), for interleaved comparisons.
struct Coverage {
    bool transparency = true;    // items with object-level transparency (stroke or fill level: never)
    bool probeNative = false;    // research: InDesign's own stroke drawn from the event, unclipped
    void (*trace)(const char* fmt, ...) = nullptr;   // research: one line per transparency attribute read
};

// At kBeginFrameStrokeDrawMessage: if the item's stroke is one this route can draw exactly,
// draws it and returns true, and the caller reports the event handled so InDesign skips its
// own stroke for the item. Otherwise draws nothing and returns false.
bool DrawStroke(GraphicsData* gd, IPMUnknown* item, const Coverage& coverage = Coverage());

// At kBeginFrameFillDrawMessage: for an item whose Inside stroke DrawStroke will draw, fills the
// item's outline instead of InDesign's inward offset and returns true, and the caller reports
// the event handled so InDesign skips its own fill. Otherwise draws nothing and returns false.
bool DrawFill(GraphicsData* gd, IPMUnknown* item, const Coverage& coverage = Coverage());

} // namespace ise_page_stroke
