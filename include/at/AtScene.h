// A scene, as read from an .ASN file: arrays of coordinate systems and motion states, then named nodes of several classes. What the format is for follows from the words in its keyword table at 0x002B1990 and from the original's
// own messages, which call this "Atlas Scene": position, rotation, bounding sphere and collision for each node; meshes, animation channels, particles and so on for the classes built on it. Scene names are inferred; keyword names are
// the original's. Original address clusters 0x0003BB60-0x0003CDFF, 0x0003D6F0-0x0003D91F, 0x0003E8C0-0x0003E91F, 0x00049800-0x0004AFDF, 0x0004F370-0x0004F4AF.
//
// The base node and the modifier node are here; ModelActor and JointedModelActor are in at/AtSceneActor.h. Every other class of node (AtSceneActorKind) is not recovered. All those arrays are made through the three AtSceneActors functions
// below. For a class not recovered, src/at/AtSceneActorsHost.cpp answers with a diagnostic, so a scene that has any stops there. A test may supply them.
#pragma once

#include "at/AtNameTableOwner.h"
#include "at/AtCoordSys.h"
#include "at/AtIoTokenizer.h"

class AtScene;
class AtSceneNode;

// Words in the keyword table. Only those this code tests for.
enum AtSceneWord {
    kAtWordNode = 57, kAtWordRotTransCoordSys = 58, kAtWordScaleCoordSys = 59, kAtWordMotionState = 60, kAtWordModifierNode = 61, kAtWordModifier = 62, kAtWordModelActor = 64, kAtWordJointedModelActor = 65, kAtWordCoordSysDatabase = 66,
    kAtWordCopyCoordSysDatabase = 67, kAtWordAnimationChannel = 68, kAtWordPlayAnimation = 69, kAtWordWeights = 70, kAtWordWeightMultiplier = 71, kAtWordPlaybackRate = 72, kAtWordMatAssignmentOffset = 80,
    kAtWordParticleSystemActor = 81, kAtWordEmitter = 82, kAtWordGroupActor = 84, kAtWordSwitchGroupActor = 86, kAtWordLodGroupActor = 88, kAtWordBoundingSphere = 90, kAtWordActiveDistance = 91, kAtWordMatAssignment = 92, kAtWordAttach = 93, kAtWordAttachBase = 94,
    kAtWordPosition = 95, kAtWordOrthoNormalize = 96, kAtWordQuaternion = 97, kAtWordLookAt = 98, kAtWordLinearVelocity = 99, kAtWordAngularVelocity = 100, kAtWordCollisionEnable = 101, kAtWordCollisionDisable = 102,
    kAtWordModificationEnable = 103, kAtWordModificationDisable = 104, kAtWordDrawingEnable = 105, kAtWordDrawingDisable = 106, kAtWordMatAnimationChannel = 109, kAtWordPlayMatAnimation = 110, kAtWordScale = 111, kAtWordDataLink = 112, kAtWordMesh = 113,
    kAtWordNameSpace = 114, kAtWordModifierReceiver = 115, kAtWordBoundingGeometry = 116, kAtWordLineSegIntersectEnable = 117, kAtWordLineSegIntersectDisable = 118, kAtWordAnimateAtDrawEnable = 119, kAtWordAnimateAtDrawDisable = 120,
    kAtWordShareCoordSysDatabase = 121, kAtWordCoordSysMirrorDatabase = 124, kAtWordCoordSysMirroringEnable = 125, kAtWordCoordSysMirroringDisable = 126, kAtWordProjectionReceiver = 127, kAtWordTextureCoordSysDatabase = 130,
    kAtWordCopyTextureCoordSysDatabase = 131, kAtWordShareTextureCoordSysDatabase = 132, kAtWordTextureCoordAnimationChannel = 133, kAtWordScaleTranslations = 134, kAtWordLensFlareActor = 135,
    kAtWordEnableBoundingGeometryDraw = 139, kAtWordEnableNodeBoundingSphereDraw = 140, kAtWordDisableBoundingGeometryDraw = 141, kAtWordDisableNodeBoundingSphereDraw = 142, kAtWordBoundingGeometryMatAssignment = 143,
    kAtWordBoundingSphereMatAssignment = 144, kAtWordWorldAdditiveInitialVelocity = 145, kAtWordWorldAdditiveAcceleration = 146, kAtWordWaterActor = 147, kAtWordSoundNode = 148, kAtWordAttachNodeToThisJoint = 149, kAtWordAttachNodeToThisBase = 150, kAtWordUpdateAtDraw = 151, kAtWordPlayParticles = 153, kAtWordPauseParticles = 154, kAtWordPhysicsActor = 163, kAtWordPhysicsActorInstance = 164, kAtWordPhysicsJointedModelActor = 166, kAtWordPhysicsEntitiesFile = 168, kAtWordAnimationSkeletonFile = 169,
    kAtWordRigidBodySkeletonFile = 170, kAtWordInitWithHavokPosition = 171, kAtWordInitWithHavokOrientation = 172
};

