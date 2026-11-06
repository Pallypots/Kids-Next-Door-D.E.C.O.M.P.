// The rumble effects, on top of the name map. All names are inferred.
#pragma once
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

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
static_assert(sizeof(GmRumbleFrame) == 12, "GmRumbleFrame");
static_assert(sizeof(GmRumbleEffect) == 0x14, "GmRumbleEffect");
static_assert(offsetof(GmRumbleEffects, m_effects) == 0x18, "GmRumbleEffects::m_effects");
static_assert(sizeof(GmRumbleEffects) == 0x20, "GmRumbleEffects");
#pragma pop_macro("offsetof")
#endif
