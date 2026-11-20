// Differential oracle for the Flash player's display manager, its contexts, sprite nodes, variables and remote function calls (Windows x86, local only). See docs/verification.md.
//
//   oracle_fpdisplay <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate] [--trace] [--quick]
//
// Real scripts on a real display: every user interface archive in the JAM cases beside the tokenizer cases (..\oracle-jam). Each one's sprites (.AFP, .XFF) and scripts (.AFA, .AIS) are loaded once by the
// recovered loaders (suites afd and aft). Then, for every script block, each side builds a level of its own from them, by its own code: the level's root sprite node (sprite 0) is constructed, named
// "_level0" and taken to its first frame, which places the frame's nodes from the real .XFF tags; the display manager has that level and the root as context and target. The block is run on the level's own
// action script machine, the original's code on one side, the recovered source on the other, each with its own display manager, so that every GetVariable, SetVariable, target, path, frame, label and
// property a real script asks for is answered by real code on each side, and scripts that call frames run the frames' real actions. The block is run whole, and once more for every instruction with that
// instruction made an End. After the build and after each run the two worlds are compared: every node of the tree, its pending and children lists in both directions, each sprite's variables with their text,
// the dead nodes, the queued actions, the contexts, the machine and its stack, and the matrices.
//
// What stands in, the same on both sides, each call logged: the nodes' virtual slots (Init, Acquire, Update, Replace), a placement's matrix, sounds, what LoadMovie loads, the matrix functions (a 4x4 of floats
// here) and the remote functions' game code (a translation and handlers, logging what they are given), and the clock. A sprite's Init stand-in sets what the original's does that this code reads and
// initialises its variables with each side's own code; Acquire gives a sprite node its sprite from the loaded table. Update does nothing: nodes placed do not run their own frames.
//
// Then written frames, sprites and scripts for every path the real files do not reach, and each recovered function called directly with written arguments.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioBank.h"
#include "at/AtCoordSys.h"
#include "at/AtEmitter.h"
#include "at/AtFlashLevel.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtRandom.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate, g_trace, g_quick;
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
template <class M> static uint32_t addr_of(M m) {
    static_assert(sizeof(M) == 4, "a plain member function");
    union { M m; uint32_t a; } u;
    u.m = m;
    return u.a;
}

