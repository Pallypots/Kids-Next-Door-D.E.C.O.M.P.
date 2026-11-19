// Differential oracle for the level manifest (Windows x86, local only): GmLevelManifest, the recovered parts of GmDatabaseManifest, and the twelve kinds of item. See oracle_ldr.cpp and
// docs/verification.md.
//
//   oracle_manifest <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// The original side runs the original's code for everything recovered here. What is not recovered is a double on both sides, logging what it is asked: the base's word parser (0x0015D000,
// which skips a base word's block here) and its Finished, and what each item loads into. A green result therefore says the parser, the arrays, the queue, the items and their teardown agree,
// and says nothing about what any real database does with a path. The exception is the sound notes: their item (gm/gon) is real on both sides, and loads one written .GON for every path into a
// real GmAudioData of its side's, with no sound lists, so no segment is found; its banks are compared after each step.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmAudioData.h"
#include "gm/GmLevelManifest.h"
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

// Every .GON a sound notes item asks for is this text, in a file of its own so that the .MNG being read is left alone.
static const char kNotesText[] =
    "SoundNotes[ 3 ]\n{\n Note \"Step\" { AnimationTrigger \"foot\" Segments[ 2 ] { \"a\" 0.25 \"b\" } }\n"
    " Note \"Jump\" { Singleton Stop \"Step\" VFXTrigger \"dust\" }\n Note \"Land\" { AnimationTrigger \"land\" Segments[ 1 ] { \"c\" } }\n}\n";
