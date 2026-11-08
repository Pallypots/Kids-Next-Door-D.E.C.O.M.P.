// GmAcCharacter, and GmDynamicNodeLoader's list of characters with shadows: see include/gm/GmAcCharacter.h. Names other than the RTTI name are inferred.
#include "gm/GmAcCharacter.h"
#include "gm/GmLevelManifest.h"

#include "at/AtSceneActor.h"

int AtFormat(char* out, const char* format, ...);  // the C library's sprintf (0x00125559); src/at/AtIoFormatHost.cpp

// ---- the list ------------------------------------------------------------------------------------------------------------------
void GmShadowCastersAdd(GmShadowCasters* list, GmAcCharacter* character) {
    for (unsigned int i = 0; i < 256; i++) {
        GmAcCharacter* held = list->m_casters[i];
        if (held == 0 || held == character) {
            list->m_casters[i] = character;
            return;
        }
    }
}

void GmShadowCastersRemove(GmShadowCasters* list, GmAcCharacter* character) {
    unsigned int i = 0;
    while (list->m_casters[i] != character) {
        i++;
        if (i >= 256) {
            return;
        }
    }
    for (; i < 255; i++) {
        list->m_casters[i] = list->m_casters[i + 1];
    }
    list->m_casters[255] = 0;
}

// ---- GmAcCharacter ---------------------------------------------------------------------------------------------------------
GmAcCharacter::GmAcCharacter() {
    Reset();
}

GmAcCharacter::~GmAcCharacter() {
    GmAcCharacter::Shutdown();
}

void GmAcCharacter::Reset() {
    m_controller = 0;
    m_flags98 |= kCharacterReset;
    m_sightCone = 0;
    m_sightConeLength = 0;
    m_sightConeYaw = 0;
    m_sightConePitch = 0;
    m_sightConeMinDistance = 0;
    m_sightConeBone = 0;
    m_shadow = 0;
    for (int i = 0; i < 8; i++) {
        m_event[i] = 0;
    }
}

// Quirks kept: GmAcObject's shutdown comes first and runs whether or not the character is initialized, as do the rest. "Shadow" is looked up in the name map again and what is found is not used.
// The sight cone and the shadow are read again after their slots, and deleted without being cleared until Reset.
void GmAcCharacter::Shutdown() {
    GmAcObject::Shutdown();
    if (m_sightCone != 0) {
        m_sightCone->Stop();
        if (m_sightCone != 0) {
            delete m_sightCone;
        }
    }
    if (m_shadow != 0) {
        GmShadowCastersRemove((GmShadowCasters*)GmDynamicNodeLoader::s_instance, this);
        s_namedMaps.Find("Shadow");
        m_shadow->Stop();
        if (m_shadow != 0) {
            delete m_shadow;
        }
    }
    Reset();
}

// The sight cone's words, as pointers into the record; the joint by its name in the scene actor's joints; the shadow by its type. Then GmAcObject's record, whose answer is returned.
// Quirks kept: a joint not found is formatted into a message ("Unable to find coord sys %.8s for %.8s", with the joint's name and the node's at +0x90) that nothing prints. The scene actor at +0xA0
// is not tested, nor is the "Shadow" map, nor what the create function returns.
int GmAcCharacter::Slot2() {
    GmAcShadow::Setup setup;
    char message[0x80];
    if (m_descriptor != 0) {
        m_sightConeLength = (float*)FindValueOrDefault("SightConeLength", 0);
        m_sightConeYaw = (float*)FindValueOrDefault("SightConeYaw", 0);
        m_sightConePitch = (float*)FindValueOrDefault("SightConePitch", 0);
        m_sightConeMinDistance = (float*)FindValueOrDefault("SightConeMinDistance", 0);
        const char* bone = (const char*)FindValueOrDefault("SightConeAttachToBone", 0);
        if (bone != 0) {
            AtSceneJointedModelActor* actor = (AtSceneJointedModelActor*)m_unknownA0;
            AtCoordSys* joint = (AtCoordSys*)actor->m_joints.m_database->Find(bone);
            if (joint != 0) {
                m_sightConeBone = joint;
            } else {
                AtFormat(message, "Unable to find coord sys %.8s for %.8s", bone, m_noteBank);
            }
        }
        const char* shadowType = (const char*)FindValueOrDefault("ShadowType", 0);
        if (shadowType != 0) {
            GmNameMap* shadows = (GmNameMap*)s_namedMaps.Find("Shadow");
            GmAcShadow::Registration* registration = (GmAcShadow::Registration*)shadows->Find(shadowType);
            if (registration != 0 && registration->m_create != 0) {
                GmAcShadow* shadow = registration->m_create(-1);
                shadow->m_registration = registration;
                m_shadow = shadow;
                setup.m_owner = this;
                shadow->Configure(&setup);
                m_shadow->Start();
                GmShadowCastersAdd((GmShadowCasters*)GmDynamicNodeLoader::s_instance, this);
            }
        }
    }
    return GmAcObject::Slot2();
}

void GmAcCharacter::Slot4() {
    if (m_sightCone != 0) {
        m_sightCone->Slot5();
    }
    GmAcActor::Slot4();
}

void GmAcCharacter::Slot10() {
    GmAcObject::Slot10();
}

void GmAcCharacter::Slot15() {
    if (m_shadow != 0) {
        m_shadow->Slot8();
    }
    GmAcActor::Slot15();
}

void GmAcCharacter::Slot20(int a) {
    if (Slot27() != 0 && m_sightCone != 0) {
        m_sightCone->Update(a);
    }
    GmAcObject::Slot20(a);
    if (m_shadow != 0 && IsVisible()) {
        m_shadow->Update(a);
    }
}

// Quirk kept: visibility is asked before the shadow is tested.
void GmAcCharacter::Slot21(int a) {
    if (IsVisible() && m_shadow != 0) {
        m_shadow->Slot13(a);
    }
}

void GmAcCharacter::Slot32() {
    if (m_shadow != 0) {
        m_shadow->Slot11();
    }
    GmAcNode::Slot32();
}

void GmAcCharacter::Slot33() {
    if (m_shadow != 0) {
        m_shadow->Slot10();
    }
    GmAcNode::Slot33();
}

void GmAcCharacter::Slot49(const char* event, int b, int c) {
    GmAcActor::Slot49(event, b, c);
}

int GmAcCharacter::Slot55() {
    if (m_sightCone != 0) {
        return m_sightCone->Slot7();
    }
    return GmAcObject::Slot55();
}

int GmAcCharacter::Slot59() {
    if (m_controller == 0) {
        return 0;
    }
    return m_controller->m_unknown04;
}

void GmAcCharacter::SetController(GmAcCharController* controller, GmAcActor* owner) {
    m_controller = controller;
    if (controller != 0) {
        if (owner == 0 && (m_flags98 & 0x100) != 0) {
            owner = this;
        }
        controller->m_owner = owner;
    }
}

void GmAcCharacter::PostEvent() {
    Slot49(m_event, 0, 1);
}
