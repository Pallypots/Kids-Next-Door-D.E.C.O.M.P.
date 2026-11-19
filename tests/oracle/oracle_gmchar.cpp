// Differential oracle for GmAcObject, GmAcCharacter and the design record reader (Windows x86, local only). See docs/verification.md.
//
//   oracle_gmchar <default.xbe> [cases dir] [--entries FILE] [--mutate]
//
// No file reaches this code: nothing that makes actors from data is recovered. So the cases are objects built by each side's own constructor (the original's at its address, the recovered one by
// new), put into chosen states by writing the same field values on both sides, and then called directly; return values, every word of the object, what it points at, and the log of calls compared.
//
// What stands in, and is the same on both sides:
//  * The design registry's search (0x000679E0, a seam) and the values it finds: one double answers both, from a table each case fills with which words are in the object's record, which in the
//    defaults, and their values. A value's slot 5 (ElementAt) is a logging fake. So the reader is tested against the search's interface, not against real .ADR data: the search itself and the value
//    classes' element access are not recovered.
//  * Every other seam (0x0014D590, 0x0014C1B0, 0x001553C0 and the interaction's slots) and the actor's and node's slots that are not recovered where these call them directly (actor 4, 10, 15, 20, 49;
//    node 32, 33): the original's address and the recovered function, both to one double that logs.
//  * The shadow, the sight cone, the controller, the scene node and scene actor: fakes, one per side where written. The scene device's material assignment tables and the joints' table are real
//    recovered name maps, one shared by both sides; the original's code reads them on one side and the recovered code on the other. So do "Shadow" in the actor's name map, built by each side's code.
//  * The C library's sprintf (0x00125559) and the host's AtFormat: one double that logs its arguments, since what it writes is never read.
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtCoordSys.h"
#include "at/AtScene.h"
#include "at/AtSceneActor.h"
#include "gm/GmAcCharacter.h"
#include "gm/GmLevelManifest.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static int g_crashed_alike;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 200).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 30) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 200).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

// The manifest loader is linked in for statics this code reads; its clock is not used here.
unsigned int AtPlatformMilliseconds() { return 0; }

int AtFormat(char* out, const char* format, ...);  // src/at/AtIoFormatHost.cpp

static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
static void** vptr(void* obj) { return *(void***)obj; }

