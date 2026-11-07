// See include/at/AtSegmentQuery.h.
#include "at/AtSegmentQuery.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

void* AtQuery::s_heap;
void* AtQueryPool::s_heap;

void AtQuery::Reset() {
    m_listA[0] = 0;
    m_listA[1] = 0;
    m_listA[2] = 0;
    m_listB[0] = 0;
    m_listB[1] = 0;
    m_listB[2] = 0;
    m_headA = m_listA;
    m_headB = m_listB;
    m_listC[0] = 0;
    m_listC[1] = 0;
    m_listC[2] = 0;
    m_listD[0] = 0;
    m_listD[1] = 0;
    m_listD[2] = 0;
    for (int i = 0; i < 6; i++) {
        m_unknown28[i] = 0;
    }
    m_headC = m_listC;
    m_headD = m_listD;
    for (int i = 0; i < 4; i++) {
        m_unknown60[i] = 0;
    }
    m_flags = 0;
    m_unknown70 = 0;
    m_unknown74 = 0;
    m_count = 0;
    m_storage80 = 0;
    m_storage84 = 0;
    m_unknown88 = 0;
    m_unknown8C = 0;
}

AtQuery::AtQuery() {
    Reset();
}

void AtQuery::SetHeap(void* heap) {
    s_heap = heap;
}

void AtQuery::Reserve(unsigned int count) {
    if (m_count > 0) {
        Clear();
    }
    m_count = count;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_storage80 = operator new(m_count * 8);
    AtPopHeap();
    if (m_storage80 == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned char* bytes = (unsigned char*)m_storage80;
    for (unsigned int i = 0; i < m_count * 8; i++) {
        bytes[i] = 0;
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_storage84 = operator new(m_count * 0x2C);
    AtPopHeap();
    if (m_storage84 == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtQuery::Attach(unsigned int count, void* storage80, void* storage84) {
    if (m_count > 0) {
        Clear();
    }
    m_storage84 = storage84;
    m_count = count;
    m_storage80 = storage80;
    m_flags |= kStorageNotOwned;
}

void AtQuery::Clear() {
    if ((m_flags & kStorageNotOwned) == 0) {
        if (m_storage84 != 0) {
            operator delete(m_storage84);
        }
        if (m_storage80 != 0) {
            operator delete(m_storage80);
        }
    }
    Reset();
}

AtQuery::~AtQuery() {
    AtQuery::Clear();
}

AtSegmentQuery::AtSegmentQuery() {
    m_storage90 = 0;
    m_unknown94 = 0;
}

void AtSegmentQuery::Reserve(unsigned int count) {
    AtQuery::Reserve(count);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_storage90 = operator new(count * 0x10);
    AtPopHeap();
    if (m_storage90 == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtSegmentQuery::Attach(unsigned int count, void* storage80, void* storage84, void* storage90) {
    AtQuery::Attach(count, storage80, storage84);
    m_storage90 = storage90;
}

void AtSegmentQuery::Clear() {
    if ((m_flags & kStorageNotOwned) == 0 && m_storage90 != 0) {
        operator delete(m_storage90);
    }
    AtQuery::Clear();
    m_storage90 = 0;
    m_unknown94 = 0;
}

AtSegmentQuery::~AtSegmentQuery() {
    AtSegmentQuery::Clear();
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
    m_queries = new AtQuery[m_count];
    AtPopHeap();
    if (m_queries == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned int entry = 0;
    for (unsigned int i = 0; i < m_count; i++) {
        m_queries[i].Attach(m_perQuery, m_entries8 + entry * 8, m_entries2C + entry * 0x2C);
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
    m_entries10 = (unsigned char*)operator new(m_perQuery * m_count * 0x10);
    AtPopHeap();
    if (m_entries10 == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned int entry = 0;
    for (unsigned int i = 0; i < m_count; i++) {
        m_segmentQueries[i].Attach(m_perQuery, m_entries8 + entry * 8, m_entries2C + entry * 0x2C, m_entries10 + entry * 0x10);
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
