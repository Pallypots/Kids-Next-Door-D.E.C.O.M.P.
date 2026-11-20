// Host test of App's start-up pieces, x64 or any host. See include/gm/GmApp.h.
//
//   test_app              synthetic checks of what is recovered, then main, which must stop at the first thing not recovered
//   test_app <directory>  the same, with main run over a game directory: app.ini and JamFiles\XBox\*.JAM beneath it
//
// The platform functions platform/fatal.cpp holds are defined here instead, so that a stop can be caught and checked rather than ending the process.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtGraphicsFactory.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtHeap.h"
#include "at/AtMatrix.h"
#include "at/AtIoJam.h"
#include "at/AtIoMountManager.h"
#include "gm/GmApp.h"
#include "gm/GmLevelManager.h"
#include "host_file_system.h"
#include "host_io.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

struct Stop {
    std::string what;
};
void AtFatal(const char* message, int, int, int) { throw Stop{message}; }
void* AtPlatformAllocateRegion(unsigned int size) { return std::malloc(size); }
void AtPlatformFreeRegion(void* region) { std::free(region); }
void AtPlatformInitDevices() {}
void AtLibraryFree(void* data) { std::free(data); }
int AtPlatformLanguage() { return 1; }
int AtPlatformGameRegion() { return 1; }
unsigned int AtPlatformMilliseconds() { return 0; }

// ---- pieces --------------------------------------------------------------------------------------------------------

static void test_find_level() {
    GmLevelManager* m = (GmLevelManager*)std::calloc(1, sizeof(GmLevelManager));
    GmLevelName names[4];
    const char* text[4] = {"ALMOND", "LBKYRD1", "TWELVECH", "ALMOND"};  // the third fills all eight characters
    for (int i = 0; i < 4; i++) names[i].SetName(text[i]);
    m->m_names.m_items = names;
    m->m_names.m_count = 4;
    m->m_levelCount = 4;
    CHECK(m->FindLevel("ALMOND") == 0);
    CHECK(m->FindLevel("LBKYRD1") == 1);
    CHECK(m->FindLevel("TWELVECH") == 2);
    CHECK(m->FindLevel("TWELVECHX") == -1);  // the ninth character must end the name
    CHECK(m->FindLevel("TWELVE") == -1);
    CHECK(m->FindLevel("almond") == -1);    // case matters
    CHECK(m->FindLevel("") == -1);
    m->m_levelCount = 0;
    CHECK(m->FindLevel("ALMOND") == -1);
    std::free(m);
}

