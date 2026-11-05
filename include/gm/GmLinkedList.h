// GmLinkedList family — recovered types. Evidence classes as in gm/GmSocket.h.
//   ORIGINAL  GmLinkedList, GmLinkedListIterator, GmLinkedListOf<T>, GmLinkedListIteratorOf<T>: names from RTTI.
//             GmLinkedListOf<FaUiBase*> : GmLinkedList(+0), GmPlug(+0x10).
//   INFERRED  GmLinkedListNode and every method/member name.
#pragma once
#include "gm/GmSocket.h"

class GmLinkedList;

// vftable 0x00278560: [0] 0x000770E0 scalar deleting dtor, [1] 0x00076D40. Nodes come only from the static pool:
// 4000 entries of 0x20 bytes at 0x002E26F0, next-search cursor at 0x002E26E8, m_active marks an entry as in use.
class GmLinkedListNode : public GmConnection {
public:
    enum { kPoolSize = 4000 };

    GmLinkedListNode();                    // 0x00076D20
    virtual ~GmLinkedListNode();           // 0x00076FF0
    virtual void Disconnect();             // 0x00076D40 unlinks from both lists and frees the pool entry
    // 0x00076F90. Returns null when the pool is exhausted. Original behaviour: GmLinkedList::Append does not check.
    static GmLinkedListNode* Allocate();

    GmLinkedListNode* next;                // +0x18
    GmLinkedListNode* prev;                // +0x1C

    static int s_cursor;
    static GmLinkedListNode s_pool[kPoolSize];
};

// vftable 0x00278568: [0] 0x00077040 scalar deleting dtor, [1] 0x00077060, [2],[3] inherited, [4] 0x00076E40.
class GmLinkedList : public GmSocket {
public:
    GmLinkedList();                        // 0x00076DA0
    virtual ~GmLinkedList();               // 0x00076DD0
    virtual void Append(GmPlug* item);     // 0x00077060
    virtual int IsEmpty();                 // 0x00076E40

    GmLinkedListNode* head;                // +0x08
    GmLinkedListNode* tail;                // +0x0C
};

// No own vftable entries: every slot is GmLinkedList's. Its destructor (e.g. 0x00097930) is implicit.
template <class T>
class GmLinkedListOf : public GmLinkedList, public GmPlug {
};

// vftable 0x0027BCC4. The destructor is implicit and emitted out of line wherever needed (0x00137C80).
class GmLinkedListIterator : public GmSocketIterator {
public:
    explicit GmLinkedListIterator(GmLinkedList* list);  // 0x00076E50 m_cur is left unset when list is null

    virtual void First();                  // slot 1  0x00076EA0
    virtual void Last();                   // slot 2  0x00076EB0
    virtual void Next();                   // slot 3  0x00076EC0
    virtual void Prev();                   // slot 4  0x00076ED0
    virtual void* NextData();              // slot 5  0x00076F30 returns current payload, then advances
    virtual void* PrevData();              // slot 6  0x00076F50 returns current payload, then steps back
    virtual void* Current();               // slot 7  0x00076F20
    virtual void* At(int index);           // slot 8  0x00076EE0
    virtual int Count();                   // slot 9  0x00076E80
    void RemoveCurrent();                  // 0x00076F70
    void RemoveAll();                      // 0x000770B0

    GmLinkedList* List() const { return static_cast<GmLinkedList*>(m_socket); }

    GmLinkedListNode* m_cur;               // +0x08
};

// Original 32-bit layout (Xbox reference build). Host builds keep the logic, not the byte layout; never load these
// structures from original binary data on a host — parse explicitly instead (docs/portability.md).
// Guarded to modern compilers: original-era MSVC has no static_assert, and the matching build uses no headers.
#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(GmConnection, m_plugNext) == 0x04, "GmConnection::m_plugNext");
static_assert(offsetof(GmConnection, m_plugPrev) == 0x08, "GmConnection::m_plugPrev");
static_assert(offsetof(GmConnection, m_socket) == 0x0C, "GmConnection::m_socket");
static_assert(offsetof(GmConnection, m_plug) == 0x10, "GmConnection::m_plug");
static_assert(offsetof(GmConnection, m_active) == 0x14, "GmConnection::m_active");
static_assert(offsetof(GmLinkedListNode, next) == 0x18, "GmLinkedListNode::next");
static_assert(offsetof(GmLinkedListNode, prev) == 0x1C, "GmLinkedListNode::prev");
static_assert(sizeof(GmLinkedListNode) == 0x20, "pool stride");
static_assert(offsetof(GmPlug, m_connections) == 0x04, "GmPlug::m_connections");
static_assert(offsetof(GmSocket, m_context) == 0x04, "GmSocket::m_context");
static_assert(offsetof(GmLinkedList, head) == 0x08, "GmLinkedList::head");
static_assert(offsetof(GmLinkedList, tail) == 0x0C, "GmLinkedList::tail");
static_assert(sizeof(GmLinkedList) == 0x10, "GmPlug base of GmLinkedListOf<T> sits at +0x10");
static_assert(offsetof(GmSocketIterator, m_socket) == 0x04, "GmSocketIterator::m_socket");
static_assert(offsetof(GmLinkedListIterator, m_cur) == 0x08, "GmLinkedListIterator::m_cur");
#pragma pop_macro("offsetof")
#endif
