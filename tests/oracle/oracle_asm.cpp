// Differential oracle for modifiers (.ASM) and space graphs (.ASG) (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-07-asm.md.
//
//   oracle_asm <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ASM and .ASG file listed in index.txt, each loaded twice, cleared and destroyed; text written for each word and error path; running out of each heap; and links, modifier states, spaces and modifiers driven directly.
//
// What stands in for what the files name: nodes are the recovered base node with a harness FinishBody, on both sides. Lights, animations, cell tables and cells are blobs in real name maps. Modifiers a graph names are real: each side loads a
// generated .ASM file through its own reader first. The owner and device are the recovered layouts, with real maps. So the original's own reader, spaces, modifiers and modifier states run against the recovered ones.
#define NOMINMAX
#include <float.h>
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

#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtModifier.h"
#include "at/AtScene.h"
#include "at/AtSpaceGraph.h"
#include "at/AtText.h"
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

// What the code under test frees stays as it was, unmoved and not reused. A graph clears with links left behind in its nodes when a node stood in two leaves in one kind (see the worklog), and reads them on the next clear: without
// this, what they hold would depend on the heap of the side they are on.
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
// The order of freeing is otherwise unseen, since the log says only that something was.
enum { kFreedMax = 1 << 16 };
static void* g_freed[2][kFreedMax];
static int g_freed_count[2];
void std::quarantine_free(void* p) {
    if (!g_recording) { ::free(p); return; }
    if (p && g_freed_count[g_side] < kFreedMax) g_freed[g_side][g_freed_count[g_side]++] = p;
}

// A file ending inside a body makes both sides report the end for ever, since the report comes back here and not in the game. So a deliberate crash ends it after this many, at the same report on both sides.
static int g_reports;
static void count_report() {
    if (g_expect_crash && ++g_reports > 40) *(volatile int*)nullptr = 0;
}
void AtFatal(const char* m, int a, int b, int c) { AtFatalInDoubles(m, a, b, c); count_report(); }
static void __cdecl o_fatal2(const char* m, int a, int b, int c) { o_fatal(m, a, b, c); count_report(); }

// The log has a limit, so a running hash of every heap push, with the alignment that heap had then. To refuse memory to one heap only.
static uint64_t g_hash[2];
static void* g_refuse_heap;
static int g_refusing;
static int g_refuse_skip[2];  // Pushes of that heap to let through first.
static int g_null_graph_heap_after[2];  // The checks for a null heap come after the heap has been used, so it has to go null between.
static void* g_null_graph_target;
static void note_push(int side, void* h) {
    if (!g_recording) return;
    const unsigned shift = h ? ((AtHeap*)h)->m_allocShift : 0xFFFFu;
    g_log[side].add('A', shift);
    g_hash[side] = (g_hash[side] ^ (uint32_t)(uintptr_t)h) * 0x100000001B3ULL;
    g_hash[side] = (g_hash[side] ^ shift) * 0x100000001B3ULL;
    if (h == g_refuse_heap && h) {
        if (g_refuse_skip[side] > 0) g_refuse_skip[side]--;
        else { g_fail_alloc = true; g_refusing++; }
    }
    if (h == g_null_graph_target && g_null_graph_heap_after[side] > 0 && --g_null_graph_heap_after[side] == 0) {
        if (side == 0) *(void**)(uintptr_t)0x002C63FC = nullptr;
        else AtSpaceGraph::s_heap = nullptr;
    }
}
static void note_pop() { if (g_refusing) { g_refusing--; g_fail_alloc = false; } }
void AtPushHeap(void* h) { AtPushHeapInDoubles(h); note_push(1, h); }
void AtPopHeap() { AtPopHeapInDoubles(); note_pop(); }
static void __cdecl o_push2(void* h) { o_push(h); note_push(0, h); }
static void __cdecl o_pop2() { o_pop(); note_pop(); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];

struct FakeHeap { unsigned char bytes[sizeof(AtHeap)]; };
static FakeHeap g_heap_file, g_heap_graph, g_heap_db, g_heap_context, g_heap_group, g_heap_stack, g_heap_state;
static FakeHeap* const kHeaps[] = {&g_heap_graph, &g_heap_db, &g_heap_context, &g_heap_group, &g_heap_stack, &g_heap_state, &g_heap_file};
static void set_alignment(unsigned shift) {
    for (FakeHeap* h : kHeaps) { ((AtHeap*)h)->m_alignShift = shift; ((AtHeap*)h)->m_allocShift = shift; }
}
static void same_alignment_restored(unsigned shift) {
    for (FakeHeap* h : kHeaps) eq("heap's alignment afterwards", ((AtHeap*)h)->m_allocShift, shift);
}

// ---- functions of the original, by address ------------------------------------------------------------------------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnP)(void*, void*, void*);
typedef void(__fastcall* FnPP)(void*, void*, void*, void*);
typedef void(__cdecl* FnHeap)(void*);

enum { kGraphLoad = 0x0003ADC0, kGraphCtor = 0x00039FD0, kGraphClear = 0x0003A030, kGraphDtor = 0x0003A450, kGraphBuild = 0x0003A410, kDbLoad = 0x0003F390, kDbCtor = 0x0003EA00 };

// The vftables, one row to a class: original, recovered.
enum { kTables = 12 };
static const void* g_table_o[kTables];
static const void* g_table_r[kTables];
static long long table_id(int side, const void* table) {
    for (int i = 0; i < kTables; i++) if ((side ? g_table_r[i] : g_table_o[i]) == table) return i + 1;
    return -1;
}

// ---- nodes, the same class on both sides ---------------------------------------------------------------------------------------------------
class TestNode : public AtSceneNode {
public:
    int m_id = 0;
    void FinishBody() override {
        if (g_recording) g_log[g_side].add('f', m_id);
        // Odd ones stay moved, so that a second test of the flag is seen.
        if ((m_id & 1) == 0) m_flags &= ~(unsigned int)kMoved;
        m_worldSphere.radius += 0.5f;
    }
};

static uint32_t hash_of(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}

// ---- pointers, by what they lead to ----------------------------------------------------------------------------------------------------------
struct Range { const char* base; size_t size; size_t stride; long long tag; };
struct Registry {
    std::vector<Range> ranges;
    void add(const void* base, size_t count, size_t stride, long long tag) { if (base && count) ranges.push_back({(const char*)base, count * stride, stride, tag}); }
    long long id(const void* p) const {
        if (!p) return 0;
        const char* c = (const char*)p;
        for (const Range& r : ranges) {
            if (c >= r.base && c < r.base + r.size) return r.tag * 1000000 + (c - r.base) / r.stride + ((c - r.base) % r.stride ? 500000 : 0);
        }
        return -1;
    }
};
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

// ---- a world: an owner and what it can find ----------------------------------------------------------------------------------------------------
struct Blob { int word0; int value; };
struct World {
    AtNameTableOwner* owner = nullptr;
    AtSceneDevice* device = nullptr;
    GmNameMap* lights = nullptr; GmNameMap* animations = nullptr; GmNameMap* nodes = nullptr; GmNameMap* tables = nullptr; GmNameMap* lightsNs = nullptr; GmNameMap* animationsNs = nullptr; GmNameMap* nodesNs = nullptr; GmNameMap* tablesNs = nullptr; GmNameMap* modifiersNs = nullptr;
    GmNameMap* standIns = nullptr;
    AtSpaceCellTable* table = nullptr;
    AtSpaceCellTable* tableNs = nullptr;
    std::vector<Blob> cellBlobsNs;
    std::vector<Blob> lightBlobs, animationBlobs, cellBlobs;
    std::vector<unsigned char> nodeMemory;
    TestNode* nodeArray = nullptr;
    std::vector<std::string> names;
    Registry registry;
};

class TestDevice : public AtSceneDevice {
public:
    void Slot0() override {} void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {} void Slot6() override {} void Slot7() override {} void Slot8() override {}
    void Slot9() override {} void Slot10() override {} void Slot11() override {} void Slot12() override {} void Slot13() override {} void Slot14() override {} void Slot15() override {} void Slot16() override {}
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
};

static std::string name8(const std::string& s) { return s.substr(0, 8); }

static void gather(const std::string& text, std::set<std::string>& names) {
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '"') continue;
        const size_t end = text.find('"', i + 1);
        if (end == std::string::npos) break;
        const std::string s = name8(text.substr(i + 1, end - i - 1));
        if (!s.empty() && s[0] != 'X') names.insert(s);
        i = end;
    }
    names.insert("Stand");
}

static void make_world(World& w, const std::set<std::string>& names, unsigned depth, unsigned slots) {
    const bool was = g_recording;
    g_recording = false;
    w.names.assign(names.begin(), names.end());
    const unsigned n = (unsigned)w.names.size();
    w.device = new TestDevice;
    w.owner = new AtNameTableOwner;
    w.owner->m_device = w.device;
    w.owner->m_modifierStack.m_depth = 0;
    w.owner->m_modifierStack.m_used = 0;
    w.owner->m_modifierStack.m_states = nullptr;
    w.owner->m_stackDepth = depth;
    w.owner->m_stackSlots = slots;
    w.owner->m_modifierSpaces.Init(8);
    w.owner->m_nodeSpaces.Init(8);
    w.owner->m_cellTableSpaces.Init(8);
    w.device->m_lightSpaces.Init(8);
    w.device->m_lightAnimationSpaces.Init(8);

    w.lightBlobs.assign(n + 1, Blob{});
    w.animationBlobs.assign(n + 1, Blob{});
    w.cellBlobs.assign(n + 1, Blob{});
    w.lights = new GmNameMap; w.animations = new GmNameMap; w.nodes = new GmNameMap; w.tables = new GmNameMap; w.standIns = new GmNameMap;
    w.lightsNs = new GmNameMap; w.animationsNs = new GmNameMap; w.nodesNs = new GmNameMap; w.tablesNs = new GmNameMap; w.modifiersNs = new GmNameMap;
    for (GmNameMap* m : {w.lights, w.animations, w.nodes, w.tables, w.lightsNs, w.animationsNs, w.nodesNs, w.tablesNs, w.modifiersNs}) m->Init(n * 2 + 8);
    w.table = (AtSpaceCellTable*)std::calloc(1, sizeof(AtSpaceCellTable));
    new (&w.table->m_cells) GmNameMap;
    w.table->m_cells.Init(n * 2 + 8);
    w.tableNs = (AtSpaceCellTable*)std::calloc(1, sizeof(AtSpaceCellTable));
    new (&w.tableNs->m_cells) GmNameMap;
    w.tableNs->m_cells.Init(n * 2 + 8);
    w.cellBlobsNs.assign(n + 1, Blob{});
    w.nodeMemory.assign((n + 1) * sizeof(TestNode), 0);
    w.nodeArray = (TestNode*)w.nodeMemory.data();
    for (unsigned i = 0; i < n; i++) {
        const std::string& s = w.names[i];
        const uint32_t h = hash_of(s);
        w.lightBlobs[i].value = (int)(h & 0xFF);
        w.animationBlobs[i].value = (int)(h >> 8 & 0xFF);
        w.lights->Insert(s.c_str(), &w.lightBlobs[i]);
        w.animations->Insert(s.c_str(), &w.animationBlobs[i]);
        w.lightsNs->Insert(s.c_str(), &w.lightBlobs[i]);
        w.animationsNs->Insert(s.c_str(), &w.animationBlobs[i]);
        w.tablesNs->Insert(s.c_str(), w.tableNs);
        w.tableNs->m_cells.Insert(s.c_str(), &w.cellBlobsNs[i]);
        w.tables->Insert(s.c_str(), w.table);
        w.table->m_cells.Insert(s.c_str(), &w.cellBlobs[i]);
        TestNode* node = new (&w.nodeArray[i]) TestNode;
        node->m_id = (int)i + 1;
        node->m_class = s[0] == 'Q' ? 0 : (unsigned short)(1 + h % 13);
        node->m_owner = w.owner;
        node->m_worldSphere.center.x = (float)(int)(h % 40000) - 20000.0f;
        node->m_worldSphere.center.y = (float)(int)((h >> 5) % 40000) - 20000.0f;
        node->m_worldSphere.center.z = (float)(int)((h >> 9) % 40000) - 20000.0f;
        node->m_worldSphere.radius = (float)(h >> 21 & 0x3FF);
        if (s.find('M') != std::string::npos) node->m_flags |= AtSceneNode::kMoved;
        w.nodes->Insert(s.c_str(), node);
        w.nodesNs->Insert(s.c_str(), node);
    }
    w.device->m_lights.Add(w.lights);
    w.device->m_lightAnimations.Add(w.animations);
    w.owner->m_nodes.Add(w.nodes);
    w.owner->m_cellTables.Add(w.tables);
    w.device->m_lightSpaces.Add("SP", w.lightsNs);
    w.device->m_lightAnimationSpaces.Add("SP", w.animationsNs);
    w.owner->m_nodeSpaces.Add("SP", w.nodesNs);
    w.owner->m_cellTableSpaces.Add("SP", w.tablesNs);
    w.owner->m_modifierSpaces.Add("SP", w.modifiersNs);
    w.registry.ranges.clear();
    w.registry.add(w.lightBlobs.data(), n + 1, sizeof(Blob), 1);
    w.registry.add(w.animationBlobs.data(), n + 1, sizeof(Blob), 2);
    w.registry.add(w.cellBlobs.data(), n + 1, sizeof(Blob), 3);
    w.registry.add(w.nodeArray, n + 1, sizeof(TestNode), 4);
    w.registry.add(w.cellBlobsNs.data(), n + 1, sizeof(Blob), 5);
    g_recording = was;
}

// ---- what a modifier database holds ------------------------------------------------------------------------------------------------------------
typedef std::vector<long long> Dump;
static void put(Dump& d, long long v) { d.push_back(v); }

static void register_db(Registry& r, const AtModifierDatabase* db, long long base) {
    r.add(db->m_lights, db->m_lightCount, sizeof(AtLightModifier), base + 0);
    r.add(db->m_ambients, db->m_ambientCount, sizeof(AtAmbientModifier), base + 1);
    r.add(db->m_perspectives, db->m_perspectiveCount, sizeof(AtPerspectiveModifier), base + 2);
    r.add(db->m_contexts, db->m_contextCount, sizeof(AtContextConstantModifier), base + 3);
    r.add(db->m_groups, db->m_groupCount, sizeof(AtGroupModifier), base + 4);
    r.add(db->m_blocks, db->m_blockCount, sizeof(AtBlockModifier), base + 5);
}

