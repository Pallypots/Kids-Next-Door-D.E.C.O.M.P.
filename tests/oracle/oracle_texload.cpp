// Differential oracle for what a 2D texture does when it loads (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-19-texload.md.
//
//   oracle_texload <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// The image files are read from <tokenizer cases dir>\..\scratch\texload\img, which analysis-local/scratch/texload/extract.py writes (every distinct .DDS, .TGA and .BMP in the archives, with an index).
//
// What it does not show, first. The device is this harness's on both sides: making a texture, a level's surface, describing a surface and locking one (0x0020B160, 0x0020B220, 0x0020AAB0, 0x0020AAD0), the
// front and back buffers (0x00208430), and releasing (0x00205D10, 0x00205E20, 0x000FB630) are doubles that log their arguments and keep the bytes written into each locked level. So are the owner's format chooser
// and texture limits, whose classes are not recovered. So a green result shows that the original and the recovered code ask the device for the same textures (size, levels, format, usage), describe and lock the same
// levels, and leave the same bytes in each, from every real .DDS file and every 2D texture of every real .AGT; it does not show what a real device makes of them. The readers for other sources, and the branches
// that stage, convert or keep copies in memory, are not recovered: their first function stops on both sides (on the original's side through a redirect of that function), and the case is compared up to there.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "at/AtGrD3DDatabase.h"
#include "at/AtGrD3DTextureLoad.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 150).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 150).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static void slot(char c, uint32_t v = 0) { if (g_recording) g_log[g_side].add(c, v); }

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);

static int g_heap_token, g_map_heap_token;

// ---- the device: the same doubles on both sides ------------------------------------------------------------------------------------------------------------------------------------------------------------

struct FakeTexture;
struct FakeSurface {
    uint32_t id;
    FakeTexture* texture;
    unsigned level, width, height, format, pitch, rows;
    unsigned char* bits;
    size_t size;
    int locks;
};
struct FakeTexture {
    uint32_t id;
    unsigned width, height, levels, format, usage;
    FakeSurface* surfaces[16];
};

enum { kMaxFakes = 2048 };
// A level's bytes are compared with this much after them: a DDS file whose header claims more than it holds is copied row by row past the device's rows, by the original as by the recovered.
enum { kSlack = 0x4000 };
static FakeTexture* g_textures[2][kMaxFakes];
static FakeSurface* g_surfaces[2][kMaxFakes];
static int g_texture_count[2], g_surface_count[2];
static uint32_t g_next_id[2];
static bool g_create_fails;   // the device makes no texture
static int g_pitch_pad;       // bytes added to a locked level's rows
static unsigned g_multi = 0x11;  // what Describe says of multisampling

static bool compressed(unsigned format) { return format == 0x0C || format == 0x0E || format == 0x0F; }
static unsigned block_bytes(unsigned format) { return format == 0x0C ? 8 : 16; }

static uint32_t id_of(const void* p) {
    if (!p) return 0;
    for (int i = 0; i < g_texture_count[g_side]; i++) if (g_textures[g_side][i] == p) return g_textures[g_side][i]->id;
    for (int i = 0; i < g_surface_count[g_side]; i++) if (g_surfaces[g_side][i] == p) return g_surfaces[g_side][i]->id;
    return 0xBAD;
}

static void* __stdcall o_back_buffer(int which) {
    slot('K', (uint32_t)which);
    FakeTexture* t = (FakeTexture*)std::calloc(1, sizeof(FakeTexture));
    t->id = g_next_id[g_side]++;
    if (g_texture_count[g_side] < kMaxFakes) g_textures[g_side][g_texture_count[g_side]++] = t;
    return t;
}

static void* __stdcall o_create(unsigned w, unsigned h, unsigned depth, unsigned levels, unsigned usage, unsigned format, unsigned type) {
    slot('T', w);
    slot('t', h);
    slot('e', depth);
    slot('v', levels);
    slot('U', usage);
    slot('F', format);
    slot('y', type);
    if (g_create_fails || g_texture_count[g_side] >= kMaxFakes) return nullptr;
    FakeTexture* t = (FakeTexture*)std::calloc(1, sizeof(FakeTexture));
    t->id = g_next_id[g_side]++;
    t->width = w;
    t->height = h;
    t->levels = levels;
    t->format = format;
    t->usage = usage;
    g_textures[g_side][g_texture_count[g_side]++] = t;
    return t;
}

static void* __stdcall o_level(void* texture, unsigned level) {
    slot('V', id_of(texture));
    slot('l', level);
    FakeTexture* t = (FakeTexture*)texture;
    if (level >= 16 || id_of(texture) == 0xBAD || id_of(texture) == 0) return nullptr;
    if (!t->surfaces[level] && g_surface_count[g_side] < kMaxFakes) {
        FakeSurface* s = (FakeSurface*)std::calloc(1, sizeof(FakeSurface));
        s->id = g_next_id[g_side]++;
        s->texture = t;
        s->level = level;
        s->width = std::max(1u, t->width >> level);
        s->height = std::max(1u, t->height >> level);
        s->format = t->format;
        t->surfaces[level] = s;
        g_surfaces[g_side][g_surface_count[g_side]++] = s;
    }
    return t->surfaces[level];
}

static void __stdcall o_describe(void* surface, AtGrD3DSurfaceDesc* desc) {
    slot('Y', id_of(surface));
    std::memset(desc, 0, sizeof *desc);
    if (id_of(surface) == 0xBAD || id_of(surface) == 0) return;
    const FakeSurface* s = (const FakeSurface*)surface;
    desc->m_format = s->format;
    desc->m_multiSample = g_multi;
    const unsigned mx = g_multi == 0x11 ? 1 : (g_multi >> 4) & 0xF, my = g_multi == 0x11 ? 1 : g_multi & 0xF;
    desc->m_width = s->width * mx;
    desc->m_height = s->height * my;
}

static void __stdcall o_lock(void* surface, AtGrD3DLockedRect* locked, const void* rect, unsigned flags) {
    slot('Z', id_of(surface));
    slot('z', flags | (rect ? 0x10000 : 0));
    locked->m_pitch = 0;
    locked->m_bits = nullptr;
    if (id_of(surface) == 0xBAD || id_of(surface) == 0) return;
    FakeSurface* s = (FakeSurface*)surface;
    if (!s->bits) {
        const unsigned natural = compressed(s->format) ? std::max(1u, s->width / 4) * block_bytes(s->format) : s->width * 4;
        s->pitch = natural + g_pitch_pad;
        s->rows = compressed(s->format) ? std::max(1u, s->height / 4) : s->height;
        s->size = (size_t)s->pitch * s->rows;
        s->bits = (unsigned char*)std::malloc(s->size + kSlack);
        std::memset(s->bits, 0xEE, s->size + kSlack);
    }
    s->locks++;
    locked->m_pitch = (int)s->pitch;
    locked->m_bits = s->bits;
}

static void __stdcall o_release(void* r) { slot('X', id_of(r)); }
static void __stdcall o_before_release(void* r) { slot('B', id_of(r)); }
static void release_stage(void*, unsigned stage) { slot('G', stage); }
static void __fastcall o_release_stage(void* stages, void*, unsigned stage) { release_stage(stages, stage); }

void* AtGrD3DGetBackBuffer(int which) { return o_back_buffer(which); }
void* AtGrD3DCreateTexture(unsigned w, unsigned h, unsigned d, unsigned l, unsigned u, unsigned f, unsigned t) { return o_create(w, h, d, l, u, f, t); }
void* AtGrD3DSurfaceLevel(void* texture, unsigned level) { return o_level(texture, level); }
void AtGrD3DDescribe(void* surface, AtGrD3DSurfaceDesc* desc) { o_describe(surface, desc); }
void AtGrD3DLockRect(void* surface, AtGrD3DLockedRect* locked, const void* rect, unsigned flags) { o_lock(surface, locked, rect, flags); }
void AtGrD3DRelease(void* r) { o_release(r); }
void AtGrD3DBeforeRelease(void* r) { o_before_release(r); }
void AtGrD3DReleaseStage(void* stages, unsigned stage) { release_stage(stages, stage); }
void AtGrD3DLoadMaterial(AtGrD3DMaterial*, AtMaterialOwner*) { slot('?'); }

// What is not recovered stops on both sides: it logs where, and crashes, which the harness catches.
static void stop_here() { *(volatile int*)nullptr = 0; }
unsigned int AtGrD3DUnrecovered(unsigned int va, void*, unsigned int, unsigned int) {
    slot('u', va);
    stop_here();
    return 0;
}
template <uint32_t VA> static void __fastcall o_stop(void*, void*) {
    slot('u', VA);
    stop_here();
}
void* AtGrD3DAllocateContiguous(unsigned int, unsigned int) { slot('?'); return nullptr; }
void AtGrD3DFreeContiguous(void*) { slot('?'); }
void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char**, char**, char**) { slot('?'); }
void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char**, char**) { slot('?'); }
void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char**, char**) { slot('?'); }
void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char**, char**, char**, char*) { slot('?'); }

static void __cdecl pure_call() { slot('!'); stop_here(); }

static void free_fakes() {
    for (int side = 0; side < 2; side++) {
        for (int i = 0; i < g_surface_count[side]; i++) { std::free(g_surfaces[side][i]->bits); std::free(g_surfaces[side][i]); }
        for (int i = 0; i < g_texture_count[side]; i++) std::free(g_textures[side][i]);
        g_surface_count[side] = g_texture_count[side] = 0;
        g_next_id[side] = 0x100;
    }
}

