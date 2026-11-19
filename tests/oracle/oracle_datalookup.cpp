// Differential oracle for reading the design data once it is loaded (Windows x86, local only). See docs/verification.md.
//
//   oracle_datalookup <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Every real .ADR file is loaded on both sides, each under the one before it in its archive, and start-up is done as the game does it: GENERIC.ADR
// hung under a registry made empty, and LIBRARY.ADD's eleven registries loaded under that one. Then every link of every registry is looked up by
// its path, from the registry and from the top of its tree, climbing and not, and by its name; and every value is read through the recovered
// slots and written through a logging writer. Each registry is copied by the copy form and the copy changed through the slots that change values.
// Text written here covers what no real file has. Loading is the recovered loader on the recovered side and the original's on the other: no class
// of the registry is a double. The comparison code is the adr oracle's (tests/oracle/oracle_adr.cpp), copied.
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

#include "at/AtDataRegistry.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 160).c_str(), what, o, r);
    if (g_failures >= 25) ExitProcess(1);  // Different already, and a mutant run has hundreds of these to get through.
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 160).c_str(), what, o, r);
    if (g_failures >= 25) ExitProcess(1);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }


// AtPushHeap comes just before each heap request on both sides: that is where the Nth request can be made to fail.
#define AtPushHeap DoublePushHeap
#include "oracle_doubles.h"
#undef AtPushHeap

static int g_fail_after;  // 0 off; n fails the nth request from now and lets the rest through
static void count_request() {
    if (g_fail_after > 0) {
        g_fail_alloc = --g_fail_after == 0;
        if (g_fail_after == 0) g_fail_after = -1;
    } else if (g_fail_after < 0) {
        g_fail_alloc = false;
    }
}
void AtPushHeap(void* h) { count_request(); DoublePushHeap(h); }
static void __cdecl o_push_counting(void* h) { count_request(); o_push(h); }

