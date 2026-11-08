// Differential oracle for the .AGD lists and the readers they reach directly: .AGG, .AKA, .AGV, .AGP (Windows x86, local only). See docs/verification.md.
//
//   oracle_agd <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Doubles stand in for what matters most on real files: every file an .AGD lists opens here as empty text, and the concrete loader, meshes, vertex arrays and keys are this harness's. So real .AGD files show that the list,
// the order and the names agree, and not that any real mesh, texture or animation loads. The .AGG and .AKA readers see only text written here: the disc has no such files.
//
// One class serves both sides for a slot the original calls through a table: x86 thiscall is what the original expects. Objects on the original's side are made by the original's constructor, then given the harness's table.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtDatabaseLoader.h"
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
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

static std::map<std::string, std::string> g_files;
static unsigned g_opens;

class PathFs : public MemFs {
public:
    int Open(const char* path, int flags, AtIoFile** out) override {
        const bool was = g_recording;
        g_recording = false;
        g_opens++;
        {
            std::string key(path);
            for (char& c : key) c = (char)toupper((unsigned char)c);
            auto it = g_files.find(key);
            static const std::string empty;
            const std::string& text = it != g_files.end() ? it->second : empty;
            files[g_side].data = (const unsigned char*)text.data();
            files[g_side].size = text.size();
            strncpy_s(files[g_side].m_path, path, _TRUNCATE);
        }
        g_recording = was;
        g_log[g_side].add('T', (uint32_t)m_path.m_top);
        if (const char* current = m_path.Current()) record_message(current, g_side);
        return MemFs::Open(path, flags, out);
    }
};

static PathFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static int g_binary;

static void slot(char c, uint32_t v = 0) { g_log[g_side].add(c, v); }
static void slot_text(const char* text) { record_message(text ? text : "(null)", g_side); }

// Makes an object of a harness class on the current side. The original's constructor on its side, the harness's table on either.
template <class T> static T* make(uint32_t ctor) {
    const bool was = g_recording;
    g_recording = false;
    void* p = std::malloc(sizeof(T));
    std::memset(p, 0xCD, sizeof(T));
    T* t = new (p) T;
    if (g_side == 0 && ctor != 0) {
        // The harness's members are made above. What the original's constructor must set, it sets over the fill.
        void* table = *(void**)p;
        std::memset(p, 0xCD, sizeof(typename T::Base));
        at<Fn0>(ctor)(p, nullptr);
        *(void**)p = table;
    }
    g_recording = was;
    return t;
}

// ---- what a mesh file needs -------------------------------------------------------------------------------------------

static float g_positions[2][64][3];

class TestArray : public AtVertexArray {
public:
    TestArray() { m_unknown04 = 0; m_flags = kPositionsInMemory; }
    float* PositionOf(float* index) override { slot('p', (uint32_t)(uintptr_t)index); return g_positions[g_side][(uintptr_t)index % 64]; }
    void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {}
    void Slot9() override {} void Slot10() override {} void Slot11() override {} void Slot12() override {} void Slot13() override {} void Slot14() override {} void Slot15() override {}
    void Finish() override { slot('v'); }
    void Slot17() override {} void Slot18() override {} void Slot19() override {} void Slot20() override {}
    void Read(AtMaterialOwner*, AtIoTokenizer* tokenizer, unsigned int index) override { slot('V', index); slot('v', (uint32_t)tokenizer->Next()); tokenizer->ExpectRightCurly(); }
};

static unsigned char g_indices[2][4096];

class TestIndices : public AtIndexArray {
public:
    TestIndices() { m_count = 0; m_flags = 0; m_loadFlags = 0; std::memset(m_name, 0, sizeof m_name); m_index = 0; }
    void Allocate(unsigned int count, unsigned int flags) override { slot('a', count); slot('a', flags); }
    void Slot1() override {} void Slot2() override {}
    void Lock(void** out, int mode) override { slot('k', (uint32_t)mode); *out = g_indices[g_side]; }
    void Unlock() override { slot('u'); }
    void Finish() override { slot('i'); }
};

class TestMesh : public AtMesh {
public:
    typedef AtMesh Base;
    TestArray m_arrays[4];
    TestIndices m_lists[2];
    int m_nullArray = -1;
    TestMesh() { m_ownVertexArrays = 0; m_ownIndexArrays = 0; m_unknown0C = 0; m_triangleCount = 0; m_triangles = nullptr; m_database = nullptr; }
    void Slot0() override {} void Slot1() override {}
    AtVertexArray* OwnVertexArrayAt(unsigned int i) override { slot('2', i); return (int)i == m_nullArray ? nullptr : &m_arrays[i % 4]; }
    AtIndexArray* OwnIndexArrayAt(unsigned int i) override { slot('3', i); return (int)i == m_nullArray ? nullptr : &m_lists[i % 2]; }
    void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {}
    void Read(AtMaterialOwner*, AtMeshDatabase* database, AtIoTokenizer* tokenizer, const char* ns) override {
        slot('H', database != nullptr);
        slot_text(ns);
        m_database = database;
        tokenizer->ExpectLeftCurly();
        tokenizer->ExpectRightCurly();
    }
    AtVertexArray* VertexArrayAt(unsigned int i) override { slot('4', i); return &m_arrays[i % 4]; }
};

class TestMeshDb : public AtMeshDatabase {
public:
    typedef AtMeshDatabase Base;
    enum { kMax = 8 };
    TestArray m_v[kMax];
    TestIndices m_i[kMax];
    TestMesh m_m[kMax];
    void Clear() override { slot('X'); if (g_side == 0) at<Fn0>(0x0002B150)(this, nullptr); else AtMeshDatabase::Clear(); }
    void MakeMeshes(AtMaterialOwner*) override { slot('M', m_meshCount); }
    void MakeVertexArrays(AtMaterialOwner*) override { slot('A', m_vertexArrayCount); }
    void MakeIndexArrays(AtMaterialOwner*) override { slot('I', m_indexArrayCount); }
    int HasBinaryForm() override { slot('?'); return g_binary; }
    void LoadBinaryForm(AtMaterialOwner*, const char* name) override { slot('B'); slot_text(name); }
    void Slot8() override {}
    AtMesh* MeshAt(unsigned int i) override { slot('6', i); return &m_m[i % kMax]; }
    AtVertexArray* VertexArrayAt(unsigned int i) override { slot('7', i); return &m_v[i % kMax]; }
    AtIndexArray* IndexArrayAt(unsigned int i) override { slot('8', i); return &m_i[i % kMax]; }
};

// ---- what an animation file needs -------------------------------------------------------------------------------------

class TestKeys : public AtAnimationKeys {
public:
    typedef AtAnimationKeys Base;
    void Allocate(unsigned int count) override { slot('a', count); }
    void Free() override { slot('f'); }
    void ReadEntries(AtIoTokenizer* tokenizer) override { slot('e', (uint32_t)tokenizer->ExpectInt()); }
    void Interpolate(unsigned int, unsigned int, float, int, int*, int*, float*) override {}
    int Find(unsigned int, unsigned int, float, int) override { return 0; }
    void Load() override { slot('l'); }
    unsigned short At(unsigned int) override { return 0; }
    void Slot9() override {} void Slot10() override {} void Slot11() override {} void Slot12() override {} void Slot13() override {}
    void Slot14() override {} void Slot15() override {} void Slot16() override {} void Slot17() override {} void Slot18() override {}
    void ReadRotationKeys(AtIoTokenizer* t) override { slot('R', (uint32_t)t->ExpectInt()); }
    void ReadTranslationKeys(AtIoTokenizer* t) override { slot('T', (uint32_t)t->ExpectInt()); }
    void ReadScaleKeys(AtIoTokenizer* t) override { slot('Z', (uint32_t)t->ExpectInt()); }
    void ReadNoteKeys(AtIoTokenizer* t) override { slot('Q', (uint32_t)t->ExpectInt()); }
    void ReadCameraKeys(AtIoTokenizer* t) override { slot('Y', (uint32_t)t->ExpectInt()); }
};

class TestAnimDb : public AtAnimationDatabase {
public:
    typedef AtAnimationDatabase Base;
    void Clear() override { slot('X'); if (g_side == 0) at<Fn0>(0x0002A9B0)(this, nullptr); else AtAnimationDatabase::Clear(); }
    int HasBinaryForm() override { slot('?'); return g_binary; }
    void LoadBinaryForm(AtAnimationDevice*, const char* name) override { slot('B'); slot_text(name); }
};

class TestDevice : public AtAnimationDevice {
public:
    AtAnimationKeys* NewKeys() override { slot('n'); return make<TestKeys>(0); }
    void FreeKeys(AtAnimationKeys* keys) override { slot('F', keys != nullptr); }
    AtAnimationDatabase* NewDatabase() override { slot('d'); return make<TestAnimDb>(0x0002A770); }
    void FreeDatabase(AtAnimationDatabase* database) override { slot('D', database != nullptr); }
};

class TestPool : public AtMatrixPool {
public:
    void Slot0() override {}
    void Reserve(unsigned int count) override { slot('r', count); }
    void Clear() override {}
    unsigned int Count() override { return 0; }
    AtMatrix* At(unsigned int) override { return nullptr; }
};

