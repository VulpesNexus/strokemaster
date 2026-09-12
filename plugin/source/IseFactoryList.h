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
