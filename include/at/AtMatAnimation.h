// Materials that change with time, as read from .AGA files: frame numbers, the materials for them, and named runs through both. Original address clusters 0x00027EE0-0x000283D7, 0x00034A80-0x00034C17 and
// 0x00034C30-0x00035099, with 0x0001A190. "AtGrFrameNumberSet" is the original's own name, from its message at 0x00274B90; the words are from its table at 0x002B06D8; other names are inferred.
// The sets' siblings beside them (0x000350E0 on) are not recovered: the base class's Read is shared with .AGO and .AGH, and 0x00036541 calls it too.
#pragma once

#include "at/AtMatAssignment.h"

// A list read from a file, or named to be read from elsewhere. Vftable 0x00274BD4. No destructor in the vftable.
class AtGrKeySet {
public:
    enum Flags { kRead = 1, kHasData = 2, kNamed = 4 };

    AtGrKeySet();                   // 0x00034F70
    ~AtGrKeySet();                  // 0x00035070

    virtual void Allocate(unsigned int count) = 0;       // slot 0
    virtual void Free() = 0;                             // slot 1
    virtual void ReadEntries(AtIoTokenizer* tokenizer) = 0;  // slot 2
    virtual void Clear();                                // slot 3, 0x00035040
    virtual void Interpolate(unsigned int first, unsigned int count, float time, int length, int* from, int* to, float* t) = 0;  // slot 4
    virtual int Find(unsigned int first, unsigned int count, float time, int length) = 0;  // slot 5
    virtual void SetExternalName(const char* name);      // slot 6, 0x00034F40
    virtual void Load() = 0;                             // slot 7
    virtual unsigned short At(unsigned int index) = 0;   // slot 8

    // 0x00034F90. "[ n ] {", then its entries or, on the word with id 0x4B, a name, and "}". The word is in the table of none of this file's formats, so an .AGA cannot use it.
    void Read(AtIoTokenizer* tokenizer);
    void Reserve(unsigned int count);  // 0x00035010 a count of 0 allocates nothing, and is still marked read

    unsigned int m_flags;   // +0x04
    char m_name[8];         // +0x08 not always terminated
    unsigned int m_count;   // +0x10
};

// Frame numbers, as 16 bits each, in rising order within any run. Vftable 0x00274B68. No destructor in the vftable.
class AtGrFrameNumberSet : public AtGrKeySet {
public:
    AtGrFrameNumberSet();           // 0x00034CE0
    ~AtGrFrameNumberSet();          // 0x00034ED0

    void Allocate(unsigned int count) override;       // slot 0, 0x00034C30
    void Free() override;                             // slot 1, 0x00034C90
    void ReadEntries(AtIoTokenizer* tokenizer) override;  // slot 2, 0x00034CA0
    void Clear() override;                            // slot 3, 0x00034D00
    // Slot 4, 0x00034D20. The two keys of a run about a time, and how far along it is between them. Before the first or at or after the last, it is between the last and the first, across the loop's length.
    void Interpolate(unsigned int first, unsigned int count, float time, int length, int* from, int* to, float* t) override;
    // Slot 5, 0x00034E30. The last key at or before a time, in a run; the run's last outside it. length is not used.
    int Find(unsigned int first, unsigned int count, float time, int length) override;
    void Load() override;                             // slot 7, 0x00034EB0 stops if named: a frame number set is never demand loaded
    unsigned short At(unsigned int index) override;   // slot 8, 0x00034CD0

    static void SetHeap(void* heap);  // 0x00034F30

    unsigned short* m_frames;   // +0x14 not freed by Clear unless it holds a count; freed with no test for null

    static void* s_heap;        // 0x002C63E4
};

class AtMatAnimationDatabase;

// One named run. 0x1C bytes.
struct AtMatAnimation {
    AtMatAnimation();    // 0x00034A80
    ~AtMatAnimation();   // 0x00034AE0 also resets it

    // 0x00034B00. Its body, a left curly through its right curly. Bounds are reported, and reading goes on. A given loop period stands however Length follows it; with none Length sets it to one more than itself.
    void Read(AtMaterialOwner* owner, AtMatAnimationDatabase* database, AtIoTokenizer* tokenizer);
    // 0x00034AA0. Puts this run's material for a time in a slot of a list. Called from 0x0005621E.
    void Apply(AtMatAssignment* list, unsigned int slot, float time);

    AtMatAnimationDatabase* m_database;  // +0x00
    float m_rate;                // +0x04 30 from new
    unsigned int m_length;       // +0x08
    unsigned int m_loopPeriod;   // +0x0C
    unsigned int m_firstFrame;   // +0x10 in the database's frame numbers
    unsigned int m_firstKey;     // +0x14 in the database's keys
    unsigned int m_keyCount;     // +0x18
};

// Vftable 0x002731F8. Concrete. 0x3C bytes.
class AtMatAnimationDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 8 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B06D8

    AtMatAnimationDatabase();              // 0x00027F30
    ~AtMatAnimationDatabase() override;    // slot 0; 0x0001A190 vector deleting, 0x000282E0 complete
    void Clear() override;                 // slot 1, 0x00028280

    void Load(AtMaterialOwner* owner, const char* name, int unused);  // 0x00028340 name + ".AGA"
    // 0x00027F90. Sections in any order until the end of the text, at least one being tried even on an empty file. A second MaterialKey or MatAnimation is reported, and read over what is there, leaking it.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);

    static void SetHeap(void* heap);       // 0x00027EE0

    AtGrFrameNumberSet m_frameNumbers;     // +0x14
    unsigned int m_keyCount;               // +0x2C
    void** m_keys;                         // +0x30 materials
    unsigned int m_animationCount;         // +0x34
    AtMatAnimation* m_animations;          // +0x38

    static void* s_heap;                   // 0x002C6354
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtGrKeySet, m_count) == 0x10, "AtGrKeySet::m_count");
static_assert(sizeof(AtGrFrameNumberSet) == 0x18, "AtGrFrameNumberSet");
static_assert(sizeof(AtMatAnimation) == 0x1C, "AtMatAnimation");
static_assert(offsetof(AtMatAnimationDatabase, m_frameNumbers) == 0x14, "AtMatAnimationDatabase::m_frameNumbers");
static_assert(offsetof(AtMatAnimationDatabase, m_animations) == 0x38, "AtMatAnimationDatabase::m_animations");
static_assert(sizeof(AtMatAnimationDatabase) == 0x3C, "AtMatAnimationDatabase");
#pragma pop_macro("offsetof")
#endif
