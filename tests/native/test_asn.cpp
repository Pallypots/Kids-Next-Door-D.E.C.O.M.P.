// Host test of the scene reader through the real text tokenizer, on any pointer size. Not evidence of a byte match. No game data: the text is written here, in the shape real files have.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtScene.h"
#include "at/AtSceneActor.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// A class not recovered, here a base node that takes Mesh and nothing else of its own.
class Actor : public AtSceneNode {
public:
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* t, int token) override {
        if (token == 113) {
            char name[8];
            t->ExpectName(name);
            t->ExpectName(name);
            m_meshes++;
            return;
        }
        AtSceneNode::ReadBody(owner, t, token);
    }
    int m_meshes = 0;
};

struct Mat { float position[3]; float rotation[4]; int identity; };
void Copy(AtMatrix* to, const AtMatrix* from) { *(Mat*)to = *(const Mat*)from; }
void Identity(AtMatrix* m) { std::memset(m, 0, sizeof(Mat)); ((Mat*)m)->identity = 1; ((Mat*)m)->rotation[3] = 1; }
void SetTranslation(AtMatrix* m, const AtVector3* p) { Mat* x = (Mat*)m; x->position[0] = p->x; x->position[1] = p->y; x->position[2] = p->z; x->identity = 0; }
void SetRotation(AtMatrix* m, const AtQuaternion* q) { Mat* x = (Mat*)m; x->rotation[0] = q->x; x->rotation[1] = q->y; x->rotation[2] = q->z; x->rotation[3] = q->w; x->identity = 0; }
void SetLookAt(AtMatrix*, const AtVector3*, const AtVector3*) {}
void SetScale(AtMatrix*, const AtVector3*) {}
void SetInverse(AtMatrix*, const AtMatrix*) {}
void Multiply(AtMatrix*, const AtMatrix*) {}
void SetProduct(AtMatrix* to, const AtMatrix* a, const AtMatrix* b) {
    Mat r = *(const Mat*)a;
    for (int i = 0; i < 3; i++) r.position[i] += ((const Mat*)b)->position[i];
    *(Mat*)to = r;
}
void TransformPoint(const AtMatrix* m, const AtVector3* from, AtVector3* to) {
    const Mat* x = (const Mat*)m;
    to->x = from->x + x->position[0];
    to->y = from->y + x->position[1];
    to->z = from->z + x->position[2];
}

class Pool : public AtMatrixPool {
public:
    std::vector<Mat> m_mats;
    int m_cleared = 0;
    void Slot0() override {}
    void Reserve(unsigned int count) override { m_mats.assign(count, Mat()); }
    void Clear() override { m_cleared++; }
    void Slot3() override {}
    AtMatrix* At(unsigned int index) override { return (AtMatrix*)&m_mats.at(index); }
};

class TestMatrices : public AtMatrixDevice {
public:
    Pool m_pool;
    int m_freedPools = 0;
    int m_freedMatrices = 0;
    AtMatrix* NewMatrix() override { return (AtMatrix*)new Mat(); }
    AtMatrixPool* NewPool() override { return &m_pool; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix* m) override { delete (Mat*)m; m_freedMatrices++; }
    void FreePool(AtMatrixPool*) override { m_freedPools++; }
};

class Device : public AtSceneDevice {
public:
    TestMatrices m_matrices;
    void Slot0() override {} void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {}
    void Slot12() override {} void Slot13() override {} void Slot14() override {} void Slot15() override {} void Slot16() override {}
    AtMatrixDevice* MatrixDevice() override { return &m_matrices; }
};

int g_heap;

void read(AtScene& scene, AtNameTableOwner& owner, const char* text) {
    std::vector<char> copy(text, text + std::strlen(text));
    Memory stream;
    stream.m_base = copy.data();
    stream.m_cur = copy.data();
    stream.m_limit = copy.data() + copy.size();
    stream.m_size = (unsigned int)copy.size();
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    scene.Read(&owner, &tokenizer);
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
}

int g_actorsMade;

}  // namespace

unsigned int AtSceneUnrecoveredActorsSize(int) { return sizeof(Actor); }
void AtSceneUnrecoveredActorsConstruct(int, void* first, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) new ((Actor*)first + i) Actor;
    g_actorsMade += (int)count;
}
void AtSceneUnrecoveredActorsDestroy(int, void* first, unsigned int count) {
    for (unsigned int i = count; i > 0; i--) ((Actor*)first + i - 1)->~Actor();
    g_actorsMade -= (int)count;
}
void* AtSceneFindDataLink(AtNameTableOwner*, const char*, const char*) { return nullptr; }