static void dump_modifier(Dump& d, int side, const AtModifier* m) {
    put(d, table_id(side, *(const void* const*)m));
    put(d, m->m_flags);
    put(d, m->m_kind);
    put(d, m->m_priority);
}

static void dump_db(Dump& d, int side, const AtModifierDatabase* db, const Registry& r) {
    put(d, db->m_lightCount); put(d, db->m_ambientCount); put(d, db->m_perspectiveCount); put(d, db->m_contextCount); put(d, db->m_groupCount); put(d, db->m_blockCount);
    put(d, db->m_lights != nullptr); put(d, db->m_ambients != nullptr); put(d, db->m_perspectives != nullptr); put(d, db->m_contexts != nullptr); put(d, db->m_groups != nullptr); put(d, db->m_blocks != nullptr);
    put(d, db->m_size);
    for (unsigned i = 0; i < db->m_size; i++) {
        if (!db->m_entries[i].m_value) { put(d, 0); continue; }
        char name[9] = {};
        std::memcpy(name, db->m_entries[i].m_name, 8);
        put(d, hash_of(name));
        put(d, r.id(db->m_entries[i].m_value));
    }
    #define COUNT_AHEAD(list) if (list) put(d, ((const int*)(list))[-1])
    COUNT_AHEAD(db->m_lights); COUNT_AHEAD(db->m_ambients); COUNT_AHEAD(db->m_perspectives); COUNT_AHEAD(db->m_contexts); COUNT_AHEAD(db->m_groups); COUNT_AHEAD(db->m_blocks);
    #undef COUNT_AHEAD
    for (unsigned i = 0; db->m_lights && i < db->m_lightCount; i++) {
        const AtLightModifier& m = db->m_lights[i];
        dump_modifier(d, side, &m);
        put(d, r.id(m.m_light));
        put(d, m.m_type);
        put(d, bits(m.m_direction.x)); put(d, bits(m.m_direction.y)); put(d, bits(m.m_direction.z));
        put(d, bits(m.m_position.x)); put(d, bits(m.m_position.y)); put(d, bits(m.m_position.z));
        put(d, m.m_unknown30);
        put(d, m.m_playback.m_flags); put(d, bits(m.m_playback.m_time)); put(d, bits(m.m_playback.m_rate)); put(d, r.id(m.m_playback.m_listener)); put(d, r.id(m.m_playback.m_animation));
    }
    for (unsigned i = 0; db->m_ambients && i < db->m_ambientCount; i++) {
        const AtAmbientModifier& m = db->m_ambients[i];
        dump_modifier(d, side, &m);
        put(d, bits(m.m_color.x)); put(d, bits(m.m_color.y)); put(d, bits(m.m_color.z)); put(d, bits(m.m_color.w));
    }
    for (unsigned i = 0; db->m_perspectives && i < db->m_perspectiveCount; i++) {
        const AtPerspectiveModifier& m = db->m_perspectives[i];
        dump_modifier(d, side, &m);
        put(d, bits(m.m_fieldOfViewY)); put(d, bits(m.m_saved));
    }
    for (unsigned i = 0; db->m_contexts && i < db->m_contextCount; i++) {
        const AtContextConstantModifier& m = db->m_contexts[i];
        dump_modifier(d, side, &m);
        put(d, m.m_vertexCount); put(d, m.m_pixelCount);
        put(d, m.m_vertex != nullptr); put(d, m.m_pixel != nullptr);
        for (unsigned j = 0; m.m_vertex && j < m.m_vertexCount; j++) { for (float v : m.m_vertex[j].m_value) put(d, bits(v)); put(d, m.m_vertex[j].m_index); }
        for (unsigned j = 0; m.m_pixel && j < m.m_pixelCount; j++) { for (float v : m.m_pixel[j].m_value) put(d, bits(v)); put(d, m.m_pixel[j].m_index); }
    }
    for (unsigned i = 0; db->m_groups && i < db->m_groupCount; i++) {
        const AtGroupModifier& m = db->m_groups[i];
        dump_modifier(d, side, &m);
        put(d, m.m_count); put(d, m.m_capacity); put(d, m.m_members != nullptr);
        for (unsigned j = 0; m.m_members && j < m.m_count; j++) put(d, r.id(m.m_members[j]));
    }
    for (unsigned i = 0; db->m_blocks && i < db->m_blockCount; i++) {
        const AtBlockModifier& m = db->m_blocks[i];
        dump_modifier(d, side, &m);
        put(d, m.m_blocks);
    }
}

static void same_dump(const char* what, const Dump& o, const Dump& r) {
    eq(what, (long long)o.size(), (long long)r.size());
    size_t differing = 0;
    for (size_t i = 0; i < o.size() && i < r.size(); i++) {
        g_checks++;
        if (o[i] != r[i]) {
            if (differing++ == 0 && g_failures++ < 25) std::printf("DIFF %s: %s: word %u: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, (unsigned)i, o[i], r[i]);
        }
    }
}

// ---- what a graph holds -----------------------------------------------------------------------------------------------------------------------
static void register_graph(Registry& r, const AtSpaceGraph* g) {
    r.add(g->m_links, g->m_linkCount, sizeof(AtSceneLink), 10);
    r.add(g->m_items, g->m_itemCount, sizeof(AtSceneLinkItem), 11);
    r.add(g->m_binary, g->m_binaryCount, sizeof(AtBinarySpace), 12);
    r.add(g->m_x, g->m_xCount, sizeof(AtXBinarySpace), 13);
    r.add(g->m_y, g->m_yCount, sizeof(AtYBinarySpace), 14);
    r.add(g->m_z, g->m_zCount, sizeof(AtZBinarySpace), 15);
    r.add(g->m_leaves, g->m_leafCount, sizeof(AtLeafSpace), 16);
    r.add(g, 1, sizeof(AtSpaceGraph), 17);
}

static void dump_state(Dump& d, const AtModifierState& s, const Registry& r) {
    put(d, s.m_count);
    put(d, s.m_slots != nullptr);
    for (unsigned i = 0; s.m_slots && i < s.m_count; i++) {
        const AtModifierSlot& slot = s.m_slots[i];
        put(d, slot.m_count);
        for (int j = 0; j < AtModifierSlot::kMaxEntries; j++) { put(d, r.id(slot.m_entries[j].m_modifier)); put(d, slot.m_entries[j].m_data); }
    }
}

static void dump_space_common(Dump& d, int side, const AtSpace* s, const Registry& r) {
    put(d, table_id(side, *(const void* const*)s));
    put(d, r.id(s->m_modifier));
    for (const AtSpace::Pending* p = s->m_pending; p; p = p->m_next) { put(d, r.id(p->m_graph)); put(d, p->m_kind); put(d, r.id(p->m_node)); }
    put(d, -9);
}

static void dump_graph(Dump& d, int side, const AtSpaceGraph* g, const Registry& r) {
    put(d, g->m_linkCount); put(d, g->m_itemCount); put(d, g->m_binaryCount); put(d, g->m_xCount); put(d, g->m_yCount); put(d, g->m_zCount); put(d, g->m_leafCount);
    put(d, r.id(g->m_root)); put(d, r.id(g->m_freeLinks)); put(d, r.id(g->m_freeItems)); put(d, r.id(g->m_hooksBefore)); put(d, r.id(g->m_hooksAfter)); put(d, g->m_kind);
    for (AtSceneLink* l : g->m_dirty) put(d, r.id(l));
    const int* in_use = side ? &AtSpaceGraph::s_itemsInUse : (const int*)(uintptr_t)0x002C63F4;
    const int* peak = side ? &AtSpaceGraph::s_itemsPeak : (const int*)(uintptr_t)0x002C63F8;
    put(d, *in_use); put(d, *peak);
    for (unsigned i = 0; g->m_links && i < g->m_linkCount; i++) {
        const AtSceneLink& l = g->m_links[i];
        put(d, r.id(l.m_set)); put(d, bits(l.m_center.x)); put(d, bits(l.m_center.y)); put(d, bits(l.m_center.z)); put(d, bits(l.m_margin)); put(d, r.id(l.m_next)); put(d, r.id(l.m_items)); put(d, r.id(l.m_nextDirty));
    }
    for (unsigned i = 0; g->m_items && i < g->m_itemCount; i++) {
        const AtSceneLinkItem& it = g->m_items[i];
        put(d, bits(it.m_low)); put(d, bits(it.m_high)); put(d, r.id(it.m_list)); put(d, r.id(it.m_node)); put(d, r.id(it.m_next)); put(d, r.id(it.m_head)); put(d, r.id(it.m_prevInList)); put(d, r.id(it.m_nextInList));
    }
    for (unsigned i = 0; g->m_binary && i < g->m_binaryCount; i++) {
        const AtBinarySpace& s = g->m_binary[i];
        dump_space_common(d, side, &s, r);
        put(d, r.id(s.m_posChild)); put(d, r.id(s.m_negChild));
        put(d, bits(s.m_plane.x)); put(d, bits(s.m_plane.y)); put(d, bits(s.m_plane.z)); put(d, bits(s.m_plane.w));
    }
    const AtAxisBinarySpace* axes[3] = {g->m_x, g->m_y, g->m_z};
    const unsigned counts[3] = {g->m_xCount, g->m_yCount, g->m_zCount};
    for (int a = 0; a < 3; a++) {
        for (unsigned i = 0; axes[a] && i < counts[a]; i++) {
            const AtAxisBinarySpace& s = axes[a][i];
            dump_space_common(d, side, &s, r);
            put(d, r.id(s.m_posChild)); put(d, r.id(s.m_negChild)); put(d, bits(s.m_distance));
        }
    }
    for (unsigned i = 0; g->m_leaves && i < g->m_leafCount; i++) {
        const AtLeafSpace& s = g->m_leaves[i];
        dump_space_common(d, side, &s, r);
        put(d, r.id(s.m_cell)); put(d, s.m_sortAxis);
        for (AtSceneLinkItem* it : s.m_first) put(d, r.id(it));
        dump_state(d, s.m_state, r);
    }
}

static void dump_nodes(Dump& d, const World& w) {
    for (size_t i = 0; i < w.names.size(); i++) {
        const TestNode& n = w.nodeArray[i];
        put(d, n.m_flags);
        for (AtSceneLink* l : n.m_links) put(d, w.registry.id(l));
        put(d, bits(n.m_worldSphere.radius));
    }
}

// ---- running one text -------------------------------------------------------------------------------------------------------------------------
static void set_text(const std::string& text) {
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)text.data();
    g_fs->files[0].size = g_fs->files[1].size = text.size();
}

static AtModifierDatabase* new_db(int side, unsigned char fill) {
    unsigned char* p = (unsigned char*)std::malloc(sizeof(AtModifierDatabase));
    std::memset(p, fill, sizeof(AtModifierDatabase));
    On on(side);
    if (side == 0) { at<Fn0>(kDbCtor)(p, nullptr); return (AtModifierDatabase*)p; }
    return new (p) AtModifierDatabase;
}

static void load_db(int side, AtModifierDatabase* db, AtNameTableOwner* owner) {
    g_reports = 0;
    On on(side);
    if (side == 0) at<FnLoad>(kDbLoad)(db, nullptr, owner, "ART\\THING", 77);
    else db->Load(owner, "ART\\THING", 77);
}

static void clear_db(int side, AtModifierDatabase* db) { On on(side); db->Clear(); }
// The original's through slot 0, with the flag that does not free: that reaches its destructor. The recovered one directly.
static void destroy_db(int side, AtModifierDatabase* db) {
    On on(side);
    if (side == 0) at<void*(__fastcall*)(void*, void*, unsigned)>(0x0002E760)(db, nullptr, 0);
    else db->~AtModifierDatabase();
    std::free(db);
}

static void check_logs(const char* what) {
    _fpreset();  // A crash in the original can leave reals on the x87 stack, and enough of those make later results NaN.
    compare_logs(what);
    eq("hash of the heaps", (long long)g_hash[0], (long long)g_hash[1]);
    g_hash[0] = g_hash[1] = 0;
}

static unsigned g_asm_files, g_asg_files, g_links_made;

static void start_freed() { g_freed_count[0] = g_freed_count[1] = 0; }
// An array's block starts a word ahead of its first element; a pool's at it. So an address one word in names either.
static void same_freed(const char* what, const Registry& a, const Registry& b) {
    eq(what, g_freed_count[0], g_freed_count[1]);
    for (int i = 0; i < g_freed_count[0] && i < g_freed_count[1]; i++) eq(what, a.id((char*)g_freed[0][i] + 4), b.id((char*)g_freed[1][i] + 4));
}

static void run_asm(const std::string& text, const std::string& context, unsigned char fill, unsigned shift) {
    g_context = context;
    if (getenv("KND_TRACE")) std::printf("asm [%s]\n", context.c_str());
    std::set<std::string> names;
    gather(text, names);
    static std::vector<World*> keep;  // A crash leaves what the two sides made, and it is simplest to leave all of it.
    World* w[2] = {new World, new World};
    keep.push_back(w[0]); keep.push_back(w[1]);
    for (int s = 0; s < 2; s++) {
        make_world(*w[s], names, 16, 6);
        // A group only keeps what it finds, so a blob will do in the name space.
        for (size_t i = 0; i < w[s]->names.size(); i++) w[s]->modifiersNs->Insert(w[s]->names[i].c_str(), &w[s]->lightBlobs[i]);
    }
    set_text(text);
    set_alignment(shift);
    g_fill = fill;
    AtModifierDatabase* db[2] = {new_db(0, fill), new_db(1, fill)};
    {
        Dump a, b;
        dump_db(a, 0, db[0], w[0]->registry);
        dump_db(b, 1, db[1], w[1]->registry);
        same_dump("constructed", a, b);
    }
    for (int pass = 0; pass < 2; pass++) {
        bool crashed[2];
        for (int s = 0; s < 2; s++) { crashed[s] = guarded([&] { load_db(s, db[s], w[s]->owner); }); g_recording = false; g_fail_alloc = false; g_refusing = 0; }
        eq("crash", crashed[0], crashed[1]);
        check_logs("load");
        if (crashed[0] || crashed[1]) return;
        same_alignment_restored(shift);
        Dump d[2];
        for (int s = 0; s < 2; s++) { Registry r = w[s]->registry; register_db(r, db[s], 100); dump_db(d[s], s, db[s], r); }
        same_dump("loaded", d[0], d[1]);
        eq("in the plain group", db[0]->m_group == &w[0]->owner->m_modifiers, db[1]->m_group == &w[1]->owner->m_modifiers);
        eq("first in the plain group", w[0]->owner->m_modifiers.m_first == db[0], w[1]->owner->m_modifiers.m_first == db[1]);
        char a[9] = {}, b[9] = {};
        if (db[0]->m_group) std::memcpy(a, db[0]->m_group->m_name, 8);
        if (db[1]->m_group) std::memcpy(b, db[1]->m_group->m_name, 8);
        eqs("group's name", a, b);
        g_asm_files += pass == 0;
    }
    Registry cleared[2] = {w[0]->registry, w[1]->registry};
    for (int s = 0; s < 2; s++) register_db(cleared[s], db[s], 100);
    start_freed();
    for (int s = 0; s < 2; s++) clear_db(s, db[s]);
    same_freed("order of freeing", cleared[0], cleared[1]);
    check_logs("clear");
    {
        Dump a, b;
        dump_db(a, 0, db[0], w[0]->registry);
        dump_db(b, 1, db[1], w[1]->registry);
        same_dump("cleared", a, b);
    }
    // Loaded once more, and destroyed loaded.
    for (int s = 0; s < 2; s++) guarded([&] { load_db(s, db[s], w[s]->owner); });
    check_logs("reloaded");
    for (int s = 0; s < 2; s++) destroy_db(s, db[s]);
    check_logs("destroyed");
}

