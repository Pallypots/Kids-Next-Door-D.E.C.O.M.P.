// Differential oracle for the graphics factory's start (subsystem at/grfactory; Windows x86, local only). See docs/verification.md.
//
//   oracle_grfactory <default.xbe> [--entries FILE] [--mutate]
//
// No game file is read. What stands in for what, first: Direct3D is a double on both sides. The original's six entry points (0x00206070,
// 0x00206080, 0x002060B0, 0x00206150, 0x002062B0, 0x002063F0) are redirected to doubles, and the recovered side's AtD3D* are the same doubles; they
// answer from a script each case writes (identifier names, whether the caps call succeeds, the caps, the modes and which formats the device takes).
// So this shows what the factory does with what Direct3D reports, not what Direct3D reports on an Xbox. The graphics device that slot 3 makes is the
// original's on one side and the recovered (at/gfxdev) on the other, its texture members by the original's constructors on both (as oracle_gfxdev
// does); only the call log and the creator word are compared here, the device itself being gfxdev's suite's. What runs for real on both sides: every
// function recorded for this subsystem, the texts (at/text), the pixel format (at/pixelformat), the saved globals (at/grlayer, at/atap) and the matrix
// functions, by address only (the tables are compared, the functions not called). The heap stack and fatal paths are doubles (oracle_doubles.h).
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtDatabaseLoader.h"
#include "at/AtGraphicsDevice.h"
#include "at/AtGraphicsFactory.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtIoJam.h"
#include "at/AtMatrix.h"
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

static uint32_t g_rng = 24680;
static uint32_t rnd() { g_rng = g_rng * 1103515245u + 12345u; return g_rng >> 8; }
static int g_bad;
static void L(uint32_t tag, uint32_t v = 0) {
    if (tag == 'BAD') g_bad++;
    if (g_recording) g_log[g_side].add(tag, v);
}

// What the recovered code linked here needs of the platform; no case reaches it.
void* AtPlatformAllocateRegion(unsigned int) { L('BAD', 9800); return nullptr; }
void AtPlatformFreeRegion(void*) { L('BAD', 9801); }
void AtPlatformInitDevices() { L('BAD', 9802); }
void AtLibraryFree(void*) { L('BAD', 9803); }
int AtPlatformLanguage() { L('BAD', 9804); return 1; }
int AtPlatformGameRegion() { L('BAD', 9805); return 1; }
unsigned int AtPlatformMilliseconds() { L('BAD', 9806); return 0; }
AtHeap* AtCurrentHeap() { return nullptr; }

// The device's texture members, by the original's constructors on the recovered side too (as oracle_gfxdev); its freeing members never run here.
typedef void(__fastcall* Ctor)(void*, void*);
void AtGfxConstructTextureA(void* p) { at<Ctor>(0x000FCEE0)(p, nullptr); }
void AtGfxDestroyTextureA(void*) { L('BAD', 9810); }
void AtGfxConstructTexture2D(void* p) { at<Ctor>(0x000FC630)(p, nullptr); }
void AtGfxDestroyTexture2D(void*) { L('BAD', 9811); }
void AtGfxConstructTextureDatabase(void* p) { at<Ctor>(0x0010FFC0)(p, nullptr); }
void AtGfxDestroyTextureDatabase(void*) { L('BAD', 9812); }
AtIndexArrayXbox::~AtIndexArrayXbox() { L('BAD', 9813); }
AtGfxBufferSet::~AtGfxBufferSet() { L('BAD', 9814); }
AtMeshDatabaseXbox::~AtMeshDatabaseXbox() { L('BAD', 9815); }
AtGfxResourceXbox::~AtGfxResourceXbox() { L('BAD', 9816); }

// The render table's functions and the adapter list's two slots not recovered (src/at/AtGraphicsFactorySeams.cpp is not linked): only their addresses
// are compared, through the map below.
void AtRenderFunction0() { L('BAD', 9820); }
void AtRenderFunction1() { L('BAD', 9821); }
void AtRenderFunction3() { L('BAD', 9823); }
void AtRenderFunction4() { L('BAD', 9824); }
void AtRenderFunction5() { L('BAD', 9825); }
void AtRenderFunction6() { L('BAD', 9826); }
AtGfxAdapter* AtGfxAdapterList::Best() { L('BAD', 9827); return nullptr; }
AtGfxAdapter* AtGfxAdapterList::Find(const AtD3DAdapterIdentifier*) { L('BAD', 9828); return nullptr; }

// ---- Direct3D, as doubles both sides share --------------------------------------------------------------------------------
static unsigned char g_token[64];
#define D3D_TOKEN ((void*)g_token)
struct Script {
    char driver[512];
    char description[512];
    uint32_t idWords[11];        // the identifier's words after the names
    int idResult;
    int capsResult;
    AtD3DCaps caps;
    unsigned int modeCount;
    AtD3DDisplayMode modes[40];
    uint32_t rejectMask;         // CheckDeviceType fails for a format f if bit (f & 31) is set
};
static Script g_script;

static void* d3d_create(unsigned int v) { L('dcr', v); return D3D_TOKEN; }
static int d3d_identifier(unsigned int adapter, unsigned int flags, AtD3DAdapterIdentifier* out) {
    L('did', adapter);
    L('didf', flags);
    std::memcpy(out->m_driver, g_script.driver, 512);
    std::memcpy(out->m_description, g_script.description, 512);
    std::memcpy(out->m_driverVersion, g_script.idWords, sizeof g_script.idWords);
    return g_script.idResult;
}
static int d3d_caps(unsigned int adapter, unsigned int type, AtD3DCaps* out) {
    L('dcp', adapter);
    L('dcpt', type);
    if (g_script.capsResult == 0) *out = g_script.caps;
    return g_script.capsResult;
}
static unsigned int d3d_mode_count(unsigned int adapter) { L('dmc', adapter); return g_script.modeCount; }
static int d3d_enum(unsigned int adapter, unsigned int mode, AtD3DDisplayMode* out) {
    L('den', adapter);
    L('denm', mode);
    *out = g_script.modes[mode % 40];
    return 0;
}
static int d3d_check(unsigned int adapter, unsigned int type, unsigned int display, unsigned int back, int windowed) {
    L('dck', adapter);
    L('dckt', type);
    L('dckd', display);
    L('dckb', back);
    L('dckw', windowed);
    return (g_script.rejectMask >> (display & 31)) & 1 ? (int)0x8876086A : 0;
}
static void* __stdcall o_create(unsigned int v) { return d3d_create(v); }
static int __stdcall o_identifier(unsigned int a, unsigned int f, AtD3DAdapterIdentifier* o) { return d3d_identifier(a, f, o); }
static int __stdcall o_caps(unsigned int a, unsigned int t, AtD3DCaps* o) { return d3d_caps(a, t, o); }
static unsigned int __stdcall o_mode_count(unsigned int a) { return d3d_mode_count(a); }
static int __stdcall o_enum(unsigned int a, unsigned int m, AtD3DDisplayMode* o) { return d3d_enum(a, m, o); }
static int __stdcall o_check(unsigned int a, unsigned int t, unsigned int d, unsigned int b, int w) { return d3d_check(a, t, d, b, w); }
void* AtD3DCreate(unsigned int v) { return d3d_create(v); }
int AtD3DGetAdapterIdentifier(unsigned int a, unsigned int f, AtD3DAdapterIdentifier* o) { return d3d_identifier(a, f, o); }
int AtD3DGetDeviceCaps(unsigned int a, unsigned int t, AtD3DCaps* o) { return d3d_caps(a, t, o); }
unsigned int AtD3DGetAdapterModeCount(unsigned int a) { return d3d_mode_count(a); }
int AtD3DEnumAdapterModes(unsigned int a, unsigned int m, AtD3DDisplayMode* o) { return d3d_enum(a, m, o); }
int AtD3DCheckDeviceType(unsigned int a, unsigned int t, unsigned int d, unsigned int b, int w) { return d3d_check(a, t, d, b, w); }

