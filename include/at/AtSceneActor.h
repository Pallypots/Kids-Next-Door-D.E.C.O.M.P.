// The classes of scene node that have something to draw: the actor base (vftable 0x00276FA8), ModelActor (0x00276620) and JointedModelActor (0x002766D0), with the animation sets and channels they read. Original address clusters
// 0x00055B80-0x00056D9F, 0x0004AFE0-0x0004C09F, 0x00059A70-0x0005A9EF, 0x0005B710, 0x0005BAA0-0x0005BADF, 0x0005EE30-0x0005EFC1, 0x0005FD30-0x0005FD7B, 0x0005FF90. The class and word names come from the keyword table at 0x002B1990 and the
// original's messages; all other names are inferred from what the code does.
//
// Only what loading a scene and clearing it reach is recovered. Slots for updating, testing and drawing in a running game stop with a diagnostic. What those slots take as arguments is not established, so their names say only where they are.
#pragma once

#include "at/AtScene.h"

class AtScenePlayback;

// Told when a playback starts. Nothing in the loading path sets one, so what it is, is not established.
class AtScenePlaybackListener {
public:
    virtual void Slot0() = 0;
    virtual void OnStarted(AtScenePlayback* playback) = 0;  // slot 1
};

// The first sixteen bytes of both kinds of channel.
class AtScenePlayback {
public:
    enum Flags { kWasRewound = 0x1, kStarted = 0x2, kPlaying = 0x4, kHasTranslationScale = 0x400 };

    AtScenePlayback() : m_flags(0), m_time(0), m_rate(0), m_listener(0) {}
    void Rewind();                     // 0x0005B710 flags to 1, whatever they were; time to 0

    unsigned int m_flags;              // +0x00
    float m_time;                      // +0x04
    float m_rate;                      // +0x08
    AtScenePlaybackListener* m_listener;  // +0x0C
};

// What a material animation channel plays. Loaded by code not recovered; these are the only fields read here.
struct AtSceneMatAnimation {
    int m_unknown00;                   // +0x00
    float m_rate;                      // +0x04
    int m_unknown08;                   // +0x08
    unsigned int m_frames;             // +0x0C
};

// What an animation channel plays. Likewise.
struct AtSceneAnimation {
    int m_unknown00;                   // +0x00
    float m_rate;                      // +0x04
    char m_unknown08[0x2E];            // +0x08
    unsigned short m_frames;           // +0x36
};

// 0x14 bytes.
class AtSceneMatPlayback : public AtScenePlayback {
public:
    AtSceneMatPlayback();                              // 0x0005BAA0
    ~AtSceneMatPlayback();                             // 0x0005FF90
    void SetAnimation(AtSceneMatAnimation* animation);  // 0x0005BAC0

    AtSceneMatAnimation* m_animation;  // +0x10
};

// One entry of MatAnimationChannel. 0x1C bytes.
class AtSceneMatChannel : public AtSceneMatPlayback {
public:
    AtSceneMatChannel();                               // 0x0005EE30
    ~AtSceneMatChannel();                              // 0x0005EFB0
    void Read(AtSceneDevice* device, AtIoTokenizer* tokenizer);  // 0x0005EE50

    unsigned int m_options;            // +0x14 bit 0: PlayMatAnimation was seen
    int m_assignmentOffset;            // +0x18 MatAssignmentOffset
};

// 0x28 bytes.
class AtSceneAnimPlayback : public AtScenePlayback {
public:
    AtSceneAnimPlayback();                             // 0x0005FD30
    void SetAnimation(AtSceneAnimation* animation);    // 0x0005FD60

    AtSceneAnimation* m_animation;     // +0x10
    int m_unknown14;                   // +0x14
    AtVector3 m_translationScale;      // +0x18 ScaleTranslations
    float m_speed;                     // +0x24 a rate given, over the animation's own
};

// One entry of AnimationChannel or TextureCoordAnimationChannel. 0x3C bytes. The words decide what it is: the two lists differ only by the word each entry starts with.
class AtSceneAnimChannel : public AtSceneAnimPlayback {
public:
    enum Options { kPlaying = 0x1, kLinearVelocity = 0x2, kAngularVelocity = 0x4, kMirroring = 0x8 };

    AtSceneAnimChannel();                              // 0x0005A720
    void Read(AtSceneDevice* device, AtIoTokenizer* tokenizer, int word);  // 0x0005A750

    float m_weightMultiplier;          // +0x28
    void* m_weights;                   // +0x2C
    unsigned int m_options;            // +0x30
    void* m_mirrors;                   // +0x34 CoordSysMirrorDatabase
    void* m_unknown38;                 // +0x38
};

// Room for one pose per coordinate system of a database. Filled in a running game; here it is made, and its first words cleared.
class AtSceneAnimBlend {
public:
    struct Rotation { float value[5]; };
    struct Vector { float value[4]; };