// A modifier of each class for each name a graph gives as a modifier, in the shapes of the classes' readers, at priorities that make the slots' choices matter.
static std::string modifiers_for(const std::set<std::string>& names) {
    std::vector<std::string> list(names.begin(), names.end());
    std::string l, a, p, b;
    unsigned counts[6] = {};
    std::vector<std::string> plain;
    for (size_t i = 0; i < list.size(); i++) {
        const std::string q = "\"" + list[i] + "\"";
        const std::string pri = " Priority " + std::to_string((int)(hash_of(list[i]) % 7) - 3) + "\n";
        switch (i % 6) {
        case 0: case 1: l += " LightModifier " + q + " { Light " + q + " \"\" Point Position 1.5 2.5 3.5" + pri + " }\n"; counts[0]++; plain.push_back(q); break;
        case 2: a += " AmbientModifier " + q + " { Color 0.25 0.5 0.75 1.0" + pri + " }\n"; counts[1]++; plain.push_back(q); break;
        case 3: p += " PerspectiveModifier " + q + " { FieldOfViewY 0.5" + pri + " }\n"; counts[2]++; plain.push_back(q); break;
        case 4: b += " BlockModifier " + q + " { Block LightModifier" + pri + " }\n"; counts[5]++; break;
        default: counts[4]++; break;
        }
    }
    std::string t = "Modifier[" + std::to_string(list.size() + 1) + "]\n";
    t += "LightModifier[" + std::to_string(counts[0]) + "]\n{\n" + l + "}\n";
    t += "AmbientModifier[" + std::to_string(counts[1]) + "]\n{\n" + a + "}\n";
    t += "PerspectiveModifier[" + std::to_string(counts[2]) + "]\n{\n" + p + "}\n";
    t += "BlockModifier[" + std::to_string(counts[5]) + "]\n{\n" + b + "}\n";
    // Groups last, and each holding what came before it.
    std::string members;
    for (const std::string& q : plain) members += q + " \"\" ";
    std::string groups;
    for (size_t i = 0; i < list.size(); i++) {
        if (i % 6 != 5) continue;
        groups += " GroupModifier \"" + list[i] + "\" {\n Modifier[" + std::to_string(plain.size()) + "]\n {\n " + members + "\n }\n Priority 1 }\n";
    }
    t += "GroupModifier[" + std::to_string(counts[4]) + "]\n{\n" + groups + "}\n";
    return t;
}

// What a graph's text gives as a modifier: the string after the word, wherever it is. A name in the file may be repeated, and is one modifier.
static void modifier_names(const std::string& text, std::set<std::string>& out) {
    size_t i = 0;
    while ((i = text.find("Modifier", i)) != std::string::npos) {
        i += 8;
        size_t q = i;
        while (q < text.size() && (text[q] == ' ' || text[q] == '\t')) q++;
        if (q < text.size() && text[q] == '"') {
            const size_t end = text.find('"', q + 1);
            if (end != std::string::npos) {
                const std::string s = name8(text.substr(q + 1, end - q - 1));
                if (!s.empty() && s[0] != 'X') out.insert(s);
            }
        }
    }
}

static void* g_refuse_for_graph;  // Not while the modifiers a graph names are read.
static int g_refuse_for_graph_skip;
static int g_graph_heap_null_after;
static bool g_graph_file_missing;

struct GraphRun {
    World* w[2];
    AtSpaceGraph* g[2];
    AtModifierDatabase* db[2];
    std::string modifiers;
};

static AtSpaceGraph* new_graph(int side, unsigned char fill) {
    unsigned char* p = (unsigned char*)std::malloc(sizeof(AtSpaceGraph));
    std::memset(p, fill, sizeof(AtSpaceGraph));
    On on(side);
    if (side == 0) { at<Fn0>(kGraphCtor)(p, nullptr); return (AtSpaceGraph*)p; }
    return new (p) AtSpaceGraph;
}

static void load_graph(int side, AtSpaceGraph* g, AtNameTableOwner* owner) {
    g_reports = 0;
    On on(side);
    if (side == 0) at<FnLoad>(kGraphLoad)(g, nullptr, owner, "ART\\THING", 77);
    else g->Load(owner, "ART\\THING", 77);
}

static void same_graph(const char* what, GraphRun& run) {
    Dump d[2], n[2];
    for (int s = 0; s < 2; s++) {
        Registry r = run.w[s]->registry;
        register_db(r, run.db[s], 100);
        register_graph(r, run.g[s]);
        dump_graph(d[s], s, run.g[s], r);
        Registry saved = run.w[s]->registry;
        run.w[s]->registry = r;
        dump_nodes(n[s], *run.w[s]);
        run.w[s]->registry = saved;
    }
    same_dump(what, d[0], d[1]);
    same_dump("nodes", n[0], n[1]);
}

static bool prepare_graph(GraphRun& run, const std::string& text, unsigned depth, unsigned slots, unsigned char fill, unsigned shift) {
    std::set<std::string> names, mods;
    gather(text, names);
    modifier_names(text, mods);
    mods.insert("Stand");
    run.modifiers = modifiers_for(mods);
    gather(run.modifiers, names);
    set_alignment(shift);
    g_fill = fill;
    for (int s = 0; s < 2; s++) {
        run.w[s] = new World;
        make_world(*run.w[s], names, depth, slots);
    }
    set_text(run.modifiers);
    bool crashed[2];
    for (int s = 0; s < 2; s++) {
        run.db[s] = new_db(s, fill);
        crashed[s] = guarded([&] { load_db(s, run.db[s], run.w[s]->owner); });
        g_recording = false;
    }
    check_logs("the graph's modifiers");
    for (int s = 0; !crashed[s] && s < 2; s++) {
        for (const std::string& name : mods) {
            void* found = run.db[s]->Find(name.c_str());
            if (found) run.w[s]->modifiersNs->Insert(name.c_str(), found);
        }
    }
    if (crashed[0] || crashed[1]) { std::printf("the generated modifiers crashed for %s\n", g_context.c_str()); g_failures++; return false; }
    for (int s = 0; s < 2; s++) run.g[s] = new_graph(s, fill);
    { Dump a, b; Registry r; dump_graph(a, 0, run.g[0], r); dump_graph(b, 1, run.g[1], r); same_dump("constructed", a, b); }
    return true;
}

static void run_asg(const std::string& text, const std::string& context, unsigned char fill, unsigned shift, unsigned depth = 32, unsigned slots = 6) {
    g_context = context;
    if (getenv("KND_TRACE")) std::printf("asg [%s]\n", context.c_str());
    GraphRun run;
    if (!prepare_graph(run, text, depth, slots, fill, shift)) return;
    set_text(text);
    g_refuse_heap = g_refuse_for_graph;
    g_refuse_skip[0] = g_refuse_skip[1] = g_refuse_for_graph_skip;
    g_null_graph_heap_after[0] = g_null_graph_heap_after[1] = g_graph_heap_null_after;
    g_null_graph_target = g_graph_heap_null_after ? &g_heap_graph : nullptr;
    g_fs->has = !g_graph_file_missing;
    // Left by a graph before: a new read starts them again.
    *(int*)(uintptr_t)0x002C63F4 = 77; AtSpaceGraph::s_itemsInUse = 77;
    *(int*)(uintptr_t)0x002C63F8 = 99; AtSpaceGraph::s_itemsPeak = 99;
    struct Restore { ~Restore() { g_fs->has = true; g_null_graph_target = nullptr; *(void**)(uintptr_t)0x002C63FC = &g_heap_graph; AtSpaceGraph::s_heap = &g_heap_graph; } } restore;
    for (int pass = 0; pass < 2; pass++) {
        bool crashed[2];
        for (int s = 0; s < 2; s++) { crashed[s] = guarded([&] { load_graph(s, run.g[s], run.w[s]->owner); }); g_recording = false; g_fail_alloc = false; g_refusing = 0; }
        eq("crash", crashed[0], crashed[1]);
        check_logs("load");
        if (crashed[0] || crashed[1]) return;
        same_alignment_restored(shift);
        same_graph("loaded", run);
        for (int s = 0; s < 2; s++) { eq("stack freed", run.w[s]->owner->m_modifierStack.m_states != nullptr, 0); }
        g_asg_files += pass == 0;
    }
    // The walk again, by itself.
    for (int s = 0; s < 2; s++) {
        On on(s);
        if (s == 0) at<FnP>(kGraphBuild)(run.g[0], nullptr, run.w[0]->owner);
        else run.g[1]->Build(run.w[1]->owner);
    }
    check_logs("built again");
    same_graph("built again", run);
    {
        Registry r[2] = {run.w[0]->registry, run.w[1]->registry};
        for (int s = 0; s < 2; s++) { register_db(r[s], run.db[s], 100); register_graph(r[s], run.g[s]); }
        start_freed();
        for (int s = 0; s < 2; s++) {
            On on(s);
            if (s == 0) at<Fn0>(kGraphClear)(run.g[0], nullptr);
            else run.g[1]->Clear();
        }
        same_freed("order of freeing", r[0], r[1]);
    }
    check_logs("cleared");
    same_graph("cleared", run);
    for (int s = 0; s < 2; s++) guarded([&] { load_graph(s, run.g[s], run.w[s]->owner); });
    check_logs("reloaded");
    for (int s = 0; s < 2; s++) {
        On on(s);
        if (s == 0) at<Fn0>(kGraphDtor)(run.g[0], nullptr);
        else run.g[1]->~AtSpaceGraph();
    }
    check_logs("destroyed");
}