// While a registry is destroyed nothing freed is given back to the heap, on either side. A copy's links have parents set by path without joining
// their lists, so clearing a block's links reads parents in blocks freed before it (Detach, 0x0006C030), in the original too; given back, what
// those reads find depends on each side's heap, which differs.
static bool g_keep_freed;
static void give_back(void* p) { if (!g_keep_freed) std::free(p); }
static void __cdecl o_delete_keeping(void* p) { if (g_recording && p) g_log[0].add('D'); give_back(p); }
static void host_delete(void* p) { if (g_recording && g_side == 1 && p) g_log[1].add('D'); give_back(p); }
static void host_delete_sized(void* p, std::size_t) { if (g_recording && g_side == 1 && p) g_log[1].add('D'); give_back(p); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static int g_heap_token2;  // the token file's heap, and a pool heap left over, so that a wrong choice of heap shows in the log

typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef int(__fastcall* FnLoad)(void*, void*, void*, const char*, int, char*, void*);
typedef int(__fastcall* FnRead)(void*, void*, void*, void*, char*, void*);
typedef int(__fastcall* FnSlotRead)(void*, void*, void*, void*);
typedef int(__fastcall* FnI)(void*, void*, unsigned);
typedef int(__fastcall* FnI2)(void*, void*, unsigned, unsigned);
typedef int(__fastcall* FnReal2)(void*, void*, float, float);
typedef int(__fastcall* FnP)(void*, void*, const void*);
typedef int(__fastcall* FnP2)(void*, void*, const void*, const void*);
typedef int(__fastcall* FnPI)(void*, void*, const void*, int);
typedef int(__fastcall* FnIP)(void*, void*, unsigned, const void*);
typedef int(__fastcall* FnP3)(void*, void*, const void*, const void*, const void*);
typedef void*(__fastcall* FnRetI)(void*, void*, unsigned);
typedef void*(__fastcall* FnRetPI)(void*, void*, const void*, int);
typedef void*(__fastcall* FnRetPP)(void*, void*, const void*, const void*);
typedef void*(__fastcall* FnRet0)(void*, void*);

static const uint32_t kOrigVft[12] = {0x00277754, 0x0027777C, 0x002777A4, 0x002777CC, 0x002777F4, 0x0027781C, 0x0027784C, 0x00277874, 0x0027789C, 0x002778C4, 0x002778EC, 0x00277914};
static const uint32_t kCtor[12] = {0x00069DF0, 0x0006A040, 0x00069E80, 0x00069ED0, 0x00069F20, 0x0006A170, 0x0006A810, 0x0006A9C0, 0x0006AC40, 0x0006AC60, 0x0006AE30, 0x0006B0D0};
static const uint32_t kDtor[12] = {0x00069E10, 0x00069E70, 0x00069EA0, 0x00069EF0, 0x00069F40, 0x0006A660, 0x0006B710, 0x0006B780, 0x0006B7F0, 0x0006B860, 0x0006B8D0, 0x0006B940};
static const uint32_t kSize[12] = {0x0C, 0x10, 0x10, 0x10, 0x14, 0x3C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x14};
static const void* g_vft[12];
static const void* g_ascii_vft;
static const void* g_wide_vft;

static void* make_value_r(int type, void* p) {
    switch (type) {
    case 0: return new (p) AtDataComponent;
    case 1: return new (p) AtDataBoolean;
    case 2: return new (p) AtDataInt;
    case 3: return new (p) AtDataReal;
    case 4: return new (p) AtDataShortName;
    case 5: return new (p) AtDataString;
    case 6: return new (p) AtDataBooleanArray;
    case 7: return new (p) AtDataByteArray;
    case 8: return new (p) AtDataIntArray;
    case 9: return new (p) AtDataRealArray;
    case 10: return new (p) AtDataShortNameArray;
    default: return new (p) AtDataStringArray;
    }
}

static int type_of(int side, const void* value) {
    const void* vft = *(void* const*)value;
    for (int i = 0; i < 12; i++) {
        if (side == 0 ? vft == (const void*)(uintptr_t)kOrigVft[i] : vft == g_vft[i]) return i;
    }
    return -1;
}

// ---- comparing all of it ------------------------------------------------------------------------------------------------

// What each side's objects are called: their place in a walk of the registries. Anything else is "elsewhere", a pointer that is stale or into something unknown.
struct Ids {
    std::map<const void*, uint32_t> m;
    uint32_t next = 1;
    void add(const void* p) { m[p] = next++; }
    long long of(const void* p) const {
        if (!p) return 0;
        const auto i = m.find(p);
        return i == m.end() ? -1 : (long long)i->second;
    }
};

static std::vector<AtDataRegistry*> g_regs[2];
static AtDataRegistryOwner* g_own[2];

static Ids ids_of(int side) {
    Ids ids;
    for (AtDataRegistry* r : g_regs[side]) {
        ids.add(r);
        ids.add(&r->m_root);
        for (int p = 0; p < AtDataRegistry::kTypeCount; p++) {
            for (AtDataComponentPool* pool = &r->m_pools[p]; pool; pool = pool->m_next) {
                ids.add(pool);
                for (unsigned i = 0; i < pool->m_count; i++) {
                    if (pool->m_links) ids.add(&pool->m_links[i]);
                    if (pool->m_descriptors) ids.add(&pool->m_descriptors[i]);
                    if (pool->m_values && pool->m_type < 12) ids.add((char*)pool->m_values + i * kSize[pool->m_type]);
                }
            }
        }
    }
    return ids;
}

static std::string text_of(const char* p) { return p ? std::string(p) : std::string("(null)"); }

static std::string group_key(int side, const AtDataRegistryGroup* g) {
    if (!g) return "(none)";
    if (g == &g_own[side]->m_plain) return "(plain)";
    if (g_own[side]->m_spaces.m_pool && g >= g_own[side]->m_spaces.m_pool && g < g_own[side]->m_spaces.m_pool + g_own[side]->m_spaces.m_poolSize) return std::string(g->m_name, strnlen(g->m_name, 8));
    return "(elsewhere)";
}

static void same_text(const char* what, const AtText* o, const AtText* r) {
    eq(what, o->m_capacity, r->m_capacity);
    eq(what, o->m_start, r->m_start);
    eq(what, o->m_end, r->m_end);
}

static void same_value(int type, const char* v0, const char* v1) {
    switch (type) {
    case AtDataRegistry::kBoolean: case AtDataRegistry::kInt: case AtDataRegistry::kReal:
        eq("value", *(uint32_t*)(v0 + 0xC), *(uint32_t*)(v1 + 0xC));
        break;
    case AtDataRegistry::kShortName:
        eq("name", *(uint32_t*)(v0 + 0xC), *(uint32_t*)(v1 + 0xC));
        eq("name", *(uint32_t*)(v0 + 0x10), *(uint32_t*)(v1 + 0x10));
        break;
    case AtDataRegistry::kString: {
        const AtDataString* o = (const AtDataString*)v0;
        const AtDataString* r = (const AtDataString*)v1;
        eq("unicode", o->m_unicode, r->m_unicode);
        eq("narrow's class", *(uint32_t*)&o->m_narrow == 0x002748F0, *(const void**)&r->m_narrow == g_ascii_vft);
        eq("wide's class", *(uint32_t*)&o->m_wide == 0x002748C0, *(const void**)&r->m_wide == g_wide_vft);
        same_text("narrow", &o->m_narrow, &r->m_narrow);
        same_text("wide", &o->m_wide, &r->m_wide);
        eq("chars", o->m_chars != nullptr, r->m_chars != nullptr);
        eq("narrow's chars", o->m_narrow.m_data == o->m_chars, r->m_narrow.m_data == r->m_chars);
        eq("wide's chars", (void*)o->m_wide.m_data == o->m_chars, (void*)r->m_wide.m_data == r->m_chars);
        if (o->m_chars && r->m_chars && o->m_narrow.m_capacity == r->m_narrow.m_capacity && o->m_wide.m_capacity == r->m_wide.m_capacity) {
            if (o->m_narrow.m_data) eq("narrow text", std::memcmp(o->m_narrow.m_data, r->m_narrow.m_data, o->m_narrow.m_capacity), 0);
            if (o->m_wide.m_data) eq("wide text", std::memcmp(o->m_wide.m_data, r->m_wide.m_data, o->m_wide.m_capacity * 2), 0);
        }
        break;
    }
    case AtDataRegistry::kBooleanArray: case AtDataRegistry::kIntArray: case AtDataRegistry::kRealArray: case AtDataRegistry::kByteArray: case AtDataRegistry::kShortNameArray: {
        const char* d0 = *(const char**)(v0 + 0xC);
        const char* d1 = *(const char**)(v1 + 0xC);
        eq("array", d0 != nullptr, d1 != nullptr);
        const AtDataDescriptor* desc = *(AtDataDescriptor**)(v0 + 4);
        if (d0 && d1 && desc) {
            const unsigned n = desc->m_count;
            if (type == AtDataRegistry::kShortNameArray) {
                // Each goes through a name on the stack, so what lies past a short name's terminator is not the same on the two sides.
                for (unsigned i = 0; i < n; i++) eq("name in array", strncmp(d0 + i * 8, d1 + i * 8, 8), 0);
            } else {
                eq("array's contents", std::memcmp(d0, d1, n * (type == AtDataRegistry::kByteArray ? 1 : 4)), 0);
            }
        }
        break;
    }
    case AtDataRegistry::kStringArray: {
        const AtDataStringArray* o = (const AtDataStringArray*)v0;
        const AtDataStringArray* r = (const AtDataStringArray*)v1;
        eq("texts", o->m_texts != nullptr, r->m_texts != nullptr);
        eq("chars", o->m_chars != nullptr, r->m_chars != nullptr);
        if (o->m_texts && r->m_texts && o->m_chars && r->m_chars && o->m_descriptor) {
            for (unsigned i = 0; i < o->m_descriptor->m_count; i++) {
                same_text("element", &o->m_texts[i], &r->m_texts[i]);
                eq("element's chars", o->m_chars[i] != nullptr, r->m_chars[i] != nullptr);
                eq("element's own", o->m_texts[i].m_data == o->m_chars[i], r->m_texts[i].m_data == r->m_chars[i]);
                if (o->m_chars[i] && r->m_chars[i] && o->m_texts[i].m_capacity == r->m_texts[i].m_capacity) eq("element's text", std::memcmp(o->m_chars[i], r->m_chars[i], o->m_texts[i].m_capacity * 2), 0);
            }
        }
        break;
    }
    }
}

static void same_descriptor(const AtDataDescriptor* o, const AtDataDescriptor* r) {
    eq("descriptor's name, first word", *(const int*)&o->m_name[0], *(const int*)&r->m_name[0]);
    eq("descriptor's name, second word", *(const int*)&o->m_name[4], *(const int*)&r->m_name[4]);
    eq("descriptor's type", o->m_type, r->m_type);
    eq("descriptor's flags", o->m_flags, r->m_flags);
    eq("descriptor's count", o->m_count, r->m_count);
    eq("int floor", o->m_intFloor, r->m_intFloor);
    eq("int ceiling", o->m_intCeiling, r->m_intCeiling);
    eq("real floor", *(uint32_t*)&o->m_realFloor, *(uint32_t*)&r->m_realFloor);
    eq("real ceiling", *(uint32_t*)&o->m_realCeiling, *(uint32_t*)&r->m_realCeiling);
    eqs("format", text_of(o->m_format).c_str(), text_of(r->m_format).c_str());
}

static void same_link(const Ids* ids, const AtDataLink* o, const AtDataLink* r) {
    eq("in use", o->m_inUse, r->m_inUse);
    eqs("link's name", text_of(o->m_name).c_str(), text_of(r->m_name).c_str());
    eq("link's value", ids[0].of(o->m_value), ids[1].of(r->m_value));
    eq("link's parent", ids[0].of(o->m_parent), ids[1].of(r->m_parent));
    eq("link's sibling", ids[0].of(o->m_sibling), ids[1].of(r->m_sibling));
    eq("link's child", ids[0].of(o->m_firstChild), ids[1].of(r->m_firstChild));
}

static unsigned g_links, g_values_seen;

static void same_pool(const Ids* ids, const AtDataRegistry* ro, const AtDataRegistry* rr, const AtDataComponentPool* o, const AtDataComponentPool* r) {
    for (; o && r; o = o->m_next, r = r->m_next) {
        eq("pool's count", o->m_count, r->m_count);
        eq("pool's capacity", o->m_capacity, r->m_capacity);
        eq("pool's type", o->m_type, r->m_type);
        eq("pool's used", o->m_used, r->m_used);
        eq("pool's values", o->m_values != nullptr, r->m_values != nullptr);
        eq("pool's descriptors", o->m_descriptors != nullptr, r->m_descriptors != nullptr);
        eq("pool's links", o->m_links != nullptr, r->m_links != nullptr);
        eq("pool's next", o->m_next != nullptr, r->m_next != nullptr);
        if (o->m_count != r->m_count || o->m_type != r->m_type) return;
        for (unsigned i = 0; i < o->m_count; i++) {
            if (o->m_descriptors && r->m_descriptors) same_descriptor(&o->m_descriptors[i], &r->m_descriptors[i]);
            if (o->m_links && r->m_links) {
                same_link(ids, &o->m_links[i], &r->m_links[i]);
                g_links += o->m_links[i].m_inUse != 0;
            }
            if (o->m_values && r->m_values && o->m_type < 12) {
                const char* v0 = (const char*)o->m_values + i * kSize[o->m_type];
                const char* v1 = (const char*)r->m_values + i * kSize[r->m_type];
                eq("value's class", type_of(0, v0), type_of(1, v1));
                eq("value's descriptor", ids[0].of(*(void**)(v0 + 4)), ids[1].of(*(void**)(v1 + 4)));
                eq("value's registry", *(const void**)(v0 + 8) == ro, *(const void**)(v1 + 8) == rr);
                same_value(o->m_type, v0, v1);
                g_values_seen++;
            }
        }
    }
    eq("chain's end", o != nullptr, r != nullptr);
}

static void same_group(const Ids* ids, const AtDataRegistryGroup* o, const AtDataRegistryGroup* r) {
    eq("group's first", ids[0].of(o->m_first), ids[1].of(r->m_first));
    eq("group's database", o->m_database != nullptr, r->m_database != nullptr);
}

static void same_owner(const Ids* ids) {
    const AtDataRegistryOwner* o = g_own[0];
    const AtDataRegistryOwner* r = g_own[1];
    same_group(ids, &o->m_plain, &r->m_plain);
    eq("plain name", o->m_plain.m_name[0], r->m_plain.m_name[0]);
    const AtDataRegistryNameSpaces& a = o->m_spaces;
    const AtDataRegistryNameSpaces& b = r->m_spaces;
    eq("pool size", a.m_poolSize, b.m_poolSize);
    eq("free", a.m_free ? a.m_free - a.m_pool : -1, b.m_free ? b.m_free - b.m_pool : -1);
    eqs("last", group_key(0, a.m_last).c_str(), group_key(1, b.m_last).c_str());
    eq("size", a.m_size, b.m_size);
    eq("pool", a.m_pool != nullptr, b.m_pool != nullptr);
    if (a.m_pool && b.m_pool && a.m_poolSize == b.m_poolSize) {
        for (unsigned i = 0; i < a.m_poolSize; i++) {
            same_group(ids, &a.m_pool[i], &b.m_pool[i]);
            eq("group's next", a.m_pool[i].m_nextFree ? a.m_pool[i].m_nextFree - a.m_pool : -1, b.m_pool[i].m_nextFree ? b.m_pool[i].m_nextFree - b.m_pool : -1);
            eq("group's name", strncmp(a.m_pool[i].m_name, b.m_pool[i].m_name, 8), 0);
        }
    }
    if (a.m_entries && b.m_entries && a.m_size == b.m_size) {
        for (unsigned i = 0; i < a.m_size; i++) {
            eq("slot used", a.m_entries[i].m_value != nullptr, b.m_entries[i].m_value != nullptr);
            if (a.m_entries[i].m_value && b.m_entries[i].m_value) {
                eq("slot's name", strncmp(a.m_entries[i].m_name, b.m_entries[i].m_name, 8), 0);
                eq("slot's group", (AtDataRegistryGroup*)a.m_entries[i].m_value - a.m_pool, (AtDataRegistryGroup*)b.m_entries[i].m_value - b.m_pool);
            }
        }
    }
}

static unsigned g_full_logs;

static void same_all(const char* what) {
    g_recording = false;
    if (g_log[0].n + 2 > (int)(sizeof g_log[0].v / 4) || g_log[1].n + 2 > (int)(sizeof g_log[1].v / 4)) g_full_logs++;
    Ids ids[2] = {ids_of(0), ids_of(1)};
    eq("registries", g_regs[0].size(), g_regs[1].size());
    for (size_t k = 0; k < g_regs[0].size() && k < g_regs[1].size(); k++) {
        const AtDataRegistry* o = g_regs[0][k];
        const AtDataRegistry* r = g_regs[1][k];
        eq("+04", o->m_unknown04, r->m_unknown04);
        eq("+08", o->m_unknown08, r->m_unknown08);
        eq("+30", o->m_unknown30, r->m_unknown30);
        eq("+38", o->m_unknown38 != nullptr, r->m_unknown38 != nullptr);
        eq("loaded", o->m_loaded, r->m_loaded);
        eq("owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
        eq("parent", ids[0].of(o->m_parent), ids[1].of(r->m_parent));
        eq("types", o->m_componentTypes, r->m_componentTypes);
        eqs("group", group_key(0, o->m_group).c_str(), group_key(1, r->m_group).c_str());
        eq("next", ids[0].of(o->m_next), ids[1].of(r->m_next));
        eqs("file name", text_of(o->m_fileName).c_str(), text_of(r->m_fileName).c_str());
        same_link(ids, &o->m_root, &r->m_root);
        eq("root's registry", o->m_root.m_registry == o, r->m_root.m_registry == r);
        for (int p = 0; p < AtDataRegistry::kTypeCount; p++) same_pool(ids, o, r, &o->m_pools[p], &r->m_pools[p]);
    }
    same_owner(ids);
    compare_logs(what);
}

// ---- making and loading -------------------------------------------------------------------------------------------------

enum { kPad = 0x40 };

static void make_owners(unsigned spaces) {
    const bool was = g_recording;
    g_recording = false;
    for (int side = 0; side < 2; side++) {
        char* p = (char*)std::malloc(sizeof(AtDataRegistryOwner));
        std::memset(p, 0xEE, sizeof(AtDataRegistryOwner));
        g_own[side] = (AtDataRegistryOwner*)p;
        if (side == 0) {
            at<FnCtor>(0x000603A0)(&g_own[0]->m_plain, nullptr);
            at<FnCtor>(0x0005FFF0)(&g_own[0]->m_spaces, nullptr);
            at<FnI>(0x00060010)(&g_own[0]->m_spaces, nullptr, spaces);
        } else {
            new (&g_own[1]->m_plain) AtDataRegistryGroup;
            new (&g_own[1]->m_spaces) AtDataRegistryNameSpaces;
            g_own[1]->m_spaces.Init(spaces);
        }
    }
    g_log[0].n = g_log[1].n = 0;
    g_recording = was;
}

static void free_owners() {
    g_recording = false;
    at<Fn0>(0x00060230)(&g_own[0]->m_spaces, nullptr);
    g_own[1]->m_spaces.~AtDataRegistryNameSpaces();
    std::free(g_own[0]);
    std::free(g_own[1]);
    g_own[0] = g_own[1] = nullptr;
}

static void make_registries(unsigned char fill) {
    g_recording = false;
    char* a = (char*)std::malloc(sizeof(AtDataRegistry) + kPad);
    char* b = (char*)std::malloc(sizeof(AtDataRegistry) + kPad);
    std::memset(a, fill, sizeof(AtDataRegistry) + kPad);
    std::memset(b, fill, sizeof(AtDataRegistry) + kPad);
    { On on(0); at<FnCtor>(0x000674E0)(a, nullptr); }
    { On on(1); new (b) AtDataRegistry; }
    g_regs[0].push_back((AtDataRegistry*)a);
    g_regs[1].push_back((AtDataRegistry*)b);
}

static void file_is(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    strcpy_s(g_fs->files[0].m_path, "DESIGN\\REGISTRY\\THING.ADR");
    strcpy_s(g_fs->files[1].m_path, "DESIGN\\REGISTRY\\THING.ADR");
}

static char g_space[2][16];

// True if it came through on both sides.
static bool load(size_t which, AtDataRegistry* parent0, AtDataRegistry* parent1, bool withSpace, const char* what) {
    AtDataRegistry* o = g_regs[0][which];
    AtDataRegistry* r = g_regs[1][which];
    std::memset(g_space, 0x5A, sizeof g_space);
    char* s0 = withSpace ? g_space[0] : nullptr;
    char* s1 = withSpace ? g_space[1] : nullptr;
    int ret0 = 0, ret1 = 0;
    const bool c0 = guarded([&] { On on(0); ret0 = at<FnLoad>(0x00068880)(o, nullptr, g_own[0], "DESIGN\\REGISTRY\\THING", 77, s0, parent0); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); ret1 = r->Load(g_own[1], "DESIGN\\REGISTRY\\THING", 77, s1, parent1); });
    g_recording = false;
    eq("crash", c0, c1);
    eq("returned", ret0, ret1);
    eq("space", std::memcmp(g_space[0], g_space[1], sizeof g_space[0]), 0);
    if (c0 || c1) {
        compare_logs(what);
        return false;
    }
    same_all(what);
    return true;
}

static void clear_both(size_t which, const char* what) {
    { On on(0); at<Fn0>(0x00067540)(g_regs[0][which], nullptr); }
    { On on(1); g_regs[1][which]->Clear(); }
    same_all(what);
}

// A registry that a reload has made forget its owner stays listed by the owner after it is gone, as in the original. So the blocks stay until the owner has, and a stale pointer stays one on both sides.
static std::vector<void*> g_graveyard;

static void destroy_last() {
    g_keep_freed = true;
    AtDataRegistry* o = g_regs[0].back();
    AtDataRegistry* r = g_regs[1].back();
    g_regs[0].pop_back();
    g_regs[1].pop_back();
    { On on(0); at<Fn0>(0x00067C50)(o, nullptr); }
    { On on(1); r->~AtDataRegistry(); }
    g_graveyard.push_back(o);
    g_graveyard.push_back(r);
    compare_logs("destroy");
    g_keep_freed = false;
}

static void bury() {
    for (void* p : g_graveyard) std::free(p);
    g_graveyard.clear();
}


// ---- text, as the adr oracle writes it ----------------------------------------------------------------------------------

static const char* const kTypes[12] = {"Empty", "Boolean", "Int", "Real", "ShortName", "String", "BooleanArray", "ByteArray", "IntArray", "RealArray", "ShortNameArray", "StringArray"};
static std::string link(const std::string& body) { return "DataLink { " + body + " }\n"; }

static std::string one_of(const char* type, unsigned n) { return "DataComponentTypes[1]\n{\n DataComponent { Type " + std::string(type) + " NumComponents " + std::to_string(n) + " }\n}\n"; }


static std::string all_types(unsigned n) {
    std::string s = "DataComponentTypes[12]\n{\n";
    for (const char* t : kTypes) s += " DataComponent { Type " + std::string(t) + " NumComponents " + std::to_string(n) + " }\n";
    return s + "}\n";
}

static std::string registry(size_t links, const std::string& body) { return "DataRegistry[" + std::to_string(links) + "]\n{\n" + body + "}\n"; }

// A whole file of one link, of a pool of two.
static std::string with_type(const char* type, const std::string& body) { return one_of(type, 2) + registry(1, link("Name \"A\" Type " + std::string(type) + " " + body)); }

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

// ---- direct ------------------------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class DirectTokenizer : public AtIoTextTokenizer {
public:
    void Error(int code) override { g_log[g_side].add('E', code); }
};

