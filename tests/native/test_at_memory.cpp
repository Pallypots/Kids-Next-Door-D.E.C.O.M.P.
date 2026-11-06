// Host test of the memory system on whatever pointer size this build has, through the stand-ins of platform/fatal.cpp. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtIoTokenizer.h"
#include "at/AtMemory.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

int main() {
    CHECK(AtHeapStack::s_top == -1);
    CHECK(AtCurrentHeap() == nullptr);
    AtRegionMemorySystem system;
    const AtMemoryParams params = {0x200000, 0, 0};
    system.Startup(&params);
    CHECK(system.m_started == 1);
    CHECK(AtHeapManager::s_instance == &system.m_manager);
    CHECK(AtIoTokenFile::s_heap == system.m_defaultHeap);
    CHECK(AtCurrentHeap() == system.m_defaultHeap);
    CHECK(system.m_defaultHeap->m_size == 0x10000);

    // Allocating follows the stack.
    AtHeap* const other = system.m_manager.CreateHeap(0x8000, 2);
    void* const a = AtHeapNew(100);
    CHECK(a != nullptr && system.m_defaultHeap->Owns(a) == 1);
    AtPushHeap(other);
    void* const b = AtHeapNew(100);
    CHECK(b != nullptr && other->Owns(b) == 1);
    AtPopHeap();
    std::memset(a, 1, 100);
    std::memset(b, 2, 100);
    AtHeapDelete(b);
    AtHeapDelete(a);
    AtHeapDelete(nullptr);
    CHECK(other->m_used == 0);
    CHECK(system.m_defaultHeap->m_used == 0);

    AtMemoryGlobals saved;
    saved.Save();
    CHECK(saved.m_manager == &system.m_manager);
    saved.Restore();
    CHECK(AtHeapStack::s_top == 0);

    system.Startup(&params);  // Started again: shuts down first.
    CHECK(AtHeapStack::s_top == 0);
    CHECK(system.m_defaultHeap->m_size == 0x10000);
    system.AtMemorySystem::Shutdown();
    CHECK(AtHeapStack::s_top == -1);
    CHECK(system.m_started == 0);
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