// ---- written text ------------------------------------------------------------------------------------------------------------------------------
static void run_written_asm() {
    const std::string head = "Modifier[9]\n";
    const std::vector<std::string> bodies = {
        // Each class, whole.
        head + "LightModifier[3]\n{\n LightModifier \"A\" { Light \"A\" \"\" Directional Direction 3.0 0.0 4.0 Priority 3 }\n LightModifier \"B\" { Light \"B\" \"SP\" Point Position 1.5 2.5 3.5 }\n LightModifier \"C\" { Light \"C\" \"\" SpotLight Position -1.5 -2.5 -3.5 Direction 0.0 0.0 0.0 LightAnimation \"C\" \"\" }\n}\n",
        head + "LightModifier[2]\n{\n LightModifier \"A\" { LightAnimation \"A\" \"SP\" }\n LightModifier \"B\" { Point Directional }\n}\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" { Light \"Xmissing\" \"\" }\n}\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" { Light \"A\" \"XS\" }\n}\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" { LightAnimation \"Xmissing\" \"\" Point }\n}\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" { LightAnimation \"A\" \"XS\" }\n}\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" { Window 1 }\n}\n",
        head + "AmbientModifier[2]\n{\n AmbientModifier \"A\" { Color 0.25 0.5 0.75 0.125 Priority -2 }\n AmbientModifier \"B\" { }\n}\n",
        head + "AmbientModifier[1]\n{\n AmbientModifier \"A\" { FieldOfViewY 1.0 }\n}\n",
        head + "PerspectiveModifier[2]\n{\n PerspectiveModifier \"A\" { FieldOfViewY 0.75 Priority 9 }\n PerspectiveModifier \"B\" { }\n}\n",
        head + "PerspectiveModifier[1]\n{\n PerspectiveModifier \"A\" { Color 1.0 }\n}\n",
        head + "ContextConstantModifier[2]\n{\n ContextConstantModifier \"A\" { VertexProcessConstant[2]\n {\n 1.5 2.5 3.5 4.5 7\n 5.5 6.5 7.5 8.5 -7\n }\n PixelProcessConstant[1]\n {\n 0.25 0.5 0.75 1.25 3\n }\n Priority 4 }\n ContextConstantModifier \"B\" { }\n}\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { VertexProcessConstant[0]\n {\n }\n }\n}\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { PixelProcessConstant[0]\n {\n }\n }\n}\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { VertexProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 1\n }\n VertexProcessConstant[1]\n {\n 5.0 6.0 7.0 8.0 2\n }\n }\n}\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { PixelProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 1\n }\n PixelProcessConstant[1]\n {\n 5.0 6.0 7.0 8.0 2\n }\n }\n}\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { Window }\n}\n",
        head + "AmbientModifier[2]\n{\n AmbientModifier \"A\" { }\n AmbientModifier \"B\" { }\n}\nGroupModifier[2]\n{\n GroupModifier \"G\" { Modifier[2]\n {\n \"A\" \"\"\n \"B\" \"\"\n }\n Priority 5 }\n GroupModifier \"H\" { Modifier[1]\n {\n \"G\" \"\"\n }\n }\n}\n",
        head + "GroupModifier[1]\n{\n GroupModifier \"G\" { Modifier[1]\n {\n \"Xmissing\" \"\"\n }\n }\n}\n",
        head + "GroupModifier[1]\n{\n GroupModifier \"G\" { Modifier[1]\n {\n \"G\" \"XS\"\n }\n }\n}\n",
        head + "AmbientModifier[1]\n{\n AmbientModifier \"A\" { }\n}\nGroupModifier[1]\n{\n GroupModifier \"G\" { Modifier[2]\n {\n \"A\" \"\"\n \"A\" \"\"\n }\n Modifier[3]\n {\n \"A\" \"\"\n \"A\" \"\"\n \"A\" \"\"\n }\n Modifier[1]\n {\n \"A\" \"\"\n }\n }\n}\n",
        head + "GroupModifier[1]\n{\n GroupModifier \"G\" { Modifier[0]\n {\n }\n Priority 2 }\n}\n",
        head + "GroupModifier[1]\n{\n GroupModifier \"G\" { Color 1.0 }\n}\n",
        head + "BlockModifier[5]\n{\n BlockModifier \"A\" { Block LightModifier }\n BlockModifier \"B\" { Block AmbientModifier Priority 2 }\n BlockModifier \"C\" { Block PerspectiveModifier }\n BlockModifier \"D\" { Block ContextConstantModifier }\n BlockModifier \"E\" { Block GroupModifier }\n}\n",
        head + "BlockModifier[1]\n{\n BlockModifier \"A\" { Block LightModifier Block Window }\n}\n",
        head + "BlockModifier[1]\n{\n BlockModifier \"A\" { Color 1.0 }\n}\n",
        // A name space, and the word after it.
        "NameSpace \"SPACE\"\nModifier[2]\nAmbientModifier[1]\n{\n AmbientModifier \"A\" { }\n}\n",
        "NameSpace \"LONGERTHANEIGHT\"\nModifier[2]\nAmbientModifier[1]\n{\n AmbientModifier \"A\" { }\n}\n",
        "NameSpace \"SPACE\"\nWindow[2]\n",
        "Window[2]\nAmbientModifier[1]\n{\n AmbientModifier \"A\" { }\n}\n",
        // The head.
        "", "Modifier", "Modifier[", "Modifier[0]\n", "Modifier 2\n", "Modifier[2", "Modifier[2]\n", "NameSpace", "NameSpace 3\n", "Modifier[1.5]\n",
        // Top level.
        head + "Window[1]\n{\n}\n",
        head + "BlockModifier[1]\n{\n BlockModifier \"A\" { }\n}\nWindow\n",
        // Lists twice, of each class, and of none.
        head + "LightModifier[1]\n{\n LightModifier \"A\" { }\n}\nLightModifier[1]\n{\n LightModifier \"B\" { }\n}\n",
        head + "AmbientModifier[1]\n{\n AmbientModifier \"A\" { }\n}\nAmbientModifier[1]\n{\n AmbientModifier \"B\" { }\n}\n",
        head + "PerspectiveModifier[1]\n{\n PerspectiveModifier \"A\" { }\n}\nPerspectiveModifier[1]\n{\n PerspectiveModifier \"B\" { }\n}\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { }\n}\nContextConstantModifier[1]\n{\n ContextConstantModifier \"B\" { }\n}\n",
        head + "GroupModifier[1]\n{\n GroupModifier \"A\" { }\n}\nGroupModifier[1]\n{\n GroupModifier \"B\" { }\n}\n",
        head + "BlockModifier[1]\n{\n BlockModifier \"A\" { }\n}\nBlockModifier[1]\n{\n BlockModifier \"B\" { }\n}\n",
        head + "LightModifier[0]\n{\n}\nAmbientModifier[0]\n{\n}\nPerspectiveModifier[0]\n{\n}\nContextConstantModifier[0]\n{\n}\nGroupModifier[0]\n{\n}\nBlockModifier[0]\n{\n}\n",
        // All six at once, for the order they are freed in.
        head + "LightModifier[1]\n{\n LightModifier \"A\" { }\n}\nAmbientModifier[1]\n{\n AmbientModifier \"B\" { }\n}\nPerspectiveModifier[1]\n{\n PerspectiveModifier \"C\" { }\n}\nContextConstantModifier[1]\n{\n ContextConstantModifier \"D\" { }\n}\nGroupModifier[1]\n{\n GroupModifier \"E\" { }\n}\nBlockModifier[1]\n{\n BlockModifier \"F\" { }\n}\n",
        // More names than the head says, for the map to grow; a duplicate; a long name.
        "Modifier[1]\nAmbientModifier[6]\n{\n AmbientModifier \"A\" { }\n AmbientModifier \"B\" { }\n AmbientModifier \"C\" { }\n AmbientModifier \"A\" { }\n AmbientModifier \"LongerThanEight\" { }\n AmbientModifier \"LongerThanNine\" { }\n}\n",
        // Wrong words where a class's word goes, and files that end in the middle.
        head + "LightModifier[1]\n{\n AmbientModifier \"A\" { }\n}\n",
        head + "AmbientModifier[1]\n{\n LightModifier \"A\" { }\n}\n",
        head + "PerspectiveModifier[1]\n{\n LightModifier \"A\" { }\n}\n",
        head + "ContextConstantModifier[1]\n{\n LightModifier \"A\" { }\n}\n",
        head + "GroupModifier[1]\n{\n LightModifier \"A\" { }\n}\n",
        head + "BlockModifier[1]\n{\n LightModifier \"A\" { }\n}\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" {\n",
        head + "AmbientModifier[1]\n{\n AmbientModifier \"A\" {\n",
        head + "PerspectiveModifier[1]\n{\n PerspectiveModifier \"A\" {\n",
        head + "ContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" {\n",
        head + "GroupModifier[1]\n{\n GroupModifier \"A\" {\n",
        head + "BlockModifier[1]\n{\n BlockModifier \"A\" {\n",
        head + "LightModifier[1]\n{\n LightModifier \"A\" { Priority 1.5 }\n}\n",
    };
    for (const std::string& t : bodies) {
        run_asm(t, t, 0xCD, 2);
    }
    run_asm(bodies[0], "fill 0, aligned 7", 0, 7);
    run_asm(bodies[17], "fill 5A, aligned 7", 0x5A, 7);
    run_asm(bodies[11], "aligned 5", 0xCD, 5);
}

static const char kAsgHead[] = "SpaceGraph\n{\n";

static std::string leaves(int n, const std::string& body = "") {
    std::string t = "LeafSpace[" + std::to_string(n) + "]\n{\n";
    for (int i = 0; i < n; i++) t += " LeafSpace { BoundingBox 0.0 8.0 0.0 1.0 0.0 1.0 " + body + " }\n";
    return t + "}\n";
}

static void run_written_asg() {
    const std::string h = kAsgHead;
    const std::vector<std::string> bodies = {
        // The smallest; each count.
        h + "LeafSpace[1]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nNodeLink[3]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nNodeLink[3]\nNodeIncidence[3]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nNodeLink[3]\nNodeIncidence[4]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nNodeIncidence[9000]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        // No leaves, and a word that is no count.
        h + "Root LeafSpace 0\n}\n",
        h + "LeafSpace[0]\nRoot LeafSpace 0\n}\n",
        h + "Window[1]\nLeafSpace[1]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nCell[7]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nNodeLink[3]\nPartition[7]\nRoot LeafSpace 0\n" + leaves(1) + "}\n",
        // The file's head.
        "", "Window", "SpaceGraph", "SpaceGraph\n", "SpaceGraph\n{\n", "SpaceGraph\n{\nLeafSpace", "SpaceGraph\n{\nLeafSpace[1]\n", "SpaceGraph\n{\nLeafSpace[1]\nRoot", "SpaceGraph\n{\nLeafSpace[1]\nRoot LeafSpace", "SpaceGraph\n{\nLeafSpace[1]\nRoot LeafSpace 0\n", "SpaceGraph\n[\n",
        // The root: each class, past the end, a wrong word.
        h + "LeafSpace[1]\nRoot LeafSpace 1\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nRoot Window 0\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nBinarySpace[1]\nRoot BinarySpace 1\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nXBinarySpace[1]\nRoot XBinarySpace 1\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nYBinarySpace[1]\nRoot YBinarySpace 1\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nZBinarySpace[1]\nRoot ZBinarySpace 1\n" + leaves(1) + "}\n",
        // A tree of each class with every word.
        h + "BinarySpace[1]\nLeafSpace[2]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Partition 0.5 0.25 0.125 -2.5 BoundingBox 1.0 2.0 3.0 4.0 5.0 6.0 PosChild LeafSpace 0 NegChild LeafSpace 1 Modifier \"ModA\" \"\" Draw \"Node1\" \"\" LineSegIntersect \"Node2\" \"\" }\n}\n" + leaves(2) + "}\n",
        h + "XBinarySpace[1]\nLeafSpace[2]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Partition 2.5 PosChild LeafSpace 0 NegChild LeafSpace 1 Modifier \"ModA\" \"\" Draw \"Node1\" \"\" LineSegIntersect \"Node2\" \"\" }\n}\n" + leaves(2) + "}\n",
        h + "YBinarySpace[1]\nLeafSpace[2]\nRoot YBinarySpace 0\nYBinarySpace[1]\n{\n YBinarySpace { Partition 2.5 PosChild LeafSpace 0 NegChild LeafSpace 1 Modifier \"ModB\" \"\" Draw \"Node1\" \"\" LineSegIntersect \"Node2\" \"\" }\n}\n" + leaves(2) + "}\n",
        h + "ZBinarySpace[1]\nLeafSpace[2]\nRoot ZBinarySpace 0\nZBinarySpace[1]\n{\n ZBinarySpace { Partition 2.5 PosChild LeafSpace 0 NegChild LeafSpace 1 Modifier \"ModC\" \"\" Draw \"Node1\" \"\" LineSegIntersect \"Node2\" \"\" }\n}\n" + leaves(2) + "}\n",
        // Only one child, or none.
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Partition 2.5 PosChild LeafSpace 0 Draw \"Node1\" \"\" }\n}\n" + leaves(1) + "}\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Partition 2.5 NegChild LeafSpace 0 Draw \"Node1\" \"\" }\n}\n" + leaves(1) + "}\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Partition 2.5 Draw \"Node1\" \"\" }\n}\n" + leaves(1) + "}\n",
        // Nested modifiers, and what each class of modifier does to a leaf's state.
        h + "BinarySpace[3]\nLeafSpace[4]\nRoot BinarySpace 0\nBinarySpace[3]\n{\n BinarySpace { PosChild BinarySpace 1 NegChild BinarySpace 2 Modifier \"ModA\" \"\" }\n BinarySpace { PosChild LeafSpace 0 NegChild LeafSpace 1 Modifier \"ModB\" \"\" }\n BinarySpace { PosChild LeafSpace 2 NegChild LeafSpace 3 Modifier \"ModC\" \"\" }\n}\nLeafSpace[4]\n{\n LeafSpace { Modifier \"ModD\" \"\" }\n LeafSpace { Modifier \"ModE\" \"\" }\n LeafSpace { Modifier \"ModF\" \"\" }\n LeafSpace { }\n}\n}\n",
        h + "BinarySpace[8]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[8]\n{\n BinarySpace { PosChild BinarySpace 1 Modifier \"ModA\" \"\" }\n BinarySpace { PosChild BinarySpace 2 Modifier \"ModG\" \"\" }\n BinarySpace { PosChild BinarySpace 3 Modifier \"ModM\" \"\" }\n BinarySpace { PosChild BinarySpace 4 Modifier \"ModS\" \"\" }\n BinarySpace { PosChild BinarySpace 5 Modifier \"ModY\" \"\" }\n BinarySpace { PosChild BinarySpace 6 Modifier \"Mode\" \"\" }\n BinarySpace { PosChild BinarySpace 7 Modifier \"Modk\" \"\" }\n BinarySpace { PosChild LeafSpace 0 Modifier \"Modq\" \"\" }\n}\n" + leaves(1) + "}\n",
        // Name spaces and names not found, in all classes.
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Draw \"Node1\" \"SP\" LineSegIntersect \"Node2\" \"SP\" Cell \"Tab\" \"SP\" \"Cell\" Modifier \"NsMod\" \"SP\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Draw \"Xn\" \"\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { LineSegIntersect \"Xn\" \"\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Modifier \"Xn\" \"\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Draw \"Node1\" \"XS\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { LineSegIntersect \"Node1\" \"XS\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Modifier \"Node1\" \"XS\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Cell \"Tab\" \"\" \"Xmissing\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Cell \"Xmiss\" \"\" \"Cell\" }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Draw \"Quarrel\" \"\" }\n}\n}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Draw \"Quarrel\" \"\" PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { LineSegIntersect \"Quarrel\" \"\" }\n}\n}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Draw \"Xn\" \"\" PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { LineSegIntersect \"Xn\" \"\" PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Modifier \"Xn\" \"\" PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        // A leaf's longest side, every order, and ties.
        h + "LeafSpace[8]\nRoot LeafSpace 0\nLeafSpace[8]\n{\n LeafSpace { BoundingBox 0.0 9.0 0.0 1.0 0.0 1.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 9.0 0.0 1.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 1.0 0.0 9.0 }\n LeafSpace { BoundingBox 0.0 9.0 0.0 9.0 0.0 1.0 }\n LeafSpace { BoundingBox 0.0 9.0 0.0 1.0 0.0 9.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 9.0 0.0 9.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 1.0 0.0 1.0 }\n LeafSpace { BoundingBox 9.0 0.0 9.0 0.0 9.0 0.0 }\n}\n}\n",
        // Words in the wrong class.
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { PotentiallyVisibleSet }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Collide }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Partition 1.0 }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Root }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { PosChild LeafSpace 0 }\n}\n}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Cell PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Collide PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Cell PosChild LeafSpace 0 }\n}\n" + leaves(1) + "}\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { PosChild LeafSpace 3 NegChild Window 0 }\n}\n" + leaves(1) + "}\n",
        // Lists: longer than their count, in pieces, an unknown word, ending early.
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[0]\n{\n}\nWindow[0]\n{\n}\n}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nBinarySpace[2]\n{\n BinarySpace { }\n}\n}\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nXBinarySpace[2]\n{\n XBinarySpace { }\n}\n}\n",
        h + "YBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nYBinarySpace[2]\n{\n YBinarySpace { }\n}\n}\n",
        h + "ZBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nZBinarySpace[2]\n{\n ZBinarySpace { }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[2]\n{\n LeafSpace { }\n}\n}\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nBinarySpace[1]\n{\n LeafSpace { }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n BinarySpace { }\n}\n}\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nXBinarySpace[1]\n{\n LeafSpace { }\n}\n}\n",
        h + "YBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nYBinarySpace[1]\n{\n XBinarySpace { }\n}\n}\n",
        h + "ZBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nZBinarySpace[1]\n{\n XBinarySpace { }\n}\n}\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace {\n",
        h + "BinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nBinarySpace[1]\n{\n BinarySpace {\n",
        h + "XBinarySpace[1]\nLeafSpace[1]\nRoot LeafSpace 0\nXBinarySpace[1]\n{\n XBinarySpace {\n",
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { BoundingBox 1.0\n",
        // Running out of links and items, reading: leaves take theirs at once, the rest after everything is read. Many draws, each one a link and an item.
        h + "LeafSpace[1]\nNodeLink[2]\nNodeIncidence[3]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Draw \"NodeA\" \"\" Draw \"NodeB\" \"\" Draw \"NodeC\" \"\" Draw \"NodeD\" \"\" Draw \"NodeE\" \"\" LineSegIntersect \"NodeA\" \"\" LineSegIntersect \"NodeF\" \"\" }\n}\n}\n",
        h + "XBinarySpace[1]\nLeafSpace[2]\nNodeLink[2]\nNodeIncidence[3]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Partition 0.0 PosChild LeafSpace 0 NegChild LeafSpace 1 Draw \"NodeA\" \"\" Draw \"NodeB\" \"\" Draw \"NodeC\" \"\" Draw \"NodeD\" \"\" }\n}\n" + leaves(2) + "}\n",
        // Moved nodes: the moved flag asks for the world sphere first.
        h + "LeafSpace[1]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { Draw \"MoverA\" \"\" LineSegIntersect \"MoverB\" \"\" Cell \"Tab\" \"\" \"Cell\" }\n}\n}\n",
    };
    for (const std::string& t : bodies) run_asg(t, t, 0xCD, 2);
    run_asg(bodies[0], "fill 0, aligned 7", 0, 7);
    run_asg(bodies[23], "fill 5A, aligned 7", 0x5A, 7);
    run_asg(bodies[33], "aligned 5", 0xCD, 5);
    // Fewer slots than kinds: the slots' checks.
    for (unsigned slots : {0u, 1u, 3u, 5u}) run_asg(bodies[33], "slots " + std::to_string(slots), 0xCD, 2, 32, slots);
    // Depth, including none, which makes an array with no element and nothing to walk on.
    run_asg(bodies[33], "depth 8", 0xCD, 2, 8, 6);
}

