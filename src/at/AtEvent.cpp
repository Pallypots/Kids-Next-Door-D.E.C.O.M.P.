// See include/at/AtEvent.h. The .AEE reader, an event's subscriptions and its publishing. Verification status per function: config/functions.d/aee.toml.
#include "at/AtEvent.h"

#include "AtArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtEventList::s_heap;
void* AtEventDatabase::s_heap;

const AtIoKeyword AtEventDatabase::s_keywords[kKeywordCount] = {{kNameSpace, "NameSpace"}, {kShortName, "ShortName"}, {kNumSubscribers, "NumSubscribers"}};

// ---- AtEvent ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtEvent::AtEvent() {
    m_subscriptions = 0;
    m_flags = 0;
}

// The list's head is left as it was.
AtEvent::~AtEvent() {
    AtEventSubscription* subscription = m_subscriptions;
    while (subscription != 0) {
        AtEventSubscription* next = subscription->m_next;
        subscription->m_subscriber = 0;
        subscription->m_next = 0;
        subscription = next;
    }
}

void AtEvent::Subscribe(void* subscriber, AtEventHandler handler, unsigned int mask) {
    AtEventSubscription* taken = m_database->FindFreeSubscription();
    if (taken == 0) {
        return;
    }
    taken->m_mask = mask;
    taken->m_subscriber = subscriber;
    taken->m_handler = handler;
    taken->m_next = 0;
    AtEventSubscription* last = m_subscriptions;
    if (last == 0) {
        m_subscriptions = taken;
        return;
    }
    for (AtEventSubscription* at = last; at != 0; at = at->m_next) {
        last = at;
        if (at->m_subscriber == subscriber) {
            taken->m_subscriber = 0;
            at->m_mask = mask;
            at->m_handler = handler;
            return;
        }
    }
    last->m_next = taken;
}

void AtEvent::Unsubscribe(void* subscriber, AtEventHandler handler, unsigned int mask) {
    AtEventSubscription* before = m_subscriptions;
    for (AtEventSubscription* at = m_subscriptions; at != 0; at = at->m_next) {
        if (at->m_subscriber == subscriber && (handler == 0 || at->m_handler == handler) && (mask == 0 || at->m_mask == mask)) {
            // When it is the first, `before` is itself, and this writes its own link back.
            before->m_next = at->m_next;
            if (at == m_subscriptions) {
                m_subscriptions = at->m_next;
            }
            at->m_subscriber = 0;
            at->m_next = 0;
            return;
        }
        before = at;
    }
}

void AtEvent::Publish(AtEventMessage* message) {
    AtEventSubscription* at = m_subscriptions;
    const unsigned int mask = message->m_mask;
    if ((message->m_event->m_flags & kEnabled) == 0) {
        return;
    }
    while (at != 0) {
        if ((at->m_mask & mask) == 0) {
            at = at->m_next;
            continue;
        }
        void* subscriber = at->m_subscriber;
        AtEventHandler handler = at->m_handler;
        at = at->m_next;
        if (subscriber != 0) {
            handler(subscriber, message);
        }
    }
}

// ---- AtEventList ------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// The events are a real new[] and delete[]: a count in the word ahead of the first, which delete[] goes by through the first event's vector deleting destructor (0x000C9400). The host
// compiler makes that destructor for real only where it also sees the class's new[], and it is this file that holds the class's vftable. A heap here may answer null: this new[] may say so.
struct AtEventNoThrow {};
inline void* operator new[](AtSize size, AtEventNoThrow) noexcept { return operator new(size); }
inline void operator delete[](void* block, AtEventNoThrow) noexcept { operator delete(block); }

namespace {

// The original's delete[] of the events: with a count of 0 the block is freed, there being no event to ask; otherwise the first event's vector deleting destructor (slot 0, with
// flags 3) destroys them all, last first, and frees the block.
void DeleteEvents(AtEvent* first) {
    AtSize* block = (AtSize*)first - 1;
    if (*block != 0) {
        delete[] first;
    } else {
        operator delete(block);
    }
}

}  // namespace

