// Differential oracle for the .ASD scene lists, the .ASC cell graphs they reach, and the .ASI and .AST flat lists (Windows x86, local only). See docs/verification.md.
//
//   oracle_asd <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// What stands in for what on real files, first. The kinds an .ASD lists whose loaders belong to other subsystems (.ASN nodes, .ASM modifiers, .ASG space graphs, .ASB bounding geometry, .ASE emitters, .ASL lens flares and .AIS
// strings) are not loaded here: on both sides their Load is redirected to a double that logs which object, which name, which mode and which owner it was given, and the current directory. Their objects are made and destroyed by the
// real constructors and destructors on each side. Cell graphs, flat images, flat text and nested lists load for real on both sides. Meshes, vertex arrays, images, fonts and strings are this harness's, found by name in maps that hold
// every quoted name of the case; the vertex positions are generated here, not the game's. So real .ASD files show that the lists, the order, the names, the directory and the cell graphs agree, not that any node or graph loads.
#define NOMINMAX
#include <float.h>
#include <malloc.h>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtSceneDatabaseLoader.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void enough() { if (g_failures > 6) { std::printf("stopping early: %d differences\n", g_failures); std::fflush(stdout); ExitProcess(1); } }
static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 200).c_str(), what, o, r);
    enough();
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 200).c_str(), what, o, r);
    enough();
}
// Bits, except that a NaN is a NaN.
static void eqf(const char* what, float o, float r) {
    uint32_t a, b;
    std::memcpy(&a, &o, 4);
    std::memcpy(&b, &r, 4);
    g_checks++;
    if (o != o && r != r) return;
    if (a != b && g_failures++ < 25) std::printf("DIFF %s: %s: original %.9g (%08X), recovered %.9g (%08X)\n", g_context.substr(0, 200).c_str(), what, o, a, r, b);
    enough();
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

// Frees are logged with the size of the block, so that the order of freeing shows.
namespace std { void quarantine_free(void* p); }
#define free(p) quarantine_free(p)
#define AtPushHeap AtPushHeapInDoubles
#define AtPopHeap AtPopHeapInDoubles
#define AtFatal AtFatalInDoubles
#include "oracle_doubles.h"
#undef AtPushHeap
#undef AtPopHeap
#undef AtFatal
#undef free
void std::quarantine_free(void* p) {
    if (g_recording && p) g_log[g_side].add('d', (uint32_t)_msize(p));
    ::free(p);
}

// A file ending inside a body makes both sides report for ever, since the report comes back here. A deliberate crash ends it, at the same report on both sides.
static int g_reports;
static void count_report() {
    if (g_expect_crash && ++g_reports > 40) *(volatile int*)nullptr = 0;
}
void AtFatal(const char* m, int a, int b, int c) { AtFatalInDoubles(m, a, b, c); count_report(); }
static void __cdecl o_fatal2(const char* m, int a, int b, int c) { o_fatal(m, a, b, c); count_report(); }

// Refusing memory to one heap only, after a number of its pushes.
static void* g_refuse_heap;
static int g_refusing;
static int g_refuse_skip[2];
static void note_push(int side, void* h) {
    if (!g_recording) return;
    if (h == g_refuse_heap && h) {
        if (g_refuse_skip[side] > 0) g_refuse_skip[side]--;
        else { g_fail_alloc = true; g_refusing++; }
    }
}
static void note_pop() { if (g_refusing) { g_refusing--; g_fail_alloc = false; } }
void AtPushHeap(void* h) { AtPushHeapInDoubles(h); note_push(1, h); }
void AtPopHeap() { AtPopHeapInDoubles(); note_pop(); }
static void __cdecl o_push2(void* h) { o_push(h); note_push(0, h); }
static void __cdecl o_pop2() { o_pop(); note_pop(); }

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnDatabases)(void*, void*, void*, int, int);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__cdecl* FnHeap)(void*);

// ---- files ---------------------------------------------------------------------------------------------------------------------------------

static std::map<std::string, std::string> g_files;
static std::string upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }

class PathFs : public MemFs {
public:
    int Open(const char* path, int flags, AtIoFile** out) override {
        const bool was = g_recording;
        g_recording = false;
        static char shown[600];
        {
            std::string key(path);
            const char* current = m_path.Current();
            if (current && *current) {
                std::string dir(current);
                while (!dir.empty() && dir.back() == '\\') dir.pop_back();
                key = dir + "\\" + key;
            }
            key = upper(key);
            auto it = g_files.find(key);
            static std::string empty;
            static std::string held[2];
            held[g_side] = it != g_files.end() ? it->second : empty;
            files[g_side].data = (const unsigned char*)held[g_side].data();
            files[g_side].size = held[g_side].size();
            strncpy_s(files[g_side].m_path, path, _TRUNCATE);
            strncpy_s(shown, key.c_str(), _TRUNCATE);
        }
        g_recording = was;
        record_message(shown, g_side);
        return MemFs::Open(path, flags, out);
    }
};

static PathFs* g_fs;
static int g_path_top;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];

// ---- heaps ---------------------------------------------------------------------------------------------------------------------------------

static int g_heap_token, g_heap_loader, g_heap_image, g_heap_text, g_heap_celldb, g_heap_cell;
static void set_heaps() {
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    const bool was = g_recording;
    g_recording = false;
    at<FnHeap>(0x0002E200)(&g_heap_loader); AtSceneDatabaseLoader::SetHeap(&g_heap_loader);
    at<FnHeap>(0x0003B030)(&g_heap_image); AtImageDrawDatabase::SetHeap(&g_heap_image);
    at<FnHeap>(0x0003B3C0)(&g_heap_text); AtTextDrawDatabase::SetHeap(&g_heap_text);
    at<FnHeap>(0x0003FC30)(&g_heap_celldb); AtCellGraphDatabase::SetHeap(&g_heap_celldb);
    at<FnHeap>(0x00052CC0)(&g_heap_cell); AtCellGraph::SetHeap(&g_heap_cell);
    g_recording = was;
}
static void* const kHeapAddress[] = {(void*)0x002C639C, (void*)0x002C6404, (void*)0x002C6408, (void*)0x002C641C, (void*)0x002C7EB0, (void*)0x002C60B0};
static void** const kHeapRecovered[] = {&AtSceneDatabaseLoader::s_heap, &AtImageDrawDatabase::s_heap, &AtTextDrawDatabase::s_heap, &AtCellGraphDatabase::s_heap, &AtCellGraph::s_heap, &AtIoTokenFile::s_heap};
static void* const kHeapToken[] = {&g_heap_loader, &g_heap_image, &g_heap_text, &g_heap_celldb, &g_heap_cell, &g_heap_token};

// ---- what the files name: meshes, vertex arrays, images, fonts, strings --------------------------------------------------------------------

enum { kVertices = 65536 + 64, kArrays = 3 };
static float* g_vertexData[kArrays];
static const unsigned kStrides[kArrays] = {12, 16, 28};

class TestArray : public AtVertexArray {
public:
    unsigned which = 0;
    float* PositionOf(float*) override { return nullptr; }
    void Slot1() override {} void Destroy(unsigned int) override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override { g_log[g_side].add('U', which); }
    void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {} void Slot12() override {} void Slot13() override {} void Slot14() override {}
    void Slot15() override {} void Finish() override {} void Slot17() override {} void Slot18() override {} void Slot19() override {} void Slot20() override {}
    void Read(AtMaterialOwner*, AtIoTokenizer*, unsigned int) override {}
    void Lock(AtVertexLock* out, int unknown) override {
        g_log[g_side].add('K', which);
        g_log[g_side].add('k', (uint32_t)unknown);
        out->m_data = g_vertexData[which];
        out->m_stride = kStrides[which];
    }
};
static TestArray g_arrays[kArrays];

class TestMeshDb : public AtMeshDatabase {
public:
    void MakeMeshes(AtMaterialOwner*) override {}
    void MakeVertexArrays(AtMaterialOwner*) override {}
    void MakeIndexArrays(AtMaterialOwner*) override {}
    void Slot8() override {}
    AtMesh* MeshAt(unsigned int) override { return nullptr; }
    AtVertexArray* VertexArrayAt(unsigned int index) override { g_log[g_side].add('B', index); return &g_arrays[index % kArrays]; }
    AtIndexArray* IndexArrayAt(unsigned int) override { return nullptr; }
};
static TestMeshDb* g_meshDb;

class TestMesh : public AtMesh {
public:
    void Slot0() override {} void Slot1() override {}
    AtVertexArray* OwnVertexArrayAt(unsigned int) override { return nullptr; }
    AtIndexArray* OwnIndexArrayAt(unsigned int) override { return nullptr; }
    void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {}
    void Read(AtMaterialOwner*, AtMeshDatabase*, AtIoTokenizer*, const char*) override {}
    AtVertexArray* VertexArrayAt(unsigned int index) override { g_log[g_side].add('V', index); return &g_arrays[index % kArrays]; }
};
// Three own arrays; none, so the database's; one, so that a reference to the second stops.
static TestMesh g_meshes[3];

static float g_scale[2] = {1.0f, 1.0f};
class TestDevice : public AtSceneDevice {
public:
    void Slot0() override {} void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {}
    void Present(int) override {} void Slot10() override {} void Slot11() override {} void Slot12() override {}
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
    void DrawScale(float* out) override { g_log[g_side].add('S'); out[0] = g_scale[0]; out[1] = g_scale[1]; }
};
static TestDevice* g_device;
static GmNameMap *g_meshMap, *g_meshMapNs, *g_imageMap, *g_imageMapNs, *g_fontMap, *g_fontMapNs, *g_stringMap;
static std::vector<int*> g_blobs;

