// The math, data management, script and kinematics layers' objects, the data registries' owner's start-up, the small-C script system's start-up and
// the heap setters these layers call. Subsystem at/atlayers; see include/at/AtApMidLayers.h. Original clusters 0x0005EFD0-0x0005F0A8,
// 0x00075400-0x00075859, 0x0009EE00-0x0009EED4, 0x000B34B0-0x000B38F3, 0x000B4250-0x000B430A, 0x000C8660-0x000C8936, and single setters. Names are
// inferred.
#include "at/AtApMidLayers.h"

#include "at/AtAnimationDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtDataLibrary.h"
#include "at/AtGrD3DDatabase.h"
#include "at/AtMatAnimation.h"
#include "at/AtSceneActor.h"
#include "at/AtSegmentQuery.h"

void AtPushHeap(void* heap);
void AtPopHeap();
void AtRaise(int code, int file, int line, int unused);

namespace {
struct Placement {};
}
inline void* operator new(decltype(sizeof(0)), void* where, Placement) noexcept { return where; }
inline void operator delete(void*, void*, Placement) noexcept {}

void* g_heap3ADF80;
void* g_heap2C6348;
void* g_heap3B23E8;
void* g_heap3B23E4;
void* g_heap2C63DC;
void* g_heap2C634C;
void* g_heap3B23E0;
void* AtSmallCScriptFactory::s_heap;
void* AtSmallCSystem::s_heap;
AtSmallCSystem* AtSmallCSystem::s_instance;
void* AtScriptHost::s_heap;

// ---- the math layer ---------------------------------------------------------------------------------------------------

AtMathSystem::AtMathSystem() {
    m_below = 0;
    m_unknown08 = 0;
}

void AtMathSystem::Init(const AtMathConfig* config, void* below, void*) {
    if (m_below != 0) {
        Shutdown();
    }
    m_below = below;
    SetHeaps(config);
}

void AtMathSystem::Shutdown() {
    m_below = 0;
    m_unknown08 = 0;
}

void AtMathSystem::SetHeaps(const AtMathConfig* config) {
    AtSegmentHitList::SetHeap(config->m_segmentHeap != 0 ? config->m_segmentHeap : config->m_heap);
    AtQueryPool::SetHeap(config->m_queryHeap != 0 ? config->m_queryHeap : config->m_heap);
}

// ---- the data management layer ----------------------------------------------------------------------------------------

AtDataRegistryOwner::AtDataRegistryOwner() {
}

void AtDataRegistryOwner::Init(unsigned int databaseSpaces, unsigned int registrySpaces) {
    char empty = 0;
    m_databases.SetName(&empty);
    m_plain.SetName(&empty);
    if (databaseSpaces != 0) {
        m_databaseSpaces.Init(databaseSpaces);
    }
    if (registrySpaces != 0) {
        m_spaces.Init(registrySpaces);
    }
}

void AtDataRegistryOwner::Clear() {
    m_databases.Reset();
    m_plain.Reset();
    m_databaseSpaces.Clear();
    m_spaces.Clear();
}

AtDataManagementSystem::AtDataManagementSystem() {
    m_below = 0;
}

void AtDataManagementSystem::Init(const AtDataManagementConfig* config, void* below, void*) {
    if (m_below != 0) {
        Shutdown();
    }
    m_below = below;
    SetHeaps(config);
    m_owner.Init(config->m_databaseSpaces, config->m_registrySpaces);
}

void AtDataManagementSystem::Shutdown() {
    m_owner.Clear();
    m_below = 0;
}

void AtDataManagementSystem::SetHeaps(const AtDataManagementConfig* config) {
    AtDataComponent::SetHeap(config->m_componentHeap != 0 ? config->m_componentHeap : config->m_heap);
    AtDataDescriptor::SetHeap(config->m_descriptorHeap != 0 ? config->m_descriptorHeap : config->m_heap);
    AtDatabaseManager::SetHeap(config->m_managerHeap != 0 ? config->m_managerHeap : config->m_heap);
    AtDataLibrary::SetHeap(config->m_libraryHeap != 0 ? config->m_libraryHeap : config->m_heap);
    AtRecordLayout::SetHeap(config->m_layoutHeap != 0 ? config->m_layoutHeap : config->m_heap);
    AtDataRecordDatabase::SetHeap(config->m_recordDatabaseHeap != 0 ? config->m_recordDatabaseHeap : config->m_heap);
    AtDataRegistry::SetHeap(config->m_registryHeap != 0 ? config->m_registryHeap : config->m_heap);
    AtDataLink::SetHeap(config->m_linkHeap != 0 ? config->m_linkHeap : config->m_heap);
    AtSetHeap3ADF80(config->m_heap28 != 0 ? config->m_heap28 : config->m_heap);
    AtDataRegistryNameSpaces::SetHeap(config->m_nameSpacesHeap != 0 ? config->m_nameSpacesHeap : config->m_heap);
}

