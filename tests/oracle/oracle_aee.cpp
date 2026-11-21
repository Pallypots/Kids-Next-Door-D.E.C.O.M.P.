// Differential oracle for events by name, .AEE files (Windows x86, local only). See docs/verification.md.
//
//   oracle_aee <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AEE file listed in index.txt, loaded through the file code on both sides, loaded over itself, cleared, loaded again, then subscribed to, published and unsubscribed from
// by a fixed pseudo-random sequence, and destroyed; text written for each word and error path, with the report coming back, and with the report ending the load as it does in the game;
// the reader alone with no memory; the event list's Allocate alone with no heap and no memory; the heap setters; Subscribe, Unsubscribe and Publish on written cases (a full block,
// handlers that unsubscribe themselves or subscribe others while a message is published, a disabled event, a message naming another event).
//
// What is real on both sides: the file code, tokenizer, name map, name space database and the owner's AddDatabase.
// Doubles on both sides, so no evidence about them: the heap (new, delete, push, pop), the fatal report and the error routine, and the handlers, which stand for game code, scripts and
// sound: each logs its subscriber, the message and its mask, and some subscribe or unsubscribe through their side's code. The original's handlers are __thiscall, the recovered side's
// plain functions (see include/at/AtEvent.h).
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <new>
#include <string>
#include <vector>

#include "at/AtEvent.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#define AtFatal AtFatalInDoubles
#include "oracle_doubles.h"
#undef AtFatal

// A report does not return in the game. Here it does; after this many in one load a deliberate crash ends the load, at the same report on both sides. Zero ends it at the first.
static int g_reports;
static int g_report_limit = 40;
static void count_report() {
    if (g_expect_crash && ++g_reports > g_report_limit) *(volatile int*)nullptr = 0;
}
void AtFatal(const char* m, int a, int b, int c) { AtFatalInDoubles(m, a, b, c); count_report(); }
static void __cdecl o_fatal2(const char* m, int a, int b, int c) { o_fatal(m, a, b, c); count_report(); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap, g_list_heap, g_sub_heap;

static uint64_t g_hash[2];
static void note(uint32_t a, uint32_t b = 0) {
    if (!g_recording) return;
    g_log[g_side].add(a, b);
    g_hash[g_side] = (g_hash[g_side] ^ a) * 0x100000001B3ULL;
    g_hash[g_side] = (g_hash[g_side] ^ b) * 0x100000001B3ULL;
}

typedef AtEventDatabase Db;
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, uint32_t);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnSub)(void*, void*, void*, void*, unsigned);
typedef void*(__fastcall* FnFind)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__cdecl* FnHeap)(void*);

// ---- owners, subscribers, handlers, messages: one set to a side --------------------------------------------------------------------
static AtDataRegistryOwner* g_owner[2];
static Db* g_db[2];  // the database each side is working on, for the handlers
static void make_owners() {
    for (int side = 0; side < 2; side++) {
        g_owner[side] = (AtDataRegistryOwner*)std::calloc(1, sizeof(AtDataRegistryOwner));
        new (&g_owner[side]->m_databases) GmNameMapGroup;
        new (&g_owner[side]->m_databaseSpaces) GmNameSpaceDb;
        g_owner[side]->m_databaseSpaces.Init(8);
    }
}

enum { kSubscribers = 6, kHandlers = 4 };
struct Subscriber { int id; int pad[3]; };
static Subscriber g_subs[2][kSubscribers];
static int sub_of(int side, const void* p) {
    for (int k = 0; k < kSubscribers; k++) if (p == &g_subs[side][k]) return k;
    return -1;
}
static uint32_t subscriber_id(int side, const void* p) {
    if (!p) return 0xFFFFFFFFu;
    const int k = sub_of(side, p);
    return k >= 0 ? 0x5B000000u + k : (uint32_t)(uintptr_t)p;  // otherwise the heap's fill, the same on both
}

struct Message { AtEventMessage head; uint32_t id; };

static void o_subscribe(AtEvent* e, void* s, int h, unsigned mask);
static void r_subscribe(AtEvent* e, void* s, int h, unsigned mask);
static void o_unsubscribe(AtEvent* e, void* s, int h, unsigned mask);
static void r_unsubscribe(AtEvent* e, void* s, int h, unsigned mask);

