//========================================================================================
//
//  The StrokeMaster panel's tooltips: each checkbox, and the Restore Defaults button, says
//  what it does when hovered. The text is looked up by the widget's ID in the panel's string
//  table (IseUI.fr), so it is translated with the rest.
//
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "AbstractTip.h"
#include "IControlView.h"

// Project includes:
#include "IseUIID.h"

namespace {

struct Tip { WidgetID widget; const char* key; };

const Tip kTips[] = {
    {kIseUIEnabledWidgetID, kIseUIEnabledTipKey},
    {kIseUITextBehindWidgetID, kIseUITextBehindTipKey},
    {kIseUITextStoryWidgetID, kIseUITextStoryTipKey},
    {kIseUITextStoriesWidgetID, kIseUITextStoriesTipKey},
    {kIseUITextMiterWidgetID, kIseUITextMiterTipKey},
    {kIseUIShapesWidgetID, kIseUIShapesTipKey},
    {kIseUIWholePointsWidgetID, kIseUIWholePointsTipKey},
    {kIseUIPatternPointsWidgetID, kIseUIPatternPointsTipKey},
    {kIseUIGlyphTipsWidgetID, kIseUIGlyphTipsTipKey},
    {kIseUIEffectCornersWidgetID, kIseUIEffectCornersTipKey},
    {kIseUIWordOutlineWidgetID, kIseUIWordOutlineTipKey},
    {kIseUIOverlapsWidgetID, kIseUIOverlapsTipKey},
    {kIseUIInsideWidgetID, kIseUIInsideTipKey},
    {kIseUIOpenPathsWidgetID, kIseUIOpenPathsTipKey},
    {kIseUITransparencyWidgetID, kIseUITransparencyTipKey},
    {kIseUICloseHolesWidgetID, kIseUICloseHolesTipKey},
    {kIseUIShapeMiterWidgetID, kIseUIShapeMiterTipKey},
    {kIseUINoteWidgetID, kIseUINoteTipKey},
    {kIseUIRestoreWidgetID, kIseUIRestoreTipKey},
};

} // namespace

/** A panel widget's tip, by its widget ID. */
class IseUITip : public AbstractTip
{
public:
    explicit IseUITip(IPMUnknown* boss) : AbstractTip(boss) {}
    virtual PMString GetTipText(const PMPoint& mouseLocation);
};

CREATE_PMINTERFACE(IseUITip, kIseUITipImpl)

PMString IseUITip::GetTipText(const PMPoint&)
{
    InterfacePtr<IControlView> view(this, UseDefaultIID());
    if (view == nil) return PMString();
    const WidgetID id = view->GetWidgetID();
    for (const Tip& t : kTips) {
        if (t.widget != id) continue;
        PMString text(t.key);
        text.Translate();
        text.SetTranslatable(kFalse);
        return text;
    }
    return PMString();
}