void AtSetHeap3ADF80(void* heap) { g_heap3ADF80 = heap; }

// ---- the script layer -------------------------------------------------------------------------------------------------

void AtSmallCScriptFactory::SetHeap(void* heap) { s_heap = heap; }
void AtSmallCSystem::SetHeap(void* heap) { s_heap = heap; }
void AtScriptHost::SetHeap(void* heap) { s_heap = heap; }

const AtSmallCNativeEntry g_smallCCoreNatives[] = {
    {"numargs", AtSmallCNumArgs, 0},   {"getarg", AtSmallCGetArg, 0},       {"setarg", AtSmallCSetArg, 0},
    {"heapspace", AtSmallCHeapSpace, 0}, {"funcidx", AtSmallCFuncIdx, 0},   {"strlen", AtSmallCStrLen, 0},
    {"strpack", AtSmallCStrPack, 0},   {"strunpack", AtSmallCStrUnpack, 0}, {"tolower", AtSmallCToLower, 0},
    {"toupper", AtSmallCToUpper, 0},   {"", 0, 0},
};
const AtSmallCNativeEntry g_smallCFloatNatives[] = {
    {"float", AtSmallCFloat, 0},         {"floatstr", AtSmallCFloatStr, 0},     {"floatmul", AtSmallCFloatMul, 0},
    {"floatdiv", AtSmallCFloatDiv, 0},   {"floatadd", AtSmallCFloatAdd, 0},     {"floatsub", AtSmallCFloatSub, 0},
    {"floatfract", AtSmallCFloatFract, 0}, {"floatround", AtSmallCFloatRound, 0}, {"floatcmp", AtSmallCFloatCmp, 0},
    {"", 0, 0},
};

AtSmallCSystem::AtSmallCSystem() {
    Clear();
}

void AtSmallCSystem::Clear() {
    m_running = 0;
    s_heap = 0;  // ORIGINAL: clearing the system clears the heap it is made from, which the host sets again straight after its Init
    m_unknown5B10 = 0;
    m_unknown5B30 = 0;
    m_factory = 0;
    m_nativeCount = 0;
    m_unknown5B08 = 0;
    m_unknown5B0C = 0;
    m_unknown5B38 = 0;
    m_unknown5B3C = 4;
    m_unknown5B18 = 0;
    m_unknown5B34 = 0;
    unsigned int* natives = (unsigned int*)m_natives;
    for (unsigned int i = 0; i < sizeof(m_natives) / sizeof(unsigned int); i++) {
        natives[i] = 0;
    }
    for (int i = 0; i < kMaxScripts; i++) {
        m_scripts[i] = 0;
    }
}

int AtSmallCSystem::Register(void* context, const char* name, AtSmallCNative function) {
    if (m_nativeCount >= kMaxNatives) {
        return 0;
    }
    m_natives[m_nativeCount].m_context = context;
    m_natives[m_nativeCount].m_function = function;
    char* to = m_natives[m_nativeCount].m_name;  // ORIGINAL: copied to its terminator, however long
    while ((*to++ = *name++) != 0) {
    }
    m_nativeCount++;
    return 1;
}

void AtSmallCSystem::Init() {
    s_instance = this;
    for (const AtSmallCNativeEntry* entry = g_smallCCoreNatives; entry->m_function != 0; entry++) {
        Register(this, entry->m_name, entry->m_function);
    }
    for (const AtSmallCNativeEntry* entry = g_smallCFloatNatives; entry->m_function != 0; entry++) {
        Register(this, entry->m_name, entry->m_function);
    }
    m_names.Init(kMaxScripts);
    m_running = 1;
}

void AtSmallCSystem::Shutdown() {
    if (m_running == 0) {
        return;
    }
    m_names.Clear();
    for (int i = 0; i < kMaxScripts; i++) {
        if (m_scripts[i] != 0) {
            m_factory->Release(m_scripts[i]);
        }
    }
    Clear();  // ORIGINAL: inlined there
}

AtScriptHost::AtScriptHost() {
    m_running = 0;
}

void AtScriptHost::Init() {
    m_factory.Init();
    m_system.Init();
    AtSmallCSystem::SetHeap(s_heap);  // ORIGINAL: both overwrite what the layer's SetHeaps set from the config
    AtSmallCScriptFactory::SetHeap(s_heap);
    m_system.m_factory = &m_factory;
    m_running = 1;
}

void AtScriptHost::Shutdown() {
    m_system.Shutdown();
    m_factory.Shutdown();
    m_running = 0;
}

