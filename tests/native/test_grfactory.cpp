// Host test of the graphics factory's start (at/grfactory), on any pointer size. Not evidence of a byte match.
//
// Direct3D is answered here by doubles (src/at/AtGraphicsFactoryD3d.cpp is not linked), from a script each case sets; the graphics device's texture
// members are stood in for as test_gfxdev does. Then: formats, the caps as flags, the start (tables, factory, adapter, names, modes), the graphics
// layer's Init on the real factory (the device made, App's material owner), and the stop.
#include <cstdio>
#include <cstring>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtGrD3DTextureLoad.h"
#include "at/AtGraphicsDevice.h"
#include "at/AtGraphicsFactory.h"
#include "at/AtGraphicsLayer.h"
#include "at/AtMatrix.h"
#include "at/AtMatrixDevice.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// ---- stand-ins for src/at/AtGraphicsDeviceHost.cpp ------------------------------------------------------------------------
AtGfxBufferSet::~AtGfxBufferSet() {}
AtGfxResourceXbox::~AtGfxResourceXbox() {}

// ---- Direct3D doubles -----------------------------------------------------------------------------------------------------
namespace {
struct Script {
    const char* driver = "drv";
    const char* description = "Desc";
    int capsResult = 0;
    AtD3DCaps caps{};
    std::vector<AtD3DDisplayMode> modes;
    unsigned int rejectFormat = 0xFFFF;
    int creates = 0, identifiers = 0, capsCalls = 0;
} g_d3d;
char g_interface;
}  // namespace

void* AtD3DCreate(unsigned int version) {
    CHECK(version == 0);
    g_d3d.creates++;
    return &g_interface;
}
int AtD3DGetAdapterIdentifier(unsigned int adapter, unsigned int flags, AtD3DAdapterIdentifier* out) {
    CHECK(adapter == 0 && flags == 0);
    g_d3d.identifiers++;
    std::memset(out, 0, sizeof *out);
    std::strcpy(out->m_driver, g_d3d.driver);
    std::strcpy(out->m_description, g_d3d.description);
    return 0;
}
int AtD3DGetDeviceCaps(unsigned int adapter, unsigned int type, AtD3DCaps* out) {
    CHECK(adapter == 0 && type == 1);
    g_d3d.capsCalls++;
    if (g_d3d.capsResult == 0) *out = g_d3d.caps;
    return g_d3d.capsResult;
}
unsigned int AtD3DGetAdapterModeCount(unsigned int) { return (unsigned int)g_d3d.modes.size(); }
int AtD3DEnumAdapterModes(unsigned int, unsigned int mode, AtD3DDisplayMode* out) {
    *out = g_d3d.modes[mode];
    return 0;
}
int AtD3DCheckDeviceType(unsigned int, unsigned int type, unsigned int display, unsigned int back, int windowed) {
    CHECK(type == 1 && display == back && windowed == 0);
    return display == g_d3d.rejectFormat ? -1 : 0;
}

