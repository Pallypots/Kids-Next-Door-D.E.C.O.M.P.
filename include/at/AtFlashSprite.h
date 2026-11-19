// The Flash user interface's sprites and their frames: .AFP files ("Atlas Flash Sprite Definition File" by their own first line), and the binary .XFF files, magic "HVSIXFF", that hold the frames on this disc. Original address clusters
// 0x00016AE0-0x00016F3F, 0x00017370-0x00017E5F, 0x00023460-0x00023A9F, 0x00023C10-0x00024A97, 0x00031830-0x000318AB, 0x00012280-0x000126DF. AtFpSprite and AtFpPlaceObject2Tag are the original's own names, from its messages
// "AtFpSprite::vReadData Expected string" and "AtFpPlaceObject2Tag::vWriteData invalid placement type"; the keywords are the original's, from tables 0x00271034 and 0x00271F98; other names are inferred.
//
// Loading is what is recovered. How a frame's tags run is not: the tags' slots that only that reaches stop with a diagnostic. See docs/worklog/2026-11-07-afd.md.
#pragma once

#include "at/AtFlashTextFormat.h"

// A frame's command. Vftable 0x00274A5C. Slots 2 and 3 of this class are called directly as well.
class AtFlashTag {
public:
    AtFlashTag();                      // 0x00031830 m_heap is left as it is
    virtual ~AtFlashTag();             // slot 0; 0x000318B0 deleting, 0x00031890 complete
    virtual void Slot1();              // slot 1, 0x000760A0 not recovered: stops with a diagnostic
    virtual void Prepare();            // slot 2, 0x00031850 asks for a reset if already prepared
    virtual void Reset();              // slot 3, 0x00031870
    virtual void ReadText(AtIoTokenizer* tokenizer, int a) = 0;   // slot 4: text frames, .AFF; arguments as far as seen
    virtual void WriteText(AtIoTokenizer* tokenizer, int a) = 0;  // slot 5
    virtual void Slot6();              // slot 6, 0x00023BA0 not recovered: stops with a diagnostic

    int m_state;                       // +0x04
    int m_type;                        // +0x08 1 place, 2 remove, 3 sound, 4 action; -1 in this class
    void* m_heap;                      // +0x0C
};

// What the three small tags share: one number, read as 4 bytes.
class AtFlashIdTag : public AtFlashTag {
public:
    void Prepare() override;           // slot 2, 0x00023BC0 not recovered: stops with a diagnostic
    void ReadBinary(const unsigned char** data);  // 0x00024760

    int m_read;                        // +0x10
    int m_id;                          // +0x14 -1 until read
};

// The word Action. Vftable 0x00272D30.
class AtFlashActionTag : public AtFlashIdTag {
public:
    AtFlashActionTag();                // 0x00023C10
    ~AtFlashActionTag() override;      // slot 0; 0x000173D0 deleting, 0x00023C40 complete
    void Reset() override;             // slot 3, 0x00023BE0 not recovered: stops with a diagnostic
    void ReadText(AtIoTokenizer* tokenizer, int a) override;   // slot 4, 0x00023AB0 not recovered: stops with a diagnostic
    void WriteText(AtIoTokenizer* tokenizer, int a) override;  // slot 5, 0x00023B50 not recovered: stops with a diagnostic
};

// The word Remove2. Vftable 0x00272DDC.
class AtFlashRemoveTag : public AtFlashIdTag {
public:
    AtFlashRemoveTag();                // 0x000246C0
    ~AtFlashRemoveTag() override;      // slot 0; 0x00017490 deleting, 0x000246F0 complete
    void Reset() override;             // slot 3, 0x00024690 not recovered: stops with a diagnostic
    void ReadText(AtIoTokenizer* tokenizer, int a) override;   // slot 4, 0x000245A0 not recovered: stops with a diagnostic
    void WriteText(AtIoTokenizer* tokenizer, int a) override;  // slot 5, 0x00024640 not recovered: stops with a diagnostic
};

// The word DoSound. Vftable 0x00272DF8.
class AtFlashSoundTag : public AtFlashIdTag {
public:
    AtFlashSoundTag();                 // 0x000248B0
    ~AtFlashSoundTag() override;       // slot 0; 0x000174F0 deleting, 0x000248E0 complete
    void Reset() override;             // slot 3, 0x00024880 not recovered: stops with a diagnostic
    void ReadText(AtIoTokenizer* tokenizer, int a) override;   // slot 4, 0x00024790 not recovered: stops with a diagnostic
    void WriteText(AtIoTokenizer* tokenizer, int a) override;  // slot 5, 0x00024830 not recovered: stops with a diagnostic
};

