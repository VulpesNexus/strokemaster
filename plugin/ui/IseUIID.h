//========================================================================================
//
//  IDs for the StrokeMaster panel, the UI plugin beside the engine (InDesignStrokeEngine).
//
//  The engine is a model plugin, so that background PDF export runs it; a model plugin may
//  not carry UI, so the panel is a plugin of its own. The two share nothing but the switches'
//  file (IseSettings.h). The IDs use the engine's development prefix (IseID.h), from offset
//  0x40 on, clear of the engine's own.
//
//========================================================================================
#ifndef __IseUIID_h__
#define __IseUIID_h__

#include "SDKDef.h"

#define kIseUICompanyKey      "StrokeMaster"
#define kIseUICompanyValue    "StrokeMaster"
#define kIseUIPluginName      "StrokeMasterUI"
#define kIseUIPrefixNumber    0x5A200          // The engine's development placeholder; not assigned by Adobe.
#define kIseUIVersion         kSDKDefPluginVersionString
#define kIseUIPrefix          RezLong(kIseUIPrefixNumber)
#define kIseUIStringPrefix    SDK_DEF_STRINGIZE(kIseUIPrefixNumber)

// PluginID:
DECLARE_PMID(kPlugInIDSpace, kIseUIPluginID, kIseUIPrefix + 0x40)
// The engine (plugin/source/IseID.h's kIsePluginID), whose presence the panel reports.
DECLARE_PMID(kPlugInIDSpace, kIseUIEnginePluginID, kIseUIPrefix + 0)

// ClassIDs:
DECLARE_PMID(kClassIDSpace, kIseUIPanelWidgetBoss, kIseUIPrefix + 0x40)
DECLARE_PMID(kClassIDSpace, kIseUIActionComponentBoss, kIseUIPrefix + 0x41)
DECLARE_PMID(kClassIDSpace, kIseUICheckBoxWidgetBoss, kIseUIPrefix + 0x42)   // a checkbox with a tip
DECLARE_PMID(kClassIDSpace, kIseUIButtonWidgetBoss, kIseUIPrefix + 0x43)     // a button with a tip

// ImplementationIDs:
DECLARE_PMID(kImplementationIDSpace, kIseUIPanelObserverImpl, kIseUIPrefix + 0x40)
DECLARE_PMID(kImplementationIDSpace, kIseUIActionComponentImpl, kIseUIPrefix + 0x41)
DECLARE_PMID(kImplementationIDSpace, kIseUITipImpl, kIseUIPrefix + 0x42)
DECLARE_PMID(kImplementationIDSpace, kIseUIPanelViewImpl, kIseUIPrefix + 0x43)

// ActionIDs:
DECLARE_PMID(kActionIDSpace, kIseUIPanelWidgetActionID, kIseUIPrefix + 0x40)
DECLARE_PMID(kActionIDSpace, kIseUIRestoreActionID, kIseUIPrefix + 0x41)
DECLARE_PMID(kActionIDSpace, kIseUISeparatorActionID, kIseUIPrefix + 0x42)
DECLARE_PMID(kActionIDSpace, kIseUIAboutActionID, kIseUIPrefix + 0x43)

