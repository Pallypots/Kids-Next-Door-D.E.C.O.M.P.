// Differential oracle for the matrix device, its pool, and the last five matrix-table slots the game reads (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-08-matdev.md.
//
//   oracle_matdev <default.xbe> <text cases> [--count N] [--entries FILE] [--mutate]
//
// The device and pool: one object of the original's (its own memory, the original's vftable, so every call runs the original's code) and one of the recovered class, driven by the same C++ calls through
// the same pointer type, slot by slot. The original's operator new, delete, heap stack and AtRaise are redirected to doubles that log; the recovered code's go to the same log. Compared: the log of calls
// (allocations with their sizes and the pool heap's allocation shift at that moment, frees, heap pushes and pops, reports), every word of each object, what each call returns, and the matrices.
//
// The matrix slots: the original's 0x000EC2B0 fills a table of the harness's; each of the five must hold the function read for it, and is called through that slot against the member of g_atMatrix; the two
// helpers directly. By bits at x87 precision 53, NaNs from arithmetic alike, counted apart, as in oracle_matrix.cpp.
//
// Then every .AKC on the disc, read by the host's AtCoordSysDatabase::Read twice: with the original's device and matrix functions, and with the recovered ones.
#include <float.h>
#include <malloc.h>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMatrixDevice.h"
#include "gm/GmNameMap.h"
#include "matdev_cases.h"
#include "oracle_image.h"

using namespace matdev_cases;

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static const char* g_op = "";
static int g_nan_alike;
static std::map<std::string, int> g_by_op;

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

static void fail_report(const char* what, const char* detail) {
    g_by_op[g_op]++;
    if (g_failures++ < 24) std::printf("DIFF %s: %s%s%s\n", g_op, what, *detail ? ": " : "", detail);
}

static void eq(const char* what, uint32_t a, uint32_t b) {
    g_checks++;
    if (a == b) return;
    char d[64];
    std::snprintf(d, sizeof d, "original %08X, recovered %08X", a, b);
    fail_report(what, d);
}

// exact: moved, not computed. Otherwise any two NaNs are alike.
static void same(const float* a, const float* b, int count, bool exact) {
    g_checks += (unsigned)count;
    bool ok = true;
    for (int i = 0; i < count; i++) {
        if (to_bits(a[i]) == to_bits(b[i])) continue;
        if (!exact && a[i] != a[i] && b[i] != b[i]) { g_nan_alike++; continue; }
        ok = false;
    }
    if (ok) return;
    g_by_op[g_op]++;
    if (g_failures++ < 24) {
        std::printf("DIFF %s: original", g_op);
        for (int i = 0; i < count; i++) std::printf(" %08X", to_bits(a[i]));
        std::printf(", recovered");
        for (int i = 0; i < count; i++) std::printf(" %08X", to_bits(b[i]));
        std::printf("\n");
    }
}

// ---- the log, and the doubles beneath both sides -------------------------------------------------------------------------
struct Log {
    uint32_t v[1 << 16];
    int n = 0;
    void add(uint32_t a, uint32_t b = 0) { if (n + 2 <= (int)(sizeof v / 4)) { v[n++] = a; v[n++] = b; } }
} static g_log[2];
static int g_side;
static bool g_recording;
static bool g_fail_alloc;
static const int kFill = 0xA5;  // The game's heap is not cleared: a missing store shows.

// Each side's heaps: the same addresses, so pushes log alike. Raw storage, not the recovered AtHeap's constructor.
static unsigned char g_heap_storage[2][sizeof(AtHeap)];
static AtHeap* const g_poolHeap = (AtHeap*)(void*)g_heap_storage[0];
static void* const g_devHeap = g_heap_storage[1];

static void* fresh(std::size_t n) {
    if (g_recording) {
        g_log[g_side].add('N', (uint32_t)n);
        g_log[g_side].add('S', g_poolHeap->m_allocShift);
        if (g_fail_alloc) return nullptr;
    }
    // 64-byte aligned, as the game's heap gives the pool: the original's SSE matrix functions fault on less than 16.
    void* p = _aligned_malloc(n ? n : 1, 64);
    if (p) std::memset(p, g_recording ? kFill : 0, n);
    return p;
}
static void gone(void* p) { if (g_recording && p) g_log[g_side].add('D'); _aligned_free(p); }

