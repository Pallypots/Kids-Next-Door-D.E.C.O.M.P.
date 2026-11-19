// Differential oracle for the Xbox shader, light, font and image databases and what they hold (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-19-xshade.md.
//
//   oracle_xshade <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// What it does not show, first. What a font or an image does with the device once read (the original's 0x00123850 and 0x00124CD0) is this harness's on both sides: it logs, and marks the item as loaded in
// a way clearing and destroying can see (a handle in its own 2D surface, now and then blocks for that surface to free, its own texture database put on the owner's list, and for a font now and then the flag
// that makes emptying forget its characters rather than free them). The device's release (0x00205D10) logs. So real files show that the databases and their items make, read, hand out, empty, clear and destroy
// the same, in the same order, with the same allocations, as far as those doubles; nothing of pixels, glyphs or anything drawn. A shader and a light have nothing of the device: they are compared whole.
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
typedef void(__fastcall* Fn2i)(void*, void*, int, int);
typedef int(__fastcall* FnInt)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef void*(__fastcall* FnMake)(void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);

static void slot(char c, uint32_t v = 0) { if (g_recording) g_log[g_side].add(c, v); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token, g_shader_heap, g_light_heap, g_font_heap, g_image_heap;

// ---- owners -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct TestOwner : public AtGrD3DOwner {};
static TestOwner* g_own[2];

static void* value_of(const std::string& name, uint32_t salt) {
    uint32_t h = 0x811C9DC5 ^ salt;
    for (const char c : name) h = (h ^ (unsigned char)c) * 16777619;
    return (void*)(uintptr_t)(h | 0x1000);
}

// Owners that know the names given, as textures and materials, plainly and in each space; or nothing.
struct Env {
    std::vector<GmNameMap*> maps[2];
    Env(const std::set<std::string>& names, const std::set<std::string>& spaces) {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            void* p = std::calloc(1, sizeof(TestOwner));
            g_own[side] = new (p) TestOwner;
            TestOwner& w = *g_own[side];
            const unsigned pool = (unsigned)spaces.size() + 8;
            w.m_textureSpaces.Init(pool);
            w.m_materialSpaces.Init(pool);
            w.m_shaderSpaces.Init(pool);
            w.m_lightSpaces.Init(pool);
            w.m_matAssignmentSpaces.Init(pool);
            w.m_fontSpaces.Init(pool);
            w.m_imageSpaces.Init(pool);
            const auto fill = [&](uint32_t salt) {
                GmNameMap* m = new GmNameMap;
                m->Init((unsigned)names.size() + 1);
                for (const std::string& n : names) m->Insert(n.c_str(), value_of(n, salt));
                maps[side].push_back(m);
                return m;
            };
            w.m_textures.Add(fill(0));
            w.m_materials.Add(fill(1));
            w.m_vertexProcesses.Add(fill(4));
            w.m_pixelProcesses.Add(fill(5));
            for (const std::string& s : spaces) {
                w.m_textureSpaces.Add(s.c_str(), fill(2));
                w.m_materialSpaces.Add(s.c_str(), fill(3));
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

static int owner_kind(int side, const void* p) { return p == g_own[side] ? 1 : p ? 2 : 0; }

// ---- the device's doubles, the same on both sides -----------------------------------------------------------------------------------------------------------------------------------------------------------------

static uint32_t g_next_handle[2];
static unsigned g_loads[2];
static void* handle() { return (void*)(uintptr_t)(g_next_handle[g_side]++); }
static void* side_new(size_t n) { return g_side == 0 ? o_new((unsigned)n) : operator new(n); }

// A name to its terminator, at most eight bytes: past that it is whatever the database's buffer held, stack on each side.
static uint32_t name_hash(const char* name) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < 8 && name[i]; i++) h = (h ^ (unsigned char)name[i]) * 16777619;
    return h;
}

// What the original's slot 9 of a font or slot 10 of an image is taken to leave, as far as emptying and destroying can see.
static void mark_loaded(AtGrD3DTextureDatabase* textures, AtGrD3DSurface2D* surface, AtMaterialOwner* owner) {
    const unsigned n = g_loads[g_side]++;
    surface->m_surface = handle();
    surface->m_flags = (unsigned short)(n & 7);
    if (n % 3 == 1) surface->m_unknown44 = side_new(8);
    if (n % 2 == 0) {
        if (g_side == 0) at<Fn1>(0x0010F810)(textures, nullptr, owner);
        else textures->OnReady(owner);
    }
}

static void load_font(AtGrD3DFont2D* f, AtMaterialOwner* owner) {
    slot('L', name_hash(f->m_name));
    slot('l', owner == g_own[g_side]);
    slot('f', f->m_flags);
    slot('c', f->m_count);
    if (g_loads[g_side] % 3 == 2) f->m_flags |= AtGrD3DFont2D::kBorrowedChars;
    mark_loaded(&f->m_textures, &f->m_surface, owner);
}

static void load_image(AtGrD3DImage* i, AtMaterialOwner* owner) {
    slot('I', name_hash(i->m_name));
    slot('l', owner == g_own[g_side]);
    slot('f', i->m_flags);
    slot('g', i->m_format);
    mark_loaded(&i->m_textures, &i->m_surface, owner);
}

static void __fastcall o_load_font(AtGrD3DFont2D* f, void*, AtMaterialOwner* owner) { load_font(f, owner); }
static void __fastcall o_load_image(AtGrD3DImage* i, void*, AtMaterialOwner* owner) { load_image(i, owner); }
static void __stdcall o_before_release(void* r) { slot('B', (uint32_t)(uintptr_t)r); }
static void __stdcall o_release(void* r) { slot('X', (uint32_t)(uintptr_t)r); }
static void __fastcall o_release_stage(void*, void*, unsigned stage) { slot('G', stage); }

void AtGrD3DLoadFont2D(AtGrD3DFont2D* f, AtMaterialOwner* owner) { load_font(f, owner); }
void AtGrD3DLoadImage(AtGrD3DImage* i, AtMaterialOwner* owner) { load_image(i, owner); }
void AtGrD3DLoadTexture2D(AtGrD3DTexture2D*, AtMaterialOwner*) { slot('?', 1); }
void AtGrD3DLoadMaterial(AtGrD3DMaterial*, AtMaterialOwner*) { slot('?', 2); }
void AtGrD3DReleaseStage(void*, unsigned int stage) { slot('G', stage); }
void AtGrD3DBeforeRelease(void* r) { o_before_release(r); }
void AtGrD3DRelease(void* r) { o_release(r); }

// The seam of src/at/AtGrD3DSeam.cpp: logs.
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

static void __cdecl pure_call() { slot('!'); *(volatile int*)nullptr = 0; }

// ---- comparing ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

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

static void same_surface(const AtGrD3DSurface2D& a, const AtGrD3DSurface2D& b) {
    eq("surface's format", std::memcmp(&a.m_format, &b.m_format, sizeof a.m_format), 0);
    eq("surface's key", *(const uint32_t*)a.m_colorKey, *(const uint32_t*)b.m_colorKey);
    eq("surface's flags", a.m_flags, b.m_flags);
    eq("surface's +20", a.m_unknown20, b.m_unknown20);
    eq("surface's surface", (uintptr_t)a.m_surface, (uintptr_t)b.m_surface);
    eq("surface's palette", a.m_palette.m_count, b.m_palette.m_count);
    eq("surface's +44", a.m_unknown44 != nullptr, b.m_unknown44 != nullptr);
    eq("surface's +48", a.m_unknown48 != nullptr, b.m_unknown48 != nullptr);
    eq("surface's next", a.m_next != nullptr, b.m_next != nullptr);
}

// An item's own texture database: as far as anything here touches it.
static void same_own_textures(int what_o, int what_r, const AtGrD3DTextureDatabase& a, const AtGrD3DTextureDatabase& b) {
    eq("textures' owner", owner_kind(0, a.m_owner), owner_kind(1, b.m_owner));
    eq("textures' next", what_o, what_r);
    eq("textures' entries", a.m_entries != nullptr, b.m_entries != nullptr);
    eq("textures' counts", a.m_texture2DCount | a.m_surface2DCount << 8, b.m_texture2DCount | b.m_surface2DCount << 8);
    eq("textures' arrays", a.m_textures2D != nullptr || a.m_surfaces2D != nullptr, b.m_textures2D != nullptr || b.m_surfaces2D != nullptr);
}

// Which item's texture database p is, by index, in an array of items of type T.
template <class T> static long long textures_of(const T* items, unsigned count, const void* p) {
    if (!p) return -1;
    for (unsigned i = 0; items && i < count; i++) if (&items[i].m_textures == p) return i;
    return -2;
}

static unsigned array_count(const void* items) { return items ? ((const unsigned*)items)[-1] : 0xFFFFFFFF; }

// Shaders.
static void same_shader(const AtShader& a, const AtShader& b) {
    eq("shader's progs", a.m_progCount, b.m_progCount);
    eq("shader's stageds", a.m_stagedCount, b.m_stagedCount);
    eq("shader's transforms", std::memcmp(a.m_transforms, b.m_transforms, sizeof a.m_transforms), 0);
    eq("shader's transform count", a.m_transformCount, b.m_transformCount);
    eq("shader's prog array", a.m_progs != nullptr, b.m_progs != nullptr);
    eq("shader's staged array", a.m_stageds != nullptr, b.m_stageds != nullptr);
    if (a.m_stageds && b.m_stageds && a.m_stagedCount == b.m_stagedCount) eq("staged components", std::memcmp(a.m_stageds, b.m_stageds, a.m_stagedCount * sizeof(AtStagedShaderComponent)), 0);
    if (a.m_progs && b.m_progs && a.m_progCount == b.m_progCount) {
        for (unsigned i = 0; i < a.m_progCount; i++) {
            eq("prog component", std::memcmp(&a.m_progs[i], &b.m_progs[i], offsetof(AtProgShaderComponent, m_vertexProcesses)), 0);
            eq("prog's processes", a.m_progs[i].m_vertexProcesses != nullptr, b.m_progs[i].m_vertexProcesses != nullptr);
            eq("prog's pixel process", (uintptr_t)a.m_progs[i].m_pixelProcess, (uintptr_t)b.m_progs[i].m_pixelProcess);
        }
    }
}

static void same_shaderdb(const char* what, const AtGrD3DShaderDatabase* o, const AtGrD3DShaderDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("shaders", o->m_shaders != nullptr, r->m_shaders != nullptr);
    same_map(o, r, o->m_shaders, r->m_shaders);
    if (o->m_shaders && r->m_shaders) {
        eq("array count", array_count(o->m_shaders), array_count(r->m_shaders));
        if (o->m_count == r->m_count) for (unsigned i = 0; i < o->m_count; i++) same_shader(o->m_shaders[i], r->m_shaders[i]);
    }
    compare_logs(what);
}

static void same_lightdb(const char* what, const AtGrD3DLightDatabase* o, const AtGrD3DLightDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("owner", owner_kind(0, o->m_owner), owner_kind(1, r->m_owner));
    eq("lights", o->m_lights != nullptr, r->m_lights != nullptr);
    same_map(o, r, o->m_lights, r->m_lights);
    if (o->m_lights && r->m_lights) {
        eq("array count", array_count(o->m_lights), array_count(r->m_lights));
        if (o->m_count == r->m_count) {
            for (unsigned i = 0; i < o->m_count; i++) {
                eq("light's owner", owner_kind(0, o->m_lights[i].m_owner), owner_kind(1, r->m_lights[i].m_owner));
                eq("light", std::memcmp((const char*)&o->m_lights[i] + 8, (const char*)&r->m_lights[i] + 8, sizeof(AtGrD3DLight) - 8), 0);
            }
        }
    }
    compare_logs(what);
}

static AtGrD3DFontDatabase* g_fdb[2][3];
static bool g_compare_tables = true;  // off once FreeData may have freed a table and left the pointer

// Two items made by each side's constructor, as bytes, but for the tables of the members: the font's or image's own texture database, surface and the surface's palette.
static void same_bytes(const char* what, const void* o, const void* r, size_t size, std::initializer_list<size_t> tables) {
    std::vector<unsigned char> a((const unsigned char*)o, (const unsigned char*)o + size), b((const unsigned char*)r, (const unsigned char*)r + size);
    std::memset(a.data(), 0, 4);
    std::memset(b.data(), 0, 4);
    for (const size_t t : tables) {
        std::memset(a.data() + t, 0, 4);
        std::memset(b.data() + t, 0, 4);
    }
    eq(what, std::memcmp(a.data(), b.data(), size), 0);
}
static AtGrD3DImageDatabase* g_idb[2][3];
template <class D> static int db_index(D* (&dbs)[2][3], int side, const void* p) {
    if (!p) return -1;
    for (int i = 0; i < 3; i++) if (dbs[side][i] == p) return i;
    return 99;
}

static void same_font(const AtGrD3DFontDatabase* od, const AtGrD3DFontDatabase* rd, const AtGrD3DFont2D& a, const AtGrD3DFont2D& b) {
    eq("font's name", std::memcmp(a.m_name, b.m_name, strnlen(a.m_name, 8)), 0);
    eq("font's flags", a.m_flags, b.m_flags);
    eq("font's count", a.m_count, b.m_count);
    eq("font's +14", a.m_unknown14, b.m_unknown14);
    eq("font's database", db_index(g_fdb, 0, a.m_database), db_index(g_fdb, 1, b.m_database));
    eq("font's materials", a.m_materials == &od->m_materials ? 1 : a.m_materials ? 2 : 0, b.m_materials == &rd->m_materials ? 1 : b.m_materials ? 2 : 0);
    eq("font's reals", std::memcmp(&a.m_kerning, &b.m_kerning, 0x20), 0);
    eq("font's material", a.m_material, b.m_material);
    eq("font's +44", std::memcmp(a.m_unknown44, b.m_unknown44, sizeof a.m_unknown44), 0);
    eq("font's format", a.m_format, b.m_format);
    eq("font's chars", a.m_chars != nullptr, b.m_chars != nullptr);
    if (g_compare_tables && a.m_chars && b.m_chars && a.m_count == b.m_count) eq("font's table", std::memcmp(a.m_chars, b.m_chars, a.m_count * sizeof(AtGrI2DFontChar)), 0);
    same_own_textures(0, 0, a.m_textures, b.m_textures);
    eq("font's textures' next", textures_of(od->m_fonts, od->m_count, a.m_textures.m_next), textures_of(rd->m_fonts, rd->m_count, b.m_textures.m_next));
    same_surface(a.m_surface, b.m_surface);
}

static void same_fontdb(const char* what, const AtGrD3DFontDatabase* o, const AtGrD3DFontDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("materials", o->m_materials.m_count, r->m_materials.m_count);
    eq("file system", o->m_fileSystem != nullptr, r->m_fileSystem != nullptr);
    eq("directory", o->m_directory != nullptr, r->m_directory != nullptr);
    eq("owner", owner_kind(0, o->m_owner), owner_kind(1, r->m_owner));
    eq("binary", o->m_binary != nullptr, r->m_binary != nullptr);
    eq("+34", o->m_unknown34, r->m_unknown34);
    eq("next", db_index(g_fdb, 0, o->m_next), db_index(g_fdb, 1, r->m_next));
    eq("fonts", o->m_fonts != nullptr, r->m_fonts != nullptr);
    same_map(o, r, o->m_fonts, r->m_fonts);
    if (o->m_fonts && r->m_fonts) {
        eq("array count", array_count(o->m_fonts), array_count(r->m_fonts));
        const unsigned n = std::min(array_count(o->m_fonts), array_count(r->m_fonts));
        for (unsigned i = 0; i < n && i < 64; i++) same_font(o, r, o->m_fonts[i], r->m_fonts[i]);
    }
    eq("owner's first", db_index(g_fdb, 0, g_own[0]->m_fontDatabases), db_index(g_fdb, 1, g_own[1]->m_fontDatabases));
    eq("owner's first textures", textures_of(o->m_fonts, array_count(o->m_fonts), g_own[0]->m_textureDatabases), textures_of(r->m_fonts, array_count(r->m_fonts), g_own[1]->m_textureDatabases));
    compare_logs(what);
}

static void same_image(const AtGrD3DImageDatabase* od, const AtGrD3DImageDatabase* rd, const AtGrD3DImage& a, const AtGrD3DImage& b) {
    eq("image's name", std::memcmp(a.m_name, b.m_name, strnlen(a.m_name, 8)), 0);
    eq("image's flags", a.m_flags, b.m_flags);
    eq("image's format", a.m_format, b.m_format);
    eq("image's +14", a.m_unknown14 | a.m_unknown16 << 16, b.m_unknown14 | b.m_unknown16 << 16);
    eq("image's materials", a.m_materials == &od->m_materials ? 1 : a.m_materials ? 2 : 0, b.m_materials == &rd->m_materials ? 1 : b.m_materials ? 2 : 0);
    eq("image's material", a.m_material | a.m_unknown1E << 16, b.m_material | b.m_unknown1E << 16);
    eq("image's rectangle", std::memcmp(&a.m_colorRectangle, &b.m_colorRectangle, sizeof a.m_colorRectangle), 0);
    eq("image's data", a.m_data != nullptr, b.m_data != nullptr);
    eq("image's database", db_index(g_idb, 0, a.m_database), db_index(g_idb, 1, b.m_database));
    eq("image's transparent", std::memcmp(&a.m_transparent, &b.m_transparent, 4), 0);
    eq("image's +3C", std::memcmp(a.m_unknown3C, b.m_unknown3C, sizeof a.m_unknown3C), 0);
    same_own_textures(0, 0, a.m_textures, b.m_textures);
    eq("image's textures' next", textures_of(od->m_images, od->m_count, a.m_textures.m_next), textures_of(rd->m_images, rd->m_count, b.m_textures.m_next));
    same_surface(a.m_surface, b.m_surface);
}

static void same_imagedb(const char* what, const AtGrD3DImageDatabase* o, const AtGrD3DImageDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("materials", o->m_materials.m_count, r->m_materials.m_count);
    eq("file system", o->m_fileSystem != nullptr, r->m_fileSystem != nullptr);
    eq("directory", o->m_directory != nullptr, r->m_directory != nullptr);
    eq("owner", owner_kind(0, o->m_owner), owner_kind(1, r->m_owner));
    eq("next", db_index(g_idb, 0, o->m_next), db_index(g_idb, 1, r->m_next));
    eq("images", o->m_images != nullptr, r->m_images != nullptr);
    same_map(o, r, o->m_images, r->m_images);
    if (o->m_images && r->m_images) {
        eq("array count", array_count(o->m_images), array_count(r->m_images));
        const unsigned n = std::min(array_count(o->m_images), array_count(r->m_images));
        for (unsigned i = 0; i < n && i < 256; i++) same_image(o, r, o->m_images[i], r->m_images[i]);
    }
    eq("owner's first", db_index(g_idb, 0, g_own[0]->m_imageDatabases), db_index(g_idb, 1, g_own[1]->m_imageDatabases));
    eq("owner's first textures", textures_of(o->m_images, array_count(o->m_images), g_own[0]->m_textureDatabases), textures_of(r->m_images, array_count(r->m_images), g_own[1]->m_textureDatabases));
    compare_logs(what);
}

// ---- making and destroying ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

template <class D> static D* make_db(int side, uint32_t ctor, uint32_t table, unsigned char fill) {
    void* p = std::malloc(sizeof(D));
    std::memset(p, fill, sizeof(D));
    if (side == 0) {
        eq("constructor returns this", (uintptr_t)at<FnMake>(ctor)(p, nullptr), (uintptr_t)p);
        eq("the original's vftable", *(uint32_t*)p, table);
        return (D*)p;
    }
    return new (p) D;
}

template <class D> static void destroy_db(D* o, D* r, uint32_t vecdel, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(vecdel)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~D(); }
    compare_logs(what);
    eq("left: entries", o->m_entries != nullptr, r->m_entries != nullptr);
    std::free(o);
    std::free(r);
}

static void reset_case() {
    g_next_handle[0] = g_next_handle[1] = 0x100;
    g_loads[0] = g_loads[1] = 0;
}

template <class D> static bool load_both(D* o, D* r, uint32_t va) {
    const bool oc = guarded([&] { On on(0); at<FnLoad>(va)(o, nullptr, g_own[0], "MODEL\\THING", 77); });
    g_recording = false;
    g_fail_alloc = false;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], "MODEL\\THING", 77); });
    g_recording = false;
    g_fail_alloc = false;
    g_fs->m_path.m_top = -1;
    eq("crashed", oc, rc);
    return oc || rc;
}

