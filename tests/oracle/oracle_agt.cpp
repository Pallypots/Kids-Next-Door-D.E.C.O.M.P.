// Differential oracle for the texture database (Windows x86, local only). See docs/verification.md.
//
//   oracle_agt <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AGT file listed in index.txt, loaded through the file code on both sides, with the owner knowing none, all, some of the names, or knowing them only in one of its two tables; and text
// written here. What an item does is the graphics layer's, so a test item stands for it on both sides: it reads its body from the side's own tokenizer and logs every token, the words DemandLoad and
// MipMap being the only ones it acts on. Slots 2 to 13 of the database are the harness's too, and log.
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

#include "at/AtIoTokenizer.h"
#include "at/AtTextureDatabase.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static bool g_fail_in_item;   // Heap requests fail from the moment an item is read: what is left to ask for then is Load's copy of the path.
static unsigned g_items, g_demand, g_surfaces;

enum { kPool = 2048, kDbs = 5, kKinds = 3, kWordDemandLoad = 0x55, kWordMipMap = 0x42, kWordNamesItself = 0x4E };

static unsigned char g_pool[2][kDbs][kKinds][kPool][0x40];
static void* g_db[2][kDbs];
static AtMaterialOwner* g_own[2];

static int which(int side, const void* self) {
    for (int i = 1; i < kDbs; i++) if (self == g_db[side][i]) return i;
    return 0;
}

class TestItem : public AtTextureItem {
public:
    int side, kind, index;
    void Slot0() override {}
    void Slot1() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Read(AtTextureDatabase* database, const char* name, AtIoTokenizer* tokenizer) override {
        Log& log = g_log[side];
        log.add('I', (kind << 16) | index);
        log.add('i', which(side, database));
        log.add('n', *(const uint32_t*)name);
        log.add('o', *(const uint32_t*)(name + 4));
        log.add('t', g_fs->m_path.m_top);
        if (const char* current = g_fs->m_path.Current()) record_message(current, side);
        if (g_fail_in_item) g_fail_alloc = true;
        m_flags = 0;
        m_levels = 1;
        int token = tokenizer->Next();
        log.add('b', token);
        if (token != AtIoTokenizer::kLeftCurly) return;
        for (int depth = 1; depth > 0;) {
            token = tokenizer->Next();
            log.add('b', token);
            if (token == AtIoTokenizer::kEnd) return;
            if (token == AtIoTokenizer::kLeftCurly) depth++;
            if (token == AtIoTokenizer::kRightCurly) depth--;
            if (token == kWordDemandLoad) m_flags |= kDemandLoad;
            // On no real file. The item names itself while it reads: the reader must look the name up after the item has read, so it finds it, and does not insert it twice.
            if (token == kWordNamesItself) database->Insert(name, this);
            if (token == kWordMipMap) {
                token = tokenizer->Next();
                log.add('b', token);
                m_levels = (unsigned char)tokenizer->m_int;
            }
        }
    }
    void OnLoaded(AtMaterialOwner* owner) override {
        g_log[side].add('L', (kind << 16) | index);
        g_log[side].add('l', owner == g_own[side]);
    }
};
static_assert(sizeof(TestItem) <= 0x40, "TestItem");

static TestItem* item(int side, const void* self, int kind, unsigned i) { return (TestItem*)g_pool[side][which(side, self)][kind][std::min(i, (unsigned)kPool - 1)]; }

static void make_items(int side, const void* self, int kind, unsigned count) {
    if (count > kPool) eq("more items than the pool holds", count, 0);
    for (unsigned i = 0; i < std::min(count, (unsigned)kPool); i++) {
        void* p = item(side, self, kind, i);
        std::memset(p, 0xCD, 0x40);
        TestItem* t = new (p) TestItem;
        t->side = side;
        t->kind = kind;
        t->index = i;
        t->m_flags = 0;
        t->m_levels = 0;
    }
}

// The slots, for either side.
static void do_ready(int side, AtTextureDatabase* self, AtMaterialOwner* owner) {
    g_log[side].add('Y', owner == g_own[side]);
    g_log[side].add('y', self->m_surface2DCount);
    g_log[side].add('y', self->m_texture2DCount);
    g_log[side].add('y', self->m_group != nullptr);
}
static void do_surfaces(int side, int kind, unsigned count) { g_log[side].add('s', kind); g_log[side].add('s', count); }
static void do_textures(int side, AtTextureDatabase* self, int kind, unsigned count) {
    g_log[side].add('m', kind);
    g_log[side].add('m', count);
    make_items(side, self, kind, count);
}
static AtTextureDatabase* g_next_more[2];
static void do_more(int side, AtTextureDatabase* self) {
    g_log[side].add('M', which(side, self));
    self->m_more = g_next_more[side];
}
static void* do_surface_at(int side, int kind, unsigned i) { g_log[side].add('S', kind); g_log[side].add('S', i); return nullptr; }
static AtTextureItem* do_at(int side, AtTextureDatabase* self, int kind, unsigned i) {
    g_log[side].add('A', (kind << 16) | i);
    return item(side, self, kind, i);
}

