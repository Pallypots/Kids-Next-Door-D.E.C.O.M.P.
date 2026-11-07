// What a segment test collects its answers in, and the block an emitter holds when its text says LineSegIntersectTesting: a pool of those, one to a particle, with their storage. Original address clusters 0x00055360-0x0005562F, 0x00057330-0x00057C6F and
// 0x00047DE0-0x0004806C, with 0x000481D0. All names are inferred. "Keys" and "hits" rest on what bounding geometry does with the list (src/at/AtBoundingGeometry.cpp); the pool's names rest only on that word and on what the emitter does with the pool, which is to make it.
// What a pool is for is in its later slots, not recovered. A slot that is not recovered stops with a diagnostic.
#pragma once

#include "at/AtScene.h"

class AtBoundingGeometry;

// What a segment test collects its answers in: keys in order and a hit to each. Vftable 0x00276F28. 0x90 bytes. What fills it is at 0x000552B0, AtSegmentQueryInsert in at/AtBoundingGeometry.h, not recovered.
class AtSegmentHitList {
public:
    AtSegmentHitList();                            // 0x000555A0
    virtual void Allocate(unsigned int capacity);  // slot 0, 0x000553A0 its own storage: the keys are zeroed, the hits are not
    // Slot 1, 0x00055360. Use storage held elsewhere, not owned: clearing does not free it. Clears first only when it already has a capacity.
    virtual void SetStorage(unsigned int capacity, void* keys, void* hits);
    virtual ~AtSegmentHitList();                   // slot 2, 0x00057440 deleting, for arrays too; 0x000555F0 complete
    virtual void Clear();                          // slot 3, 0x000555B0 frees what it owns, then Reset
    virtual void CountOne();                       // slot 4, 0x00055340 up to the capacity

    void Reset();                                  // 0x00055490 all but m_current. Two pairs of pointers point into the object.
    static void SetHeap(void* heap);               // 0x000552A0

    enum Flags { kNotOwned = 2 };
    enum { kKeySize = 8, kHitSize = 0x2C };

    unsigned int m_flags;              // +0x04
    AtVector3 m_vectors0[2];           // +0x08
    AtVector3* m_first0;               // +0x20 at m_vectors0[0]
    AtVector3* m_second0;              // +0x24 at m_vectors0[1]
    AtVector3 m_vectors1[2];           // +0x28
    AtVector3 m_vectors2[2];           // +0x40
    AtVector3* m_first2;               // +0x58 at m_vectors2[0]
    AtVector3* m_second2;              // +0x5C at m_vectors2[1]
    int m_unknown60[6];                // +0x60
    unsigned int m_capacity;           // +0x78
    unsigned int m_current;            // +0x7C the place the last key went in. Reset leaves it
    void* m_keys;                      // +0x80 eight bytes each
    void* m_hits;                      // +0x84 0x2C bytes each
    unsigned int m_count;              // +0x88
    int m_unknown8C;                   // +0x8C

    static void* s_heap;               // 0x002C7F58
};

// The derived class's array, the only part of it the code recovered touches. What a hit is otherwise is not established.
struct AtSegmentHit {
    int m_unknown00;                   // +0x00
    AtBoundingGeometry* m_geometry;    // +0x04
    unsigned int m_faces;              // +0x08 a box's, from AtBoundingBox::Clip
    int m_unknown0C;                   // +0x0C
};

// Vftable 0x002770F4. 0x98 bytes.
class AtSegmentQuery : public AtSegmentHitList {
public:
    AtSegmentQuery();                  // 0x00057B90
    void Allocate(unsigned int capacity) override;  // slot 0, 0x00057AA0
    ~AtSegmentQuery() override;        // slot 2; 0x00047FC0 vector deleting, 0x00057BF0 complete
    void Clear() override;             // slot 3, 0x00057BB0
    void CountOne() override;          // slot 4, 0x00057B10 not recovered

    using AtSegmentHitList::SetStorage;  // Slot 1 is the base's.
    // 0x00057A70. SetStorage, not by its slot, and one more block of storage.
    void SetStorage(unsigned int capacity, void* keys, void* hits, void* records);

    AtSegmentHit* m_records;           // +0x90
    int m_unknown94;                   // +0x94
};

// Vftable 0x002770D8. 0x34 bytes.
class AtQueryPool {
public:
    AtQueryPool();             // 0x000578A0
    virtual void Allocate();   // slot 0, 0x00057340 the queries, handing each its share of the two blocks
    virtual void Free();       // slot 1, 0x000574A0
    virtual void* TakeNext();  // slot 2, 0x000574F0 not recovered
    virtual void Update(void* argument);  // slot 3, 0x00057800 not recovered
    virtual ~AtQueryPool();    // slot 4; 0x00057A40 deleting, 0x00057940 complete

