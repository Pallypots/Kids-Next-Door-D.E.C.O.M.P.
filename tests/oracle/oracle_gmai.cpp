// Differential oracle for the AI action manager, the behaviour chart's commands that drive it, and the game's random numbers (Windows x86, local only). See docs/verification.md.
//
//   oracle_gmai <default.xbe> [cases dir] [--entries FILE] [--mutate]
//
// No file reaches this code: nothing that makes actors from data, or runs a behaviour chart, is recovered. So the cases are objects each side builds with its own code (the original's at its address,
// the recovered by new), records written into a design-registry tree the case makes, and direct calls; return values, every word of each manager and of every block it allocated (paired by order of
// allocation), the generator's words and the log of calls compared.
//
// What stands in, and is the same on both sides:
//  * The design registry's search (0x000679E0, a seam on this branch, recovered elsewhere): one double that searches a tree of plain links the case builds, by exact name among the children of the
//    link it is given. The values are plain structures with a descriptor (type, count) and a logging slot 5 (ElementAt). So the readers are tested against the search's interface and the values'
//    layout, not against real .ADR data, and a search's own rules (case, backslash paths, parents) are not exercised.
//  * The NPC that owns the manager: a plain block with a fake vftable whose slot 60 (TestCondition) answers from the case's set of true conditions and whose slot 49 (the event) answers the case's
//    RESUME answer, both logging. Its +0x10 and +0x14 are the registry and the record. One block for each side, alike, so that each side's +0x304 holds its own manager.
//  * The behaviour chart's text argument reader (0x001659F0, a seam): one double that logs its arguments and writes the case's text.
//  * The heap: both sides' allocations come from one double that fills with 0xCD and keeps every block until the case ends.
// Real on both sides, the original's code on one and the recovered on the other: the manager, the commands, the generator (each side's own, seeded alike, at the address the manager reads it from),
// the small readers 0x00014030, 0x00067CF0, 0x0014BAC0, and what the manager calls of the recovered branch: GmAcNode::FindValueOrDefault, GmBaObject::FindValue, GmAcNode::FindValueObject,
// AtCopyName8, AtCopyName9, AtCopyName and AtNamesEqual.
#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <string>
#include <vector>

#include "at/AtDataRegistry.h"
#include "at/AtRandomGenerator.h"
#include "gm/GmAcNPC.h"
#include "gm/GmAppSettings.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static int g_cases;
static int g_crashed_alike;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 200).c_str(), what, o, r);
}

// The platform services the libraries linked in name; nothing here calls them (GmAppSettings is linked in for GmHistoryLogs::s_instance).
unsigned int AtPlatformMilliseconds() { return 0; }
int AtPlatformLanguage() { return 0; }
int AtPlatformGameRegion() { return 0; }

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }
static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }

// ---- the call log and the heap ------------------------------------------------------------------------------------------------
struct Log {
    uint32_t v[1 << 15];
    int n = 0;
    void add(uint32_t a, uint32_t b) { if (n + 2 <= (int)(sizeof v / 4)) { v[n++] = a; v[n++] = b; } }
} static g_log[2];
static int g_side;
static bool g_recording;
static void log_(uint32_t a, uint32_t b = 0) { if (g_recording) g_log[g_side].add(a, b); }

// Every block a side allocates during a case, in order; none is freed until the case ends, so no address is used twice and the k-th of one side pairs with the k-th of the other.
struct Block { uint32_t p, size; bool alive; };
static Block g_blocks[2][1024];
static int g_nblocks[2];
static bool g_fail_alloc;
static int g_fail_after = -1;  // when not -1, each side's allocation request with this index (from the case's start) is refused
static int g_attempts[2];
static bool release_block(void* p) {
    for (int side = 0; side < 2; side++) {
        for (int k = 0; k < g_nblocks[side]; k++) {
            if (g_blocks[side][k].p == U(p)) {
                g_blocks[side][k].alive = false;
                return true;
            }
        }
    }
    return false;
}
static void free_blocks() {
    for (int side = 0; side < 2; side++) {
        for (int k = 0; k < g_nblocks[side]; k++) std::free((void*)(uintptr_t)g_blocks[side][k].p);
        g_nblocks[side] = 0;
    }
}
static void* side_new(int side, std::size_t n) {
    log_('N', (uint32_t)n);
    const int attempt = g_attempts[side]++;
    if (g_fail_alloc || attempt == g_fail_after) return nullptr;
    void* p = std::calloc(1, n + 16);
    std::memset(p, 0xCD, n + 16);
    if (g_nblocks[side] < 1024) g_blocks[side][g_nblocks[side]++] = {U(p), (uint32_t)n, true};
    return p;
}
static void* __cdecl o_new(unsigned n) { return g_recording ? side_new(0, n) : std::calloc(1, n); }
static void __cdecl o_delete(void* p) {
    if (g_recording && p) log_('D', 0);
    if (!release_block(p)) std::free(p);
}
static void __cdecl o_push(void* h) { log_('P', U(h)); }
static void __cdecl o_pop() { log_('p'); }
static void __cdecl o_fatal(const char*, int a, int, int) { log_('F', a); }
static void __cdecl o_raise(int code, int, int, int) { log_('R', code); }

void* operator new(std::size_t n) { return g_recording && g_side == 1 ? side_new(1, n) : std::calloc(1, n); }
void* operator new[](std::size_t n) { return g_recording && g_side == 1 ? side_new(1, n) : std::calloc(1, n); }
static void host_delete(void* p) {
    if (g_recording && g_side == 1 && p) log_('D', 0);
    if (!release_block(p)) std::free(p);
}
void operator delete(void* p) noexcept { host_delete(p); }
void operator delete[](void* p) noexcept { host_delete(p); }
void operator delete(void* p, std::size_t) noexcept { host_delete(p); }
void operator delete[](void* p, std::size_t) noexcept { host_delete(p); }
void AtPushHeap(void* h) { log_('P', U(h)); }
void AtPopHeap() { log_('p'); }
void AtFatal(const char*, int a, int, int) { log_('F', a); }
void AtRaise(int code, int, int, int) { log_('R', code); }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

struct On {
    explicit On(int side) { g_side = side; g_recording = true; }
    ~On() { g_recording = false; g_side = 0; }
};

