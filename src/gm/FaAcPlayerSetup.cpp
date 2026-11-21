// FaAcPlayer's set-up (slot 15) and what it alone needs: the character's sight query set-up, the sight query's Set, the head joint's lookup, the saved game's real values, the record's value
// lookup, the registry's search by name, and the target sprite's two set-up slots. See include/gm/GmAcPlayer.h. Names other than the RTTI names are inferred; record words are the files' own.
#include "gm/GmAcPlayer.h"
#include "gm/FaCmCombat.h"
#include "gm/GmAppSettings.h"
#include "gm/GmLevelManager.h"

#include "at/AtSceneActor.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName(const char* from, char* to);  // 0x00013ED0 at most eight bytes; stops after a terminator
extern "C" int toupper(int c);

namespace {
typedef decltype(sizeof(0)) Size;
struct Placement {};

const float kDegreesToRadians = 0.0174532924f;  // 0x0027B558
const float kDefaultSightLength = 1000.0f;       // 0x447A0000, immediate
const float kDefaultSightYaw = 25.0f;            // 0x41C80000
const float kDefaultSightPitch = 20.0f;          // 0x41A00000

}  // namespace
inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

// 0x002B422C: the name of the joint the player's head is, a pointer to "HEADR" (0x0027B550).
const char* g_playerHeadJoint = "HEADR";
GmSavedGame* GmSavedGame::s_instance;

// ---- the registry and the saved game ---------------------------------------------------------------------------------------------
// Quirk kept: a section given but not found is searched for no further; the name is then looked for on its own, as with no section.
void* GmSaveData::Find(const char* section, const char* name, unsigned int type) {
    AtDataLink* under = 0;
    AtDataLink* link;
    if (section[0] != 0 && (under = m_registry.FindLink(section, 0, 0)) != 0) {
        link = m_registry.FindPath(under, name, 1);
    } else {
        link = m_registry.FindLink(name, 0, 0);
        if (link == 0) {
            link = m_registry.FindPath(under, name, 1);
        }
    }
    if (link != 0 && link->m_value->m_descriptor->m_type == type) {
        return link->m_value->ElementAt(0);
    }
    return 0;
}

float* GmSaveData::FindReal(const char* section, const char* name) {
    return (float*)Find(section, name, AtDataRegistry::kReal);
}

float GmSavedGame::GetReal(const char* section, const char* name) {
    const float* value = App::s_instance->m_gameState->FindReal(section, name);
    if (value != 0) {
        return *value;
    }
    return 0.0f;
}

// ---- the record and the joints -------------------------------------------------------------------------------------------------------
AtRotTransCoordSys* AtCoordSysDatabase::FindRotTrans(const char* name) {
    AtRotTransCoordSys* found = (AtRotTransCoordSys*)Find(name);
    if (found != 0 && found >= m_rotTrans && found < m_rotTrans + m_rotTransCount) {
        return found;
    }
    return 0;
}

// ---- the sight query --------------------------------------------------------------------------------------------------------------
// The products of two floats are exact in a double, as in the x87; each square is rounded once to a float. The cosines are the x87's own (AtCos), rounded to a double and then a float.
void GmAcSightQuery::Set(const AtVector3* position, const AtVector3* direction, float length, float yaw, float pitch, float minDistance) {
    m_position = *position;
    m_direction = *direction;
    m_unknown20 = length * length;
    m_unknown1C = length;
    m_unknown18 = minDistance * minDistance;
    m_unknown24 = (float)AtCos((double)yaw * kDegreesToRadians);
    m_unknown28 = (float)AtCos((double)pitch * kDegreesToRadians);
}

