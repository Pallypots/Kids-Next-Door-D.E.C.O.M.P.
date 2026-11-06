// Differential oracle for the heap stack, new and delete, the error routine and the memory system (Windows x86, local only). See oracle_hep.cpp and docs/verification.md.
//
//   oracle_mem <default.xbe> [--entries FILE] [--mutate]
//
// The original's calls out, to allocate and free its region, to the device start-up and to the C library's free, are replaced on its side and recorded, with the arguments it passes; the recovered
// side's seams record the same way. The two regions are at fixed addresses 16 MB apart, compared byte by byte as in oracle_hep.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtMemory.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const std::string& o, const std::string& r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: %s: original [%.400s], recovered [%.400s]\n", g_context.substr(0, 120).c_str(), what, o.c_str(), r.c_str());
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

// The original's globals.
static int& o_top() { return *(int*)(uintptr_t)0x002AF048; }
static uint32_t* o_stack() { return (uint32_t*)(uintptr_t)0x002C6068; }
static uint32_t& o_content() { return *(uint32_t*)(uintptr_t)0x002C60A8; }
static uint32_t& o_instance() { return *(uint32_t*)(uintptr_t)0x002C60AC; }
static uint32_t& o_default() { return *(uint32_t*)(uintptr_t)0x002C60B0; }
static uint32_t& o_fatal_ptr() { return *(uint32_t*)(uintptr_t)0x002B48E4; }

enum { kRegion = 0x200000 };
static char* g_region[2];
static const uint32_t kBase[2] = {0x20000000, 0x21000000};
static bool g_refuse;
static std::string g_log[2];
static char* g_object[2];  // The system under test on each side.

static void note(int side, const std::string& s) { g_log[side] += s + "; "; }

// Fatal: recorded, then stops, as it does in the game.
static __declspec(noinline) void stop(const char* message, int file, int line, int unused, int side) {
    note(side, std::string("fatal ") + message + " " + std::to_string(file) + " " + std::to_string(line) + " " + std::to_string(unused));
    *(volatile int*)0 = 0;
}
static void __cdecl o_fatal(const char* m, int a, int b, int c) { stop(m, a, b, c, 0); }
void AtFatal(const char* m, int a, int b, int c) { stop(m, a, b, c, 1); }

static void* __stdcall o_valloc(void* address, unsigned int size, unsigned int type, unsigned int protect) {
    note(0, "region " + std::to_string(size) + (address == nullptr && type == 0x3000 && protect == 4 ? "" : " with other arguments"));
    return g_refuse ? nullptr : g_region[0];
}
void* AtPlatformAllocateRegion(unsigned int size) {
    note(1, "region " + std::to_string(size));
    return g_refuse ? nullptr : g_region[1];
}
static int __stdcall o_vfree(void* region, unsigned int size, unsigned int type) {
    note(0, "free region +" + std::to_string((char*)region - g_region[0]) + (size == 0 && type == 0x8000 ? "" : " with other arguments"));
    return 1;
}
void AtPlatformFreeRegion(void* region) { note(1, "free region +" + std::to_string((char*)region - g_region[1])); }
static int __stdcall o_devices(int a, void* b) {
    note(0, a == 0 && b == nullptr ? "devices" : "devices with other arguments");
    return 0;
}
void AtPlatformInitDevices() { note(1, "devices"); }
static void __cdecl o_free(void* p) { note(0, "library free " + std::to_string((uintptr_t)p)); }
void AtLibraryFree(void* p) { note(1, "library free " + std::to_string((uintptr_t)p)); }

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

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnParams)(void*, void*, const void*);
typedef void*(__fastcall* FnDeleting)(void*, void*, unsigned int);
typedef void*(__cdecl* FnNew)(unsigned int);
typedef void(__cdecl* FnPtr)(void*);
typedef void(__cdecl* FnPop)();
typedef void*(__cdecl* FnCurrent)();
typedef void(__cdecl* FnRaise)(int, int, int, int);
typedef void*(__fastcall* FnCreate)(void*, void*, unsigned int, int);

// A pointer as something comparable between the sides: where it lies.
static long long where(int side, uint32_t v) {
    if (v == 0) return 0;
    const uint32_t obj = (uint32_t)(uintptr_t)g_object[side], reg = (uint32_t)(uintptr_t)g_region[side];
    if (obj && v >= obj && v < obj + sizeof(AtRegionMemorySystem)) return 0x100000000LL + (v - obj);
    if (v >= reg && v < reg + kRegion) return 0x200000000LL + (v - reg);
    return v;
}
static long long where(int side, const void* p) { return where(side, (uint32_t)(uintptr_t)p); }

