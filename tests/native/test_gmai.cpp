// Host test of the AI action manager, the behaviour chart's commands and the game's random numbers, on any pointer size. Not evidence of a byte match. Reading actions reaches the registry's
// search, a seam: with "--seam" the test does that, and checks from outside that it stopped. Everything else is built by hand.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "gm/GmRandom.h"
#include "gm/GmAcNPC.h"
#include "gm/GmAppSettings.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

std::string g_log;

// Conditions by a set of names, and the slot 49 events logged.
class TestNPC : public GmAcNPC {
public:
    std::string m_true;
    int TestCondition(const char* name) override {
        g_log += std::string("?") + std::string(name, strnlen(name, 8)) + ";";
        return m_true.find(std::string(name, strnlen(name, 8)) + ",") != std::string::npos ? 1 : 0;
    }
    void Slot49(const char* event, int, int) override { g_log += std::string("!") + std::string(event, strnlen(event, 8)) + ";"; }
};

struct Names {
    void* m_vftable;
    AtDataDescriptor* m_descriptor;
    void* m_registry;
    char (*m_data)[8];
};

void set_name(char* to, const char* from) {
    std::memset(to, 0, 8);
    std::memcpy(to, from, strnlen(from, 8));
}

GmAiAction make_action(const char* name, float weight, int repeatable, const char* next) {
    GmAiAction a;
    std::memset(&a, 0, sizeof a);
    set_name(a.m_name, name);
    a.m_weight = weight;
    a.m_enabled = 1;
    a.m_repeatable = repeatable;
    set_name(a.m_next, next);
    return a;
}

void test_random() {
    // The reference MT19937 with the old 69069 seeding: Knuth's seed 4357 gives these first outputs.
    GmRandom r;
    CHECK(r.m_index == 625);
    CHECK(r.m_seed == 0);
    CHECK(r.SeedDefaultReturningSeed() == 4357);
    CHECK(r.m_index == 0);
    // Against the 1998 reference code (sgenrand and genrand of mt19937.c), written here in its own form.
    {
        static unsigned int mt[624];
        mt[0] = 4357u;
        for (int i = 1; i < 624; i++) mt[i] = 69069u * mt[i - 1];
        int mti = 624;
        static const unsigned int mag01[2] = {0u, 0x9908B0DFu};
        for (int draw = 0; draw < 1300; draw++) {
            if (mti >= 624) {
                int kk;
                unsigned int y;
                for (kk = 0; kk < 624 - 397; kk++) { y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7FFFFFFFu); mt[kk] = mt[kk + 397] ^ (y >> 1) ^ mag01[y & 1]; }
                for (; kk < 623; kk++) { y = (mt[kk] & 0x80000000u) | (mt[kk + 1] & 0x7FFFFFFFu); mt[kk] = mt[kk + (397 - 624)] ^ (y >> 1) ^ mag01[y & 1]; }
                y = (mt[623] & 0x80000000u) | (mt[0] & 0x7FFFFFFFu);
                mt[623] = mt[396] ^ (y >> 1) ^ mag01[y & 1];
                mti = 0;
            }
            unsigned int y = mt[mti++];
            y ^= y >> 11;
            y ^= (y << 7) & 0x9D2C5680u;
            y ^= (y << 15) & 0xEFC60000u;
            y ^= y >> 18;
            GmRandom* g = &r;
            const unsigned int got = g->Next();
            if (got != y) { CHECK(got == y); break; }
        }
        r.SeedDefaultReturningSeed();
    }
    const unsigned int first = r.Next();
    GmRandom s;
    CHECK(s.SeedDefault() == 0);
    CHECK(s.Next() == first);
    for (int i = 0; i < 2000; i++) s.Next();  // past a second twist
    CHECK(s.m_index < 624);
    CHECK(s.NextBelow(0) == 0);
    const unsigned int before = s.m_index;
    CHECK(s.NextBelow(0) == 0 && s.m_index == before);  // no draw
    CHECK(s.NextBelow(7) < 7);
    const double u = s.NextUnit();
    CHECK(u >= 0.0 && u < 1.0);
    // A known value: MT19937 seeded by init_genrand(5489) differs in seeding, so check the twist and tempering against a hand computation instead: one word of state, tempered.
    GmRandom t;
    std::memset(t.m_state, 0, sizeof t.m_state);
    t.m_state[0] = 0x12345678u;
    t.m_index = 0;
    unsigned int y = 0x12345678u;
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680u;
    y ^= (y << 15) & 0xEFC60000u;
    y ^= y >> 18;
    CHECK(t.Next() == y);
    // A twist of all zero words but one odd word gives the matrix where that word is the second of a pair.
    std::memset(t.m_state, 0, sizeof t.m_state);
    t.m_state[1] = 1;
    t.Regenerate();
    CHECK(t.m_index == 0);
    CHECK(t.m_state[0] == 0x9908B0DFu);
    CHECK(t.m_state[1] == 0);
}

