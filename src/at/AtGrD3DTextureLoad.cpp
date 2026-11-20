// What a 2D texture does when it loads, short of the device: see include/at/AtGrD3DTextureLoad.h. The device's calls are the seam, src/at/AtGrD3DTextureSeam.cpp. Names inferred.
#include "at/AtGrD3DDatabase.h"
#include "at/AtGrD3DTextureLoad.h"

void AtCopyName9(char* to, const char* from);  // 0x00013F90

#define STOP(va) AtGrD3DUnrecovered(va, this, 0, 0)

// ---- The device's formats ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const AtGrD3DFormatEntry g_atGrSwizzledFormats[28] = {
    {0x06, 0x0801, 32, {0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000}},
    {0x07, 0x0801, 32, {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000}},
    {0x05, 0x0801, 16, {0x0000F800, 0x000007E0, 0x0000001F, 0x00000000}},
    {0x27, 0x0801, 16, {0x0000FC00, 0x000003E0, 0x0000001F, 0x00000000}},
    {0x03, 0x0801, 16, {0x00007C00, 0x000003E0, 0x0000001F, 0x00000000}},
    {0x02, 0x0801, 16, {0x00007C00, 0x000003E0, 0x0000001F, 0x00008000}},
    {0x04, 0x0801, 16, {0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000}},
    {0x19, 0x0802, 8, {0x00000000, 0x00000000, 0x00000000, 0x000000FF}},
    {0x3A, 0x0801, 32, {0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000}},
    {0x3B, 0x0801, 32, {0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF}},
    {0x39, 0x0801, 16, {0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F}},
    {0x38, 0x0801, 16, {0x0000F800, 0x000007C0, 0x0000003E, 0x00000001}},
    {0x3C, 0x0801, 32, {0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF}},
    {0x29, 0x0801, 16, {0x0000FF00, 0x000000FF, 0x00000000, 0x00000000}},
    {0x28, 0x0801, 16, {0x00000000, 0x000000FF, 0x0000FF00, 0x00000000}},
    {0x0B, 0x0804, 8, {0x000000FF, 0x00000000, 0x00000000, 0x00000000}},
    {0x00, 0x0802, 8, {0x000000FF, 0x00000000, 0x00000000, 0x00000000}},
    {0x1A, 0x0802, 16, {0x000000FF, 0x00000000, 0x00000000, 0x0000FF00}},
    {0x01, 0x0802, 8, {0x00000000, 0x00000000, 0x00000000, 0x000000FF}},
    {0x32, 0x0802, 16, {0x0000FFFF, 0x00000000, 0x00000000, 0x00000000}},
    {0x28, 0x0808, 16, {0x00000000, 0x000000FF, 0x0000FF00, 0x00000000}},
    {0x27, 0x0808, 16, {0x0000FC00, 0x0000001F, 0x000003E0, 0x00000000}},
    {0x07, 0x0808, 32, {0x00FF0000, 0x000000FF, 0x0000FF00, 0x00000000}},
    {0x3A, 0x0808, 32, {0x00FF0000, 0x000000FF, 0x0000FF00, 0xFF000000}},
    {0x33, 0x0808, 32, {0x00000000, 0x0000FFFF, 0xFFFF0000, 0x00000000}},
    {0x2A, 0x0810, 32, {0xFFFFFF00, 0x000000FF, 0x00000000, 0x00000000}},
    {0x2C, 0x0810, 16, {0x0000FFFF, 0x00000000, 0x00000000, 0x00000000}},
    {0x2C, 0x0810, 16, {0x0000FFFF, 0x00000000, 0x00000000, 0x00000000}},
};