// WidgetIDs: the panel, its switches (one per IseSettings.h field), and the button.
DECLARE_PMID(kWidgetIDSpace, kIseUIPanelWidgetID, kIseUIPrefix + 0x40)
DECLARE_PMID(kWidgetIDSpace, kIseUIEnabledWidgetID, kIseUIPrefix + 0x41)
DECLARE_PMID(kWidgetIDSpace, kIseUITextBehindWidgetID, kIseUIPrefix + 0x42)
DECLARE_PMID(kWidgetIDSpace, kIseUITextStoryWidgetID, kIseUIPrefix + 0x43)
DECLARE_PMID(kWidgetIDSpace, kIseUITextStoriesWidgetID, kIseUIPrefix + 0x44)
DECLARE_PMID(kWidgetIDSpace, kIseUITextMiterWidgetID, kIseUIPrefix + 0x45)
DECLARE_PMID(kWidgetIDSpace, kIseUIShapesWidgetID, kIseUIPrefix + 0x46)
DECLARE_PMID(kWidgetIDSpace, kIseUIWholePointsWidgetID, kIseUIPrefix + 0x47)
DECLARE_PMID(kWidgetIDSpace, kIseUIPatternPointsWidgetID, kIseUIPrefix + 0x48)
DECLARE_PMID(kWidgetIDSpace, kIseUIGlyphTipsWidgetID, kIseUIPrefix + 0x49)
DECLARE_PMID(kWidgetIDSpace, kIseUIEffectCornersWidgetID, kIseUIPrefix + 0x4A)
DECLARE_PMID(kWidgetIDSpace, kIseUIWordOutlineWidgetID, kIseUIPrefix + 0x4B)
DECLARE_PMID(kWidgetIDSpace, kIseUIOverlapsWidgetID, kIseUIPrefix + 0x4C)
DECLARE_PMID(kWidgetIDSpace, kIseUIInsideWidgetID, kIseUIPrefix + 0x4D)
DECLARE_PMID(kWidgetIDSpace, kIseUIOpenPathsWidgetID, kIseUIPrefix + 0x4E)
DECLARE_PMID(kWidgetIDSpace, kIseUITransparencyWidgetID, kIseUIPrefix + 0x4F)
DECLARE_PMID(kWidgetIDSpace, kIseUIRestoreWidgetID, kIseUIPrefix + 0x50)
DECLARE_PMID(kWidgetIDSpace, kIseUITextHeadWidgetID, kIseUIPrefix + 0x51)
DECLARE_PMID(kWidgetIDSpace, kIseUIShapesHeadWidgetID, kIseUIPrefix + 0x52)
DECLARE_PMID(kWidgetIDSpace, kIseUINoteWidgetID, kIseUIPrefix + 0x53)
DECLARE_PMID(kWidgetIDSpace, kIseUIMasterSeparatorWidgetID, kIseUIPrefix + 0x54)
DECLARE_PMID(kWidgetIDSpace, kIseUIShapesSeparatorWidgetID, kIseUIPrefix + 0x55)
DECLARE_PMID(kWidgetIDSpace, kIseUIFooterSeparatorWidgetID, kIseUIPrefix + 0x56)
DECLARE_PMID(kWidgetIDSpace, kIseUICornersSeparatorWidgetID, kIseUIPrefix + 0x57)
DECLARE_PMID(kWidgetIDSpace, kIseUICornersHeadWidgetID, kIseUIPrefix + 0x58)
DECLARE_PMID(kWidgetIDSpace, kIseUIContoursSeparatorWidgetID, kIseUIPrefix + 0x59)
DECLARE_PMID(kWidgetIDSpace, kIseUIContoursHeadWidgetID, kIseUIPrefix + 0x5A)
DECLARE_PMID(kWidgetIDSpace, kIseUICloseHolesWidgetID, kIseUIPrefix + 0x5B)
DECLARE_PMID(kWidgetIDSpace, kIseUIShapeMiterWidgetID, kIseUIPrefix + 0x5C)

// Strings (StringTable in IseUI.fr):
#define kIseUIPanelTitleKey          kIseUIStringPrefix "kIseUIPanelTitleKey"
#define kIseUIInternalMenuNameKey    kIseUIStringPrefix "kIseUIInternalMenuNameKey"
#define kIseUIRestoreMenuKey         kIseUIStringPrefix "kIseUIRestoreMenuKey"
#define kIseUIEnabledKey             kIseUIStringPrefix "kIseUIEnabledKey"
#define kIseUITextHeadKey            kIseUIStringPrefix "kIseUITextHeadKey"
#define kIseUITextBehindKey          kIseUIStringPrefix "kIseUITextBehindKey"
#define kIseUITextStoryKey           kIseUIStringPrefix "kIseUITextStoryKey"
#define kIseUITextStoriesKey         kIseUIStringPrefix "kIseUITextStoriesKey"
#define kIseUITextMiterKey           kIseUIStringPrefix "kIseUITextMiterKey"
#define kIseUIShapesHeadKey          kIseUIStringPrefix "kIseUIShapesHeadKey"
#define kIseUIShapesKey              kIseUIStringPrefix "kIseUIShapesKey"
#define kIseUICornersHeadKey         kIseUIStringPrefix "kIseUICornersHeadKey"
#define kIseUIContoursHeadKey        kIseUIStringPrefix "kIseUIContoursHeadKey"
#define kIseUIWholePointsKey         kIseUIStringPrefix "kIseUIWholePointsKey"
#define kIseUIPatternPointsKey       kIseUIStringPrefix "kIseUIPatternPointsKey"
#define kIseUIGlyphTipsKey           kIseUIStringPrefix "kIseUIGlyphTipsKey"
#define kIseUIEffectCornersKey       kIseUIStringPrefix "kIseUIEffectCornersKey"
#define kIseUIWordOutlineKey         kIseUIStringPrefix "kIseUIWordOutlineKey"
#define kIseUIOverlapsKey            kIseUIStringPrefix "kIseUIOverlapsKey"
#define kIseUIInsideKey              kIseUIStringPrefix "kIseUIInsideKey"
#define kIseUIOpenPathsKey           kIseUIStringPrefix "kIseUIOpenPathsKey"
#define kIseUITransparencyKey        kIseUIStringPrefix "kIseUITransparencyKey"
#define kIseUICloseHolesKey          kIseUIStringPrefix "kIseUICloseHolesKey"
#define kIseUIShapeMiterKey          kIseUIStringPrefix "kIseUIShapeMiterKey"
#define kIseUIRestoreKey             kIseUIStringPrefix "kIseUIRestoreKey"
#define kIseUIEngineMissingKey       kIseUIStringPrefix "kIseUIEngineMissingKey"
#define kIseUIAboutMenuKey           kIseUIStringPrefix "kIseUIAboutMenuKey"