// ---- running out of each heap -------------------------------------------------------------------------------------------------------------------
static void run_shortages() {
    const std::string db_text = "Modifier[9]\nLightModifier[1]\n{\n LightModifier \"A\" { }\n}\nAmbientModifier[1]\n{\n AmbientModifier \"B\" { }\n}\nPerspectiveModifier[1]\n{\n PerspectiveModifier \"C\" { }\n}\nContextConstantModifier[1]\n{\n ContextConstantModifier \"D\" { VertexProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 1\n }\n }\n}\nGroupModifier[1]\n{\n GroupModifier \"E\" { Modifier[1]\n {\n \"A\" \"\"\n }\n }\n}\nBlockModifier[1]\n{\n BlockModifier \"F\" { }\n}\n";
    for (FakeHeap* h : {&g_heap_db, &g_heap_context, &g_heap_group}) {
        g_refuse_heap = h;
        run_asm(db_text, "no memory, modifiers", 0xCD, 2);
        g_refuse_heap = nullptr;
    }
    // One class's list only: a heap is refused from the Nth push on.
    const std::vector<std::string> onlies = {
        "Modifier[9]\nLightModifier[1]\n{\n LightModifier \"A\" { }\n}\n", "Modifier[9]\nAmbientModifier[1]\n{\n AmbientModifier \"A\" { }\n}\n", "Modifier[9]\nPerspectiveModifier[1]\n{\n PerspectiveModifier \"A\" { }\n}\n",
        "Modifier[9]\nContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { }\n}\n", "Modifier[9]\nGroupModifier[1]\n{\n GroupModifier \"A\" { }\n}\n", "Modifier[9]\nBlockModifier[1]\n{\n BlockModifier \"A\" { }\n}\n",
        "Modifier[9]\nContextConstantModifier[1]\n{\n ContextConstantModifier \"A\" { PixelProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 1\n }\n }\n}\n"};
    for (const std::string& t : onlies) for (FakeHeap* h : {&g_heap_db, &g_heap_context}) {
        g_refuse_heap = h;
        run_asm(t, "no memory: " + t, 0xCD, 2);
        g_refuse_heap = nullptr;
    }
    const std::string g = std::string(kAsgHead) + "BinarySpace[1]\nXBinarySpace[1]\nYBinarySpace[1]\nZBinarySpace[1]\nLeafSpace[1]\nNodeLink[2]\nRoot LeafSpace 0\nBinarySpace[1]\n{\n BinarySpace { Draw \"NodeA\" \"\" }\n}\n" + leaves(1, "Modifier \"ModA\" \"\"") + "}\n";
    for (FakeHeap* h : {&g_heap_graph, &g_heap_stack, &g_heap_state, &g_heap_file}) {
        g_refuse_for_graph = h;
        run_asg(g, "no memory, graph", 0xCD, 2);
        g_refuse_for_graph = nullptr;
        g_refuse_heap = nullptr;
    }
    // Each allocation of the graph's in turn: links, items, then the lists.
    for (int skip = 0; skip < 8; skip++) {
        const std::string t = std::string(kAsgHead) + "BinarySpace[1]\nXBinarySpace[1]\nYBinarySpace[1]\nZBinarySpace[1]\nLeafSpace[1]\nNodeLink[2]\nRoot LeafSpace 0\n}\n";
        g_refuse_for_graph = &g_heap_graph;
        g_refuse_for_graph_skip = skip;
        run_asg(t, "no memory, allocation " + std::to_string(skip), 0xCD, 2);
        g_refuse_for_graph = nullptr;
        g_refuse_for_graph_skip = 0;
        g_refuse_heap = nullptr;
    }
    // The graph's heap going null after its Nth use: after the links, before the items; after the X spaces, before the Y; after the Y, before the Z.
    const struct { const char* first; int after; } nulls[] = {{"LeafSpace[1]\n", 1}, {"XBinarySpace[1]\nYBinarySpace[1]\nLeafSpace[1]\n", 3}, {"YBinarySpace[1]\nZBinarySpace[1]\nLeafSpace[1]\n", 3}};
    for (const auto& n : nulls) {
        const std::string t = std::string(kAsgHead) + n.first + "NodeLink[2]\nRoot LeafSpace 0\n}\n";
        g_graph_heap_null_after = n.after;
        run_asg(t, "graph's heap going null: " + t, 0xCD, 2);
        g_graph_heap_null_after = 0;
    }
    // Files that are not there.
    g_graph_file_missing = true;
    run_asg(g, "no file", 0xCD, 2);
    g_graph_file_missing = false;
    g_fs->has = false;
    run_asm(db_text, "no file", 0xCD, 2);
    g_fs->has = true;
}

// The graph's heap is null only while the graph loads, since the modifiers it names need none of it. It crashes at once on both sides, having been aligned first.
static void run_asg_after_modifiers(const std::string& text, uint32_t, void**) {
    run_asg(text, "null graph heap", 0xCD, 2);
}

// A heap that is null, where the original checks for that. Each one's own raise is logged; then each crashes, or does not, on both sides.
static void run_no_heaps() {
    struct Null { uint32_t va; void** recovered; bool graph; };
    static std::string asm_all = "Modifier[9]\nLightModifier[1]\n{\n LightModifier \"A\" { }\n}\nContextConstantModifier[1]\n{\n ContextConstantModifier \"D\" { VertexProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 1\n }\n PixelProcessConstant[1]\n {\n 1.0 2.0 3.0 4.0 1\n }\n }\n}\nGroupModifier[1]\n{\n GroupModifier \"E\" { Modifier[1]\n {\n \"A\" \"\"\n }\n }\n}\n";
    static std::string asg_all = std::string(kAsgHead) + "XBinarySpace[1]\nYBinarySpace[1]\nZBinarySpace[1]\nLeafSpace[1]\nNodeLink[2]\nRoot LeafSpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Draw \"NodeA\" \"\" }\n}\n" + leaves(1) + "}\n";
    const Null cases[] = {
        {0x002C6410, &AtModifierDatabase::s_heap, false},
        {0x002C7E1C, &AtModifier::s_heap, false},
        {0x002C7E20, &AtGroupModifier::s_heap, false},
        {0x002C63FC, &AtSpaceGraph::s_heap, true},
    };
    for (const Null& n : cases) {
        void* saved = *(void**)(uintptr_t)n.va;
        *(void**)(uintptr_t)n.va = nullptr;
        *n.recovered = nullptr;
        if (n.graph) run_asg_after_modifiers(asg_all, n.va, n.recovered);
        else run_asm(asm_all, "null heap", 0xCD, 2);
        *(void**)(uintptr_t)n.va = saved;
        *n.recovered = saved;
    }
    // A pending entry with no file heap, and the file's own code asking the same heap: it takes the null it is given, here.
    void* saved = *(void**)(uintptr_t)0x002C60B0;
    *(void**)(uintptr_t)0x002C60B0 = nullptr;
    AtIoTokenFile::s_heap = nullptr;
    run_asg(asg_all, "null file heap", 0xCD, 2);
    *(void**)(uintptr_t)0x002C60B0 = saved;
    AtIoTokenFile::s_heap = saved;
}

// ---- driven directly -----------------------------------------------------------------------------------------------------------------------------
// One class of modifier asked for each slot, with a spy for a member, so what a group passes on is seen.
class SpyModifier : public AtModifier {
public:
    void Apply(AtModifierState*, int data) override { if (g_recording) g_log[g_side].add('a', data); }
    void Slot2(void* a, void* b, void* c, void* d, void* e) override { if (g_recording) g_log[g_side].add('2', (uint32_t)(uintptr_t)a + (uint32_t)(uintptr_t)b * 3 + (uint32_t)(uintptr_t)c * 5 + (uint32_t)(uintptr_t)d * 7 + (uint32_t)(uintptr_t)e * 11); }
    void Slot3(void* a, void* b, void* c, void* d) override { if (g_recording) g_log[g_side].add('3', (uint32_t)(uintptr_t)a + (uint32_t)(uintptr_t)b * 3 + (uint32_t)(uintptr_t)c * 5 + (uint32_t)(uintptr_t)d * 7); }
    void Slot5(void* a) override { if (g_recording) g_log[g_side].add('5', (uint32_t)(uintptr_t)a); }
};

static void run_group_slots() {
    g_context = "group's slots";
    AtGroupModifier* group[2];
    SpyModifier spies[2][3];
    AtModifier* members[2][3];
    for (int s = 0; s < 2; s++) {
        void* p = std::malloc(sizeof(AtGroupModifier));
        { On on(s); if (s == 0) at<Fn0>(0x0004FD50)(p, nullptr); else new (p) AtGroupModifier; }
        group[s] = (AtGroupModifier*)p;
        for (int i = 0; i < 3; i++) members[s][i] = &spies[s][i];
        group[s]->m_members = members[s];
    }
    for (unsigned count : {0u, 1u, 3u}) {
        for (int s = 0; s < 2; s++) {
            group[s]->m_count = count;
            On on(s);
            void** table = *(void***)group[s];
            ((void(__thiscall*)(void*, void*, void*, void*, void*, void*))table[2])(group[s], (void*)1, (void*)2, (void*)3, (void*)4, (void*)5);
            ((void(__thiscall*)(void*, void*, void*, void*, void*))table[3])(group[s], (void*)6, (void*)7, (void*)8, (void*)9);
            ((void(__thiscall*)(void*, void*))table[4])(group[s], (void*)10);
            ((void(__thiscall*)(void*, void*))table[5])(group[s], (void*)11);
            ((void(__thiscall*)(void*, void*, int))table[1])(group[s], nullptr, 12);
            AtModifier* got;
            if (s == 0) got = at<AtModifier*(__fastcall*)(void*, void*, unsigned)>(0x0004FD30)(group[s], nullptr, count ? count - 1 : 0);
            else got = group[s]->MemberAt(count ? count - 1 : 0);
            g_log[s].add('m', got ? (uint32_t)((SpyModifier*)got - spies[s]) + 1 : 0);
        }
        compare_logs("group's slots");
    }
    // Disabled: Apply passes nothing on.
    for (int s = 0; s < 2; s++) {
        group[s]->m_count = 3;
        group[s]->m_flags |= AtModifier::kDisabled;
        On on(s);
        void** table = *(void***)group[s];
        ((void(__thiscall*)(void*, void*, int))table[1])(group[s], nullptr, 13);
    }
    compare_logs("group disabled");
}

// A modifier state and each way into it, with modifiers of each class made and destroyed by both sides.
static long long spy_id(const AtModifier* m, AtLightModifier* lights, AtAmbientModifier* ambient, AtBlockModifier* block) {
    if (!m) return 0;
    if ((const void*)m >= lights && (const void*)m < lights + 12) return 100 + ((const AtLightModifier*)m - lights);
    if ((const void*)m >= ambient && (const void*)m < ambient + 6) return 200 + ((const AtAmbientModifier*)m - ambient);
    if ((const void*)m >= block && (const void*)m < block + 6) return 300 + ((const AtBlockModifier*)m - block);
    return -1;
}

typedef void(__fastcall* FnStateInit)(void*, void*, unsigned, int);
typedef void(__thiscall* FnApply)(void*, void*, int);

static void same_state(const char* what, AtModifierState* const* s, AtLightModifier* const* l, AtAmbientModifier* const* a, AtBlockModifier* const* b) {
    eq(what, s[0]->m_count, s[1]->m_count);
    // A slot past the end is the heap's room, full of what the heap filled it with, unless something wrote there.
    eq(what, std::memcmp(s[0]->m_slots + s[0]->m_count, s[1]->m_slots + s[1]->m_count, sizeof(AtModifierSlot)), 0);
    for (unsigned i = 0; i < s[0]->m_count && i < s[1]->m_count; i++) {
        eq(what, s[0]->m_slots[i].m_count, s[1]->m_slots[i].m_count);
        for (int j = 0; j < AtModifierSlot::kMaxEntries; j++) {
            eq(what, spy_id(s[0]->m_slots[i].m_entries[j].m_modifier, l[0], a[0], b[0]), spy_id(s[1]->m_slots[i].m_entries[j].m_modifier, l[1], a[1], b[1]));
            eq(what, s[0]->m_slots[i].m_entries[j].m_data, s[1]->m_slots[i].m_entries[j].m_data);
        }
    }
}

