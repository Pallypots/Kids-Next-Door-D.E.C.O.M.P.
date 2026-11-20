// The Xbox graphics layer's texture database and material database: what the concrete loader makes for an .AGD's TextureDatabase and MaterialDatabase lists, and the items they hold. All names are inferred
// but two: "AtGrIPixelSurface" is the original's own, from its message in 0x00060430, and "Texture database" from 0x0028A698. See docs/worklog/2026-11-08-xdb.md.
//
// What is recovered: both databases whole (their vftables 0x0028A6E0 and 0x00289300); the 2D texture, the 2D surface and its palette as far as reading a file, clearing and destroying need; the material and its
// eight stage states as far as clearing and destroying need. What a 2D texture does when it loads (slot 12, 0x000FCAD0: the .DDS file, the texture made, each level copied in) is recovered as far as the device,
// in include/at/AtGrD3DTextureLoad.h (docs/worklog/2026-11-19-texload.md). What is not, and stops or goes to a seam: what a material does with the device when it has been read (slot 0, 0x0010B7F0, which builds
// its stage states); the device's own calls (src/at/AtGrD3DTextureSeam.cpp, which a test supplies); 3D textures, cube maps and 3D surfaces, which no file on the disc lists (src/at/AtGrD3DUnrecovered.cpp: their
// constructors stop); and the surfaces' and palettes' slots that fill, convert and copy pixels.
#pragma once

#include "at/AtMaterialDatabase.h"
#include "at/AtTextureDatabase.h"

struct AtGrD3DOwner;
class AtGrD3DTexture2D;
class AtGrD3DMaterial;
class AtGrDdsPixelMap;  // include/at/AtGrD3DTextureLoad.h

// ---- The seam: src/at/AtGrD3DTextureSeam.cpp, or a test's ---------------------------------------------------------------------------------------------------------------------------------------------------------

// The device's calls a texture makes when it loads are declared in include/at/AtGrD3DTextureLoad.h, and are in the same file.
void AtGrD3DLoadMaterial(AtGrD3DMaterial* material, AtMaterialOwner* owner);   // 0x0010B7F0 slot 0: the stage states from the material's shader and textures
void AtGrD3DReleaseStage(void* stages, unsigned int stage);                    // 0x000FB630 the device's stage `stage` lets go of whatever texture it holds; `stages` is the owner's +0x3354
void AtGrD3DBeforeRelease(void* resource);                                     // 0x00205E20 called on a texture just before it is released; what it does is not known
void AtGrD3DRelease(void* resource);                                           // 0x00205D10 a Direct3D resource's release

// ---- Palettes -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028B118: the destructor and ten pure slots. 8 bytes.
class AtGrIPalette {
public:
    AtGrIPalette();                    // 0x0011C160
    virtual ~AtGrIPalette();           // slot 0; 0x0011C190 deleting, 0x0011C180 complete
    virtual void CopyFrom(const AtGrIPalette* from) = 0;  // slot 1
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;

    unsigned short m_count;            // +0x04 entries, of four bytes
    unsigned short m_unknown06;        // +0x06
};

// Vftable 0x00289274. What a surface with an indexed format holds its colours in. 0x10 bytes. The slots but 0 are not recovered: they stop.
class AtGrD3DPalette : public AtGrIPalette {
public:
    AtGrD3DPalette();                  // 0x000F5A70
    ~AtGrD3DPalette() override;        // slot 0; 0x000F5FA0 deleting, 0x000F5CC0 complete
    void Free();                       // 0x000F5AA0 the entries, then the Direct3D palette; counts to 0

    void CopyFrom(const AtGrIPalette* from) override;  // slot 1, 0x000F5AE0
    void Slot2() override;             // 0x000F58E0
    void Slot3() override;             // 0x000F5EC0
    void Slot4() override;             // 0x000F5920
    void Slot5() override;             // 0x000F5F20
    void Slot6() override;             // 0x000F5970
    void Slot7() override;             // 0x000F5B10
    void Slot8() override;             // 0x000F5B80
    void Slot9() override;             // 0x000F5BF0
    void Slot10() override;            // 0x000F5C60

