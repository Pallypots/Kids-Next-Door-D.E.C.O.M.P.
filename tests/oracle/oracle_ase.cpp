// Differential oracle for the particle emitters' loader, .ASE (Windows x86, local only). See docs/verification.md.
//
//   oracle_ase <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ASE file listed in index.txt, loaded through the file code on both sides, four ways (fresh, over itself, after a clear, and into the deleting form); written text for each word and for every pair of words; each reader called
// directly on a tokenizer of the harness's whose Error can stop it; and each helper called directly on made-up values. Nothing stands in for the classes under test: emitters, the query pool and the queries are all the two sides' own.
// What a material search finds is a real map, one owner to a side, filled with every string of the text except names beginning with X. Only the heap and report helpers are doubles, as everywhere.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "at/AtEmitter.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMaterial.h"
#include "at/AtText.h"
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
static volatile LONG g_progress;
static unsigned g_crashes_seen, g_emitters_seen, g_query_pools_seen, g_words_changed;

static AtMaterialOwner* g_own[2];
static AtNameTableOwner* g_scene[2];

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnRet)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnReader)(void*, void*, void*, void*);
typedef void(__fastcall* FnCross)(void*, void*, const void*, const void*);
typedef void(__fastcall* FnReal)(void*, void*, float);
typedef void(__fastcall* FnPtr)(void*, void*, const void*);
typedef void(__fastcall* FnInit)(void*, void*, unsigned, unsigned);
typedef void(__fastcall* FnAttach3)(void*, void*, unsigned, void*, void*);
typedef void(__fastcall* FnAttach4)(void*, void*, unsigned, void*, void*, void*);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);
typedef void(__fastcall* FnCount)(void*, void*, unsigned);
typedef void(__cdecl* FnSetHeap)(void*);

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
            g_scene[side] = new AtNameTableOwner;
            g_scene[side]->m_device = (AtSceneDevice*)g_own[side];
            AtMaterialOwner& w = *g_own[side];
            const unsigned pool = (unsigned)spaces.size() + 8;
            w.m_materialSpaces.Init(pool);
            g_scene[side]->m_emitterSpaces.Init(pool);
            const auto fill = [&](uint32_t salt) {
                GmNameMap* m = new GmNameMap;
                m->Init((unsigned)names.size() + 1);
                for (const std::string& n : names) m->Insert(n.c_str(), value_of(n, salt));
                maps[side].push_back(m);
                return m;
            };
            w.m_materials.Add(fill(0));
            for (const std::string& s : spaces) w.m_materialSpaces.Add(s.c_str(), fill(1));
        }
        g_recording = was;
    }
    ~Env() {
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            for (GmNameMap* m : maps[side]) delete m;
            delete g_own[side];
            delete g_scene[side];
            g_own[side] = nullptr;
            g_scene[side] = nullptr;
        }
    }
};

// Every quoted string but X names, cut to the eight bytes a name has, and those following another string on its line, or a NameSpace word.
static void gather(const std::string& d, std::set<std::string>& names, std::set<std::string>& spaces) {
    size_t i = 0;
    bool last_was_string = false;
    while (i < d.size()) {
        const char c = d[i];
        if (c == '\n') { last_was_string = false; i++; continue; }
        if (c == '"') {
            size_t e = i + 1;
            while (e < d.size() && d[e] != '"' && d[e] != '\n') e++;
            std::string s = d.substr(i + 1, e - i - 1);
            if (s.size() > 8) s.resize(8);
            if (!s.empty() && s[0] != 'X') {
                names.insert(s);
                if (last_was_string) spaces.insert(s);
            }
            last_was_string = true;
            i = e + 1;
            continue;
        }
        i++;
    }
}

// ---- comparing --------------------------------------------------------------------------------------------------------

// Word by word, as floats and as what else they may be. x87 and the host may differ in a NaN's sign, and in nothing else that a word's bits can show.
static void same_words(const char* what, const void* o, const void* r, size_t from, size_t to) {
    for (size_t i = from; i + 4 <= to; i += 4) {
        const uint32_t a = *(const uint32_t*)((const char*)o + i);
        const uint32_t b = *(const uint32_t*)((const char*)r + i);
        g_checks++;
        if (a == b) continue;
        const float fa = *(const float*)&a, fb = *(const float*)&b;
        if (fa != fa && fb != fb) continue;
        eq((std::string(what) + " word " + std::to_string(i)).c_str(), a, b);
    }
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
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
                eq("slot's item", (char*)x.m_value - (char*)obase, (char*)y.m_value - (char*)rbase);
            }
        }
    }
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
}

static void same_query(const char* what, const AtSegmentHitList* o, const AtSegmentHitList* r, size_t size, uint32_t vftable, const void* b80o, const void* b84o, const void* b80r, const void* b84r) {
    eq(what, *(const uint32_t*)o, vftable);
    eq("head A", (char*)o->m_first0 - (char*)o, (char*)r->m_first0 - (char*)r);
    eq("head B", (char*)o->m_second0 - (char*)o, (char*)r->m_second0 - (char*)r);
    eq("head C", (char*)o->m_first2 - (char*)o, (char*)r->m_first2 - (char*)r);
    eq("head D", (char*)o->m_second2 - (char*)o, (char*)r->m_second2 - (char*)r);
    eq("storage 80", (char*)o->m_keys - (char*)b80o, (char*)r->m_keys - (char*)b80r);
    eq("storage 84", (char*)o->m_hits - (char*)b84o, (char*)r->m_hits - (char*)b84r);
    same_words(what, o, r, 4, 0x20);
    same_words(what, o, r, 0x28, 0x58);
    same_words(what, o, r, 0x60, 0x80);
    same_words(what, o, r, 0x88, 0x90);
    same_words(what, o, r, 0x94, size);  // Word 0x90 of a segment query is storage, compared by its caller.
}

static void same_query_pool(const AtSegmentQueryPool& x, const AtSegmentQueryPool& y) {
    eq("pool's vftable", *(const uint32_t*)&x, 0x002764D4);
    same_words("pool", &x, &y, 0x04, 0x08);
    eq("pool's base queries", x.m_queries != nullptr, y.m_queries != nullptr);
    same_words("pool", &x, &y, 0x0C, 0x10);
    same_words("pool", &x, &y, 0x18, 0x34);
    same_words("pool", &x, &y, 0x3C, 0x40);
    eq("entries 8", x.m_entries8 != nullptr, y.m_entries8 != nullptr);
    eq("entries 2C", x.m_entries2C != nullptr, y.m_entries2C != nullptr);
    eq("entries 10", x.m_entries10 != nullptr, y.m_entries10 != nullptr);
    eq("queries", x.m_segmentQueries != nullptr, y.m_segmentQueries != nullptr);
    if (x.m_segmentQueries && y.m_segmentQueries && x.m_count == y.m_count) {
        eq("query cookie", ((const unsigned*)x.m_segmentQueries)[-1], ((const unsigned*)y.m_segmentQueries)[-1]);
        for (unsigned i = 0; i < x.m_count; i++) {
            same_query("query", &x.m_segmentQueries[i], &y.m_segmentQueries[i], sizeof(AtSegmentQuery), 0x002770F4, x.m_entries8, x.m_entries2C, y.m_entries8, y.m_entries2C);
            eq("storage 90", (char*)x.m_segmentQueries[i].m_records - (char*)x.m_entries10, (char*)y.m_segmentQueries[i].m_records - (char*)y.m_entries10);
        }
        g_query_pools_seen++;
    }
}

static void same_emitter(const AtEmitter& x, const AtEmitter& y) {
    eq("owner is the owner", x.m_owner == g_scene[0], y.m_owner == g_scene[1]);
    eq("owner set", x.m_owner != nullptr, y.m_owner != nullptr);
    eq("list's count", x.m_materials.m_count, y.m_materials.m_count);
    eq("list", x.m_materials.m_items != nullptr, y.m_materials.m_items != nullptr);
    if (x.m_materials.m_items && y.m_materials.m_items && x.m_materials.m_count == y.m_materials.m_count) {
        eq("list's items", std::memcmp(x.m_materials.m_items, y.m_materials.m_items, x.m_materials.m_count * sizeof(void*)), 0);
    }
    same_words("emitter", &x, &y, 0x0C, 0x230);
    same_query_pool(x.m_queries, y.m_queries);
}

