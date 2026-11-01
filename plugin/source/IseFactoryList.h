//========================================================================================
//
//  Factory list for the InDesign Stroke Engine plugin.
//
//  The SDK's shared plugin entry point includes this file once per implementation macro it
//  defines, which is how each implementation ID is bound to the class that provides it.
//  Every implementation declared in IseID.h needs one line here.
//
//========================================================================================
// REGISTER_PMINTERFACE(ClassName, ImplementationID)
REGISTER_PMINTERFACE(IseFillRepaintAdornment, kIseFillRepaintAdornmentImpl)
#ifdef ISE_ROUTE_W
REGISTER_PMINTERFACE(IseStrokeBehindAdornment, kIseStrokeBehindAdornmentImpl)
REGISTER_PMINTERFACE(IseStrokeRestoreAdornment, kIseStrokeRestoreAdornmentImpl)
#endif
#ifdef ISE_ROUTE_X
REGISTER_PMINTERFACE(IseXfDrawEvents, kIseXfDrawEventHandlerImpl)
REGISTER_PMINTERFACE(IseXfDrawEventService, kIseXfDrawEventServiceImpl)
REGISTER_PMINTERFACE(IseScreenObserver, kIseScreenObserverImpl)
REGISTER_PMINTERFACE(IseScreenDocResponder, kIseScreenDocResponderImpl)
REGISTER_PMINTERFACE(IseScreenDocService, kIseScreenDocServiceImpl)
REGISTER_PMINTERFACE(IseScreenIdle, kIseScreenIdleImpl)
#endif