// What each handler does besides logging: 1 unsubscribes itself, 2 subscribes subscriber 5 with handler 0 to the same event.
static void handled(int side, int handler, void* subscriber, AtEventMessage* m) {
    note('H', (uint32_t)handler);
    note(subscriber_id(side, subscriber), ((Message*)m)->id);
    note('m', m->m_mask);
    if (handler == 1) {
        if (side == 0) o_unsubscribe(m->m_event, subscriber, 1, 0);
        else r_unsubscribe(m->m_event, subscriber, 1, 0);
    } else if (handler == 2) {
        if (side == 0) o_subscribe(m->m_event, &g_subs[0][5], 0, 0xFFFFFFFFu);
        else r_subscribe(m->m_event, &g_subs[1][5], 0, 0xFFFFFFFFu);
    }
}
template <int H> static void __fastcall o_handler(void* subscriber, void*, AtEventMessage* m) { handled(0, H, subscriber, m); }
template <int H> static void r_handler(void* subscriber, AtEventMessage* m) { handled(1, H, subscriber, m); }
static void* const g_o_handlers[kHandlers] = {(void*)o_handler<0>, (void*)o_handler<1>, (void*)o_handler<2>, (void*)o_handler<3>};
static AtEventHandler const g_r_handlers[kHandlers] = {r_handler<0>, r_handler<1>, r_handler<2>, r_handler<3>};
static uint32_t handler_id(int side, const void* p) {
    for (int k = 0; k < kHandlers; k++) if (p == (side ? (const void*)g_r_handlers[k] : g_o_handlers[k])) return 0x4A000000u + k;
    return p ? (uint32_t)(uintptr_t)p : 0xFFFFFFFFu;
}

static void o_subscribe(AtEvent* e, void* s, int h, unsigned mask) { at<FnSub>(0x000DF5E0)(e, nullptr, s, h < 0 ? nullptr : g_o_handlers[h], mask); }
static void r_subscribe(AtEvent* e, void* s, int h, unsigned mask) { e->Subscribe(s, h < 0 ? nullptr : g_r_handlers[h], mask); }
static void o_unsubscribe(AtEvent* e, void* s, int h, unsigned mask) { at<FnSub>(0x000DF650)(e, nullptr, s, h < 0 ? nullptr : g_o_handlers[h], mask); }
static void r_unsubscribe(AtEvent* e, void* s, int h, unsigned mask) { e->Unsubscribe(s, h < 0 ? nullptr : g_r_handlers[h], mask); }

// ---- the database on each side --------------------------------------------------------------------------------------------------
static Db* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(Db));
    std::memset(o, fill, sizeof(Db));
    at<Fn0>(0x000C9660)(o, nullptr);
    eq("the original's vftable", *(uint32_t*)o, 0x00286688);
    return (Db*)o;
}
static Db* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(Db));
    std::memset(r, fill, sizeof(Db));
    return new (r) Db;
}

static uint32_t event_id(const Db* d, const void* p) {
    if (!p) return 0xFFFFFFFFu;
    if (d->m_list.m_entries) {
        const AtEvent* first = d->m_list.m_entries;
        const AtEvent* e = (const AtEvent*)p;
        if (e >= first && e < first + d->m_list.m_count + 8) return 0xE0000000u + (uint32_t)(e - first);
    }
    return 0xFFFFFF00u;
}
static uint32_t subscription_id(const Db* d, const void* p) {
    if (!p) return 0xFFFFFFFFu;
    if (d->m_subscriptions) {
        const AtEventSubscription* first = d->m_subscriptions;
        const AtEventSubscription* s = (const AtEventSubscription*)p;
        if (s >= first && s < first + d->m_subscriptionCount) return 0x5C000000u + (uint32_t)(s - first);
    }
    return (uint32_t)(uintptr_t)p;  // the fill, the same on both
}
static int group_id(int side, const GmNameMapGroup* g) {
    if (!g) return 0;
    if (g == &g_owner[side]->m_databases) return 1;
    return 2;
}