// Formats from both of the original's tables, and some that are in neither.
static const uint32_t kFormats[] = {0x06, 0x07, 0x05, 0x27, 0x03, 0x02, 0x04, 0x19, 0x3A, 0x3B, 0x39, 0x38, 0x3C, 0x29, 0x28, 0x0B, 0x00, 0x1A, 0x01, 0x32,
                                    0x33, 0x2A, 0x2C, 0x0C, 0x0E, 0x0F, 0x10, 0x1D, 0x1F, 0x3F, 0x12, 0x40, 0x17, 0x3E, 0x3D, 0x11, 0x37, 0x16, 0x41, 0x1C,
                                    0x1E, 0x20, 0x1B, 0x35, 0x13, 0x2E, 0x30, 0x08, 0x09, 0x14, 0x21, 0x42, 0x50, 0x7F, 0xFFFFFFFF};
static const int kFormatCount = sizeof kFormats / sizeof kFormats[0];

static void random_name(char* to, int pass) {
    std::memset(to, 0x5A, 512);  // past the terminator: never read
    const int n = pass % 5 == 0 ? 0 : (int)(rnd() % (pass % 7 == 0 ? 511 : 40));
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)(0x20 + rnd() % 0x5F);
        if (rnd() % 6 == 0) c = (unsigned char)(0x80 + rnd() % 0x7F);  // high bytes: widened with their sign
        to[i] = (char)c;
    }
    to[n] = 0;
}
static void random_script(int pass) {
    random_name(g_script.driver, pass);
    random_name(g_script.description, pass + 3);
    for (uint32_t& w : g_script.idWords) w = rnd() * 7;
    g_script.idResult = (pass % 4 == 3) ? (int)0x8876086C : 0;
    g_script.capsResult = (pass % 5 == 4) ? (int)0x8876086A : 0;
    uint32_t* c = (uint32_t*)&g_script.caps;
    for (int i = 0; i < 0x35; i++) c[i] = (rnd() << 8) ^ rnd();
    if (pass % 3 == 0) g_script.caps.m_extentsAdjust = 0.0f;
    else if (pass % 3 == 1) g_script.caps.m_extentsAdjust = (pass & 4) ? -0.0f : 1.5f;
    else c[0x84 / 4] = 0x7FC00001u;  // a NaN
    if (pass % 8 == 2) {  // the shader versions with their low halves zero
        g_script.caps.m_vertexShaderVersion &= 0xFFFF0000u;
        g_script.caps.m_pixelShaderVersion &= 0xFFFF0000u;
    }
    if (pass % 8 == 6) {  // the vertex shader's low half zero, the pixel shader's not
        g_script.caps.m_vertexShaderVersion &= 0xFFFF0000u;
        g_script.caps.m_pixelShaderVersion |= 0x0101u;
    }
    if (pass % 8 == 7) {  // and the other way about
        g_script.caps.m_vertexShaderVersion |= 0x0101u;
        g_script.caps.m_pixelShaderVersion &= 0xFFFF0000u;
    }
    g_script.modeCount = pass % 6 == 1 ? 0 : rnd() % 40;
    for (AtD3DDisplayMode& m : g_script.modes) {
        m.m_width = 320 + rnd() % 1600;
        m.m_height = 200 + rnd() % 1200;
        m.m_refreshRate = rnd() % 100;
        m.m_flags = rnd();
        m.m_format = kFormats[rnd() % kFormatCount];
    }
    g_script.rejectMask = pass % 4 == 0 ? 0 : rnd();
}

