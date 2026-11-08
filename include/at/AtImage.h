// Images and fonts, as the bodies of .AGI and .AGF files read them. Original address clusters 0x00057DC0-0x00058A8C (images), 0x000414D0-0x00042193 (fonts and the colour rectangle) and
// 0x000FE870-0x0010015C (2D fonts), with 0x000412C0. "AtGrI2DFont" is the original's own name, from its messages at 0x0027B120 onward; every other name here is inferred. The words are the
// original's, from the databases' tables at 0x002B0838 and 0x002B08E0 (src/at/AtImageDatabase.cpp).
//
// These are the base classes. What makes an image or a font of the graphics device, the concrete classes at vftables 0x0028B9B8 and 0x0028B970, is not recovered: their slots stay pure here, as they
// are in the original's base vftables, and nothing in a real build fills them. One call into the graphics layer is made from a base class, slot 5 of an image (slot 13 of a 2D font, the same body):
// it goes through AtGrDevice below, a seam whose slots stop with a diagnostic.
#pragma once

#include "at/AtMatAssignment.h"

class AtImageDatabase;  // include/at/AtImageDatabase.h
class AtFontDatabase;

// Four colours, read in order: "ColorValueRectangle". 16 bytes.
struct AtColorRect {
    AtColorRect();                            // 0x00042170 all white
    void Read(AtIoTokenizer* tokenizer);      // 0x00042140 four colours of four integers

    AtColor m_colors[4];
};

// What slot 5 of an image hands a name to. Known only by the one slot called; not recovered. Slots stop with a diagnostic.
class AtGrNameSink {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual int TakeName(const char* name, int value);  // slot 12
};

// What slot 5 of an image is given: something of the graphics layer, known only by its slot 16. Not recovered. Slots stop with a diagnostic.
class AtGrDevice {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
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
    virtual AtGrNameSink* NameSink();  // slot 16
};

// One image. 0x3C bytes. Vftable 0x0027711C; its slots 0 to 3, 10 and 14 are pure there and filled by the concrete image, 0xF0 bytes, at 0x0028B9B8, which also replaces slots 5, 6, 9 and 11.
class AtImage {
public:
    enum Flags {
        kRead = 1, kColorRectangle = 2, kTransparent = 4, kDemandLoad = 8, kOwnsData = 0x10, kPlatformSource = 0x20, kFlipHorizontally = 0x40, kFlipVertically = 0x80
    };
    // m_format: where the pixels come from, in bits 9 to 11, and how often they change.
    enum Format {
        kFrequentModify = 0x10, kInfrequentModify = 0x20, kMpegMacroBlocks = 0x100,
        kSourceMask = 0xE00, kBmpSource = 0, kTgaSource = 0x200, kDdsSource = 0x400, kTm2Source = 0x600, kTplSource = 0x800
    };

    AtImage();                                // 0x00057E30

    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    // Slot 4, 0x00057DC0. Splits total into widths: each a power of two times the smallest (minimum, but at least 4), the largest that maximum allows, as long as one is wanted; the last is what is left,
    // rounded up the same way. Writes them to out and returns how many. this and unused are not read.
    virtual unsigned int SplitStrips(unsigned int minimum, unsigned int maximum, unsigned int total, unsigned int unused, unsigned int* out);
    // Slot 5, 0x00057FB0: device's slot 16, then that one's slot 12 with image's name and value. this is not read. One body with AtGrI2DFont's slot 13.
    virtual int PassName(AtGrDevice* device, const AtImage* image, int value);
    virtual ~AtImage();                       // slot 6; 0x00058A30 deleting, 0x00058650 complete. Both empty it as Free does.
    // Slot 7, 0x00057E80. Made with no file: as Read, with the flags given, and the source among them kept as the format.
    virtual void Init(AtMaterialOwner* owner, AtImageDatabase* database, const char* name, unsigned int flags);
    // Slot 8, 0x000586A0. The image's body, a left curly through its right curly. One read before is emptied first. name is the eight bytes the database read, not always terminated: all eight
    // are copied. Text that ends inside the body never leaves the loop, since each end is reported and read past, unless the report stops.
    virtual void Read(AtMaterialOwner* owner, AtImageDatabase* database, const char* name, AtIoTokenizer* tokenizer);
    virtual void Free();                      // slot 9, 0x00057F00 back to new, but for the colour rectangle
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 10
    virtual void FreeData();                  // slot 11, 0x00057ED0 the pixels, if they are this image's own
    virtual void SetTransparentColor(const AtColor* color);  // slot 12, 0x00058620 all four bytes
    virtual void ClearTransparentColor();     // slot 13, 0x00057E10 the flag only
    virtual void Slot14() = 0;