    void* m_resource;                  // +0x08 the Direct3D palette
    unsigned int* m_entries;           // +0x0C from the heap
};

// ---- Surfaces -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x00277458. A surface of pixels, before the device: its format, a colour key and two sizes. 0x24 bytes.
class AtGrIPixelSurface {
public:
    enum Flags { kColorKey = 0x2 };    // in m_flags

    AtGrIPixelSurface();               // 0x00060480
    virtual ~AtGrIPixelSurface();      // slot 0; 0x00060540 deleting, 0x00060510 complete
    virtual void Reset();              // slot 1, 0x000604B0 as new
    virtual AtGrIPalette* Palette();   // slot 2, 0x000AB720 none, folded
    virtual int Slot3() = 0;           // slot 3
    virtual void SetColorKey(const unsigned char* rgba);  // slot 4, 0x000604E0
    virtual void ClearColorKey();      // slot 5, 0x00060400 only the flag
    virtual void Slot6();              // slot 6, 0x001CC9F0 empty, folded

    AtPixelFormat m_format;            // +0x04
    unsigned char m_colorKey[4];       // +0x18
    unsigned short m_unknown1C;        // +0x1C the texture's usage, as AtTextureBase::m_usage, when a 2D surface loads
    unsigned short m_flags;            // +0x1E
    unsigned short m_unknown20;        // +0x20 the width, when a 2D surface loads
    unsigned short m_unknown22;        // +0x22 the height, likewise
};

// Vftable 0x00289768. A level of a 2D texture: the texture database holds them in one array and hands each texture a run of them, one to a level. 0x50 bytes. Lock and Unlock (slots 9 and 10) and loading are
// recovered in src/at/AtGrD3DTextureLoad.cpp as far as a DDS file needs; the other slots from 6 fill and copy pixels and are not recovered: they stop.
class AtGrD3DSurface2D : public AtGrIPixelSurface {
public:
    // In m_flags while it is loaded.
    enum LoadFlags { kLoaded = 0x1, kLocked = 0x4, kLockedToRead = 0x8, kLockedToWrite = 0x10, kStaged = 0x40 };
    AtGrD3DSurface2D();                // 0x000FCEE0
    ~AtGrD3DSurface2D() override;      // slot 0; 0x0010F8F0 vector deleting, 0x000FD8C0 complete
    void Reset() override;             // slot 1, 0x000FCF40 the chain after it, its blocks, its Direct3D surface and palette, then the base's
    AtGrIPalette* Palette() override;  // slot 2, 0x000FD230 the palette when the format is indexed
    int Slot3() override;              // 0x000AB720 0, folded
    void Slot6() override;             // 0x000FD240
    virtual void Slot7();              // 0x000FD9A0
    virtual void Slot8();              // 0x0005F2E0
    // Slot 9, 0x000FE260. Where its bytes are, and their pitch: its own copy in memory if it has one, the device's surface directly when the device's rows can be written as they are (a DDS
    // texture's), otherwise a staging copy, which is not recovered. flags bit 0 is to read, bit 1 to write; neither is both.
    virtual void Lock(void** bits, int* pitch, unsigned int flags);
    virtual void Unlock();             // slot 10, 0x000FE320 the device's surface needs nothing; a staging copy or a copy in memory is not recovered
    virtual void Slot11();             // 0x001ACD70
    virtual void Slot12();             // 0x000FD0C0
    virtual void Slot13();             // 0x000FD190

    void ReleaseChain();               // 0x000FCFC0 the Direct3D surface of this one and of each after it; nothing else

