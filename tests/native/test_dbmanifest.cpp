// Host test of the level manifest's base words and items (gm/dbmanifest) through the real text tokenizer and the real level manifest, on any pointer size. Not evidence of a byte match.
// Every function of src/gm/GmLevelManifestHost.cpp and src/gm/GmDatabaseManifestHost.cpp is this file's, and writes what it was asked into a log, which is checked. The sound and graphics
// items load through the real AtAudioDatabase and AtDatabaseLoader into this file's databases, the bounding geometry and the registry through the real classes, from written files. No game data.
#include <cstdio>
#include <cstring>
#include <string>

#include "at/AtHeap.h"
#include "at/AtIoFileSystem.h"
#include "gm/GmAppSettings.h"
#include "gm/GmAudioData.h"
#include "gm/GmDatabaseManifestItems.h"
#include "gm/GmLevelManager.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static std::string g_log;
static void note(const char* what, const char* arg = "") {
    g_log += what;
    if (arg[0]) {
        g_log += "(";
        g_log += arg;
        g_log += ")";
    }
    g_log += " ";
}

// ---- the seams of src/gm/GmLevelManifestHost.cpp --------------------------------------------------------------------------
static int g_object;
void GmAudioFreeDatabase(void*) { note("free-sound-db"); }
void GmRegistryRelease(void*) { note("release-registry"); }
void GmBehaviorList::Prepare() {}
GmBehaviorList* GmBehaviorListOwner::LoadAnimations(const char*) { return 0; }
GmBehaviorList* GmBehaviorListOwner::LoadSounds(const char*) { return 0; }
void GmBehaviorListOwner::Release(GmBehaviorList*) {}
GmScript* GmScriptOwner::Load(const char*) { return 0; }
void GmScriptOwner::Release(GmScript*) {}
GmTriggerTable::GmTriggerTable() {}
GmTriggerTable::~GmTriggerTable() {}
void GmTriggerTable::Slot0() {}
void GmTriggerTable::Unload() {}
void GmTriggerTable::Load(const char*) {}
GmSubscriberTable::GmSubscriberTable() {}
GmSubscriberTable::~GmSubscriberTable() {}
void GmSubscriberTable::Slot0() {}
void GmSubscriberTable::Unload() {}
void GmSubscriberTable::Load(const char*) {}
GmVfxDatabase* GmVfxManager::Load(const char* path) { note("vfx", path); return 0; }
void GmVfxManager::Delete(GmVfxDatabase*) {}
void GmDynamicNodeLoader::Load(const char*, int, AtScene*) {}
void GmDynamicNodeLoader::Unload(AtScene*, int) {}
void AtScene::InsertInto(AtSpaceGraph*) { note("insert-node"); }
void AtScene::RemoveFrom(AtSpaceGraph*) { note("remove-node"); }
void* GmAudioAllocateNodeBank() { return &g_object; }
void GmAudioLoadNodes(void*, const char*) {}
void GmAudioFreeNodes(void*) {}
void GmAudioReleaseNodeBank(void*) {}
void GmAudioRegisterNodes(void*) {}
void* GmAudioLoadInstanceSounds(const char*) { return 0; }
void GmAudioFreeInstanceSounds(void*) {}
void GmAudioRegisterInstanceSounds(void*) {}
void* GmAudioLoadCollisionSounds(const char*) { return 0; }
void GmAudioFreeCollisionSounds(void*) {}
void GmMusicChain::Load(const char*, int) {}
void GmMusicChain::Clear() {}
GmMusicChain* GmMusicManager::Allocate() { return 0; }
void GmMusicManager::Stop(GmMusicChain*) {}
void GmMusicManager::Free(GmMusicChain*) {}

// ---- the seams of src/gm/GmDatabaseManifestHost.cpp -----------------------------------------------------------------------
void AtSceneDatabaseLoader::Load(AtNameTableOwner*, const char* name, int mode) { note("scene", name); CHECK(mode == 0); }
void AtCinemaDatabase::Load(GmCinemaOwner*, const char* name, int) { note("cinema", name); }
void AtEventDatabase::Load(AtDataRegistryOwner*, const char* name, int) { note("event", name); }
void GmRegistryNames::Add(AtDataRegistry* registry) { note("add-registry", registry->m_loaded ? "loaded" : "not"); }
void GmSpaceGraphOwner::SetGraph(AtSpaceGraph* graph) { note("set-graph"); m_graph = graph; }

