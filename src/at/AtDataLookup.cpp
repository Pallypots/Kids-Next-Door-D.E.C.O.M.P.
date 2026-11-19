// Reading a data registry once it is loaded: lookups by path and by name, the registry made as a copy of another, and what start-up does to the
// registry that every loaded one hangs under. See include/at/AtDataRegistry.h. All names are inferred.
#include "at/AtDataRegistry.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);
extern "C" int toupper(int c);

namespace {

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

void AppendText(char* to, const char* from) {
    while (*to != 0) {
        to++;
    }
    CopyText(to, from);
}

// The original calls _stricmp (0x00126F41), which lowers each character; only whether the two are equal is used, and for that raising is the same.
bool EqualIgnoringCase(const char* a, const char* b) {
    for (;; a++, b++) {
        const int x = toupper((unsigned char)*a);
        if (x != toupper((unsigned char)*b)) {
            return false;
        }
        if (x == 0) {
            return true;
        }
    }
}

// Inline in the original, as strcmp; only equality is used.
bool Equal(const char* a, const char* b) {
    for (;; a++, b++) {
        if (*a != *b) {
            return false;
        }
        if (*a == 0) {
            return true;
        }
    }
}

// Inline at each use in the original: no heap is reported and then used anyway.
void PushHeap(void* heap) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
}

}  // namespace

// ---- a link's path ------------------------------------------------------------------------------------------------------

void AtDataLink::AppendPath(char* out, int skipNameless) {
    AtDataLink* link = this;
    while (link->m_name == 0) {
        if (skipNameless == 0) {
            return;
        }
        link = link->m_parent;
        if (link == 0) {
            return;
        }
        skipNameless = 0;
    }
    if (link->m_name[0] == 0) {
        return;
    }
    if (link->m_parent != 0) {
        link->m_parent->AppendPath(out, 0);
    }
    AppendText(out, link->m_name);
    AppendText(out, "\\");
}

// ---- lookups ------------------------------------------------------------------------------------------------------------

AtDataLink* AtDataRegistry::FindPath(AtDataLink* from, const char* path, int climb) {
    if (path == 0) {
        return from;
    }
    while (*path != 0) {
        if (climb != 0) {
            // This part among the children of each link from here up, nearest first. Failing everywhere leaves nothing, and the next part is then looked for from the root.
            AtDataLink* at = from;
            for (;;) {
                AtDataLink* up = at != 0 ? at->m_parent : 0;
                from = FindChild(path, at);
                if (from != 0 || up == 0) {
                    break;
                }
                at = up;
            }
        } else {
            from = FindChild(path, from);
            if (from == 0) {
                return 0;
            }
        }
        while (*path != 0 && *path != '\\') {
            path++;
        }
        if (*path == '\\') {
            path++;
        }
    }
    return from;
}

AtDataLink* AtDataRegistry::FindLink(const char* name, AtDataLink* under, int exact) {
    if (under == 0) {
        under = &m_root;
    }
    if (under->m_name != 0 && (exact != 0 ? Equal(under->m_name, name) : EqualIgnoringCase(under->m_name, name))) {
        return under;
    }
    if (under->m_firstChild != 0) {
        AtDataLink* found = FindLink(name, under->m_firstChild, exact);
        if (found != 0) {
            return found;
        }
    }
    if (under->m_sibling != 0) {
        AtDataLink* found = FindLink(name, under->m_sibling, exact);
        if (found != 0) {
            return found;
        }
    }
    return 0;
}

AtDataLink* AtDataRegistry::FindInPools(const char* name, int exact) {
    AtDataLink* found = 0;
    for (unsigned int i = 0; i < kTypeCount; i++) {
        found = m_pools[i].FindByName(name, exact);
        if (found != 0) {
            break;
        }
    }
    return found;
}

AtDataComponent* AtDataRegistry::ValueNamed(const char* name) {
    for (unsigned int i = 0; i < kTypeCount; i++) {
        AtDataLink* found = m_pools[i].FindByName(name, 0);
        if (found != 0) {
            return found->m_value;
        }
    }
    return 0;
}

AtDataComponent* AtDataRegistry::ValueAt(AtDataLink* from, const char* path, int climb) {
    AtDataLink* found = FindPath(from, path, climb);
    return found != 0 ? found->m_value : 0;
}