    // 0x000FDF40. Level `level` (face `face`) of a texture, from a DDS file, onto the device's surface `surface`. previous is the level above, or null.
    void LoadFromDds(AtGrD3DOwner* owner, void* surface, AtGrDdsPixelMap* map, unsigned int usage, unsigned int level, unsigned int face, AtGrD3DTexture2D* texture, AtGrD3DSurface2D* previous);
    // 0x000FE100. A level that has no file: the device's surface, its size and format, a palette for an indexed format, and the transparent colour if there is one.
    void Prepare(AtGrD3DOwner* owner, void* surface, unsigned int usage, AtGrD3DTexture2D* texture, AtGrD3DSurface2D* previous, const unsigned char* transparent);
    void TakeDeviceSize(void* surface);  // 0x000FDB00 the size and format the device made it in
    // 0x000FD2A0. A device format number as this surface's format: a compressed one is kept as the 16-bit form the owner's chooser makes of 1-5-5-5 (DXT1) or 4-4-4-4 (the others).
    void TakeDeviceFormat(unsigned int format, AtPixelFormat* out);
    int NeedsStaging();                // 0x000FD800 1 unless its rows can be written straight to the device's surface

    AtGrD3DOwner* m_unknown24;         // +0x24 the owner, when it loads; not set by the constructor
    void* m_surface;                   // +0x28 the Direct3D surface
    AtGrD3DPalette m_palette;          // +0x2C
    AtGrD3DTexture2D* m_unknown3C;     // +0x3C the texture, when it loads
    AtGrD3DSurface2D* m_unknown40;     // +0x40 the level above, when it loads
    void* m_unknown44;                 // +0x44 from the heap
    void* m_unknown48;                 // +0x48 from the heap
    AtGrD3DSurface2D* m_next;          // +0x4C owned: reset and deleted with this one
};

// ---- Textures -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x00289700. 0x34 bytes. What is done with the device, slots 11 and 12, is not recovered; nor are slots 8 and 9, whose base type is an int and which return pointers (see Texture and PaletteOf).
class AtGrD3DTexture2D : public AtTextureItem {
public:
    AtGrD3DTexture2D();                // 0x000FC630
    ~AtGrD3DTexture2D() override;      // slot 0; 0x0010FB30 vector deleting, 0x000FCCC0 complete
    // Slot 1, 0x000FC6C0, and slot 2, 0x000FC670: the base's, then a run of the database's 2D surfaces: one for a front or back buffer source, else one to each level asked for.
    void Describe(AtTextureDatabase* database, const char* name, unsigned int usage, int autoGenMipLevels, unsigned int levels, int textureStage) override;
    void Read(AtTextureDatabase* database, const char* name, AtIoTokenizer* tokenizer) override;
    void ClearPending() override;      // slot 3, 0x000FCCA0 released first, if pending
    void Reset() override;             // slot 4, 0x000FCC80 released, then the base's
    int Unknown8() override;           // 0x000FC620 returns the texture: stops here, see above
    int Unknown9() override;           // 0x000FC590 returns the first surface's palette: stops here, see above
    // Slot 11 is 0x000FC8A0, which takes four arguments where the base declares none: its body is Make, which OnLoaded calls directly; this stops.
    void Slot11() override;
    // Slot 12, 0x000FCAD0. Once, unless already loaded: a front or back buffer source takes the device's buffer; a ProgSource is made to its size and format; a DDS or platform source reads the .DDS file
    // named for the texture, through AtGrDdsPixelMap::s_map, makes the texture no deeper than the file, and copies each level in. Choosing the reader for any other source is not recovered and stops.
    // Then marked loaded, and kTransparentColor kept only if the first level has a palette.
    void OnLoaded(AtMaterialOwner* owner) override;
    virtual AtGrD3DSurface2D* SurfaceAt(unsigned int level);  // slot 13, 0x000FC570

    // 0x000FC8A0, slot 11's body. A texture of this size and format with no file: made, each level prepared from the smallest up, then marked as OnLoaded marks it. Nothing if already loaded.
    void Make(AtMaterialOwner* owner, unsigned int width, unsigned int height, const AtPixelFormat* format);
    // 0x000FC720. Each level from a DDS file, from the smallest up, then the file freed.
    void LoadLevelsFromDds(AtGrDdsPixelMap* map);

