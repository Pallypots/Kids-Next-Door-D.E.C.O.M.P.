// Host semantic tests for the recovered socket/plug/linked-list family. Everything exercised here is recovered source
// (src/gm/): no stand-ins for recovered behaviour. These tests are not evidence of a byte match.
#include <cstdio>

#include "gm/GmLinkedList.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// Test item: the game's real items derive from GmPlug at offset 0 (FaUiBase's destructor ends in ~GmPlug).
class Item : public GmPlug {
public:
    explicit Item(int v) : value(v) {}
    int value;
};

typedef GmLinkedListOf<Item*> List;

static int val(void* p) { return p ? static_cast<Item*>(static_cast<GmPlug*>(p))->value : -1; }

static int active_nodes() {
    int n = 0;
    for (int i = 0; i < GmLinkedListNode::kPoolSize; i++) {
        n += GmLinkedListNode::s_pool[i].m_active ? 1 : 0;
    }
    return n;
}

static void test_append_order_and_iteration() {
    Item a(10), b(20), c(30);
    List list;
    CHECK(list.IsEmpty() == 1);
    list.Append(&a);
    list.Append(&b);
    list.Append(&c);
    CHECK(list.IsEmpty() == 0);
    CHECK(active_nodes() == 3);

    GmLinkedListIterator it(&list);
    CHECK(val(it.Current()) == 10);  // the constructor starts at the head
    CHECK(it.Count() == 3);
    it.Next();
    CHECK(val(it.Current()) == 20);
    it.Last();
    CHECK(val(it.Current()) == 30);
    it.Prev();
    CHECK(val(it.Current()) == 20);
    it.First();
    it.Prev();  // no previous: stays on the first
    CHECK(val(it.Current()) == 10);
    it.Last();
    it.Next();  // past the end: becomes null
    CHECK(it.Current() == nullptr);
    it.Next();  // null-safe
    CHECK(it.Current() == nullptr);

    it.First();
    CHECK(val(it.NextData()) == 10);
    CHECK(val(it.NextData()) == 20);
    CHECK(val(it.NextData()) == 30);
    CHECK(it.NextData() == nullptr);
    it.Last();
    CHECK(val(it.PrevData()) == 30);
    CHECK(val(it.PrevData()) == 20);
    CHECK(val(it.PrevData()) == 10);
    CHECK(it.PrevData() == nullptr);

    CHECK(val(it.At(0)) == 10);
    CHECK(val(it.At(2)) == 30);
    CHECK(it.At(3) == nullptr);     // out of range
    CHECK(val(it.At(-5)) == 10);    // a negative index behaves as 0 (original behaviour)
}

static void test_remove_keeps_links() {
    Item a(1), b(2), c(3);
    List list;
    list.Append(&a);
    list.Append(&b);
    list.Append(&c);
    CHECK(list.Contains(&b) == 1);

    list.Remove(&b);  // middle
    CHECK(list.Contains(&b) == 0);
    CHECK(b.m_connections == nullptr);
    CHECK(list.head->next == list.tail);
    CHECK(list.tail->prev == list.head);

    list.Remove(&a);  // head
    CHECK(val(list.head->m_plug) == 3);
    CHECK(list.head->prev == nullptr);

    list.Remove(&b);  // not a member: no effect
    CHECK(val(list.head->m_plug) == 3);

    list.Remove(&c);  // last
    CHECK(list.IsEmpty() == 1);
    CHECK(list.tail == nullptr);
    CHECK(active_nodes() == 0);
}

static void test_plug_in_two_lists() {
    Item a(1);
    List one, two;
    one.Append(&a);
    two.Append(&a);
    CHECK(one.Contains(&a) == 1);
    CHECK(two.Contains(&a) == 1);
    CHECK(active_nodes() == 2);
    one.Remove(&a);
    CHECK(one.Contains(&a) == 0);
    CHECK(two.Contains(&a) == 1);
    CHECK(a.m_connections != nullptr);
    two.Remove(&a);
    CHECK(a.m_connections == nullptr);
}

