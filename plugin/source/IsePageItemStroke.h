//========================================================================================
// Page-item strokes (shapes, frames, outlined text): the miter correction route W gives text,
// for the strokes InDesign draws on page items. See IsePageItemStroke.cpp.
//========================================================================================
#pragma once

class GraphicsData;
class IPMUnknown;

namespace ise_page_stroke {

// What the route may take over, and how: the StrokeMaster switches (IseSettings.h), or in the
// research build its control file (IseCrossFrame.cpp), for interleaved comparisons.
struct Coverage {
    bool transparency = true;    // items with opacity or blending (object, stroke, or fill level); effects on a stroke or fill: never
    bool probeNative = false;    // research: InDesign's own stroke drawn from the event, unclipped
    bool wordOutline = true;     // patterns along one outline per word (Center and Inside also need overlaps)
    bool shapeMiter = true;      // Center's interior miters clipped to the fill (else whole, as InDesign draws them)
    bool wholePoints = true;     // past the limit, a shape's own points kept whole (else beveled as InDesign)
    bool glyphTips = true;       // within the limit, glyph tips trimmed
    bool effectCorners = true;   // corner-effect corners told apart (beveled past the limit, trimmed within)
    bool patternPoints = true;   // whole points added to dashed, dotted, and striped strokes
    bool overlaps = true;        // overlapping contours: Outside and Inside strokes, Center patterns (else InDesign's)
    bool inside = true;          // Inside strokes (else left to InDesign)
    bool openPaths = true;       // open paths (else left to InDesign)
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
