// Host test of FaCmCombat's update pieces (gm/combat), on any pointer size. Not evidence of a byte match. The scene, the input, the sight test and the node iterator's search are seams, so
// these are the paths that do not reach them; with "--seam" the test runs the update on an owner, which reaches the node's slot 27 (a seam), and checks from outside that it stopped.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "at/AtScene.h"
#include "at/AtVector3.h"
#include "gm/FaCmCombat.h"
#include "gm/GmAcPlayer.h"
#include "gm/GmLevelManifest.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

std::string g_log;

class TestOwner : public GmAcCharacter {
public:
    GmAcActor* m_target = nullptr;
    int m_42 = 0;
    void Slot49(const char* event, int, int c) override { g_log += std::string(event, strnlen(event, 8)) + ":" + std::to_string(c) + ";"; }
    unsigned int Unknown17C() override { g_log += "53;"; return 0; }
    int Slot42() override { return m_42; }
    void Slot22(AtVector3* a, AtVector3* b, AtVector3*, AtVector3*, AtVector3*) override {
        if (a) { a->x = 1; a->y = 2; a->z = 3; }
        if (b) { b->x = 0; b->y = 1; b->z = 0; }
    }
};

bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

void test_helpers() {
    CHECK(AtCompareNoCase8("Robot", "rOBOT") == 0);
    CHECK(AtCompareNoCase8("RobotArm1", "robotarm2") == 0);  // eight characters only
    CHECK(AtCompareNoCase8("Ab", "Ac") == 'b' - 'c');
    CHECK(AtCompareNoCase8("Ab", "aC") == 'b' - 'C');        // the difference of the characters as they are
    CHECK(AtCompareNoCase8(nullptr, "") == 0);
    CHECK(AtCompareNoCaseN("abc", "abd", 2) == 0);

    const AtVector3 x = {1, 0, 0}, y = {0, 1, 0}, mx = {-1, 0, 0}, big = {2, 0, 0}, nbig = {-2, 0, 0};
    CHECK(near(x.AngleTo(y), std::acos(0.0)));
    CHECK(near(x.AngleTo(x), 0.0));
    CHECK(near(x.AngleTo(mx), std::acos(-1.0)));
    CHECK(x.AngleTo(big) == 0.0);                       // beyond 1: 0
    CHECK(x.AngleTo(nbig) == (double)3.14159274f);      // beyond -1: pi as a float

    GmGameEvent event;
    CHECK(event.m_unknown10 == 1 && event.m_name[0] == 0 && event.m_actor == nullptr && event.m_unknown08 == 0);
    event.Set(nullptr, 5, 6, -1);
    CHECK(event.m_unknown18 == 5 && event.m_unknown1C == 6 && event.m_unknown08 == -1 && event.m_unknown10 == 1);

    GmDynamicNodeIterator it;
    CHECK(it.m_loader == nullptr && it.m_entry == nullptr && it.m_item == nullptr);
    it.m_entry = &it;
    CHECK(it.Begin((GmDynamicNodeLoader*)&it) == 0 && it.m_entry == &it);   // nothing cleared without a loader before
    CHECK(it.Begin(nullptr) == 0 && it.m_entry == nullptr && it.m_loader == nullptr);

    GmButtonState state = {1, 0};
    CHECK(state.IsDown() == 1);
    state.m_state = 3;
    CHECK(state.IsDown() == 1);
    state.m_state = 2;
    CHECK(state.IsDown() == 0);

    FaHud* hud = (FaHud*)std::calloc(1, sizeof(FaHud));
    hud->SetTargetName("");
    CHECK(hud->m_targetNameSet == 0);
    hud->SetTargetName("Numbuh1");
    CHECK(hud->m_targetNameSet == 1 && std::strncmp(hud->m_targetName, "Numbuh1", 8) == 0);
    std::free(hud);
}

void test_combat() {
    TestOwner owner;
    // Types: the actor's own name first, else the record's TYPE.
    char type[8] = "Robot";
    owner.m_unknown240[0] = type;
    CHECK(owner.IsOfType("ROBOT") == 1 && owner.IsOfType("Ninja") == 0);
    std::memcpy(owner.m_unknown264, "Ninja", 6);
    CHECK(owner.IsOfType("ninja") == 1 && owner.IsOfType("Robot") == 0);
    std::memcpy(owner.m_noteBank, "Bank1", 6);
    char bank[8];
    owner.GetNoteBank(bank);
    CHECK(std::strncmp(bank, "Bank1", 8) == 0);

    FaCmCombat c;
    c.m_owner = &owner;
    CHECK(near(c.MaxAimAngleH(), 45.0 * 0.0174532924f));
    CHECK(c.ThreatEnterRange() == 1000.0 && c.ThreatExitRange() == 1500.0);
    float enter = 300, override_ = 50;
    c.m_threatEnterRange = &enter;
    CHECK(c.ThreatEnterRange() == 300.0);
    c.m_unknown2C = 1;  // a pointer in the original; only its being set is tested here on a 64-bit host
    c.m_unknown2C = 0;
    (void)override_;

    // PostThreat: a new threat sends THREAT; losing it sends UNTHREAT; the range's enter event follows a change.
    char enterEvent[8] = "ENTER0";
    AtDataDescriptor descriptor;
    FaCmAttackRange range = {nullptr, enterEvent, nullptr};
    (void)descriptor;
    c.m_attackRanges = &range;
    c.m_attackRangeCount = 1;
    g_log.clear();
    c.m_unknown94 = 1;
    c.m_unknown98 = 0;
    c.PostThreat(1, 0, -1, nullptr, 7);
    CHECK(g_log == "THREAT:1;53;ENTER0:1;");
    CHECK(c.m_unknownA4 == 7);  // the owner's target (none) is the threat (none)
    g_log.clear();
    c.PostThreat(0, 1, 0, nullptr, 7);  // the same range, no change: nothing
    CHECK(g_log == "");
    g_log.clear();
    c.m_unknown94 = 0;
    c.m_unknown98 = -1;
    c.PostThreat(1, 1, 0, nullptr, 7);
    CHECK(g_log == "UNTHREAT:1;" && c.m_unknownA4 == -1);
    c.m_attackRanges = nullptr;
    c.m_attackRangeCount = 0;

    // With no enemies in the list nothing is chosen; TARGETNO only when the owner has a target.
    AtDataDescriptor names;
    names.m_count = 0;
    AtDataShortNameArray list;
    list.m_descriptor = &names;
    c.m_enemies = &list;
    AtSceneNode node;
    node.m_flags = 0;
    owner.m_sceneNode = &node;
    CHECK(c.ChooseTarget(0, 1, 1.0f, 0) == nullptr);
    g_log.clear();
    c.UpdateTargetMe();
    CHECK(g_log == "53;53;53;");
    list.m_descriptor = nullptr;
    owner.m_sceneNode = nullptr;
    c.m_enemies = nullptr;
    c.m_threatEnterRange = nullptr;
    c.m_owner = nullptr;
}

void seam() {
    TestOwner owner;
    FaCmCombat c;
    c.m_owner = &owner;
    c.m_unknown08 = 0;
    c.Update(1);
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
    test_helpers();
    test_combat();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int status = std::system(command.c_str());
    CHECK(status != 0);
    std::printf("%s: %d failures\n", g_fail ? "FAILED" : "passed", g_fail);
    return g_fail ? 1 : 0;
}
