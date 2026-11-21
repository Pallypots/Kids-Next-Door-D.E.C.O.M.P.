// Events by name, .AEE files: a name space, how many subscriptions the file's events may hold between them, and the short names of its events. Each event keeps a list of
// subscriptions, a subscriber and its handler with a mask; publishing a message calls the handler of every subscription whose mask meets the message's. What a handler does is the
// subscriber's: game code, scripts, sound, none of it here. All class and member names are inferred; the words NameSpace, NumSubscribers and ShortName are the original's.
// Original address clusters 0x000C5720-0x000C5773, 0x000C9400-0x000C9659, 0x000C9660-0x000C9A99, 0x000DF5C0-0x000DF6EF, 0x000DF700-0x000DF71D. The event list's and the database's
// constructors, destructors and Clear were recovered by gm/dbmanifest and are kept here, with the class.
#pragma once
#include "at/AtDataRegistry.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

class AtEvent;
class AtEventDatabase;

// What is published: at least these two words; a message's own class goes on past them, and only its handlers read further.
struct AtEventMessage {
    AtEvent* m_event;                          // +0x00 whose flags decide whether anything is called
    unsigned int m_mask;                       // +0x04 met by a subscription's mask
};

// A handler. In the original, the address of a member function of the subscriber's class, called with the subscriber as `this` and the message as its one argument (__thiscall,
// the callee pops it). Here a function given both: C++ cannot store a member function of an unknown class in one word, and a port needs no x86 convention. Behaviour is the same.
typedef void (*AtEventHandler)(void* subscriber, AtEventMessage* message);

// One subscription, 0x10 bytes. The database keeps them all in one block; one is free while its subscriber is null.
struct AtEventSubscription {
    AtEventSubscription* m_next;               // +0x00 in its event's list
    void* m_subscriber;                        // +0x04 null when free
    AtEventHandler m_handler;                  // +0x08
    unsigned int m_mask;                       // +0x0C
};

// One event. Vftable 0x00287FD8, one slot; 0x10 bytes.
class AtEvent {
public:
    enum Flag { kEnabled = 2 };

    AtEvent();                                 // 0x000DF5C0
    virtual ~AtEvent();                        // slot 0; 0x000C9400 deleting, for arrays too; 0x000DF700 complete: frees each subscription it holds, by clearing it

    // 0x000DF5E0. Takes a free subscription from the database. A subscriber already in the list only has its handler and mask replaced, and the one taken goes back. With none free,
    // nothing happens.
    void Subscribe(void* subscriber, AtEventHandler handler, unsigned int mask);
    // 0x000DF650. The first subscription of this subscriber whose handler and mask match those given, a null handler and a mask of 0 matching any. It is freed.
    void Unsubscribe(void* subscriber, AtEventHandler handler, unsigned int mask);
    // 0x000DF6B0. Only if the message's event is enabled: each subscription in this event's list whose mask shares a bit with the message's, in order, unless its subscriber is null.
    // The next subscription is taken before a handler is called.
    void Publish(AtEventMessage* message);

    AtEventSubscription* m_subscriptions;      // +0x04 the first
    unsigned int m_flags;                      // +0x08 0 when made; kEnabled when its list is made
    AtEventDatabase* m_database;               // +0x0C
};

// Vftable 0x00286674, 0x0C bytes: the database's events, one array.
class AtEventList {
public:
    AtEventList();                             // 0x000C94B0
    virtual ~AtEventList();                    // slot 0; 0x000C9600 deleting; 0x000C94D0 complete
    void Free();                               // 0x000C9470 the events through the first one's vector deleting destructor
    void Reset();                              // 0x000E7880 folded with others: forgets both words
    // 0x000C9520. New events from s_heap, each enabled. What was here before is not freed. A count of 0 still makes an array.
    void Allocate(unsigned int count);

    AtEvent* m_entries;                        // +0x04
    unsigned int m_count;                      // +0x08

    static void SetHeap(void* heap);           // 0x000C9460
    static void* s_heap;                       // 0x003ADF98
};

// .AEE. Vftable 0x00286688, 0x28 bytes. A name map of its own events, registered with the owner under the file's name space.
class AtEventDatabase : public GmNameMap {
public:
    enum Word { kNameSpace = 0x39, kShortName = 0x3A, kNumSubscribers = 0x3B };
    enum { kKeywordCount = 3, kDefaultSubscriptions = 100 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B6848

    AtEventDatabase();                         // 0x000C9660
    ~AtEventDatabase() override;               // slot 0; 0x000C5720 deleting, for arrays too; 0x000C9960 complete
    void Clear() override;                     // slot 1, 0x000C96D0 nothing at all unless the list holds an array

    void Load(AtDataRegistryOwner* owner, const char* name, int unused);  // 0x000C9A00 name + ".AEE"
    // 0x000C9750. Clears first only if the list holds an array. Each word may come in any order and any number of times: a repeated NumSubscribers loses the earlier block, a
    // repeated ShortName the earlier events. With no NumSubscribers, 100 subscriptions. Any other token is passed by.
    void Read(AtDataRegistryOwner* owner, AtIoTokenizer* tokenizer);
    AtEventSubscription* FindFreeSubscription();  // 0x000C9720 the first whose subscriber is null, or null

    AtEventList m_list;                        // +0x14
    AtEventSubscription* m_subscriptions;      // +0x20 a block, not an array: freed by Clear
    unsigned int m_subscriptionCount;          // +0x24

    static void SetHeap(void* heap);           // 0x000C9650
    static void* s_heap;                       // 0x003ADF9C what the subscriptions come from
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtEventSubscription) == 0x10, "AtEventSubscription");
static_assert(offsetof(AtEvent, m_database) == 0x0C, "AtEvent::m_database");
static_assert(sizeof(AtEvent) == 0x10, "AtEvent");
static_assert(sizeof(AtEventList) == 0x0C, "AtEventList");
static_assert(offsetof(AtEventDatabase, m_list) == 0x14, "AtEventDatabase::m_list");
static_assert(offsetof(AtEventDatabase, m_subscriptions) == 0x20, "AtEventDatabase::m_subscriptions");
static_assert(sizeof(AtEventDatabase) == 0x28, "AtEventDatabase");
#pragma pop_macro("offsetof")
#endif
