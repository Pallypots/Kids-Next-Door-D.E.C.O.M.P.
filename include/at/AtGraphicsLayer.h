// The graphics layer's object, the AtAp layer above kinematics, and what its Init brings up short of the hardware: the saved globals it keeps on the
// stack (the memory globals, then the matrix function table and the render function table), and the heaps of the classes the graphics code makes.
// Subsystem at/grlayer. Reference build knd-xbox-usa-5454008a-v1.
//
// As for the layers below (include/at/AtApLayer.h, include/at/AtApMidLayers.h), RTTI names sixteen AtAp*Application classes but none of these objects;
// every class and member name here is inferred, from what the objects do and from RTTI's order of the layers. The layer's slot 5 reports "Atlas
// Application: unable to set display", which names the display mode at +0x54.
//
// Hardware stays behind two seams, each stopping with a diagnostic (src/at/AtGraphicsLayerSeams.cpp): AtGraphicsStartFactory 0x000C7C60, which writes
// the render function tables (0x000EC670) and starts the graphics factory, a static object at 0x003ADF64 (vftable 0x00288BE0); and its stop, 0x000C7C90.
// The factory's slot 3 makes the graphics device (constructor 0x000F1FA0, vftable 0x00289208), which is App's material owner: not recovered here.
#pragma once
#include "at/AtApLayer.h"
#include "at/AtCoordSys.h"
#include "at/AtPixelFormat.h"

class AtDatabaseLoader;
struct AtMaterialOwner;

// ---- the function tables and the saved globals ----------------------------------------------------------------------------

// The render function table at 0x002C7C48 (0x1BC bytes): the matrix table's 0x1A0 bytes, then seven more functions (0x002C7DE8-0x002C7E00) that
// 0x000EC670 sets. Zero until the graphics factory starts. Here the first part is AtMatrixTable, of which only the members other code uses are known.
struct AtRenderTable {
    AtMatrixTable m_matrix;          // +0x000 in the original, 0x1A0 bytes
    void* m_extended[7];             // +0x1A0
};
extern AtRenderTable g_atRender;     // 0x002C7C48

AtMatrixTable* AtMatrixTableGet();   // 0x000140C0 the live table, not a copy
void AtMatrixTableSet(const AtMatrixTable* table);   // 0x000140A0 copies a whole table into the live one
AtRenderTable* AtRenderTableGet();   // 0x000455D0 the live table
void AtRenderTableSet(const AtRenderTable* table);   // 0x000455B0 the matrix part into the matrix table, then the whole into the render table

// What the layers from math up keep on the stack while they work, each adding to AtIoGlobals (include/at/AtApLayer.h). Save and Restore run the base's,
// then their own. The math layer's saves where the matrix table is, the graphics layer's where the render table is; Restore copies each table from where
// it was saved, which is the live table itself. Two levels between add nothing: their names are the weakest, from their place in the chain only (the
// second lies beside the kinematics layer's heap setters, 0x00100FB0).
struct AtMathGlobals : AtIoGlobals {
    AtMathGlobals();                 // 0x00125000
    void Save();                     // 0x00125020
    void Restore();                  // 0x00125040
    int m_unknown10;                 // +0x10 never written here
    AtMatrixTable* m_matrix;         // +0x14
};
struct AtDataGlobals : AtMathGlobals {
    AtDataGlobals();                 // 0x0011E370
    void Save();                     // 0x0011E380
    void Restore();                  // 0x0011E390
};
struct AtKinematicsGlobals : AtDataGlobals {
    AtKinematicsGlobals();           // 0x00100F80
    void Save();                     // 0x00100F90
    void Restore();                  // 0x00100FA0
};
struct AtGraphicsGlobals : AtKinematicsGlobals {
    AtGraphicsGlobals();             // 0x000C8600
    void Save();                     // 0x000C8620
    void Restore();                  // 0x000C8640
    AtRenderTable* m_render;         // +0x18
};

// ---- the config ---------------------------------------------------------------------------------------------------------

