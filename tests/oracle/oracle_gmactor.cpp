// Differential oracle for the game layer's actor base (Windows x86, local only). See docs/verification.md.
//
//   oracle_gmactor <default.xbe> [--entries FILE] [--mutate]
//
// No file reaches this code: there is no loader for it yet. So the cases are objects built by each side's own constructor (the original's at its address, the recovered one by new), put into chosen
// states by writing the same field values on both sides, and then called directly; return values, the whole object, objects it points at, and the log of calls are compared.
//
// What stands in, and is the same on both sides:
//  * Every function the recovered code leaves as a seam (each stops with a diagnostic in a real build): the original's address and the recovered function are both redirected to one double that logs its
//    arguments and answers what the case says. So a green result says the recovered code calls the seams in the same order with the same arguments and does the same with their answers. It says nothing
//    about the seams themselves, which are not recovered.
//  * Slots of an actor or node that are not recovered (8, 2, 28, 29, 40, 49): the object's vftable is copied and those slots replaced, on both sides.
//  * Objects the actor only holds pointers to (component, motions, scene actor, link and its owner, trigger table): fakes, with vftables of logging functions, one copy per side where they are written.
//  * The sound manager: one GmAudioData both sides read, with two banks of notes; the original's search runs on it on one side and the recovered search on the other.
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
#include "at/AtMatAssignment.h"
#include "at/AtNameTableOwner.h"
#include "at/AtSpaceGraph.h"
#include "gm/GmAcActor.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManifest.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 160).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 30) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 160).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

// The manifest loader is linked in for the statics the actor reads; its clock is not used here.
unsigned int AtPlatformMilliseconds() { return 0; }

static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
static void** vptr(void* obj) { return *(void***)obj; }

