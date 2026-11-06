// Differential oracle for the name map and the rumble reader (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_rmb <default.xbe> <cases dir> [--entries FILE] [--mutate] [--seed N]
//
// Cases: the real rumble file, found through index.txt in the directory tools/jam.py textcases writes; and input written here.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmLevelManager.h"
#include "gm/GmNameMap.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 20) std::printf("DIFF %s: %s: original [%s], recovered [%s]\n", g_context.c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

static const char kPendingRemove[] = "GmNameMapGroup::RemoveFromParent (0x00018010) is not recovered";
static void __fastcall o_remove_from_parent(void*, void*, void*) {
    g_log[0].add('F', 0);
    record_message(kPendingRemove, 0);
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnU)(void*, void*, unsigned);
typedef void(__fastcall* FnS)(void*, void*, const char*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnInsert)(void*, void*, const char*, void*);
typedef void(__fastcall* FnInsert3)(void*, void*, const char*, void*, unsigned*);
typedef void*(__fastcall* FnFind)(void*, void*, const char*);
typedef void*(__fastcall* FnFind2)(void*, void*, const char*, unsigned*);
typedef void(__fastcall* FnGetName)(void*, void*, unsigned, char*);
typedef unsigned(__fastcall* FnHash)(void*, void*, const char*);
typedef int(__cdecl* FnEqual)(const char*, const char*);

static GmNameMap* new_original_map() {
    void* p = std::calloc(1, sizeof(GmNameMap));
    at<Fn0>(0x00024C60)(p, nullptr);
    return (GmNameMap*)p;
}

static void name8(char* out, const char* in) {
    std::memset(out, 0, 9);
    std::memcpy(out, in, 8);
}

static void same_map(const char* what, const GmNameMap* o, const GmNameMap* r) {
    eq(what, o->m_size, r->m_size);
    eq(what, o->m_entries != nullptr, r->m_entries != nullptr);
    eq(what, (intptr_t)o->m_next, (intptr_t)r->m_next);
    eq(what, (intptr_t)o->m_group, (intptr_t)r->m_group);
    if (!o->m_entries || !r->m_entries || o->m_size != r->m_size) return;
    for (unsigned i = 0; i < o->m_size; i++) {
        eq("value", (intptr_t)o->m_entries[i].m_value, (intptr_t)r->m_entries[i].m_value);
        char a[9], b[9];
        name8(a, o->m_entries[i].m_name);
        name8(b, r->m_entries[i].m_name);
        eqs("slot name", a, b);
    }
}

static unsigned rng_state;
static unsigned rng() {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 12;
}