class TestMatrices : public AtMatrixDevice {
public:
    TestPool m_pool;
    AtMatrix* NewMatrix() override { return nullptr; }
    AtMatrixPool* NewPool() override { slot('o'); return &m_pool; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override {}
    void FreePool(AtMatrixPool*) override { slot('O'); }
};

class TestOwner : public AtMaterialOwner {
public:
    TestMatrices m_matrices;
    AtMatrixDevice* MatrixDevice() override { slot('z'); return &m_matrices; }
};

static TestOwner* g_own[2];
static TestDevice g_device;

// ---- processes --------------------------------------------------------------------------------------------------------

class TestProcess : public AtProcessItem {
public:
    void OnLoaded(AtMaterialOwner*) override { slot('L'); }
    void Slot1() override {}
    void ReadPixel(AtMaterialOwner*, AtIoTokenizer* t) override { slot('P'); t->ExpectLeftCurly(); t->ExpectRightCurly(); }
    void Slot3() override {} void Slot4() override {}
    void ReadVertex(AtMaterialOwner*, AtIoTokenizer* t) override { slot('V'); t->ExpectLeftCurly(); t->ExpectRightCurly(); }
};

class TestVertexDb : public AtVertexProcessDatabase {
public:
    typedef AtVertexProcessDatabase Base;
    TestProcess m_items[8];
    void Clear() override { slot('X'); if (g_side == 0) at<Fn0>(0x0002B860)(this, nullptr); else AtVertexProcessDatabase::Clear(); }
    void Make(AtMaterialOwner*) override { slot('M', m_count); }
    AtProcessItem* ItemAt(unsigned int i) override { slot('7', i); return &m_items[i % 8]; }
};

class TestPixelDb : public AtPixelProcessDatabase {
public:
    typedef AtPixelProcessDatabase Base;
    TestProcess m_items[8];
    void Clear() override { slot('X'); if (g_side == 0) at<Fn0>(0x0002B600)(this, nullptr); else AtPixelProcessDatabase::Clear(); }
    void Make(AtMaterialOwner*) override { slot('M', m_count); }
    AtProcessItem* ItemAt(unsigned int i) override { slot('7', i); return &m_items[i % 8]; }
};

// ---- the other databases, which empty text leaves alone ---------------------------------------------------------------

class TestTexture : public AtTextureDatabase {
public:
    typedef AtTextureDatabase Base;
    void OnReady(AtMaterialOwner*) override { slot('1'); }
    void MakeSurfaces2D() override {} void MakeSurfaces3D() override {} void MakeTextures2D() override {} void MakeTextures3D() override {} void MakeCubeMaps() override {} void MakeMore() override {}
    void* Surface2DAt(unsigned int) override { return nullptr; } void* Surface3DAt(unsigned int) override { return nullptr; }
    AtTextureItem* Texture2DAt(unsigned int) override { return nullptr; } AtTextureItem* Texture3DAt(unsigned int) override { return nullptr; } AtTextureItem* CubeMapAt(unsigned int) override { return nullptr; }
};
class TestShader : public AtShaderDatabase {
public:
    typedef AtShaderDatabase Base;
    void OnReady() override { slot('1'); }
    AtShader* ShaderAt(unsigned int) override { return nullptr; }
};
class TestMaterialDb : public AtMaterialDatabase {
public:
    typedef AtMaterialDatabase Base;
    // The file's loader and reader are slots here, and each side must run its own.
    void Load(AtMaterialOwner* owner, const char* name, int mode) override {
        slot('E', (uint32_t)mode);
        if (g_side == 0) at<void(__fastcall*)(void*, void*, void*, const char*, int)>(0x00038FB0)(this, nullptr, owner, name, mode);
        else AtMaterialDatabase::Load(owner, name, mode);
    }
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) override {
        if (g_side == 0) at<void(__fastcall*)(void*, void*, void*, void*)>(0x00039500)(this, nullptr, owner, tokenizer);
        else AtMaterialDatabase::Read(owner, tokenizer);
    }
    void OnReady(AtMaterialOwner*) override { slot('1'); }
    void Slot3() override {}
    AtMaterial* MaterialAt(unsigned int) override { return nullptr; }
};
class TestLight : public AtLightDatabase {
public:
    typedef AtLightDatabase Base;
    void MakeLights(AtMaterialOwner*) override { slot('1'); }
    AtLight* LightAt(unsigned int) override { return nullptr; }
};
class TestFont : public AtFontDatabase {
public:
    typedef AtFontDatabase Base;
    void OnReady(AtMaterialOwner*) override { slot('1'); }
    void MakeFonts2D() override {}
    AtGrI2DFont* Font2DAt(unsigned int) override { return nullptr; }
};
class TestImage : public AtImageDatabase {
public:
    typedef AtImageDatabase Base;
    void MakeItems() override {}
    void OnReady(AtMaterialOwner*) override { slot('1'); }
    AtImage* ImageAt(unsigned int) override { return nullptr; }
};

// ---- the loader -------------------------------------------------------------------------------------------------------

enum { kMaxEach = 16 };

class TestLoader : public AtDatabaseLoader {
public:
    typedef AtDatabaseLoader Base;
    AtTextureDatabase* m_textureItems[kMaxEach];
    AtMaterialDatabase* m_materialItems[kMaxEach];
    AtShaderDatabase* m_shaderItems[kMaxEach];
    AtMeshDatabase* m_meshItems[kMaxEach];
    AtLightDatabase* m_lightItems[kMaxEach];
    AtFontDatabase* m_fontItems[kMaxEach];
    AtImageDatabase* m_imageItems[kMaxEach];
    AtVertexProcessDatabase* m_vertexItems[kMaxEach];
    AtPixelProcessDatabase* m_pixelItems[kMaxEach];
    AtDatabaseLoader* m_loaderItems[kMaxEach];
    int m_extraMarker;

    void clear_items() { std::memset(m_textureItems, 0, (char*)&m_extraMarker - (char*)m_textureItems); }

    template <class T, class B> static void fill(B** items, unsigned count, uint32_t ctor) {
        for (unsigned i = 0; i < std::min(count, (unsigned)kMaxEach); i++) items[i] = make<T>(ctor);
    }
    void MakeTextures() override { slot('m', 0 | (m_textureCount << 8)); fill<TestTexture>(m_textureItems, m_textureCount, 0x0001BFA0); }
    void MakeMaterials() override { slot('m', 1 | (m_materialCount << 8)); fill<TestMaterialDb>(m_materialItems, m_materialCount, 0x00038F80); }
    void MakeShaders() override { slot('m', 2 | (m_shaderCount << 8)); fill<TestShader>(m_shaderItems, m_shaderCount, 0x0002B1C0); }
    void MakeMeshes() override { slot('m', 3 | (m_meshCount << 8)); fill<TestMeshDb>(m_meshItems, m_meshCount, 0x0002ADD0); }
    void MakeLights() override { slot('m', 4 | (m_lightCount << 8)); fill<TestLight>(m_lightItems, m_lightCount, 0x0002AB00); }
    void Slot5() override { slot('m', 5); }
    void MakeFonts() override { slot('m', 6 | (m_fontCount << 8)); fill<TestFont>(m_fontItems, m_fontCount, 0x0002A020); }
    void MakeImages() override { slot('m', 7 | (m_imageCount << 8)); fill<TestImage>(m_imageItems, m_imageCount, 0x00029BA0); }
    void MakeVertexProcesses() override { slot('m', 8 | (m_vertexProcessCount << 8)); fill<TestVertexDb>(m_vertexItems, m_vertexProcessCount, 0x0002B690); }
    void MakePixelProcesses() override { slot('m', 9 | (m_pixelProcessCount << 8)); fill<TestPixelDb>(m_pixelItems, m_pixelProcessCount, 0x0002B430); }
    void MakeLoaders() override;
    void Unload() override { slot('U'); if (g_side == 0) at<Fn0>(0x0001AB10)(this, nullptr); else AtDatabaseLoader::Unload(); }
    AtTextureDatabase* TextureAt(unsigned int i) override { slot('t', i); return m_textureItems[i % kMaxEach]; }
    AtMaterialDatabase* MaterialAt(unsigned int i) override { slot('b', i); return m_materialItems[i % kMaxEach]; }
    AtShaderDatabase* ShaderAt(unsigned int i) override { slot('s', i); return m_shaderItems[i % kMaxEach]; }
    AtMeshDatabase* MeshAt(unsigned int i) override { slot('h', i); return m_meshItems[i % kMaxEach]; }
    AtLightDatabase* LightAt(unsigned int i) override { slot('g', i); return m_lightItems[i % kMaxEach]; }
    AtFontDatabase* FontAt(unsigned int i) override { slot('j', i); return m_fontItems[i % kMaxEach]; }
    AtImageDatabase* ImageAt(unsigned int i) override { slot('x', i); return m_imageItems[i % kMaxEach]; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int i) override { slot('w', i); return m_vertexItems[i % kMaxEach]; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int i) override { slot('y', i); return m_pixelItems[i % kMaxEach]; }
    AtDatabaseLoader* LoaderAt(unsigned int i) override { slot('q', i); return m_loaderItems[i % kMaxEach]; }
};

static TestLoader* make_loader() {
    TestLoader* l = make<TestLoader>(0x0001A440);
    l->clear_items();
    return l;
}

void TestLoader::MakeLoaders() {
    slot('m', 10 | (m_loaderCount << 8));
    for (unsigned i = 0; i < std::min((unsigned)m_loaderCount, (unsigned)kMaxEach); i++) m_loaderItems[i] = make_loader();
}

// A child to be unloaded: a slot that only notes, whichever side.
class TestChild : public TestLoader {
public:
    void Unload() override { slot('c'); }
    ~TestChild() override { slot('~'); m_owner = nullptr; }
    static void operator delete(void*) {}  // Not logged on either side. The global one is the harness's, which logs for one side only.
};

typedef void(__fastcall* FnLoadD)(void*, void*, void*, const char*, int, const char*, int);
typedef void(__fastcall* FnLoad3)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnLdb)(void*, void*, void*, int, const char*, int);

static const uint32_t kStatics[] = {0x002C62BC, 0x002C62A4, 0x002C629C, 0x002C6298, 0x002C62C4, 0x002C62AC, 0x002C62D4, 0x002C62B8, 0x002C62C0, 0x002C62C8, 0x002C62D0, 0x002C62B0, 0x002C62B4, 0x002C62CC, 0x002C62A8, 0x002C62D8, 0x002C62A0};
static char*** const kOurs[] = {&AtDatabaseLoader::s_textureNames, &AtDatabaseLoader::s_shaderNames, &AtDatabaseLoader::s_materialNames, &AtDatabaseLoader::s_meshNames, &AtDatabaseLoader::s_lightNames, &AtDatabaseLoader::s_coordSystemNames, &AtDatabaseLoader::s_mirrorNames, &AtDatabaseLoader::s_weightsNames, &AtDatabaseLoader::s_animationNames, &AtDatabaseLoader::s_matAssignmentNames, &AtDatabaseLoader::s_matAnimationNames, &AtDatabaseLoader::s_morphAnimationNames, &AtDatabaseLoader::s_lightAnimationNames, &AtDatabaseLoader::s_fontNames, &AtDatabaseLoader::s_imageNames, &AtDatabaseLoader::s_vertexProcessNames, &AtDatabaseLoader::s_pixelProcessNames};
static const size_t kCounts[] = {offsetof(AtDatabaseLoader, m_textureCount), offsetof(AtDatabaseLoader, m_shaderCount), offsetof(AtDatabaseLoader, m_materialCount), offsetof(AtDatabaseLoader, m_meshCount), offsetof(AtDatabaseLoader, m_lightCount), offsetof(AtDatabaseLoader, m_coordSystemCount), offsetof(AtDatabaseLoader, m_mirrorCount), offsetof(AtDatabaseLoader, m_weightsCount), offsetof(AtDatabaseLoader, m_animationCount), offsetof(AtDatabaseLoader, m_matAssignmentCount), offsetof(AtDatabaseLoader, m_matAnimationCount), offsetof(AtDatabaseLoader, m_morphAnimationCount), offsetof(AtDatabaseLoader, m_lightAnimationCount), offsetof(AtDatabaseLoader, m_fontCount), offsetof(AtDatabaseLoader, m_imageCount), offsetof(AtDatabaseLoader, m_vertexProcessCount), offsetof(AtDatabaseLoader, m_pixelProcessCount)};

