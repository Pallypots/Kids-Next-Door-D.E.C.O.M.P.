// The sound bank and segment lists, read from the .AOB and .AOS files that an .AOD file names under BankDatabase and SegmentDatabase. All class and field names are inferred, from those words, from the
// keywords in the tables and from how the fields are used. What a bank's data is and how it plays is the concrete classes' (vftables 0x00288A74, 0x0028A470, 0x0028BA40), which are not recovered.
// Original address clusters 0x00025310, 0x000254F0-0x00025B7F, 0x00031A30-0x00031A8F, 0x00031B90-0x00032074.
#pragma once

#include "at/AtIoTokenizer.h"
#include "at/AtNameTableOwner.h"
#include "gm/GmNameMap.h"

class AtAudioBank;
class AtAudioBankDatabase;

// What a bank's load requests go to. The concrete class is not recovered; only these two slots are called here, and whether they return anything used is not: the callers ignore it.
class AtAudioBankLoader {
public:
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual void Add(AtAudioBank* bank) = 0;     // slot 2
    virtual void Remove(AtAudioBank* bank) = 0;  // slot 3
};

// A file held by a bank. The same shape as AtIoFileRef, and the constructor is the same code, but this destructor does not check m_fs.
class AtAudioBankFile {
public:
    AtAudioBankFile();   // 0x00018810 folded with AtIoFileRef's
    ~AtAudioBankFile();  // 0x000249A0, also used by things not recovered

    AtIoFileSystem* m_fs;  // +0x00
    AtIoFile* m_file;      // +0x04
};

// What the .AOB entry for one bank says, handed to the concrete database.
struct AtAudioBankDesc {
    enum Format { kMsa = 0, kMia = 1, kRwf = 2, kWba = 3, kSba = 4, kGmd = 5, kGms = 6 };  // After the keywords, PlatformSource being 3 as well.
    enum Residence { kResidenceUnset = 0, kDisk = 1, kSoundRam = 2, kPrimaryRam = 3 };
    enum Flags { kDelayLoad = 1, kStreaming = 2 };

    AtAudioBankDesc();  // 0x00031D70 the name's later bytes are left as they are

    AtAudioBankDatabase* m_database;  // +0x00
    char m_name[8];                   // +0x04 not always terminated
    int m_format;                     // +0x0C
    int m_residence;                  // +0x10
    int m_blockId;                    // +0x14 0xFFFF unless given
    int m_flags;                      // +0x18
};

// One bank, which may be listed by several databases: those are its users, and the last to let go destroys it. Vftable 0x00274A90.
class AtAudioBank {
public:
    enum State { kInitialised = 1, kLoadRequested = 2, kLoaded = 4 };  // Names inferred from 0x00031F70, 0x00031FC0 and 0x00032010.
    enum { kMaxUsers = 20 };

    AtAudioBank();                             // 0x00031EC0 m_blockId is left as it is
    virtual ~AtAudioBank();                    // slot 0; 0x00031EE0 deleting, 0x00031DA0 complete
    virtual void OnInit(const AtAudioBankDesc& desc) = 0;  // slot 1
    virtual void Reset();                      // slot 2, 0x00031E20
    virtual void Unknown3() = 0;               // slots 3 to 9: argument counts are not known, and nothing here calls them
    virtual void Unknown4() = 0;
    virtual void Unknown5() = 0;
    virtual void Unknown6() = 0;
    virtual void Unknown7() = 0;
    virtual void Unknown8() = 0;
    virtual void Unknown9() = 0;
    virtual int Unload();                      // slot 10, 0x00031FC0 7 when not loaded and not in state 3. With no user it dereferences null, as the original does.
    virtual float SegmentLength(int index);    // slot 11, 0x00031E30 no data here, so zero
    virtual float Unknown12(int index);        // slot 12, 0x00031E30 folded with slot 11
    virtual int Load();                        // slot 13, 0x00031F70 with no user it dereferences null, as the original does
    virtual void Process() = 0;                // slot 14
    virtual void Update();                     // slot 15, 0x00032010
    virtual int Unknown16();                   // slot 16, 0x000AB720 returns 0, folded

    void ResetFields();                        // 0x00031DB0 not m_blockId, the name's later bytes or m_file
    void Init(const AtAudioBankDesc& desc);    // 0x00031F00
    int AddUser(AtAudioBankDatabase* user);    // 0x00031E40 the user count; no room means no change and no report
    int RemoveUser(AtAudioBankDatabase* user); // 0x00031E70 the user count
    int UserCount() const;                     // 0x00031EB0
    void GetNames(char* name, char* space);    // 0x00032020 this bank's, and the name space of its first user's map

    static void SetHeap(void* heap);           // 0x00031D90

    char m_name[8];                            // +0x04
    int m_format;                              // +0x0C
    int m_residence;                           // +0x10
    int m_blockId;                             // +0x14
    int m_state;                               // +0x18
    int m_unknown1C;                           // +0x1C 3 lets Unload go on when not loaded
    AtAudioBankDatabase* m_users[kMaxUsers];   // +0x20
    AtAudioBankFile m_file;                    // +0x70
    int m_unknown78;                           // +0x78
    int m_userCount;                           // +0x7C
    int m_unknown80;                           // +0x80
    int m_unknown84;                           // +0x84
    int m_unknown88;                           // +0x88

