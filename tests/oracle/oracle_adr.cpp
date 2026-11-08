// Differential oracle for the design data registry, .ADR (Windows x86, local only). See docs/verification.md.
//
//   oracle_adr <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ADR file listed in index.txt, alone and each under the one before it in its archive, loaded through the file code on both sides; text written here, for each word and error;
// and the methods called directly. Both sides are whole here: no class of the registry is a double. Only the owner is cut down to the two members the loader uses, each built by its own constructor on its own side.
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

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

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
    eq("descriptor +04", o->m_unknown04, r->m_unknown04);
    eq("descriptor +08", o->m_unknown08, r->m_unknown08);
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
    AtDataRegistry* o = g_regs[0].back();
    AtDataRegistry* r = g_regs[1].back();
    g_regs[0].pop_back();
    g_regs[1].pop_back();
    { On on(0); at<Fn0>(0x00067C50)(o, nullptr); }
    { On on(1); r->~AtDataRegistry(); }
    g_graveyard.push_back(o);
    g_graveyard.push_back(r);
    compare_logs("destroy");
}

static void bury() {
    for (void* p : g_graveyard) std::free(p);
    g_graveyard.clear();
}

// ---- text written here --------------------------------------------------------------------------------------------------

static std::string link(const std::string& body) { return "DataLink { " + body + " }\n"; }

static std::string one_of(const char* type, unsigned n) { return "DataComponentTypes[1]\n{\n DataComponent { Type " + std::string(type) + " NumComponents " + std::to_string(n) + " }\n}\n"; }

static const char* const kTypes[12] = {"Empty", "Boolean", "Int", "Real", "ShortName", "String", "BooleanArray", "ByteArray", "IntArray", "RealArray", "ShortNameArray", "StringArray"};

static std::string all_types(unsigned n) {
    std::string s = "DataComponentTypes[12]\n{\n";
    for (const char* t : kTypes) s += " DataComponent { Type " + std::string(t) + " NumComponents " + std::to_string(n) + " }\n";
    return s + "}\n";
}

static std::string registry(size_t links, const std::string& body) { return "DataRegistry[" + std::to_string(links) + "]\n{\n" + body + "}\n"; }

// A whole file of one link, of a pool of two.
static std::string with_type(const char* type, const std::string& body) { return one_of(type, 2) + registry(1, link("Name \"A\" Type " + std::string(type) + " " + body)); }

static std::vector<std::string> written() {
    std::vector<std::string> t;
    // Nothing in it; a name space or none.
    t.push_back("DataComponentTypes[0]\n{\n}\nDataRegistry[0]\n{\n}\n");
    for (const char* ns : {"\"Space\"", "\"\"", "\"LongerThanEight\"", "\"EightChr\""}) t.push_back("NameSpace " + std::string(ns) + "\nDataComponentTypes[0]\n{\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("NameSpace 5\nDataComponentTypes[0]\n{\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("NameSpace \"A\" NameSpace \"B\"\n[0]\n{\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataRegistry[0]\n{\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[0]\n{\n}\nDataLink[0]\n{\n}\n");
    t.push_back("datacomponenttypes[0]\n{\n}\ndataregistry[0]\n{\n}\n");
    // Each word of the first list out of place.
    t.push_back("DataComponentTypes[1]\n{\n Data { Type Int NumComponents 1 }\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent { Name Int NumComponents 1 }\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent { Type Int Name 1 }\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent { Type Name NumComponents 3 }\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent { Type NameSpace NumComponents 3 }\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent Type Int NumComponents 3 }\n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent { Type Int NumComponents 3 \n}\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[1]\n{\n DataComponent { Type Int NumComponents 3 }\nDataRegistry[0]\n{\n}\n");
    t.push_back("DataComponentTypes[2]\n{\n DataComponent { Type Int NumComponents 3 }\n DataComponent { Type Int NumComponents 2 }\n}\nDataRegistry[0]\n{\n}\n");
    // Each type declared and not used; declared with none.
    for (const char* type : kTypes) {
        t.push_back(one_of(type, 3) + "DataRegistry[0]\n{\n}\n");
        t.push_back(one_of(type, 0) + "DataRegistry[0]\n{\n}\n");
    }
    t.push_back(all_types(2) + "DataRegistry[0]\n{\n}\n");
    // Each type read, at the middle of its bounds and past each end.
    t.push_back(with_type("Empty", "Data"));
    for (const char* v : {"0", "1", "5", "-3"}) t.push_back(with_type("Boolean", std::string("Data ") + v));
    for (const char* v : {"7", "50", "-50"}) t.push_back(with_type("Int", std::string("Floor -10 Ceiling 10 Data ") + v));
    t.push_back(with_type("Int", "Data 99999"));
    for (const char* v : {"1.0", "9.5", "-9.5", "0.5", "1.5"}) t.push_back(with_type("Real", std::string("Floor 0.5 Ceiling 1.5 Data ") + v));
    t.push_back(with_type("Real", "Data 4000.0"));
    t.push_back(with_type("Real", "Data 5"));
    for (const char* v : {"\"Short\"", "\"\"", "\"EightChr\"", "\"LongerThanEight\"", "5"}) t.push_back(with_type("ShortName", std::string("Data ") + v));
    for (const char* v : {"\"Text\"", "\"\"", "\"Text\" \"Unicode\"", "\"Text\" \"unicode\"", "\"Text\" \"Other\"", "\"Text\" 5", "5"}) {
        t.push_back(with_type("String", std::string("Data ") + v));
        t.push_back(with_type("String", std::string("Ceiling 40 Data ") + v));
        t.push_back(with_type("String", std::string("Ceiling 1 Data ") + v));
    }
    t.push_back(with_type("String", "Floor 3 Data \"Text\""));
    t.push_back(with_type("BooleanArray", "NumElements 4 Data 1 0 5 -2"));
    t.push_back(with_type("ByteArray", "NumElements 4 Data 1 255 300 -1"));
    t.push_back(with_type("IntArray", "NumElements 4 Floor -2 Ceiling 2 Data 1 -5 5 2"));
    t.push_back(with_type("IntArray", "NumElements 2 Data 1 2"));
    t.push_back(with_type("RealArray", "NumElements 4 Floor -2.0 Ceiling 2.0 Data 1.0 -5.0 5.0 2.0"));
    t.push_back(with_type("RealArray", "NumElements 2 Data 1.5 2.5"));
    t.push_back(with_type("ShortNameArray", "NumElements 3 Data \"A\" \"EightChr\" \"LongerThanEight\""));
    t.push_back(with_type("ShortNameArray", "Data \"One\""));
    t.push_back(with_type("StringArray", "NumElements 3 Data \"A\" \"Longer one\" \"\""));
    t.push_back(with_type("StringArray", "NumElements 2 Ceiling 30 Data \"A\" \"B\""));
    t.push_back(with_type("StringArray", "NumElements 2 Ceiling 1 Data \"Long\" \"B\""));
    for (const char* type : {"BooleanArray", "ByteArray", "IntArray", "RealArray", "ShortNameArray", "StringArray"}) t.push_back(with_type(type, "NumElements 0 Data"));
    t.push_back(with_type("ShortNameArray", "NumElements 2 Ceiling 5 Data \"AB\" \"CD\""));
    t.push_back(with_type("ShortName", "Ceiling 5 Data \"AB\""));
    t.push_back(with_type("StringArray", "NumElements 2 Ceiling 5 Floor 9 Data \"AB\" \"CD\""));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Type NameSpace Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Type WillAcceptParentData Data 2")));
    t.push_back(with_type("IntArray", "NumElements 2 Data 1"));   // Then a word where a number should be.
    // All the words of a link, in other orders.
    t.push_back(with_type("Real", "Floor 1 Ceiling 2 Data 1.5"));
    t.push_back(one_of("Real", 1) + registry(1, link("Floor 2 Type Real Ceiling 3.0 Name \"A\" Data 9.0")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Name \"B\" Type Int Data 1")));
    t.push_back(one_of("Int", 1) + registry(1, link("Type Int Data 1 Name \"Late\"")));
    t.push_back(one_of("Int", 1) + registry(1, link("Type Int Data 1")));
    t.push_back(one_of("Int", 2) + registry(1, link("Name \"A\" Type Int Data 1 Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 1 Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int FormatString \"%d\" Data 1")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int FormatString \"%d\" FormatString \"%x\" Data 1")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 1 FormatString \"Late\"")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 1 WillAcceptParentData")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 1 WillAcceptParentData FormatString \"%d\"")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int NumElements 7 Data 1")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Flags 1 Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Empty Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int NameSpace Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int DataLink Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int \"Stray\" Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Type Name Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Type 5 Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, "DataLink \"Skipped\" 5 Name { Name \"A\" Type Int Data 2 }\n"));
    t.push_back(one_of("Int", 1) + registry(1, "Name { Name \"A\" Type Int Data 2 }\n"));
    t.push_back(one_of("Int", 1) + registry(1, link("Name 5 Type Int Data 2")));
    t.push_back(one_of("Int", 1) + "DataLink[1]\n{\n" + link("Name \"A\" Type Int Data 2") + "}\n");
    // No type yet, no data yet: crashes, on both sides or neither.
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\"")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Data 2")));
    t.push_back(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int WillAcceptParentData Data 1")));
    t.push_back(one_of("Int", 1) + registry(2, link("Name \"A\" Type Int Data 1") + link("Name \"B\"")));
    // The type carries on; the type is changed under a value.
    t.push_back(one_of("Int", 3) + registry(3, link("Name \"A\" Type Int Floor 0 Ceiling 1 Data 5") + link("Name \"B\" Data 6") + link("Name \"C\" Ceiling 3 Data 7")));
    t.push_back(all_types(1) + registry(2, link("Name \"A\" Type Real Data 2.5") + link("Name \"B\" Type Int Data 6")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data 6 Type Boolean")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data -6 Type Boolean")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Real Floor 1.0 Ceiling 2.0 Data 1.5 Type Int")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Floor 1 Ceiling 2 Data 5 Type Real")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data 5 Type IntArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data 5 Type BooleanArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data 5 Type ByteArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data 5 Type RealArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Data 5 Type Empty")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type IntArray NumElements 2 Data 5 6 Type ShortName")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type IntArray NumElements 2 Data 5 6 Type String")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type IntArray NumElements 2 Data 5 6 Type ShortNameArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type IntArray NumElements 2 Data 5 6 Type StringArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type IntArray NumElements 2 Floor 0 Ceiling 1 Data 5 -6 Type BooleanArray")));
    t.push_back(all_types(1) + registry(1, link("Name \"A\" Type Int Floor 1 Ceiling 2 Type Real Data 5.0")));
    // More than was declared, and a type never declared: blocks chain on.
    t.push_back(one_of("Int", 1) + registry(3, link("Name \"A\" Type Int Data 1") + link("Name \"B\" Type Int Data 2") + link("Name \"C\" Type Int Data 3")));
    t.push_back("DataComponentTypes[0]\n{\n}\n" + registry(1, link("Name \"A\" Type Int Data 1")));
    {
        std::string body;
        for (int i = 0; i < 9; i++) body += link("Name \"N" + std::to_string(i) + "\" Type RealArray NumElements 2 Data 1.0 2.0");
        t.push_back("DataComponentTypes[0]\n{\n}\n" + registry(9, body));
        std::string strings;
        for (int i = 0; i < 6; i++) strings += link("Name \"S" + std::to_string(i) + "\" Type String Data \"T" + std::to_string(i) + "\"");
        t.push_back(one_of("String", 2) + registry(6, strings));
    }
    // Paths, in this registry. See kParentText for the ones that reach a parent.
    const std::string tree = link("Name \"A\" Type Empty Data") + link("Name \"B\" Type Empty Parent \"A\\\" Data") + link("Name \"C\" Type Int Parent \"A\\B\\\" Data 1");
    const std::string decl = "DataComponentTypes[2]\n{\n DataComponent { Type Empty NumComponents 2 }\n DataComponent { Type Int NumComponents 8 }\n}\n";
    t.push_back(decl + registry(3, tree));
    for (const char* parent : {"A", "A\\", "A\\B", "A\\B\\", "A\\B\\C", "a", "A\\b", "B", "B\\", "C", "Nowhere", "A\\Nowhere", "Nowhere\\A", "\\", "\\A", "A\\\\B", "", "AB", "A\\B\\C\\D"}) {
        t.push_back(decl + registry(4, tree + link("Name \"D\" Type Int Parent \"" + std::string(parent) + "\" Data 2")));
    }
    t.push_back(decl + registry(4, tree + link("Name \"D\" Parent \"A\\\" Parent \"Nowhere\" Type Int Data 2")));
    t.push_back(decl + registry(4, tree + link("Name \"D\" Parent \"Nowhere\" Parent \"A\\\" Type Int Data 2")));
    t.push_back(decl + registry(4, tree + link("Name \"D\" Parent 5 Type Int Data 2")));
    t.push_back(decl + registry(3, link("Type Empty Data") + link("Name \"B\" Type Int Parent \"Nowhere\" Data 2") + link("Name \"C\" Type Int Parent \"B\" Data 2")));
    // A name holding a backslash is found whole, ignoring case, after the tree has failed; a nameless or empty-named link is passed over by that search, which the tree's cannot do.
    t.push_back(decl + registry(2, link("Name \"X\\Y\" Type Int Data 1") + link("Name \"Z\" Type Int Parent \"x\\y\" Data 2")));
    t.push_back(decl + registry(2, link("Name \"\" Type Int Data 1") + link("Name \"Z\" Type Int Parent \"\" Data 2")));
    t.push_back(decl + registry(2, link("Type Int Data 1") + link("Name \"Z\" Type Int Parent \"Q\" Data 2")));
    // Past the end of an earlier link's own pool, in the last pool, in a chained block.
    t.push_back("DataComponentTypes[0]\n{\n}\n" + registry(6, link("Name \"A\" Type StringArray NumElements 1 Data \"T\"") + link("Name \"B\" Type StringArray NumElements 1 Data \"T\"") + link("Name \"C\" Type StringArray NumElements 1 Data \"T\"") +
                                                            link("Name \"D\" Type StringArray NumElements 1 Data \"T\"") + link("Name \"E\" Type StringArray NumElements 1 Data \"T\"") + link("Name \"Z\" Type Int Parent \"e\" Data 2")));
    return t;
}

