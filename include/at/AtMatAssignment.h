// Lists of materials by name, as read from .AGN files and within .AGI, .AGF and .ASL files. Original address clusters 0x000347C0-0x00034A75 and 0x00027B60-0x00027ED4, with 0x00018810, 0x00053570 and
// 0x0001A070. The words "MatAssignment" and "MatAssignmentWithNameSpace" are the original's, from its table at 0x002B06B0; other names are inferred. A name that is empty in a file gives a null entry;
// a name that no map holds gives null too, with no report.
#pragma once

#include "at/AtMaterial.h"

// One list. 8 bytes. Also a member of the image and font databases and of a lens flare.
struct AtMatAssignment {
    AtMatAssignment();                        // 0x00018810 folded with AtIoFileRef's, where it is recorded
    ~AtMatAssignment();                       // 0x00053570 also used to empty it; frees on a pointer, unlike the reads, which free on a count

    // 0x000348D0. "[ n ] { "name" ... }", looked up in ns, which may be empty or null. A list of zero left by an earlier read is not freed: its block stays until the destructor or a later one.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, const char* ns);
    // 0x000347C0. "[ n ] { "name" "space" ... }", each entry with its own name space, which may be empty.
    void ReadWithNameSpaces(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);
    void Copy(const AtMatAssignment& from);   // 0x000349D0

    static void SetHeap(void* heap);          // 0x00034710

    unsigned int m_count;   // +0x00
    void** m_items;         // +0x04

    static void* s_heap;    // 0x002C63E0
};

// What an .AGN file holds: named lists. Vftable 0x0027319C. Concrete: nothing derives from it that is known.
class AtMatAssignmentDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 3 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B06B0

    AtMatAssignmentDatabase();              // 0x00027D30
    ~AtMatAssignmentDatabase() override;    // slot 0; 0x0001A070 vector deleting, 0x00027E50 complete
    void Clear() override;                  // slot 1, 0x00027E10

    // 0x00027D50. name + ".AGN". Unlike its siblings this empties what it holds and sets the keywords itself, so Read does neither.
    void Load(AtMaterialOwner* owner, const char* name, int unused);
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer);  // 0x00027B70

    static void SetHeap(void* heap);        // 0x00027B60

    unsigned int m_count;      // +0x14
    AtMatAssignment* m_lists;  // +0x18

    static void* s_heap;       // 0x002C6350
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtMatAssignment) == 8, "AtMatAssignment");
static_assert(offsetof(AtMatAssignmentDatabase, m_lists) == 0x18, "AtMatAssignmentDatabase::m_lists");
static_assert(sizeof(AtMatAssignmentDatabase) == 0x1C, "AtMatAssignmentDatabase");
#pragma pop_macro("offsetof")
#endif
