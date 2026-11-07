// What holds a file's materials by name, and the texture lists they point into. Read from .AGM files. Original address cluster 0x00038F70-0x000398AF. The class is
// abstract: slots 2, 3 and 8 are filled by a concrete database at vftable 0x00289300, in the graphics layer, which is not recovered. Names are inferred.
#pragma once

#include "at/AtMaterial.h"

// Vftable 0x00275124.
class AtMaterialDatabase : public GmNameMap {
public:
    AtMaterialDatabase();                      // 0x00038F80
    ~AtMaterialDatabase() override;            // slot 0; 0x00039890 deleting, 0x00039470 complete
    void Clear() override;                     // slot 1, 0x000392F0

    virtual void OnReady(AtMaterialOwner* owner) = 0;   // slot 2: m_count is known, and the concrete database makes room for that many materials
    virtual void Slot3() = 0;                  // slot 3: not known yet
    // Slot 4, 0x00039180. Copies only materials that were loaded, and takes no names: the copy finds nothing by name until something inserts them.
    virtual void CopyFrom(AtMaterialOwner* owner, AtMaterialDatabase& from, const char* ns);
    virtual void Allocate(AtMaterialOwner* owner, unsigned int stages, unsigned int textures, unsigned int count, const char* ns);  // slot 5, 0x00039050 lists cleared
    virtual void Load(AtMaterialOwner* owner, const char* name, int unused);   // slot 6, 0x00038FB0 name + ".AGM"
    virtual void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);       // slot 7, 0x00039500 lists not cleared
    virtual AtMaterial* MaterialAt(unsigned int index) = 0;  // slot 8

    // 0x00039340. Entries in a list: a name, or the word Default for none. withSpaces: each name is followed by the name space to find it in, and ns is not used.
    void ReadTextures(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, void** list, unsigned int count, const char* ns, int withSpaces);

    static void SetHeap(void* heap);           // 0x00038F70

    unsigned int m_count;            // +0x14
    unsigned int m_stageCapacity;    // +0x18 ProgShaderTexture
    unsigned int m_stageUsed;        // +0x1C
    void** m_stages;                 // +0x20
    unsigned int m_textureCapacity;  // +0x24 StagedShaderTexture
    unsigned int m_textureUsed;      // +0x28
    void** m_textures;               // +0x2C
    int m_unknown30;                 // +0x30 only ever cleared here

    static void* s_heap;             // 0x002C63F0
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtMaterialDatabase, m_count) == 0x14, "AtMaterialDatabase::m_count");
static_assert(offsetof(AtMaterialDatabase, m_stages) == 0x20, "AtMaterialDatabase::m_stages");
static_assert(offsetof(AtMaterialDatabase, m_textures) == 0x2C, "AtMaterialDatabase::m_textures");
static_assert(sizeof(AtMaterialDatabase) == 0x34, "AtMaterialDatabase");
#pragma pop_macro("offsetof")
#endif
