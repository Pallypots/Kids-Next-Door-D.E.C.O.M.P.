// AtEvSubscriber, GmBaObject, the animation controller and GmAcNode: see include/gm/GmAcActor.h. Names other than the RTTI names are inferred.
#include "at/AtMatAssignment.h"
#include "gm/GmAcActor.h"

void AtFatal(const char* message, int file, int line, int unused);
void AtCopyName(const char* from, char* to);  // 0x00013ED0 at most eight bytes; stops after a terminator

// ---- AtEvSubscriber ------------------------------------------------------------------------------------------------------
AtEvSubscriber::~AtEvSubscriber() {
}

// ---- GmBaObject ----------------------------------------------------------------------------------------------------------
GmBaObject::GmBaObject() {
    m_initialized = 0;
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_unknown10 = 0;
    m_descriptor = 0;
}

GmBaObject::~GmBaObject() {
    if (GmBaObject::IsInitialized()) {
        GmBaObject::Shutdown();
    }
}

int GmBaObject::Initialize() {
    if (IsInitialized()) {
        Shutdown();
    }
    m_initialized = 1;
    return 0;
}

int GmBaObject::Slot2() {
    return 0;
}

void GmBaObject::Shutdown() {
    m_initialized = 0;
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_unknown10 = 0;
    m_descriptor = 0;
}

void GmBaObject::Slot4() {
}

int GmBaObject::IsInitialized() {
    return m_initialized;
}

const char* GmBaObject::Name() {
    return m_descriptor != 0 ? m_descriptor->m_name : 0;
}

GmBaObject* GmBaObject::NewArray(unsigned int count) {
    return new GmBaObject[count];
}

// ---- the animation controller --------------------------------------------------------------------------------------------
void GmAcAnimSlots::Free() {
    if (m_data != 0) {
        typedef decltype(sizeof(0)) Size;
        Size* block = (Size*)m_data - 1;
        for (Size i = *block; i > 0; i--) {
            m_data[i - 1].~GmAcAnimSlot();
        }
        operator delete(block);
        m_data = 0;
    }
    m_count = 0;
}

GmAcAnimController::GmAcAnimController() {
    m_unknown04 = 0;
    m_unknown20 = 0;
    m_unknown21 = 0;
    m_owner = 0;
    m_unknown30 = 0;
}

// With an owner, what Clear does, with the slots' Resize(0) written out; then the members' destructors empty the slots again and clear the blend again.
GmAcAnimController::~GmAcAnimController() {
    if (m_owner != 0) {
        Detach();
        m_slots.Free();
        m_blend.Clear();
        m_unknown04 = 0;
        m_unknown20 = 0;
        m_unknown21 = 0;
        m_owner = 0;
        m_unknown30 = 0;
    }
}

void GmAcAnimController::Slot1(int a) {
    m_owner->Slot40(a);
}

void GmAcAnimController::Slot2(int a) {
    m_owner->Slot41(a);
}

void GmAcAnimController::Clear() {
    if (m_owner != 0) {
        Detach();
        m_slots.Resize(0);
        m_blend.Clear();
        m_unknown04 = 0;
        m_unknown20 = 0;
        m_unknown21 = 0;
        m_owner = 0;
        m_unknown30 = 0;
    }
}

// ---- GmAcNode ------------------------------------------------------------------------------------------------------------
GmAcNode::GmAcNode() {
    m_unknown144 = 0;
    m_unknown148 = 0;
    m_flags98 = 0;
    Reset();
}

GmAcNode::~GmAcNode() {
    GmAcNode::Shutdown();
    m_unknown144 = 0;
    m_unknown148 = 0;
}

void GmAcNode::Reset() {
    m_sceneNode = 0;
    m_unknownA0 = 0;
    m_unknown140 = 1;
    for (int i = 0; i < 3; i++) {
        m_unknown18[i] = 0;
    }
    for (int i = 0; i < 9; i++) {
        m_unknown30[i] = 0;
    }
    for (int i = 0; i < 3; i++) {
        m_unknown84[i] = 0;
    }
    for (int i = 0; i < 3; i++) {
        m_unknown54[i] = 0;
    }
    for (int i = 0; i < 6; i++) {
        m_unknown6C[i] = 0;
    }
    for (int i = 0; i < 7; i++) {
        m_unknownE0[i] = 0;
    }
    m_link = 0;
    m_flagsA4 = 0;
    m_noteBank[0] = 0;
    for (int i = 0; i < 7; i++) {
        m_unknown11C[i] = 0;
    }
    m_matAssignment = 0;
    m_unknown13C = 0;
    m_flags98 |= kNodeReset;
    for (int i = 0; i < 3; i++) {
        m_unknownFC[i] = 254.0f;
    }
    for (int i = 0; i < 5; i++) {
        m_unknown108[i] = 255.0f;
    }
}

// The link is deleted but m_link is not cleared until Reset. The owner's lists are told through their slot 2.
void GmAcNode::Shutdown() {
    GmAcNodeLink* link = m_link;
    if (link != 0) {
        GmAcNodeLinkOwner* owner = link->m_owner;
        if (owner != 0) {
            owner->Second()->Remove(link);
            owner->First()->Remove(link);
        }
        link->m_unknown08 = 0;
        link->m_unknown0C = 0;
        link->m_owner = 0;
        link->m_unknown14 = 0;
        link->m_unknown18 = 1;
        if (m_link != 0) {
            delete m_link;
        }
    }
    if (m_matAssignment != 0) {
        delete m_matAssignment;
    }
    if (m_unknown13C != 0) {
        delete m_unknown13C;
    }
    if (m_sceneNode != 0) {
        m_sceneNode->m_unknown40 = 0;
    }
    m_animation.Clear();
    Reset();
    GmBaObject::Shutdown();
}

