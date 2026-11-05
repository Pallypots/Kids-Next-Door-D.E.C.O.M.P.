// Vftable slots of the JAM header classes that are NOT recovered yet. Each fails with a clear diagnostic. This file is
// never recorded in config/functions.toml and never counts as recovery. A method moves out of here when it is recovered.
#include "at/AtIoJam.h"

#define AT_NOT_RECOVERED(what) AtFatal("not recovered yet: " what "\n", 0, 0, 0)

void AtIoFastAlignedJamHeader::List(void*, void*, void*) { AT_NOT_RECOVERED("AtIoFastAlignedJamHeader::List (0x0007F9E0)"); }
void AtIoFastJamHeader::List(void*, void*, void*) { AT_NOT_RECOVERED("AtIoFastJamHeader::List (0x00080710)"); }