static const AtIoKeyword kNoWords[] = {{0x39, "Unused"}};

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
        tokenizer.SetKeywords(kNoWords, 1, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void* raw(unsigned size, unsigned char fill) {
    const bool was = g_recording;
    g_recording = false;
    void* p = std::malloc(size + 0x40);
    std::memset(p, fill, size + 0x40);
    g_recording = was;
    return p;
}

static void make_descriptor(AtDataDescriptor*& o, AtDataDescriptor*& r) {
    o = (AtDataDescriptor*)raw(sizeof(AtDataDescriptor), 0xCD);
    r = (AtDataDescriptor*)raw(sizeof(AtDataDescriptor), 0xCD);
    at<FnCtor>(0x0006BC10)(o, nullptr);
    new (r) AtDataDescriptor;
}

static void set_all_heaps(void* h) {
    *(void**)(uintptr_t)0x002CB094 = h;
    AtDataRegistry::s_heap = h;
}

// ---- the original's entry points, beyond the adr oracle's ---------------------------------------------------------------

typedef void*(__fastcall* FnFind)(void*, void*, void*, const char*, int);       // 0x000679E0, 0x00067CF0: this, from, path, climb
typedef void*(__fastcall* FnFindLink)(void*, void*, const char*, void*, int);   // 0x000678A0: this, name, under, exact
typedef void*(__fastcall* FnNameFlag)(void*, void*, const char*, int);          // 0x00067950, 0x00067D20, 0x00068860
typedef void*(__fastcall* FnName)(void*, void*, const char*);                   // 0x00067CB0
typedef int(__fastcall* FnAttach)(void*, void*, void*, const char*);            // 0x00067D40
typedef int(__fastcall* FnCreate)(void*, void*, void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, void*);  // 0x000675B0, ret 0x38
typedef void(__fastcall* FnSetName)(void*, void*, const char*);                 // 0x00066860
typedef void(__fastcall* FnAppend)(void*, void*, char*, int);                   // 0x0006C080
typedef void(__fastcall* FnPool2)(void*, void*, void*, void*);                  // 0x00067200, 0x00068720
typedef void(__fastcall* FnV1)(void*, void*, void*);                            // slot 9
typedef void(__fastcall* FnVI)(void*, void*, unsigned);                         // slot 8
typedef void(__fastcall* FnSetElement)(void*, void*, const void*, unsigned, unsigned);  // slot 6
typedef int(__fastcall* FnSetInt)(void*, void*, void*, int);                    // 0x0006A060
typedef int(__fastcall* FnSetData)(void*, void*, void*, unsigned, const void*); // 0x0006AC80 and its like; 0x0006B330 (descriptor, size, count)
typedef int(__fastcall* FnBindRead)(void*, void*, void*, void*, char*, unsigned);  // 0x0006A4D0
typedef int(__fastcall* FnBind)(void*, void*, void*, char*, unsigned);          // 0x00069FD0

static uint32_t slot(const void* o, int n) { return *(const uint32_t*)(*(const uint32_t*)o + n * 4); }

static unsigned g_lookups, g_values_read, g_copies, g_chains, g_files_seen;

// ---- writing: the writer logs what it is asked to put, on the side that asks ---------------------------------------------

static uint32_t hash_of(const char* p, unsigned limit) {
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < limit && p[i]; i++) h = (h ^ (unsigned char)p[i]) * 16777619u;
    return h;
}

static void note(uint32_t a, uint32_t b) {
    if (g_recording) g_log[g_side].add(a, b);
}

class LogTokenizer : public AtIoTextTokenizer {
public:
    void PutInt(int v) override { note('I', (uint32_t)v); }
    void PutReal(float v) override { uint32_t bits; std::memcpy(&bits, &v, 4); note('r', bits); }
    void PutString(const char* t) override { note('S', hash_of(t, 0x400)); note('L', (uint32_t)strnlen(t, 0x400)); }
    void PutKeyword(int id) override { note('K', (uint32_t)id); }
    void PutSpace() override { note('_', 0); }
    void PutName(const char* n) override { note('n', hash_of(n, 8)); }
    void PutNewline() override { note('/', 0); }
};

static LogTokenizer* g_writer;

// ---- a value through its slots, on both sides ----------------------------------------------------------------------------

// Where an answer points: into the value itself, into its array, at one of its texts' characters, or elsewhere.
static long long where(const char* v, int type, const void* p) {
    if (!p) return 0;
    const char* q = (const char*)p;
    if (q >= v && q < v + kSize[type]) return 0x10000 + (q - v);
    if (type >= 6 && type <= 10) {
        const char* d = *(char* const*)(v + 0xC);
        if (d && q >= d && q < d + 0x10000) return 0x20000 + (q - d);
    }
    if (type == 11) {
        const AtDataStringArray* s = (const AtDataStringArray*)v;
        const AtDataDescriptor* desc = s->m_descriptor;
        if (s->m_chars && desc) {
            for (unsigned k = 0; k < desc->m_count && k < 0x1000; k++) {
                if (p == s->m_chars[k]) return 0x30000 + k;
            }
        }
    }
    return -1;
}

static unsigned count_of(const char* v) {
    const AtDataDescriptor* d = *(AtDataDescriptor* const*)(v + 4);
    return d ? d->m_count : 0;
}

static void same_element(int type, const char* v0, const char* v1, unsigned index) {
    void* a = nullptr;
    void* b = nullptr;
    { On on(0); a = at<FnRetI>(slot(v0, 5))((void*)v0, nullptr, index); }
    { On on(1); b = ((AtDataComponent*)v1)->ElementAt(index); }
    eq("element: where", where(v0, type, a), where(v1, type, b));
    if (a && b) {
        switch (type) {
        case AtDataRegistry::kBoolean: case AtDataRegistry::kInt: case AtDataRegistry::kReal: case AtDataRegistry::kBooleanArray: case AtDataRegistry::kIntArray: case AtDataRegistry::kRealArray:
            eq("element", *(uint32_t*)a, *(uint32_t*)b);
            break;
        case AtDataRegistry::kByteArray:
            eq("element", *(uint8_t*)a, *(uint8_t*)b);
            break;
        case AtDataRegistry::kShortName: case AtDataRegistry::kShortNameArray:
            eq("element", strncmp((const char*)a, (const char*)b, 8), 0);
            break;
        case AtDataRegistry::kStringArray: {
            const AtDataStringArray* s = (const AtDataStringArray*)v0;
            if (index < s->m_descriptor->m_count) eq("element's text", std::memcmp(a, b, s->m_texts[index].m_capacity * 2), 0);
            break;
        }
        }
    }
    compare_logs("element");
}

static void same_write(const char* v0, const char* v1) {
    const bool c0 = guarded([&] { On on(0); at<FnV1>(slot(v0, 9))((void*)v0, nullptr, g_writer); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); ((AtDataComponent*)v1)->Write(g_writer); });
    g_recording = false;
    eq("write: crash", c0, c1);
    compare_logs("write");
}

static void same_equals(const char* a0, const char* b0, const char* a1, const char* b1, const char* what) {
    int x = 7, y = 7;
    const bool c0 = guarded([&] { On on(0); x = at<FnP>(slot(a0, 7))((void*)a0, nullptr, b0); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); y = ((AtDataComponent*)a1)->Equals((const AtDataComponent*)b1); });
    g_recording = false;
    eq("equals: crash", c0, c1);
    if (!c0 && !c1) eq(what, x, y);
    compare_logs("equals");
}

static void same_remove(int type, char* v0, char* v1, unsigned index) {
    const std::string was = g_context;
    g_context += " / remove " + std::to_string(index) + " of " + std::to_string(count_of(v0));
    const bool c0 = guarded([&] { On on(0); at<FnVI>(slot(v0, 8))(v0, nullptr, index); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); ((AtDataComponent*)v1)->RemoveAt(index); });
    g_recording = false;
    eq("remove: crash", c0, c1);
    compare_logs("remove");
    if (!c0 && !c1) {
        eq("remove: count", count_of(v0), count_of(v1));
        same_value(type, v0, v1);
    }
    g_context = was;
}

static void same_set_element(int type, char* v0, char* v1, const void* from, unsigned index, unsigned count) {
    const std::string was = g_context;
    g_context += " / set " + std::to_string(index) + " " + std::to_string(count);
    { On on(0); at<FnSetElement>(slot(v0, 6))(v0, nullptr, from, index, count); }
    { On on(1); ((AtDataComponent*)v1)->SetElement(from, index, count); }
    compare_logs("set element");
    same_value(type, v0, v1);
    g_context = was;
}

// Every slot that only reads, on one value of each side; and, on a copy, those that change it.
static void read_value(int type, char* v0, char* v1, const char* prev0, const char* prev1, bool change) {
    const unsigned n = count_of(v0);
    same_element(type, v0, v1, 0);
    if (type >= 6) {
        if (n > 1) same_element(type, v0, v1, n - 1);
    } else {
        same_element(type, v0, v1, 5);
    }
    same_write(v0, v1);
    if (type != AtDataRegistry::kString) same_equals(v0, v0, v1, v1, "equals itself");
    if (prev0 && type != AtDataRegistry::kString) same_equals(v0, prev0, v1, prev1, "equals the one before");
    if (prev0 && type == AtDataRegistry::kString) same_equals(v0, prev0, v1, prev1, "string equals the one before");
    g_values_read++;
    if (!change) return;
    static const uint32_t pattern[8] = {0x41424344, 0x45464748, 7, 0xFFFFFFFF, 0x3F800000, 0, 0x80000000, 0x12345678};
    if (type <= AtDataRegistry::kShortName) same_set_element(type, v0, v1, pattern, 3, 9);
    if (type >= 6 && type <= 10 && n >= 2) same_set_element(type, v0, v1, pattern, 1, 1);
    if (type >= 6 && n > 0) {
        same_remove(type, v0, v1, 0);
        if (count_of(v0) > 2) same_remove(type, v0, v1, count_of(v0) - 1);
    }
}

// Every value of a registry through the slots.
static void read_values(AtDataRegistry* o, AtDataRegistry* r, bool change) {
    for (int p = 0; p < 12; p++) {
        const char* prev0 = nullptr;
        const char* prev1 = nullptr;
        AtDataComponentPool* a = &o->m_pools[p];
        AtDataComponentPool* b = &r->m_pools[p];
        for (; a && b; a = a->m_next, b = b->m_next) {
            if (a->m_count != b->m_count || a->m_type != b->m_type || !a->m_values || !b->m_values || !a->m_links || a->m_type >= 12) continue;
            for (unsigned i = 0; i < a->m_count; i++) {
                if (!a->m_links[i].m_inUse || !b->m_links[i].m_inUse) continue;
                char* v0 = (char*)a->m_values + i * kSize[a->m_type];
                char* v1 = (char*)b->m_values + i * kSize[b->m_type];
                if (!*(void**)(v0 + 4)) continue;
                const std::string was = g_context;
                g_context = was.substr(0, 100) + " / " + kTypes[a->m_type] + " " + std::to_string(i) + " " + text_of(a->m_links[i].m_name);
                read_value(a->m_type, v0, v1, prev0, prev1, change);
                g_context = was;
                prev0 = v0;
                prev1 = v1;
            }
        }
    }
}

// ---- lookups ------------------------------------------------------------------------------------------------------------

static std::string path_of(const AtDataLink* l) {
    std::vector<const char*> names;
    for (; l && l->m_name && l->m_name[0]; l = l->m_parent) names.push_back(l->m_name);
    std::string p;
    for (size_t i = names.size(); i-- > 0;) {
        p += names[i];
        if (i) p += "\\";
    }
    return p;
}

static std::string cased(std::string s, bool upper) {
    for (char& c : s) c = upper ? (char)toupper((unsigned char)c) : (char)tolower((unsigned char)c);
    return s;
}

// The registry whose root a link finally hangs from, on that side.
static size_t top_of(int side, const AtDataLink* l) {
    while (l && l->m_parent) l = l->m_parent;
    for (size_t k = 0; k < g_regs[side].size(); k++) {
        if (&g_regs[side][k]->m_root == l) return k;
    }
    return (size_t)-1;
}

template <class A, class B> static void same_found(const Ids* ids, const char* what, A a, B b) {
    void* x = nullptr;
    void* y = nullptr;
    // A link with no name in the way crashes the search in NameMatches, on both sides.
    const bool c0 = guarded([&] { On on(0); x = (void*)a(); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); y = (void*)b(); });
    g_recording = false;
    eq("lookup: crash", c0, c1);
    if (!c0 && !c1) eq(what, ids[0].of(x), ids[1].of(y));
    g_lookups++;
}

static void same_path(AtDataLink* lo, AtDataLink* lr, int skip, const char* start) {
    char a[0x400], b[0x400];
    std::memset(a, 0x5A, sizeof a);
    std::memset(b, 0x5A, sizeof b);
    strcpy_s(a, start);
    strcpy_s(b, start);
    { On on(0); at<FnAppend>(0x0006C080)(lo, nullptr, a, skip); }
    { On on(1); lr->AppendPath(b, skip); }
    eqs("path", a, b);
    eq("past the path", std::memcmp(a, b, sizeof a), 0);
    g_lookups++;
}