class Fs : public MemFs {
public:
    MemFile notes[2];
    int Open(const char* path, int flags, AtIoFile** out) override {
        if (!std::strstr(path, ".GON")) return MemFs::Open(path, flags, out);
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(path, g_side);
        MemFile& f = notes[g_side];
        f.side = g_side;
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = (flags & 0x21) ? 0x0C : 2;
        f.data = (const unsigned char*)kNotesText;
        f.size = sizeof kNotesText - 1;
        strncpy_s(f.m_path, path, _TRUNCATE);  // a path of 128 characters and the extension do not fit
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

// ---- fakes: what the doubles hand out ----------------------------------------------------------------------------------------------------------------------------------------------------
// One pool to a side; both sides take from theirs in the same order, so a value is compared by its place.
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
static GmAudioData* g_audio[2];
static long long ident(const void* p) {
    for (int s = 0; s < 2; s++) {
        if (g_audio[s] && p >= (const void*)&g_audio[s]->m_noteBanks[0] && p < (const void*)&g_audio[s]->m_noteBanks[GmAudioData::kNoteBanks])
            return 0x60000000 + ((const GmSoundNoteBank*)p - &g_audio[s]->m_noteBanks[0]);
        if (p >= (const void*)&g_pool[s][0] && p < (const void*)&g_pool[s][4096]) return 0x70000000 + ((const Fake*)p - &g_pool[s][0]);
    }
    return (long long)(uintptr_t)p;
}
// At most 0x80 bytes: a path of 128 characters fills an item's buffer and its terminator lands on the link, which Enqueue then overwrites with a pointer that differs by side.
// The original's loaders would read on into it; that is not compared.
static uint32_t hash(const char* s) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 0x80 && s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void note(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static void log_ptr(uint32_t a, const void* p) { note(a, (uint32_t)ident(p)); }

// Owners and managers: the same fake addresses on both sides; never dereferenced by a double.
static GmBehaviorSystem g_behavior;
static GmSpaceGraphOwner g_graph_owner;
static GmDynamicNodeState g_node_state;
#define OWNER(n) ((void*)(uintptr_t)(0x00005123 + (n) * 0x100))

// The current manifest of each side, so that a double can say which item it was given.
static void* g_cur[2];
static long long item_place(const void* p) {
    if (!p || !g_cur[g_side]) return -1;
    const GmLevelManifest* m = (const GmLevelManifest*)g_cur[g_side];
    if (m->m_dynamicNodes && p >= (const void*)m->m_dynamicNodes) return (const char*)p - (const char*)m->m_dynamicNodes;
    return -2;
}

// ---- doubles for what is not recovered ----------------------------------------------------------------------------------------------------------------------------------------------------
static void base_word(int token, AtIoTokenizer* t) {
    note('B', (uint32_t)token);
    if (token < GmDatabaseManifest::kEventDatabases || token > GmDatabaseManifest::kCinemaDatabaseLoaders) return;
    note('b', (uint32_t)t->ExpectCount());
    for (int guard = 0; guard < 10000; guard++) {
        const int n = t->Next();
        if (n == AtIoTokenizer::kString) note('s', hash(t->m_string));
        else note('t', (uint32_t)n);
        if (n == AtIoTokenizer::kRightCurly || n == 0) break;
    }
}
static void __fastcall o_base_word(void*, void*, int token, AtIoTokenizer* t) { base_word(token, t); }
void GmDatabaseManifest::OnToken(int token, AtIoTokenizer* t) { base_word(token, t); }
static void __fastcall o_base_finished(void*, void*) { note('Z'); }
void GmDatabaseManifest::Finished() { note('Z'); }

// Base items: fakes with a table of three, enough for the base's AfterParse and Free.
static void __fastcall fb_unknown0(void* self, void*) { note('o', (uint32_t)((Fake*)self)->id); }
static void __fastcall fb_run(void* self, void*) { note('r', (uint32_t)((Fake*)self)->id); }
static void* __fastcall fb_delete(void* self, void*, unsigned int flags) {
    note('X', flags);
    uint32_t* block = (uint32_t*)self - 1;
    note('x', *block);
    if (g_recording) g_log[g_side].add('D');
    std::free(block);
    return block;
}
static void* g_base_vtable[3] = {(void*)fb_unknown0, (void*)fb_run, (void*)fb_delete};
void GmDatabaseItemsDelete(int kind, GmManifestItem* first) {
    (void)kind;  // The original has this inline: what is compared is what it does.
    uint32_t* block = (uint32_t*)first - 1;
    if (*block == 0) operator delete(block);
    else (*(void* (__fastcall**)(void*, void*, unsigned int))((*(void***)first) + 2))(first, nullptr, 3);
}
static void __fastcall o_scene_leave(void* part, void*, AtSpaceGraph* graph) { note('l', (uint32_t)(uintptr_t)graph); note('l', ((uint32_t*)part)[0]); }
void GmSceneItemLeave(void* part, AtSpaceGraph* graph) { note('l', (uint32_t)(uintptr_t)graph); note('l', ((uint32_t*)part)[0]); }
static void __fastcall o_audio_free_db(void*, void*, void* db) { note('a', (uint32_t)(uintptr_t)db); }
void GmAudioFreeDatabase(void* db) { note('a', (uint32_t)(uintptr_t)db); }
static void __fastcall o_registry(void*, void*, void* part) { note('g', ((uint32_t*)part)[0]); }
void GmRegistryRelease(void* part) { note('g', ((uint32_t*)part)[0]); }

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
static void __fastcall o_insert(void* scene, void*, AtSpaceGraph* g) { note('I', (uint32_t)item_place(scene)); note('I', (uint32_t)(uintptr_t)g); }
void AtScene::InsertInto(AtSpaceGraph* g) { note('I', (uint32_t)item_place(this)); note('I', (uint32_t)(uintptr_t)g); }
static void __fastcall o_remove(void* scene, void*, AtSpaceGraph* g) { note('J', (uint32_t)item_place(scene)); note('J', (uint32_t)(uintptr_t)g); }
void AtScene::RemoveFrom(AtSpaceGraph* g) { note('J', (uint32_t)item_place(this)); note('J', (uint32_t)(uintptr_t)g); }

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

// ---- the pair --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnU)(void*, void*, unsigned int);
typedef void(__fastcall* FnP)(void*, void*, const void*);
typedef void*(__fastcall* FnD)(void*, void*, unsigned int);
typedef void(__fastcall* FnTok)(void*, void*, int, void*);

static void set_globals(void* heap) {
    *(void**)(uintptr_t)0x003B3610 = heap;
    GmDatabaseManifest::s_heap = heap;
    *(void**)(uintptr_t)0x003B35D4 = &g_behavior;
    GmBehaviorSystem::s_instance = &g_behavior;
    *(void**)(uintptr_t)0x003B3C28 = OWNER(3);
    GmVfxManager::s_instance = (GmVfxManager*)OWNER(3);
    *(void**)(uintptr_t)0x003B3984 = OWNER(4);
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)OWNER(4);
    *(void**)(uintptr_t)0x003B3604 = &g_graph_owner;
    GmSpaceGraphOwner::s_instance = &g_graph_owner;
    *(void**)(uintptr_t)0x003B357C = &g_node_state;
    GmDynamicNodeState::s_instance = &g_node_state;
    *(void**)(uintptr_t)0x003B3568 = OWNER(6);
    GmMusicManager::s_instance = (GmMusicManager*)OWNER(6);
    *(void**)(uintptr_t)0x002CD138 = OWNER(7);
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

// Which item a queued pointer is, as (array, index), on its own side.
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
        if (f && (const char*)p >= f && (const char*)p < f + arrays[k].count * arrays[k].size) return k * 0x10000 + ((const char*)p - f) / arrays[k].size;
    }
    return -2;
}

static void same_item(const char* what, const GmManifestItem* a, const GmManifestItem* b, unsigned int size) {
    eq(what, a->m_extension[0], b->m_extension[0]);
    eqs(what, a->m_extension, b->m_extension);
    eq(what, std::strncmp(a->m_path, b->m_path, sizeof a->m_path), 0);
    // What follows the path: the fields of each kind, compared word by word as values or fakes. Tables and scenes, whose tables differ by side, are compared by what their doubles logged.
    for (unsigned int off = 0x8C; size <= 0x94 && off + 4 <= size; off += 4) {
        eq(what, ident(*(void* const*)((const char*)a + off)), ident(*(void* const*)((const char*)b + off)));
    }
    if (size == 0x198) {
        eq(what, *(const uint32_t*)((const char*)a + 0x190), *(const uint32_t*)((const char*)b + 0x190));
        eq(what, *(const uint32_t*)((const char*)a + 0x194), *(const uint32_t*)((const char*)b + 0x194));
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
    struct K { size_t field, count; unsigned int size; };
    const K kinds[] = {
        {offsetof(GmLevelManifest, m_animationLists), offsetof(GmLevelManifest, m_animationListCount), 0x94}, {offsetof(GmLevelManifest, m_soundLists), offsetof(GmLevelManifest, m_soundListCount), 0x94},
        {offsetof(GmLevelManifest, m_scripts), offsetof(GmLevelManifest, m_scriptCount), 0x94}, {offsetof(GmLevelManifest, m_vfx), offsetof(GmLevelManifest, m_vfxCount), 0x94},
        {offsetof(GmLevelManifest, m_triggers), offsetof(GmLevelManifest, m_triggerCount), 0xB0}, {offsetof(GmLevelManifest, m_subscribers), offsetof(GmLevelManifest, m_subscriberCount), 0xB0},
        {offsetof(GmLevelManifest, m_dynamicNodes), offsetof(GmLevelManifest, m_dynamicNodeCount), 0x198}, {offsetof(GmLevelManifest, m_soundNodes), offsetof(GmLevelManifest, m_soundNodeCount), 0x94},
        {offsetof(GmLevelManifest, m_soundNotes), offsetof(GmLevelManifest, m_soundNoteCount), 0x90}, {offsetof(GmLevelManifest, m_musicChains), offsetof(GmLevelManifest, m_musicChainCount), 0x90},
        {offsetof(GmLevelManifest, m_collisionSounds), offsetof(GmLevelManifest, m_collisionSoundCount), 0x94}, {offsetof(GmLevelManifest, m_instanceSounds), offsetof(GmLevelManifest, m_instanceSoundCount), 0x94},
    };
    for (const K& k : kinds) {
        const char* fo = *(const char* const*)((const char*)o + k.field);
        const char* fr = *(const char* const*)((const char*)r + k.field);
        const unsigned int co = *(const unsigned int*)((const char*)o + k.count);
        const unsigned int cr = *(const unsigned int*)((const char*)r + k.count);
        if (!fo || !fr || co != cr || co > 64) continue;
        eq(what, ((const uint32_t*)fo)[-1], ((const uint32_t*)fr)[-1]);
        for (unsigned int i = 0; i < co; i++) {
            const GmManifestItem* a = (const GmManifestItem*)(fo + i * k.size);
            const GmManifestItem* b = (const GmManifestItem*)(fr + i * k.size);
            same_item(what, a, b, k.size);
            eq("item link", queue_place(o, a->m_next), queue_place(r, b->m_next));
        }
    }
    eq(what, queue_place(o, o->m_head), queue_place(r, r->m_head));
    eq(what, queue_place(o, o->m_tail), queue_place(r, r->m_tail));
    eq(what, o->m_state, r->m_state);
    eq(what, o->m_done, r->m_done);
    same_banks(what);
    // The whole queue, in order.
    const GmManifestItem* qo = o->m_head;
    const GmManifestItem* qr = r->m_head;
    for (int i = 0; i < 2000 && (qo || qr); i++) {
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
}

// Opens the file and parses it, on each side through its own Update; the clock never moves, so each call runs to the end: every item, then Finished.
static void run_file(const std::string& name, const std::vector<unsigned char>& data) {
    g_context = name;
    reset_fakes();
    Pair p;
    set_file(data);
    configure(p, "DESIGN\\LOCATION\\X\\LOADER");
    update(p, "update");
    eq("done", p.r->m_state, GmManifestLoader::kDone);
    update(p, "again");
}

// The steps by hand: open, parse, read every token, queue. Optionally the items run and Finished.
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

static std::string block(const char* word, const std::vector<std::string>& paths) {
    std::string s = std::string(word) + "[ " + std::to_string(paths.size()) + " ]\n{\n";
    for (const std::string& path : paths) s += "  \"" + path + "\"\n";
    return s + "}\n";
}

static const char* const kWords[] = {"BehaviorAnimationLists", "BehaviorSoundLists", "SmallCScripts", "TriggerDatabases", "SubscriberDatabases", "VFXDatabases", "DynamicNodes",
                                     "SoundNodeDatabases", "SoundNoteDatabases", "MusicChainDatabases", "CollisionSoundNodeDatabases", "InstanceSoundNodeDatabases"};

static void run_words() {
    // Each word alone, with none, one and three paths, and run.
    for (const char* w : kWords) {
        for (int n : {0, 1, 3}) {
            std::vector<std::string> paths;
            for (int i = 0; i < n; i++) paths.push_back(std::string("Dir\\") + w + "\\P" + std::to_string(i));
            text_case(std::string(w) + " x" + std::to_string(n), block(w, paths));
        }
    }
    // Every word together, then each kind's loads answering null, from the first and from the third.
    std::string all;
    for (const char* w : kWords) all += block(w, {std::string("A\\") + w, std::string("B\\") + w});
    text_case("all words", all);
    for (int from : {1, 3}) {
        g_null_from = from;
        text_case("all words, null from " + std::to_string(from), all);
        g_null_from = 0;
    }
    // A word given twice: the first array is not freed.
    text_case("twice", block("VFXDatabases", {"A"}) + block("VFXDatabases", {"B", "C"}) + block("SoundNodeDatabases", {"N"}) + block("SoundNodeDatabases", {}));
    // The base's words and the four words the base lets by, among these; and what DynamicNodes leaves.
    text_case("mixed", block("SoundDatabaseLoaders", {"Audio\\x"}) + block("TriggerDatabases", {"T"}) + "Videos Streaming Texture Node InSpaceGraph 5 \"loose\" { }\n" +
                           block("GraphicsDatabasesLoaders", {"g1", "g2"}) + block("DynamicNodes", {"D"}) + block("MusicChainDatabases", {"M"}));
    // Path lengths at the limit and one past it: the first seven words report it, and the copy runs one byte into the request's next field.
    const std::string p127(127, 'p'), p128(128, 'q');
    // The sound notes' items are real here and load what their path names: a path this long overruns the file name's buffer on both sides (127 characters and the extension), or is unterminated
    // (128), so for that word these are parsed and queued, not run. Real files' paths are short.
    for (const char* w : kWords) {
        text_case(std::string(w) + " long", block(w, {p127, p128, "after"}), std::strcmp(w, "SoundNoteDatabases") != 0);
    }
    // Errors: each one reported, and the parse goes on as the original's does once its report returns.
    text_case("no bracket", "VFXDatabases 2 ] { \"a\" \"b\" }");
    text_case("no count", "TriggerDatabases [ \"a\" ] { }");
    text_case("no curly", "SoundNodeDatabases [ 1 ] \"a\" }");
    text_case("too few", "MusicChainDatabases [ 3 ] { \"a\" }");
    text_case("not a string", "BehaviorAnimationLists [ 2 ] { \"a\" 5 }");
    text_case("too many", "InstanceSoundNodeDatabases [ 1 ] { \"a\" \"b\" }");
    text_case("no close", "CollisionSoundNodeDatabases [ 1 ] { \"a\"");
    text_case("empty", "");
    text_case("comments", "// nothing\n/* a */ SoundNoteDatabases[1]{\"n\"} // end\n");
    // Not run: parsed and queued only.
    text_case("queued only", all, false);
}

static void run_heap_cases() {
    // No heap: reported, and the null heap pushed.
    set_globals(nullptr);
    for (const char* w : kWords) text_case(std::string("no heap ") + w, block(w, {}), false);
    set_globals(&g_heap_token);
    // No memory, with a count of none so that nothing is configured.
    for (const char* w : kWords) {
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
}

// Fake arrays for the base's seven kinds, put in both sides alike: AfterParse queues them, Free gives them back.
static uint32_t* fake_array(unsigned int count, unsigned int size, int kind) {
    uint32_t* block = (uint32_t*)std::calloc(1, count * size + 4);
    block[0] = count;
    for (unsigned int i = 0; i < count; i++) {
        char* item = (char*)(block + 1) + i * size;
        Fake* f = (Fake*)item;
        f->vptr = g_base_vtable;
        f->id = kind * 100 + (int)i;
        *(uint32_t*)(item + 0x8C) = 0x8C00 + kind * 16 + i;
        *(uint32_t*)(item + 0x90) = 0x9000 + kind * 16 + i;
        if (size > 0x108) *(uint32_t*)(item + 0x108) = i & 1;
    }
    return block + 1;
}

static void run_base_cases() {
    const unsigned int sizes[7] = {kEventItemSize, kRegistryItemSize, kSoundDatabaseItemSize, kBoundingGeometryItemSize, kGraphicsDatabaseItemSize, kSceneDatabaseItemSize, kCinemaDatabaseItemSize};
    for (int pattern = 0; pattern < 4; pattern++) {
        for (int graph = 0; graph < 2; graph++) {
            g_context = "base arrays " + std::to_string(pattern) + " graph " + std::to_string(graph);
            g_graph_owner.m_graph = graph ? (AtSpaceGraph*)(uintptr_t)0x6A6A : nullptr;
            reset_fakes();
            Pair p;
            for (GmLevelManifest* m : {p.o, p.r}) {
                GmManifestItem** arrays[7] = {&m->m_events, &m->m_registries, &m->m_soundDatabases, &m->m_boundingGeometry, &m->m_graphicsDatabases, &m->m_sceneDatabases, &m->m_cinemaDatabases};
                unsigned int* counts[7] = {&m->m_eventCount, &m->m_registryCount, &m->m_soundDatabaseCount, &m->m_boundingGeometryCount, &m->m_graphicsDatabaseCount,
                                           &m->m_sceneDatabaseCount, &m->m_cinemaDatabaseCount};
                for (int k = 0; k < 7; k++) {
                    // Pattern 0: none; 1: one each; 2: some empty arrays, some with three; 3: two each.
                    const int n = pattern == 0 ? -1 : pattern == 1 ? 1 : pattern == 2 ? ((k & 1) ? 0 : 3) : 2;
                    if (n < 0) continue;
                    *arrays[k] = (GmManifestItem*)fake_array((unsigned)n, sizes[k], k);
                    *counts[k] = (unsigned)n;
                }
                m->m_unknown098 = 0x98;
            }
            { On on(0); at<Fn0>(0x00142640)(p.o, nullptr); }
            { On on(1); p.r->AfterParse(); }
            compare_logs("queued");
            // Compare the queue by fake ids.
            const GmManifestItem* qo = p.o->m_head;
            const GmManifestItem* qr = p.r->m_head;
            for (int i = 0; i < 200 && (qo || qr); i++) {
                eq("base queue", qo ? ((const Fake*)qo)->id : -1, qr ? ((const Fake*)qr)->id : -1);
                qo = qo ? qo->m_next : nullptr;
                qr = qr ? qr->m_next : nullptr;
            }
            // Free, through the derived class's.
            {
                On on(0);
                at<Fn0>(0x00142930)(p.o, nullptr);
            }
            {
                On on(1);
                p.r->Free();
            }
            compare_logs("freed");
            p.same("freed");
            eq("cleared 098", p.o->m_unknown098, p.r->m_unknown098);
        }
    }
    g_graph_owner.m_graph = (AtSpaceGraph*)(uintptr_t)0x6B6B;
}

// The classes on their own: each kind made singly, configured twice, run twice, its slot 0, then deleted through its deleting destructor (flags 1), or destroyed in place (flags 0).
struct Kind {
    const char* name;
    unsigned int size;
    uint32_t ctor, configure, deleting;
    void* (*make)(void*);
    void (*configure_r)(void*, const GmManifestItemRequest*);
};
template <class T> static void* make_r(void* where) { return new (where) T; }
template <class T> static void configure_r(void* item, const GmManifestItemRequest* r) { ((T*)item)->Configure(r); }
static const Kind kKinds[] = {
    {"ANL", 0x94, 0x001B2C40, 0x001B2C00, 0x001421C0, make_r<GmAnimationListItem>, configure_r<GmAnimationListItem>},
    {"SDL", 0x94, 0x001B2D60, 0x001B2D00, 0x00142220, make_r<GmSoundListItem>, configure_r<GmSoundListItem>},
    {"ASL", 0x94, 0x001B2E50, 0x001B2E10, 0x00142280, make_r<GmScriptItem>, configure_r<GmScriptItem>},
    {"AET", 0xB0, 0x001B2F70, 0x001B2F00, 0x001422E0, make_r<GmTriggerItem>, configure_r<GmTriggerItem>},
    {"AES", 0xB0, 0x001B30A0, 0x001B3060, 0x00142340, make_r<GmSubscriberItem>, configure_r<GmSubscriberItem>},
    {"VFX", 0x94, 0x001B31F0, 0x001B3190, 0x001423A0, make_r<GmVfxItem>, configure_r<GmVfxItem>},
    {"DAC", 0x198, 0x001B3340, 0x001B32D0, 0x00142400, make_r<GmDynamicNodeItem>, configure_r<GmDynamicNodeItem>},
    {"GOO", 0x94, 0x001B34F0, 0x001B33D0, 0x00142460, make_r<GmSoundNodeItem>, configure_r<GmSoundNodeItem>},
    {"GOM", 0x90, 0x001B37E0, 0x001B36D0, 0x00142520, make_r<GmMusicChainItem>, configure_r<GmMusicChainItem>},
    {"GOI", 0x94, 0x001B3940, 0x001B3830, 0x00142580, make_r<GmInstanceSoundItem>, configure_r<GmInstanceSoundItem>},
    {"GOC", 0x94, 0x001B3AC0, 0x001B39D0, 0x001425E0, make_r<GmCollisionSoundItem>, configure_r<GmCollisionSoundItem>},
};

static void run_items() {
    for (const Kind& k : kKinds) {
        for (int flags : {0, 1}) {
            for (int nulls : {0, 1, 2}) {
                g_context = std::string("item ") + k.name + " flags " + std::to_string(flags) + " nulls " + std::to_string(nulls);
                reset_fakes();
                g_null_from = nulls;
                char* o = (char*)std::malloc(k.size);
                char* r = (char*)std::malloc(k.size);
                std::memset(o, 0xCD, k.size);
                std::memset(r, 0xCD, k.size);
                // item_place reports offsets within the item for a scene.
                alignas(8) static char holder[2][sizeof(GmLevelManifest)];
                ((GmLevelManifest*)holder[0])->m_dynamicNodes = (GmDynamicNodeItem*)o;
                ((GmLevelManifest*)holder[1])->m_dynamicNodes = (GmDynamicNodeItem*)r;
                g_cur[0] = holder[0];
                g_cur[1] = holder[1];
                { On on(0); at<Fn0>(k.ctor)(o, nullptr); }
                { On on(1); k.make(r); }
                compare_logs("constructed");
                same_item("constructed", (GmManifestItem*)o, (GmManifestItem*)r, k.size);
                eq("link", (long long)(uintptr_t)((GmManifestItem*)o)->m_next, (long long)(uintptr_t)((GmManifestItem*)r)->m_next);
                GmManifestItemRequest req;
                std::memset(&req, 0x5A, sizeof req);
                std::strcpy(req.m_path, "Some\\Path");
                req.m_context = OWNER(9);
                req.m_context2 = OWNER(10);
                for (int round = 0; round < 2; round++) {
                    { On on(0); at<FnP>(k.configure)(o, nullptr, &req); }
                    { On on(1); k.configure_r(r, &req); }
                    compare_logs("configured");
                    same_item("configured", (GmManifestItem*)o, (GmManifestItem*)r, k.size);
                    for (int run = 0; run < 2; run++) {
                        { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                        { On on(1); ((GmManifestItem*)r)->Run(); }
                        compare_logs("run");
                        same_item("run", (GmManifestItem*)o, (GmManifestItem*)r, k.size);
                    }
                    std::strcpy(req.m_path, "Second");
                    req.m_context = OWNER(11);
                }
                if (!std::strcmp(k.name, "GOI")) {
                    { On on(0); at<Fn0>(0x001B3920)(o, nullptr); }
                    { On on(1); ((GmInstanceSoundItem*)r)->Register(); }
                    compare_logs("register");
                }
                // The kinds with a loaded flag at +0x8C: what they hold with the flag clear, which only a hand can make. Run keeps or replaces what is held; slot 0 gives back only
                // what is marked loaded.
                const bool flagged = !std::strcmp(k.name, "GOO") || !std::strcmp(k.name, "GOI") || !std::strcmp(k.name, "GOC");
                if (flagged && nulls == 0) {
                    *(int*)(o + 0x8C) = *(int*)(r + 0x8C) = 0;
                    { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                    { On on(1); ((GmManifestItem*)r)->Run(); }
                    compare_logs("run unflagged");
                    same_item("run unflagged", (GmManifestItem*)o, (GmManifestItem*)r, k.size);
                    *(int*)(o + 0x8C) = *(int*)(r + 0x8C) = 0;
                }
                { On on(0); (*(Fn0*)(*(void***)o + 0))(o, nullptr); }
                { On on(1); ((GmManifestItem*)r)->Unknown0(); }
                compare_logs("slot 0");
                same_item("slot 0", (GmManifestItem*)o, (GmManifestItem*)r, k.size);
                // Run again after slot 0, then destroy with what that left.
                { On on(0); (*(Fn0*)(*(void***)o + 1))(o, nullptr); }
                { On on(1); ((GmManifestItem*)r)->Run(); }
                compare_logs("run after slot 0");
                void* back;
                { On on(0); back = at<FnD>(k.deleting)(o, nullptr, (unsigned)flags); }
                eq("returns this", (long long)(uintptr_t)back, (long long)(uintptr_t)o);
                {
                    On on(1);
                    GmManifestItem* item = (GmManifestItem*)r;
                    if (flags) {
                        // Through the table, as the original's queue would: the deleting destructor of the item's own class.
                        switch (&k - kKinds) {
                        case 0: delete (GmAnimationListItem*)item; break;
                        case 1: delete (GmSoundListItem*)item; break;
                        case 2: delete (GmScriptItem*)item; break;
                        case 3: delete (GmTriggerItem*)item; break;
                        case 4: delete (GmSubscriberItem*)item; break;
                        case 5: delete (GmVfxItem*)item; break;
                        case 6: delete (GmDynamicNodeItem*)item; break;
                        case 7: delete (GmSoundNodeItem*)item; break;
                        case 8: delete (GmMusicChainItem*)item; break;
                        case 9: delete (GmInstanceSoundItem*)item; break;
                        default: delete (GmCollisionSoundItem*)item; break;
                        }
                    } else {
                        switch (&k - kKinds) {
                        case 0: ((GmAnimationListItem*)item)->~GmAnimationListItem(); break;
                        case 1: ((GmSoundListItem*)item)->~GmSoundListItem(); break;
                        case 2: ((GmScriptItem*)item)->~GmScriptItem(); break;
                        case 3: ((GmTriggerItem*)item)->~GmTriggerItem(); break;
                        case 4: ((GmSubscriberItem*)item)->~GmSubscriberItem(); break;
                        case 5: ((GmVfxItem*)item)->~GmVfxItem(); break;
                        case 6: ((GmDynamicNodeItem*)item)->~GmDynamicNodeItem(); break;
                        case 7: ((GmSoundNodeItem*)item)->~GmSoundNodeItem(); break;
                        case 8: ((GmMusicChainItem*)item)->~GmMusicChainItem(); break;
                        case 9: ((GmInstanceSoundItem*)item)->~GmInstanceSoundItem(); break;
                        default: ((GmCollisionSoundItem*)item)->~GmCollisionSoundItem(); break;
                        }
                    }
                }
                compare_logs("deleted");
                if (!flags) {
                    same_item("destroyed", (GmManifestItem*)o, (GmManifestItem*)r, k.size);
                    std::free(o);
                    std::free(r);
                }
                g_null_from = 0;
                g_cur[0] = g_cur[1] = nullptr;
            }
        }
    }
}

// The loaders' own small parts.
static void run_loader_parts() {
    g_context = "Clear, slot 5";
    {
        reset_fakes();
        Pair p;
        parse_text(p, block("VFXDatabases", {"a"}) + block("SoundNodeDatabases", {"b"}), true);
        { On on(0); at<Fn0>(0x001421B0)(p.o, nullptr); }
        { On on(1); p.r->Clear(); }
        compare_logs("cleared");
        p.same("cleared");
        eqs("path", p.o->m_path, p.r->m_path);
        eq("state", p.o->m_state, p.r->m_state);
    }
    // Free on its own, slot 6, with every word's array: what it clears is compared before anything else clears it.
    g_context = "Free, slot 6";
    {
        reset_fakes();
        Pair p;
        std::string all;
        for (const char* w : kWords) all += block(w, {std::string("F\\") + w, std::string("G\\") + w});
        parse_text(p, all, true);
        { On on(0); at<Fn0>(0x00142930)(p.o, nullptr); }
        { On on(1); p.r->Free(); }
        compare_logs("freed");
        p.same("freed");
    }
    g_context = "deleting destructors";
    for (unsigned int flags : {0u, 1u}) {
        // The level manifest, with arrays to free.
        {
            reset_fakes();
            GmLevelManifest* o = (GmLevelManifest*)std::malloc(sizeof(GmLevelManifest));
            GmLevelManifest* r = (GmLevelManifest*)std::malloc(sizeof(GmLevelManifest));
            { On on(0); at<Fn0>(0x001428B0)(o, nullptr); }
            { On on(1); new (r) GmLevelManifest; }
            compare_logs("made");
            void* back;
            { On on(0); back = at<FnD>(0x00143DB0)(o, nullptr, flags); }
            eq("returns this", (long long)(uintptr_t)back, (long long)(uintptr_t)o);
            {
                On on(1);
                if (flags) delete r;
                else r->~GmLevelManifest();
            }
            compare_logs("deleted");
            if (!flags) {
                eq("state", o->m_state, r->m_state);
                eq("098", o->m_unknown098, r->m_unknown098);
                std::free(o);
                std::free(r);
            }
        }
        // The base alone, which the game never makes.
        {
            GmDatabaseManifest* o = (GmDatabaseManifest*)std::malloc(sizeof(GmDatabaseManifest));
            GmDatabaseManifest* r = (GmDatabaseManifest*)std::malloc(sizeof(GmDatabaseManifest));
            std::memset(o, 0xCD, sizeof(GmDatabaseManifest));
            std::memset(r, 0xCD, sizeof(GmDatabaseManifest));
            { On on(0); at<Fn0>(0x0015CD10)(o, nullptr); }
            { On on(1); new (r) GmDatabaseManifest; }
            compare_logs("made");
            eq("events", (long long)(uintptr_t)o->m_events, (long long)(uintptr_t)r->m_events);
            eq("registries", o->m_registryCount, r->m_registryCount);
            eq("098", o->m_unknown098, r->m_unknown098);
            eq("D4 left", o->m_unknownD4[0], r->m_unknownD4[0]);
            // Its own Parse, on a tokenizer of each side's.
            AtIoTextTokenizer* t[2];
            t[0] = (AtIoTextTokenizer*)std::malloc(sizeof(AtIoTextTokenizer));
            at<Fn0>(0x00022500)(t[0], nullptr);
            t[1] = new AtIoTextTokenizer;
            { On on(0); at<FnP>(0x0015C8E0)(o, nullptr, t[0]); }
            { On on(1); r->Parse(t[1]); }
            compare_logs("base parse");
            eq("keyword count", t[0]->m_keywordCount, t[1]->m_keywordCount);
            for (int h = 0; h < AtIoTextTokenizer::kHashSize; h++) {
                eq("hash", t[0]->m_hash[h] ? t[0]->m_hash[h]->m_id : -1, t[1]->m_hash[h] ? t[1]->m_hash[h]->m_id : -1);
                if (t[0]->m_hash[h] && t[1]->m_hash[h]) eqs("hash word", t[0]->m_hash[h]->m_name, t[1]->m_hash[h]->m_name);
            }
            // What the destructor clears, set; and one empty array of a base kind, which it frees.
            for (GmDatabaseManifest* m : {o, r}) {
                m->m_unknown098 = 0x98;
                m->m_eventCount = 0;
                m->m_events = (GmManifestItem*)fake_array(0, kEventItemSize, 0);
                m->m_head = (GmManifestItem*)(uintptr_t)4;
                m->m_name[0] = 'N';
            }
            void* back;
            { On on(0); back = at<FnD>(0x0015D9E0)(o, nullptr, flags); }
            eq("returns this", (long long)(uintptr_t)back, (long long)(uintptr_t)o);
            {
                On on(1);
                if (flags) delete r;
                else r->~GmDatabaseManifest();
            }
            compare_logs("base deleted");
            if (!flags) {
                eq("098 after", o->m_unknown098, r->m_unknown098);
                eq("events after", (long long)(uintptr_t)o->m_events, (long long)(uintptr_t)r->m_events);
                eq("head after", (long long)(uintptr_t)o->m_head, (long long)(uintptr_t)r->m_head);
                eq("name after", o->m_name[0], r->m_name[0]);
                std::free(o);
                std::free(r);
            }
        }
    }
    // The level manifest's own words, through its Parse.
    g_context = "level Parse";
    {
        AtIoTextTokenizer* t[2];
        t[0] = (AtIoTextTokenizer*)std::malloc(sizeof(AtIoTextTokenizer));
        at<Fn0>(0x00022500)(t[0], nullptr);
        t[1] = new AtIoTextTokenizer;
        reset_fakes();
        Pair p;
        { On on(0); at<FnP>(0x00142E00)(p.o, nullptr, t[0]); }
        { On on(1); p.r->Parse(t[1]); }
        compare_logs("level parse");
        eq("keyword count", t[0]->m_keywordCount, t[1]->m_keywordCount);
        eq("own count", (long long)t[0]->m_keywordCount, 26);
        for (int h = 0; h < AtIoTextTokenizer::kHashSize; h++) {
            eq("hash", t[0]->m_hash[h] ? t[0]->m_hash[h]->m_id : -1, t[1]->m_hash[h] ? t[1]->m_hash[h]->m_id : -1);
            if (t[0]->m_hash[h] && t[1]->m_hash[h]) eqs("hash word", t[0]->m_hash[h]->m_name, t[1]->m_hash[h]->m_name);
        }
    }
    // Finished on its own, with banks of none, and data of none: GOO registers whatever it holds, GOI only what it has.
    g_context = "Finished";
    for (int nulls : {0, 1}) {
        reset_fakes();
        g_null_from = nulls;
        Pair p;
        parse_text(p, block("SoundNodeDatabases", {"a", "b"}) + block("InstanceSoundNodeDatabases", {"c", "d"}) + block("SoundNoteDatabases", {"e"}), true);
        { On on(0); at<Fn0>(0x00143CE0)(p.o, nullptr); }
        { On on(1); p.r->Finished(); }
        compare_logs("finished again");
        g_null_from = 0;
    }
    // The flag a dynamic node is unloaded with.
    for (int a : {0, 1}) {
        for (int b : {0, 1}) {
            g_context = "dynamic flag " + std::to_string(a) + std::to_string(b);
            g_node_state.m_unknown24 = a;
            g_node_state.m_unknown28 = b;
            text_case(g_context, block("DynamicNodes", {"d1", "d2"}), true);
        }
    }
    g_node_state.m_unknown24 = g_node_state.m_unknown28 = 0;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
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
    if (pos.size() != 2) { std::printf("usage: oracle_manifest <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00020640, (const void*)o_clock);
    // What is not recovered.
    redirect(0x0015D000, (const void*)o_base_word);
    redirect(0x0015D920, (const void*)o_base_finished);
    redirect(0x0002EDD0, (const void*)o_scene_leave);
    redirect(0x00139F90, (const void*)o_audio_free_db);
    redirect(0x00071DC0, (const void*)o_registry);
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
    if (entries && !arm_entries(entries)) return 2;

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
    *(void**)(uintptr_t)0x003B3548 = &g_heap_token;  // the sound manager's, for the note banks
    GmAudioData::s_heap = &g_heap_token;
    *(AtHeapManager**)(uintptr_t)0x002C60ACu = &g_manager[0];
    AtHeapManager::s_instance = &g_manager[1];
    g_behavior.m_animationLists = (GmBehaviorListOwner*)OWNER(0);
    g_behavior.m_soundLists = (GmBehaviorListOwner*)OWNER(1);
    g_behavior.m_scripts = (GmScriptOwner*)OWNER(2);
    g_graph_owner.m_graph = (AtSpaceGraph*)(uintptr_t)0x6B6B;
    set_globals(&g_heap_token);
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_items();
    run_loader_parts();
    run_words();
    run_heap_cases();
    run_base_cases();

    int files = 0;
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
        run_file(line, data);
        files++;
    }
    std::fclose(index);
    if (files == 0) { std::printf("no files listed\n"); g_failures++; }
    report_entries();
    std::printf("%s: %d files; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