// ---- comparing ------------------------------------------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> g_map;  // recovered address (vftable, function, static) -> original's
static uint32_t vptr(const void* p) { return *(const uint32_t*)p; }
static void same_mapped(const char* what, uint32_t o, uint32_t r) {
    auto m = g_map.find(r);
    eq(what, o, m != g_map.end() ? m->second : r);
}
static uint32_t word(const void* base, uint32_t offset) { return *(const uint32_t*)((const unsigned char*)base + offset); }
// A pointer as where it points among a few known places of its side: 0 for null, 1 + index for one of them, 0xEE otherwise.
static uint32_t where(uint32_t p, std::initializer_list<uint32_t> known) {
    if (p == 0) return 0;
    uint32_t i = 1;
    for (uint32_t k : known) {
        if (p == k) return i;
        i++;
    }
    return 0xEE;
}
static void compare_text(const char* what, const unsigned char* o, const unsigned char* r, uint32_t oName, uint32_t rName) {
    same_mapped(what, word(o, 0), word(r, 0));
    for (uint32_t i = 4; i < 0x10; i += 4) eq(what, word(o, i), word(r, i));
    eq(what, where(word(o, 0x10), {oName}), where(word(r, 0x10), {rName}));
}
static void compare_info(const char* what, const void* ov, const void* rv, uint32_t oAdapter, uint32_t rAdapter) {
    const unsigned char* o = (const unsigned char*)ov;
    const unsigned char* r = (const unsigned char*)rv;
    std::string w = std::string(what) + " info";
    same_mapped(w.c_str(), word(o, 0), word(r, 0));
    eq(w.c_str(), where(word(o, 4), {oAdapter}), where(word(r, 4), {rAdapter}));
    for (uint32_t i = 8; i < 0x40; i += 4) eq(w.c_str(), word(o, i), word(r, i));
    compare_text((w + " text 40").c_str(), o + 0x40, r + 0x40, word(o, 0x50), word(r, 0x50));
    compare_text((w + " text 54").c_str(), o + 0x54, r + 0x54, word(o, 0x64), word(r, 0x64));
    for (uint32_t i = 0x68; i < 0x148; i += 4) eq(w.c_str(), word(o, i), word(r, i));
    const uint32_t om = word(o, 0x148), rm = word(r, 0x148);
    eq((w + " modes").c_str(), om != 0, rm != 0);
    if (om && rm) {
        const uint32_t n = word(o, 0x144) < 64 ? word(o, 0x144) : 64;
        for (uint32_t i = 0; i < n * 0x24; i += 4) eq((w + " mode").c_str(), word((void*)(uintptr_t)om, i), word((void*)(uintptr_t)rm, i));
    }
}
static void compare_wide(const char* what, uint32_t o, uint32_t r) {
    eq(what, o != 0, r != 0);
    if (!o || !r) return;
    const unsigned short* a = (const unsigned short*)(uintptr_t)o;
    const unsigned short* b = (const unsigned short*)(uintptr_t)r;
    for (int i = 0; i < 520; i++) {
        eq(what, a[i], b[i]);
        if (a[i] == 0 || b[i] == 0) break;
    }
}
static void compare_adapter(const char* what, const void* ov, const void* rv, bool names = true) {
    const unsigned char* o = (const unsigned char*)ov;
    const unsigned char* r = (const unsigned char*)rv;
    same_mapped(what, word(o, 0), word(r, 0));
    compare_text((std::string(what) + " driver text").c_str(), o + 4, r + 4, word(o, 0x844), word(r, 0x844));
    compare_text((std::string(what) + " description text").c_str(), o + 0x18, r + 0x18, word(o, 0x848), word(r, 0x848));
    eq(what, where(word(o, 0x2C), {(uint32_t)(uintptr_t)D3D_TOKEN}), where(word(r, 0x2C), {(uint32_t)(uintptr_t)D3D_TOKEN}));
    for (uint32_t i = 0x30; i < 0x460; i += 4) eq(what, word(o, i), word(r, i));
    for (int k = 0; k < 3; k++) compare_info(what, o + 0x460 + k * 0x14C, r + 0x460 + k * 0x14C, (uint32_t)(uintptr_t)o, (uint32_t)(uintptr_t)r);
    if (names) {
        compare_wide("driver name", word(o, 0x844), word(r, 0x844));
        compare_wide("description name", word(o, 0x848), word(r, 0x848));
    } else {
        eq("driver name", word(o, 0x844) != 0, word(r, 0x844) != 0);
        eq("description name", word(o, 0x848) != 0, word(r, 0x848) != 0);
    }
}
static void compare_list(const char* what, const void* ov, const void* rv) {
    same_mapped(what, word(ov, 0), word(rv, 0));
    eq(what, where(word(ov, 4), {(uint32_t)(uintptr_t)D3D_TOKEN}), where(word(rv, 4), {(uint32_t)(uintptr_t)D3D_TOKEN}));
    eq(what, word(ov, 8), word(rv, 8));
    const uint32_t oa = word(ov, 0xC), ra = word(rv, 0xC);
    eq(what, oa != 0, ra != 0);
    if (oa && ra) {
        eq("array count", word((void*)(uintptr_t)(oa - 4), 0), word((void*)(uintptr_t)(ra - 4), 0));
        const uint32_t n = word(ov, 8) < 4 ? word(ov, 8) : 4;
        for (uint32_t i = 0; i < n; i++) compare_adapter(what, (void*)(uintptr_t)(oa + i * 0x84C), (void*)(uintptr_t)(ra + i * 0x84C));
    }
}

template <class O, class R> static bool run_pair(const char* what, O orig, R rec) {
    g_context = what;
    bool c[2];
    { On on(0); c[0] = guarded(orig); }
    { On on(1); c[1] = guarded(rec); }
    eq("crashed", c[0], c[1]);
    compare_logs(what);
    if (c[0] || c[1]) std::printf("crashed in [%s]: original %d, recovered %d\n", what, (int)c[0], (int)c[1]);
    return c[0] || c[1];
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uint32_t);
typedef void(__fastcall* Fn2)(void*, void*, uint32_t, uint32_t);
typedef uint32_t(__fastcall* Get0)(void*, void*);
typedef uint32_t(__fastcall* Get1)(void*, void*, uint32_t);

static void* block(uint32_t size, int fill) {
    void* p = std::malloc(size + 16);
    std::memset(p, fill, size + 16);
    return p;
}

// The heaps the code under test pushes: tokens, the same on both sides.
static unsigned char g_heaps[8][16];
static void set_heaps(bool present) {
    void* a = present ? g_heaps[0] : nullptr;
    void* b = present ? g_heaps[1] : nullptr;
    void* c = present ? g_heaps[2] : nullptr;
    void* d = present ? g_heaps[3] : nullptr;
    *(void**)(uintptr_t)0x003B1FA8 = a; g_heap3B1FA8 = a;
    *(void**)(uintptr_t)0x003B1FAC = b; g_heap3B1FAC = b;
    *(void**)(uintptr_t)0x003B1FB4 = c; g_heap3B1FB4 = c;
    *(void**)(uintptr_t)0x003B13CC = d; g_heap3B13CC = d;
}

// ---- formats, modes and caps ---------------------------------------------------------------------------------------------
static void __declspec(naked) __cdecl o_caps_to_flags(const void*, unsigned int*) {
    __asm {
        mov ecx, [esp + 4]
        mov eax, [esp + 8]
        mov edx, 0x000FBBE0
        jmp edx
    }
}

static void run_formats() {
    for (int i = 0; i < kFormatCount + 40; i++) {
        const uint32_t f = i < kFormatCount ? kFormats[i] : (i < kFormatCount + 20 ? (uint32_t)(i - kFormatCount) * 3 : rnd());
        for (int swizzled = 0; swizzled < 3; swizzled++) {
            unsigned char out[2][0x20];
            std::memset(out, 0x77, sizeof out);
            int got[2];
            run_pair("AtD3DFormatToPixelFormat", [&] { got[0] = at<int(__cdecl*)(uint32_t, void*, int)>(0x001197D0)(f, out[0], swizzled == 2 ? 5 : swizzled); },
                     [&] { got[1] = AtD3DFormatToPixelFormat(f, (AtPixelFormat*)out[1], swizzled == 2 ? 5 : swizzled); });
            eq("format found", got[0], got[1]);
            for (int k = 0; k < 0x20; k += 4) eq("pixel format", word(out[0], k), word(out[1], k));
        }
    }
    for (int i = 0; i < 20; i++) {
        unsigned char m[2][0x28], format[0x14];
        std::memset(m, 0x66, sizeof m);
        for (int k = 0; k < 0x14; k++) format[k] = (unsigned char)rnd();
        const uint32_t a = rnd(), b = rnd(), c = rnd(), d = rnd();
        run_pair("AtDisplayMode::Set", [&] { at<void(__fastcall*)(void*, void*, uint32_t, uint32_t, const void*, uint32_t, uint32_t)>(0x00043420)(m[0], nullptr, a, b, format, c, d); },
                 [&] { ((AtDisplayMode*)m[1])->Set(a, b, (const AtPixelFormat*)format, (int)c, (int)d); });
        for (int k = 0; k < 0x28; k += 4) eq("display mode", word(m[0], k), word(m[1], k));
    }
    for (int pass = 0; pass < 60; pass++) {
        random_script(pass);
        unsigned int flags[2];
        flags[0] = flags[1] = pass % 3 == 0 ? 0 : rnd();
        run_pair("AtGfxCapsToFlags", [&] { o_caps_to_flags(&g_script.caps, &flags[0]); }, [&] { AtGfxCapsToFlags(&g_script.caps, &flags[1]); });
        eq("caps flags", flags[0], flags[1]);
    }
}