static void* __cdecl o_new(unsigned n) { return fresh(n); }
static void __cdecl o_delete(void* p) { gone(p); }
static void __cdecl o_push(void* h) { if (g_recording) g_log[g_side].add('P', (uint32_t)(uintptr_t)h); }
static void __cdecl o_pop() { if (g_recording) g_log[g_side].add('p'); }
static void __cdecl o_raise(int code, int, int, int) { if (g_recording) g_log[g_side].add('R', (uint32_t)code); }

void* operator new(std::size_t n) { return fresh(n); }
void* operator new[](std::size_t n) { return fresh(n); }
void operator delete(void* p) noexcept { gone(p); }
void operator delete[](void* p) noexcept { gone(p); }
void operator delete(void* p, std::size_t) noexcept { gone(p); }
void operator delete[](void* p, std::size_t) noexcept { gone(p); }
void AtPushHeap(void* h) { o_push(h); }
void AtPopHeap() { o_pop(); }
void AtRaise(int code, int a, int b, int c) { o_raise(code, a, b, c); }
void AtFatal(const char* message, int, int, int) {
    if (g_recording) {
        uint32_t h = 2166136261u;  // The message by its hash: which slot stopped.
        for (const char* c = message; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
        g_log[g_side].add('F', h);
        return;
    }
    std::printf("AtFatal: %s\n", message);
    std::exit(3);
}
void AtLibraryFree(void* data) { std::free(data); }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

struct On {
    explicit On(int side) { g_side = side; g_recording = true; }
    ~On() { g_recording = false; g_side = 0; }
};

static void compare_logs(const char* what) {
    if (std::getenv("MATDEV_TRACE")) std::printf("trace: %s\n", g_op);
    eq(what, (uint32_t)g_log[0].n, (uint32_t)g_log[1].n);
    if (g_log[0].n != g_log[1].n && g_failures < 6) {
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < g_log[side].n && i < 40; i += 2) std::printf("%c%X ", (char)g_log[side].v[i], g_log[side].v[i + 1]);
            std::printf("\n");
        }
    }
    for (int i = 0; i < g_log[0].n && i < g_log[1].n; i++) {
        if (g_log[0].v[i] != g_log[1].v[i]) {
            eq(what, g_log[0].v[i], g_log[1].v[i]);
            break;
        }
    }
    g_log[0].n = g_log[1].n = 0;
}

static void reset_heaps(unsigned alignShift, unsigned allocShift) {
    std::memset(g_heap_storage, 0, sizeof g_heap_storage);
    g_poolHeap->m_alignShift = alignShift;
    g_poolHeap->m_allocShift = allocShift;
}

// ---- the device and the pool ----------------------------------------------------------------------------------------------
// Side 0: the original's, in its own memory with its own vftable. Side 1: the recovered class. Both reached through pointers the compiler cannot see through, so every call is virtual.
static AtMatrixDeviceXbox* volatile g_device[2];

static AtMatrixDeviceXbox* original_device() {
    void* p = std::calloc(1, 8);
    std::memset(p, kFill, 8);
    at<void(__fastcall*)(void*, void*)>(0x0010F800)(p, nullptr);  // The base's constructor, then the vftable: as the original's makers do it.
    *(uint32_t*)p = 0x00288B48;
    return (AtMatrixDeviceXbox*)p;
}

// What slots 6 and 7 free; each logs the flag its destructor slot is given.
class ItemA : public AtMatrixDeviceItemA {
public:
    void Slot0() override {}
    void Slot1() override {}
    void Destroy(unsigned int flags) override { if (g_recording) g_log[g_side].add('A', flags); }
};
class ItemB : public AtMatrixDeviceItemB {
public:
    void Slot0() override {}
    void Destroy(unsigned int flags) override { if (g_recording) g_log[g_side].add('B', flags); }
};