static uint32_t hash_of(const std::string& s) { uint32_t h = 2166136261u; for (char c : s) h = (h ^ (unsigned char)c) * 16777619u; return h; }
static std::string name8(const std::string& s) { return s.substr(0, 8); }

static void gather(const std::string& text, std::set<std::string>& names) {
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '"') continue;
        const size_t end = text.find('"', i + 1);
        if (end == std::string::npos) break;
        const std::string s = name8(text.substr(i + 1, end - i - 1));
        if (!s.empty()) names.insert(s);
        i = end;
    }
}

// Every name of a case, but those starting with X, is something of each kind here.
static void provide(const std::set<std::string>& names) {
    const bool was = g_recording;
    g_recording = false;
    for (const std::string& s : names) {
        if (s[0] == 'X' || g_meshMap->Find(s.c_str())) continue;
        const uint32_t h = hash_of(s);
        // MeshA and MeshB have three arrays of their own, MeshC one, MeshD none; other names by hash.
        TestMesh* mesh = &g_meshes[h % 3];
        if (s.size() == 5 && s.compare(0, 4, "Mesh") == 0) mesh = &g_meshes[s[4] == 'C' ? 2 : s[4] == 'D' ? 1 : 0];
        // Each table finds something different under one name, so that looking in the wrong one shows.
        g_meshMap->Insert(s.c_str(), mesh);
        g_meshMapNs->Insert(s.c_str(), &g_meshes[(mesh - g_meshes + 1) % 3]);
        int* blob = new int[5]{(int)h, (int)(h >> 8), (int)(h >> 16), (int)(h >> 24), (int)h + 1};
        g_blobs.push_back(blob);
        int k = 0;
        for (GmNameMap* m : {g_imageMap, g_imageMapNs, g_fontMap, g_fontMapNs, g_stringMap}) m->Insert(s.c_str(), blob + k++);
    }
    g_recording = was;
}

static void make_world() {
    std::mt19937 rng(7);
    for (unsigned a = 0; a < kArrays; a++) {
        g_arrays[a].which = a;
        const unsigned floats = kVertices * kStrides[a] / 4;
        g_vertexData[a] = new float[floats];
        for (unsigned i = 0; i < floats; i++) {
            const unsigned r = (unsigned)rng();
            g_vertexData[a][i] = (float)((int)(r % 2000001) - 1000000) / (float)(1 + (r >> 24)) + (a == 2 ? 16777216.0f * ((r >> 20) & 1) : 0.0f);
        }
    }
    g_meshDb = new TestMeshDb;
    g_meshes[0].m_ownVertexArrays = 3;
    g_meshes[1].m_ownVertexArrays = 0;
    g_meshes[1].m_database = g_meshDb;
    g_meshes[2].m_ownVertexArrays = 1;
    g_device = new TestDevice;
    g_meshMap = new GmNameMap; g_meshMapNs = new GmNameMap; g_imageMap = new GmNameMap; g_imageMapNs = new GmNameMap; g_fontMap = new GmNameMap; g_fontMapNs = new GmNameMap; g_stringMap = new GmNameMap;
    for (GmNameMap* m : {g_meshMap, g_meshMapNs, g_imageMap, g_imageMapNs, g_fontMap, g_fontMapNs, g_stringMap}) m->Init(64);
    g_device->m_meshSpaces.Init(8);
    g_device->m_imageSpaces.Init(8);
    g_device->m_fontSpaces.Init(8);
    g_device->m_meshes.Add(g_meshMap);
    g_device->m_meshSpaces.Add("SP", g_meshMapNs);
    g_device->m_images.Add(g_imageMap);
    g_device->m_imageSpaces.Add("SP", g_imageMapNs);
    g_device->m_fonts.Add(g_fontMap);
    g_device->m_fontSpaces.Add("SP", g_fontMapNs);
    g_ctx.m_defaultGroup.Add(g_stringMap);
}

static AtNameTableOwner* make_owner() {
    const bool was = g_recording;
    g_recording = false;
    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_device = g_device;
    owner->m_loader = (AtAudioBankLoader*)&g_ctx;
    owner->m_firstSpaces.Init(8);
    owner->m_secondSpaces.Init(8);
    owner->m_cellTableSpaces.Init(8);
    g_recording = was;
    return owner;
}

// ---- the loads of other subsystems' formats, on both sides --------------------------------------------------------------------------------------

enum Kind { kNode, kModifier, kGraph, kGeometry, kEmitter, kLens, kString, kKinds };
struct Visit { int kind; void* self; std::string name; int mode; void* owner; std::string dir; };
static std::vector<Visit> g_visits[2];
template <int K> static void __fastcall visit(void* self, void*, void* owner, const char* name, int mode) {
    const bool was = g_recording;
    g_recording = false;
    const char* current = g_fs->m_path.Current();
    g_visits[g_side].push_back({K, self, name ? name : "(null)", mode, owner, current ? current : "(none)"});
    g_recording = was;
    g_log[g_side].add('L', K);
}
static void forget_visits(int s) {
    const bool was = g_recording;
    g_recording = false;
    g_visits[s].clear();
    g_recording = was;
}
template <class M> static uint32_t address_of(M m) { uint32_t a; std::memcpy(&a, &m, 4); return a; }
static void redirect_children() {
    redirect(0x0003CD60, (const void*)&visit<kNode>); redirect(address_of(&AtScene::Load), (const void*)&visit<kNode>);
    redirect(0x0003F390, (const void*)&visit<kModifier>); redirect(address_of(&AtModifierDatabase::Load), (const void*)&visit<kModifier>);
    redirect(0x0003ADC0, (const void*)&visit<kGraph>); redirect(address_of(&AtSpaceGraph::Load), (const void*)&visit<kGraph>);
    redirect(0x00040F90, (const void*)&visit<kGeometry>); redirect(address_of(&AtBoundingGeometryDatabase::Load), (const void*)&visit<kGeometry>);
    redirect(0x0003F760, (const void*)&visit<kEmitter>); redirect(address_of(&AtEmitterDatabase::Load), (const void*)&visit<kEmitter>);
    redirect(0x0003FAC0, (const void*)&visit<kLens>); redirect(address_of(&AtLensFlareDatabase::Load), (const void*)&visit<kLens>);
    redirect(0x0001FA70, (const void*)&visit<kString>); redirect(address_of(&AtTextTable::Load), (const void*)&visit<kString>);
}

// ---- comparing ---------------------------------------------------------------------------------------------------------------------------

struct SideWorld {
    AtNameTableOwner* owner = nullptr;
    std::vector<const AtCellGraphDatabase*> cellDbs;  // in the order loaded, to name what a portal points at
};
static SideWorld g_world[2];

static long long cookie(const void* p) { return p ? (long long)((const uint32_t*)p)[-1] : -1; }

static long long graph_id(int side, const void* p) {
    if (!p) return -1;
    for (size_t d = 0; d < g_world[side].cellDbs.size(); d++) {
        const AtCellGraphDatabase* db = g_world[side].cellDbs[d];
        for (unsigned i = 0; i < db->m_count && db->m_graphs; i++) if (p == &db->m_graphs[i]) return (long long)(d * 1000 + i);
    }
    return -2;
}
static long long cell_id(int side, const void* p) {
    if (!p) return -1;
    for (size_t d = 0; d < g_world[side].cellDbs.size(); d++) {
        const AtCellGraphDatabase* db = g_world[side].cellDbs[d];
        for (unsigned i = 0; i < db->m_count && db->m_graphs; i++) {
            const AtCellGraph& g = db->m_graphs[i];
            if (g.m_cellArray && p >= g.m_cellArray && p < g.m_cellArray + g.m_cellCount) return (long long)(d * 1000000 + i * 1000 + ((const AtCell*)p - g.m_cellArray));
        }
    }
    return -2;
}
static long long in_array(const void* p, const void* base, unsigned count, size_t size) {
    if (!p) return -1;
    if (!base || p < base || (const char*)p >= (const char*)base + count * size) return -2;
    return (long long)(((const char*)p - (const char*)base) / size);
}

template <class F> static void compare_map(const char* what, const GmNameMap& o, const GmNameMap& r, F value) {
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_entries == nullptr, r.m_entries == nullptr);
    if (!o.m_entries || !r.m_entries || o.m_size != r.m_size) return;
    for (unsigned i = 0; i < o.m_size; i++) {
        char a[9] = {}, b[9] = {};
        std::memcpy(a, o.m_entries[i].m_name, 8);
        std::memcpy(b, r.m_entries[i].m_name, 8);
        if (o.m_entries[i].m_value) eqs(what, a, b);
        eq(what, value(0, o.m_entries[i].m_value), value(1, r.m_entries[i].m_value));
    }
}