static void same_globals(const char* what) {
    eq(what, o_top(), AtHeapStack::s_top);
    for (int i = 0; i < 16; i++) eq(what, where(0, o_stack()[i]), where(1, AtHeapStack::s_heaps[i]));
    eq(what, where(0, o_content()), where(1, AtHeapStack::s_contentHeap));
    eq(what, where(0, o_instance()), where(1, AtHeapManager::s_instance));
    eq(what, where(0, o_default()), where(1, AtIoTokenFile::s_heap));
}

static void same_region(const char* what) {
    g_checks++;
    const unsigned char* const o = (const unsigned char*)g_region[0];
    const unsigned char* const r = (const unsigned char*)g_region[1];
    for (size_t i = 0; i < kRegion; i++) {
        if (o[i] == r[i] || (o[i] == kBase[0] >> 24 && r[i] == kBase[1] >> 24)) continue;
        if (g_failures++ < 30) std::printf("DIFF %s: %s: region at +0x%X: original 0x%02X, recovered 0x%02X\n", g_context.substr(0, 120).c_str(), what, (unsigned)i, o[i], r[i]);
        return;
    }
}

static void same_system(const char* what) {
    AtRegionMemorySystem* const o = (AtRegionMemorySystem*)g_object[0];
    AtRegionMemorySystem* const r = (AtRegionMemorySystem*)g_object[1];
    eq(what, o->m_started, r->m_started);
    eq(what, where(0, o->m_manager.m_base), where(1, r->m_manager.m_base));
    eq(what, o->m_manager.m_size, r->m_manager.m_size);
    eq(what, where(0, o->m_manager.m_chunks), where(1, r->m_manager.m_chunks));
    eq(what, where(0, o->m_manager.m_override), where(1, r->m_manager.m_override));
    eq(what, o->m_manager.m_lastId, r->m_manager.m_lastId);
    for (int i = 0; i < AtHeapManager::kHeaps; i++) {
        const AtHeap& a = o->m_manager.m_heaps[i];
        const AtHeap& b = r->m_manager.m_heaps[i];
        eq(what, where(0, a.m_base), where(1, b.m_base));
        eq(what, where(0, a.m_free), where(1, b.m_free));
        eq(what, a.m_size, b.m_size);
        eq(what, a.m_used, b.m_used);
        eq(what, a.m_alignShift, b.m_alignShift);
        eq(what, a.m_allocShift, b.m_allocShift);
        eq(what, a.m_tag, b.m_tag);
        eq(what, a.m_equivalencyId, b.m_equivalencyId);
        eq(what, a.m_fixed, b.m_fixed);
    }
    eq(what, where(0, o->m_defaultHeap), where(1, r->m_defaultHeap));
    eq(what, where(0, o->m_region), where(1, r->m_region));
}

static void same_all(const char* what) {
    same_globals(what);
    same_system(what);
    same_region(what);
    eqs(what, g_log[0], g_log[1]);
}

static void clear_globals() {
    o_top() = AtHeapStack::s_top = -1;
    for (int i = 0; i < 16; i++) {
        o_stack()[i] = 0;
        AtHeapStack::s_heaps[i] = nullptr;
    }
    o_content() = 0;
    AtHeapStack::s_contentHeap = nullptr;
    o_instance() = 0;
    AtHeapManager::s_instance = nullptr;
    o_default() = 0;
    AtIoTokenFile::s_heap = nullptr;
    g_log[0].clear();
    g_log[1].clear();
}

static void fill_regions(unsigned int seed) {
    uint32_t x = seed * 2654435761u + 1;
    for (uint32_t i = 0; i < kRegion; i += 4) {
        x = x * 1664525u + 1013904223u;
        std::memcpy(g_region[0] + i, &x, 4);
        std::memcpy(g_region[1] + i, &x, 4);
    }
}

