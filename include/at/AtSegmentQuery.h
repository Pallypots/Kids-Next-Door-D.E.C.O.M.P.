// The block an emitter holds when its text says LineSegIntersectTesting: a pool of queries, one to a particle, with their storage. Original address clusters 0x00055360-0x0005562F, 0x00057330-0x00057C6F and
// 0x00047DE0-0x0004806C, with 0x000481D0. All names are inferred, and these more loosely than most: "segment" and "query" rest only on that word and on what the emitter does with the pool, which is to make it. What the queries are
// for is in their later slots, not recovered. A slot that is not recovered stops with a diagnostic.
#pragma once

#include "at/AtScene.h"

// Vftable 0x00276F28. 0x90 bytes.
class AtQuery {
public:
    enum Flags { kStorageNotOwned = 2 };

    AtQuery();                 // 0x000555A0
    virtual void Reserve(unsigned int count);   // slot 0, 0x000553A0 its own storage, the first block cleared and the second not
    // Slot 1, 0x00055360. Use storage held elsewhere. Clears first only when it already has a count.
    virtual void Attach(unsigned int count, void* storage80, void* storage84);
    virtual ~AtQuery();        // slot 2; 0x00057440 vector deleting, 0x000555F0 complete
    virtual void Clear();      // slot 3, 0x000555B0 frees what it owns, then Reset

    void Reset();              // 0x00055490 leaves the word at +0x7C alone. Two pairs of pointers point into the object.

    static void SetHeap(void* heap);  // 0x000552A0

    unsigned int m_flags;      // +0x04
    float m_listA[3];          // +0x08
    float m_listB[3];          // +0x14
    float* m_headA;            // +0x20 m_listA
    float* m_headB;            // +0x24 m_listB
    int m_unknown28[6];        // +0x28
    float m_listC[3];          // +0x40
    float m_listD[3];          // +0x4C
    float* m_headC;            // +0x58 m_listC
    float* m_headD;            // +0x5C m_listD
    int m_unknown60[4];        // +0x60
    int m_unknown70;           // +0x70
    int m_unknown74;           // +0x74
    unsigned int m_count;      // +0x78
    int m_unknown7C;           // +0x7C
    void* m_storage80;         // +0x80
    void* m_storage84;         // +0x84
    int m_unknown88;           // +0x88
    int m_unknown8C;           // +0x8C

    static void* s_heap;       // 0x002C7F58
};

// Vftable 0x002770F4. 0x98 bytes.
class AtSegmentQuery : public AtQuery {
public:
    AtSegmentQuery();          // 0x00057B90
    void Reserve(unsigned int count) override;  // slot 0, 0x00057AA0
    ~AtSegmentQuery() override;  // slot 2; 0x00047FC0 vector deleting, 0x00057BF0 complete
    void Clear() override;     // slot 3, 0x00057BB0

    using AtQuery::Attach;     // Slot 1 is the base's.
    // 0x00057A70. Attach, not by its slot, and one more block of storage.
    void Attach(unsigned int count, void* storage80, void* storage84, void* storage90);

    void* m_storage90;         // +0x90
    int m_unknown94;           // +0x94
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
    AtQuery* m_queries;        // +0x08
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
    unsigned char* m_entries10;        // +0x38 0x10 bytes each
    int m_unknown3C;                   // +0x3C
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtQuery, m_headB) == 0x24, "AtQuery::m_headB");
static_assert(offsetof(AtQuery, m_headD) == 0x5C, "AtQuery::m_headD");
static_assert(offsetof(AtQuery, m_count) == 0x78, "AtQuery::m_count");
static_assert(offsetof(AtQuery, m_storage84) == 0x84, "AtQuery::m_storage84");
static_assert(sizeof(AtQuery) == 0x90, "AtQuery");
static_assert(offsetof(AtSegmentQuery, m_storage90) == 0x90, "AtSegmentQuery::m_storage90");
static_assert(sizeof(AtSegmentQuery) == 0x98, "AtSegmentQuery");
static_assert(offsetof(AtQueryPool, m_sphere) == 0x24, "AtQueryPool::m_sphere");
static_assert(sizeof(AtQueryPool) == 0x34, "AtQueryPool");
static_assert(offsetof(AtSegmentQueryPool, m_entries10) == 0x38, "AtSegmentQueryPool::m_entries10");
static_assert(sizeof(AtSegmentQueryPool) == 0x40, "AtSegmentQueryPool");
#pragma pop_macro("offsetof")
#endif