static void compare_graph(const AtCellGraph& o, const AtCellGraph& r) {
    eq("mesh", (long long)(uintptr_t)o.m_mesh, (long long)(uintptr_t)r.m_mesh);
    eq("portal count", o.m_portalCount, r.m_portalCount);
    eq("cell count", o.m_cellCount, r.m_cellCount);
    eq("portals null", o.m_portals == nullptr, r.m_portals == nullptr);
    eq("cells null", o.m_cellArray == nullptr, r.m_cellArray == nullptr);
    eq("exterior count", o.m_exteriorPortalCount, r.m_exteriorPortalCount);
    eq("exterior", in_array(o.m_exteriorPortals, o.m_portals, o.m_portalCount + 1, sizeof(AtCellPortal)), in_array(r.m_exteriorPortals, r.m_portals, r.m_portalCount + 1, sizeof(AtCellPortal)));
    eq("unknown44", o.m_unknown44, r.m_unknown44);
    if (o.m_portals && r.m_portals && o.m_portalCount == r.m_portalCount) {
        for (unsigned i = 0; i < o.m_portalCount && i < 4096; i++) {
            const AtCellPortal& a = o.m_portals[i];
            const AtCellPortal& b = r.m_portals[i];
            eq("portal opposite", cell_id(0, a.m_opposite), cell_id(1, b.m_opposite));
            eq("portal graph", graph_id(0, a.m_oppositeGraph), graph_id(1, b.m_oppositeGraph));
            eq("portal flags", a.m_flags, b.m_flags);
            for (int k = 0; k < 3; k++) { eqf("portal centre", a.m_center[k], b.m_center[k]); eqf("portal normal", a.m_normal[k], b.m_normal[k]); }
            eqf("portal distance", a.m_closedDistanceSquared, b.m_closedDistanceSquared);
            eq("portal vertices", (long long)(uintptr_t)a.m_vertices, (long long)(uintptr_t)b.m_vertices);
            eq("portal vertex count", a.m_vertexCount, b.m_vertexCount);
            eq("portal stride", a.m_stride, b.m_stride);
            eq("portal first", a.m_firstVertex, b.m_firstVertex);
            eq("portal array", a.m_vertexArray, b.m_vertexArray);
        }
    }
    if (o.m_cellArray && r.m_cellArray && o.m_cellCount == r.m_cellCount) {
        for (unsigned i = 0; i < o.m_cellCount && i < 4096; i++) {
            const AtCell& a = o.m_cellArray[i];
            const AtCell& b = r.m_cellArray[i];
            eq("cell graph", graph_id(0, a.m_graph), graph_id(1, b.m_graph));
            eq("cell portals", in_array(a.m_portals, o.m_portals, o.m_portalCount + 1, sizeof(AtCellPortal)), in_array(b.m_portals, r.m_portals, r.m_portalCount + 1, sizeof(AtCellPortal)));
            eq("cell portal count", a.m_portalCount, b.m_portalCount);
            eq("cell 0C", a.m_unknown0C, b.m_unknown0C);
            eq("cell 10", a.m_unknown10, b.m_unknown10);
            eq("cell 14", a.m_unknown14, b.m_unknown14);
        }
    }
    const AtCellPortal* ports[2] = {o.m_portals, r.m_portals};
    compare_map("portal names", o.m_portalNames, r.m_portalNames, [&](int side, void* v) { return in_array(v, ports[side], side ? r.m_portalCount : o.m_portalCount, sizeof(AtCellPortal)); });
    compare_map("cells", o.m_cells, r.m_cells, [&](int side, void* v) { return cell_id(side, v); });
}

static void compare_celldb(const AtCellGraphDatabase* o, const AtCellGraphDatabase* r) {
    eq("graph count", o->m_count, r->m_count);
    eq("graph cookie", cookie(o->m_graphs), cookie(r->m_graphs));
    compare_map("graphs", *o, *r, [&](int side, void* v) { return graph_id(side, v); });
    if (o->m_graphs && r->m_graphs && o->m_count == r->m_count)
        for (unsigned i = 0; i < o->m_count && i < 256; i++) compare_graph(o->m_graphs[i], r->m_graphs[i]);
}

static void compare_item(const AtDrawItem& a, const AtDrawItem& b) {
    eq("item kind", a.m_kind, b.m_kind);
    eq("item flags", a.m_flags, b.m_flags);
    eq("item 0C", a.m_unknown0C, b.m_unknown0C);
    eqf("item x", a.m_x, b.m_x);
    eqf("item y", a.m_y, b.m_y);
    eqf("item scale x", a.m_scaleX, b.m_scaleX);
    eqf("item scale y", a.m_scaleY, b.m_scaleY);
    eqf("item device x", a.m_deviceScaleX, b.m_deviceScaleX);
    eqf("item device y", a.m_deviceScaleY, b.m_deviceScaleY);
    eqf("item width", a.m_drawWidth, b.m_drawWidth);
    eqf("item height", a.m_drawHeight, b.m_drawHeight);
    eq("item owner", a.m_owner == g_world[0].owner ? 1 : (a.m_owner ? 2 : 0), b.m_owner == g_world[1].owner ? 1 : (b.m_owner ? 2 : 0));
    for (int k = 0; k < 4; k++) eqf("item clip", a.m_clip[k], b.m_clip[k]);
    eq("item colours", std::memcmp(&a.m_colors, &b.m_colors, 16), 0);
}

template <class D> static void compare_draws(const D* o, const D* r) {
    eq("draw count", o->m_count, r->m_count);
    eq("draw cookie", cookie(o->m_draws), cookie(r->m_draws));
    compare_map("draws", *o, *r, [&](int side, void* v) { return in_array(v, side ? (const void*)r->m_draws : (const void*)o->m_draws, side ? r->m_count : o->m_count, sizeof(*o->m_draws)); });
}
static void compare_list(const AtImageDrawDatabase* o, const AtImageDrawDatabase* r) {
    compare_draws(o, r);
    if (o->m_draws && r->m_draws && o->m_count == r->m_count)
        for (unsigned i = 0; i < o->m_count && i < 256; i++) {
            compare_item(o->m_draws[i], r->m_draws[i]);
            eq("image", (long long)(uintptr_t)o->m_draws[i].m_image, (long long)(uintptr_t)r->m_draws[i].m_image);
        }
}
static void compare_list(const AtTextDrawDatabase* o, const AtTextDrawDatabase* r) {
    compare_draws(o, r);
    if (o->m_draws && r->m_draws && o->m_count == r->m_count)
        for (unsigned i = 0; i < o->m_count && i < 256; i++) {
            const AtTextDraw& a = o->m_draws[i];
            const AtTextDraw& b = r->m_draws[i];
            compare_item(a, b);
            eq("font", (long long)(uintptr_t)a.m_font, (long long)(uintptr_t)b.m_font);
            eq("string", (long long)(uintptr_t)a.m_string, (long long)(uintptr_t)b.m_string);
            eqf("wrap", a.m_wrapWidth, b.m_wrapWidth);
            eqf("kerning", a.m_kerning, b.m_kerning);
        }
}

// Where in a loader's tree an object is: list, index, and the path of nested loaders.
static long long where(const AtSceneDatabaseLoader* l, const void* p, int depth = 0) {
    if (!l || !p || depth > 6) return -1;
    struct List { const void* base; unsigned count; size_t size; };
    const List lists[] = {
        {l->m_nodes, l->m_nodeCount, sizeof(AtScene)}, {l->m_modifiers, l->m_modifierCount, sizeof(AtModifierDatabase)}, {l->m_spaceGraphs, l->m_spaceGraphCount, sizeof(AtSpaceGraph)},
        {l->m_geometry, l->m_geometryCount, sizeof(AtBoundingGeometryDatabase)}, {l->m_emitters, l->m_emitterCount, sizeof(AtEmitterDatabase)}, {l->m_lensFlares, l->m_lensFlareCount, sizeof(AtLensFlareDatabase)},
        {l->m_strings, l->m_stringCount, sizeof(AtTextTable)}, {l->m_cellGraphs, l->m_cellGraphCount, sizeof(AtCellGraphDatabase)}, {l->m_imageDraws, l->m_imageDrawCount, sizeof(AtImageDrawDatabase)},
        {l->m_textDraws, l->m_textDrawCount, sizeof(AtTextDrawDatabase)},
    };
    for (int k = 0; k < 10; k++) {
        const long long i = in_array(p, lists[k].base, lists[k].count, lists[k].size);
        if (i >= 0) return k * 1000 + i;
    }
    if (p == &l->m_nodesByName) return 20000;
    if (p == &l->m_spaceGraphsByName) return 20001;
    for (unsigned i = 0; i < l->m_loaderCount && l->m_loaders; i++) {
        const long long w = where(&l->m_loaders[i], p, depth + 1);
        if (w >= 0) return 100000 * (i + 1) + w;
    }
    return -2;
}

static const AtSceneDatabaseLoader* g_tree[2];

