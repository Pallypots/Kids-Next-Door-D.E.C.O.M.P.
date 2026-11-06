// AtHeap and AtHeapManager: the allocator. Original address clusters 0x00011C70-0x00012173, 0x00012180-0x0001227F, 0x000164C0-0x0001699F.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtHeap.h"

#include "at/AtIoTokenizer.h"

typedef decltype(sizeof(0)) size_t;
#define offsetof(t, m) __builtin_offsetof(t, m)

extern "C" {
size_t strlen(const char* text);
char* strcat(char* to, const char* from);
char* strncpy(char* to, const char* from, size_t count);
}

void AtFatal(const char* message, int a, int b, int c);
int AtFormat(char* out, const char* format, ...);  // Stands in for the library's sprintf: src/at/AtIoFormatHost.cpp

AtHeapManager* AtHeapManager::s_instance;

namespace {

// Where data begins in a block: after the header's first two fields, rounded up. x86: 8.
inline unsigned int Overhead(unsigned int mask) {
    return ((unsigned int)offsetof(AtHeapBlock, m_nextFree) + mask) & ~mask;
}

inline AtHeapBlock* BlockAt(void* p) {
    return (AtHeapBlock*)p;
}

inline void SetSize(AtHeapBlock* block, unsigned int size) {
    block->m_word = (block->m_word & ~(unsigned int)AtHeapBlock::kSizeMask) | (size & AtHeapBlock::kSizeMask);
}

inline unsigned int SizeOf(const AtHeapBlock* block) {
    return block->m_word & AtHeapBlock::kSizeMask;
}

inline void SetTag(AtHeapBlock* block, int tag) {
    block->m_word = (block->m_word & 0x3FFFFFF) | ((unsigned int)tag << AtHeapBlock::kTagShift);
}

}  // namespace

AtHeap::AtHeap() {
    m_base = 0;
    m_size = 0;
    m_used = 0;
    m_alignShift = 2;
    m_allocShift = 2;
    m_tag = 0;
    m_free = 0;
    m_equivalencyId = 0;
    m_fixed = 0;
}

AtHeap::~AtHeap() {
    m_base = 0;
    m_size = 0;
    m_used = 0;
    m_alignShift = 2;
    m_allocShift = 2;
    m_tag = 0;
    m_free = 0;
    m_equivalencyId = 0;
    m_fixed = 0;
}

void AtHeap::Init(void* base, unsigned int size, int tag, int alignShift) {
    m_base = (char*)base;
    // Port: a header holding 8-byte pointers needs 8-byte alignment. On x86 this changes nothing.
    if (sizeof(void*) > 4 && alignShift < 3) {
        alignShift = 3;
    }
    m_alignShift = (unsigned int)alignShift;
    m_allocShift = (unsigned int)alignShift;
    m_size = size;
    m_tag = tag;
    m_equivalencyId = tag;
    Reset();
}

void AtHeap::Reset() {
    m_free = BlockAt(m_base);
    m_used = 0;
    SetSize(m_free, m_size);
    m_free->m_word &= ~(unsigned int)(AtHeapBlock::kBackOffset | AtHeapBlock::kUsed);
    SetTag(m_free, m_tag);
    m_free->m_prev = 0;
    m_free->m_nextFree = 0;
    m_free->m_prevFree = 0;
}