static void run_states(unsigned seed) {
    g_context = "states, seed " + std::to_string(seed);
    std::mt19937 rng(seed);
    set_alignment(2);
    g_fill = 0xCD;
    AtLightModifier* lights[2];
    AtAmbientModifier* ambient[2];
    AtBlockModifier* block[2];
    AtModifierState* states[2];
    AtModifierState* copies[2];
    for (int s = 0; s < 2; s++) {
        On on(s);
        lights[s] = (AtLightModifier*)std::malloc(sizeof(AtLightModifier) * 12);
        ambient[s] = (AtAmbientModifier*)std::malloc(sizeof(AtAmbientModifier) * 6);
        block[s] = (AtBlockModifier*)std::malloc(sizeof(AtBlockModifier) * 6);
        for (int i = 0; i < 12; i++) { if (s == 0) at<Fn0>(0x00050140)(&lights[s][i], nullptr); else new (&lights[s][i]) AtLightModifier; }
        for (int i = 0; i < 6; i++) { if (s == 0) at<Fn0>(0x000506B0)(&ambient[s][i], nullptr); else new (&ambient[s][i]) AtAmbientModifier; }
        for (int i = 0; i < 6; i++) { if (s == 0) at<Fn0>(0x0004FFA0)(&block[s][i], nullptr); else new (&block[s][i]) AtBlockModifier; }
        states[s] = (AtModifierState*)std::calloc(1, sizeof(AtModifierState));
        copies[s] = (AtModifierState*)std::calloc(1, sizeof(AtModifierState));
    }
    unsigned count = 2 + rng() % 5;
    for (int s = 0; s < 2; s++) {
        On on(s);
        if (s == 0) at<FnStateInit>(0x000535A0)(states[0], nullptr, count, seed & 1);
        else states[1]->Init(count, seed & 1);
    }
    compare_logs("state init");
    for (int op = 0; op < 400; op++) {
        const unsigned what = rng() % 8;
        const unsigned i = rng();
        const int priority = (int)(rng() % 5) - 2;
        const int data = (int)(rng() % 1000);
        const unsigned flags = rng() % 4 == 0 ? AtModifier::kDisabled : 0;
        // In the later seeds every light goes to one slot, which fills.
        const int kind = seed > 4 ? 0 : (int)(rng() % 8) - 1;
        for (int s = 0; s < 2; s++) {
            AtModifier* m;
            if (what <= 2) { m = &lights[s][i % 12]; m->m_kind = kind < 0 ? 0 : kind; }
            else if (what <= 4) { m = &ambient[s][i % 6]; m->m_kind = kind; }
            else if (what == 5) { m = &block[s][i % 6]; m->m_kind = 5; block[s][i % 6].m_blocks = kind; }
            else m = nullptr;
            On on(s);
            if (m) {
                m->m_priority = priority;
                m->m_flags = flags;
                void** table = *(void***)m;
                ((FnApply)table[1])(m, states[s], data);
            } else if (what == 6) {
                if (s == 0) at<FnP>(0x00053720)(copies[0], nullptr, states[0]);
                else copies[1]->Assign(*states[1]);
            } else if (seed > 4) {
                // Nothing: the slots stay full.
            } else if (op % 16 == 7) {
                if (s == 0) at<FnStateInit>(0x000535A0)(states[0], nullptr, count, seed & 1);
                else states[1]->Init(count, seed & 1);
            } else {
                // A slot counted empty, whose first entry is still there.
                states[s]->m_slots[i % count].m_count = 0;
            }
        }
        same_state("state", states, lights, ambient, block);
        if (what == 6) { same_state("copy", copies, lights, ambient, block); }
    }
    compare_logs("state ops");
    same_alignment_restored(2);
}

// Modifiers' small functions by themselves.
static void run_small() {
    g_context = "small";
    // Keywords, against the table in the image.
    for (int t = 0; t < 2; t++) {
        const AtIoKeyword* o = (const AtIoKeyword*)(uintptr_t)(t ? 0x002B1800 : 0x002B1D40);
        const AtIoKeyword* r = t ? AtSpaceGraph::s_keywords : AtModifierDatabase::s_keywords;
        const int n = t ? AtSpaceGraph::kKeywordCount : AtModifierDatabase::kKeywordCount;
        for (int i = 0; i < n; i++) { eq("keyword id", o[i].m_id, r[i].m_id); eqs("keyword name", o[i].m_name, r[i].m_name); }
    }
    // Heaps' setters.
    struct Setter { uint32_t fn; uint32_t var; void (*recovered)(void*); void** value; };
    const Setter setters[] = {
        {0x0003E920, 0x002C6410, AtModifierDatabase::SetHeap, &AtModifierDatabase::s_heap}, {0x0004F900, 0x002C7E1C, AtModifier::SetHeap, &AtModifier::s_heap}, {0x0004FC50, 0x002C7E20, AtGroupModifier::SetHeap, &AtGroupModifier::s_heap},
        {0x00039AF0, 0x002C63FC, AtSpaceGraph::SetHeap, &AtSpaceGraph::s_heap}, {0x00046820, 0x002C7E04, AtModifierStack::SetHeap, &AtModifierStack::s_heap}};
    for (const Setter& s : setters) {
        void* saved = *(void**)(uintptr_t)s.var;
        at<FnHeap>(s.fn)((void*)0x1234);
        s.recovered((void*)0x1234);
        eq("set", (long long)(uintptr_t) * (void**)(uintptr_t)s.var, (long long)(uintptr_t)*s.value);
        at<FnHeap>(s.fn)(saved);
        s.recovered(saved);
    }
    AtModifierState::SetHeap(&g_heap_state);
    // Each modifier's constructor, base and deleting destructor, through the vftable; the base is otherwise abstract in effect.
    {
        unsigned char a[sizeof(AtModifier)], b[sizeof(AtModifier)];
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        at<Fn0>(0x0004F890)(a, nullptr);
        new (b) AtModifier;
        eq("base modifier", std::memcmp(a + 4, b + 4, sizeof(AtModifier) - 4), 0);
        eq("base's table", table_id(0, *(void**)a), table_id(1, *(void**)b));
        // The base's empty slots and its Apply, on a state with one slot.
        AtModifierSlot slots[2][1];
        AtModifierState state[2];
        AtModifier* m[2] = {(AtModifier*)a, (AtModifier*)b};
        for (int s = 0; s < 2; s++) {
            state[s].m_count = 1;
            state[s].m_slots = slots[s];
            m[s]->m_kind = 0;
            On on(s);
            void** table = *(void***)m[s];
            ((void(__thiscall*)(void*, void*, void*, void*, void*, void*))table[2])(m[s], 0, 0, 0, 0, 0);
            ((void(__thiscall*)(void*, void*, void*, void*, void*))table[3])(m[s], 0, 0, 0, 0);
            ((void(__thiscall*)(void*, void*))table[4])(m[s], 0);
            ((void(__thiscall*)(void*, void*))table[5])(m[s], 0);
            ((FnApply)table[1])(m[s], &state[s], 3);
            eq("the base took the slot", slots[s][0].m_entries[0].m_modifier == m[s], 1);
            state[s].m_slots = nullptr;
        }
        // Reset, by itself.
        for (int s = 0; s < 2; s++) {
            m[s]->m_flags = 7; m[s]->m_kind = 3; m[s]->m_priority = 9;
            if (s == 0) at<Fn0>(0x0004F8D0)(m[s], nullptr);
            else m[s]->Reset();
        }
        eq("reset", std::memcmp(a + 4, b + 4, sizeof(AtModifier) - 4), 0);
        for (int s = 0; s < 2; s++) {
            void* heap_block = ::operator new(sizeof(AtModifier));
            std::memset(heap_block, 0, sizeof(AtModifier));
            On on(s);
            if (s == 0) { at<Fn0>(0x0004F890)(heap_block, nullptr); at<void*(__fastcall*)(void*, void*, unsigned)>(0x0004F8B0)(heap_block, nullptr, 1); }
            else { AtModifier* made = new (heap_block) AtModifier; delete made; }
        }
        compare_logs("base deleted");
    }
    // The base of spaces, abstract here and in the original, whose deleting destructor nothing else can reach.
    {
        class Concrete : public AtSpace {
        public:
            void ReadWord(AtNameTableOwner*, AtSpaceGraph*, AtIoTokenizer*, int) override {}
            void Build(AtNameTableOwner*) override {}
            void Slot6() override {}
            void Slot7() override {}
            void Attach(AtSpaceGraph*, unsigned int, AtSceneNode*) override {}
            void Insert(AtSceneLink*, float, AtSceneLinkItem*) override {}
            void Slot10() override {}
            void Slot11() override {}
        };
        for (int s = 0; s < 2; s++) {
            void* block = ::operator new(sizeof(AtSpace));
            On on(s);
            if (s == 0) {
                at<Fn0>(0x00046520)(block, nullptr);
                eq("space's base", ((AtSpace*)block)->m_modifier == nullptr && ((AtSpace*)block)->m_pending == nullptr, 1);
                at<void(__fastcall*)(void*, void*, void*, unsigned, void*)>(0x00046420)(block, nullptr, nullptr, 2, nullptr);
                at<void*(__fastcall*)(void*, void*, unsigned)>(0x00046770)(block, nullptr, 1);
            } else {
                Concrete* made = new (block) Concrete;
                eq("space's base", made->m_modifier == nullptr && made->m_pending == nullptr, 1);
                made->AddPending(nullptr, 2, nullptr);
                delete made;
            }
        }
        compare_logs("space's base");
    }
    // Destructors leave the base reset, in place.
    {
        const uint32_t ctors[2] = {0x0004F910, 0x0004FD50};
        const uint32_t dtors[2] = {0x0004F9B0, 0x0004FDB0};
        for (int k = 0; k < 2; k++) {
            unsigned char a[sizeof(AtGroupModifier) + 8] = {}, b[sizeof(AtGroupModifier) + 8] = {};
            at<Fn0>(ctors[k])(a, nullptr);
            if (k == 0) new (b) AtContextConstantModifier; else new (b) AtGroupModifier;
            for (unsigned char* m : {a, b}) { ((AtModifier*)m)->m_flags = 3; ((AtModifier*)m)->m_kind = 2; ((AtModifier*)m)->m_priority = 7; }
            at<Fn0>(dtors[k])(a, nullptr);
            if (k == 0) ((AtContextConstantModifier*)b)->~AtContextConstantModifier(); else ((AtGroupModifier*)b)->~AtGroupModifier();
            eq("destroyed in place", std::memcmp(a + 4, b + 4, sizeof(AtModifier) - 4), 0);
        }
    }
    // A space with a modifier: what it passes when entered.
    {
        AtNameTableOwner* owners[2];
        SpyModifier spy[2];
        unsigned char mem[2][sizeof(AtLeafSpace)];
        set_alignment(2);
        for (int s = 0; s < 2; s++) {
            owners[s] = new AtNameTableOwner;
            std::memset(&owners[s]->m_modifierStack, 0, sizeof(AtModifierStack));
            On on(s);
            if (s == 0) at<void(__fastcall*)(void*, void*, unsigned, unsigned, int)>(0x000468A0)(&owners[0]->m_modifierStack, nullptr, 4, 2, 0);
            else owners[1]->m_modifierStack.Init(4, 2, 0);
            if (s == 0) at<Fn0>(0x00045750)(mem[0], nullptr);
            else new (mem[1]) AtLeafSpace;
            AtSpace* sp = (AtSpace*)mem[s];
            sp->m_modifier = &spy[s];
            if (s == 0) { at<FnP>(0x000466E0)(sp, nullptr, owners[0]); at<FnP>(0x00046720)(sp, nullptr, owners[0]); }
            else { sp->Enter(owners[1]); sp->Leave(owners[1]); }
        }
        compare_logs("entered");
    }
    // Refused memory, directly, for what a graph's load cannot reach alone.
    {
        typedef void(__fastcall* FnStack)(void*, void*, unsigned, unsigned, int);
        for (int file_heap = 0; file_heap < 2; file_heap++) {
            AtModifierStack stack[2] = {};
            g_refuse_heap = file_heap ? (void*)&g_heap_file : (void*)&g_heap_stack;
            bool crashed[2];
            for (int s = 0; s < 2; s++) {
                crashed[s] = guarded([&] { On on(s); if (s == 0) at<FnStack>(0x000468A0)(&stack[0], nullptr, 2, 2, file_heap); else stack[1].Init(2, 2, file_heap); });
                g_recording = false; g_fail_alloc = false; g_refusing = 0;
            }
            eq("crash", crashed[0], crashed[1]);
            check_logs("a stack refused");
            g_refuse_heap = file_heap ? (void*)&g_heap_file : (void*)&g_heap_state;
            AtModifierState state[2];
            for (int s = 0; s < 2; s++) {
                crashed[s] = guarded([&] { On on(s); if (s == 0) at<FnStateInit>(0x000535A0)(&state[0], nullptr, 2, file_heap); else state[1].Init(2, file_heap); });
                g_recording = false; g_fail_alloc = false; g_refusing = 0;
            }
            eq("crash", crashed[0], crashed[1]);
            check_logs("a state refused");
            g_refuse_heap = nullptr;
            state[0].m_slots = nullptr; state[1].m_slots = nullptr;
            set_alignment(2);
        }
        // A pending entry.
        class Concrete : public AtSpace {
        public:
            void ReadWord(AtNameTableOwner*, AtSpaceGraph*, AtIoTokenizer*, int) override {}
            void Build(AtNameTableOwner*) override {}
            void Slot6() override {}
            void Slot7() override {}
            void Attach(AtSpaceGraph*, unsigned int, AtSceneNode*) override {}
            void Insert(AtSceneLink*, float, AtSceneLinkItem*) override {}
            void Slot10() override {}
            void Slot11() override {}
        };
        unsigned char mem[2][sizeof(Concrete)];
        at<Fn0>(0x00046520)(mem[0], nullptr);
        new (mem[1]) Concrete;
        g_refuse_heap = &g_heap_file;
        bool crashed[2];
        for (int s = 0; s < 2; s++) {
            crashed[s] = guarded([&] {
                On on(s);
                if (s == 0) at<void(__fastcall*)(void*, void*, void*, unsigned, void*)>(0x00046420)(mem[0], nullptr, nullptr, 2, nullptr);
                else ((Concrete*)mem[1])->AddPending(nullptr, 2, nullptr);
            });
            g_recording = false; g_fail_alloc = false; g_refusing = 0;
        }
        g_refuse_heap = nullptr;
        eq("crash", crashed[0], crashed[1]);
        check_logs("a pending entry refused");
    }
    // Spaces: slots 3, 5, 12 and 13.
    {
        struct Kind { uint32_t ctor; size_t size; void* (*make)(void*); };
        const Kind kinds[] = {
            {0x00047870, sizeof(AtBinarySpace), [](void* p) -> void* { return new (p) AtBinarySpace; }}, {0x00047430, sizeof(AtXBinarySpace), [](void* p) -> void* { return new (p) AtXBinarySpace; }},
            {0x00046FA0, sizeof(AtYBinarySpace), [](void* p) -> void* { return new (p) AtYBinarySpace; }}, {0x00046AB0, sizeof(AtZBinarySpace), [](void* p) -> void* { return new (p) AtZBinarySpace; }},
            {0x00045750, sizeof(AtLeafSpace), [](void* p) -> void* { return new (p) AtLeafSpace; }}};
        for (const Kind& k : kinds) {
            std::vector<unsigned char> mem[2];
            AtSpace* sp[2];
            SpyModifier spy[2];
            AtSpaceDesc desc[2] = {};
            for (int s = 0; s < 2; s++) {
                mem[s].assign(k.size, 0xCD);
                On on(s);
                if (s == 0) at<Fn0>(k.ctor)(mem[0].data(), nullptr);
                else k.make(mem[1].data());
                sp[s] = (AtSpace*)mem[s].data();
                desc[s].m_modifier = &spy[s];
                void** table = *(void***)sp[s];
                ((void(__thiscall*)(void*, void*))table[3])(sp[s], &desc[s]);
                eq("slot 3 took it", sp[s]->m_modifier == &spy[s], 1);
                ((void(__thiscall*)(void*, void*))table[12])(sp[s], (void*)21);
                sp[s]->m_modifier = nullptr;
                ((void(__thiscall*)(void*, void*))table[12])(sp[s], (void*)22);
            }
            compare_logs("space slots 3 and 12");
            eq("space constructed", std::memcmp(mem[0].data() + 4, mem[1].data() + 4, k.size - 4), 0);
            if (k.size != sizeof(AtLeafSpace)) {
                float plane[2][4];
                for (int s = 0; s < 2; s++) {
                    std::memset(mem[s].data() + 0x14, 0, k.size - 0x14);
                    *(float*)(mem[s].data() + 0x14) = 2.5f;
                    if (k.size > 0x18) { *(float*)(mem[s].data() + 0x18) = 1.5f; *(float*)(mem[s].data() + 0x1C) = 3.5f; *(float*)(mem[s].data() + 0x20) = 4.5f; }
                    void** table = *(void***)sp[s];
                    ((void(__thiscall*)(void*, float*))table[13])(sp[s], plane[s]);
                }
                eq("plane", std::memcmp(plane[0], plane[1], sizeof plane[0]), 0);
            }
            // Slot 5, from what a read would leave.
            for (int s = 0; s < 2; s++) {
                std::memset(mem[s].data() + 4, 0x11, k.size - 4);
                *(void**)(mem[s].data() + 8) = nullptr;
                if (k.size == sizeof(AtLeafSpace)) { *(void**)(mem[s].data() + 0x24) = nullptr; *(unsigned*)(mem[s].data() + 0x28) = 0; }
                On on(s);
                void** table = *(void***)sp[s];
                ((void(__thiscall*)(void*))table[5])(sp[s]);
            }
            compare_logs("space slot 5");
            eq("cleared", std::memcmp(mem[0].data() + 4, mem[1].data() + 4, k.size - 4), 0);
        }
    }
}