AtEventList::AtEventList() {
    m_entries = 0;
    m_count = 0;
}

AtEventList::~AtEventList() {
    if (m_entries != 0) {
        DeleteEvents(m_entries);
        m_entries = 0;
        m_count = 0;
    }
}

void AtEventList::Free() {
    if (m_entries != 0) {
        DeleteEvents(m_entries);
    }
    m_entries = 0;
    m_count = 0;
}

void AtEventList::Reset() {
    m_entries = 0;
    m_count = 0;
}

void AtEventList::SetHeap(void* heap) {
    s_heap = heap;
}

void AtEventList::Allocate(unsigned int count) {
    m_count = count;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_entries = new (AtEventNoThrow{}) AtEvent[count];
    AtPopHeap();
    if (m_entries == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        m_entries[i].m_flags = AtEvent::kEnabled;
    }
}

// ---- AtEventDatabase --------------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

// From the database's heap, each one free. The size is not checked for overflow; on the Xbox it is count << 4.
AtEventSubscription* NewSubscriptions(unsigned int count) {
    if (AtEventDatabase::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtEventDatabase::s_heap);
    AtEventSubscription* block = (AtEventSubscription*)operator new(count * sizeof(AtEventSubscription));
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        block[i].m_subscriber = 0;
    }
    return block;
}

}  // namespace

AtEventDatabase::AtEventDatabase() {
    m_list.Reset();
    m_subscriptions = 0;
    m_subscriptionCount = 0;
}

AtEventDatabase::~AtEventDatabase() {
    AtEventDatabase::Clear();
}

// Nothing at all, the name map's own Clear included, when the list holds no array: a database read from a file with no ShortName keeps its subscriptions.
void AtEventDatabase::Clear() {
    if (m_list.m_entries == 0) {
        return;
    }
    m_list.Free();
    operator delete(m_subscriptions);
    m_list.Reset();
    m_subscriptions = 0;
    m_subscriptionCount = 0;
    GmNameMap::Clear();
}

void AtEventDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtEventSubscription* AtEventDatabase::FindFreeSubscription() {
    for (unsigned int i = 0; i < m_subscriptionCount; i++) {
        if (m_subscriptions[i].m_subscriber == 0) {
            return &m_subscriptions[i];
        }
    }
    return 0;
}

// Load is in src/at/AtEventLoad.cpp.

void AtEventDatabase::Read(AtDataRegistryOwner* owner, AtIoTokenizer* tokenizer) {
    char space[9];
    space[0] = 0;
    if (m_list.m_entries != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    // The first token is taken as a word even at the end of the text, which then asks for one more.
    int token = tokenizer->Next();
    do {
        switch (token) {
        case kNameSpace: {
            char read[8];
            tokenizer->ExpectName(read);
            AtCopyName9(space, read);
            break;
        }
        case kShortName: {
            tokenizer->ExpectLeftBracket();
            const unsigned int count = (unsigned int)tokenizer->ExpectInt();
            tokenizer->ExpectRightBracket();
            tokenizer->ExpectLeftCurly();
            Init(count);
            owner->AddDatabase(this, space);
            m_list.Allocate(count);
            for (unsigned int i = 0; i < count; i++) {
                char shortName[8];
                tokenizer->ExpectName(shortName);
                Insert(shortName, &m_list.m_entries[i]);
                m_list.m_entries[i].m_database = this;
            }
            tokenizer->ExpectRightCurly();
            break;
        }
        case kNumSubscribers:
            m_subscriptionCount = (unsigned int)tokenizer->ExpectInt();
            m_subscriptions = NewSubscriptions(m_subscriptionCount);
            break;
        }
    } while ((token = tokenizer->Next()) != AtIoTokenizer::kEnd);
    if (m_subscriptions == 0) {
        m_subscriptionCount = kDefaultSubscriptions;
        m_subscriptions = NewSubscriptions(m_subscriptionCount);
    }
}
