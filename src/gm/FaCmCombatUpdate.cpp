// FaCmCombat's update (slot 6, 0x000A4BB0) and the functions of its own that it reaches: the weapons spawned, the threat, the lock-on and its target, the ZapLights, the tags the sticks give.
// With the small helpers it alone needs from other ranges: the events, the node iterator, the button state, the display's target name, the angle between vectors. GmAcActor::GetNoteBank
// and the case-blind compare are in src/gm/CombatNames.cpp.
// Original address clusters 0x000A1E10-0x000A1F0A, 0x000A23A0-0x000A25B0, 0x000A2620-0x000A29EC, 0x000A2BB0-0x000A2DB8, 0x000A30D0-0x000A3216, 0x000A34F0-0x000A35EA,
// 0x000A3790-0x000A3BD5, 0x000A3C30-0x000A45A2, 0x000A46F0-0x000A4CAD; and 0x00013E50, 0x00013F50, 0x00075300, 0x000920D0, 0x00092C50, 0x000B06D0, 0x0014BBB0, 0x00152590, 0x00152780,
// 0x00164670-0x00164753. Names are inferred, from the events sent and the record words read (include/gm/FaCmCombat.h).
//
// Arithmetic: `double` is what the original keeps in an x87 register, `float` what it stores (include/at/AtVector3.h). The comparisons keep the original's sense on unordered values: each is
// written so that a NaN goes the way the original's flag test sends it.
#include "gm/FaCmCombat.h"
#include "gm/GmAcPlayer.h"
#include "gm/GmLevelManifest.h"
#include "at/AtScene.h"

void AtCopyName(const char* from, char* to);    // 0x00013ED0 up to eight characters, stopping after a terminator
void AtCopyName8(char* to, const char* from);   // 0x00013F70 eight bytes, or an empty name if `from` is null
void AtCopyName9(char* to, const char* from);   // 0x00013F90 eight bytes then a terminator; null means empty
int AtNamesEqual(const char* a, const char* b); // 0x00013FC0
int AtNameLength(const char* name);             // 0x00014030 at most eight
double AtAcos(double value);                    // the C library's acos, 0x00126600
extern "C" double sqrt(double value);
#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

namespace {
typedef decltype(sizeof(0)) Size;

const float kDegrees = 0.0174532924f;           // 0x0027B558, pi / 180 as a float
const float kFloatMax = 3.40282347e+38f;        // 0x7F7FFFFF
const float kTwoPi = 6.28318548f;               // 0x0027715C
const float kPi = 3.14159274f;                  // 0x002719C4
const unsigned int kPlayerFlag = 0x100;         // GmAcPlayer::kPlayerReset in m_flags98
const unsigned int kCharacterFlags = 0x6100;    // the flags a character's shutdown and kin leave: a character, whose +0x2E4 is its combat module
const unsigned int kObjectFlag = 0x400;         // GmAcObject::kObjectReset

// The owner's target as the original tests it inline wherever it is used: slot 53 asked up to three times, an actor only when its word has kActorReset.
GmAcActor* TargetOf(GmAcActor* actor) {
    if (actor->Unknown17C() != 0 && (((GmAcActor*)(Size)actor->Unknown17C())->m_flags98 & GmAcNode::kActorReset) != 0) {
        return (GmAcActor*)(Size)actor->Unknown17C();
    }
    return 0;
}

// A scene node's centre in the world, its body finished first if it has moved: inline in the original wherever used.
const AtVector3& CentreOf(AtSceneNode* node) {
    if ((node->m_flags & AtSceneNode::kMoved) != 0) {
        node->FinishBody();
    }
    return node->m_worldSphere.center;
}

int EventArgument(GmGameEvent* event) { return (int)(Size)event; }

// AtVector3::Distance (0x00042F60) as it comes back in the x87 register, before anything stores it: the original compares it so in three places. The recovered Distance returns it rounded to
// float, which is what every other use stores; this is the same arithmetic without that rounding (include/at/AtVector3.h: double for what stays in a register).
double DistanceInRegister(const AtVector3& from, const AtVector3& to) {
    const double dx = (double)to.x - from.x;
    const double dy = (double)to.y - from.y;
    const double dz = (double)to.z - from.z;
    return sqrt(dz * dz + dy * dy + dx * dx);
}

// The inline strcmp of IsEnemy, on the original's two eight-byte buffers, which are not terminated: a name of eight characters reads on past its buffer, the first into the second. Kept as one
// array of the two so that the first reads into the second as there; past the second is the original's other stack, not reproduced (the oracle does not compare that case).
int CompareCopies(const char* a, const char* b) {
    for (;;) {
        if (*a != *b) {
            return (unsigned char)*a < (unsigned char)*b ? -1 : 1;
        }
        if (*a == 0) {
            return 0;
        }
        a++;
        b++;
    }
}
}  // namespace

FaHud* FaHud::s_instance;
GmSpawnManager* GmSpawnManager::s_instance;

// ---- small helpers ----------------------------------------------------------------------------------------------------------------
// GmAcTargetOf (0x000920D0) is gm/gmnpc's, src/gm/FaAcNPC.cpp.