AtScriptSystem::AtScriptSystem() {
    m_below = 0;
    m_running = 0;
}

void AtScriptSystem::Init(const AtScriptConfig* config, void* below, void*) {
    if (m_running != 0) {
        Shutdown();
    }
    m_below = below;
    SetHeaps(config);
    m_host.Init();
    m_running = 1;
}

void AtScriptSystem::Shutdown() {
    m_host.Shutdown();
    m_below = 0;
    m_running = 0;
}

void AtScriptSystem::SetHeaps(const AtScriptConfig* config) {
    AtScriptHost::SetHeap(config->m_hostHeap != 0 ? config->m_hostHeap : config->m_heap);
    AtSmallCScriptFactory::SetHeap(config->m_factoryHeap != 0 ? config->m_factoryHeap : config->m_heap);
    AtSmallCSystem::SetHeap(config->m_systemHeap != 0 ? config->m_systemHeap : config->m_heap);
}

// ---- the kinematics layer ---------------------------------------------------------------------------------------------

void AtSetHeap2C6348(void* heap) { g_heap2C6348 = heap; }
void AtSetHeap3B23E8(void* heap) { g_heap3B23E8 = heap; }
void AtSetHeap3B23E4(void* heap) { g_heap3B23E4 = heap; }
void AtSetHeap2C63DC(void* heap) { g_heap2C63DC = heap; }
void AtSetHeap2C634C(void* heap) { g_heap2C634C = heap; }
void AtSetHeap3B23E0(void* heap) { g_heap3B23E0 = heap; }
void AtGrD3DAnimationDevice::SetHeap(void* heap) { s_heap = heap; }

AtKinematicsSystemBase::AtKinematicsSystemBase() {
    m_below = 0;
}

void AtKinematicsSystemBase::Init(const AtKinematicsConfig* config, void* below, void*) {
    if (m_below != 0) {
        Shutdown();
    }
    m_below = below;
    SetHeaps(config);
}

void AtKinematicsSystemBase::Shutdown() {
    m_below = 0;
}

void AtKinematicsSystemBase::SetHeaps(const AtKinematicsConfig* config) {
    AtSceneAnimBlend::SetHeap(config->m_animBlendHeap != 0 ? config->m_animBlendHeap : config->m_heap);
    AtAnimationDatabase::SetHeap(config->m_animationHeap != 0 ? config->m_animationHeap : config->m_heap);
    AtCoordSysDatabase::SetHeap(config->m_coordSysHeap != 0 ? config->m_coordSysHeap : config->m_heap);
    AtSetHeap2C6348(config->m_heap14 != 0 ? config->m_heap14 : config->m_heap);
    AtGrFrameNumberSet::SetHeap(config->m_frameNumberHeap != 0 ? config->m_frameNumberHeap : config->m_heap);
    AtSetHeap3B23E8(config->m_heap20 != 0 ? config->m_heap20 : config->m_heap);
    AtSetHeap3B23E4(config->m_heap24 != 0 ? config->m_heap24 : config->m_heap);
    AtSetHeap2C63DC(config->m_heap28 != 0 ? config->m_heap28 : config->m_heap);
    AtSetHeap2C634C(config->m_heap2C != 0 ? config->m_heap2C : config->m_heap);
    AtSetHeap3B23E0(config->m_heap30 != 0 ? config->m_heap30 : config->m_heap);
}

AtKinematicsSystem::AtKinematicsSystem() {
}

void AtKinematicsSystem::Init(const AtKinematicsConfig* config, void* below, void* memory) {
    AtKinematicsSystemBase::Init(config, below, memory);
    SetHeaps(config);  // ORIGINAL: a second time, after the base's Init has set them
    AtPushHeap(config->m_heap);
    void* block = operator new(sizeof(AtGrD3DAnimationDevice));
    AtGrD3DAnimationDevice* factory = block != 0 ? new (block, Placement()) AtGrD3DAnimationDevice() : 0;
    AtPopHeap();
    if (factory == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    factory->Init();
    m_factory = factory;
}

void AtKinematicsSystem::Shutdown() {
    AtGrD3DAnimationDevice* factory = m_factory;
    if (factory != 0) {
        factory->Shutdown();
        factory->~AtGrD3DAnimationDevice();
        operator delete(factory);
        m_factory = 0;
    }
    AtKinematicsSystemBase::Shutdown();
}

void AtKinematicsSystem::SetHeaps(const AtKinematicsConfig* config) {
    AtKinematicsSystemBase::SetHeaps(config);
    AtGrD3DAnimationDevice::SetHeap(config->m_factoryHeap != 0 ? config->m_factoryHeap : config->m_heap);
}
