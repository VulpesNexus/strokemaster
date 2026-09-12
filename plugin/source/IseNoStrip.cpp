//========================================================================================
//
//  Keeps the linker from stripping implementations that nothing in this plugin calls
//  directly. InDesign instantiates them by implementation ID through the class table in
//  Ise.fr, so from the linker's point of view they are unreferenced.
//
//  The SDK's shared plugin statics call DontDeadStrip from DllMain, so every plugin has to
//  define it. Including the factory list inside a branch that never runs is enough to make
//  each implementation reachable.
//
//========================================================================================
#include "VCPlugInHeaders.h"
#include "InterfaceFactory.h"
#include "IseID.h"

extern bool16 gFalse;

/** References every implementation in this plugin so none of them is dead-stripped. */
void DontDeadStrip();

void DontDeadStrip()
{
    if (gFalse)
    {
#include "IseFactoryList.h"
    }
}
