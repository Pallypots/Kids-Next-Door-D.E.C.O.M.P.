// Text strings and the table that holds them: .AIS files, the game's text by name. All names here are inferred.
#pragma once
#include "at/AtIoFileSystem.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

const char* AtIoStatusText(int status);    // 0x000140D0 the meaning of a file status code
void AtReportFileError(int status, const char* path, const char* extension);   // the only use so far of 0x00014230 and 0x000141C0

// In AtIoFormatHost.cpp: the host's C library. The Xbox's wchar_t is 16 bits, a host's may not be, so wide text is narrowed first.
int AtNarrowToInt(const char* text);       // atoi
float AtNarrowToFloat(const char* text);   // atof, as float
int AtWideToInt(const unsigned short* text);    // _wtoi
float AtWideToFloat(const unsigned short* text);  // swscanf with %f

// vftable 0x0027514C: ten pure slots. A window on characters held elsewhere: m_start to m_end, with a terminator at m_end. No destructor in the vftable.
class AtText {
public:
    AtText();                                 // 0x000398F0
    void ClearFields();                       // 0x00031BA0
    unsigned int ComputeEnd();                // 0x000398B0 the first terminator from m_start, or m_capacity - 1

    virtual unsigned int CharAt(unsigned int index) = 0;            // slot 0
    virtual void SetChar(unsigned int index, unsigned int c) = 0;   // slot 1
    virtual void RemoveRange(unsigned int first, unsigned int last) = 0;  // slot 2 both ends included
    virtual void RemoveAt(unsigned int index) = 0;                  // slot 3
    virtual void Insert(unsigned int c, unsigned int index) = 0;    // slot 4 no bound
    virtual void Recount() = 0;                                     // slot 5
    virtual void CopyNarrow(char* out, unsigned int size) = 0;      // slot 6
    virtual void CopyWide(unsigned short* out, unsigned int size) = 0;  // slot 7
    virtual int ToInt() = 0;                                        // slot 8
    virtual float ToFloat() = 0;                                    // slot 9

    unsigned int m_capacity;   // +0x04
    unsigned int m_start;      // +0x08
    unsigned int m_end;        // +0x0C
};

// vftable 0x002748C0.
class AtTextUnicode : public AtText {
public:
    AtTextUnicode();                          // 0x0002DD90
    void Release();                           // 0x0002DFB0
    void Rewind();                            // 0x0002DD70
    void Init(unsigned short* data, unsigned int capacity);  // 0x0002DDE0 overwrites the last character with a terminator

    unsigned int CharAt(unsigned int index) override;           // 0x0002DD30
    void SetChar(unsigned int index, unsigned int c) override;  // 0x0002DD50
    void RemoveRange(unsigned int first, unsigned int last) override;  // 0x0002DE10
    void RemoveAt(unsigned int index) override;                 // 0x0002DE60
    void Insert(unsigned int c, unsigned int index) override;   // 0x0002DEA0
    void Recount() override;                                    // 0x0002DEF0
    void CopyNarrow(char* out, unsigned int size) override;     // 0x0002DF20 keeps the low byte of each
    void CopyWide(unsigned short* out, unsigned int size) override;  // 0x0002DF60
    int ToInt() override;                                       // 0x0002DDB0
    float ToFloat() override;                                   // 0x0002DDC0

    unsigned short* m_data;    // +0x10
};

// vftable 0x002748F0.
class AtTextAscii : public AtText {
public:
    AtTextAscii();                            // 0x0002E000
    void Init(char* data, unsigned int capacity);  // 0x0002E040

    unsigned int CharAt(unsigned int index) override;           // 0x0002DFC0
    void SetChar(unsigned int index, unsigned int c) override;  // 0x0002DFE0
    void RemoveRange(unsigned int first, unsigned int last) override;  // 0x0002E070
    void RemoveAt(unsigned int index) override;                 // 0x0002E0C0
    void Insert(unsigned int c, unsigned int index) override;   // 0x0002E100
    void Recount() override;                                    // 0x0002E150
    void CopyNarrow(char* out, unsigned int size) override;     // 0x0002E170
    void CopyWide(unsigned short* out, unsigned int size) override;  // 0x0002E1B0
    int ToInt() override;                                       // 0x0002E020
    float ToFloat() override;                                   // 0x0002E030

    char* m_data;              // +0x10
};

// vftable 0x002726B4. A map from names to strings, and the blocks that hold them. Strings of both kinds are 0x14 bytes.
class AtTextTable : public GmNameMap {
public:
    AtTextTable();                            // 0x0001F6B0
    ~AtTextTable() override;                  // slot 0; 0x0001FB70 complete, 0x0002E510 the vector deleting form
    void Clear() override;                    // slot 1, 0x0001FB10

    AtText* StringAt(unsigned int index);     // 0x0001F680
    void AllocateAscii(unsigned int strings, unsigned int chars);    // 0x0001F6E0
    void AllocateUnicode(unsigned int strings, unsigned int chars);  // 0x0001F560
    void Parse(AtIoContext* context, AtIoTokenizer* t);              // 0x0001F810 registers itself with the context
    void Load(AtIoContext* context, const char* path, int unused);   // 0x0001FA70 extension AIS

    void* m_chars;             // +0x14
    void* m_strings;           // +0x18
    int m_ascii;               // +0x1C
    int m_loaded;              // +0x20
    unsigned int m_charCount;  // +0x24
    unsigned int m_charsUsed;  // +0x28
    unsigned int m_stringCount;  // +0x2C
    unsigned int m_stringsUsed;  // +0x30

    static void* s_heap;       // 0x002C62F8
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtTextUnicode) == 0x14, "AtTextUnicode");
static_assert(sizeof(AtTextAscii) == 0x14, "AtTextAscii");
static_assert(offsetof(AtTextTable, m_stringsUsed) == 0x30, "AtTextTable::m_stringsUsed");
static_assert(sizeof(AtTextTable) == 0x34, "AtTextTable");
#pragma pop_macro("offsetof")
#endif
