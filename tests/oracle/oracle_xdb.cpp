// Differential oracle for the Xbox texture and material databases and what they hold (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-08-xdb.md.
//
//   oracle_xdb <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// What it does not show, first. What a texture or a material does with the device once read (the original's 0x000FCAD0 and 0x0010B7F0) is this harness's on both sides: it logs, and marks the object as loaded
// in a way the release paths can see (a handle for the texture and each level's surface, now and then a chained surface with blocks and a palette; stage states with entries, blocks and the shared tables). The
// device's own calls (0x000FB630, 0x00205E20, 0x00205D10) are the harness's and log. 3D textures, cube maps and 3D surfaces are the seam classes on both sides. So real files show that the databases, the 2D
// textures, surfaces and palettes, the materials and their stage states make, read, hand out, clear and destroy the same, in the same order, with the same allocations, as far as those doubles; nothing of the
// pixels, and nothing drawn.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "at/AtGrD3DDatabase.h"
#include "at/AtGrD3DTextureLoad.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 150).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 150).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnDescribe)(void*, void*, void*, const char*, unsigned, int, unsigned, int);

static void slot(char c, uint32_t v = 0) { if (g_recording) g_log[g_side].add(c, v); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token, g_material_heap_token;

// ---- owners ----------------------------------------------------------------------------------------------------------------

struct TestOwner : public AtGrD3DOwner {};
static TestOwner* g_own[2];

static void* value_of(const std::string& name, uint32_t salt) {
    uint32_t h = 0x811C9DC5 ^ salt;
    for (const char c : name) h = (h ^ (unsigned char)c) * 16777619;
    return (void*)(uintptr_t)(h | 0x1000);
}

// Owners that know the names given, as textures and shaders, plainly and in each space; or nothing.
struct Env {
    std::vector<GmNameMap*> maps[2];
    Env(const std::set<std::string>& names, const std::set<std::string>& spaces) {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            void* p = std::calloc(1, sizeof(TestOwner));
            g_own[side] = new (p) TestOwner;
            TestOwner& w = *g_own[side];
            const unsigned pool = (unsigned)spaces.size() + 4;
            w.m_textureSpaces.Init(pool);
            w.m_materialSpaces.Init(pool);
            w.m_shaderSpaces.Init(pool);
            const auto fill = [&](uint32_t salt) {
                GmNameMap* m = new GmNameMap;
                m->Init((unsigned)names.size() + 1);
                for (const std::string& n : names) m->Insert(n.c_str(), value_of(n, salt));
                maps[side].push_back(m);
                return m;
            };
            w.m_textures.Add(fill(0));
            w.m_shaders.Add(fill(1));
            for (const std::string& s : spaces) {
                w.m_textureSpaces.Add(s.c_str(), fill(2));
                w.m_shaderSpaces.Add(s.c_str(), fill(3));
            }
        }
        g_recording = was;
    }
    ~Env() {
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            for (GmNameMap* m : maps[side]) delete m;
            g_own[side]->~TestOwner();
            std::free(g_own[side]);
            g_own[side] = nullptr;
        }
    }
};

static void gather(const std::vector<unsigned char>& d, std::set<std::string>& names, std::set<std::string>& spaces) {
    size_t i = 0;
    bool last_was_string = false;
    while (i < d.size()) {
        const unsigned char c = d[i];
        if (c == '\n') { last_was_string = false; i++; continue; }
        if (c == '/' && i + 1 < d.size() && d[i + 1] == '/') { while (i < d.size() && d[i] != '\n') i++; continue; }
        if (c == '"') {
            size_t e = i + 1;
            while (e < d.size() && d[e] != '"' && d[e] != '\n') e++;
            std::string s((const char*)d.data() + i + 1, e - i - 1);
            if (s.size() > 8) s.resize(8);
            const bool after_word = i >= 11 && !std::memcmp(d.data() + i - 10, "NameSpace", 9);
            if (!s.empty() && s[0] != 'X') {
                names.insert(s);
                if (after_word || last_was_string) spaces.insert(s);
            }
            last_was_string = true;
            i = e + 1;
            continue;
        }
        if (c != ' ' && c != '\t' && c != '\r') last_was_string = false;
        i++;
    }
}

// ---- the device's doubles, the same on both sides ----------------------------------------------------------------------------

static uint32_t g_next_handle[2];
static unsigned g_loads[2];
static void* handle() { return (void*)(uintptr_t)(g_next_handle[g_side]++); }

static void* side_new(size_t n) { return g_side == 0 ? o_new((unsigned)n) : operator new(n); }

// A 2D surface made by the side's own constructor, in the side's own heap.
static AtGrD3DSurface2D* new_surface() {
    if (g_side == 0) {
        void* p = o_new(sizeof(AtGrD3DSurface2D));
        at<Fn0>(0x000FCEE0)(p, nullptr);
        return (AtGrD3DSurface2D*)p;
    }
    return new AtGrD3DSurface2D;
}

static int g_attach_every = 3;  // every so many loads, a chained surface with blocks, and a palette, on the first level

// As the original's slot 12 leaves a texture, as far as its release can see.
static void load_texture(AtGrD3DTexture2D* t, AtMaterialOwner* owner) {
    slot('L', *(const uint32_t*)t->m_name);
    slot('l', owner == g_own[g_side]);
    slot('f', t->m_flags);
    slot('v', t->m_levels);
    t->m_owner = static_cast<AtGrD3DOwner*>(owner);
    if (t->m_flags & (AtTextureBase::kFrontBufferSource | AtTextureBase::kBackBufferSource)) t->m_levels = t->m_levelsAsked = 1;
    t->m_texture = handle();
    AtGrD3DTextureDatabase* db = static_cast<AtGrD3DTextureDatabase*>(t->m_database);
    if (!db || !t->m_surfaces || !db->m_surfaces2D) return;
    if (t->m_surfaces < db->m_surfaces2D || t->m_surfaces + t->m_levels > db->m_surfaces2D + db->m_surface2DCount) return;  // handed out past the end: left alone
    for (unsigned i = 0; i < t->m_levels; i++) t->m_surfaces[i].m_surface = handle();
    if (++g_loads[g_side] % g_attach_every == 0 && t->m_levels > 0) {
        AtGrD3DSurface2D* s = &t->m_surfaces[0];
        AtGrD3DSurface2D* next = new_surface();
        next->m_surface = handle();
        next->m_unknown44 = side_new(8);
        s->m_next = next;
        s->m_unknown48 = side_new(12);
        s->m_palette.m_entries = (unsigned int*)side_new(16);
        s->m_palette.m_resource = handle();
        s->m_palette.m_count = 4;
        s->m_format.m_flags = AtPixelFormat::kIndex;
        // Fields nothing recovered sets, so that what clears them shows.
        s->m_flags = 0x7;
        s->m_unknown20 = 0x20;
        s->m_unknown22 = 0x22;
        s->m_unknown24 = (AtGrD3DOwner*)0x24;
        s->m_unknown3C = (AtGrD3DTexture2D*)0x3C;
        s->m_unknown40 = (AtGrD3DSurface2D*)0x40;
        s->m_palette.m_unknown06 = 6;
    }
}

// The shared tables, by side.
static unsigned& shared_users() { return g_side == 0 ? *(unsigned*)(uintptr_t)0x003B1F74 : AtGrD3DMaterialStage::s_sharedUsers; }
static AtGrD3DStageEntry*& shared_entries() { return g_side == 0 ? *(AtGrD3DStageEntry**)(uintptr_t)0x003B1F78 : AtGrD3DMaterialStage::s_sharedEntries; }
static unsigned& shared_word(int k) {  // 0: 7C, 1: 80, 2: 8C
    static const uint32_t va[3] = {0x003B1F7C, 0x003B1F80, 0x003B1F8C};
    unsigned* r[3] = {&AtGrD3DMaterialStage::s_shared7C, &AtGrD3DMaterialStage::s_shared80, &AtGrD3DMaterialStage::s_shared8C};
    return g_side == 0 ? *(unsigned*)(uintptr_t)va[k] : *r[k];
}
static void*& shared_block(int k) {  // 0: 84, 1: 88
    return g_side == 0 ? *(void**)(uintptr_t)(k ? 0x003B1F88 : 0x003B1F84) : (k ? AtGrD3DMaterialStage::s_shared88 : AtGrD3DMaterialStage::s_shared84);
}

static AtGrD3DStageEntry* new_entries(unsigned n) {
    if (g_side == 0) {
        unsigned* block = (unsigned*)o_new(4 + n * sizeof(AtGrD3DStageEntry));
        block[0] = n;
        return (AtGrD3DStageEntry*)(block + 1);
    }
    return new AtGrD3DStageEntry[n];
}

