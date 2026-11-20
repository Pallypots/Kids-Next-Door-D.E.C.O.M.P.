// The sound database files. An .AOD file names .AOB, .AOS, .AOE and other .AOD files, under the words BankDatabase, SegmentDatabase, EmitterDatabase and DatabaseLoader; each name is relative to the .AOD file's
// directory. An .AOE file is a list of emitters: named places a sound comes from, each a coordinate system, listed by name in the owner's table at +0x90, which the game searches for "voice", "sfx", "actor%d" and
// so on (0x00137660). Class names are inferred, except AtSoXboxEmitter and AtSoXboxXactBank, which the original's messages use. Keyword names are the original's.
// Original address clusters 0x00018A90-0x00019147, 0x000252F0-0x000254E9, 0x0010E5E0-0x0010F336, 0x0011EB60, 0x0011EC50-0x0011EE7F, 0x0011F1B0, 0x0011F290, 0x0011F370-0x0011F45F, 0x001200D0, 0x00120550, 0x00120650-0x00120684,
// 0x00125070-0x001250BD. The classes marked Xbox here are the ones that decide what a bank and an emitter are made of. They end at the SDK and at XACT, which are behind the functions at the end of this file.
#pragma once

#include "at/AtAudioBank.h"
#include "at/AtCoordSys.h"

class AtAudioEmitter;
class AtAudioEmitterDatabase;

// What an .AOE entry says, handed to AtAudioEmitter::Init.
struct AtAudioEmitterDesc {
    enum Kind { kPlain = 0, kEmitter3D = 1, kStereo = 2, kCenter = 3 };

    AtAudioEmitterDatabase* m_database;  // +0x00
    char m_name[8];                      // +0x04 not always terminated
    char m_space[8];                     // +0x0C filled in, and nothing reads it
    int m_kind;                          // +0x14
    int m_unknown18;                     // +0x18 cleared, and nothing reads it
    AtVector3 m_position;                // +0x1C
    AtQuaternion m_rotation;             // +0x28
};

// One voice playing on an emitter: what the emitter's list at +0x40 holds. The original's message calls it a play state ("AtSoIFactory::poAllocatePlayState()"); the name here is the one already in use.
// Vftable 0x0028B444, 7 slots. Voices come from the sound owner's factory and go back to it (AtSoIFactory below); an emitter makes them and its segment's bank runs them. src/at/AtAudioEmitterPlay.cpp.
class AtAudioVoice {
public:
    // Inferred from what sets and tests them: 0x0011EFE0, 0x00120150, 0x001201D0, 0x00120590. Nothing recovered sets kPlaying: the system's update does, which is not recovered.
    enum State { kPreparing = 0, kPrepared = 1, kQueued = 2, kStarting = 3, kPlaying = 4, kStopping = 5, kDone = 6 };
    enum Flags { kInitialised = 1, kReleased = 2, kPaused = 4 };

    AtAudioVoice();                            // 0x0011EB20 the links are left as they are
    virtual ~AtAudioVoice();                   // slot 0; 0x0011EB40 deleting, 0x0011EAC0 complete
    virtual void Init(AtAudioSegment* segment, AtAudioEmitter* emitter);  // slot 1, 0x0011EAD0 kDone, and kInitialised
    virtual void Reset();                      // slot 2, 0x0011EAF0
    virtual void Release();                    // slot 3, 0x0011EB10 the caller is done with it: the emitter lets it go once it is kDone
    virtual void SetParameter(int index, float value, float seconds) = 0;  // slot 4
    virtual float GetParameter(int index) = 0;                              // slot 5
    virtual void SetParameterName(int index, const char* name);            // slot 6, 0x000447F0 empty, folded with others

    AtAudioSegment* m_segment;                 // +0x04
    AtAudioEmitter* m_emitter;                 // +0x08
    unsigned int m_flags;                      // +0x0C
    int m_state;                               // +0x10
    AtAudioVoice* m_next;                      // +0x14 on the emitter, or in the factory's free list
    AtAudioVoice* m_prev;                      // +0x18
};

