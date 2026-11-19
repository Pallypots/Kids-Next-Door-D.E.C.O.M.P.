// The sound manager's other records: sound nodes (.GOO), collision sounds (.GOC) and instance sounds (.GOI), the forty voice slots, and the little of a game actor that playing a note
// reads. All names are inferred, from the keywords in the files at 0x002B8FD0, 0x002B9038 and 0x002B9078, which are the original's, and from what the code does with each field.
// Original address clusters 0x001367A0-0x0013683F, 0x00136A10-0x00136BAF, 0x00136C60-0x00136C8C, 0x00137660-0x0013777E, 0x00137F50-0x001380D9, 0x00138290-0x001382F2,
// 0x001386D0-0x0013893B, 0x00138A10-0x00138F64, 0x00138F70-0x0013908A, 0x00139610-0x00139E80, 0x00139F90-0x0013A068, 0x0013AA80-0x0013AB42, 0x00194BB0-0x00194ECB.
#pragma once

class AtAudioEmitter;
class AtAudioSegment;
class AtSceneNode;
class AtVector3;
class GmAudioData;
class GmActorSoundNodes;

// The base class of the game's actors, whose constructor is 0x0014B5D0 and vftable 0x0028E4C8. Not recovered: only the slots and fields the sound code reads are named, and the other slots
// stay pure, so that nothing here stands for code that is not recovered.
class GmActor {
public:
    virtual void Unknown0() = 0;
    virtual void Unknown1() = 0;
    virtual void Unknown2() = 0;
    virtual void Unknown3() = 0;
    virtual void Unknown4() = 0;
    virtual void Unknown5() = 0;
    virtual void Unknown6() = 0;
    virtual void Unknown7() = 0;
    virtual void Unknown8() = 0;
    virtual void Unknown9() = 0;
    virtual void Unknown10() = 0;
    virtual void Unknown11() = 0;
    virtual void Unknown12() = 0;
    virtual void Unknown13() = 0;
    virtual void Unknown14() = 0;
    virtual void Unknown15() = 0;
    virtual void Unknown16() = 0;
    virtual void Unknown17() = 0;
    virtual void Unknown18() = 0;
    virtual void Unknown19() = 0;
    virtual void Unknown20() = 0;
    virtual void Unknown21() = 0;
    // Slot 22, 0x0014BD90 in the base: copies five vectors of the actor, each where its pointer is not null, the first being the position (+0x48).
    virtual void GetMotion(AtVector3* position, AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d) = 0;
    virtual void Unknown23() = 0;
    virtual void Unknown24() = 0;
    virtual void Unknown25() = 0;
    virtual void Unknown26() = 0;
    virtual void Unknown27() = 0;
    virtual void Unknown28() = 0;
    virtual void Unknown29() = 0;
    virtual void Unknown30() = 0;
    virtual void Unknown31() = 0;
    virtual void Unknown32() = 0;
    virtual void Unknown33() = 0;
    virtual void Unknown34() = 0;
    virtual void Unknown35() = 0;
    virtual void Unknown36() = 0;
    virtual void Unknown37() = 0;
    virtual void Unknown38() = 0;
    virtual void Unknown39() = 0;
    virtual void Unknown40() = 0;
    virtual void Unknown41() = 0;
    virtual void Unknown42() = 0;
    virtual void Unknown43() = 0;
    virtual void Unknown44() = 0;
    virtual void Unknown45() = 0;
    virtual void Unknown46() = 0;
    virtual void Unknown47() = 0;
    virtual void Unknown48() = 0;
    virtual void Unknown49() = 0;
    virtual void Unknown50() = 0;
    virtual void Unknown51() = 0;
    virtual float Unknown52() = 0;   // slot 52, 0x00091850 in the base. GmActorSoundNodes::Play passes it to GmAudioData::TakeDynamicNode, which ignores it

