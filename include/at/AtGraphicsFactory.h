// The graphics factory's start: the render function tables, and the list of display adapters with what Direct3D reports of each (identity, device
// capabilities, display modes). Subsystem at/grfactory. Reference build knd-xbox-usa-5454008a-v1. Original address clusters 0x000EB520-0x000EB9E6,
// 0x000EBD70-0x000EBF2E, 0x000EC2B0-0x000EC6CD, 0x000FA500-0x000FAD2E, 0x000FBA70-0x000FC54B, 0x0011C840-0x0011C9EE, and 0x001197D0, 0x00043420.
//
// All names are inferred. Evidence for the roles: 0x00206070 returns 1 for an argument of 0, as the Xbox SDK's Direct3DCreate8 hands back a dummy
// interface (the SDK has no COM object); 0x00206080 fails for any adapter but 0 and copies a 0x42C-byte block, the size of D3DADAPTER_IDENTIFIER8;
// 0x002062B0 fills 0xD4 bytes, the size of D3DCAPS8, and its fields are read at D3DCAPS8's offsets (DevCaps, StencilCaps, the shader versions); the
// modes enumerated are 0x14-byte records of width, height, refresh rate, flags and format, as D3DDISPLAYMODE. The layouts below are the public
// Direct3D 8 ones, named as there.
//
// Direct3D is the SDK's library, not game code, and is hardware: the six entry points the start reaches are seams (src/at/AtGraphicsFactoryD3d.cpp),
// which stop with a diagnostic unless a program supplies them (the oracle, a host test, or a port's platform/host_d3d.cpp).
#pragma once
#include "at/AtDisplayDevice.h"
#include "at/AtText.h"

struct AtMatrixTable;
struct AtRenderTable;
struct AtMaterialOwner;
class AtDatabaseLoader;

// ---- Direct3D, behind the seam ----------------------------------------------------------------------------------------------

struct AtD3DAdapterIdentifier {      // D3DADAPTER_IDENTIFIER8, 0x42C bytes
    char m_driver[512];              // +0x000
    char m_description[512];         // +0x200
    unsigned int m_driverVersion[2]; // +0x400
    unsigned int m_vendorId;         // +0x408
    unsigned int m_deviceId;         // +0x40C
    unsigned int m_subSysId;         // +0x410
    unsigned int m_revision;         // +0x414
    unsigned int m_deviceIdentifier[4];  // +0x418
    unsigned int m_whqlLevel;        // +0x428
};
struct AtD3DCaps {                   // D3DCAPS8, 0xD4 bytes; only the fields read here are named
    unsigned int m_unknown00[7];     // +0x00 DeviceType, AdapterOrdinal, Caps, Caps2, Caps3, PresentationIntervals, CursorCaps
    unsigned int m_devCaps;          // +0x1C
    unsigned int m_primitiveMiscCaps;  // +0x20
    unsigned int m_rasterCaps;       // +0x24
    unsigned int m_zCmpCaps;         // +0x28
    unsigned int m_srcBlendCaps;     // +0x2C
    unsigned int m_destBlendCaps;    // +0x30
    unsigned int m_alphaCmpCaps;     // +0x34
    unsigned int m_shadeCaps;        // +0x38
    unsigned int m_textureCaps;      // +0x3C
    unsigned int m_textureFilterCaps;  // +0x40
    unsigned int m_unknown44[2];     // +0x44 CubeTextureFilterCaps, VolumeTextureFilterCaps
    unsigned int m_textureAddressCaps;  // +0x4C
    unsigned int m_unknown50[13];    // +0x50 VolumeTextureAddressCaps to GuardBandBottom
    float m_extentsAdjust;           // +0x84
    unsigned int m_stencilCaps;      // +0x88
    unsigned int m_fvfCaps;          // +0x8C
    unsigned int m_textureOpCaps;    // +0x90
    unsigned int m_unknown94[2];     // +0x94 MaxTextureBlendStages, MaxSimultaneousTextures
    unsigned int m_vertexProcessingCaps;  // +0x9C
    unsigned int m_unknownA0[9];     // +0xA0 MaxActiveLights to MaxStreamStride
    unsigned int m_vertexShaderVersion;  // +0xC4
    unsigned int m_maxVertexShaderConst;  // +0xC8
    unsigned int m_pixelShaderVersion;  // +0xCC
    float m_maxPixelShaderValue;     // +0xD0
};
struct AtD3DDisplayMode {            // D3DDISPLAYMODE, 0x14 bytes
    unsigned int m_width;            // +0x00
    unsigned int m_height;           // +0x04
    unsigned int m_refreshRate;      // +0x08
    unsigned int m_flags;            // +0x0C
    unsigned int m_format;           // +0x10
};