// Each lookup the registry has, for every link in it, from it and from the registry at the top of its tree.
static void lookups(size_t which, bool by_name) {
    AtDataRegistry* o = g_regs[0][which];
    AtDataRegistry* r = g_regs[1][which];
    const Ids ids[2] = {ids_of(0), ids_of(1)};
    std::vector<std::pair<AtDataLink*, AtDataLink*>> links;
    for (int p = 0; p < AtDataRegistry::kTypeCount; p++) {
        AtDataComponentPool* a = &o->m_pools[p];
        AtDataComponentPool* b = &r->m_pools[p];
        for (; a && b; a = a->m_next, b = b->m_next) {
            if (!a->m_links || !b->m_links || a->m_count != b->m_count) continue;
            for (unsigned i = 0; i < a->m_count; i++) {
                if (a->m_links[i].m_inUse && b->m_links[i].m_inUse) links.push_back({&a->m_links[i], &b->m_links[i]});
            }
        }
    }
    eq("links", links.size(), links.size());
    unsigned k = 0;
    for (const auto& pair : links) {
        AtDataLink* lo = pair.first;
        AtDataLink* lr = pair.second;
        const std::string path = path_of(lo);
        const char* p = path.c_str();
        const std::string slash = path + "\\";
        const std::string lower = cased(path, false);
        const std::string lost = "Nowhere\\" + path;
        const char* leaf = lo->m_name;
        same_found(ids, "find", [&] { return at<FnFind>(0x000679E0)(o, nullptr, nullptr, p, 0); }, [&] { return r->FindPath(nullptr, p, 0); });
        same_found(ids, "find, climbing", [&] { return at<FnFind>(0x000679E0)(o, nullptr, nullptr, p, 1); }, [&] { return r->FindPath(nullptr, p, 1); });
        same_found(ids, "find, ending in a backslash", [&] { return at<FnFind>(0x000679E0)(o, nullptr, nullptr, slash.c_str(), 0); }, [&] { return r->FindPath(nullptr, slash.c_str(), 0); });
        same_found(ids, "find, in lower case", [&] { return at<FnFind>(0x000679E0)(o, nullptr, nullptr, lower.c_str(), 0); }, [&] { return r->FindPath(nullptr, lower.c_str(), 0); });
        same_found(ids, "find, climbing from nowhere", [&] { return at<FnFind>(0x000679E0)(o, nullptr, lo, lost.c_str(), 1); }, [&] { return r->FindPath(lr, lost.c_str(), 1); });
        same_found(ids, "find, not climbing from the link", [&] { return at<FnFind>(0x000679E0)(o, nullptr, lo, p, 0); }, [&] { return r->FindPath(lr, p, 0); });
        same_found(ids, "path from the root", [&] { return at<FnNameFlag>(0x00067D20)(o, nullptr, p, 0); }, [&] { return r->FindFromRoot(p, 0); });
        same_found(ids, "path from nowhere, climbing", [&] { return at<FnNameFlag>(0x00067D20)(o, nullptr, lost.c_str(), 1); }, [&] { return r->FindFromRoot(lost.c_str(), 1); });
        same_found(ids, "value at a path", [&] { return at<FnNameFlag>(0x00068860)(o, nullptr, p, 1); }, [&] { return r->ValueAtPath(p, 1); });
        const size_t top0 = top_of(0, lo);
        const size_t top1 = top_of(1, lr);
        eq("top", (long long)top0, (long long)top1);
        if (top0 != (size_t)-1 && top0 == top1 && top0 != which) {
            AtDataRegistry* to = g_regs[0][top0];
            AtDataRegistry* tr = g_regs[1][top1];
            same_found(ids, "find from the top", [&] { return at<FnFind>(0x000679E0)(to, nullptr, nullptr, p, 0); }, [&] { return tr->FindPath(nullptr, p, 0); });
            same_found(ids, "value at a path from the top", [&] { return at<FnNameFlag>(0x00068860)(to, nullptr, p, 0); }, [&] { return tr->ValueAtPath(p, 0); });
        }
        if (leaf && leaf[0]) {
            const std::string upper = cased(leaf, true);
            same_found(ids, "find from the parent", [&] { return at<FnFind>(0x000679E0)(o, nullptr, lo->m_parent, leaf, 0); }, [&] { return r->FindPath(lr->m_parent, leaf, 0); });
            same_found(ids, "climb from itself", [&] { return at<FnFind>(0x000679E0)(o, nullptr, lo, leaf, 1); }, [&] { return r->FindPath(lr, leaf, 1); });
            same_found(ids, "climb to the top", [&] { return at<FnFind>(0x000679E0)(o, nullptr, lo, "No such name", 1); }, [&] { return r->FindPath(lr, "No such name", 1); });
            same_found(ids, "value from the link", [&] { return at<FnFind>(0x00067CF0)(o, nullptr, lo, leaf, 1); }, [&] { return r->ValueAt(lr, leaf, 1); });
            same_found(ids, "value from the parent", [&] { return at<FnFind>(0x00067CF0)(o, nullptr, lo->m_parent, leaf, 0); }, [&] { return r->ValueAt(lr->m_parent, leaf, 0); });
            if (by_name || (k % 7) == 0) {
                same_found(ids, "link by name", [&] { return at<FnFindLink>(0x000678A0)(o, nullptr, leaf, nullptr, 0); }, [&] { return r->FindLink(leaf, nullptr, 0); });
                same_found(ids, "link by exact name", [&] { return at<FnFindLink>(0x000678A0)(o, nullptr, upper.c_str(), nullptr, 1); }, [&] { return r->FindLink(upper.c_str(), nullptr, 1); });
                same_found(ids, "link by name under the parent", [&] { return at<FnFindLink>(0x000678A0)(o, nullptr, upper.c_str(), lo->m_parent, 0); }, [&] { return r->FindLink(upper.c_str(), lr->m_parent, 0); });
                same_found(ids, "link by exact name under itself", [&] { return at<FnFindLink>(0x000678A0)(o, nullptr, leaf, lo, 1); }, [&] { return r->FindLink(leaf, lr, 1); });
                same_found(ids, "in the blocks", [&] { return at<FnNameFlag>(0x00067950)(o, nullptr, upper.c_str(), 0); }, [&] { return r->FindInPools(upper.c_str(), 0); });
                same_found(ids, "in the blocks, exactly", [&] { return at<FnNameFlag>(0x00067950)(o, nullptr, upper.c_str(), 1); }, [&] { return r->FindInPools(upper.c_str(), 1); });
                same_found(ids, "in the blocks, exactly as it is", [&] { return at<FnNameFlag>(0x00067950)(o, nullptr, leaf, 1); }, [&] { return r->FindInPools(leaf, 1); });
                same_found(ids, "value by name", [&] { return at<FnName>(0x00067CB0)(o, nullptr, upper.c_str()); }, [&] { return r->ValueNamed(upper.c_str()); });
            }
        }
        same_path(lo, lr, 0, "");
        if ((k % 5) == 0) same_path(lo, lr, 1, "Before\\");
        k++;
    }
    compare_logs("lookups");
}

// ---- the copy form ------------------------------------------------------------------------------------------------------

// A copy of registry which, appended to the registries; the slots that change values run on it. Destroyed afterwards.
static void copy_and_change(size_t which, bool by_name) {
    make_registries(0xCD);
    const size_t c = g_regs[0].size() - 1;
    int ret0 = 7, ret1 = 7;
    *(void**)(uintptr_t)0x002CB098 = &g_heap_token2;
    AtDataRegistry::s_poolHeap = &g_heap_token2;
    const bool c0 = guarded([&] { On on(0); ret0 = at<FnP>(0x000687D0)(g_regs[0][c], nullptr, g_regs[0][which]); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); ret1 = g_regs[1][c]->CopyFrom(g_regs[1][which]); });
    g_recording = false;
    eq("copy: crash", c0, c1);
    if (c0 || c1) {
        compare_logs("copy");
        g_regs[0].pop_back();
        g_regs[1].pop_back();
        return;
    }
    eq("copy: returned", ret0, ret1);
    same_all("copy");
    g_copies++;
    const std::string was = g_context;
    g_context = was + " / lookups";
    lookups(c, by_name);
    g_context = was + " / values";
    read_values(g_regs[0][c], g_regs[1][c], true);
    g_context = was;
    same_all("copy, changed");
    destroy_last();
}

// ---- real files ---------------------------------------------------------------------------------------------------------

static void run_registries(const std::vector<std::pair<std::string, std::string>>& list, const std::vector<std::vector<unsigned char>>& contents) {
    for (size_t i = 0; i < list.size();) {
        size_t e = i;
        while (e < list.size() && list[e].second == list[i].second) e++;
        make_owners(32);
        bool fine = true;
        for (size_t k = i; k < e && fine; k++) {
            g_context = "chain: " + list[k].first;
            file_is(contents[k]);
            make_registries(0xCD);
            const size_t at_ = k - i;
            fine = load(at_, at_ ? g_regs[0][at_ - 1] : nullptr, at_ ? g_regs[1][at_ - 1] : nullptr, true, "in the chain");
        }
        eq("every file loads", fine, true);
        if (fine) {
            g_chains++;
            const size_t n = g_regs[0].size();
            for (size_t k = 0; k < n; k++) {
                g_context = "read: " + list[i + k].first;
                lookups(k, true);
                read_values(g_regs[0][k], g_regs[1][k], false);
                g_context = "copy: " + list[i + k].first;
                copy_and_change(k, false);
                g_files_seen++;
            }
            same_all("after reading");
            while (!g_regs[0].empty()) destroy_last();
        }
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
        bury();
        i = e;
    }
}

// What start-up does: GENERIC.ADR loaded on its own and hung under a registry made empty, which gets a file name, and then the library's
// registries loaded under that one, as LIBRARY.ADD lists them.
static void run_startup(const std::vector<unsigned char>& generic, const std::vector<std::vector<unsigned char>>& library, const std::vector<std::string>& names) {
    g_context = "start-up";
    make_owners(32);
    make_registries(0xCD);  // 0: the registry everything hangs under
    make_registries(0xCD);  // 1: GENERIC
    file_is(generic);
    if (!load(1, nullptr, nullptr, true, "generic")) {
        eq("generic loads", 0, 1);
        return;
    }
    int ret0 = 7, ret1 = 7;
    { On on(0); ret0 = at<FnAttach>(0x00067D40)(g_regs[0][1], nullptr, g_regs[0][0], nullptr); }
    { On on(1); ret1 = g_regs[1][1]->AttachUnder(g_regs[1][0], nullptr); }
    eq("attached", ret0, ret1);
    eq("attached says so", ret1, 1);
    same_all("attached");
    { On on(0); ret0 = at<FnCreate>(0x000675B0)(g_regs[0][0], nullptr, g_own[0], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr); }
    { On on(1); ret1 = g_regs[1][0]->Prepare(g_own[1], 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr); }
    eq("created", ret0, ret1);
    same_all("created");
    { On on(0); at<FnSetName>(0x00066860)(g_regs[0][0], nullptr, "Library"); }
    { On on(1); g_regs[1][0]->SetFileName("Library"); }
    same_all("named");
    for (size_t k = 0; k < library.size(); k++) {
        g_context = "start-up: " + names[k];
        file_is(library[k]);
        make_registries(0xCD);
        if (!load(k + 2, g_regs[0][0], g_regs[1][0], true, "under the library's")) {
            eq("library loads", 0, 1);
            return;
        }
    }
    g_context = "start-up, loaded";
    {
        const Ids ids[2] = {ids_of(0), ids_of(1)};
        same_found(ids, "the root, by an empty path", [&] { return at<FnFind>(0x000679E0)(g_regs[0][0], nullptr, &g_regs[0][0]->m_root, "", 0); }, [&] { return g_regs[1][0]->FindPath(&g_regs[1][0]->m_root, "", 0); });
    }
    for (size_t k = 0; k < g_regs[0].size(); k++) {
        g_context = k < 2 ? (k ? "start-up: generic" : "start-up: top") : "start-up: " + names[k - 2];
        lookups(k, true);
        read_values(g_regs[0][k], g_regs[1][k], false);
    }
    // By name from GENERIC's root and from the top: past GENERIC's tree into the roots hung beside it, the library's.
    g_context = "start-up, names beside";
    {
        const Ids ids[2] = {ids_of(0), ids_of(1)};
        for (size_t k = 2; k < g_regs[0].size(); k++) {
            for (int p = 0; p < 13; p++) {
                const AtDataComponentPool& pool = g_regs[0][k]->m_pools[p];
                if (!pool.m_links || !pool.m_count || !pool.m_links[0].m_inUse || !pool.m_links[0].m_name) continue;
                const char* name = pool.m_links[0].m_name;
                for (size_t from : {(size_t)0, (size_t)1}) {
                    same_found(ids, "name beside", [&] { return at<FnFindLink>(0x000678A0)(g_regs[0][from], nullptr, name, nullptr, 1); }, [&] { return g_regs[1][from]->FindLink(name, nullptr, 1); });
                }
            }
        }
    }
    // Hung again, under a link of the library's by its path, and under one that is not there.
    g_context = "start-up, attached by path";
    make_registries(0xCD);
    const size_t x = g_regs[0].size() - 1;
    { On on(0); at<FnCreate>(0x000675B0)(g_regs[0][x], nullptr, g_own[0], 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr); }
    { On on(1); g_regs[1][x]->Prepare(g_own[1], 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr); }
    same_all("another made");
    std::string first;
    for (int p = 0; p < 13 && first.empty(); p++) {
        const AtDataComponentPool& pool = g_regs[0][2]->m_pools[p];
        for (unsigned i = 0; i < pool.m_count && first.empty(); i++) {
            if (pool.m_links && pool.m_links[i].m_inUse && pool.m_links[i].m_name) first = path_of(&pool.m_links[i]);
        }
    }
    const std::string lost = "Nowhere\\" + first;
    for (const char* path : {first.c_str(), lost.c_str(), "Nowhere", "", (const char*)nullptr}) {
        { On on(0); ret0 = at<FnAttach>(0x00067D40)(g_regs[0][x], nullptr, g_regs[0][0], path); }
        { On on(1); ret1 = g_regs[1][x]->AttachUnder(g_regs[1][0], path); }
        eq("attached by path", ret0, ret1);
        same_all("attached by path");
    }
    while (!g_regs[0].empty()) destroy_last();
    g_regs[0].clear();
    g_regs[1].clear();
    free_owners();
    bury();
}