const AtGrD3DFormatEntry g_atGrLinearFormats[26] = {
    {0x0C, 0x0040, 4, {0, 0, 0, 0}},  // DXT1
    {0x0E, 0x0100, 8, {0, 0, 0, 0}},  // DXT3
    {0x0E, 0x0080, 8, {0, 0, 0, 0}},  // DXT2
    {0x0F, 0x0200, 8, {0, 0, 0, 0}},  // DXT4
    {0x0F, 0x0400, 8, {0, 0, 0, 0}},  // DXT5
    {0x10, 0x0001, 16, {0x00007C00, 0x000003E0, 0x0000001F, 0x00008000}},
    {0x1D, 0x0001, 16, {0x00000F00, 0x000000F0, 0x0000000F, 0x0000F000}},
    {0x1F, 0x0002, 8, {0x00000000, 0x00000000, 0x00000000, 0x000000FF}},
    {0x3F, 0x0001, 32, {0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000}},
    {0x12, 0x0001, 32, {0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000}},
    {0x40, 0x0001, 32, {0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF}},
    {0x17, 0x0001, 16, {0x00000000, 0x000000FF, 0x0000FF00, 0x00000000}},
    {0x3E, 0x0001, 16, {0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F}},
    {0x3D, 0x0001, 16, {0x0000F800, 0x000007C0, 0x0000003E, 0x00000001}},
    {0x11, 0x0001, 16, {0x0000F800, 0x000007E0, 0x0000001F, 0x00000000}},
    {0x37, 0x0001, 16, {0x0000FC00, 0x000003E0, 0x0000001F, 0x00000000}},
    {0x16, 0x0001, 16, {0x0000FF00, 0x000000FF, 0x00000000, 0x00000000}},
    {0x41, 0x0001, 32, {0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF}},
    {0x1C, 0x0001, 16, {0x00007C00, 0x000003E0, 0x0000001F, 0x00000000}},
    {0x1E, 0x0001, 32, {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000}},
    {0x20, 0x0002, 16, {0x000000FF, 0x00000000, 0x00000000, 0x0000FF00}},
    {0x1B, 0x0001, 8, {0x00000000, 0x00000000, 0x00000000, 0x000000FF}},
    {0x35, 0x0002, 16, {0x0000FFFF, 0x00000000, 0x00000000, 0x00000000}},
    {0x13, 0x0004, 8, {0x000000FF, 0x00000000, 0x00000000, 0x00000000}},
    {0x2E, 0x0010, 32, {0xFFFFFF00, 0x000000FF, 0x00000000, 0x00000000}},
    {0x30, 0x0010, 16, {0x0000FFFF, 0x00000000, 0x00000000, 0x00000000}},
};

namespace {

const unsigned int kSwizzledCount = sizeof g_atGrSwizzledFormats / sizeof g_atGrSwizzledFormats[0];
const unsigned int kLinearCount = sizeof g_atGrLinearFormats / sizeof g_atGrLinearFormats[0];
const unsigned short kCompressed = AtPixelFormat::kDxt1 | AtPixelFormat::kDxt2 | AtPixelFormat::kDxt3 | AtPixelFormat::kDxt4 | AtPixelFormat::kDxt5;  // 0x7C0
const unsigned short kHasPalette = 0x20;  // in a texture's m_flags once loaded: its first level has a palette

// The device's numbers for the compressed formats, which it writes in blocks.
bool IsCompressedOnDevice(unsigned int format) {
    return format == 0x0C || format == 0x0E || format == 0x0F;
}

bool Matches(const AtGrD3DFormatEntry& entry, const AtPixelFormat* format, unsigned int flags) {
    return entry.m_flags == flags && entry.m_bits == format->m_bits && entry.m_mask[0] == format->m_mask0 && entry.m_mask[1] == format->m_mask1 && entry.m_mask[2] == format->m_mask2 &&
           entry.m_mask[3] == format->m_mask3;
}

void SetFrom(AtPixelFormat* out, const AtGrD3DFormatEntry& entry, unsigned int flags) {
    out->Set((unsigned short)flags, (unsigned short)entry.m_bits, entry.m_mask[0], entry.m_mask[1], entry.m_mask[2], entry.m_mask[3]);
}

// Meant as: up to the power of two at or above, a multiple of the highest bit set. But the original shifts the same word again on each step instead of shifting the side afresh, so the bits it looks at are
// 0, 1, 3, 6, 10, 15, 21 and 28: and the multiple is of 1 << (the step), so 33, 48 and 72 stay as they are and 9 becomes 12. A side that is a power of two comes back as it was.
unsigned int RoundUp(unsigned int value) {
    unsigned int top = 0;
    unsigned int shifted = value;
    for (unsigned int bit = 0; bit < 32; bit++) {
        shifted >>= bit;
        if ((shifted & 1) != 0) {
            top = bit;
        }
    }
    const unsigned int step = 1u << top;
    return (value + step - 1) & ~(step - 1);
}

}  // namespace