// thiscall through __fastcall: ecx = this, edx unused, the same stack arguments and cleanup.
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnI0)(void*, void*);
typedef float(__fastcall* FnF0)(void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef int(__fastcall* FnI2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn4)(void*, void*, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void*(__cdecl* FnCreate)(int);

// ---- canonical words: what a pointer leads to, rather than where ---------------------------------------------------------
// Vftables: the original's by address, the recovered side's paired with them by constructing the same class on both sides first. Two abstract bases are paired at first sight.
static std::map<uint32_t, uint32_t> g_vft_token;   // original vftable -> token
static std::map<uint32_t, uint32_t> g_host_vft;    // host vftable -> token
static const uint32_t kOrigVfts[] = {0x00285828, 0x0028E850, 0x0028E638, 0x0028E4C8, 0x0028EF44, 0x0028EF50, 0x0028E30C, 0x0028E2F4, 0x0028E308, 0x002845C0, 0x0027C1D8,
                                     0x00278568, 0x0027855C, 0x00278418, 0x0027BCC4, 0x0028E33C};
// Fakes and other objects of which each side has its own copy: canonical by an id.
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

// Every word of two objects, canonical. A host vftable not yet paired is paired with the original's at the same place, if that one is a vftable.
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

// ---- logging what a double is given --------------------------------------------------------------------------------------
static uint32_t hash_name(const char* s) {
    if (!s) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; i < 8 && s[i]; i++) h = (h ^ (unsigned char)s[i]) * 16777619u;
    return h;
}
static void log_(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }
static uint32_t C(const void* p) { return canon(g_side, U(p), nullptr, 0); }

// ---- the fakes -----------------------------------------------------------------------------------------------------------
// A fake object: a vftable of logging slots, and words. Slots answer from m_ret.
struct Fake {
    void** vptr;
    uint32_t words[0x40];
    uint32_t id;
    int ret[64];
};
template <int S> static int __fastcall fk0(Fake* f, void*) { log_('V' << 8 | S, f->id); return f->ret[S]; }
template <int S> static int __fastcall fk1(Fake* f, void*, uintptr_t a) { log_('V' << 8 | S, f->id); log_('a', C((void*)a)); return f->ret[S]; }
template <int S> static int __fastcall fk2(Fake* f, void*, uintptr_t a, uintptr_t b) { log_('V' << 8 | S, f->id); log_('a', C((void*)a)); log_('b', C((void*)b)); return f->ret[S]; }

// Vftables for the fakes. Slot 0 is the deleting destructor (one argument), the rest as each kind of object is called.
static void* g_vt_del1[64];   // slot 0 takes a flag; slots 1..63 take nothing
static void* g_vt_socket[8];  // slot 2 Remove(item)
static void* g_vt_node[64];   // a node's slots 28, 29 take nothing; 40, 41 one
static void* g_vt_scene[64];  // a scene node's: slot 7 the deleting destructor, slot 11 Clear
template <int... S> struct Fill0 { static void in(void** vt) { (void)std::initializer_list<int>{(vt[S] = (void*)&fk0<S>, 0)...}; } };
template <int... S> struct Fill1 { static void in(void** vt) { (void)std::initializer_list<int>{(vt[S] = (void*)&fk1<S>, 0)...}; } };

static void init_fake_vtables() {
    Fill0<1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20>::in(g_vt_del1);
    Fill1<0>::in(g_vt_del1);
    Fill1<0, 1, 2, 3>::in(g_vt_socket);
    Fill0<28, 29>::in(g_vt_node);
    Fill1<0, 40, 41>::in(g_vt_node);
    Fill0<0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12>::in(g_vt_scene);
    Fill1<7>::in(g_vt_scene);
}

// Up to four fake objects per side, made fresh for each case.
static Fake* new_fake(void** vt, uint32_t id) {
    Fake* f = (Fake*)std::calloc(1, sizeof(Fake));
    f->vptr = vt;
    f->id = id;
    return f;
}

// ---- the doubles for the seams ------------------------------------------------------------------------------------------
static char g_trigger_first[8], g_trigger_second[8], g_note_bank[8];
static GmSoundNote* g_trigger_note;
static int g_vfx_has;
static AtVector3 g_position;

static void __fastcall d_spawn(void* actor, void*, const char* name, int b, const char* c, int d, int e, int f) {
    log_('S', C(actor)); log_('s', hash_name(name)); log_('s', b); log_('s', c ? hash_name(c) : 0); log_('s', d); log_('s', e); log_('s', f);
}
static void __fastcall d_note_bank(void* actor, void*, char* out) { log_('B', C(actor)); std::memcpy(out, g_note_bank, 8); }
static void __fastcall d_scene_flag(void* actor, void*, int on) { log_('G', C(actor)); log_('g', on); }
static void __fastcall d_audio38(void* audio, void*, int a) { log_('A', C(audio)); log_('a', a); }
static void __fastcall d_send_event(void* component, void*, int a, const char* name) { log_('E', C(component)); log_('e', a); log_('e', hash_name(name)); }
static void __fastcall d_find_trigger(void* table, void*, const char* trigger, char* first, char* second) {
    log_('T', C(table)); log_('t', hash_name(trigger));
    std::memcpy(first, g_trigger_first, 8);
    std::memcpy(second, g_trigger_second, 8);
}
static GmSoundNote* __fastcall d_trigger_note(void* table, void*, const char* trigger) { log_('U', C(table)); log_('u', hash_name(trigger)); return g_trigger_note; }
static void __fastcall d_post(void* manager, void*, const char* event, const char* argument, void* actor) {
    log_('P' << 8, C(manager)); log_('p', hash_name(event)); log_('p', hash_name(argument)); log_('p', C(actor));
}
static void __fastcall d_detach(void* controller, void*) { log_('X', C(controller)); }
static void __fastcall d_resize(void* slots, void*, unsigned n) { log_('Z', C(slots)); log_('z', n); }
static void __fastcall d_slot_dtor(void* slot, void*) { log_('Y', C(slot)); }
static void __fastcall d_release_item(void* item, void*, const int* args) { log_('I', C(item)); log_('i', args[0]); log_('i', args[1]); }
static void __fastcall d_remove_kind1(void* graph, void*, void* node) { log_('K', C(graph)); log_('k', C(node)); }
static void __fastcall d_node_slot28(void* node, void*) { log_('N' << 8 | 28, C(node)); }
static void __fastcall d_node_slot29(void* node, void*) { log_('N' << 8 | 29, C(node)); }
static int __fastcall d_actor_slot1(void* node, void*) { log_('Q' << 8 | 1, C(node)); return 0x11; }
static int __fastcall d_actor_slot2(void* node, void*) { log_('Q' << 8 | 2, C(node)); return 0x22; }
static void __fastcall d_actor_slot8(void* node, void*, int c, const char* name) { log_('Q' << 8 | 8, C(node)); log_('q', c); log_('q', hash_name(name)); }
static void __fastcall d_actor_slot40(void* node, void*, int a) { log_('Q' << 8 | 40, C(node)); log_('q', a); }
static void __fastcall d_actor_slot49(void* node, void*, const char* event, int b, int c) { log_('Q' << 8 | 49, C(node)); log_('q', hash_name(event)); log_('q', b); log_('q', c); }
// Seams whose original is a method and whose recovered form is a free function: the same double, called two ways.
static void __fastcall o_release_node(void* loader, void*, void* node, int flag) { log_('R' << 8, C(loader)); log_('r', C(node)); log_('r', flag); }
static void r_release_node(GmAcNode* node, int flag) { log_('R' << 8, C(GmDynamicNodeLoader::s_instance)); log_('r', C(node)); log_('r', flag); }
static int __fastcall o_vfx_has(void* manager, void*, const char* name) { log_('H', C(manager)); log_('h', hash_name(name)); return g_vfx_has; }
static int r_vfx_has(GmVfxManager* manager, const char* name) { log_('H', C(manager)); log_('h', hash_name(name)); return g_vfx_has; }
static void __fastcall o_play(void* audio, void*, void* actor, void* note) { log_('L', C(audio)); log_('l', C(actor)); log_('l', U(note)); }
static void __fastcall o_position(void* cs, void*, AtVector3* out) { log_('W', C(cs)); *out = g_position; }
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static void log_vector(const AtVector3* v) { log_('v', fbits(v->x)); log_('v', fbits(v->y)); log_('v', fbits(v->z)); }
static void __fastcall o_place(void* cs, void*, const AtVector3* fwd, int b, const AtVector3* pos, int d) { log_('O', C(cs)); log_vector(fwd); log_('o', b); log_vector(pos); log_('o', d); }

template <class M> static void* member_address(M m) { void* p; std::memcpy(&p, &m, sizeof p); return p; }
__declspec(noinline) static void destroy_slot(GmAcAnimSlot* s) { s->~GmAcAnimSlot(); }
static void* call_target(const void* fn) {
    const unsigned char* p = (const unsigned char*)fn;
    for (int i = 0; i < 64; i++) {
        if (p[i] == 0xE8 || p[i] == 0xE9) return (void*)(p + i + 5 + *(const int32_t*)(p + i + 1));
    }
    return nullptr;
}

// ---- both sides of one call --------------------------------------------------------------------------------------------
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    // A crash comes back without unwinding, so On's destructor may not have run: logging is stopped here either way.
    const bool oc = guarded([&] { On on(0); fo(); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    g_recording = false;
    g_side = 0;
    eq((std::string(what) + ": crashed").c_str(), oc, rc);
    compare_logs(what);
    return oc || rc;
}

// ---- objects of both sides -----------------------------------------------------------------------------------------------
enum { kActorSize = 0x2D8 };
static void* fresh(uint32_t size) {
    void* p = std::calloc(1, size + 16);
    std::memset(p, 0xCD, size + 16);
    return p;
}

// The host's vftables of the classes paired at start. Their deleting destructors and slots are called through these on the recovered side, as the original's are through its own.
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
}

// A copy of an object's vftable with some slots replaced, so that both sides' calls into slots not recovered land in the same doubles.
struct Patch { int slot; void* fn; };
static void** patched(void** vt, int slots, std::initializer_list<Patch> patches) {
    void** copy = (void**)std::calloc(slots, sizeof(void*));
    std::memcpy(copy, vt, slots * sizeof(void*));
    for (const Patch& p : patches) copy[p.slot] = p.fn;
    return copy;
}
// Both sides' copies get one token, so that the objects still compare.
static uint32_t g_patch_token = 0xC1000000u;
static void repoint(void* o, void* r, std::initializer_list<Patch> patches, int slots = 60) {
    *(void***)o = patched(vptr(o), slots, patches);
    *(void***)r = patched(vptr(r), slots, patches);
    g_vft_token[U(vptr(o))] = g_patch_token;
    g_host_vft[U(vptr(r))] = g_patch_token++;
}

// An actor of each side, built by its own constructor, with slots 2, 8, 40 and 49 going to doubles.
struct Actors {
    unsigned char* o;
    unsigned char* r;
    Actors(bool patch = true) {
        o = (unsigned char*)fresh(kActorSize);
        r = (unsigned char*)fresh(kActorSize);
        at<FnP0>(0x0014B5D0)(o, nullptr);
        new (r) GmAcActor;
        twin(o, r, kActorSize, 1);
        if (patch) {
            const std::initializer_list<Patch> p = {{2, (void*)d_actor_slot2}, {8, (void*)d_actor_slot8}, {40, (void*)d_actor_slot40}, {49, (void*)d_actor_slot49}};
            repoint(o, r, p);
        }
    }
    GmAcActor* R() { return (GmAcActor*)r; }
    template <class T> void set(uint32_t offset, T value) { *(T*)(o + offset) = value; *(T*)(r + offset) = value; }
    void compare(const char* what) { same_words(what, o, r, kActorSize); }
    ~Actors() { g_twins.clear(); }
};

static void call_both0(const char* what, Actors& a, int slot) {
    both(what, [&] { ((Fn0)vptr(a.o)[slot])(a.o, nullptr); }, [&] { ((Fn0)vptr(a.r)[slot])(a.r, nullptr); });
}

// ---- cases ---------------------------------------------------------------------------------------------------------------
static int g_cases;

// Construction of each class alone, and destruction by each destructor, on memory filled with 0xCD.
static void run_lifetimes() {
    struct Kind { const char* name; uint32_t size; uint32_t ctor, dtor, del; void (*host_ctor)(void*); void (*host_dtor)(void*); };
    static const Kind kinds[] = {
        {"GmBaObject", 0x18, 0x0014E550, 0x0014E5D0, 0x0014E730, [](void* p) { new (p) GmBaObject; }, [](void* p) { ((GmBaObject*)p)->GmBaObject::~GmBaObject(); }},
        {"GmAcNode", 0x14C, 0x0014CE40, 0x0014E250, 0x0014E4B0, [](void* p) { new (p) GmAcNode; }, [](void* p) { ((GmAcNode*)p)->GmAcNode::~GmAcNode(); }},
        {"GmAcActor", kActorSize, 0x0014B5D0, 0x0014B680, 0x0014B8E0, [](void* p) { new (p) GmAcActor; }, [](void* p) { ((GmAcActor*)p)->GmAcActor::~GmAcActor(); }},
        {"GmAcAnimController", 0x34, 0x001598B0, 0x00159B20, 0x00159CC0, [](void* p) { new (p) GmAcAnimController; }, [](void* p) { ((GmAcAnimController*)p)->GmAcAnimController::~GmAcAnimController(); }},
        {"AtKiAnimationCallBack", 8, 0x000B1590, 0x000B15A0, 0x000B15C0, [](void* p) { new (p) AtKiAnimationCallBack; }, [](void* p) { ((AtKiAnimationCallBack*)p)->AtKiAnimationCallBack::~AtKiAnimationCallBack(); }},
        {"GmLinkedListOf", 0x18, 0x00147EE0, 0x00148110, 0x00148170, [](void* p) { new (p) GmLinkedListOf<GmAcLinkedItem*>; }, [](void* p) { ((GmLinkedListOf<GmAcLinkedItem*>*)p)->GmLinkedListOf<GmAcLinkedItem*>::~GmLinkedListOf(); }},
    };
    for (const Kind& k : kinds) {
        for (int how = 0; how < 4; how++) {
            g_context = std::string(k.name) + " lifetime " + std::to_string(how);
            void* o = fresh(k.size);
            void* r = fresh(k.size);
            both("construct", [&] { at<FnP0>(k.ctor)(o, nullptr); }, [&] { k.host_ctor(r); });
            same_words("constructed", o, r, k.size);
            // GmBaObject's destructor only clears when initialized: try both.
            if ((how & 2) && k.ctor != 0x001598B0 && k.ctor != 0x000B1590 && k.ctor != 0x00147EE0) {
                both("initialize", [&] { ((FnI0)vptr(o)[1])(o, nullptr); }, [&] { ((FnI0)vptr(r)[1])(r, nullptr); });
            }
            if (how & 1) {
                // Deleting, flag 1: frees. The fresh blocks have 16 bytes of room that need not be freed by the right size.
                both("delete", [&] { ((FnDel)vptr(o)[0])(o, nullptr, 1); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, 1); });
            } else {
                both("destroy", [&] { at<Fn0>(k.dtor)(o, nullptr); }, [&] { k.host_dtor(r); });
                same_words("destroyed", o, r, k.size);
                if (k.ctor == 0x001598B0) {
                    // 0x001590D0, the abstract base's deleting destructor. Only vftable 0x0028EF44 names it, and an object holds that table only once a
                    // destructor has run (0x00159B20 stores it at 0x00159BDE, 0x001590C0 and 0x001590D0 themselves): nothing in the game deletes a controller
                    // already destroyed, so the game never calls it. Called here through each side's base table, which the destroyed controller now holds.
                    both("base deleting, flag 0", [&] { ((FnDel)vptr(o)[0])(o, nullptr, 0); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, 0); });
                    same_words("base deleted, flag 0", o, r, k.size);
                    both("base deleting, flag 1", [&] { ((FnDel)vptr(o)[0])(o, nullptr, 1); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, 1); });
                }
                void* o2 = fresh(k.size);
                void* r2 = fresh(k.size);
                at<FnP0>(k.ctor)(o2, nullptr);
                k.host_ctor(r2);
                both("deleting, flag 0", [&] { ((FnDel)vptr(o2)[0])(o2, nullptr, 0); }, [&] { ((FnDel)vptr(r2)[0])(r2, nullptr, 0); });
                same_words("deleted, flag 0", o2, r2, k.size);
            }
            g_cases++;
        }
    }
    // AtEvSubscriber's destructor alone, and the vector set's, on any memory.
    {
        g_context = "AtEvSubscriber destructor";
        void* o = fresh(8);
        void* r = fresh(8);
        both("destroy", [&] { at<Fn0>(0x000DF5B0)(o, nullptr); }, [&] { ((AtEvSubscriber*)r)->AtEvSubscriber::~AtEvSubscriber(); });
        same_words("destroyed", o, r, 8);
        g_context = "vector set destructor";
        void* vo = fresh(0x38);
        void* vr = fresh(0x38);
        both("destroy", [&] { at<Fn0>(0x0014B550)(vo, nullptr); }, [&] { ((GmAcVectorSet*)vr)->GmAcVectorSet::~GmAcVectorSet(); });
        same_words("destroyed", vo, vr, 0x38);
        for (unsigned flag : {0u, 1u}) {
            void* vo2 = fresh(0x38);
            void* vr2 = fresh(0x38);
            ((uint32_t*)vo2)[0] = 0x0028E30C;  // what the inlined constructor writes
            ((uint32_t*)vo2)[1] = 0;
            new (vr2) GmAcVectorSet;
            both("vector set deleting", [&] { ((FnDel)vptr(vo2)[0])(vo2, nullptr, flag); }, [&] { ((FnDel)vptr(vr2)[0])(vr2, nullptr, flag); });
            if (flag == 0) same_words("vector set deleted, flag 0", vo2, vr2, 0x38);
        }
        g_context = "callback";
        void* co = fresh(0x14);
        void* cr = fresh(0x14);
        both("construct base", [&] { at<FnP0>(0x000B1590)(co, nullptr); }, [&] { new (cr) AtKiAnimationCallBack; });
        Actors a(false);
        void* co2 = a.o + 0x168;
        void* cr2 = a.r + 0x168;
        a.set<uint32_t>(0x170, 0x11111111);
        both("callback destructor", [&] { at<Fn0>(0x000994D0)(co2, nullptr); }, [&] { ((GmAcAnimationCallback*)cr2)->GmAcAnimationCallback::~GmAcAnimationCallback(); });
        a.compare("after the callback's destructor");
        Actors b(false);
        both("callback deleting, flag 0", [&] { ((FnDel)vptr(b.o + 0x168)[0])(b.o + 0x168, nullptr, 0); }, [&] { ((FnDel)vptr(b.r + 0x168)[0])(b.r + 0x168, nullptr, 0); });
        b.compare("after the callback's deleting destructor");
        // The list's GmPlug-part thunk.
        Actors c(false);
        both("list thunk, flag 0", [&] { ((FnDel)vptr(c.o + 0x21C + 0x10)[0])(c.o + 0x21C + 0x10, nullptr, 0); }, [&] { ((FnDel)vptr(c.r + 0x21C + 0x10)[0])(c.r + 0x21C + 0x10, nullptr, 0); });
        c.compare("after the list's thunk");
        g_cases += 6;
    }
}