    // 0x000FC980. The device's four stages let go of it, if it was loaded for a device; its surfaces let go of theirs, level by level; then its own texture. The owner and the surfaces are kept.
    void Release();
    void* Texture() { return m_texture; }                        // what slot 8 returns
    AtGrIPalette* PaletteOf() { return m_surfaces->Palette(); }  // what slot 9 returns

    AtGrD3DOwner* m_owner;             // +0x28 set when it is loaded
    void* m_texture;                   // +0x2C the Direct3D texture
    AtGrD3DSurface2D* m_surfaces;      // +0x30 in the database's array
};

// 3D textures, cube maps and 3D surfaces: no .AGT on the disc lists one. Vftables 0x0028B88C, 0x0028B8DC, 0x00289738; 0x30, 0x30 and 0x50 bytes. Every body stops (src/at/AtGrD3DUnrecovered.cpp), the
// constructor first.
class AtGrD3DTexture3D : public AtTextureItem {
public:
    AtGrD3DTexture3D();                // 0x001214A0
    ~AtGrD3DTexture3D() override;      // 0x001214C0
    void Slot11() override;
    void OnLoaded(AtMaterialOwner* owner) override;
    char m_unknown28[0x08];
};

class AtGrD3DCubeMap : public AtTextureItem {
public:
    AtGrD3DCubeMap();                  // 0x00121730
    ~AtGrD3DCubeMap() override;        // 0x001220E0
    void Slot11() override;
    void OnLoaded(AtMaterialOwner* owner) override;
    char m_unknown28[0x08];
};

class AtGrD3DSurface3D : public AtGrIPixelSurface {
public:
    AtGrD3DSurface3D();                // 0x000FCD50
    ~AtGrD3DSurface3D() override;      // 0x000FCDF0
    int Slot3() override;
    char m_unknown24[0x2C];
};

// ---- The texture database -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028A6E0, 0x58 bytes. Each kind's array is made from AtTextureDatabase::s_heap by its count; 2D surfaces are handed out to 2D textures in order as they read.
class AtGrD3DTextureDatabase : public AtTextureDatabase {
public:
    AtGrD3DTextureDatabase();                 // 0x0010FFC0
    ~AtGrD3DTextureDatabase() override;       // slot 0; 0x00112240 vector deleting, 0x001101D0 complete
    void Clear() override;                    // slot 1, 0x0010FFF0 off the owner's list, the arrays, then the base's. m_next is left
    void OnReady(AtMaterialOwner* owner) override;  // slot 2, 0x0010F810 keeps the owner and goes on its list
    void MakeSurfaces2D() override;           // slot 3, 0x0010F830
    void MakeSurfaces3D() override;           // slot 4, 0x0010F950
    void MakeTextures2D() override;           // slot 5, 0x0010FA70
    void MakeTextures3D() override;           // slot 6, 0x0010FB90
    void MakeCubeMaps() override;             // slot 7, 0x0010FCB0
    void MakeMore() override;                 // slot 8, 0x001100D0 a database of the same counts and name space, with no file
    void* Surface2DAt(unsigned int index) override;   // slot 9, 0x0010FDD0
    void* Surface3DAt(unsigned int index) override;   // slot 10, 0x0010FDF0
    AtTextureItem* Texture2DAt(unsigned int index) override;  // slot 11, 0x0010FE10
    AtTextureItem* Texture3DAt(unsigned int index) override;  // slot 12, 0x0010FE20
    AtTextureItem* CubeMapAt(unsigned int index) override;    // slot 13, 0x0010FE40

    // 0x0010FE60. The next `count` 2D surfaces. Running out is reported, and the surfaces past the end are handed out anyway: in the game the report does not come back.
    AtGrD3DSurface2D* TakeSurfaces2D(unsigned int count);

