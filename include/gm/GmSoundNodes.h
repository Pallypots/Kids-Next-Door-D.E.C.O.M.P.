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
class GmAcActor;
class GmAcAudio;

// The actor the sound code plays at is GmAcActor (include/gm/GmAcActor.h): its slot 22 copies out its position (+0x48) and frame, its slot 52 (Unknown27C) is the
// float TakeDynamicNode is handed, +0x9C (m_sceneNode) the scene node a dynamic node plays from, +0xA0 (m_unknownA0) the node whose world position PlayNote reads,
// and +0x20C (m_component) the component whose +0x68 (m_audio, GmAcAudio) holds the actor's dynamic sound nodes. gm/gmsound declared these as GmAcActor,
// GmActorAudio and GmActorSoundNodes; folded on integrate-batch7.

// A Node block of a .GOO file. 0x90 bytes. The reals of each Parm word are stored second first, as the reader does.
struct GmSoundNode {
    char m_name[8];                  // +0x00
    char m_actor[8];                 // +0x08 Actor; the node's own name until a word says otherwise
    GmAcActor* m_owner;                // +0x10 the actor that took this node, in the first bank, which holds the pool of dynamic nodes (GLOBAL.GOO, 500 of them)
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
    GmAcActor* m_actor;                // +0x08 the last actor played at
};

// An actor's dynamic sound nodes, what its +0x20C, +0x68 points to, are GmAcAudio (include/gm/GmAcActor.h).

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
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
#pragma pop_macro("offsetof")
#endif