// What the second of a pair hangs under.
static const char* const kParentText =
    "NameSpace \"Base\"\n"
    "DataComponentTypes[3]\n{\n DataComponent { Type Empty NumComponents 2 }\n DataComponent { Type Int NumComponents 2 }\n DataComponent { Type RealArray NumComponents 2 }\n}\n"
    "DataRegistry[4]\n{\n"
    " DataLink { Name \"A\" Type Empty Data }\n"
    " DataLink { Name \"B\" Type Empty Parent \"A\\\" Data }\n"
    " DataLink { Name \"C\" Type Int Parent \"A\\B\\\" Data 1 }\n"
    " DataLink { Name \"D\" Type RealArray NumElements 2 Parent \"A\\\" Data 1.0 2.0 WillAcceptParentData }\n"
    "}\n";

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static unsigned g_loaded;

// One text through everything a registry does with one, on its own and under a parent.
static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, unsigned spaces) {
    g_context = context;
    g_fill = fill;
    make_owners(spaces);
    // On its own, into a name space's out word.
    file_is(data);
    make_registries(fill);
    if (load(0, nullptr, nullptr, true, "load")) {
        g_loaded++;
        // Over what is there: the file name just set is what the clear frees, and the owner is forgotten.
        if (load(0, nullptr, nullptr, false, "load again")) {
            clear_both(0, "clear");
            clear_both(0, "clear again");
        } else {
            g_regs[0].clear();
            g_regs[1].clear();
        }
    } else {
        g_regs[0].clear();
        g_regs[1].clear();
    }
    if (!g_regs[0].empty()) destroy_last();
    // Under a parent, destroyed in the order a game would, the child first.
    const std::vector<unsigned char> parent_data = bytes(kParentText);
    file_is(parent_data);
    make_registries(fill);
    if (load(0, nullptr, nullptr, true, "the parent")) {
        file_is(data);
        make_registries(fill);
        if (load(1, g_regs[0][0], g_regs[1][0], true, "the child")) {
            destroy_last();
            destroy_last();
        } else {
            g_regs[0].clear();
            g_regs[1].clear();
        }
    } else {
        g_regs[0].clear();
        g_regs[1].clear();
    }
    compare_logs("after");
    g_regs[0].clear();
    g_regs[1].clear();
    free_owners();
    bury();
}

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

static void run_descriptor() {
    g_context = "descriptor";
    g_fill = 0xCD;
    AtDataDescriptor* o;
    AtDataDescriptor* r;
    make_descriptor(o, r);
    same_descriptor(o, r);
    { On on(0); eq("type", at<FnI2>(0x0006BCB0)(o, nullptr, 3, 2), 0); }
    { On on(1); r->SetType(3, 2); }
    same_descriptor(o, r);
    { On on(0); eq("real bounds", at<FnReal2>(0x0006BB40)(o, nullptr, 1.5f, 2.5f), 0); }
    { On on(1); eq("real bounds", r->SetRealBounds(1.5f, 2.5f), 0); }
    same_descriptor(o, r);
    { On on(0); eq("int bounds", at<FnI2>(0x0006BB20)(o, nullptr, 7, 9), 0); }
    { On on(1); eq("int bounds", r->SetIntBounds(7, 9), 0); }
    same_descriptor(o, r);
    int lo[2] = {1, 1}, hi[2] = {1, 1};
    { On on(0); eq("get", at<FnP2>(0x0006BAD0)(o, nullptr, &lo[0], &hi[0]), 0); }
    { On on(1); eq("get", r->GetIntBounds(&lo[1], &hi[1]), 0); }
    eq("floor", lo[0], lo[1]);
    eq("ceiling", hi[0], hi[1]);
    for (const char* f : {"%d", "longer format", ""}) {
        { On on(0); eq("format", at<FnP>(0x0006BB70)(o, nullptr, f), 0); }
        { On on(1); eq("format", r->SetFormat(f), 0); }
        same_descriptor(o, r);
        compare_logs("format");
    }
    // Typed already, so typing clears: format and bounds.
    { On on(0); at<FnI2>(0x0006BCB0)(o, nullptr, 8, 5); }
    { On on(1); r->SetType(8, 5); }
    same_descriptor(o, r);
    compare_logs("type again");
    // A type with no reals.
    { On on(0); eq("real bounds refused", at<FnReal2>(0x0006BB40)(o, nullptr, 1.5f, 2.5f), r->SetRealBounds(1.5f, 2.5f)); }
    same_descriptor(o, r);
    { On on(0); at<FnI2>(0x0006BCB0)(o, nullptr, 9, 5); }
    { On on(1); r->SetType(9, 5); }
    { On on(0); eq("real array's bounds", at<FnReal2>(0x0006BB40)(o, nullptr, 3.5f, 4.5f), r->SetRealBounds(3.5f, 4.5f)); }
    same_descriptor(o, r);
    { On on(0); at<FnP>(0x0006BB70)(o, nullptr, "gone"); }
    { On on(1); r->SetFormat("gone"); }
    o->m_unknown04 = r->m_unknown04 = 5;
    o->m_unknown08 = r->m_unknown08 = 6;
    { On on(0); at<Fn0>(0x0006BC60)(o, nullptr); }
    { On on(1); r->Clear(); }
    same_descriptor(o, r);
    compare_logs("clear");
    // No heap, no memory.
    *(void**)(uintptr_t)0x002CB0A0 = nullptr;
    AtDataDescriptor::s_heap = nullptr;
    { On on(0); at<FnP>(0x0006BB70)(o, nullptr, "f"); }
    { On on(1); r->SetFormat("f"); }
    same_descriptor(o, r);
    compare_logs("no heap");
    at<void(__cdecl*)(void*)>(0x0006BAC0)(&g_heap_token);
    AtDataDescriptor::SetHeap(&g_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002CB0A0, (uintptr_t)AtDataDescriptor::s_heap);
    g_fail_alloc = true;
    const bool c0 = guarded([&] { On on(0); at<FnP>(0x0006BB70)(o, nullptr, "f"); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); r->SetFormat("f"); });
    g_recording = false;
    g_fail_alloc = false;
    eq("crash", c0, c1);
    compare_logs("no memory");
    o->m_format = r->m_format = nullptr;
    { On on(0); at<FnP>(0x0006BB70)(o, nullptr, "last"); }
    { On on(1); r->SetFormat("last"); }
    { On on(0); at<Fn0>(0x0006BFB0)(o, nullptr); }
    { On on(1); r->~AtDataDescriptor(); }
    same_descriptor(o, r);
    compare_logs("destroyed");
}