// One emitter. Vftable 0x0028B460, 26 slots. Slots 8 and 12 to 25 keep the voices now playing here (+0x40 is the list), in src/at/AtAudioEmitterPlay.cpp; what they do to a voice is its segment's
// bank's (AtAudioBank slots 3 to 9). Their names are inferred from what their code does with the list and the bank's slot each reaches; the sound manager calls them.
class AtAudioEmitter : public AtCoordSys {
public:
    AtAudioEmitter();                          // 0x0011F1B0
    ~AtAudioEmitter() override;                // slot 2, 0x0011EC50 deleting; 0x0011EB60 complete. Does not reset: that is for the list's Clear.
    void Reset() override;                     // slot 3, 0x0011EE30 lets go of the two matrices if it has an owner
    virtual void OnRelease(AtAudioVoice* voice);  // slot 8, 0x0011EF70 a voice's Release: let it go now if it is kDone
    virtual void Init(const AtAudioEmitterDesc& desc);  // slot 9, 0x0011ED90
    // Slot 10, 0x0011EC70. The token after the name, a left curly, through its right curly.
    virtual void Read(AtAudioEmitterDatabase* database, AtIoTokenizer* tokenizer, const char* name, const char* space);
    virtual void InitPlatform() = 0;           // slot 11. The original's message calls the Xbox one vInitPlatformSpecific.
    virtual void UpdateVoice(AtAudioVoice* voice, float seconds);  // slot 12, 0x0011F1E0 the bank's slot 9, then lets the voice go if it is kDone and released
    virtual void Update(float seconds);        // slot 13, 0x0011EB70 slot 12 on each voice. Stops after the first voice it lets go, whose link the factory has cleared.
    virtual void Prepare(AtAudioSegment* segment, AtAudioVoice** voice);  // slot 14, 0x0011EF90 a new voice, prepared by the bank if the bank is loaded; `voice` must not be null
    virtual void Play(AtAudioSegment* segment, AtAudioVoice** voice);  // slot 15, 0x0011F010 slot 14, then slot 16; the voice is handed back if asked for, released if not
    virtual void Start(AtAudioVoice* voice);   // slot 16, 0x0011EFE0 the bank's slot 4, for a voice kPreparing or kPrepared
    virtual void PlayAfter(AtAudioVoice* after, AtAudioSegment* segment, AtAudioVoice** voice);  // slot 17, 0x0011F070 a new voice queued behind `after`, through the bank's slot 5
    virtual void PauseVoice(AtAudioVoice* voice);   // slot 18, 0x0011F0C0 the bank's slot 6
    virtual void PauseAll();                   // slot 19, 0x0011EBC0
    virtual void ResumeVoice(AtAudioVoice* voice);  // slot 20, 0x0011F0E0 the bank's slot 7
    virtual void ResumeAll();                  // slot 21, 0x0011EBF0
    virtual void StopSegment(AtAudioSegment* segment);  // slot 22, 0x0011F130 slot 23 on each voice of that segment
    virtual void StopVoice(AtAudioVoice* voice);  // slot 23, 0x0011F100 through the voice's segment's bank, slot 8
    virtual void StopAll();                    // slot 24, 0x0011EC20 slot 23 on each voice; the Xbox one, 0x0010F140, stops its stream as well
    virtual void SetVelocity(const AtVector3* velocity);  // slot 25, 0x0011F190 into +0x34

    AtAudioVoice* IsPlaying(const AtAudioSegment* segment);  // 0x0011F160 the first voice of that segment, or null. src/at/AtAudioVoices.cpp
    AtAudioVoice* FirstVoice();                // 0x0011EBA0
    AtAudioVoice* NextVoice(AtAudioVoice* voice);  // 0x0011EBB0 does not read this

    AtAudioVoice* NewVoice(AtAudioSegment* segment);  // 0x0011EE90 from the factory, at the end of the list; the first one tells the system. Null if the factory has none left.
    void RemoveVoice(AtAudioVoice* voice);     // 0x0011EEF0 out of the list and back to the factory; the last one tells the system

    AtNameTableOwner* m_owner;                 // +0x28
    int m_kind;                                // +0x2C
    int m_unknown30;                           // +0x30
    int m_unknown34;                           // +0x34 to +0x3C: the velocity SetVelocity copies in, word by word. Nothing recovered reads it.
    int m_unknown38;                           // +0x38
    int m_unknown3C;                           // +0x3C
    void* m_firstVoice;                        // +0x40 inferred from 0x0011EEF0
    void* m_lastVoice;                         // +0x44
    AtAudioEmitter* m_nextWithVoices;          // +0x48 no constructor sets these two: the system's list of emitters with voices (0x000EA600)
    AtAudioEmitter* m_prevWithVoices;          // +0x4C
};

