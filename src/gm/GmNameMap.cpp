// AtNamesEqual, GmNameMapGroup::Detach, GmNameMap. Original address clusters 0x00013FC0-0x0001402E, 0x00017E80-0x00017EC9, 0x00024C00-0x000252E9.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "gm/GmNameMap.h"
#include "gm/GmLevelManager.h"

extern "C" {
char* strcat(char* to, const char* from);
}

void* GmNameMap::s_heap;

namespace {

const unsigned int kSizes[] = {1, 3, 5, 7, 11, 23, 47, 89, 167, 331, 467, 659, 1289, 2579, 5393, 10939, 50159, 100237};  // 0x00272E18

unsigned int SizeFor(unsigned int count) {
    for (unsigned int i = 0; i < sizeof kSizes / sizeof kSizes[0]; i++) {
        if (count < kSizes[i]) {
            return kSizes[i];
        }
    }
    return count;
}

GmNameMap::Entry* NewEntries(unsigned int size) {
    if (GmNameMap::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmNameMap::s_heap);
    GmNameMap::Entry* entries = (GmNameMap::Entry*)operator new(size * sizeof(GmNameMap::Entry));
    if (entries != 0) {
        for (unsigned int i = 0; i < size; i++) {
            entries[i].m_name[0] = 0;
            entries[i].m_value = 0;
        }
    }
    AtPopHeap();
    if (entries == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return entries;
}

void ReportDuplicate(const char* name) {
    char message[0x2C];
    AtCopyName9(message, name);
    strcat(message, ": Duplicate name encountered");
    AtFatal(message, 0, 0, 0);
}

}  // namespace

int AtNamesEqual(const char* a, const char* b) {
    for (int i = 0; i < 7; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == 0) {
            return 1;
        }
    }
    return a[7] == b[7];
}

GmNameMapGroup::GmNameMapGroup() {
    Reset();
}

void GmNameMapGroup::Reset() {
    m_name[0] = 0;
    m_first = 0;
    m_nextFree = 0;
    m_database = 0;
    m_used = 0;
}

void GmNameMapGroup::SetName(const char* name) {
    AtCopyName8(m_name, name);
    m_used = 1;
}

void* GmNameMapGroup::Find(const char* name) {
    for (GmNameMap* m = m_first; m != 0; m = m->m_next) {
        void* value = m->Find(name);
        if (value != 0) {
            return value;
        }
    }
    return 0;
}

void GmNameMapGroup::GetNameOf(const void* value, char* out) {
    for (GmNameMap* m = m_first; m != 0; m = m->m_next) {
        m->GetNameOf(value, out);
        if (out[0] != 0) {
            return;
        }
    }
    out[0] = 0;
}

void GmNameMapGroup::Add(GmNameMap* map) {
    if (map->m_group != 0) {
        map->m_group->Detach(map);
    }
    map->m_next = m_first;
    m_first = map;
    map->m_group = this;
}

void GmNameMapGroup::MoveToFront(GmNameMap* map) {
    char name[8];
    AtCopyName8(name, (const char*)map->m_group);
    if (AtNamesEqual(name, m_name) && m_first != map) {
        Detach(map);
        if (map->m_group != 0) {
            map->m_group->Detach(map);
        }
        map->m_next = m_first;
        m_first = map;
        map->m_group = this;
    }
}

void GmNameMapGroup::Detach(GmNameMap* map) {
    GmNameMap** link = &m_first;
    for (GmNameMap* m = m_first; m != 0; m = *link) {
        if (m == map) {
            *link = m->m_next;
            m->m_group = 0;
            if (m_first == 0 && m_database != 0) {
                m_database->Release(this);
            }
            return;
        }
        link = &m->m_next;
    }
}

GmNameMap::GmNameMap() {
    m_entries = 0;
    m_size = 0;
    m_next = 0;
    m_group = 0;
}

GmNameMap::~GmNameMap() {
    GmNameMap::Clear();
}

void GmNameMap::Clear() {
    if (m_group != 0) {
        m_group->Detach(this);
    }
    if (m_entries != 0) {
        operator delete(m_entries);
    }
    m_entries = 0;
    m_size = 0;
    m_next = 0;
    m_group = 0;
}

void GmNameMap::GetNameOf(const void* value, char* out) {
    for (unsigned int i = 0; i < m_size; i++) {
        if (m_entries[i].m_value == value) {
            AtCopyName8(out, m_entries[i].m_name);
            return;
        }
    }
    out[0] = 0;
}

void GmNameMap::GetName(unsigned int index, char* out) {
    AtCopyName8(out, m_entries[index].m_name);
}

unsigned int GmNameMap::HashOf(const char* name) {
    unsigned int sum = (unsigned int)(int)(signed char)name[0];
    unsigned int shift = 7;
    // Original behaviour preserved: an empty name still reads its second byte.
    for (int i = 1; i < 8 && name[i] != 0; i++) {
        sum += (unsigned int)(int)(signed char)name[i] << shift;
        shift = (shift + 7) & 0x1F;
    }
    return sum % m_size;
}

void GmNameMap::Grow() {
    const unsigned int oldSize = m_size;
    Entry* old = m_entries;
    m_size = SizeFor(oldSize + 1);
    m_entries = NewEntries(m_size);
    for (unsigned int i = 0; i < oldSize; i++) {
        if (old[i].m_value != 0) {
            Insert(old[i].m_name, old[i].m_value);
        }
    }
    operator delete(old);
}

void GmNameMap::Init(unsigned int count) {
    if (m_entries != 0) {
        Clear();
    }
    m_size = SizeFor(count);
    m_entries = NewEntries(m_size);
}