static unsigned long long g_bytes_compared;
static unsigned g_levels_compared;

static void same_fakes() {
    eq("textures made", g_texture_count[0], g_texture_count[1]);
    eq("surfaces asked for", g_surface_count[0], g_surface_count[1]);
    for (int i = 0; i < std::min(g_texture_count[0], g_texture_count[1]); i++) {
        const FakeTexture* a = g_textures[0][i];
        const FakeTexture* b = g_textures[1][i];
        eq("made: id", a->id, b->id);
        eq("made: width", a->width, b->width);
        eq("made: height", a->height, b->height);
        eq("made: levels", a->levels, b->levels);
        eq("made: format", a->format, b->format);
    }
    for (int i = 0; i < std::min(g_surface_count[0], g_surface_count[1]); i++) {
        const FakeSurface* a = g_surfaces[0][i];
        const FakeSurface* b = g_surfaces[1][i];
        eq("level: id", a->id, b->id);
        eq("level: level", a->level, b->level);
        eq("level: locks", a->locks, b->locks);
        eq("level: locked", a->bits != nullptr, b->bits != nullptr);
        if (a->bits && b->bits && a->size == b->size) {
            eq("level: bytes", std::memcmp(a->bits, b->bits, a->size + kSlack), 0);
            g_bytes_compared += a->size;
            g_levels_compared++;
        }
    }
}

// ---- the owner's format chooser and limits -------------------------------------------------------------------------------------------------------------------------------------------------------------------

static int g_chooser_mode;  // 0 as asked; 1 swizzling dropped; 2 a compressed format as 16-bit 1-5-5-5 linear; 3 everything 8-8-8-8 swizzled; 4 compressed kept, others as 2
static unsigned format_word(const AtPixelFormat* f) { return f->m_flags | (unsigned)f->m_bits << 16; }
static void choose(const AtPixelFormat* asked, AtPixelFormat* chosen) {
    const bool dxt = (asked->m_flags & 0x7C0) != 0;
    switch (g_chooser_mode) {
    case 1: chosen->m_flags &= ~AtPixelFormat::kSwizz; break;
    case 2: if (dxt) chosen->SetRgba(16, 0x7C00, 0x3E0, 0x1F, 0x8000, 0); break;
    case 3: chosen->SetRgba(32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, 1); break;
    case 4: if (!dxt) chosen->SetRgba(16, 0x7C00, 0x3E0, 0x1F, 0x8000, 0); break;
    case 5: chosen->SetIndex(8, 0xFF, 0, 1); break;  // everything 8-bit indexed, swizzled
    case 6: chosen->SetRgba(32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0); break;  // everything 8-8-8-8 linear
    default: break;
    }
}
struct Chooser : public AtGrD3DFormatChooser {
    void Slot0() override { slot('!'); } void Slot1() override { slot('!'); } void Slot2() override { slot('!'); } void Slot3() override { slot('!'); } void Slot4() override { slot('!'); }
    void Slot5() override { slot('!'); } void Slot6() override { slot('!'); } void Slot7() override { slot('!'); } void Slot8() override { slot('!'); } void Slot10() override { slot('!'); }
    void Slot11() override { slot('!'); }
    void ChooseRenderTarget(const AtPixelFormat* a, AtPixelFormat* c) override { slot('h', format_word(a)); slot('m', a->m_mask0 ^ a->m_mask3); choose(a, c); }
    void ChooseTexture(const AtPixelFormat* a, AtPixelFormat* c) override { slot('H', format_word(a)); slot('m', a->m_mask0 ^ a->m_mask3); choose(a, c); }
};
static unsigned g_min_w = 1, g_max_w = 4096, g_min_h = 1, g_max_h = 4096;
struct Limits : public AtGrD3DTextureLimits {
    void Slot0() override { slot('!'); }
    unsigned int MinWidth(unsigned int bits) override { slot('1', bits); return g_min_w; }
    unsigned int MaxWidth(unsigned int bits) override { slot('2', bits); return g_max_w; }
    unsigned int MinHeight(unsigned int bits) override { slot('3', bits); return g_min_h; }
    unsigned int MaxHeight(unsigned int bits) override { slot('4', bits); return g_max_h; }
};
struct TestOwner : public AtGrD3DOwner {
    Chooser* m_chooser = nullptr;
    AtGrD3DFormatChooser* FormatChooser() override { slot('C'); return m_chooser; }
    AtMatrixDevice* MatrixDevice() override { slot('!'); return nullptr; }
};
static TestOwner* g_own[2];
static Chooser g_chooser[2];
static Limits g_limits[2];

static void make_owners() {
    for (int side = 0; side < 2; side++) {
        void* p = std::calloc(1, sizeof(TestOwner));
        g_own[side] = new (p) TestOwner;
        g_own[side]->m_chooser = &g_chooser[side];
        g_own[side]->m_textureLimits = &g_limits[side];
        g_own[side]->m_textureSpaces.Init(8);
        g_own[side]->m_materialSpaces.Init(8);
        g_own[side]->m_shaderSpaces.Init(8);
    }
}
static void set_caps(unsigned caps, unsigned mask) {
    for (int side = 0; side < 2; side++) {
        g_limits[side].m_caps = caps;
        g_limits[side].m_capsMask = mask;
    }
}

// ---- files: a tree of paths beneath both sides -----------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Blob { const unsigned char* data; size_t size; };
static bool g_read_fails;

