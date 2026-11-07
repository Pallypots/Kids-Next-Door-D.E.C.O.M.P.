// Differential oracle for the material database (Windows x86, local only). See docs/verification.md.
//
//   oracle_mdb <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AGM file listed in index.txt, loaded through the file code on both sides; and text written here. Materials on each side are read by that side's own
// reader, whose agreement the mat suite establishes. Textures and shaders are found in real maps, one owner to a side, filled with every string of the text except names
// beginning with X.
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
#include "at/AtMaterialDatabase.h"
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

enum { kPool = 1024, kDbs = 2 };
static unsigned char g_pool[2][kDbs][kPool][sizeof(AtMaterial)];
static void* g_db[2][kDbs];
static AtMaterialOwner* g_own[2];

static int which(int side, const void* self) { return self == g_db[side][1] ? 1 : 0; }
static AtMaterial* item(int side, const void* self, unsigned i) { return (AtMaterial*)g_pool[side][which(side, self)][std::min(i, (unsigned)kPool - 1)]; }

// What a material does when loaded is the concrete class's: both sides note that it ran.
class ConcreteMaterial : public AtMaterial {
public:
    void OnLoaded(AtMaterialOwner* owner) override { g_log[1].add('L', owner == g_own[1]); g_log[1].add('l', m_store->m_group != nullptr); }
};
static void __fastcall o_loaded(void* self, void*, void* owner) { g_log[0].add('L', owner == g_own[0]); g_log[0].add('l', ((AtMaterial*)self)->m_store->m_group != nullptr); }
static const void* g_material_table[5] = {(const void*)o_loaded, (const void*)(uintptr_t)0x00038F00, (const void*)(uintptr_t)0x00038C50, (const void*)(uintptr_t)0x00038370, (const void*)(uintptr_t)0x00038390};

typedef void(__fastcall* Fn0)(void*, void*);

static void make_items(int side, const void* self, unsigned count) {
    const bool was = g_recording;
    g_recording = false;
    for (unsigned i = 0; i < std::min(count, (unsigned)kPool); i++) {
        void* p = g_pool[side][which(side, self)][i];
        std::memset(p, 0xCD, sizeof(AtMaterial));
        if (side == 0) {
            at<Fn0>(0x00038EB0)(p, nullptr);
            *(const void***)p = g_material_table;
        } else {
            new (p) ConcreteMaterial;
        }
    }
    g_recording = was;
}

class TestDb : public AtMaterialDatabase {
public:
    void OnReady(AtMaterialOwner* owner) override {
        g_log[1].add('Y', owner == g_own[1]);
        g_log[1].add('y', m_group != nullptr);
        make_items(1, this, m_count);
    }
    void Slot3() override {}
    AtMaterial* MaterialAt(unsigned int i) override {
        g_log[1].add('A', i);
        return item(1, this, i);
    }
};
static void __fastcall o_ready(void* self, void*, void* owner) {
    g_log[0].add('Y', owner == g_own[0]);
    g_log[0].add('y', ((AtMaterialDatabase*)self)->m_group != nullptr);
    make_items(0, self, ((AtMaterialDatabase*)self)->m_count);
}
static void __fastcall o_slot3(void*, void*) {}
static AtMaterial* __fastcall o_at(void* self, void*, unsigned i) {
    g_log[0].add('A', i);
    return item(0, self, i);
}
static const void* g_db_table[9] = {(const void*)(uintptr_t)0x00039890, (const void*)(uintptr_t)0x000392F0, (const void*)o_ready, (const void*)o_slot3, (const void*)(uintptr_t)0x00039180, (const void*)(uintptr_t)0x00039050, (const void*)(uintptr_t)0x00038FB0, (const void*)(uintptr_t)0x00039500, (const void*)o_at};

typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnCopy)(void*, void*, void*, void*, const char*);
typedef void(__fastcall* FnAlloc)(void*, void*, void*, unsigned, unsigned, unsigned, const char*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnList)(void*, void*, void*, void*, void**, unsigned, const char*, int);
typedef void*(__fastcall* FnFind)(void*, void*, const char*, const char*);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);

