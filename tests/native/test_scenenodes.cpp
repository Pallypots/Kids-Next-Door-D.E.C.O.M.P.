// Host test of the group, water, lens flare and sound classes of a scene node and of the walks into a space graph, through the real tokenizer, scene and space graph, on any pointer size. Not evidence of a byte match. No game data: the text
// is written here, since no real .ASN file names these classes. The water drawer, lens flare, sound emitter, segments and the two emitter slots are stand-ins.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioDatabase.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtSceneDatabaseLoader.h"
#include "at/AtSceneNodes.h"
#include "at/AtSceneParticle.h"
#include "at/AtSpaceGraph.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// A matrix is a position and a mark of what was done last.
struct Mat { float position[3]; int op; };
void Copy(AtMatrix* to, const AtMatrix* from) { *(Mat*)to = *(const Mat*)from; }
void Identity(AtMatrix* m) { std::memset(m, 0, sizeof(Mat)); }
void SetTranslation(AtMatrix* m, const AtVector3* p) { Mat* x = (Mat*)m; x->position[0] = p->x; x->position[1] = p->y; x->position[2] = p->z; }
void SetRotation(AtMatrix* m, const AtQuaternion*) { ((Mat*)m)->op = 'q'; }
void MakeRotation(AtMatrix* m, const AtQuaternion* q) { std::memset(m, 0, sizeof(Mat)); ((Mat*)m)->op = q->w == 1.0f ? 'I' : 'r'; }
void ScaleAxes(AtMatrix* m, const AtVector3*) { ((Mat*)m)->op = 's'; }
void SetLookAt(AtMatrix*, const AtVector3*, const AtVector3*) {}
void SetScale(AtMatrix*, const AtVector3*) {}
void SetInverse(AtMatrix*, const AtMatrix*) {}
void Multiply(AtMatrix*, const AtMatrix*) {}
void SetProduct(AtMatrix* to, const AtMatrix* a, const AtMatrix* b) {
    Mat r = *(const Mat*)a;
    for (int i = 0; i < 3; i++) r.position[i] -= ((const Mat*)b)->position[i];
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
    void Slot0() override {}
    void Reserve(unsigned int count) override { m_mats.assign(count, Mat()); }
    void Clear() override {}
    unsigned int Count() override { return (unsigned int)m_mats.size(); }
    AtMatrix* At(unsigned int index) override { return (AtMatrix*)&m_mats.at(index); }
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

class Water : public AtSceneWaterDrawer {
public:
    unsigned m_count = 0;
    std::vector<AtSceneWaterPatch> m_patches;
    float m_rate = 0, m_damping = 0, m_generation = 0;
    void Slot0() override {}
    void Start(AtSceneDevice*, unsigned int count, const AtSceneWaterPatch* patches, float rate, float damping, float generation) override {
        m_count = count;
        m_patches.assign(patches, patches + count);
        m_rate = rate;
        m_damping = damping;
        m_generation = generation;
    }
};

class Drawers : public AtSceneParticleDrawerMaker {
public:
    int m_alive = 0;
    Water* m_last = nullptr;
    AtSceneParticleDrawer* New() override { return nullptr; }
    void Release(AtSceneParticleDrawer*) override {}
    AtSceneWaterDrawer* NewWater() override { m_alive++; return m_last = new Water; }
    void ReleaseWater(AtSceneWaterDrawer* drawer) override { m_alive--; delete (Water*)drawer; }
};

class Emitter : public AtAudioEmitter {
public:
    void InitPlatform() override {}
};

std::vector<std::string> g_played;
AtVector3 g_velocity = {9.0f, 9.0f, 9.0f};

void read(AtScene& scene, AtNameTableOwner& owner, const std::string& text) {
    std::vector<char> copy(text.begin(), text.end());
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

void read_graph(AtSpaceGraph& graph, AtNameTableOwner& owner, const std::string& text) {
    std::vector<char> copy(text.begin(), text.end());
    Memory stream;
    stream.m_base = copy.data();
    stream.m_cur = copy.data();
    stream.m_limit = copy.data() + copy.size();
    stream.m_size = (unsigned int)copy.size();
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    graph.Read(&owner, &tokenizer);
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
}

char g_segA, g_segB, g_flare;

}  // namespace

void AtSceneSoundSetVelocity(AtAudioEmitter*, const AtVector3* velocity) { g_velocity = *velocity; }
void AtSceneSoundPlay(AtAudioEmitter*, AtAudioSegment* segment, int unknown) {
    g_played.push_back(std::string((const char*)segment == &g_segA ? "A" : (const char*)segment == &g_segB ? "B" : "?") + std::to_string(unknown));
}
void* AtSceneFindDataLink(AtNameTableOwner*, const char*, const char*) { return nullptr; }

int main() {
    g_atMatrix.Copy = Copy;
    g_atMatrix.Identity = Identity;
    g_atMatrix.SetTranslation = SetTranslation;
    g_atMatrix.SetRotation = SetRotation;
    g_atMatrix.MakeRotation = MakeRotation;
    g_atMatrix.ScaleAxes = ScaleAxes;
    g_atMatrix.SetLookAt = SetLookAt;
    g_atMatrix.SetScale = SetScale;
    g_atMatrix.SetInverse = SetInverse;
    g_atMatrix.Multiply = Multiply;
    g_atMatrix.SetProduct = SetProduct;
    g_atMatrix.TransformPoint = TransformPoint;

    // What the space graph and its modifier states read the fields of must be a heap; on the host the memory itself is the C library's.
    static AtHeap heap;
    heap.m_alignShift = 2;
    heap.m_allocShift = 2;
    void* const g_heap = &heap;
    GmNameMap::s_heap = g_heap;
    GmNameSpaceDb::s_heap = g_heap;
    AtScene::SetHeap(g_heap);
    AtSceneNode::SetHeap(g_heap);
    AtSceneActor::SetHeap(g_heap);
    AtCoordSysDatabase::SetHeap(g_heap);
    AtIoTokenFile::s_heap = g_heap;
    AtIoStream::s_heap = g_heap;
    AtSpaceGraph::SetHeap(g_heap);
    AtModifierStack::SetHeap(g_heap);
    AtModifierState::SetHeap(g_heap);

    AtNameTableOwner* holder = static_cast<AtNameTableOwner*>(std::calloc(1, sizeof(AtNameTableOwner)));
    AtNameTableOwner& owner = *new (holder) AtNameTableOwner;
    AtNameTableOwner* soundHolder = static_cast<AtNameTableOwner*>(std::calloc(1, sizeof(AtNameTableOwner)));
    AtNameTableOwner& sounds = *new (soundHolder) AtNameTableOwner;
    Device device;
    Drawers drawers;
    owner.m_device = &device;
    owner.m_particleDrawers = &drawers;
    owner.m_unknown14 = &sounds;
    owner.m_modifierStack.m_depth = 0;
    owner.m_modifierStack.m_used = 0;
    owner.m_modifierStack.m_states = nullptr;
    owner.m_stackDepth = 4;
    owner.m_stackSlots = 4;
    owner.m_nodeSpaces.Init(4);
    owner.m_lensFlareSpaces.Init(4);
    GmNameMap flares;
    flares.Init(2);
    flares.Insert("Glare", &g_flare);
    owner.m_lensFlares.Add(&flares);
    Emitter emitter;
    emitter.SetMatrices((AtMatrix*)new Mat(), (AtMatrix*)new Mat());
    sounds.m_nodeSpaces.Init(2);
    sounds.m_secondSpaces.Init(2);
    GmNameMap emitters;
    emitters.Init(2);
    emitters.Insert("Voice", &emitter);
    sounds.m_nodes.Add(&emitters);
    GmNameMap segments;
    segments.Init(3);
    segments.Insert("SegA", &g_segA);
    segments.Insert("SegB", &g_segB);
    sounds.m_second.Add(&segments);

    std::string mask = "Mask { ";
    for (int i = 0; i < 32; i++) mask += std::to_string(i) + " ";
    mask += "}";
    const std::string text =
        "RotTransCoordSys[12]\nNode[4]\n"
        "ModelActor[3]\n{\n"
        " ModelActor \"MA\" RotTransCoordSys\n {\n  DrawingEnable\n  CollisionEnable\n }\n"
        " ModelActor \"MB\" RotTransCoordSys\n {\n  DrawingEnable\n }\n"
        " ModelActor \"MC\" RotTransCoordSys\n {\n  DrawingEnable\n  LineSegIntersectEnable\n }\n}\n"
        "WaterActor[1]\n{\n WaterActor \"Pond\" RotTransCoordSys\n {\n  DrawingEnable\n  PlaybackRate 30.0\n  DampingFactor 0.5\n  GenerationFactor 0.25\n  MatAssignmentOffset 3\n"
        "  WaterPatch [2]\n  {\n   WaterPatch { PositionRect 1.0 2.0 3.0 4.0 Neighbors 1 -1 2 -1 }\n   WaterPatch { TxtCoordRect 0.5 0.5 1.0 1.0 " + mask + " }\n  }\n }\n}\n"
        "LensFlareActor[1]\n{\n LensFlareActor \"Sun\" RotTransCoordSys\n {\n  LensFlare \"Glare\" \"\"\n  LineOfSightTestEnable\n  DrawingEnable\n }\n}\n"
        "GroupActor[1]\n{\n GroupActor \"Herd\" RotTransCoordSys\n {\n  DrawingEnable\n  Actor [1]\n  {\n   \"MA\" \"\"\n  }\n }\n}\n"
        "SwitchGroupActor[1]\n{\n SwitchGroupActor \"Lamp\" RotTransCoordSys\n {\n  Switch 1\n }\n}\n"
        "LodGroupActor[1]\n{\n LodGroupActor \"Tree\" RotTransCoordSys\n {\n  CollisionEnable\n  Lod [2]\n  {\n   \"MB\" \"\" 20.0\n   \"MC\" \"\" 5.0\n  }\n }\n}\n"
        "SoundNode[1]\n{\n SoundNode \"Bell\" RotTransCoordSys\n {\n  Position 1.0 2.0 3.0\n  Emitter \"Voice\" \"\"\n  PlaySegment \"SegA\" \"\" 4\n  PlaySegment \"SegB\" \"\" 7\n  LineSegIntersectEnable\n }\n}\n";

    {
        AtScene scene;
        read(scene, owner, text);
        CHECK(scene.m_waterCount == 1 && scene.m_lensFlareCount == 1 && scene.m_groupCount == 1 && scene.m_switchGroupCount == 1 && scene.m_lodGroupCount == 1 && scene.m_soundCount == 1);
        AtSceneModelActor* ma = (AtSceneModelActor*)scene.Find("MA");
        AtSceneModelActor* mb = (AtSceneModelActor*)scene.Find("MB");
        AtSceneModelActor* mc = (AtSceneModelActor*)scene.Find("MC");

        AtSceneWaterActor* pond = (AtSceneWaterActor*)scene.Find("Pond");
        CHECK(pond->m_class == 9);
        CHECK(drawers.m_alive == 1);
        CHECK(pond->m_drawer == drawers.m_last);
        CHECK(drawers.m_last->m_count == 2 && pond->m_patchCount == 2);
        CHECK(drawers.m_last->m_rate == 30.0f && drawers.m_last->m_damping == 0.5f && drawers.m_last->m_generation == 0.25f);
        CHECK(pond->m_matAssignmentOffset == 3);
        if (drawers.m_last->m_patches.size() == 2) {
            const AtSceneWaterPatch& a = drawers.m_last->m_patches[0];
            const AtSceneWaterPatch& b = drawers.m_last->m_patches[1];
            CHECK(a.m_position[3] == 4.0f && a.m_neighbors[1] == -1 && a.m_neighbors[2] == 2 && a.m_mask[31] == -1 && a.m_textureCoords[0] == 0.0f);
            CHECK(b.m_textureCoords[2] == 1.0f && b.m_neighbors[0] == -1 && b.m_mask[0] == 0 && b.m_mask[31] == 31 && b.m_position[0] == 0.0f);
        }

        AtSceneLensFlareActor* sun = (AtSceneLensFlareActor*)scene.Find("Sun");
        CHECK(sun->m_class == 10);
        CHECK((void*)sun->m_flare == &g_flare);
        CHECK(sun->m_flags & AtSceneLensFlareActor::kLineOfSightTest);
        CHECK(sun->m_sphere.radius == 0.0001f);

        AtSceneGroupActor* herd = (AtSceneGroupActor*)scene.Find("Herd");
        CHECK(herd->m_class == 11 && herd->m_count == 1 && herd->m_capacity == 1);
        CHECK(herd->m_children[0] == ma);
        CHECK(ma->m_sharedFrom == herd && ma->m_coordSys == herd->m_coordSys);
        CHECK((void*)herd->m_readOwner == &owner);

        AtSceneSwitchGroupActor* lamp = (AtSceneSwitchGroupActor*)scene.Find("Lamp");
        CHECK(lamp->m_class == 12 && lamp->m_switch == 1 && lamp->m_count == 0);

        AtSceneLodGroupActor* tree = (AtSceneLodGroupActor*)scene.Find("Tree");
        CHECK(tree->m_class == 13 && tree->m_count == 2);
        CHECK(tree->m_children[0] == mc && tree->m_children[1] == mb);
        CHECK(tree->m_distances[0] == 25.0f && tree->m_distances[1] == 400.0f);
        CHECK(mb->m_sharedFrom == tree && mc->m_sharedFrom == tree);

        AtSceneSoundNode* bell = (AtSceneSoundNode*)scene.Find("Bell");
        CHECK(bell->m_class == 16);
        CHECK(bell->m_emitter == &emitter);
        CHECK(g_played.size() == 2 && g_played[0] == "B0" && g_played[1] == "A0");
        CHECK(g_velocity.x == 0.0f && g_velocity.y == 0.0f && g_velocity.z == 0.0f);
        CHECK(((Mat*)emitter.m_world)->position[1] == 2.0f);
        CHECK(AtSceneSoundNode::s_pending == nullptr);
        CHECK(bell->m_flags & 0x800);

        // Into a space graph and out, directly and through a list that holds the scene.
        AtSpaceGraph graph;
        read_graph(graph, owner, "SpaceGraph\n{\nLeafSpace[1]\nNodeLink[32]\nNodeIncidence[96]\nRoot LeafSpace 0\nLeafSpace[1]\n{\n LeafSpace { BoundingBox -100.0 100.0 -100.0 100.0 -100.0 100.0 }\n}\n}\n");
        scene.InsertInto(&graph);
        CHECK(ma->m_links[2] == nullptr);  // Sharing the group's coordinate system.
        CHECK(mb->m_links[2] == nullptr && mc->m_links[3] == nullptr);
        CHECK(pond->m_links[2] != nullptr && sun->m_links[2] != nullptr && herd->m_links[2] != nullptr);
        CHECK(lamp->m_links[2] == nullptr);
        CHECK(tree->m_links[1] != nullptr && tree->m_links[2] == nullptr);
        CHECK(bell->m_links[3] == nullptr && bell->m_links[2] == nullptr);  // Sound nodes are in no walk.
        scene.RemoveFrom(&graph);
        CHECK(pond->m_links[2] == nullptr && sun->m_links[2] == nullptr && tree->m_links[1] == nullptr);
        {
            AtSceneDatabaseLoader loader;
            loader.m_nodes = &scene;
            loader.m_nodeCount = 1;
            loader.InsertInto(&graph);
            CHECK(herd->m_links[2] != nullptr && tree->m_links[1] != nullptr);
            loader.RemoveFrom(&graph);
            CHECK(herd->m_links[2] == nullptr && tree->m_links[1] == nullptr);
            loader.m_nodes = nullptr;
            loader.m_nodeCount = 0;
        }
        scene.InsertInto(&graph);

        // Clearing gives back the drawer and the children's coordinate systems.
        scene.Clear();
        CHECK(drawers.m_alive == 0);
        graph.Clear();
    }

    // A level-of-detail group's place for each child, directly.
    {
        AtSceneLodGroupActor lod;
        AtSceneModelActor kids[4];
        lod.m_capacity = 4;
        lod.m_children = new AtSceneActor*[4];
        lod.m_distances = new float[4];
        const float d[4] = {9.0f, 1.0f, 4.0f, 1.0f};
        for (int i = 0; i < 4; i++) lod.Insert(&kids[i], d[i]);
        CHECK(lod.m_count == 4);
        CHECK(lod.m_children[0] == &kids[3] && lod.m_children[1] == &kids[1] && lod.m_children[2] == &kids[2] && lod.m_children[3] == &kids[0]);
        for (int i = 0; i < 4; i++) CHECK(kids[i].m_sharedFrom == &lod);
        lod.m_count = 0;
        delete[] lod.m_children;
        delete[] lod.m_distances;
        lod.m_children = nullptr;
        lod.m_distances = nullptr;
    }

    std::printf("%s\n", g_fail ? "FAILED" : "ok");
    return g_fail ? 1 : 0;
}