static unsigned g_names_compared;

static void same_map(const char* what, GmNameMap& o, GmNameMap& r, const void* obase, const void* rbase) {
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_entries != nullptr, r.m_entries != nullptr);
    if (o.m_size == r.m_size && o.m_entries && r.m_entries) {
        for (unsigned i = 0; i < o.m_size; i++) {
            const GmNameMap::Entry& x = o.m_entries[i];
            const GmNameMap::Entry& y = r.m_entries[i];
            eq(what, x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq(what, std::strncmp(x.m_name, y.m_name, 8), 0);
                if (obase) eq(what, (char*)x.m_value - (char*)obase, (char*)y.m_value - (char*)rbase);
            }
        }
    }
}

// The stretch of the two logs about where they part, for a person reading a failure.
static void show_parting() {
    static int shown;
    int i = 0;
    while (i < g_log[0].n && i < g_log[1].n && g_log[0].v[i] == g_log[1].v[i]) i++;
    if ((i == g_log[0].n && i == g_log[1].n) || shown++ >= 3) return;
    for (int side = 0; side < 2; side++) {
        std::printf("at %d:", i);
        for (int k = std::max(0, (i & ~1) - 16); k < g_log[side].n && k < i + 24; k += 2) std::printf(" %c%X", (char)g_log[side].v[k], g_log[side].v[k + 1]);
        std::printf("\n");
    }
}

