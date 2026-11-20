// Differential oracle for the level manifest's base words and items (gm/dbmanifest), Windows x86, local only: GmDatabaseManifest's OnToken and Finished, its seven kinds of item, the cinema
// and event databases' life cycles, the scene loader's two walks, GmLevelManager::FindSpaceGraph and GmAudioData::RegisterDatabase. See oracle_manifest.cpp and docs/verification.md.
//
//   oracle_dbmanifest <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// What a green result does not show, first. The level's own items and every function they load into are doubles on both sides, as in oracle_manifest.cpp. So are: the loads of scene, cinema and
// event databases (0x0002F7F0, 0x000C5B30, 0x000C9A00), which here fill in fake arrays so that Clear, the walks and Finished have something to work on; the registry names' Add (0x00071F20) and
// Release (0x00071DC0); the sound manager's FreeDatabase (0x00139F90); the space graph setter (0x00140E40). The makers of sound databases and of graphics database loaders are this file's
// classes, used on both sides, and the databases they make are this file's subclasses of the recovered AtAudioDatabase and AtDatabaseLoader, whose slots only log. The scene loader's
// constructor, destructor, Clear, SpaceGraphAt and LoaderAt are at/asd's, here stand-ins (src/gm/GmDatabaseManifestAsd.cpp) compared against the original's code. What runs for real on both
// sides, each side its own: AtAudioDatabase::Load, AtDatabaseLoader::Load and AtBoundingGeometryDatabase::Load on empty written files, AtDataRegistry::Load on a written registry, and the
// manifest's parser and queue. Nothing here says what a real .AOD, .AGD, .ASD or .ADR holds.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmAppSettings.h"
#include "gm/GmAudioData.h"
#include "gm/GmDatabaseManifestItems.h"
#include "gm/GmLevelManager.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 20) std::printf("DIFF %s: %s: original [%.200s], recovered [%.200s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

// ---- files: the .MNG on files[], anything else on a file of its own, by extension ------------------------------------------------------------------------------------------------------
static const char kNotesText[] =
    "SoundNotes[ 3 ]\n{\n Note \"Step\" { AnimationTrigger \"foot\" Segments[ 2 ] { \"a\" 0.25 \"b\" } }\n"
    " Note \"Jump\" { Singleton Stop \"Step\" VFXTrigger \"dust\" }\n Note \"Land\" { AnimationTrigger \"land\" Segments[ 1 ] { \"c\" } }\n}\n";
static const char kAdrText[] = "DataComponentTypes[ 1 ] { DataComponent { Type Int NumComponents 2 } }\nDataRegistry[ 1 ] { DataLink { Name \"Speed\" Type Int Data 5 } }\n";
static const char kAgdText[] = "MeshDatabase[ 1 ] { \"mesh\" }\n";
// With mode 0 the mesh would be read as text, which is the mesh database's work, not this suite's: that case reads an empty .AGD instead.
static const char* g_agd_text = kAgdText;
class Fs : public MemFs {
public:
    MemFile other[2];
    int Open(const char* path, int flags, AtIoFile** out) override {
        if (std::strstr(path, ".MNG")) return MemFs::Open(path, flags, out);
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(path, g_side);
        MemFile& f = other[g_side];
        f.side = g_side;
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = (flags & 0x21) ? 0x0C : 2;
        const char* text = std::strstr(path, ".GON") ? kNotesText : std::strstr(path, ".ADR") ? kAdrText : std::strstr(path, ".AGD") ? g_agd_text : "";
        f.data = (const unsigned char*)text;
        f.size = std::strlen(text);
        strncpy_s(f.m_path, path, _TRUNCATE);
        *out = &f;
        return 0;
    }
};
static Fs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static AtHeapManager g_manager[2];

static unsigned int __cdecl o_clock() { return 1000; }
unsigned int AtPlatformMilliseconds() { return 1000; }
// GmAppSettings.cpp comes in for its globals; its reader is not run here.
int AtPlatformLanguage() { return 1; }
int AtPlatformGameRegion() { return 1; }

// ---- fakes ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Fake {
    void** vptr;
    int id;
};
static Fake g_pool[2][4096];
static int g_pool_used[2];
static int g_null_from;  // Loads and allocations from the nth on (counted on each side) answer null; 0 never.
static int g_made[2];
static void* make_fake(void** vtable) {
    const int s = g_side;
    g_made[s]++;
    if (g_null_from && g_made[s] >= g_null_from) return nullptr;
    Fake* f = &g_pool[s][g_pool_used[s]++ % 4096];
    f->vptr = vtable;
    f->id = g_pool_used[s];
    return f;
}

// Arrays a double makes: registered in order on each side, so that a pointer into one is compared by its place.
static unsigned int g_block_seq;
struct Block {
    char* p;
    size_t size;
    unsigned int seq = ++g_block_seq;  // across both sides: a block freed on one side may be handed out again on the other
};
static std::vector<Block> g_blocks[2];

// The fixed objects of each side, by tag.
struct Range {
    const void* base[2];
    size_t size;
    uint32_t tag;
};
static std::vector<Range> g_ranges;