// As the original's slot 0 might leave a material: none, one or two stage states built, the second holding the shared tables.
static void load_material(AtGrD3DMaterial* m, AtMaterialOwner* owner) {
    slot('M', *(const uint32_t*)&m->m_diffuse);
    slot('m', owner == g_own[g_side]);
    slot('k', m->m_flags);
    const unsigned built = g_loads[g_side]++ % 3;
    m->m_stageStateCount = (unsigned short)built;
    if (built >= 1) {
        m->m_stageStates[0].m_entries = new_entries(1 + built);
        m->m_stageStates[0].m_entries[0].m_unknown00 = 0x55;
        m->m_stageStates[0].m_unknown04 = side_new(8);
        m->m_stageStates[0].m_unknown08 = built == 2 ? side_new(4) : nullptr;
        m->m_stageStates[0].m_unknown10 = 7;
        m->m_stageStates[0].m_unknown0C = 0x0C;
        m->m_stageStates[0].m_unknown16 = 0x16;
        m->m_stageStates[0].m_unknown18 = 0x18;
        m->m_stageStates[0].m_unknown1A = 0x1A;
        m->m_stageStates[0].m_unknown20 = 0x20;
        m->m_stageStates[0].m_unknown26 = 0x26;
    }
    if (built >= 2) {
        m->m_stageStates[1].m_flags = AtGrD3DMaterialStage::kShared | 0x10;
        m->m_stageStates[1].m_unknown1C = 3;
        if (shared_users()++ == 0) {
            shared_entries() = new_entries(2);
            shared_block(0) = side_new(8);
            shared_block(1) = side_new(12);
            shared_word(0) = 1;
            shared_word(1) = 2;
            shared_word(2) = 3;
        }
    }
}

static void __fastcall o_load_texture(AtGrD3DTexture2D* t, void*, AtMaterialOwner* owner) { load_texture(t, owner); }
static void __fastcall o_load_material(AtGrD3DMaterial* m, void*, AtMaterialOwner* owner) { load_material(m, owner); }
static void release_stage(void* stages, unsigned stage) {
    slot('G', stage);
    slot('g', g_own[g_side] && stages == (void*)g_own[g_side]->m_stageCache);
}
static void __fastcall o_release_stage(void* stages, void*, unsigned stage) { release_stage(stages, stage); }
static void __stdcall o_before_release(void* r) { slot('B', (uint32_t)(uintptr_t)r); }
static void __stdcall o_release(void* r) { slot('X', (uint32_t)(uintptr_t)r); }

void AtGrD3DLoadTexture2D(AtGrD3DTexture2D* t, AtMaterialOwner* owner) { load_texture(t, owner); }
// A texture's slot 12 is recovered (src/at/AtGrD3DTextureLoad.cpp, tested by tests/oracle/oracle_texload.cpp). Here it stays the double above on both sides: on the recovered side by a jump written at the
// start of the compiled slot 12 (the harness's code is writable), as on the original's at 0x000FCAD0. So the device's calls it makes are not expected.
void* AtGrD3DGetBackBuffer(int) { slot('?', 3); return nullptr; }
void* AtGrD3DCreateTexture(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned) { slot('?', 4); return nullptr; }
void* AtGrD3DSurfaceLevel(void*, unsigned) { slot('?', 5); return nullptr; }
void AtGrD3DDescribe(void*, AtGrD3DSurfaceDesc*) { slot('?', 6); }
void AtGrD3DLockRect(void*, AtGrD3DLockedRect*, const void*, unsigned) { slot('?', 7); }
static void redirect_recovered_slot12(const void* to) {
    AtGrD3DTexture2D probe;
    redirect((uint32_t)(uintptr_t)(*(void***)&probe)[12], to);
}
void AtGrD3DLoadMaterial(AtGrD3DMaterial* m, AtMaterialOwner* owner) { load_material(m, owner); }
void AtGrD3DReleaseStage(void* stages, unsigned int stage) { release_stage(stages, stage); }
void AtGrD3DBeforeRelease(void* r) { o_before_release(r); }
void AtGrD3DRelease(void* r) { o_release(r); }

// The seam of src/at/AtGrD3DSeam.cpp, which this harness reaches only through the classes it does not recover: it logs.
unsigned int AtGrD3DUnrecovered(unsigned int va, void*, unsigned int a, unsigned int) {
    slot('u', va);
    slot('a', a);
    return 0;
}
void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int) { slot('?', size); return nullptr; }
void AtGrD3DFreeContiguous(void*) { slot('?'); }
void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char**, char**, char**) { slot('?'); }
void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char**, char**) { slot('?'); }
void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char**, char**) { slot('?'); }
void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char**, char**, char**, char*) { slot('?'); }

// The seam classes on the original's side: as the loader's oracle makes them. An object of a seam class gets a copy of its table, with the slot the original calls to destroy an array replaced, so that the
// original frees through its own delete, which logs.
template <class T> struct Side0 {
    static void* s_table[64];
    static bool s_ready;
    static void* __fastcall VecDel(T* self, void*, unsigned flags) {
        if (flags & 2) {
            unsigned* block = (unsigned*)self - 1;
            for (unsigned i = *block; i > 0; i--) self[i - 1].T::~T();
            if (flags & 1) o_delete(block);
            return block;
        }
        self->T::~T();
        if (flags & 1) o_delete(self);
        return self;
    }
    static T* __fastcall Construct(T* self, void*) {
        new (self) T;
        if (!s_ready) {
            std::memcpy(s_table, *(void***)self, sizeof s_table);
            s_table[0] = (void*)&VecDel;
            s_ready = true;
        }
        *(void***)self = s_table;
        return self;
    }
};
template <class T> void* Side0<T>::s_table[64];
template <class T> bool Side0<T>::s_ready;

static void __cdecl pure_call() { slot('!'); *(volatile int*)nullptr = 0; }

// ---- comparing ---------------------------------------------------------------------------------------------------------------

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

template <class T> static long long index_of(const T* base, const void* p, unsigned count) {
    if (!p) return -1;
    if (!base) return -2;
    const char* a = (const char*)base;
    const char* q = (const char*)p;
    if (q < a || (q - a) % sizeof(T)) return -3;
    const long long i = (q - a) / (long long)sizeof(T);
    return i <= (long long)count ? i : -4;
}

// Which item a texture database's map holds: kind and index.
static long long texture_id(const AtGrD3DTextureDatabase* d, const void* p) {
    long long i;
    if ((i = index_of(d->m_textures2D, p, d->m_texture2DCount)) >= 0) return i;
    if ((i = index_of(d->m_textures3D, p, d->m_texture3DCount)) >= 0) return 0x10000 + i;
    if ((i = index_of(d->m_cubeMaps, p, d->m_cubeMapCount)) >= 0) return 0x20000 + i;
    return -1;
}

static void same_texmap(const AtGrD3DTextureDatabase* o, const AtGrD3DTextureDatabase* r) {
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's item", texture_id(o, x.m_value), texture_id(r, y.m_value));
            }
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
}

static void same_map(const GmNameMap* o, const GmNameMap* r, const void* obase, const void* rbase) {
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's value", (const char*)x.m_value - (const char*)obase, (const char*)y.m_value - (const char*)rbase);
            }
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
}

static void same_palette(const AtGrD3DPalette& a, const AtGrD3DPalette& b) {
    eq("palette's count", a.m_count, b.m_count);
    eq("palette's +6", a.m_unknown06, b.m_unknown06);
    eq("palette's resource", (uintptr_t)a.m_resource, (uintptr_t)b.m_resource);
    eq("palette's entries", a.m_entries != nullptr, b.m_entries != nullptr);
}

static void same_surface(const AtGrD3DSurface2D& a, const AtGrD3DSurface2D& b, int depth = 0) {
    eq("surface's format", std::memcmp(&a.m_format, &b.m_format, sizeof a.m_format), 0);
    eq("surface's key", *(const uint32_t*)a.m_colorKey, *(const uint32_t*)b.m_colorKey);
    eq("surface's +1C", a.m_unknown1C, b.m_unknown1C);
    eq("surface's flags", a.m_flags, b.m_flags);
    eq("surface's +20", a.m_unknown20, b.m_unknown20);
    eq("surface's +22", a.m_unknown22, b.m_unknown22);
    eq("surface's +24", (uintptr_t)a.m_unknown24, (uintptr_t)b.m_unknown24);
    eq("surface's surface", (uintptr_t)a.m_surface, (uintptr_t)b.m_surface);
    same_palette(a.m_palette, b.m_palette);
    eq("surface's +3C", (uintptr_t)a.m_unknown3C, (uintptr_t)b.m_unknown3C);
    eq("surface's +40", (uintptr_t)a.m_unknown40, (uintptr_t)b.m_unknown40);
    eq("surface's +44", a.m_unknown44 != nullptr, b.m_unknown44 != nullptr);
    eq("surface's +48", a.m_unknown48 != nullptr, b.m_unknown48 != nullptr);
    eq("surface's next", a.m_next != nullptr, b.m_next != nullptr);
    if (a.m_next && b.m_next && depth < 4) same_surface(*a.m_next, *b.m_next, depth + 1);
}