class TreeFile : public AtIoFile {
public:
    int side = 0;
    Blob blob{};
    int Close() override { g_log[side].add('c'); return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { g_log[side].add('S', offset); m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        g_log[side].add('r', n);
        if (g_read_fails) return AtIoFile::kIoError;
        const size_t have = blob.size - m_position;
        if (n > have) n = (unsigned)have;
        if (n) std::memcpy(out, blob.data + m_position, n);
        m_position += n;
        if (done) *done = n;
        return n ? 0 : (int)AtIoFile::kEndOfFile;
    }
    int Write(const void*, unsigned int, unsigned int*) override { return kUnsupported; }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class TreeFs : public AtIoFileSystem {
public:
    std::map<std::string, Blob> files;
    TreeFile file[2];
    const Blob* find(const char* path) {
        const bool was = g_recording;
        g_recording = false;
        m_path.Resolve(path);
        const Blob* found = nullptr;
        {
            std::string key(m_path.m_full);  // freed before logging resumes
            for (char& c : key) { if (c >= 'a' && c <= 'z') c -= 32; if (c == '/') c = '\\'; }
            auto f = files.find(key);
            if (f != files.end()) found = &f->second;
        }
        g_recording = was;
        return found;
    }
    int Open(const char* path, int flags, AtIoFile** out) override {
        slot('O', (uint32_t)flags);
        record_message(path, g_side);
        const Blob* b = find(path);
        if (!b) return AtIoFile::kNotFound;
        TreeFile& f = file[g_side];
        f.side = g_side;
        f.blob = *b;
        f.m_position = 0;
        f.m_size = (unsigned)b->size;
        f.m_sizeCopy = (unsigned)b->size;
        f.m_flags = (flags & 0x21) ? 0x0C : 2;
        strcpy_s(f.m_path, m_path.m_full);
        *out = &f;
        return 0;
    }
    int Close(AtIoFile*) override { slot('C'); return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char* path) override { slot('M'); return find(path) ? 0 : 1; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

static TreeFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];

// ---- making, loading, comparing and destroying textures ------------------------------------------------------------------------------------------------------------------------------------------------------

struct Item {
    AtGrD3DTexture2D* t;
    AtGrD3DSurface2D* s;
    unsigned n;
};

static Item make_item(int side, unsigned surfaces) {
    Item it{};
    it.n = surfaces;
    void* p = std::malloc(sizeof(AtGrD3DTexture2D));
    std::memset(p, 0xCD, sizeof(AtGrD3DTexture2D));
    unsigned char* q = (unsigned char*)std::malloc(sizeof(AtGrD3DSurface2D) * (surfaces ? surfaces : 1));
    std::memset(q, 0xCD, sizeof(AtGrD3DSurface2D) * (surfaces ? surfaces : 1));
    if (side == 0) {
        at<Fn0>(0x000FC630)(p, nullptr);
        for (unsigned i = 0; i < surfaces; i++) at<Fn0>(0x000FCEE0)(q + i * sizeof(AtGrD3DSurface2D), nullptr);
    } else {
        new (p) AtGrD3DTexture2D;
        for (unsigned i = 0; i < surfaces; i++) new (q + i * sizeof(AtGrD3DSurface2D)) AtGrD3DSurface2D;
    }
    it.t = (AtGrD3DTexture2D*)p;
    it.s = (AtGrD3DSurface2D*)q;
    it.t->m_surfaces = it.s;
    return it;
}

static void destroy_item(Item& o, Item& r) {
    { On on(0); at<Fn0>(0x000FCCC0)(o.t, nullptr); for (unsigned i = 0; i < o.n; i++) at<Fn0>(0x000FD8C0)(&o.s[i], nullptr); }
    { On on(1); r.t->~AtGrD3DTexture2D(); for (unsigned i = 0; i < r.n; i++) r.s[i].~AtGrD3DSurface2D(); }
    compare_logs("destroy");
    std::free(o.t); std::free(o.s); std::free(r.t); std::free(r.s);
}

static long long owner_id(int side, const void* p) { return p == g_own[side] ? 1 : p ? 2 : 0; }

static void same_surface(int side0_dummy, const AtGrD3DSurface2D& a, const AtGrD3DSurface2D& b, const Item& o, const Item& r) {
    (void)side0_dummy;
    g_side = 0;
    const uint32_t ia = id_of(a.m_surface);
    g_side = 1;
    const uint32_t ib = id_of(b.m_surface);
    g_side = 0;
    eq("surface's format", std::memcmp(&a.m_format, &b.m_format, sizeof a.m_format), 0);
    eq("surface's key", *(const uint32_t*)a.m_colorKey, *(const uint32_t*)b.m_colorKey);
    eq("surface's usage", a.m_unknown1C, b.m_unknown1C);
    eq("surface's flags", a.m_flags, b.m_flags);
    eq("surface's width", a.m_unknown20, b.m_unknown20);
    eq("surface's height", a.m_unknown22, b.m_unknown22);
    eq("surface's owner", owner_id(0, a.m_unknown24), owner_id(1, b.m_unknown24));
    eq("surface's surface", ia, ib);
    eq("surface's texture", a.m_unknown3C == o.t ? 1 : a.m_unknown3C ? 2 : 0, b.m_unknown3C == r.t ? 1 : b.m_unknown3C ? 2 : 0);
    eq("surface's previous", a.m_unknown40 ? a.m_unknown40 - o.s : -1, b.m_unknown40 ? b.m_unknown40 - r.s : -1);
    eq("surface's +44", a.m_unknown44 != nullptr, b.m_unknown44 != nullptr);
    eq("surface's +48", a.m_unknown48 != nullptr, b.m_unknown48 != nullptr);
    eq("surface's next", a.m_next != nullptr, b.m_next != nullptr);
    eq("palette's count", a.m_palette.m_count, b.m_palette.m_count);
    eq("palette's resource", a.m_palette.m_resource != nullptr, b.m_palette.m_resource != nullptr);
}

static void same_item(const Item& o, const Item& r) {
    const AtGrD3DTexture2D* a = o.t;
    const AtGrD3DTexture2D* b = r.t;
    eq("item's name", std::memcmp(a->m_name, b->m_name, 8), 0);
    eq("item's usage", a->m_usage, b->m_usage);
    eq("item's flags", a->m_flags, b->m_flags);
    eq("item's levels", a->m_levels, b->m_levels);
    eq("item's levels asked", a->m_levelsAsked, b->m_levelsAsked);
    eq("item's owner", owner_id(0, a->m_owner), owner_id(1, b->m_owner));
    g_side = 0;
    const uint32_t ta = id_of(a->m_texture);
    g_side = 1;
    const uint32_t tb = id_of(b->m_texture);
    g_side = 0;
    eq("item's texture", ta, tb);
    eq("item's surfaces", a->m_surfaces == o.s, b->m_surfaces == r.s);
    for (unsigned i = 0; i < std::min(o.n, r.n); i++) same_surface(0, o.s[i], r.s[i], o, r);
}

// The DDS reader every texture shares, on each side.
static AtGrDdsPixelMap* map_of(int side) { return side == 0 ? (AtGrDdsPixelMap*)(uintptr_t)0x003B13E8 : &AtGrDdsPixelMap::s_map; }

static void same_map(const char* what) {
    const AtGrDdsPixelMap* a = map_of(0);
    const AtGrDdsPixelMap* b = map_of(1);
    eq(what, std::memcmp(a->m_name, b->m_name, sizeof a->m_name), 0);
    eq("map's width", a->m_width, b->m_width);
    eq("map's height", a->m_height, b->m_height);
    eq("map's pitch", a->m_pitch, b->m_pitch);
    eq("map's format", std::memcmp(&a->m_format, &b->m_format, sizeof a->m_format), 0);
    eq("map's file system", a->m_file.m_fs == g_fs, b->m_file.m_fs == g_fs);
    eq("map's file", a->m_file.m_file != nullptr, b->m_file.m_file != nullptr);
    eq("map's palette count", a->m_paletteCount, b->m_paletteCount);
    eq("map's file size", a->m_fileSize, b->m_fileSize);
    eq("map's buffer", a->m_buffer != nullptr, b->m_buffer != nullptr);
    eq("map's data", a->m_data ? (a->m_buffer ? a->m_data - a->m_buffer : a->m_data - a->m_header) : -1, b->m_data ? (b->m_buffer ? b->m_data - b->m_buffer : b->m_data - b->m_header) : -1);
    eq("map's depth", a->m_depth, b->m_depth);
    eq("map's level size", a->m_levelSize, b->m_levelSize);
    eq("map's levels", a->m_levels, b->m_levels);
    eq("map's kind", a->m_kind, b->m_kind);
    eq("map's header", std::memcmp(a->m_header, b->m_header, sizeof a->m_header), 0);
}

static unsigned g_loads, g_stops, g_crash_free;

// Slot 12 on each side; either may stop. Stopping must agree, and so must everything before it.
static void load_both(Item& o, Item& r, const char* what) {
    const bool oc = guarded([&] { On on(0); at<Fn1>(0x000FCAD0)(o.t, nullptr, g_own[0]); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r.t->OnLoaded(g_own[1]); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    eq("stopped", oc, rc);
    g_loads++;
    if (oc) g_stops++; else g_crash_free++;
    compare_logs(what);
    same_item(o, r);
    same_map("map's name");
    same_fakes();
}

struct Desc {
    const char* name;
    unsigned usage;
    unsigned short flags;
    unsigned levels;
    unsigned surfaces;
    const unsigned char* transparent;
};

static void describe(Item& it, const Desc& d) {
    std::memset(it.t->m_name, 0, 8);
    std::memcpy(it.t->m_name, d.name, std::min<size_t>(8, std::strlen(d.name)));
    it.t->m_usage = d.usage;
    it.t->m_flags = d.flags;
    it.t->m_levelsAsked = (unsigned char)d.levels;
    it.t->m_levels = (unsigned char)d.levels;
    if (d.transparent) std::memcpy(it.t->m_transparent, d.transparent, 4);
}

// One texture made, loaded, its untouched slots tried, released and destroyed.
static void run_one(const Desc& d, const std::string& context) {
    g_context = context;
    if (getenv("TEXLOAD_TRACE")) std::fprintf(stderr, "%s\n", context.c_str());
    free_fakes();
    Item o = make_item(0, d.surfaces);
    Item r = make_item(1, d.surfaces);
    describe(o, d);
    describe(r, d);
    load_both(o, r, "load");
    // Loaded again: nothing happens.
    {
        const bool oc = guarded([&] { On on(0); at<Fn1>(0x000FCAD0)(o.t, nullptr, g_own[0]); });
        const bool rc = guarded([&] { On on(1); r.t->OnLoaded(g_own[1]); });
        eq("again: stopped", oc, rc);
        compare_logs("load again");
    }
    // Each loaded level locked to write with the surface's flag 0x20 set, which makes even a DDS level stage; then unlocked.
    for (unsigned lv = 0; lv < std::min<unsigned>(std::min(o.t->m_levels, r.t->m_levels), std::min(o.n, r.n)); lv++) {
        if (!(o.s[lv].m_flags & AtGrD3DSurface2D::kLoaded)) continue;
        o.s[lv].m_flags |= 0x20;
        r.s[lv].m_flags |= 0x20;
        void* ba = nullptr; void* bb = nullptr; int pa = 0, pb = 0;
        const bool oc = guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, void**, int*, unsigned)>(0x000FE260)(&o.s[lv], nullptr, &ba, &pa, 2); });
        const bool rc = guarded([&] { On on(1); r.s[lv].Lock(&bb, &pb, 2); });
        eq("lock, staged: stopped", oc, rc);
        eq("lock, staged: pitch", pa, pb);
        eq("lock, staged: flags", o.s[lv].m_flags, r.s[lv].m_flags);
        compare_logs("lock, staged");
        o.s[lv].m_flags &= ~0x20;
        r.s[lv].m_flags &= ~0x20;
    }
    destroy_item(o, r);
    same_fakes();
}

// ---- cases -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static void put(const std::string& path, const std::vector<unsigned char>& data) {
    std::string key = path;
    for (char& c : key) { if (c >= 'a' && c <= 'z') c -= 32; if (c == '/') c = '\\'; }
    g_fs->files[key] = Blob{data.data(), data.size()};
}

static void reset_settings() {
    g_chooser_mode = 0;
    g_pitch_pad = 0;
    g_multi = 0x11;
    g_create_fails = false;
    g_read_fails = false;
    g_min_w = g_min_h = 1;
    g_max_w = g_max_h = 4096;
    set_caps(0x00110000, 0);
    g_fill = 0xCD;
}

static unsigned g_dds_files;

// Every real .DDS file, under a few descriptions.
static void run_dds(const std::vector<unsigned char>& data, const std::string& name) {
    g_fs->files.clear();
    put("TEX.DDS", data);
    put("TEX9.DDS", data);
    g_dds_files++;
    if (getenv("TEXLOAD_TRACE") && g_dds_files % 100 == 0) std::fprintf(stderr, "%u .DDS at %lu ms\n", g_dds_files, GetTickCount());
    const unsigned dds = AtTextureBase::kSourceDds;
    static const unsigned char key[4] = {1, 2, 3, 0};
    reset_settings();
    run_one(Desc{"TEX", dds, 0, 1, 1, nullptr}, name + " one level");
    run_one(Desc{"TEX9", dds, AtTextureBase::kPlatformSource, 9, 9, nullptr}, name + " nine levels, platform");
    g_pitch_pad = 32;
    run_one(Desc{"TEX9", dds | AtTextureBase::kFrequentModify, AtTextureBase::kTransparentColor, 4, 4, key}, name + " four levels, padded rows, transparent, often changed");
    reset_settings();
    // A device whose least side is 8 and which makes only square textures, and one choosing a format the file cannot be copied into.
    if ((g_dds_files & 7) == 0) {
        g_min_w = g_min_h = 8;
        set_caps(0, 0);
        run_one(Desc{"TEX9", dds, 0, 9, 9, nullptr}, name + " least side 8, square");
        reset_settings();
        g_chooser_mode = 2;
        run_one(Desc{"TEX", dds, 0, 2, 2, nullptr}, name + " chosen 1-5-5-5");
        reset_settings();
    }
}

// Written cases: each word and error path.
static std::vector<unsigned char> dds_file(unsigned w, unsigned h, unsigned levels, const char* fourcc, unsigned extra_flags = 0, unsigned caps2 = 0) {
    const unsigned block = !std::strcmp(fourcc, "DXT1") ? 8 : 16;
    unsigned size = 0, first = std::max(1u, w / 4) * std::max(1u, h / 4) * block;
    for (unsigned i = 0; i < std::max(levels, 1u); i++) size += std::max(first >> (2 * i), block);
    std::vector<unsigned char> d(128 + size);
    auto put32 = [&](size_t at, unsigned v) { std::memcpy(&d[at], &v, 4); };
    std::memcpy(&d[0], "DDS ", 4);
    put32(4, 0x7C);
    put32(8, 0x81007 | (levels ? 0x20000 : 0) | extra_flags);
    put32(12, h);
    put32(16, w);
    put32(20, first);
    put32(28, levels);
    put32(76, 32);
    put32(80, 4);
    std::memcpy(&d[84], fourcc, 4);
    put32(108, 0x1000 | (levels ? 0x400008 : 0));
    put32(112, caps2);
    for (size_t i = 128; i < d.size(); i++) d[i] = (unsigned char)(i * 7 + (i >> 8));
    return d;
}

static std::vector<std::vector<unsigned char>> g_keep;  // written files outlive their cases

static void run_written() {
    const unsigned dds = AtTextureBase::kSourceDds;
    g_keep.clear();
    g_keep.reserve(64);
    auto file = [&](const char* path, std::vector<unsigned char> d) { g_keep.push_back(std::move(d)); put(path, g_keep.back()); return &g_keep.back(); };
    g_fs->files.clear();
    reset_settings();
    // Formats and shapes.
    file("A.DDS", dds_file(64, 64, 7, "DXT1"));
    file("B.DDS", dds_file(32, 128, 0, "DXT3"));
    file("C.DDS", dds_file(256, 64, 3, "DXT5"));
    file("D.DDS", dds_file(16, 16, 5, "DXT2"));
    file("E.DDS", dds_file(8, 8, 0, "DXT4"));
    file("V.DDS", dds_file(16, 16, 0, "DXT1", 0x800000, 0x200000));        // a volume
    file("Q.DDS", dds_file(16, 16, 2, "DXT1", 0, 0xFE00));                 // a cube's caps
    file("F.DDS", dds_file(16, 16, 0, "BAD1"));                            // an unknown four-character code
    file("NAME.WIT", dds_file(8, 8, 0, "DXT1"));
    std::vector<unsigned char> magic = dds_file(8, 8, 0, "DXT1");
    magic[0] = 'X';
    file("M.DDS", magic);
    std::vector<unsigned char> hdr = dds_file(8, 8, 0, "DXT1");
    hdr[4] = 0x7B;
    file("H.DDS", hdr);
    std::vector<unsigned char> pitch = dds_file(8, 8, 0, "DXT1");
    pitch[8] = 0x0F;  // a pitch, not a linear size
    pitch[10] = 0;
    file("P.DDS", pitch);
    std::vector<unsigned char> none = dds_file(8, 8, 0, "DXT1");
    none[8] = 0x07;
    none[10] = 0;
    file("N.DDS", none);
    std::vector<unsigned char> rgb = dds_file(4, 4, 0, "DXT1");
    rgb[80] = 0x41;  // RGB with alpha: no code
    rgb[88] = 32;
    rgb[92] = 0; rgb[93] = 0; rgb[94] = 0xFF; rgb[95] = 0;
    file("R.DDS", rgb);
    std::vector<unsigned char> rgb2 = rgb;
    rgb2[80] = 0x40;
    file("S.DDS", rgb2);
    std::vector<unsigned char> lum = rgb;
    lum[80] = 0x00; lum[82] = 0x02;
    file("L.DDS", lum);
    std::vector<unsigned char> shortened = dds_file(8, 8, 0, "DXT1");
    shortened.resize(60);  // shorter than its header
    file("T.DDS", shortened);
    std::vector<unsigned char> trunc = dds_file(64, 64, 0, "DXT3");
    trunc.resize(trunc.size() - 100);  // fewer bytes than its levels
    file("U.DDS", trunc);
    std::vector<unsigned char> nocaps = dds_file(8, 8, 0, "DXT1");
    nocaps[8] = 0x06;  // height, width, pixel format; no caps
    file("G.DDS", nocaps);

    static const unsigned char key[4] = {9, 8, 7, 6};
    struct W { const char* name; unsigned usage; unsigned short flags; unsigned levels; const char* what; };
    const W ws[] = {
        {"A", dds, 0, 9, "DXT1 seven levels, nine asked"},
        {"A", dds, AtTextureBase::kPlatformSource, 3, "DXT1 three asked"},
        {"B", dds, 0, 1, "DXT3 tall"},
        {"C", dds, 0, 9, "DXT5 wide"},
        {"D", dds, AtTextureBase::kTransparentColor, 5, "DXT2 transparent"},
        {"E", dds, 0, 1, "DXT4"},
        {"V", dds, 0, 1, "a volume"},
        {"Q", dds, 0, 2, "a cube's caps"},
        {"F", dds, 0, 1, "unknown code"},
        {"NAME.WIT", dds, 0, 1, "a name with a dot"},
        {"M", dds, 0, 1, "bad magic"},
        {"H", dds, 0, 1, "bad header size"},
        {"P", dds, 0, 1, "a pitch"},
        {"N", dds, 0, 1, "neither pitch nor size"},
        {"R", dds, 0, 1, "RGB with alpha"},
        {"S", dds, 0, 1, "RGB"},
        {"L", dds, 0, 1, "luminance"},
        {"T", dds, 0, 1, "short header"},
        {"U", dds, 0, 1, "short file"},
        {"MISSING", dds, 0, 1, "not found"},
        {"A", dds | AtTextureBase::kRenderTarget, 0, 3, "a render target"},
        {"A", dds | AtTextureBase::kFrequentModify, 0, 3, "often changed"},
        {"A", dds | 0x80, 0, 3, "usage 0x80"},
        {"B", dds | 0x80, 0, 1, "usage 0x80, tall"},
        {"G", dds, 0, 1, "no caps in the header"},
        {"A", dds, AtTextureBase::kBackBufferSource, 3, "back buffer"},
        {"A", dds, AtTextureBase::kFrontBufferSource, 3, "front buffer"},
        {"A", dds, AtTextureBase::kFrontBufferSource | AtTextureBase::kBackBufferSource, 3, "both buffers"},
        {"A", AtTextureBase::kSourceTga, 0, 1, "TGA source"},
        {"A", AtTextureBase::kSourceTm2, 0, 1, "TM2 source"},
        {"A", AtTextureBase::kSourceBmp, 0, 1, "BMP source"},
        {"A", dds, AtTextureBase::kPending, 3, "already loaded"},
    };
    for (const W& w : ws) {
        reset_settings();
        run_one(Desc{w.name, w.usage, w.flags, w.levels, std::max(w.levels, 1u), (w.flags & AtTextureBase::kTransparentColor) ? key : nullptr}, std::string("written: ") + w.what);
    }
    // The device: each chooser, limits, padding, multisampling, a texture not made, a read that fails.
    for (int mode = 1; mode <= 6; mode++) {
        reset_settings();
        g_chooser_mode = mode;
        run_one(Desc{"A", dds, 0, 9, 9, nullptr}, "written: chooser " + std::to_string(mode));
        run_one(Desc{"A", dds | AtTextureBase::kRenderTarget, 0, 2, 2, nullptr}, "written: chooser, render target " + std::to_string(mode));
    }
    reset_settings();
    g_min_w = 16; g_min_h = 32; g_max_w = 32; g_max_h = 64;
    run_one(Desc{"C", dds, 0, 9, 9, nullptr}, "written: limits");
    run_one(Desc{"B", dds, 0, 9, 9, nullptr}, "written: limits, tall");
    reset_settings();
    set_caps(0x100000, 4); run_one(Desc{"B", dds, 0, 1, 1, nullptr}, "written: caps, mask 4");
    set_caps(0x100000 | 0x01000000, 8); run_one(Desc{"C", dds, 0, 3, 3, nullptr}, "written: caps, mask 8");
    set_caps(0, 0); run_one(Desc{"C", dds, 0, 3, 3, nullptr}, "written: square only");
    reset_settings();
    g_pitch_pad = 8;
    run_one(Desc{"C", dds, 0, 9, 9, nullptr}, "written: padded rows");
    reset_settings();
    // The reader's heap gone when the file is first read whole.
    *(void**)(uintptr_t)0x002CB090 = nullptr;
    AtGrIPixelMap::s_heap = nullptr;
    run_one(Desc{"A", dds, 0, 3, 3, nullptr}, "written: no reader heap");
    *(void**)(uintptr_t)0x002CB090 = &g_map_heap_token;
    AtGrIPixelMap::s_heap = &g_map_heap_token;
    reset_settings();
    g_create_fails = true;
    run_one(Desc{"A", dds, 0, 3, 3, nullptr}, "written: no texture made");
    reset_settings();
    g_read_fails = true;
    run_one(Desc{"A", dds, 0, 3, 3, nullptr}, "written: read fails");
    reset_settings();
    g_own[0]->m_noTextures = g_own[1]->m_noTextures = 4;
    run_one(Desc{"A", dds, 0, 3, 3, nullptr}, "written: owner keeps copies");
    run_one(Desc{"A", dds | AtTextureBase::kInfrequentModify, 0, 3, 3, nullptr}, "written: owner keeps copies, seldom changed");
    g_own[0]->m_noTextures = g_own[1]->m_noTextures = 0;
}

// A texture made from a ProgSource, and its levels locked: each side's own source block.
static void run_prog() {
    struct P { int w, h; unsigned short flags, bits; unsigned m0, m1, m2, m3; unsigned usage; unsigned short tflags; unsigned levels; int mode; unsigned multi; const char* what; };
    const unsigned prog = AtTextureBase::kSourceProg;
    const P ps[] = {
        {128, 128, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog, 0, 1, 0, 0x11, "the real one"},
        {64, 32, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0, prog, AtTextureBase::kTransparentColor, 4, 0, 0x11, "no alpha, transparent"},
        {64, 32, 1, 16, 0x7C00, 0x3E0, 0x1F, 0, prog, AtTextureBase::kTransparentColor, 4, 0, 0x24, "16 bits, transparent, multisampled"},
        {64, 32, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog, AtTextureBase::kTransparentColor, 2, 0, 0x11, "alpha, transparent"},
        {33, 17, 1, 16, 0x7C00, 0x3E0, 0x1F, 0x8000, prog, 0, 2, 0, 0x11, "16 bits, odd sizes"},
        {9, 9, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog, 0, 2, 0, 0x11, "nine"},
        {33, 17, 1, 16, 0x7C00, 0x3E0, 0x1F, 0x8000, prog, 0, 1, 0, 0x11, "16 bits, 33 wide"},  // rows of 66 bytes, a copy's pitch rounded to 68
        {64, 64, 0x400, 8, 0, 0, 0, 0, prog, 0, 3, 0, 0x11, "DXT5"},
        {64, 64, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog, 0x20, 2, 0, 0x11, "palette flag set before"},
        {64, 64, 4, 8, 0xFF, 0, 0, 0, prog, AtTextureBase::kTransparentColor, 2, 0, 0x11, "indexed, transparent"},
        {64, 64, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog | AtTextureBase::kFrequentModify, 0, 3, 0, 0x11, "often changed"},
        {64, 64, 0x40, 4, 0, 0, 0, 0, prog, 0, 3, 0, 0x11, "DXT1"},
        {64, 64, 0x100, 8, 0, 0, 0, 0, prog, 0, 3, 0, 0x11, "DXT3"},
        {64, 64, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog, 0, 3, 1, 0x11, "linear"},
        {64, 64, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog | AtTextureBase::kRenderTarget, 0, 1, 0, 0x11, "render target"},
        {33, 17, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, prog, 0, 3, 0, 0x11, "odd sizes"},
    };
    for (const P& p : ps) {
        reset_settings();
        g_chooser_mode = p.mode;
        g_multi = p.multi;
        if (!std::strcmp(p.what, "16 bits, odd sizes")) g_max_w = 32;  // one past the widest
        g_context = std::string("prog: ") + p.what;
        free_fakes();
        Item o = make_item(0, p.levels);
        Item r = make_item(1, p.levels);
        static const unsigned char key[4] = {0x10, 0x20, 0x30, 0x40};
        AtProgTextureSource src[2];
        for (int side = 0; side < 2; side++) {
            Item& it = side ? r : o;
            describe(it, Desc{"PROG", p.usage, p.tflags, p.levels, p.levels, (p.tflags & AtTextureBase::kTransparentColor) ? key : nullptr});
            std::memset(&src[side], 0, sizeof src[side]);
            src[side].m_width = p.w;
            src[side].m_height = p.h;
            src[side].m_format.Set(p.flags, p.bits, p.m0, p.m1, p.m2, p.m3);
            // Freed on loading: a block from each side's heap.
            AtProgTextureSource* block = (AtProgTextureSource*)(side ? operator new(0x1C) : o_new(0x1C));
            std::memcpy(block, &src[side], sizeof src[side]);
            it.t->m_progSource = block;
        }
        load_both(o, r, "load");
        // Each level locked to write, read and both, then unlocked; its own copy in memory as well.
        for (unsigned lv = 0; lv < std::min<unsigned>(o.t->m_levels, r.t->m_levels) && lv < p.levels; lv++) {
            for (unsigned flags : {2u, 1u, 0u}) {
                void* ba = nullptr; void* bb = nullptr; int pa = 0, pb = 0;
                const bool oc = guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, void**, int*, unsigned)>(0x000FE260)(&o.s[lv], nullptr, &ba, &pa, flags); });
                const bool rc = guarded([&] { On on(1); r.s[lv].Lock(&bb, &pb, flags); });
                eq("lock: stopped", oc, rc);
                eq("lock: pitch", pa, pb);
                eq("lock: flags", o.s[lv].m_flags, r.s[lv].m_flags);
                const bool ou = guarded([&] { On on(0); at<Fn0>(0x000FE320)(&o.s[lv], nullptr); });
                const bool ru = guarded([&] { On on(1); r.s[lv].Unlock(); });
                eq("unlock: stopped", ou, ru);
                eq("unlock: flags", o.s[lv].m_flags, r.s[lv].m_flags);
                compare_logs("lock");
            }
            static unsigned char copy[2][64];
            o.s[lv].m_unknown48 = copy[0];
            r.s[lv].m_unknown48 = copy[1];
            for (unsigned flags : {1u, 2u}) {
                void* ba = nullptr; void* bb = nullptr; int pa = 0, pb = 0;
                const bool oc = guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, void**, int*, unsigned)>(0x000FE260)(&o.s[lv], nullptr, &ba, &pa, flags); });
                const bool rc = guarded([&] { On on(1); r.s[lv].Lock(&bb, &pb, flags); });
                eq("lock copy: stopped", oc, rc);
                eq("lock copy: where", ba == copy[0], bb == copy[1]);
                eq("lock copy: pitch", pa, pb);
                const bool ou = guarded([&] { On on(0); at<Fn0>(0x000FE320)(&o.s[lv], nullptr); });
                const bool ru = guarded([&] { On on(1); r.s[lv].Unlock(); });
                eq("unlock copy: stopped", ou, ru);
                eq("unlock copy: flags", o.s[lv].m_flags, r.s[lv].m_flags);
                compare_logs("lock copy");
            }
            o.s[lv].m_unknown48 = r.s[lv].m_unknown48 = nullptr;
            int na = 0, nb = 0;
            guarded([&] { On on(0); na = at<int(__fastcall*)(void*, void*)>(0x000FD800)(&o.s[lv], nullptr); });
            guarded([&] { On on(1); nb = r.s[lv].NeedsStaging(); });
            eq("needs staging", na, nb);
            compare_logs("needs staging");
        }
        // Made again: nothing, as it is loaded.
        guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, void*, unsigned, unsigned, void*)>(0x000FC8A0)(o.t, nullptr, g_own[0], 8, 8, &src[0].m_format); });
        guarded([&] { On on(1); r.t->Make(g_own[1], 8, 8, &src[1].m_format); });
        compare_logs("made again");
        destroy_item(o, r);
        same_fakes();
    }
}

