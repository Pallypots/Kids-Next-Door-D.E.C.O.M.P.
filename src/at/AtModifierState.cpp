// The modifiers in force at a point of a space graph. See include/at/AtModifierState.h.
#include "at/AtModifierState.h"

#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtModifier.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtModifierState::s_heap;
void* AtModifierStack::s_heap;

namespace {

// The heap gives 32-byte alignment while these are made. A heap aligned more than that stays as it is.
void AlignHeapForSlots() {
    AtHeap* heap = (AtHeap*)AtModifierState::s_heap;
    heap->m_allocShift = heap->m_alignShift >= 5 ? heap->m_alignShift : 5;
}

void RestoreHeapAlignment() {
    AtHeap* heap = (AtHeap*)AtModifierState::s_heap;
    heap->m_allocShift = heap->m_alignShift;
}

AtModifierSlot* NewSlots(unsigned int count) {
    AtModifierSlot* made = (AtModifierSlot*)operator new(count * sizeof(AtModifierSlot));
    if (made != 0) {
        for (unsigned int i = 0; i < count; i++) {
            new (made + i, AtPlacement()) AtModifierSlot;
        }
    }
    return made;
}

}  // namespace

AtModifierSlot::AtModifierSlot() {
    m_count = 0;
    for (int i = 0; i < kMaxEntries; i++) {
        m_entries[i].m_modifier = 0;
        m_entries[i].m_data = 0;
    }
}

void AtModifierSlot::Insert(AtModifier* modifier, int data) {
    Entry* at = m_entries;
    Entry* end = m_entries + m_count;
    while (at < end && modifier->m_priority <= at->m_modifier->m_priority) {
        at++;
    }
    if (at == end) {
        if (m_count < kMaxEntries) {
            m_count++;
            at->m_data = data;
            at->m_modifier = modifier;
        }
        return;
    }
    if (m_count >= kMaxEntries) {
        end--;
        m_count--;
    }
    for (Entry* to = end; to > at; to--) {
        to[0] = to[-1];
    }
    m_count++;
    at->m_modifier = modifier;
    at->m_data = data;
}

AtModifierState::AtModifierState() {
    m_count = 0;
    m_slots = 0;
}

AtModifierState::~AtModifierState() {
    if (m_slots != 0) {
        operator delete(m_slots);
    }
    m_count = 0;
    m_slots = 0;
}

void AtModifierState::SetHeap(void* heap) {
    s_heap = heap;
}

void AtModifierState::Init(unsigned int slots, int fromFileHeap) {
    if (m_slots != 0) {
        operator delete(m_slots);
        m_count = 0;
        m_slots = 0;
    }
    m_count = slots;
    AlignHeapForSlots();
    // Asking for the file heap checks that it exists; the other was trusted the moment it was aligned.
    if (fromFileHeap != 0) {
        if (AtIoTokenFile::s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(AtIoTokenFile::s_heap);
    } else {
        AtPushHeap(s_heap);
    }
    m_slots = NewSlots(m_count);
    AtPopHeap();
    if (m_slots == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    RestoreHeapAlignment();
}

void AtModifierState::Assign(const AtModifierState& from) {
    if (m_slots != 0) {
        operator delete(m_slots);
        m_count = 0;
        m_slots = 0;
    }
    m_count = from.m_count;
    AlignHeapForSlots();
    AtPushHeap(s_heap);
    m_slots = NewSlots(m_count);
    AtPopHeap();
    if (m_slots == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    RestoreHeapAlignment();
    CopyFrom(from);
}

void AtModifierState::CopyFrom(const AtModifierState& from) {
    for (unsigned int i = 0; i < m_count; i++) {
        const AtModifierSlot& source = from.m_slots[i];
        m_slots[i].m_count = source.m_count;
        // A count past the end copies nothing, and stays as the count.
        if (source.m_count <= AtModifierSlot::kMaxEntries) {
            for (unsigned int j = source.m_count; j > 0; j--) {
                m_slots[i].m_entries[j - 1].m_modifier = source.m_entries[j - 1].m_modifier;
            }
        }
    }
}

void AtModifierState::Set(AtModifier* modifier) {
    const unsigned int kind = (unsigned int)modifier->m_kind;
    if (kind >= m_count) {
        return;
    }
    AtModifierSlot& slot = m_slots[kind];
    if (slot.m_count != 0 && slot.m_entries[0].m_modifier->m_priority > modifier->m_priority) {
        return;
    }
    slot.m_count = 1;
    slot.m_entries[0].m_modifier = modifier;
    slot.m_entries[0].m_data = 0;
}

// ---- the stack a graph walks with ------------------------------------------------------------------------------------------------------

void AtModifierStack::SetHeap(void* heap) {
    s_heap = heap;
}

void AtModifierStack::Free() {
    if (m_states != 0) {
        AtDeleteArray(m_states);
    }
    m_depth = 0;
    m_used = 0;
    m_states = 0;
}

void AtModifierStack::Init(unsigned int depth, unsigned int slots, int fromFileHeap) {
    if (m_states != 0) {
        AtDeleteArray(m_states);
        m_depth = 0;
        m_used = 0;
        m_states = 0;
    }
    m_depth = depth;
    void* heap = fromFileHeap != 0 ? AtIoTokenFile::s_heap : s_heap;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    m_states = AtNewArray<AtModifierState>(m_depth);
    AtPopHeap();
    if (m_states == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < depth; i++) {
        m_states[i].Init(slots, fromFileHeap);
    }
    m_used = 1;
}

void AtModifierStack::Push() {
    m_states[m_used + 1].CopyFrom(m_states[m_used]);
    m_used++;
}

void AtModifierStack::Pop() {
    m_used--;
}
