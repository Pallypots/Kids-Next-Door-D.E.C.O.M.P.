// Differential oracle for the behaviour scripts' abstract machine and the script system's loading and running (subsystem at/smallvm; Windows x86, local
// only). See docs/verification.md.
//
//   oracle_smallvm <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// What stands in for what, first. The built-ins the scripts call reach game code that is not recovered: one double stands for every one of them on both sides,
// told by its registered context which built-in it is. It logs its arguments (and the strings they address, read through each side's own GetAddr, StrLen and
// GetString), answers from a sequence both sides replay, and sometimes raises an error through each side's own RaiseError. The debug hook is a double too: at
// each event (every LINE, call, return, symbol) it logs the machine's registers and a digest of the whole memory, which is the step-by-step comparison. The
// original's heap, push, pop, raise and fatal paths are doubles (tests/oracle/oracle_doubles.h); its file code is the original's own, over the same file
// system in memory. Everything else runs for real on both sides: every function recorded for this subsystem, and the recovered members it calls from other
// subsystems (the name map, the file reference, the path stack), the original's on the original side.
//
// Programs: the three compiled scripts on the disc, from index.txt (every public, main and invalid entries, with and without debugging, single-step suspension
// and errors raised by built-ins), and programs written here for every opcode the scripts do not use.
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

// Freeing a pointer into the middle of a block is what the original's Register does on a missing built-in; the harness's free must survive it on both sides.
#include <cstdlib>
namespace std {
void* oracle_calloc(size_t n, size_t m);
void oracle_free(void* p);
}
#define calloc oracle_calloc
#define free oracle_free

#include "at/AtIoMountManager.h"
#include "at/AtIoTokenizer.h"
#include "at/AtSmallVm.h"
#include "gm/GmLevelManifest.h"
#include "gm/GmNameMap.h"
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
#undef calloc
#undef free

// ---- the harness's allocations, tracked without allocating -------------------------------------------------------------------
struct Block { unsigned char* p; size_t n; };
static Block g_blocks[1 << 16];
static int g_interior_frees;
static size_t slot_of(const void* p) { return ((uintptr_t)p >> 4) * 2654435761u % (sizeof g_blocks / sizeof g_blocks[0]); }
void* std::oracle_calloc(size_t n, size_t m) {
    unsigned char* p = (unsigned char*)::calloc(n, m);
    if (!p) return p;
    size_t i = slot_of(p);
    while (g_blocks[i].p) i = (i + 1) % (sizeof g_blocks / sizeof g_blocks[0]);
    g_blocks[i] = {p, n * m};
    return p;
}
void std::oracle_free(void* v) {
    if (!v) return;
    unsigned char* p = (unsigned char*)v;
    size_t i = slot_of(p);
    for (size_t k = 0; k < sizeof g_blocks / sizeof g_blocks[0] && g_blocks[i].p; k++, i = (i + 1) % (sizeof g_blocks / sizeof g_blocks[0])) {
        if (g_blocks[i].p == p) {
            // Remove and reinsert what follows, to keep the probing sound.
            g_blocks[i].p = nullptr;
            size_t j = (i + 1) % (sizeof g_blocks / sizeof g_blocks[0]);
            while (g_blocks[j].p) {
                Block b = g_blocks[j];
                g_blocks[j].p = nullptr;
                size_t s = slot_of(b.p);
                while (g_blocks[s].p) s = (s + 1) % (sizeof g_blocks / sizeof g_blocks[0]);
                g_blocks[s] = b;
                j = (j + 1) % (sizeof g_blocks / sizeof g_blocks[0]);
            }
            ::free(p);
            return;
        }
    }
    // Not the start of a block: find the block it points into, by scanning (rare).
    for (const Block& b : g_blocks) {
        if (b.p && p > b.p && p < b.p + b.n) {
            g_interior_frees++;
            if (g_recording) g_log[g_side].add('I', (uint32_t)(p - b.p));
            return;
        }
    }
    ::free(p);
}

static uint32_t g_rng = 13579;
static uint32_t rnd() { g_rng = g_rng * 1103515245u + 12345u; return g_rng >> 8; }

// ---- where pointers point ------------------------------------------------------------------------------------------------------
struct Region { const unsigned char* base; uint32_t size; uint32_t tag; };
static std::vector<Region> g_regions[2];
static void clear_regions() { g_regions[0].clear(); g_regions[1].clear(); }
static void add_region(int side, const void* base, uint32_t size, uint32_t tag) {
    const bool was = g_recording;
    g_recording = false;
    g_regions[side].push_back({(const unsigned char*)base, size, tag});
    g_recording = was;
}
static uint32_t rel_on(int side, const void* p) {
    if (p == nullptr) return 0;
    for (const Region& r : g_regions[side]) {
        if ((const unsigned char*)p >= r.base && (const unsigned char*)p < r.base + r.size) return r.tag + (uint32_t)((const unsigned char*)p - r.base);
    }
    return (uint32_t)(uintptr_t)p;
}
static void L(uint32_t tag, uint32_t v = 0) { if (g_recording) g_log[g_side].add(tag, v); }

// What the recovered code linked here needs of the platform; no case reaches it.
static int g_bad;
void* AtPlatformAllocateRegion(unsigned int) { g_bad++; return nullptr; }
void AtPlatformFreeRegion(void*) { g_bad++; }
void AtPlatformInitDevices() { g_bad++; }
void AtLibraryFree(void*) { g_bad++; }
int AtPlatformLanguage() { g_bad++; return 1; }
int AtPlatformGameRegion() { g_bad++; return 1; }
unsigned int AtPlatformMilliseconds() { g_bad++; return 0; }
AtHeap* AtCurrentHeap() { return nullptr; }

static unsigned char g_tokens[64][16];
static int g_hostvar[8];
static int g_heap_token;

// ---- the original's entry points ------------------------------------------------------------------------------------------------
typedef int(__cdecl* OInit)(void*, void*);
typedef int(__cdecl* OExec)(void*, int*, int, int, const int*);
typedef int(__cdecl* ORegister)(void*, const void*, int);
typedef int(__cdecl* OFindPublic)(void*, const char*, int*);
typedef int(__cdecl* ORaise)(void*, int);
typedef int(__cdecl* OGetAddr)(void*, int, int**);
typedef int(__cdecl* OStrLen)(const int*, int*);
typedef int(__cdecl* OGetString)(char*, const int*);
typedef int(__cdecl* OCompare)(const char*, const char*);
typedef void*(__cdecl* OInstance)();
typedef void(__fastcall* OThis0)(void*, void*);
typedef void*(__fastcall* OCreate)(void*, void*);
typedef void(__fastcall* ORelease)(void*, void*, void*);
typedef int(__fastcall* OName)(void*, void*, const char*);
typedef void*(__fastcall* ONameP)(void*, void*, const char*);
typedef void*(__fastcall* OLoad)(void*, void*, void*, const char*, int);
typedef int(__cdecl* ORun)(void*, void*, int, ...);
typedef int(__fastcall* OCall)(void*, void*, void*, int, int);
typedef int(__fastcall* OStart)(void*, void*, const void*);
typedef int(__fastcall* OReg)(void*, void*, void*, const char*, AtSmallCNative);
typedef void*(__fastcall* OOwnerLoad)(void*, void*, const char*);
typedef void*(__fastcall* OOwnerStart)(void*, void*, const char*, const char*);
typedef int(__fastcall* OOwnerCall)(void*, void*, void*, int);