// ---- .AGT files: each 2D texture loaded by its database, as a level loads it -------------------------------------------------------------------------------------------------------------------------------------

struct ImageFile { std::string archive, file; };
static std::map<std::string, std::vector<ImageFile>> g_images;  // by path on the disc
static std::string g_image_dir;
static std::map<std::string, std::vector<unsigned char>> g_image_cache;

static const std::vector<unsigned char>* image_bytes(const std::string& file) {
    auto c = g_image_cache.find(file);
    if (c != g_image_cache.end()) return &c->second;
    FILE* f = nullptr;
    if (fopen_s(&f, (g_image_dir + "\\" + file).c_str(), "rb") || !f) return nullptr;
    std::fseek(f, 0, SEEK_END);
    std::vector<unsigned char> d((size_t)std::ftell(f));
    std::fseek(f, 0, SEEK_SET);
    if (!d.empty()) std::fread(d.data(), 1, d.size(), f);
    std::fclose(f);
    return &(g_image_cache[file] = std::move(d));
}

static std::string upper(std::string s) { for (char& c : s) if (c >= 'a' && c <= 'z') c -= 32; return s; }

static AtGrD3DTextureDatabase* make_db(int side) {
    void* p = std::malloc(sizeof(AtGrD3DTextureDatabase));
    std::memset(p, 0xCD, sizeof(AtGrD3DTextureDatabase));
    if (side == 0) {
        at<Fn0>(0x0010FFC0)(p, nullptr);
        return (AtGrD3DTextureDatabase*)p;
    }
    return new (p) AtGrD3DTextureDatabase;
}

