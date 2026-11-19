// Host test of GmAcObject, GmAcCharacter and the record reader, on any pointer size. Not evidence of a byte match. No file makes a character yet, so these are constructed objects and direct calls.
// The registry's search is a seam, so reading a record stops: with "--seam" the test does that, and checks from outside that it stopped.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gm/GmAcCharacter.h"
#include "gm/GmLevelManifest.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

std::string g_log;

class TestCharacter : public GmAcCharacter {
public:
    std::string m_posted;
    int m_slot49 = 0;
    void Slot49(const char* event, int b, int c) override { m_slot49++; m_posted.assign(event, strnlen(event, 8)); m_posted += ":" + std::to_string(b) + std::to_string(c); }
};

class TestShadow : public GmAcShadow {
public:
    ~TestShadow() override { g_log += "shadow-deleted;"; }
    void Slot1() override {}
    void Start() override { g_log += "start;"; }
    void Stop() override { g_log += "stop;"; }
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Configure(Setup* setup) override { g_log += "configure;"; m_owner = setup->m_owner; }
    void Slot8() override { g_log += "8;"; }
    void Slot9() override {}
    void Slot10() override { g_log += "10;"; }
    void Slot11() override { g_log += "11;"; }
    void Update(int a) override { g_log += "update" + std::to_string(a) + ";"; }
    void Slot13(int a) override { g_log += "13:" + std::to_string(a) + ";"; }
    GmAcCharacter* m_owner = nullptr;
};

class TestSightCone : public GmCmCombat {
public:
    ~TestSightCone() override { g_log += "cone-deleted;"; }
    void Stop() override { g_log += "cone-stop;"; }
    void Slot5() override { g_log += "cone-5;"; }
    void Update(int a) override { g_log += "cone-update" + std::to_string(a) + ";"; }
    int Slot7() override { return 0x77; }
};

GmShadowCasters g_casters;

void test_object() {
    GmAcObject o;
    CHECK(o.m_interaction == nullptr);
    CHECK((o.m_flags98 & GmAcObject::kObjectReset) != 0);
    CHECK((o.m_flags98 & GmAcNode::kActorReset) != 0);
    CHECK(o.Slot2() == 0);  // no record: nothing read
    // Slot 55: 1 without stats, then by the health stat.
    CHECK(o.Slot55() == 1);
    GmAcStat health = {};
    std::memcpy(health.m_name, "sHealth", 7);
    GmAcStat other = {};
    std::memcpy(other.m_name, "sArmor", 6);
    GmAcStat* list[3] = {nullptr, &other, &health};
    GmAcStatSet set = {nullptr, 3, list};
    GmAcStatsHolder holder = {nullptr, &set};
    o.m_unknown270 = (GmAcOwned*)&holder;
    health.m_value = 5.0f;
    CHECK(o.Slot55() == 1);
    health.m_value = 0.0f;
    CHECK(o.Slot55() == 0);
    health.m_value = -1.0f;
    CHECK(o.Slot55() == 0);
    set.m_count = 2;
    CHECK(o.Slot55() == 1);
    CHECK(set.Find("sArmor", 0) == &other && set.Find("none", 0) == nullptr);
    o.m_unknown270 = nullptr;
    // Initialized, then shut down by slot 10's first half: the actor is reset.
    o.Initialize();
    o.m_unknown30[0] = 9;
    o.GmAcObject::Shutdown();
    CHECK(o.m_unknown30[0] == 0 && o.IsInitialized() == 0 && o.m_interaction == nullptr);
}

void test_factory() {
    static int heap;
    GmAcActor::SetHeap(&heap);
    GmAcObject* one = GmAcObject::Create(-1);
    CHECK(one != nullptr && (one->m_flags98 & GmAcObject::kObjectReset) != 0);
    delete one;
    GmAcObject* three = GmAcObject::Create(3);
    CHECK(three != nullptr && three[2].m_interaction == nullptr && three[2].m_unknown280 == 1);
    delete[] three;
}