static void same_base_item(const AtTextureBase* a, const AtTextureBase* b) {
    eq("item's name", std::memcmp(a->m_name, b->m_name, 8), 0);
    eq("item's transparent colour", *(const uint32_t*)a->m_transparent, *(const uint32_t*)b->m_transparent);
    eq("item's light direction", *(const uint32_t*)a->m_lightDirection, *(const uint32_t*)b->m_lightDirection);
    eq("item's usage", a->m_usage, b->m_usage);
    eq("item's flags", a->m_flags, b->m_flags);
    eq("item's stage", a->m_textureStage, b->m_textureStage);
    eq("item's levels", a->m_levels, b->m_levels);
    eq("item's levels asked", a->m_levelsAsked, b->m_levelsAsked);
    eq("item's +1F", a->m_unknown1F, b->m_unknown1F);
    eq("item's source", a->m_progSource != nullptr, b->m_progSource != nullptr);
}

static AtGrD3DTextureDatabase* g_tdb[2][3];  // by side: the databases of a case, so that a link can be named
static int tdb_index(int side, const void* p) {
    if (!p) return -1;
    for (int i = 0; i < 3; i++) if (g_tdb[side][i] == p) return i;
    return 99;
}

static void same_texdb(const char* what, const AtGrD3DTextureDatabase* o, const AtGrD3DTextureDatabase* r) {
    g_recording = false;
    eq("2D surfaces", o->m_surface2DCount, r->m_surface2DCount);
    eq("3D surfaces", o->m_surface3DCount, r->m_surface3DCount);
    eq("2D textures", o->m_texture2DCount, r->m_texture2DCount);
    eq("3D textures", o->m_texture3DCount, r->m_texture3DCount);
    eq("cube maps", o->m_cubeMapCount, r->m_cubeMapCount);
    eq("more", tdb_index(0, o->m_more), tdb_index(1, r->m_more));
    eq("directory", o->m_directory != nullptr, r->m_directory != nullptr);
    same_texmap(o, r);
    eq("owner", o->m_owner == g_own[0] ? 1 : o->m_owner ? 2 : 0, r->m_owner == g_own[1] ? 1 : r->m_owner ? 2 : 0);
    eq("taken", o->m_surfaces2DTaken, r->m_surfaces2DTaken);
    eq("+50", o->m_unknown50, r->m_unknown50);
    eq("next", tdb_index(0, o->m_next), tdb_index(1, r->m_next));
    eq("arrays", (o->m_surfaces2D != nullptr) | (o->m_surfaces3D != nullptr) << 1 | (o->m_textures2D != nullptr) << 2 | (o->m_textures3D != nullptr) << 3 | (o->m_cubeMaps != nullptr) << 4,
       (r->m_surfaces2D != nullptr) | (r->m_surfaces3D != nullptr) << 1 | (r->m_textures2D != nullptr) << 2 | (r->m_textures3D != nullptr) << 3 | (r->m_cubeMaps != nullptr) << 4);
    if (o->m_textures2D && r->m_textures2D && o->m_texture2DCount == r->m_texture2DCount) {
        for (unsigned i = 0; i < o->m_texture2DCount; i++) {
            const AtGrD3DTexture2D& a = o->m_textures2D[i];
            const AtGrD3DTexture2D& b = r->m_textures2D[i];
            same_base_item(&a, &b);
            eq("item's database", tdb_index(0, a.m_database), tdb_index(1, b.m_database));
            eq("item's owner", a.m_owner == g_own[0] ? 1 : a.m_owner ? 2 : 0, b.m_owner == g_own[1] ? 1 : b.m_owner ? 2 : 0);
            eq("item's texture", (uintptr_t)a.m_texture, (uintptr_t)b.m_texture);
            eq("item's surfaces", index_of(o->m_surfaces2D, a.m_surfaces, o->m_surface2DCount + 64), index_of(r->m_surfaces2D, b.m_surfaces, r->m_surface2DCount + 64));
        }
    }
    if (o->m_surfaces2D && r->m_surfaces2D && o->m_surface2DCount == r->m_surface2DCount) {
        for (unsigned i = 0; i < o->m_surface2DCount; i++) same_surface(o->m_surfaces2D[i], r->m_surfaces2D[i]);
    }
    eq("owner's first", tdb_index(0, g_own[0]->m_textureDatabases), tdb_index(1, g_own[1]->m_textureDatabases));
    compare_logs(what);
}

static void same_stage(const AtGrD3DMaterialStage& a, const AtGrD3DMaterialStage& b) {
    eq("stage's entries", a.m_entries != nullptr, b.m_entries != nullptr);
    if (a.m_entries && b.m_entries) {
        const unsigned n = ((const unsigned*)a.m_entries)[-1];
        eq("stage's entry count", n, ((const unsigned*)b.m_entries)[-1]);
        if (n == ((const unsigned*)b.m_entries)[-1]) eq("stage's entry bytes", std::memcmp(a.m_entries, b.m_entries, n * sizeof(AtGrD3DStageEntry)), 0);
    }
    eq("stage's +4", a.m_unknown04 != nullptr, b.m_unknown04 != nullptr);
    eq("stage's +8", a.m_unknown08 != nullptr, b.m_unknown08 != nullptr);
    eq("stage's words", std::memcmp((const char*)&a + 0x0C, (const char*)&b + 0x0C, sizeof(AtGrD3DMaterialStage) - 0x0C), 0);
}

static void same_shared() {
    const int was = g_side;
    unsigned v[2][6];
    bool e[2], b0[2], b1[2];
    for (int side = 0; side < 2; side++) {
        g_side = side;
        v[side][0] = shared_users();
        v[side][1] = shared_word(0);
        v[side][2] = shared_word(1);
        v[side][3] = shared_word(2);
        e[side] = shared_entries() != nullptr;
        b0[side] = shared_block(0) != nullptr;
        b1[side] = shared_block(1) != nullptr;
    }
    g_side = was;
    eq("shared users", v[0][0], v[1][0]);
    eq("shared 7C", v[0][1], v[1][1]);
    eq("shared 80", v[0][2], v[1][2]);
    eq("shared 8C", v[0][3], v[1][3]);
    eq("shared entries", e[0], e[1]);
    eq("shared 84", b0[0], b0[1]);
    eq("shared 88", b1[0], b1[1]);
}

static AtGrD3DMaterialDatabase* g_mdb[2][3];
static int mdb_index(int side, const void* p) {
    if (!p) return -1;
    for (int i = 0; i < 3; i++) if (g_mdb[side][i] == p) return i;
    return 99;
}

static long long list_index(void** list, void** p) {
    if (!p) return -1;
    if (!list) return -2;
    return p - list;
}

static void same_matdb(const char* what, const AtGrD3DMaterialDatabase* o, const AtGrD3DMaterialDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("stage capacity", o->m_stageCapacity, r->m_stageCapacity);
    eq("stages used", o->m_stageUsed, r->m_stageUsed);
    eq("texture capacity", o->m_textureCapacity, r->m_textureCapacity);
    eq("textures used", o->m_textureUsed, r->m_textureUsed);
    eq("more", mdb_index(0, o->m_more), mdb_index(1, r->m_more));
    same_map(o, r, o->m_materials, r->m_materials);
    eq("owner", o->m_owner == g_own[0] ? 1 : o->m_owner ? 2 : 0, r->m_owner == g_own[1] ? 1 : r->m_owner ? 2 : 0);
    eq("next", mdb_index(0, o->m_next), mdb_index(1, r->m_next));
    eq("materials", o->m_materials != nullptr, r->m_materials != nullptr);
    if (o->m_materials && r->m_materials && o->m_count == r->m_count) {
        for (unsigned i = 0; i < o->m_count; i++) {
            const AtGrD3DMaterial& a = o->m_materials[i];
            const AtGrD3DMaterial& b = r->m_materials[i];
            eq("material's store", mdb_index(0, a.m_store), mdb_index(1, b.m_store));
            eq("material's stages", list_index(o->m_stages, a.m_stages), list_index(r->m_stages, b.m_stages));
            eq("material's textures", list_index(o->m_textures, a.m_textures), list_index(r->m_textures, b.m_textures));
            unsigned char x[sizeof(AtMaterial)], y[sizeof(AtMaterial)];
            std::memcpy(x, &a, sizeof x);
            std::memcpy(y, &b, sizeof y);
            for (const size_t f : {offsetof(AtMaterial, m_store), offsetof(AtMaterial, m_stages), offsetof(AtMaterial, m_textures)}) {
                std::memset(x + f, 0, 4);
                std::memset(y + f, 0, 4);
            }
            eq("material", std::memcmp(x + 4, y + 4, sizeof x - 4), 0);
            for (int s = 0; s < 8; s++) same_stage(a.m_stageStates[s], b.m_stageStates[s]);
            eq("material's +188", a.m_unknown188, b.m_unknown188);
            eq("material's stage states", a.m_stageStateCount, b.m_stageStateCount);
        }
    }
    eq("owner's first", mdb_index(0, g_own[0]->m_materialDatabases), mdb_index(1, g_own[1]->m_materialDatabases));
    same_shared();
    compare_logs(what);
}