static void new_systems() {
    fill_regions(1);
    for (int s = 0; s < 2; s++) {
        if (!g_object[s]) g_object[s] = (char*)std::malloc(sizeof(AtRegionMemorySystem));
        std::memset(g_object[s], 0xCD, sizeof(AtRegionMemorySystem));
    }
    *(int*)(uintptr_t)0x003ADE40 = AtRegionMemorySystem::s_unknown3ADE40 = 0x11111111;
    *(short*)(uintptr_t)0x003ADE44 = AtRegionMemorySystem::s_unknown3ADE44 = 0x2222;
    at<Fn0>(0x000B3D80)(g_object[0], nullptr);
    new (g_object[1]) AtRegionMemorySystem;
    eq("unknown word", *(int*)(uintptr_t)0x003ADE40, AtRegionMemorySystem::s_unknown3ADE40);
    eq("unknown short", *(short*)(uintptr_t)0x003ADE44, AtRegionMemorySystem::s_unknown3ADE44);
    eq("cleared", AtRegionMemorySystem::s_unknown3ADE40 | AtRegionMemorySystem::s_unknown3ADE44, 0);
    eq("constructed started", ((AtRegionMemorySystem*)g_object[0])->m_started, ((AtRegionMemorySystem*)g_object[1])->m_started);
    eq("constructed region", (uintptr_t)((AtRegionMemorySystem*)g_object[0])->m_region, (uintptr_t)((AtRegionMemorySystem*)g_object[1])->m_region);
    // Not set by the constructor: still the fill.
    eq("default heap untouched", (uintptr_t)((AtRegionMemorySystem*)g_object[0])->m_defaultHeap, (uintptr_t)((AtRegionMemorySystem*)g_object[1])->m_defaultHeap);
}

static AtRegionMemorySystem* rec() { return (AtRegionMemorySystem*)g_object[1]; }

static void startup(const AtMemoryParams& p) {
    at<FnParams>(0x000C9B90)(g_object[0], nullptr, &p);
    rec()->Startup(&p);
}

static void run_stack() {
    g_context = "stack";
    clear_globals();
    eq("empty", (uintptr_t)at<FnCurrent>(0x00011280)(), (uintptr_t)AtCurrentHeap());
    for (uint32_t i = 1; i <= 16; i++) {
        at<FnPtr>(0x00011240)((void*)(uintptr_t)(i * 0x1010));
        AtPushHeap((void*)(uintptr_t)(i * 0x1010));
        same_globals("push");
        eq("current", (uintptr_t)at<FnCurrent>(0x00011280)(), (uintptr_t)AtCurrentHeap());
    }
    // Past the table: the depth grows, and nothing is written, as the word after the table shows.
    o_content() = 0x5A5A5A5A;
    AtHeapStack::s_contentHeap = (AtHeap*)(uintptr_t)0x5A5A5A5A;
    at<FnPtr>(0x00011240)((void*)(uintptr_t)0x7777);
    AtPushHeap((void*)(uintptr_t)0x7777);
    same_globals("past the table");
    eq("depth", AtHeapStack::s_top, 16);
    o_top() = AtHeapStack::s_top = 15;
    for (int i = 0; i < 16; i++) {
        at<FnPop>(0x00011260)();
        AtPopHeap();
        same_globals("pop");
        eq("current", (uintptr_t)at<FnCurrent>(0x00011280)(), (uintptr_t)AtCurrentHeap());
    }
    eq("empty again", AtHeapStack::s_top, -1);
    // A null on top is not the same as empty, and reads the same.
    at<FnPtr>(0x00011240)(nullptr);
    AtPushHeap(nullptr);
    eq("null on top", (uintptr_t)at<FnCurrent>(0x00011280)(), (uintptr_t)AtCurrentHeap());
    same_globals("null on top");
    at<FnPtr>(0x000112A0)((void*)(uintptr_t)0x1234);
    AtSetContentHeap((AtHeap*)(uintptr_t)0x1234);
    same_globals("content heap");
    eq("set", (uintptr_t)AtHeapStack::s_contentHeap, 0x1234);
}

static void run_raise() {
    for (int code = -9; code <= 3; code++) {
        g_context = "raise " + std::to_string(code);
        clear_globals();
        const bool a = guarded([&]() { at<FnRaise>(0x00011170)(code, 0x11, 0x22, 0x33); });
        const bool b = guarded([&]() { extern void AtRaise(int, int, int, int); AtRaise(code, 0x11, 0x22, 0x33); });
        eq("stopped", a, b);
        eq("stops", b, 1);
        eqs("what it said", g_log[0], g_log[1]);
    }
}