static unsigned g_agt_files, g_agt_textures, g_agt_loaded, g_agt_missing, g_agt_stopped;

static void run_agt(const std::vector<unsigned char>& data, const std::string& archive, const std::string& path) {
    g_context = "agt " + path;
    if (const char* only = getenv("TEXLOAD_ONLY")) {  // names separated by '|'
        bool any = false;
        std::string list = only;
        for (size_t b = 0, e; b <= list.size(); b = e + 1) {
            e = list.find('|', b);
            if (e == std::string::npos) e = list.size();
            if (e > b && path.find(list.substr(b, e - b)) != std::string::npos) any = true;
        }
        if (!any) return;
    }
    g_fs->files.clear();
    put(path, data);
    const std::string dir = path.substr(0, path.rfind('\\') + 1);
    // Each name after Texture2D, as the texture database keeps it: eight letters.
    const std::string text((const char*)data.data(), data.size());
    for (size_t at_ = text.find("Texture2D"); at_ != std::string::npos; at_ = text.find("Texture2D", at_ + 9)) {
        size_t q = at_ + 9;
        while (q < text.size() && (text[q] == ' ' || text[q] == '\t')) q++;
        if (q >= text.size() || text[q] != '"') continue;
        const size_t e = text.find('"', q + 1);
        if (e == std::string::npos) continue;
        std::string name = text.substr(q + 1, e - q - 1);
        if (name.size() > 8) name.resize(8);
        g_agt_textures++;
        const std::string want = upper(dir + name + ".DDS");
        auto f = g_images.find(want);
        if (f == g_images.end()) { g_agt_missing++; continue; }
        const ImageFile* pick = &f->second[0];
        for (const ImageFile& x : f->second) if (x.archive == archive) { pick = &x; break; }
        const std::vector<unsigned char>* bytes = image_bytes(pick->file);
        if (bytes) put(want, *bytes);
    }
    reset_settings();
    free_fakes();
    // Fresh owners for each file: a database that stopped part way stays named in its owner's tables, and is not destroyed (the old owners are left).
    make_owners();
    AtGrD3DTextureDatabase* o = make_db(0);
    AtGrD3DTextureDatabase* r = make_db(1);
    const std::string base = path.substr(0, path.rfind('.'));
    const bool oc = guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, void*, const char*, int)>(0x0001C7B0)(o, nullptr, g_own[0], base.c_str(), 0); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], base.c_str(), 0); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    eq("stopped", oc, rc);
    if (oc) g_agt_stopped++;
    if (getenv("TEXLOAD_ONLY")) {
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < g_log[side].n; i += 2) std::printf("%c%X ", (char)g_log[side].v[i], g_log[side].v[i + 1]);
            std::printf("\n%s\n", g_message[side]);
        }
    }
    compare_logs("load");
    eq("2D textures", o->m_texture2DCount, r->m_texture2DCount);
    eq("2D surfaces", o->m_surface2DCount, r->m_surface2DCount);
    if (o->m_textures2D && r->m_textures2D && o->m_texture2DCount == r->m_texture2DCount && o->m_surfaces2D && r->m_surfaces2D && o->m_surface2DCount == r->m_surface2DCount) {
        for (unsigned i = 0; i < o->m_texture2DCount; i++) {
            Item a{&o->m_textures2D[i], o->m_surfaces2D, o->m_surface2DCount};
            Item b{&r->m_textures2D[i], r->m_surfaces2D, r->m_surface2DCount};
            if (i == 0) for (unsigned k = 0; k < a.n; k++) same_surface(0, a.s[k], b.s[k], a, b);
            same_item(Item{a.t, a.t->m_surfaces, 0}, Item{b.t, b.t->m_surfaces, 0});
            eq("item's surfaces", a.t->m_surfaces - o->m_surfaces2D, b.t->m_surfaces - r->m_surfaces2D);
            if (r->m_textures2D[i].m_flags & AtTextureBase::kPending) g_agt_loaded++;
        }
    }
    same_map("map after the file");
    same_fakes();
    g_agt_files++;
    if (!oc) {
        { On on(0); at<void*(__fastcall*)(void*, void*, unsigned)>(0x00112240)(o, nullptr, 0); }
        { On on(1); r->~AtGrD3DTextureDatabase(); }
        compare_logs("destroy");
        same_fakes();
    }
    // A database that stopped part way is left as it is: it is on the owner's list, and destroying it is not what is compared here.
    g_own[0]->m_textureDatabases = nullptr;
    g_own[1]->m_textureDatabases = nullptr;
    std::free(o);
    std::free(r);
}

