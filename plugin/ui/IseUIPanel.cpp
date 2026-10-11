//========================================================================================
//
//  The StrokeMaster panel (Window > StrokeMaster): one checkbox per switch of the engine
//  (IseSettings.h). A click writes the switches' file and redraws every open document; the
//  engine reads the file again at the start of the next spread it draws. A switch whose
//  parent is off is shown disabled, its own state kept. Restore Defaults (the button, or the
//  panel menu) puts every switch back. Without the engine installed, every switch is shown
//  disabled and the note says so. The panel menu also opens the About window
//  (StrokeMasterAbout.cpp), in InDesign's interface colors.
//
//========================================================================================
#include "VCPlugInHeaders.h"

// Interface includes:
#include "IApplication.h"
#include "IBooleanControlData.h"
#include "IControlView.h"
#include "IDocument.h"
#include "IDocumentList.h"
#include "IInterfaceColors.h"
#include "ILayoutUIUtils.h"
#include "IPanelControlData.h"
#include "IPanelMgr.h"
#include "IPlugInList.h"
#include "ISession.h"
#include "ISubject.h"
#include "ITextControlData.h"
#include "ITriStateControlData.h"

// General includes:
#include "CActionComponent.h"
#include "CObserver.h"
#include "Utils.h"

// Project includes:
#include "IseUIID.h"
#include "IseSettings.h"
#include "StrokeMasterAbout.h"

