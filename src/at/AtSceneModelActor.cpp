// ModelActor and JointedModelActor. See include/at/AtSceneActor.h.
#include "at/AtSceneActor.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

namespace {

void NotRecovered() {
    AtFatal("Atlas Scene: this model actor slot is not recovered", 0, 0, 0);
}

// 0x000283F0, a small method of the device's, in a range other work holds. So it is written out here and not recorded.
void* FindMesh(AtSceneDevice* device, const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return device->m_meshSpaces.Find(ns, name);
    }
    return device->m_meshes.Find(name);
}

const unsigned short kClassModel = 2;
const unsigned short kClassJointedModel = 3;

}  // namespace

// ---- ModelActor ----------------------------------------------------------------------------------------------------------------

AtSceneModelActor::AtSceneModelActor() {
    m_mesh = 0;
    m_unknown9C = 0;
    m_class = kClassModel;
}

AtSceneModelActor::~AtSceneModelActor() {
    AtSceneActor::Clear();
    m_mesh = 0;
    m_unknown9C = 0;
}

void AtSceneModelActor::Clear() {
    AtSceneActor::Clear();
    m_mesh = 0;
    m_unknown9C = 0;
}

void AtSceneModelActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneActor::Read(owner, scene, tokenizer);
}

void AtSceneModelActor::SetMesh(void* mesh) {
    m_mesh = mesh;
}

void AtSceneModelActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kAtWordMesh: {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_mesh = FindMesh(owner->m_device, name, ns);
        if (m_mesh == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    }
    case kAtWordInitWithHavokPosition:
    case kAtWordInitWithHavokOrientation:
        break;  // These do nothing here.
    case kAtWordPhysicsActor:
    case kAtWordPhysicsActorInstance:
    case kAtWordPhysicsEntitiesFile: {
        // A copy of the string on the current heap, which nothing frees: the original's leak, kept.
        char* copy = 0;
        tokenizer->ExpectStringCopy(&copy);
        break;
    }
    default:
        AtSceneActor::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneModelActor::Slot12() { NotRecovered(); }
void AtSceneModelActor::Slot14() { NotRecovered(); }
void AtSceneModelActor::Slot15() { NotRecovered(); }
void AtSceneModelActor::Slot34() { NotRecovered(); }
void AtSceneModelActor::Slot38() { NotRecovered(); }
void AtSceneModelActor::Slot39() { NotRecovered(); }
void AtSceneModelActor::Slot42() { NotRecovered(); }

// ---- JointedModelActor ----------------------------------------------------------------------------------------------------------

AtSceneJointedModelActor::AtSceneJointedModelActor() {
    m_class = kClassJointedModel;
    m_jointsFrom = 0;
}

AtSceneJointedModelActor::~AtSceneJointedModelActor() {
    AtSceneJointedModelActor::Clear();
}

void AtSceneJointedModelActor::Clear() {
    if ((m_joints.m_database != 0 || m_joints.m_channels != 0) && m_joints.m_database != 0 && (m_flags & kOwnsJointDatabase) != 0) {
        m_joints.m_database->Clear();
        if (m_joints.m_database != 0) {
            delete m_joints.m_database;
        }
    }
    m_joints.Clear();
    AtSceneModelActor::Clear();
    m_jointsFrom = 0;
}

void AtSceneJointedModelActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneModelActor::Read(owner, scene, tokenizer);
    m_joints.PrepareBlend(0);
}

AtCoordSysDatabase* AtSceneJointedModelActor::CopyJointDatabase(AtCoordSysDatabase* from) {
    m_flags |= kOwnsJointDatabase;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    AtCoordSysDatabase* copy = AtNewObject<AtCoordSysDatabase>();
    AtPopHeap();
    if (copy == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    copy->CopyFrom(m_owner->m_device->MatrixDevice(), from);
    if (copy->m_unknown2C != 0 && ParentCoordSys() != 0) {
        ParentCoordSys()->Attach(copy->m_unknown2C);
    }
    return copy;
}

void AtSceneJointedModelActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    char name[8];
    char ns[8];
    switch (token) {
    case kAtWordCoordSysDatabase: {
        m_joints.ReadDatabase(owner, tokenizer);
        AtCoordSysDatabase* database = m_joints.m_database;
        if (database->m_unknown2C != 0 && ParentCoordSys() != 0) {
            ParentCoordSys()->Attach(database->m_unknown2C);
        }
        break;
    }
    case kAtWordCopyCoordSysDatabase: {
        if (m_joints.m_database != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        AtCoordSysDatabase* from = (AtCoordSysDatabase*)owner->m_device->m_coordSysDatabases.Find(name);
        if (from == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        m_joints.SetDatabase(CopyJointDatabase(from));
        break;
    }
    case kAtWordShareCoordSysDatabase: {
        if (m_joints.m_database != 0 || m_parent != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneJointedModelActor* other = static_cast<AtSceneJointedModelActor*>(AsJointed(owner->FindNode(name, ns)));
        if (other == 0 || other->m_joints.m_database == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        m_coordSys = 0;
        m_scale = 0;
        other->AddChild(this);
        m_flags |= kSharesJoints;
        m_jointsFrom = other;
        m_joints.SetDatabase(other->m_joints.m_database);
        break;
    }
    case kAtWordAnimationChannel:
        m_joints.ReadChannels(owner, tokenizer, kAtWordAnimationChannel);
        break;
    case kAtWordAnimateAtDrawEnable:
        m_flags |= kAnimateAtDraw;
        break;
    case kAtWordAnimateAtDrawDisable:
        m_flags &= ~(unsigned int)kAnimateAtDraw;
        break;
    case kAtWordPhysicsEntitiesFile:
    case kAtWordAnimationSkeletonFile:
    case kAtWordRigidBodySkeletonFile:
        tokenizer->ExpectString();
        break;
    case kAtWordAttach:
    case kAtWordAttachBase:
        if ((m_flags & kSharesJoints) != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtSceneModelActor::ReadBody(owner, tokenizer, token);
        break;
    default:
        AtSceneModelActor::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneJointedModelActor::Slot12() { NotRecovered(); }
void AtSceneJointedModelActor::ShareCoordSys(AtSceneActor*) { NotRecovered(); }
void AtSceneJointedModelActor::StopSharingCoordSys() { NotRecovered(); }
void AtSceneJointedModelActor::Slot38() { NotRecovered(); }
void AtSceneJointedModelActor::Slot39() { NotRecovered(); }
void AtSceneJointedModelActor::Slot42() { NotRecovered(); }
