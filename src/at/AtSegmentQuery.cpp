// See include/at/AtSegmentQuery.h.
#include "at/AtSegmentQuery.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

void* AtSegmentHitList::s_heap;
void* AtQueryPool::s_heap;

AtSegmentHitList::AtSegmentHitList() {
    Reset();
}

AtSegmentHitList::~AtSegmentHitList() {
    if ((m_flags & kNotOwned) == 0) {
        if (m_hits != 0) {
            operator delete(m_hits);
        }
        if (m_keys != 0) {
            operator delete(m_keys);
        }
    }
    Reset();
}

void AtSegmentHitList::SetHeap(void* heap) {
    s_heap = heap;
}

void AtSegmentHitList::Reset() {
    m_vectors0[0].x = 0.0f;
    m_vectors0[0].y = 0.0f;
    m_vectors0[0].z = 0.0f;
    m_vectors0[1].x = 0.0f;
    m_vectors0[1].y = 0.0f;
    m_vectors0[1].z = 0.0f;
    m_first0 = &m_vectors0[0];
    m_second0 = &m_vectors0[1];
    m_vectors2[0].x = 0.0f;
    m_vectors2[0].y = 0.0f;
    m_vectors2[0].z = 0.0f;
    m_vectors2[1].x = 0.0f;
    m_vectors2[1].y = 0.0f;
    m_vectors2[1].z = 0.0f;
    m_vectors1[0].x = 0.0f;
    m_vectors1[0].y = 0.0f;
    m_vectors1[0].z = 0.0f;
    m_vectors1[1].x = 0.0f;
    m_vectors1[1].y = 0.0f;
    m_vectors1[1].z = 0.0f;
    m_first2 = &m_vectors2[0];
    m_second2 = &m_vectors2[1];
    m_unknown60[0] = 0;
    m_unknown60[1] = 0;
    m_unknown60[2] = 0;
    m_unknown60[3] = 0;
    m_flags = 0;
    m_unknown60[4] = 0;
    m_unknown60[5] = 0;
    m_capacity = 0;
    m_keys = 0;
    m_hits = 0;
    m_count = 0;
    m_unknown8C = 0;
}

void AtSegmentHitList::Clear() {
    if ((m_flags & kNotOwned) == 0) {
        if (m_hits != 0) {
            operator delete(m_hits);
        }
        if (m_keys != 0) {
            operator delete(m_keys);
        }
    }
    Reset();
}

void AtSegmentHitList::CountOne() {
    if (m_count < m_capacity) {
        m_count++;
    }
}

void AtSegmentHitList::SetStorage(unsigned int capacity, void* keys, void* hits) {
    if (m_capacity != 0) {
        Clear();
    }
    m_hits = hits;
    m_capacity = capacity;
    m_keys = keys;
    m_flags |= kNotOwned;
}

