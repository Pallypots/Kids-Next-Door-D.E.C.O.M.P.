// Host test of the math, data management, script and kinematics layers' objects (at/atlayers), on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>
#include <new>

#include "at/AtAnimationDatabase.h"
#include "at/AtApMidLayers.h"
#include "at/AtCoordSys.h"
#include "at/AtDataLibrary.h"
#include "at/AtMatAnimation.h"
#include "at/AtSceneActor.h"
#include "at/AtSegmentQuery.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_heaps[16];
#define HEAP(i) ((AtHeap*)&g_heaps[i])

static void test_math() {
    AtMathSystem m;
    CHECK(m.m_below == nullptr && m.m_unknown08 == 0);
    AtMathConfig c = {};
    c.m_heap = HEAP(0);
    c.m_queryHeap = HEAP(1);
    m.Init(&c, HEAP(9), nullptr);
    CHECK(m.m_below == HEAP(9));
    CHECK(AtSegmentHitList::s_heap == HEAP(0) && AtQueryPool::s_heap == HEAP(1));
    m.m_unknown08 = 5;
    c.m_segmentHeap = HEAP(2);
    m.Init(&c, HEAP(8), nullptr);  // running: shuts down first
    CHECK(m.m_below == HEAP(8) && m.m_unknown08 == 0 && AtSegmentHitList::s_heap == HEAP(2));
}

static void test_data_management() {
    GmNameSpaceDb::s_heap = HEAP(0);
    GmNameMap::s_heap = HEAP(0);
    AtDataManagementSystem d;
    CHECK(d.m_below == nullptr);
    AtDataManagementConfig c = {};
    c.m_databaseSpaces = 0x80;
    c.m_registrySpaces = 0x80;
    c.m_heap = HEAP(0);
    c.m_linkHeap = HEAP(3);
    c.m_heap28 = HEAP(4);
    d.Init(&c, HEAP(9), nullptr);
    CHECK(d.m_below == HEAP(9));
    CHECK(AtDataComponent::s_heap == HEAP(0) && AtDataDescriptor::s_heap == HEAP(0) && AtDatabaseManager::s_heap == HEAP(0));
    CHECK(AtDataLibrary::s_heap == HEAP(0) && AtRecordLayout::s_heap == HEAP(0) && AtDataRecordDatabase::s_heap == HEAP(0));
    CHECK(AtDataRegistry::s_heap == HEAP(0) && AtDataLink::s_heap == HEAP(3) && g_heap3ADF80 == HEAP(4) && AtDataRegistryNameSpaces::s_heap == HEAP(0));
    CHECK(d.m_owner.m_databases.m_name[0] == 0 && d.m_owner.m_plain.m_name[0] == 0);
    CHECK(d.m_owner.m_databaseSpaces.m_poolSize == 0x80 && d.m_owner.m_databaseSpaces.m_pool != nullptr);
    CHECK(d.m_owner.m_spaces.m_poolSize == 0x80 && d.m_owner.m_spaces.m_pool != nullptr);
    d.Shutdown();
    CHECK(d.m_below == nullptr);
}

static void test_script() {
    GmNameMap::s_heap = HEAP(0);
    AtScriptSystem* s = new AtScriptSystem;
    CHECK(s->m_running == 0 && s->m_below == nullptr && s->m_host.m_running == 0);
    CHECK(s->m_host.m_system.m_running == 0 && s->m_host.m_system.m_unknown5B3C == 4 && s->m_host.m_system.m_nativeCount == 0);
    AtScriptConfig c = {};
    c.m_heap = HEAP(0);
    c.m_hostHeap = HEAP(5);
    c.m_factoryHeap = HEAP(6);
    c.m_systemHeap = HEAP(7);
    s->Init(&c, HEAP(9), nullptr);
    AtSmallCSystem& y = s->m_host.m_system;
    CHECK(s->m_running == 1 && s->m_below == HEAP(9) && s->m_host.m_running == 1);
    CHECK(AtScriptHost::s_heap == HEAP(5));
    CHECK(AtSmallCScriptFactory::s_heap == HEAP(5) && AtSmallCSystem::s_heap == HEAP(5));  // the host's Init gives both its own heap
    CHECK(AtSmallCSystem::s_instance == &y && y.m_running == 1 && y.m_factory == &s->m_host.m_factory);
    CHECK(y.m_nativeCount == 19);
    CHECK(!std::strcmp(y.m_natives[0].m_name, "numargs") && y.m_natives[0].m_function == AtSmallCNumArgs && y.m_natives[0].m_context == &y);
    CHECK(!std::strcmp(y.m_natives[9].m_name, "toupper") && !std::strcmp(y.m_natives[10].m_name, "float"));
    CHECK(!std::strcmp(y.m_natives[18].m_name, "floatcmp") && y.m_natives[18].m_function == AtSmallCFloatCmp && y.m_natives[19].m_function == nullptr);
    CHECK(y.m_names.m_size >= 2000 && y.m_names.m_entries != nullptr);
    // A second Init shuts down first: no scripts are held, so none goes back to the factory.
    s->Init(&c, HEAP(8), nullptr);
    CHECK(s->m_running == 1 && s->m_below == HEAP(8) && y.m_nativeCount == 19 && y.m_running == 1);
    // The table holds 0x200.
    unsigned int n = y.m_nativeCount;
    while (y.Register(&y, "x", AtSmallCFloat) == 1) n++;
    CHECK(n == AtSmallCSystem::kMaxNatives && y.m_nativeCount == n);
    s->Shutdown();
    CHECK(s->m_running == 0 && s->m_below == nullptr && s->m_host.m_running == 0 && y.m_running == 0 && y.m_nativeCount == 0);
    CHECK(AtSmallCSystem::s_heap == nullptr && y.m_factory == nullptr);  // ORIGINAL: Clear clears the heap too
}

static void test_kinematics() {
    AtKinematicsSystem k;
    CHECK(k.m_below == nullptr);
    AtKinematicsConfig c = {};
    c.m_heap = HEAP(0);
    c.m_coordSysHeap = HEAP(1);
    c.m_factoryHeap = HEAP(2);
    c.m_heap30 = HEAP(3);
    k.Init(&c, HEAP(9), nullptr);
    CHECK(k.m_below == HEAP(9) && k.m_factory != nullptr);
    CHECK(AtSceneAnimBlend::s_heap == HEAP(0) && AtAnimationDatabase::s_heap == HEAP(0) && AtCoordSysDatabase::s_heap == HEAP(1));
    CHECK(g_heap2C6348 == HEAP(0) && AtGrFrameNumberSet::s_heap == HEAP(0) && g_heap3B23E8 == HEAP(0) && g_heap3B23E4 == HEAP(0));
    CHECK(g_heap2C63DC == HEAP(0) && g_heap2C634C == HEAP(0) && g_heap3B23E0 == HEAP(3) && AtKinematicsFactory::s_heap == HEAP(2));
    k.Init(&c, HEAP(8), nullptr);  // running: shuts down first, which frees the factory
    CHECK(k.m_below == HEAP(8) && k.m_factory != nullptr);
    k.Shutdown();
    CHECK(k.m_below == nullptr && k.m_factory == nullptr);
}

int main() {
    test_math();
    test_data_management();
    test_script();
    test_kinematics();
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("atlayers: all checks passed\n");
    return 0;
}