void GmNameMap::Insert(const char* name, void* value) {
    for (;;) {
        const unsigned int home = HashOf(name);
        unsigned int i = home;
        do {
            if (m_entries[i].m_value == 0) {
                AtCopyName8(m_entries[i].m_name, name);
                m_entries[i].m_value = value;
                return;
            }
            if (AtNamesEqual(m_entries[i].m_name, name)) {
                ReportDuplicate(name);
            }
            i++;
            if (i >= m_size) {
                i = 0;
            }
        } while (i != home);
        Grow();
    }
}

void GmNameMap::Insert(const char* name, void* value, unsigned int* index) {
    for (;;) {
        const unsigned int home = HashOf(name);
        unsigned int i = home;
        do {
            if (m_entries[i].m_value == 0) {
                AtCopyName8(m_entries[i].m_name, name);
                if (index != 0) {
                    *index = i;
                }
                m_entries[i].m_value = value;
                return;
            }
            if (AtNamesEqual(m_entries[i].m_name, name)) {
                ReportDuplicate(name);
            }
            i++;
            if (i >= m_size) {
                i = 0;
            }
        } while (i != home);
        Grow();
    }
}

void GmNameMap::Remove(const char* name) {
    Entry* const start = m_entries;
    Entry* const end = start + m_size;
    Entry* p = start + HashOf(name);
    // Original behaviour preserved: a freed slot keeps its name, so it can match here. An absent name in a full table never ends.
    while (!AtNamesEqual(p->m_name, name)) {
        if (p->m_value == 0) {
            return;
        }
        p++;
        if (p >= end) {
            p = m_entries;
        }
    }
    p->m_value = 0;
    p++;
    if (p >= end) {
        p = m_entries;
    }
    while (p->m_value != 0) {
        void* value = p->m_value;
        p->m_value = 0;
        Insert(p->m_name, value);
        p++;
        if (p >= end) {
            p = m_entries;
        }
    }
}

void* GmNameMap::Find(const char* name) {
    const unsigned int home = HashOf(name);
    unsigned int i = home;
    while (!AtNamesEqual(m_entries[i].m_name, name)) {
        if (m_entries[i].m_value == 0) {
            return 0;
        }
        i++;
        if (i >= m_size) {
            i = 0;
        }
        if (i == home) {
            return 0;
        }
    }
    return m_entries[i].m_value;
}

void* GmNameMap::Find(const char* name, unsigned int* index) {
    const unsigned int home = HashOf(name);
    unsigned int i = home;
    while (!AtNamesEqual(m_entries[i].m_name, name)) {
        if (m_entries[i].m_value == 0) {
            return 0;
        }
        i++;
        if (i >= m_size) {
            i = 0;
        }
        if (i == home) {
            return 0;
        }
    }
    if (index != 0) {
        *index = i;
    }
    return m_entries[i].m_value;
}

void* GmNameSpaceDb::s_heap;

void GmNameSpaceDb::SetHeap(void* heap) {
    s_heap = heap;
}

GmNameSpaceDb::GmNameSpaceDb() {
    m_pool = 0;
    m_poolSize = 0;
    m_free = 0;
    m_last = 0;
}

GmNameSpaceDb::~GmNameSpaceDb() {
    GmNameSpaceDb::Clear();
}

void GmNameSpaceDb::Clear() {
    if (m_pool != 0) {
        delete[] m_pool;
    }
    m_pool = 0;
    m_poolSize = 0;
    m_free = 0;
    m_last = 0;
    GmNameMap::Clear();
}

void GmNameSpaceDb::Init(unsigned int count) {
    if (m_pool != 0) {
        Clear();
    }
    GmNameMap::Init(count);
    m_poolSize = count;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_pool = new GmNameMapGroup[count];
    AtPopHeap();
    if (m_pool == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_free = m_pool;
    GmNameMapGroup* last = m_pool + m_poolSize - 1;
    GmNameMapGroup* g = m_pool;
    while (g < last) {
        g->m_nextFree = g + 1;
        g = g + 1;
    }
    g->m_nextFree = 0;
}

void GmNameSpaceDb::Release(GmNameMapGroup* group) {
    GmNameMap::Remove(group->m_name);
    group->Reset();
    group->m_nextFree = m_free;
    m_free = group;
}

void GmNameSpaceDb::Add(const char* space, GmNameMap* map) {
    GmNameMapGroup* group = (GmNameMapGroup*)GmNameMap::Find(space);
    if (group == 0) {
        if (m_free == 0) {
            AtFatal("Atlas Base: Name space database overflow.", 0, 0, 0);
        }
        group = m_free;
        m_free = group->m_nextFree;
        GmNameMap::Insert(space, group);
        group->SetName(space);
        group->m_database = this;
    }
    m_last = group;
    group->Add(map);
}

void GmNameSpaceDb::Remove(GmNameMap* map) {
    if (map->m_group != 0) {
        map->m_group->Detach(map);
    }
}

void* GmNameSpaceDb::Find(const char* space, const char* name) {
    if (m_last == 0 || !AtNamesEqual(m_last->m_name, space)) {
        GmNameMapGroup* group = (GmNameMapGroup*)GmNameMap::Find(space);
        if (group == 0) {
            return 0;
        }
        m_last = group;
    }
    return m_last->Find(name);
}

void GmNameSpaceDb::GetNameOf(const char* space, const void* value, char* out) {
    if (m_last == 0 || !AtNamesEqual(m_last->m_name, space)) {
        GmNameMapGroup* group = (GmNameMapGroup*)GmNameMap::Find(space);
        if (group == 0) {
            out[0] = 0;
            return;
        }
        m_last = group;
    }
    m_last->GetNameOf(value, out);
}