    AtSceneAnimBlend();                                // 0x000320A0
    ~AtSceneAnimBlend() { Clear(); }                   // 0x00033CE0 likewise
    void Clear();                                      // 0x00033CE0
    void Allocate(const AtCoordSysDatabase* database);  // 0x00034310
    void Reset();                                      // 0x00033C80 only the first word of each element

    static void SetHeap(void* heap);                   // 0x00033C70

    Rotation* m_rotations;             // +0x00 one for each rotation and translation system
    Vector* m_translations;            // +0x04 likewise
    Vector* m_scales;                  // +0x08 one for each scale system
    unsigned int m_rotTransCount;      // +0x0C
    unsigned int m_scaleCount;         // +0x10
    int m_unknown14;                   // +0x14

    static void* s_heap;               // 0x002C63D8
};

// A database of coordinate systems, the channels that animate them and room to blend. 0x28 bytes. An actor has one for texture coordinates, and a jointed one another for its joints.
class AtSceneAnimSet {
public:
    AtSceneAnimSet();                                  // 0x00059AB0
    ~AtSceneAnimSet();                                 // 0x0005A2C0
    void ReadDatabase(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);  // 0x00059AD0 by name; never owned
    void ReadChannels(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int word);  // 0x00059B30
    void Clear();                                      // 0x00059D70 does not free the database, and destroys no channel, having no destructor to call
    void PrepareBlend(int always);                     // 0x00059A80 only for more than one channel, unless always
    void SetDatabase(AtCoordSysDatabase* database);    // 0x00082690

    static void SetHeap(void* heap);                   // 0x00059A70

    AtCoordSysDatabase* m_database;    // +0x00
    unsigned int m_channelCount;       // +0x04
    AtSceneAnimChannel* m_channels;    // +0x08
    AtSceneAnimBlend m_blend;          // +0x0C
    int m_unknown24;                   // +0x24

    static void* s_heap;               // 0x002C9C0C
};

// Vftable 0x00276FA8. Abstract: slots 34, 38 and 39 are pure. Size 0x98.
class AtSceneActor : public AtSceneNode {
public:
    enum Flags {
        kDrawing = 0x800, kOwnsScaleCoordSys = 0x1000, kModifierReceiver = 0x2000, kProjectionReceiver = 0x4000, kOwnsAnimDatabase = 0x8000
    };

    AtSceneActor();                                // 0x00055DB0
    void ReadHeader(AtScene* scene, AtIoTokenizer* tokenizer, int token) override;  // slot 0, 0x00055B80
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x00056500
    void FinishBody() override;                    // slot 2, 0x00056320 with a scale, the largest of the three on the radius
    AtCoordSys* CoordSys() override;               // slot 4, 0x00055BD0
    AtCoordSys* ParentCoordSys() override;         // slot 5, 0x00055BE0 through what this shares with, if anything
    void FinishHeader() override;                  // slot 6, 0x00055C00 the scale goes under the rotation
    ~AtSceneActor() override;                      // slot 7, 0x00056D80 deleting; 0x000564B0 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x00055C20
    void Clear() override;                         // slot 11, 0x00055E00
    void Slot12() override;                        // slot 12, 0x000561B0 not recovered
    void Slot16() override;                        // slot 16, 0x00055CA0 not recovered
    void Slot17() override;                        // slot 17, 0x00055CC0 not recovered
    void Slot18() override;                        // slot 18, 0x00055C80 not recovered
    virtual void ShareCoordSys(AtSceneActor* with);  // slot 32, 0x00055C50 gives up its own
    virtual void StopSharingCoordSys();            // slot 33, 0x00055C70
    virtual void Slot34() = 0;                     // slot 34, pure; called by 0x000561B0
    virtual void Slot35();                         // slot 35, 0x00055F30 not recovered
    virtual void Slot36();                         // slot 36, 0x00056AB0 not recovered
    virtual void Slot37();                         // slot 37, 0x00055FE0 not recovered
    virtual void Slot38() = 0;                     // slot 38, pure
    virtual void Slot39() = 0;                     // slot 39, pure
    virtual void Slot40();                         // slot 40, 0x00055D90 not recovered
    virtual void Slot41();                         // slot 41, 0x00055D90 not recovered
    virtual void Slot42();                         // slot 42, 0x000447F0 not recovered

    AtSceneAnimSet* EnsureAnimations();            // 0x00055CE0 only when asked for by a word
    AtCoordSysDatabase* CopyAnimDatabase(AtCoordSysDatabase* from);  // 0x00056260 the copy is this actor's own

    static void SetHeap(void* heap);               // 0x00055B70
    static AtSceneActor* AsActor(AtSceneNode* node);  // 0x00055930 reports null, or another class, and hands it back regardless

    int m_unknown6C;                   // +0x6C only ever cleared here
    int m_unknown70;                   // +0x70 not touched here
    void* m_matAssignment;             // +0x74 MatAssignment
    unsigned int m_matChannelCount;    // +0x78
    AtSceneMatChannel* m_matChannels;  // +0x7C
    AtScaleCoordSys* m_scale;          // +0x80
    void* m_modifier;                  // +0x84 Modifier
    int m_unknown88;                   // +0x88 cleared only by the constructor
    AtSceneAnimSet* m_animations;      // +0x8C
    AtSceneActor* m_sharedFrom;        // +0x90 whose coordinate system this has. A copy then does without its own
    void* m_dataLink;                  // +0x94 DataLink