static void set_file(const std::vector<unsigned char>& data, const char* path) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, path);
}

static unsigned g_files[4], g_items[4], g_compared_items[4];

// ---- shaders ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static void run_ags(const std::vector<unsigned char>& data, const std::string& context, bool stats) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    set_file(data, "ART\\THING.AGS");
    g_fill = 0xCD;
    reset_case();
    AtGrD3DShaderDatabase* o = make_db<AtGrD3DShaderDatabase>(0, 0x00110260, 0x0028A720, 0xCD);
    AtGrD3DShaderDatabase* r = make_db<AtGrD3DShaderDatabase>(1, 0, 0, 0xCD);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtGrD3DShaderDatabase) - 4), 0);
    load_both(o, r, 0x0002B2E0);
    same_shaderdb("load", o, r);
    if (stats) { g_items[0] += r->m_count; g_compared_items[0] += r->m_count; }
    for (unsigned i : {0u, 1u, o->m_count ? o->m_count - 1 : 0u}) {
        void* a;
        void* b;
        { On on(0); a = at<FnAt>(0x00110220)(o, nullptr, i); }
        { On on(1); b = r->ShaderAt(i); }
        eq("shader at", index_of(o->m_shaders, a, 1 << 20), index_of(r->m_shaders, b, 1 << 20));
    }
    load_both(o, r, 0x0002B2E0);
    same_shaderdb("load again", o, r);
    { On on(0); at<Fn0>(0x00110280)(o, nullptr); at<Fn0>(0x00110280)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_shaderdb("clear", o, r);
    load_both(o, r, 0x0002B2E0);
    same_shaderdb("load after clear", o, r);
    destroy_db(o, r, 0x001124A0, "destroy");
}

