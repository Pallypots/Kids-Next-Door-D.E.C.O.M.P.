// Host test of the game layer's actor base, on any pointer size. Not evidence of a byte match. No file loads an actor yet, so these are constructed objects and direct calls, as in the oracle.
// With "--seam" it calls into a seam, which must stop the process with a diagnostic; the test runs itself that way and checks that it does.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gm/GmAcActor.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

// What the actor's slots that are not recovered would be called with, recorded by a class built on it, as a real derived class would override them.
class TestActor : public GmAcActor {
public:
    int m_slot40 = -1;
    int m_slot49 = 0;
    std::string m_event;
    int m_slot8 = -1;
    std::string m_name8;
    void Slot40(int a) override { m_slot40 = a; }
    void Slot49(const char* event, int, int) override { m_slot49++; m_event = event; }
    void Slot8(int a, const char* name) override { m_slot8 = a; m_name8.assign(name, strnlen(name, 8)); }
    int Slot2() override { return 0x22; }
};

class TestNode : public GmAcNode {
public:
    int m_slot40 = -1, m_slot41 = -1;
    void Slot40(int a) override { m_slot40 = a; }
    void Slot41(int a) override { m_slot41 = a; }
};

void test_construction() {
    TestActor a;
    CHECK(a.m_initialized == 0);
    CHECK((a.m_flags98 & (GmAcNode::kNodeReset | GmAcNode::kActorReset)) == (GmAcNode::kNodeReset | GmAcNode::kActorReset));
    CHECK(a.m_unknownFC[2] == 254.0f && a.m_unknown108[4] == 255.0f);
    CHECK(a.m_unknown140 == 1 && a.m_unknown280 == 1);
    CHECK(a.m_unknown290[0] == 1.0f && a.m_unknown2D4 == 1.0f);
    CHECK(a.m_unknown24C < -3.0e38f);
    CHECK(a.m_callback.m_unknown04 == 1 && a.m_callback.m_actor == nullptr);
    CHECK(a.m_vectors184.m_active == 0 && a.m_vectors1BC.m_active == 0);
    CHECK(a.m_items.IsEmpty());
    CHECK(a.m_component == nullptr && a.m_records == nullptr && a.m_motions == nullptr);
    CHECK(a.Animation() == &a.m_animation && a.m_animation.m_owner == nullptr);
}

void test_object() {
    GmBaObject::Descriptor alias = {{0, 0}, "alias", nullptr};
    GmBaObject::Descriptor d = {{0, 0}, "named", &alias};
    TestActor a;
    CHECK(a.Name() == nullptr);
    a.m_descriptor = &d;
    CHECK(std::strcmp(a.Name(), "alias") == 0);
    a.m_unknown1FC = 1;
    CHECK(std::strcmp(a.Name(), "named") == 0);
    CHECK(std::strcmp(a.GmBaObject::Name(), "named") == 0);
    CHECK(a.Initialize() == 0 && a.IsInitialized() == 1);
    a.m_unknown30[0] = 5;
    // Initialized: Initialize shuts the actor down first, which resets it.
    CHECK(a.Initialize() == 0 && a.IsInitialized() == 1);
    CHECK(a.m_unknown30[0] == 0 && a.m_descriptor == nullptr);
    CHECK(a.Slot7(9) == 0);
}

void test_setup() {
    GmBaObject::Descriptor d = {{0, 0}, "123456789", nullptr};
    TestActor a;
    int x;
    a.Setup(&x, 1, 77, &d);
    CHECK(a.m_unknown10 == &x && a.m_descriptor == &d);
    CHECK(a.m_slot8 == 77 && a.m_name8 == "12345678");
    CHECK(a.SetupAndConfigure(nullptr, nullptr) == 0x22);
}

