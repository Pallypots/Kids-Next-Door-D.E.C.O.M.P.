// The I/O layer's file system factory: its start and stop, its block sizes, and how the layer reaches it. Subsystem at/atap; see include/at/AtApLayer.h.
// Original cluster 0x00102EB0-0x00102EF7, 0x0010F4B0, 0x000C9240. The slots that make the Xbox's device and file system are not here
// (src/at/AtIoSystemDevices.cpp stops; a port's platform layer replaces them).
#include "at/AtApLayer.h"

AtIoSystemFactory AtIoSystemFactory::s_instance;

AtIoSystemFactory::AtIoSystemFactory() {
    m_running = 0;
}

void AtIoSystemFactory::Init(const void*) {
    if (m_running != 0) {
        Shutdown();
    }
    m_running = 1;
}

void AtIoSystemFactory::Shutdown() {
    m_running = 0;
}

void AtIoSystemFactory::Destroy(AtIoFileSystem* fs) {
    AtIoDeleteObject(fs);
}

unsigned int AtIoSystemFactory::BlockSize() {
    return 0x8000;
}

unsigned int AtIoSystemFactory::MinimumBlocks() {
    return 4;
}

AtIoSystemFactory* AtIoGetFactory(AtIoGlobals* saved, const AtIoConfig*) {
    saved->Restore();
    char params = 0;  // ORIGINAL: one zero byte, whose address Init is given and does not read
    AtIoSystemFactory::s_instance.Init(&params);
    saved->Save();
    return &AtIoSystemFactory::s_instance;
}