static void run_map_ops(int seed, int ops) {
    rng_state = (unsigned)seed * 2654435761u + 1;
    GmNameMap* o = new_original_map();
    GmNameMap r_storage;
    GmNameMap* r = &r_storage;
    eq("vftable", *(intptr_t*)o, 0x00272E60);
    std::vector<std::string> names;
    static const char alphabet[] = "ABCDEFabc019_ ";
    for (int i = 0; i < 40; i++) {
        std::string n;
        const unsigned len = 1 + rng() % 8;
        for (unsigned k = 0; k < len; k++) n += alphabet[rng() % (sizeof alphabet - 1)];
        names.push_back(n);
    }
    names.push_back("");
    names.push_back("\xE9\xF0\x80");  // Signed bytes in the hash.
    const auto buffer = [&](const std::string& n, char* out) { std::memset(out, 0, 16); std::memcpy(out, n.data(), n.size()); };
    bool ready = false;
    unsigned next_value = 1;
    for (int step = 0; step < ops; step++) {
        g_context = "map seed " + std::to_string(seed) + " step " + std::to_string(step);
        char name[16];
        buffer(names[rng() % names.size()], name);
        const unsigned op = ready ? rng() % 12 : 0;
        switch (op) {
        case 0: case 1: {  // Init over whatever is there, at a size from each part of the table.
            if (ready && rng() % 8 != 0) break;
            static const unsigned counts[] = {0, 1, 2, 3, 4, 7, 10, 11, 22, 23, 40, 100000, 100237, 100238};
            const unsigned n = counts[rng() % (ready ? 11 : 6)];
            { On on(0); at<FnU>(0x00024DA0)(o, nullptr, n); }
            { On on(1); r->Init(n); }
            ready = true;
            break;
        }
        case 2: case 3: case 4: {
            void* v = (void*)(uintptr_t)(next_value++);
            { On on(0); at<FnInsert>(0x00024EE0)(o, nullptr, name, v); }
            { On on(1); r->Insert(name, v); }
            break;
        }
        case 5: {
            void* v = (void*)(uintptr_t)(next_value++);
            unsigned ia = 0x77, ib = 0x77;
            { On on(0); at<FnInsert3>(0x00024FC0)(o, nullptr, name, v, &ia); }
            { On on(1); r->Insert(name, v, &ib); }
            eq("index", ia, ib);
            { On on(0); at<FnInsert3>(0x00024FC0)(o, nullptr, "ZZ", v, nullptr); }
            { On on(1); r->Insert("ZZ", v, nullptr); }
            break;
        }
        case 6: {
            // Removing a name that is absent from a full table never ends, in both. Skip that one case.
            bool has_free = false;
            for (unsigned i = 0; i < r->m_size; i++) has_free = has_free || r->m_entries[i].m_value == nullptr;
            bool has_name = false;
            for (unsigned i = 0; i < r->m_size; i++) has_name = has_name || AtNamesEqual(r->m_entries[i].m_name, name);
            if (!has_free && !has_name) break;
            { On on(0); at<FnS>(0x000250C0)(o, nullptr, name); }
            { On on(1); r->Remove(name); }
            break;
        }
        case 7: case 8: {
            void* a, *b;
            { On on(0); a = at<FnFind>(0x00025150)(o, nullptr, name); }
            { On on(1); b = r->Find(name); }
            eq("Find", (intptr_t)a, (intptr_t)b);
            break;
        }
        case 9: {
            unsigned ia = 0x77, ib = 0x77;
            void* a, *b;
            { On on(0); a = at<FnFind2>(0x000251D0)(o, nullptr, name, &ia); }
            { On on(1); b = r->Find(name, &ib); }
            eq("Find with index", (intptr_t)a, (intptr_t)b);
            eq("index", ia, ib);
            { On on(0); a = at<FnFind2>(0x000251D0)(o, nullptr, name, nullptr); }
            { On on(1); b = r->Find(name, nullptr); }
            eq("Find without index", (intptr_t)a, (intptr_t)b);
            break;
        }
        case 10: {
            const unsigned i = rng() % r->m_size;
            char a[9], b[9];
            std::memset(a, 'z', 9);
            std::memset(b, 'z', 9);
            at<FnGetName>(0x00024C00)(o, nullptr, i, a);
            r->GetName(i, b);
            eq("GetName", std::memcmp(a, b, 1), 0);
            a[8] = b[8] = 0;
            eqs("GetName", a, b);
            eq("Hash", at<FnHash>(0x00024C20)(o, nullptr, name), r->HashOf(name));
            break;
        }
        case 11:
            if (rng() % 16 == 0) {
                { On on(0); o->Clear(); }
                { On on(1); r->Clear(); }
                ready = false;
            }
            break;
        }
        compare_logs("op");
        same_map("after op", o, r);
        if (g_failures > 20) break;
    }
    { On on(0); at<FnDel>(0x000252A0)(o, nullptr, 1); }
    { On on(1); r->~GmNameMap(); new (r) GmNameMap; ::operator delete(nullptr); }
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
}

