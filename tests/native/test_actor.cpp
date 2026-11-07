// Host test of the actors through the real tokenizer and the real seams of src/at/AtSceneActorsHost.cpp, on any pointer size. Not evidence of a byte match. No game data: the text is written here, in the shape real files have.
//
//   test_actor           the written cases
//   test_actor <file>    loads one scene text the way a real build would, and clears it. Exit 0 if it loaded with nothing reported. A class of node not recovered ends the process through platform/fatal.cpp, with its diagnostic.
//
// What a real build has that does not exist yet is stood in for here: the device and its matrices, and what its tables hold, which comes from other files. No class of node is a stand-in.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtSceneActor.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

struct Mat { float position[3]; };
void Copy(AtMatrix* to, const AtMatrix* from) { *(Mat*)to = *(const Mat*)from; }
void Identity(AtMatrix* m) { std::memset(m, 0, sizeof(Mat)); }
void SetTranslation(AtMatrix* m, const AtVector3* p) { Mat* x = (Mat*)m; x->position[0] = p->x; x->position[1] = p->y; x->position[2] = p->z; }
void SetRotation(AtMatrix*, const AtQuaternion*) {}
void SetLookAt(AtMatrix*, const AtVector3*, const AtVector3*) {}
void SetScale(AtMatrix*, const AtVector3*) {}
void SetInverse(AtMatrix*, const AtMatrix*) {}
void Multiply(AtMatrix*, const AtMatrix*) {}
void SetProduct(AtMatrix* to, const AtMatrix* a, const AtMatrix*) { *(Mat*)to = *(const Mat*)a; }
void TransformPoint(const AtMatrix* m, const AtVector3* from, AtVector3* to) {
    const Mat* x = (const Mat*)m;
    to->x = from->x + x->position[0];
    to->y = from->y + x->position[1];
    to->z = from->z + x->position[2];
}

class Pool : public AtMatrixPool {
public:
    std::vector<std::vector<Mat>*> m_blocks;
    void Slot0() override {}
    void Reserve(unsigned int count) override { m_blocks.push_back(new std::vector<Mat>(count + 1)); }
    void Clear() override {}
    unsigned int Count() override { return m_blocks.empty() ? 0 : (unsigned int)m_blocks.back()->size() - 1; }
    AtMatrix* At(unsigned int index) override { return (AtMatrix*)&m_blocks.back()->at(index); }
};

class Matrices : public AtMatrixDevice {
public:
    Pool m_pool;
    AtMatrix* NewMatrix() override { return (AtMatrix*)new Mat(); }
    AtMatrixPool* NewPool() override { return &m_pool; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix* m) override { delete (Mat*)m; }
    void FreePool(AtMatrixPool*) override {}
};

class Device : public AtSceneDevice {
public:
    Matrices m_matrices;
    void Slot0() override {} void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {}
    void Slot12() override {} void Slot13() override {} void Slot14() override {} void Slot15() override {} void Slot16() override {}
    AtMatrixDevice* MatrixDevice() override { return &m_matrices; }
};

int g_heap;

// What a device and an owner would hold, for each of the names given.
struct World {
    Device device;
    AtNameTableOwner* owner;
    int thing = 0;
    AtSceneMatAnimation matAnimation = {};
    AtSceneAnimation animation = {};
    std::map<std::string, AtCoordSysDatabase*> database;
    AtCoordSysDatabase targetDatabase;
    GmNameMap things, modifiers, geometry, meshes, matAssignments, matAnimations, mirrors, inner, animations, weights, databases, nodes;
    AtSceneJointedModelActor* target = nullptr;

