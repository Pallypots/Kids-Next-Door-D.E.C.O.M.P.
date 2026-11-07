// See include/at/AtBoundingGeometry.h. The reader, the classes of geometry and what they do with each other. Original address clusters 0x00040A60-0x000410DF, 0x000539F0-0x000551BF, 0x00055340-0x000555FF.
#include "AtBoundingMath.h"

#include "AtSceneArray.h"
#include "at/AtSceneActor.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

using namespace AtBoundingMath;

AtSceneNode* AtBoundingGeometry::s_otherNode;
AtSceneNode* AtBoundingGeometry::s_node;
AtCoordSys* AtBoundingGeometry::s_otherCoordSys;
AtSegmentQuery* AtBoundingGeometry::s_query;
AtBoundingContactList* AtBoundingGeometry::s_contacts;
float AtBoundingGeometry::s_segmentHalfLength;
AtCoordSys* AtBoundingGeometry::s_coordSys;
AtVector3 AtBoundingGeometry::s_segmentDirection;
AtVector3 AtBoundingGeometry::s_segmentStart;
AtVector3 AtBoundingGeometry::s_relativePosition;
AtVector3 AtBoundingGeometry::s_relativeAxes[3];
AtVector3 AtBoundingGeometry::s_segmentMiddle;
AtVector3 AtBoundingGeometry::s_segmentEnd;
void* AtBoundingGeometryDatabase::s_heap;
void* AtSegmentHitList::s_heap;

// 0x002B21A0.
const AtIoKeyword AtBoundingGeometryDatabase::s_keywords[AtBoundingGeometryDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "BoxBoundingGeometry"}, {0x3B, "SphereBoundingGeometry"}, {0x3C, "ConeBoundingGeometry"}, {0x3D, "CylinderBoundingGeometry"},
    {0x3E, "BoundingGeometry"}, {0x3F, "Box"}, {0x40, "Sphere"}, {0x41, "Cone"}, {0x42, "Cylinder"}, {0x43, "RotTransCoordSys"}, {0x44, "Parent"}, {0x45, "BoxMinMax"}, {0x46, "GroupBoundingGeometry"},
};