static void same_db(const char* what, const Db* o, const Db* r) {
    g_recording = false;
    // The name map.
    eq("map size", o->m_size, r->m_size);
    eq("map entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's event", event_id(o, x.m_value), event_id(r, y.m_value));
            }
        }
    }
    eq("map next", o->m_next != nullptr, r->m_next != nullptr);
    eq("map group", group_id(0, o->m_group), group_id(1, r->m_group));
    if (o->m_group && r->m_group) {
        eq("group's name", std::strncmp(o->m_group->m_name, r->m_group->m_name, 8), 0);
        eq("group's first is this", o->m_group->m_first == (const GmNameMap*)o, r->m_group->m_first == (const GmNameMap*)r);
    }
    // The owner: the plain group, and the name spaces in use.
    eq("plain group's first", o->m_group == nullptr && g_owner[0]->m_databases.m_first != nullptr, r->m_group == nullptr && g_owner[1]->m_databases.m_first != nullptr);
    const GmNameSpaceDb& so = g_owner[0]->m_databaseSpaces;
    const GmNameSpaceDb& sr = g_owner[1]->m_databaseSpaces;
    eq("spaces size", so.m_size, sr.m_size);
    if (so.m_size == sr.m_size && so.m_entries && sr.m_entries) {
        for (unsigned i = 0; i < so.m_size; i++) {
            eq("space used", so.m_entries[i].m_value != nullptr, sr.m_entries[i].m_value != nullptr);
            if (so.m_entries[i].m_value && sr.m_entries[i].m_value) eq("space name", std::strncmp(so.m_entries[i].m_name, sr.m_entries[i].m_name, 8), 0);
        }
    }
    // The events.
    eq("list count", o->m_list.m_count, r->m_list.m_count);
    eq("list array", o->m_list.m_entries != nullptr, r->m_list.m_entries != nullptr);
    if (o->m_list.m_entries && r->m_list.m_entries) {
        eq("array's count word", ((const uint32_t*)o->m_list.m_entries)[-1], ((const uint32_t*)r->m_list.m_entries)[-1]);
        const unsigned n = std::min(((const uint32_t*)o->m_list.m_entries)[-1], ((const uint32_t*)r->m_list.m_entries)[-1]);
        for (unsigned i = 0; i < n; i++) {
            const AtEvent& x = o->m_list.m_entries[i];
            const AtEvent& y = r->m_list.m_entries[i];
            eq("event's class", *(const uint32_t*)&x == 0x00287FD8u, *(void* const*)&y == *(void* const*)&r->m_list.m_entries[0]);
            eq("event's first", subscription_id(o, x.m_subscriptions), subscription_id(r, y.m_subscriptions));
            eq("event's flags", x.m_flags, y.m_flags);
            eq("event's database", x.m_database == o, y.m_database == r);
        }
    }
    // The subscriptions.
    eq("block", o->m_subscriptions != nullptr, r->m_subscriptions != nullptr);
    eq("block count", o->m_subscriptionCount, r->m_subscriptionCount);
    if (o->m_subscriptions && r->m_subscriptions && o->m_subscriptionCount == r->m_subscriptionCount) {
        for (unsigned i = 0; i < o->m_subscriptionCount && i < 4096; i++) {
            const AtEventSubscription& x = o->m_subscriptions[i];
            const AtEventSubscription& y = r->m_subscriptions[i];
            eq("subscription's next", subscription_id(o, x.m_next), subscription_id(r, y.m_next));
            eq("subscription's subscriber", subscriber_id(0, x.m_subscriber), subscriber_id(1, y.m_subscriber));
            eq("subscription's handler", handler_id(0, (const void*)x.m_handler), handler_id(1, (const void*)y.m_handler));
            eq("subscription's mask", x.m_mask, y.m_mask);
        }
    }
    eq("calls", (long long)g_hash[0], (long long)g_hash[1]);
    compare_logs(what);
}

static const char* const kFileName = "DESIGN\\LOCATION\\THING";

template <class F> static bool guarded_clean(F f) {
    const bool crashed = guarded(f);
    g_recording = false;
    g_side = 0;
    _fpreset();
    return crashed;
}

static void load_both(Db* o, Db* r, const char* what) {
    g_reports = 0;
    const bool oc = guarded_clean([&] { On on(0); at<FnLoad>(0x000C9A00)(o, nullptr, g_owner[0], kFileName, 77); });
    g_reports = 0;
    const bool rc = guarded_clean([&] { On on(1); r->Load(g_owner[1], kFileName, 77); });
    eq("crashed", oc, rc);
    same_db(what, o, r);
}

static void clear_both(Db* o, Db* r) {
    const bool oc = guarded_clean([&] { On on(0); at<Fn0>(0x000C96D0)(o, nullptr); });
    const bool rc = guarded_clean([&] { On on(1); r->Clear(); });
    eq("clear crashed", oc, rc);
    same_db("clear", o, r);
}

// ---- subscribing and publishing, by a fixed sequence --------------------------------------------------------------------------------
static uint32_t g_rand;
static uint32_t next_rand() { g_rand = g_rand * 1103515245u + 12345u; return g_rand >> 8; }
static const unsigned kMasks[] = {0xFFFFFFFFu, 1, 2, 3, 0x10, 0};
static unsigned g_messages;

static void publish_both(Db* o, Db* r, unsigned on, unsigned named, unsigned mask) {
    Message mo = {{&o->m_list.m_entries[named], mask}, ++g_messages};
    Message mr = {{&r->m_list.m_entries[named], mask}, g_messages};
    g_db[0] = o;
    g_db[1] = r;
    const bool oc = guarded_clean([&] { On on0(0); at<Fn1>(0x000DF6B0)(&o->m_list.m_entries[on], nullptr, (uint32_t)(uintptr_t)&mo); });
    const bool rc = guarded_clean([&] { On on1(1); r->m_list.m_entries[on].Publish(&mr.head); });
    eq("publish crashed", oc, rc);
}