static void same_links(AtDataLink* const* a, AtDataLink* const* b, int n, const char* what) {
    Ids ids[2];
    for (int i = 0; i < n; i++) {
        ids[0].add(a[i]);
        ids[1].add(b[i]);
    }
    for (int i = 0; i < n; i++) same_link(ids, a[i], b[i]);
    compare_logs(what);
}

static void run_link() {
    g_context = "link";
    g_fill = 0xCD;
    enum { N = 6 };
    AtDataLink* a[N];
    AtDataLink* b[N];
    const auto build = [&](unsigned char fill) {
        for (int i = 0; i < N; i++) {
            a[i] = (AtDataLink*)raw(sizeof(AtDataLink), fill);
            b[i] = (AtDataLink*)raw(sizeof(AtDataLink), fill);
            at<FnCtor>(0x0006BFD0)(a[i], nullptr);
            new (b[i]) AtDataLink;
        }
        // 0 over 1, 2, 3; 3 over 4; 5 alone.
        static const char* const names[N] = {"Zero", "One", nullptr, "", "Four", "Five"};
        static const int parents[N] = {-1, 0, 0, 0, 3, -1};
        for (int i = 0; i < N; i++) {
            AtDataLink* pa = parents[i] < 0 ? nullptr : a[parents[i]];
            AtDataLink* pb = parents[i] < 0 ? nullptr : b[parents[i]];
            { On on(0); eq("set", at<FnP3>(0x0006C160)(a[i], nullptr, names[i], nullptr, pa), 0); }
            { On on(1); eq("set", b[i]->Set(names[i], nullptr, pb), 0); }
        }
        same_links(a, b, N, "built");
    };
    build(0xCD);
    // Set again: it leaves where it was, and goes last under the new parent.
    { On on(0); at<FnP3>(0x0006C160)(a[2], nullptr, "Again", nullptr, a[5]); }
    { On on(1); b[2]->Set("Again", nullptr, b[5]); }
    same_links(a, b, N, "set again");
    // Out of each place among its siblings: not there at all, the first, the last, the middle.
    for (int i : {5, 1, 3, 4, 2, 0}) {
        { On on(0); at<Fn0>(0x0006C030)(a[i], nullptr); }
        { On on(1); b[i]->Detach(); }
        same_links(a, b, N, "detach");
    }
    build(0x33);
    { On on(0); at<Fn0>(0x0006C030)(a[2], nullptr); }
    { On on(1); b[2]->Detach(); }
    same_links(a, b, N, "detach the middle");
    // A parent pointing at someone who does not list it.
    a[5]->m_parent = a[0];
    b[5]->m_parent = b[0];
    { On on(0); at<Fn0>(0x0006C030)(a[5], nullptr); }
    { On on(1); b[5]->Detach(); }
    same_links(a, b, N, "not listed");
    a[5]->m_parent = a[5];
    b[5]->m_parent = b[5];
    { On on(0); at<Fn0>(0x0006C030)(a[5], nullptr); }
    { On on(1); b[5]->Detach(); }
    same_links(a, b, N, "no children at all");
    // Attach, to nothing and to something: it does not leave what it had.
    { On on(0); at<FnP>(0x0006C000)(a[5], nullptr, nullptr); }
    { On on(1); b[5]->AttachTo(nullptr); }
    same_links(a, b, N, "attach to none");
    { On on(0); at<FnP>(0x0006C000)(a[5], nullptr, a[1]); }
    { On on(1); b[5]->AttachTo(b[1]); }
    same_links(a, b, N, "attach, first child");
    { On on(0); at<FnP>(0x0006C000)(a[2], nullptr, a[1]); }
    { On on(1); b[2]->AttachTo(b[1]); }
    same_links(a, b, N, "attach, second child");
    // Clearing a parent: the children keep their siblings.
    { On on(0); at<Fn0>(0x0006C110)(a[0], nullptr); }
    { On on(1); b[0]->Clear(); }
    same_links(a, b, N, "clear");
    // A child of what was cleared: no parent to detach from, and still a sibling and a value, which only its own clear takes.
    a[1]->m_value = (AtDataComponent*)a[1];
    b[1]->m_value = (AtDataComponent*)a[1];
    a[1]->m_sibling = a[3];
    b[1]->m_sibling = b[3];
    a[1]->m_parent = nullptr;
    b[1]->m_parent = nullptr;
    { On on(0); at<Fn0>(0x0006C110)(a[1], nullptr); }
    { On on(1); b[1]->Clear(); }
    eq("cleared: value", (uintptr_t)a[1]->m_value, (uintptr_t)b[1]->m_value);
    eq("cleared: sibling", (uintptr_t)a[1]->m_sibling, (uintptr_t)b[1]->m_sibling);
    compare_logs("cleared, orphan");
    // Not in use, with a name left in the word: a null name replaces it.
    a[1]->m_name = b[1]->m_name = (char*)a[1];
    a[1]->m_inUse = b[1]->m_inUse = 0;
    { On on(0); at<FnP3>(0x0006C160)(a[1], nullptr, nullptr, nullptr, nullptr); }
    { On on(1); b[1]->Set(nullptr, nullptr, nullptr); }
    eq("null name", (uintptr_t)a[1]->m_name, (uintptr_t)b[1]->m_name);
    compare_logs("null name");
    // Its value's registry, through the slot.
    AtDataInt rvalue;
    rvalue.m_registry = (AtDataRegistry*)&rvalue;
    a[3]->m_value = b[3]->m_value = &rvalue;
    eq("registry of a value", (uintptr_t)at<FnRet0>(0x0006BFF0)(a[3], nullptr), (uintptr_t)b[3]->Registry());
    eq("registry of none", (uintptr_t)at<FnRet0>(0x0006BFF0)(a[4], nullptr), (uintptr_t)b[4]->Registry());
    a[3]->m_value = b[3]->m_value = nullptr;
    // No heap, no memory, on the name.
    *(void**)(uintptr_t)0x002CB0A4 = nullptr;
    AtDataLink::s_heap = nullptr;
    { On on(0); at<FnP3>(0x0006C160)(a[4], nullptr, "Name", nullptr, nullptr); }
    { On on(1); b[4]->Set("Name", nullptr, nullptr); }
    same_links(a, b, N, "no heap");
    at<void(__cdecl*)(void*)>(0x0006BFC0)(&g_heap_token);
    AtDataLink::SetHeap(&g_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002CB0A4, (uintptr_t)AtDataLink::s_heap);
    g_fail_alloc = true;
    const bool c0 = guarded([&] { On on(0); at<FnP3>(0x0006C160)(a[3], nullptr, "Name", nullptr, nullptr); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); b[3]->Set("Name", nullptr, nullptr); });
    g_recording = false;
    g_fail_alloc = false;
    eq("crash", c0, c1);
    compare_logs("no memory");
    a[3]->m_name = b[3]->m_name = nullptr;
    // Through the destructor.
    { On on(0); at<Fn0>(0x0006C240)(a[4], nullptr); }
    { On on(1); b[4]->~AtDataLink(); }
    same_links(a, b, N, "destroyed");
}

static void run_root() {
    g_context = "root";
    make_owners(4);
    make_registries(0xCD);
    AtDataRegistry* o = g_regs[0][0];
    AtDataRegistry* r = g_regs[1][0];
    eq("root's registry", at<FnRet0>(0x0010F4C0)(&o->m_root, nullptr) == o, r->m_root.Registry() == r);
    same_all("constructed");
    // Reset frees nothing, and leaves what it does not know.
    o->m_unknown04 = r->m_unknown04 = 4;
    o->m_unknown30 = r->m_unknown30 = 5;
    o->m_componentTypes = r->m_componentTypes = 6;
    o->m_next = r->m_next = nullptr;
    { On on(0); at<Fn0>(0x00066780)(o, nullptr); }
    { On on(1); r->Reset(); }
    same_all("reset");
    // A clear when not loaded takes +38 only.
    o->m_unknown38 = at<void*(__cdecl*)(unsigned)>(0x000111F0)(4);
    r->m_unknown38 = operator new(4);
    o->m_owner = g_own[0];
    r->m_owner = g_own[1];
    clear_both(0, "clear of none loaded");
    o->m_owner = r->m_owner = nullptr;
    g_regs[0].clear();
    g_regs[1].clear();
    // With the delete, loaded and not.
    {
        const std::vector<unsigned char> data = bytes(one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 1")));
        for (int loaded = 0; loaded < 2; loaded++) {
            file_is(data);
            make_registries(0xCD);
            if (loaded) load(0, nullptr, nullptr, false, "to delete");
            AtDataRegistry* x = g_regs[0][0];
            AtDataRegistry* y = g_regs[1][0];
            { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x000687B0)(x, nullptr, 0), (uintptr_t)x); }
            { On on(1); y->~AtDataRegistry(); }
            compare_logs("destroyed, no delete");
            g_regs[0].clear();
            g_regs[1].clear();
            std::free(x);
            std::free(y);
        }
        make_registries(0xCD);
        AtDataRegistry* x = g_regs[0][0];
        g_regs[0].clear();
        AtDataRegistry* y = g_regs[1][0];
        g_regs[1].clear();
        { On on(0); at<FnDel>(0x000687B0)(x, nullptr, 1); }
        { On on(1); delete y; }
        compare_logs("deleted");
    }
    // Not loaded, with something at +38: the destructor leaves it, and frees nothing.
    {
        make_registries(0xCD);
        AtDataRegistry* x = g_regs[0][0];
        AtDataRegistry* y = g_regs[1][0];
        g_regs[0].clear();
        g_regs[1].clear();
        x->m_unknown38 = y->m_unknown38 = x;
        { On on(0); at<Fn0>(0x00067C50)(x, nullptr); }
        { On on(1); y->~AtDataRegistry(); }
        compare_logs("destroyed with +38");
        std::free(x);
        std::free(y);
    }
    // The root's forms, on their own.
    AtDataLinkRoot* ra = (AtDataLinkRoot*)raw(sizeof(AtDataLinkRoot), 0xCD);
    AtDataLinkRoot* rb = (AtDataLinkRoot*)raw(sizeof(AtDataLinkRoot), 0xCD);
    at<FnCtor>(0x0006BFD0)(ra, nullptr);
    *(uint32_t*)ra = 0x0027760C;
    new (rb) AtDataLinkRoot;
    { On on(0); at<Fn0>(0x00066750)(ra, nullptr); }
    { On on(1); rb->~AtDataLinkRoot(); }
    eq("vftable after", *(uint32_t*)ra == 0x00277940, 1);
    compare_logs("root");
    ra = (AtDataLinkRoot*)raw(sizeof(AtDataLinkRoot), 0xCD);
    rb = new AtDataLinkRoot;
    at<FnCtor>(0x0006BFD0)(ra, nullptr);
    *(uint32_t*)ra = 0x0027760C;
    { On on(0); at<FnDel>(0x00066760)(ra, nullptr, 1); }
    { On on(1); delete rb; }
    compare_logs("root deleted");
    free_owners();
}

