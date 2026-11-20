// Differential oracle for the Flash action script machine: its dispatch loop, its handlers, and the values' and stack's methods only they use (Windows x86, local only). See docs/verification.md.
//
//   oracle_asvm <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate] [--trace] [--quick]
//
// Real scripts: every user interface archive in the JAM cases beside the tokenizer cases (..\oracle-jam). Each one's .AFA and .AIS are loaded once, by the recovered loader (suite aft), and every script block
// is run on two machines, the original's code and the recovered source, from the same starting state: a fresh machine with an empty stack of 30, the same application, the same strings, the same random
// cursor. The block is run whole, and once more for every instruction with that instruction made an End, so that the state when control first reaches each instruction is compared too. After each run the
// two machines are compared field by field, all 30 stack values with their text; every call out, with its arguments and the stack as it stands at that call, is logged on each side and the logs compared.
//
// What stands in, the same on both sides, each call logged: the display manager (frames, labels, targets, variables, properties, movies, scripts called), its stack of contexts, the remote function
// calls and their arguments (GetURL2's event calls into game code, Trace), and the clock. Variables live in a table on each side: what a script sets it gets back; a name never set gives a value chosen by
// the name (undefined, an integer, a real, short or long text). The heap and fatal helpers are logged as in the other suites.
//
// Then written scripts for every action code and error path, and the values' and stack's methods called directly, a real's text over random bit patterns among them.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtFlashScript.h"
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



static int g_heap_token, g_heap_b, g_heap_c;

// No room from the Nth heap push on, on each side.
static int g_fail_at_push, g_pushes;
static void __cdecl push_counting(void* h) {
    if (g_recording) g_log[g_side].add('P', (uint32_t)(uintptr_t)h);
    if (g_fail_at_push && ++g_pushes == g_fail_at_push) g_fail_alloc = true;
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uintptr_t);
typedef void(__fastcall* Fn2)(void*, void*, uintptr_t, uintptr_t);
typedef int(__fastcall* FnI0)(void*, void*);
typedef int(__fastcall* FnI1)(void*, void*, uintptr_t);
typedef int(__fastcall* FnI2)(void*, void*, uintptr_t, uintptr_t);
typedef int(__fastcall* FnIF)(void*, void*, float);
typedef float(__fastcall* FnF0)(void*, void*);
typedef char*(__fastcall* FnS0)(void*, void*);
typedef void*(__fastcall* FnP1)(void*, void*, uintptr_t);

// ---- the call log's text: a hash and a length, so that long runs do not overflow the message buffer -----------------------------------

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

// ---- the application each side's machine is given ----------------------------------------------------------------------

static AtFlashApplication g_app[2];
static int g_context_token[2], g_resolved[2][0x40];
static AtFpActionScriptVM* g_vm[2];  // The machine running on each side, for logging its stack.
static AtTextTable* g_strings;
static void* g_last_args[2];
static int g_frame[2], g_time[2], g_callouts[2];
static int g_callout_limit = 4000;

struct Saved {
    int type = 3, integer = 0;
    uint32_t real = 0;
    std::string text;
};
static std::map<std::string, Saved> g_vars[2];

static bool has_text(const AtFpActionScriptValue* v) {
    return v->m_text != nullptr && (v->m_type == 0 || v->m_haveTextInt || v->m_haveTextReal);
}
static void log_value(const AtFpActionScriptValue* v) {
    if (!g_recording) return;
    Log& l = g_log[g_side];
    l.add('t', (uint32_t)v->m_type);
    l.add('i', (uint32_t)v->m_int);
    l.add('r', (uint32_t)v->m_unknown14);
    l.add('f', (uint32_t)((v->m_haveTextInt != 0) | (v->m_haveTextReal != 0) << 1 | (v->m_haveIntReal != 0) << 2));
    l.add('k', v->m_capacity);
    l.add('u', (v->m_text == (const char*)v + 0x2C) | (v->m_text == nullptr) << 1);
    log_text('x', has_text(v) ? v->m_text : "-");
}
static void log_stack() {
    if (!g_recording || !g_vm[g_side]) return;
    const AtFpActionScriptStack& s = g_vm[g_side]->m_stack;
    g_log[g_side].add('S', (uint32_t)s.m_top);
    for (int i = 0; i <= s.m_top && i < 30; i++) log_value(&s.m_values[i]);
}
// Every call out: counted, and past the limit a fault, so that a script that loops forever stops on both sides at the same call.
static void callout(int code, uint32_t a = 0) {
    if (!g_recording) return;
    g_log[g_side].add(code, a);
    log_stack();
    if (++g_callouts[g_side] > g_callout_limit) *(volatile int*)nullptr = 0;
}

// A value given out: through the value's own SetString for text, so that each side's code takes it; numbers by their fields.
static void give(AtFpActionScriptValue* out, const Saved& s) {
    if (s.type == 0) {
        const unsigned cap = (unsigned)s.text.size() + 1;
        char* p = (char*)std::calloc(1, cap);
        std::memcpy(p, s.text.c_str(), cap);
        out->SetString(p, cap);
    } else if (s.type == 7 || s.type == 1) {
        out->m_type = s.type;
        out->m_int = s.integer;
        out->m_unknown14 = (int)s.real;
        out->m_haveTextInt = out->m_haveTextReal = out->m_haveIntReal = 0;
    }
}
static Saved by_name(const char* name) {
    Saved s;
    const uint32_t h = hash_text(name);
    switch (h % 7) {
    case 0: break;
    case 1: s.type = 7; s.integer = (int)(h >> 8) % 50; break;
    case 2: s.type = 0; s.text = "12"; break;
    case 3: s.type = 1; { float f = 2.5f; std::memcpy(&s.real, &f, 4); } s.integer = 2; break;
    case 4: s.type = 0; s.text = "abc"; break;
    case 5: s.type = 0; s.text = std::string(300, 'x'); break;
    case 6: s.type = 7; s.integer = 1; break;
    }
    return s;
}
static Saved saved_of(const AtFpActionScriptValue* v) {
    Saved s;
    s.type = v->m_type;
    s.integer = v->m_int;
    s.real = (uint32_t)v->m_unknown14;
    if (s.type == 0 && v->m_text) s.text = v->m_text;
    if (s.type != 0 && s.type != 1 && s.type != 7) s.type = 3;
    return s;
}