static void run_map_edges() {
    g_context = "names equal";
    static const char* const pairs[][2] = {{"", ""}, {"A", "A"}, {"A", "B"}, {"AB", "A"}, {"ABCDEFG", "ABCDEFG"}, {"ABCDEFGH", "ABCDEFGH"}, {"ABCDEFGH", "ABCDEFGX"}, {"ABCDEFGX", "ABCDEFGH"},
                                           {"ABCDEFGHI", "ABCDEFGHJ"}, {"ABCDEFG", "ABCDEFGH"}, {"X", "ABCDEFGH"}, {"ABCDXFGH", "ABCDEFGH"}, {"ABXDEFGH", "ABCDEFGH"}, {"ABCDEXGH", "ABCDEFGH"}, {"ABCDEFXH", "ABCDEFGH"}, {"AXCDEFGH", "ABCDEFGH"}, {"ABCXEFGH", "ABCDEFGH"}};
    for (const auto& p : pairs) {
        char a[16] = {0}, b[16] = {0};
        strncpy_s(a, p[0], _TRUNCATE);
        strncpy_s(b, p[1], _TRUNCATE);
        eq(p[0], at<FnEqual>(0x00013FC0)(a, b), AtNamesEqual(a, b));
    }

    g_context = "map, no heap and full";
    void* saved = *(void**)(uintptr_t)0x002C632C;
    *(void**)(uintptr_t)0x002C632C = nullptr;
    GmNameMap::s_heap = nullptr;
    {
        GmNameMap* o = new_original_map();
        GmNameMap r;
        { On on(0); at<FnU>(0x00024DA0)(o, nullptr, 1); }
        { On on(1); r.Init(1); }
        same_map("Init with no heap", o, &r);
        compare_logs("Init with no heap");
        // Fill a table of three and grow it with no heap too.
        const char n1[16] = "ONE", n2[16] = "TWO", n3[16] = "THREE", n4[16] = "FOUR";
        { On on(0); for (const char* n : {n1, n2, n3, n4}) at<FnInsert>(0x00024EE0)(o, nullptr, n, (void*)n); }
        { On on(1); for (const char* n : {n1, n2, n3, n4}) r.Insert(n, (void*)n); }
        same_map("Grow with no heap", o, &r);
        compare_logs("Grow with no heap");
        std::free(o->m_entries);
        std::free(o);
    }
    *(void**)(uintptr_t)0x002C632C = saved;
    GmNameMap::s_heap = saved;

    g_context = "map, sizes at and past the end of the table";
    for (unsigned n : {50158u, 50159u, 100236u, 100237u, 100238u, 150000u}) {
        GmNameMap* o = new_original_map();
        GmNameMap r;
        { On on(0); at<FnU>(0x00024DA0)(o, nullptr, n); }
        { On on(1); r.Init(n); }
        eq("size", o->m_size, r.m_size);
        compare_logs("Init, large");
        { On on(0); o->Clear(); }
        { On on(1); r.Clear(); }
        std::free(o);
    }

    g_context = "map, a free slot ends a search";
    {
        GmNameMap* o = new_original_map();
        GmNameMap r;
        { On on(0); at<FnU>(0x00024DA0)(o, nullptr, 10); }
        { On on(1); r.Init(10); }
        const char name[16] = "SOUGHT";
        const unsigned h = r.HashOf(name);
        // The name sits one past a free slot that holds another name: only the free slot's check stops the search short.
        for (GmNameMap* m : {o, &r}) {
            const unsigned next = (h + 1) % m->m_size;
            std::memcpy(m->m_entries[h].m_name, "OTHERNAM", 8);
            m->m_entries[h].m_value = nullptr;
            std::memcpy(m->m_entries[next].m_name, "SOUGHT\0\0", 8);
            m->m_entries[next].m_value = (void*)(uintptr_t)7;
        }
        eq("Find", (intptr_t)at<FnFind>(0x00025150)(o, nullptr, name), (intptr_t)r.Find(name));
        eq("Find stops", (intptr_t)r.Find(name), 0);
        unsigned ia = 0x77, ib = 0x77;
        eq("Find", (intptr_t)at<FnFind2>(0x000251D0)(o, nullptr, name, &ia), (intptr_t)r.Find(name, &ib));
        eq("index", ia, ib);
        { On on(0); o->Clear(); }
        { On on(1); r.Clear(); }
        std::free(o);
    }

    g_context = "map in a group";
    {
        GmNameMap* o = new_original_map();
        GmNameMap r;
        GmNameMap o2, r2, o3, r3;
        GmNameMapGroup og = {}, rg = {}, parent = {};
        // A list of three: leave from the middle, the head, and last, with and without a parent.
        o->m_group = &og;
        r.m_group = &rg;
        o2.m_group = &og;
        r2.m_group = &rg;
        o3.m_group = &og;
        r3.m_group = &rg;
        og.m_first = &o2;
        o2.m_next = o;
        o->m_next = &o3;
        rg.m_first = &r2;
        r2.m_next = &r;
        r.m_next = &r3;
        og.m_parent = &parent;
        rg.m_parent = &parent;
        { On on(0); o->Clear(); }
        { On on(1); r.Clear(); }
        same_map("Clear in a group", o, &r);
        eq("next link", og.m_first == &o2 && o2.m_next == &o3, rg.m_first == &r2 && r2.m_next == &r3);
        compare_logs("Clear in a group");
        // Something not in the list: no change.
        typedef void(__fastcall* FnDetach)(void*, void*, void*);
        { On on(0); at<FnDetach>(0x00017E80)(&og, nullptr, o); }
        { On on(1); rg.Detach(&r); }
        eq("still there", og.m_first == &o2, rg.m_first == &r2);
        compare_logs("Detach of a stranger");
        { On on(0); at<FnDetach>(0x00017E80)(&og, nullptr, &o2); }
        { On on(1); rg.Detach(&r2); }
        eq("head left", o2.m_group == nullptr, r2.m_group == nullptr);
        eq("head moved", og.m_first == &o3, rg.m_first == &r3);
        compare_logs("Detach of the head");
        { On on(0); at<FnDetach>(0x00017E80)(&og, nullptr, &o3); }
        { On on(1); rg.Detach(&r3); }
        eq("empty", og.m_first == nullptr, rg.m_first == nullptr);
        compare_logs("Detach of the last, with a parent");
        // Without a parent, the last one leaves quietly.
        og.m_parent = rg.m_parent = nullptr;
        o2.m_group = &og;
        r2.m_group = &rg;
        o2.m_next = nullptr;
        r2.m_next = nullptr;
        og.m_first = &o2;
        rg.m_first = &r2;
        { On on(0); at<FnDetach>(0x00017E80)(&og, nullptr, &o2); }
        { On on(1); rg.Detach(&r2); }
        compare_logs("Detach of the last, without a parent");
        o2.m_entries = nullptr;
        r2.m_entries = nullptr;
        o3.m_entries = nullptr;
        r3.m_entries = nullptr;
        std::free(o);
    }
    g_context = "map destructors";
    {
        GmNameMap* o = new_original_map();
        GmNameMap r;
        { On on(0); at<FnU>(0x00024DA0)(o, nullptr, 5); }
        { On on(1); r.Init(5); }
        { On on(0); at<Fn0>(0x00025260)(o, nullptr); }
        { On on(1); r.~GmNameMap(); }
        eq("vftable", *(intptr_t*)o, 0x00272E60);
        compare_logs("destructor");
        eq("cleared", (intptr_t)o->m_entries, 0);
        eq("size", o->m_size, 0);
        new (&r) GmNameMap;
        at<Fn0>(0x00024C60)(o, nullptr);
        at<FnDel>(0x000252A0)(o, nullptr, 0);  // Not freed by the call.
        std::free(o);
    }
}

