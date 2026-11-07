// Differential oracle for the shader loader (Windows x86, local only). See docs/verification.md.
//
//   oracle_ags <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AGS file listed in index.txt, loaded through the file code on both sides; text written here; and each reader called directly on a tokenizer of the harness's, whose Error can stop a
// reader that would otherwise never finish: the readers loop until a closing brace, so at the end of the data they spin if the report comes back. In the game it does not. Textures and processes are
// found in real maps, one owner to a side, filled with every string of the text except names beginning with X.
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
#include "at/AtShaderDatabase.h"
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

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

enum { kPool = 512 };
static unsigned char g_pool[2][kPool][sizeof(AtShader)];
static bool g_made[2][kPool];
static void* g_db[2];
static AtMaterialOwner* g_own[2];

// The original's report, stopping when asked to, as it does in the game.
static bool g_stop_at_fatal;
static void __cdecl o_fatal_stops(const char* m, int a, int b, int c) {
    o_fatal(m, a, b, c);
    if (g_stop_at_fatal) *(volatile int*)8 = 0;
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnDbRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnShaderRead)(void*, void*, void*, void*, void*, const char*);
typedef void(__fastcall* FnStagedRead)(void*, void*, void*, void*, unsigned, const char*);
typedef void(__fastcall* FnPartRead)(void*, void*, void*, void*, const char*);
typedef void(__fastcall* FnWord)(void*, void*, void*);
typedef unsigned(__fastcall* FnCount)(void*, void*);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);

// A shader's virtual Clear is the one call Read makes through its table, so both sides note it.
static void __fastcall o_shader_clear(void* self, void*) {
    g_log[0].add('c');
    at<Fn0>(0x000375A0)(self, nullptr);
}
static const void* g_shader_table[2] = {(const void*)(uintptr_t)0x000379D0, (const void*)o_shader_clear};

class TestShader : public AtShader {
public:
    void Clear() override {
        g_log[1].add('c');
        AtShader::Clear();
    }
};

static void make_items(int side, unsigned count) {
    const bool was = g_recording;
    g_recording = false;
    for (unsigned i = 0; i < std::min(count, (unsigned)kPool); i++) {
        if (g_made[side][i]) continue;  // Left as they are, so that a second load reads over what is there.
        g_made[side][i] = true;
        void* p = g_pool[side][i];
        std::memset(p, 0xCD, sizeof(AtShader));
        if (side == 0) {
            at<FnCtor>(0x00037560)(p, nullptr);
            *(const void***)p = g_shader_table;
        } else {
            new (p) TestShader;
        }
    }
    g_recording = was;
}

static AtShader* item(int side, unsigned i) { return (AtShader*)g_pool[side][std::min(i, (unsigned)kPool - 1)]; }

class TestDb : public AtShaderDatabase {
public:
    void Clear() override {
        g_log[1].add('b');
        AtShaderDatabase::Clear();
    }
    void OnReady() override {
        g_log[1].add('Y', m_count);
        g_log[1].add('y', m_group != nullptr);
        make_items(1, m_count);
    }
    AtShader* ShaderAt(unsigned int i) override {
        g_log[1].add('A', i);
        return item(1, i);
    }
};
static void __fastcall o_db_clear(void* self, void*) {
    g_log[0].add('b');
    at<Fn0>(0x0002B380)(self, nullptr);
}
static void __fastcall o_ready(void* self, void*) {
    g_log[0].add('Y', ((AtShaderDatabase*)self)->m_count);
    g_log[0].add('y', ((AtShaderDatabase*)self)->m_group != nullptr);
    make_items(0, ((AtShaderDatabase*)self)->m_count);
}
static AtShader* __fastcall o_at(void*, void*, unsigned i) {
    g_log[0].add('A', i);
    return item(0, i);
}
static const void* g_db_table[4] = {(const void*)(uintptr_t)0x0002B400, (const void*)o_db_clear, (const void*)o_ready, (const void*)o_at};

static AtShaderDatabase* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtShaderDatabase));
    std::memset(o, fill, sizeof(AtShaderDatabase));
    g_db[0] = o;
    eq("constructor returns this", (uintptr_t)at<FnCtor>(0x0002B1C0)(o, nullptr), (uintptr_t)o);
    *(const void***)o = g_db_table;
    return (AtShaderDatabase*)o;
}
static TestDb* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(TestDb));
    std::memset(r, fill, sizeof(TestDb));
    g_db[1] = r;
    return new (r) TestDb;
}

// ---- comparing --------------------------------------------------------------------------------------------------------

static void same_bytes(const char* what, const void* o, const void* r, size_t n) { eq(what, std::memcmp(o, r, n), 0); }

// Past the array too: a stage list over eight runs on.
static void same_progs(const AtProgShaderComponent* o, const AtProgShaderComponent* r, unsigned count) {
    eq("prog array", o != nullptr, r != nullptr);
    if (!o || !r) return;
    eq("prog cookie", ((const unsigned*)o)[-1], ((const unsigned*)r)[-1]);
    const size_t n = count * sizeof(AtProgShaderComponent) + 0x20;
    std::vector<unsigned char> a((const unsigned char*)o, (const unsigned char*)o + n), b((const unsigned char*)r, (const unsigned char*)r + n);
    for (unsigned i = 0; i < count; i++) {
        eq("prog's list", o[i].m_vertexProcesses != nullptr, r[i].m_vertexProcesses != nullptr);
        if (o[i].m_vertexProcesses && r[i].m_vertexProcesses && o[i].m_vertexProcessCount == r[i].m_vertexProcessCount) {
            same_bytes("prog's list", o[i].m_vertexProcesses, r[i].m_vertexProcesses, o[i].m_vertexProcessCount * sizeof(void*));
        }
        std::memset(a.data() + i * sizeof(AtProgShaderComponent) + offsetof(AtProgShaderComponent, m_vertexProcesses), 0, sizeof(void*));
        std::memset(b.data() + i * sizeof(AtProgShaderComponent) + offsetof(AtProgShaderComponent, m_vertexProcesses), 0, sizeof(void*));
    }
    same_bytes("prog components", a.data(), b.data(), n);
}

static void same_stageds(const AtStagedShaderComponent* o, const AtStagedShaderComponent* r, unsigned count) {
    eq("staged array", o != nullptr, r != nullptr);
    if (!o || !r) return;
    eq("staged cookie", ((const unsigned*)o)[-1], ((const unsigned*)r)[-1]);
    same_bytes("staged components", o, r, count * sizeof(AtStagedShaderComponent));
}

