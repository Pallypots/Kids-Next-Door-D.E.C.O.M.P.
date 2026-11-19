// Host test of the AtAp layers' start-ups and the event, network and I/O layers' objects (at/atap, gm/atap), x64 or any host. See include/at/AtApLayer.h.
//
// The platform functions platform/fatal.cpp holds are defined here instead, so that a stop can be caught and checked. The I/O factory's device and file
// systems are the host's (platform/host_io.cpp).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtApLayer.h"
#include "at/AtIoJam.h"
#include "at/AtIoTokenizer.h"
#include "at/AtText.h"
#include "gm/GmApp.h"
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

void GmSetHeap00083490(void* heap);
extern void* g_heap00301B70;

template <class F> static std::string stop_of(F f) {
    try {
        f();
    } catch (const Stop& s) {
        return s.what;
    }
    return "";
}

// ---- App's configs --------------------------------------------------------------------------------------------------

static void test_configs() {
    App* app = new App;
    AtHeap* heaps[4];
    for (int i = 0; i < 4; i++) {
        heaps[i] = (AtHeap*)(std::malloc(64));
        app->m_heaps[i] = heaps[i];
    }
    AtIoConfig io;
    std::memset(&io, 0, sizeof io);
    io.m_jamHeap = (void*)1;
    app->GetIoConfig(&io);
    CHECK(io.m_kind == 1 && io.m_stackSize == 6 && io.m_spaces == 0x10 && io.m_bufferBytes == 0x40000 && io.m_requests == 0x10 && io.m_heap == heaps[0]);
    CHECK(io.m_jamHeap == (void*)1);  // not cleared: start-up has
    AppMathConfig math;
    std::memset(&math, 0x5A, sizeof math);
    app->GetMathConfig(&math);
    CHECK(math.m_heap == heaps[0] && math.m_unknown00 == 0 && math.m_segmentHeap == nullptr && math.m_queryHeap == nullptr);
    AppDataManagementConfig data;
    std::memset(&data, 0x5A, sizeof data);
    app->GetDataManagementConfig(&data);
    CHECK(data.m_databaseSpaces == 0x80 && data.m_registrySpaces == 0x80 && data.m_heap == heaps[0] && data.m_libraryHeap == nullptr);
    AppScriptConfig script;
    std::memset(&script, 0x5A, sizeof script);
    app->GetScriptConfig(&script);
    CHECK(script.m_heap == heaps[0] && script.m_unknown00 == 0 && script.m_systemHeap == nullptr);
    AppKinematicsConfig kinematics;
    std::memset(&kinematics, 0x5A, sizeof kinematics);
    app->GetKinematicsConfig(&kinematics);
    CHECK(kinematics.m_heap == heaps[0] && kinematics.m_unknown00 == 0 && kinematics.m_heap30 == nullptr);
    AppGraphicsConfig graphics;
    std::memset(&graphics, 0x5A, sizeof graphics);
    app->GetGraphicsConfig(&graphics);
    CHECK(graphics.m_unknown00 == 1 && graphics.m_unknown04 == 0 && graphics.m_heap == heaps[0] && graphics.m_unknownEC == 0);
    AppControllerConfig controller;
    std::memset(&controller, 0x5A, sizeof controller);
    app->GetControllerConfig(&controller);
    CHECK(controller.m_unknown00 == 1 && controller.m_heap == heaps[0] && controller.m_unknown08[9] == 0);
    AppSoundConfig sound;
    std::memset(&sound, 0x5A, sizeof sound);
    app->GetSoundConfig(&sound);
    CHECK(sound.m_heap == heaps[1] && sound.m_unknown2C == 100 && sound.m_unknown04[9] == 0);
    AppSceneConfig scene;
    std::memset(&scene, 0x5A, sizeof scene);
    app->GetSceneConfig(&scene);
    CHECK(scene.m_heap == heaps[0] && scene.m_unknown00[6] == 0 && scene.m_unknown20[26] == 0);
    AppVideoConfig video;
    std::memset(&video, 0x5A, sizeof video);
    app->GetVideoConfig(&video);
    CHECK(video.m_unknown00[0] == 0x5A5A5A5A);  // App leaves it as it was
    for (int i = 0; i < 4; i++) {
        app->m_heaps[i] = 0;
        std::free(heaps[i]);
    }
    delete app;
}