class TestDb : public AtTextureDatabase {
public:
    void OnReady(AtMaterialOwner* owner) override { do_ready(1, this, owner); }
    void MakeSurfaces2D() override { do_surfaces(1, 0, m_surface2DCount); }
    void MakeSurfaces3D() override { do_surfaces(1, 1, m_surface3DCount); }
    void MakeTextures2D() override { do_textures(1, this, 0, m_texture2DCount); }
    void MakeTextures3D() override { do_textures(1, this, 1, m_texture3DCount); }
    void MakeCubeMaps() override { do_textures(1, this, 2, m_cubeMapCount); }
    void MakeMore() override { do_more(1, this); }
    void* Surface2DAt(unsigned i) override { return do_surface_at(1, 0, i); }
    void* Surface3DAt(unsigned i) override { return do_surface_at(1, 1, i); }
    AtTextureItem* Texture2DAt(unsigned i) override { return do_at(1, this, 0, i); }
    AtTextureItem* Texture3DAt(unsigned i) override { return do_at(1, this, 1, i); }
    AtTextureItem* CubeMapAt(unsigned i) override { return do_at(1, this, 2, i); }
};
typedef AtTextureDatabase Db;
static void __fastcall o_ready(Db* s, void*, AtMaterialOwner* owner) { do_ready(0, s, owner); }
static void __fastcall o_s2(Db* s, void*) { do_surfaces(0, 0, s->m_surface2DCount); }
static void __fastcall o_s3(Db* s, void*) { do_surfaces(0, 1, s->m_surface3DCount); }
static void __fastcall o_t2(Db* s, void*) { do_textures(0, s, 0, s->m_texture2DCount); }
static void __fastcall o_t3(Db* s, void*) { do_textures(0, s, 1, s->m_texture3DCount); }
static void __fastcall o_tc(Db* s, void*) { do_textures(0, s, 2, s->m_cubeMapCount); }
static void __fastcall o_more(Db* s, void*) { do_more(0, s); }
static void* __fastcall o_sa2(Db*, void*, unsigned i) { return do_surface_at(0, 0, i); }
static void* __fastcall o_sa3(Db*, void*, unsigned i) { return do_surface_at(0, 1, i); }
static AtTextureItem* __fastcall o_a2(Db* s, void*, unsigned i) { return do_at(0, s, 0, i); }
static AtTextureItem* __fastcall o_a3(Db* s, void*, unsigned i) { return do_at(0, s, 1, i); }
static AtTextureItem* __fastcall o_ac(Db* s, void*, unsigned i) { return do_at(0, s, 2, i); }
static const void* g_db_table[14] = {(const void*)(uintptr_t)0x0002ADF0, (const void*)(uintptr_t)0x0001C6B0, (const void*)o_ready, (const void*)o_s2, (const void*)o_s3, (const void*)o_t2, (const void*)o_t3,
                                     (const void*)o_tc, (const void*)o_more, (const void*)o_sa2, (const void*)o_sa3, (const void*)o_a2, (const void*)o_a3, (const void*)o_ac};

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnAlloc)(void*, void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, const char*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);
typedef unsigned(__fastcall* FnCount)(void*, void*);
typedef void*(__fastcall* FnFind)(void*, void*);

static Db* make_o(int slot, unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(Db));
    std::memset(o, fill, sizeof(Db));
    g_db[0][slot] = o;
    at<Fn0>(0x0001BFA0)(o, nullptr);
    eq("the original's vftable", *(uint32_t*)o, 0x00272574);
    *(const void***)o = g_db_table;
    return (Db*)o;
}
static TestDb* make_r(int slot, unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(TestDb));
    std::memset(r, fill, sizeof(TestDb));
    g_db[1][slot] = r;
    return new (r) TestDb;
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

// The items compared whatever the counts say now, as many as were ever asked for.
static void same_items(int slot, const Db* o, const Db* r) {
    const unsigned counts[kKinds] = {o->m_texture2DCount, o->m_texture3DCount, o->m_cubeMapCount};
    for (int kind = 0; kind < kKinds; kind++) {
        for (unsigned i = 0; i < std::min(counts[kind], (unsigned)kPool); i++) {
            const TestItem* a = (const TestItem*)g_pool[0][slot][kind][i];
            const TestItem* b = (const TestItem*)g_pool[1][slot][kind][i];
            eq("item's flags", a->m_flags, b->m_flags);
            eq("item's levels", a->m_levels, b->m_levels);
        }
    }
}

static void same_db(const char* what, int slot, const Db* o, const Db* r) {
    g_recording = false;
    eq("2D surfaces", o->m_surface2DCount, r->m_surface2DCount);
    eq("3D surfaces", o->m_surface3DCount, r->m_surface3DCount);
    eq("2D textures", o->m_texture2DCount, r->m_texture2DCount);
    eq("3D textures", o->m_texture3DCount, r->m_texture3DCount);
    eq("cube maps", o->m_cubeMapCount, r->m_cubeMapCount);
    eq("file system", (uintptr_t)o->m_fileSystem, (uintptr_t)r->m_fileSystem);
    eq("more", which(0, o->m_more) * (o->m_more != nullptr), which(1, r->m_more) * (r->m_more != nullptr));
    eq("has a directory", o->m_directory != nullptr, r->m_directory != nullptr);
    if (o->m_directory && r->m_directory) eqs("directory", o->m_directory, r->m_directory);
    eq("demand count", *(unsigned*)(uintptr_t)0x002C62E0, AtTextureDatabase::s_demandLoadCount);
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's item", (char*)x.m_value - (char*)g_pool[0], (char*)y.m_value - (char*)g_pool[1]);
            }
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    if (g_own[0] && g_own[1]) {
        eq("in the plain group", o->m_group == &g_own[0]->m_textures, r->m_group == &g_own[1]->m_textures);
        eq("in the wrong group", o->m_group == &g_own[0]->m_materials, r->m_group == &g_own[1]->m_materials);
        eqs("last texture space", group_name(g_own[0]->m_textureSpaces.m_last).c_str(), group_name(g_own[1]->m_textureSpaces.m_last).c_str());
    }
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    same_items(slot, o, r);
    compare_logs(what);
}

