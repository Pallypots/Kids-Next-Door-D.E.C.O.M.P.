// Differential oracle for the allocator (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_hep <default.xbe> [--seeds N] [--ops N] [--entries FILE] [--mutate]
//
// Each side has its own arena with the same bytes to begin with, driven by the same operations. The arenas are at fixed addresses exactly 16 MB apart, so alignment is the same on
// both, and a pointer differs only in its top byte, wherever it lies, even unaligned. After each batch the whole arena is compared byte by byte: the only difference allowed is
// the original's top byte beside the recovered one's. So headers, links, tags and even what is left in freed memory are all covered.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <string>
#include <vector>

#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 20) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

static volatile bool g_expect_crash;
static volatile int g_crashes;
static uint32_t g_resume_seh;
static uint32_t g_saved_ebx, g_saved_esi, g_saved_edi, g_saved_ebp, g_saved_esp, g_saved_eip;
static std::string g_fatal[2];

// A fatal stops the game. It stops here too, or the code carries on past the end of its tables.
static __declspec(noinline) void stop(const char* message, int side) {
    g_fatal[side] = message;
    *(volatile int*)0 = 0;
}
static void __cdecl o_fatal(const char* m, int, int, int) { stop(m, 0); }
void AtFatal(const char* m, int, int, int) { stop(m, 1); }

// What the library wants of these, none used here.
void AtRaise(int, int, int, int) {}
void AtPushHeap(void*) {}
void AtPopHeap() {}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (g_expect_crash) {
            g_expect_crash = false;
            g_crashes = g_crashes + 1;
            __writefsdword(0, g_resume_seh);
            e->ContextRecord->Ebx = g_saved_ebx;
            e->ContextRecord->Esi = g_saved_esi;
            e->ContextRecord->Edi = g_saved_edi;
            e->ContextRecord->Ebp = g_saved_ebp;
            e->ContextRecord->Esp = g_saved_esp;
            e->ContextRecord->Eip = g_saved_eip;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s] ebx %08X ecx %08X esi %08X esp %08X\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str(), (unsigned)e->ContextRecord->Ebx, (unsigned)e->ContextRecord->Ecx, (unsigned)e->ContextRecord->Esi, (unsigned)e->ContextRecord->Esp);
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// The original's functions, as methods of the original's object.
typedef void*(__fastcall* FnAlloc)(void*, void*, unsigned int);
typedef void*(__fastcall* FnAllocA)(void*, void*, unsigned int, unsigned int);
typedef void(__fastcall* FnP)(void*, void*, void*);
typedef int(__fastcall* FnOwns)(const void*, void*, const void*);
typedef unsigned int(__fastcall* FnU0)(const void*, void*);
typedef void(__fastcall* FnInit)(void*, void*, void*, unsigned int, int, int);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnMgrInit)(void*, void*, void*, unsigned int, int);  // ret 0xC: three arguments on the stack, one never read.
typedef void*(__fastcall* FnCreate)(void*, void*, unsigned int, int);
typedef void*(__fastcall* FnCreateAt)(void*, void*, void*, unsigned int, int);
typedef void*(__fastcall* FnMgrAlloc)(void*, void*, void*, unsigned int);
typedef int(__fastcall* FnMgrFree)(void*, void*, void*);
typedef void(__fastcall* FnEquiv)(void*, void*, void*, void*);
typedef void(__fastcall* FnReport)(void*, void*, char*, unsigned int);
typedef void(__fastcall* FnAppend)(void*, void*, char*, const char*, unsigned int*, unsigned int);
typedef int(__fastcall* FnId)(void*, void*);

enum { kArena = 0x60000, kFixed = 0x10000, kGuard = 0x400 };

struct Side {
    char* mem;      // Guards either side of the arena and of the fixed region.
    char* arena;
    char* fixed;
    AtHeapManager* mgr;
};
static Side g_s[2];

static const uint32_t kBase[2] = {0x20000000, 0x21000000};

static bool make_side(Side& s, int side) {
    s.mem = (char*)VirtualAlloc((void*)(uintptr_t)kBase[side], kArena + kFixed + 3 * kGuard, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!s.mem) { std::printf("cannot place the arena at 0x%08X\n", kBase[side]); return false; }
    s.arena = s.mem + kGuard;
    s.fixed = s.arena + kArena + kGuard;
    return true;
}

static void fill_same(unsigned int seed) {
    std::mt19937 g(seed);
    for (int i = 0; i < kArena + kFixed + 3 * kGuard; i += 4) {
        const uint32_t v = g();
        std::memcpy(g_s[0].mem + i, &v, 4);
        std::memcpy(g_s[1].mem + i, &v, 4);
    }
}

// Is a word a pointer into that side's memory? As an offset if so.
static bool offset_of(const Side& s, uint32_t v, uint32_t* out) {
    const uint32_t b = (uint32_t)(uintptr_t)s.mem;
    if (v >= b && v < b + kArena + kFixed + 3 * kGuard) { *out = v - b; return true; }
    return false;
}

static void same_ptr(const char* what, const void* a, const void* b) {
    g_checks++;
    const uint32_t x = (uint32_t)(uintptr_t)a, y = (uint32_t)(uintptr_t)b;
    if (!x || !y) { eq(what, x, y); return; }
    uint32_t p, q;
    const bool bp = offset_of(g_s[0], x, &p), bq = offset_of(g_s[1], y, &q);
    if (bp && bq) { eq(what, p, q); return; }
    // A heap of the manager, by slot.
    const int64_t hp = (const char*)a - (const char*)g_s[0].mgr, hq = (const char*)b - (const char*)g_s[1].mgr;
    eq(what, hp, hq);
}

