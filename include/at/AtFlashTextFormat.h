// Text formats of the Flash user interface: .AFX files ("Atlas Flash Text Format Definition File" by their own first line). Original address cluster 0x00016FB0-0x000173BF, 0x00020280-0x0002063F, 0x00023400-0x0002345F
// and 0x00030270-0x0003056F. The keyword names are the original's, from its table at 0x002727B0; other names are inferred. How a format is drawn (0x000307A0, on a sibling class) is not recovered.
#pragma once

#include "at/AtIoTokenizer.h"

// Vftable 0x00272D28, one slot. What the tables hold; its numbered references and chain link are used by siblings not recovered here.
class AtFlashResource {
public:
    AtFlashResource();                 // 0x00023420
    ~AtFlashResource();                // 0x00023440 not virtual
    virtual void Read(AtIoTokenizer* tokenizer);  // slot 0, folded empty body at 0x000CBF30
    void ResetLinks();                 // 0x00023400 what the destructor does too

    void* m_groupNext;                 // +0x04 a group's list runs through here; RecycleGroup finds the resource from it
    void* m_groupPrev;                 // +0x08
    int m_index;                       // +0x0C -1 until read
    int m_references;                  // +0x10
    int m_uses;                        // +0x14 what the group holds of it
    void* m_table;                     // +0x18
};

// One TextFormat, read from the file: 0x6C bytes. Vftable 0x002749D0, one slot.
class AtFlashTextFormat : public AtFlashResource {
public:
    enum Alignment { kAlignmentLeft = 0, kAlignmentRight = 1, kAlignmentCenter = 2, kAlignmentJustify = 3 };

    AtFlashTextFormat();               // 0x000304A0
    ~AtFlashTextFormat();              // 0x00030510 not virtual
    void Read(AtIoTokenizer* tokenizer) override;  // slot 0, 0x000304F0
    void ReadFields(AtIoTokenizer* tokenizer);     // 0x00030270 what a file says; words it omits keep their value
    void Reset();                      // 0x00030440

    int m_read;                        // +0x1C
    float m_bounds[4];                 // +0x20 left, top, right, bottom by the look of it; the file's BoundsRectangle
    int m_wordWrap;                    // +0x30
    int m_multiLine;                   // +0x34
    int m_password;                    // +0x38
    int m_readOnly;                    // +0x3C
    int m_noSelect;                    // +0x40
    int m_border;                      // +0x44
    float m_fontHeight;                // +0x48 12 by default
    unsigned char m_color[4];          // +0x4C white by default, red, green, blue, alpha in the file's order
    int m_textMaxLength;               // +0x50 -1 by default
    int m_alignment;                   // +0x54
    float m_leftMargin;                // +0x58
    float m_rightMargin;               // +0x5C
    float m_indent;                    // +0x60
    float m_leading;                   // +0x64
    void* m_heap;                      // +0x68
};

// Vftable 0x00271F78. Resources by index, with their storage in groups. What adds to a group, 0x00016F50, 0x00017030 and 0x00017050, is not recovered.
class AtFlashResourceTable {
public:
    struct Group {
        Group* m_next;                 // +0x00
        Group* m_prev;                 // +0x04
        void* m_first;                 // +0x08
        void* m_last;                  // +0x0C
    };

    AtFlashResourceTable();            // 0x000172A0
    virtual void Allocate(unsigned int count);   // slot 0, 0x00016FB0 a cleared list of that many
    virtual ~AtFlashResourceTable();   // slot 1; 0x000173A0 deleting, 0x00017360 complete
    virtual void Release(unsigned int index);    // slot 2, 0x00017040 one reference fewer
    virtual void Slot3(int unused);    // slot 3, 0x00017290 RecycleGroup(1)

    void Clear();                      // 0x000172E0 nothing unless m_initialised
    void Init();                       // 0x00017370 clears, if initialised, and makes the 6 groups
    // 0x000170E0. The first group gives up its members, each Released as often as it was counted in, and goes last if keep, or is freed. A table with no groups crashes.
    void RecycleGroup(int keep);
    void SetGroupCount(unsigned int count);      // 0x000171B0 adds at the end, frees from the front
    void SetHeap(void* heap);          // 0x00058AF0 folded with others that store at +0x24

    static void SetDefaultHeap(void* heap);      // 0x00016F40

    int m_initialised;                 // +0x04
    Group* m_firstGroup;               // +0x08
    Group* m_lastGroup;                // +0x0C
    char m_name[8];                    // +0x10 "NotInitd" until loaded
    AtFlashResource** m_items;         // +0x18
    unsigned int m_itemCount;          // +0x1C
    unsigned int m_groupCount;         // +0x20
    void* m_heap;                      // +0x24 null means s_defaultHeap

    static void* s_defaultHeap;        // 0x002C6270
};

// Vftable 0x0027294C.
class AtFlashTextFormatTable : public AtFlashResourceTable {
public:
    AtFlashTextFormatTable();          // 0x00020280
    void Allocate(unsigned int count) override;  // slot 0, 0x00020430 the formats, one block, and the list pointing into it
    ~AtFlashTextFormatTable() override;  // slot 1; 0x00020620 deleting, 0x00020520 complete

    void ReleaseFormats();             // 0x000202A0 nothing unless m_loaded
    void Read(const char* name);       // 0x00020310 name + ".AFX"; a count of 0 makes no list
    void Load(const char* name, int unused);  // 0x00020570 Read inside a directory "Text"; in src/at/AtFlashText.cpp

    int m_loaded;                      // +0x28 set by Load
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFlashResource, m_index) == 0x0C, "AtFlashResource::m_index");
static_assert(sizeof(AtFlashResource) == 0x1C, "AtFlashResource");
static_assert(offsetof(AtFlashTextFormat, m_bounds) == 0x20, "AtFlashTextFormat::m_bounds");
static_assert(offsetof(AtFlashTextFormat, m_fontHeight) == 0x48, "AtFlashTextFormat::m_fontHeight");
static_assert(offsetof(AtFlashTextFormat, m_heap) == 0x68, "AtFlashTextFormat::m_heap");
static_assert(sizeof(AtFlashTextFormat) == 0x6C, "AtFlashTextFormat");
static_assert(offsetof(AtFlashResourceTable, m_name) == 0x10, "AtFlashResourceTable::m_name");
static_assert(offsetof(AtFlashResourceTable, m_heap) == 0x24, "AtFlashResourceTable::m_heap");
static_assert(sizeof(AtFlashTextFormatTable) == 0x2C, "AtFlashTextFormatTable");
#pragma pop_macro("offsetof")
#endif