static AtMaterialDatabase* make_o(int slot, unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtMaterialDatabase));
    std::memset(o, fill, sizeof(AtMaterialDatabase));
    g_db[0][slot] = o;
    at<Fn0>(0x00038F80)(o, nullptr);
    *(const void***)o = g_db_table;
    return (AtMaterialDatabase*)o;
}
static TestDb* make_r(int slot, unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(TestDb));
    std::memset(r, fill, sizeof(TestDb));
    g_db[1][slot] = r;
    return new (r) TestDb;
}

// A list's entries are the same on both sides: values given to the maps, or what the heap's fill left.
static void same_list(const char* what, void** o, void** r, unsigned count) {
    eq(what, o != nullptr, r != nullptr);
    if (o && r) eq(what, std::memcmp(o, r, count * sizeof(void*)), 0);
}

static long long index_in(void** list, void** p) {
    if (!p) return -1;
    if (!list) return -2;
    return p - list;
}

static void same_material(const AtMaterialDatabase* odb, const AtMaterialDatabase* rdb, const AtMaterial* o, const AtMaterial* r) {
    eq("material's store", o->m_store == odb, r->m_store == rdb);
    eq("material's stages", index_in(odb->m_stages, o->m_stages), index_in(rdb->m_stages, r->m_stages));
    eq("material's textures", index_in(odb->m_textures, o->m_textures), index_in(rdb->m_textures, r->m_textures));
    // All but the vftable and those three, which differ by side.
    unsigned char a[sizeof(AtMaterial)], b[sizeof(AtMaterial)];
    std::memcpy(a, o, sizeof a);
    std::memcpy(b, r, sizeof b);
    for (const size_t at_ : {offsetof(AtMaterial, m_store), offsetof(AtMaterial, m_stages), offsetof(AtMaterial, m_textures)}) {
        std::memset(a + at_, 0, sizeof(void*));
        std::memset(b + at_, 0, sizeof(void*));
    }
    eq("material", std::memcmp(a + 4, b + 4, sizeof a - 4), 0);
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static void same_owner() {
    AtMaterialOwner* a = g_own[0];
    AtMaterialOwner* b = g_own[1];
    eqs("last texture space", group_name(a->m_textureSpaces.m_last).c_str(), group_name(b->m_textureSpaces.m_last).c_str());
    eqs("last material space", group_name(a->m_materialSpaces.m_last).c_str(), group_name(b->m_materialSpaces.m_last).c_str());
    eq("material spaces in use", a->m_materialSpaces.m_free != nullptr ? a->m_materialSpaces.m_free - a->m_materialSpaces.m_pool : -1, b->m_materialSpaces.m_free != nullptr ? b->m_materialSpaces.m_free - b->m_materialSpaces.m_pool : -1);
}

static unsigned g_overruns;

static void same_db(const char* what, int slot, const AtMaterialDatabase* o, const AtMaterialDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("stage capacity", o->m_stageCapacity, r->m_stageCapacity);
    eq("stages used", o->m_stageUsed, r->m_stageUsed);
    eq("texture capacity", o->m_textureCapacity, r->m_textureCapacity);
    eq("textures used", o->m_textureUsed, r->m_textureUsed);
    eq("unknown 30", o->m_unknown30, r->m_unknown30);
    eq("size", o->m_size, r->m_size);
    // A copy packs each material's range after the last, and ranges may share slots in the file, so it can run past the list. The pad holds that, and it is compared.
    if (o->m_stageUsed > o->m_stageCapacity || o->m_textureUsed > o->m_textureCapacity) g_overruns++;
    if (o->m_stageCapacity == r->m_stageCapacity && o->m_stageUsed == r->m_stageUsed) same_list("stages", o->m_stages, r->m_stages, std::max(o->m_stageCapacity, o->m_stageUsed));
    if (o->m_textureCapacity == r->m_textureCapacity && o->m_textureUsed == r->m_textureUsed) same_list("textures", o->m_textures, r->m_textures, std::max(o->m_textureCapacity, o->m_textureUsed));
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's material", (char*)x.m_value - (char*)g_pool[0][slot], (char*)y.m_value - (char*)g_pool[1][slot]);
            }
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &g_own[0]->m_materials, r->m_group == &g_own[1]->m_materials);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    if (o->m_count == r->m_count) {
        for (unsigned i = 0; i < std::min(o->m_count, (unsigned)kPool); i++) {
            same_material(o, r, (AtMaterial*)g_pool[0][slot][i], (AtMaterial*)g_pool[1][slot][i]);
        }
    }
    same_owner();
    compare_logs(what);
}

