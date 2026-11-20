// What the graphics device's members need and this work does not recover: the texture work's three classes, and the destructors that release graphics resources. Each stops with a diagnostic.
// In a file of its own, so that a test can supply them instead, and so that the texture work's classes replace the first six by deleting them. Not recovered code.
#include "at/AtGraphicsDevice.h"

namespace {

void NotRecovered(const char* message) {
    AtFatal(message, 0, 0, 0);
}

}  // namespace

void AtGfxConstructTextureA(void*) { NotRecovered("Atlas Graphics: the texture at vftable 0x00289768 (0x000FCEE0) is the texture work's, not recovered here"); }
void AtGfxDestroyTextureA(void*) { NotRecovered("Atlas Graphics: the texture at vftable 0x00289768 (0x000FD8C0) is the texture work's, not recovered here"); }
void AtGfxConstructTexture2D(void*) { NotRecovered("Atlas Graphics: the 2D texture (0x000FC630) is the texture work's, not recovered here"); }
void AtGfxDestroyTexture2D(void*) { NotRecovered("Atlas Graphics: the 2D texture (0x000FCCC0) is the texture work's, not recovered here"); }
void AtGfxConstructTextureDatabase(void*) { NotRecovered("Atlas Graphics: the texture database (0x0010FFC0) is the texture work's, not recovered here"); }
void AtGfxDestroyTextureDatabase(void*) { NotRecovered("Atlas Graphics: the texture database (0x001101D0) is the texture work's, not recovered here"); }

AtIndexArrayXbox::~AtIndexArrayXbox() { NotRecovered("Atlas Graphics: the index array's destructor, 0x000F84B0, is not recovered"); }
AtGfxBufferSet::~AtGfxBufferSet() { NotRecovered("Atlas Graphics: the buffer set's destructor, 0x000FA490, is not recovered"); }
AtMeshDatabaseXbox::~AtMeshDatabaseXbox() { NotRecovered("Atlas Graphics: the mesh database's destructor, 0x00111140, is not recovered"); }
AtGfxResourceXbox::~AtGfxResourceXbox() { NotRecovered("Atlas Graphics: the resource's destructor, 0x0011B040, is not recovered"); }