// The word Place2. Vftable 0x00272DC0. 0x58 bytes. What the numbered fields mean is in the text reader, not recovered.
class AtFpPlaceObject2Tag : public AtFlashTag {
public:
    AtFpPlaceObject2Tag();             // 0x00024210
    ~AtFpPlaceObject2Tag() override;   // slot 0; 0x00017430 deleting, 0x00024230 complete
    void Prepare() override;           // slot 2, 0x00023D10 not recovered: stops with a diagnostic
    void Reset() override;             // slot 3, 0x00023D90 not recovered: stops with a diagnostic
    void ReadText(AtIoTokenizer* tokenizer, int a) override;   // slot 4, 0x00023DC0 not recovered: stops with a diagnostic
    void WriteText(AtIoTokenizer* tokenizer, int a) override;  // slot 5, 0x00024370 not recovered: stops with a diagnostic

    void ResetFields();                // 0x00023D30
    void SetName(const char* name);    // 0x00024180 a copy, from m_heap or s_heap
    void SetColorScale(const unsigned char* bytes);  // 0x00023CB0 each byte times 1/255
    void ReadBinary(const unsigned char** data);     // 0x000242B0 0x18 bytes; what they point at follows the tags, by offsets from their end

    float m_colorScale[4];             // +0x10 1 each by default
    unsigned char m_colorOffset[4];    // +0x20
    char* m_name;                      // +0x24
    unsigned short m_unknown28;        // +0x28 0xFFFF by default
    unsigned short m_unknown2A;        // +0x2A 0xFFFF by default
    unsigned char m_loaded;            // +0x2C
    int m_unknown30;                   // +0x30 -1 by default
    unsigned char m_unknown34;         // +0x34
    unsigned int m_transform[6];       // +0x38 as the file has them; the last two are 1.0f by default
    int m_unknown50;                   // +0x50 -1 by default
    int m_unknown54;                   // +0x54 -1 by default

    static void* s_heap;               // 0x002C63D0
};

// One frame: its tags in the order they run. 0x30 bytes. Vftable 0x00272D2C.
class AtFlashFrame : public AtFlashResource {
public:
    enum Flags { kUsed = 1, kTagsNotOwned = 2 };

    AtFlashFrame();                    // 0x00023460
    ~AtFlashFrame();                   // 0x00023A60 not virtual
    void Read(AtIoTokenizer* tokenizer) override;  // slot 0, 0x00023A50: text frames, .AFF, not recovered: stops with a diagnostic. No .AFF is on the disc.
    void Clear();                      // 0x000234C0 nothing unless used
    // 0x00023860. 8 bytes: index, then how many place, action, remove and sound tags, one byte each; then the tags, removes first, then places, actions, sounds, each taken from its own run of objects.
    void ReadBinary(const unsigned char** data, AtFlashActionTag** actions, AtFpPlaceObject2Tag** places, AtFlashRemoveTag** removes, AtFlashSoundTag** sounds);

    unsigned int m_flags;              // +0x1C
    unsigned int m_tagCount;           // +0x20
    unsigned int m_tagCapacity;        // +0x24
    AtFlashTag** m_tags;               // +0x28
    void* m_heap;                      // +0x2C

    static void* s_heap;               // 0x002C6324
};

// Vftable 0x00272108. All of a level's frames, in one .XFF or .AFF.
class AtFlashFrameTable : public AtFlashResourceTable {
public:
    AtFlashFrameTable();               // 0x00017550
    void Allocate(unsigned int count) override;  // slot 0, 0x00017B80 the frames of an .AFF
    ~AtFlashFrameTable() override;     // slot 1; 0x00017E40 deleting, 0x00017C70 complete

    void ClearFrames();                // 0x00017580 nothing unless loaded
    void Load(const char* name, int mode);       // 0x00017CC0 mode 2: Sprites\name.XFF; otherwise Sprites\name.AFF
    void LoadBinary(const char* path);           // 0x00017680

