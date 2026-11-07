// What holds a file's images and fonts by name. Read from .AGI and .AGF files. Original address clusters 0x00029B90-0x0002A00B and 0x0002A010-0x0002A51B. The words are the original's, from its tables at
// 0x002B0838 and 0x002B08E0; other names are inferred. Both classes are abstract: the concrete databases, at vftables 0x0028A794 and 0x0028A774, and the items, are in the graphics layer.
// Not recovered: an image's body, slot 8, is 0x000586A0, and a font's, slot 7, is 0x000415A0 with 0x000FEA90 and what follows. The words of those bodies are in these tables but are not read here.
// PushDirectory and PopDirectory are one body each in the original, at the addresses noted on the font and the image respectively: the two classes keep the same fields at the same offsets.
#pragma once

#include "at/AtMatAssignment.h"

class AtImageDatabase;
class AtFontDatabase;

// One image, as this file's reader sees it. Base class at vftable 0x0027711C, concrete at 0x0028B9B8.
class AtImageItem {
public:
    enum Flags { kDemandLoad = 8 };

    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    // Slot 8. Reads the item's body, a left curly through its right curly. name is the eight bytes given in the file, not always terminated.
    virtual void Read(AtMaterialOwner* owner, AtImageDatabase* database, const char* name, AtIoTokenizer* tokenizer) = 0;
    virtual void Slot9() = 0;
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 10

    char m_unknown04[8];
    unsigned int m_flags;   // +0x0C
};

// One font, likewise. Base class at vftable 0x00276208, then 0x00289820; concrete at 0x0028B970. The original's own name for what draws them, from its messages, is AtGrI2DFont.
class AtFontItem {
public:
    enum Flags { kDemandLoad = 8 };

    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    // Slot 7. Reads the item's body, as for an image.
    virtual void Read(AtMaterialOwner* owner, AtFontDatabase* database, const char* name, AtIoTokenizer* tokenizer) = 0;
    virtual void Slot8() = 0;
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 9

    char m_unknown04[8];
    unsigned int m_flags;   // +0x0C
};

// Vftable 0x002733E0.
class AtImageDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 18 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0838 for the whole file, the items' words among them

    AtImageDatabase();                       // 0x00029BA0
    ~AtImageDatabase() override;             // slot 0; 0x00029FF0 deleting, 0x00029F60 complete. It empties the list twice, the second time on nothing.
    void Clear() override;                   // slot 1, 0x00029F00

    virtual void MakeItems() = 0;            // slot 2: the count is known, and the concrete database makes room for that many
    virtual void OnReady(AtMaterialOwner* owner) = 0;  // slot 3: the concrete database keeps owner
    virtual AtImageItem* ImageAt(unsigned int index) = 0;  // slot 4
    virtual int Slot5();                     // slot 5, 0x000AB720: returns 0, one body shared by many classes

    // 0x00029DC0. name + ".AGI". The file's directory is pushed on its file system while it reads, and the path kept if an item is to load later.
    void Load(AtMaterialOwner* owner, const char* name, int unused);
    // 0x00029C00. Sections in any order until the end of the text, at least one being tried even on an empty file.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);
    void PushDirectory();                    // the same body as AtFontDatabase's at 0x0002A450
    void PopDirectory();                     // 0x00029F40 what Load kept, if it kept anything; takes no last entry, so only pops

    static void SetHeap(void* heap);         // 0x00029B90

    AtMatAssignment m_materials;             // +0x14
    unsigned int m_count;                    // +0x1C
    AtIoFileSystem* m_fileSystem;            // +0x20 what Load's file was opened on
    char* m_directory;                       // +0x24 the path of Load's file, kept only when an item is to load on demand

    static unsigned int s_demandLoadCount;   // 0x002C6360 demand-loaded items in the last file read
    static void* s_heap;                     // 0x002C6364 for m_directory, and the concrete database's list
};

// Vftable 0x0027344C.
class AtFontDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 20 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B08E0 for the whole file, the items' words among them

    AtFontDatabase();                        // 0x0002A020
    ~AtFontDatabase() override;              // slot 0; 0x0002A500 deleting, 0x0002A470 complete
    void Clear() override;                   // slot 1, 0x0002A410

    virtual void OnReady(AtMaterialOwner* owner) = 0;  // slot 2: the concrete database keeps owner
    virtual void MakeFonts2D() = 0;          // slot 3
    virtual int Slot4();                     // slot 4, 0x000AB720: returns 0
    virtual void Slot5(int a, int b);        // slot 5, 0x000447F0: does nothing
    virtual AtFontItem* Font2DAt(unsigned int index) = 0;  // slot 6
    virtual int Slot7();                     // slot 7, 0x000AB720: returns 0

    void Load(AtMaterialOwner* owner, const char* name, int unused);  // 0x0002A290 name + ".AGF"
    // 0x0002A080. "Font[ n ]" must come first, with any name space before it; it sizes the table, and a Font2D count over it is reported.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);
    void Allocate(AtMaterialOwner* owner, unsigned int count, const char* ns);  // 0x0002A3D0 room for fonts, with no file. The fonts are made before the owner is told, and OnReady is not called.
    void PushDirectory();                    // 0x0002A450
    void PopDirectory();                     // the same body as AtImageDatabase's at 0x00029F40

    static void SetHeap(void* heap);         // 0x0002A010

    AtMatAssignment m_materials;             // +0x14
    unsigned int m_count;                    // +0x1C
    AtIoFileSystem* m_fileSystem;            // +0x20
    char* m_directory;                       // +0x24

    static unsigned int s_demandLoadCount;   // 0x002C636C
    static void* s_heap;                     // 0x002C6370
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtImageItem, m_flags) == 0x0C, "AtImageItem::m_flags");
static_assert(offsetof(AtFontItem, m_flags) == 0x0C, "AtFontItem::m_flags");
static_assert(offsetof(AtImageDatabase, m_materials) == 0x14, "AtImageDatabase::m_materials");
static_assert(offsetof(AtImageDatabase, m_directory) == 0x24, "AtImageDatabase::m_directory");
static_assert(sizeof(AtImageDatabase) == 0x28, "AtImageDatabase");
static_assert(offsetof(AtFontDatabase, m_directory) == 0x24, "AtFontDatabase::m_directory");
static_assert(sizeof(AtFontDatabase) == 0x28, "AtFontDatabase");
#pragma pop_macro("offsetof")
#endif
