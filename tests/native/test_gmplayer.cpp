// Host test of GmAcPlayer, FaAcPlayer and the controller frame, on any pointer size. Not evidence of a byte match. No file makes a player yet, so these are constructed objects and direct calls.
// Reading input is a seam, so asking FaAcPlayer for its look stops: with "--seam" the test does that, and checks from outside that it stopped.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gm/GmAcPlayer.h"
#include "gm/GmLevelManifest.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

std::string g_log;

// GmAcPlayer is abstract (slot 65 is pure): a host class that logs it.
class TestPlayer : public GmAcPlayer {
public:
    void ReadLook() override { g_log += "look;"; }
    // The node's frame slots are not recovered: these stand in, with a frame of their own.
    AtVector3 m_frame[4] = {{1, 2, 3}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}};
    AtVector3 m_set[4] = {};
    void Slot22(AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d, AtVector3*) override {
        if (a) *a = m_frame[0];
        if (b) *b = m_frame[1];
        if (c) *c = m_frame[2];
        if (d) *d = m_frame[3];
    }
    void Slot23(AtVector3* a, AtVector3* b, AtVector3* c, AtVector3* d, AtVector3*) override {
        m_set[0] = *a;
        m_set[1] = *b;
        m_set[2] = *c;
        m_set[3] = *d;
    }
};

class TestInput : public AppUnknown692C {
public:
    int FindAxis(const char* name) override { return std::strcmp(name, "GameAxisForward") == 0 ? 7 : 9; }
};

class TestCombat : public GmCmCombat {
public:
    ~TestCombat() override { g_log += "combat-deleted;"; }
    void Stop() override {}
    void Slot5() override {}
    void Update(int) override {}
    int Slot7() override { return 0; }
};

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

void test_controller() {
    GmAcCharController c;
    CHECK(c.m_unknown00 == 0 && c.m_unknown04 == -1 && c.m_owner == nullptr && c.m_pending == 0);
    CHECK(c.m_forward.y == 1.0f && c.m_up.z == 1.0f && c.m_right.x == 1.0f && c.m_forward.x == 0.0f);
    c.Activate();
    CHECK(c.m_unknown00 == 1 && c.Id() == -1 && c.Owner() == nullptr && c.HasPending() == 0);
    const AtVector3 f = {1, 2, 3}, u = {4, 5, 6}, r = {7, 8, 9};
    TestPlayer owner;
    c.m_owner = &owner;
    g_log.clear();
    c.SetFrame(&f, &u, &r, 0);
    CHECK(g_log == "look;" && c.HasPending() == 1 && c.m_forward.y == 1.0f && c.m_pendingUp.y == 5.0f);
    AtVector3 a, b, d;
    c.GetFrame(&a, &b, &d);
    CHECK(a.y == 1.0f && b.z == 1.0f && d.x == 1.0f);
    c.ApplyPending();
    CHECK(c.HasPending() == 0 && c.m_forward.z == 3.0f && c.m_right.x == 7.0f);
    c.SetFrame(&r, &f, &u, 1);
    CHECK(c.m_forward.x == 7.0f && c.m_up.x == 1.0f && c.HasPending() == 0 && g_log == "look;look;");
    c.ApplyPending();
    CHECK(c.m_forward.x == 7.0f);
    c.m_owner = nullptr;
}

void test_player() {
    TestPlayer p;
    CHECK((p.m_flags98 & GmAcPlayer::kPlayerReset) != 0 && (p.m_flags98 & GmAcCharacter::kCharacterReset) != 0);
    CHECK(p.m_unknown30C == 1 && p.m_partner == nullptr && p.m_backwardsThreshold == nullptr);
    // Slot 2 always asks the registry (there is no test for a record), whose search is a seam: not called here.
    CHECK(p.Slot27() == 1 && p.Slot42() == 1 && p.Slot66() == 0xFFFF && p.Slot62() == 0.0f && p.FollowMaxVerticalLook() == 0.0f);
    // Slot 61.
    float threshold = 0.5f;
    CHECK(p.Slot61(1.0f, 0.1f) == 0);  // no threshold: 0
    p.m_backwardsThreshold = &threshold;
    CHECK(p.Slot61(1.0f, 0.1f) == 1 && p.Slot61(1.0f, -0.4f) == 1 && p.Slot61(1.0f, 0.5f) == 0 && p.Slot61(0.0f, 0.1f) == 0 && p.Slot61(-1.0f, 0.1f) == 0);
    p.m_unknown30C = 0;
    CHECK(p.Slot61(1.0f, 0.1f) == 0);
    // Slot 44.
    GmAcPlayerUnknown304* view = (GmAcPlayerUnknown304*)std::calloc(1, sizeof(GmAcPlayerUnknown304));
    p.m_unknown304 = view;
    GmAcQuery q = {}, box = {};
    CHECK(p.Slot44(nullptr) == 1);
    q.m_kind = 1;
    CHECK(p.Slot44(&q) == 1);
    view->m_unknown3C28 = 1;
    CHECK(p.Slot44(&q) == 0);
    q.m_kind = 0;
    CHECK(p.Slot44(&q) == 1);  // no names
    static int heap;
    GmNameMap::s_heap = &heap;
    GmNameMap names;
    names.Init(8);
    names.Insert("BIGBOX", &box);
    q.m_names = &names;
    CHECK(p.Slot44(&q) == 0);  // a box, not this one, while the word is set
    CHECK(p.Slot44(&box) == 1);  // kind 0 without names
    box.m_names = &names;
    CHECK(p.Slot44(&box) == 1);  // the box itself
    view->m_unknown3C28 = 0;
    CHECK(p.Slot44(&q) == 1 && p.Slot44(&box) == 0);
    std::free(view);
    p.m_unknown304 = nullptr;
    // The move frame: the node's.
    AtVector3 f, u, r;
    CHECK(p.MoveFrame(&f, &u, &r) == 0 && f.y == 1.0f && u.z == 1.0f && r.x == 1.0f);
    // A partner: its frame, levelled, becomes this player's; with the world's up.
    GmWorldAxes world = {};
    world.m_up = {0, 0, 1};
    GmWorldAxes::s_instance = &world;
    TestPlayer partner;
    partner.m_frame[1] = {0, 3, 4};
    partner.m_frame[0] = {5, 6, 7};
    GmAcCharController controller;
    p.m_controller = &controller;
    p.SetPartner(&partner);
    CHECK(p.m_partner == &partner && partner.m_controller == &controller && controller.m_owner == &p);
    p.SetPartner(nullptr);
    CHECK(p.m_partner == nullptr && partner.m_controller == nullptr);
    // The controller's forwarders.
    CHECK(p.HasPendingFrame() == 0);
    controller.m_owner = &p;
    g_log.clear();
    p.SetControllerFrame(&f, &u, &r, 0);
    CHECK(p.HasPendingFrame() == 1 && g_log == "look;");
    p.ApplyPendingFrame();
    p.ControllerFrame(&f, &u, &r);
    CHECK(p.HasPendingFrame() == 0 && f.y == 1.0f);
    p.m_controller = nullptr;
    p.SetControllerFrame(&f, &u, &r, 1);
    CHECK(p.HasPendingFrame() == 0);
    // Slot 11.
    p.m_link = (GmAcNodeLink*)&heap;
    p.Slot11();
    CHECK(p.m_link == nullptr);
    // The loader's hold.
    GmLoadedPlayers loaded = {};
    GmLoadedPlayersAdd(&loaded, &p);
    GmLoadedPlayersAdd(&loaded, &partner);
    CHECK(loaded.m_players[0] == &p && loaded.m_count == 1);
    p.m_unknown308 = 5;
    p.GmAcPlayer::Shutdown();
    CHECK(p.m_unknown308 == 0 && p.m_unknown30C == 1);
}