// ---- what the owners know ----------------------------------------------------------------------------------------------

static void* value_of(const std::string& name, uint32_t salt) {
    uint32_t h = 0x811C9DC5 ^ salt;
    for (const char c : name) h = (h ^ (unsigned char)c) * 16777619;
    return (void*)(uintptr_t)(h | 0x1000);
}

enum KnownMode { kKnowsNone, kKnowsAll, kKnowsSome, kKnowsPlainOnly, kKnowsSpacesOnly, kKnownModes };

struct Env {
    std::vector<GmNameMap*> maps[2];
    Env(const std::set<std::string>& names, const std::set<std::string>& spaces, int mode) {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            g_own[side] = new AtMaterialOwner;
            AtMaterialOwner& w = *g_own[side];
            w.m_textureSpaces.Init((unsigned)spaces.size() + 4);
            const auto fill = [&](bool all, bool some) {
                GmNameMap* m = new GmNameMap;
                m->Init((unsigned)names.size() + 1);
                bool take = true;
                for (const std::string& n : names) {
                    if (n[0] != 'X' && (all || (some && take))) m->Insert(n.c_str(), value_of(n, 1));
                    take = !take;
                }
                maps[side].push_back(m);
                return m;
            };
            w.m_textures.Add(fill(mode == kKnowsAll || mode == kKnowsPlainOnly, mode == kKnowsSome));
            for (const std::string& s : spaces) w.m_textureSpaces.Add(s.c_str(), fill(mode == kKnowsAll || mode == kKnowsSpacesOnly, mode == kKnowsSome));
        }
        g_recording = was;
    }
    ~Env() {
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            for (GmNameMap* m : maps[side]) delete m;
            delete g_own[side];
            g_own[side] = nullptr;
        }
    }
};

// Every quoted string, cut to the eight bytes a name has; those after a NameSpace word are spaces. Names beginning with X are never known, nor spaces beginning with X held.
static void gather(const std::vector<unsigned char>& d, std::set<std::string>& names, std::set<std::string>& spaces) {
    size_t i = 0;
    while (i < d.size()) {
        const unsigned char c = d[i];
        if (c == '/' && i + 1 < d.size() && d[i + 1] == '/') { while (i < d.size() && d[i] != '\n') i++; continue; }
        if (c == '"') {
            size_t e = i + 1;
            while (e < d.size() && d[e] != '"' && d[e] != '\n') e++;
            std::string s((const char*)d.data() + i + 1, e - i - 1);
            if (s.size() > 8) s.resize(8);
            const bool after_word = i >= 10 && !_strnicmp((const char*)d.data() + i - 10, "NameSpace", 9);
            if (!s.empty()) {
                names.insert(s);
                if (after_word && s[0] != 'X') spaces.insert(s);
            }
            i = e + 1;
            continue;
        }
        i++;
    }
}

static void reset_path() {
    g_fs->m_path.m_top = -1;
}

static void set_path(const char* path) {
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, path);
}

struct LoadArg { Db* o; TestDb* r; };

// A load on each side, either of which may crash: the report does not stop here, as the game's does, and a list may be missing. Crashing must agree, and so must everything up to it.
static void load_both(LoadArg a, const char* what, int slot = 0) {
    const bool oc = guarded([&] { On on(0); at<FnLoad>(0x0001C7B0)(a.o, nullptr, g_own[0], "MODEL\\THING", 77); });
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    const int otop = g_fs->m_path.m_top;
    reset_path();
    const bool rc = guarded([&] { On on(1); a.r->Load(g_own[1], "MODEL\\THING", 77); });
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    const int rtop = g_fs->m_path.m_top;
    reset_path();
    eq("crashed", oc, rc);
    eq("depth of the path after", otop, rtop);
    same_db(what, slot, a.o, a.r);
}

static void destroy(Db* o, TestDb* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002ADF0)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~TestDb(); }
    compare_logs(what);
    // What it leaves: the base's fields cleared, and its own as they were, the directory not freed.
    eq("left: 2D surfaces", o->m_surface2DCount, r->m_surface2DCount);
    eq("left: 2D textures", o->m_texture2DCount, r->m_texture2DCount);
    eq("left: directory", o->m_directory != nullptr, r->m_directory != nullptr);
    eq("left: entries", o->m_entries != nullptr, r->m_entries != nullptr);
    eq("left: size", o->m_size, r->m_size);
    std::free(o);
    std::free(r);
}

static unsigned g_files;
static const char kPath[] = "ART\\CAST/KND1\\THING.AGT";