// Tips (IseUITip.cpp), one per checkbox and the button:
#define kIseUIEnabledTipKey            kIseUIStringPrefix "kIseUIEnabledTipKey"
#define kIseUITextBehindTipKey         kIseUIStringPrefix "kIseUITextBehindTipKey"
#define kIseUITextStoryTipKey          kIseUIStringPrefix "kIseUITextStoryTipKey"
#define kIseUITextStoriesTipKey        kIseUIStringPrefix "kIseUITextStoriesTipKey"
#define kIseUITextMiterTipKey          kIseUIStringPrefix "kIseUITextMiterTipKey"
#define kIseUIShapesTipKey             kIseUIStringPrefix "kIseUIShapesTipKey"
#define kIseUIWholePointsTipKey        kIseUIStringPrefix "kIseUIWholePointsTipKey"
#define kIseUIPatternPointsTipKey      kIseUIStringPrefix "kIseUIPatternPointsTipKey"
#define kIseUIGlyphTipsTipKey          kIseUIStringPrefix "kIseUIGlyphTipsTipKey"
#define kIseUIEffectCornersTipKey      kIseUIStringPrefix "kIseUIEffectCornersTipKey"
#define kIseUIWordOutlineTipKey        kIseUIStringPrefix "kIseUIWordOutlineTipKey"
#define kIseUIOverlapsTipKey           kIseUIStringPrefix "kIseUIOverlapsTipKey"
#define kIseUIInsideTipKey             kIseUIStringPrefix "kIseUIInsideTipKey"
#define kIseUIOpenPathsTipKey          kIseUIStringPrefix "kIseUIOpenPathsTipKey"
#define kIseUITransparencyTipKey       kIseUIStringPrefix "kIseUITransparencyTipKey"
#define kIseUICloseHolesTipKey         kIseUIStringPrefix "kIseUICloseHolesTipKey"
#define kIseUIShapeMiterTipKey         kIseUIStringPrefix "kIseUIShapeMiterTipKey"
#define kIseUINoteTipKey               kIseUIStringPrefix "kIseUINoteTipKey"
#define kIseUIRestoreTipKey            kIseUIStringPrefix "kIseUIRestoreTipKey"

// The panel's icon (plugin/ui/icons, from tools/make_panel_icons.py): 23 px, its 2x copy
// (IconPNGDefs.h's kHIDPIIconOffset), and the vector.
#define kIseUIPanelIconRsrcID  1000

#define kIseUIRestoreMenuItemPosition    1.0
#define kIseUISeparatorMenuItemPosition  2.0
#define kIseUIAboutMenuItemPosition      3.0

// Format IDs for the PluginVersion resource:
#define kIseUIMajorFormat  kSDKDef_30_PersistMajorVersionNumber
#define kIseUIMinorFormat  kSDKDef_30_PersistMinorVersionNumber

#endif // __IseUIID_h__
