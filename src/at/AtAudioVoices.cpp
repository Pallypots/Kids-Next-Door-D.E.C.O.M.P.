// What the sound manager's playing needs of the Atlas classes beneath it: an emitter's voice list, placing an emitter in the world, and a scene node's position. See include/at/AtAudioDatabase.h,
// include/at/AtCoordSys.h and include/at/AtScene.h. Verification status per function: config/functions.d/gmsound.toml. Names inferred.
#include "at/AtAudioDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtScene.h"

void AtFatal(const char* message, int a, int b, int c);

// ---- an emitter's voices --------------------------------------------------------------------------------------------------

AtAudioVoice* AtAudioEmitter::IsPlaying(const AtAudioSegment* segment) {
    for (AtAudioVoice* voice = (AtAudioVoice*)m_firstVoice; voice != 0; voice = voice->m_next) {
        if (voice->m_segment == segment) {
            return voice;
        }
    }
    return 0;
}

AtAudioVoice* AtAudioEmitter::FirstVoice() {
    return (AtAudioVoice*)m_firstVoice;
}

AtAudioVoice* AtAudioEmitter::NextVoice(AtAudioVoice* voice) {
    return voice->m_next;
}

// ---- the owner's loader: the slots the sound manager calls and nothing here recovers ------------------------------------

namespace {
void LoaderSlotNotRecovered() {
    AtFatal("Atlas Sound: this slot of the sound system is not recovered", 0, 0, 0);
}
}  // namespace

void AtAudioBankLoader::Unknown4() { LoaderSlotNotRecovered(); }
void AtAudioBankLoader::Unknown5() { LoaderSlotNotRecovered(); }
void AtAudioBankLoader::Unknown6() { LoaderSlotNotRecovered(); }
void AtAudioBankLoader::Wait(float) { LoaderSlotNotRecovered(); }

// ---- placing a coordinate system ------------------------------------------------------------------------------------------

void AtCoordSys::MarkChildrenDirty(unsigned int mask) {
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->m_dirty |= mask;
    }
}

void AtCoordSys::LocalFromWorld() {
    MarkChildrenDirty(kAllLocal);
    m_dirty |= kAllLocal;
    if (m_parent == 0) {
        m_dirty &= ~(unsigned int)kWorld;
        g_atMatrix.Copy(m_local, m_world);
        return;
    }
    m_parent->UpdateInverse();
    g_atMatrix.SetProduct(m_local, m_world, m_parent->m_world);
}

void AtCoordSys::Place(const AtVector3* axis, float angle, const AtVector3* position, const AtVector3* with) {
    g_atMatrix.MakeAxisAngle(m_world, axis, angle);
    g_atMatrix.SetTranslation(m_world, position);
    if (with != 0) {
        g_atMatrix.ScaleAxes(m_world, with);
    }
    LocalFromWorld();
}

void AtCoordSys::WorldPosition(AtVector3* out) {
    if (m_parent == 0) {
        g_atMatrix.GetPosition(m_local, out);
        return;
    }
    UpdateWorld();
    g_atMatrix.GetPosition(m_world, out);
}

// ---- a scene node ---------------------------------------------------------------------------------------------------------

void AtSceneNode::GetWorldPosition(AtVector3* out) {
    if (m_coordSys != 0) {
        m_coordSys->WorldPosition(out);
        return;
    }
    out->x = 0;
    out->y = 0;
    out->z = 0;
}