// ---- canonical words ------------------------------------------------------------------------------------------------------------
// Objects each side has its own of, alike: the k-th of one side's pairs with the k-th of the other's.
struct Twin { uint32_t p[2]; uint32_t size; };
static std::vector<Twin> g_twins;
static void twin(const void* a, const void* b, uint32_t size) { g_twins.push_back({{U(a), U(b)}, size}); }
static uint32_t canon(int side, uint32_t w) {
    for (std::size_t t = 0; t < g_twins.size(); t++) {
        if (w >= g_twins[t].p[side] && w < g_twins[t].p[side] + g_twins[t].size) return 0xA0000000u | ((uint32_t)t << 16) | (w - g_twins[t].p[side]);
    }
    for (int k = 0; k < g_nblocks[side]; k++) {
        const Block& b = g_blocks[side][k];
        if (w >= b.p && w < b.p + b.size + (b.size == 0 ? 1 : 0)) return 0xE0000000u | ((uint32_t)k << 16) | (w - b.p);
    }
    return w;
}
static uint32_t C(const void* p) { return canon(g_side, U(p)); }

static void same_words(const char* what, const void* o, const void* r, uint32_t size) {
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    for (uint32_t i = 0; i < size / 4; i++) {
        const uint32_t ca = canon(0, a[i]), cb = canon(1, b[i]);
        if (ca != cb) {
            char text[96];
            sprintf_s(text, "%s word +0x%X", what, i * 4);
            eq(text, ca, cb);
            return;
        }
        g_checks++;
    }
    for (uint32_t i = size & ~3u; i < size; i++) {
        char text[96];
        sprintf_s(text, "%s byte +0x%X", what, i);
        eq(text, ((const unsigned char*)o)[i], ((const unsigned char*)r)[i]);
    }
}

// Words that are values, not addresses (the generator's state): compared as they are, since a random word can fall inside either side's objects.
static void same_raw(const char* what, const void* o, const void* r, uint32_t size) {
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    for (uint32_t i = 0; i < size / 4; i++) {
        if (a[i] != b[i]) {
            char text[96];
            sprintf_s(text, "%s word +0x%X", what, i * 4);
            eq(text, a[i], b[i]);
            return;
        }
        g_checks++;
    }
}

static void compare_blocks() {
    eq("blocks allocated", g_nblocks[0], g_nblocks[1]);
    const int n = g_nblocks[0] < g_nblocks[1] ? g_nblocks[0] : g_nblocks[1];
    for (int k = 0; k < n; k++) {
        const Block& a = g_blocks[0][k];
        const Block& b = g_blocks[1][k];
        char text[64];
        sprintf_s(text, "block %d size", k);
        eq(text, a.size, b.size);
        sprintf_s(text, "block %d alive", k);
        eq(text, a.alive, b.alive);
        if (a.alive && b.alive && a.size == b.size) {
            sprintf_s(text, "block %d", k);
            same_words(text, (void*)(uintptr_t)a.p, (void*)(uintptr_t)b.p, a.size);
        }
    }
}

