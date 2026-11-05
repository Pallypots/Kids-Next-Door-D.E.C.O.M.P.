// Host semantic tests for recovered GmLinkedListIterator / GmLinkedListOf code.
// These test the recovered SOURCE's behaviour on the host; they are not evidence of a byte match.
// Test-only definitions below stand in for original functions that are not yet recovered; they are never linked
// into a game build and never counted as recovered.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gm/GmLinkedList.h"

// ---- test-only stand-ins (NOT recovered code) ----
GmSocket::~GmSocket() {}
GmPlug::~GmPlug() {}
GmLinkedListIterator::~GmLinkedListIterator() {}
static GmSocket* g_member_of = nullptr;
static GmSocket* g_removed_from = nullptr;
int FaUiBase::IsMemberOf(GmSocket* s) { return s == g_member_of; }
void FaUiBase::RemoveFrom(GmSocket* s) { g_removed_from = s; }
template <class T> GmLinkedListOf<T>::~GmLinkedListOf() {}
template <class T> void GmLinkedListOf<T>::Append(T) {}
template class GmLinkedListOf<FaUiBase*>;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

struct Fixture {
    GmLinkedListOf<FaUiBase*> list;
    GmLinkedListNode n[3];
    int payload[3] = {10, 20, 30};
    GmLinkedListIterator it;
    Fixture() {
        std::memset(n, 0, sizeof n);
        for (int i = 0; i < 3; i++) {
            n[i].data = &payload[i];
            n[i].next = i < 2 ? &n[i + 1] : nullptr;
            n[i].prev = i > 0 ? &n[i - 1] : nullptr;
        }
        list.head = &n[0];
        list.tail = &n[2];
        it.m_list = &list;
        it.m_cur = nullptr;
    }
};

static int val(void* p) { return p ? *static_cast<int*>(p) : -1; }

int main() {
    {
        Fixture f;
        CHECK(f.it.Count() == 3);
        f.it.First();
        CHECK(val(f.it.Current()) == 10);
        f.it.Next();
        CHECK(val(f.it.Current()) == 20);
        f.it.Last();
        CHECK(val(f.it.Current()) == 30);
        f.it.Prev();
        CHECK(val(f.it.Current()) == 20);
        f.it.First();
        f.it.Prev();  // no previous: stays on first
        CHECK(val(f.it.Current()) == 10);
        f.it.Last();
        f.it.Next();  // past the end: becomes null
        CHECK(f.it.Current() == nullptr);
        f.it.Next();  // null-safe
        CHECK(f.it.Current() == nullptr);
    }
    {
        Fixture f;
        f.it.First();
        CHECK(val(f.it.NextData()) == 10);
        CHECK(val(f.it.NextData()) == 20);
        CHECK(val(f.it.NextData()) == 30);
        CHECK(f.it.NextData() == nullptr);
        f.it.Last();
        CHECK(val(f.it.PrevData()) == 30);
        CHECK(val(f.it.PrevData()) == 20);
        CHECK(val(f.it.PrevData()) == 10);
        CHECK(f.it.PrevData() == nullptr);
    }
    {
        Fixture f;
        CHECK(val(f.it.At(0)) == 10);
        CHECK(val(f.it.At(2)) == 30);
        CHECK(f.it.At(3) == nullptr);  // out of range
        CHECK(val(f.it.At(-5)) == 10);  // negative index behaves as 0 (original behaviour)
    }
    {
        Fixture f;
        CHECK(f.list.IsEmpty() == 0);
        f.list.head = nullptr;
        CHECK(f.list.IsEmpty() == 1);
        f.it.m_list = nullptr;
        CHECK(f.it.Count() == 0);  // null list is tolerated by Count()
    }
    {
        Fixture f;
        FaUiBase item;
        g_member_of = &f.list;
        CHECK(f.list.Contains(&item) == 1);
        g_member_of = nullptr;
        CHECK(f.list.Contains(&item) == 0);
        f.list.Remove(&item);
        CHECK(g_removed_from == static_cast<GmSocket*>(&f.list));
    }
    std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
    return g_fail ? 1 : 0;
}