int AtGrD3DFormatFromDevice(unsigned int format, AtPixelFormat* out, int swizzled) {
    for (unsigned int i = 0; i < kSwizzledCount; i++) {
        const AtGrD3DFormatEntry& entry = g_atGrSwizzledFormats[i];
        if (entry.m_format == format) {
            SetFrom(out, entry, swizzled != 0 ? entry.m_flags : entry.m_flags & ~(unsigned int)AtPixelFormat::kSwizz);
            return 1;
        }
    }
    for (unsigned int i = 0; i < kLinearCount; i++) {
        const AtGrD3DFormatEntry& entry = g_atGrLinearFormats[i];
        if (entry.m_format == format) {
            SetFrom(out, entry, entry.m_flags);
            return 1;
        }
    }
    out->Clear();
    return 0;
}

int AtGrD3DIsNotLinear(unsigned int format) {
    for (unsigned int i = 0; i < kLinearCount; i++) {
        if (g_atGrLinearFormats[i].m_format == format) {
            return 0;
        }
    }
    return 1;
}

unsigned int AtGrD3DFormatToDevice(const AtPixelFormat* format) {
    const unsigned int flags = format->m_flags;
    if ((flags & AtPixelFormat::kSwizz) == 0) {
        for (unsigned int i = 0; i < kLinearCount; i++) {
            if (Matches(g_atGrLinearFormats[i], format, flags)) {
                return g_atGrLinearFormats[i].m_format;
            }
        }
        for (unsigned int i = 0; i < kLinearCount; i++) {
            if (g_atGrLinearFormats[i].m_bits == format->m_bits) {
                return g_atGrLinearFormats[i].m_format;
            }
        }
        return 0xFFFFFFFF;
    }
    for (unsigned int i = 0; i < kSwizzledCount; i++) {
        if (Matches(g_atGrSwizzledFormats[i], format, flags)) {
            return g_atGrSwizzledFormats[i].m_format;
        }
    }
    return 0xFFFFFFFF;
}

unsigned int AtGrD3DTextureLimits::Caps() const {
    unsigned int caps = m_caps;
    if ((m_capsMask & 4) != 0) {
        caps &= ~0x01000000u;
    }
    if ((m_capsMask & 8) != 0) {
        caps &= ~0x02000000u;
    }
    return caps;
}