static void same_loader(const char* what, TestLoader* o, TestLoader* r) {
    g_recording = false;
    show_parting();
    compare_logs(what);
    eq("owner set", o->m_owner != nullptr, r->m_owner != nullptr);
    eq("the owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
    eq("counts", std::memcmp((char*)o + 0x80, (char*)r + 0x80, 0xA6 - 0x80), 0);
    eq("coord systems", o->m_coordSystems != nullptr, r->m_coordSystems != nullptr);
    eq("mat lists", o->m_matAssignments != nullptr, r->m_matAssignments != nullptr);
    eq("mat animations", o->m_matAnimations != nullptr, r->m_matAnimations != nullptr);
    eq("animations", o->m_animations != nullptr, r->m_animations != nullptr);
    eq("unknown 28", o->m_unknown28, r->m_unknown28);
    eq("unknown 2C", o->m_unknown2C, r->m_unknown2C);
    eq("child", o->m_child != nullptr, r->m_child != nullptr);
    eq("loader names", o->m_loaderNames != nullptr, r->m_loaderNames != nullptr);
    same_map("coord map", o->m_coordSystemMap, r->m_coordSystemMap, o->m_coordSystems, r->m_coordSystems);
    same_map("mirror map", o->m_mirrorMap, r->m_mirrorMap, nullptr, nullptr);
    same_map("weights map", o->m_weightsMap, r->m_weightsMap, nullptr, nullptr);
    same_map("animation map", o->m_animationMap, r->m_animationMap, nullptr, nullptr);
    if (o->m_animations && r->m_animations && o->m_animationCount == r->m_animationCount) {
        for (unsigned i = 0; i < o->m_animationCount; i++) {
            void* a = o->m_animationMap.m_entries ? nullptr : nullptr;
            (void)a;
            eq("animation made", o->m_animations[i] != nullptr, r->m_animations[i] != nullptr);
        }
    }
    for (size_t i = 0; i < sizeof kStatics / sizeof kStatics[0]; i++) {
        char** a = *(char***)(uintptr_t)kStatics[i];
        char** b = *kOurs[i];
        eq("name list", a != nullptr, b != nullptr);
        if (a && b) {
            const unsigned count = *(unsigned short*)((char*)r + kCounts[i]);
            for (unsigned k = 0; k < count; k++) {
                eq("a name", a[k] != nullptr, b[k] != nullptr);
                if (a[k] && b[k]) { eqs("a name", a[k], b[k]); g_names_compared++; }
            }
        }
    }
    if (o->m_loaderNames && r->m_loaderNames) {
        for (unsigned k = 0; k < r->m_loaderCount; k++) eqs("a loader's name", o->m_loaderNames[k], r->m_loaderNames[k]);
    }
    // In the owner.
    eq("owner's coord group", g_own[0]->m_coordSystems.m_first == &o->m_coordSystemMap, g_own[1]->m_coordSystems.m_first == &r->m_coordSystemMap);
    eq("owner's animation group", g_own[0]->m_animations.m_first == &o->m_animationMap, g_own[1]->m_animations.m_first == &r->m_animationMap);
    eq("owner's mirror group", g_own[0]->m_mirrors.m_first != nullptr, g_own[1]->m_mirrors.m_first != nullptr);
    eq("owner's weights group", g_own[0]->m_weights.m_first != nullptr, g_own[1]->m_weights.m_first != nullptr);
    eq("owner's mesh group", g_own[0]->m_meshes.m_first != nullptr, g_own[1]->m_meshes.m_first != nullptr);
}

static void make_owners() {
    const bool was = g_recording;
    g_recording = false;
    for (int side = 0; side < 2; side++) {
        g_own[side] = new (std::malloc(sizeof(TestOwner))) TestOwner;
        AtMaterialOwner& w = *g_own[side];
        w.m_animationDevice = &g_device;
        w.m_meshSpaces.Init(8);
        w.m_lightAnimationSpaces.Init(8);
        w.m_morphAnimationSpaces.Init(8);
        w.m_textureSpaces.Init(8);
        w.m_materialSpaces.Init(8);
        w.m_shaderSpaces.Init(8);
        w.m_lightSpaces.Init(8);
        w.m_matAssignmentSpaces.Init(8);
        w.m_matAnimationSpaces.Init(8);
        w.m_fontSpaces.Init(8);
        w.m_imageSpaces.Init(8);
    }
    g_recording = was;
}

static TestLoader* on_side(int side) {
    g_side = side;
    TestLoader* l = make_loader();
    g_side = 0;
    return l;
}

struct LoadArgs { TestLoader* l; const char* name; int mode; const char* prefix; int in_directory; };
static void load_o(void* p) { LoadArgs* a = (LoadArgs*)p; On on(0); at<FnLoadD>(0x0001ADC0)(a->l, nullptr, g_own[0], a->name, a->mode, a->prefix, a->in_directory); }
static void load_r(void* p) { LoadArgs* a = (LoadArgs*)p; On on(1); a->l->Load(g_own[1], a->name, a->mode, a->prefix, a->in_directory); }

static unsigned g_files_run;

static void reset_statics_after(TestLoader* o, TestLoader* r, const char* what) {
    { On on(0); at<Fn0>(0x0001AD40)(o, nullptr); }
    { On on(1); ((AtDatabaseLoader*)r)->AtDatabaseLoader::~AtDatabaseLoader(); }
    compare_logs(what);
    for (size_t i = 0; i < sizeof kStatics / sizeof kStatics[0]; i++) {
        eq("name list after", *(char***)(uintptr_t)kStatics[i] != nullptr, *kOurs[i] != nullptr);
    }
}

static void run_agd(const std::string& text, const std::string& context, int mode, const char* prefix, int in_directory, unsigned char fill, bool again) {
    g_context = context;
    g_files["MODEL\\THING.AGD"] = text;
    g_fill = fill;
    make_owners();
    TestLoader* o = on_side(0);
    TestLoader* r = on_side(1);
    same_loader("constructed", o, r);
    LoadArgs ao = {o, "MODEL\\THING", mode, prefix, in_directory};
    LoadArgs ar = {r, "MODEL\\THING", mode, prefix, in_directory};
    g_fs->m_path.m_top = -1;
    // The stack's bytes too: a pop can overwrite the entry it leaves.
    static char before[sizeof(AtIoPath)], after[sizeof(AtIoPath)];
    std::memcpy(before, &g_fs->m_path, sizeof before);
    load_o(&ao);
    eq("directory stack after", g_fs->m_path.m_top, -1);
    std::memcpy(after, &g_fs->m_path, sizeof after);
    std::memcpy((void*)&g_fs->m_path, before, sizeof before);
    load_r(&ar);
    eq("directory stack after", g_fs->m_path.m_top, -1);
    eq("directory stack's bytes", std::memcmp(after, &g_fs->m_path, sizeof after), 0);
    g_fs->m_path.m_top = -1;
    same_loader("load", o, r);
    if (again) {
        load_o(&ao);
        g_fs->m_path.m_top = -1;
        load_r(&ar);
        g_fs->m_path.m_top = -1;
        same_loader("load again", o, r);
    }
    { On on(0); o->Unload(); }
    { On on(1); r->Unload(); }
    same_loader("unload", o, r);
    { On on(0); o->Unload(); }
    { On on(1); r->Unload(); }
    same_loader("unload again", o, r);
    reset_statics_after(o, r, "destroyed");
    std::free(o);
    std::free(r);
}

static void run_written_agd() {
    static const char* const words[] = {"TextureDatabase", "ShaderDatabase", "MaterialDatabase", "MeshDatabase", "LightDatabase", "CoordSysDatabase", "AnimationDatabase", "MatAssignmentDatabase", "MatAnimationDatabase", "FontDatabase", "ImageDatabase", "VertexProcessDatabase", "PixelProcessDatabase"};
    // Each word: one name, two, none, a second list, and a name with no string. Not here: the four words behind the seam. Their numbers go to it, on the recovered side alone; see the end of this function.
    for (const char* w : words) {
        const std::string word(w);
        for (const std::string& body : {std::string("[1]\n{\n \"A\"\n}\n"), std::string("[2]\n{\n \"DIR\\A\"\n \"DIR\\LONGERTHANEIGHT.X\"\n}\n"), std::string("[0]\n{\n}\n"), std::string("[1]\n{\n \"A\"\n \"B\"\n}\n")}) {
            run_agd(word + body, "word " + word + body, 0, nullptr, 1, 0xCD, false);
        }
        run_agd(word + "[2]\n{\n \"A\"\n \"B\"\n}\n", "word prefix " + word, 0, "PRE", 1, 0xCD, true);
        run_agd(word + "[2]\n{\n \"A\\B\"\n \"B\"\n}\n", "word binary " + word, 2, nullptr, 0, 0x33, true);
        // A second list of the same kind reports, and reads over the first. Names leak, as in the original.
        run_agd(word + "[1]\n{\n \"A\"\n}\n" + word + "[1]\n{\n \"B\"\n}\n", "second " + word, 0, nullptr, 1, 0xCD, false);
    }
    static const char* const texts[] = {
        "", "// a comment\n", "Other\n", "5\n", "\"Name\"\n", "}\n",
        "TextureDatabase[1]\n{\n \"T\"\n}\nShaderDatabase[1]\n{\n \"S\"\n}\nMaterialDatabase[1]\n{\n \"M\"\n}\nMeshDatabase[1]\n{\n \"G\"\n}\nLightDatabase[1]\n{\n \"L\"\n}\nCoordSysDatabase[1]\n{\n \"C\"\n}\nAnimationDatabase[1]\n{\n \"A\"\n}\nMatAssignmentDatabase[1]\n{\n \"N\"\n}\nMatAnimationDatabase[1]\n{\n \"U\"\n}\nFontDatabase[1]\n{\n \"F\"\n}\nImageDatabase[1]\n{\n \"I\"\n}\nVertexProcessDatabase[1]\n{\n \"V\"\n}\nPixelProcessDatabase[1]\n{\n \"P\"\n}\n",
        "PixelProcessDatabase[1]\n{\n \"P\"\n}\nVertexProcessDatabase[1]\n{\n \"V\"\n}\nImageDatabase[1]\n{\n \"I\"\n}\nTextureDatabase[1]\n{\n \"T\"\n}\n",
        "CoordSysDatabase[2]\n{\n \"X\\Y.Z\"\n \"A/B\"\n}\nAnimationDatabase[3]\n{\n \"C:D\"\n \"ABCDEFGHIJ\"\n \"ABCDEFG\"\n}\n",
        "ImageDatabase[2]\n{\n \"A\\B\\C\"\n \"D\"\n}\n",
        "TextureDatabase[1] { 5 }\n",
    };
    for (const char* t : texts) {
        for (int mode : {0, 2}) {
            for (const char* prefix : {(const char*)nullptr, "", "PRE"}) {
                for (int dir : {0, 1}) run_agd(t, std::string("text [") + t + "]", mode, prefix, dir, 0xCD, true);
            }
        }
    }
    // Nested. The child's own list, and a grandchild.
    g_files["MODEL\\CHILD.AGD"] = "DatabaseLoader[1]\n{\n \"GRAND\"\n}\nTextureDatabase[1]\n{\n \"CT\"\n}\n";
    g_files["SUB\\DEEP.AGD"] = "DatabaseLoader[1]\n{\n \"GRAND\"\n}\nMeshDatabase[1]\n{\n \"CG\"\n}\nImageDatabase[1]\n{\n \"CI\"\n}\n";
    for (int binary : {0, 1}) {
        g_binary = binary;
        for (const char* prefix : {(const char*)nullptr, "PRE"}) {
            run_agd("DatabaseLoader[1]\n{\n \"SUB\\DEEP\"\n}\n", "nested in a directory", 2, prefix, 1, 0xCD, false);
        }
    }
    g_binary = 0;
    g_files["MODEL\\GRAND.AGD"] = "MaterialDatabase[1]\n{\n \"GM\"\n}\n";
    g_files["CHILD.AGD"] = g_files["MODEL\\CHILD.AGD"];
    g_files["GRAND.AGD"] = g_files["MODEL\\GRAND.AGD"];
    for (int dir : {0, 1}) {
        g_context = "nested";
        run_agd("DatabaseLoader[2]\n{\n \"CHILD\"\n \"GRAND\"\n}\nTextureDatabase[1]\n{\n \"T\"\n}\nMaterialDatabase[1]\n{\n \"M\"\n}\n", "nested", 0, nullptr, dir, 0xCD, true);
    }
    // A list before DatabaseLoader: making the nested loaders resets them, which empties the statics, and the first list's names are gone. Harness loaders are made as the concrete class would be: a crash on both sides.
    g_context = "nested after a list";
    g_files["MODEL\\LATE.AGD"] = "TextureDatabase[1]\n{\n \"T\"\n}\nDatabaseLoader[1]\n{\n \"GRAND\"\n}\n";
    {
        make_owners();
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        LoadArgs ao = {o, "MODEL\\LATE", 0, nullptr, 0};
        LoadArgs ar = {r, "MODEL\\LATE", 0, nullptr, 0};
        const bool oc = guarded([&] { load_o(&ao); });
        g_recording = false;
        const bool rc = guarded([&] { load_r(&ar); });
        g_recording = false;
        eq("crash on a lost list", oc, rc);
        eq("it does crash", oc, 1);
        compare_logs("nested after a list");
    }
}

// Everything given by hand, so that the names can be seen after a load that does not free them. No file of Load's can do that.
static void run_names_kept() {
    g_context = "names kept";
    g_files.erase("MODEL\\THING.AGD");
    make_owners();
    for (const char* prefix : {(const char*)nullptr, "PRE"}) {
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        TestLoader* both[2] = {o, r};
        for (int side = 0; side < 2; side++) {
            g_side = side;
            TestLoader* l = both[side];
            l->m_vertexProcessCount = 1; l->m_pixelProcessCount = 1; l->m_textureCount = 1; l->m_shaderCount = 1; l->m_materialCount = 1; l->m_meshCount = 1; l->m_lightCount = 1;
            l->m_fontCount = 1; l->m_imageCount = 2;
            l->MakeVertexProcesses(); l->MakePixelProcesses(); l->MakeTextures(); l->MakeShaders(); l->MakeMaterials(); l->MakeMeshes(); l->MakeLights(); l->MakeFonts(); l->MakeImages();
        }
        g_side = 0;
        g_log[0].n = g_log[1].n = 0;
        static const char* const words[] = {"Vp", "Pp", "Tx", "Sh", "Ma", "Me", "Li", "Fo", "D\\Im"};
        const size_t which[] = {15, 16, 0, 1, 2, 3, 4, 13, 14};
        char** lists[2][9];
        for (int side = 0; side < 2; side++) {
            for (int k = 0; k < 9; k++) {
                char** list = (char**)std::calloc(2, sizeof(char*));
                list[0] = _strdup(words[k]);
                if (k == 8) list[1] = _strdup("Plain");
                lists[side][k] = list;
                if (side == 0) *(char***)(uintptr_t)kStatics[which[k]] = list;
                else *kOurs[which[k]] = list;
            }
        }
        { On on(0); at<FnLdb>(0x0001A4B0)(o, nullptr, g_own[0], 2, prefix, 0); }
        { On on(1); r->LoadDatabases(g_own[1], 2, prefix, 0); }
        o->m_owner = g_own[0];
        r->m_owner = g_own[1];
        same_loader("names kept", o, r);
        { On on(0); at<Fn0>(0x000198C0)(o, nullptr); }
        { On on(1); r->FreeNames(); }
        same_loader("names freed", o, r);
        o->m_owner = nullptr;
        r->m_owner = nullptr;
    }
    // Freeing with one list at a time, so that a list given the wrong count or left unfreed shows.
    for (size_t i = 0; i < sizeof kStatics / sizeof kStatics[0]; i++) {
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        *(unsigned short*)((char*)o + kCounts[i]) = 2;
        *(unsigned short*)((char*)r + kCounts[i]) = 2;
        char** a = (char**)std::calloc(2, sizeof(char*));
        char** b = (char**)std::calloc(2, sizeof(char*));
        a[0] = (char*)std::malloc(2);
        b[0] = (char*)std::malloc(2);
        a[1] = (char*)std::malloc(2);
        b[1] = (char*)std::malloc(2);
        *(char***)(uintptr_t)kStatics[i] = a;
        *kOurs[i] = b;
        // Deletes are logged for the original's by its redirected delete, and for the recovered by the global one: the same count of them means the same count of names.
        { On on(0); at<Fn0>(0x000198C0)(o, nullptr); }
        { On on(1); r->FreeNames(); }
        same_loader("one list freed", o, r);
    }
}

static void set_heaps(void* h);

static void run_reset_and_destroy() {
    g_context = "reset";
    make_owners();
    static char* nothing[1];
    for (size_t i = 0; i < sizeof kStatics / sizeof kStatics[0]; i++) {
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        *(char***)(uintptr_t)kStatics[i] = nothing;
        *kOurs[i] = nothing;
        { On on(0); at<Fn0>(0x0001AB10)(o, nullptr); }
        { On on(1); r->AtDatabaseLoader::Unload(); }
        same_loader("reset empties a list", o, r);
    }
    g_context = "destroy";
    for (int owned : {0, 1}) {
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        g_side = 0; TestChild* co = make<TestChild>(0);
        g_side = 1; TestChild* cr = make<TestChild>(0);
        g_side = 0;
        co->clear_items();
        cr->clear_items();
        o->m_child = co;
        r->m_child = cr;
        o->m_owner = owned ? g_own[0] : nullptr;
        r->m_owner = owned ? g_own[1] : nullptr;
        { On on(0); at<Fn0>(0x0001AD40)(o, nullptr); }
        { On on(1); ((AtDatabaseLoader*)r)->AtDatabaseLoader::~AtDatabaseLoader(); }
        compare_logs("destroy with and without an owner");
    }
    g_context = "joined name with no heap";
    {
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        TestLoader* both[2] = {o, r};
        for (int side = 0; side < 2; side++) {
            g_side = side;
            both[side]->m_imageCount = 1;
            both[side]->MakeImages();
            char** list = (char**)std::calloc(1, sizeof(char*));
            list[0] = _strdup("D\\Im");
            if (side == 0) *(char***)(uintptr_t)0x002C62A8 = list;
            else AtDatabaseLoader::s_imageNames = list;
        }
        g_side = 0;
        g_log[0].n = g_log[1].n = 0;
        *(void**)(uintptr_t)0x002C60B0 = nullptr;
        AtIoTokenFile::s_heap = nullptr;
        const bool oc = guarded([&] { On on(0); at<FnLdb>(0x0001A4B0)(o, nullptr, g_own[0], 0, "PRE", 0); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->LoadDatabases(g_own[1], 0, "PRE", 0); });
        g_recording = false;
        set_heaps(&g_heap_token);
        eq("crash with no heap for a name", oc, rc);
        compare_logs("no heap for a joined name");
        g_fs->m_path.m_top = -1;
    }
}

// ---- the seam, on the recovered side alone ----------------------------------------------------------------------------

static void run_seam() {
    g_context = "seam";
    make_owners();
    for (const char* word : {"CoordSysMirrorDatabase", "WeightsDatabase", "MorphAnimationDatabase", "LightAnimationDatabase"}) {
        g_files["MODEL\\SEAM.AGD"] = std::string(word) + "[1]\n{\n \"A\"\n}\n";
        TestLoader* r = on_side(1);
        LoadArgs a = {r, "MODEL\\SEAM", 0, nullptr, 0};
        load_r(&a);
        g_recording = false;
        eq("the seam stops", std::strstr(g_message[1], "is not recovered") != nullptr, 1);
        g_log[1].n = 0;
        g_message[1][0] = 0;
        g_syntax_errors[1] = 0;
        r->m_owner = nullptr;
    }
}

// ---- meshes -----------------------------------------------------------------------------------------------------------

static void same_indices(const char* what, const TestIndices& o, const TestIndices& r) {
    eq(what, o.m_count, r.m_count);
    eq(what, o.m_flags, r.m_flags);
    eq(what, o.m_loadFlags, r.m_loadFlags);
    eq(what, std::memcmp(o.m_name, r.m_name, 8), 0);
    eq(what, o.m_index, r.m_index);
}

static void same_meshdb(const char* what, TestMeshDb* o, TestMeshDb* r) {
    compare_logs(what);
    eq("meshes", o->m_meshCount, r->m_meshCount);
    eq("unknown 18", o->m_unknown18, r->m_unknown18);
    eq("arrays", o->m_vertexArrayCount, r->m_vertexArrayCount);
    eq("lists", o->m_indexArrayCount, r->m_indexArrayCount);
    same_map("mesh map", *o, *r, o->m_m, r->m_m);
    for (int i = 0; i < TestMeshDb::kMax; i++) same_indices("index array", o->m_i[i], r->m_i[i]);
    eq("indices", std::memcmp(g_indices[0], g_indices[1], sizeof g_indices[0]), 0);
    eq("mesh space", g_own[0]->m_meshes.m_first == o, g_own[1]->m_meshes.m_first == r);
    eq("mesh space name", std::strncmp(o->m_group ? o->m_group->m_name : "", r->m_group ? r->m_group->m_name : "", 8), 0);
}

static void run_mesh_text(const std::string& text, const std::string& context, int mode, int binary, unsigned char fill) {
    g_context = context;
    g_files["MESH\\THING.AGG"] = text;
    g_fill = fill;
    g_binary = binary;
    make_owners();
    g_side = 0;
    TestMeshDb* o = make<TestMeshDb>(0x0002ADD0);
    g_side = 1;
    TestMeshDb* r = make<TestMeshDb>(0);
    g_side = 0;
    std::memset(g_indices, 0xEE, sizeof g_indices);
    for (int i = 0; i < TestMeshDb::kMax; i++) { o->m_i[i].m_flags = 0xFFFF; r->m_i[i].m_flags = 0xFFFF; }  // What Read must clear.
    for (int twice = 0; twice < 2; twice++) {
        { On on(0); at<FnLoad3>(0x0002AFE0)(o, nullptr, g_own[0], "MESH\\THING", mode); }
        eq("directory stack after", g_fs->m_path.m_top, -1);
        g_fs->m_path.m_top = -1;
        { On on(1); r->Load(g_own[1], "MESH\\THING", mode); }
        eq("directory stack after", g_fs->m_path.m_top, -1);
        g_fs->m_path.m_top = -1;
        same_meshdb(twice ? "load again" : "load", o, r);
    }
    { On on(0); o->Clear(); }
    { On on(1); r->Clear(); }
    same_meshdb("clear", o, r);
    { On on(0); o->AtMeshDatabase::~AtMeshDatabase(); }
    { On on(1); r->AtMeshDatabase::~AtMeshDatabase(); }
    compare_logs("destroyed");
    std::free(o);
    std::free(r);
    g_binary = 0;
}

static void run_mesh_written() {
    static const char* const texts[] = {
        "", "Mesh[0]\n{\n}\n", "Mesh[1]\n{\n Mesh \"M\"\n {\n }\n}\n", "NameSpace \"Sp\"\nMesh[1]\n{\n Mesh \"M\"\n {\n }\n}\n", "NameSpace \"\"\nMesh[1]\n{\n Mesh \"M\"\n {\n }\n}\n",
        "Mesh[2]\n{\n Mesh \"A\"\n {\n }\n Mesh \"LongerThanEight\"\n {\n }\n}\nMesh[1]\n{\n Mesh \"NotRead\"\n {\n }\n}\n",
        "Other[1]\n{\n Mesh \"M\"\n {\n }\n}\n", "Mesh[1]\n{\n Other \"M\"\n {\n }\n}\n",
        "VertexArray[2]\n{\n VertexArray 11 }\n VertexArray 12 }\n}\nMesh[1]\n{\n Mesh \"M\"\n {\n }\n}\n",
        "VertexArray[1]\n{\n VertexArray 11 }\n}\nVertexArray[1]\n{\n VertexArray 12 }\n}\nMesh[0]\n{\n}\n",
        "VertexArray[1]\n{\n Other 11 }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index[3]\n {\n 1 2 3\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index8Bit\n Index[3]\n {\n 257 2 3\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index32Bit\n Index[3]\n {\n 70000 2 3\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index16Bit\n Index[3]\n {\n 70000 2 3\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index32Bit\n Index16Bit\n Index[2]\n {\n 70000 2\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index8Bit\n Index32Bit\n Index[2]\n {\n 70000 2\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FrequentModify\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n InfrequentModify\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FixedSysMem\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FixedSysMemOnly\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FixedSysMemOnly\n FrequentModify\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FrequentModify\n FixedSysMem\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FixedSysMem\n FrequentModify\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n InfrequentModify\n FixedSysMem\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FrequentModify\n FixedSysMemOnly\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n InfrequentModify\n FixedSysMemOnly\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FixedSysMem\n InfrequentModify\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n FrequentModify\n InfrequentModify\n FixedSysMemOnly\n Index8Bit\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index8Bit\n FixedSysMemOnly\n FixedSysMem\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Other\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n IndexArray\n {\n Index[9]\n {\n DemandLoad \"Name\"\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[2]\n{\n IndexArray\n {\n Index[1]\n {\n 1\n }\n }\n IndexArray\n {\n Index8Bit\n Index[2]\n {\n 7 8\n }\n }\n}\nIndexArray[1]\n{\n IndexArray\n {\n Index[1]\n {\n 9\n }\n }\n}\nMesh[0]\n{\n}\n",
        "IndexArray[1]\n{\n Other\n {\n Index[1]\n {\n 1\n }\n }\n}\nMesh[0]\n{\n}\n",
        "VertexArray[1]\n{\n VertexArray 11 }\n}\nIndexArray[1]\n{\n IndexArray\n {\n Index[1]\n {\n 1\n }\n }\n}\nVertexArray[1]\n{\n VertexArray 11 }\n}\nMesh[1]\n{\n Mesh \"M\"\n {\n }\n}\n",
    };
    for (const char* t : texts) {
        for (unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
            run_mesh_text(t, std::string("mesh [") + t + "]", 0, 0, fill);
        }
    }
    run_mesh_text("Mesh[0]\n{\n}\n", "mode 2, no binary", 2, 0, 0xCD);
    run_mesh_text("Mesh[0]\n{\n}\n", "mode 2, binary", 2, 1, 0xCD);
    run_mesh_text("Mesh[0]\n{\n}\n", "mode 1, binary", 1, 1, 0xCD);
    run_mesh_text("Mesh[0]\n{\n}\n", "mode 3, binary", 3, 1, 0xCD);
}

// Own arrays finish; each triangle gets its positions and axis.
static void run_triangles() {
    g_context = "triangles";
    typedef void(__cdecl* FnNormal)(const void*, float*);
    typedef void(__cdecl* FnAxis)(void*);
    static const float cases[][9] = {
        {0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 1, 0, 0}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 1, 2, 3, 1, 2, 3},
        {0.1f, 0.2f, 0.3f, 1.1f, -3.7f, 2.25f, -4.5f, 0.125f, 9.75f}, {1, 1, 0, 2, 1, 0, 1, 2, 0}, {1e20f, 0, 0, 0, 1e20f, 0, 0, 0, 1e20f}, {1e-20f, 0, 0, 0, 1e-20f, 0, 0, 0, 1e-20f},
        {0, 0, 0, 1, 1, 0, 1, 0, 1}, {0, 0, 0, 1, 0, 1, 0, 1, 1}, {0, 0, 0, 1, 1, 1, 1, -1, 1}, {0, 0, 0, 1, 1, 1, -1, 1, 1}, {0, 0, 0, 1, 1, 0, -1, 1, 0},
        {NAN, 0, 0, 1, 0, 0, 0, 1, 0}, {0, NAN, 0, 1, 0, 0, 0, 1, 0}, {0, 0, NAN, 1, 0, 0, 0, 1, 0}, {INFINITY, 0, 0, 1, 0, 0, 0, 1, 0}, {3, 1, 4, 1, 5, 9, 2, 6, 5}, {-2, 7, 1, 8, -2, 8, 1, 8, -2},
    };
    for (const auto& c : cases) {
        float v[3][3];
        std::memcpy(v, c, sizeof v);
        AtMathTriangle a = {}, b = {};
        for (int i = 0; i < 3; i++) a.m_vertices[i] = b.m_vertices[i] = v[i];
        float na[3], nb[3];
        at<FnNormal>(0x00043B70)(&a, na);
        AtMathTriangleNormal(&b, nb);
        eq("normal", std::memcmp(na, nb, sizeof na), 0);
        at<FnAxis>(0x00043BE0)(&a);
        AtMathTriangleSetAxis(&b);
        eq("axis", a.m_axis, b.m_axis);
    }
    // Through the mesh.
    for (int ownArrays : {0, 2}) {
        for (int ownLists : {0, 2}) {
            for (int nullEntry : {-1, 0, 1}) {
                g_side = 0; TestMesh* mo = make<TestMesh>(0);
                g_side = 1; TestMesh* mr = make<TestMesh>(0);
                g_side = 0; TestMeshDb* doo = make<TestMeshDb>(0);
                g_side = 1; TestMeshDb* dor = make<TestMeshDb>(0);
                g_side = 0;
                for (int side = 0; side < 2; side++) {
                    for (int i = 0; i < 64; i++) for (int k = 0; k < 3; k++) g_positions[side][i][k] = (float)((i * 7 + k * 3) % 11) - 5.0f + (float)k * 0.5f;
                }
                AtMathTriangle to[6] = {}, tr[6] = {};
                const unsigned char arrays[6] = {0, 1, 1, 0, 5, 3};
                for (int i = 0; i < 6; i++) {
                    for (int k = 0; k < 3; k++) to[i].m_vertices[k] = tr[i].m_vertices[k] = (float*)(uintptr_t)(i * 5 + k * k + 1);
                    to[i].m_array = tr[i].m_array = ownArrays ? (arrays[i] % 3) : arrays[i];
                }
                TestMesh* both[2] = {mo, mr};
                TestMeshDb* dbs[2] = {doo, dor};
                AtMathTriangle* ts[2] = {to, tr};
                for (int s = 0; s < 2; s++) {
                    both[s]->m_ownVertexArrays = (unsigned)ownArrays;
                    both[s]->m_ownIndexArrays = (unsigned)ownLists;
                    both[s]->m_triangleCount = 6;
                    both[s]->m_triangles = ts[s];
                    both[s]->m_database = dbs[s];
                    both[s]->m_nullArray = nullEntry;
                    both[s]->m_arrays[1].m_flags = 0;
                    dbs[s]->m_v[1].m_flags = 0;
                    dbs[s]->m_v[3].m_flags = 0x1F;
                }
                { On on(0); at<Fn0>(0x00037220)(mo, nullptr); }
                { On on(1); mr->Finish(); }
                compare_logs("mesh finish");
                eq("triangles", std::memcmp(ts[0] + 0, ts[0] + 0, 0), 0);
                for (int i = 0; i < 6; i++) {
                    eq("axis", to[i].m_axis, tr[i].m_axis);
                    for (int k = 0; k < 3; k++) {
                        const long long off = (uintptr_t)to[i].m_vertices[k] >= (uintptr_t)g_positions[0] && (uintptr_t)to[i].m_vertices[k] < (uintptr_t)g_positions[1] ? (char*)to[i].m_vertices[k] - (char*)g_positions[0] : (long long)(uintptr_t)to[i].m_vertices[k];
                        const long long ofr = (uintptr_t)tr[i].m_vertices[k] >= (uintptr_t)g_positions[1] && (uintptr_t)tr[i].m_vertices[k] < (uintptr_t)g_positions[1] + sizeof g_positions[1] ? (char*)tr[i].m_vertices[k] - (char*)g_positions[1] : (long long)(uintptr_t)tr[i].m_vertices[k];
                        eq("vertex", off, ofr);
                    }
                }
                // The database's finish.
                for (int s = 0; s < 2; s++) { dbs[s]->m_vertexArrayCount = 2; dbs[s]->m_indexArrayCount = 3; dbs[s]->m_meshCount = 2; }
                for (int m = 0; m < 2; m++) { for (int s = 0; s < 2; s++) { dbs[s]->m_m[m].m_database = dbs[s]; } }
                { On on(0); at<Fn0>(0x0002AD60)(doo, nullptr); }
                { On on(1); dor->Finish(); }
                compare_logs("database finish");
            }
        }
    }
}