static int __fastcall d_current_frame(AtFpDisplayManager*, void*) { callout('a'); return g_frame[g_side]; }
static void __fastcall d_goto_frame(AtFpDisplayManager*, void*, int frame) { callout('b', (uint32_t)frame); g_frame[g_side] = frame; }
static void __fastcall d_set_playing(AtFpDisplayManager*, void*, int playing) { callout('c', (uint32_t)playing); }
static void __fastcall d_stop_sounds(AtFpDisplayManager*, void*) { callout('d'); }
static void __fastcall d_goto_label(AtFpDisplayManager*, void*, const char* label) { log_text('e', label); callout('e'); }
static void __fastcall d_goto_expression(AtFpDisplayManager*, void*, const char* e) { log_text('g', e); callout('g'); }
static void __fastcall d_set_target(AtFpDisplayManager*, void*, const char* path) { log_text('h', path); callout('h'); }
static void __fastcall d_load_movie(AtFpDisplayManager*, void*, const char* name, const char* target) { log_text('j', name); log_text('J', target); callout('j'); }
static AtFpRfcArgs* __fastcall d_resolve(AtFpDisplayManager*, void*, const char* path) { log_text('k', path); callout('k'); return (AtFpRfcArgs*)g_resolved[g_side]; }
static int __fastcall d_get_variable(AtFpDisplayManager* self, void*, const char* name, AtFpActionScriptValue* out) {
    log_text('m', name);
    callout('m', self == &g_app[g_side].m_display);
    if (!name) return 0;
    const bool was = g_recording;
    g_recording = false;  // The harness's own containers.
    // Held in a static, so that the harness's strings are never freed while logging is on.
    static Saved given[2];
    auto it = g_vars[g_side].find(name);
    given[g_side] = it != g_vars[g_side].end() ? it->second : by_name(name);
    g_recording = was;
    give(out, given[g_side]);
    log_value(out);
    return 1;
}
static int __fastcall d_set_variable(AtFpDisplayManager*, void*, const char* name, AtFpActionScriptValue* value) {
    log_text('n', name);
    log_value(value);
    callout('n');
    if (!name) return 0;
    const bool was = g_recording;
    g_recording = false;
    g_vars[g_side][name] = saved_of(value);
    g_recording = was;
    return 1;
}
static void __fastcall d_get_property(AtFpDisplayManager*, void*, const char* target, int index, AtFpActionScriptValue* out) {
    log_text('o', target);
    callout('o', (uint32_t)index);
    static Saved given[2];
    const bool was = g_recording;
    g_recording = false;
    Saved& s = given[g_side];
    s = Saved();
    if (index & 1) { s.type = 0; s.text = index > 10 ? std::string(280, 'p') : "7.5"; }
    else { s.type = 7; s.integer = index * 3 + 1; }
    g_recording = was;
    give(out, s);
}
static void __fastcall d_set_property(AtFpDisplayManager*, void*, const char* target, int index, AtFpActionScriptValue* value) {
    log_text('q', target);
    log_value(value);
    callout('q', (uint32_t)index);
}
static int __fastcall d_exec_frame(AtFpDisplayManager*, void*, int frame) { callout('w', (uint32_t)frame); return frame == 9 || frame == 12; }
static int __fastcall d_exec_label(AtFpDisplayManager*, void*, const char* label) { log_text('y', label); callout('y'); return label && std::strncmp(label, "Stop", 4) == 0; }
static void __fastcall d_context_target(AtFpDisplayMgrContextStack* self, void*, void* target) {
    callout('z', (target == &g_context_token[g_side]) | (self == &g_app[g_side].m_display.m_contexts) << 1);
}
static void __fastcall d_rfc_call(AtFpRfcTable* self, void*, const char* name, AtFpRfcArgs* args) {
    log_text('R', name);
    callout('R', (args == (AtFpRfcArgs*)g_resolved[g_side]) | (args == g_last_args[g_side]) << 1 | (self == &g_app[g_side].m_rfc) << 2);
}
static void __fastcall d_args_construct(AtFpRfcArgs* self, void*) { g_last_args[g_side] = self; callout('A'); }
static void __fastcall d_args_destruct(AtFpRfcArgs* self, void*) { callout('B', self == g_last_args[g_side]); }
static void __fastcall d_args_init(AtFpRfcArgs* self, void*) { callout('C', (uint32_t)(self->m_heap == &g_heap_token) | (self->m_heap == &g_heap_b) << 1 | (self == g_last_args[g_side]) << 2); }
static void __fastcall d_args_add(AtFpRfcArgs* self, void*, const char* name, AtFpActionScriptValue* value) {
    log_text('E', name);
    log_value(value);
    callout('E', self == g_last_args[g_side]);
}
static void __fastcall d_args_clear(AtFpRfcArgs* self, void*) { callout('G', self == g_last_args[g_side]); }
static unsigned __cdecl d_time() { callout('T'); return 1000u + 37u * (unsigned)++g_time[g_side]; }
unsigned int AtPlatformMilliseconds() { return d_time(); }

// ---- files: an archive's, by path ---------------------------------------------------------------------------------------

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
        if (data == nullptr) {
            *out = nullptr;
            return 2;
        }
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

// ---- comparing --------------------------------------------------------------------------------------------------------

static void same_text(const char* what, const char* o, const char* r, unsigned cap) {
    g_checks++;
    const unsigned n = std::min<unsigned>(cap, 0x1000);
    for (unsigned i = 0; i < n; i++) {
        if (o[i] != r[i]) {
            eq(what, (unsigned char)o[i], (unsigned char)r[i]);
            return;
        }
        if (o[i] == 0) return;
    }
}