static void test_defaults_and_table() {
    App* app = new App;
    CHECK(App::s_instance == nullptr);
    CHECK(App::s_heapMemory == 0x2280000);
    CHECK(app->m_heapConfig[0].m_size == 0xA00000 && app->m_heapConfig[3].m_size == 0x280000);
    CHECK(app->m_heapConfig[2].m_partner == -1);
    CHECK(app->m_manifest == nullptr && app->m_frameworks[3] == nullptr);
    App::s_instance = app;
    app->SetFrameworkTable();
    const int expect[16] = {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 1, 3, 3, 2, 2};
    for (int i = 0; i < 16; i++) CHECK(app->m_frameworkTable[i] == expect[i]);
    app->m_settings.m_testAllLevels = 1;
    app->SetFrameworkTable();
    for (int i = 0; i < 16; i++) CHECK(app->m_frameworkTable[i] == expect[i]);

    AtMemoryParams params;
    app->GetMemoryParams(&params);
    CHECK(params.m_regionSize == 0x2282800 && params.m_unused == 0x1000 && params.m_defaultHeapSize == 0x100000);

    // The four heaps, made by the recovered heap manager over a region of the host's.
    AtHeapManager manager;
    void* region = std::malloc(params.m_regionSize);
    manager.Init(region, params.m_regionSize, 0);
    AtHeapManager::s_instance = &manager;
    AtHeap* defaultHeap = manager.CreateHeap(0x1000, 4);
    AtIoTokenFile::s_heap = defaultHeap;
    app->m_heapConfig[1].m_partner = -2;
    app->m_heapConfig[3].m_partner = 0;
    app->CreateHeaps();
    for (int i = 0; i < 4; i++) CHECK(app->m_heaps[i] != nullptr && app->m_heaps[i]->m_size == app->m_heapConfig[i].m_size);
    CHECK(app->m_heaps[1]->m_equivalencyId == defaultHeap->m_equivalencyId);
    CHECK(app->m_heaps[3]->m_equivalencyId == app->m_heaps[0]->m_equivalencyId);
    CHECK(app->m_heaps[2]->m_equivalencyId != app->m_heaps[0]->m_equivalencyId);
    CHECK(app->GetHeap(2) == app->m_heaps[2] && app->GetHeap(4) == nullptr && app->GetHeap(0xFFFFFFFF) == nullptr);
    unsigned int size = 0, used = 0;
    for (int i = 0; i < 4; i++) size += app->m_heaps[i]->m_size, used += app->m_heaps[i]->m_used;
    std::printf("heaps: %u bytes, %u used\n", size, used);
    CHECK(size == 0x2180000);  // the four; s_heapMemory is 1 MB more, the default heap of GetMemoryParams
    CHECK(app->HeapsUsed() == used);
    CHECK(app->HeapsFree() == size - used);
    app->m_heaps[1]->m_used += 0x30;
    CHECK(app->HeapsUsed() == used + 0x30 && app->HeapsFree() == size - used - 0x30);
    app->m_heaps[1]->m_used -= 0x30;
    AtHeap* saved = app->m_heaps[2];
    app->m_heaps[2] = nullptr;
    CHECK(app->HeapsUsed() == used - saved->m_used && app->HeapsFree() == size - used - (saved->m_size - saved->m_used));
    app->m_heaps[2] = saved;
    AppAtlasConfig atlas;
    std::memset(&atlas, 0x5A, sizeof atlas);
    app->GetAtlasConfig(&atlas);
    CHECK(atlas.m_heap == app->m_heaps[0] && atlas.m_unknown00 == 0 && atlas.m_unknown08[9] == 0);
    AppLayerConfig07 layer;
    std::memset(&layer, 0x5A, sizeof layer);
    app->GetConfig07(&layer);
    CHECK(layer.m_heap == app->m_heaps[0] && layer.m_spaces2 == 0 && layer.m_nameSpaceHeap == 0);
    app->DestroyHeaps();
    for (int i = 0; i < 4; i++) CHECK(manager.m_heaps[i + 1].m_size == 0);  // slot 0 is the default heap
    app->m_heapConfig[2].m_size = 0;
    app->CreateHeaps();
    CHECK(app->m_heaps[2] == nullptr && app->m_heaps[0] != nullptr);
    AtHeapManager::s_instance = nullptr;
    std::free(region);
    App::s_instance = nullptr;
    delete app;
}

static void test_setters() {
    char token;
    GmSetHeap00083490(&token);
    GmAcPlayerGameStateDataSetHeap(&token);
    GmSetHeap000980F0(&token);
    GmMoFramework::SetHeap(&token);
    CHECK(g_heap00301B70 == &token && g_heap003B3614 == &token && g_heap00305914 == &token && GmMoFramework::s_heap == &token);
    CHECK(GmGameSaveSize() == 0xF8);
    GmNameMap::s_heap = &token;
    GmScriptsInitNames();
    CHECK(g_scriptNames.m_size > 50 && g_scriptNames.m_entries != nullptr);  // the map rounds 50 up to its next size
}

class PathFs : public AtIoHostFileSystem {
public:
    PathFs() : AtIoHostFileSystem(".") {}
};

static void test_paths() {
    PathFs a, b;
    AtIoFileSystem* stack[2] = {&a, &b};
    AtIoContext context;
    context.m_stack = stack;
    context.m_stackCount = 2;
    context.m_stackSize = 2;
    const int topA = a.m_path.m_top, topB = b.m_path.m_top;
    context.PushPath("Design\\Registry\\");
    CHECK(a.m_path.m_top == topA + 1 && b.m_path.m_top == topB + 1);
    CHECK(std::strcmp(a.m_path.Current(), "Design\\Registry\\") == 0 && std::strcmp(b.m_path.Current(), "Design\\Registry\\") == 0);
    context.PopPath();
    CHECK(a.m_path.m_top == topA && b.m_path.m_top == topB);
    context.m_stackCount = 1;
    context.PushPath("x\\");
    CHECK(a.m_path.m_top == topA + 1 && b.m_path.m_top == topB);
    context.PopPath();
}