static GmAudioData* g_audio[2];
static void* g_cur[2];
static long long queue_place(const GmLevelManifest* m, const GmManifestItem* p);
static long long ident(const void* p) {
    if (!p) return 0;
    // The newest block that holds it, on either side: a block freed and handed out again is the later one.
    {
        const Block* best = nullptr;
        size_t index = 0;
        for (int s = 0; s < 2; s++) {
            for (size_t i = 0; i < g_blocks[s].size(); i++) {
                const Block& b = g_blocks[s][i];
                if ((const char*)p >= b.p && (const char*)p < b.p + b.size && (!best || b.seq > best->seq)) {
                    best = &b;
                    index = i;
                }
            }
        }
        if (best) return 0x50000000 + (long long)index * 0x10000 + ((const char*)p - best->p);
    }
    for (int s = 0; s < 2; s++) {
        if (g_audio[s] && p >= (const void*)&g_audio[s]->m_noteBanks[0] && p < (const void*)&g_audio[s]->m_noteBanks[GmAudioData::kNoteBanks])
            return 0x60000000 + ((const GmSoundNoteBank*)p - &g_audio[s]->m_noteBanks[0]);
        if (p >= (const void*)&g_pool[s][0] && p < (const void*)&g_pool[s][4096]) return 0x70000000 + ((const Fake*)p - &g_pool[s][0]);
        for (const Range& r : g_ranges) {
            if ((const char*)p >= (const char*)r.base[s] && (const char*)p < (const char*)r.base[s] + r.size) return ((long long)r.tag << 24) + ((const char*)p - (const char*)r.base[s]);
        }
        if (g_cur[s]) {
            const long long q = queue_place((const GmLevelManifest*)g_cur[s], (const GmManifestItem*)p);
            if (q >= 0) return 0x40000000 + q;
        }
    }
    return (long long)(uintptr_t)p;
}
static uint32_t hash(const char* s) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 0x80 && s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void note(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static void log_ptr(uint32_t a, const void* p) { note(a, (uint32_t)ident(p)); }

static GmBehaviorSystem g_behavior;
#define OWNER(n) ((void*)(uintptr_t)(0x00005123 + (n) * 0x100))

static long long item_place(const void* p) {
    if (!p || !g_cur[g_side]) return -1;
    const GmLevelManifest* m = (const GmLevelManifest*)g_cur[g_side];
    if (m->m_dynamicNodes && p >= (const void*)m->m_dynamicNodes) return (const char*)p - (const char*)m->m_dynamicNodes;
    return -2;
}

// ---- doubles: the level's items, as in oracle_manifest.cpp ------------------------------------------------------------------------------------------------------------------------------
static void __fastcall o_prepare(void* list, void*) { log_ptr('P', list); }
void GmBehaviorList::Prepare() { log_ptr('P', this); }
static void* __fastcall o_load_anim(void* owner, void*, const char* path) { note('A', (uint32_t)(uintptr_t)owner); note('h', hash(path)); return make_fake(nullptr); }
GmBehaviorList* GmBehaviorListOwner::LoadAnimations(const char* path) { note('A', (uint32_t)(uintptr_t)this); note('h', hash(path)); return (GmBehaviorList*)make_fake(nullptr); }
static void* __fastcall o_load_sounds(void* owner, void*, const char* path) { note('S', (uint32_t)(uintptr_t)owner); note('h', hash(path)); return make_fake(nullptr); }
GmBehaviorList* GmBehaviorListOwner::LoadSounds(const char* path) { note('S', (uint32_t)(uintptr_t)this); note('h', hash(path)); return (GmBehaviorList*)make_fake(nullptr); }
static void __fastcall o_release_list(void* owner, void*, void* list) { note('L', (uint32_t)(uintptr_t)owner); log_ptr('l', list); }
void GmBehaviorListOwner::Release(GmBehaviorList* list) { note('L', (uint32_t)(uintptr_t)this); log_ptr('l', list); }
static void* __fastcall o_load_script(void* owner, void*, const char* path) { note('C', (uint32_t)(uintptr_t)owner); note('h', hash(path)); return make_fake(nullptr); }
GmScript* GmScriptOwner::Load(const char* path) { note('C', (uint32_t)(uintptr_t)this); note('h', hash(path)); return (GmScript*)make_fake(nullptr); }
static void __fastcall o_release_script(void* owner, void*, void* s) { note('c', (uint32_t)(uintptr_t)owner); log_ptr('l', s); }
void GmScriptOwner::Release(GmScript* s) { note('c', (uint32_t)(uintptr_t)this); log_ptr('l', s); }

// The two tables an AET or AES item holds: a fake table on the original side, the class's own on the recovered side; both log alike. The first word after the table says who was built.
static void __fastcall ft_slot0(void*, void*) { note('!'); }
static void __fastcall ft_trigger_unload(void* self, void*) { note('U', ((uint32_t*)self)[1]); }
static void __fastcall ft_subscriber_unload(void* self, void*) { note('u', ((uint32_t*)self)[1]); }
static void* g_trigger_vtable[2] = {(void*)ft_slot0, (void*)ft_trigger_unload};
static void* g_subscriber_vtable[2] = {(void*)ft_slot0, (void*)ft_subscriber_unload};
static void* __fastcall o_trigger_ctor(void* self, void*) { *(void**)self = g_trigger_vtable; ((uint32_t*)self)[1] = 0x7171; note('k'); return self; }
static void __fastcall o_trigger_dtor(void* self, void*) { note('K', ((uint32_t*)self)[1]); }
static void __fastcall o_trigger_load(void* self, void*, const char* path) { note('T', ((uint32_t*)self)[1]); note('h', hash(path)); }
GmTriggerTable::GmTriggerTable() { *(uint32_t*)m_unknown04 = 0x7171; note('k'); }
GmTriggerTable::~GmTriggerTable() { note('K', *(uint32_t*)m_unknown04); }
void GmTriggerTable::Slot0() { note('!'); }
void GmTriggerTable::Unload() { note('U', *(uint32_t*)m_unknown04); }
void GmTriggerTable::Load(const char* path) { note('T', *(uint32_t*)m_unknown04); note('h', hash(path)); }
static void* __fastcall o_subscriber_ctor(void* self, void*) { *(void**)self = g_subscriber_vtable; ((uint32_t*)self)[1] = 0x7272; note('m'); return self; }
static void __fastcall o_subscriber_dtor(void* self, void*) { note('M', ((uint32_t*)self)[1]); }
static void __fastcall o_subscriber_load(void* self, void*, const char* path) { note('W', ((uint32_t*)self)[1]); note('h', hash(path)); }
GmSubscriberTable::GmSubscriberTable() { *(uint32_t*)m_unknown04 = 0x7272; note('m'); }
GmSubscriberTable::~GmSubscriberTable() { note('M', *(uint32_t*)m_unknown04); }
void GmSubscriberTable::Slot0() { note('!'); }
void GmSubscriberTable::Unload() { note('u', *(uint32_t*)m_unknown04); }
void GmSubscriberTable::Load(const char* path) { note('W', *(uint32_t*)m_unknown04); note('h', hash(path)); }

// A VFX database: the original deletes it through 0x000EB9D0, which runs; its slot 0 is the fake's.
static void* __fastcall fv_delete(void* self, void*, unsigned int flags) { log_ptr('x', self); note('x', flags); return self; }
static void* g_vfx_vtable[1] = {(void*)fv_delete};
static void* __fastcall o_load_vfx(void* mgr, void*, const char* path) { note('V', (uint32_t)(uintptr_t)mgr); note('h', hash(path)); return make_fake(g_vfx_vtable); }
GmVfxDatabase* GmVfxManager::Load(const char* path) { note('V', (uint32_t)(uintptr_t)this); note('h', hash(path)); return (GmVfxDatabase*)make_fake(g_vfx_vtable); }
void GmVfxManager::Delete(GmVfxDatabase* d) { if (d) { log_ptr('x', d); note('x', 1); } }

static void __fastcall o_node_load(void* loader, void*, const char* path, int unused, AtScene* scene) {
    note('n', (uint32_t)(uintptr_t)loader); note('h', hash(path)); note('n', (uint32_t)unused); note('n', (uint32_t)item_place(scene));
}
void GmDynamicNodeLoader::Load(const char* path, int unused, AtScene* scene) {
    note('n', (uint32_t)(uintptr_t)this); note('h', hash(path)); note('n', (uint32_t)unused); note('n', (uint32_t)item_place(scene));
}
static void __fastcall o_node_unload(void* loader, void*, AtScene* scene, int flag) { note('N', (uint32_t)(uintptr_t)loader); note('N', (uint32_t)item_place(scene)); note('N', (uint32_t)flag); }
void GmDynamicNodeLoader::Unload(AtScene* scene, int flag) { note('N', (uint32_t)(uintptr_t)this); note('N', (uint32_t)item_place(scene)); note('N', (uint32_t)flag); }
static void __fastcall o_insert(void* scene, void*, AtSpaceGraph* g) { log_ptr('I', scene); log_ptr('I', g); }
void AtScene::InsertInto(AtSpaceGraph* g) { log_ptr('I', this); log_ptr('I', g); }
static void __fastcall o_remove(void* scene, void*, AtSpaceGraph* g) { log_ptr('J', scene); log_ptr('J', g); }
void AtScene::RemoveFrom(AtSpaceGraph* g) { log_ptr('J', this); log_ptr('J', g); }

static void* __fastcall o_alloc_nodes(void*, void*) { note('1'); return make_fake(nullptr); }
void* GmAudioAllocateNodeBank() { note('1'); return make_fake(nullptr); }
static void __fastcall o_load_nodes(void*, void*, void* bank, const char* path) { log_ptr('2', bank); note('h', hash(path)); }
void GmAudioLoadNodes(void* bank, const char* path) { log_ptr('2', bank); note('h', hash(path)); }
static void __fastcall o_free_nodes(void*, void*, void* bank) { log_ptr('3', bank); }
void GmAudioFreeNodes(void* bank) { log_ptr('3', bank); }
static void __fastcall o_release_bank(void*, void*, void* bank) { log_ptr('4', bank); }
void GmAudioReleaseNodeBank(void* bank) { log_ptr('4', bank); }
static void __fastcall o_register_nodes(void*, void*, void* bank) { log_ptr('5', bank); }
void GmAudioRegisterNodes(void* bank) { log_ptr('5', bank); }
static void* __fastcall o_load_instance(void*, void*, const char* path) { note('6', hash(path)); return make_fake(nullptr); }
void* GmAudioLoadInstanceSounds(const char* path) { note('6', hash(path)); return make_fake(nullptr); }
static void __fastcall o_free_instance(void*, void*, void* d) { log_ptr('7', d); }
void GmAudioFreeInstanceSounds(void* d) { log_ptr('7', d); }
static void __fastcall o_register_instance(void*, void*, void* d) { log_ptr('8', d); }
void GmAudioRegisterInstanceSounds(void* d) { log_ptr('8', d); }
static void* __fastcall o_load_collision(void*, void*, const char* path) { note('9', hash(path)); return make_fake(nullptr); }
void* GmAudioLoadCollisionSounds(const char* path) { note('9', hash(path)); return make_fake(nullptr); }
static void __fastcall o_free_collision(void*, void*, void* d) { log_ptr('0', d); }
void GmAudioFreeCollisionSounds(void* d) { log_ptr('0', d); }

static void* __fastcall o_music_alloc(void* mgr, void*) { note('G', (uint32_t)(uintptr_t)mgr); return make_fake(nullptr); }
GmMusicChain* GmMusicManager::Allocate() { note('G', (uint32_t)(uintptr_t)this); return (GmMusicChain*)make_fake(nullptr); }
static void __fastcall o_chain_load(void* chain, void*, const char* path, int unused) { log_ptr('H', chain); note('h', hash(path)); note('H', (uint32_t)unused); }
void GmMusicChain::Load(const char* path, int unused) { log_ptr('H', this); note('h', hash(path)); note('H', (uint32_t)unused); }
static void __fastcall o_chain_clear(void* chain, void*) { log_ptr('Q', chain); }
void GmMusicChain::Clear() { log_ptr('Q', this); }
static void __fastcall o_music_stop(void* mgr, void*, void* chain) { note('E', (uint32_t)(uintptr_t)mgr); log_ptr('E', chain); }
void GmMusicManager::Stop(GmMusicChain* chain) { note('E', (uint32_t)(uintptr_t)this); log_ptr('E', chain); }
static void __fastcall o_music_free(void* mgr, void*, void* chain) { note('F', (uint32_t)(uintptr_t)mgr); log_ptr('F', chain); }
void GmMusicManager::Free(GmMusicChain* chain) { note('F', (uint32_t)(uintptr_t)this); log_ptr('F', chain); }

// ---- doubles: what the base's items reach and is not recovered --------------------------------------------------------------------------------------------------------------------------
static void __fastcall o_audio_free_db(void*, void*, void* db) { log_ptr('a', db); }
void GmAudioFreeDatabase(void* db) { log_ptr('a', db); }
static void __fastcall o_registry_release(void* names, void*, void* registry) { note('g', (uint32_t)(uintptr_t)names); log_ptr('g', registry); }
void GmRegistryNames::Release(AtDataRegistry* registry) { note('g', (uint32_t)(uintptr_t)this); log_ptr('g', registry); }
static void __fastcall o_registry_add(void* names, void*, AtDataRegistry* registry) { note('+', (uint32_t)(uintptr_t)names); log_ptr('+', registry); note('+', (uint32_t)registry->m_loaded); }
void GmRegistryNames::Add(AtDataRegistry* registry) { note('+', (uint32_t)(uintptr_t)this); log_ptr('+', registry); note('+', (uint32_t)registry->m_loaded); }
static void __fastcall o_set_graph(GmSpaceGraphOwner* owner, void*, AtSpaceGraph* graph) { log_ptr('$', owner); log_ptr('$', graph); owner->m_graph = graph; }
void GmSpaceGraphOwner::SetGraph(AtSpaceGraph* graph) { log_ptr('$', this); log_ptr('$', graph); m_graph = graph; }

// An array a database's Clear deletes through its first element's slot 0: a count ahead, and a table whose slot 0 logs and frees the block.
static void* __fastcall fe_delete(void* self, void*, unsigned int flags) {
    note('X', flags);
    uint32_t* block = (uint32_t*)self - 1;
    note('x', *block);
    note('x', ((Fake*)self)->id);
    std::free(block);
    return block;
}
static void* g_entry_vtable[1] = {(void*)fe_delete};
static int g_entry_ids[2];
static void* make_entries(unsigned int count, unsigned int size) {
    const bool was = g_recording;
    g_recording = false;
    uint32_t* block = (uint32_t*)std::calloc(1, 4 + (count ? count : 1) * size);
    block[0] = count;
    for (unsigned int i = 0; i < count; i++) {
        Fake* f = (Fake*)((char*)(block + 1) + i * size);
        f->vptr = g_entry_vtable;
        f->id = ++g_entry_ids[g_side];
    }
    g_blocks[g_side].push_back({(char*)(block + 1), (count ? count : 1) * size});
    g_recording = was;
    return block + 1;
}
static int g_loads[2];  // what each Load double fills in depends on how many came before, alike on both sides

// The scene loader's Load: logs, and fills in nodes and two other arrays so that Clear, the walks and Free have work.
static void scene_load(AtSceneDatabaseLoader* db, AtNameTableOwner* owner, const char* name, int mode) {
    note('s', (uint32_t)ident(db));
    log_ptr('s', owner);
    note('h', hash(name));
    note('s', (uint32_t)mode);
    const int n = g_loads[g_side]++ % 3;
    db->m_owner = owner;
    db->m_nodes = (AtScene*)make_entries((unsigned)n, sizeof(AtScene));
    db->m_nodeCount = (unsigned short)n;
    db->m_modifiers = (AtSceneDatabaseEntry*)make_entries((unsigned)(2 - n), 0x10);
    db->m_modifierCount = (unsigned short)(2 - n);
    if (n == 1) {
        db->m_textDraws = (AtSceneDatabaseEntry*)make_entries(0, 0x10);
    }
}
static void __fastcall o_scene_load(AtSceneDatabaseLoader* db, void*, AtNameTableOwner* owner, const char* name, int mode) { scene_load(db, owner, name, mode); }
void AtSceneDatabaseLoader::Load(AtNameTableOwner* owner, const char* name, int mode) { scene_load(this, owner, name, mode); }

static void cinema_load(AtCinemaDatabase* db, GmCinemaOwner* owner, const char* name, int unused) {
    note('k', (uint32_t)ident(db));
    log_ptr('k', owner);
    note('h', hash(name));
    note('k', (uint32_t)unused);
    const int n = g_loads[g_side]++ % 3;
    db->m_unknown04 = 1;
    db->m_entries08 = (AtCinemaDatabaseEntry*)make_entries((unsigned)n, 0x10);
    db->m_count1C = (unsigned short)n;
    db->m_entries14 = (AtCinemaDatabaseEntry*)make_entries(1, 0x10);
    db->m_count22 = 1;
    if (n != 0) db->m_entries18 = (AtCinemaDatabaseEntry*)make_entries(2, 0x10);
}
static void __fastcall o_cinema_load(AtCinemaDatabase* db, void*, GmCinemaOwner* owner, const char* name, int unused) { cinema_load(db, owner, name, unused); }
void AtCinemaDatabase::Load(GmCinemaOwner* owner, const char* name, int unused) { cinema_load(this, owner, name, unused); }

// The event database's: every third leaves the list empty, which Clear then passes over entirely.
static void event_load(AtEventDatabase* db, AtDataRegistryOwner* owner, const char* name, int unused) {
    note('e', (uint32_t)ident(db));
    log_ptr('e', owner);
    note('h', hash(name));
    note('e', (uint32_t)unused);
    const int n = g_loads[g_side]++ % 3;
    if (n == 0) return;
    db->m_list.m_entries = (AtEvent*)make_entries((unsigned)(n - 1), 0x10);
    db->m_list.m_count = (unsigned)(n - 1);
    db->m_subscriptions = (AtEventSubscription*)std::malloc(16);
    db->m_subscriptionCount = 0x24;
}
static void __fastcall o_event_load(AtEventDatabase* db, void*, AtDataRegistryOwner* owner, const char* name, int unused) { event_load(db, owner, name, unused); }
void AtEventDatabase::Load(AtDataRegistryOwner* owner, const char* name, int unused) { event_load(this, owner, name, unused); }

// ---- the makers: one of each to a side, used through their slots on both ----------------------------------------------------------------------------------------------------------------
class SoundDb : public AtAudioDatabase {
public:
    int side = 0;
    void AllocateBankDatabases() override { note('1', (uint32_t)ident(this)); }
    void AllocateSegmentDatabases() override { note('2', (uint32_t)ident(this)); }
    void AllocateEmitterDatabases() override { note('3', (uint32_t)ident(this)); }
    void AllocateNestedDatabases() override { note('4', (uint32_t)ident(this)); }
    ~SoundDb() override {}
    void Clear() override { note('5', (uint32_t)ident(this)); }
    AtAudioBankDatabase* BankDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioDatabase* NestedDatabaseAt(unsigned int) override { return nullptr; }
};
static SoundDb g_sound_dbs[2][512];
static int g_sound_db_used[2];
// Aligned so that a path of 128 characters, whose terminator clears the low byte of the request's context, leaves the pointer as it was on both sides.
class alignas(256) SoundMaker : public GmAudioDatabaseMaker {
public:
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    AtAudioDatabase* NewDatabase() override {
        log_ptr('n', this);
        const int s = g_side;
        g_made[s]++;
        if (g_null_from && g_made[s] >= g_null_from) return nullptr;
        SoundDb* db = &g_sound_dbs[s][g_sound_db_used[s]++ % 512];
        const bool was = g_recording;
        g_recording = false;
        db->~SoundDb();
        new (db) SoundDb;
        g_recording = was;
        return db;
    }
    void DeleteDatabase(AtAudioDatabase* db) override { log_ptr('m', this); log_ptr('m', db); }
};

// The one mesh database each loader lists, so that the mode and the directory the graphics item gives AtDatabaseLoader::Load are seen: with mode 2 it asks for a binary form,
// which this one has, and loads it, logging the depth of the file system's directory stack.
class Mesh : public AtMeshDatabase {
public:
    void MakeMeshes(AtMaterialOwner*) override { note('Y'); }
    void MakeVertexArrays(AtMaterialOwner*) override { note('Y'); }
    void MakeIndexArrays(AtMaterialOwner*) override { note('Y'); }
    int HasBinaryForm() override { note('B'); return 1; }
    void LoadBinaryForm(AtMaterialOwner*, const char* name) override { note('b', hash(name)); note('b', (uint32_t)g_fs->m_path.m_top); }
    void Slot8() override {}
    AtMesh* MeshAt(unsigned int) override { return nullptr; }
    AtVertexArray* VertexArrayAt(unsigned int) override { return nullptr; }
    AtIndexArray* IndexArrayAt(unsigned int) override { return nullptr; }
};
static Mesh* g_mesh[2];

class Loader : public AtDatabaseLoader {
public:
    void MakeTextures() override { note('T'); }
    void MakeMaterials() override { note('T'); }
    void MakeShaders() override { note('T'); }
    void MakeMeshes() override { note('T'); }
    void MakeLights() override { note('T'); }
    void Slot5() override { note('T'); }
    void MakeFonts() override { note('T'); }
    void MakeImages() override { note('T'); }
    void MakeVertexProcesses() override { note('T'); }
    void MakePixelProcesses() override { note('T'); }
    void MakeLoaders() override { note('T'); }
    AtTextureDatabase* TextureAt(unsigned int) override { return nullptr; }
    AtMaterialDatabase* MaterialAt(unsigned int) override { return nullptr; }
    AtShaderDatabase* ShaderAt(unsigned int) override { return nullptr; }
    AtMeshDatabase* MeshAt(unsigned int) override { return g_mesh[g_side]; }
    AtLightDatabase* LightAt(unsigned int) override { return nullptr; }
    AtFontDatabase* FontAt(unsigned int) override { return nullptr; }
    AtImageDatabase* ImageAt(unsigned int) override { return nullptr; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int) override { return nullptr; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int) override { return nullptr; }
    AtDatabaseLoader* LoaderAt(unsigned int) override { return nullptr; }
};
static Loader* g_loaders[2][256];
static int g_loader_used[2];
class alignas(256) Graphics : public AppGraphics {
public:
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
    AtDatabaseLoader* NewLoader() override {
        log_ptr('L', this);
        const int s = g_side;
        Loader* l = g_loaders[s][g_loader_used[s]++ % 256];
        const bool was = g_recording;
        g_recording = false;
        l->~Loader();
        new (l) Loader;
        g_recording = was;
        return l;
    }
    void DeleteLoader(AtDatabaseLoader* l) override { log_ptr('l', this); log_ptr('l', l); }
};
struct alignas(256) MaterialOwner : AtMaterialOwner {};
struct alignas(256) EventHome : GmEventHome {};

// ---- each side's world ------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct World {
    App* app;
    GmHistoryLogs logs;
    EventHome events;
    GmDataOwners* data;
    GmDynamicNodeState* state;
    AtNameTableOwner* soundOwner;
    MaterialOwner material;
    SoundMaker maker;
    Graphics graphics;
    GmSpaceGraphOwner graph;
};
static World g_world[2];
static int g_platform = 1;
static const char kLanguage[] = "EnglishUS";

typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnU)(void*, void*, unsigned int);
typedef void(__fastcall* FnP)(void*, void*, const void*);
typedef void*(__fastcall* FnD)(void*, void*, unsigned int);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* FnI)(void*, void*, unsigned int);

static void make_worlds() {
    for (int s = 0; s < 2; s++) {
        World& w = g_world[s];
        // Aligned, so that a request's context with its low byte cleared by a path of 128 characters lands at the same offset on both sides.
        w.app = new (_aligned_malloc(sizeof(App), 256)) App;
        w.app->m_graphics = &w.graphics;
        w.app->m_materialOwner = &w.material;
        w.app->m_sceneOwner.m_geometrySpaces.Init(8);
        w.logs.m_app = w.app;
        w.soundOwner = new AtNameTableOwner;
        w.data = (GmDataOwners*)std::calloc(1, sizeof(GmDataOwners));
        char* state = (char*)std::calloc(1, sizeof(GmDynamicNodeState));
        if (s == 0) {
            at<FnCtor>(0x000603A0)(&w.data->m_registryOwner.m_plain, nullptr);
            at<FnCtor>(0x0005FFF0)(&w.data->m_registryOwner.m_spaces, nullptr);
            at<FnI>(0x00060010)(&w.data->m_registryOwner.m_spaces, nullptr, 8);
            at<FnCtor>(0x000674E0)(state + offsetof(GmDynamicNodeState, m_registry), nullptr);
        } else {
            new (&w.data->m_registryOwner.m_plain) AtDataRegistryGroup;
            new (&w.data->m_registryOwner.m_spaces) AtDataRegistryNameSpaces;
            w.data->m_registryOwner.m_spaces.Init(8);
            new (state + offsetof(GmDynamicNodeState, m_registry)) AtDataRegistry;
        }
        w.state = (GmDynamicNodeState*)state;
        w.state->m_dataOwners = w.data;
        w.graph.m_graph = (AtSpaceGraph*)(uintptr_t)0x6B6B;
        for (int i = 0; i < 256; i++) g_loaders[s][i] = new Loader;
        g_mesh[s] = new Mesh;
    }
    const uint32_t kTags[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    g_ranges.push_back({{g_world[0].app, g_world[1].app}, sizeof(App), kTags[0]});
    g_ranges.push_back({{&g_world[0].events, &g_world[1].events}, sizeof(EventHome), kTags[1]});
    g_ranges.push_back({{g_world[0].data, g_world[1].data}, sizeof(GmDataOwners), kTags[2]});
    g_ranges.push_back({{g_world[0].state, g_world[1].state}, sizeof(GmDynamicNodeState), kTags[3]});
    g_ranges.push_back({{g_world[0].soundOwner, g_world[1].soundOwner}, sizeof(AtNameTableOwner), kTags[4]});
    g_ranges.push_back({{&g_world[0].material, &g_world[1].material}, sizeof(MaterialOwner), kTags[5]});
    g_ranges.push_back({{&g_world[0].maker, &g_world[1].maker}, sizeof(SoundMaker), kTags[6]});
    g_ranges.push_back({{&g_world[0].graphics, &g_world[1].graphics}, sizeof(Graphics), kTags[7]});
    g_ranges.push_back({{&g_world[0].graph, &g_world[1].graph}, sizeof(GmSpaceGraphOwner), kTags[8]});
    g_ranges.push_back({{&g_sound_dbs[0][0], &g_sound_dbs[1][0]}, sizeof g_sound_dbs[0], kTags[9]});
    for (int i = 0; i < 256; i++) g_ranges.push_back({{g_loaders[0][i], g_loaders[1][i]}, sizeof(Loader), 0x20 + (uint32_t)i});
}

static void set_globals(void* heap) {
    *(void**)(uintptr_t)0x003B3610 = heap;
    GmDatabaseManifest::s_heap = heap;
    *(void**)(uintptr_t)0x003B35D4 = &g_behavior;
    GmBehaviorSystem::s_instance = &g_behavior;
    *(void**)(uintptr_t)0x003B3C28 = OWNER(3);
    GmVfxManager::s_instance = (GmVfxManager*)OWNER(3);
    *(void**)(uintptr_t)0x003B3984 = OWNER(4);
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)OWNER(4);
    *(void**)(uintptr_t)0x003B3604 = &g_world[0].graph;
    GmSpaceGraphOwner::s_instance = &g_world[1].graph;
    *(void**)(uintptr_t)0x003B357C = g_world[0].state;
    GmDynamicNodeState::s_instance = g_world[1].state;
    *(void**)(uintptr_t)0x003B3568 = OWNER(6);
    GmMusicManager::s_instance = (GmMusicManager*)OWNER(6);
    *(void**)(uintptr_t)0x002CD138 = OWNER(7);
    GmRegistryNames::s_instance = (GmRegistryNames*)OWNER(7);
    *(void**)(uintptr_t)0x003B3570 = OWNER(8);
    GmLevelManager::s_instance = (GmLevelManager*)OWNER(8);
    *(void**)(uintptr_t)0x003B3550 = &g_world[0].maker;
    GmAudioDatabaseMaker::s_instance = &g_world[1].maker;
    *(void**)(uintptr_t)0x003B3558 = g_world[0].soundOwner;
    GmAudioData::s_soundOwner = g_world[1].soundOwner;
    *(const char**)(uintptr_t)0x003B3BE4 = kLanguage;
    g_audioLanguage = kLanguage;
    *(void**)(uintptr_t)0x002CB0A8 = &g_world[0].logs;
    GmHistoryLogs::s_instance = &g_world[1].logs;
    *(void**)(uintptr_t)0x003B358C = &g_world[0].events;
    GmEventHome::s_instance = &g_world[1].events;
    *(int*)(uintptr_t)0x002B9348 = g_platform;
    g_usePlatformBinary = g_platform;
}

