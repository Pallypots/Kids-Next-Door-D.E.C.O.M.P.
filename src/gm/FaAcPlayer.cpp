// FaAcPlayer, the playable characters, and what it holds: see include/gm/GmAcPlayer.h. Names other than the RTTI name are inferred; record words are the files' own.
#include "gm/GmAcPlayer.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

namespace {
typedef decltype(sizeof(0)) Size;
struct Placement {};

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

const float kDefaultMinMoveStick = 0.2f;   // 0x0027B52C
const float kDefaultMaxLook = 60.0f;       // 0x0027B540
const float kDefaultMinLook = -60.0f;      // 0x0027B534
const float kDegreesToRadians = 0.0174532924f;  // 0x0027B558
const double kTurnScale = 1.5;             // 0x0027B76C, a float in the original, used in the x87's precision
}  // namespace
inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

// ---- names and records -----------------------------------------------------------------------------------------------------------
int AtNameLength(const char* name) {
    int length = 0;
    while (length < 8 && name[length] != 0) {
        length++;
    }
    return length;
}

// Quirk kept: the whole name is measured only to test it for zero.
AtDataLink* GmAcNode::FindRecord(const char* name, int) {
    Size length = 0;
    while (name[length] != 0) {
        length++;
    }
    if (length == 0) {
        return 0;
    }
    return AtDataRegistryFind((AtDataRegistry*)m_unknown10, (AtDataLink*)m_descriptor, name, 1);
}

// ---- the sight query -----------------------------------------------------------------------------------------------------------
unsigned int GmAcSightQuery::s_count;
AtSegmentQuery GmAcSightQuery::s_query;

GmAcSightQuery::GmAcSightQuery() {
    m_unknown24 = 1.0f;
    m_unknown28 = 1.0f;
    m_unknown1C = -1.0f;
    m_unknown18 = 0;
    if (s_query.m_capacity == 0) {
        s_count = 0;
        s_query.AtSegmentQuery::Allocate(3);
    }
    s_count++;
}

GmAcSightQuery::~GmAcSightQuery() {
    s_count--;
    if (s_query.m_capacity != 0 && s_count == 0) {
        s_query.AtSegmentQuery::Clear();
    }
    m_unknown1C = -1.0f;
    m_unknown24 = 1.0f;
    m_unknown28 = 1.0f;
}

// ---- the target sprite ---------------------------------------------------------------------------------------------------------
GmAcSprite::GmAcSprite() {
    m_unknown20 = 0;
    m_scene = 0;
    m_unknown98 = 0;
    m_material = 0;
    m_unknownA0 = 0;
}

GmAcSprite::~GmAcSprite() {
    Clear();
}

void GmAcSprite::Slot2(int) {}

void GmAcSprite::Clear() {
    m_unknown20 = 0;
    m_scene = 0;
    m_unknown98 = 0;
    m_material = 0;
    m_unknownA0 = 0;
}

// ---- FaAcPlayer ------------------------------------------------------------------------------------------------------------
FaAcPlayer::FaAcPlayer() {
    Reset();
}

FaAcPlayer::~FaAcPlayer() {
    FaAcPlayer::Shutdown();
}

// Quirk kept: the combat object at +0x2E4 is cleared without being deleted; the shutdown deletes it first.
void FaAcPlayer::Reset() {
    m_flags98 |= kFaPlayerReset;
    m_followMaxHorizontalLook = 0;
    m_followMinVerticalLook = 0;
    m_followMaxVerticalLook = 0;
    m_fpMaxZoom = 0;
    m_fpHorizontalDegreesPerSec = 0;
    m_fpVerticalDegreesPerSec = 0;
    m_fpMaxVerticalLook = 0;
    m_jumpsAllowed = 0;
    m_powuhUp = 0;
    m_powuhUpDecay = 0;
    m_powuhUpDeduct = 0;
    m_powuhUpRecord = 0;
    m_level2x4PickupType = 0;
    m_level2x4PickupName = 0;
    m_unknown3A4 = 0;
    m_unknown3B8[0] = 0;
    m_unknown3B8[1] = 0;
    m_head = 0;
    m_unknown334 = 0;
    m_unknown364[0] = 0;
    m_unknown364[1] = 0;
    m_combat = 0;
    m_unknown3A8 = 1;
    m_unknown3AC = 0;
    m_unknown3B0 = 1;
    m_cosOfAngleToSwitchMouthSides = 0;
    m_unknown3DC[0] = 0;
    m_unknown3DC[1] = 0;
    for (int i = 0; i < 6; i++) {
        m_unknown3E4[i] = 0;
    }
    m_unknown3FC = -1;
    m_unknown400[0] = 0;
    m_unknown400[1] = 0;
    m_unknown408 = 1;
    m_unknown40C = 2;
    m_gameAxisForward = -1;
    m_gameAxisTurning = -1;
    m_unknown4C0 = 0.0f;
    m_unknown4C4[0] = 0;
    m_unknown4C4[1] = 0;
    m_unknown4C4[2] = 0;
}

