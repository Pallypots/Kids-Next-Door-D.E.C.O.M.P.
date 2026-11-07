// The base node and the modifier node of a scene. See include/at/AtScene.h.
#include "at/AtScene.h"

#include "at/AtSceneActor.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void* AtSceneNode::s_heap;
int AtSceneModifierSet::s_itemsInUse;

namespace {

void NotRecovered() {
    AtFatal("Atlas Scene: this node slot is not recovered", 0, 0, 0);
}

// The largest float, which is what the original uses for "no size yet".
float LargestFloat() {
    union { unsigned int bits; float value; } u;
    u.bits = 0x7F7FFFFF;
    return u.value;
}

const unsigned int kClassFirstActor = 1;
const unsigned int kClassLastActor = 13;
const unsigned short kClassJointedModel = 3;
const unsigned short kClassModifierNode = 14;

}  // namespace

// ---- modifier sets ------------------------------------------------------------------------------------------------------

void AtSceneLinkItem::Unlink(unsigned int kind) {
    if (m_prevInList != 0) {
        m_prevInList->m_nextInList = m_nextInList;
    }
    if (m_nextInList != 0) {
        m_nextInList->m_prevInList = m_prevInList;
    } else {
        m_list->m_first[kind] = m_prevInList;
    }
}

void AtSceneModifierSet::RemoveDirty(AtSceneNode* node) {
    AtSceneLink** at = &m_dirty[m_kind];
    while (*at != 0) {
        AtSceneLink* link = *at;
        if (link->m_items->m_node == node) {
            *at = link->m_nextDirty;
            node->m_flags &= ~((unsigned int)AtSceneNode::kInDirtyList0 << m_kind);
            return;
        }
        at = &link->m_nextDirty;
    }
}

void AtSceneModifierSet::RemoveLink(AtSceneNode* node) {
    AtSceneLink** at = &node->m_links[m_kind];
    AtSceneLink* link = *at;
    while (link != 0 && link->m_set != this) {
        at = &link->m_next;
        link = *at;
    }
    if (link == 0) {
        return;
    }
    *at = link->m_next;
    AtSceneLinkItem* item = link->m_items;
    while (item != 0) {
        AtSceneLinkItem* next = item->m_next;
        item->Unlink(m_kind);
        s_itemsInUse--;
        item->m_next = m_freeItems;
        m_freeItems = item;
        item = next;
    }
    link->m_set = 0;
    link->m_next = m_freeLinks;
    m_freeLinks = link;
}

void AtSceneModifierSet::RemoveAsKind0(AtSceneNode* node) {
    m_kind = 0;
    RemoveDirty(node);
    RemoveLink(node);
}

void AtSceneModifierSet::RemoveAsKind1(AtSceneNode* node) {
    m_kind = 1;
    RemoveDirty(node);
    RemoveLink(node);
}

void AtSceneModifierSet::RemoveAsKind3(AtSceneNode* node) {
    m_kind = 3;
    RemoveDirty(node);
    RemoveLink(node);
}

// ---- spheres and motion states ------------------------------------------------------------------------------------------------------

AtSceneSphere::AtSceneSphere() {
    center.x = 0;
    center.y = 0;
    center.z = 0;
    radius = 0;
}

void AtSceneSphere::Set(const AtSceneSphere& from) {
    center.x = from.center.x;
    center.y = from.center.y;
    center.z = from.center.z;
    radius = from.radius;
}

void AtSceneMotionState::Set(const AtSceneMotionState& from) {
    m_linearVelocity = from.m_linearVelocity;
    m_unknown0C[0] = from.m_unknown0C[0];
    m_unknown0C[1] = from.m_unknown0C[1];
    m_unknown0C[2] = from.m_unknown0C[2];
    m_angularVelocity = from.m_angularVelocity;
    m_unknown24[0] = from.m_unknown24[0];
    m_unknown24[1] = from.m_unknown24[1];
    m_unknown24[2] = from.m_unknown24[2];
}

AtSceneMotionState::AtSceneMotionState() {
    m_linearVelocity.x = 0;
    m_linearVelocity.y = 0;
    m_linearVelocity.z = 0;
    m_unknown0C[0] = 0;
    m_unknown0C[1] = 0;
    m_unknown0C[2] = 0;
    m_angularVelocity.x = 0;
    m_angularVelocity.y = 0;
    m_angularVelocity.z = 0;
    m_unknown24[0] = 0;
    m_unknown24[1] = 0;
    m_unknown24[2] = 0;
}