// The sound manager of each side, fresh for each pair: only its sixty note banks are used.
typedef void*(__fastcall* FnCtor)(void*, void*);
static void new_audio() {
    for (int side = 0; side < 2; side++) {
        GmAudioData* m = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
        for (int i = 0; i < GmAudioData::kNoteBanks; i++) {
            std::memset(&m->m_noteBanks[i], 0xCD, sizeof(GmSoundNoteBank));
            if (side == 0) at<FnCtor>(0x00136CC0)(&m->m_noteBanks[i], nullptr);
            else new (&m->m_noteBanks[i]) GmSoundNoteBank;
        }
        g_audio[side] = m;
    }
    *(GmAudioData**)(uintptr_t)0x003B354C = g_audio[0];
    GmAudioData::s_instance = g_audio[1];
}
// What is left in the banks is freed with them, compared.
static void free_audio() {
    { On on(0); for (int i = 0; i < GmAudioData::kNoteBanks; i++) at<Fn0>(0x00136740)(&g_audio[0]->m_noteBanks[i], nullptr); }
    { On on(1); for (int i = 0; i < GmAudioData::kNoteBanks; i++) g_audio[1]->m_noteBanks[i].~GmSoundNoteBank(); }
    compare_logs("note banks freed");
    std::free(g_audio[0]);
    std::free(g_audio[1]);
    g_audio[0] = g_audio[1] = nullptr;
}

static uint32_t name8(const char* s) {
    char n[9] = {0};
    std::strncpy(n, s, 8);
    return hash(n);
}
static void same_map(const char* what, const GmSoundNoteBank* a, const GmNameMap& x, const GmSoundNoteBank* b, const GmNameMap& y) {
    eq(what, x.m_size, y.m_size);
    eq(what, x.m_entries != nullptr, y.m_entries != nullptr);
    if (!x.m_entries || !y.m_entries || x.m_size != y.m_size) return;
    for (unsigned int i = 0; i < x.m_size; i++) {
        const void* u = x.m_entries[i].m_value;
        const void* v = y.m_entries[i].m_value;
        eq(what, u ? (const GmSoundNote*)u - a->m_notes : -1, v ? (const GmSoundNote*)v - b->m_notes : -1);
        if (u && v) eq(what, name8(x.m_entries[i].m_name), name8(y.m_entries[i].m_name));
    }
}
static void same_banks(const char* what) {
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) {
        eq("bank used", g_audio[0]->m_noteBankUsed[i], g_audio[1]->m_noteBankUsed[i]);
        const GmSoundNoteBank* a = &g_audio[0]->m_noteBanks[i];
        const GmSoundNoteBank* b = &g_audio[1]->m_noteBanks[i];
        eq("bank's notes", a->m_notes != nullptr, b->m_notes != nullptr);
        eq("bank's note count", a->m_noteCount, b->m_noteCount);
        if (!a->m_notes || !b->m_notes || a->m_noteCount != b->m_noteCount) continue;
        eq("bank's name", name8(a->m_name), name8(b->m_name));
        for (unsigned int k = 0; k < a->m_noteCount && k < 64; k++) {
            const GmSoundNote& x = a->m_notes[k];
            const GmSoundNote& y = b->m_notes[k];
            eq("note's name", name8(x.m_name), name8(y.m_name));
            eq("note's trigger", name8(x.m_trigger), name8(y.m_trigger));
            eq("note's stop", name8(x.m_stop), name8(y.m_stop));
            eq("note's singleton", x.m_singleton, y.m_singleton);
            eq("note's choices", x.m_choiceCount, y.m_choiceCount);
            if (x.m_choices && y.m_choices && x.m_choiceCount == y.m_choiceCount) {
                for (unsigned int c = 0; c < x.m_choiceCount; c++) {
                    eq("choice's name", name8(x.m_choices[c].m_name), name8(y.m_choices[c].m_name));
                    eq("choice's weight", *(const uint32_t*)&x.m_choices[c].m_weight, *(const uint32_t*)&y.m_choices[c].m_weight);
                    eq("choice's segment", x.m_choices[c].m_segment != nullptr, y.m_choices[c].m_segment != nullptr);
                }
            }
        }
        same_map("notes by name", a, a->m_byName, b, b->m_byName);
        same_map("notes by trigger", a, a->m_byTrigger, b, b->m_byTrigger);
        same_map("notes by VFX trigger", a, a->m_byVfxTrigger, b, b->m_byVfxTrigger);
    }
    (void)what;
}

struct Pair {
    GmLevelManifest* o;
    GmLevelManifest* r;
    Pair() {
        new_audio();
        o = (GmLevelManifest*)std::malloc(sizeof(GmLevelManifest));
        r = (GmLevelManifest*)std::malloc(sizeof(GmLevelManifest));
        std::memset(o, 0xCD, sizeof(GmLevelManifest));
        std::memset(r, 0xCD, sizeof(GmLevelManifest));
        { On on(0); at<Fn0>(0x001428B0)(o, nullptr); }
        { On on(1); new (r) GmLevelManifest; }
        g_cur[0] = o;
        g_cur[1] = r;
        compare_logs("constructed");
    }
    ~Pair() {
        { On on(0); at<Fn0>(0x001428D0)(o, nullptr); }
        { On on(1); r->~GmLevelManifest(); }
        compare_logs("destroyed");
        same("destroyed");
        g_cur[0] = g_cur[1] = nullptr;
        std::free(o);
        std::free(r);
        free_audio();
    }
    void same(const char* what);
};

static long long queue_place(const GmLevelManifest* m, const GmManifestItem* p) {
    if (!p) return -1;
    struct A { const void* first; unsigned int count, size; };
    const A arrays[] = {
        {m->m_animationLists, m->m_animationListCount, 0x94}, {m->m_soundLists, m->m_soundListCount, 0x94}, {m->m_scripts, m->m_scriptCount, 0x94},
        {m->m_vfx, m->m_vfxCount, 0x94}, {m->m_triggers, m->m_triggerCount, 0xB0}, {m->m_subscribers, m->m_subscriberCount, 0xB0},
        {m->m_dynamicNodes, m->m_dynamicNodeCount, 0x198}, {m->m_soundNodes, m->m_soundNodeCount, 0x94}, {m->m_soundNotes, m->m_soundNoteCount, 0x90},
        {m->m_musicChains, m->m_musicChainCount, 0x90}, {m->m_collisionSounds, m->m_collisionSoundCount, 0x94}, {m->m_instanceSounds, m->m_instanceSoundCount, 0x94},
        {m->m_events, m->m_eventCount, kEventItemSize}, {m->m_registries, m->m_registryCount, kRegistryItemSize}, {m->m_soundDatabases, m->m_soundDatabaseCount, kSoundDatabaseItemSize},
        {m->m_boundingGeometry, m->m_boundingGeometryCount, kBoundingGeometryItemSize}, {m->m_graphicsDatabases, m->m_graphicsDatabaseCount, kGraphicsDatabaseItemSize},
        {m->m_sceneDatabases, m->m_sceneDatabaseCount, kSceneDatabaseItemSize}, {m->m_cinemaDatabases, m->m_cinemaDatabaseCount, kCinemaDatabaseItemSize},
    };
    for (int k = 0; k < (int)(sizeof arrays / sizeof arrays[0]); k++) {
        const char* f = (const char*)arrays[k].first;
        if (f && (const char*)p >= f && (const char*)p < f + arrays[k].count * arrays[k].size) return k * 0x10000 + ((const char*)p - f) / arrays[k].size * 0x1000 + ((const char*)p - f) % arrays[k].size;
    }
    return -2;
}

static void same_item(const char* what, const GmManifestItem* a, const GmManifestItem* b, unsigned int size) {
    eq(what, a->m_extension[0], b->m_extension[0]);
    eqs(what, a->m_extension, b->m_extension);
    eq(what, std::strncmp(a->m_path, b->m_path, sizeof a->m_path), 0);
    for (unsigned int off = 0x8C; size <= 0x94 && off + 4 <= size; off += 4) {
        eq(what, ident(*(void* const*)((const char*)a + off)), ident(*(void* const*)((const char*)b + off)));
    }
    if (size == 0x198) {
        eq(what, *(const uint32_t*)((const char*)a + 0x190), *(const uint32_t*)((const char*)b + 0x190));
        eq(what, *(const uint32_t*)((const char*)a + 0x194), *(const uint32_t*)((const char*)b + 0x194));
    }
}

