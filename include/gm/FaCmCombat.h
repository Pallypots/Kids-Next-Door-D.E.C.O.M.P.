// The combat modules of the game's characters: FaCmCombat, and the playable characters' FaCmPlayerCombat, on GmCmCombat (include/gm/GmAcCharacter.h). Original address clusters
// 0x000A1C50-0x000A1CBF, 0x000A1F10-0x000A2397, 0x000A29F0-0x000A34E9, 0x000A3BE0, 0x000A46D0, 0x000A4CB0-0x000A4D9B, 0x001645D0-0x0016466C.
//
// Class names. FaCmCombat and FaCmPlayerCombat are RTTI names (locators 0x0029E0B0 and 0x0029E0FC, before vftables 0x00282EF4 and 0x00282F5C); the hierarchy descriptor gives FaCmPlayerCombat,
// FaCmCombat, GmCmCombat, single inheritance. Every other name is inferred, from the record words where there are any.
//
// What is recovered: the three classes' constructors and destructors, the record read (slot 2), Attach, Stop, slot 5, slot 7, and the weapon sets' making and releasing (gm/plsetup); the
// update (slot 6, 0x000A4BB0) and its own functions in 0x000A1E10-0x000A4BAA, with the small helpers it alone needs (gm/combat, src/gm/FaCmCombatUpdate.cpp). What the update reaches in the
// scene, physics, input, effects and other classes' code stays behind seams (src/gm/CombatSeams.cpp) that stop with a diagnostic.
#pragma once

#include "at/AtDataRegistry.h"
#include "gm/GmAcCharacter.h"