static void same_memory(const char* what) {
    const size_t n = kArena + kFixed + 3 * kGuard;
    g_checks++;
    const unsigned char* const o = (const unsigned char*)g_s[0].mem;
    const unsigned char* const r = (const unsigned char*)g_s[1].mem;
    for (size_t i = 0; i < n; i++) {
        if (o[i] == r[i] || (o[i] == kBase[0] >> 24 && r[i] == kBase[1] >> 24)) continue;
        if (g_failures++ < 20) std::printf("DIFF %s: %s: memory at +0x%X: original 0x%02X, recovered 0x%02X\n", g_context.substr(0, 120).c_str(), what, (unsigned)i, o[i], r[i]);
        return;
    }
}

static void same_heap(const char* what, const AtHeap& a, const AtHeap& b) {
    same_ptr(what, a.m_base, b.m_base);
    same_ptr(what, a.m_free, b.m_free);
    eq(what, a.m_size, b.m_size);
    eq(what, a.m_used, b.m_used);
    eq(what, a.m_alignShift, b.m_alignShift);
    eq(what, a.m_allocShift, b.m_allocShift);
    eq(what, a.m_tag, b.m_tag);
    eq(what, a.m_equivalencyId, b.m_equivalencyId);
    eq(what, a.m_fixed, b.m_fixed);
}

static void same_all(const char* what) {
    AtHeapManager* const o = g_s[0].mgr;
    AtHeapManager* const r = g_s[1].mgr;
    same_ptr(what, o->m_base, r->m_base);
    eq(what, o->m_size, r->m_size);
    same_ptr(what, o->m_chunks, r->m_chunks);
    same_ptr(what, o->m_override, r->m_override);
    eq(what, o->m_lastId, r->m_lastId);
    for (int i = 0; i < AtHeapManager::kHeaps; i++) same_heap(what, o->m_heaps[i], r->m_heaps[i]);
    same_memory(what);
}

static void new_managers() {
    for (int s = 0; s < 2; s++) {
        if (g_s[s].mgr) { std::free(g_s[s].mgr); g_s[s].mgr = nullptr; }
        void* const m = std::malloc(sizeof(AtHeapManager));
        std::memset(m, 0xCD, sizeof(AtHeapManager));
        g_s[s].mgr = (AtHeapManager*)m;
    }
    at<Fn0>(0x00012140)(g_s[0].mgr, nullptr);
    new (g_s[1].mgr) AtHeapManager;
}

// A fatal on both sides, or neither: what each was doing, by a function that may not return.
// Calls fn(arg); a crash inside comes back here as a plain return, with the registers a caller relies on. RtlCaptureContext will not do: on x86 it captures its caller's frame, so a resume
// from it returns out of the capturing function with callee-saved registers as they stood.
static __declspec(naked) void __cdecl guarded_call(void (*fn)(void*), void* arg) {
    __asm {
        mov g_saved_ebx, ebx
        mov g_saved_esi, esi
        mov g_saved_edi, edi
        mov g_saved_ebp, ebp
        mov g_saved_esp, esp
        lea eax, come_back
        mov g_saved_eip, eax
        mov eax, [esp + 4]
        mov ecx, [esp + 8]
        push ecx
        call eax
        add esp, 4
    come_back:
        ret
    }
}

template <class F> static void trampoline(void* f) { (*(F*)f)(); }

// A fatal on both sides, or neither.
template <class A, class B> static void may_stop(A a, B b, const char* what) {
    g_fatal[0].clear();
    g_fatal[1].clear();
    const int before = g_crashes;
    g_resume_seh = __readfsdword(0);
    g_expect_crash = true;
    guarded_call(&trampoline<A>, &a);
    g_resume_seh = __readfsdword(0);
    g_expect_crash = true;
    guarded_call(&trampoline<B>, &b);
    g_expect_crash = false;
    eq(what, g_crashes - before == 2, g_fatal[0].empty() ? 0 : 1);
    eq(what, g_crashes - before == 0 || g_crashes - before == 2, 1);
    eqs(what, g_fatal[0].c_str(), g_fatal[1].c_str());
}

static void* heap_of(int side, int i) { return &g_s[side].mgr->m_heaps[i]; }

struct Live {
    char* p[2];
    unsigned int size;
    int heap;
};

static int g_every = 8;
static volatile int g_where;