// The base's kinds, in queue_place's order from 12: events, registries, sound, bounding geometry, graphics, scenes, cinema.
enum BaseKind { kEv, kReg, kSnd, kBnd, kGfx, kScn, kCin };
static void same_scene_db(const char* what, const AtSceneDatabaseLoader& x, const AtSceneDatabaseLoader& y) {
    eq(what, ident(x.m_owner), ident(y.m_owner));
    const void* const* px = (const void* const*)&x.m_nodes;
    const void* const* py = (const void* const*)&y.m_nodes;
    for (int i = 0; i < 11; i++) eq(what, ident(px[i]), ident(py[i]));
    const unsigned short* cx = &x.m_nodeCount;
    const unsigned short* cy = &y.m_nodeCount;
    for (int i = 0; i < 11; i++) eq(what, cx[i], cy[i]);
    eq(what, ident(x.m_loaderNames), ident(y.m_loaderNames));
    eq(what, x.m_nodesByName.m_size, y.m_nodesByName.m_size);
    eq(what, x.m_spaceGraphsByName.m_size, y.m_spaceGraphsByName.m_size);
}
static void same_base_item(const char* what, int kind, const GmManifestItem* a, const GmManifestItem* b) {
    eqs(what, a->m_extension, b->m_extension);
    eq(what, std::strncmp(a->m_path, b->m_path, sizeof a->m_path), 0);
    switch (kind) {
    case kSnd: {
        const GmSoundDatabaseItem* x = (const GmSoundDatabaseItem*)a;
        const GmSoundDatabaseItem* y = (const GmSoundDatabaseItem*)b;
        eq(what, x->m_ran, y->m_ran);
        eq(what, ident(x->m_database), ident(y->m_database));
        eq(what, ident(x->m_maker), ident(y->m_maker));
        eq(what, ident(x->m_owner), ident(y->m_owner));
        if (x->m_database && y->m_database) {
            eq(what, ident(x->m_database->m_owner), ident(y->m_database->m_owner));
            eq(what, x->m_database->m_bankCount + x->m_database->m_segmentCount, y->m_database->m_bankCount + y->m_database->m_segmentCount);
        }
        break;
    }
    case kGfx: {
        const GmGraphicsDatabaseItem* x = (const GmGraphicsDatabaseItem*)a;
        const GmGraphicsDatabaseItem* y = (const GmGraphicsDatabaseItem*)b;
        eq(what, ident(x->m_loader), ident(y->m_loader));
        eq(what, ident(x->m_maker), ident(y->m_maker));
        eq(what, ident(x->m_owner), ident(y->m_owner));
        if (x->m_loader && y->m_loader) eq(what, ident(x->m_loader->m_owner), ident(y->m_loader->m_owner));
        break;
    }
    case kScn: {
        const GmSceneDatabaseItem* x = (const GmSceneDatabaseItem*)a;
        const GmSceneDatabaseItem* y = (const GmSceneDatabaseItem*)b;
        eq(what, ident(x->m_owner), ident(y->m_owner));
        eq(what, x->m_inSpaceGraph, y->m_inSpaceGraph);
        same_scene_db(what, x->m_database, y->m_database);
        break;
    }
    case kCin: {
        const GmCinemaDatabaseItem* x = (const GmCinemaDatabaseItem*)a;
        const GmCinemaDatabaseItem* y = (const GmCinemaDatabaseItem*)b;
        eq(what, ident(x->m_owner), ident(y->m_owner));
        eq(what, x->m_database.m_unknown04, y->m_database.m_unknown04);
        const void* const* px = (const void* const*)&x->m_database.m_entries08;
        const void* const* py = (const void* const*)&y->m_database.m_entries08;
        static const char* const kField[5] = {"cinema +08", "cinema +0C", "cinema +10", "cinema +14", "cinema +18"};
        for (int i = 0; i < 5; i++) eq(kField[i], ident(px[i]), ident(py[i]));
        const unsigned short* cx = &x->m_database.m_count1C;
        const unsigned short* cy = &y->m_database.m_count1C;
        for (int i = 0; i < 5; i++) eq(what, cx[i], cy[i]);
        break;
    }
    case kBnd: {
        const GmBoundingGeometryItem* x = (const GmBoundingGeometryItem*)a;
        const GmBoundingGeometryItem* y = (const GmBoundingGeometryItem*)b;
        eq(what, ident(x->m_owner), ident(y->m_owner));
        eq(what, x->m_database.m_size, y->m_database.m_size);
        eq(what, x->m_database.m_entries != nullptr, y->m_database.m_entries != nullptr);
        eq(what, x->m_database.m_group != nullptr, y->m_database.m_group != nullptr);
        eq(what, x->m_database.m_boxCount, y->m_database.m_boxCount);
        break;
    }
    case kEv: {
        const GmEventDatabaseItem* x = (const GmEventDatabaseItem*)a;
        const GmEventDatabaseItem* y = (const GmEventDatabaseItem*)b;
        eq(what, ident(x->m_owner), ident(y->m_owner));
        eq(what, ident(x->m_database.m_list.m_entries), ident(y->m_database.m_list.m_entries));
        eq(what, x->m_database.m_list.m_count, y->m_database.m_list.m_count);
        eq(what, x->m_database.m_subscriptions != nullptr, y->m_database.m_subscriptions != nullptr);
        eq(what, x->m_database.m_subscriptionCount, y->m_database.m_subscriptionCount);
        eq(what, x->m_database.m_size, y->m_database.m_size);
        break;
    }
    default: {
        const GmRegistryItem* x = (const GmRegistryItem*)a;
        const GmRegistryItem* y = (const GmRegistryItem*)b;
        eq(what, ident(x->m_manager), ident(y->m_manager));
        eq(what, ident(x->m_parent), ident(y->m_parent));
        eq(what, ident(x->m_owner), ident(y->m_owner));
        eq(what, x->m_registry.m_loaded, y->m_registry.m_loaded);
        eq(what, x->m_registry.m_componentTypes, y->m_registry.m_componentTypes);
        eq(what, ident(x->m_registry.m_owner), ident(y->m_registry.m_owner));
        eq(what, ident(x->m_registry.m_parent), ident(y->m_registry.m_parent));
        eq(what, x->m_registry.m_fileName != nullptr, y->m_registry.m_fileName != nullptr);
        break;
    }
    }
}

void Pair::same(const char* what) {
    const unsigned int* oc = &o->m_animationListCount;
    const unsigned int* rc = &r->m_animationListCount;
    for (int i = 0; i < 12; i++) eq(what, oc[i], rc[i]);
    const unsigned int* obc = &o->m_soundDatabaseCount;
    const unsigned int* rbc = &r->m_soundDatabaseCount;
    for (int i = 0; i < 7; i++) eq(what, obc[i], rbc[i]);
    eq(what, o->m_unknown098, r->m_unknown098);
    void* const* oa = (void* const*)&o->m_animationLists;
    void* const* ra = (void* const*)&r->m_animationLists;
    for (int i = 0; i < 12; i++) eq(what, oa[i] != nullptr, ra[i] != nullptr);
    void* const* ob = (void* const*)&o->m_events;
    void* const* rb = (void* const*)&r->m_events;
    for (int i = 0; i < 7; i++) eq(what, ob[i] != nullptr, rb[i] != nullptr);
    struct K { size_t field, count; unsigned int size; int base; };
    const K kinds[] = {
        {offsetof(GmLevelManifest, m_vfx), offsetof(GmLevelManifest, m_vfxCount), 0x94, -1},
        {offsetof(GmLevelManifest, m_soundNotes), offsetof(GmLevelManifest, m_soundNoteCount), 0x90, -1},
        {offsetof(GmLevelManifest, m_musicChains), offsetof(GmLevelManifest, m_musicChainCount), 0x90, -1},
        {offsetof(GmLevelManifest, m_events), offsetof(GmLevelManifest, m_eventCount), kEventItemSize, kEv},
        {offsetof(GmLevelManifest, m_registries), offsetof(GmLevelManifest, m_registryCount), kRegistryItemSize, kReg},
        {offsetof(GmLevelManifest, m_soundDatabases), offsetof(GmLevelManifest, m_soundDatabaseCount), kSoundDatabaseItemSize, kSnd},
        {offsetof(GmLevelManifest, m_boundingGeometry), offsetof(GmLevelManifest, m_boundingGeometryCount), kBoundingGeometryItemSize, kBnd},
        {offsetof(GmLevelManifest, m_graphicsDatabases), offsetof(GmLevelManifest, m_graphicsDatabaseCount), kGraphicsDatabaseItemSize, kGfx},
        {offsetof(GmLevelManifest, m_sceneDatabases), offsetof(GmLevelManifest, m_sceneDatabaseCount), kSceneDatabaseItemSize, kScn},
        {offsetof(GmLevelManifest, m_cinemaDatabases), offsetof(GmLevelManifest, m_cinemaDatabaseCount), kCinemaDatabaseItemSize, kCin},
    };
    for (const K& k : kinds) {
        const char* fo = *(const char* const*)((const char*)o + k.field);
        const char* fr = *(const char* const*)((const char*)r + k.field);
        const unsigned int co = *(const unsigned int*)((const char*)o + k.count);
        const unsigned int cr = *(const unsigned int*)((const char*)r + k.count);
        if (!fo || !fr || co != cr || co > 2000) continue;
        eq(what, ((const uint32_t*)fo)[-1], ((const uint32_t*)fr)[-1]);
        for (unsigned int i = 0; i < co; i++) {
            const GmManifestItem* a = (const GmManifestItem*)(fo + i * k.size);
            const GmManifestItem* b = (const GmManifestItem*)(fr + i * k.size);
            if (k.base < 0) same_item(what, a, b, k.size);
            else same_base_item(what, k.base, a, b);
            eq("item link", queue_place(o, a->m_next), queue_place(r, b->m_next));
        }
    }
    eq(what, queue_place(o, o->m_head), queue_place(r, r->m_head));
    eq(what, queue_place(o, o->m_tail), queue_place(r, r->m_tail));
    eq(what, o->m_state, r->m_state);
    eq(what, o->m_done, r->m_done);
    same_banks(what);
    for (int i = 0; i < GmAudioData::kDatabases; i++) eq("sound databases", ident(g_audio[0]->m_databases[i]), ident(g_audio[1]->m_databases[i]));
    eq("graph", ident(g_world[0].graph.m_graph), ident(g_world[1].graph.m_graph));
    const GmManifestItem* qo = o->m_head;
    const GmManifestItem* qr = r->m_head;
    for (int i = 0; i < 5000 && (qo || qr); i++) {
        eq("queue", queue_place(o, qo), queue_place(r, qr));
        qo = qo ? qo->m_next : nullptr;
        qr = qr ? qr->m_next : nullptr;
    }
}

static void set_file(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
}

static void configure(Pair& p, const char* path) {
    GmManifestRequest req;
    std::memset(&req, 0, sizeof req);
    std::strncpy(req.m_name, "LVL", 8);
    req.m_path = path;
    req.m_heap = (AtHeap*)(uintptr_t)0x4444;
    at<FnP>(0x00141E10)(p.o, nullptr, &req);
    p.r->Configure(&req);
}

static int update(Pair& p, const char* what) {
    int a, b;
    { On on(0); a = at<FnU>(0x00142040)(p.o, nullptr, 30); }
    { On on(1); b = p.r->Update(30); }
    eq(what, a, b);
    compare_logs(what);
    p.same(what);
    return b;
}

static void reset_fakes() {
    g_pool_used[0] = g_pool_used[1] = 0;
    g_made[0] = g_made[1] = 0;
    g_loads[0] = g_loads[1] = 0;
    g_entry_ids[0] = g_entry_ids[1] = 0;
    g_blocks[0].clear();
    g_blocks[1].clear();
    g_sound_db_used[0] = g_sound_db_used[1] = 0;
    g_loader_used[0] = g_loader_used[1] = 0;
    g_world[0].graph.m_graph = g_world[1].graph.m_graph = (AtSpaceGraph*)(uintptr_t)0x6B6B;
}

// Every real file through each side's own Update: opened, parsed (the sound databases loaded on the way), every item run, Finished, then destroyed.
static int g_file_items[8];
static void run_file(const std::string& name, const std::vector<unsigned char>& data, int* reached) {
    g_context = name;
    reset_fakes();
    Pair p;
    set_file(data);
    configure(p, "DESIGN\\LOCATION\\X\\LOADER");
    update(p, "update");
    eq("done", p.r->m_state, GmManifestLoader::kDone);
    *reached = p.r->m_state == GmManifestLoader::kDone && p.o->m_state == GmManifestLoader::kDone;
    const unsigned int* c = &p.r->m_soundDatabaseCount;
    for (int i = 0; i < 7; i++) g_file_items[i] += (int)c[i];
    update(p, "again");
}

static void parse_text(Pair& p, const std::string& text, bool run) {
    static std::vector<unsigned char> data;
    data.assign(text.begin(), text.end());
    set_file(data);
    configure(p, "T");
    { On on(0); at<Fn0>(0x00141EA0)(p.o, nullptr); }
    { On on(1); p.r->Open(); }
    compare_logs("opened");
    if (!p.o->m_file.m_tokenizer || !p.r->m_file.m_tokenizer) return;
    { On on(0); at<FnP>(0x00142E00)(p.o, nullptr, p.o->m_file.m_tokenizer); }
    { On on(1); p.r->Parse(p.r->m_file.m_tokenizer); }
    compare_logs("parse");
    { On on(0); at<FnP>(0x00141E70)(p.o, nullptr, p.o->m_file.m_tokenizer); }
    { On on(1); p.r->ReadTokens(p.r->m_file.m_tokenizer); }
    compare_logs("tokens");
    p.same("tokens");
    { On on(0); at<Fn0>(0x00142640)(p.o, nullptr); }
    { On on(1); p.r->AfterParse(); }
    compare_logs("queued");
    p.same("queued");
    if (!run) return;
    for (int i = 0; i < 300 && (p.o->m_head || p.r->m_head); i++) {
        { On on(0); at<Fn0>(0x00141F80)(p.o, nullptr); }
        { On on(1); p.r->RunNext(); }
        compare_logs("run");
        p.same("run");
    }
}