// ---- rumble ------------------------------------------------------------------------------------------------------------
static GmRumbleEffects* new_original_effects() {
    void* p = std::calloc(1, sizeof(GmRumbleEffects));
    at<Fn0>(0x00074440)(p, nullptr);
    return (GmRumbleEffects*)p;
}

static void same_effect(const GmRumbleEffect& a, const GmRumbleEffect& b) {
    eq("ready", a.m_ready, b.m_ready);
    if (!a.m_ready || !b.m_ready) return;
    char x[9], y[9];
    name8(x, a.m_name);
    name8(y, b.m_name);
    eqs("effect name", x, y);
    eq("frames", a.m_frames.m_count, b.m_frames.m_count);
    if (a.m_frames.m_count != b.m_frames.m_count) return;
    eq("frame bits", a.m_frames.m_count ? std::memcmp(a.m_frames.m_items, b.m_frames.m_items, a.m_frames.m_count * sizeof(GmRumbleFrame)) : 0, 0);
}

// An effect whose name failed to parse has an empty name followed by stack bytes, and the hash reads past the terminator, so its slot depends on those bytes, in the original too.
static bool has_empty_name(const GmRumbleEffects* m) {
    for (unsigned i = 0; i < m->m_effects.m_count; i++) {
        if (m->m_effects.m_items[i].m_name[0] == 0) return true;
    }
    return false;
}