void AtSegmentHitList::Allocate(unsigned int capacity) {
    if (m_capacity != 0) {
        Clear();
    }
    m_capacity = capacity;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_keys = operator new(m_capacity * kKeySize);
    AtPopHeap();
    if (m_keys == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned char* keyBytes = (unsigned char*)m_keys;
    for (unsigned int i = 0; i < m_capacity * kKeySize; i++) {
        keyBytes[i] = 0;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_hits = operator new(m_capacity * kHitSize);
    AtPopHeap();
    if (m_hits == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

AtSegmentQuery::AtSegmentQuery() {
    m_records = 0;
    m_unknown94 = 0;
}

void AtSegmentQuery::Allocate(unsigned int capacity) {
    AtSegmentHitList::Allocate(capacity);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_records = (AtSegmentHit*)operator new(capacity * sizeof(AtSegmentHit));
    AtPopHeap();
    if (m_records == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtSegmentQuery::SetStorage(unsigned int capacity, void* keys, void* hits, void* records) {
    AtSegmentHitList::SetStorage(capacity, keys, hits);
    m_records = (AtSegmentHit*)records;
}

void AtSegmentQuery::Clear() {
    if ((m_flags & kNotOwned) == 0 && m_records != 0) {
        operator delete(m_records);
    }
    AtSegmentHitList::Clear();
    m_records = 0;
    m_unknown94 = 0;
}

AtSegmentQuery::~AtSegmentQuery() {
    AtSegmentQuery::Clear();
}

void AtSegmentQuery::CountOne() {
    AtFatal("AtSegmentQuery::CountOne (0x00057B10) is not recovered", 0, 0, 0);
}

void AtQueryPool::SetHeap(void* heap) {
    s_heap = heap;
}

AtQueryPool::AtQueryPool() {
    m_count = 0;
    m_queries = 0;
    m_unknown18 = 0;
    m_made = 0;
    m_perQuery = 0;
    m_entries8 = 0;
    m_entries2C = 0;
    m_unknown20 = 0;
}

void AtQueryPool::Allocate() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_queries = new AtSegmentHitList[m_count];
    AtPopHeap();
    if (m_queries == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned int entry = 0;
    for (unsigned int i = 0; i < m_count; i++) {
        m_queries[i].SetStorage(m_perQuery, m_entries8 + entry * 8, m_entries2C + entry * 0x2C);
        entry += m_perQuery;
    }
}

void AtQueryPool::Free() {
    if (m_queries != 0) {
        delete[] m_queries;
        m_queries = 0;
    }
}

void* AtQueryPool::TakeNext() {
    AtFatal("AtQueryPool::TakeNext (0x000574F0) is not recovered", 0, 0, 0);
    return 0;
}

void AtQueryPool::Update(void*) {
    AtFatal("AtQueryPool::Update (0x00057800) is not recovered", 0, 0, 0);
}

void AtQueryPool::Release() {
    if (m_entries2C != 0) {
        operator delete(m_entries2C);
    }
    if (m_entries8 != 0) {
        operator delete(m_entries8);
    }
    Free();
    m_count = 0;
    m_queries = 0;
    m_unknown18 = 0;
    m_made = 0;
    m_perQuery = 0;
    m_entries8 = 0;
    m_entries2C = 0;
    m_sphere.center.x = 0;
    m_sphere.center.y = 0;
    m_sphere.center.z = 0;
    m_sphere.radius = 0;
    m_unknown20 = 0;
}

AtQueryPool::~AtQueryPool() {
    Release();
}

void AtQueryPool::Init(unsigned int count, unsigned int perQuery) {
    if ((m_made & 1) != 0) {
        Release();
    }
    m_count = count;
    m_unknown18 = 0;
    m_perQuery = perQuery;
    if (m_perQuery < 1) {
        m_perQuery = 1;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_entries8 = (unsigned char*)operator new(m_perQuery * m_count * 8);
    AtPopHeap();
    if (m_entries8 == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_entries2C = (unsigned char*)operator new(m_perQuery * m_count * 0x2C);
    AtPopHeap();
    if (m_entries2C == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Allocate();
    m_made = 1;
}

AtSegmentQueryPool::AtSegmentQueryPool() {
    m_segmentQueries = 0;
    m_entries10 = 0;
    m_unknown3C = 0;
}

void AtSegmentQueryPool::Allocate() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_segmentQueries = new AtSegmentQuery[m_count];
    AtPopHeap();
    if (m_segmentQueries == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_entries10 = (unsigned char*)operator new(m_perQuery * m_count * sizeof(AtSegmentHit));
    AtPopHeap();
    if (m_entries10 == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned int entry = 0;
    for (unsigned int i = 0; i < m_count; i++) {
        m_segmentQueries[i].SetStorage(m_perQuery, m_entries8 + entry * 8, m_entries2C + entry * 0x2C, m_entries10 + entry * sizeof(AtSegmentHit));
        entry += m_perQuery;
    }
}

void AtSegmentQueryPool::Free() {
    if (m_entries10 != 0) {
        operator delete(m_entries10);
        m_entries10 = 0;
    }
    if (m_segmentQueries != 0) {
        delete[] m_segmentQueries;
        m_segmentQueries = 0;
    }
}

void* AtSegmentQueryPool::TakeNext() {
    AtFatal("AtSegmentQueryPool::TakeNext (0x00048070) is not recovered", 0, 0, 0);
    return 0;
}

void AtSegmentQueryPool::Update(void*) {
    AtFatal("AtSegmentQueryPool::Update (0x00048D10) is not recovered", 0, 0, 0);
}

AtSegmentQueryPool::~AtSegmentQueryPool() {
    Release();
}