namespace {

enum Word {
    kEndOfFile = 0, kRightCurly = 6, kNameSpace = 0x39, kBoxWord = 0x3A, kSphereWord = 0x3B, kBoundingGeometry = 0x3E, kBoxShape = 0x3F, kSphereShape = 0x40, kRotTransCoordSys = 0x43, kParent = 0x44, kBoxMinMax = 0x45, kGroupWord = 0x46
};

// Codes passed to the tokenizer's Error; the meanings are inferred from where they are used.
enum Problem { kUnexpectedWord = 0x1E };

void NoGetPosition(const AtMatrix*, AtVector3*) { AtFatal("Atlas: the matrix table's position is not set", 0, 0, 0); }
void NoGetAxes(const AtMatrix*, AtVector3*, AtVector3*, AtVector3*) { AtFatal("Atlas: the matrix table's axes are not set", 0, 0, 0); }
void NoTransformDirection(const AtMatrix*, const AtVector3*, AtVector3*) { AtFatal("Atlas: the matrix table's direction transform is not set", 0, 0, 0); }

// The joint a geometry follows, without the checks: at +0xA0 in the node, which only a jointed model has.
AtCoordSys* JointOf(AtSceneNode* node, int index) {
    return &((AtSceneJointedModelActor*)node)->m_joints.m_database->m_rotTrans[index - 1];
}

// With them. A report does not return in the game.
AtCoordSys* CheckedJointOf(AtSceneNode* node, int index) {
    if (node->m_class != 3) {
        AtFatal("Atlas Scene: Node type must be jointed for bounding geometry", 0, 0, 0);
    }
    if (((AtSceneJointedModelActor*)node)->m_joints.m_database == 0) {
        AtFatal("Atlas Scene: Node type must have csys DB for bounding geometry", 0, 0, 0);
    }
    return JointOf(node, index);
}

void ReadMatrixAxes(const AtMatrix* m, AtVector3* position, AtVector3* axes) {
    g_atGeometryMatrix.GetPosition(m, position);
    g_atGeometryMatrix.GetAxes(m, &axes[0], &axes[1], &axes[2]);
}

// The result of the two kinds of overlap, the same for both: a contact, if anyone is asking.
int Found(AtBoundingGeometry* mine, AtBoundingGeometry* other) {
    if (AtBoundingGeometry::s_contacts != 0) {
        AtBoundingGeometry::s_contacts->Add(mine, other, AtBoundingGeometry::s_node, AtBoundingGeometry::s_otherNode);
    }
    return 1;
}

// What Penetration does with a vector of its own frame: into world space, as a unit, and facing away from other.
void NormalToWorld(int jointIndex, AtSceneNode* node, const AtVector3* vector, AtVector3* normal) {
    if (jointIndex == 0) {
        AtCoordSys* coordSys = node->ParentCoordSys();
        if (coordSys != 0) {
            AtCoordSysTransformDirection(coordSys, vector, normal);
        } else {
            *normal = *vector;
        }
    } else {
        AtCoordSysTransformDirection(JointOf(node, jointIndex), vector, normal);
    }
    normal->Normalize();
    const AtVector3& to = AtBoundingGeometry::s_relativePosition;
    if (D(to.z) * vector->z + D(to.y) * vector->y + D(to.x) * vector->x < 0.0) {
        normal->x = -normal->x;
        normal->y = -normal->y;
        normal->z = -normal->z;
    }
}

// What PenetrationAlong does with its direction: out of world space, as a unit.
void DirectionFromWorld(int jointIndex, AtSceneNode* node, const AtVector3* direction, AtVector3* local) {
    if (jointIndex == 0) {
        AtCoordSys* coordSys = node->ParentCoordSys();
        if (coordSys != 0) {
            coordSys->TransformDirectionInverse(direction, local);
        } else {
            *local = *direction;
        }
    } else {
        JointOf(node, jointIndex)->TransformDirectionInverse(direction, local);
    }
    local->Normalize();
}

// other's centre in a shape's frame, all stored.
AtVector3 StoredRelative(const AtVector3& center, const AtVector3& point) {
    double relative[3];
    Relative(AtBoundingGeometry::s_relativeAxes, AtBoundingGeometry::s_relativePosition, center, point, relative);
    AtVector3 r;
    r.x = (float)relative[0];
    r.y = (float)relative[1];
    r.z = (float)relative[2];
    return r;
}

// The original's arrays here have no count ahead of them, and are freed with no destructors. Null if there was no memory, after the raise.
template <class T> T* NewGeometries(unsigned int count) {
    if (AtBoundingGeometryDatabase::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtBoundingGeometryDatabase::s_heap);
    T* first = (T*)operator new(count * sizeof(T));
    if (first != 0) {
        T* each = first;
        for (int left = (int)count - 1; left >= 0; left--) {
            new (each, AtPlacement()) T;
            each++;
        }
    }
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}

// "[ n ]".
unsigned int ReadSize(AtIoTokenizer* tokenizer, unsigned int* count) {
    tokenizer->ExpectLeftBracket();
    *count = tokenizer->ExpectInt();
    tokenizer->ExpectRightBracket();
    return *count;
}

}  // namespace

AtGeometryMatrixTable g_atGeometryMatrix = {NoGetPosition, NoGetAxes, NoTransformDirection};

// ---- AtCoordSys, for this ----------------------------------------------------------------------------------------------

