// The .ADR loader, its pools and tree, and where a registry is filed. See include/at/AtDataRegistry.h.
#include "at/AtDataRegistry.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtFatal(const char* message, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);
extern "C" int toupper(int c);

void* AtDataRegistry::s_heap;
void* AtDataRegistry::s_poolHeap;
void* AtDataLink::s_heap;
void* AtDataDescriptor::s_heap;
void* AtDataRegistryNameSpaces::s_heap;

namespace {

// The words of the format. 0x002B2690. Boolean is ahead of Empty in the table, and behind it in the numbering.
enum Word {
    kDataComponentTypes = 0x39, kDataComponent = 0x3A, kNumComponents = 0x3B, kDataRegistry = 0x3C, kDataLink = 0x3D, kName = 0x3E, kType = 0x3F, kParent = 0x40, kFlags = 0x41,
    kNumElements = 0x42, kFloor = 0x43, kCeiling = 0x44, kFormatString = 0x45, kData = 0x46, kFirstType = 0x47, kBooleanWord = 0x48, kIntWord = 0x49, kRealWord = 0x4A, kShortNameWord = 0x4B,
    kStringWord = 0x4C, kBooleanArrayWord = 0x4D, kByteArrayWord = 0x4E, kIntArrayWord = 0x4F, kRealArrayWord = 0x50, kShortNameArrayWord = 0x51, kStringArrayWord = 0x52, kNameSpace = 0x53,
    kWillAcceptParentData = 0x54
};

const unsigned int kLastTypeOffset = 0xB;

const AtIoKeyword s_words[] = {
    {kDataComponentTypes, "DataComponentTypes"}, {kDataComponent, "DataComponent"}, {kNumComponents, "NumComponents"}, {kDataRegistry, "DataRegistry"}, {kDataLink, "DataLink"}, {kName, "Name"},
    {kType, "Type"}, {kParent, "Parent"}, {kFlags, "Flags"}, {kNumElements, "NumElements"}, {kFloor, "Floor"}, {kCeiling, "Ceiling"}, {kFormatString, "FormatString"}, {kData, "Data"},
    {kBooleanWord, "Boolean"}, {kFirstType, "Empty"}, {kIntWord, "Int"}, {kRealWord, "Real"}, {kShortNameWord, "ShortName"}, {kStringWord, "String"}, {kBooleanArrayWord, "BooleanArray"},
    {kByteArrayWord, "ByteArray"}, {kIntArrayWord, "IntArray"}, {kRealArrayWord, "RealArray"}, {kShortNameArrayWord, "ShortNameArray"}, {kStringArrayWord, "StringArray"}, {kNameSpace, "NameSpace"},
    {kWillAcceptParentData, "WillAcceptParentData"},
};
const unsigned int kWordCount = 0x1C;

unsigned int LengthOf(const char* text) {
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

void CopyText(char* to, const char* from) {
    while ((*to++ = *from++) != 0) {
    }
}

int CompareIgnoringCase(const char* a, const char* b) {
    for (;; a++, b++) {
        const int x = toupper((unsigned char)*a);
        const int y = toupper((unsigned char)*b);
        if (x != y) {
            return x - y;
        }
        if (x == 0) {
            return 0;
        }
    }
}

int Compare(const char* a, const char* b) {
    for (;; a++, b++) {
        if (*a != *b) {
            return 1;
        }
        if (*a == 0) {
            return 0;
        }
    }
}

// The original has these inline at each use: no heap is reported and then used, no memory is reported after the pop.
void PushHeap(void* heap) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
}

char* NewCopy(void* heap, const char* text) {
    PushHeap(heap);
    char* copy = (char*)operator new(LengthOf(text) + 1);
    AtPopHeap();
    if (copy == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyText(copy, text);
    return copy;
}

template <class T> AtDataComponent* NewValues(unsigned int count) {
    return AtNewArray<T>(count);
}

template <class T> void DeleteValues(AtDataComponent* values) {
    AtDeleteArray(static_cast<T*>(values));
}

template <class T> void ClampToBounds(T* value, T floor, T ceiling) {
    if (*value > ceiling) {
        *value = ceiling;
    }
    if (*value < floor) {
        *value = floor;
    }
}

}  // namespace

// ---- descriptor ---------------------------------------------------------------------------------------------------------

void AtDataDescriptor::SetHeap(void* heap) {
    s_heap = heap;
}

AtDataDescriptor::AtDataDescriptor() {
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_type = AtDataRegistry::kTypeCount;
    m_flags = 0;
    m_count = 1;
    m_intFloor = (int)0x80000000;
    m_intCeiling = 0x7FFFFFFF;
    *(unsigned int*)&m_realFloor = 0xFF7FFFFF;
    *(unsigned int*)&m_realCeiling = 0x7F7FFFFF;
    m_format = 0;
}

AtDataDescriptor::~AtDataDescriptor() {
    Clear();
}

void AtDataDescriptor::Clear() {
    if (m_format != 0) {
        operator delete(m_format);
    }
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_type = AtDataRegistry::kTypeCount;
    m_flags = 0;
    m_count = 1;
    m_intFloor = (int)0x80000000;
    m_intCeiling = 0x7FFFFFFF;
    *(unsigned int*)&m_realFloor = 0xFF7FFFFF;
    *(unsigned int*)&m_realCeiling = 0x7F7FFFFF;
    m_format = 0;
}

int AtDataDescriptor::SetType(unsigned int type, unsigned int count) {
    if (m_flags & kTyped) {
        Clear();
    }
    m_type = type;
    m_flags |= kTyped;
    m_count = count;
    return 0;
}

int AtDataDescriptor::SetIntBounds(int floor, int ceiling) {
    m_intFloor = floor;
    m_intCeiling = ceiling;
    return 0;
}

int AtDataDescriptor::SetRealBounds(float floor, float ceiling) {
    if (m_type != AtDataRegistry::kReal && m_type != AtDataRegistry::kRealArray) {
        return -1;
    }
    m_realFloor = floor;
    m_realCeiling = ceiling;
    return 0;
}

int AtDataDescriptor::GetIntBounds(int* floor, int* ceiling) {
    *floor = m_intFloor;
    *ceiling = m_intCeiling;
    return 0;
}

int AtDataDescriptor::SetFormat(const char* format) {
    if (m_format != 0) {
        operator delete(m_format);
        m_format = 0;
    }
    PushHeap(s_heap);
    m_format = (char*)operator new(LengthOf(format));
    AtPopHeap();
    if (m_format == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyText(m_format, format);
    return 0;
}

// ---- link ---------------------------------------------------------------------------------------------------------------

void AtDataLink::SetHeap(void* heap) {
    s_heap = heap;
}

AtDataLink::AtDataLink() {
    m_value = 0;
    m_name = 0;
    m_parent = 0;
    m_sibling = 0;
    m_firstChild = 0;
    m_inUse = 0;
}

AtDataLink::~AtDataLink() {
    Clear();
}

AtDataRegistry* AtDataLink::Registry() {
    return m_value != 0 ? m_value->m_registry : 0;
}

void AtDataLink::AttachTo(AtDataLink* parent) {
    m_parent = parent;
    if (parent == 0) {
        return;
    }
    if (parent->m_firstChild == 0) {
        parent->m_firstChild = this;
        return;
    }
    AtDataLink* last = parent->m_firstChild;
    while (last->m_sibling != 0) {
        last = last->m_sibling;
    }
    last->m_sibling = this;
}

void AtDataLink::Detach() {
    AtDataLink* parent = m_parent;
    if (parent == 0) {
        return;
    }
    if (parent->m_firstChild == this) {
        parent->m_firstChild = m_sibling;
        m_sibling = 0;
        m_parent = 0;
        return;
    }
    for (AtDataLink* before = parent->m_firstChild; before != 0; before = before->m_sibling) {
        if (before->m_sibling == this) {
            before->m_sibling = m_sibling;
            m_sibling = 0;
            break;
        }
    }
    m_parent = 0;
}

void AtDataLink::Clear() {
    Detach();
    for (AtDataLink* child = m_firstChild; child != 0; child = child->m_sibling) {
        child->m_parent = 0;
    }
    if (m_name != 0) {
        operator delete(m_name);
    }
    m_value = 0;
    m_name = 0;
    m_parent = 0;
    m_sibling = 0;
    m_firstChild = 0;
    m_inUse = 0;
}

int AtDataLink::Set(const char* name, AtDataComponent* value, AtDataLink* parent) {
    if (m_inUse != 0) {
        Clear();
    }
    m_value = value;
    if (name != 0) {
        m_name = NewCopy(s_heap, name);
    } else {
        m_name = 0;
    }
    AttachTo(parent);
    m_inUse = 1;
    return 0;
}

AtDataLinkRoot::~AtDataLinkRoot() {
}

AtDataRegistry* AtDataLinkRoot::Registry() {
    return m_registry;
}

// ---- pool ---------------------------------------------------------------------------------------------------------------

AtDataComponent* AtDataComponentPool::ElementAt(unsigned int index) {
    AtDataComponentPool* pool = this;
    while (index >= pool->m_count) {
        pool = pool->m_next;
        if (pool == 0) {
            return 0;
        }
    }
    char* first = (char*)pool->m_values;
    switch (pool->m_type) {
    case AtDataRegistry::kEmpty: return (AtDataComponent*)(first + index * sizeof(AtDataComponent));
    case AtDataRegistry::kBoolean: return (AtDataComponent*)(first + index * sizeof(AtDataBoolean));
    case AtDataRegistry::kInt: return (AtDataComponent*)(first + index * sizeof(AtDataInt));
    case AtDataRegistry::kReal: return (AtDataComponent*)(first + index * sizeof(AtDataReal));
    case AtDataRegistry::kShortName: return (AtDataComponent*)(first + index * sizeof(AtDataShortName));
    case AtDataRegistry::kString: return (AtDataComponent*)(first + index * sizeof(AtDataString));
    case AtDataRegistry::kBooleanArray: return (AtDataComponent*)(first + index * sizeof(AtDataBooleanArray));
    case AtDataRegistry::kByteArray: return (AtDataComponent*)(first + index * sizeof(AtDataByteArray));
    case AtDataRegistry::kIntArray: return (AtDataComponent*)(first + index * sizeof(AtDataIntArray));
    case AtDataRegistry::kRealArray: return (AtDataComponent*)(first + index * sizeof(AtDataRealArray));
    case AtDataRegistry::kShortNameArray: return (AtDataComponent*)(first + index * sizeof(AtDataShortNameArray));
    case AtDataRegistry::kStringArray: return (AtDataComponent*)(first + index * sizeof(AtDataStringArray));
    }
    return 0;
}

void AtDataComponentPool::Grow() {
    PushHeap(AtDataRegistry::s_poolHeap);
    m_next = (AtDataComponentPool*)operator new(sizeof(AtDataComponentPool));
    AtPopHeap();
    if (m_next == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_next->m_used = 0;
    m_next->m_values = 0;
    m_next->m_descriptors = 0;
    m_next->m_links = 0;
    m_next->m_next = 0;
    m_next->m_count = m_capacity;
    m_next->m_capacity = m_capacity;
    m_next->m_type = m_type;
}

void AtDataComponentPool::Allocate() {
    if (m_count == 0 || m_values != 0) {
        return;
    }
    if (m_type <= AtDataRegistry::kStringArray) {
        PushHeap(AtDataRegistry::s_poolHeap);
        switch (m_type) {
        case AtDataRegistry::kEmpty: m_values = NewValues<AtDataComponent>(m_count); break;
        case AtDataRegistry::kBoolean: m_values = NewValues<AtDataBoolean>(m_count); break;
        case AtDataRegistry::kInt: m_values = NewValues<AtDataInt>(m_count); break;
        case AtDataRegistry::kReal: m_values = NewValues<AtDataReal>(m_count); break;
        case AtDataRegistry::kShortName: m_values = NewValues<AtDataShortName>(m_count); break;
        case AtDataRegistry::kString: m_values = NewValues<AtDataString>(m_count); break;
        case AtDataRegistry::kBooleanArray: m_values = NewValues<AtDataBooleanArray>(m_count); break;
        case AtDataRegistry::kByteArray: m_values = NewValues<AtDataByteArray>(m_count); break;
        case AtDataRegistry::kIntArray: m_values = NewValues<AtDataIntArray>(m_count); break;
        case AtDataRegistry::kRealArray: m_values = NewValues<AtDataRealArray>(m_count); break;
        case AtDataRegistry::kShortNameArray: m_values = NewValues<AtDataShortNameArray>(m_count); break;
        case AtDataRegistry::kStringArray: m_values = NewValues<AtDataStringArray>(m_count); break;
        }
        AtPopHeap();
        if (m_values == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    PushHeap(AtDataRegistry::s_poolHeap);
    m_descriptors = AtNewArray<AtDataDescriptor>(m_count);
    AtPopHeap();
    if (m_descriptors == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_count; i++) {
        m_descriptors[i].SetType(m_type, 1);
    }
    PushHeap(AtDataRegistry::s_poolHeap);
    m_links = AtNewArray<AtDataLink>(m_count);
    AtPopHeap();
    if (m_links == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtDataComponentPool::Clear() {
    if (m_count == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_count; i++) {
        ElementAt(i)->Reset();
        m_descriptors[i].Clear();
        m_links[i].Clear();
    }
    // The original's is one delete[] on the base's pointer, through the first value's vftable, which C++ does not promise. Here it goes by the block's type, by way of its count, as the scene's arrays do.
    if (m_values != 0) {
        switch (m_type) {
        case AtDataRegistry::kEmpty: DeleteValues<AtDataComponent>(m_values); break;
        case AtDataRegistry::kBoolean: DeleteValues<AtDataBoolean>(m_values); break;
        case AtDataRegistry::kInt: DeleteValues<AtDataInt>(m_values); break;
        case AtDataRegistry::kReal: DeleteValues<AtDataReal>(m_values); break;
        case AtDataRegistry::kShortName: DeleteValues<AtDataShortName>(m_values); break;
        case AtDataRegistry::kString: DeleteValues<AtDataString>(m_values); break;
        case AtDataRegistry::kBooleanArray: DeleteValues<AtDataBooleanArray>(m_values); break;
        case AtDataRegistry::kByteArray: DeleteValues<AtDataByteArray>(m_values); break;
        case AtDataRegistry::kIntArray: DeleteValues<AtDataIntArray>(m_values); break;
        case AtDataRegistry::kRealArray: DeleteValues<AtDataRealArray>(m_values); break;
        case AtDataRegistry::kShortNameArray: DeleteValues<AtDataShortNameArray>(m_values); break;
        case AtDataRegistry::kStringArray: DeleteValues<AtDataStringArray>(m_values); break;
        }
    }
    if (m_descriptors != 0) {
        AtDeleteArray(m_descriptors);
    }
    if (m_links != 0) {
        AtDeleteArray(m_links);
    }
    if (m_next != 0) {
        m_next->Clear();
        operator delete(m_next);
    }
    m_count = 0;
    m_capacity = 4;
    m_type = 0;
    m_used = 0;
    m_values = 0;
    m_descriptors = 0;
    m_links = 0;
    m_next = 0;
}

AtDataLink* AtDataComponentPool::FindByName(const char* name, int exact) {
    AtDataComponentPool* pool = this;
    while (pool->m_count != 0 && pool->m_values != 0) {
        for (unsigned int i = 0; i < pool->m_count; i++) {
            AtDataLink* link = &pool->m_links[i];
            if (link->m_inUse != 0 && link->m_name != 0 && link->m_name[0] != 0) {
                if ((exact != 0 ? Compare(name, link->m_name) : CompareIgnoringCase(name, link->m_name)) == 0) {
                    return link;
                }
            }
        }
        pool = pool->m_next;
        if (pool == 0) {
            return 0;
        }
        exact = 0;
    }
    return 0;
}

AtDataLink* AtDataComponentPool::New(int) {
    AtDataComponentPool* pool = this;
    unsigned int index;
    for (;;) {
        if (pool->m_count == 0) {
            pool->m_used = 0;
            pool->m_values = 0;
            pool->m_descriptors = 0;
            pool->m_links = 0;
            pool->m_next = 0;
            pool->m_count = 4;
            pool->m_capacity = 4;
        }
        if (pool->m_values == 0) {
            pool->Allocate();
        }
        bool found = false;
        for (index = 0; index < pool->m_count; index++) {
            if (pool->m_links[index].m_inUse == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
        if (pool->m_next == 0) {
            pool->Grow();
        }
        pool = pool->m_next;
    }
    pool->m_used++;
    AtDataComponent* value = pool->ElementAt(index);
    AtDataLink* link = &pool->m_links[index];
    link->Set(0, value, 0);
    value->SetDescriptor(&pool->m_descriptors[index]);
    return link;
}

// ---- where registries are filed -----------------------------------------------------------------------------------------

AtDataRegistryGroup::AtDataRegistryGroup() {
    m_name[0] = 0;
    m_first = 0;
    m_nextFree = 0;
    m_database = 0;
}

void AtDataRegistryGroup::Reset() {
    m_name[0] = 0;
    m_first = 0;
    m_nextFree = 0;
    m_database = 0;
}

void AtDataRegistryGroup::SetName(const char* name) {
    AtCopyName8(m_name, name);
}

void AtDataRegistryGroup::Add(AtDataRegistry* registry) {
    if (registry->m_group != 0) {
        registry->m_group->Remove(registry);
    }
    registry->m_next = m_first;
    m_first = registry;
    registry->m_group = this;
}

void AtDataRegistryGroup::Remove(AtDataRegistry* registry) {
    AtDataRegistry** link = &m_first;
    while (*link != 0 && *link != registry) {
        link = &(*link)->m_next;
    }
    if (*link == 0) {
        return;
    }
    *link = registry->m_next;
    registry->m_group = 0;
    if (m_first == 0 && m_database != 0) {
        m_database->Release(this);
    }
}

void AtDataRegistryNameSpaces::SetHeap(void* heap) {
    s_heap = heap;
}

AtDataRegistryNameSpaces::AtDataRegistryNameSpaces() {
    m_pool = 0;
    m_poolSize = 0;
    m_free = 0;
    m_last = 0;
}

AtDataRegistryNameSpaces::~AtDataRegistryNameSpaces() {
    if (m_pool != 0) {
        operator delete(m_pool);
    }
    GmNameMap::Clear();
    m_pool = 0;
    m_poolSize = 0;
    m_free = 0;
    m_last = 0;
}

void AtDataRegistryNameSpaces::Clear() {
    if (m_pool != 0) {
        operator delete(m_pool);
    }
    GmNameMap::Clear();
    m_pool = 0;
    m_poolSize = 0;
    m_free = 0;
    m_last = 0;
}

void AtDataRegistryNameSpaces::Init(unsigned int count) {
    if (m_pool != 0) {
        Clear();
    }
    GmNameMap::Init(count);
    m_poolSize = count;
    PushHeap(s_heap);
    AtDataRegistryGroup* pool = (AtDataRegistryGroup*)operator new(count * sizeof(AtDataRegistryGroup));
    if (pool != 0) {
        for (unsigned int i = 0; i < count; i++) {
            new (pool + i, AtPlacement()) AtDataRegistryGroup;
        }
    }
    m_pool = pool;
    AtPopHeap();
    if (m_pool == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_free = m_pool;
    AtDataRegistryGroup* last = m_pool + m_poolSize - 1;
    AtDataRegistryGroup* group = m_pool;
    while (group < last) {
        group->m_nextFree = group + 1;
        group++;
    }
    group->m_nextFree = 0;
}

void AtDataRegistryNameSpaces::Release(AtDataRegistryGroup* group) {
    GmNameMap::Remove(group->m_name);
    group->Reset();
    group->m_nextFree = m_free;
    m_free = group;
}

void AtDataRegistryNameSpaces::Add(const char* space, AtDataRegistry* registry) {
    AtDataRegistryGroup* group = (AtDataRegistryGroup*)Find(space);
    if (group == 0) {
        if (m_free == 0) {
            AtFatal("Atlas Base: Name space database overflow.", 0, 0, 0);
        }
        group = m_free;
        m_free = group->m_nextFree;
        Insert(space, group);
        group->SetName(space);
        group->m_database = this;
    }
    m_last = group;
    group->Add(registry);
}

void AtDataRegistryNameSpaces::Remove(AtDataRegistry* registry) {
    if (registry->m_group != 0) {
        registry->m_group->Remove(registry);
    }
}

void AtDataRegistryOwner::Add(AtDataRegistry* registry, const char* space) {
    if (space != 0 && space[0] != 0) {
        m_spaces.Add(space, registry);
    } else {
        m_plain.Add(registry);
    }
}

void AtDataRegistryOwner::Remove(AtDataRegistry* registry) {
    char space[8];
    space[0] = 0;
    if (registry->m_group != 0 && registry->m_group->m_name[0] != 0) {
        AtCopyName8(space, registry->m_group->m_name);
    }
    if (space[0] != 0) {
        AtDataRegistryNameSpaces::Remove(registry);
    } else {
        m_plain.Remove(registry);
    }
}

// ---- registry -----------------------------------------------------------------------------------------------------------

void AtDataRegistry::SetHeap(void* heap) {
    s_heap = heap;
}

AtDataRegistry::AtDataRegistry() {
    Reset();
}

AtDataRegistry::~AtDataRegistry() {
    if (m_loaded != 0) {
        Clear();
    }
}

void AtDataRegistry::Reset() {
    m_owner = 0;
    m_parent = 0;
    m_root.Set(0, 0, 0);
    m_root.m_registry = this;
    m_unknown38 = 0;
    for (unsigned int i = 0; i < kTypeCount; i++) {
        AtDataComponentPool& pool = m_pools[i];
        pool.m_count = 0;
        pool.m_capacity = 0;
        pool.m_type = i;
        pool.m_used = 0;
        pool.m_values = 0;
        pool.m_descriptors = 0;
        pool.m_links = 0;
        pool.m_next = 0;
    }
    m_loaded = 0;
    m_group = 0;
    m_fileName = 0;
}

void AtDataRegistry::Clear() {
    if (m_unknown38 != 0) {
        operator delete(m_unknown38);
        m_unknown38 = 0;
    }
    if (m_loaded == 0) {
        return;
    }
    if (m_owner != 0) {
        m_owner->Remove(this);
    }
    for (unsigned int i = 0; i < kTypeCount; i++) {
        m_pools[i].Clear();
    }
    if (m_fileName != 0) {
        operator delete(m_fileName);
    }
    Reset();
}

int AtDataRegistry::NameMatches(const char* name, const char* path) {
    while (*name != 0) {
        if (*path == 0 || *name == '\\' || *path == '\\') {
            if (*name != 0 && *name != '\\') {
                return 0;
            }
            break;
        }
        if (*name != *path) {
            return 0;
        }
        name++;
        path++;
    }
    return *path == 0 || *path == '\\';
}

AtDataLink* AtDataRegistry::FindChild(const char* path, AtDataLink* under) {
    for (AtDataLink* link = under != 0 ? under->m_firstChild : m_root.m_firstChild; link != 0; link = link->m_sibling) {
        if (link->m_value == 0) {
            AtDataLink* found = FindChild(path, link);
            if (found != 0) {
                return found;
            }
        } else if (NameMatches(link->m_name, path)) {
            return link;
        }
    }
    return 0;
}

void AtDataRegistry::Clamp(unsigned int type, AtDataComponent* value) {
    switch (type) {
    case kBoolean: {
        int* v = &static_cast<AtDataBoolean*>(value)->m_value;
        ClampToBounds(v, 0, 1);
        break;
    }
    case kInt: {
        int* v = &static_cast<AtDataInt*>(value)->m_value;
        ClampToBounds(v, value->m_descriptor->m_intFloor, value->m_descriptor->m_intCeiling);
        break;
    }
    case kReal: {
        float* v = &static_cast<AtDataReal*>(value)->m_value;
        ClampToBounds(v, value->m_descriptor->m_realFloor, value->m_descriptor->m_realCeiling);
        break;
    }
    case kBooleanArray: {
        int* data = static_cast<AtDataBooleanArray*>(value)->m_data;
        for (unsigned int i = 0; i < value->m_descriptor->m_count; i++) {
            ClampToBounds(&data[i], 0, 1);
        }
        break;
    }
    case kByteArray: {
        // The original tests each byte against 0xFF, which none exceeds. So it only reads them, and that is a crash if the value is of another class.
        const volatile unsigned char* data = static_cast<AtDataByteArray*>(value)->m_data;
        for (unsigned int i = 0; i < value->m_descriptor->m_count; i++) {
            (void)data[i];
        }
        break;
    }
    case kIntArray: {
        int* data = static_cast<AtDataIntArray*>(value)->m_data;
        for (unsigned int i = 0; i < value->m_descriptor->m_count; i++) {
            ClampToBounds(&data[i], value->m_descriptor->m_intFloor, value->m_descriptor->m_intCeiling);
        }
        break;
    }
    case kRealArray: {
        float* data = static_cast<AtDataRealArray*>(value)->m_data;
        for (unsigned int i = 0; i < value->m_descriptor->m_count; i++) {
            ClampToBounds(&data[i], value->m_descriptor->m_realFloor, value->m_descriptor->m_realCeiling);
        }
        break;
    }
    }
}

int AtDataRegistry::Read(AtDataRegistryOwner* owner, AtIoTokenizer* tokenizer, char* spaceOut, AtDataRegistry* parent) {
    m_owner = owner;
    if (spaceOut != 0) {
        spaceOut[0] = 0;
    }
    m_parent = parent;
    if (parent != 0) {
        m_root.AttachTo(&parent->m_root);
    }
    if (m_loaded != 0) {
        Clear();
    }
    s_poolHeap = s_heap;
    tokenizer->SetKeywords(s_words, kWordCount, 0);

    int word = tokenizer->Next();
    char space[8];
    space[0] = 0;
    if (word == kNameSpace) {
        tokenizer->ExpectName(space);
        if (spaceOut != 0) {
            AtCopyName8(spaceOut, space);
        }
        word = tokenizer->Next();
    }
    if (word != kDataComponentTypes) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }

    // How many of each.
    m_componentTypes = tokenizer->ExpectCount();
    for (unsigned int i = 0; i < m_componentTypes; i++) {
        if (tokenizer->Next() != kDataComponent) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        tokenizer->ExpectLeftCurly();
        if (tokenizer->Next() != kType) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        const unsigned int typeOffset = tokenizer->Next() - kFirstType;
        if (tokenizer->Next() != kNumComponents) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        const int count = tokenizer->ExpectInt();
        if (typeOffset <= kLastTypeOffset) {
            AtDataComponentPool& pool = m_pools[typeOffset];
            pool.m_used = 0;
            pool.m_values = 0;
            pool.m_descriptors = 0;
            pool.m_links = 0;
            pool.m_next = 0;
            pool.m_count = count;
            pool.m_capacity = count;
            pool.m_type = typeOffset;
        }
        tokenizer->ExpectRightCurly();
    }
    tokenizer->ExpectRightCurly();
    for (unsigned int i = 0; i < kTypeCount; i++) {
        m_pools[i].Allocate();
    }

    // The links. What the type and the value were carries on from one to the next, so a link that gives no Type takes the last one's, and one with no Data crashes on its link being null.
    unsigned int type = kTypeCount;
    AtDataComponent* value = 0;
    if (tokenizer->Next() != kDataRegistry) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const unsigned int links = tokenizer->ExpectCount();
    for (unsigned int i = 0; i < links; i++) {
        AtDataLink* link = 0;
        AtDataLink* under = &m_root;
        char* name = 0;
        char* format = 0;
        int intFloor = (int)0x80000000;
        int intCeiling = 0x7FFFFFFF;
        float realFloor;
        float realCeiling;
        *(unsigned int*)&realFloor = 0xFF7FFFFF;
        *(unsigned int*)&realCeiling = 0x7F7FFFFF;
        int elements = 1;
        if (tokenizer->Next() != kDataLink) {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        while (tokenizer->Next() != AtIoTokenizer::kLeftCurly) {
        }
        for (word = tokenizer->Next(); word != AtIoTokenizer::kRightCurly; word = tokenizer->Next()) {
            switch (word) {
            case kName:
                name = NewCopy(AtIoTokenFile::s_heap, tokenizer->ExpectString());
                break;
            case kType: {
                const unsigned int typeOffset = tokenizer->Next() - kFirstType;
                if (typeOffset > kLastTypeOffset) {
                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                } else {
                    type = typeOffset;
                }
                break;
            }
            case kParent: {
                // Part by part from the parent registry's tree, or this one's; failing that, the whole text as one name, among this registry's links; failing that, the root.
                const char* path = tokenizer->ExpectString();
                AtDataRegistry* within = parent != 0 ? parent : this;
                AtDataLink* found = 0;
                if (path != 0) {
                    const char* part = path;
                    while (*part != 0) {
                        found = within->FindChild(part, found);
                        if (found == 0) {
                            break;
                        }
                        while (*part != 0 && *part != '\\') {
                            part++;
                        }
                        if (*part == '\\') {
                            part++;
                        }
                    }
                }
                if (found == 0) {
                    for (unsigned int p = 0; p < kTypeCount && found == 0; p++) {
                        found = m_pools[p].FindByName(path, 0);
                    }
                }
                under = found;
                break;
            }
            case kNumElements:
                elements = tokenizer->ExpectInt();
                break;
            case kFloor:
                if (type == kReal || type == kRealArray) {
                    realFloor = tokenizer->ExpectReal();
                } else {
                    intFloor = tokenizer->ExpectInt();
                }
                break;
            case kCeiling:
                if (type == kReal || type == kRealArray) {
                    realCeiling = tokenizer->ExpectReal();
                } else {
                    intCeiling = tokenizer->ExpectInt();
                }
                break;
            case kFormatString:
                format = NewCopy(AtIoTokenFile::s_heap, tokenizer->ExpectString());
                break;
            case kData: {
                link = m_pools[type].New(1);
                value = link->m_value;
                AtDataDescriptor* descriptor = value->m_descriptor;
                descriptor->SetType(type, elements);
                if (type == kReal || type == kRealArray) {
                    descriptor->SetRealBounds(realFloor, realCeiling);
                } else {
                    descriptor->SetIntBounds(intFloor, intCeiling);
                }
                value->Read(descriptor, tokenizer);
                value->m_registry = this;
                break;
            }
            case kWillAcceptParentData:
                value = link->m_value;
                value->m_descriptor->m_flags |= AtDataDescriptor::kAcceptsParentData;
                break;
            default:
                tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                break;
            }
        }
        link->Set(name, value, under != 0 ? under : &m_root);
        operator delete(name);
        if (format != 0) {
            link->m_value->m_descriptor->SetFormat(format);
            operator delete(format);
        }
        Clamp(type, value);
    }
    tokenizer->ExpectRightCurly();
    m_loaded = 1;
    owner->Add(this, space);
    return 0;
}

int AtDataRegistry::Load(AtDataRegistryOwner* owner, const char* name, int, char* spaceOut, AtDataRegistry* parent) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ADR", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ADR");
    }
    const char* path = file.m_file.m_file->m_path;
    PushHeap(s_heap);
    m_fileName = (char*)operator new(LengthOf(path) + 1);
    AtPopHeap();
    if (m_fileName == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyText(m_fileName, path);
    Read(owner, file.m_tokenizer, spaceOut, parent);
    file.Close();
    m_loaded = 1;
    return 0;
}