// The six entry points of the SDK's Direct3D that the start reaches (stdcall in the original; each argument count is the original's ret N / 4).
void* AtD3DCreate(unsigned int sdkVersion);                                                    // 0x00206070 Direct3DCreate8
int AtD3DGetAdapterIdentifier(unsigned int adapter, unsigned int flags, AtD3DAdapterIdentifier* out);  // 0x00206080
int AtD3DGetDeviceCaps(unsigned int adapter, unsigned int deviceType, AtD3DCaps* out);        // 0x002062B0
unsigned int AtD3DGetAdapterModeCount(unsigned int adapter);                                    // 0x002060B0
int AtD3DEnumAdapterModes(unsigned int adapter, unsigned int mode, AtD3DDisplayMode* out);    // 0x00206150
int AtD3DCheckDeviceType(unsigned int adapter, unsigned int deviceType, unsigned int displayFormat, unsigned int backBufferFormat, int windowed);  // 0x002063F0

// A Direct3D format as a pixel format, from two tables of the original: 0x0028A930, 28 swizzled formats, each with AtPixelFormat::kSwizz, and
// 0x0028AC40, 26 linear and compressed ones. Not found: the format cleared and 0. swizzled 0 takes kSwizz out of what the first table gives; the
// modes are looked up with 1. The format codes are the Xbox's D3DFORMAT values.
// AtD3DFormatToPixelFormat (0x001197D0) is at/texload's AtGrD3DFormatFromDevice, include/at/AtGrD3DTextureLoad.h.

// ---- the render function tables ------------------------------------------------------------------------------------------------

// 0x000EC2B0 writes all 104 slots of a matrix table (cdecl). Here AtMatrixTable has only the 39 members some code reads (include/at/AtCoordSys.h), in
// another order than the original's, and those are what this writes; the 65 slots no code reads are not in the host's table.
void AtMatrixTableFill(AtMatrixTable* table);   // 0x000EC2B0
// 0x000EC670: both tables, the matrix table and the render table's matrix part, then the render table's seven functions of its own.
void AtGraphicsFillTables();                     // 0x000EC670
// The render table's seven functions (0x00117130, 0x001171D0, 0x001CC9F0, 0x00117250, 0x00117380, 0x00117470, 0x001175A0). Not recovered but the third,
// which is the empty function (folded); the others stop with a diagnostic (src/at/AtGraphicsFactorySeams.cpp). What they take is not known.
void AtRenderFunction0();                        // 0x00117130
void AtRenderFunction1();                        // 0x001171D0
void AtRenderFunction2();                        // 0x001CC9F0 empty
void AtRenderFunction3();                        // 0x00117250
void AtRenderFunction4();                        // 0x00117380
void AtRenderFunction5();                        // 0x00117470
void AtRenderFunction6();                        // 0x001175A0

// ---- the modes and capabilities of one device type of an adapter -------------------------------------------------------------

class AtGfxAdapter;

