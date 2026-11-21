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
    float Duration();          // 0x00074930 the last frame's time; reads before the frames if there are none. src/gm/GmRumblePlayback.cpp
    unsigned int FrameCount(); // 0x000ED690 folded with other classes' accessors. src/gm/GmRumblePlayback.cpp

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

// One effect playing on a controller: 16 bytes, from a pool of ten each rumble player keeps (include/at/AtControllerManager.h). src/gm/GmRumblePlayback.cpp. Its start, 0x00073DD0, is
// not recovered.
class GmRumblePlayback {
public:
    void Stop();                        // 0x00073E00 no longer looping, and at the end
    void Advance(float seconds);        // 0x00073E20 a looping one past its end goes back by one length, once
    float Intensity(int motor);         // 0x00073E60 motor 1 the right, any other the left, of the last frame strictly before now; no blending
    int Finished();                     // 0x00073F00 1 if not looping and at or past the end
    void Clear();                       // 0x000B9A90 all four words; folded with other classes' bodies

    int m_unknown00;           // +0x00
    GmRumbleEffect* m_effect;  // +0x04
    float m_time;              // +0x08 seconds from the start
    int m_loop;                // +0x0C
};

// The object at 0x002CF15C, set by its constructor 0x00073F30 (not recovered): five words, of which two are known.
struct GmRumbleSystem {
    class AtControllerManager* m_manager;  // +0x00 told of the time by the controller module's update
    int m_unknown04;           // +0x04
    int m_unknown08;           // +0x08
    GmRumbleEffects* m_effects;  // +0x0C where 0x0006D750 finds effects by name
    int m_unknown10;           // +0x10

    static GmRumbleSystem* s_instance;  // 0x002CF15C src/gm/GmRumblePlayback.cpp
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmRumbleFrame) == 12, "GmRumbleFrame");
static_assert(sizeof(GmRumbleEffect) == 0x14, "GmRumbleEffect");
static_assert(offsetof(GmRumbleEffects, m_effects) == 0x18, "GmRumbleEffects::m_effects");
static_assert(sizeof(GmRumbleEffects) == 0x20, "GmRumbleEffects");
static_assert(sizeof(GmRumblePlayback) == 0x10, "GmRumblePlayback");
static_assert(sizeof(GmRumbleSystem) == 0x14, "GmRumbleSystem");
#pragma pop_macro("offsetof")
#endif