static void one_op(Db* o, Db* r, unsigned count, int kind) {
    const unsigned e = next_rand() % count;
    const int s = (int)(next_rand() % kSubscribers);
    const int h = (int)(next_rand() % kHandlers);
    const unsigned mask = kMasks[next_rand() % 6];
    char what[64];
    switch (kind) {
    case 0:
        std::sprintf(what, "subscribe e%u s%d h%d m%X", e, s, h, mask);
        { On on(0); o_subscribe(&o->m_list.m_entries[e], &g_subs[0][s], h, mask); }
        { On on(1); r_subscribe(&r->m_list.m_entries[e], &g_subs[1][s], h, mask); }
        break;
    case 1: {
        const int hh = next_rand() % 3 == 0 ? -1 : h;
        const unsigned mm = next_rand() % 3 == 0 ? 0 : mask;
        std::sprintf(what, "unsubscribe e%u s%d h%d m%X", e, s, hh, mm);
        { On on(0); o_unsubscribe(&o->m_list.m_entries[e], &g_subs[0][s], hh, mm); }
        { On on(1); r_unsubscribe(&r->m_list.m_entries[e], &g_subs[1][s], hh, mm); }
        break;
    }
    case 2: {
        const unsigned named = next_rand() % 5 == 0 ? next_rand() % count : e;
        std::sprintf(what, "publish e%u named e%u m%X", e, named, mask);
        publish_both(o, r, e, named, mask);
        break;
    }
    case 3: {
        static const unsigned kFlags[] = {0, 2, 3, 1, 0xFFFFFFFD};
        const unsigned f = kFlags[next_rand() % 5];
        std::sprintf(what, "flags e%u %X", e, f);
        o->m_list.m_entries[e].m_flags = r->m_list.m_entries[e].m_flags = f;
        break;
    }
    default: {
        std::sprintf(what, "find free");
        void* a;
        void* b;
        { On on(0); a = at<FnFind>(0x000C9720)(o, nullptr); }
        { On on(1); b = r->FindFreeSubscription(); }
        eq("free found", subscription_id(o, a), subscription_id(r, b));
        break;
    }
    }
    same_db(what, o, r);
}

static void run_ops(Db* o, Db* r, unsigned ops, uint32_t seed) {
    if (!o->m_list.m_entries || !r->m_list.m_entries || o->m_list.m_count == 0 || o->m_list.m_count != r->m_list.m_count) return;
    if (!o->m_subscriptions || !r->m_subscriptions) return;
    g_rand = seed;
    static const int kKinds[] = {0, 0, 0, 0, 1, 1, 2, 2, 2, 3, 4};
    for (unsigned i = 0; i < ops; i++) one_op(o, r, o->m_list.m_count, kKinds[next_rand() % 11]);
}

static unsigned g_files, g_events, g_with_space, g_subscriptions;
static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static bool g_trace;
static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, unsigned ops, bool stats = false) {
    g_context = context;
    if (g_trace) std::printf("case %s\n", context.c_str());
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    g_hash[0] = g_hash[1] = 0;
    make_owners();
    Db* o = make_o(fill);
    Db* r = make_r(fill);
    eq("constructed: map", o->m_entries == nullptr && o->m_size == 0 && o->m_group == nullptr, r->m_entries == nullptr && r->m_size == 0 && r->m_group == nullptr);
    eq("constructed: list", (long long)(uintptr_t)o->m_list.m_entries + o->m_list.m_count, (long long)(uintptr_t)r->m_list.m_entries + r->m_list.m_count);
    eq("constructed: block", (long long)(uintptr_t)o->m_subscriptions + o->m_subscriptionCount, (long long)(uintptr_t)r->m_subscriptions + r->m_subscriptionCount);
    load_both(o, r, "load");
    if (stats) {
        g_events += r->m_list.m_count;
        g_with_space += r->m_group != nullptr && r->m_group != &g_owner[1]->m_databases;
        g_subscriptions += r->m_subscriptionCount;
    }
    load_both(o, r, "load again");
    clear_both(o, r);
    load_both(o, r, "load after clear");
    g_context = context + " ops";
    run_ops(o, r, ops, (uint32_t)data.size() * 7919u + fill);
    g_context = context + " destroy";
    const bool oc = guarded([&] { On on(0); at<FnDel>(0x000C5720)(o, nullptr, 0); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); r->~Db(); });
    g_recording = false;
    g_side = 0;
    eq("destroy crashed", oc, rc);
    eq("destroyed: plain group empty", g_owner[0]->m_databases.m_first == nullptr, g_owner[1]->m_databases.m_first == nullptr);
    compare_logs("destroy");
    eq("calls", (long long)g_hash[0], (long long)g_hash[1]);
    std::free(o);
    std::free(r);
}