static void run_system() {
    static const AtMemoryParams kParams[] = {{0x100000, 0, 0}, {0x80000, 7, 0x4000}, {kRegion, 0x77, 0x40}, {0x10000, 0, 0x10000}, {0x1000, 0, 0x41}};
    for (const AtMemoryParams& p : kParams) {
        g_context = "system size " + std::to_string(p.m_regionSize) + " heap " + std::to_string(p.m_defaultHeapSize);
        clear_globals();
        new_systems();
        startup(p);
        same_all("started");
        eq("instance", where(1, AtHeapManager::s_instance), 0x100000000LL + 8);
        eq("depth", AtHeapStack::s_top, 0);

        // Allocating through the stack, and giving it back.
        void* po[8];
        void* pr[8];
        for (int i = 0; i < 8; i++) {
            po[i] = at<FnNew>(0x000111F0)(0x20u * (unsigned)(i + 1));
            pr[i] = AtHeapNew(0x20u * (unsigned)(i + 1));
            eq("new", where(0, po[i]), where(1, pr[i]));
        }
        same_all("allocated");
        // From whatever is on top.
        if (p.m_regionSize >= 0x10000 && p.m_defaultHeapSize != 0x10000) {
            void* const ho = at<FnCreate>(0x00011D00)(&((AtRegionMemorySystem*)g_object[0])->m_manager, nullptr, 0x400, 3);
            AtHeap* const hr = rec()->m_manager.CreateHeap(0x400, 3);
            at<FnPtr>(0x00011240)(ho);
            AtPushHeap(hr);
            void* const qo = at<FnNew>(0x000111F0)(0x30);
            void* const qr = AtHeapNew(0x30);
            eq("new from the top", where(0, qo), where(1, qr));
            eq("really from it", hr->Owns(qr), 1);
            same_all("from the top");
            at<FnPop>(0x00011260)();
            AtPopHeap();
        }
        for (int i : {3, 0, 7, 5}) {
            if (po[i] && pr[i]) {
                at<FnPtr>(0x00011210)(po[i]);
                AtHeapDelete(pr[i]);
            }
        }
        same_all("deleted");
        at<FnPtr>(0x00011210)(nullptr);
        AtHeapDelete(nullptr);
        same_all("delete null");
        // Nothing owns this, so it goes to the library.
        char outside[64] = {0};
        at<FnPtr>(0x00011210)(outside + 16);
        AtHeapDelete(outside + 16);
        same_all("delete outside");

        // Started again: shuts down first, so frees the region it then asks for again.
        startup(p);
        same_all("started again");
        // Shut down, twice: the second frees the same pointer.
        at<Fn0>(0x000B3E30)(g_object[0], nullptr);
        rec()->Shutdown();
        same_all("shut down");
        eq("not started", rec()->m_started, 0);
        at<Fn0>(0x000B3E30)(g_object[0], nullptr);
        rec()->Shutdown();
        same_all("shut down twice");

        // The destructor, and the deleting one without the delete: extra heaps on the stack show how many it pops.
        startup(p);
        for (int s = 0; s < 3; s++) {
            at<FnPtr>(0x00011240)((void*)(uintptr_t)0x4000);
            AtPushHeap((void*)(uintptr_t)0x4000);
        }
        const bool deleting = p.m_defaultHeapSize == 0;
        if (deleting) eq("returns this", (uintptr_t)at<FnDeleting>(0x000B3ED0)(g_object[0], nullptr, 0), (uintptr_t)g_object[0]);
        else at<Fn0>(0x000B3E60)(g_object[0], nullptr);
        rec()->~AtRegionMemorySystem();
        same_all("destroyed");
    }

    // Shutting down with no default heap set, or a null on the stack: nothing popped.
    for (int how = 0; how < 3; how++) {
        g_context = "pop conditions " + std::to_string(how);
        clear_globals();
        new_systems();
        const AtMemoryParams p = {0x10000, 0, 0x400};
        startup(p);
        if (how == 0) { o_default() = 0; AtIoTokenFile::s_heap = nullptr; }
        if (how == 1) { at<FnPtr>(0x00011240)(nullptr); AtPushHeap(nullptr); }
        if (how == 2) { at<FnPop>(0x00011260)(); AtPopHeap(); }
        at<Fn0>(0x000B3E30)(g_object[0], nullptr);
        rec()->Shutdown();
        same_globals("pop");
        eqs("log", g_log[0], g_log[1]);
    }

    // Refused memory, and a default heap larger than the region: both stop.
    {
        g_context = "region refused";
        clear_globals();
        new_systems();
        const AtMemoryParams p = {0x10000, 0, 0};
        g_refuse = true;
        const bool a = guarded([&]() { at<FnParams>(0x000B3DB0)(g_object[0], nullptr, &p); });
        const bool b = guarded([&]() { rec()->Initialize(&p); });
        g_refuse = false;
        eq("stopped", a, b);
        eq("stops", b, 1);
        eqs("log", g_log[0], g_log[1]);
        eq("region", where(0, ((AtRegionMemorySystem*)g_object[0])->m_region), where(1, rec()->m_region));
    }
    {
        g_context = "default heap too large";
        clear_globals();
        new_systems();
        const AtMemoryParams p = {0x1000, 0, 0x2000};
        const bool a = guarded([&]() { at<FnParams>(0x000B3DB0)(g_object[0], nullptr, &p); });
        const bool b = guarded([&]() { rec()->Initialize(&p); });
        eq("stopped", a, b);
        eq("stops", b, 1);
        eqs("log", g_log[0], g_log[1]);
    }
}

