// Differential oracle for the Flash user interface's .AFD reader, its sprites and frames (.AFP, .XFF, .AFF) and its shapes (.AFH), and the resource table's groups (Windows x86, local only). See docs/verification.md.
//
//   oracle_afd <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Real files: every user interface archive in the JAM cases beside the tokenizer cases (..\oracle-jam): each .AFD, .AFL, .AFP, .XFF and .AFH is served from its archive, as the game would find it, and the sprite table, the
// shape table and the whole .AFD are loaded on both sides and compared to the byte. Then written text and synthetic .XFF for each word, branch and error path.
//
// What stands in, the same on both sides: the graphics database loader (0x0001ADC0, AtDatabaseLoader::Load) and the count of its textures (0x0001C6F0), which have suites of their own; and the scripts and texts of an .AFD
// (0x000134A0, 0x00013CB0), which are not recovered. Both are logged with their arguments. The material owner, its factory and the loader are this file's.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioBank.h"
#include "at/AtDatabaseLoader.h"
#include "at/AtFlashLevel.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMemory.h"
#include "at/AtTextureDatabase.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 160).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 160).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }
template <class M> static uint32_t addr_of(M m) {
    static_assert(sizeof(M) == 4, "a plain member function");
    union { M m; uint32_t a; } u;
    u.m = m;
    return u.a;
}

#include "oracle_doubles.h"

AtHeap* AtHeapStack::s_contentHeap;

// No room from the Nth heap push on, on each side: every request is made under a push, so each one's failure can be reached in turn. Both sides' push go through here.
static int g_fail_at_push, g_pushes;
static void __cdecl push_counting(void* h) {
    if (g_recording) g_log[g_side].add('P', (uint32_t)(uintptr_t)h);
    if (g_fail_at_push && ++g_pushes == g_fail_at_push) g_fail_alloc = true;
}

static int g_heap_token, g_heap_a, g_heap_b;

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef char*(__fastcall* FnName)(void*, void*, const char*);
typedef void(__fastcall* FnFrame)(void*, void*, const unsigned char**, void*, void*, void*, void*);
typedef void(__cdecl* FnStatic)(void*);

// ---- files: an archive's, by path ---------------------------------------------------------------------------------------

static std::map<std::string, std::string> g_files;
static std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

static Mode g_mode = kNormal;
static bool g_fail_after_close;

class JamFs : public MemFs {
public:
    MemFile pool[2][8];
    int next[2] = {};
    // The harness's strings grow on the harness's heap, not the code under test's: nothing here is logged.
    const std::string* find(const char* path) {
        const bool was = g_recording;
        g_recording = false;
        const char* current = m_path.Current();
        auto it = g_files.find(upper(std::string(current ? current : "") + path));
        g_recording = was;
        return it == g_files.end() ? nullptr : &it->second;
    }
    int Open(const char* path, int flags, AtIoFile** out) override {
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(path, g_side);
        const std::string* data = find(path);
        if (open_status || data == nullptr) {
            *out = nullptr;
            return open_status ? open_status : 2;
        }
        MemFile& f = pool[g_side][next[g_side]++ & 7];
        f.side = g_side;
        f.data = (const unsigned char*)data->data();
        f.size = data->size();
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = 2;
        f.mode = g_mode;
        *out = &f;
        return 0;
    }
    int Close(AtIoFile* file) override {
        if (g_fail_after_close) g_fail_alloc = true;
        return MemFs::Close(file);
    }
    int IsFileMissing(const char* path) override {
        g_log[g_side].add('M');
        return find(path) ? 0 : 1;
    }
};

static JamFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];

// ---- what stands in --------------------------------------------------------------------------------------------------

static unsigned g_texture_dbs = 1, g_used_textures = 3;
static int g_texture_token;

class FakeLoader : public AtDatabaseLoader {
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
    void Unload() override { if (g_recording) g_log[g_side].add('U'); }
    AtTextureDatabase* TextureAt(unsigned int index) override { if (g_recording) g_log[g_side].add('T', index); return (AtTextureDatabase*)&g_texture_token; }
    AtMaterialDatabase* MaterialAt(unsigned int) override { return nullptr; }
    AtShaderDatabase* ShaderAt(unsigned int) override { return nullptr; }
    AtMeshDatabase* MeshAt(unsigned int) override { return nullptr; }
    AtLightDatabase* LightAt(unsigned int) override { return nullptr; }
    AtFontDatabase* FontAt(unsigned int) override { return nullptr; }
    AtImageDatabase* ImageAt(unsigned int) override { return nullptr; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int) override { return nullptr; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int) override { return nullptr; }
    AtDatabaseLoader* LoaderAt(unsigned int) override { return nullptr; }
};
static FakeLoader* g_loaders[2];

class FakeFactory : public AtFlashGraphicsFactory {
public:
    AtDatabaseLoader* MakeLoader() override { if (g_recording) g_log[g_side].add('L'); return g_loaders[g_side]; }
    void FreeLoader(AtDatabaseLoader* loader) override { if (g_recording) g_log[g_side].add('K', loader == g_loaders[g_side]); }
};
static FakeFactory g_factory;

struct FakeOwner : AtMaterialOwner {
    AtFlashGraphicsFactory* GraphicsFactory() override { if (g_recording) g_log[g_side].add('G'); return &g_factory; }
};
static FakeOwner* g_owner;
static AtFlashGraphics g_graphics;
static AtFlashApplication g_app;
static AtFlashEnvironment g_env;
static unsigned char* g_level[2];

static void __fastcall loader_load(AtDatabaseLoader* self, void*, AtMaterialOwner* owner, const char* name, int mode, const char* prefix, int inDirectory) {
    if (g_recording) {
        g_log[g_side].add('A', (uint32_t)mode);
        g_log[g_side].add('a', (owner == g_owner) | (prefix != nullptr) << 1 | inDirectory << 2 | (self == g_loaders[g_side]) << 3);
        record_message(name, g_side);
    }
    self->m_textureCount = (unsigned short)g_texture_dbs;
}
static unsigned __fastcall count_used(void* self, void*) {
    if (g_recording) g_log[g_side].add('u', self == &g_texture_token);
    return g_used_textures;
}
static void __fastcall script_load(void* self, void*, const char* name, int mode, void* app) {
    if (g_recording) {
        g_log[g_side].add('S', (uint32_t)mode);
        g_log[g_side].add('s', (uint32_t)((unsigned char*)self - g_level[g_side]) | (app == &g_app) << 16);
        record_message(name, g_side);
    }
}
static void __fastcall text_load(void* self, void*, const char* name, int mode, void* owner, int own) {
    if (g_recording) {
        g_log[g_side].add('X', (uint32_t)mode);
        g_log[g_side].add('x', (uint32_t)((unsigned char*)self - g_level[g_side]) | (owner == g_owner) << 16 | own << 20);
        record_message(name, g_side);
    }
}

// ---- comparing --------------------------------------------------------------------------------------------------------

static void same_bytes(const char* what, const void* o, const void* r, size_t from, size_t to) {
    const unsigned char* a = (const unsigned char*)o;
    const unsigned char* b = (const unsigned char*)r;
    for (size_t i = from; i < to; i++) {
        if (a[i] != b[i]) {
            eq((std::string(what) + " byte " + std::to_string(i)).c_str(), a[i], b[i]);
            return;
        }
    }
    g_checks++;
}

static void same_groups(const AtFlashResourceTable* o, const AtFlashResourceTable* r) {
    const AtFlashResourceTable::Group* a = o->m_firstGroup;
    const AtFlashResourceTable::Group* b = r->m_firstGroup;
    int n = 0;
    for (; a && b && n < 64; a = a->m_next, b = b->m_next, n++) {
        eq("group's first", a->m_first != nullptr, b->m_first != nullptr);
        eq("group's last", a->m_last != nullptr, b->m_last != nullptr);
        eq("group's back link", a->m_prev == nullptr, b->m_prev == nullptr);
        if (!a->m_next) eq("last group", a == o->m_lastGroup, true);
        if (!b->m_next) eq("last group", b == r->m_lastGroup, true);
    }
    eq("groups end together", a == nullptr, b == nullptr);
    eq("groups", n, n);
}

