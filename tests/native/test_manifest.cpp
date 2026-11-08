// Host test of the level manifest through the real text tokenizer, on any pointer size. Not evidence of a byte match. Every function of src/gm/GmLevelManifestHost.cpp and
// src/gm/GmLevelManifestNotes.cpp is this file's, and writes what it was asked into a log, which is checked. No game data.
#include <cstdio>
#include <cstring>
#include <string>

#include "at/AtHeap.h"
#include "gm/GmLevelManifest.h"

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

// ---- the seams ----------------------------------------------------------------------------------------------------------
static int g_object;  // stands for anything loaded
static int g_baseWords;
void GmDatabaseManifest::OnToken(int token, AtIoTokenizer* tokenizer) {
    // The base's words are not recovered; this skips one, as a stand-in, and counts it.
    g_baseWords++;
    if (token != kSoundDatabaseLoaders) return;
    const int count = tokenizer->ExpectCount();
    for (int i = 0; i < count; i++) tokenizer->ExpectString();
    tokenizer->ExpectRightCurly();
}
void GmDatabaseManifest::Finished() { note("base-finished"); }
void GmDatabaseItemsDelete(int, GmManifestItem*) { note("base-delete"); }
void GmAudioFreeDatabase(void*) {}
void GmSceneItemLeave(void*, AtSpaceGraph*) {}
void GmRegistryRelease(void*) {}
void GmBehaviorList::Prepare() { note("prepare"); }
GmBehaviorList* GmBehaviorListOwner::LoadAnimations(const char* path) { note("animations", path); return (GmBehaviorList*)&g_object; }
GmBehaviorList* GmBehaviorListOwner::LoadSounds(const char* path) { note("sounds", path); return 0; }
void GmBehaviorListOwner::Release(GmBehaviorList*) { note("release-list"); }
GmScript* GmScriptOwner::Load(const char* path) { note("script", path); return (GmScript*)&g_object; }
void GmScriptOwner::Release(GmScript*) { note("release-script"); }
GmTriggerTable::GmTriggerTable() {}
GmTriggerTable::~GmTriggerTable() {}
void GmTriggerTable::Slot0() {}
void GmTriggerTable::Unload() { note("unload-triggers"); }
void GmTriggerTable::Load(const char* path) { note("triggers", path); }
GmSubscriberTable::GmSubscriberTable() {}
GmSubscriberTable::~GmSubscriberTable() {}
void GmSubscriberTable::Slot0() {}
void GmSubscriberTable::Unload() { note("unload-subscribers"); }
void GmSubscriberTable::Load(const char* path) { note("subscribers", path); }
GmVfxDatabase* GmVfxManager::Load(const char* path) { note("vfx", path); return (GmVfxDatabase*)&g_object; }
void GmVfxManager::Delete(GmVfxDatabase*) { note("delete-vfx"); }
void GmDynamicNodeLoader::Load(const char* path, int, AtScene*) { note("node", path); }
void GmDynamicNodeLoader::Unload(AtScene*, int flag) { note("unload-node", flag ? "1" : "0"); }
void AtScene::InsertInto(AtSpaceGraph*) { note("insert"); }
void AtScene::RemoveFrom(AtSpaceGraph*) { note("remove"); }
void* GmAudioAllocateNodeBank() { return &g_object; }
void GmAudioLoadNodes(void*, const char* path) { note("nodes", path); }
void GmAudioFreeNodes(void*) { note("free-nodes"); }
void GmAudioReleaseNodeBank(void*) { note("release-bank"); }
void GmAudioRegisterNodes(void* bank) { note("register-nodes", bank ? "bank" : "none"); }
void* GmAudioLoadInstanceSounds(const char* path) { note("instance", path); return &g_object; }
void GmAudioFreeInstanceSounds(void*) { note("free-instance"); }
void GmAudioRegisterInstanceSounds(void*) { note("register-instance"); }
void* GmAudioLoadCollisionSounds(const char* path) { note("collision", path); return &g_object; }
void GmAudioFreeCollisionSounds(void*) { note("free-collision"); }
void GmMusicChain::Load(const char* path, int) { note("music", path); }
void GmMusicChain::Clear() { note("clear-music"); }
GmMusicChain* GmMusicManager::Allocate() { return (GmMusicChain*)&g_object; }
void GmMusicManager::Stop(GmMusicChain*) { note("stop-music"); }
void GmMusicManager::Free(GmMusicChain*) { note("free-music"); }