void* AtHeap::Allocate(unsigned int size) {
    const unsigned int unit = 1u << m_alignShift;
    const unsigned int mask = unit - 1;
    unsigned int overhead = Overhead(mask);
    unsigned int plainOverhead = 0;
    const unsigned int rounded = (size + mask) & ~mask;
    if (m_allocShift != m_alignShift) {
        const unsigned int wide = 1u << m_allocShift;
        const unsigned int wideMask = wide - 1;
        plainOverhead = overhead;
        overhead = ((wide - unit) + wideMask + overhead + (unsigned int)offsetof(AtHeapBlock, m_nextFree)) & ~wideMask;
    }
    unsigned int need = rounded + overhead;
    if (need < sizeof(AtHeapBlock)) {
        need = sizeof(AtHeapBlock);
    }
    // First fit.
    AtHeapBlock* block = m_free;
    while (block != 0 && SizeOf(block) < need) {
        block = block->m_nextFree;
    }
    if (block == 0) {
        return 0;
    }
    const unsigned int have = SizeOf(block);
    unsigned int taken;
    if (need + sizeof(AtHeapBlock) < have) {
        // Split: the rest takes the block's place on the free list.
        AtHeapBlock* const after = BlockAt((char*)block + have);
        AtHeapBlock* const rest = BlockAt((char*)block + need);
        SetSize(block, need);
        rest->m_word = (have - need) & AtHeapBlock::kSizeMask;
        rest->m_nextFree = block->m_nextFree;
        rest->m_prevFree = block->m_prevFree;
        if (rest->m_nextFree != 0) {
            rest->m_nextFree->m_prevFree = rest;
        }
        if (m_free == block) {
            m_free = rest;
        } else if (rest->m_prevFree != 0) {
            rest->m_prevFree->m_nextFree = rest;
        }
        rest->m_prev = block;
        if ((char*)after < m_base + m_size) {
            after->m_prev = rest;
        }
        SetTag(rest, m_tag);
        taken = need;
    } else {
        if (m_free == block) {
            m_free = block->m_nextFree;
        } else {
            block->m_prevFree->m_nextFree = block->m_nextFree;
        }
        if (block->m_nextFree != 0) {
            block->m_nextFree->m_prevFree = block->m_prevFree;
        }
        taken = have;
    }
    m_used += taken;
    block->m_word |= AtHeapBlock::kUsed;
    SetTag(block, m_tag);
    if (m_allocShift == m_alignShift) {
        return (char*)block + overhead;
    }
    // Wider alignment: a marker just before the data says how far back the header is. Its top byte stays whatever was in the memory.
    const unsigned int wideMask = (1u << m_allocShift) - 1;
    const size_t aligned = ((size_t)block + overhead) & ~(size_t)wideMask;
    AtHeapBlock* const marker = BlockAt((char*)aligned - plainOverhead);
    marker->m_word = (marker->m_word & 0xFF000000) | ((unsigned int)((char*)marker - (char*)block) & AtHeapBlock::kSizeMask) | AtHeapBlock::kBackOffset;
    return (char*)marker + plainOverhead;
}

void AtHeap::Free(void* data) {
    const unsigned int overhead = Overhead((1u << m_alignShift) - 1);
    AtHeapBlock* block = BlockAt((char*)data - overhead);
    if (block->m_word & AtHeapBlock::kBackOffset) {
        block = BlockAt((char*)data - SizeOf(block) - overhead);
    }
    unsigned int size = SizeOf(block);
    block->m_word &= ~(unsigned int)AtHeapBlock::kUsed;
    m_used -= size;
    char* const end = m_base + m_size;
    int merged = 0;
    AtHeapBlock* next = BlockAt((char*)block + size);
    if ((char*)next < end && !(next->m_word & AtHeapBlock::kUsed)) {
        size += SizeOf(next);
        SetSize(block, size);
        block->m_nextFree = next->m_nextFree;
        block->m_prevFree = next->m_prevFree;
        if (m_free == next) {
            m_free = block;
        } else if (next->m_prevFree != 0) {
            next->m_prevFree->m_nextFree = block;
        }
        if (next->m_nextFree != 0) {
            next->m_nextFree->m_prevFree = block;
        }
        AtHeapBlock* const after = BlockAt((char*)block + size);
        if ((char*)after < end) {
            after->m_prev = block;
        }
        merged = 1;
    }
    AtHeapBlock* const before = block->m_prev;
    if (before != 0 && !(before->m_word & AtHeapBlock::kUsed)) {
        size += SizeOf(before);
        SetSize(before, size);
        if (merged) {
            // It went onto the list in the place of the block after it: now it leaves.
            if (m_free == block) {
                m_free = block->m_nextFree;
            } else {
                block->m_prevFree->m_nextFree = block->m_nextFree;
            }
            if (block->m_nextFree != 0) {
                block->m_nextFree->m_prevFree = block->m_prevFree;
            }
        }
        AtHeapBlock* const after = BlockAt((char*)before + size);
        if ((char*)after < end) {
            after->m_prev = before;
        }
        return;
    }
    if (!merged) {
        block->m_nextFree = m_free;
        block->m_prevFree = 0;
        if (m_free != 0) {
            m_free->m_prevFree = block;
        }
        m_free = block;
    }
}

int AtHeap::Owns(const void* data) const {
    const char* const p = (const char*)data;
    if (p < m_base || p >= m_base + m_size) {
        return 0;
    }
    const unsigned int overhead = Overhead((1u << m_alignShift) - 1);
    const AtHeapBlock* block = (const AtHeapBlock*)(p - overhead);
    if (block->m_word & AtHeapBlock::kBackOffset) {
        const unsigned int back = SizeOf(block);
        if ((back & 3) != 0 || back > 0x800) {
            return 0;
        }
        block = (const AtHeapBlock*)(p - back - overhead);
    }
    return (block->m_word >> AtHeapBlock::kTagShift) == (unsigned int)m_tag ? 1 : 0;
}

