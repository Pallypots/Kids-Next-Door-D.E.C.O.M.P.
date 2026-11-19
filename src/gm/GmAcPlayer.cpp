// GmAcPlayer, the controller frame's methods, and the loader's hold on the player: see include/gm/GmAcPlayer.h and include/gm/GmAcCharacter.h. Names other than the RTTI name are inferred.
#include "gm/GmAcPlayer.h"
#include "gm/GmLevelManifest.h"

GmPlayerApp* GmPlayerApp::s_instance;
GmWorldAxes* GmWorldAxes::s_instance;

namespace {

// fabs as the x87 does it: the sign bit cleared, so that -0 is +0.
float Abs(float value) {
    union {
        float f;
        unsigned int u;
    } bits;
    bits.f = value;
    bits.u &= 0x7FFFFFFFu;
    return bits.f;
}

}  // namespace

// ---- the loader's hold on the player -----------------------------------------------------------------------------------------
// Quirk kept: a loop over one slot.
void GmLoadedPlayersAdd(GmLoadedPlayers* loader, GmAcPlayer* player) {
    for (unsigned int i = 0; i < 1; i++) {
        if (loader->m_players[i] == 0) {
            loader->m_players[i] = player;
            loader->m_count++;
            return;
        }
    }
}

// ---- the controller frame ------------------------------------------------------------------------------------------------------
GmAcCharController::GmAcCharController() {
    m_unknown00 = 0;
    m_owner = 0;
    m_pending = 0;
    m_unknown04 = -1;
    m_right.x = 0.0f;
    m_right.y = 0.0f;
    m_right.z = 0.0f;
    m_up.x = 0.0f;
    m_up.y = 0.0f;
    m_up.z = 0.0f;
    m_forward.x = 0.0f;
    m_forward.y = 0.0f;
    m_forward.z = 0.0f;
    m_pendingForward.x = 0.0f;
    m_pendingForward.y = 0.0f;
    m_pendingForward.z = 0.0f;
    m_pendingUp.x = 0.0f;
    m_pendingUp.y = 0.0f;
    m_pendingUp.z = 0.0f;
    m_pendingRight.x = 0.0f;
    m_pendingRight.y = 0.0f;
    m_pendingRight.z = 0.0f;
    m_right.x = 1.0f;
    m_forward.y = 1.0f;
    m_up.z = 1.0f;
}

// Quirk kept: the destructor makes the controller new again, word for word.
GmAcCharController::~GmAcCharController() {
    m_unknown00 = 0;
    m_owner = 0;
    m_pending = 0;
    m_unknown04 = -1;
    m_right.x = 0.0f;
    m_right.y = 0.0f;
    m_right.z = 0.0f;
    m_up.x = 0.0f;
    m_up.y = 0.0f;
    m_up.z = 0.0f;
    m_forward.x = 0.0f;
    m_forward.y = 0.0f;
    m_forward.z = 0.0f;
    m_pendingForward.x = 0.0f;
    m_pendingForward.y = 0.0f;
    m_pendingForward.z = 0.0f;
    m_pendingUp.x = 0.0f;
    m_pendingUp.y = 0.0f;
    m_pendingUp.z = 0.0f;
    m_pendingRight.x = 0.0f;
    m_pendingRight.y = 0.0f;
    m_pendingRight.z = 0.0f;
    m_right.x = 1.0f;
    m_forward.y = 1.0f;
    m_up.z = 1.0f;
}

void GmAcCharController::Activate() {
    m_unknown00 = 1;
}

int GmAcCharController::HasPending() {
    return m_pending;
}

void GmAcCharController::GetFrame(AtVector3* forward, AtVector3* up, AtVector3* right) {
    *forward = m_forward;
    *up = m_up;
    *right = m_right;
}

void GmAcCharController::SetFrame(const AtVector3* forward, const AtVector3* up, const AtVector3* right, int now) {
    if (now != 0) {
        if (m_owner != 0) {
            static_cast<GmAcPlayer*>(m_owner)->ReadLook();
        }
        m_forward = *forward;
        m_up = *up;
        m_right = *right;
        m_pending = 0;
    } else {
        if (m_owner != 0) {
            static_cast<GmAcPlayer*>(m_owner)->ReadLook();
        }
        m_pendingForward = *forward;
        m_pendingUp = *up;
        m_pendingRight = *right;
        m_pending = 1;
    }
}

void GmAcCharController::ApplyPending() {
    if (m_pending != 0) {
        m_forward = m_pendingForward;
        m_up = m_pendingUp;
        m_right = m_pendingRight;
        m_pending = 0;
    }
}

// ---- GmAcPlayer ------------------------------------------------------------------------------------------------------------
GmAcPlayer::GmAcPlayer() {
    Reset();
}

GmAcPlayer::~GmAcPlayer() {
    GmAcPlayer::Shutdown();
}

