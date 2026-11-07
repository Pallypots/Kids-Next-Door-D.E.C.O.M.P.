// See include/at/AtInputMapper.h.
#include "at/AtInputMapper.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtInputLinkList::s_heap;
void* AtInputRouter::s_heap;

namespace {

// The original has this inline, once for each kind of chain.
int UnbindFrom(AtInputController::Input* inputs, AtInputController::Chain* chains, unsigned int count, AtInputLink* link) {
    unsigned int index = 0;
    while (index < count && link->m_id != inputs[index].m_id) {
        index++;
    }
    if (index >= count) {
        return 0;
    }
    AtInputController::Chain* chain = &chains[index];
    AtInputLink* next = link->m_next;
    if (link->m_prev != 0) {
        link->m_prev->m_next = next;
        if (next != 0) {
            next->m_prev = link->m_prev;
        } else {
            chain->m_last = link->m_prev;
        }
    } else {
        chain->m_first = next;
        if (next != 0) {
            next->m_prev = 0;
        } else {
            chain->m_last = 0;
        }
    }
    return 1;
}

void PushFree(AtInputRouter* router, AtInputLink* link) {
    link->m_next = router->m_freeFirst;
    if (router->m_freeFirst != 0) {
        router->m_freeFirst->m_prev = link;
    } else {
        router->m_freeLast = link;
    }
    router->m_freeFirst = link;
    link->m_prev = 0;
}

AtInputLink* NewLinks(unsigned int count) {
    if (AtInputRouter::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtInputRouter::s_heap);
    AtInputLink* links = (AtInputLink*)operator new(count * sizeof(AtInputLink));
    AtPopHeap();
    if (links == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned char* bytes = (unsigned char*)links;
    for (unsigned int i = 0; i < count * sizeof(AtInputLink); i++) {
        bytes[i] = 0;
    }
    return links;
}

}  // namespace

int AtInputController::UnbindButton(AtInputLink* link) {
    return UnbindFrom(m_buttonInputs, m_buttonChains, m_buttonInputCount, link);
}

int AtInputController::UnbindAxis(AtInputLink* link) {
    return UnbindFrom(m_axisInputs, m_axisChains, m_axisInputCount, link);
}

void AtInputLinkList::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputLinkList::AtInputLinkList() {
    m_items = 0;
    m_count = 0;
    m_capacity = 0;
}

AtInputLinkList::~AtInputLinkList() {
    if (m_items != 0) {
        operator delete(m_items);
    }
    m_items = 0;
    m_count = 0;
    m_capacity = 0;
}

void AtInputLinkList::Clear() {
    if (m_items != 0) {
        operator delete(m_items);
    }
    m_items = 0;
    m_count = 0;
    m_capacity = 0;
}

void AtInputLinkList::Allocate(unsigned int capacity) {
    if (m_items != 0) {
        operator delete(m_items);
    }
    m_items = 0;
    m_count = 0;
    m_capacity = capacity;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_items = (AtInputLink**)operator new(m_capacity * sizeof(AtInputLink*));
    AtPopHeap();
    if (m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_capacity; i++) {
        m_items[i] = 0;
    }
}

int AtInputLinkList::Remove(AtInputLink* link) {
    unsigned int index = 0;
    if (link != m_items[0]) {
        while (index < m_count) {
            AtInputLink* item = m_items[index + 1];
            index++;
            if (link == item) {
                break;
            }
        }
    }
    if (link != m_items[index]) {
        return 0;
    }
    for (; index < m_count - 1; index++) {
        m_items[index] = m_items[index + 1];
    }
    m_count--;
    return 1;
}

void AtInputRouter::SetHeap(void* heap) {
    s_heap = heap;
}

AtInputRouter::AtInputRouter() {
    m_freeFirst = 0;
    m_freeLast = 0;
    m_device = 0;
    m_buttonLinks = 0;
    m_axisLinks = 0;
    m_bindFunction = 0;
    m_passOnTo = 0;
}

AtInputRouter::~AtInputRouter() {
    Release();
}

void AtInputRouter::Slot0(void* a) {
    if (m_passOnTo != 0) {
        m_passOnTo->Slot0(a);
    }
}

void AtInputRouter::Slot1(void* a) {
    if (m_passOnTo != 0) {
        m_passOnTo->Slot1(a);
    }
}

void AtInputRouter::Slot2(void* a) {
    if (m_passOnTo != 0) {
        m_passOnTo->Slot2(a);
    }
}

void AtInputRouter::Slot3(void* a, void* b) {
    if (m_passOnTo != 0) {
        m_passOnTo->Slot3(a, b);
    }
}

void AtInputRouter::AllocateButtonLinks(unsigned int count) {
    if (count == 0) {
        return;
    }
    m_buttonLinks = NewLinks(count);
    m_boundButtons.Allocate(count);
    for (unsigned int i = 0; i < count; i++) {
        PushFree(this, &m_buttonLinks[i]);
    }
}

void AtInputRouter::AllocateAxisLinks(unsigned int count) {
    if (count == 0) {
        return;
    }
    m_axisLinks = NewLinks(count);
    m_boundAxes.Allocate(count);
    for (unsigned int i = 0; i < count; i++) {
        PushFree(this, &m_axisLinks[i]);
    }
}

void AtInputRouter::UnbindAll() {
    while (m_boundButtons.m_count != 0) {
        AtInputLink* link = m_boundButtons.m_items[0];
        link->m_controller->UnbindButton(link);
        m_boundButtons.Remove(link);
        PushFree(this, link);
    }
    while (m_boundAxes.m_count != 0) {
        AtInputLink* link = m_boundAxes.m_items[0];
        link->m_controller->UnbindAxis(link);
        m_boundAxes.Remove(link);
        PushFree(this, link);
    }
}

void AtInputRouter::Unbind(AtInputController* controller, int id) {
    for (unsigned int i = 0; i < m_boundButtons.m_count; i++) {
        AtInputLink* link = m_boundButtons.m_items[i];
        if (link->m_id == id && link->m_controller == controller) {
            controller->UnbindButton(link);
            PushFree(this, link);
            m_boundButtons.Remove(link);
            i = (unsigned int)-1;  // From the start again.
        }
    }
    if (id & 0x40000000) {
        for (unsigned int i = 0; i < m_boundAxes.m_count; i++) {
            AtInputLink* link = m_boundAxes.m_items[i];
            if (link->m_id == id && link->m_controller == controller) {
                controller->UnbindAxis(link);
                PushFree(this, link);
                m_boundAxes.Remove(link);
                i = (unsigned int)-1;
            }
        }
    }
    if (m_passOnTo != 0) {
        m_passOnTo->Unbind(controller, id);
    }
}

void AtInputRouter::Release() {
    UnbindAll();
    m_boundButtons.Clear();
    m_boundAxes.Clear();
    if (m_buttonLinks != 0) {
        operator delete(m_buttonLinks);
    }
    if (m_axisLinks != 0) {
        operator delete(m_axisLinks);
    }
    m_device = 0;
    m_buttonLinks = 0;
    m_axisLinks = 0;
    m_bindFunction = 0;
    m_passOnTo = 0;
}

void AtInputRouter::Allocate(void* device, unsigned int buttons, unsigned int axes) {
    Release();
    m_device = device;
    m_linkCount = buttons + axes;
    AllocateButtonLinks(buttons);
    AllocateAxisLinks(axes);
}
