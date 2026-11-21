// The concrete graphics device: vftable 0x00289208, 26 slots, 0x305C0 bytes, constructor 0x000F1FA0, made by 0x000EBE90 (slot 25 of the class at vftable 0x00288B88), which stores itself at +0x3A8.
// Its base is AtMaterialOwner (include/at/AtMaterial.h), and that one's AtDisplayDevice (include/at/AtDisplayDevice.h). Original address clusters 0x000EEFC0-0x000EF39D, 0x000F0120-0x000F02E5,
// 0x000F1FA0-0x000F226E and the members' constructors listed below. All names are inferred, from offsets, from what each does and from the callers; several member classes are named only by role.
//
// Recovered: the constructor and every member's set-up, Init, the slots that hand back members (13 to 18, 21, 23 to 25), slot 20, and the destructors of the members that only free memory. Not
// recovered: everything that makes Direct3D objects, renders or touches graphics memory (slots 1 to 12, 19, 22, and the destructors of the members that release graphics resources). Each stops with
// a diagnostic, and the device's own destructor stops at the first of them, slot 1, which it calls first: see src/at/AtGraphicsDevice.cpp.
//
// Three members are of classes the texture work recovers (the concrete texture database, vftable 0x0028A6E0, and two kinds of texture, 0x00289768 and 0x00289700): they are held here as storage
// of their size, made and destroyed through the six seams below, which src/at/AtGraphicsDeviceHost.cpp defines to stop. Replacing them is a matter of using those classes as the members' types and
// deleting the wrappers and that file.
#pragma once

#include "at/AtGfxDrawLists.h"
#include "at/AtGrD3DDatabase.h"
#include "at/AtMaterial.h"
#include "at/AtMatrixDevice.h"
#include "at/AtMeshDatabase.h"

class AtGraphicsDeviceXbox;

// ---- the texture work's classes: the device's texture members are xdb's AtGrD3DSurface2D (vftable 0x00289768, 0x000FCEE0), AtGrD3DTexture2D (0x00289700,
// 0x000FC630) and AtGrD3DTextureDatabase (0x0028A6E0, 0x0010FFC0), include/at/AtGrD3DTextures.h; at/gfxdev stood in for them with storage and six seams.

// What slot 20 is given: it calls the object's slot 1. 0x00080D90, folded with a function of the jam work.
class AtGfxObject {
public:
    virtual void Slot0() = 0;
    virtual void Release() = 0;  // slot 1
};

// ---- the context, +0x450 --------------------------------------------------------------------------------------------------
// Vftable 0x00277168, twelve slots, 0x2950 bytes; constructor 0x00059500, whose one caller is the context's. Slot 0 (0x00059550, deleting) is not recovered; slot 1 is pure.
class AtGfxContextBase {
public:
    // +0x30, 0x18 bytes: the constructor 0x00037E90 sets only +0x10 and +0x14.
    struct Part30 {
        Part30();                      // 0x00037E90 +0x10 0, +0x14 1.0f; the rest left
        int m_unknown00[4];
        int m_unknown10;
        float m_unknown14;
    };
    // +0x48, 0xEC bytes.
    struct Part48 {
        Part48();                      // 0x0005C650 all zero but +0xD0 5, +0xDC -1.0f, +0xE0 1.0f
        float m_unknown00[0x18];
        float m_unknown60[0x1B];
        int m_unknownCC;
        int m_unknownD0;
        int m_unknownD4;
        int m_unknownD8;
        float m_unknownDC;
        float m_unknownE0;
        int m_unknownE4;
        int m_unknownE8;
    };

    AtGfxContextBase();                // 0x00059500 the parts constructed, then ResetBase
    ~AtGfxContextBase();               // 0x00059540 this vftable put back, then ResetBase
    virtual void Slot0();
    virtual void Slot1();
    virtual void Reset();              // slot 2, 0x00059530 ResetBase
    // 0x00058BD0. Also clears the three words of s_shared and points +0x294C at them: every context shares one, and resetting any context clears it.
    void ResetBase();