// ---- the objects alone ------------------------------------------------------------------------------------------------

static char g_heapA, g_heapB, g_heapC;

static void test_network() {
    AtNetworkSystem network;
    AtNetworkConfig config;
    std::memset(&config, 0, sizeof config);
    config.m_enabled = 1;
    for (int i = 0; i < 4; i++) config.m_settings[i] = 10 + i;
    char below;
    network.Init(&config, &below);
    CHECK(config.m_enabled == 0 && network.m_enabled == 0);  // always off
    CHECK(network.m_below == &below && network.m_settings[0] == 10 && network.m_settings[3] == 13);
    CHECK(stop_of([&] { network.Init(&config, &below); }) == "AtNetworkSystem: Shutdown (0x000B3B30) is not recovered");
}

static void test_event() {
    GmNameMap::s_heap = 0;
    GmNameSpaceDb::s_heap = 0;
    AtEventSystem events;
    AtEventConfig config;
    std::memset(&config, 0, sizeof config);
    config.m_heap = (AtHeap*)&g_heapA;
    config.m_heap14 = &g_heapB;
    config.m_spaces2 = 5;
    char below;
    events.Init(&config, &below);
    CHECK(events.m_below == &below);
    CHECK(AtEventSystem::s_heap3ADF9C == &g_heapA && AtEventSystem::s_heap305918 == &g_heapB && AtEventSystem::s_heap3ADF8C == &g_heapA);
    CHECK(GmNameMap::s_heap == &g_heapA && GmNameSpaceDb::s_heap == &g_heapA);
    CHECK(events.m_names.m_group.m_used == 1 && events.m_names.m_group2.m_used == 1 && events.m_names.m_group.m_name[0] == 0);
    CHECK(events.m_names.m_spaces.m_pool == nullptr && events.m_names.m_spaces2.m_pool != nullptr && events.m_names.m_spaces2.m_poolSize == 5);
    config.m_nameMapHeap = &g_heapC;  // already set: kept
    events.SetHeaps(&config);
    CHECK(GmNameMap::s_heap == &g_heapA);
    CHECK(stop_of([&] { events.Init(&config, &below); }) == "AtEventSystem: Shutdown (0x000B3BB0) is not recovered");
}