// ---- text written here: what no real file has --------------------------------------------------------------------------

static std::vector<std::string> written_registries() {
    std::vector<std::string> t;
    std::string all;
    all += link("Name \"E\" Type Empty Data WillAcceptParentData");
    all += link("Name \"B\" Type Boolean Data 1 WillAcceptParentData");
    all += link("Name \"B2\" Type Boolean Data 0");
    all += link("Name \"I\" Type Int Data -7");
    all += link("Name \"I2\" Type Int Data -7");
    all += link("Name \"R\" Type Real Data 2.5 WillAcceptParentData");
    all += link("Name \"R2\" Type Real Data 2.5");
    all += link("Name \"R3\" Type Real Data -0.0");
    all += link("Name \"N\" Type ShortName Data \"Abc\"");
    all += link("Name \"N2\" Type ShortName Data \"EightChr\"");
    all += link("Name \"N3\" Type ShortName Data \"Abc\"");
    all += link("Name \"S\" Type String Data \"\"");
    all += link("Name \"S2\" Type String Data \"\"");
    all += link("Name \"S3\" Type String Data \"Text\" \"Unicode\"");
    all += link("Name \"S4\" Type String Ceiling 30 Data \"Other\"");
    all += link("Name \"BA\" Type BooleanArray NumElements 3 Data 1 0 1 WillAcceptParentData");
    all += link("Name \"YA\" Type ByteArray NumElements 4 Data 1 255 300 7");
    all += link("Name \"YB\" Type ByteArray NumElements 1 Data 9");
    all += link("Name \"IA\" Type IntArray NumElements 4 Data 1 2 3 4");
    all += link("Name \"RA\" Type RealArray NumElements 3 Data 1.5 -2.5 3.5");
    all += link("Name \"NA\" Type ShortNameArray NumElements 3 Data \"A\" \"EightChr\" \"LongerThanEight\"");
    all += link("Name \"SA\" Type StringArray NumElements 3 Ceiling 12 Data \"One\" \"\" \"Three\"");
    all += link("Name \"SB\" Type StringArray NumElements 1 Data \"Lone\"");
    all += link("Name \"Z\" Type IntArray NumElements 0 Data");
    t.push_back(all_types(3) + registry(24, all));
    // A tree by paths, with names a path cannot reach: a backslash in a name, an empty name, no name, names alike but for case.
    const std::string decl = "DataComponentTypes[2]\n{\n DataComponent { Type Empty NumComponents 4 }\n DataComponent { Type Int NumComponents 8 }\n}\n";
    std::string tree = link("Name \"A\" Type Empty Data") + link("Name \"B\" Type Empty Parent \"A\\\" Data") + link("Name \"C\" Type Int Parent \"A\\B\\\" Data 1") +
                       link("Name \"c\" Type Int Parent \"A\\\" Data 2") + link("Name \"X\\Y\" Type Int Data 3") + link("Name \"\" Type Int Parent \"A\\\" Data 4") +
                       link("Type Int Parent \"A\\B\\\" Data 5") + link("Name \"D\" Type Int Parent \"A\\B\\\" Data 6") + link("Name \"AB\" Type Int Data 7") +
                       link("Name \"Same\" Type Int Parent \"A\\\" Data 8") + link("Name \"Same\" Type Int Parent \"A\\B\\\" Data 9");
    t.push_back(decl + registry(11, tree));
    // More than declared, so names are found in a chained block, where the exact flag is lost.
    t.push_back(one_of("Int", 1) + registry(3, link("Name \"P\" Type Int Data 1") + link("Name \"q\" Type Int Data 2") + link("Name \"R\" Type Int Data 3")));
    // Formats, which the copy's descriptors take too.
    t.push_back(one_of("Int", 2) + registry(2, link("Name \"F\" Type Int FormatString \"%d units\" Data 1") + link("Name \"G\" Type Int Data 2")));
    return t;
}

static const char* const kWrittenParent =
    "DataComponentTypes[2]\n{\n DataComponent { Type Empty NumComponents 2 }\n DataComponent { Type Int NumComponents 2 }\n}\n"
    "DataRegistry[3]\n{\n"
    " DataLink { Name \"A\" Type Empty Data }\n"
    " DataLink { Name \"K\" Type Int Parent \"A\\\" Data 1 }\n"
    " DataLink { Name \"Same\" Type Int Parent \"A\\\" Data 2 }\n"
    "}\n";

static void run_written() {
    for (const std::string& text : written_registries()) {
        for (const bool under : {false, true}) {
            g_context = std::string(under ? "written, under a parent: " : "written: ") + text.substr(0, 100);
            make_owners(8);
            size_t at_ = 0;
            if (under) {
                const std::vector<unsigned char> parent = bytes(kWrittenParent);
                file_is(parent);
                make_registries(0xCD);
                if (!load(0, nullptr, nullptr, true, "parent")) { eq("parent loads", 0, 1); continue; }
                at_ = 1;
            }
            const std::vector<unsigned char> data = bytes(text);
            file_is(data);
            make_registries(0xCD);
            if (load(at_, under ? g_regs[0][0] : nullptr, under ? g_regs[1][0] : nullptr, true, "written")) {
                lookups(at_, true);
                // Paths no link has.
                const Ids ids[2] = {ids_of(0), ids_of(1)};
                AtDataRegistry* o = g_regs[0][at_];
                AtDataRegistry* r = g_regs[1][at_];
                for (const char* p : {"", "\\", "\\A", "A\\\\B", "A\\B\\C\\", "A\\b\\C", "X\\Y", "X", "AB\\", "A\\Same", "A\\B\\Same", "Same", "K", "A\\K", "Nowhere", (const char*)nullptr}) {
                    for (int climb = 0; climb < 2; climb++) {
                        same_found(ids, "written path", [&] { return at<FnFind>(0x000679E0)(o, nullptr, nullptr, p, climb); }, [&] { return r->FindPath(nullptr, p, climb); });
                        same_found(ids, "written path from the root", [&] { return at<FnFind>(0x000679E0)(o, nullptr, &o->m_root, p, climb); }, [&] { return r->FindPath(&r->m_root, p, climb); });
                        same_found(ids, "written value", [&] { return at<FnNameFlag>(0x00068860)(o, nullptr, p, climb); }, [&] { return r->ValueAtPath(p, climb); });
                    }
                    if (!p) continue;
                    for (int exact = 0; exact < 2; exact++) {
                        same_found(ids, "written name", [&] { return at<FnFindLink>(0x000678A0)(o, nullptr, p, nullptr, exact); }, [&] { return r->FindLink(p, nullptr, exact); });
                        same_found(ids, "written name in the blocks", [&] { return at<FnNameFlag>(0x00067950)(o, nullptr, p, exact); }, [&] { return r->FindInPools(p, exact); });
                    }
                    same_found(ids, "written value by name", [&] { return at<FnName>(0x00067CB0)(o, nullptr, p); }, [&] { return r->ValueNamed(p); });
                }
                for (const char* n : {"r", "R", "q", "Q", "p", "P", "same", "c", "C"}) {
                    for (int exact = 0; exact < 2; exact++) same_found(ids, "written name, case", [&] { return at<FnNameFlag>(0x00067950)(o, nullptr, n, exact); }, [&] { return r->FindInPools(n, exact); });
                }
                read_values(o, r, false);
                copy_and_change(at_, true);
                // A copy over a copy that is loaded, and a copy of the parent.
                make_registries(0xCD);
                const size_t c = g_regs[0].size() - 1;
                for (const size_t from : {at_, (size_t)0, at_}) {
                    { On on(0); at<FnP>(0x000687D0)(g_regs[0][c], nullptr, g_regs[0][from]); }
                    { On on(1); g_regs[1][c]->CopyFrom(g_regs[1][from]); }
                    same_all("copy over a copy");
                }
            }
            while (!g_regs[0].empty()) destroy_last();
            g_regs[0].clear();
            g_regs[1].clear();
            free_owners();
            bury();
        }
    }
}

// ---- made and named directly --------------------------------------------------------------------------------------------

static void run_create() {
    static const unsigned kCounts[][12] = {{0}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, {0, 0, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0}};
    for (const auto& counts : kCounts) {
        for (const bool parent : {false, true}) {
            for (const bool loaded_first : {false, true}) {
                g_context = "create";
                make_owners(4);
                make_registries(0xCD);
                make_registries(0xCD);
                if (loaded_first) {
                    const std::vector<unsigned char> data = bytes(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 2")));
                    file_is(data);
                    load(1, nullptr, nullptr, false, "loaded first");
                }
                if (parent) {
                    const std::vector<unsigned char> data = bytes(kWrittenParent);
                    file_is(data);
                    load(0, nullptr, nullptr, false, "the parent");
                }
                int ret0 = 7, ret1 = 7;
                const unsigned* n = counts;
                { On on(0); ret0 = at<FnCreate>(0x000675B0)(g_regs[0][1], nullptr, g_own[0], n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9], n[10], n[11], parent ? g_regs[0][0] : nullptr); }
                { On on(1); ret1 = g_regs[1][1]->Prepare(g_own[1], n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9], n[10], n[11], parent ? g_regs[1][0] : nullptr); }
                eq("created", ret0, ret1);
                eq("type count", g_regs[0][1]->m_componentTypes, g_regs[1][1]->m_componentTypes);
                eq("thirteenth block", g_regs[0][1]->m_pools[12].m_count, g_regs[1][1]->m_pools[12].m_count);
                same_all("created");
                // Names through the file name's block: none, longer, as long, shorter, empty, longer again.
                for (const char* name : {"Library", "Library and more", "Same length here", "Short", "", "Longest of all the names here"}) {
                    { On on(0); at<FnSetName>(0x00066860)(g_regs[0][1], nullptr, name); }
                    { On on(1); g_regs[1][1]->SetFileName(name); }
                    same_all("named");
                    eq("past the name", std::memcmp(g_regs[0][1]->m_fileName, g_regs[1][1]->m_fileName, std::strlen(name) + 4), 0);
                }
                while (!g_regs[0].empty()) destroy_last();
                g_regs[0].clear();
                g_regs[1].clear();
                free_owners();
                bury();
            }
        }
    }
    // Into blocks that hold leftovers, in a registry not loaded, so that neither clears them first: the copy form, and Create with no parent.
    for (int form = 0; form < 2; form++) {
        g_context = form ? "create over leftovers" : "copy over leftovers";
        make_owners(4);
        make_registries(0xCD);
        make_registries(0xCD);
        make_registries(0xCD);
        const std::vector<unsigned char> data = bytes(all_types(2) + registry(2, link("Name \"A\" Type Int Data 2") + link("Name \"B\" Type Real Data 2.5")));
        file_is(data);
        load(0, nullptr, nullptr, false, "the source");
        for (int side = 0; side < 2; side++) {
            AtDataRegistry* x = g_regs[side][1];
            x->m_parent = g_regs[side][2];
            for (int p = 0; p < 13; p++) {
                AtDataComponentPool& pool = x->m_pools[p];
                pool.m_used = 5;
                pool.m_values = nullptr;
                pool.m_descriptors = nullptr;
                pool.m_links = nullptr;
                pool.m_next = (AtDataComponentPool*)&g_regs[side][2]->m_pools[p];
            }
            x->m_pools[2].m_values = (AtDataComponent*)g_regs[side][2];
        }
        int ret0, ret1;
        if (form == 0) {
            { On on(0); ret0 = at<FnP>(0x000687D0)(g_regs[0][1], nullptr, g_regs[0][0]); }
            { On on(1); ret1 = g_regs[1][1]->CopyFrom(g_regs[1][0]); }
        } else {
            { On on(0); ret0 = at<FnCreate>(0x000675B0)(g_regs[0][1], nullptr, g_own[0], 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr); }
            { On on(1); ret1 = g_regs[1][1]->Prepare(g_own[1], 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, nullptr); }
        }
        eq("over leftovers", ret0, ret1);
        for (int p = 0; p < 13; p++) {
            eq("leftover used", g_regs[0][1]->m_pools[p].m_used, g_regs[1][1]->m_pools[p].m_used);
            eq("leftover next", g_regs[0][1]->m_pools[p].m_next == (AtDataComponentPool*)&g_regs[0][2]->m_pools[p], g_regs[1][1]->m_pools[p].m_next == (AtDataComponentPool*)&g_regs[1][2]->m_pools[p]);
            eq("leftover values", g_regs[0][1]->m_pools[p].m_values == (AtDataComponent*)g_regs[0][2], g_regs[1][1]->m_pools[p].m_values == (AtDataComponent*)g_regs[1][2]);
        }
        eq("leftover parent", g_regs[0][1]->m_parent == g_regs[0][2], g_regs[1][1]->m_parent == g_regs[1][2]);
        compare_logs("over leftovers");
        // Not destroyed: the leftovers would be freed. Only what was filed is taken back out.
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
    }
    // No heap for the name, and no memory.
    g_context = "create, failing";
    for (int stage = 0; stage < 2; stage++) {
        make_owners(4);
        make_registries(0xCD);
        if (stage == 0) set_all_heaps(nullptr);
        else g_fail_alloc = true;
        const bool c0 = guarded([&] { On on(0); at<FnSetName>(0x00066860)(g_regs[0][0], nullptr, "Name"); });
        g_recording = false;
        const bool c1 = guarded([&] { On on(1); g_regs[1][0]->SetFileName("Name"); });
        g_recording = false;
        eq("no name: crash", c0, c1);
        compare_logs("no name");
        const bool d0 = guarded([&] { On on(0); at<FnCreate>(0x000675B0)(g_regs[0][0], nullptr, g_own[0], 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr); });
        g_recording = false;
        const bool d1 = guarded([&] { On on(1); g_regs[1][0]->Prepare(g_own[1], 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr); });
        g_recording = false;
        eq("no blocks: crash", d0, d1);
        compare_logs("no blocks");
        g_fail_alloc = false;
        at<void(__cdecl*)(void*)>(0x00066100)(&g_heap_token);
        AtDataRegistry::SetHeap(&g_heap_token);
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
    }
}