static void run_fuzz(unsigned int seed, int ops) {
    g_context = "fuzz seed " + std::to_string(seed);
    std::mt19937 g(seed);
    fill_same(seed);
    new_managers();
    const unsigned int total = kArena - (g() % 2 ? 0 : 0x40 * (g() % 8));
    const int misalign = (g() % 3 == 0) ? (int)(g() % 0x40) : 0;
    at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena + misalign, total - 0x40, 0x77);
    g_s[1].mgr->Init(g_s[1].arena + misalign, total - 0x40, 0x77);
    same_all("init");

    std::vector<Live> live;
    int fixed_next = 0;
    for (int op = 0; op < ops; op++) {
        const int kind = (int)(g() % 100);
        std::vector<int> used;
        for (int i = 0; i < AtHeapManager::kHeaps; i++) if (g_s[1].mgr->m_heaps[i].m_size) used.push_back(i);
        const auto pick = [&]() { return used[g() % used.size()]; };
        if (kind < 6 && used.size() < AtHeapManager::kHeaps) {
            g_context = "create seed " + std::to_string(seed) + " op " + std::to_string(op);
            const unsigned int size = 0x100 + g() % 0x8000;
            const int shift = 2 + (int)(g() % 5);
            // May not fit: both stop, and the managers are as they were left, so go on.
            may_stop([&]() { at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, size, shift); }, [&]() { g_s[1].mgr->CreateHeap(size, shift); }, "create");
        } else if (kind < 8 && used.size() < AtHeapManager::kHeaps && fixed_next + 0x800 <= kFixed) {
            g_context = "create at seed " + std::to_string(seed) + " op " + std::to_string(op);
            const unsigned int size = 0x400 + g() % 0x400;
            const int shift = 2 + (int)(g() % 4);
            at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, g_s[0].fixed + fixed_next, size, shift);
            g_s[1].mgr->CreateHeapAt(g_s[1].fixed + fixed_next, size, shift);
            fixed_next += 0x800;
        } else if (used.empty()) {
            continue;
        } else if (kind < 50) {
            g_context = "allocate seed " + std::to_string(seed) + " op " + std::to_string(op);
            const int h = pick();
            const unsigned int r = g() % 16;
            const unsigned int size = r == 0 ? 0 : r < 4 ? g() % 16 : r < 13 ? g() % 400 : g() % 6000;
            Live l;
            l.size = size;
            l.heap = h;
            const int how = (int)(g() % 4);
            const unsigned int shift = 2 + g() % 7;
            g_context += " how " + std::to_string(how) + " heap " + std::to_string(h) + " size " + std::to_string(size) + " shift " + std::to_string(shift);
            g_where = 0;
            if (how == 0) l.p[0] = (char*)at<FnAllocA>(0x00016960)(heap_of(0, h), nullptr, size, shift);
            else if (how == 1) l.p[0] = (char*)at<FnAlloc>(0x00016520)(heap_of(0, h), nullptr, size);
            else l.p[0] = (char*)at<FnMgrAlloc>(0x00011EB0)(g_s[0].mgr, nullptr, heap_of(0, h), size);
            g_where = 1;
            if (how == 0) l.p[1] = (char*)g_s[1].mgr->m_heaps[h].AllocateAligned(size, shift);
            else if (how == 1) l.p[1] = (char*)g_s[1].mgr->m_heaps[h].Allocate(size);
            else l.p[1] = (char*)g_s[1].mgr->Allocate(&g_s[1].mgr->m_heaps[h], size);
            same_ptr("result", l.p[0], l.p[1]);
            if (l.p[0] && l.p[1]) {
                // Data, the same on both sides: it stays behind in freed memory, where headers get written over it.
                const uint32_t v = g();
                for (unsigned int i = 0; i + 4 <= size; i += 4) {
                    const uint32_t w = v * (i + 1);
                    std::memcpy(l.p[0] + i, &w, 4);
                    std::memcpy(l.p[1] + i, &w, 4);
                }
                live.push_back(l);
            }
        } else if (kind < 80) {
            if (live.empty()) continue;
            g_context = "free seed " + std::to_string(seed) + " op " + std::to_string(op);
            const size_t i = g() % live.size();
            int a, b;
            a = at<FnMgrFree>(0x00011F40)(g_s[0].mgr, nullptr, live[i].p[0]);
            b = g_s[1].mgr->Free(live[i].p[1]);
            eq("free result", a, b);
            live.erase(live.begin() + (long)i);
        } else if (kind < 84) {
            g_context = "owns seed " + std::to_string(seed) + " op " + std::to_string(op);
            const int h = pick();
            for (int k = 0; k < 4 && !live.empty(); k++) {
                const Live& l = live[g() % live.size()];
                eq("owns", at<FnOwns>(0x00016810)(heap_of(0, h), nullptr, l.p[0]), g_s[1].mgr->m_heaps[h].Owns(l.p[1]));
            }
            // Outside, at the bounds, and just inside on either side.
            const AtHeap& hr = g_s[1].mgr->m_heaps[h];
            const AtHeap& ho = g_s[0].mgr->m_heaps[h];
            for (const int64_t d : {(int64_t)-1, (int64_t)0, (int64_t)hr.m_size, (int64_t)hr.m_size - 1}) {
                eq("owns edge", at<FnOwns>(0x00016810)(heap_of(0, h), nullptr, ho.m_base + d), hr.Owns(hr.m_base + d));
            }
            eq("largest", at<FnU0>(0x00016880)(heap_of(0, h), nullptr), hr.LargestFree());
            eq("count", at<FnU0>(0x000168B0)(heap_of(0, h), nullptr), hr.FreeBlockCount());
        } else if (kind < 88 && used.size() >= 1) {
            g_context = "equivalent seed " + std::to_string(seed) + " op " + std::to_string(op);
            const int a = pick(), b = pick();
            at<FnEquiv>(0x00011FC0)(g_s[0].mgr, nullptr, heap_of(0, a), heap_of(0, b));
            g_s[1].mgr->MakeEquivalent(&g_s[1].mgr->m_heaps[a], &g_s[1].mgr->m_heaps[b]);
        } else if (kind < 92) {
            g_context = "override seed " + std::to_string(seed) + " op " + std::to_string(op);
            if (g() % 2) {
                g_s[0].mgr->m_override = g_s[1].mgr->m_override = nullptr;
            } else {
                const int h = pick();
                g_s[0].mgr->m_override = &g_s[0].mgr->m_heaps[h];
                g_s[1].mgr->m_override = &g_s[1].mgr->m_heaps[h];
            }
        } else if (kind < 95) {
            g_context = "default heap seed " + std::to_string(seed) + " op " + std::to_string(op);
            const int h = pick();
            *(void**)(uintptr_t)0x002C60B0 = heap_of(0, h);
            AtIoTokenFile::s_heap = heap_of(1, h);
            Live l;
            l.size = g() % 200;
            l.heap = h;
            l.p[0] = (char*)at<FnMgrAlloc>(0x00011EB0)(g_s[0].mgr, nullptr, heap_of(0, h), l.size);
            l.p[1] = (char*)g_s[1].mgr->Allocate(&g_s[1].mgr->m_heaps[h], l.size);
            same_ptr("default result", l.p[0], l.p[1]);
            if (l.p[0] && l.p[1]) live.push_back(l);
            *(void**)(uintptr_t)0x002C60B0 = nullptr;
            AtIoTokenFile::s_heap = nullptr;
        } else if (kind < 98) {
            g_context = "destroy seed " + std::to_string(seed) + " op " + std::to_string(op);
            const int h = pick();
            // What was in it is gone, and an override must not point at an empty heap.
            for (size_t i = live.size(); i-- > 0;) {
                if (live[i].p[1] >= g_s[1].mgr->m_heaps[h].m_base && live[i].p[1] < g_s[1].mgr->m_heaps[h].m_base + g_s[1].mgr->m_heaps[h].m_size) live.erase(live.begin() + (long)i);
            }
            if (g_s[1].mgr->m_override == &g_s[1].mgr->m_heaps[h]) g_s[0].mgr->m_override = g_s[1].mgr->m_override = nullptr;
            at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, heap_of(0, h));
            g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[h]);
        } else {
            g_context = "next id seed " + std::to_string(seed) + " op " + std::to_string(op);
            eq("id", at<FnId>(0x00011CC0)(g_s[0].mgr, nullptr), g_s[1].mgr->NextId());
        }
        if (op % g_every == 0) same_all("batch");
    }
    g_context = "end seed " + std::to_string(seed);
    same_all("end");
    // Every owner of every pointer still alive, then the report, with addresses taken out: the bases differ.
    for (const Live& l : live) {
        for (int h = 0; h < AtHeapManager::kHeaps; h++) {
            if (g_s[1].mgr->m_heaps[h].m_size) eq("owner", at<FnOwns>(0x00016810)(heap_of(0, h), nullptr, l.p[0]), g_s[1].mgr->m_heaps[h].Owns(l.p[1]));
        }
    }
    char a[8192], b[8192];
    at<FnReport>(0x000120A0)(g_s[0].mgr, nullptr, a, sizeof a);
    g_s[1].mgr->Report(b, sizeof b);
    const auto strip = [](char* s) { for (char* p = s; (p = std::strstr(p, ":0x")); p += 11) std::memset(p + 3, '_', 8); };
    strip(a);
    strip(b);
    eqs("report", a, b);
    // ResetAll last.
    g_context = "reset all seed " + std::to_string(seed);
    at<Fn0>(0x00011C70)(g_s[0].mgr, nullptr);
    g_s[1].mgr->ResetAll();
    same_all("reset all");
}

