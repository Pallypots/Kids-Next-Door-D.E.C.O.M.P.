// Differential oracle for the graphics device: its two abstract layers, the concrete device's construction, its member-returning slots, and the members' set-up and freeing (Windows x86, local only).
// See docs/verification.md and docs/worklog/2026-11-08-gfxdev.md.
//
//   oracle_gfxdev <default.xbe> <text cases> [--entries FILE] [--mutate]
//
// Each part makes one object of the original's (its own constructor on fresh memory, so its own vftables and code) and one of the recovered class on memory filled alike, drives both the same way,
// and compares every word of the two blocks. Words may differ only as follows, each checked: a vftable of a recovered class against the original's address for it (a fixed map, from objects of each
// recovered class); a pointer into the block itself against the same offset in the other block; the address of the shared words of a context (0x002C9BF8) and of the ordering function (0x000EF380)
// against the recovered ones. The original's operator new, delete, heap push and pop and AtRaise go to doubles that log, as do the recovered code's; each freed block is logged by the order of its
// allocation, so which block is freed when is compared. Fresh memory is 0xA5, so a missing store shows.
//
// The texture work's members are made on both sides by the original's own constructors (this harness supplies src/at/AtGraphicsDeviceHost.cpp's seams so), so they are the same on both sides and
// say nothing about recovered code. Then every .AKC on the disc, read with the matrix device each device's slot 17 hands out: the original's device's own slot on one side, the recovered on the other.
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
#include "at/AtGraphicsDevice.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static const char* g_op = "";

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

static void fail_report(const char* what, const char* detail) {
    if (g_failures++ < 30) std::printf("DIFF %s: %s%s%s\n", g_op, what, *detail ? ": " : "", detail);
}

static void eq(const char* what, uint32_t a, uint32_t b) {
    g_checks++;
    if (a == b) return;
    char d[64];
    std::snprintf(d, sizeof d, "original %08X, recovered %08X", a, b);
    fail_report(what, d);
}

// ---- the log, and the doubles beneath both sides -------------------------------------------------------------------------
struct Log {
    uint32_t v[1 << 16];
    int n = 0;
    void add(uint32_t a, uint32_t b = 0) { if (n + 2 <= (int)(sizeof v / 4)) { v[n++] = a; v[n++] = b; } }
} static g_log[2];
static int g_side;
static bool g_recording;
static const int kFill = 0xA5;
static uint32_t g_next[2];

