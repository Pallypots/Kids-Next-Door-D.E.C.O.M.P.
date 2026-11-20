// What the graphics layer reaches and is not recovered (at/grlayer): the factory's slots but 0, 3 and 4 (at/grfactory recovers those and the start and stop), the display
// slots and the deleting destructors. Each stops with a diagnostic; none is recorded or counted. In a file of its own, so that a harness can supply them
// instead. See include/at/AtGraphicsLayer.h.
#include "at/AtGraphicsLayer.h"

void AtFatal(const char* message, int file, int line, int unused);  // 0x00011070

namespace {
void StopHere(const char* what) {
    AtFatal(what, 0, 0, 0);
}
}


void AtGraphicsSystemBase::DeletingDestructor(unsigned int) { StopHere("Graphics layer: the base's deleting destructor (0x000C85B0) is not recovered"); }
void AtGraphicsSystemBase::SetDisplay(AtDisplayMode*, int, int, int, const AtPixelFormat*, int, int) {
    StopHere("Graphics layer: the base's slot 4 is pure (0x00125F25)");
}
void AtGraphicsSystem::DeletingDestructor(unsigned int) { StopHere("Graphics layer: the deleting destructor (0x000B3490) is not recovered"); }
void AtGraphicsSystem::SetDisplay(AtDisplayMode*, int, int, int, const AtPixelFormat*, int, int) {
    StopHere("Graphics layer: setting the display (slot 4, 0x000B32D0) is not recovered");
}
void AtGraphicsSystem::UpdateDisplay() { StopHere("Graphics layer: slot 5 (0x000B3350) is not recovered"); }

AtDatabaseLoader* AtGraphicsFactory::DatabaseLoader() { StopHere("Graphics factory: the database loader (slot 29, 0x000EBB50) is not recovered"); return 0; }
#define SLOT(n) \
    void AtGraphicsFactory::Slot##n() { StopHere("Graphics factory: slot " #n " is not recovered"); }
SLOT(1) SLOT(2) SLOT(5) SLOT(6) SLOT(7) SLOT(8) SLOT(9) SLOT(10) SLOT(11) SLOT(12) SLOT(13) SLOT(14) SLOT(15) SLOT(16) SLOT(17) SLOT(18) SLOT(19)
SLOT(20) SLOT(21) SLOT(22) SLOT(23) SLOT(24) SLOT(25) SLOT(26) SLOT(27) SLOT(28)
#undef SLOT