// What a weapon set holds: an actor the update spawns (SpawnWeapons), whose slot 4 is GmAcActor's slot 4. Kept as its own class so that the release stays a virtual call, as in the original.
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
    void Update(int a) override;        // slot 6, 0x000A4BB0 the weapons spawned, the threat, TARGETME, the lock-on, the ZapLights, the target's name, the lock tag; the argument unused
    int Slot7() override;               // slot 7, 0x000A3380 0 when the health stat is there and not above zero; 1 otherwise

    void Reset();                       // 0x000A2210 every own word but +0x60
    void MakeWeaponSets();              // 0x000A29F0 once: a set for each name in WeaponSets that is itself a value under the record
    void ReleaseWeapons();              // 0x000A1C50 each weapon held is released and let go

    // The update and what it alone reaches in this class: src/gm/FaCmCombatUpdate.cpp (subsystem gm/combat). Names inferred from the events they send and the record words they read.
    void SpawnWeapons();                // 0x000A2BB0 each empty place in each weapon set: the actor its set's value names there, spawned, with "SPAWN"
    void UpdateThreat(int post);        // 0x000A4170 the threat among the enemies, its attack range and priority; with post, THREAT/UNTHREAT and the range's enter event, else THREATAC
    void UpdateTargetMe();              // 0x000A46F0 TARGETME to the target chosen unless the lock-on button holds the current one; TARGETNO when that is not the owner's target
    void UpdateLockOn();                // 0x000A48B0 keeps or drops the owner's target, chooses another, the bump tags, HAVETRGT/LOSTTRGT
    void UpdateZap();                   // 0x000A30D0 counts the ZapLight actors within their radius: ZAPSTART/ZAPEND
    void UpdateLockTag();               // 0x000A24E0 the right stick's tag, TAGL_DN or TAGR_DN, sent when it changes
    void ReadLockStick(char* tag, int axisX, int axisY);  // 0x000A23A0 LOCKL_DN or LOCKR_DN by the stick's angle, or an empty name
    void UpdateBumpTag();               // 0x000A34F0 a stick's tag, sent when it changes, by DigitalBumpTargetting or AnalogBumpTargetting
    // 0x000A2620. Whether target is a threat (by ThreatByRadius's ranges, or seen by ThreatBySightCone) nearer than *best when nearest is set; if so *isThreat = 1, *best = the distance and
    // *range the attack range it is in, -1 for none. wasThreat and oldRange are last frame's, for the ranges' hysteresis.
    void TestThreat(GmAcActor* target, int wasThreat, int oldRange, int* isThreat, int* range, float* best, int nearest);
    int IsAlive(GmAcActor* target);     // 0x000A2810 0 only for a character that is dead or whose combat module says it cannot be attacked
    void PostThreat(int changed, int wasThreat, int oldRange, GmAcActor* threat, int priority);  // 0x000A28A0
    void TargetPosition(AtVector3* out, GmAcActor* target);  // 0x000A2D40 its aim node's centre, or its position; the owner's target when null
    int IsEnemy(GmAcActor* target, int listed);  // 0x000A3790 not the owner; of a type in the enemy list when listed; not a sibling unless CanAttackSiblings
    int InRange(float minRangeH, float maxRange, GmAcActor* target);  // 0x000A38E0 at least minRangeH across, at most maxRange in all
    int InAimCone(int checkH, float maxAngleH, GmAcActor* target);    // 0x000A39D0 within MaxAimAngleV up and down, and within maxAngleH across when checkH
    int InLockOnRange(GmAcActor* target);   // 0x000A3B40 LockOnMinRangeH (300), LockOnMaxRange (1000)
    int InLockOffRange(GmAcActor* target);  // 0x000A3B90 LockOffMinRangeH (150), LockOffMaxRange (1200)
    // 0x000A3C30. The enemy to lock on to: among the enemies in lock-on range that answer LOCKON? and can be seen, by angle from the facing (side 1 or 2 measures it round one way), by
    // distance when side is 0; keepFacing leaves the facing alone rather than turning it to the current target. See the source for the quirks.
    GmAcActor* ChooseTarget(int side, int keepFacing, float maxAngleH, int nearestFallback);
    double MaxAimAngleH();              // 0x000A1E90 in radians, 45 degrees by default
    double ThreatEnterRange();          // 0x000A1ED0 m_unknown2C's, ThreatEnterRange's, or 1000
    double ThreatExitRange();           // 0x000A1EF0 m_unknown30's, ThreatExitRange's, or 1500

    AtDataComponent* m_weaponSets;      // +0x0C the record's values, where they are: WeaponSets, if it is an array of short names
    AtDataComponent* m_enemies;         // +0x10 Enemies
    AtDataComponent* m_enemyPriorities; // +0x14 EnemyPriorities, if it is an array of integers
    void* m_useEnemyPriorities;         // +0x18 UseEnemyPriorities
    void* m_threatByRadius;             // +0x1C ThreatByRadius
    void* m_threatBySightCone;          // +0x20 ThreatBySightCone
    void* m_threatEnterRange;           // +0x24 ThreatEnterRange
    void* m_threatExitRange;            // +0x28 ThreatExitRange
    int m_unknown2C;                    // +0x2C a value (float*) used instead of ThreatEnterRange when set; nothing here sets it
    int m_unknown30;                    // +0x30 likewise for ThreatExitRange
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
    // From here the meanings are the update's (gm/combat); the names are kept, being others' too.
    int m_unknown78;                    // +0x78 an enemy list (AtDataShortNameArray*) used instead of Enemies when set; nothing here sets it
    int m_unknown7C;                    // +0x7C likewise for EnemyPriorities (AtDataIntArray*)
    GmAcStat* m_health;                 // +0x80 the owner's stat sHealth
    FaCmWeaponSet* m_weaponSetList;     // +0x84
    unsigned int m_weaponSetCount;      // +0x88
    int m_unknown8C;                    // +0x8C
    int m_unknown90;                    // +0x90 -1 when reset
    int m_threat;                       // +0x94 1 while there is a threat ("cThreat" when not 0; gm/gmnpc's name)
    int m_attackRange;                  // +0x98 -1: the attack range the threat is in ("cAtkRg<n>"; gm/gmnpc's name)
    int m_unknown9C;                    // +0x9C 1: the attack ranges' enter events are sent
    int m_unknownA0;                    // +0xA0 1: others may take this owner for a threat
    int m_unknownA4;                    // +0xA4 -1: the threat's enemy priority
    int m_unknownA8[2];                 // +0xA8 [0] an enemy's answer to LOCKON?, which its script sets; [1] TARGETME is sent, and the lock-on may choose without the PCNPC
    int m_unknownB0[8];                 // +0xB0 four eight-byte names: +0xB8 the digital bump tag, +0xC0 the analog one, +0xC8 the lock tag
    int m_unknownD0;                    // +0xD0 1: the lock tag is not sent (not under control last frame)
    int m_unknownD4[3];                 // +0xD4 [0] ZapLights in reach last frame; [1] the target's note bank goes to the display; [2] HAVETRGT sent
    int m_unknownE0;                    // +0xE0 1: LOSTTRGT sent

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

// ---- what the update uses from other classes (gm/combat) -----------------------------------------------------------------------
class AtSceneNode;
class GmDynamicNodeLoader;

// RTTI GmEvDescription: vftable 0x0027B304, one slot (0x00152680, deleting; its complete destructor 0x000A6CC0 only stores this vftable). The base of the events actors are sent through slot 49.
class GmEvDescription {
public:
    GmEvDescription() {}
    virtual ~GmEvDescription() {}
};

// An event that names an actor: vftable 0x0028EBCC (its one slot is GmEvDescription's 0x00152680, folded), 0x28 bytes. Made on the stack by the update and handed to slot 49 as its second
// argument. The name GmGameEvent is the gmnpc branch's (inferred); the words' meanings are not established.
class GmGameEvent : public GmEvDescription {
public:
    GmGameEvent();                      // 0x00152590 +0x10 is 1, the name empty, every other word 0
    void Set(GmAcActor* actor, int a, int b, int c);  // 0x00152780 the name emptied again