AtDataLink* AtDataRegistry::FindFromRoot(const char* path, int climb) {
    return FindPath(0, path, climb);
}

AtDataComponent* AtDataRegistry::ValueAtPath(const char* path, int climb) {
    AtDataLink* found = FindPath(0, path, climb);
    return found != 0 ? found->m_value : 0;
}

// ---- a registry made as a copy of another -------------------------------------------------------------------------------

int AtDataDescriptor::CopyFrom(const AtDataDescriptor* other) {
    if (m_flags & kTyped) {
        Clear();
    }
    m_type = other->m_type;
    m_flags = other->m_flags;
    m_count = other->m_count;
    m_intFloor = other->m_intFloor;
    m_intCeiling = other->m_intCeiling;
    *(unsigned int*)&m_realFloor = *(const unsigned int*)&other->m_realFloor;
    *(unsigned int*)&m_realCeiling = *(const unsigned int*)&other->m_realCeiling;
    AtCopyName8(m_name, other->m_name);
    if (other->m_format != 0) {
        // Its length with no room for the terminator, as SetFormat: one byte over.
        PushHeap(s_heap);
        m_format = (char*)operator new(LengthOf(other->m_format));
        AtPopHeap();
        if (m_format == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        CopyText(m_format, other->m_format);
    }
    m_flags |= kTyped;
    return 0;
}

void AtDataComponentPool::CopyFrom(AtDataComponentPool* other, AtDataRegistry* registry) {
    m_count = 0;
    m_capacity = 4;
    m_type = 0;
    m_used = 0;
    m_values = 0;
    m_descriptors = 0;
    m_links = 0;
    m_next = 0;
    // The first block only: a chained block of the other's is not copied.
    m_count = other->m_count;
    m_capacity = other->m_capacity;
    m_type = other->m_type;
    Allocate();
    for (unsigned int i = 0; i < m_count; i++) {
        AtDataLink* from = &other->m_links[i];
        if (from->m_inUse == 0) {
            continue;
        }
        AtDataComponent* source = from->m_value;
        m_used++;
        AtDataComponent* value = ElementAt(i);
        // Every link hangs from the root here; Relink gives it its parent afterwards, without moving it.
        m_links[i].Set(from->m_name, value, &registry->m_root);
        AtDataDescriptor* descriptor = &m_descriptors[i];
        descriptor->CopyFrom(&other->m_descriptors[i]);
        switch (m_type) {
        case AtDataRegistry::kEmpty:
            value->SetDescriptor(descriptor);
            break;
        case AtDataRegistry::kBoolean:
            static_cast<AtDataBoolean*>(value)->Set(descriptor, static_cast<AtDataBoolean*>(source)->m_value);
            break;
        case AtDataRegistry::kInt:
            static_cast<AtDataInt*>(value)->Set(descriptor, static_cast<AtDataInt*>(source)->m_value);
            break;
        case AtDataRegistry::kReal:
            static_cast<AtDataReal*>(value)->Set(descriptor, static_cast<AtDataReal*>(source)->m_value);
            break;
        case AtDataRegistry::kShortName: {
            char name[8];
            AtCopyName8(name, static_cast<AtDataShortName*>(source)->m_value);
            static_cast<AtDataShortName*>(value)->Set(descriptor, name);
            break;
        }
        case AtDataRegistry::kString: {
            AtDataString* text = static_cast<AtDataString*>(source);
            static_cast<AtDataString*>(value)->CopyText(descriptor, text->m_unicode != 0 ? (AtText*)&text->m_wide : (AtText*)&text->m_narrow);
            break;
        }
        case AtDataRegistry::kBooleanArray:
            static_cast<AtDataBooleanArray*>(value)->Copy(descriptor, static_cast<AtDataBooleanArray*>(source));
            break;
        case AtDataRegistry::kByteArray:
            static_cast<AtDataByteArray*>(value)->Copy(descriptor, static_cast<AtDataByteArray*>(source));
            break;
        case AtDataRegistry::kIntArray:
            static_cast<AtDataIntArray*>(value)->Copy(descriptor, static_cast<AtDataIntArray*>(source));
            break;
        case AtDataRegistry::kRealArray:
            static_cast<AtDataRealArray*>(value)->Copy(descriptor, static_cast<AtDataRealArray*>(source));
            break;
        case AtDataRegistry::kShortNameArray:
            static_cast<AtDataShortNameArray*>(value)->Copy(descriptor, static_cast<AtDataShortNameArray*>(source));
            break;
        case AtDataRegistry::kStringArray:
            static_cast<AtDataStringArray*>(value)->Copy(descriptor, static_cast<AtDataStringArray*>(source));
            break;
        }
        value->m_registry = registry;
    }
}

void AtDataComponentPool::Relink(AtDataComponentPool* other, AtDataRegistry* registry) {
    for (unsigned int i = 0; i < m_count; i++) {
        AtDataLink* from = &other->m_links[i];
        if (from->m_inUse == 0 || from->m_parent == 0) {
            continue;
        }
        char path[0x100];
        path[0] = 0;
        from->m_parent->AppendPath(path, 0);
        if (path[0] == 0) {
            continue;
        }
        // Found by the whole path from this registry's root, where every copied link hangs: so only a parent one part deep is found.
        AtDataLink* parent = registry->FindPath(0, path, 0);
        if (parent != 0) {
            m_links[i].m_parent = parent;
        }
    }
}

int AtDataRegistry::CopyFrom(AtDataRegistry* other) {
    s_poolHeap = s_heap;
    if (m_loaded != 0) {
        Clear();
    }
    m_owner = other->m_owner;
    for (unsigned int i = 0; i < kTypeCount; i++) {
        m_pools[i].CopyFrom(&other->m_pools[i], this);
    }
    for (unsigned int i = 0; i < kTypeCount; i++) {
        m_pools[i].Relink(&other->m_pools[i], this);
    }
    m_loaded = 1;
    m_owner->Add(this, "");
    return 0;
}

// ---- start-up -----------------------------------------------------------------------------------------------------------

int AtDataRegistry::AttachUnder(AtDataRegistry* parent, const char* path) {
    AtDataLink* under = path != 0 ? parent->FindPath(0, path, 0) : &parent->m_root;
    if (under == 0) {
        return 0;
    }
    m_root.AttachTo(under);
    return 1;
}

int AtDataRegistry::Prepare(AtDataRegistryOwner* owner, unsigned int empties, unsigned int booleans, unsigned int ints, unsigned int reals, unsigned int shortNames, unsigned int strings,
                           unsigned int booleanArrays, unsigned int byteArrays, unsigned int intArrays, unsigned int realArrays, unsigned int shortNameArrays, unsigned int stringArrays,
                           AtDataRegistry* parent) {
    s_poolHeap = s_heap;
    if (m_loaded != 0) {
        Clear();
    }
    m_owner = owner;
    // Twelve counts: the thirteenth block, of no type, is left as it was.
    const unsigned int counts[kStringArray + 1] = {empties, booleans, ints, reals, shortNames, strings, booleanArrays, byteArrays, intArrays, realArrays, shortNameArrays, stringArrays};
    for (unsigned int i = 0; i <= kStringArray; i++) {
        AtDataComponentPool& pool = m_pools[i];
        pool.m_used = 0;
        pool.m_values = 0;
        pool.m_descriptors = 0;
        pool.m_links = 0;
        pool.m_next = 0;
        pool.m_type = i;
        pool.m_count = counts[i];
        pool.m_capacity = counts[i];
    }
    for (unsigned int i = 0; i < kTypeCount; i++) {
        m_pools[i].Allocate();
    }
    if (parent != 0) {
        m_parent = parent;
        m_root.AttachTo(&parent->m_root);
    }
    m_loaded = 1;
    owner->Add(this, "");
    return 0;
}

void AtDataRegistry::SetFileName(const char* name) {
    if (m_fileName != 0) {
        // Into the old block if it is long enough.
        if (LengthOf(name) <= LengthOf(m_fileName)) {
            CopyText(m_fileName, name);
            return;
        }
        operator delete(m_fileName);
    }
    PushHeap(s_heap);
    m_fileName = (char*)operator new(LengthOf(name) + 1);
    AtPopHeap();
    if (m_fileName == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    CopyText(m_fileName, name);
}