// A link's path, on links built by hand and shared by both sides.
static void run_paths() {
    g_context = "paths by hand";
    struct Spec { const char* name; int parent; };
    const Spec specs[] = {{nullptr, -1}, {"Top", 0}, {"", 1}, {"Mid", 1}, {nullptr, 3}, {"Leaf", 4}, {"Low", 2}, {nullptr, -1}, {"Deep", 3}, {nullptr, 4}, {"Under", 9}};
    const int n = (int)(sizeof specs / sizeof specs[0]);
    AtDataLink links[n];
    for (int i = 0; i < n; i++) {
        links[i].m_name = (char*)specs[i].name;
        links[i].m_parent = specs[i].parent >= 0 ? &links[specs[i].parent] : nullptr;
    }
    for (int i = 0; i < n; i++) {
        for (int skip = 0; skip < 2; skip++) {
            for (const char* start : {"", "Was\\"}) same_path(&links[i], &links[i], skip, start);
        }
    }
    for (int i = 0; i < n; i++) links[i].m_name = nullptr, links[i].m_parent = nullptr;
}

// One value of each type and each side, through every slot, directly.
static void run_slots() {
    g_fill = 0xCD;
    static const char* const kText[12] = {"", "5 0", "5 -3", "1.5 -2.5", "\"Abc\" \"EightChr\"", "\"Text\" \"B\"", "1 0 1", "1 255 300", "1 2 3", "1.5 -2.5 3.5", "\"A\" \"EightChr\" \"LongerThanEight\"", "\"One\" \"\" \"Three\""};
    static const char* const kOther[12] = {"", "1 0", "-3 5", "1.5 -2.5", "\"Abc\" \"X\"", "\"Text\" \"Unicode\"", "0 0 0", "9 9 9", "4 5 6", "0.5 0.5 0.5", "\"B\" \"C\" \"D\"", "\"Uno\" \"Dos\" \"Tres\""};
    for (int t = 0; t < 12; t++) {
        for (const unsigned flags : {0u, (unsigned)AtDataDescriptor::kAcceptsParentData}) {
            g_context = std::string("slots: ") + kTypes[t];
            char* o = (char*)raw(kSize[t], 0xCD);
            char* r = (char*)raw(kSize[t], 0xCD);
            char* o2 = (char*)raw(kSize[t], 0xCD);
            char* r2 = (char*)raw(kSize[t], 0xCD);
            at<FnCtor>(kCtor[t])(o, nullptr);
            make_value_r(t, r);
            at<FnCtor>(kCtor[t])(o2, nullptr);
            make_value_r(t, r2);
            AtDataDescriptor *d0, *d1, *e0, *e1;
            make_descriptor(d0, d1);
            make_descriptor(e0, e1);
            d0->m_count = d1->m_count = e0->m_count = e1->m_count = 3;
            d0->m_flags = d1->m_flags = e0->m_flags = e1->m_flags = flags;
            {
                Direct da(kText[t]), db(kText[t]), dc(kOther[t]), dd(kOther[t]);
                { On on(0); at<FnSlotRead>(slot(o, 1))(o, nullptr, d0, &da.tokenizer); at<FnSlotRead>(slot(o2, 1))(o2, nullptr, e0, &dc.tokenizer); }
                { On on(1); ((AtDataComponent*)r)->Read(d1, &db.tokenizer); ((AtDataComponent*)r2)->Read(e1, &dd.tokenizer); }
                compare_logs("read");
            }
            for (unsigned i : {0u, 1u, 2u, 6u}) {
                if (t >= 6 && i >= count_of(o)) continue;
                same_element(t, o, r, i);
            }
            same_write(o, r);
            same_write(o2, r2);
            same_equals(o, o2, r, r2, "equals another");
            same_equals(o2, o, r2, r, "equals another, the other way");
            if (t != 5) same_equals(o, o, r, r, "equals itself");
            if (t == 0) {
                typedef int(__fastcall* FnResize)(void*, void*, unsigned);
                int x, y;
                { On on(0); x = at<FnResize>(slot(o, 4))(o, nullptr, 3); }
                { On on(1); y = ((AtDataComponent*)r)->Resize(3); }
                eq("resize: no type", x, y);
                compare_logs("resize: no type");
            }
            if (t == 3) {
                // Reals: the zeroes, a NaN, equal bits.
                for (const uint32_t a : {0x00000000u, 0x80000000u, 0x7FC00000u, 0x3FC00000u}) {
                    for (const uint32_t b : {0x00000000u, 0x80000000u, 0x7FC00000u, 0x3FC00000u}) {
                        std::memcpy(o + 0xC, &a, 4); std::memcpy(r + 0xC, &a, 4);
                        std::memcpy(o2 + 0xC, &b, 4); std::memcpy(r2 + 0xC, &b, 4);
                        same_equals(o, o2, r, r2, "real equals");
                    }
                }
            }
            if (t == 4) {
                // Equal up to a terminator, different past it; equal in all eight.
                std::memcpy(o + 0xC, "Ab\0XXXXX", 8); std::memcpy(r + 0xC, "Ab\0XXXXX", 8);
                std::memcpy(o2 + 0xC, "Ab\0YYYYY", 8); std::memcpy(r2 + 0xC, "Ab\0YYYYY", 8);
                same_equals(o, o2, r, r2, "names equal to the terminator");
                std::memcpy(o2 + 0xC, "Ab\x01YYYYY", 8); std::memcpy(r2 + 0xC, "Ab\x01YYYYY", 8);
                same_equals(o, o2, r, r2, "names unequal");
                std::memcpy(o + 0xC, "EightChr", 8); std::memcpy(r + 0xC, "EightChr", 8);
                std::memcpy(o2 + 0xC, "EightChr", 8); std::memcpy(r2 + 0xC, "EightChr", 8);
                same_equals(o, o2, r, r2, "names of eight");
            }
            if (t == 5) {
                // Strings: lengths differ, both empty, one length (a crash on null in both).
                Direct da("\"\""), db("\"\""), dc("\"\" \"Unicode\""), dd("\"\" \"Unicode\"");
                { On on(0); at<FnSlotRead>(slot(o, 1))(o, nullptr, d0, &da.tokenizer); }
                { On on(1); ((AtDataComponent*)r)->Read(d1, &db.tokenizer); }
                same_equals(o, o2, r, r2, "strings of different lengths");
                { On on(0); at<FnSlotRead>(slot(o2, 1))(o2, nullptr, e0, &dc.tokenizer); }
                { On on(1); ((AtDataComponent*)r2)->Read(e1, &dd.tokenizer); }
                compare_logs("read empty");
                same_equals(o, o2, r, r2, "strings both empty");
                Direct de("\"Same\""), df("\"Same\""), dg("\"Abcd\""), dh("\"Abcd\"");
                { On on(0); at<FnSlotRead>(slot(o, 1))(o, nullptr, d0, &de.tokenizer); at<FnSlotRead>(slot(o2, 1))(o2, nullptr, e0, &dg.tokenizer); }
                { On on(1); ((AtDataComponent*)r)->Read(d1, &df.tokenizer); ((AtDataComponent*)r2)->Read(e1, &dh.tokenizer); }
                compare_logs("read four");
                same_equals(o, o2, r, r2, "strings of one length");
                same_write(o, r);
            }
            // Setting elements.
            static const uint32_t pattern[8] = {0x5A595857, 0x56555453, 9, 0xFFFFFFFE, 0x40000000, 0, 0x7F800000, 0x01020304};
            if (t != 11) {
                for (const unsigned index : {0u, 1u, 2u}) {
                    for (const unsigned count : {0u, 1u, 2u}) {
                        if (t >= 6 && index + count > count_of(o)) continue;
                        same_set_element(t, o, r, pattern, index, count);
                    }
                }
            } else {
                // Over the list of pointers, from an index on, for as many bytes as the text has: each side's own pointers differ, so the bytes
                // written are compared, and what is left of the pointers is compared as left as it was. Then put back.
                AtDataStringArray* x = (AtDataStringArray*)o;
                AtDataStringArray* y = (AtDataStringArray*)r;
                for (const char* text : {"AB", "ABCDEFG", ""}) {
                    for (const unsigned index : {0u, 1u}) {
                        unsigned short* kept0[3] = {x->m_chars[0], x->m_chars[1], x->m_chars[2]};
                        unsigned short* kept1[3] = {y->m_chars[0], y->m_chars[1], y->m_chars[2]};
                        { On on(0); at<FnSetElement>(slot(o, 6))(o, nullptr, text, index, 99); }
                        { On on(1); y->SetElement(text, index, 99); }
                        compare_logs("set element over the pointers");
                        const unsigned len = (unsigned)std::strlen(text);
                        eq("over the pointers", std::memcmp((char*)x->m_chars + index * 4, (char*)y->m_chars + index * 4, len), 0);
                        eq("over the pointers: the text", std::memcmp((char*)y->m_chars + index * 4, text, len), 0);
                        for (unsigned b = index * 4 + len; b < 12; b++) {
                            eq("the rest of the pointers", ((char*)x->m_chars)[b] == ((char*)kept0)[b], ((char*)y->m_chars)[b] == ((char*)kept1)[b]);
                        }
                        for (int k = 0; k < 3; k++) x->m_chars[k] = kept0[k], y->m_chars[k] = kept1[k];
                    }
                }
            }
            if (t == 11) {
                ((AtDataStringArray*)o)->m_texts[0].m_start = 1;
                ((AtDataStringArray*)r)->m_texts[0].m_start = 1;
                same_write(o, r);
                ((AtDataStringArray*)o)->m_texts[0].m_start = 0;
                ((AtDataStringArray*)r)->m_texts[0].m_start = 0;
            }
            // Removing: one in the middle, one past the end, the last, until none. Not on none: that asks Resize for 0xFFFFFFFF elements, whose
            // size wraps, and the harness's padding wraps it again into a small block that both sides then overrun.
            for (const unsigned index : {1u, 7u, 0u, 0u, 0u, 0u}) {
                if (t >= 6 && count_of(o) == 0) break;
                same_remove(t, o, r, index);
            }
            { On on(0); at<Fn0>(kDtor[t])(o, nullptr); at<Fn0>(kDtor[t])(o2, nullptr); }
            { On on(1); ((AtDataComponent*)r)->~AtDataComponent(); ((AtDataComponent*)r2)->~AtDataComponent(); }
            compare_logs("destroyed");
        }
    }
}