// What App's slot 35 fills in (0xF0 bytes). Each heap that is 0 falls back to m_heap. Where the class a heap serves is recovered it is named; otherwise
// the field is named by its offset, and the comment gives the setters it goes to (src/at/AtGraphicsLayer.cpp) and the global each sets. The names
// m_unknown00 (the kind), m_unknown04 and m_heap are App's, from before.
struct AtGraphicsConfig {
    int m_unknown00;                 // +0x00 the kind: 0 brings nothing up, 1 is taken as 2 (Init writes the 2 back)
    int m_unknown04;                 // +0x04 copied to the layer's +0x28
    AtHeap* m_heap;                  // +0x08
    void* m_heap0C;                  // +0x0C 0x000FCE60 (0x003B1FC0)
    void* m_heap10;                  // +0x10 0x000FCD20 (0x003B1FBC)
    void* m_displayModeHeap;         // +0x14 0x00043760 (0x002C7C38), what the display mode code allocates from
    void* m_loaderHeap;              // +0x18 AtDatabaseLoader's
    void* m_heap1C;                  // +0x1C 0x000FBA60 (0x003B1FB4)
    void* m_heap20;                  // +0x20 0x000FAD30 (0x003B1FB0)
    void* m_heap24;                  // +0x24 0x000FA930 (0x003B1FAC)
    void* m_heap28;                  // +0x28 0x000FA4F0 (0x003B1FA8), read by the factory's start (0x000FA7F0)
    void* m_heap2C;                  // +0x2C 0x000F98E0 (0x003B1FA4)
    void* m_heap30;                  // +0x30 0x00100DB0 (0x003B23DC) and 0x000EB510 (0x003B13CC, the factory's, for the device its slot 3 makes)
    void* m_heap34;                  // +0x34 0x000FE870 (0x003B1FCC) and 0x00100CF0 (0x003B23D8)
    void* m_fontHeap;                // +0x38 AtFontDatabase's
    void* m_heap3C;                  // +0x3C 0x00057DB0 (0x002C7FF0)
    void* m_imageHeap;               // +0x40 AtImageDatabase's
    void* m_heap44;                  // +0x44 0x000F7ED0 (0x003B1F98)
    void* m_heap48;                  // +0x48 0x00028CD0 (0x002C635C)
    void* m_heap4C;                  // +0x4C 0x0002AAF0 (0x002C6378)
    void* m_matAnimationHeap;        // +0x50 AtMatAnimationDatabase's
    void* m_matAssignmentHeap;       // +0x54 AtMatAssignment's
    void* m_matAssignmentDbHeap;     // +0x58 AtMatAssignmentDatabase's
    void* m_materialHeap;            // +0x5C AtMaterialDatabase's, and 0x000F7780 (0x003B1F90)
    void* m_heap60;                  // +0x60 0x000F61C0 (0x003B1F70)
    void* m_heap64;                  // +0x64 0x000FE500 (0x003B1FC4)
    void* m_heap68;                  // +0x68 0x000F7BD0 (0x003B1F94)
    void* m_heap6C;                  // +0x6C 0x000F5FC0 (0x003B1F58)
    void* m_heap70;                  // +0x70 0x00036AB0 (0x002C63E8) and 0x00100350 (0x003B23D4)
    void* m_heap74;                  // +0x74 0x0002AD50 (0x002C637C)
    void* m_heap78;                  // +0x78 0x000283E0 (0x002C6358)
    void* m_heap7C;                  // +0x7C 0x000F58D0 (0x003B1F54)
    void* m_heap80;                  // +0x80 0x00060590 (0x002CB08C)
    void* m_heap84;                  // +0x84 0x00065B00 (0x002CB090)
    void* m_heap88;                  // +0x88 0x000F93C0 (0x003B1FA0)
    void* m_heap8C;                  // +0x8C 0x0002B420 (0x002C6384)
    void* m_progShaderHeap;          // +0x90 AtProgShaderComponent's
    void* m_heap94;                  // +0x94 0x000F5610 (0x003B1F50)
    void* m_heap98;                  // +0x98 0x000EEF90 (0x003B13E0)
    void* m_heap9C;                  // +0x9C 0x00100340 (0x003B23D0)
    void* m_shaderHeap;              // +0xA0 AtShader's
    void* m_shaderDbHeap;            // +0xA4 AtShaderDatabase's
    void* m_heapA8;                  // +0xA8 0x000EE550 (0x003B13DC)
    void* m_heapAC;                  // +0xAC 0x000ED5E0 (0x003B13D8)
    void* m_heapB0;                  // +0xB0 0x000FC550 (0x003B1FB8)
    void* m_textureHeap;             // +0xB4 AtTextureDatabase's
    void* m_heapB8;                  // +0xB8 0x00059580 (0x002C9C08)
    void* m_heapBC;                  // +0xBC 0x000ED2B0 (0x003B13D4)
    void* m_heapC0;                  // +0xC0 0x000EC6D0 (0x003B13D0)
    void* m_heapC4;                  // +0xC4 0x000F8500 (0x003B1F9C)
    void* m_heapC8;                  // +0xC8 0x0002B680 (0x002C6388)
    void* m_nameMapHeap;             // +0xCC GmNameMap's, unless already set
    void* m_nameSpaceHeap;           // +0xD0 GmNameSpaceDb's, unless already set
    int m_unknownD4[2];              // +0xD4 not read
    int m_unknownDC;                 // +0xDC copied to the layer's +0x34
    int m_unknownE0[2];              // +0xE0 not read
    int m_unknownE8;                 // +0xE8 copied to the layer's +0x40 once the factory has started
    int m_unknownEC;                 // +0xEC copied to the layer's +0x48
};