static void same_effects(const char* what, const GmRumbleEffects* o, const GmRumbleEffects* r) {
    eq(what, o->m_loaded, r->m_loaded);
    eq(what, o->m_effects.m_count, r->m_effects.m_count);
    eq(what, o->m_effects.m_items != nullptr, r->m_effects.m_items != nullptr);
    eq(what, o->m_byName.m_size, r->m_byName.m_size);
    if (o->m_effects.m_count != r->m_effects.m_count || !o->m_effects.m_items || !r->m_effects.m_items) return;
    for (unsigned i = 0; i < o->m_effects.m_count; i++) same_effect(o->m_effects.m_items[i], r->m_effects.m_items[i]);
    if (o->m_byName.m_size != r->m_byName.m_size || !o->m_byName.m_entries || !r->m_byName.m_entries) return;
    if (has_empty_name(r)) return;
    for (unsigned i = 0; i < o->m_byName.m_size; i++) {
        const void* a = o->m_byName.m_entries[i].m_value;
        const void* b = r->m_byName.m_entries[i].m_value;
        eq("map slot used", a != nullptr, b != nullptr);
        if (a && b) eq("map slot is the same effect", (const GmRumbleEffect*)a - o->m_effects.m_items, (const GmRumbleEffect*)b - r->m_effects.m_items);
    }
}

static int g_rumble_effects_seen;

static void run_rumble(const std::string& name, const std::vector<unsigned char>& data, GmRumbleEffects* o, GmRumbleEffects* r) {
    g_context = name;
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    { On on(0); at<FnS>(0x000748A0)(o, nullptr, "Design\\Control\\rumble"); }
    { On on(1); r->Load("Design\\Control\\rumble"); }
    same_effects("after Load", o, r);
    compare_logs("Load");
    g_rumble_effects_seen = (int)r->m_effects.m_count;
    typedef void*(__fastcall* FnFindE)(void*, void*, const char*);
    for (unsigned i = 0; i < r->m_effects.m_count && !has_empty_name(r); i++) {
        char query[16] = {0};
        std::memcpy(query, r->m_effects.m_items[i].m_name, 8);
        GmRumbleEffect* a = (GmRumbleEffect*)at<FnFindE>(0x000741B0)(o, nullptr, query);
        GmRumbleEffect* b = r->Find(query);
        eq("Find", a != nullptr, b != nullptr);
        if (a && b) eq("Find index", a - o->m_effects.m_items, b - r->m_effects.m_items);
    }
    // A map never initialised has size 0, and looking in it divides by zero, in both.
    if (o->m_byName.m_size) eq("Find absent", (intptr_t)at<FnFindE>(0x000741B0)(o, nullptr, "ABSENT__"), (intptr_t)r->Find("ABSENT__"));
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
}

static void run_rumble_text(const std::string& name, const std::string& text) {
    const std::vector<unsigned char> data(text.begin(), text.end());
    GmRumbleEffects* o = new_original_effects();
    GmRumbleEffects* r = new GmRumbleEffects;
    run_rumble(name, data, o, r);
    { On on(0); at<Fn0>(0x00074710)(o, nullptr); }
    { On on(1); r->~GmRumbleEffects(); }
    compare_logs("destructor");
    std::free(o);
    ::operator delete(r);
}

