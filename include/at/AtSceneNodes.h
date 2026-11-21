// The classes of scene node no real .ASN file uses: GroupActor (vftable 0x00276870), LodGroupActor (0x00276920), SwitchGroupActor (0x002769D0), WaterActor (0x00276A80), LensFlareActor (0x00276B30) and SoundNode (0x00276BE0).
// Original address clusters 0x0004D410-0x0004D84D, 0x0004DA30-0x0004DE62, 0x0004E100-0x0004E321, 0x0004E460-0x0004EAFF, 0x0004EBA0-0x0004F244, 0x0003B980-0x0003BB54, with 0x00032310, 0x00032DF0-0x00032F34.
// The class and word names come from the keyword table at 0x002B1990; all other names are inferred from what the code does. None of the 652 distinct .ASN texts on the disc names any of these classes, so all of this rests on
// written text alone.
//
// Only reading, clearing and destroying are recovered, with the three slots that only pass a call on to the children. Drawing, updating and sound playback stay behind seams: the water's drawer, the emitter's two slots a sound node
// calls, and the other running-game slots, which stop with a diagnostic.
#pragma once

#include "at/AtSceneActor.h"
#include "at/AtSegmentQuery.h"

class AtAudioEmitter;
class AtAudioSegment;
class AtLensFlare;

// Words in the keyword table at 0x002B1990 these classes test for, apart from those in AtScene.h.
enum AtSceneNodeWord {
    kAtWordActor = 85, kAtWordSwitch = 87, kAtWordLod = 89, kAtWordLensFlare = 136, kAtWordLineOfSightTestEnable = 137, kAtWordLineOfSightTestDisable = 138, kAtWordPlaySegment = 152, kAtWordWaterPatch = 156,
    kAtWordPositionRect = 157, kAtWordTxtCoordRect = 158, kAtWordNeighbors = 159, kAtWordMask = 160, kAtWordDampingFactor = 161, kAtWordGenerationFactor = 162
};

// Vftable 0x00276870. Size 0xA8. Actors that share this one's coordinate system: each child takes it in its slot 32, and gives it back in its slot 33 when the group is cleared.
class AtSceneGroupActor : public AtSceneActor {
public:
    AtSceneGroupActor();                           // 0x0004D520
    // Slot 1, 0x0004D600. "Actor [n] { name ns ... }". The first list makes room for n; a later one fills from the start again, up to that room.
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;
    ~AtSceneGroupActor() override;                 // slot 7, 0x0003B980 deleting, for arrays too; 0x0004DA30 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004D410 keeps the owner, then the base's
    void Clear() override;                         // slot 11, 0x0004D7E0
    void Slot12() override;                        // slot 12, 0x0004D4D0 not recovered
    void Slot14() override;                        // slot 14, 0x0004D960 not recovered
    void Slot15() override;                        // slot 15, 0x0004D9C0 not recovered
    void Slot34() override;                        // slot 34, 0x0004D510 not recovered: what it passes on is not established
    void Slot38() override;                        // slot 38, 0x0004D430 not recovered: what it passes on is not established
    void Slot39() override;                        // slot 39, 0x0004D850 not recovered
    void Slot42() override;                        // slot 42, 0x0004D480 not recovered: what it passes on is not established

    void* m_readOwner;                 // +0x98 the owner given to Read, kept again here
    unsigned int m_capacity;           // +0x9C the first list's count
    unsigned int m_count;              // +0xA0
    AtSceneActor** m_children;         // +0xA4
};

// Vftable 0x00276920. Size 0xB0. Children in order of the distance given with each, nearest first; the distances are kept squared.
class AtSceneLodGroupActor : public AtSceneGroupActor {
public:
    AtSceneLodGroupActor();                        // 0x0004DB60
    // Slot 1, 0x0004DC10. "Lod [n] { name ns distance ... }". Each goes in by its distance. A later list adds to the earlier, with no check against the room.
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;
    ~AtSceneLodGroupActor() override;              // slot 7, 0x0003B9E0 deleting, for arrays too; 0x0004E100 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004DA80 the group's, through a jump. Also the switch group's slot, folded
    void Clear() override;                         // slot 11, 0x0004DE30
    void Slot14() override;                        // slot 14, 0x0004E020 not recovered
    void Slot15() override;                        // slot 15, 0x0004E090 not recovered
    void Slot38() override;                        // slot 38, 0x0004DA90 not recovered: what it passes on is not established
    void Slot39() override;                        // slot 39, 0x0004DF40 not recovered
    void Slot42() override;                        // slot 42, 0x0004DFA0 not recovered

    void Insert(AtSceneActor* child, float distanceSquared);  // 0x0004DAC0 before the first not nearer, and shares this one's coordinate system