// The sound notes: a stand-in item of the same size, which logs.
class GmSoundNoteItem : public GmManifestItem {
public:
    GmSoundNoteItem() { m_extension[0] = 0; m_path[0] = 0; m_next = 0; }
    void Unknown0() override {}
    void Run() override { note("notes", m_path); }
    int m_bank = 0;
};
GmSoundNoteItem* GmSoundNoteItemsNew(unsigned int count) { return new GmSoundNoteItem[count]; }
GmManifestItem* GmSoundNoteItemAt(GmSoundNoteItem* items, unsigned int index) { return &items[index]; }
void GmSoundNoteItemConfigure(GmManifestItem* item, const GmManifestItemRequest* request) { std::strcpy(item->m_path, request->m_path); std::strcpy(item->m_extension, "GON"); }
void GmSoundNoteItemRegister(GmManifestItem*) { note("register-notes"); }
void GmSoundNoteItemsDelete(GmSoundNoteItem* items) { note("delete-notes"); delete[] items; }

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

const char kText[] =
    "// Level: test\n"
    "SoundDatabaseLoaders[ 1 ]\n{\n \"Audio\\x\"\n}\n"
    "TriggerDatabases[ 1 ]\n{\n \"Design\\T\"\n}\n"
    "SubscriberDatabases[ 1 ]\n{\n \"Design\\S\"\n}\n"
    "VFXDatabases[ 2 ]\n{\n \"Art\\a.VFX\"\n \"Art\\b.VFX\"\n}\n"
    "SoundNodeDatabases[ 1 ]\n{\n \"Audio\\N\"\n}\n"
    "SoundNoteDatabases[ 1 ]\n{\n \"Audio\\O\"\n}\n"
    "MusicChainDatabases[ 1 ]\n{\n \"Audio\\M\"\n}\n"
    "BehaviorAnimationLists[ 1 ]\n{\n \"B\\A\"\n}\n"
    "BehaviorSoundLists[ 1 ]\n{\n \"B\\S\"\n}\n"
    "SmallCScripts[ 1 ]\n{\n \"C\\S\"\n}\n"
    "CollisionSoundNodeDatabases[ 1 ]\n{\n \"Audio\\C\"\n}\n"
    "InstanceSoundNodeDatabases[ 1 ]\n{\n \"Audio\\I\"\n}\n"
    "DynamicNodes[ 1 ]\n{\n \"D\\N\"\n}\n";

}  // namespace