// Codes passed to the tokenizer's Error; the meanings are inferred from where they are used.
enum AtSceneProblem { kAtProblemIsEmpty = 0x23, kAtProblemNotFound = 0x25, kAtProblemWrongUse = 0x27 };

// The graphics device, at +4 in the owner: AtMaterialOwner (include/at/AtMaterial.h), by the name the scene code gave it first. One class: FindMesh 0x000283F0 and FindMatAssignment 0x00028430, which
// the scene's words call on this object, are the same code as AtMaterialOwner::FindMaterial 0x00027EF0 on its neighbouring tables, all three in AtMaterialOwner's cluster; every table the scene uses
// is one AtMaterialOwner's constructor 0x00026190 builds at that offset; and slot 17 is the same slot. See docs/worklog/2026-11-08-gfxdev.md.
#include "at/AtMaterial.h"

// A bounding sphere.
struct AtSceneSphere {
    AtSceneSphere();                           // 0x0017AED0 clears it
    void Set(const AtSceneSphere& from);       // 0x00039F80
    AtVector3 center;
    float radius;
};

// One node's membership of a space graph, in one of four kinds: a NodeLink in the file's words. See include/at/AtSpaceGraph.h. Made by the graph; released by it, in src/at/AtSceneNode.cpp.
class AtSpaceGraph;
class AtLeafSpace;
struct AtSceneLinkItem;
struct AtSceneLink {
    AtSceneLink();                     // 0x00047DC0 clears it
    AtSpaceGraph* m_set;               // +0x00 null when free
    AtVector3 m_center;                // +0x04 of the node's sphere when it was placed
    float m_margin;                    // +0x10 how far it can move and stay in its leaves; squared once placed
    AtSceneLink* m_next;               // +0x14 among the node's, or among the graph's free ones
    AtSceneLinkItem* m_items;          // +0x18
    AtSceneLink* m_nextDirty;          // +0x1C
};

// A link's presence in one leaf: a NodeIncidence.
struct AtSceneLinkItem {
    AtSceneLinkItem();                 // 0x00047DC0 the same code as the link's, folded
    float m_low;                       // +0x00 the node's extent along the leaf's axis, for kinds 0 and 1
    float m_high;                      // +0x04
    AtLeafSpace* m_list;               // +0x08
    AtSceneNode* m_node;               // +0x0C
    AtSceneLinkItem* m_next;           // +0x10 among a link's, or among the graph's free ones
    AtSceneLinkItem* m_head;           // +0x14 the link's first
    AtSceneLinkItem* m_prevInList;     // +0x18 towards the tail of the leaf's list, despite the name it got first
    AtSceneLinkItem* m_nextInList;     // +0x1C towards the leaf's first