int AtCoordSys::UpdateDrawing(AtMatrix* base, int generation, int multiply) {
    if (m_parent == 0) {
        if ((m_dirty & 2) == 0 && generation == m_unknown20 && multiply != 0) {
            return 0;
        }
        if (multiply != 0) {
            g_atMatrix.SetProduct(m_world, m_local, base);
        } else {
            g_atMatrix.Copy(m_world, base);
        }
        m_unknown20 = generation;
    } else {
        m_parent->UpdateDrawing(base, generation, multiply);
        if ((m_dirty & 2) == 0 && generation == m_unknown20) {
            return 0;
        }
        m_unknown20 = generation;
        if (multiply != 0) {
            g_atMatrix.SetProduct(m_world, m_local, m_parent->m_world);
        } else {
            g_atMatrix.Copy(m_world, m_parent->m_world);
        }
    }
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->m_dirty |= 2;
    }
    m_dirty = (m_dirty & ~2u) | 0x29;
    return 2;
}

void AtCoordSys::TransformDirectionInverse(const AtVector3* from, AtVector3* to) {
    UpdateInverse();
    g_atGeometryMatrix.TransformDirection(m_world, from, to);
}

// ---- the owner's table --------------------------------------------------------------------------------------------------

void AtNameTableOwner::AddGeometry(GmNameMap* map, const char* ns) {
    if (ns != 0) {
        m_geometrySpaces.Add(ns, map);
    } else {
        m_geometry.Add(map);
    }
}

// ---- the list of contacts ------------------------------------------------------------------------------------------------

void AtBoundingContactList::Add(AtBoundingGeometry* first, AtBoundingGeometry* second, AtSceneNode* firstNode, AtSceneNode* secondNode) {
    if (m_count >= m_capacity) {
        return;
    }
    if ((m_flags & kLeavesOnly) != 0 && (first->m_firstChild != 0 || second->m_firstChild != 0)) {
        return;
    }
    m_items[m_count].m_first = first;
    m_items[m_count].m_second = second;
    m_items[m_count].m_firstNode = firstNode;
    m_items[m_count].m_secondNode = secondNode;
    m_count++;
}

// ---- the base ----------------------------------------------------------------------------------------------------------

AtBoundingGeometry::AtBoundingGeometry() {
    m_kind = -1;
    m_jointIndex = 0;
    m_unknown0C = 0;
    m_parent = 0;
    m_nextSibling = 0;
    m_firstChild = 0;
    m_database = 0;
}

void AtBoundingGeometry::Draw(void*, AtGeometryDrawDevice*, AtSceneNode*, AtCoordSys*) {}

void AtBoundingGeometry::DrawTree(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys) {
    Draw(material, device, node, coordSys);
    for (AtBoundingGeometry* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->DrawTree(material, device, node, coordSys);
    }
}

void AtBoundingGeometry::SwapNodes() {
    AtSceneNode* node = s_node;
    s_node = s_otherNode;
    s_otherNode = node;
}

void AtBoundingGeometry::Read(GmNameMap* database, AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != kRightCurly) {
        if (token == kRotTransCoordSys) {
            m_jointIndex = tokenizer->ExpectInt();
        } else if (token == kParent) {
            char name[8];
            tokenizer->ExpectName(name);
            AtBoundingGeometry* parent = (AtBoundingGeometry*)database->Find(name);
            if (parent == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            // Given twice, the first parent keeps it in its list.
            if (m_parent != parent) {
                m_parent = parent;
                m_nextSibling = parent->m_firstChild;
                parent->m_firstChild = this;
            }
        } else {
            ReadWord(tokenizer, token);
        }
        token = tokenizer->Next();
    }
    m_database = database;
}

void AtBoundingGeometry::SetDrawTransform(AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys) {
    if (coordSys == 0) {
        if (m_jointIndex == 0) {
            coordSys = node->ParentCoordSys();
        } else {
            coordSys = JointOf(AtSceneNode::AsJointed(node), m_jointIndex);
        }
    }
    const int generation = device->DrawingGeneration();
    coordSys->UpdateDrawing(device->DrawingMatrix(), generation, 1);
    device->SetWorld(0, coordSys->m_world);
}