// ---- Making the texture -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// The original constructs a pixel format on its frame here and never uses it.
void AtGrD3DChooseFormat(AtMaterialOwner* owner, unsigned int usage, int transparent, AtPixelFormat* format) {
    int swizzled = 1;
    if ((usage & AtTextureBase::kFrequentModify) != 0) {
        swizzled = 0;
    } else if ((format->m_flags & kCompressed) == 0) {
        format->m_flags |= AtPixelFormat::kSwizz;
    }
    AtGrD3DTextureLimits* limits = owner->m_textureLimits;
    AtGrD3DFormatChooser* chooser = owner->DrawLists();
    const unsigned int target = usage & AtTextureBase::kRenderTarget;
    if (target != 0) {
        format->m_flags |= AtPixelFormat::kSwizz;
    }
    if (transparent != 0) {
        // A transparent colour needs an alpha channel: an RGB format without one gets 8-8-8-8 if deeper than 16 bits, else 1-5-5-5; an indexed one gets 1-5-5-5 unless the device keeps indexed colour keys.
        if ((format->m_flags & AtPixelFormat::kRgba) != 0 && format->m_mask3 == 0) {
            if (format->m_bits > 16) {
                format->SetRgba(32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000, swizzled);
            } else {
                format->SetRgba(16, 0x7C00, 0x3E0, 0x1F, 0x8000, swizzled);
            }
        } else if ((format->m_flags & AtPixelFormat::kIndex) != 0 && (limits->Caps() & 0x10000) == 0) {
            format->SetRgba(16, 0x7C00, 0x3E0, 0x1F, 0x8000, swizzled);
        }
    }
    if (target != 0) {
        chooser->ChooseRenderTarget(format, format);
    } else {
        chooser->ChooseTexture(format, format);
    }
}

// Usage bit 0x80, which no word of a .AGT sets, also makes the texture square.
void AtGrD3DFitSize(AtMaterialOwner* owner, unsigned int usage, unsigned int* width, unsigned int* height, const AtPixelFormat* format) {
    AtGrD3DTextureLimits* limits = owner->m_textureLimits;
    *width = RoundUp(*width);
    *height = RoundUp(*height);
    if ((usage & 0x80) != 0 || (limits->Caps() & 0x100000) == 0) {
        if (*height < *width) {
            *height = *width;
        } else if (*height > *width) {
            *width = *height;
        }
    }
    unsigned int limit = limits->MinWidth(format->m_bits);
    if (*width < limit) {
        *width = limit;
    }
    limit = limits->MaxWidth(format->m_bits);
    if (*width > limit) {
        *width = limit;
    }
    limit = limits->MinHeight(format->m_bits);
    if (*height < limit) {
        *height = limit;
    }
    limit = limits->MaxHeight(format->m_bits);
    if (*height > limit) {
        *height = limit;
    }
}

void AtGrD3DMakeTexture(AtMaterialOwner*, unsigned int usage, unsigned int levels, unsigned int width, unsigned int height, unsigned int format, void** texture) {
    void* made = AtGrD3DCreateTexture(width, height, 1, levels, (usage & AtTextureBase::kRenderTarget) != 0 ? 1 : 0, format, 3);
    *texture = made;
    if (made == 0) {
        // The original prints its result, 0x8007000E, into the message.
        AtFatal("Atlas Graphics: create mip map failed = 8007000e\n", 0, 0, 0);
    }
}

void AtGrD3DCreateTexture2D(AtMaterialOwner* owner, unsigned int usage, int transparent, unsigned int* levels, unsigned int width, unsigned int height, const AtPixelFormat* format, void** texture) {
    AtPixelFormat chosen;
    chosen.Copy(*format);
    AtGrD3DChooseFormat(owner, usage, transparent, &chosen);
    AtGrD3DFitSize(owner, usage, &width, &height, &chosen);
    const unsigned int smallest = owner->m_textureLimits->MinWidth(chosen.m_bits);
    if ((chosen.m_flags & (kCompressed | AtPixelFormat::kSwizz)) == 0) {
        *levels = 1;
    }
    unsigned int most = 1;
    if (width > smallest) {
        unsigned int side = width;
        do {
            side >>= 1;
            most++;
        } while (side > smallest);
    }
    if (*levels > most) {
        *levels = most;
    }
    AtGrD3DMakeTexture(owner, usage, *levels, width, height, AtGrD3DFormatToDevice(&chosen), texture);
}

void AtGrD3DLockSurface(void* surface, unsigned int flags, void** bits, int* pitch) {
    AtGrD3DLockedRect locked;
    AtGrD3DLockRect(surface, &locked, 0, (flags & 1) != 0 ? 0x80 : 0);
    *bits = locked.m_bits;
    *pitch = locked.m_pitch;
}