static void compare_loader(const AtSceneDatabaseLoader* o, const AtSceneDatabaseLoader* r, int depth = 0) {
    if (depth > 6) return;
    eq("owner", o->m_owner == g_world[0].owner ? 1 : (o->m_owner ? 2 : 0), r->m_owner == g_world[1].owner ? 1 : (r->m_owner ? 2 : 0));
    const unsigned short* oc = &o->m_nodeCount;
    const unsigned short* rc = &r->m_nodeCount;
    for (int k = 0; k < 11; k++) eq("count", oc[k], rc[k]);
    const void* const* op = (const void* const*)&o->m_nodes;
    const void* const* rp = (const void* const*)&r->m_nodes;
    for (int k = 0; k < 11; k++) { eq("list made", op[k] == nullptr, rp[k] == nullptr); eq("list cookie", cookie(op[k]), cookie(rp[k])); }
    eq("loader names", o->m_loaderNames == nullptr, r->m_loaderNames == nullptr);
    compare_map("nodes by name", o->m_nodesByName, r->m_nodesByName, [&](int side, void* v) { return where(g_tree[side], v); });
    compare_map("graphs by name", o->m_spaceGraphsByName, r->m_spaceGraphsByName, [&](int side, void* v) { return where(g_tree[side], v); });
    if (o->m_cellGraphs && r->m_cellGraphs && o->m_cellGraphCount == r->m_cellGraphCount)
        for (unsigned i = 0; i < o->m_cellGraphCount; i++) compare_celldb(&o->m_cellGraphs[i], &r->m_cellGraphs[i]);
    if (o->m_imageDraws && r->m_imageDraws && o->m_imageDrawCount == r->m_imageDrawCount)
        for (unsigned i = 0; i < o->m_imageDrawCount; i++) compare_list(&o->m_imageDraws[i], &r->m_imageDraws[i]);
    if (o->m_textDraws && r->m_textDraws && o->m_textDrawCount == r->m_textDrawCount)
        for (unsigned i = 0; i < o->m_textDrawCount; i++) compare_list(&o->m_textDraws[i], &r->m_textDraws[i]);
    if (o->m_loaders && r->m_loaders && o->m_loaderCount == r->m_loaderCount)
        for (unsigned i = 0; i < o->m_loaderCount; i++) compare_loader(&o->m_loaders[i], &r->m_loaders[i], depth + 1);
}

// The lists of names the original keeps at fixed addresses, against the recovered statics.
static char*** const kNamesRecovered[] = {&AtSceneDatabaseLoader::s_nodeNames, &AtSceneDatabaseLoader::s_modifierNames, &AtSceneDatabaseLoader::s_emitterNames, &AtSceneDatabaseLoader::s_lensFlareNames,
    &AtSceneDatabaseLoader::s_cellGraphNames, &AtSceneDatabaseLoader::s_spaceGraphNames, &AtSceneDatabaseLoader::s_geometryNames, &AtSceneDatabaseLoader::s_stringNames,
    &AtSceneDatabaseLoader::s_imageDrawNames, &AtSceneDatabaseLoader::s_textDrawNames};
static const uint32_t kNamesOriginal[] = {0x002C63A0, 0x002C63A4, 0x002C63A8, 0x002C63AC, 0x002C63B0, 0x002C63B4, 0x002C63B8, 0x002C6398, 0x002C63BC, 0x002C63C0};
static void compare_statics() {
    for (int k = 0; k < 10; k++) eq("names static", *(void**)(uintptr_t)kNamesOriginal[k] == nullptr, *kNamesRecovered[k] == nullptr);
    eq("portal index", *(uint32_t*)(uintptr_t)0x002C7E28, AtCellGraph::s_portalIndex);
    eq("locked array", *(uint32_t*)(uintptr_t)0x002C7EAC, AtCellGraph::s_lockedArray);
    eq("lock data", *(uint32_t*)(uintptr_t)0x002C7E30, (uint32_t)(uintptr_t)AtCellGraph::s_lock.m_data);
    eq("lock stride", *(uint32_t*)(uintptr_t)0x002C7E34, AtCellGraph::s_lock.m_stride);
    eq("portal names", *(void**)(uintptr_t)0x002C7EA8 == nullptr, AtCellGraph::s_cellNames == nullptr);
}

static void compare_visits() {
    eq("visits", (long long)g_visits[0].size(), (long long)g_visits[1].size());
    for (size_t i = 0; i < g_visits[0].size() && i < g_visits[1].size(); i++) {
        const Visit& a = g_visits[0][i];
        const Visit& b = g_visits[1][i];
        eq("visit kind", a.kind, b.kind);
        eqs("visit name", a.name.c_str(), b.name.c_str());
        eq("visit mode", a.mode, b.mode);
        eqs("visit directory", a.dir.c_str(), b.dir.c_str());
        const void* expected[2] = {a.kind == kString ? (void*)&g_ctx : (void*)g_world[0].owner, b.kind == kString ? (void*)&g_ctx : (void*)g_world[1].owner};
        eq("visit owner", a.owner == expected[0], b.owner == expected[1]);
        eq("visit object", where(g_tree[0], a.self), where(g_tree[1], b.self));
    }
}

// What the owner's tables now find, by every name of the case, in and out of the name spaces it uses.
static void compare_owner(const std::set<std::string>& names) {
    AtNameTableOwner* o = g_world[0].owner;
    AtNameTableOwner* r = g_world[1].owner;
    auto id = [&](int side, void* v) -> long long {
        const long long w = where(g_tree[side], v);
        if (w != -2) return w;
        return 1000000 + graph_id(side, v);
    };
    for (const std::string& s : names) {
        for (const char* ns : {(const char*)nullptr, "SP", "Ns", s.c_str()}) {
            eq("owner first", id(0, o->FindInFirst(s.c_str(), ns)), id(1, r->FindInFirst(s.c_str(), ns)));
            void* a = ns ? o->m_secondSpaces.Find(ns, s.c_str()) : o->m_second.Find(s.c_str());
            void* b = ns ? r->m_secondSpaces.Find(ns, s.c_str()) : r->m_second.Find(s.c_str());
            eq("owner second", id(0, a), id(1, b));
            a = ns ? o->m_cellTableSpaces.Find(ns, s.c_str()) : o->m_cellTables.Find(s.c_str());
            b = ns ? r->m_cellTableSpaces.Find(ns, s.c_str()) : r->m_cellTables.Find(s.c_str());
            eq("owner cell graphs", id(0, a), id(1, b));
        }
    }
    eq("owner space graphs", where(g_tree[0], o->m_spaceGraphs.m_first), where(g_tree[1], r->m_spaceGraphs.m_first));
}

// ---- running both sides ---------------------------------------------------------------------------------------------------------------------

static void reset_side_state() {
    const bool was = g_recording;
    g_recording = false;
    for (int s = 0; s < 2; s++) { g_visits[s].clear(); g_world[s] = SideWorld(); g_tree[s] = nullptr; }
    g_recording = was;
}

template <class F> static bool side(int s, F f) {
    g_reports = 0;
    bool crashed;
    {
        On on(s);
        crashed = guarded(f);
    }
    _fpreset();
    return crashed;
}

static void* object(size_t size, int fill) {
    void* p = std::malloc(size + 64);
    std::memset(p, fill, size + 64);
    return p;
}

enum Target { kEntryCells, kEntryImages, kEntryTexts, kEntryLoader };
struct TargetInfo { uint32_t ctor, load, del; size_t size; };
static const TargetInfo kTargets[] = {
    {0x0003FC40, 0x0003FE00, 0x0002EAC0, 0x1C}, {0x0003B0A0, 0x0003B320, 0x0002E2D0, 0x1C}, {0x0003B430, 0x0003B6F0, 0x0002E3F0, 0x1C}, {0x0002EE30, 0x0002F7F0, 0x0002F790, 0x78},
};

static void* construct(int s, Target e, int fill) {
    void* p = object(kTargets[e].size, fill);
    if (s == 0) { at<Fn0>(kTargets[e].ctor)(p, nullptr); return p; }
    switch (e) {
    case kEntryCells: return new (p) AtCellGraphDatabase;
    case kEntryImages: return new (p) AtImageDrawDatabase;
    case kEntryTexts: return new (p) AtTextDrawDatabase;
    default: return new (p) AtSceneDatabaseLoader;
    }
}
static void load(int s, Target e, void* p, const char* name, int mode) {
    if (s == 0) { at<FnLoad>(kTargets[e].load)(p, nullptr, g_world[0].owner, name, mode); return; }
    switch (e) {
    case kEntryCells: ((AtCellGraphDatabase*)p)->Load(g_world[1].owner, name, mode); break;
    case kEntryImages: ((AtImageDrawDatabase*)p)->Load(g_world[1].owner, name, mode); break;
    case kEntryTexts: ((AtTextDrawDatabase*)p)->Load(g_world[1].owner, name, mode); break;
    default: ((AtSceneDatabaseLoader*)p)->Load(g_world[1].owner, name, mode); break;
    }
}
static void destroy(int s, Target e, void* p) {
    if (s == 0) { at<FnDel>(kTargets[e].del)(p, nullptr, 0); std::free(p); return; }
    switch (e) {
    case kEntryCells: ((AtCellGraphDatabase*)p)->~AtCellGraphDatabase(); break;
    case kEntryImages: ((AtImageDrawDatabase*)p)->~AtImageDrawDatabase(); break;
    case kEntryTexts: ((AtTextDrawDatabase*)p)->~AtTextDrawDatabase(); break;
    default: ((AtSceneDatabaseLoader*)p)->~AtSceneDatabaseLoader(); break;
    }
    std::free(p);
}
static void note_cells(int s, Target e, void* p) {
    if (e == kEntryCells) g_world[s].cellDbs.push_back((const AtCellGraphDatabase*)p);
    if (e == kEntryLoader) {
        g_tree[s] = (const AtSceneDatabaseLoader*)p;
        std::vector<const AtSceneDatabaseLoader*> todo = {(const AtSceneDatabaseLoader*)p};
        while (!todo.empty()) {
            const AtSceneDatabaseLoader* l = todo.back();
            todo.pop_back();
            for (unsigned i = 0; l->m_cellGraphs && i < l->m_cellGraphCount; i++) g_world[s].cellDbs.push_back(&l->m_cellGraphs[i]);
            for (unsigned i = 0; l->m_loaders && i < l->m_loaderCount && todo.size() < 64; i++) todo.push_back(&l->m_loaders[i]);
        }
    }
}