static const std::string kAllLoaded() {
    std::string body;
    for (const char* b : {"Name \"Empty\" Type Empty Data", "Name \"Bool\" Type Boolean Data 1", "Name \"Int\" Type Int Floor 0 Ceiling 9 Data 3", "Name \"Real\" Type Real Floor 0.5 Ceiling 1.5 Data 1.0",
                          "Name \"Short\" Type ShortName Data \"Sn\"", "Name \"Text\" Type String Data \"Words\"", "Name \"BoolA\" Type BooleanArray NumElements 2 Data 1 0",
                          "Name \"ByteA\" Type ByteArray NumElements 2 Data 1 2", "Name \"IntA\" Type IntArray NumElements 2 Floor 0 Ceiling 9 Data 1 2", "Name \"RealA\" Type RealArray NumElements 2 Floor 0.5 Ceiling 1.5 Data 1.0 1.25",
                          "Name \"ShortA\" Type ShortNameArray NumElements 2 Data \"P\" \"Q\"", "Name \"TextA\" Type StringArray NumElements 2 Data \"M\" \"N\""}) {
        body += link(b);
    }
    return all_types(2) + registry(12, body);
}

static void run_pool() {
    g_context = "pool";
    g_fill = 0xCD;
    make_owners(4);
    make_registries(0xCD);
    const std::vector<unsigned char> data = bytes(kAllLoaded());
    file_is(data);
    if (load(0, nullptr, nullptr, true, "all types")) {
        AtDataRegistry* o = g_regs[0][0];
        AtDataRegistry* r = g_regs[1][0];
        // Each block's elements, past them, and in a chain.
        for (int p = 0; p < AtDataRegistry::kTypeCount; p++) {
            for (unsigned i : {0u, 1u, 2u, 9u}) {
                const char* x = (const char*)at<FnRetI>(0x00066110)(&o->m_pools[p], nullptr, i);
                const char* y = (const char*)r->m_pools[p].ElementAt(i);
                eq("element", x ? x - (const char*)o->m_pools[p].m_values : -1, y ? y - (const char*)r->m_pools[p].m_values : -1);
            }
        }
        // A type no block has.
        const unsigned kept0 = o->m_pools[2].m_type;
        o->m_pools[2].m_type = r->m_pools[2].m_type = 12;
        eq("no such type", (uintptr_t)at<FnRetI>(0x00066110)(&o->m_pools[2], nullptr, 0), (uintptr_t)r->m_pools[2].ElementAt(0));
        o->m_pools[2].m_type = r->m_pools[2].m_type = kept0;
        // By name, both ways of comparing.
        Ids ids[2] = {ids_of(0), ids_of(1)};
        for (int p = 0; p < AtDataRegistry::kTypeCount; p++) {
            for (const char* n : {"Empty", "EMPTY", "Int", "int", "TextA", "texta", "", "Nope"}) {
                for (int exact : {0, 1}) {
                    eq("by name", ids[0].of(at<FnRetPI>(0x00066A70)(&o->m_pools[p], nullptr, n, exact)), ids[1].of(r->m_pools[p].FindByName(n, exact)));
                }
            }
        }
        // Names, part by part.
        static const char* const names[] = {"A", "a", "", "A\\", "A\\B", "AB", "\\", "\\A", "Ab"};
        for (const char* x : names) {
            for (const char* y : names) {
                eq("name matches", at<FnP2>(0x00066800)(o, nullptr, x, y), r->NameMatches(x, y));
            }
        }
        // Through the tree, under nothing and under a link.
        for (const char* n : {"Int", "int", "Int\\More", "TextA", "Nope", ""}) {
            eq("child", ids[0].of(at<FnRetPP>(0x00067980)(o, nullptr, n, nullptr)), ids[1].of(r->FindChild(n, nullptr)));
            eq("child of the first", ids[0].of(at<FnRetPP>(0x00067980)(o, nullptr, n, &o->m_pools[0].m_links[0])), ids[1].of(r->FindChild(n, &r->m_pools[0].m_links[0])));
            eq("child of the root", ids[0].of(at<FnRetPP>(0x00067980)(o, nullptr, n, &o->m_root)), ids[1].of(r->FindChild(n, &r->m_root)));
        }
        // Clamp by hand, past each end, by each type's own rule, on each type's own.
        for (int t = 0; t < 14; t++) {
            for (const float f : {-1000.0f, 1.0f, 1000.0f}) {
                if (t >= 1 && t <= 3) {
                    AtDataComponent* v0 = (AtDataComponent*)at<FnRetI>(0x00066110)(&o->m_pools[t], nullptr, 0);
                    AtDataComponent* v1 = r->m_pools[t].ElementAt(0);
                    const int as_int = (int)f;
                    if (t == 3) { *(float*)((char*)v0 + 0xC) = f; *(float*)((char*)v1 + 0xC) = f; }
                    else { *(int*)((char*)v0 + 0xC) = as_int; *(int*)((char*)v1 + 0xC) = as_int; }
                }
                if (t == 6 || t == 8 || t == 7 || t == 9) {
                    AtDataComponent* v0 = (AtDataComponent*)at<FnRetI>(0x00066110)(&o->m_pools[t], nullptr, 0);
                    AtDataComponent* v1 = r->m_pools[t].ElementAt(0);
                    if (t == 9) { (*(float**)((char*)v0 + 0xC))[1] = f; (*(float**)((char*)v1 + 0xC))[1] = f; }
                    else if (t != 7) { (*(int**)((char*)v0 + 0xC))[1] = (int)f; (*(int**)((char*)v1 + 0xC))[1] = (int)f; }
                }
                const int p = t < 12 ? t : 0;
                { On on(0); at<FnIP>(0x00067A60)(o, nullptr, t, at<FnRetI>(0x00066110)(&o->m_pools[p], nullptr, 0)); }
                { On on(1); r->Clamp(t, r->m_pools[p].ElementAt(0)); }
                same_all("clamp");
            }
        }
        // A real at the very bounds and beyond numbers.
        {
            AtDataComponent* v0 = (AtDataComponent*)at<FnRetI>(0x00066110)(&o->m_pools[3], nullptr, 0);
            AtDataComponent* v1 = r->m_pools[3].ElementAt(0);
            for (const uint32_t bits : {0x3F000000u, 0x3FC00000u, 0x7FC00000u, 0xFFC00000u, 0x7F800000u, 0xFF800000u}) {
                *(uint32_t*)((char*)v0 + 0xC) = bits;
                *(uint32_t*)((char*)v1 + 0xC) = bits;
                { On on(0); at<FnIP>(0x00067A60)(o, nullptr, 3, v0); }
                { On on(1); r->Clamp(3, v1); }
                same_all("clamp, odd real");
            }
        }
        // A new block by hand, and into it.
        AtDataComponentPool& po = o->m_pools[2];
        AtDataComponentPool& pr = r->m_pools[2];
        { On on(0); at<Fn0>(0x000666E0)(&po, nullptr); }
        { On on(1); pr.Grow(); }
        same_all("grown");
        { On on(0); at<Fn0>(0x00066B30)(po.m_next, nullptr); }
        { On on(1); pr.m_next->Allocate(); }
        same_all("allocated");
        { On on(0); at<Fn0>(0x00066B30)(po.m_next, nullptr); }
        { On on(1); pr.m_next->Allocate(); }
        same_all("allocated, again");
        for (int i = 0; i < 6; i++) {
            AtDataLink* x;
            AtDataLink* y;
            { On on(0); x = (AtDataLink*)at<FnRetI>(0x00067430)(&po, nullptr, 1); }
            { On on(1); y = pr.New(1); }
            Ids after[2] = {ids_of(0), ids_of(1)};
            eq("new", after[0].of(x), after[1].of(y));
            same_all("new");
        }
        // Clear one pool directly, with its chain, then again: its fields are its own to see only here, since a registry's clear goes on to Reset.
        { On on(0); at<Fn0>(0x00066930)(&po, nullptr); }
        { On on(1); pr.Clear(); }
        eq("cleared: count", po.m_count, pr.m_count);
        eq("cleared: capacity", po.m_capacity, pr.m_capacity);
        eq("cleared: type", po.m_type, pr.m_type);
        eq("cleared: used", po.m_used, pr.m_used);
        eq("cleared: values", (uintptr_t)po.m_values, (uintptr_t)pr.m_values);
        eq("cleared: descriptors", (uintptr_t)po.m_descriptors, (uintptr_t)pr.m_descriptors);
        eq("cleared: links", (uintptr_t)po.m_links, (uintptr_t)pr.m_links);
        eq("cleared: next", (uintptr_t)po.m_next, (uintptr_t)pr.m_next);
        compare_logs("cleared directly");
        { On on(0); at<Fn0>(0x00066930)(&po, nullptr); }
        { On on(1); pr.Clear(); }
        compare_logs("cleared with nothing there");
        // The last, empty pool made to take one: of four, keeping its type, which has no class, so its value is null and the descriptor is set through nothing.
        const bool c0 = guarded([&] { On on(0); at<FnRetI>(0x00067430)(&o->m_pools[12], nullptr, 1); });
        g_recording = false;
        const bool c1 = guarded([&] { On on(1); r->m_pools[12].New(1); });
        g_recording = false;
        eq("the type with no class", c0, c1);
        eq("the type with no class crashes", c0, 1);
        eq("its count", o->m_pools[12].m_count, r->m_pools[12].m_count);
        eq("its capacity", o->m_pools[12].m_capacity, r->m_pools[12].m_capacity);
        eq("its used", o->m_pools[12].m_used, r->m_pools[12].m_used);
        compare_logs("the type with no class");
    }
    g_regs[0].clear();
    g_regs[1].clear();
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
    free_owners();
}

