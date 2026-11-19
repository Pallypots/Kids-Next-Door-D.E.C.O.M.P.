// Host test of ParticleSystemActor and the random words, through the real tokenizer and scene, on any pointer size. Not evidence of a byte match. No game data: the text is written here, in the shape real files have, and the emitter is a stand-in.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtRandom.h"
#include "at/AtSceneActor.h"
#include "at/AtSceneParticle.h"

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
void SetTranslation(AtMatrix*, const AtVector3*) {}
void SetRotation(AtMatrix*, const AtQuaternion*) {}
void SetLookAt(AtMatrix*, const AtVector3*, const AtVector3*) {}
void SetScale(AtMatrix*, const AtVector3*) {}
void SetInverse(AtMatrix*, const AtMatrix*) {}
void Multiply(AtMatrix*, const AtMatrix*) {}
void SetProduct(AtMatrix* to, const AtMatrix* a, const AtMatrix*) { *(Mat*)to = *(const Mat*)a; }
void TransformPoint(const AtMatrix*, const AtVector3* from, AtVector3* to) { *to = *from; }

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
    void Slot12() override {}
    AtMatrixDevice* MatrixDevice() override { return &m_matrices; }
};

class Drawer : public AtSceneParticleDrawer {
public:
    AtEmitter* m_emitter = nullptr;
    void Slot0() override {}
    void Start(AtNameTableOwner* owner, AtEmitter* emitter) override { m_owner = owner; m_emitter = emitter; }
};