// Words of a pool, the vftable aside, with the block given as an offset from nothing, or as present.
static void same_pool(const char* what, AtMatrixPoolXbox* a, AtMatrixPoolXbox* b) {
    eq(what, a->m_matrices != nullptr, b->m_matrices != nullptr);
    eq(what, a->m_count, b->m_count);
}

static void test_device() {
    // The heap statics: the original's own setters on its side.
    g_device[0] = original_device();
    g_device[1] = new AtMatrixDeviceXbox;
    g_op = "device constructed";
    eq("base member", ((uint32_t*)g_device[0])[1], ((uint32_t*)g_device[1])[1]);

    const unsigned kAlign[] = {0, 2, 5, 6, 7, 12};
    for (int heapCase = 0; heapCase < 4; heapCase++) {
        // 0: both heaps. 1: no device heap. 2: allocation fails. 3: both heaps again, to run the rest after a failure.
        for (int side = 0; side < 2; side++) {
            void* dev = heapCase == 1 ? nullptr : g_devHeap;
            if (side == 0) {
                at<void(__cdecl*)(void*)>(0x000F7BD0)(dev);
                at<void(__cdecl*)(void*)>(0x000F5FC0)(g_poolHeap);
            } else {
                AtMatrixDeviceXbox::SetHeap(dev);
                AtMatrixPoolXbox::SetHeap(g_poolHeap);
            }
        }
        g_op = "heap setters";
        eq("device heap", *(uint32_t*)(uintptr_t)0x003B1F94, (uint32_t)(uintptr_t)AtMatrixDeviceXbox::s_heap);
        eq("pool heap", *(uint32_t*)(uintptr_t)0x003B1F58, (uint32_t)(uintptr_t)AtMatrixPoolXbox::s_heap);
        g_fail_alloc = heapCase == 2;

        // NewMatrix, through slot 0 and through slot 8.
        for (int via = 0; via < 2; via++) {
            g_op = via ? "DoNewMatrix" : "NewMatrix";
            AtMatrix* m[2];
            for (int side = 0; side < 2; side++) { On on(side); m[side] = via ? g_device[side]->DoNewMatrix() : g_device[side]->NewMatrix(); }
            compare_logs("log");
            eq("null", m[0] == nullptr, m[1] == nullptr);
            if (m[0] && m[1]) same(m[0]->m, m[1]->m, 16, true);
            g_fail_alloc = false;
            g_op = via ? "ReleaseMatrix" : "FreeMatrix";
            for (int side = 0; side < 2; side++) { On on(side); if (via) g_device[side]->ReleaseMatrix(m[side]); else g_device[side]->FreeMatrix(m[side]); }
            compare_logs("log");
            g_fail_alloc = heapCase == 2;
        }

        // NewPool, through slot 1 and slot 9, and everything a pool does.
        for (int via = 0; via < 2; via++) {
            g_fail_alloc = heapCase == 2;
            g_op = via ? "DoNewPool" : "NewPool";
            AtMatrixPoolXbox* pool[2];
            for (int side = 0; side < 2; side++) { On on(side); pool[side] = (AtMatrixPoolXbox*)(via ? g_device[side]->DoNewPool() : g_device[side]->NewPool()); }
            compare_logs("log");
            eq("null", pool[0] == nullptr, pool[1] == nullptr);
            g_fail_alloc = false;
            if (!pool[0] || !pool[1]) continue;
            same_pool("new pool", pool[0], pool[1]);
            eq("original's vftable", *(uint32_t*)pool[0], 0x002892E4);

            const unsigned kCounts[] = {1, 0, 2, 3, 37, 0x80000001u, 0xC0000000u, 0xFFFFFFFFu, 4, 0x80000002u};
            int c = 0;
            for (unsigned count : kCounts) {
                const unsigned align = kAlign[c % 6];
                const bool failing = (c % 5) == 3;
                c++;
                g_op = "Reserve";
                uint32_t shifts[2];
                for (int side = 0; side < 2; side++) {
                    reset_heaps(align, 0xDEAD0000u + align);
                    g_fail_alloc = failing;
                    { On on(side); pool[side]->Reserve(count); }
                    g_fail_alloc = false;
                    shifts[side] = g_poolHeap->m_allocShift;
                }
                compare_logs("log");
                eq("allocation shift after", shifts[0], shifts[1]);
                same_pool("pool", pool[0], pool[1]);
                if (pool[0]->m_matrices && pool[1]->m_matrices) {
                    // What was constructed and what was not: the first matrices and, for counts that wrap, the one block of 64 bytes.
                    const unsigned shown = count < 40 ? count : ((count << 6) >= 64 ? 1 : 0);
                    for (unsigned i = 0; i < shown; i++) same(pool[0]->m_matrices[i].m, pool[1]->m_matrices[i].m, 16, true);
                }
                g_op = "pool slots";
                for (int side = 0; side < 2; side++) {
                    On on(side);
                    g_log[side].add('c', pool[side]->Count());
                    g_log[side].add('r', (uint32_t)pool[side]->IsReserved());
                    const unsigned kIndex[] = {0, 1, 5, 0x3FFFFFF, 0xFFFFFFFFu};
                    for (unsigned index : kIndex) {
                        g_log[side].add('a', (uint32_t)((char*)pool[side]->At(index) - (char*)pool[side]->m_matrices));
                        g_log[side].add('m', (uint32_t)((char*)pool[side]->Matrix(index) - (char*)pool[side]->m_matrices));
                    }
                }
                compare_logs("log");
            }
            g_op = "Clear";
            for (int side = 0; side < 2; side++) { On on(side); pool[side]->Clear(); }
            compare_logs("log");
            same_pool("pool", pool[0], pool[1]);
            for (int side = 0; side < 2; side++) { On on(side); g_log[side].add('r', (uint32_t)pool[side]->IsReserved()); pool[side]->Clear(); }
            compare_logs("log");
            g_op = "Reserve, then freed by the device";
            for (int side = 0; side < 2; side++) { reset_heaps(4, 4); On on(side); pool[side]->Reserve(3); }
            compare_logs("log");
            for (int side = 0; side < 2; side++) { On on(side); if (via) g_device[side]->ReleasePool(pool[side]); else g_device[side]->FreePool(pool[side]); }
            compare_logs("log");
        }
        g_fail_alloc = false;

        g_op = "free null";
        for (int side = 0; side < 2; side++) {
            On on(side);
            g_device[side]->FreePool(nullptr);
            g_device[side]->ReleasePool(nullptr);
            g_device[side]->FreeMatrix(nullptr);
            g_device[side]->FreeItemA(nullptr);
            g_device[side]->FreeItemB(nullptr);
            g_device[side]->ReleaseItemA(nullptr);
            g_device[side]->ReleaseItemB(nullptr);
        }
        compare_logs("log");
        g_op = "free items";
        for (int side = 0; side < 2; side++) {
            ItemA a;
            ItemB b;
            On on(side);
            g_device[side]->FreeItemA(&a);
            g_device[side]->FreeItemB(&b);
            g_device[side]->ReleaseItemA(&a);
            g_device[side]->ReleaseItemB(&b);
        }
        compare_logs("log");
    }

    // Slots 2 and 3 pass on to slots 10 and 11. The original's would make graphics objects, which are not recovered: so the original's two forwarders are run on the recovered device, whose slots 10 and
    // 11 stop with a diagnostic, and that must be what they reach, as the recovered forwarders do.
    g_op = "slots 2 and 3";
    {
        On on(0);
        at<void(__fastcall*)(void*, void*)>(0x000F7D60)(g_device[1], nullptr);
        g_log[0].add('|');
        at<void(__fastcall*)(void*, void*)>(0x000F7D70)(g_device[1], nullptr);
    }
    {
        On on(1);
        g_device[1]->Slot2();
        g_log[1].add('|');
        g_device[1]->Slot3();
    }
    compare_logs("log");
    if (g_log[0].n != 0) eq("cleared", 0, 1);

    g_op = "pool destroyed in place";
    {
        // The complete destructors, which the deleting one calls; and the base's, which leaves its vftable.
        AtMatrixPoolXbox* pool[2];
        for (int side = 0; side < 2; side++) { On on(side); pool[side] = (AtMatrixPoolXbox*)g_device[side]->NewPool(); }
        for (int side = 0; side < 2; side++) { reset_heaps(3, 3); On on(side); pool[side]->Reserve(2); }
        compare_logs("log");
        { On on(0); at<void(__fastcall*)(void*, void*)>(0x000F6000)(pool[0], nullptr); }
        { On on(1); pool[1]->~AtMatrixPoolXbox(); }
        compare_logs("log");
        same_pool("pool", pool[0], pool[1]);
        eq("base vftable", *(uint32_t*)pool[0], 0x002892CC);
        AtMatrixPoolXbox probe;
        const uint32_t derived = *(uint32_t*)&probe;
        probe.~AtMatrixPoolXbox();
        eq("base vftable put back", *(uint32_t*)&probe != derived, 1);
        new (&probe) AtMatrixPoolXbox;
        _aligned_free(pool[0]);
        _aligned_free(pool[1]);
    }
}

