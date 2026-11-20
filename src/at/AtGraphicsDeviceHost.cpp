// What the graphics device's members need and this work does not recover: the texture work's three classes, and the destructors that release graphics resources. Each stops with a diagnostic.
// In a file of its own, so that a test can supply them instead, and so that the texture work's classes replace the first six by deleting them. Not recovered code.
#include "at/AtGraphicsDevice.h"

namespace {

void NotRecovered(const char* message) {
    AtFatal(message, 0, 0, 0);
}

}  // namespace


AtGfxBufferSet::~AtGfxBufferSet() { NotRecovered("Atlas Graphics: the buffer set's destructor, 0x000FA490, is not recovered"); }
AtGfxResourceXbox::~AtGfxResourceXbox() { NotRecovered("Atlas Graphics: the resource's destructor, 0x0011B040, is not recovered"); }