// Each block carries its number, in allocation order on its side, in 64 bytes before it: no allocation of the harness's own, so it works before static constructors have run.
static void* fresh(std::size_t n) {
    unsigned char* raw = (unsigned char*)_aligned_malloc(n + 64, 64);
    if (!raw) return nullptr;
    std::memset(raw + 64, kFill, n);
    if (g_recording) g_log[g_side].add('N', (uint32_t)n);
    *(uint32_t*)raw = g_recording ? g_next[g_side]++ : 0xFFFFFFFFu;  // only what is made while recording is numbered, so the two sides count alike
    return raw + 64;
}
static void gone(void* p) {
    if (!p) return;
    unsigned char* raw = (unsigned char*)p - 64;
    if (g_recording) g_log[g_side].add('D', *(uint32_t*)raw);
    _aligned_free(raw);
}

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
        uint32_t h = 2166136261u;
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
    eq(what, (uint32_t)g_log[0].n, (uint32_t)g_log[1].n);
    if (g_log[0].n != g_log[1].n && g_failures < 8) {
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

// ---- the seams of src/at/AtGraphicsDeviceHost.cpp: the original's own code for the texture work's classes ----------------
typedef void(__fastcall* Ctor)(void* self, void* unused);
AtGfxBufferSet::~AtGfxBufferSet() { if (g_recording) g_log[g_side].add('X', 2); }
AtGfxResourceXbox::~AtGfxResourceXbox() { if (g_recording) g_log[g_side].add('X', 4); }

// ---- comparing blocks -----------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_vtables;   // recovered vftable -> original's
static std::map<uint32_t, uint32_t> g_statics;   // recovered address -> original's

static uint32_t vptr(const void* p) { return *(const uint32_t*)p; }

static void same_block(const char* what, const void* o, const void* r, uint32_t size) {
    const uint32_t* a = (const uint32_t*)o;
    const uint32_t* b = (const uint32_t*)r;
    const uint32_t oa = (uint32_t)(uintptr_t)o, ra = (uint32_t)(uintptr_t)r;
    int shown = 0;
    for (uint32_t i = 0; i < size / 4; i++) {
        g_checks++;
        const uint32_t x = a[i], y = b[i];
        if (x == y) continue;
        auto v = g_vtables.find(y);
        if (v != g_vtables.end() && v->second == x) continue;
        auto s = g_statics.find(y);
        if (s != g_statics.end() && s->second == x) continue;
        if (x - oa < size && y - ra < size && x - oa == y - ra) continue;
        char d[96];
        std::snprintf(d, sizeof d, "+0x%X original %08X, recovered %08X", i * 4, x, y);
        if (shown++ < 6) fail_report(what, d);
        else g_failures++;
    }
}

static void* block(uint32_t size) {
    void* p = _aligned_malloc(size, 64);
    std::memset(p, kFill, size);
    return p;
}

// Words of garbage, the same on both sides, skipping the vftable.
static void garble(void* p, uint32_t size, uint32_t seed) {
    uint32_t* w = (uint32_t*)p;
    for (uint32_t i = 1; i < size / 4; i++) {
        seed = seed * 1664525u + 1013904223u;
        w[i] = seed | 1;  // never null, so no freeing of garbage
    }
}

// A block to be freed, on the current side; garble makes nothing freeable, so these are put in by hand.
static void* tagged() { return operator new(8); }

// ---- the abstract layers --------------------------------------------------------------------------------------------------
static void test_layers() {
    // The display mode.
    g_op = "display mode constructor";
    void* m[2] = {block(0x24), block(0x24)};
    at<Ctor>(0x00043480)(m[0], nullptr);
    new (m[1]) AtDisplayMode;
    same_block("mode", m[0], m[1], 0x24);
    g_op = "display mode clear";
    garble(m[0], 0x24, 7);
    garble(m[1], 0x24, 7);
    ((uint32_t*)m[0])[0] = ((uint32_t*)m[1])[0] = 0x12345678;
    at<Ctor>(0x000434A0)(m[0], nullptr);
    ((AtDisplayMode*)m[1])->Clear();
    same_block("mode", m[0], m[1], 0x24);

    // The display device.
    g_op = "display device constructor";
    void* d[2] = {block(0x34), block(0x34)};
    at<Ctor>(0x00032200)(d[0], nullptr);
    new (d[1]) AtDisplayDevice;
    same_block("device", d[0], d[1], 0x34);
    g_op = "display device reset";
    garble(d[0], 0x34, 11);
    garble(d[1], 0x34, 11);
    at<Ctor>(((uint32_t*)vptr(d[0]))[1])(d[0], nullptr);
    ((AtDisplayDevice*)d[1])->Slot1();
    same_block("device", d[0], d[1], 0x34);
    g_op = "display device destructor";
    garble(d[0], 0x34, 13);
    garble(d[1], 0x34, 13);
    at<Ctor>(0x000321A0)(d[0], nullptr);
    ((AtDisplayDevice*)d[1])->~AtDisplayDevice();
    same_block("device", d[0], d[1], 0x34);
    g_op = "display device deleting destructor";
    for (int side = 0; side < 2; side++) {
        On on(side);
        void* p = operator new(0x34);
        if (side == 0) {
            at<Ctor>(0x00032200)(p, nullptr);
            at<void(__fastcall*)(void*, void*, unsigned)>(((uint32_t*)vptr(p))[0])(p, nullptr, 1);
        } else {
            (new (p) AtDisplayDevice)->Slot0();
        }
    }
    compare_logs("log");

    // The owner of the name tables.
    g_op = "owner constructor";
    void* w[2] = {block(0x3A8), block(0x3A8)};
    for (int side = 0; side < 2; side++) {
        On on(side);
        if (side == 0) at<Ctor>(0x00026190)(w[0], nullptr);
        else new (w[1]) AtMaterialOwner;
    }
    compare_logs("log");
    same_block("owner", w[0], w[1], 0x3A8);
    // Reset, the destructor and the deleting destructor, each on an owner with every name space database holding a pool, and garbage in the words and groups.
    for (int what = 0; what < 3; what++) {
        static const char* kWhat[] = {"owner reset", "owner destructor", "owner deleting destructor"};
        g_op = kWhat[what];
        int heap;
        GmNameSpaceDb::s_heap = &heap;
        GmNameMap::s_heap = &heap;
        void* p[2];
        for (int side = 0; side < 2; side++) {
            On on(side);
            p[side] = what == 2 ? operator new(0x3A8) : w[side];
            if (what == 2) {
                if (side == 0) at<Ctor>(0x00026190)(p[0], nullptr);
                else new (p[1]) AtMaterialOwner;
            }
            AtMaterialOwner* o = (AtMaterialOwner*)p[side];
            GmNameSpaceDb* dbs[] = {&o->m_textureSpaces, &o->m_materialSpaces, &o->m_shaderSpaces, &o->m_meshSpaces, &o->m_unknown12CSpaces, &o->m_lightSpaces, &o->m_matAssignmentSpaces,
                                    &o->m_matAnimationSpaces, &o->m_morphAnimationSpaces, &o->m_fontSpaces, &o->m_imageSpaces, &o->m_lightAnimationSpaces};
            unsigned k = 2;
            for (GmNameSpaceDb* db : dbs) db->GmNameSpaceDb::Init(k++);
            GmNameMapGroup* groups[] = {&o->m_textures, &o->m_materials, &o->m_shaders, &o->m_meshes, &o->m_unknown12C, &o->m_lights, &o->m_matAssignments, &o->m_matAnimations,
                                        &o->m_morphAnimations, &o->m_fonts, &o->m_images, &o->m_lightAnimations, &o->m_coordSystems, &o->m_mirrors, &o->m_weights, &o->m_animations,
                                        &o->m_vertexProcesses, &o->m_pixelProcesses};
            uint32_t seed = 99;
            for (GmNameMapGroup* g : groups) garble((char*)g - 4, 4 + sizeof(GmNameMapGroup), seed++);
            garble(o, 0x34, 5);  // the base's words
            o->m_textureLimits = (AtGrD3DTextureLimits*)0x34;
            o->m_unknown39C = 0x39C;
            o->m_unknown3A0 = 0x3A0;
            o->m_unknown3A4 = 0x3A4;
        }
        for (int side = 0; side < 2; side++) {
            On on(side);
            AtMaterialOwner* o = (AtMaterialOwner*)p[side];
            if (side == 0) {
                const uint32_t* t = (const uint32_t*)vptr(o);
                if (what == 0) at<Ctor>(t[1])(o, nullptr);
                else if (what == 1) at<Ctor>(0x000266F0)(o, nullptr);
                else at<void(__fastcall*)(void*, void*, unsigned)>(t[0])(o, nullptr, 1);
            } else {
                if (what == 0) o->Slot1();
                else if (what == 1) o->~AtMaterialOwner();
                else o->Slot0();
            }
        }
        compare_logs("log");
        if (what < 2) same_block("owner", w[0], w[1], 0x3A8);
        if (what == 1) {
            for (int side = 0; side < 2; side++) { std::memset(w[side], kFill, 0x3A8); }
            at<Ctor>(0x00026190)(w[0], nullptr);
            new (w[1]) AtMaterialOwner;
        }
    }
}

// ---- the device -----------------------------------------------------------------------------------------------------------
static const uint32_t kDeviceSize = 0x305C0;
static AtGraphicsDeviceXbox* g_device[2];
#define DEV(side) ((AtMaterialOwner*)g_device[side])

static uint32_t off(int side, const void* p) { return (uint32_t)((const char*)p - (const char*)g_device[side]); }

static void test_device() {
    g_op = "device constructor";
    uint32_t* shared = (uint32_t*)(uintptr_t)0x002C9BF8;
    shared[0] = AtGfxContextBase::s_shared[0] = 0x11111111;
    shared[1] = AtGfxContextBase::s_shared[1] = 0x22222222;
    shared[2] = AtGfxContextBase::s_shared[2] = 0x33333333;
    for (int side = 0; side < 2; side++) {
        void* p = block(kDeviceSize);
        On on(side);
        if (side == 0) {
            at<Ctor>(0x000F1FA0)(p, nullptr);
            g_device[0] = (AtGraphicsDeviceXbox*)p;
        } else {
            g_device[1] = new (p) AtGraphicsDeviceXbox;
        }
    }
    compare_logs("log");
    same_block("device", g_device[0], g_device[1], kDeviceSize);
    for (int i = 0; i < 3; i++) eq("shared words", shared[i], (uint32_t)AtGfxContextBase::s_shared[i]);

    // The slots, through the base, on each side's own vftable.
    g_op = "device slots";
    for (int side = 0; side < 2; side++) {
        AtMaterialOwner* o = DEV(side);
        On on(side);
        g_log[side].add('3', off(side, o->RenderTarget0()));
        g_log[side].add('4', off(side, o->RenderTarget1()));
        g_log[side].add('5', off(side, o->Context()));
        g_log[side].add('6', off(side, o->DrawLists()));
        g_log[side].add('7', off(side, o->MatrixDevice()));
        g_log[side].add('8', (uint32_t)(uintptr_t)o->GraphicsFactory());
        g_log[side].add('L', off(side, o->BufferSet()));
        g_log[side].add('W', o->Width());
        g_log[side].add('H', o->Height());
        g_device[side]->m_creator = (void*)(uintptr_t)0xC0FFEE00;
        g_device[side]->m_mode.m_width = 0x280;
        g_device[side]->m_mode.m_height = 0x1E0;
        g_log[side].add('8', (uint32_t)(uintptr_t)o->GraphicsFactory());
        g_log[side].add('W', o->Width());
        g_log[side].add('H', o->Height());
        g_device[side]->m_mode.m_width = 0xFFFFFFFF;
        g_device[side]->m_mode.m_height = 7;
        g_log[side].add('W', o->Width());
        g_log[side].add('H', o->Height());
        float s[3] = {-3.0f, -4.0f, -5.0f};
        o->DrawScale(s);
        for (float f : s) { uint32_t u; std::memcpy(&u, &f, 4); g_log[side].add('S', u); }
        struct Released : AtGfxObject {
            void Slot0() override { g_log[g_side].add('0'); }
            void Release() override { g_log[g_side].add('1'); }
        } r;
        o->ReleaseObject(&r);
    }
    compare_logs("log");

    // Taking a context out of the list at +0x3334: the device's own and four more, in several orders, and one not in it.
    g_op = "unlink a context";
    static const int kLists[][6] = {{0, 1, 2, 3, -1}, {2, 0, 3, 1, 4, -1}, {4, -1}, {-1}, {3, 2, -1}};
    for (const auto& list : kLists) {
        for (int victim = 0; victim < 5; victim++) {
            for (int side = 0; side < 2; side++) {
                static unsigned char extra[2][4][0x2A80];
                AtGfxContext* c[5] = {&g_device[side]->m_context, (AtGfxContext*)extra[side][0], (AtGfxContext*)extra[side][1], (AtGfxContext*)extra[side][2], (AtGfxContext*)extra[side][3]};
                AtGfxContext** link = &g_device[side]->m_contexts;
                for (int i = 0; list[i] >= 0; i++) { *link = c[list[i]]; link = &c[list[i]]->m_next; }
                *link = nullptr;
                On on(side);
                if (side == 0) at<void(__fastcall*)(void*, void*, void*)>(0x000EF060)(g_device[0], nullptr, c[victim]);
                else g_device[1]->UnlinkContext(c[victim]);
                for (AtGfxContext* p = g_device[side]->m_contexts; p; p = p->m_next) {
                    int k = 0;
                    while (k < 5 && c[k] != p) k++;
                    g_log[side].add('c', (uint32_t)k);
                }
            }
            compare_logs("list");
        }
    }
    g_device[0]->m_contexts = nullptr;
    g_device[1]->m_contexts = nullptr;
}

// ---- the members' freeing -------------------------------------------------------------------------------------------------
static void test_members() {
    // A context's destructor, in and out of its device's list.
    for (int inList = 0; inList < 3; inList++) {
        // 0: in the list, but +0x10 says not, so it stays. 1: in the middle. 2: first.
        g_op = inList == 0 ? "context destructor, not marked as in a list" : inList == 1 ? "context destructor, in the middle" : "context destructor, first";
        uint32_t* shared = (uint32_t*)(uintptr_t)0x002C9BF8;
        void* c[2];
        for (int side = 0; side < 2; side++) {
            c[side] = block(0x2A80);
            if (side == 0) at<Ctor>(0x00107F50)(c[0], nullptr);
            else new (c[1]) AtGfxContext;
            garble(c[side], 0x2A80, 300 + inList);
            AtGfxContext* x = (AtGfxContext*)c[side];
            x->m_device = g_device[side];
            x->m_unknown10 = inList ? 1 : 0;
            if (inList == 1) { g_device[side]->m_contexts = &g_device[side]->m_context; g_device[side]->m_context.m_next = x; x->m_next = nullptr; }
            if (inList == 2) { g_device[side]->m_contexts = x; x->m_next = &g_device[side]->m_context; g_device[side]->m_context.m_next = nullptr; }
            if (inList == 0) { g_device[side]->m_contexts = &g_device[side]->m_context; g_device[side]->m_context.m_next = x; x->m_next = nullptr; }
        }
        shared[0] = AtGfxContextBase::s_shared[0] = 5;
        for (int side = 0; side < 2; side++) {
            On on(side);
            if (side == 0) at<Ctor>(0x001099B0)(c[0], nullptr);
            else ((AtGfxContext*)c[1])->~AtGfxContext();
        }
        compare_logs("log");
        same_block("context", c[0], c[1], 0x2A80);
        eq("shared", shared[0], (uint32_t)AtGfxContextBase::s_shared[0]);
        eq("list head", off(0, g_device[0]->m_contexts), off(1, g_device[1]->m_contexts));
        eq("list next", g_device[0]->m_context.m_next ? 1 : 0, g_device[1]->m_context.m_next ? 1 : 0);
        g_device[0]->m_contexts = g_device[1]->m_contexts = nullptr;
        g_device[0]->m_context.m_next = g_device[1]->m_context.m_next = nullptr;
    }

    // The context base's destructor alone, on garbage: its one caller resets first, which would hide it.
    {
        g_op = "context base destructor";
        uint32_t* shared = (uint32_t*)(uintptr_t)0x002C9BF8;
        void* c[2] = {block(0x2950), block(0x2950)};
        at<Ctor>(0x00059500)(c[0], nullptr);
        new (c[1]) AtGfxContextBase;
        for (int side = 0; side < 2; side++) garble(c[side], 0x2950, 400);
        shared[1] = AtGfxContextBase::s_shared[1] = 9;
        for (int side = 0; side < 2; side++) {
            On on(side);
            if (side == 0) at<Ctor>(0x00059540)(c[0], nullptr);
            else ((AtGfxContextBase*)c[1])->~AtGfxContextBase();
        }
        compare_logs("log");
        same_block("context base", c[0], c[1], 0x2950);
        eq("shared", shared[1], (uint32_t)AtGfxContextBase::s_shared[1]);
    }

    // The state cache: constructor, Reset, ResetState and the destructor, with and without a block.
    struct { const char* name; uint32_t va; int kind; } kCache[] = {{"state cache constructor", 0x000FB8C0, 0}, {"state cache reset", 0x000FB710, 1}, {"state cache reset state", 0x000FB380, 2},
                                                                    {"state cache destructor", 0x000FBA40, 3}, {"state cache destructor, no block", 0x000FBA40, 4}};
    for (auto& k : kCache) {
        g_op = k.name;
        void* s[2] = {block(0xAA4), block(0xAA4)};
        for (int side = 0; side < 2; side++) {
            On on(side);
            if (k.kind) {
                garble(s[side], 0xAA4, 77);
                ((uint32_t*)s[side])[0] = 0x900D;
                if (k.kind == 1 || k.kind == 3) ((AtGfxStateCache*)s[side])->m_blockA58 = tagged();
                if (k.kind == 4) ((AtGfxStateCache*)s[side])->m_blockA58 = nullptr;
            }
            if (side == 0) at<Ctor>(k.va)(s[0], nullptr);
            else if (k.kind == 0) new (s[1]) AtGfxStateCache;
            else if (k.kind == 1) ((AtGfxStateCache*)s[1])->Reset();
            else if (k.kind == 2) ((AtGfxStateCache*)s[1])->ResetState();
            else ((AtGfxStateCache*)s[1])->~AtGfxStateCache();
        }
        compare_logs("log");
        same_block("cache", s[0], s[1], 0xAA4);
    }

    // The draw lists: Clear through slot 2, and the destructor, with each block present or not.
    for (int mask = 0; mask < 8; mask++) {
        for (int dtor = 0; dtor < 2; dtor++) {
            g_op = dtor ? "draw lists destructor" : "draw lists clear";
            void* l[2] = {block(0x2C), block(0x2C)};
            for (int side = 0; side < 2; side++) {
                if (side == 0) at<Ctor>(0x000ED6A0)(l[0], nullptr);
                else new (l[1]) AtGfxDrawLists;
            }
            same_block("lists constructed", l[0], l[1], 0x2C);
            for (int side = 0; side < 2; side++) {
                On on(side);
                garble(l[side], 0x2C, 40 + mask);
                AtGfxDrawLists* x = (AtGfxDrawLists*)l[side];
                x->m_block0C = mask & 1 ? tagged() : nullptr;
                x->m_block18 = mask & 2 ? tagged() : nullptr;
                x->m_block24 = mask & 4 ? tagged() : nullptr;
                if (side == 0) {
                    if (dtor) at<Ctor>(0x000EE210)(l[0], nullptr);
                    else at<Ctor>(((uint32_t*)vptr(l[0]))[2])(l[0], nullptr);
                } else {
                    if (dtor) x->~AtGfxDrawLists();
                    else ((AtGfxDrawListsBase*)x)->Clear();
                }
            }
            compare_logs("log");
            same_block("lists", l[0], l[1], 0x2C);
        }
    }

    // The two lists of blocks, and the pair.
    for (int mask = 0; mask < 32; mask++) {
        for (int which = 0; which < 4; which++) {
            static const char* kNames[] = {"block list clear", "indexed list clear", "indexed list destructor", "block pair clear"};
            g_op = kNames[which];
            const uint32_t size = which == 0 ? 0x14 : which == 3 ? 0x10 : 0x24;
            void* l[2] = {block(size), block(size)};
            if (which == 3) { at<Ctor>(0x0017AED0)(l[0], nullptr); new (l[1]) AtGfxBlockPair; }
            else if (which == 0) { at<Ctor>(0x000FE520)(l[0], nullptr); new (l[1]) AtGfxBlockList; }
            else { at<Ctor>(0x000ED2C0)(l[0], nullptr); new (l[1]) AtGfxIndexedList; }
            same_block("constructed", l[0], l[1], size);
            for (int side = 0; side < 2; side++) {
                On on(side);
                garble((char*)l[side] - 4, size + 4, 60 + mask);  // garble skips the first word: this way it covers them all
                void** words = (void**)l[side];
                const int blocks[] = {0, 2, 4, 5, 7};  // the words that hold blocks: +0x00 +0x08 +0x10 +0x14 +0x1C
                for (int b = 0; b < 5; b++) {
                    if (blocks[b] * 4 >= (int)size) break;
                    words[blocks[b]] = mask & (1 << b) ? tagged() : nullptr;
                }
                if (which == 3) { if (side == 0) at<Ctor>(0x000F56B0)(l[0], nullptr); else ((AtGfxBlockPair*)l[1])->Clear(); }
                else if (which == 0) { if (side == 0) at<Ctor>(0x000FE540)(l[0], nullptr); else ((AtGfxBlockList*)l[1])->Clear(); }
                else if (which == 1) { if (side == 0) at<Ctor>(0x000ED2E0)(l[0], nullptr); else ((AtGfxIndexedList*)l[1])->Clear(); }
                else { if (side == 0) at<Ctor>(0x000ED470)(l[0], nullptr); else ((AtGfxIndexedList*)l[1])->~AtGfxIndexedList(); }
            }
            compare_logs("log");
            same_block("list", l[0], l[1], size);
        }
    }

    // The ordering of the sorted pools.
    g_op = "ordering";
    const uint32_t kBits[] = {0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x3F800001, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00000, 0x7F800001, 0x00000001, 0x80000001, 0x7F7FFFFF, 0x4B000000};
    for (uint32_t x : kBits) {
        for (uint32_t y : kBits) {
            uint32_t a[5] = {1, 2, 3, 4, x}, b[5] = {5, 6, 7, 8, y};
            eq("less", (uint32_t)at<int(__cdecl*)(const void*, const void*)>(0x000EF380)(a, b), (uint32_t)AtGfxLessAt10(a, b));
        }
    }
}

// ---- real .AKC files, through each device's slot 17 -----------------------------------------------------------------------
class FileStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

static unsigned char g_heap_storage[sizeof(AtHeap)];
static AtHeap* const g_poolHeap = (AtHeap*)(void*)g_heap_storage;
static int g_devHeap;

struct FileTotals { unsigned files, entries, matrices, logged; };

static void run_akc_file(const std::vector<unsigned char>& data, FileTotals* totals) {
    std::vector<float> out[2];
    unsigned counts[2] = {}, entries[2] = {};
    for (int side = 0; side < 2; side++) {
        std::memset(g_heap_storage, 0, sizeof g_heap_storage);
        g_poolHeap->m_alignShift = 4;
        g_poolHeap->m_allocShift = 4;
        std::string text((const char*)data.data(), data.size());
        FileStream stream;
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        AtIoTextTokenizer tokenizer;
        tokenizer.SetStream(&stream);
        On on(side);
        AtMatrixDevice* device = DEV(side)->MatrixDevice();  // the original's own slot 17 on side 0
        g_log[side].add('m', off(side, device));
        {
            AtCoordSysDatabase db;
            db.Read(device, &tokenizer);
            AtMatrixPoolXbox* pool = (AtMatrixPoolXbox*)db.m_pool;
            counts[side] = pool ? pool->m_count : 0;
            entries[side] = db.m_rotTransCount + db.m_scaledCount;
            AtCoordSys* all[2] = {db.m_rotTrans, db.m_scaled};
            const unsigned n[2] = {db.m_rotTransCount, db.m_scaledCount};
            const unsigned size[2] = {sizeof(AtRotTransCoordSys), sizeof(AtScaleCoordSys)};
            for (int kind = 0; kind < 2; kind++) {
                for (unsigned i = 0; i < n[kind]; i++) {
                    AtCoordSys* cs = (AtCoordSys*)((char*)all[kind] + i * size[kind]);
                    cs->UpdateWorld();
                    cs->UpdateInverse();
                    out[side].insert(out[side].end(), cs->m_world->m, cs->m_world->m + 16);
                }
            }
            for (unsigned i = 0; pool && pool->m_matrices && i < pool->m_count; i++) out[side].insert(out[side].end(), pool->m_matrices[i].m, pool->m_matrices[i].m + 16);
            tokenizer.SetStream(nullptr);
            stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        }
    }
    eq("matrices", counts[0], counts[1]);
    eq("coordinate systems", entries[0], entries[1]);
    eq("outputs", (uint32_t)out[0].size(), (uint32_t)out[1].size());
    if (out[0].size() == out[1].size()) {
        for (size_t i = 0; i < out[0].size(); i++) {
            g_checks++;
            uint32_t a, b;
            std::memcpy(&a, &out[0][i], 4);
            std::memcpy(&b, &out[1][i], 4);
            if (a != b) { eq("matrix word", a, b); break; }
        }
    }
    totals->logged += (unsigned)g_log[0].n / 2;
    compare_logs("calls to the matrix device's heaps and reports");
    totals->files++;
    totals->entries += entries[0];
    totals->matrices += counts[0];
}

// ---- the vftable map ------------------------------------------------------------------------------------------------------
template <class T> static uint32_t vtable_of() {
    static unsigned char room[sizeof(T) + 64];
    T* t = new (room) T;
    return vptr(t);
}

static void build_maps() {
    g_vtables[vtable_of<AtDisplayDevice>()] = 0x00274AD4;
    g_vtables[vtable_of<AtMaterialOwner>()] = 0x00273030;
    g_vtables[vtable_of<GmNameSpaceDb>()] = 0x0027211C;
    g_vtables[vtable_of<GmNameMap>()] = 0x00272E60;
    g_vtables[vtable_of<AtGfxContextBase>()] = 0x00277168;
    g_vtables[vtable_of<AtGfxContext>()] = 0x0028A080;
    g_vtables[vtable_of<AtGfxDrawListsBase>()] = 0x00288E78;
    g_vtables[vtable_of<AtGfxDrawLists>()] = 0x00288EC0;
    g_vtables[vtable_of<AtGeometryArray>()] = 0x002762B8;
    g_vtables[vtable_of<AtGrD3DIndexArray>()] = 0x00289358;
    g_vtables[vtable_of<AtGfxBufferSet>()] = 0x002893DC;
    g_vtables[vtable_of<AtGrD3DMeshDatabase>()] = 0x0028A730;
    g_vtables[vtable_of<AtGrD3DSurface2D>()] = 0x00289768;
    g_vtables[vtable_of<AtGrD3DPalette>()] = 0x00289274;
    g_vtables[vtable_of<AtGrD3DTexture2D>()] = 0x00289700;
    g_vtables[vtable_of<AtGrD3DTextureDatabase>()] = 0x0028A6E0;
    g_vtables[vtable_of<AtGfxResource>()] = 0x0028B3C0;
    g_vtables[vtable_of<AtGfxResourceBlock>()] = 0x00289A6C;
    g_vtables[vtable_of<AtGfxResourceXbox>()] = 0x0028AFC4;
    g_vtables[vtable_of<AtMatrixPoolXbox>()] = 0x002892E4;
    g_vtables[vtable_of<AtMatrixDeviceXbox>()] = 0x00288B48;
    // The device's own: one made here, its texture work's members by the original's constructors through the seams above.
    void* d = block(kDeviceSize);
    g_vtables[vptr(new (d) AtGraphicsDeviceXbox)] = 0x00289208;
    g_statics[(uint32_t)(uintptr_t)AtGfxContextBase::s_shared] = 0x002C9BF8;
    g_statics[(uint32_t)(uintptr_t)&AtGfxLessAt10] = 0x000EF380;
    g_log[0].n = g_log[1].n = 0;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() < 2) { std::printf("usage: oracle_gfxdev <default.xbe> <text cases> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011170, (const void*)o_raise);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    if (entries && !arm_entries(entries)) return 2;
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.
    build_maps();

    test_layers();
    std::printf("abstract layers: done\n");
    test_device();
    eq("device vftable", vptr(g_device[0]), 0x00289208);
    std::printf("device: done\n");
    test_members();
    std::printf("members: done\n");

    int nameHeap = 0;
    GmNameMap::s_heap = &nameHeap;
    GmNameSpaceDb::s_heap = &nameHeap;
    AtCoordSysDatabase::s_heap = &nameHeap;
    AtIoStream::s_heap = &nameHeap;
    AtIoTokenFile::s_heap = &nameHeap;
    at<void(__cdecl*)(void*)>(0x000F7BD0)(&g_devHeap);
    at<void(__cdecl*)(void*)>(0x000F5FC0)(g_poolHeap);
    AtMatrixDeviceXbox::SetHeap(&g_devHeap);
    AtMatrixPoolXbox::SetHeap(g_poolHeap);
    FileTotals totals = {};
    const std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AKC\t")) continue;
        *std::strchr(line, '\t') = 0;
        g_op = line;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty() && std::fread(data.data(), 1, data.size(), f) != data.size()) { std::fclose(f); return 2; }
        std::fclose(f);
        run_akc_file(data, &totals);
    }
    std::fclose(index);
    std::printf(".AKC files read through each device's slot 17, the original's and the recovered: %u files, %u coordinate systems, %u matrices, %u calls logged\n", totals.files, totals.entries,
                totals.matrices, totals.logged);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
