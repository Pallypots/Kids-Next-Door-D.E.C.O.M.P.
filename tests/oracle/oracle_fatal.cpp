// Differential oracle for the game's fatal error path (Windows x86, local only): the report of a failed file, AtFatal, the rumble it stops and the error screen it draws. See
// include/at/AtFatal.h and docs/verification.md.
//
//   oracle_fatal <default.xbe> [--entries FILE] [--mutate] [--seed N]
//
// No game file is read. What stands in for what, first:
//  * The library's code is a logging double on both sides: XLaunchNewImage (0x00085197), printf (0x001254F3), exit (0x001254DB), sprintf (0x00125559) and vsprintf
//    (0x00125ECE) on the original side; AtPlatformLaunchImage, AtLibraryPrint and AtLibraryExit, defined here, and the host's own sprintf and vsprintf on the recovered side.
//    Exit, on both sides, ends the run by a deliberate crash that oracle_guard.h turns back into a return.
//  * Everything the graphics layer, the input device, the physical controllers, the controller module's tickers and the material owner would be is a fake of this file, called
//    through its vftable by both sides alike, each side with its own copies: the 2D renderer, the camera's matrix device, the images (concrete slots 0 and 3), the database
//    loader and the image database, App's graphics, the device and its controllers, and the controllers' slot 8. Present (the owner's slot 9) crashes after a set number of
//    frames, which is how the never-ending loop of the error screen is driven for a bounded number of frames.
//  * The matrix table's Identity, which the camera's SetMatrices reaches, is the recovered one on both sides.
//  * What runs for real: everything recovered here, and on each side its own AtCopyName8, AtIoStatusText, GmRumbleEffect::Frame, AtColorRectangle's constructor and SetAll,
//    AtColor::SetWhite, AtFpPlaceObject2Tag::SetColorScale's body, AtCoordSys::SetMatrices and Reset, AtInputEventQueue::Flush and the C library's _ftol2.
// So a green result says the recovered logic agrees call by call and value by value with the original's over these fakes; it says nothing about the real renderer, device or
// dashboard behind them, none of which is recovered.
#define NOMINMAX
#include <windows.h>