// Links, by every door: the graph's add and remove functions and slot 9 of every class, on trees of each. Small pools run out.
typedef void(__fastcall* FnNode)(void*, void*, void*);

static void run_links(unsigned seed, const std::string& text, unsigned ops, unsigned least_radius = 0, float tie_x = 0, float tie_y = 0) {
    g_context = "links, seed " + std::to_string(seed);
    GraphRun run;
    std::set<std::string> extra;
    std::mt19937 rng(seed);
    std::string names;
    for (int i = 0; i < 24; i++) names += " \"Node" + std::to_string(i) + "\"";
    const std::string all = text + "//" + names;
    if (!prepare_graph(run, all, 32, 6, 0xCD, 2)) return;
    set_text(text);
    bool crashed[2];
    for (int s = 0; s < 2; s++) { crashed[s] = guarded([&] { load_graph(s, run.g[s], run.w[s]->owner); }); g_recording = false; }
    eq("crash", crashed[0], crashed[1]);
    check_logs("load");
    if (crashed[0] || crashed[1]) { std::printf("links: load crashed\n"); g_failures++; return; }
    std::vector<int> node_of(24);
    for (int i = 0; i < 24; i++) {
        const std::string want = "Node" + std::to_string(i);
        node_of[i] = (int)(std::find(run.w[0]->names.begin(), run.w[0]->names.end(), want) - run.w[0]->names.begin());
    }
    struct Op { uint32_t fn; void (AtSpaceGraph::*r)(AtSceneNode*); };
    const Op table[] = {
        {0x0003AEE0, &AtSpaceGraph::AddAsKind0}, {0x0003AE80, &AtSpaceGraph::AddAsKind1}, {0x0003AE70, &AtSpaceGraph::AddAsKind2}, {0x0003AEB0, &AtSpaceGraph::AddAsKind3},
        {0x0003A9C0, &AtSpaceGraph::RemoveAsKind2}, {0x0003A370, &AtSpaceGraph::RemoveAsKind0}, {0x0003A310, &AtSpaceGraph::RemoveAsKind1}, {0x0003A340, &AtSpaceGraph::RemoveAsKind3}};
    const unsigned adds = sizeof table / sizeof table[0];
    for (unsigned op = 0; op < ops; op++) {
        const unsigned which = rng() % (adds + 3);
        const int n = node_of[rng() % 24];
        float x = (float)((int)(rng() % 4000) - 2000) / 8.0f, y = (float)((int)(rng() % 4000) - 2000) / 8.0f, z = (float)((int)(rng() % 4000) - 2000) / 8.0f, radius = (float)(rng() % 800) / 8.0f + (float)least_radius;
        bool moved = rng() % 4 == 0;
        const unsigned shape = rng() % 8;
        const float sign = rng() % 2 ? 1.0f : -1.0f;
        if (shape == 0) {
            // Exactly touching a plane.
            x = tie_x + sign * radius;
            y = tie_y + sign * radius;
            moved = false;
        } else if (shape == 1) {
            // Spheres that begin at one place, so that a leaf has to order equal ends.
            x = 5.0f; y = 5.0f; z = 5.0f; radius = 8.0f;
            moved = false;
        }
        for (int s = 0; s < 2; s++) {
            TestNode& node = run.w[s]->nodeArray[n];
            if (which < adds) {
                node.m_worldSphere.center.x = x; node.m_worldSphere.center.y = y; node.m_worldSphere.center.z = z; node.m_worldSphere.radius = radius;
                if (moved) node.m_flags |= AtSceneNode::kMoved;
                On on(s);
                if (s == 0) at<FnNode>(table[which].fn)(run.g[0], nullptr, &node);
                else (run.g[1]->*table[which].r)(&node);
            } else if (which == adds) {
                run.g[s]->m_kind = (unsigned)(n % 4);
                On on(s);
                if (s == 0) at<FnNode>(0x0003A9F0)(run.g[0], nullptr, &node);
                else run.g[1]->AddNode(&node);
            } else if (which == adds + 2) {
                // To a leaf directly, as a Draw does: a graph with no item left gives its link back.
                AtSpace* leaf = &run.g[s]->m_leaves[n % run.g[s]->m_leafCount];
                void** vt = *(void***)leaf;
                On on(s);
                ((void(__thiscall*)(void*, void*, unsigned, void*))vt[8])(leaf, run.g[s], (unsigned)(n % 4), &node);
            } else if (which == adds + 1) {
                // A dirty link to take off, to be the case RemoveDirty sees.
                AtSceneLink* l = node.m_links[n % 4];
                if (l && l->m_set == run.g[s] && (node.m_flags & (AtSceneNode::kInDirtyList0 << (n % 4))) == 0) { l->m_nextDirty = run.g[s]->m_dirty[n % 4]; run.g[s]->m_dirty[n % 4] = l; node.m_flags |= AtSceneNode::kInDirtyList0 << (n % 4); }
            }
        }
        if (op % 8 == 0 || which == adds + 2) {
            check_logs("links");
            same_graph("links", run);
            if (g_failures > 25) return;
        }
    }
    check_logs("links");
    same_graph("links", run);
    g_links_made += ops;
    // Taking and giving back directly, in the order no caller here uses.
    {
        typedef void*(__fastcall* FnTake)(void*, void*);
        AtSceneLink* taken[2][2];
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i < 2; i++) taken[s][i] = s == 0 ? (AtSceneLink*)at<FnTake>(0x00039E90)(run.g[0], nullptr) : run.g[1]->TakeLink();
            if (taken[s][0] && taken[s][1]) {
                eq("taken", taken[s][0]->m_set == run.g[s], 1);
                if (s == 0) { at<FnP>(0x00039EB0)(run.g[0], nullptr, taken[0][0]); at<FnP>(0x00039EB0)(run.g[0], nullptr, taken[0][1]); }
                else { run.g[1]->ReturnLink(taken[1][0]); run.g[1]->ReturnLink(taken[1][1]); }
            }
        }
        same_graph("links given back", run);
        // A link not taken through TakeLink, made directly.
        TestNode* nodes[2] = {&run.w[0]->nodeArray[node_of[0]], &run.w[1]->nodeArray[node_of[0]]};
        for (int s = 0; s < 2; s++) {
            AtSceneLink* link = run.g[s]->m_freeLinks;
            AtSceneLinkItem* item = run.g[s]->m_freeItems;
            if (!link || !item) continue;
            run.g[s]->m_freeLinks = link->m_next;
            run.g[s]->m_freeItems = item->m_next;
            link->m_set = nullptr;
            On on(s);
            if (s == 0) at<void(__fastcall*)(void*, void*, void*, void*, void*)>(0x0003A3A0)(run.g[0], nullptr, nodes[0], link, item);
            else run.g[1]->MakeLink(nodes[1], link, item);
        }
        check_logs("made directly");
        same_graph("made directly", run);
    }
    // Hooks.
    AtSpaceGraphHook hooks[2][4] = {};
    typedef void(__fastcall* FnHook)(void*, void*, void*);
    for (int phase = 0; phase < 2; phase++) {
        const uint32_t add = phase ? 0x0003A240 : 0x0003A1C0, remove = phase ? 0x0003A270 : 0x0003A1F0;
        void (AtSpaceGraph::*radd)(AtSpaceGraphHook*) = phase ? &AtSpaceGraph::AddHookAfter : &AtSpaceGraph::AddHookBefore;
        void (AtSpaceGraph::*rremove)(AtSpaceGraphHook*) = phase ? &AtSpaceGraph::RemoveHookAfter : &AtSpaceGraph::RemoveHookBefore;
        const int script[][2] = {{1, 0}, {0, 0}, {1, 0}, {1, 1}, {1, 2}, {1, 1}, {0, 3}, {0, 2}, {1, 2}, {0, 1}, {1, 3}, {0, 0}, {0, 2}, {0, 3}, {0, 3}};
        for (const auto& step : script) {
            at<FnHook>(step[0] ? add : remove)(run.g[0], nullptr, &hooks[0][step[1]]);
            (run.g[1]->*(step[0] ? radd : rremove))(&hooks[1][step[1]]);
            for (int i = 0; i < 4; i++) {
                eq("hook's next", hooks[0][i].m_next ? hooks[0][i].m_next - hooks[0] : -1, hooks[1][i].m_next ? hooks[1][i].m_next - hooks[1] : -1);
            }
            AtSpaceGraphHook* first[2] = {phase ? run.g[0]->m_hooksAfter : run.g[0]->m_hooksBefore, phase ? run.g[1]->m_hooksAfter : run.g[1]->m_hooksBefore};
            eq("first hook", first[0] ? first[0] - hooks[0] : -1, first[1] ? first[1] - hooks[1] : -1);
        }
    }
    for (int s = 0; s < 2; s++) { run.g[s]->m_hooksBefore = nullptr; run.g[s]->m_hooksAfter = nullptr; }
    for (int s = 0; s < 2; s++) {
        On on(s);
        if (s == 0) at<Fn0>(kGraphClear)(run.g[0], nullptr);
        else run.g[1]->Clear();
    }
    check_logs("cleared");
    same_graph("cleared", run);
}

static void run_tree_links() {
    const std::string h = kAsgHead;
    const std::string tree =
        h + "BinarySpace[1]\nXBinarySpace[1]\nYBinarySpace[1]\nZBinarySpace[1]\nLeafSpace[5]\nNodeLink[64]\nNodeIncidence[256]\nRoot BinarySpace 0\nBinarySpace[1]\n{\n BinarySpace { Partition 0.5 0.25 0.8 3.5 PosChild XBinarySpace 0 NegChild YBinarySpace 0 }\n}\n"
        "XBinarySpace[1]\n{\n XBinarySpace { Partition 25.5 PosChild LeafSpace 0 NegChild ZBinarySpace 0 }\n}\nYBinarySpace[1]\n{\n YBinarySpace { Partition -25.5 PosChild LeafSpace 1 NegChild LeafSpace 2 }\n}\nZBinarySpace[1]\n{\n ZBinarySpace { Partition 0.0 PosChild LeafSpace 3 NegChild LeafSpace 4 }\n}\n"
        "LeafSpace[5]\n{\n LeafSpace { BoundingBox 0.0 9.0 0.0 1.0 0.0 1.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 9.0 0.0 1.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 1.0 0.0 9.0 }\n LeafSpace { BoundingBox 0.0 9.0 0.0 1.0 0.0 1.0 }\n LeafSpace { BoundingBox 0.0 1.0 0.0 9.0 0.0 1.0 }\n}\n}\n";
    const std::string tight =
        h + "XBinarySpace[1]\nLeafSpace[2]\nNodeLink[3]\nNodeIncidence[4]\nRoot XBinarySpace 0\nXBinarySpace[1]\n{\n XBinarySpace { Partition 0.0 PosChild LeafSpace 0 NegChild LeafSpace 1 }\n}\n" + leaves(2) + "}\n";
    const std::string items =
        h + "YBinarySpace[1]\nLeafSpace[2]\nNodeLink[8]\nNodeIncidence[9]\nRoot YBinarySpace 0\nYBinarySpace[1]\n{\n YBinarySpace { Partition 0.0 PosChild LeafSpace 0 NegChild LeafSpace 1 }\n}\n" + leaves(2) + "}\n";
    for (unsigned seed = 1; seed <= 6; seed++) run_links(seed, tree, 600, 0, 25.5f, -25.5f);
    run_links(11, tight, 300);
    run_links(12, items, 300);
    run_links(13, items, 300, 400);
}

