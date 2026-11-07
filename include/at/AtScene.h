// A scene, as read from an .ASN file: arrays of coordinate systems and motion states, then named nodes of several classes. What the format is for follows from the words in its keyword table at 0x002B1990 and from the original's
// own messages, which call this "Atlas Scene": position, rotation, bounding sphere and collision for each node; meshes, animation channels, particles and so on for the classes built on it. Scene names are inferred; keyword names are
// the original's. Original address clusters 0x0003BB60-0x0003CDFF, 0x0003D6F0-0x0003D91F, 0x0003E8C0-0x0003E91F, 0x00049800-0x0004AFDF, 0x0004F370-0x0004F4AF.
//
// Only the base node and the modifier node are recovered. Every other class of node (AtSceneActorKind) is not: their code is about meshes, particles and joints, and reaches the graphics layer. Those arrays are made through the three
// AtSceneActors functions below, which src/at/AtSceneActorsHost.cpp answers with a diagnostic, so a scene that has any stops there. A test may supply them.
#pragma once

#include "at/AtCoordSys.h"
#include "at/AtIoTokenizer.h"

class AtScene;
class AtSceneNode;

// Words in the keyword table. Only those this code tests for.
enum AtSceneWord {
    kAtWordNode = 57, kAtWordRotTransCoordSys = 58, kAtWordScaleCoordSys = 59, kAtWordMotionState = 60, kAtWordModifierNode = 61, kAtWordModifier = 62, kAtWordModelActor = 64, kAtWordJointedModelActor = 65,
    kAtWordParticleSystemActor = 81, kAtWordGroupActor = 84, kAtWordSwitchGroupActor = 86, kAtWordLodGroupActor = 88, kAtWordBoundingSphere = 90, kAtWordActiveDistance = 91, kAtWordAttach = 93, kAtWordAttachBase = 94,
    kAtWordPosition = 95, kAtWordOrthoNormalize = 96, kAtWordQuaternion = 97, kAtWordLookAt = 98, kAtWordLinearVelocity = 99, kAtWordAngularVelocity = 100, kAtWordCollisionEnable = 101, kAtWordCollisionDisable = 102,
    kAtWordModificationEnable = 103, kAtWordModificationDisable = 104, kAtWordNameSpace = 114, kAtWordBoundingGeometry = 116, kAtWordLineSegIntersectEnable = 117, kAtWordLineSegIntersectDisable = 118, kAtWordLensFlareActor = 135,
    kAtWordEnableBoundingGeometryDraw = 139, kAtWordEnableNodeBoundingSphereDraw = 140, kAtWordDisableBoundingGeometryDraw = 141, kAtWordDisableNodeBoundingSphereDraw = 142, kAtWordBoundingGeometryMatAssignment = 143,
    kAtWordBoundingSphereMatAssignment = 144, kAtWordWaterActor = 147, kAtWordSoundNode = 148, kAtWordAttachNodeToThisJoint = 149, kAtWordAttachNodeToThisBase = 150, kAtWordPhysicsJointedModelActor = 166
};

// Codes passed to the tokenizer's Error; the meanings are inferred from where they are used.
enum AtSceneProblem { kAtProblemIsEmpty = 0x23, kAtProblemNotFound = 0x25, kAtProblemWrongUse = 0x27 };

// The graphics device, at +4 in the owner. Only its slot 17 is used here.
class AtSceneDevice {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void Slot9() = 0;
    virtual void Slot10() = 0;
    virtual void Slot11() = 0;
    virtual void Slot12() = 0;
    virtual void Slot13() = 0;
    virtual void Slot14() = 0;
    virtual void Slot15() = 0;
    virtual void Slot16() = 0;
    virtual AtMatrixDevice* MatrixDevice() = 0;  // slot 17
};

// What scenes load for, known here by the tables they use. Most of it is not recovered, and its constructor is not known; these members' own constructors stand in. A different class from AtMaterialOwner, as far as these offsets show.
struct AtSceneOwner {
    AtSceneNode* FindNode(const char* name, const char* ns);       // 0x00045680 ns may be empty or null
    void AddScene(GmNameMap* scene, const char* ns);               // 0x00031A90 ns may be empty, which is a name space with an empty name, or null
    void* FindBoundingGeometry(const char* name, const char* ns);  // 0x000499B0 ns may be empty or null

