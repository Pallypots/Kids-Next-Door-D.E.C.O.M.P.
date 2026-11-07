// The actor base, and the arrays of the classes that are recovered. See include/at/AtSceneActor.h.
#include "at/AtSceneActor.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void* AtSceneActor::s_heap;

namespace {

void NotRecovered() {
    AtFatal("Atlas Scene: this actor slot is not recovered", 0, 0, 0);
}

void ConstructMatChannel(void* where) {
    new (where, AtPlacement()) AtSceneMatChannel;
}

void CopyString(char* to, const char* from, unsigned int count) {
    unsigned int i = 0;
    for (; i < count && from[i] != 0; i++) {
        to[i] = from[i];
    }
    for (; i < count; i++) {
        to[i] = 0;
    }
}

// These three are small methods of the owner's and the device's: 0x000464E0, 0x00028430 and 0x0003A9C0. They are in ranges other work holds, so they are written out here and not recorded.
void* FindModifier(AtNameTableOwner* owner, const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return owner->m_modifierSpaces.Find(ns, name);
    }
    return owner->m_modifiers.Find(name);
}

void* FindMatAssignment(AtSceneDevice* device, const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return device->m_matAssignmentSpaces.Find(ns, name);
    }
    return device->m_matAssignments.Find(name);
}

void RemoveAsKind2(AtSceneModifierSet* set, AtSceneNode* node) {
    set->m_kind = 2;
    set->RemoveDirty(node);
    set->RemoveLink(node);
}

const unsigned short kClassActor = 1;
const unsigned short kClassLastActor = 13;

}  // namespace

// ---- the arrays a scene makes --------------------------------------------------------------------------------------------------

unsigned int AtSceneActorsSize(int kind) {
    switch (kind) {
    case kAtActorModel:
        return sizeof(AtSceneModelActor);
    case kAtActorJointedModel:
        return sizeof(AtSceneJointedModelActor);
    default:
        return AtSceneUnrecoveredActorsSize(kind);
    }
}

void AtSceneActorsConstruct(int kind, void* first, unsigned int count) {
    switch (kind) {
    case kAtActorModel:
        for (unsigned int i = 0; i < count; i++) {
            new ((AtSceneModelActor*)first + i, AtPlacement()) AtSceneModelActor;
        }
        break;
    case kAtActorJointedModel:
        for (unsigned int i = 0; i < count; i++) {
            new ((AtSceneJointedModelActor*)first + i, AtPlacement()) AtSceneJointedModelActor;
        }
        break;
    default:
        AtSceneUnrecoveredActorsConstruct(kind, first, count);
        break;
    }
}

void AtSceneActorsDestroy(int kind, void* first, unsigned int count) {
    switch (kind) {
    case kAtActorModel:
        for (unsigned int i = count; i > 0; i--) {
            ((AtSceneModelActor*)first)[i - 1].~AtSceneModelActor();
        }
        break;
    case kAtActorJointedModel:
        for (unsigned int i = count; i > 0; i--) {
            ((AtSceneJointedModelActor*)first)[i - 1].~AtSceneJointedModelActor();
        }
        break;
    default:
        AtSceneUnrecoveredActorsDestroy(kind, first, count);
        break;
    }
}

// ---- the actor -----------------------------------------------------------------------------------------------------------------

void AtSceneActor::SetHeap(void* heap) {
    s_heap = heap;
}

AtSceneActor* AtSceneActor::AsActor(AtSceneNode* node) {
    if (node == 0) {
        AtFatal("Atlas Scene: Error trying to cast a NULL node", 0, 0, 0);
    }
    if (node->m_class < kClassActor || node->m_class > kClassLastActor) {
        AtFatal("Atlas Scene: Error trying to cast a node to an illegal type", 0, 0, 0);
    }
    return static_cast<AtSceneActor*>(node);
}

AtSceneActor::AtSceneActor() {
    m_unknown88 = 0;
    m_unknown6C = 0;
    m_matAssignment = 0;
    m_matChannelCount = 0;
    m_matChannels = 0;
    m_scale = 0;
    m_modifier = 0;
    m_sharedFrom = 0;
    m_animations = 0;
    m_dataLink = 0;
    m_class = kClassActor;
}

