// A level of the Flash user interface, as far as its .AFL file goes ("Atlas Flash Level Definition File" by its own first line). The keyword names are the original's, from its table at 0x00270E4C; other names are inferred.
// Only the reader is recovered. The class starts at about 0x00011440 and its .AFD reader, 0x00011770, calls this one; the rest of it is not known here.
#pragma once

// Two members of the level take the heap this way. The setter is at 0x00058AF0, one store, in an address range held by another worker at the time of writing: this is a seam for whoever recovers that class.
class AtFlashHeapUser {
public:
    void SetHeap(void* heap);          // 0x00058AF0 not defined here

    char m_unknown00[0x24];
    void* m_heap;                      // +0x24
};

class AtFlashLevel {
public:
    // 0x000114A0, name + ".AFL". Sets defaults, reads, and hands out a heap. The name is copied first, to eight bytes. A HeapSize in the file is read and thrown away, so the file cannot give the level
    // a heap of its own: the branch that would make one is never taken. The second argument is not read.
    void Load(const char* name, int unused);

    char m_unknown000[0x150];
    void* m_heap150;                   // +0x150
    AtFlashHeapUser m_first;           // +0x154
    char m_unknown17C[0x1DC - 0x17C];
    void* m_heap1DC;                   // +0x1DC
    char m_unknown1E0[0x204 - 0x1E0];
    void* m_heap204;                   // +0x204
    char m_unknown208[0x248 - 0x208];
    AtFlashHeapUser m_second;          // +0x248
    char m_unknown270[0x32C - 0x270];
    unsigned int m_heapSize;           // +0x32C always 0 after Load
    int m_unknown330;                  // +0x330 20000; nothing here reads it
    int m_maxVariableValueLength;      // +0x334 1000 by default
    int m_maxVariables;                // +0x338 10000 by default
    int m_fontSourceLevel;             // +0x33C -1 by default and for the word Self, -2 for All
    char m_unknown340[0x34C - 0x340];
    void* m_heapInUse;                 // +0x34C
    void* m_block;                     // +0x350 what was taken for the heap of its own, from the heap it fell back on
    void* m_heap;                      // +0x354 kept if already set; otherwise AtHeapStack::s_contentHeap
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFlashLevel, m_first) == 0x154, "AtFlashLevel::m_first");
static_assert(offsetof(AtFlashLevel, m_heap1DC) == 0x1DC, "AtFlashLevel::m_heap1DC");
static_assert(offsetof(AtFlashLevel, m_second) == 0x248, "AtFlashLevel::m_second");
static_assert(offsetof(AtFlashLevel, m_heapSize) == 0x32C, "AtFlashLevel::m_heapSize");
static_assert(offsetof(AtFlashLevel, m_heap) == 0x354, "AtFlashLevel::m_heap");
#pragma pop_macro("offsetof")
#endif