// The .AOE list. Vftable 0x00272EC4.
class AtAudioEmitterDatabase : public GmNameMap {
public:
    AtAudioEmitterDatabase();                  // 0x00025320
    ~AtAudioEmitterDatabase() override;        // slot 0; 0x00025340 deleting, 0x00025300 complete
    void Clear() override;                     // slot 1, 0x00025310 folded with AtAudioSegmentDatabase::Clear
    virtual void OnReady() = 0;                // slot 2: m_count is known, and the concrete database makes its emitters
    virtual AtAudioEmitter* EmitterAt(unsigned int index) = 0;  // slot 3

    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);        // 0x00025360 stops after the first Emitter block; what follows it is not read
    void Load(AtNameTableOwner* owner, const char* name, int unused);    // 0x00025450 name + ".AOE"

    static void SetHeap(void* heap);           // 0x000252F0

    enum { kKeywordCount = 7 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B049C

    AtNameTableOwner* m_owner;                 // +0x14
    unsigned int m_count;                      // +0x18

    static void* s_heap;                       // 0x002C6330 only the concrete class allocates from it
};

// An .AOD file. Vftable 0x00272194. Holds only counts: the concrete class makes the lists and the names are kept only while loading, in s_names.
class AtAudioDatabase {
public:
    AtAudioDatabase();                         // 0x00018AA0
    virtual void AllocateBankDatabases() = 0;      // slot 0, m_bankCount being known
    virtual void AllocateSegmentDatabases() = 0;   // slot 1
    virtual void AllocateEmitterDatabases() = 0;   // slot 2
    virtual void AllocateNestedDatabases() = 0;    // slot 3
    virtual ~AtAudioDatabase();                // slot 4; 0x00019150 deleting, 0x00019130 complete
    virtual void Clear();                      // slot 5, 0x00019110 also the slot of other classes, in vftables 0x0027629C and 0x002865EC
    virtual AtAudioBankDatabase* BankDatabaseAt(unsigned int index) = 0;        // slot 6
    virtual AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int index) = 0;  // slot 7
    virtual AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int index) = 0;  // slot 8
    virtual AtAudioDatabase* NestedDatabaseAt(unsigned int index) = 0;          // slot 9

    // 0x00018AC0 name + ".AOD". The names are read, the file closed, and then each list is loaded from the file's directory. The third argument goes to each list's Load, where nothing uses it.
    void Load(AtNameTableOwner* owner, const char* name, int third);
    void LoadLists(AtNameTableOwner* owner, int third);  // 0x00019030 banks, then segments, emitters and other .AOD files

    static void SetHeap(void* heap);           // 0x00018A90

    enum { kKeywordCount = 4 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0140

    AtNameTableOwner* m_owner;                 // +0x04
    unsigned int m_bankCount;                  // +0x08
    unsigned int m_segmentCount;               // +0x0C
    unsigned int m_emitterCount;               // +0x10
    unsigned int m_nestedCount;                // +0x14

    static void* s_heap;                       // 0x002C627C only the concrete class allocates from it
    // Statics, not members, so that an .AOD file named by an .AOD file replaces and frees those of the one that named it. No file on the disc does that.
    static char** s_bankNames;                 // 0x002C6280
    static char** s_segmentNames;              // 0x002C6284
    static char** s_emitterNames;              // 0x002C6288
    static char** s_nestedNames;               // 0x002C628C

private:
    void ReadNames(AtIoTokenizer* tokenizer, unsigned int count, char*** names);
};

// ---- The Xbox classes --------------------------------------------------------------------------------------------------------

// Vftable 0x00288A74.
class AtSoXboxBankDatabase : public AtAudioBankDatabase {
public:
    AtSoXboxBankDatabase();                    // 0x000E9F30
    ~AtSoXboxBankDatabase() override;          // slot 0, 0x0010EAB0 deleting, for arrays too; 0x000EA230 complete
    void CreateBank(unsigned int index, const AtAudioBankDesc& desc) override;  // slot 2, 0x0010EC80 a bank only for PlatformSource with no residence given; otherwise none, and no report
    void SetDirectory(const char* directory) override;  // slot 3, 0x000CBF30 empty, folded with others
};

// Vftable 0x0028B7D8.
class AtSoXboxSegment : public AtAudioSegment {
public:
    AtSoXboxSegment();                         // 0x00120650
    ~AtSoXboxSegment() override;               // slot 0, 0x0010EE70 deleting, for arrays too; 0x00120670 complete
    void OnRead() override;                    // slot 1, 0x001CC9F0 empty, folded with others
    void Reset() override;                     // slot 2, 0x00120680 the base's
};

