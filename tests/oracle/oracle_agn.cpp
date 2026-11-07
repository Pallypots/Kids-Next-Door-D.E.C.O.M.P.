// Differential oracle for the sibling loaders of .AGN, .AGA, .AGI, .AGF, .AGL and .ASL (Windows x86, local only). See docs/verification.md.
//
//   oracle_agn <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every file of the six extensions in index.txt, loaded through the file code on both sides; and text written here. What each side's material search finds is a real map, one owner to a
// side, filled with every string of the text except names beginning with X.
//
// What stands in for the original first: images, fonts and lights are made by the graphics layer, which is not recovered, so both sides are given the same harness items. An image's and a font's
// body are read by the harness item, which only walks the tokens and logs them; a light's body is the recovered AtLight::Read against the original's 0x000368C0. So on .AGI and .AGF this compares the
// databases, their material lists and the order of calls, not what an image or a font makes of its words.
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

#include "at/AtImageDatabase.h"
#include "at/AtIoTokenizer.h"
#include "at/AtLensFlare.h"
#include "at/AtLight.h"
#include "at/AtMatAnimation.h"
#include "at/AtMatAssignment.h"
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

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

static AtMaterialOwner* g_own[2];
static AtNameTableOwner* g_scene[2];

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void*(__fastcall* FnFind)(void*, void*, const char*, const char*);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);

// ---- the items ----------------------------------------------------------------------------------------------------------

enum { kPool = 512, kWordImageDemand = 0x41, kWordFontDemand = 0x48 };

// Walks a body's tokens. Text that ends in a body stops here, unlike the items in the game.
static bool g_fail_in_item;

template <class Item> static void walk_body(Item* item, int side, int demand_word, const char* name, AtIoTokenizer* tokenizer, bool database_right, bool found_by_name) {
    Log& log = g_log[side];
    log.add('f', found_by_name);
    if (g_fail_in_item) g_fail_alloc = true;
    log.add('I', item->index);
    log.add('i', database_right);
    // Past its terminator a short name is whatever was on the stack.
    for (int i = 0; i < 8 && name[i] != 0; i++) log.add('n', (unsigned char)name[i]);
    log.add('t', g_fs->m_path.m_top);
    if (const char* current = g_fs->m_path.Current()) record_message(current, side);
    item->m_flags = 0;
    int token = tokenizer->Next();
    log.add('b', token);
    if (token != AtIoTokenizer::kLeftCurly) return;
    for (int depth = 1; depth > 0;) {
        token = tokenizer->Next();
        log.add('b', token);
        if (token == AtIoTokenizer::kEnd) return;
        if (token == AtIoTokenizer::kLeftCurly) depth++;
        if (token == AtIoTokenizer::kRightCurly) depth--;
        if (token == demand_word) item->m_flags |= 8;
    }
}

static void* g_db_of[2];

class TestImage : public AtImageItem {
public:
    int side, index;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot9() override {}
    void Read(AtMaterialOwner* owner, AtImageDatabase* database, const char* name, AtIoTokenizer* tokenizer) override {
        g_log[side].add('w', owner == g_own[side]);
        walk_body(this, side, kWordImageDemand, name, tokenizer, database == g_db_of[side], database->Find(name) == this);
    }
    void OnLoaded(AtMaterialOwner* owner) override {
        g_log[side].add('L', index);
        g_log[side].add('l', owner == g_own[side]);
    }
};
class TestFont : public AtFontItem {
public:
    int side, index;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot8() override {}
    void Read(AtMaterialOwner* owner, AtFontDatabase* database, const char* name, AtIoTokenizer* tokenizer) override {
        g_log[side].add('w', owner == g_own[side]);
        walk_body(this, side, kWordFontDemand, name, tokenizer, database == g_db_of[side], database->Find(name) == this);
    }
    void OnLoaded(AtMaterialOwner* owner) override {
        g_log[side].add('L', index);
        g_log[side].add('l', owner == g_own[side]);
    }
};
class TestLight : public AtLight {
public:
    int side, index;
    void Slot0() override {}
    void Reset() override { g_log[side].add('x', index); }
};

static unsigned char g_images[2][kPool][sizeof(TestImage)];
static unsigned char g_fonts[2][kPool][sizeof(TestFont)];
static unsigned char g_lights[2][kPool][sizeof(TestLight)];
static bool g_keep_lights;

template <class Item, size_t N> static void make_items(unsigned char (&pool)[kPool][N], int side, unsigned count) {
    const bool was = g_recording;
    g_recording = false;
    for (unsigned i = 0; i < std::min(count, (unsigned)kPool); i++) {
        std::memset(pool[i], 0x5A, N);
        Item* item = new (pool[i]) Item;
        item->side = side;
        item->index = (int)i;
    }
    g_recording = was;
}
template <size_t N> static void* item_at(unsigned char (&pool)[kPool][N], int side, unsigned i) {
    g_log[side].add('A', i);
    return pool[std::min(i, (unsigned)kPool - 1)];
}

// ---- the abstract databases on each side ------------------------------------------------------------------------------

class TestImageDb : public AtImageDatabase {
public:
    void MakeItems() override { g_log[1].add('M', m_count); make_items<TestImage>(g_images[1], 1, m_count); }
    void OnReady(AtMaterialOwner* owner) override { g_log[1].add('Y', owner == g_own[1]); g_log[1].add('y', m_group != nullptr); }
    AtImageItem* ImageAt(unsigned i) override { return (AtImageItem*)(TestImage*)item_at(g_images[1], 1, i); }
};
static void __fastcall oi_make(AtImageDatabase* s, void*) { g_log[0].add('M', s->m_count); make_items<TestImage>(g_images[0], 0, s->m_count); }
static void __fastcall oi_ready(AtImageDatabase* s, void*, AtMaterialOwner* owner) { g_log[0].add('Y', owner == g_own[0]); g_log[0].add('y', s->m_group != nullptr); }
static void* __fastcall oi_at(void*, void*, unsigned i) { return (AtImageItem*)(TestImage*)item_at(g_images[0], 0, i); }
static const void* g_image_table[6] = {(const void*)(uintptr_t)0x00029FF0, (const void*)(uintptr_t)0x00029F00, (const void*)oi_make, (const void*)oi_ready, (const void*)oi_at, (const void*)(uintptr_t)0x000AB720};

class TestFontDb : public AtFontDatabase {
public:
    void OnReady(AtMaterialOwner* owner) override { g_log[1].add('Y', owner == g_own[1]); g_log[1].add('y', m_group != nullptr); }
    void MakeFonts2D() override { g_log[1].add('M', m_count); g_log[1].add('y', m_group != nullptr); make_items<TestFont>(g_fonts[1], 1, m_count); }
    AtFontItem* Font2DAt(unsigned i) override { return (AtFontItem*)(TestFont*)item_at(g_fonts[1], 1, i); }
};
static void __fastcall of_ready(AtFontDatabase* s, void*, AtMaterialOwner* owner) { g_log[0].add('Y', owner == g_own[0]); g_log[0].add('y', s->m_group != nullptr); }
static void __fastcall of_make(AtFontDatabase* s, void*) { g_log[0].add('M', s->m_count); g_log[0].add('y', s->m_group != nullptr); make_items<TestFont>(g_fonts[0], 0, s->m_count); }
static void* __fastcall of_at(void*, void*, unsigned i) { return (AtFontItem*)(TestFont*)item_at(g_fonts[0], 0, i); }
static const void* g_font_table[8] = {(const void*)(uintptr_t)0x0002A500, (const void*)(uintptr_t)0x0002A410, (const void*)of_ready, (const void*)of_make, (const void*)(uintptr_t)0x000AB720, (const void*)(uintptr_t)0x000447F0, (const void*)of_at, (const void*)(uintptr_t)0x000AB720};

class TestLightDb : public AtLightDatabase {
public:
    void MakeLights(AtMaterialOwner* owner) override;
    AtLight* LightAt(unsigned i) override { return (AtLight*)(TestLight*)item_at(g_lights[1], 1, i); }
};
static void do_make_lights(int side, AtLightDatabase* s, AtMaterialOwner* owner) {
    g_log[side].add('M', s->m_count);
    g_log[side].add('m', owner == g_own[side]);
    g_log[side].add('y', s->m_group != nullptr);
    if (g_keep_lights) return;
    make_items<TestLight>(g_lights[side], side, s->m_count);
    for (unsigned i = 0; i < std::min(s->m_count, (unsigned)kPool); i++) ((TestLight*)g_lights[side][i])->m_owner = nullptr;
}
void TestLightDb::MakeLights(AtMaterialOwner* owner) { do_make_lights(1, this, owner); }
static void __fastcall ol_make(AtLightDatabase* s, void*, AtMaterialOwner* owner) { do_make_lights(0, s, owner); }
static void* __fastcall ol_at(void*, void*, unsigned i) { return (AtLight*)(TestLight*)item_at(g_lights[0], 0, i); }
static const void* g_light_table[4] = {(const void*)(uintptr_t)0x0002AD30, (const void*)(uintptr_t)0x0002ACB0, (const void*)ol_make, (const void*)ol_at};

// ---- comparing --------------------------------------------------------------------------------------------------------

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

// A slot's value as an offset from where that side keeps what it names.
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

static void same_table(const GmNameMap* o, const GmNameMap* r, const GmNameMapGroup& og, const GmNameMapGroup& rg, const GmNameSpaceDb& os, const GmNameSpaceDb& rs) {
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &og, r->m_group == &rg);
    eq("first in the plain group", og.m_first == o, rg.m_first == r);
    eq("spaces in use", os.m_free != nullptr ? os.m_free - os.m_pool : -1, rs.m_free != nullptr ? rs.m_free - rs.m_pool : -1);
}

static void same_list(const char* what, const AtMatAssignment& o, const AtMatAssignment& r) {
    eq(what, o.m_count, r.m_count);
    eq(what, o.m_items != nullptr, r.m_items != nullptr);
    if (o.m_items && r.m_items && o.m_count == r.m_count) eq(what, std::memcmp(o.m_items, r.m_items, o.m_count * sizeof(void*)), 0);
}

