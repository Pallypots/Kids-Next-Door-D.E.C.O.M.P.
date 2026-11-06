// The two small data files of the audio manager: volume presets (.GOV) and memory blocks (.GOB). PARTIAL: the manager itself is not recovered, only the fields these two fill.
// All names are inferred, from the keywords in the files.
#pragma once
#include "at/AtIoTokenizer.h"

// The manager's source of audio memory, reached through 0x003B355C. Only slot 13 is known.
class GmAudioMemory {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;
    virtual void Slot11() = 0;
    virtual void Slot12() = 0;
    virtual void* Allocate(int where, int size, int id, int unused) = 0;  // slot 13; null skips the block

    static GmAudioMemory* s_instance;  // 0x003B355C
};

struct GmVolumePreset {
    GmVolumePreset();          // 0x00136DA0
    char m_name[8];
    float m_ambience;          // +0x08
    float m_sfx;               // +0x0C
    float m_voice;             // +0x10
    float m_music;             // +0x14
    float m_ui;                // +0x18
    float m_cinema;            // +0x1C
    float m_unknown20[10];     // +0x20 set to 1 and not read from the file
};

struct GmAudioBlock {
    int m_where;               // +0x00 0 primary, 1 secondary, 2 sound memory
    int m_id;                  // +0x04
    int m_size;                // +0x08
    void* m_memory;            // +0x0C
};

class GmAudioData {
public:
    void LoadVolumePresets(const char* path);  // 0x001370E0 extension GOV; a file that will not open is not an error
    void LoadMemoryBlocks(const char* path);   // 0x00136E70 extension GOB; only a section named XBOX counts, and this disc's file has none

    char m_unknown000[0x160];
    GmVolumePreset* m_presets;   // +0x160
    unsigned int m_presetCount;  // +0x164
    char m_unknown168[0x170];
    GmAudioBlock m_blocks[100];  // +0x2D8 no bound is checked; the capacity is inferred from where the count sits
    char m_unknown918[4];
    int m_blockCount;            // +0x91C

    static void* s_heap;         // 0x003B3548
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmVolumePreset) == 0x48, "GmVolumePreset");
static_assert(sizeof(GmAudioBlock) == 0x10, "GmAudioBlock");
static_assert(offsetof(GmAudioData, m_presets) == 0x160, "GmAudioData::m_presets");
static_assert(offsetof(GmAudioData, m_blocks) == 0x2D8, "GmAudioData::m_blocks");
static_assert(offsetof(GmAudioData, m_blockCount) == 0x91C, "GmAudioData::m_blockCount");
#pragma pop_macro("offsetof")
#endif