// ---- what the owners find ----------------------------------------------------------------------------------------------

static void* value_of(const std::string& name, uint32_t salt) {
    uint32_t h = 0x811C9DC5 ^ salt;
    for (const char c : name) h = (h ^ (unsigned char)c) * 16777619;
    return (void*)(uintptr_t)(h | 0x1000);
}

struct Env {
    std::vector<GmNameMap*> maps[2];
    Env(const std::set<std::string>& names, const std::set<std::string>& spaces) {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            g_own[side] = new AtMaterialOwner;
            AtMaterialOwner& w = *g_own[side];
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
            delete g_own[side];
            g_own[side] = nullptr;
        }
    }
};

// Every quoted string but X names, cut to the eight bytes a name has, and those after a NameSpace word or following another string on its line.
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

static unsigned g_files, g_materials;

struct LoadArg { AtMaterialDatabase* o; TestDb* r; };

static void load_o(void* p) { LoadArg* a = (LoadArg*)p; On on(0); at<FnLoad>(0x00038FB0)(a->o, nullptr, g_own[0], "MODEL\\THING", 77); }
static void load_r(void* p) { LoadArg* a = (LoadArg*)p; On on(1); a->r->Load(g_own[1], "MODEL\\THING", 77); }

static void destroy(AtMaterialDatabase* o, TestDb* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00039890)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~TestDb(); }
    compare_logs(what);
    std::free(o);
    std::free(r);
}