static void test_io() {
    AtIoStream::s_heap = 0;
    AtIoSystem io;
    AtIoConfig config;
    std::memset(&config, 0, sizeof config);
    char below, memory;
    io.Init(&config, &below, &memory);  // kind 0: nothing comes up
    CHECK(io.m_below == &below && io.m_kind == 0 && io.m_factory == nullptr && AtIoSystemFactory::s_instance.m_running == 0);
    io.m_below = 0;
    config.m_kind = 1;
    config.m_heap = (AtHeap*)&g_heapA;
    config.m_textHeap = &g_heapB;
    config.m_bufferBytes = 0x9000;  // less than four blocks: four
    config.m_requests = 3;
    io.Init(&config, &below, &memory);
    CHECK(config.m_kind == 2 && io.m_kind == 2 && config.m_stackSize == 1 && config.m_spaces == 8);
    CHECK(io.m_factory == &AtIoSystemFactory::s_instance && AtIoSystemFactory::s_instance.m_running == 1);
    CHECK(AtIoJamFileSystem::s_heap == &g_heapA && AtTextTable::s_heap == &g_heapB && AtIoContext::s_heap == &g_heapA && AtIoBufferPool::s_heap == &g_heapA);
    CHECK(AtIoStream::s_heap == &g_heapA && g_heap3B23EC == &g_heapA && g_heap301B6C == &g_heapA && g_heap00301B70 == &g_heapA);
    AtIoContext& c = io.m_context;
    CHECK(c.m_unknown04 == io.m_factory && c.m_stackSize == 1 && c.m_stack != nullptr && c.m_stack[0] == nullptr && c.m_device != nullptr);
    CHECK(c.m_spaces.m_poolSize == 8 && c.m_defaultGroup.m_used == 1);
    CHECK(c.m_buffers.m_blockSize == 0x8000 && c.m_buffers.m_count == 4 && c.m_buffers.m_size == 0x20000);
    CHECK(c.m_buffers.m_blocks[3].m_data == c.m_buffers.m_data + 0x18000 && c.m_buffers.m_blocks[3].m_size == 0x8000 && c.m_buffers.m_blocks[3].m_flags == 4);
    CHECK(((unsigned int*)c.m_buffers.m_blocks)[-1] == 4);
    CHECK(AtIoContext::s_instance == &c && AtIoFileRef::s_context == &c && AtIoContext::s_context2C6328 == &c && AtIoContext::s_context2C633C == &c);
    // A second Init of the context clears the first; Clear leaves the second of the two unnamed pointers.
    c.Init(io.m_factory, 2, 0x40001, 4, 0);
    CHECK(c.m_stackSize == 2 && c.m_device == nullptr && c.m_buffers.m_count == 8 && c.m_buffers.m_size == 0x40000);
    c.Clear();
    CHECK(AtIoContext::s_instance == nullptr && AtIoFileRef::s_context == nullptr && AtIoContext::s_context2C633C == nullptr && AtIoContext::s_context2C6328 == &c);
    CHECK(c.m_stack == nullptr && c.m_buffers.m_blocks == nullptr && c.m_buffers.m_count == 0);
    AtIoBuffer b;
    std::memset(&b, 0x11, sizeof b);
    b.m_flags = 0;
    unsigned char data[4];
    b.Init(data, 4);
    CHECK(b.m_unknown00 == 0x11111111 && b.m_unknown0C == 0 && b.m_data == data && b.m_flags == 4 && b.m_size == 4);  // not cleared first: not in use
    b.m_unknown00 = 7;
    b.Init(data, 2);
    CHECK(b.m_unknown00 == 0 && b.m_unknown14 == 0 && b.m_size == 2);
    CHECK(stop_of([&] { io.Init(&config, &below, &memory); }) == "AtIoSystem: Shutdown (0x000B3A20) is not recovered");
    CHECK(io.m_factory->Create(AtIoFileSystemFactory::kRoot, 0, 1) != nullptr);  // the host's
}

// ---- the chain --------------------------------------------------------------------------------------------------------