// GmBaObject's own slots, on a GmBaObject.
static void run_object() {
    for (int init = 0; init < 2; init++) {
        for (int desc = 0; desc < 2; desc++) {
            g_context = "GmBaObject init " + std::to_string(init) + " descriptor " + std::to_string(desc);
            void* o = fresh(0x18);
            void* r = fresh(0x18);
            at<FnP0>(0x0014E550)(o, nullptr);
            new (r) GmBaObject;
            static GmBaObject::Descriptor d = {{0, 0}, "objname", nullptr};
            for (void* p : {o, r}) {
                ((GmBaObject*)p)->m_initialized = init;
                ((GmBaObject*)p)->m_unknown04 = 4;
                ((GmBaObject*)p)->m_unknown08 = 8;
                ((GmBaObject*)p)->m_descriptor = desc ? &d : nullptr;
            }
            uint32_t ro = 0, rr = 0;
            both("name", [&] { ro = U(((FnP0)vptr(o)[6])(o, nullptr)); }, [&] { rr = U(((FnP0)vptr(r)[6])(r, nullptr)); });
            eq("name", ro, rr);
            both("is initialized", [&] { ro = ((FnI0)vptr(o)[5])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[5])(r, nullptr); });
            eq("is initialized", ro, rr);
            both("slot 2", [&] { ro = ((FnI0)vptr(o)[2])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[2])(r, nullptr); });
            eq("slot 2", ro, rr);
            both("slot 4", [&] { ((Fn0)vptr(o)[4])(o, nullptr); }, [&] { ((Fn0)vptr(r)[4])(r, nullptr); });
            both("initialize", [&] { ro = ((FnI0)vptr(o)[1])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[1])(r, nullptr); });
            eq("initialize", ro, rr);
            same_words("after initialize", o, r, 0x18);
            both("shutdown", [&] { ((Fn0)vptr(o)[3])(o, nullptr); }, [&] { ((Fn0)vptr(r)[3])(r, nullptr); });
            same_words("after shutdown", o, r, 0x18);
            for (void* p : {o, r}) {
                ((GmBaObject*)p)->m_initialized = init;
                ((GmBaObject*)p)->m_unknown04 = 4;
                ((GmBaObject*)p)->m_unknown08 = 8;
            }
            both("destroy", [&] { at<Fn0>(0x0014E5D0)(o, nullptr); }, [&] { ((GmBaObject*)r)->GmBaObject::~GmBaObject(); });
            same_words("after destroy", o, r, 0x18);
            g_cases++;
        }
    }
}