static void test_duplicate_membership_removes_one() {
    Item a(1);
    List list;
    list.Append(&a);
    list.Append(&a);
    GmLinkedListIterator it(&list);
    CHECK(it.Count() == 2);
    list.Remove(&a);  // GmPlug::Disconnect stops at the first connection (0x00076C80 returns after the call)
    CHECK(it.Count() == 1);
    CHECK(list.Contains(&a) == 1);
    list.Remove(&a);
    CHECK(list.IsEmpty() == 1);
}

static void test_destroying_a_plug_severs_it() {
    Item a(1), c(3);
    List list;
    list.Append(&a);
    {
        Item b(2);
        list.Append(&b);
        list.Append(&c);
    }
    GmLinkedListIterator it(&list);
    CHECK(it.Count() == 2);
    CHECK(val(it.NextData()) == 1);
    CHECK(val(it.NextData()) == 3);
    list.Remove(&a);
    list.Remove(&c);
    CHECK(active_nodes() == 0);
}

static void test_destroying_a_list_severs_it() {
    Item a(1), b(2);
    {
        List list;
        list.Append(&a);
        list.Append(&b);
        CHECK(a.m_connections != nullptr);
    }
    CHECK(a.m_connections == nullptr);
    CHECK(b.m_connections == nullptr);
    CHECK(active_nodes() == 0);
}

static void test_disconnect_all() {
    Item a(1);
    List one, two;
    one.Append(&a);
    two.Append(&a);
    a.DisconnectAll();
    CHECK(a.m_connections == nullptr);
    CHECK(one.IsEmpty() == 1);
    CHECK(two.IsEmpty() == 1);
    CHECK(active_nodes() == 0);
}

static void test_iterator_removal() {
    Item a(1), b(2), c(3);
    List list;
    list.Append(&a);
    list.Append(&b);
    list.Append(&c);
    GmLinkedListIterator it(&list);
    it.RemoveCurrent();  // removes the first, moves to the following
    CHECK(val(it.Current()) == 2);
    CHECK(list.Contains(&a) == 0);
    it.RemoveAll();
    CHECK(list.IsEmpty() == 1);
    CHECK(it.Current() == nullptr);
    it.RemoveCurrent();  // null-safe
    CHECK(active_nodes() == 0);

    GmLinkedListIterator none(static_cast<GmLinkedList*>(nullptr));
    CHECK(none.Count() == 0);  // a null list is tolerated by the constructor and Count()
}

static void test_pool_allocation() {
    // Allocation searches forward from the cursor and wraps; a freed entry becomes available again.
    GmLinkedListNode* pool = GmLinkedListNode::s_pool;
    const int n = GmLinkedListNode::kPoolSize;
    GmLinkedListNode::s_cursor = n - 1;
    GmLinkedListNode* last = GmLinkedListNode::Allocate();
    CHECK(last == &pool[n - 1]);
    CHECK(last->m_active == 1);
    GmLinkedListNode* wrapped = GmLinkedListNode::Allocate();
    CHECK(wrapped == &pool[0]);  // the cursor stayed on the used entry, then the search wrapped
    last->m_active = 0;
    wrapped->m_active = 0;

    for (int i = 0; i < n; i++) {
        CHECK(GmLinkedListNode::Allocate() != nullptr);
    }
    CHECK(active_nodes() == n);
    CHECK(GmLinkedListNode::Allocate() == nullptr);  // exhausted
    pool[7].m_active = 0;
    CHECK(GmLinkedListNode::Allocate() == &pool[7]);
    for (int i = 0; i < n; i++) {
        pool[i].m_active = 0;
    }
    GmLinkedListNode::s_cursor = 0;
}

int main() {
    test_append_order_and_iteration();  // leaves three nodes in use only until its list goes out of scope
    CHECK(active_nodes() == 0);
    test_remove_keeps_links();
    test_plug_in_two_lists();
    test_duplicate_membership_removes_one();
    test_destroying_a_plug_severs_it();
    test_destroying_a_list_severs_it();
    test_disconnect_all();
    test_iterator_removal();
    test_pool_allocation();
    std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
    return g_fail ? 1 : 0;
}
