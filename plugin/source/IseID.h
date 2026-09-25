//========================================================================================
//
//  IDs for the InDesign Stroke Engine plugin.
//
//  The plugin adds one global text adornment. In the foreground draw pass, after InDesign
//  has drawn every wax run in a frame, it repaints the fill of each run that carries an
//  Outside stroke, so that no run's stroke covers an earlier run's fill. See
//  docs/INDESIGN_STROKE_INVESTIGATION.md, candidate I1.
//
//  The prefix number below is a development placeholder. Adobe assigns prefix numbers, and
//  a plugin that is distributed must use an assigned one, otherwise its object model IDs can
//  collide with another plugin's.
//
//========================================================================================
#ifndef __IseID_h__
#define __IseID_h__

#include "SDKDef.h"

// Company:
#define kIseCompanyKey      "InDesignStrokeEngine"
#define kIseCompanyValue    "InDesign Stroke Engine"

// Plug-in:
#define kIsePluginName      "InDesignStrokeEngine"
#define kIsePrefixNumber    0x5A200          // Development placeholder; not assigned by Adobe.
#define kIseVersion         kSDKDefPluginVersionString
#define kIseAuthor          "InDesign Stroke Engine investigation"

// Plug-in Prefix: (change kIsePrefixNumber above to modify the prefix.)
#define kIsePrefix          RezLong(kIsePrefixNumber)
#define kIseStringPrefix    SDK_DEF_STRINGIZE(kIsePrefixNumber)

// Missing plug-in: (see the ExtraPluginInfo resource)
#define kIseMissingPluginURLValue     kSDKDefPartnersStandardValue_enUS
#define kIseMissingPluginAlertValue   kSDKDefMissingPluginAlertValue

// PluginID:
DECLARE_PMID(kPlugInIDSpace, kIsePluginID, kIsePrefix + 0)

// ClassIDs:
DECLARE_PMID(kClassIDSpace, kIseFillRepaintAdornmentBoss, kIsePrefix + 0)
// Experimental route W (build-plugin.ps1 -RouteW); declared always, registered only then.
DECLARE_PMID(kClassIDSpace, kIseStrokeBehindAdornmentBoss, kIsePrefix + 1)
DECLARE_PMID(kClassIDSpace, kIseStrokeRestoreAdornmentBoss, kIsePrefix + 2)
// Experimental route X (build-plugin.ps1 -RouteX): route W across the frames of one story.
DECLARE_PMID(kClassIDSpace, kIseXfDrawEventBoss, kIsePrefix + 3)

// InterfaceIDs:
// None: the plugin implements interfaces the API already declares.

// ImplementationIDs:
DECLARE_PMID(kImplementationIDSpace, kIseFillRepaintAdornmentImpl, kIsePrefix + 0)
DECLARE_PMID(kImplementationIDSpace, kIseStrokeBehindAdornmentImpl, kIsePrefix + 1)
DECLARE_PMID(kImplementationIDSpace, kIseStrokeRestoreAdornmentImpl, kIsePrefix + 2)
DECLARE_PMID(kImplementationIDSpace, kIseXfDrawEventHandlerImpl, kIsePrefix + 3)
DECLARE_PMID(kImplementationIDSpace, kIseXfDrawEventServiceImpl, kIsePrefix + 4)

// Format IDs for the PluginVersion resource:
#define kIseInitialMajorFormat      kSDKDef_20_PersistMajorVersionNumber
#define kIseInitialMinorFormat      kSDKDef_20_PersistMinorVersionNumber
#define kIseLastMajorFormatChange   kIseInitialMajorFormat
#define kIseLastMinorFormatChange   kIseInitialMinorFormat

#endif // __IseID_h__