// The node's own methods, on an actor (its node part) and on a node.
static void run_node() {
    static GmBaObject::Descriptor d = {{0, 0}, "nodename", nullptr};
    static GmBaObject::Descriptor d9 = {{0, 0}, "123456789", nullptr};
    for (int which = 0; which < 2; which++) {
        g_context = std::string("node methods ") + (which ? "on an actor" : "on a node");
        unsigned char* o;
        unsigned char* r;
        uint32_t size;
        Actors a;
        if (which) {
            o = a.o; r = a.r; size = kActorSize;
        } else {
            size = 0x14C;
            o = (unsigned char*)fresh(size);
            r = (unsigned char*)fresh(size);
            at<FnP0>(0x0014CE40)(o, nullptr);
            new (r) GmAcNode;
            const std::initializer_list<Patch> p = {{2, (void*)d_actor_slot2}, {8, (void*)d_actor_slot8}};
            repoint(o, r, p, 45);
            g_twins.clear();
            twin(o, r, size, 1);
        }
        uint32_t ro = 0, rr = 0;
        both("slot 7, the real Initialize", [&] { ro = ((FnI1)vptr(o)[7])(o, nullptr, 5); }, [&] { rr = ((FnI1)vptr(r)[7])(r, nullptr, 5); });
        eq("slot 7", ro, rr);
        repoint(o, r, {{1, (void*)d_actor_slot1}}, which ? 60 : 45);
        both("slot 7", [&] { ro = ((FnI1)vptr(o)[7])(o, nullptr, 5); }, [&] { rr = ((FnI1)vptr(r)[7])(r, nullptr, 5); });
        eq("slot 7", ro, rr);
        for (GmBaObject::Descriptor* dd : {&d, &d9}) {
            both("setup", [&] { ((Fn4)vptr(o)[9])(o, nullptr, 0x1234, 77, 99, (uintptr_t)dd); }, [&] { ((Fn4)vptr(r)[9])(r, nullptr, 0x1234, 77, 99, (uintptr_t)dd); });
            same_words("after setup", o, r, size);
        }
        for (uint32_t flags : {0u, 0x100u, 0xFFFFFFFFu, 0x10u, 0xFFFFFEFFu}) {
            *(uint32_t*)(o + 0xA4) = flags;
            *(uint32_t*)(r + 0xA4) = flags;
            both("slot 13", [&] { ro = ((FnI0)vptr(o)[13])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[13])(r, nullptr); });
            eq("slot 13", ro, rr);
            if (!which) {
                for (uintptr_t on : {(uintptr_t)0, (uintptr_t)1, (uintptr_t)0x80000000u}) {
                    both("set flag 0x10", [&] { ((Fn1)vptr(o)[26])(o, nullptr, on); }, [&] { ((Fn1)vptr(r)[26])(r, nullptr, on); });
                    same_words("after set flag 0x10", o, r, size);
                }
            }
        }
        both("set +0x140", [&] { at<Fn1>(0x0014BA90)(o, nullptr, 0x77); }, [&] { ((GmAcNode*)r)->SetUnknown140(0x77); });
        both("animation", [&] { ro = U(at<FnP0>(0x0014BAB0)(o, nullptr)); }, [&] { rr = U(((GmAcNode*)r)->Animation()); });
        eq("animation", canon(0, ro, o, size), canon(1, rr, r, size));
        both("setup and configure", [&] { ro = at<FnI2>(0x0014BB00)(o, nullptr, 0x55, (uintptr_t)&d); }, [&] { rr = ((GmAcNode*)r)->SetupAndConfigure((void*)0x55, &d); });
        eq("setup and configure", ro, rr);
        same_words("after the node's methods", o, r, size);
        // The empty slots.
        for (int s : {14, 18, 21, 37}) {
            both("empty slot", [&] { ((Fn1)vptr(o)[s])(o, nullptr, 1); }, [&] { ((Fn1)vptr(r)[s])(r, nullptr, 1); });
        }
        if (!which) {
            both("empty slot 38", [&] { ((Fn1)vptr(o)[38])(o, nullptr, 1); }, [&] { ((Fn1)vptr(r)[38])(r, nullptr, 1); });
            for (int s : {40, 41}) both("empty slot", [&] { ((Fn1)vptr(o)[s])(o, nullptr, 1); }, [&] { ((Fn1)vptr(r)[s])(r, nullptr, 1); });
            both("empty slot 16", [&] { ((Fn0)vptr(o)[16])(o, nullptr); }, [&] { ((Fn0)vptr(r)[16])(r, nullptr); });
        }
        both("slot 36", [&] { ((Fn2)vptr(o)[36])(o, nullptr, 1, 2); }, [&] { ((Fn2)vptr(r)[36])(r, nullptr, 1, 2); });
        both("slot 42", [&] { ro = ((FnI0)vptr(o)[42])(o, nullptr); }, [&] { rr = ((FnI0)vptr(r)[42])(r, nullptr); });
        eq("slot 42", ro, rr);
        same_words("after the empty slots", o, r, size);
        // Reset on a dirty object.
        std::memset(o + 4, 0x5A, size - 4);
        std::memset(r + 4, 0x5A, size - 4);
        both("reset", [&] { at<Fn0>(0x0014BC60)(o, nullptr); }, [&] { ((GmAcNode*)r)->Reset(); });
        same_words("after reset", o, r, size);
        g_cases++;
    }
}

// The animation controller: its slots pass to the owner's 40 and 41; Clear and the destructor with and without an owner and slots.
static void run_controller() {
    for (int owner = 0; owner < 2; owner++) {
        for (int slots = 0; slots < 4; slots++) {
            for (int how = 0; how < 2; how++) {
                g_context = "controller owner " + std::to_string(owner) + " slots " + std::to_string(slots) + (how ? " clear" : " destroy");
                void* o = fresh(0x34);
                void* r = fresh(0x34);
                at<FnP0>(0x001598B0)(o, nullptr);
                new (r) GmAcAnimController;
                same_words("constructed", o, r, 0x34);
                Fake* node = new_fake(g_vt_node, 7);
                twin(node, node, sizeof(Fake), 7);
                twin(o, r, 0x34, 2);
                uint32_t* blocks[2] = {nullptr, nullptr};
                for (void* p : {o, r}) {
                    GmAcAnimController* c = (GmAcAnimController*)p;
                    c->m_owner = owner ? (GmAcNode*)node : nullptr;
                    c->m_unknown04 = 3;
                    c->m_unknown20 = 1;
                    c->m_unknown21 = 2;
                    c->m_unknown30 = 9;
                    c->m_blend.m_rotations = (AtSceneAnimBlend::Rotation*)std::calloc(1, 16);
                    if (slots) {
                        uint32_t* block = (uint32_t*)std::calloc(1, 4 + 0x20 * slots);
                        block[0] = slots - 1;  // 0, 1 or 2 elements
                        c->m_slots.m_data = (GmAcAnimSlot*)(block + 1);
                        c->m_slots.m_count = 5;
                        blocks[p == o ? 0 : 1] = block;
                    }
                }
                if (slots) twin(blocks[0], blocks[1], 4 + 0x20 * slots, 3);
                twin(((GmAcAnimController*)o)->m_blend.m_rotations, ((GmAcAnimController*)r)->m_blend.m_rotations, 16, 4);
                if (owner) {
                    both("slot 1", [&] { ((Fn1)vptr(o)[1])(o, nullptr, 11); }, [&] { ((Fn1)vptr(r)[1])(r, nullptr, 11); });
                    both("slot 2", [&] { ((Fn1)vptr(o)[2])(o, nullptr, 12); }, [&] { ((Fn1)vptr(r)[2])(r, nullptr, 12); });
                }
                if (how) {
                    both("clear", [&] { at<Fn0>(0x00159910)(o, nullptr); }, [&] { ((GmAcAnimController*)r)->Clear(); });
                    same_words("after clear", o, r, 0x34);
                } else {
                    both("destroy", [&] { at<Fn0>(0x00159B20)(o, nullptr); }, [&] { ((GmAcAnimController*)r)->GmAcAnimController::~GmAcAnimController(); });
                    same_words("after the destructor", o, r, 0x34);
                }
                g_twins.clear();
                g_cases++;
            }
        }
    }
}