// The camera, jump and powuh words as pointers into the record, the input's ids for the two game axes, and the powuh record's two words; then GmAcPlayer's, whose answer it returns.
// Quirks kept: PowuhUp's name is copied as at most eight characters before it is looked for; the record is looked for only when the name has a character. The words of the powuh record are not
// cleared when it or they are not found.
int FaAcPlayer::Slot2() {
    char name[12];
    m_followMaxHorizontalLook = (float*)FindValueOrDefault("FollowMaxHorizontalLook", 0);
    m_followMinVerticalLook = (float*)FindValueOrDefault("FollowMinVerticalLook", 0);
    m_followMaxVerticalLook = (float*)FindValueOrDefault("FollowMaxVerticalLook", 0);
    m_fpMaxZoom = (float*)FindValueOrDefault("FPMaxZoom", 0);
    m_fpHorizontalDegreesPerSec = (float*)FindValueOrDefault("FPHorizontalDegreesPerSec", 0);
    m_fpVerticalDegreesPerSec = (float*)FindValueOrDefault("FPVerticalDegreesPerSec", 0);
    m_fpMaxVerticalLook = (float*)FindValueOrDefault("FPMaxVerticalLook", 0);
    m_jumpsAllowed = (int*)FindValueOrDefault("JumpsAllowed", 0);
    m_cosOfAngleToSwitchMouthSides = (float*)FindValueOrDefault("CosOfAngleToSwitchMouthSides", 0);
    m_powuhUp = (const char*)FindValueOrDefault("PowuhUp", 0);
    m_level2x4PickupType = FindValueOrDefault("Level2x4PickupType", 0);
    m_level2x4PickupName = FindValueOrDefault("Level2x4PickupName", 0);
    m_gameAxisForward = GmPlayerApp::s_instance->m_input->FindAxis("GameAxisForward");
    m_gameAxisTurning = GmPlayerApp::s_instance->m_input->FindAxis("GameAxisTurning");
    if (m_powuhUp != 0 && AtNameLength(m_powuhUp) > 0) {
        AtCopyName9(name, m_powuhUp);
        m_powuhUpRecord = FindRecord(name, 0);
        if (m_powuhUpRecord != 0) {
            AtDataLink* link = AtDataRegistryFind((AtDataRegistry*)m_unknown10, m_powuhUpRecord, "PowuhUpDecay", 1);
            if (link != 0) {
                m_powuhUpDecay = link->m_value->ElementAt(0);
            }
            link = AtDataRegistryFind((AtDataRegistry*)m_unknown10, m_powuhUpRecord, "PowuhUpDeduct", 1);
            if (link != 0) {
                m_powuhUpDeduct = link->m_value->ElementAt(0);
            }
        }
    }
    return GmAcPlayer::Slot2();
}

// Quirk kept: the combat object is deleted and left in place for Reset to clear.
void FaAcPlayer::Shutdown() {
    if (m_combat != 0) {
        delete m_combat;
    }
    m_target.Clear();
    Reset();
    GmAcPlayer::Shutdown();
}

void FaAcPlayer::Slot4() {
    GmAcPlayer::Slot4();
}