    // 0x00057950. Over any it made before. Each query has at least one of everything.
    void Init(unsigned int count, unsigned int perQuery);
    void Release();            // 0x000578E0 also what the destructors do. Free is virtual, so a destructor sees its own class's.

    static void SetHeap(void* heap);  // 0x00057330

    unsigned int m_count;      // +0x04
    AtSegmentHitList* m_queries;  // +0x08
    unsigned int m_perQuery;   // +0x0C
    unsigned char* m_entries8;     // +0x10 8 bytes each
    unsigned char* m_entries2C;    // +0x14 0x2C bytes each
    int m_unknown18;           // +0x18
    unsigned int m_made;       // +0x1C 1 once Init has run
    int m_unknown20;           // +0x20
    AtSceneSphere m_sphere;    // +0x24

    static void* s_heap;       // 0x002C7F64
};

// Vftable 0x002764D4. 0x40 bytes.
class AtSegmentQueryPool : public AtQueryPool {
public:
    AtSegmentQueryPool();      // 0x00047DE0
    void Allocate() override;  // slot 0, 0x00047E50
    void Free() override;      // slot 1, 0x00048020
    void* TakeNext() override; // slot 2, 0x00048070 not recovered
    void Update(void* argument) override;  // slot 3, 0x00048D10 not recovered
    ~AtSegmentQueryPool() override;  // slot 4; 0x000481D0 deleting, 0x00047E00 complete

    AtSegmentQuery* m_segmentQueries;  // +0x34
    unsigned char* m_entries10;        // +0x38 an AtSegmentHit each; 0x10 bytes in the original
    int m_unknown3C;                   // +0x3C
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtSegmentHitList, m_flags) == 0x04, "AtSegmentHitList::m_flags");
static_assert(offsetof(AtSegmentHitList, m_vectors0) == 0x08, "AtSegmentHitList::m_vectors0");
static_assert(offsetof(AtSegmentHitList, m_first0) == 0x20, "AtSegmentHitList::m_first0");
static_assert(offsetof(AtSegmentHitList, m_second0) == 0x24, "AtSegmentHitList::m_second0");
static_assert(offsetof(AtSegmentHitList, m_vectors1) == 0x28, "AtSegmentHitList::m_vectors1");
static_assert(offsetof(AtSegmentHitList, m_vectors2) == 0x40, "AtSegmentHitList::m_vectors2");
static_assert(offsetof(AtSegmentHitList, m_first2) == 0x58, "AtSegmentHitList::m_first2");
static_assert(offsetof(AtSegmentHitList, m_second2) == 0x5C, "AtSegmentHitList::m_second2");
static_assert(offsetof(AtSegmentHitList, m_unknown60) == 0x60, "AtSegmentHitList::m_unknown60");
static_assert(offsetof(AtSegmentHitList, m_capacity) == 0x78, "AtSegmentHitList::m_capacity");
static_assert(offsetof(AtSegmentHitList, m_current) == 0x7C, "AtSegmentHitList::m_current");
static_assert(offsetof(AtSegmentHitList, m_keys) == 0x80, "AtSegmentHitList::m_keys");
static_assert(offsetof(AtSegmentHitList, m_hits) == 0x84, "AtSegmentHitList::m_hits");
static_assert(offsetof(AtSegmentHitList, m_count) == 0x88, "AtSegmentHitList::m_count");
static_assert(offsetof(AtSegmentHitList, m_unknown8C) == 0x8C, "AtSegmentHitList::m_unknown8C");
static_assert(sizeof(AtSegmentHitList) == 0x90, "AtSegmentHitList");
static_assert(sizeof(AtSegmentHit) == 0x10, "AtSegmentHit");
static_assert(offsetof(AtSegmentQuery, m_records) == 0x90, "AtSegmentQuery::m_records");
static_assert(offsetof(AtSegmentQuery, m_unknown94) == 0x94, "AtSegmentQuery::m_unknown94");
static_assert(sizeof(AtSegmentQuery) == 0x98, "AtSegmentQuery");
static_assert(offsetof(AtQueryPool, m_sphere) == 0x24, "AtQueryPool::m_sphere");
static_assert(sizeof(AtQueryPool) == 0x34, "AtQueryPool");
static_assert(offsetof(AtSegmentQueryPool, m_entries10) == 0x38, "AtSegmentQueryPool::m_entries10");
static_assert(sizeof(AtSegmentQueryPool) == 0x40, "AtSegmentQueryPool");
#pragma pop_macro("offsetof")
#endif
