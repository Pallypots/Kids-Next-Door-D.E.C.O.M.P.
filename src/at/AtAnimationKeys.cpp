// The base of an animation's keys, and an animation read from a binary record. See include/at/AtAnimationDatabase.h and include/at/AtGrD3DDatabase.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

namespace {

// A word copied as it is, real or not. The records' reals are copied whole by the original, never through the floating-point unit, so a signalling value stays one.
inline void CopyWord(float* to, const float* from) {
    *(unsigned int*)to = *(const unsigned int*)from;
}

}  // namespace

AtAnimationKeys::AtAnimationKeys() : m_rotationKeyCount(0), m_translationKeyCount(0), m_scaleKeyCount(0), m_noteKeyCount(0), m_cameraKeyCount(0) {
}

AtAnimationKeys::~AtAnimationKeys() {
    AtAnimationKeys::Clear();
}

void AtAnimationKeys::Clear() {
    if (m_rotationKeyCount > 0) {
        Slot14();
    }
    if (m_translationKeyCount > 0) {
        Slot15();
    }
    if (m_scaleKeyCount > 0) {
        Slot16();
    }
    if (m_noteKeyCount > 0) {
        Slot17();
    }
    if (m_cameraKeyCount > 0) {
        Slot18();
    }
    AtGrKeySet::Clear();
    m_rotationKeyCount = 0;
    m_translationKeyCount = 0;
    m_scaleKeyCount = 0;
    m_noteKeyCount = 0;
    m_cameraKeyCount = 0;
}

void AtAnimationKeys::SetExternalName(const char* name) {
    AtGrKeySet::SetExternalName(name);
    m_flags |= 0x150;
}

void AtAnimationKeys::Init(unsigned int rotations, unsigned int translations, unsigned int scales, unsigned int notes, unsigned int cameras, unsigned int frames) {
    if (m_flags & kRead) {
        Clear();
    }
    Reserve(frames);
    m_rotationKeyCount = rotations;
    if (rotations != 0) {
        Slot9(rotations);
    }
    m_translationKeyCount = translations;
    if (translations != 0) {
        Slot10(translations);
    }
    m_scaleKeyCount = scales;
    if (scales != 0) {
        Slot11(scales);
    }
    m_noteKeyCount = notes;
    if (notes != 0) {
        Slot12(notes);
    }
    m_cameraKeyCount = cameras;
    if (cameras != 0) {
        Slot13(cameras);
    }
    m_flags |= 0xA8;
}

void AtAnimation::ReadBinary(AtAnimationDatabase* database, const AtXkaAnimation* record) {
    m_database = database;
    m_flags = record->m_flags;
    CopyWord(&m_rate, &record->m_rate);
    CopyWord(&m_velocity[0], &record->m_velocity[0]);
    CopyWord(&m_velocity[1], &record->m_velocity[1]);
    CopyWord(&m_velocity[2], &record->m_velocity[2]);
    CopyWord(&m_angularVelocity[0], &record->m_angularVelocity[0]);
    CopyWord(&m_angularVelocity[1], &record->m_angularVelocity[1]);
    CopyWord(&m_angularVelocity[2], &record->m_angularVelocity[2]);
    m_rotation[0] = (unsigned short)record->m_rotation[0];
    m_rotation[1] = (unsigned short)record->m_rotation[1];
    m_translation[0] = (unsigned short)record->m_translation[0];
    m_translation[1] = (unsigned short)record->m_translation[1];
    m_scale[0] = (unsigned short)record->m_scale[0];
    m_scale[1] = (unsigned short)record->m_scale[1];
    m_note[0] = (unsigned short)record->m_note[0];
    m_note[1] = (unsigned short)record->m_note[1];
    m_camera[0] = (unsigned short)record->m_camera[0];
    m_camera[1] = (unsigned short)record->m_camera[1];
    m_length = (unsigned short)record->m_length;
    m_loopPeriod = (unsigned short)record->m_loopPeriod;
    CopyWord(&m_boundingSphere.center.x, &record->m_boundingSphere[0]);
    CopyWord(&m_boundingSphere.center.y, &record->m_boundingSphere[1]);
    CopyWord(&m_boundingSphere.center.z, &record->m_boundingSphere[2]);
    CopyWord(&m_boundingSphere.radius, &record->m_boundingSphere[3]);
}