namespace {
int g_heaps[8];

bool wide_is(const unsigned short* w, const char* s) {
    for (; *s; s++, w++) {
        if (*w != (unsigned short)(short)(signed char)*s) return false;
    }
    return *w == 0;
}

void test_formats() {
    AtPixelFormat f;
    CHECK(AtGrD3DFormatFromDevice(0x06, &f, 1) == 1);  // A8R8G8B8, swizzled
    CHECK(f.m_flags == (AtPixelFormat::kRgba | AtPixelFormat::kSwizz) && f.m_bits == 32);
    CHECK(AtGrD3DFormatFromDevice(0x06, &f, 0) == 1);
    CHECK(f.m_flags == AtPixelFormat::kRgba);
    CHECK(AtGrD3DFormatFromDevice(0x12, &f, 0) == 1);  // LIN_A8R8G8B8
    CHECK(f.m_flags == AtPixelFormat::kRgba && f.m_bits == 32);
    CHECK(AtGrD3DFormatFromDevice(0x0C, &f, 1) == 1);  // DXT1
    CHECK(f.m_flags == AtPixelFormat::kDxt1 && f.m_bits == 4);
    CHECK(AtGrD3DFormatFromDevice(0x99, &f, 1) == 0);
    CHECK(f.m_flags == 0 && f.m_bits == 0);
}

void test_caps() {
    AtD3DCaps caps{};
    unsigned int flags = 0;
    AtGfxCapsToFlags(&caps, &flags);
    CHECK(flags == (0x200 | 0x800 | 0x80000 | 0x100000 | 0x200000));  // the three always, and the two set by a bit's absence
    caps.m_rasterCaps = 0x8000;
    caps.m_textureCaps = 0x22;
    caps.m_textureAddressCaps = 0x8;
    flags = 0x40000000;
    AtGfxCapsToFlags(&caps, &flags);
    CHECK(flags == (0x40000000 | 0x1 | 0x200 | 0x800 | 0x80000 | 0x20000000));
}

void set_script() {
    g_d3d = Script();
    g_d3d.caps.m_devCaps = 0x80000 | 0x4000;
    g_d3d.caps.m_vertexShaderVersion = 0xFFFE0101;
    g_d3d.caps.m_stencilCaps = 0x80;
    g_d3d.caps.m_textureOpCaps = 0x40 | 0x200;
    g_d3d.caps.m_vertexProcessingCaps = 0x10;
    g_d3d.caps.m_zCmpCaps = 0x80;
    g_d3d.caps.m_srcBlendCaps = 0x40;
    g_d3d.caps.m_extentsAdjust = 0.5f;
    g_d3d.caps.m_unknown50[2] = 4096;
    g_d3d.modes.push_back({640, 480, 60, 0x10, 0x06});   // kept: swizzled A8R8G8B8, flag 0x40
    g_d3d.modes.push_back({720, 480, 60, 0x00, 0x99});   // dropped: not a known format
    g_d3d.modes.push_back({1280, 720, 60, 0x00, 0x05});  // dropped: the device does not take it
    g_d3d.modes.push_back({640, 480, 50, 0xA0, 0x12});   // kept: linear, flags 0x10 | 0x80
    g_d3d.rejectFormat = 0x05;
}

void test_start() {
    set_script();
    std::memset(&g_atMatrix, 0, sizeof g_atMatrix);
    std::memset(&g_atRender, 0, sizeof g_atRender);
    AtSetHeap3B1FA8(&g_heaps[0]);
    AtSetHeap3B1FAC(&g_heaps[1]);
    AtSetHeap3B1FB4(&g_heaps[2]);
    AtGraphicsGlobals saved;
    saved.Save();
    AtGraphicsConfig config{};
    AtGraphicsFactory* factory = AtGraphicsStartFactory(&saved, &config);
    CHECK(factory == &g_atGraphicsFactory);
    // The tables: both filled, the render table's own seven too.
    CHECK(g_atMatrix.Copy == AtMatrixCopy && g_atMatrix.Read == AtMatrixRead && g_atMatrix.GetRotation == AtMatrixGetRotation);
    CHECK(g_atRender.m_matrix.SetProduct == AtMatrixSetProduct && g_atRender.m_matrix.PostRotateZ == AtMatrixPostRotateZ);
    CHECK(g_atRender.m_extended[0] == (void*)AtRenderFunction0 && g_atRender.m_extended[6] == (void*)AtRenderFunction6);
    // One adapter, whatever Direct3D has.
    AtGfxAdapterList& list = factory->m_adapters;
    CHECK(g_d3d.creates == 1 && g_d3d.identifiers == 1 && g_d3d.capsCalls == 1);
    CHECK(list.Count() == 1 && list.m_d3d == &g_interface);
    AtGfxAdapter* a = list.At(0);
    CHECK(a->m_d3d == &g_interface && a->m_ordinal == 0);
    CHECK(wide_is(a->m_driverName, "drv") && wide_is(a->m_descriptionName, "Desc"));
    CHECK(a->m_driverText.m_data == a->m_driverName && a->m_driverText.m_capacity == 4);
    CHECK(a->HasDeviceInfo1() == 1 && a->HasDeviceInfo2() == 0 && a->HasDeviceInfo3() == 0);
    AtGfxDeviceInfo& info = a->m_info[0];
    CHECK(info.m_adapter == a && info.m_deviceType == 1);
    CHECK(info.m_modeCount == 2);
    CHECK(info.m_modes[0].m_width == 640 && info.m_modes[0].m_unknown1C == 60 && info.m_modes[0].m_unknown20 == 0x40);
    CHECK(info.m_modes[0].m_format.m_flags == (AtPixelFormat::kRgba | AtPixelFormat::kSwizz));
    CHECK(info.m_modes[1].m_unknown1C == 50 && info.m_modes[1].m_unknown20 == (0x10 | 0x80) && info.m_modes[1].m_format.m_flags == AtPixelFormat::kRgba);
    CHECK(info.m_deviceFlags == (0x1 | 0x4 | 0x80 | 0x400 | 0x200));
    CHECK(info.m_stencilFlags == 0x1 && info.m_textureOpFlags == (0x1 | 0x400000) && info.m_vertexFlags == (0x10 | 0x20));
    CHECK(info.m_zCmpFlags == 1 && info.m_zCmpFlags2 == 1 && info.m_srcBlendFlags == 1 && info.m_destBlendFlags == 0);
    CHECK((info.m_rasterFlags & 0x80) && (info.m_rasterFlags2 & 0x80));
    CHECK(info.Slot2(0) == 4096 && info.Slot1(0) == 1);

    // The graphics layer's Init on the real factory: the factory's slot 0 is the adapter list, its slot 3 the real device.
    AtGraphicsStopFactory();
    CHECK(list.m_adapters == nullptr && list.Count() == 0);
    set_script();
    g_d3d.capsResult = (int)0x8876086A;  // the caps call fails: the adapter is kept with nothing enumerated
    AtGraphicsSystem layer;
    static unsigned char memory[64];
    config.m_unknown00 = 1;
    config.m_heap = (AtHeap*)&g_heaps[3];
    layer.Init(&config, nullptr, memory);
    CHECK(config.m_unknown00 == 2 && layer.m_factory == &g_atGraphicsFactory);
    CHECK(layer.m_unknown18 == &g_atGraphicsFactory.m_adapters);
    AtGraphicsDeviceXbox* device = (AtGraphicsDeviceXbox*)layer.m_materialOwner;
    CHECK(device != nullptr && device->m_creator == &g_atGraphicsFactory);
    CHECK(device->MatrixDevice() == &device->m_matrixDevice);
    AtGfxAdapter* b = g_atGraphicsFactory.m_adapters.At(0);
    CHECK(b->m_info[0].m_adapter == nullptr && b->m_info[0].m_modes == nullptr && b->HasDeviceInfo1() == 0);
    CHECK(wide_is(b->m_descriptionName, "Desc"));
    // The device is not destroyed: its destructor's first step, Release, is not recovered (at/gfxdev).
    AtGraphicsStopFactory();
}
}  // namespace

int main() {
    std::printf("formats\n");
    test_formats();
    std::printf("caps\n");
    test_caps();
    std::printf("start\n");
    test_start();
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("grfactory: all checks passed\n");
    return 0;
}
