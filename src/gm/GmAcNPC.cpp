// GmAcNPC, the record reader GmAcNode::FindValueObject, and the health range test: see include/gm/GmAcNPC.h. Names other than RTTI names are inferred.
#include "gm/GmAcNPC.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();

namespace {
typedef decltype(sizeof(0)) Size;
struct Placement {};
}  // namespace
inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

// ---- the reader and the range ----------------------------------------------------------------------------------------------
// Quirks kept: the whole name is measured only to test it for zero; neither the registry nor the record is tested, so a null one is handed to the search.
AtDataComponent* GmAcNode::FindValueObject(const char* name, int) {
    if (name[0] == 0) {
        return 0;
    }
    AtDataLink* link = AtDataRegistryFind((AtDataRegistry*)m_unknown10, (AtDataLink*)m_descriptor, name, 1);
    if (link == 0) {
        return 0;
    }
    return link->m_value;
}

// The original's x87 code keeps the span and each bound in registers: double here. The lower bound is open and the upper closed; an unordered comparison answers 0.
int GmStatInRange(GmAcStat* stat, AtDataComponent* range) {
    if (stat == 0 || range == 0) {
        return 0;
    }
    const AtDataDescriptor* bounds = range->m_descriptor;
    const double value = stat->m_value;
    const double low = stat->m_min;
    const double span = (double)stat->m_max - low;
    if (!(value > span * bounds->m_realFloor + low)) {
        return 0;
    }
    return value <= span * bounds->m_realCeiling + low ? 1 : 0;
}

// ---- GmAcNPC -----------------------------------------------------------------------------------------------------------------
GmAcNPC::GmAcNPC() {
    Reset();
}

GmAcNPC::~GmAcNPC() {
    GmAcNPC::Shutdown();
}

void GmAcNPC::Reset() {
    m_flags98 |= kNPCReset;
    m_aiActions = 0;
}

// Quirk kept: unlike GmAcObject's, it does not ask whether the object is initialized.
void GmAcNPC::Shutdown() {
    if (m_aiActions != 0) {
        delete m_aiActions;
    }
    Reset();
    GmAcCharacter::Shutdown();
}

int GmAcNPC::Slot2() {
    return GmAcCharacter::Slot2();
}

void GmAcNPC::Slot4() {
    GmAcCharacter::Slot4();
}

void GmAcNPC::Slot10() {
    GmAcCharacter::Slot10();
}

// Quirks kept: the record is not tested before the word is looked up (the defaults are searched without one), and a manager already made is not deleted before another is.
void GmAcNPC::Slot15() {
    int* create = (int*)FindValueOrDefault("CreateAiActionManager", 0);
    if (create != 0 && *create != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_aiActions = new GmAiActionManager;
        AtPopHeap();
        if (m_aiActions == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        m_aiActions->Setup(this);
    }
    GmAcCharacter::Slot15();
}

void GmAcNPC::Slot20(int a) {
    GmAcCharacter::Slot20(a);
}

void GmAcNPC::Slot49(const char* event, int b, int c) {
    GmAcCharacter::Slot49(event, b, c);
}

int GmAcNPC::TestCondition(const char*) {
    return 0;
}

// One from the heap, or for more than one an array from new[], with the count in the word ahead of the first element.
GmAcNPC* GmAcNPC::Create(int count) {
    if (count <= 1) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(sizeof(GmAcNPC));
        GmAcNPC* npc = block != 0 ? new (block, Placement()) GmAcNPC : 0;
        AtPopHeap();
        if (npc == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        return npc;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    GmAcNPC* first = new GmAcNPC[count];
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}