    int m_unknown04[3];                // +0x04 left
    int m_unknown10;                   // +0x10 nonzero: the context is in its device's list (the context's destructor)
    int m_unknown14;                   // +0x14
    int m_unknown18;                   // +0x18
    int m_unknown1C;                   // +0x1C
    unsigned char m_color20[4];        // +0x20 0, 0, 0, 0xFF
    float m_unknown24;                 // +0x24 1.0f
    int m_unknown28;                   // +0x28
    int m_unknown2C;                   // +0x2C
    Part30 m_part30;                   // +0x30
    Part48 m_part48;                   // +0x48
    int m_unknown134;                  // +0x134
    unsigned char m_unknown138[0x2940 - 0x138];
    int m_unknown2940;                 // +0x2940
    int m_unknown2944;                 // +0x2944
    int m_unknown2948;                 // +0x2948
    int* m_shared;                     // +0x294C &s_shared

    static int s_shared[3];            // 0x002C9BF8
};

// Vftable 0x0028A080, forty slots, 0x2A80 bytes; constructor 0x00107F50 (callers 0x000EBD0A and the device's). None of its own slots is recovered.
class AtGfxContext : public AtGfxContextBase {
public:
    AtGfxContext();                    // 0x00107F50 the base, then ResetLocal
    ~AtGfxContext();                   // 0x001099B0 out of its device's list if +0x10 says it is in one; the base's Reset (not this class's slot 2); ResetLocal; the base's
    void Slot0() override;
    void Slot1() override;
    void Reset() override;             // slot 2, 0x00108000, not recovered
    void ResetLocal();                 // 0x00107B40 this class's words; +0x2968 to +0x297C and +0x2A4C are left

    AtGraphicsDeviceXbox* m_device;    // +0x2950
    int m_unknown2954;                 // +0x2954
    int m_unknown2958;                 // +0x2958
    int m_unknown295C;                 // +0x295C
    int m_unknown2960;                 // +0x2960
    int m_unknown2964;                 // +0x2964
    int m_unknown2968[6];              // +0x2968 left
    int m_unknown2980;                 // +0x2980
    int m_unknown2984[0x15];           // +0x2984
    int m_unknown29D8[0x15];           // +0x29D8
    int m_unknown2A2C;                 // +0x2A2C
    int m_unknown2A30[7];              // +0x2A30
    int m_unknown2A4C;                 // +0x2A4C left
    int m_unknown2A50;                 // +0x2A50
    int m_unknown2A54;                 // +0x2A54
    int m_unknown2A58;                 // +0x2A58
    int m_unknown2A5C;                 // +0x2A5C
    int m_unknown2A60;                 // +0x2A60 2
    int m_unknown2A64;                 // +0x2A64
    int m_unknown2A68;                 // +0x2A68
    int m_unknown2A6C;                 // +0x2A6C
    float m_unknown2A70;               // +0x2A70 1.0f
    float m_unknown2A74;               // +0x2A74 1.0f
    int m_unknown2A78;                 // +0x2A78
    AtGfxContext* m_next;              // +0x2A7C in the device's list at +0x3334
};

// ---- the state cache, +0x3354 ---------------------------------------------------------------------------------------------
// No vftable; 0xAA4 bytes. Everything is zero after Reset. 0x000F7FB0 (not recovered) shows what it is for: it keeps the last two values given and calls the graphics library only when they change.
class AtGfxStateCache {
public:
    AtGfxStateCache();                 // 0x000FB8C0 Reset
    ~AtGfxStateCache();                // 0x000FBA40 frees +0xA58, then Reset
    void Reset();                      // 0x000FB710 the words, then ResetState, then +0x9B0
    void ResetState();                 // 0x000FB380

    int m_unknown000[3];               // +0x000
    int m_unknown00C[0x160];           // +0x00C
    int m_unknown58C[8];               // +0x58C
    int m_unknown5AC[0x100];           // +0x5AC
    int m_unknown9AC;                  // +0x9AC
    int m_unknown9B0;                  // +0x9B0
    int m_unknown9B4[0x11];            // +0x9B4
    int m_unknown9F8;                  // +0x9F8
    int m_unknown9FC;                  // +0x9FC
    int m_unknownA00;                  // +0xA00
    int m_cachedA04;                   // +0xA04 nonzero: the two below are valid
    int m_cachedA08;                   // +0xA08
    int m_cachedA0C;                   // +0xA0C
    int m_unknownA10;                  // +0xA10
    int m_unknownA14[8];               // +0xA14
    int m_unknownA34[2];               // +0xA34
    int m_unknownA3C[2];               // +0xA3C
    int m_unknownA44[2];               // +0xA44
    int m_unknownA4C[2];               // +0xA4C
    int m_unknownA54;                  // +0xA54
    void* m_blockA58;                  // +0xA58 freed by the destructor; Reset clears it without freeing
    int m_unknownA5C;                  // +0xA5C
    int m_unknownA60;                  // +0xA60
    int m_unknownA64[0xC];             // +0xA64
    int m_unknownA94;                  // +0xA94
    int m_unknownA98;                  // +0xA98
    int m_unknownA9C;                  // +0xA9C
    int m_unknownAA0;                  // +0xAA0
};