    explicit World(const std::set<std::string>& names) {
        owner = new (std::calloc(1, sizeof(AtNameTableOwner))) AtNameTableOwner;
        owner->m_device = &device;
        const unsigned size = (unsigned)names.size() + 4;
        owner->m_nodeSpaces.Init(4);
        owner->m_modifierSpaces.Init(4);
        owner->m_geometrySpaces.Init(4);
        device.m_meshSpaces.Init(4);
        device.m_matAssignmentSpaces.Init(4);
        device.m_matAnimationSpaces.Init(4);
        matAnimation.m_rate = 30.0f;
        matAnimation.m_frames = 7;
        animation.m_rate = 15.0f;
        animation.m_frames = 11;
        const auto makeDatabase = [&](AtCoordSysDatabase* d) {
            d->Allocate(&device.m_matrices, 2, 1, 1);
            d->m_rotTrans[0].Attach(&d->m_rotTrans[1]);
            d->m_rotTrans[1].Attach(&d->m_scaled[0]);
            d->m_root = &d->m_rotTrans[0];
            for (const std::string& n : names) d->Insert(n.c_str(), &d->m_rotTrans[1]);
        };
        for (GmNameMap* m : {&modifiers, &geometry, &meshes, &matAssignments, &matAnimations, &mirrors, &inner, &animations, &weights, &databases, &nodes}) m->Init(size);
        target = new AtSceneJointedModelActor;
        target->m_owner = owner;
        makeDatabase(&targetDatabase);
        target->m_coordSys = &targetDatabase.m_rotTrans[0];
        target->m_joints.m_database = &targetDatabase;
        for (const std::string& n : names) {
            modifiers.Insert(n.c_str(), &thing);
            geometry.Insert(n.c_str(), &thing);
            meshes.Insert(n.c_str(), &thing);
            matAssignments.Insert(n.c_str(), &thing);
            matAnimations.Insert(n.c_str(), &matAnimation);
            mirrors.Insert(n.c_str(), &thing);
            inner.Insert(n.c_str(), &animation);
            animations.Insert(n.c_str(), &inner);
            weights.Insert(n.c_str(), &inner);
            database[n] = new AtCoordSysDatabase;
            makeDatabase(database[n]);
            databases.Insert(n.c_str(), database[n]);
            nodes.Insert(n.c_str(), target);
        }
        owner->m_modifiers.Add(&modifiers);
        owner->m_geometry.Add(&geometry);
        owner->m_nodes.Add(&nodes);
        device.m_meshes.Add(&meshes);
        device.m_matAssignments.Add(&matAssignments);
        device.m_matAnimations.Add(&matAnimations);
        device.m_mirrorDatabases.Add(&mirrors);
        device.m_animationDatabases.Add(&animations);
        device.m_weightDatabases.Add(&weights);
        device.m_coordSysDatabases.Add(&databases);
    }
};

void set_tables() {
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
    AtSceneActor::SetHeap(&g_heap);
    AtSceneAnimSet::SetHeap(&g_heap);
    AtSceneAnimBlend::SetHeap(&g_heap);
    AtCoordSysDatabase::SetHeap(&g_heap);
    AtIoStream::s_heap = &g_heap;
    AtIoTokenFile::s_heap = &g_heap;
}

void read(AtScene& scene, AtNameTableOwner& owner, const char* text, size_t size) {
    std::vector<char> copy(text, text + size);
    copy.push_back(0);
    Memory stream;
    stream.m_base = copy.data();
    stream.m_cur = copy.data();
    stream.m_limit = copy.data() + size;
    stream.m_size = (unsigned int)size;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    scene.Read(&owner, &tokenizer);
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
}

std::set<std::string> names_in(const char* text, size_t size) {
    std::set<std::string> names;
    size_t i = 0;
    while (i < size) {
        if (text[i] == '/' && i + 1 < size && text[i + 1] == '/') {
            while (i < size && text[i] != '\n') i++;
        } else if (text[i] == '"') {
            size_t e = i + 1;
            while (e < size && text[e] != '"' && text[e] != '\n') e++;
            std::string s(text + i + 1, e - i - 1);
            if (s.size() > 8) s.resize(8);
            if (!s.empty()) names.insert(s);
            i = e + 1;
        } else {
            i++;
        }
    }
    return names;
}

int load_one_file(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::printf("cannot read %s\n", path); return 2; }
    std::fseek(f, 0, SEEK_END);
    std::vector<char> data((size_t)std::ftell(f));
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty() && std::fread(data.data(), 1, data.size(), f) != data.size()) { std::fclose(f); return 2; }
    std::fclose(f);
    World world(names_in(data.data(), data.size()));
    AtScene scene;
    read(scene, *world.owner, data.data(), data.size());
    scene.Clear();
    std::printf("loaded and cleared: %s\n", path);
    return 0;
}