// The heap setters only the graphics layer calls, by the global each sets; the classes they serve are not identified. Each stores its argument.
void AtSetHeap3B23DC(void* heap);    // 0x00100DB0
void AtSetHeap2C7C38(void* heap);    // 0x00043760 read by 0x00043840, the display mode list's allocation
void AtSetHeap3B23D8(void* heap);    // 0x00100CF0
void AtSetHeap2C7FF0(void* heap);    // 0x00057DB0
void AtSetHeap2C635C(void* heap);    // 0x00028CD0
void AtSetHeap2C63E8(void* heap);    // 0x00036AB0
void AtSetHeap3B23D4(void* heap);    // 0x00100350
void AtSetHeap3B1FC4(void* heap);    // 0x000FE500
void AtSetHeap2C6358(void* heap);    // 0x000283E0
void AtSetHeap2CB08C(void* heap);    // 0x00060590
void AtSetHeap2CB090(void* heap);    // 0x00065B00
void AtSetHeap2C9C08(void* heap);    // 0x00059580
void AtSetHeap3B13D4(void* heap);    // 0x000ED2B0
void AtSetHeap2C6378(void* heap);    // 0x0002AAF0
void AtSetHeap2C637C(void* heap);    // 0x0002AD50
void AtSetHeap3B1F50(void* heap);    // 0x000F5610
void AtSetHeap3B23D0(void* heap);    // 0x00100340
void AtSetHeap3B1FA0(void* heap);    // 0x000F93C0
void AtSetHeap3B1F9C(void* heap);    // 0x000F8500
void AtSetHeap2C6384(void* heap);    // 0x0002B420
void AtSetHeap2C6388(void* heap);    // 0x0002B680
void AtSetHeap3B1FC0(void* heap);    // 0x000FCE60
void AtSetHeap3B1FBC(void* heap);    // 0x000FCD20
void AtSetHeap3B1FB8(void* heap);    // 0x000FC550
void AtSetHeap3B1FB4(void* heap);    // 0x000FBA60
void AtSetHeap3B1FB0(void* heap);    // 0x000FAD30
void AtSetHeap3B1FAC(void* heap);    // 0x000FA930
void AtSetHeap3B1FA8(void* heap);    // 0x000FA4F0
void AtSetHeap3B1FA4(void* heap);    // 0x000F98E0
void AtSetHeap3B13CC(void* heap);    // 0x000EB510 the graphics factory's
void AtSetHeap3B1F98(void* heap);    // 0x000F7ED0
// 0x000F7780 (0x003B1F90) is AtGrD3DMaterialDatabase::SetHeap (at/xdb, include/at/AtGrD3DTextures.h).
void AtSetHeap3B1F70(void* heap);    // 0x000F61C0
void AtSetHeap3B1F54(void* heap);    // 0x000F58D0
void AtSetHeap3B13E0(void* heap);    // 0x000EEF90
void AtSetHeap3B13DC(void* heap);    // 0x000EE550
void AtSetHeap3B13D8(void* heap);    // 0x000ED5E0
void AtSetHeap3B13D0(void* heap);    // 0x000EC6D0
extern void* g_heap3B23DC;           // 0x003B23DC
extern void* g_heap2C7C38;           // 0x002C7C38
extern void* g_heap3B23D8;           // 0x003B23D8
extern void* g_heap2C7FF0;           // 0x002C7FF0
extern void* g_heap2C635C;           // 0x002C635C
extern void* g_heap2C63E8;           // 0x002C63E8
extern void* g_heap3B23D4;           // 0x003B23D4
extern void* g_heap3B1FC4;           // 0x003B1FC4
extern void* g_heap2C6358;           // 0x002C6358
extern void* g_heap2CB08C;           // 0x002CB08C
extern void* g_heap2CB090;           // 0x002CB090
extern void* g_heap2C9C08;           // 0x002C9C08
extern void* g_heap3B13D4;           // 0x003B13D4
extern void* g_heap2C6378;           // 0x002C6378
extern void* g_heap2C637C;           // 0x002C637C
extern void* g_heap3B1F50;           // 0x003B1F50
extern void* g_heap3B23D0;           // 0x003B23D0
extern void* g_heap3B1FA0;           // 0x003B1FA0
extern void* g_heap3B1F9C;           // 0x003B1F9C
extern void* g_heap2C6384;           // 0x002C6384
extern void* g_heap2C6388;           // 0x002C6388
extern void* g_heap3B1FC0;           // 0x003B1FC0
extern void* g_heap3B1FBC;           // 0x003B1FBC
extern void* g_heap3B1FB8;           // 0x003B1FB8
extern void* g_heap3B1FB4;           // 0x003B1FB4
extern void* g_heap3B1FB0;           // 0x003B1FB0
extern void* g_heap3B1FAC;           // 0x003B1FAC
extern void* g_heap3B1FA8;           // 0x003B1FA8
extern void* g_heap3B1FA4;           // 0x003B1FA4
extern void* g_heap3B13CC;           // 0x003B13CC
extern void* g_heap3B1F98;           // 0x003B1F98
extern void* g_heap3B1F70;           // 0x003B1F70
extern void* g_heap3B1F54;           // 0x003B1F54
extern void* g_heap3B13E0;           // 0x003B13E0
extern void* g_heap3B13DC;           // 0x003B13DC
extern void* g_heap3B13D8;           // 0x003B13D8
extern void* g_heap3B13D0;           // 0x003B13D0

