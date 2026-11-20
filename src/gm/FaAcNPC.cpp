// FaAcNPC, the enemies and the game's other AI characters, and the helpers its object file holds: see include/gm/GmAcNPC.h. Names other than RTTI names are inferred.
#include "gm/GmAcNPC.h"
#include "gm/GmLevelManifest.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();

namespace {
typedef decltype(sizeof(0)) Size;
struct Placement {};
const float kFloatMax = 3.40282347e+38f;  // 0x7F7FFFFF
}  // namespace
inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

// The combat object's empty slot, folded in the original with others of the same body. The rest of the class is not recovered: src/gm/GmAcNPCSeams.cpp.
void FaCmCombat::Start() {}

// ---- the helpers ---------------------------------------------------------------------------------------------------------------
// Quirk kept: unlike the C library's strncmp, the characters are compared as signed chars.
int AtCompareN(const char* a, const char* b, unsigned int count) {
    if (a == 0) {
        a = "";
    }
    if (b == 0) {
        b = "";
    }
    while (count != 0) {
        count--;
        if (*a != *b) {
            return (int)(signed char)*a - (int)(signed char)*b;
        }
        if (*a == 0) {
            return 0;
        }
        a++;
        b++;
    }
    return 0;
}

// Slot 53 answers the word at +0x17C, declared in include/gm/GmAcActor.h as unsigned: it holds an actor. On a 64-bit host that word cannot hold a pointer; see the worklog.
GmAcActor* GmAcTargetOf(GmAcActor* actor) {
    if (actor->Unknown17C() != 0 && (((GmAcActor*)(Size)actor->Unknown17C())->m_flags98 & GmAcNode::kActorReset) != 0) {
        return (GmAcActor*)(Size)actor->Unknown17C();
    }
    return 0;
}

int GmAcFloat154NotZero(GmAcActor* actor) {
    const float* words = (const float*)actor->m_unknown14C;
    return words[2] != 0.0f ? 1 : 0;
}

GmAcCharacter* GmNthControlledCharacter(GmControlledCharacters* list, unsigned int n) {
    GmAcCharacter** characters = &list->m_first;
    const unsigned int count = list->m_count;
    for (unsigned int i = 0; i < count; i++) {
        GmAcCharacter* character = characters[i];
        if (character != 0 && character->m_controller != 0) {
            if (n == 0) {
                return characters[i];
            }
            n--;
        }
    }
    return 0;
}

// ---- FaAcNPC -----------------------------------------------------------------------------------------------------------------
FaAcNPC::FaAcNPC() {
    Reset();
}

FaAcNPC::~FaAcNPC() {
    FaAcNPC::Shutdown();
}

// Clears the character's combat object too, which GmAcCharacter's shutdown has deleted by then.
void FaAcNPC::Reset() {
    m_flags98 |= kFaNPCReset;
    m_lowHealthRange = 0;
    m_medHealthRange = 0;
    m_highHealthRange = 0;
    m_combat = 0;
    m_numAIHealthLevels = 0;
    m_lastHealth = kFloatMax;
    m_unknown31C = 0;
}

void FaAcNPC::Shutdown() {
    GmAcNPC::Shutdown();
    Reset();
}

// Quirk kept: a range absent from the record keeps what was there; the level count is replaced whatever is found. Neither is looked for in the defaults' place first: the ranges are searched in
// the record with its parents only, the count in the record and then the defaults.
int FaAcNPC::Slot2() {
    int result = GmAcNPC::Slot2();
    AtDataComponent* range = FindValueObject("LowHealthRange", 0);
    if (range != 0) {
        m_lowHealthRange = range;
    }
    range = FindValueObject("MedHealthRange", 0);
    if (range != 0) {
        m_medHealthRange = range;
    }
    range = FindValueObject("HighHealthRange", 0);
    if (range != 0) {
        m_highHealthRange = range;
    }
    m_numAIHealthLevels = (int*)FindValueOrDefault("NumAIHealthLevels", 0);
    return result;
}

void FaAcNPC::Slot4() {
    m_lastHealth = kFloatMax;
    GmAcNPC::Slot4();
}

void FaAcNPC::Slot10() {
    GmAcNPC::Slot10();
}