// ---- the device infos -----------------------------------------------------------------------------------------------------
static uint32_t g_infoBaseTable, g_adapterBaseTable, g_listBaseTable;  // the recovered bases' vftables, found by build_map
static const uint32_t kInfo = 0x14C;
static void make_infos(void** o, void** r, int fill) {
    *o = block(kInfo, fill);
    *r = block(kInfo, fill);
    run_pair("AtGfxDeviceInfo()", [&] { at<Fn0>(0x000FBD40)(*o, nullptr); }, [&] { new (*r) AtGfxDeviceInfo(); });
    compare_info("constructed", *o, *r, 0, 0);
}
static void fill_info_words(void* o, void* r) {
    for (uint32_t i = 8; i < 0x40; i += 4) {
        const uint32_t v = rnd() % 3 == 0 ? 0 : rnd();
        *(uint32_t*)((unsigned char*)o + i) = v;
        *(uint32_t*)((unsigned char*)r + i) = v;
    }
    const uint32_t v68 = rnd();
    *(uint32_t*)((unsigned char*)o + 0x68) = v68;
    *(uint32_t*)((unsigned char*)r + 0x68) = v68;
}

static void run_infos() {
    // A stand-in adapter for each side: only its ordinal (+0x30) is read.
    static unsigned char adapter[2][0x84C];
    for (int pass = 0; pass < 48; pass++) {
        random_script(pass);
        void* o; void* r;
        make_infos(&o, &r, pass & 1 ? 0xA5 : 0);
        fill_info_words(o, r);
        // Enumerate, once or twice (a second time clears the first).
        const int times = 1 + pass % 3;
        for (int t = 0; t < times; t++) {
            const uint32_t ordinal = rnd() % 3, type = 1 + rnd() % 3;
            *(uint32_t*)(adapter[0] + 0x30) = ordinal;
            *(uint32_t*)(adapter[1] + 0x30) = ordinal;
            g_fill = (pass & 2) ? 0x5C : 0;
            set_heaps(pass % 11 != 5);
            if (t > 0 && pass % 2 == 0) g_script.capsResult = (int)0x8876086A;
            const bool failing = pass == 13 && t == 0;
            if (failing && g_script.modeCount == 0) g_script.modeCount = 3;
            g_fail_alloc = failing;
            run_pair("AtGfxDeviceInfo::Enumerate", [&] { at<Fn2>(0x000FC320)(o, nullptr, (uint32_t)(uintptr_t)adapter[0], type); },
                     [&] { ((AtGfxDeviceInfo*)r)->Enumerate((AtGfxAdapter*)adapter[1], type); });
            g_fill = 0;
            g_fail_alloc = false;
            if (failing) break;  // the objects are left as the crash left them: not compared, not used again
            compare_info("enumerated", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
            if (t == 0 && pass % 4 == 1) random_script(pass + 100);
        }
        // ReadCaps a second time on what is there: the flags are or'ed in.
        if (pass % 5 == 0) {
            fill_info_words(o, r);
            run_pair("AtGfxDeviceInfo::ReadCaps", [&] { at<Fn0>(0x000FBF60)(o, nullptr); }, [&] { ((AtGfxDeviceInfo*)r)->ReadCaps(); });
            compare_info("caps read", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
        }
        // The slots.
        for (int s = 1; s <= 4; s++) {
            uint32_t got[2];
            const uint32_t arg = rnd();
            run_pair("AtGfxDeviceInfo slot", [&] { got[0] = at<Get1>(*(uint32_t*)(uintptr_t)(0x00289698 + 4 * s))(o, nullptr, arg); },
                     [&] {
                         AtGfxDeviceInfo* i = (AtGfxDeviceInfo*)r;
                         got[1] = s == 1 ? i->Slot1((int)arg) : s == 2 ? i->Slot2((int)arg) : s == 3 ? i->Slot3((int)arg) : i->Slot4((int)arg);
                     });
            eq("info slot", got[0], got[1]);
        }
        if (pass == 13) {
            std::free(o);
            std::free(r);
            continue;
        }
        static unsigned short texts[2][2][8];
        if (pass % 2 == 0) {
            for (int side = 0; side < 2; side++) {
                unsigned char* x = (unsigned char*)(side ? r : o);
                ((AtTextUnicode*)(x + 0x40))->Init(texts[side][0], 8);
                ((AtTextUnicode*)(x + 0x54))->Init(texts[side][1], 5);
            }
        }
        // Then one of: Clear, Reset, the destructor, the deleting destructor, the base's destructor, the base's deleting destructor.
        const int end = pass % 6;
        if (end == 0) {
            run_pair("AtGfxDeviceInfo::Clear", [&] { at<Fn0>(0x000FBD80)(o, nullptr); }, [&] { ((AtGfxDeviceInfo*)r)->Clear(); });
            compare_info("cleared", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
        } else if (end == 1) {
            run_pair("AtGfxDeviceInfoBase::Reset", [&] { at<Fn0>(0x0011C900)(o, nullptr); }, [&] { ((AtGfxDeviceInfo*)r)->Reset(); });
            compare_info("reset", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
        } else if (end == 2) {
            run_pair("~AtGfxDeviceInfo", [&] { at<Fn0>(0x000FC290)(o, nullptr); }, [&] { ((AtGfxDeviceInfo*)r)->~AtGfxDeviceInfo(); });
            compare_info("destroyed", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
        } else if (end == 3) {
            // The deleting destructor, through slot 0 of each side's own vftable, with 1: the block is freed.
            void* oo = filled(kInfo); void* rr = filled(kInfo);
            std::memcpy(oo, o, kInfo); std::memcpy(rr, r, kInfo);
            run_pair("AtGfxDeviceInfo deleting", [&] { at<Fn1>(*(uint32_t*)(uintptr_t)word(oo, 0))(oo, nullptr, 1); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)word(rr, 0))(rr, nullptr, 1); });
            continue;
        } else if (end == 4) {
            run_pair("~AtGfxDeviceInfoBase", [&] { at<Fn0>(0x0011C8E0)(o, nullptr); },
                     [&] { ((AtGfxDeviceInfoBase*)r)->AtGfxDeviceInfoBase::~AtGfxDeviceInfoBase(); });
            compare_info("base destroyed", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
        } else {
            // The base's deleting destructor, slot 0 of the base's vftable, with 0 and then (on a copy) with 1.
            run_pair("AtGfxDeviceInfoBase deleting, 0", [&] { at<Fn1>(*(uint32_t*)(uintptr_t)0x0028B1B8)(o, nullptr, 0); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)g_infoBaseTable)(r, nullptr, 0); });
            compare_info("base deleted (0)", o, r, (uint32_t)(uintptr_t)adapter[0], (uint32_t)(uintptr_t)adapter[1]);
            void* oo = filled(kInfo); void* rr = filled(kInfo);
            std::memcpy(oo, o, kInfo); std::memcpy(rr, r, kInfo);
            run_pair("AtGfxDeviceInfoBase deleting, 1", [&] { at<Fn1>(*(uint32_t*)(uintptr_t)0x0028B1B8)(oo, nullptr, 1); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)g_infoBaseTable)(rr, nullptr, 1); });
        }
        std::free(o);
        std::free(r);
    }
}