// The base class on its own: a double supplies slot 0, which is pure in the original, with no default heap.
class BaseDouble : public AtMemorySystem {
public:
    void Initialize(const AtMemoryParams*) override { m_defaultHeap = nullptr; note(1, "initialize"); }
};
static void __fastcall o_initialize(void* self, void*, const void*) {
    ((AtMemorySystem*)self)->m_defaultHeap = nullptr;
    note(0, "initialize");
}

static void run_base() {
    g_context = "base";
    clear_globals();
    fill_regions(2);
    alignas(8) static char o[sizeof(AtMemorySystem)];
    alignas(8) static char r[sizeof(AtMemorySystem)];
    std::memset(o, 0xCD, sizeof o);
    std::memset(r, 0xCD, sizeof r);
    g_object[0] = g_object[1] = nullptr;
    at<Fn0>(0x000C9AF0)(o, nullptr);
    new (r) BaseDouble;
    AtMemorySystem* const a = (AtMemorySystem*)o;
    AtMemorySystem* const b = (AtMemorySystem*)r;
    eq("started", a->m_started, b->m_started);
    eq("heap", (uintptr_t)a->m_defaultHeap, (uintptr_t)b->m_defaultHeap);
    eq("manager", a->m_manager.m_lastId, b->m_manager.m_lastId);

    // Its own vftable, with the real slots 2 and 3 and a slot 0 of this harness. Started without a default heap: nothing pushed.
    static const void* table[4] = {(const void*)o_initialize, (const void*)(uintptr_t)0x000C9BE0, (const void*)(uintptr_t)0x000C9B90, (const void*)(uintptr_t)0x000C9AC0};
    *(const void***)o = table;
    const AtMemoryParams p = {0, 0, 0};
    at<FnParams>(0x000C9B90)(o, nullptr, &p);
    b->Startup(&p);
    eq("depth", o_top(), AtHeapStack::s_top);
    eq("instance", (char*)(uintptr_t)o_instance() - o, (char*)AtHeapManager::s_instance - r);
    eq("default", (long long)o_default(), (long long)(uintptr_t)AtIoTokenFile::s_heap);
    eq("started", a->m_started, b->m_started);
    eq("is started", b->m_started, 1);
    // Twice: through slot 3.
    a->m_started = b->m_started = 1;
    at<FnParams>(0x000C9B90)(o, nullptr, &p);
    b->Startup(&p);
    eq("depth", o_top(), AtHeapStack::s_top);
    eqs("log", g_log[0], g_log[1]);
    // Slot 3 with something to pop, then the two destructors.
    o_default() = 0x1000;
    AtIoTokenFile::s_heap = (void*)(uintptr_t)0x1000;
    for (int i = 0; i < 4; i++) {
        at<FnPtr>(0x00011240)((void*)(uintptr_t)0x4000);
        AtPushHeap((void*)(uintptr_t)0x4000);
    }
    at<Fn0>(0x000C9AC0)(o, nullptr);
    b->AtMemorySystem::Shutdown();
    eq("depth", o_top(), AtHeapStack::s_top);
    eq("depth is less", AtHeapStack::s_top, 2);
    eq("started", a->m_started, b->m_started);
    eq("is stopped", b->m_started, 0);
    a->m_started = b->m_started = 1;
    at<Fn0>(0x000C9B10)(o, nullptr);
    b->~AtMemorySystem();
    eq("depth", o_top(), AtHeapStack::s_top);
    eq("depth is less", AtHeapStack::s_top, 1);
    eq("started", a->m_started, b->m_started);
    // The destructor with no default heap: nothing popped. And it empties the manager, whatever was in it.
    at<Fn0>(0x000C9AF0)(o, nullptr);
    new (r) BaseDouble;
    o_default() = 0;
    AtIoTokenFile::s_heap = nullptr;
    a->m_manager.m_lastId = b->m_manager.m_lastId = 9;
    a->m_manager.m_size = b->m_manager.m_size = 0x100;
    const int before = AtHeapStack::s_top;
    at<Fn0>(0x000C9B10)(o, nullptr);
    b->~AtMemorySystem();
    eq("depth", o_top(), AtHeapStack::s_top);
    eq("nothing popped", AtHeapStack::s_top, before);
    eq("last id", a->m_manager.m_lastId, b->m_manager.m_lastId);
    eq("size", a->m_manager.m_size, b->m_manager.m_size);
    eq("emptied", b->m_manager.m_lastId | (int)b->m_manager.m_size, 0);
    o_default() = 0x1000;
    AtIoTokenFile::s_heap = (void*)(uintptr_t)0x1000;
    // The deleting one, without the delete.
    at<Fn0>(0x000C9AF0)(o, nullptr);
    new (r) BaseDouble;
    eq("returns this", (uintptr_t)at<FnDeleting>(0x000C9BE0)(o, nullptr, 0), (uintptr_t)o);
    b->~AtMemorySystem();
    eq("depth", o_top(), AtHeapStack::s_top);
    eq("depth is less", AtHeapStack::s_top, 0);
}

