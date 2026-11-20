// Differential oracle for GmAcPlayer, FaAcPlayer, what FaAcPlayer holds, and the controller frame (Windows x86, local only). See docs/verification.md.
//
//   oracle_gmplayer <default.xbe> [cases dir] [--entries FILE] [--mutate]
//
// No file reaches this code: nothing that makes actors from data is recovered. So the cases are objects built by each side's own constructor (the original's at its address, the recovered one by
// new), put into chosen states by writing the same field values on both sides, and then called directly; return values, every word of the object, what it points at, and the log of calls compared.
// GmAcPlayer is abstract (slot 65 is __purecall): the recovered side's is a host class that adds only slot 65, which logs; the original's slot 65 is never called.
//
// What stands in, and is the same on both sides:
//  * The design registry's search (0x000679E0, a seam) and the values it finds: one double answers both, from a table each case fills, as the gmchar oracle does. The powuh record and its two
//    words are answered by the same double when searched under that record.
//  * Reading input (0x0006DFE0, a seam): one double answering by axis, and the input object at App+0x692C is a fake whose slot 0 answers axis ids by name.
//  * The character's slots 2, 4, 10 and 20, which these call directly and the gmchar oracle compares; the node's slots 22, 23 and 27, not recovered: the original's address and the recovered
//    function, both to one double that logs (22 writes a frame chosen by the case; 23 logs every component it is given).
//  * The partner (a fake character: its slot 22 is a double, and SetController, real on both sides, writes into it), the combat object at +0x2E4, a controller owner other than a player: fakes.
// Real on both sides: GmAcCharacter's shutdown and destructor and everything beneath them, SetController, the vector methods (AtVector3, recovered elsewhere), the name helpers, GmNameMap,
// AtSegmentQuery's Allocate and Clear on the shared query of the sight queries, AtVertexFormat's constructor.
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
#include "gm/GmAcPlayer.h"
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