int main() {
    g_atMatrix.Copy = Copy;
    g_atMatrix.Identity = Identity;
    g_atMatrix.SetTranslation = SetTranslation;
    g_atMatrix.SetRotation = SetRotation;
    g_atMatrix.SetLookAt = SetLookAt;
    g_atMatrix.SetScale = SetScale;
    g_atMatrix.SetInverse = SetInverse;
    g_atMatrix.Multiply = Multiply;
    g_atMatrix.SetProduct = SetProduct;
    g_atMatrix.TransformPoint = TransformPoint;

    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtScene::SetHeap(&g_heap);
    AtSceneNode::SetHeap(&g_heap);
    AtCoordSysDatabase::SetHeap(&g_heap);

    AtNameTableOwner* holder = static_cast<AtNameTableOwner*>(std::calloc(1, sizeof(AtNameTableOwner)));
    AtNameTableOwner& owner = *new (holder) AtNameTableOwner;
    Device device;
    owner.m_device = &device;
    owner.m_nodeSpaces.Init(4);
    owner.m_modifierSpaces.Init(4);
    owner.m_geometrySpaces.Init(4);
    GmNameMap geometry;
    geometry.Init(2);
    int shape = 0;
    geometry.Insert("Geo", &shape);
    owner.m_geometry.Add(&geometry);
    GmNameMap modifiers;
    modifiers.Init(2);
    int modifier = 0;
    modifiers.Insert("Mod", &modifier);
    owner.m_modifiers.Add(&modifiers);
    GmNameMap meshes;
    meshes.Init(2);
    int mesh = 0;
    meshes.Insert("Thing", &mesh);
    device.m_meshes.Add(&meshes);
    device.m_meshSpaces.Init(2);
    AtSceneActor::SetHeap(&g_heap);

    {
        AtScene scene;
        read(scene, owner,
             "RotTransCoordSys[3]\nMotionState[1]\nNode[5]\n"
             "Node[2]\n{\n"
             " Node \"Base\" RotTransCoordSys\n {\n  Position 1.0 2.0 3.0\n  Quaternion 0.0 0.0 0.0 1.0\n  BoundingSphere 4.0 5.0 6.0 2.5\n  CollisionEnable\n  ActiveDistance 3.0\n }\n"
             " Node \"Child\" RotTransCoordSys MotionState\n {\n  AttachBase \"Base\" \"\"\n  LinearVelocity 7.0 8.0 9.0\n  AngularVelocity 1.0 1.5 2.0\n  BoundingGeometry \"Geo\" \"\"\n  BoundingSphere 0.0 0.0 0.0 1.0\n }\n"
             "}\n"
             "ModifierNode[1]\n{\n ModifierNode \"Mover\" \n {\n  Modifier \"Mod\" \"\"\n  ModificationEnable\n }\n}\n"
             "ModelActor[1]\n{\n ModelActor \"Thing\" RotTransCoordSys\n {\n  Mesh \"Thing\" \"\"\n  LineSegIntersectEnable\n }\n}\n"
             "ParticleSystemActor[1]\n{\n ParticleSystemActor \"Puff\"\n {\n  Mesh \"Puff\" \"\"\n  CollisionEnable\n }\n}\n");
        CHECK(scene.m_nodeCount == 2);
        CHECK(scene.m_modifierNodeCount == 1);
        CHECK(scene.m_modelCount == 1);
        CHECK(g_actorsMade == 1);
        CHECK(scene.m_rotTransUsed == 3);
        CHECK(scene.m_motionUsed == 1);
        CHECK(device.m_matrices.m_pool.m_mats.size() == 6);

        AtSceneNode* base = (AtSceneNode*)scene.Find("Base");
        AtSceneNode* child = (AtSceneNode*)scene.Find("Child");
        CHECK(base == &scene.m_nodes[0]);
        CHECK(child == &scene.m_nodes[1]);
        CHECK(owner.FindNode("Child", nullptr) == child);
        CHECK(owner.FindNode("Gone", "") == nullptr);
        CHECK(base->m_owner == &owner);
        CHECK(base->m_flags & AtSceneNode::kLoaded);
        CHECK(base->m_flags & AtSceneNode::kCollision);
        CHECK(base->m_activeDistanceSquared == 9.0f);
        CHECK(base->m_sphere.radius == 2.5f);
        CHECK(base->m_sphere.center.y == 5.0f);
        CHECK(base->m_worldSphere.radius == 2.5f);
        // The world sphere comes out through the matrices: the node's own offset, and for the child its parent's.
        CHECK(base->m_worldSphere.center.x == 5.0f);
        CHECK(child->m_parent == base);
        CHECK(base->m_firstChild == child);
        CHECK(child->m_coordSys->m_parent == base->m_coordSys);
        CHECK(child->m_worldSphere.center.y == 2.0f);
        CHECK(child->m_motion == &scene.m_motionStates[0]);
        CHECK(child->m_motion->m_linearVelocity.z == 9.0f);
        CHECK(child->m_motion->m_angularVelocity.y == 1.5f);
        CHECK(child->m_boundingGeometry == &shape);

        AtSceneModifierNode* mover = (AtSceneModifierNode*)scene.Find("Mover");
        CHECK(mover == &scene.m_modifierNodes[0]);
        CHECK(mover->m_class == 14);
        CHECK(mover->m_modifier == &modifier);
        CHECK(mover->m_flags & AtSceneNode::kModificationEnabled);

        AtSceneModelActor* thing = (AtSceneModelActor*)scene.Find("Thing");
        CHECK(thing != nullptr);
        CHECK(thing->m_class == 2);
        CHECK(thing->m_mesh == &mesh);
        CHECK(thing->m_flags & AtSceneNode::kLineSegIntersect);
        Actor* puff = (Actor*)scene.Find("Puff");
        CHECK(puff != nullptr);
        CHECK(puff->m_meshes == 1);
        CHECK(puff->m_flags & AtSceneNode::kCollision);

        // A copy owns a coordinate system and motion state of its own, and gives them back.
        {
            AtSceneNode copy;
            copy.CopyFrom(&owner, child);
            CHECK(copy.m_flags & AtSceneNode::kOwnsCoordSys);
            CHECK(copy.m_flags & AtSceneNode::kOwnsMotion);
            CHECK(copy.m_coordSys != child->m_coordSys);
            CHECK(copy.m_motion->m_linearVelocity.y == 8.0f);
            CHECK(copy.m_boundingGeometry == &shape);
        }
        CHECK(device.m_matrices.m_freedMatrices == 2);

        // Over what is there, with a name space.
        read(scene, owner, "NameSpace \"Set\"\nRotTransCoordSys[1]\nNode[1]\nNode[1]\n{\n Node \"Again\" RotTransCoordSys\n {\n  Position 1.0 1.0 1.0\n }\n}\n");
        CHECK(g_actorsMade == 0);
        CHECK(device.m_matrices.m_freedPools == 1);
        CHECK(owner.FindNode("Base", nullptr) == nullptr);
        CHECK(owner.FindNode("Again", "Set") == scene.m_nodes);
        CHECK(owner.FindNode("Again", nullptr) == nullptr);

        // A list of none is only freed.
        scene.Clear();
        read(scene, owner, "Node[1]\nNode[0]\n{\n}\nModelActor[0]\n{\n}\n");
        CHECK(scene.m_nodes != nullptr);
        CHECK(scene.m_models != nullptr);
        CHECK(g_actorsMade == 0);
    }
    CHECK(device.m_matrices.m_freedPools == 2);

    // The scale coordinate system, and destroying an array of them through the database.
    {
        AtCoordSysDatabase db;
        db.Allocate(&device.m_matrices, 1, 2, 3);
        CHECK(device.m_matrices.m_pool.m_mats.size() == 12);
        CHECK(db.m_nextMatrix == 6);
        AtVector3 scale;
        scale.x = 2.0f; scale.y = 4.0f; scale.z = 8.0f;
        db.m_scaled[0].SetScale(&scale);
        CHECK(db.m_scaled[0].m_scale.z == 8.0f);
        db.m_scaled[0].Attach(&db.m_rotTrans[0]);
        CHECK(db.m_rotTrans[0].m_parent == &db.m_scaled[0]);
        CHECK(db.m_rotTrans[0].m_dirty & AtCoordSys::kScaled);
        CHECK(db.m_rotTrans[0].ScaleFactor() == 1.0f);
        db.m_scaled[0].Detach(&db.m_rotTrans[0]);
        CHECK(db.m_rotTrans[0].m_parent == nullptr);
        CHECK((db.m_rotTrans[0].m_dirty & AtCoordSys::kScaled) == 0);
        db.m_scaled[0].Reset();
        CHECK(db.m_scaled[0].m_scale.x == 1.0f);
        CHECK(db.m_scaled[0].m_local == nullptr);
        db.Clear();
        CHECK(db.m_rotTrans == nullptr);
        CHECK(db.m_scaled == nullptr);
    }

    if (g_fail == 0) std::printf("ok\n");
    return g_fail ? 1 : 0;
}