static void run_globals() {
    g_object[0] = g_object[1] = nullptr;
    static const uint32_t kValues[][3] = {{0, 0, 0}, {0x1000, 0, 0x2000}, {0, 0x3000, 0x4000}, {0x1000, 0x3000, 0x4000}};
    for (const auto& v : kValues) {
        for (int depth = 0; depth < 2; depth++) {
            for (int nullTop = 0; nullTop < 2; nullTop++) {
                g_context = "globals " + std::to_string(v[1]) + " depth " + std::to_string(depth) + " null " + std::to_string(nullTop);
                clear_globals();
                if (depth) {
                    const uint32_t top = nullTop ? 0 : 0x4000;
                    at<FnPtr>(0x00011240)((void*)(uintptr_t)top);
                    AtPushHeap((void*)(uintptr_t)top);
                }
                AtMemoryGlobals a, b;
                std::memset(&a, 0xCD, sizeof a);
                std::memset(&b, 0xCD, sizeof b);
                a.m_manager = b.m_manager = (AtHeapManager*)(uintptr_t)v[0];
                a.m_defaultHeap = b.m_defaultHeap = (AtHeap*)(uintptr_t)v[1];
                a.m_fatal = b.m_fatal = (void*)(uintptr_t)v[2];
                at<Fn0>(0x001038E0)(&a, nullptr);
                b.Restore();
                same_globals("restored");
                eq("handler", o_fatal_ptr(), (uint32_t)(uintptr_t)AtMemoryGlobals::s_fatal);
                std::memset(&a, 0xCD, sizeof a);
                std::memset(&b, 0xCD, sizeof b);
                at<Fn0>(0x001038C0)(&a, nullptr);
                b.Save();
                eq("saved", std::memcmp(&a, &b, sizeof a), 0);
                eq("saved value", (uint32_t)(uintptr_t)b.m_fatal, v[2]);
            }
        }
    }
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
    if (pos.empty()) { std::printf("usage: oracle_mem <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x000853E7, (const void*)o_valloc);
    redirect(0x00085415, (const void*)o_vfree);
    redirect(0x0026D478, (const void*)o_devices);
    redirect(0x001256A3, (const void*)o_free);
    if (entries && !arm_entries(entries)) return 2;
    for (int s = 0; s < 2; s++) {
        g_region[s] = (char*)VirtualAlloc((void*)(uintptr_t)kBase[s], kRegion, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!g_region[s]) { std::printf("cannot place the region at 0x%08X\n", kBase[s]); return 2; }
    }
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    // The depth a game starts with, before anything here writes it.
    g_context = "start";
    eq("starting depth", o_top(), AtHeapStack::s_top);
    eq("is empty", AtHeapStack::s_top, -1);
    run_stack();
    run_raise();
    run_system();
    run_base();
    run_globals();
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