    int m_unknown04;                    // +0x04
    int m_unknown08;                    // +0x08 Set's last argument (-1 from the update)
    int m_unknown0C;                    // +0x0C
    int m_unknown10;                    // +0x10 1
    GmAcActor* m_actor;                 // +0x14
    int m_unknown18;                    // +0x18
    int m_unknown1C;                    // +0x1C
    char m_name[8];                     // +0x20
};

// Walks the dynamic node loader's actors by name: 0x1C bytes on the stack. In GmCmCombat's object file (0x00164670-0x001647E3). Find and Next search the loader's tables (0x00070A00,
// 0x000703F0) and are seams; the rest is recovered. The users read m_entry (none left when null) and m_item, whose first word is the actor.
struct GmDynamicNodeIterator {
    GmDynamicNodeIterator();            // 0x00164670 every word and the name's first byte 0
    ~GmDynamicNodeIterator();           // 0x00164740 likewise
    int Begin(GmDynamicNodeLoader* loader);  // 0x00164690 the loader, the rest cleared if there was one before; 0
    void Find(const char* name);        // 0x00164760 seam: the first actor of that name, or of any when null
    void Next();                        // 0x001646C0 seam

    GmDynamicNodeLoader* m_loader;      // +0x00
    void* m_bucket;                     // +0x04
    void* m_entry;                      // +0x08
    GmAcActor** m_item;                 // +0x0C
    int m_named;                        // +0x10
    char m_name[8];                     // +0x14
};

// What the input's button lookup gives back, by value: two words. In other classes' ranges: IsDown 0x00075300, the destructor 0x0014BBB0 (in GmAcNode's range; a folded body, probably).
struct GmButtonState {
    ~GmButtonState();                   // 0x0014BBB0 both words 0
    int IsDown();                       // 0x00075300 the state is 1 or 3

    int m_state;                        // +0x00
    int m_unknown04;                    // +0x04
};

// The object at 0x003B353C, code 0x000AFD00-0x000B0E00: only this setter is used here. Name inferred (it shows the target's name).
struct FaHud {
    void SetTargetName(const char* name);   // 0x000B06D0 nothing for an empty name; else the name, and +0x1290 1

    char m_unknown0000[0x1290];
    int m_targetNameSet;                // +0x1290
    char m_targetName[8];               // +0x1294

    static FaHud* s_instance;           // 0x003B353C
};

// The object at 0x003B3A3C: only its spawning is used here, a seam. Name inferred.
struct GmSpawnManager {
    GmAcActor* Spawn(const char* name);  // 0x00153CA0 seam

    static GmSpawnManager* s_instance;  // 0x003B3A3C
};

// 0x000920D0. The actor's slot 53 word as an actor, when it is one (m_flags98 has kActorReset); else null. Slot 53 is asked three times. The same function is recovered on the gmnpc branch,
// with this name and signature, in src/gm/FaAcNPC.cpp: one of the two definitions goes at integration.
GmAcActor* GmAcTargetOf(GmAcActor* actor);

// 0x00013E50: the first n characters compared ignoring case (the C library's tolower, 0x00125DCF), null as empty; 0 when equal, else the difference of the first unequal characters as they
// are, signed. Its own register convention in the original (the names in ecx and eax). 0x00013F50 is it with n = 8.
int AtCompareNoCaseN(const char* a, const char* b, int n);
int AtCompareNoCase8(const char* a, const char* b);

// ---- seams: other classes' functions the update calls, not recovered. Each stops with a diagnostic; src/gm/CombatSeams.cpp. ----------------
// 0x00130E50, a method of GmAcCharacter: whether it sees target; ten arguments, the fourth a float* (the distance) from TestThreat. The gmnpc branch declares it with an int there.
int GmAcCharacterCanSee(GmAcCharacter* self, GmAcActor* target, int a, int b, int c, float* d, int e, int f, int g, int h, int i);
// 0x00093300 and 0x00093350, methods of FaAcPlayer on the controller's owner: the stick of two axes (through the owner's slot 59 and the input's AxisValue), and its angle in degrees.
void FaAcPlayerReadStick(GmAcActor* player, float* out, int axisX, int axisY);
float FaAcPlayerStickAngle(GmAcActor* player, int unused, int axisX, int axisY);
// 0x00169730: an actor attached beneath `actor` by name ("PCNPC"), or null. cdecl.
GmAcActor* GmAcFindAttached(GmAcActor* actor, const char* name);
// 0x0014C300, a method of GmAcNode: the scene node to aim at, by the scene node's class (11 to 13 through the jointed actor, 2 and 3 itself), or null.
AtSceneNode* GmAcNodeAimNode(GmAcNode* node);

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