int GmAcNode::Slot7(int) {
    return Initialize();
}

void GmAcNode::Setup(void* a, int, int c, Descriptor* descriptor) {
    m_unknown10 = a;
    m_descriptor = descriptor;
    char name[8];
    AtCopyName(descriptor->m_name, name);
    Slot8(c, name);
}

int GmAcNode::Slot13() {
    return (m_flagsA4 >> 8) & 1;
}

void GmAcNode::SetFlag10(int on) {
    if (on) {
        m_flagsA4 |= kFlagA4_10;
    } else {
        m_flagsA4 &= ~kFlagA4_10;
    }
}

void GmAcNode::SetUnknown140(int value) {
    m_unknown140 = value;
}

GmAcAnimController* GmAcNode::Animation() {
    return &m_animation;
}

int GmAcNode::SetupAndConfigure(void* a, Descriptor* descriptor) {
    m_unknown10 = a;
    m_descriptor = descriptor;
    return Slot2();
}

// Empty slots, each folded in the original with others of the same body.
void GmAcNode::Slot14(int) {}
void GmAcNode::Slot16() {}
void GmAcNode::Slot18(int) {}
void GmAcNode::Slot21(int) {}
void GmAcNode::Slot36(int, int) {}
void GmAcNode::Slot37(int) {}
void GmAcNode::Slot38(int) {}
void GmAcNode::Slot40(int) {}
void GmAcNode::Slot41(int) {}
int GmAcNode::Slot42() { return 0; }

// Slots not recovered. Each stops with a diagnostic naming its original.
void GmAcNode::Slot4() { AtFatal("Gm actor: GmAcNode slot 4, 0x0014D2E0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot8(int, const char*) { AtFatal("Gm actor: GmAcNode slot 8, 0x0014CEB0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot10() { AtFatal("Gm actor: GmAcNode slot 10, 0x0014BBC0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot11() { AtFatal("Gm actor: GmAcNode slot 11, 0x0014E2C0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot12() { AtFatal("Gm actor: GmAcNode slot 12, 0x0014D0F0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot15() { AtFatal("Gm actor: GmAcNode slot 15, 0x0014DB70, is not recovered", 0, 0, 0); }
void GmAcNode::Slot17(int) { AtFatal("Gm actor: GmAcNode slot 17, 0x0014D070, is not recovered", 0, 0, 0); }
void GmAcNode::Slot19() { AtFatal("Gm actor: GmAcNode slot 19, 0x0014D120, is not recovered", 0, 0, 0); }
void GmAcNode::Slot20(int) { AtFatal("Gm actor: GmAcNode slot 20, 0x0014E4D0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot22(AtVector3*, AtVector3*, AtVector3*, AtVector3*, AtVector3*) { AtFatal("Gm actor: GmAcNode slot 22, 0x0014BD90, is not recovered", 0, 0, 0); }
void GmAcNode::Slot23(AtVector3*, AtVector3*, AtVector3*, AtVector3*, AtVector3*) { AtFatal("Gm actor: GmAcNode slot 23, 0x0014BE10, is not recovered", 0, 0, 0); }
void GmAcNode::Slot24(int, int, int, int, int) { AtFatal("Gm actor: GmAcNode slot 24, 0x0014BF20, is not recovered", 0, 0, 0); }
void GmAcNode::Slot25(int, int, int, int, int) { AtFatal("Gm actor: GmAcNode slot 25, 0x0014BFB0, is not recovered", 0, 0, 0); }
int GmAcNode::Slot27() { AtFatal("Gm actor: GmAcNode slot 27, 0x0014C170, is not recovered", 0, 0, 0); return 0; }
void GmAcNode::Slot28() { AtFatal("Gm actor: GmAcNode slot 28, 0x0014C040, is not recovered", 0, 0, 0); }
void GmAcNode::Slot29() { AtFatal("Gm actor: GmAcNode slot 29, 0x0014C080, is not recovered", 0, 0, 0); }
void GmAcNode::Slot30() { AtFatal("Gm actor: GmAcNode slot 30, 0x0014C6B0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot31() { AtFatal("Gm actor: GmAcNode slot 31, 0x0014C6F0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot32() { AtFatal("Gm actor: GmAcNode slot 32, 0x0014C0C0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot33() { AtFatal("Gm actor: GmAcNode slot 33, 0x0014C120, is not recovered", 0, 0, 0); }
void GmAcNode::Slot34() { AtFatal("Gm actor: GmAcNode slot 34, 0x0014C730, is not recovered", 0, 0, 0); }
void GmAcNode::Slot35() { AtFatal("Gm actor: GmAcNode slot 35, 0x0014C7B0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot39() { AtFatal("Gm actor: GmAcNode slot 39, 0x0014C8A0, is not recovered", 0, 0, 0); }
void GmAcNode::Slot43(int, int, int) { AtFatal("Gm actor: GmAcNode slot 43, 0x0014D600, is not recovered", 0, 0, 0); }
int GmAcNode::Slot44(GmAcQuery*) { AtFatal("Gm actor: GmAcNode slot 44, 0x0014D800, is not recovered", 0, 0, 0); return 0; }
