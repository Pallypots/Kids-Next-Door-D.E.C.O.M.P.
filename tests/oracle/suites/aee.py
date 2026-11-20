# Mutants of the .AEE reader, an event's subscriptions and publishing, and the event list's and database's life cycle (the last eleven came from the dbmanifest suite with the code).
# Each text occurs exactly once in the source.
# Dropped as equivalent, each surviving the first full run:
# * "delete: count 0 asks the first" (DeleteEvents' own test of a count of 0): the host compiler's delete[] of a class with a virtual destructor reads the cookie and makes the same
#   test itself, as the dbmanifest suite found for DeleteObjects.
# * "load: not closed": AtIoTokenFile's destructor calls Close, and a second Close does nothing, so Load's own Close cannot be told apart from the destructor's.
# * "read: always clear": Clear begins with the same test of the list (0x000C96D8), so calling it unconditionally is the same.
F = "AtEvent.cpp"
L = "AtEventLoad.cpp"

MUTANTS = [
    # The table.
    ("keywords: an id", F, '{kShortName, "ShortName"}', '{kNumSubscribers, "ShortName"}'),
    ("keywords: a word", F, '{kNameSpace, "NameSpace"}', '{kNameSpace, "Namespace"}'),
    ("keywords: the last word", F, '{kNumSubscribers, "NumSubscribers"}', '{kNumSubscribers, "NumSubscriber"}'),
    ("keywords: fewer known", F, "tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);", "tokenizer->SetKeywords(s_keywords, kKeywordCount - 1, 0);"),
    # The event.
    ("event constructor: first", F, "AtEvent::AtEvent() {\n    m_subscriptions = 0;\n", "AtEvent::AtEvent() {\n"),
    ("event constructor: flags", F, "    m_flags = 0;\n}\n", "}\n"),
    ("event destructor: subscriber kept", F, "        subscription->m_subscriber = 0;\n        subscription->m_next = 0;\n        subscription = next;\n",
     "        subscription->m_next = 0;\n        subscription = next;\n"),
    ("event destructor: link kept", F, "        subscription->m_subscriber = 0;\n        subscription->m_next = 0;\n        subscription = next;\n",
     "        subscription->m_subscriber = 0;\n        subscription = next;\n"),
    ("event destructor: first only", F, "        subscription = next;\n", "        subscription = 0;\n"),
    # Subscribe.
    ("subscribe: none free goes on", F, "    if (taken == 0) {\n        return;\n    }\n    taken->m_mask = mask;\n", "    if (taken == 0) {\n        taken = m_database->m_subscriptions;\n    }\n    taken->m_mask = mask;\n"),
    ("subscribe: mask", F, "    taken->m_mask = mask;\n", "    taken->m_mask = 0xFFFFFFFF;\n"),
    ("subscribe: subscriber", F, "    taken->m_subscriber = subscriber;\n", "    taken->m_subscriber = this;\n"),
    ("subscribe: handler", F, "    taken->m_handler = handler;\n", ""),
    ("subscribe: link", F, "    taken->m_next = 0;\n", ""),
    ("subscribe: first", F, "        m_subscriptions = taken;\n        return;\n", "        return;\n"),
    ("subscribe: again: not given back", F, "            taken->m_subscriber = 0;\n", ""),
    ("subscribe: again: mask kept", F, "            at->m_mask = mask;\n", ""),
    ("subscribe: again: handler kept", F, "            at->m_handler = handler;\n            return;\n", "            return;\n"),
    ("subscribe: again: added too", F, "            at->m_handler = handler;\n            return;\n", "            at->m_handler = handler;\n            break;\n"),
    ("subscribe: at the head", F, "    last->m_next = taken;\n", "    taken->m_next = m_subscriptions;\n    m_subscriptions = taken;\n"),
    ("subscribe: after the first", F, "        last = at;\n        if (at->m_subscriber == subscriber) {", "        if (at->m_subscriber == subscriber) {"),
    # Unsubscribe.
    ("unsubscribe: handler always matched", F, "(handler == 0 || at->m_handler == handler)", "(handler == 0 || at->m_handler == at->m_handler)"),
    ("unsubscribe: no handler matches none", F, "(handler == 0 || at->m_handler == handler)", "(at->m_handler == handler)"),
    ("unsubscribe: mask always matched", F, "(mask == 0 || at->m_mask == mask)", "(mask == 0 || at->m_mask != 0)"),
    ("unsubscribe: no mask matches none", F, "(mask == 0 || at->m_mask == mask)", "(at->m_mask == mask)"),
    ("unsubscribe: mask as bits", F, "(mask == 0 || at->m_mask == mask)", "(mask == 0 || (at->m_mask & mask) != 0)"),
    ("unsubscribe: any subscriber", F, "        if (at->m_subscriber == subscriber && (handler", "        if (at->m_subscriber != 0 && (handler"),
    ("unsubscribe: not unlinked", F, "            before->m_next = at->m_next;\n", ""),
    ("unsubscribe: head kept", F, "            if (at == m_subscriptions) {\n                m_subscriptions = at->m_next;\n            }\n", ""),
    ("unsubscribe: not freed", F, "            at->m_subscriber = 0;\n            at->m_next = 0;\n            return;\n", "            at->m_next = 0;\n            return;\n"),
    ("unsubscribe: link kept", F, "            at->m_subscriber = 0;\n            at->m_next = 0;\n            return;\n", "            at->m_subscriber = 0;\n            return;\n"),
    ("unsubscribe: every match", F, "            at->m_next = 0;\n            return;\n        }\n        before = at;\n", "            return;\n        }\n        before = at;\n"),
    ("unsubscribe: before not moved", F, "        before = at;\n    }\n", "    }\n"),
    # Publish.
    ("publish: flags not tested", F, "    if ((message->m_event->m_flags & kEnabled) == 0) {\n        return;\n    }\n", ""),
    ("publish: this event's flags", F, "    if ((message->m_event->m_flags & kEnabled) == 0) {", "    if ((m_flags & kEnabled) == 0) {"),
    ("publish: flag bit", F, "(message->m_event->m_flags & kEnabled)", "(message->m_event->m_flags & 3)"),
    ("publish: mask not tested", F, "        if ((at->m_mask & mask) == 0) {\n            at = at->m_next;\n            continue;\n        }\n", ""),
    ("publish: mask equal", F, "        if ((at->m_mask & mask) == 0) {", "        if (at->m_mask != mask) {"),
    ("publish: next taken after", F, "        at = at->m_next;\n        if (subscriber != 0) {\n            handler(subscriber, message);\n        }\n",
     "        if (subscriber != 0) {\n            handler(subscriber, message);\n        }\n        at = at->m_next;\n"),
    ("publish: null subscriber called", F, "        if (subscriber != 0) {\n            handler(subscriber, message);", "        if (handler != 0) {\n            handler(subscriber, message);"),
    ("publish: first only", F, "    while (at != 0) {\n        if ((at->m_mask", "    for (int once = 0; at != 0 && once == 0; once++) {\n        if ((at->m_mask"),
    # The list.
    ("allocate: count", F, "    m_count = count;\n    if (s_heap == 0) {", "    if (s_heap == 0) {"),
    ("allocate: heap not tested", F, "    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(s_heap);\n", "    AtPushHeap(s_heap);\n"),
    ("allocate: heap", F, "    AtPushHeap(s_heap);\n    m_entries", "    AtPushHeap(AtEventDatabase::s_heap);\n    m_entries"),
    ("allocate: no memory not raised", F, "    if (m_entries == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("allocate: flags", F, "        m_entries[i].m_flags = AtEvent::kEnabled;\n", ""),
    ("allocate: flag value", F, "        m_entries[i].m_flags = AtEvent::kEnabled;\n", "        m_entries[i].m_flags = 3;\n"),
    ("allocate: one short", F, "    for (unsigned int i = 0; i < count; i++) {\n        m_entries[i].m_flags", "    for (unsigned int i = 1; i < count; i++) {\n        m_entries[i].m_flags"),
    ("list destructor: count kept", F, "        DeleteEvents(m_entries);\n        m_entries = 0;\n        m_count = 0;\n", "        DeleteEvents(m_entries);\n        m_entries = 0;\n"),
    ("list destructor: nothing", F, "    if (m_entries != 0) {\n        DeleteEvents(m_entries);\n        m_entries = 0;\n", "    if (m_entries == (AtEvent*)1) {\n        DeleteEvents(m_entries);\n        m_entries = 0;\n"),
    ("list free: count kept", F, "        DeleteEvents(m_entries);\n    }\n    m_entries = 0;\n    m_count = 0;\n", "        DeleteEvents(m_entries);\n    }\n    m_entries = 0;\n"),
    ("list free: nothing", F, "    if (m_entries != 0) {\n        DeleteEvents(m_entries);\n    }\n", ""),
    ("list constructor: count", F, "AtEventList::AtEventList() {\n    m_entries = 0;\n    m_count = 0;\n", "AtEventList::AtEventList() {\n    m_entries = 0;\n"),
    # The subscriptions' block.
    ("block: heap not tested", F, "    if (AtEventDatabase::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("block: size", F, "operator new(count * sizeof(AtEventSubscription))", "operator new(count * 8)"),
    ("block: no memory not raised", F, "    if (block == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("block: not freed", F, "        block[i].m_subscriber = 0;\n", ""),
    ("block: last kept", F, "    for (unsigned int i = 0; i < count; i++) {\n        block[i]", "    for (unsigned int i = 1; i < count; i++) {\n        block[i]"),
    ("find free: the first, used or not", F, "        if (m_subscriptions[i].m_subscriber == 0) {", "        if (m_subscriptions[i].m_subscriber == 0 || i == 0) {"),
    ("find free: past the count", F, "    for (unsigned int i = 0; i < m_subscriptionCount; i++) {\n        if (m_subscriptions", "    for (unsigned int i = 0; i <= m_subscriptionCount; i++) {\n        if (m_subscriptions"),
    # The database's life cycle.
    ("database constructor: count", F, "    m_subscriptions = 0;\n    m_subscriptionCount = 0;\n}\n\nAtEventDatabase::~", "    m_subscriptions = 0;\n}\n\nAtEventDatabase::~"),
    ("database destructor: nothing", F, "    AtEventDatabase::Clear();\n", ""),
    ("clear: no test", F, "    if (m_list.m_entries == 0) {\n        return;\n    }\n    m_list.Free();", "    m_list.Free();"),
    ("clear: block kept", F, "    operator delete(m_subscriptions);\n", ""),
    ("clear: no free", F, "    m_list.Free();\n    operator delete", "    m_list.Reset();\n    operator delete"),
    ("clear: map kept", F, "    GmNameMap::Clear();\n", ""),
    ("clear: count kept", F, "    m_subscriptions = 0;\n    m_subscriptionCount = 0;\n    GmNameMap::Clear();\n", "    m_subscriptions = 0;\n    GmNameMap::Clear();\n"),
    # Load.
    ("load: no report", L, "        AtReportFileError(status, name, \"AEE\");\n", ""),
    ("load: extension", L, 'file.Open(name, "AEE", 2, 0)', 'file.Open(name, "AED", 2, 0)'),
    ("load: flags", L, 'file.Open(name, "AEE", 2, 0)', 'file.Open(name, "AEE", 0, 0)'),
    # Read.
    ("read: no clear", F, "    if (m_list.m_entries != 0) {\n        Clear();\n    }\n", ""),
    ("read: end at once", F, "    } while ((token = tokenizer->Next()) != AtIoTokenizer::kEnd);", "    } while (token != AtIoTokenizer::kEnd && (token = tokenizer->Next()) != AtIoTokenizer::kEnd);"),
    ("read: name space ignored", F, "            AtCopyName9(space, read);\n", ""),
    ("read: no left bracket", F, "            tokenizer->ExpectLeftBracket();\n", ""),
    ("read: no right bracket", F, "            tokenizer->ExpectRightBracket();\n", ""),
    ("read: no left curly", F, "            tokenizer->ExpectLeftCurly();\n", ""),
    ("read: no right curly", F, "            tokenizer->ExpectRightCurly();\n", ""),
    ("read: count one more", F, "            const unsigned int count = (unsigned int)tokenizer->ExpectInt();", "            const unsigned int count = (unsigned int)tokenizer->ExpectInt() + 1;"),
    ("read: map not sized", F, "            Init(count);\n", ""),
    ("read: map sized larger", F, "            Init(count);\n", "            Init(count * 2);\n"),
    ("read: not registered", F, "            owner->AddDatabase(this, space);\n", ""),
    ("read: no name space", F, "            owner->AddDatabase(this, space);\n", "            owner->AddDatabase(this, 0);\n"),
    ("read: list not made", F, "            m_list.Allocate(count);\n", ""),
    ("read: names not inserted", F, "                Insert(shortName, &m_list.m_entries[i]);\n", ""),
    ("read: names backwards", F, "                Insert(shortName, &m_list.m_entries[i]);\n", "                Insert(shortName, &m_list.m_entries[count - 1 - i]);\n"),
    ("read: database not set", F, "                m_list.m_entries[i].m_database = this;\n", ""),
    ("read: subscriptions count", F, "            m_subscriptionCount = (unsigned int)tokenizer->ExpectInt();\n            m_subscriptions = NewSubscriptions(m_subscriptionCount);\n            break;",
     "            m_subscriptionCount = (unsigned int)tokenizer->ExpectInt() + 1;\n            m_subscriptions = NewSubscriptions(m_subscriptionCount);\n            break;"),
    ("read: subscriptions not made", F, "            m_subscriptions = NewSubscriptions(m_subscriptionCount);\n            break;", "            break;"),
    ("read: no default", F, "    if (m_subscriptions == 0) {\n        m_subscriptionCount = kDefaultSubscriptions;\n        m_subscriptions = NewSubscriptions(m_subscriptionCount);\n    }\n", ""),
    ("read: default always", F, "    if (m_subscriptions == 0) {\n        m_subscriptionCount = kDefaultSubscriptions;", "    if (m_subscriptions != (AtEventSubscription*)1) {\n        m_subscriptionCount = kDefaultSubscriptions;"),
    ("read: default count", F, "        m_subscriptionCount = kDefaultSubscriptions;\n", "        m_subscriptionCount = kDefaultSubscriptions - 1;\n"),
]
SUITE = {"target": "oracle_aee", "subsystem": "at/aee", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