static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
static void** vptr(void* obj) { return *(void***)obj; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float bitsf(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

// thiscall through __fastcall: ecx = this, edx unused, the same stack arguments and cleanup.
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnI0)(void*, void*);
typedef float(__fastcall* FnF0)(void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnP2)(void*, void*, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef int(__fastcall* FnI3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn4)(void*, void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef int(__fastcall* FnIFF)(void*, void*, float, float);
typedef void*(__cdecl* FnCreate)(int);
typedef int(__cdecl* FnLen)(const char*);
typedef void(__fastcall* FnAdd)(void*, void*, uintptr_t);

// ---- canonical words ---------------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_vft_token;   // original vftable -> token
static std::map<uint32_t, uint32_t> g_host_vft;    // host vftable -> token
static const uint32_t kOrigVfts[] = {0x00285828, 0x0028E850, 0x0028E638, 0x0028E4C8, 0x0028EF44, 0x0028EF50, 0x0028E30C, 0x0028E2F4, 0x0028E308, 0x002845C0, 0x0027C1D8,
                                     0x00278568, 0x0027855C, 0x00278418, 0x0027BCC4, 0x0028E33C, 0x0028C890, 0x0028C980, 0x0028CB48, 0x0027B794, 0x0027B78C, 0x00277A3C, 0x002770F4};
struct Twin { uint32_t p[2]; uint32_t size; uint32_t id; };
static std::vector<Twin> g_twins;
static void twin(const void* a, const void* b, uint32_t size, uint32_t id) { g_twins.push_back({{U(a), U(b)}, size, id}); }

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
    return w;
}

static void same_words(const char* what, const void* o, const void* r, uint32_t size, const void* obase = nullptr, const void* rbase = nullptr, uint32_t bsize = 0) {
    if (!obase) { obase = o; rbase = r; bsize = size; }
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    for (uint32_t i = 0; i < size / 4; i++) {
        // A host vftable not paired yet takes the token of the original's in the same place, once (destructors pass through base classes' vftables no case constructs).
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
static uint32_t hash_str(const char* s) {
    if (!s) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void log_(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static uint32_t C(const void* p) { return canon(g_side, U(p), nullptr, 0); }
static void log_vec(const AtVector3* v) {
    if (!v) { log_('v', 0xFFFFFFFFu); return; }
    log_('v', fbits(v->x));
    log_('v', fbits(v->y));
    log_('v', fbits(v->z));
}

// ---- fakes -------------------------------------------------------------------------------------------------------------------------
struct Fake {
    void** vptr;
    uint32_t words[0x100];
    int ret[72];
};
template <int S> static int __fastcall fk0(Fake* f, void*) { log_('V' << 8 | S, C(f)); return f->ret[S]; }
template <int S> static int __fastcall fk1(Fake* f, void*, uintptr_t a) { log_('V' << 8 | S, C(f)); log_('a', C((void*)a)); return f->ret[S]; }
static void* g_vt_combat[8];   // the combat object at +0x2E4: deleted through slot 0, stopped through slot 4 by the character's shutdown
static void* g_vt_owner[72];   // a controller owner that is not a player: only its slot 65 is called
static void* g_vt_partner[72]; // the partner: only its slot 22 is called; the rest of it is words SetController writes
static Fake* new_fake(void** vt) {
    Fake* f = (Fake*)std::calloc(1, sizeof(Fake));
    f->vptr = vt;
    return f;
}

// The frame the partner's slot 22 and the node's give out, each pointer not null.
static AtVector3 g_frame[5];
static void write_frame(AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d, AtVector3* e) {
    AtVector3* out[5] = {a, b, c, d, e};
    for (int i = 0; i < 5; i++) {
        log_('n', out[i] != nullptr);
        if (out[i]) *out[i] = g_frame[i];
    }
}
static void __fastcall d_partner22(void* self, void*, AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d, AtVector3* e) { log_('G' << 8 | 1, C(self)); write_frame(a, b, c, d, e); }
static void __fastcall d_node22(void* self, void*, AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d, AtVector3* e) { log_('G', C(self)); write_frame(a, b, c, d, e); }
static void __fastcall d_node23(void* self, void*, AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d, AtVector3* e) {
    log_('H', C(self));
    for (AtVector3* v : {a, b, c, d, e}) log_vec(v);
}
static int g_slot27;
static int __fastcall d_node27(void* self, void*) { log_('T', C(self)); return g_slot27; }
static int g_char2;
static int __fastcall d_char2(void* self, void*) { log_('Q' << 8 | 2, C(self)); return g_char2; }
static void __fastcall d_char4(void* self, void*) { log_('Q' << 8 | 4, C(self)); }
static void __fastcall d_char10(void* self, void*) { log_('Q' << 8 | 10, C(self)); }
static void __fastcall d_char20(void* self, void*, int a) { log_('Q' << 8 | 20, C(self)); log_('q', a); }

// The input: a fake object at App+0x692C whose slot 0 gives ids by name, and the axis values by axis.
struct InputF { void** vptr; };
static void* g_vt_input[4];
static int __fastcall d_find_axis(InputF* self, void*, const char* name) {
    log_('A', C(self));
    log_('a', hash_str(name));
    if (!std::strcmp(name, "GameAxisForward")) return 0x31;
    if (!std::strcmp(name, "GameAxisTurning")) return 0x32;
    return -7;
}
static float g_axis[4];  // 0x84, 0x85, forward (0x31), turning (0x32)
static float __fastcall d_axis(void* self, void*, int controller, int axis) {
    log_('X', C(self));
    log_('x', controller);
    log_('x', axis);
    switch (axis) {
    case 0x84: return g_axis[0];
    case 0x85: return g_axis[1];
    case 0x31: return g_axis[2];
    case 0x32: return g_axis[3];
    }
    return 0.0f;
}

static void init_fake_vtables() {
    g_vt_combat[0] = (void*)fk1<0>;
    for (int s = 1; s < 8; s++) g_vt_combat[s] = (void*)fk0<1>;
    g_vt_combat[4] = (void*)fk0<4>;
    for (int s = 0; s < 72; s++) g_vt_owner[s] = nullptr;
    g_vt_owner[65] = (void*)fk0<65>;
    g_vt_partner[22] = (void*)d_partner22;
    g_vt_input[0] = (void*)d_find_axis;
}

// ---- the design registry and its values ------------------------------------------------------------------------------------------
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
struct Word {
    const char* name;
    int where;          // 1: in the object's record, 2: in the defaults, 3: both, 0: neither
    uint32_t own[4];
    uint32_t def[4];
    Val ownVal, defVal;
    LinkF ownLink, defLink;
};
static Word g_words[] = {
    {"BackwardsThresholdTurningValue"}, {"MinMoveStickMagnitude"}, {"FollowMaxHorizontalLook"}, {"FollowMinVerticalLook"}, {"FollowMaxVerticalLook"}, {"FPMaxZoom"},
    {"FPHorizontalDegreesPerSec"}, {"FPVerticalDegreesPerSec"}, {"FPMaxVerticalLook"}, {"JumpsAllowed"}, {"CosOfAngleToSwitchMouthSides"}, {"PowuhUp"}, {"Level2x4PickupType"},
    {"Level2x4PickupName"},
};
static void* const kRegistry = (void*)0x5EC00000;
static LinkF g_record;
static GmDesignDefaults g_defaults;
static LinkF g_defaultRecord;
// The powuh record: found by the names in g_powNames when g_powFound; its two words when g_decay and g_deduct.
static LinkF g_powRecord;
static Val g_decayVal, g_deductVal;
static LinkF g_decayLink, g_deductLink;
static uint32_t g_decayElem[4], g_deductElem[4];
static bool g_powFound, g_decay, g_deduct;
static const char* const g_powNames[] = {"pow1", "powuhlon"};

static AtDataLink* find_impl(void* registry, void* under, const char* path, int parents) {
    log_('F' << 8, C(registry));
    log_('f', C(under));
    log_('f', hash_str(path));
    log_('f', parents);
    if (under == &g_powRecord) {
        if (!std::strcmp(path, "PowuhUpDecay") && g_decay) return (AtDataLink*)&g_decayLink;
        if (!std::strcmp(path, "PowuhUpDeduct") && g_deduct) return (AtDataLink*)&g_deductLink;
        return nullptr;
    }
    if (under == &g_record && g_powFound) {
        for (const char* n : g_powNames) if (!std::strcmp(n, path)) return (AtDataLink*)&g_powRecord;
    }
    const int table = under == &g_defaultRecord ? 2 : 1;
    for (Word& w : g_words) {
        if (std::strcmp(w.name, path) == 0 && (w.where & table)) return (AtDataLink*)(table == 2 ? &w.defLink : &w.ownLink);
    }
    return nullptr;
}
static AtDataLink* __fastcall o_find(void* registry, void*, void* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }
static AtDataLink* r_find(AtDataRegistry* registry, AtDataLink* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }

static uint32_t g_rand;
static uint32_t rnd() { g_rand = g_rand * 1103515245u + 12345u; return (g_rand >> 16) & 0x7FFF; }
static const char* const kPowTexts[] = {"pow1", "powuhlongname", "", "nopow"};
static void fill_words(uint32_t seed, int density) {
    g_rand = seed * 2654435761u + 1;
    for (Word& w : g_words) {
        const uint32_t r = rnd() % 8;
        w.where = r < (uint32_t)(4 - density) ? 0 : (int)(1 + rnd() % 3);
        std::memset(w.own, 0, 16);
        std::memset(w.def, 0, 16);
        w.own[0] = fbits((float)(rnd() % 50) / 10.0f);
        w.def[0] = fbits(-(float)(rnd() % 50) / 10.0f);
        if (!std::strcmp(w.name, "PowuhUp")) {
            const char* a = kPowTexts[rnd() % 4];
            const char* b = kPowTexts[rnd() % 4];
            std::memcpy(w.own, a, std::strlen(a));
            std::memcpy(w.def, b, std::strlen(b));
        }
    }
    g_powFound = rnd() % 4 != 0;
    g_decay = rnd() % 3 != 0;
    g_deduct = rnd() % 3 != 0;
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
    g_decayVal = {g_vt_val, 900, g_decayElem};
    g_deductVal = {g_vt_val, 901, g_deductElem};
    g_decayLink = {nullptr, &g_decayVal};
    g_deductLink = {nullptr, &g_deductVal};
    g_defaults.m_registry = (AtDataRegistry*)0x0DEF0000;
    g_defaults.m_link = (AtDataLink*)&g_defaultRecord;
    *(void**)(uintptr_t)0x003B3524 = &g_defaults;
    GmDesignDefaults::s_instance = &g_defaults;
}

template <class M> static void* member_address(M m) { void* p; std::memcpy(&p, &m, sizeof p); return p; }
namespace {
struct Placement {};
}  // namespace
#pragma warning(disable : 4291)
inline void* operator new(std::size_t, void* where, Placement) { return where; }

// ---- the recovered side's GmAcPlayer: slot 65 is pure in the class --------------------------------------------------------------
class HostPlayer : public GmAcPlayer {
public:
    void ReadLook() override { log_('L', C(this)); }
};

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
        if (getenv("GMPLAYER_TRACE")) std::printf("crashed alike: %s [%s]\n", what, g_context.c_str());
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
    HostPlayer* h = new HostPlayer; pair_host(0x0028CB48, h);
    FaAcPlayer* f = new FaAcPlayer; pair_host(0x0027B794, f); pair_host(0x0027B78C, f, 0x32C); pair_host(0x00277A3C, &f->m_target);
    delete f;  // its sight query counted itself and allocated the shared query: given back, so both sides start with none
}

// ---- statics: the sight queries' count and shared query ----------------------------------------------------------------------------
static AtSegmentQuery* orig_query() { return (AtSegmentQuery*)(uintptr_t)0x003B39A0; }
static void compare_statics(const char* what) {
    const std::string w = what;
    eq((w + ": sight queries").c_str(), *(uint32_t*)(uintptr_t)0x003B3998, GmAcSightQuery::s_count);
    AtSegmentQuery* o = orig_query();
    AtSegmentQuery* r = &GmAcSightQuery::s_query;
    eq((w + ": query capacity").c_str(), o->m_capacity, r->m_capacity);
    eq((w + ": query count").c_str(), o->m_count, r->m_count);
    eq((w + ": query flags").c_str(), o->m_flags, r->m_flags);
    eq((w + ": query keys").c_str(), o->m_keys != nullptr, r->m_keys != nullptr);
    eq((w + ": query records").c_str(), o->m_records != nullptr, r->m_records != nullptr);
}

// ---- objects of both sides ---------------------------------------------------------------------------------------------------------
enum ObjKind { kPlayer, kFa };
static const uint32_t kSizes[] = {0x32C, 0x4D0};
static const uint32_t kCtors[] = {0x00131F80, 0x00093FA0};
static void construct_host(ObjKind kind, void* r) {
    if (kind == kPlayer) new (r, Placement()) HostPlayer; else new (r, Placement()) FaAcPlayer;
}

static GmAcPlayerUnknown304* g_view;   // the player's +0x304, one for both sides
static InputF g_input;
static unsigned char* g_app;
static GmWorldAxes g_world;

struct Pair {
    unsigned char* o;
    unsigned char* r;
    uint32_t size;
    ObjKind kind;
    unsigned char* scene[2];
    GmAcCharController* controller[2];
    Fake* partner[2];
    unsigned char* loader[2];
    explicit Pair(ObjKind k) : kind(k) {
        size = kSizes[k];
        o = (unsigned char*)fresh(size);
        r = (unsigned char*)fresh(size);
        at<FnP0>(kCtors[k])(o, nullptr);
        construct_host(k, r);
        twin(o, r, size, 1);
        for (int s = 0; s < 2; s++) {
            scene[s] = (unsigned char*)fresh(0x70);
            controller[s] = (GmAcCharController*)fresh(0x58);
            if (s == 0) at<FnP0>(0x001546C0)(controller[s], nullptr); else new (controller[s], Placement()) GmAcCharController;
            controller[s]->m_unknown04 = 0x5A;
            partner[s] = new_fake(g_vt_partner);
            partner[s]->words[0x98 / 4 - 1] = GmAcCharacter::kCharacterReset;
            loader[s] = (unsigned char*)std::calloc(1, sizeof(GmShadowCasters));
        }
        twin(scene[0], scene[1], 0x70, 2);
        twin(partner[0], partner[1], sizeof(Fake), 3);
        twin(controller[0], controller[1], 0x58, 4);
        twin(loader[0], loader[1], sizeof(GmShadowCasters), 5);
        *(void**)(uintptr_t)0x003B3984 = loader[0];
        GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)loader[1];
    }
    GmAcPlayer* P() { return (GmAcPlayer*)r; }
    FaAcPlayer* F() { return (FaAcPlayer*)r; }
    template <class T> void set(uint32_t offset, T value) { *(T*)(o + offset) = value; *(T*)(r + offset) = value; }
    template <class T> T get(int side, uint32_t offset) { return *(T*)((side ? r : o) + offset); }
    void record(bool registry = true, bool descriptor = true) {
        set<void*>(0x10, registry ? kRegistry : nullptr);
        set<void*>(0x14, descriptor ? (void*)&g_record : nullptr);
    }
    void give_scene() { *(void**)(o + 0x9C) = scene[0]; *(void**)(r + 0x9C) = scene[1]; }
    void give_controller(bool owner) {
        *(void**)(o + 0x2DC) = controller[0];
        *(void**)(r + 0x2DC) = controller[1];
        controller[0]->m_owner = owner ? (GmAcActor*)o : nullptr;
        controller[1]->m_owner = owner ? (GmAcActor*)r : nullptr;
    }
    void give_partner(uint32_t flags) {
        for (int s = 0; s < 2; s++) {
            partner[s]->words[0x98 / 4 - 1] = flags;
            *(void**)((s ? r : o) + 0x328) = partner[s];
        }
    }
    void compare(const char* what) {
        same_words(what, o, r, size);
        same_words("scene node", scene[0], scene[1], 0x70);
        same_words("controller", controller[0], controller[1], 0x58);
        same_words("partner", partner[0], partner[1], 0x304);
        same_words("loader", loader[0], loader[1], 0x50);
        compare_statics(what);
    }
    ~Pair() { g_twins.clear(); }
};

static int g_cases;
static int g_heap_token;
static void set_heap(bool on) {
    *(void**)(uintptr_t)0x003B3930 = on ? &g_heap_token : nullptr;
    GmAcActor::s_heap = on ? &g_heap_token : nullptr;
}
static Fake* g_combat;

// ---- lifetimes ---------------------------------------------------------------------------------------------------------------------
static void run_lifetimes() {
    for (int kind : {kPlayer, kFa}) {
        for (int how = 0; how < 32; how++) {
            g_context = std::string(kind == kPlayer ? "player" : "fa player") + " lifetime " + std::to_string(how);
            const uint32_t size = kSizes[kind];
            void* o = fresh(size);
            void* r = fresh(size);
            both("construct", [&] { at<FnP0>(kCtors[kind])(o, nullptr); }, [&] { construct_host((ObjKind)kind, r); });
            same_words("constructed", o, r, size);
            compare_statics("constructed");
            twin(o, r, size, 1);
            if (how & 2) {
                both("initialize", [&] { ((FnI0)vptr(o)[1])(o, nullptr); }, [&] { ((FnI0)vptr(r)[1])(r, nullptr); });
                *(uint32_t*)((char*)o + 0x30) = *(uint32_t*)((char*)r + 0x30) = 0x30;
            }
            if (how & 4) {
                *(Fake**)((char*)o + 0x2E4) = *(Fake**)((char*)r + 0x2E4) = g_combat;
            }
            if (how & 8) {
                // Every own word but the pointers a destructor follows, and the GmPlug part's.
                for (uint32_t off = 0x304; off < size; off += 4) {
                    if (off == 0x32C || off == 0x330 || (off >= 0x338 && off < 0x364)) continue;
                    if (off == 0x410) continue;
                    *(uint32_t*)((char*)o + off) = *(uint32_t*)((char*)r + off) = 0x1000 + off;
                }
                *(uint32_t*)((char*)o + 0x98) = *(uint32_t*)((char*)r + 0x98) = 0;
            }
            const int form = how & 1 ? 1 + ((how >> 4) & 1) : 0;  // 0 complete, 1 deleting through slot 0, 2 through the GmPlug part
            if (form == 0) {
                const uint32_t dtor = kind == kPlayer ? 0x00132220 : 0x00094430;
                both("destroy", [&] { at<Fn0>(dtor)(o, nullptr); }, [&] { if (kind == kPlayer) ((GmAcPlayer*)r)->GmAcPlayer::~GmAcPlayer(); else ((FaAcPlayer*)r)->FaAcPlayer::~FaAcPlayer(); });
                same_words("destroyed", o, r, size);
                compare_statics("destroyed");
            } else if (form == 2 && kind == kFa) {
                both("delete through the plug", [&] { void* g = (char*)o + 0x32C; ((FnDel)vptr(g)[0])(g, nullptr, 1); }, [&] { void* g = (char*)r + 0x32C; ((FnDel)vptr(g)[0])(g, nullptr, 1); });
                compare_statics("deleted");
            } else {
                const unsigned flag = (how >> 4) & 1;
                both("delete", [&] { ((FnDel)vptr(o)[0])(o, nullptr, flag); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, flag); });
                if (!flag) same_words("deleted, flag 0", o, r, size);
                compare_statics("deleted");
            }
            g_twins.clear();
            g_cases++;
        }
    }
    // The factory, with and without a heap, failing allocation, and counts around 1; given back through slot 0.
    for (int count : {-1, 0, 1, 2, 3}) {
        for (int trouble = 0; trouble < 3; trouble++) {
            g_context = "fa create " + std::to_string(count) + " trouble " + std::to_string(trouble);
            set_heap(trouble != 1);
            void* o = nullptr;
            void* r = nullptr;
            const bool crashed = both("create",
                [&] { g_fail_alloc = trouble == 2; o = at<FnCreate>(0x000948E0)(count); g_fail_alloc = false; },
                [&] { g_fail_alloc = trouble == 2; r = FaAcPlayer::Create(count); g_fail_alloc = false; });
            set_heap(true);
            eq("created", o != nullptr, r != nullptr);
            compare_statics("created");
            if (crashed || !o || !r) { g_cases++; continue; }
            const int n = count > 1 ? count : 1;
            if (count > 1) eq("count ahead", ((int*)o)[-1], ((int*)r)[-1]);
            for (int i = 0; i < n; i++) same_words("created player", (char*)o + i * 0x4D0, (char*)r + i * 0x4D0, 0x4D0, o, r, n * 0x4D0);
            const unsigned flags = count > 1 ? 3 : 1;
            both("give back", [&] { ((FnDel)vptr(o)[0])(o, nullptr, flags); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, flags); });
            compare_statics("given back");
            g_cases++;
        }
    }
    // The controller frame, the sight query and the sprite on dirty memory.
    for (int k = 0; k < 2; k++) {
        g_context = "members " + std::to_string(k);
        void* o = fresh(0x58);
        void* r = fresh(0x58);
        both("controller", [&] { at<FnP0>(0x001546C0)(o, nullptr); }, [&] { new (r, Placement()) GmAcCharController; });
        same_words("controller", o, r, 0x58);
        std::memset(o, 0x5A, 0x58);
        std::memset(r, 0x5A, 0x58);
        both("controller destructor", [&] { at<Fn0>(0x00154720)(o, nullptr); }, [&] { ((GmAcCharController*)r)->~GmAcCharController(); });
        same_words("controller destroyed", o, r, 0x58);
        void* so = fresh(0xA4);
        void* sr = fresh(0xA4);
        both("sprite", [&] { at<FnP0>(0x0006E1F0)(so, nullptr); }, [&] { new (sr, Placement()) GmAcSprite; });
        same_words("sprite", so, sr, 0xA4);
        std::memset((char*)so + 4, 0x6B, 0xA0);
        std::memset((char*)sr + 4, 0x6B, 0xA0);
        both("sprite clear", [&] { at<Fn0>(0x0006E1D0)(so, nullptr); }, [&] { ((GmAcSprite*)sr)->Clear(); });
        same_words("sprite cleared", so, sr, 0xA4);
        std::memset((char*)so + 4, 0x6C, 0xA0);
        std::memset((char*)sr + 4, 0x6C, 0xA0);
        both("sprite destructor", [&] { at<Fn0>(0x0006E230)(so, nullptr); }, [&] { ((GmAcSprite*)sr)->~GmAcSprite(); });
        same_words("sprite destroyed", so, sr, 0xA4);
        g_cases++;
    }
    // The sight queries: sequences of making and destroying, from each state of the shared query.
    for (int k = 0; k < 32; k++) {
        g_context = "sight queries " + std::to_string(k);
        void* qo[3];
        void* qr[3];
        // Start from a known state, the same on both sides: capacity 0 or 3, the count as the case says.
        if (k & 1) {
            if (orig_query()->m_capacity == 0) at<Fn1>(0x00057AA0)(orig_query(), nullptr, 3);
            if (GmAcSightQuery::s_query.m_capacity == 0) GmAcSightQuery::s_query.AtSegmentQuery::Allocate(3);
        } else {
            if (orig_query()->m_capacity != 0) at<Fn0>(0x00057BB0)(orig_query(), nullptr);
            if (GmAcSightQuery::s_query.m_capacity != 0) GmAcSightQuery::s_query.AtSegmentQuery::Clear();
        }
        *(uint32_t*)(uintptr_t)0x003B3998 = GmAcSightQuery::s_count = (k >> 1) & 3;
        for (int i = 0; i < 3; i++) {
            qo[i] = fresh(0x2C);
            qr[i] = fresh(0x2C);
        }
        const int makes = 1 + ((k >> 3) & 1) + ((k >> 4) & 1);
        for (int i = 0; i < makes; i++) {
            both("make", [&] { at<FnP0>(0x00152A80)(qo[i], nullptr); }, [&] { new (qr[i], Placement()) GmAcSightQuery; });
            same_words("made", qo[i], qr[i], 0x2C);
            compare_statics("made");
        }
        for (int i = makes - 1; i >= 0; i--) {
            std::memset(qo[i], 0x3D, 0x2C);
            std::memset(qr[i], 0x3D, 0x2C);
            both("destroy", [&] { at<Fn0>(0x00152AC0)(qo[i], nullptr); }, [&] { ((GmAcSightQuery*)qr[i])->~GmAcSightQuery(); });
            same_words("destroyed", qo[i], qr[i], 0x2C);
            compare_statics("destroyed");
        }
        g_cases++;
    }
    // The last one destroyed while the shared query holds nothing (its capacity cleared behind it): the destructor must not clear it again. The query's every word is compared, pointers
    // into itself made canonical, since with no storage it holds no heap pointers.
    for (int k = 0; k < 2; k++) {
        g_context = "sight query, empty shared query " + std::to_string(k);
        if (orig_query()->m_capacity == 0) at<Fn1>(0x00057AA0)(orig_query(), nullptr, 3);
        if (GmAcSightQuery::s_query.m_capacity == 0) GmAcSightQuery::s_query.AtSegmentQuery::Allocate(3);
        at<Fn0>(0x00057BB0)(orig_query(), nullptr);
        GmAcSightQuery::s_query.AtSegmentQuery::Clear();
        for (AtSegmentQuery* q : {orig_query(), &GmAcSightQuery::s_query}) {
            q->m_unknown60[0] = 0x6060;
            q->m_current = 0x7C7C;
            q->m_vectors1[0].x = 2.5f;
            q->m_flags = k ? 2 : 0;
        }
        *(uint32_t*)(uintptr_t)0x003B3998 = GmAcSightQuery::s_count = 1;
        void* qo = fresh(0x2C);
        void* qr = fresh(0x2C);
        both("destroy", [&] { at<Fn0>(0x00152AC0)(qo, nullptr); }, [&] { ((GmAcSightQuery*)qr)->~GmAcSightQuery(); });
        compare_statics("destroyed");
        g_twins.clear();
        same_words("shared query", orig_query(), &GmAcSightQuery::s_query, sizeof(AtSegmentQuery));
        g_cases++;
    }
    *(uint32_t*)(uintptr_t)0x003B3998 = GmAcSightQuery::s_count = 0;
    // The name length, and the loader's hold.
    {
        g_context = "name length";
        for (const char* s : {"", "a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefg", "abcdefgh", "abcdefghi", "abcdefghijk"}) {
            int ro = 0, rr = 0;
            both("length", [&] { ro = at<FnLen>(0x00014030)(s); }, [&] { rr = AtNameLength(s); });
            eq(s, ro, rr);
        }
        g_cases++;
        for (int k = 0; k < 4; k++) {
            g_context = "loader hold " + std::to_string(k);
            unsigned char* lo = (unsigned char*)std::calloc(1, 0x60);
            unsigned char* lr = (unsigned char*)std::calloc(1, 0x60);
            for (unsigned char* l : {lo, lr}) {
                *(uint32_t*)(l + 0x48) = (k & 1) ? 0x4848 : 0;
                *(uint32_t*)(l + 0x4C) = (k & 2) ? 7 : 0;
                *(uint32_t*)(l + 0x50) = 0;
            }
            both("add", [&] { at<FnAdd>(0x0014F230)(lo, nullptr, 0x9999); }, [&] { GmLoadedPlayersAdd((GmLoadedPlayers*)lr, (GmAcPlayer*)(uintptr_t)0x9999); });
            same_words("loader", lo, lr, 0x60);
            g_cases++;
        }
    }
}

// ---- GmAcPlayer's slots, on both kinds -------------------------------------------------------------------------------------------
static float nanf_() { return bitsf(0x7FC00000u); }

static void run_player() {
    for (int kind : {kPlayer, kFa}) {
        const std::string name = kind == kPlayer ? "player" : "fa player";
        // Slot 2: GmAcPlayer's words, then the character's (a double); for FaAcPlayer, its own words first.
        for (int k = 0; k < (kind == kFa ? 300 : 60); k++) {
            g_context = name + " slot 2 " + std::to_string(k);
            Pair p((ObjKind)kind);
            fill_words(100 + k + kind * 1000, k % 4);
            p.record(k % 17 != 3, k % 13 != 5);
            g_char2 = 0x200 + k;
            if (kind == kFa) {
                p.set<uint32_t>(0x390, 0x390);
                p.set<uint32_t>(0x394, 0x394);
                p.set<uint32_t>(0x398, 0x398);
            }
            uint32_t ro = 0, rr = 0;
            both("slot 2", [&] { ro = ((FnI0)vptr(p.o)[2])(p.o, nullptr); }, [&] { rr = ((FnI0)vptr(p.r)[2])(p.r, nullptr); });
            eq("slot 2", ro, rr);
            if (kind == kFa) {
                both("GmAcPlayer slot 2", [&] { ro = at<FnI0>(0x00131910)(p.o, nullptr); }, [&] { rr = p.P()->GmAcPlayer::Slot2(); });
                eq("GmAcPlayer slot 2", ro, rr);
            }
            p.compare("after");
            g_cases++;
        }
        // The small slots, by state.
        for (int k = 0; k < 64; k++) {
            g_context = name + " slots " + std::to_string(k);
            Pair p((ObjKind)kind);
            p.give_scene();
            *(int*)(p.scene[0] + 0x60) = *(int*)(p.scene[1] + 0x60) = -1;
            if (k & 1) p.give_controller(kind == kFa && (k & 2) != 0);  // GmAcPlayer's own slot 65 is __purecall: never its owner
            p.set<uint32_t>(0x30C, (k & 4) ? 0 : 1);
            p.set<uint32_t>(0xA8, 0xA8);
            static float threshold[2] = {0.5f, 0.0f};
            p.set<float*>(0x320, (k & 8) ? &threshold[(k >> 4) & 1] : nullptr);
            for (int s = 0; s < 2; s++) {
                std::memset(p.loader[s], 0, 0x50);
                if (k & 32) *(uint32_t*)(p.loader[s] + 0x48) = 0x4848;
            }
            if (kind == kFa) {
                static float looks[2] = {12.5f, -7.25f};
                p.set<float*>(0x370, (k & 16) ? &looks[0] : nullptr);
                p.set<float*>(0x374, (k & 32) ? &looks[1] : nullptr);
                p.set<int>(0x40C, 0x40C + k);
            }
            uint32_t ro = 0, rr = 0;
            for (int s : {4, 10, 11}) {
                char text[32];
                sprintf_s(text, "slot %d", s);
                both(text, [&] { ((Fn0)vptr(p.o)[s])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[s])(p.r, nullptr); });
            }
            for (int s : {27, 42, 66}) {
                char text[32];
                sprintf_s(text, "slot %d", s);
                both(text, [&] { ro = ((FnI0)vptr(p.o)[s])(p.o, nullptr); }, [&] { rr = ((FnI0)vptr(p.r)[s])(p.r, nullptr); });
                eq(text, ro, rr);
            }
            for (int s : {62, 63, 64}) {
                char text[32];
                sprintf_s(text, "slot %d", s);
                float fo = 0, fr = 0;
                both(text, [&] { fo = ((FnF0)vptr(p.o)[s])(p.o, nullptr); }, [&] { fr = ((FnF0)vptr(p.r)[s])(p.r, nullptr); });
                eq(text, fbits(fo), fbits(fr));
            }
            if (kind == kFa) {
                // GmAcPlayer's own, through the FaAcPlayer object.
                both("GmAcPlayer slot 4", [&] { at<Fn0>(0x001318F0)(p.o, nullptr); }, [&] { p.P()->GmAcPlayer::Slot4(); });
                both("GmAcPlayer slot 66", [&] { ro = at<FnI0>(0x00132060)(p.o, nullptr); }, [&] { rr = p.P()->GmAcPlayer::Slot66(); });
                eq("GmAcPlayer slot 66", ro, rr);
                float fo = 0, fr = 0;
                both("GmAcPlayer slot 63", [&] { fo = at<FnF0>(0x00094020)(p.o, nullptr); }, [&] { fr = p.P()->GmAcPlayer::FollowMaxVerticalLook(); });
                eq("GmAcPlayer slot 63", fbits(fo), fbits(fr));
            }
            // Slot 61 over arguments.
            const float args[] = {1.0f, 0.0f, -0.0f, -1.0f, 0.25f, -0.25f, 0.5f, -0.5f, 0.75f, nanf_(), 1e-30f};
            for (float a : {1.0f, 0.0f, -0.0f, -2.0f, nanf_(), 1e-30f}) {
                for (float b : args) {
                    both("slot 61", [&] { ro = ((FnIFF)vptr(p.o)[61])(p.o, nullptr, a, b); }, [&] { rr = ((FnIFF)vptr(p.r)[61])(p.r, nullptr, a, b); });
                    eq("slot 61", ro, rr);
                    if (kind == kFa) {
                        both("GmAcPlayer slot 61", [&] { ro = at<FnIFF>(0x00132000)(p.o, nullptr, a, b); }, [&] { rr = p.P()->GmAcPlayer::Slot61(a, b); });
                        eq("GmAcPlayer slot 61", ro, rr);
                    }
                }
            }
            p.compare("after");
            // The controller's forwarders, with a controller.
            if (k & 1) {
                const AtVector3 f = {1, 2, 3}, u = {4, 5, 6}, r = {7, 8, 9};
                both("has pending", [&] { ro = at<FnI0>(0x00131A80)(p.o, nullptr); }, [&] { rr = p.P()->HasPendingFrame(); });
                eq("has pending", ro, rr);
                both("set frame", [&] { at<Fn4>(0x00131A50)(p.o, nullptr, U(&f), U(&u), U(&r), (k >> 3) & 1); }, [&] { p.P()->SetControllerFrame(&f, &u, &r, (k >> 3) & 1); });
                both("has pending", [&] { ro = at<FnI0>(0x00131A80)(p.o, nullptr); }, [&] { rr = p.P()->HasPendingFrame(); });
                eq("has pending", ro, rr);
                AtVector3 out[2][3];
                std::memset(out, 0x77, sizeof out);
                both("frame", [&] { at<Fn3>(0x00131A70)(p.o, nullptr, U(&out[0][0]), U(&out[0][1]), U(&out[0][2])); }, [&] { p.P()->ControllerFrame(&out[1][0], &out[1][1], &out[1][2]); });
                same_words("frame", out[0], out[1], sizeof out[0]);
                both("apply pending", [&] { at<Fn0>(0x00131A40)(p.o, nullptr); }, [&] { p.P()->ApplyPendingFrame(); });
                p.compare("after forwarders");
            } else {
                const AtVector3 f = {1, 2, 3};
                uint32_t ro2 = 0, rr2 = 0;
                both("has pending, none", [&] { ro2 = at<FnI0>(0x00131A80)(p.o, nullptr); }, [&] { rr2 = p.P()->HasPendingFrame(); });
                eq("has pending, none", ro2, rr2);
                both("set frame, none", [&] { at<Fn4>(0x00131A50)(p.o, nullptr, U(&f), U(&f), U(&f), 1); }, [&] { p.P()->SetControllerFrame(&f, &f, &f, 1); });
            }
            g_cases++;
        }
        // Slot 44 over queries.
        {
            static GmAcQuery queries[4];
            static GmNameMap* withBox = nullptr;
            static GmNameMap* withoutBox = nullptr;
            if (!withBox) {
                withBox = new GmNameMap;
                withBox->Init(8);
                withBox->Insert("BIGBOX", &queries[1]);
                withBox->Insert("OTHER", &queries[2]);
                withoutBox = new GmNameMap;
                withoutBox->Init(8);
                withoutBox->Insert("OTHER", &queries[2]);
            }
            for (int k = 0; k < 4 * 4 * 3 * 2; k++) {
                g_context = name + " slot 44 " + std::to_string(k);
                Pair p((ObjKind)kind);
                p.set<GmAcPlayerUnknown304*>(0x304, g_view);
                g_view->m_unknown3C28 = (k & 1) ? 0x3C28 : 0;
                const int which = (k >> 1) & 3;   // none, queries 1..3
                const int kind44 = (k >> 3) % 4;  // 0, 1, 2, -1
                const int maps = (k / 32) % 3;    // none, with BIGBOX, without
                GmAcQuery* q = which ? &queries[which] : nullptr;
                if (q) {
                    q->m_kind = kind44 == 3 ? -1 : kind44;
                    q->m_names = maps == 0 ? nullptr : maps == 1 ? withBox : withoutBox;
                }
                uint32_t ro = 0, rr = 0;
                both("slot 44", [&] { ro = ((FnI1)vptr(p.o)[44])(p.o, nullptr, U(q)); }, [&] { rr = ((FnI1)vptr(p.r)[44])(p.r, nullptr, U(q)); });
                eq("slot 44", ro, rr);
                g_cases++;
            }
        }
        // Slot 20: the partner's frame, by the node's slot 27, the world's up and the frames given.
        for (int k = 0; k < 96; k++) {
            g_context = name + " slot 20 " + std::to_string(k);
            Pair p((ObjKind)kind);
            if (k & 1) p.give_partner(GmAcCharacter::kCharacterReset);
            g_slot27 = (k & 2) ? 1 : 0;
            static const AtVector3 ups[] = {{0, 0, 1}, {0, 1, 0}, {0.1f, 0.2f, 0.97f}, {0, 0, 0}};
            g_world.m_up = ups[(k >> 2) & 3];
            static const AtVector3 forwards[] = {{0, 1, 0}, {1, 2, 0.5f}, {0, 0, 3}, {0, 0, 0}, {-2.5f, 0.001f, 7}, {1e-7f, 0, 0}};
            g_frame[0] = {5, 6, 7};
            g_frame[1] = forwards[(k >> 4) % 6];
            g_frame[2] = {0.3f, 0.3f, 0.3f};
            g_frame[3] = {9, 9, 9};
            g_frame[4] = {8, 8, 8};
            // GmAcPlayer's, called directly: FaAcPlayer's own slot 20 is not recovered.
            both("slot 20", [&] { at<Fn1>(0x00131980)(p.o, nullptr, 0x20 + k); }, [&] { p.P()->GmAcPlayer::Slot20(0x20 + k); });
            p.compare("after");
            // The node's frame as the move frame.
            AtVector3 out[2][3];
            std::memset(out, 0x55, sizeof out);
            uint32_t ro = 0, rr = 0;
            both("GmAcPlayer move frame", [&] { ro = at<FnI3>(0x00131FE0)(p.o, nullptr, U(&out[0][0]), U(&out[0][1]), U(&out[0][2])); },
                 [&] { rr = p.P()->GmAcPlayer::MoveFrame(&out[1][0], &out[1][1], &out[1][2]); });
            eq("GmAcPlayer move frame", ro, rr);
            same_words("move frame", out[0], out[1], sizeof out[0]);
            g_cases++;
        }
        // The partner: set and let go, by its flag, a partner before, and a controller.
        for (int k = 0; k < 64; k++) {
            g_context = name + " partner " + std::to_string(k);
            Pair p((ObjKind)kind);
            if (k & 1) p.give_controller(false);
            const uint32_t flags = (k & 2) ? GmAcCharacter::kCharacterReset | 0x100 : 0x100;
            if (k & 4) p.give_partner(flags);
            for (int s = 0; s < 2; s++) {
                *(void**)((unsigned char*)p.partner[s] + 0x2DC) = (void*)(uintptr_t)0x2DC2DC;
            }
            const bool give = (k & 8) != 0;
            const bool other = (k & 16) != 0;
            // The partner given is another fake character than the one held, so that letting the old one go shows apart from giving the new one.
            Fake* given[2];
            for (int s = 0; s < 2; s++) {
                given[s] = new_fake(g_vt_partner);
                given[s]->words[0x98 / 4 - 1] = other ? 0 : flags;
                *(void**)((unsigned char*)given[s] + 0x2DC) = (void*)(uintptr_t)0x2DC2DD;
            }
            twin(given[0], given[1], sizeof(Fake), 6);
            if (k & 32) for (int s = 0; s < 2; s++) p.partner[s]->words[0x98 / 4 - 1] = other ? 0 : flags;
            both("set partner", [&] { at<Fn1>(0x00131F20)(p.o, nullptr, give ? U(given[0]) : 0); }, [&] { p.P()->SetPartner(give ? (GmAcActor*)given[1] : nullptr); });
            p.compare("after");
            same_words("given partner", given[0], given[1], 0x304);
            g_cases++;
        }
        // Shutdown and the complete destructor, by what the player holds.
        for (int k = 0; k < 16; k++) {
            for (int how = 0; how < 2; how++) {
                g_context = name + " shutdown " + std::to_string(k) + " how " + std::to_string(how);
                Pair p((ObjKind)kind);
                if (k & 1) both("initialize", [&] { ((FnI0)vptr(p.o)[1])(p.o, nullptr); }, [&] { ((FnI0)vptr(p.r)[1])(p.r, nullptr); });
                if (k & 2) p.set<Fake*>(0x2E4, g_combat);
                if (k & 4) p.give_controller(true);
                for (uint32_t off = 0x304; off < 0x328; off += 4) p.set<uint32_t>(off, 0x3000 + off);
                if (kind == kFa && (k & 8)) {
                    for (uint32_t off = 0x364; off < 0x410; off += 4) p.set<uint32_t>(off, 0x4000 + off);
                    for (uint32_t off = 0x414; off < 0x4D0; off += 4) p.set<uint32_t>(off, 0x5000 + off);
                }
                p.set<uint32_t>(0x98, 0);
                if (how == 0) {
                    both("shutdown", [&] { ((Fn0)vptr(p.o)[3])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[3])(p.r, nullptr); });
                    p.compare("after shutdown");  // before the calls below, which would hide a missing part of it
                    if (kind == kFa) {
                        both("reset", [&] { at<Fn0>(0x00092E40)(p.o, nullptr); }, [&] { p.F()->Reset(); });
                        both("GmAcPlayer shutdown", [&] { at<Fn0>(0x00132070)(p.o, nullptr); }, [&] { p.P()->GmAcPlayer::Shutdown(); });
                    }
                    p.compare("after");
                } else {
                    const uint32_t dtor = kind == kPlayer ? 0x00132220 : 0x00094430;
                    both("destroy", [&] { at<Fn0>(dtor)(p.o, nullptr); }, [&] { if (kind == kPlayer) p.P()->GmAcPlayer::~GmAcPlayer(); else p.F()->FaAcPlayer::~FaAcPlayer(); });
                    p.compare("after");
                }
                g_cases++;
            }
        }
    }
}