// ---- animations -------------------------------------------------------------------------------------------------------

static void same_list(const char* what, const AtAnimationComponent* o, const AtAnimationComponent* r, unsigned oc, unsigned rc) {
    eq(what, oc, rc);
    eq(what, o != nullptr, r != nullptr);
    if (o && r && oc == rc) eq(what, std::memcmp(o, r, oc * sizeof *o), 0);
}

static void same_animdb(const char* what, TestAnimDb* o, TestAnimDb* r) {
    compare_logs(what);
    eq("device", o->m_device != nullptr, r->m_device != nullptr);
    eq("keys", o->m_keys != nullptr, r->m_keys != nullptr);
    same_list("rotation", o->m_rotation, r->m_rotation, o->m_rotationCount, r->m_rotationCount);
    same_list("translation", o->m_translation, r->m_translation, o->m_translationCount, r->m_translationCount);
    same_list("scale", o->m_scale, r->m_scale, o->m_scaleCount, r->m_scaleCount);
    same_list("note", o->m_note, r->m_note, o->m_noteCount, r->m_noteCount);
    same_list("camera", o->m_camera, r->m_camera, o->m_cameraCount, r->m_cameraCount);
    eq("animations", o->m_animationCount, r->m_animationCount);
    eq("animations", o->m_animations != nullptr, r->m_animations != nullptr);
    same_map("animation map", *o, *r, o->m_animations, r->m_animations);
    if (o->m_keys && r->m_keys) {
        eq("key flags", o->m_keys->m_flags, r->m_keys->m_flags);
        eq("key count", o->m_keys->m_count, r->m_keys->m_count);
    }
    if (o->m_animations && r->m_animations && o->m_animationCount == r->m_animationCount) {
        for (unsigned i = 0; i < o->m_animationCount; i++) {
            const AtAnimation& a = o->m_animations[i];
            const AtAnimation& b = r->m_animations[i];
            eq("its database", a.m_database == o, b.m_database == r);
            eq("an animation", std::memcmp((const char*)&a + 4, (const char*)&b + 4, sizeof a - 4), 0);
        }
    }
}