// Three setters the layer calls are recovered on another branch (integrate-batch6: AtGrI2DFont::SetHeap 0x000FE870, AtMatrixDeviceXbox::SetHeap
// 0x000F7BD0, AtMatrixPoolXbox::SetHeap 0x000F5FC0). Here they are stand-ins with the same effect, not recorded and not counted
// (src/at/AtGraphicsLayerStandIns.cpp): at integration, call those and delete the stand-ins.
// 0x000FE870, 0x000F7BD0 and 0x000F5FC0 are AtGrI2DFont::SetHeap, AtMatrixDeviceXbox::SetHeap and AtMatrixPoolXbox::SetHeap (at/imgfont, at/matdev).

// The layer's own heaps beyond those its base sets: 0x000C7CA0, a free function (cdecl), the second half of the layer's SetHeaps.
void AtGraphicsSetHeaps(const AtGraphicsConfig* config);  // 0x000C7CA0

// ---- the graphics factory, a seam ---------------------------------------------------------------------------------------

// The static object at 0x003ADF64 (vftable 0x00288BE0, 34 slots), App+0x5F90 (App's AppGraphics). Not recovered: every slot stops with a diagnostic.
// The odd slots from 4 to 26 are one function, 0x000EB9D0, which deletes what it is handed through its slot 1.
class AtGraphicsFactory {
public:
    virtual void* Slot0();           // 0x000EB540 the object at +4 of the factory (App+0x5F94)
    virtual void Slot1();
    virtual void Slot2();
    virtual AtMaterialOwner* CreateDevice();  // slot 3, 0x000EBE90 makes the graphics device, 0x305C0 bytes from 0x003B13CC's heap; App's material owner
    virtual void Destroy(void* object);       // slot 4, 0x000EB9D0
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
    virtual void Slot27();
    virtual void Slot28();
    virtual AtDatabaseLoader* DatabaseLoader();  // slot 29, 0x000EBB50
    virtual void DeleteLoader(AtDatabaseLoader* loader);  // slot 30, 0x000EBBF0 frees what slot 29 made (the level manifest's graphics items, gm/dbmanifest)
};

