// The design record reader every actor shares: GmBaObject::FindValue, GmAcNode::FindValueOrDefault, and the two slot 2 bodies that read an actor's and a node's words from its record. See
// include/gm/GmAcActor.h and include/gm/GmAcCharacter.h. Names are inferred; the record words are the files' own.
#include "gm/GmAcCharacter.h"
#include "gm/GmLevelManifest.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();

GmDesignDefaults* GmDesignDefaults::s_instance;

namespace {

// 0x00028430, a method of the scene device, written out as src/at/AtSceneActor.cpp does: in a namespace if one is given, else the plain table. The node's slot 2 calls it only with a namespace
// pointer, and looks in the plain table itself without one; the two come to the same.
void* FindMatAssignment(AtSceneDevice* device, const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return device->m_matAssignmentSpaces.Find(ns, name);
    }
    return device->m_matAssignments.Find(name);
}

AtSceneDevice* SceneDevice() {
    return (*(GmSceneDeviceHolder**)((char*)GmDynamicNodeState::s_instance + 4))->m_device;
}

}  // namespace

// ---- the readers -------------------------------------------------------------------------------------------------------------
void* GmBaObject::FindValue(const char* name, int) {
    if (m_unknown10 != 0 && m_descriptor != 0) {
        AtDataLink* link = ((AtDataRegistry*)m_unknown10)->FindPath((AtDataLink*)m_descriptor, name, 1);
        if (link != 0) {
            return link->m_value->ElementAt(0);
        }
    }
    return 0;
}

// Quirk kept: the original measures the name's whole length only to test it for zero. A null name crashes, as there.
void* GmAcNode::FindValueOrDefault(const char* name, int) {
    if (name[0] == 0) {
        return 0;
    }
    void* value = GmBaObject::FindValue(name, 0);
    if (value != 0) {
        return value;
    }
    GmDesignDefaults* defaults = GmDesignDefaults::s_instance;
    AtDataLink* link = defaults->m_registry->FindPath(defaults->m_link, name, 0);
    if (link == 0) {
        return 0;
    }
    return link->m_value->ElementAt(0);
}

// ---- the node's record ---------------------------------------------------------------------------------------------------------
// Quirks kept: the scene node is not tested before the collision groups are written to it. Each material assignment's namespace is looked up whether or not its name was found. bCopyMaterials
// clears bit 0x100 when it is absent or false. The material assignments are applied once at the end, as the first, whatever was found.
int GmAcNode::Slot2() {
    if (m_descriptor == 0) {
        return 0;
    }
    m_unknownE0[0] = FindValueOrDefault("rUpdateDistance", 0);
    m_unknownE0[1] = FindValueOrDefault("rCriticalUpdateDistanceSquared", 0);
    m_unknownE0[2] = FindValueOrDefault("rHighUpdateDistanceSquared", 0);
    m_unknownE0[3] = FindValueOrDefault("rMediumUpdateDistanceSquared", 0);
    m_unknownE0[4] = FindValueOrDefault("nMinUpdateLevel", 0);
    m_unknownE0[5] = FindValueOrDefault("nMaxUpdateLevel", 0);
    m_unknownE0[6] = FindValueOrDefault("nCulledUpdateLevel", 0);
    int* groups = (int*)FindValueOrDefault("nCollisionGroups", 0);
    if (groups != 0) {
        m_sceneNode->m_unknown60 = *groups;
    }
    int* testGroups = (int*)FindValueOrDefault("nCollisionTestGroups", 0);
    if (testGroups != 0) {
        m_sceneNode->m_unknown64 = *testGroups;
    }
    int* ignoreSpheres = (int*)FindValueOrDefault("bIgnoreAnimationBoundingSpheres", 0);
    if (ignoreSpheres != 0 && *ignoreSpheres != 0 && m_unknownA0 != 0) {
        m_unknownA0->m_flags |= 0x200000;
    }
    int* copyMaterials = (int*)FindValueOrDefault("bCopyMaterials", 0);
    if (copyMaterials != 0 && *copyMaterials != 0) {
        m_flagsA4 |= kFlagA4_100;
    } else {
        m_flagsA4 &= ~kFlagA4_100;
    }
    static const char* const kNames[3][2] = {
        {"NormalMatAssignment", "NormalMatAssignmentNameSpace"},
        {"BlendMatAssignment", "BlendMatAssignmentNameSpace"},
        {"ZOnlyMatAssignment", "ZOnlyMatAssignmentNameSpace"},
    };
    for (int i = 0; i < 3; i++) {
        const char* name = (const char*)FindValueOrDefault(kNames[i][0], 0);
        const char* ns = (const char*)FindValueOrDefault(kNames[i][1], 0);
        if (name != 0) {
            m_unknown11C[4 + i] = FindMatAssignment(SceneDevice(), name, ns);
        }
    }
    ApplyMatAssignment(1);
    return 0;
}