static void text_case(const std::string& name, const std::string& text, bool run = true) {
    g_context = name;
    reset_fakes();
    Pair p;
    parse_text(p, text, run);
}

static std::string block(const char* word, const std::vector<std::string>& paths, const char* after = "") {
    std::string s = std::string(word) + "[ " + std::to_string(paths.size()) + " ]\n{\n";
    for (const std::string& path : paths) s += "  \"" + path + "\" " + after + "\n";
    return s + "}\n";
}

static const char* const kBaseWords[] = {"SoundDatabaseLoaders", "GraphicsDatabasesLoaders", "SceneDatabasesLoaders", "CinemaDatabaseLoaders", "BoundingGeometryDatabase",
                                         "EventDatabases", "InstanceRegistries"};
static const char* after_of(const char* w) { return std::strcmp(w, "SceneDatabasesLoaders") ? "" : "InSpaceGraph InsertInSnoopie"; }

static void run_words() {
    for (const char* w : kBaseWords) {
        for (int n : {0, 1, 3}) {
            std::vector<std::string> paths;
            for (int i = 0; i < n; i++) paths.push_back(std::string("Dir\\") + w + "\\P" + std::to_string(i));
            text_case(std::string(w) + " x" + std::to_string(n), block(w, paths, after_of(w)));
        }
    }
    std::string all;
    for (const char* w : kBaseWords) all += block(w, {std::string("A\\") + w, std::string("B\\") + w}, after_of(w));
    all += block("VFXDatabases", {"V\\v.VFX"}) + block("MusicChainDatabases", {"M\\m"});
    text_case("all words", all);
    // The sound maker answering null from the first and from the third.
    for (int from : {1, 3}) {
        g_null_from = from;
        text_case("sound maker null from " + std::to_string(from), block("SoundDatabaseLoaders", {"a", "b", "c"}));
        g_null_from = 0;
    }
    // No sound owner: nothing made, and the item still counts as run.
    {
        GmAudioData::s_soundOwner = nullptr;
        *(void**)(uintptr_t)0x003B3558 = nullptr;
        // With nothing to load into, a path of 128 characters can be parsed: the sound word's limit is 128, the others' 127.
        text_case("no sound owner", block("SoundDatabaseLoaders", {"a", std::string(128, 's'), "b"}));
        set_globals(&g_heap_token);
    }
    // No sound maker.
    {
        GmAudioDatabaseMaker::s_instance = nullptr;
        *(void**)(uintptr_t)0x003B3550 = nullptr;
        text_case("no sound maker", block("SoundDatabaseLoaders", {"a"}));
        set_globals(&g_heap_token);
    }
    // The language: a format with its %s, twice, and a name of nine characters cut to eight.
    text_case("language", block("SoundDatabaseLoaders", {"Audio\\VO\\%s\\x", "%s%s", "plain"}));
    // Platform binaries off: the graphics loader's mode is 0.
    g_platform = 0;
    set_globals(&g_heap_token);
    g_agd_text = "";
    text_case("no platform binary", block("GraphicsDatabasesLoaders", {"g1", "g2"}));
    g_agd_text = kAgdText;
    g_platform = 1;
    set_globals(&g_heap_token);
    // The scene paths' flags in each combination; a missing flag swallows the next path, which then reaches a keyword.
    text_case("scene flags", "SceneDatabasesLoaders[ 5 ] { \"a\" InSpaceGraph InsertInSnoopie \"b\" NotInSpaceGraph \"c\" InSpaceGraph \"d\" NotInSpaceGraph InsertInSnoopie \"e\" InsertInSnoopie }");
    text_case("scene flag missing", "SceneDatabasesLoaders[ 2 ] { \"a\" \"b\" InSpaceGraph }");
    text_case("scene no graph", "SceneDatabasesLoaders[ 1 ] { \"a\" InSpaceGraph }");
    // A word twice; the level's words the base lets by; a loose brace.
    text_case("twice", block("EventDatabases", {"A"}) + block("EventDatabases", {"B", "C"}) + block("InstanceRegistries", {"R"}) + block("InstanceRegistries", {}));
    text_case("let by", "Videos Streaming Texture Node InSpaceGraph NotInSpaceGraph InsertInSnoopie 5 \"loose\" { } " + block("CinemaDatabaseLoaders", {"c"}) +
                            block("DynamicNodes", {"D"}) + block("BoundingGeometryDatabase", {"b"}));
    // Paths at the limit and one past it. Not for the sound databases, which open a file named by the path at once, whose name overruns the open's buffer on both sides; the graphics,
    // geometry and registry items open theirs when they run, so for them these are parsed and queued, not run. 128 characters clear the low byte of the request's first context.
    const std::string p127(127, 'p'), p128(128, 'q');
    for (const char* w : kBaseWords) {
        if (!std::strcmp(w, "SoundDatabaseLoaders")) continue;
        const bool run = !std::strcmp(w, "SceneDatabasesLoaders") || !std::strcmp(w, "CinemaDatabaseLoaders") || !std::strcmp(w, "EventDatabases");
        text_case(std::string(w) + " long", block(w, {p127, p128, "after"}, after_of(w)), run);
    }
    // Errors, each reported, the parse going on as the original's does once its report returns.
    text_case("no bracket", "EventDatabases 2 ] { \"a\" \"b\" }");
    text_case("no count", "CinemaDatabaseLoaders [ \"a\" ] { }");
    text_case("no curly", "InstanceRegistries [ 1 ] \"a\" }");
    text_case("too few", "SceneDatabasesLoaders [ 3 ] { \"a\" NotInSpaceGraph }");
    text_case("not a string", "GraphicsDatabasesLoaders [ 2 ] { \"a\" 5 }");
    text_case("too many", "EventDatabases [ 1 ] { \"a\" \"b\" }");
    text_case("no close", "CinemaDatabaseLoaders [ 1 ] { \"a\"");
    text_case("queued only", all, false);
}

static void run_heap_cases() {
    set_globals(nullptr);
    for (const char* w : kBaseWords) text_case(std::string("no heap ") + w, block(w, {}), false);
    set_globals(&g_heap_token);
    for (const char* w : kBaseWords) {
        g_context = std::string("no memory ") + w;
        reset_fakes();
        Pair p;
        static std::vector<unsigned char> data;
        const std::string text = block(w, {});
        data.assign(text.begin(), text.end());
        set_file(data);
        configure(p, "T");
        { On on(0); at<Fn0>(0x00141EA0)(p.o, nullptr); }
        { On on(1); p.r->Open(); }
        { On on(0); at<FnP>(0x00142E00)(p.o, nullptr, p.o->m_file.m_tokenizer); }
        { On on(1); p.r->Parse(p.r->m_file.m_tokenizer); }
        compare_logs("parse");
        g_fail_alloc = true;
        { On on(0); at<FnP>(0x00141E70)(p.o, nullptr, p.o->m_file.m_tokenizer); }
        { On on(1); p.r->ReadTokens(p.r->m_file.m_tokenizer); }
        g_fail_alloc = false;
        compare_logs("tokens");
        p.same("tokens");
    }
    // The sound databases' registry full: the sixty places taken, nothing more goes in.
    {
        g_context = "registry full";
        reset_fakes();
        Pair p;
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < GmAudioData::kDatabases; i++) g_audio[s]->m_databases[i] = (i == 41) ? nullptr : (AtAudioDatabase*)OWNER(12);
        parse_text(p, block("SoundDatabaseLoaders", {"a", "b"}), true);
        for (int s = 0; s < 2; s++)
            for (int i = 0; i < GmAudioData::kDatabases; i++) g_audio[s]->m_databases[i] = nullptr;
    }
}

// Finished on its own, with space graphs put by hand: in a scene's own loader, in a nested loader (depth first), or nowhere; with and without a current graph.
static AtSceneDatabaseLoader* make_loaders(int count) {
    char* p = (char*)std::calloc(count, sizeof(AtSceneDatabaseLoader));
    g_blocks[g_side].push_back({p, count * sizeof(AtSceneDatabaseLoader)});
    return (AtSceneDatabaseLoader*)p;
}
static void run_finished() {
    for (int pattern = 0; pattern < 6; pattern++) {
        g_context = "Finished " + std::to_string(pattern);
        reset_fakes();
        Pair p;
        parse_text(p, "SceneDatabasesLoaders[ 4 ] { \"a\" InSpaceGraph \"b\" InSpaceGraph InsertInSnoopie \"c\" InSpaceGraph \"d\" NotInSpaceGraph }", true);
        GmLevelManifest* m[2] = {p.o, p.r};
        AtSceneDatabaseLoader* first[2];
        for (int s = 0; s < 2; s++) {
            g_side = s;
            GmSceneDatabaseItem* items = static_cast<GmSceneDatabaseItem*>(m[s]->m_sceneDatabases);
            first[s] = &items[0].m_database;
            char* graphs = (char*)std::calloc(4, 0x60);
            g_blocks[s].push_back({graphs, 4 * 0x60});
            if (pattern == 1 && items) {
                // The first database has none, but a nested loader's nested loader has one; the second has its own.
                AtSceneDatabaseLoader* nested = make_loaders(2);
                AtSceneDatabaseLoader* deeper = make_loaders(1);
                nested[1].m_loaders = deeper;
                nested[1].m_loaderCount = 1;
                deeper->m_spaceGraphs = (AtSpaceGraph*)(graphs + 0x60);
                deeper->m_spaceGraphCount = 2;
                nested[0].m_nodes = (AtScene*)make_entries(1, sizeof(AtScene));
                nested[0].m_nodeCount = 1;
                items[0].m_database.m_loaders = nested;
                items[0].m_database.m_loaderCount = 2;
                // The third has nodes of its own and a nested loader with one: which goes in first is seen.
                AtSceneDatabaseLoader* third = make_loaders(1);
                third->m_nodes = (AtScene*)make_entries(1, sizeof(AtScene));
                third->m_nodeCount = 1;
                items[2].m_database.m_loaders = third;
                items[2].m_database.m_loaderCount = 1;
            }
            if (pattern == 5) {
                // Both the first and the second have a graph of their own, the first exactly one: the first is set, and only it.
                items[0].m_database.m_spaceGraphs = (AtSpaceGraph*)(graphs + 0x60);
                items[0].m_database.m_spaceGraphCount = 1;
                items[1].m_database.m_spaceGraphs = (AtSpaceGraph*)graphs;
                items[1].m_database.m_spaceGraphCount = 1;
            }
            if (pattern == 2 || pattern == 3) {
                // The first has none at all; the second its own two.
                items[1].m_database.m_spaceGraphs = (AtSpaceGraph*)graphs;
                items[1].m_database.m_spaceGraphCount = 2;
            }
            if (pattern == 3) g_world[s].graph.m_graph = nullptr;
            if (pattern == 4) g_world[s].graph.m_graph = nullptr;
            g_side = 0;
        }
        { On on(0); at<Fn0>(0x0015D920)(p.o, nullptr); }
        { On on(1); p.r->GmDatabaseManifest::Finished(); }
        compare_logs("finished");
        p.same("finished");
        // The search itself, on each database.
        for (unsigned int i = 0; i < p.r->m_sceneDatabaseCount; i++) {
            void* a;
            void* b;
            typedef void*(__fastcall * FnF)(void*, void*, void*);
            { On on(0); a = at<FnF>(0x0013D9D0)(OWNER(8), nullptr, &static_cast<GmSceneDatabaseItem*>(p.o->m_sceneDatabases)[i].m_database); }
            { On on(1); b = GmLevelManager::s_instance->FindSpaceGraph(&static_cast<GmSceneDatabaseItem*>(p.r->m_sceneDatabases)[i].m_database); }
            eq("found", ident(a), ident(b));
            compare_logs("found");
        }
        // Leaving the graph by hand, then what was put by hand taken away before the items are freed.
        for (unsigned int i = 0; i < p.r->m_sceneDatabaseCount; i++) {
            AtSpaceGraph* g = (AtSpaceGraph*)(uintptr_t)0x6C6C;
            { On on(0); at<FnP>(0x0002EDD0)(&static_cast<GmSceneDatabaseItem*>(p.o->m_sceneDatabases)[i].m_database, nullptr, g); }
            { On on(1); static_cast<GmSceneDatabaseItem*>(p.r->m_sceneDatabases)[i].m_database.RemoveFrom(g); }
            compare_logs("left");
            { On on(0); at<FnP>(0x0002ED70)(&static_cast<GmSceneDatabaseItem*>(p.o->m_sceneDatabases)[i].m_database, nullptr, g); }
            { On on(1); static_cast<GmSceneDatabaseItem*>(p.r->m_sceneDatabases)[i].m_database.InsertInto(g); }
            compare_logs("entered");
        }
        for (int s = 0; s < 2; s++) {
            GmSceneDatabaseItem* items = static_cast<GmSceneDatabaseItem*>(m[s]->m_sceneDatabases);
            for (unsigned int i = 0; i < m[s]->m_sceneDatabaseCount; i++) {
                items[i].m_database.m_loaders = nullptr;
                items[i].m_database.m_loaderCount = 0;
                items[i].m_database.m_spaceGraphs = nullptr;
                items[i].m_database.m_spaceGraphCount = 0;
            }
        }
        (void)first;
    }
}