// ---- making and destroying -----------------------------------------------------------------------------------------------------

static AtGrD3DTextureDatabase* make_tex(int side, int k, unsigned char fill) {
    void* p = std::malloc(sizeof(AtGrD3DTextureDatabase));
    std::memset(p, fill, sizeof(AtGrD3DTextureDatabase));
    AtGrD3DTextureDatabase* d;
    if (side == 0) {
        eq("constructor returns this", (uintptr_t)at<void*(__fastcall*)(void*, void*)>(0x0010FFC0)(p, nullptr), (uintptr_t)p);
        eq("the original's vftable", *(uint32_t*)p, 0x0028A6E0);
        d = (AtGrD3DTextureDatabase*)p;
    } else {
        d = new (p) AtGrD3DTextureDatabase;
    }
    g_tdb[side][k] = d;
    return d;
}

static void destroy_tex(AtGrD3DTextureDatabase* o, AtGrD3DTextureDatabase* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00112240)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~AtGrD3DTextureDatabase(); }
    compare_logs(what);
    eq("left: entries", o->m_entries != nullptr, r->m_entries != nullptr);
    eq("left: owner", o->m_owner != nullptr, r->m_owner != nullptr);
    eq("left: next", tdb_index(0, o->m_next), tdb_index(1, r->m_next));
    for (int side = 0; side < 2; side++) for (int k = 0; k < 3; k++) if (g_tdb[side][k] == (side ? r : o)) g_tdb[side][k] = nullptr;
    std::free(o);
    std::free(r);
}

static AtGrD3DMaterialDatabase* make_mat(int side, int k, unsigned char fill) {
    void* p = std::malloc(sizeof(AtGrD3DMaterialDatabase));
    std::memset(p, fill, sizeof(AtGrD3DMaterialDatabase));
    AtGrD3DMaterialDatabase* d;
    if (side == 0) {
        eq("constructor returns this", (uintptr_t)at<void*(__fastcall*)(void*, void*)>(0x000F7790)(p, nullptr), (uintptr_t)p);
        eq("the original's vftable", *(uint32_t*)p, 0x00289300);
        d = (AtGrD3DMaterialDatabase*)p;
    } else {
        d = new (p) AtGrD3DMaterialDatabase;
    }
    g_mdb[side][k] = d;
    return d;
}

static void destroy_mat(AtGrD3DMaterialDatabase* o, AtGrD3DMaterialDatabase* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00112370)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~AtGrD3DMaterialDatabase(); }
    compare_logs(what);
    eq("left: entries", o->m_entries != nullptr, r->m_entries != nullptr);
    eq("left: next", mdb_index(0, o->m_next), mdb_index(1, r->m_next));
    for (int side = 0; side < 2; side++) for (int k = 0; k < 3; k++) if (g_mdb[side][k] == (side ? r : o)) g_mdb[side][k] = nullptr;
    std::free(o);
    std::free(r);
    same_shared();
}

static void reset_case() {
    g_next_handle[0] = g_next_handle[1] = 0x100;
    g_loads[0] = g_loads[1] = 0;
}

// A load on each side, either of which may crash. Crashing must agree, and so must everything up to it.
template <class D> static void load_both(D* o, D* r, uint32_t va, const char* what, bool expect_crash_free = true) {
    const bool oc = guarded([&] { On on(0); at<FnLoad>(va)(o, nullptr, g_own[0], "MODEL\\THING", 77); });
    g_recording = false;
    g_fail_alloc = false;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], "MODEL\\THING", 77); });
    g_recording = false;
    g_fail_alloc = false;
    g_fs->m_path.m_top = -1;
    eq("crashed", oc, rc);
    if (expect_crash_free) eq("crash-free", oc, 0);
    (void)what;
}

static unsigned g_tex_files, g_mat_files, g_textures, g_surfaces, g_materials, g_chained, g_built;

// A spy on a material array's destruction, by side: before the array's own vector deleting destructor runs, each material's stage-state count and flags are logged, so that what Clear did to each material
// before deleting them shows. Each material gets a copy of its table with slot 1 replaced.
static void* g_spy_table[2][5];
static void* g_spy_real[2];
typedef void*(__fastcall* FnVecDel)(void*, void*, unsigned);
template <int Side> static void* __fastcall spy_vecdel(AtGrD3DMaterial* self, void*, unsigned flags) {
    const unsigned n = (flags & 2) ? ((unsigned*)self)[-1] : 1;
    for (unsigned i = 0; i < n; i++) {
        slot('S', self[i].m_stageStateCount);
        slot('s', self[i].m_flags);
    }
    return ((FnVecDel)g_spy_real[Side])(self, nullptr, flags);
}
static void spy_on(int side, AtGrD3DMaterial* m, unsigned count) {
    if (!m || !count) return;
    void** real = *(void***)m;
    std::memcpy(g_spy_table[side], real, sizeof g_spy_table[side]);
    g_spy_real[side] = real[1];
    g_spy_table[side][1] = side ? (void*)&spy_vecdel<1> : (void*)&spy_vecdel<0>;
    for (unsigned i = 0; i < count; i++) *(void***)&m[i] = g_spy_table[side];
}

static void set_file(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, "ART\\CAST/KND1\\THING.AGT");
}

