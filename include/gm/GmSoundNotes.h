// .GON files: "sound notes", the table that says what an animation's trigger plays. Text, one file to a character, a location or a family of effects, under AUDIO\ on the disc. A note makes no object of
// its own in the game world: it is a plain record, kept by the sound manager (GmAudioData) in one of sixty banks, and found by name when an animation fires its trigger. All names are inferred, from the
// keywords in the file at 0x002B9090, except those keywords themselves, which are the original's.
// Original address clusters 0x00136740-0x00136799, 0x00136930-0x00136A0C, 0x00136CC0-0x00136D96, 0x00137D20-0x00137EAA, 0x00137EB0-0x00137F4A, 0x00139090-0x0013960C, 0x001424C0-0x00142516, 0x001B3540-0x001B36CD.
#pragma once
#include "gm/GmManifestLoader.h"
#include "gm/GmNameMap.h"

class AtAudioEmitter;
class AtAudioSegment;

// One of the sounds a note may play, chosen at random by weight. The Segments block of a note.
struct GmSoundChoice {
    GmSoundChoice();                     // 0x00136D30

    char m_name[8];                      // +0x00 of a segment in an .AOS list; not always terminated
    AtAudioSegment* m_segment;           // +0x08 null until GmAudioData::RegisterNotes, and after it if no list has the name, with no report
    float m_weight;                      // +0x0C from the file, 1 if none; the load scales a note's weights to sum to 1
};

// One Note block.
struct GmSoundNote {
    GmSoundNote();                       // 0x00136D50

    char m_name[8];                      // +0x00 not always terminated, as with every name here
    char m_trigger[8];                   // +0x08 AnimationTrigger
    char m_vfxTrigger[8];                // +0x10 VFXTrigger. No real file has one
    char m_gameEvent[8];                 // +0x18 GameEvent. No real file has one
    char m_gameEventArgument[8];         // +0x20 the optional string after it
    AtAudioEmitter* m_emitter;           // +0x28 GlobalEmitter, by name in the sound owner's table. Null is ActorEmitter, which is the default, so that word does nothing
    AtAudioEmitter* m_conditional;       // +0x2C Conditional. 0x001386D0 plays nothing while this emitter has a voice playing
    char m_stop[8];                      // +0x30 Stop: 0x001386D0 first stops the first segment of the note of this name
    int m_singleton;                     // +0x38 Singleton: 0x001386D0 skips the play if that segment is already playing on the emitter
    unsigned int m_choiceCount;          // +0x3C
    GmSoundChoice* m_choices;            // +0x40 a second Segments block leaks the first
};

// What one .GON file loads into. The manager has sixty, at +0xEF0, with a used flag each at +0x20C0.
class GmSoundNoteBank {
public:
    GmSoundNoteBank();                   // 0x00136CC0
    ~GmSoundNoteBank();                  // 0x00136740

    char m_name[8];                      // +0x00 the file's NameSpace, which FindNote can filter on. No real file has one
    GmNameMap m_byName;                  // +0x08 filled by GmAudioData::RegisterNotes, not by the load
    GmSoundNote* m_notes;                // +0x1C null is an unused bank
    unsigned int m_noteCount;            // +0x20
    GmNameMap m_byTrigger;               // +0x24 AnimationTrigger to note
    GmNameMap m_byVfxTrigger;            // +0x38 VFXTrigger to note. Nothing recovered reads it
};

// What a level's manifest makes of each file named under its GON word: the parser is 0x00142E90, the array is at +0xF8 of that loader, and 0x00143CE0 registers them when all have run. Vftable 0x002950AC.
class GmSoundNoteItem : public GmManifestItem {
public:
    struct Request {
        int m_unused;                    // +0x00
        char m_path[0x80];               // +0x04 the size is the caller's: at most what the item's own path has room for is meant
    };

    GmSoundNoteItem();                   // 0x001B3640
    virtual ~GmSoundNoteItem();          // slot 2; 0x001424C0 deleting, for arrays too; 0x001B3680 complete
    void Unknown0() override;            // slot 0, 0x001B3580 gives back the bank
    void Run() override;                 // slot 1, 0x001B35D0 takes a bank and loads the file. All sixty in use: nothing is loaded, and nothing said

    void Configure(const Request* request);  // 0x001B3540 what the item held is given back first; the extension becomes GON

    GmSoundNoteBank* m_bank;             // +0x8C
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmSoundChoice) == 0x10, "GmSoundChoice");
static_assert(offsetof(GmSoundNote, m_emitter) == 0x28, "GmSoundNote::m_emitter");
static_assert(offsetof(GmSoundNote, m_stop) == 0x30, "GmSoundNote::m_stop");
static_assert(offsetof(GmSoundNote, m_choices) == 0x40, "GmSoundNote::m_choices");
static_assert(sizeof(GmSoundNote) == 0x44, "GmSoundNote");
static_assert(offsetof(GmSoundNoteBank, m_byName) == 0x08, "GmSoundNoteBank::m_byName");
static_assert(offsetof(GmSoundNoteBank, m_notes) == 0x1C, "GmSoundNoteBank::m_notes");
static_assert(offsetof(GmSoundNoteBank, m_byTrigger) == 0x24, "GmSoundNoteBank::m_byTrigger");
static_assert(offsetof(GmSoundNoteBank, m_byVfxTrigger) == 0x38, "GmSoundNoteBank::m_byVfxTrigger");
static_assert(sizeof(GmSoundNoteBank) == 0x4C, "GmSoundNoteBank");
static_assert(offsetof(GmSoundNoteItem, m_bank) == 0x8C, "GmSoundNoteItem::m_bank");
static_assert(sizeof(GmSoundNoteItem) == 0x90, "GmSoundNoteItem");
#pragma pop_macro("offsetof")
#endif