// ---- the doubles beneath both sides -------------------------------------------------------------------------------------------
// The built-ins: one function, its context tells which.
static uint32_t g_answer[2];
static int g_answer_mode;      // 0: zero; 1: one; 2: from the sequence; 3: the sequence, raising now and then
static int g_native_calls[2];
static uint32_t answer(int side) { g_answer[side] = g_answer[side] * 69069u + 1u; return g_answer[side] >> 16; }
static uint32_t digest(const unsigned char* p, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}
static int __cdecl d_native(void* context, AtAmx* amx, int* params) {
    const int side = g_side;
    g_native_calls[side]++;
    L('n', (uint32_t)(((unsigned char*)context - &g_tokens[0][0]) / 16));
    L('nb', (uint32_t)params[0]);
    L('nc', (uint32_t)amx->m_cip);
    L('ns', (uint32_t)amx->m_stk);
    L('nh', (uint32_t)amx->m_hea);
    L('nf', (uint32_t)amx->m_frm);
    const int count = params[0] / 4;
    for (int i = 1; i <= count && i <= 8; i++) {
        L('na', (uint32_t)params[i]);
        int* physical = nullptr;
        const int r = side == 0 ? at<OGetAddr>(0x0007AFD0)(amx, params[i], &physical) : AtAmxGetAddr(amx, params[i], &physical);
        L('ng', (uint32_t)r);
        if (r == 0 && params[i] >= 0 && params[i] < amx->m_hea) {
            L('np', rel_on(side, physical));
            int length = -1;
            const int s = side == 0 ? at<OStrLen>(0x0007B000)(physical, &length) : AtAmxStrLen(physical, &length);
            L('nl', (uint32_t)(s * 0x10000 + length));
            if (length >= 0 && length < 200) {
                char text[1024];
                std::memset(text, 0x5A, sizeof text);
                side == 0 ? at<OGetString>(0x0007B080)(text, physical) : AtAmxGetString(text, physical);
                L('nt', digest((const unsigned char*)text, length + 2));
            }
        }
    }
    uint32_t v = 0;
    if (g_answer_mode == 1) v = 1;
    if (g_answer_mode >= 2) v = answer(side) % 3 == 0 ? 0 : (answer(side) & 0xFF);
    if (g_answer_mode == 3 && answer(side) % 11 == 0) {
        const int e = 10 + (int)(answer(side) % 3);
        const int r = side == 0 ? at<ORaise>(0x0007AFC0)(amx, e) : AtAmxRaiseError(amx, e);
        L('ne', (uint32_t)r);
    }
    return (int)v;
}

static int& oglobal(uint32_t va);
// The debug hook: the step-by-step comparison.
static int g_debug_init_answer = 21;
static int g_debug_events[2];
static uint32_t g_mem_size[2];
static unsigned char* g_mem_base[2];
static int __cdecl d_debug(AtAmx* amx) {
    const int side = g_side;
    g_debug_events[side]++;
    L('d', (uint32_t)amx->m_dbgcode);
    L('dl', (uint32_t)amx->m_curline);
    L('df', (uint32_t)amx->m_curfile);
    L('da', (uint32_t)amx->m_dbgaddr);
    L('dp', (uint32_t)amx->m_dbgparam);
    L('dF', (uint32_t)amx->m_frm);
    L('dS', (uint32_t)amx->m_stk);
    L('dH', (uint32_t)amx->m_hea);
    L('dn', rel_on(side, amx->m_dbgname));
    L('dg', (uint32_t)amx->m_flags);
    L('go', (uint32_t)(side == 0 ? oglobal(0x00301B3C) : AtAmxGlobals::s_offs));
    L('gn', (uint32_t)(side == 0 ? oglobal(0x00301B38) : AtAmxGlobals::s_num));
    L('gp', (uint32_t)(side == 0 ? oglobal(0x00301B40) : AtAmxGlobals::s_op));
    if (g_mem_base[side] && amx->m_base == g_mem_base[side]) {
        const AtAmxHeader* h = (const AtAmxHeader*)amx->m_base;
        L('dm', digest(amx->m_base + h->m_dat, (int)g_mem_size[side] - h->m_dat));
    }
    return amx->m_dbgcode == 0 ? g_debug_init_answer : 0;
}

static int __cdecl d_callback(AtAmx* amx, int index, int* result, int* params) {
    L('cb', (uint32_t)index);
    return g_side == 0 ? at<int(__cdecl*)(AtAmx*, int, int*, int*)>(0x0007A7D0)(amx, index, result, params) : AtAmxCallback(amx, index, result, params);
}
static bool g_preset_callback;

// The file system in memory beneath both sides.
class PathFs : public MemFs {
public:
    int Open(const char* path, int flags, AtIoFile** out) override {
        const char* current = m_path.Current();
        g_log[g_side].add('pt', (uint32_t)m_path.m_top);
        record_message(current ? current : "(no directory)", g_side);
        return MemFs::Open(path, flags, out);
    }
};
static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static unsigned char g_mount[0x40];

// ---- comparing ------------------------------------------------------------------------------------------------------------------
static uint32_t fn_id(int side, const void* f) {
    const uint32_t v = (uint32_t)(uintptr_t)f;
    if (side == 0) {
        if (v == 0x0007A7D0) return 1;
        if (v == 0x0007A880) return 2;
    } else {
        if (f == (const void*)AtAmxCallback) return 1;
        if (f == (const void*)AtAmxDebugNone) return 2;
    }
    if (f == (const void*)d_debug) return 3;
    if (f == (const void*)d_callback) return 4;
    return v;
}

// The program image, relocation-aware: in the code, a word either side changed must have changed by that side's own code address (a relocated jump).
static void compare_memory(const char* what, const unsigned char* o, const unsigned char* r, const std::vector<unsigned char>& file, int size) {
    if (!o || !r) { eq(what, o != nullptr, r != nullptr); return; }
    const AtAmxHeader* ho = (const AtAmxHeader*)o;
    const AtAmxHeader* hr = (const AtAmxHeader*)r;
    const int cod = ho->m_cod, dat = ho->m_dat;
    eq(what, cod, hr->m_cod);
    eq(what, dat, hr->m_dat);
    const uint32_t co = (uint32_t)(uintptr_t)(o + cod), cr = (uint32_t)(uintptr_t)(r + cod);
    int diffs = 0;
    for (int i = 0; i + 4 <= size; i += 4) {
        const uint32_t a = *(const uint32_t*)(o + i), b = *(const uint32_t*)(r + i);
        if (a == b) { g_checks++; continue; }
        const uint32_t f = i + 4 <= (int)file.size() ? *(const uint32_t*)(&file[i]) : 0xDEADBEEF;
        if (i >= cod && i < dat && (a != f || b != f) && a - co == b - cr) { g_checks++; continue; }
        if (diffs++ < 3) {
            char text[300];
            _snprintf_s(text, sizeof text, _TRUNCATE, "%s: memory at +0x%X", what, i);
            eq(text, a, b);
        }
    }
    if (diffs) eq(what, 0, diffs);
}

static const char* F(const char* what, const char* field) {
    static char text[8][256];  // labels nest: each call has a buffer of its own
    static int next;
    char* t = text[next++ % 8];
    _snprintf_s(t, 256, _TRUNCATE, "%s: %s", what, field);
    return t;
}

static void compare_amx(const char* label, int ob, const AtAmx& o, const AtAmx& r) {
    const std::string copy = label;  // the label may sit in a buffer F reuses
    const char* what = copy.c_str();
    (void)ob;
    eq(F(what, "m_base"), rel_on(0, o.m_base), rel_on(1, r.m_base));
    eq(F(what, "m_callback"), fn_id(0, (const void*)o.m_callback), fn_id(1, (const void*)r.m_callback));
    eq(F(what, "m_debug"), fn_id(0, (const void*)o.m_debug), fn_id(1, (const void*)r.m_debug));
    eq(F(what, "m_cip"), o.m_cip, r.m_cip);
    eq(F(what, "m_frm"), o.m_frm, r.m_frm);
    eq(F(what, "m_hea"), o.m_hea, r.m_hea);
    eq(F(what, "m_stk"), o.m_stk, r.m_stk);
    eq(F(what, "m_stp"), o.m_stp, r.m_stp);
    eq(F(what, "m_flags"), o.m_flags, r.m_flags);
    eq(F(what, "m_curline"), o.m_curline, r.m_curline);
    eq(F(what, "m_curfile"), o.m_curfile, r.m_curfile);
    eq(F(what, "m_dbgcode"), o.m_dbgcode, r.m_dbgcode);
    eq(F(what, "m_dbgaddr"), o.m_dbgaddr, r.m_dbgaddr);
    eq(F(what, "m_dbgparam"), o.m_dbgparam, r.m_dbgparam);
    eq(F(what, "m_dbgname"), rel_on(0, o.m_dbgname), rel_on(1, r.m_dbgname));
    eq(what, std::memcmp(o.m_user, r.m_user, sizeof o.m_user), 0);
    eq(F(what, "m_error"), o.m_error, r.m_error);
}

