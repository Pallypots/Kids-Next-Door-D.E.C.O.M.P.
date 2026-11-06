// A hash table from eight-byte names to objects, used across the engine, and the tree of namespaces that holds such tables. All names are inferred, except that the
// executable's own message calls the tree a "name space database".
#pragma once

int AtNamesEqual(const char* a, const char* b);   // 0x00013FC0 1 when equal, comparing at most eight bytes

class GmNameMap;
class GmNameSpaceDb;

// vftable 0x00272E60. Open addressing with linear probing. A slot is free when its value is null. Sizes come from a table of primes.
class GmNameMap {
public:
    struct Entry {
        char m_name[8];
        void* m_value;
    };

    GmNameMap();                                  // 0x00024C60
    virtual ~GmNameMap();                         // slot 0; 0x000252A0 deleting, 0x00025260 complete
    virtual void Clear();                         // slot 1, 0x00024EA0

    void Init(unsigned int count);                // 0x00024DA0 the first size in the table above `count`, or `count` past its end
    void Insert(const char* name, void* value);   // 0x00024EE0 grows when full; reports a duplicate and inserts it anyway
    void Insert(const char* name, void* value, unsigned int* index);  // 0x00024FC0
    void Remove(const char* name);                // 0x000250C0 and reinserts what follows it, to keep probing sound
    void* Find(const char* name);                 // 0x00025150
    void* Find(const char* name, unsigned int* index);  // 0x000251D0
    void GetName(unsigned int index, char* out);  // 0x00024C00
    void GetNameOf(const void* value, char* out);  // 0x00024BB0 linear; an empty name when absent
    unsigned int HashOf(const char* name);        // 0x00024C20
    void Grow();                                  // 0x00024C80

    Entry* m_entries;          // +0x04
    unsigned int m_size;       // +0x08
    GmNameMap* m_next;         // +0x0C in its group
    class GmNameMapGroup* m_group;  // +0x10

    static void* s_heap;       // 0x002C632C
};

// A namespace: 0x18 bytes, the maps registered under one name, searched newest first.
class GmNameMapGroup {
public:
    GmNameMapGroup();                  // 0x00017F30
    void Reset();                      // 0x00017F50
    void SetName(const char* name);    // 0x00017E60
    void* Find(const char* name);      // 0x00017ED0 the first map that has it
    void GetNameOf(const void* value, char* out);  // 0x00017F00
    void Add(GmNameMap* map);          // 0x00017F70 leaves any group it was in
    void MoveToFront(GmNameMap* map);  // 0x00017FA0 when the map belongs to a group of this name
    void Detach(GmNameMap* map);       // 0x00017E80 an emptied namespace goes back to its database

    char m_name[8];            // +0x00
    GmNameMap* m_first;        // +0x08
    int m_used;                // +0x0C
    GmNameMapGroup* m_nextFree;  // +0x10
    GmNameSpaceDb* m_database;   // +0x14
};

// vftable 0x0027211C. A map of namespaces by name, from a fixed pool.
class GmNameSpaceDb : public GmNameMap {
public:
    GmNameSpaceDb();                   // 0x00018040
    ~GmNameSpaceDb() override;         // slot 0; 0x00018320 deleting, 0x000182B0 complete
    void Clear() override;             // slot 1, 0x00018160

    void Init(unsigned int count);     // 0x00018060
    void Release(GmNameMapGroup* group);  // 0x00018010 back to the pool
    void Add(const char* space, GmNameMap* map);  // 0x00018190 reports running out of the pool, then crashes on it in the original
    static void Remove(GmNameMap* map);  // 0x000181F0
    void* Find(const char* space, const char* name);  // 0x00018210 caches the last namespace
    void GetNameOf(const char* space, const void* value, char* out);  // 0x00018260

    GmNameMapGroup* m_pool;    // +0x14
    unsigned int m_poolSize;   // +0x18
    GmNameMapGroup* m_free;    // +0x1C
    GmNameMapGroup* m_last;    // +0x20

    static void SetHeap(void* heap);   // 0x00018000
    static void* s_heap;       // 0x002C6274
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmNameMap) == 0x14, "GmNameMap");
static_assert(sizeof(GmNameMap::Entry) == 12, "GmNameMap::Entry");
static_assert(offsetof(GmNameMapGroup, m_database) == 0x14, "GmNameMapGroup::m_database");
static_assert(sizeof(GmNameMapGroup) == 0x18, "GmNameMapGroup");
static_assert(sizeof(GmNameSpaceDb) == 0x24, "GmNameSpaceDb");
#pragma pop_macro("offsetof")
#endif