static void same_database(const AtEmitterDatabase* o, const AtEmitterDatabase* r) {
    eq("count", o->m_count, r->m_count);
    eq("emitters", o->m_emitters != nullptr, r->m_emitters != nullptr);
    same_map(o, r, o->m_emitters, r->m_emitters);
    const GmNameMapGroup& og = g_scene[0]->m_emitters;
    const GmNameMapGroup& rg = g_scene[1]->m_emitters;
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &og, r->m_group == &rg);
    eq("first in the plain group", og.m_first == o, rg.m_first == r);
    eq("spaces in use", g_scene[0]->m_emitterSpaces.m_free - g_scene[0]->m_emitterSpaces.m_pool, g_scene[1]->m_emitterSpaces.m_free - g_scene[1]->m_emitterSpaces.m_pool);
    if (o->m_emitters && r->m_emitters && o->m_count == r->m_count) {
        // The block begins with the count, 0x10 before the first.
        eq("cookie", *(const unsigned*)((const char*)o->m_emitters - 0x10), *(const unsigned*)((const char*)r->m_emitters - 0x10));
        for (unsigned i = 0; i < o->m_count; i++) same_emitter(o->m_emitters[i], r->m_emitters[i]);
        g_emitters_seen += o->m_count;
    }
}

// ---- through the file code --------------------------------------------------------------------------------------------

template <class T> static T* raw_object(size_t size, unsigned char fill) {
    unsigned char* p = (unsigned char*)std::malloc(size);
    std::memset(p, fill, size);
    return (T*)p;
}

static AtEmitterDatabase* make_db(int side, unsigned char fill) {
    AtEmitterDatabase* d = raw_object<AtEmitterDatabase>(sizeof(AtEmitterDatabase), fill);
    if (side == 0) {
        eq("constructor returns this", (uintptr_t)at<FnRet>(0x0003F590)(d, nullptr), (uintptr_t)d);
        eq("database's vftable", *(const uint32_t*)d, 0x0027602C);
    } else {
        new (d) AtEmitterDatabase;
    }
    return d;
}

static void load_both(AtEmitterDatabase* o, AtEmitterDatabase* r, const char* what) {
    const int top = g_fs->m_path.m_top;
    const bool oc = guarded([&] { On on(0); at<FnLoad>(0x0003F760)(o, nullptr, g_scene[0], "ART\\THING", 77); });
    g_recording = false;
    const int otop = g_fs->m_path.m_top;
    g_fs->m_path.m_top = top;
    const bool rc = guarded([&] { On on(1); r->Load(g_scene[1], "ART\\THING", 77); });
    g_recording = false;
    eq("crash in load", oc, rc);
    eq("left pushed", otop, g_fs->m_path.m_top);
    g_fs->m_path.m_top = top;
    if (oc) g_crashes_seen++;
    same_database(o, r);
    compare_logs(what);
}

static void run_text(const std::string& data, const std::string& context, unsigned char fill) {
    g_context = context;
    InterlockedIncrement(&g_progress);
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, "ART\\SUB\\THING.ASE");
    g_fill = fill;
    AtEmitterDatabase* o = make_db(0, fill);
    AtEmitterDatabase* r = make_db(1, fill);
    same_words("constructed", o, r, 4, sizeof(AtEmitterDatabase));
    load_both(o, r, "load");
    // Over what is there.
    load_both(o, r, "load again");
    // Cleared, and clearing nothing.
    { On on(0); at<Fn0>(0x0003F800)(o, nullptr); at<Fn0>(0x0003F800)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    g_recording = false;
    same_database(o, r);
    compare_logs("clear");
    load_both(o, r, "load after clear");
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002E880)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~AtEmitterDatabase(); }
    compare_logs("destroy");
    same_words("destroyed", o, r, 4, sizeof(AtEmitterDatabase));
    std::free(o);
    std::free(r);
    // The deleting form, with something held.
    o = make_db(0, fill);
    r = make_db(1, fill);
    load_both(o, r, "load to delete");
    { On on(0); at<FnDel>(0x0002E880)(o, nullptr, 1); }
    { On on(1); r->~AtEmitterDatabase(); ::operator delete(r); }
    compare_logs("delete");
}

static std::string one(const std::string& body) {
    return "Emitter[1]\n{\n Emitter \"A\"\n {\n" + body + "\n }\n}\n";
}

// One value of each kind of value, none of them what Reset leaves, so that a store to the wrong field shows. Reals for reals: an integer would read as nothing.
static const char* const kWords[] = {
    "EmissionPoint 1.5 2.5 3.5",
    "EmissionLine 1.5 2.5 3.5 4.5 5.5 6.5",
    "EmissionRectangle 1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5 10.5 11.5 12.5",
    "EmissionCone 0.25 0.5 0.75 12.5",
    "EmissionConeInnerAngle 3.25",
    "MaxEmissionEventRate 33.0",
    "EmissionEventChance 0.625",
    "MaxParticlesPerEvent 3",
    "ParticleChance 0.375",
    "ParticleInitialSpeed 1.125",
    "DeltaInitialSpeed 2.125",
    "ParticleRotationSpeed 3.125",
    "LocalAcceleration 1.25 2.75 -3.5",
    "SquareSize 7.5",
    "LinearSquareChange 1.75",
    "QuadraticSquareChange 2.75 3.75",
    "LinearSquareSizeKeys 1.5 2.25 3.5",
    "QuadraticSquareSizeKeys 1.5 2.25 3.5 4.75 1.25",
    "RectWidthHeight 3.5 4.5",
    "LinearRectChange 1.75 2.75",
    "QuadraticRectChange 2.75 3.75 4.75 5.75",
    "LinearRectSizeKeys 1.5 2.5 3.25 4.25 5.5 6.5",
    "QuadraticRectSizeKeys 1.5 2.5 3.25 4.25 5.5 6.5 7.75 8.75 1.25 1.75",
    "DiffuseColor 0.1 0.2 0.3 0.4",
    "MotionBlur 3 7.0 0.1 0.2 0.3 0.4",
    "LinearColorChange 0.1 0.2 0.3 0.4",
    "QuadraticColorChange 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8",
    "LinearColorChangeKeys 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 1.5 2.5 3.5 4.5",
    "QuadraticColorChangeKeys 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 1.5 2.5 3.5 4.5 0.9 0.11 0.12 0.13 0.25 0.75 1.25 1.75",
    "MotionBlurLinearColorChange 0.1 0.2 0.3 0.4",
    "MotionBlurQuadraticColorChange 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8",
    "LinearMotionBlurKeys 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 1.5 2.5 3.5 4.5",
    "QuadraticMotionBlurKeys 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 1.5 2.5 3.5 4.5 0.9 0.11 0.12 0.13 0.25 0.75 1.25 1.75",
    "ParticleLifetime 5.5",
    "DeltaParticleLifetime 1.5",
    "EmitterLifetime 9.5",
    "MaxParticles 3",
    "MatAssignment[2]\n{\n\"M\"\n\"XGone\"\n}",
    "MatAssignmentWithNameSpace[2]\n{\n\"M\" \"Space\"\n\"\" \"\"\n}",
    "LocalBoundingSphere 1.5 2.5 3.5 4.5",
    "ParticleRadius 6.5",
    "LineSegIntersectTesting",
    "Opaque",
    "EulerUpdate",
    "EmissionConeScale 0.5 0.25 0.125 1.5 2.5",
};