static void compare_script(const char* label, const AtSmallCScript& o, const AtSmallCScript& r) {
    const std::string copy = label;  // the label may sit in a buffer F reuses
    const char* what = copy.c_str();
    eq(F(what, "m_owner"), rel_on(0, o.m_owner), rel_on(1, r.m_owner));
    compare_amx(what, 0, o.m_amx, r.m_amx);
        eq(F(what, "m_name"), strncmp(o.m_name, r.m_name, 8), 0);  // past a short name's terminator the bytes are the caller's stack: not compared
    eq(F(what, "m_program"), rel_on(0, o.m_program), rel_on(1, r.m_program));
    eq(F(what, "m_flags"), o.m_flags, r.m_flags);
    eq(F(what, "m_valid"), o.m_valid, r.m_valid);
    eq(F(what, "m_unknown78"), o.m_unknown78, r.m_unknown78);
    eq(F(what, "m_entry"), o.m_entry, r.m_entry);
    eq(F(what, "m_shared"), o.m_shared, r.m_shared);
    eq(F(what, "m_savedPublic"), rel_on(0, o.m_savedPublic), rel_on(1, r.m_savedPublic));
    eq(F(what, "m_savedData"), rel_on(0, o.m_savedData), rel_on(1, r.m_savedData));
    eq(F(what, "m_savedPri"), o.m_savedPri, r.m_savedPri);
    eq(F(what, "m_savedAlt"), o.m_savedAlt, r.m_savedAlt);
    eq(F(what, "m_savedStk"), o.m_savedStk, r.m_savedStk);
    eq(F(what, "m_savedFrm"), o.m_savedFrm, r.m_savedFrm);
    eq(F(what, "m_savedHea"), o.m_savedHea, r.m_savedHea);
    eq(F(what, "m_resetStk"), o.m_resetStk, r.m_resetStk);
    eq(F(what, "m_savedCip"), rel_on(0, o.m_savedCip), rel_on(1, r.m_savedCip));
    eq(F(what, "m_retval"), o.m_retval, r.m_retval);
}

static int& oglobal(uint32_t va) { return *(int*)(uintptr_t)va; }
static void compare_globals(const char* what) {
    eq(F(what, "little-endian"), oglobal(0x00301B34), AtAmxGlobals::s_littleEndian);
    eq(F(what, "num"), oglobal(0x00301B38), AtAmxGlobals::s_num);
    eq(F(what, "offs"), oglobal(0x00301B3C), AtAmxGlobals::s_offs);
    eq(F(what, "op"), oglobal(0x00301B40), AtAmxGlobals::s_op);
    eq(F(what, "debugging"), oglobal(0x00301B44), AtAmxGlobals::s_debugging);
}
static void set_globals(int v) {
    oglobal(0x00301B34) = AtAmxGlobals::s_littleEndian = v & 1;
    oglobal(0x00301B38) = AtAmxGlobals::s_num = v;
    oglobal(0x00301B3C) = AtAmxGlobals::s_offs = v;
    oglobal(0x00301B40) = AtAmxGlobals::s_op = v;
    oglobal(0x00301B44) = AtAmxGlobals::s_debugging = v & 1;
}

// ---- the two systems ------------------------------------------------------------------------------------------------------------
static unsigned char g_osys_mem[sizeof(AtSmallCSystem)];
static AtSmallCSystem* g_osys = (AtSmallCSystem*)g_osys_mem;  // the original's layout: the same on x86
static AtSmallCSystem* g_rsys;
static unsigned char g_ofactory[16], g_rfactory_mem[16];
static unsigned char g_oowner[8], g_rowner[8];
static AtSmallCSystem* sys(int side) { return side == 0 ? g_osys : g_rsys; }

// The names of every built-in the disc's scripts declare, and a few more; context i is g_tokens[i].
static const char* const kNativeNames[] = {
    "bProcessEvent", "vState", "bAnim", "nGetStat", "nGetStatMax", "bSpawnActor", "vSetStat", "bStaticPath", "vStartTimer", "bGetActorRegBool",
    "vStopTimer", "vMotionResume", "vMotion", "vInvalidEvent", "bProcessEventUp", "bIsTimerOn", "bIsAnimOver", "vSetActorRegBool", "vSubscribe",
    "vEnableActorDraw", "vEnableActorCol", "vEnableLineSeg", "vPublishEvent", "vDisableActorDraw", "vDisableActorCol", "vDisableLineSeg",
    "bSendTriggerEvent", "vSetUsable", "vSendTriggerAnim", "nA", "nB", "nC", "nD"};
static const int kNativeCount = sizeof kNativeNames / sizeof kNativeNames[0];
static AtSmallCNativeEntry g_list[64];  // a list as AtAmxRegister reads it, ending with an empty name

static void make_list() {
    std::memset(g_list, 0, sizeof g_list);
    for (int i = 0; i < kNativeCount; i++) {
        strcpy_s(g_list[i].m_name, kNativeNames[i]);
        g_list[i].m_function = (AtSmallCNative)d_native;
        g_list[i].m_context = g_tokens[i];
    }
}

static void setup_systems() {
    // The original's system, built by its own constructor and Init.
    std::memset(g_osys_mem, 0xCD, sizeof g_osys_mem);
    at<OThis0>(0x000756C0)(g_osys, nullptr);
    *(void**)(uintptr_t)0x002CF170 = &g_heap_token;
    at<OThis0>(0x000756E0)(g_osys, nullptr);
    g_osys->m_factory = (AtSmallCScriptFactory*)g_ofactory;
    void* block = std::malloc(sizeof(AtSmallCSystem));
    std::memset(block, 0xCD, sizeof(AtSmallCSystem));
    g_rsys = new (block) AtSmallCSystem();
    AtSmallCSystem::s_heap = &g_heap_token;
    g_rsys->Init();
    g_rsys->m_factory = (AtSmallCScriptFactory*)g_rfactory_mem;
    for (int i = 0; i < kNativeCount; i++) {
        at<OReg>(0x000754D0)(g_osys, nullptr, g_tokens[i], kNativeNames[i], (AtSmallCNative)d_native);
        g_rsys->Register(g_tokens[i], kNativeNames[i], (AtSmallCNative)d_native);
    }
    // The host variables the written programs declare. With a count of -1 the search for a name stops only at a match, so every name a started
    // program declares must be here (the disc's scripts declare none).
    for (int side = 0; side < 2; side++) {
        AtSmallCHostVar* v = sys(side)->HostVars();
        for (int i = 0; i < 3; i++) {
            std::memset(&v[i], 0, sizeof v[i]);
            sprintf_s(v[i].m_name, "hv%c", 'A' + i);
            v[i].m_address = &g_hostvar[i];
        }
    }
    *(AtSmallCSystem**)(uintptr_t)0x002CF174 = g_osys;
    AtSmallCSystem::s_instance = g_rsys;
    *(void**)(g_oowner + 4) = g_osys;
    ((GmScriptOwner*)g_rowner)->m_system = g_rsys;
}

// A script object of each side, made by that side's constructor over filled memory.
static AtSmallCScript* new_script(int side, unsigned char fill) {
    unsigned char* p = (unsigned char*)std::malloc(sizeof(AtSmallCScript));
    std::memset(p, fill, sizeof(AtSmallCScript));
    if (side == 0) at<OThis0>(0x00075350)(p, nullptr);
    else new (p) AtSmallCScript();
    ((AtSmallCScript*)p)->m_owner = (AtSmallCScriptFactory*)(side == 0 ? g_ofactory : g_rfactory_mem);
    return (AtSmallCScript*)p;
}