// Quirks kept: without a position the scene node is not tested. Each measure is copied as a word, not loaded as a real.
void GmAcCharacter::SetupSightQuery(GmAcSightQuery* query, const AtVector3* position, const AtVector3* direction, const float* length, const float* yaw, const float* pitch,
                                    const float* minDistance) {
    AtVector3 from;
    if (position == 0) {
        AtSceneNode* node = m_sceneNode;
        if ((node->m_flags & AtSceneNode::kMoved) != 0) {
            node->FinishBody();
        }
        from = node->m_worldSphere.center;
    } else {
        from = *position;
    }
    if (direction == 0) {
        direction = (const AtVector3*)m_unknown18;
    }
    const float least = minDistance != 0 ? *minDistance : m_sightConeMinDistance != 0 ? *m_sightConeMinDistance : 0.0f;
    const float down = pitch != 0 ? *pitch : m_sightConePitch != 0 ? *m_sightConePitch : kDefaultSightPitch;
    const float across = yaw != 0 ? *yaw : m_sightConeYaw != 0 ? *m_sightConeYaw : kDefaultSightYaw;
    const float far = length != 0 ? *length : m_sightConeLength != 0 ? *m_sightConeLength : kDefaultSightLength;
    query->Set(&from, direction, far, across, down, least);
}

// ---- the target sprite ---------------------------------------------------------------------------------------------------------
void GmAcSprite::Setup(GmSpriteScene* scene) {
    m_scene = scene;
    m_unknown20 = scene->m_materials->BufferSet();
    m_format.Set(0xA080, 0, 1);
    m_material = 0;
}

// The material lookup is AtMaterialOwner::FindMaterial (0x00027EF0), inlined in the original.
void GmAcSprite::SetupNamed(GmSpriteScene* scene, const char* name, const char* ns) {
    m_scene = scene;
    m_unknown20 = scene->m_materials->BufferSet();
    m_format.Set(0xA080, 0, 1);
    m_material = m_scene->m_materials->FindMaterial(name, ns);
}

// ---- FaAcPlayer's slot 15 ---------------------------------------------------------------------------------------------------------
// The character's slot 15; with LevelContinuation, the current level's SavedTime into the level manager's +0x60; the scene actor's joint HEADR, when it has joints; the sight query from the
// record's words; with Combat, a FaCmPlayerCombat from the actor heap, attached to this player and given its slot 3; and the target sprite, from the scenes, with the material "Target" in no
// name space. Quirks kept: the level's name is copied whether or not it is used. A failed allocation is reported and then called through. The sprite is set up through its vftable.
void FaAcPlayer::Slot15() {
    GmAcCharacter::Slot15();
    char level[16];
    const char* from = GmLevelManager::s_instance->m_currentName;
    for (int i = 0; (level[i] = from[i]) != 0; i++) {
    }
    const int* continuation = (const int*)FindValueOrDefault("LevelContinuation", 0);
    if (continuation != 0 && *continuation != 0) {
        // +0x60 holds the saved time, a real; the level manager's header has it as an integer.
        *(float*)&GmLevelManager::s_instance->m_unknown060 = GmSavedGame::s_instance->GetReal(level, "SavedTime");
    }
    AtRotTransCoordSys* head = 0;
    AtSceneJointedModelActor* actor = (AtSceneJointedModelActor*)m_unknownA0;
    if (actor != 0 && actor->m_joints.m_database != 0) {
        AtCoordSysDatabase* joints = actor->m_joints.m_database;
        char name[12];
        AtCopyName(g_playerHeadJoint, name);
        head = joints->FindRotTrans(name);
    }
    m_head = head;
    SetupSightQuery(&m_sightQuery, 0, 0, 0, 0, 0, 0);
    const int* combat = (const int*)FindValueOrDefault("Combat", 0);
    if (combat != 0 && *combat != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(sizeof(FaCmPlayerCombat));
        m_combat = block != 0 ? new (block, Placement()) FaCmPlayerCombat : 0;
        AtPopHeap();
        if (m_combat == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_combat->Attach(this);
        m_combat->Slot3();
    }
    GmAcSprite* target = &m_target;
    target->SetupNamed(GmHistoryLogs::s_instance->m_scenes->m_spriteScene, "Target", "");
}