// ---- written text ---------------------------------------------------------------------------------------------------------------
#define SN3 "ShortName[3]\n{\n \"Start\"\n \"Mid\"\n \"End\"\n}\n"
static const char* const kTexts[] = {
    // Each word alone, and none.
    "", "// a comment\n", "NameSpace \"Level\"\n", "NumSubscribers 5\n", "NumSubscribers 0\n", "ShortName[0]\n{\n}\n", SN3,
    "NameSpace \"Level\"\n" SN3, "NameSpace \"Level\"\nNumSubscribers 7\n" SN3, "NumSubscribers 7\n" SN3, SN3 "NumSubscribers 7\n",
    // Order and repetition.
    SN3 "NameSpace \"Late\"\n", "NameSpace \"A\"\nNameSpace \"B\"\n" SN3, "NumSubscribers 3\nNumSubscribers 9\n" SN3, "NameSpace \"A\"\n" SN3 SN3,
    "NameSpace \"A\"\n" SN3 "NameSpace \"B\"\nShortName[1]\n{\n \"x\"\n}\n", "ShortName[1]\n{\n \"x\"\n}\nShortName[0]\n{\n}\n",
    // Names.
    "NameSpace \"LongerThanEight\"\n" SN3, "NameSpace \"Exactly8\"\n" SN3, "NameSpace \"\"\n" SN3, "ShortName[2]\n{\n \"LongerThanEight\"\n \"Exactly8\"\n}\n",
    "ShortName[3]\n{\n \"a\"\n \"a\"\n \"b\"\n}\n",
    // Not an empty short name: the name map's hash reads its second byte, which is what the stack held (GmNameMap::HashOf).
    // Counts.
    "ShortName[12]\n{\n \"a\" \"b\" \"c\" \"d\" \"e\" \"f\" \"g\" \"h\" \"i\" \"j\" \"k\" \"l\"\n}\n", "NumSubscribers 1\nShortName[2]\n{\n \"a\" \"b\"\n}\n",
    "ShortName[2]\n{\n \"a\"\n}\n", "ShortName[1]\n{\n \"a\"\n \"b\"\n}\n", "ShortName[2]\n{\n \"a\"\n \"b\"\n", "ShortName[1]\n{\n \"a\"\n",
    // Syntax.
    "ShortName\n{\n \"a\"\n}\n", "ShortName 1]\n{\n \"a\"\n}\n", "ShortName[1\n{\n \"a\"\n}\n", "ShortName[1]\n \"a\"\n}\n", "ShortName[\"a\"]\n{\n}\n", "ShortName[1.5]\n{\n \"a\"\n}\n",
    "NumSubscribers\n" SN3, "NumSubscribers \"x\"\n" SN3, "NumSubscribers 2.5\n" SN3, "NameSpace\n",
    // Tokens the reader passes by.
    "5\n" SN3, "1.5\n" SN3, "\"text\"\n" SN3, "{\n}\n" SN3, "[\n]\n" SN3, "Nonsense\n" SN3, "}\n",
};
// A name that is not a string: past the report, the name is what the stack held, so a load that goes on cannot be compared. These stop at the report, as the game does.
static const char* const kTextsToTheReport[] = {
    "NameSpace 5\n" SN3, "ShortName[1]\n{\n 5\n}\n", "ShortName[2]\n{\n \"a\"\n}\n", "ShortName[1]\n{\n}\n",
};

static void run_written() {
    g_report_limit = 0;
    for (const char* t : kTextsToTheReport) run_text(bytes(t), std::string("to the report [") + t + "]", 0xCD, 40);
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const int limit : {40, 0}) {
            g_report_limit = limit;
            for (const char* t : kTexts) run_text(bytes(t), std::string("text [") + t + "] limit " + std::to_string(limit), fill, 60);
        }
    }
    g_report_limit = 40;
}

// ---- the publishing cases, written out --------------------------------------------------------------------------------------------
struct Pair {
    Db* o;
    Db* r;
    explicit Pair(const char* text) {
        static std::vector<unsigned char> data;
        data = bytes(text);
        g_fs->files[0].data = g_fs->files[1].data = data.data();
        g_fs->files[0].size = g_fs->files[1].size = data.size();
        g_fill = 0xCD;
        make_owners();
        o = make_o(0xCD);
        r = make_r(0xCD);
        load_both(o, r, "pair load");
    }
    ~Pair() {
        { On on(0); at<FnDel>(0x000C5720)(o, nullptr, 0); }
        { On on(1); r->~Db(); }
        compare_logs("pair destroy");
        std::free(o);
        std::free(r);
    }
    void sub(unsigned e, int s, int h, unsigned mask) {
        { On on(0); o_subscribe(&o->m_list.m_entries[e], &g_subs[0][s], h, mask); }
        { On on(1); r_subscribe(&r->m_list.m_entries[e], &g_subs[1][s], h, mask); }
        same_db("sub", o, r);
    }
    void unsub(unsigned e, int s, int h, unsigned mask) {
        { On on(0); o_unsubscribe(&o->m_list.m_entries[e], &g_subs[0][s], h, mask); }
        { On on(1); r_unsubscribe(&r->m_list.m_entries[e], &g_subs[1][s], h, mask); }
        same_db("unsub", o, r);
    }
    void pub(unsigned e, unsigned named, unsigned mask) {
        publish_both(o, r, e, named, mask);
        same_db("pub", o, r);
    }
};

