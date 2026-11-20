// For host tests that load 2D textures through the real slot 12 (src/at/AtGrD3DTextureLoad.cpp): the device's calls of src/at/AtGrD3DTextureSeam.cpp that loading makes, counted; an owner's format chooser and
// texture limits; and a written .DDS file. The including test supplies the seam's other functions. Not recovered code.
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>

#include "at/AtGrD3DTextureLoad.h"

static int g_textures_made;  // textures the device made, and front or back buffers taken

namespace texload_device {
struct Level { unsigned width, height, format; unsigned char* bits; };
struct Texture { Level level[16]; };
}  // namespace texload_device

void* AtGrD3DGetBackBuffer(int) { g_textures_made++; static int buffer; return &buffer; }
void* AtGrD3DCreateTexture(unsigned int w, unsigned int h, unsigned int, unsigned int levels, unsigned int, unsigned int format, unsigned int) {
    g_textures_made++;
    texload_device::Texture* t = new texload_device::Texture();
    for (unsigned i = 0; i < levels && i < 16; i++) {
        t->level[i].width = w >> i ? w >> i : 1;
        t->level[i].height = h >> i ? h >> i : 1;
        t->level[i].format = format;
    }
    return t;  // not freed: a test's handful
}
void* AtGrD3DSurfaceLevel(void* texture, unsigned int level) { return &((texload_device::Texture*)texture)->level[level]; }
void AtGrD3DDescribe(void* surface, AtGrD3DSurfaceDesc* desc) {
    const texload_device::Level* l = (const texload_device::Level*)surface;
    std::memset(desc, 0, sizeof *desc);
    desc->m_format = l->format;
    desc->m_multiSample = 0x11;
    desc->m_width = l->width;
    desc->m_height = l->height;
}
void AtGrD3DLockRect(void* surface, AtGrD3DLockedRect* locked, const void*, unsigned int) {
    texload_device::Level* l = (texload_device::Level*)surface;
    const unsigned block = l->format == 0x0C ? 8 : 16;
    const unsigned pitch = (l->width / 4 ? l->width / 4 : 1) * block;
    if (!l->bits) l->bits = (unsigned char*)std::calloc(pitch, (l->height / 4 ? l->height / 4 : 1));
    locked->m_pitch = (int)pitch;
    locked->m_bits = l->bits;
}

struct TexloadChooser : public AtGrD3DFormatChooser {
    void Slot0() override {} void Slot1() override {} void Clear() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot10() override {} void Slot11() override {}
    void ChooseRenderTarget(const AtPixelFormat*, AtPixelFormat*) override {}
    void ChooseTexture(const AtPixelFormat*, AtPixelFormat*) override {}
};
struct TexloadLimits : public AtGrD3DTextureLimits {
    TexloadLimits() { m_caps = 0x100000; m_capsMask = 0; }
    void Slot0() override {}
    unsigned int MinWidth(unsigned int) override { return 1; }
    unsigned int MaxWidth(unsigned int) override { return 4096; }
    unsigned int MinHeight(unsigned int) override { return 1; }
    unsigned int MaxHeight(unsigned int) override { return 4096; }
};

// A DXT1 .DDS file of this size and number of levels (0 for one, with no count).
inline std::string TexloadDds(unsigned w, unsigned h, unsigned levels) {
    const unsigned first = (w / 4 ? w / 4 : 1) * (h / 4 ? h / 4 : 1) * 8;
    unsigned size = 0;
    for (unsigned i = 0; i < (levels ? levels : 1); i++) size += (first >> (2 * i)) > 8 ? first >> (2 * i) : 8;
    std::string d(128 + size, '\0');
    auto put = [&](size_t at, unsigned v) { for (int k = 0; k < 4; k++) d[at + k] = (char)(v >> (8 * k)); };
    std::memcpy(&d[0], "DDS ", 4);
    put(4, 0x7C);
    put(8, 0x81007 | (levels ? 0x20000 : 0));
    put(12, h);
    put(16, w);
    put(20, first);
    put(28, levels);
    put(76, 32);
    put(80, 4);
    std::memcpy(&d[84], "DXT1", 4);
    put(108, 0x1000);
    return d;
}
