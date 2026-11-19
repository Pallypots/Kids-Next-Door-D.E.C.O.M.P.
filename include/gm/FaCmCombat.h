// The combat modules of the game's characters: FaCmCombat, and the playable characters' FaCmPlayerCombat, on GmCmCombat (include/gm/GmAcCharacter.h). Original address clusters
// 0x000A1C50-0x000A1CBF, 0x000A1F10-0x000A2397, 0x000A29F0-0x000A34E9, 0x000A3BE0, 0x000A46D0, 0x000A4CB0-0x000A4D9B, 0x001645D0-0x0016466C.
//
// Class names. FaCmCombat and FaCmPlayerCombat are RTTI names (locators 0x0029E0B0 and 0x0029E0FC, before vftables 0x00282EF4 and 0x00282F5C); the hierarchy descriptor gives FaCmPlayerCombat,
// FaCmCombat, GmCmCombat, single inheritance. Every other name is inferred, from the record words where there are any.
//
// What is recovered: the three classes' constructors and destructors, the record read (slot 2), Attach, Stop, slot 5, slot 7, and the weapon sets' making and releasing. The update (slot 6,
// 0x000A4BB0, 252 functions beneath it) is not, and stops with a diagnostic; so do the weapons' own slots, which nothing recovered makes.
#pragma once

#include "at/AtDataRegistry.h"
#include "gm/GmAcCharacter.h"

// What a weapon set holds. Made by the update, which is not recovered: only the slot the release calls is known.
class GmCmWeapon {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Release() = 0;         // slot 4, from FaCmCombat's ReleaseWeapons
};

// One of the record's WeaponSets, by name: room for as many weapons as the value of that name has elements. 0x10 bytes.
struct FaCmWeaponSet {
    char m_name[8];                     // +0x00 not terminated
    unsigned int m_count;               // +0x08
    GmCmWeapon** m_weapons;             // +0x0C
};

// One of NumAttackRanges: AttackRange%d, AttackRangeEnterEvent%d, AttackRangeExitEvent%d. 0x0C bytes.
struct FaCmAttackRange {
    AtDataComponent* m_range;           // +0x00 the value itself
    void* m_enterEvent;                 // +0x04 its first element, as the other words
    void* m_exitEvent;                  // +0x08
};

// ---- FaCmCombat: vftable 0x00282EF4, 8 slots, 0xE4 bytes ---------------------------------------------------------------------
class FaCmCombat : public GmCmCombat {
public:
    FaCmCombat();                       // 0x000A3360
    ~FaCmCombat() override;             // slot 0; 0x000A46D0 deleting, not for arrays; 0x000A3BE0 complete
    void Attach(GmAcActor* owner) override;  // slot 1, 0x000A33B0 GmCmCombat's (which reads the record), the health stat, the weapon sets
    int ReadRecord() override;          // slot 2, 0x000A1F10 the record's words; returns GmCmCombat's answer
    void Stop() override;               // slot 4, 0x000A3430 frees the weapon sets and attack ranges, GmCmCombat's, then Reset
    void Slot5() override;              // slot 5, 0x000A2310 releases the weapons, clears the state words, then GmCmCombat's
    void Update(int a) override;        // slot 6, 0x000A4BB0 not recovered: stops with a diagnostic
    int Slot7() override;               // slot 7, 0x000A3380 0 when the health stat is there and not above zero; 1 otherwise

    void Reset();                       // 0x000A2210 every own word but +0x60
    void MakeWeaponSets();              // 0x000A29F0 once: a set for each name in WeaponSets that is itself a value under the record
    void ReleaseWeapons();              // 0x000A1C50 each weapon held is released and let go

