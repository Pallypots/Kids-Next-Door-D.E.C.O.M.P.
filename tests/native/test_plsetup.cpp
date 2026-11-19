// Host test of the player's set-up pieces and the combat modules, on any pointer size. Not evidence of a byte match. The registry's search by path (0x000679E0) and the combat update are seams,
// so these are the paths that do not reach them; with "--seam" the test calls the update, and checks from outside that it stopped.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gm/FaCmCombat.h"
#include "gm/GmAcPlayer.h"
#include "gm/GmLevelManifest.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

std::string g_log;

class TestWeapon : public GmCmWeapon {
public:
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Release() override { g_log += "release;"; }
};

class TestMaterials : public AtMaterialOwner {
public:
    int Slot21() override { g_log += "21;"; return 0x2121; }
};

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

void test_combat() {
    static int heap;
    FaCmCombat::s_heap = &heap;
    FaCmPlayerCombat* c = new FaCmPlayerCombat;
    CHECK(c->m_owner == nullptr && c->m_unknown08 == 1 && c->m_unknown90 == -1 && c->m_unknown98 == -1 && c->m_unknownA4 == -1);
    CHECK(c->m_unknown9C == 1 && c->m_unknownD0 == 1 && c->m_unknownE0 == 1 && c->m_weaponSetList == nullptr && c->m_health == nullptr);
    CHECK(c->Slot7() == 1);
    GmAcStat health = {};
    c->m_health = &health;
    CHECK(c->Slot7() == 0);
    health.m_value = 0.5f;
    CHECK(c->Slot7() == 1);
    health.m_value = std::nanf("");
    CHECK(c->Slot7() == 0);  // a NaN health is dead

    // Two weapon sets, one holding a weapon: slot 5 releases it and lets it go; Stop frees both and resets.
    TestWeapon weapon;
    c->m_weaponSetCount = 2;
    c->m_weaponSetList = (FaCmWeaponSet*)operator new(2 * sizeof(FaCmWeaponSet));
    std::memset(c->m_weaponSetList, 0, 2 * sizeof(FaCmWeaponSet));
    c->m_weaponSetList[1].m_count = 2;
    c->m_weaponSetList[1].m_weapons = (GmCmWeapon**)operator new(2 * sizeof(GmCmWeapon*));
    c->m_weaponSetList[1].m_weapons[0] = nullptr;
    c->m_weaponSetList[1].m_weapons[1] = &weapon;
    c->m_unknown2C = 5;
    c->m_unknown94 = 6;
    c->m_unknown08 = 0;
    c->Slot5();
    CHECK(g_log == "release;" && c->m_weaponSetList[1].m_weapons[1] == nullptr && c->m_unknown2C == 0 && c->m_unknown94 == 0 && c->m_unknown08 == 1);
    c->m_useStartPositionForAttackRange = &health;
    c->m_attackRangeCount = 1;
    c->m_attackRanges = (FaCmAttackRange*)operator new(sizeof(FaCmAttackRange));
    c->Stop();
    CHECK(c->m_weaponSetList == nullptr && c->m_weaponSetCount == 0 && c->m_attackRanges == nullptr && c->m_attackRangeCount == 0 && c->m_health == nullptr);
    CHECK(c->m_useStartPositionForAttackRange == &health);  // Reset leaves it
    delete c;
    GmCmCombat base;
    CHECK(base.ReadRecord() == 0 && base.Slot7() == 1 && base.m_unknown08 == 1);
}

void test_sight() {
    GmAcSightQuery q;
    const AtVector3 at = {1, 2, 3}, way = {0, 1, 0};
    q.Set(&at, &way, 10.0f, 60.0f, 0.0f, 2.0f);
    CHECK(q.m_position.z == 3 && q.m_direction.y == 1 && q.m_unknown1C == 10.0f && q.m_unknown20 == 100.0f && q.m_unknown18 == 4.0f);
    CHECK(near(q.m_unknown24, 0.5f) && near(q.m_unknown28, 1.0f));

    // Given every measure, the character's set-up reads nothing of its own.
    FaAcPlayer p;
    const float length = 7, yaw = 90, pitch = 60, least = 3;
    p.SetupSightQuery(&q, &at, &way, &length, &yaw, &pitch, &least);
    CHECK(q.m_unknown1C == 7.0f && near(q.m_unknown24, 0.0f) && near(q.m_unknown28, 0.5f) && q.m_unknown18 == 9.0f);
    // With none, the record's; with no record, the defaults.
    float recordYaw = 0;
    p.m_sightConeYaw = &recordYaw;
    p.SetupSightQuery(&q, &at, &way, nullptr, nullptr, nullptr, nullptr);
    CHECK(q.m_unknown1C == 1000.0f && near(q.m_unknown24, 1.0f) && near(q.m_unknown28, std::cos(20.0f * 0.0174532924f)) && q.m_unknown18 == 0.0f);
}

void test_lookups() {
    // The registry's search by name: the root, children depth first, then siblings; exact or not.
    AtDataRegistry* registry = (AtDataRegistry*)std::calloc(1, sizeof(AtDataRegistry));
    AtDataLink* a = (AtDataLink*)std::calloc(1, sizeof(AtDataLink));
    AtDataLink* b = (AtDataLink*)std::calloc(1, sizeof(AtDataLink));
    AtDataLink* c = (AtDataLink*)std::calloc(1, sizeof(AtDataLink));
    char na[] = "Level1", nb[] = "SavedTime", nc[] = "Other";
    a->m_name = na;
    b->m_name = nb;
    c->m_name = nc;
    registry->m_root.m_firstChild = a;
    a->m_firstChild = b;
    a->m_sibling = c;
    CHECK(AtDataRegistryFindNamed(registry, "Other", nullptr, 1) == c);
    CHECK(AtDataRegistryFindNamed(registry, "savedtime", nullptr, 0) == b);
    CHECK(AtDataRegistryFindNamed(registry, "savedtime", nullptr, 1) == nullptr);
    CHECK(AtDataRegistryFindNamed(registry, "Level1", c, 1) == nullptr);

    // The sprite: the materials' slot 21, the format, the material by name.
    TestMaterials materials;
    GmSpriteScene scene = {};
    scene.m_materials = &materials;
    GmAcSprite sprite;
    g_log.clear();
    sprite.Setup(&scene);
    CHECK(g_log == "21;" && sprite.m_unknown20 == 0x2121 && sprite.m_scene == &scene && sprite.m_material == nullptr);
}

void seam() {
    FaCmPlayerCombat c;
    c.Update(1);
    std::printf("the seam came back\n");
}

}  // namespace

int main(int argc, char** argv) {
    static int heap;
    GmNameMap::s_heap = &heap;
    GmNameSpaceDb::s_heap = &heap;
    AtSegmentHitList::SetHeap(&heap);
    if (argc > 1 && std::strcmp(argv[1], "--seam") == 0) {
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);  // no error-report dialog in the child
#endif
        seam();
        return 0;
    }
    test_combat();
    test_sight();
    test_lookups();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int status = std::system(command.c_str());
    CHECK(status != 0);
    std::printf("%s: %d failures\n", g_fail ? "FAILED" : "passed", g_fail);
    return g_fail ? 1 : 0;
}