// ---- the reader's parsing, reading and copying, called directly -------------------------------------------------------------------------------------------------------------------------------------------------

static std::vector<unsigned char> raw_dds(unsigned w, unsigned h, unsigned flags, unsigned pitch, unsigned depth, unsigned levels, unsigned pf, const char* fourcc, unsigned bits, unsigned m0, unsigned m1, unsigned m2,
                                          unsigned m3, unsigned caps, unsigned caps2, unsigned data) {
    std::vector<unsigned char> d(128 + data);
    auto put32 = [&](size_t at_, unsigned v) { std::memcpy(&d[at_], &v, 4); };
    std::memcpy(&d[0], "DDS ", 4);
    put32(4, 0x7C); put32(8, flags); put32(12, h); put32(16, w); put32(20, pitch); put32(24, depth); put32(28, levels);
    put32(76, 32); put32(80, pf);
    if (fourcc) std::memcpy(&d[84], fourcc, 4);
    put32(88, bits); put32(92, m0); put32(96, m1); put32(100, m2); put32(104, m3); put32(108, caps); put32(112, caps2);
    d[127] = 0x5A;  // the last byte of the header, which reading the file whole copies
    for (size_t i = 128; i < d.size(); i++) d[i] = (unsigned char)(i * 5 + 3);
    return d;
}

typedef void(__fastcall* FnCopy)(void*, void*, void*, void*, unsigned, unsigned);
typedef void(__fastcall* FnDdsLevel)(void*, void*, void*, void*, void*, unsigned, unsigned, unsigned, void*, void*);

static AtGrD3DSurface2D* side_surface(int side) {
    void* p = std::malloc(sizeof(AtGrD3DSurface2D));
    std::memset(p, 0xCD, sizeof(AtGrD3DSurface2D));
    if (side == 0) at<Fn0>(0x000FCEE0)(p, nullptr); else new (p) AtGrD3DSurface2D;
    return (AtGrD3DSurface2D*)p;
}