// The node's shutdown, with each thing it may hold.
static void run_node_shutdown() {
    for (int mask = 0; mask < 128; mask++) {
        g_context = "node shutdown " + std::to_string(mask) + ((mask & 64) ? " by the node's destructor" : "");
        Actors a;
        Fake* controller_owner = new_fake(g_vt_node, 25);
        // Each side's own copies of what is written: link, owner and its sockets, scene node; the deletable and the material assignment are freed.
        Fake* link[2];
        Fake* owner[2];
        Fake* sockets[2][2];
        unsigned char* scene[2];
        for (int s = 0; s < 2; s++) {
            link[s] = new_fake(g_vt_del1, 20);
            owner[s] = new_fake(g_vt_del1, 21);
            for (int k = 0; k < 2; k++) {
                sockets[s][k] = (Fake*)((char*)owner[s] + 0x14 + 0x18 * k);
                sockets[s][k]->vptr = g_vt_socket;
                sockets[s][k]->id = 26 + k;
            }
            scene[s] = (unsigned char*)fresh(0x70);
        }
        twin(link[0], link[1], sizeof(Fake), 20);
        twin(owner[0], owner[1], sizeof(Fake), 21);
        twin(scene[0], scene[1], 0x70, 22);
        unsigned char* objs[2] = {a.o, a.r};
        for (int s = 0; s < 2; s++) {
            GmAcActor* x = (GmAcActor*)objs[s];
            GmAcNodeLink* l = (GmAcNodeLink*)link[s];
            l->m_unknown08 = 0x81;
            l->m_unknown0C = 0x82;
            l->m_unknown14 = 0x84;
            l->m_unknown18 = 0x85;
            if (mask & 32) x->m_animation.m_owner = (GmAcNode*)controller_owner;
            if (mask & 1) {
                x->m_link = (GmAcNodeLink*)link[s];
                if (mask & 2) ((GmAcNodeLink*)link[s])->m_owner = (GmAcNodeLinkOwner*)owner[s];
                else ((GmAcNodeLink*)link[s])->m_owner = nullptr;
            }
            if (mask & 4) {
                AtMatAssignment* m = (AtMatAssignment*)std::calloc(1, sizeof(AtMatAssignment));
                x->m_matAssignment = m;
            }
            if (mask & 8) x->m_unknown13C = (GmAcOwned*)new_fake(g_vt_del1, 23);
            if (mask & 16) x->m_sceneNode = (AtSceneNode*)scene[s];
            x->m_initialized = 1;
            x->m_unknown30[2] = 0x33;
            x->m_unknown144 = 0x44;
            x->m_unknown148 = 0x48;
        }
        if (mask & 64) both("node destructor", [&] { at<Fn0>(0x0014E250)(a.o, nullptr); }, [&] { a.R()->GmAcNode::~GmAcNode(); });
        else both("node shutdown", [&] { at<Fn0>(0x0014DC60)(a.o, nullptr); }, [&] { a.R()->GmAcNode::Shutdown(); });
        a.compare("after the node's shutdown");
        same_words("link", link[0], link[1], 0x20, link[0], link[1], sizeof(Fake));
        same_words("scene node", scene[0], scene[1], 0x70);
        g_cases++;
    }
}

// The actor's reset, shutdown, and destructor, with what each may hold.
static void run_actor_shutdown() {
    for (int mask = 0; mask < 512; mask++) {
        for (int how = 0; how < 2; how++) {
            g_context = "actor shutdown " + std::to_string(mask) + (how ? " by the destructor" : "");
            Actors a;
            Fake* component = new_fake(g_vt_del1, 30);
            Fake* attached = new_fake(g_vt_node, 31);
            Fake* scene = new_fake(g_vt_scene, 32);
            Fake* deletable = new_fake(g_vt_del1, 33);
            Fake* u218 = new_fake(g_vt_del1, 34);
            Fake* motions[3] = {new_fake(g_vt_del1, 35), nullptr, new_fake(g_vt_del1, 36)};
            for (Fake* f : {component, attached, scene, deletable, u218, motions[0], motions[2]}) twin(f, f, sizeof(Fake), f->id);
            unsigned char* objs[2] = {a.o, a.r};
            if (mask & 256) {
                unsigned char* item[2];
                for (int s = 0; s < 2; s++) {
                    item[s] = (unsigned char*)fresh(0x28);
                    if (s == 0) at<FnP0>(0x00076C10)(item[s], nullptr);
                    else new (item[s]) GmPlug;
                    unsigned char* list = objs[s] + 0x21C;
                    ((Fn1)vptr(list)[1])(list, nullptr, (uintptr_t)item[s]);
                }
                twin(item[0], item[1], 0x28, 39);
            }
            unsigned int count = 3;
            for (int s = 0; s < 2; s++) {
                GmAcActor* x = (GmAcActor*)objs[s];
                x->m_initialized = 1;
                if (mask & 1) x->m_component = (GmAcComponent*)component;
                if (mask & 2) x->m_attached = (GmAcNode*)attached;
                if (mask & 4) x->m_sceneActor = (AtSceneNode*)scene;
                if (mask & 8) x->m_unknown270 = (GmAcOwned*)deletable;
                if (mask & 16) x->m_unknown218 = (GmAcUnknown218*)u218;
                if (mask & 32) {
                    GmAcMotion** list = (GmAcMotion**)std::calloc(3, sizeof(void*));
                    for (int i = 0; i < 3; i++) list[i] = (GmAcMotion*)motions[i];
                    x->m_motions = list;
                    x->m_motionCount = (mask & 64) ? 3 : 2;
                }
                if (mask & 128) {
                    x->m_records = (GmAcActorRecord*)std::calloc(4, sizeof(GmAcActorRecord));
                    std::memset(x->m_records, 0x77, 4 * sizeof(GmAcActorRecord));
                    x->m_recordCount = (mask & 64) ? &count : nullptr;
                }
                x->m_callback.m_actor = x;
                x->m_vectors184.m_active = 1;
                x->m_vectors184.m_vectors[1][2] = 2.0f;
                x->m_vectors1BC.m_vectors[3][0] = 3.0f;
                x->m_unknown24C = 5.0f;
                x->m_unknown2D4 = 7.0f;
                x->m_flags98 = 0x10;
            }
            if (how) {
                both("destroy", [&] { at<Fn0>(0x0014B680)(a.o, nullptr); }, [&] { a.R()->GmAcActor::~GmAcActor(); });
            } else {
                both("shutdown", [&] { ((Fn0)vptr(a.o)[3])(a.o, nullptr); }, [&] { ((Fn0)vptr(a.r)[3])(a.r, nullptr); });
            }
            a.compare("after");
            g_cases++;
        }
    }
    // Reset alone, on a dirty actor.
    {
        g_context = "actor reset";
        Actors a;
        std::memset(a.o + 0x14C, 0x5A, kActorSize - 0x14C);
        std::memset(a.r + 0x14C, 0x5A, kActorSize - 0x14C);
        a.set<uint32_t>(0x98, 0x12345678);
        both("reset", [&] { at<Fn0>(0x001481A0)(a.o, nullptr); }, [&] { a.R()->ResetActor(); });
        a.compare("after reset");
        g_cases++;
    }
    // Records alone: none, three by default, a count, a count of zero.
    for (int k = 0; k < 4; k++) {
        g_context = "records " + std::to_string(k);
        Actors a;
        unsigned int counts[] = {0, 0, 4, 0};
        GmAcActorRecord* rec[2] = {nullptr, nullptr};
        for (int s = 0; s < 2; s++) {
            if (k) {
                rec[s] = (GmAcActorRecord*)std::calloc(5, sizeof(GmAcActorRecord));
                std::memset(rec[s], 0x66, 5 * sizeof(GmAcActorRecord));
            }
            GmAcActor* x = (GmAcActor*)(s ? a.r : a.o);
            x->m_records = rec[s];
            x->m_recordCount = k >= 2 ? &counts[k] : nullptr;
        }
        both("clear records", [&] { at<Fn0>(0x00148EF0)(a.o, nullptr); }, [&] { a.R()->ClearRecords(); });
        if (k) same_words("records", rec[0], rec[1], 5 * sizeof(GmAcActorRecord));
        g_cases++;
    }
}

// The items of the list at +0x21C: real items on real lists, each side's own.
static void run_items() {
    for (int n = 0; n < 4; n++) {
        for (uintptr_t flag : {(uintptr_t)0, (uintptr_t)1, (uintptr_t)7}) {
            g_context = "release items " + std::to_string(n) + " flag " + std::to_string(flag);
            Actors a;
            unsigned char* items[2][3];
            for (int s = 0; s < 2; s++) {
                for (int i = 0; i < n; i++) {
                    items[s][i] = (unsigned char*)fresh(0x28);
                    if (s == 0) at<FnP0>(0x00076C10)(items[s][i], nullptr);
                    else new (items[s][i]) GmPlug;
                    *(uint32_t*)(items[s][i] + 0x24) = 0x99;
                    unsigned char* list = (s ? a.r : a.o) + 0x21C;
                    ((Fn1)vptr(list)[1])(list, nullptr, (uintptr_t)items[s][i]);
                }
            }
            for (int i = 0; i < n; i++) twin(items[0][i], items[1][i], 0x28, 40 + i);
            unsigned int two = 2;
            GmAcActorRecord* rec[2];
            for (int s = 0; s < 2; s++) {
                rec[s] = (GmAcActorRecord*)std::calloc(3, sizeof(GmAcActorRecord));
                std::memset(rec[s], 0x55, 3 * sizeof(GmAcActorRecord));
                ((GmAcActor*)(s ? a.r : a.o))->m_records = rec[s];
                ((GmAcActor*)(s ? a.r : a.o))->m_recordCount = &two;
            }
            twin(rec[0], rec[1], 3 * sizeof(GmAcActorRecord), 45);
            both("release items", [&] { at<Fn1>(0x0014A630)(a.o, nullptr, flag); }, [&] { a.R()->ReleaseItems((int)flag); });
            a.compare("after");
            for (int i = 0; i < n; i++) same_words("item", items[0][i], items[1][i], 0x28);
            same_words("records", rec[0], rec[1], 3 * sizeof(GmAcActorRecord));
            g_cases++;
        }
    }
}