// Blocks built by hand, for what no file can make. These read only plain fields, so both sides share them, and answers are compared as pointers.
static void run_hand_pools() {
    g_context = "pools by hand";
    g_fill = 0xCD;
    AtDataLink links[4];
    char q[] = "Q", cased[] = "Cased", empty[] = "";
    links[0].m_inUse = 0;          // Not in use, with a name.
    links[0].m_name = q;
    links[1].m_inUse = 1;          // In use, nameless.
    links[2].m_inUse = 1;          // In use, empty name.
    links[2].m_name = empty;
    links[3].m_inUse = 1;
    links[3].m_name = cased;
    AtDataComponentPool first, second, empty_block, no_values;
    std::memset(&first, 0, sizeof first);
    std::memset(&second, 0, sizeof second);
    std::memset(&empty_block, 0, sizeof empty_block);
    std::memset(&no_values, 0, sizeof no_values);
    first.m_count = 3;
    first.m_values = (AtDataComponent*)links;
    first.m_links = links;
    first.m_next = &second;
    first.m_type = AtDataRegistry::kInt;
    second.m_count = 7;
    second.m_values = (AtDataComponent*)(links + 1);
    second.m_links = links + 3;
    second.m_count = 1;
    second.m_type = AtDataRegistry::kReal;
    empty_block.m_values = (AtDataComponent*)links;
    empty_block.m_next = &second;
    no_values.m_count = 1;
    no_values.m_next = &second;
    for (const char* n : {"Q", "", "Cased", "CASED", "Nope"}) {
        for (int exact : {0, 1}) {
            eq("first", (uintptr_t)at<FnRetPI>(0x00066A70)(&first, nullptr, n, exact), (uintptr_t)first.FindByName(n, exact));
            eq("second", (uintptr_t)at<FnRetPI>(0x00066A70)(&second, nullptr, n, exact), (uintptr_t)second.FindByName(n, exact));
            eq("no count", (uintptr_t)at<FnRetPI>(0x00066A70)(&empty_block, nullptr, n, exact), (uintptr_t)empty_block.FindByName(n, exact));
            eq("no values", (uintptr_t)at<FnRetPI>(0x00066A70)(&no_values, nullptr, n, exact), (uintptr_t)no_values.FindByName(n, exact));
        }
    }
    // An index past the first block and within the next, which is how a chain with a larger block answers.
    second.m_count = 5;
    for (unsigned i : {0u, 2u, 3u, 4u, 5u}) {
        eq("along the chain", (uintptr_t)at<FnRetI>(0x00066110)(&first, nullptr, i), (uintptr_t)first.ElementAt(i));
    }
    // Clearing a block with no count touches nothing.
    AtDataComponentPool a, b;
    std::memset(&a, 0x11, sizeof a);
    std::memset(&b, 0x11, sizeof b);
    a.m_count = b.m_count = 0;
    { On on(0); at<Fn0>(0x00066930)(&a, nullptr); }
    { On on(1); b.Clear(); }
    eq("left alone", std::memcmp(&a, &b, sizeof a), 0);
    eq("left alone, capacity", a.m_capacity, 0x11111111);
    compare_logs("no count");
    for (AtDataLink& l : links) {
        l.m_name = nullptr;
        l.m_inUse = 0;
    }
}

static void run_groups() {
    g_context = "groups";
    g_fill = 0xCD;
    make_owners(2);
    make_registries(0xCD);
    make_registries(0xCD);
    make_registries(0xCD);
    AtDataRegistryOwner* wo = g_own[0];
    AtDataRegistryOwner* wr = g_own[1];
    AtDataRegistry** o = g_regs[0].data();
    AtDataRegistry** r = g_regs[1].data();
    typedef void(__fastcall* FnAdd)(void*, void*, const void*, const char*);
    typedef void(__fastcall* FnAddNs)(void*, void*, const char*, const void*);
    typedef void(__fastcall* FnOne)(void*, void*, const void*);
    const auto add = [&](int i, const char* ns) {
        { On on(0); at<FnAdd>(0x0005F0B0)(wo, nullptr, o[i], ns); }
        { On on(1); wr->Add(r[i], ns); }
        same_all("add");
    };
    const auto rem = [&](int i) {
        { On on(0); at<FnOne>(0x0005F1F0)(wo, nullptr, o[i]); }
        { On on(1); wr->Remove(r[i]); }
        same_all("remove");
    };
    // Into the plain group, and moved about; a space, a second in it, a null and an empty name.
    add(0, nullptr);
    add(1, "");
    add(2, nullptr);
    add(1, "Sp");
    add(2, "Sp");
    add(0, "Other");
    rem(1);
    rem(1);
    rem(2);
    rem(0);
    add(0, "Third");
    add(1, "Sp");
    rem(0);
    add(0, nullptr);
    rem(0);
    rem(0);
    // Not in the group it is removed from.
    add(0, nullptr);
    { On on(0); at<FnOne>(0x000602E0)(&wo->m_plain, nullptr, o[1]); }
    { On on(1); wr->m_plain.Remove(r[1]); }
    same_all("not here");
    { On on(0); at<FnOne>(0x000602E0)(&wo->m_plain, nullptr, o[0]); }
    { On on(1); wr->m_plain.Remove(r[0]); }
    same_all("here");
    add(0, nullptr);
    add(2, nullptr);
    { On on(0); at<FnOne>(0x000602E0)(&wo->m_plain, nullptr, o[0]); }
    { On on(1); wr->m_plain.Remove(r[0]); }
    same_all("the second");
    { On on(0); at<FnOne>(0x00060170)(&wo->m_spaces, nullptr, o[0]); }
    { On on(1); AtDataRegistryNameSpaces::Remove(r[0]); }
    same_all("in none");
    { On on(0); at<FnOne>(0x00060170)(&wo->m_spaces, nullptr, o[2]); }
    { On on(1); AtDataRegistryNameSpaces::Remove(r[2]); }
    same_all("through the spaces");
    // A group's own forms.
    AtDataRegistryGroup ga, gb;
    at<FnCtor>(0x000603A0)(&ga, nullptr);
    at<FnP>(0x000602C0)(&ga, nullptr, "GroupNam");
    gb.SetName("GroupNam");
    eq("group's name", std::memcmp(ga.m_name, gb.m_name, 8), 0);
    ga.m_first = gb.m_first = (AtDataRegistry*)&ga;
    ga.m_nextFree = gb.m_nextFree = &ga;
    ga.m_database = gb.m_database = (AtDataRegistryNameSpaces*)&ga;
    at<Fn0>(0x000B3F70)(&ga, nullptr);
    gb.Reset();
    eq("reset: name", ga.m_name[0], gb.m_name[0]);
    eq("reset: first", (uintptr_t)ga.m_first, (uintptr_t)gb.m_first);
    eq("reset: next", (uintptr_t)ga.m_nextFree, (uintptr_t)gb.m_nextFree);
    eq("reset: database", (uintptr_t)ga.m_database, (uintptr_t)gb.m_database);
    // Out of room: reported, then a crash on the null, in both.
    rem(1);
    add(0, "One");
    add(1, "Two");
    const bool c0 = guarded([&] { On on(0); at<FnAdd>(0x0005F0B0)(wo, nullptr, o[2], "Three"); });
    g_recording = false;
    const bool c1 = guarded([&] { On on(1); wr->Add(r[2], "Three"); });
    g_recording = false;
    eq("overflow crash", c0, c1);
    eq("overflow did crash", c0, 1);
    same_all("overflow");
    // Another pool over this one, the table cleared; a pool cleared and left; destroyed with and without the delete.
    { On on(0); at<FnI>(0x00060010)(&wo->m_spaces, nullptr, 3); }
    { On on(1); wr->m_spaces.Init(3); }
    o[0]->m_group = o[1]->m_group = r[0]->m_group = r[1]->m_group = nullptr;
    same_all("init over");
    { On on(0); at<Fn0>(0x000B40E0)(&wo->m_spaces, nullptr); }
    { On on(1); wr->m_spaces.Clear(); }
    same_all("cleared");
    { On on(0); at<Fn0>(0x000B40E0)(&wo->m_spaces, nullptr); }
    { On on(1); wr->m_spaces.Clear(); }
    same_all("cleared with none");
    // One of one, so that Init's chain has no middle.
    { On on(0); at<FnI>(0x00060010)(&wo->m_spaces, nullptr, 1); }
    { On on(1); wr->m_spaces.Init(1); }
    same_all("one");
    // No memory, no heap, for the pool.
    g_fail_alloc = true;
    const bool f0 = guarded([&] { On on(0); at<FnI>(0x00060010)(&wo->m_spaces, nullptr, 2); });
    g_recording = false;
    const bool f1 = guarded([&] { On on(1); wr->m_spaces.Init(2); });
    g_recording = false;
    g_fail_alloc = false;
    eq("no memory for a pool", f0, f1);
    eq("no memory for a pool did crash", f0, 1);
    compare_logs("no memory");
    *(void**)(uintptr_t)0x002C9F9C = nullptr;
    AtDataRegistryNameSpaces::s_heap = nullptr;
    { On on(0); at<FnI>(0x00060010)(&wo->m_spaces, nullptr, 2); }
    { On on(1); wr->m_spaces.Init(2); }
    same_all("no heap");
    at<void(__cdecl*)(void*)>(0x0005FFB0)(&g_heap_token);
    AtDataRegistryNameSpaces::SetHeap(&g_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002C9F9C, (uintptr_t)AtDataRegistryNameSpaces::s_heap);
    // A database's own fields, right after its constructor, and right after its destructor.
    {
        AtDataRegistryNameSpaces* x = (AtDataRegistryNameSpaces*)raw(sizeof(AtDataRegistryNameSpaces), 0xCD);
        AtDataRegistryNameSpaces* y = (AtDataRegistryNameSpaces*)raw(sizeof(AtDataRegistryNameSpaces), 0xCD);
        at<FnCtor>(0x0005FFF0)(x, nullptr);
        new (y) AtDataRegistryNameSpaces;
        eq("constructed: pool", (uintptr_t)x->m_pool, (uintptr_t)y->m_pool);
        eq("constructed: size", x->m_poolSize, y->m_poolSize);
        eq("constructed: free", (uintptr_t)x->m_free, (uintptr_t)y->m_free);
        eq("constructed: last", (uintptr_t)x->m_last, (uintptr_t)y->m_last);
        { On on(0); at<FnI>(0x00060010)(x, nullptr, 2); }
        { On on(1); y->Init(2); }
        x->m_last = x->m_pool;
        y->m_last = y->m_pool;
        { On on(0); at<Fn0>(0x00060230)(x, nullptr); }
        { On on(1); y->~AtDataRegistryNameSpaces(); }
        compare_logs("database destroyed");
        eq("destroyed: pool", (uintptr_t)x->m_pool, (uintptr_t)y->m_pool);
        eq("destroyed: size", x->m_poolSize, y->m_poolSize);
        eq("destroyed: free", (uintptr_t)x->m_free, (uintptr_t)y->m_free);
        eq("destroyed: last", (uintptr_t)x->m_last, (uintptr_t)y->m_last);
    }
    // A database by itself, with the delete.
    AtDataRegistryNameSpaces* dbo = (AtDataRegistryNameSpaces*)raw(sizeof(AtDataRegistryNameSpaces), 0xCD);
    at<FnCtor>(0x0005FFF0)(dbo, nullptr);
    AtDataRegistryNameSpaces* dbr = new AtDataRegistryNameSpaces;
    { On on(0); at<FnI>(0x00060010)(dbo, nullptr, 2); }
    { On on(1); dbr->Init(2); }
    compare_logs("database");
    { On on(0); at<FnDel>(0x000602A0)(dbo, nullptr, 1); }
    { On on(1); delete dbr; }
    compare_logs("database deleted");
    g_regs[0].clear();
    g_regs[1].clear();
    free_owners();
}