void test_manager(GmRandom* random) {
    GmAiActionManager m;
    CHECK(m.m_owner == nullptr && m.m_actions == nullptr && m.m_actionCount == 0 && m.m_pattern == nullptr && m.m_current[0] == 0);

    TestNPC npc;
    npc.m_true = "cNear,";
    m.m_owner = &npc;
    const char defaults[8] = {'I', 'd', 'l', 'e', 0, 0, 0, 0};
    m.m_defaultAction = defaults;
    m.m_actionCount = 3;
    m.m_actions = (GmAiAction*)operator new(3 * sizeof(GmAiAction));
    m.m_weights = (float*)operator new(3 * sizeof(float));
    m.m_actions[0] = make_action("Punch", 2.0f, 0, "");
    m.m_actions[1] = make_action("Kick", 1.0f, 1, "Punch");
    m.m_actions[2] = make_action("Run", 4.0f, 1, "");
    m.m_actions[2].m_modifierCount = 2;
    m.m_actions[2].m_modifiers = (GmAiActionModifier*)operator new(2 * sizeof(GmAiActionModifier));
    set_name(m.m_actions[2].m_modifiers[0].m_condition, "cNear");
    m.m_actions[2].m_modifiers[0].m_factor = 0.25f;
    set_name(m.m_actions[2].m_modifiers[1].m_condition, "cFar");
    m.m_actions[2].m_modifiers[1].m_factor = 0.0f;

    CHECK(m.Find("Kick", 0) == &m.m_actions[1]);
    CHECK(m.Find("Jump", 0) == nullptr);
    CHECK(m.IsEnabled("Kick") == 1);
    m.Disable("Kick");
    CHECK(m.IsEnabled("Kick") == 0);
    m.Enable("Kick");
    CHECK(m.IsEnabled("Kick") == 1);
    m.Disable("Jump");

    g_log.clear();
    CHECK(m.Weight(2, 0) == 1.0f);  // 4 * 0.25; cFar false
    CHECK(g_log == "?cNear;?cFar;");
    set_name(m.m_current, "Punch");
    CHECK(m.Weight(0, 0) == 0.0f);  // not Repeatable, and the last
    CHECK(m.Weight(0, 1) == 2.0f);
    m.m_actions[0].m_enabled = 0;
    CHECK(m.Weight(0, 1) == 0.0f);
    m.m_actions[0].m_enabled = 1;

    // The last action names a next: that one.
    char out[8];
    set_name(m.m_current, "Kick");
    m.Choose(out);
    CHECK(std::strncmp(out, "Punch", 8) == 0);

    // A random pick: each weight kept, the pick one of the actions with weight.
    set_name(m.m_current, "Punch");
    for (int i = 0; i < 50; i++) {
        m.Choose(out);
        CHECK(m.m_weights[0] == 0.0f && m.m_weights[1] == 1.0f && m.m_weights[2] == 1.0f);
        CHECK(std::strncmp(out, "Kick", 8) == 0 || std::strncmp(out, "Run", 8) == 0);
    }
    // Nothing has weight unless repeats are allowed: then the last one again.
    m.m_actions[1].m_enabled = 0;
    m.m_actions[2].m_enabled = 0;
    m.Choose(out);
    CHECK(m.m_weights[0] == 2.0f);
    CHECK(std::strncmp(out, "Punch", 8) == 0);
    // Nothing at all: the default.
    m.m_actions[0].m_enabled = 0;
    m.Choose(out);
    CHECK(std::strncmp(out, "Idle", 8) == 0);
    m.m_actions[0].m_enabled = 1;
    m.m_actions[1].m_enabled = 1;
    m.m_actions[2].m_enabled = 1;

    // The draw: with a known generator, the pick is where Unit() * total falls.
    random->Seed(99);
    GmRandom copy = *random;
    m.Choose(out);
    const float target = (float)(copy.NextUnit() * 2.0f);
    CHECK(std::strncmp(out, target < 1.0f ? "Kick" : "Run", 8) == 0);

    // A pattern: its entries in turn, moved on only by storing the entry.
    char (*entries)[8] = (char (*)[8])std::calloc(2, 8);
    set_name(entries[0], "Run");
    set_name(entries[1], "Kick");
    AtDataDescriptor* descriptor = (AtDataDescriptor*)std::calloc(1, sizeof(AtDataDescriptor));
    descriptor->m_type = AtDataRegistry::kShortNameArray;
    descriptor->m_count = 2;
    Names names = {nullptr, descriptor, nullptr, entries};
    m.m_pattern = (AtDataShortNameArray*)&names;
    m.m_patternIndex = 5;
    m.Choose(out);
    CHECK(m.m_patternIndex == 0 && std::strncmp(out, "Run", 8) == 0);
    m.Store("Kick");
    CHECK(m.m_patternIndex == 0 && std::strncmp(m.m_current, "Kick", 8) == 0);
    m.Store("Run");
    CHECK(m.m_patternIndex == 1);
    m.Choose(out);
    CHECK(std::strncmp(out, "Kick", 8) == 0);
    m.Store("Kick");
    CHECK(m.m_patternIndex == 0);
    m.m_patternIndex = 2;
    m.Store("Run");
    CHECK(m.m_patternIndex == 0);
    descriptor->m_count = 0;
    m.Choose(out);
    CHECK(std::strncmp(out, "Idle", 8) == 0);
    m.m_pattern = nullptr;

    // The chart's command: the choice told to the character.
    GmChartFrame* frame = (GmChartFrame*)std::calloc(1, sizeof(GmChartFrame));
    GmChartContext* context = (GmChartContext*)std::calloc(1, sizeof(GmChartContext));
    context->m_current = 3;
    context->m_frames[3] = frame;
    frame->m_actor = &npc;
    npc.m_aiActions = nullptr;
    CHECK(GmChartChooseAIAction(context, nullptr, nullptr) == 0);
    npc.m_aiActions = &m;
    set_name(m.m_current, "Kick");
    g_log.clear();
    CHECK(GmChartChooseAIAction(context, nullptr, nullptr) == 1);  // Punch, not the last: told without asking RESUME
    CHECK(g_log == "!Punch;");
    npc.m_aiActions = nullptr;

    m.Clear();
    CHECK(m.m_actions == nullptr && m.m_weights == nullptr && m.m_actionCount == 0 && m.m_owner == nullptr && m.m_defaultAction == nullptr);
    std::free(entries);
    std::free(descriptor);
    std::free(frame);
    std::free(context);
}

void test_helpers() {
    CHECK(AtNameLength("") == 0);
    CHECK(AtNameLength("abc") == 3);
    CHECK(AtNameLength("abcdefghij") == 8);
    GmAcNPC npc;
    CHECK(npc.FindRecord("", 0) == nullptr);
}

void seam() {
    GmAcNPC npc;
    GmAiActionManager m;
    m.m_owner = &npc;
    m.ReadActions();  // the list is found through the registry's search, a seam: stops
    std::printf("the seam came back\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--seam") == 0) {
        seam();
        return 0;
    }
    static GmRandom random;
    random.SeedDefaultReturningSeed();
    static GmHistoryLogs holder;
    holder.m_random = &random;
    GmHistoryLogs::s_instance = &holder;
    test_random();
    test_manager(&random);
    test_helpers();
    const std::string command = std::string("\"") + argv[0] + "\" --seam";
    const int code = std::system(command.c_str());
    CHECK(code != 0);
    if (g_fail == 0) std::printf("test_gmai: all passed\n");
    return g_fail == 0 ? 0 : 1;
}