static unsigned g_animations;

static void run_anim_text(const std::string& text, const std::string& context, int mode, int binary, unsigned char fill) {
    g_context = context;
    g_files["ANIM\\THING.AKA"] = text;
    g_fill = fill;
    g_binary = binary;
    make_owners();
    g_side = 0;
    TestAnimDb* o = make<TestAnimDb>(0x0002A770);
    g_side = 1;
    TestAnimDb* r = make<TestAnimDb>(0);
    g_side = 0;
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtAnimationDatabase) - 4), 0);
    for (int twice = 0; twice < 2; twice++) {
        { On on(0); at<FnLoad3>(0x0002A8E0)(o, nullptr, &g_device, "ANIM\\THING", mode); }
        { On on(1); r->Load(&g_device, "ANIM\\THING", mode); }
        same_animdb(twice ? "load again" : "load", o, r);
        g_animations += r->m_animationCount;
    }
    { On on(0); o->Clear(); }
    { On on(1); r->Clear(); }
    same_animdb("clear", o, r);
    { On on(0); o->Clear(); }
    { On on(1); r->Clear(); }
    same_animdb("clear nothing", o, r);
    // The complete destructor, on something read.
    { On on(0); at<FnLoad3>(0x0002A8E0)(o, nullptr, &g_device, "ANIM\\THING", 0); }
    { On on(1); r->Load(&g_device, "ANIM\\THING", 0); }
    same_animdb("load before destroying", o, r);
    { On on(0); at<Fn0>(0x0002AA80)(o, nullptr); }
    { On on(1); r->AtAnimationDatabase::~AtAnimationDatabase(); }
    compare_logs("destroyed");
    std::free(o);
    std::free(r);
    g_binary = 0;
}

static void run_anim_written() {
    static const char* const texts[] = {
        "", "Other\n", "5\n",
        "KeyframeSet\n{\n}\n",
        "KeyframeSet\n{\n RotationKey 1\n TranslationKey 2\n ScaleKey 3\n NoteKey 4\n CameraKey 5\n}\n",
        "KeyframeSet\n{\n FrameNumber[2]\n {\n 7\n }\n}\n",
        "KeyframeSet\n{\n FrameNumber[1]\n {\n DemandLoad \"Name\"\n }\n}\n",
        "KeyframeSet\n{\n Other\n}\n",
        "KeyframeSet\n{\n RotationKey 1\n}\nKeyframeSet\n{\n ScaleKey 2\n}\n",
        "KeyframeSet\n{\n FrameNumber[2]\n {\n 7\n }\n}\nKeyframeSet\n{\n}\n",
        "RotAnimComponent[2]\n{\n 1 2 3 4\n 5 6 7 8\n}\n",
        "TransAnimComponent[1]\n{\n 11 12 13 14\n}\n", "ScaleAnimComponent[1]\n{\n 21 22 23 24\n}\n", "NoteAnimComponent[1]\n{\n 31 32 33 34\n}\n", "CameraAnimComponent[1]\n{\n 41 42 43 44\n}\n",
        "RotAnimComponent[1]\n{\n 1 2 0 4\n}\n", "ScaleAnimComponent[1]\n{\n 1 2 65536 70000\n}\n", "RotAnimComponent[0]\n{\n}\n",
        "RotAnimComponent[1]\n{\n 1 2 3 4\n}\nRotAnimComponent[1]\n{\n 5 6 7 8\n}\n", "TransAnimComponent[1]\n{\n 1 2 3 4\n}\nTransAnimComponent[1]\n{\n 5 6 7 8\n}\n",
        "ScaleAnimComponent[1]\n{\n 1 2 3 4\n}\nScaleAnimComponent[1]\n{\n 5 6 7 8\n}\n", "NoteAnimComponent[1]\n{\n 1 2 3 4\n}\nNoteAnimComponent[1]\n{\n 5 6 7 8\n}\n", "CameraAnimComponent[1]\n{\n 1 2 3 4\n}\nCameraAnimComponent[1]\n{\n 5 6 7 8\n}\n",
        "RotAnimComponent[1]\n{\n 1 2 3 4\n}\nTransAnimComponent[2]\n{\n 5 6 7 8\n 9 10 11 12\n}\nScaleAnimComponent[3]\n{\n 1 1 1 1\n 2 2 2 2\n 3 3 3 3\n}\nNoteAnimComponent[1]\n{\n 4 4 4 4\n}\nCameraAnimComponent[1]\n{\n 5 5 5 5\n}\n",
        "Animation[0]\n{\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n }\n}\n",
        "Animation[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "Animation[2]\n{\n Animation \"A\"\n {\n }\n Animation \"LongerThanEight\"\n {\n Rate 30.5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n RotAnimComponent 1 2\n TransAnimComponent 3 4\n ScaleAnimComponent 5 6\n NoteAnimComponent 7 8\n CameraAnimComponent 9 10\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n RotAnimComponent 65537 70000\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n Rate 1.5\n VelocityVector 1.5 2.5 3.5\n AngularVelocity 4.5 5.5 6.5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n LinearVelocity 7.5 8.5 9.5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n VelocityVector 1.5 2.5 3.5\n LinearVelocity 7.5 8.5 9.5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n Length 10\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n Length 10\n Length 20\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n LoopPeriod 5\n Length 10\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n Length 10\n LoopPeriod 5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n Length 65535\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n LoopPeriod 70000\n Length 70001\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n BoundingSphere 1.5 2.5 3.5 4.5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n Other\n Rate 2.5\n }\n}\n",
        "Animation[1]\n{\n Animation \"A\"\n {\n FrameNumber 2.5\n }\n}\n",
        "KeyframeSet\n{\n RotationKey 1\n}\nRotAnimComponent[1]\n{\n 1 2 3 4\n}\nAnimation[1]\n{\n Animation \"A\"\n {\n RotAnimComponent 0 1\n Rate 30.0\n Length 4\n }\n}\n",
    };
    for (const char* t : texts) {
        for (unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
            run_anim_text(t, std::string("anim [") + t + "]", 0, 0, fill);
        }
    }
    // A second Animation list: the map's Init finds a table, and so calls Clear through the table, which frees all of it, the keys too. Then the end of the file reaches for the keys. A crash on both sides.
    {
        g_context = "second animation list";
        g_files["ANIM\\TWO.AKA"] = "Animation[1]\n{\n Animation \"A\"\n {\n }\n}\nAnimation[1]\n{\n Animation \"B\"\n {\n }\n}\n";
        g_fill = 0xCD;
        make_owners();
        g_side = 0; TestAnimDb* o = make<TestAnimDb>(0x0002A770);
        g_side = 1; TestAnimDb* r = make<TestAnimDb>(0);
        g_side = 0;
        const bool oc = guarded([&] { On on(0); at<FnLoad3>(0x0002A8E0)(o, nullptr, &g_device, "ANIM\\TWO", 0); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Load(&g_device, "ANIM\\TWO", 0); });
        g_recording = false;
        eq("crash on a second list", oc, rc);
        eq("it does crash", oc, 1);
        compare_logs("second animation list");
    }
    run_anim_text("Other\n", "mode 2, no binary", 2, 0, 0xCD);
    run_anim_text("Other\n", "mode 2, binary", 2, 1, 0xCD);
    run_anim_text("Other\n", "mode 1, binary", 1, 1, 0xCD);
    run_anim_text("Other\n", "mode 3, binary", 3, 1, 0xCD);
}

// ---- processes --------------------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// Reading over what is there, which no file can reach: a database's list makes new animations.
static void run_read_over() {
    g_context = "read over";
    typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
    for (const char* first : {"{ Rate 1.5 Length 9 RotAnimComponent 1 2 BoundingSphere 1.0 2.0 3.0 4.0 }", "{ }"}) {
        AtAnimation a, b;
        int database[2];
        for (int side = 0; side < 2; side++) {
            for (const char* text : {first, "{ LoopPeriod 3 }"}) {
                std::string copy(text);
                MemoryStream stream;
                AtIoTextTokenizer tokenizer;
                stream.m_base = stream.m_cur = &copy[0];
                stream.m_limit = stream.m_base + copy.size();
                stream.m_size = (unsigned)copy.size();
                tokenizer.SetStream(&stream);
                tokenizer.SetKeywords(AtAnimationDatabase::s_keywords, AtAnimationDatabase::kKeywordCount, nullptr);
                if (side == 0) at<FnRead>(0x00036240)(&a, nullptr, &database[0], &tokenizer);
                else b.Read((AtAnimationDatabase*)&database[1], &tokenizer);
                stream.m_base = stream.m_cur = stream.m_limit = nullptr;
                tokenizer.SetStream(nullptr);
            }
        }
        eq("read over", std::memcmp((char*)&a + 4, (char*)&b + 4, sizeof a - 4), 0);
    }
}

template <class T> static void same_process(const char* what, T* o, T* r) {
    compare_logs(what);
    eq("count", o->m_count, r->m_count);
    same_map("process map", *o, *r, o->m_items, r->m_items);
}

template <class T, class FLoad> static void run_process(const std::string& text, const std::string& context, uint32_t ctor, uint32_t load, uint32_t complete, const char* path, const char* key, FLoad) {
    g_context = context;
    g_files[key] = text;
    make_owners();
    g_side = 0;
    T* o = make<T>(ctor);
    g_side = 1;
    T* r = make<T>(0);
    g_side = 0;
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(GmNameMap) + 4 - 4), 0);
    for (int twice = 0; twice < 2; twice++) {
        { On on(0); at<FnLoad3>(load)(o, nullptr, g_own[0], path, 3); }
        eq("directory stack after", g_fs->m_path.m_top, -1);
        g_fs->m_path.m_top = -1;
        { On on(1); r->Load(g_own[1], path, 3); }
        eq("directory stack after", g_fs->m_path.m_top, -1);
        g_fs->m_path.m_top = -1;
        same_process(twice ? "load again" : "load", o, r);
        eq("in its group", o->m_group != nullptr, r->m_group != nullptr);
    }
    { On on(0); o->Clear(); }
    { On on(1); r->Clear(); }
    same_process("clear", o, r);
    { On on(0); at<Fn0>(complete)(o, nullptr); }
    { On on(1); r->T::~T(); }
    compare_logs("destroyed");
    eq("count after", o->m_count, r->m_count);
    std::free(o);
    std::free(r);
}