// ---- the matrix slots ---------------------------------------------------------------------------------------------------
static uint32_t g_table[104];
static void* slot(uint32_t va) { return (void*)(uintptr_t)g_table[(va - 0x002C60C8) / 4]; }
struct alignas(16) M { AtMatrix v; };
static void add(Hash& h, const AtMatrix& m) { for (float f : m.m) h.add(f); }

typedef void(__cdecl* FnRotate)(AtMatrix*, float);
typedef void(__cdecl* FnProduct)(AtMatrix*, const float*);
typedef void(__cdecl* FnGetRotation)(const AtMatrix*, AtQuaternion*);

static void round_of(unsigned seed, int count, int kind, Hash* hash) {
    Rng rng{seed};
    for (int i = 0; i < count; i++) {
        Inputs in;
        draw(rng, kind, &in);
        M a, start, b, o, r;
        a.v = in.a;
        start.v = in.start;
        b.v = in.b;
        const float t = in.t;
        const float* rows = in.rows;

        struct { const char* name; uint32_t slot; FnRotate mine; } kRotations[] = {
            {"pre rotate x", 0x002C6158, g_atMatrix.PreRotateX}, {"pre rotate y", 0x002C615C, g_atMatrix.PreRotateY},
            {"pre rotate z", 0x002C6160, g_atMatrix.PreRotateZ}, {"post rotate z", 0x002C6198, g_atMatrix.PostRotateZ}};
        for (auto& k : kRotations) {
            g_op = k.name;
            o = a; r = a;
            ((FnRotate)slot(k.slot))(&o.v, t);
            k.mine(&r.v, t);
            same(o.v.m, r.v.m, 16, false);
        }

        g_op = "pre multiply";
        o = a; r = a;
        at<FnProduct>(0x001155F0)(&o.v, rows);
        AtMatrixPreMultiply3x3(&r.v, rows);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "post multiply";
        o = a; r = a;
        at<FnProduct>(0x00115850)(&o.v, rows);
        AtMatrixPostMultiply3x3(&r.v, rows);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        // The rows in the matrix itself, as no caller does: what each reads after writing shows.
        g_op = "pre multiply by itself";
        o = a; r = a;
        at<FnProduct>(0x001155F0)(&o.v, o.v.m + 4);
        AtMatrixPreMultiply3x3(&r.v, r.v.m + 4);
        same(o.v.m, r.v.m, 16, false);
        g_op = "post multiply by itself";
        o = a; r = a;
        at<FnProduct>(0x00115850)(&o.v, o.v.m);
        AtMatrixPostMultiply3x3(&r.v, r.v.m);
        same(o.v.m, r.v.m, 16, false);

        g_op = "get rotation";
        AtQuaternion qo, qr;
        std::memcpy(&qo, start.v.m, sizeof qo);
        qr = qo;
        ((FnGetRotation)slot(0x002C61F4))(&b.v, &qo);
        g_atMatrix.GetRotation(&b.v, &qr);
        same(&qo.x, &qr.x, 4, false);
        if (hash) hash->add(qo.x), hash->add(qo.y), hash->add(qo.z), hash->add(qo.w);
        g_op = "get rotation into itself";
        o = b; r = b;
        ((FnGetRotation)slot(0x002C61F4))(&o.v, (AtQuaternion*)o.v.m);
        g_atMatrix.GetRotation(&r.v, (AtQuaternion*)r.v.m);
        same(o.v.m, r.v.m, 16, false);
    }
}