// Vftable 0x0028A470.
class AtSoXboxSegmentDatabase : public AtAudioSegmentDatabase {
public:
    AtSoXboxSegmentDatabase();                 // 0x0010EF50
    ~AtSoXboxSegmentDatabase() override;       // slot 0, 0x0010E6A0 deleting, for arrays too; 0x0010ED90 complete
    void Clear() override;                     // slot 1, 0x0010EED0
    void OnReady() override;                   // slot 2, 0x0010EDA0
    void Unknown3(void* a) override;           // slot 3, 0x000CBF30 empty, folded with others
    AtAudioSegment* SegmentAt(unsigned int index) override;  // slot 4, 0x0010EF40

    AtSoXboxSegment* m_segments;               // +0x1C
};

// Vftable 0x0028A488. The stream is XACT's (the functions behind it are in the XBE's XACTENG section): a sound source, very likely.
class AtSoXboxEmitter : public AtAudioEmitter {
public:
    AtSoXboxEmitter();                         // 0x0010EF80
    ~AtSoXboxEmitter() override;               // slot 2, 0x0010F2B0 deleting, for arrays too; 0x0010EF70 complete
    void Reset() override;                     // slot 3, 0x0010F070 stops the stream, waits for it, and lets it go
    void InitPlatform() override;              // slot 11, 0x0010EFA0 a 3D stream, a stream on the front centre mix bin, or none
    void Update(float seconds) override;       // slot 13, 0x0010F120 places the stream, then the base's. src/at/AtSoXboxPlay.cpp
    void StopAll() override;                   // slot 24, 0x0010F140 the base's, then stops the stream

    void UpdatePosition();                     // 0x0010F0C0 a 3D emitter only: its world position over the owner's scale, to the stream. The bank calls it too.

    void* m_stream;                            // +0x50
    int m_unknown54;                           // +0x54
};

// Vftable 0x0028A5B4.
class AtSoXboxEmitterDatabase : public AtAudioEmitterDatabase {
public:
    AtSoXboxEmitterDatabase();                 // 0x0010F320
    ~AtSoXboxEmitterDatabase() override;       // slot 0, 0x0010E7C0 deleting, for arrays too; 0x0010F160 complete
    void Clear() override;                     // slot 1, 0x0010F170
    void OnReady() override;                   // slot 2, 0x0010F1E0
    AtAudioEmitter* EmitterAt(unsigned int index) override;  // slot 3, 0x0010F310

    AtSoXboxEmitter* m_emitters;               // +0x1C
};

// Vftable 0x0028A448. What 0x000E9E90 makes, and what .AOD files are loaded into.
class AtSoXboxDatabase : public AtAudioDatabase {
public:
    AtSoXboxDatabase();                        // 0x0010E870
    void AllocateBankDatabases() override;     // slot 0, 0x0010E9E0 nothing for a count of zero; the other three allocate regardless
    void AllocateSegmentDatabases() override;  // slot 1, 0x0010E5E0
    void AllocateEmitterDatabases() override;  // slot 2, 0x0010E700
    void AllocateNestedDatabases() override;   // slot 3, 0x0010EB60
    ~AtSoXboxDatabase() override;              // slot 4, 0x0010EC20 deleting, for arrays too; 0x0010EB10 complete
    void Clear() override;                     // slot 5, 0x0010E890 nested last
    AtAudioBankDatabase* BankDatabaseAt(unsigned int index) override;        // slot 6, 0x0010E820 null past the end, unlike the other three
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int index) override;  // slot 7, 0x0010E840
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int index) override;  // slot 8, 0x0010E850
    AtAudioDatabase* NestedDatabaseAt(unsigned int index) override;          // slot 9, 0x0010E860

    AtSoXboxBankDatabase* m_bankDatabases;       // +0x18
    AtSoXboxSegmentDatabase* m_segmentDatabases; // +0x1C
    AtSoXboxEmitterDatabase* m_emitterDatabases; // +0x20
    AtSoXboxDatabase* m_nestedDatabases;         // +0x24
};

// The owner's loader, on the Xbox: the sound system. Vftable 0x0028A338, some 30 slots; its constructor is not recovered, so neither is the vftable. Slots 0 to 3 keep two lists. src/at/AtSoXboxPlay.cpp.
class AtSoXboxSystem : public AtAudioBankLoader {
public:
    void AddEmitter(AtAudioEmitter* emitter) override;     // slot 0, 0x000EA600 at the end
    void RemoveEmitter(AtAudioEmitter* emitter) override;  // slot 1, 0x000EA640 the emitter's own links are left
    void Add(AtAudioBank* bank) override;      // slot 2, 0x000EA6A0 an AtSoXboxXactBank, at the end
    void Remove(AtAudioBank* bank) override;   // slot 3, 0x000EA6E0

