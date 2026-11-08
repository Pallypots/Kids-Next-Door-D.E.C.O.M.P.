// Animations, as read from .AKA text. Original address clusters 0x0002A520-0x0002A9AF, 0x0002AA80-0x0002AAEF, 0x00035B20-0x00035B7C, 0x00036210-0x000363B3, 0x000364D0-0x0003655F. The words are the original's, from table
// 0x002B0990; other names are inferred. No file on the disc has this extension: the disc's are .XKA, whose reader is not found here, and a concrete database's slot 3 is the way to it. See docs/worklog/2026-11-06-agd.md.
#pragma once

#include "at/AtMatAnimation.h"
#include "at/AtScene.h"

class AtAnimationDatabase;

// The keys of a file. Made by the device and concrete in the graphics layer. Its frame numbers are its base's.
class AtAnimationKeys : public AtGrKeySet {
public:
    // 0x000364D0. "{", then any of the five kinds of key and FrameNumber, up to "}". Clears itself first if it has been read.
    void ReadKeys(AtIoTokenizer* tokenizer);

    // The base's own, vftable 0x00274C00: recovered in src/at/AtAnimationKeys.cpp, with the binary keys. Every other slot of that table is the C library's pure-call handler.
    AtAnimationKeys();                        // 0x000364A0
    ~AtAnimationKeys();                       // 0x00036820
    void Clear() override;                    // slot 3, 0x00036630 frees each kind it has a count of, then the base set's Clear
    void SetExternalName(const char* name) override;  // slot 6, 0x00036480 the base's, and marked 0x150
    // 0x000365A0. Clears if read, reserves frames, then allocates each kind with a count: the counts are kept first. Marked 0xA8.
    void Init(unsigned int rotations, unsigned int translations, unsigned int scales, unsigned int notes, unsigned int cameras, unsigned int frames);

    virtual void Slot9(unsigned int count) = 0;
    virtual void Slot10(unsigned int count) = 0;
    virtual void Slot11(unsigned int count) = 0;
    virtual void Slot12(unsigned int count) = 0;
    virtual void Slot13(unsigned int count) = 0;
    virtual void Slot14() = 0;
    virtual void Slot15() = 0;
    virtual void Slot16() = 0;
    virtual void Slot17() = 0;
    virtual void Slot18() = 0;
    virtual void ReadRotationKeys(AtIoTokenizer* tokenizer) = 0;     // slot 19
    virtual void ReadTranslationKeys(AtIoTokenizer* tokenizer) = 0;  // slot 20
    virtual void ReadScaleKeys(AtIoTokenizer* tokenizer) = 0;        // slot 21
    virtual void ReadNoteKeys(AtIoTokenizer* tokenizer) = 0;         // slot 22
    virtual void ReadCameraKeys(AtIoTokenizer* tokenizer) = 0;       // slot 23

    unsigned int m_rotationKeyCount;      // +0x14
    unsigned int m_translationKeyCount;   // +0x18
    unsigned int m_scaleKeyCount;         // +0x1C
    unsigned int m_noteKeyCount;          // +0x20
    unsigned int m_cameraKeyCount;        // +0x24
};

// Where animations come from: the object at +0x38 of the owner. In the graphics layer.
class AtAnimationDevice {
public:
    virtual AtAnimationKeys* NewKeys() = 0;                        // slot 0
    virtual void FreeKeys(AtAnimationKeys* keys) = 0;              // slot 1
    virtual AtAnimationDatabase* NewDatabase() = 0;                // slot 2
    virtual void FreeDatabase(AtAnimationDatabase* database) = 0;  // slot 3
};

// One component's run through the keys. 0xC bytes.
struct AtAnimationComponent {
    int m_unknown00;             // +0x00
    int m_unknown04;             // +0x04
    unsigned short m_unknown08;  // +0x08 at least 1, or it is reported
    unsigned short m_unknown0A;  // +0x0A
};

// One named animation. 0x4C bytes.
class AtAnimation {
public:
    enum Flags { kHasBoundingSphere = 1 };

    AtAnimation();             // 0x00036210
    ~AtAnimation();            // 0x00036230 also resets it
    void Reset();              // 0x00035B20 the sphere's radius becomes the largest float