// ---- adapters ---------------------------------------------------------------------------------------------------------------
static const uint32_t kAdapter = 0x84C;
static void run_adapters() {
    for (int pass = 0; pass < 40; pass++) {
        random_script(pass);
        void* o = block(kAdapter, pass & 1 ? 0xA5 : 0);
        void* r = block(kAdapter, pass & 1 ? 0xA5 : 0);
        run_pair("AtGfxAdapter()", [&] { at<Fn0>(0x000FA9A0)(o, nullptr); }, [&] { new (r) AtGfxAdapter(); });
        compare_adapter("adapter constructed", o, r);
        const int times = pass % 4 == 3 ? 0 : 1 + pass % 2;
        for (int t = 0; t < times; t++) {
            const uint32_t ordinal = rnd() % 2;
            set_heaps(pass % 13 != 7);
            g_fill = (pass & 2) ? 0x5C : 0;
            g_fail_alloc = pass == 9 && t == 0;
            run_pair("AtGfxAdapter::Init", [&] { at<Fn2>(0x000FAA30)(o, nullptr, (uint32_t)(uintptr_t)D3D_TOKEN, ordinal); },
                     [&] { ((AtGfxAdapter*)r)->Init(D3D_TOKEN, ordinal); });
            g_fill = 0;
            if (g_fail_alloc) break;
            compare_adapter("adapter set up", o, r);
            random_script(pass + 50);
        }
        if (g_fail_alloc) {  // as the crash left it: not compared, not used again
            g_fail_alloc = false;
            continue;
        }
        if (pass % 3 == 0) {
            set_heaps(true);
            random_script(pass + 70);
            run_pair("AtGfxDeviceInfo::Enumerate, the third", [&] { at<Fn2>(0x000FC320)((char*)o + 0x6F8, nullptr, (uint32_t)(uintptr_t)o, 3); },
                     [&] { ((AtGfxAdapter*)r)->m_info[2].Enumerate((AtGfxAdapter*)r, 3); });
            compare_adapter("adapter's third info enumerated", o, r);
        }
        for (int s = 2; s <= 4; s++) {
            uint32_t got[2];
            run_pair("AtGfxAdapter slot", [&] { got[0] = at<Get0>(*(uint32_t*)(uintptr_t)(0x0028943C + 4 * s))(o, nullptr); },
                     [&] {
                         AtGfxAdapter* a = (AtGfxAdapter*)r;
                         got[1] = s == 2 ? a->HasDeviceInfo1() : s == 3 ? a->HasDeviceInfo2() : a->HasDeviceInfo3();
                     });
            eq("adapter slot", got[0], got[1]);
        }
        const int end = pass % 5;
        if (end == 0) {
            run_pair("AtGfxAdapter::Clear", [&] { at<Fn0>(0x000FABB0)(o, nullptr); }, [&] { ((AtGfxAdapter*)r)->ReleaseTexts(); });
            compare_adapter("adapter cleared", o, r, false);
        } else if (end == 1) {
            run_pair("~AtGfxAdapter", [&] { at<Fn0>(0x000FACB0)(o, nullptr); }, [&] { ((AtGfxAdapter*)r)->~AtGfxAdapter(); });
            compare_adapter("adapter destroyed", o, r, false);
        } else if (end == 2) {
            // Slot 1, the deleting destructor: with 0 on the object itself (destroyed, not freed), then with 1 on an adapter made and set up in a
            // block of the heap.
            run_pair("AtGfxAdapter deleting, 0", [&] { at<Fn1>(*(uint32_t*)(uintptr_t)(word(o, 0) + 4))(o, nullptr, 0); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)(word(r, 0) + 4))(r, nullptr, 0); });
            compare_adapter("adapter deleted (0)", o, r, false);
            void* oo = filled(kAdapter); void* rr = filled(kAdapter);
            set_heaps(true);
            run_pair("AtGfxAdapter made on the heap", [&] { at<Fn0>(0x000FA9A0)(oo, nullptr); at<Fn2>(0x000FAA30)(oo, nullptr, (uint32_t)(uintptr_t)D3D_TOKEN, 0); },
                     [&] { (new (rr) AtGfxAdapter())->Init(D3D_TOKEN, 0); });
            run_pair("AtGfxAdapter deleting", [&] { at<Fn1>(*(uint32_t*)(uintptr_t)(word(oo, 0) + 4))(oo, nullptr, 1); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)(word(rr, 0) + 4))(rr, nullptr, 1); });
        } else if (end == 3) {
            // The base's slot 0, its destructor, and its deleting destructor with 0, then 1 on a copy.
            run_pair("AtGfxAdapterBase slot 0", [&] { at<Fn0>(0x0011C880)(o, nullptr); }, [&] { ((AtGfxAdapter*)r)->AtGfxAdapterBase::ReleaseTexts(); });
            compare_adapter("adapter texts released", o, r);
            run_pair("~AtGfxAdapterBase", [&] { at<Fn0>(0x0011C870)(o, nullptr); },
                     [&] { ((AtGfxAdapterBase*)r)->AtGfxAdapterBase::~AtGfxAdapterBase(); });
            compare_adapter("adapter base destroyed", o, r);
            run_pair("AtGfxAdapterBase deleting, 0", [&] { at<Fn1>(0x0011C8C0)(o, nullptr, 0); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)(g_adapterBaseTable + 4))(r, nullptr, 0); });
            compare_adapter("adapter base deleted (0)", o, r);
            void* oo = filled(kAdapter); void* rr = filled(kAdapter);
            std::memcpy(oo, o, kAdapter); std::memcpy(rr, r, kAdapter);
            run_pair("AtGfxAdapterBase deleting, 1", [&] { at<Fn1>(0x0011C8C0)(oo, nullptr, 1); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)(g_adapterBaseTable + 4))(rr, nullptr, 1); });
        }
        std::free(o);
        std::free(r);
    }
}