// ---- the draw lists, +0x3E14: include/at/AtGfxDrawLists.h ------------------------------------------------------------

// ---- the concrete index array, and the buffer set holding two, +0x3E48 ---------------------------------------------------
// The concrete index array (vftable 0x00289358, 0x24 bytes) is loader's AtGrD3DIndexArray (include/at/AtGrD3DDatabase.h), whose constructor 0x000F8010 at/gfxdev
// recovered (as AtIndexArrayXbox): src/at/AtGraphicsDevice.cpp. The matrix device's slot 3 makes these.

// Vftable 0x002893DC, 33 slots, 0x8C bytes; constructor 0x000F99E0. Only the constructor is recovered; the destructor (0x000FA490) stops here.
class AtGfxBufferSet {
public:
    AtGfxBufferSet();                  // 0x000F99E0 the two arrays constructed, then the words cleared; +0x24 and +0x28 left
    ~AtGfxBufferSet();                 // 0x000FA490, not recovered: src/at/AtGraphicsDeviceHost.cpp
    virtual void Slot0();

    int m_unknown04;                   // +0x04
    int m_unknown08;                   // +0x08
    int m_unknown0C;                   // +0x0C
    int m_unknown10;                   // +0x10
    int m_unknown14;                   // +0x14
    int m_unknown18;                   // +0x18
    int m_unknown1C;                   // +0x1C
    int m_unknown20;                   // +0x20
    int m_unknown24;                   // +0x24 left
    int m_unknown28;                   // +0x28 left
    int m_unknown2C;                   // +0x2C
    AtGrD3DIndexArray m_indexArrays[2];  // +0x30
    int m_unknown78;                   // +0x78
    int m_unknown7C;                   // +0x7C
    int m_unknown80;                   // +0x80
    int m_unknown84;                   // +0x84
    int m_unknown88;                   // +0x88
};

// ---- the concrete mesh database, +0x3ED4: loader's AtGrD3DMeshDatabase (vftable 0x0028A730, constructor 0x001108B0), include/at/AtGrD3DDatabase.h. at/gfxdev
// recovered its constructor again as AtMeshDatabaseXbox; folded.

// ---- two lists of blocks, +0x1314C and +0x13170 ---------------------------------------------------------------------------
// No vftables. Each Clear frees what it holds and clears every word, and is also the destructor.
struct AtGfxBlockList {
    AtGfxBlockList();                  // 0x000FE520 all zero
    ~AtGfxBlockList() { Clear(); }
    void Clear();                      // 0x000FE540 the blocks at +0x10, +0x08, +0x00 freed in that order
    void* m_block00;                   // +0x00
    int m_unknown04;                   // +0x04
    void* m_block08;                   // +0x08
    int m_unknown0C;                   // +0x0C
    void* m_block10;                   // +0x10
};

struct AtGfxIndexedList : AtGfxBlockList {
    AtGfxIndexedList();                // 0x000ED2C0 the base, then its own words
    ~AtGfxIndexedList();               // 0x000ED470 Clear, then the base's (which clears again)
    void Clear();                      // 0x000ED2E0 the base's Clear, then the blocks at +0x1C and +0x14
    void* m_block14;                   // +0x14
    int m_unknown18;                   // +0x18
    void* m_block1C;                   // +0x1C
    int m_unknown20;                   // +0x20
};

struct AtGfxBlockPair {
    AtGfxBlockPair();                  // 0x0017AED0 all zero; folded with AtScenePlayback's constructor
    ~AtGfxBlockPair() { Clear(); }     // 0x000F56B0, the same function as Clear
    void Clear();                      // 0x000F56B0 the blocks at +0x08 and +0x00 freed in that order
    void* m_block00;
    int m_unknown04;
    void* m_block08;
    int m_unknown0C;
};