int main() {
    alignas(AtHeapManager) static char managerSpace[sizeof(AtHeapManager)];
    AtHeapManager::s_instance = (AtHeapManager*)managerSpace;
    GmDatabaseManifest::s_heap = &g_heap;
    GmBehaviorListOwner owners[2];
    GmScriptOwner scripts;
    GmBehaviorSystem behavior = {};
    behavior.m_animationLists = &owners[0];
    behavior.m_soundLists = &owners[1];
    behavior.m_scripts = &scripts;
    GmBehaviorSystem::s_instance = &behavior;
    int vfxManager;
    GmVfxManager::s_instance = (GmVfxManager*)&vfxManager;
    int nodeLoader;
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)&nodeLoader;
    GmSpaceGraphOwner graphOwner = {};
    GmSpaceGraphOwner::s_instance = &graphOwner;
    GmDynamicNodeState nodeState = {};
    nodeState.m_unknown28 = 1;
    GmDynamicNodeState::s_instance = &nodeState;
    int music;
    GmMusicManager::s_instance = (GmMusicManager*)&music;

    char text[sizeof kText];
    std::memcpy(text, kText, sizeof kText);
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + sizeof(text) - 1;
    stream.m_size = sizeof(text) - 1;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);

    {
        GmLevelManifest manifest;
        CHECK(manifest.m_animationLists == 0 && manifest.m_instanceSoundCount == 0 && manifest.m_events == 0);
        manifest.Parse(&tokenizer);
        manifest.ReadTokens(&tokenizer);
        CHECK(g_baseWords == 2);  // SoundDatabaseLoaders, and the closing brace DynamicNodes leaves
        CHECK(manifest.m_triggerCount == 1 && manifest.m_vfxCount == 2 && manifest.m_dynamicNodeCount == 1);
        CHECK(std::strcmp(manifest.m_vfx[1].m_path, "Art\\b.VFX") == 0);
        CHECK(std::strcmp(manifest.m_vfx[1].m_extension, "VFX") == 0);
        CHECK(manifest.m_vfx[0].m_manager == GmVfxManager::s_instance);
        CHECK(manifest.m_animationLists[0].m_owner == &owners[0]);
        CHECK(manifest.m_soundLists[0].m_owner == &owners[1]);
        CHECK(manifest.m_scripts[0].m_owner == &scripts);
        CHECK(manifest.m_dynamicNodes[0].m_loader == GmDynamicNodeLoader::s_instance);
        CHECK(std::strcmp(manifest.m_instanceSounds[0].m_extension, "GOI") == 0);
        CHECK(std::strcmp(manifest.m_collisionSounds[0].m_path, "Audio\\C") == 0);

        manifest.AfterParse();
        while (manifest.m_head != 0) manifest.RunNext();
        // Configure unloads first, so the tables were asked once already.
        CHECK(g_log == "unload-triggers unload-subscribers animations(B\\A) prepare sounds(B\\S) script(C\\S) vfx(Art\\a.VFX) vfx(Art\\b.VFX) triggers(Design\\T) subscribers(Design\\S) node(D\\N) insert "
                       "nodes(Audio\\N) notes(Audio\\O) music(Audio\\M) collision(Audio\\C) instance(Audio\\I) base-finished register-nodes(bank) register-notes register-instance ");
        if (g_fail) std::printf("%s\n", g_log.c_str());
        g_log.clear();
        CHECK(manifest.m_soundNodes[0].m_loaded == 1);
        manifest.m_soundNodes[0].Run();  // once only
        CHECK(g_log.empty());
    }
    // Destroyed: everything given back, in the order of the arrays.
    CHECK(g_log == "release-list release-script unload-triggers unload-subscribers delete-vfx delete-vfx remove unload-node(1) free-nodes release-bank delete-notes stop-music clear-music free-music "
                   "free-collision free-instance ");
    if (g_fail) std::printf("%s\n", g_log.c_str());

    // A word given twice keeps only the second array, and a count of none makes an empty one.
    {
        const char twice[] = "VFXDatabases[ 1 ] { \"A\" } VFXDatabases[ 0 ] { }";
        char buffer[sizeof twice];
        std::memcpy(buffer, twice, sizeof twice);
        Memory s;
        s.m_base = s.m_cur = buffer;
        s.m_limit = buffer + sizeof(buffer) - 1;
        s.m_size = sizeof(buffer) - 1;
        AtIoTextTokenizer t;
        t.SetStream(&s);
        GmLevelManifest manifest;
        manifest.Parse(&t);
        manifest.ReadTokens(&t);
        CHECK(manifest.m_vfxCount == 0 && manifest.m_vfx != 0);
    }

    if (g_fail) return 1;
    std::printf("test_manifest: all checks passed\n");
    return 0;
}
