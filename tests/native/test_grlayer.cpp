// Host test of the graphics layer's object short of the hardware (at/grlayer), on any pointer size. Not evidence of a byte match.
//
// It supplies the layer's seams itself (so src/at/AtGraphicsLayerSeams.cpp is not linked): a stand-in factory whose start is logged, to run the path of
// Init a real build cannot pass on the host. What the stand-in returns is the test's, not the game's.
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtDatabaseLoader.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtGrD3DTextures.h"
#include "at/AtImage.h"
#include "at/AtMatrixDevice.h"
#include "at/AtImageDatabase.h"
#include "at/AtMatAnimation.h"
#include "at/AtMatAssignment.h"
#include "at/AtMaterial.h"
#include "at/AtMaterialDatabase.h"
#include "at/AtShader.h"
#include "at/AtShaderDatabase.h"
#include "at/AtTextureDatabase.h"
#include "gm/GmNameMap.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_heaps[64];
#define HEAP(i) ((void*)&g_heaps[i])

static std::vector<std::string> g_log;

// ---- the seams, as the test's -------------------------------------------------------------------------------------------------
struct Owner : AtMaterialOwner {
    void Slot1() override { g_log.push_back("owner slot 1"); }
};
static Owner g_owner;
static int g_factoryPart;
static AtGraphicsGlobals* g_savedSeen;
static void* g_destroyed;

void* AtGraphicsFactory::Slot0() { g_log.push_back("factory slot 0"); return &g_factoryPart; }
AtMaterialOwner* AtGraphicsFactory::CreateDevice() { g_log.push_back("factory slot 3"); return &g_owner; }
void AtGraphicsFactory::Destroy(void* object) { g_log.push_back("factory slot 4"); g_destroyed = object; }
AtDatabaseLoader* AtGraphicsFactory::DatabaseLoader() { g_log.push_back("BAD"); return nullptr; }
void AtGraphicsFactory::DeleteLoader(AtDatabaseLoader*) { g_log.push_back("BAD"); }
#define SLOT(n) \
    void AtGraphicsFactory::Slot##n() { g_log.push_back("BAD"); }
SLOT(1) SLOT(2) SLOT(5) SLOT(6) SLOT(7) SLOT(8) SLOT(9) SLOT(10) SLOT(11) SLOT(12) SLOT(13) SLOT(14) SLOT(15) SLOT(16) SLOT(17) SLOT(18) SLOT(19)
SLOT(20) SLOT(21) SLOT(22) SLOT(23) SLOT(24) SLOT(25) SLOT(26) SLOT(28)
#undef SLOT
AtCamera* AtGraphicsFactory::Camera2D() { g_log.push_back("BAD"); return nullptr; }
static AtGraphicsFactory g_factory;

AtGraphicsFactory* AtGraphicsStartFactory(AtGraphicsGlobals* saved, const AtGraphicsConfig*) {
    g_log.push_back("start");
    g_savedSeen = saved;
    return &g_factory;
}
void AtGraphicsStopFactory() { g_log.push_back("stop"); }
void AtGraphicsSystemBase::DeletingDestructor(unsigned int) { g_log.push_back("BAD"); }
void AtGraphicsSystemBase::SetDisplay(AtDisplayMode*, int, int, int, const AtPixelFormat*, int, int) { g_log.push_back("BAD"); }
void AtGraphicsSystem::DeletingDestructor(unsigned int) { g_log.push_back("BAD"); }
void AtGraphicsSystem::SetDisplay(AtDisplayMode*, int, int, int, const AtPixelFormat*, int, int) { g_log.push_back("BAD"); }
void AtGraphicsSystem::UpdateDisplay() { g_log.push_back("BAD"); }

// ---- cases ------------------------------------------------------------------------------------------------------------------
static void* const* config_words(const AtGraphicsConfig& c) { return (void* const*)&c.m_heap0C; }

static void test_tables() {
    void (*read)(AtMatrix*, AtIoTokenizer*) = g_atMatrix.Read;
    CHECK(AtMatrixTableGet() == &g_atMatrix && AtRenderTableGet() == &g_atRender);
    AtRenderTable t = {};
    t.m_matrix.Read = AtMatrixRead;
    t.m_extended[6] = HEAP(1);
    AtRenderTableSet(&t);
    CHECK(g_atRender.m_extended[6] == HEAP(1) && g_atMatrix.Read == AtMatrixRead && g_atMatrix.Copy == nullptr);
    // The globals: Save notes where the live tables are, Restore copies each onto itself and the render table's matrix part over the matrix table.
    AtGraphicsGlobals saved;
    CHECK(saved.m_matrix == nullptr && saved.m_render == nullptr && saved.m_unknown0C == 0);
    saved.Save();
    CHECK(saved.m_matrix == &g_atMatrix && saved.m_render == &g_atRender);
    g_atRender.m_matrix.Read = nullptr;
    saved.Restore();
    CHECK(g_atMatrix.Read == nullptr);  // as in the original: the render table's (here empty) matrix part wins
    g_atMatrix.Read = read;
    g_atRender.m_matrix.Read = read;
}