static void compare_logs(const char* what) {
    g_recording = false;
    char text[160];
    sprintf_s(text, "%s: log length", what);
    eq(text, g_log[0].n, g_log[1].n);
    if (g_log[0].n != g_log[1].n && g_failures < 3) {
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < g_log[side].n && i < 60; i += 2) std::printf("%c%X ", (char)g_log[side].v[i], g_log[side].v[i + 1]);
            std::printf("\n");
        }
    }
    for (int i = 0; i < g_log[0].n && i < g_log[1].n; i++) {
        if (g_log[0].v[i] != g_log[1].v[i]) {
            sprintf_s(text, "%s: log entry %d", what, i);
            eq(text, g_log[0].v[i], g_log[1].v[i]);
            break;
        }
    }
    g_log[0].n = g_log[1].n = 0;
}

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
static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float bitsf(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
static unsigned long long dbits(double d) { unsigned long long u; std::memcpy(&u, &d, 8); return u; }

// ---- the design registry tree ---------------------------------------------------------------------------------------------------
// The layouts the code reads: a link's value (+0x04), name (+0x08), sibling (+0x10) and first child (+0x14); a value's descriptor (+0x04) and, for a short name array, its names (+0x0C); a
// descriptor's type (+0x0C) and count (+0x14).
struct FDesc {
    uint32_t w[3];
    uint32_t type;   // +0x0C
    uint32_t flags;  // +0x10
    uint32_t count;  // +0x14
    uint32_t rest[6];
};
struct FVal {
    void** vptr;
    FDesc* desc;     // +0x04
    void* registry;  // +0x08
    void* data;      // +0x0C a short name array's names
    uint32_t storage[4];
    uint32_t id;
    FDesc ownDesc;
};
struct FLink {
    void* vptr;
    FVal* value;     // +0x04
    const char* name;  // +0x08
    FLink* parent;   // +0x0C
    FLink* sibling;  // +0x10
    FLink* first;    // +0x14
    int inUse;       // +0x18
    char nameText[32];
};
static void* g_vt_val[10];
static void* __fastcall val_element(FVal* v, void*, unsigned index) { log_('E', v->id); log_('e', index); return index == 0 ? (void*)v->storage : (void*)&v->storage[index & 3]; }
static void __fastcall val_trap(FVal* v, void*) { log_('X', v->id); }

static FLink g_links[512];
static FVal g_vals[512];
static char g_names[64][8][8];  // short name arrays' names
static int g_nlinks, g_nvals, g_nnames;
static FLink* g_record;      // the owner's record
static FLink* g_defrec;      // the defaults' record

static void reset_tree() {
    std::memset(g_links, 0, sizeof g_links);
    std::memset(g_vals, 0, sizeof g_vals);
    std::memset(g_names, 0, sizeof g_names);
    g_nlinks = g_nvals = g_nnames = 0;
    g_record = &g_links[g_nlinks++];
    g_defrec = &g_links[g_nlinks++];
}
static FVal* new_val(uint32_t type, uint32_t count = 1) {
    FVal* v = &g_vals[g_nvals++];
    v->vptr = g_vt_val;
    v->desc = &v->ownDesc;
    v->ownDesc.type = type;
    v->ownDesc.count = count;
    v->id = 0x100 + g_nvals;
    v->registry = (void*)0x5EC00000;
    return v;
}
static FVal* real_val(float f) { FVal* v = new_val(AtDataRegistry::kReal); v->storage[0] = fbits(f); return v; }
static FVal* int_val(uint32_t type, uint32_t value) { FVal* v = new_val(type); v->storage[0] = value; return v; }
static FVal* name_val(const char* text) {
    FVal* v = new_val(AtDataRegistry::kShortName);
    std::memcpy(v->storage, text, std::strlen(text) < 16 ? std::strlen(text) : 16);
    return v;
}
static FVal* names_val(std::initializer_list<const char*> names, uint32_t type = AtDataRegistry::kShortNameArray) {
    FVal* v = new_val(type, (uint32_t)names.size());
    char (*data)[8] = g_names[g_nnames++];
    int i = 0;
    for (const char* n : names) std::memcpy(data[i++], n, std::strlen(n) < 8 ? std::strlen(n) : 8);
    v->data = data;
    return v;
}
static FLink* add_link(FLink* parent, const char* name, FVal* value) {
    FLink* l = &g_links[g_nlinks++];
    strcpy_s(l->nameText, name);
    l->name = l->nameText;
    l->value = value;
    l->parent = parent;
    l->inUse = 1;
    if (parent) {
        FLink** at = &parent->first;
        while (*at) at = &(*at)->sibling;
        *at = l;
    }
    return l;
}

static void* const kRegistry = (void*)0x5EC00000;
static void* const kDefRegistry = (void*)0x0DEF0000;
static GmDesignDefaults g_defaults;

static AtDataLink* find_impl(void* registry, void* under, const char* path, int parents) {
    log_('S', C(registry));
    log_('s', C(under));
    log_('s', hash_str(path));
    log_('s', parents);
    FLink* u = (FLink*)under;
    if (u == nullptr) return nullptr;
    for (FLink* c = u->first; c; c = c->sibling) {
        if (std::strcmp(c->name, path) == 0) return (AtDataLink*)c;
    }
    return nullptr;
}
static AtDataLink* __fastcall o_find(void* registry, void*, void* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }
static AtDataLink* r_find(AtDataRegistry* registry, AtDataLink* under, const char* path, int parents) { return find_impl(registry, under, path, parents); }

// ---- the owner, the chart and the generator -------------------------------------------------------------------------------------
static void* g_vt_owner[61];
static char g_true[8][8];
static int g_ntrue;
static int g_resume;
template <int S> static void __fastcall owner_trap(void* o, void*) { log_('X' << 8 | S, C(o)); }
template <int... S> struct FillTrap { static void in(void** vt) { (void)std::initializer_list<int>{(vt[S] = (void*)&owner_trap<S>, 0)...}; } };
static int __fastcall owner_condition(void* o, void*, const char* name) {
    log_('C', C(o));
    log_('c', hash_name(name));
    for (int i = 0; i < g_ntrue; i++) {
        if (std::strncmp(g_true[i], name, 8) == 0) return 1;
    }
    return 0;
}
static int __fastcall owner_event(void* o, void*, const char* event, int b, int c) {
    log_('V', C(o));
    log_('v', hash_name(event));
    log_('v', b);
    log_('v', c);
    return g_resume;
}

static unsigned char* g_owner[2];      // 0x308 each
static unsigned char* g_manager[2];    // 0x24 each
static unsigned char* g_rng[2];        // 0x9C8 each, the generators the manager draws from
static unsigned char* g_holder[2];     // what 0x002CB0A8 points to: +0x10 is the generator
static GmChartContext* g_context_[2];
static GmChartFrame* g_frame[2];
static char g_text[64];                // what the text argument double writes
static unsigned char* g_rng2[2];       // generators for the direct cases

static void __cdecl d_text(char* out, unsigned size, GmChartContext* context, void* a2, void** args, int index, int flag) {
    log_('T', size);
    log_('t', C(context));
    log_('t', U(a2));
    log_('t', U(args));
    log_('t', index);
    log_('t', flag);
    unsigned i = 0;
    for (; g_text[i] && i + 1 < size; i++) out[i] = g_text[i];
    out[i] = 0;
}

static void set_owner(void* registry, void* record) {
    for (int s = 0; s < 2; s++) {
        std::memset(g_owner[s], 0, 0x308);
        *(void***)g_owner[s] = g_vt_owner;
        *(void**)(g_owner[s] + 0x10) = registry;
        *(void**)(g_owner[s] + 0x14) = record;
        *(void**)(g_owner[s] + 0x304) = g_manager[s];
    }
}
// The manager's owner, as Setup would keep it: each side's own.
static void set_manager_owner() {
    for (int s = 0; s < 2; s++) *(void**)g_manager[s] = g_owner[s];
}
static void set_true(std::initializer_list<const char*> names) {
    std::memset(g_true, 0, sizeof g_true);
    g_ntrue = 0;
    for (const char* n : names) std::memcpy(g_true[g_ntrue++], n, std::strlen(n) < 8 ? std::strlen(n) : 8);
}

// ---- both sides of one call -----------------------------------------------------------------------------------------------------
static void reset_x87() {
    unsigned short cw;
    __asm {
        fnstcw cw
        fninit
        fldcw cw
    }
}
// True when neither side crashed.
template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    const bool oc = guarded([&] { On on(0); fo(); });
    reset_x87();
    g_recording = false;
    const bool rc = guarded([&] { On on(1); fr(); });
    reset_x87();
    g_recording = false;
    g_side = 0;
    char text[160];
    sprintf_s(text, "%s: crashed", what);
    eq(text, oc, rc);
    if (oc && rc) g_crashed_alike++;
    compare_logs(what);
    return !oc && !rc;
}