// Base vftable 0x0028B1B8 (slot 0 the deleting destructor, 1-3 pure), 0x6C bytes: fifteen words of capability flags and two texts.
class AtGfxDeviceInfoBase {
public:
    AtGfxDeviceInfoBase();             // 0x0011C950
    virtual ~AtGfxDeviceInfoBase();    // slot 0, 0x0011C9C0 deleting; complete 0x0011C8E0. Releases both texts
    virtual int Slot1(int unused) = 0;
    virtual unsigned int Slot2(int unused) = 0;
    virtual int Slot3(int unused) = 0;
    void Reset();                      // 0x0011C900 the words cleared, the texts released

    AtGfxAdapter* m_adapter;           // +0x04 the adapter enumerated for; 0 until then
    unsigned int m_deviceFlags;        // +0x08 from DevCaps and the shader versions; 0x200 always once read
    unsigned int m_stencilFlags;       // +0x0C
    unsigned int m_textureOpFlags;     // +0x10
    unsigned int m_vertexFlags;        // +0x14
    unsigned int m_rasterFlags;        // +0x18 raster, shading, texture, filter and address caps (AtGfxCapsToFlags)
    unsigned int m_zCmpFlags;          // +0x1C
    unsigned int m_alphaCmpFlags;      // +0x20
    unsigned int m_srcBlendFlags;      // +0x24
    unsigned int m_destBlendFlags;     // +0x28
    unsigned int m_rasterFlags2;       // +0x2C the same as +0x18, again
    unsigned int m_zCmpFlags2;         // +0x30 the same as +0x1C
    unsigned int m_alphaCmpFlags2;     // +0x34
    unsigned int m_srcBlendFlags2;     // +0x38
    unsigned int m_destBlendFlags2;    // +0x3C
    AtTextUnicode m_text40;            // +0x40 not written here
    AtTextUnicode m_text54;            // +0x54 not written here
    int m_unknown68;                   // +0x68
};

// Vftable 0x00289698, 0x14C bytes; three in each adapter, of which start-up enumerates the first, for device type 1.
class AtGfxDeviceInfo : public AtGfxDeviceInfoBase {
public:
    AtGfxDeviceInfo();                 // 0x000FBD40
    ~AtGfxDeviceInfo() override;       // slot 0, 0x000FC530 deleting; complete 0x000FC290
    int Slot1(int unused) override;    // slot 1, 0x000FBA70 1
    unsigned int Slot2(int unused) override;  // slot 2, 0x000FBA80 caps +0x58, MaxTextureWidth
    int Slot3(int unused) override;    // slot 3, 0x000FBA70 1
    virtual unsigned int Slot4(int unused);   // slot 4, 0x000FBA90 caps +0x5C, MaxTextureHeight
    void Clear();                      // 0x000FBD80
    // 0x000FC320. Asks Direct3D for the device's caps; if that fails, nothing more (an earlier enumeration stays cleared). Otherwise the modes: each that
    // the device takes as display and back buffer format, windowed off, and whose format is known, kept in order; then ReadCaps.
    void Enumerate(AtGfxAdapter* adapter, unsigned int deviceType);
    void ReadCaps();                   // 0x000FBF60 the caps as the engine's flags

    AtD3DCaps m_caps;                  // +0x6C
    unsigned int m_deviceType;         // +0x140 1 from new
    unsigned int m_modeCount;          // +0x144
    AtDisplayMode* m_modes;            // +0x148 from 0x003B1FB4's heap
};

void AtGfxCapsToFlags(const AtD3DCaps* caps, unsigned int* flags);  // 0x000FBBE0 (ecx caps, eax flags in the original: register arguments)

// ---- an adapter ----------------------------------------------------------------------------------------------------------------

// Base vftable 0x0028B194 (slot 0 releases the texts, slot 1 the deleting destructor, 2-3 pure), 0x2C bytes.
class AtGfxAdapterBase {
public:
    AtGfxAdapterBase();                // 0x0011C850
    virtual void ReleaseTexts();       // slot 0, 0x0011C880
    virtual ~AtGfxAdapterBase();       // slot 1, 0x0011C8C0 deleting; complete 0x0011C870, which only puts the vftable back
    virtual int HasDeviceInfo1() = 0;  // slot 2
    virtual int HasDeviceInfo2() = 0;  // slot 3

