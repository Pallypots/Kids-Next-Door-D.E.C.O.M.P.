// The graphics factory, its adapter list, the adapters and what Direct3D reports of each. Subsystem at/grfactory; see
// include/at/AtGraphicsFactory.h. Names are inferred. Verification status per function: config/functions.d/grfactory.toml.
#include "at/AtGraphicsFactory.h"

#include "at/AtCoordSys.h"
#include "at/AtGraphicsDevice.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtMatrix.h"

void AtPushHeap(void* heap);
void AtPopHeap();
void AtRaise(int code, int file, int line, int unused);

// ---- the factory -----------------------------------------------------------------------------------------------------------

AtGraphicsFactory::AtGraphicsFactory() {}

AtGraphicsFactory::~AtGraphicsFactory() {
    m_adapters.Clear();
}

void AtGraphicsFactory::Start() {
    m_adapters.Init();
}

void AtGraphicsFactory::Stop() {
    m_adapters.Clear();
}

// ---- the adapter list ------------------------------------------------------------------------------------------------------

AtGfxAdapterListBase::~AtGfxAdapterListBase() {}

AtGfxAdapterList::AtGfxAdapterList() {
    m_d3d = 0;
    m_adapters = 0;
    m_count = 0;
}

AtGfxAdapterList::~AtGfxAdapterList() {
    Clear();
}

unsigned int AtGfxAdapterList::Count() {
    return m_count;
}

AtGfxAdapter* AtGfxAdapterList::At(unsigned int index) {
    return &m_adapters[index];
}

void AtGfxAdapterList::Clear() {
    if (m_adapters != 0) {
        delete[] m_adapters;
    }
    m_d3d = 0;
    m_adapters = 0;
    m_count = 0;
}