// thiscall through __fastcall: ecx = this, edx unused, the same stack arguments and cleanup.
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnI0)(void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnP2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void*(__cdecl* FnCreate)(int);

// ---- canonical words ---------------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_vft_token;   // original vftable -> token
static std::map<uint32_t, uint32_t> g_host_vft;    // host vftable -> token
static const uint32_t kOrigVfts[] = {0x00285828, 0x0028E850, 0x0028E638, 0x0028E4C8, 0x0028EF44, 0x0028EF50, 0x0028E30C, 0x0028E2F4, 0x0028E308, 0x002845C0, 0x0027C1D8,
                                     0x00278568, 0x0027855C, 0x00278418, 0x0027BCC4, 0x0028E33C, 0x0028C890, 0x0028C980};
struct Twin { uint32_t p[2]; uint32_t size; uint32_t id; };
static std::vector<Twin> g_twins;
static void twin(const void* a, const void* b, uint32_t size, uint32_t id) { g_twins.push_back({{U(a), U(b)}, size, id}); }
// What each side makes during a call (interactions, shadows), in order: the k-th of one side pairs with the k-th of the other.
struct Made { uint32_t p; uint32_t size; };
static Made g_made[2][16];
static int g_made_n[2];
static void made(const void* p, uint32_t size) { if (g_made_n[g_side] < 16) g_made[g_side][g_made_n[g_side]++] = {U(p), size}; }
static void clear_made() { g_made_n[0] = g_made_n[1] = 0; }

static uint32_t canon(int side, uint32_t w, const void* base, uint32_t size) {
    if (base && w >= U(base) && w < U(base) + size) return 0xA0000000u | (w - U(base));
    const uint32_t pool0 = 0x002E26F0, pool1 = U(GmLinkedListNode::s_pool), poolSize = 0x20 * GmLinkedListNode::kPoolSize;
    if (side == 0 && w >= pool0 && w < pool0 + poolSize) return 0xB0000000u | (w - pool0);
    if (side == 1 && w >= pool1 && w < pool1 + poolSize) return 0xB0000000u | (w - pool1);
    if (side == 0) {
        auto it = g_vft_token.find(w);
        if (it != g_vft_token.end()) return it->second;
    } else {
        auto it = g_host_vft.find(w);
        if (it != g_host_vft.end()) return it->second;
    }
    for (const Twin& t : g_twins) {
        if (w >= t.p[side] && w < t.p[side] + t.size) return 0xD0000000u | (t.id << 12) | (w - t.p[side]);
    }
    for (int k = 0; k < g_made_n[side]; k++) {
        if (w >= g_made[side][k].p && w < g_made[side][k].p + g_made[side][k].size) return 0xE0000000u | (k << 12) | (w - g_made[side][k].p);
    }
    return w;
}

static void same_words(const char* what, const void* o, const void* r, uint32_t size, const void* obase = nullptr, const void* rbase = nullptr, uint32_t bsize = 0) {
    if (!obase) { obase = o; rbase = r; bsize = size; }
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    for (uint32_t i = 0; i < size / 4; i++) {
        auto it = g_vft_token.find(a[i]);
        if (it != g_vft_token.end() && g_host_vft.find(b[i]) == g_host_vft.end()) {
            bool taken = false;
            for (auto& kv : g_host_vft) taken = taken || kv.second == it->second;
            if (!taken) g_host_vft[b[i]] = it->second;
        }
        const uint32_t ca = canon(0, a[i], obase, bsize), cb = canon(1, b[i], rbase, bsize);
        if (ca != cb) {
            char text[96];
            sprintf_s(text, "%s word +0x%X", what, i * 4);
            eq(text, ca, cb);
            return;
        }
        g_checks++;
    }
}

// ---- logging ---------------------------------------------------------------------------------------------------------------------
static uint32_t hash_name(const char* s) {
    if (!s) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; i < 8 && s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static uint32_t hash_str(const char* s) {
    if (!s) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void log_(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static uint32_t C(const void* p) { return canon(g_side, U(p), nullptr, 0); }

// ---- fakes -------------------------------------------------------------------------------------------------------------------------
struct Fake {
    void** vptr;
    uint32_t words[0x40];
    uint32_t id;
    int ret[64];
};
template <int S> static int __fastcall fk0(Fake* f, void*) { log_('V' << 8 | S, C(f)); return f->ret[S]; }
template <int S> static int __fastcall fk1(Fake* f, void*, uintptr_t a) { log_('V' << 8 | S, C(f)); log_('a', C((void*)a)); return f->ret[S]; }
template <int... S> struct Fill0 { static void in(void** vt) { (void)std::initializer_list<int>{(vt[S] = (void*)&fk0<S>, 0)...}; } };
template <int... S> struct Fill1 { static void in(void** vt) { (void)std::initializer_list<int>{(vt[S] = (void*)&fk1<S>, 0)...}; } };
static void* g_vt_shadow[16];
static void* g_vt_cone[8];
static void* g_vt_holder[4];   // the stats holder at +0x270: only deleted, by the actor's shutdown
static Fake* new_fake(void** vt, uint32_t id) {
    Fake* f = (Fake*)std::calloc(1, sizeof(Fake));
    f->vptr = vt;
    f->id = id;
    return f;
}
// The shadow's slot 7 is given a pointer to a structure on the stack whose first word is the character: log that word.
static void __fastcall shadow_configure(Fake* f, void*, void** setup) { log_('V' << 8 | 7, C(f)); log_('a', C(setup[0])); }

// The interaction: made by each side's new, in 0x1DC bytes; its constructor is a seam, whose double gives it a vftable of logging slots.
static void* g_vt_inter[6];
template <int S> static void __fastcall inter0(void* p, void*) { log_('I' << 8 | S, C(p)); }
template <int S> static void* __fastcall inter1(void* p, void*, uintptr_t a) { log_('I' << 8 | S, C(p)); log_('i', C((void*)a)); return p; }
static void* __fastcall d_inter_ctor(void* p, void*) {
    made(p, 0x1DC);
    log_('I' << 8 | 0xC, C(p));
    *(void***)p = g_vt_inter;
    return p;
}

static void init_fake_vtables() {
    Fill1<0, 12, 13>::in(g_vt_shadow);
    Fill0<1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 14, 15>::in(g_vt_shadow);
    g_vt_shadow[7] = (void*)shadow_configure;
    Fill1<0, 6>::in(g_vt_cone);
    Fill0<1, 2, 3, 4, 5, 7>::in(g_vt_cone);
    Fill1<0>::in(g_vt_holder);
    Fill0<1, 2, 3>::in(g_vt_holder);
    g_vt_inter[0] = (void*)inter1<0>;
    g_vt_inter[1] = (void*)inter1<1>;
    g_vt_inter[2] = (void*)inter0<2>;
    g_vt_inter[3] = (void*)inter0<3>;
    g_vt_inter[4] = (void*)inter0<4>;
    g_vt_inter[5] = (void*)inter1<5>;
}

// ---- the design registry and its values ------------------------------------------------------------------------------------------
// A value: what the registry's search finds is a link whose +0x04 is this; its slot 5 hands back the element.
struct Val {
    void** vptr;
    uint32_t id;
    void* elem;
};
static void* g_vt_val[8];
static void* __fastcall val_element(Val* v, void*, unsigned index) { log_('E' << 8, v->id); log_('e', index); return v->elem; }
struct LinkF {
    void* vptr;
    Val* value;
};

enum Kind { kReal, kInt, kBool, kCount, kMat, kNs, kBone, kShadow };
struct Word {
    const char* name;
    Kind kind;
    int where;          // 1: in the object's record, 2: in the defaults, 3: both, 0: neither
    uint32_t own[4];    // values, or text
    uint32_t def[4];
    Val ownVal, defVal;
    LinkF ownLink, defLink;
};
static Word g_words[] = {
    {"rUpdateDistance", kReal}, {"rCriticalUpdateDistanceSquared", kReal}, {"rHighUpdateDistanceSquared", kReal}, {"rMediumUpdateDistanceSquared", kReal},
    {"nMinUpdateLevel", kInt}, {"nMaxUpdateLevel", kInt}, {"nCulledUpdateLevel", kInt}, {"nCollisionGroups", kInt}, {"nCollisionTestGroups", kInt},
    {"bIgnoreAnimationBoundingSpheres", kBool}, {"bCopyMaterials", kBool},
    {"NormalMatAssignment", kMat}, {"NormalMatAssignmentNameSpace", kNs}, {"BlendMatAssignment", kMat}, {"BlendMatAssignmentNameSpace", kNs},
    {"ZOnlyMatAssignment", kMat}, {"ZOnlyMatAssignmentNameSpace", kNs},
    {"TYPE", kInt}, {"Flags", kInt}, {"Mass", kReal}, {"NearRange", kReal}, {"AllowEventsWhenDisabled", kBool}, {"UpdateActorWhenDrawDisabled", kBool},
    {"UpdateChartWhenDrawDisabled", kBool}, {"RicochetShotFlags", kInt}, {"UVAnimationDefaultVelocity", kReal}, {"NumEmitters", kInt}, {"Num2DEmitters", kInt},
    {"MaxDistanceOverride", kInt}, {"ConserveVfx", kBool}, {"VfxWaitingListSize", kCount}, {"RecyclableWhileActive", kInt}, {"SendOnScreenEvents", kBool},
    {"SendOOEEvents", kBool}, {"WorldCollisionHeight", kReal},
    {"AllowInteraction", kBool},
    {"SightConeLength", kReal}, {"SightConeYaw", kReal}, {"SightConePitch", kReal}, {"SightConeMinDistance", kReal}, {"SightConeAttachToBone", kBone}, {"ShadowType", kShadow},
};
static const int kWords = sizeof g_words / sizeof g_words[0];

static void* const kRegistry = (void*)0x5EC00000;  // the object's registry: a token, only handed on
static LinkF g_record;                              // the object's record
static GmDesignDefaults g_defaults;                 // 0x003B3524
static LinkF g_defaultRecord;
static int g_find_parents_seen;

static AtDataLink* find_impl(void* registry, void* under, const char* path, int parents) {
    log_('F' << 8, C(registry));
    log_('f', C(under));
    log_('f', hash_str(path));
    log_('f', parents);
    const int table = under == &g_defaultRecord ? 2 : 1;
    for (Word& w : g_words) {
        if (std::strcmp(w.name, path) == 0 && (w.where & table)) return (AtDataLink*)(table == 2 ? &w.defLink : &w.ownLink);
    }
    return nullptr;
}
static AtDataLink* __fastcall o_find(void* registry, void*, void* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }

static const char* const kMats[] = {"matA", "matB", "matC", "nomat"};
static const char* const kSpaces[] = {"nsX", "", "nsY"};
static const char* const kBones[] = {"bone1", "bone2", "nobone"};
static const char* const kShadows[] = {"shad1", "shad0", "noshad"};

static uint32_t g_rand;
static uint32_t rnd() { g_rand = g_rand * 1103515245u + 12345u; return (g_rand >> 16) & 0x7FFF; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

static void fill_value(Kind kind, uint32_t* out) {
    std::memset(out, 0, 16);
    static const float reals[] = {0.0f, 1.5f, -2.0f, 1000.0f};
    static const int ints[] = {0, 1, 2, 7, -1, 0x12345};
    static const unsigned counts[] = {0, 1, 2, 4};
    const char* text = nullptr;
    switch (kind) {
    case kReal: out[0] = fbits(reals[rnd() % 4]); break;
    case kInt: out[0] = (uint32_t)ints[rnd() % 6]; break;
    case kBool: out[0] = rnd() % 3 == 0 ? 0 : (rnd() % 2 ? 1 : 0x100); break;
    case kCount: out[0] = counts[rnd() % 4]; break;
    case kMat: text = kMats[rnd() % 4]; break;
    case kNs: text = kSpaces[rnd() % 3]; break;
    case kBone: text = kBones[rnd() % 3]; break;
    case kShadow: text = kShadows[rnd() % 3]; break;
    }
    if (text) std::memcpy(out, text, std::strlen(text));
}

// Each word in neither, the record, the defaults or both, with a value; `density` in 0..3 makes words more likely present.
static void fill_words(uint32_t seed, int density) {
    g_rand = seed * 2654435761u + 1;
    for (Word& w : g_words) {
        const uint32_t r = rnd() % 8;
        w.where = r < (uint32_t)(4 - density) ? 0 : (int)(1 + rnd() % 3);
        fill_value(w.kind, w.own);
        fill_value(w.kind, w.def);
    }
}
static Word* word(const char* name) {
    for (Word& w : g_words) if (!std::strcmp(w.name, name)) return &w;
    return nullptr;
}
static void set_word(const char* name, int where, uint32_t value) {
    Word* w = word(name);
    w->where = where;
    std::memset(w->own, 0, 16);
    std::memset(w->def, 0, 16);
    w->own[0] = w->def[0] = value;
}
static void set_text(const char* name, int where, const char* text) {
    Word* w = word(name);
    w->where = where;
    std::memset(w->own, 0, 16);
    std::memset(w->def, 0, 16);
    std::memcpy(w->own, text, std::strlen(text));
    std::memcpy(w->def, text, std::strlen(text));
}
static void clear_words() { for (Word& w : g_words) w.where = 0; }

static void init_registry() {
    g_vt_val[5] = (void*)val_element;
    uint32_t id = 1;
    for (Word& w : g_words) {
        w.ownVal = {g_vt_val, id++, w.own};
        w.defVal = {g_vt_val, id++, w.def};
        w.ownLink = {nullptr, &w.ownVal};
        w.defLink = {nullptr, &w.defVal};
    }
    g_defaults.m_registry = (AtDataRegistry*)0x0DEF0000;
    g_defaults.m_link = (AtDataLink*)&g_defaultRecord;
    *(void**)(uintptr_t)0x003B3524 = &g_defaults;
    GmDesignDefaults::s_instance = &g_defaults;
}

// ---- the doubles for the seams and the slots not recovered ---------------------------------------------------------------------
static int g_visible;
static void __fastcall d_apply(void* node, void*, int which) { log_('M', C(node)); log_('m', which); }
static int __fastcall d_visible(void* node, void*) { log_('W', C(node)); return g_visible; }
static void __fastcall d_actor4(void* p, void*) { log_('Q' << 8 | 4, C(p)); }
static void __fastcall d_actor10(void* p, void*) { log_('Q' << 8 | 10, C(p)); }
static void __fastcall d_actor15(void* p, void*) { log_('Q' << 8 | 15, C(p)); }
static void __fastcall d_actor20(void* p, void*, int a) { log_('Q' << 8 | 20, C(p)); log_('q', a); }
static void __fastcall d_actor49(void* p, void*, const char* event, int b, int c) { log_('Q' << 8 | 49, C(p)); log_('q', hash_name(event)); log_('q', b); log_('q', c); }
static void __fastcall d_node32(void* p, void*) { log_('Q' << 8 | 32, C(p)); }
static void __fastcall d_node33(void* p, void*) { log_('Q' << 8 | 33, C(p)); }
static int __cdecl d_format(char* out, const char* format, const char* a, const char* b) {
    log_('S', hash_str(format));
    log_('s', hash_name(a));
    log_('s', hash_name(b));
    out[0] = 0;
    return 0;
}
// The shadow's create function, as a registration in the "Shadow" map holds it: each side gets its own fake.
static int g_shadow_ret[64];
static void* __cdecl d_create_shadow(int count) {
    Fake* f = new_fake(g_vt_shadow, 80);
    std::memcpy(f->ret, g_shadow_ret, sizeof f->ret);
    made(f, sizeof(Fake));
    log_('K', count);
    return f;
}

template <class M> static void* member_address(M m) { void* p; std::memcpy(&p, &m, sizeof p); return p; }
static void* call_target(const void* fn) {
    const unsigned char* p = (const unsigned char*)fn;
    for (int i = 0; i < 64; i++) {
        if (p[i] == 0xE8 || p[i] == 0xE9) return (void*)(p + i + 5 + *(const int32_t*)(p + i + 1));
    }
    return nullptr;
}
namespace {
struct Placement {};
}  // namespace
// No matching placement delete, on purpose: with one, construct_interaction gets an exception frame and call_target no longer finds the call to the constructor.
#pragma warning(disable : 4291)
inline void* operator new(std::size_t, void* where, Placement) { return where; }
__declspec(noinline) static void construct_interaction(void* p) { new (p, Placement()) GmAcInteraction; }

// ---- both sides of one call --------------------------------------------------------------------------------------------------------
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    const bool oc = guarded([&] { On on(0); fo(); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    g_recording = false;
    g_side = 0;
    eq((std::string(what) + ": crashed").c_str(), oc, rc);
    if (oc && rc) {
        g_crashed_alike++;
        if (getenv("GMCHAR_TRACE")) std::printf("crashed alike: %s [%s]\n", what, g_context.c_str());
    }
    compare_logs(what);
    return oc || rc;
}

static void* fresh(uint32_t size) {
    void* p = std::calloc(1, size + 16);
    std::memset(p, 0xCD, size + 16);
    return p;
}

static void pair_host(uint32_t orig, const void* host_obj, uint32_t offset = 0) {
    g_host_vft[*(const uint32_t*)((const char*)host_obj + offset)] = g_vft_token[orig];
}

static void pair_vftables() {
    uint32_t token = 0xC0000000u;
    for (uint32_t v : kOrigVfts) g_vft_token[v] = token++;
    AtEvSubscriber* e = new AtEvSubscriber; pair_host(0x00285828, e);
    GmBaObject* b = new GmBaObject; pair_host(0x0028E850, b);
    GmAcNode* n = new GmAcNode; pair_host(0x0028E638, n);
    GmAcActor* a = new GmAcActor; pair_host(0x0028E4C8, a);
    pair_host(0x0028EF50, &a->m_animation);
    pair_host(0x0028E30C, &a->m_vectors184);
    pair_host(0x0028E2F4, &a->m_items);
    pair_host(0x0028E308, &a->m_items, 0x10);
    pair_host(0x0027C1D8, &a->m_callback);
    AtKiAnimationCallBack* k = new AtKiAnimationCallBack; pair_host(0x002845C0, k);
    GmLinkedList* l = new GmLinkedList; pair_host(0x00278568, l);
    GmPlug* p = new GmPlug; pair_host(0x0027855C, p);
    GmLinkedListIteratorOf<GmAcLinkedItem*>* it = new GmLinkedListIteratorOf<GmAcLinkedItem*>(l); pair_host(0x0028E33C, it);
    GmLinkedListIterator* it2 = new GmLinkedListIterator(l); pair_host(0x0027BCC4, it2);
    GmAcObject* o = new GmAcObject; pair_host(0x0028C890, o);
    GmAcCharacter* c = new GmAcCharacter; pair_host(0x0028C980, c);
}

// ---- objects of both sides ---------------------------------------------------------------------------------------------------------
enum ObjKind { kNode, kActor, kObject, kCharacter };
static const uint32_t kSizes[] = {0x14C, 0x2D8, 0x2DC, 0x304};
static const uint32_t kCtors[] = {0x0014CE40, 0x0014B5D0, 0x00130AB0, 0x00130F80};

// Per side: the scene node, the scene actor at +0xA0 (a jointed actor's joints at its +0xA0), the controller, the list of characters with shadows.
static GmNameMap* g_joints;
static GmNameMap* g_shadow_map;
struct Pair {
    unsigned char* o;
    unsigned char* r;
    uint32_t size;
    unsigned char* scene[2];
    unsigned char* actorA0[2];
    GmAcCharController* controller[2];
    explicit Pair(ObjKind kind) {
        size = kSizes[kind];
        o = (unsigned char*)fresh(size);
        r = (unsigned char*)fresh(size);
        at<FnP0>(kCtors[kind])(o, nullptr);
        switch (kind) {
        case kNode: new (r) GmAcNode; break;
        case kActor: new (r) GmAcActor; break;
        case kObject: new (r) GmAcObject; break;
        case kCharacter: new (r) GmAcCharacter; break;
        }
        clear_made();
        twin(o, r, size, 1);
        for (int s = 0; s < 2; s++) {
            scene[s] = (unsigned char*)fresh(0x70);
            actorA0[s] = (unsigned char*)fresh(0xD0);
            *(GmNameMap**)(actorA0[s] + 0xA0) = g_joints;
            controller[s] = (GmAcCharController*)std::calloc(1, sizeof(GmAcCharController));
            controller[s]->m_unknown04 = 0x5A;
            controller[s]->m_owner = (GmAcActor*)0x0B0B;
        }
        twin(scene[0], scene[1], 0x70, 2);
        twin(actorA0[0], actorA0[1], 0xD0, 3);
        twin(controller[0], controller[1], sizeof(GmAcCharController), 4);
    }
    GmAcCharacter* R() { return (GmAcCharacter*)r; }
    template <class T> void set(uint32_t offset, T value) { *(T*)(o + offset) = value; *(T*)(r + offset) = value; }
    template <class T> T get(int side, uint32_t offset) { return *(T*)((side ? r : o) + offset); }
    void record(bool registry = true, bool descriptor = true) {
        set<void*>(0x10, registry ? kRegistry : nullptr);
        set<void*>(0x14, descriptor ? (void*)&g_record : nullptr);
    }
    void scene_parts(bool node, bool a0) {
        *(void**)(o + 0x9C) = node ? scene[0] : nullptr;
        *(void**)(r + 0x9C) = node ? scene[1] : nullptr;
        *(void**)(o + 0xA0) = a0 ? actorA0[0] : nullptr;
        *(void**)(r + 0xA0) = a0 ? actorA0[1] : nullptr;
    }
    // Records each side allocated in the call are twinned so that they compare, then compared.
    void compare(const char* what) {
        GmAcActorRecord* ro = size >= 0x2D8 ? get<GmAcActorRecord*>(0, 0x234) : nullptr;
        GmAcActorRecord* rr = size >= 0x2D8 ? get<GmAcActorRecord*>(1, 0x234) : nullptr;
        uint32_t count = 0;
        if (ro && rr && ro != rr) {
            unsigned int* n = get<unsigned int*>(0, 0x238);
            count = n ? *n : 3;
            twin(ro, rr, count * 0x44, 9);
        }
        same_words(what, o, r, size);
        if (count) same_words("records", ro, rr, count * 0x44);
        same_words("scene node", scene[0], scene[1], 0x70);
        same_words("scene actor", actorA0[0], actorA0[1], 0xD0);
        same_words("controller", controller[0], controller[1], sizeof(GmAcCharController));
    }
    ~Pair() { g_twins.clear(); clear_made(); }
};

static int g_cases;
static int g_heap_token;
static void set_heap(bool on) {
    *(void**)(uintptr_t)0x003B3930 = on ? &g_heap_token : nullptr;
    GmAcActor::s_heap = on ? &g_heap_token : nullptr;
}

// ---- the readers alone -------------------------------------------------------------------------------------------------------------
static void run_find() {
    static const char* const names[] = {"TYPE", "Flags", "", "Mass"};
    for (int k = 0; k < 64; k++) {
        g_context = "find " + std::to_string(k);
        Pair p(kNode);
        clear_words();
        set_word("TYPE", k & 3, 0x11);
        word("TYPE")->def[0] = 0x22;
        set_word("Flags", (k >> 2) & 3, 0x33);
        p.record((k & 16) == 0, (k & 32) == 0);
        for (const char* name : names) {
            uint32_t ro = 0, rr = 0;
            const std::string what = std::string("[") + name + "]";
            both(("find value " + what).c_str(), [&] { ro = U(at<FnP2>(0x0014E590)(p.o, nullptr, (uintptr_t)name, 7)); }, [&] { rr = U(((GmBaObject*)p.r)->FindValue(name, 7)); });
            eq(("find value " + what).c_str(), ro, rr);
            both(("or default " + what).c_str(), [&] { ro = U(at<FnP2>(0x0014CAA0)(p.o, nullptr, (uintptr_t)name, 7)); }, [&] { rr = U(((GmAcNode*)p.r)->FindValueOrDefault(name, 7)); });
            eq(("or default " + what).c_str(), ro, rr);
        }
        p.compare("after");
        g_cases++;
    }
}

// The node's slot 2 on a node, with every word in turn absent, in the record, in the defaults or in both.
static void run_node_record() {
    for (int k = 0; k < 400; k++) {
        g_context = "node record " + std::to_string(k);
        Pair p(k & 1 ? kNode : kActor);
        fill_words(k, k % 4);
        p.record(true, k % 23 != 0);
        p.scene_parts(true, k % 3 != 0);
        p.set<uint32_t>(0xA4, (k & 2) ? 0x100 : 0x41);
        for (int s = 0; s < 2; s++) {
            *(uint32_t*)(p.scene[s] + 4) = 0x10 + k;
            *(uint32_t*)(p.actorA0[s] + 4) = 0x20 + k;
            *(int*)(p.scene[s] + 0x60) = -1;
            *(int*)(p.scene[s] + 0x64) = -1;
        }
        uint32_t ro = 0, rr = 0;
        both("node slot 2", [&] { ro = at<FnI0>(0x0014D900)(p.o, nullptr); }, [&] { rr = p.R()->GmAcNode::Slot2(); });
        eq("node slot 2", ro, rr);
        p.compare("after");
        g_cases++;
    }
}

// The actor's slot 2: the actor's words, the records, then the node's.
static void run_actor_record() {
    static GmAcActorRecord held[3];
    for (int k = 0; k < 400; k++) {
        g_context = "actor record " + std::to_string(k);
        Pair p(kActor);
        fill_words(1000 + k, k % 4);
        if (k % 5 == 0) { set_word("ConserveVfx", 1 + k % 3, 1); }
        if (k % 7 == 0) { set_word("VfxWaitingListSize", 1 + k % 3, k % 3); }
        p.record(k % 31 != 0, k % 29 != 0);
        p.scene_parts(true, true);
        p.set<uint32_t>(0x204, 0x204);
        p.set<uint32_t>(0x15C, 0x15C);
        if (k % 11 == 0) p.set<GmAcActorRecord*>(0x234, held);
        set_heap(k % 13 != 0);
        uint32_t ro = 0, rr = 0;
        both("actor slot 2",
             [&] { g_fail_alloc = k % 17 == 0; ro = at<FnI0>(0x0014ACF0)(p.o, nullptr); g_fail_alloc = false; },
             [&] { g_fail_alloc = k % 17 == 0; rr = p.R()->GmAcActor::Slot2(); g_fail_alloc = false; });
        eq("actor slot 2", ro, rr);
        set_heap(true);
        p.compare("after");
        g_cases++;
    }
}

// The records alone: what decides whether there are any, and how many.
static void run_records() {
    static int values[] = {0, 1, 5};
    static unsigned counts[] = {0, 1, 3, 4};
    static GmAcActorRecord held[3];
    for (int k = 0; k < 4 * 5 * 2 * 3; k++) {
        const int conserve = k % 4, count = (k / 4) % 5, already = (k / 20) % 2, trouble = (k / 40) % 3;
        g_context = "records conserve " + std::to_string(conserve) + " count " + std::to_string(count) + " already " + std::to_string(already) + " trouble " + std::to_string(trouble);
        Pair p(kActor);
        p.set<int*>(0x23C, conserve ? &values[conserve - 1] : nullptr);
        p.set<unsigned int*>(0x238, count ? &counts[count - 1] : nullptr);
        p.set<GmAcActorRecord*>(0x234, already ? held : nullptr);
        set_heap(trouble != 1);
        both("allocate records",
             [&] { g_fail_alloc = trouble == 2; at<Fn0>(0x0014A7D0)(p.o, nullptr); g_fail_alloc = false; },
             [&] { g_fail_alloc = trouble == 2; p.R()->AllocateRecords(); g_fail_alloc = false; });
        set_heap(true);
        p.compare("after");
        g_cases++;
    }
    for (int k = 0; k < 2; k++) {
        g_context = "record constructor";
        void* o = fresh(0x44);
        void* r = fresh(0x44);
        both("construct", [&] { at<FnP0>(0x001480C0)(o, nullptr); }, [&] { new (r, Placement()) GmAcActorRecord; });
        same_words("record", o, r, 0x44);
        g_cases++;
    }
}

// ---- GmAcObject ----------------------------------------------------------------------------------------------------------------------
static void run_lifetimes() {
    for (int kind : {kObject, kCharacter}) {
        for (int how = 0; how < 6; how++) {
            g_context = std::string(kind == kObject ? "object" : "character") + " lifetime " + std::to_string(how);
            const uint32_t size = kSizes[kind];
            void* o = fresh(size);
            void* r = fresh(size);
            both("construct", [&] { at<FnP0>(kCtors[kind])(o, nullptr); }, [&] { if (kind == kObject) new (r) GmAcObject; else new (r) GmAcCharacter; });
            same_words("constructed", o, r, size);
            twin(o, r, size, 1);
            if (how & 2) {
                both("initialize", [&] { ((FnI0)vptr(o)[1])(o, nullptr); }, [&] { ((FnI0)vptr(r)[1])(r, nullptr); });
                *(uint32_t*)((char*)o + 0x30) = *(uint32_t*)((char*)r + 0x30) = 0x30;
            }
            if (how & 4) {
                *(uint32_t*)((char*)o + 0x98) = *(uint32_t*)((char*)r + 0x98) = 0;
                for (uint32_t off = 0x2D8; off < size; off += 4) {
                    if (off == 0x2D8 || off == 0x2DC || off == 0x2E0 || off == 0x2E4) continue;  // pointers the destructor follows
                    *(uint32_t*)((char*)o + off) = *(uint32_t*)((char*)r + off) = 0x1000 + off;
                }
            }
            if (how & 1) {
                both("delete", [&] { ((FnDel)vptr(o)[0])(o, nullptr, 1); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, 1); });
            } else {
                const uint32_t dtor = kind == kObject ? 0x00130B00 : 0x00131210;
                both("destroy", [&] { at<Fn0>(dtor)(o, nullptr); }, [&] { if (kind == kObject) ((GmAcObject*)r)->GmAcObject::~GmAcObject(); else ((GmAcCharacter*)r)->GmAcCharacter::~GmAcCharacter(); });
                same_words("destroyed", o, r, size);
                void* o2 = fresh(size);
                void* r2 = fresh(size);
                at<FnP0>(kCtors[kind])(o2, nullptr);
                if (kind == kObject) new (r2) GmAcObject; else new (r2) GmAcCharacter;
                g_twins.clear();
                twin(o2, r2, size, 1);
                both("deleting, flag 0", [&] { ((FnDel)vptr(o2)[0])(o2, nullptr, 0); }, [&] { ((FnDel)vptr(r2)[0])(r2, nullptr, 0); });
                same_words("deleted, flag 0", o2, r2, size);
            }
            g_twins.clear();
            g_cases++;
        }
    }
    // Reset on a dirty object.
    g_context = "object reset";
    Pair p(kObject);
    std::memset(p.o + 0x14C, 0x5A, 0x2DC - 0x14C);
    std::memset(p.r + 0x14C, 0x5A, 0x2DC - 0x14C);
    p.set<uint32_t>(0x98, 0x12345678);
    both("reset", [&] { at<Fn0>(0x0015B810)(p.o, nullptr); }, [&] { ((GmAcObject*)p.r)->Reset(); });
    p.compare("after reset");
    g_cases++;
}

// Gives each side an interaction already made: each side's own 0x1DC bytes with the logging vftable.
static void give_interaction(Pair& p) {
    void* i[2];
    for (int s = 0; s < 2; s++) {
        i[s] = fresh(0x1DC);
        *(void***)i[s] = g_vt_inter;
        *(void**)((s ? p.r : p.o) + 0x2D8) = i[s];
    }
    twin(i[0], i[1], 0x1DC, 5);
}

static void run_object() {
    // Slot 2: the actor's record, then AllowInteraction.
    for (int k = 0; k < 160; k++) {
        g_context = "object slot 2 " + std::to_string(k);
        Pair p(kObject);
        fill_words(5000 + k, 2 + k % 2);
        set_word("AllowInteraction", k % 4, (k & 4) ? 0 : (k & 8) ? 1 : 0x55);
        p.record(true, k % 19 != 0);
        p.scene_parts(true, true);
        if (k % 9 == 0) give_interaction(p);
        const int trouble = k % 10 == 3 ? 1 : k % 10 == 7 ? 2 : 0;
        set_heap(trouble != 1);
        uint32_t ro = 0, rr = 0;
        both("object slot 2",
             [&] { g_fail_alloc = trouble == 2; ro = ((FnI0)vptr(p.o)[2])(p.o, nullptr); g_fail_alloc = false; },
             [&] { g_fail_alloc = trouble == 2; rr = ((FnI0)vptr(p.r)[2])(p.r, nullptr); g_fail_alloc = false; });
        eq("object slot 2", ro, rr);
        set_heap(true);
        p.compare("after");
        g_cases++;
    }
    // Shutdown, slots 10 and 20, by state.
    for (int k = 0; k < 16; k++) {
        g_context = "object shutdown and slots " + std::to_string(k);
        for (int which = 0; which < 4; which++) {
            Pair p(kObject);
            if (k & 1) { both("initialize", [&] { ((FnI0)vptr(p.o)[1])(p.o, nullptr); }, [&] { ((FnI0)vptr(p.r)[1])(p.r, nullptr); }); }
            if (k & 2) give_interaction(p);
            p.set<uint32_t>(0xA4, (k & 4) ? 2 : 0);
            p.set<uint32_t>(0x30, 0x30);
            p.set<uint32_t>(0x98, (k & 8) ? 0 : 0x80);
            switch (which) {
            case 0: both("shutdown", [&] { ((Fn0)vptr(p.o)[3])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[3])(p.r, nullptr); }); break;
            case 1: both("slot 10", [&] { ((Fn0)vptr(p.o)[10])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[10])(p.r, nullptr); }); break;
            case 2: both("slot 20", [&] { ((Fn1)vptr(p.o)[20])(p.o, nullptr, 0x20 + k); }, [&] { ((Fn1)vptr(p.r)[20])(p.r, nullptr, 0x20 + k); }); break;
            case 3: both("destroy", [&] { at<Fn0>(0x00130B00)(p.o, nullptr); }, [&] { ((GmAcObject*)p.r)->GmAcObject::~GmAcObject(); }); break;
            }
            p.compare("after");
            g_cases++;
        }
    }
    // Slot 55: the health stat.
    {
        static GmAcStat stats[3];
        std::memcpy(stats[0].m_name, "sArmor", 6);
        std::memcpy(stats[1].m_name, "sHealth", 7);
        std::memcpy(stats[2].m_name, "sHealth2", 8);
        static GmAcStat* lists[3][3] = {{nullptr, &stats[0], &stats[1]}, {&stats[2], nullptr, nullptr}, {&stats[1], &stats[1], &stats[0]}};
        static GmAcStatSet sets[3] = {{nullptr, 3, lists[0]}, {nullptr, 3, lists[1]}, {nullptr, 0, lists[2]}};
        static GmAcStatsHolder holders[3] = {{g_vt_holder, &sets[0]}, {g_vt_holder, &sets[1]}, {g_vt_holder, &sets[2]}};
        float nan;
        const uint32_t nanBits = 0x7FC00000u;
        std::memcpy(&nan, &nanBits, 4);
        const float values[] = {1.0f, 0.0f, -0.0f, -1.0f, 1e-30f, nan, 3.4e38f};
        for (int h = 0; h < 4; h++) {
            for (float v : values) {
                g_context = "object slot 55 holder " + std::to_string(h) + " value " + std::to_string(v);
                stats[1].m_value = v;
                stats[2].m_value = 5.0f;
                Pair p(kObject);
                p.set<void*>(0x270, h ? (void*)&holders[h - 1] : nullptr);
                uint32_t ro = 0, rr = 0;
                both("slot 55", [&] { ro = ((FnI0)vptr(p.o)[55])(p.o, nullptr); }, [&] { rr = ((FnI0)vptr(p.r)[55])(p.r, nullptr); });
                eq("slot 55", ro, rr);
                for (int s = 0; s < 3; s++) {
                    for (const char* name : {"sHealth", "sArmor", "none", "sHealth2"}) {
                        both("stat find", [&] { ro = U(at<FnP2>(0x0014EFB0)(&sets[s], nullptr, (uintptr_t)name, 1)); }, [&] { rr = U(sets[s].Find(name, 1)); });
                        eq("stat find", ro, rr);
                    }
                }
                p.set<void*>(0x270, nullptr);
                g_cases++;
            }
        }
    }
    // The factory, with and without a heap, failing allocation, and counts around 1.
    for (int count : {-1, 0, 1, 2, 3}) {
        for (int trouble = 0; trouble < 3; trouble++) {
            g_context = "object create " + std::to_string(count) + " trouble " + std::to_string(trouble);
            set_heap(trouble != 1);
            void* o = nullptr;
            void* r = nullptr;
            const bool crashed = both("create",
                [&] { g_fail_alloc = trouble == 2; o = at<FnCreate>(0x0015B8C0)(count); g_fail_alloc = false; },
                [&] { g_fail_alloc = trouble == 2; r = GmAcObject::Create(count); g_fail_alloc = false; });
            set_heap(true);
            eq("created", o != nullptr, r != nullptr);
            if (crashed || !o || !r) { g_cases++; continue; }
            const int n = count > 1 ? count : 1;
            if (count > 1) eq("count ahead", ((int*)o)[-1], ((int*)r)[-1]);
            for (int i = 0; i < n; i++) same_words("created object", (char*)o + i * 0x2DC, (char*)r + i * 0x2DC, 0x2DC, o, r, n * 0x2DC);
            const unsigned flags = count > 1 ? 3 : 1;
            both("give back", [&] { ((FnDel)vptr(o)[0])(o, nullptr, flags); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, flags); });
            g_cases++;
        }
    }
}

// ---- GmAcCharacter -------------------------------------------------------------------------------------------------------------------
static GmShadowCasters* g_casters[2];
static void reset_casters(int fill, GmAcCharacter* self_o, GmAcCharacter* self_r) {
    for (int s = 0; s < 2; s++) {
        std::memset(g_casters[s], 0, sizeof(GmShadowCasters));
        GmAcCharacter* self = s ? self_r : self_o;
        for (int i = 0; i < 256; i++) {
            if (fill == 0) break;
            if (fill == 1 && i >= 3) break;
            g_casters[s]->m_casters[i] = (GmAcCharacter*)(uintptr_t)(0x7000 + i * 16);
        }
        if (fill == 1) g_casters[s]->m_casters[1] = self;
        if (fill == 3) g_casters[s]->m_casters[255] = self;
        if (fill == 4) g_casters[s]->m_casters[255] = nullptr;
    }
}
static void compare_casters(const char* what) { same_words(what, g_casters[0], g_casters[1], sizeof(GmShadowCasters)); }

// Gives each side a shadow already made: each side's own fake.
static Fake* g_shadows[2];
static void give_shadow(Pair& p) {
    for (int s = 0; s < 2; s++) {
        g_shadows[s] = new_fake(g_vt_shadow, 81);
        *(Fake**)((s ? p.r : p.o) + 0x2E0) = g_shadows[s];
    }
    twin(g_shadows[0], g_shadows[1], sizeof(Fake), 6);
}
static Fake* g_cone;

static void run_character_record() {
    for (int k = 0; k < 300; k++) {
        g_context = "character slot 2 " + std::to_string(k);
        Pair p(kCharacter);
        fill_words(9000 + k, 2 + k % 2);
        if (k % 3 == 0) set_text("ShadowType", 1 + k % 3, kShadows[(k / 3) % 3]);
        if (k % 4 == 0) set_text("SightConeAttachToBone", 1 + k % 3, kBones[(k / 4) % 3]);
        p.record(true, k % 37 != 0);
        p.scene_parts(true, k % 41 != 5);
        reset_casters(k % 3, (GmAcCharacter*)p.o, p.R());
        for (int s = 0; s < 2; s++) std::memcpy((s ? p.r : p.o) + 0x90, (k & 1) ? "noteBank" : "nb\0\0\0\0\0\0", 8);
        std::memset(g_shadow_ret, 0, sizeof g_shadow_ret);
        uint32_t ro = 0, rr = 0;
        both("character slot 2", [&] { ro = ((FnI0)vptr(p.o)[2])(p.o, nullptr); }, [&] { rr = ((FnI0)vptr(p.r)[2])(p.r, nullptr); });
        eq("character slot 2", ro, rr);
        p.compare("after");
        compare_casters("casters");
        if (g_made_n[0] && g_made_n[1]) same_words("made", (void*)(uintptr_t)g_made[0][0].p, (void*)(uintptr_t)g_made[1][0].p, 0x10);
        g_cases++;
    }
    // Without the "Shadow" map: crashes on both sides, as the original does not test it.
    {
        g_context = "character slot 2 without the Shadow map";
        Pair p(kCharacter);
        clear_words();
        set_text("ShadowType", 1, "shad1");
        p.record(true, true);
        p.scene_parts(true, true);
        at<Fn1>(0x000250C0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Shadow");
        GmAcActor::s_namedMaps.Remove("Shadow");
        both("character slot 2", [&] { ((FnI0)vptr(p.o)[2])(p.o, nullptr); }, [&] { ((FnI0)vptr(p.r)[2])(p.r, nullptr); });
        at<Fn2>(0x00024EE0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Shadow", (uintptr_t)g_shadow_map);
        GmAcActor::s_namedMaps.Insert("Shadow", g_shadow_map);
        g_cases++;
    }
}

static void run_character() {
    // Shutdown and the destructor, by what the character holds.
    for (int k = 0; k < 64; k++) {
        for (int how = 0; how < 3; how++) {
            g_context = "character shutdown " + std::to_string(k) + " how " + std::to_string(how);
            Pair p(kCharacter);
            if (k & 1) both("initialize", [&] { ((FnI0)vptr(p.o)[1])(p.o, nullptr); }, [&] { ((FnI0)vptr(p.r)[1])(p.r, nullptr); });
            if (k & 2) give_interaction(p);
            if (k & 4) { p.set<Fake*>(0x2E4, g_cone); }
            if (k & 8) give_shadow(p);
            reset_casters((k >> 4) & 3, (GmAcCharacter*)p.o, p.R());
            for (uint32_t off = 0x2E8; off < 0x304; off += 4) p.set<uint32_t>(off, 0x3000 + off);
            p.set<uint32_t>(0x2DC, 0x2DC);
            p.set<uint32_t>(0x98, 0);
            switch (how) {
            case 0: both("shutdown", [&] { ((Fn0)vptr(p.o)[3])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[3])(p.r, nullptr); }); break;
            case 1: both("destroy", [&] { at<Fn0>(0x00131210)(p.o, nullptr); }, [&] { p.R()->GmAcCharacter::~GmAcCharacter(); }); break;
            case 2: both("slot 10", [&] { ((Fn0)vptr(p.o)[10])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[10])(p.r, nullptr); }); break;
            }
            p.compare("after");
            compare_casters("casters");
            g_cases++;
        }
    }
    // The small slots, by state.
    for (int k = 0; k < 64; k++) {
        g_context = "character slots " + std::to_string(k);
        Pair p(kCharacter);
        if (k & 1) p.set<Fake*>(0x2E4, g_cone);
        if (k & 2) give_shadow(p);
        if (k & 4) give_interaction(p);
        p.set<uint32_t>(0xA4, (k & 8) ? 2 : 0);
        g_visible = (k & 16) ? 1 : 0;
        g_cone->ret[7] = 0x77 + k;
        static GmAcStat zeroHealth;
        static GmAcStat* zeroList[1] = {&zeroHealth};
        static GmAcStatSet zeroSet = {nullptr, 1, zeroList};
        static GmAcStatsHolder zeroHolder = {g_vt_holder, &zeroSet};
        std::memcpy(zeroHealth.m_name, "sHealth", 7);
        zeroHealth.m_value = 0.0f;
        p.set<void*>(0x270, (k & 4) ? (void*)&zeroHolder : nullptr);
        GmAcCharController* controllers[2] = {(k & 32) ? p.controller[0] : nullptr, (k & 32) ? p.controller[1] : nullptr};
        *(GmAcCharController**)(p.o + 0x2DC) = controllers[0];
        *(GmAcCharController**)(p.r + 0x2DC) = controllers[1];
        uint32_t ro = 0, rr = 0;
        for (int s : {4, 15, 32, 33}) {
            char text[32];
            sprintf_s(text, "slot %d", s);
            both(text, [&] { ((Fn0)vptr(p.o)[s])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[s])(p.r, nullptr); });
        }
        for (int s : {20, 21}) {
            char text[32];
            sprintf_s(text, "slot %d", s);
            both(text, [&] { ((Fn1)vptr(p.o)[s])(p.o, nullptr, 0x100 + s); }, [&] { ((Fn1)vptr(p.r)[s])(p.r, nullptr, 0x100 + s); });
        }
        for (int s : {55, 59}) {
            char text[32];
            sprintf_s(text, "slot %d", s);
            both(text, [&] { ro = ((FnI0)vptr(p.o)[s])(p.o, nullptr); }, [&] { rr = ((FnI0)vptr(p.r)[s])(p.r, nullptr); });
            eq(text, ro, rr);
        }
        both("slot 49", [&] { ((Fn3)vptr(p.o)[49])(p.o, nullptr, (uintptr_t)"EVNAME", 3, 4); }, [&] { ((Fn3)vptr(p.r)[49])(p.r, nullptr, (uintptr_t)"EVNAME", 3, 4); });
        for (int s = 0; s < 2; s++) std::memcpy((s ? p.r : p.o) + 0x2FC, (k & 1) ? "EVENT123" : "ev\0\0\0\0\0\0", 8);
        both("post event", [&] { at<Fn0>(0x00130A70)(p.o, nullptr); }, [&] { p.R()->PostEvent(); });
        p.compare("after");
        g_cases++;
    }
    // The controller.
    for (int k = 0; k < 16; k++) {
        g_context = "set controller " + std::to_string(k);
        Pair p(kCharacter);
        p.set<uint32_t>(0x98, (k & 1) ? 0x100 : 0xFEFF);
        const int owner = (k >> 1) & 3;  // none, the other side's twin, this character, a constant
        const bool has = (k & 8) == 0;
        both("set controller",
             [&] { at<Fn2>(0x00130F50)(p.o, nullptr, has ? U(p.controller[0]) : 0, owner == 0 ? 0 : owner == 2 ? U(p.o) : owner == 1 ? U(p.scene[0]) : 0x5000); },
             [&] { p.R()->SetController(has ? p.controller[1] : nullptr, owner == 0 ? nullptr : owner == 2 ? (GmAcActor*)p.r : owner == 1 ? (GmAcActor*)p.scene[1] : (GmAcActor*)0x5000); });
        p.compare("after");
        g_cases++;
    }
    // The list of characters with shadows, alone.
    for (int fill = 0; fill < 5; fill++) {
        for (int who = 0; who < 3; who++) {
            g_context = "casters fill " + std::to_string(fill) + " who " + std::to_string(who);
            Pair p(kCharacter);
            reset_casters(fill, (GmAcCharacter*)p.o, p.R());
            if (fill == 2) for (int s = 0; s < 2; s++) g_casters[s]->m_casters[100] = (GmAcCharacter*)0;
            GmAcCharacter* who_o = who == 0 ? (GmAcCharacter*)p.o : (GmAcCharacter*)(uintptr_t)(0x7000 + who * 16);
            GmAcCharacter* who_r = who == 0 ? p.R() : who_o;
            both("add", [&] { at<Fn1>(0x00130B50)(g_casters[0], nullptr, U(who_o)); }, [&] { GmShadowCastersAdd(g_casters[1], who_r); });
            compare_casters("after add");
            both("remove", [&] { at<Fn1>(0x00130B90)(g_casters[0], nullptr, U(who_o)); }, [&] { GmShadowCastersRemove(g_casters[1], who_r); });
            compare_casters("after remove");
            both("remove again", [&] { at<Fn1>(0x00130B90)(g_casters[0], nullptr, U(who_o)); }, [&] { GmShadowCastersRemove(g_casters[1], who_r); });
            compare_casters("after remove again");
            g_cases++;
        }
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("GMCHAR_TRACE")) std::printf("crash at 0x%p eax=%08X ecx=%08X esi=%08X [%s]\n", e->ExceptionRecord->ExceptionAddress, (unsigned)e->ContextRecord->Eax, (unsigned)e->ContextRecord->Ecx, (unsigned)e->ContextRecord->Esi, g_context.c_str());
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
    if (pos.size() < 1) { std::printf("usage: oracle_gmchar <default.xbe> [cases dir] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    init_fake_vtables();
    pair_vftables();
    for (int i = 0; i < GmLinkedListNode::kPoolSize; i++) at<Fn0>((uint32_t)0x00076D20)((void*)(uintptr_t)(0x002E26F0 + 0x20 * i), nullptr);

    // The seams: the original's address and the recovered function, to the same double.
    struct Seam { uint32_t va; void* host; const void* fo; const void* fr; };
    void* inter_ctor = call_target((const void*)&construct_interaction);
    if (!inter_ctor) { std::printf("cannot find the interaction's constructor\n"); return 2; }
    const Seam seams[] = {
        {0x000679E0, member_address(&AtDataRegistry::FindPath), (const void*)o_find, (const void*)o_find},
        {0x0014D590, member_address(&GmAcNode::ApplyMatAssignment), (const void*)d_apply, (const void*)d_apply},
        {0x0014C1B0, member_address(&GmAcNode::IsVisible), (const void*)d_visible, (const void*)d_visible},
        {0x001553C0, inter_ctor, (const void*)d_inter_ctor, (const void*)d_inter_ctor},
        {0x00125559, (void*)&AtFormat, (const void*)d_format, (const void*)d_format},
    };
    for (const Seam& s : seams) {
        redirect(s.va, s.fo);
        if (s.host) redirect(U(s.host), s.fr);
    }
    {
        // The slots not recovered that these call directly, on both sides; the recovered side's by its vftables.
        GmAcActor* a = new GmAcActor;
        GmAcNode* n = new GmAcNode;
        const struct { uint32_t va; void* host; const void* d; } slots[] = {
            {0x0014AC20, vptr(a)[4], (const void*)d_actor4}, {0x001494C0, vptr(a)[10], (const void*)d_actor10}, {0x0014AE90, vptr(a)[15], (const void*)d_actor15},
            {0x0014B340, vptr(a)[20], (const void*)d_actor20}, {0x00148A00, vptr(a)[49], (const void*)d_actor49},
            {0x0014C0C0, vptr(n)[32], (const void*)d_node32}, {0x0014C120, vptr(n)[33], (const void*)d_node33},
        };
        for (const auto& s : slots) {
            redirect(s.va, s.d);
            redirect(U(s.host), s.d);
        }
    }

    // The statics this code reads.
    static int heap_token;
    for (uint32_t va : {0x002C632Cu, 0x002C6274u, 0x003B3930u}) *(void**)(uintptr_t)va = &heap_token;
    GmNameMap::s_heap = &heap_token;
    GmNameSpaceDb::s_heap = &heap_token;
    set_heap(true);
    init_registry();
    // The scene device's material assignments: a plain table and one namespace, real recovered name maps, read by both sides.
    static unsigned char device[0x200];
    static GmSceneDeviceHolder* holder = (GmSceneDeviceHolder*)std::calloc(1, sizeof(GmSceneDeviceHolder));
    static unsigned char state[0x40];
    *(void**)(state + 4) = holder;
    holder->m_device = (AtSceneDevice*)device;
    *(void**)(uintptr_t)0x003B357C = state;
    GmDynamicNodeState::s_instance = (GmDynamicNodeState*)state;
    {
        GmNameMapGroup* plain = new (device + 0x1A4, Placement()) GmNameMapGroup;
        GmNameSpaceDb* spaces = new (device + 0x1BC, Placement()) GmNameSpaceDb;
        GmNameMap* plainMap = new GmNameMap;
        plainMap->Init(8);
        plainMap->Insert("matA", (void*)0xAA01);
        plainMap->Insert("matB", (void*)0xAA02);
        plain->Add(plainMap);
        spaces->Init(4);
        GmNameMap* spaceMap = new GmNameMap;
        spaceMap->Init(8);
        spaceMap->Insert("matA", (void*)0xBB01);
        spaceMap->Insert("matC", (void*)0xBB03);
        spaces->Add("nsX", spaceMap);
    }
    // The joints a scene actor's +0xA0 holds.
    g_joints = new GmNameMap;
    g_joints->Init(8);
    g_joints->Insert("bone1", (void*)0xC0C01);
    g_joints->Insert("bone2", (void*)0xC0C02);
    // The list of characters with shadows: each side's own.
    for (int s = 0; s < 2; s++) g_casters[s] = (GmShadowCasters*)std::calloc(1, sizeof(GmShadowCasters));
    *(void**)(uintptr_t)0x003B3984 = g_casters[0];
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)g_casters[1];
    // The "Shadow" map in the actor's name map, made on both sides: create functions by name.
    static GmAcShadow::Registration good = {(GmAcShadow * (*)(int)) d_create_shadow};
    static GmAcShadow::Registration none = {nullptr};
    GmNameMap* shadows = g_shadow_map = new GmNameMap;
    shadows->Init(8);
    shadows->Insert("shad1", &good);
    shadows->Insert("shad0", &none);
    at<FnP0>(0x00024C60)((void*)(uintptr_t)0x002CD124, nullptr);
    at<Fn1>(0x00024DA0)((void*)(uintptr_t)0x002CD124, nullptr, 8);
    at<Fn2>(0x00024EE0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Shadow", (uintptr_t)shadows);
    at<Fn2>(0x00024EE0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Motion", 0x4D4D);
    GmAcActor::s_namedMaps.Init(8);
    GmAcActor::s_namedMaps.Insert("Shadow", shadows);
    GmAcActor::s_namedMaps.Insert("Motion", (void*)0x4D4D);
    g_cone = new_fake(g_vt_cone, 70);
    g_fill = 0xCD;
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_find();
    run_records();
    run_node_record();
    run_actor_record();
    run_lifetimes();
    run_object();
    run_character_record();
    run_character();
    report_entries();
    std::printf("%s: %d cases, %d calls crashed alike; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_crashed_alike, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
