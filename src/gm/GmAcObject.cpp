// GmAcObject: see include/gm/GmAcCharacter.h. Names other than the RTTI name are inferred.
#include "gm/GmAcCharacter.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();

namespace {
typedef decltype(sizeof(0)) Size;
struct Placement {};
}  // namespace
inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

// The interaction's two empty slots, each folded in the original with others of the same body. The rest of the class is not recovered: src/gm/GmAcCharSeams.cpp.
void GmAcInteraction::Slot4() {}
void GmAcInteraction::Update(int) {}

GmAcStat* GmAcStatSet::Find(const char* name, int) {
    for (unsigned int i = 0; i < m_count; i++) {
        GmAcStat* stat = m_stats[i];
        if (stat != 0 && AtNamesEqual(stat->m_name, name)) {
            return m_stats[i];
        }
    }
    return 0;
}

GmAcObject::GmAcObject() {
    Reset();
}

GmAcObject::~GmAcObject() {
    GmAcObject::Shutdown();
}

void GmAcObject::Reset() {
    m_flags98 |= kObjectReset;
    m_interaction = 0;
}

// Quirk kept: the interaction is cleared twice, once after deleting it and again by Reset.
void GmAcObject::Shutdown() {
    if (!IsInitialized()) {
        return;
    }
    if (m_interaction != 0) {
        delete m_interaction;
        m_interaction = 0;
    }
    Reset();
    GmAcActor::Shutdown();
}

// The actor's record first; an interaction only for AllowInteraction there and not 0, and only if there is none. Its slot 1 is told this object. The actor's answer is returned.
int GmAcObject::Slot2() {
    int result = GmAcActor::Slot2();
    if (m_descriptor != 0) {
        int* allow = (int*)FindValueOrDefault("AllowInteraction", 0);
        if (allow != 0 && *allow != 0 && m_interaction == 0) {
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_interaction = new GmAcInteraction;
            AtPopHeap();
            if (m_interaction == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            m_interaction->Attach(this);
        }
    }
    return result;
}

void GmAcObject::Slot10() {
    if (IsInitialized()) {
        Shutdown();
    }
    GmAcActor::Slot10();
}

void GmAcObject::Slot20(int a) {
    GmAcActor::Slot20(a);
    if (Slot27() != 0 && m_interaction != 0) {
        m_interaction->Update(a);
    }
}

// The comparison is the original's x87 test: 0 when the value is not above zero, which a NaN is not.
int GmAcObject::Slot55() {
    GmAcStatsHolder* holder = (GmAcStatsHolder*)m_unknown270;
    if (holder != 0) {
        GmAcStat* health = holder->m_stats->Find("sHealth", 1);
        if (health != 0) {
            return health->m_value > 0.0f ? 1 : 0;
        }
    }
    return 1;
}

// One from the heap, or for more than one an array from new[], with the count in the word ahead of the first element.
GmAcObject* GmAcObject::Create(int count) {
    if (count <= 1) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        void* block = operator new(sizeof(GmAcObject));
        GmAcObject* object = block != 0 ? new (block, Placement()) GmAcObject : 0;
        AtPopHeap();
        if (object == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        return object;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    GmAcObject* first = new GmAcObject[count];
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}