    // 0x00036240. Its body, a left curly through its right curly. A given loop period stands however Length follows it; with none Length sets it to one more than itself. Reads over what is there.
    void Read(AtAnimationDatabase* database, AtIoTokenizer* tokenizer);
    // 0x000359B0. From an .XKA's record of 0x60 bytes, include/at/AtGrD3DDatabase.h: every field as it is, its 32-bit runs cut to 16. Recovered in src/at/AtAnimationKeys.cpp.
    void ReadBinary(AtAnimationDatabase* database, const struct AtXkaAnimation* record);

    AtAnimationDatabase* m_database;  // +0x00
    float m_rate;                     // +0x04
    float m_velocity[3];              // +0x08 VelocityVector and LinearVelocity are one
    float m_angularVelocity[3];       // +0x14
    unsigned short m_rotation[2];     // +0x20
    unsigned short m_translation[2];  // +0x24
    unsigned short m_scale[2];        // +0x28
    unsigned short m_note[2];         // +0x2C
    unsigned short m_camera[2];       // +0x30
    unsigned short m_length;          // +0x34
    unsigned short m_loopPeriod;      // +0x36
    unsigned int m_flags;             // +0x38
    AtSceneSphere m_boundingSphere;   // +0x3C
};

// Vftable 0x00273590. Slots 2 and 3 are a concrete database's.
class AtAnimationDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 21 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B0990

    AtAnimationDatabase();              // 0x0002A770
    ~AtAnimationDatabase() override;    // slot 0; 0x0002AAD0 deleting, 0x0002AA80 complete
    void Clear() override;              // slot 1, 0x0002A9B0

    virtual int HasBinaryForm() = 0;    // slot 2
    virtual void LoadBinaryForm(AtAnimationDevice* device, const char* name) = 0;  // slot 3

    // 0x0002A8E0. mode 2 and a binary form: that form, and no text. Otherwise name + ".AKA".
    void Load(AtAnimationDevice* device, const char* name, int mode);
    void Read(AtAnimationDevice* device, AtIoTokenizer* tokenizer);  // 0x0002A7B0 sections in any order up to the end of the text

    static void SetHeap(void* heap);    // 0x0002A520

    AtAnimationDevice* m_device;            // +0x14
    AtAnimationKeys* m_keys;                // +0x18
    AtAnimationComponent* m_rotation;       // +0x1C
    AtAnimationComponent* m_translation;    // +0x20
    AtAnimationComponent* m_scale;          // +0x24
    AtAnimationComponent* m_note;           // +0x28
    AtAnimationComponent* m_camera;         // +0x2C
    unsigned int m_rotationCount;           // +0x30
    unsigned int m_translationCount;        // +0x34
    unsigned int m_scaleCount;              // +0x38
    unsigned int m_noteCount;               // +0x3C
    unsigned int m_cameraCount;             // +0x40
    AtAnimation* m_animations;              // +0x44
    unsigned int m_animationCount;          // +0x48

    static void* s_heap;                    // 0x002C6374

    void ReadAnimations(AtIoTokenizer* tokenizer);  // 0x0002A620 a second list is reported, then Init finds a table and calls Clear, which takes the keys and the new list with it; the end of Read then crashes on no keys
    void ReadComponents(AtIoTokenizer* tokenizer, AtAnimationComponent** list, unsigned int* count);  // 0x0002A530 the same
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtAnimationComponent) == 0xC, "AtAnimationComponent");
static_assert(offsetof(AtAnimationKeys, m_rotationKeyCount) == 0x14, "AtAnimationKeys::m_rotationKeyCount");
static_assert(sizeof(AtAnimationKeys) == 0x28, "AtAnimationKeys");
static_assert(offsetof(AtAnimation, m_rotation) == 0x20, "AtAnimation::m_rotation");
static_assert(offsetof(AtAnimation, m_flags) == 0x38, "AtAnimation::m_flags");
static_assert(sizeof(AtAnimation) == 0x4C, "AtAnimation");
static_assert(offsetof(AtAnimationDatabase, m_animationCount) == 0x48, "AtAnimationDatabase::m_animationCount");
static_assert(sizeof(AtAnimationDatabase) == 0x4C, "AtAnimationDatabase");
#pragma pop_macro("offsetof")
#endif
