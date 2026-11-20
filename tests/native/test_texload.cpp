// Host test of what a 2D texture does when it loads, on any pointer size: written .DDS files on a file tree, read by the real DDS reader, made and copied through the device's calls, which are counted here.
// Not evidence of a byte match. Supplies the functions of src/at/AtGrD3DTextureSeam.cpp and src/at/AtGrD3DSeam.cpp.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "at/AtGrD3DDatabase.h"
#include "at/AtGrD3DTextureLoad.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// ---- the device ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Level { unsigned width, height, format; std::vector<unsigned char> bits; int pitch; int locks; };
struct Texture { unsigned width, height, levels, usage, format; Level level[16]; };
static std::vector<Texture*> g_made;
static int g_back, g_releases, g_unrecovered;
static int g_pad;

void* AtGrD3DGetBackBuffer(int which) { g_back = which == -1 ? 1 : 2; static int buffer; return &buffer; }
void* AtGrD3DCreateTexture(unsigned int w, unsigned int h, unsigned int depth, unsigned int levels, unsigned int usage, unsigned int format, unsigned int type) {
    CHECK(depth == 1 && type == 3);
    Texture* t = new Texture();
    t->width = w; t->height = h; t->levels = levels; t->usage = usage; t->format = format;
    for (unsigned i = 0; i < levels && i < 16; i++) {
        Level& l = t->level[i];
        l.width = w >> i ? w >> i : 1;
        l.height = h >> i ? h >> i : 1;
        l.format = format;
    }
    g_made.push_back(t);
    return t;
}
void* AtGrD3DSurfaceLevel(void* texture, unsigned int level) { return &((Texture*)texture)->level[level]; }
void AtGrD3DDescribe(void* surface, AtGrD3DSurfaceDesc* desc) {
    const Level* l = (const Level*)surface;
    std::memset(desc, 0, sizeof *desc);
    desc->m_format = l->format;
    desc->m_multiSample = 0x11;
    desc->m_width = l->width;
    desc->m_height = l->height;
}
void AtGrD3DLockRect(void* surface, AtGrD3DLockedRect* locked, const void*, unsigned int) {
    Level* l = (Level*)surface;
    const unsigned block = l->format == 0x0C ? 8 : 16;
    l->pitch = (int)((l->width / 4 ? l->width / 4 : 1) * block) + g_pad;
    l->bits.assign((size_t)l->pitch * (l->height / 4 ? l->height / 4 : 1), 0xEE);
    l->locks++;
    locked->m_pitch = l->pitch;
    locked->m_bits = l->bits.data();
}
void AtGrD3DRelease(void*) { g_releases++; }
void AtGrD3DBeforeRelease(void*) {}
void AtGrD3DReleaseStage(void*, unsigned int) {}
void AtGrD3DLoadMaterial(AtGrD3DMaterial*, AtMaterialOwner*) {}
unsigned int AtGrD3DUnrecovered(unsigned int, void*, unsigned int, unsigned int) { g_unrecovered++; return 0; }
void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int) { return std::malloc(size); }
void AtGrD3DFreeContiguous(void* data) { std::free(data); }
void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char**, char**, char**) {}
void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char**, char**) {}
void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char**, char**) {}
void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char**, char**, char**, char*) {}