static void run_reader_direct() {
    static std::vector<std::vector<unsigned char>> files;
    files.clear();
    files.reserve(16);
    struct F { const char* name; std::vector<unsigned char> d; };
    std::vector<F> fs;
    fs.push_back({"DV", raw_dds(16, 16, 0x881007, 128, 4, 0, 4, "DXT1", 0, 0, 0, 0, 0, 0x1000, 0x200000, 128)});
    fs.push_back({"DV2", raw_dds(16, 16, 0x81007, 128, 4, 0, 4, "DXT1", 0, 0, 0, 0, 0, 0x1000, 0x200000, 128)});
    fs.push_back({"DQ", raw_dds(16, 16, 0xA1007, 128, 0, 2, 4, "DXT1", 0, 0, 0, 0, 0, 0x401008, 0xFE00, 160)});
    fs.push_back({"DQ2", raw_dds(16, 16, 0xA1007, 128, 0, 2, 4, "DXT1", 0, 0, 0, 0, 0, 0x401008, 0x7E00, 160)});
    fs.push_back({"DP", raw_dds(16, 8, 0x100F, 32, 0, 0, 4, "DXT1", 0, 0, 0, 0, 0, 0x1000, 0, 256)});
    fs.push_back({"DN", raw_dds(12, 8, 0x1007, 0, 0, 0, 4, "DXT1", 0, 0, 0, 0, 0, 0x1000, 0, 64)});
    fs.push_back({"DR", raw_dds(4, 4, 0xA1007, 64, 0, 2, 0x41, nullptr, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0x401008, 0, 80)});
    fs.push_back({"DS", raw_dds(4, 4, 0x81007, 64, 0, 0, 0x40, nullptr, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0x1000, 0, 64)});
    fs.push_back({"DL", raw_dds(4, 4, 0x81007, 16, 0, 0, 0x20000, nullptr, 8, 0xFF, 0, 0, 0x0F, 0x1000, 0, 16)});
    fs.push_back({"DA", raw_dds(4, 4, 0x81007, 16, 0, 0, 0x2, nullptr, 8, 0, 0, 0, 0xFF, 0x1000, 0, 16)});
    g_fs->files.clear();
    for (F& f : fs) { files.push_back(f.d); put(std::string(f.name) + ".DDS", files.back()); }
    reset_settings();
    for (F& f : fs) {
        g_context = std::string("reader direct: ") + f.name;
        for (int side = 0; side < 2; side++) if (map_of(side)->m_format.m_flags == 0) map_of(side)->m_paletteCount = 5;
        int ra = 0, rb = 0;
        guarded([&] { On on(0); ra = at<int(__fastcall*)(void*, void*, const char*)>(0x00065C70)(map_of(0), nullptr, f.name); });
        guarded([&] { On on(1); rb = map_of(1)->TryLoad(f.name); });
        eq("try load", ra, rb);
        compare_logs("try load");
        same_map("map's name, read");
        // A level copied straight into a surface of the file's format and the level's size; another face; a render target; a surface of another format.
        if (!std::strcmp(f.name, "DR") || !std::strcmp(f.name, "DQ")) {
            const bool rgb = f.name[1] == 'R';
            const unsigned w = rgb ? 4 : 16, fmt = rgb ? 0x12 : 0x0C;
            struct V { unsigned level, face, usage; bool other; };
            const V vs[] = {{0, 0, 0x400, false}, {1, 0, 0x400, false}, {0, 1, 0x400, false}, {1, 1, 0x400, false}, {0, 0, 0x401, false}, {0, 0, 0x400, true}};
            for (const V& v : vs) {
                free_fakes();
                AtGrD3DSurface2D* s[2];
                void* level[2];
                for (int side = 0; side < 2; side++) {
                    g_side = side;
                    void* t = o_create(w, w, 1, 2, 0, v.other ? (rgb ? 0x0C : 0x12) : fmt, 3);
                    level[side] = o_level(t, v.level);
                    s[side] = side_surface(side);
                    s[side]->m_format.Copy(map_of(side)->m_format);
                    if (v.other) { if (rgb) s[side]->m_format.SetDxt1(0); else s[side]->m_format.SetRgba(32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0); }
                    s[side]->m_unknown20 = (unsigned short)(w >> v.level);
                    s[side]->m_unknown22 = (unsigned short)(w >> v.level);
                    s[side]->m_unknown1C = (unsigned short)v.usage;
                    s[side]->m_surface = level[side];
                }
                g_side = 0;
                const bool oc = guarded([&] { On on(0); at<FnCopy>(0x0011B9C0)(map_of(0), nullptr, g_own[0], s[0], v.level, v.face); });
                const bool rc = guarded([&] { On on(1); map_of(1)->CopyLevel(g_own[1], s[1], v.level, v.face); });
                eq("copy: stopped", oc, rc);
                compare_logs("copy");
                same_fakes();
                same_map("map's name, copied");
                // A level from the file onto a surface, as the texture does it, for a face past the first.
                if (!v.other && v.usage == 0x400) {
                    AtGrD3DSurface2D* u[2] = {side_surface(0), side_surface(1)};
                    const bool ol = guarded([&] { On on(0); at<FnDdsLevel>(0x000FDF40)(u[0], nullptr, g_own[0], level[0], map_of(0), 0x400, v.level, v.face, nullptr, nullptr); });
                    const bool rl = guarded([&] { On on(1); u[1]->LoadFromDds(g_own[1], level[1], map_of(1), 0x400, v.level, v.face, nullptr, nullptr); });
                    eq("level: stopped", ol, rl);
                    compare_logs("level");
                    same_fakes();
                    eq("level: format", std::memcmp(&u[0]->m_format, &u[1]->m_format, sizeof u[0]->m_format), 0);
                    eq("level: size", u[0]->m_unknown20 | u[0]->m_unknown22 << 16, u[1]->m_unknown20 | u[1]->m_unknown22 << 16);
                    std::free(u[0]); std::free(u[1]);
                }
                std::free(s[0]); std::free(s[1]);
            }
        }
        // The whole file read into a buffer of each side's, twice: the second time the file is at its end.
        for (int k = 0; k < 2; k++) {
            const unsigned n = map_of(0)->m_fileSize + 0x100;
            std::vector<unsigned char> ba(n, 0xCD), bb(n, 0xCD);
            guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, void*)>(0x0011B260)(map_of(0), nullptr, ba.data()); });
            guarded([&] { On on(1); map_of(1)->ReadFile(bb.data()); });
            eq("read file", std::memcmp(ba.data(), bb.data(), n), 0);
            compare_logs("read file");
        }
        // Parsed again, over itself: freed first, which empties the header it then reads.
        guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, const unsigned char*)>(0x0011B310)(map_of(0), nullptr, map_of(0)->m_header); });
        guarded([&] { On on(1); map_of(1)->Parse(map_of(1)->m_header); });
        compare_logs("parse again");
        same_map("map's name, parsed again");
        { On on(0); at<Fn0>(0x0011B910)(map_of(0), nullptr); }
        { On on(1); map_of(1)->Free(); }
        compare_logs("free");
    }
    free_fakes();
    // A reader destroyed in place, holding sizes and a format.
    {
        g_context = "reader destroyed in place";
        void* a = std::malloc(sizeof(AtGrDdsPixelMap));
        void* b = std::malloc(sizeof(AtGrDdsPixelMap));
        std::memset(a, 0xCD, sizeof(AtGrDdsPixelMap));
        std::memset(b, 0xCD, sizeof(AtGrDdsPixelMap));
        at<Fn0>(0x0011B2C0)(a, nullptr);
        new (b) AtGrDdsPixelMap;
        for (AtGrDdsPixelMap* m : {(AtGrDdsPixelMap*)a, (AtGrDdsPixelMap*)b}) {
            m->m_width = 3; m->m_height = 4; m->m_pitch = 5; m->m_paletteCount = 6; m->m_name[0] = 'N';
            m->m_format.SetDxt1(0);
        }
        { On on(0); at<Fn0>(0x000EEFA0)(a, nullptr); }
        { On on(1); ((AtGrDdsPixelMap*)b)->~AtGrDdsPixelMap(); }
        compare_logs("destroyed in place");
        eq("destroyed in place", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtGrDdsPixelMap) - 4), 0);
        std::free(a);
        std::free(b);
    }
}

// ---- the readers on their own ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static void run_direct() {
    g_context = "direct";
    reset_settings();
    // A reader made and destroyed through its tables; its slots that loading does not reach.
    for (int k = 0; k < 2; k++) {
        void* a = o_new(sizeof(AtGrDdsPixelMap));
        void* b = operator new(sizeof(AtGrDdsPixelMap));
        std::memset(a, 0xCD, sizeof(AtGrDdsPixelMap));
        std::memset(b, 0xCD, sizeof(AtGrDdsPixelMap));
        { On on(0); at<Fn0>(0x0011B2C0)(a, nullptr); }
        { On on(1); new (b) AtGrDdsPixelMap; }
        eq("reader's table", *(uint32_t*)a, 0x0028AFF0);
        eq("reader made", std::memcmp((char*)a + 4, (char*)b + 4, sizeof(AtGrDdsPixelMap) - 4), 0);
        unsigned sa, sb;
        ((AtGrDdsPixelMap*)a)->m_fileSize = ((AtGrDdsPixelMap*)b)->m_fileSize = 0x1234;
        { On on(0); sa = at<unsigned(__fastcall*)(void*, void*)>(0x0011B250)(a, nullptr); at<void(__fastcall*)(void*, void*, unsigned, unsigned, unsigned)>(0x001ACD70)(a, nullptr, 1, 2, 3); }
        { On on(1); sb = ((AtGrDdsPixelMap*)b)->FileSize(); ((AtGrDdsPixelMap*)b)->Slot5(1, 2, 3); }
        eq("file size", sa, sb);
        const char* ea;
        const char* eb;
        { On on(0); ea = at<const char*(__fastcall*)(void*, void*)>(0x0011B2B0)(a, nullptr); }
        { On on(1); eb = ((AtGrDdsPixelMap*)b)->Extension(); }
        eqs("extension", ea, eb);
        ((AtGrDdsPixelMap*)a)->m_buffer = (unsigned char*)o_new(16);
        ((AtGrDdsPixelMap*)b)->m_buffer = (unsigned char*)operator new(16);
        if (k == 0) {
            { On on(0); at<void*(__fastcall*)(void*, void*, unsigned)>(0x0011BD50)(a, nullptr, 1); }
            { On on(1); delete (AtGrDdsPixelMap*)b; }
        } else {
            // The base's deleting destructor on its own.
            { On on(0); at<void*(__fastcall*)(void*, void*, unsigned)>(0x000660E0)(a, nullptr, 1); }
            { On on(1); ((AtGrIPixelMap*)b)->AtGrIPixelMap::~AtGrIPixelMap(); operator delete(b); }
        }
        compare_logs(k ? "base destroyed" : "reader destroyed");
    }
    // The heap.
    at<void(__cdecl*)(void*)>(0x00065B00)(&g_map_heap_token);
    AtGrIPixelMap::SetHeap(&g_map_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002CB090, (uintptr_t)AtGrIPixelMap::s_heap);
    // A file that cannot be opened, and messages with no heap.
    g_fs->files.clear();
    {
        static std::vector<unsigned char> d = dds_file(8, 8, 0, "DXT1");
        put("Z.DDS", d);
        int ra = 0, rb = 0;
        guarded([&] { On on(0); ra = at<int(__fastcall*)(void*, void*, const char*)>(0x00065C70)(map_of(0), nullptr, "NOPE"); });
        guarded([&] { On on(1); rb = map_of(1)->TryLoad("NOPE"); });
        eq("try load: missing", ra, rb);
        guarded([&] { On on(0); ra = at<int(__fastcall*)(void*, void*, void*, const char*)>(0x00065D40)(map_of(0), nullptr, g_fs, "NOPE.DDS"); });
        guarded([&] { On on(1); rb = map_of(1)->Open(g_fs, "NOPE.DDS"); });
        eq("open: missing", ra, rb);
        guarded([&] { On on(0); ra = at<int(__fastcall*)(void*, void*, const char*)>(0x00065C70)(map_of(0), nullptr, "Z"); });
        guarded([&] { On on(1); rb = map_of(1)->TryLoad("Z"); });
        eq("try load: found", ra, rb);
        same_map("map after try load");
        compare_logs("try load");
        for (int fail = 0; fail < 2; fail++) {
            *(void**)(uintptr_t)0x002CB090 = fail ? nullptr : &g_map_heap_token;
            AtGrIPixelMap::s_heap = fail ? nullptr : &g_map_heap_token;
            g_fail_alloc = fail != 0;
            guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, const char*)>(0x00065B30)(map_of(0), nullptr, "message"); });
            g_fail_alloc = fail != 0;
            guarded([&] { On on(1); map_of(1)->Fail("message"); });
            g_fail_alloc = fail != 0;
            guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, int)>(0x00065C20)(map_of(0), nullptr, 2); });
            g_fail_alloc = fail != 0;
            guarded([&] { On on(1); map_of(1)->FailIo(2); });
            g_fail_alloc = fail != 0;
            guarded([&] { On on(0); at<void(__fastcall*)(void*, void*, const char*)>(0x00065F80)(map_of(0), nullptr, "GONE"); });
            g_fail_alloc = fail != 0;
            guarded([&] { On on(1); map_of(1)->Load("GONE"); });
            g_fail_alloc = false;
            compare_logs(fail ? "messages, no heap" : "messages");
        }
        *(void**)(uintptr_t)0x002CB090 = &g_map_heap_token;
        AtGrIPixelMap::s_heap = &g_map_heap_token;
        { On on(0); at<Fn0>(0x0011B910)(map_of(0), nullptr); }
        { On on(1); map_of(1)->Free(); }
        compare_logs("free");
        same_map("map freed");
    }
    run_reader_direct();
    // The format tables, every number; making a format a number, each way.
    for (unsigned f = 0; f < 0x48; f++) {
        for (int sw = 0; sw < 2; sw++) {
            AtPixelFormat a, b;
            std::memset(&a, 0xCD, sizeof a);
            std::memset(&b, 0xCD, sizeof b);
            int ra, rb;
            { On on(0); ra = at<int(__cdecl*)(unsigned, AtPixelFormat*, int)>(0x001197D0)(f, &a, sw); }
            { On on(1); rb = AtGrD3DFormatFromDevice(f, &b, sw); }
            eq("from device", ra, rb);
            eq("from device: format", std::memcmp(&a, &b, sizeof a), 0);
            unsigned ta, tb;
            { On on(0); ta = at<unsigned(__cdecl*)(const AtPixelFormat*)>(0x00119B90)(&a); }
            { On on(1); tb = AtGrD3DFormatToDevice(&b); }
            eq("to device", ta, tb);
            AtPixelFormat a2 = a, b2 = b;
            a2.m_mask2 ^= 1; b2.m_mask2 ^= 1;  // a third mask that differs
            { On on(0); ta = at<unsigned(__cdecl*)(const AtPixelFormat*)>(0x00119B90)(&a2); }
            { On on(1); tb = AtGrD3DFormatToDevice(&b2); }
            eq("to device, third mask", ta, tb);
            a.m_mask0 ^= 1; b.m_mask0 ^= 1;  // matched by bits alone, or not at all
            { On on(0); ta = at<unsigned(__cdecl*)(const AtPixelFormat*)>(0x00119B90)(&a); }
            { On on(1); tb = AtGrD3DFormatToDevice(&b); }
            eq("to device, by bits", ta, tb);
            a.m_bits = b.m_bits = 24;
            { On on(0); ta = at<unsigned(__cdecl*)(const AtPixelFormat*)>(0x00119B90)(&a); }
            { On on(1); tb = AtGrD3DFormatToDevice(&b); }
            eq("to device, no bits", ta, tb);
        }
        int la, lb;
        { On on(0); la = at<int(__cdecl*)(unsigned)>(0x00119A60)(f); }
        { On on(1); lb = AtGrD3DIsNotLinear(f); }
        eq("not linear", la, lb);
    }
    compare_logs("formats");
}

