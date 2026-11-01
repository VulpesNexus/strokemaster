// The native palette view supplies the dock's resize affordance; keep every switch visible.
#include "VCPlugInHeaders.h"
#include "PalettePanelView.h"
#include "IseUIID.h"
#include "DVTriStateControlEH.h"
#include "DVStdButtonEH.h"
#include "IControlView.h"
#include "IPanelControlData.h"
#include "IWidgetParent.h"
#include "IKeyBoard.h"
#include "IEvent.h"
#include "KeyboardDefs.h"

namespace {
bool16 MovePanelFocus(IPMUnknown* control, bool backwards)
{
    InterfacePtr<IWidgetParent> parent(control, UseDefaultIID());
    InterfacePtr<IPanelControlData> panel(parent ? static_cast<IPanelControlData*>(parent->QueryParentFor(IID_IPANELCONTROLDATA)) : nil);
    InterfacePtr<IControlView> current(control, UseDefaultIID());
    InterfacePtr<IControlView> panelView(panel, UseDefaultIID());
    InterfacePtr<IKeyBoard> keyboard(GetExecutionContextSession(), UseDefaultIID());
    if (!panel || !current || !panelView || !keyboard || panelView->GetWidgetID() != kIseUIPanelWidgetID) return kFalse;
    const int32 count = panel->Length(), start = panel->GetIndex(current);
    if (start < 0 || count <= 0) return kFalse;
    for (int32 step = 1; step <= count; ++step) {
        IControlView* next = panel->GetWidget((start + (backwards ? -step : step) + count) % count);
        if (!next || !next->IsEnabled() || !next->IsVisible()) continue;
        InterfacePtr<IEventHandler> handler(next, UseDefaultIID());
        if (handler && handler->CanHaveKeyFocus() && handler->WantsTabKeyFocus()) {
            panel->SetKeyboardFocus(next);
            return keyboard->GetKeyFocus() == handler.get();
        }
    }
    return kFalse;
}

// Native control behavior still handles mouse activation and Space. Only focus admission
// and Tab traversal change, scoped to StrokeMaster's own controls, with no global key hook.
template<class Base> class PanelKeyHandler : public Base {
public:
    explicit PanelKeyHandler(IPMUnknown* boss) : Base(boss) {}
    bool16 CanHaveKeyFocus() const override { return kTrue; }
    bool16 WantsTabKeyFocus() const override { return kTrue; }
    bool16 LButtonDn(IEvent* event) override {
        const bool16 handled = Base::LButtonDn(event);
        InterfacePtr<IControlView> view(this, UseDefaultIID());
        InterfacePtr<IWidgetParent> parent(this, UseDefaultIID());
        InterfacePtr<IPanelControlData> panel(parent ? static_cast<IPanelControlData*>(parent->QueryParentFor(IID_IPANELCONTROLDATA)) : nil);
        if (view && view->IsEnabled() && panel) panel->SetKeyboardFocus(view);
        return handled;
    }
    bool16 KeyDown(IEvent* event) override {
        if (event && event->GetVirtualKey() == kVirtualEscapeKey) {
            InterfacePtr<IKeyBoard> keyboard(GetExecutionContextSession(), UseDefaultIID());
            return keyboard ? keyboard->RelinquishKeyFocus() : kFalse;
        }
        if (event && event->GetVirtualKey() == kVirtualTabKey && !event->CmdKeyDown() && !event->OptionAltKeyDown())
            return MovePanelFocus(this, event->ShiftKeyDown());
        return Base::KeyDown(event);
    }
};
}

class IseUICheckKey : public PanelKeyHandler<DVTriStateControlEH> {
public: explicit IseUICheckKey(IPMUnknown* boss) : PanelKeyHandler(boss) {}
};
class IseUIButtonKey : public PanelKeyHandler<DVStdButtonEH> {
public: explicit IseUIButtonKey(IPMUnknown* boss) : PanelKeyHandler(boss) {}
};
CREATE_PMINTERFACE(IseUICheckKey, kIseUICheckKeyImpl)
CREATE_PMINTERFACE(IseUIButtonKey, kIseUIButtonKeyImpl)

class IseUIPanelView : public PalettePanelView
{
public:
    explicit IseUIPanelView(IPMUnknown* boss) : PalettePanelView(boss) {}
    PMPoint ConstrainDimensions(const PMPoint& desired) const override
    {
        PMPoint size(desired);
        if (size.X() < 270) size.X(270);
        if (size.Y() < 520) size.Y(520);
        return size;
    }
};
CREATE_PERSIST_PMINTERFACE(IseUIPanelView, kIseUIPanelViewImpl)