// Quirk kept: the dot product and its magnitude are each rounded to float; the magnitude is compared with 1 and the dot product itself with 0.
double AtVector3::AngleTo(const AtVector3& other) const {
    const float dot = (float)((double)other.z * z + (double)other.y * y + (double)other.x * x);
    const float magnitude = dot < 0.0f ? -dot : dot;
    if (magnitude <= 1.0f) {
        return AtAcos(dot);
    }
    if (!(dot > 0.0f)) {
        return kPi;
    }
    return 0.0f;
}

GmButtonState::~GmButtonState() {
    m_state = 0;
    m_unknown04 = 0;
}

int GmButtonState::IsDown() {
    return m_state == 1 || m_state == 3 ? 1 : 0;
}

void FaHud::SetTargetName(const char* name) {
    if (name[0] != 0) {
        AtCopyName8(m_targetName, name);
        m_targetNameSet = 1;
    }
}

GmGameEvent::GmGameEvent() {
    m_unknown04 = 0;
    m_unknown0C = 0;
    m_unknown08 = 0;
    m_actor = 0;
    m_unknown18 = 0;
    m_unknown1C = 0;
    m_unknown10 = 1;
    AtCopyName("", m_name);
}

void GmGameEvent::Set(GmAcActor* actor, int a, int b, int c) {
    m_unknown18 = 0;
    m_unknown1C = 0;
    AtCopyName("", m_name);
    m_actor = actor;
    m_unknown08 = c;
    m_unknown18 = a;
    m_unknown1C = b;
}

GmDynamicNodeIterator::GmDynamicNodeIterator() {
    m_loader = 0;
    m_bucket = 0;
    m_entry = 0;
    m_item = 0;
    m_named = 0;
    m_name[0] = 0;
}

GmDynamicNodeIterator::~GmDynamicNodeIterator() {
    m_loader = 0;
    m_bucket = 0;
    m_entry = 0;
    m_item = 0;
    m_named = 0;
    m_name[0] = 0;
}

int GmDynamicNodeIterator::Begin(GmDynamicNodeLoader* loader) {
    if (m_loader != 0) {
        m_loader = 0;
        m_bucket = 0;
        m_entry = 0;
        m_item = 0;
        m_named = 0;
        m_name[0] = 0;
    }
    m_loader = loader;
    return 0;
}

// ---- GmAcActor's type, in this range (GetNoteBank is in src/gm/CombatNames.cpp) ---------------------------------------------------------
int GmAcActor::IsOfType(const char* type) {
    const char* own = AtNameLength((const char*)m_unknown264) > 0 ? (const char*)m_unknown264 : (const char*)m_unknown240[0];
    if (own == 0) {
        return 0;
    }
    own = AtNameLength((const char*)m_unknown264) > 0 ? (const char*)m_unknown264 : (const char*)m_unknown240[0];
    return AtCompareNoCase8(type, own) == 0 ? 1 : 0;
}

// ---- the record's values with their defaults -----------------------------------------------------------------------------------------
double FaCmCombat::MaxAimAngleH() {
    if (m_maxAimAngleH != 0) {
        return (double)*(const float*)m_maxAimAngleH * kDegrees;
    }
    return 45.0 * kDegrees;
}

double FaCmCombat::ThreatEnterRange() {
    if (m_unknown2C != 0) {
        return *(const float*)(Size)m_unknown2C;
    }
    if (m_threatEnterRange != 0) {
        return *(const float*)m_threatEnterRange;
    }
    return 1000.0f;
}

double FaCmCombat::ThreatExitRange() {
    if (m_unknown30 != 0) {
        return *(const float*)(Size)m_unknown30;
    }
    if (m_threatExitRange != 0) {
        return *(const float*)m_threatExitRange;
    }
    return 1500.0f;
}

// ---- the update ------------------------------------------------------------------------------------------------------------------------
// Quirks kept: the display is read before anything is called. "Under control" is a character with a controller and without the player flag. The lock tag is sent only after a frame under
// control (m_unknownD0 is cleared after it, and set by a frame not under control).
void FaCmCombat::Update(int) {
    if (m_unknown08 != 0) {
        SpawnWeapons();
        m_unknown08 = 0;
    }
    if (m_updateCombatModule != 0 && *(const int*)m_updateCombatModule == 0) {
        return;
    }
    GmAcCharacter* owner = (GmAcCharacter*)m_owner;
    int controlled;
    if ((owner->m_flags98 & kPlayerFlag) == 0 && owner->m_controller != 0) {
        controlled = 1;
    } else {
        controlled = 0;
        m_unknownD0 = 1;
    }
    FaHud* hud = FaHud::s_instance;
    if (m_owner->GmAcNode::Slot27() != 0 && m_owner->Slot27() != 0 && m_owner->Slot55() != 0) {
        UpdateThreat(1);
        if (m_unknownA8[1] != 0) {
            UpdateTargetMe();
        }
        if (controlled != 0) {
            UpdateLockOn();
        }
        UpdateZap();
        if (m_unknownD4[1] != 0 && GmAcTargetOf(m_owner) != 0) {
            char name[8];
            GmAcTargetOf(m_owner)->GetNoteBank(name);
            hud->SetTargetName(name);
        }
    }
    if (controlled != 0) {
        UpdateLockTag();
        m_unknownD0 = 0;
    }
}