// ---- lights -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static void run_agl(const std::vector<unsigned char>& data, const std::string& context, bool stats) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    set_file(data, "ART\\THING.AGL");
    g_fill = 0xCD;
    reset_case();
    AtGrD3DLightDatabase* o = make_db<AtGrD3DLightDatabase>(0, 0x001111B0, 0x0028A764, 0xCD);
    AtGrD3DLightDatabase* r = make_db<AtGrD3DLightDatabase>(1, 0, 0, 0xCD);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtGrD3DLightDatabase) - 4), 0);
    load_both(o, r, 0x0002AC10);
    same_lightdb("load", o, r);
    if (stats) { g_items[1] += r->m_count; g_compared_items[1] += r->m_count; }
    for (unsigned i : {0u, 1u, o->m_count ? o->m_count - 1 : 0u}) {
        void* a;
        void* b;
        { On on(0); a = at<FnAt>(0x00111190)(o, nullptr, i); }
        { On on(1); b = r->LightAt(i); }
        eq("light at", index_of(o->m_lights, a, 1 << 20), index_of(r->m_lights, b, 1 << 20));
    }
    // A light reset, as a second read of it would: through its table, which is the original's on that side.
    if (o->m_count && r->m_count) {
        { On on(0); (*(void(__fastcall**)(void*, void*))(*(void***)o->m_lights + 1))(o->m_lights, nullptr); }
        { On on(1); r->m_lights[0].Reset(); }
        same_lightdb("reset", o, r);
    }
    load_both(o, r, 0x0002AC10);
    same_lightdb("load again", o, r);
    { On on(0); at<Fn0>(0x001111D0)(o, nullptr); at<Fn0>(0x001111D0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_lightdb("clear", o, r);
    load_both(o, r, 0x0002AC10);
    same_lightdb("load after clear", o, r);
    destroy_db(o, r, 0x00112700, "destroy");
}

// ---- fonts --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Emptying and freeing the data of the first fonts, directly: loading does not reach them.
static void font_slots(AtGrD3DFontDatabase* o, AtGrD3DFontDatabase* r) {
    const unsigned n = std::min(std::min(o->m_count, r->m_count), 3u);
    for (unsigned i = 0; i < n && o->m_fonts && r->m_fonts; i++) {
        AtGrD3DFont2D* a = &o->m_fonts[i];
        AtGrD3DFont2D* b = &r->m_fonts[i];
        if (i == 0) { a->m_flags |= AtFont::kOwnsData; b->m_flags |= AtFont::kOwnsData; }
        if (i == 1) { a->m_flags &= ~AtFont::kOwnsData; b->m_flags &= ~AtFont::kOwnsData; }
        const bool owned = (a->m_flags & AtFont::kOwnsData) != 0;
        { On on(0); at<Fn0>(0x001238A0)(a, nullptr); }
        { On on(1); b->FreeData(); }
        g_compare_tables = false;
        same_fontdb("free data", o, r);
        if (owned) a->m_chars = b->m_chars = nullptr;
        if (i != 2) {
            if (!a->m_textures.m_owner && !b->m_textures.m_owner) {
                { On on(0); at<Fn1>(0x0010F810)(&a->m_textures, nullptr, g_own[0]); }
                { On on(1); b->m_textures.OnReady(g_own[1]); }
            }
            { On on(0); at<Fn0>(0x00123710)(a, nullptr); }
            { On on(1); b->Free(); }
            same_fontdb("free", o, r);
        }
    }
}

static void run_agf(const std::vector<unsigned char>& data, const std::string& context, bool stats) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    set_file(data, "UI\\FONTS\\THING.AGF");
    g_fill = 0xCD;
    reset_case();
    AtGrD3DFontDatabase* o = make_db<AtGrD3DFontDatabase>(0, 0x001115A0, 0x0028A774, 0xCD);
    AtGrD3DFontDatabase* r = make_db<AtGrD3DFontDatabase>(1, 0, 0, 0xCD);
    g_fdb[0][0] = o;
    g_fdb[1][0] = r;
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtGrD3DFontDatabase) - 4), 0);
    load_both(o, r, 0x0002A290);
    same_fontdb("load", o, r);
    if (stats) { g_items[2] += r->m_count; g_compared_items[2] += r->m_count; }
    for (unsigned i : {0u, 1u, o->m_count ? o->m_count - 1 : 0u}) {
        void* a;
        void* b;
        { On on(0); a = at<FnAt>(0x001113E0)(o, nullptr, i); }
        { On on(1); b = r->Font2DAt(i); }
        eq("font at", index_of(o->m_fonts, a, 1 << 20), index_of(r->m_fonts, b, 1 << 20));
    }
    {
        int a, b;
        { On on(0); a = at<FnInt>(0x001A2B40)(o, nullptr); }
        { On on(1); b = r->Slot4(); }
        eq("binary form", a, b);
    }
    font_slots(o, r);
    g_compare_tables = true;
    { On on(0); at<Fn0>(0x001115D0)(o, nullptr); at<Fn0>(0x001115D0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_fontdb("clear", o, r);
    load_both(o, r, 0x0002A290);
    same_fontdb("load after clear", o, r);
    destroy_db(o, r, 0x00112830, "destroy");
    g_fdb[0][0] = g_fdb[1][0] = nullptr;
}

// ---- images -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static void image_slots(AtGrD3DImageDatabase* o, AtGrD3DImageDatabase* r) {
    const unsigned n = std::min(std::min(o->m_count, r->m_count), 3u);
    for (unsigned i = 0; i < n && o->m_images && r->m_images; i++) {
        AtGrD3DImage* a = &o->m_images[i];
        AtGrD3DImage* b = &r->m_images[i];
        if (i == 1) {
            a->m_flags &= ~AtImage::kOwnsData;
            b->m_flags &= ~AtImage::kOwnsData;
        }
        if (i == 0) {
            a->m_flags |= AtImage::kOwnsData;
            b->m_flags |= AtImage::kOwnsData;
            { On on(0); a->m_data = o_new(4); }
            { On on(1); b->m_data = operator new(4); }
        }
        { On on(0); at<Fn0>(0x00124730)(a, nullptr); }
        { On on(1); b->FreeData(); }
        same_imagedb("free data", o, r);
        if (i != 2) {
            if (!a->m_textures.m_owner && !b->m_textures.m_owner) {
                { On on(0); at<Fn1>(0x0010F810)(&a->m_textures, nullptr, g_own[0]); }
                { On on(1); b->m_textures.OnReady(g_own[1]); }
            }
            { On on(0); at<Fn0>(0x001245E0)(a, nullptr); }
            { On on(1); b->Free(); }
            same_imagedb("free", o, r);
        }
    }
}