static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    AtMaterialDatabase* o = make_o(0, fill);
    TestDb* r = make_r(0, fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtMaterialDatabase) - 4), 0);
    {
        LoadArg a = {o, r};
        load_o(&a);
        load_r(&a);
        same_db("load", 0, o, r);
        g_materials += r->m_count;
        // Over what is there.
        load_o(&a);
        load_r(&a);
        same_db("load again", 0, o, r);
    }
    // Copy it, without and with a space, and over a copy.
    AtMaterialDatabase* oc = make_o(1, fill);
    TestDb* rc = make_r(1, fill);
    for (const char* ns : {(const char*)nullptr, "", "Space"}) {
        { On on(0); at<FnCopy>(0x00039180)(oc, nullptr, g_own[0], o, ns); }
        { On on(1); rc->CopyFrom(g_own[1], *r, ns); }
        same_db("copy", 1, oc, rc);
    }
    // Only some of them loaded.
    if (r->m_count > 1) {
        for (unsigned i = 0; i < std::min(r->m_count, (unsigned)kPool); i += 2) {
            ((AtMaterial*)g_pool[0][0][i])->m_flags &= ~1u;
            ((AtMaterial*)g_pool[1][0][i])->m_flags &= ~1u;
        }
        { On on(0); at<FnCopy>(0x00039180)(oc, nullptr, g_own[0], o, nullptr); }
        { On on(1); rc->CopyFrom(g_own[1], *r, nullptr); }
        same_db("copy of some", 1, oc, rc);
    }
    destroy(oc, rc, "destroy the copy");
    g_db[0][1] = g_db[1][1] = nullptr;
    // Cleared, and clearing nothing.
    { On on(0); at<Fn0>(0x000392F0)(o, nullptr); at<Fn0>(0x000392F0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_db("clear", 0, o, r);
    destroy(o, r, "destroy");
    g_db[0][0] = g_db[1][0] = nullptr;
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static void run_written() {
    static const char* const texts[] = {
        "",
        "// nothing but a comment\n",
        "Material[0]\n{\n}\n",
        "Material[1]\n{\n Material \"A\"\n {\n }\n}\n",
        "StagedShaderTexture[2]\n{\n \"T\"\n \"U\"\n}\nMaterial[2]\n{\n Material \"A\"\n {\n }\n Material \"B\"\n {\n SimpleTextureShader 1\n }\n}\n",
        "Material[2]\n{\n Material \"A\"\n {\n }\n Material \"A\"\n {\n }\n}\n",
        "Material[1]\n{\n Material \"LongerThanEight\"\n {\n }\n}\n",
        "Material[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "Material[1]\n{\n Material \"A\"\n {\n }\n",
        "Material[1]\n{\n Material \"A\"\n {\n }\n}\nMaterial[1]\n{\n Material \"B\"\n {\n }\n}\n",
        "Material[1]\n{\n Material \"A\"\n {\n }\n}\nMaterial[0]\n{\n}\n",
        "Material\n{\n}\n",
        "StagedShaderTexture[2]\n{\n \"A\"\n \"B\"\n}\n",
        "ProgShaderTexture[2]\n{\n \"A\"\n \"B\"\n}\n",
        "StagedShaderTexture[0]\n{\n}\n",
        "ProgShaderTexture[0]\n{\n}\n",
        "StagedShaderTexture[1]\n{\n \"A\"\n}\nStagedShaderTexture[2]\n{\n \"B\"\n \"C\"\n}\n",
        "ProgShaderTexture[1]\n{\n \"A\"\n}\nProgShaderTexture[2]\n{\n \"B\"\n \"C\"\n}\n",
        "StagedShaderTexture[3]\n{\n Default\n \"A\"\n Default\n}\n",
        "ProgShaderTexture[3]\n{\n \"A\"\n Default\n \"B\"\n}\n",
        "StagedShaderTexture[2]\n{\n \"XGone\"\n \"A\"\n}\n",
        "ProgShaderTexture[2]\n{\n \"A\"\n \"XGone\"\n}\n",
        "StagedShaderTexture[2]\n{\n 5\n \"A\"\n}\n",
        "ProgShaderTexture[2]\n{\n \"A\"\n 1.5\n}\n",
        "StagedShaderTexture[3]\n{\n \"A\"\n}\n",
        "StagedShaderTexture[1]\n{\n \"A\"\n \"B\"\n}\n",
        "StagedShaderTextureWithNameSpace[2]\n{\n \"A\" \"Other\"\n \"B\" \"Space\"\n}\n",
        "ProgShaderTextureWithNameSpace[2]\n{\n \"A\" \"Other\"\n \"B\" \"Space\"\n}\n",
        "StagedShaderTextureWithNameSpace[2]\n{\n \"A\" \"\"\n Default\n}\n",
        "ProgShaderTextureWithNameSpace[2]\n{\n \"A\" \"\"\n Default\n}\n",
        "StagedShaderTextureWithNameSpace[1]\n{\n \"A\" \"XNoSpace\"\n}\n",
        "ProgShaderTextureWithNameSpace[1]\n{\n \"XGone\" \"Other\"\n}\n",
        "ProgShaderTextureWithNameSpace[1]\n{\n 5 \"Other\"\n}\n",
        "StagedShaderTexture[1]\n{\n \"A\"\n}\nStagedShaderTextureWithNameSpace[1]\n{\n \"B\" \"Other\"\n}\n",
        "ProgShaderTexture[1]\n{\n \"A\"\n}\nProgShaderTextureWithNameSpace[1]\n{\n \"B\" \"Other\"\n}\n",
        "StagedShaderTextureWithNameSpace[1]\n{\n \"A\" \"Other\"\n}\nStagedShaderTextureWithNameSpace[1]\n{\n \"B\" \"Other\"\n}\n",
        "ProgShaderTextureWithNameSpace[1]\n{\n \"A\" \"Other\"\n}\nProgShaderTextureWithNameSpace[1]\n{\n \"B\" \"Other\"\n}\n",
        "NameSpace \"Other\"\nStagedShaderTexture[2]\n{\n \"A\"\n \"XGone\"\n}\nProgShaderTexture[1]\n{\n \"B\"\n}\nMaterial[1]\n{\n Material \"M\"\n {\n SimpleTextureShader 0\n Shader \"A\" 0 1 0 1\n }\n}\n",
        "NameSpace \"\"\nStagedShaderTexture[1]\n{\n \"A\"\n}\nMaterial[1]\n{\n Material \"M\"\n {\n Shader \"A\" 0 0 0 1\n }\n}\n",
        "NameSpace \"XNoSpace\"\nStagedShaderTexture[1]\n{\n \"A\"\n}\n",
        "NameSpace \"Other\"\nNameSpace \"Space\"\n",
        "Material[0]\n{\n}\nNameSpace \"Other\"\n",
        "Default\n",
        "Blend\nMaterial[0]\n{\n}\n",
        "5\n",
        "\"Name\"\n",
        "{\n}\n",
        "StagedShaderTexture[2]\n{\n \"A\"\n \"B\"\n}\nProgShaderTexture[2]\n{\n \"C\"\n \"D\"\n}\nMaterial[2]\n{\n Material \"M\"\n {\n SimpleTextureShader 1\n }\n Material \"N\"\n {\n Shader \"A\" 0 2 1 1\n }\n}\n",
        "Material[1]\n{\n Material \"M\"\n {\n Shader \"XGone\" 0 0 0 0\n }\n}\n",
        "Material[1]\n{\n Material \"M\"\n {\n ShaderWithNameSpace \"A\" \"Other\" 0 0 0 0\n }\n}\n",
    };
    // Not here: a name that is not a string, where a name is required. ExpectName then leaves its buffer as it was, which is uninitialised stack on both sides.
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const char* t : texts) run_text(bytes(t), std::string("text [") + t + "]", fill);
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
    void Error(int code) override { g_log[g_side].add('E', code); }  // The recovered report would log on the recovered side, whichever side is calling.
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
        tokenizer.SetKeywords(AtMaterial::s_keywords, AtMaterial::kKeywordCount, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void run_direct() {
    g_context = "allocate";
    std::set<std::string> names = {"A", "B"}, spaces = {"Space", "Other"};
    {
        Env env(names, spaces);
        g_fill = 0xCD;
        AtMaterialDatabase* o = make_o(0, 0xCD);
        TestDb* r = make_r(0, 0xCD);
        static const unsigned kinds[][3] = {{0, 0, 0}, {2, 0, 1}, {0, 3, 2}, {4, 5, 3}, {1, 1, 0}, {0, 0, 4}, {3, 2, 1}};
        // From new, and over that.
        for (const auto& k : kinds) {
            for (const char* ns : {(const char*)nullptr, "", "Space", "New"}) {
                AtMaterialDatabase* fo = make_o(0, 0xCD);
                TestDb* fr = make_r(0, 0xCD);
                { On on(0); at<FnAlloc>(0x00039050)(fo, nullptr, g_own[0], k[0], k[1], k[2], ns); }
                { On on(1); fr->Allocate(g_own[1], k[0], k[1], k[2], ns); }
                same_db("allocate from new", 0, fo, fr);
                { On on(0); at<FnAlloc>(0x00039050)(fo, nullptr, g_own[0], 1, 2, 1, "Other"); }
                { On on(1); fr->Allocate(g_own[1], 1, 2, 1, "Other"); }
                same_db("allocate over it", 0, fo, fr);
                destroy(fo, fr, "destroy");
            }
        }
        // In a row on one object. Once a count of zero leaves a table behind, each later call's Init clears what it has just set, so the count stays zero: as in the original.
        for (const auto& k : kinds) {
            for (const char* ns : {(const char*)nullptr, "", "Space", "New"}) {
                { On on(0); at<FnAlloc>(0x00039050)(o, nullptr, g_own[0], k[0], k[1], k[2], ns); }
                { On on(1); r->Allocate(g_own[1], k[0], k[1], k[2], ns); }
                same_db("allocate", 0, o, r);
            }
        }
        // Nothing loaded: a copy takes the sizes and no material.
        AtMaterialDatabase* oc = make_o(1, 0xCD);
        TestDb* rc = make_r(1, 0xCD);
        { On on(0); at<FnCopy>(0x00039180)(oc, nullptr, g_own[0], o, "Other"); }
        { On on(1); rc->CopyFrom(g_own[1], *r, "Other"); }
        same_db("copy of none loaded", 1, oc, rc);
        // A count with no lists; used counts that a copy resets.
        o->m_stageUsed = r->m_stageUsed = 7;
        o->m_textureUsed = r->m_textureUsed = 9;
        o->m_unknown30 = r->m_unknown30 = 11;
        { On on(0); at<FnCopy>(0x00039180)(oc, nullptr, g_own[0], o, nullptr); }
        { On on(1); rc->CopyFrom(g_own[1], *r, nullptr); }
        same_db("copy again", 1, oc, rc);
        oc->m_stageUsed = rc->m_stageUsed = 7;
        oc->m_textureUsed = rc->m_textureUsed = 9;
        oc->m_unknown30 = rc->m_unknown30 = 11;
        { On on(0); at<Fn0>(0x000392F0)(oc, nullptr); }
        { On on(1); rc->Clear(); }
        same_db("clear with used counts", 1, oc, rc);
        // Destroying on dirty counts.
        o->m_stageUsed = r->m_stageUsed = 7;
        o->m_textureUsed = r->m_textureUsed = 9;
        o->m_unknown30 = r->m_unknown30 = 11;
        {
            On on(0);
            eq("returns this", (uintptr_t)at<FnDel>(0x00039890)(o, nullptr, 0), (uintptr_t)o);
        }
        { On on(1); r->~TestDb(); }
        compare_logs("destroyed");
        eq("destroyed: used", o->m_stageUsed, r->m_stageUsed);
        eq("destroyed: textures used", o->m_textureUsed, r->m_textureUsed);
        eq("destroyed: unknown", o->m_unknown30, r->m_unknown30);
        eq("destroyed: count", o->m_count, r->m_count);
        eq("destroyed: capacities", o->m_stageCapacity + o->m_textureCapacity, r->m_stageCapacity + r->m_textureCapacity);
        eq("destroyed: lists", (o->m_stages == nullptr) + (o->m_textures == nullptr), (r->m_stages == nullptr) + (r->m_textures == nullptr));
        std::free(o);
        std::free(r);
        // The deleting destructor, with the delete.
        o = make_o(0, 0xCD);
        r = make_r(0, 0xCD);
        { On on(0); at<FnAlloc>(0x00039050)(o, nullptr, g_own[0], 2, 2, 1, nullptr); }
        { On on(1); r->Allocate(g_own[1], 2, 2, 1, nullptr); }
        compare_logs("before delete");
        { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00039890)(o, nullptr, 1), (uintptr_t)o); }
        { On on(1); delete r; }
        compare_logs("deleted");
        g_db[0][0] = g_db[1][0] = nullptr;
        destroy(oc, rc, "destroy the copy");
        g_db[0][1] = g_db[1][1] = nullptr;
    }

    // Each list reader on its own: both spaces, none, and a null one.
    g_context = "list";
    {
        Env env({"A", "B", "C"}, {"Space", "Other"});
        static const char* const lists[] = {"\"A\" \"B\" }", "Default \"A\" }", "\"XGone\" Default }", "5 6 }", "\"A\" }", "}", "\"A\" \"B\" \"C\" }", "\"A\" \"Other\" \"B\" \"\" }", "\"A\" \"XNope\" Default }", "\"XGone\" \"Other\" Default }"};
        for (const char* text : lists) {
            for (const char* ns : {(const char*)nullptr, "", "Space", "XNope"}) {
                for (int with : {0, 1, 7}) {
                    for (unsigned count : {0u, 1u, 2u}) {
                        void* a[4], *b[4];
                        std::memset(a, 0xAB, sizeof a);
                        std::memset(b, 0xAB, sizeof b);
                        Direct da(text), db(text);
                        unsigned char oself[4], rself[sizeof(TestDb)];
                        std::memset(oself, 0xEE, sizeof oself);
                        TestDb* r = new (rself) TestDb;
                        { On on(0); at<FnList>(0x00039340)(oself, nullptr, g_own[0], &da.tokenizer, a, count, ns, with); }
                        { On on(1); r->ReadTextures(g_own[1], &db.tokenizer, b, count, ns, with); }
                        eq("list", std::memcmp(a, b, sizeof a), 0);
                        compare_logs("list");
                        r->m_entries = nullptr;
                        g_recording = false;
                        r->~TestDb();
                    }
                }
            }
        }
    }

    // The owner's two, one at a time.
    g_context = "owner";
    {
        Env env({"A", "Name"}, {"Space", "Other"});
        for (const char* name : {"A", "Name", "XGone", ""}) {
            for (const char* ns : {(const char*)nullptr, "", "Space", "Other", "XNope"}) {
                eq("shader", (uintptr_t)at<FnFind>(0x000383A0)(g_own[0], nullptr, name, ns), (uintptr_t)g_own[1]->FindShader(name, ns));
                eqs("last space", group_name(g_own[0]->m_shaderSpaces.m_last).c_str(), group_name(g_own[1]->m_shaderSpaces.m_last).c_str());
            }
        }
        GmNameMap a, b, c, d;
        for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
            at<FnAdd>(0x00025EB0)(g_own[0], nullptr, &a, ns);
            g_own[1]->AddMaterials(&b, ns);
            eq("group", a.m_group == &g_own[0]->m_materials, b.m_group == &g_own[1]->m_materials);
            eqs("group's name", group_name(a.m_group).c_str(), group_name(b.m_group).c_str());
            eq("first", g_own[0]->m_materials.m_first == &a, g_own[1]->m_materials.m_first == &b);
            at<FnAdd>(0x00025EB0)(g_own[0], nullptr, &c, ns);
            g_own[1]->AddMaterials(&d, ns);
            eq("second's next", c.m_next == &a, d.m_next == &b);
        }
    }

    // No room: the raise, on all the lists of a read that does not write into them.
    g_context = "no memory";
    {
        Env env({"A"}, {"Space"});
        g_fill = 0xCD;
        for (const char* text : {"StagedShaderTexture[0]\n{\n}\n", "ProgShaderTexture[0]\n{\n}\n", "StagedShaderTextureWithNameSpace[0]\n{\n}\n", "ProgShaderTextureWithNameSpace[0]\n{\n}\n"}) {
            AtMaterialDatabase* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            Direct da(text), db(text);
            g_fail_alloc = true;
            { On on(0); at<FnRead>(0x00039500)(o, nullptr, g_own[0], &da.tokenizer); }
            { On on(1); r->Read(g_own[1], &db.tokenizer); }
            g_fail_alloc = false;
            same_db("no memory", 0, o, r);
            destroy(o, r, "destroy");
        }
        g_db[0][0] = g_db[1][0] = nullptr;
    }

    // No heap.
    g_context = "no heap";
    {
        *(void**)(uintptr_t)0x002C63F0 = nullptr;
        AtMaterialDatabase::s_heap = nullptr;
        for (const char* text : {"StagedShaderTexture[1]\n{\n \"A\"\n}\nProgShaderTexture[1]\n{\n \"A\"\n}\nMaterial[0]\n{\n}\n"}) run_text(bytes(text), "no heap", 0xCD);
        Env env({"A"}, {"Space"});
        AtMaterialDatabase* o = make_o(0, 0xCD);
        TestDb* r = make_r(0, 0xCD);
        { On on(0); at<FnAlloc>(0x00039050)(o, nullptr, g_own[0], 1, 1, 0, nullptr); }
        { On on(1); r->Allocate(g_own[1], 1, 1, 0, nullptr); }
        same_db("allocate", 0, o, r);
        destroy(o, r, "destroy");
        g_db[0][0] = g_db[1][0] = nullptr;
        at<void(__cdecl*)(void*)>(0x00038F70)(&g_heap_token);
        AtMaterialDatabase::SetHeap(&g_heap_token);
        eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002C63F0, (uintptr_t)AtMaterialDatabase::s_heap);
    }

    // A file that will not open: the report, and then the reader on no tokenizer. In the game the report does not come back.
    g_context = "no file";
    {
        Env env({"A"}, {"Space"});
        for (int status : {1, 2, 5}) {
            g_fs->open_status = status;
            AtMaterialDatabase* o = make_o(0, 0xCD);
            TestDb* r = make_r(0, 0xCD);
            LoadArg a = {o, r};
            const bool oc = guarded([&] { load_o(&a); });
            g_recording = false;
            const bool rc = guarded([&] { load_r(&a); });
            g_recording = false;
            eq("crash on no tokenizer", oc, rc);
            compare_logs("no file");
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
    if (pos.size() != 2) { std::printf("usage: oracle_mdb <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C63F0u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtMaterialDatabase::s_heap = &g_heap_token;

    g_heap_pad = 0x10000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_written();
    run_direct();

    g_materials = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AGM\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_text(data, line, 0xCD);
        g_files++;
    }
    std::fclose(index);
    std::printf("copies that ran past a list: %u\n", g_overruns);
    report_entries();
    std::printf("%s: %u files, %u materials; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_materials, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