void AtGfxAdapterList::Init() {
    if (m_adapters != 0) {
        delete[] m_adapters;
        m_d3d = 0;
        m_adapters = 0;
        m_count = 0;
    }
    m_d3d = AtD3DCreate(0);
    m_count = 1;  // ORIGINAL: one adapter, whatever Direct3D has
    if (g_heap3B1FA8 == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(g_heap3B1FA8);
    m_adapters = new AtGfxAdapter[m_count];
    AtPopHeap();
    if (m_adapters == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_count; i++) {
        m_adapters[i].Init(m_d3d, i);
    }
}

// ---- an adapter ------------------------------------------------------------------------------------------------------------

AtGfxAdapterBase::AtGfxAdapterBase() {}

void AtGfxAdapterBase::ReleaseTexts() {
    m_driverText.Release();
    m_descriptionText.Release();
}

AtGfxAdapterBase::~AtGfxAdapterBase() {}

AtGfxAdapter::AtGfxAdapter() {
    m_d3d = 0;
    m_ordinal = 0;
    unsigned int* words = (unsigned int*)&m_identifier;
    for (unsigned int i = 0; i < sizeof(m_identifier) / 4; i++) {
        words[i] = 0;
    }
    m_driverName = 0;
    m_descriptionName = 0;
}

AtGfxAdapter::~AtGfxAdapter() {
    AtGfxAdapter::ReleaseTexts();
}

void AtGfxAdapter::ReleaseTexts() {
    if (m_driverName != 0) {
        operator delete(m_driverName);
    }
    if (m_descriptionName != 0) {
        operator delete(m_descriptionName);
    }
    m_info[0].Clear();
    m_info[1].Clear();
    m_info[2].Clear();
    m_d3d = 0;
    m_ordinal = 0;
    unsigned int* words = (unsigned int*)&m_identifier;
    for (unsigned int i = 0; i < sizeof(m_identifier) / 4; i++) {
        words[i] = 0;
    }
    m_driverName = 0;  // ORIGINAL: the texts over the names are not released and keep pointing at them
    m_descriptionName = 0;
}

int AtGfxAdapter::HasDeviceInfo1() {
    return m_info[0].m_adapter != 0;
}

int AtGfxAdapter::HasDeviceInfo2() {
    return m_info[1].m_adapter != 0;
}

int AtGfxAdapter::HasDeviceInfo3() {
    return m_info[2].m_adapter != 0;
}

namespace {
// A name widened from the identifier into a new block of the adapter heap, terminated. Each byte is taken as signed (movsx).
unsigned short* Widen(const char* name, unsigned int* length) {
    unsigned int n = 0;
    while (name[n] != 0) {
        n++;
    }
    *length = n + 1;
    if (g_heap3B1FAC == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(g_heap3B1FAC);
    unsigned short* wide = (unsigned short*)operator new(*length * 2);
    AtPopHeap();
    return wide;
}
void CopyWide(unsigned short* to, const char* from) {
    while (*from != 0) {
        *to++ = (unsigned short)(short)(signed char)*from++;
    }
    *to = 0;
}
}  // namespace

void AtGfxAdapter::Init(void* d3d, unsigned int ordinal) {
    if (m_d3d != 0) {
        ReleaseTexts();  // slot 0: Clear
    }
    m_d3d = d3d;
    m_ordinal = ordinal;
    AtD3DGetAdapterIdentifier(ordinal, 0, &m_identifier);  // ORIGINAL: what it returns is not looked at

    unsigned int length;
    m_driverName = Widen(m_identifier.m_driver, &length);
    if (m_driverName == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyWide(m_driverName, m_identifier.m_driver);
    m_driverText.Init(m_driverName, length);

    m_descriptionName = Widen(m_identifier.m_description, &length);
    if (m_descriptionName == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyWide(m_descriptionName, m_identifier.m_description);
    m_descriptionText.Init(m_descriptionName, length);

    m_info[0].Enumerate(this, 1);
}

// ---- the device infos ------------------------------------------------------------------------------------------------------

AtGfxDeviceInfoBase::AtGfxDeviceInfoBase() {
    m_adapter = 0;
    m_deviceFlags = 0;
    m_stencilFlags = 0;
    m_textureOpFlags = 0;
    m_vertexFlags = 0;
    m_rasterFlags = 0;
    m_zCmpFlags = 0;
    m_alphaCmpFlags = 0;
    m_srcBlendFlags = 0;
    m_destBlendFlags = 0;
    m_rasterFlags2 = 0;
    m_zCmpFlags2 = 0;
    m_alphaCmpFlags2 = 0;
    m_srcBlendFlags2 = 0;
    m_destBlendFlags2 = 0;
    m_unknown68 = 0;
    m_text40.Release();
    m_text54.Release();
}

AtGfxDeviceInfoBase::~AtGfxDeviceInfoBase() {
    m_text40.Release();
    m_text54.Release();
}

void AtGfxDeviceInfoBase::Reset() {
    m_adapter = 0;
    m_deviceFlags = 0;
    m_stencilFlags = 0;
    m_textureOpFlags = 0;
    m_vertexFlags = 0;
    m_rasterFlags = 0;
    m_zCmpFlags = 0;
    m_alphaCmpFlags = 0;
    m_srcBlendFlags = 0;
    m_destBlendFlags = 0;
    m_rasterFlags2 = 0;
    m_zCmpFlags2 = 0;
    m_alphaCmpFlags2 = 0;
    m_srcBlendFlags2 = 0;
    m_destBlendFlags2 = 0;
    m_unknown68 = 0;
    m_text40.Release();
    m_text54.Release();
}

namespace {
void ClearCaps(AtD3DCaps* caps) {
    unsigned int* words = (unsigned int*)caps;
    for (unsigned int i = 0; i < sizeof(AtD3DCaps) / 4; i++) {
        words[i] = 0;
    }
}
}  // namespace

AtGfxDeviceInfo::AtGfxDeviceInfo() {
    ClearCaps(&m_caps);
    m_modeCount = 0;
    m_modes = 0;
    m_deviceType = 1;
}

AtGfxDeviceInfo::~AtGfxDeviceInfo() {
    if (m_modes != 0) {
        operator delete(m_modes);
    }
    Reset();
    ClearCaps(&m_caps);
    m_deviceType = 1;
    m_modeCount = 0;
    m_modes = 0;
}

int AtGfxDeviceInfo::Slot1(int) {
    return 1;
}

unsigned int AtGfxDeviceInfo::Slot2(int) {
    return m_caps.m_unknown50[2];  // MaxTextureWidth, caps +0x58
}

int AtGfxDeviceInfo::Slot3(int) {
    return 1;
}

unsigned int AtGfxDeviceInfo::Slot4(int) {
    return m_caps.m_unknown50[3];  // MaxTextureHeight, caps +0x5C
}

void AtGfxDeviceInfo::Clear() {
    if (m_modes != 0) {
        operator delete(m_modes);
    }
    Reset();
    ClearCaps(&m_caps);
    m_deviceType = 1;
    m_modeCount = 0;
    m_modes = 0;
}

void AtGfxDeviceInfo::Enumerate(AtGfxAdapter* adapter, unsigned int deviceType) {
    AtPixelFormat format;
    if (m_adapter != 0) {
        if (m_modes != 0) {
            operator delete(m_modes);
        }
        Reset();
        ClearCaps(&m_caps);
        m_deviceType = 1;
        m_modeCount = 0;
        m_modes = 0;
    }
    const unsigned int ordinal = adapter->m_ordinal;
    if (AtD3DGetDeviceCaps(ordinal, deviceType, &m_caps) != 0) {
        return;
    }
    m_adapter = adapter;
    m_deviceType = deviceType;
    m_modeCount = AtD3DGetAdapterModeCount(ordinal);
    if (g_heap3B1FB4 == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(g_heap3B1FB4);
    m_modes = new AtDisplayMode[m_modeCount];
    AtPopHeap();
    if (m_modes == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    AtDisplayMode* kept = m_modes;
    for (unsigned int i = 0; i < m_modeCount; i++) {
        AtD3DDisplayMode mode;
        AtD3DEnumAdapterModes(ordinal, i, &mode);  // ORIGINAL: what it returns is not looked at
        if (AtD3DCheckDeviceType(ordinal, m_deviceType, mode.m_format, mode.m_format, 0) != 0) {
            continue;
        }
        if (AtD3DFormatToPixelFormat(mode.m_format, &format, 1) == 0) {
            continue;
        }
        const unsigned char d3dFlags = (unsigned char)mode.m_flags;
        int flags = 0;
        if (d3dFlags & 0x10) {
            flags = 0x40;
        }
        if (d3dFlags & 0x20) {
            flags |= 0x10;
        }
        if (d3dFlags & 0x40) {
            flags |= 0x20;
        }
        if (d3dFlags & 0x80) {
            flags |= 0x80;
        }
        kept->Set(mode.m_width, mode.m_height, &format, (int)mode.m_refreshRate, flags);
        kept++;
    }
    m_modeCount = (unsigned int)(kept - m_modes);  // the modes not kept stay at the end, as constructed
    ReadCaps();
}

namespace {
// Each word of the caps against a list of (bit of Direct3D's, bit of the engine's): 0x00289518 for the texture operations (23 pairs), 0x00289640 for
// the blend factors (11 pairs).
struct BitPair {
    unsigned int from;
    unsigned int to;
};
const BitPair kTextureOps[23] = {
    {0x40, 0x1},       {0x80, 0x2},       {0x100, 0x4},     {0x400, 0x8},     {0x800, 0x10},    {0x1000, 0x20},   {0x2000, 0x40},   {0x8000, 0x80},
    {0x4000, 0x100},   {0x200000, 0x200}, {0x400000, 0x400}, {0x800000, 0x800}, {0x8, 0x1000},  {0x10, 0x2000},   {0x20, 0x4000},   {0x20000, 0x8000},
    {0x40000, 0x10000}, {0x80000, 0x20000}, {0x100000, 0x40000}, {0x10000, 0x80000}, {0x2, 0x100000}, {0x4, 0x200000}, {0x200, 0x400000},
};
const BitPair kBlends[11] = {
    {0x40, 0x1}, {0x100, 0x2}, {0x80, 0x4}, {0x200, 0x8}, {0x20, 0x10}, {0x8, 0x20}, {0x2, 0x40}, {0x10, 0x80}, {0x400, 0x100}, {0x4, 0x200}, {0x1, 0x400},
};
unsigned int Map(const BitPair* pairs, unsigned int count, unsigned int bits) {
    unsigned int out = 0;
    for (unsigned int i = 0; i < count; i++) {
        if (bits & pairs[i].from) {
            out |= pairs[i].to;
        }
    }
    return out;
}
// The comparison caps (Z and alpha alike).
unsigned int CmpFlags(unsigned int c) {
    unsigned int out = 0;
    if (c & 0x80) out |= 1;
    if (c & 0x04) out |= 2;
    if (c & 0x10) out |= 4;
    if (c & 0x40) out |= 8;
    if (c & 0x02) out |= 0x10;
    if (c & 0x08) out |= 0x20;
    if (c & 0x01) out |= 0x40;
    if (c & 0x20) out |= 0x80;
    return out;
}
}  // namespace

void AtGfxCapsToFlags(const AtD3DCaps* caps, unsigned int* flags) {
    const unsigned int raster = caps->m_rasterCaps;
    if (raster & 0x8000) *flags |= 0x1;
    if (raster & 0x40000) *flags |= 0x2;
    if (raster & 0x100000) *flags |= 0x4;
    if (raster & 0x200000) *flags |= 0x8;
    if (raster & 0x100) *flags |= 0x10;
    if (raster & 0x80) *flags |= 0x20;
    *flags |= 0x200;
    if (caps->m_shadeCaps & 0x8) *flags |= 0x400;
    *flags |= 0x800;
    if (caps->m_shadeCaps & 0x4000) *flags |= 0x1000;
    if (caps->m_shadeCaps & 0x80000) *flags |= 0x4000;
    const unsigned int texture = caps->m_textureCaps;
    if (texture & 0x4) *flags |= 0x8000;
    if (texture & 0x80) *flags |= 0x10000;
    if (texture & 0x800) *flags |= 0x20000;
    if (texture & 0x1) *flags |= 0x40000;
    *flags |= 0x80000;
    if ((texture & 0x20) == 0) *flags |= 0x100000;
    if ((texture & 0x2) == 0) *flags |= 0x200000;
    const unsigned int filter = caps->m_textureFilterCaps;
    if (filter & 0x2000200) *flags |= 0x400000;
    if (filter & 0x10000) *flags |= 0x800000;
    if (filter & 0x20000) *flags |= 0x1000000;
    if (filter & 0x4000400) *flags |= 0x2000000;
    const unsigned int address = caps->m_textureAddressCaps;
    if (address & 0x1) *flags |= 0x4000000;
    if (address & 0x4) *flags |= 0x8000000;
    if (address & 0x2) *flags |= 0x10000000;
    if (address & 0x8) *flags |= 0x20000000;
    if (texture & 0x400) *flags |= 0x40000000;
}

void AtGfxDeviceInfo::ReadCaps() {
    const unsigned int dev = m_caps.m_devCaps;
    if (dev & 0x80000) m_deviceFlags |= 0x1;
    if (dev & 0x10000) m_deviceFlags |= 0x2;
    if (dev & 0x4000) m_deviceFlags |= 0x4;
    if (dev & 0x1000) m_deviceFlags |= 0x8;
    if (dev & 0x100000) m_deviceFlags |= 0x20;
    if (dev & 0x20000) m_deviceFlags |= 0x40;
    if (dev & 0x80000) m_deviceFlags |= 0x80;  // ORIGINAL: the same bit as 0x1
    if (m_caps.m_vertexShaderVersion & 0xFFFF) m_deviceFlags |= 0x400;
    if (m_caps.m_pixelShaderVersion & 0xFFFF) m_deviceFlags |= 0x800;

    const unsigned int stencil = m_caps.m_stencilCaps;
    if (stencil & 0x80) m_stencilFlags |= 0x1;
    if (stencil & 0x10) m_stencilFlags |= 0x2;
    if (stencil & 0x40) m_stencilFlags |= 0x4;
    if (stencil & 0x08) m_stencilFlags |= 0x8;
    if (stencil & 0x20) m_stencilFlags |= 0x10;
    if (stencil & 0x01) m_stencilFlags |= 0x20;
    if (stencil & 0x04) m_stencilFlags |= 0x40;
    if (stencil & 0x02) m_stencilFlags |= 0x80;

    m_textureOpFlags |= Map(kTextureOps, 23, m_caps.m_textureOpCaps);

    const unsigned int vertex = m_caps.m_vertexProcessingCaps;
    if (vertex & 0x1) m_vertexFlags |= 0x1;
    if (vertex & 0x2) m_vertexFlags |= 0x2;
    if (vertex & 0x8) m_vertexFlags |= 0x8;
    if (vertex & 0x10) m_vertexFlags |= 0x10;
    if (vertex & 0x10) m_vertexFlags |= 0x20;  // ORIGINAL: both from the one bit
    if (vertex & 0x20) m_vertexFlags |= 0x40;

    AtGfxCapsToFlags(&m_caps, &m_rasterFlags);
    m_zCmpFlags |= CmpFlags(m_caps.m_zCmpCaps);
    m_alphaCmpFlags |= CmpFlags(m_caps.m_alphaCmpCaps);
    m_srcBlendFlags |= Map(kBlends, 11, m_caps.m_srcBlendCaps);
    m_destBlendFlags |= Map(kBlends, 11, m_caps.m_destBlendCaps);
    AtGfxCapsToFlags(&m_caps, &m_rasterFlags2);
    m_zCmpFlags2 |= CmpFlags(m_caps.m_zCmpCaps);
    m_alphaCmpFlags2 |= CmpFlags(m_caps.m_alphaCmpCaps);
    m_srcBlendFlags2 |= Map(kBlends, 11, m_caps.m_srcBlendCaps);
    m_destBlendFlags2 |= Map(kBlends, 11, m_caps.m_destBlendCaps);

    if (!(m_caps.m_extentsAdjust == 0.0f)) {  // a NaN counts as not zero
        m_rasterFlags |= 0x80;
        m_rasterFlags2 |= 0x80;
    }
    m_deviceFlags |= 0x200;
}

// ---- formats and modes -----------------------------------------------------------------------------------------------------

namespace {
struct FormatEntry {
    unsigned int format;
    unsigned int flags;
    unsigned int bits;
    unsigned int masks[4];
};
// 0x0028A930: 28 entries.
// The Xbox's swizzled formats, each with AtPixelFormat::kSwizz.
const FormatEntry kSwizzledTable[28] = {
    {0x06, 0x801, 32, {0xFF0000, 0xFF00, 0xFF, 0xFF000000}}, {0x07, 0x801, 32, {0xFF0000, 0xFF00, 0xFF, 0}},
    {0x05, 0x801, 16, {0xF800, 0x7E0, 0x1F, 0}},            {0x27, 0x801, 16, {0xFC00, 0x3E0, 0x1F, 0}},
    {0x03, 0x801, 16, {0x7C00, 0x3E0, 0x1F, 0}},            {0x02, 0x801, 16, {0x7C00, 0x3E0, 0x1F, 0x8000}},
    {0x04, 0x801, 16, {0xF00, 0xF0, 0xF, 0xF000}},          {0x19, 0x802, 8, {0, 0, 0, 0xFF}},
    {0x3A, 0x801, 32, {0xFF, 0xFF00, 0xFF0000, 0xFF000000}}, {0x3B, 0x801, 32, {0xFF00, 0xFF0000, 0xFF000000, 0xFF}},
    {0x39, 0x801, 16, {0xF000, 0xF00, 0xF0, 0xF}},          {0x38, 0x801, 16, {0xF800, 0x7C0, 0x3E, 0x1}},
    {0x3C, 0x801, 32, {0xFF000000, 0xFF0000, 0xFF00, 0xFF}}, {0x29, 0x801, 16, {0xFF00, 0xFF, 0, 0}},
    {0x28, 0x801, 16, {0, 0xFF, 0xFF00, 0}},                {0x0B, 0x804, 8, {0xFF, 0, 0, 0}},
    {0x00, 0x802, 8, {0xFF, 0, 0, 0}},                      {0x1A, 0x802, 16, {0xFF, 0, 0, 0xFF00}},
    {0x01, 0x802, 8, {0, 0, 0, 0xFF}},                      {0x32, 0x802, 16, {0xFFFF, 0, 0, 0}},
    {0x28, 0x808, 16, {0, 0xFF, 0xFF00, 0}},                {0x27, 0x808, 16, {0xFC00, 0x1F, 0x3E0, 0}},
    {0x07, 0x808, 32, {0xFF0000, 0xFF, 0xFF00, 0}},         {0x3A, 0x808, 32, {0xFF0000, 0xFF, 0xFF00, 0xFF000000}},
    {0x33, 0x808, 32, {0, 0xFFFF, 0xFFFF0000, 0}},          {0x2A, 0x810, 32, {0xFFFFFF00, 0xFF, 0, 0}},
    {0x2C, 0x810, 16, {0xFFFF, 0, 0, 0}},                   {0x2C, 0x810, 16, {0xFFFF, 0, 0, 0}},
};
// 0x0028AC40: 26 entries.
// The linear (D3DFMT_LIN_) and compressed formats.
const FormatEntry kLinearTable[26] = {
    {0x0C, 0x40, 4, {0, 0, 0, 0}},                          {0x0E, 0x100, 8, {0, 0, 0, 0}},
    {0x0E, 0x80, 8, {0, 0, 0, 0}},                          {0x0F, 0x200, 8, {0, 0, 0, 0}},
    {0x0F, 0x400, 8, {0, 0, 0, 0}},                         {0x10, 0x1, 16, {0x7C00, 0x3E0, 0x1F, 0x8000}},
    {0x1D, 0x1, 16, {0xF00, 0xF0, 0xF, 0xF000}},            {0x1F, 0x2, 8, {0, 0, 0, 0xFF}},
    {0x3F, 0x1, 32, {0xFF, 0xFF00, 0xFF0000, 0xFF000000}},  {0x12, 0x1, 32, {0xFF0000, 0xFF00, 0xFF, 0xFF000000}},
    {0x40, 0x1, 32, {0xFF00, 0xFF0000, 0xFF000000, 0xFF}},  {0x17, 0x1, 16, {0, 0xFF, 0xFF00, 0}},
    {0x3E, 0x1, 16, {0xF000, 0xF00, 0xF0, 0xF}},            {0x3D, 0x1, 16, {0xF800, 0x7C0, 0x3E, 0x1}},
    {0x11, 0x1, 16, {0xF800, 0x7E0, 0x1F, 0}},              {0x37, 0x1, 16, {0xFC00, 0x3E0, 0x1F, 0}},
    {0x16, 0x1, 16, {0xFF00, 0xFF, 0, 0}},                  {0x41, 0x1, 32, {0xFF000000, 0xFF0000, 0xFF00, 0xFF}},
    {0x1C, 0x1, 16, {0x7C00, 0x3E0, 0x1F, 0}},              {0x1E, 0x1, 32, {0xFF0000, 0xFF00, 0xFF, 0}},
    {0x20, 0x2, 16, {0xFF, 0, 0, 0xFF00}},                  {0x1B, 0x1, 8, {0, 0, 0, 0xFF}},
    {0x35, 0x2, 16, {0xFFFF, 0, 0, 0}},                     {0x13, 0x4, 8, {0xFF, 0, 0, 0}},
    {0x2E, 0x10, 32, {0xFFFFFF00, 0xFF, 0, 0}},             {0x30, 0x10, 16, {0xFFFF, 0, 0, 0}},
};
}  // namespace

int AtD3DFormatToPixelFormat(unsigned int format, AtPixelFormat* out, int swizzled) {
    for (const FormatEntry& e : kSwizzledTable) {
        if (e.format == format) {
            unsigned int flags = e.flags;
            if (swizzled == 0) {
                flags &= ~(unsigned int)AtPixelFormat::kSwizz;
            }
            out->Set((unsigned short)flags, (unsigned short)e.bits, e.masks[0], e.masks[1], e.masks[2], e.masks[3]);
            return 1;
        }
    }
    for (const FormatEntry& e : kLinearTable) {
        if (e.format == format) {
            out->Set((unsigned short)e.flags, (unsigned short)e.bits, e.masks[0], e.masks[1], e.masks[2], e.masks[3]);
            return 1;
        }
    }
    out->Clear();
    return 0;
}

void AtDisplayMode::Set(unsigned int width, unsigned int height, const AtPixelFormat* format, int unknown1C, int unknown20) {
    m_width = width;
    m_height = height;
    m_format.Copy(*format);
    m_unknown1C = unknown1C;
    m_unknown20 = unknown20;
}