// Each empty place of each set whose name is a value under the record: the name that value has there, unless empty, spawned; the actor goes in the place, is set up through its slots 4 and
// 32, and is sent SPAWN with an event naming the owner. Quirks kept: the set's address is tested for null; an empty place is cleared again before its name is read.
void FaCmCombat::SpawnWeapons() {
    if (m_weaponSetList == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_weaponSetCount; i++) {
        FaCmWeaponSet* set = &m_weaponSetList[i];
        if (set == 0) {
            continue;
        }
        char setName[12];
        AtCopyName9(setName, set->m_name);
        AtDataShortNameArray* names = (AtDataShortNameArray*)m_owner->FindValueObject(setName, 0);
        if (names == 0) {
            continue;
        }
        GmGameEvent event;
        event.Set(m_owner, 0, 0, -1);
        for (unsigned int j = 0; j < set->m_count; j++) {
            if (set->m_weapons[j] != 0) {
                continue;
            }
            set->m_weapons[j] = 0;
            char name[8];
            AtCopyName8(name, names->m_data[j]);
            if (AtNamesEqual(name, "")) {
                continue;
            }
            GmAcActor* weapon = GmSpawnManager::s_instance->Spawn(name);
            if (weapon != 0) {
                set->m_weapons[j] = (GmCmWeapon*)weapon;
                weapon->Slot4();
                weapon->Slot32();
                weapon->Slot49("SPAWN", EventArgument(&event), 1);
            }
        }
    }
}

// The threat: the current target first, if it is one; then, unless that settled it without priorities, every actor of each enemy type, by the type's priority when UseEnemyPriorities, the
// nearest of the highest priority. Quirks kept: priorities are used only when EnemyPriorities itself is there, though m_unknown7C is read in its place; the owner's +0x04's name is copied and
// not used; the iterator's Begin is not tested here.
void FaCmCombat::UpdateThreat(int post) {
    const int wasThreat = m_threat;
    const int oldRange = m_attackRange;
    m_attackRange = -1;
    m_unknownA4 = -1;
    m_threat = 0;
    float best = kFloatMax;
    GmAcActor* threat = 0;
    AtDataShortNameArray* enemies = (AtDataShortNameArray*)(m_unknown78 != 0 ? (AtDataComponent*)(Size)m_unknown78 : m_enemies);
    AtDataIntArray* priorities = (AtDataIntArray*)(m_unknown7C != 0 ? (AtDataComponent*)(Size)m_unknown7C : m_enemyPriorities);
    if (TargetOf(m_owner) != 0) {
        TestThreat(TargetOf(m_owner), wasThreat, oldRange, &m_threat, &m_attackRange, &best, 1);
        if (m_threat != 0) {
            threat = GmAcTargetOf(m_owner);
            if (m_useEnemyPriorities != 0 && *(const int*)m_useEnemyPriorities != 0 && m_enemyPriorities != 0) {
                if (enemies == 0) {
                    goto posted;
                }
                if (priorities != 0) {
                    for (unsigned int i = 0; i < enemies->m_descriptor->m_count; i++) {
                        char name[8];
                        AtCopyName8(name, enemies->m_data[i]);
                        if (threat->IsOfType(name)) {
                            m_unknownA4 = priorities->m_data[i];
                            break;
                        }
                    }
                }
            }
        }
    }
    if (enemies == 0) {
        goto posted;
    }
    if (m_threat != 0 && !(m_useEnemyPriorities != 0 && *(const int*)m_useEnemyPriorities != 0 && m_enemyPriorities != 0)) {
        goto posted;
    }
    {
        char ownName[8];
        AtCopyName8(ownName, (const char*)(Size)m_owner->m_unknown04 + 0x0C);
        GmDynamicNodeIterator it;
        it.Begin(GmDynamicNodeLoader::s_instance);
        for (unsigned int i = 0; i < enemies->m_descriptor->m_count; i++) {
            const int usePriorities = m_useEnemyPriorities != 0 && *(const int*)m_useEnemyPriorities != 0 && m_enemyPriorities != 0;
            const int priority = usePriorities ? priorities->m_data[i] : -1;
            if (usePriorities && priority < m_unknownA4) {
                continue;
            }
            char name[8];
            AtCopyName8(name, enemies->m_data[i]);
            it.Find(name);
            while (it.m_entry != 0) {
                GmAcActor* enemy = it.m_item != 0 ? *it.m_item : 0;
                if (IsEnemy(enemy, 0) && IsAlive(enemy)) {
                    int isThreat = 0;
                    int range = -1;
                    const int nearest = (m_useEnemyPriorities != 0 && *(const int*)m_useEnemyPriorities != 0 && m_enemyPriorities != 0 && priority > m_unknownA4) ? 0 : 1;
                    TestThreat(enemy, wasThreat, oldRange, &isThreat, &range, &best, nearest);
                    if (isThreat != 0) {
                        m_threat = 1;
                        m_attackRange = range;
                        m_unknownA4 = priority;
                        threat = enemy;
                    }
                }
                it.Next();
            }
        }
    }
posted:
    if (post != 0) {
        GmAcActor* current = TargetOf(m_owner);
        PostThreat(threat != current ? 1 : 0, wasThreat, oldRange, threat, m_unknownA4);
    } else if (m_threat != 0) {
        GmGameEvent event;
        event.Set(threat, 0, 0, -1);
        m_owner->Slot49("THREATAC", EventArgument(&event), 1);
    }
}