// ---- main ----------------------------------------------------------------------------------------------------------

static AtIoFileSystem* g_stack[8];
static char g_ioHeap;

// What the framework layers would have done before main reaches the disc: an I/O context over the host's files, and the mount manager on it. Port code.
static void mount_directory(const char* directory) {
    AtIoContext* context = new AtIoContext;
    context->m_stack = g_stack;
    context->m_stackSize = 8;
    AtIoContext::s_instance = context;
    AtIoFileRef::s_context = context;
    AtIoFastAlignedJamHeader::SetHeap(&g_ioHeap);
    AtIoFastJamHeader::SetHeap(&g_ioHeap);
    AtIoJamFileSystem::SetHeap(&g_ioHeap);
    AtIoJamFileSystem::SetHeaderHeap(&g_ioHeap);
    AtIoTokenFile::s_heap = &g_ioHeap;
    AtIoStream::s_heap = &g_ioHeap;
    AtIoHostFileSystem* root = new AtIoHostFileSystem(directory);
    context->Push(root);
    AtIoMountManager* mounts = new AtIoMountManager;
    mounts->m_context = context;
    mounts->m_device = root;
    AtIoMountManager::s_instance = mounts;
}

static void test_main(const char* directory) {
    if (directory) mount_directory(directory);
    if (directory) AtHostIoSetRoot(directory);
    GmNameMap::s_heap = 0;  // test_setters left it pointing at its stack
    AtIoStream::s_heap = 0;
    std::string stopped;
    try {
        char name[] = "default.xbe";
        char* argv[] = {name, nullptr};
        AppMain(1, argv, nullptr);
    } catch (const Stop& s) {
        stopped = s.what;
    }
    std::printf("main stopped: %s\n", stopped.c_str());
    // The first call of App::Init is GmApp's start-up. The host answers its memory query (platform/host_sdk.cpp), and the AtAp layers come up from the
    // base (at/atap): memory and the heaps, events, the network (off), I/O; the math layer's object is the first that is not recovered. No disc file is
    // read before it.
    // Then graphics: the factory's start (at/grfactory) runs on the host's answer to Direct3D (platform/host_d3d.cpp: one adapter, no device
    // capabilities), and the layer's Init makes the real graphics device (at/gfxdev), with its texture members (xdb's classes, folded in batch 8). The next
    // layer, the controllers', is the first that is not recovered.
    CHECK(stopped == "App start-up: the controller layer's Init (0x000B2E00) is not recovered");
    CHECK(g_atGraphicsFactory.m_adapters.Count() == 1 && g_atGraphicsFactory.m_adapters.At(0)->m_info[0].m_modes == nullptr);
    CHECK(g_atMatrix.Copy == AtMatrixCopy && g_atRender.m_matrix.Copy == AtMatrixCopy);
    CHECK(App::s_memoryTotal != 0);
    CHECK(g_app.m_memory.m_started == 1 && g_app.m_heaps[0] != nullptr && g_app.m_heaps[3] != nullptr);
    CHECK(g_app.m_io.m_below == &g_app.m_network && AtIoContext::s_instance == &g_app.m_io.m_context && g_app.m_io.m_context.m_device != nullptr);
    std::printf("memory reported: %u total, %u overhead; I/O context: %u file systems, %u buffer blocks of %u bytes\n", App::s_memoryTotal,
                App::s_memoryOverhead, g_app.m_io.m_context.m_stackSize, g_app.m_io.m_context.m_buffers.m_count, g_app.m_io.m_context.m_buffers.m_blockSize);
    CHECK(App::s_instance == &g_app);
    CHECK(GmAppSettings::s_argc == 1);
    CHECK(App::s_gmApp == &g_app);
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("levels\n");
    test_find_level();
    std::printf("defaults\n");
    test_defaults_and_table();
    std::printf("setters\n");
    test_setters();
    std::printf("paths\n");
    test_paths();
    test_main(argc > 1 ? argv[1] : nullptr);
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("app: all checks passed\n");
    return 0;
}