// The copies a registry made from another takes, one class at a time.
static void run_copies() {
    g_fill = 0xCD;
    static const char* const kText[12] = {"", "5", "-5", "1.5", "\"EightChr\"", "\"Text\"", "1 0 1", "1 255 300", "1 2 3", "1.5 -2.5 3.5", "\"A\" \"EightChr\" \"LongerThanEight\"", "\"One\" \"\" \"Three and more\""};
    typedef int(__fastcall* FnCopy)(void*, void*, void*, const void*);
    static const uint32_t kCopy[12] = {0, 0, 0, 0, 0, 0, 0x0006AD70, 0x0006AB90, 0x0006AD70, 0x0006AD70, 0x0006B010, 0x0006B9E0};
    static const uint32_t kSetData[12] = {0, 0, 0, 0, 0, 0, 0x0006AC80, 0x0006AAA0, 0x0006AC80, 0x0006AC80, 0x0006AF10, 0};
    for (int t = 1; t < 12; t++) {
        for (const int ceiling : {0x7FFFFFFF, 40, 1}) {
            g_context = std::string("copies: ") + kTypes[t];
            char* src0 = (char*)raw(kSize[t], 0xCD);
            char* src1 = (char*)raw(kSize[t], 0xCD);
            at<FnCtor>(kCtor[t])(src0, nullptr);
            make_value_r(t, src1);
            AtDataDescriptor *d0, *d1, *e0, *e1;
            make_descriptor(d0, d1);
            make_descriptor(e0, e1);
            d0->m_count = d1->m_count = 3;
            e0->m_intCeiling = e1->m_intCeiling = ceiling;
            Direct da(kText[t]), db(kText[t]);
            { On on(0); at<FnSlotRead>(slot(src0, 1))(src0, nullptr, d0, &da.tokenizer); }
            { On on(1); ((AtDataComponent*)src1)->Read(d1, &db.tokenizer); }
            compare_logs("source");
            char* o = (char*)raw(kSize[t], 0xCD);
            char* r = (char*)raw(kSize[t], 0xCD);
            at<FnCtor>(kCtor[t])(o, nullptr);
            make_value_r(t, r);
            // Twice: the second over what the first left.
            for (int round = 0; round < 2; round++) {
                e0->m_count = e1->m_count = 3;
                switch (t) {
                case 1: case 2: case 3:
                    { On on(0); eq("set", at<FnSetInt>(0x0006A060)(o, nullptr, e0, *(int*)(src0 + 0xC)), 0); }
                    { On on(1); eq("set", t == 1 ? ((AtDataBoolean*)r)->Set(e1, *(int*)(src1 + 0xC)) : t == 2 ? ((AtDataInt*)r)->Set(e1, *(int*)(src1 + 0xC)) : ((AtDataReal*)r)->Set(e1, *(float*)(src1 + 0xC)), 0); }
                    break;
                case 4:
                    { On on(0); eq("set", at<FnP2>(0x0006A100)(o, nullptr, e0, src0 + 0xC), 0); }
                    { On on(1); eq("set", ((AtDataShortName*)r)->Set(e1, src1 + 0xC), 0); }
                    break;
                case 5: {
                    AtDataString* s0 = (AtDataString*)src0;
                    AtDataString* s1 = (AtDataString*)src1;
                    void* text0 = s0->m_unicode ? (void*)&s0->m_wide : (void*)&s0->m_narrow;
                    AtText* text1 = s1->m_unicode ? (AtText*)&s1->m_wide : (AtText*)&s1->m_narrow;
                    { On on(0); eq("copy text", at<FnP2>(0x0006A250)(o, nullptr, e0, text0), 0); }
                    { On on(1); eq("copy text", ((AtDataString*)r)->CopyText(e1, text1), 0); }
                    break;
                }
                default:
                    { On on(0); eq("copy", at<FnCopy>(kCopy[t])(o, nullptr, e0, src0), 0); }
                    {
                        On on(1);
                        int ret = 7;
                        switch (t) {
                        case 6: ret = ((AtDataBooleanArray*)r)->Copy(e1, (AtDataBooleanArray*)src1); break;
                        case 7: ret = ((AtDataByteArray*)r)->Copy(e1, (AtDataByteArray*)src1); break;
                        case 8: ret = ((AtDataIntArray*)r)->Copy(e1, (AtDataIntArray*)src1); break;
                        case 9: ret = ((AtDataRealArray*)r)->Copy(e1, (AtDataRealArray*)src1); break;
                        case 10: ret = ((AtDataShortNameArray*)r)->Copy(e1, (AtDataShortNameArray*)src1); break;
                        default: ret = ((AtDataStringArray*)r)->Copy(e1, (AtDataStringArray*)src1); break;
                        }
                        eq("copy", ret, 0);
                    }
                    break;
                }
                compare_logs("copied");
                eq("copy's descriptor", *(void**)(o + 4) == e0, *(void**)(r + 4) == e1);
                same_value(t, o, r);
                same_descriptor(e0, e1);
            }
            // Set from nothing, and from fewer than held.
            if (kSetData[t]) {
                for (const bool from : {false, true}) {
                    const void* data0 = from ? *(void**)(src0 + 0xC) : nullptr;
                    const void* data1 = from ? *(void**)(src1 + 0xC) : nullptr;
                    { On on(0); at<FnSetData>(kSetData[t])(o, nullptr, e0, 2, data0); }
                    {
                        On on(1);
                        switch (t) {
                        case 6: ((AtDataBooleanArray*)r)->SetData(e1, 2, (const int*)data1); break;
                        case 7: ((AtDataByteArray*)r)->SetData(e1, 2, (const unsigned char*)data1); break;
                        case 8: ((AtDataIntArray*)r)->SetData(e1, 2, (const int*)data1); break;
                        case 9: ((AtDataRealArray*)r)->SetData(e1, 2, (const float*)data1); break;
                        default: ((AtDataShortNameArray*)r)->SetData(e1, 2, (char(*)[8])data1); break;
                        }
                    }
                    compare_logs("set data");
                    e0->m_count = e1->m_count = 2;
                    if (from) same_value(t, o, r);
                    else eq("set from nothing", *(void**)(o + 0xC) != nullptr, *(void**)(r + 0xC) != nullptr);
                }
            }
            if (t == 11) {
                // Made empty, over what it holds and from nothing; and from an array that holds no texts, which reads null in both.
                for (const unsigned count : {2u, 0u}) {
                    { On on(0); at<FnSetData>(0x0006B330)(o, nullptr, e0, 5, (const void*)(uintptr_t)count); }
                    { On on(1); ((AtDataStringArray*)r)->Make(e1, 5, count); }
                    compare_logs("made");
                    e0->m_count = e1->m_count = count;
                    same_value(t, o, r);
                }
                char* n0 = (char*)raw(kSize[t], 0xCD);
                char* n1 = (char*)raw(kSize[t], 0xCD);
                at<FnCtor>(kCtor[t])(n0, nullptr);
                make_value_r(t, n1);
                *(void**)(n0 + 4) = d0;
                *(void**)(n1 + 4) = d1;
                const bool c0 = guarded([&] { On on(0); at<FnCopy>(0x0006B9E0)(o, nullptr, e0, n0); });
                g_recording = false;
                const bool c1 = guarded([&] { On on(1); ((AtDataStringArray*)r)->Copy(e1, (AtDataStringArray*)n1); });
                g_recording = false;
                eq("copy of no texts: crash", c0, c1);
                eq("copy of no texts crashes", c1, true);
                compare_logs("copy of no texts");
            }
        }
    }
    // A text from another: narrow and wide, into narrow and wide, at and past the room, from a start past 0.
    g_context = "text from text";
    typedef void(__fastcall* FnTextCopy)(void*, void*, void*);
    for (const unsigned room : {1u, 3u, 8u, 9u}) {
        for (const unsigned start : {0u, 2u}) {
            for (int kinds = 0; kinds < 4; kinds++) {
                unsigned char from_o[0x20], from_r[0x20], to_o[0x20], to_r[0x20];
                char narrow[16] = "Seven c";
                unsigned short wide[16] = {'W', 'i', 'd', 'e', 0x1234, 'x', 0};
                unsigned short to_wide0[0x20], to_wide1[0x20];
                char to_narrow0[0x20], to_narrow1[0x20];
                std::memset(to_wide0, 0x33, sizeof to_wide0);
                std::memset(to_wide1, 0x33, sizeof to_wide1);
                std::memset(to_narrow0, 0x33, sizeof to_narrow0);
                std::memset(to_narrow1, 0x33, sizeof to_narrow1);
                AtText* src;
                AtText* dst0;
                AtText* dst1;
                if (kinds & 1) {
                    AtTextUnicode* u = new (from_r) AtTextUnicode;
                    u->Init(wide, 16);
                    u->m_end = 6;
                    src = u;
                } else {
                    AtTextAscii* a = new (from_r) AtTextAscii;
                    a->Init(narrow, 16);
                    a->m_end = 7;
                    src = a;
                }
                (void)from_o;
                src->m_start = start;
                if (kinds & 2) {
                    at<FnCtor>(0x0002DD90)(to_o, nullptr);
                    at<void(__fastcall*)(void*, void*, unsigned short*, unsigned)>(0x0002DDE0)(to_o, nullptr, to_wide0, room);
                    AtTextUnicode* u = new (to_r) AtTextUnicode;
                    u->Init(to_wide1, room);
                    dst1 = u;
                } else {
                    at<FnCtor>(0x0002E000)(to_o, nullptr);
                    at<void(__fastcall*)(void*, void*, char*, unsigned)>(0x0002E040)(to_o, nullptr, to_narrow0, room);
                    AtTextAscii* a = new (to_r) AtTextAscii;
                    a->Init(to_narrow1, room);
                    dst1 = a;
                }
                dst0 = (AtText*)to_o;
                dst0->m_start = dst1->m_start = 1;
                { On on(0); at<FnTextCopy>(0x00039910)(dst0, nullptr, src); }
                { On on(1); dst1->CopyFrom(src); }
                compare_logs("text from text");
                same_text("text from text", dst0, dst1);
                eq("text from text: characters", std::memcmp(to_wide0, to_wide1, sizeof to_wide0), 0);
                eq("text from text: narrow characters", std::memcmp(to_narrow0, to_narrow1, sizeof to_narrow0), 0);
            }
        }
    }
    // The string in characters of the caller's: bound, read into, over held text, and let go.
    g_context = "string in the caller's characters";
    for (const char* held : {(const char*)nullptr, "\"Held\"", "\"Held\" \"Unicode\"", "wide, holding nothing"}) {
        for (int how = 0; how < 3; how++) {
            char* o = (char*)raw(kSize[5], 0xCD);
            char* r = (char*)raw(kSize[5], 0xCD);
            at<FnCtor>(kCtor[5])(o, nullptr);
            make_value_r(5, r);
            AtDataDescriptor *d0, *d1;
            make_descriptor(d0, d1);
            if (held && held[0] == 'w') {
                *(int*)(o + 0x34) = *(int*)(r + 0x34) = 1;
            } else if (held) {
                Direct da(held), db(held);
                { On on(0); at<FnSlotRead>(slot(o, 1))(o, nullptr, d0, &da.tokenizer); }
                { On on(1); ((AtDataComponent*)r)->Read(d1, &db.tokenizer); }
                compare_logs("held");
            }
            char c0[12], c1[12];
            std::memset(c0, 0x44, sizeof c0);
            std::memset(c1, 0x44, sizeof c1);
            Direct dc("\"Read in\""), dd("\"Read in\"");
            int x = 7, y = 7;
            switch (how) {
            case 0:
                { On on(0); x = at<FnBind>(0x00069FD0)(o, nullptr, d0, c0, 6); }
                { On on(1); y = ((AtDataString*)r)->Bind(d1, c1, 6); }
                break;
            case 1:
                { On on(0); x = at<FnBindRead>(0x0006A4D0)(o, nullptr, d0, &dc.tokenizer, c0, 6); }
                { On on(1); y = ((AtDataString*)r)->BindAndRead(d1, &dd.tokenizer, c1, 6); }
                break;
            default:
                { On on(0); at<Fn0>(0x0006A1D0)(o, nullptr); x = 0; }
                { On on(1); ((AtDataString*)r)->Forget(); y = 0; }
                break;
            }
            eq("caller's: returned", x, y);
            compare_logs("caller's");
            same_value(5, o, r);
            eq("caller's: descriptor", *(void**)(o + 4) == d0, *(void**)(r + 4) == d1);
            eq("caller's: characters", std::memcmp(c0, c1, sizeof c0), 0);
            eq("caller's: bound", ((AtTextAscii*)(o + 0xC))->m_data == c0, ((AtTextAscii*)(r + 0xC))->m_data == c1);
            if (how < 2) {
                // Let go, so destroying does not free the caller's characters.
                { On on(0); at<Fn0>(0x0006A1D0)(o, nullptr); }
                { On on(1); ((AtDataString*)r)->Forget(); }
                compare_logs("caller's, let go");
                same_value(5, o, r);
            }
        }
    }
    // A descriptor from another: untyped and typed, with and without a format, and its name.
    g_context = "descriptor from descriptor";
    for (const bool typed : {false, true}) {
        for (const char* format : {(const char*)nullptr, "%d", ""}) {
            AtDataDescriptor *s0, *s1, *t0, *t1;
            make_descriptor(s0, s1);
            make_descriptor(t0, t1);
            for (AtDataDescriptor* s : {s0, s1}) {
                std::memcpy(s->m_name, "NameOf8!", 8);
                s->m_type = 9;
                s->m_flags = 4;
                s->m_count = 6;
                s->m_intFloor = -3;
                s->m_intCeiling = 77;
                *(uint32_t*)&s->m_realFloor = 0xFF800001;  // signalling: a copy through the FPU would quiet it
                *(uint32_t*)&s->m_realCeiling = 0x40490FDB;
            }
            if (format) {
                { On on(0); at<FnP>(0x0006BB70)(s0, nullptr, format); }
                { On on(1); s1->SetFormat(format); }
            }
            if (typed) {
                { On on(0); at<FnI2>(0x0006BCB0)(t0, nullptr, 2, 1); at<FnP>(0x0006BB70)(t0, nullptr, "Old"); }
                { On on(1); t1->SetType(2, 1); t1->SetFormat("Old"); }
            }
            compare_logs("descriptors made");
            int x, y;
            { On on(0); x = at<FnP>(0x0006BCE0)(t0, nullptr, s0); }
            { On on(1); y = t1->CopyFrom(s1); }
            eq("descriptor copied", x, y);
            compare_logs("descriptor copied");
            same_descriptor(t0, t1);
        }
    }
    for (int stage = 0; stage < 2; stage++) {
        AtDataDescriptor *s0, *s1, *t0, *t1;
        make_descriptor(s0, s1);
        make_descriptor(t0, t1);
        { On on(0); at<FnP>(0x0006BB70)(s0, nullptr, "F"); }
        { On on(1); s1->SetFormat("F"); }
        g_log[0].n = g_log[1].n = 0;
        if (stage == 0) {
            *(void**)(uintptr_t)0x002CB0A0 = nullptr;
            AtDataDescriptor::s_heap = nullptr;
        } else {
            g_fail_alloc = true;
        }
        const bool c0 = guarded([&] { On on(0); at<FnP>(0x0006BCE0)(t0, nullptr, s0); });
        g_recording = false;
        const bool c1 = guarded([&] { On on(1); t1->CopyFrom(s1); });
        g_recording = false;
        g_fail_alloc = false;
        eq("descriptor, none: crash", c0, c1);
        compare_logs("descriptor, none");
        at<void(__cdecl*)(void*)>(0x0006BAC0)(&g_heap_token);
        AtDataDescriptor::SetHeap(&g_heap_token);
    }
    // No heap, then no memory, for each value that allocates.
    g_context = "copies, none";
    for (int stage = 0; stage < 2; stage++) {
        for (int t = 5; t < 12; t++) {
            char* src0 = (char*)raw(kSize[t], 0xCD);
            char* src1 = (char*)raw(kSize[t], 0xCD);
            at<FnCtor>(kCtor[t])(src0, nullptr);
            make_value_r(t, src1);
            AtDataDescriptor *d0, *d1, *e0, *e1;
            make_descriptor(d0, d1);
            make_descriptor(e0, e1);
            d0->m_count = d1->m_count = 2;
            e0->m_count = e1->m_count = 2;
            Direct da(kText[t]), db(kText[t]);
            { On on(0); at<FnSlotRead>(slot(src0, 1))(src0, nullptr, d0, &da.tokenizer); }
            { On on(1); ((AtDataComponent*)src1)->Read(d1, &db.tokenizer); }
            char* o = (char*)raw(kSize[t], 0xCD);
            char* r = (char*)raw(kSize[t], 0xCD);
            at<FnCtor>(kCtor[t])(o, nullptr);
            make_value_r(t, r);
            g_log[0].n = g_log[1].n = 0;
            g_message[0][0] = g_message[1][0] = 0;
            if (stage == 0) {
                *(void**)(uintptr_t)0x002CB09C = nullptr;
                AtDataComponent::s_heap = nullptr;
                *(void**)(uintptr_t)0x002C60B0 = nullptr;
                AtIoTokenFile::s_heap = nullptr;
            } else {
                g_fail_alloc = true;
            }
            const bool w0 = guarded([&] { On on(0); at<FnV1>(slot(src0, 9))(src0, nullptr, g_writer); });
            g_recording = false;
            const bool w1 = guarded([&] { On on(1); ((AtDataComponent*)src1)->Write(g_writer); });
            g_recording = false;
            eq("write, none: crash", w0, w1);
            compare_logs("write, none");
            bool c0, c1;
            if (t == 5) {
                AtText* text1 = &((AtDataString*)src1)->m_narrow;
                void* text0 = &((AtDataString*)src0)->m_narrow;
                c0 = guarded([&] { On on(0); at<FnP2>(0x0006A250)(o, nullptr, e0, text0); });
                g_recording = false;
                c1 = guarded([&] { On on(1); ((AtDataString*)r)->CopyText(e1, text1); });
            } else {
                c0 = guarded([&] { On on(0); at<FnCopy>(kCopy[t])(o, nullptr, e0, src0); });
                g_recording = false;
                c1 = guarded([&] {
                    On on(1);
                    switch (t) {
                    case 6: ((AtDataBooleanArray*)r)->Copy(e1, (AtDataBooleanArray*)src1); break;
                    case 7: ((AtDataByteArray*)r)->Copy(e1, (AtDataByteArray*)src1); break;
                    case 8: ((AtDataIntArray*)r)->Copy(e1, (AtDataIntArray*)src1); break;
                    case 9: ((AtDataRealArray*)r)->Copy(e1, (AtDataRealArray*)src1); break;
                    case 10: ((AtDataShortNameArray*)r)->Copy(e1, (AtDataShortNameArray*)src1); break;
                    default: ((AtDataStringArray*)r)->Copy(e1, (AtDataStringArray*)src1); break;
                    }
                });
            }
            g_recording = false;
            g_fail_alloc = false;
            eq("copy, none: crash", c0, c1);
            compare_logs("copy, none");
            if (t == 11 && stage == 1) {
                // Make alone, failing from each of its requests in turn: the texts, the list, each text's characters.
                for (int fail_at = 1; fail_at <= 4; fail_at++) {
                    char* m0 = (char*)raw(kSize[t], 0xCD);
                    char* m1 = (char*)raw(kSize[t], 0xCD);
                    at<FnCtor>(kCtor[t])(m0, nullptr);
                    make_value_r(t, m1);
                    g_fail_after = fail_at;
                    const bool f0 = guarded([&] { On on(0); at<FnSetData>(0x0006B330)(m0, nullptr, e0, 4, (const void*)(uintptr_t)2); });
                    g_recording = false;
                    g_fail_after = fail_at;
                    const bool f1 = guarded([&] { On on(1); ((AtDataStringArray*)m1)->Make(e1, 4, 2); });
                    g_recording = false;
                    g_fail_after = 0;
                    g_fail_alloc = false;
                    eq("make, failing: crash", f0, f1);
                    compare_logs("make, failing");
                }
            }
            at<void(__cdecl*)(void*)>(0x00069DE0)(&g_heap_token);
            AtDataComponent::SetHeap(&g_heap_token);
            *(void**)(uintptr_t)0x002C60B0 = &g_heap_token2;
            AtIoTokenFile::s_heap = &g_heap_token2;
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
    if (pos.size() != 2) { std::printf("usage: oracle_datalookup <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete_keeping);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*)>(::operator delete), (const void*)host_delete);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*)>(::operator delete[]), (const void*)host_delete);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*, std::size_t)>(::operator delete), (const void*)host_delete_sized);
    redirect((uint32_t)(uintptr_t)static_cast<void (*)(void*, std::size_t)>(::operator delete[]), (const void*)host_delete_sized);
    redirect(0x00011240, (const void*)o_push_counting);
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
    for (uint32_t va : {0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    *(void**)(uintptr_t)0x002C60B0 = &g_heap_token2;
    AtIoTokenFile::s_heap = &g_heap_token2;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    at<void(__cdecl*)(void*)>(0x00066100)(&g_heap_token);
    at<void(__cdecl*)(void*)>(0x00069DE0)(&g_heap_token);
    at<void(__cdecl*)(void*)>(0x0006BAC0)(&g_heap_token);
    at<void(__cdecl*)(void*)>(0x0006BFC0)(&g_heap_token);
    at<void(__cdecl*)(void*)>(0x0005FFB0)(&g_heap_token);
    AtDataRegistry::SetHeap(&g_heap_token);
    AtDataRegistry::s_poolHeap = &g_heap_token;
    AtDataComponent::SetHeap(&g_heap_token);
    AtDataDescriptor::SetHeap(&g_heap_token);
    AtDataLink::SetHeap(&g_heap_token);
    AtDataRegistryNameSpaces::SetHeap(&g_heap_token);
    *(void**)(uintptr_t)0x002CB098 = &g_heap_token;
    g_heap_pad = 0x40;
    g_writer = new LogTokenizer;
    for (int t = 0; t < 12; t++) {
        char buf[0x80];
        make_value_r(t, buf);
        g_vft[t] = *(void**)buf;
        if (t == 5) {
            g_ascii_vft = *(void**)(buf + 0xC);
            g_wide_vft = *(void**)(buf + 0x20);
        }
    }
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.

    run_slots();
    run_copies();
    run_paths();
    run_create();
    run_written();

    // The real files.
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    std::vector<std::pair<std::string, std::string>> list;
    std::vector<std::string> disc;
    std::string library_file;
    while (std::fgets(line, sizeof line, index)) {
        const bool adr = std::strstr(line, ".ADR\t") != nullptr;
        const bool add = std::strstr(line, "\tLIBRARY.ADD\t") != nullptr;
        if (!adr && !add) continue;
        char* tab = std::strchr(line, '\t');
        *tab = 0;
        char* tab2 = std::strchr(tab + 1, '\t');
        *tab2 = 0;
        char* tab3 = std::strchr(tab2 + 1, '\t');
        if (tab3) *tab3 = 0;
        if (add) library_file = line;
        else {
            list.push_back({line, tab + 1});
            disc.push_back(tab2 + 1);
        }
    }
    std::fclose(index);
    auto read_file = [&](const std::string& name, std::vector<unsigned char>& data) {
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + name).c_str(), "rb") || !f) return false;
        std::fseek(f, 0, SEEK_END);
        data.resize((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        return true;
    };
    std::vector<std::vector<unsigned char>> contents(list.size());
    for (size_t i = 0; i < list.size(); i++) {
        if (!read_file(list[i].first, contents[i])) { std::printf("cannot read %s\n", list[i].first.c_str()); return 2; }
    }
    // Start-up: GENERIC.ADR, and the names LIBRARY.ADD lists, found by their paths on the disc.
    std::vector<unsigned char> library_text;
    if (library_file.empty() || !read_file(library_file, library_text)) { std::printf("cannot read LIBRARY.ADD\n"); return 2; }
    auto find_disc = [&](std::string path) -> int {
        path = cased(path, true) + ".ADR";
        for (size_t i = 0; i < disc.size(); i++) {
            if (disc[i] == path && list[i].second.find("AppInit.JAM") != std::string::npos) return (int)i;
        }
        return -1;
    };
    std::vector<std::vector<unsigned char>> library;
    std::vector<std::string> names;
    const std::string text(library_text.begin(), library_text.end());
    for (size_t q = text.find('"'); q != std::string::npos; q = text.find('"', q + 1)) {
        const size_t e = text.find('"', q + 1);
        if (e == std::string::npos) break;
        const std::string name = text.substr(q + 1, e - q - 1);
        const int i = find_disc(name);
        if (i < 0) { std::printf("LIBRARY.ADD names %s, which is not in the index\n", name.c_str()); return 2; }
        library.push_back(contents[i]);
        names.push_back(name);
        q = e;
    }
    const int generic = find_disc("Design\\Registry\\Generic");
    if (generic < 0 || library.size() != 11) { std::printf("start-up's files are not all in the index\n"); return 2; }
    run_startup(contents[generic], library, names);
    run_registries(list, contents);

    report_entries();
    std::printf("%s: %u .ADR files in %u archives read, and start-up with GENERIC.ADR and %u under it; %u registries copied; %u lookups, %u values through the slots; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files_seen, g_chains, (unsigned)library.size(), g_copies, g_lookups, g_values_read, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