void test_slots() {
    TestActor a;
    int zero = 0, one = 1;
    CHECK(a.Slot27() == 0 && a.HasUnknown254() == 0);
    a.m_unknown254 = &zero;
    CHECK(a.Slot27() == 0);
    a.m_unknown254 = &one;
    CHECK(a.Slot27() == 1);
    a.m_unknown1FC = 1;
    a.m_unknown200 = 1;
    CHECK(a.HasUnknown254() == 0 && a.Slot27() == 0);
    CHECK(a.Slot56() == 1);
    a.m_flagsA4 = GmAcNode::kFlagA4_2;
    CHECK(a.Slot27() == 1 && a.Slot56() == 0);
    a.m_unknown204 = 1;
    CHECK(a.Slot57() == 1);
    a.m_flagsA4 = 0x100;
    CHECK(a.Slot13() == 1);
    a.GmAcNode::SetFlag10(1);
    CHECK(a.m_flagsA4 == 0x110);
    a.GmAcNode::SetFlag10(0);
    CHECK(a.m_flagsA4 == 0x100);
    a.SetUnknown284(&a);
    CHECK(a.m_unknown284 == nullptr);
    TestActor b;
    a.SetUnknown284(&b);
    CHECK(a.m_unknown284 == &b);
    unsigned int big = 0xFFFFFFFFu, high = 0x80000001u;
    CHECK(a.Unknown27C() == 0.0f);
    a.m_unknown27C = &big;
    CHECK(a.Unknown27C() == 4294967296.0f);
    a.m_unknown27C = &high;
    CHECK(a.Unknown27C() == 2147483648.0f);
    int v = 42;
    a.m_unknown278 = &v;
    CHECK(a.Unknown278() == 42 && a.Unknown274() == 0);
    int out[2] = {5, 5};
    a.Slot54(out);
    CHECK(out[0] == 0 && out[1] == 0);
    CHECK(a.Slot44(0) == 1 && a.Slot55() == 0 && a.Slot59() == 0 && a.Slot42() == 0);
    a.Slot41(7);
    CHECK(a.m_slot40 == 7);
    a.m_component = (GmAcComponent*)&v;
    a.ForgetComponent();
    CHECK(a.m_component == nullptr);
    // Without a component these do nothing.
    a.StartCollision(1, 2);
    a.EndCollision(1);
    a.Slot38(1);
}

void test_controller() {
    TestNode node;
    GmAcAnimController c;
    c.m_owner = &node;
    c.Slot1(3);
    c.Slot2(4);
    CHECK(node.m_slot40 == 3 && node.m_slot41 == 4);
    c.m_owner = nullptr;
    c.Clear();  // nothing without an owner
    CHECK(c.m_owner == nullptr);
}

void test_records() {
    TestActor a;
    GmAcActorRecord records[4];
    std::memset(records, 0x66, sizeof records);
    a.m_records = records;
    a.ClearRecords();
    CHECK(records[2].m_unknown00[0] == 0 && records[2].m_unknown24 == 0 && records[2].m_unknown2C[1] == 0 && records[2].m_unknown08[2] == 0);
    CHECK(records[2].m_unknown14[0] == 0x66666666 && records[3].m_unknown00[0] == 0x66666666);
    unsigned int four = 4;
    a.m_recordCount = &four;
    a.ClearRecords();
    CHECK(records[3].m_unknown00[0] == 0);
    a.m_records = nullptr;
    a.m_recordCount = nullptr;
}

void test_callback() {
    TestActor a;
    a.m_callback.m_actor = &a;
    // Slot 27 answers 0: nothing happens.
    a.m_callback.OnTrigger("t1", 0, 0);
    CHECK(a.m_slot49 == 0);
    a.m_callback.OnLoop(0);
    CHECK(a.m_slot49 == 1 && a.m_event == "ANIMLOOP");
    // No actor and no node: nothing.
    a.m_callback.Reset();
    a.m_callback.OnTrigger("t1", 0, 0);
    a.m_callback.OnLoop(0);
    CHECK(a.m_slot49 == 1);
}

void test_factories() {
    static int heap;
    GmAcActor::SetHeap(&heap);
    GmAcActor* one = GmAcActor::CreateRotator(1);
    CHECK(one != nullptr && one->m_unknown280 == 1);
    delete one;
    GmAcActor* three = GmAcActor::CreateNode(3);
    CHECK(three != nullptr && three[2].m_unknown140 == 1);
    delete[] three;
    GmBaObject* objects = GmBaObject::NewArray(2);
    objects[1].m_initialized = 1;
    delete[] objects;
}

void seam() {
    TestActor a;
    a.m_callback.m_actor = &a;
    a.m_flagsA4 = GmAcNode::kFlagA4_2;
    a.m_callback.OnTrigger("t1", 0, 0);  // asks the effects manager, a seam: stops
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
    test_construction();
    test_object();
    test_setup();
    test_slots();
    test_controller();
    test_records();
    test_callback();
    test_factories();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int status = std::system(command.c_str());
    CHECK(status != 0);
    std::printf("%s: %d failures\n", g_fail ? "FAILED" : "passed", g_fail);
    return g_fail ? 1 : 0;
}