// A word that is not the one wanted is reported, and read on from; the loop must not reach the end of the text, so these close their braces.
static void run_written() {
    const size_t n = sizeof(kWords) / sizeof(kWords[0]);
    // Each word changes something from what an empty body leaves: this shows the text reaches what it tests, for the words a default could hide.
    {
        const std::string empty = one("");
        const auto read_bytes = [](const std::string& text, std::vector<unsigned char>& out) {
            std::set<std::string> names, spaces;
            gather(text, names, spaces);
            spaces.insert("Space");
            Env env(names, spaces);
            g_fs->files[1].data = (const unsigned char*)text.data();
            g_fs->files[1].size = text.size();
            strcpy_s(g_fs->files[1].m_path, "ART\\THING.ASE");
            g_fill = 0xCD;
            AtEmitterDatabase* r = make_db(1, 0xCD);
            { On on(1); r->Load(g_scene[1], "ART\\THING", 77); }
            g_log[1].n = 0;
            g_message[1][0] = 0;
            const char* first = (const char*)r->m_emitters;
            out.assign(first + 0x0C, first + 0x230);
            r->~AtEmitterDatabase();
            std::free(r);
        };
        std::vector<unsigned char> blank, b;
        read_bytes(empty, blank);
        for (size_t i = 0; i < n; i++) {
            read_bytes(one(kWords[i]), b);
            g_context = kWords[i];
            eq("the word changes nothing", b != blank, 1);
            g_words_changed++;
        }
    }
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        run_text(one(""), "empty body", fill);
        for (size_t i = 0; i < n; i++) run_text(one(kWords[i]), std::string("word: ") + kWords[i], fill);
    }
    // Every pair, in each order: which words undo which.
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (i != j) run_text(one(std::string(kWords[i]) + "\n" + kWords[j]), std::string("pair: ") + kWords[i] + " / " + kWords[j], 0xCD);
        }
    }

    static const char* const kValues[] = {
        // Rates either side of the shortest interval, and none.
        "MaxEmissionEventRate 250.0", "MaxEmissionEventRate 249.9", "MaxEmissionEventRate 250.1", "MaxEmissionEventRate 1000.0", "MaxEmissionEventRate 0.0", "MaxEmissionEventRate -2.0",
        // Chances, each way past the ends.
        "EmissionEventChance -1.0", "EmissionEventChance 0.0", "EmissionEventChance 1.0", "EmissionEventChance 2.0", "EmissionEventChance 0.9999", "EmissionEventChance 0.0001",
        "ParticleChance -1.0", "ParticleChance 0.0", "ParticleChance 1.0", "ParticleChance 2.0", "ParticleChance 0.9999", "ParticleChance 0.0001",
        // Directions: along z, the identity; too short, which normalizing turns to x; along each axis, and off them.
        "EmissionCone 0.0 0.0 1.0 5.5", "EmissionCone 0.0 0.0 -1.0 5.5", "EmissionCone 0.0 0.0 0.0 5.5", "EmissionCone 1.0 0.0 0.0 5.5", "EmissionCone 0.0 1.0 0.0 5.5", "EmissionCone -1.0 0.0 0.0 5.5", "EmissionCone 0.0 -1.0 0.0 5.5",
        "EmissionCone 0.991161 -0.107685 0.077488 10.0", "EmissionCone 0.000001 0.0 1.0 5.5", "EmissionCone 0.0 0.000001 1.0 5.5", "EmissionCone 0.000001 0.000001 1.0 0.0", "EmissionCone 3.0 4.0 12.0 0.0",
        "EmissionCone 1.0 2.0 3.0 4.0\nEmissionConeInnerAngle 2.5", "EmissionConeInnerAngle 2.5\nEmissionCone 1.0 2.0 3.0 4.0",
        // Axes of no length, which the cross product turns to x.
        "EmissionCone 0.0 0.0 1.0 1.0\nEmissionConeScale 0.0 0.0 1.0 1.5 2.5", "EmissionConeScale 0.0 0.0 0.0 1.5 2.5", "EmissionCone 1.0 2.0 3.0 1.0\nEmissionConeScale 3.0 2.0 1.0 0.5 0.25",
        // Blur and material counts: none is reported, and divides by nothing; a negative count is taken as is by the division.
        "MotionBlur 0 1.5 0.1 0.2 0.3 0.4", "MotionBlur -2 1.5 0.1 0.2 0.3 0.4", "MotionBlur 1 1.5 0.1 0.2 0.3 0.4",
        "ParticleLifetime 7.0\nMatAssignment[0]\n{\n}", "MatAssignment[3]\n{\n\"M\"\n\"N\"\n\"\"\n}\nParticleLifetime 7.0", "MatAssignmentWithNameSpace[0]\n{\n}", "ParticleLifetime 7.0\nMatAssignmentWithNameSpace[1]\n{\n\"M\" \"Space\"\n}",
        // Every stream count's terms, alone and together.
        "MaxParticlesPerEvent 5\nDeltaInitialSpeed 1.5", "MaxParticlesPerEvent 5\nEmissionCone 0.0 0.0 1.0 1.5", "MaxParticlesPerEvent 5\nDeltaParticleLifetime 1.5", "MaxParticlesPerEvent 5\nEmissionLine 1.0 1.0 1.0 2.0 2.0 2.0",
        "MaxParticlesPerEvent 5\nEmissionRectangle 1.0 1.0 1.0 2.0 2.0 2.0 3.0 3.0 3.0 4.0 4.0 4.0", "MaxParticlesPerEvent 5\nEmissionPoint 1.0 1.0 1.0",
        "MaxParticlesPerEvent 5\nDeltaInitialSpeed 1.5\nEmissionCone 0.0 0.0 1.0 1.5\nDeltaParticleLifetime 1.5\nEmissionLine 1.0 1.0 1.0 2.0 2.0 2.0",
        // A pool's sizes.
        "LineSegIntersectTesting\nMaxParticles 1", "LineSegIntersectTesting\nMaxParticles 0", "LineSegIntersectTesting\nMaxParticles 7", "MaxParticles 7", "LineSegIntersectTesting",
        // Keys with a time of nothing, or times that coincide: the curves' divisions by nothing.
        "LinearSquareSizeKeys 1.5 2.5 0.0", "QuadraticSquareSizeKeys 1.5 2.5 0.0 1.0 1.0", "QuadraticSquareSizeKeys 1.5 2.5 1.0 1.0 1.0", "QuadraticSquareSizeKeys 1.5 2.5 1.0 1.0 0.0",
        "LinearRectSizeKeys 1.5 2.5 3.5 4.5 0.0 0.0", "QuadraticRectSizeKeys 1.5 2.5 3.5 4.5 1.0 1.0 2.5 2.5 1.0 1.0", "QuadraticRectSizeKeys 1.5 2.5 3.5 4.5 1.0 2.0 2.5 2.5 0.0 0.5",
        // Curves over each other's flags, with the keys held from an earlier word.
        "LinearSquareSizeKeys 1.5 2.5 3.5\nLinearRectSizeKeys 1.5 2.5 3.5 4.5 2.0 3.0\nQuadraticSquareSizeKeys 1.5 2.5 3.5 4.5 1.5",
        "DiffuseColor 0.1 0.2 0.3 0.4\nLinearColorChangeKeys 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 1.5 2.5 3.5 4.5\nDiffuseColor 0.5 0.5 0.5 0.5",
        // Reals as integers, strings for numbers, words where they do not belong. Reported, and read on from.
        "EmissionPoint 1 2 3", "EmissionPoint \"a\" 2.5 3.5", "MaxParticles 2.5", "MaxParticles \"a\"", "MaxParticlesPerEvent \"a\"", "SquareSize \"a\"", "MotionBlur \"a\" 1.0 0.5 0.5 0.5 0.5",
        "Bogus", "7", "2.5", "\"text\"", "NameSpace", "Emitter", "[", "]", "{ }",
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const char* v : kValues) run_text(one(v), std::string("value: ") + v, fill);
    }

    static const char* const kTexts[] = {
        "Emitter[0]\n{\n}\n",
        "NameSpace \"Space\"\nEmitter[1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "NameSpace \"Fresh\"\nEmitter[1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "NameSpace \"\"\nEmitter[1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "NameSpace \"LongerThanEight\"\nEmitter[1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "NameSpace \"Fresh\"\nOther[1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "Other[1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "Emitter[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "Emitter[1]\n{\n Emitter \"A\"\n {\n }\n",
        "Emitter[1]\n{\n Emitter \"LongerThanEight\"\n {\n }\n}\n",
        // Not here: an empty name. The map's hash, 0x00024C20, reads its second byte anyway, which is whatever was on the stack: where it lands cannot be compared.
        "Emitter[1]\n Emitter \"A\"\n {\n }\n}\n",
        "Emitter 1]\n{\n Emitter \"A\"\n {\n }\n}\n",
        "Emitter[1\n{\n Emitter \"A\"\n {\n }\n}\n",
        "Emitter[\"a\"]\n{\n Emitter \"A\"\n {\n }\n}\n",
        // Two of one name: a report, and both are held. And the order of two.
        "Emitter[2]\n{\n Emitter \"A\"\n {\n }\n Emitter \"A\"\n {\n }\n}\n",
        "Emitter[3]\n{\n Emitter \"A\"\n {\n SquareSize 2.5\n }\n Emitter \"B\"\n {\n LineSegIntersectTesting\n MaxParticles 2\n }\n Emitter \"C\"\n {\n MatAssignment[1]\n {\n \"M\"\n }\n }\n}\n",
        // More emitters than the first prime the map begins with.
        "Emitter[40]\n{\n"
        "Emitter \"E0\"\n{\n}\nEmitter \"E1\"\n{\n}\nEmitter \"E2\"\n{\n}\nEmitter \"E3\"\n{\n}\nEmitter \"E4\"\n{\n}\nEmitter \"E5\"\n{\n}\nEmitter \"E6\"\n{\n}\nEmitter \"E7\"\n{\n}\nEmitter \"E8\"\n{\n}\nEmitter \"E9\"\n{\n}\n"
        "Emitter \"F0\"\n{\n}\nEmitter \"F1\"\n{\n}\nEmitter \"F2\"\n{\n}\nEmitter \"F3\"\n{\n}\nEmitter \"F4\"\n{\n}\nEmitter \"F5\"\n{\n}\nEmitter \"F6\"\n{\n}\nEmitter \"F7\"\n{\n}\nEmitter \"F8\"\n{\n}\nEmitter \"F9\"\n{\n}\n"
        "Emitter \"G0\"\n{\n}\nEmitter \"G1\"\n{\n}\nEmitter \"G2\"\n{\n}\nEmitter \"G3\"\n{\n}\nEmitter \"G4\"\n{\n}\nEmitter \"G5\"\n{\n}\nEmitter \"G6\"\n{\n}\nEmitter \"G7\"\n{\n}\nEmitter \"G8\"\n{\n}\nEmitter \"G9\"\n{\n}\n"
        "Emitter \"H0\"\n{\n}\nEmitter \"H1\"\n{\n}\nEmitter \"H2\"\n{\n}\nEmitter \"H3\"\n{\n}\nEmitter \"H4\"\n{\n}\nEmitter \"H5\"\n{\n}\nEmitter \"H6\"\n{\n}\nEmitter \"H7\"\n{\n}\nEmitter \"H8\"\n{\n}\nEmitter \"H9\"\n{\n}\n"
        "}\n",
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const char* t : kTexts) run_text(t, std::string("text: ") + t, fill);
    }
}