// The actor's small slots.
static void run_actor_slots() {
    static GmBaObject::Descriptor alias = {{0, 0}, "alias", nullptr};
    static GmBaObject::Descriptor plain = {{0, 0}, "plain", nullptr};
    static GmBaObject::Descriptor aliased = {{0, 0}, "aliased", &alias};
    static int zero = 0, one = 1, minus = -1;
    static unsigned int values[] = {0, 1, 2, 16777217, 0x7FFFFFFF, 0x80000000u, 0x80000001u, 0xFFFFFFFFu, 12345};
    for (int k = 0; k < 64; k++) {
        g_context = "actor slots " + std::to_string(k);
        Actors a;
        Fake* component = new_fake(g_vt_del1, 50);
        Fake* audio = new_fake(g_vt_del1, 51);
        Fake* attached = new_fake(g_vt_node, 52);
        for (Fake* f : {component, audio, attached}) twin(f, f, sizeof(Fake), f->id);
        ((GmAcComponent*)component)->m_audio = (k & 2) ? (GmAcAudio*)audio : nullptr;
        GmBaObject::Descriptor* descs[] = {nullptr, &plain, &aliased, &alias};
        int* ptrs[] = {nullptr, &zero, &one, &minus};
        a.set<uint32_t>(0x1FC, (k & 4) ? 1 : 0);
        a.set<uint32_t>(0x200, (k & 8) ? 1 : 0);
        a.set<uint32_t>(0x204, (k & 16) ? 1 : 0);
        a.set<uint32_t>(0xA4, (k & 32) ? 2 : 0x1FD);
        a.set<int*>(0x254, ptrs[k & 3]);
        a.set<int*>(0x278, ptrs[(k >> 2) & 3]);
        a.set<int*>(0x274, ptrs[(k >> 4) & 3]);
        a.set<unsigned int*>(0x27C, (k & 1) ? &values[k % 9] : nullptr);
        a.set<uint32_t>(0x17C, 0x1000 + k);
        a.set<GmBaObject::Descriptor*>(0x14, descs[k & 3]);
        a.set<GmAcComponent*>(0x20C, (k & 1) ? (GmAcComponent*)component : nullptr);
        a.set<GmAcNode*>(0x1F8, (k & 2) ? (GmAcNode*)attached : nullptr);
        uint32_t ro = 0, rr = 0;
        for (int s : {6, 27, 50, 51, 53, 55, 56, 57, 59}) {
            both("slot", [&] { ro = ((FnI0)vptr(a.o)[s])(a.o, nullptr); }, [&] { rr = ((FnI0)vptr(a.r)[s])(a.r, nullptr); });
            char text[32];
            sprintf_s(text, "slot %d", s);
            eq(text, ro, rr);
        }
        both("has +0x254", [&] { ro = at<FnI0>(0x000918C0)(a.o, nullptr); }, [&] { rr = a.R()->HasUnknown254(); });
        eq("has +0x254", ro, rr);
        float fo = 0, fr = 0;
        both("slot 52", [&] { fo = ((FnF0)vptr(a.o)[52])(a.o, nullptr); }, [&] { fr = ((FnF0)vptr(a.r)[52])(a.r, nullptr); });
        eq("slot 52", fbits(fo), fbits(fr));
        int out_o[3] = {5, 5, 5}, out_r[3] = {5, 5, 5};
        both("slot 54", [&] { ((Fn1)vptr(a.o)[54])(a.o, nullptr, (uintptr_t)out_o); }, [&] { ((Fn1)vptr(a.r)[54])(a.r, nullptr, (uintptr_t)out_r); });
        eq("slot 54", std::memcmp(out_o, out_r, sizeof out_o), 0);
        both("slot 44", [&] { ro = ((FnI1)vptr(a.o)[44])(a.o, nullptr, 3); }, [&] { rr = ((FnI1)vptr(a.r)[44])(a.r, nullptr, 3); });
        eq("slot 44", ro, rr);
        both("slot 16", [&] { ((Fn0)vptr(a.o)[16])(a.o, nullptr); }, [&] { ((Fn0)vptr(a.r)[16])(a.r, nullptr); });
        both("slot 28", [&] { ((Fn0)vptr(a.o)[28])(a.o, nullptr); }, [&] { ((Fn0)vptr(a.r)[28])(a.r, nullptr); });
        both("slot 29", [&] { ((Fn0)vptr(a.o)[29])(a.o, nullptr); }, [&] { ((Fn0)vptr(a.r)[29])(a.r, nullptr); });
        both("slot 38", [&] { ((Fn1)vptr(a.o)[38])(a.o, nullptr, 0x38); }, [&] { ((Fn1)vptr(a.r)[38])(a.r, nullptr, 0x38); });
        both("slot 41", [&] { ((Fn1)vptr(a.o)[41])(a.o, nullptr, 0x41); }, [&] { ((Fn1)vptr(a.r)[41])(a.r, nullptr, 0x41); });
        both("start collision", [&] { ((Fn2)vptr(a.o)[45])(a.o, nullptr, 0x45, 0x46); }, [&] { ((Fn2)vptr(a.r)[45])(a.r, nullptr, 0x45, 0x46); });
        both("end collision", [&] { ((Fn1)vptr(a.o)[46])(a.o, nullptr, 0x47); }, [&] { ((Fn1)vptr(a.r)[46])(a.r, nullptr, 0x47); });
        for (uintptr_t on : {(uintptr_t)0, (uintptr_t)1, (uintptr_t)2}) {
            both("set flag 0x10", [&] { ((Fn1)vptr(a.o)[26])(a.o, nullptr, on); }, [&] { ((Fn1)vptr(a.r)[26])(a.r, nullptr, on); });
        }
        for (int t = 0; t < 3; t++) {
            both("set +0x284", [&] { ((Fn1)vptr(a.o)[58])(a.o, nullptr, t == 0 ? U(a.o) : t == 1 ? 0 : 0x5000); },
                 [&] { ((Fn1)vptr(a.r)[58])(a.r, nullptr, t == 0 ? U(a.r) : t == 1 ? 0 : 0x5000); });
            a.compare("after set +0x284");
        }
        both("forget component", [&] { ((Fn0)vptr(a.o)[48])(a.o, nullptr); }, [&] { ((Fn0)vptr(a.r)[48])(a.r, nullptr); });
        a.compare("after the slots");
        g_cases++;
    }
}