void GmAcPlayer::Reset() {
    m_flags98 |= kPlayerReset;
    m_unknown304 = 0;
    m_unknown308 = 0;
    m_unknown318[0] = 0.0f;
    m_unknown318[1] = 0.0f;
    m_look[0] = 0.0f;
    m_look[1] = 0.0f;
    m_unknown30C = 1;
    m_backwardsThreshold = 0;
    m_partner = 0;
    m_minMoveStick = 0;
}

void GmAcPlayer::Shutdown() {
    Reset();
    GmAcCharacter::Shutdown();
}

int GmAcPlayer::Slot2() {
    m_backwardsThreshold = (float*)FindValueOrDefault("BackwardsThresholdTurningValue", 0);
    m_minMoveStick = (float*)FindValueOrDefault("MinMoveStickMagnitude", 0);
    return GmAcCharacter::Slot2();
}

void GmAcPlayer::Slot4() {
    GmAcCharacter::Slot4();
}

void GmAcPlayer::Slot10() {
    GmAcCharacter::Slot10();
    GmLoadedPlayersAdd((GmLoadedPlayers*)GmDynamicNodeLoader::s_instance, this);
    m_sceneNode->m_unknown60 = 2;
}

void GmAcPlayer::Slot11() {
    m_link = 0;
}

// With a partner, and the node's own slot 27 (called directly, not this class's) answering 0: the partner's position and forward, the world's up, the forward made level and unit, and right
// across them, become this player's frame.
void GmAcPlayer::Slot20(int a) {
    AtVector3 position;
    AtVector3 forward;
    AtVector3 up;
    AtVector3 right;
    if (m_partner != 0 && GmAcNode::Slot27() == 0) {
        m_partner->Slot22(&position, &forward, &up, &right, 0);
        up = GmWorldAxes::s_instance->m_up;
        forward.Reject(up);
        forward.Normalize();
        right.SetCrossNormalized(forward, up);
        Slot23(&position, &forward, &up, &right, 0);
    }
    GmAcCharacter::Slot20(a);
}

int GmAcPlayer::Slot27() {
    return 1;
}

int GmAcPlayer::Slot42() {
    return 1;
}

// Kind 1: whether the +0x304 object's +0x3C28 is clear. Kind 0 with names: the "BIGBOX" they hold decides, by that same word. Anything else, and no query: 1.
int GmAcPlayer::Slot44(GmAcQuery* query) {
    if (query != 0) {
        if (query->m_kind == 1) {
            return m_unknown304->m_unknown3C28 == 0;
        }
        if (query->m_kind == 0 && query->m_names != 0) {
            GmAcQuery* bigBox = (GmAcQuery*)query->m_names->Find("BIGBOX");
            if (m_unknown304->m_unknown3C28 != 0) {
                if (bigBox != 0 && query != bigBox) {
                    return 0;
                }
            } else {
                return query != bigBox;
            }
        }
    }
    return 1;
}

int GmAcPlayer::MoveFrame(AtVector3* forward, AtVector3* up, AtVector3* right) {
    Slot22(0, forward, up, right, 0);
    return 0;
}

// The original's x87 tests: a above zero, which a NaN is not; |b| below the threshold, which a NaN is not.
int GmAcPlayer::Slot61(float a, float b) {
    if (m_unknown30C != 0 && a > 0.0f) {
        const float turning = Abs(b);
        const float threshold = m_backwardsThreshold != 0 ? *m_backwardsThreshold : 0.0f;
        if (turning < threshold) {
            return 1;
        }
    }
    return 0;
}

float GmAcPlayer::Slot62() {
    return 0.0f;
}

float GmAcPlayer::FollowMaxVerticalLook() {
    return 0.0f;
}

float GmAcPlayer::FollowMinVerticalLook() {
    return 0.0f;
}

int GmAcPlayer::Slot66() {
    return 0xFFFF;
}

// Quirk kept: a partner let go of is told to drop its controller only when no new one comes; the flag tested is the one every character sets.
void GmAcPlayer::SetPartner(GmAcActor* partner) {
    if (partner == 0 && m_partner != 0 && (m_partner->m_flags98 & GmAcCharacter::kCharacterReset) != 0) {
        static_cast<GmAcCharacter*>(m_partner)->SetController(0, 0);
    }
    m_partner = partner;
    if (partner != 0 && (partner->m_flags98 & GmAcCharacter::kCharacterReset) != 0) {
        static_cast<GmAcCharacter*>(partner)->SetController(m_controller, this);
    }
}

void GmAcPlayer::ApplyPendingFrame() {
    m_controller->ApplyPending();
}

void GmAcPlayer::SetControllerFrame(const AtVector3* forward, const AtVector3* up, const AtVector3* right, int now) {
    if (m_controller != 0) {
        m_controller->SetFrame(forward, up, right, now);
    }
}

void GmAcPlayer::ControllerFrame(AtVector3* forward, AtVector3* up, AtVector3* right) {
    m_controller->GetFrame(forward, up, right);
}

int GmAcPlayer::HasPendingFrame() {
    if (m_controller != 0 && m_controller->HasPending() != 0) {
        return 1;
    }
    return 0;
}