static void run_rumble_synthetic() {
    static const char* const texts[] = {
        "",
        "// nothing",
        "other 1 2 3",
        "RumbleEffects [0] { }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.25 Right 0.125 } } } }",
        "RumbleEffects [1] { RumbleEffect \"12345678\" { KeyFrames [0] { } } }",
        "RumbleEffects [1] { RumbleEffect \"123456789\" { KeyFrames [0] { } } }",
        "RumbleEffects [2] { RumbleEffect \"A\" { KeyFrames [2] { KeyFrame 0.0 { Left 0.1 Right 0.2 } KeyFrame 1.5 { Left 0.3 Right 0.4 } } } RumbleEffect \"B\" { KeyFrames [1] { KeyFrame 2.25 { Left 0.5 Right 0.6 } } } }",
        "RumbleEffects [2] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.0 { Left 0.1 Right 0.2 } } } RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.0 { Left 0.1 Right 0.2 } } } }",
        // Wrong or missing tokens at each point: what the reader skips, and what a mistake does to the frames.
        "RumbleEffects [1] { Other \"A\" }",
        "RumbleEffects [1] { RumbleEffect 5 { KeyFrames [1] { } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" KeyFrames [1] { } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { Other [1] } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames 1 { } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { Other } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 1 { Left 0.1 Right 0.2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 Left 0.1 Right 0.2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Right 0.2 Left 0.1 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 1 Right 0.2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.1 Other 0.2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.1 Right 2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.1 Right 0.2 ] } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.1 Right 0.2 } ] }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.1 Right 0.2 } } ] ",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.1 Right 0.2 } } } ] ",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.5",
        // Case does not matter to the keywords.
        "rumbleeffects [1] { rumbleeffect \"a\" { keyframes [1] { keyframe 0.5 { left 0.1 right 0.2 } } } }",
        // Two blocks: the effect array grows or shrinks, keeping what it held, and the map starts over.
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [2] { KeyFrame 0.1 { Left 0.1 Right 0.1 } KeyFrame 0.2 { Left 0.2 Right 0.2 } } } } RumbleEffects [3] { RumbleEffect \"B\" { KeyFrames [1] { KeyFrame 0.3 { Left 0.3 Right 0.3 } } } RumbleEffect \"C\" { KeyFrames [0] { } } RumbleEffect \"D\" { KeyFrames [1] { KeyFrame 0.4 { Left 0.4 Right 0.4 } } } }",
        "RumbleEffects [3] { RumbleEffect \"A\" { KeyFrames [0] { } } RumbleEffect \"B\" { KeyFrames [1] { KeyFrame 0.5 { Left 0.5 Right 0.5 } } } RumbleEffect \"C\" { KeyFrames [0] { } } } RumbleEffects [1] { RumbleEffect \"D\" { KeyFrames [2] { KeyFrame 0.1 { Left 0.1 Right 0.1 } KeyFrame 0.2 { Left 0.2 Right 0.2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.1 { Left 0.1 Right 0.1 } } } } RumbleEffects [1] { RumbleEffect \"B\" { KeyFrames [2] { KeyFrame 0.1 { Left 0.1 Right 0.1 } KeyFrame 0.2 { Left 0.2 Right 0.2 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [2] { KeyFrame 0.1 { Left 0.1 Right 0.1 } KeyFrame 0.2 { Left 0.2 Right 0.2 } } } } RumbleEffects [1] { RumbleEffect \"B\" { KeyFrames [1] { KeyFrame 0.3 { Left 0.3 Right 0.3 } } } }",
        "RumbleEffects [1] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.1 { Left 0.1 Right 0.1 } } } } RumbleEffects [1] { RumbleEffect \"B\" { KeyFrames [0] { } } }",
        "RumbleEffects [2] { RumbleEffect \"A\" { KeyFrames [1] { KeyFrame 0.1 { Left 0.1 Right 0.1 } } } RumbleEffect \"B\" { KeyFrames [1] { KeyFrame 0.1 { Left 0.1 Right 0.1 } } } } RumbleEffects [0] { }",
    };
    for (const char* t : texts) run_rumble_text(std::string("rumble [") + t + "]", t);
}