AtSceneActor::~AtSceneActor() {
    AtSceneActor::Clear();
}

void AtSceneActor::ReadHeader(AtScene* scene, AtIoTokenizer* tokenizer, int token) {
    if (token == kAtWordScaleCoordSys) {
        if (m_scale != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        m_scale = scene->NextScale(tokenizer);
    } else {
        AtSceneNode::ReadHeader(scene, tokenizer, token);
    }
}

AtCoordSys* AtSceneActor::CoordSys() {
    return m_coordSys != 0 ? m_coordSys : m_scale;
}

AtCoordSys* AtSceneActor::ParentCoordSys() {
    if (m_sharedFrom != 0) {
        return m_sharedFrom->ParentCoordSys();
    }
    return m_scale != 0 ? m_scale : m_coordSys;
}

void AtSceneActor::FinishHeader() {
    if (m_scale != 0 && m_coordSys != 0) {
        m_coordSys->Attach(m_scale);
    }
}

void AtSceneActor::FinishBody() {
    if (m_scale != 0) {
        m_scale->Transform(&m_sphere.center, &m_worldSphere.center);
        const AtVector3& scale = m_scale->m_scale;
        // A NaN compares as not greater, which is what the original's status word test gives too.
        float largest;
        if (scale.x > scale.y) {
            largest = scale.x > scale.z ? scale.x : scale.z;
        } else {
            largest = scale.y > scale.z ? scale.y : scale.z;
        }
        m_flags &= ~(unsigned int)kMoved;
        m_worldSphere.radius = m_sphere.radius * largest;
    } else if (m_coordSys != 0) {
        m_coordSys->Transform(&m_sphere.center, &m_worldSphere.center);
        m_worldSphere.radius = m_sphere.radius;
        m_flags &= ~(unsigned int)kMoved;
    } else {
        m_worldSphere.center = m_sphere.center;
        m_worldSphere.radius = m_sphere.radius;
        m_flags &= ~(unsigned int)kMoved;
    }
}

void AtSceneActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneNode::Read(owner, scene, tokenizer);
    if (m_animations != 0) {
        m_animations->PrepareBlend(0);
    }
}

void AtSceneActor::Clear() {
    AtSceneAnimSet* animations = m_animations;
    if (animations != 0) {
        if ((animations->m_database != 0 || animations->m_channels != 0) && animations->m_database != 0 && (m_flags & kOwnsAnimDatabase) != 0) {
            animations->m_database->Clear();
            if (m_animations->m_database != 0) {
                delete m_animations->m_database;
            }
        }
        m_animations->Clear();
        if (m_animations != 0) {
            m_animations->~AtSceneAnimSet();
            operator delete(m_animations);
        }
    }
    if (m_links[2] != 0) {
        RemoveAsKind2(m_links[2]->m_set, this);
    }
    if (m_scale != 0 && (m_flags & kOwnsScaleCoordSys) != 0) {
        AtMatrixDevice* device = m_owner->m_device->MatrixDevice();
        device->FreeMatrix(m_scale->m_local);
        device->FreeMatrix(m_scale->m_world);
        if (m_scale != 0) {
            delete m_scale;
        }
    }
    if (m_matChannels != 0) {
        for (unsigned int i = 0; i < m_matChannelCount; i++) {
            m_matChannels[i].~AtSceneMatChannel();
        }
        operator delete(m_matChannels);
    }
    AtSceneNode::Clear();
    m_unknown6C = 0;
    m_matAssignment = 0;
    m_matChannelCount = 0;
    m_matChannels = 0;
    m_scale = 0;
    m_modifier = 0;
    m_sharedFrom = 0;
    m_animations = 0;
    m_dataLink = 0;
}

