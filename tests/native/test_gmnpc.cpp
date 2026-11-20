// Host test of GmAcNPC, FaAcNPC and their helpers, on any pointer size. Not evidence of a byte match. No file makes an NPC yet, so these are constructed objects and direct calls.
// Reading a record reaches the registry's search, a seam: with "--seam" the test does that, and checks from outside that it stopped.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gm/GmAcNPC.h"
#include "gm/GmLevelManifest.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

std::string g_log;

// What CombatOf reads: FaCmCombat's words behind a vftable pointer, on GmCmCombat (the declared type of the character's +0x2E4), so that the layout is the same on any host.
class TestCombat : public GmCmCombat {
public:
    ~TestCombat() override { g_log += "combat-deleted;"; }
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Stop() override { g_log += "combat-stop;"; }
    void Slot5() override { g_log += "combat-5;"; }
    void Update(int) override {}
    int Slot7() override { return 0; }

    char m_unknown04[0x7C];
    GmAcStat* m_health = nullptr;
    char m_unknown84[0x10];
    int m_threat = 0;
    int m_attackRange = -1;
    char m_unknown9C[0x48];
};

class TestNPC : public FaAcNPC {
public:
    std::string m_posted;
    void Slot49(const char* event, int b, int c) override { m_posted.assign(event, strnlen(event, 8)); m_posted += ":" + std::to_string(b) + std::to_string(c); }
};

struct Range {
    AtDataComponent* Value() { return (AtDataComponent*)this; }
    void* m_vftable;
    AtDataDescriptor* m_descriptor;
};

void test_lifetimes() {
    {
        GmAcNPC n;
        CHECK(n.m_aiActions == nullptr);
        CHECK((n.m_flags98 & GmAcNPC::kNPCReset) != 0);
        CHECK((n.m_flags98 & GmAcCharacter::kCharacterReset) != 0);
        CHECK(n.TestCondition("cDead") == 0);
        n.Shutdown();
        CHECK(n.m_aiActions == nullptr);
    }
    {
        FaAcNPC f;
        CHECK((f.m_flags98 & FaAcNPC::kFaNPCReset) != 0);
        CHECK(f.m_lastHealth > 3.4e38f);
        CHECK(f.m_lowHealthRange == nullptr && f.m_numAIHealthLevels == nullptr && f.m_unknown31C == 0);
        TestCombat* combat = new TestCombat;
        f.m_combat = combat;
        g_log.clear();
        f.Shutdown();  // GmAcCharacter's shutdown stops and deletes the combat object, then FaAcNPC's reset clears it
        CHECK(g_log == "combat-stop;combat-deleted;");
        CHECK(f.m_combat == nullptr);
    }
}

void test_conditions() {
    TestNPC n;
    TestCombat combat;
    GmAcStat health = {};
    health.m_min = 0.0f;
    health.m_max = 100.0f;
    health.m_value = 30.0f;
    combat.m_health = &health;
    n.m_combat = &combat;
    AtDataDescriptor low, med, high;
    low.m_realFloor = 0.0f; low.m_realCeiling = 0.25f;
    med.m_realFloor = 0.25f; med.m_realCeiling = 0.5f;
    high.m_realFloor = 0.5f; high.m_realCeiling = 1.0f;
    Range ranges[3] = {{nullptr, &low}, {nullptr, &med}, {nullptr, &high}};
    n.m_lowHealthRange = ranges[0].Value();
    n.m_medHealthRange = ranges[1].Value();
    n.m_highHealthRange = ranges[2].Value();
    CHECK(n.TestCondition("cDead") == 0);
    CHECK(n.TestCondition("cAlive") == 1);
    CHECK(n.TestCondition("cLowHP") == 0);
    CHECK(n.TestCondition("cMedHP") == 1);
    CHECK(n.TestCondition("cHighHP") == 0);
    health.m_value = 25.0f;  // the lower bound is open, the upper closed
    CHECK(n.TestCondition("cLowHP") == 1);
    CHECK(n.TestCondition("cMedHP") == 0);
    health.m_value = 0.0f;
    CHECK(n.TestCondition("cDead") == 1);
    CHECK(n.TestCondition("cAlive") == 0);
    CHECK(n.TestCondition("cLowHP") == 0);
    CHECK(n.TestCondition("cThreat") == 0);
    CHECK(n.TestCondition("cNThreat") == 1);
    combat.m_threat = 1;
    CHECK(n.TestCondition("cThreat") == 1);
    CHECK(n.TestCondition("cNThreatZ") == 0);  // compared as eight-byte names
    CHECK(n.TestCondition("cAtkRng") == 0);
    CHECK(n.TestCondition("cNAtkRng") == 1);
    combat.m_attackRange = 2;
    CHECK(n.TestCondition("cAtkRng") == 1);
    CHECK(n.TestCondition("cAtkRg2") == 1);
    CHECK(n.TestCondition("cAtkRg3") == 0);
    CHECK(n.TestCondition("cNAtkRg2") == 0);
    CHECK(n.TestCondition("cNAtkRg5") == 1);
    CHECK(n.TestCondition("cTarget") == 0);
    CHECK(n.TestCondition("cNTarget") == 1);
    CHECK(n.TestCondition("unknown") == 0);
    // No stats holder: the shield conditions give the base's 0.
    CHECK(n.TestCondition("cShield") == 0);
    CHECK(n.TestCondition("cNShield") == 0);
    GmAcStat shield = {};
    std::memcpy(shield.m_name, "sShield", 7);
    shield.m_value = 3.0f;
    GmAcStat* list[1] = {&shield};
    GmAcStatSet set = {nullptr, 1, list};
    GmAcStatsHolder holder = {nullptr, &set};
    n.m_unknown270 = (GmAcOwned*)&holder;
    CHECK(n.TestCondition("cShield") == 1);
    CHECK(n.TestCondition("cNShield") == 0);
    shield.m_value = 0.0f;
    CHECK(n.TestCondition("cNShield") == 1);
    n.m_unknown270 = nullptr;
    // The player list: no target, and no player, are the same.
    GmShadowCasters loader = {};
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)&loader;
    CHECK(n.TestCondition("cTgtPlr") == 1);
    CHECK(n.TestCondition("cNTgtPlr") == 0);
    n.m_combat = nullptr;
    CHECK(n.TestCondition("cAlive") == 1);
    CHECK(n.TestCondition("cNThreat") == 1);
    CHECK(n.TestCondition("cAtkRg0") == 0);
    CHECK(n.TestCondition("cNAtkRg0") == 1);
}