// ---- the 128 resources at +0x2D038 ----------------------------------------------------------------------------------------
// Three classes, each only constructed by the next: vftables 0x0028B3C0 (0x34 bytes), 0x00289A6C (0x40) and 0x0028AFC4 (0x50). Only the constructors are recovered. The last one's destructor
// (0x0011B040) releases a graphics resource held at +0x48 and stops here.
class AtGfxResource {
public:
    AtGfxResource();                   // 0x0011D570
    virtual void Slot0();
    int m_unknown04[4];                // +0x04
    int m_unknown14;                   // +0x14
    int m_unknown18;                   // +0x18
    int m_unknown1C;                   // +0x1C
    int m_unknown20;                   // +0x20
    unsigned short m_unknown24;        // +0x24
    unsigned short m_unknown26;        // +0x26
    unsigned short m_unknown28;        // +0x28 the last class's destructor tests 0x200 here
    unsigned short m_unknown2A;        // +0x2A
    unsigned short m_unknown2C;        // +0x2C
    unsigned short m_unknown2E;        // +0x2E 1
    int m_unknown30;                   // +0x30 stored twice by the original
};

class AtGfxResourceBlock : public AtGfxResource {
public:
    AtGfxResourceBlock();              // 0x001006E0
    int m_unknown34;                   // +0x34
    int m_unknown38;                   // +0x38
    int m_unknown3C;                   // +0x3C
};

class AtGfxResourceXbox : public AtGfxResourceBlock {
public:
    AtGfxResourceXbox();               // 0x0011AB30
    ~AtGfxResourceXbox();              // 0x0011B040, not recovered: src/at/AtGraphicsDeviceHost.cpp
    int m_unknown40;                   // +0x40
    int m_unknown44;                   // +0x44
    void* m_resource;                  // +0x48
    int m_unknown4C;                   // +0x4C left: the array's stride is 0x50
};

// A pool with an ordering of its items, +0x2CFE0 and +0x2D014.
struct AtGfxSortedPool {
    AtMatrixPoolXbox m_pool;           // +0x00
    unsigned char m_unknown0C[0x14];   // +0x0C
    int (*m_less)(const void* a, const void* b);  // +0x20 Init sets AtGfxLessAt10
};

// 0x000EF380. 1 when the float at +0x10 of a is below b's; 0 when not, equal, or either is a NaN.
int AtGfxLessAt10(const void* a, const void* b);

// ---- the device -----------------------------------------------------------------------------------------------------------
class AtGraphicsDeviceXbox : public AtMaterialOwner {
public:
    AtGraphicsDeviceXbox();            // 0x000F1FA0 the base, every member in order, +0x3A8 cleared, then Init
    // 0x000F5290. Slot 1 (Release) first, which is not recovered and stops; then the members in reverse order and the base's. Draft: not compared, since it cannot run past slot 1.
    ~AtGraphicsDeviceXbox();
    void Slot0() override;             // slot 0, 0x000F55F0 the deleting destructor; draft, as the destructor
    void Slot1() override;             // slot 1, 0x000F2750 Release, not recovered
    void Slot2() override;             // slot 2, 0x000F18B0 not recovered: calls the graphics library
    void Slot3() override;             // slot 3, 0x000EF350 not recovered: takes a byte in the original and calls slot 4 with it as a grey
    void Slot4() override;             // slot 4, 0x000F18C0 not recovered: opens the display with a mode
    void Slot5() override;             // slot 5, 0x000F44E0 not recovered: render state
    void Slot6() override;             // slot 6, 0x000F2270 not recovered
    void Slot7() override;             // slot 7, 0x000EFEC0 not recovered
    void Slot8() override;             // slot 8, 0x000F0010 not recovered
    void Present(int unknown) override;  // slot 9, 0x000F0310 not recovered: shows the frame (at/fatal's name)
    void Slot10() override;            // slot 10, 0x000F0350 not recovered
    void Slot11() override;            // slot 11, 0x000F14C0 not recovered
    void Slot12() override;            // slot 12, 0x000F1560 not recovered: turns off waiting for the vertical refresh
    void* RenderTarget0() override;    // slot 13, 0x000EEFC0 &+0x3B0
    void* RenderTarget1() override;    // slot 14, 0x000EEFD0 &+0x400
    AtGfxContext* Context() override;  // slot 15, 0x000EEFE0
    AtGfxDrawLists* DrawLists() override;  // slot 16, 0x000EF2D0
    AtMatrixDevice* MatrixDevice() override;  // slot 17, 0x000EF2E0
    AtFlashGraphicsFactory* GraphicsFactory() override;  // slot 18, 0x000EF2F0 m_creator
    void* Slot19(void* unknown) override;  // slot 19, 0x000EF300 not recovered: the mesh database's 0x0002B170, then slot 11 of what it gives
    void ReleaseObject(AtGfxObject* object) override;  // slot 20, 0x00080D90 folded
    AtGfxBufferSet* BufferSet() override;  // slot 21, 0x000EF330
    void Slot22() override;            // slot 22, 0x000EF340 not recovered: the buffer set's 0x000F9A40
    unsigned int Width() override;     // slot 23, 0x00154540 folded
    unsigned int Height() override;    // slot 24, 0x00079DA0 folded
    void DrawScale(float* out) override;  // slot 25, 0x000F02F0 1.0f and 1.0f

