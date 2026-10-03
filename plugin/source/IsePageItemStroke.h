//========================================================================================
// Page-item strokes (shapes, frames, outlined text): the miter correction route W gives text,
// for the strokes InDesign draws on page items. See IsePageItemStroke.cpp.
//========================================================================================
#pragma once

class GraphicsData;
class IPMUnknown;

namespace ise_page_stroke {

// At kBeginFrameStrokeDrawMessage: if the item's stroke is one this route can draw exactly,
// draws it and returns true, and the caller reports the event handled so InDesign skips its
// own stroke for the item. Otherwise draws nothing and returns false.
bool DrawStroke(GraphicsData* gd, IPMUnknown* item);

} // namespace ise_page_stroke