    int m_unknown04;                           // +0x04
    int m_unknown08;                           // +0x08
    AtAudioEmitter* m_firstEmitter;            // +0x0C emitters with voices, linked through their +0x48 and +0x4C
    AtAudioEmitter* m_lastEmitter;             // +0x10
    class AtSoXboxXactBank* m_firstBank;       // +0x14 linked through the bank's +0x8C and +0x90
    class AtSoXboxXactBank* m_lastBank;        // +0x18
    char m_unknown01C[0x584];
    void* m_directSound;                       // +0x5A0 the XACT engine: what the emitter's stream is made from (0x00241DB6) and what the bank's variables go to (0x00242121)
};

// Where voices come from: the sound owner's +0x04 (AtNameTableOwner::m_device, which on the scene owner is a scene device). The name is the original's, from its message. Vftable 0x00288A30; its
// constructor, which fills the free list, is not recovered, nor are its other slots.
class AtSoIFactory {
public:
    AtAudioVoice* AllocatePlayState();         // 0x0010D770 the last free voice; none left reports, and gives null
    void FreePlayState(AtAudioVoice* voice);   // 0x0010D7B0 at the end of the free list

    void* m_vftable;                           // +0x00 not recovered
    AtAudioVoice* m_firstFree;                 // +0x04 linked through the voice's m_next and m_prev
    AtAudioVoice* m_lastFree;                  // +0x08
};

// One of a voice's four parameters: a value that goes to the voice's XACT cue as a named variable, and may move to a new value over time. Names inferred.
struct AtSoXboxVoiceParameter {
    enum Flags { kChanged = 1, kSet = 2 };

    void Step(float seconds);                  // 0x000EAB80 on along the ramp; kChanged if it moved or was just set, and kSet goes

    int m_unknown00;                           // +0x00 nothing recovered touches it
    float m_value;                             // +0x04
    float m_remaining;                         // +0x08 seconds left of the ramp
    float m_rate;                              // +0x0C per second
    unsigned int m_flags;                      // +0x10
    char m_name[8];                            // +0x14 not always terminated; empty means the variable is named by its index
};

// A voice on the Xbox: an XACT cue. Vftable 0x0028A31C, 7 slots. src/at/AtSoXboxPlay.cpp. The class name is inferred.
class AtSoXboxVoice : public AtAudioVoice {
public:
    enum { kParameterCount = 4 };

    AtSoXboxVoice();                           // 0x0010D950
    ~AtSoXboxVoice() override;                 // slot 0, 0x000E9CC0 deleting, for arrays too; 0x0010D830 complete
    void Init(AtAudioSegment* segment, AtAudioEmitter* emitter) override;  // slot 1, 0x0010D890 the cue index is the segment's
    void Reset() override;                     // slot 2, 0x0010D8C0
    void SetParameter(int index, float value, float seconds) override;    // slot 4, 0x0010D8D0 at once if under 5 microseconds, otherwise a ramp
    float GetParameter(int index) override;                               // slot 5, 0x0010D940
    void SetParameterName(int index, const char* name) override;          // slot 6, 0x0010D920

    void ClearFields();                        // 0x0010D840 not +0x28 to +0x30, nor each parameter's first word

    int m_unknown1C;                           // +0x1C
    int m_unknown20;                           // +0x20
    AtAudioVoice* m_queued;                    // +0x24 the voice to start when this one is playing (the bank's slots 5 and 9)
    int m_unknown28;                           // +0x28 to +0x30 never written here
    int m_unknown2C;                           // +0x2C
    int m_unknown30;                           // +0x30
    unsigned int m_playFlags;                  // +0x34 to the XACT play description; nothing recovered sets it
    int m_soundBank;                           // +0x38 which of the bank's XACT sound banks; nothing recovered sets it
    int m_cueIndex;                            // +0x3C
    void* m_cue;                               // +0x40 XACT's
    int m_unknown44;                           // +0x44
    AtSoXboxVoiceParameter m_parameters[kParameterCount];  // +0x48
};

// Vftable 0x0028BA40. Adds nothing recovered between a bank and the class below.
class AtSoBank : public AtAudioBank {
public:
    AtSoBank();                                // 0x00125070
    ~AtSoBank() override;                      // slot 0; 0x001250A0 deleting, 0x00125090 complete
};