// GetRotation's y and z ways on radicands whose grouping shows: found by analysis-local/scratch/matdev/search_rotation.py, since random values reach a float
// rounding midpoint about once in 2^29. Each gives another w if the radicand is grouped (a - b) - c.
static const AtMatrix kCraftedRotations[] = {
    {{from_bits(0xCEB34A79), from_bits(0x00000000), from_bits(0x45E2799B), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x3FF4C523), from_bits(0x00000000), from_bits(0x00000000), from_bits(0xB505B820), from_bits(0x00000000), from_bits(0x3FBAC4E5), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // y way: w 3DBF5B98, grouped 3DBF5B99
    {{from_bits(0xD0828F13), from_bits(0x00000000), from_bits(0x45770FB6), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x3FFF4BCE), from_bits(0x00000000), from_bits(0x00000000), from_bits(0xB8319972), from_bits(0x00000000), from_bits(0x3FC29530), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // y way: w 3C74A0D6, grouped 3C74A0D5
    {{from_bits(0xD2B8FA3F), from_bits(0x00000000), from_bits(0x4979E385), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x400F0686), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x3B34A10F), from_bits(0x00000000), from_bits(0x3FADA83D), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // y way: w 3F4FDEC4, grouped 3F4FDEC5
    {{from_bits(0xCF08F4F5), from_bits(0x00000000), from_bits(0x4AFEAF54), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x3FD93F5B), from_bits(0x00000000), from_bits(0x00000000), from_bits(0xBB0F7AC0), from_bits(0x00000000), from_bits(0x3F845B32), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // y way: w 42AE19DA, grouped 42AE19DB
    {{from_bits(0xCFBE7EA0), from_bits(0xB7C37F71), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x451ECC10), from_bits(0x3FFD72FC), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x4002C142), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // z way: w 3C822B2A, grouped 3C822B29
    {{from_bits(0xCF372C0D), from_bits(0xB74D63E3), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x4423FA35), from_bits(0x3F8AAB5F), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x3FD8292D), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // z way: w 3BC1DA92, grouped 3BC1DA91
    {{from_bits(0xD102E521), from_bits(0xBADDB055), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x475BCA30), from_bits(0x3FEA255B), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x4016DB73), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // z way: w 3E19AFC3, grouped 3E19AFC4
    {{from_bits(0xD0A910E6), from_bits(0x3C70F3C1), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x48832A7C), from_bits(0x3FD003AD), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x4025422E), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000), from_bits(0x00000000)}},  // z way: w 3F64424E, grouped 3F64424F
};

