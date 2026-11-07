// See include/at/AtScene.h.
#include "at/AtScene.h"

#include "at/AtText.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtScene::s_heap;

const AtIoKeyword AtScene::s_keywords[AtScene::kKeywordCount] = {
    {57, "Node"}, {58, "RotTransCoordSys"}, {59, "ScaleCoordSys"}, {60, "MotionState"}, {61, "ModifierNode"}, {62, "Modifier"}, {63, "BspBoundModifierNode"}, {64, "ModelActor"},
    {65, "JointedModelActor"}, {66, "CoordSysDatabase"}, {67, "CopyCoordSysDatabase"}, {68, "AnimationChannel"}, {69, "PlayAnimation"}, {70, "Weights"}, {71, "WeightMultiplier"}, {72, "PlaybackRate"},
    {73, "BoxActor"}, {74, "SphereActor"}, {75, "ConeActor"}, {76, "Radius"}, {77, "Height"}, {78, "CylinderActor"}, {79, "Width"}, {80, "MatAssignmentOffset"},
    {81, "ParticleSystemActor"}, {82, "Emitter"}, {83, "StartTime"}, {84, "GroupActor"}, {85, "Actor"}, {86, "SwitchGroupActor"}, {87, "Switch"}, {88, "LodGroupActor"},
    {89, "Lod"}, {90, "BoundingSphere"}, {91, "ActiveDistance"}, {92, "MatAssignment"}, {93, "Attach"}, {94, "AttachBase"}, {95, "Position"}, {96, "OrthoNormalize"},
    {97, "Quaternion"}, {98, "LookAt"}, {99, "LinearVelocity"}, {100, "AngularVelocity"}, {101, "CollisionEnable"}, {102, "CollisionDisable"}, {103, "ModificationEnable"}, {104, "ModificationDisable"},
    {105, "DrawingEnable"}, {106, "DrawingDisable"}, {107, "LightBlocker"}, {108, "ShadowReceiver"}, {109, "MatAnimationChannel"}, {110, "PlayMatAnimation"}, {111, "Scale"}, {112, "DataLink"},
    {113, "Mesh"}, {114, "NameSpace"}, {115, "ModifierReceiver"}, {116, "BoundingGeometry"}, {117, "LineSegIntersectEnable"}, {118, "LineSegIntersectDisable"}, {119, "AnimateAtDrawEnable"}, {120, "AnimateAtDrawDisable"},
    {121, "ShareCoordSysDatabase"}, {122, "Morph"}, {123, "PingPong"}, {124, "CoordSysMirrorDatabase"}, {125, "CoordSysMirroringEnable"}, {126, "CoordSysMirroringDisable"}, {127, "ProjectionReceiver"}, {128, "MorphAnimationChannel"},
    {129, "PlayMorphAnimation"}, {130, "TextureCoordSysDatabase"}, {131, "CopyTextureCoordSysDatabase"}, {132, "ShareTextureCoordSysDatabase"}, {133, "TextureCoordAnimationChannel"}, {134, "ScaleTranslations"}, {135, "LensFlareActor"}, {136, "LensFlare"},
    {139, "EnableBoundingGeometryDraw"}, {141, "DisableBoundingGeometryDraw"}, {140, "EnableNodeBoundingSphereDraw"}, {142, "DisableNodeBoundingSphereDraw"}, {143, "BoundingGeometryMatAssignment"}, {144, "BoundingSphereMatAssignment"}, {137, "LineOfSightTestEnable"}, {138, "LineOfSightTestDisable"},
    {145, "WorldAdditiveInitialVelocity"}, {146, "WorldAdditiveAcceleration"}, {147, "WaterActor"}, {148, "SoundNode"}, {149, "AttachNodeToThisJoint"}, {150, "AttachNodeToThisBase"}, {151, "UpdateAtDraw"}, {152, "PlaySegment"},
    {153, "PlayParticles"}, {154, "PauseParticles"}, {155, "MorphEnable"}, {156, "WaterPatch"}, {157, "PositionRect"}, {158, "TxtCoordRect"}, {159, "Neighbors"}, {160, "Mask"},
    {161, "DampingFactor"}, {162, "GenerationFactor"}, {163, "PhysicsActor"}, {164, "PhysicsActorInstance"}, {165, "PhysicsModelActor"}, {166, "PhysicsJointedModelActor"}, {167, "PhysicsVehicleActor"}, {168, "PhysicsEntitiesFile"},
    {169, "AnimationSkeletonFile"}, {170, "RigidBodySkeletonFile"}, {171, "InitWithHavokPosition"}, {172, "InitWithHavokOrientation"},
};