    float* m_distances;                // +0xA8 squared
    int m_unknownAC;                   // +0xAC only ever cleared
};

// Vftable 0x002769D0. Size 0xAC. One child at a time, by number.
class AtSceneSwitchGroupActor : public AtSceneGroupActor {
public:
    AtSceneSwitchGroupActor();                     // 0x0004E300
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004E1D0
    ~AtSceneSwitchGroupActor() override;           // slot 7, 0x0003BA40 deleting, for arrays too; 0x0004E180 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004DA80, the level-of-detail group's, folded
    // Slot 11 is the group's, 0x0004D7E0, which leaves m_switch.
    void Slot12() override;                        // slot 12, 0x0004E290 not recovered
    void Slot14() override;                        // slot 14, 0x0004E380 not recovered
    void Slot15() override;                        // slot 15, 0x0004E3F0 not recovered
    void Slot34() override;                        // slot 34, 0x0004E2D0 not recovered: what it passes on is not established
    void Slot38() override;                        // slot 38, 0x0004E210 not recovered: what it passes on is not established
    void Slot39() override;                        // slot 39, 0x0004E330 not recovered
    void Slot42() override;                        // slot 42, 0x0004E250 not recovered: what it passes on is not established

    int m_switch;                      // +0xA8 Switch: the child drawn, if it is below the count
};

// One entry of WaterPatch. 0xB0 bytes. All zero, then the neighbours and the mask -1, before its words are read.
struct AtSceneWaterPatch {
    float m_position[4];               // +0x00 PositionRect
    float m_textureCoords[4];          // +0x10 TxtCoordRect
    int m_neighbors[4];                // +0x20 Neighbors
    int m_mask[32];                    // +0x30 Mask { 32 integers }
};

// What draws a water actor: made by slot 2 of the owner's drawer maker (include/at/AtSceneParticle.h) when the actor is read, and given back by its slot 3. A class of the running game's; nothing here makes one.
class AtSceneWaterDrawer {
public:
    virtual void Slot0() = 0;
    // Slot 1. Handed the patches, which the actor frees straight after: what it keeps of them is its own business.
    virtual void Start(AtSceneDevice* device, unsigned int patchCount, const AtSceneWaterPatch* patches, float rate, float damping, float generation) = 0;
};

// Vftable 0x00276A80. Size 0xB8.
class AtSceneWaterActor : public AtSceneActor {
public:
    AtSceneWaterActor();                           // 0x0004E4A0
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004E780
    ~AtSceneWaterActor() override;                 // slot 7, 0x0003BAA0 deleting, for arrays too; 0x0004E6F0 complete
    // Slot 10, 0x0004E4F0. Clears first if loaded; then the base's; then a drawer from the owner's maker, which is handed the patches, and the patches freed.
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;
    void Clear() override;                         // slot 11, 0x0004E570 only its own fields: not the actor's, which the destructor's base clears
    void Slot12() override;                        // slot 12, 0x0004E470 not recovered
    void Slot34() override;                        // slot 34, 0x0004E460 not recovered: returns m_matAssignment, which the base's declaration cannot
    void Slot35() override;                        // slot 35, 0x0004E650 not recovered
    void Slot38() override;                        // slot 38, 0x0004E5C0 not recovered
    void Slot39() override;                        // slot 39, 0x0004E6A0 not recovered

    AtSceneWaterDrawer* m_drawer;      // +0x98
    int m_unknown9C;                   // +0x9C not touched here
    int m_unknownA0;                   // +0xA0 not touched here
    unsigned int m_patchCount;         // +0xA4 WaterPatch
    float m_rate;                      // +0xA8 PlaybackRate, 60 to begin with
    float m_damping;                   // +0xAC DampingFactor
    float m_generation;                // +0xB0 GenerationFactor
    int m_matAssignmentOffset;         // +0xB4 MatAssignmentOffset

    // 0x002C7E14. The patches WaterPatch read, on the token file's heap, until Read hands them to the drawer and frees them. Not cleared after that.
    static AtSceneWaterPatch* s_patches;
};

// Vftable 0x00276B30. Size 0x13C.
class AtSceneLensFlareActor : public AtSceneActor {
public:
    enum Flags { kLineOfSightTest = 0x10000 };

    AtSceneLensFlareActor();                       // 0x0004EBE0
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004ECE0
    ~AtSceneLensFlareActor() override;             // slot 7, 0x0003BB00 deleting, for arrays too; 0x0004EF00 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004EBA0 both radii to 0.0001, then the base's
    void Clear() override;                         // slot 11, 0x0004ECA0
    void Slot34() override;                        // slot 34, 0x0004EEF0 not recovered: returns the flare's materials, which the base's declaration cannot
    void Slot35() override;                        // slot 35, 0x0004EBB0 not recovered
    void Slot38() override;                        // slot 38, 0x0004EDB0 not recovered
    void Slot39() override;                        // slot 39, 0x0004EDE0 not recovered