static void crafted_rotations() {
    g_op = "get rotation, crafted";
    for (const AtMatrix& m : kCraftedRotations) {
        M b;
        b.v = m;
        AtQuaternion qo = {}, qr = {};
        ((FnGetRotation)slot(0x002C61F4))(&b.v, &qo);
        g_atMatrix.GetRotation(&b.v, &qr);
        same(&qo.x, &qr.x, 4, false);
    }
}

// ---- real .AKC files ------------------------------------------------------------------------------------------------------
class FileStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

static AtMatrixTable original_table() {
    AtMatrixTable t = g_atMatrix;
#define SLOT(member, va) t.member = (decltype(t.member))slot(va)
    SLOT(Copy, 0x002C60C8); SLOT(Identity, 0x002C60CC); SLOT(SetTranslation, 0x002C60D4); SLOT(SetRotation, 0x002C60E0); SLOT(SetLookAt, 0x002C60F4); SLOT(SetScale, 0x002C6120);
    SLOT(SetInverse, 0x002C61D4); SLOT(Multiply, 0x002C6228); SLOT(SetProduct, 0x002C6234); SLOT(TransformPoint, 0x002C6250); SLOT(SetUniformScale, 0x002C611C); SLOT(Read, 0x002C6264);
    SLOT(GetPosition, 0x002C61F8); SLOT(GetAxes, 0x002C61FC); SLOT(TransformDirection, 0x002C625C);
    SLOT(PreRotateX, 0x002C6158); SLOT(PreRotateY, 0x002C615C); SLOT(PreRotateZ, 0x002C6160); SLOT(PostRotateZ, 0x002C6198); SLOT(GetRotation, 0x002C61F4);
#undef SLOT
    return t;
}