static void run_agt(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, bool known, bool stats) {
    g_context = context + (known ? " known" : "");
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    if (!known) names.clear();
    Env env(names, spaces);
    set_file(data);
    g_fill = fill;
    reset_case();
    AtGrD3DTextureDatabase* o = make_tex(0, 0, fill);
    AtGrD3DTextureDatabase* r = make_tex(1, 0, fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtGrD3DTextureDatabase) - 4), 0);
    load_both(o, r, 0x0001C7B0, "load");
    same_texdb("load", o, r);
    if (stats) {
        g_textures += r->m_texture2DCount;
        g_surfaces += r->m_surface2DCount;
        for (unsigned i = 0; i < r->m_surface2DCount; i++) g_chained += r->m_surfaces2D[i].m_next != nullptr;
    }
    // The texture slots that loading does not reach, on the first two textures.
    for (unsigned i = 0; i < std::min(o->m_texture2DCount, 2u); i++) {
        AtGrD3DTexture2D* a = &o->m_textures2D[i];
        AtGrD3DTexture2D* b = &r->m_textures2D[i];
        void* sa;
        void* sb;
        { On on(0); sa = at<FnAt>(0x000FC570)(a, nullptr, 1); }
        { On on(1); sb = b->SurfaceAt(1); }
        eq("surface at", index_of(o->m_surfaces2D, sa, 1 << 20), index_of(r->m_surfaces2D, sb, 1 << 20));
        if (a->m_surfaces && b->m_surfaces && index_of(o->m_surfaces2D, a->m_surfaces, o->m_surface2DCount) >= 0 && a->m_surfaces < o->m_surfaces2D + o->m_surface2DCount) {
            void* pa;
            void* pb;
            { On on(0); pa = at<void*(__fastcall*)(void*, void*)>(0x000FD230)(a->m_surfaces, nullptr); }
            { On on(1); pb = b->PaletteOf(); }
            eq("palette", pa ? (char*)pa - (char*)a->m_surfaces : -1, pb ? (char*)pb - (char*)b->m_surfaces : -1);
        }
    }
    same_texdb("slots", o, r);
    // Not loaded over what is there: the base's Read clears first, which forgets the file system Load pushed the directory on, and Load's pop then crashes, on both sides. The texture suite shows that.
    // More of the same, which goes on the owner's list in front.
    if (o->m_texture2DCount < 64) {
        g_tdb[0][1] = g_tdb[1][1] = nullptr;
        { On on(0); at<Fn0>(0x001100D0)(o, nullptr); }
        { On on(1); r->MakeMore(); }
        g_tdb[0][1] = (AtGrD3DTextureDatabase*)o->m_more;
        g_tdb[1][1] = (AtGrD3DTextureDatabase*)r->m_more;
        same_texdb("more", o, r);
        if (g_tdb[0][1] && g_tdb[1][1]) {
            same_texdb("the more", g_tdb[0][1], g_tdb[1][1]);
            { On on(0); at<FnDel>(0x00112240)(g_tdb[0][1], nullptr, 1); }
            { On on(1); delete g_tdb[1][1]; }
            g_tdb[0][1] = g_tdb[1][1] = nullptr;
            same_texdb("the more deleted", o, r);
        }
    }
    // Cleared, and clearing nothing.
    { On on(0); at<Fn0>(0x0010FFF0)(o, nullptr); at<Fn0>(0x0010FFF0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_texdb("clear", o, r);
    load_both(o, r, 0x0001C7B0, "load after clear");
    same_texdb("load after clear", o, r);
    destroy_tex(o, r, "destroy");
}

static void run_agm(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, bool stats) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    set_file(data);
    g_fill = fill;
    reset_case();
    AtGrD3DMaterialDatabase* o = make_mat(0, 0, fill);
    AtGrD3DMaterialDatabase* r = make_mat(1, 0, fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtGrD3DMaterialDatabase) - 4), 0);
    load_both(o, r, 0x00038FB0, "load");
    same_matdb("load", o, r);
    if (stats) {
        g_materials += r->m_count;
        for (unsigned i = 0; r->m_materials && i < r->m_count; i++) g_built += r->m_materials[i].m_stageStateCount;
    }
    load_both(o, r, 0x00038FB0, "load again");
    same_matdb("load again", o, r);
    // More, and a material past the count, which is the more's.
    if (o->m_count < 64 && o->m_owner && r->m_owner) {  // with no owner kept, the more is made for none, and both crash: not run
        { On on(0); at<Fn0>(0x000F7AF0)(o, nullptr); }
        { On on(1); r->Slot3(); }
        g_mdb[0][1] = (AtGrD3DMaterialDatabase*)o->m_more;
        g_mdb[1][1] = (AtGrD3DMaterialDatabase*)r->m_more;
        same_matdb("more", o, r);
        if (g_mdb[0][1] && g_mdb[1][1]) {
            same_matdb("the more", g_mdb[0][1], g_mdb[1][1]);
            for (unsigned i : {0u, o->m_count, o->m_count + (o->m_count ? o->m_count - 1 : 0)}) {
                if (o->m_count == 0) break;  // the more has none either, and goes on to its own more, which is nothing: both crash
                void* a;
                void* b;
                { On on(0); a = at<FnAt>(0x000F7840)(o, nullptr, i); }
                { On on(1); b = r->MaterialAt(i); }
                const bool mine = i < o->m_count;
                eq("material at", index_of(mine ? o->m_materials : g_mdb[0][1]->m_materials, a, 1 << 20), index_of(mine ? r->m_materials : g_mdb[1][1]->m_materials, b, 1 << 20));
            }
            compare_logs("material at");
            { On on(0); at<FnDel>(0x00112370)(g_mdb[0][1], nullptr, 1); }
            { On on(1); delete g_mdb[1][1]; }
            g_mdb[0][1] = g_mdb[1][1] = nullptr;
            same_matdb("the more deleted", o, r);
        }
    }
    spy_on(0, o->m_materials, o->m_count);
    spy_on(1, r->m_materials, r->m_count);
    { On on(0); at<Fn0>(0x000F77B0)(o, nullptr); at<Fn0>(0x000F77B0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_matdb("clear", o, r);
    load_both(o, r, 0x00038FB0, "load after clear");
    same_matdb("load after clear", o, r);
    destroy_mat(o, r, "destroy");
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

// ---- written cases -----------------------------------------------------------------------------------------------------------

static void run_written() {
    static const char* const textures[] = {
        "",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n PlatformSource\n }\n}\n",
        "Surface2D[6]\nTexture[3]\nTexture2D[3]\n{\n Texture2D \"A\"\n {\n DDSSource\n MipMap 3\n }\n Texture2D \"B\"\n {\n BackBufferSource\n MipMap 2\n }\n Texture2D \"C\"\n {\n FrontBufferSource\n }\n}\n",
        "Surface2D[5]\nTexture[4]\nTexture2D[4]\n{\n Texture2D \"A\"\n {\n }\n Texture2D \"B\"\n {\n DemandLoad\n }\n Texture2D \"C\"\n {\n MipMap 2\n }\n Texture2D \"D\"\n {\n }\n}\n",
        // Too few surfaces: the report, and the surfaces past the end handed out anyway. Run with a zero fill only: clearing releases the surfaces past the end, which are what the heap left there, and with
        // any other fill both sides follow a chain into nowhere.
        "Surface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"A\"\n {\n MipMap 2\n }\n Texture2D \"B\"\n {\n MipMap 3\n }\n}\n",
        "Texture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        // An empty list of each: arrays of none.
        "Surface2D[0]\nTexture[0]\nTexture2D[0]\n{\n}\n",
        // The kinds the disc has none of: seam classes on both sides.
        "Surface2D[1]\nSurface3D[2]\nTexture[3]\nTexture3D[1]\n{\n Texture3D \"V\"\n {\n }\n}\nCubeMapTexture[1]\n{\n CubeMapTexture \"C\"\n {\n }\n}\nTexture2D[1]\n{\n Texture2D \"T\"\n {\n }\n}\n",
        "NameSpace \"Other\"\nSurface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"A\"\n {\n }\n Texture2D \"B\"\n {\n }\n}\n",
        "NameSpace \"Exactly8\"\nSurface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        // A texture given twice: known the second time, so not loaded.
        "Surface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"XA\"\n {\n }\n Texture2D \"XA\"\n {\n }\n}\n",
    };
    for (const int every : {1, 3}) {
        g_attach_every = every;
        for (const unsigned char fill_given : {(unsigned char)0xCD, (unsigned char)0x33}) {
            int n = 0;
            for (const char* t : textures) {
                const std::vector<unsigned char> d = bytes(t);
                const unsigned char fill = n == 4 ? 0 : fill_given;
                run_agt(d, "written texture " + std::to_string(n) + " fill " + std::to_string(fill) + " every " + std::to_string(every), fill, false, false);
                run_agt(d, "written texture " + std::to_string(n), fill, true, false);
                n++;
            }
        }
    }
    g_attach_every = 3;
    static const char* const materials[] = {
        "",
        "Material[0]\n{\n}\n",
        "Material[1]\n{\n Material \"A\"\n {\n DiffuseColor 1 2 3 4\n }\n}\n",
        "StagedShaderTexture[2]\n{\n \"T\"\n \"U\"\n}\nMaterial[4]\n{\n Material \"A\"\n {\n }\n Material \"B\"\n {\n SimpleTextureShader 1\n }\n Material \"C\"\n {\n AmbientColor 9 9 9 9\n }\n Material \"D\"\n {\n }\n}\n",
        "NameSpace \"Other\"\nMaterial[3]\n{\n Material \"A\"\n {\n }\n Material \"B\"\n {\n }\n Material \"C\"\n {\n }\n}\n",
        "NameSpace \"Exactly8\"\nMaterial[1]\n{\n Material \"A\"\n {\n }\n}\n",
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        int n = 0;
        for (const char* t : materials) run_agm(bytes(t), "written material " + std::to_string(n++) + " fill " + std::to_string(fill), fill, false);
    }
}