// Quirks kept: the slot that answers is called before the owner is tested for null. Without the button held, a target is chosen by angle within MaxAimAngleH.
void FaCmCombat::UpdateTargetMe() {
    GmAcActor* current = TargetOf(m_owner);
    int down = 0;
    if (m_owner->Slot42() != 0 && m_owner != 0) {
        const int controller = m_owner->Slot59();
        if (controller >= 0) {
            down = App::s_instance->m_unknown692C->FindButton(controller, "GameButtonLockOn").IsDown() != 0 ? 1 : 0;
        }
    }
    GmAcActor* chosen;
    if (down != 0) {
        chosen = current;
    } else {
        const float maxAngle = (float)MaxAimAngleH();
        chosen = ChooseTarget(0, 1, maxAngle, 0);
        if (chosen != 0 && chosen != current) {
            GmGameEvent event;
            event.Set(chosen, 0, 0, -1);
            m_owner->Slot49("TARGETME", EventArgument(&event), 1);
        }
    }
    if (TargetOf(m_owner) != 0) {
        if (chosen == 0 || chosen != GmAcTargetOf(m_owner)) {
            m_owner->Slot49("TARGETNO", 0, 1);
        }
    }
}

// The target is kept while it is enabled, answers slot 55 if an object, is within the lock-off ranges and, with the PCNPC and the button not held, within MaxAimAngleH; else dropped, and
// with none another is chosen when the controller's owner allows (its +0x3A8), by MaxNextLockOnAngleH, or MaxAimAngleH with the PCNPC and the button not held. Quirks kept: the controller is
// not tested; the stored target is compared, and written only when it differs.
void FaCmCombat::UpdateLockOn() {
    GmAcActor* target = TargetOf(m_owner);
    GmAcActor* pcnpc = GmAcFindAttached(m_owner, "PCNPC");
    int down = 0;
    if (pcnpc != 0) {
        AppUnknown692C* input = App::s_instance->m_unknown692C;
        down = input->FindButton(m_owner->Slot59(), "GameButtonLockOn").IsDown();
    }
    if (target != 0) {
        if (target->Slot27() == 0) {
            target = 0;
        } else if ((target->m_flags98 & kObjectFlag) != 0 && target->Slot55() == 0) {
            target = 0;
        } else if (!InLockOffRange(0)) {
            target = 0;
        } else {
            const int checkH = pcnpc != 0 && down == 0 ? 1 : 0;
            const float maxAngle = (float)MaxAimAngleH();
            if (!InAimCone(checkH, maxAngle, 0)) {
                target = 0;
            }
        }
    }
    if (target != TargetOf(m_owner)) {
        m_owner->m_unknown17C = (unsigned int)(Size)target;
    }
    if (target == 0) {
        GmAcActor* player = ((GmAcCharacter*)m_owner)->m_controller->Owner();
        if (((FaAcPlayer*)player)->m_unknown3A8 != 0) {
            if (pcnpc == 0 || m_unknownA8[1] != 0) {
                float angle = (float)((m_maxNextLockOnAngleH != 0 ? (double)*(const float*)m_maxNextLockOnAngleH : 90.0) * kDegrees);
                if (pcnpc != 0 && down == 0) {
                    angle = (float)MaxAimAngleH();
                }
                target = ChooseTarget(0, 1, angle, 0);
            }
        }
    }
    if (target != TargetOf(m_owner)) {
        m_owner->m_unknown17C = (unsigned int)(Size)target;
    }
    UpdateBumpTag();
    if (pcnpc == 0) {
        return;
    }
    if (TargetOf(m_owner) != 0) {
        if (m_unknownD4[2] == 0) {
            m_unknownD4[2] = 1;
            m_unknownE0 = 0;
            m_owner->Slot49("HAVETRGT", 0, 1);
        }
    } else if (m_unknownE0 == 0) {
        m_unknownD4[2] = 0;
        m_unknownE0 = 1;
        m_owner->Slot49("LOSTTRGT", 0, 1);
    }
}