    int m_unknown00;                   // +0x00
    AtSceneDevice* m_device;           // +0x04
    char m_unknown08[0x88];
    GmNameMapGroup m_nodes;            // +0x90
    GmNameSpaceDb m_nodeSpaces;        // +0xA8
    GmNameMapGroup m_modifiers;        // +0xCC
    GmNameSpaceDb m_modifierSpaces;    // +0xE4
    char m_unknown108[0x78];
    GmNameMapGroup m_geometry;         // +0x180
    GmNameSpaceDb m_geometrySpaces;    // +0x198
};

// A bounding sphere.
struct AtSceneSphere {
    AtSceneSphere();                           // 0x0017AED0 clears it
    void Set(const AtSceneSphere& from);       // 0x00039F80
    AtVector3 center;
    float radius;
};

// One node's membership of a modifier set. Made by code not recovered; released here.
struct AtSceneLinkItem;
struct AtSceneLink {
    class AtSceneModifierSet* m_set;   // +0x00 null when free
    int m_unknown04[4];                // +0x04
    AtSceneLink* m_next;               // +0x14 among the node's, or among the set's free ones
    AtSceneLinkItem* m_items;          // +0x18
    AtSceneLink* m_nextDirty;          // +0x1C
};

struct AtSceneLinkItem {
    int m_unknown00[2];                // +0x00
    struct AtSceneDirtyOwner* m_list;  // +0x08
    AtSceneNode* m_node;               // +0x0C
    AtSceneLinkItem* m_next;           // +0x10 among a link's, or among the set's free ones
    int m_unknown14;                   // +0x14
    AtSceneLinkItem* m_prevInList;     // +0x18
    AtSceneLinkItem* m_nextInList;     // +0x1C

    void Unlink(unsigned int kind);    // 0x000467F0
};

struct AtSceneDirtyOwner {
    int m_unknown00[5];
    AtSceneLinkItem* m_first[4];       // +0x14 one list per kind
};

// A set a node can be a member of, in four kinds. The rest of the class, at 0x00039E90-0x0003AFFF, is not recovered. Its fields are from these functions only.
class AtSceneModifierSet {
public:
    void RemoveDirty(AtSceneNode* node);     // 0x0003A2C0
    void RemoveLink(AtSceneNode* node);      // 0x00039F00
    void RemoveAsKind0(AtSceneNode* node);   // 0x0003A370
    void RemoveAsKind1(AtSceneNode* node);   // 0x0003A310
    void RemoveAsKind3(AtSceneNode* node);   // 0x0003A340

    int m_unknown00[2];                // +0x00
    AtSceneLink* m_freeLinks;          // +0x08
    int m_unknown0C[2];                // +0x0C
    AtSceneLinkItem* m_freeItems;      // +0x14
    int m_unknown18[13];               // +0x18
    unsigned int m_kind;               // +0x4C
    AtSceneLink* m_dirty[4];           // +0x50

    static int s_itemsInUse;           // 0x002C63F4
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
    virtual void ReadBody(AtSceneOwner* owner, AtIoTokenizer* tokenizer, int token);  // slot 1, 0x0004AA10 a word inside the curly brackets
    virtual void FinishBody();                     // slot 2, 0x00049F30 the sphere in world space
    virtual void OnMoved();                        // slot 3, 0x0004A040 once, until something clears kMoved: joins each kind's dirty list, and so does everything attached
    virtual AtCoordSys* CoordSys();                // slot 4, 0x000498A0
    virtual AtCoordSys* ParentCoordSys();          // slot 5, 0x000498A0 the same code; other classes differ
    virtual void FinishHeader();                   // slot 6, 0x001CC9F0 empty here, folded with others
    virtual ~AtSceneNode();                        // slot 7, 0x0003B7F0 deleting, for arrays too; 0x0004AFD0 complete
    virtual void CopyFrom(AtSceneOwner* owner, const AtSceneNode* from);  // slot 8, 0x00049A80
    virtual void InitFrom(AtSceneOwner* owner, const AtSceneNodeDesc* desc);  // slot 9, 0x00049CA0
    virtual void Read(AtSceneOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer);  // slot 10, 0x000499F0
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
    void AttachBase(AtSceneNode* parent);          // 0x0004A210
    void AttachToJoint(AtSceneNode* parent, const char* joint);  // 0x0004A310 parent is a jointed actor; nothing happens unless it has that joint

    static AtSceneNode* AsJointed(AtSceneNode* node);  // 0x000559B0 reports null, or another class, and hands it back regardless
    static void SetHeap(void* heap);               // 0x000497F0