// ---- Surfaces -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int AtGrD3DSurface2D::NeedsStaging() {
    if (m_surface == 0) {
        return 1;
    }
    if ((m_unknown1C & AtTextureBase::kSourceMask) == AtTextureBase::kSourceDds && (m_flags & 0x20) == 0) {
        return 0;
    }
    if (m_unknown40 != 0) {
        return 1;
    }
    AtGrD3DSurfaceDesc desc;
    AtGrD3DDescribe(m_surface, &desc);
    if (IsCompressedOnDevice(desc.m_format)) {
        return 1;
    }
    return AtGrD3DIsNotLinear(desc.m_format) != 0;
}

void AtGrD3DSurface2D::Lock(void** bits, int* pitch, unsigned int flags) {
    m_flags |= kLocked;
    unsigned short now = m_flags;
    if ((flags & 1) != 0) {
        now |= kLockedToRead;
    } else if ((flags & 2) != 0) {
        now |= kLockedToWrite;
    } else {
        now |= kLockedToRead | kLockedToWrite;
    }
    m_flags = now;
    if (m_unknown48 != 0) {
        *bits = m_unknown48;
        *pitch = (int)((((unsigned int)m_unknown20 * m_format.m_bits + 31) >> 3) & ~3u);
        return;
    }
    if (NeedsStaging()) {
        // A staging copy (0x000FDB70), filled from the device's surface when it is to be read (0x000FD870), then locked in its place: not recovered.
        STOP(0x000FDB70);
        if ((m_flags & kLockedToRead) != 0) {
            STOP(0x000FD870);
        }
        m_next->Lock(bits, pitch, flags);
        m_flags |= kStaged;
        return;
    }
    AtGrD3DLockSurface(m_surface, flags, bits, pitch);
}

// Each way out clears the lock flags (the low byte with 0xA3). For the device's surface itself, the original calls 0x001CC9F0, an empty function folded with others.
void AtGrD3DSurface2D::Unlock() {
    if (m_unknown48 != 0) {
        if (m_surface != 0 && (m_flags & kLockedToWrite) != 0) {
            // Writing its copy in memory back to the device, through a staging copy if need be (0x000FDB70, 0x000FD5A0, 0x000FDC90, 0x0005F370, 0x000FCEB0): not recovered.
            STOP(NeedsStaging() ? 0x000FDB70 : 0x000FD5A0);
        }
        m_flags &= 0xFFA3;
        return;
    }
    if ((m_flags & kStaged) != 0) {
        // The staging copy unlocked and written to the device (0x000FDC90, 0x0005F370), then deleted: not recovered.
        STOP(0x000FDC90);
        m_flags &= 0xFFA3;
        return;
    }
    m_flags &= 0xFFA3;
}

void AtGrD3DSurface2D::TakeDeviceFormat(unsigned int format, AtPixelFormat* out) {
    if (IsCompressedOnDevice(format)) {
        AtGrD3DFormatChooser* chooser = m_unknown24->DrawLists();
        if (format == 0x0C) {
            out->SetRgba(16, 0x7C00, 0x3E0, 0x1F, 0x8000, 0);
        } else {
            out->SetRgba(16, 0xF00, 0xF0, 0xF, 0xF000, 0);
        }
        chooser->ChooseTexture(out, out);
        return;
    }
    if (!AtGrD3DFormatFromDevice(format, out, 0)) {
        AtFatal("Atlas graphics: Unknown pixel type", 0, 0, 0);
    }
}