// Quirks kept: an actor found is tested through the node's slot 27 even when null; the radius is the world sphere's, read after its body is finished; ZAPEND is tested as unsigned.
void FaCmCombat::UpdateZap() {
    GmDynamicNodeIterator it;
    unsigned int count = 0;
    if (it.Begin(GmDynamicNodeLoader::s_instance) == 0) {
        AtVector3 own;
        m_owner->Slot22(&own, 0, 0, 0, 0);
        it.Find("ZapLight");
        while (it.m_entry != 0) {
            GmAcActor* zap = it.m_item != 0 ? *it.m_item : 0;
            if (zap->GmAcNode::Slot27() != 0) {
                AtVector3 position;
                zap->Slot22(&position, 0, 0, 0, 0);
                AtSceneNode* node = zap->m_sceneNode;
                if ((node->m_flags & AtSceneNode::kMoved) != 0) {
                    node->FinishBody();
                }
                const float radius = node->m_worldSphere.radius;
                if (DistanceInRegister(own, position) < radius) {
                    count++;
                }
            }
            it.Next();
        }
    }
    if (m_unknownD4[0] == 0) {
        if (count > 0) {
            m_owner->Slot49("ZAPSTART", 0, 1);
        }
    } else if ((unsigned int)m_unknownD4[0] > 0 && count == 0) {
        m_owner->Slot49("ZAPEND", 0, 1);
    }
    m_unknownD4[0] = (int)count;
}

// LOCKL_DN and LOCKR_DN become TAGL_DN and TAGR_DN; the tag is sent when it is not empty and differs from the last, unless m_unknownD0. Quirk kept: another tag (none, today) is sent as it is.
void FaCmCombat::UpdateLockTag() {
    char tag[8];
    int changed = 0;
    ReadLockStick(tag, 0x89, 0x88);
    if (AtNameLength(tag) > 0) {
        if (AtNamesEqual(tag, "LOCKL_DN")) {
            AtCopyName("TAGL_DN", tag);
        } else if (AtNamesEqual(tag, "LOCKR_DN")) {
            AtCopyName("TAGR_DN", tag);
        }
        if (!AtNamesEqual(tag, (const char*)&m_unknownB0[6])) {
            changed = 1;
        }
    }
    AtCopyName8((char*)&m_unknownB0[6], tag);
    if (changed != 0 && m_unknownD0 == 0) {
        m_owner->Slot49(tag, 0, 1);
    }
}

// The stick of the controller's owner: nothing below MinLookStickMagnitude (0.2); LOCKL_DN within half BumpTargetAngle (90) of 90 degrees, LOCKR_DN within it of 270. The tag is written as
// eight bytes, so not terminated. Quirks kept: the square of the magnitude is rounded to float before its root; each band is closed below and open above.
void FaCmCombat::ReadLockStick(char* tag, int axisX, int axisY) {
    ((int*)tag)[0] = 0;
    ((int*)tag)[1] = 0;
    GmAcActor* player = ((GmAcCharacter*)m_owner)->m_controller->Owner();
    float stick[2];
    FaAcPlayerReadStick(player, stick, axisX, axisY);
    const float squared = (float)((double)stick[1] * stick[1] + (double)stick[0] * stick[0]);
    const double minimum = m_minLookStickMagnitude != 0 ? *(const float*)m_minLookStickMagnitude : 0.2f;
    if (!(sqrt((double)squared) >= minimum)) {
        return;
    }
    const double angle = FaAcPlayerStickAngle(player, 0, axisX, axisY);
    const float half = (float)((m_bumpTargetAngle != 0 ? (double)*(const float*)m_bumpTargetAngle : 90.0) * 0.5f);
    if (angle >= 90.0 - half && angle < half + 90.0) {
        AtCopyName8(tag, "LOCKL_DN");
        return;
    }
    if (angle >= 270.0 - half && angle < half + 270.0) {
        AtCopyName8(tag, "LOCKR_DN");
    }
}

// Two sticks: the digital one's tag is sent when DigitalBumpTargetting, not empty and new; else the analog one's when AnalogBumpTargetting, not empty and new. Both are kept for next time.
void FaCmCombat::UpdateBumpTag() {
    char digital[8];
    char analog[8];
    char sent[8];
    ReadLockStick(digital, 0x89, 0x88);
    ReadLockStick(analog, 0x86, 0x87);
    if (m_digitalBumpTargetting != 0 && *(const int*)m_digitalBumpTargetting != 0 && AtNameLength(digital) > 0 && !AtNamesEqual(digital, (const char*)&m_unknownB0[2])) {
        AtCopyName8(sent, digital);
        m_owner->Slot49(sent, 0, 1);
    } else if (m_analogBumpTargetting != 0 && *(const int*)m_analogBumpTargetting != 0 && !AtNamesEqual(analog, "") && !AtNamesEqual(analog, (const char*)&m_unknownB0[4])) {
        AtCopyName8(sent, analog);
        m_owner->Slot49(sent, 0, 1);
    }
    AtCopyName8((char*)&m_unknownB0[2], digital);
    AtCopyName8((char*)&m_unknownB0[4], analog);
}

