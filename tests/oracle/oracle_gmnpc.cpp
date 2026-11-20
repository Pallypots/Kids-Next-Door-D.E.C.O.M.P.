// Differential oracle for GmAcNPC, FaAcNPC and the helpers they alone need (Windows x86, local only). See docs/verification.md.
//
//   oracle_gmnpc <default.xbe> [cases dir] [--entries FILE] [--mutate]
//
// No file reaches this code: nothing that makes actors from data is recovered. So the cases are objects built by each side's own constructor (the original's at its address, the recovered one by
// new), put into chosen states by writing the same field values on both sides, and then called directly; return values, every word of the object, and the log of calls compared.
//
// What stands in, and is the same on both sides:
//  * The design registry's search (0x000679E0, a seam on this branch, recovered elsewhere) and the values it finds: one double answers both from a table each case fills (which words are in the
//    object's record, which in the defaults, and their values). A value's slot 5 (ElementAt) is a logging fake, and a health range value's descriptor is a plain structure with the two bounds.
//    So the readers are tested against the search's interface, not against real .ADR data.
//  * The AI action manager (constructor 0x0017D420, destructor 0x0017D6C0, setup 0x0017D4E0), FaCmCombat's constructor (0x000A3360) and every slot of it (a logging vftable the constructor's double
//    installs, or the same fake given to both sides), the game event (0x00152590, 0x00152780) and its sending (0x001512B0), the passing formatter (0x00072330: one double that formats with the
//    host's C library into buffers of its own) and the sight test (0x00130E50): the original's address and the recovered function, both to one double that logs and answers as the case says.
//  * The actor's and node's slots that the bases call directly and that are not recovered (actor 4, 10, 15, 20, 49), and the bases' seams (0x0014D590, 0x0014C1B0): doubles, as in oracle_gmchar.
//  * The target, the player list (GmDynamicNodeLoader's +0x48), the players, the stats holder and the health ranges are plain structures, one shared by both sides, only read.
// Real on both sides, the original's code on one and the recovered on the other: the bases (GmAcCharacter, GmAcObject, GmAcActor, GmAcNode, GmBaObject, recovered on the branch this sits on),
// AtNamesEqual and the stat search.
#define NOMINMAX
#include <windows.h>