namespace {

// ---- the makers: other branches' classes, here databases that only log ------------------------------------------------------
class SoundDatabase : public AtAudioDatabase {
public:
    void AllocateBankDatabases() override { note("banks"); }
    void AllocateSegmentDatabases() override { note("segments"); }
    void AllocateEmitterDatabases() override { note("emitters"); }
    void AllocateNestedDatabases() override { note("nested"); }
    AtAudioBankDatabase* BankDatabaseAt(unsigned int) override { return 0; }
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int) override { return 0; }
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int) override { return 0; }
    AtAudioDatabase* NestedDatabaseAt(unsigned int) override { return 0; }
};

class SoundMaker : public GmAudioDatabaseMaker {
public:
    SoundDatabase databases[4];
    int made = 0;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    AtAudioDatabase* NewDatabase() override { note("new-sound-db"); return made < 4 ? &databases[made++] : 0; }
    void DeleteDatabase(AtAudioDatabase*) override { note("delete-sound-db"); }
};

class Loader : public AtDatabaseLoader {
public:
    void MakeTextures() override {}
    void MakeMaterials() override {}
    void MakeShaders() override {}
    void MakeMeshes() override {}
    void MakeLights() override {}
    void Slot5() override {}
    void MakeFonts() override {}
    void MakeImages() override {}
    void MakeVertexProcesses() override {}
    void MakePixelProcesses() override {}
    void MakeLoaders() override {}
    AtTextureDatabase* TextureAt(unsigned int) override { return 0; }
    AtMaterialDatabase* MaterialAt(unsigned int) override { return 0; }
    AtShaderDatabase* ShaderAt(unsigned int) override { return 0; }
    AtMeshDatabase* MeshAt(unsigned int) override { return 0; }
    AtLightDatabase* LightAt(unsigned int) override { return 0; }
    AtFontDatabase* FontAt(unsigned int) override { return 0; }
    AtImageDatabase* ImageAt(unsigned int) override { return 0; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int) override { return 0; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int) override { return 0; }
    AtDatabaseLoader* LoaderAt(unsigned int) override { return 0; }
};

class Graphics : public AppGraphics {
public:
    Loader loaders[2];
    int made = 0;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Slot12() override {}
    void Slot13() override {}
    void Slot14() override {}
    void Slot15() override {}
    void Slot16() override {}
    void Slot17() override {}
    void Slot18() override {}
    void Slot19() override {}
    void Slot20() override {}
    void Slot21() override {}
    void Slot22() override {}
    void Slot23() override {}
    void Slot24() override {}
    void Slot25() override {}
    void Slot26() override {}
    void Slot27() override {}
    void Slot28() override {}
    AtDatabaseLoader* NewLoader() override { note("new-loader"); return &loaders[made++ % 2]; }
    void DeleteLoader(AtDatabaseLoader*) override { note("delete-loader"); }
};

// ---- files: a written registry for .ADR, nothing for anything else --------------------------------------------------------
const char kAdr[] = "DataComponentTypes[ 1 ] { DataComponent { Type Int NumComponents 2 } }\nDataRegistry[ 1 ] { DataLink { Name \"Speed\" Type Int Data 5 } }\n";