void test_health_levels() {
    TestNPC n;
    TestCombat combat;
    GmAcStat health = {};
    health.m_max = 100.0f;
    health.m_value = 40.0f;
    combat.m_health = &health;
    n.m_combat = &combat;
    n.m_flagsA4 = 2;  // slot 27 says so
    n.UpdateHealthLevels();  // no levels: only the last value changes
    CHECK(n.m_lastHealth == 40.0f);
    health.m_value = 50.0f;
    n.UpdateHealthLevels();  // a rise: nothing but the last value
    CHECK(n.m_lastHealth == 50.0f);
    combat.m_health = nullptr;
    n.m_lastHealth = 7.0f;
    n.UpdateHealthLevels();
    CHECK(n.m_lastHealth == 7.0f);
    CHECK(n.m_posted.empty());
    n.m_combat = nullptr;
}

void test_helpers() {
    CHECK(AtCompareN("cAtkRg3", "cAtkRg", 6) == 0);
    CHECK(AtCompareN("cAtkRng", "cAtkRg", 6) != 0);
    CHECK(AtCompareN(nullptr, "", 4) == 0);
    CHECK(AtCompareN("\x80", "a", 1) < 0);  // signed characters
    CHECK(AtCompareN("abc", "abd", 2) == 0);
    GmAcStat s = {};
    s.m_min = 10.0f;
    s.m_max = 110.0f;
    AtDataDescriptor d;
    d.m_realFloor = 0.5f;
    d.m_realCeiling = 1.0f;
    Range r = {nullptr, &d};
    s.m_value = 60.0f;
    CHECK(GmStatInRange(&s, r.Value()) == 0);
    s.m_value = 60.5f;
    CHECK(GmStatInRange(&s, r.Value()) == 1);
    s.m_value = 110.0f;
    CHECK(GmStatInRange(&s, r.Value()) == 1);
    CHECK(GmStatInRange(nullptr, r.Value()) == 0);
    CHECK(GmStatInRange(&s, nullptr) == 0);
    GmAcCharacter player;
    GmAcCharController controller = {};
    player.m_controller = &controller;
    GmControlledCharacters list = {};
    list.m_first = &player;
    CHECK(GmNthControlledCharacter(&list, 0) == nullptr);  // a count of 0
    list.m_count = 1;
    CHECK(GmNthControlledCharacter(&list, 0) == &player);
    CHECK(GmNthControlledCharacter(&list, 1) == nullptr);
    player.m_controller = nullptr;
    CHECK(GmNthControlledCharacter(&list, 0) == nullptr);
    float* words = (float*)player.m_unknown14C;
    words[2] = 0.0f;
    CHECK(GmAcFloat154NotZero(&player) == 0);
    words[2] = -0.0f;
    CHECK(GmAcFloat154NotZero(&player) == 0);
    words[2] = 2.0f;
    CHECK(GmAcFloat154NotZero(&player) == 1);
    GmAcNPC n;
    CHECK(GmAcTargetOf(&n) == nullptr);
}

void test_factories() {
    static int heap;
    GmAcActor::s_heap = &heap;
    GmAcNPC* one = GmAcNPC::Create(1);
    CHECK(one != nullptr && (one->m_flags98 & GmAcNPC::kNPCReset) != 0);
    delete one;
    FaAcNPC* many = FaAcNPC::Create(3);
    CHECK(many != nullptr && many[2].m_lastHealth > 3.4e38f);
    delete[] many;
}

void seam() {
    FaAcNPC f;
    f.Slot2();  // the ranges are read through the registry's search, a seam: stops
    std::printf("the seam came back\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--seam") == 0) {
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);  // no error-report dialog in the child
#endif
        seam();
        return 0;
    }
    test_lifetimes();
    test_conditions();
    test_health_levels();
    test_helpers();
    test_factories();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int status = std::system(command.c_str());
    CHECK(status != 0);
    std::printf("%s: %d failures\n", g_fail ? "FAILED" : "passed", g_fail);
    return g_fail ? 1 : 0;
}
