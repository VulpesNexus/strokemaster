//========================================================================================
//  Keeps the StrokeMaster panel's implementations from being dead-stripped.
//========================================================================================
#include "VCPlugInHeaders.h"
#include "InterfaceFactory.h"
#include "IseUIID.h"

extern bool16 gFalse;

void DontDeadStrip();

void DontDeadStrip()
{
    if (gFalse)
    {
#include "IseUIFactoryList.h"
    }
}