    AtLensFlare* m_flare;              // +0x98 LensFlare, from the owner's table
    int m_unknown9C;                   // +0x9C only ever cleared
    AtSegmentQuery m_query;            // +0xA0 for the line of sight, in slot 39
    int m_unknown138;                  // +0x138 only ever cleared
};

// What PlaySegment leaves for Read. 0xC bytes, on the token file's heap.
struct AtScenePendingSegment {
    AtAudioSegment* m_segment;         // +0x00 null when the owner has no sound owner
    int m_start;                       // +0x04 the integer after the names
    AtScenePendingSegment* m_next;     // +0x08
};

// Vftable 0x00276BE0. Size 0x70. Built on the plain node, so it has none of an actor's fields and is not an actor to AsActor. Its slot 7 is the modifier node's, 0x0003CD00: the destructors are the same code.
class AtSceneSoundNode : public AtSceneNode {
public:
    AtSceneSoundNode();                            // 0x0004EF90
    // Slot 1, 0x0004F0D0. Emitter and PlaySegment look in the sound owner, at the owner's +0x14; with none, Emitter does nothing and PlaySegment keeps a null segment.
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;
    // Slot 10, 0x0004EFB0. The base's; then each pending segment, last first, is played from the emitter, put where this node is, and freed. Sets bit 0x800 whatever.
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;
    void Clear() override;                         // slot 11, 0x0004F0B0
    void Slot12() override;                        // slot 12, 0x0004F250 not recovered

    AtAudioEmitter* m_emitter;         // +0x6C Emitter

    static AtScenePendingSegment* s_pending;  // 0x002C7E18 shared by every sound node: a body's PlaySegment words, until Read
};

// What a sound node asks of its emitter: slots 25 and 15 of AtAudioEmitter, vftable 0x0028B460, which include/at/AtAudioDatabase.h does not declare. Sound playback is not recovered; src/at/AtSceneNodesHost.cpp stops with a diagnostic, and a
// test may supply these instead. Replacing them later means declaring the two slots there and deleting these.
void AtSceneSoundSetVelocity(AtAudioEmitter* emitter, const AtVector3* velocity);   // slot 25, 0x0011F190 stores it at +0x34
void AtSceneSoundPlay(AtAudioEmitter* emitter, AtAudioSegment* segment, int unknown);  // slot 15, 0x0011F010

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtSceneGroupActor, m_children) == 0xA4, "AtSceneGroupActor::m_children");
static_assert(sizeof(AtSceneGroupActor) == 0xA8, "AtSceneGroupActor");
static_assert(offsetof(AtSceneLodGroupActor, m_distances) == 0xA8, "AtSceneLodGroupActor::m_distances");
static_assert(sizeof(AtSceneLodGroupActor) == 0xB0, "AtSceneLodGroupActor");
static_assert(offsetof(AtSceneSwitchGroupActor, m_switch) == 0xA8, "AtSceneSwitchGroupActor::m_switch");
static_assert(sizeof(AtSceneSwitchGroupActor) == 0xAC, "AtSceneSwitchGroupActor");
static_assert(sizeof(AtSceneWaterPatch) == 0xB0, "AtSceneWaterPatch");
static_assert(offsetof(AtSceneWaterActor, m_patchCount) == 0xA4, "AtSceneWaterActor::m_patchCount");
static_assert(offsetof(AtSceneWaterActor, m_matAssignmentOffset) == 0xB4, "AtSceneWaterActor::m_matAssignmentOffset");
static_assert(sizeof(AtSceneWaterActor) == 0xB8, "AtSceneWaterActor");
static_assert(offsetof(AtSceneLensFlareActor, m_query) == 0xA0, "AtSceneLensFlareActor::m_query");
static_assert(offsetof(AtSceneLensFlareActor, m_unknown138) == 0x138, "AtSceneLensFlareActor::m_unknown138");
static_assert(sizeof(AtSceneLensFlareActor) == 0x13C, "AtSceneLensFlareActor");
static_assert(sizeof(AtScenePendingSegment) == 0xC, "AtScenePendingSegment");
static_assert(offsetof(AtSceneSoundNode, m_emitter) == 0x6C, "AtSceneSoundNode::m_emitter");
static_assert(sizeof(AtSceneSoundNode) == 0x70, "AtSceneSoundNode");
#pragma pop_macro("offsetof")
#endif