// A program's memory: the file, then whatever the heap held, as LoadProgram allocates it.
static unsigned char* new_program(const std::vector<unsigned char>& file, int memory, unsigned char fill) {
    const int n = memory > (int)file.size() ? memory : (int)file.size();
    unsigned char* p = (unsigned char*)std::malloc(n + 0x100);
    std::memset(p, fill, n + 0x100);
    std::memcpy(p, file.data(), file.size());
    return p;
}
static int memory_of(const std::vector<unsigned char>& file) {
    const int stp = file.size() >= 0x40 ? ((const AtAmxHeader*)file.data())->m_stp : 0;
    return stp > 0 && stp < 0x100000 ? stp : 0;
}
static int extent(const std::vector<unsigned char>& file) { const int m = memory_of(file); return m > (int)file.size() ? m : (int)file.size(); }

struct Pair {
    AtSmallCScript* s[2] = {nullptr, nullptr};
    unsigned char* prog[2] = {nullptr, nullptr};
    int memory = 0;
    int init[2] = {0, 0};
    int reg[2] = {0, 0};
};

// Both sides: a script object, the program in memory, Init with the given hooks, the built-ins registered (count -9: not registered).
static void prepare(Pair& p, const std::vector<unsigned char>& file, bool debug, int list_count) {
    p.memory = memory_of(file);
    clear_regions();
    for (int side = 0; side < 2; side++) {
        p.s[side] = new_script(side, 0xA0);
        p.prog[side] = new_program(file, p.memory, 0xE5);
        add_region(side, p.prog[side], (uint32_t)extent(file), 0x10000000);
        add_region(side, p.s[side], sizeof(AtSmallCScript), 0x20000000);
        add_region(side, side == 0 ? g_ofactory : g_rfactory_mem, 16, 0x30000000);
        g_mem_base[side] = p.prog[side];
        g_mem_size[side] = (uint32_t)(p.memory > 0 ? p.memory : 0);
        AtAmx* amx = &p.s[side]->m_amx;
        std::memset(amx, 0, sizeof(AtAmx));
        if (debug) amx->m_debug = d_debug;
        if (g_preset_callback) amx->m_callback = d_callback;
        On on(side);
        p.init[side] = side == 0 ? at<OInit>(0x0007AB40)(amx, p.prog[side]) : AtAmxInit(amx, p.prog[side]);
        if (p.init[side] == 0 && list_count != -9) {
            p.reg[side] = side == 0 ? at<ORegister>(0x0007ADD0)(amx, g_list, list_count) : AtAmxRegister(amx, g_list, list_count);
        }
    }
    eq("init", p.init[0], p.init[1]);
    eq("register", p.reg[0], p.reg[1]);
}
static void release(Pair& p) {
    for (int side = 0; side < 2; side++) {
        std::free(p.s[side]);
        std::free(p.prog[side]);
        g_mem_base[side] = nullptr;
    }
}

static int g_log_dumps;
static void show_log_mismatch() {
    for (int i = 0; i < g_log[0].n && i < g_log[1].n; i += 2) {
        if (g_log[0].v[i] != g_log[1].v[i] || g_log[0].v[i + 1] != g_log[1].v[i + 1]) {
            if (g_log_dumps++ < 6) {
                std::printf("LOG at %d of %d/%d in [%s]:\n", i, g_log[0].n, g_log[1].n, g_context.c_str());
                for (int side = 0; side < 2; side++) {
                    for (int k = i < 16 ? 0 : i - 16; k < i + 8 && k < g_log[side].n; k += 2) {
                        const uint32_t t = g_log[side].v[k];
                        std::printf("%c%c%c%c=%X ", t >> 24 ? (char)(t >> 24) : ' ', t >> 16 ? (char)(t >> 16) : ' ', t >> 8 ? (char)(t >> 8) : ' ', (char)t, g_log[side].v[k + 1]);
                    }
                    std::printf("\n");
                }
            }
            return;
        }
    }
}

static void compare_pair(const char* what, Pair& p, const std::vector<unsigned char>& file) {
    show_log_mismatch();
    compare_logs(what);
    compare_script(what, *p.s[0], *p.s[1]);
    compare_memory(what, p.prog[0], p.prog[1], file, extent(file));
    compare_globals(what);
}

// One call of Exec on each side; each side's answers replay the same sequence.
static AtSmallCHostVar g_hostvars[8];

static int exec_both(Pair& p, int index, int numparams, const int* params, unsigned flags, bool give_retval) {
    int r[2] = {0, 0}, retval[2] = {0x1234, 0x1234};
    bool crashed[2];
    const uint32_t seed = g_answer[0];
    int vars[2][8];
    std::memcpy(vars[0], g_hostvar, sizeof g_hostvar);
    for (int side = 0; side < 2; side++) {
        if (side == 1) {  // One set of host variables serves both sides: the first side's writes are kept aside and undone.
            std::memcpy(vars[1], g_hostvar, sizeof g_hostvar);
            std::memcpy(g_hostvar, vars[0], sizeof g_hostvar);
        }
        g_answer[side] = seed;
        p.s[side]->m_flags = flags;
        On on(side);
        crashed[side] = guarded([&] {
            r[side] = side == 0 ? at<OExec>(0x0007B1C0)(p.s[side], give_retval ? &retval[side] : nullptr, index, numparams, params)
                                : AtAmxExec(p.s[side], give_retval ? &retval[side] : nullptr, index, numparams, params);
        });
    }
    eq("exec crashed", crashed[0], crashed[1]);
    eq("host variables", std::memcmp(vars[1], g_hostvar, sizeof g_hostvar), 0);
    if (!crashed[0] && !crashed[1]) {
        eq("exec result", r[0], r[1]);
        eq("exec retval", retval[0], retval[1]);
    }
    eq("answers", g_answer[0], g_answer[1]);
    return crashed[0] ? 0x7777 : r[0];
}

static void set_step(int v) {
    g_osys->m_stepMode = v;
    g_rsys->m_stepMode = v;
}

// A whole call: fresh, then resumed while it suspends.
static int g_suspensions, g_runs;
static void run_call(Pair& p, const std::vector<unsigned char>& file, int index, int numparams, const int* params, bool give_retval) {
    int r = exec_both(p, index, numparams, params, AtSmallCScript::kFresh | (rnd() & 0x7E), give_retval);
    compare_pair("call", p, file);
    for (int k = 0; r == -1 && k < 400; k++) {
        g_suspensions++;
        r = exec_both(p, index, numparams, params, rnd() & 0x7E, give_retval);
        compare_pair("resumed call", p, file);
    }
    g_runs++;
}

// ---- the disc's scripts -------------------------------------------------------------------------------------------------------
struct Script { std::string name; std::vector<unsigned char> data; };
static std::vector<Script> g_scripts;
static unsigned g_script_runs;

static void run_real(const Script& s) {
    const AtAmxHeader* h = (const AtAmxHeader*)s.data.data();
    std::vector<int> entries = {-1, h->m_numPublics, -3, 0x7FFFFFFF};
    for (int i = 0; i < h->m_numPublics; i++) entries.push_back(i);
    for (int debug = 0; debug < 2; debug++) {
        for (int step = 0; step <= debug; step++) {
            for (int mode = 0; mode < 4; mode++) {
                g_context = s.name + (debug ? " debugging" : "") + (step ? " stepping" : "") + " answers " + std::to_string(mode);
                g_answer_mode = mode;
                g_debug_init_answer = debug ? 0 : 21;
                set_step(step);
                Pair p;
                prepare(p, s.data, debug != 0, -1);
                compare_pair("prepared", p, s.data);
                // Every entry, one after another on the same machine, as the game calls them.
                for (int e : entries) {
                    const int params[3] = {(int)rnd() % 50, 7, -1};
                    run_call(p, s.data, e, (int)(rnd() % 3), params, (rnd() & 3) != 0);
                    g_script_runs++;
                }
                release(p);
            }
        }
    }
    set_step(0);
    g_debug_init_answer = 21;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_INT_OVERFLOW) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void run_written();