void AtBoundingGeometry::RelateTo(const AtBoundingGeometry* other) {
    AtVector3 position;
    AtVector3 axes[3];
    if (other->m_jointIndex == 0) {
        AtCoordSys* coordSys = s_otherCoordSys;
        if (coordSys == 0) {
            position.x = 0.0f;
            position.y = 0.0f;
            position.z = 0.0f;
            axes[0].x = 1.0f;
            axes[0].y = 0.0f;
            axes[0].z = 0.0f;
            axes[1].x = 0.0f;
            axes[1].y = 1.0f;
            axes[1].z = 0.0f;
            axes[2].x = 0.0f;
            axes[2].y = 0.0f;
            axes[2].z = 1.0f;
        } else {
            coordSys->UpdateWorld();
            ReadMatrixAxes(s_otherCoordSys->m_world, &position, axes);
        }
    } else {
        AtCoordSys* joint = CheckedJointOf(s_otherNode, other->m_jointIndex);
        joint->UpdateWorld();
        ReadMatrixAxes(joint->m_world, &position, axes);
    }

    AtCoordSys* mine;
    if (m_jointIndex == 0) {
        mine = s_coordSys;
        if (mine == 0) {
            s_relativePosition = position;
            s_relativeAxes[0] = axes[0];
            s_relativeAxes[1] = axes[1];
            s_relativeAxes[2] = axes[2];
            return;
        }
    } else {
        mine = CheckedJointOf(s_node, m_jointIndex);
    }
    mine->UpdateInverse();
    g_atMatrix.TransformPoint(mine->m_world, &position, &s_relativePosition);
    g_atGeometryMatrix.TransformDirection(mine->m_world, &axes[0], &s_relativeAxes[0]);
    g_atGeometryMatrix.TransformDirection(mine->m_world, &axes[1], &s_relativeAxes[1]);
    g_atGeometryMatrix.TransformDirection(mine->m_world, &axes[2], &s_relativeAxes[2]);
}

// ---- a group ------------------------------------------------------------------------------------------------------------

AtGroupBoundingGeometry::AtGroupBoundingGeometry() {
    m_kind = kGroup;
}

void AtGroupBoundingGeometry::ReadWord(AtIoTokenizer* tokenizer, int) {
    tokenizer->Error(kUnexpectedWord);
}

int AtGroupBoundingGeometry::Overlaps(AtBoundingGeometry*) {
    return 1;
}

int AtGroupBoundingGeometry::TestSegment() {
    return 1;
}

void AtGroupBoundingGeometry::CollectSegment() {}

double AtGroupBoundingGeometry::Penetration(AtSceneNode*, AtBoundingGeometry*, AtSceneNode*, AtVector3*, float*, float*) {
    return 0.0;
}

double AtGroupBoundingGeometry::PenetrationAlong(AtSceneNode*, AtBoundingGeometry*, AtSceneNode*, const AtVector3*, float*, float*) {
    return 0.0;
}

// ---- a sphere -----------------------------------------------------------------------------------------------------------

AtSphereBoundingGeometry::AtSphereBoundingGeometry() {
    m_kind = kSphere;
}

void AtSphereBoundingGeometry::ReadWord(AtIoTokenizer* tokenizer, int token) {
    if (token != kSphereShape) {
        tokenizer->Error(kUnexpectedWord);
        return;
    }
    m_sphere.center.Read(tokenizer);
    m_sphere.radius = Abs(tokenizer->ExpectReal());
}