namespace {

struct Chooser : public AtGrD3DFormatChooser {
    int calls = 0;
    void Slot0() override {} void Slot1() override {} void Clear() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot10() override {} void Slot11() override {}
    void ChooseRenderTarget(const AtPixelFormat*, AtPixelFormat*) override { calls++; }
    void ChooseTexture(const AtPixelFormat*, AtPixelFormat*) override { calls++; }
};
struct Limits : public AtGrD3DTextureLimits {
    unsigned least = 1;
    void Slot0() override {}
    unsigned int MinWidth(unsigned int) override { return least; }
    unsigned int MaxWidth(unsigned int) override { return 4096; }
    unsigned int MinHeight(unsigned int) override { return least; }
    unsigned int MaxHeight(unsigned int) override { return 4096; }
};
struct Owner : public AtGrD3DOwner {
    Chooser m_chooserObject;
    AtGfxDrawLists* DrawLists() override { return &m_chooserObject; }
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
};

class File : public AtIoFile {
public:
    const std::vector<unsigned char>* m_data = nullptr;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        const size_t have = m_data->size() - m_position;
        if (n > have) n = (unsigned)have;
        if (n) std::memcpy(out, m_data->data() + m_position, n);
        m_position += n;
        if (done) *done = n;
        return n ? 0 : (int)kEndOfFile;
    }
    int Write(const void*, unsigned int, unsigned int*) override { return kUnsupported; }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class Tree : public AtIoFileSystem {
public:
    std::map<std::string, std::vector<unsigned char>> m_files;
    File m_file;
    const std::vector<unsigned char>* Find(const char* path) {
        m_path.Resolve(path);
        std::string key = m_path.m_full;
        for (char& c : key) if (c >= 'a' && c <= 'z') c -= 32;
        auto f = m_files.find(key);
        return f == m_files.end() ? nullptr : &f->second;
    }
    int Open(const char* path, int, AtIoFile** out) override {
        const std::vector<unsigned char>* d = Find(path);
        if (!d) return AtIoFile::kNotFound;
        m_file.m_data = d;
        m_file.m_position = 0;
        m_file.m_size = (unsigned)d->size();
        *out = &m_file;
        return 0;
    }
    int Close(AtIoFile*) override { return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char* path) override { return Find(path) ? 0 : 1; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

// A DXT file of this size and these levels; its bytes after the header count up.
std::vector<unsigned char> Dds(unsigned w, unsigned h, unsigned levels, bool dxt1) {
    const unsigned block = dxt1 ? 8 : 16;
    const unsigned first = (w / 4 ? w / 4 : 1) * (h / 4 ? h / 4 : 1) * block;
    unsigned size = 0;
    for (unsigned i = 0; i < (levels ? levels : 1); i++) size += (first >> (2 * i)) > block ? first >> (2 * i) : block;
    std::vector<unsigned char> d(128 + size);
    auto put = [&](size_t at, unsigned v) { for (int k = 0; k < 4; k++) d[at + k] = (unsigned char)(v >> (8 * k)); };
    std::memcpy(&d[0], "DDS ", 4);
    put(4, 0x7C);
    put(8, 0x81007 | (levels ? 0x20000 : 0));
    put(12, h);
    put(16, w);
    put(20, first);
    put(28, levels);
    put(76, 32);
    put(80, 4);
    std::memcpy(&d[84], dxt1 ? "DXT1" : "DXT3", 4);
    put(108, 0x1000);
    for (size_t i = 128; i < d.size(); i++) d[i] = (unsigned char)(i * 13);
    return d;
}

AtGrD3DTexture2D* Make(const char* name, unsigned usage, unsigned short flags, unsigned levels, std::vector<AtGrD3DSurface2D>& surfaces) {
    surfaces.resize(levels);
    AtGrD3DTexture2D* t = new AtGrD3DTexture2D;
    std::memset(t->m_name, 0, 8);
    std::memcpy(t->m_name, name, std::strlen(name) < 8 ? std::strlen(name) : 8);
    t->m_usage = usage;
    t->m_flags = flags;
    t->m_levelsAsked = (unsigned char)levels;
    t->m_levels = (unsigned char)levels;
    t->m_surfaces = surfaces.data();
    return t;
}

}  // namespace

int main() {
    Tree tree;
    tree.m_path.m_top = -1;
    AtIoContext context;
    AtIoFileSystem* stack[4] = {&tree};
    context.m_stackCount = 1;
    context.m_stackSize = 4;
    context.m_stack = stack;
    AtIoFileRef::s_context = &context;
    static int token;
    AtGrIPixelMap::SetHeap(&token);

    Owner* owner = new Owner;
    Limits limits;
    owner->m_textureLimits = &limits;
    limits.m_caps = 0x100000;  // other than square allowed
    owner->m_noTextures = 0;

    // Three levels of DXT1 asked, the file has two: two made, each level's bytes where the file has them.
    const std::vector<unsigned char> a = Dds(16, 8, 2, true);
    tree.m_files["A.DDS"] = a;
    {
        std::vector<AtGrD3DSurface2D> s;
        AtGrD3DTexture2D* t = Make("A", AtTextureBase::kSourceDds, AtTextureBase::kPlatformSource, 3, s);
        t->OnLoaded(owner);
        CHECK(g_made.size() == 1);
        const Texture* m = g_made.back();
        CHECK(m->width == 16 && m->height == 8 && m->levels == 2 && m->format == 0x0C && m->usage == 0);
        CHECK(t->m_levels == 2);
        CHECK(t->m_texture == m);
        CHECK((t->m_flags & AtTextureBase::kPending) != 0);
        CHECK((t->m_flags & 0x20) == 0);  // no palette
        CHECK(m->level[0].locks == 1 && m->level[1].locks == 1);
        CHECK(m->level[0].bits.size() == 64 && std::memcmp(m->level[0].bits.data(), a.data() + 128, 64) == 0);
        CHECK(m->level[1].bits.size() == 16 && std::memcmp(m->level[1].bits.data(), a.data() + 192, 16) == 0);
        CHECK(s[0].m_unknown20 == 16 && s[0].m_unknown22 == 8 && s[1].m_unknown20 == 8 && s[1].m_unknown22 == 4);
        CHECK(s[0].m_unknown40 == &s[1] && s[1].m_unknown40 == nullptr && s[0].m_unknown3C == t && s[0].m_unknown24 == owner);
        CHECK(s[0].m_format.m_flags == AtPixelFormat::kDxt1 && s[0].m_flags == AtGrD3DSurface2D::kLoaded);
        CHECK(AtGrDdsPixelMap::s_map.m_format.m_flags == 0 && AtGrDdsPixelMap::s_map.m_buffer == nullptr);  // freed after the levels
        CHECK(owner->m_chooserObject.calls == 1);
        t->OnLoaded(owner);  // loaded already: nothing
        CHECK(g_made.size() == 1);
        delete t;
    }
    // DXT3 with longer rows on the device: copied row by row.
    const std::vector<unsigned char> b = Dds(32, 32, 0, false);
    tree.m_files["B.DDS"] = b;
    g_pad = 16;
    {
        std::vector<AtGrD3DSurface2D> s;
        AtGrD3DTexture2D* t = Make("B", AtTextureBase::kSourceDds, 0, 9, s);
        t->OnLoaded(owner);
        const Texture* m = g_made.back();
        CHECK(m->levels == 1 && m->format == 0x0E);
        const Level& l = m->level[0];
        CHECK(l.pitch == 8 * 16 + 16);
        bool rows = true;
        for (int r = 0; r < 8; r++) rows = rows && std::memcmp(l.bits.data() + r * l.pitch, b.data() + 128 + r * 128, 128) == 0 && l.bits[r * l.pitch + 128] == 0xEE;
        CHECK(rows);
        delete t;
    }
    g_pad = 0;
    // A device whose least side is 8: no levels below it.
    limits.least = 8;
    tree.m_files["C.DDS"] = Dds(64, 64, 7, true);
    {
        std::vector<AtGrD3DSurface2D> s;
        AtGrD3DTexture2D* t = Make("C", AtTextureBase::kSourceDds, 0, 9, s);
        t->OnLoaded(owner);
        CHECK(g_made.back()->levels == 4);
        CHECK(t->m_levels == 4);
        delete t;
    }
    limits.least = 1;
    // A front buffer source: the device's, one level, its reference dropped.
    {
        std::vector<AtGrD3DSurface2D> s;
        AtGrD3DTexture2D* t = Make("A", AtTextureBase::kSourceDds, AtTextureBase::kFrontBufferSource, 3, s);
        const int releases = g_releases;
        t->OnLoaded(owner);
        CHECK(g_back == 1 && t->m_levels == 1 && t->m_levelsAsked == 1 && g_releases == releases + 1);
        CHECK((t->m_flags & AtTextureBase::kPending) == 0);
        t->m_texture = nullptr;
        delete t;
    }
    // A ProgSource: made to its size, 8-8-8-8 swizzled, every level prepared.
    {
        std::vector<AtGrD3DSurface2D> s;
        AtGrD3DTexture2D* t = Make("P", AtTextureBase::kSourceProg, 0, 3, s);
        AtProgTextureSource* src = (AtProgTextureSource*)operator new(sizeof(AtProgTextureSource));
        src->m_width = 128;
        src->m_height = 64;
        src->m_format.SetRgba(32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0);
        t->m_progSource = src;
        t->OnLoaded(owner);
        const Texture* m = g_made.back();
        CHECK(m->width == 128 && m->height == 64 && m->levels == 3 && m->format == 0x06);
        CHECK(s[2].m_unknown20 == 32 && s[2].m_format.m_flags == (AtPixelFormat::kRgba) && s[0].m_unknown40 == &s[1]);
        CHECK((t->m_flags & AtTextureBase::kPending) != 0);
        // Locking the smallest level: swizzled on the device, so a staging copy, which is not recovered.
        void* bits;
        int pitch;
        const int before = g_unrecovered;
        s[0].m_unknown48 = std::malloc(64);
        s[0].Lock(&bits, &pitch, 2);
        CHECK(bits == s[0].m_unknown48 && pitch == 128 * 4 && (s[0].m_flags & (AtGrD3DSurface2D::kLocked | AtGrD3DSurface2D::kLockedToWrite)) != 0);
        s[0].m_surface = nullptr;
        s[0].Unlock();
        CHECK(s[0].m_flags == AtGrD3DSurface2D::kLoaded && g_unrecovered == before);
        std::free(s[0].m_unknown48);
        s[0].m_unknown48 = nullptr;
        delete t;
    }
    // The format tables both ways.
    {
        AtPixelFormat f;
        CHECK(AtGrD3DFormatFromDevice(0x12, &f, 0) == 1 && f.m_bits == 32 && f.m_flags == AtPixelFormat::kRgba);
        CHECK(AtGrD3DFormatToDevice(&f) == 0x12);
        CHECK(AtGrD3DFormatFromDevice(0x06, &f, 1) == 1 && (f.m_flags & AtPixelFormat::kSwizz) != 0 && AtGrD3DFormatToDevice(&f) == 0x06);
        CHECK(AtGrD3DFormatFromDevice(0x45, &f, 1) == 0 && f.m_flags == 0);
        CHECK(AtGrD3DIsNotLinear(0x0C) == 0 && AtGrD3DIsNotLinear(0x06) == 1);
    }
    for (Texture* t : g_made) delete t;
    if (g_fail) { std::printf("%d failures\n", g_fail); return 1; }
    std::printf("test_texload: all passed\n");
    return 0;
}
