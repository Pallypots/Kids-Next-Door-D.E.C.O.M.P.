// The combat modules: GmCmCombat (include/gm/GmAcCharacter.h), FaCmCombat and FaCmPlayerCombat (include/gm/FaCmCombat.h). Names other than the RTTI names are inferred; record words are
// the files' own.
#include "gm/FaCmCombat.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);   // 0x00013F70
void AtCopyName9(char* to, const char* from);   // 0x00013F90
int AtFormat(char* out, const char* format, ...);  // the C library's sprintf (0x00125559); src/at/AtIoFormatHost.cpp

// ---- GmCmCombat ------------------------------------------------------------------------------------------------------------
GmCmCombat::GmCmCombat() {
    m_owner = 0;
    m_unknown08 = 1;
}

GmCmCombat::~GmCmCombat() {
    m_owner = 0;
    m_unknown08 = 1;
}

void GmCmCombat::Attach(GmAcActor* owner) {
    m_owner = owner;
    ReadRecord();
}

int GmCmCombat::ReadRecord() {
    return 0;
}

void GmCmCombat::Slot3() {}

void GmCmCombat::Stop() {
    m_owner = 0;
    m_unknown08 = 1;
}

void GmCmCombat::Slot5() {
    m_unknown08 = 1;
}

void GmCmCombat::Update(int) {}

int GmCmCombat::Slot7() {
    return 1;
}

// ---- FaCmCombat ------------------------------------------------------------------------------------------------------------
void* FaCmCombat::s_heap;