static void compare_entry(Target e, void* o, void* r) {
    switch (e) {
    case kEntryCells: compare_celldb((AtCellGraphDatabase*)o, (AtCellGraphDatabase*)r); break;
    case kEntryImages: compare_list((AtImageDrawDatabase*)o, (AtImageDrawDatabase*)r); break;
    case kEntryTexts: compare_list((AtTextDrawDatabase*)o, (AtTextDrawDatabase*)r); break;
    default: compare_loader((AtSceneDatabaseLoader*)o, (AtSceneDatabaseLoader*)r); break;
    }
}

struct Options {
    int times = 1;          // loads of the same object
    int mode = 0x5A;
    int fill = 0xCD;
    bool destroy = true;
    int open_status = 0;
    int heap_null = -1;     // index into kHeapAddress, made null on both sides
    int refuse = -1;        // index into kHeapToken, refused after skip pushes
    int skip = 0;
    std::vector<std::string> names;  // further loads of the same object, by these names, after the first
};

// One case: construct, load, compare everything, destroy. A crash on either side ends the comparison at the logs.
static void run(const char* label, Target e, const std::string& name, const Options& opt = Options()) {
    g_context = std::string(label) + " " + name;
    reset_side_state();
    std::set<std::string> names;
    for (auto& kv : g_files) gather(kv.second, names);
    provide(names);
    g_fs->open_status = opt.open_status;
    void* objs[2] = {};
    bool crashed[2] = {};
    for (int s = 0; s < 2; s++) {
        g_world[s].owner = make_owner();
        g_fill = opt.fill;
        void* saved = nullptr;
        if (opt.heap_null >= 0) {
            saved = *(void**)kHeapAddress[opt.heap_null];
            *(void**)kHeapAddress[opt.heap_null] = nullptr;
            *kHeapRecovered[opt.heap_null] = nullptr;
        }
        g_refuse_heap = opt.refuse >= 0 ? kHeapToken[opt.refuse] : nullptr;
        g_refuse_skip[s] = opt.skip;
        g_fs->m_path.m_top = g_path_top;
        crashed[s] = side(s, [&] {
            objs[s] = construct(s, e, opt.fill);
            // Only the last load's visits are compared: an earlier load's objects are freed, and where the heap puts the next is the harness's.
            load(s, e, objs[s], name.c_str(), opt.mode);
            for (int t = 1; t < opt.times; t++) { forget_visits(s); load(s, e, objs[s], name.c_str(), opt.mode); }
            for (const std::string& n : opt.names) { forget_visits(s); load(s, e, objs[s], n.c_str(), opt.mode); }
        });
        g_refuse_heap = nullptr;
        g_fail_alloc = false;
        g_refusing = 0;
        if (opt.heap_null >= 0) set_heaps();
        (void)saved;
    }
    g_fs->open_status = 0;
    g_fill = 0xCD;
    eq("crashed", crashed[0], crashed[1]);
    compare_logs("load");
    if (crashed[0] || crashed[1]) return;
    for (int s = 0; s < 2; s++) note_cells(s, e, objs[s]);
    compare_entry(e, objs[0], objs[1]);
    compare_visits();
    compare_statics();
    compare_owner(names);
    if (!opt.destroy) return;
    for (int s = 0; s < 2; s++) crashed[s] = side(s, [&] { destroy(s, e, objs[s]); });
    eq("destroy crashed", crashed[0], crashed[1]);
    compare_logs("destroy");
}

// ---- written cases ------------------------------------------------------------------------------------------------------------------------

static const char kCells[] =
    "NameSpace \"SP\"\n"
    "CellGraph[2]\n{\n"
    " CellGraph \"GA\"\n {\n"
    "  Mesh \"MeshA\" \"\"\n"
    "  Portal[5]\n  {\n"
    "   Portal \"P1\" { OppositeCell \"C2\" Vertices 0 0 4 ClosedByViewerDist 7500.0 }\n"
    "   Portal \"P2\" { OppositeCell \"C1\" Vertices 1 4 3 Closed }\n"
    "   Portal \"P3\" { OppositeCellDifferentGraph \"GA\" \"SP\" \"C3\" Vertices 2 9 5 }\n"
    "   Portal \"P4\" { Vertices 0 100 31 Closed ClosedByViewerDist 0.5 }\n"
    "   Portal \"P5\" { OppositeCellDifferentGraph \"\" \"\" \"C3\" Vertices 1 65000 6 }\n"
    "  }\n"
    "  Cell[3]\n  {\n"
    "   Cell \"C1\" { Portals 0 1 }\n"
    "   Cell \"C2\" { Portals 1 2 Portals 3 1 }\n"
    "   Cell \"C3\" { }\n"
    "  }\n"
    "  ExteriorPortals 3 2\n"
    " }\n"
    " CellGraph \"GB\"\n {\n"
    "  Mesh \"MeshB\" \"SP\"\n"
    "  Portal[2]\n  {\n"
    "   Portal \"Q1\" { OppositeCellDifferentGraph \"GA\" \"SP\" \"C1\" Vertices 0 7 8 }\n"
    "   Portal \"Q2\" { OppositeCell \"D1\" Vertices 4 3 4 }\n"
    "  }\n"
    "  Cell[1]\n  {\n"
    "   Cell \"D1\" { Portals 0 2 }\n"
    "  }\n"
    " }\n"
    "}\n";

static std::string cell_graph(const char* body) { return std::string("CellGraph[1]\n{\n CellGraph \"G\"\n {\n") + body + " }\n}\n"; }