// A bank, and an XACT sound bank and wave bank. Vftable 0x0028B4D8. The name is the original's. Everything that opens the .WBA file or speaks to XACT is not recovered: AtSoXboxXactBankLoad and so on below.
class AtSoXboxXactBank : public AtSoBank {
public:
    AtSoXboxXactBank();                        // 0x00120550
    ~AtSoXboxXactBank() override;              // slot 0, 0x0011F380 deleting; 0x0011F290 complete
    void OnInit(const AtAudioBankDesc& desc) override;  // slot 1, 0x0011F460 starts loading at once and waits for it, unless DelayLoad
    void Reset() override;                     // slot 2, 0x001200D0
    // Slots 3 to 9, 11 and 12 are in src/at/AtSoXboxPlay.cpp, and end at XACT, behind the functions at the end of this file.
    void Prepare(AtAudioVoice* voice) override;                     // slot 3, 0x00120150 prepares the cue; kPreparing or kPrepared, or kDone on failure
    void Start(AtAudioVoice* voice) override;                       // slot 4, 0x001201D0 places the emitter and plays the cue: kStarting, or kDone on failure
    void Queue(AtAudioVoice* after, AtAudioVoice* voice) override;  // slot 5, 0x00120260 does not use `this`: prepares through the voice's own bank
    void Pause(AtAudioVoice* voice) override;                       // slot 6, 0x001202A0
    void Resume(AtAudioVoice* voice) override;                      // slot 7, 0x001202E0
    void Stop(AtAudioVoice* voice) override;                        // slot 8, 0x00120590 and the voice queued behind it
    void UpdateVoice(AtAudioVoice* voice, float seconds) override;  // slot 9, 0x00120340 a playing voice's changed parameters to XACT; starts the queued voice
    int Unload() override;                     // slot 10, 0x0011F6F0 not recovered. Its messages call it eUpdateLoad, and it is not what the base's name says.
    float SegmentLength(int index) override;   // slot 11, 0x00120470 from the cue's properties, in seconds; the largest float if it loops
    float Unknown12(int index) override;       // slot 12, 0x001204F0 from the cue's properties: a distance, or the largest float if it has none

    static void Finish(AtAudioVoice* voice);   // 0x00120320 kDone, with no cue. Takes no `this` (the original's ecx is left alone).
    int Load() override;                       // slot 13, 0x0011F4B0 not recovered. Its messages call it vInitPlatformSpecific. Opens the .WBA file.
    void Process() override;                   // slot 14, 0x0011F2A0 not recovered. Calls slot 10 until it finishes.
    void Update() override;                    // slot 15, 0x0011FF80 not recovered. Its messages call it vUnload.
    int Unknown16() override;                  // slot 16, 0x0011F370

    void ClearFields();                        // 0x0011F3A0

    // Copied whole from an uninitialised local by ClearFields, with only these parts set.
    struct Names {
        Names();
        char m_name[0x10C];                    // +0x00
        int m_unknown10C;                      // +0x10C
        int m_unknown110;                      // +0x110
        int m_unknown114;                      // +0x114
    };

    AtSoXboxXactBank* m_nextInSystem;          // +0x8C the system's list of banks (0x000EA6A0)
    AtSoXboxXactBank* m_prevInSystem;          // +0x90
    AtAudioBankLoader* m_system;               // +0x94
    Names m_names;                             // +0x98
    int m_unknown1B0;                          // +0x1B0
    int m_unknown1B4;                          // +0x1B4
    void* m_header;                            // +0x1B8 what Load tests for having read the file's header. Prepare reads its word at +0x0C: not zero leaves a voice kPreparing.
    int m_unknown1BC;                          // +0x1BC
    void* m_block1C0;                          // +0x1C0
    int m_fileHandle;                          // +0x1C4 -1 when closed
    void* m_block1C8;                          // +0x1C8
    void* m_block1CC;                          // +0x1CC XACT sound banks, an array indexed by a voice's m_soundBank; the first serves the cue properties
    int m_status;                              // +0x1D0 3 after clearing
    int m_step;                                // +0x1D4 Process runs until 8
    void* m_block1D8;                          // +0x1D8
    void* m_block1DC;                          // +0x1DC
    int m_unknown1E0;                          // +0x1E0
    int m_unknown1E4;                          // +0x1E4
    int m_unknown1E8;                          // +0x1E8
    int m_unknown1EC;                          // +0x1EC
    int m_unknown1F0;                          // +0x1F0
    int m_unknown1F4;                          // +0x1F4
    int m_unknown1F8;                          // +0x1F8
};