// ---- the readers directly ----------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

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
        tokenizer.SetKeywords((const AtIoKeyword*)(uintptr_t)0x002B1F20, AtEmitterDatabase::kKeywordCount, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static AtEmitter* make_emitter(int side, unsigned char fill) {
    AtEmitter* e = (AtEmitter*)_aligned_malloc(sizeof(AtEmitter), 16);
    std::memset(e, fill, sizeof(AtEmitter));
    if (side == 0) eq("emitter's constructor returns this", (uintptr_t)at<FnRet>(0x00051290)(e, nullptr), (uintptr_t)e);
    else new (e) AtEmitter;
    return e;
}

// Text that ends in a body, and a body over an earlier read, one emitter alone.
static void run_emitter_direct() {
    static const char* const kTexts[] = {
        "{\n}\n", "}\n", "", "{", "{\nBogus", "{\nEmissionPoint 1.5 2.5", "{\nMatAssignment", "{\nMatAssignment[1]\n{\n\"M\"", "Bogus {\n}\n",
        "{\nSquareSize 2.5\nLineSegIntersectTesting\nMaxParticles 3\n}\n",
    };
    Env env({"M"}, {"Space"});
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        g_fill = fill;
        for (const char* t : kTexts) {
            g_context = std::string("emitter [") + t + "]";
            InterlockedIncrement(&g_progress);
            AtEmitter* o = make_emitter(0, fill);
            AtEmitter* r = make_emitter(1, fill);
            same_emitter(*o, *r);
            for (int turn = 0; turn < 2; turn++) {
                Direct a(t), b(t);
                const bool oc = guarded([&] { On on(0); at<FnReader>(0x00051380)(o, nullptr, g_scene[0], &a.tokenizer); });
                g_recording = false;
                g_side = 0;
                const bool rc = guarded([&] { On on(1); r->Read(g_scene[1], &b.tokenizer); });
                g_recording = false;
                g_side = 0;
                eq("crash", oc, rc);
                if (oc) g_crashes_seen++;
                compare_logs("emitter read");
                same_emitter(*o, *r);
                eq("left at", a.stream.m_cur - a.stream.m_base, b.stream.m_cur - b.stream.m_base);
            }
            { On on(0); at<Fn0>(0x00051300)(o, nullptr); }
            { On on(1); r->~AtEmitter(); }
            compare_logs("emitter destroyed");
            same_words("emitter destroyed", o, r, 0x0C, 0x230);
            _aligned_free(o);
            _aligned_free(r);
        }
    }
    // A second read with less in it than the first: nothing of the first may stay, the pool included.
    g_fill = 0xCD;
    {
        g_context = "emitter read over another";
        InterlockedIncrement(&g_progress);
        const std::string full = "{\nSquareSize 2.5\nMaxParticlesPerEvent 3\nEmissionLine 1.5 2.5 3.5 4.5 5.5 6.5\nMatAssignment[1]\n{\n\"M\"\n}\nLineSegIntersectTesting\nMaxParticles 3\n}\n";
        AtEmitter* o = make_emitter(0, 0xCD);
        AtEmitter* r = make_emitter(1, 0xCD);
        for (const std::string& t : {full, std::string("{\n}\n")}) {
            Direct a(t), b(t);
            { On on(0); at<FnReader>(0x00051380)(o, nullptr, g_scene[0], &a.tokenizer); }
            { On on(1); r->Read(g_scene[1], &b.tokenizer); }
            compare_logs("emitter read over another");
            same_emitter(*o, *r);
        }
        { On on(0); at<Fn0>(0x00051300)(o, nullptr); }
        { On on(1); r->~AtEmitter(); }
        compare_logs("emitter destroyed");
        _aligned_free(o);
        _aligned_free(r);
    }
    // Reset alone: what it leaves of what was there.
    g_context = "emitter reset";
    {
        AtEmitter* o = make_emitter(0, 0xCD);
        AtEmitter* r = make_emitter(1, 0xCD);
        std::memset(o, 0x77, 0x230);
        std::memset(r, 0x77, 0x230);
        at<Fn0>(0x00050F20)(o, nullptr);
        r->Reset();
        same_words("reset", o, r, 0, 0x230);
        _aligned_free(o);
        _aligned_free(r);
    }
}

// ---- the helpers directly -----------------------------------------------------------------------------------------------

static uint32_t g_seed = 0x1234567;
static uint32_t rnd() { g_seed ^= g_seed << 13; g_seed ^= g_seed >> 17; g_seed ^= g_seed << 5; return g_seed; }
static float rnd_real() {
    switch (rnd() % 16) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return 1.0f;
    case 3: return -1.0f;
    case 4: return 5.0e-6f;
    case 5: return (float)(rnd() % 1000) * 1e-9f;
    case 6: return 1.0e30f;
    default: return ((float)(int)(rnd() % 2000001) - 1000000.0f) / 1024.0f;
    }
}
static float rnd_time() { return (rnd() % 8 == 0) ? 0.0f : ((float)(rnd() % 4000) + 1.0f) / 256.0f; }