// The pieces directly: what loading does not reach, and what fails.
static void run_direct() {
    g_context = "direct";
    std::set<std::string> none, spaces = {"Space"};
    Env env(none, spaces);
    g_fill = 0xCD;
    reset_case();

    // The owner's lists, in each order of removal, and a database not on them.
    {
        AtGrD3DTextureDatabase* t[2][3];
        AtGrD3DMaterialDatabase* m[2][3];
        for (int side = 0; side < 2; side++) for (int k = 0; k < 3; k++) { t[side][k] = make_tex(side, k, 0xCD); m[side][k] = make_mat(side, k, 0xCD); }
        for (int order = 0; order < 4; order++) {
            g_context = "lists, order " + std::to_string(order);
            for (int k = 0; k < 3; k++) {
                { On on(0); at<Fn1>(0x000EF0A0)(g_own[0], nullptr, t[0][k]); at<Fn1>(0x000EF0F0)(g_own[0], nullptr, m[0][k]); }
                { On on(1); g_own[1]->AddTextureDatabase(t[1][k]); g_own[1]->AddMaterialDatabase(m[1][k]); }
            }
            static const int orders[4][4] = {{0, 1, 2, 2}, {2, 1, 0, 0}, {1, 0, 2, 1}, {1, 2, 0, 2}};
            for (int i = 0; i < 4; i++) {
                const int k = orders[order][i];
                { On on(0); at<Fn1>(0x000EF0C0)(g_own[0], nullptr, t[0][k]); at<Fn1>(0x000EF110)(g_own[0], nullptr, m[0][k]); }
                { On on(1); g_own[1]->RemoveTextureDatabase(t[1][k]); g_own[1]->RemoveMaterialDatabase(m[1][k]); }
                eq("texture list", tdb_index(0, g_own[0]->m_textureDatabases), tdb_index(1, g_own[1]->m_textureDatabases));
                eq("material list", mdb_index(0, g_own[0]->m_materialDatabases), mdb_index(1, g_own[1]->m_materialDatabases));
                for (int j = 0; j < 3; j++) {
                    eq("texture link", tdb_index(0, t[0][j]->m_next), tdb_index(1, t[1][j]->m_next));
                    eq("material link", mdb_index(0, m[0][j]->m_next), mdb_index(1, m[1][j]->m_next));
                }
            }
            compare_logs("lists");
        }
        g_context = "direct";
        for (int k = 0; k < 3; k++) { destroy_tex(t[0][k], t[1][k], "destroy listed"); destroy_mat(m[0][k], m[1][k], "destroy listed"); }
    }

    // Each kind's array by count, from the base's allocator, with the accessors, then the slots of a 2D texture and surface.
    for (const unsigned count : {0u, 1u, 3u}) {
        g_context = "make " + std::to_string(count);
        AtGrD3DTextureDatabase* o = make_tex(0, 0, 0xCD);
        AtGrD3DTextureDatabase* r = make_tex(1, 0, 0xCD);
        o->m_surface2DCount = r->m_surface2DCount = count + 2;
        o->m_surface3DCount = r->m_surface3DCount = count;
        o->m_texture2DCount = r->m_texture2DCount = count;
        o->m_texture3DCount = r->m_texture3DCount = count + 1;  // each kind its own count, so that one taken for another shows
        o->m_cubeMapCount = r->m_cubeMapCount = count + 2;
        { On on(0); at<Fn1>(0x0010F810)(o, nullptr, g_own[0]); for (uint32_t va : {0x0010F830u, 0x0010F950u, 0x0010FA70u, 0x0010FB90u, 0x0010FCB0u}) at<Fn0>(va)(o, nullptr); }
        { On on(1); r->OnReady(g_own[1]); r->MakeSurfaces2D(); r->MakeSurfaces3D(); r->MakeTextures2D(); r->MakeTextures3D(); r->MakeCubeMaps(); }
        g_context += " made";
        same_texdb("made", o, r);
        if (count) {
            static const uint32_t vas[5] = {0x0010FDD0, 0x0010FDF0, 0x0010FE10, 0x0010FE20, 0x0010FE40};
            for (int k = 0; k < 5; k++) {
                void* a;
                void* b;
                { On on(0); a = at<FnAt>(vas[k])(o, nullptr, count - 1); }
                {
                    On on(1);
                    b = k == 0 ? r->Surface2DAt(count - 1) : k == 1 ? r->Surface3DAt(count - 1) : k == 2 ? (void*)r->Texture2DAt(count - 1) : k == 3 ? (void*)r->Texture3DAt(count - 1) : (void*)r->CubeMapAt(count - 1);
                }
                char* const obase[5] = {(char*)o->m_surfaces2D, (char*)o->m_surfaces3D, (char*)o->m_textures2D, (char*)o->m_textures3D, (char*)o->m_cubeMaps};
                char* const rbase[5] = {(char*)r->m_surfaces2D, (char*)r->m_surfaces3D, (char*)r->m_textures2D, (char*)r->m_textures3D, (char*)r->m_cubeMaps};
                eq("accessor", (char*)a - obase[k], (char*)b - rbase[k]);
            }
            // More of the same counts, each its own.
            {
                { On on(0); at<Fn0>(0x001100D0)(o, nullptr); }
                { On on(1); r->MakeMore(); }
                g_tdb[0][1] = (AtGrD3DTextureDatabase*)o->m_more;
                g_tdb[1][1] = (AtGrD3DTextureDatabase*)r->m_more;
                same_texdb("more of each", o, r);
                same_texdb("the more of each", g_tdb[0][1], g_tdb[1][1]);
                { On on(0); at<FnDel>(0x00112240)(g_tdb[0][1], nullptr, 1); }
                { On on(1); delete g_tdb[1][1]; }
                o->m_more = r->m_more = nullptr;
                g_tdb[0][1] = g_tdb[1][1] = nullptr;
                same_texdb("more of each deleted", o, r);
            }
            g_context += " describe";
            // Describe, as Allocate's callers would: one surface for a buffer source, else one to a level.
            for (int k = 0; k < 2; k++) {
                const unsigned usage = k ? 0x41u : 0x10u;
                { On on(0); at<FnDescribe>(0x000FC6C0)(&o->m_textures2D[0], nullptr, o, k ? "BUFFER" : "Exactly8", usage, k, k ? 2 : 1, 3); }
                { On on(1); r->m_textures2D[0].Describe(r, k ? "BUFFER" : "Exactly8", usage, k, k ? 2 : 1, 3); }
                same_texdb("describe", o, r);
                o->m_textures2D[0].m_flags |= AtTextureBase::kBackBufferSource;
                r->m_textures2D[0].m_flags |= AtTextureBase::kBackBufferSource;
            }
            { On on(0); at<FnDescribe>(0x000FC6C0)(&o->m_textures2D[0], nullptr, o, "AGAIN", 0, 0, 4, 0); }
            { On on(1); r->m_textures2D[0].Describe(r, "AGAIN", 0, 0, 4, 0); }
            same_texdb("describe a buffer", o, r);
            // No levels asked: one, as the base makes it.
            o->m_textures2D[0].m_flags = r->m_textures2D[0].m_flags = 0;
            { On on(0); at<FnDescribe>(0x000FC6C0)(&o->m_textures2D[0], nullptr, o, "NONE", 0, 0, 0, 0); }
            { On on(1); r->m_textures2D[0].Describe(r, "NONE", 0, 0, 0, 0); }
            same_texdb("describe no levels", o, r);
            g_context += " pending";
            // Loaded, then pending and not, cleared of pending; then reset.
            {
                AtGrD3DTexture2D* a = &o->m_textures2D[0];
                AtGrD3DTexture2D* b = &r->m_textures2D[0];
                a->m_surfaces = o->m_surfaces2D;
                b->m_surfaces = r->m_surfaces2D;
                a->m_levels = b->m_levels = 1;
                { On on(0); o_load_texture(a, nullptr, g_own[0]); }
                { On on(1); AtGrD3DLoadTexture2D(b, g_own[1]); }
                for (const bool pending : {false, true}) {
                    if (pending) { a->m_flags |= AtTextureBase::kPending; b->m_flags |= AtTextureBase::kPending; }
                    { On on(0); at<Fn0>(0x000FCCA0)(a, nullptr); }
                    { On on(1); b->ClearPending(); }
                    same_texdb(pending ? "clear pending" : "not pending", o, r);
                }
                // Pending with no surfaces: the release is skipped.
                a->m_surfaces = nullptr;
                b->m_surfaces = nullptr;
                a->m_texture = b->m_texture = (void*)(uintptr_t)0x77;
                a->m_flags |= AtTextureBase::kPending;
                b->m_flags |= AtTextureBase::kPending;
                { On on(0); at<Fn0>(0x000FCCA0)(a, nullptr); }
                { On on(1); b->ClearPending(); }
                same_texdb("pending, no surfaces", o, r);
                a->m_surfaces = o->m_surfaces2D;
                b->m_surfaces = r->m_surfaces2D;
                { On on(0); o_load_texture(a, nullptr, g_own[0]); }
                { On on(1); AtGrD3DLoadTexture2D(b, g_own[1]); }
                { On on(0); at<Fn0>(0x000FCC80)(a, nullptr); }
                { On on(1); b->Reset(); }
                same_texdb("reset", o, r);
            }
            g_context += " surface";
            // A surface's base slots: a colour key, cleared; a palette only when indexed.
            {
                AtGrD3DSurface2D* a = &o->m_surfaces2D[1];
                AtGrD3DSurface2D* b = &r->m_surfaces2D[1];
                const unsigned char key[4] = {1, 2, 3, 4};
                a->m_flags = b->m_flags = 0xFFF0;  // other flags, which clearing the key keeps
                { On on(0); at<void(__fastcall*)(void*, void*, const unsigned char*)>(0x000604E0)(a, nullptr, key); }
                { On on(1); b->SetColorKey(key); }
                same_surface(*a, *b);
                { On on(0); at<Fn0>(0x00060400)(a, nullptr); }
                { On on(1); b->ClearColorKey(); }
                same_surface(*a, *b);
                for (const unsigned short flags : {(unsigned short)AtPixelFormat::kIndex, (unsigned short)AtPixelFormat::kRgba, (unsigned short)0xFFFB}) {
                    a->m_format.m_flags = b->m_format.m_flags = flags;
                    void* pa;
                    void* pb;
                    { On on(0); pa = at<void*(__fastcall*)(void*, void*)>(0x000FD230)(a, nullptr); }
                    { On on(1); pb = b->Palette(); }
                    eq("palette", pa ? (char*)pa - (char*)a : -1, pb ? (char*)pb - (char*)b : -1);
                }
                g_context += " chain";
                // Reset with everything held: the chain, its blocks, the surface, the palette.
                g_attach_every = 1;
                AtGrD3DTexture2D* ta = &o->m_textures2D[0];
                AtGrD3DTexture2D* tb = &r->m_textures2D[0];
                ta->m_surfaces = a;
                tb->m_surfaces = b;
                ta->m_levels = tb->m_levels = 1;
                ta->m_database = o;  // the reset above cleared it
                tb->m_database = r;
                { On on(0); o_load_texture(ta, nullptr, g_own[0]); }
                { On on(1); AtGrD3DLoadTexture2D(tb, g_own[1]); }
                a->m_next->m_next = nullptr;
                b->m_next->m_next = nullptr;
                { On on(0); at<Fn0>(0x000FCFC0)(a, nullptr); }
                { On on(1); b->ReleaseChain(); }
                same_surface(*a, *b);
                { On on(0); o_load_texture(ta, nullptr, g_own[0]); }
                { On on(1); AtGrD3DLoadTexture2D(tb, g_own[1]); }
                { On on(0); at<Fn0>(0x000FCF40)(a, nullptr); }
                { On on(1); b->Reset(); }
                same_surface(*a, *b);
                compare_logs("surface reset");
                g_attach_every = 3;
                // The base's destructor by itself, on a surface whose base fields are all set; then the surface is destroyed again with the array.
                AtGrD3DSurface2D* c = &o->m_surfaces2D[2];
                AtGrD3DSurface2D* d = &r->m_surfaces2D[2];
                for (AtGrD3DSurface2D* x : {c, d}) {
                    x->m_format.m_flags = 3;
                    x->m_format.m_mask0 = 0xFF;
                    x->m_colorKey[0] = 9;
                    x->m_colorKey[3] = 7;
                    x->m_flags = 0x1234;
                    x->m_unknown20 = 0x20;
                    x->m_unknown22 = 0x22;
                }
                { On on(0); at<Fn0>(0x00060510)(c, nullptr); }
                { On on(1); d->AtGrIPixelSurface::~AtGrIPixelSurface(); }
                eq("base destroyed", std::memcmp((char*)c + 4, (char*)d + 4, sizeof(AtGrIPixelSurface) - 4), 0);
                compare_logs("base destroyed");
            }
            // A palette by itself, destroyed holding entries and a palette.
            {
                void* pa = std::malloc(sizeof(AtGrD3DPalette));
                void* pb = std::malloc(sizeof(AtGrD3DPalette));
                std::memset(pa, 0xCD, sizeof(AtGrD3DPalette));
                std::memset(pb, 0xCD, sizeof(AtGrD3DPalette));
                AtGrD3DPalette* a;
                AtGrD3DPalette* b;
                { On on(0); a = at<AtGrD3DPalette*(__fastcall*)(void*, void*)>(0x000F5A70)(pa, nullptr); a->m_entries = (unsigned*)side_new(8); a->m_resource = handle(); a->m_count = 2; a->m_unknown06 = 6; }
                { On on(1); b = new (pb) AtGrD3DPalette; b->m_entries = (unsigned*)side_new(8); b->m_resource = handle(); b->m_count = 2; b->m_unknown06 = 6; }
                { On on(0); at<Fn0>(0x000F5CC0)(a, nullptr); }
                { On on(1); b->~AtGrD3DPalette(); }
                eq("palette destroyed", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtGrD3DPalette) - 4), 0);
                compare_logs("palette destroyed");
                std::free(pa);
                std::free(pb);
            }
            // A stage entry by itself.
            {
                AtGrD3DStageEntry ea, eb;
                std::memset(&ea, 0x5A, sizeof ea);
                std::memset(&eb, 0x5A, sizeof eb);
                { On on(0); at<Fn0>(0x00105330)(&ea, nullptr); }
                { On on(1); eb.~AtGrD3DStageEntry(); }
                eq("entry destroyed", std::memcmp(&ea, &eb, sizeof ea), 0);
                eq("entry cleared", ea.m_unknown00 | ea.m_unknown04 | ea.m_unknown08 | ea.m_unknown0C | ea.m_unknown0E | *(uint32_t*)ea.m_unknown10, 0);
                compare_logs("entry destroyed");
            }
        }
        { On on(0); at<Fn0>(0x0010FFF0)(o, nullptr); }
        { On on(1); r->Clear(); }
        g_context += " clear";
        same_texdb("cleared", o, r);
        destroy_tex(o, r, "destroy made");
    }

    // A material by itself: built, released; destroyed with stage states built, the last shared stage freeing the shared tables.
    for (int built = 0; built < 4; built++) {
        g_context = "material " + std::to_string(built);
        void* pa = std::malloc(sizeof(AtGrD3DMaterial));
        void* pb = std::malloc(sizeof(AtGrD3DMaterial));
        std::memset(pa, 0xCD, sizeof(AtGrD3DMaterial));
        std::memset(pb, 0xCD, sizeof(AtGrD3DMaterial));
        AtGrD3DMaterial* a;
        AtGrD3DMaterial* b;
        { On on(0); a = at<AtGrD3DMaterial*(__fastcall*)(void*, void*)>(0x0010A3F0)(pa, nullptr); }
        { On on(1); b = new (pb) AtGrD3DMaterial; }
        eq("material's vftable", *(uint32_t*)a, 0x0028A200);
        eq("new material", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtGrD3DMaterial) - 4), 0);
        compare_logs("new material");
        g_loads[0] = g_loads[1] = (unsigned)built;
        a->m_store = b->m_store = nullptr;
        { On on(0); o_load_material(a, nullptr, g_own[0]); }
        { On on(1); AtGrD3DLoadMaterial(b, g_own[1]); }
        if (built == 3) {
            { On on(0); o_load_material(a, nullptr, g_own[0]); o_load_material(a, nullptr, g_own[0]); }
            { On on(1); AtGrD3DLoadMaterial(b, g_own[1]); AtGrD3DLoadMaterial(b, g_own[1]); }
            a->m_stageStateCount = b->m_stageStateCount = 8;
            a->m_stageStates[5].m_flags = b->m_stageStates[5].m_flags = AtGrD3DMaterialStage::kShared;
            g_side = 0; shared_users()++; g_side = 1; shared_users()++; g_side = 0;
        }
        a->m_flags = b->m_flags = 0x7;
        if (built & 1) {
            { On on(0); at<Fn1>(0x0010A460)(a, nullptr, g_own[0]); }
            { On on(1); b->Release(g_own[1]); }
        } else {
            { On on(0); at<Fn1>(0x0010A330)(a, nullptr, g_own[0]); }
            { On on(1); b->ReleaseStages(g_own[1]); }
        }
        eq("released", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtGrD3DMaterial) - 4), 0);
        same_shared();
        compare_logs("released");
        { On on(0); o_load_material(a, nullptr, g_own[0]); }
        { On on(1); AtGrD3DLoadMaterial(b, g_own[1]); }
        { On on(0); at<FnDel>(0x000F7A90)(a, nullptr, 0); }
        { On on(1); b->~AtGrD3DMaterial(); }
        eq("destroyed", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtGrD3DMaterial) - 4), 0);
        same_shared();
        compare_logs("destroyed");
        std::free(pa);
        std::free(pb);
    }
    // A stage by itself, new and reset with each part, and the shared tables freed with and without their parts.
    for (int k = 0; k < 6; k++) {
        g_context = "stage " + std::to_string(k);
        AtGrD3DMaterialStage sa, sb;
        std::memset(&sa, 0xCD, sizeof sa);
        std::memset(&sb, 0xCD, sizeof sb);
        { On on(0); at<Fn0>(0x000F6400)(&sa, nullptr); }
        { On on(1); new (&sb) AtGrD3DMaterialStage; }
        eq("new stage", std::memcmp(&sa, &sb, sizeof sa), 0);
        for (int side = 0; side < 2; side++) {
            On on(side);
            AtGrD3DMaterialStage& s = side ? sb : sa;
            if (k & 1) s.m_entries = new_entries(3);
            if (k & 2) s.m_unknown08 = side_new(4);
            s.m_unknown0C = 0x0C;
            s.m_unknown16 = 0x16;
            s.m_unknown18 = 0x18;
            s.m_unknown1A = 0x1A;
            s.m_unknown1C = 0x1C;
            s.m_unknown1E = 0x1E;
            s.m_unknown20 = 0x20;
            s.m_unknown22 = 0x22;
            s.m_unknown24 = 0x24;
            s.m_unknown26 = 0x26;
            if (k >= 4) {
                // The last user frees the tables; one of two does not.
                s.m_flags = AtGrD3DMaterialStage::kShared;
                shared_users() = k == 4 ? 1 : 2;
                shared_entries() = new_entries(1);
                shared_block(k == 4 ? 1 : 0) = side_new(4);
                shared_word(0) = 1;
                shared_word(1) = 2;
                shared_word(2) = 3;
            }
        }
        { On on(0); at<Fn0>(0x000F6250)(&sa, nullptr); }
        { On on(1); sb.Reset(); }
        eq("reset stage", std::memcmp(&sa, &sb, sizeof sa), 0);
        same_shared();
        compare_logs("stage");
        { On on(0); at<Fn0>(0x000F6450)(&sa, nullptr); }
        { On on(1); sb.~AtGrD3DMaterialStage(); }
        eq("destroyed stage", std::memcmp(&sa, &sb, sizeof sa), 0);
        compare_logs("stage destroyed");
        if (k == 5) {
            { On on(0); at<void(__cdecl*)()>(0x000F61D0)(); }
            { On on(1); AtGrD3DMaterialStage::FreeShared(); }
            same_shared();
            compare_logs("the tables left");
        }
        g_side = 0; shared_users() = 0; g_side = 1; shared_users() = 0; g_side = 0;
    }
    // The shared tables freed directly, with none of their parts.
    {
        g_context = "free shared";
        { On on(0); at<void(__cdecl*)()>(0x000F61D0)(); }
        { On on(1); AtGrD3DMaterialStage::FreeShared(); }
        same_shared();
        compare_logs("free shared");
    }

    // A material database's materials by count, then more of the same, and the heap set.
    for (const unsigned count : {0u, 2u}) {
        g_context = "materials " + std::to_string(count);
        AtGrD3DMaterialDatabase* o = make_mat(0, 0, 0xCD);
        AtGrD3DMaterialDatabase* r = make_mat(1, 0, 0xCD);
        o->m_count = r->m_count = count;
        o->m_stageCapacity = r->m_stageCapacity = 0;
        o->m_textureCapacity = r->m_textureCapacity = 0;
        { On on(0); at<Fn1>(0x000F79C0)(o, nullptr, g_own[0]); }
        { On on(1); r->OnReady(g_own[1]); }
        same_matdb("ready", o, r);
        { On on(0); at<Fn0>(0x000F77B0)(o, nullptr); }
        { On on(1); r->Clear(); }
        same_matdb("cleared", o, r);
        destroy_mat(o, r, "destroy");
    }

    // No heap, then no memory: each array, each more.
    for (int fail = 0; fail < 2; fail++) {
        g_context = fail ? "no memory" : "no heap";
        if (!fail) {
            *(void**)(uintptr_t)0x002C62E4 = nullptr;
            AtTextureDatabase::s_heap = nullptr;
            *(void**)(uintptr_t)0x003B1F90 = nullptr;
            AtGrD3DMaterialDatabase::s_heap = nullptr;
        }
        AtGrD3DTextureDatabase* o = make_tex(0, 0, 0xCD);
        AtGrD3DTextureDatabase* r = make_tex(1, 0, 0xCD);
        o->m_surface2DCount = r->m_surface2DCount = 1;
        o->m_texture2DCount = r->m_texture2DCount = 1;
        static const uint32_t vas[6] = {0x0010F830, 0x0010F950, 0x0010FA70, 0x0010FB90, 0x0010FCB0, 0x001100D0};
        for (int k = 0; k < 6; k++) {
            g_fail_alloc = fail != 0;
            const bool oc = guarded([&] { On on(0); at<Fn0>(vas[k])(o, nullptr); });
            g_recording = false;
            g_fail_alloc = fail != 0;
            const bool rc = guarded([&] {
                On on(1);
                switch (k) {
                case 0: r->MakeSurfaces2D(); break;
                case 1: r->MakeSurfaces3D(); break;
                case 2: r->MakeTextures2D(); break;
                case 3: r->MakeTextures3D(); break;
                case 4: r->MakeCubeMaps(); break;
                default: r->MakeMore(); break;
                }
            });
            g_recording = false;
            g_fail_alloc = false;
            eq("crashed", oc, rc);
            compare_logs("make");
            if (fail) { o->m_more = nullptr; r->m_more = nullptr; }
        }
        g_tdb[0][1] = (AtGrD3DTextureDatabase*)o->m_more;
        g_tdb[1][1] = (AtGrD3DTextureDatabase*)r->m_more;
        same_texdb("made without", o, r);
        if (!fail && o->m_more && r->m_more) {
            { On on(0); at<FnDel>(0x00112240)(o->m_more, nullptr, 1); }
            { On on(1); delete r->m_more; }
            compare_logs("more freed");
            o->m_more = nullptr;
            r->m_more = nullptr;
        }
        g_tdb[0][1] = g_tdb[1][1] = nullptr;
        destroy_tex(o, r, "destroy");
        AtGrD3DMaterialDatabase* mo = make_mat(0, 0, 0xCD);
        AtGrD3DMaterialDatabase* mr = make_mat(1, 0, 0xCD);
        mo->m_count = mr->m_count = 1;
        for (int k = 0; k < 2; k++) {
            g_fail_alloc = fail != 0;
            const bool oc = guarded([&] { On on(0); if (k) at<Fn0>(0x000F7AF0)(mo, nullptr); else at<Fn1>(0x000F79C0)(mo, nullptr, g_own[0]); });
            g_recording = false;
            g_fail_alloc = fail != 0;
            const bool rc = guarded([&] { On on(1); if (k) mr->Slot3(); else mr->OnReady(g_own[1]); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crashed", oc, rc);
            compare_logs("make materials");
            if (fail) { mo->m_more = nullptr; mr->m_more = nullptr; }
        }
        if (!fail && mo->m_more && mr->m_more) {
            { On on(0); at<FnDel>(0x00112370)(mo->m_more, nullptr, 1); }
            { On on(1); delete mr->m_more; }
            compare_logs("more materials freed");
        }
        mo->m_more = nullptr;
        mr->m_more = nullptr;
        if (fail) {
            // The raise came back, which in the game it does not: the database is on the owner's list with no materials, and clearing it releases material 0 of none. Both crash.
            const bool oc = guarded([&] { On on(0); at<Fn0>(0x000F77B0)(mo, nullptr); });
            g_recording = false;
            const bool rc = guarded([&] { On on(1); mr->Clear(); });
            g_recording = false;
            eq("clearing with no materials crashed", oc, rc);
            eq("and did crash", oc, 1);
            compare_logs("clear with no materials");
            mo->m_owner = mr->m_owner = nullptr;
            g_own[0]->m_materialDatabases = g_own[1]->m_materialDatabases = nullptr;
        }
        destroy_mat(mo, mr, "destroy materials");
        if (!fail) {
            *(void**)(uintptr_t)0x002C62E4 = &g_heap_token;
            AtTextureDatabase::s_heap = &g_heap_token;
            at<void(__cdecl*)(void*)>(0x000F7780)(&g_material_heap_token);
            AtGrD3DMaterialDatabase::SetHeap(&g_material_heap_token);
            eq("material heap", (uintptr_t)*(void**)(uintptr_t)0x003B1F90, (uintptr_t)AtGrD3DMaterialDatabase::s_heap);
        }
    }
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

static bool read_index(const std::string& dir, const char* ext, void (*each)(const std::vector<unsigned char>&, const std::string&)) {
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return false; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ext)) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return false; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        each(data, line);
    }
    std::fclose(index);
    return true;
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
    if (pos.size() != 2) { std::printf("usage: oracle_xdb <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x000FCAD0, (const void*)o_load_texture);
    redirect_recovered_slot12((const void*)o_load_texture);
    redirect(0x0010B7F0, (const void*)o_load_material);
    redirect(0x000FB630, (const void*)o_release_stage);
    redirect(0x00205E20, (const void*)o_before_release);
    redirect(0x00205D10, (const void*)o_release);
    redirect(0x001214A0, (const void*)&Side0<AtGrD3DTexture3D>::Construct);
    redirect(0x00121730, (const void*)&Side0<AtGrD3DCubeMap>::Construct);
    redirect(0x000FCD50, (const void*)&Side0<AtGrD3DSurface3D>::Construct);
    redirect(0x00125F25, (const void*)pure_call);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C62E4u, 0x002C63F0u}) *(void**)(uintptr_t)va = &g_heap_token;
    *(void**)(uintptr_t)0x003B1F90 = &g_material_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtTextureDatabase::s_heap = &g_heap_token;
    AtMaterialDatabase::s_heap = &g_heap_token;
    AtGrD3DMaterialDatabase::s_heap = &g_material_heap_token;
    g_heap_pad = 0x100;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_direct();
    run_written();

    const std::string dir = pos[1];
    g_textures = g_surfaces = g_materials = g_chained = g_built = 0;
    if (!read_index(dir, ".AGT\t", [](const std::vector<unsigned char>& d, const std::string& name) {
            run_agt(d, name, 0xCD, false, true);
            run_agt(d, name, 0xCD, true, false);
            g_tex_files++;
        }))
        return 2;
    if (!read_index(dir, ".AGM\t", [](const std::vector<unsigned char>& d, const std::string& name) {
            run_agm(d, name, 0xCD, true);
            g_mat_files++;
        }))
        return 2;
    report_entries();
    std::printf("%s: %u .AGT (%u 2D textures, %u surfaces, %u chained), %u .AGM (%u materials, %u stage states built); %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED",
                g_tex_files, g_textures, g_surfaces, g_chained, g_mat_files, g_materials, g_built, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