static void run_written_cells() {
    g_files.clear();
    g_scale[0] = 1.5f; g_scale[1] = 0.75f;
    g_files["W\\CELLS.ASC"] = kCells;
    run("cells", kEntryCells, "W\\CELLS");
    { Options o; o.times = 2; run("cells twice", kEntryCells, "W\\CELLS", o); }
    { Options o; o.fill = 0; o.mode = 3; run("cells zero fill", kEntryCells, "W\\CELLS", o); }
    // An even fill: a portal's first flag clear in what the heap held, so that it is not reset when read.
    { Options o; o.fill = 0xCC; run("cells even fill", kEntryCells, "W\\CELLS", o); }    // A graph in another file, found through the owner's table.
    g_files["W\\OTHER.ASC"] = cell_graph("  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"R\" { OppositeCellDifferentGraph \"GA\" \"SP\" \"C3\" Vertices 0 1 3 }\n  }\n  Cell[1]\n  {\n   Cell \"E\" { Portals 0 1 }\n  }\n");
    { Options o; o.names = {"W\\OTHER"}; run("cells then other", kEntryCells, "W\\CELLS", o); }
    // Each word, and each error, alone.
    const char* bodies[] = {
        "  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Cell[2]\n  {\n   Cell \"A\" { }\n   Cell \"B\" { }\n  }\n",
        "  Mesh \"XNone\" \"\"\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 3 Bogus }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { Portals 0 1 }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 2 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { Portals 0 1 }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 32 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { Portals 0 1 }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 65539 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { Portals 0 1 }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[2]\n  {\n   Portal \"P\" { Vertices 0 0 4 }\n   Portal \"Q\" { Vertices 0 4 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { Portals 1 2 }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[2]\n  {\n   Portal \"P\" { Vertices 0 0 4 }\n   Portal \"Q\" { Vertices 0 4 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { Portals 0 2 Bogus }\n  }\n  ExteriorPortals 1 2\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n  ExteriorPortals 0 1\n  Bogus\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[0]\n  {\n  }\n  Cell[0]\n  {\n  }\n",
        "  Portal[1]\n  {\n   Portal \"P\" { }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Bogus[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { OppositeCell \"XNo\" Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { OppositeCellDifferentGraph \"XNo\" \"\" \"A\" Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { OppositeCellDifferentGraph \"G\" \"XNs\" \"A\" Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Bogus \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Bogus \"P\" { Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n  ExteriorPortals 1 1\n",
        // The second array of a mesh with one reports, and goes on.
        "  Mesh \"MeshC\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 1 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        // A mesh with none of its own: the database's.
        "  Mesh \"MeshD\" \"\"\n  Portal[2]\n  {\n   Portal \"P\" { Vertices 2 0 4 }\n   Portal \"Q\" { Vertices 2 9 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        // A graph named with an empty name space: the plain table, where this file's graph is.
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { OppositeCellDifferentGraph \"G\" \"\" \"A\" Vertices 0 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        // A mesh in a name space.
        "  Mesh \"MeshA\" \"SP\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 1 0 4 }\n  }\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n",
        // Unterminated.
        "  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { Vertices 0 0 4\n",
    };
    // MeshA and MeshB have three arrays, MeshC one, MeshD none, outside name spaces; in SP each is another.
    int n = 0;
    for (const char* body : bodies) {
        g_files["W\\ONE.ASC"] = cell_graph(body);
        char label[32];
        sprintf_s(label, "cells written %d", n++);
        run(label, kEntryCells, "W\\ONE");
    }
    // A graph can name only graphs before it in its own file: a later one is not yet in the map when its portals are resolved, at the end of its own body.
    g_files["W\\AHEAD.ASC"] = "NameSpace \"SP\"\nCellGraph[2]\n{\n CellGraph \"F1\"\n {\n  Mesh \"MeshA\" \"\"\n  Portal[1]\n  {\n   Portal \"P\" { OppositeCellDifferentGraph \"F2\" \"SP\" \"A\" Vertices 0 0 4 }\n  }\n"
                               "  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n }\n CellGraph \"F2\"\n {\n  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n }\n}\n";
    run("cells ahead", kEntryCells, "W\\AHEAD");
    g_files["W\\NS.ASC"] = "NameSpace \"Ns\"\n" + cell_graph("  Cell[1]\n  {\n   Cell \"A\" { }\n  }\n");
    run("cells name space", kEntryCells, "W\\NS");
    g_files["W\\NOCOUNT.ASC"] = "CellGraph[0]\n{\n}\n";
    run("cells none", kEntryCells, "W\\NOCOUNT");
    g_files["W\\EMPTY.ASC"] = "";
    run("cells empty", kEntryCells, "W\\EMPTY");
    { Options o; o.open_status = 2; run("cells no file", kEntryCells, "W\\CELLS", o); }
}

static const char kImages[] =
    "NameSpace \"SP\"\n"
    "ImageDraw[3]\n{\n"
    " ImageDraw \"I1\" { Image \"Pic\" \"\" Position 10.5 -20.25 Scale 2.0 3.0 CenterHorizontally ClipRectangle 1.0 2.0 3.0 4.0 NormalizedCoordinates DrawArea 5.0 6.0 }\n"
    " ImageDraw \"I2\" { Image \"Pic\" \"SP\" ColorValueRectangle 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 CenterVertically Scale 0.1 0.3 Scale 7.0 0.9 }\n"
    " ImageDraw \"I3\" { Image \"XNone\" \"\" Bogus Kerning 1.0 }\n"
    "}\n"
    "Bogus\n";

// A second list: it leaks the first's array, then the map's Init calls the list's own Clear, which frees the new one. See the worklog.
static const char kTwoImageLists[] =
    "ImageDraw[1]\n{\n ImageDraw \"I1\" { Position 1.0 2.0 }\n}\n"
    "ImageDraw[2]\n{\n ImageDraw \"I4\" { Position 1.0 2.0 }\n ImageDraw \"I5\" { }\n}\n";

static const char kTexts[] =
    "TextDraw[3]\n{\n"
    " TextDraw \"T1\" { Font \"Big\" \"SP\" String \"Hello\" Position 10.5 -20.25 WrapWidth 300.0 Kerning 1.5 CenterHorizontally CenterVertically }\n"
    " TextDraw \"T2\" { Font \"Big\" \"\" String \"XNone\" Scale 2.0 3.0 ClipRectangle 1.0 2.0 3.0 4.0 DrawArea 5.0 6.0 NormalizedCoordinates ColorValueRectangle 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 }\n"
    " TextDraw \"T3\" { Image \"Pic\" \"\" Bogus }\n"
    "}\n";

static void run_written_draws() {
    g_files.clear();
    g_scale[0] = 0.8125f; g_scale[1] = 1.3333334f;
    g_files["W\\IMG.ASI"] = kImages;
    g_files["W\\TXT.AST"] = kTexts;
    run("images", kEntryImages, "W\\IMG");
    { Options o; o.times = 2; run("images twice", kEntryImages, "W\\IMG", o); }
    run("texts", kEntryTexts, "W\\TXT");
    { Options o; o.times = 2; run("texts twice", kEntryTexts, "W\\TXT", o); }
    g_files["W\\NSI.ASI"] = "NameSpace \"Ns\"\nImageDraw[1]\n{\n ImageDraw \"A\" { }\n}\n";
    g_files["W\\NST.AST"] = "NameSpace \"Ns\"\nTextDraw[1]\n{\n TextDraw \"A\" { }\n}\n";
    run("images name space", kEntryImages, "W\\NSI");
    run("texts name space", kEntryTexts, "W\\NST");
    g_files["W\\ZERO.ASI"] = "ImageDraw[0]\n{\n}\n";
    g_files["W\\ZERO.AST"] = "TextDraw[0]\n{\n}\nTextDraw[0]\n{\n}\n";
    { Options o; o.times = 2; run("images none", kEntryImages, "W\\ZERO", o); }
    { Options o; o.times = 2; run("texts none", kEntryTexts, "W\\ZERO", o); }
    g_files["W\\TWO.ASI"] = kTwoImageLists;
    { Options o; o.times = 2; run("images two lists", kEntryImages, "W\\TWO", o); }
    g_files["W\\CUT.ASI"] = "ImageDraw[1]\n{\n ImageDraw \"A\" { Position 1.0\n";
    run("images cut", kEntryImages, "W\\CUT");
    { Options o; o.open_status = 2; run("images no file", kEntryImages, "W\\IMG", o); }
    { Options o; o.open_status = 2; run("texts no file", kEntryTexts, "W\\TXT", o); }
}

static void run_written_loader() {
    g_files.clear();
    g_scale[0] = 2.0f; g_scale[1] = 0.5f;
    g_files["W\\CELLS.ASC"] = kCells;
    g_files["W\\IMG.ASI"] = kImages;
    g_files["W\\TXT.AST"] = kTexts;
    g_files["W\\OTHER.ASC"] = cell_graph("  Mesh \"MeshD\" \"\"\n  Portal[1]\n  {\n   Portal \"R\" { OppositeCellDifferentGraph \"GA\" \"SP\" \"C3\" Vertices 0 1 3 }\n  }\n  Cell[1]\n  {\n   Cell \"E\" { Portals 0 1 }\n  }\n");
    g_files["W\\TXT2.AST"] = "TextDraw[1]\n{\n TextDraw \"U1\" { Font \"Big\" \"\" Kerning 2.5 }\n}\n";
    g_files["W\\ALL.ASD"] =
        "TextDrawDatabase[2]\n{\n \"TXT\"\n \"TXT2\"\n}\n"
        "NodeDatabase[3]\n{\n \"Alpha\"\n \"sub\\\\Beta.x\"\n \"Gamma\"\n}\n"
        "ModifierDatabase[2]\n{\n \"Mod1\"\n \"Mod2\"\n}\n"
        "SpaceGraph[2]\n{\n \"Space1\"\n \"d:sp2\"\n}\n"
        "BoundingGeometryDatabase[1]\n{\n \"Geo\"\n}\n"
        "EmitterDatabase[4]\n{\n \"E1\"\n \"E2\"\n \"E3\"\n \"E4\"\n}\n"
        "LensFlareDatabase[1]\n{\n \"Lens\"\n}\n"
        "CellGraphDatabase[2]\n{\n \"CELLS\"\n \"OTHER\"\n}\n"
        "StringDatabase[2]\n{\n \"Str1\"\n \"Str2\"\n}\n"
        "ImageDrawDatabase[1]\n{\n \"IMG\"\n}\n";
    run("loader all", kEntryLoader, "W\\ALL");
    { Options o; o.times = 2; o.mode = 0x77; run("loader twice", kEntryLoader, "W\\ALL", o); }
    { Options o; o.fill = 0; run("loader zero fill", kEntryLoader, "W\\ALL", o); }
    // Nested lists, each in its own directory, one twice.
    g_files["W\\TOP.ASD"] = "NodeDatabase[1]\n{\n \"N\"\n}\nDatabaseLoader[3]\n{\n \"SUB\\\\MID\"\n \"SUB\\\\MID\"\n \"LEAF\"\n}\nSpaceGraph[1]\n{\n \"G\"\n}\n";
    g_files["W\\SUB\\MID.ASD"] = "DatabaseLoader[1]\n{\n \"DEEP\\\\LEAF\"\n}\nModifierDatabase[1]\n{\n \"M\"\n}\nSpaceGraph[1]\n{\n \"G2\"\n}\n";
    g_files["W\\SUB\\DEEP\\LEAF.ASD"] = "EmitterDatabase[1]\n{\n \"E\"\n}\nCellGraphDatabase[1]\n{\n \"..\\\\..\\\\CELLS\"\n}\n";
    g_files["W\\LEAF.ASD"] = "NodeDatabase[1]\n{\n \"Leaf\"\n}\n";
    run("loader nested", kEntryLoader, "W\\TOP");
    { Options o; o.times = 2; run("loader nested twice", kEntryLoader, "W\\TOP", o); }
    // Each word alone, and each error.
    const char* texts[] = {
        "NodeDatabase[1]\n{\n \"A\"\n}\nNodeDatabase[2]\n{\n \"B\"\n \"C\"\n}\n",
        "ModifierDatabase[1]\n{\n \"A\"\n}\nModifierDatabase[1]\n{\n \"B\"\n}\n",
        "SpaceGraph[1]\n{\n \"A\"\n}\nSpaceGraph[1]\n{\n \"B\"\n}\n",
        "BoundingGeometryDatabase[1]\n{\n \"A\"\n}\nBoundingGeometryDatabase[1]\n{\n \"B\"\n}\n",
        "EmitterDatabase[1]\n{\n \"A\"\n}\nEmitterDatabase[1]\n{\n \"B\"\n}\n",
        "LensFlareDatabase[1]\n{\n \"A\"\n}\nLensFlareDatabase[1]\n{\n \"B\"\n}\n",
        "StringDatabase[1]\n{\n \"A\"\n}\nStringDatabase[1]\n{\n \"B\"\n}\n",
        "CellGraphDatabase[1]\n{\n \"CELLS\"\n}\nCellGraphDatabase[1]\n{\n \"CELLS\"\n}\n",
        "ImageDrawDatabase[1]\n{\n \"IMG\"\n}\nImageDrawDatabase[1]\n{\n \"IMG\"\n}\n",
        "TextDrawDatabase[1]\n{\n \"TXT\"\n}\nTextDrawDatabase[1]\n{\n \"TXT\"\n}\n",
        "DatabaseLoader[1]\n{\n \"LEAF\"\n}\nDatabaseLoader[1]\n{\n \"LEAF\"\n}\n",
        "Bogus[1]\n{\n \"A\"\n}\nNodeDatabase[1]\n{\n \"B\"\n}\n",
        "NodeDatabase[0]\n{\n}\nSpaceGraph[0]\n{\n}\nDatabaseLoader[0]\n{\n}\n",
        "NodeDatabase[2]\n{\n \"A\"\n}\n",
        "",
        "SpaceGraph[3]\n{\n \"VeryLongName\"\n \"a/b/c.d\"\n \"x\\\\\"\n}\nNodeDatabase[2]\n{\n \"path\\\\to\\\\Node.ASN\"\n \"\"\n}\n",
    };
    int n = 0;
    for (const char* text : texts) {
        g_files["W\\ONE.ASD"] = text;
        char label[32];
        sprintf_s(label, "loader written %d", n++);
        run(label, kEntryLoader, "W\\ONE");
    }
    { Options o; o.open_status = 2; run("loader no file", kEntryLoader, "W\\ALL", o); }
}

// Memory refused, and heaps that are not there, at each place they are taken.
static void run_shortages() {
    g_files.clear();
    g_scale[0] = 1.0f; g_scale[1] = 1.0f;
    g_files["W\\CELLS.ASC"] = kCells;
    g_files["W\\IMG.ASI"] = kImages;
    g_files["W\\TXT.AST"] = kTexts;
    g_files["W\\ALL.ASD"] = "CellGraphDatabase[1]\n{\n \"CELLS\"\n}\nImageDrawDatabase[1]\n{\n \"IMG\"\n}\nTextDrawDatabase[1]\n{\n \"TXT\"\n}\nNodeDatabase[1]\n{\n \"N\"\n}\nSpaceGraph[1]\n{\n \"G\"\n}\n"
                            "DatabaseLoader[1]\n{\n \"LEAF\"\n}\nModifierDatabase[1]\n{\n \"M\"\n}\nEmitterDatabase[1]\n{\n \"E\"\n}\nLensFlareDatabase[1]\n{\n \"L\"\n}\n"
                            "BoundingGeometryDatabase[1]\n{\n \"B\"\n}\nStringDatabase[1]\n{\n \"S\"\n}\n";
    g_files["W\\LEAF.ASD"] = "NodeDatabase[1]\n{\n \"Leaf\"\n}\n";
    for (int h = 0; h < 6; h++) {
        for (int skip = 0; skip < 14; skip++) {
            Options o;
            o.refuse = h;
            o.skip = skip;
            char label[48];
            sprintf_s(label, "refused heap %d after %d", h, skip);
            run(label, kEntryLoader, "W\\ALL", o);
        }
        Options o;
        o.heap_null = h;
        char label[48];
        sprintf_s(label, "no heap %d", h);
        run(label, kEntryLoader, "W\\ALL", o);
    }
    for (int skip = 0; skip < 4; skip++) {
        Options o;
        o.refuse = 4;
        o.skip = skip;
        run("refused cells", kEntryCells, "W\\CELLS", o);
        o.refuse = 1;
        run("refused images", kEntryImages, "W\\IMG", o);
        o.refuse = 2;
        run("refused texts", kEntryTexts, "W\\TXT", o);
        o.refuse = 3;
        run("refused cell lists", kEntryCells, "W\\CELLS", o);
    }
    // The token file's heap is pushed many times before a graph's names: by the stream, the tokenizer, each name.
    for (int skip = 0; skip < 40; skip++) {
        Options o;
        o.refuse = 5;
        o.skip = skip;
        run("refused names", kEntryCells, "W\\CELLS", o);
    }
}

// What is not reached by loading a file: the accessors, LoadDatabases keeping its names, and a portal read again.
static void run_direct() {
    g_files.clear();
    g_scale[0] = 0.625f; g_scale[1] = 3.0f;
    g_files["W\\CELLS.ASC"] = kCells;
    g_files["W\\IMG.ASI"] = kImages;
    g_files["W\\TXT.AST"] = kTexts;
    g_files["W\\ALL.ASD"] = "NodeDatabase[3]\n{\n \"A\"\n \"B\"\n \"C\"\n}\nSpaceGraph[2]\n{\n \"G1\"\n \"G2\"\n}\nCellGraphDatabase[1]\n{\n \"CELLS\"\n}\nModifierDatabase[1]\n{\n \"M\"\n}\n"
                            "DatabaseLoader[2]\n{\n \"LEAF\"\n \"LEAF\"\n}\nEmitterDatabase[1]\n{\n \"E\"\n}\nStringDatabase[1]\n{\n \"S\"\n}\nTextDrawDatabase[1]\n{\n \"TXT\"\n}\n";
    g_files["W\\LEAF.ASD"] = "NodeDatabase[1]\n{\n \"Leaf\"\n}\n";
    Options keep;
    keep.destroy = false;
    run("direct", kEntryLoader, "W\\ALL", keep);
    void* o = (void*)g_tree[0];
    void* r = (void*)g_tree[1];
    if (!o || !r) return;
    g_context = "accessors";
    for (unsigned i = 0; i < 4; i++) {
        void* a[3];
        void* b[3];
        {
            On on(0);
            a[0] = at<FnAt>(0x0002ED20)(o, nullptr, i);
            a[1] = at<FnAt>(0x0002ED40)(o, nullptr, i);
            a[2] = at<FnAt>(0x0002ED60)(o, nullptr, i);
        }
        {
            On on(1);
            AtSceneDatabaseLoader* l = (AtSceneDatabaseLoader*)r;
            b[0] = l->NodeDatabaseAt(i);
            b[1] = l->SpaceGraphAt(i);
            b[2] = l->LoaderAt(i);
        }
        const AtSceneDatabaseLoader* lo = (const AtSceneDatabaseLoader*)o;
        const AtSceneDatabaseLoader* lr = (const AtSceneDatabaseLoader*)r;
        eq("node at", (char*)a[0] - (char*)lo->m_nodes, (char*)b[0] - (char*)lr->m_nodes);
        eq("graph at", (char*)a[1] - (char*)lo->m_spaceGraphs, (char*)b[1] - (char*)lr->m_spaceGraphs);
        eq("loader at", (char*)a[2] - (char*)lo->m_loaders, (char*)b[2] - (char*)lr->m_loaders);
    }
    // LoadDatabases with names given and kept, then freed. The nested loaders' names are gone, so their count is set to none first.
    for (int freeNames = 0; freeNames < 2; freeNames++) {
        g_context = freeNames ? "databases, names freed" : "databases, names kept";
        for (int s = 0; s < 2; s++) {
            const bool was = g_recording;
            g_recording = false;
            AtSceneDatabaseLoader* l = (AtSceneDatabaseLoader*)(s ? r : o);
            l->m_loaderCount = 0;
            const unsigned short listCount[10] = {l->m_nodeCount, l->m_modifierCount, l->m_emitterCount, l->m_lensFlareCount, l->m_cellGraphCount, l->m_spaceGraphCount, l->m_geometryCount, l->m_stringCount, l->m_imageDrawCount, l->m_textDrawCount};
            for (int k = 0; k < 10; k++) {
                char** names = (char**)std::calloc(listCount[k] + 1, sizeof(char*));
                for (unsigned i = 0; i < listCount[k]; i++) {
                    char text[16];
                    sprintf_s(text, k == 4 ? "CELLS" : (k == 9 ? "TXT" : "K%dN%u"), k, i);
                    names[i] = (char*)std::calloc(16, 1);
                    std::strcpy(names[i], text);
                }
                if (s == 0) *(char***)(uintptr_t)kNamesOriginal[k] = names;
                else *kNamesRecovered[k] = names;
            }
            g_recording = was;
        }
        g_visits[0].clear();
        g_visits[1].clear();
        bool crashed[2];
        for (int s = 0; s < 2; s++) crashed[s] = side(s, [&] {
            if (s == 0) at<FnDatabases>(0x0002EED0)(o, nullptr, g_world[0].owner, 0x31, freeNames);
            else ((AtSceneDatabaseLoader*)r)->LoadDatabases(g_world[1].owner, 0x31, freeNames);
        });
        eq("crashed", crashed[0], crashed[1]);
        compare_logs("databases");
        compare_visits();
        compare_statics();
        compare_loader((AtSceneDatabaseLoader*)o, (AtSceneDatabaseLoader*)r);
    }
    for (int k = 0; k < 10; k++) { *(char***)(uintptr_t)kNamesOriginal[k] = nullptr; *kNamesRecovered[k] = nullptr; }
    // Clear through the table, then destroy.
    g_context = "clear";
    bool crashed[2];
    for (int s = 0; s < 2; s++) crashed[s] = side(s, [&] {
        if (s == 0) at<Fn0>(0x0002F470)(o, nullptr);
        else ((AtSceneDatabaseLoader*)r)->Clear();
    });
    eq("crashed", crashed[0], crashed[1]);
    compare_logs("clear");
    compare_loader((AtSceneDatabaseLoader*)o, (AtSceneDatabaseLoader*)r);
    for (int s = 0; s < 2; s++) crashed[s] = side(s, [&] { destroy(s, kEntryLoader, s ? r : o); });
    compare_logs("destroy");

    g_files["W\\ONEI.ASI"] = "ImageDraw[1]\n{\n ImageDraw \"I1\" { Image \"Pic\" \"\" Position 10.5 -20.25 ColorValueRectangle 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 }\n}\n";
    // An item's own Clear, which a list never reaches: it destroys its items before reading them again.
    for (int k = 0; k < 2; k++) {
        const Target e = k ? kEntryTexts : kEntryImages;
        run(k ? "direct texts" : "direct images", e, k ? "W\\TXT" : "W\\ONEI", keep);
        // run() leaves the objects to the last case; find them again through the owners' tables.
        void* first[2] = {g_world[0].owner->FindInFirst("I1", nullptr), g_world[1].owner->FindInFirst("I1", nullptr)};
        if (k) {
            first[0] = g_world[0].owner->m_second.Find("T1");
            first[1] = g_world[1].owner->m_second.Find("T1");
        }
        if (getenv("KND_TRACE")) std::printf("clear %d: %p %p\n", k, first[0], first[1]);
        if (!first[0] || !first[1]) { eq("found for clear", first[0] != nullptr, first[1] != nullptr); continue; }
        g_context = k ? "text item clear" : "image item clear";
        bool crashed[2];
        for (int s2 = 0; s2 < 2; s2++) crashed[s2] = side(s2, [&] {
            if (s2 == 0) at<Fn0>(k ? 0x00049300 : 0x00048EE0)(first[0], nullptr);
            else if (k) ((AtTextDraw*)first[1])->Clear();
            else ((AtImageDraw*)first[1])->Clear();
        });
        eq("crashed", crashed[0], crashed[1]);
        compare_logs("item clear");
        if (k) {
            compare_item(*(AtTextDraw*)first[0], *(AtTextDraw*)first[1]);
            const AtTextDraw* a = (const AtTextDraw*)first[0];
            const AtTextDraw* b = (const AtTextDraw*)first[1];
            eq("font", (long long)(uintptr_t)a->m_font, (long long)(uintptr_t)b->m_font);
            eq("string", (long long)(uintptr_t)a->m_string, (long long)(uintptr_t)b->m_string);
            eqf("wrap", a->m_wrapWidth, b->m_wrapWidth);
            eqf("kerning", a->m_kerning, b->m_kerning);
        } else {
            compare_item(*(AtImageDraw*)first[0], *(AtImageDraw*)first[1]);
            eq("image", (long long)(uintptr_t)((AtImageDraw*)first[0])->m_image, (long long)(uintptr_t)((AtImageDraw*)first[1])->m_image);
        }
    }

    // A portal read again with its first flag set, through the recovered tokenizer on both sides.
    g_context = "portal again";
    for (int s = 0; s < 2; s++) {
        AtCellGraph* graph = (AtCellGraph*)object(sizeof(AtCellGraph), 0xCD);
        graph->m_mesh = &g_meshes[0];
        AtCellPortal* portal = (AtCellPortal*)object(sizeof(AtCellPortal), 0xCD);
        portal->m_flags = 0x1F;
        char* names = (char*)object(64, 0);
        AtIoTextTokenizer* tokenizer = new AtIoTextTokenizer;
        tokenizer->SetKeywords(AtCellGraphDatabase::s_keywords, AtCellGraphDatabase::kKeywordCount, 0);
        class Text : public AtIoStream { public: int Refill() override { return 0; } };
        Text* stream = new Text;
        const char text[] = "{ OppositeCellDifferentGraph \"g\" \"n\" \"c\" Vertices 1 3 5 }";
        stream->Allocate(sizeof text);
        std::memcpy(stream->m_base, text, sizeof text - 1);
        stream->m_limit = stream->m_base + sizeof text - 1;
        stream->m_cur = stream->m_base;
        tokenizer->SetStream(stream);
        *(uint32_t*)(uintptr_t)0x002C7EAC = 0;
        AtCellGraph::s_lockedArray = 0;
        if (s == 0) {
            *(uint32_t*)(uintptr_t)0x002C7E28 = 2;
            *(char**)(uintptr_t)0x002C7EA0 = names;
            *(char**)(uintptr_t)0x002C7EA4 = names + 24;
            *(char**)(uintptr_t)0x002C7EA8 = names + 48 - 16;
            *(uint32_t*)(uintptr_t)0x002C7E30 = 0;
        } else {
            AtCellGraph::s_portalIndex = 2;
            AtCellGraph::s_graphNames = names;
            AtCellGraph::s_spaceNames = names + 24;
            AtCellGraph::s_cellNames = names + 48 - 16;
            AtCellGraph::s_lock.m_data = 0;
        }
        static AtCellPortal kept[2];
        static char keptNames[2][64];
        side(s, [&] {
            if (s == 0) at<FnRead>(0x0005C4D0)(portal, nullptr, graph, tokenizer);
            else portal->Read(graph, tokenizer);
        });
        kept[s] = *portal;
        std::memcpy(keptNames[s], names, 64);
        if (s == 1) {
            compare_logs("portal again");
            const AtCellPortal& a = kept[0];
            const AtCellPortal& b = kept[1];
            eq("again flags", a.m_flags, b.m_flags);
            eq("again opposite", (long long)(uintptr_t)a.m_opposite, (long long)(uintptr_t)b.m_opposite);
            eq("again count", a.m_vertexCount, b.m_vertexCount);
            eqf("again distance", a.m_closedDistanceSquared, b.m_closedDistanceSquared);
            for (int k = 0; k < 3; k++) { eqf("again centre", a.m_center[k], b.m_center[k]); eqf("again normal", a.m_normal[k], b.m_normal[k]); }
            eq("again names", std::memcmp(keptNames[0], keptNames[1], 64), 0);
            *(char**)(uintptr_t)0x002C7EA0 = nullptr; *(char**)(uintptr_t)0x002C7EA4 = nullptr; *(char**)(uintptr_t)0x002C7EA8 = nullptr;
            AtCellGraph::s_graphNames = AtCellGraph::s_spaceNames = AtCellGraph::s_cellNames = nullptr;
        }
    }
}

// ---- real files -----------------------------------------------------------------------------------------------------------------------------

struct Row { std::string file, archive, path, data; };
static std::vector<Row> read_index(const std::string& dir) {
    std::vector<Row> rows;
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); std::exit(2); }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        Row row;
        char* fields[4] = {line, nullptr, nullptr, nullptr};
        for (int k = 1; k < 4; k++) {
            fields[k] = fields[k - 1] ? std::strchr(fields[k - 1], '\t') : nullptr;
            if (fields[k]) *fields[k]++ = 0;
        }
        if (!fields[2]) continue;
        const std::string path = upper(fields[2]);
        if (path.size() < 4) continue;
        const std::string ext = path.substr(path.size() - 4);
        if (ext != ".ASD" && ext != ".ASC" && ext != ".ASI" && ext != ".AST") continue;
        row.file = fields[0];
        row.archive = fields[1];
        row.path = path;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + row.file).c_str(), "rb") || !f) { std::printf("cannot read %s\n", row.file.c_str()); std::exit(2); }
        std::fseek(f, 0, SEEK_END);
        row.data.assign((size_t)std::ftell(f), 0);
        std::fseek(f, 0, SEEK_SET);
        if (!row.data.empty()) std::fread(&row.data[0], 1, row.data.size(), f);
        std::fclose(f);
        rows.push_back(row);
    }
    std::fclose(index);
    return rows;
}