// ---- the adapter list and the factory ---------------------------------------------------------------------------------------
static void run_lists() {
    for (int pass = 0; pass < 30; pass++) {
        random_script(pass);
        void* o = block(0x10, 0xA5);
        void* r = block(0x10, 0xA5);
        run_pair("AtGfxAdapterList()", [&] { at<Fn0>(0x000FA5A0)(o, nullptr); }, [&] { new (r) AtGfxAdapterList(); });
        compare_list("list constructed", o, r);
        const int times = pass % 5 == 4 ? 0 : 1 + pass % 3;
        for (int t = 0; t < times; t++) {
            set_heaps(pass % 9 != 8);
            run_pair("AtGfxAdapterList::Init", [&] { at<Fn0>(0x000FA7F0)(o, nullptr); }, [&] { ((AtGfxAdapterList*)r)->Init(); });
            compare_list("list set up", o, r);
            random_script(pass + 30 + t);
        }
        if (word(o, 0xC) != 0) {
            uint32_t got[2];
            run_pair("AtGfxAdapterList::Count", [&] { got[0] = at<Get0>(0x00154540)(o, nullptr); }, [&] { got[1] = ((AtGfxAdapterList*)r)->Count(); });
            eq("count", got[0], got[1]);
            const uint32_t index = rnd() % 3;
            run_pair("AtGfxAdapterList::At", [&] { got[0] = at<Get1>(0x000FA580)(o, nullptr, index) - word(o, 0xC); },
                     [&] { got[1] = (uint32_t)(uintptr_t)((AtGfxAdapterList*)r)->At(index) - word(r, 0xC); });
            eq("at", got[0], got[1]);
        }
        const int end = pass % 4;
        if (end == 0) {
            run_pair("AtGfxAdapterList::Clear", [&] { at<Fn0>(0x000FA5C0)(o, nullptr); }, [&] { ((AtGfxAdapterList*)r)->Clear(); });
            compare_list("list cleared", o, r);
        } else if (end == 1) {
            run_pair("~AtGfxAdapterList", [&] { at<Fn0>(0x000FA770)(o, nullptr); }, [&] { ((AtGfxAdapterList*)r)->~AtGfxAdapterList(); });
            compare_list("list destroyed", o, r);
        } else if (end == 2) {
            void* oo = filled(0x10); void* rr = filled(0x10);
            std::memcpy(oo, o, 0x10); std::memcpy(rr, r, 0x10);
            run_pair("AtGfxAdapterList deleting", [&] { at<Fn1>(0x000FA910)(oo, nullptr, 1); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)word(rr, 0))(rr, nullptr, 1); });
            std::free(o); std::free(r);
            continue;
        } else {
            // The base's destructor, then its deleting destructor with 0 and with 1 (the adapters are left behind, as the base knows nothing of them).
            run_pair("~AtGfxAdapterListBase", [&] { at<Fn0>(0x0011C840)(o, nullptr); },
                     [&] { ((AtGfxAdapterListBase*)r)->AtGfxAdapterListBase::~AtGfxAdapterListBase(); });
            same_mapped("list base vftable", word(o, 0), word(r, 0));
            run_pair("AtGfxAdapterListBase deleting, 0", [&] { at<Fn1>(0x000FA500)(o, nullptr, 0); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)g_listBaseTable)(r, nullptr, 0); });
            same_mapped("list base vftable", word(o, 0), word(r, 0));
            void* oo = filled(0x10); void* rr = filled(0x10);
            std::memcpy(oo, o, 0x10); std::memcpy(rr, r, 0x10);
            run_pair("AtGfxAdapterListBase deleting, 1", [&] { at<Fn1>(0x000FA500)(oo, nullptr, 1); },
                     [&] { at<Fn1>(*(uint32_t*)(uintptr_t)g_listBaseTable)(rr, nullptr, 1); });
        }
        std::free(o);
        std::free(r);
    }
}

// A stand-in for what slot 4 is handed: each side's own table, whose slot 0 logs. The original hands it 1 (and pops it); the recovered slot takes none,
// so the flag is not compared (the recovered side logs 1).
struct Handed { const void* const* vt; char pad[60]; };
static Handed g_handed[2];
static void __fastcall o_destroyed(void* self, void*, uint32_t flag) { L('ds0', self == &g_handed[0] || self == &g_handed[1]); L('dsf', flag); }
static void __fastcall r_destroyed(void* self, void*) { L('ds0', self == &g_handed[0] || self == &g_handed[1]); L('dsf', 1); }
static const void* g_destroyed_table[2][4] = {{(const void*)o_destroyed}, {(const void*)r_destroyed}};
struct HandedInit { HandedInit() { g_handed[0].vt = g_destroyed_table[0]; g_handed[1].vt = g_destroyed_table[1]; } } static g_handedInit;

static void run_factory() {
    for (int pass = 0; pass < 16; pass++) {
        random_script(pass);
        void* o = block(0x14, 0xA5);
        void* r = block(0x14, 0xA5);
        run_pair("AtGraphicsFactory()", [&] { at<Fn0>(0x000EBD70)(o, nullptr); }, [&] { new (r) AtGraphicsFactory(); });
        same_mapped("factory vftable", word(o, 0), word(r, 0));
        compare_list("factory constructed", (char*)o + 4, (char*)r + 4);
        if (pass % 4 != 3) {
            set_heaps(true);
            run_pair("AtGraphicsFactory::Start", [&] { at<Fn0>(0x000EB520)(o, nullptr); }, [&] { ((AtGraphicsFactory*)r)->Start(); });
            compare_list("factory started", (char*)o + 4, (char*)r + 4);
        }
        uint32_t got[2];
        run_pair("AtGraphicsFactory::Slot0", [&] { got[0] = at<Get0>(0x000EB540)(o, nullptr) - (uint32_t)(uintptr_t)o; },
                 [&] { got[1] = (uint32_t)(uintptr_t)((AtGraphicsFactory*)r)->Slot0() - (uint32_t)(uintptr_t)r; });
        eq("slot 0", got[0], got[1]);
        // Slot 4, with the object or null.
        run_pair("AtGraphicsFactory::Destroy", [&] { at<Fn1>(0x000EB9D0)(o, nullptr, pass & 1 ? 0 : (uint32_t)(uintptr_t)&g_handed[0]); },
                 [&] { ((AtGraphicsFactory*)r)->Destroy(pass & 1 ? nullptr : &g_handed[1]); });
        if (pass % 4 == 0) {
            // Slot 3: the device. Its heap present or not, and failing allocation once.
            set_heaps(pass != 8);
            g_fail_alloc = pass == 12;
            void* dev[2] = {nullptr, nullptr};
            run_pair("AtGraphicsFactory::CreateDevice", [&] { dev[0] = (void*)(uintptr_t)at<Get0>(0x000EBE90)(o, nullptr); },
                     [&] { dev[1] = ((AtGraphicsFactory*)r)->CreateDevice(); });
            g_fail_alloc = false;
            eq("device made", dev[0] != nullptr, dev[1] != nullptr);
            if (dev[0] && dev[1]) {
                eq("device creator", word(dev[0], 0x3A8) - (uint32_t)(uintptr_t)o, word(dev[1], 0x3A8) - (uint32_t)(uintptr_t)r);
                same_mapped("device vftable", word(dev[0], 0), word(dev[1], 0));
            }
        }
        if (pass % 2 == 0) {
            run_pair("AtGraphicsFactory::Stop", [&] { at<Fn0>(0x000EB530)(o, nullptr); }, [&] { ((AtGraphicsFactory*)r)->Stop(); });
            compare_list("factory stopped", (char*)o + 4, (char*)r + 4);
        }
        run_pair("~AtGraphicsFactory", [&] { at<Fn0>(0x000EBD90)(o, nullptr); }, [&] { ((AtGraphicsFactory*)r)->~AtGraphicsFactory(); });
        same_mapped("factory vftable", word(o, 0), word(r, 0));
        compare_list("factory destroyed", (char*)o + 4, (char*)r + 4);
        std::free(o);
        std::free(r);
    }
}

