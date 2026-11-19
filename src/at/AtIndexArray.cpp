// The index array's constructors: AtGeometryArray and AtIndexArray, include/at/AtMeshDatabase.h. The concrete Xbox one is in src/at/AtGraphicsDevice.cpp.
// Verification status per function: config/functions.d/gfxdev.toml.
#include "at/AtMeshDatabase.h"

AtGeometryArray::AtGeometryArray() {
    m_count = 0;
    m_flags = 0;
    m_loadFlags = 0;  // One halfword in the original.
    m_unknown0B = 0;
}

namespace {

void NotRecovered(const char* message) {
    AtFatal(message, 0, 0, 0);
}

}  // namespace

void AtGeometryArray::Slot1() { NotRecovered("AtGeometryArray: slot 1, 0x00044910, is not recovered"); }
void AtGeometryArray::Slot2() { NotRecovered("AtGeometryArray: slot 2, 0x000448E0, is not recovered"); }
void AtGeometryArray::Lock(void**, int) { NotRecovered("AtGeometryArray: slot 3 is pure"); }
void AtGeometryArray::Unlock() { NotRecovered("AtGeometryArray: slot 4 is pure"); }
void AtGeometryArray::Finish() { NotRecovered("AtGeometryArray: slot 5 is pure"); }
void AtGeometryArray::Slot6() { NotRecovered("AtGeometryArray: slot 6 is pure"); }
void AtGeometryArray::Slot7() { NotRecovered("AtGeometryArray: slot 7 is pure"); }
void AtGeometryArray::Slot8() { NotRecovered("AtGeometryArray: slot 8 is pure"); }

AtIndexArray::AtIndexArray() {
    m_index = 0;
    m_name[0] = 0;
}