static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, int mode, bool stats = false) {
    g_context = context + " mode " + std::to_string(mode);
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces, mode);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    set_path(kPath);
    g_fill = fill;
    Db* o = make_o(0, fill);
    TestDb* r = make_r(0, fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(Db) - 4), 0);
    LoadArg a = {o, r};
    load_both(a, "load");
    if (stats) {
        g_items += r->m_texture2DCount + r->m_texture3DCount + r->m_cubeMapCount;
        g_surfaces += r->m_surface2DCount + r->m_surface3DCount;
        g_demand += AtTextureDatabase::s_demandLoadCount;
    }
    // Over what is there.
    load_both(a, "load again");
    // Cleared, and clearing nothing.
    { On on(0); at<Fn0>(0x0001C6B0)(o, nullptr); at<Fn0>(0x0001C6B0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_db("clear", 0, o, r);
    // From clear, so that a count left over does not stand in for one the file sets.
    load_both(a, "load after clear");
    destroy(o, r, "destroy");
    g_db[0][0] = g_db[1][0] = nullptr;
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

#define ONE2D "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n"

static void run_written() {
    static const char* const texts[] = {
        "",
        "// nothing but a comment\n",
        ONE2D,
        // Real shape, with each kind.
        "Surface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"A\"\n {\n PlatformSource\n }\n Texture2D \"B\"\n {\n DDSSource\n MipMap 3\n }\n}\n",
        "Surface3D[2]\nTexture[2]\nTexture3D[2]\n{\n Texture3D \"A\"\n {\n }\n Texture3D \"B\"\n {\n }\n}\n",
        "Surface2D[6]\nTexture[2]\nCubeMapTexture[2]\n{\n CubeMapTexture \"A\"\n {\n }\n CubeMapTexture \"B\"\n {\n }\n}\n",
        "Surface2D[1]\nSurface3D[1]\nTexture[3]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\nTexture3D[1]\n{\n Texture3D \"B\"\n {\n }\n}\nCubeMapTexture[1]\n{\n CubeMapTexture \"C\"\n {\n }\n}\n",
        // Kinds in another order, and only the later kinds on demand.
        "Surface2D[1]\nTexture[3]\nCubeMapTexture[1]\n{\n CubeMapTexture \"C\"\n {\n DemandLoad\n }\n}\nTexture3D[1]\n{\n Texture3D \"B\"\n {\n }\n}\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        // On demand, in each kind.
        "Surface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"A\"\n {\n DemandLoad\n }\n Texture2D \"B\"\n {\n }\n}\n",
        "Surface2D[2]\nTexture[2]\nTexture3D[2]\n{\n Texture3D \"A\"\n {\n }\n Texture3D \"B\"\n {\n DemandLoad\n }\n}\n",
        "Surface2D[2]\nTexture[2]\nCubeMapTexture[2]\n{\n CubeMapTexture \"A\"\n {\n DemandLoad\n }\n CubeMapTexture \"B\"\n {\n DemandLoad\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n DemandLoad\n }\n}\n",
        // A name given twice: known the second time, through the database itself.
        "Surface2D[3]\nTexture[3]\nTexture2D[3]\n{\n Texture2D \"XA\"\n {\n }\n Texture2D \"XA\"\n {\n DemandLoad\n }\n Texture2D \"XB\"\n {\n }\n}\n",
        // An item that names itself as it reads: see TestItem::Read.
        "Surface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"XA\"\n {\n Tm2Source\n }\n Texture2D \"XB\"\n {\n }\n}\n",
        // Never known to the owner, some of them.
        "Surface2D[3]\nTexture[3]\nTexture2D[3]\n{\n Texture2D \"XA\"\n {\n }\n Texture2D \"B\"\n {\n }\n Texture2D \"XC\"\n {\n DemandLoad\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"LongerThanEight\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"Exactly8\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"\"\n {\n }\n}\n",
        // More names than the table was sized for; fewer.
        "Surface2D[1]\nTexture[1]\nTexture2D[9]\n{\n Texture2D \"XA\"\n {\n }\n Texture2D \"XB\"\n {\n }\n Texture2D \"XC\"\n {\n }\n Texture2D \"XD\"\n {\n }\n Texture2D \"XE\"\n {\n }\n Texture2D \"XF\"\n {\n }\n Texture2D \"XG\"\n {\n }\n Texture2D \"XH\"\n {\n }\n Texture2D \"XI\"\n {\n }\n}\n",
        // Name spaces.
        "NameSpace \"Other\"\nSurface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"A\"\n {\n }\n Texture2D \"XB\"\n {\n }\n}\n",
        "NameSpace \"Other\"\nSurface2D[2]\nTexture[2]\nTexture3D[1]\n{\n Texture3D \"A\"\n {\n }\n}\nCubeMapTexture[1]\n{\n CubeMapTexture \"B\"\n {\n }\n}\n",
        "NameSpace \"\"\nSurface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        "NameSpace \"XNoSpace\"\nSurface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        "NameSpace\nSurface2D[1]\n",
        "NameSpace 5\n",
        "NameSpace \"Other\"\nNameSpace \"Space\"\n",
        "Surface2D[1]\nNameSpace \"Other\"\n",
        // A word given twice, with and without the word Texture before it.
        "Surface2D[1]\nSurface2D[2]\n",
        "Surface3D[1]\nSurface3D[2]\n",
        "Surface2D[1]\nTexture[1]\nTexture[2]\n",
        "Surface3D[1]\nSurface2D[1]\nTexture[2]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[2]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\nTexture2D[1]\n{\n Texture2D \"B\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[2]\nTexture3D[1]\n{\n Texture3D \"A\"\n {\n }\n}\nTexture3D[1]\n{\n Texture3D \"B\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[2]\nCubeMapTexture[1]\n{\n CubeMapTexture \"A\"\n {\n }\n}\nCubeMapTexture[1]\n{\n CubeMapTexture \"B\"\n {\n }\n}\n",
        // Given twice, shorter the second time: the first names are lost, and the count is the last.
        "Surface2D[1]\nTexture[3]\nTexture2D[2]\n{\n Texture2D \"XA\"\n {\n }\n Texture2D \"XB\"\n {\n }\n}\nTexture2D[1]\n{\n Texture2D \"XC\"\n {\n }\n}\n",
        // Texture as a word coming after a list, and never.
        "Surface2D[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\nTexture[1]\n",
        "Texture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture2D[1]\n{\n Texture2D \"XA\"\n {\n }\n}\n",
        // Empty lists.
        "Surface2D[0]\n", "Surface3D[0]\n", "Texture[0]\n", "Surface2D[1]\nTexture[0]\nTexture2D[0]\n{\n}\n", "Surface2D[1]\nTexture[0]\nTexture3D[0]\n{\n}\n", "Surface2D[1]\nTexture[0]\nCubeMapTexture[0]\n{\n}\n",
        "Texture2D[0]\n{\n}\n", "Texture3D[0]\n{\n}\n", "CubeMapTexture[0]\n{\n}\n",
        // Wrong punctuation around each.
        "Surface2D\n", "Surface2D 1\n", "Surface2D[\"A\"]\n", "Surface2D[1\n", "Surface3D\n", "Surface3D[1\n", "Texture\n", "Texture[1\n", "Texture[\"A\"]\n",
        "Surface2D[1]\nTexture2D\n{\n}\n", "Surface2D[1]\nTexture2D[1]\n", "Surface2D[1]\nTexture2D[1\n{\n}\n",
        // Wrong entries.
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture3D \"A\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture3D[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nCubeMapTexture[1]\n{\n Texture2D \"A\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[2]\nTexture2D[1]\n{\n Texture2D \"XA\"\n {\n }\n Texture2D \"XB\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"XA\"\n {\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D 5\n {\n }\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n}\n",
        "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n",
        // Words that are not the file's, or nothing's.
        "BmpSource\n", "DemandLoad\n", "MipMap 2\n", "ProgSource\n", "5\n", "\"Name\"\n", "{\n}\n", "[\n", "Nonsense\n",
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const char* t : texts) {
            for (int mode = 0; mode < kKnownModes; mode++) run_text(bytes(t), std::string("text [") + t + "]", fill, mode);
        }
    }
    // Other paths for the directory to come from.
    for (const char* path : {"THING.AGT", "A/B/THING.AGT", "\\THING.AGT", ""}) {
        const std::string text = "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"A\"\n {\n DemandLoad\n }\n}\n";
        g_context = path;
        const auto data = bytes(text);
        std::set<std::string> names = {"A"}, spaces = {"Space"};
        Env env(names, spaces, kKnowsNone);
        g_fs->files[0].data = g_fs->files[1].data = data.data();
        g_fs->files[0].size = g_fs->files[1].size = data.size();
        set_path(path);
        g_fill = 0xCD;
        Db* o = make_o(0, 0xCD);
        TestDb* r = make_r(0, 0xCD);
        load_both({o, r}, "another path");
        { On on(0); at<Fn0>(0x0001C770)(o, nullptr); }
        const std::string pushed = g_fs->m_path.m_top >= 0 ? g_fs->m_path.Current() : "(nothing)";
        const int top = g_fs->m_path.m_top;
        reset_path();
        { On on(1); r->PushDirectory(); }
        eq("pushed depth", top, g_fs->m_path.m_top);
        eqs("pushed", pushed.c_str(), g_fs->m_path.m_top >= 0 ? g_fs->m_path.Current() : "(nothing)");
        reset_path();
        destroy(o, r, "destroy");
        g_db[0][0] = g_db[1][0] = nullptr;
    }
}

// ---- entries called directly -------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// One text through a tokenizer of the harness's, not reading through the file code at all.
class DirectTokenizer : public AtIoTextTokenizer {
public:
    void Error(int code) override { g_log[g_side].add('E', code); }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const char* t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void read_direct(Db* o, TestDb* r, const char* text, const char* what) {
    Direct da(text), db(text);
    const bool oc = guarded([&] { On on(0); at<FnRead>(0x0001BFE0)(o, nullptr, g_own[0], &da.tokenizer); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer); });
    g_recording = false;
    g_side = 0;
    eq("crashed", oc, rc);
    same_db(what, 0, o, r);
}

static void run_tables() {
    // The keyword table.
    g_context = "keywords";
    const AtIoKeyword* table = (const AtIoKeyword*)(uintptr_t)0x002B0230;
    for (int i = 0; i < AtTextureDatabase::kKeywordCount; i++) {
        eq("keyword's id", table[i].m_id, AtTextureDatabase::s_keywords[i].m_id);
        eqs("keyword's word", table[i].m_name, AtTextureDatabase::s_keywords[i].m_name);
    }
    // The heap's setter.
    g_context = "set heap";
    int other;
    at<void(__cdecl*)(void*)>(0x0001BF90)(&other);
    AtTextureDatabase::SetHeap(&other);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002C62E4 == (uintptr_t)&other, (uintptr_t)AtTextureDatabase::s_heap == (uintptr_t)&other);
    at<void(__cdecl*)(void*)>(0x0001BF90)(&g_heap_token);
    AtTextureDatabase::SetHeap(&g_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002C62E4, (uintptr_t)AtTextureDatabase::s_heap);
}

