// What creates the manager and its first heap. Original address clusters 0x000B3D80-0x000B3EEB, 0x000C9AC0-0x000C9BFB, 0x001038C0-0x0010392D.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtMemory.h"

#include "at/AtIoTokenizer.h"

void AtRaise(int code, int file, int line, int unused);
void AtFatal(const char* message, int file, int line, int unused);

int AtRegionMemorySystem::s_unknown3ADE40;
short AtRegionMemorySystem::s_unknown3ADE44;
void* AtMemoryGlobals::s_fatal = (void*)&AtFatal;

AtMemorySystem::AtMemorySystem() {
    m_started = 0;
}

AtMemorySystem::~AtMemorySystem() {
    if (AtIoTokenFile::s_heap != 0 && AtCurrentHeap() != 0) {
        AtPopHeap();
    }
    m_manager.ResetAll();
    m_started = 0;
}

void AtMemorySystem::Startup(const AtMemoryParams* params) {
    if (m_started != 0) {
        Shutdown();
    }
    Initialize(params);
    AtHeapManager::s_instance = &m_manager;
    AtIoTokenFile::s_heap = m_defaultHeap;
    if (m_defaultHeap != 0) {
        AtPushHeap(m_defaultHeap);
    }
    m_started = 1;
}

void AtMemorySystem::Shutdown() {
    if (AtIoTokenFile::s_heap != 0 && AtCurrentHeap() != 0) {
        AtPopHeap();
    }
    m_manager.ResetAll();
    m_started = 0;
}

AtRegionMemorySystem::AtRegionMemorySystem() {
    m_region = 0;
    s_unknown3ADE44 = 0;
    s_unknown3ADE40 = 0;
}

void AtRegionMemorySystem::Initialize(const AtMemoryParams* params) {
    m_region = AtPlatformAllocateRegion(params->m_regionSize);
    if (m_region == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_manager.Init(m_region, params->m_regionSize, params->m_unused);
    unsigned int size = params->m_defaultHeapSize;
    if (size == 0) {
        size = 0x10000;
    }
    m_defaultHeap = m_manager.CreateHeap(size, 2);
    AtPlatformInitDevices();
}

void AtRegionMemorySystem::Shutdown() {
    AtMemorySystem::Shutdown();
    if (m_region != 0) {
        AtPlatformFreeRegion(m_region);
    }
}

AtRegionMemorySystem::~AtRegionMemorySystem() {
    AtMemorySystem::Shutdown();
    if (m_region != 0) {
        AtPlatformFreeRegion(m_region);
    }
}

void AtMemoryGlobals::Save() {
    m_manager = AtHeapManager::s_instance;
    m_defaultHeap = (AtHeap*)AtIoTokenFile::s_heap;
    m_fatal = s_fatal;
}

void AtMemoryGlobals::Restore() {
    AtHeapManager::s_instance = m_manager;
    AtIoTokenFile::s_heap = m_defaultHeap;
    if (m_defaultHeap != 0 && AtCurrentHeap() == 0) {
        AtPushHeap(AtIoTokenFile::s_heap);
    }
    s_fatal = m_fatal;
}