static void run_vectors() {
    for (int i = 0; i < 30000; i++) {
        g_context = "vectors";
        float a[3] = {rnd_real(), rnd_real(), rnd_real()}, b[3] = {rnd_real(), rnd_real(), rnd_real()};
        if (rnd() % 16 == 0) std::memcpy(b, a, sizeof b);  // Parallel: nothing to the cross product.
        float o[3] = {7, 8, 9}, r[3] = {7, 8, 9};
        at<FnCross>(0x00050790)(o, nullptr, a, b);
        ((AtVector3*)r)->SetCrossNormalized(*(AtVector3*)a, *(AtVector3*)b);
        same_words("cross", o, r, 0, sizeof o);

        float f[2] = {rnd_real(), rnd_real()}, g[2] = {f[0], f[1]};
        at<Fn0>(0x00021EC0)(f, nullptr);
        ((AtVector2*)g)->Normalize();
        same_words("flat", f, g, 0, sizeof f);
    }
    // Either side of the shortest length, exactly.
    for (const float v : {5.0e-6f, std::nextafter(5.0e-6f, 0.0f), std::nextafter(5.0e-6f, 1.0f)}) {
        float f[2] = {v, 0}, g[2] = {v, 0};
        at<Fn0>(0x00021EC0)(f, nullptr);
        ((AtVector2*)g)->Normalize();
        same_words("flat, at the shortest", f, g, 0, sizeof f);
    }
}

static void fill_curves(AtEmitter* e) {
    float* base = (float*)e;
    for (const std::pair<int, int>& range : std::vector<std::pair<int, int>>{{0x8C, 0xD0}, {0xE0, 0xF0}, {0x100, 0x130}, {0x13C, 0x150}, {0x160, 0x170}, {0x180, 0x1A0}, {0x1A8, 0x1B0}}) {
        for (int at_ = range.first; at_ < range.second; at_ += 4) base[at_ / 4] = rnd_real();
    }
    for (const std::pair<int, int>& range : std::vector<std::pair<int, int>>{{0xD0, 0xE0}, {0xF0, 0x100}, {0x150, 0x160}, {0x170, 0x180}, {0x1A0, 0x1A8}, {0x1B0, 0x1B8}}) {
        for (int at_ = range.first; at_ < range.second; at_ += 4) base[at_ / 4] = rnd_time();
    }
}

static void run_curves() {
    AtEmitter* o = make_emitter(0, 0xCD);
    AtEmitter* r = make_emitter(1, 0xCD);
    static const uint32_t kCurves[][2] = {{0x00050A90, 0}, {0x00050B50, 1}, {0x00050C70, 2}, {0x00050830, 3}};
    for (int turn = 0; turn < 20000; turn++) {
        g_context = "curves";
        fill_curves(o);
        // The same again for the other side: the generator's next values would differ.
        std::memcpy((char*)r + 0x8C, (char*)o + 0x8C, 0x1B8 - 0x8C);
        const unsigned flags = rnd();
        o->m_flags = r->m_flags = flags & 0x7FFFF;
        const auto& curve = kCurves[turn % 4];
        at<Fn0>(curve[0])(o, nullptr);
        switch (curve[1]) {
        case 0: r->CurveFromSizeKeys(); break;
        case 1: r->CurveFromRectKeys(); break;
        case 2: r->CurveFromColorKeys(); break;
        case 3: r->CurveFromBlurKeys(); break;
        }
        same_words("curve", o, r, 0x0C, 0x230);
    }
    // Dividing by a time and multiplying by its inverse differ after rounding to float about once in 10^9 cases, which random values will not find. These were searched for: on each the two give different floats.
    {
        g_context = "size curve, by the inverse";
        static const uint32_t kLinear[][3] = {
            {0x4131DAD7u, 0x452837A6u, 0x404E0000u}, {0x410D231Du, 0x44BC7D66u, 0x38440000u}, {0x40FE5C0Eu, 0x4513D41Fu, 0x3FEA0000u},
            {0x3FF00210u, 0x4532FED2u, 0x3BC40000u}, {0x416E4087u, 0x45383749u, 0x3C560000u}, {0x414541E9u, 0x4493BF28u, 0x3DC40000u},
        };
        static const uint32_t kQuadratic[][5] = {{0x3F8DFD38u, 0x45139D30u, 0x40D7E00Cu, 0x41BB4860u, 0x3F46EFC8u}};
        const auto real = [](uint32_t bits) { float v; std::memcpy(&v, &bits, 4); return v; };
        for (const auto& k : kLinear) {
            o->m_flags = r->m_flags = AtEmitter::kLinearSize;
            o->m_size[0] = r->m_size[0] = real(k[0]);
            o->m_sizeEnd[0] = r->m_sizeEnd[0] = real(k[1]);
            o->m_sizeEndTime[0] = r->m_sizeEndTime[0] = real(k[2]);
            at<Fn0>(0x00050A90)(o, nullptr);
            r->CurveFromSizeKeys();
            same_words("size curve, by the inverse", o, r, 0x188, 0x198);
        }
        for (const auto& k : kQuadratic) {
            o->m_flags = r->m_flags = AtEmitter::kQuadraticSize;
            o->m_size[0] = r->m_size[0] = real(k[0]);
            o->m_sizeEnd[0] = r->m_sizeEnd[0] = real(k[1]);
            o->m_sizeEndTime[0] = r->m_sizeEndTime[0] = real(k[2]);
            o->m_sizeMiddle[0] = r->m_sizeMiddle[0] = real(k[3]);
            o->m_sizeMiddleTime[0] = r->m_sizeMiddleTime[0] = real(k[4]);
            at<Fn0>(0x00050A90)(o, nullptr);
            r->CurveFromSizeKeys();
            same_words("quadratic size curve, by the inverse", o, r, 0x188, 0x198);
        }
    }
    for (int turn = 0; turn < 3000; turn++) {
        g_context = "streams";
        o->m_maxParticlesPerEvent = r->m_maxParticlesPerEvent = (int)(rnd() % 9) - 1;
        o->m_deltaInitialSpeed = r->m_deltaInitialSpeed = (rnd() & 1) ? 0.0f : 1.5f;
        o->m_coneOuterAngle = r->m_coneOuterAngle = (rnd() & 1) ? -0.0f : 1.5f;
        o->m_deltaParticleLifetime = r->m_deltaParticleLifetime = (rnd() & 1) ? 0.0f : 1.5f;
        o->m_flags = r->m_flags = rnd() & 7;
        at<Fn0>(0x000510E0)(o, nullptr);
        r->CountStreams();
        eq("streams", o->m_streamCount, r->m_streamCount);
    }
    for (int turn = 0; turn < 30000; turn++) {
        g_context = "chance";
        const float v = (turn & 1) ? rnd_real() : (float)((double)(rnd() % 4097) / 4096.0 * 1.5 - 0.25);
        at<FnReal>(0x00050E90)(o, nullptr, v);
        r->SetEventChance(v);
        eq("chance", o->m_eventChance, r->m_eventChance);
    }
    for (int turn = 0; turn < 30000; turn++) {
        g_context = "cone matrix";
        float d[3] = {rnd_real(), rnd_real(), rnd_real()};
        if (turn & 1) {
            // Unit lengths, from the middle of the range.
            for (float& c : d) c = ((float)(int)(rnd() % 2001) - 1000.0f) / 1000.0f;
        }
        for (int i = 0; i < 9; i++) o->m_coneMatrix[i] = r->m_coneMatrix[i] = 77.5f;  // So that a store of 0 shows.
        std::memcpy(&o->m_coneDirection, d, sizeof d);
        std::memcpy(&r->m_coneDirection, d, sizeof d);
        at<Fn0>(0x00051170)(o, nullptr);
        r->MakeConeMatrix();
        same_words("cone", o, r, 0x40, 0x4C);
        same_words("cone", o, r, 0x1F0, 0x214);
        float axis[3] = {rnd_real(), rnd_real(), rnd_real()};
        at<FnPtr>(0x00050EF0)(o, nullptr, axis);
        r->SetConeScaleAxes(*(AtVector3*)axis);
        same_words("cone scale axes", o, r, 0x1D0, 0x1E8);
    }
    { On on(0); at<Fn0>(0x00051300)(o, nullptr); }
    { On on(1); r->~AtEmitter(); }
    g_log[0].n = g_log[1].n = 0;
    _aligned_free(o);
    _aligned_free(r);
}

// ---- the pool ---------------------------------------------------------------------------------------------------------

