// Host test of the .ASE loader through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "at/AtEmitter.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMaterial.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)
#define NEAR(a, b) (std::fabs((double)(a) - (double)(b)) < 1e-4)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

struct Text {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    explicit Text(char* text) {
        stream.m_base = stream.m_cur = text;
        stream.m_limit = text + std::strlen(text);
        stream.m_size = (unsigned int)std::strlen(text);
        tokenizer.SetStream(&stream);
    }
    ~Text() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

char g_text[] =
    "NameSpace \"Fx\"\n"
    "Emitter[2]\n"
    "{\n"
    "  Emitter \"First\"\n"
    "  {\n"
    "    MatAssignment[1]\n"
    "    {\n"
    "      \"Mat\"\n"
    "    }\n"
    "    EmissionLine 1.0 2.0 3.0 4.0 5.0 6.0\n"
    "    EmissionCone 0.0 3.0 4.0 10.0\n"
    "    EmissionConeInnerAngle 2.0\n"
    "    MaxEmissionEventRate 1000.0\n"
    "    EmissionEventChance 0.25\n"
    "    MaxParticlesPerEvent 2\n"
    "    ParticleChance 2.0\n"
    "    LocalAcceleration 2.0 4.0 6.0\n"
    "    LinearSquareSizeKeys 1.0 3.0 2.0\n"
    "    LinearColorChangeKeys 0.0 0.0 0.0 1.0 1.0 1.0 1.0 1.0 2.0 2.0 2.0 2.0\n"
    "    ParticleLifetime 3.0\n"
    "    MaxParticles 4\n"
    "    LineSegIntersectTesting\n"
    "    LocalBoundingSphere 1.0 2.0 3.0 9.0\n"
    "  }\n"
    "  Emitter \"Second\"\n"
    "  {\n"
    "    QuadraticRectSizeKeys 1.0 1.0 3.0 5.0 1.0 2.0 2.0 2.0 0.5 0.5\n"
    "    MotionBlur 2 1.0 0.0 0.0 0.0 0.0\n"
    "    Opaque\n"
    "    EulerUpdate\n"
    "  }\n"
    "}\n";

void read_emitters() {
    AtEmitterDatabase::SetHeap(&g_heap);
    AtQueryPool::SetHeap(&g_heap);
    AtMatAssignment::SetHeap(&g_heap);
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::SetHeap(&g_heap);

    int material = 5;
    AtMaterialOwner* materials = new AtMaterialOwner;
    GmNameMap names;
    names.Init(4);
    names.Insert("Mat", &material);
    materials->m_materials.Add(&names);

    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_device = (AtSceneDevice*)materials;
    owner->m_emitterSpaces.Init(4);

    Text text(g_text);
    AtEmitterDatabase* database = new AtEmitterDatabase;
    database->Read(owner, &text.tokenizer);

    CHECK(database->m_count == 2);
    CHECK(owner->m_emitterSpaces.Find("Fx", "Second") == &database->m_emitters[1]);
    const AtEmitter& first = *(const AtEmitter*)database->Find("First");
    const AtEmitter& second = database->m_emitters[1];
    CHECK(first.m_owner == owner);
    CHECK(first.m_materials.m_count == 1);
    CHECK(first.m_materials.m_items[0] == &material);
    CHECK(first.m_materialCount == 1);
    CHECK(first.m_flags == (AtEmitter::kEmissionLine | AtEmitter::kSizeKeys | AtEmitter::kSquareParticles | AtEmitter::kLinearSize | AtEmitter::kColorKeys | AtEmitter::kLinearColor | AtEmitter::kLineSegIntersectTesting | AtEmitter::kHasBoundingSphere));
    CHECK(NEAR(first.m_emission[1].z, 6.0));
    // Normalized when the body ends.
    CHECK(NEAR(first.m_coneDirection.y, 0.6));
    CHECK(NEAR(first.m_coneAngleStep, 8.0 / 2047.0));
    // Held at its shortest.
    CHECK(NEAR(first.m_eventInterval, 0.004));
    CHECK(first.m_eventChance == (int)(0.75f * 2047.0f));
    // Over 1 means it always happens.
    CHECK(first.m_particleChance == 0);
    CHECK(NEAR(first.m_halfAcceleration.z, 3.0));
    CHECK(NEAR(first.m_sizeLinear[0], 1.0));
    CHECK(NEAR(first.m_sizeLinear[1], 1.0));
    CHECK(NEAR(first.m_color.x, 0.0));
    CHECK(NEAR(first.m_colorEnd.x, 255.0));
    CHECK(NEAR(first.m_colorLinear.x, 127.5));
    CHECK(NEAR(first.m_lifetimeOverMaterials, 3.0));
    // 3, a line's 2, and the cone's 4.
    CHECK(first.m_streamCount == 3 + 2 + 4);
    CHECK(first.m_coneMatrix[8] == first.m_coneDirection.z);
    CHECK(first.m_boundingSphere.radius == 9.0f);

    CHECK(first.m_queries.m_count == 4);
    CHECK(first.m_queries.m_perQuery == 1);
    CHECK(first.m_queries.m_made == 1);
    CHECK(first.m_queries.m_segmentQueries != nullptr);
    const AtSegmentQuery& query = first.m_queries.m_segmentQueries[3];
    CHECK(query.m_capacity == 1);
    CHECK(query.m_flags == AtSegmentHitList::kNotOwned);
    CHECK(query.m_keys == first.m_queries.m_entries8 + 3 * 8);
    CHECK(query.m_hits == first.m_queries.m_entries2C + 3 * 0x2C);
    CHECK((unsigned char*)query.m_records == first.m_queries.m_entries10 + 3 * sizeof(AtSegmentHit));
    CHECK(query.m_second0 == &query.m_vectors0[1]);

    CHECK((second.m_flags & AtEmitter::kSquareParticles) == 0);
    CHECK(second.m_flags & AtEmitter::kQuadraticSize);
    CHECK(second.m_flags & AtEmitter::kOpaque);
    CHECK(second.m_flags & AtEmitter::kEulerUpdate);
    CHECK(second.m_flags & AtEmitter::kMotionBlur);
    CHECK(NEAR(second.m_blurStep, 0.5));
    CHECK(second.m_queries.m_made == 0);
    CHECK(second.m_queries.m_segmentQueries == nullptr);
    // No words for these: as Reset leaves them.
    CHECK(second.m_maxParticles == 0x80);
    CHECK(second.m_coneMatrix[0] == 1.0f);
    CHECK(second.m_streamCount == 2);
    // A quadratic through (0, 1), (0.5, 2) and (1, 3) is a line: the fitted quadratic part is nothing, the linear part 2.
    CHECK(NEAR(second.m_sizeQuadratic[0], 0.0));
    CHECK(NEAR(second.m_sizeLinear[0], 2.0));

    database->Clear();
    CHECK(database->m_emitters == nullptr);
    CHECK(database->m_count == 0);
    delete database;
    names.Clear();
    delete owner;
    delete materials;
}

void axes() {
    AtVector3 a, b, out;
    a.x = 1.0f; a.y = 0; a.z = 0;
    b.x = 0; b.y = 2.0f; b.z = 0;
    out.SetCrossNormalized(a, b);
    CHECK(out.x == 0.0f);
    CHECK(out.y == 0.0f);
    CHECK(out.z == 1.0f);
    out.SetCrossNormalized(a, a);
    CHECK(out.x == 1.0f);
    CHECK(out.y == 0.0f);
    CHECK(out.z == 0.0f);

    AtVector2 flat;
    flat.x = 3.0f;
    flat.y = 4.0f;
    flat.Normalize();
    CHECK(NEAR(flat.x, 0.6));
    CHECK(NEAR(flat.y, 0.8));
    flat.x = 1e-7f;
    flat.y = 0;
    flat.Normalize();
    CHECK(flat.x == 1.0f);
    CHECK(flat.y == 0.0f);
}

}  // namespace

int main() {
    read_emitters();
    axes();
    if (g_fail) {
        std::printf("test_ase: %d failed\n", g_fail);
        return 1;
    }
    std::printf("test_ase: ok\n");
    return 0;
}