// The base's AfterParse and Free with the real classes, each kind with several items, and with and without a current graph.
static void run_free() {
    for (int graph = 0; graph < 2; graph++) {
        g_context = "Free graph " + std::to_string(graph);
        reset_fakes();
        Pair p;
        std::string all;
        for (const char* w : kBaseWords) all += block(w, {std::string("F\\") + w, std::string("G\\") + w, std::string("H\\") + w}, after_of(w));
        parse_text(p, all, true);
        if (!graph) g_world[0].graph.m_graph = g_world[1].graph.m_graph = nullptr;
        { On on(0); at<Fn0>(0x0015CD90)(p.o, nullptr); }
        { On on(1); p.r->GmDatabaseManifest::Free(); }
        compare_logs("freed");
        p.same("freed");
    }
}

// Each base kind on its own: made, configured twice from a request of each side's, run twice per round, slot 0, run again, then deleted through its deleting destructor (flags 1) or
// destroyed in place (flags 0).
struct Kind {
    const char* name;
    int kind;
    unsigned int size;
    uint32_t ctor, configure, deleting;
    void* (*make)(void*);
    void (*configure_r)(void*, const void*);
    void (*destroy)(void*, int);
};
template <class T> static void* make_r(void* where) { return new (where) T; }
template <class T, class R> static void configure_r(void* item, const void* r) { ((T*)item)->Configure((const R*)r); }
template <class T> static void destroy_r(void* item, int flags) {
    if (flags) delete (T*)item;
    else ((T*)item)->~T();
}
static const Kind kKinds[] = {
    {"AEE", kEv, kEventItemSize, 0x001B48F0, 0x001B4880, 0x0015CAE0, make_r<GmEventDatabaseItem>, configure_r<GmEventDatabaseItem, GmManifestItemRequest>, destroy_r<GmEventDatabaseItem>},
    {"ADR", kReg, kRegistryItemSize, 0x001B4AA0, 0x001B4A10, 0x0015CB40, make_r<GmRegistryItem>, configure_r<GmRegistryItem, GmRegistryItemRequest>, destroy_r<GmRegistryItem>},
    {"AOD", kSnd, kSoundDatabaseItemSize, 0x001B4150, 0x001B3FF0, 0x0015C900, make_r<GmSoundDatabaseItem>, configure_r<GmSoundDatabaseItem, GmManifestItemRequest>, destroy_r<GmSoundDatabaseItem>},
    {"ASB", kBnd, kBoundingGeometryItemSize, 0x001B4760, 0x001B46F0, 0x0015CA80, make_r<GmBoundingGeometryItem>, configure_r<GmBoundingGeometryItem, GmManifestItemRequest>, destroy_r<GmBoundingGeometryItem>},
    {"AGD", kGfx, kGraphicsDatabaseItemSize, 0x001B42C0, 0x001B4220, 0x0015C960, make_r<GmGraphicsDatabaseItem>, configure_r<GmGraphicsDatabaseItem, GmManifestItemRequest>, destroy_r<GmGraphicsDatabaseItem>},
    {"ASD", kScn, kSceneDatabaseItemSize, 0x001B4430, 0x001B43A0, 0x0015C9C0, make_r<GmSceneDatabaseItem>, configure_r<GmSceneDatabaseItem, GmManifestItemRequest>, destroy_r<GmSceneDatabaseItem>},
    {"ACD", kCin, kCinemaDatabaseItemSize, 0x001B45D0, 0x001B4560, 0x0015CA20, make_r<GmCinemaDatabaseItem>, configure_r<GmCinemaDatabaseItem, GmManifestItemRequest>, destroy_r<GmCinemaDatabaseItem>},
};

static void request_for(int side, int kind, int round, GmRegistryItemRequest* req) {
    std::memset(req, 0x5A, sizeof *req);
    std::strcpy(req->m_path, round ? "Second\\%s" : "Some\\Path");
    World& w = g_world[side];
    switch (kind) {
    case kSnd: req->m_context = &w.maker; req->m_context2 = w.soundOwner; break;
    case kGfx: req->m_context = &w.material; req->m_context2 = &w.graphics; break;
    case kScn: req->m_context = &w.app->m_sceneOwner; req->m_context2 = (void*)(uintptr_t)(round ? 1 : 0); break;
    case kCin: req->m_context = &w.app->m_cinemaOwner; break;
    case kBnd: req->m_context = &w.app->m_sceneOwner; break;
    case kEv: req->m_context = &w.events.m_owner; break;
    default: req->m_context = OWNER(7 + round); req->m_context2 = &w.state->m_registry; req->m_context3 = &w.data->m_registryOwner; break;
    }
}