// ---- running -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (getenv("TEXLOAD_TRACE")) std::fprintf(stderr, "caught 0x%08X at 0x%p reading 0x%p, side %d, [%s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress,
                                                  (void*)e->ExceptionRecord->ExceptionInformation[1], g_side, g_context.c_str());
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool read_lines(const std::string& path, std::vector<std::vector<std::string>>& rows) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "r") || !f) { std::printf("cannot read %s\n", path.c_str()); return false; }
    char line[2048];
    while (std::fgets(line, sizeof line, f)) {
        std::vector<std::string> cols;
        std::string cur;
        for (char* p = line; *p && *p != '\n' && *p != '\r'; p++) {
            if (*p == '\t') { cols.push_back(cur); cur.clear(); } else cur += *p;
        }
        cols.push_back(cur);
        if (cols.size() >= 3) rows.push_back(cols);
    }
    std::fclose(f);
    return true;
}

static bool read_file(const std::string& path, std::vector<unsigned char>& d) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    d.resize((size_t)std::ftell(f));
    std::fseek(f, 0, SEEK_SET);
    if (!d.empty()) std::fread(d.data(), 1, d.size(), f);
    std::fclose(f);
    return true;
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
    if (pos.size() != 2) { std::printf("usage: oracle_texload <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00208430, (const void*)o_back_buffer);
    redirect(0x0020B160, (const void*)o_create);
    redirect(0x0020B220, (const void*)o_level);
    redirect(0x0020AAB0, (const void*)o_describe);
    redirect(0x0020AAD0, (const void*)o_lock);
    redirect(0x00205D10, (const void*)o_release);
    redirect(0x00205E20, (const void*)o_before_release);
    redirect(0x000FB630, (const void*)o_release_stage);
    redirect(0x0010B7F0, (const void*)&o_stop<0x0010B7F0>);
    // What is not recovered stops on the original's side too, at the same function.
    redirect(0x000FCE80, (const void*)&o_stop<0x000FCE80>);
    redirect(0x000FC790, (const void*)&o_stop<0x000FC790>);
    redirect(0x000FDB70, (const void*)&o_stop<0x000FDB70>);
    redirect(0x000FD870, (const void*)&o_stop<0x000FD870>);
    redirect(0x000FD5A0, (const void*)&o_stop<0x000FD5A0>);
    redirect(0x000FDC90, (const void*)&o_stop<0x000FDC90>);
    redirect(0x000FD330, (const void*)&o_stop<0x000FD330>);
    redirect(0x000FD380, (const void*)&o_stop<0x000FD380>);
    redirect(0x000F5D40, (const void*)&o_stop<0x000F5D40>);
    redirect(0x0005F280, (const void*)&o_stop<0x0005F280>);
    redirect(0x00125F25, (const void*)pure_call);
    at<void(__cdecl*)()>(0x00125BC7)();

    g_fs = new TreeFs;
    g_fs->m_path.m_top = -1;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C62E4u, 0x002C63F0u}) *(void**)(uintptr_t)va = &g_heap_token;
    *(void**)(uintptr_t)0x002CB090 = &g_map_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtTextureDatabase::s_heap = &g_heap_token;
    AtGrIPixelMap::s_heap = &g_map_heap_token;
    g_heap_pad = 0x100;
    // The original's reader, as its static constructor makes it.
    at<Fn0>(0x0011B2C0)((void*)(uintptr_t)0x003B13E8, nullptr);
    make_owners();
    free_fakes();
    if (entries && !arm_entries(entries)) return 2;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    const bool only = getenv("TEXLOAD_ONLY") != nullptr;  // for looking at one .AGT: nothing else runs
    if (!only) {
        run_direct();
        run_written();
        run_prog();
    }

    g_image_dir = std::string(pos[1]) + "\\..\\scratch\\texload\\img";
    std::vector<std::vector<std::string>> images;
    if (!read_lines(g_image_dir + "\\index.txt", images)) return 2;
    // Every path of each image, for a texture's name beside its .AGT: one content may lie in several places.
    std::vector<std::vector<std::string>> every;
    if (!read_lines(g_image_dir + "\\index_all.txt", every)) return 2;
    for (const auto& row : every) g_images[upper(row[2])].push_back(ImageFile{row[1], row[0]});
    for (const auto& row : images) {
        if (only || getenv("TEXLOAD_SKIP_DDS") || upper(row[2]).find(".DDS") == std::string::npos) continue;
        std::vector<unsigned char> d;
        if (!read_file(g_image_dir + "\\" + row[0], d)) { std::printf("cannot read %s\n", row[0].c_str()); return 2; }
        run_dds(d, row[0]);
    }
    std::vector<std::vector<std::string>> texts;
    if (!read_lines(std::string(pos[1]) + "\\index.txt", texts)) return 2;
    for (const auto& row : texts) {
        if (upper(row[2]).find(".AGT") == std::string::npos) continue;
        std::vector<unsigned char> d;
        if (!read_file(std::string(pos[1]) + "\\" + row[0], d)) { std::printf("cannot read %s\n", row[0].c_str()); return 2; }
        run_agt(d, row[1], upper(row[2]));
    }
    g_context = "end";
    free_fakes();
    report_entries();
    std::printf("%s: %u .DDS, %u .AGT (%u 2D textures named, %u loaded, %u with no .DDS beside, %u files stopped); %u loads (%u stopped), %u levels and %llu bytes compared; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_dds_files, g_agt_files, g_agt_textures, g_agt_loaded, g_agt_missing, g_agt_stopped, g_loads, g_stops, g_levels_compared, g_bytes_compared,
                g_checks, g_failures);
    return g_failures ? 1 : 0;
}