    void Unlink(unsigned int kind);    // 0x000467F0
    void AddSecond(AtSceneLinkItem* added);  // 0x000467C0 another item for the same link. In src/at/AtSpace.cpp
};

// What 0x00049CA0 starts a node from. Its users are not recovered.
struct AtSceneNodeDesc {
    int m_collision;                   // +0x00
    AtSceneSphere m_sphere;            // +0x04
    float m_activeDistance;            // +0x14
    int m_unknown18;                   // +0x18
    AtCoordSys* m_coordSys;            // +0x1C null: left as it is
    struct AtSceneMotionState* m_motion;  // +0x20 null: left as it is
};

// 0x30 bytes.
struct AtSceneMotionState {
    AtVector3 m_linearVelocity;        // +0x00
    float m_unknown0C[3];              // +0x0C
    AtVector3 m_angularVelocity;       // +0x18
    float m_unknown24[3];              // +0x24

    AtSceneMotionState();                      // 0x0004F5A0 clears it
    void Set(const AtSceneMotionState& from);  // 0x0004F5D0
};

// Vftable 0x002765A0. Slots 12 and 13 to 31 belong to moving and testing nodes in a running game; they are not recovered, and stop with a diagnostic. Their names say only where they are.
class AtSceneNode {
public:
    enum Flags {
        kLoaded = 0x1, kCollision = 0x2, kMoved = 0x4, kOwnsCoordSys = 0x8, kOwnsMotion = 0x10, kInDirtyList0 = 0x20, kInDirtyList1 = 0x40, kInDirtyList2 = 0x80, kInDirtyList3 = 0x100, kLineSegIntersect = 0x200,
        kModificationEnabled = 0x800
    };