// Not recovered (hardware); each stops with a diagnostic (src/at/AtGraphicsLayerSeams.cpp). Start: restores the saved globals (so the render table's
// matrix part, zero until now, is copied over the matrix table), writes both function tables (0x000EC670), starts the factory's device list
// (0x000EB520, 0x000FA7F0, from 0x003B1FA8's heap), saves the globals again and returns the factory. config is handed on and not read.
AtGraphicsFactory* AtGraphicsStartFactory(AtGraphicsGlobals* saved, const AtGraphicsConfig* config);  // 0x000C7C60
void AtGraphicsStopFactory();        // 0x000C7C90 the factory's device list, 0x000FA5C0

// ---- the layer's object ---------------------------------------------------------------------------------------------------

// The display mode at +0x54 of the layer (0x24 bytes): two words, a pixel format and two words more. Its constructor alone is recovered; the layer's slot
// 4 copies a mode in (0x00043450). The name is inferred from slot 5's report.
struct AtDisplayMode {
    AtDisplayMode();                 // 0x00043480 all zero
    int m_unknown00;                 // +0x00
    int m_unknown04;                 // +0x04
    AtPixelFormat m_format;          // +0x08
    int m_unknown1C;                 // +0x1C
    int m_unknown20;                 // +0x20
};

// vftable 0x00286608 (0x30 bytes).
class AtGraphicsSystemBase {
public:
    enum Flags { kRunning = 1, kDisplaySet = 2, kNoPresent = 4, kQuit = 8 };  // 1 and 2 from the layer, 4 and 8 from App's use (include/gm/GmApp.h)

    AtGraphicsSystemBase();                               // 0x000C81B0
    virtual void DeletingDestructor(unsigned int flags);  // slot 0, 0x000C85B0 not recovered
    virtual void Init(AtGraphicsConfig* config, void* below, void* memory);  // slot 1, 0x000C81E0
    virtual void Shutdown();                              // slot 2, 0x000C8210 clears all but the vftable
    virtual void SetHeaps(const AtGraphicsConfig* config);  // slot 3, 0x000C8240
    virtual void SetDisplay(AtDisplayMode* mode, int a, int b, int c, const AtPixelFormat* format, int d, int e);  // slot 4: pure here (0x00125F25)
    virtual void UpdateDisplay() {}                       // slot 5, 0x001CC9F0 empty (folded)
    virtual int RunFrame() { return 1; }                  // slot 6, 0x001A2B40 (folded): the loop goes on
    virtual int Slot7() { return 0; }                     // slot 7, 0x000AB720 (folded)