template <class Q> static void same_base_pool(const AtQueryPool* o, const AtQueryPool* r, uint32_t vftable) {
    eq("base pool's vftable", *(const uint32_t*)o, vftable);
    same_words("base pool", o, r, 4, 8);
    same_words("base pool", o, r, 0x0C, 0x10);
    same_words("base pool", o, r, 0x18, 0x34);
    eq("entries 8", o->m_entries8 != nullptr, r->m_entries8 != nullptr);
    eq("entries 2C", o->m_entries2C != nullptr, r->m_entries2C != nullptr);
    eq("queries", o->m_queries != nullptr, r->m_queries != nullptr);
    if (o->m_queries && r->m_queries && o->m_count == r->m_count) {
        eq("cookie", ((const unsigned*)o->m_queries)[-1], ((const unsigned*)r->m_queries)[-1]);
        for (unsigned i = 0; i < o->m_count; i++) {
            same_query("query", &o->m_queries[i], &r->m_queries[i], sizeof(Q), 0x00276F28, o->m_entries8, o->m_entries2C, r->m_entries8, r->m_entries2C);
        }
    }
}

static void run_pools() {
    typedef AtQueryPool B;
    typedef AtSegmentQueryPool D;
    g_context = "pool";
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        g_fill = fill;
        for (const unsigned count : {0u, 1u, 3u}) {
            for (const unsigned per : {0u, 1u, 2u, 5u}) {
                g_context = "pools " + std::to_string(count) + " " + std::to_string(per);
                InterlockedIncrement(&g_progress);
                // The derived.
                D* o = raw_object<D>(sizeof(D), fill);
                D* r = raw_object<D>(sizeof(D), fill);
                eq("ctor returns this", (uintptr_t)at<FnRet>(0x00047DE0)(o, nullptr), (uintptr_t)o);
                new (r) D;
                same_query_pool(*o, *r);
                o->m_unknown18 = r->m_unknown18 = 0x55;  // Init clears it on its own account.
                { On on(0); at<FnInit>(0x00057950)(o, nullptr, count, per); }
                { On on(1); r->Init(count, per); }
                compare_logs("init");
                same_query_pool(*o, *r);
                // Over itself.
                { On on(0); at<FnInit>(0x00057950)(o, nullptr, count + 1, per); }
                { On on(1); r->Init(count + 1, per); }
                compare_logs("init again");
                same_query_pool(*o, *r);
                // Released, by hand and then by the destructor.
                o->m_unknown20 = r->m_unknown20 = 0x55;
                o->m_sphere.radius = r->m_sphere.radius = 3.5f;
                { On on(0); at<Fn0>(0x000578E0)(o, nullptr); }
                { On on(1); r->Release(); }
                compare_logs("release");
                same_query_pool(*o, *r);
                { On on(0); at<FnInit>(0x00057950)(o, nullptr, count, per); }
                { On on(1); r->Init(count, per); }
                compare_logs("init after release");
                { On on(0); at<Fn0>(0x00047E00)(o, nullptr); }
                { On on(1); r->~D(); }
                compare_logs("destroyed");
                std::free(o);
                std::free(r);
                // The deleting forms.
                for (const unsigned flags : {0u, 1u}) {
                    o = raw_object<D>(sizeof(D), fill);
                    r = raw_object<D>(sizeof(D), fill);
                    at<FnRet>(0x00047DE0)(o, nullptr);
                    new (r) D;
                    { On on(0); at<FnInit>(0x00057950)(o, nullptr, count, per); }
                    { On on(1); r->Init(count, per); }
                    { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x000481D0)(o, nullptr, flags), (uintptr_t)o); }
                    { On on(1); r->~D(); if (flags & 1) ::operator delete(r); }
                    compare_logs("deleted");
                    if (!(flags & 1)) { std::free(o); std::free(r); }
                }
                // The base, on its own.
                B* p = raw_object<B>(sizeof(B), fill);
                B* q = raw_object<B>(sizeof(B), fill);
                eq("base ctor returns this", (uintptr_t)at<FnRet>(0x000578A0)(p, nullptr), (uintptr_t)p);
                new (q) B;
                same_base_pool<AtSegmentHitList>(p, q, 0x002770D8);
                { On on(0); at<FnInit>(0x00057950)(p, nullptr, count, per); }
                { On on(1); q->Init(count, per); }
                compare_logs("base init");
                same_base_pool<AtSegmentHitList>(p, q, 0x002770D8);
                { On on(0); at<Fn0>(0x000574A0)(p, nullptr); at<Fn0>(0x000574A0)(p, nullptr); }
                { On on(1); q->AtQueryPool::Free(); q->AtQueryPool::Free(); }
                compare_logs("base freed");
                same_base_pool<AtSegmentHitList>(p, q, 0x002770D8);
                { On on(0); at<Fn0>(0x00057940)(p, nullptr); }
                { On on(1); q->~B(); }
                compare_logs("base destroyed");
                std::free(p);
                std::free(q);
                for (const unsigned flags : {0u, 1u}) {
                    p = raw_object<B>(sizeof(B), fill);
                    q = raw_object<B>(sizeof(B), fill);
                    at<FnRet>(0x000578A0)(p, nullptr);
                    new (q) B;
                    { On on(0); at<FnInit>(0x00057950)(p, nullptr, count, per); }
                    { On on(1); q->Init(count, per); }
                    { On on(0); eq("base deleting returns this", (uintptr_t)at<FnDel>(0x00057A40)(p, nullptr, flags), (uintptr_t)p); }
                    { On on(1); q->~B(); if (flags & 1) ::operator delete(q); }
                    compare_logs("base deleted");
                    if (!(flags & 1)) { std::free(p); std::free(q); }
                }
            }
        }
    }

    // No room: each raise, with a count of nothing so that no list is walked; then with one, which crashes the same on both sides.
    for (const unsigned count : {0u, 2u}) {
        g_context = "pool, no room";
        g_fail_alloc = true;
        for (int kind = 0; kind < 2; kind++) {
            unsigned char ob[sizeof(D)], rb[sizeof(D)];
            std::memset(ob, 0x33, sizeof ob);
            std::memset(rb, 0x33, sizeof rb);
            if (kind) { at<FnRet>(0x00047DE0)(ob, nullptr); new (rb) D; }
            else { at<FnRet>(0x000578A0)(ob, nullptr); new (rb) B; }
            const bool oc = guarded([&] { On on(0); at<FnInit>(0x00057950)(ob, nullptr, count, 1); });
            g_recording = false;
            g_side = 0;
            const bool rc = guarded([&] { On on(1); ((B*)rb)->Init(count, 1); });
            g_recording = false;
            g_side = 0;
            eq("crash", oc, rc);
            if (oc) g_crashes_seen++;
            compare_logs("no room");
            same_words("no room", ob, rb, 4, 8);
            same_words("no room", ob, rb, 0x0C, 0x10);
            same_words("no room", ob, rb, 0x18, 0x34);
        }
        g_fail_alloc = false;
    }
    // No heap: each raise.
    {
        g_context = "pool, no heap";
        *(void**)(uintptr_t)0x002C7F64 = nullptr;
        AtQueryPool::s_heap = nullptr;
        g_fill = 0xCD;
        for (int kind = 0; kind < 2; kind++) {
            unsigned char ob[sizeof(D)], rb[sizeof(D)];
            std::memset(ob, 0x33, sizeof ob);
            std::memset(rb, 0x33, sizeof rb);
            if (kind) { at<FnRet>(0x00047DE0)(ob, nullptr); new (rb) D; }
            else { at<FnRet>(0x000578A0)(ob, nullptr); new (rb) B; }
            { On on(0); at<FnInit>(0x00057950)(ob, nullptr, 2, 1); }
            { On on(1); ((B*)rb)->Init(2, 1); }
            compare_logs("no heap");
            if (kind) same_query_pool(*(D*)ob, *(D*)rb);
            else same_base_pool<AtSegmentHitList>((B*)ob, (B*)rb, 0x002770D8);
            g_log[0].n = g_log[1].n = 0;
        }
        *(void**)(uintptr_t)0x002C7F64 = &g_heap_token;
        AtQueryPool::s_heap = &g_heap_token;
    }
    // The setters.
    {
        g_context = "setters";
        int h;
        at<FnSetHeap>(0x00057330)(&h);
        AtQueryPool::SetHeap(&h);
        eq("pool's heap", (uintptr_t) * (void**)(uintptr_t)0x002C7F64, (uintptr_t)AtQueryPool::s_heap);
        at<FnSetHeap>(0x00057330)(&g_heap_token);
        AtQueryPool::SetHeap(&g_heap_token);
        at<FnSetHeap>(0x0003F580)(&h);
        AtEmitterDatabase::SetHeap(&h);
        eq("database's heap", (uintptr_t) * (void**)(uintptr_t)0x002C6414, (uintptr_t)AtEmitterDatabase::s_heap);
        at<FnSetHeap>(0x000552A0)(&h);
        AtSegmentHitList::SetHeap(&h);
        eq("query's heap", (uintptr_t) * (void**)(uintptr_t)0x002C7F58, (uintptr_t)AtSegmentHitList::s_heap);
        at<FnSetHeap>(0x000552A0)(&g_heap_token);
        AtSegmentHitList::SetHeap(&g_heap_token);
        at<FnSetHeap>(0x0003F580)(&g_heap_token);
        AtEmitterDatabase::SetHeap(&g_heap_token);
    }
}