    AtSceneNode();                                 // 0x0004A740
    virtual void ReadHeader(AtScene* scene, AtIoTokenizer* tokenizer, int token);  // slot 0, 0x00049800 a word between the name and the curly bracket
    virtual void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token);  // slot 1, 0x0004AA10 a word inside the curly brackets
    virtual void FinishBody();                     // slot 2, 0x00049F30 the sphere in world space
    virtual void OnMoved();                        // slot 3, 0x0004A040 once, until something clears kMoved: joins each kind's dirty list, and so does everything attached
    virtual AtCoordSys* CoordSys();                // slot 4, 0x000498A0
    virtual AtCoordSys* ParentCoordSys();          // slot 5, 0x000498A0 the same code; other classes differ
    virtual void FinishHeader();                   // slot 6, 0x001CC9F0 empty here, folded with others
    virtual ~AtSceneNode();                        // slot 7, 0x0003B7F0 deleting, for arrays too; 0x0004AFD0 complete
    virtual void CopyFrom(AtNameTableOwner* owner, const AtSceneNode* from);  // slot 8, 0x00049A80
    virtual void InitFrom(AtNameTableOwner* owner, const AtSceneNodeDesc* desc);  // slot 9, 0x00049CA0
    virtual void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer);  // slot 10, 0x000499F0
    virtual void Clear();                          // slot 11, 0x0004AF30
    virtual void Slot12();                         // slot 12, 0x00049870 not recovered
    virtual void Slot13();                         // slot 13, 0x000CBF30 not recovered
    virtual void Slot14();                         // slot 14, 0x0004A110 not recovered
    virtual void Slot15();                         // slot 15, 0x000498E0 not recovered
    virtual void Slot16();                         // slot 16, 0x0004A640 not recovered
    virtual void Slot17();                         // slot 17, 0x0004A6A0 not recovered
    virtual void Slot18();                         // slot 18, 0x0004A5E0 not recovered
    virtual void Slot19();                         // slot 19, 0x0004A610 not recovered
    virtual void Slot20();                         // slot 20, 0x0004A800 not recovered
    virtual void Slot21();                         // slot 21, 0x0004A7D0 not recovered
    virtual void Slot22();                         // slot 22, 0x0004A7A0 not recovered
    virtual void Slot23();                         // slot 23, 0x0004A770 not recovered
    virtual void Slot24();                         // slot 24, 0x0004A830 not recovered
    virtual void Slot25();                         // slot 25, 0x0004A870 not recovered
    virtual void Slot26();                         // slot 26, 0x0004A910 not recovered
    virtual void Slot27();                         // slot 27, 0x0004A8E0 not recovered
    virtual void Slot28();                         // slot 28, 0x0004A8A0 not recovered
    virtual void Slot29();                         // slot 29, 0x0004A950 not recovered
    virtual void Slot30();                         // slot 30, 0x0004A990 not recovered
    virtual void Slot31();                         // slot 31, 0x0004A9D0 not recovered

    void Reset();                                  // 0x00049F80 not the class word, and not +0x68
    void ReleaseCoordSys();                        // 0x00049FE0
    void RemoveChild(AtSceneNode* child);          // 0x00049920
    void AddChild(AtSceneNode* child);             // 0x00049900 nothing if it is already this one's. It stays in any other's list
    void AttachBase(AtSceneNode* parent);          // 0x0004A210
    void AttachToJoint(AtSceneNode* parent, const char* joint);  // 0x0004A310 parent is a jointed actor; nothing happens unless it has that joint
    void GetWorldPosition(AtVector3* out);         // 0x00049990 the coordinate system's; zero with none. src/at/AtAudioVoices.cpp
    void GetWorldSphere(AtSceneSphere* out);       // 0x00136C60, a copy the sound code has: m_worldSphere, after FinishBody if kMoved. src/gm/GmSoundPlay.cpp

    static AtSceneNode* AsJointed(AtSceneNode* node);  // 0x000559B0 reports null, or another class, and hands it back regardless
    static void SetHeap(void* heap);               // 0x000497F0

    unsigned int m_flags;              // +0x04
    unsigned short m_class;            // +0x08 0 here, 14 in a modifier node; others in the actors
    AtSceneSphere m_sphere;            // +0x0C
    AtSceneSphere m_worldSphere;       // +0x1C
    float m_activeDistanceSquared;     // +0x2C
    AtNameTableOwner* m_owner;             // +0x30
    AtCoordSys* m_coordSys;            // +0x34
    AtSceneMotionState* m_motion;      // +0x38
    void* m_boundingGeometry;          // +0x3C
    int m_unknown40;                   // +0x40
    AtSceneLink* m_links[4];           // +0x44 one per kind
    AtSceneNode* m_parent;             // +0x54
    AtSceneNode* m_nextChild;          // +0x58
    AtSceneNode* m_firstChild;         // +0x5C
    int m_unknown60;                   // +0x60 -1 when reset
    int m_unknown64;                   // +0x64 -1 when reset
    int m_unknown68;                   // +0x68 cleared only by the constructor

    static void* s_heap;               // 0x002C7E0C
};

// Vftable 0x00276C68.
class AtSceneModifierNode : public AtSceneNode {
public:
    AtSceneModifierNode();                         // 0x0004F3A0
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004F3F0
    ~AtSceneModifierNode() override;               // slot 7, 0x0003CD00 deleting, for arrays too; 0x0003B910 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004F370 the base's
    void Clear() override;                         // slot 11, 0x0004F3C0
    void Slot12() override;                        // slot 12, 0x0004F380 not recovered

    void* m_modifier;                  // +0x6C
};

// The classes with an array in a scene, in the order of the scene's fields.
enum AtSceneActorKind {
    kAtActorModel, kAtActorJointedModel, kAtActorParticleSystem, kAtActorWater, kAtActorLensFlare, kAtActorGroup, kAtActorSwitchGroup, kAtActorLodGroup, kAtActorSound, kAtActorKinds
};