static void test_layer() {
    AtGraphicsSystem g;
    CHECK(g.m_flags == 0 && g.m_kind == 0 && g.m_factory == nullptr && g.m_materialOwner == nullptr && g.m_mode.m_format.m_bits == 0);
    CHECK(g.RunFrame() == 1 && g.Slot7() == 0);
    g.m_unknown3C = 7;
    CHECK(g.Slot7() == 7);
    g.m_unknown38 = 9;
    CHECK(g.Slot7() == 9);

    // Kind 0: nothing is brought up; no heap is set.
    AtGraphicsConfig c = {};
    c.m_heap = (AtHeap*)HEAP(0);
    c.m_unknown04 = 0x44;
    c.m_unknownDC = 0x55;
    c.m_unknownE8 = 0x66;
    c.m_unknownEC = 0x77;
    AtDatabaseLoader::s_heap = nullptr;
    g.Init(&c, HEAP(9), HEAP(10));
    CHECK(g.m_below == HEAP(9) && g.m_unknown28 == 0x44 && g.m_unknown34 == 0x55 && g.m_unknown38 == 0 && g.m_kind == 0);
    CHECK(g.m_flags == 0 && g.m_factory == nullptr && g.m_unknown40 == 0 && AtDatabaseLoader::s_heap == nullptr && g_log.empty());

    // Kind 1: taken as 2, written back; heaps, the factory's start, its slots 0 and 3, the config's words, running.
    c.m_unknown00 = 1;
    c.m_loaderHeap = HEAP(1);
    c.m_heap30 = HEAP(2);
    c.m_heap34 = HEAP(3);
    c.m_materialHeap = HEAP(4);
    GmNameMap::s_heap = HEAP(5);  // already set: kept
    GmNameSpaceDb::s_heap = nullptr;
    g.Init(&c, HEAP(9), HEAP(10));
    CHECK(c.m_unknown00 == 2 && g.m_kind == 2 && (g.m_flags & AtGraphicsSystem::kRunning) != 0);
    CHECK(g.m_factory == &g_factory && g.m_unknown18 == &g_factoryPart && g.m_materialOwner == &g_owner);
    CHECK(g.m_unknown40 == 0x66 && g.m_unknown48 == 0x77 && g.m_unknown34 == 0x55);
    CHECK(g_log.size() == 3 && g_log[0] == "start" && g_log[1] == "factory slot 0" && g_log[2] == "factory slot 3");
    CHECK(g_savedSeen != nullptr);
    CHECK(AtDatabaseLoader::s_heap == HEAP(1) && g_heap3B23DC == HEAP(2) && g_heap3B13CC == HEAP(2));
    CHECK(AtGrI2DFont::s_heap == HEAP(3) && g_heap3B23D8 == HEAP(3) && AtMaterialDatabase::s_heap == HEAP(4) && AtGrD3DMaterialDatabase::s_heap == HEAP(4));
    CHECK(GmNameMap::s_heap == HEAP(5) && GmNameSpaceDb::s_heap == HEAP(0));
    CHECK(AtTextureDatabase::s_heap == HEAP(0) && AtShader::s_heap == HEAP(0) && g_heap3B13D0 == HEAP(0) && g_heap2C7C38 == HEAP(0));

    // Running: a second Init shuts down first (the device given back, the factory stopped).
    g_log.clear();
    g.Init(&c, HEAP(8), HEAP(10));
    CHECK(g_log.size() >= 4 && g_log[0] == "owner slot 1" && g_log[1] == "factory slot 4" && g_log[2] == "stop" && g_log[3] == "start");
    CHECK(g_destroyed == &g_owner && g.m_below == HEAP(8) && (g.m_flags & 1) != 0);

    // Shutdown: everything cleared, the pixel format too.
    g_log.clear();
    g.m_format.m_bits = 32;
    g.m_unknown94 = 3;
    g.Shutdown();
    CHECK(g.m_flags == 0 && g.m_kind == 0 && g.m_factory == nullptr && g.m_materialOwner == nullptr && g.m_below == nullptr);
    CHECK(g.m_format.m_bits == 0 && g.m_unknown94 == 0 && g.m_unknown40 == 0 && g_log.size() == 3);
    // Shut down again, with nothing: no device, kind 0, so neither the factory nor its stop is reached.
    g_log.clear();
    g.Shutdown();
    CHECK(g_log.empty());
}

static void test_set_heaps() {
    // Each heap word alone, against the default: every field that matters sets something.
    AtGraphicsConfig c = {};
    c.m_heap = (AtHeap*)HEAP(0);
    GmNameMap::s_heap = nullptr;
    GmNameSpaceDb::s_heap = nullptr;
    void** words = (void**)&c.m_heap0C;
    for (int i = 0; i < 50; i++) words[i] = HEAP(1 + i);
    AtGraphicsSystem g;
    g.SetHeaps(&c);
    CHECK(g_heap3B1FC0 == HEAP(1) && g_heap3B1FBC == HEAP(2) && g_heap2C7C38 == HEAP(3) && AtDatabaseLoader::s_heap == HEAP(4));
    CHECK(g_heap3B13CC == HEAP(10) && g_heap3B23DC == HEAP(10) && AtFontDatabase::s_heap == HEAP(12) && AtImageDatabase::s_heap == HEAP(14));
    CHECK(GmNameMap::s_heap == HEAP(49) && GmNameSpaceDb::s_heap == HEAP(50) && g_heap3B13D0 == HEAP(46));
    CHECK(config_words(c)[0] == HEAP(1));
    // Each falls back to the default.
    for (int i = 0; i < 50; i++) words[i] = nullptr;
    GmNameMap::s_heap = nullptr;
    GmNameSpaceDb::s_heap = nullptr;
    g.SetHeaps(&c);
    CHECK(g_heap3B1FC0 == HEAP(0) && g_heap3B13CC == HEAP(0) && AtShaderDatabase::s_heap == HEAP(0) && GmNameMap::s_heap == HEAP(0));
    CHECK(AtProgShaderComponent::s_heap == HEAP(0) && AtMatAssignmentDatabase::s_heap == HEAP(0) && (void*)AtMatrixPoolXbox::s_heap == HEAP(0) && AtMatrixDeviceXbox::s_heap == HEAP(0));
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    test_tables();
    test_layer();
    test_set_heaps();
    for (const std::string& s : g_log) CHECK(s != "BAD");
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("grlayer: all checks passed\n");
    return 0;
}