class DrawerMaker : public AtSceneParticleDrawerMaker {
public:
    int m_made = 0;
    int m_released = 0;
    AtSceneParticleDrawer* New() override { m_made++; return new Drawer; }
    void Release(AtSceneParticleDrawer* drawer) override { m_released++; delete (Drawer*)drawer; }
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

}  // namespace

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
    AtSceneActor::SetHeap(&g_heap);
    AtSceneParticleSystemActor::SetHeap(&g_heap);
    AtSceneParticleLayer::SetHeap(&g_heap);
    AtSceneAnimSet::SetHeap(&g_heap);
    AtSceneAnimBlend::SetHeap(&g_heap);
    AtCoordSysDatabase::SetHeap(&g_heap);
    AtIoStream::s_heap = &g_heap;
    AtIoTokenFile::s_heap = &g_heap;

    // The table is 11-bit words. The first and the wrap are the formula's own; the oracle has the rest against the XBE.
    {
        unsigned int state = 0x0332F1A2;
        unsigned int high = 0;
        for (unsigned int i = 0; i < 0x800; i++) {
            CHECK(AtRandomWord(i) == ((state >> 16) & 0x7FF));
            high |= AtRandomWord(i);
            state = state * 214013 + 2531011;
        }
        CHECK(high == 0x7FF);
        g_atRandomCursor = 0x7FF;
        CHECK(AtNextRandomWord() == AtRandomWord(0));
        CHECK(g_atRandomCursor == 0);
        CHECK(AtNextRandomWord() == AtRandomWord(1));
        g_atRandomCursor = 0;
    }

    Device device;
    DrawerMaker drawers;
    AtNameTableOwner* owner = new (std::calloc(1, sizeof(AtNameTableOwner))) AtNameTableOwner;
    owner->m_device = &device;
    owner->m_particleDrawers = &drawers;
    owner->m_nodeSpaces.Init(4);
    owner->m_modifierSpaces.Init(4);
    owner->m_geometrySpaces.Init(4);
    owner->m_emitterSpaces.Init(4);
    device.m_meshSpaces.Init(4);
    device.m_matAssignmentSpaces.Init(4);
    device.m_matAnimationSpaces.Init(4);

    alignas(16) static unsigned char room[2][sizeof(AtEmitter)];  // Zeroed, unconstructed: only what a test sets.
    AtEmitter& plain = *(AtEmitter*)room[0];
    plain.m_blurCount = 0;
    plain.m_maxParticles = 3;
    plain.m_boundingSphere.center.x = 1.0f;
    plain.m_boundingSphere.radius = 300.0f;
    AtEmitter& blurred = *(AtEmitter*)room[1];
    blurred.m_blurCount = 2;
    blurred.m_blurStep = 0.5f;
    blurred.m_maxParticles = 1;
    blurred.m_blur.x = 10.0f;
    blurred.m_blurLinear.y = 4.0f;
    blurred.m_blurQuadratic.z = 8.0f;
    blurred.m_blurQuadratic.w = 1.0f;
    blurred.m_boundingSphere.radius = 7.0f;
    GmNameMap emitters, spaced;
    emitters.Init(8);
    spaced.Init(8);
    emitters.Insert("Plain", &plain);
    spaced.Insert("Blur", &blurred);
    owner->m_emitters.Add(&emitters);
    owner->m_emitterSpaces.Add("Space", &spaced);

    {
        AtScene scene;
        read(scene, *owner,
             "RotTransCoordSys[2]\nMotionState[2]\nNode[2]\n"
             "ParticleSystemActor[2]\n{\n"
             " ParticleSystemActor \"One\" RotTransCoordSys MotionState\n {\n  BoundingSphere 0.0 0.0 0.0 5.0\n  DrawingDisable\n  Emitter \"Plain\" \"\"\n  WorldAdditiveInitialVelocity 1.0 2.0 3.0\n  WorldAdditiveAcceleration 2.0 4.0 8.0\n  UpdateAtDraw\n }\n"
             " ParticleSystemActor \"Two\" RotTransCoordSys MotionState\n {\n  Emitter \"Blur\" \"Space\"\n  PauseParticles\n }\n"
             "}\n");
        CHECK(scene.m_particleSystemCount == 2);
        AtSceneParticleSystemActor* one = (AtSceneParticleSystemActor*)scene.Find("One");
        AtSceneParticleSystemActor* two = (AtSceneParticleSystemActor*)scene.Find("Two");
        CHECK(one->m_class == 8);
        CHECK(one->m_emitter == &plain);
        CHECK(one->m_state == AtSceneParticleSystemActor::kPlaying);
        CHECK(one->m_flags & AtSceneParticleSystemActor::kUpdateAtDraw);
        CHECK((one->m_flags & AtSceneActor::kDrawing) == 0);
        CHECK(one->m_worldInitialVelocity.z == 3.0f);
        CHECK(one->m_worldAcceleration.y == 4.0f);
        CHECK(one->m_halfWorldAcceleration.z == 4.0f);
        // The file's sphere is lost to the emitter's, and the world's is the local one until something moves it.
        CHECK(one->m_sphere.radius == 300.0f);
        CHECK(one->m_sphere.center.x == 1.0f);
        CHECK(one->m_worldSphere.radius == 300.0f);
        CHECK(one->m_worldSphere.center.x == 1.0f);
        CHECK(one->m_seed == AtRandomWord(1));
        CHECK(one->m_unknownB0 == 1.0f);
        CHECK(one->m_layerCount == 1);
        const AtSceneParticleLayer& only = one->m_layers[0];
        CHECK(only.m_count == 4);
        CHECK(only.m_unknown0C == 3);
        CHECK(only.m_unknown10 == 3);
        CHECK(only.m_color.w == 0.0f);
        CHECK(only.m_emitter == &plain);
        CHECK(only.m_particles[3].m_words[15] == 0);
        CHECK(only.m_drawer->m_owner == owner);
        CHECK(((Drawer*)only.m_drawer)->m_emitter == &plain);

        CHECK(two->m_emitter == &blurred);
        CHECK(two->m_state == AtSceneParticleSystemActor::kPausedState);
        CHECK(two->m_flags & AtSceneParticleSystemActor::kPaused);
        CHECK(two->m_seed == AtRandomWord(2));
        CHECK(two->m_layerCount == 3);
        CHECK(two->m_layers[0].m_color.x == 0.0f);
        // At time 0.5: 10 and nothing, 4 * 0.5, 8 * 0.25, 1 * 0.25. At time 1: 10, 4, 8, 1.
        CHECK(two->m_layers[1].m_color.x == 10.0f);
        CHECK(two->m_layers[1].m_color.y == 2.0f);
        CHECK(two->m_layers[1].m_color.z == 2.0f);
        CHECK(two->m_layers[1].m_color.w == 0.25f);
        CHECK(two->m_layers[2].m_color.x == 10.0f);
        CHECK(two->m_layers[2].m_color.y == 4.0f);
        CHECK(two->m_layers[2].m_color.z == 8.0f);
        CHECK(two->m_layers[2].m_color.w == 1.0f);
        CHECK(two->m_layers[2].m_count == 2);
        CHECK(drawers.m_made == 4);
        CHECK(drawers.m_released == 0);

        scene.Clear();
        CHECK(drawers.m_released == 4);
        CHECK(scene.m_particleSystems == nullptr);
    }

    {
        // Clear on one alone: it can be read and cleared again, and +0xF4 is left as it was.
        AtSceneParticleSystemActor actor;
        actor.m_owner = owner;
        actor.m_unknownF4 = 77;
        actor.m_seed = 1;
        actor.m_unknownB0 = 2.0f;
        actor.Clear();
        CHECK(actor.m_seed == 0);
        CHECK(actor.m_unknownB0 == 1.0f);
        CHECK(actor.m_unknownF4 == 77);
        CHECK(actor.m_layers == nullptr);
    }

    if (g_fail == 0) std::printf("test_particle: all passed\n");
    return g_fail ? 1 : 0;
}