    void Init();                       // 0x000F0120 the device's own words; also the end of Release
    void UnlinkContext(AtGfxContext* context);  // 0x000EF060 out of the list at +0x3334; nothing if absent

    void* m_creator;                   // +0x3A8
    int m_unknown3AC;                  // +0x3AC
    AtGrD3DSurface2D m_target0;      // +0x3B0
    AtGrD3DSurface2D m_target1;      // +0x400
    AtGfxContext m_context;            // +0x450
    AtGrD3DSurface2D m_textureA2ED0; // +0x2ED0
    AtGrD3DTexture2D m_texture2F20; // +0x2F20
    AtGrD3DTexture2D m_texture2F54; // +0x2F54
    AtGrD3DSurface2D m_textureA2F88; // +0x2F88
    AtGrD3DTexture2D m_texture2FD8; // +0x2FD8
    AtGrD3DSurface2D m_textureA300C; // +0x300C
    AtGrD3DTexture2D m_texture305C; // +0x305C
    AtGrD3DSurface2D m_textureA3090; // +0x3090
    AtGrD3DTexture2D m_texture30E0; // +0x30E0
    AtGrD3DSurface2D m_textureA3114; // +0x3114
    AtGrD3DTexture2D m_texture3164; // +0x3164
    AtGrD3DSurface2D m_textureA3198; // +0x3198
    AtGrD3DTexture2D m_texture31E8; // +0x31E8
    AtGrD3DSurface2D m_textureA321C; // +0x321C
    AtGrD3DTexture2D m_texture326C; // +0x326C
    AtGrD3DSurface2D m_textureA32A0; // +0x32A0
    AtGrD3DTexture2D m_texture32F0; // +0x32F0
    unsigned char m_unknown3324[8];    // +0x3324 left
    int m_unknown332C;                 // +0x332C slots 9 and 12 test it, as a sign the display is open
    int m_unknown3330;                 // +0x3330
    AtGfxContext* m_contexts;          // +0x3334 a list by each context's m_next
    int m_unknown3338;                 // +0x3338
    int m_unknown333C;                 // +0x333C
    AtGrD3DMeshDatabase* m_meshDatabases;  // +0x3340 a list by each database's m_next
    int m_unknown3344;                 // +0x3344
    int m_unknown3348;                 // +0x3348
    int m_unknown334C;                 // +0x334C
    int m_unknown3350;                 // +0x3350
    AtGfxStateCache m_state;           // +0x3354
    int m_unknown3DF8;                 // +0x3DF8
    int m_unknown3DFC;                 // +0x3DFC left
    int m_unknown3E00;                 // +0x3E00 1
    int m_unknown3E04;                 // +0x3E04
    int m_unknown3E08;                 // +0x3E08
    int m_unknown3E0C;                 // +0x3E0C
    int m_unknown3E10;                 // +0x3E10 0x901
    AtGfxDrawLists m_drawLists;        // +0x3E14
    AtMatrixDeviceXbox m_matrixDevice; // +0x3E40 what 0x0001A68C hands to AtCoordSysDatabase::Load
    AtGfxBufferSet m_bufferSet;        // +0x3E48
    AtGrD3DMeshDatabase m_meshDatabase;  // +0x3ED4
    AtGrD3DTextureDatabase m_textureDatabase;  // +0x3F24
    void* m_unknown3F7C[10];           // +0x3F7C Release frees 0, 1, 2, 4, 6, 8 and 3, in that order
    int* m_unknown3FA4;                // +0x3FA4 &m_table3FA8
    int m_table3FA8[0x100];            // +0x3FA8
    int m_table43A8[0x100];            // +0x43A8
    AtMatrixPoolXbox m_pool47A8;       // +0x47A8
    int m_table47B4[0x100];            // +0x47B4
    AtMatrixPoolXbox m_pool4BB4;       // +0x4BB4
    int m_table4BC0[0x100];            // +0x4BC0
    AtMatrixPoolXbox m_pool4FC0;       // +0x4FC0
    int m_table4FCC[0x100];            // +0x4FCC
    AtMatrixPoolXbox m_pool53CC;       // +0x53CC
    int m_table53D8[0x100];            // +0x53D8
    AtMatrixPoolXbox m_pool57D8;       // +0x57D8
    int m_table57E4[0x100];            // +0x57E4
    int m_table5BE4[0x100];            // +0x5BE4
    int m_unknown5FE4[4];              // +0x5FE4
    int m_unknown5FF4;                 // +0x5FF4
    unsigned char m_unknown5FF8[8];    // +0x5FF8 left
    AtMatrixObject m_matrices[4];      // +0x6000 each the identity
    int m_unknown6100;                 // +0x6100
    int m_unknown6104[0x10];           // +0x6104
    int m_unknown6144;                 // +0x6144
    int m_unknown6148;                 // +0x6148
    int m_unknown614C[0x3400];         // +0x614C
    AtGfxIndexedList m_lists;          // +0x1314C
    AtGfxBlockPair m_pair13170;        // +0x13170
    AtGfxBlockPair m_pair13180;        // +0x13180
    unsigned char m_unknown13190[0x2CFE0 - 0x13190];  // +0x13190 left
    AtGfxSortedPool m_sortedA;         // +0x2CFE0
    unsigned char m_unknown2D004[0x10];  // +0x2D004 left
    AtGfxSortedPool m_sortedB;         // +0x2D014
    AtGfxResourceXbox m_resources[0x80];  // +0x2D038
    unsigned char m_unknown2F838[0x30240 - 0x2F838];  // +0x2F838 left
    int m_unknown30240[5];             // +0x30240 slot 9 sets [2]
    int m_unknown30254[0x11];          // +0x30254 slot 12 reads and sets [0xC] and hands the block to the library
    unsigned char m_unknown30298[0x30598 - 0x30298];  // +0x30298 left
    int m_unknown30598;                // +0x30598 2
    int m_unknown3059C;                // +0x3059C left
    int m_unknown305A0[4];             // +0x305A0
    unsigned char m_unknown305B0[0x10];  // +0x305B0 left
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtGfxContextBase::Part30) == 0x18, "Part30");
static_assert(sizeof(AtGfxContextBase::Part48) == 0xEC, "Part48");
static_assert(offsetof(AtGfxContextBase, m_unknown134) == 0x134, "AtGfxContextBase::m_unknown134");
static_assert(offsetof(AtGfxContextBase, m_shared) == 0x294C, "AtGfxContextBase::m_shared");
static_assert(sizeof(AtGfxContextBase) == 0x2950, "AtGfxContextBase");
static_assert(offsetof(AtGfxContext, m_unknown2A30) == 0x2A30, "AtGfxContext::m_unknown2A30");
static_assert(offsetof(AtGfxContext, m_next) == 0x2A7C, "AtGfxContext::m_next");
static_assert(sizeof(AtGfxContext) == 0x2A80, "AtGfxContext");
static_assert(offsetof(AtGfxStateCache, m_unknown5AC) == 0x5AC, "AtGfxStateCache::m_unknown5AC");
static_assert(offsetof(AtGfxStateCache, m_cachedA04) == 0xA04, "AtGfxStateCache::m_cachedA04");
static_assert(offsetof(AtGfxStateCache, m_blockA58) == 0xA58, "AtGfxStateCache::m_blockA58");
static_assert(sizeof(AtGfxStateCache) == 0xAA4, "AtGfxStateCache");
static_assert(sizeof(AtGfxDrawLists) == 0x2C, "AtGfxDrawLists");
static_assert(offsetof(AtGfxBufferSet, m_indexArrays) == 0x30, "AtGfxBufferSet::m_indexArrays");
static_assert(sizeof(AtGfxBufferSet) == 0x8C, "AtGfxBufferSet");
static_assert(sizeof(AtGfxIndexedList) == 0x24, "AtGfxIndexedList");
static_assert(offsetof(AtGfxResource, m_unknown30) == 0x30, "AtGfxResource::m_unknown30");
static_assert(sizeof(AtGfxResourceXbox) == 0x50, "AtGfxResourceXbox");
static_assert(sizeof(AtGfxSortedPool) == 0x24, "AtGfxSortedPool");
static_assert(offsetof(AtGraphicsDeviceXbox, m_creator) == 0x3A8, "m_creator");
static_assert(offsetof(AtGraphicsDeviceXbox, m_target0) == 0x3B0, "m_target0");
static_assert(offsetof(AtGraphicsDeviceXbox, m_context) == 0x450, "m_context");
static_assert(offsetof(AtGraphicsDeviceXbox, m_textureA2ED0) == 0x2ED0, "m_textureA2ED0");
static_assert(offsetof(AtGraphicsDeviceXbox, m_texture32F0) == 0x32F0, "m_texture32F0");
static_assert(offsetof(AtGraphicsDeviceXbox, m_unknown332C) == 0x332C, "m_unknown332C");
static_assert(offsetof(AtGraphicsDeviceXbox, m_contexts) == 0x3334, "m_contexts");
static_assert(offsetof(AtGraphicsDeviceXbox, m_meshDatabases) == 0x3340, "m_meshDatabases");
static_assert(offsetof(AtGraphicsDeviceXbox, m_state) == 0x3354, "m_state");
static_assert(offsetof(AtGraphicsDeviceXbox, m_unknown3E10) == 0x3E10, "m_unknown3E10");
static_assert(offsetof(AtGraphicsDeviceXbox, m_drawLists) == 0x3E14, "m_drawLists");
static_assert(offsetof(AtGraphicsDeviceXbox, m_matrixDevice) == 0x3E40, "m_matrixDevice");
static_assert(offsetof(AtGraphicsDeviceXbox, m_bufferSet) == 0x3E48, "m_bufferSet");
static_assert(offsetof(AtGraphicsDeviceXbox, m_meshDatabase) == 0x3ED4, "m_meshDatabase");
static_assert(offsetof(AtGraphicsDeviceXbox, m_textureDatabase) == 0x3F24, "m_textureDatabase");
static_assert(offsetof(AtGraphicsDeviceXbox, m_unknown3F7C) == 0x3F7C, "m_unknown3F7C");
static_assert(offsetof(AtGraphicsDeviceXbox, m_pool47A8) == 0x47A8, "m_pool47A8");
static_assert(offsetof(AtGraphicsDeviceXbox, m_pool57D8) == 0x57D8, "m_pool57D8");
static_assert(offsetof(AtGraphicsDeviceXbox, m_table5BE4) == 0x5BE4, "m_table5BE4");
static_assert(offsetof(AtGraphicsDeviceXbox, m_matrices) == 0x6000, "m_matrices");
static_assert(offsetof(AtGraphicsDeviceXbox, m_unknown614C) == 0x614C, "m_unknown614C");
static_assert(offsetof(AtGraphicsDeviceXbox, m_lists) == 0x1314C, "m_lists");
static_assert(offsetof(AtGraphicsDeviceXbox, m_pair13180) == 0x13180, "m_pair13180");
static_assert(offsetof(AtGraphicsDeviceXbox, m_sortedA) == 0x2CFE0, "m_sortedA");
static_assert(offsetof(AtGraphicsDeviceXbox, m_sortedB) == 0x2D014, "m_sortedB");
static_assert(offsetof(AtGraphicsDeviceXbox, m_resources) == 0x2D038, "m_resources");
static_assert(offsetof(AtGraphicsDeviceXbox, m_unknown30240) == 0x30240, "m_unknown30240");
static_assert(offsetof(AtGraphicsDeviceXbox, m_unknown30598) == 0x30598, "m_unknown30598");
static_assert(sizeof(AtGraphicsDeviceXbox) == 0x305C0, "AtGraphicsDeviceXbox");
#pragma pop_macro("offsetof")
#endif
