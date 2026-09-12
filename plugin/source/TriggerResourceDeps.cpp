//========================================================================================
//
//  Gives the resource step something to depend on.
//
//  The project makes the ODFRC custom build step depend on this file's object, which is how
//  the SDK's sample projects make resources rebuild when the plugin's IDs change. It compiles
//  the ID header and nothing else.
//
//========================================================================================
#include "VCPlugInHeaders.h"
#include "IseID.h"

// Nothing to run: including IseID.h above is the whole point of this file.