// ---- FaAcPlayer's own --------------------------------------------------------------------------------------------------------------
static void run_fa() {
    // The move frame from the sticks.
    const float sticks[] = {0.0f, 0.1f, -0.1f, 0.2f, -0.25f, 0.5f, -1.0f, 1.0f, 1e-30f, -0.0f, 0.19999f, 0.7071f};
    static float thresholds[] = {0.0f, 0.3f, 0.2f};
    static const AtVector3 ups[] = {{0, 0, 1}, {0.1f, 0.2f, 0.97f}, {0, 1, 0}};
    const float degrees[] = {0.0f, 30.0f, -45.5f, 400.0f};
    int k = 0;
    for (float turning : sticks) {
        for (float moving : sticks) {
            for (int variant = 0; variant < 4; variant++, k++) {
                g_context = "fa move frame " + std::to_string(k);
                Pair p(kFa);
                const int t = (k / 3) % 4;  // none (0.2), then each threshold
                p.set<float*>(0x324, t ? &thresholds[t - 1] : nullptr);
                p.set<int>(0x4B8, 0x31);
                p.set<int>(0x4BC, 0x32);
                p.set<float>(0x4C0, degrees[(k / 7) % 4]);
                g_world.m_up = ups[(k / 5) % 3];
                g_axis[2] = moving;
                g_axis[3] = turning;
                if (variant != 3 || (k / 4) % 2) p.give_controller(false);  // without a controller, moving crashes on both sides
                if (variant == 1) {
                    for (int s = 0; s < 2; s++) {
                        p.controller[s]->m_forward = {1, 2, 0.5f};
                        p.controller[s]->m_up = {0, 0.3f, 1};
                        p.controller[s]->m_right = {-1, 0.25f, 0};
                    }
                }
                if (variant == 2) {
                    for (int s = 0; s < 2; s++) p.controller[s]->m_forward = {0, 0, 2};
                }
                g_frame[1] = {0.5f, 0.5f, 0.5f};
                g_frame[2] = {0.25f, 0, 1};
                g_frame[3] = {1, -1, 0};
                AtVector3 out[2][3];
                std::memset(out, 0x55, sizeof out);
                uint32_t ro = 0, rr = 0;
                both("move frame", [&] { ro = ((FnI3)vptr(p.o)[60])(p.o, nullptr, U(&out[0][0]), U(&out[0][1]), U(&out[0][2])); },
                     [&] { rr = ((FnI3)vptr(p.r)[60])(p.r, nullptr, U(&out[1][0]), U(&out[1][1]), U(&out[1][2])); });
                eq("move frame", ro, rr);
                same_words("move frame out", out[0], out[1], sizeof out[0]);
                p.compare("after");
                g_cases++;
            }
        }
    }
    // Reading the look, with and without a frame pending.
    for (int j = 0; j < 16; j++) {
        g_context = "fa read look " + std::to_string(j);
        Pair p(kFa);
        p.give_controller((j & 4) != 0);
        for (int s = 0; s < 2; s++) p.controller[s]->m_pending = j & 1;
        g_axis[0] = (j & 2) ? 0.75f : -0.0f;
        g_axis[1] = (j & 8) ? -1.0f : 1e-30f;
        p.set<float>(0x318, 3.0f);
        p.set<float>(0x31C, 4.0f);
        both("read look", [&] { ((Fn0)vptr(p.o)[65])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[65])(p.r, nullptr); });
        p.compare("after");
        g_cases++;
    }
    // Reset on a dirty object.
    {
        g_context = "fa reset";
        Pair p(kFa);
        for (uint32_t off = 0x2DC; off < 0x4D0; off += 4) {
            if (off == 0x32C || (off >= 0x338 && off < 0x364) || (off >= 0x410 && off < 0x4B4)) continue;
            p.set<uint32_t>(off, 0x77000000u + off);
        }
        p.set<uint32_t>(0x98, 0x12345678);
        both("reset", [&] { at<Fn0>(0x00092E40)(p.o, nullptr); }, [&] { p.F()->Reset(); });
        p.compare("after reset");
        g_cases++;
    }
    // Finding a record by name, alone.
    for (int j = 0; j < 24; j++) {
        g_context = "find record " + std::to_string(j);
        Pair p(kFa);
        p.record((j & 1) == 0, (j & 2) == 0);
        g_powFound = (j & 4) != 0;
        const char* names[] = {"pow1", "", "zzz"};
        const char* n = names[(j >> 3) % 3];
        uint32_t ro = 0, rr = 0;
        both("find record", [&] { ro = U(at<FnP2>(0x0014BAC0)(p.o, nullptr, U(n), 9)); }, [&] { rr = U(p.F()->FindRecord(n, 9)); });
        eq("find record", ro, rr);
        g_cases++;
    }
}