    static void* s_heap;                       // 0x002C63D4 only the concrete classes allocate from it

private:
    AtAudioBankDatabase* FirstUser() const;    // Inline at each use in the original.
};

// The .AOB list. Vftable 0x00272FF0.
class AtAudioBankDatabase : public GmNameMap {
public:
    // Local to Read in the original; 0x00025700 takes it by address.
    struct Args {
        AtNameTableOwner* m_owner;  // +0x00
        unsigned int m_count;       // +0x04
        const char* m_space;        // +0x08
        const char* m_directory;    // +0x0C
    };

    AtAudioBankDatabase();                     // 0x00025820
    ~AtAudioBankDatabase() override;           // slot 0; 0x00025840 deleting, 0x000256F0 complete. Does not let go of its banks: that is for Clear.
    void Clear() override;                     // slot 1, 0x00025790
    virtual void CreateBank(unsigned int index, const AtAudioBankDesc& desc) = 0;  // slot 2: must store m_banks[index], which may be null
    virtual void SetDirectory(const char* directory) = 0;                          // slot 3: where the file was found

    void Allocate(const Args& args);           // 0x00025700 the list is not cleared
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, const char* directory);  // 0x00025860 stops after the first Bank block; what follows it is not read
    void Load(AtNameTableOwner* owner, const char* name, int unused);                     // 0x00025A90 name + ".AOB"

    static void SetHeap(void* heap);           // 0x000256E0

    enum { kKeywordCount = 20 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0518

    AtNameTableOwner* m_owner;                 // +0x14
    unsigned int m_count;                      // +0x18
    AtAudioBank** m_banks;                     // +0x1C

    static void* s_heap;                       // 0x002C6338
};

class AtAudioSegmentDatabase;

// One named piece of sound: an index into a bank. Vftable 0x00274A84.
class AtAudioSegment {
public:
    AtAudioSegment();                          // 0x00031C10
    virtual ~AtAudioSegment();                 // slot 0; 0x00031C30 deleting, 0x00031B90 complete
    virtual void OnRead() = 0;                 // slot 1
    virtual void Reset();                      // slot 2, 0x00031BA0

    float Length();                            // 0x00031BB0 the bank's answer; if that is zero and this loops, the largest float
    float Unknown31BF0();                      // 0x00031BF0 the bank's slot 12, zero with no bank
    // 0x00031C50. The token after the name, a left curly, through its right curly. The fourth argument is not used. A later Bank replaces an earlier one. It keeps no name of its own: it enters the
    // database's map under `name`. A null database is allowed only if no Bank word comes.
    void Read(AtAudioSegmentDatabase* database, AtIoTokenizer* tokenizer, const char* name, const char* unused);

    AtAudioBank* m_bank;                       // +0x04 null when not found, with no report
    int m_index;                               // +0x08
    int m_looping;                             // +0x0C
};

// The .AOS list. Vftable 0x00272EF0.
class AtAudioSegmentDatabase : public GmNameMap {
public:
    AtAudioSegmentDatabase();                  // 0x00025510
    ~AtAudioSegmentDatabase() override;        // slot 0; 0x00025530 deleting, 0x00025500 complete
    void Clear() override;                     // slot 1, 0x00025310 also the slot of a neighbouring class, whose table is at 0x00272EC4
    virtual void OnReady() = 0;                // slot 2: m_count is known, and the concrete database makes its segments
    virtual void Unknown3(void* a) = 0;        // slot 3: an empty body in the concrete class
    virtual AtAudioSegment* SegmentAt(unsigned int index) = 0;  // slot 4

    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);        // 0x00025550 stops after the first Segment block; what follows it is not read
    void Load(AtNameTableOwner* owner, const char* name, int unused);    // 0x00025640 name + ".AOS"

    static void SetHeap(void* heap);           // 0x000254F0

    enum { kKeywordCount = 4 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B04E4

    AtNameTableOwner* m_owner;                 // +0x14
    unsigned int m_count;                      // +0x18

    static void* s_heap;                       // 0x002C6334 only the concrete class allocates from it
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtAudioBankDesc) == 0x1C, "AtAudioBankDesc");
static_assert(offsetof(AtAudioBank, m_state) == 0x18, "AtAudioBank::m_state");
static_assert(offsetof(AtAudioBank, m_users) == 0x20, "AtAudioBank::m_users");
static_assert(offsetof(AtAudioBank, m_file) == 0x70, "AtAudioBank::m_file");
static_assert(offsetof(AtAudioBank, m_userCount) == 0x7C, "AtAudioBank::m_userCount");
static_assert(offsetof(AtAudioBankDatabase, m_owner) == 0x14, "AtAudioBankDatabase::m_owner");
static_assert(sizeof(AtAudioBankDatabase) == 0x20, "AtAudioBankDatabase");
static_assert(sizeof(AtAudioBankDatabase::Args) == 0x10, "AtAudioBankDatabase::Args");
static_assert(sizeof(AtAudioSegment) == 0x10, "AtAudioSegment");
static_assert(sizeof(AtAudioSegmentDatabase) == 0x1C, "AtAudioSegmentDatabase");
#pragma pop_macro("offsetof")
#endif