// ---- the node -----------------------------------------------------------------------------------------------------------

void AtSceneNode::SetHeap(void* heap) {
    s_heap = heap;
}

AtSceneNode::AtSceneNode() {
    m_class = 0;
    m_unknown68 = 0;
    Reset();
}

AtSceneNode::~AtSceneNode() {
    AtSceneNode::Clear();
}

void AtSceneNode::Reset() {
    m_flags = 0;
    m_sphere.center.x = 0;
    m_sphere.center.y = 0;
    m_sphere.center.z = 0;
    m_sphere.radius = LargestFloat();
    m_worldSphere.center.x = 0;
    m_worldSphere.center.y = 0;
    m_worldSphere.center.z = 0;
    m_worldSphere.radius = LargestFloat();
    m_activeDistanceSquared = LargestFloat();
    m_owner = 0;
    m_coordSys = 0;
    m_motion = 0;
    m_boundingGeometry = 0;
    m_unknown40 = 0;
    m_links[0] = 0;
    m_links[1] = 0;
    m_links[2] = 0;
    m_links[3] = 0;
    m_parent = 0;
    m_nextChild = 0;
    m_firstChild = 0;
    m_unknown60 = -1;
    m_unknown64 = -1;
}

void AtSceneNode::ReleaseCoordSys() {
    if (m_coordSys != 0 && (m_flags & kOwnsCoordSys) != 0) {
        AtMatrixDevice* device = m_owner->m_device->MatrixDevice();
        device->FreeMatrix(m_coordSys->m_local);
        device->FreeMatrix(m_coordSys->m_world);
        if (m_coordSys != 0) {
            delete m_coordSys;
        }
        m_flags &= ~(unsigned int)kOwnsCoordSys;
    }
    m_coordSys = 0;
}

void AtSceneNode::RemoveChild(AtSceneNode* child) {
    AtSceneNode** at = &m_firstChild;
    while (*at != 0) {
        if (*at == child) {
            *at = child->m_nextChild;
            child->m_nextChild = 0;
            child->m_parent = 0;
            return;
        }
        at = &(*at)->m_nextChild;
    }
}

void AtSceneNode::AddChild(AtSceneNode* child) {
    if (child->m_parent != this) {
        child->m_parent = this;
        child->m_nextChild = m_firstChild;
        m_firstChild = child;
    }
}

void AtSceneNode::Clear() {
    if (m_links[1] != 0) {
        m_links[1]->m_set->RemoveAsKind1(this);
    }
    if (m_links[3] != 0) {
        m_links[3]->m_set->RemoveAsKind3(this);
    }
    if (m_parent != 0) {
        m_parent->RemoveChild(this);
    }
    AtSceneNode* child = m_firstChild;
    while (child != 0) {
        AtSceneNode* next = child->m_nextChild;
        RemoveChild(child);
        child = next;
    }
    ReleaseCoordSys();
    if (m_motion != 0 && (m_flags & kOwnsMotion) != 0) {
        operator delete(m_motion);
    }
    Reset();
}

