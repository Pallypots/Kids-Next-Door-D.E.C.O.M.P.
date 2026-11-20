// The graphics factory's slots 0, 3 and 4: the adapter list, making the graphics device and destroying it. Subsystem at/grfactory; see
// include/at/AtGraphicsFactory.h. In a file of their own so that a harness of the layer above can supply them (tests/native/test_grlayer.cpp,
// tests/oracle/oracle_grlayer.cpp). Names are inferred. Verification status per function: config/functions.d/grfactory.toml.
#include "at/AtGraphicsFactory.h"

#include "at/AtGraphicsDevice.h"
#include "at/AtGraphicsLayer.h"

void AtPushHeap(void* heap);
void AtPopHeap();
void AtRaise(int code, int file, int line, int unused);

namespace {
struct Placement {};
}
inline void* operator new(decltype(sizeof(0)), void* where, Placement) noexcept { return where; }
inline void operator delete(void*, void*, Placement) noexcept {}

void* AtGraphicsFactory::Slot0() {
    return &m_adapters;
}

AtMaterialOwner* AtGraphicsFactory::CreateDevice() {
    if (g_heap3B13CC == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(g_heap3B13CC);
    void* block = operator new(sizeof(AtGraphicsDeviceXbox));
    AtGraphicsDeviceXbox* device = block != 0 ? new (block, Placement()) AtGraphicsDeviceXbox() : 0;
    AtPopHeap();
    if (device == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    device->m_creator = this;
    return device;
}

void AtGraphicsFactory::Destroy(void* object) {
    if (object != 0) {
        static_cast<AtDisplayDevice*>(object)->Slot0();  // ORIGINAL: slot 0 with the flag 1
    }
}