namespace {

using ise_settings::Settings;

/** Each checkbox, the switch it shows, and the switch that must be on for it to apply. */
struct Box {
    WidgetID widget;
    bool Settings::*field;
    bool Settings::*parent;    // nullptr: only the master switch
    bool Settings::*parent2;   // a second parent, or nullptr
};

const Box kBoxes[] = {
    {kIseUITextBehindWidgetID, &Settings::textBehind, nullptr, nullptr},
    {kIseUITextStoryWidgetID, &Settings::textStory, &Settings::textBehind, nullptr},
    {kIseUITextStoriesWidgetID, &Settings::textStories, &Settings::textBehind, &Settings::textStory},
    {kIseUITextMiterWidgetID, &Settings::textMiter, &Settings::textBehind, nullptr},
    {kIseUIShapesWidgetID, &Settings::shapes, nullptr, nullptr},
    {kIseUIWholePointsWidgetID, &Settings::wholePoints, &Settings::shapes, nullptr},
    {kIseUIPatternPointsWidgetID, &Settings::patternPoints, &Settings::shapes, &Settings::wholePoints},
    {kIseUIGlyphTipsWidgetID, &Settings::glyphTips, &Settings::shapes, nullptr},
    {kIseUIEffectCornersWidgetID, &Settings::effectCorners, &Settings::shapes, nullptr},
    {kIseUIWordOutlineWidgetID, &Settings::wordOutline, &Settings::shapes, nullptr},
    {kIseUIOverlapsWidgetID, &Settings::overlaps, &Settings::shapes, nullptr},
    {kIseUIInsideWidgetID, &Settings::inside, &Settings::shapes, nullptr},
    {kIseUIOpenPathsWidgetID, &Settings::openPaths, &Settings::shapes, nullptr},
    {kIseUITransparencyWidgetID, &Settings::transparency, &Settings::shapes, nullptr},
};

void SetChecked(IPanelControlData* panel, const WidgetID& id, bool on, bool enabled)
{
    IControlView* view = panel->FindWidget(id);
    if (view == nil) return;
    InterfacePtr<ITriStateControlData> state(view, UseDefaultIID());
    if (state != nil) {
        if (on && !state->IsSelected()) state->Select(kTrue, kFalse);
        if (!on && state->IsSelected()) state->Deselect(kTrue, kFalse);
    }
    if (enabled) view->Enable(); else view->Disable();
}

/** Whether the engine (InDesignStrokeEngine.pln) is installed and did not fail to load: the
    switches do nothing without it. */
bool EngineInstalled()
{
    InterfacePtr<IPlugInList> plugins(GetExecutionContextSession(), UseDefaultIID());
    return plugins == nil || (!plugins->GetPluginName(kIseUIEnginePluginID).IsEmpty() && !plugins->LoadFailed(kIseUIEnginePluginID));
}

/** Shows the switches on the panel's checkboxes, without notifying anyone; all of them
    disabled, and the note saying why, when the engine is missing. */
void ShowSettings(IPanelControlData* panel, const Settings& s)
{
    const bool engine = EngineInstalled();
    SetChecked(panel, kIseUIEnabledWidgetID, s.enabled, engine);
    for (const Box& b : kBoxes) {
        const bool enabled = engine && s.enabled && (b.parent == nullptr || s.*(b.parent)) && (b.parent2 == nullptr || s.*(b.parent2));
        SetChecked(panel, b.widget, s.*(b.field), enabled);
    }
    IControlView* note = panel->FindWidget(kIseUINoteWidgetID);
    InterfacePtr<ITextControlData> text(note, UseDefaultIID());
    if (text != nil) {
        PMString line(engine ? kIseUINoteKey : kIseUIEngineMissingKey);
        line.Translate();
        text->SetString(line);
    }
}

/** One of InDesign's interface colors (InterfaceColorDefines.h), or `fallback`. */
COLORREF InterfaceColor(IInterfaceColors* colors, int32 id, COLORREF fallback)
{
    RealAGMColor c;
    if (colors == nil || !colors->GetRealAGMColor(id, c)) return fallback;
    auto byte = [](PMReal v) { return static_cast<BYTE>(ToInt32(Round(v * 255))); };
    return RGB(byte(c.red), byte(c.green), byte(c.blue));
}

/** The About window, in InDesign's own interface colors, centered on InDesign's window. */
void ShowAbout()
{
    InterfacePtr<IInterfaceColors> colors(static_cast<IInterfaceColors*>(GetExecutionContextSession()->QueryInterface(IID_IINTERFACECOLORS)));
    StrokeMasterAboutTheme t;
    t.panel = InterfaceColor(colors, kInterfacePaletteFill, t.panel);
    t.panelText = InterfaceColor(colors, kInterfaceTextColor, t.panelText);
    t.band = t.panel;
    t.bandText = t.panelText;
    t.rule = InterfaceColor(colors, kInterfaceSeparatorColor, t.rule);
    t.link = InterfaceColor(colors, kInterfaceBrightnessAwareHyperlinkColor, t.link);
    t.ownerDrawButton = true;   // InDesign's own buttons are drawn, in either brightness
    t.button = InterfaceColor(colors, kInterfaceButtonUpFill, t.button);
    t.buttonText = t.panelText;
    t.buttonBorder = InterfaceColor(colors, kInterfaceBorder, t.buttonBorder);
    t.darkTitleBar = GetRValue(t.panel) * 299 + GetGValue(t.panel) * 587 + GetBValue(t.panel) * 114 < 128 * 1000;
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ShowAbout), &module);
    HWND active = ::GetActiveWindow();
    HWND owner = active != nullptr ? ::GetAncestor(active, GA_ROOTOWNER) : nullptr;
    StrokeMasterShowAboutDialog(module, owner, t);
}

/** Every open document is drawn again, with the switches just written. A layout view keeps
    each page item's rendering and repaints from it (zooms and page turns included), so the
    items' cached renderings go first; invalidating the views alone shows the old strokes. */
void Redraw()
{
    InterfacePtr<IApplication> app(GetExecutionContextSession()->QueryApplication());
    InterfacePtr<IDocumentList> docs(app ? app->QueryDocumentList() : nil);
    if (docs != nil) {
        for (int32 i = 0; i < docs->GetDocCount(); i++) {
            IDocument* doc = docs->GetNthDoc(i);
            if (doc != nil) Utils<ILayoutUIUtils>()->InvalidateShapeGraphixCache(doc);
        }
    }
    Utils<ILayoutUIUtils>()->InvalidateAllDocumentViews();
}

} // namespace

/** Keeps the checkboxes and the switches' file in step. */
class IseUIPanelObserver : public CObserver
{
public:
    explicit IseUIPanelObserver(IPMUnknown* boss) : CObserver(boss) {}
    virtual ~IseUIPanelObserver() {}
    virtual void AutoAttach();
    virtual void AutoDetach();
    virtual void Update(const ClassID& theChange, ISubject* theSubject, const PMIID& protocol, void* changedBy);

private:
    void Attach(IPanelControlData* panel, const WidgetID& id, const PMIID& protocol, bool attach);
    void AttachAll(bool attach);
};