// One query alone: its storage owned, which is the branch no pool reaches.
static void run_queries() {
    g_fill = 0xCD;
    for (int kind = 0; kind < 2; kind++) {
        const size_t size = kind ? sizeof(AtSegmentQuery) : sizeof(AtSegmentHitList);
        const uint32_t vftable = kind ? 0x002770F4 : 0x00276F28;
        const uint32_t ctor = kind ? 0x00057B90 : 0x000555A0;
        const uint32_t clear = kind ? 0x00057BB0 : 0x000555B0;
        const uint32_t dtor = kind ? 0x00057BF0 : 0x000555F0;
        const uint32_t deleting = kind ? 0x00047FC0 : 0x00057440;
        const uint32_t reserve = kind ? 0x00057AA0 : 0x000553A0;
        const auto make_o = [&]() { void* p = std::malloc(size); std::memset(p, 0x33, size); eq("query's ctor returns this", (uintptr_t)at<FnRet>(ctor)(p, nullptr), (uintptr_t)p); return (AtSegmentHitList*)p; };
        const auto make_r = [&]() -> AtSegmentHitList* { void* p = std::malloc(size); std::memset(p, 0x33, size); if (kind) return new (p) AtSegmentQuery; return new (p) AtSegmentHitList; };
        g_context = kind ? "segment query" : "query";
        AtSegmentHitList* o = make_o();
        AtSegmentHitList* r = make_r();
        // No storage yet.
        same_query("made", o, r, size, vftable, nullptr, nullptr, nullptr, nullptr);
        // Reset, of everything but its word at +0x7C.
        std::memset((char*)o + 4, 0x77, size - 4);
        std::memset((char*)r + 4, 0x77, size - 4);
        at<Fn0>(0x00055490)(o, nullptr);
        r->Reset();
        same_query("reset", o, r, size, vftable, nullptr, nullptr, nullptr, nullptr);
        // Owned storage is freed; each block held or not.
        for (int held = 0; held < 8; held++) {
            const auto give = [&](AtSegmentHitList* q) {
                q->m_flags = 0;
                q->m_capacity = 1;
                q->m_keys = (held & 1) ? std::malloc(4) : nullptr;
                q->m_hits = (held & 2) ? std::malloc(4) : nullptr;
                if (kind) ((AtSegmentQuery*)q)->m_records = (AtSegmentHit*)((held & 4) ? std::malloc(4) : nullptr);
                if (kind) ((AtSegmentQuery*)q)->m_unknown94 = 0x66;
            };
            give(o);
            give(r);
            { On on(0); at<Fn0>(clear)(o, nullptr); }
            { On on(1); r->Clear(); }
            compare_logs("cleared");
            same_query("cleared", o, r, size, vftable, nullptr, nullptr, nullptr, nullptr);
            if (kind) eq("third cleared", ((AtSegmentQuery*)o)->m_records != nullptr, ((AtSegmentQuery*)r)->m_records != nullptr);
            // Not owned: left.
            char a[8], b[8], c[8];
            const auto lend = [&](AtSegmentHitList* q) {
                q->m_flags = AtSegmentHitList::kNotOwned;
                q->m_capacity = 1;
                q->m_keys = a;
                q->m_hits = b;
                if (kind) ((AtSegmentQuery*)q)->m_records = (AtSegmentHit*)c;
            };
            lend(o);
            lend(r);
            { On on(0); at<Fn0>(clear)(o, nullptr); }
            { On on(1); r->Clear(); }
            compare_logs("cleared, not owned");
            same_query("cleared, not owned", o, r, size, vftable, nullptr, nullptr, nullptr, nullptr);
            if (kind) eq("third cleared, not owned", ((AtSegmentQuery*)o)->m_records != nullptr, ((AtSegmentQuery*)r)->m_records != nullptr);
            // Attach: nothing to clear, then something, by the slot, with owned storage.
            char d[8], e[8];
            { On on(0); at<FnAttach3>(0x00055360)(o, nullptr, 4, d, e); }
            { On on(1); r->AtSegmentHitList::SetStorage(4, d, e); }
            compare_logs("attached");
            same_query("attached", o, r, size, vftable, d, e, d, e);
            give(o);
            give(r);
            { On on(0); at<FnAttach3>(0x00055360)(o, nullptr, 6, d, e); }
            { On on(1); r->AtSegmentHitList::SetStorage(6, d, e); }
            compare_logs("attached over");
            same_query("attached over", o, r, size, vftable, d, e, d, e);
            if (kind) {
                { On on(0); at<FnAttach4>(0x00057A70)(o, nullptr, 2, d, e, c); }
                { On on(1); ((AtSegmentQuery*)r)->SetStorage(2, d, e, c); }
                compare_logs("attached with a third");
                same_query("attached with a third", o, r, size, vftable, d, e, d, e);
            }
            { On on(0); at<Fn0>(clear)(o, nullptr); }
            { On on(1); r->Clear(); }
            g_log[0].n = g_log[1].n = 0;
        }
        // Its own storage: fresh, over what it has, with no heap, and with no room.
        for (const unsigned count : {0u, 3u, 5u}) {
            g_context = (kind ? "segment query reserve " : "query reserve ") + std::to_string(count);
            { On on(0); at<FnCount>(reserve)(o, nullptr, count); }
            { On on(1); r->Allocate(count); }
            compare_logs("reserved");
            eq("first block cleared", std::memcmp(o->m_keys, r->m_keys, count * 8), 0);
            eq("second block as the heap left it", std::memcmp(o->m_hits, r->m_hits, count * 0x2C), 0);
            eq("count", o->m_capacity, r->m_capacity);
            eq("flags", o->m_flags, r->m_flags);
            if (kind) eq("third", ((AtSegmentQuery*)o)->m_records != nullptr, ((AtSegmentQuery*)r)->m_records != nullptr);
            if (kind) eq("third as the heap left it", std::memcmp(((AtSegmentQuery*)o)->m_records, ((AtSegmentQuery*)r)->m_records, count * 0x10), 0);
        }
        { On on(0); at<Fn0>(clear)(o, nullptr); }
        { On on(1); r->Clear(); }
        g_log[0].n = g_log[1].n = 0;
        g_context = kind ? "segment query reserve, no heap" : "query reserve, no heap";
        *(void**)(uintptr_t)0x002C7F58 = nullptr;
        AtSegmentHitList::s_heap = nullptr;
        { On on(0); at<FnCount>(reserve)(o, nullptr, 2); }
        { On on(1); r->Allocate(2); }
        compare_logs("no heap");
        *(void**)(uintptr_t)0x002C7F58 = &g_heap_token;
        AtSegmentHitList::s_heap = &g_heap_token;
        { On on(0); at<Fn0>(clear)(o, nullptr); }
        { On on(1); r->Clear(); }
        g_log[0].n = g_log[1].n = 0;
        g_context = kind ? "segment query reserve, no room" : "query reserve, no room";
        g_fail_alloc = true;
        {
            // With a count of nothing, a clear of nothing, so the null is not written to.
            { On on(0); at<FnCount>(reserve)(o, nullptr, 0); }
            { On on(1); r->Allocate(0); }
            compare_logs("no room");
        }
        g_fail_alloc = false;
        same_query("no room", o, r, size, vftable, nullptr, nullptr, nullptr, nullptr);
        // Destroyed, holding and not.
        for (int held = 0; held < 2; held++) {
            const auto give = [&](AtSegmentHitList* q) {
                q->m_flags = held ? 0 : (unsigned)AtSegmentHitList::kNotOwned;
                q->m_keys = std::malloc(4);
                q->m_hits = std::malloc(4);
                if (kind) ((AtSegmentQuery*)q)->m_records = (AtSegmentHit*)std::malloc(4);
            };
            give(o);
            give(r);
            { On on(0); at<Fn0>(dtor)(o, nullptr); }
            { On on(1); if (kind) ((AtSegmentQuery*)r)->~AtSegmentQuery(); else r->~AtSegmentHitList(); }
            compare_logs("query destroyed");
            same_query("query destroyed", o, r, sizeof(AtSegmentHitList), 0x00276F28, nullptr, nullptr, nullptr, nullptr);
            std::free(o);
            std::free(r);
            o = make_o();
            r = make_r();
        }
        // The scalar forms of the deleting destructor. The vector form is reached through a pool.
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(deleting)(o, nullptr, 0), (uintptr_t)o); }
        { On on(1); if (kind) ((AtSegmentQuery*)r)->~AtSegmentQuery(); else r->~AtSegmentHitList(); }
        compare_logs("scalar, no free");
        std::free(o);
        std::free(r);
        o = make_o();
        r = make_r();
        { On on(0); at<FnDel>(deleting)(o, nullptr, 1); }
        { On on(1); if (kind) delete (AtSegmentQuery*)r; else delete r; }
        compare_logs("scalar, freed");
    }
}