    // 0x00057FD0. One word of the body. owner is not read. A word not of an image is reported, PntSource among them: it has no case.
    void ReadWord(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, int token);

    char m_name[8];                 // +0x04
    unsigned int m_flags;           // +0x0C Flags
    unsigned int m_format;          // +0x10 Format
    unsigned short m_unknown14;     // +0x14
    unsigned short m_unknown16;     // +0x16
    AtMatAssignment* m_materials;   // +0x18 the database's list
    unsigned short m_material;      // +0x1C an index into it; 0xFFFF for none. Only one past its end is reported, and it is kept all the same.
    unsigned short m_unknown1E;     // +0x1E
    AtColorRect m_colorRectangle;   // +0x20
    void* m_data;                   // +0x30 pixels, freed on the heap's delete
    AtImageDatabase* m_database;    // +0x34
    AtColor m_transparent;          // +0x38 TransparentColor sets three bytes; alpha is 255 from new
};

// One character of a 2D font. 16 bytes.
struct AtGrI2DFontChar {
    unsigned int m_unknown00;
    unsigned short m_code;          // +0x04
    unsigned char m_unknown06[8];
    unsigned char m_space;          // +0x0E 1 for the code 0x20
    unsigned char m_newline;        // +0x0F 1 for the code 0x0A
};

// Where a font made with no file takes its characters: not recovered, known by what Allocate and BuildCharacterSet use. Its count is a difference of byte pointers.
class AtGrCharacterSource {
public:
    virtual unsigned short At(unsigned int index) = 0;  // slot 0
    unsigned int m_unknown04;
    const char* m_begin;            // +0x08
    const char* m_end;              // +0x0C
};

// One font. 0x40 bytes. Vftable 0x00276208, whose slots 0 to 4, 9 and 10 are pure. Slot 0 has a body all the same, which the 2D font calls for the words it does not take.
class AtFont {
public:
    enum Flags { kColorRectangle = 1, kColorRectangleCharX = 2, kColorRectangleCharY = 4, kDemandLoad = 8, kOwnsData = 0x10, kTransparent = 0x100 };

    AtFont();                                 // 0x00042050

    virtual void ReadWord(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, int token) = 0;  // slot 0, 0x000414D0
    virtual void BuildCharacterSet(AtGrCharacterSource* source) = 0;  // slot 1
    virtual int FindCharacter(unsigned short code) = 0;               // slot 2
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual ~AtFont();                        // slot 5; 0x000420F0 deleting, 0x000420C0 complete. Both empty it as Free does.
    // Slot 6, 0x00041610. Made with no file: as many characters as source holds, the flags given. With none, only the count is set. owner is not read.
    virtual void Allocate(AtMaterialOwner* owner, AtFontDatabase* database, const char* name, AtGrCharacterSource* source, unsigned int flags);
    // Slot 7, 0x000415A0. The font's body, as for an image; emptied first when it holds characters, through slot 8. Nothing marks it read.
    virtual void Read(AtMaterialOwner* owner, AtFontDatabase* database, const char* name, AtIoTokenizer* tokenizer);
    virtual void Free();                      // slot 8, 0x00042090 back to new, but for the colour rectangle
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 9
    virtual void FreeData() = 0;              // slot 10

    char m_name[8];                 // +0x04
    unsigned int m_flags;           // +0x0C Flags
    unsigned int m_count;           // +0x10 characters
    unsigned int m_unknown14;       // +0x14
    AtFontDatabase* m_database;     // +0x18
    AtMatAssignment* m_materials;   // +0x1C the database's list
    float m_kerning;                // +0x20
    float m_leading;                // +0x24
    float m_scaleX;                 // +0x28 1 from new
    float m_scaleY;                 // +0x2C 1 from new
    AtColorRect m_colorRectangle;   // +0x30
};

// A 2D font. 0x64 bytes. Vftable 0x00289820; slots 4, 9, 11 and 12 are pure there, and filled by the concrete font at 0x0028B970, which also replaces slots 5 and 8.
class AtGrI2DFont : public AtFont {
public:
    // m_format, as an image's but assigned whole: a source word drops the modify words given before it.
    enum Format { kFrequentModify = 0x10, kInfrequentModify = 0x20, kBmpSource = 0, kTgaSource = 0x200, kTm2Source = 0x600, kTplSource = 0x800 };