// The factories, with and without a heap, failing allocation, and counts around 1.
static void run_factories() {
    static int heap_token;
    for (int which = 0; which < 2; which++) {
        for (int count : {-1, 0, 1, 2, 3}) {
            for (int trouble = 0; trouble < 3; trouble++) {
                g_context = std::string(which ? "create node " : "create rotator ") + std::to_string(count) + " trouble " + std::to_string(trouble);
                void* heap = trouble == 1 ? nullptr : &heap_token;
                *(void**)(uintptr_t)0x003B3930 = heap;
                GmAcActor::SetHeap(heap);
                g_fail_alloc = false;
                void* o = nullptr;
                void* r = nullptr;
                const bool crashed = both("create",
                    [&] { g_fail_alloc = trouble == 2; o = at<FnCreate>(which ? 0x0014B950 : 0x0014B7A0)(count); g_fail_alloc = false; },
                    [&] { g_fail_alloc = trouble == 2; r = which ? GmAcActor::CreateNode(count) : GmAcActor::CreateRotator(count); g_fail_alloc = false; });
                eq("created", o != nullptr, r != nullptr);
                if (crashed || !o || !r) { g_cases++; continue; }
                const int n = count > 1 ? count : 1;
                if (count > 1) eq("count ahead", ((int*)o)[-1], ((int*)r)[-1]);
                for (int i = 0; i < n; i++) same_words("created actor", (char*)o + i * kActorSize, (char*)r + i * kActorSize, kActorSize, o, r, n * kActorSize);
                // Back as made: one by flag 1, an array by flag 3, through slot 0.
                const unsigned flags = count > 1 ? 3 : 1;
                both("give back", [&] { ((FnDel)vptr(o)[0])(o, nullptr, flags); }, [&] { ((FnDel)vptr(r)[0])(r, nullptr, flags); });
                g_cases++;
            }
        }
    }
    *(void**)(uintptr_t)0x003B3930 = &heap_token;
    GmAcActor::SetHeap(&heap_token);
    both("set heap", [&] { at<void(__cdecl*)(void*)>(0x0014B940)((void*)0x1234); }, [&] { GmAcActor::SetHeap((void*)0x1234); });
    eq("heap", *(uint32_t*)(uintptr_t)0x003B3930, U(GmAcActor::s_heap));
    // GmBaObject's arrays: its deleting destructor handles flag 2, by a count ahead of 0x18-byte elements.
    for (int n : {0, 1, 3}) {
        g_context = "GmBaObject array " + std::to_string(n);
        uint32_t* bo = (uint32_t*)std::calloc(1, 4 + 0x18 * n + 16);
        uint32_t* br = (uint32_t*)std::calloc(1, 4 + 0x18 * n + 16);
        bo[0] = br[0] = n;
        for (int i = 0; i < n; i++) {
            at<FnP0>(0x0014E550)((char*)(bo + 1) + 0x18 * i, nullptr);
            new ((char*)(br + 1) + 0x18 * i) GmBaObject;
            ((GmBaObject*)((char*)(bo + 1) + 0x18 * i))->m_initialized = i & 1;
            ((GmBaObject*)((char*)(br + 1) + 0x18 * i))->m_initialized = i & 1;
        }
        if (n == 0) { g_cases++; continue; }  // no element to ask
        twin(bo, br, 4 + 0x18 * n, 90);
        both("array, flag 2", [&] { ((FnDel)vptr(bo + 1)[0])(bo + 1, nullptr, 2); }, [&] { ((FnDel)vptr(br + 1)[0])(br + 1, nullptr, 2); });
        same_words("array after flag 2", bo, br, 4 + 0x18 * n);
        g_twins.clear();
        g_cases++;
    }
}

// ---- the animation callback ----------------------------------------------------------------------------------------------
static GmAudioData* g_audio;
static GmSoundNote g_notes[2][4];

static void build_audio() {
    g_audio = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) new (&g_audio->m_noteBanks[i]) GmSoundNoteBank;
    static const char* const triggers[2][4] = {{"t1", "t2", "ev", "12345678"}, {"t1", "t3", "ev2", "x"}};
    static const char* const banks[2] = {"bankA", "bankB"};
    for (int b = 0; b < 2; b++) {
        GmSoundNoteBank* bank = &g_audio->m_noteBanks[b * 3 + 1];
        std::memset(bank->m_name, 0, 8);
        std::memcpy(bank->m_name, banks[b], std::strlen(banks[b]));
        bank->m_byTrigger.Init(16);
        bank->m_byName.Init(16);
        for (int i = 0; i < 4; i++) {
            GmSoundNote* n = &g_notes[b][i];
            std::memset(n, 0, sizeof *n);
            std::memcpy(n->m_trigger, triggers[b][i], std::strlen(triggers[b][i]) > 8 ? 8 : std::strlen(triggers[b][i]));
            if (i == 2) { std::memcpy(n->m_gameEvent, "event", 5); std::memcpy(n->m_gameEventArgument, b ? "" : "arg", b ? 0 : 3); }
            bank->m_byTrigger.Insert(n->m_trigger, n);
            std::memcpy(n->m_name, n->m_trigger, 8);
            bank->m_byName.Insert(n->m_name, n);
        }
        bank->m_notes = g_notes[b];
        bank->m_noteCount = 4;
    }
    *(GmAudioData**)(uintptr_t)0x003B354C = g_audio;
    GmAudioData::s_instance = g_audio;
}

static void set_banks_in_use(bool on) {
    g_audio->m_noteBanks[1].m_notes = on ? g_notes[0] : nullptr;
    g_audio->m_noteBanks[4].m_notes = on ? g_notes[1] : nullptr;
}

