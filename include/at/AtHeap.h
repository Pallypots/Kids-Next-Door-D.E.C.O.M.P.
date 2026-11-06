// The game's allocator: a manager carving named heaps out of one region, and first-fit heaps with coalescing free lists.
// The names are inferred; "equivalency", "fragments", "alignment" come from the original's own report.
//
// Portability: a block's header holds pointers, so its size differs on a 64-bit host, and every offset here is written through AtHeapBlock. On x86 these are the original's 8 and 0x10.
// Sizes keep the original's 24 bits, so a heap of 16 MB or more does not work, there as here.
#pragma once

struct AtHeapBlock {
    enum { kSizeMask = 0xFFFFFF, kBackOffset = 0x1000000, kUsed = 0x2000000, kTagShift = 26 };
    unsigned int m_word;       // +0x00 size, flags, and the tag of the owning heap
    AtHeapBlock* m_prev;       // +0x04 the block before it in memory
    // Only in a free block: an allocated block's data starts here.
    AtHeapBlock* m_nextFree;   // +0x08
    AtHeapBlock* m_prevFree;   // +0x0C
};

class AtHeap {
public:
    AtHeap();                  // 0x000168D0
    ~AtHeap();                 // 0x00016900; also how a heap is emptied
    void Init(void* base, unsigned int size, int tag, int alignShift);  // 0x00016930
    void Reset();              // 0x000164C0 one free block, the whole heap
    void* Allocate(unsigned int size);  // 0x00016520 null when nothing fits
    void* AllocateAligned(unsigned int size, unsigned int alignShift);  // 0x00016960 never less aligned than the heap
    void Free(void* data);     // 0x000166C0 the pointer is not checked
    int Owns(const void* data) const;   // 0x00016810 reads the bytes before data, so a foreign pointer inside the range can be misjudged
    unsigned int LargestFree() const;   // 0x00016880
    unsigned int FreeBlockCount() const;  // 0x000168B0

    char* m_base;              // +0x00
    AtHeapBlock* m_free;       // +0x04
    unsigned int m_size;       // +0x08 0 means this slot of the manager is unused
    unsigned int m_used;       // +0x0C
    unsigned int m_alignShift;  // +0x10
    unsigned int m_allocShift;  // +0x14
    int m_tag;                 // +0x18 the slot's index; checked by Owns
    int m_equivalencyId;       // +0x1C heaps with the same one stand in for each other
    int m_fixed;               // +0x20 memory was given, not carved from the manager's region
};

class AtHeapManager {
public:
    enum { kHeaps = 16 };
    struct Chunk {
        int m_size;
        Chunk* m_next;
        Chunk* m_prev;
    };

    AtHeapManager();           // 0x00012140
    void Init(void* base, unsigned int size, int unused);  // 0x00012180 a second call first forgets every heap; the third argument is not read
    void ResetAll();           // 0x00011C70
    int NextId();              // 0x00011CC0 the next not in use by any slot
    AtHeap* CreateHeap(unsigned int size, int alignShift);  // 0x00011D00 stops through AtFatal if it cannot
    AtHeap* CreateHeapAt(void* base, unsigned int size, int alignShift);  // 0x00011E40
    // 0x00011EB0. From the override if one is set, unless asked for the default heap; then any equivalent heap.
    void* Allocate(AtHeap* heap, unsigned int size);
    int Free(void* data);      // 0x00011F40 1 if some heap owned it: fixed heaps are asked first
    void MakeEquivalent(AtHeap* a, AtHeap* b);  // 0x00011FC0 a new id for both groups
    void DestroyHeap(AtHeap* heap);  // 0x000121D0
    void Report(char* out, unsigned int capacity);  // 0x000120A0
    // 0x00012010. Does not read the object. out holds a terminated string of *length characters, or the copy lands in the wrong place.
    void Append(char* out, const char* text, unsigned int* length, unsigned int capacity);

    char* m_base;              // +0x00
    unsigned int m_size;       // +0x04
    Chunk* m_chunks;           // +0x08 sorted by address
    AtHeap m_heaps[kHeaps];    // +0x0C
    AtHeap* m_override;        // +0x24C allocations go here while it is set; the loader installs one around each item
    int m_lastId;              // +0x250

    static AtHeapManager* s_instance;  // 0x002C60AC
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtHeapBlock) == 0x10, "AtHeapBlock");
static_assert(sizeof(AtHeap) == 0x24, "AtHeap");
static_assert(offsetof(AtHeap, m_equivalencyId) == 0x1C, "AtHeap::m_equivalencyId");
static_assert(offsetof(AtHeapManager, m_heaps) == 0xC, "AtHeapManager::m_heaps");
static_assert(offsetof(AtHeapManager, m_override) == 0x24C, "AtHeapManager::m_override");
static_assert(sizeof(AtHeapManager) == 0x254, "AtHeapManager");
#pragma pop_macro("offsetof")
#endif