unsigned int AtSceneActorsSize(int kind);                                         // the size of one
void AtSceneActorsConstruct(int kind, void* first, unsigned int count);           // given room for count
void AtSceneActorsDestroy(int kind, void* first, unsigned int count);             // last first; not the memory
unsigned int AtSceneUnrecoveredActorsSize(int kind);                              // The same three, for the classes not recovered. src/at/AtSceneActorsHost.cpp
void AtSceneUnrecoveredActorsConstruct(int kind, void* first, unsigned int count);
void AtSceneUnrecoveredActorsDestroy(int kind, void* first, unsigned int count);
void* AtSceneFindDataLink(AtNameTableOwner* owner, const char* path, const char* ns);  // 0x0005F130 on the owner's +0x10, whose class is not recovered. No real scene file uses the word

// Vftable 0x00275A44. A name map of nodes.
class AtScene : public GmNameMap {
public:
    enum { kKeywordCount = 116 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B1990

    AtScene();                                     // 0x0003BC70
    ~AtScene() override;                           // slot 0, 0x0002E640 deleting, for arrays too; 0x0003E8C0 complete
    void Clear() override;                         // slot 1, 0x0003D6F0

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003CD60 name + ".ASN"
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003BCD0
    // NOT recovered: each passes the graph to four functions over the scene's arrays (0x0003D920, 0x0003DDB0, 0x0003E2B0, 0x0003E7C0, and 0x0003DBB0, 0x0003E070, 0x0003E580, 0x0003E810).
    // The names are inferred from the pairing; the manifest's dynamic nodes are their callers. Stop with a diagnostic in src/gm/GmLevelManifestHost.cpp.
    void InsertInto(AtSpaceGraph* graph);                              // 0x0003E860
    void RemoveFrom(AtSpaceGraph* graph);                              // 0x0003E890

    AtRotTransCoordSys* NextRotTrans(AtIoTokenizer* tokenizer);    // 0x0003D8C0 reports running out, and hands one back regardless
    AtScaleCoordSys* NextScale(AtIoTokenizer* tokenizer);          // 0x0003D8F0 likewise
    AtSceneMotionState* NextMotionState(AtIoTokenizer* tokenizer);  // 0x0003BC40 likewise

    static void SetHeap(void* heap);               // 0x0003B790

    AtCoordSysDatabase m_coordSys;     // +0x14
    AtSceneMotionState* m_motionStates;  // +0x4C
    unsigned int m_rotTransCount;      // +0x50
    unsigned int m_scaleCount;         // +0x54
    unsigned int m_motionCount;        // +0x58
    unsigned int m_rotTransUsed;       // +0x5C
    unsigned int m_scaleUsed;          // +0x60
    unsigned int m_motionUsed;         // +0x64
    unsigned int m_nodeCount;          // +0x68
    unsigned int m_modifierNodeCount;  // +0x6C
    unsigned int m_unknown70;          // +0x70
    unsigned int m_modelCount;         // +0x74
    unsigned int m_jointedModelCount;  // +0x78
    unsigned int m_unknown7C[4];       // +0x7C
    unsigned int m_particleSystemCount;  // +0x8C
    unsigned int m_waterCount;         // +0x90
    unsigned int m_lensFlareCount;     // +0x94
    unsigned int m_groupCount;         // +0x98
    unsigned int m_switchGroupCount;   // +0x9C
    unsigned int m_lodGroupCount;      // +0xA0
    unsigned int m_soundCount;         // +0xA4
    unsigned int m_unknownA8[3];       // +0xA8
    AtSceneNode* m_nodes;              // +0xB4
    AtSceneModifierNode* m_modifierNodes;  // +0xB8
    void* m_unknownBC;                 // +0xBC
    void* m_models;                    // +0xC0
    void* m_jointedModels;             // +0xC4
    void* m_unknownC8[4];              // +0xC8
    void* m_particleSystems;           // +0xD8
    void* m_water;                     // +0xDC
    void* m_lensFlares;                // +0xE0
    void* m_groups;                    // +0xE4
    void* m_switchGroups;              // +0xE8
    void* m_lodGroups;                 // +0xEC
    void* m_sounds;                    // +0xF0
    AtNameTableOwner* m_owner;             // +0xF4 the constructor leaves it