    AtTextUnicode m_driverText;        // +0x04 over m_driverName
    AtTextUnicode m_descriptionText;   // +0x18 over m_descriptionName
};

// Vftable 0x0028943C, 0x84C bytes.
class AtGfxAdapter : public AtGfxAdapterBase {
public:
    AtGfxAdapter();                    // 0x000FA9A0
    // The base's slot 0 is overridden by Clear, 0x000FABB0: the names freed, the device infos cleared, the words and the identifier zeroed. The texts are not
    // released, and keep pointing at the freed names.
    void ReleaseTexts() override;
    ~AtGfxAdapter() override;          // slot 1, 0x000FA520 (vector) deleting; complete 0x000FACB0
    int HasDeviceInfo1() override;     // slot 2, 0x000FAC20 m_info[0] enumerated
    int HasDeviceInfo2() override;     // slot 3, 0x000FAC30 m_info[1]
    virtual int HasDeviceInfo3();      // slot 4, 0x000FAC40 m_info[2]
    // 0x000FAA30. Clears first if it was set up; asks Direct3D for the adapter's identifier, widens its driver and description into new names (from
    // 0x003B1FAC's heap) under the two texts, and enumerates m_info[0] for device type 1.
    void Init(void* d3d, unsigned int ordinal);

    void* m_d3d;                       // +0x02C what Direct3DCreate8 gave
    unsigned int m_ordinal;            // +0x030
    AtD3DAdapterIdentifier m_identifier;  // +0x034
    AtGfxDeviceInfo m_info[3];         // +0x460
    unsigned short* m_driverName;      // +0x844
    unsigned short* m_descriptionName; // +0x848
};

// ---- the list of adapters, the factory's +4 ------------------------------------------------------------------------------------

// Base vftable 0x00289414 (slot 0 the deleting destructor 0x000FA500, 1-4 pure), 4 bytes.
class AtGfxAdapterListBase {
public:
    virtual ~AtGfxAdapterListBase();   // slot 0; complete 0x0011C840
    virtual unsigned int Count() = 0;
    virtual AtGfxAdapter* At(unsigned int index) = 0;
    virtual AtGfxAdapter* Best() = 0;
    virtual AtGfxAdapter* Find(const AtD3DAdapterIdentifier* identifier) = 0;
};

// Vftable 0x00289428, 0x10 bytes.
class AtGfxAdapterList : public AtGfxAdapterListBase {
public:
    AtGfxAdapterList();                // 0x000FA5A0
    ~AtGfxAdapterList() override;      // slot 0, 0x000FA910 deleting; complete 0x000FA770
    unsigned int Count() override;     // slot 1, 0x00154540 (folded) m_count
    AtGfxAdapter* At(unsigned int index) override;  // slot 2, 0x000FA580 no bound
    AtGfxAdapter* Best() override;     // slot 3, 0x000FA600 not recovered: stops
    AtGfxAdapter* Find(const AtD3DAdapterIdentifier* identifier) override;  // slot 4, 0x000FA700 not recovered: stops
    void Clear();                      // 0x000FA5C0
    // 0x000FA7F0. Clears what there was, makes Direct3D's interface, then one adapter (the count is 1, whatever Direct3D has) from 0x003B1FA8's heap,
    // and sets each up.
    void Init();

    void* m_d3d;                       // +0x04
    unsigned int m_count;              // +0x08
    AtGfxAdapter* m_adapters;          // +0x0C new[], with its count before it
};

// ---- the factory ---------------------------------------------------------------------------------------------------------------

