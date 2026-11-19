// The Flash user interface's texts: Text\name.AFT ("Atlas Flash Text Definition File" by its own first line), each text naming a string, a font and a text format, with the strings in Text\name.AIS, the formats in
// Text\name.AFX and the fonts in Text\Font.AGF when there is one. Original address clusters 0x00013870-0x00013DEF and 0x0001FBC0-0x0002027F, 0x00020570-0x0002061F. The keywords are the original's, from table
// 0x00271280; every other name is inferred.
#pragma once

#include "at/AtFlashShape.h"
#include "at/AtText.h"

class AtFontDatabase;
class AtFlashFontTable;

// What the factory's slot 9 makes for the fonts, and slot 10 frees. Only its slot 6 is called here: it is given the owner, the name "Font" and the mode. Nothing here says what it is, and nothing about it is recovered.
class AtFlashFontLoader {
public:
    virtual void Slot0() {}
    virtual void Slot1() {}
    virtual void Slot2() {}
    virtual void Slot3() {}
    virtual void Slot4() {}
    virtual void Slot5() {}
    virtual void Load(AtMaterialOwner* owner, const char* name, int mode) = 0;  // slot 6
};

// One Text: 0x50 bytes. Vftable 0x00272750, one slot.
class AtFlashText : public AtFlashResource {
public:
    AtFlashText();                     // 0x0001FCB0
    ~AtFlashText();                    // 0x0001FE20 not virtual
    void Read(AtIoTokenizer* tokenizer) override;  // slot 0, 0x0001FD30 ReadData, and noted as read
    void ReadData(AtIoTokenizer* tokenizer);       // 0x0001FBC0 "Text n { String Font2D TextFormat Variable }" in any order; any other word is reported and reading goes on
    void Reset();                      // 0x0001FD50 nothing unless read
    // 0x0001FC70. Gives back the text format, and the font unless +0x28 says not to.
    void Release(AtFlashResourceTable* formats, AtFlashFontTable* fonts);

    int m_read;                        // +0x1C
    int m_unknown20;                   // +0x20
    char* m_variable;                  // +0x24 a new copy, or null
    int m_unknown28;                   // +0x28 not 0 keeps the font when released
    char m_string[8];                  // +0x2C the name of a string in the .AIS, not always terminated
    int m_unknown34;                   // +0x34
    char m_font[8];                    // +0x38 the name of a font in the .AGF, not always terminated
    int m_unknown40;                   // +0x40
    int m_format;                      // +0x44 -1 until read
    int m_unknown48;                   // +0x48
    void* m_heap;                      // +0x4C
};

// Vftable 0x0027275C. The fonts of a level's texts: Text\name.AGF, when there is one. Only Load's name "Font" is ever given.
class AtFlashFontTable : public AtFlashResourceTable {
public:
    struct FontRef {
        unsigned char m_references;    // +0x00
        void* m_font;                  // +0x04
    };

    AtFlashFontTable();                // 0x0001FE70
    ~AtFlashFontTable() override;      // slot 1; 0x00020260 deleting, 0x00020060 complete

    void ClearFonts();                 // 0x0001FEA0 nothing unless loaded; reports a font still held, and frees all the same
    void Load(const char* name, int mode, AtMaterialOwner* owner);  // 0x000200B0
    // 0x0001FFE0. 0 if the database has no font of that name. A demand-loaded font is counted down, and unloaded at none. Not found among the references, it is the first one's count that goes down.
    int ReleaseFont(const char* name);

    int m_loaded;                      // +0x28
    AtFontDatabase* m_fonts;           // +0x2C the factory's slot 19
    AtFlashFontLoader* m_fontLoader;   // +0x30 the factory's slot 9
    AtMaterialOwner* m_owner;          // +0x34
    AtFlashGraphicsFactory* m_factory; // +0x38
    FontRef* m_refs;                   // +0x3C one per font of the database
    unsigned int m_refCount;           // +0x40
};

// Vftable 0x002712D8. Text\name.AFT with its strings, formats and fonts.
class AtFlashTextTable : public AtFlashResourceTable {
public:
    AtFlashTextTable();                // 0x00013870
    void Allocate(unsigned int count) override;  // slot 0, 0x00013B40
    ~AtFlashTextTable() override;      // slot 1; 0x00013DD0 deleting, 0x00013C30 complete
    void Release(unsigned int index) override;   // slot 2, 0x00013B00 the last reference gives back the text's format and font

    void ClearTexts();                 // 0x000138F0 nothing unless loaded
    void ReadFile(const char* name);   // 0x000139A0 name + ".AFT"
    // 0x00013CB0. The formats (.AFX), the fonts (Font.AGF), then the texts (.AFT) and their strings (.AIS), all in "Text". The fourth argument is not read.
    void Load(const char* name, int mode, AtMaterialOwner* owner, int ownFonts);

    int m_loaded;                      // +0x28
    int m_unknown2C;                   // +0x2C nothing here touches it
    AtTextTable m_strings;             // +0x30
    AtFlashFontTable m_fonts;          // +0x64
    AtFlashTextFormatTable m_formats;  // +0xA8
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFlashText, m_variable) == 0x24, "AtFlashText::m_variable");
static_assert(offsetof(AtFlashText, m_font) == 0x38, "AtFlashText::m_font");
static_assert(offsetof(AtFlashText, m_heap) == 0x4C, "AtFlashText::m_heap");
static_assert(sizeof(AtFlashText) == 0x50, "AtFlashText");
static_assert(sizeof(AtFlashFontTable::FontRef) == 8, "AtFlashFontTable::FontRef");
static_assert(offsetof(AtFlashFontTable, m_refCount) == 0x40, "AtFlashFontTable::m_refCount");
static_assert(sizeof(AtFlashFontTable) == 0x44, "AtFlashFontTable");
static_assert(offsetof(AtFlashTextTable, m_strings) == 0x30, "AtFlashTextTable::m_strings");
static_assert(offsetof(AtFlashTextTable, m_fonts) == 0x64, "AtFlashTextTable::m_fonts");
static_assert(offsetof(AtFlashTextTable, m_formats) == 0xA8, "AtFlashTextTable::m_formats");
static_assert(sizeof(AtFlashTextTable) == 0xD4, "AtFlashTextTable");
#pragma pop_macro("offsetof")
#endif