    int m_loaded;                      // +0x28
    AtFlashActionTag* m_actions;       // +0x2C
    AtFpPlaceObject2Tag* m_places;     // +0x30
    AtFlashRemoveTag* m_removes;       // +0x34
    AtFlashSoundTag* m_sounds;         // +0x38
};

// One SpriteDef: 0x34 bytes. Vftable 0x00271F40.
class AtFpSprite : public AtFlashResource {
public:
    struct FrameRef {
        char* m_name;                  // +0x00 null for most
        int m_frame;                   // +0x04 -1 until read
        int m_unknown08;               // +0x08
    };

    AtFpSprite();                      // 0x00016AE0
    ~AtFpSprite();                     // 0x00016EF0 not virtual
    void Read(AtIoTokenizer* tokenizer) override;  // slot 0, 0x00016EE0 only jumps to ReadData
    void ReadData(AtIoTokenizer* tokenizer);       // 0x00016C80
    void Clear();                      // 0x00016B40 nothing unless used
    void Allocate(int index, unsigned int capacity);  // 0x00016BB0

    int m_used;                        // +0x1C
    unsigned int m_unknown20;          // +0x20
    unsigned int m_refCount;           // +0x24
    unsigned int m_refCapacity;        // +0x28
    FrameRef* m_refs;                  // +0x2C
    void* m_heap;                      // +0x30

    static void* s_heap;               // 0x002C626C
    static void SetHeap(void* heap);   // 0x000169A0
};

// Vftable 0x00271080. The sprites of a level: Sprites\name.AFP, with its frames.
class AtFlashSpriteTable : public AtFlashResourceTable {
public:
    AtFlashSpriteTable();              // 0x00012280
    void Allocate(unsigned int count) override;  // slot 0, 0x00012570
    ~AtFlashSpriteTable() override;    // slot 1; 0x000126C0 deleting, 0x00012660 complete
    void Release(unsigned int index) override;   // slot 2, 0x00012540 not recovered: stops with a diagnostic. Only what runs a sprite gets here.

    void ClearSprites();               // 0x000122E0 nothing unless loaded
    void Load(const char* name, int mode);       // 0x00012370 mode goes on to the frames

    int m_loaded;                      // +0x28
    AtFlashFrameTable m_frames;        // +0x2C
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtFlashIdTag, m_id) == 0x14, "AtFlashIdTag::m_id");
static_assert(sizeof(AtFlashActionTag) == 0x18, "AtFlashActionTag");
static_assert(sizeof(AtFlashRemoveTag) == 0x18, "AtFlashRemoveTag");
static_assert(sizeof(AtFlashSoundTag) == 0x18, "AtFlashSoundTag");
static_assert(offsetof(AtFpPlaceObject2Tag, m_name) == 0x24, "AtFpPlaceObject2Tag::m_name");
static_assert(offsetof(AtFpPlaceObject2Tag, m_unknown34) == 0x34, "AtFpPlaceObject2Tag::m_unknown34");
static_assert(offsetof(AtFpPlaceObject2Tag, m_transform) == 0x38, "AtFpPlaceObject2Tag::m_transform");
static_assert(sizeof(AtFpPlaceObject2Tag) == 0x58, "AtFpPlaceObject2Tag");
static_assert(offsetof(AtFlashFrame, m_flags) == 0x1C, "AtFlashFrame::m_flags");
static_assert(offsetof(AtFlashFrame, m_heap) == 0x2C, "AtFlashFrame::m_heap");
static_assert(sizeof(AtFlashFrame) == 0x30, "AtFlashFrame");
static_assert(offsetof(AtFlashFrameTable, m_sounds) == 0x38, "AtFlashFrameTable::m_sounds");
static_assert(sizeof(AtFlashFrameTable) == 0x3C, "AtFlashFrameTable");
static_assert(offsetof(AtFpSprite, m_refs) == 0x2C, "AtFpSprite::m_refs");
static_assert(sizeof(AtFpSprite::FrameRef) == 0xC, "AtFpSprite::FrameRef");
static_assert(sizeof(AtFpSprite) == 0x34, "AtFpSprite");
static_assert(offsetof(AtFlashSpriteTable, m_frames) == 0x2C, "AtFlashSpriteTable::m_frames");
static_assert(sizeof(AtFlashSpriteTable) == 0x68, "AtFlashSpriteTable");
#pragma pop_macro("offsetof")
#endif