class File : public AtIoFile {
public:
    const char* data = nullptr;
    size_t size = 0;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        const size_t have = size - m_position;
        if (n > have) n = (unsigned int)have;
        if (n) std::memcpy(out, data + m_position, n);
        m_position += n;
        if (done) *done = n;
        return n ? 0 : (int)AtIoFile::kEndOfFile;
    }
    int Write(const void*, unsigned int, unsigned int*) override { return 0; }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class Fs : public AtIoFileSystem {
public:
    File file;
    std::string opened;
    int Open(const char* path, int flags, AtIoFile** out) override {
        opened += path;
        opened += " ";
        const size_t n = std::strlen(path);
        const bool adr = n > 4 && std::strcmp(path + n - 4, ".ADR") == 0;
        file.data = adr ? kAdr : "";
        file.size = adr ? sizeof kAdr - 1 : 0;
        file.m_position = 0;
        file.m_flags = (flags & 0x21) ? 0x0C : 2;
        std::strncpy(file.m_path, path, sizeof file.m_path - 1);
        *out = &file;
        return 0;
    }
    int Close(AtIoFile*) override { return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char*) override { return 0; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

const char kText[] =
    "// Level: test\n"
    "SoundDatabaseLoaders[ 2 ]\n{\n \"Audio\\VO\\%s\\omni\"\n \"Audio\\Objects\\x\"\n}\n"
    "GraphicsDatabasesLoaders[ 1 ]\n{\n \"Art\\G\"\n}\n"
    "SceneDatabasesLoaders[ 3 ]\n{\n \"Art\\S1\" InSpaceGraph InsertInSnoopie\n \"Art\\S2\" NotInSpaceGraph\n \"Art\\S3\" InSpaceGraph\n}\n"
    "CinemaDatabaseLoaders[ 1 ]\n{\n \"Cin\\C\"\n}\n"
    "BoundingGeometryDatabase[ 1 ]\n{\n \"Geo\\B\"\n}\n"
    "EventDatabases[ 1 ]\n{\n \"Ev\\E\"\n}\n"
    "InstanceRegistries[ 1 ]\n{\n \"Reg\\R\"\n}\n"
    "VFXDatabases[ 1 ]\n{\n \"Art\\v.VFX\"\n}\n"
    "Videos\n";

void SetText(Memory& stream, char* text, size_t size) {
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + size - 1;
    stream.m_size = (unsigned int)(size - 1);
}

}  // namespace

int main() {
    alignas(AtHeapManager) static char managerSpace[sizeof(AtHeapManager)];
    AtHeapManager::s_instance = (AtHeapManager*)managerSpace;
    GmDatabaseManifest::s_heap = &g_heap;
    static Fs fs;
    static AtIoContext context;
    static AtIoFileSystem* stack[4] = {&fs};
    context.m_stack = stack;
    context.m_stackCount = 1;
    context.m_stackSize = 4;
    AtIoFileRef::s_context = &context;
    AtIoTokenFile::s_heap = &g_heap;
    AtIoStream::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtDataRegistry::SetHeap(&g_heap);
    AtDataComponent::SetHeap(&g_heap);
    AtDataDescriptor::SetHeap(&g_heap);
    AtDataLink::SetHeap(&g_heap);
    AtDataRegistryNameSpaces::SetHeap(&g_heap);
    AtAudioDatabase::SetHeap(&g_heap);
    AtDatabaseLoader::SetHeap(&g_heap);
    AtBoundingGeometryDatabase::SetHeap(&g_heap);

    // What start-up would have made.
    static GmAudioData audio;
    GmAudioData::s_instance = &audio;
    GmAudioData::s_heap = &g_heap;
    AtNameTableOwner soundOwner;
    GmAudioData::s_soundOwner = &soundOwner;
    static SoundMaker soundMaker;
    GmAudioDatabaseMaker::s_instance = &soundMaker;
    g_audioLanguage = "EnglishXYZ";  // only eight characters are copied
    static Graphics graphics;
    App* app = new App;
    app->m_graphics = &graphics;
    AtMaterialOwner materialOwner;
    app->m_materialOwner = &materialOwner;
    GmHistoryLogs logs = {};
    logs.m_app = app;
    GmHistoryLogs::s_instance = &logs;
    GmEventHome eventHome = {};
    GmEventHome::s_instance = &eventHome;
    static GmDataOwners dataOwners;
    dataOwners.m_registryOwner.m_spaces.Init(4);
    static GmDynamicNodeState nodeState;
    nodeState.m_dataOwners = &dataOwners;
    GmDynamicNodeState::s_instance = &nodeState;
    int registryNames;
    GmRegistryNames::s_instance = (GmRegistryNames*)&registryNames;
    static char levelManagerSpace[sizeof(GmLevelManager)];
    GmLevelManager::s_instance = (GmLevelManager*)levelManagerSpace;  // FindSpaceGraph does not read it
    GmSpaceGraphOwner graphOwner = {};
    GmSpaceGraphOwner::s_instance = &graphOwner;
    int vfxManager;
    GmVfxManager::s_instance = (GmVfxManager*)&vfxManager;
    g_usePlatformBinary = 1;

    char text[sizeof kText];
    std::memcpy(text, kText, sizeof kText);
    Memory stream;
    SetText(stream, text, sizeof text);
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);

    {
        GmLevelManifest manifest;
        manifest.Parse(&tokenizer);
        manifest.ReadTokens(&tokenizer);
        // The sound databases were loaded during the parse: the path is a format, and the language is cut to eight characters.
        CHECK(g_log == "new-sound-db new-sound-db ");
        CHECK(fs.opened == "Audio\\VO\\EnglishX\\omni.AOD Audio\\Objects\\x.AOD ");
        if (g_fail) std::printf("[%s] [%s]\n", g_log.c_str(), fs.opened.c_str());
        g_log.clear();
        fs.opened.clear();
        CHECK(manifest.m_soundDatabaseCount == 2 && manifest.m_graphicsDatabaseCount == 1 && manifest.m_sceneDatabaseCount == 3 && manifest.m_cinemaDatabaseCount == 1);
        CHECK(manifest.m_boundingGeometryCount == 1 && manifest.m_eventCount == 1 && manifest.m_registryCount == 1 && manifest.m_vfxCount == 1);
        GmSoundDatabaseItem* sound = static_cast<GmSoundDatabaseItem*>(manifest.m_soundDatabases);
        CHECK(sound[0].m_ran == 1 && sound[0].m_database == &soundMaker.databases[0] && sound[0].m_maker == &soundMaker && sound[0].m_owner == &soundOwner);
        CHECK(std::strcmp(sound[0].m_extension, "AOD") == 0 && std::strcmp(sound[0].m_path, "Audio\\VO\\EnglishX\\omni") == 0);
        CHECK(soundMaker.databases[0].m_owner == &soundOwner);
        CHECK(audio.m_databases[0] == &soundMaker.databases[0] && audio.m_databases[1] == &soundMaker.databases[1] && audio.m_databases[2] == 0);
        GmGraphicsDatabaseItem* gfx = static_cast<GmGraphicsDatabaseItem*>(manifest.m_graphicsDatabases);
        CHECK(gfx[0].m_maker == &graphics && gfx[0].m_owner == &materialOwner && gfx[0].m_loader == 0 && std::strcmp(gfx[0].m_extension, "AGD") == 0);
        GmSceneDatabaseItem* scene = static_cast<GmSceneDatabaseItem*>(manifest.m_sceneDatabases);
        CHECK(scene[0].m_inSpaceGraph == 1 && scene[1].m_inSpaceGraph == 0 && scene[2].m_inSpaceGraph == 1);
        CHECK(scene[1].m_owner == &app->m_sceneOwner && std::strcmp(scene[2].m_path, "Art\\S3") == 0 && std::strcmp(scene[2].m_extension, "ASD") == 0);
        GmCinemaDatabaseItem* cinema = static_cast<GmCinemaDatabaseItem*>(manifest.m_cinemaDatabases);
        CHECK(cinema[0].m_owner == &app->m_cinemaOwner && std::strcmp(cinema[0].m_extension, "ACD") == 0);
        GmBoundingGeometryItem* geometry = static_cast<GmBoundingGeometryItem*>(manifest.m_boundingGeometry);
        CHECK(geometry[0].m_owner == &app->m_sceneOwner && std::strcmp(geometry[0].m_extension, "ASB") == 0);
        GmEventDatabaseItem* events = static_cast<GmEventDatabaseItem*>(manifest.m_events);
        CHECK(events[0].m_owner == &eventHome.m_owner && std::strcmp(events[0].m_path, "Ev\\E") == 0 && std::strcmp(events[0].m_extension, "AEE") == 0);
        GmRegistryItem* registry = static_cast<GmRegistryItem*>(manifest.m_registries);
        CHECK(registry[0].m_manager == GmRegistryNames::s_instance && registry[0].m_parent == &nodeState.m_registry && registry[0].m_owner == &dataOwners.m_registryOwner);
        CHECK(std::strcmp(registry[0].m_extension, "ADR") == 0);

        manifest.AfterParse();
        while (manifest.m_head != 0) manifest.RunNext();
        // The base's arrays first, in the order of AfterParse; the sound databases do not run twice; then the level's; then Finished, with no space graph to find.
        CHECK(g_log == "event(Ev\\E) add-registry(loaded) new-loader scene(Art\\S1) scene(Art\\S2) scene(Art\\S3) cinema(Cin\\C) vfx(Art\\v.VFX) ");
        CHECK(fs.opened == "Reg\\R.ADR Art\\G.AGD Geo\\B.ASB ");
        if (g_fail) std::printf("[%s] [%s]\n", g_log.c_str(), fs.opened.c_str());
        g_log.clear();
        CHECK(gfx[0].m_loader == &graphics.loaders[0] && graphics.loaders[0].m_owner == &materialOwner);
        CHECK(registry[0].m_registry.m_loaded == 1 && registry[0].m_registry.m_parent == &nodeState.m_registry);
        CHECK(graphOwner.m_graph == 0);

        // Finished again, with a space graph in the second scene's nested loader and a node in the first and third: the graph found is set, and the scenes marked InSpaceGraph go in.
        AtSceneDatabaseLoader nested;
        AtSpaceGraph* graph = (AtSpaceGraph*)&g_object;
        nested.m_spaceGraphs = graph;
        nested.m_spaceGraphCount = 1;
        scene[1].m_database.m_loaders = &nested;
        scene[1].m_database.m_loaderCount = 1;
        AtScene* node = (AtScene*)&g_object;
        scene[0].m_database.m_nodes = node;
        scene[0].m_database.m_nodeCount = 1;
        scene[2].m_database.m_nodes = node;
        scene[2].m_database.m_nodeCount = 2;
        CHECK(GmLevelManager::s_instance->FindSpaceGraph(&scene[0].m_database) == 0);
        CHECK(GmLevelManager::s_instance->FindSpaceGraph(&scene[1].m_database) == graph);
        manifest.GmDatabaseManifest::Finished();
        CHECK(g_log == "set-graph insert-node insert-node insert-node ");
        CHECK(graphOwner.m_graph == graph);
        if (g_fail) std::printf("[%s]\n", g_log.c_str());
        g_log.clear();
        // What a real load would have made is not this test's to free.
        nested.m_spaceGraphs = 0;
        nested.m_spaceGraphCount = 0;
        scene[1].m_database.m_loaders = 0;
        scene[1].m_database.m_loaderCount = 0;
        scene[0].m_database.m_nodeCount = 0;
        scene[0].m_database.m_nodes = 0;
        // Leaving: the nested loaders first, then the nodes.
        nested.m_nodes = node;
        nested.m_nodeCount = 1;
        scene[2].m_database.m_loaders = &nested;
        scene[2].m_database.m_loaderCount = 1;
        scene[2].m_database.RemoveFrom(graph);
        CHECK(g_log == "remove-node remove-node remove-node ");
        g_log.clear();
        nested.m_nodes = 0;
        nested.m_nodeCount = 0;
        scene[2].m_database.m_loaders = 0;
        scene[2].m_database.m_loaderCount = 0;
        scene[2].m_database.m_nodes = 0;
        scene[2].m_database.m_nodeCount = 0;
    }
    // Destroyed: the level's arrays, then the base's: cinema, scenes (leaving the graph, with nothing in them), bounding geometry, the sound databases given back, graphics, events, registries.
    CHECK(g_log == "free-sound-db delete-sound-db free-sound-db delete-sound-db delete-loader release-registry ");
    if (g_fail) std::printf("[%s]\n", g_log.c_str());
    g_log.clear();

    // A word given twice keeps the second array; a count of none makes an empty one; a level word nothing handles goes by.
    {
        const char again[] = "EventDatabases[ 1 ] { \"A\" } EventDatabases[ 0 ] { } Node SceneDatabasesLoaders[ 2 ] { \"x\" NotInSpaceGraph InsertInSnoopie \"y\" InSpaceGraph }";
        char buffer[sizeof again];
        std::memcpy(buffer, again, sizeof again);
        Memory s;
        SetText(s, buffer, sizeof buffer);
        AtIoTextTokenizer t;
        t.SetStream(&s);
        GmLevelManifest manifest;
        manifest.Parse(&t);
        manifest.ReadTokens(&t);
        CHECK(manifest.m_eventCount == 0 && manifest.m_events != 0);
        GmSceneDatabaseItem* scene = static_cast<GmSceneDatabaseItem*>(manifest.m_sceneDatabases);
        CHECK(manifest.m_sceneDatabaseCount == 2 && std::strcmp(scene[0].m_path, "x") == 0 && scene[0].m_inSpaceGraph == 0);
    }
    // The registry of sound databases is full at 60.
    {
        GmAudioData* full = new GmAudioData();
        for (int i = 0; i < GmAudioData::kDatabases; i++) full->RegisterDatabase(&soundMaker.databases[0]);
        full->RegisterDatabase(&soundMaker.databases[1]);
        CHECK(full->m_databases[59] == &soundMaker.databases[0]);
        delete full;
    }

    if (g_fail) return 1;
    std::printf("test_dbmanifest: all checks passed\n");
    return 0;
}