static void run_publishing() {
    g_report_limit = 40;
    {
        g_context = "publish: order, masks, a disabled event, another event named";
        Pair p("NumSubscribers 8\nShortName[2]\n{\n \"a\" \"b\"\n}\n");
        p.sub(0, 0, 0, 1);
        p.sub(0, 1, 3, 2);
        p.sub(0, 2, 0, 3);
        p.sub(1, 3, 3, 0xFFFFFFFFu);
        for (const unsigned m : {1u, 2u, 3u, 4u, 0u, 0xFFFFFFFFu}) p.pub(0, 0, m);
        p.o->m_list.m_entries[0].m_flags = p.r->m_list.m_entries[0].m_flags = 0;
        p.pub(0, 0, 0xFFFFFFFFu);
        p.pub(1, 0, 0xFFFFFFFFu);  // b's list, a's flags: nothing
        p.pub(0, 1, 0xFFFFFFFFu);  // a's list, b's flags: a's subscribers are called
        p.o->m_list.m_entries[0].m_flags = p.r->m_list.m_entries[0].m_flags = 0xFFFFFFFDu;
        p.pub(0, 0, 0xFFFFFFFFu);
        p.o->m_list.m_entries[0].m_flags = p.r->m_list.m_entries[0].m_flags = 2;
        // The same subscriber again: its handler and mask replaced, nothing added.
        p.sub(0, 1, 0, 1);
        p.pub(0, 0, 1);
        // Unsubscribe by each match.
        p.unsub(0, 1, 3, 0);   // wrong handler
        p.unsub(0, 1, 0, 2);   // wrong mask
        p.unsub(0, 4, -1, 0);  // not there
        p.unsub(0, 1, 0, 1);   // the middle
        p.unsub(0, 0, -1, 0);  // the first
        p.unsub(0, 2, -1, 3);  // the last and only
        p.unsub(0, 2, -1, 0);  // an empty list
        p.pub(0, 0, 0xFFFFFFFFu);
    }
    {
        g_context = "publish: handlers that unsubscribe themselves and subscribe others";
        Pair p("NumSubscribers 6\nShortName[1]\n{\n \"a\"\n}\n");
        p.sub(0, 0, 1, 0xFFFFFFFFu);
        p.sub(0, 1, 2, 0xFFFFFFFFu);
        p.sub(0, 2, 1, 0xFFFFFFFFu);
        p.pub(0, 0, 1);
        p.pub(0, 0, 1);
        p.pub(0, 0, 1);
    }
    {
        g_context = "publish: a full block";
        Pair p("NumSubscribers 2\nShortName[2]\n{\n \"a\" \"b\"\n}\n");
        p.sub(0, 0, 0, 1);
        p.sub(1, 1, 0, 1);
        p.sub(0, 2, 0, 1);  // none free
        p.sub(0, 0, 3, 2);  // none free, already subscribed: still nothing
        p.unsub(1, 1, -1, 0);
        p.sub(0, 2, 0, 1);  // the freed one
        p.pub(0, 0, 1);
    }
    {
        g_context = "publish: a subscriber of null";
        Pair p("NumSubscribers 4\nShortName[1]\n{\n \"a\"\n}\n");
        { On on(0); at<FnSub>(0x000DF5E0)(&p.o->m_list.m_entries[0], nullptr, nullptr, g_o_handlers[0], 1); }
        { On on(1); p.r->m_list.m_entries[0].Subscribe(nullptr, g_r_handlers[0], 1); }
        same_db("null subscriber", p.o, p.r);
        p.sub(0, 1, 0, 1);
        p.pub(0, 0, 1);
    }
    {
        // An event made alone: in a file every event is enabled after it is made, so only this shows what the constructor leaves.
        g_context = "an event made alone";
        unsigned char eo[sizeof(AtEvent)], er[sizeof(AtEvent)];
        std::memset(eo, 0xCD, sizeof eo);
        std::memset(er, 0xCD, sizeof er);
        { On on(0); at<Fn0>(0x000DF5C0)(eo, nullptr); }
        { On on(1); new (er) AtEvent; }
        eq("made: vftable", *(uint32_t*)eo == 0x00287FD8u, true);
        eq("made: first", (long long)(uintptr_t)((AtEvent*)eo)->m_subscriptions, (long long)(uintptr_t)((AtEvent*)er)->m_subscriptions);
        eq("made: flags", ((AtEvent*)eo)->m_flags, ((AtEvent*)er)->m_flags);
        eq("made: database", (long long)(uintptr_t)((AtEvent*)eo)->m_database, (long long)(uintptr_t)((AtEvent*)er)->m_database);
        compare_logs("an event made alone");
    }
    {
        // An event destroyed alone, its subscriptions still in the block: in a database the block is freed right after, so only this shows what the destructor does to them.
        g_context = "an event destroyed alone";
        Pair p("NumSubscribers 6\nShortName[2]\n{\n \"a\" \"b\"\n}\n");
        p.sub(0, 0, 0, 1);
        p.sub(0, 1, 3, 2);
        p.sub(0, 2, 0, 3);
        p.sub(1, 3, 0, 1);
        { On on(0); at<Fn0>(0x000DF700)(&p.o->m_list.m_entries[0], nullptr); }
        { On on(1); p.r->m_list.m_entries[0].AtEvent::~AtEvent(); }
        same_db("an event destroyed alone", p.o, p.r);
    }
    {
        g_context = "destroying events with subscriptions";
        Pair p("NumSubscribers 5\nShortName[2]\n{\n \"a\" \"b\"\n}\n");
        p.sub(0, 0, 0, 1);
        p.sub(0, 1, 0, 1);
        p.sub(1, 2, 0, 1);
        clear_both(p.o, p.r);
    }
}