static void run_rumble_parts() {
    g_context = "frame";
    float raw[3] = {1, 2, 3};
    at<Fn0>(0x001015C0)(raw, nullptr);
    GmRumbleFrame made;
    eq("cleared", std::memcmp(raw, &made, sizeof raw), 0);

    g_context = "frame array";
    GmRumbleFrameArray oa = {nullptr, 0}, ra = {nullptr, 0};
    const auto resize = [&](unsigned n) {
        { On on(0); at<FnU>(0x00074320)(&oa, nullptr, n); }
        { On on(1); ra.Resize(n); }
        eq("count", oa.m_count, ra.m_count);
        eq("items", oa.m_items != nullptr, ra.m_items != nullptr);
        if (oa.m_count && ra.m_count) eq("bits", std::memcmp(oa.m_items, ra.m_items, oa.m_count * sizeof(GmRumbleFrame)), 0);
        compare_logs("frames Resize");
    };
    resize(0);
    resize(3);
    for (unsigned i = 0; i < 3; i++) {
        oa.m_items[i].m_time = ra.m_items[i].m_time = (float)i + 0.5f;
        oa.m_items[i].m_left = ra.m_items[i].m_left = (float)i + 0.25f;
        oa.m_items[i].m_right = ra.m_items[i].m_right = (float)i + 0.125f;
    }
    resize(3);
    resize(5);
    resize(2);
    resize(1);
    resize(0);
    *(void**)(uintptr_t)0x002CF168 = nullptr;
    GmRumbleFrameArray::s_heap = nullptr;
    resize(2);
    resize(0);
    *(void**)(uintptr_t)0x002CF168 = &g_heap_token;
    GmRumbleFrameArray::s_heap = &g_heap_token;

    g_context = "effect array";
    GmRumbleEffectArray oe = {nullptr, 0}, re = {nullptr, 0};
    const auto eresize = [&](unsigned n) {
        { On on(0); at<FnU>(0x00074540)(&oe, nullptr, n); }
        { On on(1); re.Resize(n); }
        eq("count", oe.m_count, re.m_count);
        eq("items", oe.m_items != nullptr, re.m_items != nullptr);
        compare_logs("effects Resize");
    };
    eresize(0);
    eresize(1);
    eresize(1);
    { On on(0); at<Fn0>(0x000742E0)(&oe, nullptr); }
    { On on(1); re.Free(); }
    eq("freed", oe.m_items == nullptr && oe.m_count == 0, re.m_items == nullptr && re.m_count == 0);
    compare_logs("effects Free");
    { On on(0); at<Fn0>(0x000742E0)(&oe, nullptr); }
    { On on(1); re.Free(); }
    compare_logs("effects Free again");
    *(void**)(uintptr_t)0x002CF164 = nullptr;
    GmRumbleEffectArray::s_heap = nullptr;
    eresize(1);
    eresize(0);
    *(void**)(uintptr_t)0x002CF164 = &g_heap_token;
    GmRumbleEffectArray::s_heap = &g_heap_token;

    g_context = "effect";
    GmRumbleEffect ox, rx;
    typedef void(__fastcall* FnInitE)(void*, void*, const char*, unsigned);
    { On on(0); at<FnInitE>(0x000749B0)(&ox, nullptr, "ABCDEFGH", 2); }
    { On on(1); rx.Init("ABCDEFGH", 2); }
    same_effect(ox, rx);
    compare_logs("effect Init");
    GmRumbleFrame f;
    f.m_time = 1.5f;
    f.m_left = 0.5f;
    f.m_right = 0.75f;
    typedef void(__fastcall* FnSet)(void*, void*, unsigned, const void*);
    typedef void*(__fastcall* FnGet)(void*, void*, unsigned);
    at<FnSet>(0x00074900)(&ox, nullptr, 1, &f);
    rx.SetFrame(1, f);
    same_effect(ox, rx);
    eq("Frame", (const char*)at<FnGet>(0x000748F0)(&ox, nullptr, 1) - (const char*)ox.m_frames.m_items, (const char*)rx.Frame(1) - (const char*)rx.m_frames.m_items);
    typedef const char*(__fastcall* FnName)(void*, void*);
    eq("Name", at<FnName>(0x000EB540)(&ox, nullptr) - (const char*)&ox, rx.Name() - (const char*)&rx);
    // Assignment: from one with frames, from one without, to itself.
    typedef void*(__fastcall* FnAssign)(void*, void*, const void*);
    GmRumbleEffect oy, ry;
    { On on(0); at<FnAssign>(0x000744B0)(&oy, nullptr, &ox); }
    { On on(1); ry = rx; }
    same_effect(oy, ry);
    compare_logs("assign with frames");
    { On on(0); at<FnAssign>(0x000744B0)(&oy, nullptr, &oy); }
    { On on(1); GmRumbleEffect& alias = ry; ry = alias; }
    same_effect(oy, ry);
    compare_logs("assign to itself");
    GmRumbleEffect oz, rz;
    oz.m_ready = rz.m_ready = 1;
    std::memcpy(oz.m_name, "EMPTYNAM", 8);
    std::memcpy(rz.m_name, "EMPTYNAM", 8);
    { On on(0); at<FnAssign>(0x000744B0)(&oy, nullptr, &oz); }
    { On on(1); ry = rz; }
    same_effect(oy, ry);
    eq("frames dropped", oy.m_frames.m_items == nullptr, ry.m_frames.m_items == nullptr);
    compare_logs("assign without frames");
    { On on(0); at<FnAssign>(0x000744B0)(&oy, nullptr, &oz); }
    { On on(1); ry = rz; }
    compare_logs("assign without frames, to none");
    // Init over a ready effect.
    { On on(0); at<FnInitE>(0x000749B0)(&ox, nullptr, "SECOND", 1); }
    { On on(1); rx.Init("SECOND", 1); }
    same_effect(ox, rx);
    compare_logs("effect Init again");
    { On on(0); at<Fn0>(0x00074940)(&ox, nullptr); }
    { On on(1); rx.~GmRumbleEffect(); }
    compare_logs("effect destructor");
    eq("ready after destruction", ox.m_ready, rx.m_ready);
    eq("count after destruction", ox.m_frames.m_count, rx.m_frames.m_count);
    eq("items after destruction", (intptr_t)ox.m_frames.m_items, (intptr_t)rx.m_frames.m_items);
    new (&ox) GmRumbleEffect;
    new (&rx) GmRumbleEffect;

    g_context = "effects, constructor";
    void* op = std::malloc(sizeof(GmRumbleEffects));
    std::memset(op, 0xCD, sizeof(GmRumbleEffects));
    at<Fn0>(0x00074440)(op, nullptr);
    GmRumbleEffects* o = (GmRumbleEffects*)op;
    GmRumbleEffects r;
    eq("loaded", o->m_loaded, r.m_loaded);
    eq("items", (intptr_t)o->m_effects.m_items, (intptr_t)r.m_effects.m_items);
    eq("count", o->m_effects.m_count, r.m_effects.m_count);
    eq("map", (intptr_t)o->m_byName.m_entries, (intptr_t)r.m_byName.m_entries);
    // A file that will not open: quietly nothing, and still marked as loaded by Load.
    g_context = "rumble, no file";
    g_fs->has = false;
    { On on(0); at<FnS>(0x000748A0)(o, nullptr, "MISSING"); }
    { On on(1); r.Load("MISSING"); }
    same_effects("after Load of nothing", o, &r);
    compare_logs("Load of nothing");
    g_fs->has = true;
    g_fs->open_status = 9;
    { On on(0); at<FnS>(0x00074800)(o, nullptr, "BROKEN"); }
    { On on(1); r.LoadFile("BROKEN"); }
    compare_logs("LoadFile with an open error");
    g_fs->open_status = 0;
    // A path with its own extension keeps it: AtIoFileRef adds RMB otherwise.
    static const unsigned char empty[] = "";
    g_fs->files[0].data = g_fs->files[1].data = empty;
    g_fs->files[0].size = g_fs->files[1].size = 0;
    { On on(0); at<FnS>(0x00074800)(o, nullptr, "DIR\\NAME.ABC"); }
    { On on(1); r.LoadFile("DIR\\NAME.ABC"); }
    compare_logs("LoadFile with an extension");
    // A tokenizer that failed to appear is skipped; the allocator cannot be made to fail here, so this path is not reached.
    std::free(op);
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
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
    int seed = 1;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::atoi(argv[++i]);
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_rmb <default.xbe> <cases dir> [--entries FILE] [--mutate] [--seed N]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00018010, (const void*)o_remove_from_parent);
    at<void(__cdecl*)()>(0x00125BC7)();  // The C library's floating-point formatters, as in oracle_tok.cpp.
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002CF164u, 0x002CF168u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmRumbleEffectArray::s_heap = &g_heap_token;
    GmRumbleFrameArray::s_heap = &g_heap_token;
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_map_edges();
    for (int s = 0; s < 40; s++) run_map_ops(seed * 1000 + s, 1500);
    run_rumble_parts();
    run_rumble_synthetic();

    std::string dir = pos[1], file;
    {
        FILE* index = nullptr;
        if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
        char line[1024];
        while (std::fgets(line, sizeof line, index)) {
            if (!std::strstr(line, "RUMBLE.RMB")) continue;
            *std::strchr(line, '\t') = 0;
            file = line;
            break;
        }
        std::fclose(index);
    }
    if (file.empty()) { std::printf("RUMBLE.RMB not listed\n"); return 2; }
    std::vector<unsigned char> real;
    {
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + file).c_str(), "rb") || !f) { std::printf("cannot read the rumble file\n"); return 2; }
        std::fseek(f, 0, SEEK_END);
        real.resize((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(real.data(), 1, real.size(), f);
        std::fclose(f);
    }
    GmRumbleEffects* o = new_original_effects();
    GmRumbleEffects* r = new GmRumbleEffects;
    run_rumble("RUMBLE.RMB", real, o, r);
    int effects = g_rumble_effects_seen;
    run_rumble("RUMBLE.RMB again", real, o, r);  // Over what was loaded.
    if (effects == 0) { std::printf("no effects read\n"); g_failures++; }

    report_entries();
    std::printf("%s: seed %d, RUMBLE.RMB with %d effects; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", seed, effects, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