int AtSphereBoundingGeometry::Overlaps(AtBoundingGeometry* other) {
    if (other->m_kind == kSphere) {
        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;
        RelateTo(other);
        double relative[3];
        Relative(s_relativeAxes, s_relativePosition, m_sphere.center, theirs.center, relative);
        AtVector3 center;
        center.x = (float)(relative[0] + m_sphere.center.x);
        center.y = (float)(relative[1] + m_sphere.center.y);
        center.z = (float)(relative[2] + m_sphere.center.z);
        const float reach = (float)(D(theirs.radius) + m_sphere.radius);
        if (!(D(reach) * reach >= DistanceSquaredHeld(center, m_sphere.center))) {
            return 0;
        }
        return Found(this, other);
    }
    if (other->m_kind == kBox) {
        SwapNodes();
        const int result = other->Overlaps(this);
        SwapNodes();
        return result;
    }
    return other->m_kind == kGroup ? 1 : 0;
}

int AtSphereBoundingGeometry::TestSegment() {
    float length = (float)(D(s_segmentHalfLength) + s_segmentHalfLength);
    float key;
    AtVector3 point;
    return AtSphereSegment(&m_sphere, &s_segmentStart, &s_segmentEnd, &length, &s_segmentDirection, &point, &key, 0);
}

void AtSphereBoundingGeometry::CollectSegment() {
    float length = (float)(D(s_segmentHalfLength) + s_segmentHalfLength);
    float key;
    AtVector3 point;
    if (AtSphereSegment(&m_sphere, &s_segmentStart, &s_segmentEnd, &length, &s_segmentDirection, &point, &key, 0) != 0 && AtSegmentQueryInsert(s_query, key) != 0) {
        s_query->m_records[s_query->m_current].m_geometry = this;
        AtSegmentQueryFinish(s_query, 3);
    }
}

double AtSphereBoundingGeometry::Penetration(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, AtVector3* normal, float* extent, float* otherExtent) {
    if (other->m_kind == kSphere) {
        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;
        RelateTo(other);
        AtVector3 between = StoredRelative(m_sphere.center, theirs.center);
        const float depth = (float)(NormalizeHeld(&between) - m_sphere.radius - theirs.radius);
        *extent = (float)(D(m_sphere.radius) + m_sphere.radius);
        *otherExtent = (float)(D(theirs.radius) + theirs.radius);
        NormalToWorld(m_jointIndex, node, &between, normal);
        return depth;
    }
    if (other->m_kind == kBox) {
        SwapNodes();
        const float depth = (float)other->Penetration(otherNode, this, node, normal, otherExtent, extent);
        SwapNodes();
        normal->x = -normal->x;
        normal->y = -normal->y;
        normal->z = -normal->z;
        return depth;
    }
    return 0.0;
}

double AtSphereBoundingGeometry::PenetrationAlong(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode* otherNode, const AtVector3* direction, float* extent, float* otherExtent) {
    if (other->m_kind == kSphere) {
        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;
        RelateTo(other);
        AtVector3 local;
        DirectionFromWorld(m_jointIndex, node, direction, &local);
        const AtVector3 between = StoredRelative(m_sphere.center, theirs.center);
        local.Normalize();  // Again.
        const double depth = D(between.z) * local.z + D(between.y) * local.y + D(local.x) * between.x - m_sphere.radius - theirs.radius;
        *extent = (float)(D(m_sphere.radius) + m_sphere.radius);
        *otherExtent = (float)(D(theirs.radius) + theirs.radius);
        return depth;
    }
    if (other->m_kind == kBox) {
        SwapNodes();
        AtVector3 reversed;
        reversed.x = -direction->x;
        reversed.y = -direction->y;
        reversed.z = -direction->z;
        const float depth = (float)other->PenetrationAlong(otherNode, this, node, &reversed, otherExtent, extent);
        SwapNodes();
        return depth;
    }
    return 0.0;
}

void AtSphereBoundingGeometry::Draw(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys) {
    SetDrawTransform(device, node, coordSys);
    device->DrawSphere(&m_sphere, material, 6, 6, 0);
}

// ---- a box --------------------------------------------------------------------------------------------------------------

AtBoxBoundingGeometry::AtBoxBoundingGeometry() {
    m_kind = kBox;
}

