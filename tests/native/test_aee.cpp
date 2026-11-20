// Host test of the .AEE reader and an event's subscriptions and publishing through the real text tokenizer, on any pointer size. Not evidence of a byte match. Load is left out, for want of a
// file system here. No game data.
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtEvent.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

struct Text {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    std::vector<char> bytes;
    explicit Text(const char* source) : bytes(source, source + std::strlen(source)) {
        stream.m_base = bytes.data();
        stream.m_cur = bytes.data();
        stream.m_limit = bytes.data() + bytes.size();
        stream.m_size = (unsigned int)bytes.size();
        tokenizer.SetStream(&stream);
    }
    ~Text() {
        stream.m_base = nullptr;
        stream.m_cur = nullptr;
        stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

int g_heap;

struct Owner {
    AtDataRegistryOwner* owner;
    Owner() {
        owner = (AtDataRegistryOwner*)::operator new(sizeof(AtDataRegistryOwner));
        std::memset((void*)owner, 0, sizeof(AtDataRegistryOwner));
        new (&owner->m_databases) GmNameMapGroup;
        new (&owner->m_databaseSpaces) GmNameSpaceDb;
        owner->m_databaseSpaces.Init(8);
    }
};

std::string g_calls;
struct Listener { char name; };
void Note(void* subscriber, AtEventMessage* message) {
    g_calls += ((Listener*)subscriber)->name;
    g_calls += (char)('0' + message->m_mask);
}
void Leave(void* subscriber, AtEventMessage* message) {
    g_calls += ((Listener*)subscriber)->name;
    g_calls += '!';
    message->m_event->Unsubscribe(subscriber, Leave, 0);
}

void TestRead() {
    Owner o;
    AtEventDatabase db;
    Text text("NameSpace \"Level\"\nNumSubscribers 3\nShortName[3]\n{\n \"Start\" // a comment\n \"Mid\"\n \"End\"\n}\n");
    db.Read(o.owner, &text.tokenizer);
    CHECK(db.m_list.m_count == 3);
    CHECK(db.m_subscriptionCount == 3);
    CHECK(db.m_subscriptions != nullptr);
    AtEvent* start = (AtEvent*)db.Find("Start");
    AtEvent* end = (AtEvent*)db.Find("End");
    CHECK(start == &db.m_list.m_entries[0]);
    CHECK(end == &db.m_list.m_entries[2]);
    CHECK(db.Find("Other") == nullptr);
    for (unsigned int i = 0; i < 3; i++) {
        CHECK(db.m_list.m_entries[i].m_database == &db);
        CHECK(db.m_list.m_entries[i].m_flags == AtEvent::kEnabled);
        CHECK(db.m_list.m_entries[i].m_subscriptions == nullptr);
        CHECK(db.m_subscriptions[i].m_subscriber == nullptr);
    }
    CHECK(db.m_group != nullptr && db.m_group != &o.owner->m_databases);
    CHECK(db.m_group != nullptr && std::strncmp(db.m_group->m_name, "Level", 8) == 0);
    CHECK(o.owner->m_databaseSpaces.Find("Level", "Mid") == &db.m_list.m_entries[1]);

    // Subscribing, publishing in order, by mask; a full block; leaving while being called.
    Listener a = {'a'}, b = {'b'}, c = {'c'}, d = {'d'};
    start->Subscribe(&a, Note, 1);
    start->Subscribe(&b, Leave, 3);
    start->Subscribe(&c, Note, 2);
    CHECK(db.FindFreeSubscription() == nullptr);
    start->Subscribe(&d, Note, 1);  // none free
    AtEventMessage m = {start, 1};
    g_calls.clear();
    start->Publish(&m);
    CHECK(g_calls == "a1b!");
    m.m_mask = 3;
    g_calls.clear();
    start->Publish(&m);
    CHECK(g_calls == "a3c3");
    start->Subscribe(&a, Note, 2);  // again: mask replaced, nothing added
    m.m_mask = 1;
    g_calls.clear();
    start->Publish(&m);
    CHECK(g_calls.empty());
    start->m_flags = 0;
    m.m_mask = 2;
    g_calls.clear();
    start->Publish(&m);
    CHECK(g_calls.empty());
    start->m_flags = AtEvent::kEnabled;
    start->Unsubscribe(&a, Leave, 0);  // the wrong handler
    g_calls.clear();
    start->Publish(&m);
    CHECK(g_calls == "a2c2");
    start->Unsubscribe(&a, nullptr, 0);
    CHECK(start->m_subscriptions != nullptr && start->m_subscriptions->m_subscriber == &c);
    CHECK(db.FindFreeSubscription() != nullptr);

    // A reload clears first; the database leaves its name space when destroyed.
    Text again("ShortName[1]\n{\n \"Only\"\n}\n");
    db.Read(o.owner, &again.tokenizer);
    CHECK(db.m_list.m_count == 1);
    CHECK(db.m_subscriptionCount == AtEventDatabase::kDefaultSubscriptions);
    CHECK(db.m_group == &o.owner->m_databases);
    CHECK(db.Find("Start") == nullptr);
    CHECK(db.Find("Only") == &db.m_list.m_entries[0]);
}

void TestQuirks() {
    Owner o;
    // No ShortName: no list, so Clear does nothing and the block stays.
    AtEventDatabase db;
    Text text("NumSubscribers 4\nNonsense 5 \"x\"\n");
    db.Read(o.owner, &text.tokenizer);
    CHECK(db.m_list.m_entries == nullptr);
    CHECK(db.m_subscriptionCount == 4);
    db.Clear();
    CHECK(db.m_subscriptions != nullptr);
    ::operator delete(db.m_subscriptions);
    db.m_subscriptions = nullptr;
    // ShortName[0] still makes an array.
    AtEventDatabase empty;
    Text none("ShortName[0]\n{\n}\n");
    empty.Read(o.owner, &none.tokenizer);
    CHECK(empty.m_list.m_entries != nullptr);
    CHECK(empty.m_list.m_count == 0);
    CHECK(empty.m_subscriptionCount == 100);
}

}  // namespace

int main() {
    AtIoTokenFile::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtEventList::SetHeap(&g_heap);
    AtEventDatabase::SetHeap(&g_heap);
    TestRead();
    TestQuirks();
    if (g_fail == 0) std::printf("test_aee: all passed\n");
    return g_fail ? 1 : 0;
}