static void same_shader(const AtShader* o, const AtShader* r) {
    eq("prog count", o->m_progCount, r->m_progCount);
    eq("staged count", o->m_stagedCount, r->m_stagedCount);
    eq("transform count", o->m_transformCount, r->m_transformCount);
    same_bytes("transforms", o->m_transforms, r->m_transforms, sizeof o->m_transforms);
    if (o->m_progCount == r->m_progCount) same_progs(o->m_progs, r->m_progs, o->m_progCount);
    if (o->m_stagedCount == r->m_stagedCount) same_stageds(o->m_stageds, r->m_stageds, o->m_stagedCount);
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static void same_owner() {
    AtMaterialOwner* a = g_own[0];
    AtMaterialOwner* b = g_own[1];
    eqs("last texture space", group_name(a->m_textureSpaces.m_last).c_str(), group_name(b->m_textureSpaces.m_last).c_str());
    eqs("last shader space", group_name(a->m_shaderSpaces.m_last).c_str(), group_name(b->m_shaderSpaces.m_last).c_str());
    eq("shader spaces in use", a->m_shaderSpaces.m_free != nullptr ? a->m_shaderSpaces.m_free - a->m_shaderSpaces.m_pool : -1, b->m_shaderSpaces.m_free != nullptr ? b->m_shaderSpaces.m_free - b->m_shaderSpaces.m_pool : -1);
}

static void same_db(const char* what, const AtShaderDatabase* o, const AtShaderDatabase* r) {
    g_recording = false;
    eq("count", o->m_count, r->m_count);
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's shader", (char*)x.m_value - (char*)g_pool[0], (char*)y.m_value - (char*)g_pool[1]);
            }
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &g_own[0]->m_shaders, r->m_group == &g_own[1]->m_shaders);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    for (unsigned i = 0; i < kPool; i++) {
        if (!g_made[0][i] || !g_made[1][i]) continue;
        same_shader(item(0, i), item(1, i));
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
            w.m_materials.Add(fill(4));
            w.m_vertexProcesses.Add(fill(5));
            w.m_pixelProcesses.Add(fill(6));
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
static void gather(const std::string& d, std::set<std::string>& names, std::set<std::string>& spaces) {
    size_t i = 0;
    bool last_was_string = false;
    while (i < d.size()) {
        const unsigned char c = d[i];
        if (c == '\n') { last_was_string = false; i++; continue; }
        if (c == '/' && i + 1 < d.size() && d[i + 1] == '/') { while (i < d.size() && d[i] != '\n') i++; continue; }
        if (c == '"') {
            size_t e = i + 1;
            while (e < d.size() && d[e] != '"' && d[e] != '\n') e++;
            std::string s(d.data() + i + 1, e - i - 1);
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

// ---- through the file code ------------------------------------------------------------------------------------------------

static unsigned g_files, g_shaders, g_stageds, g_progs;

struct LoadArg { AtShaderDatabase* o; TestDb* r; };

static void load_o(void* p) { LoadArg* a = (LoadArg*)p; On on(0); at<FnLoad>(0x0002B2E0)(a->o, nullptr, g_own[0], "MODEL\\THING", 77); }
static void load_r(void* p) { LoadArg* a = (LoadArg*)p; On on(1); a->r->Load(g_own[1], "MODEL\\THING", 77); }

static void destroy_items(const char* what) {
    for (unsigned i = 0; i < kPool; i++) {
        if (g_made[0][i]) {
            On on(0);
            eq("shader destructor returns this", (uintptr_t)at<FnDel>(0x000379D0)(g_pool[0][i], nullptr, 0), (uintptr_t)g_pool[0][i]);
        }
        if (g_made[1][i]) {
            On on(1);
            ((TestShader*)g_pool[1][i])->~TestShader();
        }
        if (g_made[0][i] && g_made[1][i]) {
            same_bytes("destroyed shader", g_pool[0][i] + 4, g_pool[1][i] + 4, sizeof(AtShader) - 4);
        }
        g_made[0][i] = g_made[1][i] = false;
    }
    compare_logs(what);
}

static void destroy(AtShaderDatabase* o, TestDb* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002B400)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~TestDb(); }
    compare_logs(what);
    std::free(o);
    std::free(r);
    g_db[0] = g_db[1] = nullptr;
}

// What only the graphics layer calls, on what was read. These may run over: see below.
static void note_transforms(const char* what) {
    for (unsigned i = 0; i < kPool; i++) {
        if (!g_made[0][i] || !g_made[1][i]) continue;
        { On on(0); at<Fn0>(0x00037680)(item(0, i), nullptr); }
        { On on(1); item(1, i)->NoteProgTransforms(); }
        same_shader(item(0, i), item(1, i));
        { On on(0); at<Fn0>(0x000376F0)(item(0, i), nullptr); }
        { On on(1); item(1, i)->NoteStagedTransforms(); }
        same_shader(item(0, i), item(1, i));
        for (unsigned j = 0; j < item(0, i)->m_progCount && j < item(1, i)->m_progCount; j++) {
            eq("textures", at<FnCount>(0x000453B0)(&item(0, i)->m_progs[j], nullptr), item(1, i)->m_progs[j].CountTextures());
        }
    }
    compare_logs(what);
}

static void run_text(const std::string& data, const std::string& context, unsigned char fill) {
    g_context = context;
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    AtShaderDatabase* o = make_o(fill);
    TestDb* r = make_r(fill);
    same_bytes("constructed", (char*)o + 4, (char*)r + 4, sizeof(AtShaderDatabase) - 4);
    LoadArg a = {o, r};
    load_o(&a);
    load_r(&a);
    same_db("load", o, r);
    g_shaders += r->m_count;
    for (unsigned i = 0; i < std::min(r->m_count, (unsigned)kPool); i++) {
        g_stageds += item(1, i)->m_stagedCount;
        g_progs += item(1, i)->m_progCount;
    }
    note_transforms("transforms");
    // Over what is there: the database clears itself, and each shader clears itself.
    load_o(&a);
    load_r(&a);
    same_db("load again", o, r);
    { On on(0); o_db_clear(o, nullptr); o_db_clear(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_db("clear", o, r);
    destroy_items("destroy the shaders");
    destroy(o, r, "destroy");
}

static std::string staged(const std::string& body) {
    return "StagedShaderComponent\n{\n" + body + "\n}\n";
}
static std::string stage(const std::string& body) {
    return "TextureStage\n{\n" + body + "\n}\n";
}
static std::string one_shader(const std::string& body) {
    return "Shader[1]\n{\nShader \"S\"\n{\n" + body + "\n}\n}\n";
}
static std::string with_stageds(const std::vector<std::string>& bodies) {
    std::string s = "StagedShaderComponent[" + std::to_string(bodies.size()) + "]\n{\n";
    for (const std::string& b : bodies) s += staged(b);
    return one_shader(s + "}\n");
}
static std::string prog(const std::string& body) {
    return "ProgShaderComponent\n{\n" + body + "\n}\n";
}
static std::string with_progs(const std::vector<std::string>& bodies) {
    std::string s = "ProgShaderComponent[" + std::to_string(bodies.size()) + "]\n{\n";
    for (const std::string& b : bodies) s += prog(b);
    return one_shader(s + "}\n");
}

static void run_written() {
    std::vector<std::string> texts = {
        "",
        "// nothing but a comment\n",
        "Shader[0]\n{\n}\n",
        "Shader[1]\n{\nShader \"A\"\n{\n}\n}\n",
        "Shader[2]\n{\nShader \"A\"\n{\n}\nShader \"B\"\n{\n}\n}\n",
        "Shader[2]\n{\nShader \"A\"\n{\n}\nShader \"A\"\n{\n}\n}\n",
        "Shader[1]\n{\nShader \"LongerThanEight\"\n{\n}\n}\n",
        "Shader[1]\n{\nShader \"EightCha\"\n{\n}\n}\n",
        "Shader[1]\n{\nAdd \"A\"\n{\n}\n}\n",
        "Shader[1]\n{\nShader \"A\"\n{\n}\n",
        "Shader[1]\n{\nShader \"A\"\n{\n}\n}\nShader[1]\n{\nShader \"B\"\n{\n}\n}\n",
        // A duplicate's report comes before what its shader's own reading reports.
        "Shader[2]\n{\nShader \"A\"\n{\nAdd\n}\nShader \"A\"\n{\nAdd\n}\n}\n",
        "Add[0]\n{\n}\n",
        "[0]\n{\n}\n",
        "Shader\n{\n}\n",
        "NameSpace \"Other\"\nShader[1]\n{\nShader \"A\"\n{\n}\n}\n",
        "NameSpace \"\"\nShader[1]\n{\nShader \"A\"\n{\n}\n}\n",
        "NameSpace \"XNoSpace\"\nShader[1]\n{\nShader \"A\"\n{\n}\n}\n",
        "NameSpace \"LongerThanEight\"\nShader[0]\n{\n}\n",
        "NameSpace \"Other\"\nAdd[0]\n{\n}\n",
        "NameSpace \"Other\"\nNameSpace[0]\n{\n}\n",
        // Whatever a shader's block holds.
        one_shader("Add"),
        one_shader("5"),
        one_shader("\"Name\""),
        one_shader("StagedShaderComponent[0]\n{\n}\n"),
        one_shader("ProgShaderComponent[0]\n{\n}\n"),
        one_shader("StagedShaderComponent[1]\n{\n" + staged("") + "}\nStagedShaderComponent[1]\n{\n" + staged("") + "}\n"),
        one_shader("ProgShaderComponent[1]\n{\n" + prog("") + "}\nProgShaderComponent[1]\n{\n" + prog("") + "}\n"),
        one_shader("StagedShaderComponent[1]\n{\n" + staged("") + "}\nProgShaderComponent[2]\n{\n" + prog("") + prog("") + "}\n"),
        one_shader("ProgShaderComponent[1]\n{\n" + prog("") + "}\nStagedShaderComponent[2]\n{\n" + staged("") + staged("") + "}\n"),
        one_shader("StagedShaderComponent[1]\n{\n" + staged("") + staged("") + "}\n"),
        one_shader("StagedShaderComponent[2]\n{\n" + staged("") + "}\n}\n{\n}\n{\n"),
        one_shader("StagedShaderComponent[1]\n{\nProgShaderComponent\n{\n}\n}\n"),
        one_shader("ProgShaderComponent[1]\n{\nStagedShaderComponent\n{\n}\n}\n"),
        // A staged component's words, and those it refuses.
        with_stageds({"CombineOp Add", "CombineOp BlendAlpha", "CombineOp BlendDestAlpha", "CombineOp Modulate", "CombineOp Modulate2x", "CombineOp ModulateByAlpha", "CombineOp ModulateByAlpha2x", "CombineOp ModulateByAlphaAddColor", "CombineOp Subtract", "CombineOp BlendInverseAlpha", "CombineOp Wrap", "CombineOp StagedShaderComponent", "CombineOp 5"}),
        with_stageds({"ModulateOp SelectConstant", "ModulateOp SelectDiffuse", "ModulateOp SelectTexture", "ModulateOp SelectModulatedOutput", "ModulateOp ModulateTextureByConstant", "ModulateOp ModulateTextureByDiffuse", "ModulateOp ModulateTextureByDiffuseAndConstant", "ModulateOp Add", "ModulateOp CubeNormalizationTexture", "ModulateOp 5"}),
        with_stageds({"AlphaSource SelectConstant", "AlphaSource SelectDiffuse", "AlphaSource SelectTexture", "AlphaSource SelectModulatedOutput", "AlphaSource ModulateTextureByConstant", "AlphaSource BlendInverseAlpha", "AlphaSource 5"}),
        with_stageds({"FlipFaceCulling", "DisableDepthBuffer", "DisableDepthWrites", "DisableColorWrites", "FlipFaceCulling\nDisableColorWrites\nDisableDepthBuffer"}),
        with_stageds({"Shader", "VertexProcess", "PixelProcess", "Add", "Texture", "5", "\"Name\"", "CubeNormalizationTexture", "ProgShaderComponent", "StagedShaderComponent", "AlphaSource SelectTexture\nModulateOp SelectDiffuse\nModulateOp SelectConstant\nAlphaSource SelectConstant\nCombineOp Add\nCombineOp Subtract"}),
        with_stageds({stage("") + stage("Clamp")}),
        // A stage's words.
        with_stageds({stage("Texture \"A\""), stage("Texture \"XGone\""), stage("Texture \"LongerThanEight\""), stage("Texture \"XightCha\""), stage("Texture \"A\"\nTexture \"B\""), stage("CubeNormalizationTexture"), stage("CubeNormalizationTexture\nTexture \"A\""), stage("Texture \"A\"\nCubeNormalizationTexture"), stage("Texture \"XGone\"\nCubeNormalizationTexture"), stage("CubeNormalizationTexture\nTextureWithNameSpace \"A\" \"Space\"")}),
        with_stageds({stage("TextureWithNameSpace \"A\" \"Space\""), stage("TextureWithNameSpace \"A\" \"\""), stage("TextureWithNameSpace \"XGone\" \"Space\""), stage("TextureWithNameSpace \"XGone\" \"\""), stage("TextureWithNameSpace \"A\" \"XNoSpace\""), stage("TextureWithNameSpace \"A\" \"Other\"\nTexture \"A\""), stage("TextureWithNameSpace \"LongName1\" \"Other\"")}),
        "NameSpace \"Other\"\n" + with_stageds({stage("Texture \"A\""), stage("Texture \"XGone\""), stage("TextureWithNameSpace \"A\" \"Space\""), stage("TextureWithNameSpace \"A\" \"\"")}),
        "NameSpace \"\"\n" + with_stageds({stage("Texture \"A\""), stage("TextureWithNameSpace \"A\" \"Space\"")}),
        "NameSpace \"XNoSpace\"\n" + with_stageds({stage("Texture \"A\"")}),
        with_stageds({stage("TxtCoordOffset 3"), stage("TxtCoordOffset 70000"), stage("TxtCoordOffset -1"), stage("TxtCoordOffset \"x\""), stage("TxtCoordOffset 1.5"), stage("TxtCoordOffset 3\nTxtCoordOffset 4")}),
        with_stageds({stage("Anisotropic 2.5"), stage("Anisotropic 3"), stage("Anisotropic \"x\""), stage("Anisotropic 1.1\nAnisotropic 7.25"), stage("Anisotropic 0.1\nPointSampled")}),
        with_stageds({stage("Border 1 2 3 4"), stage("Border 300 -1 0 255"), stage("Border 1 2\n}\n}"), stage("Clamp\nBorder 5 6 7 8\nWrap")}),
        with_stageds({stage("Coord2DTransform 3"), stage("Coord2DTransform 300"), stage("Coord3DTransform 511"), stage("Coord4DTransform 256"), stage("Coord2DTransform \"x\""), stage("Coord3DTransform 1.5"), stage("Coord4DTransform 7")}),
        with_stageds({stage("MipNearest"), stage("MipLinear"), stage("MipNearest\nMipLinear"), stage("MipLinear\nMipNearest"), stage("CoordProjected"), stage("CoordProjected\nCoordProjected")}),
        with_stageds({stage("Add"), stage("5"), stage("\"Name\""), stage("TextureStage"), stage("PixelProcess"), stage("DisableColorWrites"), stage("FlipFaceCulling"), stage("StagedShaderComponent"), stage("AlphaSource"), stage("ModulateTextureByDiffuseAndConstant"), stage("BlendInverseAlpha")}),
        // More transforms than the list has room for, in order: the ninth goes over the count, and what follows lands where that says. Values kept small, to stay inside the object.
        with_stageds({stage("Coord2DTransform 1"), stage("Coord2DTransform 2"), stage("Coord2DTransform 3"), stage("Coord2DTransform 4"), stage("Coord3DTransform 5"), stage("Coord3DTransform 6"), stage("Coord4DTransform 7"), stage("Coord4DTransform 1"), stage("Coord2DTransform 2"), stage("Coord2DTransform 3"), stage("Coord2DTransform 1")}),
        with_stageds({stage("CoordViewToWorldTransform"), stage("Coord2DTransform 0"), stage("CoordViewToWorldTransform\nCoord2DTransform 1"), stage("Coord4DTransform 2\nCoordViewToWorldTransform"), stage("MipLinear")}),
        // A programmable component's words.
        with_progs({"CombineOp Add", "CombineOp BlendAlpha", "CombineOp Subtract", "CombineOp BlendInverseAlpha", "CombineOp Wrap"}),
        with_progs({"FlipFaceCulling", "DisableDepthBuffer", "DisableDepthWrites", "DisableColorWrites", "DisableColorWrites\nFlipFaceCulling", "FlipFaceCulling\nDisableColorWrites"}),
        with_progs({"Add", "5", "\"Name\"", "Texture", "ModulateOp", "AlphaSource", "CubeNormalizationTexture", "StagedShaderComponent"}),
        with_progs({"VertexProcess[1]\n{\n\"V\"\n}", "VertexProcess[3]\n{\n\"V\"\n\"W\"\n\"U\"\n}", "VertexProcess[2]\n{\n\"V\"\n\"XGone\"\n}", "VertexProcess[1]\n{\n\"LongerThanEight\"\n}", "VertexProcess[1]\n{\n\"XightCha\"\n}"}),
        // A closing brace taken early, with as many more after it: unbalanced text would spin at the end of the data, which only the direct cases can stop.
        with_progs({"VertexProcess[1]\n{\n\"V\"\n\"W\"\n}\n{\n"}),
        with_progs({"VertexProcess[0]\n{\n}", "VertexProcess[256]\n{\n}", "VertexProcess[257]\n{\n\"V\"\n}", "VertexProcess[1]\n{\n\"V\"\n}\nVertexProcess[1]\n{\n\"W\"\n}", "VertexProcess[0]\n{\n}\nVertexProcess[1]\n{\n\"W\"\n}"}),
        with_progs({"PixelProcess \"P\"", "PixelProcess \"LongerThanEight\"", "PixelProcess \"P\"\nPixelProcess \"Q\"", "VertexProcess[1]\n{\n\"P\"\n}\nPixelProcess \"P\""}),
        with_progs({"TextureStage[0]\n{\n}", "TextureStage[1]\n{\n" + stage("Texture \"A\"") + "}", "TextureStage[2]\n{\n" + stage("Texture \"A\"\nCoord2DTransform 1") + stage("Clamp\nCoord3DTransform 2") + "}", "TextureStage[1]\n{\n" + stage("") + "}\nTextureStage[1]\n{\n" + stage("Texture \"B\"") + "}", "TextureStage[256]\n{\n}", "TextureStage[257]\n{\n" + stage("Wrap") + "}", "TextureStage[1]\n{\nAdd\n{\n}\n}"}),
        with_progs({"TextureStage[8]\n{\n" + stage("Texture \"A\"\nCoord2DTransform 1") + stage("Texture \"B\"\nCoord3DTransform 2") + stage("Coord4DTransform 3") + stage("Texture \"A\"\nCoordViewToWorldTransform") + stage("Texture \"B\"") + stage("Texture \"C\"\nCoord2DTransform 4") + stage("") + stage("Texture \"A\"\nCoord2DTransform 5") + "}"}),
        with_progs({"TextureStage[1]\n{\n" + stage("Coord2DTransform 1") + "}", "TextureStage[2]\n{\n" + stage("Coord2DTransform 2") + stage("Coord2DTransform 3") + "}", "TextureStage[8]\n{\n" + stage("Coord2DTransform 4") + stage("Coord2DTransform 5") + stage("Coord3DTransform 6") + stage("Coord3DTransform 7") + stage("Coord4DTransform 1") + stage("Coord2DTransform 2") + stage("Coord2DTransform 3") + stage("Coord2DTransform 1") + "}"}),
        // Over eight stages reports and reads on over what follows. A ninth with a texture would be freed as a list, so it has none.
        with_progs({"TextureStage[9]\n{\n" + stage("") + stage("") + stage("") + stage("") + stage("") + stage("") + stage("") + stage("") + stage("Clamp") + "}", "CombineOp Add"}),
        with_progs({"TextureStage[9]\n{\n" + stage("") + stage("") + stage("") + stage("") + stage("") + stage("") + stage("") + stage("") + stage("Anisotropic 1.5") + "}"}),
        "NameSpace \"Other\"\n" + with_progs({"TextureStage[2]\n{\n" + stage("Texture \"A\"") + stage("Texture \"XGone\"") + "}", "VertexProcess[1]\n{\n\"V\"\n}\nPixelProcess \"P\""}),
        one_shader("ProgShaderComponent[1]\n{\n" + prog("CombineOp Modulate\nVertexProcess[1]\n{\n\"V\"\n}\nPixelProcess \"P\"\nTextureStage[1]\n{\n" + stage("Texture \"A\"\nCoord2DTransform 3") + "}") + "}\nStagedShaderComponent[1]\n{\n" + staged("TextureStage\n{\nCoord3DTransform 2\n}") + "}\n"),
    };
    // A list count with its top bit set.
    {
        std::string list;
        for (int i = 0; i < 128; i++) list += "\"V\"\n";
        texts.push_back(with_progs({"VertexProcess[128]\n{\n" + list + "}"}));
    }
    // Every pair in each group of a stage's words, each way round, one to a component.
    const std::vector<std::vector<std::string>> groups = {
        {"PointSampled", "Bilinear", "Anisotropic 2", "FlatCubic", "GaussianCubic"},
        {"Wrap", "Clamp", "Border 1 2 3 4"},
        {"CoordGenCameraSpaceNormal", "CoordGenCameraSpaceNormalWithScaleAndBias", "CoordGenCameraSpacePos", "CoordGenCameraSpaceReflection", "CoordGenCameraSpaceReflectionWithScaleAndBias", "CoordGenCameraSpaceToonLighting"},
        {"CoordViewToWorldTransform", "Coord2DTransform 1", "Coord3DTransform 2", "Coord4DTransform 3"},
    };
    for (const auto& g : groups) {
        for (const std::string& a : g) {
            std::vector<std::string> bodies = {stage(a)};
            for (const std::string& b : g) bodies.push_back(stage(a + "\n" + b));
            texts.push_back(with_stageds(bodies));
        }
    }
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const std::string& t : texts) run_text(t, "text [" + t + "]", fill);
    }
}

// ---- entries called directly -------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// Both sides read through this, so the words are read alike and the report can be stopped.
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
    explicit Direct(const std::string& t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(AtShaderDatabase::s_keywords, AtShaderDatabase::kKeywordCount, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

// One reader on one object a side, both from raw memory of the same bytes, compared whole.
template <class Make, class Original, class Recovered>
static void read_part(const std::string& text, size_t size, Make make, Original original, Recovered recovered, const char* what) {
    g_context = std::string(what) + " [" + text + "]";
    std::vector<unsigned char> a(size + 0x40, 0xEE), b(size + 0x40, 0xEE);
    make(a.data(), b.data());
    Direct da(text), db(text);
    const bool oc = guarded([&] { On on(0); original(a.data(), &da.tokenizer); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); recovered(b.data(), &db.tokenizer); });
    g_recording = false;
    g_side = 0;
    eq("crash", oc, rc);
    same_bytes(what, a.data(), b.data(), a.size());
    compare_logs(what);
    eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
}

static void run_direct() {
    g_fill = 0xCD;
    const std::set<std::string> names = {"A", "B", "V", "P"}, spaces = {"Space", "Other"};
    Env env(names, spaces);

    // The table, against the image.
    g_context = "keywords";
    {
        const AtIoKeyword* image = (const AtIoKeyword*)(uintptr_t)0x002B0CC0;
        for (unsigned i = 0; i < AtShaderDatabase::kKeywordCount; i++) {
            eq("keyword id", image[i].m_id, AtShaderDatabase::s_keywords[i].m_id);
            eqs("keyword", image[i].m_name, AtShaderDatabase::s_keywords[i].m_name);
        }
    }

    // Each small reader, on each word it can see and on others.
    static const char* const words[] = {"", "Add", "BlendAlpha", "BlendDestAlpha", "Modulate", "Modulate2x", "ModulateByAlpha", "ModulateByAlpha2x", "ModulateByAlphaAddColor", "Subtract", "BlendInverseAlpha", "CoordProjected", "StagedShaderComponent",
                                       "SelectConstant", "SelectDiffuse", "SelectTexture", "SelectModulatedOutput", "ModulateTextureByConstant", "ModulateTextureByDiffuse", "ModulateTextureByDiffuseAndConstant", "AlphaSource", "CubeNormalizationTexture", "5", "1.5", "\"Name\"", "{", "}"};
    for (const char* w : words) {
        const auto raw = [](void*, void*) {};
        const auto ctor_part = [](void* o, void* r) {
            at<FnCtor>(0x00055630)(o, nullptr);
            new (r) AtShaderCombineOp;
        };
        read_part(w, sizeof(AtShaderCombineOp), ctor_part, [](void* o, AtIoTokenizer* t) { at<FnWord>(0x00055640)(o, nullptr, t); }, [](void* r, AtIoTokenizer* t) { ((AtShaderCombineOp*)r)->Read(t); }, "combine op");
        read_part(w, sizeof(AtShaderCombineOp), raw, [](void* o, AtIoTokenizer* t) { at<FnWord>(0x00055640)(o, nullptr, t); }, [](void* r, AtIoTokenizer* t) { ((AtShaderCombineOp*)r)->Read(t); }, "combine op over raw");
        read_part(w, sizeof(AtStagedShaderComponent), raw, [](void* o, AtIoTokenizer* t) { at<FnWord>(0x00044E00)(o, nullptr, t); }, [](void* r, AtIoTokenizer* t) { ((AtStagedShaderComponent*)r)->ReadModulateOp(t); }, "modulate op");
        read_part(w, sizeof(AtStagedShaderComponent), raw, [](void* o, AtIoTokenizer* t) { at<FnWord>(0x00044E80)(o, nullptr, t); }, [](void* r, AtIoTokenizer* t) { ((AtStagedShaderComponent*)r)->ReadAlphaSource(t); }, "alpha source");
    }

    // Each object from raw memory, of two fills. A vftable differs, so the shader is compared past it.
    for (const unsigned char fill : {(unsigned char)0xEE, (unsigned char)0x11}) {
        g_context = "constructors";
        unsigned char a[0x200], b[0x200];
        const auto start = [&]() { std::memset(a, fill, sizeof a); std::memset(b, fill, sizeof b); };
        start();
        eq("returns this", (uintptr_t)at<FnCtor>(0x00055630)(a, nullptr), (uintptr_t)a);
        new (b) AtShaderCombineOp;
        same_bytes("combine op", a, b, sizeof a);
        start();
        eq("returns this", (uintptr_t)at<FnCtor>(0x00044940)(a, nullptr), (uintptr_t)a);
        new (b) AtTextureStage;
        same_bytes("stage", a, b, sizeof a);
        start();
        at<Fn0>(0x00044DB0)(a, nullptr);
        ((AtTextureStage*)b)->Reset();
        same_bytes("stage reset", a, b, sizeof a);
        start();
        eq("returns this", (uintptr_t)at<FnCtor>(0x00044EE0)(a, nullptr), (uintptr_t)a);
        new (b) AtStagedShaderComponent;
        same_bytes("staged", a, b, sizeof a);
        at<Fn0>(0x00044DE0)(a, nullptr);
        ((AtStagedShaderComponent*)b)->Clear();
        same_bytes("staged cleared", a, b, sizeof a);
        start();
        at<Fn0>(0x00044DE0)(a, nullptr);
        ((AtStagedShaderComponent*)b)->Clear();
        same_bytes("staged cleared from raw", a, b, sizeof a);
        start();
        at<Fn0>(0x00037550)(a, nullptr);
        ((AtStagedShaderComponent*)b)->~AtStagedShaderComponent();
        same_bytes("staged destroyed from raw", a, b, sizeof a);
        start();
        eq("returns this", (uintptr_t)at<FnCtor>(0x00045030)(a, nullptr), (uintptr_t)a);
        new (b) AtProgShaderComponent;
        same_bytes("prog", a, b, sizeof a);
        start();
        at<Fn0>(0x00037530)(a, nullptr);
        ((AtProgShaderComponent*)b)->~AtProgShaderComponent();
        same_bytes("prog destroyed from raw", a, b, sizeof a);
        start();
        ((AtProgShaderComponent*)a)->m_vertexProcesses = nullptr;
        ((AtProgShaderComponent*)b)->m_vertexProcesses = nullptr;
        { On on(0); at<Fn0>(0x00045370)(a, nullptr); }
        { On on(1); ((AtProgShaderComponent*)b)->Clear(); }
        same_bytes("prog cleared from raw", a, b, sizeof a);
        compare_logs("prog cleared from raw");
        // And with a list, which goes.
        ((AtProgShaderComponent*)a)->m_vertexProcesses = (void**)std::malloc(4);
        ((AtProgShaderComponent*)b)->m_vertexProcesses = (void**)std::malloc(4);
        { On on(0); at<Fn0>(0x00045370)(a, nullptr); }
        { On on(1); ((AtProgShaderComponent*)b)->Clear(); }
        same_bytes("prog's list cleared", a, b, sizeof a);
        compare_logs("prog's list cleared");
        start();
        eq("returns this", (uintptr_t)at<FnCtor>(0x00037560)(a, nullptr), (uintptr_t)a);
        eq("vftable", *(uint32_t*)a, 0x00274D6C);
        new (b) AtShader;
        same_bytes("shader", a + 4, b + 4, sizeof a - 4);
        // The complete destructor, and the deleting one with its delete.
        { On on(0); at<Fn0>(0x00037730)(a, nullptr); }
        { On on(1); ((AtShader*)b)->~AtShader(); }
        same_bytes("shader destroyed", a + 4, b + 4, sizeof a - 4);
        eq("vftable", *(uint32_t*)a, 0x00274D6C);
        compare_logs("shader destroyed");
    }
    {
        g_context = "deleting";
        void* a = std::malloc(sizeof(AtShader));
        at<FnCtor>(0x00037560)(a, nullptr);
        { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x000379D0)(a, nullptr, 1), (uintptr_t)a); }
        AtShader* b;
        { On on(1); b = new AtShader; g_log[1].n = 0; delete b; }
        compare_logs("shader deleted");
        void* c = std::malloc(sizeof(AtShaderDatabase));
        at<FnCtor>(0x0002B1C0)(c, nullptr);
        { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002B400)(c, nullptr, 1), (uintptr_t)c); }
        TestDb* d;
        { On on(1); d = new TestDb; g_log[1].n = 0; delete d; }
        compare_logs("database deleted");
    }
    {
        g_context = "complete database destructor";
        std::vector<unsigned char> a(sizeof(AtShaderDatabase), 0xEE), b(sizeof(AtShaderDatabase), 0xEE);
        at<FnCtor>(0x0002B1C0)(a.data(), nullptr);
        AtShaderDatabase* r = new (b.data()) TestDb;
        ((AtShaderDatabase*)a.data())->m_count = 5;
        r->m_count = 5;
        { On on(0); at<Fn0>(0x0002B3A0)(a.data(), nullptr); }
        { On on(1); r->~AtShaderDatabase(); }
        same_bytes("destroyed", a.data() + 4, b.data() + 4, a.size() - 4);
        compare_logs("complete database destructor");
    }

    // A stage on its own, under each name space, and through the end of the data.
    static const char* const stage_bodies[] = {
        "{}", "{ Texture \"A\" }", "{ Texture \"XGone\" }", "{ TextureWithNameSpace \"A\" \"Space\" }", "{ TextureWithNameSpace \"A\" \"Other\" }", "{ TextureWithNameSpace \"A\" \"\" }", "{ TextureWithNameSpace \"A\" \"XNope\" }", "{ Coord2DTransform 3 }", "{ Border 1 2 3 4 }",
        "", "{", "{ Bilinear", "Bilinear }", "{ Texture \"A\"", "{ Add", "{ 5 5 5",
    };
    for (const char* text : stage_bodies) {
        for (const char* ns : {(const char*)nullptr, "", "Space", "Other", "XNope"}) {
            const auto ctor = [](void* o, void* r) {
                at<FnCtor>(0x00044940)(o, nullptr);
                new (r) AtTextureStage;
            };
            read_part(text, sizeof(AtTextureStage), ctor, [=](void* o, AtIoTokenizer* t) { at<FnPartRead>(0x00044970)(o, nullptr, g_own[0], t, ns); }, [=](void* r, AtIoTokenizer* t) { ((AtTextureStage*)r)->Read(g_own[1], t, ns); }, "stage");
            read_part(text, sizeof(AtTextureStage), [](void*, void*) {}, [=](void* o, AtIoTokenizer* t) { at<FnPartRead>(0x00044970)(o, nullptr, g_own[0], t, ns); }, [=](void* r, AtIoTokenizer* t) { ((AtTextureStage*)r)->Read(g_own[1], t, ns); }, "stage over raw");
        }
    }

    // A staged component on its own. The index does nothing, on either side.
    static const char* const staged_bodies[] = {
        "StagedShaderComponent {}", "StagedShaderComponent { CombineOp Add }", "StagedShaderComponent { TextureStage { Texture \"A\" } }", "StagedShaderComponent { ModulateOp SelectDiffuse AlphaSource SelectConstant FlipFaceCulling }",
        "Shader {}", "", "StagedShaderComponent", "StagedShaderComponent {", "StagedShaderComponent { Add", "StagedShaderComponent { TextureStage {", "{ }",
    };
    for (const char* text : staged_bodies) {
        for (const char* ns : {(const char*)nullptr, "", "Space"}) {
            for (const unsigned index : {0u, 7u}) {
                const auto ctor = [](void* o, void* r) {
                    at<FnCtor>(0x00044EE0)(o, nullptr);
                    new (r) AtStagedShaderComponent;
                };
                read_part(text, sizeof(AtStagedShaderComponent), ctor, [=](void* o, AtIoTokenizer* t) { at<FnStagedRead>(0x00044F10)(o, nullptr, g_own[0], t, index, ns); }, [=](void* r, AtIoTokenizer* t) { ((AtStagedShaderComponent*)r)->Read(g_own[1], t, index, ns); }, "staged");
            }
        }
    }

    // A programmable one on its own, and with the list's room refused. The array's lists are compared by value, so here they stay out of the bytes.
    static const char* const prog_bodies[] = {
        "ProgShaderComponent {}", "ProgShaderComponent { CombineOp Add }", "ProgShaderComponent { TextureStage[1] { TextureStage { Texture \"A\" } } }", "ProgShaderComponent { PixelProcess \"P\" }",
        "Shader {}", "", "ProgShaderComponent", "ProgShaderComponent {", "ProgShaderComponent { Add", "ProgShaderComponent { TextureStage[1] { TextureStage {", "{ }", "ProgShaderComponent { TextureStage[128] { } } }",
    };
    for (const char* text : prog_bodies) {
        for (const char* ns : {(const char*)nullptr, "", "Space"}) {
            const auto ctor = [](void* o, void* r) {
                at<FnCtor>(0x00045030)(o, nullptr);
                new (r) AtProgShaderComponent;
            };
            read_part(text, sizeof(AtProgShaderComponent), ctor, [=](void* o, AtIoTokenizer* t) { at<FnPartRead>(0x00045070)(o, nullptr, g_own[0], t, ns); }, [=](void* r, AtIoTokenizer* t) { ((AtProgShaderComponent*)r)->Read(g_own[1], t, ns); }, "prog");
        }
    }
    for (const char* text : {"ProgShaderComponent { VertexProcess[0] { } }", "ProgShaderComponent { VertexProcess[1] { \"V\" } }"}) {
        g_context = std::string("prog, no room [") + text + "]";
        AtProgShaderComponent o, r;
        { const bool was = g_recording; g_recording = false; at<FnCtor>(0x00045030)(&o, nullptr); g_recording = was; }
        Direct da(text), db(text);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnPartRead>(0x00045070)(&o, nullptr, g_own[0], &da.tokenizer, nullptr); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r.Read(g_own[1], &db.tokenizer, nullptr); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        eq("list", o.m_vertexProcesses != nullptr, r.m_vertexProcesses != nullptr);
        compare_logs("prog, no room");
    }

    // A pixel process not found. The original's buffer for this one message is too small: 72 bytes in 52, over its saved registers. It does no harm in the game, whose report does not come back, but
    // a report that returns leaves the original crashing in whatever it does next. So on that side the report stops, here, and what is compared is everything up to it. The recovered source's buffer
    // has room, and it goes on to the closing brace. That is the one deliberate difference from the original here: see the notes.
    for (const char* text : {"ProgShaderComponent { PixelProcess \"XGone\" }", "ProgShaderComponent { CombineOp Add PixelProcess \"XightCha\" }", "ProgShaderComponent { PixelProcess \"XLongerThanEight\" }"}) {
        g_context = std::string("pixel not found [") + text + "]";
        AtProgShaderComponent o, r;
        Direct da(text), db(text);
        g_stop_at_fatal = true;
        const bool oc = guarded([&] { On on(0); at<FnPartRead>(0x00045070)(&o, nullptr, g_own[0], &da.tokenizer, nullptr); });
        g_stop_at_fatal = false;
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r.Read(g_own[1], &db.tokenizer, nullptr); });
        g_recording = false;
        g_side = 0;
        eq("original stopped at the report", oc, 1);
        eq("recovered went on", rc, 0);
        eq("combine", o.m_combine.m_op, r.m_combine.m_op);
        eq("pixel", (uintptr_t)o.m_pixelProcess, (uintptr_t)r.m_pixelProcess);
        compare_logs("pixel not found");
    }

    // Texture counts: each of eight stages, by mask.
    g_context = "texture counts";
    for (unsigned mask = 0; mask < 256; mask++) {
        AtProgShaderComponent o, r;
        for (unsigned i = 0; i < 8; i++) {
            o.m_stages[i].m_texture = (mask >> i & 1) ? &o : nullptr;
            r.m_stages[i].m_texture = (mask >> i & 1) ? &r : nullptr;
        }
        eq("textures", at<FnCount>(0x000453B0)(&o, nullptr), r.CountTextures());
    }

    // A shader on its own: read and read again; a second call of each transform note on what the first left.
    static const char* const shader_bodies[] = {
        "{}", "{ StagedShaderComponent[1] { StagedShaderComponent { TextureStage { Texture \"A\" Coord2DTransform 3 } } } }", "{ ProgShaderComponent[1] { ProgShaderComponent { VertexProcess[1] { \"V\" } TextureStage[1] { TextureStage { Coord3DTransform 2 } } } } }",
        "{ ProgShaderComponent[1] { ProgShaderComponent { } } StagedShaderComponent[1] { StagedShaderComponent { } } }",
        "", "{", "{ Add", "{ StagedShaderComponent[1] {", "{ StagedShaderComponent[1] { StagedShaderComponent {", "{ StagedShaderComponent[2] { StagedShaderComponent { } }", "{ ProgShaderComponent[1] { ProgShaderComponent {", "{ ProgShaderComponent[2] { ProgShaderComponent { } }",
    };
    for (const char* text : shader_bodies) {
        for (const char* ns : {(const char*)nullptr, "", "Space"}) {
            g_context = std::string("shader [") + text + "]";
            make_items(0, 1);
            make_items(1, 1);
            for (int pass = 0; pass < 2; pass++) {
                Direct da(text), db(text);
                const bool oc = guarded([&] { On on(0); at<FnShaderRead>(0x00037740)(item(0, 0), nullptr, g_own[0], g_pool, &da.tokenizer, ns); });
                g_recording = false;
                g_side = 0;
                const bool rc = guarded([&] { On on(1); item(1, 0)->Read(g_own[1], nullptr, &db.tokenizer, ns); });
                g_recording = false;
                g_side = 0;
                eq("crash", oc, rc);
                same_shader(item(0, 0), item(1, 0));
                compare_logs("shader");
                eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
            }
            destroy_items("shader destroyed");
        }
    }

    // No room for the array: a raise, then the reads with a null array.
    for (const char* text : {"{ StagedShaderComponent[0] { } }", "{ ProgShaderComponent[0] { } }", "{ StagedShaderComponent[1] { StagedShaderComponent { } } }", "{ ProgShaderComponent[1] { ProgShaderComponent { } } }", "{ StagedShaderComponent[1] { } }", "{ ProgShaderComponent[1] { } }"}) {
        g_context = std::string("no room [") + text + "]";
        make_items(0, 1);
        make_items(1, 1);
        Direct da(text), db(text);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnShaderRead>(0x00037740)(item(0, 0), nullptr, g_own[0], nullptr, &da.tokenizer, nullptr); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); item(1, 0)->Read(g_own[1], nullptr, &db.tokenizer, nullptr); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        eq("prog count", item(0, 0)->m_progCount, item(1, 0)->m_progCount);
        eq("staged count", item(0, 0)->m_stagedCount, item(1, 0)->m_stagedCount);
        eq("progs", item(0, 0)->m_progs != nullptr, item(1, 0)->m_progs != nullptr);
        eq("stageds", item(0, 0)->m_stageds != nullptr, item(1, 0)->m_stageds != nullptr);
        compare_logs("no room");
        // The counts are left high with a null array, which Clear walks past only when the array is null.
        destroy_items("no room, destroyed");
    }

    // Counts with no arrays: the arrays alone decide what Read and Clear do.
    g_context = "clear, counts with no arrays";
    {
        make_items(0, 1);
        make_items(1, 1);
        item(0, 0)->m_progCount = item(1, 0)->m_progCount = 3;
        item(0, 0)->m_transformCount = item(1, 0)->m_transformCount = 5;
        item(0, 0)->m_transforms[7] = item(1, 0)->m_transforms[7] = 6;
        Direct da("{}"), db("{}");
        { On on(0); at<FnShaderRead>(0x00037740)(item(0, 0), nullptr, g_own[0], nullptr, &da.tokenizer, nullptr); }
        { On on(1); item(1, 0)->Read(g_own[1], nullptr, &db.tokenizer, nullptr); }
        same_shader(item(0, 0), item(1, 0));
        compare_logs("counts with no arrays");
        item(0, 0)->m_stagedCount = item(1, 0)->m_stagedCount = 4;  // Read would walk these, with no array, and crash.
        { On on(0); at<Fn0>(0x000375A0)(item(0, 0), nullptr); }
        { On on(1); item(1, 0)->AtShader::Clear(); }
        same_shader(item(0, 0), item(1, 0));
        compare_logs("cleared");
        destroy_items("destroyed");
    }

    // The owner's.
    g_context = "owner";
    {
        GmNameMap a, b, c, d;
        for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
            at<FnAdd>(0x00025EF0)(g_own[0], nullptr, &a, ns);
            g_own[1]->AddShaders(&b, ns);
            eq("group", a.m_group == &g_own[0]->m_shaders, b.m_group == &g_own[1]->m_shaders);
            eqs("group's name", group_name(a.m_group).c_str(), group_name(b.m_group).c_str());
            eq("first", g_own[0]->m_shaders.m_first == &a, g_own[1]->m_shaders.m_first == &b);
            at<FnAdd>(0x00025EF0)(g_own[0], nullptr, &c, ns);
            g_own[1]->AddShaders(&d, ns);
            eq("second's next", c.m_next == &a, d.m_next == &b);
        }
    }

    // SetHeaps.
    g_context = "heaps";
    {
        int token;
        at<void(__cdecl*)(void*)>(0x0002B1B0)(&token);
        AtShaderDatabase::SetHeap(&token);
        eq("database heap", (uintptr_t)*(void**)(uintptr_t)0x002C6380, (uintptr_t)AtShaderDatabase::s_heap);
        at<void(__cdecl*)(void*)>(0x00037520)(&token);
        AtShader::SetHeap(&token);
        eq("shader heap", (uintptr_t)*(void**)(uintptr_t)0x002C63EC, (uintptr_t)AtShader::s_heap);
        at<void(__cdecl*)(void*)>(0x00045020)(&token);
        AtProgShaderComponent::SetHeap(&token);
        eq("prog heap", (uintptr_t)*(void**)(uintptr_t)0x002C7C44, (uintptr_t)AtProgShaderComponent::s_heap);
        at<void(__cdecl*)(void*)>(0x00037520)(&g_heap_token);
        AtShader::SetHeap(&g_heap_token);
        at<void(__cdecl*)(void*)>(0x00045020)(&g_heap_token);
        AtProgShaderComponent::SetHeap(&g_heap_token);
        at<void(__cdecl*)(void*)>(0x0002B1B0)(&g_heap_token);
        AtShaderDatabase::SetHeap(&g_heap_token);
    }
}