// Quirk kept, as GmAcNPC's: a combat object already there is not deleted before another is made.
void FaAcNPC::Slot15() {
    GmAcNPC::Slot15();
    int* combat = (int*)FindValueOrDefault("Combat", 0);
    if (combat != 0 && *combat != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_combat = (GmCmCombat*)new FaCmCombat;
        AtPopHeap();
        if (m_combat == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        CombatOf(this)->Attach(this);
        CombatOf(this)->Start();
    }
}

void FaAcNPC::Slot20(int a) {
    if (Slot27() != 0) {
        UpdateHealthLevels();
    }
    GmAcNPC::Slot20(a);
}

void FaAcNPC::Slot49(const char* event, int b, int c) {
    GmAcNPC::Slot49(event, b, c);
}

// Levels are tried from the highest down. A level is crossed when value <= threshold * maximum < the last value; its Action goes to slot 49 and its Event, with its NameSpace, is posted. The
// search stops at the first level that does either. The comparisons are the original's x87 ones, with the product kept in a register (double here): a NaN crosses nothing.
// Quirks kept: nothing is done unless the last value is above zero; the last value is replaced on every call that reaches the health stat, crossing or not; the level's name is formatted into
// a buffer of the formatter's and copied to the stack without a bound, as the original does.
void FaAcNPC::UpdateHealthLevels() {
    FaCmCombat* combat = CombatOf(this);
    if (combat == 0 || combat->m_health == 0) {
        return;
    }
    GmAcStat* health = combat->m_health;
    const float value = health->m_value;
    const float maximum = health->m_max;
    if (m_lastHealth > 0.0f && value < m_lastHealth) {
        int level = (m_numAIHealthLevels != 0 ? *m_numAIHealthLevels : 0) - 1;
        for (; level >= 0; level--) {
            char name[0x100];
            {
                AtTempText text;
                char* from = AtFormatTemp(&text, "AIHealthLevel%d", level)->m_text;
                char* to = name;
                while ((*to++ = *from++) != 0) {
                }
            }
            float* threshold;
            {
                AtTempText text;
                threshold = (float*)FindValueOrDefault(AtFormatTemp(&text, "%sThreshold", name)->m_text, 0);
            }
            if (threshold == 0) {
                continue;
            }
            const double crossing = (double)maximum * *threshold;
            if (!(crossing < m_lastHealth) || !(value <= crossing)) {
                continue;
            }
            const char* action;
            const char* event;
            const char* nameSpace;
            {
                AtTempText text;
                action = (const char*)FindValueOrDefault(AtFormatTemp(&text, "%sAction", name)->m_text, 0);
            }
            {
                AtTempText text;
                event = (const char*)FindValueOrDefault(AtFormatTemp(&text, "%sEvent", name)->m_text, 0);
            }
            {
                AtTempText text;
                nameSpace = (const char*)FindValueOrDefault(AtFormatTemp(&text, "%sNameSpace", name)->m_text, 0);
            }
            int acted = 0;
            if (action != 0) {
                Slot49(action, 0, 1);
                acted = 1;
            }
            if (event != 0 && nameSpace != 0) {
                GmGameEvent posted;
                posted.Set(this, 0, 0, -1);
                GmEventManagerSend(GmEventManager::s_instance, event, nameSpace, &posted, 1);
                break;
            }
            if (acted) {
                break;
            }
        }
    }
    m_lastHealth = value;
}

// The AI's condition names, compared as eight-byte names, each with its "cN" negation. The base's answer (always 0) is what an unknown name, and a few paths below, return.
// Quirks kept: "cNCanSee" has no "cCanSee"; "cFront" answers the sight test's own value, the negations 0 or 1. "cNTgtAir" answers the base's 0, not 1, when the target is not the player. A
// condition on the combat object with none answers as the plain form's false (0) and the negation's true (1). "cAtkRg<n>" and "cNAtkRg<n>" compare the range with one digit after the prefix.
int FaAcNPC::TestCondition(const char* name) {
    const int result = GmAcNPC::TestCondition(name);
    FaCmCombat* combat = CombatOf(this);
    GmAcStat* health = combat != 0 ? combat->m_health : 0;
    if (AtNamesEqual(name, "cDead")) {
        return health != 0 && health->m_value <= 0.0f ? 1 : 0;
    }
    if (AtNamesEqual(name, "cAlive")) {
        if (health != 0 && !(health->m_value > 0.0f)) {
            return 0;
        }
        return 1;
    }
    if (AtNamesEqual(name, "cLowHP")) {
        return health != 0 && GmStatInRange(health, m_lowHealthRange) ? 1 : 0;
    }
    if (AtNamesEqual(name, "cMedHP")) {
        return health != 0 && GmStatInRange(health, m_medHealthRange) ? 1 : 0;
    }
    if (AtNamesEqual(name, "cHighHP")) {
        return health != 0 && GmStatInRange(health, m_highHealthRange) ? 1 : 0;
    }
    if (AtNamesEqual(name, "cThreat")) {
        return combat != 0 && combat->m_threat != 0 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cNThreat")) {
        return combat != 0 && combat->m_threat != 0 ? 0 : 1;
    }
    if (AtNamesEqual(name, "cNCanSee")) {
        return GmAcCharacterCanSee(this, GmAcTargetOf(this), 0, 0, 1, 0, 0, 0, 0, 0, 0) == 0 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cFront")) {
        return GmAcCharacterCanSee(this, GmAcTargetOf(this), 0, 1, 0, 0, 0, 0, 0, 0, 0);
    }
    if (AtNamesEqual(name, "cNFront")) {
        return GmAcCharacterCanSee(this, GmAcTargetOf(this), 0, 1, 0, 0, 0, 0, 0, 0, 0) == 0 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cTarget")) {
        return Unknown17C() != 0 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cNTarget")) {
        return Unknown17C() == 0 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cAtkRng")) {
        return combat != 0 && combat->m_attackRange != -1 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cNAtkRng")) {
        return combat != 0 && combat->m_attackRange != -1 ? 0 : 1;
    }
    if (AtCompareN(name, "cAtkRg", 6) == 0) {
        return combat != 0 && combat->m_attackRange == (signed char)name[6] - '0' ? 1 : 0;
    }
    if (AtCompareN(name, "cNAtkRg", 7) == 0) {
        return combat != 0 && combat->m_attackRange == (signed char)name[7] - '0' ? 0 : 1;
    }
    if (AtNamesEqual(name, "cTgtAir")) {
        GmAcActor* player = GmNthControlledCharacter((GmControlledCharacters*)GmDynamicNodeLoader::s_instance, 0);
        if (GmAcTargetOf(this) != player) {
            return result;
        }
        return GmAcFloat154NotZero(player);
    }
    if (AtNamesEqual(name, "cNTgtAir")) {
        GmAcActor* player = GmNthControlledCharacter((GmControlledCharacters*)GmDynamicNodeLoader::s_instance, 0);
        if (GmAcTargetOf(this) != player) {
            return result;
        }
        return GmAcFloat154NotZero(player) == 0 ? 1 : 0;
    }
    if (AtNamesEqual(name, "cTgtPlr")) {
        GmAcActor* player = GmNthControlledCharacter((GmControlledCharacters*)GmDynamicNodeLoader::s_instance, 0);
        return GmAcTargetOf(this) == player ? 1 : 0;
    }
    if (AtNamesEqual(name, "cNTgtPlr")) {
        GmAcActor* player = GmNthControlledCharacter((GmControlledCharacters*)GmDynamicNodeLoader::s_instance, 0);
        return GmAcTargetOf(this) != player ? 1 : 0;
    }
    if (AtNamesEqual(name, "cShield")) {
        GmAcStatsHolder* holder = (GmAcStatsHolder*)m_unknown270;
        if (holder == 0) {
            return result;
        }
        GmAcStat* shield = holder->m_stats->Find("sShield", 1);
        return shield != 0 && shield->m_value > 0.0f ? 1 : 0;
    }
    if (AtNamesEqual(name, "cNShield")) {
        GmAcStatsHolder* holder = (GmAcStatsHolder*)m_unknown270;
        if (holder == 0) {
            return result;
        }
        GmAcStat* shield = holder->m_stats->Find("sShield", 1);
        return shield != 0 && shield->m_value <= 0.0f ? 1 : 0;
    }
    return result;
}

FaAcNPC* FaAcNPC::Create(int count) {
    if (count <= 1) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(sizeof(FaAcNPC));
        FaAcNPC* npc = block != 0 ? new (block, Placement()) FaAcNPC : 0;
        AtPopHeap();
        if (npc == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        return npc;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    FaAcNPC* first = new FaAcNPC[count];
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}