// ---- the actor's record --------------------------------------------------------------------------------------------------------
// Quirks kept: NearRange is looked up in the actor's own record only, without the defaults. TYPE is asked for with a second argument of 1, which nothing reads. The words are kept as pointers into
// the record's values; only Flags and RecyclableWhileActive are copied, and the second only when present. The node's record is read after, and its answer, always 0, returned.
int GmAcActor::Slot2() {
    if (m_descriptor != 0) {
        m_unknown240[0] = FindValueOrDefault("TYPE", 1);
        int* flags = (int*)FindValueOrDefault("Flags", 0);
        m_unknown14C[4] = flags != 0 ? *flags : 0;
        m_unknown240[1] = FindValueOrDefault("Mass", 0);
        m_unknown240[2] = GmBaObject::FindValue("NearRange", 0);
        m_unknown250 = FindValueOrDefault("AllowEventsWhenDisabled", 0);
        m_unknown254 = (int*)FindValueOrDefault("UpdateActorWhenDrawDisabled", 0);
        m_unknown258[0] = FindValueOrDefault("UpdateChartWhenDrawDisabled", 0);
        m_unknown258[1] = FindValueOrDefault("RicochetShotFlags", 0);
        m_unknown258[2] = FindValueOrDefault("UVAnimationDefaultVelocity", 0);
        m_unknown274 = (int*)FindValueOrDefault("NumEmitters", 0);
        m_unknown278 = (int*)FindValueOrDefault("Num2DEmitters", 0);
        m_unknown27C = (unsigned int*)FindValueOrDefault("MaxDistanceOverride", 0);
        m_unknown23C = (int*)FindValueOrDefault("ConserveVfx", 0);
        m_recordCount = (unsigned int*)FindValueOrDefault("VfxWaitingListSize", 0);
        AllocateRecords();
        int* recyclable = (int*)FindValueOrDefault("RecyclableWhileActive", 0);
        if (recyclable != 0) {
            m_unknown204 = *recyclable;
        }
        m_unknown2A8[1] = FindValueOrDefault("SendOnScreenEvents", 0);
        m_unknown2A8[2] = FindValueOrDefault("SendOOEEvents", 0);
        m_unknown2A8[3] = FindValueOrDefault("WorldCollisionHeight", 0);
    }
    return GmAcNode::Slot2();
}

GmAcActorRecord::GmAcActorRecord() {
    m_unknown00[0] = 0;
    m_unknown00[1] = 0;
    m_unknown08[0] = 0;
    m_unknown08[1] = 0;
    m_unknown08[2] = 0;
    m_unknown14[0] = 0;
    m_unknown14[1] = 0;
    m_unknown14[2] = 0;
    m_unknown20 = 1.0f;
    m_unknown24 = 0;
    m_unknown28 = 0;
    m_unknown34[0] = 0;
    m_unknown2C[0] = 0;
    m_unknown2C[1] = 0;
    m_unknown34[1] = 0;
    m_unknown34[2] = 0;
    m_unknown34[3] = 0;
}

// Only when there are none yet and ConserveVfx is there and not 0. A VfxWaitingListSize of 0 makes none; none given means 3. -2 is raised for no heap, -4 for no memory, then the new records
// are cleared as ClearRecords clears them.
void GmAcActor::AllocateRecords() {
    if (m_records != 0 || m_unknown23C == 0 || *m_unknown23C == 0) {
        return;
    }
    unsigned int count = 3;
    if (m_recordCount != 0) {
        count = *m_recordCount;
        if (count == 0) {
            return;
        }
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_records = new GmAcActorRecord[count];
    AtPopHeap();
    if (m_records == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    ClearRecords();
}