// ---- the tables and the start -------------------------------------------------------------------------------------------------
static const uint32_t kMatrix = 0x002C60C8, kRender = 0x002C7C48;
struct Member { uint32_t orig; uint32_t rec; };
#define M(o, name) {o, offsetof(AtMatrixTable, name)}
static const Member kMembers[] = {
    M(0x000, Copy), M(0x004, Identity), M(0x00C, SetTranslation), M(0x010, SetScale3x3), M(0x014, SetAxisAngle), M(0x018, SetRotation), M(0x01C, SetAxes),
    M(0x02C, SetLookAt), M(0x030, SetLookAtZY), M(0x034, SetLookAtXY), M(0x038, MakeTranslation), M(0x03C, MakeAxisAngle), M(0x040, MakeRotation),
    M(0x044, MakeFromAxes), M(0x054, SetUniformScale), M(0x058, SetScale), M(0x090, PreRotateX), M(0x094, PreRotateY), M(0x098, PreRotateZ),
    M(0x0A0, ScaleAxes), M(0x0BC, TranslateAfter), M(0x0D0, PostRotateZ), M(0x0DC, ScaleColumnX), M(0x0E0, ScaleColumnY), M(0x108, Invert),
    M(0x10C, SetInverse), M(0x118, Transpose), M(0x120, SetElement), M(0x124, GetElement), M(0x12C, GetRotation), M(0x130, GetPosition),
    M(0x134, GetAxes), M(0x144, GetXAxis), M(0x148, GetYAxis), M(0x14C, GetZAxis), M(0x160, Multiply), M(0x164, MultiplyRight), M(0x16C, SetProduct),
    M(0x188, TransformPoint), M(0x18C, TransformPointZ), M(0x190, InverseTransformPoint), M(0x194, TransformDirection),
    M(0x198, InverseTransformDirection), M(0x19C, Read),
};
#undef M
static_assert(sizeof kMembers / sizeof kMembers[0] == sizeof(AtMatrixTable) / sizeof(void*), "every member of the host's table");
static void compare_matrix(const char* what, const void* orig, const AtMatrixTable* rec) {
    for (const Member& m : kMembers) same_mapped(what, word(orig, m.orig), word(rec, m.rec));
}
static void compare_render(const char* what) {
    compare_matrix(what, (void*)(uintptr_t)kRender, &g_atRender.m_matrix);
    for (int i = 0; i < 7; i++) same_mapped(what, word((void*)(uintptr_t)kRender, 0x1A0 + 4 * i), (uint32_t)(uintptr_t)g_atRender.m_extended[i]);
}
static void garble_tables(uint32_t seed) {
    for (uint32_t i = 0; i < 0x1A0; i += 4) *(uint32_t*)(uintptr_t)(kMatrix + i) = 0xAB000000u + seed + i;
    for (uint32_t i = 0; i < 0x1BC; i += 4) *(uint32_t*)(uintptr_t)(kRender + i) = 0xAC000000u + seed + i;
    for (const Member& m : kMembers) {
        *(uint32_t*)((unsigned char*)&g_atMatrix + m.rec) = 0xAB000000u + seed + m.orig;
        *(uint32_t*)((unsigned char*)&g_atRender.m_matrix + m.rec) = 0xAC000000u + seed + m.orig;
    }
    for (int i = 0; i < 7; i++) g_atRender.m_extended[i] = (void*)(uintptr_t)(0xAC000000u + seed + 0x1A0 + 4 * i);
}

static void run_tables() {
    // 0x000EC2B0 alone, on a table of garbage.
    static unsigned char room[2][0x200];
    for (int pass = 0; pass < 3; pass++) {
        for (uint32_t i = 0; i < 0x200; i += 4) {
            *(uint32_t*)(room[0] + i) = 0xAD000000u + pass * 0x1000 + i;
            *(uint32_t*)(room[1] + i) = 0xAD000000u + pass * 0x1000 + i;
        }
        for (const Member& m : kMembers) *(uint32_t*)(room[1] + m.rec) = 0xAD000000u + pass * 0x1000 + m.orig;
        run_pair("AtMatrixTableFill", [&] { at<void(__cdecl*)(void*)>(0x000EC2B0)(room[0]); }, [&] { AtMatrixTableFill((AtMatrixTable*)room[1]); });
        compare_matrix("matrix table filled", room[0], (AtMatrixTable*)room[1]);
    }
    // 0x000EC670 on the live tables.
    garble_tables(0x100);
    run_pair("AtGraphicsFillTables", [&] { at<void(__cdecl*)()>(0x000EC670)(); }, [&] { AtGraphicsFillTables(); });
    compare_matrix("matrix table", (void*)(uintptr_t)kMatrix, &g_atMatrix);
    compare_render("render table");
}

// The saved globals, each side's own (at/grlayer): constructed and saved by each side's code, so the start's Restore and Save are each side's too.
static void run_start() {
    at<Fn0>(0x000EBD70)((void*)(uintptr_t)0x003ADF64, nullptr);  // the original's static factory, constructed afresh
    for (int pass = 0; pass < 12; pass++) {
        random_script(pass);
        garble_tables(0x200 + pass);
        set_heaps(pass % 6 != 5);
        unsigned char saved[2][0x40];
        std::memset(saved, 0x3C, sizeof saved);
        at<Fn0>(0x000C8600)(saved[0], nullptr);
        at<Fn0>(0x000C8620)(saved[0], nullptr);
        new (saved[1]) AtGraphicsGlobals();
        ((AtGraphicsGlobals*)saved[1])->Save();
        static uint32_t config[64];
        static unsigned char tokens[3][16];
        if (pass % 2 == 0) {  // what the memory globals save, moved after the save: Restore puts it back
            *(void**)(uintptr_t)0x002C60AC = tokens[0];
            AtHeapManager::s_instance = (AtHeapManager*)tokens[0];
            *(void**)(uintptr_t)0x002C60B0 = tokens[1];
            AtIoTokenFile::s_heap = tokens[1];
        }
        uint32_t got[2];
        run_pair("AtGraphicsStartFactory", [&] { got[0] = at<uint32_t(__cdecl*)(void*, void*)>(0x000C7C60)(saved[0], config); },
                 [&] { got[1] = (uint32_t)(uintptr_t)AtGraphicsStartFactory((AtGraphicsGlobals*)saved[1], (const AtGraphicsConfig*)config); });
        same_mapped("factory handed back", got[0], got[1]);
        eq("heap manager after the start", *(uint32_t*)(uintptr_t)0x002C60AC == (uint32_t)(uintptr_t)tokens[0], AtHeapManager::s_instance == (AtHeapManager*)tokens[0]);
        eq("token file heap after the start", *(uint32_t*)(uintptr_t)0x002C60B0 == (uint32_t)(uintptr_t)tokens[1], AtIoTokenFile::s_heap == tokens[1]);
        compare_matrix("matrix table after the start", (void*)(uintptr_t)kMatrix, &g_atMatrix);
        compare_render("render table after the start");
        compare_list("the factory's adapters", (void*)(uintptr_t)(0x003ADF64 + 4), &g_atGraphicsFactory.m_adapters);
        if (pass % 3 != 1) {
            run_pair("AtGraphicsStopFactory", [&] { at<void(__cdecl*)()>(0x000C7C90)(); }, [&] { AtGraphicsStopFactory(); });
            compare_list("the factory's adapters, stopped", (void*)(uintptr_t)(0x003ADF64 + 4), &g_atGraphicsFactory.m_adapters);
        }
    }
}