void AtSceneNode::ReadHeader(AtScene* scene, AtIoTokenizer* tokenizer, int token) {
    if (token == kAtWordRotTransCoordSys) {
        if (m_coordSys != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        m_coordSys = scene->NextRotTrans(tokenizer);
    } else if (token == kAtWordMotionState) {
        if (m_motion != 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        m_motion = scene->NextMotionState(tokenizer);
    } else {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
}

void AtSceneNode::FinishBody() {
    AtCoordSys* parent = ParentCoordSys();
    if (parent != 0) {
        parent->Transform(&m_sphere.center, &m_worldSphere.center);
        m_worldSphere.radius = m_sphere.radius;
    } else {
        m_worldSphere.Set(m_sphere);
    }
    m_flags &= ~(unsigned int)kMoved;
}

void AtSceneNode::OnMoved() {
    if ((m_flags & kMoved) != 0) {
        return;
    }
    if (m_links[0] != 0 && (m_flags & kInDirtyList0) == 0) {
        for (AtSceneLink* link = m_links[0]; link != 0; link = link->m_next) {
            link->m_nextDirty = link->m_set->m_dirty[0];
            link->m_set->m_dirty[0] = link;
        }
        m_flags |= kInDirtyList0;
    }
    if (m_links[1] != 0 && (m_flags & kInDirtyList1) == 0) {
        for (AtSceneLink* link = m_links[1]; link != 0; link = link->m_next) {
            link->m_nextDirty = link->m_set->m_dirty[1];
            link->m_set->m_dirty[1] = link;
        }
        m_flags |= kInDirtyList1;
    }
    if (m_links[2] != 0 && (m_flags & kInDirtyList2) == 0) {
        for (AtSceneLink* link = m_links[2]; link != 0; link = link->m_next) {
            link->m_nextDirty = link->m_set->m_dirty[2];
            link->m_set->m_dirty[2] = link;
        }
        m_flags |= kInDirtyList2;
    }
    if (m_links[3] != 0 && (m_flags & kInDirtyList3) == 0) {
        for (AtSceneLink* link = m_links[3]; link != 0; link = link->m_next) {
            link->m_nextDirty = link->m_set->m_dirty[3];
            link->m_set->m_dirty[3] = link;
        }
        m_flags |= kInDirtyList3;
    }
    m_flags |= kMoved;
    for (AtSceneNode* child = m_firstChild; child != 0; child = child->m_nextChild) {
        child->OnMoved();
    }
}

AtCoordSys* AtSceneNode::CoordSys() {
    return m_coordSys;
}

AtCoordSys* AtSceneNode::ParentCoordSys() {
    return m_coordSys;
}

void AtSceneNode::FinishHeader() {}

void AtSceneNode::AttachBase(AtSceneNode* parent) {
    AtCoordSys* mine = CoordSys();
    AtCoordSys* theirs = parent->ParentCoordSys();
    if (mine == 0 || theirs == 0) {
        return;
    }
    theirs->Attach(mine);
    if (m_parent != parent) {
        m_parent = parent;
        m_nextChild = parent->m_firstChild;
        parent->m_firstChild = this;
    }
    OnMoved();
}

void AtSceneNode::AttachToJoint(AtSceneNode* parent, const char* joint) {
    AtCoordSys* mine = CoordSys();
    if (mine == 0) {
        return;
    }
    AtCoordSys* theirs = (AtCoordSys*)static_cast<AtSceneJointedModelActor*>(parent)->m_joints.m_database->Find(joint);
    if (theirs == 0) {
        return;
    }
    parent->m_flags &= ~(unsigned int)0x00100000;
    theirs->Attach(mine);
    if (m_parent != parent) {
        m_parent = parent;
        m_nextChild = parent->m_firstChild;
        parent->m_firstChild = this;
    }
    OnMoved();
}

AtSceneNode* AtSceneNode::AsJointed(AtSceneNode* node) {
    if (node == 0) {
        AtFatal("Atlas Scene: Error trying to cast a NULL node", 0, 0, 0);
    }
    if (node->m_class != kClassJointedModel) {
        AtFatal("Atlas Scene: Error trying to cast a node to an illegal type", 0, 0, 0);
    }
    return node;
}

void AtSceneNode::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    char name[8];
    char ns[8];
    switch (token) {
    case kAtWordBoundingGeometryMatAssignment:
    case kAtWordBoundingSphereMatAssignment:
        tokenizer->ExpectInt();
        break;
    case kAtWordCollisionEnable:
        m_flags |= kCollision;
        break;
    case kAtWordCollisionDisable:
        m_flags &= ~(unsigned int)kCollision;
        break;
    case kAtWordBoundingSphere:
        m_sphere.center.Read(tokenizer);
        m_sphere.radius = tokenizer->ExpectReal();
        m_worldSphere.Set(m_sphere);
        break;
    case kAtWordActiveDistance: {
        const double distance = tokenizer->ExpectReal();
        m_activeDistanceSquared = (float)(distance * distance);
        break;
    }
    case kAtWordAttach: {
        if (CoordSys() == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* found = owner->FindNode(name, ns);
        if (found == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        AtSceneNode* jointed = AsJointed(found);
        if (jointed == 0) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
        tokenizer->ExpectName(name);
        AttachToJoint(jointed, name);
        break;
    }
    case kAtWordAttachBase: {
        if (CoordSys() == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* found = owner->FindNode(name, ns);
        if (found == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        AttachBase(found);
        break;
    }
    case kAtWordAttachNodeToThisJoint: {
        if (CoordSys() == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* found = owner->FindNode(name, ns);
        if (found == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        AtSceneNode* jointed = AsJointed(this);
        if (jointed == 0) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
        tokenizer->ExpectName(name);
        found->AttachToJoint(jointed, name);
        break;
    }
    case kAtWordAttachNodeToThisBase: {
        if (CoordSys() == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* found = owner->FindNode(name, ns);
        if (found == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        found->AttachBase(this);
        break;
    }
    case kAtWordPosition: {
        if (m_coordSys == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtVector3 position;
        position.Read(tokenizer);
        m_coordSys->SetPosition(&position);
        break;
    }
    case kAtWordOrthoNormalize: {
        if (m_coordSys == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtVector3 direction;
        direction.Read(tokenizer);
        AtVector3 up;
        up.Read(tokenizer);
        m_coordSys->SetLookAt(&direction, &up);
        break;
    }
    case kAtWordQuaternion: {
        if (m_coordSys == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtQuaternion rotation;
        rotation.Read(tokenizer);
        m_coordSys->SetRotation(&rotation);
        break;
    }
    case kAtWordLookAt: {
        if (m_coordSys == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtVector3 position;
        position.Read(tokenizer);
        m_coordSys->SetPosition(&position);
        AtVector3 target;
        target.Read(tokenizer);
        AtVector3 direction;
        direction.x = target.x - position.x;
        direction.y = target.y - position.y;
        direction.z = target.z - position.z;
        AtVector3 up;
        up.Read(tokenizer);
        m_coordSys->SetLookAt(&direction, &up);
        break;
    }
    case kAtWordLinearVelocity: {
        if (m_motion == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtVector3 velocity;
        velocity.Read(tokenizer);
        m_motion->m_linearVelocity = velocity;
        break;
    }
    case kAtWordAngularVelocity: {
        if (m_motion == 0) {
            tokenizer->Error(kAtProblemWrongUse);
        }
        AtVector3 velocity;
        velocity.Read(tokenizer);
        m_motion->m_angularVelocity = velocity;
        break;
    }
    case kAtWordBoundingGeometry:
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_boundingGeometry = owner->FindBoundingGeometry(name, ns);
        if (m_boundingGeometry == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    case kAtWordLineSegIntersectEnable:
        m_flags |= kLineSegIntersect;
        break;
    case kAtWordLineSegIntersectDisable:
        m_flags &= ~(unsigned int)kLineSegIntersect;
        break;
    case kAtWordEnableBoundingGeometryDraw:
    case kAtWordEnableNodeBoundingSphereDraw:
    case kAtWordDisableBoundingGeometryDraw:
    case kAtWordDisableNodeBoundingSphereDraw:
        break;  // Debug words: the original takes them and does nothing.
    default:
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        break;
    }
}

void AtSceneNode::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    if ((m_flags & kLoaded) != 0) {
        Clear();
    }
    m_owner = owner;
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kLeftCurly) {
        ReadHeader(scene, tokenizer, token);
        token = tokenizer->Next();
    }
    FinishHeader();
    token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        ReadBody(owner, tokenizer, token);
        token = tokenizer->Next();
    }
    FinishBody();
    m_flags |= kLoaded;
}

void AtSceneNode::CopyFrom(AtNameTableOwner* owner, const AtSceneNode* from) {
    if ((m_flags & kLoaded) != 0) {
        Clear();
    }
    m_flags = (from->m_flags & (kCollision | kLineSegIntersect)) | kLoaded;
    m_sphere.Set(from->m_sphere);
    m_worldSphere.Set(from->m_worldSphere);
    OnMoved();
    m_activeDistanceSquared = from->m_activeDistanceSquared;
    m_owner = owner;
    AtMatrixDevice* device = owner->m_device->MatrixDevice();
    m_boundingGeometry = from->m_boundingGeometry;
    if (from->m_coordSys != 0) {
        const bool isActor = from->m_class >= kClassFirstActor && from->m_class <= kClassLastActor;
        if (!isActor || static_cast<const AtSceneActor*>(from)->m_sharedFrom == 0) {
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_coordSys = AtNewObject<AtRotTransCoordSys>();
            AtPopHeap();
            if (m_coordSys == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            AtMatrix* world = device->NewMatrix();
            AtMatrix* local = device->NewMatrix();
            m_coordSys->SetMatrices(local, world);
            m_coordSys->CopyLocal(from->m_coordSys);
            m_flags |= kOwnsCoordSys;
        }
    }
    if (from->m_motion != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_motion = AtNewObject<AtSceneMotionState>();
        AtPopHeap();
        if (m_motion == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_motion->Set(*from->m_motion);
        m_flags |= kOwnsMotion;
    }
    m_unknown40 = from->m_unknown40;
}

void AtSceneNode::InitFrom(AtNameTableOwner* owner, const AtSceneNodeDesc* desc) {
    if ((m_flags & kLoaded) != 0) {
        Clear();
    }
    m_flags = kLoaded;
    if (desc->m_collision != 0) {
        m_flags = kLoaded | kCollision;
    }
    m_sphere.Set(desc->m_sphere);
    m_worldSphere.Set(desc->m_sphere);
    m_activeDistanceSquared = desc->m_activeDistance;
    m_owner = owner;
    if (desc->m_coordSys != 0) {
        m_coordSys = desc->m_coordSys;
    }
    if (desc->m_motion != 0) {
        m_motion = desc->m_motion;
    }
    OnMoved();
}

void AtSceneNode::Slot12() { NotRecovered(); }
void AtSceneNode::Slot13() { NotRecovered(); }
void AtSceneNode::Slot14() { NotRecovered(); }
void AtSceneNode::Slot15() { NotRecovered(); }
void AtSceneNode::Slot16() { NotRecovered(); }
void AtSceneNode::Slot17() { NotRecovered(); }
void AtSceneNode::Slot18() { NotRecovered(); }
void AtSceneNode::Slot19() { NotRecovered(); }
void AtSceneNode::Slot20() { NotRecovered(); }
void AtSceneNode::Slot21() { NotRecovered(); }
void AtSceneNode::Slot22() { NotRecovered(); }
void AtSceneNode::Slot23() { NotRecovered(); }
void AtSceneNode::Slot24() { NotRecovered(); }
void AtSceneNode::Slot25() { NotRecovered(); }
void AtSceneNode::Slot26() { NotRecovered(); }
void AtSceneNode::Slot27() { NotRecovered(); }
void AtSceneNode::Slot28() { NotRecovered(); }
void AtSceneNode::Slot29() { NotRecovered(); }
void AtSceneNode::Slot30() { NotRecovered(); }
void AtSceneNode::Slot31() { NotRecovered(); }

// ---- the modifier node --------------------------------------------------------------------------------------------------

AtSceneModifierNode::AtSceneModifierNode() {
    m_class = kClassModifierNode;
    m_modifier = 0;
}

AtSceneModifierNode::~AtSceneModifierNode() {}

void AtSceneModifierNode::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneNode::Read(owner, scene, tokenizer);
}

void AtSceneModifierNode::Clear() {
    if (m_links[0] != 0) {
        m_links[0]->m_set->RemoveAsKind0(this);
    }
    AtSceneNode::Clear();
    m_modifier = 0;
}

void AtSceneModifierNode::Slot12() { NotRecovered(); }

void AtSceneModifierNode::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kAtWordModifier: {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        if (ns[0] != 0) {
            m_modifier = owner->m_modifierSpaces.Find(ns, name);
        } else {
            m_modifier = owner->m_modifiers.Find(name);
        }
        if (m_modifier == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    }
    case kAtWordModificationEnable:
        m_flags |= kModificationEnabled;
        break;
    case kAtWordModificationDisable:
        m_flags &= ~(unsigned int)kModificationEnabled;
        break;
    default:
        AtSceneNode::ReadBody(owner, tokenizer, token);
        break;
    }
}

// ---- what the owner finds -----------------------------------------------------------------------------------------------

AtSceneNode* AtNameTableOwner::FindNode(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return (AtSceneNode*)m_nodeSpaces.Find(ns, name);
    }
    return (AtSceneNode*)m_nodes.Find(name);
}

void* AtNameTableOwner::FindBoundingGeometry(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return m_geometrySpaces.Find(ns, name);
    }
    return m_geometry.Find(name);
}

void AtNameTableOwner::AddToThird(GmNameMap* scene, const char* ns) {
    if (ns != 0) {
        m_nodeSpaces.Add(ns, scene);
    } else {
        m_nodes.Add(scene);
    }
}