static void run_agi(const std::vector<unsigned char>& data, const std::string& context, bool stats) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    set_file(data, "UI\\IMAGES\\THING.AGI");
    g_fill = 0xCD;
    reset_case();
    AtGrD3DImageDatabase* o = make_db<AtGrD3DImageDatabase>(0, 0x00111BC0, 0x0028A794, 0xCD);
    AtGrD3DImageDatabase* r = make_db<AtGrD3DImageDatabase>(1, 0, 0, 0xCD);
    g_idb[0][0] = o;
    g_idb[1][0] = r;
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtGrD3DImageDatabase) - 4), 0);
    load_both(o, r, 0x00029DC0);
    same_imagedb("load", o, r);
    if (stats) { g_items[3] += r->m_count; g_compared_items[3] += std::min(r->m_count, 256u); }
    for (unsigned i : {0u, 1u, o->m_count ? o->m_count - 1 : 0u}) {
        void* a;
        void* b;
        { On on(0); a = at<FnAt>(0x00111A00)(o, nullptr, i); }
        { On on(1); b = r->ImageAt(i); }
        eq("image at", index_of(o->m_images, a, 1 << 20), index_of(r->m_images, b, 1 << 20));
    }
    image_slots(o, r);
    { On on(0); at<Fn0>(0x00111BE0)(o, nullptr); at<Fn0>(0x00111BE0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_imagedb("clear", o, r);
    load_both(o, r, 0x00029DC0);
    same_imagedb("load after clear", o, r);
    destroy_db(o, r, 0x00112960, "destroy");
    g_idb[0][0] = g_idb[1][0] = nullptr;
}

// ---- written texts ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static std::vector<unsigned char> bytes(const char* s) { return std::vector<unsigned char>(s, s + std::strlen(s)); }

static void run_written() {
    // Shaders: one, several, a name space, staged components with textures known and not, a prog component.
    static const char* const ags[] = {
        "Shader[1]\n{\n Shader \"One\"\n {\n }\n}\n",
        "Shader[3]\n{\n Shader \"A\"\n {\n  StagedShaderComponent[1]\n  {\n   StagedShaderComponent\n   {\n    TextureStage\n    {\n     Texture \"Tex\"\n     Bilinear\n     Coord2DTransform 3\n    }\n    ModulateOp ModulateTextureByDiffuse\n   }\n  }\n }\n"
        " Shader \"B\"\n {\n  StagedShaderComponent[2]\n  {\n   StagedShaderComponent\n   {\n    TextureStage\n    {\n     Texture \"Gone\"\n     Coord3DTransform 1\n    }\n   }\n   StagedShaderComponent\n   {\n    CombineOp Add\n    AlphaSource SelectConstant\n   }\n  }\n }\n"
        " Shader \"C\"\n {\n }\n}\n",
        "NameSpace \"Space\"\nShader[2]\n{\n Shader \"S1\"\n {\n }\n Shader \"S2\"\n {\n  ProgShaderComponent[1]\n  {\n   ProgShaderComponent\n   {\n    TextureStage[1]\n    {\n     TextureStage\n     {\n      Texture \"Tex\"\n     }\n    }\n   }\n  }\n }\n}\n",
    };
    for (const char* t : ags) run_ags(bytes(t), std::string("written .AGS: ") + t, false);
    // Lights: one with every word, several, a name space.
    static const char* const agl[] = {
        "Light[1]\n{\n Light \"L\"\n {\n  AmbientColor 0.1 0.2 0.3 0.4\n  DiffuseColor 0.5 0.6 0.7 0.8\n  SpecularColor 0.9 1.0 0.25 0.5\n  Range 50.5\n  Attenuation 0.5 0.25 0.125\n  InnerConeAngle 0.75\n  OuterConeAngle 1.5\n }\n}\n",
        "NameSpace \"Space\"\nLight[3]\n{\n Light \"A\"\n {\n }\n Light \"B\"\n {\n  Range 2.0\n }\n Light \"C\"\n {\n  DiffuseColor 1.0 0.0 0.0 1.0\n }\n}\n",
    };
    for (const char* t : agl) run_agl(bytes(t), std::string("written .AGL: ") + t, false);
    // Fonts: demand loading, several, a name space, a character set, owning data.
    static const char* const agf[] = {
        "Font[2]\nFont2D[2]\n{\n Font2D \"A\"\n {\n  TgaSource\n  CharacterSet[3]\n  {\n   \"a\" \"b\" 32\n  }\n }\n Font2D \"B\"\n {\n  DemandLoad\n  Kerning 1.5\n }\n}\n",
        "NameSpace \"Space\"\nFont[3]\nMatAssignment[1]\n{\n \"alpha\"\n}\nFont2D[3]\n{\n Font2D \"X1\"\n {\n  MatAssignment 0\n  CharacterSet[2]\n  {\n   \"A\" \"B\"\n  }\n }\n Font2D \"X2\"\n {\n  Leading 2.0\n }\n"
        " Font2D \"X3\"\n {\n  Scale 2.0 3.0\n  CharacterSet[1]\n  {\n   10\n  }\n }\n}\n",
    };
    for (const char* t : agf) run_agf(bytes(t), std::string("written .AGF: ") + t, false);
    // Images: demand loading, a name space, every source.
    static const char* const agi[] = {
        "Image[3]\n{\n Image \"A\"\n {\n  TgaSource\n }\n Image \"B\"\n {\n  DemandLoad\n  DdsSource\n }\n Image \"C\"\n {\n  PlatformSource\n  FlipTextureVertically\n }\n}\n",
        "NameSpace \"Space\"\nMatAssignment[1]\n{\n \"alpha\"\n}\nImage[2]\n{\n Image \"I1\"\n {\n  MatAssignment 0\n  TransparentColor 1 2 3\n }\n Image \"I2\"\n {\n  BmpSource\n  FrequentModify\n }\n}\n",
    };
    for (const char* t : agi) run_agi(bytes(t), std::string("written .AGI: ") + t, false);
}

// ---- direct cases -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// The owner's lists of font and image databases, in each order of removal, and a removal of one not there.
template <class D> static void list_case(uint32_t add, uint32_t remove, D* (&dbs)[2][3], D* AtGrD3DOwner::*head, void (AtGrD3DOwner::*add_r)(D*), void (AtGrD3DOwner::*remove_r)(D*), const char* what) {
    static const int orders[4][3] = {{0, 1, 2}, {2, 1, 0}, {1, 0, 2}, {1, 2, 0}};
    for (const auto& order : orders) {
        g_context = std::string("list of ") + what;
        D* made[2][3];
        for (int side = 0; side < 2; side++) for (int k = 0; k < 3; k++) {
            made[side][k] = (D*)std::calloc(1, sizeof(D));
            dbs[side][k] = made[side][k];
        }
        for (int k = 0; k < 3; k++) {
            { On on(0); at<Fn1>(add)(g_own[0], nullptr, made[0][k]); }
            { On on(1); (g_own[1]->*add_r)(made[1][k]); }
        }
        eq("first", db_index(dbs, 0, g_own[0]->*head), db_index(dbs, 1, g_own[1]->*head));
        D* stray[2] = {(D*)std::calloc(1, sizeof(D)), (D*)std::calloc(1, sizeof(D))};
        { On on(0); at<Fn1>(remove)(g_own[0], nullptr, stray[0]); }
        { On on(1); (g_own[1]->*remove_r)(stray[1]); }
        for (int k : order) {
            { On on(0); at<Fn1>(remove)(g_own[0], nullptr, made[0][k]); }
            { On on(1); (g_own[1]->*remove_r)(made[1][k]); }
            eq("first", db_index(dbs, 0, g_own[0]->*head), db_index(dbs, 1, g_own[1]->*head));
            for (int j = 0; j < 3; j++) eq("next", db_index(dbs, 0, made[0][j]->m_next), db_index(dbs, 1, made[1][j]->m_next));
        }
        compare_logs("list");
        for (int side = 0; side < 2; side++) {
            for (int k = 0; k < 3; k++) { std::free(made[side][k]); dbs[side][k] = nullptr; }
            std::free(stray[side]);
        }
    }
}

// Each Make with no heap, then with no memory, and the database then cleared and destroyed.
template <class D> static void make_without(int fail, uint32_t ctor, uint32_t table, uint32_t make, bool with_owner, void (*make_r)(D*), void (*clear_r)(D*), uint32_t clear, uint32_t vecdel, const char* what) {
    g_context = std::string(fail ? "no memory: " : "no heap: ") + what;
    D* o = make_db<D>(0, ctor, table, 0xCD);
    D* r = make_db<D>(1, 0, 0, 0xCD);
    *(unsigned*)((char*)o + (with_owner ? 0x1C : 0x14)) = 2;
    *(unsigned*)((char*)r + (with_owner ? 0x1C : 0x14)) = 2;
    g_fail_alloc = fail != 0;
    const bool oc = guarded([&] { On on(0); if (with_owner) at<Fn1>(make)(o, nullptr, g_own[0]); else at<Fn0>(make)(o, nullptr); });
    g_recording = false;
    g_fail_alloc = fail != 0;
    const bool rc = guarded([&] { On on(1); make_r(r); });
    g_recording = false;
    g_fail_alloc = false;
    eq("crashed", oc, rc);
    compare_logs("make");
    if (with_owner) eq("owner kept", owner_kind(0, *(void**)((char*)o + 0x1C)), owner_kind(1, *(void**)((char*)r + 0x1C)));
    { On on(0); at<Fn0>(clear)(o, nullptr); }
    { On on(1); clear_r(r); }
    compare_logs("clear");
    destroy_db(o, r, vecdel, "destroy");
}

static void run_direct() {
    std::set<std::string> none;
    Env env(none, none);
    reset_case();
    g_fill = 0xCD;
    list_case<AtGrD3DFontDatabase>(0x000EF190, 0x000EF1B0, g_fdb, &AtGrD3DOwner::m_fontDatabases, &AtGrD3DOwner::AddFontDatabase, &AtGrD3DOwner::RemoveFontDatabase, "fonts");
    list_case<AtGrD3DImageDatabase>(0x000EF1E0, 0x000EF200, g_idb, &AtGrD3DOwner::m_imageDatabases, &AtGrD3DOwner::AddImageDatabase, &AtGrD3DOwner::RemoveImageDatabase, "images");

    // Two font and two image databases ready on one owner, cleared in each order: each goes off the list, and the other stays.
    for (int first = 0; first < 2; first++) {
        g_context = "two databases";
        AtGrD3DFontDatabase* fo[2];
        AtGrD3DFontDatabase* fr[2];
        AtGrD3DImageDatabase* io[2];
        AtGrD3DImageDatabase* ir[2];
        for (int k = 0; k < 2; k++) {
            g_fdb[0][k] = fo[k] = make_db<AtGrD3DFontDatabase>(0, 0x001115A0, 0x0028A774, 0xCD);
            g_fdb[1][k] = fr[k] = make_db<AtGrD3DFontDatabase>(1, 0, 0, 0xCD);
            g_idb[0][k] = io[k] = make_db<AtGrD3DImageDatabase>(0, 0x00111BC0, 0x0028A794, 0xCD);
            g_idb[1][k] = ir[k] = make_db<AtGrD3DImageDatabase>(1, 0, 0, 0xCD);
            { On on(0); at<Fn1>(0x00111400)(fo[k], nullptr, g_own[0]); at<Fn1>(0x00111A20)(io[k], nullptr, g_own[0]); }
            { On on(1); fr[k]->OnReady(g_own[1]); ir[k]->OnReady(g_own[1]); }
        }
        same_fontdb("ready", fo[0], fr[0]);
        same_fontdb("ready", fo[1], fr[1]);
        same_imagedb("ready", io[0], ir[0]);
        same_imagedb("ready", io[1], ir[1]);
        for (int j = 0; j < 2; j++) {
            const int k = j ^ first;
            { On on(0); at<Fn0>(0x001115D0)(fo[k], nullptr); at<Fn0>(0x00111BE0)(io[k], nullptr); }
            { On on(1); fr[k]->Clear(); ir[k]->Clear(); }
            same_fontdb("cleared", fo[0], fr[0]);
            same_fontdb("cleared", fo[1], fr[1]);
            same_imagedb("cleared", io[0], ir[0]);
            same_imagedb("cleared", io[1], ir[1]);
        }
        for (int k = 0; k < 2; k++) {
            destroy_db(fo[k], fr[k], 0x00112830, "destroy fonts");
            destroy_db(io[k], ir[k], 0x00112960, "destroy images");
        }
        for (int side = 0; side < 2; side++) for (int k = 0; k < 3; k++) g_fdb[side][k] = nullptr, g_idb[side][k] = nullptr;
    }

    // A font database holding a binary block and a +34, cleared: the block is freed, and both go to 0.
    {
        g_context = "binary block";
        AtGrD3DFontDatabase* o = make_db<AtGrD3DFontDatabase>(0, 0x001115A0, 0x0028A774, 0xCD);
        AtGrD3DFontDatabase* r = make_db<AtGrD3DFontDatabase>(1, 0, 0, 0xCD);
        g_fdb[0][0] = o;
        g_fdb[1][0] = r;
        { On on(0); o->m_binary = o_new(16); }
        { On on(1); r->m_binary = operator new(16); }
        o->m_unknown34 = r->m_unknown34 = 0x34;
        same_fontdb("held", o, r);
        { On on(0); at<Fn0>(0x001115D0)(o, nullptr); }
        { On on(1); r->Clear(); }
        same_fontdb("cleared", o, r);
        destroy_db(o, r, 0x00112830, "destroy");
        g_fdb[0][0] = g_fdb[1][0] = nullptr;
    }

    // Each kind's array of 0, 1 and 3, made, cleared and destroyed; fonts and images made with a borrowed table or blocks to free.
    for (unsigned count : {0u, 1u, 3u}) {
        g_context = "arrays of " + std::to_string(count);
        {
            AtGrD3DShaderDatabase* o = make_db<AtGrD3DShaderDatabase>(0, 0x00110260, 0x0028A720, 0xCD);
            AtGrD3DShaderDatabase* r = make_db<AtGrD3DShaderDatabase>(1, 0, 0, 0xCD);
            o->m_count = r->m_count = count;
            { On on(0); at<Fn0>(0x001102D0)(o, nullptr); }
            { On on(1); r->OnReady(); }
            same_shaderdb("shaders made", o, r);
            destroy_db(o, r, 0x001124A0, "shaders destroyed");
        }
        {
            AtGrD3DLightDatabase* o = make_db<AtGrD3DLightDatabase>(0, 0x001111B0, 0x0028A764, 0xCD);
            AtGrD3DLightDatabase* r = make_db<AtGrD3DLightDatabase>(1, 0, 0, 0xCD);
            o->m_count = r->m_count = count;
            { On on(0); at<Fn1>(0x00111210)(o, nullptr, g_own[0]); }
            { On on(1); r->MakeLights(g_own[1]); }
            same_lightdb("lights made", o, r);
            destroy_db(o, r, 0x00112700, "lights destroyed");
        }
        {
            AtGrD3DFontDatabase* o = make_db<AtGrD3DFontDatabase>(0, 0x001115A0, 0x0028A774, 0xCD);
            AtGrD3DFontDatabase* r = make_db<AtGrD3DFontDatabase>(1, 0, 0, 0xCD);
            g_fdb[0][0] = o;
            g_fdb[1][0] = r;
            o->m_count = r->m_count = count;
            { On on(0); at<Fn1>(0x00111400)(o, nullptr, g_own[0]); at<Fn0>(0x00111420)(o, nullptr); }
            { On on(1); r->OnReady(g_own[1]); r->MakeFonts2D(); }
            for (unsigned i = 0; i < count; i++) {
                for (int side = 0; side < 2; side++) {
                    On on(side);
                    AtGrD3DFont2D* f = side ? &r->m_fonts[i] : &o->m_fonts[i];
                    f->m_count = 2;
                    f->m_chars = (AtGrI2DFontChar*)side_new(32);
                    if (i == 1) f->m_flags = AtGrD3DFont2D::kBorrowedChars;
                    load_font(f, g_own[side]);
                }
            }
            same_fontdb("fonts made", o, r);
            destroy_db(o, r, 0x00112830, "fonts destroyed");
            g_fdb[0][0] = g_fdb[1][0] = nullptr;
        }
        {
            AtGrD3DImageDatabase* o = make_db<AtGrD3DImageDatabase>(0, 0x00111BC0, 0x0028A794, 0xCD);
            AtGrD3DImageDatabase* r = make_db<AtGrD3DImageDatabase>(1, 0, 0, 0xCD);
            g_idb[0][0] = o;
            g_idb[1][0] = r;
            o->m_count = r->m_count = count;
            { On on(0); at<Fn0>(0x00111A40)(o, nullptr); at<Fn1>(0x00111A20)(o, nullptr, g_own[0]); }
            { On on(1); r->MakeItems(); r->OnReady(g_own[1]); }
            for (unsigned i = 0; i < count; i++) {
                for (int side = 0; side < 2; side++) {
                    On on(side);
                    AtGrD3DImage* m = side ? &r->m_images[i] : &o->m_images[i];
                    m->m_data = side_new(12);
                    load_image(m, g_own[side]);
                }
            }
            same_imagedb("images made", o, r);
            destroy_db(o, r, 0x00112960, "images destroyed");
            g_idb[0][0] = g_idb[1][0] = nullptr;
        }
    }

    // A font and an image made and destroyed by themselves, through their own slot 0 or 5 with and without delete.
    for (unsigned flags : {0u, 1u}) {
        g_context = "items alone " + std::to_string(flags);
        void* fo = std::malloc(sizeof(AtGrD3DFont2D));
        void* fr = std::malloc(sizeof(AtGrD3DFont2D));
        std::memset(fo, 0xCD, sizeof(AtGrD3DFont2D));
        std::memset(fr, 0xCD, sizeof(AtGrD3DFont2D));
        { On on(0); at<FnMake>(0x001236B0)(fo, nullptr); }
        { On on(1); new (fr) AtGrD3DFont2D; }
        same_bytes("font made", fo, fr, sizeof(AtGrD3DFont2D), {0x64, 0xBC, 0xE8});
        eq("font's table", *(uint32_t*)fo, 0x0028B970);
        compare_logs("font made");
        if (flags) {
            // A deleting destroy frees the block it was given: give each side a block of its own heap.
            std::free(fo);
            std::free(fr);
            { On on(0); fo = o_new(sizeof(AtGrD3DFont2D)); at<FnMake>(0x001236B0)(fo, nullptr); }
            { On on(1); fr = new AtGrD3DFont2D; }
            compare_logs("font made on the heap");
            { On on(0); at<FnDel>(0x001114E0)(fo, nullptr, 1); }
            { On on(1); delete (AtGrD3DFont2D*)fr; }
            compare_logs("font deleted");
        } else {
            { On on(0); at<Fn0>(0x00123770)(fo, nullptr); }
            { On on(1); ((AtGrD3DFont2D*)fr)->~AtGrD3DFont2D(); }
            compare_logs("font destroyed");
            same_bytes("font left", fo, fr, sizeof(AtGrD3DFont2D), {0x64, 0xBC, 0xE8});
            std::free(fo);
            std::free(fr);
        }
        void* io = std::malloc(sizeof(AtGrD3DImage));
        void* ir = std::malloc(sizeof(AtGrD3DImage));
        std::memset(io, 0xCD, sizeof(AtGrD3DImage));
        std::memset(ir, 0xCD, sizeof(AtGrD3DImage));
        { On on(0); at<FnMake>(0x00124580)(io, nullptr); }
        { On on(1); new (ir) AtGrD3DImage; }
        same_bytes("image made", io, ir, sizeof(AtGrD3DImage), {0x48, 0xA0, 0xCC});
        eq("image's table", *(uint32_t*)io, 0x0028B9B8);
        compare_logs("image made");
        { On on(0); at<Fn0>(0x00124660)(io, nullptr); }
        { On on(1); ((AtGrD3DImage*)ir)->~AtGrD3DImage(); }
        compare_logs("image destroyed");
        same_bytes("image left", io, ir, sizeof(AtGrD3DImage), {0x48, 0xA0, 0xCC});
        std::free(io);
        std::free(ir);
        // A light and a shader alone.
        unsigned char lo[sizeof(AtGrD3DLight)], lr[sizeof(AtGrD3DLight)];
        std::memset(lo, 0xCD, sizeof lo);
        std::memset(lr, 0xCD, sizeof lr);
        { On on(0); at<FnMake>(0x000EB7D0)(lo, nullptr); }
        { On on(1); new (lr) AtGrD3DLight; }
        eq("light made", std::memcmp(lo + 4, lr + 4, sizeof lo - 4), 0);
        eq("light's table", *(uint32_t*)lo, 0x00288B88);
        std::memset(lo + 4, 0xAB, sizeof lo - 4);
        std::memset(lr + 4, 0xAB, sizeof lr - 4);
        { On on(0); at<Fn0>(0x000369A0)(lo, nullptr); }
        { On on(1); ((AtGrD3DLight*)lr)->Reset(); }
        eq("light reset", std::memcmp(lo + 4, lr + 4, sizeof lo - 4), 0);
        std::memset(lo + 4, 0xAB, sizeof lo - 4);
        std::memset(lr + 4, 0xAB, sizeof lr - 4);
        { On on(0); at<Fn0>(0x000EBFD0)(lo, nullptr); }
        { On on(1); ((AtGrD3DLight*)lr)->~AtGrD3DLight(); }
        eq("light destroyed", std::memcmp(lo, lr, 4) == 0 ? 0 : (*(uint32_t*)lo == 0x00274C78 ? 0 : 1), 0);
        eq("light left", std::memcmp(lo + 4, lr + 4, sizeof lo - 4), 0);
        unsigned char so[sizeof(AtGrD3DShader)], sr[sizeof(AtGrD3DShader)];
        std::memset(so, 0xCD, sizeof so);
        std::memset(sr, 0xCD, sizeof sr);
        { On on(0); at<FnMake>(0x00110230)(so, nullptr); }
        { On on(1); new (sr) AtGrD3DShader; }
        eq("shader made", std::memcmp(so + 4, sr + 4, sizeof so - 4), 0);
        eq("shader's table", *(uint32_t*)so, 0x0028A718);
        { On on(0); at<Fn0>(0x00110250)(so, nullptr); }
        { On on(1); ((AtGrD3DShader*)sr)->~AtGrD3DShader(); }
        eq("shader left", std::memcmp(so + 4, sr + 4, sizeof so - 4), 0);
        compare_logs("light and shader");
    }

    // No heap, then no memory, for each kind's array.
    for (int fail = 0; fail < 2; fail++) {
        if (!fail) {
            for (uint32_t va : {0x002C6380u, 0x002C6378u, 0x002C6370u, 0x002C6364u}) *(void**)(uintptr_t)va = nullptr;
            AtShaderDatabase::s_heap = nullptr;
            AtLightDatabase::s_heap = nullptr;
            AtFontDatabase::s_heap = nullptr;
            AtImageDatabase::s_heap = nullptr;
        }
        make_without<AtGrD3DShaderDatabase>(fail, 0x00110260, 0x0028A720, 0x001102D0, false, [](AtGrD3DShaderDatabase* d) { d->OnReady(); }, [](AtGrD3DShaderDatabase* d) { d->Clear(); }, 0x00110280, 0x001124A0, "shaders");
        make_without<AtGrD3DLightDatabase>(fail, 0x001111B0, 0x0028A764, 0x00111210, true, [](AtGrD3DLightDatabase* d) { d->MakeLights(g_own[1]); }, [](AtGrD3DLightDatabase* d) { d->Clear(); }, 0x001111D0, 0x00112700, "lights");
        make_without<AtGrD3DFontDatabase>(fail, 0x001115A0, 0x0028A774, 0x00111420, false, [](AtGrD3DFontDatabase* d) { d->MakeFonts2D(); }, [](AtGrD3DFontDatabase* d) { d->Clear(); }, 0x001115D0, 0x00112830, "fonts");
        make_without<AtGrD3DImageDatabase>(fail, 0x00111BC0, 0x0028A794, 0x00111A40, false, [](AtGrD3DImageDatabase* d) { d->MakeItems(); }, [](AtGrD3DImageDatabase* d) { d->Clear(); }, 0x00111BE0, 0x00112960, "images");
        if (!fail) {
            at<void(__cdecl*)(void*)>(0x0002B1B0)(&g_shader_heap);
            AtShaderDatabase::SetHeap(&g_shader_heap);
            at<void(__cdecl*)(void*)>(0x0002AAF0)(&g_light_heap);
            AtLightDatabase::SetHeap(&g_light_heap);
            eq("light heap", (uintptr_t)*(void**)(uintptr_t)0x002C6378, (uintptr_t)AtLightDatabase::s_heap);
            *(void**)(uintptr_t)0x002C6370 = &g_font_heap;
            AtFontDatabase::s_heap = &g_font_heap;
            *(void**)(uintptr_t)0x002C6364 = &g_image_heap;
            AtImageDatabase::s_heap = &g_image_heap;
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
    if (pos.size() != 2) { std::printf("usage: oracle_xshade <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00123850, (const void*)o_load_font);
    redirect(0x00124CD0, (const void*)o_load_image);
    redirect(0x000FB630, (const void*)o_release_stage);
    redirect(0x00205E20, (const void*)o_before_release);
    redirect(0x00205D10, (const void*)o_release);
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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C62E4u, 0x002C63F0u, 0x002C63E0u, 0x002C63ECu, 0x002C7C44u, 0x003B1FCCu}) *(void**)(uintptr_t)va = &g_heap_token;
    *(void**)(uintptr_t)0x002C6380 = &g_shader_heap;
    *(void**)(uintptr_t)0x002C6378 = &g_light_heap;
    *(void**)(uintptr_t)0x002C6370 = &g_font_heap;
    *(void**)(uintptr_t)0x002C6364 = &g_image_heap;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtTextureDatabase::s_heap = &g_heap_token;
    AtMaterialDatabase::s_heap = &g_heap_token;
    AtMatAssignment::s_heap = &g_heap_token;
    AtShader::s_heap = &g_heap_token;
    AtProgShaderComponent::s_heap = &g_heap_token;
    AtGrI2DFont::s_heap = &g_heap_token;
    AtShaderDatabase::s_heap = &g_shader_heap;
    AtLightDatabase::s_heap = &g_light_heap;
    AtFontDatabase::s_heap = &g_font_heap;
    AtImageDatabase::s_heap = &g_image_heap;
    g_heap_pad = 0x100;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_direct();
    run_written();

    const std::string dir = pos[1];
    if (!read_index(dir, ".AGS\t", [](const std::vector<unsigned char>& d, const std::string& name) { run_ags(d, name, true); g_files[0]++; })) return 2;
    if (!read_index(dir, ".AGL\t", [](const std::vector<unsigned char>& d, const std::string& name) { run_agl(d, name, true); g_files[1]++; })) return 2;
    if (!read_index(dir, ".AGF\t", [](const std::vector<unsigned char>& d, const std::string& name) { run_agf(d, name, true); g_files[2]++; })) return 2;
    if (!read_index(dir, ".AGI\t", [](const std::vector<unsigned char>& d, const std::string& name) { run_agi(d, name, true); g_files[3]++; })) return 2;
    report_entries();
    std::printf("%s: %u .AGS (%u shaders), %u .AGL (%u lights), %u .AGF (%u fonts), %u .AGI (%u images, %u compared item by item); %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED",
                g_files[0], g_items[0], g_files[1], g_items[1], g_files[2], g_items[2], g_files[3], g_items[3], g_compared_items[3], g_checks, g_failures);
    return g_failures ? 1 : 0;
}