// A threat is enabled (the node's slot 27 and its own), attackable (a character's combat module's m_unknownA0), alive, nearer than *best when nearest; and within ThreatEnterRange
// (ThreatExitRange if it was a threat) unless ThreatByRadius is 0, or else seen when ThreatBySightCone. Its attack range: the last it was in while within that range's ceiling, then the
// last later one whose floor it is within. Quirks kept: the start position (slot 24) replaces the owner's when UseStartPositionForAttackRange; the distance is rounded to float.
void FaCmCombat::TestThreat(GmAcActor* target, int wasThreat, int oldRange, int* isThreat, int* range, float* best, int nearest) {
    if (target == 0 || target->GmAcNode::Slot27() == 0 || target->Slot27() == 0) {
        return;
    }
    if ((target->m_flags98 & kCharacterFlags) != 0) {
        FaCmCombat* combat = (FaCmCombat*)((GmAcCharacter*)target)->m_combat;
        if (combat != 0 && combat->m_unknownA0 == 0) {
            return;
        }
    }
    if (target->m_unknown270 != 0) {
        GmAcStat* health = ((GmAcStatsHolder*)target->m_unknown270)->m_stats->Find("sHealth", 1);
        if (health != 0 && !(health->m_value > 0.0f)) {
            return;
        }
    }
    AtVector3 own;
    m_owner->Slot22(&own, 0, 0, 0, 0);
    if (m_useStartPositionForAttackRange != 0 && *(const int*)m_useStartPositionForAttackRange != 0) {
        m_owner->Slot24((int)(Size)&own, 0, 0, 0, 0);
    }
    AtVector3 position;
    target->Slot22(&position, 0, 0, 0, 0);
    float distance = own.Distance(position);
    if (nearest != 0 && !(distance < *best)) {
        return;
    }
    int hit;
    if (wasThreat == 0 && m_threatByRadius != 0 && *(const int*)m_threatByRadius == 0) {
        hit = 0;
    } else {
        const double limit = wasThreat == 0 ? ThreatEnterRange() : ThreatExitRange();
        hit = limit >= distance ? 1 : 0;
    }
    if (hit == 0) {
        if (m_threatBySightCone == 0 || *(const int*)m_threatBySightCone == 0) {
            return;
        }
        if (GmAcCharacterCanSee((GmAcCharacter*)m_owner, target, 1, 1, 1, &distance, 0, 0, 0, 0, 0) == 0) {
            return;
        }
    }
    *isThreat = 1;
    *best = distance;
    if (oldRange >= 0 && distance <= m_attackRanges[oldRange].m_range->m_descriptor->m_realCeiling) {
        *range = oldRange;
    } else {
        *range = -1;
    }
    for (int i = *range + 1; i < m_attackRangeCount; i++) {
        if (distance <= m_attackRanges[i].m_range->m_descriptor->m_realFloor) {
            *range = i;
        }
    }
}

// Quirk kept: 1 for anything not an enabled character with stats, null included.
int FaCmCombat::IsAlive(GmAcActor* target) {
    int alive = 1;
    int attackable = 1;
    if (target == 0 || target->GmAcNode::Slot27() == 0 || target->Slot27() == 0 || (target->m_flags98 & kCharacterFlags) == 0 || target->m_unknown270 == 0) {
        return 1;
    }
    GmAcStat* health = ((GmAcStatsHolder*)target->m_unknown270)->m_stats->Find("sHealth", 1);
    if (health != 0 && !(health->m_value > 0.0f)) {
        alive = 0;
    }
    FaCmCombat* combat = (FaCmCombat*)((GmAcCharacter*)target)->m_combat;
    if (combat != 0) {
        attackable = combat->m_unknownA0;
    }
    return alive != 0 && attackable != 0 ? 1 : 0;
}

// With a threat, THREAT when it is new or changed (and its priority kept when it is the owner's target); without one, UNTHREAT when there was one or it changed. Then, while alive and
// m_unknown9C, the attack range's enter event when the range is new or the threat changed. Quirks kept: the event names the threat, made before either branch, and is sent with THREAT and the
// range's event only; a range of -1 after a change sends nothing.
void FaCmCombat::PostThreat(int changed, int wasThreat, int oldRange, GmAcActor* threat, int priority) {
    GmGameEvent event;
    event.Set(threat, 0, 0, -1);
    if (m_threat != 0) {
        if (wasThreat == 0 || changed != 0) {
            m_owner->Slot49("THREAT", EventArgument(&event), 1);
            if (TargetOf(m_owner) == threat) {
                m_unknownA4 = priority;
            }
        }
    } else if (wasThreat != 0 || changed != 0) {
        m_owner->Slot49("UNTHREAT", 0, 1);
        if (threat == 0) {
            m_unknownA4 = -1;
        }
    }
    if (m_unknown9C != 0 && Slot7() != 0) {
        const int range = m_attackRange;
        if (range < 0) {
            return;
        }
        if (range == oldRange && changed == 0) {
            return;
        }
        const char* enter = (const char*)m_attackRanges[range].m_enterEvent;
        if (enter != 0) {
            m_owner->Slot49(enter, EventArgument(&event), 1);
        }
    }
}

// Quirk kept: with no target given, the owner's slot 53 is asked whether it has one, and then the aim node of the null target is asked for, as the original does (it crashes there).
void FaCmCombat::TargetPosition(AtVector3* out, GmAcActor* target) {
    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
    if (target == 0 && m_owner->Unknown17C() == 0) {
        return;
    }
    if (GmAcNodeAimNode(target) != 0) {
        *out = CentreOf(GmAcNodeAimNode(target));
        return;
    }
    target->Slot22(out, 0, 0, 0, 0);
}