static void run_owner() {
    g_context = "owner";
    Env env({"A"}, {"Space", "Other"}, kKnowsNone);
    GmNameMap a, b, c, d;
    for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
        at<FnAdd>(0x00025E70)(g_own[0], nullptr, &a, ns);
        g_own[1]->AddTextures(&b, ns);
        eq("in the plain group", a.m_group == &g_own[0]->m_textures, b.m_group == &g_own[1]->m_textures);
        eq("in the materials' group", a.m_group == &g_own[0]->m_materials, b.m_group == &g_own[1]->m_materials);
        eqs("group's name", group_name(a.m_group).c_str(), group_name(b.m_group).c_str());
        eq("first", g_own[0]->m_textures.m_first == &a, g_own[1]->m_textures.m_first == &b);
        at<FnAdd>(0x00025E70)(g_own[0], nullptr, &c, ns);
        g_own[1]->AddTextures(&d, ns);
        eq("second's next", c.m_next == &a, d.m_next == &b);
    }
}

static void run_allocate() {
    g_context = "allocate";
    Env env({"A", "B"}, {"Space", "Other"}, kKnowsNone);
    g_fill = 0xCD;
    static const unsigned kinds[][5] = {{0, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 0, 0, 1}, {2, 3, 4, 5, 6}, {4, 0, 3, 0, 1}, {0, 4, 0, 3, 0}, {30, 0, 30, 0, 0}};
    for (const auto& k : kinds) {
        for (const char* ns : {(const char*)nullptr, "", "Space", "New"}) {
            Db* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            { On on(0); at<FnAlloc>(0x0001C600)(o, nullptr, g_own[0], k[0], k[1], k[2], k[3], k[4], ns); }
            { On on(1); r->Allocate(g_own[1], k[0], k[1], k[2], k[3], k[4], ns); }
            same_db("allocate from new", 0, o, r);
            // Over it, through Clear, or else with the table's own Clear undoing the counts.
            { On on(0); at<FnAlloc>(0x0001C600)(o, nullptr, g_own[0], 1, 1, 2, 2, 1, "Other"); }
            { On on(1); r->Allocate(g_own[1], 1, 1, 2, 2, 1, "Other"); }
            same_db("allocate over it", 0, o, r);
            // Only the second surface count set, or only the first: either calls for Clear.
            o->m_surface2DCount = r->m_surface2DCount = 0;
            o->m_surface3DCount = r->m_surface3DCount = 1;
            o->m_cubeMapCount = r->m_cubeMapCount = 7;
            { On on(0); at<FnAlloc>(0x0001C600)(o, nullptr, g_own[0], 0, 0, 1, 0, 0, nullptr); }
            { On on(1); r->Allocate(g_own[1], 0, 0, 1, 0, 0, nullptr); }
            same_db("allocate over 3D surfaces", 0, o, r);
            o->m_surface2DCount = r->m_surface2DCount = 1;
            o->m_surface3DCount = r->m_surface3DCount = 0;
            { On on(0); at<FnAlloc>(0x0001C600)(o, nullptr, g_own[0], 0, 0, 1, 0, 0, nullptr); }
            { On on(1); r->Allocate(g_own[1], 0, 0, 1, 0, 0, nullptr); }
            same_db("allocate over 2D surfaces", 0, o, r);
            destroy(o, r, "destroy");
        }
    }
    g_db[0][0] = g_db[1][0] = nullptr;
}