static void run_no_heap() {
    g_context = "no heap";
    *(void**)(uintptr_t)0x002C63EC = nullptr;
    *(void**)(uintptr_t)0x002C7C44 = nullptr;
    AtShader::s_heap = nullptr;
    AtProgShaderComponent::s_heap = nullptr;
    run_text("Shader[1]\n{\nShader \"A\"\n{\nStagedShaderComponent[1]\n{\nStagedShaderComponent\n{\n}\n}\nProgShaderComponent[1]\n{\nProgShaderComponent\n{\nVertexProcess[1]\n{\n\"A\"\n}\n}\n}\n}\n}\n", "no heap", 0xCD);
    // One heap at a time, so that each raise is known to read its own.
    *(void**)(uintptr_t)0x002C7C44 = &g_heap_token;
    AtProgShaderComponent::s_heap = &g_heap_token;
    run_text("Shader[1]\n{\nShader \"A\"\n{\nStagedShaderComponent[1]\n{\nStagedShaderComponent\n{\n}\n}\nProgShaderComponent[1]\n{\nProgShaderComponent\n{\nVertexProcess[1]\n{\n\"A\"\n}\n}\n}\n}\n}\n", "no shader heap", 0xCD);
    *(void**)(uintptr_t)0x002C63EC = &g_heap_token;
    AtShader::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C7C44 = nullptr;
    AtProgShaderComponent::s_heap = nullptr;
    run_text("Shader[1]\n{\nShader \"A\"\n{\nStagedShaderComponent[1]\n{\nStagedShaderComponent\n{\n}\n}\nProgShaderComponent[1]\n{\nProgShaderComponent\n{\nVertexProcess[1]\n{\n\"A\"\n}\n}\n}\n}\n}\n", "no prog heap", 0xCD);
    *(void**)(uintptr_t)0x002C7C44 = &g_heap_token;
    AtProgShaderComponent::s_heap = &g_heap_token;
}