    char m_unknown04[0x98];
    AtSceneNode* m_soundSource;      // +0x9C a dynamic node plays from its world sphere
    AtSceneNode* m_node;             // +0xA0 PlayNote reads its world position
    char m_unknownA4[0x168];
    struct GmActorAudio* m_audio;    // +0x20C
};

// What an actor's +0x20C points to. Not recovered: only this member is read here.
struct GmActorAudio {
    char m_unknown00[0x68];
    GmActorSoundNodes* m_soundNodes;  // +0x68 when null, PlayNote plays at the actor through a voice slot instead
};

// A Node block of a .GOO file. 0x90 bytes. The reals of each Parm word are stored second first, as the reader does.
struct GmSoundNode {
    char m_name[8];                  // +0x00
    char m_actor[8];                 // +0x08 Actor; the node's own name until a word says otherwise
    GmActor* m_owner;                // +0x10 the actor that took this node, in the first bank, which holds the pool of dynamic nodes (GLOBAL.GOO, 500 of them)
    AtSceneNode* m_source;           // +0x14 that actor's +0x9C
    AtAudioEmitter* m_emitter;       // +0x18 Emitter, by name in the sound owner's table; or a voice slot, taken when the node first plays
    char m_segmentName[8];           // +0x1C Segment
    AtAudioSegment* m_segment;       // +0x24 found by RegisterNodes; also a looping segment waiting to come within range
    char m_velocityParm[8];          // +0x28 VelocityParm
    float m_velocitySecond;          // +0x30
    float m_velocityFirst;           // +0x34
    char m_angularVelocityParm[8];   // +0x38 AngularVelocityParm
    float m_angularVelocitySecond;   // +0x40
    float m_angularVelocityFirst;    // +0x44
    char m_customParm[8];            // +0x48 CustomParm
    float m_customSecond;            // +0x50
    float m_customFirst;             // +0x54
    int m_unknown58;                 // +0x58
    char m_analogInputThird[8];      // +0x5C AnalogInputParm's third name
    float m_analogInputSecond;       // +0x64
    float m_analogInputFirst;        // +0x68
    char m_analogInputParm[8];       // +0x6C its first name
    char m_analogInputSecondName[8]; // +0x74
    float m_range;                   // +0x7C the segment's bank slot 12, not MaxDistanceFromListener, whose value is never stored
    int m_pauseWhenAirborne;         // +0x80 PauseWhenAirborne
    int m_unknown84;                 // +0x84
    int m_triggeredOnly;             // +0x88 TriggeredOnly
    int m_active;                    // +0x8C TriggeredOnly clears it; PlayNode sets it
};

// The manager has sixty, at +0x21BC. The first is the pool of dynamic nodes.
struct GmSoundNodeBank {
    GmSoundNode* m_nodes;            // +0x00
    unsigned int m_count;            // +0x04
    int m_used;                      // +0x08
};

struct GmCollisionMaterial {
    char m_material[8];              // +0x00
    char m_segment[8];               // +0x08
};

// A CollisionSoundNode block of a .GOC file. 0x40 bytes. No .GOC file is on the disc.
struct GmCollisionSound {
    char m_name[8];                  // +0x00
    AtAudioEmitter* m_emitter;       // +0x08 DedicatedEmitter: a voice slot, taken as the word is read
    char m_defaultSegments[3][8];    // +0x0C DefaultSegment, three names
    unsigned int m_materialCount;    // +0x24 MaterialSegments
    GmCollisionMaterial* m_materials;  // +0x28
    int m_playerInitiatedOnly;       // +0x2C PlayerInitiatedOnly
    int m_ragdollOnly;               // +0x30 RagdollOnly
    float m_impulseThresholds[3];    // +0x34 ImpulseThresholds; the manager's three until then
};

// Ten at +0x248C.
struct GmCollisionSoundBank {
    GmCollisionSound* m_sounds;      // +0x00
    unsigned int m_count;            // +0x04
    int m_used;                      // +0x08
};

