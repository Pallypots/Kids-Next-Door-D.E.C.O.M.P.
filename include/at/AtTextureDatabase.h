// What holds a file's textures by name. Read from .AGT files. Original address clusters 0x0001BF90-0x0001BFD4 and 0x0001BFE0-0x0001C8EB, with 0x00025E70 on the owner and 0x0002ADF0, the deleting
// destructor. "Texture database" is the original's own phrase, from its message at 0x0010FE60; other names are inferred. The class is abstract: slots 2 to 13 are filled by a concrete database at
// vftable 0x0028A6E0, in the graphics layer, which is not recovered, and neither is what an item does with the device.
#pragma once

#include "at/AtMaterial.h"

class AtTextureDatabase;

// One texture as this file's reader sees it. Base class at vftable 0x00274A04, not recovered: only the two slots and the two fields used here are declared. The other slots are placeholders, slot 0
// being a deleting destructor in the original.
class AtTextureItem {
public:
    enum Flags { kDemandLoad = 0x400 };

    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    // Slot 2, 0x000312D0 in the base class. Reads the item's body, a left curly through its right curly. name is the eight bytes given in the file, not always terminated.
    virtual void Read(AtTextureDatabase* database, const char* name, AtIoTokenizer* tokenizer) = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;
    virtual void Slot11() = 0;
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 12, 0x000FCAD0 in the concrete 2D texture

    char m_unknown04[0x14];
    unsigned short m_flags;     // +0x18
    char m_unknown1A[3];
    unsigned char m_levels;     // +0x1D the word MipMap's value, and 1 at the start of the base's Read; 0 from new, which is what marks an item unused
    char m_unknown1E[0x0A];
};

// Vftable 0x00272574.
class AtTextureDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 30 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0230 for the whole file, the items' keywords among them

    AtTextureDatabase();                      // 0x0001BFA0
    ~AtTextureDatabase() override;            // slot 0; 0x0002ADF0 deleting, 0x0001BFD0 complete. Only the base's work: unlike Clear, it does not free m_directory.
    void Clear() override;                    // slot 1, 0x0001C6B0

    virtual void OnReady(AtMaterialOwner* owner) = 0;  // slot 2; the concrete database keeps owner
    virtual void MakeSurfaces2D() = 0;        // slot 3: the count is known, and the concrete database makes room for that many
    virtual void MakeSurfaces3D() = 0;        // slot 4
    virtual void MakeTextures2D() = 0;        // slot 5
    virtual void MakeTextures3D() = 0;        // slot 6
    virtual void MakeCubeMaps() = 0;          // slot 7
    virtual void MakeMore() = 0;              // slot 8: makes m_more, a database of the same sizes
    virtual void* Surface2DAt(unsigned int index) = 0;  // slot 9
    virtual void* Surface3DAt(unsigned int index) = 0;  // slot 10
    virtual AtTextureItem* Texture2DAt(unsigned int index) = 0;  // slot 11
    virtual AtTextureItem* Texture3DAt(unsigned int index) = 0;  // slot 12
    virtual AtTextureItem* CubeMapAt(unsigned int index) = 0;    // slot 13

    // 0x0001C7B0. name + ".AGT". The file's directory is pushed on its file system while it reads, so that an item can find what it names beside the file. The path is kept if an item is to load later.
    void Load(AtMaterialOwner* owner, const char* name, int unused);
    // 0x0001BFE0. An item whose name the owner already knows is read, and then neither named here nor told it has loaded. Errors go to the tokenizer, and reading goes on.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);
    // 0x0001C600. Makes room as a file with these counts would, with no file. Items stay as new.
    void Allocate(AtMaterialOwner* owner, unsigned int surfaces2D, unsigned int surfaces3D, unsigned int textures2D, unsigned int textures3D, unsigned int cubeMaps, const char* ns);
    unsigned int CountUsedTextures();         // 0x0001C6F0 2D textures with levels, along m_more
    AtTextureItem* FindFreeTexture();         // 0x0001C730 the first 2D texture with no levels, along m_more, which it makes on reaching the end. Returns only on finding one.
    void PushDirectory();                     // 0x0001C770 what Load kept, if it kept anything
    void PopDirectory();                      // 0x0001C790

    static void SetHeap(void* heap);          // 0x0001BF90

    unsigned int m_surface2DCount;   // +0x14
    unsigned int m_surface3DCount;   // +0x18
    unsigned int m_texture2DCount;   // +0x1C
    unsigned int m_texture3DCount;   // +0x20
    unsigned int m_cubeMapCount;     // +0x24
    AtIoFileSystem* m_fileSystem;    // +0x28 what Load's file was opened on
    char* m_directory;               // +0x2C the path of Load's file, kept only when an item is to load on demand
    AtTextureDatabase* m_more;       // +0x30

    static unsigned int s_demandLoadCount;  // 0x002C62E0 demand-loaded items in the last file read
    static void* s_heap;             // 0x002C62E4 for m_directory, and the concrete database's lists
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtTextureItem, m_flags) == 0x18, "AtTextureItem::m_flags");
static_assert(offsetof(AtTextureItem, m_levels) == 0x1D, "AtTextureItem::m_levels");
static_assert(sizeof(AtTextureItem) == 0x28, "AtTextureItem");
static_assert(offsetof(AtTextureDatabase, m_surface2DCount) == 0x14, "AtTextureDatabase::m_surface2DCount");
static_assert(offsetof(AtTextureDatabase, m_fileSystem) == 0x28, "AtTextureDatabase::m_fileSystem");
static_assert(offsetof(AtTextureDatabase, m_more) == 0x30, "AtTextureDatabase::m_more");
static_assert(sizeof(AtTextureDatabase) == 0x34, "AtTextureDatabase");
#pragma pop_macro("offsetof")
#endif