void AtGrD3DSurface2D::TakeDeviceSize(void* surface) {
    AtGrD3DSurfaceDesc desc;
    AtGrD3DDescribe(surface, &desc);
    if (desc.m_multiSample == 0x11) {
        m_unknown20 = (unsigned short)desc.m_width;
        m_unknown22 = (unsigned short)desc.m_height;
    } else {
        m_unknown20 = (unsigned short)(desc.m_width / ((desc.m_multiSample >> 4) & 0xF));
        m_unknown22 = (unsigned short)(desc.m_height / (desc.m_multiSample & 0xF));
    }
    TakeDeviceFormat(desc.m_format, &m_format);
}

// The transparent colour is set before m_flags is set to kLoaded alone, which forgets the colour key's flag.
void AtGrD3DSurface2D::Prepare(AtGrD3DOwner* owner, void* surface, unsigned int usage, AtGrD3DTexture2D* texture, AtGrD3DSurface2D* previous, const unsigned char* transparent) {
    if ((m_flags & kLoaded) != 0) {
        Reset();
    }
    if (transparent != 0) {
        SetColorKey(transparent);
    }
    m_unknown1C = (unsigned short)usage;
    m_unknown40 = previous;
    m_flags = kLoaded;
    m_unknown24 = owner;
    m_surface = surface;
    m_unknown3C = texture;
    TakeDeviceSize(surface);
    if ((m_format.m_flags & AtPixelFormat::kIndex) != 0) {
        // The palette made for an indexed format (0x000F5D40): not recovered.
        STOP(0x000F5D40);
    }
}

void AtGrD3DSurface2D::LoadFromDds(AtGrD3DOwner* owner, void* surface, AtGrDdsPixelMap* map, unsigned int usage, unsigned int level, unsigned int face, AtGrD3DTexture2D* texture, AtGrD3DSurface2D* previous) {
    if ((m_flags & kLoaded) != 0) {
        Reset();
    }
    m_unknown1C = (unsigned short)usage;
    m_flags = kLoaded;
    m_unknown24 = owner;
    m_surface = surface;
    m_unknown40 = previous;
    m_unknown3C = texture;
    AtGrD3DSurfaceDesc desc;
    AtGrD3DDescribe(surface, &desc);
    m_unknown20 = (unsigned short)desc.m_width;
    m_unknown22 = (unsigned short)desc.m_height;
    if (!AtGrD3DFormatFromDevice(desc.m_format, &m_format, 1)) {
        AtFatal("Atlas graphics: Unknown pixel type", 0, 0, 0);
    }
    if ((m_unknown24->m_noTextures & 4) != 0) {
        // The owner keeps a copy of each surface in memory. A level that is seldom changed and not compressed is copied into one at once (0x000FD330, 0x000FD4F0); otherwise the file's bytes are kept when
        // the first level loads (0x000FD380), and every level is loaded from them (0x000FD040). Neither is recovered.
        if ((m_unknown1C & AtTextureBase::kInfrequentModify) != 0 && (map->m_format.m_flags & kCompressed) == 0) {
            STOP(0x000FD330);
            return;
        }
        m_unknown1C &= ~(unsigned short)AtTextureBase::kInfrequentModify;
        if (level == 0 && face == 0) {
            STOP(0x000FD380);
            map->Free();
            STOP(0x000FD040);
        }
        return;
    }
    if ((m_format.m_flags & kCompressed) == 0 && (m_format.m_flags & AtPixelFormat::kIndex) == 0) {
        // A device format the file's rows cannot be copied into: loaded into a staging copy and converted (0x000FDB70, 0x0011D180), not recovered.
        STOP(0x000FDB70);
        return;
    }
    if ((m_format.m_flags & AtPixelFormat::kIndex) != 0) {
        STOP(0x000F5D40);  // the palette, as in Prepare
    }
    if ((m_unknown1C & AtTextureBase::kRenderTarget) == 0) {
        map->CopyLevel(owner, this, level, face);
    }
}