    AtDataComponent* m_weaponSets;      // +0x0C the record's values, where they are: WeaponSets, if it is an array of short names
    AtDataComponent* m_enemies;         // +0x10 Enemies
    AtDataComponent* m_enemyPriorities; // +0x14 EnemyPriorities, if it is an array of integers
    void* m_useEnemyPriorities;         // +0x18 UseEnemyPriorities
    void* m_threatByRadius;             // +0x1C ThreatByRadius
    void* m_threatBySightCone;          // +0x20 ThreatBySightCone
    void* m_threatEnterRange;           // +0x24 ThreatEnterRange
    void* m_threatExitRange;            // +0x28 ThreatExitRange
    int m_unknown2C;                    // +0x2C
    int m_unknown30;                    // +0x30
    int m_attackRangeCount;             // +0x34 NumAttackRanges, or 0
    FaCmAttackRange* m_attackRanges;    // +0x38
    void* m_lockOnMinRangeH;            // +0x3C LockOnMinRangeH
    void* m_lockOnMaxRange;             // +0x40 LockOnMaxRange
    void* m_lockOffMinRangeH;           // +0x44 LockOffMinRangeH
    void* m_lockOffMaxRange;            // +0x48 LockOffMaxRange
    void* m_maxNewLockOnAngleH;         // +0x4C MaxNewLockOnAngleH
    void* m_maxNextLockOnAngleH;        // +0x50 MaxNextLockOnAngleH
    void* m_maxAimAngleH;               // +0x54 MaxAimAngleH
    void* m_maxAimAngleV;               // +0x58 MaxAimAngleV
    void* m_canAttackSiblings;          // +0x5C CanAttackSiblings
    void* m_useStartPositionForAttackRange;  // +0x60 UseStartPositionForAttackRange; the one word Reset leaves
    void* m_minLookStickMagnitude;      // +0x64 MinLookStickMagnitude
    void* m_bumpTargetAngle;            // +0x68 BumpTargetAngle
    void* m_digitalBumpTargetting;      // +0x6C DigitalBumpTargetting
    void* m_analogBumpTargetting;       // +0x70 AnalogBumpTargetting
    void* m_updateCombatModule;         // +0x74 UpdateCombatModule
    int m_unknown78;                    // +0x78
    int m_unknown7C;                    // +0x7C
    GmAcStat* m_health;                 // +0x80 the owner's stat sHealth
    FaCmWeaponSet* m_weaponSetList;     // +0x84
    unsigned int m_weaponSetCount;      // +0x88
    int m_unknown8C;                    // +0x8C
    int m_unknown90;                    // +0x90 -1 when reset
    int m_unknown94;                    // +0x94
    int m_unknown98;                    // +0x98 -1
    int m_unknown9C;                    // +0x9C 1
    int m_unknownA0;                    // +0xA0 1
    int m_unknownA4;                    // +0xA4 -1
    int m_unknownA8[2];                 // +0xA8
    int m_unknownB0[8];                 // +0xB0 four pairs
    int m_unknownD0;                    // +0xD0 1
    int m_unknownD4[3];                 // +0xD4
    int m_unknownE0;                    // +0xE0 1

    static void* s_heap;                // 0x003B3C5C the attack ranges and weapon sets come from it
};

// ---- FaCmPlayerCombat: vftable 0x00282F5C, 8 slots, 0xE4 bytes ---------------------------------------------------------------
// Made by FaAcPlayer's slot 15. Its own slots are FaCmCombat's, called; Attach reads the record once more.
class FaCmPlayerCombat : public FaCmCombat {
public:
    FaCmPlayerCombat();                 // 0x000A4D00
    ~FaCmPlayerCombat() override;       // slot 0; 0x000A4D80 deleting, not for arrays; 0x000A4D30 complete
    void Attach(GmAcActor* owner) override;  // slot 1, 0x000A4CB0 FaCmCombat's, then ReadRecord again
    int ReadRecord() override;          // slot 2, 0x000A4CD0 FaCmCombat's
    void Stop() override;               // slot 4, 0x000A4D20 FaCmCombat's
    void Slot5() override;              // slot 5, 0x000A4CE0 FaCmCombat's
    void Update(int a) override;        // slot 6, 0x000A4CF0 FaCmCombat's
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(FaCmWeaponSet) == 0x10, "FaCmWeaponSet");
static_assert(sizeof(FaCmAttackRange) == 0x0C, "FaCmAttackRange");
static_assert(offsetof(FaCmCombat, m_weaponSets) == 0x0C, "FaCmCombat::m_weaponSets");
static_assert(offsetof(FaCmCombat, m_attackRanges) == 0x38, "FaCmCombat::m_attackRanges");
static_assert(offsetof(FaCmCombat, m_lockOnMinRangeH) == 0x3C, "FaCmCombat::m_lockOnMinRangeH");
static_assert(offsetof(FaCmCombat, m_updateCombatModule) == 0x74, "FaCmCombat::m_updateCombatModule");
static_assert(offsetof(FaCmCombat, m_health) == 0x80, "FaCmCombat::m_health");
static_assert(offsetof(FaCmCombat, m_unknown90) == 0x90, "FaCmCombat::m_unknown90");
static_assert(offsetof(FaCmCombat, m_unknownB0) == 0xB0, "FaCmCombat::m_unknownB0");
static_assert(offsetof(FaCmCombat, m_unknownE0) == 0xE0, "FaCmCombat::m_unknownE0");
static_assert(sizeof(FaCmCombat) == 0xE4, "FaCmCombat");
static_assert(sizeof(FaCmPlayerCombat) == 0xE4, "FaCmPlayerCombat");
#pragma pop_macro("offsetof")
#endif