void test_character() {
    TestCharacter c;
    CHECK((c.m_flags98 & (GmAcCharacter::kCharacterReset | GmAcObject::kObjectReset)) == (GmAcCharacter::kCharacterReset | GmAcObject::kObjectReset));
    CHECK(c.m_controller == nullptr && c.m_shadow == nullptr && c.m_combat == nullptr && c.m_sightConeBone == nullptr && c.m_event[0] == 0);
    CHECK(c.Slot2() == 0);
    CHECK(c.Slot59() == 0);
    GmAcCharController controller;
    controller.m_unknown04 = 0x5A;
    controller.m_owner = nullptr;
    c.SetController(&controller, nullptr);
    CHECK(c.Slot59() == 0x5A && controller.m_owner == nullptr);
    c.m_flags98 |= 0x100;
    c.SetController(&controller, nullptr);
    CHECK(controller.m_owner == &c);
    TestCharacter d;
    c.SetController(&controller, &d);
    CHECK(controller.m_owner == &d);
    c.SetController(nullptr, &d);
    CHECK(c.Slot59() == 0 && controller.m_owner == &d);
    std::memcpy(c.m_event, "EVENT123", 8);
    c.PostEvent();
    CHECK(c.m_slot49 == 1 && c.m_posted == "EVENT123:01");
    // The sight cone answers slot 55; without it the object's answer.
    CHECK(c.Slot55() == 1);
    TestSightCone* cone = new TestSightCone;
    c.m_combat = cone;
    CHECK(c.Slot55() == 0x77);
    // Shutdown: the cone stopped and deleted, the shadow taken off the list, stopped and deleted, every word reset.
    static int maps_heap;
    GmNameMap::s_heap = &maps_heap;
    GmAcActor::s_namedMaps.Init(4);
    TestShadow* shadow = new TestShadow;
    c.m_shadow = shadow;
    GmDynamicNodeLoader::s_instance = (GmDynamicNodeLoader*)&g_casters;
    GmShadowCastersAdd(&g_casters, &d);
    GmShadowCastersAdd(&g_casters, &c);
    float length = 3;
    c.m_sightConeLength = &length;
    g_log.clear();
    c.GmAcCharacter::Shutdown();
    CHECK(g_log == "cone-stop;cone-deleted;stop;shadow-deleted;");
    CHECK(c.m_shadow == nullptr && c.m_combat == nullptr && c.m_sightConeLength == nullptr);
    CHECK(g_casters.m_casters[0] == &d && g_casters.m_casters[1] == nullptr);
}

void test_casters() {
    GmShadowCasters list;
    std::memset(&list, 0, sizeof list);
    TestCharacter* c = new TestCharacter[3];
    GmShadowCastersAdd(&list, &c[0]);
    GmShadowCastersAdd(&list, &c[1]);
    GmShadowCastersAdd(&list, &c[0]);  // already there: nothing new
    GmShadowCastersAdd(&list, &c[2]);
    CHECK(list.m_casters[0] == &c[0] && list.m_casters[1] == &c[1] && list.m_casters[2] == &c[2] && list.m_casters[3] == nullptr);
    GmShadowCastersRemove(&list, &c[0]);
    CHECK(list.m_casters[0] == &c[1] && list.m_casters[1] == &c[2] && list.m_casters[2] == nullptr);
    GmShadowCastersRemove(&list, &c[0]);  // not there: nothing
    CHECK(list.m_casters[0] == &c[1]);
    for (int i = 0; i < 256; i++) list.m_casters[i] = &c[i % 2];
    GmShadowCastersAdd(&list, &c[2]);  // full: nothing
    CHECK(list.m_casters[255] == &c[1]);
    GmShadowCastersRemove(&list, &c[1]);  // the first only
    CHECK(list.m_casters[0] == &c[0] && list.m_casters[1] == &c[0] && list.m_casters[2] == &c[1] && list.m_casters[254] == &c[1] && list.m_casters[255] == nullptr);
    delete[] c;
}

void test_records() {
    static int heap;
    GmAcActor::SetHeap(&heap);
    TestCharacter c;
    c.AllocateRecords();  // no ConserveVfx: none
    CHECK(c.m_records == nullptr);
    int conserve = 1;
    unsigned int zero = 0, two = 2;
    c.m_unknown23C = &conserve;
    c.m_recordCount = &zero;
    c.AllocateRecords();
    CHECK(c.m_records == nullptr);
    c.m_recordCount = &two;
    c.AllocateRecords();
    CHECK(c.m_records != nullptr && c.m_records[1].m_unknown20 == 1.0f && c.m_records[1].m_unknown34[3] == 0 && c.m_records[0].m_unknown00[0] == 0);
    GmAcActorRecord* first = c.m_records;
    c.AllocateRecords();  // once only
    CHECK(c.m_records == first);
    // The shutdown clears as many records as the count says (3 without one), so the count stays until then.
    c.m_unknown23C = nullptr;
    c.GmAcActor::Shutdown();
    CHECK(c.m_records == nullptr);
}

void seam() {
    TestCharacter c;
    GmBaObject::Descriptor d = {{0, 0}, "proto", nullptr};
    int registry;
    c.m_unknown10 = &registry;
    c.m_descriptor = &d;
    c.Slot2();  // the registry's search is a seam: stops
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
    test_object();
    test_factory();
    test_character();
    test_casters();
    test_records();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int status = std::system(command.c_str());
    CHECK(status != 0);
    std::printf("%s: %d failures\n", g_fail ? "FAILED" : "passed", g_fail);
    return g_fail ? 1 : 0;
}