namespace {

void ConstructMotionState(void* where) {
    new (where, AtPlacement()) AtSceneMotionState;
}

// The array of a class not recovered: the same shape as AtNewArray, with the size and the constructor from the seam.
void* NewActors(int kind, unsigned int count) {
    AtSize* block = (AtSize*)operator new(count * AtSceneActorsSize(kind) + sizeof(AtSize));
    if (block == 0) {
        return 0;
    }
    *block = count;
    AtSceneActorsConstruct(kind, block + 1, count);
    return block + 1;
}

void DeleteActors(int kind, void* first) {
    AtSize* block = (AtSize*)first - 1;
    if (*block != 0) {
        AtSceneActorsDestroy(kind, first, (unsigned int)*block);
    }
    operator delete(block);
}

void CheckHeap() {
    if (AtScene::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
}

// The count of a word such as RotTransCoordSys: "[ n ]" with nothing after it.
unsigned int ReadBracketedCount(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftBracket();
    const unsigned int count = (unsigned int)tokenizer->ExpectInt();
    tokenizer->ExpectRightBracket();
    return count;
}

}  // namespace

void AtConstructEach(void* first, unsigned int size, int count, void (*construct)(void*)) {
    char* at = (char*)first;
    for (int i = count; i > 0; i--) {
        construct(at);
        at += size;
    }
}

void AtScene::SetHeap(void* heap) {
    s_heap = heap;
}

AtScene::AtScene() {
    ResetFields();
}

AtScene::~AtScene() {
    AtScene::Clear();
}

void AtScene::ResetFields() {
    m_motionStates = 0;
    m_rotTransCount = 0;
    m_scaleCount = 0;
    m_motionCount = 0;
    m_rotTransUsed = 0;
    m_scaleUsed = 0;
    m_motionUsed = 0;
    m_nodeCount = 0;
    m_modifierNodeCount = 0;
    m_unknown70 = 0;
    m_modelCount = 0;
    m_jointedModelCount = 0;
    m_unknown7C[0] = 0;
    m_unknown7C[1] = 0;
    m_unknown7C[2] = 0;
    m_unknown7C[3] = 0;
    m_particleSystemCount = 0;
    m_waterCount = 0;
    m_lensFlareCount = 0;
    m_groupCount = 0;
    m_switchGroupCount = 0;
    m_lodGroupCount = 0;
    m_soundCount = 0;
    m_unknownA8[0] = 0;
    m_unknownA8[1] = 0;
    m_unknownA8[2] = 0;
    m_nodes = 0;
    m_modifierNodes = 0;
    m_unknownBC = 0;
    m_models = 0;
    m_jointedModels = 0;
    m_unknownC8[0] = 0;
    m_unknownC8[1] = 0;
    m_unknownC8[2] = 0;
    m_unknownC8[3] = 0;
    m_particleSystems = 0;
    m_water = 0;
    m_lensFlares = 0;
    m_groups = 0;
    m_switchGroups = 0;
    m_lodGroups = 0;
    m_sounds = 0;
}

void AtScene::Clear() {
    if (m_sounds != 0) {
        DeleteActors(kAtActorSound, m_sounds);
    }
    if (m_lodGroups != 0) {
        DeleteActors(kAtActorLodGroup, m_lodGroups);
    }
    if (m_switchGroups != 0) {
        DeleteActors(kAtActorSwitchGroup, m_switchGroups);
    }
    if (m_groups != 0) {
        DeleteActors(kAtActorGroup, m_groups);
    }
    if (m_particleSystems != 0) {
        DeleteActors(kAtActorParticleSystem, m_particleSystems);
    }
    if (m_water != 0) {
        DeleteActors(kAtActorWater, m_water);
    }
    if (m_lensFlares != 0) {
        DeleteActors(kAtActorLensFlare, m_lensFlares);
    }
    if (m_jointedModels != 0) {
        DeleteActors(kAtActorJointedModel, m_jointedModels);
    }
    if (m_models != 0) {
        DeleteActors(kAtActorModel, m_models);
    }
    if (m_nodes != 0) {
        AtDeleteArray(m_nodes);
    }
    if (m_modifierNodes != 0) {
        AtDeleteArray(m_modifierNodes);
    }
    if (m_motionStates != 0) {
        operator delete(m_motionStates);
    }
    m_coordSys.Clear();
    GmNameMap::Clear();
    ResetFields();
}

AtRotTransCoordSys* AtScene::NextRotTrans(AtIoTokenizer* tokenizer) {
    if (m_rotTransUsed >= m_rotTransCount) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    return &m_coordSys.m_rotTrans[m_rotTransUsed++];
}

AtScaleCoordSys* AtScene::NextScale(AtIoTokenizer* tokenizer) {
    if (m_scaleUsed >= m_scaleCount) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    return &m_coordSys.m_scaled[m_scaleUsed++];
}

AtSceneMotionState* AtScene::NextMotionState(AtIoTokenizer* tokenizer) {
    if (m_motionUsed >= m_motionCount) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    return &m_motionStates[m_motionUsed++];
}

void AtScene::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    m_owner = owner;
    const int status = file.Open(name, "ASN", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASN");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

void AtScene::ReadActors(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int kind, int word, unsigned int* count, void** actors) {
    if (*count != 0) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    *count = (unsigned int)tokenizer->ExpectCount();
    CheckHeap();
    AtPushHeap(s_heap);
    *actors = NewActors(kind, *count);
    AtPopHeap();
    if (*actors == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    const unsigned int size = AtSceneActorsSize(kind);
    for (unsigned int i = 0; i < *count; i++) {
        tokenizer->ExpectToken(word);
        char name[8];
        tokenizer->ExpectName(name);
        AtSceneNode* actor = (AtSceneNode*)((char*)*actors + i * size);
        Insert(name, actor);
        actor->Read(owner, this, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

void AtScene::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_entries != 0) {
        Clear();
    }
    m_owner = owner;
    AtSceneDevice* device = owner->m_device;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    char space[8];
    const char* ns = 0;
    if (token == kAtWordNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }
    while (token != kAtWordNode) {
        switch (token) {
        case kAtWordRotTransCoordSys:
            m_rotTransCount = ReadBracketedCount(tokenizer);
            break;
        case kAtWordScaleCoordSys:
            m_scaleCount = ReadBracketedCount(tokenizer);
            break;
        case kAtWordMotionState:
            m_motionCount = ReadBracketedCount(tokenizer);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    if (m_rotTransCount != 0 || m_scaleCount != 0) {
        m_coordSys.Allocate(device->MatrixDevice(), m_rotTransCount, m_scaleCount, 0);
    }
    if (m_motionCount != 0) {
        CheckHeap();
        AtPushHeap(s_heap);
        const unsigned int count = m_motionCount;
        void* block = operator new(count * sizeof(AtSceneMotionState));
        if (block != 0) {
            AtConstructEach(block, sizeof(AtSceneMotionState), (int)count, ConstructMotionState);
        }
        m_motionStates = (AtSceneMotionState*)block;
        AtPopHeap();
        if (m_motionStates == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    // The word Node, seen above, with the number of names in the map after it. It says nothing about the array of base nodes, whose own word is the same and comes later.
    const unsigned int names = ReadBracketedCount(tokenizer);
    if (names < 1) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    GmNameMap::Init(names);
    owner->AddToThird(this, ns);
    token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd) {
        switch (token) {
        case kAtWordNode:
            if (m_nodeCount != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_nodeCount = (unsigned int)tokenizer->ExpectCount();
            CheckHeap();
            AtPushHeap(s_heap);
            m_nodes = AtNewArray<AtSceneNode>(m_nodeCount);
            AtPopHeap();
            if (m_nodes == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int i = 0; i < m_nodeCount; i++) {
                tokenizer->ExpectToken(kAtWordNode);
                char name[8];
                tokenizer->ExpectName(name);
                Insert(name, &m_nodes[i]);
                m_nodes[i].Read(owner, this, tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordModifierNode:
            if (m_modifierNodeCount != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_modifierNodeCount = (unsigned int)tokenizer->ExpectCount();
            CheckHeap();
            AtPushHeap(s_heap);
            m_modifierNodes = AtNewArray<AtSceneModifierNode>(m_modifierNodeCount);
            AtPopHeap();
            if (m_modifierNodes == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int i = 0; i < m_modifierNodeCount; i++) {
                tokenizer->ExpectToken(kAtWordModifierNode);
                char name[8];
                tokenizer->ExpectName(name);
                Insert(name, &m_modifierNodes[i]);
                m_modifierNodes[i].Read(owner, this, tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kAtWordModelActor:
            ReadActors(owner, tokenizer, kAtActorModel, token, &m_modelCount, &m_models);
            break;
        case kAtWordJointedModelActor:
        case kAtWordPhysicsJointedModelActor:
            ReadActors(owner, tokenizer, kAtActorJointedModel, token, &m_jointedModelCount, &m_jointedModels);
            break;
        case kAtWordParticleSystemActor:
            ReadActors(owner, tokenizer, kAtActorParticleSystem, token, &m_particleSystemCount, &m_particleSystems);
            break;
        case kAtWordWaterActor:
            ReadActors(owner, tokenizer, kAtActorWater, token, &m_waterCount, &m_water);
            break;
        case kAtWordLensFlareActor:
            ReadActors(owner, tokenizer, kAtActorLensFlare, token, &m_lensFlareCount, &m_lensFlares);
            break;
        case kAtWordGroupActor:
            ReadActors(owner, tokenizer, kAtActorGroup, token, &m_groupCount, &m_groups);
            break;
        case kAtWordSwitchGroupActor:
            ReadActors(owner, tokenizer, kAtActorSwitchGroup, token, &m_switchGroupCount, &m_switchGroups);
            break;
        case kAtWordLodGroupActor:
            ReadActors(owner, tokenizer, kAtActorLodGroup, token, &m_lodGroupCount, &m_lodGroups);
            break;
        case kAtWordSoundNode:
            ReadActors(owner, tokenizer, kAtActorSound, token, &m_soundCount, &m_sounds);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
}