CREATE_PMINTERFACE(IseUIPanelObserver, kIseUIPanelObserverImpl)

void IseUIPanelObserver::Attach(IPanelControlData* panel, const WidgetID& id, const PMIID& protocol, bool attach)
{
    IControlView* view = panel->FindWidget(id);
    if (view == nil) return;
    InterfacePtr<ISubject> subject(view, UseDefaultIID());
    if (subject == nil) return;
    if (attach) {
        if (!subject->IsAttached(this, protocol)) subject->AttachObserver(this, protocol);
    } else if (subject->IsAttached(this, protocol)) {
        subject->DetachObserver(this, protocol);
    }
}

void IseUIPanelObserver::AttachAll(bool attach)
{
    InterfacePtr<IPanelControlData> panel(this, UseDefaultIID());
    if (panel == nil) return;
    Attach(panel, kIseUIEnabledWidgetID, IID_ITRISTATECONTROLDATA, attach);
    for (const Box& b : kBoxes) Attach(panel, b.widget, IID_ITRISTATECONTROLDATA, attach);
    Attach(panel, kIseUIRestoreWidgetID, IID_IBOOLEANCONTROLDATA, attach);
}

void IseUIPanelObserver::AutoAttach()
{
    InterfacePtr<IPanelControlData> panel(this, UseDefaultIID());
    if (panel == nil) return;
    Settings s;
    ise_settings::Load(&s);
    ShowSettings(panel, s);
    AttachAll(true);
}

void IseUIPanelObserver::AutoDetach()
{
    AttachAll(false);
}

void IseUIPanelObserver::Update(const ClassID& theChange, ISubject* theSubject, const PMIID& protocol, void*)
{
    InterfacePtr<IControlView> view(theSubject, UseDefaultIID());
    InterfacePtr<IPanelControlData> panel(this, UseDefaultIID());
    if (view == nil || panel == nil) return;
    const WidgetID id = view->GetWidgetID();
    Settings s;
    ise_settings::Load(&s);
    if (protocol == IID_IBOOLEANCONTROLDATA) {
        if (id != kIseUIRestoreWidgetID || theChange != kTrueStateMessage) return;
        s = Settings();
    } else {
        if (theChange != kTrueStateMessage && theChange != kFalseStateMessage) return;
        const bool on = theChange == kTrueStateMessage;
        if (id == kIseUIEnabledWidgetID) {
            s.enabled = on;
        } else {
            bool found = false;
            for (const Box& b : kBoxes)
                if (b.widget == id) { s.*(b.field) = on; found = true; }
            if (!found) return;
        }
    }
    ise_settings::Save(s);
    ShowSettings(panel, s);
    Redraw();
}

/** The panel menu's Restore Defaults. */
class IseUIActionComponent : public CActionComponent
{
public:
    explicit IseUIActionComponent(IPMUnknown* boss) : CActionComponent(boss) {}
    virtual void DoAction(IActiveContext* ac, ActionID actionID, GSysPoint mousePoint = kInvalidMousePoint, IPMUnknown* widget = nil);
};

CREATE_PMINTERFACE(IseUIActionComponent, kIseUIActionComponentImpl)

void IseUIActionComponent::DoAction(IActiveContext*, ActionID actionID, GSysPoint, IPMUnknown*)
{
    if (actionID == kIseUIAboutActionID) { ShowAbout(); return; }
    if (actionID != kIseUIRestoreActionID) return;
    const Settings s;
    ise_settings::Save(s);
    InterfacePtr<IApplication> app(GetExecutionContextSession()->QueryApplication());
    InterfacePtr<IPanelMgr> panels(app ? app->QueryPanelManager() : nil);
    IControlView* view = panels ? panels->GetPanelFromWidgetID(kIseUIPanelWidgetID) : nil;
    InterfacePtr<IPanelControlData> panel(view, UseDefaultIID());
    if (panel != nil) ShowSettings(panel, s);
    Redraw();
}