static void run_failures() {
    const std::string text = one("LineSegIntersectTesting\nMaxParticles 2");
    // No heap for the database, and no room for it. The list and pool have their own.
    for (int kind = 0; kind < 2; kind++) {
        g_context = kind ? "database, no room" : "database, no heap";
        InterlockedIncrement(&g_progress);
        std::set<std::string> names, spaces;
        gather(text, names, spaces);
        spaces.insert("Space");
        Env env(names, spaces);
        g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)text.data();
        g_fs->files[0].size = g_fs->files[1].size = text.size();
        g_fill = 0xCD;
        AtEmitterDatabase* o = make_db(0, 0xCD);
        AtEmitterDatabase* r = make_db(1, 0xCD);
        if (kind == 0) {
            *(void**)(uintptr_t)0x002C6414 = nullptr;
            AtEmitterDatabase::s_heap = nullptr;
            load_both(o, r, "no heap");
            *(void**)(uintptr_t)0x002C6414 = &g_heap_token;
            AtEmitterDatabase::s_heap = &g_heap_token;
        } else {
            // The file's own first requests must have room: the database's is the block of this size.
            Direct a(text), b(text);
            g_fail_alloc = true;
            const bool oc = guarded([&] { On on(0); at<FnReader>(0x0003F5B0)(o, nullptr, g_scene[0], &a.tokenizer); });
            g_recording = false;
            g_side = 0;
            const bool rc = guarded([&] { On on(1); r->Read(g_scene[1], &b.tokenizer); });
            g_recording = false;
            g_side = 0;
            g_fail_alloc = false;
            eq("crash", oc, rc);
            if (oc) g_crashes_seen++;
            compare_logs("no room");
            eq("count", o->m_count, r->m_count);
        }
    }
    // File errors, reported on both sides. A missing file leaves no tokenizer to read from.
    {
        g_context = "database, no file";
        Env env({"M"}, {"Space"});
        AtEmitterDatabase* o = make_db(0, 0xCD);
        AtEmitterDatabase* r = make_db(1, 0xCD);
        g_fs->open_status = 7;
        load_both(o, r, "no file");
        g_fs->open_status = 0;
    }
    // The add method, by what it adds to.
    for (const char* ns : {(const char*)nullptr, "", "Sp"}) {
        g_context = "add";
        Env env({"M"}, {"Space"});
        GmNameMap x, y;
        at<FnAdd>(0x00040030)(g_scene[0], nullptr, &x, ns);
        g_scene[1]->AddEmitters(&y, ns);
        eq("emitter group", x.m_group == &g_scene[0]->m_emitters, y.m_group == &g_scene[1]->m_emitters);
        eqs("emitter group's name", group_name(x.m_group).c_str(), group_name(y.m_group).c_str());
        eq("emitter first", g_scene[0]->m_emitters.m_first == &x, g_scene[1]->m_emitters.m_first == &y);
        GmNameSpaceDb::Remove(&x);
        GmNameSpaceDb::Remove(&y);
    }
    // The direct reader's words: read what the file code has not reached with text that does not stop.
    {
        g_context = "database direct";
        Env env({"M"}, {"Space"});
        static const char* const kTexts[] = {"", "Emitter", "Emitter[1]", "Emitter[1]\n{", "Emitter[1]\n{\nEmitter", "Emitter[1]\n{\nEmitter \"A\"", "Emitter[1]\n{\nEmitter \"A\"\n{", "NameSpace", "NameSpace \"A\""};
        for (const char* t : kTexts) {
            g_context = std::string("database direct [") + t + "]";
            InterlockedIncrement(&g_progress);
            AtEmitterDatabase* o = make_db(0, 0xCD);
            AtEmitterDatabase* r = make_db(1, 0xCD);
            Direct a(t), b(t);
            const bool oc = guarded([&] { On on(0); at<FnReader>(0x0003F5B0)(o, nullptr, g_scene[0], &a.tokenizer); });
            g_recording = false;
            g_side = 0;
            const bool rc = guarded([&] { On on(1); r->Read(g_scene[1], &b.tokenizer); });
            g_recording = false;
            g_side = 0;
            eq("crash", oc, rc);
            if (oc) g_crashes_seen++;
            compare_logs("read");
            eq("left at", a.stream.m_cur - a.stream.m_base, b.stream.m_cur - b.stream.m_base);
            eq("count", o->m_count, r->m_count);
        }
    }
}

// ---- running ----------------------------------------------------------------------------------------------------------

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

// A body that reaches the end of its text loops on its report for ever, on either side: a mutant can bring that about, and the original's loop has no way out.
static DWORD WINAPI watchdog(void*) {
    LONG last = -1;
    for (;;) {
        Sleep(120000);
        const LONG now = g_progress;
        if (now == last) {
            std::printf("HUNG during [%.300s]\n", g_context.c_str());
            ExitProcess(4);
        }
        last = now;
    }
}

static bool read_case(const std::string& path, std::string& data) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    data.resize((size_t)std::ftell(f));
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    return true;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    CreateThread(nullptr, 0, watchdog, nullptr, 0, nullptr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_ase <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C63E0u, 0x002C6414u, 0x002C7F64u, 0x002C7F58u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtMatAssignment::s_heap = &g_heap_token;
    AtEmitterDatabase::s_heap = &g_heap_token;
    AtQueryPool::s_heap = &g_heap_token;
    AtSegmentHitList::s_heap = &g_heap_token;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    g_context = "keywords";
    for (unsigned i = 0; i < AtEmitterDatabase::kKeywordCount; i++) {
        const AtIoKeyword& k = ((const AtIoKeyword*)(uintptr_t)0x002B1F20)[i];
        eq("id", k.m_id, AtEmitterDatabase::s_keywords[i].m_id);
        eqs("word", k.m_name, AtEmitterDatabase::s_keywords[i].m_name);
    }

    run_written();
    run_emitter_direct();
    run_vectors();
    run_curves();
    run_pools();
    run_queries();
    run_failures();
    InterlockedIncrement(&g_progress);

    const unsigned written_emitters = g_emitters_seen;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    unsigned files = 0;
    g_emitters_seen = 0;
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ASE\t")) continue;
        *std::strchr(line, '\t') = 0;
        std::string data;
        if (!read_case(dir + "\\" + line, data)) { std::printf("cannot read %s\n", line); return 2; }
        run_text(data, std::string("ASE ") + line, 0xCD);
        files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("words shown to change an emitter: %u; loads and reads that crashed the same on both sides: %u; query pools compared: %u\n", g_words_changed, g_crashes_seen, g_query_pools_seen);
    std::printf("%s: files ASE %u, %u emitter comparisons in files, %u in written text; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_emitters_seen, written_emitters, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
