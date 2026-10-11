// The native palette view supplies the dock's resize affordance; keep every switch visible.
#include "VCPlugInHeaders.h"
#include "PalettePanelView.h"
#include "IseUIID.h"

class IseUIPanelView : public PalettePanelView
{
public:
    explicit IseUIPanelView(IPMUnknown* boss) : PalettePanelView(boss) {}
    PMPoint ConstrainDimensions(const PMPoint& desired) const override
    {
        PMPoint size(desired);
        if (size.X() < 270) size.X(270);
        if (size.Y() < 418) size.Y(418);
        return size;
    }
};
CREATE_PERSIST_PMINTERFACE(IseUIPanelView, kIseUIPanelViewImpl)