    AtGrD3DOwner* m_owner;                    // +0x34
    AtGrD3DSurface2D* m_surfaces2D;           // +0x38
    AtGrD3DSurface3D* m_surfaces3D;           // +0x3C
    AtGrD3DTexture2D* m_textures2D;           // +0x40
    AtGrD3DTexture3D* m_textures3D;           // +0x44
    AtGrD3DCubeMap* m_cubeMaps;               // +0x48
    unsigned int m_surfaces2DTaken;           // +0x4C
    unsigned int m_unknown50;                 // +0x50 only ever cleared
    AtGrD3DTextureDatabase* m_next;           // +0x54 on the owner's list
};

// ---- Materials ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// One of a stage state's entries. 0x14 bytes. Made by what builds the stage states, not recovered; its destructor clears it.
struct AtGrD3DStageEntry {
    ~AtGrD3DStageEntry();              // 0x00105330
    unsigned int m_unknown00;          // +0x00
    unsigned int m_unknown04;          // +0x04
    unsigned int m_unknown08;          // +0x08
    unsigned short m_unknown0C;        // +0x0C
    unsigned short m_unknown0E;        // +0x0E
    unsigned char m_unknown10[4];      // +0x10
};

// What a material's slot 0 builds for one texture stage. 0x28 bytes. Not a polymorphic class.
class AtGrD3DMaterialStage {
public:
    enum Flags { kShared = 0x1 };      // in m_flags: holds a count of s_sharedUsers

    AtGrD3DMaterialStage();            // 0x000F6400
    ~AtGrD3DMaterialStage();           // 0x000F6450 Reset, by a jump
    // 0x000F6250. Frees its blocks and entries; the last stage marked shared frees the shared tables. Then as new.
    void Reset();

    AtGrD3DStageEntry* m_entries;      // +0x00 an array with its count before it
    void* m_unknown04;                 // +0x04 from the heap
    void* m_unknown08;                 // +0x08 from the heap
    unsigned int m_unknown0C;          // +0x0C
    unsigned int m_unknown10;          // +0x10
    unsigned short m_flags;            // +0x14
    unsigned short m_unknown16;        // +0x16
    unsigned short m_unknown18;        // +0x18
    unsigned short m_unknown1A;        // +0x1A
    unsigned short m_unknown1C;        // +0x1C 0x8006 when new
    unsigned short m_unknown1E;        // +0x1E 1 when new
    unsigned short m_unknown20;        // +0x20
    unsigned short m_unknown22;        // +0x22 0x8006 when new
    unsigned short m_unknown24;        // +0x24 1 when new
    unsigned short m_unknown26;        // +0x26

    // The shared tables, at 0x003B1F74 to 0x003B1F8C. Filled by what builds the stage states; freed by the last shared stage.
    static unsigned int s_sharedUsers;        // 0x003B1F74
    static AtGrD3DStageEntry* s_sharedEntries;  // 0x003B1F78 an array with its count before it
    static unsigned int s_shared7C;           // 0x003B1F7C
    static unsigned int s_shared80;           // 0x003B1F80
    static void* s_shared84;                  // 0x003B1F84 from the heap
    static void* s_shared88;                  // 0x003B1F88 from the heap
    static unsigned int s_shared8C;           // 0x003B1F8C
    static void FreeShared();                 // 0x000F61D0
};

// Vftable 0x0028A200. 0x18C bytes. Slot 0, what it does with the device when it has been read, is the seam.
class AtGrD3DMaterial : public AtMaterial {
public:
    AtGrD3DMaterial();                 // 0x0010A3F0
    ~AtGrD3DMaterial() override;       // slot 1; 0x000F7A90 vector deleting, 0x000F7870 complete
    void OnLoaded(AtMaterialOwner* owner) override;  // slot 0, 0x0010B7F0: the seam

    void ReleaseStages(AtMaterialOwner* owner);  // 0x0010A330 each stage state built, reset; the owner is not used
    void Release(AtMaterialOwner* owner);        // 0x0010A460 the stage states, then AtMaterial::Reset

    AtGrD3DMaterialStage m_stageStates[8];  // +0x48
    unsigned short m_unknown188;       // +0x188
    unsigned short m_stageStateCount;  // +0x18A built by slot 0
};