static void run_parts() {
    // Heap objects alone, on dirty memory.
    g_context = "heap constructor";
    {
        unsigned char x[sizeof(AtHeap)], y[sizeof(AtHeap)];
        std::memset(x, 0xCD, sizeof x);
        std::memset(y, 0xCD, sizeof y);
        at<Fn0>(0x000168D0)(x, nullptr);
        new (y) AtHeap;
        eq("constructor", std::memcmp(x, y, sizeof x), 0);
        std::memset(x, 0xCD, sizeof x);
        std::memset(y, 0xCD, sizeof y);
        at<Fn0>(0x00016900)(x, nullptr);
        ((AtHeap*)y)->~AtHeap();
        eq("destructor", std::memcmp(x, y, sizeof x), 0);
    }
    // A heap of its own: every split edge and each order of merging, by size.
    g_context = "single heap";
    for (int shift : {0, 1, 2, 3, 4, 6}) {
        for (unsigned int first : {0u, 1u, 7u, 8u, 9u, 15u, 16u, 17u, 100u}) {
            fill_same(first + 77u * (unsigned)shift);
            g_context = "single heap shift " + std::to_string(shift) + " first " + std::to_string(first);
            AtHeap o, r;
            at<FnInit>(0x00016930)(&o, nullptr, g_s[0].arena, 0x1000, 5, shift);
            r.Init(g_s[1].arena, 0x1000, 5, shift);
            same_heap("init", o, r);
            same_memory("init");
            // Three in a row, then each order of freeing: both neighbours free, one, neither.
            static const int orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
            for (const auto& order : orders) {
                void* po[4];
                void* pr[4];
                for (int i = 0; i < 4; i++) {
                    po[i] = at<FnAlloc>(0x00016520)(&o, nullptr, first + (unsigned)i);
                    pr[i] = r.Allocate(first + (unsigned)i);
                    same_ptr("allocate", po[i], pr[i]);
                }
                same_heap("three", o, r);
                same_memory("three");
                for (int i : order) {
                    at<FnP>(0x000166C0)(&o, nullptr, po[i]);
                    r.Free(pr[i]);
                    same_heap("free", o, r);
                    same_memory("free");
                    eq("fragments", at<FnU0>(0x000168B0)(&o, nullptr), r.FreeBlockCount());
                    eq("largest", at<FnU0>(0x00016880)(&o, nullptr), r.LargestFree());
                }
                at<FnP>(0x000166C0)(&o, nullptr, po[3]);
                r.Free(pr[3]);
                same_heap("empty again", o, r);
                same_memory("empty again");
                eq("all back", r.m_used, 0);
            }
            // Filling exactly, and refused.
            for (unsigned int ask : {0x1000u, 0xFFFu, 0xFF8u, 0xFF0u, 0xFE8u, 0xFE0u, 0x2000u}) {
                void* const a = at<FnAlloc>(0x00016520)(&o, nullptr, ask);
                void* const b = r.Allocate(ask);
                same_ptr("big", a, b);
                same_heap("big", o, r);
                same_memory("big");
                if (a && b) {
                    at<FnP>(0x000166C0)(&o, nullptr, a);
                    r.Free(b);
                    same_heap("big free", o, r);
                    same_memory("big free");
                }
            }
            // Wider than the heap, narrower, and equal.
            for (unsigned int want : {0u, 1u, 2u, 3u, 5u, 8u, 11u}) {
                void* const a = at<FnAllocA>(0x00016960)(&o, nullptr, first, want);
                void* const b = r.AllocateAligned(first, want);
                same_ptr("aligned", a, b);
                eq("shift kept", o.m_allocShift, r.m_allocShift);
                same_heap("aligned", o, r);
                same_memory("aligned");
                if (a && b) {
                    eq("owns", at<FnOwns>(0x00016810)(&o, nullptr, a), r.Owns(b));
                    at<FnP>(0x000166C0)(&o, nullptr, a);
                    r.Free(b);
                    same_heap("aligned free", o, r);
                    same_memory("aligned free");
                }
            }
            at<Fn0>(0x000164C0)(&o, nullptr);
            r.Reset();
            same_heap("reset", o, r);
            same_memory("reset");
        }
    }
    // Owns: a marker whose distance is odd or too far, and the wrong tag.
    g_context = "owns markers";
    {
        fill_same(9);
        AtHeap o, r;
        at<FnInit>(0x00016930)(&o, nullptr, g_s[0].arena, 0x4000, 5, 2);
        r.Init(g_s[1].arena, 0x4000, 5, 2);
        for (uint32_t word : {0x01000000u, 0x01000001u, 0x01000002u, 0x01000004u, 0x01000800u, 0x01000804u, 0x01FFFFFCu, 0x14000000u, 0x18000000u, 0u}) {
            if ((word & 0xFFFFFF) > 0x1000) continue;  // Its back reference would be outside the arena, and Owns refuses it first in any case: covered by 0x01000804.
            for (int s = 0; s < 2; s++) std::memcpy(g_s[s].arena + 0x2000 - 8, &word, 4);
            // What the marker points back to: tag 5, and something else.
            const uint32_t tag5 = 5u << 26, tag6 = 6u << 26;
            for (uint32_t back : {tag5, tag6}) {
                for (int s = 0; s < 2; s++) {
                    std::memcpy(g_s[s].arena + 0x2000 - 8 - (word & 0xFFFFFF), &back, 4);
                }
                eq("owns marker", at<FnOwns>(0x00016810)(&o, nullptr, g_s[0].arena + 0x2000), r.Owns(g_s[1].arena + 0x2000));
            }
        }
    }
    // Sizes of 16 MB and over keep 24 bits.
    g_context = "wide size";
    {
        fill_same(11);
        AtHeap o, r;
        at<FnInit>(0x00016930)(&o, nullptr, g_s[0].arena, 0x1000100, 3, 2);
        r.Init(g_s[1].arena, 0x1000100, 3, 2);
        same_heap("wide", o, r);
        eq("largest", at<FnU0>(0x00016880)(&o, nullptr), r.LargestFree());
        eq("the size is cut", r.LargestFree(), 0x100);
        same_memory("wide");
    }
    // The manager: its limits, each a fatal.
    g_context = "manager limits";
    {
        fill_same(12);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x8000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x8000, 0x77);
        same_all("init");
        // Too large for any chunk.
        may_stop([]() { at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x9000, 2); }, []() { g_s[1].mgr->CreateHeap(0x9000, 2); }, "too large");
        eq("stopped", g_fatal[1].empty(), 0);
        // Exactly the rest, then none left.
        for (unsigned int size : {0x40u, 0x41u, 0x7Fu, 0x80u}) {
            at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, size, 2);
            g_s[1].mgr->CreateHeap(size, 2);
            same_all("small heaps");
        }
        // A rest of up to 0x40 goes with the heap.
        const unsigned int left = (unsigned int)g_s[1].mgr->m_chunks->m_size;
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, left - 0x40, 2);
        g_s[1].mgr->CreateHeap(left - 0x40, 2);
        same_all("kept the rest");
        eq("no chunks left", g_s[1].mgr->m_chunks == nullptr, 1);
        may_stop([]() { at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x40, 2); }, []() { g_s[1].mgr->CreateHeap(0x40, 2); }, "no chunks");
        eq("stopped", g_fatal[1].empty(), 0);
        // Fill the slots, by both ways.
        for (int i = 5; i < AtHeapManager::kHeaps; i++) {
            at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, g_s[0].fixed + i * 0x100, 0x100, 2);
            g_s[1].mgr->CreateHeapAt(g_s[1].fixed + i * 0x100, 0x100, 2);
        }
        same_all("full");
        may_stop([]() { at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x40, 2); }, []() { g_s[1].mgr->CreateHeap(0x40, 2); }, "no slot");
        eq("stopped", g_fatal[1].empty(), 0);
        may_stop([]() { at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, g_s[0].fixed, 0x100, 2); }, []() { g_s[1].mgr->CreateHeapAt(g_s[1].fixed, 0x100, 2); }, "no slot at");
        eq("stopped", g_fatal[1].empty(), 0);
        // Free memory no heap owns.
        char outside[64] = {0};
        eq("free outside", at<FnMgrFree>(0x00011F40)(g_s[0].mgr, nullptr, outside + 16), g_s[1].mgr->Free(outside + 16));
    }
    // Destroying: chunks joining on each side, in each order. A fixed heap gives nothing back.
    g_context = "destroy orders";
    static const int orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (const auto& order : orders) {
        fill_same(13);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x8000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x8000, 0x77);
        void* ho[3];
        for (int i = 0; i < 3; i++) {
            ho[i] = at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
            g_s[1].mgr->CreateHeap(0x400, 2);
        }
        same_all("three heaps");
        for (int i : order) {
            at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, ho[i]);
            g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[i]);
            same_all("destroyed");
        }
        eq("one chunk again", g_s[1].mgr->m_chunks->m_next == nullptr, 1);
        // A hole between two, left open: the chunk goes into the middle of the list.
        for (int i = 0; i < 4; i++) {
            at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
            g_s[1].mgr->CreateHeap(0x400, 2);
        }
        static const int holes[2] = {0, 2};
        for (int i : holes) {
            at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, heap_of(0, i));
            g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[i]);
            same_all("holes");
        }
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
        g_s[1].mgr->CreateHeap(0x400, 2);
        same_all("into a later chunk");
        // A chunk taken from the middle of the list.
        at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, heap_of(0, 1));
        g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[1]);
        same_all("holes again");
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x800, 2);
        g_s[1].mgr->CreateHeap(0x800, 2);
        same_all("from the middle");
    }
    {
        g_context = "destroy fixed";
        fill_same(14);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x4000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x4000, 0x77);
        // A fixed heap outside the region, and one inside it.
        at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, g_s[0].fixed, 0x400, 2);
        g_s[1].mgr->CreateHeapAt(g_s[1].fixed, 0x400, 2);
        at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, g_s[0].arena + 0x1000, 0x400, 2);
        g_s[1].mgr->CreateHeapAt(g_s[1].arena + 0x1000, 0x400, 2);
        same_all("fixed");
        for (int i = 0; i < 2; i++) {
            at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, heap_of(0, i));
            g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[i]);
            same_all("fixed destroyed");
        }
        // A carved heap whose base someone moved outside the region gives nothing back either.
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
        g_s[1].mgr->CreateHeap(0x400, 2);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[0].m_base = g_s[s].fixed;
        at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, heap_of(0, 0));
        g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[0]);
        same_all("moved base");
    }
    // Which is asked first: a fixed heap inside a carved one, with a pointer both claim.
    {
        g_context = "fixed asked first";
        fill_same(15);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x8000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x8000, 0x77);
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x2000, 2);
        g_s[1].mgr->CreateHeap(0x2000, 2);
        char* const po = (char*)at<FnAlloc>(0x00016520)(heap_of(0, 0), nullptr, 0x100);
        char* const pr = (char*)g_s[1].mgr->m_heaps[0].Allocate(0x100);
        at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, po, 0x80, 2);
        g_s[1].mgr->CreateHeapAt(pr, 0x80, 2);
        same_all("nested");
        char* const qo = (char*)at<FnAlloc>(0x00016520)(heap_of(0, 1), nullptr, 8);
        char* const qr = (char*)g_s[1].mgr->m_heaps[1].Allocate(8);
        // Make the carved heap claim it as well: same tag on the fixed heap's block.
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[0].m_tag = 1;
        eq("result", at<FnMgrFree>(0x00011F40)(g_s[0].mgr, nullptr, qo), g_s[1].mgr->Free(qr));
        same_all("freed by the fixed one");
    }
    // Init: a second call forgets; a base that is not aligned.
    {
        g_context = "second init";
        fill_same(16);
        new_managers();
        for (int misalign : {0, 1, 0x3F}) {
            at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena + misalign, 0x4000, 0x77);
            g_s[1].mgr->Init(g_s[1].arena + misalign, 0x4000, 0x77);
            same_all("init");
            at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
            g_s[1].mgr->CreateHeap(0x400, 2);
            same_all("heap");
        }
    }
    // Allocate's order of asking, with the first heap full.
    {
        g_context = "equivalents";
        fill_same(17);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x10000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x10000, 0x77);
        for (int i = 0; i < 4; i++) {
            at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x200, i == 2 ? 6 : 2);
            g_s[1].mgr->CreateHeap(0x200, i == 2 ? 6 : 2);
        }
        at<FnEquiv>(0x00011FC0)(g_s[0].mgr, nullptr, heap_of(0, 0), heap_of(0, 1));
        g_s[1].mgr->MakeEquivalent(&g_s[1].mgr->m_heaps[0], &g_s[1].mgr->m_heaps[1]);
        at<FnEquiv>(0x00011FC0)(g_s[0].mgr, nullptr, heap_of(0, 1), heap_of(0, 2));
        g_s[1].mgr->MakeEquivalent(&g_s[1].mgr->m_heaps[1], &g_s[1].mgr->m_heaps[2]);
        same_all("group");
        at<FnEquiv>(0x00011FC0)(g_s[0].mgr, nullptr, heap_of(0, 1), heap_of(0, 1));
        g_s[1].mgr->MakeEquivalent(&g_s[1].mgr->m_heaps[1], &g_s[1].mgr->m_heaps[1]);
        same_all("group with itself");
        for (int i = 0; i < 16; i++) {
            void* const a = at<FnMgrAlloc>(0x00011EB0)(g_s[0].mgr, nullptr, heap_of(0, i % 3), 0x90);
            void* const b = g_s[1].mgr->Allocate(&g_s[1].mgr->m_heaps[i % 3], 0x90);
            same_ptr("spill", a, b);
            same_all("spill");
        }
        // The same through an override, whose group is what is searched; the asked heap's alignment is used.
        at<Fn0>(0x00011C70)(g_s[0].mgr, nullptr);
        g_s[1].mgr->ResetAll();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x10000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x10000, 0x77);
        for (int i = 0; i < 4; i++) {
            at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x200, i == 3 ? 6 : 2);
            g_s[1].mgr->CreateHeap(0x200, i == 3 ? 6 : 2);
        }
        at<FnEquiv>(0x00011FC0)(g_s[0].mgr, nullptr, heap_of(0, 0), heap_of(0, 1));
        g_s[1].mgr->MakeEquivalent(&g_s[1].mgr->m_heaps[0], &g_s[1].mgr->m_heaps[1]);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_override = &g_s[s].mgr->m_heaps[0];
        for (int i = 0; i < 10; i++) {
            const int asked = i % 2 ? 3 : 2;
            void* const a = at<FnMgrAlloc>(0x00011EB0)(g_s[0].mgr, nullptr, heap_of(0, asked), 0x60);
            void* const b = g_s[1].mgr->Allocate(&g_s[1].mgr->m_heaps[asked], 0x60);
            same_ptr("override spill", a, b);
            same_all("override spill");
        }
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_override = nullptr;
    }
    // Append and Report: every edge of the capacity.
    {
        g_context = "append";
        for (unsigned int capacity : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 8u, 9u, 16u}) {
            for (unsigned int start : {0u, 2u, 5u, 20u}) {
                for (const char* text : {"", "a", "abc", "abcd", "abcdefgh"}) {
                    char a[64], b[64];
                    std::memset(a, 'x', sizeof a);
                    std::memset(b, 'x', sizeof b);
                    a[63] = b[63] = 0;
                    const unsigned int prefix = start < 60 ? start : 60;
                    std::memset(a, 'p', prefix);
                    std::memset(b, 'p', prefix);
                    a[prefix] = b[prefix] = 0;
                    unsigned int la = start, lb = start;
                    at<FnAppend>(0x00012010)(nullptr, nullptr, a, text, &la, capacity);
                    g_s[1].mgr->Append(b, text, &lb, capacity);
                    eq("length", la, lb);
                    eq("bytes", std::memcmp(a, b, sizeof a), 0);
                }
            }
        }
        g_context = "report";
        fill_same(18);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, 0x10000, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, 0x10000, 0x77);
        for (int i = 0; i < 3; i++) {
            at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2 + i);
            g_s[1].mgr->CreateHeap(0x400, 2 + i);
            at<FnAlloc>(0x00016520)(heap_of(0, i), nullptr, 0x10);
            g_s[1].mgr->m_heaps[i].Allocate(0x10);
        }
        for (unsigned int capacity : {4096u, 200u, 20u, 2u}) {
            char a[4096], b[4096];
            std::memset(a, 'x', sizeof a);
            std::memset(b, 'x', sizeof b);
            at<FnReport>(0x000120A0)(g_s[0].mgr, nullptr, a, capacity);
            g_s[1].mgr->Report(b, capacity);
            // Only the first line can tell the addresses apart: take the hex out.
            const auto strip = [](char* s) { for (char* p = s; (p = std::strstr(p, ":0x")); p += 3) { char* q = p + 3; while (*q && *q != ' ') *q++ = '_'; } };
            a[capacity - 1 < 4095 ? capacity - 1 : 4095] = b[capacity - 1 < 4095 ? capacity - 1 : 4095] = 0;
            strip(a);
            strip(b);
            eqs("report", a, b);
        }
    }
}