unsigned int AtHeap::LargestFree() const {
    unsigned int largest = 0;
    for (const AtHeapBlock* b = m_free; b != 0; b = b->m_nextFree) {
        if (SizeOf(b) > largest) {
            largest = SizeOf(b);
        }
    }
    return largest;
}

unsigned int AtHeap::FreeBlockCount() const {
    unsigned int count = 0;
    for (const AtHeapBlock* b = m_free; b != 0; b = b->m_nextFree) {
        count++;
    }
    return count;
}

void* AtHeap::AllocateAligned(unsigned int size, unsigned int alignShift) {
    const unsigned int saved = m_allocShift;
    m_allocShift = alignShift > m_alignShift ? alignShift : m_alignShift;
    void* const result = Allocate(size);
    m_allocShift = saved;
    return result;
}

AtHeapManager::AtHeapManager() {
    m_base = 0;
    m_chunks = 0;
    m_size = 0;
    m_lastId = 0;
    m_override = 0;
}

void AtHeapManager::Init(void* base, unsigned int size, int unused) {
    (void)unused;
    if (m_size != 0) {
        ResetAll();
    }
    char* const aligned = (char*)(((size_t)base + 0x3F) & ~(size_t)0x3F);
    m_chunks = (Chunk*)aligned;
    m_base = aligned;
    m_size = (unsigned int)((char*)base - aligned) + size;
    // The whole size, not what is left after aligning: a base that was not aligned gives a first chunk running past the end. As in the original.
    m_chunks->m_size = (int)size;
    m_chunks->m_next = 0;
    m_chunks->m_prev = 0;
}

void AtHeapManager::ResetAll() {
    for (int i = 0; i < kHeaps; i++) {
        if (m_heaps[i].m_size != 0) {
            m_heaps[i].~AtHeap();
        }
    }
    m_base = 0;
    m_chunks = 0;
    m_size = 0;
    m_lastId = 0;
    m_override = 0;
}

int AtHeapManager::NextId() {
    for (;;) {
        const int id = ++m_lastId;
        int used = 0;
        for (int i = 0; i < kHeaps; i++) {
            if (m_heaps[i].m_equivalencyId == id) {
                used = 1;
                break;
            }
        }
        if (!used) {
            return id;
        }
    }
}

AtHeap* AtHeapManager::CreateHeap(unsigned int size, int alignShift) {
    int need = (int)((size + 0x3F) & ~0x3Fu);
    int slot = 0;
    while (slot < kHeaps && m_heaps[slot].m_size != 0) {
        slot++;
    }
    if (slot == kHeaps) {
        AtFatal("Cannot allocate heap.\nToo many heap objects allocated.", 0, 0, 0);
    }
    Chunk* chunk = m_chunks;
    while (chunk != 0 && chunk->m_size < need) {
        chunk = chunk->m_next;
    }
    if (chunk == 0) {
        AtFatal("Cannot allocate heap.\nNot enough continuous memory left.", 0, 0, 0);
    }
    const unsigned int rest = (unsigned int)(chunk->m_size - need);
    if (rest > 0x40) {
        Chunk* const after = (Chunk*)((char*)chunk + need);
        after->m_size = (int)rest;
        after->m_next = chunk->m_next;
        if (chunk->m_next != 0) {
            chunk->m_next->m_prev = after;
        }
        after->m_prev = chunk;
        chunk->m_size = need;
        chunk->m_next = after;
    } else {
        need = chunk->m_size;
    }
    if (m_chunks == chunk) {
        m_chunks = chunk->m_next;
        if (m_chunks != 0) {
            m_chunks->m_prev = 0;
        }
    } else {
        chunk->m_prev->m_next = chunk->m_next;
    }
    if (chunk->m_next != 0) {
        chunk->m_next->m_prev = chunk->m_prev;
    }
    const int id = NextId();
    AtHeap* const heap = &m_heaps[slot];
    heap->Init(chunk, (unsigned int)need, slot, alignShift);
    heap->m_equivalencyId = id;
    heap->m_fixed = 0;
    return heap;
}

AtHeap* AtHeapManager::CreateHeapAt(void* base, unsigned int size, int alignShift) {
    int slot = 0;
    while (slot < kHeaps && m_heaps[slot].m_size != 0) {
        slot++;
    }
    if (slot == kHeaps) {
        AtFatal("Cannot allocate heap.\nToo many heap objects allocated.", 0, 0, 0);
    }
    const int id = NextId();
    AtHeap* const heap = &m_heaps[slot];
    heap->Init(base, size, slot, alignShift);
    heap->m_equivalencyId = id;
    heap->m_fixed = 1;
    return heap;
}