// ---- pieces alone -----------------------------------------------------------------------------------------------------------------
class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};
class DirectTokenizer : public AtIoTextTokenizer {
public:
    void Error(int code) override { if (g_recording) g_log[g_side].add('E', code); count_report(); }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const char* t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void set_heaps(void* list, void* subs) {
    *(void**)(uintptr_t)0x003ADF98 = list;
    *(void**)(uintptr_t)0x003ADF9C = subs;
    AtEventList::s_heap = list;
    AtEventDatabase::s_heap = subs;
}

static void run_pieces() {
    g_context = "keywords";
    const AtIoKeyword* table = (const AtIoKeyword*)(uintptr_t)0x002B6848;
    for (int i = 0; i < AtEventDatabase::kKeywordCount; i++) {
        eq("keyword's id", table[i].m_id, AtEventDatabase::s_keywords[i].m_id);
        eqs("keyword's word", table[i].m_name, AtEventDatabase::s_keywords[i].m_name);
    }

    g_context = "heap setters";
    int a, b;
    at<FnHeap>(0x000C9460)(&a);
    AtEventList::SetHeap(&a);
    at<FnHeap>(0x000C9650)(&b);
    AtEventDatabase::SetHeap(&b);
    eq("list heap", *(void**)(uintptr_t)0x003ADF98 == &a, AtEventList::s_heap == &a);
    eq("subscription heap", *(void**)(uintptr_t)0x003ADF9C == &b, AtEventDatabase::s_heap == &b);
    set_heaps(&g_list_heap, &g_sub_heap);

    // Allocate alone: with a heap, with none, with no memory.
    for (int mode = 0; mode < 3; mode++) {
        for (const unsigned count : {0u, 1u, 3u}) {
            g_context = "allocate alone, mode " + std::to_string(mode) + " count " + std::to_string(count); if (g_trace) std::printf("%s\n", g_context.c_str());
            g_fill = 0xCD;
            AtEventList* lo = (AtEventList*)std::malloc(sizeof(AtEventList));
            AtEventList* lr = (AtEventList*)std::malloc(sizeof(AtEventList));
            std::memset(lo, 0xCD, sizeof(AtEventList));
            std::memset(lr, 0xCD, sizeof(AtEventList));
            at<Fn0>(0x000C94B0)(lo, nullptr);
            new (lr) AtEventList;
            eq("list constructed", (long long)(uintptr_t)lo->m_entries + lo->m_count, (long long)(uintptr_t)lr->m_entries + lr->m_count);
            eq("list constructed: count", lo->m_count, lr->m_count);
            if (mode == 1) set_heaps(nullptr, nullptr);
            g_fail_alloc = mode == 2;
            const bool oc = guarded_clean([&] { On on(0); at<Fn1>(0x000C9520)(lo, nullptr, count); });
            const bool rc = guarded_clean([&] { On on(1); lr->Allocate(count); });
            g_fail_alloc = false;
            set_heaps(&g_list_heap, &g_sub_heap);
            eq("allocate crashed", oc, rc);
            eq("allocated count", lo->m_count, lr->m_count);
            eq("allocated array", lo->m_entries != nullptr, lr->m_entries != nullptr);
            if (lo->m_entries && lr->m_entries) {
                eq("count word", ((uint32_t*)lo->m_entries)[-1], ((uint32_t*)lr->m_entries)[-1]);
                for (unsigned i = 0; i < count; i++) {
                    eq("flags", lo->m_entries[i].m_flags, lr->m_entries[i].m_flags);
                    eq("first", (long long)(uintptr_t)lo->m_entries[i].m_subscriptions, (long long)(uintptr_t)lr->m_entries[i].m_subscriptions);
                    eq("database", (long long)(uintptr_t)lo->m_entries[i].m_database, (long long)(uintptr_t)lr->m_entries[i].m_database);
                    eq("class", *(uint32_t*)&lo->m_entries[i] == 0x00287FD8u, true);
                }
            }
            compare_logs("allocate alone");
            { On on(0); at<Fn0>(0x000C9470)(lo, nullptr); }
            { On on(1); lr->Free(); }
            compare_logs("allocate alone: free");
            eq("freed", (long long)(uintptr_t)lo->m_entries + lo->m_count, (long long)(uintptr_t)lr->m_entries + lr->m_count);
            // Made again, then destroyed through the list's deleting destructor without freeing the list, and through its complete destructor.
            if (mode == 0) {
                { On on(0); at<Fn1>(0x000C9520)(lo, nullptr, count); }
                { On on(1); lr->Allocate(count); }
                { On on(0); at<FnDel>(0x000C9600)(lo, nullptr, 0); }
                { On on(1); lr->~AtEventList(); }
                compare_logs("allocate alone: destroy");
                eq("destroyed", (long long)(uintptr_t)lo->m_entries + lo->m_count, (long long)(uintptr_t)lr->m_entries + lr->m_count);
            }
            std::free(lo);
            std::free(lr);
        }
    }

    // The reader alone, on a tokenizer in memory: no heap, and no memory, for each word that takes it.
    static const char* const texts[] = {"NumSubscribers 4\n", "", "NameSpace \"A\"\nShortName[2]\n{\n \"a\" \"b\"\n}\n", "ShortName[0]\n{\n}\nNumSubscribers 2\n"};
    for (int mode = 0; mode < 4; mode++) {
        for (const char* t : texts) {
            g_context = std::string("reader alone, mode ") + std::to_string(mode) + " [" + t + "]"; if (g_trace) std::printf("%s\n", g_context.c_str());
            g_fill = 0xCD;
            g_hash[0] = g_hash[1] = 0;
            make_owners();
            Db* o = make_o(0xCD);
            Db* r = make_r(0xCD);
            Direct da(t), db(t);
            if (mode == 1) set_heaps(nullptr, nullptr);
            if (mode == 2) set_heaps(&g_list_heap, nullptr);
            g_fail_alloc = mode == 3;
            g_reports = 0;
            const bool oc = guarded_clean([&] { On on(0); at<FnRead>(0x000C9750)(o, nullptr, g_owner[0], &da.tokenizer); });
            g_reports = 0;
            const bool rc = guarded_clean([&] { On on(1); r->Read(g_owner[1], &db.tokenizer); });
            g_fail_alloc = false;
            set_heaps(&g_list_heap, &g_sub_heap);
            eq("reader crashed", oc, rc);
            eq("position after", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
            same_db("reader alone", o, r);
            // What a failed allocation left is not for destroying.
            std::free(o);
            std::free(r);
        }
    }
}

static void run_no_file() {
    g_context = "no file";
    static const std::string none;
    const auto data = bytes(none);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = 0;
    for (const int status : {1, 2, 5}) {
        g_fs->open_status = status;
        make_owners();
        Db* o = make_o(0xCD);
        Db* r = make_r(0xCD);
        const int before = g_crashes;
        load_both(o, r, "no file");
        eq("this crashed, on both", g_crashes - before, 2);
        g_fs->open_status = 0;
        std::free(o);
        std::free(r);
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else if (!std::strcmp(argv[i], "--trace")) g_trace = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_aee <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal2);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6344u}) *(void**)(uintptr_t)va = &g_heap;
    AtIoTokenFile::s_heap = &g_heap;
    AtIoStream::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    set_heaps(&g_list_heap, &g_sub_heap);
    for (int side = 0; side < 2; side++) {
        std::strcpy(g_fs->files[side].m_path, "DESIGN\\LOCATION\\THING.AEE");
        for (int k = 0; k < kSubscribers; k++) g_subs[side][k].id = k;
    }

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    if (g_trace) std::printf("pieces\n");
    run_pieces();
    if (g_trace) std::printf("no file\n");
    run_no_file();
    if (g_trace) std::printf("publishing\n");
    run_publishing();
    run_written();

    g_events = g_with_space = g_subscriptions = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AEE\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_text(data, line, 0xCD, 200, true);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u events, %u in a name space, %u subscriptions; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_events, g_with_space,
                g_subscriptions, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