static void run_system();

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
    if (pos.size() != 2) { std::printf("usage: oracle_smallvm <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new PathFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    *(AtIoContext**)(g_mount + 0x14) = &g_ctx;
    *(void**)(uintptr_t)0x002E26E0 = g_mount;
    AtIoMountManager::s_instance = (AtIoMountManager*)g_mount;
    for (uint32_t va : {0x002C60B0u, 0x002C632Cu, 0x002CD0E8u, 0x002CF170u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    AtSmallCScriptFactory::s_heap = &g_heap_token;
    AtSmallCSystem::s_heap = &g_heap_token;
    g_heap_pad = 0x200;
    g_fill = 0xCD;
    make_list();
    for (int i = 0; i < 3; i++) {
        sprintf_s(g_hostvars[i].m_name, "hv%c", 'A' + i);
        g_hostvars[i].m_address = &g_hostvar[i];
    }
    setup_systems();

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.

    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ASL\t")) continue;
        char* tab = std::strchr(line, '\t');
        *tab = 0;
        const char* leaf = std::strrchr(tab + 1, '\\');
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        Script s;
        s.data.resize((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!s.data.empty()) std::fread(s.data.data(), 1, s.data.size(), f);
        std::fclose(f);
        s.name = leaf ? std::string(leaf + 1, std::strcspn(leaf + 1, "\t")) : line;
        g_scripts.push_back(s);
    }
    std::fclose(index);
    if (g_scripts.size() != 3) { std::printf("expected the disc's three .ASL files, found %u\n", (unsigned)g_scripts.size()); return 2; }
    for (const Script& s : g_scripts) run_real(s);
    std::printf("the disc's scripts: %d differences\n", g_failures);
    run_written();
    std::printf("written programs: %d differences\n", g_failures);
    run_system();
    std::printf("the system: %d differences\n", g_failures);

    report_entries();
    if (g_bad) { std::printf("the platform layer was reached %d times\n", g_bad); g_failures++; }
    std::printf("scripts: %u files, %u calls, %d suspensions; built-in calls %d/%d; debug events %d/%d; interior frees %d\n", (unsigned)g_scripts.size(),
                g_script_runs, g_suspensions, g_native_calls[0], g_native_calls[1], g_debug_events[0], g_debug_events[1], g_interior_frees);
    std::printf("%s: %d crashes on both sides; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", (int)g_crashes, g_checks,
                g_failures);
    return g_failures ? 1 : 0;
}

// ---- programs written here -----------------------------------------------------------------------------------------------------
enum Op {
    LOAD_PRI = 1, LOAD_ALT, LOAD_S_PRI, LOAD_S_ALT, LREF_PRI, LREF_ALT, LREF_S_PRI, LREF_S_ALT, LOAD_I, LODB_I, CONST_PRI, CONST_ALT, ADDR_PRI, ADDR_ALT,
    STOR_PRI, STOR_ALT, STOR_S_PRI, STOR_S_ALT, SREF_PRI, SREF_ALT, SREF_S_PRI, SREF_S_ALT, STOR_I, STRB_I, LIDX, LIDX_B, IDXADDR, IDXADDR_B, ALIGN_PRI,
    ALIGN_ALT, LCTRL, SCTRL, MOVE_PRI, MOVE_ALT, XCHG, PUSH_PRI, PUSH_ALT, PUSH_R, PUSH_C, PUSH, PUSH_S, POP_PRI, POP_ALT, STACK, HEAP, PROC_, RET, RETN,
    CALL, CALL_PRI, JUMP, JREL, JZER, JNZ, JEQ, JNEQ, JLESS, JLEQ, JGRTR, JGEQ, JSLESS, JSLEQ, JSGRTR, JSGEQ, SHL, SHR, SSHR, SHL_C_PRI, SHL_C_ALT,
    SHR_C_PRI, SHR_C_ALT, SMUL, SDIV, SDIV_ALT, UMUL, UDIV, UDIV_ALT, ADD, SUB, SUB_ALT, AND, OR, XOR, NOT, NEG, INVERT, ADD_C, SMUL_C, ZERO_PRI, ZERO_ALT,
    ZERO, ZERO_S, SIGN_PRI, SIGN_ALT, EQ, NEQ, LESS, LEQ, GRTR, GEQ, SLESS, SLEQ, SGRTR, SGEQ, EQ_C_PRI, EQ_C_ALT, INC_PRI, INC_ALT, INC, INC_S, INC_I,
    DEC_PRI, DEC_ALT, DEC, DEC_S, DEC_I, MOVS, CMPS, FILL, HALT, BOUNDS, SYSREQ_PRI, SYSREQ_C, FILE_, LINE, SYMBOL, SRANGE, JUMP_PRI, SWITCH, CASETBL,
    HOSTGET, HOSTSET, SUSPEND
};

struct Prog {
    std::vector<int> code, data;
    struct Fix { int at, label; bool relative; };
    std::vector<int> labels;
    std::vector<Fix> fixes;
    std::vector<std::string> publics;
    std::vector<int> public_labels;
    std::vector<std::string> natives = {"nA", "bAnim", "nB", "vState"};
    std::vector<std::string> hostvars;
    int stack = 0x800, flags = 0, version = 1, main_label = -1, compact = 0;
    bool no_main = false;
    int label() { labels.push_back(-1); return (int)labels.size() - 1; }
    void mark(int l) { labels[l] = (int)code.size() * 4; }
    void op(int o) { code.push_back(o); }
    void op(int o, int a) { code.push_back(o); code.push_back(a); }
    void to(int o, int l) { code.push_back(o); fixes.push_back({(int)code.size(), l, false}); code.push_back(0); }
    void word_to(int l) { fixes.push_back({(int)code.size(), l, false}); code.push_back(0); }
    void rel(int o, int l) { code.push_back(o); fixes.push_back({(int)code.size(), l, true}); code.push_back(0); }
    void name(const char* s) {  // a name padded to whole cells
        const int n = ((int)std::strlen(s) + 4) / 4;
        for (int i = 0; i < n; i++) {
            int w = 0;
            for (int k = 0; k < 4; k++) {
                const size_t at = (size_t)(i * 4 + k);
                w |= (at < std::strlen(s) ? (unsigned char)s[at] : 0) << (k * 8);
            }
            code.push_back(w);
        }
    }
    std::vector<unsigned char> build() {
        for (const Fix& f : fixes) code[f.at] = f.relative ? labels[f.label] - (f.at * 4 + 4) : labels[f.label];
        const int np = (int)publics.size(), nn = (int)natives.size(), nh = (int)hostvars.size();
        const int cod = 0x40 + (np + nn + nh) * 0x1C;
        std::vector<int> cells(code);
        cells.insert(cells.end(), data.begin(), data.end());
        std::vector<unsigned char> body;
        if (compact) {
            for (int v : cells) {
                std::vector<int> g;
                int x = v;
                for (;;) {
                    g.push_back(x & 0x7F);
                    x >>= 7;
                    if ((x == 0 && !(g.back() & 0x40)) || (x == -1 && (g.back() & 0x40))) break;
                }
                if (compact == 2 && v == 0x1234567) { g.push_back(0); g.push_back(0x15); }  // more groups than a cell holds: the shift wraps
                for (size_t i = g.size(); i-- > 0;) body.push_back((unsigned char)(g[i] | (i ? 0x80 : 0)));
            }
        } else {
            body.resize(cells.size() * 4);
            std::memcpy(body.data(), cells.data(), body.size());
        }
        const int dat = cod + (int)code.size() * 4, hea = dat + (int)data.size() * 4, stp = hea + stack;
        std::vector<unsigned char> f(cod + body.size(), 0);
        AtAmxHeader* h = (AtAmxHeader*)f.data();
        h->m_size = (int)f.size();
        h->m_magic = 0xF1E0;
        h->m_fileVersion = 3;
        h->m_amxVersion = (signed char)version;
        h->m_flags = (unsigned short)(flags | (compact ? 4 : 0));
        h->m_defsize = 0x1C;
        h->m_cod = cod;
        h->m_dat = dat;
        h->m_hea = hea;
        h->m_stp = stp;
        h->m_cip = no_main ? -1 : (main_label >= 0 ? labels[main_label] : 0);
        h->m_numPublics = (short)np;
        h->m_publics = 0x40;
        h->m_numNatives = (short)nn;
        h->m_natives = 0x40 + np * 0x1C;
        h->m_libraries = cod;
        h->m_pubVars = cod;
        h->m_numHostVars = (short)nh;
        h->m_hostVars = 0x40 + (np + nn) * 0x1C;
        AtAmxFuncStub* stub = (AtAmxFuncStub*)(f.data() + 0x40);
        for (int i = 0; i < np; i++, stub++) { stub->m_address = labels[public_labels[i]]; strcpy_s(stub->m_name, publics[i].c_str()); }
        for (int i = 0; i < nn; i++, stub++) strcpy_s(stub->m_name, natives[i].c_str());
        for (int i = 0; i < nh; i++, stub++) strcpy_s(stub->m_name, hostvars[i].c_str());
        std::memcpy(f.data() + cod, body.data(), body.size());
        return f;
    }
};

static int rv() {
    static const int v[] = {0, 1, -1, 2, 3, 4, 5, 7, 8, 31, 32, 33, 0x7F, 0x80, 0xFF, 0x100, 0x7FFFFFFF, (int)0x80000000, -2, -4, -7, 100, -100, 0x1FF, 0x180};
    return rnd() % 4 == 0 ? (int)(rnd() * 7919u) : v[rnd() % (sizeof v / sizeof v[0])];
}
enum { kCells = 64, kResults = 8 };
static int valid_address() { return 4 * (kResults + 8 + (int)(rnd() % (kCells - kResults - 16))); }
// An address the checked instructions test: mostly inside the data, sometimes at the heap, the stack's end, past it, before the data or between cells.
static int checked_address(const Prog& p) {
    const int hea = (int)p.data.size() * 4, stp = hea + p.stack - 4;
    switch (rnd() % 10) {
    case 0: return hea + 4 * (int)(rnd() % 8);
    case 1: return stp - 4 * (int)(rnd() % 4);
    case 2: return stp + 4 * (int)(rnd() % 3);
    case 3: return -4 * (int)(1 + rnd() % 8);
    case 4: return valid_address() + 1 + (int)(rnd() % 3);
    default: return valid_address();
    }
}

// One instruction under test, with what it needs before and after; the stack is left as it was found.
static void body(Prog& p, int o, int end_label) {
    switch (o) {
    case LOAD_PRI: case LOAD_ALT: case STOR_PRI: case STOR_ALT: case PUSH: case ZERO: case INC: case DEC:
        p.op(o, valid_address());
        if (o == PUSH) p.op(POP_ALT);
        break;
    case LOAD_S_PRI: case LOAD_S_ALT: case PUSH_S: {
        static const int offs[] = {-4, -8, -12, -16, 0, 4, 8, 12};
        p.op(STACK, -16);
        p.op(o, offs[rnd() % 8]);
        if (o == PUSH_S) p.op(POP_ALT);
        p.op(STACK, 16);
        break;
    }
    case STOR_S_PRI: case STOR_S_ALT: case ZERO_S: case INC_S: case DEC_S:
        p.op(STACK, -16);
        p.op(o, -4 * (int)(1 + rnd() % 4));
        p.op(STACK, 16);
        break;
    case LREF_PRI: case LREF_ALT: case SREF_PRI: case SREF_ALT:
        p.op(o, 4 * (kResults + (int)(rnd() % 8)));  // those cells hold addresses
        break;
    case LREF_S_PRI: case LREF_S_ALT: case SREF_S_PRI: case SREF_S_ALT:
        p.op(STACK, -16);
        p.op(PUSH_PRI);
        p.op(CONST_PRI, valid_address());
        p.op(STOR_S_PRI, -8);
        p.op(POP_PRI);
        p.op(o, -8);
        p.op(STACK, 16);
        break;
    case LOAD_I: case LODB_I: case INC_I: case DEC_I:
        p.op(CONST_PRI, o == INC_I || o == DEC_I ? valid_address() : checked_address(p));
        if (o == LODB_I) p.op(o, (int)(rnd() % 6)); else p.op(o);
        break;
    case POP_PRI: case POP_ALT:
        p.op(PUSH_C, rv());
        p.op(o);
        break;
    case STOR_I: case STRB_I: {
        int a = checked_address(p);
        if (a < 0) a = -a + 4 * kResults;  // a write before the data would change the code that runs next
        p.op(CONST_ALT, a);
        if (o == STRB_I) p.op(o, (int)(rnd() % 6)); else p.op(o);
        break;
    }
    case LIDX: case LIDX_B: {
        static const int idx[] = {0, 1, 2, 3, 5, 8, -1, -4, 100000, 30};
        p.op(CONST_ALT, valid_address());
        p.op(CONST_PRI, idx[rnd() % 10]);
        if (o == LIDX_B) p.op(o, (int)(rnd() % 4)); else p.op(o);
        break;
    }
    case IDXADDR_B: case SHL_C_PRI: case SHL_C_ALT: case SHR_C_PRI: case SHR_C_ALT:
        p.op(o, (int)(rnd() % 40));
        break;
    case ALIGN_PRI: case ALIGN_ALT:
        p.op(o, (int)(rnd() % 6) - (int)(rnd() % 2));
        break;
    case CONST_PRI: case CONST_ALT: case ADDR_PRI: case ADDR_ALT:
        p.op(o, rv());
        break;
    case ADD_C: case SMUL_C: case EQ_C_PRI: case EQ_C_ALT: case BOUNDS: case PUSH_C:
        if (o == EQ_C_PRI || o == EQ_C_ALT || o == BOUNDS) p.op(rnd() % 2 ? MOVE_PRI : MOVE_ALT);
        if ((o == EQ_C_PRI || o == EQ_C_ALT) && rnd() % 2) {
            const int v = rv();
            p.op(o == EQ_C_PRI ? CONST_PRI : CONST_ALT, v);
            p.op(o, v);
        } else {
            p.op(o, rnd() % 3 == 0 ? 0 : rv());
        }
        if (o == PUSH_C) p.op(POP_PRI);
        break;
    case LCTRL:  // every register, each kept in a result cell
        for (int k = -1; k <= 7; k++) {
            p.op(LCTRL, k);
            p.op(STOR_PRI, 4 * (kResults + 20 + k));  // past the cells main's end overwrites
        }
        break;
    case SCTRL: {
        const int which = rnd() % 3 == 0 ? 6 : (int)(rnd() % 8) - 1;
        if (which == 4 || which == 5) {
            p.op(LCTRL, which);
            p.op(SCTRL, which);
        } else if (which == 6) {
            const int l = p.label();
            p.to(CONST_PRI, l);
            p.op(SCTRL, 6);
            p.op(CONST_PRI, 0x333);
            p.mark(l);
        } else {
            p.op(CONST_PRI, rnd() % 2 ? (int)p.data.size() * 4 + 4 * (int)(rnd() % 16) : rv());
            p.op(SCTRL, which);
        }
        break;
    }
    case PUSH_PRI: case PUSH_ALT:
        p.op(o);
        p.op(rnd() % 2 ? POP_PRI : POP_ALT);
        break;
    case PUSH_R: {
        const int n = (int)(rnd() % 5);
        p.op(o, n);
        p.op(LINE, 4);  // the debug hook sees the operand global PUSH_R leaves
        p.op(0);
        p.op(STACK, 4 * n);
        break;
    }
    case STACK: {
        switch (rnd() % 6) {
        case 0: p.op(STACK, -0x100000); break;  // into the heap
        case 1: p.op(STACK, 0x100000); break;   // past the stack's end
        default: { const int k = 4 * (int)(rnd() % 20); p.op(STACK, -k); p.op(STACK, k); }
        }
        break;
    }
    case HEAP: {
        switch (rnd() % 6) {
        case 0: p.op(HEAP, 0x100000); break;
        case 1: p.op(HEAP, -0x100000); break;
        case 2:  // the heap moved to just under the stack: within 0x40 of it is a collision
            p.op(LCTRL, 4);
            p.op(ADD_C, -0x40 - 4 * (int)(rnd() % 4) - (int)(rnd() % 3));
            p.op(SCTRL, 2);
            p.op(HEAP, 0);
            break;
        default: { const int k = 4 * (int)(rnd() % 20); p.op(HEAP, k); p.op(HEAP, -k); }
        }
        break;
    }
    case JUMP: case JZER: case JNZ: case JEQ: case JNEQ: case JLESS: case JLEQ: case JGRTR: case JGEQ: case JSLESS: case JSLEQ: case JSGRTR: case JSGEQ: {
        const int l = p.label();
        if (rnd() % 3 == 0) p.op(MOVE_PRI);
        p.to(o, l);
        p.op(CONST_PRI, 0x111);
        p.mark(l);
        break;
    }
    case JREL: {
        const int l = p.label();
        p.rel(JREL, l);
        p.op(CONST_PRI, 0x222);
        p.mark(l);
        break;
    }
    case JUMP_PRI: {
        const int l = p.label();
        p.to(CONST_PRI, l);
        p.op(JUMP_PRI);
        p.op(CONST_PRI, 0x444);
        p.mark(l);
        break;
    }
    case CALL: case CALL_PRI: case RET: case RETN: case PROC_: {
        // A function past the end of main: PROC_, reads its argument, returns by RETN (or RET, the caller then pops).
        const int f = p.label(), after = p.label();
        const bool by_ret = o == RET || rnd() % 3 == 0;
        p.op(PUSH_C, rv());
        p.op(PUSH_C, 4);
        if (o == CALL_PRI) { p.to(CONST_PRI, f); p.op(CALL_PRI); } else p.to(CALL, f);
        if (by_ret) p.op(STACK, 8);
        p.to(JUMP, after);
        p.mark(f);
        p.op(PROC_);
        p.op(LINE, 900);
        p.op(0);
        p.op(LOAD_S_PRI, 12);
        p.op(ADD_C, 1);
        p.op(by_ret ? RET : RETN);
        p.mark(after);
        break;
    }
    case MOVS: case CMPS: {
        static const int sizes[] = {0, 1, 2, 3, 4, 5, 8, 13, 16};
        p.op(CONST_PRI, valid_address());
        p.op(CONST_ALT, rnd() % 4 ? valid_address() : 4 * kResults + 32);
        p.op(o, sizes[rnd() % 9]);
        break;
    }
    case FILL: {
        static const int sizes[] = {0, 1, 3, 4, 5, 8, 16, 7};
        p.op(CONST_ALT, valid_address());
        p.op(o, sizes[rnd() % 8]);
        break;
    }
    case HALT:
        p.op(HALT, rnd() % 2 ? 0 : rv());
        break;
    case SYSREQ_PRI: case SYSREQ_C: {
        p.op(PUSH_C, rnd() % 2 ? valid_address() : checked_address(p));  // the built-in's GetAddr sees the heap gap and the stack's end too
        p.op(PUSH_C, rv());
        p.op(PUSH_C, 8);
        const int n = (int)(rnd() % p.natives.size());
        if (o == SYSREQ_PRI) { p.op(CONST_PRI, n); p.op(SYSREQ_PRI); } else p.op(SYSREQ_C, n);
        p.op(STACK, 12);
        break;
    }
    case HOSTGET: case HOSTSET:
        p.op(PUSH_C, rv());
        p.op(PUSH_C, (int)(rnd() % p.hostvars.size()));
        p.op(PUSH_C, 8);
        p.op(o, (int)(rnd() % 4));
        p.op(STACK, 12);
        break;
    case FILE_: {
        const char* names[] = {"a.sma", "behaviour.sma", "", "abc"};
        const char* n = names[rnd() % 4];
        p.op(FILE_, ((int)std::strlen(n) + 4) / 4 * 4 + 4);
        p.op(rv());
        p.name(n);
        break;
    }
    case SYMBOL: {
        const char* names[] = {"x", "counter", "", "abcdefg"};
        const char* n = names[rnd() % 4];
        p.op(SYMBOL, ((int)std::strlen(n) + 4) / 4 * 4 + 8);
        p.op(rv());
        p.op(rnd() % 2 ? (int)(rnd() % 256) : rv());
        p.name(n);
        break;
    }
    case SRANGE:  // two, so that relocation's last parameter is not the first's
        p.op(SRANGE, rv());
        p.op(rv());
        p.op(SRANGE, rv());
        p.op(rv());
        break;
    case LINE:
        p.op(LINE, rv());
        p.op(rv());
        break;
    case SWITCH: {
        const int table = p.label(), def = p.label(), done = p.label();
        const int n = (int)(rnd() % 4) - (rnd() % 5 == 0 ? 1 : 0);
        std::vector<int> cases, values;
        p.op(CONST_PRI, (int)(rnd() % 5) - 1);
        p.to(SWITCH, table);
        p.mark(table);
        p.op(CASETBL, n);
        p.word_to(def);
        for (int i = 0; i < n; i++) {
            cases.push_back(p.label());
            p.op(i);
            p.word_to(cases.back());
        }
        for (int i = 0; i < n; i++) {
            p.mark(cases[i]);
            p.op(CONST_ALT, 0x500 + i);
            p.to(JUMP, done);
        }
        p.mark(def);
        p.op(CONST_ALT, 0x5FF);
        p.mark(done);
        break;
    }
    case CASETBL:
        p.op(CASETBL, 0);  // the count is then taken for an opcode
        p.word_to(end_label);
        break;
    case SUSPEND:
        p.op(SUSPEND);
        break;
    default:  // instructions with no operand and no needs
        if (o >= EQ && o <= SGEQ && rnd() % 3 == 0) p.op(rnd() % 2 ? MOVE_ALT : MOVE_PRI);  // equal operands
        if ((o == SDIV || o == UDIV) && rnd() % 3 == 0) p.op(ZERO_ALT);  // division by zero
        if ((o == SDIV_ALT || o == UDIV_ALT) && rnd() % 3 == 0) p.op(ZERO_PRI);
        if ((o == SIGN_PRI || o == SIGN_ALT) && rnd() % 2) p.op(o == SIGN_PRI ? CONST_PRI : CONST_ALT, (int)(rnd() % 3) + 0x7F + 0x100 * (int)(rnd() % 3));
        p.op(o);
        break;
    }
}

static Prog make_program(const std::vector<int>& ops) {
    Prog p;
    p.data.resize(kCells);
    for (int i = 0; i < kCells; i++) p.data[i] = i < kResults ? 0 : (i < kResults + 8 ? valid_address() : (rnd() % 2 ? rv() : (int)(rnd() * 2654435761u)));
    p.hostvars = {"hvA", "hvB", "hvC"};
    const int alpha = p.label(), end = p.label(), beta = p.label();
    p.publics = {"alpha", "beta"};
    p.public_labels = {alpha, beta};
    p.mark(alpha);  // alpha is main
    p.op(PROC_);
    p.op(LINE, 1);
    p.op(0);
    p.op(CONST_PRI, rv());
    p.op(CONST_ALT, rv());
    for (int o : ops) body(p, o, end);
    p.mark(end);
    p.op(LINE, 2);
    p.op(0);
    p.op(STOR_PRI, 0);
    p.op(STOR_ALT, 4);
    p.op(LCTRL, 4);
    p.op(STOR_PRI, 8);
    p.op(LCTRL, 5);
    p.op(STOR_PRI, 12);
    p.op(LCTRL, 2);
    p.op(STOR_PRI, 16);
    p.op(LOAD_PRI, 0);
    p.op(rnd() % 3 == 0 ? RET : RETN);  // RET at the end resets the machine's stk, RETN does not
    p.mark(beta);  // a second public: returns its argument
    p.op(PROC_);
    p.op(LOAD_S_PRI, 12);
    p.op(RETN);
    return p;
}

static unsigned g_written;
static void run_program(const std::vector<unsigned char>& file, const char* what, int entry) {
    for (int mode = 0; mode < 4; mode++) {
        const int debug = mode & 1, step = (mode >> 1) & debug;
        if (mode == 2) continue;
        g_context = std::string(what) + (debug ? " debugging" : "") + (step ? " stepping" : "");
        g_answer_mode = 2 + (int)(rnd() % 2);
        g_debug_init_answer = debug ? 0 : (rnd() % 2 ? 21 : 5);
        g_preset_callback = rnd() % 4 == 0;
        set_step(step);
        Pair p;
        prepare(p, file, debug != 0 || rnd() % 3 == 0, -1);
        g_preset_callback = false;
        for (int side = 0; side < 2 && p.init[0] == 0 && p.init[1] == 0; side++) {
            On on(side);
            const int r = side == 0 ? at<int(__cdecl*)(void*, const void*, int)>(0x0007B160)(&p.s[side]->m_amx, g_hostvars, 3)
                                    : AtAmxRegisterHostVars(&p.s[side]->m_amx, g_hostvars, 3);
            L('hv', (uint32_t)r);
        }
        if (rnd() % 8 == 0) set_globals(0);  // as if no Init had run since: little-endian off
        compare_pair("prepared", p, file);
        const int params[2] = {rv(), rv()};
        run_call(p, file, entry, (int)(rnd() % 3), params, true);
        if (p.init[0] == 0) run_call(p, file, 1, 1, params, true);  // beta, on the same machine
        g_written++;
        release(p);
    }
    set_step(0);
    g_debug_init_answer = 21;
}

// Exec's own checks, by state set between Init and the call.
static void run_exec_checks() {
    for (int k = 0; k < 14; k++) {
        Prog p = make_program(k == 9 ? std::vector<int>(100, NOT) : std::vector<int>{ADD});
        if (k == 6) p.no_main = true;
        const std::vector<unsigned char> file = p.build();
        g_context = "exec check " + std::to_string(k);
        Pair q;
        prepare(q, file, false, -1);
        int index = -1;
        for (int side = 0; side < 2; side++) {
            AtAmx& a = q.s[side]->m_amx;
            switch (k) {
            case 0: a.m_callback = nullptr; break;
            case 1: a.m_flags &= ~kAmxFlagRelocated; break;
            case 2: a.m_stk = 0x7FFFFF00; break;
            case 3: a.m_hea = -4; break;
            case 4: a.m_hea = a.m_stk - 0x40; break;
            case 5: a.m_hea = a.m_stk - 0x4C; break;
            case 7: index = 2; break;
            case 8: index = -3; break;
            case 9: index = -2; break;  // the entry two before the first public: its address word is the header's cod
            case 10: a.m_flags |= kAmxFlagBrowse | kAmxFlagDebug; a.m_debug = d_debug; break;
            case 11: index = 0x7FFFFFFF; break;
            case 12: a.m_stk = a.m_stp + 8; break;  // past the machine's stp, short of the header's: not "stack low"
            case 13: a.m_hea = a.m_stk - 12 - 0x3E; break;  // after one parameter, 2 bytes inside the 0x40 margin
            }
        }
        if (k == 9) {
            // Make the code at offset cod a HALT, so the pseudo-entry ends cleanly.
            const int cod = ((const AtAmxHeader*)file.data())->m_cod;
            if (cod + 8 <= ((const AtAmxHeader*)file.data())->m_dat - ((const AtAmxHeader*)file.data())->m_cod) {
                for (int side = 0; side < 2; side++) {
                    *(int*)(q.prog[side] + 2 * cod) = HALT;
                    *(int*)(q.prog[side] + 2 * cod + 4) = 77;
                }
            } else {
                index = -1;
            }
        }
        const int params[1] = {5};
        run_call(q, file, index, k == 4 || k == 13 ? 1 : 0, params, true);
        release(q);
    }
}

static void run_written() {
    // Every opcode alone, many times with different values; then sequences of several.
    for (int round = 0; round < 24; round++) {
        for (int o = 1; o <= SUSPEND; o++) {
            if (o == HALT && round % 2) continue;
            Prog p = make_program({o});
            const std::vector<unsigned char> file = p.build();
            run_program(file, ("opcode " + std::to_string(o) + " round " + std::to_string(round)).c_str(), -1);
        }
    }
    static const int kSafe[] = {LOAD_PRI, LREF_S_ALT, LOAD_I, STOR_I, LIDX, XCHG, PUSH_R, STACK, HEAP, CALL, JSLEQ, SDIV, UDIV_ALT, CMPS, MOVS, FILL, SYSREQ_C,
                                HOSTSET, HOSTGET, SWITCH, LINE, SYMBOL, SUSPEND, NEG, SIGN_PRI, SCTRL, ALIGN_PRI, LODB_I, STRB_I, JUMP_PRI, CALL_PRI, RET};
    for (int round = 0; round < 120; round++) {
        std::vector<int> ops;
        const int n = 2 + (int)(rnd() % 6);
        for (int i = 0; i < n; i++) ops.push_back(rnd() % 2 ? kSafe[rnd() % (sizeof kSafe / sizeof kSafe[0])] : 1 + (int)(rnd() % (SUSPEND - 1)));
        Prog p = make_program(ops);
        const std::vector<unsigned char> file = p.build();
        run_program(file, ("sequence " + std::to_string(round)).c_str(), -1);
    }
    // Invalid instructions: relocation stops at the first, and Exec refuses an unrelocated program; or Exec meets one it relocated past.
    for (int bad : {0, 134, 135, 200, 0x100, -5, 0x7FFFFFFF}) {
        Prog p = make_program({ADD});
        p.code[6] = bad;  // in place of CONST_ALT and its operand
        p.code[7] = ZERO_PRI;
        run_program(p.build(), ("invalid " + std::to_string(bad)).c_str(), -1);
    }
    // Compact encoding, including a cell spread over six bytes.
    for (int round = 0; round < 25; round++) {
        Prog p = make_program({1 + (int)(rnd() % (SUSPEND - 1)), 1 + (int)(rnd() % (SUSPEND - 1))});
        p.compact = round % 5 == 0 ? 2 : 1;
        if (p.compact == 2) p.data[kResults + 9] = 0x1234567;
        run_program(p.build(), ("compact " + std::to_string(round)).c_str(), -1);
    }
    // Headers Init refuses or accepts at the edge.
    for (int k = 0; k < 12; k++) {
        Prog p = make_program({ADD});
        std::vector<unsigned char> f = p.build();
        AtAmxHeader* h = (AtAmxHeader*)f.data();
        switch (k) {
        case 9: h->m_stp = 0; break;
        case 10: h->m_stp = 0x7FFFFFFF; break;
        case 11: h->m_stp = -8; break;
        case 0: h->m_magic = 0xF1E1; break;
        case 1: h->m_defsize = 0x1D; break;
        case 2: h->m_amxVersion = 4; break;
        case 3: h->m_amxVersion = -1; break;
        case 4: h->m_flags |= 1; break;
        case 5: h->m_flags |= 2; break;
        case 6: h->m_amxVersion = 3; break;
        case 7: h->m_fileVersion = 9; break;
        case 8: h->m_flags |= 0x10; break;
        }
        run_program(f, ("header " + std::to_string(k)).c_str(), -1);
    }
    run_exec_checks();
}

#include "smallvm_system.h"