struct FileTotals { unsigned files, entries, matrices, logged; };

// One side: read, update every world matrix, transform a point, update every inverse, take each one's rotation; keep every matrix and point; destroy the database, which frees the pool.
static void run_akc_side(const std::vector<unsigned char>& data, int side, std::vector<float>* out, unsigned* count, unsigned* entries) {
    std::string text((const char*)data.data(), data.size());
    FileStream stream;
    stream.m_base = (char*)text.data();
    stream.m_cur = stream.m_base;
    stream.m_limit = stream.m_base + text.size();
    stream.m_size = (unsigned)text.size();
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    {
        On on(side);
        AtCoordSysDatabase db;
        db.Read(g_device[side], &tokenizer);
        AtMatrixPoolXbox* pool = (AtMatrixPoolXbox*)db.m_pool;
        *count = pool ? pool->m_count : 0;
        *entries = db.m_rotTransCount + db.m_scaledCount;
        AtCoordSys* all[2] = {db.m_rotTrans, db.m_scaled};
        const unsigned n[2] = {db.m_rotTransCount, db.m_scaledCount};
        const unsigned size[2] = {sizeof(AtRotTransCoordSys), sizeof(AtScaleCoordSys)};
        for (int pass = 0; pass < 2; pass++) {
            for (int kind = 0; kind < 2; kind++) {
                for (unsigned i = 0; i < n[kind]; i++) {
                    AtCoordSys* cs = (AtCoordSys*)((char*)all[kind] + i * size[kind]);
                    if (pass == 0) {
                        cs->UpdateWorld();
                        const AtVector3 p = {1.5f, -2.25f, 0.125f};
                        AtVector3 to;
                        cs->Transform(&p, &to);
                        out->push_back(to.x); out->push_back(to.y); out->push_back(to.z);
                    } else {
                        cs->UpdateInverse();
                        AtQuaternion q;
                        g_atMatrix.GetRotation(cs->m_world, &q);
                        out->push_back(q.x); out->push_back(q.y); out->push_back(q.z); out->push_back(q.w);
                    }
                }
            }
        }
        for (unsigned i = 0; pool && pool->m_matrices && i < pool->m_count; i++) out->insert(out->end(), pool->m_matrices[i].m, pool->m_matrices[i].m + 16);
        tokenizer.SetStream(nullptr);
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
    }
}