    static void* s_heap;               // 0x002C7F5C
};

// Vftable 0x00276620. Size 0xA0.
class AtSceneModelActor : public AtSceneActor {
public:
    AtSceneModelActor();                           // 0x0004B0B0
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004B100
    ~AtSceneModelActor() override;                 // slot 7, 0x0003B850 deleting, for arrays too; 0x0004B5E0 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004AFE0 the base's
    void Clear() override;                         // slot 11, 0x0004B0E0
    void Slot12() override;                        // slot 12, 0x0004B090 not recovered
    void Slot14() override;                        // slot 14, 0x0004B310 not recovered
    void Slot15() override;                        // slot 15, 0x0004B3E0 not recovered
    void Slot34() override;                        // slot 34, 0x0004B5D0 not recovered
    void Slot38() override;                        // slot 38, 0x0004E5C0 not recovered
    void Slot39() override;                        // slot 39, 0x0004B220 not recovered
    void Slot42() override;                        // slot 42, 0x0004B270 not recovered
    virtual void SetMesh(void* mesh);              // slot 43, 0x0004B0A0

    void* m_mesh;                      // +0x98 Mesh
    int m_unknown9C;                   // +0x9C only ever cleared here
};

// Vftable 0x002766D0. Size 0xCC.
class AtSceneJointedModelActor : public AtSceneModelActor {
public:
    enum Flags { kOwnsJointDatabase = 0x40000, kSharesJoints = 0x80000, kAnimateAtDraw = 0x100000 };

    AtSceneJointedModelActor();                    // 0x0004B680
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004B9E0
    ~AtSceneJointedModelActor() override;          // slot 7, 0x0003B8B0 deleting, for arrays too; 0x0004C040 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004B650
    void Clear() override;                         // slot 11, 0x0004B7C0
    void Slot12() override;                        // slot 12, 0x0004B820 not recovered
    void ShareCoordSys(AtSceneActor* with) override;  // slot 32, 0x0004B940 not recovered
    void StopSharingCoordSys() override;           // slot 33, 0x0004B990 not recovered
    void Slot38() override;                        // slot 38, 0x0004BDE0 not recovered
    void Slot39() override;                        // slot 39, 0x0004BE70 not recovered
    void Slot42() override;                        // slot 42, 0x0004BEB0 not recovered

    AtCoordSysDatabase* CopyJointDatabase(AtCoordSysDatabase* from);  // 0x0004B6E0 the copy is this actor's own, and goes under it

    AtSceneAnimSet m_joints;           // +0xA0
    AtSceneJointedModelActor* m_jointsFrom;  // +0xC8 ShareCoordSysDatabase
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtScenePlayback) == 0x10, "AtScenePlayback");
static_assert(sizeof(AtSceneMatPlayback) == 0x14, "AtSceneMatPlayback");
static_assert(sizeof(AtSceneMatChannel) == 0x1C, "AtSceneMatChannel");
static_assert(sizeof(AtSceneAnimPlayback) == 0x28, "AtSceneAnimPlayback");
static_assert(sizeof(AtSceneAnimChannel) == 0x3C, "AtSceneAnimChannel");
static_assert(offsetof(AtSceneAnimation, m_frames) == 0x36, "AtSceneAnimation::m_frames");
static_assert(sizeof(AtSceneAnimBlend::Rotation) == 0x14, "AtSceneAnimBlend::Rotation");
static_assert(sizeof(AtSceneAnimBlend::Vector) == 0x10, "AtSceneAnimBlend::Vector");
static_assert(sizeof(AtSceneAnimBlend) == 0x18, "AtSceneAnimBlend");
static_assert(sizeof(AtSceneAnimSet) == 0x28, "AtSceneAnimSet");
static_assert(offsetof(AtSceneActor, m_matAssignment) == 0x74, "AtSceneActor::m_matAssignment");
static_assert(offsetof(AtSceneActor, m_scale) == 0x80, "AtSceneActor::m_scale");
static_assert(offsetof(AtSceneActor, m_animations) == 0x8C, "AtSceneActor::m_animations");
static_assert(offsetof(AtSceneActor, m_sharedFrom) == 0x90, "AtSceneActor::m_sharedFrom");
static_assert(sizeof(AtSceneActor) == 0x98, "AtSceneActor");
static_assert(offsetof(AtSceneModelActor, m_mesh) == 0x98, "AtSceneModelActor::m_mesh");
static_assert(sizeof(AtSceneModelActor) == 0xA0, "AtSceneModelActor");
static_assert(offsetof(AtSceneJointedModelActor, m_joints) == 0xA0, "AtSceneJointedModelActor::m_joints");
static_assert(sizeof(AtSceneJointedModelActor) == 0xCC, "AtSceneJointedModelActor");
#pragma pop_macro("offsetof")
#endif
