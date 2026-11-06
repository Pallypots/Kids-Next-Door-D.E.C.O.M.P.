// A hash table from eight-byte names to objects, used across the engine, and the rumble effects that sit on top of it. All names are inferred.
#pragma once
#include "at/AtIoTokenizer.h"

int AtNamesEqual(const char* a, const char* b);   // 0x00013FC0 1 when equal, comparing at most eight bytes

class GmNameMap;

// A list of maps that belongs to something larger, not recovered. Only what a map needs in order to leave it.
class GmNameMapGroup {
public:
    void Detach(GmNameMap* map);       // 0x00017E80
    void RemoveFromParent(GmNameMapGroup* parent);   // 0x00018010 NOT recovered: stops with a diagnostic in GmNamePending.cpp

    char m_unknown00[8];
    GmNameMap* m_first;                // +0x08
    char m_unknown0C[8];
    GmNameMapGroup* m_parent;          // +0x14
};

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
    unsigned int HashOf(const char* name);        // 0x00024C20
    void Grow();                                  // 0x00024C80

    Entry* m_entries;          // +0x04
    unsigned int m_size;       // +0x08
    GmNameMap* m_next;         // +0x0C in its group
    GmNameMapGroup* m_group;   // +0x10

    static void* s_heap;       // 0x002C632C
};

// One key of a rumble effect. Times in seconds and intensities from 0 to 1 by the comment in RUMBLE.RMB.
struct GmRumbleFrame {
    GmRumbleFrame();           // 0x001015C0
    float m_time;
    float m_left;
    float m_right;
};

class GmRumbleFrameArray {
public:
    void Resize(unsigned int count);    // 0x00074320
    void Assign(const GmRumbleFrame* frames, unsigned int count);  // 0x00074460

    GmRumbleFrame* m_items;
    unsigned int m_count;

    static void* s_heap;       // 0x002CF168
};

class GmRumbleEffect {
public:
    GmRumbleEffect();          // 0x000748E0
    ~GmRumbleEffect();         // 0x00074940
    GmRumbleEffect& operator=(const GmRumbleEffect& other);   // 0x000744B0
    void Init(const char* name, unsigned int frames);         // 0x000749B0
    GmRumbleFrame* Frame(unsigned int index);                 // 0x000748F0
    void SetFrame(unsigned int index, const GmRumbleFrame& frame);  // 0x00074900
    const char* Name();        // 0x000EB540 folded with other classes' accessors

    int m_ready;               // +0x00
    char m_name[8];            // +0x04
    GmRumbleFrameArray m_frames;   // +0x0C
};

class GmRumbleEffectArray {
public:
    void Resize(unsigned int count);    // 0x00074540
    void Free();                        // 0x000742E0

    GmRumbleEffect* m_items;
    unsigned int m_count;

    static void* s_heap;       // 0x002CF164
};

// Every effect in RUMBLE.RMB, by name. Who plays them, and how, is not recovered.
class GmRumbleEffects {
public:
    GmRumbleEffects();                  // 0x00074440
    ~GmRumbleEffects();                 // 0x00074710
    void Load(const char* path);        // 0x000748A0 replaces what was loaded
    void LoadFile(const char* path);    // 0x00074800 extension RMB; a file that will not open is not an error
    void Parse(AtIoTokenizer* t);       // 0x000747D0
    void ReadEffects(AtIoTokenizer* t);  // 0x000746A0
    void ReadEffect(AtIoTokenizer* t, GmRumbleEffect* effect);  // 0x00074220
    void ReadFrame(AtIoTokenizer* t, GmRumbleFrame* frame);     // 0x000741C0
    GmRumbleEffect* Find(const char* name);  // 0x000741B0

    int m_loaded;              // +0x00
    GmNameMap m_byName;        // +0x04
    GmRumbleEffectArray m_effects;  // +0x18
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmNameMap) == 0x14, "GmNameMap");
static_assert(sizeof(GmNameMap::Entry) == 12, "GmNameMap::Entry");
static_assert(offsetof(GmNameMapGroup, m_parent) == 0x14, "GmNameMapGroup::m_parent");
static_assert(sizeof(GmRumbleFrame) == 12, "GmRumbleFrame");
static_assert(sizeof(GmRumbleEffect) == 0x14, "GmRumbleEffect");
static_assert(offsetof(GmRumbleEffects, m_effects) == 0x18, "GmRumbleEffects::m_effects");
static_assert(sizeof(GmRumbleEffects) == 0x20, "GmRumbleEffects");
#pragma pop_macro("offsetof")
#endif
