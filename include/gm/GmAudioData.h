// The two small data files of the audio manager: volume presets (.GOV) and memory blocks (.GOB). PARTIAL: the manager itself is not recovered, only the fields these two fill, and those of the .GON notes
// (src/gm/GmSoundNotes.cpp). Its constructor is 0x0013AB50 and its vftable 0x0028D7D4.
// All names are inferred, from the keywords in the files.
#pragma once
#include "at/AtIoTokenizer.h"
#include "gm/GmSoundNotes.h"

class AtAudioDatabase;
struct AtNameTableOwner;

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

    enum { kFirstDatabases = 5, kDatabases = 60, kNoteBanks = 60 };

    // 0x00139090 extension GON. A file that will not open is reported and, if that comes back, crashes. No notes in it: the bank stays unused. The manager itself is not read: only the statics are.
    void LoadNotes(GmSoundNoteBank* bank, const char* path);
    void RegisterNotes(GmSoundNoteBank* bank);      // 0x00139550 finds each choice's segment, and enters the notes in the bank's maps. Once all a level's sound lists are in. A second call enters them twice
    GmSoundNoteBank* AllocateNoteBank();            // 0x00136930 the first free, or null
    void ReleaseNoteBank(GmSoundNoteBank* bank);    // 0x00136960
    void FreeNotes(GmSoundNoteBank* bank);          // 0x00136990 what a Segments block with a count of 0 allocated is not freed
    AtAudioSegment* FindSegment(const char* name);  // 0x00137D20 the first list that has it, of the first databases, then the rest
    void GetSegmentName(const AtAudioSegment* segment, char* out);  // 0x00137DE0 the same order; an empty name when absent
    // 0x00137EB0. Across the banks in use. bankName empty means any; null crashes once a bank is in use, and 0x00160237 passes null.
    GmSoundNote* FindNote(const char* name, const char* bankName);
    GmSoundNote* FindNoteByTrigger(const char* trigger, const char* bankName);  // 0x00137F00 the same, by AnimationTrigger

    char m_unknown000[0x24];
    AtAudioDatabase* m_firstDatabases[kFirstDatabases];  // +0x024 the capacity is what fits before the next
    AtAudioDatabase* m_databases[kDatabases];            // +0x038 those of the .GOD file's lists, 0x0013AF18
    char m_unknown128[0x18];
    unsigned int m_firstDatabaseCount;  // +0x140
    char m_unknown144[0x1C];
    GmVolumePreset* m_presets;   // +0x160
    unsigned int m_presetCount;  // +0x164
    char m_unknown168[0x170];
    GmAudioBlock m_blocks[100];  // +0x2D8 no bound is checked; the capacity is inferred from where the count sits
    char m_unknown918[4];
    int m_blockCount;            // +0x91C
    char m_unknown920[0x5D0];
    GmSoundNoteBank m_noteBanks[kNoteBanks];  // +0xEF0
    int m_noteBankUsed[kNoteBanks];           // +0x20C0

    static void* s_heap;         // 0x003B3548
    static GmAudioData* s_instance;           // 0x003B354C
    static AtNameTableOwner* s_soundOwner;    // 0x003B3558 its table at +0x90 holds the emitters by name
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
static_assert(offsetof(GmAudioData, m_firstDatabases) == 0x24, "GmAudioData::m_firstDatabases");
static_assert(offsetof(GmAudioData, m_databases) == 0x38, "GmAudioData::m_databases");
static_assert(offsetof(GmAudioData, m_firstDatabaseCount) == 0x140, "GmAudioData::m_firstDatabaseCount");
static_assert(offsetof(GmAudioData, m_noteBanks) == 0xEF0, "GmAudioData::m_noteBanks");
static_assert(offsetof(GmAudioData, m_noteBankUsed) == 0x20C0, "GmAudioData::m_noteBankUsed");
#pragma pop_macro("offsetof")
#endif
