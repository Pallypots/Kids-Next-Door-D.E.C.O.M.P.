// Things drawn flat on the screen, images and text, as read by the loaders of ".ASI" and ".AST" text. Original address clusters 0x0003B030-0x0003B787, 0x00048E50-0x000497E5 and 0x00057C70-0x00057DA3. The words are the original's, from its
// tables at 0x002B18A8 and 0x002B1910; other names are inferred. No .ASD on the disc lists either kind, and the disc has no file of either among its text. Drawing them, 0x00048F00 and 0x00049330, is not recovered: each stops with a diagnostic.
//
//   NameSpace "x"                         optional
//   ImageDraw[n]  { ImageDraw "name" { Image "name" "ns"  Position x y  DrawArea w h  CenterHorizontally  CenterVertically  Scale x y  ClipRectangle a b c d  ColorValueRectangle ...16 integers...  NormalizedCoordinates } }
//   TextDraw[n]   { TextDraw "name" { Font "name" "ns"  String "name"  WrapWidth w  Kerning k  Position x y ...  the rest as above } }
//
// A file may hold more than one list, and only the first works. A second makes a new array, leaking the first's, and then the map's Init calls the list's own Clear, which frees the new array and sets the count to nothing:
// the second list's items are then reported word by word, and the list ends empty.
#pragma once

#include "at/AtMaterial.h"
#include "at/AtNameTableOwner.h"

class AtIoTokenizer;

// Vftable 0x00277108. 0x54 bytes.
class AtDrawItem {
public:
    enum Kind { kImage = 1, kText = 2 };
    enum Flags { kHasColors = 1, kCenterHorizontally = 2, kCenterVertically = 4, kNormalizedCoordinates = 8 };

    AtDrawItem();                                      // 0x00057D40
    virtual void ReadWord(AtIoTokenizer* tokenizer, int token);  // slot 0, 0x00057C70 reports it: a word no class takes
    virtual ~AtDrawItem();                             // slot 1; 0x00057D80 deleting, 0x00057D70 complete
    virtual void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);  // slot 2, 0x00057CE0 the body, a left curly through a right curly
    virtual void Clear();                              // slot 3, 0x00057D60
    virtual void Draw(void* target) = 0;               // slot 4

    void Reset();                                      // 0x00057C80

    int m_kind;                        // +0x04
    unsigned int m_flags;              // +0x08
    int m_unknown0C;                   // +0x0C
    float m_x;                         // +0x10
    float m_y;                         // +0x14
    float m_scaleX;                    // +0x18 starts as the device's scale
    float m_scaleY;                    // +0x1C
    float m_deviceScaleX;              // +0x20
    float m_deviceScaleY;              // +0x24
    float m_drawWidth;                 // +0x28
    float m_drawHeight;                // +0x2C
    AtNameTableOwner* m_owner;         // +0x30 what Clear looks at
    float m_clip[4];                   // +0x34 scaled by the device as it is read
    AtColorRectangle m_colors;         // +0x44
};

// Vftable 0x002764F0. 0x58 bytes.
class AtImageDraw : public AtDrawItem {
public:
    AtImageDraw();                                     // 0x00048E90
    ~AtImageDraw() override;                           // 0x00049230; 0x0003B040 vector deleting
    void ReadWord(AtIoTokenizer* tokenizer, int token) override;  // 0x000490A0
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) override;  // 0x00048EB0
    void Clear() override;                             // 0x00048EE0
    void Draw(void* target) override;                  // 0x00048F00 not recovered

    void* m_image;                     // +0x54 in the device's images. Null is not reported here.
};

// Vftable 0x00276504. 0x64 bytes. The position is scaled by the device as it is read, which an image's is not.
class AtTextDraw : public AtDrawItem {
public:
    AtTextDraw();                                      // 0x000492D0
    ~AtTextDraw() override;                            // 0x00049780; 0x0003B3D0 vector deleting
    void ReadWord(AtIoTokenizer* tokenizer, int token) override;  // 0x00049590
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) override;  // 0x00049560
    void Clear() override;                             // 0x00049300
    void Draw(void* target) override;                  // 0x00049330 not recovered

    void* m_font;                      // +0x54 in the device's fonts
    void* m_string;                    // +0x58 from every string table the context holds, with no name space
    float m_wrapWidth;                 // +0x5C
    float m_kerning;                   // +0x60 the largest real, 0x7F7FFFFF, until read
};

// Vftable 0x002752CC. 0x1C bytes. Each list goes on the first of the owner's two tables that sound also uses.
class AtImageDrawDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 11 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B18A8

    AtImageDrawDatabase();                  // 0x0003B0A0
    ~AtImageDrawDatabase() override;        // slot 0; 0x0002E2D0 vector deleting, 0x0003B290 complete
    void Clear() override;                  // slot 1, 0x0003B450 the same code as the text list's

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003B320 name + ".ASI"
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003B0C0

    static void SetHeap(void* heap);        // 0x0003B030

    AtImageDraw* m_draws;                   // +0x14
    unsigned int m_count;                   // +0x18

    static void* s_heap;                    // 0x002C6404
};

// Vftable 0x002752F0. 0x1C bytes.
class AtTextDrawDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 14 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B1910

    AtTextDrawDatabase();                   // 0x0003B430
    ~AtTextDrawDatabase() override;         // slot 0; 0x0002E3F0 vector deleting, 0x0003B660 complete
    void Clear() override;                  // slot 1, 0x0003B450

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003B6F0 name + ".AST"
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003B490

    static void SetHeap(void* heap);        // 0x0003B3C0

    AtTextDraw* m_draws;                    // +0x14
    unsigned int m_count;                   // +0x18

    static void* s_heap;                    // 0x002C6408
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtImageDraw) == 0x58, "AtImageDraw");
static_assert(sizeof(AtTextDraw) == 0x64, "AtTextDraw");
static_assert(offsetof(AtDrawItem, m_owner) == 0x30, "AtDrawItem::m_owner");
static_assert(offsetof(AtDrawItem, m_colors) == 0x44, "AtDrawItem::m_colors");
static_assert(offsetof(AtTextDraw, m_kerning) == 0x60, "AtTextDraw::m_kerning");
static_assert(sizeof(AtImageDrawDatabase) == 0x1C, "AtImageDrawDatabase");
static_assert(sizeof(AtTextDrawDatabase) == 0x1C, "AtTextDrawDatabase");
#pragma pop_macro("offsetof")
#endif
