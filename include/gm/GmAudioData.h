// The two small data files of the audio manager: volume presets (.GOV) and memory blocks (.GOB). PARTIAL: the manager itself is not recovered, only the fields these two fill, those of the .GON notes
// (src/gm/GmSoundNotes.cpp), and those of playing them and of the sound nodes (src/gm/GmSoundPlay.cpp, src/gm/GmSoundNodes.cpp). Its constructor is 0x0013AB50 and its vftable 0x0028D7D4.
// All names are inferred, from the keywords in the files.
#pragma once
#include "at/AtIoTokenizer.h"
#include "gm/GmSoundNotes.h"
#include "gm/GmSoundNodes.h"

class AtAudioBank;
class AtAudioDatabase;
class AtCoordSys;
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

    enum { kFirstDatabases = 5, kDatabases = 60, kNoteBanks = 60, kVoiceSlots = 40, kNodeBanks = 60, kSoundBanks = 10 };

    // 0x00139090 extension GON. A file that will not open is reported and, if that comes back, crashes. No notes in it: the bank stays unused. The manager itself is not read: only the statics are.
    void LoadNotes(GmSoundNoteBank* bank, const char* path);
    void RegisterNotes(GmSoundNoteBank* bank);      // 0x00139550 finds each choice's segment, and enters the notes in the bank's maps. Once all a level's sound lists are in. A second call enters them twice
    GmSoundNoteBank* AllocateNoteBank();            // 0x00136930 the first free, or null
    void RegisterDatabase(AtAudioDatabase* database);  // 0x00137AD0 into the first empty place of m_databases; when all 60 are taken, nowhere and no report. src/gm/GmDatabaseManifest.cpp
    void ReleaseNoteBank(GmSoundNoteBank* bank);    // 0x00136960
    void FreeNotes(GmSoundNoteBank* bank);          // 0x00136990 what a Segments block with a count of 0 allocated is not freed
    AtAudioSegment* FindSegment(const char* name);  // 0x00137D20 the first list that has it, of the first databases, then the rest
    void GetSegmentName(const AtAudioSegment* segment, char* out);  // 0x00137DE0 the same order; an empty name when absent
    // 0x00137EB0. Across the banks in use. bankName empty means any; null crashes once a bank is in use, and 0x00160237 passes null.
    GmSoundNote* FindNote(const char* name, const char* bankName);
    GmSoundNote* FindNoteByTrigger(const char* trigger, const char* bankName);  // 0x00137F00 the same, by AnimationTrigger

    // Playing. src/gm/GmSoundPlay.cpp. What plays is the emitter's slot 15, and what it reaches is behind the emitter, not here.
    // 0x001386D0. Stops the first segment of the note named by Stop, chooses a segment by weight with the game's random numbers, and plays it: on the note's emitter if it has one, otherwise
    // through the actor's dynamic nodes, or failing those at the actor through a voice slot. Nothing while the Conditional emitter has a voice.
    void PlayNote(GmActor* actor, GmSoundNote* note);
    void PlayOnEmitter(AtAudioEmitter* emitter, AtAudioSegment* segment);  // 0x00137F50 not with NoAudio; with no App object at all, it plays
    void PlayByName(const char* emitter, const char* segment);  // 0x00137FA0 an emitter of the sound owner's table, and a segment of the lists
    void PlayAtActor(GmActor* actor, AtAudioSegment* segment);  // 0x00138010 the first free voice slot with an emitter, placed at the actor; nothing if none is free
    AtAudioEmitter* TakeVoiceSlot();                 // 0x001388B0 round the forty from the last taken: a free one with an emitter and no voice playing
    void ReleaseVoiceSlot(AtAudioEmitter* emitter);  // 0x001367A0 stops everything on it and frees the slot
    void FindEmitters();                             // 0x00137660 the five named emitters, collsn1-5 and actor1-40, by name in the sound owner's table
    GmSoundNode* TakeDynamicNode(GmActor* actor, float unused);  // 0x00138F00 a node of the first bank with no owner
    void PlayNode(GmSoundNode* node, AtAudioSegment* segment);   // 0x00138F70 within range of the listener: plays on its emitter at the node's place. Out of range, a looping segment waits
    void StopBankVoices(AtAudioEmitter* emitter, AtAudioBank* bank);  // 0x00138290 stops each voice of that bank, and waits a tenth until none is left
    void RemoveDatabase(AtAudioDatabase* database);  // 0x00139F90 stops the voices of its banks on the named and actor emitters, and lets go of it. Not the first databases

    // Sound nodes, collision and instance sounds. src/gm/GmSoundNodes.cpp.
    GmSoundNodeBank* AllocateNodeBank();             // 0x001367E0 the first free, or null
    void ReleaseNodeBank(GmSoundNodeBank* bank);     // 0x00136810
    void LoadNodes(GmSoundNodeBank* bank, const char* path);  // 0x0013AA80 extension GOO. Each SoundNodes block replaces what an earlier one read, which leaks
    void ReadNodes(AtIoTokenizer* tokenizer, GmSoundNodeBank* bank);  // 0x00138A70 the count, then each Node block. The outer right curly is left to the caller
    void FreeNodes(GmSoundNodeBank* bank);           // 0x00138E50 stops each node's emitter and waits for it
    void RegisterNodes(GmSoundNodeBank* bank);       // 0x00138A10 finds each node's segment. A null bank crashes
    GmInstanceSoundBank* LoadInstanceSounds(const char* path);  // 0x00139C40 extension GOI; null when all ten banks are in use
    GmInstanceSoundBank* AllocateInstanceBank(unsigned int count);  // 0x00139A90
    void FreeInstanceSounds(GmInstanceSoundBank* bank);      // 0x00136B30
    void RegisterInstanceSounds(GmInstanceSoundBank* bank);  // 0x00136A10 each finds the collision sound of its name, the last bank's winning
    GmCollisionSoundBank* LoadCollisionSounds(const char* path);  // 0x00139610 extension GOC; null when all ten banks are in use
    void FreeCollisionSounds(GmCollisionSoundBank* bank);    // 0x00139B50

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
    char m_unknown920[0x3C4];
    AtAudioEmitter* m_ambience;  // +0xCE4 FindEmitters
    AtAudioEmitter* m_sfx;       // +0xCE8
    AtAudioEmitter* m_voice;     // +0xCEC
    AtAudioEmitter* m_music;     // +0xCF0
    AtAudioEmitter* m_ui;        // +0xCF4
    AtAudioEmitter* m_collisionEmitters[5];  // +0xCF8 collsn1-5
    GmSoundVoiceSlot m_voiceSlots[kVoiceSlots];  // +0xD0C
    unsigned int m_lastVoiceSlot;  // +0xEEC where TakeVoiceSlot goes round from
    GmSoundNoteBank m_noteBanks[kNoteBanks];  // +0xEF0
    int m_noteBankUsed[kNoteBanks];           // +0x20C0
    float m_impulseThresholds[3];             // +0x21B0 a collision sound's until its file gives its own
    GmSoundNodeBank m_nodeBanks[kNodeBanks];  // +0x21BC
    GmCollisionSoundBank m_collisionBanks[kSoundBanks];  // +0x248C
    GmInstanceSoundBank m_instanceBanks[kSoundBanks];    // +0x2504

    static void* s_heap;         // 0x003B3548
    static GmAudioData* s_instance;           // 0x003B354C
    static AtNameTableOwner* s_soundOwner;    // 0x003B3558 its table at +0x90 holds the emitters by name
    static AtCoordSys* s_listener;            // 0x003B3554 where the sound is heard from: PlayNode measures from it
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
static_assert(offsetof(GmAudioData, m_ambience) == 0xCE4, "GmAudioData::m_ambience");
static_assert(offsetof(GmAudioData, m_collisionEmitters) == 0xCF8, "GmAudioData::m_collisionEmitters");
static_assert(offsetof(GmAudioData, m_voiceSlots) == 0xD0C, "GmAudioData::m_voiceSlots");
static_assert(offsetof(GmAudioData, m_lastVoiceSlot) == 0xEEC, "GmAudioData::m_lastVoiceSlot");
static_assert(offsetof(GmAudioData, m_impulseThresholds) == 0x21B0, "GmAudioData::m_impulseThresholds");
static_assert(offsetof(GmAudioData, m_nodeBanks) == 0x21BC, "GmAudioData::m_nodeBanks");
static_assert(offsetof(GmAudioData, m_collisionBanks) == 0x248C, "GmAudioData::m_collisionBanks");
static_assert(offsetof(GmAudioData, m_instanceBanks) == 0x2504, "GmAudioData::m_instanceBanks");
#pragma pop_macro("offsetof")
#endif