static void run_real(const std::string& dir, unsigned& asd, unsigned& asc) {
    const std::vector<Row> rows = read_index(dir);
    std::map<std::string, std::string> children;
    for (const Row& row : rows)
        if (row.path.substr(row.path.size() - 4) != ".ASD" && !children.count(row.path)) children[row.path] = row.data;
    g_scale[0] = 1.0f; g_scale[1] = 1.0f;
    for (const Row& row : rows) {
        const std::string ext = row.path.substr(row.path.size() - 4);
        const std::string name = row.path.substr(0, row.path.size() - 4);
        if (ext == ".ASC") {
            g_files.clear();
            g_files[row.path] = row.data;
            run("real", kEntryCells, name);
            asc++;
        }
    }
    for (const Row& row : rows) {
        const std::string ext = row.path.substr(row.path.size() - 4);
        if (ext != ".ASD") continue;
        g_files = children;
        g_files[row.path] = row.data;
        run("real", kEntryLoader, row.path.substr(0, row.path.size() - 4));
        asd++;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_STACK_OVERFLOW) {
        if (g_expect_crash && getenv("KND_TRACE")) std::printf("crash at 0x%p reading 0x%p [%.100s]\n", e->ExceptionRecord->ExceptionAddress, (void*)e->ExceptionRecord->ExceptionInformation[1], g_context.c_str());
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
    if (pos.size() != 2) { std::printf("usage: oracle_asd <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push2);
    redirect(0x00011260, (const void*)o_pop2);
    redirect(0x00011070, (const void*)o_fatal2);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    redirect_children();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new PathFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    set_heaps();
    make_world();
    g_path_top = g_fs->m_path.m_top;
    g_heap_pad = 0x400;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_written_cells(); std::printf("written cells done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_written_draws(); std::printf("written draws done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_written_loader(); std::printf("written loader done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_shortages(); std::printf("shortages done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_direct(); std::printf("direct done: %llu comparisons, %d differences\n", g_checks, g_failures);
    unsigned asd = 0, asc = 0;
    run_real(pos[1], asd, asc);
    report_entries();
    std::printf("%s: %u .ASD, %u .ASC files; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", asd, asc, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