// Quirks kept: siblings are actors of the owner's own type name (its +0x04's name) and the same slot 6 name; the type names are compared as the original's unterminated copies.
int FaCmCombat::IsEnemy(GmAcActor* target, int listed) {
    struct {
        char target[8];
        char own[8];
        char end;
    } names;
    names.end = 0;
    AtCopyName8(names.own, (const char*)(Size)m_owner->m_unknown04 + 0x0C);
    const char* ownName = m_owner->Name();
    AtDataShortNameArray* enemies = (AtDataShortNameArray*)(m_unknown78 != 0 ? (AtDataComponent*)(Size)m_unknown78 : m_enemies);
    if (target == 0 || target == m_owner) {
        return 0;
    }
    AtCopyName8(names.target, AtNameLength((const char*)target->m_unknown264) > 0 ? (const char*)target->m_unknown264 : (const char*)target->m_unknown240[0]);
    if (listed != 0) {
        int found = 0;
        for (unsigned int i = 0; i < enemies->m_descriptor->m_count; i++) {
            char name[8];
            AtCopyName8(name, enemies->m_data[i]);
            found = AtNamesEqual(names.target, name);
            if (found != 0) {
                break;
            }
        }
        if (found == 0) {
            return 0;
        }
    }
    if (m_canAttackSiblings != 0 && *(const int*)m_canAttackSiblings != 0) {
        return 1;
    }
    if (CompareCopies(names.target, names.own) != 0) {
        return 1;
    }
    return ownName == target->Name() ? 0 : 1;
}

// Quirks kept: with no target given only the owner's slot 53 word is used, and only to give up when it is 0 (the null target then goes on to TargetPosition). The facing is asked for and not used.
int FaCmCombat::InRange(float minRangeH, float maxRange, GmAcActor* target) {
    if (target == 0) {
        target = (GmAcActor*)(Size)m_owner->Unknown17C();
        if (target == 0) {
            return 0;
        }
    }
    const AtVector3 own = CentreOf(m_owner->m_sceneNode);
    AtVector3 facing;
    m_owner->Slot22(0, &facing, 0, 0, 0);
    AtVector3 position;
    TargetPosition(&position, target);
    AtVector3 level;
    level.x = own.x;
    level.y = own.y;
    level.z = position.z;
    const float distance = own.Distance(position);
    if (!(DistanceInRegister(level, position) >= minRangeH)) {
        return 0;
    }
    if (!(distance <= maxRange)) {
        return 0;
    }
    return 1;
}

// The vertical angle is between the facing and (facing x, facing y, the direction's z), the horizontal between the facing and the level direction. Quirks kept: as InRange for a null target;
// the vertical angle is compared at the precision it comes back in, the horizontal one rounded to float.
int FaCmCombat::InAimCone(int checkH, float maxAngleH, GmAcActor* target) {
    if (target == 0) {
        target = (GmAcActor*)(Size)m_owner->Unknown17C();
        if (target == 0) {
            return 0;
        }
    }
    const AtVector3 own = CentreOf(m_owner->m_sceneNode);
    AtVector3 facing;
    m_owner->Slot22(0, &facing, 0, 0, 0);
    AtVector3 position;
    TargetPosition(&position, target);
    AtVector3 direction;
    direction.x = (float)((double)position.x - own.x);
    direction.y = (float)((double)position.y - own.y);
    direction.z = (float)((double)position.z - own.z);
    direction.Normalize();
    AtVector3 level;
    level.x = direction.x;
    level.y = direction.y;
    level.z = 0.0f;
    level.Normalize();
    AtVector3 raised;
    raised.x = facing.x;
    raised.y = facing.y;
    raised.z = direction.z;
    raised.Normalize();
    const float angleH = (float)facing.AngleTo(level);
    const double angleV = facing.AngleTo(raised);
    const double limitV = (m_maxAimAngleV != 0 ? (double)*(const float*)m_maxAimAngleV : 45.0) * kDegrees;
    if (!(angleV < limitV)) {
        return 0;
    }
    if (checkH != 0 && !(angleH < maxAngleH)) {
        return 0;
    }
    return 1;
}

int FaCmCombat::InLockOnRange(GmAcActor* target) {
    const float maxRange = m_lockOnMaxRange != 0 ? *(const float*)m_lockOnMaxRange : 1000.0f;
    const float minRangeH = m_lockOnMinRangeH != 0 ? *(const float*)m_lockOnMinRangeH : 300.0f;
    return InRange(minRangeH, maxRange, target);
}

int FaCmCombat::InLockOffRange(GmAcActor* target) {
    const float maxRange = m_lockOffMaxRange != 0 ? *(const float*)m_lockOffMaxRange : 1200.0f;
    const float minRangeH = m_lockOffMinRangeH != 0 ? *(const float*)m_lockOffMinRangeH : 150.0f;
    return InRange(minRangeH, maxRange, target);
}