static void run_items() {
    for (const Kind& k : kKinds) {
        for (int flags : {0, 1}) {
            for (int nulls : {0, 1}) {
                if (nulls && k.kind != kSnd) continue;
                g_context = std::string("item ") + k.name + " flags " + std::to_string(flags) + " nulls " + std::to_string(nulls);
                reset_fakes();
                new_audio();
                g_null_from = nulls;
                char* o = (char*)std::malloc(k.size);
                char* r = (char*)std::malloc(k.size);
                std::memset(o, 0xCD, k.size);
                std::memset(r, 0xCD, k.size);
                g_blocks[0].push_back({o, k.size});
                g_blocks[1].push_back({r, k.size});
                { On on(0); at<Fn0>(k.ctor)(o, nullptr); }
                { On on(1); k.make(r); }
                compare_logs("constructed");
                same_base_item("constructed", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                eq("link", (long long)(uintptr_t)((GmManifestItem*)o)->m_next, (long long)(uintptr_t)((GmManifestItem*)r)->m_next);
                // A registry loaded twice is attached under its parent twice, which the original's code then walks for ever: one load, as the game does.
                const int rounds = k.kind == kReg ? 1 : 2;
                for (int round = 0; round < rounds; round++) {
                    GmRegistryItemRequest req[2];
                    request_for(0, k.kind, round, &req[0]);
                    request_for(1, k.kind, round, &req[1]);
                    { On on(0); at<FnP>(k.configure)(o, nullptr, &req[0]); }
                    { On on(1); k.configure_r(r, &req[1]); }
                    compare_logs("configured");
                    same_base_item("configured", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                    for (int run = 0; run < rounds; run++) {
                        // A second run of a graphics or registry item loads again into what it holds; a cleared scene's or cinema's again; harmless here.
                        { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                        { On on(1); ((GmManifestItem*)r)->Run(); }
                        compare_logs("run");
                        same_base_item("run", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                    }
                }
                // A sound database with no owner is not given back: one with its owner cleared by hand.
                if (k.kind == kSnd && flags == 1) {
                    for (char* item : {o, r}) {
                        AtAudioDatabase* db = ((GmSoundDatabaseItem*)item)->m_database;
                        if (db) db->m_owner = nullptr;
                    }
                }
                { On on(0); (*(Fn0*)(*(void***)o + 0))(o, nullptr); }
                { On on(1); ((GmManifestItem*)r)->Unknown0(); }
                compare_logs("slot 0");
                same_base_item("slot 0", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                // Not for the kinds whose slot 0 forgets what Run needs, the owner or maker, which Run then uses unchecked: a crash on both sides, which the game never asks for.
                if (k.kind != kReg && k.kind != kBnd && k.kind != kGfx) {
                    { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                    { On on(1); ((GmManifestItem*)r)->Run(); }
                    compare_logs("run after slot 0");
                    same_base_item("run after slot 0", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                }
                // Configured and run once more, so that the destructor has something to give back. The registry is not loaded twice.
                {
                    GmRegistryItemRequest req[2];
                    request_for(0, k.kind, 1, &req[0]);
                    request_for(1, k.kind, 1, &req[1]);
                    { On on(0); at<FnP>(k.configure)(o, nullptr, &req[0]); }
                    { On on(1); k.configure_r(r, &req[1]); }
                    compare_logs("configured again");
                    if (k.kind != kReg) {
                        { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                        { On on(1); ((GmManifestItem*)r)->Run(); }
                        compare_logs("run again");
                    }
                    same_base_item("before destruction", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                }
                void* back;
                { On on(0); back = at<FnD>(k.deleting)(o, nullptr, (unsigned)flags); }
                eq("returns this", (long long)(uintptr_t)back, (long long)(uintptr_t)o);
                { On on(1); k.destroy(r, flags); }
                compare_logs("deleted");
                if (!flags) {
                    same_base_item("destroyed", k.kind, (GmManifestItem*)o, (GmManifestItem*)r);
                    std::free(o);
                    std::free(r);
                }
                g_null_from = 0;
                free_audio();
            }
        }
    }
}

// The three small database classes alone: made, filled as a load would, cleared, filled, then deleted (flags 1) or destroyed in place (flags 0).
// The bytes after the vftable, less an embedded member's vftable at skip (0 for none), whose value differs by side.
template <class T> static int differ(const T* o, const T* r, unsigned int skip) {
    const char* a = (const char*)o;
    const char* b = (const char*)r;
    for (unsigned int i = 4; i < sizeof(T); i++) {
        if (skip && i >= skip && i < skip + 4) continue;
        if (a[i] != b[i]) return (int)i;
    }
    return 0;
}
template <class T> static void small_class(const char* name, uint32_t ctor, uint32_t deleting, void (*fill)(T*), unsigned int skip) {
    for (int flags : {0, 1}) {
        for (int filled : {0, 1}) {
            g_context = std::string(name) + " flags " + std::to_string(flags) + " filled " + std::to_string(filled);
            reset_fakes();
            T* o = (T*)std::malloc(sizeof(T));
            T* r = (T*)std::malloc(sizeof(T));
            std::memset(o, 0xCD, sizeof(T));
            std::memset(r, 0xCD, sizeof(T));
            g_blocks[0].push_back({(char*)o, sizeof(T)});
            g_blocks[1].push_back({(char*)r, sizeof(T)});
            { On on(0); at<Fn0>(ctor)(o, nullptr); }
            { On on(1); new (r) T; }
            compare_logs("made");
            eq("made", differ(o, r, skip), 0);
            if (filled) {
                { On on(0); fill(o); }
                { On on(1); fill(r); }
                { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                { On on(1); r->Clear(); }
                compare_logs("cleared");
                eq("cleared", differ(o, r, skip), 0);
                { On on(0); fill(o); }
                { On on(1); fill(r); }
                compare_logs("filled");
            }
            void* back;
            { On on(0); back = at<FnD>(deleting)(o, nullptr, (unsigned)flags); }
            eq("returns this", (long long)(uintptr_t)back, (long long)(uintptr_t)o);
            {
                On on(1);
                if (flags) delete r;
                else r->~T();
            }
            compare_logs("deleted");
            if (!flags) std::free(o), std::free(r);
        }
    }
}
static void fill_cinema(AtCinemaDatabase* d) { cinema_load(d, nullptr, "c", 0); }
static void fill_event(AtEventDatabase* d) { g_loads[g_side] = 1 + (g_loads[g_side] & 1); event_load(d, nullptr, "e", 0); }
// The event database's Clear, by what it holds: a block and a word with no list (Clear leaves all), and a list with a name map of three (Clear frees both).
static void run_event_clear() {
    for (int variant = 0; variant < 2; variant++) {
        g_context = "event clear " + std::to_string(variant);
        reset_fakes();
        AtEventDatabase* d[2];
        for (int s = 0; s < 2; s++) {
            d[s] = (AtEventDatabase*)std::calloc(1, sizeof(AtEventDatabase));
            g_side = s;
            if (s == 0) at<Fn0>(0x000C9660)(d[s], nullptr);
            else new (d[s]) AtEventDatabase;
            if (variant == 0) {
                d[s]->m_subscriptions = (AtEventSubscription*)std::malloc(16);
                g_blocks[s].push_back({(char*)d[s]->m_subscriptions, 16});
                d[s]->m_subscriptionCount = 5;
            } else {
                d[s]->m_list.m_entries = (AtEvent*)make_entries(1, 0x10);
                d[s]->m_list.m_count = 1;
                d[s]->m_subscriptions = (AtEventSubscription*)std::malloc(16);
                if (s == 0) at<FnI>(0x00024DA0)(d[s], nullptr, 3);
                else d[s]->Init(3);
            }
        }
        g_side = 0;
        { On on(0); (*(Fn0*)(*(void***)d[0] + 1))(d[0], nullptr); }
        { On on(1); d[1]->Clear(); }
        compare_logs("event cleared");
        eq("event block", ident(d[0]->m_subscriptions), ident(d[1]->m_subscriptions));
        eq("event 24", d[0]->m_subscriptionCount, d[1]->m_subscriptionCount);
        eq("event map size", d[0]->m_size, d[1]->m_size);
        eq("event map entries", d[0]->m_entries != nullptr, d[1]->m_entries != nullptr);
        eq("event list", ident(d[0]->m_list.m_entries), ident(d[1]->m_list.m_entries));
        // Destroyed, which clears the same way.
        { On on(0); at<FnD>(0x000C5720)(d[0], nullptr, 1); }
        { On on(1); delete d[1]; }
        compare_logs("event deleted");
    }
}

static void run_small_classes() {
    run_event_clear();
    small_class<AtCinemaDatabase>("cinema db", 0x000C5780, 0x000C5AD0, fill_cinema, 0);
    small_class<AtEventDatabase>("event db", 0x000C9660, 0x000C5720, fill_event, 0x14);
    // The list's Free, and its deleting destructor, alone.
    for (int flags : {0, 1}) {
        for (int n : {0, 1, 2}) {
            g_context = "event list flags " + std::to_string(flags) + " n " + std::to_string(n);
            reset_fakes();
            AtEventList* o = (AtEventList*)std::malloc(sizeof(AtEventList));
            AtEventList* r = (AtEventList*)std::malloc(sizeof(AtEventList));
            { On on(0); at<Fn0>(0x000C94B0)(o, nullptr); }
            { On on(1); new (r) AtEventList; }
            eq("made entries", ident(o->m_entries), ident(r->m_entries));
            eq("made count", o->m_count, r->m_count);
            for (int s = 0; s < 2; s++) {
                g_side = s;
                AtEventList* l = s ? r : o;
                if (n) {
                    l->m_entries = (AtEvent*)make_entries((unsigned)(n - 1), 0x10);
                    l->m_count = 7;
                }
            }
            g_side = 0;
            { On on(0); at<Fn0>(0x000C9470)(o, nullptr); }
            { On on(1); r->Free(); }
            compare_logs("list freed");
            eq("entries", ident(o->m_entries), ident(r->m_entries));
            eq("count", o->m_count, r->m_count);
            for (int s = 0; s < 2; s++) {
                g_side = s;
                AtEventList* l = s ? r : o;
                if (n) {
                    l->m_entries = (AtEvent*)make_entries((unsigned)(n - 1), 0x10);
                    l->m_count = 7;
                }
            }
            g_side = 0;
            void* back;
            { On on(0); back = at<FnD>(0x000C9600)(o, nullptr, (unsigned)flags); }
            eq("returns this", (long long)(uintptr_t)back, (long long)(uintptr_t)o);
            {
                On on(1);
                if (flags) delete r;
                else r->~AtEventList();
            }
            compare_logs("list deleted");
            if (!flags) {
                eq("entries after", ident(o->m_entries), ident(r->m_entries));
                eq("count after", o->m_count, r->m_count);
                std::free(o);
                std::free(r);
            }
        }
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s] edi %08X esi %08X side %d\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str(),
                                  (unsigned)e->ContextRecord->Edi, (unsigned)e->ContextRecord->Esi, g_side);
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_dbmanifest <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00020640, (const void*)o_clock);
    // What is not recovered: the base's.
    redirect(0x00139F90, (const void*)o_audio_free_db);
    redirect(0x00071DC0, (const void*)o_registry_release);
    redirect(0x00071F20, (const void*)o_registry_add);
    redirect(0x00140E40, (const void*)o_set_graph);
    redirect(0x0002F7F0, (const void*)o_scene_load);
    redirect(0x000C5B30, (const void*)o_cinema_load);
    redirect(0x000C9A00, (const void*)o_event_load);
    // The level's, as in oracle_manifest.cpp.
    redirect(0x00198C90, (const void*)o_prepare);
    redirect(0x0019C5E0, (const void*)o_load_anim);
    redirect(0x0019C100, (const void*)o_load_sounds);
    redirect(0x00198E70, (const void*)o_release_list);
    redirect(0x00070310, (const void*)o_load_script);
    redirect(0x00070150, (const void*)o_release_script);
    redirect(0x001963B0, (const void*)o_trigger_ctor);
    redirect(0x00196480, (const void*)o_trigger_dtor);
    redirect(0x00196500, (const void*)o_trigger_load);
    redirect(0x001968B0, (const void*)o_subscriber_ctor);
    redirect(0x00196980, (const void*)o_subscriber_dtor);
    redirect(0x00196A00, (const void*)o_subscriber_load);
    redirect(0x001616A0, (const void*)o_load_vfx);
    redirect(0x0014FF60, (const void*)o_node_load);
    redirect(0x00150020, (const void*)o_node_unload);
    redirect(0x0003E860, (const void*)o_insert);
    redirect(0x0003E890, (const void*)o_remove);
    redirect(0x001367E0, (const void*)o_alloc_nodes);
    redirect(0x0013AA80, (const void*)o_load_nodes);
    redirect(0x00138E50, (const void*)o_free_nodes);
    redirect(0x00136810, (const void*)o_release_bank);
    redirect(0x00138A10, (const void*)o_register_nodes);
    redirect(0x00139C40, (const void*)o_load_instance);
    redirect(0x00136B30, (const void*)o_free_instance);
    redirect(0x00136A10, (const void*)o_register_instance);
    redirect(0x00139610, (const void*)o_load_collision);
    redirect(0x00139B50, (const void*)o_free_collision);
    redirect(0x0013D0C0, (const void*)o_music_alloc);
    redirect(0x0013D2A0, (const void*)o_chain_load);
    redirect(0x0013CFF0, (const void*)o_chain_clear);
    redirect(0x0013D3A0, (const void*)o_music_stop);
    redirect(0x0013D360, (const void*)o_music_free);
    at<void(__cdecl*)()>(0x00125BC7)();

    g_fs = new Fs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x003B3548 = &g_heap_token;
    GmAudioData::s_heap = &g_heap_token;
    // The loaders that run for real on both sides.
    for (uint32_t va : {0x00066100u, 0x00069DE0u, 0x0006BAC0u, 0x0006BFC0u, 0x0005FFB0u, 0x00018A90u, 0x000196E0u, 0x00040A60u}) at<void(__cdecl*)(void*)>(va)(&g_heap_token);
    *(void**)(uintptr_t)0x002CB098 = &g_heap_token;
    AtDataRegistry::SetHeap(&g_heap_token);
    AtDataComponent::SetHeap(&g_heap_token);
    AtDataDescriptor::SetHeap(&g_heap_token);
    AtDataLink::SetHeap(&g_heap_token);
    AtDataRegistryNameSpaces::SetHeap(&g_heap_token);
    AtAudioDatabase::SetHeap(&g_heap_token);
    AtDatabaseLoader::SetHeap(&g_heap_token);
    AtBoundingGeometryDatabase::SetHeap(&g_heap_token);
    *(AtHeapManager**)(uintptr_t)0x002C60ACu = &g_manager[0];
    AtHeapManager::s_instance = &g_manager[1];
    g_behavior.m_animationLists = (GmBehaviorListOwner*)OWNER(0);
    g_behavior.m_soundLists = (GmBehaviorListOwner*)OWNER(1);
    g_behavior.m_scripts = (GmScriptOwner*)OWNER(2);
    make_worlds();
    set_globals(&g_heap_token);
    // Armed only now: the worlds' registries and owners are made with the original's constructors, which are not this suite's.
    if (entries && !arm_entries(entries)) return 2;
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_items();
    run_small_classes();
    run_words();
    run_heap_cases();
    run_finished();
    run_free();

    int files = 0, done = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".MNG\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        int reached = 0;
        run_file(line, data, &reached);
        files++;
        done += reached;
    }
    std::fclose(index);
    if (files == 0) { std::printf("no files listed\n"); g_failures++; }
    std::printf("real files run to the end on both sides: %d / %d; base items made: sound %d, graphics %d, scene %d, cinema %d, bounding %d, event %d, registry %d\n", done, files,
                g_file_items[0], g_file_items[1], g_file_items[2], g_file_items[3], g_file_items[4], g_file_items[5], g_file_items[6]);
    report_entries();
    std::printf("%s: %d files; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