static void run_clear() {
    g_context = "clear";
    Env env({"A"}, {"Space"}, kKnowsNone);
    g_fill = 0xCD;
    Db* o = make_o(0, 0xCD);
    TestDb* r = make_r(0, 0xCD);
    int more;
    for (const bool withDirectory : {false, true}) {
        for (Db* d : {(Db*)o, (Db*)r}) {
            d->m_surface2DCount = 1;
            d->m_surface3DCount = 2;
            d->m_texture2DCount = 3;
            d->m_texture3DCount = 4;
            d->m_cubeMapCount = 5;
            d->m_fileSystem = g_fs;
            d->m_more = (AtTextureDatabase*)&more;
            d->m_directory = withDirectory ? (char*)std::calloc(1, 8) : nullptr;
        }
        { On on(0); at<Fn0>(0x0001C6B0)(o, nullptr); }
        { On on(1); r->Clear(); }
        eq("cleared: 2D surfaces", o->m_surface2DCount, r->m_surface2DCount);
        eq("cleared: 3D surfaces", o->m_surface3DCount, r->m_surface3DCount);
        eq("cleared: 2D textures", o->m_texture2DCount, r->m_texture2DCount);
        eq("cleared: 3D textures", o->m_texture3DCount, r->m_texture3DCount);
        eq("cleared: cube maps", o->m_cubeMapCount, r->m_cubeMapCount);
        eq("cleared: file system", o->m_fileSystem != nullptr, r->m_fileSystem != nullptr);
        eq("cleared: more", o->m_more != nullptr, r->m_more != nullptr);
        eq("cleared: directory", o->m_directory != nullptr, r->m_directory != nullptr);
        compare_logs("clear");
    }
    // Destroying, deleting and not.
    for (Db* d : {(Db*)o, (Db*)r}) {
        d->m_surface2DCount = 1;
        d->m_directory = (char*)std::calloc(1, 8);
        d->m_fileSystem = g_fs;
    }
    destroy(o, r, "destroy with a directory");
    o = make_o(0, 0xCD);
    r = make_r(0, 0xCD);
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002ADF0)(o, nullptr, 1), (uintptr_t)o); }
    { On on(1); delete r; }
    compare_logs("deleted");
    // The complete destructor by itself.
    o = make_o(0, 0xCD);
    r = make_r(0, 0xCD);
    { On on(0); at<Fn0>(0x0001BFD0)(o, nullptr); }
    { On on(1); r->~TestDb(); }
    compare_logs("destroyed");
    eq("vftable after", *(uint32_t*)o, 0x00272E60);
    std::free(o);
    std::free(r);
    g_db[0][0] = g_db[1][0] = nullptr;
}