void* AtHeapManager::Allocate(AtHeap* heap, unsigned int size) {
    AtHeap* first;
    void* result;
    if (m_override != 0 && heap != (AtHeap*)AtIoTokenFile::s_heap) {
        first = m_override;
        result = first->AllocateAligned(size, heap->m_allocShift);
    } else {
        first = heap;
        result = first->Allocate(size);
    }
    if (result != 0) {
        return result;
    }
    for (int i = 0; i < kHeaps; i++) {
        AtHeap* const other = &m_heaps[i];
        if (other != first && other->m_size != 0 && other->m_equivalencyId == first->m_equivalencyId) {
            result = other->AllocateAligned(size, heap->m_allocShift);
            if (result != 0) {
                return result;
            }
        }
    }
    return 0;
}

int AtHeapManager::Free(void* data) {
    for (int pass = 1; pass >= 0; pass--) {
        for (int i = 0; i < kHeaps; i++) {
            AtHeap* const heap = &m_heaps[i];
            if (heap->m_size != 0 && (heap->m_fixed != 0) == (pass != 0) && heap->Owns(data)) {
                heap->Free(data);
                return 1;
            }
        }
    }
    return 0;
}

void AtHeapManager::MakeEquivalent(AtHeap* a, AtHeap* b) {
    const int id = NextId();
    int group = a->m_equivalencyId;
    for (int i = 0; i < kHeaps; i++) {
        if (m_heaps[i].m_equivalencyId == group) {
            m_heaps[i].m_equivalencyId = id;
        }
    }
    // Read only now: when both are of one group, b has the new id already, and this pass changes nothing.
    group = b->m_equivalencyId;
    for (int i = 0; i < kHeaps; i++) {
        if (m_heaps[i].m_equivalencyId == group) {
            m_heaps[i].m_equivalencyId = id;
        }
    }
}

void AtHeapManager::DestroyHeap(AtHeap* heap) {
    heap->m_equivalencyId = NextId();
    Chunk* const chunk = (Chunk*)heap->m_base;
    if ((char*)chunk >= m_base && (char*)chunk < m_base + m_size && heap->m_fixed == 0) {
        // Back into the sorted list, joining its neighbours.
        Chunk* before = 0;
        Chunk* after = m_chunks;
        while (after != 0 && (char*)chunk > (char*)after) {
            before = after;
            after = after->m_next;
        }
        chunk->m_size = (int)heap->m_size;
        chunk->m_next = after;
        chunk->m_prev = before;
        if (before == 0) {
            m_chunks = chunk;
        } else {
            before->m_next = chunk;
        }
        if (after != 0) {
            if ((char*)chunk + chunk->m_size == (char*)after) {
                chunk->m_next = after->m_next;
                chunk->m_size += after->m_size;
                if (after->m_next != 0) {
                    after->m_next->m_prev = chunk;
                }
            } else {
                after->m_prev = chunk;
            }
        }
        if (before != 0 && (char*)before + before->m_size == (char*)chunk) {
            before->m_next = chunk->m_next;
            before->m_size += chunk->m_size;
            if (chunk->m_next != 0) {
                chunk->m_next->m_prev = before;
            }
        }
    }
    heap->~AtHeap();
}

void AtHeapManager::Append(char* out, const char* text, unsigned int* length, unsigned int capacity) {
    if (*length >= capacity) {
        return;
    }
    const unsigned int total = *length + (unsigned int)strlen(text);
    if (total < capacity) {
        strcat(out, text);
        *length = total;
    } else {
        strncpy(out + *length, text, capacity - *length - 1);
        out[capacity - 1] = 0;
        *length = capacity;
    }
}

void AtHeapManager::Report(char* out, unsigned int capacity) {
    unsigned int length = 0;
    char line[256];
    line[0] = 0;
    out[0] = 0;
    for (int i = 0; i < kHeaps; i++) {
        const AtHeap& heap = m_heaps[i];
        if (heap.m_size != 0) {
            // The two zeros: the original keeps no such figures.
            AtFormat(line, "Heap %d:0x%08X %8d bytes Fragments: %4d Largest Free Block: %8d Total Free: %8d Max Allocated: %8d Largest Requested: %8d Alignment: %2d Equivalency ID: %2d\n", i,
                    (unsigned int)(size_t)heap.m_base, heap.m_size, heap.FreeBlockCount(), heap.LargestFree(), heap.m_size - heap.m_used, 0, 0, heap.m_alignShift, heap.m_equivalencyId);
            Append(out, line, &length, capacity);
        }
    }
}