// Stacks and states by direct calls, for depth checks and second calls.
static void run_stack() {
    g_context = "stack";
    set_alignment(2);
    g_fill = 0xCD;
    for (int file_heap = 0; file_heap < 2; file_heap++) {
        AtModifierStack stack[2] = {};
        typedef void(__fastcall* FnInit)(void*, void*, unsigned, unsigned, int);
        const unsigned shapes[][2] = {{4, 3}, {2, 1}, {1, 2}, {0, 2}, {4, 0}};
        for (const auto& shape : shapes) {
            for (int s = 0; s < 2; s++) {
                On on(s);
                if (s == 0) at<FnInit>(0x000468A0)(&stack[0], nullptr, shape[0], shape[1], file_heap);
                else stack[1].Init(shape[0], shape[1], file_heap);
            }
            check_logs("stack init");
            same_alignment_restored(2);
            eq("depth", stack[0].m_depth, stack[1].m_depth);
            eq("used", stack[0].m_used, stack[1].m_used);
            if (shape[0] >= 3 && shape[1] >= 1) {
                // Entries, and their data, through a push and a pop.
                for (int s = 0; s < 2; s++) {
                    AtModifierSlot& slot = stack[s].m_states[1].m_slots[0];
                    slot.m_count = 2;
                    slot.m_entries[0].m_modifier = (AtModifier*)&stack[s];
                    slot.m_entries[0].m_data = 3;
                    slot.m_entries[1].m_modifier = (AtModifier*)&stack[s] + 1;
                    slot.m_entries[1].m_data = 4;
                    On on(s);
                    if (s == 0) at<Fn0>(0x00046830)(&stack[0], nullptr);
                    else stack[1].Push();
                }
                eq("pushed", stack[0].m_used, stack[1].m_used);
                for (int s = 0; s < 2; s++) {
                    AtModifierSlot& slot = stack[s].m_states[2].m_slots[0];
                    eq("pushed entries", slot.m_count, 2);
                    eq("pushed modifier", slot.m_entries[1].m_modifier == (AtModifier*)&stack[s] + 1, 1);
                    eq("data not copied", slot.m_entries[1].m_data, 0);
                    // Past the end of a slot: a count to keep, and nothing copied.
                    AtModifierSlot& source = stack[s].m_states[2].m_slots[0];
                    source.m_count = 8;
                    On on(s);
                    if (s == 0) at<Fn0>(0x00046830)(&stack[0], nullptr);
                    else stack[1].Push();
                    eq("count past the end", stack[s].m_states[3].m_slots[0].m_count, 8);
                    eq("nothing copied", stack[s].m_states[3].m_slots[0].m_entries[0].m_modifier == nullptr, 1);
                    source.m_count = 0;
                }
                for (int s = 0; s < 2; s++) {
                    On on(s);
                    for (int i = 0; i < 2; i++) { if (s == 0) at<Fn0>(0x00046850)(&stack[0], nullptr); else stack[1].Pop(); }
                }
                eq("popped", stack[0].m_used, stack[1].m_used);
            }
        }
        for (int s = 0; s < 2; s++) {
            On on(s);
            if (s == 0) at<Fn0>(0x00046860)(&stack[0], nullptr);
            else stack[1].Free();
        }
        check_logs("stack freed");
        eq("freed", stack[0].m_states == nullptr, stack[1].m_states == nullptr);
        eq("freed depth", stack[0].m_depth, stack[1].m_depth);
        eq("freed used", stack[0].m_used, stack[1].m_used);
        // Already free.
        for (int s = 0; s < 2; s++) {
            On on(s);
            if (s == 0) at<Fn0>(0x00046860)(&stack[0], nullptr);
            else stack[1].Free();
        }
        check_logs("stack freed twice");
    }
    // A null stack heap, where the code checks.
    for (int file_heap = 0; file_heap < 2; file_heap++) {
        AtModifierStack stack[2] = {};
        const uint32_t va = file_heap ? 0x002C60B0 : 0x002C7E04;
        void* saved = *(void**)(uintptr_t)va;
        *(void**)(uintptr_t)va = nullptr;
        void** recovered = file_heap ? &AtIoTokenFile::s_heap : &AtModifierStack::s_heap;
        *recovered = nullptr;
        typedef void(__fastcall* FnInit)(void*, void*, unsigned, unsigned, int);
        bool crashed[2];
        for (int s = 0; s < 2; s++) {
            crashed[s] = guarded([&] {
                On on(s);
                if (s == 0) at<FnInit>(0x000468A0)(&stack[0], nullptr, 2, 2, file_heap);
                else stack[1].Init(2, 2, file_heap);
            });
            g_recording = false;
            g_side = 0;
        }
        eq("crash", crashed[0], crashed[1]);
        compare_logs("null heap for a stack");
        g_hash[0] = g_hash[1] = 0;
        *(void**)(uintptr_t)va = saved;
        *recovered = saved;
    }
}

// A tokenizer on each side, and what only a second call to a reader reaches.
struct SideFile {
    unsigned char memory[sizeof(AtIoTokenFile)];
    AtIoTokenizer* open(int side, const char* ext, const AtIoKeyword* keywords, unsigned count) {
        if (side == 0) {
            at<Fn0>(0x00015460)(memory, nullptr);
            at<int(__fastcall*)(void*, void*, const char*, const char*, int, void*)>(0x00015560)(memory, nullptr, "ART\\THING", ext, 2, nullptr);
        } else {
            AtIoTokenFile* f = new (memory) AtIoTokenFile;
            f->Open("ART\\THING", ext, 2, nullptr);
        }
        AtIoTokenizer* t = ((AtIoTokenFile*)memory)->m_tokenizer;
        if (keywords) t->SetKeywords(keywords, count, nullptr);
        return t;
    }
    void close(int side) {
        if (side == 0) { at<Fn0>(0x000154B0)(memory, nullptr); at<Fn0>(0x00015500)(memory, nullptr); }
        else { ((AtIoTokenFile*)memory)->Close(); ((AtIoTokenFile*)memory)->~AtIoTokenFile(); }
    }
};

static void run_read_again() {
    g_context = "read again";
    set_alignment(2);
    g_fill = 0xCD;
    {
        const std::string text = "{ Modifier[1]\n {\n \"Stand\" \"\"\n }\n Priority 3 }\n{ Modifier[2]\n {\n \"Stand\" \"\"\n \"Stand\" \"\"\n }\n }\n";
        std::set<std::string> names;
        gather(text, names);
        World* w[2] = {new World, new World};
        AtGroupModifier* group[2];
        AtModifierDatabase* db[2];
        const std::string mods = modifiers_for(names);
        gather(mods, names);
        set_text(mods);
        for (int s = 0; s < 2; s++) {
            make_world(*w[s], names, 16, 6);
            db[s] = new_db(s, 0xCD);
            load_db(s, db[s], w[s]->owner);
        }
        check_logs("modifiers for reading again");
        set_text(text);
        for (int s = 0; s < 2; s++) {
            void* p = std::malloc(sizeof(AtGroupModifier));
            On on(s);
            if (s == 0) at<Fn0>(0x0004FD50)(p, nullptr);
            else new (p) AtGroupModifier;
            group[s] = (AtGroupModifier*)p;
            SideFile file;
            AtIoTokenizer* t = file.open(s, "ASM", s == 0 ? (const AtIoKeyword*)(uintptr_t)0x002B1D40 : AtModifierDatabase::s_keywords, 58);
            for (int i = 0; i < 2; i++) {
                if (s == 0) at<FnPP>(0x0004FE20)(group[0], nullptr, w[0]->owner, t);
                else group[1]->Read(w[1]->owner, t);
            }
            file.close(s);
        }
        check_logs("a group read again");
        eq("count", group[0]->m_count, group[1]->m_count);
        eq("capacity", group[0]->m_capacity, group[1]->m_capacity);
        eq("priority", group[0]->m_priority, group[1]->m_priority);
        eq("flags", group[0]->m_flags, group[1]->m_flags);
    }
    {
        const std::string text = std::string(kAsgHead) + "LeafSpace[1]\nNodeLink[2]\nRoot LeafSpace 0\n" + leaves(1) + "}\n";
        GraphRun run;
        if (!prepare_graph(run, text, 8, 6, 0xCD, 2)) return;
        set_text(text);
        for (int s = 0; s < 2; s++) {
            On on(s);
            for (int i = 0; i < 2; i++) {
                SideFile file;
                AtIoTokenizer* t = file.open(s, "ASG", nullptr, 0);
                if (s == 0) at<FnPP>(0x0003AC90)(run.g[0], nullptr, run.w[0]->owner, t);
                else run.g[1]->Read(run.w[1]->owner, t);
                file.close(s);
            }
        }
        check_logs("a graph read again");
        same_graph("a graph read again", run);
    }
}

// Destroying spaces through their own vftables, with something pending.
static void run_destroy_spaces() {
    g_context = "spaces destroyed";
    const std::string h = kAsgHead;
    const std::string t = h + "BinarySpace[2]\nXBinarySpace[1]\nYBinarySpace[1]\nZBinarySpace[1]\nLeafSpace[1]\nRoot BinarySpace 0\nBinarySpace[2]\n{\n BinarySpace { PosChild XBinarySpace 0 Draw \"NodeA\" \"\" Draw \"NodeB\" \"\" }\n BinarySpace { }\n}\nXBinarySpace[1]\n{\n XBinarySpace { PosChild YBinarySpace 0 Draw \"NodeA\" \"\" LineSegIntersect \"NodeB\" \"\" }\n}\nYBinarySpace[1]\n{\n YBinarySpace { PosChild ZBinarySpace 0 Draw \"NodeA\" \"\" }\n}\nZBinarySpace[1]\n{\n ZBinarySpace { PosChild LeafSpace 0 Draw \"NodeA\" \"\" }\n}\n" + leaves(1, "Modifier \"ModA\" \"\" Draw \"NodeA\" \"\"") + "}\n";
    GraphRun run;
    if (!prepare_graph(run, t, 32, 6, 0xCD, 2)) return;
    set_text(t);
    for (int s = 0; s < 2; s++) guarded([&] { load_graph(s, run.g[s], run.w[s]->owner); });
    check_logs("load");
    same_graph("load", run);
    // An element of each class, slot 5: frees what is pending.
    for (int s = 0; s < 2; s++) {
        On on(s);
        void* elements[] = {&run.g[s]->m_binary[1], &run.g[s]->m_x[0], &run.g[s]->m_y[0], &run.g[s]->m_z[0], &run.g[s]->m_leaves[0]};
        for (void* e : elements) {
            void** table = *(void***)e;
            ((void(__thiscall*)(void*))table[5])(e);
        }
    }
    check_logs("space slot 5 with pending");
    same_graph("cleared spaces", run);
    for (int s = 0; s < 2; s++) {
        On on(s);
        if (s == 0) at<Fn0>(kGraphDtor)(run.g[0], nullptr);
        else run.g[1]->~AtSpaceGraph();
    }
    check_logs("destroyed");
}

static unsigned read_all(const std::string& dir, const char* extension, bool graph) {
    unsigned count = 0;
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); std::exit(2); }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, extension)) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); std::exit(2); }
        std::fseek(f, 0, SEEK_END);
        std::string data((size_t)std::ftell(f), 0);
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
        std::fclose(f);
        if (graph) run_asg(data, line, 0xCD, 2);
        else run_asm(data, line, 0xCD, 2);
        count++;
    }
    std::fclose(index);
    return count;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
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

static void set_heaps() {
    for (uint32_t va : {0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C60B0u}) *(void**)(uintptr_t)va = &g_heap_file;
    AtIoTokenFile::s_heap = &g_heap_file;
    AtIoStream::s_heap = &g_heap_file;
    GmNameMap::s_heap = &g_heap_file;
    GmNameSpaceDb::s_heap = &g_heap_file;
    *(void**)(uintptr_t)0x002C63FC = &g_heap_graph; AtSpaceGraph::s_heap = &g_heap_graph;
    *(void**)(uintptr_t)0x002C6410 = &g_heap_db; AtModifierDatabase::s_heap = &g_heap_db;
    *(void**)(uintptr_t)0x002C7E1C = &g_heap_context; AtModifier::s_heap = &g_heap_context;
    *(void**)(uintptr_t)0x002C7E20 = &g_heap_group; AtGroupModifier::s_heap = &g_heap_group;
    *(void**)(uintptr_t)0x002C7E04 = &g_heap_stack; AtModifierStack::s_heap = &g_heap_stack;
    *(void**)(uintptr_t)0x002C7EB8 = &g_heap_state; AtModifierState::s_heap = &g_heap_state;
}

template <class T> static void table_of(int row, uint32_t original) {
    g_table_o[row] = (const void*)(uintptr_t)original;
    T object;
    g_table_r[row] = *(const void* const*)&object;
}

static void set_tables() {
    table_of<AtLeafSpace>(0, 0x0027638C);
    table_of<AtBinarySpace>(1, 0x0027649C);
    table_of<AtXBinarySpace>(2, 0x00276464);
    table_of<AtYBinarySpace>(3, 0x0027642C);
    table_of<AtZBinarySpace>(4, 0x002763F4);
    table_of<AtModifier>(5, 0x00276CE8);
    table_of<AtLightModifier>(6, 0x00276D48);
    table_of<AtAmbientModifier>(7, 0x00276D78);
    table_of<AtPerspectiveModifier>(8, 0x00276D60);
    table_of<AtContextConstantModifier>(9, 0x00276D00);
    table_of<AtGroupModifier>(10, 0x00276D18);
    table_of<AtBlockModifier>(11, 0x00276D30);
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
    if (pos.size() != 2) { std::printf("usage: oracle_asm <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push2);
    redirect(0x00011260, (const void*)o_pop2);
    redirect(0x00011070, (const void*)o_fatal2);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    set_heaps();
    set_tables();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    set_heaps();
    set_alignment(2);
    g_heap_pad = 0x400;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_small(); std::printf("run_small done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_group_slots(); std::printf("run_group_slots done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_stack(); std::printf("run_stack done: %llu comparisons, %d differences\n", g_checks, g_failures);
    for (unsigned seed = 1; seed <= 8; seed++) run_states(seed);
    std::printf("run_states done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_written_asm(); std::printf("run_written_asm done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_written_asg(); std::printf("run_written_asg done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_shortages(); run_no_heaps(); std::printf("shortages done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_tree_links(); std::printf("run_tree_links done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_read_again(); std::printf("run_read_again done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_destroy_spaces(); std::printf("run_destroy_spaces done: %llu comparisons, %d differences\n", g_checks, g_failures);

    const unsigned asm_files = read_all(pos[1], ".ASM\t", false);
    const unsigned asg_files = read_all(pos[1], ".ASG\t", true);
    report_entries();
    std::printf("%s: %u .ASM, %u .ASG files; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", asm_files, asg_files, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