static void run_directory() {
    g_context = "directory";
    Env env({"A"}, {"Space"}, kKnowsNone);
    Db* o = make_o(0, 0xCD);
    TestDb* r = make_r(0, 0xCD);
    static char dir[] = "SOME/WHERE\\FILE.AGT";
    for (const bool withFs : {false, true}) {
        for (const bool withDirectory : {false, true}) {
            o->m_fileSystem = r->m_fileSystem = withFs ? g_fs : nullptr;
            o->m_directory = r->m_directory = withDirectory ? dir : nullptr;
            reset_path();
            at<Fn0>(0x0001C770)(o, nullptr);
            const int top = g_fs->m_path.m_top;
            const std::string name = top >= 0 ? g_fs->m_path.Current() : "";
            reset_path();
            r->PushDirectory();
            eq("pushed", top, g_fs->m_path.m_top);
            eqs("pushed", name.c_str(), g_fs->m_path.m_top >= 0 ? g_fs->m_path.Current() : "");
            // Popped from a depth, on each.
            reset_path();
            g_fs->m_path.Push("ONE");
            at<Fn0>(0x0001C790)(o, nullptr);
            const int otop = g_fs->m_path.m_top;
            reset_path();
            g_fs->m_path.Push("ONE");
            r->PopDirectory();
            eq("popped", otop, g_fs->m_path.m_top);
            reset_path();
        }
    }
    o->m_fileSystem = r->m_fileSystem = nullptr;
    o->m_directory = r->m_directory = nullptr;
    destroy(o, r, "destroy");
    g_db[0][0] = g_db[1][0] = nullptr;
}

// A chain of databases, each with some items having levels.
static void run_chain() {
    g_context = "chain";
    Env env({"A"}, {"Space"}, kKnowsNone);
    static const std::vector<std::vector<int>> chains[] = {
        {{1}}, {{0}}, {{2, 1, 0}}, {{0x80, 0}}, {{1, 1, 1, 1}}, {{}}, {{0x80}},
        {{1}, {0}}, {{1, 2}, {0, 1}}, {{}, {1, 1}, {5}}, {{1}, {1}, {1}, {1, 0}}, {{1, 3}, {2, 1}, {1, 1}, {0xFF}}, {{1}, {1}, {1}, {1}},
    };
    for (const auto& chain : chains) {
        Db* o[kDbs];
        TestDb* r[kDbs];
        for (int i = 0; i < kDbs; i++) {
            o[i] = make_o(i, 0xCD);
            r[i] = make_r(i, 0xCD);
        }
        const int links = (int)chain.size();
        for (int i = 0; i < links; i++) {
            const unsigned count = (unsigned)chain[i].size();
            o[i]->m_texture2DCount = r[i]->m_texture2DCount = count;
            make_items(0, o[i], 0, count);
            make_items(1, r[i], 0, count);
            for (unsigned j = 0; j < count; j++) {
                item(0, o[i], 0, j)->m_levels = (unsigned char)chain[i][j];
                item(1, r[i], 0, j)->m_levels = (unsigned char)chain[i][j];
            }
            // 3D textures and cube maps with levels, which neither looks at.
            o[i]->m_texture3DCount = r[i]->m_texture3DCount = 1;
            make_items(0, o[i], 1, 1);
            make_items(1, r[i], 1, 1);
            item(0, o[i], 1, 0)->m_levels = item(1, r[i], 1, 0)->m_levels = 9;
            if (i + 1 < links) {
                o[i]->m_more = o[i + 1];
                r[i]->m_more = r[i + 1];
            }
        }
        { On on(0); const unsigned a = at<FnCount>(0x0001C6F0)(o[0], nullptr); g_side = 1; const unsigned b = r[0]->CountUsedTextures(); eq("count", a, b); }
        compare_logs("count");
        // The chain's own end makes more: the harness's next database has a free one, so the search ends.
        Db* lasto = o[kDbs - 1];
        TestDb* lastr = r[kDbs - 1];
        lasto->m_texture2DCount = lastr->m_texture2DCount = 1;
        make_items(0, lasto, 0, 1);
        make_items(1, lastr, 0, 1);
        item(0, lasto, 0, 0)->m_levels = item(1, lastr, 0, 0)->m_levels = 0;
        g_next_more[0] = lasto;
        g_next_more[1] = lastr;
        void* found[2];
        { On on(0); found[0] = at<FnFind>(0x0001C730)(o[0], nullptr); }
        { On on(1); found[1] = r[0]->FindFreeTexture(); }
        eq("found", (char*)found[0] - (char*)g_pool[0], (char*)found[1] - (char*)g_pool[1]);
        for (int i = 0; i < kDbs; i++) eq("linked", which(0, o[i]->m_more) * (o[i]->m_more != nullptr), which(1, r[i]->m_more) * (r[i]->m_more != nullptr));
        compare_logs("find");
        for (int i = 0; i < kDbs; i++) {
            at<FnDel>(0x0002ADF0)(o[i], nullptr, 0);
            r[i]->~TestDb();
            std::free(o[i]);
            std::free(r[i]);
            g_db[0][i] = g_db[1][i] = nullptr;
        }
    }
}