// Vftable 0x00289300, 0x40 bytes. Materials are made from a heap of its own.
class AtGrD3DMaterialDatabase : public AtMaterialDatabase {
public:
    AtGrD3DMaterialDatabase();                // 0x000F7790
    ~AtGrD3DMaterialDatabase() override;      // slot 0; 0x00112370 vector deleting, 0x000F7970 complete
    void Clear() override;                    // slot 1, 0x000F77B0 off the owner's list, each material released, the array, then the base's. m_next is left
    void OnReady(AtMaterialOwner* owner) override;  // slot 2, 0x000F79C0 m_count materials; keeps the owner and goes on its list
    void Slot3() override;                    // slot 3, 0x000F7AF0 makes m_more, a database of the same counts and name space, with no file
    AtMaterial* MaterialAt(unsigned int index) override;  // slot 8, 0x000F7840 past m_count, m_more's

    static void SetHeap(void* heap);          // 0x000F7780

    AtGrD3DOwner* m_owner;                    // +0x34
    AtGrD3DMaterial* m_materials;             // +0x38
    AtGrD3DMaterialDatabase* m_next;          // +0x3C on the owner's list

    static void* s_heap;                      // 0x003B1F90
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtGrIPalette) == 0x08, "AtGrIPalette");
static_assert(sizeof(AtGrD3DPalette) == 0x10, "AtGrD3DPalette");
static_assert(sizeof(AtGrIPixelSurface) == 0x24, "AtGrIPixelSurface");
static_assert(offsetof(AtGrIPixelSurface, m_flags) == 0x1E, "AtGrIPixelSurface::m_flags");
static_assert(offsetof(AtGrD3DSurface2D, m_palette) == 0x2C, "AtGrD3DSurface2D::m_palette");
static_assert(offsetof(AtGrD3DSurface2D, m_next) == 0x4C, "AtGrD3DSurface2D::m_next");
static_assert(sizeof(AtGrD3DSurface2D) == 0x50, "AtGrD3DSurface2D");
static_assert(sizeof(AtGrD3DSurface3D) == 0x50, "AtGrD3DSurface3D");
static_assert(offsetof(AtGrD3DTexture2D, m_owner) == 0x28, "AtGrD3DTexture2D::m_owner");
static_assert(sizeof(AtGrD3DTexture2D) == 0x34, "AtGrD3DTexture2D");
static_assert(sizeof(AtGrD3DTexture3D) == 0x30, "AtGrD3DTexture3D");
static_assert(sizeof(AtGrD3DCubeMap) == 0x30, "AtGrD3DCubeMap");
static_assert(offsetof(AtGrD3DTextureDatabase, m_owner) == 0x34, "AtGrD3DTextureDatabase::m_owner");
static_assert(offsetof(AtGrD3DTextureDatabase, m_next) == 0x54, "AtGrD3DTextureDatabase::m_next");
static_assert(sizeof(AtGrD3DTextureDatabase) == 0x58, "AtGrD3DTextureDatabase");
static_assert(sizeof(AtGrD3DStageEntry) == 0x14, "AtGrD3DStageEntry");
static_assert(offsetof(AtGrD3DMaterialStage, m_flags) == 0x14, "AtGrD3DMaterialStage::m_flags");
static_assert(sizeof(AtGrD3DMaterialStage) == 0x28, "AtGrD3DMaterialStage");
static_assert(offsetof(AtGrD3DMaterial, m_stageStates) == 0x48, "AtGrD3DMaterial::m_stageStates");
static_assert(offsetof(AtGrD3DMaterial, m_stageStateCount) == 0x18A, "AtGrD3DMaterial::m_stageStateCount");
static_assert(sizeof(AtGrD3DMaterial) == 0x18C, "AtGrD3DMaterial");
static_assert(offsetof(AtGrD3DMaterialDatabase, m_owner) == 0x34, "AtGrD3DMaterialDatabase::m_owner");
static_assert(sizeof(AtGrD3DMaterialDatabase) == 0x40, "AtGrD3DMaterialDatabase");
#pragma pop_macro("offsetof")
#endif