    int m_unknown04;                 // +0x04
    int m_kind;                      // +0x08 the config's kind; 2 once the derived layer brings the factory up
    unsigned int m_flags;            // +0x0C App+0x5F88
    void* m_below;                   // +0x10 the kinematics layer's object
    AtGraphicsFactory* m_factory;    // +0x14 App+0x5F90
    void* m_unknown18;               // +0x18 App+0x5F94, the factory's slot 0
    AtMaterialOwner* m_materialOwner;  // +0x1C App+0x5F98, the factory's slot 3
    int m_unknown20;                 // +0x20
    int m_unknown24;                 // +0x24 slot 5 tests it for 0 and 2
    int m_unknown28;                 // +0x28 config +0x04
    int m_unknown2C;                 // +0x2C
};

// The graphics layer's object, at +0x5F7C of App (vftable 0x0028478C; App's constructor writes 0x0028DC48 over it, which differs in the destructor).
class AtGraphicsSystem : public AtGraphicsSystemBase {
public:
    AtGraphicsSystem();                                   // 0x000B2F80
    void DeletingDestructor(unsigned int flags) override; // slot 0, 0x000B3490 not recovered
    void Init(AtGraphicsConfig* config, void* below, void* memory) override;  // slot 1, 0x000B3000
    void Shutdown() override;                             // slot 2, 0x000B30B0
    void SetHeaps(const AtGraphicsConfig* config) override;  // slot 3, 0x000B2F60
    void SetDisplay(AtDisplayMode* mode, int a, int b, int c, const AtPixelFormat* format, int d, int e) override;  // slot 4, 0x000B32D0 not recovered
    void UpdateDisplay() override;                        // slot 5, 0x000B3350 not recovered: "unable to set display"
    int Slot7() override;                                 // slot 7, 0x000B2F50 +0x38, or +0x3C if that is 0

    int m_unknown30;                 // +0x30 not written by either constructor
    int m_unknown34;                 // +0x34 config +0xDC
    int m_unknown38;                 // +0x38
    int m_unknown3C;                 // +0x3C
    int m_unknown40;                 // +0x40 config +0xE8
    int m_unknown44;                 // +0x44 slot 5's guard
    int m_unknown48;                 // +0x48 config +0xEC
    int m_unknown4C;                 // +0x4C set by slot 4
    int m_unknown50;                 // +0x50 set by slot 4
    AtDisplayMode m_mode;            // +0x54
    AtPixelFormat m_format;          // +0x78
    int m_unknown8C;                 // +0x8C set by slot 4
    int m_unknown90;                 // +0x90
    int m_unknown94;                 // +0x94
    int m_unknown98[3];              // +0x98 not written here
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtMathGlobals) == 0x18, "AtMathGlobals");
static_assert(sizeof(AtGraphicsGlobals) == 0x1C, "AtGraphicsGlobals");
static_assert(sizeof(AtGraphicsConfig) == 0xF0, "AtGraphicsConfig");
static_assert(offsetof(AtGraphicsConfig, m_nameSpaceHeap) == 0xD0, "AtGraphicsConfig::m_nameSpaceHeap");
static_assert(offsetof(AtGraphicsConfig, m_unknownDC) == 0xDC, "AtGraphicsConfig::m_unknownDC");
static_assert(sizeof(AtDisplayMode) == 0x24, "AtDisplayMode");
static_assert(sizeof(AtGraphicsSystemBase) == 0x30, "AtGraphicsSystemBase");
static_assert(offsetof(AtGraphicsSystem, m_mode) == 0x54, "AtGraphicsSystem::m_mode");
static_assert(offsetof(AtGraphicsSystem, m_format) == 0x78, "AtGraphicsSystem::m_format");
static_assert(sizeof(AtGraphicsSystem) == 0xA4, "AtGraphicsSystem");
#pragma pop_macro("offsetof")
#endif