static void run_process_written() {
    static const char* const vertex[] = {"", "Other[0]\n{\n}\n", "VertexProcess[0]\n{\n}\n", "VertexProcess[1]\n{\n VertexProcess \"A\"\n {\n }\n}\n", "VertexProcess[2]\n{\n VertexProcess \"A\"\n {\n }\n VertexProcess \"LongerThanEight\"\n {\n }\n}\n", "VertexProcess[1]\n{\n Other \"A\"\n {\n }\n}\n"};
    static const char* const pixel[] = {"", "Other[0]\n{\n}\n", "PixelProcess[0]\n{\n}\n", "PixelProcess[1]\n{\n PixelProcess \"A\"\n {\n }\n}\n", "PixelProcess[2]\n{\n PixelProcess \"A\"\n {\n }\n PixelProcess \"LongerThanEight\"\n {\n }\n}\n", "PixelProcess[1]\n{\n Other \"A\"\n {\n }\n}\n"};
    for (const char* t : vertex) run_process<TestVertexDb>(t, std::string("vertex [") + t + "]", 0x0002B690, 0x0002B7A0, 0x0002B870, "P\\THING", "P\\THING.AGV", 0);
    for (const char* t : pixel) run_process<TestPixelDb>(t, std::string("pixel [") + t + "]", 0x0002B430, 0x0002B540, 0x0002B610, "P\\THING", "P\\THING.AGP", 0);
}

// ---- helpers, tables, no heap, no file --------------------------------------------------------------------------------

static void same_table(const char* what, uint32_t va, const AtIoKeyword* ours, unsigned count) {
    g_context = what;
    const AtIoKeyword* theirs = (const AtIoKeyword*)(uintptr_t)va;
    for (unsigned i = 0; i < count; i++) {
        eq("id", theirs[i].m_id, ours[i].m_id);
        eqs("word", theirs[i].m_name, ours[i].m_name);
    }
}

static void run_helpers() {
    same_table("table", 0x002B0190, AtDatabaseLoader::s_keywords, AtDatabaseLoader::kKeywordCount);
    same_table("mesh table", 0x002B0AA0, AtMeshDatabase::s_keywords, AtMeshDatabase::kKeywordCount);
    same_table("vertex table", 0x002B0F58, AtVertexProcessDatabase::s_keywords, AtVertexProcessDatabase::kKeywordCount);
    same_table("pixel table", 0x002B0E90, AtPixelProcessDatabase::s_keywords, AtPixelProcessDatabase::kKeywordCount);
    same_table("animation table", 0x002B0990, AtAnimationDatabase::s_keywords, AtAnimationDatabase::kKeywordCount);

    g_context = "leaf";
    typedef void(__cdecl* FnLeaf)(const char*, char*);
    for (const char* p : {"", "A", "A.B", "A.B.C", "D\\A", "D\\E\\A.X", "D/A", "D:A", "D/E\\A", "D\\E/A", "C:D/A", "ABCDEFG", "ABCDEFGH", "ABCDEFGHI", "ABCDEFGHI.J", "ABCDEFG.H", "X.Y\\A", "X\\", "X/", "X:", "\\", ".A", "D\\.A"}) {
        char a[12], b[12];
        std::memset(a, 0x77, sizeof a);
        std::memset(b, 0x77, sizeof b);
        at<FnLeaf>(0x00019730)(p, a);
        AtLeafName(p, b);
        eq(p, std::memcmp(a, b, sizeof a), 0);
    }
    g_context = "join";
    typedef void(__fastcall* FnJoin)(void*, void*, char*, char*, const char*);
    for (const char* n : {"", "A", "D\\A", "D\\E\\A", "\\A", "A\\"}) {
        for (const char* p : {"", "P", "PRE"}) {
            char a[64], b[64], na[16], nb[16];
            std::memset(a, 0x77, sizeof a);
            std::memset(b, 0x77, sizeof b);
            strcpy_s(na, n);
            strcpy_s(nb, n);
            at<FnJoin>(0x000197D0)(nullptr, nullptr, a, na, p);
            AtJoinPrefix(b, nb, p);
            eq(n, std::memcmp(a, b, sizeof a), 0);
            eqs("name put back", na, nb);
        }
    }
    g_context = "free list";
    typedef void(__fastcall* FnFree)(void*, void*, void*, unsigned);
    for (unsigned count : {0u, 1u, 3u}) {
        g_fill = 0;
        void** a = (void**)std::calloc(4, sizeof(void*));
        void** b = (void**)std::calloc(4, sizeof(void*));
        for (unsigned i = 0; i < count; i++) { if (i != 1) { a[i] = std::malloc(4); b[i] = std::malloc(4); } }
        { On on(0); at<FnFree>(0x000196F0)(nullptr, nullptr, a, count); }
        { On on(1); AtFreeNameList((char**)b, count); }
        compare_logs("free list");
    }
    g_context = "set heap";
    typedef void(__cdecl* FnSet)(void*);
    at<FnSet>(0x000196E0)(&g_heap_token);
    AtDatabaseLoader::SetHeap(&g_heap_token);
    eq("loader heap", (uintptr_t) * (void**)(uintptr_t)0x002C62DC, (uintptr_t)AtDatabaseLoader::s_heap);
    at<FnSet>(0x0002A520)(&g_heap_token);
    AtAnimationDatabase::SetHeap(&g_heap_token);
    eq("animation heap", (uintptr_t) * (void**)(uintptr_t)0x002C6374, (uintptr_t)AtAnimationDatabase::s_heap);

    g_context = "owner";
    make_owners();
    typedef void(__fastcall* FnAdd2)(void*, void*, void*, const char*);
    typedef void(__fastcall* FnAdd1)(void*, void*, void*);
    struct { uint32_t va; void (AtMaterialOwner::*add)(GmNameMap*, const char*); GmNameMapGroup AtMaterialOwner::*group; GmNameSpaceDb AtMaterialOwner::*spaces; } adds2[] = {
        {0x00025F30, &AtMaterialOwner::AddMeshes, &AtMaterialOwner::m_meshes, &AtMaterialOwner::m_meshSpaces},
        {0x00025FB0, &AtMaterialOwner::AddLightAnimations, &AtMaterialOwner::m_lightAnimations, &AtMaterialOwner::m_lightAnimationSpaces},
        {0x00026070, &AtMaterialOwner::AddMorphAnimations, &AtMaterialOwner::m_morphAnimations, &AtMaterialOwner::m_morphAnimationSpaces},
    };
    for (const auto& a : adds2) {
        for (const char* ns : {(const char*)nullptr, "", "Sp"}) {
            GmNameMap x, y;
            at<FnAdd2>(a.va)(g_own[0], nullptr, &x, ns);
            (g_own[1]->*a.add)(&y, ns);
            eq("in the plain group", (g_own[0]->*a.group).m_first == &x, (g_own[1]->*a.group).m_first == &y);
            eq("in a group", x.m_group != nullptr, y.m_group != nullptr);
            eq("the space", (g_own[0]->*a.spaces).m_last != nullptr, (g_own[1]->*a.spaces).m_last != nullptr);
            eq("group's name", std::strncmp(x.m_group->m_name, y.m_group->m_name, 8), 0);
        }
    }
    struct { uint32_t va; void (AtMaterialOwner::*add)(GmNameMap*); GmNameMapGroup AtMaterialOwner::*group; } adds1[] = {
        {0x00026130, &AtMaterialOwner::AddCoordSystems, &AtMaterialOwner::m_coordSystems},
        {0x00026140, &AtMaterialOwner::AddMirrors, &AtMaterialOwner::m_mirrors},
        {0x00026150, &AtMaterialOwner::AddWeights, &AtMaterialOwner::m_weights},
        {0x00026160, &AtMaterialOwner::AddAnimations, &AtMaterialOwner::m_animations},
        {0x00026170, &AtMaterialOwner::AddVertexProcesses, &AtMaterialOwner::m_vertexProcesses},
        {0x00026180, &AtMaterialOwner::AddPixelProcesses, &AtMaterialOwner::m_pixelProcesses},
    };
    for (const auto& a : adds1) {
        static GmNameMap x, y;
        at<FnAdd1>(a.va)(g_own[0], nullptr, &x);
        (g_own[1]->*a.add)(&y);
        eq("in the group", (g_own[0]->*a.group).m_first == &x, (g_own[1]->*a.group).m_first == &y);
    }
    // No child, and a child.
    g_context = "child";
    make_owners();
    for (int k = 0; k < 2; k++) {
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        g_side = 0; TestChild* co = make<TestChild>(0);
        g_side = 1; TestChild* cr = make<TestChild>(0);
        g_side = 0;
        co->clear_items();
        cr->clear_items();
        o->m_child = co;
        r->m_child = cr;
        o->m_owner = g_own[0];
        r->m_owner = g_own[1];
        { On on(0); at<Fn0>(0x0001AB10)(o, nullptr); }
        { On on(1); r->AtDatabaseLoader::Unload(); }
        same_loader("child", o, r);
    }
    // Keys and an array of animations in a direct reset.
    g_context = "animation reset";
    {
        AtAnimation a, b;
        at<Fn0>(0x00036210)(&a, nullptr);
        eq("constructed", std::memcmp(&a, &b, sizeof a), 0);
        std::memset(&a, 0x5A, sizeof a);
        std::memset(&b, 0x5A, sizeof b);
        at<Fn0>(0x00035B20)(&a, nullptr);
        b.Reset();
        eq("reset", std::memcmp(&a, &b, sizeof a), 0);
        std::memset(&a, 0x5A, sizeof a);
        std::memset(&b, 0x5A, sizeof b);
        at<Fn0>(0x00036230)(&a, nullptr);
        b.~AtAnimation();
        eq("destroyed", std::memcmp(&a, &b, sizeof a), 0);
    }
}