// Not recovered, in src/at/AtSoXboxHost.cpp, so that a test can supply them instead. Each stops there with a diagnostic. The first three are the bank slots above, which reach XACT; the rest are SDK calls the emitter
// makes directly, in the original's order of arguments, and all are stdcall there. A status below zero is a failure.
int AtSoXboxXactBankLoad(AtSoXboxXactBank* bank);        // 0x0011F4B0
void AtSoXboxXactBankProcess(AtSoXboxXactBank* bank);    // 0x0011F2A0
void AtSoXboxXactBankUpdate(AtSoXboxXactBank* bank);     // 0x0011FF80
int AtSoXboxCreateStream(void* directSound, int kind, void** stream);  // 0x00241DB6 kind 2 for a 3D emitter, 1 for a centre one. Inferred from what follows and from the message.
struct AtSoXboxMixBin { unsigned int m_bin; int m_volume; };
struct AtSoXboxMixBins { unsigned int m_count; const AtSoXboxMixBin* m_bins; };
int AtSoXboxSetMixBins(void* stream, const AtSoXboxMixBins* bins);     // 0x002422BA
void AtSoXboxStopStream(void* stream);                   // 0x002422BF
void AtSoXboxGetStreamStatus(void* stream, unsigned int* status);  // 0x002422C4 1 while it is still playing
void AtSoXboxReleaseStream(void* stream);                // 0x0024228C