typedef void*(__fastcall* FnP0)(void*, void*);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnP2)(void*, void*, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnP3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef float(__fastcall* FnWeight)(void*, void*, unsigned, int);
typedef unsigned(__fastcall* FnU0)(void*, void*);
typedef unsigned(__fastcall* FnU1)(void*, void*, unsigned);
typedef double(__fastcall* FnD0)(void*, void*);
typedef int(__cdecl* FnChart)(void*, void*, void**);
typedef int(__cdecl* FnLen)(const char*);
typedef void(__cdecl* FnHeap)(void*);

static GmAiActionManager* M() { return (GmAiActionManager*)g_manager[1]; }
static AtRandomGenerator* R(int which) { return (AtRandomGenerator*)(which ? g_rng2[1] : g_rng[1]); }

static void compare_state(const char* what) {
    char text[96];
    // The last action's name is compared up to its terminator only. vNPCStoreAIAction copies the chart's argument into eight bytes of its stack with AtCopyName, which stops after the
    // terminator, and the manager then copies all eight with AtCopyName8: the bytes after the terminator are each side's stack, uninitialised, and cannot be compared. Nothing reads them as a name.
    unsigned char copy[2][0x24];
    for (int s = 0; s < 2; s++) {
        std::memcpy(copy[s], g_manager[s], 0x24);
        bool ended = false;
        for (int i = 0x14; i < 0x1C; i++) {
            if (ended) copy[s][i] = 0;
            ended = ended || copy[s][i] == 0;
        }
    }
    sprintf_s(text, "%s: manager", what);
    same_words(text, copy[0], copy[1], 0x24);
    compare_blocks();
    sprintf_s(text, "%s: generator", what);
    same_raw(text, g_rng[0], g_rng[1], 0x9C8);
}

static void seed_generators(unsigned seed) {
    R(0)->Seed(seed);
    std::memcpy(g_rng[0], g_rng[1], 0x9C8);
}

// The managers: each side constructs its own. Every case starts here and ends with end_case.
static bool begin_case(const char* name) {
    g_cases++;
    g_context = name;
    free_blocks();
    g_fail_alloc = false;
    g_fail_after = -1;
    g_attempts[0] = g_attempts[1] = 0;
    std::memset(g_manager[0], 0xCD, 0x24);
    std::memset(g_manager[1], 0xCD, 0x24);
    const bool ok = both("constructor", [&] { at<FnP0>(0x0017D420)(g_manager[0], nullptr); }, [&] { new (g_manager[1]) GmAiActionManager; });
    same_words("constructor", g_manager[0], g_manager[1], 0x24);
    return ok;
}
static void end_case(bool destroy = true) {
    if (destroy) {
        both("destructor", [&] { at<Fn0>(0x0017D6C0)(g_manager[0], nullptr); }, [&] { M()->~GmAiActionManager(); });
        compare_state("destructor");
    }
    g_fail_alloc = false;
    g_fail_after = -1;
    free_blocks();
}

static void set_heap(void* heap) {
    both("heap", [&] { at<FnHeap>(0x0017CDD0)(heap); }, [&] { GmAiActionManager::SetHeap(heap); });
    eq("heap", (long long)*(uint32_t*)(uintptr_t)0x003B3C88, (long long)U(GmAiActionManager::s_heap));
}

static bool op_read() {
    const bool ok = both("read", [&] { at<Fn0>(0x0017CEA0)(g_manager[0], nullptr); }, [&] { M()->ReadActions(); });
    if (ok) compare_state("read");
    return ok;
}
static bool op_setup() {
    const bool ok = both("setup", [&] { at<Fn1>(0x0017D4E0)(g_manager[0], nullptr, U(g_owner[0])); }, [&] { M()->Setup((GmAcNPC*)g_owner[1]); });
    if (ok) compare_state("setup");
    return ok;
}
static bool op_pattern(const char* name) {
    const bool ok = both("pattern", [&] { at<Fn1>(0x0017D3C0)(g_manager[0], nullptr, U(name)); }, [&] { M()->SetPattern(name); });
    if (ok) compare_state("pattern");
    return ok;
}
static bool op_find(const char* name, int second) {
    uint32_t r[2] = {0, 0};
    const bool ok = both("find", [&] { r[0] = canon(0, U(at<FnP2>(0x0017CDE0)(g_manager[0], nullptr, U(name), second))); }, [&] { r[1] = canon(1, U(M()->Find(name, second))); });
    eq("find", r[0], r[1]);
    return ok;
}
static bool op_enabled(const char* name) {
    int r[2] = {0, 0};
    const bool ok = both("enabled", [&] { r[0] = at<FnI1>(0x0017CE30)(g_manager[0], nullptr, U(name)); }, [&] { r[1] = M()->IsEnabled(name); });
    eq("enabled", r[0], r[1]);
    return ok;
}
static bool op_enable(const char* name, bool on) {
    const bool ok = both(on ? "enable" : "disable", [&] { at<Fn1>(on ? 0x0017CE60 : 0x0017CE80)(g_manager[0], nullptr, U(name)); }, [&] { if (on) M()->Enable(name); else M()->Disable(name); });
    if (ok) compare_state("enable");
    return ok;
}
static bool op_weight(unsigned index, int allow) {
    uint32_t r[2] = {0, 0};
    const bool ok = both("weight", [&] { r[0] = fbits(at<FnWeight>(0x0017D290)(g_manager[0], nullptr, index, allow)); }, [&] { r[1] = fbits(M()->Weight(index, allow)); });
    eq("weight", r[0], r[1]);
    return ok;
}
static bool op_choose() {
    char out[2][12];
    std::memset(out, 0xEE, sizeof out);
    const bool ok = both("choose", [&] { at<Fn1>(0x0017D530)(g_manager[0], nullptr, U(out[0])); }, [&] { M()->Choose(out[1]); });
    if (ok) {
        same_words("choose: out", out[0], out[1], 12);
        compare_state("choose");
    }
    return ok;
}
static bool op_store(const char* name) {
    const bool ok = both("store", [&] { at<Fn1>(0x0017D340)(g_manager[0], nullptr, U(name)); }, [&] { M()->Store(name); });
    if (ok) compare_state("store");
    return ok;
}
// Writes the same value into both managers' word at an offset.
static void poke(uint32_t offset, uint32_t value) {
    *(uint32_t*)(g_manager[0] + offset) = value;
    *(uint32_t*)(g_manager[1] + offset) = value;
}

// ---- the cases --------------------------------------------------------------------------------------------------------------------
static uint32_t g_rand = 1;
static uint32_t rnd() { g_rand = g_rand * 1103515245u + 12345u; return (g_rand >> 16) & 0x7FFF; }
static uint32_t pick(uint32_t n) { return rnd() % n; }

static const char* const kActionNames[] = {"", "Punch", "Kick", "Run", "Taunt", "LongName", "Wait", "Kick"};
static const char* const kConditions[] = {"cNear", "cFar", "cLowHP", "cThreat", "cAtkRg1", "LongCond"};
static const float kWeights[] = {2.0f, 0.1f, 0.0f, -1.0f, -0.0f, 1e30f, 3.3f, 1.0f};
static const float kFactors[] = {0.3f, 2.0f, -1.0f, 1.0f, 0.0f, 0.7f, -0.0f, 0.75f};
static const char* const kNextNames[] = {"Punch", "Kick", "A", "Taunt"};  // one of one character

// One action's record under the AIActions link, with what the case's bits say.
static void add_action_record(FLink* list, const char* name, uint32_t bits) {
    if ((bits & 7) == 0) return;  // no record
    FVal* weight;
    if ((bits & 7) == 1) weight = nullptr;
    else if ((bits & 7) == 2) weight = int_val(AtDataRegistry::kInt, 2);
    else if ((bits & 7) == 3) weight = real_val(bitsf(0x7FC00000u));
    else weight = real_val(kWeights[(bits >> 3) & 7]);
    FLink* record = add_link(list, name, weight);
    if (weight == nullptr) return;  // a record with no value: nothing under it is read
    switch ((bits >> 6) & 7) {
        case 1: add_link(record, "Repeatable", int_val(AtDataRegistry::kBoolean, 0)); break;
        case 2: add_link(record, "Repeatable", int_val(AtDataRegistry::kBoolean, 1)); break;
        case 3: add_link(record, "Repeatable", int_val(AtDataRegistry::kBoolean, 7)); break;
        case 4: add_link(record, "Repeatable", int_val(AtDataRegistry::kInt, 0)); break;
        case 5: add_link(record, "Repeatable", real_val(0.0f)); break;  // a real: a modifier, not Repeatable
        default: break;
    }
    switch ((bits >> 9) & 3) {
        case 1: add_link(record, "NextAction", name_val(kNextNames[(bits >> 11) & 3])); break;
        case 2: add_link(record, "NextAction", int_val(AtDataRegistry::kString, 0)); break;
        case 3: add_link(record, "NextAction", name_val("")); break;
        default: break;
    }
    const uint32_t modifiers = (bits >> 13) & 3;
    for (uint32_t i = 0; i < modifiers; i++) {
        if ((bits >> (15 + i)) & 1) add_link(record, "Note", int_val(AtDataRegistry::kInt, 4));
        add_link(record, kConditions[pick(6)], real_val(((bits >> (18 + i)) & 1) ? bitsf(0x7FC00000u) : kFactors[pick(8)]));
    }
}

static FLink* build_actions(int count, uint32_t type = AtDataRegistry::kShortNameArray) {
    const char* chosen[8];
    for (int i = 0; i < count; i++) chosen[i] = kActionNames[pick(8)];
    FVal* list = new_val(type, (uint32_t)count);
    char (*data)[8] = g_names[g_nnames++];
    for (int i = 0; i < count; i++) std::memcpy(data[i], chosen[i], std::strlen(chosen[i]) < 8 ? std::strlen(chosen[i]) : 8);
    list->data = data;
    FLink* link = add_link(g_record, "AIActions", list);
    for (int i = 0; i < count; i++) {
        bool seen = false;
        for (FLink* c = link->first; c; c = c->sibling) seen = seen || std::strcmp(c->name, chosen[i]) == 0;
        if (!seen && chosen[i][0]) add_action_record(link, chosen[i], (rnd() << 15) | rnd());
    }
    return link;
}

static void add_patterns() {
    add_link(g_record, "PatA", names_val({"Kick", "Run", "Kick"}));
    add_link(g_record, "PatB", names_val({"Taunt"}));
    add_link(g_record, "PatEmpty", names_val({}));
    add_link(g_record, "PatInt", int_val(AtDataRegistry::kInt, 3));
    add_link(g_record, "LongPatt", names_val({"Run", "Punch"}));  // what "LongPattern" becomes when copied to nine bytes
}

static const char* const kStoreNames[] = {"Punch", "Kick", "Run", "Taunt", "LongName", "Wait", "", "Nothing", "LongNameX"};
static const char* const kPatternNames[] = {"PatA", "PatB", "PatEmpty", "PatInt", "Missing", "", "LongPattern"};

// Ops on a manager that has read its actions: the condition set, choices, stores, enables, patterns, weights.
static void run_ops(int count) {
    for (int k = 0; k < 40; k++) {
        bool ok = true;
        switch (pick(12)) {
            case 0: case 1: case 2: ok = op_choose(); break;
            case 3: case 4: ok = op_store(kStoreNames[pick(9)]); break;
            case 5: ok = op_enable(kStoreNames[pick(9)], pick(2) != 0); break;
            case 6: ok = op_enabled(kStoreNames[pick(9)]); break;
            case 7: ok = op_find(kStoreNames[pick(9)], (int)pick(3)); break;
            case 8: if (pick(3) == 0) ok = op_pattern(kPatternNames[pick(7)]); break;
            case 9: {
                const uint32_t n = *(uint32_t*)(g_manager[1] + 0x0C);
                if (n) ok = op_weight(pick(n), (int)pick(2));
                break;
            }
            case 10: {
                const char* sets[][3] = {{"cNear", "", ""}, {"cFar", "cLowHP", ""}, {"cThreat", "cAtkRg1", "LongCond"}, {"", "", ""}, {"cNear", "cFar", "cLowHP"}};
                const int s = (int)pick(5);
                set_true({sets[s][0], sets[s][1], sets[s][2]});
                break;
            }
            case 11: if (pick(4) == 0) poke(0x20, pick(6)); break;
        }
        if (!ok) return;
        (void)count;
    }
}

static void run_lifetimes() {
    // Built and destroyed with nothing read.
    if (begin_case("empty")) end_case();
    // Clear directly, twice.
    if (begin_case("clear twice")) {
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        build_actions(3);
        op_read();
        for (int i = 0; i < 2; i++) {
            both("clear", [&] { at<Fn0>(0x0017D450)(g_manager[0], nullptr); }, [&] { M()->Clear(); });
            compare_state("clear");
        }
        end_case();
    }
    set_heap(nullptr);
    set_heap((void*)0x4EA90000);
}

static void run_reads() {
    // Without the registry, without the record, without the list, a list with no value, a list of another type, an empty list.
    for (int v = 0; v < 9; v++) {
        if (!begin_case("read: absent")) continue;
        reset_tree();
        void* registry = v == 0 || v == 8 ? nullptr : kRegistry;
        void* record = v == 1 ? nullptr : g_record;
        set_owner(registry, record);
        set_manager_owner();
        if (v == 3) add_link(g_record, "AIActions", nullptr);
        if (v == 4) add_link(g_record, "AIActions", names_val({"Punch"}, AtDataRegistry::kShortName));
        if (v == 5) build_actions(0);
        if (v == 6) build_actions(2, AtDataRegistry::kStringArray);
        if (v == 7) add_link(g_record, "aiactions", names_val({"Punch"}));
        if (v == 8) build_actions(3);  // a list, but no registry
        if (op_read()) end_case();
        else end_case(false);
    }
    // No heap: raised, then the allocation goes on (the raise double returns).
    for (int v = 0; v < 2; v++) {
        if (!begin_case("read: no heap")) continue;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        build_actions(3);
        set_heap(nullptr);
        op_read();
        set_heap((void*)0x4EA90000);
        end_case();
    }
    // Refused memory, at each allocation in turn: raised, then a crash or more.
    for (int fail = 0; fail < 6; fail++) {
        if (!begin_case("read: refused")) continue;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        g_rand = 77;
        build_actions(4);
        set_manager_owner();
        g_fail_after = fail;
        const bool ok = op_read();
        g_fail_after = -1;
        end_case(ok);
    }
    // A record whose child has no value: a crash on both sides.
    if (begin_case("read: child without value")) {
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        FLink* list = add_link(g_record, "AIActions", names_val({"Punch"}));
        FLink* record = add_link(list, "Punch", real_val(1.0f));
        add_link(record, "cNear", nullptr);
        end_case(op_read());
    }
    // Duplicates: the list ends at the first name already read.
    for (int v = 0; v < 6; v++) {
        if (!begin_case("read: duplicates")) continue;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        FLink* list;
        switch (v) {
            case 0: list = add_link(g_record, "AIActions", names_val({"Punch", "Kick", "Punch", "Run"})); break;
            case 1: list = add_link(g_record, "AIActions", names_val({"Punch", "Punch"})); break;
            case 2: list = add_link(g_record, "AIActions", names_val({"Missing", "Missing", "Kick"})); break;  // the first was not read: no repeat
            case 3: list = add_link(g_record, "AIActions", names_val({"", "", "Kick", ""})); break;
            case 4: list = add_link(g_record, "AIActions", names_val({"LongNameA", "LongNameB", "Kick"})); break;  // eight bytes alike
            default: list = add_link(g_record, "AIActions", names_val({"Kick", "Run", "Taunt", "Kick", "Kick", "Run"})); break;
        }
        for (const char* n : {"Punch", "Kick", "Run", "Taunt", "LongName"}) add_action_record(list, n, 0x4 | (1 << 3) | (2 << 6) | (1 << 13));
        if (op_read()) {
            run_ops(4);
            end_case();
        } else {
            end_case(false);
        }
    }
    // Weights and factors at every value, and each word's kinds.
    for (int v = 0; v < 64; v++) {
        if (!begin_case("read: values")) continue;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        FLink* list = add_link(g_record, "AIActions", names_val({"Punch", "Kick", "Run"}));
        const uint32_t base = (uint32_t)v;
        add_action_record(list, "Punch", 4 | ((base & 7) << 3) | (((base >> 3) & 7) << 6) | (1 << 9) | (3 << 13));
        add_action_record(list, "Kick", 4 | (((base + 3) & 7) << 3) | (((base >> 2) & 7) << 6) | ((base & 3) << 9) | (2 << 13) | ((base & 1) << 15));
        add_action_record(list, "Run", (v % 4 == 0 ? 2 : v % 4 == 1 ? 3 : 5) | ((base & 7) << 3) | (3 << 13) | (1u << 18));
        if (op_read()) {
            set_true({"cNear", "cFar", "cLowHP"});
            for (unsigned i = 0; i < 3; i++) {
                op_weight(i, 0);
                op_weight(i, 1);
            }
            run_ops(3);
            end_case();
        } else {
            end_case(false);
        }
    }
    // Random trees, and many ops on each.
    for (int v = 0; v < 1500; v++) {
        if (!begin_case("read: random")) continue;
        g_rand = 1000 + v;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        build_actions((int)pick(8));
        add_patterns();
        seed_generators(5000 + v);
        if (op_read()) {
            run_ops(0);
            end_case();
        } else {
            end_case(false);
        }
    }
}

// The pick at the edge of float precision: weights 2^24, 1, 2^24 - 1 (a float total of 2^25) and a draw of 5000, whose target rounds to exactly 2^24 as a float (16777215.58 in double).
// The running sum in double passes it at the second action, in float only at the third; a target kept in double passes at the first. The draw is set by writing one word the
// generator tempers next into both generators alike.
static unsigned temper(unsigned y) {
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680u;
    y ^= (y << 15) & 0xEFC60000u;
    y ^= y >> 18;
    return y;
}
static void run_edges() {
    unsigned word = 1;
    while (temper(word) % 10000 != 5000) word = word * 1664525u + 1013904223u;
    for (int v = 0; v < 4; v++) {
        if (!begin_case("choose: edge")) continue;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        FLink* list = add_link(g_record, "AIActions", names_val({"Big", "One", "Rest"}));
        add_link(list, "Big", real_val(16777216.0f));
        add_link(list, "One", real_val(1.0f));
        add_link(list, "Rest", real_val(16777215.0f));
        if (!op_read()) { end_case(false); continue; }
        seed_generators(1 + v);
        for (int s = 0; s < 2; s++) {
            AtRandomGenerator* g = (AtRandomGenerator*)g_rng[s];
            g->m_state[g->m_index] = v < 2 ? word : temper(v) ;
        }
        op_choose();
        end_case();
    }
}

static void run_setups() {
    for (int v = 0; v < 216; v++) {
        if (!begin_case("setup")) continue;
        g_rand = 9000 + v;
        reset_tree();
        set_owner(kRegistry, g_record);
        add_patterns();
        const int action = v % 3, pattern = (v / 3) % 6, preset = (v / 18) % 2, list = (v / 36) % 2, value = (v / 72) % 3;
        // DefaultAIAction: in the record, in the defaults, neither.
        if (action == 0) add_link(g_record, "DefaultAIAction", name_val("Idle"));
        if (action == 1) add_link(g_defrec, "DefaultAIAction", name_val(value == 0 ? "DefIdle" : value == 1 ? "" : "LongDefaultName"));
        // DefaultAIPattern: the name of each pattern, or none; in the defaults for one.
        const char* const patterns[] = {"PatA", "PatEmpty", "PatInt", "Missing", "", nullptr};
        if (patterns[pattern] != nullptr) add_link(pattern == 3 ? g_defrec : g_record, "DefaultAIPattern", name_val(patterns[pattern]));
        if (list) build_actions(3);
        // Actions already there: no read.
        static GmAiAction dummy[2];
        if (preset) poke(0x08, U(dummy));
        if (op_setup()) {
            if (preset) poke(0x08, 0);
            run_ops(0);
            end_case();
        } else {
            end_case(false);
        }
    }
    // Patterns directly, with and without a record.
    for (int v = 0; v < 14; v++) {
        if (!begin_case("pattern")) continue;
        reset_tree();
        set_owner(kRegistry, v < 7 ? g_record : nullptr);
        set_manager_owner();
        add_patterns();
        op_pattern(kPatternNames[v % 7]);
        end_case();
    }
}

static void run_chart() {
    for (int v = 0; v < 400; v++) {
        if (!begin_case("chart")) continue;
        g_rand = 20000 + v;
        reset_tree();
        set_owner(kRegistry, g_record);
        set_manager_owner();
        build_actions(1 + (int)pick(6));
        add_patterns();
        seed_generators(777 + v);
        if (!op_read()) { end_case(false); continue; }
        if (pick(2)) op_pattern(kPatternNames[pick(7)]);
        const int frame = (int)pick(3);  // 0: a frame; 1: no manager; 2: a null frame
        const int current = (int)pick(0x21);
        for (int s = 0; s < 2; s++) {
            std::memset(g_context_[s], 0, sizeof(GmChartContext));
            g_context_[s]->m_current = current;
            g_frame[s]->m_actor = (GmAcNPC*)g_owner[s];
            g_context_[s]->m_frames[g_context_[s]->m_current] = frame == 2 ? nullptr : g_frame[s];
            *(void**)(g_owner[s] + 0x304) = frame == 1 ? nullptr : g_manager[s];
        }
        bool ok = true;
        for (int k = 0; k < 12 && ok; k++) {
            g_resume = (int)pick(3) - (pick(4) == 0 ? 1 : 0);
            static const char* const texts[] = {"Kick", "Punch", "LongNameXYZ", "", "Run", "Taunt", "Wait"};
            strcpy_s(g_text, texts[pick(7)]);
            int r[2] = {0, 0};
            void* a2 = (void*)(uintptr_t)(0x1000 + k);
            void** args = (void**)(uintptr_t)(0x2000 + v);
            switch (pick(5)) {
                case 4:
                    ok = op_enable(kStoreNames[pick(9)], pick(2) != 0);
                    continue;
                case 0: case 1:
                    ok = both("bNPCChooseAIAction", [&] { r[0] = at<FnChart>(0x0016C0C0)(g_context_[0], a2, args); }, [&] { r[1] = GmChartChooseAIAction(g_context_[1], a2, args); });
                    break;
                case 2:
                    ok = both("vNPCStoreAIAction", [&] { r[0] = at<FnChart>(0x0016C180)(g_context_[0], a2, args); }, [&] { r[1] = GmChartStoreAIAction(g_context_[1], a2, args); });
                    break;
                default:
                    ok = both("bNPCHasAIAction", [&] { r[0] = at<FnChart>(0x0016C1F0)(g_context_[0], a2, args); }, [&] { r[1] = GmChartHasAIAction(g_context_[1], a2, args); });
                    break;
            }
            eq("chart answer", r[0], r[1]);
            if (ok) compare_state("chart");
            if (ok && pick(3) == 0) set_true({kConditions[pick(6)], kConditions[pick(6)], ""});
        }
        end_case(ok || frame == 2);
    }
}

static void run_random() {
    for (int v = 0; v < 120; v++) {
        g_cases++;
        g_context = "generator";
        free_blocks();
        for (int s = 0; s < 2; s++) std::memset(g_rng2[s], (v & 1) ? 0x5A : 0, 0x9C8);
        both("generator: constructor", [&] { at<FnP0>(0x00071BB0)(g_rng2[0], nullptr); }, [&] { new (g_rng2[1]) AtRandomGenerator; });
        same_raw("generator", g_rng2[0], g_rng2[1], 0x9C8);
        if (v % 3 == 1) {
            const unsigned seed = v == 1 ? 0u : v == 4 ? 0xFFFFFFFFu : (unsigned)(v * 2654435761u);
            both("generator: seed", [&] { at<Fn1>(0x00071CA0)(g_rng2[0], nullptr, seed); }, [&] { R(1)->Seed(seed); });
        } else if (v % 3 == 2) {
            unsigned r[2] = {0, 0};
            if (v % 2) both("generator: default", [&] { r[0] = at<FnU0>(0x00071D90)(g_rng2[0], nullptr); }, [&] { r[1] = (unsigned)R(1)->SeedDefault(); });
            else both("generator: reset", [&] { r[0] = at<FnU0>(0x00071DA0)(g_rng2[0], nullptr); }, [&] { r[1] = R(1)->ResetSeed(); });
            eq("generator: seed answer", r[0], r[1]);
        }
        // Unseeded (v % 3 == 0): the first draw twists the words the constructor left, as they are.
        same_raw("generator", g_rng2[0], g_rng2[1], 0x9C8);
        for (int k = 0; k < 1400; k++) {
            unsigned r[2] = {0, 0};
            unsigned long long d[2] = {0, 0};
            const unsigned n = k % 5 == 0 ? 0u : k % 5 == 1 ? 1u : k % 5 == 2 ? 7u : k % 5 == 3 ? 0xFFFFFFFFu : 10000u;
            switch (k % 4) {
                case 0: both("generator: next", [&] { r[0] = at<FnU0>(0x00071D00)(g_rng2[0], nullptr); }, [&] { r[1] = R(1)->Next(); }); break;
                case 1: both("generator: below", [&] { r[0] = at<FnU1>(0x00071D50)(g_rng2[0], nullptr, n); }, [&] { r[1] = R(1)->Below(n); }); break;
                case 2: both("generator: unit", [&] { d[0] = dbits(at<FnD0>(0x00071D70)(g_rng2[0], nullptr)); }, [&] { d[1] = dbits(R(1)->Unit()); }); break;
                default: if (k % 200 == 3) both("generator: reload", [&] { at<Fn0>(0x00071BD0)(g_rng2[0], nullptr); }, [&] { R(1)->Reload(); }); break;
            }
            eq("generator: answer", r[0], r[1]);
            eq("generator: unit", (long long)d[0], (long long)d[1]);
            if (k % 97 == 0) same_raw("generator", g_rng2[0], g_rng2[1], 0x9C8);
        }
        same_raw("generator", g_rng2[0], g_rng2[1], 0x9C8);
        // Index set past the end and negative, alike.
        for (int s = 0; s < 2; s++) *(int*)(g_rng2[s] + 0x9C0) = v % 2 ? 700 : -3;
        unsigned r[2] = {0, 0};
        both("generator: odd index", [&] { r[0] = at<FnU0>(0x00071D00)(g_rng2[0], nullptr); }, [&] { r[1] = R(1)->Next(); });
        eq("generator: odd index", r[0], r[1]);
        same_raw("generator", g_rng2[0], g_rng2[1], 0x9C8);
    }
}

static void run_helpers() {
    // The name length over every length, with bytes after.
    static const char* const texts[] = {"", "a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefg", "abcdefgh", "abcdefghijk", "\x80\xFF"};
    for (const char* t : texts) {
        g_cases++;
        int r[2] = {0, 0};
        both("name length", [&] { r[0] = at<FnLen>(0x00014030)(t); }, [&] { r[1] = AtNameLength8(t); });
        eq("name length", r[0], r[1]);
    }
    // The registry's value and the node's link, found and not.
    reset_tree();
    add_link(g_record, "Found", real_val(1.0f));
    add_link(g_record, "NoValue", nullptr);
    for (const char* name : {"Found", "NoValue", "Missing", ""}) {
        for (int parents = 0; parents < 2; parents++) {
            g_cases++;
            uint32_t r[2] = {0, 0};
            both("registry value", [&] { r[0] = U(at<FnP3>(0x00067CF0)(kRegistry, nullptr, U(g_record), U(name), parents)); },
                 [&] { r[1] = U(AtDataRegistryFindValue((AtDataRegistry*)kRegistry, (AtDataLink*)g_record, name, parents)); });
            eq("registry value", r[0], r[1]);
        }
        for (int record = 0; record < 2; record++) {
            g_cases++;
            set_owner(record ? kRegistry : nullptr, record ? g_record : nullptr);
            uint32_t r[2] = {0, 0};
            both("node link", [&] { r[0] = U(at<FnP2>(0x0014BAC0)(g_owner[0], nullptr, U(name), 5)); }, [&] { r[1] = U(GmAcNodeFindLink((GmAcNode*)g_owner[1], name, 5)); });
            eq("node link", r[0], r[1]);
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
    if (pos.size() < 1) { std::printf("usage: oracle_gmai <default.xbe> [cases dir] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();

    // The seams: the original's address and the recovered function, to the same double.
    redirect(0x000679E0, (const void*)o_find);
    redirect(U((const void*)&AtDataRegistryFind), (const void*)r_find);
    redirect(0x001659F0, (const void*)d_text);
    redirect(U((const void*)&GmChartTextArgument), (const void*)d_text);

    g_vt_val[5] = (void*)val_element;
    for (int i : {0, 1, 2, 3, 4, 6, 7, 8, 9}) g_vt_val[i] = (void*)val_trap;
    FillTrap<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
             48, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59>::in(g_vt_owner);
    g_vt_owner[49] = (void*)owner_event;
    g_vt_owner[60] = (void*)owner_condition;

    for (int s = 0; s < 2; s++) {
        g_owner[s] = (unsigned char*)std::calloc(1, 0x308 + 16);
        g_manager[s] = (unsigned char*)std::calloc(1, 0x24 + 16);
        g_rng[s] = (unsigned char*)std::calloc(1, 0x9C8 + 16);
        // Room ahead, alike on both sides: a negative index reads before the generator.
        g_rng2[s] = (unsigned char*)std::calloc(1, 0x9C8 + 64) + 32;
        g_holder[s] = (unsigned char*)std::calloc(1, 0x40);
        *(void**)(g_holder[s] + 0x10) = g_rng[s];
        g_context_[s] = (GmChartContext*)std::calloc(1, sizeof(GmChartContext));
        g_frame[s] = (GmChartFrame*)std::calloc(1, sizeof(GmChartFrame));
    }
    twin(g_owner[0], g_owner[1], 0x308);
    twin(g_manager[0], g_manager[1], 0x24);
    twin(g_rng[0], g_rng[1], 0x9C8);
    twin(g_rng2[0], g_rng2[1], 0x9C8);
    twin(g_holder[0], g_holder[1], 0x40);
    twin(g_context_[0], g_context_[1], sizeof(GmChartContext));
    twin(g_frame[0], g_frame[1], sizeof(GmChartFrame));
    // The generator the manager draws from: through 0x002CB0A8 on one side, GmHistoryLogs::s_instance on the other.
    *(void**)(uintptr_t)0x002CB0A8 = g_holder[0];
    GmHistoryLogs::s_instance = (GmHistoryLogs*)g_holder[1];
    new (g_rng[1]) AtRandomGenerator;
    seed_generators(4357);
    // The defaults record, which FindValueOrDefault falls back on.
    g_defaults.m_registry = (AtDataRegistry*)kDefRegistry;
    reset_tree();
    g_defaults.m_link = (AtDataLink*)g_defrec;
    *(void**)(uintptr_t)0x003B3524 = &g_defaults;
    GmDesignDefaults::s_instance = &g_defaults;
    // The heap the manager allocates from.
    *(void**)(uintptr_t)0x003B3C88 = (void*)0x4EA90000;
    GmAiActionManager::s_heap = (void*)0x4EA90000;
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_lifetimes();
    run_reads();
    run_edges();
    run_setups();
    run_chart();
    run_random();
    run_helpers();
    report_entries();
    std::printf("%s: %d cases, %d calls crashed alike; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_cases, g_crashed_alike, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