// A file that will not open: the report, and then the reader on no tokenizer. In the game the report does not come back.
static void run_no_file() {
    g_context = "no file";
    Env env({"A"}, {"Space"});
    for (int status : {1, 2, 5}) {
        g_fs->open_status = status;
        AtShaderDatabase* o = make_o(0xCD);
        TestDb* r = make_r(0xCD);
        LoadArg a = {o, r};
        const bool oc = guarded([&] { load_o(&a); });
        g_recording = false;
        const bool rc = guarded([&] { load_r(&a); });
        g_recording = false;
        eq("crash on no tokenizer", oc, rc);
        compare_logs("no file");
        g_fs->open_status = 0;
    }
    g_db[0] = g_db[1] = nullptr;
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
    if (pos.size() != 2) { std::printf("usage: oracle_ags <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal_stops);
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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6380u, 0x002C63ECu, 0x002C7C44u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtShader::s_heap = &g_heap_token;
    AtProgShaderComponent::s_heap = &g_heap_token;
    AtShaderDatabase::s_heap = &g_heap_token;

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_written();
    run_direct();
    run_no_heap();
    run_no_file();

    g_shaders = g_stageds = g_progs = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AGS\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::string data((size_t)std::ftell(f), '\0');
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
        std::fclose(f);
        run_text(data, line, 0xCD);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u shaders, %u staged and %u programmable components; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_shaders, g_stageds, g_progs, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