static void same_float(const char* what, float o, float r) {
    if (o != o && r != r) { g_checks++; return; }  // x87 and the host may differ in a NaN's sign.
    uint32_t a, b;
    std::memcpy(&a, &o, 4);
    std::memcpy(&b, &r, 4);
    eq(what, a, b);
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
            g_scene[side] = new AtNameTableOwner;
            g_scene[side]->m_device = (AtSceneDevice*)g_own[side];
            AtMaterialOwner& w = *g_own[side];
            const unsigned pool = (unsigned)spaces.size() + 8;
            w.m_materialSpaces.Init(pool);
            w.m_lightSpaces.Init(pool);
            w.m_matAssignmentSpaces.Init(pool);
            w.m_matAnimationSpaces.Init(pool);
            w.m_fontSpaces.Init(pool);
            w.m_imageSpaces.Init(pool);
            g_scene[side]->m_lensFlareSpaces.Init(pool);
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

// ---- one format, as its object on each side ----------------------------------------------------------------------------

static unsigned g_items_seen;

template <class Db> static Db* raw_object(size_t size, unsigned char fill) {
    unsigned char* p = (unsigned char*)std::malloc(size);
    std::memset(p, fill, size);
    return (Db*)p;
}

struct Agn {
    enum { kInnerTable = 0 };
    typedef AtMatAssignmentDatabase Db;
    static constexpr const char* kExt = ".AGN\t";
    static Db* make(int side, unsigned char fill) {
        Db* d = raw_object<Db>(sizeof(Db), fill);
        if (side == 0) at<Fn0>(0x00027D30)(d, nullptr);
        else new (d) Db;
        return d;
    }
    static void load(int side, Db* d) {
        if (side == 0) at<FnLoad>(0x00027D50)(d, nullptr, g_own[0], "MODEL\\THING", 77);
        else d->Load(g_own[1], "MODEL\\THING", 77);
    }
    static void clear(int side, Db* d) { if (side == 0) at<Fn0>(0x00027E10)(d, nullptr); else d->Clear(); }
    static void del(int side, Db* d, unsigned flags) {
        if (side == 0) eq("returns this", (uintptr_t)at<FnDel>(0x0001A070)(d, nullptr, flags), (uintptr_t)d);
        else { d->~Db(); if (flags & 1) ::operator delete(d); }
    }
    static void same(const Db* o, const Db* r) {
        eq("count", o->m_count, r->m_count);
        eq("lists", o->m_lists != nullptr, r->m_lists != nullptr);
        same_map(o, r, o->m_lists, r->m_lists);
        same_table(o, r, g_own[0]->m_matAssignments, g_own[1]->m_matAssignments, g_own[0]->m_matAssignmentSpaces, g_own[1]->m_matAssignmentSpaces);
        if (o->m_lists && r->m_lists && o->m_count == r->m_count) {
            eq("cookie", ((const unsigned*)o->m_lists)[-1], ((const unsigned*)r->m_lists)[-1]);
            for (unsigned i = 0; i < o->m_count; i++) same_list("list", o->m_lists[i], r->m_lists[i]);
            g_items_seen += o->m_count;
        }
    }
    static void more(Db*, Db*) {}
};

static void same_set(const AtGrFrameNumberSet& o, const AtGrFrameNumberSet& r) {
    eq("set's flags", o.m_flags, r.m_flags);
    eq("set's count", o.m_count, r.m_count);
    eq("set's name", std::strncmp(o.m_name, r.m_name, 8), 0);
    eq("set's frames", o.m_frames != nullptr, r.m_frames != nullptr);
    if (o.m_frames && r.m_frames && o.m_count == r.m_count && (o.m_flags & 2)) eq("set's frames", std::memcmp(o.m_frames, r.m_frames, o.m_count * 2), 0);
}

typedef void(__fastcall* FnInterp)(void*, void*, unsigned, unsigned, float, int, int*, int*, float*);
typedef int(__fastcall* FnFindKey)(void*, void*, unsigned, unsigned, float, int);
typedef void(__fastcall* FnApply)(void*, void*, void*, unsigned, float);

static void same_run(AtGrFrameNumberSet* o, AtGrFrameNumberSet* r, unsigned first, unsigned count, int length, float time) {
    int a[2] = {0x55555555, 0x55555555}, b[2] = {0x55555555, 0x55555555};
    float ta = 77.0f, tb = 77.0f;
    at<FnInterp>(0x00034D20)(o, nullptr, first, count, time, length, &a[0], &a[1], &ta);
    r->Interpolate(first, count, time, length, &b[0], &b[1], &tb);
    eq("interpolate from", a[0], b[0]);
    eq("interpolate to", a[1], b[1]);
    same_float("interpolate along", ta, tb);
    eq("find", at<FnFindKey>(0x00034E30)(o, nullptr, first, count, time, length), r->Find(first, count, time, length));
}

// Each time from before the first key to past the last, on a run.
static void same_times(AtGrFrameNumberSet* o, AtGrFrameNumberSet* r, unsigned first, unsigned count, int length) {
    if (!o->m_frames || !r->m_frames || count == 0 || first + count > o->m_count) return;
    int top = 0;
    for (unsigned i = first; i < first + count; i++) top = std::max(top, (int)o->m_frames[i]);
    top = std::min(top, 2000);
    const int step = std::max(1, top / 40);
    for (int t = -2; t <= top + 3; t += step) {
        for (float part : {0.0f, 0.25f, 0.999f}) same_run(o, r, first, count, length, (float)t + part);
    }
}

struct Aga {
    enum { kInnerTable = 0x14 };
    typedef AtMatAnimationDatabase Db;
    static constexpr const char* kExt = ".AGA\t";
    static Db* make(int side, unsigned char fill) {
        Db* d = raw_object<Db>(sizeof(Db), fill);
        if (side == 0) at<Fn0>(0x00027F30)(d, nullptr);
        else new (d) Db;
        return d;
    }
    static void load(int side, Db* d) {
        if (side == 0) at<FnLoad>(0x00028340)(d, nullptr, g_own[0], "MODEL\\THING", 77);
        else d->Load(g_own[1], "MODEL\\THING", 77);
    }
    static void clear(int side, Db* d) { if (side == 0) at<Fn0>(0x00028280)(d, nullptr); else d->Clear(); }
    static void del(int side, Db* d, unsigned flags) {
        if (side == 0) eq("returns this", (uintptr_t)at<FnDel>(0x0001A190)(d, nullptr, flags), (uintptr_t)d);
        else { d->~Db(); if (flags & 1) ::operator delete(d); }
    }
    static void same(const Db* o, const Db* r) {
        same_set(o->m_frameNumbers, r->m_frameNumbers);
        eq("key count", o->m_keyCount, r->m_keyCount);
        eq("keys", o->m_keys != nullptr, r->m_keys != nullptr);
        if (o->m_keys && r->m_keys && o->m_keyCount == r->m_keyCount) eq("keys", std::memcmp(o->m_keys, r->m_keys, o->m_keyCount * sizeof(void*)), 0);
        eq("animation count", o->m_animationCount, r->m_animationCount);
        eq("animations", o->m_animations != nullptr, r->m_animations != nullptr);
        same_map(o, r, o->m_animations, r->m_animations);
        same_table(o, r, g_own[0]->m_matAnimations, g_own[1]->m_matAnimations, g_own[0]->m_matAnimationSpaces, g_own[1]->m_matAnimationSpaces);
        if (o->m_animations && r->m_animations && o->m_animationCount == r->m_animationCount) {
            eq("cookie", ((const unsigned*)o->m_animations)[-1], ((const unsigned*)r->m_animations)[-1]);
            for (unsigned i = 0; i < o->m_animationCount; i++) {
                const AtMatAnimation& x = o->m_animations[i];
                const AtMatAnimation& y = r->m_animations[i];
                eq("run's database", x.m_database == o, y.m_database == r);
                same_float("run's rate", x.m_rate, y.m_rate);
                eq("run's length", x.m_length, y.m_length);
                eq("run's loop", x.m_loopPeriod, y.m_loopPeriod);
                eq("run's first frame", x.m_firstFrame, y.m_firstFrame);
                eq("run's first key", x.m_firstKey, y.m_firstKey);
                eq("run's key count", x.m_keyCount, y.m_keyCount);
            }
            g_items_seen += o->m_animationCount;
        }
    }
    static void more(Db* o, Db* r) {
        if (!o->m_animations || !r->m_animations) return;
        g_recording = false;
        for (unsigned i = 0; i < o->m_animationCount; i++) {
            AtMatAnimation& x = o->m_animations[i];
            AtMatAnimation& y = r->m_animations[i];
            if (x.m_keyCount == 0 || x.m_firstFrame + x.m_keyCount > o->m_frameNumbers.m_count || x.m_firstKey + x.m_keyCount > o->m_keyCount || !o->m_keys) continue;
            same_times(&o->m_frameNumbers, &r->m_frameNumbers, x.m_firstFrame, x.m_keyCount, (int)x.m_loopPeriod);
            void* la[3] = {nullptr, (void*)7, nullptr};
            void* lb[3] = {nullptr, (void*)7, nullptr};
            AtMatAssignment ta, tb;
            ta.m_count = tb.m_count = 3;
            ta.m_items = la;
            tb.m_items = lb;
            for (int t = -1; t < (int)x.m_length + 2 && t < 200; t++) {
                at<FnApply>(0x00034AA0)(&x, nullptr, &ta, 1, (float)t + 0.5f);
                y.Apply(&tb, 1, (float)t + 0.5f);
                eq("applied", (uintptr_t)la[1], (uintptr_t)lb[1]);
            }
            eq("applied only there", (uintptr_t)la[0] + (uintptr_t)la[2], (uintptr_t)lb[0] + (uintptr_t)lb[2]);
            ta.m_items = tb.m_items = nullptr;
        }
    }
};

template <class Db> static void same_sourced(const Db* o, const Db* r) {
    same_list("materials", o->m_materials, r->m_materials);
    eq("count", o->m_count, r->m_count);
    eq("file system", o->m_fileSystem == g_fs, r->m_fileSystem == g_fs);
    eq("file system set", o->m_fileSystem != nullptr, r->m_fileSystem != nullptr);
    eq("directory", o->m_directory != nullptr, r->m_directory != nullptr);
    if (o->m_directory && r->m_directory) eqs("directory", o->m_directory, r->m_directory);
    g_items_seen += o->m_count;
}

// Over a side's kept directory: what the file system's path then is.
template <class Db> static void same_directory_calls(Db* o, Db* r, uint32_t push, uint32_t pop, void (Db::*rpush)(), void (Db::*rpop)()) {
    const int before = g_fs->m_path.m_top;
    at<Fn0>(push)(o, nullptr);
    const int t0 = g_fs->m_path.m_top;
    const std::string c0 = g_fs->m_path.Current() ? g_fs->m_path.Current() : "";
    at<Fn0>(pop)(o, nullptr);
    eq("popped", g_fs->m_path.m_top, before);
    (r->*rpush)();
    eq("pushed", t0, g_fs->m_path.m_top);
    eqs("directory pushed", c0.c_str(), g_fs->m_path.Current() ? g_fs->m_path.Current() : "");
    (r->*rpop)();
    eq("popped", g_fs->m_path.m_top, before);
}

struct Agi {
    enum { kInnerTable = 0 };
    typedef AtImageDatabase Db;
    static constexpr const char* kExt = ".AGI\t";
    static Db* make(int side, unsigned char fill) {
        Db* d;
        if (side == 0) {
            d = raw_object<Db>(sizeof(Db), fill);
            at<Fn0>(0x00029BA0)(d, nullptr);
            *(const void***)d = g_image_table;
        } else {
            d = raw_object<Db>(sizeof(TestImageDb), fill);
            new (d) TestImageDb;
        }
        g_db_of[side] = d;
        return d;
    }
    static void load(int side, Db* d) {
        if (side == 0) at<FnLoad>(0x00029DC0)(d, nullptr, g_own[0], "MODEL\\THING", 77);
        else d->Load(g_own[1], "MODEL\\THING", 77);
    }
    static void clear(int side, Db* d) { if (side == 0) at<Fn0>(0x00029F00)(d, nullptr); else d->Clear(); }
    static void del(int side, Db* d, unsigned flags) {
        if (side == 0) eq("returns this", (uintptr_t)at<FnDel>(0x00029FF0)(d, nullptr, flags), (uintptr_t)d);
        else { d->~Db(); if (flags & 1) ::operator delete(d); }
    }
    static void same(const Db* o, const Db* r) {
        same_sourced(o, r);
        eq("demand count", *(unsigned*)(uintptr_t)0x002C6360, AtImageDatabase::s_demandLoadCount);
        same_map(o, r, g_images[0], g_images[1]);
        same_table(o, r, g_own[0]->m_images, g_own[1]->m_images, g_own[0]->m_imageSpaces, g_own[1]->m_imageSpaces);
    }
    static void more(Db* o, Db* r) { same_directory_calls<Db>(o, r, 0x0002A450, 0x00029F40, &Db::PushDirectory, &Db::PopDirectory); }
};

struct Agf {
    enum { kInnerTable = 0 };
    typedef AtFontDatabase Db;
    static constexpr const char* kExt = ".AGF\t";
    static Db* make(int side, unsigned char fill) {
        Db* d;
        if (side == 0) {
            d = raw_object<Db>(sizeof(Db), fill);
            at<Fn0>(0x0002A020)(d, nullptr);
            *(const void***)d = g_font_table;
        } else {
            d = raw_object<Db>(sizeof(TestFontDb), fill);
            new (d) TestFontDb;
        }
        g_db_of[side] = d;
        return d;
    }
    static void load(int side, Db* d) {
        if (side == 0) at<FnLoad>(0x0002A290)(d, nullptr, g_own[0], "MODEL\\THING", 77);
        else d->Load(g_own[1], "MODEL\\THING", 77);
    }
    static void clear(int side, Db* d) { if (side == 0) at<Fn0>(0x0002A410)(d, nullptr); else d->Clear(); }
    static void del(int side, Db* d, unsigned flags) {
        if (side == 0) eq("returns this", (uintptr_t)at<FnDel>(0x0002A500)(d, nullptr, flags), (uintptr_t)d);
        else { d->~Db(); if (flags & 1) ::operator delete(d); }
    }
    static void same(const Db* o, const Db* r) {
        same_sourced(o, r);
        eq("demand count", *(unsigned*)(uintptr_t)0x002C636C, AtFontDatabase::s_demandLoadCount);
        same_map(o, r, g_fonts[0], g_fonts[1]);
        same_table(o, r, g_own[0]->m_fonts, g_own[1]->m_fonts, g_own[0]->m_fontSpaces, g_own[1]->m_fontSpaces);
    }
    static void more(Db* o, Db* r) { same_directory_calls<Db>(o, r, 0x0002A450, 0x00029F40, &Db::PushDirectory, &Db::PopDirectory); }
};

struct Agl {
    enum { kInnerTable = 0 };
    typedef AtLightDatabase Db;
    static constexpr const char* kExt = ".AGL\t";
    static Db* make(int side, unsigned char fill) {
        Db* d;
        if (side == 0) {
            d = raw_object<Db>(sizeof(Db), fill);
            at<Fn0>(0x0002AB00)(d, nullptr);
            *(const void***)d = g_light_table;
        } else {
            d = raw_object<Db>(sizeof(TestLightDb), fill);
            new (d) TestLightDb;
        }
        return d;
    }
    static void load(int side, Db* d) {
        if (side == 0) at<FnLoad>(0x0002AC10)(d, nullptr, g_own[0], "MODEL\\THING", 77);
        else d->Load(g_own[1], "MODEL\\THING", 77);
    }
    static void clear(int side, Db* d) { if (side == 0) at<Fn0>(0x0002ACB0)(d, nullptr); else d->Clear(); }
    static void del(int side, Db* d, unsigned flags) {
        if (side == 0) eq("returns this", (uintptr_t)at<FnDel>(0x0002AD30)(d, nullptr, flags), (uintptr_t)d);
        else { d->~Db(); if (flags & 1) ::operator delete(d); }
    }
    static void same(const Db* o, const Db* r) {
        eq("count", o->m_count, r->m_count);
        same_map(o, r, g_lights[0], g_lights[1]);
        same_table(o, r, g_own[0]->m_lights, g_own[1]->m_lights, g_own[0]->m_lightSpaces, g_own[1]->m_lightSpaces);
        for (unsigned i = 0; i < std::min(std::min(o->m_count, r->m_count), (unsigned)kPool); i++) {
            const AtLight* x = (const AtLight*)(const TestLight*)g_lights[0][i];
            const AtLight* y = (const AtLight*)(const TestLight*)g_lights[1][i];
            eq("light's owner", x->m_owner == g_own[0], y->m_owner == g_own[1]);
            eq("light", std::memcmp((const char*)x + 8, (const char*)y + 8, sizeof(AtLight) - 8), 0);
        }
        g_items_seen += o->m_count;
    }
    static void more(Db*, Db*) {}
};

struct Asl {
    enum { kInnerTable = 0 };
    typedef AtLensFlareDatabase Db;
    static constexpr const char* kExt = ".ASL\t";
    static Db* make(int side, unsigned char fill) {
        Db* d = raw_object<Db>(sizeof(Db), fill);
        if (side == 0) at<Fn0>(0x0003F8F0)(d, nullptr);
        else new (d) Db;
        return d;
    }
    static void load(int side, Db* d) {
        if (side == 0) at<FnLoad>(0x0003FAC0)(d, nullptr, g_scene[0], "MODEL\\THING", 77);
        else d->Load(g_scene[1], "MODEL\\THING", 77);
    }
    static void clear(int side, Db* d) { if (side == 0) at<Fn0>(0x0003FB60)(d, nullptr); else d->Clear(); }
    static void del(int side, Db* d, unsigned flags) {
        if (side == 0) eq("returns this", (uintptr_t)at<FnDel>(0x0002E9A0)(d, nullptr, flags), (uintptr_t)d);
        else { d->~Db(); if (flags & 1) ::operator delete(d); }
    }
    static void same_flare(const AtLensFlare& x, const AtLensFlare& y) {
        eq("flare's owner", x.m_owner == g_scene[0], y.m_owner == g_scene[1]);
        eq("flare's owner set", x.m_owner != nullptr, y.m_owner != nullptr);
        eq("flare's flags", x.m_flags, y.m_flags);
        same_list("flare's materials", x.m_materials, y.m_materials);
        eq("flare", std::memcmp(&x.m_centerColor, &y.m_centerColor, offsetof(AtLensFlare, m_squareCount) - offsetof(AtLensFlare, m_centerColor)), 0);
        eq("flare's squares", x.m_squareCount, y.m_squareCount);
        eq("flare's squares", x.m_squares != nullptr, y.m_squares != nullptr);
        if (x.m_squares && y.m_squares && x.m_squareCount == y.m_squareCount) eq("flare's squares", std::memcmp(x.m_squares, y.m_squares, x.m_squareCount * sizeof(AtLensFlareSquare)), 0);
        eq("flare's end", std::memcmp(&x.m_unknown58, &y.m_unknown58, 8), 0);
    }
    static void same(const Db* o, const Db* r) {
        eq("count", o->m_count, r->m_count);
        eq("flares", o->m_flares != nullptr, r->m_flares != nullptr);
        same_map(o, r, o->m_flares, r->m_flares);
        same_table(o, r, g_scene[0]->m_lensFlares, g_scene[1]->m_lensFlares, g_scene[0]->m_lensFlareSpaces, g_scene[1]->m_lensFlareSpaces);
        if (o->m_flares && r->m_flares && o->m_count == r->m_count) {
            eq("cookie", ((const unsigned*)o->m_flares)[-1], ((const unsigned*)r->m_flares)[-1]);
            for (unsigned i = 0; i < o->m_count; i++) same_flare(o->m_flares[i], r->m_flares[i]);
            g_items_seen += o->m_count;
        }
    }
    static void more(Db*, Db*) {}
};

static unsigned g_files[6];
static volatile LONG g_progress;

// A second Image word, or any load over a table, has an image's or font's database clear the file system it has just noted, and it then comes to its pop on null, the original no less. So every load is guarded,
// and a crash is compared, with the path put back as it was.
static unsigned g_crashes_seen;
template <class T> static void load_both(typename T::Db* o, typename T::Db* r, const char* what) {
    const int top = g_fs->m_path.m_top;
    const bool oc = guarded([&] { On on(0); T::load(0, o); });
    g_recording = false;
    const int otop = g_fs->m_path.m_top;
    g_fs->m_path.m_top = top;
    const bool rc = guarded([&] { On on(1); T::load(1, r); });
    g_recording = false;
    eq("crash in load", oc, rc);
    eq("left pushed", otop, g_fs->m_path.m_top);
    g_fs->m_path.m_top = top;
    if (oc) g_crashes_seen++;
    T::same(o, r);
    compare_logs(what);
}

template <class T> static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill) {
    g_context = context;
    InterlockedIncrement(&g_progress);
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, "MODEL\\SUB\\THING.EXT");
    g_fill = fill;
    typename T::Db* o = T::make(0, fill);
    typename T::Db* r = T::make(1, fill);
    {
        // All but the vftables.
        unsigned char x[sizeof(typename T::Db)], y[sizeof(typename T::Db)];
        std::memcpy(x, o, sizeof x);
        std::memcpy(y, r, sizeof y);
        std::memset(x, 0, 4);
        std::memset(y, 0, 4);
        if (T::kInnerTable) {
            std::memset(x + T::kInnerTable, 0, 4);
            std::memset(y + T::kInnerTable, 0, 4);
        }
        eq("constructed", std::memcmp(x, y, sizeof x), 0);
    }
    load_both<T>(o, r, "load");
    T::more(o, r);
    compare_logs("after");
    // Over what is there.
    g_keep_lights = true;
    load_both<T>(o, r, "load again");
    g_keep_lights = false;
    // Cleared, and clearing nothing.
    { On on(0); T::clear(0, o); T::clear(0, o); }
    { On on(1); T::clear(1, r); T::clear(1, r); }
    g_recording = false;
    T::same(o, r);
    compare_logs("clear");
    load_both<T>(o, r, "load after clear");
    { On on(0); T::del(0, o, 0); }
    { On on(1); T::del(1, r, 0); }
    compare_logs("destroy");
    std::free(o);
    std::free(r);
    // The deleting form, with something held.
    o = T::make(0, fill);
    r = T::make(1, fill);
    load_both<T>(o, r, "load to delete");
    { On on(0); T::del(0, o, 1); }
    { On on(1); T::del(1, r, 1); }
    compare_logs("delete");
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