void test_fa() {
    static int heap;
    AtSegmentHitList::SetHeap(&heap);
    AtQueryPool::SetHeap(&heap);
    GmAcActor::SetHeap(&heap);
    {
        FaAcPlayer p;
        CHECK(GmAcSightQuery::s_count == 1 && GmAcSightQuery::s_query.m_capacity == 3);
        CHECK((p.m_flags98 & FaAcPlayer::kFaPlayerReset) != 0 && p.m_unknown40C == 2 && p.m_gameAxisForward == -1 && p.m_unknown3FC == -1 && p.m_unknown408 == 1);
        CHECK(p.m_sightQuery.m_unknown1C == -1.0f && p.m_sightQuery.m_unknown24 == 1.0f && p.m_target.m_scene == nullptr);
        CHECK(p.Slot66() == 2 && p.Slot61(1.0f, 0.0f) == 0 && p.FollowMaxVerticalLook() == 60.0f && p.FollowMinVerticalLook() == -60.0f);
        float look = 12.0f;
        p.m_followMaxVerticalLook = &look;
        CHECK(p.FollowMaxVerticalLook() == 12.0f);
        TestInput input;
        App* app = (App*)std::calloc(1, sizeof(App));  // only its +0x692C is read
        app->m_unknown692C = &input;
        App::s_instance = app;
        CHECK(input.FindAxis("GameAxisForward") == 7);
        p.m_gameAxisForward = 7;
        p.m_combat = new TestCombat;
        g_log.clear();
        p.FaAcPlayer::Shutdown();
        CHECK(g_log == "combat-deleted;" && p.m_combat == nullptr && p.m_gameAxisForward == -1);
        FaAcPlayer q;
        CHECK(GmAcSightQuery::s_count == 2);
        App::s_instance = nullptr;
        std::free(app);
    }
    CHECK(GmAcSightQuery::s_count == 0 && GmAcSightQuery::s_query.m_capacity == 0);
    FaAcPlayer* one = FaAcPlayer::Create(-1);
    CHECK(one != nullptr && one->m_unknown40C == 2);
    GmPlug* plug = one;
    delete plug;  // through the GmPlug part
    FaAcPlayer* three = FaAcPlayer::Create(3);
    CHECK(three != nullptr && three[2].m_unknown408 == 1 && GmAcSightQuery::s_count == 3);
    delete[] three;
    CHECK(GmAcSightQuery::s_count == 0);
    CHECK(AtNameLength("") == 0 && AtNameLength("abc") == 3 && AtNameLength("abcdefghij") == 8);
}

void seam() {
    FaAcPlayer p;
    TestInput input;
    App* app = (App*)std::calloc(1, sizeof(App));  // only its +0x692C is read
    app->m_unknown692C = &input;
    App::s_instance = app;
    GmAcCharController controller;
    p.m_controller = &controller;
    p.ReadLook();  // the input's axis value is a seam: stops
    std::printf("the seam came back\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--seam") == 0) {
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);  // no error-report dialog in the child
#endif
        static int heap;
        AtSegmentHitList::SetHeap(&heap);
        seam();
        return 0;
    }
    test_controller();
    test_player();
    test_fa();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int status = std::system(command.c_str());
    CHECK(status != 0);
    std::printf("%s: %d failures\n", g_fail ? "FAILED" : "passed", g_fail);
    return g_fail ? 1 : 0;
}
