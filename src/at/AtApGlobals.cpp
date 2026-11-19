// The memory globals as the AtAp layers keep them on the stack. Subsystem at/atap; see include/at/AtApLayer.h. In a file of its own, because the memory
// tests construct AtMemoryGlobals and should not pull in the layers.
#include "at/AtApLayer.h"

AtMemoryGlobals::AtMemoryGlobals() {
    m_manager = 0;
    m_fatal = 0;  // ORIGINAL: m_defaultHeap is left as it is
}

AtApGlobals::AtApGlobals() {
}

void AtApGlobals::Save() {
    AtMemoryGlobals::Save();
}

void AtApGlobals::Restore() {
    AtMemoryGlobals::Restore();
}

AtIoGlobals::AtIoGlobals() {
    m_unknown0C = 0;
}

void AtIoGlobals::Save() {
    AtApGlobals::Save();
}

void AtIoGlobals::Restore() {
    AtApGlobals::Restore();
}