static void run_failures() {
    // No room for names: the raise, on lists that are not written into.
    g_context = "no memory";
    {
        Env env({"A"}, {"Space"}, kKnowsNone);
        g_fill = 0xCD;
        for (const char* text : {"Texture2D[0]\n{\n}\n", "Texture3D[0]\n{\n}\n", "CubeMapTexture[0]\n{\n}\n"}) {
            Db* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            g_fail_alloc = true;
            read_direct(o, r, text, "no memory");
            g_fail_alloc = false;
            destroy(o, r, "destroy");
        }
        g_db[0][0] = g_db[1][0] = nullptr;
    }
    // A stale count with no list: Read does not clear when only texture counts remain, and finds no names.
    g_context = "stale counts";
    {
        Env env({"A"}, {"Space"}, kKnowsNone);
        g_fill = 0xCD;
        for (int kind = 0; kind < kKinds; kind++) {
            Db* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            unsigned* a = kind == 0 ? &o->m_texture2DCount : kind == 1 ? &o->m_texture3DCount : &o->m_cubeMapCount;
            unsigned* b = kind == 0 ? &r->m_texture2DCount : kind == 1 ? &r->m_texture3DCount : &r->m_cubeMapCount;
            *a = *b = 1;
            make_items(0, o, kind, 1);
            make_items(1, r, kind, 1);
            read_direct(o, r, "", "stale count");
            destroy(o, r, "destroy");
        }
        g_db[0][0] = g_db[1][0] = nullptr;
    }
    // Stale surface counts: each alone calls for Clear.
    g_context = "stale surfaces";
    {
        Env env({"A"}, {"Space"}, kKnowsNone);
        g_fill = 0xCD;
        for (int which_one = 0; which_one < 2; which_one++) {
            Db* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            (which_one ? o->m_surface3DCount : o->m_surface2DCount) = 1;
            (which_one ? r->m_surface3DCount : r->m_surface2DCount) = 1;
            o->m_cubeMapCount = r->m_cubeMapCount = 3;
            read_direct(o, r, "", "stale surfaces");
            destroy(o, r, "destroy");
        }
        g_db[0][0] = g_db[1][0] = nullptr;
    }
    // Stale directory and file system through Load, with no demand-loaded item the second time.
    g_context = "second file";
    {
        Env env({"A", "B"}, {"Space"}, kKnowsNone);
        g_fill = 0xCD;
        Db* o = make_o(0, 0xCD);
        TestDb* r = make_r(0, 0xCD);
        static const char first[] = "Texture[1]\nTexture2D[1]\n{\n Texture2D \"XA\"\n {\n DemandLoad\n }\n}\n";
        static const char second[] = "Texture2D[1]\n{\n Texture2D \"XB\"\n {\n }\n}\n";
        for (const char* text : {first, second}) {
            g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)text;
            g_fs->files[0].size = g_fs->files[1].size = std::strlen(text);
            set_path(text == first ? "FIRST\\A.AGT" : "SECOND\\B.AGT");
            load_both({o, r}, "second file");
        }
        destroy(o, r, "destroy");
        g_db[0][0] = g_db[1][0] = nullptr;
    }
    // No room for the path, in Load: the raise, and then both write through null.
    g_context = "no memory for the path";
    {
        Env env({"A"}, {"Space"}, kKnowsNone);
        g_fill = 0xCD;
        static const char text[] = "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"XA\"\n {\n DemandLoad\n }\n}\n";
        g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)text;
        g_fs->files[0].size = g_fs->files[1].size = sizeof text - 1;
        set_path(kPath);
        Db* o = make_o(0, 0xCD);
        TestDb* r = make_r(0, 0xCD);
        g_fail_in_item = true;
        load_both({o, r}, "no memory for the path");
        g_fail_in_item = false;
        eq("this crashed", g_crashes > 0, 1);
        destroy(o, r, "destroy");
        g_db[0][0] = g_db[1][0] = nullptr;
    }
    // No heaps: each raise. Items are made by the harness, so only the names and the path ask.
    g_context = "no heap for names";
    {
        static const std::string text = "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"XA\"\n {\n DemandLoad\n }\n}\n";
        *(void**)(uintptr_t)0x002C60B0 = nullptr;
        AtIoTokenFile::s_heap = nullptr;
        run_text(bytes(text), "no heap for names", 0xCD, kKnowsNone);
        *(void**)(uintptr_t)0x002C60B0 = &g_heap_token;
        AtIoTokenFile::s_heap = &g_heap_token;
        g_context = "no heap for the path";
        *(void**)(uintptr_t)0x002C62E4 = nullptr;
        AtTextureDatabase::s_heap = nullptr;
        run_text(bytes(text), "no heap for the path", 0xCD, kKnowsNone);
        *(void**)(uintptr_t)0x002C62E4 = &g_heap_token;
        AtTextureDatabase::s_heap = &g_heap_token;
    }
    // A file that will not open: the report, and then a null file. In the game the report does not come back.
    g_context = "no file";
    {
        Env env({"A"}, {"Space"}, kKnowsNone);
        for (int status : {1, 2, 5}) {
            g_fs->open_status = status;
            Db* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            load_both({o, r}, "no file");
            g_fs->open_status = 0;
        }
        g_db[0][0] = g_db[1][0] = nullptr;
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
    if (pos.size() != 2) { std::printf("usage: oracle_agt <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C62E4u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtTextureDatabase::s_heap = &g_heap_token;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_tables();
    run_owner();
    run_allocate();
    run_clear();
    run_directory();
    run_chain();
    run_failures();
    run_written();

    g_items = g_surfaces = g_demand = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AGT\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_text(data, line, 0xCD, kKnowsNone, true);
        run_text(data, line, 0xCD, kKnowsAll);
        run_text(data, line, 0xCD, kKnowsSome);
        run_text(data, line, 0xCD, kKnowsPlainOnly);
        run_text(data, line, 0xCD, kKnowsSpacesOnly);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u textures, %u surfaces, %u on demand; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_items, g_surfaces, g_demand, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