static void same_value(const AtFpActionScriptValue* o, const AtFpActionScriptValue* r, bool local = false) {
    if (!local) eq("value's vftable", *(const uint32_t*)o, 0x00272964);
    else eq("local value's vftable", *(const uint32_t*)o, 0x00274878);
    eq("value's type", o->m_type, r->m_type);
    eq("value's int", o->m_int, r->m_int);
    eq("value's real", o->m_unknown14, r->m_unknown14);
    eq("value's text and int", o->m_haveTextInt, r->m_haveTextInt);
    eq("value's text and real", o->m_haveTextReal, r->m_haveTextReal);
    eq("value's int and real", o->m_haveIntReal, r->m_haveIntReal);
    eq("value's capacity", o->m_capacity, r->m_capacity);
    eq("value's text held", o->m_text != nullptr, r->m_text != nullptr);
    if (local) eq("value's text inline", o->m_text == (const char*)o + 0x2C, r->m_text == (const char*)r + 0x2C);
    if (o->m_text && r->m_text) same_text("value's text", o->m_text, r->m_text, local && o->m_text == (const char*)o + 0x2C ? 0x100 : std::max<unsigned>(o->m_capacity, 1));
    eq("value's +24", o->m_unknown24 != nullptr, r->m_unknown24 != nullptr);
    eq("value's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
}

static void same_stack(const AtFpActionScriptStack* o, const AtFpActionScriptStack* r) {
    eq("stack allocated", o->m_allocated, r->m_allocated);
    eq("stack's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("stack's top", o->m_top, r->m_top);
    eq("stack's capacity", o->m_capacity, r->m_capacity);
    eq("stack's values", o->m_values != nullptr, r->m_values != nullptr);
    if (o->m_values && r->m_values && o->m_capacity == r->m_capacity) {
        for (unsigned i = 0; i < o->m_capacity && i < 64; i++) same_value(&o->m_values[i], &r->m_values[i]);
    }
}

static void same_vm(const AtFpActionScriptVM* o, const AtFpActionScriptVM* r) {
    eq("vm's strings", (uintptr_t)o->m_strings, (uintptr_t)r->m_strings);
    eq("vm's application", o->m_application == &g_app[0], r->m_application == &g_app[1]);
    eq("vm's heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    same_stack(&o->m_stack, &r->m_stack);
}

static void same_world() {
    eq("random cursor", *(uint32_t*)(uintptr_t)0x002C60C0, g_atRandomCursor);
    eq("frame", g_frame[0], g_frame[1]);
    eq("clock", g_time[0], g_time[1]);
    eq("variables", (long long)g_vars[0].size(), (long long)g_vars[1].size());
    if (g_vars[0].size() == g_vars[1].size()) {
        auto a = g_vars[0].begin();
        auto b = g_vars[1].begin();
        for (; a != g_vars[0].end(); ++a, ++b) {
            eqs("variable's name", a->first.c_str(), b->first.c_str());
            eq("variable's type", a->second.type, b->second.type);
            eq("variable's int", a->second.integer, b->second.integer);
            eq("variable's real", a->second.real, b->second.real);
            eqs("variable's text", a->second.text.c_str(), b->second.text.c_str());
        }
    }
    eq("stack's static heap", (uintptr_t)*(void**)(uintptr_t)0x002C6390, (uintptr_t)AtFpActionScriptStack::s_heap);
}

static void check_logs(const char* what) {
    eq("log room", g_log[0].n < (int)(sizeof g_log[0].v / 4) - 4, 1);
    eq("log room", g_log[1].n < (int)(sizeof g_log[1].v / 4) - 4, 1);
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
    compare_logs(what);
}

// ---- making and running ----------------------------------------------------------------------------------------------

template <class T> static T* make(uint32_t ctor, unsigned char fill, bool original) {
    unsigned char* p = (unsigned char*)std::malloc(sizeof(T) + 0x40);
    std::memset(p, fill, sizeof(T) + 0x40);
    if (original) {
        eq("constructor returns this", (uintptr_t)at<FnCtor>(ctor)(p, nullptr), (uintptr_t)p);
        return (T*)p;
    }
    return new (p) T;
}

template <class F> static bool run_side(int side, F f) {
    g_callouts[side] = 0;
    g_pushes = 0;
    g_fail_alloc = false;
    if (g_trace) std::printf("run %d [%s]\n", side, g_context.c_str());
    const bool crashed = guarded([&] { On on(side); f(); });
    if (g_trace && crashed) std::printf("crashed %d\n", side);
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    return crashed;
}

// Both sides' statics and the heaps they fall back on.
static void set_statics(void* fallback, void* stack, void* value, void* vm, void* general) {
    *(void**)(uintptr_t)0x002C6390 = stack;
    AtFpActionScriptStack::s_heap = stack;
    *(void**)(uintptr_t)0x002C6318 = value;
    AtFpActionScriptValue::s_heap = value;
    *(void**)(uintptr_t)0x002C62EC = vm;
    AtFpActionScriptVM::s_heap = vm;
    *(void**)(uintptr_t)0x002C60B0 = general;
    AtIoTokenFile::s_heap = general;
    (void)fallback;
}

static void reset_world(unsigned cursor) {
    *(uint32_t*)(uintptr_t)0x002C60C0 = cursor;
    g_atRandomCursor = cursor;
    for (int side = 0; side < 2; side++) {
        g_frame[side] = 5;
        g_time[side] = 0;
        g_vars[side].clear();
        g_last_args[side] = nullptr;
    }
}

static unsigned g_runs, g_steps;

struct Machines {
    AtFpActionScriptVM* o = nullptr;
    AtFpActionScriptVM* r = nullptr;
    bool oc = false, rc = false;
};

// A machine on each side, with its stack of 30: made, given its heap, initialised. vmHeap 0 leaves the machine's own heap null.
static Machines make_machines(void* vmHeap) {
    Machines m;
    g_fill = 0xCD;
    m.o = make<AtFpActionScriptVM>(0x0001CC80, 0xCD, true);
    m.r = make<AtFpActionScriptVM>(0x0001CC80, 0xCD, false);
    m.o->m_heap = m.r->m_heap = vmHeap;
    run_side(0, [&] { at<Fn2>(0x0001CCC0)(m.o, nullptr, (uintptr_t)&g_app[0], (uintptr_t)g_strings); });
    run_side(1, [&] { m.r->Init(&g_app[1], g_strings); });
    same_vm(m.o, m.r);
    check_logs("machines made");
    return m;
}

static void free_machines(Machines& m) {
    if (!m.oc && !m.rc) {
        run_side(0, [&] { at<Fn0>(0x0001EF00)(m.o, nullptr); });
        run_side(1, [&] { m.r->~AtFpActionScriptVM(); });
        check_logs("machines destroyed");
    }
    g_vm[0] = g_vm[1] = nullptr;
    std::free(m.o);
    std::free(m.r);
}

// Runs one script on both machines, then compares them.
static void run_both(Machines& m, AtFpScript* script) {
    g_vm[0] = m.o;
    g_vm[1] = m.r;
    m.oc = run_side(0, [&] { at<Fn1>(0x0001F020)(m.o, nullptr, (uintptr_t)script); });
    m.rc = run_side(1, [&] { m.r->Run(script); });
    eq("crash", m.oc, m.rc);
    eq("call outs", g_callouts[0], g_callouts[1]);
    same_vm(m.o, m.r);
    same_world();
    check_logs("run");
    g_runs++;
}

// A whole run from a fresh start: machines made, the script run, the machines destroyed.
static void run_script(AtFpScript* script, unsigned cursor, void* vmHeap = &g_heap_token) {
    reset_world(cursor);
    Machines m = make_machines(vmHeap);
    run_both(m, script);
    free_machines(m);
}

// The block whole, and for every instruction, the block with that one an End.
static const unsigned char kEndInstruction[4] = {0, 0, 0, 0};
static void run_steps(AtFpScript* script, unsigned cursor) {
    const std::string base = g_context;
    run_script(script, cursor);
    if (g_quick) return;
    std::vector<unsigned char*> pointers(script->m_instructions, script->m_instructions + script->m_read);
    unsigned char** saved = script->m_instructions;
    for (unsigned k = 0; k < script->m_read; k++) {
        std::vector<unsigned char*> cut = pointers;
        cut[k] = (unsigned char*)kEndInstruction;
        script->m_instructions = cut.data();
        g_context = base + " to " + std::to_string(k);
        run_script(script, cursor);
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
    unsigned read = 0;  // 0: the count of instructions
    // The script's destructor would free what it points at, which is this harness's.
    ~Asm() { script.m_instructions = nullptr; script.m_bytes = nullptr; script.m_read = 0; script.m_instructionCount = 0; }
    Asm& op(unsigned c) { code.push_back({(unsigned char)c, (unsigned char)(c >> 8)}); return *this; }
    Asm& op(unsigned c, std::vector<unsigned char> data) {
        std::vector<unsigned char> i = {(unsigned char)c, 0, (unsigned char)data.size(), (unsigned char)(data.size() >> 8)};
        i.insert(i.end(), data.begin(), data.end());
        code.push_back(i);
        return *this;
    }
    static void put32(std::vector<unsigned char>& d, uint32_t v) { for (int i = 0; i < 4; i++) d.push_back((unsigned char)(v >> (8 * i))); }
    Asm& i(int v) { std::vector<unsigned char> d = {7, 0}; put32(d, (uint32_t)v); return op(0x96, d); }
    Asm& f(float v) { uint32_t b; std::memcpy(&b, &v, 4); std::vector<unsigned char> d = {1, 0}; put32(d, b); return op(0x96, d); }
    Asm& s(const std::string& t) { std::vector<unsigned char> d = {0, 0}; d.insert(d.end(), t.begin(), t.end()); d.push_back(0); return op(0x96, d); }
    Asm& id(const char* name) { std::vector<unsigned char> d = {0x20, 0}; for (int k = 0; k < 8; k++) d.push_back((unsigned char)name[k]); return op(0x96, d); }
    Asm& jump(unsigned c, unsigned target) { return op(c, {(unsigned char)target, (unsigned char)(target >> 8)}); }
    Asm& label(unsigned c, unsigned kind, const std::string& t) {
        std::vector<unsigned char> d = {(unsigned char)kind, 0};
        d.insert(d.end(), t.begin(), t.end());
        d.push_back(0);
        return op(c, d);
    }
    AtFpScript* done(bool end = true) {
        if (end) op(0);
        pointers.clear();
        for (auto& c : code) pointers.push_back(c.data());
        script.m_instructions = pointers.data();
        script.m_read = read ? read : (unsigned)pointers.size();
        script.m_instructionCount = (unsigned)pointers.size();
        return &script;
    }
};

static std::string g_id_found;  // A string id the loaded strings have.

static void written(const char* name, Asm& a, unsigned cursor = 1, bool steps = true) {
    g_context = std::string("written ") + name;
    AtFpScript* s = a.done();
    if (steps) run_steps(s, cursor);
    else run_script(s, cursor);
}

static void run_written() {
    const char* sid = g_id_found.c_str();
    // Every arithmetic and comparison over each pair of kinds: integer, integral and fractional real, numeric, non-numeric and empty text, undefined (from an empty stack), NaN and infinity.
    struct Operand { const char* name; int kind; int i; float f; const char* t; };
    const Operand ops[] = {
        {"int 7", 0, 7, 0, nullptr}, {"int -3", 0, -3, 0, nullptr}, {"int 0", 0, 0, 0, nullptr}, {"real 2.5", 1, 0, 2.5f, nullptr}, {"real -0.25", 1, 0, -0.25f, nullptr},
        {"real 4", 1, 0, 4.0f, nullptr}, {"real 3e9", 1, 0, 3e9f, nullptr}, {"text 12", 2, 0, 0, "12"}, {"text 1.5e1", 2, 0, 0, "1.5e1"}, {"text abc", 2, 0, 0, "abc"}, {"text empty", 2, 0, 0, ""},
        {"text -", 2, 0, 0, "-"}, {"text long", 2, 0, 0, nullptr}, {"undefined", 3, 0, 0, nullptr}, {"text 0x10", 2, 0, 0, "0x10"}, {"text  inf", 2, 0, 0, " inf"},
    };
    const std::string longText(270, 'q');
    // Undefined is pushed as a variable never set whose name gives undefined.
    std::string undefinedName;
    for (int k = 0; undefinedName.empty(); k++) {
        const std::string n = "u" + std::to_string(k);
        if (hash_text(n.c_str()) % 7 == 0) undefinedName = n;
    }
    auto push = [&](Asm& a, const Operand& o) {
        if (o.kind == 0) a.i(o.i);
        else if (o.kind == 1) a.f(o.f);
        else if (o.kind == 2) a.s(o.t ? o.t : longText);
        else a.s(undefinedName).op(0x1C);
    };
    const unsigned binary[] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x21, 0x29};
    for (unsigned code : binary) {
        for (const Operand& x : ops) {
            for (const Operand& y : ops) {
                Asm a;
                a.s("name");
                push(a, x);
                push(a, y);
                a.op(code);
                a.op(0x1D);  // What it made, set as a variable, which logs it.
                char n[160];
                std::snprintf(n, sizeof n, "op %02X %s, %s", code, x.name, y.name);
                written(n, a, 1, false);
            }
        }
    }
    // Unary, and the handlers that pop one and push nothing or one.
    const unsigned unary[] = {0x12, 0x18, 0x14, 0x32, 0x33, 0x25, 0x8D, 0x20, 0x9E, 0x9D, 0x1C, 0x26, 0x30, 0x9F};
    for (unsigned code : unary) {
        for (const Operand& x : ops) {
            Asm a;
            a.s("name");
            push(a, x);
            if (code == 0x8D || code == 0x9F) a.op(code, {3});
            else if (code == 0x9D) a.jump(code, 6);
            else a.op(code);
            a.op(0x1D);
            char n[160];
            std::snprintf(n, sizeof n, "op %02X %s", code, x.name);
            written(n, a, 1, false);
        }
    }
    {
        // Divide by zero, and NaN; then their text.
        Asm a;
        a.i(1).i(0).op(0x0D).s("").op(0x21).i(-1).i(0).op(0x0D).s("").op(0x21).i(0).i(0).op(0x0D).s("").op(0x21).f(1e-6f).f(3.0f).op(0x0D).s("x").op(0x21);
        a.f(1e20f).s("").op(0x21).f(-123456.7f).s("").op(0x21).f(0.000123f).s("").op(0x21).f(123456.5f).s("").op(0x21);
        written("special reals", a);
    }
    {
        // Stack overflow and underflow: 35 pushes, then 40 pops.
        Asm a;
        for (int k = 0; k < 35; k++) a.i(k);
        a.s("over").s("long text over the stack's room").id(sid);
        for (int k = 0; k < 40; k++) a.op(0x17);
        a.op(0x0A).op(0x21).op(0x15).op(0x13).op(0x14);
        written("overflow and underflow", a, 1, false);
        Asm b;
        for (int k = 0; k < 29; k++) b.i(k);
        b.s("a").op(0x1C);  // GetVariable with the stack full: its value is not pushed.
        b.op(0x22).op(0x0A);
        written("full stack", b, 1, false);
    }
    {
        // Push: several in one, every kind; and the frames.
        Asm a;
        std::vector<unsigned char> d = {7, 0, 5, 0, 0, 0, 1, 0, 0, 0, 0x40, 0x40, 0, 0, 'h', 'i', 0, 0x20, 0};
        for (int k = 0; k < 8; k++) d.push_back((unsigned char)sid[k]);
        d.push_back(0); d.push_back(0);
        for (int k = 0; k < 40; k++) d.push_back('L');
        d.push_back(0);
        a.op(0x96, d);
        a.op(0x96, {7, 0, 1, 0, 0, 0, 7, 0, 2, 0, 0, 0});
        a.op(0x96, {7, 0, 3, 0, 0, 0, 0, 0, 'a', 'b', 'c', 'd', 'e', 0, 1, 0, 0, 0, 0x80, 0x3F});
        a.op(0x04).op(0x05).op(0x06).op(0x07).op(0x09).jump(0x81, 33).op(0x81, {0xFF, 0xFF});
        a.op(0x01).op(0x08).op(0x50).op(0x80, {}).op(0x8A, {1, 2, 3}).op(0xA0, {1}).op(0xFF, {}).op(0x16).op(0x27).op(0x2A);
        written("push and frames", a);
    }
    {
        // Labels and targets: text, a string id found and not found, an unknown kind; SetTarget2 of undefined, empty, a path; GotoFrame2.
        Asm a;
        a.label(0x8C, 0, "intro").label(0x8C, 0x20, std::string(sid, 8)).label(0x8C, 0x20, "~ZZ99999").label(0x8C, 5, "x");
        a.label(0x8B, 0, "/menu").label(0x8B, 0, "").label(0x8B, 0x20, std::string(sid, 8)).label(0x8B, 0x20, "~ZZ99999").label(0x8B, 3, "y");
        a.op(0x20).s("").op(0x20).s("_root.a").op(0x20).i(5).op(0x20);
        a.s("label").op(0x9F, {0}).i(12).op(0x9F, {1}).op(0x9F, {1});
        written("labels and targets", a);
    }
    {
        // Movies: GetURL with each length of name, GetURL2 by each method.
        Asm a;
        auto url = [&](const std::string& u, const std::string& t) { std::vector<unsigned char> d(u.begin(), u.end()); d.push_back(0); d.insert(d.end(), t.begin(), t.end()); d.push_back(0); a.op(0x83, d); };
        url("menu.swf", "_level0");
        url("", "");
        url("ab", "t");
        url("abcd", "t");
        url("abcde", "");
        url(std::string(0x43, 'm'), "big");
        for (unsigned method : {0u, 1u, 2u, 3u, 0x40u, 0x41u, 0x80u, 0xC0u, 0xC2u, 0xC3u, 4u, 0x44u, 0xC4u, 0xFFu}) {
            a.s("movie.swf").s("_root.x").op(0x9A, {(unsigned char)method});
        }
        a.op(0x9A, {0xC0}).op(0x9A, {0});  // Both from an empty stack.
        written("movies", a);
    }
    {
        // Properties, sprites, time, trace, random.
        Asm a;
        a.s("_root").i(3).op(0x22).s("_root.b").s("7").op(0x22).s("t").f(11.0f).op(0x22).s("t").i(13).op(0x22).op(0x22);
        a.s("_root").i(2).s("v").op(0x23).s("_root").s("4").f(1.5f).op(0x23).s("x").i(1).s(longText).op(0x23).op(0x23);
        a.s("src").s("dst").i(4).op(0x24).i(1).f(2.5f).s("9").op(0x24).op(0x24).s("spr").op(0x25).i(3).op(0x25).f(0.5f).op(0x25).op(0x25);
        a.op(0x34).op(0x34).s("trace me").op(0x26).i(7).op(0x26).op(0x26);
        a.i(10).op(0x30).i(1).op(0x30).i(-1).op(0x30).s("1000").op(0x30).f(7.9f).op(0x30);
        written("properties and the rest", a, 77);
        Asm b;
        b.i(0).op(0x30);  // A limit of 0 faults, on both sides.
        written("random by zero", b, 5, false);
    }
    {
        // Strings: extract at each edge; add; compare; lengths; characters.
        Asm a;
        const int edges[][2] = {{0, 2}, {1, 2}, {4, 9}, {5, 1}, {6, 1}, {2, 0}, {2, -1}, {-1, 2}, {3, 100}, {1, 5}};
        for (auto& e : edges) a.s("hello").i(e[0]).i(e[1]).op(0x15);
        a.s(longText).i(200).i(60).op(0x15).i(12345).i(2).i(2).op(0x15).f(2.5f).i(1).i(2).op(0x15).s("abc").s("2").s("1").op(0x15);
        a.op(0x15).s("x").op(0x15);
        a.s(longText).s(longText).op(0x21).i(5).f(0.5f).op(0x21).s("").s("").op(0x21);
        a.s("abc").s("abd").op(0x29).s("abd").s("abc").op(0x29).s("ab").s("abc").op(0x29).s("\xE9").s("a").op(0x29).s("same").s("same").op(0x13).s("A").s("a").op(0x13);
        a.s("A").op(0x32).s("").op(0x32).s("\xE9t").op(0x32).i(65).op(0x33).i(0).op(0x33).i(321).op(0x33).f(66.7f).op(0x33).s("67").op(0x33);
        a.s(longText).op(0x14).i(1234).op(0x14).f(0.125f).op(0x14);
        written("strings", a);
    }
    {
        // Control: If by each kind; Jump and If past m_read; a backward Jump that ends; Call that stops and that does not.
        Asm a;
        a.i(0).jump(0x9D, 3).i(1).f(0.5f).jump(0x9D, 6).i(2).s("1").jump(0x9D, 9).i(3).s("abc").jump(0x9D, 12).i(4).jump(0x9D, 15).i(5);  // 0..14
        a.i(1).jump(0x9D, 500).jump(0x99, 600).jump(0x99, 19).i(99).i(6);  // 15..20
        a.i(3).op(0x9E).f(4.0f).op(0x9E).f(4.5f).op(0x9E).s("Go").op(0x9E).s("12").op(0x9E).s("-4").op(0x9E).op(0x9E);
        a.s("Stop now").op(0x9E).i(42);
        written("control", a);
        Asm b;
        b.s("n").i(0).op(0x1D);                                                       // 0..2  n = 0
        b.s("n").s("n").op(0x1C).i(1).op(0x0A).op(0x1D);                              // 3..8  n = n + 1
        b.s("n").op(0x1C).i(4).op(0x0F).jump(0x9D, 3);                                // 9..13 while n < 4
        b.i(10).op(0x9E).i(13).op(0x9E).i(77);
        written("loop", b);
        Asm c;
        c.jump(0x99, 0);  // Forever: stopped by the call budget? No call out: not run.
        (void)c;
        Asm d;
        d.s("v").op(0x1C).op(0x17).jump(0x99, 0);  // Forever with a call out each time: the budget stops both sides at the same call.
        g_callout_limit = 300;  // Within the log's room.
        written("forever, stopped by the budget", d, 1, false);
        g_callout_limit = 4000;
        Asm e;
        e.i(1).op(0x9E);  // A frame of 0 does not stop.
        e.read = 1;       // and Jumps limited to instruction 0
        e.i(1).jump(0x9D, 1).jump(0x99, 1).i(8);
        written("m_read of 1", e);
    }
    {
        // The variables: set and get back each kind, long text through a handler's own value, and undefined.
        Asm a;
        a.s("a").i(5).op(0x1D).s("b").f(2.5f).op(0x1D).s("c").s("str").op(0x1D).s("d").s(longText).op(0x1D).s("e").op(0x1D).s("u_never").op(0x1D);
        for (const char* n : {"a", "b", "c", "d", "e", "u_never", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "z9"}) a.s(n).op(0x1C);
        a.i(1).op(0x1C).f(2.5f).op(0x1C).op(0x1C);
        written("variables", a);
    }
}

// Heaps: the machine's own, none, the stack's static only, and the general heap missing (a handler's long text raises -2 and goes on).
static void run_heaps() {
    Asm a;
    a.s("first").s(std::string(300, 'h')).op(0x21).s("x").op(0x1C).s("y").s(std::string(400, 'g')).op(0x1D).s("y").op(0x1C).s("abcdef").i(2).i(3).op(0x15).s("z").op(0x26).label(0x8C, 0, std::string(300, 'l'));
    AtFpScript* s = a.done();
    g_context = "heaps: machine's own";
    run_script(s, 3, &g_heap_c);
    // The stack's static changed after Init, which set it to the machine's heap: the stack's own heap is then not the static.
    g_context = "heaps: the stack's static changed";
    reset_world(3);
    {
        Machines m = make_machines(&g_heap_c);
        *(void**)(uintptr_t)0x002C6390 = &g_heap_b;
        AtFpActionScriptStack::s_heap = &g_heap_b;
        run_both(m, s);
        free_machines(m);
        set_statics(nullptr, &g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);
    }
    g_context = "heaps: none of the machine's";
    run_script(s, 3, nullptr);
    g_context = "heaps: no statics";
    set_statics(nullptr, nullptr, nullptr, nullptr, nullptr);
    run_script(s, 3, nullptr);
    g_context = "heaps: no general heap";
    set_statics(nullptr, &g_heap_token, &g_heap_token, &g_heap_token, nullptr);
    run_script(s, 3, &g_heap_token);
    set_statics(nullptr, &g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);
    // No room: each allocation failing in turn. A failure is reported and the code writes through the null, so both sides fault: compared up to there.
    for (int k = 1; k <= 8; k++) {
        g_context = "heaps: no room from allocation " + std::to_string(k);
        reset_world(3);
        Machines m = make_machines(&g_heap_token);
        g_fail_at_push = k;
        run_both(m, s);
        g_fail_at_push = 0;
        free_machines(m);
    }
}

// ---- the values and the stack, called directly ---------------------------------------------------------------------------

static const uint32_t kValueVft = 0x00272964;

struct ValueCase {
    int type;
    int integer;
    uint32_t real;
    int flags;
    const char* text;
};

static void fill_value(AtFpActionScriptValue* v, const ValueCase& c) {
    v->m_type = c.type;
    v->m_int = c.integer;
    v->m_unknown14 = (int)c.real;
    v->m_haveTextInt = c.flags & 1;
    v->m_haveTextReal = (c.flags >> 1) & 1;
    v->m_haveIntReal = (c.flags >> 2) & 1;
    if (c.text) {
        const unsigned cap = (unsigned)std::strlen(c.text) + 1;
        v->m_text = (char*)std::calloc(1, std::max(cap, 0x40u));
        std::memcpy(v->m_text, c.text, cap);
        v->m_capacity = std::max(cap, 0x40u);
    }
    v->m_heap = &g_heap_token;
}

static uint32_t bits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }

static void run_value_methods() {
    std::vector<ValueCase> cases = {
        {0, 0, 0, 0, "123"}, {0, 0, 0, 0, "-45xyz"}, {0, 0, 0, 0, "  7.25e2"}, {0, 0, 0, 0, ""}, {0, 0, 0, 0, "-"}, {0, 0, 0, 0, "--1"}, {0, 0, 0, 0, "12-"}, {0, 0, 0, 0, "0x1F"},
        {0, 0, 0, 0, "inf"}, {0, 0, 0, 0, "nan"}, {0, 0, 0, 0, "1e400"}, {0, 0, 0, 0, "99999999999"}, {0, 0, 0, 0, "+8"}, {0, 0, 0, 0, ".5"}, {0, 0, 0, 0, "5."}, {0, 0, 0, 0, "1e"},
        {0, 0, 0, 0, "1e+"}, {0, 0, 0, 0, "\t\n 3"}, {0, 0, 0, 0, "1.5E-3"}, {0, 0, 0, 0, "abc"}, {0, 9, bits(9.5f), 3, "9"}, {0, 0, 0, 0, "\xB5\xB6"},
        {7, 42, 0, 0, nullptr}, {7, -7, 0, 0, "old text"}, {7, (int)0x80000000, 0, 0, nullptr}, {7, 5, bits(1.0f), 4, nullptr}, {7, 5, 0, 1, "5"},
        {1, 0, bits(2.5f), 0, nullptr}, {1, 0, bits(-1e-7f), 0, nullptr}, {1, 0, bits(3e9f), 0, nullptr}, {1, 0, bits(-3e9f), 0, nullptr}, {1, 7, bits(7.75f), 4, nullptr},
        {1, 0, 0x7F800000, 0, nullptr}, {1, 0, 0xFF800000, 0, nullptr}, {1, 0, 0xFFC00000, 0, nullptr}, {1, 0, 0x7FC00001, 0, nullptr}, {1, 0, 0x7F800001, 0, nullptr},
        {1, 0, bits(1.0e-40f), 0, nullptr}, {1, 0, bits(2147483648.0f), 0, nullptr}, {1, 0, bits(0.1f), 2, "0.1"},
        {3, 0, 0, 0, nullptr}, {3, 3, 0, 0, "stale"}, {2, 0, 0, 0, nullptr}, {5, 1, 0, 0, "five"},
        // Each cache flag alone, with a cached form that disagrees, so that a wrong flag shows.
        {0, 4, bits(7.5f), 2, "9"}, {0, 4, bits(7.5f), 1, "9"}, {7, 5, bits(6.5f), 1, "stale"}, {7, 5, bits(6.5f), 2, "stale"}, {7, 5, bits(6.5f), 4, "stale"},
        {1, 3, bits(2.5f), 1, "cached"}, {1, 3, bits(2.5f), 2, "cached"}, {1, 3, bits(2.5f), 4, nullptr},
        // Rounding that carries into a new digit.
        {1, 0, bits(999999.5f), 0, nullptr}, {1, 0, bits(9.999996f), 0, nullptr}, {1, 0, bits(0.0009999996f), 0, nullptr}, {1, 0, bits(99999.95f), 0, nullptr}, {1, 0, bits(-9999999.0f), 0, nullptr},
    };
    const unsigned fixedCases = (unsigned)cases.size();
    // Random reals, as bit patterns: every sign, exponent and mantissa, for the text of a real.
    uint32_t seed = 12345;
    const int randoms = g_quick ? 2000 : 200000;
    for (int k = 0; k < randoms; k++) {
        seed = seed * 1103515245u + 12345u;
        uint32_t b = seed;
        seed = seed * 1103515245u + 12345u;
        b ^= seed >> 16;
        if (k % 4 == 1) b = (b & 0x807FFFFF) | ((uint32_t)(100 + (seed >> 8) % 60) << 23);  // Around 1 to 1e9.
        if (k % 4 == 2) b = (b & 0x80003FFF) | ((uint32_t)(127 + (seed >> 8) % 24) << 23);  // Short mantissas: ties at the sixth digit.
        cases.push_back({1, 0, b, 0, nullptr});
    }
    for (int k = 0; k < (g_quick ? 200 : 20000); k++) {
        seed = seed * 1103515245u + 12345u;
        char t[64];
        switch (k % 4) {
        case 0: std::snprintf(t, sizeof t, "%d", (int)seed); break;
        case 1: std::snprintf(t, sizeof t, "%.*g", 1 + (int)(seed >> 28), (double)(int)seed / (double)(1 + (seed & 0xFFFF))); break;
        case 2: std::snprintf(t, sizeof t, "%ue%d", seed >> 20, (int)(seed & 0x3F) - 32); break;
        default: std::snprintf(t, sizeof t, " -%u.%u", seed >> 24, seed & 0xFFF); break;
        }
        static std::deque<std::string> keep;
        keep.push_back(t);
        cases.push_back({0, 0, 0, 0, keep.back().c_str()});
    }
    enum Method { kToReal, kToInt, kToText, kLength, kIsInteger, kCount };
    unsigned index = 0;
    for (const ValueCase& c : cases) {
        for (int method = 0; method < kCount; method++) {
            // Random reals only through ToText, ToInt and IsInteger.
            if (index >= fixedCases && c.type == 1 && method != kToText && method != kIsInteger) continue;
            if (index >= fixedCases && c.type == 0 && method != kToReal && method != kToInt && method != kIsInteger) continue;
            AtFpActionScriptValue* o = make<AtFpActionScriptValue>(0x00020A50, 0xCD, true);
            AtFpActionScriptValue* r = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
            fill_value(o, c);
            fill_value(r, c);
            char what[64];
            std::snprintf(what, sizeof what, "value method %d on case %u", method, index);
            g_context = what;
            long long ro = 0, rr = 0;
            std::string so, sr;
            const bool oc = run_side(0, [&] {
                switch (method) {
                case kToReal: ro = bits(at<FnF0>(0x000207E0)(o, nullptr)); break;
                case kToInt: ro = at<FnI0>(0x00020830)(o, nullptr); break;
                case kToText: { const char* p = at<FnS0>(0x00020880)(o, nullptr); so = p ? p : "(null)"; break; }
                case kLength: ro = at<FnI0>(0x00020900)(o, nullptr); break;
                case kIsInteger: ro = at<FnI0>(0x00020930)(o, nullptr); break;
                }
            });
            const bool rc = run_side(1, [&] {
                switch (method) {
                case kToReal: rr = bits(r->ToReal()); break;
                case kToInt: rr = r->ToInt(); break;
                case kToText: { const char* p = r->ToText(); sr = p ? p : "(null)"; break; }
                case kLength: rr = r->Length(); break;
                case kIsInteger: rr = r->IsInteger(); break;
                }
            });
            eq("crash", oc, rc);
            eq("returns", ro, rr);
            eqs("returns text", so.c_str(), sr.c_str());
            same_value(o, r);
            check_logs(what);
            run_side(0, [&] { at<Fn0>(0x00020A80)(o, nullptr); });
            run_side(1, [&] { r->Reset(); });
            check_logs("reset");
            std::free(o);
            std::free(r);
        }
        index++;
    }
}

static void run_value_setters() {
    // SetInt and SetReal over values that held text; SetReal over every kind of real.
    std::vector<uint32_t> reals = {bits(0.0f), 0x80000000u, bits(1.0f), bits(-1.0f), bits(0.5f), bits(2147483520.0f), bits(2147483648.0f), bits(-2147483648.0f), bits(-2147483904.0f), 0x7F800000u, 0xFFC00000u,
                                   0x7FC00000u, 0x00000001u, bits(16777217.0f), bits(1e10f), bits(-0.999f)};
    uint32_t seed = 99;
    for (int k = 0; k < (g_quick ? 100 : 5000); k++) { seed = seed * 1103515245u + 12345u; reals.push_back(seed); }
    for (uint32_t b : reals) {
        AtFpActionScriptValue* o = make<AtFpActionScriptValue>(0x00020A50, 0xCD, true);
        AtFpActionScriptValue* r = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
        fill_value(o, {0, 0, 0, 7, "held"});
        fill_value(r, {0, 0, 0, 7, "held"});
        g_context = "SetReal " + std::to_string(b);
        float f;
        std::memcpy(&f, &b, 4);
        run_side(0, [&] { at<Fn1>(0x00020790)(o, nullptr, (uintptr_t)&f); });
        run_side(1, [&] { r->SetReal(&f); });
        same_value(o, r);
        const int i = (int)b;
        run_side(0, [&] { at<Fn1>(0x00020770)(o, nullptr, (uintptr_t)&i); });
        run_side(1, [&] { r->SetInt(&i); });
        same_value(o, r);
        run_side(0, [&] { at<Fn0>(0x00020740)(o, nullptr); });
        run_side(1, [&] { r->Clear(); });
        same_value(o, r);
        check_logs("setters");
        std::free(o);
        std::free(r);
    }
    // SetText of a C string and of a string object: null, short, long, over text already held.
    AtTextAscii ascii;
    static char data[300];
    std::memset(data, 'w', sizeof data - 1);
    ascii.Init(data, sizeof data);
    AtText* fromTable = g_strings ? g_strings->StringAt(0) : nullptr;
    const std::string longText(500, 'n');
    for (int k = 0; k < 8; k++) {
        for (int held = 0; held < 3; held++) {
            AtFpActionScriptValue* o = make<AtFpActionScriptValue>(0x00020A50, 0xCD, true);
            AtFpActionScriptValue* r = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
            if (held == 1) { fill_value(o, {0, 0, 0, 7, "held"}); fill_value(r, {0, 0, 0, 7, "held"}); }
            if (held == 2) { fill_value(o, {7, 4, 0, 7, nullptr}); fill_value(r, {7, 4, 0, 7, nullptr}); o->m_heap = r->m_heap = nullptr; }
            g_context = "SetText " + std::to_string(k) + " over " + std::to_string(held);
            const char* texts[] = {nullptr, "", "short", longText.c_str()};
            const bool oc = run_side(0, [&] {
                if (k < 4) at<Fn1>(0x00020C60)(o, nullptr, (uintptr_t)texts[k]);
                else at<Fn1>(0x00020C00)(o, nullptr, (uintptr_t)(k == 4 ? nullptr : k == 5 ? (AtText*)&ascii : fromTable));
            });
            const bool rc = run_side(1, [&] {
                if (k < 4) r->SetText(texts[k]);
                else r->SetText(k == 4 ? nullptr : k == 5 ? (AtText*)&ascii : fromTable);
            });
            eq("crash", oc, rc);
            same_value(o, r);
            check_logs("SetText");
            run_side(0, [&] { at<Fn0>(0x00020A80)(o, nullptr); });
            run_side(1, [&] { r->Reset(); });
            check_logs("reset");
            std::free(o);
            std::free(r);
        }
    }
}

// A handler's own value: made, given room of each size, moved, given a block, reset, destroyed.
static void run_local_values() {
    const unsigned sizes[] = {0, 1, 0x20, 0xFE, 0xFF, 0x100, 0x1000, 0x7FFFFFFF, 0x80000000u};
    for (unsigned first : sizes) {
        for (unsigned second : sizes) {
            for (int then = 0; then < 5; then++) {
                char what[96];
                std::snprintf(what, sizeof what, "local value: room %X then %X, then %d", first, second, then);
                g_context = what;
                if ((first >= 0x80000000u || second >= 0x80000000u || first == 0x7FFFFFFF || second == 0x7FFFFFFF) && then != 0) continue;
                AtFpActionScriptLocalValue* o = make<AtFpActionScriptLocalValue>(0x0002D650, 0xCD, true);
                AtFpActionScriptLocalValue* r = make<AtFpActionScriptLocalValue>(0x0002D650, 0xCD, false);
                same_value(o, r, true);
                AtFpActionScriptValue* ot = make<AtFpActionScriptValue>(0x00020A50, 0xCD, true);
                AtFpActionScriptValue* rt = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
                ot->m_heap = rt->m_heap = &g_heap_token;
                AtFpActionScriptLocalValue* ol = make<AtFpActionScriptLocalValue>(0x0002D650, 0xCD, true);
                AtFpActionScriptLocalValue* rl = make<AtFpActionScriptLocalValue>(0x0002D650, 0xCD, false);
                const bool big = first >= 0x7FFFFFFF || second >= 0x7FFFFFFF;
                if (big) g_fail_alloc = false;
                char* block[2] = {(char*)std::calloc(1, 16), (char*)std::calloc(1, 16)};
                std::strcpy(block[0], "given");
                std::strcpy(block[1], "given");
                auto body = [&](int side, AtFpActionScriptLocalValue* v, AtFpActionScriptValue* to, AtFpActionScriptLocalValue* local) {
                    if (big) g_fail_alloc = true;  // So that a huge request fails rather than allocating.
                    if (then == 1) v->m_heap = &g_heap_c;  // Not the heap a local value's blocks come from.
                    v->Reserve(first);
                    if (v->m_text && first < 0x80000000u) std::strcpy(v->m_text, first > 3 ? "abc" : "");
                    v->Reserve(second);
                    g_fail_alloc = false;
                    if (big) return;
                    v->m_type = 0;
                    if (v->m_text) std::strcpy(v->m_text, "xyz");
                    switch (then) {
                    case 0: v->MoveTo(to); break;
                    case 1: v->MoveTo(local); break;
                    case 2: v->SetString(block[side], 16); block[side] = nullptr; break;
                    case 3: v->Reset(); break;
                    case 4: to->MoveTo(v); break;
                    }
                };
                // Through each side's vftable: the original's slots on the original's object.
                const bool oc = run_side(0, [&] { body(0, o, ot, ol); });
                const bool rc = run_side(1, [&] { body(1, r, rt, rl); });
                eq("crash", oc, rc);
                same_value(o, r, true);
                same_value(ot, rt);
                same_value(ol, rl, true);
                check_logs(what);
                if (!oc && !rc) {
                    run_side(0, [&] { at<Fn0>(0x0002D7C0)(o, nullptr); at<Fn0>(0x0002D7C0)(ol, nullptr); at<Fn0>(0x00020A80)(ot, nullptr); });
                    run_side(1, [&] { r->~AtFpActionScriptLocalValue(); rl->~AtFpActionScriptLocalValue(); rt->Reset(); });
                    check_logs("local values destroyed");
                }
                std::free(block[0]);
                std::free(block[1]);
                std::free(o); std::free(r); std::free(ot); std::free(rt); std::free(ol); std::free(rl);
            }
        }
    }
    // CopyFrom over each kind of from, into a value with and without room.
    const ValueCase froms[] = {{0, 1, 0, 0, "copy me"}, {7, 3, 0, 0, "stale"}, {7, 3, 0, 1, "3"}, {1, 2, bits(2.5f), 2, "2.5"}, {1, 2, bits(2.5f), 4, nullptr}, {3, 0, 0, 0, nullptr}, {3, 0, 0, 1, "x"}};
    for (const ValueCase& c : froms) {
        for (int room = 0; room < 2; room++) {
            g_context = "CopyFrom";
            AtFpActionScriptValue* f0 = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
            fill_value(f0, c);
            AtFpActionScriptValue* o = make<AtFpActionScriptValue>(0x00020A50, 0xCD, true);
            AtFpActionScriptValue* r = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
            if (room) { fill_value(o, {0, 0, 0, 0, std::string(100, 'r').c_str()}); fill_value(r, {0, 0, 0, 0, std::string(100, 'r').c_str()}); }
            else { o->m_heap = r->m_heap = &g_heap_b; }
            uintptr_t ro = 0, rr = 0;
            run_side(0, [&] { ro = (uintptr_t)at<FnP1>(0x0002D200)(o, nullptr, (uintptr_t)f0); });
            run_side(1, [&] { rr = (uintptr_t)r->CopyFrom(f0); });
            eq("returns this", ro == (uintptr_t)o, rr == (uintptr_t)r);
            same_value(o, r);
            check_logs("CopyFrom");
            std::free(o); std::free(r);
        }
    }
}

// The stack's methods directly: on an empty, a part-full and a full stack, with its own heap or the static.
static void run_stack_methods() {
    AtTextAscii ascii;
    static char data[40] = "an ascii string";
    ascii.Init(data, sizeof data);
    for (int fill = 0; fill < 4; fill++) {
        for (int method = 0; method < 12; method++) {
            for (int own = 0; own < 2; own++) {
                char what[96];
                std::snprintf(what, sizeof what, "stack method %d at fill %d, own heap %d", method, fill, own);
                g_context = what;
                AtFpActionScriptStack* o = make<AtFpActionScriptStack>(0x0002D270, 0xCD, true);
                AtFpActionScriptStack* r = make<AtFpActionScriptStack>(0x0002D270, 0xCD, false);
                o->m_heap = r->m_heap = own ? &g_heap_c : nullptr;
                run_side(0, [&] { at<Fn1>(0x0002D580)(o, nullptr, 6); });
                run_side(1, [&] { r->Allocate(6); });
                const int depth = fill == 0 ? 0 : fill == 1 ? 2 : fill == 2 ? 5 : 6;
                for (int k = 0; k < depth; k++) {
                    run_side(0, [&] { at<FnI1>(0x0002D2F0)(o, nullptr, (uintptr_t)(k * 11)); });
                    run_side(1, [&] { r->PushInt(k * 11); });
                }
                AtFpActionScriptLocalValue* lo = make<AtFpActionScriptLocalValue>(0x0002D650, 0xCD, true);
                AtFpActionScriptLocalValue* lr = make<AtFpActionScriptLocalValue>(0x0002D650, 0xCD, false);
                AtFpActionScriptValue* from = make<AtFpActionScriptValue>(0x00020A50, 0xCD, false);
                fill_value(from, {0, 0, 0, 0, "from"});
                char* block[2] = {(char*)std::calloc(1, 8), (char*)std::calloc(1, 8)};
                std::strcpy(block[0], "blk");
                std::strcpy(block[1], "blk");
                int ro = 0, rr = 0;
                const bool oc = run_side(0, [&] {
                    switch (method) {
                    case 0: ro = at<FnI1>(0x0002D1B0)(o, nullptr, (uintptr_t)lo); break;
                    case 1: ro = at<FnI0>(0x0002D1E0)(o, nullptr); break;
                    case 2: ro = at<FnI1>(0x0002D2F0)(o, nullptr, (uintptr_t)-5); break;
                    case 3: ro = at<FnIF>(0x0002D340)(o, nullptr, 2.75f); break;
                    case 4: ro = at<FnIF>(0x0002D340)(o, nullptr, -8.0f); break;
                    case 5: ro = at<FnI1>(0x0002D390)(o, nullptr, (uintptr_t)"text"); break;
                    case 6: ro = at<FnI1>(0x0002D3E0)(o, nullptr, (uintptr_t)&ascii); break;
                    case 7: ro = at<FnI0>(0x0002D430)(o, nullptr); break;
                    case 8: ro = at<FnI2>(0x0002D480)(o, nullptr, (uintptr_t)block[0], 8); if (ro) block[0] = nullptr; break;
                    case 9: ro = at<FnI1>(0x0002D4D0)(o, nullptr, (uintptr_t)from); break;
                    case 10: lo->Reserve(4); std::strcpy(lo->m_text, "lcl"); lo->m_type = 0; ro = at<FnI1>(0x0002D520)(o, nullptr, (uintptr_t)lo); break;
                    case 11: ro = at<FnI1>(0x0002D390)(o, nullptr, 0); break;
                    }
                });
                const bool rc = run_side(1, [&] {
                    switch (method) {
                    case 0: rr = r->Pop(lr); break;
                    case 1: rr = r->Discard(); break;
                    case 2: rr = r->PushInt(-5); break;
                    case 3: rr = r->PushReal(2.75f); break;
                    case 4: rr = r->PushReal(-8.0f); break;
                    case 5: rr = r->PushText("text"); break;
                    case 6: rr = r->PushText(&ascii); break;
                    case 7: rr = r->PushUndefined(); break;
                    case 8: rr = r->PushString(block[1], 8); if (rr) block[1] = nullptr; break;
                    case 9: rr = r->PushCopy(from); break;
                    case 10: lr->Reserve(4); std::strcpy(lr->m_text, "lcl"); lr->m_type = 0; rr = r->PushFrom(lr); break;
                    case 11: rr = r->PushText((const char*)nullptr); break;
                    }
                });
                eq("crash", oc, rc);
                eq("returns", ro, rr);
                same_stack(o, r);
                same_value(lo, lr, true);
                check_logs(what);
                run_side(0, [&] { at<Fn0>(0x0002D7C0)(lo, nullptr); at<Fn0>(0x0002D290)(o, nullptr); });
                run_side(1, [&] { lr->~AtFpActionScriptLocalValue(); r->Free(); });
                check_logs("stack freed");
                std::free(block[0]);
                std::free(block[1]);
                std::free(o); std::free(r); std::free(lo); std::free(lr);
            }
        }
    }
}

// ---- real archives --------------------------------------------------------------------------------------------------

static unsigned g_archives, g_afa, g_blocks, g_instructions;

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

// The archive's scripts and strings, loaded by the recovered loader, outside the comparison.
static AtFlashScriptTable* load_scripts(const std::string& afd) {
    const std::string sname = word_name(g_files[afd + ".AFD"], "Script");
    if (!g_files.count(upper("ASCRIPT\\" + sname + ".AFA"))) return nullptr;
    AtFlashScriptTable* t = new AtFlashScriptTable;
    g_fs->m_path.m_top = 0;
    t->Load(sname.c_str(), 2, &g_app[1]);
    g_log[0].n = g_log[1].n = 0;  // The file's reads, logged outside the comparison.
    g_message[0][0] = g_message[1][0] = 0;
    return t;
}

static void run_archive(const std::string& afd, bool written_here) {
    AtFlashScriptTable* t = load_scripts(afd);
    if (!t) return;
    g_afa++;
    g_strings = &t->m_strings;
    // A string id this archive has, for the written scripts.
    if (g_id_found.empty() && t->m_strings.m_entries) {
        for (unsigned k = 0; k < t->m_strings.m_size; k++) {
            if (t->m_strings.m_entries[k].m_value) { g_id_found.assign(t->m_strings.m_entries[k].m_name, 8); break; }
        }
    }
    for (unsigned k = 0; k < t->m_count; k++) {
        AtFpScript* s = t->m_scripts[k];
        g_context = afd + " block " + std::to_string(k);
        run_steps(s, 17 + k);
        g_blocks++;
        g_instructions += s->m_read;
    }
    if (written_here) {
        run_value_methods();
        run_value_setters();
        run_local_values();
        run_stack_methods();
        run_written();
        run_heaps();
    }
    // The table is left loaded: its strings are not needed again, and its clear is the aft suite's.
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
        else if (!std::strcmp(argv[i], "--trace")) g_trace = true;
        else if (!std::strcmp(argv[i], "--quick")) g_quick = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_asvm <default.xbe> <cases dir> [--entries FILE] [--mutate] [--quick]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)push_counting);
    redirect((uint32_t)(uintptr_t)&AtPushHeap, (const void*)push_counting);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    // What the machine calls out to, on both sides.
    struct Seam { uint32_t original, recovered; const void* to; };
    const Seam seams[] = {
        {0x0002BF50, addr_of(&AtFpDisplayManager::GetCurrentFrame), (const void*)d_current_frame},
        {0x0002B970, addr_of(&AtFpDisplayManager::GotoFrame), (const void*)d_goto_frame},
        {0x0002BFC0, addr_of(&AtFpDisplayManager::SetPlaying), (const void*)d_set_playing},
        {0x0002BF20, addr_of(&AtFpDisplayManager::StopSounds), (const void*)d_stop_sounds},
        {0x0002BF80, addr_of(&AtFpDisplayManager::GotoFrameByLabel), (const void*)d_goto_label},
        {0x0002CB70, addr_of(&AtFpDisplayManager::GotoFrameByExpression), (const void*)d_goto_expression},
        {0x0002C3B0, addr_of(&AtFpDisplayManager::SetTarget), (const void*)d_set_target},
        {0x0002CFD0, addr_of(&AtFpDisplayManager::LoadMovie), (const void*)d_load_movie},
        {0x0002CC90, addr_of(&AtFpDisplayManager::ResolveTarget), (const void*)d_resolve},
        {0x0002C9C0, addr_of(&AtFpDisplayManager::GetVariable), (const void*)d_get_variable},
        {0x0002CAA0, addr_of(&AtFpDisplayManager::SetVariable), (const void*)d_set_variable},
        {0x0002C320, addr_of(&AtFpDisplayManager::GetProperty), (const void*)d_get_property},
        {0x0002C360, addr_of(&AtFpDisplayManager::SetProperty), (const void*)d_set_property},
        {0x0002C150, addr_of(static_cast<int (AtFpDisplayManager::*)(int)>(&AtFpDisplayManager::ExecuteScripts)), (const void*)d_exec_frame},
        {0x0002C1C0, addr_of(static_cast<int (AtFpDisplayManager::*)(const char*)>(&AtFpDisplayManager::ExecuteScripts)), (const void*)d_exec_label},
        {0x00016130, addr_of(&AtFpDisplayMgrContextStack::SetTarget), (const void*)d_context_target},
        {0x0002D9E0, addr_of(&AtFpRfcTable::Call), (const void*)d_rfc_call},
        {0x00021620, addr_of(&AtFpRfcArgs::Construct), (const void*)d_args_construct},
        {0x00021AD0, addr_of(&AtFpRfcArgs::Destruct), (const void*)d_args_destruct},
        {0x000216E0, addr_of(&AtFpRfcArgs::Init), (const void*)d_args_init},
        {0x00021DD0, addr_of(&AtFpRfcArgs::Add), (const void*)d_args_add},
        {0x00021680, addr_of(&AtFpRfcArgs::Clear), (const void*)d_args_clear},
    };
    for (const Seam& s : seams) {
        redirect(s.original, s.to);
        redirect(s.recovered, s.to);
    }
    redirect(0x00020640, (const void*)d_time);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

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
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C626Cu, 0x002C6324u, 0x002C63D0u, 0x002C62F8u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    g_ctx.m_spaces.Init(64);
    AtTextTable::s_heap = &g_heap_token;
    AtFlashResourceTable::s_defaultHeap = &g_heap_token;
    AtFlashScriptTable::s_defaultHeap = &g_heap_token;
    AtFpScript::s_heap = &g_heap_token;
    set_statics(nullptr, &g_heap_token, &g_heap_token, &g_heap_token, &g_heap_token);
    for (int side = 0; side < 2; side++) {
        std::memset((void*)&g_app[side], 0, sizeof g_app[side]);
        g_app[side].m_display.m_contexts.m_count = 1;
        g_app[side].m_display.m_contexts.m_entries[0].m_context = &g_context_token[side];
    }
    g_heap_pad = 0x100;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.

    std::string dir = pos[1];
    const std::string jams = dir + "\\..\\oracle-jam\\";
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((jams + "*.txt").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) { std::printf("cannot list %s\n", jams.c_str()); return 2; }
    std::vector<std::string> lists;
    do lists.push_back(found.cFileName); while (FindNextFileA(h, &found));
    FindClose(h);
    std::sort(lists.begin(), lists.end());
    bool first = true;
    for (const std::string& l : lists) {
        std::string afd;
        const std::string stem = l.substr(0, l.size() - 4);
        if (!load_archive(jams + l, jams + stem + ".bin", &afd) || afd.empty()) continue;
        g_archives++;
        const unsigned before = g_afa;
        run_archive(afd, first);
        if (g_afa != before) first = false;
    }
    report_entries();
    std::printf("%s: %u archives, %u .AFA; %u script blocks, %u instructions; %u runs, %u of them cut at an instruction; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_archives, g_afa, g_blocks, g_instructions, g_runs, g_steps, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