// ---- Textures -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DTexture2D::LoadLevelsFromDds(AtGrDdsPixelMap* map) {
    AtGrD3DSurface2D* surface = &m_surfaces[m_levels - 1];
    AtGrD3DSurface2D* previous = 0;
    void* level = 0;  // kept from one level to the next when there is no texture
    for (int i = (int)m_levels - 1; i >= 0; i--) {
        if (m_texture != 0) {
            level = AtGrD3DSurfaceLevel(m_texture, (unsigned int)i);
        }
        surface->LoadFromDds(m_owner, level, map, m_usage, (unsigned int)i, 0, this, previous);
        previous = surface;
        surface--;
    }
    map->Free();
}

void AtGrD3DTexture2D::Make(AtMaterialOwner* owner, unsigned int width, unsigned int height, const AtPixelFormat* format) {
    if ((m_flags & kPending) != 0) {
        return;
    }
    unsigned int levels = m_levelsAsked;
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    AtGrD3DCreateTexture2D(owner, m_usage, (m_flags >> 3) & 1, &levels, width, height, format, &m_texture);
    m_levels = (unsigned char)levels;
    const unsigned char* transparent = (m_flags & kTransparentColor) != 0 ? m_transparent : 0;
    AtGrD3DSurface2D* surface = &m_surfaces[m_levels - 1];
    AtGrD3DSurface2D* previous = 0;
    for (int i = (int)m_levels - 1; i >= 0; i--) {
        void* level = AtGrD3DSurfaceLevel(m_texture, (unsigned int)i);
        surface->Prepare(m_owner, level, m_usage, this, previous, transparent);
        previous = surface;
        surface--;
    }
    if (m_surfaces->Palette() != 0) {
        m_flags |= kHasPalette;
    } else {
        m_flags &= ~kHasPalette;
    }
    m_flags |= kPending;
}

void AtGrD3DTexture2D::OnLoaded(AtMaterialOwner* owner) {
    if ((m_flags & kPending) != 0) {
        return;
    }
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    if ((m_flags & (kFrontBufferSource | kBackBufferSource)) != 0) {
        m_levelsAsked = 1;
        m_levels = 1;
        m_texture = AtGrD3DGetBackBuffer((m_flags & kFrontBufferSource) != 0 ? -1 : 0);
        AtGrD3DRelease(m_texture);
        return;
    }
    if ((m_usage & kSourceMask) == kSourceProg) {
        Make(owner, (unsigned int)m_progSource->m_width, (unsigned int)m_progSource->m_height, &m_progSource->m_format);  // through slot 11 in the original
        FreeProgSource();
        return;
    }
    unsigned int levels = m_levelsAsked;
    AtGrIPixelMap* map;
    AtGrDdsPixelMap* dds;
    char name[16];
    if ((m_usage & kSourceMask) == kSourceDds) {
        dds = &AtGrDdsPixelMap::s_map;
        map = dds;
        AtCopyName9(name, m_name);
        name[8] = 0;
        dds->Load(name);
        if (dds->m_levels < levels) {
            levels = dds->m_levels;
        }
    } else {
        // The reader for the other sources (0x000FCE80, then 0x00060420): not recovered.
        map = (AtGrIPixelMap*)(unsigned long long)STOP(0x000FCE80);
        dds = 0;
        AtCopyName9(name, m_name);
        name[8] = 0;
        map->Load(name);
    }
    if ((m_usage & kSourceMask) == kSourceTm2) {
        m_usage &= ~(unsigned int)kInfrequentModify;
        levels = 1;
    }
    AtGrD3DCreateTexture2D(m_owner, m_usage, (m_flags >> 3) & 1, &levels, map->m_width, map->m_height, &map->m_format, &m_texture);
    m_levels = (unsigned char)levels;
    if (dds != 0) {
        LoadLevelsFromDds(dds);
    } else {
        STOP(0x000FC790);  // each level from the other readers, converted: not recovered
    }
    if (m_surfaces->Palette() != 0) {
        m_flags |= kHasPalette;
    } else {
        m_flags &= ~kHasPalette;
    }
    m_flags |= kPending;
}