void AtBoxBoundingGeometry::ReadWord(AtIoTokenizer* tokenizer, int token) {
    if (token == kBoxShape) {
        m_box.Read(tokenizer);
        return;
    }
    if (token != kBoxMinMax) {
        tokenizer->Error(kUnexpectedWord);
        return;
    }
    AtVector3 low;
    AtVector3 high;
    low.Read(tokenizer);
    high.Read(tokenizer);
    m_box.m_half.x = (float)(D(high.x) - low.x);
    m_box.m_half.y = (float)(D(high.y) - low.y);
    m_box.m_half.z = (float)(D(high.z) - low.z);
    m_box.m_half.x = (float)(D(m_box.m_half.x) * 0.5f);
    m_box.m_half.y = (float)(D(m_box.m_half.y) * 0.5f);
    m_box.m_half.z = (float)(D(m_box.m_half.z) * 0.5f);
    m_box.m_center.x = (float)(D(low.x) + m_box.m_half.x);
    m_box.m_center.y = (float)(D(low.y) + m_box.m_half.y);
    m_box.m_center.z = (float)(D(low.z) + m_box.m_half.z);
}

int AtBoxBoundingGeometry::Overlaps(AtBoundingGeometry* other) {
    if (other->m_kind == kBox) {
        RelateTo(other);
        return m_box.Overlaps(&((AtBoxBoundingGeometry*)other)->m_box, s_relativeAxes, &s_relativePosition) != 0 ? Found(this, other) : 0;
    }
    if (other->m_kind == kSphere) {
        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;
        RelateTo(other);
        AtSceneSphere sphere;
        sphere.center = StoredRelative(m_box.m_center, theirs.center);
        sphere.radius = theirs.radius;
        return m_box.OverlapsSphere(&sphere) != 0 ? Found(this, other) : 0;
    }
    return other->m_kind == kGroup ? 1 : 0;
}

int AtBoxBoundingGeometry::TestSegment() {
    return m_box.OverlapsSegment(&s_segmentDirection, &s_segmentMiddle, s_segmentHalfLength);
}

void AtBoxBoundingGeometry::CollectSegment() {
    AtVector3 clippedStart;
    AtVector3 clippedEnd;
    float tStart;
    float tEnd;
    const unsigned int faces = m_box.Clip(&s_segmentStart, &s_segmentEnd, &clippedStart, &clippedEnd, &tStart, &tEnd);
    if ((faces & 0x3F) != 0 && AtSegmentQueryInsert(s_query, tStart) != 0) {
        s_query->m_records[s_query->m_current].m_geometry = this;
        s_query->m_records[s_query->m_current].m_faces = faces;
        AtSegmentQueryFinish(s_query, 2);
    }
    if ((faces & 0xFC0) != 0 && (s_query->m_flags & 0x10) == 0 && AtSegmentQueryInsert(s_query, tEnd) != 0) {
        s_query->m_records[s_query->m_current].m_geometry = this;
        s_query->m_records[s_query->m_current].m_faces = faces;
        AtSegmentQueryFinish(s_query, 2);
    }
}

double AtBoxBoundingGeometry::Penetration(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode*, AtVector3* normal, float* extent, float* otherExtent) {
    AtVector3 local;
    float depth;
    if (other->m_kind == kBox) {
        RelateTo(other);
        depth = (float)m_box.Penetration(&((AtBoxBoundingGeometry*)other)->m_box, s_relativeAxes, &s_relativePosition, &local, extent, otherExtent);
    } else if (other->m_kind == kSphere) {
        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;
        RelateTo(other);
        AtSceneSphere sphere;
        sphere.center = StoredRelative(m_box.m_center, theirs.center);
        sphere.radius = theirs.radius;
        depth = (float)m_box.PenetrationSphere(&sphere, &local, extent, otherExtent);
    } else {
        return 0.0;
    }
    NormalToWorld(m_jointIndex, node, &local, normal);
    return depth;
}