// Each actor of each enemy type that is not the owner (nor the current target when side is not 0), enabled, of an actor, answering slot 55 if an object, and in lock-on range, is asked
// LOCKON?; if it answers (writing m_unknownA8[0]) and the owner sees it, it is a candidate three ways: by angle (A), by distance (B), by angle within the aim cone (C), each the best of the
// highest priority. Side 0 returns C, else B when nearestFallback, else A; other sides return A.
// Quirks kept: the facing is turned to the current target unless keepFacing, then flattened; with side 0 an actor's distance is compared with C's angle to let it through; distance is FLT_MAX
// for B unless side is 0; the side's angle is measured the other way round by the sign of a cross product; priorities as UpdateThreat's.
GmAcActor* FaCmCombat::ChooseTarget(int side, int keepFacing, float maxAngleH, int nearestFallback) {
    GmAcActor* const current = TargetOf(m_owner);
    const AtVector3 own = CentreOf(m_owner->m_sceneNode);
    AtVector3 facing;
    m_owner->Slot22(0, &facing, 0, 0, 0);
    if (current != 0 && keepFacing == 0) {
        AtVector3 position;
        current->Slot22(&position, 0, 0, 0, 0);
        facing.x = (float)((double)position.x - own.x);
        facing.y = (float)((double)position.y - own.y);
        facing.z = (float)((double)position.z - own.z);
        facing.Normalize();
    }
    facing.z = 0.0f;
    facing.Normalize();
    GmAcActor* bestA = 0;
    GmAcActor* bestB = 0;
    GmAcActor* bestC = 0;
    float angleA = kFloatMax;
    float distanceB = kFloatMax;
    float angleC = kFloatMax;
    int priorityA = -1;
    int priorityB = -1;
    int priorityC = -1;
    AtDataShortNameArray* enemies = (AtDataShortNameArray*)(m_unknown78 != 0 ? (AtDataComponent*)(Size)m_unknown78 : m_enemies);
    AtDataIntArray* priorities = (AtDataIntArray*)(m_unknown7C != 0 ? (AtDataComponent*)(Size)m_unknown7C : m_enemyPriorities);
    GmDynamicNodeIterator it;
    if (it.Begin(GmDynamicNodeLoader::s_instance) == 0) {
        for (unsigned int i = 0; i < enemies->m_descriptor->m_count; i++) {
            char name[8];
            AtCopyName8(name, enemies->m_data[i]);
            it.Find(name);
            while (it.m_entry != 0) {
                GmAcActor* enemy = it.m_item != 0 ? *it.m_item : 0;
                const int priority = (m_useEnemyPriorities != 0 && *(const int*)m_useEnemyPriorities != 0 && m_enemyPriorities != 0) ? priorities->m_data[i] : -1;
                if (enemy == 0 || enemy == m_owner || (side != 0 && enemy == current) || (enemy->m_flags98 & GmAcNode::kActorReset) == 0 || enemy->Slot27() == 0 ||
                    ((enemy->m_flags98 & kObjectFlag) != 0 && enemy->Slot55() == 0) || !InLockOnRange(enemy)) {
                    it.Next();
                    continue;
                }
                AtVector3 position;
                enemy->Slot22(&position, 0, 0, 0, 0);
                float distance = kFloatMax;
                AtVector3 to;
                to.x = (float)((double)position.x - own.x);
                to.y = (float)((double)position.y - own.y);
                to.z = 0.0f;
                to.Normalize();
                float angle = (float)facing.AngleTo(to);
                const int clockwise = (double)to.y * facing.x - (double)facing.y * to.x < 0.0 ? 1 : 0;
                if (side == 1) {
                    if (clockwise != 0) {
                        angle = (float)(kTwoPi - (double)angle);
                    }
                } else if (side == 2) {
                    if (clockwise == 0) {
                        angle = (float)(kTwoPi - (double)angle);
                    }
                }
                const int inCone = InAimCone(keepFacing, maxAngleH, enemy);
                if (side == 0) {
                    const double exact = DistanceInRegister(own, position);
                    distance = (float)exact;
                    if (!(exact < distanceB) && !(distance < angleC)) {
                        it.Next();
                        continue;
                    }
                    if (inCone == 0 && nearestFallback == 0) {
                        it.Next();
                        continue;
                    }
                } else if (!(angle < angleA) || inCone == 0) {
                    it.Next();
                    continue;
                }
                m_unknownA8[0] = 0;
                {
                    GmGameEvent event;
                    event.Set(m_owner, 0, 0, -1);
                    enemy->Slot49("LOCKON?", EventArgument(&event), 1);
                    if (m_unknownA8[0] != 0 && GmAcCharacterCanSee((GmAcCharacter*)m_owner, enemy, 0, 0, 1, 0, 0, 0, 0, 0, 0) != 0) {
                        if (angle < angleA && priority >= priorityA) {
                            priorityA = priority;
                            angleA = angle;
                            bestA = enemy;
                        }
                        if (distance < distanceB && priority >= priorityB) {
                            priorityB = priority;
                            distanceB = distance;
                            bestB = enemy;
                        }
                        if (angle < angleC && inCone != 0 && priority >= priorityC) {
                            priorityC = priority;
                            angleC = angle;
                            bestC = enemy;
                        }
                    }
                }
                it.Next();
            }
        }
    }
    GmAcActor* chosen = 0;
    if (side == 0) {
        chosen = bestC;
        if (chosen == 0 && nearestFallback != 0) {
            chosen = bestB;
        }
    }
    if (chosen == 0) {
        chosen = bestA;
    }
    return chosen;
}