static void set_heaps(void* h) {
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C63F0u, 0x002C62DCu, 0x002C6374u}) *(void**)(uintptr_t)va = h;
    AtIoTokenFile::s_heap = h;
    AtIoStream::s_heap = h;
    GmNameMap::s_heap = h;
    GmNameSpaceDb::s_heap = h;
    AtMaterialDatabase::s_heap = h;
    AtDatabaseLoader::s_heap = h;
    AtAnimationDatabase::s_heap = h;
}

static void run_failures() {
    // No memory, and no heap, for the lists alone: counts of zero, so nothing writes where nothing was given.
    g_context = "no memory";
    static const char* const words[] = {"TextureDatabase", "ShaderDatabase", "MaterialDatabase", "MeshDatabase", "LightDatabase", "FontDatabase", "ImageDatabase", "VertexProcessDatabase", "PixelProcessDatabase", "DatabaseLoader"};
    for (const char* w : words) {
        g_files["MODEL\\NOMEM.AGD"] = std::string(w) + "[0]\n{\n}\n";
        make_owners();
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        LoadArgs ao = {o, "MODEL\\NOMEM", 0, nullptr, 0};
        LoadArgs ar = {r, "MODEL\\NOMEM", 0, nullptr, 0};
        // The file's own buffers need the heap, so only the list's own new fails: on the third request on, which is past the tokenizer's and the stream's.
        load_o(&ao);
        load_r(&ar);
        same_loader("with memory", o, r);
        // No heap for the token file: the raise, on both sides, at each stage that tests it.
        g_context = std::string("no heap ") + w;
        *(void**)(uintptr_t)0x002C60B0 = nullptr;
        AtIoTokenFile::s_heap = nullptr;
        const bool oc = guarded([&] { load_o(&ao); });
        g_recording = false;
        const bool rc = guarded([&] { load_r(&ar); });
        g_recording = false;
        eq("crash with no heap", oc, rc);
        set_heaps(&g_heap_token);
        compare_logs("no heap");
        g_fs->m_path.m_top = -1;
    }
    // A list of the loader's own, with no heap of its own.
    g_context = "no loader heap";
    for (const char* text : {"CoordSysDatabase[0]\n{\n}\n", "AnimationDatabase[0]\n{\n}\n", "MatAssignmentDatabase[0]\n{\n}\n", "MatAnimationDatabase[0]\n{\n}\n"}) {
        g_files["MODEL\\NOHEAP.AGD"] = text;
        make_owners();
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        LoadArgs ao = {o, "MODEL\\NOHEAP", 0, nullptr, 0};
        LoadArgs ar = {r, "MODEL\\NOHEAP", 0, nullptr, 0};
        *(void**)(uintptr_t)0x002C62DC = nullptr;
        AtDatabaseLoader::s_heap = nullptr;
        load_o(&ao);
        load_r(&ar);
        set_heaps(&g_heap_token);
        same_loader("no heap", o, r);
    }
    // Image names with no heap for the joining.
    g_context = "no heap for a joined name";
    {
        g_files["MODEL\\JOIN.AGD"] = "ImageDatabase[1]\n{\n \"A\"\n}\n";
        make_owners();
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        typedef void(__fastcall* FnReadOnly)(void*, void*, void*, int, const char*, int);
        LoadArgs ao = {o, "MODEL\\JOIN", 0, "PRE", 0};
        LoadArgs ar = {r, "MODEL\\JOIN", 0, "PRE", 0};
        load_o(&ao);
        load_r(&ar);
        same_loader("joined", o, r);
        (void)sizeof(FnReadOnly);
    }
    // An animation file with no heap.
    g_context = "no animation heap";
    for (const char* text : {"RotAnimComponent[0]\n{\n}\n", "Animation[0]\n{\n}\n"}) {
        g_files["ANIM\\NOHEAP.AKA"] = text;
        make_owners();
        g_side = 0; TestAnimDb* o = make<TestAnimDb>(0x0002A770);
        g_side = 1; TestAnimDb* r = make<TestAnimDb>(0);
        g_side = 0;
        *(void**)(uintptr_t)0x002C6374 = nullptr;
        AtAnimationDatabase::s_heap = nullptr;
        { On on(0); at<FnLoad3>(0x0002A8E0)(o, nullptr, &g_device, "ANIM\\NOHEAP", 0); }
        { On on(1); r->Load(&g_device, "ANIM\\NOHEAP", 0); }
        set_heaps(&g_heap_token);
        same_animdb("no heap", o, r);
    }
    // A file that will not open: the report, then what comes of no tokenizer. In the game the report does not come back.
    g_context = "no file";
    for (int status : {1, 2, 5}) {
        g_fs->open_status = status;
        make_owners();
        TestLoader* o = on_side(0);
        TestLoader* r = on_side(1);
        for (int dir : {0, 1}) {
            LoadArgs ao = {o, "MODEL\\THING", 0, nullptr, dir};
            LoadArgs ar = {r, "MODEL\\THING", 0, nullptr, dir};
            const bool oc = guarded([&] { load_o(&ao); });
            g_recording = false;
            const bool rc = guarded([&] { load_r(&ar); });
            g_recording = false;
            eq("crash on no file", oc, rc);
            compare_logs("no file");
        }
        g_side = 0; TestMeshDb* mo = make<TestMeshDb>(0x0002ADD0);
        g_side = 1; TestMeshDb* mr = make<TestMeshDb>(0);
        g_side = 0;
        eq("crash mesh", guarded([&] { On on(0); at<FnLoad3>(0x0002AFE0)(mo, nullptr, g_own[0], "M", 0); }), guarded([&] { On on(1); mr->Load(g_own[1], "M", 0); }));
        g_recording = false;
        compare_logs("no mesh file");
        g_side = 0; TestAnimDb* ao2 = make<TestAnimDb>(0x0002A770);
        g_side = 1; TestAnimDb* ar2 = make<TestAnimDb>(0);
        g_side = 0;
        eq("crash anim", guarded([&] { On on(0); at<FnLoad3>(0x0002A8E0)(ao2, nullptr, &g_device, "M", 0); }), guarded([&] { On on(1); ar2->Load(&g_device, "M", 0); }));
        g_recording = false;
        compare_logs("no animation file");
        g_side = 0; TestVertexDb* vo = make<TestVertexDb>(0x0002B690);
        g_side = 1; TestVertexDb* vr = make<TestVertexDb>(0);
        g_side = 0;
        eq("crash vertex", guarded([&] { On on(0); at<FnLoad3>(0x0002B7A0)(vo, nullptr, g_own[0], "M", 0); }), guarded([&] { On on(1); vr->Load(g_own[1], "M", 0); }));
        g_recording = false;
        compare_logs("no vertex file");
        g_side = 0; TestPixelDb* po = make<TestPixelDb>(0x0002B430);
        g_side = 1; TestPixelDb* pr = make<TestPixelDb>(0);
        g_side = 0;
        eq("crash pixel", guarded([&] { On on(0); at<FnLoad3>(0x0002B540)(po, nullptr, g_own[0], "M", 0); }), guarded([&] { On on(1); pr->Load(g_own[1], "M", 0); }));
        g_recording = false;
        compare_logs("no pixel file");
        g_fs->open_status = 0;
        g_fs->m_path.m_top = -1;
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
    if (pos.size() != 2) { std::printf("usage: oracle_agd <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new PathFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    set_heaps(&g_heap_token);
    g_heap_pad = 0x1000;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_helpers();
    run_written_agd();
    run_names_kept();
    run_reset_and_destroy();
    run_seam();
    run_mesh_written();
    run_triangles();
    run_anim_written();
    run_read_over();
    run_process_written();
    run_failures();

    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AGD\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::string text((size_t)std::ftell(f), 0);
        std::fseek(f, 0, SEEK_SET);
        if (!text.empty()) std::fread(&text[0], 1, text.size(), f);
        std::fclose(f);
        g_binary = 1;
        run_agd(text, line, 2, nullptr, 1, 0xCD, false);
        g_binary = 0;
        run_agd(text, line, 0, "PRE", 0, 0xCD, true);
        g_files_run++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u names, %u opens, %u animations; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files_run, g_names_compared, g_opens, g_animations, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