    unsigned int m_flags;              // +0x04
    unsigned short m_class;            // +0x08 0 here, 14 in a modifier node; others in the actors
    AtSceneSphere m_sphere;            // +0x0C
    AtSceneSphere m_worldSphere;       // +0x1C
    float m_activeDistanceSquared;     // +0x2C
    AtSceneOwner* m_owner;             // +0x30
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
    void ReadBody(AtSceneOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004F3F0
    ~AtSceneModifierNode() override;               // slot 7, 0x0003CD00 deleting, for arrays too; 0x0003B910 complete
    void Read(AtSceneOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004F370 the base's
    void Clear() override;                         // slot 11, 0x0004F3C0
    void Slot12() override;                        // slot 12, 0x0004F380 not recovered

    void* m_modifier;                  // +0x6C
};

// The classes not recovered, in the order of the scene's fields. Sizes on the Xbox, for the record, are in AtScene.cpp.
enum AtSceneActorKind {
    kAtActorModel, kAtActorJointedModel, kAtActorParticleSystem, kAtActorWater, kAtActorLensFlare, kAtActorGroup, kAtActorSwitchGroup, kAtActorLodGroup, kAtActorSound, kAtActorKinds
};

unsigned int AtSceneActorsSize(int kind);                                         // the size of one
void AtSceneActorsConstruct(int kind, void* first, unsigned int count);           // given room for count
void AtSceneActorsDestroy(int kind, void* first, unsigned int count);             // last first; not the memory
AtCoordSys* AtSceneActorSharedCoordSys(const AtSceneNode* actor);                 // +0x90 in an actor of classes 1 to 13, which a copy then does without a coordinate system of its own
GmNameMap* AtSceneActorJoints(const AtSceneNode* actor);                          // +0xA0 in a jointed actor, a map of coordinate systems by joint

// Vftable 0x00275A44. A name map of nodes.
class AtScene : public GmNameMap {
public:
    enum { kKeywordCount = 116 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B1990

    AtScene();                                     // 0x0003BC70
    ~AtScene() override;                           // slot 0, 0x0002E640 deleting, for arrays too; 0x0003E8C0 complete
    void Clear() override;                         // slot 1, 0x0003D6F0

    void Load(AtSceneOwner* owner, const char* name, int unused);  // 0x0003CD60 name + ".ASN"
    void Read(AtSceneOwner* owner, AtIoTokenizer* tokenizer);      // 0x0003BCD0

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
    AtSceneOwner* m_owner;             // +0xF4 the constructor leaves it

    static void* s_heap;               // 0x002C640C

private:
    void ResetFields();                            // 0x0003BB60 all but the owner
    // A count in brackets and a list of named nodes, of a class not recovered.
    void ReadActors(AtSceneOwner* owner, AtIoTokenizer* tokenizer, int kind, int word, unsigned int* count, void** actors);
};

void AtConstructEach(void* first, unsigned int size, int count, void (*construct)(void*));  // 0x00011000 the original's takes a thiscall pointer

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtSceneOwner, m_device) == 0x04, "AtSceneOwner::m_device");
static_assert(offsetof(AtSceneOwner, m_nodes) == 0x90, "AtSceneOwner::m_nodes");
static_assert(offsetof(AtSceneOwner, m_nodeSpaces) == 0xA8, "AtSceneOwner::m_nodeSpaces");
static_assert(offsetof(AtSceneOwner, m_modifiers) == 0xCC, "AtSceneOwner::m_modifiers");
static_assert(offsetof(AtSceneOwner, m_modifierSpaces) == 0xE4, "AtSceneOwner::m_modifierSpaces");
static_assert(offsetof(AtSceneOwner, m_geometry) == 0x180, "AtSceneOwner::m_geometry");
static_assert(offsetof(AtSceneOwner, m_geometrySpaces) == 0x198, "AtSceneOwner::m_geometrySpaces");
static_assert(sizeof(AtSceneLink) == 0x20, "AtSceneLink");
static_assert(sizeof(AtSceneLinkItem) == 0x20, "AtSceneLinkItem");
static_assert(offsetof(AtSceneDirtyOwner, m_first) == 0x14, "AtSceneDirtyOwner::m_first");
static_assert(offsetof(AtSceneModifierSet, m_kind) == 0x4C, "AtSceneModifierSet::m_kind");
static_assert(offsetof(AtSceneModifierSet, m_dirty) == 0x50, "AtSceneModifierSet::m_dirty");
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