static void run_callback() {
    static Fake triggers_table;  // only its address is used: the trigger table's methods are seams
    static Fake component, audio;
    triggers_table.id = 60;
    component.id = 61;
    audio.id = 62;
    static const char* const names[] = {"t1", "t2", "ev", "ev2", "none", "12345678x"};
    static const char* const firsts[] = {"", "fx", "fx"};
    static const char* const seconds[] = {"", "", "tgt"};
    static const char* const actor_banks[] = {"bankA", "bankB", "", "zz"};
    static const char* const got_banks[] = {"bankB", "", "zz"};
    for (int slot27 = 0; slot27 < 2; slot27++)
    for (int table = 0; table < 2; table++)
    for (int tr = 0; tr < 3; tr++)
    for (int vfx = 0; vfx < 2; vfx++)
    for (int comp = 0; comp < 3; comp++)
    for (int tnote = 0; tnote < 2; tnote++)
    for (int ab = 0; ab < 4; ab++) {
        if (!table && (tr || tnote)) continue;
        if (comp != 2 && tnote) continue;
        for (int nm = 0; nm < 6; nm++) {
            const int gb = (nm + ab) % 3;
            g_context = "trigger [" + std::string(names[nm]) + "] slot27 " + std::to_string(slot27) + " table " + std::to_string(table) + " effect " + std::to_string(tr) + " vfx " +
                        std::to_string(vfx) + " component " + std::to_string(comp) + " table note " + std::to_string(tnote) + " bank " + actor_banks[ab];
            Actors a;
            twin(&triggers_table, &triggers_table, sizeof(Fake), 60);
            twin(&component, &component, sizeof(Fake), 61);
            twin(&audio, &audio, sizeof(Fake), 62);
            ((GmAcComponent*)&component)->m_audio = comp == 2 ? (GmAcAudio*)&audio : nullptr;
            a.set<uint32_t>(0xA4, slot27 ? 2 : 0);
            a.set<GmAcTriggerTable*>(0x164, table ? (GmAcTriggerTable*)&triggers_table : nullptr);
            a.set<GmAcComponent*>(0x20C, comp ? (GmAcComponent*)&component : nullptr);
            char bank[8] = {0};
            std::memcpy(bank, actor_banks[ab], std::strlen(actor_banks[ab]));
            for (int i = 0; i < 8; i++) a.set<char>(0x90 + i, bank[i]);
            std::memset(g_trigger_first, 0, 8); std::memcpy(g_trigger_first, firsts[tr], std::strlen(firsts[tr]));
            std::memset(g_trigger_second, 0, 8); std::memcpy(g_trigger_second, seconds[tr], std::strlen(seconds[tr]));
            std::memset(g_note_bank, 0, 8); std::memcpy(g_note_bank, got_banks[gb], std::strlen(got_banks[gb]));
            g_trigger_note = tnote ? &g_notes[nm & 1][2] : nullptr;
            g_vfx_has = vfx;
            *(GmAcActor**)(a.o + 0x170) = (GmAcActor*)a.o;
            *(GmAcActor**)(a.r + 0x170) = (GmAcActor*)a.r;
            both("on trigger", [&] { ((Fn3)vptr(a.o + 0x168)[4])(a.o + 0x168, nullptr, (uintptr_t)names[nm], 1, 2); },
                 [&] { ((Fn3)vptr(a.r + 0x168)[4])(a.r + 0x168, nullptr, (uintptr_t)names[nm], 1, 2); });
            if (nm == 0) {
                both("on loop", [&] { ((Fn1)vptr(a.o + 0x168)[3])(a.o + 0x168, nullptr, 9); }, [&] { ((Fn1)vptr(a.r + 0x168)[3])(a.r + 0x168, nullptr, 9); });
            }
            a.compare("after");
            g_cases++;
        }
    }
    // No actor: nothing without both node and place; with them, FindNote with a null bank name, which finds nothing when no bank is in use and crashes when one is.
    static unsigned char node[0x70];
    static Fake place;
    for (int k = 0; k < 8; k++) {
        g_context = "no actor " + std::to_string(k);
        Actors a(false);
        twin(node, node, sizeof node, 70);
        twin(&place, &place, sizeof place, 71);
        a.set<void*>(0x174, (k & 1) ? node : nullptr);
        a.set<void*>(0x178, (k & 2) ? &place : nullptr);
        set_banks_in_use((k & 4) != 0);
        both("on trigger", [&] { ((Fn3)vptr(a.o + 0x168)[4])(a.o + 0x168, nullptr, (uintptr_t)"t1", 0, 0); }, [&] { ((Fn3)vptr(a.r + 0x168)[4])(a.r + 0x168, nullptr, (uintptr_t)"t1", 0, 0); });
        both("on loop", [&] { ((Fn1)vptr(a.o + 0x168)[3])(a.o + 0x168, nullptr, 0); }, [&] { ((Fn1)vptr(a.r + 0x168)[3])(a.r + 0x168, nullptr, 0); });
        set_banks_in_use(true);
        g_cases++;
    }
    // The base's slots, which the callback does not override.
    {
        g_context = "callback base slots";
        Actors a(false);
        uint32_t ro = 0, rr = 0;
        for (int s : {1, 2, 6}) both("empty slot", [&] { ((Fn1)vptr(a.o + 0x168)[s])(a.o + 0x168, nullptr, 1); }, [&] { ((Fn1)vptr(a.r + 0x168)[s])(a.r + 0x168, nullptr, 1); });
        both("slot 5", [&] { ro = ((FnI1)vptr(a.o + 0x168)[5])(a.o + 0x168, nullptr, 1); }, [&] { rr = ((FnI1)vptr(a.r + 0x168)[5])(a.r + 0x168, nullptr, 1); });
        eq("slot 5", ro, rr);
        void* bo = fresh(8);
        void* br = fresh(8);
        at<FnP0>(0x000B1590)(bo, nullptr);
        new (br) AtKiAnimationCallBack;
        both("base on trigger", [&] { ((Fn3)vptr(bo)[4])(bo, nullptr, (uintptr_t)"t1", 0, 0); }, [&] { ((Fn3)vptr(br)[4])(br, nullptr, (uintptr_t)"t1", 0, 0); });
        both("base on loop", [&] { ((Fn1)vptr(bo)[3])(bo, nullptr, 0); }, [&] { ((Fn1)vptr(br)[3])(br, nullptr, 0); });
        same_words("base", bo, br, 8);
        g_cases++;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("GMACTOR_TRACE")) std::printf("crash at 0x%p eax=%08X ecx=%08X esi=%08X [%s]\n", e->ExceptionRecord->ExceptionAddress, (unsigned)e->ContextRecord->Eax, (unsigned)e->ContextRecord->Ecx, (unsigned)e->ContextRecord->Esi, g_context.c_str());
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
    if (pos.size() < 1) { std::printf("usage: oracle_gmactor <default.xbe> [cases dir] [--entries FILE] [--mutate]\n"); return 2; }
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
    // The original's pool of list nodes is constructed by the game's start-up, which does not run here.
    for (int i = 0; i < GmLinkedListNode::kPoolSize; i++) at<Fn0>(0x00076D20)((void*)(uintptr_t)(0x002E26F0 + 0x20 * i), nullptr);

    // The seams: the original's address and the recovered function, to the same double.
    struct Seam { uint32_t va; void* host; const void* fo; const void* fr; };
    const Seam seams[] = {
        {0x0014A260, member_address(&GmAcActor::SpawnEffect), (const void*)d_spawn, (const void*)d_spawn},
        {0x000A1E10, member_address(&GmAcActor::GetNoteBank), (const void*)d_note_bank, (const void*)d_note_bank},
        {0x0014A8D0, member_address(&GmAcActor::SceneSetFlag10), (const void*)d_scene_flag, (const void*)d_scene_flag},
        {0x00194BF0, member_address(&GmAcAudio::Slot38), (const void*)d_audio38, (const void*)d_audio38},
        {0x0015EC10, member_address(&GmAcComponent::SendEvent), (const void*)d_send_event, (const void*)d_send_event},
        {0x0017E7C0, member_address(&GmAcTriggerTable::FindTrigger), (const void*)d_find_trigger, (const void*)d_find_trigger},
        {0x0017E1A0, member_address(&GmAcTriggerTable::FindNote), (const void*)d_trigger_note, (const void*)d_trigger_note},
        {0x001511E0, member_address(&GmEventManager::Post), (const void*)d_post, (const void*)d_post},
        {0x00159300, member_address(&GmAcAnimController::Detach), (const void*)d_detach, (const void*)d_detach},
        {0x00159760, member_address(&GmAcAnimSlots::Resize), (const void*)d_resize, (const void*)d_resize},
        {0x001B3B80, nullptr, (const void*)d_slot_dtor, (const void*)d_slot_dtor},
        {0x001B1420, member_address(&GmAcLinkedItem::Release), (const void*)d_release_item, (const void*)d_release_item},
        {0x0003A310, member_address(&AtSpaceGraph::RemoveAsKind1), (const void*)d_remove_kind1, (const void*)d_remove_kind1},
        {0x0014FB40, (void*)&GmDynamicNodeRelease, (const void*)o_release_node, (const void*)r_release_node},
        {0x001616C0, (void*)&GmVfxManagerHas, (const void*)o_vfx_has, (const void*)r_vfx_has},
        {0x001386D0, member_address(&GmAudioData::PlayNote), (const void*)o_play, (const void*)o_play},  // recovered (gm/gmsound), a double on both sides here
        {0x000332B0, member_address(&AtCoordSys::WorldPosition), (const void*)o_position, (const void*)o_position},
        {0x00032EA0, member_address(&AtCoordSys::Place), (const void*)o_place, (const void*)o_place},
        // The node's slots 28 and 29, which the actor's call directly: not recovered.
        {0x0014C040, nullptr, (const void*)d_node_slot28, nullptr},
        {0x0014C080, nullptr, (const void*)d_node_slot29, nullptr},
    };
    for (const Seam& s : seams) {
        redirect(s.va, s.fo);
        if (s.host) redirect(U(s.host), s.fr);
    }
    {
        // The recovered node's slots 28 and 29 are reached by direct calls; take their addresses from the vftable. The anim slot's destructor has no address in C++: take it from a call to it.
        GmAcNode* n = new GmAcNode;
        redirect(U(vptr(n)[28]), (const void*)d_node_slot28);
        redirect(U(vptr(n)[29]), (const void*)d_node_slot29);
        void* dtor = call_target((const void*)&destroy_slot);
        if (!dtor) { std::printf("cannot find the anim slot's destructor\n"); return 2; }
        redirect(U(dtor), (const void*)d_slot_dtor);
    }

    // The statics the actor's code reads.
    static GmEventManager events;
    static unsigned char loader[16], vfx[16];
    static struct { char pad[0x28]; void* graph; } space;
    static unsigned char graph[16];
    space.graph = graph;
    *(void**)(uintptr_t)0x003B3990 = &events;
    GmEventManager::s_instance = &events;
    *(void**)(uintptr_t)0x003B3984 = loader;
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)loader;
    *(void**)(uintptr_t)0x003B3C28 = vfx;
    GmVfxManager::s_instance = (GmVfxManager*)vfx;
    *(void**)(uintptr_t)0x003B3604 = &space;
    GmSpaceGraphOwner::s_instance = (GmSpaceGraphOwner*)&space;
    static int heap_token;
    for (uint32_t va : {0x002C632Cu, 0x003B3930u}) *(void**)(uintptr_t)va = &heap_token;
    GmNameMap::s_heap = &heap_token;
    GmAcActor::s_heap = &heap_token;
    // The name map the shutdown looks "Motion" up in: made on both sides, with an entry by that name.
    at<FnP0>(0x00024C60)((void*)(uintptr_t)0x002CD124, nullptr);
    at<Fn1>(0x00024DA0)((void*)(uintptr_t)0x002CD124, nullptr, 8);
    at<Fn2>(0x00024EE0)((void*)(uintptr_t)0x002CD124, nullptr, (uintptr_t)"Motion", 0x4D4D);
    GmAcActor::s_namedMaps.Init(8);
    GmAcActor::s_namedMaps.Insert("Motion", (void*)0x4D4D);
    build_audio();
    g_fill = 0xCD;
    // Each recorded entry stops once at its first byte; report_entries() names those never reached.
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_lifetimes\n");
    run_lifetimes();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_object\n");
    run_object();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_node\n");
    run_node();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_controller\n");
    run_controller();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_node_shutdown\n");
    run_node_shutdown();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_actor_shutdown\n");
    run_actor_shutdown();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_items\n");
    run_items();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_actor_slots\n");
    run_actor_slots();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_factories\n");
    run_factories();
    if (getenv("GMACTOR_TRACE")) std::printf("phase run_callback\n");
    run_callback();
    report_entries();
    std::printf("%s: %d cases; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