template <class T> static void run_list(const char* const* texts, size_t n) {
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (size_t i = 0; i < n; i++) run_text<T>(bytes(texts[i]), std::string(T::kExt) + " text [" + texts[i] + "]", fill);
    }
}
#define RUN_LIST(T, texts) run_list<T>(texts, sizeof(texts) / sizeof(texts[0]))

// Not written anywhere here: a name that is not a string, where a name is required, since ExpectName then leaves its buffer as it was, which is uninitialised stack on both sides; an empty name that is
// searched for, as a .AGA key's is, since the hash of an empty name reads its second byte, which is uninitialised stack too, and so finds or misses by chance (a key found so differed here, and the difference
// is the stack's); and text that ends
// inside a light's, a flare's, a square's or a run's body, which both sides report in a loop for ever.
static void run_written() {
    static const char* const agn[] = {
        "MatAssignment[0]\n{\n}\n",
        "MatAssignment[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n}\n",
        "MatAssignment[2]\n{\n MatAssignment \"A\"[2]\n {\n \"M\"\n \"XGone\"\n }\n MatAssignment \"B\"[1]\n {\n \"\"\n }\n}\n",
        "NameSpace \"Space\"\nMatAssignment[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n}\n",
        "NameSpace \"\"\nMatAssignment[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n}\n",
        "NameSpace \"Fresh\"\nMatAssignment[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n}\n",
        "MatAssignmentWithNameSpace[2]\n{\n MatAssignment \"A\"[2]\n {\n \"M\" \"Space\"\n \"N\" \"\"\n }\n MatAssignment \"B\"[1]\n {\n \"\" \"Space\"\n }\n}\n",
        "NameSpace \"Space\"\nMatAssignmentWithNameSpace[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\" \"\"\n }\n}\n",
        "NameSpace \"Space\"\nMatAssignmentWithNameSpace[1]\n{\n MatAssignmentWithNameSpace \"A\"[1]\n {\n \"M\" \"XNope\"\n }\n}\n",
        "MatAssignment[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n",
        "MatAssignment[1]\n{\n MatAssignment \"A\"[0]\n {\n }\n}\n",
        "MatAssignment[1]\n{\n MatAssignment \"Longer than eight\"[1]\n {\n \"Longer than eight\"\n }\n}\n",
        "MatAssignment[2]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n MatAssignment \"A\"[1]\n {\n \"N\"\n }\n}\n",
        "MatAssignment[1]\n{\n Other \"A\"[1]\n {\n \"M\"\n }\n}\n",
        "Other[0]\n{\n}\n",
        "NameSpace \"Space\"\nOther[0]\n{\n}\n",
    };
    RUN_LIST(Agn, agn);

    static const char* const aga[] = {
        "",
        "// nothing but a comment\n",
        "5\n",
        "NameSpace \"Space\"\n",
        "FrameNumber[3]\n{\n 0\n 2\n 4\n}\n",
        "FrameNumber[0]\n{\n}\n",
        "FrameNumber[2]\n{\n 70000\n -1\n}\n",
        "FrameNumber[1]\n{\n 1\n}\nFrameNumber[2]\n{\n 3\n 4\n}\n",
        "MaterialKey[2]\n{\n \"M\" \"\"\n \"XGone\" \"Space\"\n}\n",
        "MaterialKey[0]\n{\n}\n",
        "MaterialKey[1]\n{\n \"M\" \"\"\n}\nMaterialKey[1]\n{\n \"N\" \"Space\"\n}\n",
        "MatAnimation[0]\n{\n}\n",
        "MatAnimation[1]\n{\n MatAnimation \"A\"\n {\n }\n}\n",
        "MatAnimation[1]\n{\n MatAnimation \"A\"\n {\n }\n}\nMatAnimation[1]\n{\n MatAnimation \"B\"\n {\n }\n}\n",
        "FrameNumber[4]\n{\n 0\n 2\n 4\n 6\n}\nMaterialKey[4]\n{\n \"M\" \"\"\n \"N\" \"\"\n \"O\" \"\"\n \"P\" \"Space\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 0 0 4\n Rate 15.5\n Length 8\n LoopPeriod 10\n }\n}\n",
        "NameSpace \"Space\"\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nFrameNumber[2]\n{\n 1\n 5\n}\nMatAnimation[2]\n{\n MatAnimation \"A\"\n {\n LoopPeriod 12\n Length 8\n Keyframes 0 0 2\n }\n MatAnimation \"B\"\n {\n Length 4\n Keyframes 1 1 1\n }\n}\n",
        "FrameNumber[2]\n{\n 1\n 5\n}\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Length 8\n LoopPeriod 3\n }\n}\n",
        "FrameNumber[2]\n{\n 1\n 5\n}\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Length 8\n LoopPeriod 8\n Keyframes 2 0 1\n }\n}\n",
        "FrameNumber[2]\n{\n 1\n 5\n}\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 0 2 1\n }\n}\n",
        "FrameNumber[2]\n{\n 1\n 5\n}\nMaterialKey[3]\n{\n \"M\" \"\"\n \"N\" \"\"\n \"O\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 1 0 2\n }\n}\n",
        "FrameNumber[3]\n{\n 1\n 5\n 6\n}\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 0 1 2\n }\n}\n",
        "FrameNumber[3]\n{\n 1\n 5\n 6\n}\nMaterialKey[3]\n{\n \"M\" \"\"\n \"N\" \"\"\n \"O\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 0 0 3\n Other\n Rate\n }\n}\n",
        "MatAnimation[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "MatAnimation[1]\n{\n MatAnimation \"A\"\n {\n }\n",
    };
    RUN_LIST(Aga, aga);

    static const char* const agi[] = {
        "",
        "5\n",
        "MatAssignment[2]\n{\n \"M\"\n \"XGone\"\n}\n",
        "NameSpace \"Space\"\nMatAssignment[1]\n{\n \"M\"\n}\n",
        "NameSpace \"\"\nMatAssignment[1]\n{\n \"M\"\n}\n",
        "Image[0]\n{\n}\n",
        "Image[2]\n{\n Image \"A\"\n {\n TgaSource\n }\n Image \"B\"\n {\n MatAssignment 0\n FrequentModify\n }\n}\n",
        "NameSpace \"Fresh\"\nImage[1]\n{\n Image \"A\"\n {\n }\n}\nMatAssignment[1]\n{\n \"M\"\n}\n",
        "Image[2]\n{\n Image \"A\"\n {\n DemandLoad\n }\n Image \"B\"\n {\n }\n}\n",
        "Image[1]\n{\n Image \"A\"\n {\n DemandLoad\n }\n}\n",
        "Image[1]\n{\n Image \"A\"\n {\n }\n}\nImage[1]\n{\n Image \"B\"\n {\n }\n}\n",
        "Image[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "Image[1]\n{\n Image \"A\"\n {\n }\n",
        "Image\n{\n}\n",
        "MatAssignment[1]\n{\n \"M\"\n}\nMatAssignment[1]\n{\n \"N\"\n}\n",
    };
    RUN_LIST(Agi, agi);

    static const char* const agf[] = {
        "",
        "5\n",
        "Font[1]\n",
        "Font[0]\n",
        "Font\n",
        "NameSpace \"Space\"\nFont[1]\n",
        "NameSpace \"\"\nFont[1]\n",
        "NameSpace \"Space\"\n5\n",
        "Font[1]\nMatAssignment[2]\n{\n \"M\"\n \"XGone\"\n}\n",
        "NameSpace \"Space\"\nFont[1]\nMatAssignment[1]\n{\n \"M\"\n}\n",
        "Font[2]\nFont2D[2]\n{\n Font2D \"A\"\n {\n TgaSource\n }\n Font2D \"B\"\n {\n FrequentModify\n CharacterSet[1]\n {\n 65\n }\n }\n}\n",
        "Font[2]\nFont2D[2]\n{\n Font2D \"A\"\n {\n DemandLoad\n }\n Font2D \"B\"\n {\n }\n}\n",
        "Font[2]\nFont2D[1]\n{\n Font2D \"A\"\n {\n DemandLoad\n }\n}\n",
        "Font[1]\nFont2D[2]\n{\n Font2D \"A\"\n {\n }\n Font2D \"B\"\n {\n }\n}\n",
        "Font[1]\nFont2D[0]\n{\n}\n",
        "Font[2]\nFont2D[1]\n{\n Font2D \"A\"\n {\n }\n}\nFont2D[1]\n{\n Font2D \"B\"\n {\n }\n}\n",
        "Font[1]\nFont3D\n",
        "Font[1]\nOther\n",
        "Font[1]\nFont2D[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "Font[1]\nFont2D[1]\n{\n Font2D \"A\"\n {\n }\n",
    };
    RUN_LIST(Agf, agf);

    static const char* const agl[] = {
        "Light[0]\n{\n}\n",
        "Light[1]\n{\n Light \"A\"\n {\n }\n}\n",
        "Light[2]\n{\n Light \"A\"\n {\n AmbientColor 0.25 0.5 0.75 1.5\n DiffuseColor 1.25 2.5 3.75 4.5\n SpecularColor 5.25 6.5 7.75 8.5\n Range 400.5\n Attenuation 1.5 0.25 0.125\n InnerConeAngle 12.5\n OuterConeAngle 22.5\n }\n Light \"B\"\n {\n Range 9.5\n }\n}\n",
        "NameSpace \"Space\"\nLight[1]\n{\n Light \"A\"\n {\n }\n}\n",
        "NameSpace \"\"\nLight[1]\n{\n Light \"A\"\n {\n }\n}\n",
        "NameSpace \"Fresh\"\nLight[1]\n{\n Light \"A\"\n {\n }\n}\n",
        "NameSpace \"Space\"\nOther[0]\n{\n}\n",
        "Other[1]\n{\n Light \"A\"\n {\n }\n}\n",
        "Light[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "Light[1]\n{\n Light \"A\"\n {\n Other\n Light\n 5\n }\n}\n",
        "Light[1]\n{\n Light \"A\"\n {\n Range\n }\n}\n",
        "Light[1]\n{\n Light \"A\"\n {\n }\n",
        "Light[2]\n{\n Light \"A\"\n {\n }\n Light \"A\"\n {\n }\n}\n",
    };
    RUN_LIST(Agl, agl);

    static const char* const asl[] = {
        "LensFlare[0]\n{\n}\n",
        "[0]\n{\n}\n",
        "Other[1]\n{\n LensFlare \"A\"\n {\n }\n}\n",
        "LensFlare[1]\n{\n LensFlare \"A\"\n {\n }\n}\n",
        "NameSpace \"Space\"\nLensFlare[1]\n{\n LensFlare \"A\"\n {\n }\n}\n",
        "NameSpace \"\"\nLensFlare[1]\n{\n LensFlare \"A\"\n {\n }\n}\n",
        "NameSpace \"Fresh\"\nOther[1]\n{\n LensFlare \"A\"\n {\n }\n}\n",
        "LensFlare[1]\n{\n Other \"A\"\n {\n }\n}\n",
        "LensFlare[1]\n{\n LensFlare \"A\"\n {\n }\n",
        "LensFlare[2]\n{\n LensFlare \"A\"\n {\n LensCenter 0.5 0.25 0.125 1.0\n LensInnerRing 2.5 3.5 0.1 0.2 0.3 0.4\n LensOuterRing 4.5 5.5 0.6 0.7 0.8 0.9\n DisableDistanceColoring\n }\n LensFlare \"B\"\n {\n DisableDepthColoring\n }\n}\n",
        "LensFlare[1]\n{\n LensFlare \"A\"\n {\n MatAssignment[2]\n {\n \"M\"\n \"XGone\"\n }\n Flare[3]\n {\n Flare\n {\n MatAssignment 1\n AmountTowardsLight 0.5\n SquareSize 7.5\n RotateFlare\n }\n Flare\n {\n }\n Flare\n {\n MatAssignment 2\n }\n }\n }\n}\n",
        "LensFlare[1]\n{\n LensFlare \"A\"\n {\n MatAssignmentWithNameSpace[2]\n {\n \"M\" \"Space\"\n \"\" \"\"\n }\n }\n}\n",
        "LensFlare[1]\n{\n LensFlare \"A\"\n {\n Flare[1]\n {\n Flare\n {\n MatAssignmentWithNameSpace\n LensCenter\n }\n }\n Flare[1]\n {\n Other\n {\n }\n }\n }\n}\n",
        "LensFlare[1]\n{\n LensFlare \"A\"\n {\n Flare[0]\n {\n }\n AmountTowardsLight\n SquareSize\n RotateFlare\n Other\n }\n}\n",
    };
    RUN_LIST(Asl, asl);
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
    explicit Direct(const char* t, const AtIoKeyword* table, unsigned count) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(table, count, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void same_keywords(const char* what, uint32_t va, const AtIoKeyword* mine, unsigned count) {
    g_context = what;
    for (unsigned i = 0; i < count; i++) {
        const AtIoKeyword& k = ((const AtIoKeyword*)(uintptr_t)va)[i];
        eq("id", k.m_id, mine[i].m_id);
        eqs("word", k.m_name, mine[i].m_name);
    }
}

typedef void(__fastcall* FnReadList)(void*, void*, void*, void*, const char*);
typedef void(__fastcall* FnReadListNs)(void*, void*, void*, void*);
typedef void(__fastcall* FnCopyList)(void*, void*, void*);
typedef void(__fastcall* FnSetRead)(void*, void*, void*);
typedef void(__fastcall* FnReserve)(void*, void*, unsigned);
typedef unsigned short(__fastcall* FnFrameAt)(void*, void*, unsigned);
typedef void(__fastcall* FnSetName)(void*, void*, const char*);
typedef void(__cdecl* FnSetHeap)(void*);

static void run_lists() {
    g_context = "list";
    Env env({"A", "B", "C"}, {"Space", "Other"});
    g_fill = 0xCD;
    static const char* const single[] = {"[2]\n{\n \"A\"\n \"B\"\n}\n", "[0]\n{\n}\n", "[1]\n{\n \"\"\n}\n", "[2]\n{\n \"XGone\"\n \"C\"\n}\n", "[1]\n{\n \"A\"\n \"B\"\n}\n", "[3]\n{\n \"A\"\n}\n"};
    static const char* const pairs[] = {"[2]\n{\n \"A\" \"Other\"\n \"B\" \"\"\n}\n", "[0]\n{\n}\n", "[1]\n{\n \"\" \"Other\"\n}\n", "[2]\n{\n \"XGone\" \"Other\"\n \"C\" \"XNope\"\n}\n"};
    AtMatAssignment o, r;  // Held on the object from one read to the next, over each case.
    for (const char* text : single) {
        for (const char* ns : {(const char*)nullptr, "", "Space", "XNope"}) {
            Direct da(text, AtMatAssignmentDatabase::s_keywords, 3), db(text, AtMatAssignmentDatabase::s_keywords, 3);
            { On on(0); at<FnReadList>(0x000348D0)(&o, nullptr, g_own[0], &da.tokenizer, ns); }
            { On on(1); r.Read(g_own[1], &db.tokenizer, ns); }
            same_list("read", o, r);
            compare_logs("read");
        }
    }
    for (const char* text : pairs) {
        Direct da(text, AtMatAssignmentDatabase::s_keywords, 3), db(text, AtMatAssignmentDatabase::s_keywords, 3);
        { On on(0); at<FnReadListNs>(0x000347C0)(&o, nullptr, g_own[0], &da.tokenizer); }
        { On on(1); r.ReadWithNameSpaces(g_own[1], &db.tokenizer); }
        same_list("read with spaces", o, r);
        compare_logs("read with spaces");
    }
    // A copy: over nothing, over something, of nothing. Each list on its own side's.
    for (const char* text : single) {
        Direct da(text, AtMatAssignmentDatabase::s_keywords, 3), db(text, AtMatAssignmentDatabase::s_keywords, 3);
        { On on(0); at<FnReadList>(0x000348D0)(&o, nullptr, g_own[0], &da.tokenizer, nullptr); }
        { On on(1); r.Read(g_own[1], &db.tokenizer, nullptr); }
        compare_logs("before copy");
        if (o.m_count > 2) continue;
        AtMatAssignment co, cr;
        { On on(0); at<FnCopyList>(0x000349D0)(&co, nullptr, &o); at<FnCopyList>(0x000349D0)(&co, nullptr, &o); }
        { On on(1); cr.Copy(r); cr.Copy(r); }
        same_list("copy", co, cr);
        compare_logs("copy");
        eq("copy's own block", co.m_items != o.m_items, cr.m_items != r.m_items);
    }
    // The list's destructor as a call.
    { On on(0); at<Fn0>(0x00053570)(&o, nullptr); at<Fn0>(0x00053570)(&o, nullptr); }
    { On on(1); r.~AtMatAssignment(); r.~AtMatAssignment(); }
    same_list("emptied", o, r);
    compare_logs("emptied");
    { AtMatAssignment a; std::memset(&a, 0xEE, sizeof a); at<Fn0>(0x00018810)(&a, nullptr); eq("from new", a.m_count + (uintptr_t)a.m_items, 0); }

    // An empty name is not searched for. A map with nothing but the empty name in every slot finds it whatever the hash of it, so a search would show.
    g_context = "empty names";
    {
        GmNameMap full[2];
        for (int side = 0; side < 2; side++) {
            full[side].Init(3);
            for (unsigned i = 0; i < full[side].m_size; i++) {
                std::memset(full[side].m_entries[i].m_name, 0, 8);
                full[side].m_entries[i].m_value = (void*)(uintptr_t)0x1234;
            }
            g_own[side]->m_materials.Add(&full[side]);
        }
        {
            Direct da("[1]\n{\n \"\"\n}\n", AtMatAssignmentDatabase::s_keywords, 3), db("[1]\n{\n \"\"\n}\n", AtMatAssignmentDatabase::s_keywords, 3);
            { On on(0); at<FnReadList>(0x000348D0)(&o, nullptr, g_own[0], &da.tokenizer, nullptr); }
            { On on(1); r.Read(g_own[1], &db.tokenizer, nullptr); }
            same_list("empty name", o, r);
            compare_logs("empty name");
        }
        {
            Direct da("[1]\n{\n \"\" \"\"\n}\n", AtMatAssignmentDatabase::s_keywords, 3), db("[1]\n{\n \"\" \"\"\n}\n", AtMatAssignmentDatabase::s_keywords, 3);
            { On on(0); at<FnReadListNs>(0x000347C0)(&o, nullptr, g_own[0], &da.tokenizer); }
            { On on(1); r.ReadWithNameSpaces(g_own[1], &db.tokenizer); }
            same_list("empty name with spaces", o, r);
            compare_logs("empty name with spaces");
        }
        g_recording = false;
        for (int side = 0; side < 2; side++) g_own[side]->m_materials.Detach(&full[side]);
    }

    // The owner's.
    g_context = "owner";
    for (const char* name : {"A", "B", "XGone", ""}) {
        for (const char* ns : {(const char*)nullptr, "", "Space", "Other", "XNope"}) {
            eq("material", (uintptr_t)at<FnFind>(0x00027EF0)(g_own[0], nullptr, name, ns), (uintptr_t)g_own[1]->FindMaterial(name, ns));
            eqs("last space", group_name(g_own[0]->m_materialSpaces.m_last).c_str(), group_name(g_own[1]->m_materialSpaces.m_last).c_str());
        }
    }
    typedef void (AtMaterialOwner::*Add)(GmNameMap*, const char*);
    struct { uint32_t va; Add add; GmNameMapGroup AtMaterialOwner::*group; } adds[] = {
        {0x00025F70, &AtMaterialOwner::AddLights, &AtMaterialOwner::m_lights},
        {0x00025FF0, &AtMaterialOwner::AddMatAssignments, &AtMaterialOwner::m_matAssignments},
        {0x00026030, &AtMaterialOwner::AddMatAnimations, &AtMaterialOwner::m_matAnimations},
        {0x000260B0, &AtMaterialOwner::AddFonts, &AtMaterialOwner::m_fonts},
        {0x000260F0, &AtMaterialOwner::AddImages, &AtMaterialOwner::m_images},
    };
    for (const auto& a : adds) {
        for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
            GmNameMap x, y;
            at<FnAdd>(a.va)(g_own[0], nullptr, &x, ns);
            (g_own[1]->*a.add)(&y, ns);
            eq("group", x.m_group == &(g_own[0]->*a.group), y.m_group == &(g_own[1]->*a.group));
            eqs("group's name", group_name(x.m_group).c_str(), group_name(y.m_group).c_str());
            eq("first", (g_own[0]->*a.group).m_first == &x, (g_own[1]->*a.group).m_first == &y);
        }
    }
    for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
        GmNameMap x, y;
        at<FnAdd>(0x00040070)(g_scene[0], nullptr, &x, ns);
        g_scene[1]->AddLensFlares(&y, ns);
        eq("flare group", x.m_group == &g_scene[0]->m_lensFlares, y.m_group == &g_scene[1]->m_lensFlares);
        eqs("flare group's name", group_name(x.m_group).c_str(), group_name(y.m_group).c_str());
        eq("flare first", g_scene[0]->m_lensFlares.m_first == &x, g_scene[1]->m_lensFlares.m_first == &y);
    }
}

static void run_sets() {
    g_context = "frame set";
    g_fill = 0xCD;
    static const AtIoKeyword words[] = {{0x4B, "Elsewhere"}};
    unsigned char ob[sizeof(AtGrFrameNumberSet)], rb[sizeof(AtGrFrameNumberSet)];
    std::memset(ob, 0xEE, sizeof ob);
    std::memset(rb, 0xEE, sizeof rb);
    { On on(0); at<Fn0>(0x00034CE0)(ob, nullptr); }
    { On on(1); new (rb) AtGrFrameNumberSet; }
    AtGrFrameNumberSet* o = (AtGrFrameNumberSet*)ob;
    AtGrFrameNumberSet* r = (AtGrFrameNumberSet*)rb;
    same_set(*o, *r);
    compare_logs("new");

    // Named elsewhere: no entries, and the load stops.
    {
        Direct da("[3]\n{\n Elsewhere \"Far\"\n}\n", words, 1), db("[3]\n{\n Elsewhere \"Far\"\n}\n", words, 1);
        { On on(0); at<FnSetRead>(0x00034F90)(o, nullptr, &da.tokenizer); }
        { On on(1); r->Read(&db.tokenizer); }
        o->m_frames = r->m_frames = nullptr;
        same_set(*o, *r);
        compare_logs("named");
        // A count with nothing allocated: the free is on a null block on both sides.
        { On on(0); at<Fn0>(0x00034EB0)(o, nullptr); }
        { On on(1); r->Load(); }
        compare_logs("load of a named set");
    }
    // Reading over it, with a count left, and over a second read.
    for (const char* text : {"[2]\n{\n 5\n 9\n}\n", "[3]\n{\n 1\n 2\n 3\n}\n", "[0]\n{\n}\n", "[1]\n{\n Elsewhere \"Far\"\n}\n", "[2]\n{\n 7\n Elsewhere\n}\n"}) {
        Direct da(text, words, 1), db(text, words, 1);
        { On on(0); at<FnSetRead>(0x00034F90)(o, nullptr, &da.tokenizer); }
        { On on(1); r->Read(&db.tokenizer); }
        same_set(*o, *r);
        compare_logs("read");
        if (o->m_flags & 2) {
            for (unsigned i = 0; i < o->m_count; i++) eq("At", at<FnFrameAt>(0x00034CD0)(o, nullptr, i), r->At(i));
        }
        { On on(0); at<Fn0>(0x00034EB0)(o, nullptr); }
        { On on(1); r->Load(); }
        compare_logs("load");
    }
    // A count at the start of a read that nothing marked read: the report.
    { On on(0); at<Fn0>(0x00034D00)(o, nullptr); }
    { On on(1); r->Clear(); }
    compare_logs("clear");
    {
        at<FnReserve>(0x00035010)(o, nullptr, 2);
        r->Reserve(2);
        o->m_flags = r->m_flags = 0;
        Direct da("[1]\n{\n 4\n}\n", words, 1), db("[1]\n{\n 4\n}\n", words, 1);
        { On on(0); at<FnSetRead>(0x00034F90)(o, nullptr, &da.tokenizer); }
        { On on(1); r->Read(&db.tokenizer); }
        same_set(*o, *r);
        compare_logs("read with a count left");
    }
    // Room with no file.
    for (unsigned n : {0u, 3u, 1u}) {
        { On on(0); at<FnReserve>(0x00035010)(o, nullptr, n); }
        { On on(1); r->Reserve(n); }
        eq("reserve flags", o->m_flags, r->m_flags);
        eq("reserve count", o->m_count, r->m_count);
        compare_logs("reserve");
    }
    // A name given later, on one that holds a count.
    for (const char* name : {"Named", "LongerThanEight", ""}) {
        { On on(0); at<FnSetName>(0x00034F40)(o, nullptr, name); at<Fn0>(0x00034EB0)(o, nullptr); }
        { On on(1); r->SetExternalName(name); r->Load(); }
        same_set(*o, *r);
        compare_logs("named later");
    }

    // Runs.
    { On on(0); at<Fn0>(0x00034D00)(o, nullptr); }
    { On on(1); r->Clear(); }
    compare_logs("clear");
    static const char* const runs[] = {"[1]\n{\n 4\n}\n", "[2]\n{\n 4\n 9\n}\n", "[5]\n{\n 0\n 2\n 4\n 6\n 8\n}\n", "[8]\n{\n 1\n 2\n 3\n 7\n 7\n 20\n 21\n 40\n}\n", "[3]\n{\n 65535\n 65535\n 65535\n}\n"};
    for (const char* text : runs) {
        Direct da(text, words, 1), db(text, words, 1);
        { On on(0); at<FnSetRead>(0x00034F90)(o, nullptr, &da.tokenizer); }
        { On on(1); r->Read(&db.tokenizer); }
        compare_logs("run");
        const unsigned n = o->m_count;
        for (unsigned first = 0; first < n; first++) {
            for (unsigned count = 1; first + count <= n; count++) {
                for (const int length : {0, 4, 60, 100000}) same_times(o, r, first, count, length);
            }
        }
    }
    // Times the arithmetic cares about, on the last set read.
    for (const float t : {-1e9f, 1e9f, 0.0f, -0.0f, 3.5f, 65534.5f, 70000.0f}) same_run(o, r, 0, o->m_count, 60, t);

    // Gone: the free, and nothing left.
    { On on(0); at<Fn0>(0x00034ED0)(o, nullptr); }
    { On on(1); r->~AtGrFrameNumberSet(); }
    same_set(*o, *r);
    compare_logs("destroyed");

    // No room.
    {
        std::memset(ob, 0xEE, sizeof ob);
        std::memset(rb, 0xEE, sizeof rb);
        at<Fn0>(0x00034CE0)(ob, nullptr);
        new (rb) AtGrFrameNumberSet;
        g_fail_alloc = true;
        { On on(0); at<FnReserve>(0x00035010)(o, nullptr, 2); }
        { On on(1); r->Reserve(2); }
        g_fail_alloc = false;
        compare_logs("no room");
        o->m_count = r->m_count = 0;
    }
}

// A run's own read, and the flare's parts, called with no database reading around them.
static void run_parts() {
    g_context = "parts";
    Env env({"M", "N"}, {"Space"});
    g_fill = 0xCD;

    typedef void(__fastcall* FnFlareRead)(void*, void*, void*, void*);
    typedef void(__fastcall* FnSquare)(void*, void*, void*, void*);
    unsigned char ob[sizeof(AtLensFlare)], rb[sizeof(AtLensFlare)];
    std::memset(ob, 0xEE, sizeof ob);
    std::memset(rb, 0xEE, sizeof rb);
    { On on(0); at<Fn0>(0x000521A0)(ob, nullptr); }
    { On on(1); new (rb) AtLensFlare; }
    AtLensFlare* o = (AtLensFlare*)ob;
    AtLensFlare* r = (AtLensFlare*)rb;
    Asl::same_flare(*o, *r);
    compare_logs("new flare");
    for (const char* text : {"{\n DisableDepthColoring\n}\n", "{\n Flare[1]\n {\n Flare\n {\n SquareSize 3\n }\n }\n}\n", "{\n MatAssignment[1]\n {\n \"M\"\n }\n}\n"}) {
        Direct da(text, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount), db(text, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount);
        { On on(0); at<FnFlareRead>(0x00052970)(o, nullptr, g_scene[0], &da.tokenizer); }
        { On on(1); r->Read(g_scene[1], &db.tokenizer); }
        Asl::same_flare(*o, *r);
        compare_logs("flare again");
    }
    for (const char* text : {"Flare\n{\n}\n", "Other\n{\n MatAssignment 9\n}\n", "Flare\n{\n MatAssignment 0\n AmountTowardsLight 2.5\n SquareSize 1.5\n RotateFlare\n}\n"}) {
        AtLensFlareSquare a, b;
        std::memset(&a, 0x44, sizeof a);
        std::memset(&b, 0x44, sizeof b);
        Direct da(text, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount), db(text, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount);
        { On on(0); at<FnSquare>(0x00052230)(o, nullptr, &a, &da.tokenizer); }
        { On on(1); r->ReadSquare(&b, &db.tokenizer); }
        eq("square", std::memcmp(&a, &b, sizeof a), 0);
        compare_logs("square");
    }
    { On on(0); at<Fn0>(0x000522E0)(o, nullptr); }
    { On on(1); r->Reset(); }
    Asl::same_flare(*o, *r);
    compare_logs("reset flare");
    // Scale of a colour by hand-picked values.
    {
        const char* text = "{\n LensCenter 1 0.5 0.003921 2\n}\n";
        Direct da(text, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount), db(text, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount);
        { On on(0); at<FnFlareRead>(0x00052970)(o, nullptr, g_scene[0], &da.tokenizer); }
        { On on(1); r->Read(g_scene[1], &db.tokenizer); }
        Asl::same_flare(*o, *r);
        compare_logs("flare's colour");
    }
    { On on(0); at<Fn0>(0x00052920)(o, nullptr); }
    { On on(1); r->~AtLensFlare(); }
    Asl::same_flare(*o, *r);
    compare_logs("destroyed flare");

    // One run on its own: from new, and read over.
    {
        typedef void(__fastcall* FnRunRead)(void*, void*, void*, void*, void*);
        unsigned char ra[sizeof(AtMatAnimation)], rc[sizeof(AtMatAnimation)];
        std::memset(ra, 0xEE, sizeof ra);
        std::memset(rc, 0xEE, sizeof rc);
        at<Fn0>(0x00034A80)(ra, nullptr);
        new (rc) AtMatAnimation;
        eq("run from new", std::memcmp(ra, rc, sizeof ra), 0);
        AtMatAnimationDatabase* dbo = Aga::make(0, 0xCD);
        AtMatAnimationDatabase* dbr = Aga::make(1, 0xCD);
        const auto same_run_fields = [&] {
            eq("run's database", ((AtMatAnimation*)ra)->m_database == dbo, ((AtMatAnimation*)rc)->m_database == dbr);
            eq("run", std::memcmp(ra + 4, rc + 4, sizeof ra - 4), 0);
        };
        for (const char* text : {"{\n Rate 2.5\n Length 3\n LoopPeriod 7\n}\n", "{\n}\n", "{\n Length 5\n}\n"}) {
            Direct da(text, AtMatAnimationDatabase::s_keywords, 8), db(text, AtMatAnimationDatabase::s_keywords, 8);
            { On on(0); at<FnRunRead>(0x00034B00)(ra, nullptr, g_own[0], dbo, &da.tokenizer); }
            { On on(1); ((AtMatAnimation*)rc)->Read(g_own[1], dbr, &db.tokenizer); }
            same_run_fields();
            compare_logs("run read over");
        }
    }

    // Fonts made with no file.
    typedef void(__fastcall* FnAlloc)(void*, void*, void*, unsigned, const char*);
    for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
        AtFontDatabase* fo = Agf::make(0, 0xCD);
        AtFontDatabase* fr = Agf::make(1, 0xCD);
        { On on(0); at<FnAlloc>(0x0002A3D0)(fo, nullptr, g_own[0], 3, ns); }
        { On on(1); fr->Allocate(g_own[1], 3, ns); }
        Agf::same(fo, fr);
        compare_logs("fonts allocated");
        { On on(0); at<FnAlloc>(0x0002A3D0)(fo, nullptr, g_own[0], 2, "Other"); }
        { On on(1); fr->Allocate(g_own[1], 2, "Other"); }
        eq("count", fo->m_count, fr->m_count);
        same_map(fo, fr, g_fonts[0], g_fonts[1]);
        compare_logs("fonts allocated over");
        same_directory_calls<AtFontDatabase>(fo, fr, 0x0002A450, 0x00029F40, &AtFontDatabase::PushDirectory, &AtFontDatabase::PopDirectory);
        { On on(0); Agf::del(0, fo, 1); }
        { On on(1); Agf::del(1, fr, 1); }
        compare_logs("fonts gone");
    }

    // A slot of the databases that does nothing, on the recovered side.
    {
        class Abstract : public TestFontDb {} f;
        eq("slots", f.Slot4() + f.Slot7(), 0);
        f.Slot5(1, 2);
        class AbstractImage : public TestImageDb {} i;
        eq("slot", i.Slot5(), 0);
    }
}

// The reads that make room, with none to be had: the raise, and whatever follows the same on both sides.
template <class T> static void run_broken(const char* text) {
    std::vector<unsigned char> data = bytes(text);
    std::set<std::string> names = {"M"}, spaces = {"Space"};
    g_context = std::string(T::kExt) + " broken [" + text + "]";
    Env env(names, spaces);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = 0xCD;
    typename T::Db* o = T::make(0, 0xCD);
    typename T::Db* r = T::make(1, 0xCD);
    for (int side = 0; side < 2; side++) strcpy_s(g_fs->files[side].m_path, "MODEL\\SUB\\THING.EXT");
    const int top = g_fs->m_path.m_top;
    const bool oc = guarded([&] { On on(0); T::load(0, o); });
    g_recording = false;
    g_fail_alloc = false;
    const int otop = g_fs->m_path.m_top;
    g_fs->m_path.m_top = top;
    const bool rc = guarded([&] { On on(1); T::load(1, r); });
    g_recording = false;
    g_fail_alloc = false;
    eq("crash", oc, rc);
    eq("left pushed", otop, g_fs->m_path.m_top);
    g_fs->m_path.m_top = top;
    compare_logs("broken");
    // Left as it is: the file's objects and the half-read database are not unwound.
}

static void run_failures() {
    g_context = "failures";
    typedef void(__cdecl* Set)(void*);
    struct Heap { uint32_t va; void** mine; } heaps[] = {
        {0x002C63E0, &AtMatAssignment::s_heap}, {0x002C6350, &AtMatAssignmentDatabase::s_heap}, {0x002C63E4, &AtGrFrameNumberSet::s_heap}, {0x002C6354, &AtMatAnimationDatabase::s_heap},
        {0x002C6364, &AtImageDatabase::s_heap}, {0x002C6370, &AtFontDatabase::s_heap}, {0x002C6418, &AtLensFlareDatabase::s_heap}, {0x002C7E24, &AtLensFlare::s_heap},
    };

    // No heaps: each raise. A demand-loaded item asks for the path on the heap of its database.
    for (const Heap& h : heaps) { *(void**)(uintptr_t)h.va = nullptr; *h.mine = nullptr; }
    run_text<Agn>(bytes("MatAssignment[1]\n{\n MatAssignment \"A\"[1]\n {\n \"M\"\n }\n}\n"), "no heap agn", 0xCD);
    run_text<Aga>(bytes("FrameNumber[2]\n{\n 1\n 5\n}\nMaterialKey[2]\n{\n \"M\" \"\"\n \"N\" \"\"\n}\nMatAnimation[1]\n{\n MatAnimation \"A\"\n {\n Keyframes 0 0 2\n }\n}\n"), "no heap aga", 0xCD);
    run_text<Agi>(bytes("MatAssignment[1]\n{\n \"M\"\n}\nImage[1]\n{\n Image \"A\"\n {\n DemandLoad\n }\n}\n"), "no heap agi", 0xCD);
    run_text<Agf>(bytes("Font[1]\nMatAssignment[1]\n{\n \"M\"\n}\nFont2D[1]\n{\n Font2D \"A\"\n {\n DemandLoad\n }\n}\n"), "no heap agf", 0xCD);
    run_text<Asl>(bytes("LensFlare[1]\n{\n LensFlare \"A\"\n {\n Flare[1]\n {\n Flare\n {\n }\n }\n }\n}\n"), "no heap asl", 0xCD);
    {
        g_context = "no heap: copy";
        g_fill = 0xCD;
        AtMatAssignment empty, co, cr;
        { On on(0); at<FnCopyList>(0x000349D0)(&co, nullptr, &empty); }
        { On on(1); cr.Copy(empty); }
        same_list("copy", co, cr);
        compare_logs("no heap: copy");
    }
    // And back, through each side's setter.
    at<FnSetHeap>(0x00034710)(&g_heap_token);
    AtMatAssignment::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x00027B60)(&g_heap_token);
    AtMatAssignmentDatabase::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x00034F30)(&g_heap_token);
    AtGrFrameNumberSet::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x00027EE0)(&g_heap_token);
    AtMatAnimationDatabase::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x00029B90)(&g_heap_token);
    AtImageDatabase::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x0002A010)(&g_heap_token);
    AtFontDatabase::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x0003F8E0)(&g_heap_token);
    AtLensFlareDatabase::SetHeap(&g_heap_token);
    at<FnSetHeap>(0x00052190)(&g_heap_token);
    AtLensFlare::SetHeap(&g_heap_token);
    g_context = "heaps set";
    for (const Heap& h : heaps) eq("heap", (uintptr_t)*(void**)(uintptr_t)h.va, (uintptr_t)*h.mine);

    // No room on a heap: the raise. Lists of zero, so that nothing is written through the null.
    {
        Env env({"M"}, {"Space"});
        const auto direct = [&](auto fo, auto fr, const char* text, const AtIoKeyword* table, unsigned count) {
            Direct da(text, table, count), db(text, table, count);
            g_fail_alloc = true;
            { On on(0); fo(&da.tokenizer); }
            { On on(1); fr(&db.tokenizer); }
            g_fail_alloc = false;
            compare_logs("no room");
        };
        g_fill = 0xCD;
        g_context = "no room: list";
        AtMatAssignment lo, lr;
        direct([&](AtIoTokenizer* t) { at<FnReadList>(0x000348D0)(&lo, nullptr, g_own[0], t, nullptr); }, [&](AtIoTokenizer* t) { lr.Read(g_own[1], t, nullptr); }, "[0]\n{\n}\n", AtMatAssignmentDatabase::s_keywords, 3);
        direct([&](AtIoTokenizer* t) { at<FnReadListNs>(0x000347C0)(&lo, nullptr, g_own[0], t); }, [&](AtIoTokenizer* t) { lr.ReadWithNameSpaces(g_own[1], t); }, "[0]\n{\n}\n", AtMatAssignmentDatabase::s_keywords, 3);
        AtMatAssignment empty;
        { g_fail_alloc = true; { On on(0); at<FnCopyList>(0x000349D0)(&lo, nullptr, &empty); } { On on(1); lr.Copy(empty); } g_fail_alloc = false; compare_logs("no room: copy"); }
    }
    {
        // Read directly on each concrete database, so that the failure is on its own list. The name map is asked for nothing at a count of zero only once Init has run; both sides do the same with the null it gets.
        typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
        Env env({"M"}, {"Space"});
        g_fill = 0xCD;
        g_context = "no room: agn";
        {
            AtMatAssignmentDatabase* o = Agn::make(0, 0xCD);
            AtMatAssignmentDatabase* r = Agn::make(1, 0xCD);
            const char* text = "MatAssignment[0]\n{\n}\n";
            Direct da(text, AtMatAssignmentDatabase::s_keywords, 3), db(text, AtMatAssignmentDatabase::s_keywords, 3);
            g_fail_alloc = true;
            const bool oc = guarded([&] { On on(0); at<FnRead>(0x00027B70)(o, nullptr, g_own[0], &da.tokenizer); });
            g_recording = false;
            const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crash", oc, rc);
            compare_logs("no room");
        }
        g_context = "no room: aga";
        for (const char* text : {"MaterialKey[0]\n{\n}\n", "MatAnimation[0]\n{\n}\n"}) {
            AtMatAnimationDatabase* o = Aga::make(0, 0xCD);
            AtMatAnimationDatabase* r = Aga::make(1, 0xCD);
            Direct da(text, AtMatAnimationDatabase::s_keywords, 8), db(text, AtMatAnimationDatabase::s_keywords, 8);
            g_fail_alloc = true;
            const bool oc = guarded([&] { On on(0); at<FnRead>(0x00027F90)(o, nullptr, g_own[0], &da.tokenizer); });
            g_recording = false;
            const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crash", oc, rc);
            compare_logs("no room");
        }
        g_context = "no room: asl";
        {
            AtLensFlareDatabase* o = Asl::make(0, 0xCD);
            AtLensFlareDatabase* r = Asl::make(1, 0xCD);
            const char* text = "LensFlare[0]\n{\n}\n";
            Direct da(text, AtLensFlareDatabase::s_keywords, 13), db(text, AtLensFlareDatabase::s_keywords, 13);
            g_fail_alloc = true;
            const bool oc = guarded([&] { On on(0); at<FnRead>(0x0003F910)(o, nullptr, g_scene[0], &da.tokenizer); });
            g_recording = false;
            const bool rc = guarded([&] { On on(1); r->Read(g_scene[1], &db.tokenizer); });
            g_recording = false;
            g_fail_alloc = false;
            eq("crash", oc, rc);
            compare_logs("no room");
        }
        g_context = "no room: flare";
        {
            AtLensFlare fo, fr;
            const char* text = "{\n Flare[0]\n {\n }\n}\n";
            Direct da(text, AtLensFlareDatabase::s_keywords, 13), db(text, AtLensFlareDatabase::s_keywords, 13);
            g_fail_alloc = true;
            { On on(0); at<FnRead>(0x00052970)(&fo, nullptr, g_scene[0], &da.tokenizer); }
            { On on(1); fr.Read(g_scene[1], &db.tokenizer); }
            g_fail_alloc = false;
            compare_logs("no room");
            fo.m_squares = fr.m_squares = nullptr;
        }
    }

    // A demand-loaded item's path, with no room for it. Its list and map have been made; only what is asked for after the item fails. The copy then writes through null on both sides.
    g_fail_in_item = true;
    run_broken<Agi>("Image[1]\n{\n Image \"A\"\n {\n DemandLoad\n }\n}\n");
    run_broken<Agf>("Font[1]\nFont2D[1]\n{\n Font2D \"A\"\n {\n DemandLoad\n }\n}\n");
    g_fail_in_item = false;

    // A file that will not open: the report, and then the reader on no tokenizer. In the game the report does not come back.
    for (int status : {1, 2, 5}) {
        g_fs->open_status = status;
        run_broken<Agn>("");
        run_broken<Aga>("");
        run_broken<Agi>("");
        run_broken<Agf>("");
        run_broken<Agl>("");
        run_broken<Asl>("");
        g_fs->open_status = 0;
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

// A body that reaches the end of its text loops on its report for ever, on either side: a mutant can bring that about, and the original's loop has no way out. Each case's progress restarts this.
static DWORD WINAPI watchdog(void*) {
    LONG last = -1;
    for (;;) {
        Sleep(60000);
        const LONG now = g_progress;
        if (now == last) {
            std::printf("HUNG during [%.300s]\n", g_context.c_str());
            ExitProcess(4);
        }
        last = now;
    }
}

static bool read_case(const std::string& path, std::vector<unsigned char>& data) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    data.resize((size_t)std::ftell(f));
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
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
    if (pos.size() != 2) { std::printf("usage: oracle_agn <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C63E0u, 0x002C6350u, 0x002C63E4u, 0x002C6354u, 0x002C6364u, 0x002C6370u, 0x002C6418u, 0x002C7E24u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtMatAssignment::s_heap = &g_heap_token;
    AtMatAssignmentDatabase::s_heap = &g_heap_token;
    AtGrFrameNumberSet::s_heap = &g_heap_token;
    AtMatAnimationDatabase::s_heap = &g_heap_token;
    AtImageDatabase::s_heap = &g_heap_token;
    AtFontDatabase::s_heap = &g_heap_token;
    AtLensFlareDatabase::s_heap = &g_heap_token;
    AtLensFlare::s_heap = &g_heap_token;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    same_keywords("agn keywords", 0x002B06B0, AtMatAssignmentDatabase::s_keywords, AtMatAssignmentDatabase::kKeywordCount);
    same_keywords("aga keywords", 0x002B06D8, AtMatAnimationDatabase::s_keywords, AtMatAnimationDatabase::kKeywordCount);
    same_keywords("agi keywords", 0x002B0838, AtImageDatabase::s_keywords, AtImageDatabase::kKeywordCount);
    same_keywords("agf keywords", 0x002B08E0, AtFontDatabase::s_keywords, AtFontDatabase::kKeywordCount);
    same_keywords("agl keywords", 0x002B0A48, AtLightDatabase::s_keywords, AtLightDatabase::kKeywordCount);
    same_keywords("asl keywords", 0x002B20A8, AtLensFlareDatabase::s_keywords, AtLensFlareDatabase::kKeywordCount);

    run_written();
    run_lists();
    run_sets();
    run_parts();
    run_failures();

    g_items_seen = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    static const char* const kNames[6] = {"AGN", "AGA", "AGI", "AGF", "AGL", "ASL"};
    while (std::fgets(line, sizeof line, index)) {
        int kind = -1;
        if (std::strstr(line, Agn::kExt)) kind = 0;
        else if (std::strstr(line, Aga::kExt)) kind = 1;
        else if (std::strstr(line, Agi::kExt)) kind = 2;
        else if (std::strstr(line, Agf::kExt)) kind = 3;
        else if (std::strstr(line, Agl::kExt)) kind = 4;
        else if (std::strstr(line, Asl::kExt)) kind = 5;
        if (kind < 0) continue;
        *std::strchr(line, '\t') = 0;
        std::vector<unsigned char> data;
        if (!read_case(dir + "\\" + line, data)) { std::printf("cannot read %s\n", line); return 2; }
        const std::string context = std::string(kNames[kind]) + " " + line;
        switch (kind) {
        case 0: run_text<Agn>(data, context, 0xCD); break;
        case 1: run_text<Aga>(data, context, 0xCD); break;
        case 2: run_text<Agi>(data, context, 0xCD); break;
        case 3: run_text<Agf>(data, context, 0xCD); break;
        case 4: run_text<Agl>(data, context, 0xCD); break;
        case 5: run_text<Asl>(data, context, 0xCD); break;
        }
        g_files[kind]++;
    }
    std::fclose(index);
    report_entries();
    std::printf("loads that crashed the same on both sides: %u\n", g_crashes_seen);
    std::printf("%s: files AGN %u, AGA %u, AGI %u, AGF %u, AGL %u, ASL %u, %u items; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files[0], g_files[1], g_files[2], g_files[3], g_files[4], g_files[5], g_items_seen, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