double AtBoxBoundingGeometry::PenetrationAlong(AtSceneNode* node, AtBoundingGeometry* other, AtSceneNode*, const AtVector3* direction, float* extent, float* otherExtent) {
    AtVector3 local;
    if (other->m_kind == kBox) {
        RelateTo(other);
        DirectionFromWorld(m_jointIndex, node, direction, &local);
        return m_box.Penetration(&((AtBoxBoundingGeometry*)other)->m_box, s_relativeAxes, &s_relativePosition, &local, extent, otherExtent);
    }
    if (other->m_kind == kSphere) {
        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;
        RelateTo(other);
        AtSceneSphere sphere;
        sphere.center = StoredRelative(m_box.m_center, theirs.center);
        sphere.radius = theirs.radius;
        DirectionFromWorld(m_jointIndex, node, direction, &local);
        return m_box.PenetrationSphere(&sphere, &local, extent, otherExtent);
    }
    return 0.0;
}

void AtBoxBoundingGeometry::Draw(void* material, AtGeometryDrawDevice* device, AtSceneNode* node, AtCoordSys* coordSys) {
    SetDrawTransform(device, node, coordSys);
    device->DrawBox(&m_box, material, 0);
}

// ---- the database -------------------------------------------------------------------------------------------------------

AtBoundingGeometryDatabase::AtBoundingGeometryDatabase() {
    m_boxCount = 0;
    m_sphereCount = 0;
    m_groupCount = 0;
    m_boxes = 0;
    m_spheres = 0;
    m_groups = 0;
}

AtBoundingGeometryDatabase::~AtBoundingGeometryDatabase() {
    AtBoundingGeometryDatabase::Clear();
}

void AtBoundingGeometryDatabase::Clear() {
    if (m_groups != 0) {
        operator delete(m_groups);
    }
    if (m_spheres != 0) {
        operator delete(m_spheres);
    }
    if (m_boxes != 0) {
        operator delete(m_boxes);
    }
    GmNameMap::Clear();
    m_boxCount = 0;
    m_sphereCount = 0;
    m_groupCount = 0;
    m_boxes = 0;
    m_spheres = 0;
    m_groups = 0;
}

void AtBoundingGeometryDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

void AtBoundingGeometryDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ASB", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASB");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

void AtBoundingGeometryDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_entries != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    char space[8];
    const char* ns = 0;
    int token = tokenizer->Next();
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }

    // The counts. A list given twice is reported, and allocated again, leaking the first.
    while (token != kBoundingGeometry) {
        if (token == kEndOfFile) {
            Init(m_groupCount + m_sphereCount + m_boxCount);
            owner->AddGeometry(this, ns);
            return;
        }
        if (token == kBoxWord) {
            if (m_boxCount != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_boxes = NewGeometries<AtBoxBoundingGeometry>(ReadSize(tokenizer, &m_boxCount));
        } else if (token == kSphereWord) {
            if (m_sphereCount != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_spheres = NewGeometries<AtSphereBoundingGeometry>(ReadSize(tokenizer, &m_sphereCount));
        } else if (token == kGroupWord) {
            if (m_groupCount != 0) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            m_groups = NewGeometries<AtGroupBoundingGeometry>(ReadSize(tokenizer, &m_groupCount));
        } else {
            tokenizer->Error(kUnexpectedWord);
        }
        token = tokenizer->Next();
    }

    const unsigned int total = m_groupCount + m_sphereCount + m_boxCount;
    Init(total);
    owner->AddGeometry(this, ns);

    unsigned int boxes = 0;
    unsigned int spheres = 0;
    unsigned int groups = 0;
    const unsigned int listed = tokenizer->ExpectCount();
    if (listed > total) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    for (unsigned int i = 0; i < listed; i++) {
        token = tokenizer->Next();
        AtBoundingGeometry* geometry;
        char name[8];
        if (token == kBoxWord) {
            if (boxes >= m_boxCount) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            geometry = m_boxes + boxes;
            boxes++;
        } else if (token == kSphereWord) {
            if (spheres >= m_sphereCount) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            geometry = m_spheres + spheres;
            spheres++;
        } else if (token == kGroupWord) {
            if (groups >= m_groupCount) {
                tokenizer->Error(kAtProblemWrongUse);
            }
            geometry = m_groups + groups;
            groups++;
        } else {
            tokenizer->Error(kUnexpectedWord);
            continue;
        }
        tokenizer->ExpectName(name);
        Insert(name, geometry);
        geometry->Read(this, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

// ---- the list of hits ---------------------------------------------------------------------------------------------------

AtSegmentHitList::AtSegmentHitList() {
    Reset();
}

AtSegmentHitList::~AtSegmentHitList() {
    if ((m_flags & kNotOwned) == 0) {
        if (m_hits != 0) {
            operator delete(m_hits);
        }
        if (m_keys != 0) {
            operator delete(m_keys);
        }
    }
    Reset();
}

void AtSegmentHitList::SetHeap(void* heap) {
    s_heap = heap;
}

void AtSegmentHitList::Reset() {
    m_vectors0[0].x = 0.0f;
    m_vectors0[0].y = 0.0f;
    m_vectors0[0].z = 0.0f;
    m_vectors0[1].x = 0.0f;
    m_vectors0[1].y = 0.0f;
    m_vectors0[1].z = 0.0f;
    m_first0 = &m_vectors0[0];
    m_second0 = &m_vectors0[1];
    m_vectors2[0].x = 0.0f;
    m_vectors2[0].y = 0.0f;
    m_vectors2[0].z = 0.0f;
    m_vectors2[1].x = 0.0f;
    m_vectors2[1].y = 0.0f;
    m_vectors2[1].z = 0.0f;
    m_vectors1[0].x = 0.0f;
    m_vectors1[0].y = 0.0f;
    m_vectors1[0].z = 0.0f;
    m_vectors1[1].x = 0.0f;
    m_vectors1[1].y = 0.0f;
    m_vectors1[1].z = 0.0f;
    m_first2 = &m_vectors2[0];
    m_second2 = &m_vectors2[1];
    m_unknown60[0] = 0;
    m_unknown60[1] = 0;
    m_unknown60[2] = 0;
    m_unknown60[3] = 0;
    m_flags = 0;
    m_unknown60[4] = 0;
    m_unknown60[5] = 0;
    m_capacity = 0;
    m_keys = 0;
    m_hits = 0;
    m_count = 0;
    m_unknown8C = 0;
}

void AtSegmentHitList::Clear() {
    if ((m_flags & kNotOwned) == 0) {
        if (m_hits != 0) {
            operator delete(m_hits);
        }
        if (m_keys != 0) {
            operator delete(m_keys);
        }
    }
    Reset();
}

void AtSegmentHitList::CountOne() {
    if (m_count < m_capacity) {
        m_count++;
    }
}

void AtSegmentHitList::SetStorage(unsigned int capacity, void* keys, void* hits) {
    if (m_capacity != 0) {
        Clear();
    }
    m_hits = hits;
    m_capacity = capacity;
    m_keys = keys;
    m_flags |= kNotOwned;
}

void AtSegmentHitList::Allocate(unsigned int capacity) {
    if (m_capacity != 0) {
        Clear();
    }
    m_capacity = capacity;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_keys = operator new(m_capacity * kKeySize);
    AtPopHeap();
    if (m_keys == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned char* keyBytes = (unsigned char*)m_keys;
    for (unsigned int i = 0; i < m_capacity * kKeySize; i++) {
        keyBytes[i] = 0;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_hits = operator new(m_capacity * kHitSize);
    AtPopHeap();
    if (m_hits == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}