// One value of a type on each side, its own descriptor, through its slots directly: read, grow, shrink, again, reset, destroy.
static void run_values() {
    g_fill = 0xCD;
    g_recording = false;
    for (int t = 0; t < 12; t++) {
        g_context = std::string("value ") + kTypes[t];
        char buf[0x80];
        make_value_r(t, buf);
        g_vft[t] = *(void**)buf;
        if (t == 5) {
            g_ascii_vft = *(void**)(buf + 0xC);
            g_wide_vft = *(void**)(buf + 0x20);
        }
    }
    for (int t = 0; t < 12; t++) {
        g_context = std::string("value ") + kTypes[t];
        for (const char* text : {"1 2 3 4 5 6", "1.5 2.5 3.5 4.5 5.5 6.5", "\"A\" \"Bb\" \"Ccc\" \"Dddd\" \"Eeeee\" \"F\"", "\"Text\" \"Unicode\" \"X\" \"Y\" \"Z\"", "\"Text\" \"unicodE\" \"X\" \"Y\" \"Z\"", "\"Text\" \"Plain\" \"X\" \"Y\" \"Z\"", "\"\xE9\xFF\" 5", "{ { { { { {"}) {
            for (const int ceiling : {0x7FFFFFFF, 30, 1}) {
                char* o = (char*)raw(kSize[t], 0xCD);
                char* r = (char*)raw(kSize[t], 0xCD);
                at<FnCtor>(kCtor[t])(o, nullptr);
                make_value_r(t, r);
                eq("value's class", type_of(0, o), type_of(1, r));
                eq("constructed: descriptor", *(uintptr_t*)(o + 4), *(uintptr_t*)(r + 4));
                eq("constructed: registry", *(uintptr_t*)(o + 8), *(uintptr_t*)(r + 8));
                AtDataDescriptor* d0;
                AtDataDescriptor* d1;
                make_descriptor(d0, d1);
                d0->m_count = d1->m_count = 3;
                d0->m_intCeiling = d1->m_intCeiling = ceiling;
                Direct da(text), db(text);
                {
                    On on(0);
                    eq("read", at<FnSlotRead>(*(uint32_t*)(*(uint32_t*)o + 4))(o, nullptr, d0, &da.tokenizer), 0);
                }
                {
                    On on(1);
                    eq("read", ((AtDataComponent*)r)->Read(d1, &db.tokenizer), 0);
                }
                eq("descriptor kept", *(void**)(o + 4) == d0, *(void**)(r + 4) == d1);
                same_value(t, o, r);
                same_descriptor(d0, d1);
                compare_logs("read");
                if (t >= 6) {
                    typedef int(__fastcall* FnResize)(void*, void*, unsigned);
                    unsigned live = 3;  // Elements still as the read left them. A short name's are compared up to its terminator, the rest whole.
                    unsigned char step = 0x10;
                    for (const unsigned n : {5u, 2u, 2u, 0u, 3u}) {
                        g_fill = step;
                        step = (unsigned char)(step + 0x11);
                        { On on(0); eq("resize", at<FnResize>(*(uint32_t*)(*(uint32_t*)o + 0x10))(o, nullptr, n), 0); }
                        { On on(1); eq("resize", ((AtDataComponent*)r)->Resize(n), 0); }
                        g_fill = 0xCD;
                        eq("count", d0->m_count, d1->m_count);
                        compare_logs("resize");
                        live = std::min(live, n);
                        if (t == 11) {
                            const AtDataStringArray* x = (const AtDataStringArray*)o;
                            const AtDataStringArray* y = (const AtDataStringArray*)r;
                            for (unsigned i = 0; i < n; i++) {
                                eq("kept chars", x->m_chars[i] != nullptr, y->m_chars[i] != nullptr);
                                if (i < live) same_text("kept text", &x->m_texts[i], &y->m_texts[i]);
                            }
                            eq("past the list", *(uint32_t*)(x->m_chars + n), *(uint32_t*)(y->m_chars + n));
                            eq("past the texts", *(uint32_t*)((char*)x->m_texts + n * 0x14 + 4), *(uint32_t*)((char*)y->m_texts + n * 0x14 + 4));
                        } else if (t != 10) {
                            const unsigned width = t == 7 ? 1 : 4;
                            eq("kept, and past the end", std::memcmp(*(void**)(o + 0xC), *(void**)(r + 0xC), n * width + 8), 0);
                        } else {
                            const char* x = *(char**)(o + 0xC);
                            const char* y = *(char**)(r + 0xC);
                            for (unsigned i = 0; i < live; i++) eq("kept name", strncmp(x + i * 8, y + i * 8, 8), 0);
                            eq("new names, and past the end", std::memcmp(x + live * 8, y + live * 8, (n - live) * 8 + 8), 0);
                        }
                    }
                    d0->m_count = d1->m_count = 1;
                    same_value(t == 11 ? 0 : t, o, r);
                }
                if (t == 11) {
                    Direct dc("\"More text\""), dd("\"More text\"");
                    AtDataStringArray* x = (AtDataStringArray*)o;
                    AtDataStringArray* y = (AtDataStringArray*)r;
                    d0->m_count = d1->m_count = 3;
                    { On on(0); at<FnIP>(0x0006B4F0)(o, nullptr, 2, "Two"); }
                    { On on(1); y->SetText(2, "Two"); }
                    same_text("set", &x->m_texts[2], &y->m_texts[2]);
                    eq("set", std::memcmp(x->m_chars[2], y->m_chars[2], x->m_texts[2].m_capacity * 2), 0);
                    compare_logs("set text");
                }
                if (t == 5) {
                    // Over text it has; wide over wide; with a descriptor bound.
                    for (const char* more : {"Second", "", "Third and longer"}) {
                        { On on(0); eq("assign", at<FnP2>(0x0006A330)(o, nullptr, d0, more), 0); }
                        { On on(1); eq("assign", ((AtDataString*)r)->Assign(d1, more), 0); }
                        same_value(t, o, r);
                        compare_logs("assign");
                    }
                    *(int*)(o + 0x34) = *(int*)(r + 0x34) = 1;
                    { On on(0); at<FnP2>(0x0006A330)(o, nullptr, d0, "Wide"); }
                    { On on(1); ((AtDataString*)r)->Assign(d1, "Wide"); }
                    same_value(t, o, r);
                    compare_logs("assign, from wide");
                }
                // Reset through the slot, then again on nothing; the base's fields are compared raw.
                {
                    On on(0);
                    at<Fn0>(*(uint32_t*)(*(uint32_t*)o + 0xC))(o, nullptr);
                }
                {
                    On on(1);
                    ((AtDataComponent*)r)->Reset();
                }
                eq("reset: descriptor", *(void**)(o + 4) == nullptr, *(void**)(r + 4) == nullptr);
                eq("reset: registry", *(void**)(o + 8) == nullptr, *(void**)(r + 8) == nullptr);
                compare_logs("reset");
                if (t >= 6) {
                    eq("reset: data", *(void**)(o + 0xC) == nullptr, *(void**)(r + 0xC) == nullptr);
                    if (t == 11) eq("reset: texts", *(void**)(o + 0x10) == nullptr, *(void**)(r + 0x10) == nullptr);
                }
                if (t == 5) {
                    eq("reset: unicode", *(int*)(o + 0x34), *(int*)(r + 0x34));
                    eq("reset: chars", *(void**)(o + 0x38) == nullptr, *(void**)(r + 0x38) == nullptr);
                    same_text("reset: narrow", (AtText*)(o + 0xC), (AtText*)(r + 0xC));
                    same_text("reset: wide", (AtText*)(o + 0x20), (AtText*)(r + 0x20));
                }
                // Nothing held: a string array's reset leaves the base's fields as they are.
                *(void**)(o + 4) = (void*)7;
                *(void**)(r + 4) = (void*)7;
                *(void**)(o + 8) = (void*)7;
                *(void**)(r + 8) = (void*)7;
                {
                    On on(0);
                    at<Fn0>(*(uint32_t*)(*(uint32_t*)o + 0xC))(o, nullptr);
                }
                {
                    On on(1);
                    ((AtDataComponent*)r)->Reset();
                }
                eq("reset again: descriptor", *(uintptr_t*)(o + 4), *(uintptr_t*)(r + 4));
                eq("reset again: registry", *(uintptr_t*)(o + 8), *(uintptr_t*)(r + 8));
                compare_logs("reset again");
                { On on(0); at<Fn0>(kDtor[t])(o, nullptr); }
                { On on(1); ((AtDataComponent*)r)->~AtDataComponent(); }
                eq("descriptor at the end", *(uintptr_t*)(o + 4), *(uintptr_t*)(r + 4));
                eq("registry at the end", *(uintptr_t*)(o + 8), *(uintptr_t*)(r + 8));
                compare_logs("destroyed");
                // And each destructor over a value that holds something: narrow text, then wide.
                for (const char* hold : {"\"Hold\" \"Hold\" 5", "\"Hold\" \"Unicode\" 5"}) {
                    o = (char*)raw(kSize[t], 0xCD);
                    r = (char*)raw(kSize[t], 0xCD);
                    at<FnCtor>(kCtor[t])(o, nullptr);
                    make_value_r(t, r);
                    d0->m_count = d1->m_count = 2;
                    Direct dx(hold), dy(hold);
                    { On on(0); at<FnSlotRead>(*(uint32_t*)(*(uint32_t*)o + 4))(o, nullptr, d0, &dx.tokenizer); }
                    { On on(1); ((AtDataComponent*)r)->Read(d1, &dy.tokenizer); }
                    compare_logs("read to hold");
                    { On on(0); at<Fn0>(kDtor[t])(o, nullptr); }
                    { On on(1); ((AtDataComponent*)r)->~AtDataComponent(); }
                    compare_logs("destroyed holding");
                    eq("held: descriptor at the end", *(void**)(o + 4) == nullptr, *(void**)(r + 4) == nullptr);
                    if (t == 5) {
                        eq("held: unicode", *(int*)(o + 0x34), *(int*)(r + 0x34));
                        eq("held: chars", *(uintptr_t*)(o + 0x38), *(uintptr_t*)(r + 0x38));
                        eq("held: unicode cleared", *(int*)(o + 0x34), 0);
                        same_text("held: narrow", (AtText*)(o + 0xC), (AtText*)(r + 0xC));
                        same_text("held: wide", (AtText*)(o + 0x20), (AtText*)(r + 0x20));
                        eq("held: narrow's data", (uintptr_t)((AtTextAscii*)(o + 0xC))->m_data, (uintptr_t)((AtTextAscii*)(r + 0xC))->m_data);
                        eq("held: wide's data", (uintptr_t)((AtTextUnicode*)(o + 0x20))->m_data, (uintptr_t)((AtTextUnicode*)(r + 0x20))->m_data);
                    }
                    if (t >= 6) eq("held: data at the end", *(void**)(o + 0xC) == nullptr, *(void**)(r + 0xC) == nullptr);
                }
            }
        }
    }
    // No heap, then no memory.
    g_context = "values, none";
    for (int stage = 0; stage < 2; stage++) {
        for (int t = 5; t < 12; t++) {
            char* o = (char*)raw(kSize[t], 0xCD);
            char* r = (char*)raw(kSize[t], 0xCD);
            at<FnCtor>(kCtor[t])(o, nullptr);
            make_value_r(t, r);
            AtDataDescriptor* d0;
            AtDataDescriptor* d1;
            make_descriptor(d0, d1);
            d0->m_count = d1->m_count = 2;
            Direct da("\"A\" \"B\" \"C\""), db("\"A\" \"B\" \"C\"");
            if (stage == 0) {
                *(void**)(uintptr_t)0x002CB09C = nullptr;
                AtDataComponent::s_heap = nullptr;
            } else {
                g_fail_alloc = true;
            }
            int ret0 = 0, ret1 = 0;
            const bool c0 = guarded([&] { On on(0); at<FnSlotRead>(*(uint32_t*)(*(uint32_t*)o + 4))(o, nullptr, d0, &da.tokenizer); });
            g_recording = false;
            const bool c1 = guarded([&] { On on(1); ((AtDataComponent*)r)->Read(d1, &db.tokenizer); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crash", c0, c1);
            compare_logs("none: read");
            if (t >= 6 && stage == 1) {
                g_fail_alloc = true;
                { On on(0); ret0 = at<FnI>(*(uint32_t*)(*(uint32_t*)o + 0x10))(o, nullptr, 2); }
                { On on(1); ret1 = ((AtDataComponent*)r)->Resize(2); }
                g_fail_alloc = false;
                eq("no memory", ret0, ret1);
                eq("no memory says so", ret0, -4);
                eq("count kept", d0->m_count, d1->m_count);
                compare_logs("none: resize");
            }
            if (t == 5 && stage == 1) {
                char* wo = (char*)raw(kSize[t], 0xCD);
                char* wr = (char*)raw(kSize[t], 0xCD);
                at<FnCtor>(kCtor[t])(wo, nullptr);
                make_value_r(t, wr);
                *(int*)(wo + 0x34) = *(int*)(wr + 0x34) = 1;
                g_fail_alloc = true;
                const bool w0 = guarded([&] { On on(0); at<FnP2>(0x0006A330)(wo, nullptr, d0, "Wide"); });
                g_recording = false;
                const bool w1 = guarded([&] { On on(1); ((AtDataString*)wr)->Assign(d1, "Wide"); });
                g_recording = false;
                g_fail_alloc = false;
                eq("no memory for wide text", w0, w1);
                compare_logs("none: wide");
            }
            if (t == 11 && stage == 1) {
                char* so = (char*)raw(kSize[t], 0xCD);
                char* sr = (char*)raw(kSize[t], 0xCD);
                at<FnCtor>(kCtor[t])(so, nullptr);
                make_value_r(t, sr);
                *(void**)(so + 4) = d0;
                *(void**)(sr + 4) = d1;
                at<FnI>(0x0006B170)(so, nullptr, 2);
                ((AtDataStringArray*)sr)->Resize(2);
                g_log[0].n = g_log[1].n = 0;
                g_fail_alloc = true;
                const bool s0 = guarded([&] { On on(0); at<FnIP>(0x0006B4F0)(so, nullptr, 0, "X"); });
                g_recording = false;
                const bool s1 = guarded([&] { On on(1); ((AtDataStringArray*)sr)->SetText(0, "X"); });
                g_recording = false;
                g_fail_alloc = false;
                eq("no memory for an element", s0, s1);
                compare_logs("none: element");
            }
            at<void(__cdecl*)(void*)>(0x00069DE0)(&g_heap_token);
            AtDataComponent::SetHeap(&g_heap_token);
            eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002CB09C, (uintptr_t)AtDataComponent::s_heap);
        }
    }
}

// Setting a text from narrow characters, on both kinds of text, at and past its room.
static void run_text_class() {
    g_context = "text";
    g_fill = 0xCD;
    typedef void(__fastcall* FnSet)(void*, void*, const char*);
    for (const char* text : {"", "Ab", "\xE9\xFF", "Exactly8", "Longer than the room"}) {
        for (const unsigned capacity : {1u, 2u, 8u, 9u, 16u}) {
            char a[0x80], b[0x80];
            std::memset(a, 0x33, sizeof a);
            std::memset(b, 0x33, sizeof b);
            unsigned char ao[0x20], ar[0x20];
            unsigned short wa[0x40], wb[0x40];
            std::memset(wa, 0x33, sizeof wa);
            std::memset(wb, 0x33, sizeof wb);
            at<FnCtor>(0x0002E000)(ao, nullptr);
            at<void(__fastcall*)(void*, void*, char*, unsigned)>(0x0002E040)(ao, nullptr, a, capacity);
            AtTextAscii* ra = new (ar) AtTextAscii;
            ra->Init(b, capacity);
            ((AtText*)ao)->m_start = 3;
            ra->m_start = 3;
            at<FnSet>(0x00039970)(ao, nullptr, text);
            ra->SetNarrow(text);
            same_text("ascii", (AtText*)ao, ra);
            eq("ascii's chars", std::memcmp(a, b, sizeof a), 0);
            at<FnCtor>(0x0002DD90)(ao, nullptr);
            at<void(__fastcall*)(void*, void*, unsigned short*, unsigned)>(0x0002DDE0)(ao, nullptr, wa, capacity);
            AtTextUnicode* ru = new (ar) AtTextUnicode;
            ru->Init(wb, capacity);
            at<FnSet>(0x00039970)(ao, nullptr, text);
            ru->SetNarrow(text);
            same_text("wide", (AtText*)ao, ru);
            eq("wide's chars", std::memcmp(wa, wb, sizeof wa), 0);
        }
    }
    AtTextAscii t;
    char c[4];
    t.Init(c, 4);
    t.Release();
    eq("released", t.m_data == nullptr && t.m_capacity == 0 && t.m_start == 0 && t.m_end == 0, 1);
}

static void set_all_heaps(void* h) {
    *(void**)(uintptr_t)0x002CB094 = h;
    AtDataRegistry::s_heap = h;
}

static void run_failures() {
    // No registry heap: the pools, the file name.
    g_context = "no registry heap";
    {
        const std::vector<unsigned char> data = bytes(all_types(1) + registry(1, link("Name \"A\" Type Int Data 2")));
        make_owners(2);
        file_is(data);
        make_registries(0xCD);
        set_all_heaps(nullptr);
        if (load(0, nullptr, nullptr, false, "no heap")) clear_both(0, "clear");
        at<void(__cdecl*)(void*)>(0x00066100)(&g_heap_token);
        AtDataRegistry::SetHeap(&g_heap_token);
        eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002CB094, (uintptr_t)AtDataRegistry::s_heap);
        eq("pool heap", (uintptr_t)*(void**)(uintptr_t)0x002CB098, (uintptr_t)AtDataRegistry::s_poolHeap);
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
    }
    // No heap for the strings the reader holds a while.
    g_context = "no token heap";
    {
        const std::vector<unsigned char> data = bytes(one_of("Int", 1) + registry(1, link("Name \"A\" FormatString \"F\" Type Int Data 2")));
        make_owners(2);
        file_is(data);
        make_registries(0xCD);
        *(void**)(uintptr_t)0x002C60B0 = nullptr;
        AtIoTokenFile::s_heap = nullptr;
        if (load(0, nullptr, nullptr, false, "no heap")) clear_both(0, "clear");
        *(void**)(uintptr_t)0x002C60B0 = &g_heap_token;
        AtIoTokenFile::s_heap = &g_heap_token;
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
    }
    // Pools by hand with no memory, for the two kinds of block a pool has first.
    g_context = "no memory for a pool";
    {
        for (int t : {2, 12}) {
            AtDataComponentPool a, b;
            std::memset(&a, 0, sizeof a);
            std::memset(&b, 0, sizeof b);
            a.m_count = b.m_count = 2;
            a.m_type = b.m_type = t;
            g_fail_alloc = true;
            const bool c0 = guarded([&] { On on(0); at<Fn0>(0x00066B30)(&a, nullptr); });
            g_recording = false;
            const bool c1 = guarded([&] { On on(1); b.Allocate(); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crash", c0, c1);
            compare_logs("allocate");
        }
        AtDataComponentPool a, b;
        std::memset(&a, 0, sizeof a);
        std::memset(&b, 0, sizeof b);
        a.m_capacity = b.m_capacity = 2;
        a.m_type = b.m_type = 4;
        g_fail_alloc = true;
        const bool c0 = guarded([&] { On on(0); at<Fn0>(0x000666E0)(&a, nullptr); });
        g_recording = false;
        const bool c1 = guarded([&] { On on(1); b.Grow(); });
        g_recording = false;
        g_fail_alloc = false;
        eq("crash", c0, c1);
        compare_logs("grow");
        *(void**)(uintptr_t)0x002CB098 = nullptr;
        AtDataRegistry::s_poolHeap = nullptr;
        a.m_next = b.m_next = nullptr;
        { On on(0); at<Fn0>(0x000666E0)(&a, nullptr); }
        { On on(1); b.Grow(); }
        compare_logs("grow, no heap");
        eq("next's count", a.m_next->m_count, b.m_next->m_count);
        eq("next's capacity", a.m_next->m_capacity, b.m_next->m_capacity);
        eq("next's type", a.m_next->m_type, b.m_next->m_type);
        eq("next's used", a.m_next->m_used, b.m_next->m_used);
        eq("next's values", (uintptr_t)a.m_next->m_values, (uintptr_t)b.m_next->m_values);
        eq("next's descriptors", (uintptr_t)a.m_next->m_descriptors, (uintptr_t)b.m_next->m_descriptors);
        eq("next's links", (uintptr_t)a.m_next->m_links, (uintptr_t)b.m_next->m_links);
        eq("next's next", (uintptr_t)a.m_next->m_next, (uintptr_t)b.m_next->m_next);
        *(void**)(uintptr_t)0x002CB098 = &g_heap_token;
        AtDataRegistry::s_poolHeap = &g_heap_token;
    }
    // Through a reader that starts refusing memory at a given word, so that each copy it makes is refused in turn.
    g_context = "no memory in a link";
    {
        class Refusing : public DirectTokenizer {
        public:
            int left = 0;
            int Next() override {
                if (left > 0 && --left == 0) g_fail_alloc = true;
                return AtIoTextTokenizer::Next();
            }
        };
        const std::string text = one_of("Int", 1) + registry(1, link("Name \"A\" FormatString \"F\" Type Int Data 2"));
        // Which word: the Name, the FormatString's string, the closing brace, and on.
        for (int at_word = 1; at_word < 28; at_word++) {
            make_owners(2);
            make_registries(0xCD);
            MemoryStream sa, sb;
            Refusing ta, tb;
            std::string copy_a = text, copy_b = text;
            sa.m_base = sa.m_cur = (char*)copy_a.data();
            sa.m_limit = sa.m_base + copy_a.size();
            sa.m_size = (unsigned)copy_a.size();
            sb.m_base = sb.m_cur = (char*)copy_b.data();
            sb.m_limit = sb.m_base + copy_b.size();
            sb.m_size = (unsigned)copy_b.size();
            ta.SetStream(&sa);
            tb.SetStream(&sb);
            ta.left = tb.left = at_word;
            AtDataRegistry* o = g_regs[0][0];
            AtDataRegistry* r = g_regs[1][0];
            const bool c0 = guarded([&] { On on(0); at<FnRead>(0x00067D80)(o, nullptr, g_own[0], &ta, nullptr, nullptr); });
            g_recording = false;
            g_fail_alloc = false;
            const bool c1 = guarded([&] { On on(1); r->Read(g_own[1], &tb, nullptr, nullptr); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crash", c0, c1);
            compare_logs("refused");
            if (!c0 && !c1) same_all("refused, and through");
            sa.m_base = sa.m_cur = sa.m_limit = nullptr;
            sb.m_base = sb.m_cur = sb.m_limit = nullptr;
            ta.SetStream(nullptr);
            tb.SetStream(nullptr);
            g_regs[0].clear();
            g_regs[1].clear();
            free_owners();
        }
    }
    // Read by itself, through a tokenizer of the harness's: Load's own store of the flag would hide its.
    g_context = "read alone";
    {
        const std::string text = one_of("Int", 1) + registry(1, link("Name \"A\" Type Int Data 2"));
        make_owners(2);
        make_registries(0xCD);
        Direct da(text), db(text);
        int ret0 = 7, ret1 = 7;
        { On on(0); ret0 = at<FnRead>(0x00067D80)(g_regs[0][0], nullptr, g_own[0], &da.tokenizer, nullptr, nullptr); }
        { On on(1); ret1 = g_regs[1][0]->Read(g_own[1], &db.tokenizer, nullptr, nullptr); }
        eq("returned", ret0, ret1);
        eq("returned none", ret1, 0);
        eq("loaded", g_regs[1][0]->m_loaded, 1);
        same_all("read alone");
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
        make_owners(2);
        make_registries(0xCD);
        for (int side = 0; side < 2; side++) {
            AtDataComponentPool& pool = g_regs[side][0]->m_pools[AtDataRegistry::kInt];
            pool.m_used = 5;
            pool.m_values = (AtDataComponent*)&pool;
            pool.m_descriptors = (AtDataDescriptor*)&pool;
            pool.m_links = (AtDataLink*)&pool;
            pool.m_next = &pool;
            pool.m_type = 9;
        }
        Direct dc(one_of("Int", 1) + registry(0, "")), dd(one_of("Int", 1) + registry(0, ""));
        { On on(0); at<FnRead>(0x00067D80)(g_regs[0][0], nullptr, g_own[0], &dc.tokenizer, nullptr, nullptr); }
        { On on(1); g_regs[1][0]->Read(g_own[1], &dd.tokenizer, nullptr, nullptr); }
        same_all("read over dirty blocks");
        // And declared with none, so that nothing allocates over them.
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
        make_owners(2);
        make_registries(0xCD);
        for (int side = 0; side < 2; side++) {
            AtDataComponentPool& pool = g_regs[side][0]->m_pools[AtDataRegistry::kInt];
            pool.m_descriptors = (AtDataDescriptor*)&pool;
            pool.m_links = (AtDataLink*)&pool;
        }
        Direct de(one_of("Int", 0) + registry(0, "")), df(one_of("Int", 0) + registry(0, ""));
        { On on(0); at<FnRead>(0x00067D80)(g_regs[0][0], nullptr, g_own[0], &de.tokenizer, nullptr, nullptr); }
        { On on(1); g_regs[1][0]->Read(g_own[1], &df.tokenizer, nullptr, nullptr); }
        same_all("read over dirty blocks, none declared");
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
    }
    // A file that will not open: the report, and then the crash on no file.
    g_context = "no file";
    {
        const std::vector<unsigned char> data = bytes("");
        for (int status : {1, 2, 5}) {
            make_owners(2);
            file_is(data);
            make_registries(0xCD);
            g_fs->open_status = status;
            eq("no file goes on and crashes", load(0, nullptr, nullptr, false, "no file"), 0);
            g_fs->open_status = 0;
            g_regs[0].clear();
            g_regs[1].clear();
            free_owners();
        }
    }
    // Clearing a parent ahead of its child, and reloading a child.
    g_context = "parents";
    {
        const std::vector<unsigned char> parent = bytes(kParentText);
        const std::vector<unsigned char> child = bytes(one_of("Int", 3) + registry(3, link("Name \"K\" Type Int Parent \"A\\B\\\" Data 3") + link("Name \"L\" Type Int Parent \"A\\Nope\" Data 3") + link("Name \"M\" Type Int Parent \"K\" Data 3")));
        make_owners(4);
        file_is(parent);
        make_registries(0xCD);
        if (load(0, nullptr, nullptr, true, "parent")) {
            file_is(child);
            make_registries(0xCD);
            if (load(1, g_regs[0][0], g_regs[1][0], true, "child")) {
                // Found through the parent, and through a registry root in the way.
                Ids ids[2] = {ids_of(0), ids_of(1)};
                for (const char* n : {"K", "A", "M", "L", "Nope"}) {
                    eq("found from the parent", ids[0].of(at<FnRetPP>(0x00067980)(g_regs[0][0], nullptr, n, nullptr)), ids[1].of(g_regs[1][0]->FindChild(n, nullptr)));
                }
                file_is(child);
                if (load(1, g_regs[0][0], g_regs[1][0], true, "child again")) {
                    clear_both(0, "the parent first");
                    clear_both(1, "then the child");
                }
            }
        }
        g_regs[0].clear();
        g_regs[1].clear();
        g_log[0].n = g_log[1].n = 0;
        g_message[0][0] = g_message[1][0] = 0;
        free_owners();
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
    if (pos.size() != 2) { std::printf("usage: oracle_adr <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
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

    // A format's block is one byte short by the original's own count, and a text's terminator may be one past its room. Pads are compared where they matter.
    g_heap_pad = 0x40;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.

    run_values();
    run_text_class();
    run_descriptor();
    run_link();
    run_root();
    run_pool();
    run_hand_pools();
    run_groups();
    run_failures();
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const std::string& t : written()) run_text(bytes(t), "text [" + t + "]", fill, 8);
    }

    // Every real file: alone, and each under the one before it in its archive. Names in the first column are distinct contents, so each is one case.
    unsigned files = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    std::vector<std::pair<std::string, std::string>> list;
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ADR\t")) continue;
        char* tab = std::strchr(line, '\t');
        *tab = 0;
        char* tab2 = std::strchr(tab + 1, '\t');
        *tab2 = 0;
        list.push_back({line, tab + 1});
    }
    std::fclose(index);
    std::vector<std::vector<unsigned char>> contents;
    for (const auto& item : list) {
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + item.first).c_str(), "rb") || !f) { std::printf("cannot read %s\n", item.first.c_str()); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        contents.push_back(std::move(data));
    }
    unsigned chained = 0, crashes = 0;
    g_fill = 0xCD;
    for (size_t i = 0; i < list.size(); i++) {
        run_text(contents[i], list[i].first, 0xCD, 8);
        files++;
    }
    for (size_t i = 0; i < list.size();) {
        size_t e = i;
        while (e < list.size() && list[e].second == list[i].second) e++;
        g_context = "chain in " + list[i].second;
        make_owners(32);
        bool fine = true;
        for (size_t k = i; k < e && fine; k++) {
            g_context = "chain: " + list[k].first;
            file_is(contents[k]);
            make_registries(0xCD);
            const size_t at_ = k - i;
            fine = load(at_, at_ ? g_regs[0][at_ - 1] : nullptr, at_ ? g_regs[1][at_ - 1] : nullptr, true, "in the chain");
            chained += fine;
            crashes += !fine;
        }
        if (fine) {
            while (!g_regs[0].empty()) destroy_last();
            same_all("chain destroyed");
        }
        g_regs[0].clear();
        g_regs[1].clear();
        free_owners();
        bury();
        i = e;
    }

    std::printf("loads whose call log filled before they ended, and were compared by state from there: %u\n", g_full_logs);
    report_entries();
    std::printf("%s: %u .ADR files, %u chained, %u crashed alike; %u texts or files loaded, %u links and %u values compared; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, chained, crashes, g_loaded, g_links, g_values_seen, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