static void build_map() {
    static unsigned char room[0x1000];
    AtGfxDeviceInfo* info = new (room) AtGfxDeviceInfo;
    g_map[vptr(info)] = 0x00289698;
    info->AtGfxDeviceInfoBase::~AtGfxDeviceInfoBase();
    g_infoBaseTable = vptr(info);
    g_map[g_infoBaseTable] = 0x0028B1B8;
    AtGfxAdapter* adapter = new (room) AtGfxAdapter;
    g_map[vptr(adapter)] = 0x0028943C;
    g_map[vptr(&adapter->m_driverText)] = 0x002748C0;
    adapter->AtGfxAdapterBase::~AtGfxAdapterBase();
    g_adapterBaseTable = vptr(adapter);
    g_map[g_adapterBaseTable] = 0x0028B194;
    AtGfxAdapterList* list = new (room) AtGfxAdapterList;
    g_map[vptr(list)] = 0x00289428;
    list->AtGfxAdapterListBase::~AtGfxAdapterListBase();
    g_listBaseTable = vptr(list);
    g_map[g_listBaseTable] = 0x00289414;
    AtGraphicsFactory* factory = new (room) AtGraphicsFactory;
    g_map[vptr(factory)] = 0x00288BE0;
    g_map[(uint32_t)(uintptr_t)&g_atGraphicsFactory] = 0x003ADF64;
    static unsigned char device[0x30600];
    g_map[vptr(new (device) AtGraphicsDeviceXbox)] = 0x00289208;
    struct F { void* rec; uint32_t orig; };
    const F fns[] = {
        {(void*)AtMatrixCopy, 0x00105440}, {(void*)AtMatrixIdentity, 0x00115020}, {(void*)AtMatrixSetTranslation, 0x001150A0},
        {(void*)AtMatrixSetScale3x3, 0x001150C0}, {(void*)AtMatrixSetAxisAngle, 0x00117690}, {(void*)AtMatrixSetRotation, 0x001150F0},
        {(void*)AtMatrixSetAxes, 0x001151F0}, {(void*)AtMatrixSetLookAt, 0x00117780}, {(void*)AtMatrixSetLookAtZY, 0x00117870},
        {(void*)AtMatrixSetLookAtXY, 0x00117950}, {(void*)AtMatrixMakeTranslation, 0x00115240}, {(void*)AtMatrixMakeAxisAngle, 0x00117A40},
        {(void*)AtMatrixMakeRotation, 0x00115290}, {(void*)AtMatrixMakeFromAxes, 0x001152C0}, {(void*)AtMatrixSetUniformScale, 0x00115320},
        {(void*)AtMatrixSetScale, 0x00115370}, {(void*)AtMatrixPreRotateX, 0x00117EE0}, {(void*)AtMatrixPreRotateY, 0x00117F40},
        {(void*)AtMatrixPreRotateZ, 0x00117FA0}, {(void*)AtMatrixScaleAxes, 0x00105780}, {(void*)AtMatrixTranslateAfter, 0x001157B0},
        {(void*)AtMatrixPostRotateZ, 0x00118100}, {(void*)AtMatrixScaleColumnX, 0x00115B00}, {(void*)AtMatrixScaleColumnY, 0x00115B30},
        {(void*)AtMatrixInvert, 0x001160C0}, {(void*)AtMatrixSetInverse, 0x00116150}, {(void*)AtMatrixTranspose, 0x001056A0},
        {(void*)AtMatrixSetElement, 0x00116390}, {(void*)AtMatrixGetElement, 0x001163B0}, {(void*)AtMatrixGetRotation, 0x001163D0},
        {(void*)AtMatrixGetPosition, 0x00116540}, {(void*)AtMatrixGetAxes, 0x00116560}, {(void*)AtMatrixGetXAxis, 0x00116640},
        {(void*)AtMatrixGetYAxis, 0x00116660}, {(void*)AtMatrixGetZAxis, 0x00116680}, {(void*)AtMatrixMultiply, 0x00116A20},
        {(void*)AtMatrixMultiplyRight, 0x00116A50}, {(void*)AtMatrixSetProduct, 0x00105470}, {(void*)AtMatrixTransformPoint, 0x00116E30},
        {(void*)AtMatrixTransformPointZ, 0x00116EB0}, {(void*)AtMatrixInverseTransformPoint, 0x00116EE0}, {(void*)AtMatrixTransformDirection, 0x00116FA0},
        {(void*)AtMatrixInverseTransformDirection, 0x00117010}, {(void*)AtMatrixRead, 0x00117080},
        {(void*)AtRenderFunction0, 0x00117130}, {(void*)AtRenderFunction1, 0x001171D0}, {(void*)AtRenderFunction2, 0x001CC9F0},
        {(void*)AtRenderFunction3, 0x00117250}, {(void*)AtRenderFunction4, 0x00117380}, {(void*)AtRenderFunction5, 0x00117470},
        {(void*)AtRenderFunction6, 0x001175A0},
    };
    for (const F& f : fns) {
        if (g_map.count((uint32_t)(uintptr_t)f.rec)) std::printf("harness: two functions share the address of 0x%08X\n", f.orig);
        g_map[(uint32_t)(uintptr_t)f.rec] = f.orig;
    }
    g_log[0].n = g_log[1].n = 0;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == 0xC0000374) {
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
    if (pos.size() < 1) { std::printf("usage: oracle_grfactory <default.xbe> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();

    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00206070, (const void*)o_create);
    redirect(0x00206080, (const void*)o_identifier);
    redirect(0x002062B0, (const void*)o_caps);
    redirect(0x002060B0, (const void*)o_mode_count);
    redirect(0x00206150, (const void*)o_enum);
    redirect(0x002063F0, (const void*)o_check);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    build_map();
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_formats();
    run_infos();
    run_adapters();
    run_lists();
    run_factory();
    run_tables();
    run_start();
    report_entries();
    if (g_bad) { std::printf("a function no case should reach was reached %d times\n", g_bad); g_failures++; }
    std::printf("%s: %d crashes on both sides; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", (int)g_crashes, g_checks,
                g_failures);
    return g_failures ? 1 : 0;
}