// Stand-ins for the layers above the I/O layer, which record their Init and go on.
static std::vector<std::string> g_order;
static std::vector<const void*> g_below;
#define LAYER3(Name, Base) \
    class Name : public Base { \
    public: \
        void Init(const void*, void* below, void*) override { g_order.push_back(#Base); g_below.push_back(below); } \
    }
#define LAYER2(Name, Base) \
    class Name : public Base { \
    public: \
        void Init(const void*, void* below) override { g_order.push_back(#Base); g_below.push_back(below); } \
    }
// The four of at/atlayers take their own configs.
#define LAYERC(Name, Base, Config) \
    class Name : public Base { \
    public: \
        void Init(const Config*, void* below, void*) override { g_order.push_back(#Base); g_below.push_back(below); } \
    }
LAYERC(Math, AtMathSystem, AtMathConfig);
LAYERC(Data, AtDataManagementSystem, AtDataManagementConfig);
LAYERC(Script, AtScriptSystem, AtScriptConfig);
LAYERC(Kinematics, AtKinematicsSystem, AtKinematicsConfig);
LAYER3(Controller, AppControllerLayer);
LAYER2(Sound, AppSoundLayer);
LAYER3(Video, AppVideoLayer);
LAYER3(Scene, AppSceneLayer);
LAYER2(Ui, AppUserInterfaceLayer);
class Graphics : public AppDevice {
public:
    void Init(AtGraphicsConfig*, void* below, void*) override { g_order.push_back("AppDevice"); g_below.push_back(below); }
};

static void test_chain() {
    GmNameMap::s_heap = 0;
    GmNameSpaceDb::s_heap = 0;
    AtIoStream::s_heap = 0;
    AtIoSystemFactory::s_instance.m_running = 0;
    App* app = new App;
    App::s_instance = app;
    // The real chain stops at the first layer that is not recovered.
    std::string stopped = stop_of([&] { app->StartUpUserInterface(); });
    std::printf("chain stopped: %s\n", stopped.c_str());
    CHECK(stopped == "Graphics layer: the graphics factory's start (0x000C7C60: the render function tables and the factory at 0x003ADF64) is not recovered");
    CHECK(app->m_memory.m_started == 1 && AtHeapManager::s_instance == &app->m_memory.m_manager && app->m_memory.m_region != nullptr);
    for (int i = 0; i < 4; i++) CHECK(app->m_heaps[i] != nullptr && app->m_heaps[i]->m_size == app->m_heapConfig[i].m_size);
    CHECK(app->m_events.m_below == &app->m_memory && AtEventSystem::s_heap3ADF8C == app->m_heaps[0]);
    CHECK(app->m_network.m_below == &app->m_events && app->m_network.m_enabled == 0);
    CHECK(app->m_io.m_below == &app->m_network && app->m_io.m_factory == &AtIoSystemFactory::s_instance && app->m_io.m_kind == 2);
    AtIoContext& c = app->m_io.m_context;
    CHECK(AtIoContext::s_instance == &c && c.m_stackSize == 6 && c.m_spaces.m_poolSize == 0x10 && c.m_device != nullptr);
    CHECK(c.m_buffers.m_count == 8 && c.m_buffers.m_blockSize == 0x8000);
    CHECK(AtIoJamFileSystem::s_heap == app->m_heaps[0] && GmNameMap::s_heap == app->m_heaps[0] && AtIoStream::s_heap == app->m_heaps[0]);
    std::printf("heaps after the I/O layer: %u bytes of the four used\n", app->HeapsUsed());

    // Every layer, with stand-ins above the I/O layer: the order, and what each is handed.
    App* b = new App;
    App::s_instance = b;
    new (&b->m_math) Math;
    new (&b->m_dataManagement) Data;
    new (&b->m_script) Script;
    new (&b->m_kinematics) Kinematics;
    new (b->m_device.m_bytes) Graphics;
    b->m_device->m_flags = 0;
    new (b->m_controller.m_bytes) Controller;
    new (b->m_sound.m_bytes) Sound;
    new (b->m_video.m_bytes) Video;
    new (b->m_scene.m_bytes) Scene;
    new (b->m_userInterface.m_bytes) Ui;
    stopped = stop_of([&] { b->StartUpUserInterface(); });
    CHECK(stopped == "App start-up: the user interface layer's config (slot 56, 0x0008DB30) is not recovered");
    const char* order[] = {"AtMathSystem", "AtDataManagementSystem", "AtScriptSystem", "AtKinematicsSystem", "AppDevice", "AppControllerLayer", "AppSoundLayer", "AppVideoLayer", "AppSceneLayer"};
    const void* below[] = {&b->m_io, &b->m_math, &b->m_dataManagement, &b->m_script, &b->m_kinematics, b->m_device.m_bytes, b->m_controller.m_bytes, b->m_sound.m_bytes, b->m_video.m_bytes};
    CHECK(g_order.size() == 9);
    for (size_t i = 0; i < g_order.size() && i < 9; i++) {
        CHECK(g_order[i] == order[i]);
        CHECK(g_below[i] == below[i]);
    }
    // A layer already running shuts App down first: App's Shutdown is not recovered.
    b->m_sceneRunning = 1;
    CHECK(stop_of([&] { b->StartUpScene(); }) == "App: Shutdown (0x0008ED60) is not recovered");
    b->m_sceneRunning = 0;
    b->m_soundFlags = 2;  // only bit 0 counts
    g_order.clear();
    CHECK(stop_of([&] { b->StartUpSound(); }) == "App: Shutdown (0x0008ED60) is not recovered");  // not at the sound layer: at the base, running since the first run
    App::s_instance = nullptr;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    test_configs();
    test_network();
    test_event();
    test_io();
    test_chain();
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("atap: all checks passed\n");
    return 0;
}