#include "oracle_doubles.h"

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef void(__fastcall* Fn3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef int(__fastcall* FnI0)(void*, void*);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI2)(void*, void*, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef void*(__fastcall* FnP1)(void*, void*, uintptr_t);
typedef void*(__fastcall* FnP2)(void*, void*, uintptr_t, uintptr_t);
typedef void*(__fastcall* FnP3)(void*, void*, uintptr_t, uintptr_t, uintptr_t);
typedef float(__fastcall* FnF0)(void*, void*);
typedef void(__fastcall* FnInit)(void*, void*, int, int, void*, void*, int, int);

static int g_heap_token, g_heap_b;

// Harness allocations while a side is recording are not the code under test's.
struct Quiet {
    bool was;
    Quiet() : was(g_recording) { g_recording = false; }
    ~Quiet() { g_recording = was; }
};

// ---- the call log's text ------------------------------------------------------------------------------------------------

static uint32_t hash_text(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}
static void log_text(int code, const char* s) {
    if (!g_recording) return;
    if (!s) { g_log[g_side].add(code, 0xFFFFFFFF); return; }
    g_log[g_side].add(code, hash_text(s));
    g_log[g_side].add('L', (uint32_t)std::strlen(s));
}

// The log holds 8192 entries: past half of it, what is there is folded into a hash, at the same point on both sides when they agree.
static uint32_t g_fold[2];
static void fold_log() {
    Log& l = g_log[g_side];
    if (l.n < (int)(sizeof l.v / 4) / 2) return;
    uint32_t h = g_fold[g_side];
    for (int i = 0; i < l.n; i++) h = (h ^ l.v[i]) * 16777619u;
    g_fold[g_side] = h;
    l.n = 0;
    l.add('H', h);
}

// Every heap push is counted: past the budget a fault, so that a script that loops forever with allocations stops on both sides at the same point.
static int g_pushes[2], g_push_limit = 200000;
static int g_fail_at_push, g_push_count;
static void count_push() {
    fold_log();
    if (g_fail_at_push && ++g_push_count == g_fail_at_push) g_fail_alloc = true;
    if (++g_pushes[g_side] > g_push_limit) *(volatile int*)nullptr = 0;
}
static void __cdecl push_counting(void* h) {
    if (!g_recording) return;
    g_log[g_side].add('P', (uint32_t)(uintptr_t)h);
    count_push();
}
static void callout(int code, uint32_t a = 0) {
    if (!g_recording) return;
    g_log[g_side].add(code, a);
    fold_log();
}

// ---- matrices: a 4x4 of floats each, two to a node ---------------------------------------------------------------------

struct Mat { float f[16]; };
class FakePool : public AtMatrixPool {
public:
    Mat mats[2];
    int id = -1;
    void Slot0() override { callout('0'); }
    void Reserve(unsigned int count) override { callout('1', count); }
    void Clear() override { callout('2'); }
    unsigned int Count() override { callout('3'); return 2; }
    AtMatrix* At(unsigned int index) override { callout('4', index); return (AtMatrix*)&mats[index & 1]; }
};
static void identity(Mat* m) { std::memset(m, 0, sizeof *m); m->f[0] = m->f[5] = m->f[10] = m->f[15] = 1.0f; }
static uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float __cdecl m_get(const AtMatrix* m, int row, int column) {
    callout('g', (uint32_t)(row * 4 + column));
    return ((const Mat*)m)->f[(row * 4 + column) & 15];
}
static void __cdecl m_set(AtMatrix* m, int row, int column, float v) {
    callout('s', (uint32_t)(row * 4 + column));
    callout('v', bits(v));
    ((Mat*)m)->f[(row * 4 + column) & 15] = v;
}
static void __cdecl m_get_position(const AtMatrix* m, AtVector3* p) {
    callout('G');
    const Mat* t = (const Mat*)m;
    p->x = t->f[12]; p->y = t->f[13]; p->z = t->f[14];
}
static void __cdecl m_identity(AtMatrix* m) { callout('I'); identity((Mat*)m); }
static void __cdecl m_translation(AtMatrix* m, const AtVector3* p) {
    callout('T', bits(p->x) ^ bits(p->y) * 3 ^ bits(p->z) * 7);
    Mat* t = (Mat*)m;
    t->f[12] = p->x; t->f[13] = p->y; t->f[14] = p->z;
}

// ---- the worlds -------------------------------------------------------------------------------------------------------

static AtFlashApplication g_app[2];
static AtFlashEnvironment g_env[2];
static unsigned char* g_level[2];
static int g_next_id[2];
static AtFlashSpriteTable* g_sprites;
static AtFlashScriptTable* g_scripts;
static AtTextTable* g_strings;
static AtFpActionScriptVM* g_vm[2];
static int g_time[2];
static AtFlashLevel* level_of(int side) { return (AtFlashLevel*)g_level[side]; }

static uint32_t g_vft[3];   // the recovered sprite, shape and text vftables
static const uint32_t kVft[3] = {0x00271994, 0x00272ACC, 0x002749D8};
static uint32_t class_of(const void* node, int side) {
    const uint32_t v = *(const uint32_t*)node;
    if (side == 0) return v;
    for (int i = 0; i < 3; i++) if (v == g_vft[i]) return kVft[i];
    return v;
}

static int id_of(const void* node) {
    if (!node) return -2;
    const AtFpCharacterNode* n = (const AtFpCharacterNode*)node;
    const unsigned char* level = g_level[g_side];
    if (level && node == level_of(g_side)->m_player) return 0;
    return n->m_matrices ? ((FakePool*)n->m_matrices)->id : -1;
}

static bool has_text(const AtFpActionScriptValue* v) {
    return v->m_text != nullptr && (v->m_type == 0 || v->m_haveTextInt || v->m_haveTextReal);
}
static void log_value(const AtFpActionScriptValue* v) {
    if (!g_recording) return;
    Log& l = g_log[g_side];
    l.add('t', (uint32_t)v->m_type);
    l.add('i', (uint32_t)v->m_int);
    l.add('r', (uint32_t)v->m_unknown14);
    log_text('x', has_text(v) ? v->m_text : "-");
}
static void log_stack() {
    if (!g_recording || !g_vm[g_side]) return;
    const AtFpActionScriptStack& s = g_vm[g_side]->m_stack;
    g_log[g_side].add('S', (uint32_t)s.m_top);
    for (int i = 0; i <= s.m_top && i < 30; i++) log_value(&s.m_values[i]);
}

// ---- the stand-ins for the nodes' slots -------------------------------------------------------------------------------

static uint32_t g_original_variables_init = 0x000216E0;
static void init_common(AtFpCharacterNode* n, int kind, int character, void* parent, void* scripts, int frame, int state) {
    callout('i', (uint32_t)kind);
    callout('c', (uint32_t)character);
    callout('p', (uint32_t)id_of(parent));
    callout('l', scripts == &level_of(g_side)->m_scripts);
    callout('f', (uint32_t)frame);
    callout('e', (uint32_t)state);
    if (n->m_initialized) callout('!');
    n->m_kind = kind;
    n->m_character = character;
    n->m_parent = (AtFpCharacterNode*)parent;
    n->m_scripts = (AtFlashScriptTable*)scripts;
    n->m_placedFrame = frame;
    n->m_state = state;
    n->m_initialized = 1;
    if (!n->m_matrices) {
        Quiet q;
        FakePool* p = new FakePool;
        identity(&p->mats[0]);
        identity(&p->mats[1]);
        p->id = ++g_next_id[g_side];
        n->m_matrices = p;
    }
    callout('#', (uint32_t)id_of(n));
}
static void __fastcall d_init_sprite(AtFpSpriteCharacterNode* n, void*, int kind, int character, void* parent, void* scripts, int frame, int state) {
    callout('Z');
    init_common(n, kind, character, parent, scripts, frame, state);
    n->m_branchInitialized = 1;
    n->m_playing = 1;
    n->m_frame = -1;
    n->m_variables.m_heap = n->m_heap ? n->m_heap : AtFpCharacterNode::s_heap;
    if (g_side == 0) at<Fn0>(g_original_variables_init)(&n->m_variables, nullptr);
    else n->m_variables.Init();
    n->m_spriteInitialized = 1;
}
static void __fastcall d_init_shape(AtFpCharacterNode* n, void*, int kind, int character, void* parent, void* scripts, int frame, int state) {
    callout('Y');
    init_common(n, kind, character, parent, scripts, frame, state);
}
static void __fastcall d_init_text(AtFpCharacterNode* n, void*, int kind, int character, void* parent, void* scripts, int frame, int state) {
    callout('X');
    init_common(n, kind, character, parent, scripts, frame, state);
}
static void __fastcall d_acquire_sprite(AtFpSpriteCharacterNode* n, void*) {
    callout('a', (uint32_t)id_of(n));
    if (n->m_unknown34) return;
    if (g_sprites && (unsigned)n->m_character < g_sprites->m_itemCount) {
        AtFpSprite* s = (AtFpSprite*)g_sprites->m_items[n->m_character];
        AtFlashFrameTable& frames = g_sprites->m_frames;
        for (unsigned i = 0; s && i < s->m_refCount; i++) {
            if (!s->m_refs[i].m_frameData && (unsigned)s->m_refs[i].m_frame < frames.m_itemCount) s->m_refs[i].m_frameData = (AtFlashFrame*)frames.m_items[s->m_refs[i].m_frame];
        }
        n->m_sprite = s;
    }
    n->m_unknown34 = 1;
}
static void __fastcall d_acquire_other(AtFpCharacterNode* n, void*) { callout('b', (uint32_t)id_of(n)); }
static void __fastcall d_update(AtFpCharacterNode* n, void*) { callout('u', (uint32_t)id_of(n)); }
static void __fastcall d_replace(AtFpCharacterNode* n, void*, const int* kc, int frame) {
    callout('k', (uint32_t)id_of(n));
    callout('K', (uint32_t)kc[0]);
    callout('C', (uint32_t)kc[1]);
    callout('F', (uint32_t)frame);
    n->m_kind = kc[0];
    n->m_character = kc[1];
    n->m_placedFrame = frame;
    n->m_scripted = 0;
}
static void __fastcall d_set_matrix(AtFpCharacterNode* n, void*, const float* t) {
    callout('M', (uint32_t)id_of(n));
    for (int i = 0; i < 6; i++) callout('m', bits(t[i]));
    if (n->m_matrices) {
        Mat* m = (Mat*)((FakePool*)n->m_matrices)->mats;
        m->f[0] = t[0]; m->f[1] = t[1]; m->f[4] = t[2]; m->f[5] = t[3]; m->f[12] = t[4]; m->f[13] = t[5];
    }
}
static void __fastcall d_stop_all(void* sounds, void*) { callout('q', sounds == level_of(g_side)->m_sounds); }
static void __fastcall d_play(void* sounds, void*, int sound) { callout('Q', (uint32_t)sound); callout('q', sounds == level_of(g_side)->m_sounds); }
static void __fastcall d_unload(AtFpDisplayManager* d, void*, int number) { callout('U', (uint32_t)number); callout('d', d == &g_app[g_side].m_display); }
static AtFlashLevel* __fastcall d_find_loaded(AtFpDisplayManager*, void*, const char* name) { log_text('n', name); callout('n'); return nullptr; }
static AtFlashLevel* g_new_level[2];
static AtFlashLevel* __fastcall d_new_level(AtFpDisplayManager*, void*, const char* name, int a) { log_text('W', name); callout('W', (uint32_t)a); return g_new_level[g_side]; }
static void __fastcall d_attach(AtFpDisplayManager*, void*, AtFlashLevel* level, int number) { callout('A', level == g_new_level[g_side]); callout('A', (uint32_t)number); }
static unsigned __cdecl d_time() { callout('t'); return 1000u + 37u * (unsigned)++g_time[g_side]; }
unsigned int AtPlatformMilliseconds() { return d_time(); }

// ---- remote functions: the game's translation and handlers ----------------------------------------------------------

static AtFpRfcTable::Registered g_registered[2][2];
static void log_args(AtFpRfcArgs* args) {
    if (!g_recording) return;
    // Trace's arguments live on the machine's stack; a target's are a sprite node's variables.
    const NT_TIB* tib = (const NT_TIB*)NtCurrentTeb();
    const bool onStack = (void*)args >= tib->StackLimit && (void*)args < tib->StackBase;
    const int owner = !args ? -3 : onStack ? -4 : id_of((char*)args - offsetof(AtFpSpriteCharacterNode, m_variables));
    callout('o', (uint32_t)owner);
    if (!args || (uintptr_t)args < 0x10000) return;
    callout('O', (uint32_t)args->m_count);
    for (int i = 0; i < args->m_count && i < 64 && args->m_values; i++) {
        log_text('N', args->m_values[i].m_name);
        log_value(&args->m_values[i]);
    }
}
static bool g_identity;  // the remote table's direct calls: names as given
static void __cdecl d_translate(const char* name, char* out) {
    log_text('y', name);
    callout('y');
    const char* to = name;
    switch (g_identity ? 0 : hash_text(name) % 4) {
    case 1: to = "PRELOADmovie"; break;
    case 2: to = "event"; break;
    case 3: to = "NullFn"; break;
    }
    if (std::strcmp(name, "TraceOutput") == 0) to = name;
    // A name not found is formatted into a buffer of the original's that cannot hold two long names: past about 107 characters it writes over its return address. Long names are found here.
    if (std::strlen(name) >= 100) to = "event";
    std::strcpy(out, to);
}
static void __cdecl d_rfc_trace(AtFpRfcArgs* args) { callout('1'); log_args(args); }
static void __cdecl d_rfc_event(AtFpRfcArgs* args) { callout('2'); log_args(args); }
static void __fastcall d_rfc_preload(AtFpRfcTable* table, void*, AtFpRfcArgs* args) { callout('3', table == &g_app[g_side].m_rfc); log_args(args); }

static void set_rfc(int side) {
    AtFpRfcTable& t = g_app[side].m_rfc;
    t.m_translate = d_translate;
    g_registered[side][0] = {nullptr, nullptr, "NullFn"};
    g_registered[side][1] = {&g_registered[side][0], d_rfc_event, "EVENT"};
    t.m_first.m_next = &g_registered[side][1];
    t.m_first.m_function = d_rfc_trace;
    t.m_first.m_name = "TraceOutput";
}

// ---- comparing ---------------------------------------------------------------------------------------------------------

static void same_text(const char* what, const char* o, const char* r) {
    g_checks++;
    if (!o || !r) { eq(what, o != nullptr, r != nullptr); return; }
    eqs(what, o, r);
}

static void same_value(const AtFpActionScriptValue* o, const AtFpActionScriptValue* r) {
    eq("value's vftable", *(const uint32_t*)o, 0x00272964);
    eq("value's type", o->m_type, r->m_type);
    eq("value's int", o->m_int, r->m_int);
    eq("value's real", o->m_unknown14, r->m_unknown14);
    eq("value's text and int", o->m_haveTextInt, r->m_haveTextInt);
    eq("value's text and real", o->m_haveTextReal, r->m_haveTextReal);
    eq("value's int and real", o->m_haveIntReal, r->m_haveIntReal);
    eq("value's capacity", o->m_capacity, r->m_capacity);
    eq("value's text held", o->m_text != nullptr, r->m_text != nullptr);
    if (has_text(o) && has_text(r)) same_text("value's text", o->m_text, r->m_text);
    same_text("value's name", o->m_name, r->m_name);
    eq("value's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
}

static void same_vars(const AtFpRfcArgs* o, const AtFpRfcArgs* r) {
    eq("variables' capacity", o->m_capacity, r->m_capacity);
    eq("variables' count", o->m_count, r->m_count);
    eq("variables' heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("variables' values", o->m_values != nullptr, r->m_values != nullptr);
    if (o->m_values && r->m_values && o->m_count == r->m_count && o->m_capacity == r->m_capacity) {
        const int n = std::min<int>((int)o->m_capacity, 400);
        for (int i = 0; i < n; i++) same_value(&o->m_values[i], &r->m_values[i]);
        eq("variables' array count", ((const uint32_t*)o->m_values)[-1], ((const uint32_t*)r->m_values)[-1]);
    }
    same_value(((AtFpRfcArgs*)o)->Undefined(), ((AtFpRfcArgs*)r)->Undefined());
}

static void same_stack(const AtFpActionScriptStack* o, const AtFpActionScriptStack* r) {
    eq("stack allocated", o->m_allocated, r->m_allocated);
    eq("stack's top", o->m_top, r->m_top);
    eq("stack's capacity", o->m_capacity, r->m_capacity);
    if (o->m_values && r->m_values && o->m_capacity == r->m_capacity) {
        for (unsigned i = 0; i < o->m_capacity && i < 64; i++) {
            const AtFpActionScriptValue* a = &o->m_values[i];
            const AtFpActionScriptValue* b = &r->m_values[i];
            eq("stack value's type", a->m_type, b->m_type);
            eq("stack value's int", a->m_int, b->m_int);
            eq("stack value's real", a->m_unknown14, b->m_unknown14);
            if (has_text(a) && has_text(b)) same_text("stack value's text", a->m_text, b->m_text);
        }
    }
}

static int g_ids_seen;
static void same_node(const AtFpCharacterNode* o, const AtFpCharacterNode* r, int depth);

// Walks a list both ways and compares its nodes; deep for the tree, shallow for the dead.
static void same_list(const char* what, const AtFlashList& a, const AtFlashList& b, int depth, bool deep) {
    int n0 = 0, n1 = 0;
    const AtFlashLink* x = a.m_head;
    const AtFlashLink* y = b.m_head;
    eq("list head's prev", x ? x->m_prev != nullptr : 0, y ? y->m_prev != nullptr : 0);
    while (x && y && n0 < 2000) {
        const AtFpCharacterNode* nx = (const AtFpCharacterNode*)((const char*)x - 4);
        const AtFpCharacterNode* ny = (const AtFpCharacterNode*)((const char*)y - 4);
        if (deep) same_node(nx, ny, depth + 1);
        else {
            g_side = 0; const int ix = id_of(nx); g_side = 1; const int iy = id_of(ny); g_side = 0;
            eq("dead node", ix, iy);
            eq("dead node's state", nx->m_state, ny->m_state);
            eq("dead node's kind", nx->m_kind, ny->m_kind);
        }
        if (!x->m_next) eq("list tail", a.m_tail == x, 1);
        if (!y->m_next) eq("list tail", b.m_tail == y, 1);
        x = x->m_next; y = y->m_next; n0++; n1++;
    }
    eq(what, x != nullptr, y != nullptr);
    // Backwards: the same count.
    int b0 = 0, b1 = 0;
    for (const AtFlashLink* p = a.m_tail; p && b0 < 2000; p = p->m_prev) b0++;
    for (const AtFlashLink* p = b.m_tail; p && b1 < 2000; p = p->m_prev) b1++;
    eq("list backwards", b0, b1);
}

static void same_node(const AtFpCharacterNode* o, const AtFpCharacterNode* r, int depth) {
    if (depth > 40) return;
    g_ids_seen++;
    eq("node's class", class_of(o, 0), class_of(r, 1));
    g_side = 0; const int io = id_of(o), po = id_of(o->m_parent);
    g_side = 1; const int ir = id_of(r), pr = id_of(r->m_parent);
    g_side = 0;
    eq("node id", io, ir);
    eq("node's parent", po, pr);
    eq("node initialised", o->m_initialized, r->m_initialized);
    eq("node's kind", o->m_kind, r->m_kind);
    eq("node's character", o->m_character, r->m_character);
    eq("node's state", o->m_state, r->m_state);
    eq("node scripted", o->m_scripted, r->m_scripted);
    eq("node's scripts", o->m_scripts == &level_of(0)->m_scripts, r->m_scripts == &level_of(1)->m_scripts);
    eq("node's placed frame", o->m_placedFrame, r->m_placedFrame);
    eq("node's depth", o->m_depth, r->m_depth);
    eq("node's +34", o->m_unknown34, r->m_unknown34);
    eq("node's color offset", *(const uint32_t*)o->m_colorOffset, *(const uint32_t*)r->m_colorOffset);
    for (int i = 0; i < 4; i++) eq("node's color scale", bits(o->m_colorScale[i]), bits(r->m_colorScale[i]));
    same_text("node's name", o->m_name, r->m_name);
    eq("node's ratio", o->m_ratio, r->m_ratio);
    eq("node's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("node's matrices", o->m_matrices != nullptr, r->m_matrices != nullptr);
    if (o->m_matrices && r->m_matrices) {
        const FakePool* a = (const FakePool*)o->m_matrices;
        const FakePool* b = (const FakePool*)r->m_matrices;
        for (int k = 0; k < 2; k++) for (int i = 0; i < 16; i++) eq("node's matrix", bits(a->mats[k].f[i]), bits(b->mats[k].f[i]));
    }
    if (class_of(o, 0) == kVft[0] && class_of(r, 1) == kVft[0]) {
        const AtFpSpriteCharacterNode* a = (const AtFpSpriteCharacterNode*)o;
        const AtFpSpriteCharacterNode* b = (const AtFpSpriteCharacterNode*)r;
        eq("branch initialised", a->m_branchInitialized, b->m_branchInitialized);
        eq("sprite initialised", a->m_spriteInitialized, b->m_spriteInitialized);
        eq("sprite playing", a->m_playing, b->m_playing);
        eq("sprite's frame", a->m_frame, b->m_frame);
        eq("sprite's sprite", (uintptr_t)a->m_sprite, (uintptr_t)b->m_sprite);
        same_list("children", a->m_children, b->m_children, depth, true);
        same_list("pending", a->m_pending, b->m_pending, depth, true);
        if (a->m_spriteInitialized && b->m_spriteInitialized) same_vars(&a->m_variables, &b->m_variables);
    } else if (class_of(o, 0) == kVft[1]) {
        eq("shape's +58", ((const AtFpShapeCharacterNode*)o)->m_unknown58, ((const AtFpShapeCharacterNode*)r)->m_unknown58);
    }
}

static void same_world() {
    g_ids_seen = 0;
    AtFlashLevel* a = level_of(0);
    AtFlashLevel* b = level_of(1);
    same_node((AtFpCharacterNode*)(void*)a->m_player, (AtFpCharacterNode*)(void*)b->m_player, 0);
    eq("level's root", a->m_root == (void*)a->m_player, b->m_root == (void*)b->m_player);
    eq("level's instances", a->m_read, b->m_read);
    same_list("dead", g_env[0].m_dead, g_env[1].m_dead, 0, false);
    // Queued actions.
    const AtFlashLink* x = g_env[0].m_actions.m_items.m_head;
    const AtFlashLink* y = g_env[1].m_actions.m_items.m_head;
    while (x && y) {
        const AtFpActionQueue::Item* i = (const AtFpActionQueue::Item*)x;
        const AtFpActionQueue::Item* j = (const AtFpActionQueue::Item*)y;
        eq("action's script", i->m_script, j->m_script);
        g_side = 0; const int ix = id_of(i->m_node); g_side = 1; const int iy = id_of(j->m_node); g_side = 0;
        eq("action's node", ix, iy);
        x = x->m_next; y = y->m_next;
    }
    eq("actions", x != nullptr, y != nullptr);
    eq("actions' tail", g_env[0].m_actions.m_items.m_tail != nullptr, g_env[1].m_actions.m_items.m_tail != nullptr);
    // Contexts.
    const AtFpDisplayMgrContextStack& c = g_app[0].m_display.m_contexts;
    const AtFpDisplayMgrContextStack& d = g_app[1].m_display.m_contexts;
    eq("contexts", c.m_count, d.m_count);
    for (unsigned k = 0; k < c.m_count && k < 50 && k < d.m_count; k++) {
        g_side = 0; const int c0 = id_of(c.m_entries[k].m_context), t0 = id_of(c.m_entries[k].m_target);
        g_side = 1; const int c1 = id_of(d.m_entries[k].m_context), t1 = id_of(d.m_entries[k].m_target);
        g_side = 0;
        eq("context", c0, c1);
        eq("target", t0, t1);
    }
    if (g_vm[0] && g_vm[1]) {
        eq("vm's application", g_vm[0]->m_application == &g_app[0], g_vm[1]->m_application == &g_app[1]);
        same_stack(&g_vm[0]->m_stack, &g_vm[1]->m_stack);
    }
    eq("random cursor", *(uint32_t*)(uintptr_t)0x002C60C0, g_atRandomCursor);
    eq("clock", g_time[0], g_time[1]);
}

static void check_logs(const char* what) {
    for (int side = 0; side < 2; side++) eq("log room", g_log[side].n < (int)(sizeof g_log[side].v / 4) - 4, 1);
    if (g_failures < 6) {
        for (int i = 0; i < g_log[0].n && i < g_log[1].n; i++) {
            if (g_log[0].v[i] != g_log[1].v[i]) {
                std::printf("logs part at %d:\n", i);
                for (int side = 0; side < 2; side++) {
                    for (int k = std::max(0, (i & ~1) - 24); k < g_log[side].n && k < i + 24; k += 2) std::printf("%c%X ", (char)g_log[side].v[k], g_log[side].v[k + 1]);
                    std::printf("\n");
                }
                break;
            }
        }
    }
    eq("folded log", g_fold[0], g_fold[1]);
    g_fold[0] = g_fold[1] = 0;
    compare_logs(what);
}

// ---- running a side ---------------------------------------------------------------------------------------------------

// A watchdog: a run that takes too long is made to fault, on the side it is in; the oracle then reports it.
static HANDLE g_main_thread;
static volatile LONG g_run_serial, g_hung;
static void __cdecl hang_fault() { g_hung = 1; *(volatile int*)nullptr = 0; }
static DWORD WINAPI watchdog(void*) {
    LONG last = -1;
    int still = 0;
    for (;;) {
        Sleep(1000);
        const LONG now = g_run_serial;
        if (now == last && now >= 0 && g_expect_crash) {
            if (++still >= 20) {
                SuspendThread(g_main_thread);
                CONTEXT c = {};
                c.ContextFlags = CONTEXT_CONTROL;
                GetThreadContext(g_main_thread, &c);
                c.Eip = (DWORD)(uintptr_t)&hang_fault;
                SetThreadContext(g_main_thread, &c);
                ResumeThread(g_main_thread);
                still = 0;
            }
        } else {
            still = 0;
        }
        last = now;
    }
}

template <class F> static bool run_side(int side, F f) {
    g_pushes[side] = 0;
    g_push_count = 0;
    g_fail_alloc = false;
    InterlockedIncrement(&g_run_serial);
    if (g_trace) std::printf("run %d [%s]\n", side, g_context.c_str());
    const bool crashed = guarded([&] { On on(side); f(); });
    if ((g_trace || g_real) && crashed) std::printf("crashed %d [%s]\n", side, g_context.c_str());
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    return crashed;
}

// ---- building a level -------------------------------------------------------------------------------------------------

// Written worlds give the root a heap of its own, which its children and names take; real ones do not, so that nodes fall back on the class's.
static void* g_root_heap;
static AtFpSpriteCharacterNode* root_of(int side) { return (AtFpSpriteCharacterNode*)(void*)level_of(side)->m_player; }

// The world on one side, built by that side's code. Logging is on: the build is compared too.
static void build_side(int side, AtFlashScriptTable* scripts, AtTextTable* strings) {
    {
        Quiet q;
        if (!g_level[side]) g_level[side] = (unsigned char*)std::calloc(1, sizeof(AtFlashLevel) + 0x40);
    }
    std::memset(g_level[side], 0, sizeof(AtFlashLevel));
    std::memset(&g_env[side], 0, sizeof g_env[side]);
    std::memset(&g_app[side].m_display, 0, sizeof g_app[side].m_display);
    g_next_id[side] = 0;
    AtFlashLevel* L = level_of(side);
    std::memcpy(&L->m_scripts, scripts, sizeof(AtFlashScriptTable));
    AtFpActionScriptVM* vm = &L->m_scripts.m_vm;
    if (side == 0) {
        at<FnCtor>(0x0001CC80)(vm, nullptr);
        at<Fn2>(0x0001CCC0)(vm, nullptr, (uintptr_t)&g_app[0], (uintptr_t)strings);
    } else {
        new (vm) AtFpActionScriptVM;
        vm->Init(&g_app[1], strings);
    }
    g_vm[side] = vm;
    L->m_environment = &g_env[side];
    g_env[side].m_application = &g_app[side];
    g_env[side].m_actions.m_heap = &g_heap_b;
    AtFpSpriteCharacterNode* root = root_of(side);
    if (side == 0) at<FnCtor>(0x00014300)(root, nullptr);
    else new (root) AtFpSpriteCharacterNode;
    L->m_root = root;
    root->m_heap = g_root_heap;
    d_init_sprite(root, nullptr, 2, 0, nullptr, &L->m_scripts, 0, 1);
    if (side == 0) at<Fn1>(0x00020E10)(root, nullptr, (uintptr_t)"_level0");
    else root->SetName("_level0");
    d_acquire_sprite(root, nullptr);
    AtFpDisplayManager& d = g_app[side].m_display;
    d.m_active.m_head = d.m_active.m_tail = &L->m_link;
    d.m_contexts.m_count = 1;
    d.m_contexts.m_entries[0].m_context = d.m_contexts.m_entries[0].m_target = root;
    d.m_environment = &g_env[side];
    d.m_maxLevel = 9;
    set_rfc(side);
    if (side == 0) at<Fn1>(0x00015130)(root, nullptr, 0);
    else root->GotoFrame(0);
}

// Frees what a world holds that its code made, quietly; not after a crash.
static void free_node(AtFpCharacterNode* n, bool self) {
    if (!n) return;
    const uint32_t v = *(uint32_t*)n;
    const bool sprite = v == kVft[0] || v == g_vft[0];
    if (sprite) {
        AtFpSpriteCharacterNode* s = (AtFpSpriteCharacterNode*)n;
        for (AtFlashLink* l = s->m_children.m_head; l;) { AtFlashLink* next = l->m_next; free_node((AtFpCharacterNode*)((char*)l - 4), true); l = next; }
        for (AtFlashLink* l = s->m_pending.m_head; l;) { AtFlashLink* next = l->m_next; free_node((AtFpCharacterNode*)((char*)l - 4), true); l = next; }
        if (s->m_variables.m_values && s->m_spriteInitialized) std::free((char*)s->m_variables.m_values - 4);
    }
    delete (FakePool*)n->m_matrices;
    std::free(n->m_name);
    if (self) std::free(n);
}
static void free_world(int side) {
    Quiet q;
    free_node((AtFpCharacterNode*)(void*)level_of(side)->m_player, false);
    for (AtFlashLink* l = g_env[side].m_dead.m_head; l;) { AtFlashLink* next = l->m_next; free_node((AtFpCharacterNode*)((char*)l - 4), true); l = next; }
    for (AtFlashList* list : {&g_env[side].m_actions.m_items, &g_env[side].m_actions.m_free}) {
        for (AtFlashLink* l = list->m_head; l;) { AtFlashLink* next = l->m_next; std::free(l); l = next; }
    }
    AtFpActionScriptVM* vm = &level_of(side)->m_scripts.m_vm;
    if (side == 0) at<Fn0>(0x0001EF00)(vm, nullptr);
    else vm->~AtFpActionScriptVM();
    g_vm[side] = nullptr;
}

static unsigned g_runs, g_steps, g_crash_runs, g_hangs;
// What real scripts reach, on the original's side: counted over whole runs of real blocks.
static bool g_real, g_whole;
static unsigned g_stat_runs, g_stat_nodes, g_stat_vars, g_stat_runs_vars, g_stat_reports, g_stat_actions, g_stat_targets, g_stat_dead;
static void count_vars(const AtFpCharacterNode* n, unsigned* nodes, unsigned* vars, int depth) {
    if (!n || depth > 40) return;
    (*nodes)++;
    if (class_of(n, 0) != kVft[0]) return;
    const AtFpSpriteCharacterNode* s = (const AtFpSpriteCharacterNode*)n;
    if (s->m_spriteInitialized) *vars += (unsigned)s->m_variables.m_count;
    for (const AtFlashLink* l = s->m_children.m_head; l; l = l->m_next) count_vars((const AtFpCharacterNode*)((const char*)l - 4), nodes, vars, depth + 1);
}
static void count_real() {
    if (!g_real || !g_whole) return;
    unsigned nodes = 0, vars = 0;
    count_vars((const AtFpCharacterNode*)(void*)level_of(0)->m_player, &nodes, &vars, 0);
    g_stat_runs++;
    g_stat_nodes += nodes;
    g_stat_vars += vars;
    if (vars) g_stat_runs_vars++;
    if (g_message[0][0]) g_stat_reports++;
    for (const AtFlashLink* l = g_env[0].m_actions.m_items.m_head; l; l = l->m_next) g_stat_actions++;
    for (const AtFlashLink* l = g_env[0].m_dead.m_head; l; l = l->m_next) g_stat_dead++;
    const AtFpDisplayMgrContextStack& c = g_app[0].m_display.m_contexts;
    if (c.m_count == 1 && c.m_entries[0].m_target != c.m_entries[0].m_context) g_stat_targets++;
}

// Both worlds built, f run on each, compared, and freed.
template <class F> static void with_worlds(AtFlashScriptTable* scripts, AtTextTable* strings, unsigned cursor, F f) {
    *(uint32_t*)(uintptr_t)0x002C60C0 = cursor;
    g_atRandomCursor = cursor;
    g_time[0] = g_time[1] = 0;
    const bool b0 = run_side(0, [&] { build_side(0, scripts, strings); });
    const bool b1 = run_side(1, [&] { build_side(1, scripts, strings); });
    eq("build crash", b0, b1);
    if (b0 || b1) { check_logs("build"); return; }
    same_world();
    check_logs("build");
    g_hung = 0;
    const bool c0 = run_side(0, [&] { f(0); });
    const bool h0 = g_hung != 0;
    g_hung = 0;
    const bool c1 = run_side(1, [&] { f(1); });
    const bool h1 = g_hung != 0;
    g_hung = 0;
    eq("hang", h0, h1);
    eq("crash", c0, c1);
    if (h0 || h1) {
        g_hangs++;
        g_log[0].n = g_log[1].n = 0;
        g_message[0][0] = g_message[1][0] = 0;
        g_fold[0] = g_fold[1] = 0;
        return;  // Where each was stopped is not comparable.
    }
    same_world();
    count_real();
    check_logs("run");
    g_runs++;
    if (c0 || c1) { g_crash_runs++; return; }
    free_world(0);
    free_world(1);
    g_log[0].n = g_log[1].n = 0;
}

static void run_block(AtFpScript* script, unsigned cursor) {
    with_worlds(g_scripts, g_strings, cursor, [&](int side) {
        AtFpActionScriptVM* vm = &level_of(side)->m_scripts.m_vm;
        if (side == 0) at<Fn1>(0x0001F020)(vm, nullptr, (uintptr_t)script);
        else vm->Run(script);
    });
}

static const unsigned char kEndInstruction[4] = {0, 0, 0, 0};
static void run_steps(AtFpScript* script, unsigned cursor) {
    const std::string base = g_context;
    g_whole = true;
    run_block(script, cursor);
    g_whole = false;
    if (g_quick) return;
    std::vector<unsigned char*> pointers(script->m_instructions, script->m_instructions + script->m_read);
    unsigned char** saved = script->m_instructions;
    for (unsigned k = 0; k < script->m_read; k++) {
        std::vector<unsigned char*> cut = pointers;
        cut[k] = (unsigned char*)kEndInstruction;
        script->m_instructions = cut.data();
        g_context = base + " to " + std::to_string(k);
        run_block(script, cursor);
        g_steps++;
    }
    script->m_instructions = saved;
    g_context = base;
}

// ---- written scripts --------------------------------------------------------------------------------------------------

struct Asm {
    std::vector<std::vector<unsigned char>> code;
    std::vector<unsigned char*> pointers;
    AtFpScript script;
    ~Asm() { script.m_instructions = nullptr; script.m_bytes = nullptr; script.m_read = 0; script.m_instructionCount = 0; }
    bool keep = false;  // each GetProperty's result traced, so that it is logged
    Asm& op(unsigned c) {
        code.push_back({(unsigned char)c, (unsigned char)(c >> 8)});
        if (keep && c == 0x22) code.push_back({0x26, 0});
        return *this;
    }
    Asm& op(unsigned c, std::vector<unsigned char> data) {
        std::vector<unsigned char> i = {(unsigned char)c, 0, (unsigned char)data.size(), (unsigned char)(data.size() >> 8)};
        i.insert(i.end(), data.begin(), data.end());
        code.push_back(i);
        return *this;
    }
    static void put32(std::vector<unsigned char>& d, uint32_t v) { for (int i = 0; i < 4; i++) d.push_back((unsigned char)(v >> (8 * i))); }
    Asm& i(int v) { std::vector<unsigned char> d = {7, 0}; put32(d, (uint32_t)v); return op(0x96, d); }
    Asm& f(float v) { std::vector<unsigned char> d = {1, 0}; put32(d, bits(v)); return op(0x96, d); }
    Asm& s(const std::string& t) { std::vector<unsigned char> d = {0, 0}; d.insert(d.end(), t.begin(), t.end()); d.push_back(0); return op(0x96, d); }
    Asm& label(unsigned c, const std::string& t) { std::vector<unsigned char> d = {0, 0}; d.insert(d.end(), t.begin(), t.end()); d.push_back(0); return op(c, d); }
    Asm& jump(unsigned c, unsigned target) { return op(c, {(unsigned char)target, (unsigned char)(target >> 8)}); }
    Asm& get(const std::string& n) { return s(n).op(0x1C); }
    Asm& set(const std::string& n, int v) { return s(n).i(v).op(0x1D); }
    Asm& sets(const std::string& n, const std::string& v) { return s(n).s(v).op(0x1D); }
    AtFpScript* done() {
        op(0);
        pointers.clear();
        for (auto& c : code) pointers.push_back(c.data());
        script.m_instructions = pointers.data();
        script.m_read = (unsigned)pointers.size();
        script.m_instructionCount = (unsigned)pointers.size();
        return &script;
    }
};

// A written level: sprites with labelled frames whose tags place, move, replace and remove nodes of each kind, play sounds and queue actions; and a table of written scripts the frames' actions run.
struct Written {
    std::vector<AtFpSprite*> sprites;
    std::vector<AtFlashFrame*> frames;
    std::vector<std::vector<AtFlashTag*>> tags;
    AtFlashSpriteTable table;
    std::vector<AtFlashResource*> spriteItems, frameItems;
    std::vector<AtFpScript*> scripts;
    std::vector<Asm*> asms;
    AtFlashScriptTable scriptTable;
};
static Written* g_written;

static AtFpPlaceObject2Tag* place(int type, unsigned depth, int kind, int character, unsigned flags, const char* name = nullptr, int ratio = -1) {
    AtFpPlaceObject2Tag* t = new AtFpPlaceObject2Tag;
    t->m_type = 1;
    t->m_unknown30 = type;
    t->m_unknown28 = (unsigned short)depth;
    t->m_unknown50 = kind;
    t->m_unknown54 = character;
    t->m_unknown34 = (unsigned char)flags;
    t->m_unknown2A = (unsigned short)(ratio >= 0 ? ratio : depth * 3 + 1);
    for (int i = 0; i < 4; i++) { t->m_colorScale[i] = 0.25f * (float)(i + 1); t->m_colorOffset[i] = (unsigned char)(i * 9 + depth); }
    const float tr[6] = {1.5f, 0.25f, -0.5f, 2.0f, 10.0f + (float)depth, -3.0f};
    std::memcpy(t->m_transform, tr, sizeof tr);
    if (name) { t->m_name = (char*)std::malloc(std::strlen(name) + 1); std::strcpy(t->m_name, name); }
    return t;
}
template <class T> static T* idtag(int type, int id) { T* t = new T; t->m_type = type; t->m_id = id; return t; }

static void make_written() {
    Written* w = g_written = new Written;
    // Scripts the frames' actions run, by id; 300 of them, most null, so that an id past 255 and a null one are run.
    w->scripts.assign(300, nullptr);
    auto script = [&](int id, Asm* a) { w->asms.push_back(a); w->scripts[id] = a->done(); };
    { Asm* a = new Asm; a->set("ran0", 1).get("ran0").s("count").get("count").i(1).op(0x0A).op(0x1D); script(0, a); }
    { Asm* a = new Asm; a->set("ran1", 2).i(1).op(0x81, {0, 0}); script(1, a); }
    { Asm* a = new Asm; a->sets("ran2", "x").label(0x8B, "/kid").set("inkid", 3).label(0x8B, "").set("after", 4); script(2, a); }
    { Asm* a = new Asm; a->set("stop", 1).op(0x07).op(0x09); script(3, a); }
    { Asm* a = new Asm; a->set("hi", 256); script(256, a); }
    // Frames of sprite 0, the root: 0 places a sprite "kid" (1), a shape, a text, sprites at 7 and 50, and moves at 11 with nothing there; 1 moves and colours, plays, queues several actions; 2 replaces (the shape
    // into a sprite), moves 7 with another ratio, replaces 50 and removes it in the same frame; 3 removes the shape, places at 11; 4 places at a depth taken, before the first, a second "kid", one with an empty
    // name, two at one depth, and removes what is pending; 5 removes 11, a place of an unknown type; 6 places an unknown kind.
    auto frame = [&](std::vector<AtFlashTag*> t) { AtFlashFrame* f = new AtFlashFrame; w->tags.push_back(t); f->m_tags = w->tags.back().data(); f->m_tagCount = (unsigned)t.size(); w->frames.push_back(f); return (int)w->frames.size() - 1; };
    std::vector<int> f0;
    f0.push_back(frame({place(0, 5, 2, 1, 1 | 2 | 4 | 8, "kid"), place(0, 3, 0, 7, 1), place(0, 9, 3, 8, 2), place(0, 7, 2, 2, 8), place(0, 50, 2, 2, 0), place(1, 11, 2, 1, 0), idtag<AtFlashActionTag>(4, 0)}));
    f0.push_back(frame({place(1, 5, 2, 1, 1 | 8), place(1, 3, 0, 7, 2), idtag<AtFlashSoundTag>(3, 4), idtag<AtFlashActionTag>(4, 0), idtag<AtFlashActionTag>(4, 1), idtag<AtFlashActionTag>(4, 5), idtag<AtFlashActionTag>(4, 256)}));
    f0.push_back(frame({place(2, 7, 2, 2, 0), place(2, 9, 3, 9, 4, "label"), place(2, 3, 2, 1, 0), place(2, 50, 2, 2, 0), idtag<AtFlashRemoveTag>(2, 50), idtag<AtFlashActionTag>(4, 3)}));
    f0.push_back(frame({idtag<AtFlashRemoveTag>(2, 3), place(0, 11, 2, 1, 0), place(1, 7, 2, 2, 8, nullptr, 77), idtag<AtFlashSoundTag>(3, 2)}));
    f0.push_back(frame({place(0, 5, 2, 2, 1), place(0, 12, 2, 1, 4, "kid"), place(0, 1, 2, 2, 4, "first"), place(0, 20, 2, 1, 4, ""), place(0, 30, 2, 2, 4, "twin"), place(0, 30, 2, 2, 4, "twin2"), place(0, 13, 0, 1, 0), idtag<AtFlashRemoveTag>(2, 13)}));
    f0.push_back(frame({idtag<AtFlashRemoveTag>(2, 11), place(7, 2, 0, 1, 0), idtag<AtFlashActionTag>(4, 2)}));
    f0.push_back(frame({place(0, 60, 5, 1, 0)}));
    // Sprite 1 ("kid"): two frames, labelled; the first places a sprite, a text and a shape.
    std::vector<int> f1;
    f1.push_back(frame({place(0, 2, 2, 1, 4, "inner"), place(0, 4, 3, 1, 0), place(0, 6, 0, 1, 0), idtag<AtFlashActionTag>(4, 0)}));
    f1.push_back(frame({idtag<AtFlashActionTag>(4, 2)}));
    // Sprite 2: one empty frame.
    std::vector<int> f2;
    f2.push_back(frame({}));
    // Each sprite's list of frames has one entry more past its end, a ghost, which only reading past the end finds.
    auto sprite = [&](const std::vector<int>& fr, std::vector<const char*> names) {
        AtFpSprite* s = new AtFpSprite;
        s->m_refCount = s->m_refCapacity = (unsigned)fr.size();
        s->m_refs = new AtFpSprite::FrameRef[fr.size() + 1];
        for (size_t i = 0; i < fr.size(); i++) { s->m_refs[i].m_frame = fr[i]; s->m_refs[i].m_frameData = nullptr; s->m_refs[i].m_name = i < names.size() && names[i] ? (char*)names[i] : nullptr; }
        s->m_refs[fr.size()].m_name = (char*)"ghost";
        s->m_refs[fr.size()].m_frame = 1;
        s->m_refs[fr.size()].m_frameData = w->frames[1];
        w->sprites.push_back(s);
    };
    sprite(f0, {"Start", nullptr, "Moved", "29", "Late", "\xB5nd", "bad"});
    sprite(f1, {"In", "OUT"});
    sprite(f2, {});
    for (AtFpSprite* s : w->sprites) w->spriteItems.push_back(s);
    for (AtFlashFrame* f : w->frames) w->frameItems.push_back(f);
    w->table.m_items = w->spriteItems.data();
    w->table.m_itemCount = (unsigned)w->spriteItems.size();
    w->table.m_frames.m_items = w->frameItems.data();
    w->table.m_frames.m_itemCount = (unsigned)w->frameItems.size();
    w->scriptTable.m_scripts = w->scripts.data();
    w->scriptTable.m_count = (unsigned)w->scripts.size();
    w->scriptTable.m_application = &g_app[1];
}

static void written(const char* name, Asm& a, unsigned cursor = 1, bool steps = true) {
    g_context = std::string("written ") + name;
    AtFlashSpriteTable* sprites = g_sprites;
    AtFlashScriptTable* scripts = g_scripts;
    g_sprites = &g_written->table;
    g_scripts = &g_written->scriptTable;
    g_root_heap = &g_heap_b;
    AtFpScript* s = a.done();
    if (steps) run_steps(s, cursor);
    else run_block(s, cursor);
    g_root_heap = nullptr;
    g_sprites = sprites;
    g_scripts = scripts;
}

static void run_written() {
    {
        // Frames: by number, label, expression, backwards; play and stop; at the end and one past it.
        Asm a;
        a.op(0x81, {1, 0}).op(0x81, {3, 0}).op(0x81, {2, 0}).op(0x81, {40, 0}).op(0x81, {7, 0}).op(0x81, {5, 0}).op(0x06).op(0x07);
        a.label(0x8C, "moved").label(0x8C, "nothing").label(0x8C, "START").label(0x8C, "Late");
        a.s("4").op(0x9F, {0}).s("Start").op(0x9F, {1}).s("_level0:3").op(0x9F, {0}).s("/kid:OUT").op(0x9F, {1}).s("/kid:2").op(0x9F, {0}).s("/none:2").op(0x9F, {0});
        a.s("12x").op(0x9F, {0}).s("").op(0x9F, {0}).s(":").op(0x9F, {0}).s("\xB5nd").op(0x9F, {0}).s("1").op(0x9F, {0}).s("05").op(0x9F, {0}).s("1").op(0x9F, {0}).s("29").op(0x9F, {0});
        a.s("1").op(0x9F, {0}).s("4x").op(0x9F, {0}).s("1").op(0x9F, {0});
        a.op(0x04).op(0x05).op(0x05).op(0x04);
        written("frames", a);
    }
    {
        Asm a;
        // Call: by number (from 1), by label, by path; frames past the end; a frame of the kid's whose script goes back to its context; a target that is not there last, since it stops the script.
        a.i(1).op(0x9E).i(2).op(0x9E).i(99).op(0x9E).i(8).op(0x9E).s("Moved").op(0x9E).s("_level0:1").op(0x9E).s("/kid:In").op(0x9E).s("/kid:1").op(0x9E).s("/kid:nope").op(0x9E);
        a.i(4).op(0x9E).s("3a").op(0x9E).s("0").op(0x9E).s("29").op(0x9E);
        a.label(0x8B, "/kid").i(2).op(0x9E).set("onkid", 1).label(0x8B, "");
        a.op(0x81, {5, 0}).i(6).op(0x9E);  // frame 5's action goes back to the context
        a.s(":1").op(0x9E).s("").op(0x9E).s("nowhere:1").op(0x9E).set("unreached", 1);
        written("calls", a);
    }
    {
        // Targets and paths: SetTarget by every form; variables on each target.
        Asm a;
        a.label(0x8B, "kid").set("a", 1).get(":x").get("/kid:b").s(":y").i(4).op(0x1D).label(0x8B, "/kid").set("b", 2).label(0x8B, "_level0").set("c", 3).label(0x8B, "_level0/kid").set("d", 4);
        a.label(0x8B, "/").set("e", 5).label(0x8B, "/kid/inner").set("f", 6).label(0x8B, "nothing").set("g", 7).label(0x8B, "").set("h", 8);
        a.label(0x8B, "_LEVEL0/KID/").set("i", 9).label(0x8B, "_level0/").set("j", 10).label(0x8B, "kid/").set("k", 11).label(0x8B, "/kid/inner/deeper").set("l", 12);
        a.label(0x8B, "_level01").set("m", 13).label(0x8B, "_level");
        a.s("/kid").op(0x20).set("n", 14).s("").op(0x20).set("o", 15);
        a.get("/kid:b").get("_level0/kid:d").get("/:e").get("kid:a").get(":c").get("nothing:x").get("UNDEFINED").get("A").get("b");
        a.s("/kid:new").i(5).op(0x1D).s("/kid/inner:z").s("deep").op(0x1D).s("none:z").i(1).op(0x1D).s(":top").i(1).op(0x1D);
        a.op(0x81, {3, 0}).label(0x8B, "/kid").set("p", 16).op(0x81, {0, 0}).set("q", 17).get("q").label(0x8B, "kid").op(0x81, {1, 0});
        written("targets", a);
    }
    {
        // Many variables: the table grows from 10 to 60, 110, 165, 247 and 347, in and out of order, names in each case and with bytes past 0x7F.
        Asm a;
        for (int k = 0; k < 260; k++) {
            char n[16];
            std::snprintf(n, sizeof n, (k & 1) ? "V%03d" : "v%03d", (k * 37) % 261);
            a.set(n, k);
        }
        a.set("\xE9t\xE9", 1).set("\xC9T\xC9", 2).set("_x", 3).set("[y", 4).set("`z", 5).set("Zz", 6).set("\x7F", 7);
        a.set("v000", -1).set("V000", -2).set("", 3).get("").get("v230").get("V037").get("\xE9T\xE9");
        a.s("x").s(std::string(300, 'q')).op(0x1D).get("x").s(std::string(40, 'n')).i(1).op(0x1D);
        written("many variables", a, 1, false);
    }
    {
        // Properties: every index, read and set, on the root and a child, with each kind of operand.
        Asm a;
        a.keep = true;
        a.s("/kid:2").op(0x9F, {0});
        for (int k = -1; k <= 15; k++) a.s("").i(k).op(0x22).s("/kid").i(k).op(0x22);
        a.s("nothing").i(0).op(0x22);
        const float vals[] = {3.5f, -20.0f, 150.0f, 0.0f, 45.0f};
        for (int k : {0, 1, 2, 3, 10, 4, 5, 13}) {
            for (float v : vals) a.s("/kid").i(k).f(v).op(0x23).s("/kid").i(k).op(0x22).s("/kid").i(10).op(0x22).s("/kid").i(2).op(0x22).s("/kid").i(3).op(0x22);
            a.s("").i(k).s("12.5").op(0x23).s("").i(k).i(7).op(0x23);
        }
        a.s("nothing").i(0).i(5).op(0x23).s("/kid").i(2).i(0).op(0x23).s("/kid").i(3).i(0).op(0x23).s("/kid").i(10).i(0).op(0x23).s("/kid").i(2).op(0x22);
        a.s("/kid").i(2).i(150).op(0x23).s("/kid").i(3).i(50).op(0x23).s("/kid").i(10).i(30).op(0x23).s("/kid").i(10).op(0x22).s("/kid").i(2).op(0x22);
        a.s("/kid/inner").i(11).op(0x22).s("/kid").i(11).op(0x22).s("/kid/inner").i(13).op(0x22);
        a.op(0x81, {1, 0}).s("/kid").i(0).op(0x22);  // a placement's matrix after a script set it
        a.op(0x81, {3, 0}).s("").i(4).op(0x22).s("").i(5).op(0x22);
        written("properties", a);
    }
    {
        // A rotation alone marks the node, so the next placement leaves its matrix.
        Asm a;
        a.s("/kid").i(10).i(30).op(0x23).op(0x81, {1, 0}).s("/kid").i(0).op(0x22);
        written("rotation then place", a);
    }
    {
        // Remote calls, each path; Trace; loading movies.
        Asm a;
        for (const char* n : {"TraceOutput", "PreloadMovie", "event", "Event", "NullFn", "Missing", "a", "bb", "ccc", "dddd"}) a.s(n).s("/kid").op(0x9A, {0xC0}).s(n).s("").op(0x9A, {0x80});
        a.s(std::string(300, 'r')).s("").op(0x9A, {0xC0}).s("x").s("nothing").op(0x9A, {0xC0});
        a.s("trace me").op(0x26).i(5).op(0x26);
        a.s("menu.swf").s("_level1").op(0x9A, {0}).s("").s("_level2").op(0x9A, {0}).s("a.swf").s("_level9").op(0x9A, {0}).s("a.swf").s("_level10").op(0x9A, {0}).s("abcdefghijk.swf").s("_level3").op(0x9A, {0});
        written("calls out", a);
    }
    {
        // Sounds; and the frame that places an unknown kind, last.
        Asm a;
        a.op(0x09).op(0x81, {3, 0}).op(0x09).op(0x81, {6, 0});
        written("sounds", a);
    }
}

// ---- direct calls -----------------------------------------------------------------------------------------------------

static AtFpActionScriptValue* make_value(int type, int i, const char* text, int side) {
    AtFpActionScriptValue* v;
    {
        Quiet q;
        v = (AtFpActionScriptValue*)std::calloc(1, 0x60);
    }
    if (side == 0) at<FnCtor>(0x00020A50)(v, nullptr);
    else new (v) AtFpActionScriptValue;
    v->m_heap = &g_heap_token;
    if (type == 0) {
        char* p;
        { Quiet q; p = (char*)std::calloc(1, std::strlen(text) + 1); std::strcpy(p, text); }
        v->SetString(p, (unsigned)std::strlen(text) + 1);
    } else if (type == 7) {
        v->m_type = 7; v->m_int = i;
    } else if (type == 1) {
        v->m_type = 1; float f = (float)i + 0.5f; std::memcpy(&v->m_unknown14, &f, 4);
    }
    return v;
}

// The variables and their values, called directly: each insert position, growth, each lookup.
static void run_variables() {
    struct Op { int kind; const char* name; int value; };
    std::vector<std::vector<Op>> plans;
    plans.push_back({{0, "b", 1}, {0, "a", 2}, {0, "c", 3}, {0, "B", 4}, {1, "A", 0}, {1, "z", 0}, {2, "c", 0}, {2, "Q", 0}});
    std::vector<Op> grow;
    for (int k = 0; k < 75; k++) { static char names[75][8]; std::snprintf(names[k], 8, "n%02d", (k * 29) % 75); grow.push_back({0, names[k], k}); }
    grow.push_back({1, "n10", 0});
    grow.push_back({3, nullptr, 0});
    plans.push_back(grow);
    plans.push_back({{1, "a", 0}, {4, "a", 0}});  // looked up and set with no room made
    plans.push_back({{0, "x", 1}, {5, nullptr, 0}, {0, "y", 2}, {1, "y", 0}, {6, nullptr, 0}});
    plans.push_back({{0, "\xE9" "a", 1}, {0, "a", 2}, {0, "Za", 3}, {0, "_a", 4}, {0, "\x80", 5}, {0, "~", 6}, {2, "\xE9" "A", 0}, {2, "za", 0}, {2, "\x7F", 0}});
    // Insert called directly: a name already there, at each place, and new ones between.
    plans.push_back({{0, "d", 1}, {0, "b", 2}, {0, "f", 3}, {0, "h", 4}, {7, "b", 5}, {7, "h", 6}, {7, "f", 7}, {7, "D", 8}, {7, "c", 9}, {7, "a", 10}, {7, "i", 11}, {7, "e", 12}, {7, "g", 13}, {2, "e", 0}});
    plans.push_back({{0, "m", 1}, {7, "m", 2}, {7, "a", 3}, {7, "z", 4}, {7, "M", 5}, {2, "m", 0}});
    int index = 0;
    for (auto& plan : plans) {
        for (int heap = 0; heap < 3; heap++) {
            for (int fail = 0; fail < (heap ? 1 : 6); fail++) {
                g_context = "variables " + std::to_string(index) + " heap " + std::to_string(heap) + " fail " + std::to_string(fail);
                AtFpRfcArgs* t[2];
                {
                    Quiet q;
                    t[0] = (AtFpRfcArgs*)std::calloc(1, 0x80);
                    t[1] = (AtFpRfcArgs*)std::calloc(1, 0x80);
                    std::memset(t[0], 0xCD, 0x80);
                    std::memset(t[1], 0xCD, 0x80);
                }
                bool crash[2] = {};
                int results[2][128] = {};
                for (int side = 0; side < 2; side++) {
                    AtFpRfcArgs* a = t[side];
                    crash[side] = run_side(side, [&] {
                        g_fail_at_push = fail;
                        // heap 2: neither the table's own heap nor the class's.
                        if (heap == 2) { *(void**)(uintptr_t)0x002C6320 = nullptr; AtFpRfcArgs::s_heap = nullptr; }
                        if (side == 0) at<Fn0>(0x00021620)(a, nullptr); else a->Construct();
                        a->m_heap = heap == 1 ? &g_heap_b : nullptr;
                        bool inited = false;
                        int r = 0;
                        for (const Op& op : plan) {
                            if (op.kind != 4 && op.kind != 1 && !inited && op.kind != 5) {
                                if (side == 0) at<Fn0>(0x000216E0)(a, nullptr); else a->Init();
                                inited = true;
                            }
                            AtFpActionScriptValue* v = make_value(op.value % 3 == 0 ? 0 : op.value % 3 == 1 ? 7 : 1, op.value, "text", side);
                            switch (op.kind) {
                            case 0: case 4:
                                if (side == 0) at<Fn2>(0x00021DD0)(a, nullptr, (uintptr_t)op.name, (uintptr_t)v); else a->Add(op.name, v);
                                break;
                            case 1:
                                results[side][r++] = side == 0 ? at<FnI2>(0x00021820)(a, nullptr, (uintptr_t)op.name, (uintptr_t)v) : a->Get(op.name, v);
                                log_value(v);
                                break;
                            case 2: {
                                AtFpActionScriptValue* f = side == 0 ? (AtFpActionScriptValue*)at<FnP1>(0x000217B0)(a, nullptr, (uintptr_t)op.name) : a->Find(op.name);
                                results[side][r++] = f == a->Undefined() ? -1 : (int)(f - a->m_values);
                                break;
                            }
                            case 3:
                                if (side == 0) at<Fn0>(0x00021680)(a, nullptr); else a->Clear();
                                inited = false;
                                break;
                            case 5:
                                if (side == 0) at<Fn0>(0x00021680)(a, nullptr); else a->Clear();
                                break;
                            case 6:
                                if (side == 0) at<Fn0>(0x00021AD0)(a, nullptr); else a->Destruct();
                                break;
                            case 7:
                                if (side == 0) at<Fn1>(0x00020B60)(v, nullptr, (uintptr_t)op.name); else v->SetName(op.name);
                                if (side == 0) at<Fn1>(0x00021860)(a, nullptr, (uintptr_t)v); else a->Insert(v);
                                break;
                            }
                        }
                        g_fail_at_push = 0;
                    });
                    *(void**)(uintptr_t)0x002C6320 = &g_heap_token;
                    AtFpRfcArgs::s_heap = &g_heap_token;
                }
                g_fail_at_push = 0;
                eq("crash", crash[0], crash[1]);
                for (int k = 0; k < 128; k++) eq("result", results[0][k], results[1][k]);
                if (!crash[0] && !crash[1]) same_vars(t[0], t[1]);
                check_logs("variables");
            }
        }
        index++;
    }
    // A value's name: set, replaced, cleared, with each heap; undefined named.
    for (int k = 0; k < 4; k++) {
        g_context = "value name " + std::to_string(k);
        AtFpActionScriptValue* v[2];
        for (int side = 0; side < 2; side++) {
            v[side] = make_value(0, 0, "abc", side);
            v[side]->m_heap = k & 1 ? &g_heap_b : nullptr;
            run_side(side, [&] {
                g_fail_alloc = k == 3;
                if (side == 0) { at<Fn1>(0x00020B60)(v[0], nullptr, (uintptr_t)"first"); at<Fn1>(0x00020B60)(v[0], nullptr, (uintptr_t)(k == 2 ? nullptr : "second")); at<Fn1>(0x00020D20)(v[0], nullptr, (uintptr_t)"undef"); }
                else { v[1]->SetName("first"); v[1]->SetName(k == 2 ? nullptr : "second"); v[1]->SetUndefinedNamed("undef"); }
            });
        }
        same_value(v[0], v[1]);
        check_logs("value name");
    }
}

// The remote function table called directly: each early return, a long name, each way a name is found or not.
static void run_rfc() {
    const char* names[] = {"TraceOutput", "traceoutput", "PreloadMovie", "EVENT", "NullFn", "Missing", "a"};
    g_identity = true;
    for (int variant = 0; variant < 8; variant++) {
        g_identity = variant != 7;
        for (const char* n : names) {
            for (int long_name = 0; long_name < 3; long_name++) {
                g_context = std::string("rfc ") + n + " variant " + std::to_string(variant) + " long " + std::to_string(long_name);
                std::string name = n;
                if (long_name == 1) name += std::string(0xFF - name.size(), 'x');
                if (long_name == 2) name += std::string(0x100 - name.size(), 'x');
                bool crashed[2] = {};
                for (int side = 0; side < 2; side++) {
                    set_rfc(side);
                    AtFpRfcTable& t = g_app[side].m_rfc;
                    if (variant == 1) t.m_first.m_function = nullptr;
                    if (variant == 2) t.m_first.m_name = nullptr;
                    if (variant == 3) { if (side == 0) *(uint32_t*)(uintptr_t)0x002B1370 = 0; else AtFpRfcTable::s_count = 0; }
                    if (variant == 4) { if (side == 0) *(uint32_t*)(uintptr_t)0x002B1378 = 0; else std::memset(&AtFpRfcTable::s_entries[0].m_method, 0, sizeof AtFpRfcTable::s_entries[0].m_method); }
                    if (variant == 5) t.m_first.m_next = nullptr;
                    if (variant == 6) g_registered[side][0].m_name = "preloadMOVIE";  // found registered, with no function: the table is not looked in
                    // A long name not found would be formatted into the original's message buffer, past its end: with the registered list cut, or translated.
                    if ((variant == 7 || variant == 5) && long_name) continue;
                    crashed[side] = run_side(side, [&] {
                        g_fail_alloc = false;
                        if (side == 0) at<Fn2>(0x0002D9E0)(&t, nullptr, (uintptr_t)name.c_str(), (uintptr_t)nullptr);
                        else t.Call(name.c_str(), nullptr);
                    });
                    // Back as main() set them.
                    *(uint32_t*)(uintptr_t)0x002B1370 = 1;
                    AtFpRfcTable::s_count = 1;
                    *(uint32_t*)(uintptr_t)0x002B1378 = (uint32_t)(uintptr_t)&d_rfc_preload;
                    union { void (AtFpRfcTable::*m)(AtFpRfcArgs*); void* p; } u = {};
                    u.p = (void*)&d_rfc_preload;
                    AtFpRfcTable::s_entries[0].m_method = u.m;
                }
                eq("crash", crashed[0], crashed[1]);
                check_logs("rfc");
            }
        }
    }
}

// The stack of contexts, the queue and the lists, called directly at each count.
static void run_contexts() {
    for (unsigned count : {0u, 1u, 2u, 49u, 50u, 51u, 200u}) {
        for (int method = 0; method < 5; method++) {
            g_context = "contexts " + std::to_string(count) + " method " + std::to_string(method);
            AtFpDisplayMgrContextStack* s[2];
            int r[2][4] = {};
            for (int side = 0; side < 2; side++) {
                // 16 words of the harness's ahead of it, so that what a count of 0 reads before it is the same on both sides.
                { Quiet q; s[side] = (AtFpDisplayMgrContextStack*)((uint32_t*)std::calloc(1, sizeof(AtFpDisplayMgrContextStack) + 0x840) + 16); }
                ((uint32_t*)s[side])[-1] = 0x4444;
                for (int k = 0; k < 52; k++) { ((uint32_t*)s[side])[k * 2 + 1] = 0x1000 + k; ((uint32_t*)s[side])[k * 2 + 2] = 0x2000 + k; }
                ((uint32_t*)s[side])[0] = 0x777;
                s[side]->m_count = count;
                run_side(side, [&] {
                    AtFpDisplayMgrContextStack::Entry e = {};
                    switch (method) {
                    case 0: if (side == 0) at<Fn2>(0x00016080)(s[0], nullptr, 0x55, 0x66); else s[1]->Push((void*)0x55, (void*)0x66); break;
                    case 1: if (side == 0) at<Fn1>(0x000160E0)(s[0], nullptr, (uintptr_t)&e); else s[1]->GetCurrentContext(&e); r[side][0] = (int)(uintptr_t)e.m_context; r[side][1] = (int)(uintptr_t)e.m_target; break;
                    case 2: if (side == 0) at<Fn1>(0x00016130)(s[0], nullptr, 0x99); else s[1]->SetTarget((void*)0x99); break;
                    case 3: r[side][0] = (int)(uintptr_t)(side == 0 ? at<FnP0>(0x00016180)(s[0], nullptr) : (void*)s[1]->GetContext()); break;
                    case 4: r[side][0] = (int)(uintptr_t)(side == 0 ? at<FnP0>(0x000161B0)(s[0], nullptr) : (void*)s[1]->GetTarget()); break;
                    }
                });
            }
            for (int k = 0; k < 4; k++) eq("result", r[0][k], r[1][k]);
            eq("count", s[0]->m_count, s[1]->m_count);
            for (int k = 0; k < 110; k++) eq("contexts' words", ((uint32_t*)s[0])[k], ((uint32_t*)s[1])[k]);
            check_logs("contexts");
            std::free((uint32_t*)s[0] - 16);
            std::free((uint32_t*)s[1] - 16);
        }
    }
    // The queue: new items from each heap, from the free list, failing.
    for (int c = 0; c < 6; c++) {
        g_context = "queue " + std::to_string(c);
        AtFpActionQueue q[2];
        AtFpActionQueue::Item spare[2][2];
        for (int side = 0; side < 2; side++) {
            std::memset(&q[side], 0, sizeof q[side]);
            std::memset(spare[side], 0, sizeof spare[side]);
            q[side].m_heap = c & 1 ? &g_heap_b : nullptr;
            if (c >= 2) {
                q[side].m_free.m_head = &spare[side][0].m_link;
                spare[side][0].m_link.m_next = c >= 4 ? &spare[side][1].m_link : nullptr;
                if (c >= 4) spare[side][1].m_link.m_prev = &spare[side][0].m_link;
                q[side].m_free.m_tail = c >= 4 ? &spare[side][1].m_link : &spare[side][0].m_link;
            }
            run_side(side, [&] {
                for (int k = 0; k < (c >= 4 ? 1 : 3); k++) {
                    if (side == 0) at<Fn2>(0x000162C0)(&q[0], nullptr, (uintptr_t)(10 + k), (uintptr_t)(0x100 + k));
                    else q[1].Push(10 + k, (AtFpSpriteCharacterNode*)(uintptr_t)(0x100 + k));
                }
            });
        }
        int n = 0;
        for (AtFlashLink *x = q[0].m_items.m_head, *y = q[1].m_items.m_head; x || y; x = x ? x->m_next : nullptr, y = y ? y->m_next : nullptr, n++) {
            eq("queue item", x != nullptr, y != nullptr);
            if (!x || !y) break;
            eq("queue script", ((AtFpActionQueue::Item*)x)->m_script, ((AtFpActionQueue::Item*)y)->m_script);
            eq("queue node", (uintptr_t)((AtFpActionQueue::Item*)x)->m_node, (uintptr_t)((AtFpActionQueue::Item*)y)->m_node);
            eq("queue spare", x == &spare[0][0].m_link || x == &spare[0][1].m_link, y == &spare[1][0].m_link || y == &spare[1][1].m_link);
            eq("queue prev", x->m_prev != nullptr, y->m_prev != nullptr);
        }
        eq("queue free", q[0].m_free.m_head != nullptr, q[1].m_free.m_head != nullptr);
        if (q[0].m_free.m_head && q[1].m_free.m_head) eq("queue free head's prev", q[0].m_free.m_head->m_prev != nullptr, q[1].m_free.m_head->m_prev != nullptr);
        eq("queue free tail", q[0].m_free.m_tail != nullptr, q[1].m_free.m_tail != nullptr);
        check_logs("queue");
    }
    // The queue failing: both fault writing through null, after the same calls.
    {
        g_context = "queue fails";
        AtFpActionQueue q[2] = {};
        bool c[2];
        for (int side = 0; side < 2; side++) c[side] = run_side(side, [&] {
            g_fail_alloc = true;
            if (side == 0) at<Fn2>(0x000162C0)(&q[0], nullptr, 1, 2); else q[1].Push(1, nullptr);
        });
        eq("crash", c[0], c[1]);
        check_logs("queue fails");
    }
    // The two list calls: append to empty and not; remove the head, the tail, a middle, the only one.
    for (int c = 0; c < 6; c++) {
        g_context = "list " + std::to_string(c);
        AtFlashList l[2];
        AtFlashLink k[2][3];
        for (int side = 0; side < 2; side++) {
            std::memset(&l[side], 0, sizeof l[side]);
            std::memset(k[side], 0xAB, sizeof k[side]);
            run_side(side, [&] {
                const int n = c < 2 ? c + 1 : 3;
                for (int i = 0; i < n; i++) { if (side == 0) at<Fn1>(0x00012720)(&l[0], nullptr, (uintptr_t)&k[0][i]); else l[1].Append(&k[1][i]); }
                if (c >= 1) {
                    const int which = c == 1 ? 0 : c - 2 > 2 ? 2 : c - 2;
                    if (side == 0) at<Fn1>(0x00012750)(&l[0], nullptr, (uintptr_t)&k[0][which]); else l[1].Remove(&k[1][which]);
                    if (c == 5) { if (side == 0) at<Fn1>(0x00012750)(&l[0], nullptr, (uintptr_t)&k[0][1]); else l[1].Remove(&k[1][1]); if (side == 0) at<Fn1>(0x00012750)(&l[0], nullptr, (uintptr_t)&k[0][0]); else l[1].Remove(&k[1][0]); }
                }
            });
        }
        auto ix = [&](int side, AtFlashLink* p) { for (int i = 0; i < 3; i++) if (p == &k[side][i]) return i; return p ? 99 : -1; };
        eq("list head", ix(0, l[0].m_head), ix(1, l[1].m_head));
        eq("list tail", ix(0, l[0].m_tail), ix(1, l[1].m_tail));
        for (int i = 0; i < 3; i++) {
            eq("link next", ix(0, k[0][i].m_next), ix(1, k[1][i].m_next));
            eq("link prev", ix(0, k[0][i].m_prev), ix(1, k[1][i].m_prev));
        }
        check_logs("list");
    }
}

// Each node class constructed on both sides over the same fill: every field but the link, which the constructors leave.
static void run_constructors() {
    const uint32_t ctors[3] = {0x00014300, 0x00021FE0, 0x00030570};
    const unsigned sizes[3] = {0xB8, 0x64, 0x60};
    for (int k = 0; k < 3; k++) {
        for (unsigned fill : {0xCDu, 0x00u, 0xFFu}) {
            g_context = "constructor " + std::to_string(k) + " fill " + std::to_string(fill);
            unsigned char* p[2];
            for (int side = 0; side < 2; side++) {
                p[side] = (unsigned char*)std::calloc(1, 0x100);
                std::memset(p[side], (int)fill, 0x100);
                run_side(side, [&] {
                    if (side == 0) at<FnCtor>(ctors[k])(p[0], nullptr);
                    else if (k == 0) new (p[1]) AtFpSpriteCharacterNode;
                    else if (k == 1) new (p[1]) AtFpShapeCharacterNode;
                    else new (p[1]) AtFpTextCharacterNode;
                });
            }
            eq("class", class_of(p[0], 0), class_of(p[1], 1));
            for (unsigned o = 0xC; o < sizes[k]; o += 4) {
                if (k == 0 && o >= 0x7C + 0xC && o < 0x7C + 0xC + 0x2C) {
                    // The variables' undefined value: its vftable is each side's own.
                    if (o == 0x7C + 0xC) { eq("undefined value's vftable", *(uint32_t*)(p[0] + o), 0x00272964); continue; }
                }
                char what[64];
                std::snprintf(what, sizeof what, "constructed +%02X", o);
                eq(what, *(uint32_t*)(p[0] + o), *(uint32_t*)(p[1] + o));
            }
            check_logs("constructor");
            std::free(p[0]);
            std::free(p[1]);
        }
    }
}

// Two floats, normalized: each side's code over many vectors, the short ones and NaN among them.
static void run_vectors() {
    unsigned s = 12345;
    for (int k = 0; k < 20000; k++) {
        s = s * 1103515245u + 12345u;
        uint32_t a = s;
        s = s * 1103515245u + 12345u;
        uint32_t b = s;
        if (k < 40) { const float tiny[] = {0, 1e-6f, 3e-6f, -5e-6f, 5e-6f, -4e-6f, 1, -1, 1e30f, NAN}; a = bits(tiny[k % 10]); b = bits(tiny[(k / 10) % 10]); }
        if (k >= 40 && k % 3 == 1) { a = (a & 0x807FFFFF) | ((120u + (a >> 27)) << 23); b = (b & 0x807FFFFF) | ((120u + (b >> 29)) << 23); }
        AtVector2 from[2], to[2], self[2];
        float len[2];
        for (int side = 0; side < 2; side++) {
            std::memcpy(&from[side].x, &a, 4);
            std::memcpy(&from[side].y, &b, 4);
            self[side] = from[side];
            to[side].x = to[side].y = 7;
        }
        g_context = "vectors";
        at<Fn1>(0x00021F10)(&to[0], nullptr, (uintptr_t)&from[0]);
        to[1].SetNormalized(from[1]);
        len[0] = at<FnF0>(0x00021F60)(&self[0], nullptr);
        len[1] = self[1].NormalizeLength();
        eq("normalized x", bits(to[0].x), bits(to[1].x));
        eq("normalized y", bits(to[0].y), bits(to[1].y));
        eq("in place x", bits(self[0].x), bits(self[1].x));
        eq("in place y", bits(self[0].y), bits(self[1].y));
        eq("length", bits(len[0]), bits(len[1]));
    }
}

// Each side's own code for what the edges call directly.
static void side_goto(int side, AtFpSpriteCharacterNode* n, int frame) { if (side == 0) at<Fn1>(0x00015130)(n, nullptr, (uintptr_t)frame); else n->GotoFrame(frame); }
static void side_remove(int side, AtFpSpriteCharacterNode* parent, AtFpCharacterNode* child) { if (side == 0) at<Fn1>(0x00021560)(parent, nullptr, (uintptr_t)child); else parent->RemoveChild(child); }
static void side_remove_all(int side, AtFpSpriteCharacterNode* n) { if (side == 0) at<Fn0>(0x000214A0)(n, nullptr); else n->RemoveAllChildren(); }
static void side_name(int side, AtFpCharacterNode* n, const char* name) { if (side == 0) at<Fn1>(0x00020E10)(n, nullptr, (uintptr_t)name); else n->SetName(name); }
static void side_place(int side, AtFpSpriteCharacterNode* n, AtFpPlaceObject2Tag* t) { if (side == 0) at<Fn1>(0x00014F00)(n, nullptr, (uintptr_t)t); else n->ExecutePlace(t); }
static void log_resolved(int side, AtFpDisplayManager& d, const char* path, int fromContext) {
    void* r = side == 0 ? at<FnP2>(0x0002BFF0)(&d, nullptr, (uintptr_t)path, (uintptr_t)fromContext) : d.ResolvePath(path, fromContext);
    callout('@', (uint32_t)id_of(r));
}
static AtFpSpriteCharacterNode* child_at(AtFpSpriteCharacterNode* n, unsigned depth) {
    for (AtFlashLink* l = n->m_children.m_head; l; l = l->m_next) {
        AtFpCharacterNode* c = (AtFpCharacterNode*)((char*)l - 4);
        if (c->m_depth == depth) return (AtFpSpriteCharacterNode*)c;
    }
    return nullptr;
}

// The display manager called directly where a script cannot reach: no context, an empty stack, a null path, long paths, dead contexts and targets, a second level, nodes removed with their children.
static void run_manager_edges() {
    // The harness's strings and tags, made before any side records.
    const std::string longPath = std::string(0xFF, 'p') + ":v";
    const std::string longFrame = std::string(0xFF, 'p') + ":3";
    const std::string longName(1022, 'n');
    AtFpPlaceObject2Tag* unnamed[2] = {place(0, 61, 2, 2, 0), place(0, 62, 2, 2, 0)};
    unsigned char* second[2] = {(unsigned char*)std::calloc(1, sizeof(AtFlashLevel) + 0x40), (unsigned char*)std::calloc(1, sizeof(AtFlashLevel) + 0x40)};
    for (int c = 0; c < 29; c++) {
        g_context = "manager edge " + std::to_string(c);
        AtFlashSpriteTable* sprites = g_sprites;
        AtFlashScriptTable* scripts = g_scripts;
        g_sprites = &g_written->table;
        g_scripts = &g_written->scriptTable;
        with_worlds(g_scripts, nullptr, 3, [&](int side) {
            AtFpDisplayManager& d = g_app[side].m_display;
            AtFpSpriteCharacterNode* root = root_of(side);
            AtFpSpriteCharacterNode* kid = child_at(root, 5);
            AtFpActionScriptValue* v = make_value(7, 9, nullptr, side);
            switch (c) {
            case 0: d.m_contexts.m_count = 0; break;
            case 1: d.m_contexts.m_entries[0].m_context = nullptr; break;
            case 2: root->m_state = 2; break;
            case 3: d.m_contexts.m_entries[0].m_target = nullptr; break;
            default: break;
            }
            if (c < 4) {
                for (int k = 4; k <= 18; k++) {
                    int r = 0;
                    switch (k) {
                    case 4: r = side == 0 ? at<FnI2>(0x0002C9C0)(&d, nullptr, (uintptr_t)"a", (uintptr_t)v) : d.GetVariable("a", v); break;
                    case 5: r = side == 0 ? at<FnI2>(0x0002C9C0)(&d, nullptr, (uintptr_t)longPath.c_str(), (uintptr_t)v) : d.GetVariable(longPath.c_str(), v); break;
                    case 6: if (side == 0) at<FnI2>(0x0002CAA0)(&d, nullptr, (uintptr_t)"b", (uintptr_t)v); else d.SetVariable("b", v); break;
                    case 7: if (side == 0) at<FnI2>(0x0002CAA0)(&d, nullptr, (uintptr_t)longPath.c_str(), (uintptr_t)v); else d.SetVariable(longPath.c_str(), v); break;
                    case 8: if (side == 0) at<Fn1>(0x0002CB70)(&d, nullptr, (uintptr_t)"2"); else d.GotoFrameByExpression("2"); break;
                    case 9: if (side == 0) at<Fn1>(0x0002CB70)(&d, nullptr, (uintptr_t)longFrame.c_str()); else d.GotoFrameByExpression(longFrame.c_str()); break;
                    case 10: r = side == 0 ? at<FnI1>(0x0002C1C0)(&d, nullptr, (uintptr_t)"2") : d.ExecuteScripts("2"); break;
                    case 11: r = side == 0 ? at<FnI1>(0x0002C1C0)(&d, nullptr, (uintptr_t)longFrame.c_str()) : d.ExecuteScripts(longFrame.c_str()); break;
                    case 12: r = side == 0 ? at<FnI1>(0x0002C150)(&d, nullptr, 1) : d.ExecuteScripts(1); break;
                    case 13: r = side == 0 ? at<FnI0>(0x0002BF50)(&d, nullptr) : d.GetCurrentFrame(); break;
                    case 14: if (side == 0) at<Fn1>(0x0002B970)(&d, nullptr, 2); else d.GotoFrame(2); if (side == 0) at<Fn1>(0x0002BFC0)(&d, nullptr, 0); else d.SetPlaying(0); if (side == 0) at<Fn1>(0x0002BF80)(&d, nullptr, (uintptr_t)"Moved"); else d.GotoFrameByLabel("Moved"); break;
                    case 15: log_resolved(side, d, nullptr, 1); break;
                    case 16: if (side == 0) at<Fn1>(0x0002C3B0)(&d, nullptr, (uintptr_t)"/kid"); else d.SetTarget("/kid"); break;
                    case 17: r = (int)(uintptr_t)(side == 0 ? at<FnP1>(0x0002CC90)(&d, nullptr, (uintptr_t)"nothing") : d.ResolveTarget("nothing")); break;
                    case 18: log_resolved(side, d, "kid", 0); break;
                    }
                    callout('=', (uint32_t)r);
                    log_value(v);
                }
                d.m_contexts.m_count = 1;
                d.m_contexts.m_entries[0].m_context = d.m_contexts.m_entries[0].m_target = root;
                root->m_state = 1;
                return;
            }
            switch (c) {
            case 4: {
                // A second level shown, after the first: its root named "_level1".
                AtFlashLevel* L2 = (AtFlashLevel*)second[side];
                std::memset(L2, 0, sizeof(AtFlashLevel));
                AtFpSpriteCharacterNode* r2 = (AtFpSpriteCharacterNode*)(void*)L2->m_player;
                if (side == 0) at<FnCtor>(0x00014300)(r2, nullptr); else new (r2) AtFpSpriteCharacterNode;
                r2->m_kind = 2;
                r2->m_state = 1;
                side_name(side, r2, "_level1");
                level_of(side)->m_link.m_next = &L2->m_link;
                L2->m_link.m_prev = &level_of(side)->m_link;
                d.m_active.m_tail = &L2->m_link;
                if (side == 0) at<Fn0>(0x0002BF20)(&d, nullptr); else d.StopSounds();
                log_resolved(side, d, "_level0/kid", 0);
                log_resolved(side, d, "_level1", 0);
                log_resolved(side, d, "kid", 0);
                level_of(side)->m_link.m_next = nullptr;
                d.m_active.m_tail = &level_of(side)->m_link;
                break;
            }
            case 5:
                // The kid with children of each kind, removed; then all the root's.
                side_goto(side, kid, 1);
                side_goto(side, child_at(kid, 2), 1);
                side_remove(side, root, kid);
                side_remove_all(side, root);
                break;
            case 6:
                // The target dead, the context alive.
                d.m_contexts.m_entries[0].m_target = kid;
                side_goto(side, kid, 1);
                side_remove(side, root, kid);
                if (side == 0) at<Fn3>(0x0002C320)(&d, nullptr, (uintptr_t)"", 0, (uintptr_t)v); else d.GetProperty("", 0, v);
                log_value(v);
                if (side == 0) at<Fn3>(0x0002C360)(&d, nullptr, (uintptr_t)"", 0, (uintptr_t)v); else d.SetProperty("", 0, v);
                if (side == 0) at<Fn3>(0x0002C320)(&d, nullptr, (uintptr_t)"nothing", 0, (uintptr_t)v); else d.GetProperty("nothing", 0, v);
                log_value(v);
                callout('=', (uint32_t)(side == 0 ? at<FnI2>(0x0002C9C0)(&d, nullptr, (uintptr_t)"inner:x", (uintptr_t)v) : d.GetVariable("inner:x", v)));
                log_value(v);
                log_resolved(side, d, "inner", 0);
                {
                    AtFpRfcArgs* a = side == 0 ? (AtFpRfcArgs*)at<FnP1>(0x0002CC90)(&d, nullptr, (uintptr_t)"") : d.ResolveTarget("");
                    callout('=', (uint32_t)id_of((char*)a - offsetof(AtFpSpriteCharacterNode, m_variables)));
                }
                d.m_contexts.m_entries[0].m_target = root;
                break;
            case 7:
                // The context and target dead.
                d.m_contexts.m_entries[0].m_target = d.m_contexts.m_entries[0].m_context = kid;
                side_remove(side, root, kid);
                if (side == 0) at<Fn1>(0x0002C3B0)(&d, nullptr, (uintptr_t)"x"); else d.SetTarget("x");
                callout('=', (uint32_t)id_of(d.m_contexts.m_entries[0].m_target));
                d.m_contexts.m_entries[0].m_target = d.m_contexts.m_entries[0].m_context = root;
                break;
            case 8:
                // The kid the last sprite child, with a child "inner": a path that names the kid's child alone.
                side_remove(side, root, child_at(root, 7));
                side_remove(side, root, child_at(root, 50));
                side_goto(side, kid, 1);
                log_resolved(side, d, "inner", 0);
                log_resolved(side, d, "inner", 1);
                if (side == 0) at<Fn1>(0x0002C3B0)(&d, nullptr, (uintptr_t)"inner"); else d.SetTarget("inner");
                break;
            case 9:
                // A second "kid", later, with a child: the first decides.
                side_goto(side, root, 4);
                side_goto(side, child_at(root, 12), 1);
                log_resolved(side, d, "kid/inner", 0);
                log_resolved(side, d, "_level0/kid/inner", 0);
                break;
            case 10:
                // A sprite named "", with a child.
                side_goto(side, root, 4);
                side_goto(side, child_at(root, 20), 1);
                log_resolved(side, d, "_level0//inner", 0);
                log_resolved(side, d, "/", 0);
                break;
            case 11:
                // A name that fills the path's room.
                side_name(side, kid, longName.c_str());
                if (side == 0) at<Fn3>(0x0002C320)(&d, nullptr, (uintptr_t)longName.c_str(), 11, (uintptr_t)v); else d.GetProperty(longName.c_str(), 11, v);
                log_value(v);
                break;
            case 12:
                // Instances numbered from a negative count and from one of four digits.
                level_of(side)->m_read = -12;
                side_place(side, root, unnamed[0]);
                level_of(side)->m_read = 1234;
                side_place(side, root, unnamed[1]);
                break;
            case 13: {
                // A sprite's frames read past the end.
                AtFpSprite* s = g_written->sprites[0];
                const char* name = side == 0 ? (const char*)at<FnP1>(0x00016AA0)(s, nullptr, 7) : s->FrameName(7);
                log_text('|', name);
                void* f = side == 0 ? at<FnP1>(0x00016A60)(s, nullptr, 7) : (void*)s->Frame(7);
                callout('|', f != nullptr);
                break;
            }
            case 14: {
                // A sprite with no frames: both report and read through null.
                AtFpSprite empty;
                empty.m_refs = nullptr;
                empty.m_refCount = 0;
                if (side == 0) at<FnP1>(0x00016A60)(&empty, nullptr, 0); else empty.Frame(0);
                break;
            }
            default:
                // 15 to 28: the manager's calls with the target on the kid, each by itself.
                d.m_contexts.m_entries[0].m_target = kid;
                switch (c) {
                case 15: callout('=', (uint32_t)id_of((char*)(side == 0 ? at<FnP1>(0x0002CC90)(&d, nullptr, (uintptr_t)"") : (void*)d.ResolveTarget("")) - offsetof(AtFpSpriteCharacterNode, m_variables))); break;
                case 16: log_resolved(side, d, "", 0); log_resolved(side, d, "", 1); break;
                case 17: log_resolved(side, d, "/kid", 0); log_resolved(side, d, "/", 1); break;
                case 18: callout('=', (uint32_t)(side == 0 ? at<FnI2>(0x0002C9C0)(&d, nullptr, (uintptr_t)":x", (uintptr_t)v) : d.GetVariable(":x", v))); log_value(v); break;
                case 19: if (side == 0) at<Fn1>(0x0002C3B0)(&d, nullptr, (uintptr_t)""); else d.SetTarget(""); break;
                case 20: if (side == 0) at<Fn1>(0x0002CB70)(&d, nullptr, (uintptr_t)"OUT"); else d.GotoFrameByExpression("OUT"); break;
                case 21: callout('=', (uint32_t)(side == 0 ? at<FnI1>(0x0002C150)(&d, nullptr, 1) : d.ExecuteScripts(1))); break;
                case 22: callout('=', (uint32_t)(side == 0 ? at<FnI1>(0x0002C1C0)(&d, nullptr, (uintptr_t)"OUT") : d.ExecuteScripts("OUT"))); break;
                case 23: if (side == 0) at<Fn3>(0x0002C360)(&d, nullptr, (uintptr_t)"", 1, (uintptr_t)v); else d.SetProperty("", 1, v); break;
                case 24: if (side == 0) at<Fn1>(0x0002C3B0)(&d, nullptr, (uintptr_t)"nothing"); else d.SetTarget("nothing"); break;
                case 25: callout('=', (uint32_t)(side == 0 ? at<FnI0>(0x0002BF50)(&d, nullptr) : d.GetCurrentFrame())); break;
                case 26: if (side == 0) at<Fn1>(0x0002BF80)(&d, nullptr, (uintptr_t)"out"); else d.GotoFrameByLabel("out"); break;
                case 27: if (side == 0) at<FnI2>(0x0002CAA0)(&d, nullptr, (uintptr_t)":z", (uintptr_t)v); else d.SetVariable(":z", v); break;
                case 28: if (side == 0) at<Fn1>(0x0002B970)(&d, nullptr, 1); else d.GotoFrame(1); break;
                }
                d.m_contexts.m_entries[0].m_target = root;
                break;
            }
        });
        g_sprites = sprites;
        g_scripts = scripts;
    }
}

// ---- real archives --------------------------------------------------------------------------------------------------

static std::map<std::string, std::string> g_files;
static std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

class JamFs : public MemFs {
public:
    MemFile pool[2][8];
    int next[2] = {};
    const std::string* find(const char* path) {
        const char* current = m_path.Current();
        auto it = g_files.find(upper(std::string(current ? current : "") + path));
        return it == g_files.end() ? nullptr : &it->second;
    }
    int Open(const char* path, int, AtIoFile** out) override {
        const std::string* data = find(path);
        if (data == nullptr) { *out = nullptr; return 2; }
        MemFile& f = pool[g_side][next[g_side]++ & 7];
        f.side = g_side;
        f.data = (const unsigned char*)data->data();
        f.size = data->size();
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = 2;
        f.mode = kNormal;
        *out = &f;
        return 0;
    }
    int IsFileMissing(const char* path) override { return find(path) ? 0 : 1; }
};

static JamFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static unsigned g_archives, g_afa, g_blocks, g_instructions, g_nodes_built;

static bool load_archive(const std::string& list, const std::string& bin, std::string* afd) {
    FILE* f = nullptr;
    if (fopen_s(&f, bin.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    std::string data((size_t)std::ftell(f), '\0');
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    if (fopen_s(&f, list.c_str(), "r") || !f) return false;
    char line[1024];
    g_files.clear();
    while (std::fgets(line, sizeof line, f)) {
        char kind[8], path[512];
        unsigned offset, size;
        if (std::sscanf(line, "%7s\t%511[^\t]\t%u\t%u", kind, path, &offset, &size) != 4 || std::strcmp(kind, "F") != 0) continue;
        if ((size_t)offset + 32 + size > data.size()) continue;
        const std::string p = upper(path);
        g_files[p] = data.substr(offset + 32, size);
        if (p.size() > 4 && p.compare(p.size() - 4, 4, ".AFD") == 0 && p.find('\\') == std::string::npos) *afd = p.substr(0, p.size() - 4);
    }
    std::fclose(f);
    return true;
}

static std::string word_name(const std::string& text, const char* word) {
    const size_t q = text.find(word);
    if (q == std::string::npos) return "";
    const size_t a = text.find('"', q), b = text.find('"', a + 1);
    return text.substr(a + 1, b - a - 1);
}

static void run_archive(const std::string& afd) {
    const std::string sname = word_name(g_files[afd + ".AFD"], "Script");
    const std::string pname = word_name(g_files[afd + ".AFD"], "Sprite");
    if (!g_files.count(upper("ASCRIPT\\" + sname + ".AFA")) || !g_files.count(upper("SPRITES\\" + pname + ".AFP"))) return;
    // Loaded by the recovered loaders, outside the comparison, and kept.
    g_fs->m_path.m_top = 0;
    AtFlashScriptTable* t = new AtFlashScriptTable;
    t->Load(sname.c_str(), 2, &g_app[1]);
    g_fs->m_path.m_top = 0;
    AtFlashSpriteTable* sp = new AtFlashSpriteTable;
    sp->Load(pname.c_str(), 2);
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
    g_afa++;
    g_scripts = t;
    g_sprites = sp;
    g_strings = &t->m_strings;
    g_real = true;
    for (unsigned k = 0; k < t->m_count; k++) {
        AtFpScript* s = t->m_scripts[k];
        if (!s) continue;
        g_context = afd + " block " + std::to_string(k);
        run_steps(s, 17 + k);
        g_blocks++;
        g_instructions += s->m_read;
    }
    g_real = false;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_STACK_OVERFLOW) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

template <class T> static uint32_t* vftable_of() {
    static unsigned char storage[sizeof(T) + 16];
    T* t = new (storage) T;
    return *(uint32_t**)t;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else if (!std::strcmp(argv[i], "--trace")) g_trace = true;
        else if (!std::strcmp(argv[i], "--quick")) g_quick = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_fpdisplay <default.xbe> <cases dir> [--entries FILE] [--mutate] [--quick]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)push_counting);
    redirect((uint32_t)(uintptr_t)&AtPushHeap, (const void*)push_counting);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    // The recovered nodes' slots, from their vftables.
    uint32_t* vs = vftable_of<AtFpSpriteCharacterNode>();
    uint32_t* vh = vftable_of<AtFpShapeCharacterNode>();
    uint32_t* vt = vftable_of<AtFpTextCharacterNode>();
    g_vft[0] = (uint32_t)(uintptr_t)vs;
    g_vft[1] = (uint32_t)(uintptr_t)vh;
    g_vft[2] = (uint32_t)(uintptr_t)vt;
    struct Seam { uint32_t original, recovered; const void* to; };
    const Seam seams[] = {
        {0x000143B0, vs[1], (const void*)d_init_sprite},
        {0x00022060, vh[1], (const void*)d_init_shape},
        {0x00030590, vt[1], (const void*)d_init_text},
        {0x00014DB0, vs[6], (const void*)d_acquire_sprite},
        {0x000221D0, vh[6], (const void*)d_acquire_other},
        {0x00030F60, vt[6], (const void*)d_acquire_other},
        {0x000153C0, vs[4], (const void*)d_update},
        {0x001CC9F0, vh[4], (const void*)d_update},
        {0x001CC9F0, vt[4], (const void*)d_update},
        {0x00014C40, vs[9], (const void*)d_replace},
        {0x00021FB0, vh[9], (const void*)d_replace},
        {0x00021FB0, vt[9], (const void*)d_replace},
        {0x00020EA0, addr_of(&AtFpCharacterNode::SetMatrix), (const void*)d_set_matrix},
        {0x00012AA0, addr_of(&AtFlashSoundTable::StopAll), (const void*)d_stop_all},
        {0x00012790, addr_of(&AtFlashSoundTable::Play), (const void*)d_play},
        {0x0002C570, addr_of(&AtFpDisplayManager::UnloadLevel), (const void*)d_unload},
        {0x0002C5D0, addr_of(&AtFpDisplayManager::FindLoadedLevel), (const void*)d_find_loaded},
        {0x0002CCB0, addr_of(&AtFpDisplayManager::NewLevel), (const void*)d_new_level},
        {0x0002C430, addr_of(&AtFpDisplayManager::AttachLevel), (const void*)d_attach},
    };
    for (const Seam& s : seams) {
        redirect(s.original, s.to);
        redirect(s.recovered, s.to);
    }
    redirect(0x00020640, (const void*)d_time);
    // The matrix table, each side's.
    *(void**)(uintptr_t)0x002C61EC = (void*)m_get;
    *(void**)(uintptr_t)0x002C61E8 = (void*)m_set;
    *(void**)(uintptr_t)0x002C61F8 = (void*)m_get_position;
    *(void**)(uintptr_t)0x002C60CC = (void*)m_identity;
    *(void**)(uintptr_t)0x002C60D4 = (void*)m_translation;
    g_atMatrix.Get = m_get;
    g_atMatrix.Set = m_set;
    g_atMatrix.GetPosition = m_get_position;
    g_atMatrix.Identity = m_identity;
    g_atMatrix.SetTranslation = m_translation;
    // The game's remote function, as its start would set it: one each.
    *(uint32_t*)(uintptr_t)0x002B1378 = (uint32_t)(uintptr_t)&d_rfc_preload;
    *(uint32_t*)(uintptr_t)0x002B137C = 0;
    {
        union { void (AtFpRfcTable::*m)(AtFpRfcArgs*); void* p; } u = {};
        u.p = (void*)&d_rfc_preload;
        AtFpRfcTable::s_entries[0].m_method = u.m;
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(nullptr, 0, watchdog, nullptr, 0, nullptr);

    g_fs = new JamFs;
    g_fs->m_path.m_top = 0;
    g_fs->m_path.m_stack[0][0] = 0;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    *(AtIoContext**)(uintptr_t)0x002C62F0 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    AtIoContext::s_instance = &g_ctx;
    AtAudioBankFile::s_context = &g_ctx;
    *(AtIoContext**)(uintptr_t)0x002C6328 = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C626Cu, 0x002C6324u, 0x002C63D0u, 0x002C62F8u, 0x002C6270u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    g_ctx.m_spaces.Init(64);
    AtTextTable::s_heap = &g_heap_token;
    AtFlashResourceTable::s_defaultHeap = &g_heap_token;
    AtFlashScriptTable::s_defaultHeap = &g_heap_token;
    AtFpScript::s_heap = &g_heap_token;
    AtFpSprite::s_heap = &g_heap_token;
    AtFlashFrame::s_heap = &g_heap_token;
    AtFpPlaceObject2Tag::s_heap = &g_heap_token;
    // The statics of the code under test, each side's.
    *(void**)(uintptr_t)0x002C6390 = &g_heap_token;
    AtFpActionScriptStack::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C6318 = &g_heap_token;
    AtFpActionScriptValue::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C62EC = &g_heap_token;
    AtFpActionScriptVM::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C6320 = &g_heap_token;
    AtFpRfcArgs::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C6268 = &g_heap_token;
    AtFpActionQueue::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C631C = &g_heap_token;
    AtFpCharacterNode::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C6394 = &g_heap_b;
    AtFpRfcTable::s_heap = &g_heap_b;
    for (int side = 0; side < 2; side++) std::memset((void*)&g_app[side], 0, sizeof g_app[side]);
    g_heap_pad = 0x20;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.

    run_constructors();
    run_vectors();
    run_variables();
    run_rfc();
    run_contexts();
    make_written();
    run_written();
    run_manager_edges();

    std::string dir = pos[1];
    const std::string jams = dir + "\\..\\oracle-jam\\";
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((jams + "*.txt").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) { std::printf("cannot list %s\n", jams.c_str()); return 2; }
    std::vector<std::string> lists;
    do lists.push_back(found.cFileName); while (FindNextFileA(h, &found));
    FindClose(h);
    std::sort(lists.begin(), lists.end());
    for (const std::string& l : lists) {
        std::string afd;
        const std::string stem = l.substr(0, l.size() - 4);
        if (!load_archive(jams + l, jams + stem + ".bin", &afd) || afd.empty()) continue;
        g_archives++;
        run_archive(afd);
    }
    report_entries();
    std::printf("real blocks run whole: %u; nodes in the trees after them: %u; variables held after them: %u, in %u runs; runs that reported something: %u; actions left queued: %u; nodes dead: %u; runs ending on another target: %u\n",
                g_stat_runs, g_stat_nodes, g_stat_vars, g_stat_runs_vars, g_stat_reports, g_stat_actions, g_stat_dead, g_stat_targets);
    std::printf("%s: %u archives, %u .AFA and .AFP; %u script blocks, %u instructions; %u runs, %u of them cut at an instruction, %u crashed alike, %u stopped alike as hung; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_archives, g_afa, g_blocks, g_instructions, g_runs, g_steps, g_crash_runs, g_hangs, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