#include <float.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtControllerManager.h"
#include "at/AtDatabaseLoader.h"
#include "at/AtFatal.h"
#include "at/AtGr2D.h"
#include "at/AtImageDatabase.h"
#include "at/AtMatrix.h"
#include "gm/GmApp.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 200).c_str(), what, o, r);
}
static void eqs(const char* what, const std::string& o, const std::string& r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 200).c_str(), what, o.c_str(), r.c_str());
}
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float real(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
// NaNs that differ only in sign or payload count as alike: which the x87 gives depends on operand order (docs/verification.md).
static void eqf(const char* what, float o, float r) {
    if (o != o && r != r) { g_checks++; return; }
    eq(what, bits(o), bits(r));
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }
static uint32_t& orig_u32(uint32_t va) { return *(uint32_t*)(uintptr_t)va; }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

// ---- the call log: one for each side ---------------------------------------------------------------------------------
static std::vector<uint32_t> g_log[2];
static std::string g_printed[2];
static int g_side;
static void note(int side, uint32_t tag, uint32_t value = 0) { g_log[side].push_back(tag); g_log[side].push_back(value); }
static void compare_logs(const char* what) {
    eq(what, (long long)g_log[0].size(), (long long)g_log[1].size());
    if (g_log[0].size() != g_log[1].size() && g_failures < 4) {
        for (int s = 0; s < 2; s++) {
            for (size_t i = 0; i < g_log[s].size() && i < 60; i += 2) std::printf("%c%X ", (char)g_log[s][i], g_log[s][i + 1]);
            std::printf("\n");
        }
    }
    for (size_t i = 0; i < g_log[0].size() && i < g_log[1].size(); i++) {
        if (g_log[0][i] != g_log[1][i]) {
            eq(what, g_log[0][i], g_log[1][i]);
            if (g_failures < 4) std::printf("  at word %u, tag %c\n", (unsigned)i, (char)g_log[0][i & ~1u]);
            break;
        }
    }
    eqs(what, g_printed[0], g_printed[1]);
    g_log[0].clear();
    g_log[1].clear();
    g_printed[0].clear();
    g_printed[1].clear();
}

static unsigned g_rng;
static unsigned rng() {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 8;
}
static float frand(float lo, float hi) { return lo + (hi - lo) * (float)(rng() % 100000) / 100000.0f; }
// Ordinary values, ties, integers, and now and then something extreme.
static float wild() {
    switch (rng() % 16) {
    case 0: return 0.0f;
    case 1: return -0.0f;
    case 2: return (float)(int)(rng() % 9) - 4.0f;
    case 3: return real(0x7FC00000u);           // NaN
    case 4: return real(0x7F800000u);           // infinity
    case 5: return real(0x00000001u + rng() % 100);  // denormal
    case 6: return frand(-1e6f, 1e6f);
    default: return frand(-700.0f, 700.0f);
    }
}

// ---- the library's doubles -------------------------------------------------------------------------------------------
static int g_frames_left[2];
static void stop_here() { *(volatile int*)8 = 0; }

static void __stdcall o_launch(const char* path, AtLaunchData* data) {
    note(0, 'L', path == nullptr);
    uint32_t h = 0;
    for (unsigned i = 0; i < 0x300; i++) h = h * 31 + data->m_words[i];
    note(0, 'l', h);
}
void AtPlatformLaunchImage(const char* path, AtLaunchData* data) {
    note(1, 'L', path == nullptr);
    uint32_t h = 0;
    for (unsigned i = 0; i < 0x300; i++) h = h * 31 + data->m_words[i];
    note(1, 'l', h);
}
static int __cdecl o_print(const char* format, ...) {
    char text[2048];
    va_list a;
    va_start(a, format);
    const int n = vsnprintf_s(text, sizeof text, _TRUNCATE, format, a);
    va_end(a);
    g_printed[0] += text;
    note(0, 'P', n);
    return n;
}
int AtLibraryPrint(const char* format, ...) {
    char text[2048];
    va_list a;
    va_start(a, format);
    const int n = vsnprintf_s(text, sizeof text, _TRUNCATE, format, a);
    va_end(a);
    g_printed[1] += text;
    note(1, 'P', n);
    return n;
}
static void __cdecl o_exit(int code) { note(0, 'X', code); stop_here(); }
void AtLibraryExit(int code) { note(1, 'X', code); stop_here(); }
static int __cdecl o_sprintf(char* out, const char* format, ...) {
    va_list a;
    va_start(a, format);
    const int n = vsprintf(out, format, a);
    va_end(a);
    return n;
}
static int __cdecl o_vsprintf(char* out, const char* format, va_list a) { return vsprintf(out, format, a); }

// What the rest of the recovered code links to and this oracle never reaches.
void* AtPlatformAllocateRegion(unsigned int size) { return std::malloc(size); }
void AtPlatformFreeRegion(void* region) { std::free(region); }
void AtPlatformInitDevices() {}
void AtLibraryFree(void* data) { std::free(data); }
int AtPlatformLanguage() { return 1; }
int AtPlatformGameRegion() { return 1; }
unsigned int AtPlatformMilliseconds() { return 0; }

// ---- fakes beneath both sides ----------------------------------------------------------------------------------------
class Phys : public AtInputPhysicalController {
public:
    int side = 0;
    int id = 0;
    int Slot0() override { return 0; }
    int CopyStates() override { return 0; }
    int Update() override { return 0; }
    unsigned int ButtonCount() override { return 0; }
    unsigned int AxisCount() override { return 0; }
    unsigned int PovCount() override { return 0; }
    AtInputAxisHandler* AxisHandler(unsigned int) override { return nullptr; }
    void PhysicalSlot0() override {}
    void Slot15(int motor, int value, int ms) override { note(side, 'M', id * 16 + motor); note(side, 'm', value); note(side, 't', ms); }
    void Slot16(int motor) override { note(side, 'S', id * 16 + motor); }
};

class Dev : public AtInputDevice {
public:
    int side = 0;
    int result = 0;
    Phys phys[4];
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Slot12() override {}
    void Slot13() override {}
    int Update(int ms) override { note(side, 'U', ms); return result; }
    void Slot15() override {}
    void Slot16() override {}
    void Slot17() override {}
    unsigned int PhysicalCount() override { return 4; }
    unsigned int ConnectedCount() override { return 4; }
    AtInputPhysicalController* First() override { return &phys[0]; }
    AtInputPhysicalController* Second() override { return &phys[1]; }
    AtInputPhysicalController* Numbered(unsigned int i) override { return &phys[i & 3]; }
    AtInputPhysicalController* Physical(unsigned int i) override { note(side, 'p', i); return &phys[i & 3]; }
};

class Controller : public AtAbstractController {
public:
    int side = 0;
    int id = 0;
    int result = 0;
    int Tick(int ms) override { note(side, 'K', id); note(side, 'k', ms); return result; }
};

class Ticker : public GmCoTicker {
public:
    int side = 0;
    int id = 0;
    void Tick(int ms) override { note(side, 'T', id); note(side, 't', ms); }
};

static void __fastcall o_resume(void* context, void*) { note(0, 'R', *(int*)context); }
static void r_resume(void* context) { note(1, 'R', *(int*)context); }

class Matrices : public AtMatrixDevice {
public:
    int side = 0;
    int next = 0;
    AtMatrix matrices[8];
    AtMatrix* NewMatrix() override { return nullptr; }
    AtMatrixPool* NewPool() override { return nullptr; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override {}
    void FreePool(AtMatrixPool*) override {}
    AtMatrix* NewMatrix8() override { note(side, 'x', next); return &matrices[next++ & 7]; }
};

class Owner : public AtMaterialOwner {
public:
    int side = 0;
    Matrices matrices;
    AtMatrixDevice* MatrixDevice() override { note(side, 'D'); return &matrices; }
    void Present(int a) override {
        note(side, 'F', a);
        if (--g_frames_left[side] <= 0) stop_here();
    }
};

class Renderer : public AtGr2DRenderer {
public:
    int side = 0;
    void Begin(AtCamera* camera, int a, int b) override { note(side, 'B', camera != nullptr); note(side, 'b', a * 16 + b); }
    void End() override { note(side, 'E'); }
};

class Material : public AtMaterial {
public:
    int id = 0;
    void OnLoaded(AtMaterialOwner*) override {}
};

class Camera : public AtCamera {
public:
    int side = 0;
    void Reset() override { note(side, 'r'); AtCamera::Reset(); }
};

class Image : public AtImage {
public:
    int side = 0;
    int id = 0;
    ::Material* material = nullptr;
    AtMaterial* Material() override { note(side, 'm', id); return material; }
    void Slot1() override {}
    void Slot2() override {}
    void DrawStrip(AtGr2DRenderer* r, unsigned int strip, AtMaterial* m, const AtRect* rect, const AtRect* uv, const AtColorRectangle* c) override {
        note(side, 'd', id * 256 + strip);
        note(side, 'w', r != nullptr);
        note(side, 'a', m ? ((::Material*)m)->id : 0);
        for (const float* f = &rect->left; f <= &rect->bottom; f++) note(side, 'q', (*f != *f) ? 0x7FC00000u : bits(*f));
        for (const float* f = &uv->left; f <= &uv->bottom; f++) note(side, 'u', (*f != *f) ? 0x7FC00000u : bits(*f));
        for (int i = 0; i < 4; i++) { uint32_t w; std::memcpy(&w, &c->m_colors[i], 4); note(side, 'c', w); }
    }
    void OnLoaded(AtMaterialOwner*) override {}
    void Slot14() override {}
};

class Images : public AtImageDatabase {
public:
    int side = 0;
    Image* items[8] = {};
    void MakeItems() override {}
    void OnReady(AtMaterialOwner*) override {}
    AtImage* ImageAt(unsigned int i) override { note(side, 'i', i); return items[i & 7]; }
};

class Loader : public AtDatabaseLoader {
public:
    int side = 0;
    Images* images = nullptr;
    void MakeTextures() override {}
    void MakeMaterials() override {}
    void MakeShaders() override {}
    void MakeMeshes() override {}
    void MakeLights() override {}
    void Slot5() override {}
    void MakeFonts() override {}
    void MakeImages() override {}
    void MakeVertexProcesses() override {}
    void MakePixelProcesses() override {}
    void MakeLoaders() override {}
    AtTextureDatabase* TextureAt(unsigned int) override { return nullptr; }
    AtMaterialDatabase* MaterialAt(unsigned int) override { return nullptr; }
    AtShaderDatabase* ShaderAt(unsigned int) override { return nullptr; }
    AtMeshDatabase* MeshAt(unsigned int) override { return nullptr; }
    AtLightDatabase* LightAt(unsigned int) override { return nullptr; }
    AtFontDatabase* FontAt(unsigned int) override { return nullptr; }
    AtImageDatabase* ImageAt(unsigned int i) override { note(side, 'I', i); return images; }
    AtVertexProcessDatabase* VertexProcessAt(unsigned int) override { return nullptr; }
    AtPixelProcessDatabase* PixelProcessAt(unsigned int) override { return nullptr; }
    AtDatabaseLoader* LoaderAt(unsigned int) override { return nullptr; }
};

class Graphics : public AppGraphics {
public:
    int side = 0;
    AtCamera* camera = nullptr;
    AtCamera* Camera2D() override { note(side, 'C'); return camera; }
};

// ---- the original's entries ------------------------------------------------------------------------------------------
typedef int(__fastcall* FnClip)(void*, void*, const AtRect*, const AtRect*, AtRect*);
typedef void(__fastcall* FnLerp)(void*, void*, const void*, const void*, float);
typedef void(__fastcall* FnRectClip)(void*, void*, const AtColorRectangle*, const AtRect*, const AtRect*);
typedef void(__fastcall* FnDraw)(void*, void*, AtGr2DRenderer*, const AtVector2*, const AtVector2*, const AtColorRectangle*, const AtRect*);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef void(__fastcall* FnF)(void*, void*, float);
typedef void(__fastcall* FnI)(void*, void*, int);
typedef int(__fastcall* FnIntI)(void*, void*, int);
typedef int(__fastcall* FnIntII)(void*, void*, int, int);
typedef float(__fastcall* FnRealI)(void*, void*, int);
typedef float(__fastcall* FnReal)(void*, void*);
typedef int(__fastcall* FnInt)(void*, void*);
typedef void*(__fastcall* FnPtrU)(void*, void*, unsigned);
typedef int(__cdecl* FnCompare)(const char*, const char*);
typedef void*(__cdecl* FnInstance)();
typedef void(__cdecl* FnReport)(int, const char*, ...);
typedef void(__cdecl* FnReportV)(int, const char*, va_list);
typedef void(__cdecl* FnFatal)(const char*, int, int, int);

// ---- the pieces, one by one ------------------------------------------------------------------------------------------
static unsigned long long g_pieces;

static void run_names() {
    static const char* words[] = {"GERMAN", "FRENCH", "ITALIAN", "ENGLISH", "", "GERMANY", "ITALIANX", "ABCDEFGH", "ABCDEFGI", "\xE9\x80", "a", "A"};
    const int n = sizeof words / sizeof words[0];
    for (int i = -1; i < n; i++) {
        for (int j = -1; j < n; j++) {
            const char* a = i < 0 ? nullptr : words[i];
            const char* b = j < 0 ? nullptr : words[j];
            g_context = "names";
            eq("AtCompareName8", at<FnCompare>(0x00013F00)(a, b), AtCompareName8(a, b));
            g_pieces++;
        }
    }
    for (int k = 0; k < 20000; k++) {
        char a[12] = {}, b[12] = {};
        const int len = 1 + rng() % 11;
        for (int i = 0; i < len; i++) a[i] = b[i] = (char)("AB\x80\xFF\x01z"[rng() % 6]);
        if (rng() % 2) b[rng() % 11] = (char)(rng() % 256);
        eq("AtCompareName8", at<FnCompare>(0x00013F00)(a, b), AtCompareName8(a, b));
        g_pieces++;
    }
}

static void same_rect(const char* what, const AtRect& o, const AtRect& r) {
    eqf(what, o.left, r.left);
    eqf(what, o.top, r.top);
    eqf(what, o.right, r.right);
    eqf(what, o.bottom, r.bottom);
}
static AtRect any_rect() {
    AtRect r;
    if (rng() % 4 == 0) {
        r.left = wild(); r.top = wild(); r.right = wild(); r.bottom = wild();
    } else {
        r.left = frand(-300, 600); r.top = frand(-300, 600);
        r.right = r.left + frand(-50, 700); r.bottom = r.top + frand(-50, 700);
        if (rng() % 5 == 0) r.right = r.left;
    }
    return r;
}

static void run_rects() {
    for (int k = 0; k < 200000; k++) {
        g_context = "rect " + std::to_string(k);
        const AtRect clip = any_rect(), rect = rng() % 6 == 0 ? clip : any_rect();
        AtRect uv[2], out[2];
        uv[0] = rng() % 3 ? AtRect{0, 0, frand(1, 512), frand(1, 512)} : any_rect();
        uv[1] = uv[0];
        std::memset(out, 0xCD, sizeof out);
        const int o = at<FnClip>(0x0005F600)(&out[0], nullptr, &clip, &rect, &uv[0]);
        const int r = out[1].Clip(clip, rect, &uv[1]);
        eq("Clip result", o, r);
        same_rect("Clip out", out[0], out[1]);
        same_rect("Clip uv", uv[0], uv[1]);
        g_pieces++;
    }
}

static AtColor any_color() {
    AtColor c;
    c.r = (unsigned char)rng(); c.g = (unsigned char)rng(); c.b = (unsigned char)rng(); c.a = (unsigned char)rng();
    if (rng() % 8 == 0) c.r = c.g = c.b = c.a = (unsigned char)(rng() % 2 ? 255 : 0);
    return c;
}
static AtColorF any_colorf() {
    AtColorF c;
    float* f = &c.r;
    for (int i = 0; i < 4; i++) f[i] = rng() % 10 == 0 ? (float)(rng() % 256) * 0.0039215689f : frand(0.0f, 1.0f);
    return c;
}
static float any_t() {
    switch (rng() % 8) {
    case 0: return 0.0f;
    case 1: return 1.0f;
    case 2: return 0.5f;
    case 3: return frand(-0.2f, 1.2f);
    default: return frand(0.0f, 1.0f);
    }
}
static uint32_t word_of(const AtColor& c) { uint32_t w; std::memcpy(&w, &c, 4); return w; }

// Reals of any mantissa in [0, 2), where which product is rounded before its sum shows.
static float any_bits_unit() { return real(0x3F000000u + (rng() & 0x00FFFFFFu)) - 0.5f; }

static void run_color_rounding() {
    for (int k = 0; k < 1000000; k++) {
        AtColorF from, to;
        float* f = &from.r;
        float* g = &to.r;
        for (int i = 0; i < 4; i++) { f[i] = any_bits_unit(); g[i] = any_bits_unit(); }
        const float t = any_bits_unit();
        AtColorF o, r;
        at<FnLerp>(0x00035840)(&o, nullptr, &from, &to, t);
        r.Lerp(from, to, t);
        if (bits(o.r) != bits(r.r) || bits(o.g) != bits(r.g) || bits(o.b) != bits(r.b) || bits(o.a) != bits(r.a)) {
            g_context = "colour rounding " + std::to_string(k);
            eqf("AtColorF::Lerp r", o.r, r.r); eqf("AtColorF::Lerp g", o.g, r.g); eqf("AtColorF::Lerp b", o.b, r.b); eqf("AtColorF::Lerp a", o.a, r.a);
        } else {
            g_checks++;
        }
        g_pieces++;
    }
}

static void run_colors() {
    run_color_rounding();
    for (int k = 0; k < 200000; k++) {
        g_context = "colour " + std::to_string(k);
        const AtColor a = any_color(), b = any_color();
        const float t = any_t();
        AtColor oc, rc;
        std::memset(&oc, 0xCD, 4); std::memset(&rc, 0xCD, 4);
        at<FnLerp>(0x000412D0)(&oc, nullptr, &a, &b, t);
        rc.Lerp(a, b, t);
        eq("AtColor::Lerp", word_of(oc), word_of(rc));
        const AtColorF fa = any_colorf(), fb = any_colorf();
        at<FnLerp>(0x00041380)(&oc, nullptr, &fa, &fb, t);
        rc.LerpReal(fa, fb, t);
        eq("AtColor::LerpReal", word_of(oc), word_of(rc));
        AtColorF of, rf;
        at<FnLerp>(0x00035840)(&of, nullptr, &fa, &fb, t);
        rf.Lerp(fa, fb, t);
        eqf("AtColorF::Lerp r", of.r, rf.r); eqf("AtColorF::Lerp g", of.g, rf.g); eqf("AtColorF::Lerp b", of.b, rf.b); eqf("AtColorF::Lerp a", of.a, rf.a);
        at<FnLerp>(0x000358D0)(&of, nullptr, &a, &b, t);
        rf.LerpBytes(a, b, t);
        eqf("AtColorF::LerpBytes r", of.r, rf.r); eqf("AtColorF::LerpBytes g", of.g, rf.g); eqf("AtColorF::LerpBytes b", of.b, rf.b); eqf("AtColorF::LerpBytes a", of.a, rf.a);
        // In place: this is from, as nothing in the game does, but the order of reads shows.
        AtColorF ip[2] = {fa, fa};
        at<FnLerp>(0x00035840)(&ip[0], nullptr, &ip[0], &fb, t);
        ip[1].Lerp(ip[1], fb, t);
        eqf("AtColorF::Lerp in place", ip[0].r, ip[1].r); eqf("AtColorF::Lerp in place", ip[0].a, ip[1].a);
        // The colour rectangle, over a part of a whole.
        AtColorRectangle colors;
        for (int i = 0; i < 4; i++) colors.m_colors[i] = any_color();
        AtRect full = any_rect(), part = any_rect();
        if (rng() % 3 == 0) { part = full; part.left = frand(full.left, full.right); }
        if (rng() % 4 == 0) part.top = full.top;
        if (rng() % 4 == 0) part.bottom = full.bottom;
        AtColorRectangle ocr, rcr;
        std::memset(&ocr, 0xCD, sizeof ocr); std::memset(&rcr, 0xCD, sizeof rcr);
        at<FnRectClip>(0x00042280)(&ocr, nullptr, &colors, &full, &part);
        rcr.Clip(colors, full, part);
        for (int i = 0; i < 4; i++) eq("AtColorRectangle::Clip", word_of(ocr.m_colors[i]), word_of(rcr.m_colors[i]));
        g_pieces++;
    }
}

// ---- the rumble ------------------------------------------------------------------------------------------------------
struct RumbleWorld {
    int side = 0;
    GmRumbleFrame frames[4][6];
    GmRumbleEffect effects[4];
    GmRumblePlayback pool[4][10];
    AtRumblePlayer players[4];
    Dev device;
    Controller controllers[4];
    AtInputEventQueue::Node nodes[4][6];
    int events[4][6][7];
    AtInputControllerSlot slots[4];
    AtInputControllerSlot* slotTable[5];
    AtControllerManager manager;
    AppUnknown692C module;
    GmNameMap::Entry entries[4][6];
    Ticker tickers[12];
    GmRumbleSystem system;
    int context = 0;
};

// The same world on both sides, from one seed.
static void build(RumbleWorld& w, int side, unsigned seed) {
    g_rng = seed;
    w.side = side;
    for (int e = 0; e < 4; e++) {
        const int n = rng() % 6;  // the first frame is a guard: an effect with none reads it as its last
        float time = frand(-0.2f, 0.3f);
        for (int f = 0; f < 6; f++) {
            w.frames[e][f].m_time = time;
            w.frames[e][f].m_left = rng() % 6 == 0 ? 0.0f : frand(0, 1);
            w.frames[e][f].m_right = rng() % 6 == 0 ? 0.0f : frand(0, 1);
            if (rng() % 4) time = (float)(time + frand(0, 0.8f));
        }
        w.effects[e].m_frames.m_items = &w.frames[e][1];
        w.effects[e].m_frames.m_count = n > 4 ? 4 : n;
    }
    w.device.side = side;
    w.device.result = (int)(rng() % 7);
    for (int i = 0; i < 4; i++) {
        w.device.phys[i].side = side;
        w.device.phys[i].id = i;
        w.device.phys[i].m_unknown10 = (int)(rng() % 4);
    }
    const unsigned playerCount = rng() % 5;
    for (int p = 0; p < 4; p++) {
        AtRumblePlayer& pl = w.players[p];
        pl.m_enabled = rng() % 5 != 0;
        pl.m_port = rng() % 4;
        pl.m_manager = &w.manager;
        pl.m_paused = rng() % 5 == 0;
        pl.m_motorsOn = rng() % 6 != 0;
        pl.m_count = rng() % 11;
        for (int i = 0; i < 10; i++) {
            GmRumblePlayback& pb = w.pool[p][i];
            pb.m_unknown00 = (int)(rng() % 3);
            pb.m_effect = &w.effects[rng() % 4];
            pb.m_time = rng() % 8 == 0 ? pb.m_effect->m_frames.m_items[0].m_time : frand(-0.5f, 3.0f);
            pb.m_loop = rng() % 2;
            pl.m_playing[i] = &pb;
        }
    }
    for (int c = 0; c < 4; c++) {
        Controller& k = w.controllers[c];
        k.side = side;
        k.id = c;
        k.result = (int)(rng() % 3);
        // A hand-made queue of events of both kinds, for the slots that are not physical.
        AtInputEventQueue& q = k.m_queue;
        const int n = rng() % 6;
        q.m_first = q.m_last = nullptr;
        q.m_freeFirst = q.m_freeLast = nullptr;
        for (int i = 0; i < n; i++) {
            AtInputEventQueue::Node& node = w.nodes[c][i];
            w.events[c][i][1] = (int)(rng() % 2);
            node.m_event = w.events[c][i];
            node.m_next = nullptr;
            node.m_prev = q.m_last;
            if (q.m_last) q.m_last->m_next = &node; else q.m_first = &node;
            q.m_last = &node;
        }
        AtInputControllerSlot& s = w.slots[c];
        std::memset(&s, 0, sizeof s);
        s.m_controller = &k;
        s.m_physical = rng() % 2;
    }
    for (int i = 0; i < 5; i++) w.slotTable[i] = rng() % 4 ? &w.slots[rng() % 4] : nullptr;
    AtControllerManager& m = w.manager;
    m.m_device = &w.device;
    m.m_slots = rng() % 5 ? w.slotTable : nullptr;
    m.m_slotCount = rng() % 6;
    m.m_players = w.players;
    m.m_playerCount = playerCount;
    m.m_suspended = rng() % 3 == 0;
    w.context = (int)(rng() % 1000);
    m.m_resumeContext = rng() % 2 ? &w.context : nullptr;
    m.m_resume = side == 0 ? (AtControllerManager::Callback)(void*)o_resume : r_resume;
    int t = 0;
    for (int map = 0; map < 4; map++) {
        const unsigned size = rng() % 6;
        for (unsigned i = 0; i < 6; i++) {
            std::memset(w.entries[map][i].m_name, 0, 8);
            w.entries[map][i].m_value = nullptr;
            if (i < size && rng() % 3 && t < 12) {
                w.tickers[t].side = side;
                w.tickers[t].id = t;
                w.entries[map][i].m_value = &w.tickers[t++];
            }
        }
        w.module.m_tickers[map].m_entries = size ? w.entries[map] : nullptr;
        w.module.m_tickers[map].m_size = size;
    }
    std::memset(&w.system, 0, sizeof w.system);
    w.system.m_manager = &w.manager;
}

static int index_in(const GmRumblePlayback* p, const RumbleWorld& w) {
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < 10; b++)
            if (p == &w.pool[a][b]) return a * 10 + b;
    return -1;
}
static int effect_index(const GmRumbleEffect* e, const RumbleWorld& w) {
    for (int i = 0; i < 4; i++) if (e == &w.effects[i]) return i;
    return e == nullptr ? -1 : -2;
}
static int queue_length(const AtInputEventQueue::Node* n) { int k = 0; for (; n && k < 50; n = n->m_next) k++; return k; }

static void same_world(const RumbleWorld& o, const RumbleWorld& r) {
    for (int p = 0; p < 4; p++) {
        eq("player count", o.players[p].m_count, r.players[p].m_count);
        for (int i = 0; i < 10; i++) {
            eq("player pool", index_in(o.players[p].m_playing[i], o), index_in(r.players[p].m_playing[i], r));
            const GmRumblePlayback& a = o.pool[p][i];
            const GmRumblePlayback& b = r.pool[p][i];
            eq("playback word 0", a.m_unknown00, b.m_unknown00);
            eq("playback effect", effect_index(a.m_effect, o), effect_index(b.m_effect, r));
            eqf("playback time", a.m_time, b.m_time);
            eq("playback loop", a.m_loop, b.m_loop);
        }
    }
    for (int c = 0; c < 4; c++) {
        eq("queue", queue_length(o.controllers[c].m_queue.m_first), queue_length(r.controllers[c].m_queue.m_first));
        eq("queue free", queue_length(o.controllers[c].m_queue.m_freeFirst), queue_length(r.controllers[c].m_queue.m_freeFirst));
    }
}

static RumbleWorld* g_rw[2];
static unsigned long long g_rumble_runs, g_motor_calls;

static void set_rumble_globals(int ready, bool module) {
    orig_u32(0x002CD0C8) = ready;
    orig_u32(0x002CD0C4) = (uint32_t)(uintptr_t)&g_rw[0]->manager;
    orig_u32(0x002CD0D4) = module ? (uint32_t)(uintptr_t)&g_rw[0]->module : 0;
    orig_u32(0x002CF15C) = (uint32_t)(uintptr_t)&g_rw[0]->system;
    AtControllerManager::s_ready = ready;
    AtControllerManager::s_instance = &g_rw[1]->manager;
    AppUnknown692C::s_instance = module ? &g_rw[1]->module : nullptr;
    GmRumbleSystem::s_instance = &g_rw[1]->system;
}

static void count_motors() {
    for (size_t i = 0; i < g_log[0].size(); i += 2) if (g_log[0][i] == 'M') g_motor_calls++;
}

static void run_rumble(int seed) {
    for (int k = 0; k < 3000; k++) {
        const unsigned s = (unsigned)seed * 7919u + (unsigned)k * 104729u;
        for (int side = 0; side < 2; side++) build(*g_rw[side], side, s);
        RumbleWorld& o = *g_rw[0];
        RumbleWorld& r = *g_rw[1];
        g_rng = s ^ 0x5A5A5A5Au;
        const unsigned what = rng() % 8;
        g_context = "rumble " + std::to_string(k) + " kind " + std::to_string(what);
        switch (what) {
        case 0: {  // the playback's members
            for (int i = 0; i < 10; i++) {
                const float dt = rng() % 4 == 0 ? wild() : frand(-0.5f, 2.0f);
                const int motor = (int)(rng() % 3);
                GmRumblePlayback& a = o.pool[0][i];
                GmRumblePlayback& b = r.pool[0][i];
                eqf("Intensity", at<FnRealI>(0x00073E60)(&a, nullptr, motor), b.Intensity(motor));
                eq("Finished", at<FnInt>(0x00073F00)(&a, nullptr), b.Finished());
                at<FnF>(0x00073E20)(&a, nullptr, dt);
                b.Advance(dt);
                eqf("Duration", at<FnReal>(0x00074930)(a.m_effect, nullptr), b.m_effect->Duration());
                eq("FrameCount", at<FnInt>(0x000ED690)(a.m_effect, nullptr), (int)b.m_effect->FrameCount());
                if (rng() % 3 == 0) { at<Fn0>(0x00073E00)(&a, nullptr); b.Stop(); }
                if (rng() % 5 == 0) { at<Fn0>(0x000B9A90)(&a, nullptr); b.Clear(); }
            }
            break;
        }
        case 1: case 2: {  // one player
            const float dt = rng() % 6 == 0 ? wild() : frand(0.0f, 1.5f);
            if (rng() % 3 == 0) { at<Fn0>(0x0006D7E0)(&o.players[0], nullptr); r.players[0].StopEffects(); }
            at<FnF>(0x0006D810)(&o.players[0], nullptr, dt);
            g_side = 1;
            r.players[0].Update(dt);
            break;
        }
        case 3: {  // a slot
            const int ms = (int)(rng() % 20000);
            for (int c = 0; c < 4; c++) eq("slot result", at<FnIntI>(0x00072610)(&o.slots[c], nullptr, ms), r.slots[c].Update(ms));
            break;
        }
        case 4: {  // the manager
            const int ms = (int)(rng() % 20000) - 100;
            const int force = (int)(rng() % 2);
            eq("manager result", at<FnIntII>(0x0006CE30)(&o.manager, nullptr, ms, force), r.manager.Update(ms, force));
            break;
        }
        case 5: {  // the module, and its pieces
            set_rumble_globals(0, true);
            eq("Instance", (long long)(uintptr_t)at<FnInstance>(0x0008F4F0)(), (long long)(uintptr_t)&o.module);
            eq("BaseInstance", (long long)(uintptr_t)at<FnInstance>(0x0006DA50)(), (long long)(uintptr_t)&o.module);
            eq("Instance", (long long)(uintptr_t)AppUnknown692C::Instance(), (long long)(uintptr_t)&r.module);
            for (unsigned i = 0; i < o.module.m_tickers[0].m_size; i++)
                eq("ValueAt", (long long)(uintptr_t)at<FnPtrU>(0x00024B70)(&o.module.m_tickers[0], nullptr, i) == 0, r.module.m_tickers[0].ValueAt(i) == nullptr);
            const int ms = (int)(rng() % 20000);
            at<FnI>(0x0006DEA0)(&o.module, nullptr, ms);
            r.module.Update(ms);
            break;
        }
        default: {  // AtFatal itself, with the rumble ready or not, the module there or not, and no screen
            const int ready = rng() % 4 != 0;
            const bool module = rng() % 4 != 0;
            set_rumble_globals(ready, module);
            orig_u32(0x002C6060) = 0;
            orig_u32(0x002C605C) = 0;
            g_atFatalScreenShown = 0;
            g_atFatalExiting = 0;
            orig_u32(0x002BF310 + 0x6490) = 0;
            g_app.m_dirtyDiskLoader = nullptr;
            const int line = (int)(rng() % 1000);
            const bool oc = guarded([&] { at<FnFatal>(0x00011070)("rumble", 0, line, 7); });
            const bool rc = guarded([&] { AtFatal("rumble", 0, line, 7); });
            eq("AtFatal stops at exit", oc, rc);
            eq("screen flag", orig_u32(0x002C6060), g_atFatalScreenShown);
            eq("exit flag", orig_u32(0x002C605C), g_atFatalExiting);
            break;
        }
        }
        count_motors();
        compare_logs("rumble log");
        same_world(o, r);
        g_rumble_runs++;
    }
}

// ---- an image, and the error screen ----------------------------------------------------------------------------------
struct ScreenWorld {
    Renderer renderer;
    Owner owner;
    Material materials[3];
    Image images[8];
    AtImageStrip strips[8][7];
    Images database;
    Loader loader;
    Graphics graphics;
    Camera camera;
};

static void build_image(Image& im, AtImageStrip* strips, Material* materials, int side, int id) {
    im.side = side;
    im.id = id;
    im.m_flags = (rng() % 6 ? AtImage::kOwnsData : 0) | (rng() % 3 == 0 ? AtImage::kColorRectangle : 0) | (rng() % 4 == 0 ? AtImage::kFlipHorizontally : 0) |
                 (rng() % 4 == 0 ? AtImage::kFlipVertically : 0) | (rng() % 4 == 0 ? AtImage::kRead : 0);
    im.m_unknown14 = (unsigned short)(rng() % 1100);
    im.m_unknown16 = (unsigned short)(rng() % 600);
    for (int i = 0; i < 4; i++) im.m_colorRectangle.m_colors[i] = any_color();
    im.material = rng() % 4 ? &materials[rng() % 3] : nullptr;
    const unsigned n = 1 + rng() % 7;
    im.m_unknown1E = (unsigned short)(rng() % 9 == 0 ? 0 : n);
    const int rowStart = (int)(rng() % 64);
    int x = rowStart, y = 0;
    for (unsigned i = 0; i < 7; i++) {
        AtImageStrip& s = strips[i];
        if (i > 0 && rng() % 3 == 0) { x = rowStart; y += 64; }
        const int w = (int)(rng() % 300) - (rng() % 10 == 0 ? 20 : 0);
        const int h = (int)(rng() % 300);
        s.m_unknown00 = (int)rng(); s.m_unknown04 = (int)rng();
        s.x0 = x; s.y0 = y; s.x1 = x + w; s.y1 = y + h;
        x += w;
    }
    im.m_data = strips;
}

static void build_screen(ScreenWorld& w, int side, unsigned seed) {
    g_rng = seed;
    w.renderer.side = side;
    w.renderer.m_flags = (rng() % 2 ? AtGr2DRenderer::kQuarterPixelX : 0) | (rng() % 2 ? AtGr2DRenderer::kQuarterPixelY : 0) | (rng() % 4 == 0 ? 0x400u : 0u);
    w.renderer.m_viewport[0] = (int)(rng() % 100) - 20;
    w.renderer.m_viewport[1] = (int)(rng() % 100) - 20;
    w.renderer.m_viewport[2] = 300 + (int)(rng() % 700);
    w.renderer.m_viewport[3] = 200 + (int)(rng() % 500);
    for (int i = 0; i < 3; i++) {
        w.materials[i].id = i + 1;
        w.materials[i].m_constant = any_color();
    }
    w.renderer.m_material = rng() % 5 == 0 ? &w.materials[rng() % 3] : nullptr;
    w.owner.side = side;
    w.owner.matrices.side = side;
    w.owner.matrices.next = 0;
    for (int i = 0; i < 8; i++) build_image(w.images[i], w.strips[i], w.materials, side, i + 1);
    w.database.side = side;
    for (int i = 0; i < 8; i++) w.database.items[i] = &w.images[i];
    w.loader.side = side;
    w.loader.images = &w.database;
    w.graphics.side = side;
    w.graphics.camera = &w.camera;
    w.camera.side = side;
    w.camera.m_local = nullptr;
    w.camera.m_world = nullptr;
    w.camera.m_owner = nullptr;
    w.camera.m_matrix2C = nullptr;
    w.camera.m_matrix30 = rng() % 4 == 0 ? &w.owner.matrices.matrices[7] : nullptr;
    if (w.camera.m_matrix30) w.camera.m_local = &w.owner.matrices.matrices[6];
}

static ScreenWorld* g_sw[2];
static unsigned long long g_draws, g_strips, g_screens, g_frames_drawn;

static void count_strips() {
    for (size_t i = 0; i < g_log[0].size(); i += 2) {
        if (g_log[0][i] == 'd') g_strips++;
        if (g_log[0][i] == 'F') g_frames_drawn++;
    }
}

static void run_draws(int seed) {
    for (int k = 0; k < 20000; k++) {
        const unsigned s = (unsigned)seed * 31337u + (unsigned)k * 2654435761u;
        for (int side = 0; side < 2; side++) build_screen(*g_sw[side], side, s);
        g_rng = s ^ 0x3C3C3C3Cu;
        g_context = "draw " + std::to_string(k);
        const int which = (int)(rng() % 8);
        AtVector2 at_ = {frand(-200, 800), frand(-200, 600)};
        if (rng() % 6 == 0) at_ = {wild(), wild()};
        AtVector2 scale = {rng() % 3 ? 1.0f : frand(-1, 3), rng() % 3 ? 1.0f : frand(-1, 3)};
        const bool useScale = rng() % 3 == 0;
        AtColorRectangle colors;
        for (int i = 0; i < 4; i++) colors.m_colors[i] = any_color();
        const bool useColors = rng() % 3 == 0;
        AtRect clip = any_rect();
        const bool useClip = rng() % 3 == 0;
        at<FnDraw>(0x000582F0)(&g_sw[0]->images[which], nullptr, &g_sw[0]->renderer, &at_, useScale ? &scale : nullptr, useColors ? &colors : nullptr, useClip ? &clip : nullptr);
        g_sw[1]->images[which].Draw(&g_sw[1]->renderer, &at_, useScale ? &scale : nullptr, useColors ? &colors : nullptr, useClip ? &clip : nullptr);
        count_strips();
        compare_logs("draw log");
        g_draws++;
        // The camera alone.
        if (k % 4 == 0) {
            at<Fn1>(0x00037EB0)(&g_sw[0]->camera, nullptr, &g_sw[0]->owner);
            g_sw[1]->camera.Attach(&g_sw[1]->owner);
            compare_logs("camera log");
            for (int side = 0; side < 2; side++) {
                Camera& c = g_sw[side]->camera;
                Matrices& m = g_sw[side]->owner.matrices;
                note(side, '1', (uint32_t)(c.m_matrix2C - m.matrices));
                note(side, '2', (uint32_t)(c.m_matrix30 - m.matrices));
                note(side, '3', (uint32_t)(c.m_world - m.matrices));
                note(side, '4', (uint32_t)(c.m_local - m.matrices));
                note(side, '5', c.m_owner == &g_sw[side]->owner);
                note(side, '6', c.m_dirty);
                note(side, '7', bits(c.m_world->m[0]) ^ bits(c.m_local->m[5]));
            }
            compare_logs("camera state");
        }
    }
}

static const char* const kLanguages[] = {"GERMAN", "FRENCH", "ITALIAN", "ENGLISH", "", "GERMANY", "ITALIANX", "SPANISH", "german"};

static void set_app(int side, ScreenWorld& w, const char* language, bool loaded) {
    if (side == 0) {
        const uint32_t app = 0x002BF310;
        orig_u32(0x00304D58) = app;
        orig_u32(app + 0x6490) = loaded ? (uint32_t)(uintptr_t)&w.loader : 0;
        orig_u32(app + 0x5F90) = (uint32_t)(uintptr_t)&w.graphics;
        orig_u32(app + 0x5F98) = (uint32_t)(uintptr_t)&w.owner;
        orig_u32(app + 0x5F9C) = (uint32_t)(uintptr_t)&w.renderer;
        char* name = (char*)(uintptr_t)(app + 0x654C + 0x60);
        std::memset(name, 0, 8);
        std::memcpy(name, language, std::strlen(language) < 8 ? std::strlen(language) : 8);
    } else {
        App::s_instance = &g_app;
        g_app.m_dirtyDiskLoader = loaded ? &w.loader : nullptr;
        g_app.m_device->m_factory = &w.graphics;
        g_app.m_device->m_materialOwner = &w.owner;
        g_app.m_device->m_unknown20 = &w.renderer;
        std::memset(g_app.m_settings.m_language, 0, 8);
        std::memcpy(g_app.m_settings.m_language, language, std::strlen(language) < 8 ? std::strlen(language) : 8);
    }
}

static void run_screens(int seed) {
    for (int k = 0; k < 400; k++) {
        const unsigned s = (unsigned)seed * 7u + (unsigned)k * 40503u;
        for (int side = 0; side < 2; side++) build_screen(*g_sw[side], side, s);
        g_rng = s ^ 0x0F0F0F0Fu;
        const char* language = kLanguages[rng() % (sizeof kLanguages / sizeof kLanguages[0])];
        const bool loaded = rng() % 8 != 0;
        const int frames = 1 + (int)(rng() % 3);
        const int how = (int)(rng() % 3);  // the screen alone, through AtFatal, or through AtFatal with the screen shown already
        g_context = "screen " + std::to_string(k) + " " + language + " how " + std::to_string(how);
        for (int side = 0; side < 2; side++) set_app(side, *g_sw[side], language, loaded);
        set_rumble_globals(0, false);
        orig_u32(0x002C6060) = how == 2;
        g_atFatalScreenShown = how == 2;
        orig_u32(0x002C605C) = 0;
        g_atFatalExiting = 0;
        g_frames_left[0] = g_frames_left[1] = frames;
        bool oc, rc;
        if (how == 0) {
            oc = guarded([&] { at<Fn0>(0x0008E2B0)((void*)(uintptr_t)0x002BF310, nullptr); });
            rc = guarded([&] { g_app.DrawFatalScreen(); });
        } else {
            oc = guarded([&] { at<FnFatal>(0x00011070)("screen", 0, k, 0); });
            rc = guarded([&] { AtFatal("screen", 0, k, 0); });
        }
        eq("stopped", oc, rc);
        eq("screen flag", orig_u32(0x002C6060), g_atFatalScreenShown);
        eq("exit flag", orig_u32(0x002C605C), g_atFatalExiting);
        count_strips();
        compare_logs("screen log");
        g_screens++;
    }
}

// ---- the report ------------------------------------------------------------------------------------------------------
static unsigned long long g_reports;

static void report_both(int status, const char* format, const char* a, const char* b) {
    orig_u32(0x002C6060) = 1;
    g_atFatalScreenShown = 1;
    set_rumble_globals(0, false);
    g_context = std::string("report ") + std::to_string(status) + " [" + (format ? format : "null") + "]";
    guarded([&] { at<FnReport>(0x00014230)(status, format, a, b); });
    guarded([&] { AtReport(status, format, a, b); });
    compare_logs("report");
    g_reports++;
}

static void reportv_both(int status, const char* format, ...) {
    va_list list;
    va_start(list, format);
    orig_u32(0x002C6060) = 1;
    g_atFatalScreenShown = 1;
    set_rumble_globals(0, false);
    g_context = std::string("reportv ") + std::to_string(status);
    guarded([&] { at<FnReportV>(0x000141C0)(status, format, list); });
    guarded([&] { AtReportV(status, format, list); });
    va_end(list);
    compare_logs("reportv");
    g_reports++;
}

static void run_reports() {
    std::string longText(0x150, 'L');
    for (int status = -2; status < 25; status++) {
        report_both(status, "%s.%s", "DataXbox\\Levels\\NAME", "AGT");
        report_both(status, nullptr, "x", "y");
        report_both(status, "plain", nullptr, nullptr);
    }
    report_both(1, "%s", longText.c_str(), nullptr);
    report_both(3, "%s and %s", "", "z");
    reportv_both(2, "%d %u %x %c|%s", -7, 4000000000u, 0xBEEF, 'q', "tail");
    reportv_both(5, nullptr);
    // AtFatal's own message, with numbers for its file and line.
    for (int line = -1; line < 3; line++) {
        orig_u32(0x002C6060) = 1;
        g_atFatalScreenShown = 1;
        set_rumble_globals(0, false);
        g_context = "fatal line " + std::to_string(line);
        guarded([&] { at<FnFatal>(0x00011070)(longText.c_str(), 0, line, line); });
        guarded([&] { AtFatal(longText.c_str(), 0, line, line); });
        compare_logs("fatal message");
        g_reports++;
    }
}

// ---- main ------------------------------------------------------------------------------------------------------------
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
    int seed = 1;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::atoi(argv[++i]);
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 1) { std::printf("usage: oracle_fatal <default.xbe> [--entries FILE] [--mutate] [--seed N]\n"); return 2; }
    unsigned int control;
    _controlfp_s(&control, _PC_53, _MCW_PC);  // the game's main thread's precision: docs/portability.md
    if (!map_image(pos[0])) return 2;
    redirect(0x00085197, (const void*)o_launch);
    redirect(0x001254F3, (const void*)o_print);
    redirect(0x001254DB, (const void*)o_exit);
    redirect(0x00125559, (const void*)o_sprintf);
    redirect(0x00125ECE, (const void*)o_vsprintf);
    orig_u32(0x002C60CC) = (uint32_t)(uintptr_t)g_atMatrix.Identity;  // the table's Identity, for SetMatrices
    if (entries && !arm_entries(entries)) return 2;

    g_rw[0] = new RumbleWorld;
    g_rw[1] = new RumbleWorld;
    g_sw[0] = new ScreenWorld;
    g_sw[1] = new ScreenWorld;
    g_rng = (unsigned)seed;

    if (g_mutate) eq("deliberate", 1, 2);  // positive control of the harness
    run_names();
    run_rects();
    run_colors();
    run_reports();
    run_rumble(seed);
    run_draws(seed);
    run_screens(seed);

    report_entries();
    std::printf("%s: seed %d, %llu pieces, %llu reports, %llu rumble runs (%llu motor calls), %llu draws (%llu strips), %llu screens (%llu frames); %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", seed, g_pieces, g_reports, g_rumble_runs, g_motor_calls, g_draws, g_strips, g_screens, g_frames_drawn, g_checks,
                g_failures);
    std::fflush(stdout);
    ExitProcess(g_failures ? 1 : 0);  // the fakes stand on memory that is not the game's heap: their destructors are not run
}