#include <cstdarg>
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
#include "at/AtScene.h"
#include "at/AtSceneActor.h"
#include "gm/GmAcNPC.h"
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
typedef void*(__fastcall* FnP1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void*(__fastcall* FnP2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void*(__cdecl* FnCreate)(int);
typedef int(__cdecl* FnRange)(void*, void*);
typedef int(__cdecl* FnCmp)(const char*, const char*, unsigned);

// ---- canonical words ---------------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_vft_token;   // original vftable -> token
static std::map<uint32_t, uint32_t> g_host_vft;    // host vftable -> token
static const uint32_t kOrigVfts[] = {0x00285828, 0x0028E850, 0x0028E638, 0x0028E4C8, 0x0028EF44, 0x0028EF50, 0x0028E30C, 0x0028E2F4, 0x0028E308, 0x002845C0, 0x0027C1D8,
                                     0x00278568, 0x0027855C, 0x00278418, 0x0027BCC4, 0x0028E33C, 0x0028C890, 0x0028C980, 0x0028E9A8, 0x0027B41C};
struct Twin { uint32_t p[2]; uint32_t size; uint32_t id; };
static std::vector<Twin> g_twins;
static void twin(const void* a, const void* b, uint32_t size, uint32_t id) { g_twins.push_back({{U(a), U(b)}, size, id}); }
// What each side makes during a call (managers, combat objects), in order: the k-th of one side pairs with the k-th of the other.
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
static void* g_vt_combat[8];   // FaCmCombat: 0 deleting (flag), 1 Attach (owner), 2..5, 6 Update (argument), 7
static void* g_vt_holder[4];
static Fake* new_fake(void** vt, uint32_t id) {
    Fake* f = (Fake*)std::calloc(1, sizeof(Fake));
    f->vptr = vt;
    f->id = id;
    return f;
}
static uint32_t& word_at(Fake* f, uint32_t offset) { return f->words[(offset - 4) / 4]; }

// The combat object's slots answer 0 and read nothing of the object: a combat object the constructor's double makes is only 0xE4 bytes, smaller than a Fake, and reading a Fake's answers past
// its end once made a rare crash on one side.
template <int S> static int __fastcall cb0(void* f, void*) { log_('V' << 8 | S, C(f)); return 0; }
template <int S> static int __fastcall cb1(void* f, void*, uintptr_t a) { log_('V' << 8 | S, C(f)); log_('a', C((void*)a)); return 0; }
static void init_fake_vtables() {
    g_vt_combat[0] = (void*)&cb1<0>;
    g_vt_combat[1] = (void*)&cb1<1>;
    g_vt_combat[2] = (void*)&cb0<2>;
    g_vt_combat[3] = (void*)&cb0<3>;
    g_vt_combat[4] = (void*)&cb0<4>;
    g_vt_combat[5] = (void*)&cb0<5>;
    g_vt_combat[6] = (void*)&cb1<6>;
    g_vt_combat[7] = (void*)&cb0<7>;
    Fill1<0>::in(g_vt_holder);
    Fill0<1, 2, 3>::in(g_vt_holder);
}

// ---- the design registry and its values ------------------------------------------------------------------------------------------
// A value: what the registry's search finds is a link whose +0x04 is this. Its +0x04 is a descriptor, whose real bounds (+0x20, +0x24) a health range uses; its slot 5 hands back the element.
struct Desc {
    uint32_t words[8];
    float floor;    // +0x20
    float ceiling;  // +0x24
};
struct Val {
    void** vptr;
    Desc* desc;
    void* elem;
    uint32_t id;
};
static void* g_vt_val[8];
static void* __fastcall val_element(Val* v, void*, unsigned index) { log_('E' << 8, v->id); log_('e', index); return v->elem; }
struct LinkF {
    void* vptr;
    Val* value;
};

enum Kind { kInt, kReal, kText, kRange };
struct Word {
    std::string name;
    Kind kind;
    int where;          // 1: in the object's record, 2: in the defaults, 3: both, 0: neither
    uint32_t own[4];
    uint32_t def[4];
    Desc ownDesc, defDesc;
    Val ownVal, defVal;
    LinkF ownLink, defLink;
};
static std::vector<Word*> g_words;
static Word* add_word(const std::string& name, Kind kind) {
    Word* w = new Word();
    w->name = name;
    w->kind = kind;
    g_words.push_back(w);
    return w;
}

static void* const kRegistry = (void*)0x5EC00000;
static LinkF g_record;
static GmDesignDefaults g_defaults;
static LinkF g_defaultRecord;

static AtDataLink* find_impl(void* registry, void* under, const char* path, int parents) {
    log_('F' << 8, C(registry));
    log_('f', C(under));
    log_('f', hash_str(path));
    log_('f', parents);
    if (under != &g_record && under != &g_defaultRecord) return nullptr;
    const int table = under == &g_defaultRecord ? 2 : 1;
    for (Word* w : g_words) {
        if (w->name == path && (w->where & table)) return (AtDataLink*)(table == 2 ? &w->defLink : &w->ownLink);
    }
    return nullptr;
}
static AtDataLink* __fastcall o_find(void* registry, void*, void* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }

static uint32_t g_rand;
static uint32_t rnd() { g_rand = g_rand * 1103515245u + 12345u; return (g_rand >> 16) & 0x7FFF; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float bitsf(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
static const uint32_t kNaN = 0x7FC00000u;

static void clear_words() { for (Word* w : g_words) w->where = 0; }
static Word* word(const std::string& name) {
    for (Word* w : g_words) if (w->name == name) return w;
    std::printf("no word %s\n", name.c_str());
    std::exit(2);
}
static void set_word(const std::string& name, int where, uint32_t own, uint32_t def) {
    Word* w = word(name);
    w->where = where;
    std::memset(w->own, 0, 16);
    std::memset(w->def, 0, 16);
    w->own[0] = own;
    w->def[0] = def;
}
static void set_text(const std::string& name, int where, const char* own, const char* def) {
    Word* w = word(name);
    w->where = where;
    std::memset(w->own, 0, 16);
    std::memset(w->def, 0, 16);
    std::memcpy(w->own, own, std::strlen(own));
    std::memcpy(w->def, def, std::strlen(def));
}
static void set_range(const std::string& name, int where, float floor, float ceiling) {
    Word* w = word(name);
    w->where = where;
    w->ownDesc.floor = w->defDesc.floor = floor;
    w->ownDesc.ceiling = w->defDesc.ceiling = ceiling;
}

static void init_registry() {
    g_vt_val[5] = (void*)val_element;
    add_word("CreateAiActionManager", kInt);
    add_word("Combat", kInt);
    add_word("LowHealthRange", kRange);
    add_word("MedHealthRange", kRange);
    add_word("HighHealthRange", kRange);
    add_word("NumAIHealthLevels", kInt);
    char name[64];
    for (int level = 0; level < 5; level++) {
        sprintf_s(name, "AIHealthLevel%dThreshold", level); add_word(name, kReal);
        sprintf_s(name, "AIHealthLevel%dAction", level); add_word(name, kText);
        sprintf_s(name, "AIHealthLevel%dEvent", level); add_word(name, kText);
        sprintf_s(name, "AIHealthLevel%dNameSpace", level); add_word(name, kText);
    }
    uint32_t id = 1;
    for (Word* w : g_words) {
        w->ownVal = {g_vt_val, &w->ownDesc, w->own, id++};
        w->defVal = {g_vt_val, &w->defDesc, w->def, id++};
        w->ownLink = {nullptr, &w->ownVal};
        w->defLink = {nullptr, &w->defVal};
    }
    g_defaults.m_registry = (AtDataRegistry*)0x0DEF0000;
    g_defaults.m_link = (AtDataLink*)&g_defaultRecord;
    *(void**)(uintptr_t)0x003B3524 = &g_defaults;
    GmDesignDefaults::s_instance = &g_defaults;
}

// ---- the doubles for the seams and the slots not recovered ---------------------------------------------------------------------
static void __fastcall d_apply(void* node, void*, int which) { log_('M', C(node)); log_('m', which); }
static int __fastcall d_visible(void* node, void*) { log_('W', C(node)); return 1; }
static void __fastcall d_actor4(void* p, void*) { log_('Q' << 8 | 4, C(p)); }
static void __fastcall d_actor10(void* p, void*) { log_('Q' << 8 | 10, C(p)); }
static void __fastcall d_actor15(void* p, void*) { log_('Q' << 8 | 15, C(p)); }
static void __fastcall d_actor20(void* p, void*, int a) { log_('Q' << 8 | 20, C(p)); log_('q', a); }
static void __fastcall d_actor49(void* p, void*, const char* event, int b, int c) { log_('Q' << 8 | 49, C(p)); log_('q', hash_name(event)); log_('q', b); log_('q', c); }

// The AI action manager.
static void* __fastcall d_manager_ctor(void* p, void*) { made(p, 0x24); log_('A' << 8 | 0, C(p)); return p; }
static void __fastcall d_manager_dtor(void* p, void*) { log_('A' << 8 | 1, C(p)); }
static void __fastcall d_manager_setup(void* p, void*, void* owner) { log_('A' << 8 | 2, C(p)); log_('a', C(owner)); }

// FaCmCombat: its constructor installs the logging vftable.
static void* __fastcall d_combat_ctor(void* p, void*) {
    made(p, 0xE4);
    log_('K' << 8, C(p));
    *(void***)p = g_vt_combat;
    return p;
}

// The game event, and its sending: the object is on each side's stack, so only whether it is the one constructed last is logged.
static void* g_event[2];
static void* __fastcall d_event_ctor(void* p, void*) { g_event[g_side] = p; log_('G' << 8, 0); return p; }
static void __fastcall d_event_set(void* p, void*, void* actor, int b, int c, int d) { log_('G' << 8 | 1, p == g_event[g_side]); log_('g', C(actor)); log_('g', b); log_('g', c); log_('g', d); }
static void send_impl(void* manager, const char* event, const char* ns, void* object, int d) {
    log_('H', C(manager));
    log_('h', hash_str(event));
    log_('h', hash_str(ns));
    log_('h', object == g_event[g_side]);
    log_('h', d);
}
static void __fastcall o_send(void* manager, void*, const char* event, const char* ns, void* object, int d) { send_impl(manager, event, ns, object, d); }
static void r_send(GmEventManager* manager, const char* event, const char* ns, GmGameEvent* object, int d) { send_impl(manager, event, ns, object, d); }

// The passing formatter: each side formats into buffers of its own, so what is found by the text is compared and not where it is.
static char g_temp[2][7][0x400];
static int g_temp_next[2];
static AtTempText* __cdecl d_format_temp(AtTempText* out, const char* format, ...) {
    char* buffer = g_temp[g_side][g_temp_next[g_side]];
    g_temp_next[g_side] = (g_temp_next[g_side] + 1) % 7;
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, 0x400, format, args);
    va_end(args);
    out->m_text = buffer;
    log_('T', hash_str(format));
    log_('t', hash_str(buffer));
    return out;
}

// The sight test.
static int g_cansee;
static int cansee_impl(void* character, void* target, const int* rest) {
    log_('S', C(character));
    log_('s', C(target));
    for (int i = 0; i < 9; i++) log_('s', rest[i]);
    return g_cansee;
}
static int __fastcall o_cansee(void* character, void*, void* target, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10) {
    const int rest[9] = {a2, a3, a4, a5, a6, a7, a8, a9, a10};
    return cansee_impl(character, target, rest);
}
static int r_cansee(GmAcCharacter* character, GmAcActor* target, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10) {
    const int rest[9] = {a2, a3, a4, a5, a6, a7, a8, a9, a10};
    return cansee_impl(character, target, rest);
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
// No matching placement delete, on purpose: with one, these get an exception frame and call_target no longer finds the call.
#pragma warning(disable : 4291)
inline void* operator new(std::size_t, void* where, Placement) { return where; }
__declspec(noinline) static void construct_manager(void* p) { new (p, Placement()) GmAiActionManager; }
__declspec(noinline) static void destroy_manager(GmAiActionManager* p) { p->~GmAiActionManager(); }
__declspec(noinline) static void construct_combat(void* p) { new (p, Placement()) FaCmCombat; }
__declspec(noinline) static void construct_event(void* p) { new (p, Placement()) GmGameEvent; }

// ---- both sides of one call --------------------------------------------------------------------------------------------------------
// A call that crashes in the middle of x87 work leaves values on the register stack, and enough of them make every later load a NaN: the stack is emptied after each side, keeping the control word.
static void reset_x87() {
    unsigned short cw;
    __asm {
        fnstcw cw
        fninit
        fldcw cw
    }
}
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    g_temp_next[0] = g_temp_next[1] = 0;
    const bool oc = guarded([&] { On on(0); fo(); });
    reset_x87();
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    reset_x87();
    g_recording = false;
    g_side = 0;
    eq((std::string(what) + ": crashed").c_str(), oc, rc);
    if (oc && rc) {
        g_crashed_alike++;
        if (getenv("GMNPC_TRACE")) std::printf("crashed alike: %s [%s]\n", what, g_context.c_str());
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
    GmAcNPC* g = new GmAcNPC; pair_host(0x0028E9A8, g);
    FaAcNPC* f = new FaAcNPC; pair_host(0x0027B41C, f);
}

// ---- objects of both sides ---------------------------------------------------------------------------------------------------------
enum ObjKind { kNPC, kFaNPC };
static const uint32_t kSizes[] = {0x308, 0x320};
static const uint32_t kCtors[] = {0x0014EA60, 0x000928F0};
static const uint32_t kDtors[] = {0x0014EAE0, 0x000929C0};

struct Pair {
    unsigned char* o;
    unsigned char* r;
    uint32_t size;
    ObjKind kind;
    unsigned char* scene[2];
    explicit Pair(ObjKind k) : kind(k) {
        size = kSizes[k];
        o = (unsigned char*)fresh(size);
        r = (unsigned char*)fresh(size);
        at<FnP0>(kCtors[k])(o, nullptr);
        if (k == kNPC) new (r) GmAcNPC; else new (r) FaAcNPC;
        clear_made();
        twin(o, r, size, 1);
        for (int s = 0; s < 2; s++) scene[s] = (unsigned char*)fresh(0x70);
        twin(scene[0], scene[1], 0x70, 2);
        *(void**)(o + 0x9C) = scene[0];
        *(void**)(r + 0x9C) = scene[1];
        set<void*>(0xA0, nullptr);
    }
    FaAcNPC* F() { return (FaAcNPC*)r; }
    GmAcNPC* G() { return (GmAcNPC*)r; }
    template <class T> void set(uint32_t offset, T value) { *(T*)(o + offset) = value; *(T*)(r + offset) = value; }
    template <class T> T get(int side, uint32_t offset) { return *(T*)((side ? r : o) + offset); }
    void record(bool registry = true, bool descriptor = true) {
        set<void*>(0x10, registry ? kRegistry : nullptr);
        set<void*>(0x14, descriptor ? (void*)&g_record : nullptr);
    }
    void compare(const char* what) {
        same_words(what, o, r, size);
        same_words("scene node", scene[0], scene[1], 0x70);
        for (int k = 0; k < g_made_n[0] && k < g_made_n[1]; k++) {
            if (g_made[0][k].size == 0x24) same_words("manager", (void*)(uintptr_t)g_made[0][k].p, (void*)(uintptr_t)g_made[1][k].p, 0x24);
        }
    }
    void initialize() { both("initialize", [&] { ((FnI0)vptr(o)[1])(o, nullptr); }, [&] { ((FnI0)vptr(r)[1])(r, nullptr); }); }
    ~Pair() { g_twins.clear(); clear_made(); }
};

static int g_cases;
static int g_heap_token;
static void set_heap(bool on) {
    *(void**)(uintptr_t)0x003B3930 = on ? &g_heap_token : nullptr;
    GmAcActor::s_heap = on ? &g_heap_token : nullptr;
}

// Each side's own AI action manager, already made: 0x24 bytes from the heap, so that each side's delete frees it.
static void give_manager(Pair& p) {
    void* m[2];
    for (int s = 0; s < 2; s++) {
        m[s] = std::calloc(1, 0x24);
        std::memset(m[s], 0x4D, 0x24);
        *(void**)((s ? p.r : p.o) + 0x304) = m[s];
    }
    twin(m[0], m[1], 0x24, 5);
}

// A combat object both sides share: a logging fake with FaCmCombat's words.
static Fake* g_combat;
static void give_combat(Pair& p, Fake* combat = nullptr) { p.set<Fake*>(0x2E4, combat ? combat : g_combat); }

// ---- the shared world the condition test reads ---------------------------------------------------------------------------------------
static unsigned char* g_loader;       // GmDynamicNodeLoader: the characters at +0x48, the count at +0x4C
static unsigned char* g_player;       // a character with a controller and an actor's flag
static unsigned char* g_bystander;    // a character with no controller
static unsigned char* g_target;       // an actor that is not the player
static unsigned char* g_notActor;     // a word without the actor's flag
static GmAcStat g_health;
static GmAcStat g_shield;
static GmAcStat g_armor;
static GmAcStat* g_statList[2];
static GmAcStatSet g_statSet;
static GmAcStatsHolder g_holder;
static Desc g_rangeDesc[4];
static Val g_range[4];

static void set_loader(int how) {
    // 0: none; 1: the player; 2: a character with no controller; 3: a null character
    *(void**)(g_loader + 0x48) = how == 1 ? (void*)g_player : how == 2 ? (void*)g_bystander : nullptr;
    *(uint32_t*)(g_loader + 0x4C) = how == 0 ? 0 : 1;
}

static void init_world() {
    g_loader = (unsigned char*)std::calloc(1, sizeof(GmShadowCasters));
    *(void**)(uintptr_t)0x003B3984 = g_loader;
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)g_loader;
    for (unsigned char** c : {&g_player, &g_bystander, &g_target, &g_notActor}) *c = (unsigned char*)std::calloc(1, 0x400);
    *(uint32_t*)(g_player + 0x98) = 0x4 | 0x2000;
    *(uint32_t*)(g_player + 0x2DC) = 0x2DC0;
    *(uint32_t*)(g_bystander + 0x98) = 0x4;
    *(uint32_t*)(g_target + 0x98) = 0x4 | 0x8;
    *(uint32_t*)(g_notActor + 0x98) = 0x1;
    std::memcpy(g_health.m_name, "sHealth", 7);
    std::memcpy(g_shield.m_name, "sShield", 7);
    std::memcpy(g_armor.m_name, "sArmor", 6);
    g_statList[0] = &g_armor;
    g_statList[1] = &g_shield;
    g_statSet = {nullptr, 2, g_statList};
    g_holder = {g_vt_holder, &g_statSet};
    const float bounds[3][2] = {{0.0f, 0.25f}, {0.25f, 0.5f}, {0.5f, 1.0f}};
    for (int i = 0; i < 3; i++) {
        g_rangeDesc[i].floor = bounds[i][0];
        g_rangeDesc[i].ceiling = bounds[i][1];
        g_range[i] = {g_vt_val, &g_rangeDesc[i], nullptr, 900u + i};
    }
    // A range whose lower bound is NaN and upper is not: an unordered first comparison must answer 0.
    g_rangeDesc[3].floor = bitsf(kNaN);
    g_rangeDesc[3].ceiling = 1.0f;
    g_range[3] = {g_vt_val, &g_rangeDesc[3], nullptr, 903u};
    *(void**)(uintptr_t)0x003B3990 = (void*)0x0E7E0000;
    GmEventManager::s_instance = (GmEventManager*)0x0E7E0000;
}

// ---- lifetimes ---------------------------------------------------------------------------------------------------------------------
static void run_lifetimes() {
    for (int kind : {kNPC, kFaNPC}) {
        for (int how = 0; how < 24; how++) {
            g_context = std::string(kind == kNPC ? "npc" : "fa npc") + " lifetime " + std::to_string(how);
            Pair p((ObjKind)kind);
            if (how & 2) p.initialize();
            if (how & 4) give_manager(p);
            if (how & 8) give_combat(p);
            p.set<uint32_t>(0x30, 0x30);
            if (kind == kFaNPC) for (uint32_t off = 0x308; off < 0x320; off += 4) p.set<uint32_t>(off, 0x3000 + off);
            const int form = (how >> 4) * 2 + (how & 1);  // 0 destroy, 1 delete 1, 2 delete 0
            if (form == 1) {
                both("delete", [&] { ((FnDel)vptr(p.o)[0])(p.o, nullptr, 1); }, [&] { ((FnDel)vptr(p.r)[0])(p.r, nullptr, 1); });
                p.o = nullptr;
            } else if (form == 0) {
                both("destroy", [&] { at<Fn0>(kDtors[kind])(p.o, nullptr); },
                     [&] { if (kind == kNPC) p.G()->GmAcNPC::~GmAcNPC(); else p.F()->FaAcNPC::~FaAcNPC(); });
                p.compare("destroyed");
            } else {
                both("delete, flag 0", [&] { ((FnDel)vptr(p.o)[0])(p.o, nullptr, 0); }, [&] { ((FnDel)vptr(p.r)[0])(p.r, nullptr, 0); });
                p.compare("deleted, flag 0");
            }
            g_cases++;
        }
    }
}

// ---- shutdown and the small slots ------------------------------------------------------------------------------------------------
static void run_slots() {
    for (int kind : {kNPC, kFaNPC}) {
        for (int k = 0; k < 64; k++) {
            for (int which = 0; which < 6; which++) {
                g_context = std::string(kind == kNPC ? "npc" : "fa npc") + " slots " + std::to_string(k) + " which " + std::to_string(which);
                Pair p((ObjKind)kind);
                if (k & 1) p.initialize();
                if (k & 2) give_manager(p);
                if (k & 4) give_combat(p);
                p.set<uint32_t>(0xA4, (k & 8) ? 2 : 0);
                p.set<uint32_t>(0x30, 0x30);
                p.set<uint32_t>(0x98, (k & 16) ? 0 : 0x100);
                if (kind == kFaNPC) {
                    for (uint32_t off = 0x308; off < 0x320; off += 4) p.set<uint32_t>(off, 0x3000 + off);
                    p.set<int*>(0x314, nullptr);  // no health levels, so slot 20 reaches the combat object's words only
                    p.set<uint32_t>(0x318, (k & 32) ? fbits(5.0f) : fbits(-1.0f));
                }
                if (k & 4) { word_at(g_combat, 0x80) = (k & 32) ? U(&g_health) : 0; g_health.m_value = 1.0f; }
                switch (which) {
                case 0: both("shutdown", [&] { ((Fn0)vptr(p.o)[3])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[3])(p.r, nullptr); }); break;
                case 1: both("slot 4", [&] { ((Fn0)vptr(p.o)[4])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[4])(p.r, nullptr); }); break;
                case 2: both("slot 10", [&] { ((Fn0)vptr(p.o)[10])(p.o, nullptr); }, [&] { ((Fn0)vptr(p.r)[10])(p.r, nullptr); }); break;
                case 3: both("slot 20", [&] { ((Fn1)vptr(p.o)[20])(p.o, nullptr, 0x20 + k); }, [&] { ((Fn1)vptr(p.r)[20])(p.r, nullptr, 0x20 + k); }); break;
                case 4: both("slot 49", [&] { ((Fn3)vptr(p.o)[49])(p.o, nullptr, (uintptr_t)"EVNAME", k, 4); }, [&] { ((Fn3)vptr(p.r)[49])(p.r, nullptr, (uintptr_t)"EVNAME", k, 4); }); break;
                case 5: {
                    uint32_t ro = 0, rr = 0;
                    both("slot 60 unknown", [&] { ro = ((FnI1)vptr(p.o)[60])(p.o, nullptr, (uintptr_t)"cNone"); }, [&] { rr = ((FnI1)vptr(p.r)[60])(p.r, nullptr, (uintptr_t)"cNone"); });
                    eq("slot 60 unknown", ro, rr);
                    break;
                }
                }
                p.compare("after");
                g_cases++;
            }
        }
    }
    word_at(g_combat, 0x80) = 0;
}

// ---- records: slots 2 and 15 ----------------------------------------------------------------------------------------------------------
static void run_records() {
    // Slot 15: the action manager, and FaAcNPC's combat object.
    for (int kind : {kNPC, kFaNPC}) {
        for (int k = 0; k < 256; k++) {
            g_context = std::string(kind == kNPC ? "npc" : "fa npc") + " slot 15 " + std::to_string(k);
            Pair p((ObjKind)kind);
            clear_words();
            static const uint32_t values[] = {0, 1, 0x100, 0xFFFFFFFFu};
            set_word("CreateAiActionManager", k & 3, values[(k >> 2) & 3], values[((k >> 2) + 1) & 3]);
            g_rand = 9000 + k * 13;
            set_word("Combat", (k >> 4) & 3, values[rnd() % 4], values[rnd() % 4]);
            p.record(k % 13 != 5, k % 11 != 7);
            if (k % 9 == 4) give_manager(p);
            if (k % 7 == 3) give_combat(p);
            const int trouble = k % 10 == 3 ? 1 : k % 10 == 8 ? 2 : 0;
            set_heap(trouble != 1);
            both("slot 15",
                 [&] { g_fail_alloc = trouble == 2; ((Fn0)vptr(p.o)[15])(p.o, nullptr); g_fail_alloc = false; },
                 [&] { g_fail_alloc = trouble == 2; ((Fn0)vptr(p.r)[15])(p.r, nullptr); g_fail_alloc = false; });
            set_heap(true);
            p.compare("after");
            g_cases++;
        }
    }
    // Slot 2: GmAcNPC's is the character's; FaAcNPC's reads the ranges (record and parents only) and the level count (and the defaults).
    for (int kind : {kNPC, kFaNPC}) {
        for (int k = 0; k < 256; k++) {
            g_context = std::string(kind == kNPC ? "npc" : "fa npc") + " slot 2 " + std::to_string(k);
            Pair p((ObjKind)kind);
            clear_words();
            g_rand = 77 + k * 31;
            for (const char* name : {"LowHealthRange", "MedHealthRange", "HighHealthRange"}) set_range(name, rnd() % 4, 0.1f * (rnd() % 5), 0.2f * (rnd() % 5));
            set_word("NumAIHealthLevels", rnd() % 4, rnd() % 5, 7);
            p.record(k % 13 != 5, k % 11 != 7);
            if (kind == kFaNPC) for (uint32_t off = 0x308; off < 0x320; off += 4) p.set<uint32_t>(off, 0x3000 + off);
            uint32_t ro = 0, rr = 0;
            both("slot 2", [&] { ro = ((FnI0)vptr(p.o)[2])(p.o, nullptr); }, [&] { rr = ((FnI0)vptr(p.r)[2])(p.r, nullptr); });
            eq("slot 2", ro, rr);
            p.compare("after");
            g_cases++;
        }
    }
    // The reader alone.
    for (int k = 0; k < 48; k++) {
        g_context = "find value object " + std::to_string(k);
        Pair p(kNPC);
        clear_words();
        set_range("LowHealthRange", k & 3, 0.5f, 1.0f);
        set_word("Combat", (k >> 2) & 3, 1, 2);
        p.record((k & 16) == 0, (k & 32) == 0);
        for (const char* name : {"LowHealthRange", "Combat", "", "None"}) {
            uint32_t ro = 0, rr = 0;
            both("find", [&] { ro = U(at<FnP2>(0x0014CB00)(p.o, nullptr, (uintptr_t)name, 9)); }, [&] { rr = U(p.G()->FindValueObject(name, 9)); });
            eq("find", ro, rr);
        }
        g_cases++;
    }
}

// ---- the health levels -----------------------------------------------------------------------------------------------------------
static void run_health_levels() {
    static int count;
    static const uint32_t lasts[] = {0x7F7FFFFFu, fbits(100.0f), fbits(50.0f), 0, fbits(-1.0f), kNaN, fbits(30.0f), fbits(75.0f)};
    static const float values[] = {100.0f, 60.0f, 50.0f, 49.0f, 25.0f, 10.0f, 0.0f, -5.0f, 75.0f, 30.0f};
    static const float maxima[] = {100.0f, 200.0f, 0.0f, 50.0f};
    static const float thresholds[] = {0.75f, 0.5f, 0.25f, 0.0f, 1.0f, 0.3f};
    for (int k = 0; k < 1500; k++) {
        g_context = "health levels " + std::to_string(k);
        Pair p(kFaNPC);
        g_rand = 1234 + k * 7919;
        clear_words();
        const int combat = k % 17 == 0 ? 0 : k % 17 == 1 ? 1 : 2;
        if (combat) give_combat(p);
        word_at(g_combat, 0x80) = combat == 2 ? U(&g_health) : 0;
        g_health.m_value = k % 23 == 0 ? bitsf(kNaN) : values[rnd() % 10];
        g_health.m_max = maxima[rnd() % 4];
        g_health.m_min = 0.0f;
        p.set<uint32_t>(0x318, lasts[rnd() % 8]);
        const int levels = (int)(rnd() % 7) - 1;  // -1 .. 5, and absent
        count = levels;
        p.set<int*>(0x314, k % 13 == 6 ? nullptr : &count);
        for (int level = 0; level < 5; level++) {
            char name[64];
            sprintf_s(name, "AIHealthLevel%dThreshold", level);
            set_word(name, rnd() % 4, fbits(thresholds[rnd() % 6]), fbits(thresholds[rnd() % 6]));
            sprintf_s(name, "AIHealthLevel%dAction", level);
            set_text(name, rnd() % 3 == 0 ? 0 : 1 + rnd() % 3, "ACTION", "DACTION");
            sprintf_s(name, "AIHealthLevel%dEvent", level);
            set_text(name, rnd() % 3 == 0 ? 0 : 1 + rnd() % 3, "EVENT", "DEVENT");
            sprintf_s(name, "AIHealthLevel%dNameSpace", level);
            set_text(name, rnd() % 3 == 0 ? 0 : 1 + rnd() % 3, "NS", "DNS");
        }
        p.record(true, k % 29 != 3);
        if (k % 2) {
            both("levels", [&] { at<Fn0>(0x00092650)(p.o, nullptr); }, [&] { p.F()->UpdateHealthLevels(); });
        } else {
            p.set<uint32_t>(0xA4, (k % 8 == 2) ? 0 : 2);
            both("slot 20", [&] { ((Fn1)vptr(p.o)[20])(p.o, nullptr, 7); }, [&] { ((Fn1)vptr(p.r)[20])(p.r, nullptr, 7); });
        }
        p.compare("after");
        g_cases++;
    }
    word_at(g_combat, 0x80) = 0;
}

// ---- the condition test ----------------------------------------------------------------------------------------------------------------
static const char* const kConditions[] = {
    "cDead", "cAlive", "cLowHP", "cMedHP", "cHighHP", "cThreat", "cNThreat", "cNCanSee", "cFront", "cNFront", "cTarget", "cNTarget", "cAtkRng", "cNAtkRng",
    "cAtkRg0", "cAtkRg3", "cAtkRg-", "cAtkRgX9", "cAtkRg", "cNAtkRg0", "cNAtkRg3", "cNAtkRg", "cTgtAir", "cNTgtAir", "cTgtPlr", "cNTgtPlr", "cShield", "cNShield",
    "cNThreatZ", "cDeadX", "cDea", "unknown", "", "cNCanSe", "cAtkRx3", "cAtkRx0", "cNAtkRx0", "cNAtkRx3"};
static void run_conditions() {
    static const uint32_t healths[] = {fbits(1.0f), 0, 0x80000000u, fbits(-1.0f), kNaN, fbits(25.0f), fbits(50.0f), fbits(37.5f), fbits(100.0f), fbits(0.5f)};
    static const int ranges[] = {-1, 0, 3, 5, 0x3B};
    for (int k = 0; k < 1200; k++) {
        g_context = "conditions " + std::to_string(k);
        Pair p(kFaNPC);
        g_rand = 4242 + k * 104729;
        const int combat = rnd() % 4;  // none, no health, with health, with health
        if (combat) give_combat(p);
        word_at(g_combat, 0x80) = combat >= 2 ? U(&g_health) : 0;
        word_at(g_combat, 0x94) = rnd() % 3 == 0 ? 0 : rnd() % 2 ? 1 : 0x100;
        word_at(g_combat, 0x98) = (uint32_t)ranges[rnd() % 5];
        g_health.m_min = rnd() % 4 == 0 ? 10.0f : 0.0f;
        g_health.m_max = 100.0f;
        g_health.m_value = bitsf(healths[rnd() % 10]);
        for (int i = 0; i < 3; i++) p.set<void*>(0x308 + 4 * i, rnd() % 5 == 0 ? nullptr : (void*)&g_range[(i + rnd() % 2) % 3]);
        const int target = rnd() % 5;
        void* targets[] = {nullptr, g_player, g_target, g_notActor, g_bystander};
        p.set<void*>(0x17C, targets[target]);
        set_loader(rnd() % 4);
        static const uint32_t airs[] = {0, fbits(1.0f), kNaN, 0x80000000u};
        *(uint32_t*)(g_player + 0x154) = airs[rnd() % 4];
        const int holder = rnd() % 3;
        p.set<void*>(0x270, holder ? (void*)&g_holder : nullptr);
        g_statSet.m_count = holder == 2 ? 1 : 2;  // without sShield, or with it
        g_shield.m_value = bitsf(healths[rnd() % 5]);
        g_cansee = (int)(rnd() % 3) * 2 - 1 + 1;  // 0, 2, 4
        for (const char* name : kConditions) {
            uint32_t ro = 0, rr = 0;
            const std::string what = std::string("condition [") + name + "]";
            both(what.c_str(), [&] { ro = ((FnI1)vptr(p.o)[60])(p.o, nullptr, (uintptr_t)name); }, [&] { rr = ((FnI1)vptr(p.r)[60])(p.r, nullptr, (uintptr_t)name); });
            eq(what.c_str(), ro, rr);
            if (ro != rr && getenv("GMNPC_TRACE")) {
                void* low = p.get<void*>(0, 0x308);
                std::printf("  health %08X min %g max %g low %p (desc %g %g) combat %d; direct: orig %d\n", fbits(g_health.m_value), g_health.m_min, g_health.m_max, low,
                            low ? ((Val*)low)->desc->floor : 0.0f, low ? ((Val*)low)->desc->ceiling : 0.0f, combat, at<FnRange>(0x001524C0)(&g_health, low));
            }
        }
        uint32_t ro = 0, rr = 0;
        both("target", [&] { ro = U(at<FnP0>(0x000920D0)(p.o, nullptr)); }, [&] { rr = U(GmAcTargetOf((GmAcActor*)p.r)); });
        eq("target", ro, rr);
        p.compare("after");
        g_cases++;
    }
    word_at(g_combat, 0x80) = 0;
    g_statSet.m_count = 2;
}

// ---- the helpers alone ------------------------------------------------------------------------------------------------------------------
static void run_helpers() {
    static const uint32_t values[] = {0, fbits(10.0f), fbits(25.0f), fbits(25.000002f), fbits(50.0f), fbits(49.999996f), fbits(100.0f), fbits(-1.0f), kNaN, fbits(12.5f), fbits(62.5f), 0x7F800000u};
    static const float mins[] = {0.0f, 10.0f, -50.0f};
    static const float maxs[] = {100.0f, 10.0f, 0.0f, 1e30f};
    for (uint32_t v : values) {
        for (float lo : mins) {
            for (float hi : maxs) {
                for (int r = 0; r < 5; r++) {
                    g_context = "range";
                    GmAcStat stat = {};
                    stat.m_value = bitsf(v);
                    stat.m_min = lo;
                    stat.m_max = hi;
                    void* range = r < 4 ? (void*)&g_range[r] : nullptr;
                    int ro = 0, rr = 0;
                    both("range", [&] { ro = at<FnRange>(0x001524C0)(&stat, range); }, [&] { rr = GmStatInRange(&stat, (AtDataComponent*)range); });
                    eq("range", ro, rr);
                    both("range, no stat", [&] { ro = at<FnRange>(0x001524C0)(nullptr, range); }, [&] { rr = GmStatInRange(nullptr, (AtDataComponent*)range); });
                    eq("range, no stat", ro, rr);
                    g_cases++;
                }
            }
        }
    }
    // Two copies of one text with different bytes after the terminator: a compare that ran past the end would tell them apart.
    static const char tailA[] = "cAtk\0AAAA";
    static const char tailB[] = "cAtk\0BBBB";
    static const char* const texts[] = {nullptr, "", "cAtkRg1", "cAtkRg", "cAtk", "\x80x", "x", "cNAtkRg2", "cAtkRgZ", tailA, tailB};
    for (const char* a : texts) {
        for (const char* b : texts) {
            for (unsigned n : {0u, 1u, 3u, 6u, 7u, 100u}) {
                g_context = "compare";
                int ro = 0, rr = 0;
                both("compare", [&] { ro = at<FnCmp>(0x00013E00)(a, b, n); }, [&] { rr = AtCompareN(a, b, n); });
                eq("compare", ro, rr);
                g_cases++;
            }
        }
    }
    for (int how = 0; how < 4; how++) {
        set_loader(how);
        for (unsigned n : {0u, 1u, 2u}) {
            g_context = "nth controlled " + std::to_string(how) + " " + std::to_string(n);
            uint32_t ro = 0, rr = 0;
            both("nth", [&] { ro = U(at<FnP1>(0x00092110)(g_loader, nullptr, n)); }, [&] { rr = U(GmNthControlledCharacter((GmControlledCharacters*)g_loader, n)); });
            eq("nth", ro, rr);
            g_cases++;
        }
    }
    for (uint32_t v : {0u, fbits(1.0f), kNaN, 0x80000000u, fbits(-2.0f), 1u}) {
        g_context = "float 154";
        *(uint32_t*)(g_player + 0x154) = v;
        int ro = 0, rr = 0;
        both("float 154", [&] { ro = at<FnI0>(0x00092C20)(g_player, nullptr); }, [&] { rr = GmAcFloat154NotZero((GmAcActor*)g_player); });
        eq("float 154", ro, rr);
        g_cases++;
    }
}

// ---- the factories ---------------------------------------------------------------------------------------------------------------------
static void run_factories() {
    for (int kind : {kNPC, kFaNPC}) {
        const uint32_t size = kSizes[kind];
        for (int count : {-1, 0, 1, 2, 3}) {
            for (int trouble = 0; trouble < 3; trouble++) {
                g_context = std::string(kind == kNPC ? "npc" : "fa npc") + " create " + std::to_string(count) + " trouble " + std::to_string(trouble);
                set_heap(trouble != 1);
                void* o = nullptr;
                void* r = nullptr;
                const bool crashed = both("create",
                    [&] { g_fail_alloc = trouble == 2; o = at<FnCreate>(kind == kNPC ? 0x0014EB70 : 0x00092A50)(count); g_fail_alloc = false; },
                    [&] { g_fail_alloc = trouble == 2; r = kind == kNPC ? (void*)GmAcNPC::Create(count) : (void*)FaAcNPC::Create(count); g_fail_alloc = false; });
                set_heap(true);
                eq("created", o != nullptr, r != nullptr);
                if (crashed || !o || !r) { g_cases++; continue; }
                const int n = count > 1 ? count : 1;
                if (count > 1) eq("count ahead", ((int*)o)[-1], ((int*)r)[-1]);
                for (int i = 0; i < n; i++) same_words("created object", (char*)o + i * size, (char*)r + i * size, size, o, r, n * size);
                const unsigned flags = count > 1 ? 3 : 1;
                both("give back", [&] { ((FnDel)vptr(o)[0])(o, nullptr, flags); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, flags); });
                g_cases++;
            }
        }
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("GMNPC_TRACE")) std::printf("crash at 0x%p eax=%08X ecx=%08X esi=%08X [%s]\n", e->ExceptionRecord->ExceptionAddress, (unsigned)e->ContextRecord->Eax, (unsigned)e->ContextRecord->Ecx, (unsigned)e->ContextRecord->Esi, g_context.c_str());
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
    if (pos.size() < 1) { std::printf("usage: oracle_gmnpc <default.xbe> [cases dir] [--entries FILE] [--mutate]\n"); return 2; }
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
    void* manager_ctor = call_target((const void*)&construct_manager);
    void* manager_dtor = call_target((const void*)&destroy_manager);
    void* combat_ctor = call_target((const void*)&construct_combat);
    void* event_ctor = call_target((const void*)&construct_event);
    if (!manager_ctor || !manager_dtor || !combat_ctor || !event_ctor) { std::printf("cannot find a seam's host address\n"); return 2; }
    const Seam seams[] = {
        {0x000679E0, member_address(&AtDataRegistry::FindPath), (const void*)o_find, (const void*)o_find},  // recovered (at/datalookup), a double on both sides here
        {0x0014D590, member_address(&GmAcNode::ApplyMatAssignment), (const void*)d_apply, (const void*)d_apply},
        {0x0014C1B0, member_address(&GmAcNode::IsVisible), (const void*)d_visible, (const void*)d_visible},
        {0x0017D420, manager_ctor, (const void*)d_manager_ctor, (const void*)d_manager_ctor},
        {0x0017D6C0, manager_dtor, (const void*)d_manager_dtor, (const void*)d_manager_dtor},
        {0x0017D4E0, member_address(&GmAiActionManager::Setup), (const void*)d_manager_setup, (const void*)d_manager_setup},
        {0x000A3360, combat_ctor, (const void*)d_combat_ctor, (const void*)d_combat_ctor},
        {0x00152590, event_ctor, (const void*)d_event_ctor, (const void*)d_event_ctor},
        {0x00152780, member_address(&GmGameEvent::Set), (const void*)d_event_set, (const void*)d_event_set},
        {0x001512B0, (void*)&GmEventManagerSend, (const void*)o_send, (const void*)r_send},
        {0x00072330, (void*)&AtFormatTemp, (const void*)d_format_temp, (const void*)d_format_temp},
        {0x00130E50, (void*)&GmAcCharacterCanSee, (const void*)o_cansee, (const void*)r_cansee},
    };
    for (const Seam& s : seams) {
        redirect(s.va, s.fo);
        if (s.host) redirect(U(s.host), s.fr);
    }
    {
        // The slots not recovered that the bases call directly, on both sides; the recovered side's by its vftables.
        GmAcActor* a = new GmAcActor;
        const struct { uint32_t va; void* host; const void* d; } slots[] = {
            {0x0014AC20, vptr(a)[4], (const void*)d_actor4}, {0x001494C0, vptr(a)[10], (const void*)d_actor10}, {0x0014AE90, vptr(a)[15], (const void*)d_actor15},
            {0x0014B340, vptr(a)[20], (const void*)d_actor20}, {0x00148A00, vptr(a)[49], (const void*)d_actor49},
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
    init_world();
    // The material assignments the node's slot 2 looks up: empty tables, so nothing is found; the device is read by both sides.
    static unsigned char device[0x200];
    static GmSceneDeviceHolder* holder = (GmSceneDeviceHolder*)std::calloc(1, sizeof(GmSceneDeviceHolder));
    static unsigned char state[0x40];
    *(void**)(state + 4) = holder;
    holder->m_device = (AtSceneDevice*)device;
    *(void**)(uintptr_t)0x003B357C = state;
    GmDataRoot::s_instance = (GmDataRoot*)state;
    {
        GmNameMapGroup* plain = new (device + 0x1A4, Placement()) GmNameMapGroup;
        GmNameSpaceDb* spaces = new (device + 0x1BC, Placement()) GmNameSpaceDb;
        GmNameMap* plainMap = new GmNameMap;
        plainMap->Init(8);
        plain->Add(plainMap);
        spaces->Init(4);
    }
    // The actor's name map, which the character's shutdown looks "Shadow" up in.
    at<FnP0>(0x00024C60)((void*)(uintptr_t)0x002CD124, nullptr);
    at<Fn1>(0x00024DA0)((void*)(uintptr_t)0x002CD124, nullptr, 8);
    GmAcActor::s_namedMaps.Init(8);
    g_combat = new_fake(g_vt_combat, 70);
    g_fill = 0xCD;
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_lifetimes();
    run_slots();
    run_records();
    run_health_levels();
    run_conditions();
    run_helpers();
    run_factories();
    report_entries();
    std::printf("%s: %d cases, %d calls crashed alike; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_crashed_alike, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