// ---- the controller frame alone ----------------------------------------------------------------------------------------------------
static void run_controller() {
    for (int k = 0; k < 48; k++) {
        g_context = "controller " + std::to_string(k);
        Pair p(kFa);   // a player to own it
        GmAcCharController* c[2] = {p.controller[0], p.controller[1]};
        Fake* owner = new_fake(g_vt_owner);
        const int who = k % 3;  // none, the player, a fake
        for (int s = 0; s < 2; s++) {
            c[s]->m_owner = who == 0 ? nullptr : who == 1 ? (GmAcActor*)(s ? p.r : p.o) : (GmAcActor*)owner;
            c[s]->m_pending = (k / 3) & 1;
            c[s]->m_pendingForward = {11, 12, 13};
        }
        p.give_controller(who == 1);
        if (who == 2) { c[0]->m_owner = c[1]->m_owner = (GmAcActor*)owner; }
        g_axis[0] = 0.5f;
        g_axis[1] = -0.5f;
        uint32_t ro = 0, rr = 0;
        both("activate", [&] { at<Fn0>(0x00154520)(c[0], nullptr); }, [&] { c[1]->Activate(); });
        both("has pending", [&] { ro = at<FnI0>(0x00154550)(c[0], nullptr); }, [&] { rr = c[1]->HasPending(); });
        eq("has pending", ro, rr);
        const AtVector3 f = {1, 2, 3}, u = {4, 5, 6}, r = {7, 8, 9};
        const int now = (k / 6) & 1;
        both("set frame", [&] { at<Fn4>(0x001545B0)(c[0], nullptr, U(&f), U(&u), U(&r), now ? 1 + k : 0); }, [&] { c[1]->SetFrame(&f, &u, &r, now ? 1 + k : 0); });
        p.compare("after set");
        AtVector3 out[2][3];
        std::memset(out, 0x66, sizeof out);
        both("get frame", [&] { at<Fn3>(0x00154560)(c[0], nullptr, U(&out[0][0]), U(&out[0][1]), U(&out[0][2])); }, [&] { c[1]->GetFrame(&out[1][0], &out[1][1], &out[1][2]); });
        same_words("got", out[0], out[1], sizeof out[0]);
        if ((k / 12) & 1) {
            for (int s = 0; s < 2; s++) c[s]->m_pending = (k / 24) & 1 ? 0 : 1;
        }
        both("apply pending", [&] { at<Fn0>(0x00154670)(c[0], nullptr); }, [&] { c[1]->ApplyPending(); });
        p.compare("after apply");
        g_cases++;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("GMPLAYER_TRACE")) std::printf("crash at 0x%p eax=%08X ecx=%08X esi=%08X [%s]\n", e->ExceptionRecord->ExceptionAddress, (unsigned)e->ContextRecord->Eax, (unsigned)e->ContextRecord->Ecx, (unsigned)e->ContextRecord->Esi, g_context.c_str());
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
    if (pos.size() < 1) { std::printf("usage: oracle_gmplayer <default.xbe> [cases dir] [--entries FILE] [--mutate]\n"); return 2; }
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

    // The seams and the slots these call directly that are compared elsewhere or not recovered: the original's address and the recovered function, to the same double.
    {
        GmAcNode* n = new GmAcNode;
        GmAcCharacter* c = new GmAcCharacter;
        const struct { uint32_t va; void* host; const void* d; } slots[] = {
            {0x000679E0, (void*)&AtDataRegistryFind, nullptr},
            {0x0006DFE0, member_address(&GmPlayerInput::AxisValue), (const void*)d_axis},
            {0x0014BD90, vptr(n)[22], (const void*)d_node22},
            {0x0014BE10, vptr(n)[23], (const void*)d_node23},
            {0x0014C170, vptr(n)[27], (const void*)d_node27},
            {0x00131020, vptr(c)[2], (const void*)d_char2},
            {0x00130930, vptr(c)[4], (const void*)d_char4},
            {0x00130920, vptr(c)[10], (const void*)d_char10},
            {0x00130970, vptr(c)[20], (const void*)d_char20},
        };
        for (const auto& s : slots) {
            if (s.va == 0x000679E0) {
                redirect(s.va, (const void*)o_find);
                redirect(U(s.host), (const void*)r_find);
                continue;
            }
            redirect(s.va, s.d);
            redirect(U(s.host), s.d);
        }
    }

    // The statics this code reads.
    static int heap_token;
    for (uint32_t va : {0x002C632Cu, 0x002C6274u, 0x002C7F58u, 0x002C7F64u}) *(void**)(uintptr_t)va = &heap_token;
    GmNameMap::s_heap = &heap_token;
    GmNameSpaceDb::s_heap = &heap_token;
    AtSegmentHitList::s_heap = &heap_token;
    AtQueryPool::s_heap = &heap_token;
    set_heap(true);
    init_registry();
    at<FnP0>(0x00057B90)(orig_query(), nullptr);   // the original's static initializer 0x00202460, without its atexit
    g_view = (GmAcPlayerUnknown304*)std::calloc(1, sizeof(GmAcPlayerUnknown304));
    g_input.vptr = g_vt_input;
    g_app = (unsigned char*)std::calloc(1, sizeof(GmPlayerApp));
    ((GmPlayerApp*)g_app)->m_input = (GmPlayerInput*)&g_input;
    *(void**)(uintptr_t)0x00304D58 = g_app;
    GmPlayerApp::s_instance = (GmPlayerApp*)g_app;
    g_world.m_up = {0, 0, 1};
    *(void**)(uintptr_t)0x003B34E4 = &g_world;
    GmWorldAxes::s_instance = &g_world;
    // The actor's shutdown looks "Motion" up in the actor's name map: made on both sides.
    at<FnP0>(0x00024C60)((void*)(uintptr_t)0x002CD124, nullptr);
    at<Fn1>(0x00024DA0)((void*)(uintptr_t)0x002CD124, nullptr, 8);
    at<Fn2>(0x00024EE0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Motion", 0x4D4D);
    GmAcActor::s_namedMaps.Init(8);
    GmAcActor::s_namedMaps.Insert("Motion", (void*)0x4D4D);
    g_combat = new_fake(g_vt_combat);
    g_fill = 0xCD;
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_lifetimes();
    run_player();
    run_fa();
    run_controller();
    report_entries();
    std::printf("%s: %d cases, %d calls crashed alike; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_crashed_alike, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