// An InstanceSoundNode block of a .GOI file. 0x18 bytes. No .GOI file is on the disc.
struct GmInstanceSound {
    char m_name[8];                  // +0x00 the collision sound of the same name is this one's
    GmCollisionSound* m_collision;   // +0x08 found by RegisterInstanceSounds
    unsigned int m_instanceCount;    // +0x0C Instances
    char (*m_instances)[8];          // +0x10
    void** m_unknown14;              // +0x14 allocated alongside, four bytes an instance, and not written here
};

// Ten at +0x2504. The used flag comes first here, unlike the other banks.
struct GmInstanceSoundBank {
    int m_used;                      // +0x00
    unsigned int m_count;            // +0x04
    GmInstanceSound* m_sounds;       // +0x08
};

// One of the forty voices at +0xD0C: an emitter of the sound owner's actor1 to actor40, lent out.
struct GmSoundVoiceSlot {
    enum State { kFree = 0x65, kNode = 0x66, kActor = 0x67 };  // kNode: taken by TakeVoiceSlot, for a node or a collision sound. kActor: by PlayAtActor, and not given back by anything recovered

    int m_state;                     // +0x00
    AtAudioEmitter* m_emitter;       // +0x04 set by FindEmitters
    GmActor* m_actor;                // +0x08 the last actor played at
};

// An actor's dynamic sound nodes: what its +0x20C, +0x68 points to. The class is not recovered beyond these two methods and the fields they read.
class GmActorSoundNodes {
public:
    void StopSegment(AtAudioSegment* segment, int index);  // 0x00194BB0 on the emitter of the node at index, if there is one
    // 0x00194E40. Takes a dynamic node for that index the first time. The position is not used. With singleton, nothing if the segment already plays on the node's emitter.
    void Play(AtAudioSegment* segment, int index, const AtVector3* position, int singleton);

    char m_unknown00[0x18];
    GmAudioData* m_manager;          // +0x18
    int m_count;                     // +0x1C compared signed
    GmSoundNode** m_nodes;           // +0x20
    char m_unknown24[0xC];
    GmActor* m_actor;                // +0x30
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(GmActor, m_soundSource) == 0x9C, "GmActor::m_soundSource");
static_assert(offsetof(GmActor, m_audio) == 0x20C, "GmActor::m_audio");
static_assert(offsetof(GmActorAudio, m_soundNodes) == 0x68, "GmActorAudio::m_soundNodes");
static_assert(offsetof(GmSoundNode, m_segment) == 0x24, "GmSoundNode::m_segment");
static_assert(offsetof(GmSoundNode, m_unknown58) == 0x58, "GmSoundNode::m_unknown58");
static_assert(offsetof(GmSoundNode, m_range) == 0x7C, "GmSoundNode::m_range");
static_assert(sizeof(GmSoundNode) == 0x90, "GmSoundNode");
static_assert(sizeof(GmSoundNodeBank) == 0xC, "GmSoundNodeBank");
static_assert(offsetof(GmCollisionSound, m_materialCount) == 0x24, "GmCollisionSound::m_materialCount");
static_assert(offsetof(GmCollisionSound, m_impulseThresholds) == 0x34, "GmCollisionSound::m_impulseThresholds");
static_assert(sizeof(GmCollisionSound) == 0x40, "GmCollisionSound");
static_assert(sizeof(GmInstanceSound) == 0x18, "GmInstanceSound");
static_assert(sizeof(GmSoundVoiceSlot) == 0xC, "GmSoundVoiceSlot");
static_assert(offsetof(GmActorSoundNodes, m_manager) == 0x18, "GmActorSoundNodes::m_manager");
static_assert(offsetof(GmActorSoundNodes, m_actor) == 0x30, "GmActorSoundNodes::m_actor");
#pragma pop_macro("offsetof")
#endif
