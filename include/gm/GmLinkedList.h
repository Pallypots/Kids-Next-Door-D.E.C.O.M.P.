// GmSocket / GmPlug / GmLinkedList / GmLinkedListIterator — recovered types (partial).
//
// Reference build: knd-xbox-usa-5454008a-v1 (docs/provenance.md).
// Evidence classes:
//   ORIGINAL  class names and inheritance, from MSVC RTTI (type descriptors + class hierarchy descriptors):
//               GmLinkedListOf<FaUiBase*> : GmLinkedList(+0) : GmSocket(+0), and GmPlug(+0x10)   [multiple inh.]
//               GmLinkedListIteratorOf<FaUiBase*> : GmLinkedListIterator : GmSocketIterator : GmIterator
//   OBSERVED  field offsets and vftable slot order, from the instruction stream of the functions cited below.
//   INFERRED  every member/method name (no symbols survive in the retail XBE).
// Unobserved bytes are padding named by offset. Virtual functions are declared where the body was verified; which
// base class first introduces each slot is NOT yet known.
#pragma once

struct GmLinkedListNode {
    unsigned char unk_00[0x10];
    void* data;              // +0x10 payload (Current/At/NextData)
    int unk_14;              // +0x14 set to 1 by Append (0x00077060)
    GmLinkedListNode* next;  // +0x18
    GmLinkedListNode* prev;  // +0x1C
};

class GmSocket {
public:
    virtual ~GmSocket();
    int unk_04;              // +0x04 not yet observed
};

class GmPlug {
public:
    virtual ~GmPlug();
};

class GmLinkedList : public GmSocket {
public:
    GmLinkedListNode* head;  // +0x08
    GmLinkedListNode* tail;  // +0x0C
};

// Payload class for the only observed instantiation. Layout unknown; the membership query at 0x00076CB0 walks a
// chain at +0x04 of the item (entries: +0x04 next, +0x0C owning socket). Owner class of that method is INFERRED.
class FaUiBase {
public:
    int IsMemberOf(GmSocket* socket);   // 0x00076CB0 (not yet recovered)
    void RemoveFrom(GmSocket* socket);  // 0x00076C80 (not yet recovered)
};

template <class T>
class GmLinkedListOf : public GmLinkedList, public GmPlug {
public:
    virtual ~GmLinkedListOf();           // slot 0  0x00097990 (not yet recovered)
    virtual void Append(T item);         // slot 1  0x00077060 (not yet recovered)
    virtual void Remove(T item);         // slot 2  0x00075FF0
    virtual int Contains(T item);        // slot 3  0x00075FE0
    virtual int IsEmpty();               // slot 4  0x00076E40
};

template <class T>
void GmLinkedListOf<T>::Remove(T item) {
    item->RemoveFrom(this);
}

template <class T>
int GmLinkedListOf<T>::Contains(T item) {
    return item->IsMemberOf(this);
}

template <class T>
int GmLinkedListOf<T>::IsEmpty() {
    return head == 0;
}

class GmLinkedListIterator {
public:
    virtual ~GmLinkedListIterator();       // slot 0  0x00097A20 scalar deleting dtor (not yet recovered)
    virtual void First();                  // slot 1  0x00076EA0
    virtual void Last();                   // slot 2  0x00076EB0
    virtual void Next();                   // slot 3  0x00076EC0
    virtual void Prev();                   // slot 4  0x00076ED0
    virtual void* NextData();              // slot 5  0x00076F30  returns current payload, then advances
    virtual void* PrevData();              // slot 6  0x00076F50  returns current payload, then steps back
    virtual void* Current();               // slot 7  0x00076F20
    virtual void* At(int index);           // slot 8  0x00076EE0
    virtual int Count();                   // slot 9  0x00076E80

    GmLinkedList* m_list;                  // +0x04 (may be declared in GmSocketIterator as a GmSocket*)
    GmLinkedListNode* m_cur;               // +0x08
};

// Original 32-bit layout (Xbox reference build). Host builds keep the logic, not the byte layout; never load these
// structures from original binary data on a host — parse explicitly instead (docs/portability.md).
// Guarded to modern compilers: original-era MSVC has no static_assert, and the matching build uses no headers.
#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(GmLinkedListNode, data) == 0x10, "GmLinkedListNode::data");
static_assert(offsetof(GmLinkedListNode, next) == 0x18, "GmLinkedListNode::next");
static_assert(offsetof(GmLinkedListNode, prev) == 0x1C, "GmLinkedListNode::prev");
static_assert(offsetof(GmLinkedList, head) == 0x08, "GmLinkedList::head");
static_assert(offsetof(GmLinkedList, tail) == 0x0C, "GmLinkedList::tail");
static_assert(sizeof(GmLinkedList) == 0x10, "GmPlug base of GmLinkedListOf<T> sits at +0x10");
static_assert(offsetof(GmLinkedListIterator, m_list) == 0x04, "GmLinkedListIterator::m_list");
static_assert(offsetof(GmLinkedListIterator, m_cur) == 0x08, "GmLinkedListIterator::m_cur");
#undef offsetof
#endif