static void same_base(const AtFlashResourceTable* o, const AtFlashResourceTable* r) {
    eq("initialised", o->m_initialised, r->m_initialised);
    eq("first group", o->m_firstGroup != nullptr, r->m_firstGroup != nullptr);
    eq("last group", o->m_lastGroup != nullptr, r->m_lastGroup != nullptr);
    eq("name", std::memcmp(o->m_name, r->m_name, 8), 0);
    eq("items", o->m_items != nullptr, r->m_items != nullptr);
    eq("item count", o->m_itemCount, r->m_itemCount);
    eq("group count", o->m_groupCount, r->m_groupCount);
    eq("heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    same_groups(o, r);
}

// The resource's links are not touched by anything here, so they keep the heap's fill on both sides.
static void same_resource(const AtFlashResource* o, const AtFlashResource* r) {
    same_bytes("resource", o, r, 4, sizeof(AtFlashResource));
}

static void same_name(const char* what, const char* o, const char* r) {
    eq(what, o != nullptr, r != nullptr);
    if (o && r) eqs(what, o, r);
}

static void same_tag(const AtFlashTag* o, const AtFlashTag* r, size_t size, uint32_t vft) {
    eq("tag's vftable", *(const uint32_t*)o, vft);
    if (size == sizeof(AtFpPlaceObject2Tag)) {
        same_bytes("place", o, r, 4, offsetof(AtFpPlaceObject2Tag, m_name));
        same_bytes("place", o, r, offsetof(AtFpPlaceObject2Tag, m_name) + 4, size);
        same_name("place's name", ((const AtFpPlaceObject2Tag*)o)->m_name, ((const AtFpPlaceObject2Tag*)r)->m_name);
    } else {
        same_bytes("tag", o, r, 4, size);
    }
}

template <class T> static void same_array(const char* what, const T* o, const T* r, uint32_t vft) {
    eq(what, o != nullptr, r != nullptr);
    if (!o || !r) return;
    const unsigned n = ((const unsigned*)o)[-1];
    eq((std::string(what) + " count").c_str(), n, ((const unsigned*)r)[-1]);
    if (n != ((const unsigned*)r)[-1]) return;
    for (unsigned i = 0; i < n; i++) same_tag(&o[i], &r[i], sizeof(T), vft);
}

// Which run a tag is from, and where in it.
static long long tag_place(const AtFlashFrameTable* t, const AtFlashTag* tag) {
    const unsigned char* p = (const unsigned char*)tag;
    const unsigned char* runs[4] = {(const unsigned char*)t->m_actions, (const unsigned char*)t->m_places, (const unsigned char*)t->m_removes, (const unsigned char*)t->m_sounds};
    const size_t sizes[4] = {sizeof(AtFlashActionTag), sizeof(AtFpPlaceObject2Tag), sizeof(AtFlashRemoveTag), sizeof(AtFlashSoundTag)};
    for (int k = 0; k < 4; k++) {
        if (runs[k] && p >= runs[k] && p < runs[k] + sizes[k] * ((const unsigned*)runs[k])[-1]) return (long long)k << 32 | (p - runs[k]) / sizes[k];
    }
    return -1;
}

static unsigned g_frames, g_tags, g_sprites, g_shapes;

static void same_frame(const AtFlashFrameTable* ot, const AtFlashFrameTable* rt, const AtFlashFrame* o, const AtFlashFrame* r) {
    eq("frame's vftable", *(const uint32_t*)o, 0x00272D2C);
    same_resource(o, r);
    same_bytes("frame", o, r, offsetof(AtFlashFrame, m_flags), offsetof(AtFlashFrame, m_tags));
    eq("frame's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("frame's tags", o->m_tags != nullptr, r->m_tags != nullptr);
    if (o->m_tags && r->m_tags && o->m_tagCount == r->m_tagCount) {
        for (unsigned i = 0; i < o->m_tagCount; i++) eq("tag", tag_place(ot, o->m_tags[i]), tag_place(rt, r->m_tags[i]));
    }
}

static void same_frames(const AtFlashFrameTable* o, const AtFlashFrameTable* r, bool count) {
    eq("frames' vftable", *(const uint32_t*)o, 0x00272108);
    same_base(o, r);
    eq("frames loaded", o->m_loaded, r->m_loaded);
    same_array("actions", o->m_actions, r->m_actions, 0x00272D30);
    same_array("places", o->m_places, r->m_places, 0x00272DC0);
    same_array("removes", o->m_removes, r->m_removes, 0x00272DDC);
    same_array("sounds", o->m_sounds, r->m_sounds, 0x00272DF8);
    if (o->m_items && r->m_items && o->m_itemCount == r->m_itemCount && o->m_loaded) {
        for (unsigned i = 0; i < o->m_itemCount; i++) {
            eq("frame held", o->m_items[i] != nullptr, r->m_items[i] != nullptr);
            if (!o->m_items[i] || !r->m_items[i]) continue;
            eq("frame's place", (char*)o->m_items[i] - (char*)o->m_items[0], (char*)r->m_items[i] - (char*)r->m_items[0]);
            same_frame(o, r, (const AtFlashFrame*)o->m_items[i], (const AtFlashFrame*)r->m_items[i]);
            if (count) {
                g_frames++;
                g_tags += ((const AtFlashFrame*)r->m_items[i])->m_tagCount;
            }
        }
    }
}

static void same_sprite(const AtFpSprite* o, const AtFpSprite* r) {
    eq("sprite's vftable", *(const uint32_t*)o, 0x00271F40);
    same_resource(o, r);
    same_bytes("sprite", o, r, offsetof(AtFpSprite, m_used), offsetof(AtFpSprite, m_refs));
    eq("sprite's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("sprite's refs", o->m_refs != nullptr, r->m_refs != nullptr);
    if (o->m_refs && r->m_refs && o->m_refCapacity == r->m_refCapacity && o->m_used) {
        // A run past the capacity writes into the heap's padding: look there too.
        for (unsigned i = 0; i < o->m_refCapacity + 4; i++) {
            eq("ref's frame", o->m_refs[i].m_frame, r->m_refs[i].m_frame);
            eq("ref's word", o->m_refs[i].m_unknown08, r->m_refs[i].m_unknown08);
            if (i < o->m_refCapacity) same_name("ref's name", o->m_refs[i].m_name, r->m_refs[i].m_name);
            else eq("ref's name past the end", (uintptr_t)o->m_refs[i].m_name, (uintptr_t)r->m_refs[i].m_name);
        }
    }
}

static void same_sprites(const AtFlashSpriteTable* o, const AtFlashSpriteTable* r, bool count) {
    eq("sprites' vftable", *(const uint32_t*)o, 0x00271080);
    same_base(o, r);
    eq("sprites loaded", o->m_loaded, r->m_loaded);
    same_frames(&o->m_frames, &r->m_frames, count);
    if (o->m_items && r->m_items && o->m_itemCount == r->m_itemCount && o->m_loaded) {
        for (unsigned i = 0; i < o->m_itemCount; i++) {
            eq("sprite held", o->m_items[i] != nullptr, r->m_items[i] != nullptr);
            if (!o->m_items[i] || !r->m_items[i]) continue;
            eq("sprite's place", (char*)o->m_items[i] - (char*)o->m_items[0], (char*)r->m_items[i] - (char*)r->m_items[0]);
            same_sprite((const AtFpSprite*)o->m_items[i], (const AtFpSprite*)r->m_items[i]);
            if (count) g_sprites++;
        }
    }
}

static void same_shapes(const AtFlashShapeTable* o, const AtFlashShapeTable* r, bool count) {
    eq("shapes' vftable", *(const uint32_t*)o, 0x00271158);
    same_base(o, r);
    eq("shapes loaded", o->m_loaded, r->m_loaded);
    eq("owner", (uintptr_t)o->m_owner, (uintptr_t)r->m_owner);
    eq("loader", o->m_loader == g_loaders[0], r->m_loader == g_loaders[1]);
    eq("loader null", o->m_loader == nullptr, r->m_loader == nullptr);
    eq("texture count", o->m_textureCount, r->m_textureCount);
    eq("textures", o->m_textures != nullptr, r->m_textures != nullptr);
    if (o->m_textures && r->m_textures && o->m_textureCount == r->m_textureCount) {
        for (unsigned i = 0; i < o->m_textureCount; i++) {
            eq("texture's references", o->m_textures[i].m_references, r->m_textures[i].m_references);
            eq("texture", (uintptr_t)o->m_textures[i].m_texture, (uintptr_t)r->m_textures[i].m_texture);
        }
    }
    if (o->m_items && r->m_items && o->m_itemCount == r->m_itemCount && o->m_loaded) {
        for (unsigned i = 0; i < o->m_itemCount; i++) {
            eq("shape held", o->m_items[i] != nullptr, r->m_items[i] != nullptr);
            if (!o->m_items[i] || !r->m_items[i]) continue;
            eq("shape's vftable", *(const uint32_t*)o->m_items[i], 0x00272210);
            eq("shape's place", (char*)o->m_items[i] - (char*)o->m_items[0], (char*)r->m_items[i] - (char*)r->m_items[0]);
            same_bytes("shape", o->m_items[i], r->m_items[i], 4, sizeof(AtFlashShape));
            if (count) g_shapes++;
        }
    }
}

// ---- making and running ----------------------------------------------------------------------------------------------

template <class T> static T* make(uint32_t ctor, unsigned char fill, bool original) {
    unsigned char* p = (unsigned char*)std::malloc(sizeof(T));
    std::memset(p, fill, sizeof(T));
    if (original) {
        eq("constructor returns this", (uintptr_t)at<FnCtor>(ctor)(p, nullptr), (uintptr_t)p);
        return (T*)p;
    }
    return new (p) T;
}

static int g_path_after[2];
template <class F> static bool run_side(int side, F f) {
    if (g_fail_after_close) g_fail_alloc = false;  // Each side has its room until its own first close.
    if (g_fail_at_push) {
        g_fail_alloc = false;
        g_pushes = 0;
    }
    g_fs->m_path.m_top = 0;  // A crash leaves a directory pushed.
    const bool crashed = guarded([&] { On on(side); f(); });
    g_path_after[side] = g_fs->m_path.m_top;
    g_recording = false;
    g_side = 0;
    return crashed;
}

static void set_heaps(void* fallback) {
    *(void**)(uintptr_t)0x002C6270 = fallback;
    AtFlashResourceTable::s_defaultHeap = fallback;
}

// A member in the first group, counted in no times: recycling it unlinks it and clears its table.
static AtFlashResource g_member[2];
static void add_member(AtFlashResourceTable* t, int side) {
    if (!t->m_firstGroup) return;
    std::memset(&g_member[side], 0x44, sizeof g_member[side]);
    g_member[side].m_uses = 0;
    g_member[side].m_groupNext = nullptr;
    g_member[side].m_groupPrev = nullptr;
    t->m_firstGroup->m_first = t->m_firstGroup->m_last = &g_member[side].m_groupNext;
}

// The sprites alone: load, compare, load again over it, clear twice, destroy.
static void run_sprites(const char* name, int mode, bool count, bool again = false, bool destroyLoaded = false) {
    g_fill = 0xCD;
    AtFlashSpriteTable* o = make<AtFlashSpriteTable>(0x00012280, 0xCD, true);
    AtFlashSpriteTable* r = make<AtFlashSpriteTable>(0x00012280, 0xCD, false);
    same_sprites(o, r, false);
    same_bytes("constructed sprites", o, r, 4, offsetof(AtFlashSpriteTable, m_frames));
    same_bytes("constructed frames", &o->m_frames, &r->m_frames, 4, sizeof(AtFlashFrameTable));
    const bool oc = run_side(0, [&] { at<Fn2>(0x00012370)(o, nullptr, (uintptr_t)name, (uintptr_t)mode); });
    const bool rc = run_side(1, [&] { r->Load(name, mode); });
    eq("crash", oc, rc);
    same_sprites(o, r, count && !oc);
    compare_logs("sprites loaded");
    eq("sprites' heap", (uintptr_t)*(void**)(uintptr_t)0x002C626C, (uintptr_t)AtFpSprite::s_heap);
    if (oc || rc) return;  // What a crash leaves is not compared further, nor freed.
    if (again) {
        const bool oa = run_side(0, [&] { at<Fn2>(0x00012370)(o, nullptr, (uintptr_t)name, (uintptr_t)mode); });
        const bool ra = run_side(1, [&] { r->Load(name, mode); });
        eq("crash", oa, ra);
        same_sprites(o, r, false);
        compare_logs("sprites loaded again");
        if (oa || ra) return;
    }
    if (destroyLoaded) {
        run_side(0, [&] { at<FnDel>(0x000126C0)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("sprites destroyed loaded");
        return;
    }
    add_member(o, 0);
    add_member(r, 1);
    o->m_heap = r->m_heap = &g_heap_a;
    run_side(0, [&] { at<Fn0>(0x000122E0)(o, nullptr); at<Fn0>(0x000122E0)(o, nullptr); });
    run_side(1, [&] { r->ClearSprites(); r->ClearSprites(); });
    same_sprites(o, r, false);
    same_bytes("group member", &g_member[0], &g_member[1], offsetof(AtFlashResource, m_index), sizeof(AtFlashResource));
    compare_logs("sprites cleared");
    run_side(0, [&] { eq("returns this", (uintptr_t)at<FnDel>(0x000126C0)(o, nullptr, 1), (uintptr_t)o); });
    run_side(1, [&] { delete r; });
    compare_logs("sprites destroyed");
}

static void run_shapes(const char* name, int mode, bool count, bool again = false, bool destroyLoaded = false) {
    g_fill = 0xCD;
    AtFlashShapeTable* o = make<AtFlashShapeTable>(0x00012BD0, 0xCD, true);
    AtFlashShapeTable* r = make<AtFlashShapeTable>(0x00012BD0, 0xCD, false);
    same_bytes("constructed shapes", o, r, 4, sizeof(AtFlashShapeTable));
    const bool oc = run_side(0, [&] { at<Fn3>(0x00013190)(o, nullptr, (uintptr_t)name, (uintptr_t)mode, (uintptr_t)&g_app); });
    const bool rc = run_side(1, [&] { r->Load(name, mode, &g_app); });
    eq("crash", oc, rc);
    same_shapes(o, r, count && !oc);
    compare_logs("shapes loaded");
    eq("path left", g_path_after[0], g_path_after[1]);
    if (!oc) eq("path left as found", g_path_after[0], 0);
    if (oc || rc) return;
    if (again) {
        const bool oa = run_side(0, [&] { at<Fn3>(0x00013190)(o, nullptr, (uintptr_t)name, (uintptr_t)mode, (uintptr_t)&g_app); });
        const bool ra = run_side(1, [&] { r->Load(name, mode, &g_app); });
        eq("crash", oa, ra);
        same_shapes(o, r, false);
        compare_logs("shapes loaded again");
        if (oa || ra) return;
    }
    if (destroyLoaded) {
        run_side(0, [&] { at<FnDel>(0x00013360)(o, nullptr, 1); });
        run_side(1, [&] { delete r; });
        compare_logs("shapes destroyed loaded");
        return;
    }
    add_member(o, 0);
    add_member(r, 1);
    // A texture still held is reported, and freed all the same.
    if (o->m_textures && o->m_textureCount > 1) o->m_textures[1].m_references = r->m_textures[1].m_references = 2;
    run_side(0, [&] { at<Fn0>(0x00012C00)(o, nullptr); at<Fn0>(0x00012C00)(o, nullptr); });
    run_side(1, [&] { r->ClearShapes(); r->ClearShapes(); });
    same_shapes(o, r, false);
    same_bytes("group member", &g_member[0], &g_member[1], offsetof(AtFlashResource, m_index), sizeof(AtFlashResource));
    compare_logs("shapes cleared");
    run_side(0, [&] { eq("returns this", (uintptr_t)at<FnDel>(0x00013360)(o, nullptr, 1), (uintptr_t)o); });
    run_side(1, [&] { delete r; });
    compare_logs("shapes destroyed");
}

static void same_level(const unsigned char* o, const unsigned char* r) {
    const AtFlashLevel* a = (const AtFlashLevel*)o;
    const AtFlashLevel* b = (const AtFlashLevel*)r;
    same_sprites(&a->m_sprites, &b->m_sprites, false);
    same_shapes(&a->m_shapes, &b->m_shapes, false);
    eq("block", a->m_block != nullptr, b->m_block != nullptr);
    same_bytes("level, head", o, r, 0, offsetof(AtFlashLevel, m_shapes));
    same_bytes("level, sounds", o, r, offsetof(AtFlashLevel, m_sounds), offsetof(AtFlashLevel, m_sprites));
    same_bytes("level, texts", o, r, offsetof(AtFlashLevel, m_texts), offsetof(AtFlashLevel, m_block));
    same_bytes("level, tail", o, r, offsetof(AtFlashLevel, m_heap), sizeof(AtFlashLevel));
}

static unsigned g_levels;

static void run_level(const char* name, int platformSpecific, unsigned char fill, bool count) {
    g_fill = 0xCD;
    static unsigned char o[sizeof(AtFlashLevel)], r[sizeof(AtFlashLevel)];
    g_level[0] = o;
    g_level[1] = r;
    std::memset(o, fill, sizeof o);
    std::memset(r, fill, sizeof r);
    at<FnCtor>(0x00012BD0)(o + offsetof(AtFlashLevel, m_shapes), nullptr);
    at<FnCtor>(0x00012280)(o + offsetof(AtFlashLevel, m_sprites), nullptr);
    new (&((AtFlashLevel*)r)->m_shapes) AtFlashShapeTable;
    new (&((AtFlashLevel*)r)->m_sprites) AtFlashSpriteTable;
    g_env.m_platformSpecificFiles = platformSpecific;
    for (unsigned char* p : {o, r}) {
        AtFlashLevel* level = (AtFlashLevel*)p;
        level->m_environment = &g_env;
    }
    { On on(0); at<Fn0>(0x000113B0)(o, nullptr); }
    { On on(1); ((AtFlashLevel*)r)->Reset(); }
    same_level(o, r);
    compare_logs("level reset");
    for (unsigned char* p : {o, r}) {
        ((AtFlashLevel*)p)->m_environment = &g_env;
        ((AtFlashLevel*)p)->m_heap = nullptr;
    }
    const bool oc = run_side(0, [&] { at<Fn2>(0x00011770)(o, nullptr, (uintptr_t)name, 77); });
    const bool rc = run_side(1, [&] { ((AtFlashLevel*)r)->ReadDefinition(name, 77); });
    eq("crash", oc, rc);
    same_level(o, r);
    compare_logs("level read");
    if (count && !oc) g_levels++;
    g_fs->m_path.m_top = 0;
    if (oc || rc) return;
    run_side(0, [&] { at<Fn0>(0x000122E0)(o + offsetof(AtFlashLevel, m_sprites), nullptr); at<Fn0>(0x00012C00)(o + offsetof(AtFlashLevel, m_shapes), nullptr); });
    run_side(1, [&] { ((AtFlashLevel*)r)->m_sprites.ClearSprites(); ((AtFlashLevel*)r)->m_shapes.ClearShapes(); });
    compare_logs("level's tables cleared");
    std::free(((AtFlashLevel*)o)->m_block);
    std::free(((AtFlashLevel*)r)->m_block);
}

// ---- synthetic .XFF -------------------------------------------------------------------------------------------------

struct Xff {
    std::string data;  // after the two headers
    unsigned frames = 0, places = 0, actions = 0, removes = 0, sounds = 0;
    void u8(unsigned v) { data += (char)v; }
    void u16(unsigned v) { u8(v & 0xFF); u8(v >> 8); }
    void u32(unsigned v) { u16(v & 0xFFFF); u16(v >> 16); }
    void frame(unsigned index, unsigned p, unsigned a, unsigned r, unsigned s) {
        frames++;
        u32(index); u8(p); u8(a); u8(r); u8(s);
        for (unsigned i = 0; i < r; i++, removes++) u32(0x200 + removes);
        for (unsigned i = 0; i < p; i++, places++) {
            // Name, transform and color, as offsets past the record, into a tail of their own.
            const unsigned n = places % 4;
            u8(1 + places); u8(0x77); u8(2 + places); u8(3 + places); u16(0x1000 + places); u16(0x2000 + places); u32(0x30000 + places);
            u32(n & 1 ? 0x40 : 0); u32(n & 2 ? 0x60 : 0); u32(n == 3 || n == 1 ? 0x80 : 0);
        }
        for (unsigned i = 0; i < a; i++, actions++) u32(0x100 + actions);
        for (unsigned i = 0; i < s; i++, sounds++) u32(0x300 + sounds);
    }
    std::string file(unsigned offset = 0x20, int sizeFix = 0) const {
        std::string tail(0x200, '\0');
        // What every place's offsets find: from wherever the record ends, so the tail repeats a pattern. Bytes past the data are the padding's.
        std::string all = data + tail;
        for (size_t i = data.size(); i < all.size(); i++) all[i] = (char)(i * 7 + 1);
        for (size_t i = data.size(); i + 8 < all.size(); i += 0x20) std::memcpy(&all[i], "Nm", 3);
        std::string out("HVSIXFF\0\1\0\1\0\0\0\0\0", 16);
        auto put16 = [&](unsigned v) { out += (char)(v & 0xFF); out += (char)(v >> 8); };
        put16(frames); put16(offset); put16((unsigned)(all.size() + sizeFix) & 0xFFFF); put16((unsigned)(all.size() + sizeFix) >> 16);
        put16(places); put16(actions); put16(removes); put16(sounds);
        out.resize(offset, '\x5A');
        return out + all;
    }
};

// ---- the groups -----------------------------------------------------------------------------------------------------

static void run_groups() {
    for (unsigned char fill : {0xCD, 0x00}) {
        g_context = "groups";
        g_fill = fill;
        AtFlashResourceTable* o = make<AtFlashResourceTable>(0x000172A0, fill, true);
        AtFlashResourceTable* r = make<AtFlashResourceTable>(0x000172A0, fill, false);
        run_side(0, [&] { at<Fn0>(0x00017370)(o, nullptr); });
        run_side(1, [&] { r->Init(); });
        same_base(o, r);
        compare_logs("init");
        // Three members in the first group, counted in 2, 0 and 1 times; then the group recycled, kept or freed.
        for (int keep : {1, 0, 7}) {
            AtFlashResource items[2][3];
            for (int side = 0; side < 2; side++) {
                AtFlashResourceTable* t = side ? r : o;
                std::memset(items[side], 0x44, sizeof items[side]);
                for (int i = 0; i < 3; i++) {
                    items[side][i].m_index = i;
                    items[side][i].m_references = 10 * (i + 1);
                    items[side][i].m_uses = i == 0 ? 2 : i == 1 ? 0 : 1;
                    items[side][i].m_groupNext = i < 2 ? &items[side][i + 1].m_groupNext : nullptr;
                    items[side][i].m_groupPrev = i > 0 ? &items[side][i - 1].m_groupNext : nullptr;
                }
                t->m_items = (AtFlashResource**)std::malloc(3 * sizeof(void*));
                for (int i = 0; i < 3; i++) t->m_items[i] = &items[side][i];
                t->m_firstGroup->m_first = &items[side][0].m_groupNext;
                t->m_firstGroup->m_last = &items[side][2].m_groupNext;
            }
            if (keep == 7) {
                run_side(0, [&] { at<Fn1>(0x00017290)(o, nullptr, 0); });
                run_side(1, [&] { r->Slot3(0); });
            } else {
                run_side(0, [&] { at<Fn1>(0x000170E0)(o, nullptr, (uintptr_t)keep); });
                run_side(1, [&] { r->RecycleGroup(keep); });
            }
            same_base(o, r);
            compare_logs("recycled");
            for (int i = 0; i < 3; i++) {
                same_bytes("member", &items[0][i], &items[1][i], offsetof(AtFlashResource, m_index), sizeof(AtFlashResource));
                eq("member's links", items[0][i].m_groupNext == nullptr, items[1][i].m_groupNext == nullptr);
                eq("member's links", items[0][i].m_groupPrev == nullptr, items[1][i].m_groupPrev == nullptr);
            }
            std::free(o->m_items);
            std::free(r->m_items);
            o->m_items = r->m_items = nullptr;
            if (keep == 0) {
                // Freed: the count is not kept by RecycleGroup, so set it as SetGroupCount would.
                o->m_groupCount = r->m_groupCount = 5;
            }
        }
        for (unsigned n : {2u, 2u, 9u, 3u, 0u}) {
            run_side(0, [&] { at<Fn1>(0x000171B0)(o, nullptr, n); });
            run_side(1, [&] { r->SetGroupCount(n); });
            same_base(o, r);
            compare_logs("group count");
        }
        // No groups: recycling crashes on both.
        {
            const bool oc = run_side(0, [&] { at<Fn1>(0x000170E0)(o, nullptr, 0); });
            const bool rc = run_side(1, [&] { r->RecycleGroup(0); });
            eq("crash", oc, rc);
            eq("crashed", oc, true);
            compare_logs("no groups");
        }
        // Init again: cleared first.
        run_side(0, [&] { at<Fn1>(0x000171B0)(o, nullptr, 4); });
        run_side(1, [&] { r->SetGroupCount(4); });
        o->m_initialised = r->m_initialised = 1;
        run_side(0, [&] { at<Fn0>(0x00017370)(o, nullptr); });
        run_side(1, [&] { r->Init(); });
        same_base(o, r);
        compare_logs("init again");
        // Groups on each heap; and none.
        for (void* own : {(void*)&g_heap_a, (void*)nullptr}) {
            for (void* fallback : {(void*)&g_heap_b, (void*)nullptr}) {
                set_heaps(fallback);
                o->m_heap = r->m_heap = own;
                run_side(0, [&] { at<Fn1>(0x000171B0)(o, nullptr, 8); });
                run_side(1, [&] { r->SetGroupCount(8); });
                same_base(o, r);
                compare_logs("groups on a heap");
            }
        }
        set_heaps(&g_heap_token);
        // No room for a group.
        g_fail_alloc = true;
        {
            const bool oc = run_side(0, [&] { at<Fn1>(0x000171B0)(o, nullptr, 9); });
            const bool rc = run_side(1, [&] { r->SetGroupCount(9); });
            eq("crash", oc, rc);
            compare_logs("no room for a group");
        }
        g_fail_alloc = false;
        // The default heap's setter.
        run_side(0, [&] { at<FnStatic>(0x00016F40)(&g_heap_a); });
        eq("default heap set", (uintptr_t)*(void**)(uintptr_t)0x002C6270, (uintptr_t)&g_heap_a);
        run_side(1, [&] { AtFlashResourceTable::SetDefaultHeap(&g_heap_a); });
        eq("default heap set", (uintptr_t)AtFlashResourceTable::s_defaultHeap, (uintptr_t)&g_heap_a);
        set_heaps(&g_heap_token);
        // 0x00058AF0, called by the .AFL reader on its tables; here on its own.
        run_side(0, [&] { at<Fn1>(0x00058AF0)(o, nullptr, (uintptr_t)&g_heap_b); });
        run_side(1, [&] { r->SetHeap(&g_heap_b); });
        eq("heap set", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
        std::free(o);
        std::free(r);
    }
}

// ---- the readers directly --------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class DirectTokenizer : public AtIoTextTokenizer {
public:
    int m_errors = 0;
    void Error(int code) override {
        g_log[g_side].add('E', code);
        if (++m_errors > 12) *(volatile int*)8 = 0;  // Does not come back, as in the game. guarded() takes it from here.
    }
};

struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    Direct(const std::string& t, uint32_t words, unsigned count) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords((const AtIoKeyword*)(uintptr_t)words, count, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void run_sprite_text(const std::string& text, void* heap, void* fallback) {
    g_context = "sprite [" + text + "]";
    g_fill = 0x33;
    *(void**)(uintptr_t)0x002C626C = fallback;
    AtFpSprite::s_heap = fallback;
    AtFpSprite* o = make<AtFpSprite>(0x00016AE0, 0x33, true);
    AtFpSprite* r = make<AtFpSprite>(0x00016AE0, 0x33, false);
    same_sprite(o, r);
    o->m_heap = r->m_heap = heap;
    Direct da(text, 0x00271034, 4), db(text, 0x00271034, 4);
    const bool oc = run_side(0, [&] { at<Fn1>(0x00016EE0)(o, nullptr, (uintptr_t)&da.tokenizer); });
    const bool rc = run_side(1, [&] { AtFlashResource* x = r; x->Read(&db.tokenizer); });
    eq("crash", oc, rc);
    same_sprite(o, r);
    compare_logs("sprite read");
    eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
    if (oc || rc) return;
    // Read again over what is there: the old list goes first.
    Direct dc(text, 0x00271034, 4), dd(text, 0x00271034, 4);
    const bool oc2 = run_side(0, [&] { at<Fn1>(0x00016C80)(o, nullptr, (uintptr_t)&dc.tokenizer); });
    const bool rc2 = run_side(1, [&] { r->ReadData(&dd.tokenizer); });
    eq("crash", oc2, rc2);
    same_sprite(o, r);
    compare_logs("sprite read again");
    if (oc2 || rc2) return;
    run_side(0, [&] { at<Fn0>(0x00016EF0)(o, nullptr); });
    run_side(1, [&] { r->~AtFpSprite(); });
    same_bytes("sprite destroyed", o, r, 4, sizeof(AtFpSprite));
    compare_logs("sprite destroyed");
    std::free(o);
    std::free(r);
}

static void run_shape_text(const std::string& text) {
    g_context = "shape [" + text + "]";
    g_fill = 0x33;
    AtFlashShape* o = make<AtFlashShape>(0x00019600, 0x33, true);
    AtFlashShape* r = make<AtFlashShape>(0x00019600, 0x33, false);
    same_bytes("shape", o, r, 4, sizeof(AtFlashShape));
    Direct da(text, 0x002710FC, 5), db(text, 0x002710FC, 5);
    const bool oc = run_side(0, [&] { at<Fn1>(0x00019660)(o, nullptr, (uintptr_t)&da.tokenizer); });
    const bool rc = run_side(1, [&] { AtFlashResource* x = r; x->Read(&db.tokenizer); });
    eq("crash", oc, rc);
    same_bytes("shape", o, r, 4, sizeof(AtFlashShape));
    compare_logs("shape read");
    eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
    o->m_unknown30 = r->m_unknown30 = &g_heap_a;
    o->m_heap = r->m_heap = &g_heap_b;
    run_side(0, [&] { at<Fn0>(0x00019670)(o, nullptr); });
    run_side(1, [&] { r->~AtFlashShape(); });
    same_bytes("shape destroyed", o, r, 4, sizeof(AtFlashShape));
    std::free(o);
    std::free(r);
}

// A frame, read from bytes on its own, on each heap; then cleared.
static void run_frame_bytes(void* heap, void* fallback) {
    g_context = "frame bytes";
    g_fill = 0x33;
    *(void**)(uintptr_t)0x002C6324 = fallback;
    AtFlashFrame::s_heap = fallback;
    *(void**)(uintptr_t)0x002C63D0 = fallback;
    AtFpPlaceObject2Tag::s_heap = fallback;
    Xff x;
    x.frame(0x12345, 3, 2, 2, 1);
    x.frame(0x10006, 0, 0, 0, 0);
    const std::string file = x.file();
    const unsigned char* data = (const unsigned char*)file.data() + 0x20;
    AtFlashFrame* o = make<AtFlashFrame>(0x00023460, 0x33, true);
    AtFlashFrame* r = make<AtFlashFrame>(0x00023460, 0x33, false);
    same_bytes("constructed frame", o, r, 4, sizeof(AtFlashFrame));
    o->m_heap = r->m_heap = heap;
    o->m_flags = r->m_flags = 0x10;  // Kept: the reader adds its flags.
    // Each run of tags, made by each side's constructors.
    static unsigned char runs[2][4][0x58 * 4];
    for (int side = 0; side < 2; side++) std::memset(runs[side], 0x33, sizeof runs[side]);
    const uint32_t ctors[4] = {0x00023C10, 0x00024210, 0x000246C0, 0x000248B0};
    const size_t sizes[4] = {0x18, 0x58, 0x18, 0x18};
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 4; i++) at<FnCtor>(ctors[k])(runs[0][k] + i * sizes[k], nullptr);
    }
    for (int i = 0; i < 4; i++) {
        new (runs[1][0] + i * 0x18) AtFlashActionTag;
        new (runs[1][1] + i * 0x58) AtFpPlaceObject2Tag;
        new (runs[1][2] + i * 0x18) AtFlashRemoveTag;
        new (runs[1][3] + i * 0x18) AtFlashSoundTag;
    }
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 4; i++) same_bytes("constructed tag", runs[0][k] + i * sizes[k], runs[1][k] + i * sizes[k], 4, sizes[k]);
    }
    void* cursors[2][4];
    for (int side = 0; side < 2; side++) {
        for (int k = 0; k < 4; k++) cursors[side][k] = runs[side][k];
    }
    const unsigned char* oa = data;
    const unsigned char* ra = data;
    run_side(0, [&] { at<FnFrame>(0x00023860)(o, nullptr, &oa, &cursors[0][0], &cursors[0][1], &cursors[0][2], &cursors[0][3]); });
    run_side(1, [&] { r->ReadBinary(&ra, (AtFlashActionTag**)&cursors[1][0], (AtFpPlaceObject2Tag**)&cursors[1][1], (AtFlashRemoveTag**)&cursors[1][2], (AtFlashSoundTag**)&cursors[1][3]); });
    compare_logs("frame read");
    g_context = "frame bytes, read";
    eq("data left at", oa - data, ra - data);
    for (int k = 0; k < 4; k++) eq("run left at", (char*)cursors[0][k] - (char*)runs[0][k], (char*)cursors[1][k] - (char*)runs[1][k]);
    same_resource(o, r);
    same_bytes("frame", o, r, offsetof(AtFlashFrame, m_flags), offsetof(AtFlashFrame, m_tags));
    for (unsigned i = 0; i < o->m_tagCount && i < r->m_tagCount; i++) {
        long long a = -1, b = -1;
        for (int k = 0; k < 4; k++) {
            if ((unsigned char*)o->m_tags[i] >= runs[0][k] && (unsigned char*)o->m_tags[i] < runs[0][k] + sizeof runs[0][k]) a = k << 16 | (int)((unsigned char*)o->m_tags[i] - runs[0][k]);
            if ((unsigned char*)r->m_tags[i] >= runs[1][k] && (unsigned char*)r->m_tags[i] < runs[1][k] + sizeof runs[1][k]) b = k << 16 | (int)((unsigned char*)r->m_tags[i] - runs[1][k]);
        }
        eq("frame's tag", a, b);
    }
    for (int k = 0; k < 4; k++) {
        for (int i = 0; i < 4; i++) same_tag((AtFlashTag*)(runs[0][k] + i * sizes[k]), (AtFlashTag*)(runs[1][k] + i * sizes[k]), sizes[k], *(uint32_t*)(runs[0][k] + i * sizes[k]));
    }
    // The second frame: no tags, no list.
    run_side(0, [&] { at<FnFrame>(0x00023860)(o, nullptr, &oa, &cursors[0][0], &cursors[0][1], &cursors[0][2], &cursors[0][3]); });
    run_side(1, [&] { r->ReadBinary(&ra, (AtFlashActionTag**)&cursors[1][0], (AtFpPlaceObject2Tag**)&cursors[1][1], (AtFlashRemoveTag**)&cursors[1][2], (AtFlashSoundTag**)&cursors[1][3]); });
    compare_logs("empty frame read");
    same_bytes("empty frame", o, r, offsetof(AtFlashFrame, m_flags), offsetof(AtFlashFrame, m_tags));
    g_context = "frame bytes, tags destroyed";
    // The tags' destructors, the place's freeing its name.
    for (int k = 0; k < 4; k++) {
        static const uint32_t dtors[4] = {0x00023C40, 0x00024230, 0x000246F0, 0x000248E0};
        for (int i = 0; i < 4; i++) {
            run_side(0, [&] { at<Fn0>(dtors[k])(runs[0][k] + i * sizes[k], nullptr); });
            run_side(1, [&] { ((AtFlashTag*)(runs[1][k] + i * sizes[k]))->~AtFlashTag(); });
            same_bytes("tag destroyed", runs[0][k] + i * sizes[k], runs[1][k] + i * sizes[k], 4, sizes[k]);
        }
    }
    compare_logs("tags destroyed");
    run_side(0, [&] { at<Fn0>(0x000234C0)(o, nullptr); at<Fn0>(0x000234C0)(o, nullptr); });
    run_side(1, [&] { r->Clear(); r->Clear(); });
    same_bytes("frame cleared", o, r, 4, sizeof(AtFlashFrame));
    compare_logs("frame cleared");
    g_context = "frame bytes, owned";
    // A frame that owns its tags, as a text frame would: each is deleted.
    {
        void* list[2][3];
        for (int i = 0; i < 3; i++) {
            list[0][i] = std::malloc(0x58);
            at<FnCtor>(i == 1 ? 0x00024210 : 0x00023C10)(list[0][i], nullptr);
            list[1][i] = i == 1 ? (void*)new AtFpPlaceObject2Tag : (void*)new AtFlashActionTag;
        }
        list[0][2] = list[1][2] = nullptr;
        for (int side = 0; side < 2; side++) {
            AtFlashFrame* f = side ? r : o;
            f->m_flags = 1;
            f->m_tagCount = 3;
            f->m_tags = (AtFlashTag**)std::malloc(sizeof(void*) * 3);
            std::memcpy(f->m_tags, list[side], sizeof(void*) * 3);
        }
        run_side(0, [&] { at<Fn0>(0x000234C0)(o, nullptr); });
        run_side(1, [&] { r->Clear(); });
        same_bytes("frame cleared, tags owned", o, r, 4, sizeof(AtFlashFrame));
        compare_logs("frame cleared, tags owned");
    }
    run_side(0, [&] { at<Fn0>(0x00023A60)(o, nullptr); });
    run_side(1, [&] { r->~AtFlashFrame(); });
    same_bytes("frame destroyed", o, r, 4, sizeof(AtFlashFrame));
    compare_logs("frame destroyed");
    std::free(o);
    std::free(r);
    *(void**)(uintptr_t)0x002C6324 = &g_heap_token;
    AtFlashFrame::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C63D0 = &g_heap_token;
    AtFpPlaceObject2Tag::s_heap = &g_heap_token;
}

// ---- written cases --------------------------------------------------------------------------------------------------

static const char* kAfp = "// Atlas Flash Sprite Definition File\nNameSpace \"Test\"\nSprites[2]\n{\nSpriteDef { 0 FrameRefs[3] { 57,\"Init\" 0,\"Update\" 58; } }\nSpriteDef { 1 FrameRefs[1] { 4; } }\n}\n";

static void serve(const std::string& path, const std::string& data) { g_files[upper(path)] = data; }

static void run_written() {
    // Sprites' frame lists, each form and error.
    const char* sprites[] = {
        "SpriteDef { 7 FrameRefs[4] { 57,\"Init\" 0,\"Update\" 58; 59,\"Done\" } }",
        "SpriteDef { 7 FrameRefs[6] { 3:5 9*2 1; } }",
        "SpriteDef { 7 FrameRefs[6] { 1;  3:3 9*1 2*0 } }",
        "SpriteDef { 7 FrameRefs[3] { 1:6 } }",              // a run past the end
        "SpriteDef { 7 FrameRefs[2] { 1*5 } }",              // a repeat past the end
        // Not run: a run backwards, "6:2", counts to nearly 2^32 and writes through the heap until it faults, on both sides, taking the harness's own memory with it.
        "SpriteDef { 7 FrameRefs[2] { 6:6 1; } }",           // a run of one
        "SpriteDef { 7 FrameRefs[2] { 1,\"\" 2; } }",         // an empty name
        "SpriteDef { 7 FrameRefs[2] { 1! 2; } }",            // a mark that is not one of the four
        "SpriteDef { 7 FrameRefs[2] { 1( 2; } }",
        "SpriteDef { 7 FrameRefs[2] { 1{ 2; } }",
        "SpriteDef { 7 FrameRefs[0] { } }",
        "Sprites { 7 FrameRefs[1] { 1; } }",                 // the wrong words
        "SpriteDef { 7 SpriteDef[1] { 1; } }",
    };
    for (const char* t : sprites) {
        run_sprite_text(t, &g_heap_a, &g_heap_b);
        run_sprite_text(t, nullptr, &g_heap_b);
    }
    run_sprite_text(sprites[0], nullptr, nullptr);
    *(void**)(uintptr_t)0x002C626C = &g_heap_token;
    AtFpSprite::s_heap = &g_heap_token;
    // Shapes.
    for (const char* t : {"Shape 3 { Rectangle -320.0 -240.0 320.0 240.5 MeshAssignment 4 \"00000004\" }", "Shape 3 { Rectangle 1.5 2.5 3.5 4.5 MeshAssignment -2 \"\" }",
                          "Shape 3 { Rectangle 1.5 2.5 3.5 4.5 MeshAssignment 4 5 }", "Rectangle 3 { Shape 1.5 2.5 3.5 4.5 Shape 4 \"x\" }"}) {
        run_shape_text(t);
    }
    // A frame from bytes.
    for (void* heap : {(void*)&g_heap_a, (void*)nullptr}) {
        for (void* fallback : {(void*)&g_heap_b, (void*)nullptr}) run_frame_bytes(heap, fallback);
    }

    // Through the files: an .AFP, .XFF and .AFF of this file's.
    Xff x;
    x.frame(0, 0, 0, 0, 0);
    x.frame(1, 2, 1, 0, 0);
    x.frame(2, 1, 1, 2, 1);
    x.frame(3, 4, 0, 1, 2);
    serve("Sprites\\TEST.AFP", kAfp);
    serve("Sprites\\TEST.XFF", x.file());
    serve("Sprites\\TEST.AFF", "// Atlas Flash Frame Definition File\nNameSpace \"Test\"\nFrames[0]\n{\n}\n");
    g_context = "written sprites, xff";
    run_sprites("TEST", 2, true);
    g_context = "written sprites, aff";
    run_sprites("TEST", 0, false);
    g_context = "written sprites, aff, mode 1";
    run_sprites("TEST", 1, false);
    // The .XFF's data elsewhere in the file, and no frames at all.
    serve("Sprites\\TEST.XFF", x.file(0x30));
    g_context = "xff, data later";
    run_sprites("TEST", 2, false);
    serve("Sprites\\TEST.XFF", Xff().file());
    g_context = "xff, nothing";
    run_sprites("TEST", 2, false);
    serve("Sprites\\TEST.XFF", x.file());
    // Reads left pending, and read errors.
    // Not compared: a header read that fails or comes up short (kFailAfterFirst, kOneByte) leaves the counts' stack bytes as they were, which differ between the sides. A failed wait still delivers the bytes here.
    for (Mode m : {kPendingIo, kWaitFails}) {
        g_context = "xff, file mode " + std::to_string((int)m);
        g_mode = m;
        run_sprites("TEST", 2, false);
        g_mode = kNormal;
    }
    // The frames' heap: the table's own, the default, none.
    for (void* fallback : {(void*)&g_heap_b, (void*)nullptr}) {
        set_heaps(fallback);
        g_context = "xff, default heap";
        run_sprites("TEST", 2, false);
    }
    set_heaps(&g_heap_token);
    {
        g_context = "xff, own heap";
        AtFlashFrameTable* o = make<AtFlashFrameTable>(0x00017550, 0xCD, true);
        AtFlashFrameTable* r = make<AtFlashFrameTable>(0x00017550, 0xCD, false);
        o->m_heap = r->m_heap = &g_heap_a;
        run_side(0, [&] { at<Fn2>(0x00017CC0)(o, nullptr, (uintptr_t)"TEST", 2); at<Fn2>(0x00017CC0)(o, nullptr, (uintptr_t)"TEST", 2); });
        run_side(1, [&] { r->Load("TEST", 2); r->Load("TEST", 2); });
        same_frames(o, r, false);
        compare_logs("frames loaded twice");
        o->m_heap = r->m_heap = nullptr;
        set_heaps(nullptr);
        *(void**)(uintptr_t)0x002C60B0 = nullptr;
        AtIoTokenFile::s_heap = nullptr;
        const bool oc = run_side(0, [&] { at<Fn2>(0x00017CC0)(o, nullptr, (uintptr_t)"TEST", 2); });
        const bool rc = run_side(1, [&] { r->Load("TEST", 2); });
        eq("crash", oc, rc);
        same_frames(o, r, false);
        compare_logs("frames, no heaps");
        set_heaps(&g_heap_token);
        *(void**)(uintptr_t)0x002C60B0 = &g_heap_token;
        AtIoTokenFile::s_heap = &g_heap_token;
        if (!oc && !rc) {
            // Allocating an .AFF's frames, as its reader would.
            run_side(0, [&] { at<Fn0>(0x00017580)(o, nullptr); at<Fn1>(0x00017B80)(o, nullptr, 3); });
            run_side(1, [&] { r->ClearFrames(); r->Allocate(3); });
            o->m_loaded = r->m_loaded = 1;
            same_frames(o, r, false);
            compare_logs("frames allocated");
            run_side(0, [&] { eq("returns this", (uintptr_t)at<FnDel>(0x00017E40)(o, nullptr, 1), (uintptr_t)o); });
            run_side(1, [&] { delete r; });
            compare_logs("frames destroyed");
        }
    }
    // An .AFF with frames: its text reader is not recovered, so only the original's would run. Not compared.
    // No room after the file closes: every request fails, each reported.
    g_fail_after_close = true;
    for (const char* t : {"xff, no room"}) {
        g_context = t;
        run_sprites("TEST", 2, false);
        g_fail_alloc = false;
    }
    serve("Sprites\\TEST.XFF", Xff().file());
    g_context = "xff, nothing, no room";
    run_sprites("TEST", 2, false);
    g_fail_after_close = false;
    g_fail_alloc = false;
    serve("Sprites\\TEST.XFF", x.file());
    // Files missing: the reports, then each reader on nothing.
    for (const char* missing : {"Sprites\\TEST.XFF", "Sprites\\TEST.AFP"}) {
        const std::string keep = g_files[upper(missing)];
        g_files.erase(upper(missing));
        g_context = std::string("missing ") + missing;
        run_sprites("TEST", 2, false);
        g_files[upper(missing)] = keep;
    }
    g_context = "missing aff";
    g_files.erase(upper("Sprites\\TEST.AFF"));
    run_sprites("TEST", 0, false);
    // The .AFP's own words.
    for (const char* t : {"NameSpace \"T\" Sprites[0] { }", "NameSpace \"T\" SpriteDef[0] { }", "NameSpace \"T\" Sprites[1] { SpriteDef { 2 FrameRefs[1] { 5; } } }"}) {
        serve("Sprites\\TEST.AFP", t);
        g_context = std::string("afp [") + t + "]";
        run_sprites("TEST", 2, false);
    }
    serve("Sprites\\TEST.AFP", kAfp);

    // Shapes through the file: with and without shapes, each count of texture databases.
    serve("Shapes\\TEST.AFH", "// Atlas Flash Shape Definition File\nNameSpace \"Test\"\nGraphicsDatabaseLoader \"TestDb\"\nShape[ 2 ]\n{\nShape 0 { Rectangle -1.0 -2.0 3.0 4.0 MeshAssignment 0 \"00000000\" }\nShape 1 { Rectangle 5.0 6.0 7.0 8.5 MeshAssignment 1 \"00000001\" }\n}\n");
    for (unsigned dbs : {1u, 0u, 2u}) {
        for (unsigned used : {3u, 0u}) {
            g_texture_dbs = dbs;
            g_used_textures = used;
            for (int mode : {2, 0}) {
                g_context = "shapes, " + std::to_string(dbs) + " databases, " + std::to_string(used) + " used, mode " + std::to_string(mode);
                run_shapes("TEST", mode, false);
            }
        }
    }
    g_texture_dbs = 1;
    g_used_textures = 3;
    for (void* own : {(void*)nullptr}) {
        (void)own;
        set_heaps(nullptr);
        g_context = "shapes, no heap";
        run_shapes("TEST", 2, false);
        set_heaps(&g_heap_token);
    }
    g_fail_after_close = true;
    g_context = "shapes, no room";
    run_shapes("TEST", 2, false);
    g_fail_after_close = false;
    g_fail_alloc = false;
    serve("Shapes\\TEST2.AFH", "NameSpace \"Test\" GraphicsDatabaseLoader \"None\" Shape[ 0 ] { }");
    g_context = "shapes, none";
    run_shapes("TEST2", 2, false);
    for (const char* t : {"NameSpace \"T\" Shape \"x\" Shape[1] { }", "GraphicsDatabaseLoader \"T\"  NameSpace \"x\" Shape[0] { }", "NameSpace \"T\" GraphicsDatabaseLoader \"x\" Rectangle[0] { }"}) {
        serve("Shapes\\TEST3.AFH", t);
        g_context = std::string("afh [") + t + "]";
        run_shapes("TEST3", 2, false);
    }
    g_context = "shapes, missing";
    run_shapes("TEST4", 2, false);

    // The .AFD: each word; Text with fonts of its own and not; a bad word.
    serve("TEST.AFL", "Level { HeapSize 0 MaxVariables 3 FontSourceLevel All }");
    serve("TEST2.AFL", "Level { FontSourceLevel Self }");
    serve("TEST.AFD", "Loader\n{\n\tLevel\t\"Test\"\n\tSprite\t\"Test\"\n\tShape\t\"Test\"\n\tText\t\"Text1\"\n\tScript\t\"Script1\"\n\tSound\t\"Noise\"\n}\n");
    serve("TEST2.AFD", "Loader { Level \"Test2\" Text \"T\" Script \"S\" Sprite \"Test\" }");
    serve("TEST3.AFD", "Loader { Text \"Before\" Level \"Test\" Text \"After\" }");
    serve("TEST4.AFD", "Loader { Sprites \"Test\" Sound 5 Level \"Test2\" }");
    serve("TEST5.AFD", "Loader { Shape \"Test2\" } }");
    for (const char* name : {"TEST", "TEST2", "TEST3", "TEST4", "TEST5"}) {
        for (int platform : {1, 0}) {
            if (platform == 0 && std::strcmp(name, "TEST") != 0) continue;
            g_context = std::string("level ") + name + (platform ? "" : ", aff");
            if (platform == 0) serve("Sprites\\TEST.AFF", "NameSpace \"Test\" Frames[0] { }");
            run_level(name, platform, 0xCD, false);
        }
    }
    run_level("TEST", 7, 0x00, false);
    g_context = "level missing";
    run_level("NONE", 1, 0xCD, false);
}

// ---- more written cases: each request failing in turn, the file on its own, a sprite allocated on its own -------------

typedef int(__fastcall* FnOpenExt)(void*, void*, const char*, const char*, int);
typedef int(__fastcall* FnRelease)(void*, void*, void*);

static void run_more() {
    // The file: an extension added or not, by the leaf; Release's answer with and without a file.
    serve("A\\B.XFF", "x");
    serve("A\\B.C", "x");
    serve("A.B\\C", "x");
    serve("C.XFF", "x");
    for (const char* path : {"A\\B", "A\\B.C", "A.B\\C", "C"}) {
        g_context = std::string("file ") + path;
        AtAudioBankFile o, r;
        int os = 0, rs = 0;
        run_side(0, [&] { os = at<FnOpenExt>(0x00024AA0)(&o, nullptr, path, "XFF", 2); });
        run_side(1, [&] { rs = r.OpenWithExtension(path, "XFF", 2); });
        eq("opened", os, rs);
        compare_logs("file opened");
        int oa = 0, ra = 0;
        run_side(0, [&] { oa = at<FnRelease>(0x00024980)(&o, nullptr, nullptr); });
        run_side(1, [&] { ra = r.Release(nullptr); });
        eq("released", oa, ra);
        compare_logs("file released");
        run_side(0, [&] { at<Fn0>(0x000249A0)(&o, nullptr); });
        run_side(1, [&] { r.Close(); });
        compare_logs("file closed");
        run_side(0, [&] { oa = at<FnRelease>(0x00024980)(&o, nullptr, nullptr); });
        run_side(1, [&] { ra = r.Release(nullptr); });
        eq("released with no file", oa, ra);
    }
    // A sprite allocated on its own, then with no room.
    for (int fail : {0, 1}) {
        g_context = "sprite allocated";
        g_fill = 0x33;
        AtFpSprite* o = make<AtFpSprite>(0x00016AE0, 0x33, true);
        AtFpSprite* r = make<AtFpSprite>(0x00016AE0, 0x33, false);
        g_fail_at_push = fail;
        const bool oc = run_side(0, [&] { at<Fn2>(0x00016BB0)(o, nullptr, 9, 3); });
        const bool rc = run_side(1, [&] { r->Allocate(9, 3); });
        g_fail_at_push = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        same_sprite(o, r);
        compare_logs("sprite allocated");
    }
    // Each request failing in turn: the sprites (.AFP, .XFF), the shapes, a sprite's names.
    for (int n = 1; n <= 60; n++) {
        g_fail_at_push = n;
        g_context = "sprites, push " + std::to_string(n) + " fails";
        run_sprites("TEST", 2, false);
        g_context = "shapes, push " + std::to_string(n) + " fails";
        if (n <= 20) run_shapes("TEST", 2, false);
        g_context = "sprite names, push " + std::to_string(n) + " fails";
        if (n <= 4) run_sprite_text("SpriteDef { 7 FrameRefs[3] { 57,\"Init\" 0,\"Update\" 58; } }", &g_heap_a, &g_heap_b);
        g_fail_at_push = 0;
        g_fail_alloc = false;
    }
    // Loaded twice; destroyed loaded.
    g_context = "sprites twice";
    run_sprites("TEST", 2, false, true);
    g_context = "sprites destroyed loaded";
    run_sprites("TEST", 2, false, false, true);
    g_context = "shapes twice";
    run_shapes("TEST", 2, false, true);
    g_context = "shapes destroyed loaded";
    run_shapes("TEST", 2, false, false, true);
    // A sprite with no brace after its word.
    run_sprite_text("SpriteDef 7 FrameRefs[1] { 1; } }", &g_heap_a, &g_heap_b);
    // An .AFD named longer than eight, and one whose Loader has no brace.
    serve("TESTLONG.AFD", "Loader { Level \"Test2\" }");
    g_context = "level, long name";
    run_level("TESTLONGER", 1, 0xCD, false);
    serve("TEST6.AFD", "Loader Level \"Test2\" }");
    g_context = "level, no brace";
    run_level("TEST6", 1, 0xCD, false);
}

// ---- real archives --------------------------------------------------------------------------------------------------

static unsigned g_archives, g_xff, g_afp, g_afh, g_afd;

static bool load_archive(const std::string& list, const std::string& bin, std::string* afd) {
    FILE* f = nullptr;
    if (fopen_s(&f, bin.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    std::string data((size_t)std::ftell(f), '\0');
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    if (fopen_s(&f, list.c_str(), "r") || !f) return false;
    char line[1024];
    g_files.clear();
    bool all = true;
    while (std::fgets(line, sizeof line, f)) {
        char kind[8], path[512];
        unsigned offset, size;
        if (std::sscanf(line, "%7s\t%511[^\t]\t%u\t%u", kind, path, &offset, &size) != 4 || std::strcmp(kind, "F") != 0) continue;
        // Each file follows 32 bytes of its own in the archive.
        if ((size_t)offset + 32 + size > data.size()) {
            all = false;
            continue;
        }
        const std::string p = upper(path);
        g_files[p] = data.substr(offset + 32, size);
        if (p.size() > 4 && p.compare(p.size() - 4, 4, ".AFD") == 0 && p.find('\\') == std::string::npos) *afd = p.substr(0, p.size() - 4);
    }
    std::fclose(f);
    if (!all && !afd->empty()) std::printf("note: %s: some files lie past the end of the cases' copy\n", list.c_str());
    return true;
}

static void run_archive(const std::string& afd) {
    // The names the .AFD gives, which are the archive's: one name for all of a level's files.
    const std::string& text = g_files[afd + ".AFD"];
    std::string name;
    const size_t q = text.find("Sprite");
    if (q != std::string::npos) {
        const size_t a = text.find('"', q), b = text.find('"', a + 1);
        name = text.substr(a + 1, b - a - 1);
    }
    if (g_files.count(upper("SPRITES\\" + name + ".XFF"))) g_xff++;
    if (g_files.count(upper("SPRITES\\" + name + ".AFP"))) g_afp++;
    if (g_files.count(upper("SHAPES\\" + name + ".AFH"))) g_afh++;
    g_context = afd + " sprites";
    run_sprites(name.c_str(), 2, true);
    g_context = afd + " shapes";
    run_shapes(name.c_str(), 2, true);
    g_context = afd + " level";
    run_level(afd.c_str(), 1, 0xCD, true);
    g_afd++;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
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
    if (pos.size() != 2) { std::printf("usage: oracle_afd <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)push_counting);
    redirect((uint32_t)(uintptr_t)&AtPushHeap, (const void*)push_counting);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    // What stands in, on both sides.
    redirect(0x0001ADC0, (const void*)loader_load);
    redirect(addr_of(&AtDatabaseLoader::Load), (const void*)loader_load);
    redirect(0x0001C6F0, (const void*)count_used);
    redirect(addr_of(&AtTextureDatabase::CountUsedTextures), (const void*)count_used);
    redirect(0x000134A0, (const void*)script_load);
    redirect(addr_of(&AtFlashScriptTable::Load), (const void*)script_load);
    redirect(0x00013CB0, (const void*)text_load);
    redirect(addr_of(&AtFlashTextTable::Load), (const void*)text_load);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new JamFs;
    g_fs->m_path.m_top = 0;
    g_fs->m_path.m_stack[0][0] = 0;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    *(AtIoContext**)(uintptr_t)0x002C6328 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    AtAudioBankFile::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6270u, 0x002C626Cu, 0x002C6324u, 0x002C63D0u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    AtFlashResourceTable::s_defaultHeap = &g_heap_token;
    AtFpSprite::s_heap = &g_heap_token;
    AtFlashFrame::s_heap = &g_heap_token;
    AtFpPlaceObject2Tag::s_heap = &g_heap_token;
    g_owner = new FakeOwner;
    g_loaders[0] = new FakeLoader;
    g_loaders[1] = new FakeLoader;
    g_graphics.m_owner = g_owner;
    g_app.m_graphics = &g_graphics;
    g_env.m_application = &g_app;
    *(void**)(uintptr_t)0x002C60A8 = &g_heap_b;
    AtHeapStack::s_contentHeap = (AtHeap*)&g_heap_b;

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_groups();
    run_written();
    run_more();
    const unsigned written_frames = g_frames;

    // The real archives: every one beside the text cases that has an .AFD.
    std::string dir = pos[1];
    const std::string jams = dir + "\\..\\oracle-jam\\";
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((jams + "*.txt").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) { std::printf("cannot list %s\n", jams.c_str()); return 2; }
    std::vector<std::string> lists;
    do lists.push_back(found.cFileName); while (FindNextFileA(h, &found));
    FindClose(h);
    std::sort(lists.begin(), lists.end());
    g_frames = g_tags = g_sprites = g_shapes = 0;
    for (const std::string& l : lists) {
        std::string afd;
        const std::string stem = l.substr(0, l.size() - 4);
        if (!load_archive(jams + l, jams + stem + ".bin", &afd) || afd.empty()) continue;
        g_archives++;
        run_archive(afd);
    }
    (void)written_frames;
    report_entries();
    std::printf("%s: %u archives, %u .AFD, %u .AFP, %u .XFF, %u .AFH; %u sprites, %u frames, %u tags, %u shapes, %u levels; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_archives, g_afd, g_afp,
                g_xff, g_afh, g_sprites, g_frames, g_tags, g_shapes, g_levels, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