// Not recovered, in src/at/AtSoXboxXactHost.cpp, which stops with a diagnostic: what a voice's play reaches. XACT's C entry points, in the XBE's XACTENG section, each taking XACT's lock and calling
// in. All stdcall there, and the argument counts are their `ret N`. What each is in the SDK's terms is inferred from the arguments and from what the caller does with the result; the addresses are exact.
// A status of zero is success.
struct AtSoXboxCuePlay {                       // 16 bytes on the caller's stack
    unsigned int m_flags;                      // +0x00 the voice's m_playFlags, with 8 to prepare or, from kPrepared, 4 to play
    int m_cueIndex;                            // +0x04
    void* m_source;                            // +0x08 the emitter's stream to play from; null to prepare
    int m_unknown0C;                           // +0x0C zero
};
struct AtSoXboxCueProperties {                 // 0x64 bytes on the caller's stack; only these are read
    int m_hasDistance;                         // +0x00
    char m_unknown04[0x20];
    int m_looping;                             // +0x24
    char m_unknown28[0x0C];
    int m_milliseconds;                        // +0x34 -1 for none
    char m_unknown38[0x1C];
    float m_distance;                          // +0x54
    char m_unknown58[0x0C];
};
struct AtSoXboxVariable {                      // 0x18 bytes
    unsigned short m_type;                     // +0x00 1
    unsigned short m_unknown02;                // +0x02 zero
    unsigned short m_unknown04;                // +0x04 zero
    unsigned short m_unknown06;                // +0x06 never written: stack bytes
    int m_unknown08;                           // +0x08 zero
    float m_value;                             // +0x0C
    void* m_cue;                               // +0x10
    const char* m_name;                        // +0x14
};
struct AtSoXboxVariables { unsigned int m_count; AtSoXboxVariable* m_variables; };
int AtSoXboxXactPrepareCue(void* soundBank, const AtSoXboxCuePlay* play, void** cue);   // 0x00242189 three arguments
int AtSoXboxXactPlayCue(void* soundBank, const AtSoXboxCuePlay* play, void** cue);      // 0x002421BC three
int AtSoXboxXactStopCue(void* soundBank, int cueIndex, unsigned int flags, void* cue);  // 0x002421EF four; called with index -1 and flags 0
int AtSoXboxXactPauseCue(void* soundBank, void* cue, int pause);                       // 0x00242259 three
int AtSoXboxXactGetCueProperties(void* soundBank, int cueIndex, AtSoXboxCueProperties* properties);  // 0x00242226 three
int AtSoXboxXactSetVariables(void* engine, const AtSoXboxVariables* variables);        // 0x00242121 two
int AtSoXboxXactSetPosition(void* stream, float x, float y, float z, int apply);       // 0x00242291 five; called with apply 1. The result is not used.

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtAudioEmitterDesc, m_kind) == 0x14, "AtAudioEmitterDesc::m_kind");
static_assert(offsetof(AtAudioEmitterDesc, m_position) == 0x1C, "AtAudioEmitterDesc::m_position");
static_assert(offsetof(AtAudioEmitterDesc, m_rotation) == 0x28, "AtAudioEmitterDesc::m_rotation");
static_assert(sizeof(AtAudioEmitterDesc) == 0x38, "AtAudioEmitterDesc");
static_assert(offsetof(AtAudioEmitter, m_owner) == 0x28, "AtAudioEmitter::m_owner");
static_assert(offsetof(AtAudioEmitter, m_firstVoice) == 0x40, "AtAudioEmitter::m_firstVoice");
static_assert(sizeof(AtAudioEmitterDatabase) == 0x1C, "AtAudioEmitterDatabase");
static_assert(offsetof(AtAudioDatabase, m_nestedCount) == 0x14, "AtAudioDatabase::m_nestedCount");
static_assert(sizeof(AtAudioDatabase) == 0x18, "AtAudioDatabase");
static_assert(sizeof(AtSoXboxBankDatabase) == 0x20, "AtSoXboxBankDatabase");
static_assert(sizeof(AtSoXboxSegment) == 0x10, "AtSoXboxSegment");
static_assert(sizeof(AtSoXboxSegmentDatabase) == 0x20, "AtSoXboxSegmentDatabase");
static_assert(offsetof(AtSoXboxEmitter, m_stream) == 0x50, "AtSoXboxEmitter::m_stream");
static_assert(sizeof(AtSoXboxEmitter) == 0x58, "AtSoXboxEmitter");
static_assert(sizeof(AtSoXboxEmitterDatabase) == 0x20, "AtSoXboxEmitterDatabase");
static_assert(sizeof(AtSoXboxDatabase) == 0x28, "AtSoXboxDatabase");
static_assert(offsetof(AtSoXboxSystem, m_directSound) == 0x5A0, "AtSoXboxSystem::m_directSound");
static_assert(offsetof(AtSoXboxSystem, m_firstEmitter) == 0x0C, "AtSoXboxSystem::m_firstEmitter");
static_assert(offsetof(AtSoXboxSystem, m_firstBank) == 0x14, "AtSoXboxSystem::m_firstBank");
static_assert(offsetof(AtAudioVoice, m_segment) == 0x04, "AtAudioVoice::m_segment");
static_assert(offsetof(AtAudioVoice, m_state) == 0x10, "AtAudioVoice::m_state");
static_assert(offsetof(AtAudioVoice, m_prev) == 0x18, "AtAudioVoice::m_prev");
static_assert(sizeof(AtAudioVoice) == 0x1C, "AtAudioVoice");
static_assert(sizeof(AtSoXboxVoiceParameter) == 0x1C, "AtSoXboxVoiceParameter");
static_assert(offsetof(AtSoXboxVoice, m_queued) == 0x24, "AtSoXboxVoice::m_queued");
static_assert(offsetof(AtSoXboxVoice, m_playFlags) == 0x34, "AtSoXboxVoice::m_playFlags");
static_assert(offsetof(AtSoXboxVoice, m_cue) == 0x40, "AtSoXboxVoice::m_cue");
static_assert(offsetof(AtSoXboxVoice, m_parameters) == 0x48, "AtSoXboxVoice::m_parameters");
static_assert(sizeof(AtSoXboxVoice) == 0xB8, "AtSoXboxVoice");
static_assert(offsetof(AtSoIFactory, m_lastFree) == 0x08, "AtSoIFactory::m_lastFree");
static_assert(sizeof(AtSoXboxCuePlay) == 0x10, "AtSoXboxCuePlay");
static_assert(offsetof(AtSoXboxCueProperties, m_looping) == 0x24, "AtSoXboxCueProperties::m_looping");
static_assert(offsetof(AtSoXboxCueProperties, m_milliseconds) == 0x34, "AtSoXboxCueProperties::m_milliseconds");
static_assert(offsetof(AtSoXboxCueProperties, m_distance) == 0x54, "AtSoXboxCueProperties::m_distance");
static_assert(sizeof(AtSoXboxCueProperties) == 0x64, "AtSoXboxCueProperties");
static_assert(sizeof(AtSoXboxVariable) == 0x18, "AtSoXboxVariable");
static_assert(sizeof(AtSoBank) == 0x8C, "AtSoBank");
static_assert(sizeof(AtSoXboxXactBank::Names) == 0x118, "AtSoXboxXactBank::Names");
static_assert(offsetof(AtSoXboxXactBank, m_system) == 0x94, "AtSoXboxXactBank::m_system");
static_assert(offsetof(AtSoXboxXactBank, m_names) == 0x98, "AtSoXboxXactBank::m_names");
static_assert(offsetof(AtSoXboxXactBank, m_header) == 0x1B8, "AtSoXboxXactBank::m_header");
static_assert(offsetof(AtSoXboxXactBank, m_status) == 0x1D0, "AtSoXboxXactBank::m_status");
static_assert(offsetof(AtSoXboxXactBank, m_unknown1F8) == 0x1F8, "AtSoXboxXactBank::m_unknown1F8");
static_assert(sizeof(AtSoXboxXactBank) == 0x1FC, "AtSoXboxXactBank");
#pragma pop_macro("offsetof")
#endif