void FaAcPlayer::Slot10() {
    GmAcPlayer::Slot10();
}

void FaAcPlayer::Slot20(int) { AtFatal("Gm player: FaAcPlayer slot 20, 0x00094A80, is not recovered", 0, 0, 0); }
void FaAcPlayer::Slot21(int) { AtFatal("Gm player: FaAcPlayer slot 21, 0x00093450, is not recovered", 0, 0, 0); }

// Each axis is 0 when its size is below MinMoveStickMagnitude (0.2 without one). With either left: the controller's frame, made level against the world's up with right across it, both turned
// about up by m_unknown4C0 degrees; then right scaled by turning times its size times 1.5, and forward by the forward axis negated; 1. With neither: the node's frame, and 0.
// The original tests the square root of the sum of squares against zero, in the x87's precision; the sum in double is above zero exactly when that is. The scale of right is kept in double, as
// the x87 kept it in a register; it is exact there, and the product with each component is rounded twice in both, to 53 or 64 bits and then to a float.
int FaAcPlayer::MoveFrame(AtVector3* forward, AtVector3* up, AtVector3* right) {
    GmPlayerInput* input = GmPlayerApp::s_instance->m_input;
    float turning = input->AxisValue(Slot59(), m_gameAxisTurning);
    input = GmPlayerApp::s_instance->m_input;
    float moving = input->AxisValue(Slot59(), m_gameAxisForward);
    if (Abs(turning) < (m_minMoveStick != 0 ? *m_minMoveStick : kDefaultMinMoveStick)) {
        turning = 0.0f;
    }
    if (Abs(moving) < (m_minMoveStick != 0 ? *m_minMoveStick : kDefaultMinMoveStick)) {
        moving = 0.0f;
    }
    if ((double)moving * moving + (double)turning * turning > 0.0) {
        ControllerFrame(forward, up, right);
        *up = GmWorldAxes::s_instance->m_up;
        forward->Reject(*up);
        forward->Normalize();
        right->SetCrossNormalized(*forward, *up);
        forward->Rotate(*up, m_unknown4C0 * kDegreesToRadians);
        right->Rotate(*up, m_unknown4C0 * kDegreesToRadians);
        const double scale = (double)Abs(turning) * kTurnScale * turning;
        right->x = (float)(scale * right->x);
        right->y = (float)(scale * right->y);
        right->z = (float)(scale * right->z);
        const float back = -moving;
        forward->x = back * forward->x;
        forward->y = back * forward->y;
        forward->z = back * forward->z;
        return 1;
    }
    Slot22(0, forward, up, right, 0);
    return 0;
}

int FaAcPlayer::Slot61(float, float) {
    return 0;
}

float FaAcPlayer::FollowMaxVerticalLook() {
    if (m_followMaxVerticalLook != 0) {
        return *m_followMaxVerticalLook;
    }
    return kDefaultMaxLook;
}

float FaAcPlayer::FollowMinVerticalLook() {
    if (m_followMinVerticalLook != 0) {
        return *m_followMinVerticalLook;
    }
    return kDefaultMinLook;
}

// The two look axes of this player's controller; kept again unless the controller has a frame pending. Quirk kept: the controller is not tested.
void FaAcPlayer::ReadLook() {
    m_look[0] = GmPlayerApp::s_instance->m_input->AxisValue(Slot59(), 0x84);
    m_look[1] = GmPlayerApp::s_instance->m_input->AxisValue(Slot59(), 0x85);
    if (m_controller->HasPending() == 0) {
        m_unknown318[0] = m_look[0];
        m_unknown318[1] = m_look[1];
    }
}

int FaAcPlayer::Slot66() {
    return m_unknown40C;
}

// One from the heap, or for more than one an array from new[], with the count in the word ahead of the first element.
FaAcPlayer* FaAcPlayer::Create(int count) {
    if (count <= 1) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(sizeof(FaAcPlayer));
        FaAcPlayer* player = block != 0 ? new (block, Placement()) FaAcPlayer : 0;
        AtPopHeap();
        if (player == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        return player;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    FaAcPlayer* first = new FaAcPlayer[count];
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}