const char kScene[] =
    "RotTransCoordSys[3]\nScaleCoordSys[1]\nMotionState[1]\nNode[3]\n"
    "ModelActor[2]\n{\n"
    " ModelActor \"Crate\" RotTransCoordSys ScaleCoordSys\n {\n"
    "  Position 1.0 2.0 3.0\n  Scale 2.0 5.0 3.0\n  DrawingEnable\n  ModifierReceiver\n  ProjectionReceiver\n  Mesh \"Crate\" \"\"\n  BoundingSphere 1.0 0.0 0.0 2.0\n"
    "  CopyTextureCoordSysDatabase \"Crate\"\n  TextureCoordAnimationChannel[1]\n  {\n   TextureCoordAnimationChannel\n   {\n    PlayAnimation \"Crate\" \"Crate\" 25\n    PlaybackRate 30.0\n   }\n  }\n"
    "  MatAnimationChannel[1]\n  {\n   MatAnimationChannel\n   {\n    PlayMatAnimation \"Crate\" \"\" 9\n    PlaybackRate 12.5\n    MatAssignmentOffset 4\n   }\n  }\n }\n"
    " ModelActor \"Plain\"\n {\n  DrawingDisable\n  PhysicsActor \"thrown away\"\n  InitWithHavokPosition\n }\n"
    "}\n"
    "JointedModelActor[1]\n{\n"
    " JointedModelActor \"Rig\" RotTransCoordSys MotionState\n {\n"
    "  Mesh \"Rig\" \"\"\n  AnimationSkeletonFile \"thrown away\"\n  AnimateAtDrawEnable\n  CopyCoordSysDatabase \"Rig\"\n"
    "  AnimationChannel[2]\n  {\n   AnimationChannel\n   {\n    PlayAnimation \"Rig\" \"Rig\" 3\n    PlaybackRate 15.0\n    WeightMultiplier 0.5\n    Weights \"Rig\" \"Rig\"\n    ScaleTranslations 2.0 3.0 4.0\n    CoordSysMirrorDatabase \"Rig\"\n    CoordSysMirroringEnable\n   }\n"
    "   AnimationChannel\n   {\n    LinearVelocity\n    AngularVelocity\n    CoordSysMirroringEnable\n    CoordSysMirroringDisable\n   }\n  }\n }\n"
    "}\n";

}  // namespace

