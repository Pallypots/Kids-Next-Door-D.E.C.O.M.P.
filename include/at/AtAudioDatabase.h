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

// One emitter. Vftable 0x0028B460. Slots 8 and 12 to 23 keep the voices now playing here (0x00040 is the list) and are not recovered: no list file reaches them. They stop with a diagnostic.
class AtAudioEmitter : public AtCoordSys {
public:
    AtAudioEmitter();                          // 0x0011F1B0
    ~AtAudioEmitter() override;                // slot 2, 0x0011EC50 deleting; 0x0011EB60 complete. Does not reset: that is for the list's Clear.
    void Reset() override;                     // slot 3, 0x0011EE30 lets go of the two matrices if it has an owner
    virtual void Unknown8();                   // slot 8, 0x0011EF70 not recovered
    virtual void Init(const AtAudioEmitterDesc& desc);  // slot 9, 0x0011ED90
    // Slot 10, 0x0011EC70. The token after the name, a left curly, through its right curly.
    virtual void Read(AtAudioEmitterDatabase* database, AtIoTokenizer* tokenizer, const char* name, const char* space);
    virtual void InitPlatform() = 0;           // slot 11. The original's message calls the Xbox one vInitPlatformSpecific.
    virtual void Unknown12();                  // slots 12 to 23 not recovered, and neither are their arguments
    virtual void Unknown13();
    virtual void Unknown14();
    virtual void Unknown15();
    virtual void Unknown16();
    virtual void Unknown17();
    virtual void Unknown18();
    virtual void Unknown19();
    virtual void Unknown20();
    virtual void Unknown21();
    virtual void Unknown22();
    virtual void Unknown23();

    AtNameTableOwner* m_owner;                 // +0x28
    int m_kind;                                // +0x2C
    int m_unknown30;                           // +0x30
    int m_unknown34;                           // +0x34
    int m_unknown38;                           // +0x38
    int m_unknown3C;                           // +0x3C
    void* m_firstVoice;                        // +0x40 inferred from 0x0011EEF0
    void* m_lastVoice;                         // +0x44
    int m_unknown48;                           // +0x48 no constructor sets these two
    int m_unknown4C;                           // +0x4C
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

// Vftable 0x0028A488, whose slots 13 and 22-ish play sound and are not recovered. The stream is the SDK's.
class AtSoXboxEmitter : public AtAudioEmitter {
public:
    AtSoXboxEmitter();                         // 0x0010EF80
    ~AtSoXboxEmitter() override;               // slot 2, 0x0010F2B0 deleting, for arrays too; 0x0010EF70 complete
    void Reset() override;                     // slot 3, 0x0010F070 stops the stream, waits for it, and lets it go
    void InitPlatform() override;              // slot 11, 0x0010EFA0 a 3D stream, a stream on the front centre mix bin, or none

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

// The owner's loader, on the Xbox. Only this member of it is used here; it is at 0x5A0 in the original, and the class is not recovered.
class AtSoXboxSystem : public AtAudioBankLoader {
public:
    char m_unknown004[0x59C];
    void* m_directSound;                       // +0x5A0
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
    void Unknown3() override;                  // slots 3 to 12 play, or parse: not recovered, and they stop with a diagnostic
    void Unknown4() override;
    void Unknown5() override;
    void Unknown6() override;
    void Unknown7() override;
    void Unknown8() override;
    void Unknown9() override;
    int Unload() override;                     // slot 10, 0x0011F6F0 not recovered. Its messages call it eUpdateLoad, and it is not what the base's name says.
    float SegmentLength(int index) override;   // slot 11, 0x00120470 not recovered
    float Unknown12(int index) override;       // slot 12, 0x001204F0 not recovered
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

    int m_unknown8C;                           // +0x8C
    int m_unknown90;                           // +0x90
    AtAudioBankLoader* m_system;               // +0x94
    Names m_names;                             // +0x98
    int m_unknown1B0;                          // +0x1B0
    int m_unknown1B4;                          // +0x1B4
    void* m_header;                            // +0x1B8 what Load tests for having read the file's header
    int m_unknown1BC;                          // +0x1BC
    void* m_block1C0;                          // +0x1C0
    int m_fileHandle;                          // +0x1C4 -1 when closed
    void* m_block1C8;                          // +0x1C8
    void* m_block1CC;                          // +0x1CC
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