    AtGrI2DFont();                            // 0x000FEA00

    // Slot 0, 0x000FEA90. One word of the body; those of every font go to AtFont's.
    void ReadWord(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, int token) override;
    // Slot 1, 0x000FE880. A table of source's characters, on s_heap, in its order. The count must be set; a table already held is not freed.
    void BuildCharacterSet(AtGrCharacterSource* source) override;
    // Slot 2, 0x000FE950. The index of code, by halves, the table taken as sorted. A code not there gives 0, the first character, but for a newline, which gives -1.
    int FindCharacter(unsigned short code) override;
    // Slot 3, 0x000FF510: 2582 bytes, not recovered. Stops with a diagnostic.
    void Slot3() override;
    ~AtGrI2DFont() override;                  // slot 5; 0x00100140 deleting, 0x000FFF60 complete
    void Free() override;                     // slot 8, 0x000FEA50 its own fields only: the base's stay, the count among them
    // Slot 10, 0x000FEA30. The table, if it is the font's own; the pointer is left as it was, so a later Free frees it again.
    void FreeData() override;
    virtual void Slot11() = 0;
    virtual void Slot12() = 0;
    virtual int PassName(AtGrDevice* device, const AtGrI2DFont* font, int value);  // slot 13: the body of AtImage's slot 5, 0x00057FB0
    virtual void SetTransparentColor(const AtColor* color);  // slot 14, 0x000FFF30 all four bytes
    virtual void ClearTransparentColor();     // slot 15, 0x000FE9F0 the flag only

    static void SetHeap(void* heap);          // 0x000FE870

    unsigned int m_material;        // +0x40 an index into the list; 0xFFFFFFFF for none. As for an image, one past the end is reported and kept.
    unsigned int m_unknown44[5];    // +0x44
    unsigned int m_format;          // +0x58 Format
    AtGrI2DFontChar* m_chars;       // +0x5C m_count of them
    AtColor m_transparent;          // +0x60 not set from new

    static void* s_heap;            // 0x003B1FCC for m_chars
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtColorRect) == 0x10, "AtColorRect");
static_assert(offsetof(AtImage, m_name) == 0x04, "AtImage::m_name");
static_assert(offsetof(AtImage, m_flags) == 0x0C, "AtImage::m_flags");
static_assert(offsetof(AtImage, m_materials) == 0x18, "AtImage::m_materials");
static_assert(offsetof(AtImage, m_material) == 0x1C, "AtImage::m_material");
static_assert(offsetof(AtImage, m_colorRectangle) == 0x20, "AtImage::m_colorRectangle");
static_assert(offsetof(AtImage, m_data) == 0x30, "AtImage::m_data");
static_assert(offsetof(AtImage, m_database) == 0x34, "AtImage::m_database");
static_assert(offsetof(AtImage, m_transparent) == 0x38, "AtImage::m_transparent");
static_assert(sizeof(AtImage) == 0x3C, "AtImage");
static_assert(sizeof(AtGrI2DFontChar) == 0x10, "AtGrI2DFontChar");
static_assert(offsetof(AtGrI2DFontChar, m_space) == 0x0E, "AtGrI2DFontChar::m_space");
static_assert(offsetof(AtGrCharacterSource, m_begin) == 0x08, "AtGrCharacterSource::m_begin");
static_assert(offsetof(AtFont, m_count) == 0x10, "AtFont::m_count");
static_assert(offsetof(AtFont, m_database) == 0x18, "AtFont::m_database");
static_assert(offsetof(AtFont, m_kerning) == 0x20, "AtFont::m_kerning");
static_assert(offsetof(AtFont, m_colorRectangle) == 0x30, "AtFont::m_colorRectangle");
static_assert(sizeof(AtFont) == 0x40, "AtFont");
static_assert(offsetof(AtGrI2DFont, m_material) == 0x40, "AtGrI2DFont::m_material");
static_assert(offsetof(AtGrI2DFont, m_format) == 0x58, "AtGrI2DFont::m_format");
static_assert(offsetof(AtGrI2DFont, m_chars) == 0x5C, "AtGrI2DFont::m_chars");
static_assert(offsetof(AtGrI2DFont, m_transparent) == 0x60, "AtGrI2DFont::m_transparent");
static_assert(sizeof(AtGrI2DFont) == 0x64, "AtGrI2DFont");
#pragma pop_macro("offsetof")
#endif