int main(int argc, char** argv) {
    set_tables();
    if (argc == 2) return load_one_file(argv[1]);

    {
        World world(names_in(kScene, sizeof(kScene) - 1));
        AtScene scene;
        read(scene, *world.owner, kScene, sizeof(kScene) - 1);
        CHECK(scene.m_modelCount == 2);
        CHECK(scene.m_jointedModelCount == 1);

        AtSceneModelActor* crate = (AtSceneModelActor*)scene.Find("Crate");
        CHECK(crate == (AtSceneModelActor*)scene.m_models);
        CHECK(crate->m_class == 2);
        CHECK(crate->m_mesh == &world.thing);
        CHECK(crate->m_flags & AtSceneActor::kDrawing);
        CHECK(crate->m_flags & AtSceneActor::kModifierReceiver);
        CHECK(crate->m_flags & AtSceneActor::kProjectionReceiver);
        CHECK(crate->m_flags & AtSceneActor::kOwnsAnimDatabase);
        CHECK(crate->m_scale == &scene.m_coordSys.m_scaled[0]);
        CHECK(crate->m_scale->m_parent == crate->m_coordSys);
        CHECK(crate->CoordSys() == crate->m_coordSys);
        CHECK(crate->ParentCoordSys() == crate->m_scale);
        CHECK(crate->m_scale->m_scale.y == 5.0f);
        CHECK(crate->m_worldSphere.radius == 10.0f);

        AtCoordSysDatabase* copy = crate->m_animations->m_database;
        CHECK(copy != world.database["Crate"]);
        CHECK(copy->m_rotTransCount == 2);
        CHECK(copy->m_scaledCount == 1);
        CHECK(copy->m_rotTrans[1].m_parent == &copy->m_rotTrans[0]);
        CHECK(copy->m_scaled[0].m_parent == &copy->m_rotTrans[1]);
        CHECK(copy->m_root == &copy->m_rotTrans[0]);
        // One name an element in a copy, whichever the source's table comes to first.
        char nameInSource[9] = {};
        world.database["Crate"]->GetNameOf(&world.database["Crate"]->m_rotTrans[1], nameInSource);
        CHECK(nameInSource[0] != 0);
        CHECK(copy->Find(nameInSource) == &copy->m_rotTrans[1]);

        AtSceneAnimChannel* channel = &crate->m_animations->m_channels[0];
        CHECK(crate->m_animations->m_channelCount == 1);
        CHECK(channel->m_animation == &world.animation);
        CHECK(channel->m_time == 3.0f);      // 25 modulo 11 frames, as well as the first of a kind.
        CHECK(channel->m_rate == 30.0f);
        CHECK(channel->m_speed == 2.0f);
        CHECK(channel->m_flags == 7);
        CHECK(crate->m_animations->m_blend.m_rotations == nullptr);  // One channel: no room to blend.

        AtSceneMatChannel* mat = &crate->m_matChannels[0];
        CHECK(crate->m_matChannelCount == 1);
        CHECK(mat->m_animation == &world.matAnimation);
        CHECK(mat->m_time == 2.0f);
        CHECK(mat->m_rate == 12.5f);
        CHECK(mat->m_assignmentOffset == 4);

        AtSceneModelActor* plain = (AtSceneModelActor*)scene.Find("Plain");
        CHECK(plain == (AtSceneModelActor*)scene.m_models + 1);
        CHECK(plain->m_mesh == nullptr);
        CHECK(plain->m_animations == nullptr);
        CHECK((plain->m_flags & AtSceneActor::kDrawing) == 0);

        AtSceneJointedModelActor* rig = (AtSceneJointedModelActor*)scene.Find("Rig");
        CHECK(rig == (AtSceneJointedModelActor*)scene.m_jointedModels);
        CHECK(rig->m_class == 3);
        CHECK(rig->m_flags & AtSceneJointedModelActor::kOwnsJointDatabase);
        CHECK(rig->m_flags & AtSceneJointedModelActor::kAnimateAtDraw);
        CHECK(rig->m_joints.m_channelCount == 2);
        CHECK(rig->m_joints.m_blend.m_rotTransCount == 2);       // Two channels: room to blend.
        CHECK(rig->m_joints.m_blend.m_scaleCount == 1);
        CHECK(rig->m_joints.m_blend.m_rotations[1].value[0] == 0.0f);
        CHECK(rig->m_joints.m_database->m_root->m_parent == rig->m_coordSys);
        AtSceneAnimChannel* first = &rig->m_joints.m_channels[0];
        CHECK(first->m_time == 3.0f);
        CHECK(first->m_weightMultiplier == 0.5f);
        CHECK(first->m_weights == &world.animation);
        CHECK(first->m_mirrors == &world.thing);
        CHECK(first->m_translationScale.z == 4.0f);
        CHECK(first->m_flags & AtScenePlayback::kHasTranslationScale);
        CHECK(first->m_options & AtSceneAnimChannel::kMirroring);
        AtSceneAnimChannel* second = &rig->m_joints.m_channels[1];
        CHECK(second->m_options == (AtSceneAnimChannel::kLinearVelocity | AtSceneAnimChannel::kAngularVelocity));
        CHECK(second->m_animation == nullptr);
        CHECK(second->m_translationScale.x == 1.0f);

        scene.Clear();
        CHECK(scene.m_models == nullptr);
        CHECK(scene.m_jointedModels == nullptr);
    }

    // Sharing a jointed actor's database, a database by name, and a coordinate system.
    {
        const char text[] = "RotTransCoordSys[2]\nNode[3]\nJointedModelActor[2]\n{\n"
                            " JointedModelActor \"A\" RotTransCoordSys\n {\n  CoordSysDatabase \"A\"\n }\n"
                            " JointedModelActor \"B\" RotTransCoordSys\n {\n  ShareCoordSysDatabase \"A\" \"\"\n }\n"
                            "}\n";
        World world(names_in(text, sizeof(text) - 1));
        AtScene scene;
        read(scene, *world.owner, text, sizeof(text) - 1);
        AtSceneJointedModelActor* a = (AtSceneJointedModelActor*)scene.Find("A");
        AtSceneJointedModelActor* b = (AtSceneJointedModelActor*)scene.Find("B");
        CHECK(a->m_joints.m_database == world.database["A"]);
        CHECK(world.database["A"]->m_root->m_parent == a->m_coordSys);
        CHECK(b->m_joints.m_database == world.database["A"]);
        CHECK(b->m_jointsFrom == a);
        CHECK(b->m_coordSys == nullptr);
        CHECK(b->m_parent == a);
        CHECK(a->m_firstChild == b);
        CHECK(b->m_flags & AtSceneJointedModelActor::kSharesJoints);

        AtSceneModelActor other;
        other.m_coordSys = a->m_coordSys;
        other.ShareCoordSys(a);
        CHECK(other.m_sharedFrom == a);
        CHECK(other.ParentCoordSys() == a->ParentCoordSys());
        other.StopSharingCoordSys();
        CHECK(other.m_sharedFrom == nullptr);
        CHECK(other.m_coordSys == nullptr);
        other.SetMesh(&world.thing);
        CHECK(other.m_mesh == &world.thing);
        scene.Clear();
    }

    if (g_fail == 0) std::printf("test_actor: all passed\n");
    return g_fail ? 1 : 0;
}