static void run_akc_file(const std::vector<unsigned char>& data, const char* name, FileTotals* totals) {
    g_op = name;
    const AtMatrixTable mine = g_atMatrix;
    const AtMatrixTable theirs = original_table();
    std::vector<float> out[2];
    unsigned counts[2], entries[2];
    for (int side = 0; side < 2; side++) {
        g_atMatrix = side == 0 ? theirs : mine;
        reset_heaps(4, 4);
        run_akc_side(data, side, &out[side], &counts[side], &entries[side]);
    }
    g_atMatrix = mine;
    eq("matrices", counts[0], counts[1]);
    eq("coordinate systems", entries[0], entries[1]);
    eq("outputs", (uint32_t)out[0].size(), (uint32_t)out[1].size());
    if (out[0].size() == out[1].size() && !out[0].empty()) same(out[0].data(), out[1].data(), (int)out[0].size(), false);
    totals->logged += (unsigned)g_log[0].n / 2;
    compare_logs("calls to the device, heaps and reports");
    totals->files++;
    totals->entries += entries[0];
    totals->matrices += counts[0];
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    int count = 200000;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--count") && i + 1 < argc) count = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() < 2) { std::printf("usage: oracle_matdev <default.xbe> <text cases> [--count N] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    if (*(uint32_t*)(uintptr_t)0x00272AC4 != 0x3F800000 || *(uint32_t*)(uintptr_t)0x0027463C != 0x3F000000 || *(uint32_t*)(uintptr_t)0x002725F0 != 0) {
        std::printf("constants are not as read\n");
        return 2;
    }
    at<void(__cdecl*)()>(0x00125BC7)();
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011170, (const void*)o_raise);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);

    std::memset(g_table, 0xCC, sizeof g_table);
    at<void(__cdecl*)(void*)>(0x000EC2B0)(g_table);
    struct { uint32_t slot, target; void* mine; } kSlots[] = {
        {0x002C6158, 0x00117EE0, (void*)g_atMatrix.PreRotateX}, {0x002C615C, 0x00117F40, (void*)g_atMatrix.PreRotateY}, {0x002C6160, 0x00117FA0, (void*)g_atMatrix.PreRotateZ},
        {0x002C6198, 0x00118100, (void*)g_atMatrix.PostRotateZ}, {0x002C61F4, 0x001163D0, (void*)g_atMatrix.GetRotation}};
    for (auto& k : kSlots) {
        g_checks++;
        if (!k.mine) { std::printf("DIFF slot %08X has no member\n", k.slot); g_failures++; }
        if (g_table[(k.slot - 0x002C60C8) / 4] != k.target) { std::printf("DIFF slot %08X holds %08X\n", k.slot, g_table[(k.slot - 0x002C60C8) / 4]); g_failures++; }
    }
    g_checks++;
    if ((void*)g_atMatrix.PreRotateX != (void*)AtMatrixPreRotateX || (void*)g_atMatrix.GetRotation != (void*)AtMatrixGetRotation) { std::printf("DIFF g_atMatrix members\n"); g_failures++; }
    if (entries && !arm_entries(entries)) return 2;
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.
    unsigned int control;
    _controlfp_s(&control, _PC_53, _MCW_PC);

    test_device();
    std::printf("device and pool: done\n");

    Hash hash;
    round_of(4242, count, 0, &hash);
    std::printf("hash of the original's products and rotations to quaternions, %d rounds: %016llX\n", count, hash.h);
    round_of(777, count / 4, 1, nullptr);   // Infinities and NaNs.
    round_of(999, count / 4, 2, nullptr);   // Signalling NaNs too.
    crafted_rotations();
    std::printf("matrix slots: done\n");

    int nameHeap = 0;
    GmNameMap::s_heap = &nameHeap;
    GmNameSpaceDb::s_heap = &nameHeap;
    AtCoordSysDatabase::s_heap = &nameHeap;
    AtIoStream::s_heap = &nameHeap;
    AtIoTokenFile::s_heap = &nameHeap;
    // Both devices, with both heaps set.
    at<void(__cdecl*)(void*)>(0x000F7BD0)(g_devHeap);
    at<void(__cdecl*)(void*)>(0x000F5FC0)(g_poolHeap);
    AtMatrixDeviceXbox::SetHeap(g_devHeap);
    AtMatrixPoolXbox::SetHeap(g_poolHeap);
    FileTotals totals = {};
    const std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AKC\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty() && std::fread(data.data(), 1, data.size(), f) != data.size()) { std::fclose(f); return 2; }
        std::fclose(f);
        run_akc_file(data, line, &totals);
    }
    std::fclose(index);
    std::printf(".AKC files read with the original's device and matrix functions and with the recovered ones: %u files, %u coordinate systems, %u matrices, %u calls logged\n", totals.files,
                totals.entries, totals.matrices, totals.logged);
    std::printf("NaNs of arithmetic differing only in sign or payload: %d\n", g_nan_alike);
    for (const auto& k : g_by_op) std::printf("  %s: %d\n", k.first.c_str(), k.second);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