namespace {
// The original has this inline at each use: no heap is reported and then pushed regardless; no memory is reported after the pop, and the caller carries on.
void* NewFromCombatHeap(unsigned int size) {
    if (FaCmCombat::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(FaCmCombat::s_heap);
    void* block = operator new(size);
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return block;
}
}  // namespace

FaCmCombat::FaCmCombat() {
    Reset();
}

// Quirk kept: Stop is run again here after FaCmPlayerCombat's destructor has run it.
FaCmCombat::~FaCmCombat() {
    FaCmCombat::Stop();
}

// Quirk kept: m_useStartPositionForAttackRange (+0x60) is not cleared.
void FaCmCombat::Reset() {
    m_unknown98 = -1;
    m_unknownA4 = -1;
    m_unknown90 = -1;
    m_weaponSets = 0;
    m_enemies = 0;
    m_enemyPriorities = 0;
    m_useEnemyPriorities = 0;
    m_threatByRadius = 0;
    m_threatBySightCone = 0;
    m_threatEnterRange = 0;
    m_threatExitRange = 0;
    m_unknown2C = 0;
    m_unknown30 = 0;
    m_attackRangeCount = 0;
    m_attackRanges = 0;
    m_lockOnMinRangeH = 0;
    m_lockOnMaxRange = 0;
    m_lockOffMinRangeH = 0;
    m_lockOffMaxRange = 0;
    m_maxNewLockOnAngleH = 0;
    m_maxNextLockOnAngleH = 0;
    m_maxAimAngleH = 0;
    m_maxAimAngleV = 0;
    m_canAttackSiblings = 0;
    m_minLookStickMagnitude = 0;
    m_bumpTargetAngle = 0;
    m_digitalBumpTargetting = 0;
    m_analogBumpTargetting = 0;
    m_updateCombatModule = 0;
    m_unknown78 = 0;
    m_unknown7C = 0;
    m_health = 0;
    m_unknown94 = 0;
    m_weaponSetList = 0;
    m_weaponSetCount = 0;
    m_unknown8C = 0;
    m_unknownA8[0] = 0;
    m_unknownA8[1] = 0;
    m_unknown9C = 1;
    m_unknownA0 = 1;
    for (int i = 0; i < 8; i++) {
        m_unknownB0[i] = 0;
    }
    m_unknownD0 = 1;
    m_unknownD4[0] = 0;
    m_unknownD4[1] = 0;
    m_unknownD4[2] = 0;
    m_unknownE0 = 1;
}

// GmCmCombat's Attach reads the record (through the vftable, so a FaCmPlayerCombat's); then the owner's health stat is set from its base, held within its bounds and copied back, as the
// stat's own setter does inline; then the weapon sets. Quirks kept: the owner's stats (+0x270) are not tested. The stat is clamped to its maximum first and then its minimum, so a stat whose
// minimum is above its maximum ends at the minimum; a NaN stays.
void FaCmCombat::Attach(GmAcActor* owner) {
    GmCmCombat::Attach(owner);
    GmAcStat* health = ((GmAcStatsHolder*)owner->m_unknown270)->m_stats->Find("sHealth", 0);
    m_health = health;
    if (health != 0) {
        health->m_value = health->m_base;
        health->m_changed = 1;
        float value = health->m_value;
        if (value > health->m_max) {
            value = health->m_max;
        }
        if (value < health->m_min) {
            value = health->m_min;
        }
        health->m_value = value;
        if (health->m_changed != 0) {
            health->m_base = health->m_value;
        }
    }
    MakeWeaponSets();
}

// Quirks kept: WeaponSets and EnemyPriorities found with the wrong type leave what was there. A second read makes the attack ranges again, and the first are not freed. A failed allocation is
// reported and then written through.
int FaCmCombat::ReadRecord() {
    char name[0x100];
    const int answer = GmCmCombat::ReadRecord();
    AtDataComponent* sets = m_owner->FindComponent("WeaponSets", 0);
    if (sets != 0 && sets->m_descriptor->m_type == AtDataRegistry::kShortNameArray) {
        m_weaponSets = sets;
    }
    m_enemies = m_owner->FindComponent("Enemies", 0);
    m_useEnemyPriorities = m_owner->FindValueOrDefault("UseEnemyPriorities", 0);
    AtDataComponent* priorities = m_owner->FindComponent("EnemyPriorities", 0);
    if (priorities != 0 && priorities->m_descriptor->m_type == AtDataRegistry::kIntArray) {
        m_enemyPriorities = priorities;
    }
    m_threatByRadius = m_owner->FindValueOrDefault("ThreatByRadius", 0);
    m_threatBySightCone = m_owner->FindValueOrDefault("ThreatBySightCone", 0);
    m_threatEnterRange = m_owner->FindValueOrDefault("ThreatEnterRange", 0);
    m_threatExitRange = m_owner->FindValueOrDefault("ThreatExitRange", 0);
    m_lockOnMinRangeH = m_owner->FindValueOrDefault("LockOnMinRangeH", 0);
    m_lockOnMaxRange = m_owner->FindValueOrDefault("LockOnMaxRange", 0);
    m_lockOffMinRangeH = m_owner->FindValueOrDefault("LockOffMinRangeH", 0);
    m_lockOffMaxRange = m_owner->FindValueOrDefault("LockOffMaxRange", 0);
    m_maxNewLockOnAngleH = m_owner->FindValueOrDefault("MaxNewLockOnAngleH", 0);
    m_maxNextLockOnAngleH = m_owner->FindValueOrDefault("MaxNextLockOnAngleH", 0);
    m_maxAimAngleH = m_owner->FindValueOrDefault("MaxAimAngleH", 0);
    m_maxAimAngleV = m_owner->FindValueOrDefault("MaxAimAngleV", 0);
    m_canAttackSiblings = m_owner->FindValueOrDefault("CanAttackSiblings", 0);
    m_useStartPositionForAttackRange = m_owner->FindValueOrDefault("UseStartPositionForAttackRange", 0);
    m_bumpTargetAngle = m_owner->FindValueOrDefault("BumpTargetAngle", 0);
    m_minLookStickMagnitude = m_owner->FindValueOrDefault("MinLookStickMagnitude", 0);
    m_digitalBumpTargetting = m_owner->FindValueOrDefault("DigitalBumpTargetting", 0);
    m_analogBumpTargetting = m_owner->FindValueOrDefault("AnalogBumpTargetting", 0);
    m_updateCombatModule = m_owner->FindValueOrDefault("UpdateCombatModule", 0);
    const int* count = (const int*)m_owner->FindValueOrDefault("NumAttackRanges", 0);
    m_attackRangeCount = count != 0 ? *count : 0;
    if (m_attackRangeCount > 0) {
        m_attackRanges = (FaCmAttackRange*)NewFromCombatHeap(m_attackRangeCount * sizeof(FaCmAttackRange));
        for (int i = 0; i < m_attackRangeCount; i++) {
            AtFormat(name, "AttackRange%d", i);
            m_attackRanges[i].m_range = m_owner->FindComponent(name, 1);
            AtFormat(name, "AttackRangeEnterEvent%d", i);
            m_attackRanges[i].m_enterEvent = m_owner->FindValueOrDefault(name, 0);
            AtFormat(name, "AttackRangeExitEvent%d", i);
            m_attackRanges[i].m_exitEvent = m_owner->FindValueOrDefault(name, 0);
        }
    }
    return answer;
}

// Each name of WeaponSets is copied as eight characters, terminated, and looked for as a value under the owner's record; a set found takes its name and room for as many weapons as that value
// has elements, all null. Quirks kept: nothing happens once the list is made, or without WeaponSets, or with none in it. A set not found is left nameless with no room. A failed allocation is
// reported and then written through.
void FaCmCombat::MakeWeaponSets() {
    if (m_weaponSetList != 0 || m_weaponSets == 0) {
        return;
    }
    const unsigned int count = m_weaponSets->m_descriptor->m_count;
    if (count == 0) {
        return;
    }
    m_weaponSetCount = count;
    m_weaponSetList = (FaCmWeaponSet*)NewFromCombatHeap(m_weaponSetCount * sizeof(FaCmWeaponSet));
    const char(*names)[8] = static_cast<AtDataShortNameArray*>(m_weaponSets)->m_data;
    for (unsigned int i = 0; i < m_weaponSetCount; i++) {
        char name[8];
        char terminated[12];
        AtCopyName8(name, names[i]);
        FaCmWeaponSet* set = &m_weaponSetList[i];
        for (int k = 0; k < 8; k++) {
            set->m_name[k] = 0;
        }
        set->m_count = 0;
        set->m_weapons = 0;
        AtCopyName9(terminated, name);
        AtDataComponent* value = m_owner->FindComponent(terminated, 0);
        if (value != 0) {
            AtCopyName8(set->m_name, name);
            set->m_count = value->m_descriptor->m_count;
            set->m_weapons = (GmCmWeapon**)NewFromCombatHeap(set->m_count * sizeof(GmCmWeapon*));
            for (unsigned int j = 0; j < set->m_count; j++) {
                set->m_weapons[j] = 0;
            }
        }
    }
}

// Quirk kept: the original tests each set's address for null, which it cannot be.
void FaCmCombat::ReleaseWeapons() {
    if (m_weaponSetList == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_weaponSetCount; i++) {
        FaCmWeaponSet* set = &m_weaponSetList[i];
        for (unsigned int j = 0; j < set->m_count; j++) {
            if (set->m_weapons[j] != 0) {
                set->m_weapons[j]->Release();
                set->m_weapons[j] = 0;
            }
        }
    }
}

// Quirk kept: the weapons are let go of, not released or deleted; the list's pointer is left for Reset to clear.
void FaCmCombat::Stop() {
    if (m_weaponSetList != 0) {
        for (unsigned int i = 0; i < m_weaponSetCount; i++) {
            FaCmWeaponSet* set = &m_weaponSetList[i];
            if (set->m_weapons != 0) {
                for (unsigned int j = 0; j < set->m_count; j++) {
                    set->m_weapons[j] = 0;
                }
                operator delete(set->m_weapons);
                set->m_weapons = 0;
            }
        }
        operator delete(m_weaponSetList);
    }
    if (m_attackRanges != 0) {
        operator delete(m_attackRanges);
    }
    GmCmCombat::Stop();
    Reset();
}

void FaCmCombat::Slot5() {
    ReleaseWeapons();
    m_unknown98 = -1;
    m_unknownA4 = -1;
    m_unknown2C = 0;
    m_unknown30 = 0;
    m_unknown78 = 0;
    m_unknown7C = 0;
    m_unknown94 = 0;
    m_unknown9C = 1;
    m_unknownA0 = 1;
    for (int i = 0; i < 8; i++) {
        m_unknownB0[i] = 0;
    }
    m_unknownE0 = 1;
    m_unknownD4[0] = 0;
    m_unknownD4[1] = 0;
    m_unknownD4[2] = 0;
    GmCmCombat::Slot5();
}

// A NaN health is dead: the original's test is that the value is not above zero.
int FaCmCombat::Slot7() {
    if (m_health != 0 && !(m_health->m_value > 0.0f)) {
        return 0;
    }
    return 1;
}

// ---- FaCmPlayerCombat --------------------------------------------------------------------------------------------------------
FaCmPlayerCombat::FaCmPlayerCombat() {}

FaCmPlayerCombat::~FaCmPlayerCombat() {
    FaCmCombat::Stop();
}

// Quirk kept: the record is read twice, once from GmCmCombat's Attach and again here; the attack ranges of the first read are lost.
void FaCmPlayerCombat::Attach(GmAcActor* owner) {
    FaCmCombat::Attach(owner);
    ReadRecord();
}

int FaCmPlayerCombat::ReadRecord() {
    return FaCmCombat::ReadRecord();
}

void FaCmPlayerCombat::Stop() {
    FaCmCombat::Stop();
}

void FaCmPlayerCombat::Slot5() {
    FaCmCombat::Slot5();
}

void FaCmPlayerCombat::Update(int a) {
    FaCmCombat::Update(a);
}