// States the game's own calls do not reach, set by hand on both sides: each tells apart code that ordinary use cannot.
static void run_states() {
    const auto start = [](unsigned int seed, unsigned int size) {
        fill_same(seed);
        new_managers();
        at<FnMgrInit>(0x00012180)(g_s[0].mgr, nullptr, g_s[0].arena, size, 0x77);
        g_s[1].mgr->Init(g_s[1].arena, size, 0x77);
    };
    // Reset with something allocated.
    {
        g_context = "reset while used";
        fill_same(30);
        AtHeap o, r;
        at<FnInit>(0x00016930)(&o, nullptr, g_s[0].arena, 0x1000, 5, 2);
        r.Init(g_s[1].arena, 0x1000, 5, 2);
        at<FnAlloc>(0x00016520)(&o, nullptr, 0x40);
        r.Allocate(0x40);
        eq("used", r.m_used != 0, 1);
        at<Fn0>(0x000164C0)(&o, nullptr);
        r.Reset();
        same_heap("reset", o, r);
        same_memory("reset");
    }
    // The tag changing between allocations: the block taken gets the current one.
    {
        g_context = "tag changed";
        fill_same(31);
        AtHeap o, r;
        at<FnInit>(0x00016930)(&o, nullptr, g_s[0].arena, 0x1000, 5, 2);
        r.Init(g_s[1].arena, 0x1000, 5, 2);
        o.m_tag = r.m_tag = 9;
        same_ptr("allocate", at<FnAlloc>(0x00016520)(&o, nullptr, 0x40), r.Allocate(0x40));
        same_memory("tag");
    }
    // Merging past 24 bits. Headers written by hand, at the two ends of 18 MB of untouched memory, so little of it is ever committed in practice.
    {
        g_context = "merge past 24 bits";
        const unsigned int half = 0x900000;
        char* const base[2] = {(char*)VirtualAlloc(nullptr, 2 * half + 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE), (char*)VirtualAlloc(nullptr, 2 * half + 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)};
        eq("reserved", base[0] != nullptr && base[1] != nullptr, 1);
        AtHeap h[2];
        for (int s = 0; s < 2; s++) {
            AtHeapBlock* const first = (AtHeapBlock*)base[s];
            AtHeapBlock* const second = (AtHeapBlock*)(base[s] + half);
            first->m_word = half | AtHeapBlock::kUsed | (5u << AtHeapBlock::kTagShift);
            first->m_prev = nullptr;
            second->m_word = half | (5u << AtHeapBlock::kTagShift);
            second->m_prev = first;
            second->m_nextFree = nullptr;
            second->m_prevFree = nullptr;
            h[s].m_base = base[s];
            h[s].m_free = second;
            h[s].m_size = 2 * half;
            h[s].m_used = half;
            h[s].m_alignShift = 2;
            h[s].m_allocShift = 2;
            h[s].m_tag = 5;
        }
        at<FnP>(0x000166C0)(&h[0], nullptr, base[0] + 8);
        h[1].Free(base[1] + 8);
        eq("word", *(uint32_t*)base[0], *(uint32_t*)base[1]);
        eq("cut", *(uint32_t*)base[1] & AtHeapBlock::kBackOffset, 0);
        eq("used", h[0].m_used, h[1].m_used);
        eq("list", (char*)h[0].m_free - base[0], (char*)h[1].m_free - base[1]);
        for (int s = 0; s < 2; s++) {
            h[s].m_base = nullptr;
            h[s].m_free = nullptr;
            VirtualFree(base[s], 0, MEM_RELEASE);
        }
    }
    // An id already taken by a slot is passed over.
    {
        g_context = "id taken";
        start(32, 0x8000);
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
        g_s[1].mgr->CreateHeap(0x400, 2);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[3].m_equivalencyId = g_s[s].mgr->m_lastId + 1;
        const int want = g_s[1].mgr->m_lastId + 2;
        eq("id", at<FnId>(0x00011CC0)(g_s[0].mgr, nullptr), g_s[1].mgr->NextId());
        eq("passed over", g_s[1].mgr->m_lastId, want);
        same_all("id");
    }
    // A carved heap clears the flag of a slot that had it.
    {
        g_context = "flag on an empty slot";
        start(33, 0x8000);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[0].m_fixed = 1;
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 2);
        g_s[1].mgr->CreateHeap(0x400, 2);
        same_all("flag");
        eq("clear", g_s[1].mgr->m_heaps[0].m_fixed, 0);
    }
    // An empty slot of the same group, with a list that would serve: it is not asked.
    {
        g_context = "empty slot in the group";
        start(34, 0x8000);
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x100, 2);
        g_s[1].mgr->CreateHeap(0x100, 2);
        for (int s = 0; s < 2; s++) {
            AtHeapBlock* const b = (AtHeapBlock*)g_s[s].fixed;
            b->m_word = 0x1000 | (1u << AtHeapBlock::kTagShift);
            b->m_prev = nullptr;
            b->m_nextFree = nullptr;
            b->m_prevFree = nullptr;
            AtHeap& e = g_s[s].mgr->m_heaps[1];
            e.m_base = g_s[s].fixed;
            e.m_free = b;
            e.m_equivalencyId = g_s[s].mgr->m_heaps[0].m_equivalencyId;
            e.m_tag = 1;
        }
        same_ptr("refused", at<FnMgrAlloc>(0x00011EB0)(g_s[0].mgr, nullptr, heap_of(0, 0), 0x800), g_s[1].mgr->Allocate(&g_s[1].mgr->m_heaps[0], 0x800));
        same_all("refused");
    }
    // Allocation wider than asked of itself: narrower than the heap's own alignment, the wide sum is larger than the plain one, so the first try fails where a second through
    // AllocateAligned would fit. The first heap is not asked twice.
    {
        g_context = "first not asked again";
        start(35, 0x8000);
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x100, 2);
        g_s[1].mgr->CreateHeap(0x100, 2);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[0].m_allocShift = 1;
        void* const a = at<FnMgrAlloc>(0x00011EB0)(g_s[0].mgr, nullptr, heap_of(0, 0), 0xF8);
        void* const b = g_s[1].mgr->Allocate(&g_s[1].mgr->m_heaps[0], 0xF8);
        same_ptr("result", a, b);
        eq("refused", b == nullptr, 1);
        same_all("refused");
    }
    // A carved heap whose base someone moved below the region gives nothing back.
    {
        g_context = "below the region";
        start(36, 0x8000);
        at<FnCreateAt>(0x00011E40)(g_s[0].mgr, nullptr, g_s[0].mem, 0x100, 2);
        g_s[1].mgr->CreateHeapAt(g_s[1].mem, 0x100, 2);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[0].m_fixed = 0;
        at<FnP>(0x000121D0)(g_s[0].mgr, nullptr, heap_of(0, 0));
        g_s[1].mgr->DestroyHeap(&g_s[1].mgr->m_heaps[0]);
        same_all("below");
    }
    // A length that is not the string's: the text goes after the string, not at the length.
    {
        g_context = "append after a shorter string";
        char a[64], b[64];
        std::memset(a, 'x', sizeof a);
        std::memset(b, 'x', sizeof b);
        a[0] = b[0] = 'p';
        a[1] = b[1] = 0;
        unsigned int la = 5, lb = 5;
        at<FnAppend>(0x00012010)(nullptr, nullptr, a, "abc", &la, 32);
        g_s[1].mgr->Append(b, "abc", &lb, 32);
        eq("length", la, lb);
        eq("bytes", std::memcmp(a, b, sizeof a), 0);
    }
    // The report gives the heap's own alignment, not the one for allocating.
    {
        g_context = "report alignment";
        start(37, 0x8000);
        at<FnCreate>(0x00011D00)(g_s[0].mgr, nullptr, 0x400, 3);
        g_s[1].mgr->CreateHeap(0x400, 3);
        for (int s = 0; s < 2; s++) g_s[s].mgr->m_heaps[0].m_allocShift = 7;
        char a[1024], b[1024];
        at<FnReport>(0x000120A0)(g_s[0].mgr, nullptr, a, sizeof a);
        g_s[1].mgr->Report(b, sizeof b);
        const auto strip = [](char* t) { for (char* q = t; (q = std::strstr(q, ":0x")); q += 11) std::memset(q + 3, '_', 8); };
        strip(a);
        strip(b);
        eqs("report", a, b);
    }
}

// A loop in a free list hangs without a word otherwise. A list made of bytes this harness wrote can do that on either side, so say which step it was.
static DWORD WINAPI watchdog(void*) {
    Sleep(240000);
    char text[300];
    const int len = sprintf_s(text, "HANG in [%.200s], %llu comparisons so far, side %d\n", g_context.c_str(), g_checks, (int)g_where);
    DWORD ignored;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
    ExitProcess(4);
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    int seeds = 40, ops = 3000;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--ops") && i + 1 < argc) ops = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--every") && i + 1 < argc) g_every = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.empty()) { std::printf("usage: oracle_hep <default.xbe> [cases dir] [--seeds N] [--ops N] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x00011070, (const void*)o_fatal);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    if (!make_side(g_s[0], 0) || !make_side(g_s[1], 1)) return 2;

    CreateThread(nullptr, 0, watchdog, nullptr, 0, nullptr);
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_parts();
    run_states();
    std::printf("parts done: %llu comparisons, %d differences\n", g_checks, g_failures);
    for (int s = 1; s <= seeds; s++) run_fuzz((unsigned)s, ops);
    report_entries();
    std::printf("%s: %d seeds of %d operations; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", seeds, ops, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