// The static object at 0x003ADF64 (vftable 0x00288BE0, 34 slots, 0x14 bytes), App+0x5F90 (App's AppGraphics). Its constructor runs before main
// (0x00201AD0) and its destructor at exit (0x00203EB0). Slots 0, 3 and 4 are recovered; the others stop (src/at/AtGraphicsLayerSeams.cpp). The odd
// slots from 4 to 26 are one function, 0x000EB9D0. Slots 30-33 (0x000EBBF0, 0x000EC0B0, 0x000EBD50, 0x000EBC10) are not declared: nothing here calls them.
class AtGraphicsFactory {
public:
    AtGraphicsFactory();             // 0x000EBD70
    ~AtGraphicsFactory();            // 0x000EBD90 not virtual: no slot of the vftable is a destructor
    void Start();                    // 0x000EB520 the adapters' Init
    void Stop();                     // 0x000EB530 the adapters' Clear

    virtual void* Slot0();           // 0x000EB540 &m_adapters (App+0x5F94)
    virtual void Slot1();
    virtual void Slot2();
    virtual AtMaterialOwner* CreateDevice();  // slot 3, 0x000EBE90 the graphics device, 0x305C0 bytes from 0x003B13CC's heap, its creator this
    // Slot 4, 0x000EB9D0: calls the object's slot 0, its deleting destructor, with 1. In the original every odd slot from 4 to 26 is this, each handed
    // another class; here it is handed the device, whose slot 0 is AtDisplayDevice::Slot0.
    virtual void Destroy(void* object);
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void Slot21();
    virtual void Slot22();
    virtual void Slot23();
    virtual void Slot24();
    virtual void Slot25();
    virtual void Slot26();
    virtual class AtCamera* Camera2D();  // slot 27: the camera the fatal error screen draws with (0x0008E2FB, at/fatal)
    virtual void Slot28();
    virtual AtDatabaseLoader* DatabaseLoader();  // slot 29, 0x000EBB50
    virtual void DeleteLoader(AtDatabaseLoader* loader);  // slot 30, 0x000EBBF0 frees what slot 29 made (the level manifest's graphics items, gm/dbmanifest)

    AtGfxAdapterList m_adapters;     // +0x04
};
extern AtGraphicsFactory g_atGraphicsFactory;  // 0x003ADF64

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtD3DAdapterIdentifier) == 0x42C, "AtD3DAdapterIdentifier");
static_assert(sizeof(AtD3DCaps) == 0xD4, "AtD3DCaps");
static_assert(offsetof(AtD3DCaps, m_extentsAdjust) == 0x84, "AtD3DCaps::m_extentsAdjust");
static_assert(offsetof(AtD3DCaps, m_vertexShaderVersion) == 0xC4, "AtD3DCaps::m_vertexShaderVersion");
static_assert(sizeof(AtD3DDisplayMode) == 0x14, "AtD3DDisplayMode");
static_assert(offsetof(AtGfxDeviceInfoBase, m_text40) == 0x40, "AtGfxDeviceInfoBase::m_text40");
static_assert(offsetof(AtGfxDeviceInfoBase, m_unknown68) == 0x68, "AtGfxDeviceInfoBase::m_unknown68");
static_assert(offsetof(AtGfxDeviceInfo, m_caps) == 0x6C, "AtGfxDeviceInfo::m_caps");
static_assert(offsetof(AtGfxDeviceInfo, m_deviceType) == 0x140, "AtGfxDeviceInfo::m_deviceType");
static_assert(sizeof(AtGfxDeviceInfo) == 0x14C, "AtGfxDeviceInfo");
static_assert(sizeof(AtGfxAdapterBase) == 0x2C, "AtGfxAdapterBase");
static_assert(offsetof(AtGfxAdapter, m_identifier) == 0x34, "AtGfxAdapter::m_identifier");
static_assert(offsetof(AtGfxAdapter, m_info) == 0x460, "AtGfxAdapter::m_info");
static_assert(offsetof(AtGfxAdapter, m_driverName) == 0x844, "AtGfxAdapter::m_driverName");
static_assert(sizeof(AtGfxAdapter) == 0x84C, "AtGfxAdapter");
static_assert(sizeof(AtGfxAdapterList) == 0x10, "AtGfxAdapterList");
static_assert(sizeof(AtGraphicsFactory) == 0x14, "AtGraphicsFactory");
#pragma pop_macro("offsetof")
#endif