    static void* s_heap;               // 0x002C640C

private:
    void ResetFields();                            // 0x0003BB60 all but the owner
    // A count in brackets and a list of named nodes, of a class not recovered.
    void ReadActors(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int kind, int word, unsigned int* count, void** actors);
};

void AtConstructEach(void* first, unsigned int size, int count, void (*construct)(void*));  // 0x00011000 the original's takes a thiscall pointer

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtSceneDevice, m_meshes) == 0xF0, "AtSceneDevice::m_meshes");
static_assert(offsetof(AtSceneDevice, m_matAssignments) == 0x1A4, "AtSceneDevice::m_matAssignments");
static_assert(offsetof(AtSceneDevice, m_matAnimationSpaces) == 0x1F8, "AtSceneDevice::m_matAnimationSpaces");
static_assert(offsetof(AtSceneDevice, m_lights) == 0x168, "AtSceneDevice::m_lights");
static_assert(offsetof(AtSceneDevice, m_lightAnimationSpaces) == 0x2E8, "AtSceneDevice::m_lightAnimationSpaces");
static_assert(offsetof(AtSceneDevice, m_coordSystems) == 0x30C, "AtSceneDevice::m_coordSystems");
static_assert(offsetof(AtSceneDevice, m_animations) == 0x354, "AtSceneDevice::m_animations");
static_assert(sizeof(AtSceneLink) == 0x20, "AtSceneLink");
static_assert(sizeof(AtSceneLinkItem) == 0x20, "AtSceneLinkItem");
static_assert(sizeof(AtSceneMotionState) == 0x30, "AtSceneMotionState");
static_assert(offsetof(AtSceneNode, m_class) == 0x08, "AtSceneNode::m_class");
static_assert(offsetof(AtSceneNode, m_owner) == 0x30, "AtSceneNode::m_owner");
static_assert(offsetof(AtSceneNode, m_links) == 0x44, "AtSceneNode::m_links");
static_assert(offsetof(AtSceneNode, m_unknown68) == 0x68, "AtSceneNode::m_unknown68");
static_assert(sizeof(AtSceneNode) == 0x6C, "AtSceneNode");
static_assert(sizeof(AtSceneModifierNode) == 0x70, "AtSceneModifierNode");
static_assert(offsetof(AtScene, m_coordSys) == 0x14, "AtScene::m_coordSys");
static_assert(offsetof(AtScene, m_motionStates) == 0x4C, "AtScene::m_motionStates");
static_assert(offsetof(AtScene, m_nodeCount) == 0x68, "AtScene::m_nodeCount");
static_assert(offsetof(AtScene, m_modelCount) == 0x74, "AtScene::m_modelCount");
static_assert(offsetof(AtScene, m_particleSystemCount) == 0x8C, "AtScene::m_particleSystemCount");
static_assert(offsetof(AtScene, m_soundCount) == 0xA4, "AtScene::m_soundCount");
static_assert(offsetof(AtScene, m_nodes) == 0xB4, "AtScene::m_nodes");
static_assert(offsetof(AtScene, m_models) == 0xC0, "AtScene::m_models");
static_assert(offsetof(AtScene, m_particleSystems) == 0xD8, "AtScene::m_particleSystems");
static_assert(offsetof(AtScene, m_sounds) == 0xF0, "AtScene::m_sounds");
static_assert(offsetof(AtScene, m_owner) == 0xF4, "AtScene::m_owner");
static_assert(sizeof(AtScene) == 0xF8, "AtScene");
#pragma pop_macro("offsetof")
#endif