AtSceneAnimSet* AtSceneActor::EnsureAnimations() {
    if (m_animations == 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_animations = AtNewObject<AtSceneAnimSet>();
        AtPopHeap();
        if (m_animations == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    return m_animations;
}

AtCoordSysDatabase* AtSceneActor::CopyAnimDatabase(AtCoordSysDatabase* from) {
    m_flags |= kOwnsAnimDatabase;
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
    return copy;
}

void AtSceneActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    char name[8];
    char ns[8];
    switch (token) {
    case kAtWordDrawingEnable:
        m_flags |= kDrawing;
        break;
    case kAtWordDrawingDisable:
        m_flags &= ~(unsigned int)kDrawing;
        break;
    case kAtWordMatAssignment:
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_matAssignment = FindMatAssignment(owner->m_device, name, ns);
        if (m_matAssignment == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    case kAtWordScale: {
        if (m_scale == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtVector3 scale;
        scale.Read(tokenizer);
        m_scale->SetScale(&scale);
        break;
    }
    case kAtWordModifierReceiver:
        m_flags |= kModifierReceiver;
        break;
    case kAtWordModifier:
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_modifier = FindModifier(owner, name, ns);
        if (m_modifier == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    case kAtWordProjectionReceiver:
        m_flags |= kProjectionReceiver;
        break;
    case kAtWordMatAnimationChannel: {
        if (m_matChannelCount > 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        m_matChannelCount = (unsigned int)tokenizer->ExpectCount();
        if (m_matChannelCount < 1) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(m_matChannelCount * sizeof(AtSceneMatChannel));
        if (block != 0) {
            AtConstructEach(block, sizeof(AtSceneMatChannel), (int)m_matChannelCount, ConstructMatChannel);
        }
        m_matChannels = (AtSceneMatChannel*)block;
        AtPopHeap();
        if (m_matChannels == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < m_matChannelCount; i++) {
            m_matChannels[i].Read(owner->m_device, tokenizer);
        }
        tokenizer->ExpectRightCurly();
        break;
    }
    case kAtWordTextureCoordSysDatabase:
        EnsureAnimations();
        m_animations->ReadDatabase(owner, tokenizer);
        break;
    case kAtWordCopyTextureCoordSysDatabase: {
        EnsureAnimations();
        if (m_animations->m_database != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        AtCoordSysDatabase* from = (AtCoordSysDatabase*)owner->m_device->m_coordSysDatabases.Find(name);
        if (from == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        m_animations->SetDatabase(CopyAnimDatabase(from));
        break;
    }
    case kAtWordShareTextureCoordSysDatabase: {
        EnsureAnimations();
        if (m_animations->m_database != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneActor* other = AsActor(owner->FindNode(name, ns));
        if (other == 0 || other->m_animations == 0 || other->m_animations->m_database == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        AtSceneAnimSet* theirs = other->m_animations;
        m_animations->SetDatabase(theirs != 0 ? theirs->m_database : 0);
        break;
    }
    case kAtWordTextureCoordAnimationChannel:
        EnsureAnimations();
        m_animations->ReadChannels(owner, tokenizer, kAtWordTextureCoordAnimationChannel);
        break;
    case kAtWordDataLink: {
        char path[0x100];
        CopyString(path, tokenizer->ExpectString(), sizeof(path));
        tokenizer->ExpectName(ns);
        m_dataLink = AtSceneFindDataLink(owner, path, ns);
        break;
    }
    default:
        AtSceneNode::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneActor::ShareCoordSys(AtSceneActor* with) {
    ReleaseCoordSys();
    m_coordSys = with->m_coordSys;
    m_sharedFrom = with;
}

void AtSceneActor::StopSharingCoordSys() {
    m_sharedFrom = 0;
    m_coordSys = 0;
}

void AtSceneActor::Slot12() { NotRecovered(); }
void AtSceneActor::Slot16() { NotRecovered(); }
void AtSceneActor::Slot17() { NotRecovered(); }
void AtSceneActor::Slot18() { NotRecovered(); }
void AtSceneActor::Slot35() { NotRecovered(); }
void AtSceneActor::Slot36() { NotRecovered(); }
void AtSceneActor::Slot37() { NotRecovered(); }
void AtSceneActor::Slot40() { NotRecovered(); }
void AtSceneActor::Slot41() { NotRecovered(); }
void AtSceneActor::Slot42() { NotRecovered(); }
