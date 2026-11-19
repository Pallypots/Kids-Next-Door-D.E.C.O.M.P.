# The design data registry's mutants, .ADR. Each text occurs exactly once in its file. Dropped as equivalent, each with its reason:
#   * The store of a null into m_format right after freeing it in SetFormat: the next statement that can read it stores over it.
#   * The order of stores in a block of zeroes: nothing reads between them.
#   * The m_inUse check in pool New's link and the like reading m_count again: nothing changes it within the loop.
#   * In FindByName, Compare's direction: only equality is used.
#   * The ByteArray case in Clamp: it only reads. A mutant dropping the read is only seen by a crash on a value of another class, and that text is compared: see "clamp byte array: no read".
#   * Setting value again in WillAcceptParentData: link is only non-null after Data set value from it, so they agree. With link null the next statement but one crashes, and a registry past that is not compared.
#   * Load's store of m_loaded: Read has just done the same on every path that returns.
#   * Load's raise when the file name gets no memory: not a mutant here. The allocations ahead of it in the same call, for the stream and tokenizer, cannot be let through alone: see the worklog.
#   * AtDataStringArray::Reset's Release of each text: the block is freed two lines on with nothing reading it between.
#   * The word Flags's spelling: its only reader reports it as it would report a word not in the table, and takes the same path.
#   * m_parent in the link's Clear: Detach, just before it, has cleared it on every path or found it null.
#   * The pool Clear's calls to each descriptor's and link's Clear: the destructors, which run next, do the same, and the order of frees is not visible.
#   * The clear at the head of the database's Init: GmNameMap::Init, next, makes the same virtual call whenever there is a table, and there is one whenever there is a pool.
#   * NameMatches's three tests for a stop: each of the three, gone, leaves a character compare that returns 0 when the stop's result would be 0, and the other two tests take the cases where it would be 1.
#   * Clearing value per link in Read: a value is used across links only by a link with no Data, which crashes on its null link first.
#   * Starting under at null rather than the root: Set's argument turns null into the root.
#   * The null test around operator delete(m_values-like) blocks where delete of null logs nothing on either side: AtDataString's m_chars, name's free in Read, and the arrays' FreeData.
R = "AtDataRegistry.cpp"
C = "AtDataComponent.cpp"

WORDS = [
    ("kDataComponentTypes", "DataComponentTypes"), ("kDataComponent", "DataComponent"), ("kNumComponents", "NumComponents"), ("kDataRegistry", "DataRegistry"), ("kDataLink", "DataLink"), ("kName", "Name"),
    ("kType", "Type"), ("kParent", "Parent"), ("kFlags", "Flags"), ("kNumElements", "NumElements"), ("kFloor", "Floor"), ("kCeiling", "Ceiling"), ("kFormatString", "FormatString"), ("kData", "Data"),
    ("kBooleanWord", "Boolean"), ("kFirstType", "Empty"), ("kIntWord", "Int"), ("kRealWord", "Real"), ("kShortNameWord", "ShortName"), ("kStringWord", "String"), ("kBooleanArrayWord", "BooleanArray"),
    ("kByteArrayWord", "ByteArray"), ("kIntArrayWord", "IntArray"), ("kRealArrayWord", "RealArray"), ("kShortNameArrayWord", "ShortNameArray"), ("kStringArrayWord", "StringArray"), ("kNameSpace", "NameSpace"),
    ("kWillAcceptParentData", "WillAcceptParentData"),
]
M = [("word: " + w, R, '{%s, "%s"}' % (i, w), '{%s, "%sX"}' % (i, w)) for i, w in WORDS if w != "Flags"]
M += [
    ("word values: type order", R, "kFirstType = 0x47, kBooleanWord = 0x48", "kFirstType = 0x48, kBooleanWord = 0x47"),
    ("word count", R, "kWordCount = 0x1C", "kWordCount = 0x1B"),
    ("last type", R, "kLastTypeOffset = 0xB;", "kLastTypeOffset = 0xA;"),
    ("heap: no raise", R, "    if (heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n    AtPushHeap(heap);", "    AtPushHeap(heap);"),
    ("NewCopy: no terminator's room", R, "char* copy = (char*)operator new(LengthOf(text) + 1);", "char* copy = (char*)operator new(LengthOf(text));"),
    ("NewCopy: no pop", R, "char* copy = (char*)operator new(LengthOf(text) + 1);\n    AtPopHeap();", "char* copy = (char*)operator new(LengthOf(text) + 1);"),
    ("NewCopy: no raise", R, "    if (copy == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("clamp: ceiling", R, "    if (*value > ceiling) {\n        *value = ceiling;\n    }\n", ""),
    ("clamp: floor", R, "    if (*value < floor) {\n        *value = floor;\n    }\n", ""),
    ("clamp: ceiling's test", R, "if (*value > ceiling) {", "if (*value >= ceiling + ceiling) {"),
    ("clamp: floor's test", R, "if (*value < floor) {", "if (!(*value >= floor)) {"),
]


def fields(where, prefix, lines, suffix=""):
    """Each line of a block gone, the block being found by the text before it."""
    out = []
    block = "".join("%s\n" % l for l in lines)
    for l in lines:
        out.append(("%s: %s" % (where, l.strip()), R, prefix + block + suffix, prefix + block.replace("%s\n" % l, "", 1) + suffix))
    return out


D_FIELDS = ["    *(int*)&m_name[0] = 0;", "    *(int*)&m_name[4] = 0;", "    m_type = AtDataRegistry::kTypeCount;", "    m_flags = 0;", "    m_count = 1;", "    m_intFloor = (int)0x80000000;", "    m_intCeiling = 0x7FFFFFFF;",
            "    *(unsigned int*)&m_realFloor = 0xFF7FFFFF;", "    *(unsigned int*)&m_realCeiling = 0x7F7FFFFF;", "    m_format = 0;"]
M += fields("descriptor ctor", "AtDataDescriptor::AtDataDescriptor() {\n", D_FIELDS)
M += fields("descriptor clear", "void AtDataDescriptor::Clear() {\n    if (m_format != 0) {\n        operator delete(m_format);\n    }\n", D_FIELDS)
M += [
    ("descriptor clear: format kept", R, "void AtDataDescriptor::Clear() {\n    if (m_format != 0) {\n        operator delete(m_format);\n    }\n", "void AtDataDescriptor::Clear() {\n"),
    ("descriptor dtor: no clear", R, "AtDataDescriptor::~AtDataDescriptor() {\n    Clear();\n", "AtDataDescriptor::~AtDataDescriptor() {\n"),
    ("set type: no clear", R, "    if (m_flags & kTyped) {\n        Clear();\n    }\n", ""),
    ("set type: test", R, "if (m_flags & kTyped) {", "if (m_flags & kAcceptsParentData) {"),
    ("set type: type", R, "    m_type = type;\n", ""),
    ("set type: flag", R, "    m_flags |= kTyped;\n", ""),
    ("set type: flag replaces", R, "    m_flags |= kTyped;\n", "    m_flags = kTyped | 8;\n"),
    ("set type: count", R, "    m_count = count;\n    return 0;", "    return 0;"),
    ("int bounds: floor", R, "    m_intFloor = floor;\n", ""),
    ("int bounds: ceiling", R, "    m_intCeiling = ceiling;\n", ""),
    ("real bounds: real not allowed", R, "m_type != AtDataRegistry::kReal && m_type != AtDataRegistry::kRealArray", "m_type != AtDataRegistry::kRealArray"),
    ("real bounds: real array not allowed", R, "m_type != AtDataRegistry::kReal && m_type != AtDataRegistry::kRealArray", "m_type != AtDataRegistry::kReal"),
    ("real bounds: refusal's value", R, "        return -1;\n", "        return 0;\n"),
    ("real bounds: floor", R, "    m_realFloor = floor;\n", ""),
    ("real bounds: ceiling", R, "    m_realCeiling = ceiling;\n", ""),
    ("get bounds: floor", R, "    *floor = m_intFloor;\n", ""),
    ("get bounds: ceiling", R, "    *ceiling = m_intCeiling;\n", ""),
    ("descriptor heap", R, "void AtDataDescriptor::SetHeap(void* heap) {\n    s_heap = heap;", "void AtDataDescriptor::SetHeap(void* heap) {\n    (void)heap;"),
    ("format: old kept", R, "    if (m_format != 0) {\n        operator delete(m_format);\n        m_format = 0;\n    }\n    PushHeap(s_heap);", "    PushHeap(s_heap);"),
    ("format: one more", R, "m_format = (char*)operator new(LengthOf(format));", "m_format = (char*)operator new(LengthOf(format) + 1);"),
    ("format: no pop", R, "m_format = (char*)operator new(LengthOf(format));\n    AtPopHeap();", "m_format = (char*)operator new(LengthOf(format));"),
    ("format: no raise", R, "    if (m_format == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("format: text", R, "    CopyText(m_format, format);\n", "    m_format[0] = 0;\n"),
    # Link
    ("link heap", R, "void AtDataLink::SetHeap(void* heap) {\n    s_heap = heap;", "void AtDataLink::SetHeap(void* heap) {\n    (void)heap;"),
    ("link dtor: no clear", R, "AtDataLink::~AtDataLink() {\n    Clear();\n", "AtDataLink::~AtDataLink() {\n"),
    ("link registry: no test", R, "return m_value != 0 ? m_value->m_registry : 0;", "return m_value->m_registry;"),
    ("link registry: wrong", R, "return m_value != 0 ? m_value->m_registry : 0;", "return 0;"),
    ("attach: parent", R, "    m_parent = parent;\n    if (parent == 0) {\n        return;\n    }\n", "    if (parent == 0) {\n        return;\n    }\n"),
    ("attach: none", R, "    m_parent = parent;\n    if (parent == 0) {\n        return;\n    }\n", "    m_parent = parent;\n"),
    ("attach: first child", R, "        parent->m_firstChild = this;\n        return;\n", "        return;\n"),
    ("attach: first, then the walk", R, "        parent->m_firstChild = this;\n        return;\n", "        parent->m_firstChild = this;\n"),
    ("attach: at the front", R, "    last->m_sibling = this;\n", "    m_sibling = parent->m_firstChild;\n    parent->m_firstChild = this;\n"),
    ("attach: second, not last", R, "    while (last->m_sibling != 0) {\n        last = last->m_sibling;\n    }\n", ""),
    ("detach: no parent", R, "    AtDataLink* parent = m_parent;\n    if (parent == 0) {\n        return;\n    }\n", "    AtDataLink* parent = m_parent;\n"),
    ("detach: first: list", R, "        parent->m_firstChild = m_sibling;\n", ""),
    ("detach: first: sibling", R, "        parent->m_firstChild = m_sibling;\n        m_sibling = 0;\n", "        parent->m_firstChild = m_sibling;\n"),
    ("detach: first: parent", R, "        m_sibling = 0;\n        m_parent = 0;\n        return;\n", "        m_sibling = 0;\n        return;\n"),
    ("detach: other: list", R, "            before->m_sibling = m_sibling;\n", ""),
    ("detach: other: sibling", R, "            before->m_sibling = m_sibling;\n            m_sibling = 0;\n", "            before->m_sibling = m_sibling;\n"),
    ("detach: other: parent", R, "            break;\n        }\n    }\n    m_parent = 0;\n", "            break;\n        }\n    }\n"),
    ("link clear: detach", R, "void AtDataLink::Clear() {\n    Detach();\n", "void AtDataLink::Clear() {\n"),
    ("link clear: children", R, "    for (AtDataLink* child = m_firstChild; child != 0; child = child->m_sibling) {\n        child->m_parent = 0;\n    }\n", ""),
    ("link clear: only the first child", R, "for (AtDataLink* child = m_firstChild; child != 0; child = child->m_sibling) {", "for (AtDataLink* child = m_firstChild; child != 0; child = 0) {"),
    ("link clear: name", R, "    if (m_name != 0) {\n        operator delete(m_name);\n    }\n", ""),
]
LK = ["    m_value = 0;", "    m_name = 0;", "    m_parent = 0;", "    m_sibling = 0;", "    m_firstChild = 0;", "    m_inUse = 0;"]
M += [m for m in fields("link clear", "    if (m_name != 0) {\n        operator delete(m_name);\n    }\n", LK) if "m_parent" not in m[0]]
M += fields("link ctor", "AtDataLink::AtDataLink() {\n", LK)
M += [
    ("link set: clear", R, "    if (m_inUse != 0) {\n        Clear();\n    }\n", ""),
    ("link set: value", R, "    m_value = value;\n", ""),
    ("link set: name", R, "        m_name = NewCopy(s_heap, name);\n", "        m_name = 0;\n"),
    ("link set: no name", R, "    } else {\n        m_name = 0;\n    }\n    AttachTo(parent);", "    }\n    AttachTo(parent);"),
    ("link set: attach", R, "    AttachTo(parent);\n    m_inUse = 1;\n", "    m_inUse = 1;\n"),
    ("link set: in use", R, "    AttachTo(parent);\n    m_inUse = 1;\n", "    AttachTo(parent);\n"),
    ("link set: returns", R, "    m_inUse = 1;\n    return 0;", "    m_inUse = 1;\n    return 1;"),
    ("root registry", R, "    return m_registry;\n", "    return 0;\n"),
    # Pool
    ("element: limit", R, "while (index >= pool->m_count) {", "while (index > pool->m_count) {"),
    ("element: no chain", R, "        pool = pool->m_next;\n        if (pool == 0) {\n            return 0;\n        }\n    }\n    char* first", "        return 0;\n    }\n    char* first"),
    ("element: first of the block", R, "char* first = (char*)pool->m_values;", "char* first = (char*)m_values;"),
    ("element: no such type", R, "    case AtDataRegistry::kStringArray: return (AtDataComponent*)(first + index * sizeof(AtDataStringArray));\n    }\n    return 0;", "    case AtDataRegistry::kStringArray: return (AtDataComponent*)(first + index * sizeof(AtDataStringArray));\n    }\n    return (AtDataComponent*)first;"),
]
for k, cls, other in [("kEmpty", "AtDataComponent", "AtDataInt"), ("kBoolean", "AtDataBoolean", "AtDataShortName"), ("kInt", "AtDataInt", "AtDataShortName"), ("kReal", "AtDataReal", "AtDataShortName"), ("kShortName", "AtDataShortName", "AtDataInt"),
                      ("kString", "AtDataString", "AtDataInt"), ("kBooleanArray", "AtDataBooleanArray", "AtDataShortName"), ("kByteArray", "AtDataByteArray", "AtDataShortName"), ("kIntArray", "AtDataIntArray", "AtDataShortName"),
                      ("kRealArray", "AtDataRealArray", "AtDataShortName"), ("kShortNameArray", "AtDataShortNameArray", "AtDataShortName"), ("kStringArray", "AtDataStringArray", "AtDataInt")]:
    M.append(("element: size of " + cls, R, "case AtDataRegistry::%s: return (AtDataComponent*)(first + index * sizeof(%s));" % (k, cls), "case AtDataRegistry::%s: return (AtDataComponent*)(first + index * sizeof(%s));" % (k, other)))
    M.append(("allocate: class " + cls, R, "case AtDataRegistry::%s: m_values = NewValues<%s>(m_count); break;" % (k, cls), "case AtDataRegistry::%s: m_values = NewValues<%s>(m_count); break;" % (k, "AtDataBoolean" if cls != "AtDataBoolean" else "AtDataInt")))
GROW = ["    m_next->m_used = 0;", "    m_next->m_values = 0;", "    m_next->m_descriptors = 0;", "    m_next->m_links = 0;", "    m_next->m_next = 0;", "    m_next->m_count = m_capacity;", "    m_next->m_capacity = m_capacity;", "    m_next->m_type = m_type;"]
M += fields("grow", "    if (m_next == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", GROW)
M += [
    ("grow: no raise", R, "    AtPopHeap();\n    if (m_next == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", "    AtPopHeap();\n"),
    ("grow: no pop", R, "m_next = (AtDataComponentPool*)operator new(sizeof(AtDataComponentPool));\n    AtPopHeap();", "m_next = (AtDataComponentPool*)operator new(sizeof(AtDataComponentPool));"),
    ("allocate: no count", R, "if (m_count == 0 || m_values != 0) {", "if (m_values != 0) {"),
    ("allocate: already", R, "if (m_count == 0 || m_values != 0) {", "if (m_count == 0) {"),
    ("allocate: no class limit", R, "if (m_type <= AtDataRegistry::kStringArray) {", "if (m_type <= AtDataRegistry::kStringArray + 1) {"),
    ("allocate: values' raise", R, "        if (m_values == 0) {\n            AtRaise(-4, 0, 0, 0);\n        }\n", ""),
    ("allocate: values' pop", R, "        }\n        AtPopHeap();\n        if (m_values == 0) {", "        }\n        if (m_values == 0) {"),
    ("allocate: descriptors' raise", R, "    if (m_descriptors == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("allocate: descriptor's type", R, "m_descriptors[i].SetType(m_type, 1);", "m_descriptors[i].SetType(m_type, 2);"),
    ("allocate: descriptor not typed", R, "        m_descriptors[i].SetType(m_type, 1);\n", ""),
    ("pool clear: no count", R, "void AtDataComponentPool::Clear() {\n    if (m_count == 0) {\n        return;\n    }\n", "void AtDataComponentPool::Clear() {\n"),
    ("pool clear: reset", R, "        ElementAt(i)->Reset();\n", ""),
    ("pool clear: no descriptors freed", R, "        AtDeleteArray(m_descriptors);\n", ""),
    ("pool clear: no links freed", R, "        AtDeleteArray(m_links);\n", ""),
    ("pool clear: chain", R, "        m_next->Clear();\n", ""),
    ("pool clear: chain freed", R, "        operator delete(m_next);\n", ""),
    ("pool clear: values kept", R, "    if (m_values != 0) {\n        switch (m_type) {\n        case AtDataRegistry::kEmpty: DeleteValues", "    if (m_values == (AtDataComponent*)1) {\n        switch (m_type) {\n        case AtDataRegistry::kEmpty: DeleteValues"),
]
for k, cls in [("kEmpty", "AtDataComponent"), ("kBoolean", "AtDataBoolean"), ("kInt", "AtDataInt"), ("kReal", "AtDataReal"), ("kShortName", "AtDataShortName"), ("kString", "AtDataString"), ("kBooleanArray", "AtDataBooleanArray"), ("kByteArray", "AtDataByteArray"),
               ("kIntArray", "AtDataIntArray"), ("kRealArray", "AtDataRealArray"), ("kShortNameArray", "AtDataShortNameArray"), ("kStringArray", "AtDataStringArray")]:
    M.append(("pool clear: not freed: " + cls, R, "        case AtDataRegistry::%s: DeleteValues<%s>(m_values); break;\n" % (k, cls), ""))
PC = ["    m_count = 0;", "    m_capacity = 4;", "    m_type = 0;", "    m_used = 0;", "    m_values = 0;", "    m_descriptors = 0;", "    m_links = 0;", "    m_next = 0;"]
M += fields("pool clear", "        operator delete(m_next);\n    }\n", PC)
M += [
    ("by name: no count", R, "while (pool->m_count != 0 && pool->m_values != 0) {", "while (pool->m_values != 0) {"),
    ("by name: no values", R, "while (pool->m_count != 0 && pool->m_values != 0) {", "while (pool->m_count != 0) {"),
    ("by name: unused", R, "if (link->m_inUse != 0 && link->m_name != 0 && link->m_name[0] != 0) {", "if (link->m_name != 0 && link->m_name[0] != 0) {"),
    ("by name: null name", R, "if (link->m_inUse != 0 && link->m_name != 0 && link->m_name[0] != 0) {", "if (link->m_inUse != 0 && link->m_name[0] != 0) {"),
    ("by name: empty name", R, "if (link->m_inUse != 0 && link->m_name != 0 && link->m_name[0] != 0) {", "if (link->m_inUse != 0 && link->m_name != 0) {"),
    ("by name: exact ignored", R, "(exact != 0 ? Compare(name, link->m_name) : CompareIgnoringCase(name, link->m_name))", "CompareIgnoringCase(name, link->m_name)"),
    ("by name: always exact", R, "(exact != 0 ? Compare(name, link->m_name) : CompareIgnoringCase(name, link->m_name))", "Compare(name, link->m_name)"),
    ("by name: no chain", R, "        pool = pool->m_next;\n        if (pool == 0) {\n            return 0;\n        }\n        exact = 0;\n", "        return 0;\n"),
    ("by name: exact carries on", R, "        exact = 0;\n", ""),
    ("new: empty block", R, "        if (pool->m_count == 0) {\n            pool->m_used = 0;", "        if (pool->m_count == 99) {\n            pool->m_used = 0;"),
    ("new: of four", R, "            pool->m_count = 4;\n", "            pool->m_count = 3;\n"),
    ("new: capacity four", R, "            pool->m_capacity = 4;\n", "            pool->m_capacity = 3;\n"),
    ("new: allocate", R, "        if (pool->m_values == 0) {\n            pool->Allocate();\n        }\n", ""),
    ("new: first free", R, "if (pool->m_links[index].m_inUse == 0) {", "if (pool->m_links[index].m_inUse != 0) {"),
    ("new: no grow", R, "        if (pool->m_next == 0) {\n            pool->Grow();\n        }\n", ""),
    ("new: used", R, "    pool->m_used++;\n", ""),
    ("new: link set", R, "    link->Set(0, value, 0);\n", "    link->m_inUse = 1;\n"),
    ("new: descriptor", R, "    value->SetDescriptor(&pool->m_descriptors[index]);\n", ""),
    ("new: which link", R, "AtDataLink* link = &pool->m_links[index];", "AtDataLink* link = &m_links[index];"),
    # Groups
    ("group reset: name", R, "void AtDataRegistryGroup::Reset() {\n    m_name[0] = 0;\n", "void AtDataRegistryGroup::Reset() {\n"),
    ("group reset: first", R, "void AtDataRegistryGroup::Reset() {\n    m_name[0] = 0;\n    m_first = 0;\n", "void AtDataRegistryGroup::Reset() {\n    m_name[0] = 0;\n"),
    ("group reset: next", R, "    m_first = 0;\n    m_nextFree = 0;\n    m_database = 0;\n}\n\nvoid AtDataRegistryGroup::SetName", "    m_first = 0;\n    m_database = 0;\n}\n\nvoid AtDataRegistryGroup::SetName"),
    ("group reset: database", R, "    m_nextFree = 0;\n    m_database = 0;\n}\n\nvoid AtDataRegistryGroup::SetName", "    m_nextFree = 0;\n}\n\nvoid AtDataRegistryGroup::SetName"),
    ("group ctor: name", R, "AtDataRegistryGroup::AtDataRegistryGroup() {\n    m_name[0] = 0;\n", "AtDataRegistryGroup::AtDataRegistryGroup() {\n"),
    ("group ctor: first", R, "AtDataRegistryGroup::AtDataRegistryGroup() {\n    m_name[0] = 0;\n    m_first = 0;\n", "AtDataRegistryGroup::AtDataRegistryGroup() {\n    m_name[0] = 0;\n"),
    ("group ctor: database", R, "    m_nextFree = 0;\n    m_database = 0;\n}\n\nvoid AtDataRegistryGroup::Reset", "    m_nextFree = 0;\n}\n\nvoid AtDataRegistryGroup::Reset"),
    ("group name", R, "    AtCopyName8(m_name, name);\n", ""),
    ("group add: leaves", R, "    if (registry->m_group != 0) {\n        registry->m_group->Remove(registry);\n    }\n    registry->m_next = m_first;", "    registry->m_next = m_first;"),
    ("group add: next", R, "    registry->m_next = m_first;\n", ""),
    ("group add: first", R, "    m_first = registry;\n", ""),
    ("group add: group", R, "    registry->m_group = this;\n", ""),
    ("group remove: not found", R, "    if (*link == 0) {\n        return;\n    }\n", ""),
    ("group remove: unlink", R, "    *link = registry->m_next;\n", ""),
    ("group remove: group", R, "    *link = registry->m_next;\n    registry->m_group = 0;\n", "    *link = registry->m_next;\n"),
    ("group remove: release", R, "    if (m_first == 0 && m_database != 0) {\n        m_database->Release(this);\n    }\n", ""),
    ("group remove: release when not empty", R, "if (m_first == 0 && m_database != 0) {", "if (m_database != 0) {"),
    ("group remove: no database", R, "if (m_first == 0 && m_database != 0) {", "if (m_first == 0) {"),
    ("spaces heap", R, "void AtDataRegistryNameSpaces::SetHeap(void* heap) {\n    s_heap = heap;", "void AtDataRegistryNameSpaces::SetHeap(void* heap) {\n    (void)heap;"),
]
SP = ["    m_pool = 0;", "    m_poolSize = 0;", "    m_free = 0;", "    m_last = 0;"]
M += fields("spaces ctor", "AtDataRegistryNameSpaces::AtDataRegistryNameSpaces() {\n", SP)
M += fields("spaces dtor", "AtDataRegistryNameSpaces::~AtDataRegistryNameSpaces() {\n    if (m_pool != 0) {\n        operator delete(m_pool);\n    }\n    GmNameMap::Clear();\n", SP)
M += fields("spaces clear", "void AtDataRegistryNameSpaces::Clear() {\n    if (m_pool != 0) {\n        operator delete(m_pool);\n    }\n    GmNameMap::Clear();\n", SP)
M += [
    ("spaces dtor: pool", R, "AtDataRegistryNameSpaces::~AtDataRegistryNameSpaces() {\n    if (m_pool != 0) {\n        operator delete(m_pool);\n    }\n", "AtDataRegistryNameSpaces::~AtDataRegistryNameSpaces() {\n"),
    ("spaces clear: pool", R, "void AtDataRegistryNameSpaces::Clear() {\n    if (m_pool != 0) {\n        operator delete(m_pool);\n    }\n", "void AtDataRegistryNameSpaces::Clear() {\n"),
    ("spaces clear: table", R, "void AtDataRegistryNameSpaces::Clear() {\n    if (m_pool != 0) {\n        operator delete(m_pool);\n    }\n    GmNameMap::Clear();\n", "void AtDataRegistryNameSpaces::Clear() {\n    if (m_pool != 0) {\n        operator delete(m_pool);\n    }\n"),
    ("spaces init: table", R, "    GmNameMap::Init(count);\n", ""),
    ("spaces init: size", R, "    m_poolSize = count;\n", "    m_poolSize = count + 1;\n"),
    ("spaces init: raise", R, "    if (m_pool == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("spaces init: no pop", R, "    m_pool = pool;\n    AtPopHeap();\n", "    m_pool = pool;\n"),
    ("spaces init: free", R, "    m_free = m_pool;\n", ""),
    ("spaces init: chain", R, "        group->m_nextFree = group + 1;\n", ""),
    ("spaces init: end", R, "    group->m_nextFree = 0;\n", "    group->m_nextFree = group;\n"),
    ("spaces release: map", R, "    GmNameMap::Remove(group->m_name);\n", ""),
    ("spaces release: reset", R, "    group->Reset();\n", ""),
    ("spaces release: free", R, "    group->m_nextFree = m_free;\n", ""),
    ("spaces release: head", R, "    m_free = group;\n", ""),
    ("spaces add: reported", R, '            AtFatal("Atlas Base: Name space database overflow.", 0, 0, 0);\n', ""),
    ("spaces add: message", R, "Atlas Base: Name space database overflow.", "Atlas Base: overflow."),
    ("spaces add: free", R, "        m_free = group->m_nextFree;\n", ""),
    ("spaces add: map", R, "        Insert(space, group);\n", ""),
    ("spaces add: name", R, "        group->SetName(space);\n", ""),
    ("spaces add: database", R, "        group->m_database = this;\n", ""),
    ("spaces add: last", R, "    m_last = group;\n", ""),
    ("spaces add: to group", R, "    group->Add(registry);\n", ""),
    ("spaces remove: test", R, "void AtDataRegistryNameSpaces::Remove(AtDataRegistry* registry) {\n    if (registry->m_group != 0) {\n        registry->m_group->Remove(registry);\n    }\n", "void AtDataRegistryNameSpaces::Remove(AtDataRegistry* registry) {\n    registry->m_group->Remove(registry);\n"),
    ("spaces remove: none", R, "void AtDataRegistryNameSpaces::Remove(AtDataRegistry* registry) {\n    if (registry->m_group != 0) {\n        registry->m_group->Remove(registry);\n    }\n", "void AtDataRegistryNameSpaces::Remove(AtDataRegistry* registry) {\n"),
    ("owner add: null", R, "if (space != 0 && space[0] != 0) {", "if (space[0] != 0) {"),
    ("owner add: empty", R, "if (space != 0 && space[0] != 0) {", "if (space != 0) {"),
    ("owner remove: no group", R, "if (registry->m_group != 0 && registry->m_group->m_name[0] != 0) {", "if (registry->m_group->m_name[0] != 0) {"),
    ("owner remove: plain", R, "        AtDataRegistryNameSpaces::Remove(registry);\n    } else {\n        m_plain.Remove(registry);\n", "        AtDataRegistryNameSpaces::Remove(registry);\n"),
    ("owner remove: named", R, "    if (space[0] != 0) {\n        AtDataRegistryNameSpaces::Remove(registry);\n    } else {\n", "    if (space[0] != 0) {\n        m_plain.Remove(registry);\n    } else {\n"),
    # Registry
    ("registry heap", R, "void AtDataRegistry::SetHeap(void* heap) {\n    s_heap = heap;", "void AtDataRegistry::SetHeap(void* heap) {\n    (void)heap;"),
    ("registry ctor", R, "AtDataRegistry::AtDataRegistry() {\n    Reset();\n", "AtDataRegistry::AtDataRegistry() {\n"),
    ("registry dtor: clear", R, "    if (m_loaded != 0) {\n        Clear();\n    }\n}\n\nvoid AtDataRegistry::Reset", "}\n\nvoid AtDataRegistry::Reset"),
    ("registry dtor: clear when not loaded", R, "AtDataRegistry::~AtDataRegistry() {\n    if (m_loaded != 0) {\n", "AtDataRegistry::~AtDataRegistry() {\n    {\n"),
    ("reset: owner", R, "void AtDataRegistry::Reset() {\n    m_owner = 0;\n", "void AtDataRegistry::Reset() {\n"),
    ("reset: parent", R, "    m_owner = 0;\n    m_parent = 0;\n", "    m_owner = 0;\n"),
    ("reset: root", R, "    m_root.Set(0, 0, 0);\n", ""),
    ("reset: root's registry", R, "    m_root.m_registry = this;\n", ""),
    ("reset: +38", R, "    m_root.m_registry = this;\n    m_unknown38 = 0;\n", "    m_root.m_registry = this;\n"),
    ("reset: pools' last", R, "for (unsigned int i = 0; i < kTypeCount; i++) {\n        AtDataComponentPool& pool = m_pools[i];", "for (unsigned int i = 0; i < kTypeCount - 1; i++) {\n        AtDataComponentPool& pool = m_pools[i];"),
    ("reset: loaded", R, "    m_loaded = 0;\n", ""),
    ("reset: group", R, "    m_group = 0;\n", ""),
    ("reset: file", R, "    m_fileName = 0;\n", ""),
]
RP = ["        pool.m_count = 0;", "        pool.m_capacity = 0;", "        pool.m_type = i;", "        pool.m_used = 0;", "        pool.m_values = 0;", "        pool.m_descriptors = 0;", "        pool.m_links = 0;", "        pool.m_next = 0;"]
M += fields("reset pool", "        AtDataComponentPool& pool = m_pools[i];\n", RP)
M += [
    ("clear: +38 free", R, "    if (m_unknown38 != 0) {\n        operator delete(m_unknown38);\n        m_unknown38 = 0;\n    }\n", "    if (m_unknown38 != 0) {\n        m_unknown38 = 0;\n    }\n"),
    ("clear: +38 kept", R, "        operator delete(m_unknown38);\n        m_unknown38 = 0;\n", "        operator delete(m_unknown38);\n"),
    ("clear: loaded", R, "    if (m_loaded == 0) {\n        return;\n    }\n", ""),
    ("clear: no owner", R, "    if (m_owner != 0) {\n        m_owner->Remove(this);\n    }\n", "    m_owner->Remove(this);\n"),
    ("clear: owner", R, "    if (m_owner != 0) {\n        m_owner->Remove(this);\n    }\n", ""),
    ("clear: pools", R, "        m_pools[i].Clear();\n", ""),
    ("clear: pools' last", R, "    for (unsigned int i = 0; i < kTypeCount; i++) {\n        m_pools[i].Clear();\n", "    for (unsigned int i = 0; i + 2 < kTypeCount; i++) {\n        m_pools[i].Clear();\n"),
    ("clear: file", R, "    if (m_fileName != 0) {\n        operator delete(m_fileName);\n    }\n", ""),
    ("clear: reset", R, "        operator delete(m_fileName);\n    }\n    Reset();\n", "        operator delete(m_fileName);\n    }\n"),
    ("match: name goes on", R, "if (*name != 0 && *name != '\\\\') {", "if (0) {"),
    ("match: name's slash", R, "if (*name != 0 && *name != '\\\\') {", "if (*name != 0) {"),
    ("match: differs", R, "        if (*name != *path) {\n            return 0;\n        }\n", ""),
    ("match: result", R, "return *path == 0 || *path == '\\\\';", "return *path == 0;"),
    ("match: result, slash", R, "return *path == 0 || *path == '\\\\';", "return *path == '\\\\';"),
    ("child: under", R, "under != 0 ? under->m_firstChild : m_root.m_firstChild", "m_root.m_firstChild"),
    ("child: no under", R, "under != 0 ? under->m_firstChild : m_root.m_firstChild", "under->m_firstChild"),
    ("child: into nodes without a value", R, "        if (link->m_value == 0) {\n            AtDataLink* found = FindChild(path, link);\n            if (found != 0) {\n                return found;\n            }\n        } else if", "        if"),
    ("child: not returned", R, "            if (found != 0) {\n                return found;\n            }\n", ""),
    ("clamp: boolean", R, "        ClampToBounds(v, 0, 1);\n", ""),
    ("clamp: int's floor", R, "ClampToBounds(v, value->m_descriptor->m_intFloor, value->m_descriptor->m_intCeiling);", "ClampToBounds(v, (int)0x80000000, value->m_descriptor->m_intCeiling);"),
    ("clamp: int's ceiling", R, "ClampToBounds(v, value->m_descriptor->m_intFloor, value->m_descriptor->m_intCeiling);", "ClampToBounds(v, value->m_descriptor->m_intFloor, 0x7FFFFFFF);"),
    ("clamp: real's floor", R, "ClampToBounds(v, value->m_descriptor->m_realFloor, value->m_descriptor->m_realCeiling);", "ClampToBounds(v, -1e30f, value->m_descriptor->m_realCeiling);"),
    ("clamp: real's ceiling", R, "ClampToBounds(v, value->m_descriptor->m_realFloor, value->m_descriptor->m_realCeiling);", "ClampToBounds(v, value->m_descriptor->m_realFloor, 1e30f);"),
    ("clamp: boolean array", R, "            ClampToBounds(&data[i], 0, 1);\n", ""),
    ("clamp: int array", R, "            ClampToBounds(&data[i], value->m_descriptor->m_intFloor, value->m_descriptor->m_intCeiling);\n", ""),
    ("clamp: real array", R, "            ClampToBounds(&data[i], value->m_descriptor->m_realFloor, value->m_descriptor->m_realCeiling);\n", ""),
    ("clamp byte array: no read", R, "            (void)data[i];\n", ""),
    ("clamp: string clamps as an int", R, "    case kInt: {\n", "    case kString:\n    case kInt: {\n"),
    ("clamp: short name clamps as an int", R, "    case kBoolean: {\n", "    case kShortName:\n    case kBoolean: {\n"),
    ("clamp: name array clamps as an int", R, "    case kIntArray: {\n", "    case kShortNameArray:\n    case kIntArray: {\n"),
    # The reader
    ("read: owner", R, "    m_owner = owner;\n", ""),
    ("read: out", R, "    if (spaceOut != 0) {\n        spaceOut[0] = 0;\n    }\n", ""),
    ("read: out null", R, "    if (spaceOut != 0) {\n        spaceOut[0] = 0;\n    }\n", "    spaceOut[0] = 0;\n"),
    ("read: parent", R, "    m_parent = parent;\n    if (parent != 0) {", "    if (parent != 0) {"),
    ("read: under the parent", R, "    if (parent != 0) {\n        m_root.AttachTo(&parent->m_root);\n    }\n", ""),
    ("read: over what is there", R, "    if (m_loaded != 0) {\n        Clear();\n    }\n    s_poolHeap", "    s_poolHeap"),
    ("read: pool heap", R, "    s_poolHeap = s_heap;\n", ""),
    ("read: words", R, "    tokenizer->SetKeywords(s_words, kWordCount, 0);\n", ""),
    ("read: space's first byte", R, "    int word = tokenizer->Next();\n    char space[8];\n    space[0] = 0;\n", "    int word = tokenizer->Next();\n    char space[8];\n    space[0] = 1;\n    space[1] = 0;\n"),
    ("read: space's name", R, "        tokenizer->ExpectName(space);\n", "        tokenizer->ExpectString();\n"),
    ("read: space out", R, "        if (spaceOut != 0) {\n            AtCopyName8(spaceOut, space);\n        }\n", ""),
    ("read: space out null", R, "        if (spaceOut != 0) {\n            AtCopyName8(spaceOut, space);\n        }\n", "        AtCopyName8(spaceOut, space);\n"),
    ("read: word after the space", R, "            AtCopyName8(spaceOut, space);\n        }\n        word = tokenizer->Next();\n", "            AtCopyName8(spaceOut, space);\n        }\n"),
    ("read: no first word's error", R, "    if (word != kDataComponentTypes) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n    }\n", ""),
    ("read: first word's error", R, "    if (word != kDataComponentTypes) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);", "    if (word != kDataComponentTypes) {\n        tokenizer->Error(AtIoTokenizer::kSyntaxError);"),
    ("read: types' count", R, "    m_componentTypes = tokenizer->ExpectCount();\n", "    m_componentTypes = 0;\n"),
    ("read: component's error", R, "        if (tokenizer->Next() != kDataComponent) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        }\n", "        tokenizer->Next();\n"),
    ("read: component's curly", R, "        tokenizer->ExpectLeftCurly();\n", ""),
    ("read: Type's error", R, "        if (tokenizer->Next() != kType) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        }\n", "        tokenizer->Next();\n"),
    ("read: NumComponents' error", R, "        if (tokenizer->Next() != kNumComponents) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        }\n", "        tokenizer->Next();\n"),
    ("read: count", R, "        const int count = tokenizer->ExpectInt();\n", "        tokenizer->ExpectInt();\n        const int count = 1;\n"),
    ("read: any type", R, "        if (typeOffset <= kLastTypeOffset) {\n            AtDataComponentPool& pool", "        if (typeOffset < kTypeCount) {\n            AtDataComponentPool& pool"),
]
CP = ["            pool.m_used = 0;", "            pool.m_values = 0;", "            pool.m_descriptors = 0;", "            pool.m_links = 0;", "            pool.m_next = 0;", "            pool.m_count = count;", "            pool.m_capacity = count;", "            pool.m_type = typeOffset;"]
M += fields("read pool", "            AtDataComponentPool& pool = m_pools[typeOffset];\n", CP)
M += [
    ("read: component's end", R, "            pool.m_type = typeOffset;\n        }\n        tokenizer->ExpectRightCurly();\n", "            pool.m_type = typeOffset;\n        }\n"),
    ("read: list's end", R, "    }\n    tokenizer->ExpectRightCurly();\n    for (unsigned int i = 0; i < kTypeCount; i++) {\n        m_pools[i].Allocate();\n", "    }\n    for (unsigned int i = 0; i < kTypeCount; i++) {\n        m_pools[i].Allocate();\n"),
    ("read: allocate", R, "        m_pools[i].Allocate();\n", ""),
    ("read: the first type", R, "    unsigned int type = kTypeCount;\n", "    unsigned int type = kInt;\n"),
    ("read: registry's word", R, "    if (tokenizer->Next() != kDataRegistry) {\n        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n    }\n", "    tokenizer->Next();\n"),
    ("read: links' count", R, "    const unsigned int links = tokenizer->ExpectCount();\n", "    tokenizer->ExpectCount();\n    const unsigned int links = 0;\n"),
    ("read: type carries", R, "        AtDataLink* link = 0;\n        AtDataLink* under = &m_root;\n", "        AtDataLink* link = 0;\n        type = kTypeCount;\n        AtDataLink* under = &m_root;\n"),
    ("read: int floor", R, "        int intFloor = (int)0x80000000;\n", "        int intFloor = 0;\n"),
    ("read: int ceiling", R, "        int intCeiling = 0x7FFFFFFF;\n", "        int intCeiling = 100;\n"),
    ("read: real floor", R, "        *(unsigned int*)&realFloor = 0xFF7FFFFF;\n", "        *(unsigned int*)&realFloor = 0;\n"),
    ("read: real ceiling", R, "        *(unsigned int*)&realCeiling = 0x7F7FFFFF;\n", "        *(unsigned int*)&realCeiling = 0x3F800000;\n"),
    ("read: elements", R, "        int elements = 1;\n", "        int elements = 2;\n"),
    ("read: link's word", R, "        if (tokenizer->Next() != kDataLink) {\n            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n        }\n", "        tokenizer->Next();\n"),
    ("read: skips to the curly", R, "        while (tokenizer->Next() != AtIoTokenizer::kLeftCurly) {\n        }\n", "        tokenizer->Next();\n"),
    ("read: Name", R, "                name = NewCopy(AtIoTokenFile::s_heap, tokenizer->ExpectString());\n", "                tokenizer->ExpectString();\n"),
    ("read: Name's heap", R, "                name = NewCopy(AtIoTokenFile::s_heap, tokenizer->ExpectString());\n", "                name = NewCopy(s_heap, tokenizer->ExpectString());\n"),
    ("read: Type's limit", R, "                if (typeOffset > kLastTypeOffset) {\n", "                if (typeOffset > kTypeCount) {\n"),
    ("read: a link's Type's error", R, "                if (typeOffset > kLastTypeOffset) {\n                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);", "                if (typeOffset > kLastTypeOffset) {\n                    tokenizer->Error(AtIoTokenizer::kSyntaxError);"),
    ("read: Type stored", R, "                    type = typeOffset;\n", ""),
    ("read: Parent: within", R, "AtDataRegistry* within = parent != 0 ? parent : this;", "AtDataRegistry* within = this;"),
    ("read: Parent: always the parent", R, "AtDataRegistry* within = parent != 0 ? parent : this;", "AtDataRegistry* within = parent;"),
    ("read: Parent: down from the last found", R, "found = within->FindChild(part, found);", "found = within->FindChild(part, 0);"),
    ("read: Parent: stops at a failure", R, "                        if (found == 0) {\n                            break;\n                        }\n", ""),
    ("read: Parent: to the slash", R, "                        while (*part != 0 && *part != '\\\\') {\n                            part++;\n                        }\n", ""),
    ("read: Parent: past the slash", R, "                        if (*part == '\\\\') {\n                            part++;\n                        }\n", ""),
    ("read: Parent: empty path", R, "                    while (*part != 0) {\n                        found", "                    while (true) {\n                        found"),
    ("read: Parent: whole name", R, "                if (found == 0) {\n                    for (unsigned int p = 0;", "                if (found == (AtDataLink*)1) {\n                    for (unsigned int p = 0;"),
    ("read: Parent: all pools", R, "p < kTypeCount && found == 0; p++", "p + 1 < kStringArray && found == 0; p++"),
    ("read: Parent: stops at the first found", R, "p < kTypeCount && found == 0; p++", "p < kTypeCount; p++"),
    ("read: Parent: case", R, "found = m_pools[p].FindByName(path, 0);", "found = m_pools[p].FindByName(path, 1);"),
    ("read: Parent: stored", R, "                under = found;\n", ""),
    ("read: NumElements", R, "                elements = tokenizer->ExpectInt();\n", "                tokenizer->ExpectInt();\n"),
    ("read: Floor: real", R, "                    realFloor = tokenizer->ExpectReal();\n", "                    tokenizer->ExpectReal();\n"),
    ("read: Floor: int", R, "                    intFloor = tokenizer->ExpectInt();\n", "                    tokenizer->ExpectInt();\n"),
    ("read: Ceiling: real", R, "                    realCeiling = tokenizer->ExpectReal();\n", "                    tokenizer->ExpectReal();\n"),
    ("read: Ceiling: int", R, "                    intCeiling = tokenizer->ExpectInt();\n", "                    tokenizer->ExpectInt();\n"),
    ("read: Floor: real array", R, "            case kFloor:\n                if (type == kReal || type == kRealArray) {", "            case kFloor:\n                if (type == kReal) {"),
    ("read: Floor: real kind", R, "            case kFloor:\n                if (type == kReal || type == kRealArray) {", "            case kFloor:\n                if (type == kRealArray) {"),
    ("read: Ceiling: real array", R, "            case kCeiling:\n                if (type == kReal || type == kRealArray) {", "            case kCeiling:\n                if (type == kReal) {"),
    ("read: Ceiling: real kind", R, "            case kCeiling:\n                if (type == kReal || type == kRealArray) {", "            case kCeiling:\n                if (type == kRealArray) {"),
    ("read: FormatString", R, "                format = NewCopy(AtIoTokenFile::s_heap, tokenizer->ExpectString());\n", "                tokenizer->ExpectString();\n"),
    ("read: Data: value", R, "                value = link->m_value;\n                AtDataDescriptor* descriptor", "                AtDataDescriptor* descriptor"),
    ("read: Data: type, count", R, "                descriptor->SetType(type, elements);\n", ""),
    ("read: Data: real bounds", R, "                    descriptor->SetRealBounds(realFloor, realCeiling);\n", ""),
    ("read: Data: int bounds", R, "                    descriptor->SetIntBounds(intFloor, intCeiling);\n", ""),
    ("read: Data: which", R, "                if (type == kReal || type == kRealArray) {\n                    descriptor->SetRealBounds", "                if (type == kReal) {\n                    descriptor->SetRealBounds"),
    ("read: Data: which, real", R, "                if (type == kReal || type == kRealArray) {\n                    descriptor->SetRealBounds", "                if (type == kRealArray) {\n                    descriptor->SetRealBounds"),
    ("read: Data: read", R, "                value->Read(descriptor, tokenizer);\n", ""),
    ("read: Data: registry", R, "                value->m_registry = this;\n", ""),
    ("read: Accepts: flag", R, "                value->m_descriptor->m_flags |= AtDataDescriptor::kAcceptsParentData;\n", ""),
    ("read: Accepts: flag replaces", R, "value->m_descriptor->m_flags |= AtDataDescriptor::kAcceptsParentData;", "value->m_descriptor->m_flags = AtDataDescriptor::kAcceptsParentData;"),
    ("read: unknown word", R, "            default:\n                tokenizer->Error(AtIoTokenizer::kExpectedKeyword);\n", "            default:\n"),
    ("read: Flags is taken", R, "            case kNumElements:\n", "            case kFlags:\n            case kNumElements:\n"),
    ("read: link's set", R, "        link->Set(name, value, under != 0 ? under : &m_root);\n", ""),
    ("read: link's parent", R, "under != 0 ? under : &m_root", "under"),
    ("read: name freed", R, "        operator delete(name);\n", ""),
    ("read: format", R, "        if (format != 0) {\n            link->m_value->m_descriptor->SetFormat(format);\n", "        if (format != 0) {\n"),
    ("read: format freed", R, "            operator delete(format);\n", ""),
    ("read: clamp", R, "        Clamp(type, value);\n", ""),
    ("read: registry's end", R, "        Clamp(type, value);\n    }\n    tokenizer->ExpectRightCurly();\n", "        Clamp(type, value);\n    }\n"),
    ("read: loaded", R, "    tokenizer->ExpectRightCurly();\n    m_loaded = 1;\n", "    tokenizer->ExpectRightCurly();\n"),
    ("read: filed", R, "    owner->Add(this, space);\n", ""),
    ("read: filed plainly", R, "    owner->Add(this, space);\n", "    owner->Add(this, 0);\n"),
    ("read: returns", R, "    owner->Add(this, space);\n    return 0;", "    owner->Add(this, space);\n    return 1;"),
    # Load
    ("load: extension", R, 'file.Open(name, "ADR", 2, 0)', 'file.Open(name, "ADS", 2, 0)'),
    ("load: flags", R, 'file.Open(name, "ADR", 2, 0)', 'file.Open(name, "ADR", 3, 0)'),
    ("load: report", R, '        AtReportFileError(status, name, "ADR");\n', ""),
    ("load: report's extension", R, 'AtReportFileError(status, name, "ADR");', 'AtReportFileError(status, name, "ADS");'),
    ("load: file's path", R, "    CopyText(m_fileName, path);\n", "    m_fileName[0] = 0;\n"),
    ("load: path's room", R, "m_fileName = (char*)operator new(LengthOf(path) + 1);", "m_fileName = (char*)operator new(LengthOf(path));"),
    ("load: heap", R, "    PushHeap(s_heap);\n    m_fileName =", "    PushHeap(AtIoTokenFile::s_heap);\n    m_fileName ="),
    ("load: no pop", R, "m_fileName = (char*)operator new(LengthOf(path) + 1);\n    AtPopHeap();", "m_fileName = (char*)operator new(LengthOf(path) + 1);"),
    ("load: read's parent", R, "    Read(owner, file.m_tokenizer, spaceOut, parent);\n", "    Read(owner, file.m_tokenizer, spaceOut, 0);\n"),
    ("load: read's space", R, "    Read(owner, file.m_tokenizer, spaceOut, parent);\n", "    Read(owner, file.m_tokenizer, 0, parent);\n"),
    ("load: returns", R, "    m_loaded = 1;\n    return 0;\n}\n", "    m_loaded = 1;\n    return 1;\n}\n"),
    # Values
    ("values' heap", C, "void AtDataComponent::SetHeap(void* heap) {\n    s_heap = heap;", "void AtDataComponent::SetHeap(void* heap) {\n    (void)heap;"),
    ("no heap: raise", C, "    if (AtDataComponent::s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n", ""),
    ("new block: pop", C, "    void* block = operator new(size);\n    AtPopHeap();\n", "    void* block = operator new(size);\n"),
    ("room: ceiling ignored", C, "        return (unsigned int)ceiling;\n", "        return length;\n"),
    ("room: no-ceiling test", C, "if (ceiling != kNoCeiling && (unsigned int)ceiling > length) {", "if ((unsigned int)ceiling > length) {"),
    ("room: larger only", C, "if (ceiling != kNoCeiling && (unsigned int)ceiling > length) {", "if (ceiling != kNoCeiling) {"),
    ("resize: no raise", C, "        AtRaise(-4, 0, 0, 0);\n        return -4;\n    }\n    if (data != 0) {", "        return -4;\n    }\n    if (data != 0) {"),
    ("resize: refusal's value", C, "        AtRaise(-4, 0, 0, 0);\n        return -4;\n    }\n    if (data != 0) {", "        AtRaise(-4, 0, 0, 0);\n        return 0;\n    }\n    if (data != 0) {"),
    ("resize: keeps", C, "        memcpy(grown, data, (count > had ? had : count) * sizeof(T));\n", ""),
    ("resize: keeps by the new", C, "(count > had ? had : count) * sizeof(T)", "count * sizeof(T)"),
    ("resize: keeps by the old", C, "(count > had ? had : count) * sizeof(T)", "had * sizeof(T)"),
    ("resize: frees", C, "        memcpy(grown, data, (count > had ? had : count) * sizeof(T));\n        operator delete(data);\n", "        memcpy(grown, data, (count > had ? had : count) * sizeof(T));\n"),
    ("resize: stored", C, "    data = grown;\n", ""),
    ("resize: count", C, "    self->m_descriptor->m_count = count;\n", ""),
    ("free data: freed", C, "    if (data != 0) {\n        operator delete(data);\n    }\n    data = 0;\n", "    data = 0;\n"),
    ("free data: cleared", C, "        operator delete(data);\n    }\n    data = 0;\n", "        operator delete(data);\n    }\n"),
    ("free data: base's", C, "    self->AtDataComponent::Reset();\n", ""),
    ("narrow: to the room", C, "    if (length >= m_capacity) {\n        length = m_capacity;\n    }\n", ""),
    ("narrow: room less one", C, "        length = m_capacity;\n", "        length = m_capacity - 1;\n"),
    ("narrow: limit", C, "if (length >= m_capacity) {", "if (length > m_capacity + 1) {"),
    ("narrow: sign", C, "(unsigned int)(int)(signed char)text[i]", "(unsigned int)(unsigned char)text[i]"),
    ("narrow: chars", C, "        SetChar(i, (unsigned int)(int)(signed char)text[i]);\n", ""),
    ("narrow: terminator", C, "    SetChar(length, 0);\n", ""),
    ("narrow: end", C, "    m_end = length;\n", ""),
    ("narrow: start", C, "    m_end = length;\n    m_start = 0;\n", "    m_end = length;\n"),
    ("ascii release: data", C, "void AtTextAscii::Release() {\n    m_data = 0;\n", "void AtTextAscii::Release() {\n"),
    ("ascii release: fields", C, "    m_data = 0;\n    ClearFields();\n", "    m_data = 0;\n"),
    ("base ctor: descriptor", C, "AtDataComponent::AtDataComponent() {\n    m_descriptor = 0;\n", "AtDataComponent::AtDataComponent() {\n"),
    ("base ctor: registry", C, "AtDataComponent::AtDataComponent() {\n    m_descriptor = 0;\n    m_registry = 0;\n", "AtDataComponent::AtDataComponent() {\n    m_descriptor = 0;\n"),
    ("base read", C, "int AtDataComponent::Read(AtDataDescriptor* descriptor, AtIoTokenizer*) {\n    m_descriptor = descriptor;\n", "int AtDataComponent::Read(AtDataDescriptor* descriptor, AtIoTokenizer*) {\n"),
    ("base set descriptor", C, "int AtDataComponent::SetDescriptor(AtDataDescriptor* descriptor) {\n    m_descriptor = descriptor;\n", "int AtDataComponent::SetDescriptor(AtDataDescriptor* descriptor) {\n"),
    ("base reset: descriptor", C, "void AtDataComponent::Reset() {\n    m_descriptor = 0;\n", "void AtDataComponent::Reset() {\n"),
    ("base reset: registry", C, "void AtDataComponent::Reset() {\n    m_descriptor = 0;\n    m_registry = 0;\n", "void AtDataComponent::Reset() {\n    m_descriptor = 0;\n"),
    ("boolean ctor", C, "AtDataBoolean::AtDataBoolean() {\n    m_value = 0;\n", "AtDataBoolean::AtDataBoolean() {\n"),
    ("int ctor clears", C, "AtDataInt::AtDataInt() {\n", "AtDataInt::AtDataInt() {\n    m_value = 0;\n"),
    ("real ctor clears", C, "AtDataReal::AtDataReal() {\n", "AtDataReal::AtDataReal() {\n    m_value = 0;\n"),
    ("short name ctor clears", C, "AtDataShortName::AtDataShortName() {\n", "AtDataShortName::AtDataShortName() {\n    m_value[0] = 0;\n"),
]
for cls, reader in [("AtDataBoolean", "m_value = tokenizer->ExpectInt();"), ("AtDataInt", "m_value = tokenizer->ExpectInt();"), ("AtDataReal", "m_value = tokenizer->ExpectReal();"), ("AtDataShortName", "tokenizer->ExpectName(m_value);")]:
    head = "int %s::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n" % cls
    M.append((cls + " read: descriptor", C, head, "int %s::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n" % cls))
    M.append((cls + " read: value", C, head + "    " + reader + "\n", head))
    M.append((cls + " reset", C, "void %s::Reset() {\n    AtDataComponent::Reset();\n" % cls, "void %s::Reset() {\n" % cls))
for cls, read in [("AtDataBooleanArray", "        m_data[i] = tokenizer->ExpectInt();\n"), ("AtDataByteArray", "        m_data[i] = (unsigned char)tokenizer->ExpectInt();\n"), ("AtDataIntArray", "        m_data[i] = tokenizer->ExpectInt();\n"), ("AtDataRealArray", "        m_data[i] = tokenizer->ExpectReal();\n")]:
    head = "int %s::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n    const unsigned int count = m_descriptor->m_count;\n    Resize(count);\n    for (unsigned int i = 0; i < count; i++) {\n" % cls
    M.append((cls + " ctor", C, "%s::%s() {\n    m_data = 0;\n" % (cls, cls), "%s::%s() {\n" % (cls, cls)))
    M.append((cls + " dtor", C, "%s::~%s() {\n    FreeData(this, m_data);\n" % (cls, cls), "%s::~%s() {\n" % (cls, cls)))
    M.append((cls + " reset", C, "void %s::Reset() {\n    FreeData(this, m_data);\n" % cls, "void %s::Reset() {\n" % cls))
    M.append((cls + " read: value", C, head + read, head + "        tokenizer->ExpectInt();\n"))
    M.append((cls + " read: resize", C, head, head.replace("    Resize(count);\n", "")))
    M.append((cls + " resize", C, "int %s::Resize(unsigned int count) {\n    return ResizeData(this, m_data, count);\n" % cls, "int %s::Resize(unsigned int count) {\n    return 0;\n" % cls))
M += [
    ("name array ctor", C, "AtDataShortNameArray::AtDataShortNameArray() {\n    m_data = 0;\n", "AtDataShortNameArray::AtDataShortNameArray() {\n"),
    ("name array dtor", C, "AtDataShortNameArray::~AtDataShortNameArray() {\n    FreeData(this, m_data);\n", "AtDataShortNameArray::~AtDataShortNameArray() {\n"),
    ("name array reset", C, "void AtDataShortNameArray::Reset() {\n    FreeData(this, m_data);\n", "void AtDataShortNameArray::Reset() {\n"),
    ("name array read", C, "        memcpy(m_data[i], name, sizeof name);\n", ""),
    ("name array read: descriptor", C, "int AtDataShortNameArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n", "int AtDataShortNameArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    (void)descriptor;\n"),
    ("name array read: resize", C, "int AtDataShortNameArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n    const unsigned int count = m_descriptor->m_count;\n    Resize(count);\n", "int AtDataShortNameArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n    const unsigned int count = m_descriptor->m_count;\n"),
    ("name array resize", C, "int AtDataShortNameArray::Resize(unsigned int count) {\n    return ResizeData(this, m_data, count);\n", "int AtDataShortNameArray::Resize(unsigned int count) {\n    return 0;\n"),
    ("string ctor: unicode", C, "AtDataString::AtDataString() {\n    m_unicode = 0;\n", "AtDataString::AtDataString() {\n"),
    ("string ctor: chars", C, "AtDataString::AtDataString() {\n    m_unicode = 0;\n    m_chars = 0;\n", "AtDataString::AtDataString() {\n    m_unicode = 0;\n"),
]
for head, what in [("AtDataString::~AtDataString() {\n", "string dtor"), ("void AtDataString::Reset() {\n", "string reset")]:
    body = "    if (m_narrow.m_capacity > 0) {\n        m_narrow.Release();\n    }\n    if (m_wide.m_capacity > 0) {\n        m_wide.Release();\n    }\n    if (m_chars != 0) {\n        operator delete(m_chars);\n    }\n    m_unicode = 0;\n    m_chars = 0;\n    AtDataComponent::Reset();\n"
    M += [
        (what + ": narrow", C, head + body, head + body.replace("    if (m_narrow.m_capacity > 0) {\n        m_narrow.Release();\n    }\n", "")),
        (what + ": wide", C, head + body, head + body.replace("    if (m_wide.m_capacity > 0) {\n        m_wide.Release();\n    }\n", "")),
        (what + ": freed", C, head + body, head + body.replace("    if (m_chars != 0) {\n        operator delete(m_chars);\n    }\n", "")),
        (what + ": unicode", C, head + body, head + body.replace("    m_unicode = 0;\n", "")),
        (what + ": chars", C, head + body, head + body.replace("    m_chars = 0;\n", "")),
        (what + ": base", C, head + body, head + body.replace("    AtDataComponent::Reset();\n", "")),
    ]
M += [
    ("assign: which is active", C, "const AtText& active = m_unicode != 0 ? (const AtText&)m_wide : (const AtText&)m_narrow;", "const AtText& active = m_narrow;"),
    ("assign: over what it has", C, "    if (active.m_capacity > 0) {\n        Reset();\n    }\n", ""),
    ("assign: descriptor", C, "    SetDescriptor(descriptor);\n    if (m_unicode != 0) {", "    if (m_unicode != 0) {"),
    ("assign: wide's room", C, "RoomFor(descriptor, LengthOf(text) + 2)", "RoomFor(descriptor, LengthOf(text) + 1)"),
    ("assign: narrow's room", C, "const unsigned int room = RoomFor(descriptor, LengthOf(text) + 1);\n        char* chars", "const unsigned int room = RoomFor(descriptor, LengthOf(text) + 2);\n        char* chars"),
    ("assign: wide's block", C, "unsigned short* chars = (unsigned short*)NewBlock(room * sizeof(unsigned short));\n        if (chars == 0) {", "unsigned short* chars = (unsigned short*)NewBlock(room * 3);\n        if (chars == 0) {"),
    ("assign: wide's raise", C, "unsigned short* chars = (unsigned short*)NewBlock(room * sizeof(unsigned short));\n        if (chars == 0) {\n            AtRaise(-4, 0, 0, 0);\n        }\n", "unsigned short* chars = (unsigned short*)NewBlock(room * sizeof(unsigned short));\n"),
    ("assign: narrow's raise", C, "char* chars = (char*)NewBlock(room);\n        if (chars == 0) {\n            AtRaise(-4, 0, 0, 0);\n        }\n", "char* chars = (char*)NewBlock(room);\n"),
    ("assign: wide kept", C, "        m_chars = chars;\n        memset(chars, 0, room);\n        m_wide.Init", "        memset(chars, 0, room);\n        m_wide.Init"),
    ("assign: narrow kept", C, "        m_chars = chars;\n        memset(chars, 0, room);\n        m_narrow.Init", "        memset(chars, 0, room);\n        m_narrow.Init"),
    ("assign: wide's clear", C, "        memset(chars, 0, room);\n        m_wide.Init", "        memset(chars, 0, room * 2);\n        m_wide.Init"),
    ("assign: narrow's clear", C, "        memset(chars, 0, room);\n        m_narrow.Init", "        m_narrow.Init"),
    ("assign: wide init", C, "        m_wide.Init(chars, room);\n", ""),
    ("assign: narrow init", C, "        m_narrow.Init(chars, room);\n", ""),
    ("assign: wide text", C, "        m_wide.SetNarrow(text);\n", ""),
    ("assign: narrow text", C, "        m_narrow.SetNarrow(text);\n", ""),
    ("string read: word", C, 'm_unicode = EqualIgnoringCase(tokenizer->m_string, "Unicode");', 'm_unicode = EqualIgnoringCase(tokenizer->m_string, "Unicodes");'),
    ("string read: case", C, 'm_unicode = EqualIgnoringCase(tokenizer->m_string, "Unicode");', 'm_unicode = tokenizer->m_string[0] == \'U\';'),
    ("string read: only with a string", C, "if (tokenizer->Next() == AtIoTokenizer::kString) {", "if (tokenizer->Next() != AtIoTokenizer::kEnd) {"),
    ("string read: pushed back", C, "        tokenizer->m_pushedBack = 1;\n", ""),
    ("string read: assign", C, "    Assign(descriptor, text);\n", ""),
    # String arrays
    ("strings ctor: chars", C, "AtDataStringArray::AtDataStringArray() {\n    m_chars = 0;\n", "AtDataStringArray::AtDataStringArray() {\n"),
    ("strings ctor: texts", C, "    m_chars = 0;\n    m_texts = 0;\n}\n\nAtDataStringArray::~", "    m_chars = 0;\n}\n\nAtDataStringArray::~"),
    ("strings dtor", C, "AtDataStringArray::~AtDataStringArray() {\n    Reset();\n", "AtDataStringArray::~AtDataStringArray() {\n"),
    ("strings reset: none", C, "    if (m_texts == 0) {\n        return;\n    }\n", ""),
    ("strings reset: none, base", C, "    if (m_texts == 0) {\n        return;\n    }\n", "    if (m_texts == 0) {\n        AtDataComponent::Reset();\n        return;\n    }\n"),
    ("strings reset: each freed", C, "        if (m_chars[i] != 0) {\n            operator delete(m_chars[i]);\n        }\n", ""),
    ("strings reset: list freed", C, "    operator delete(m_chars);\n    operator delete(m_texts);\n    m_texts = 0;", "    operator delete(m_texts);\n    m_texts = 0;"),
    ("strings reset: texts freed", C, "    operator delete(m_chars);\n    operator delete(m_texts);\n    m_texts = 0;", "    operator delete(m_chars);\n    m_texts = 0;"),
    ("strings reset: texts", C, "    m_texts = 0;\n    m_chars = 0;\n    AtDataComponent::Reset();", "    m_chars = 0;\n    AtDataComponent::Reset();"),
    ("strings reset: chars", C, "    m_texts = 0;\n    m_chars = 0;\n    AtDataComponent::Reset();", "    m_texts = 0;\n    AtDataComponent::Reset();"),
    ("strings reset: base", C, "    m_texts = 0;\n    m_chars = 0;\n    AtDataComponent::Reset();", "    m_texts = 0;\n    m_chars = 0;"),
    ("strings resize: first's raise", C, "    if (chars == 0) {\n        AtRaise(-4, 0, 0, 0);\n        return -4;\n    }\n", "    if (chars == 0) {\n        return -4;\n    }\n"),
    ("strings resize: refusal's value", C, "    if (chars == 0) {\n        AtRaise(-4, 0, 0, 0);\n        return -4;\n    }\n", "    if (chars == 0) {\n        AtRaise(-4, 0, 0, 0);\n        return 0;\n    }\n"),
    ("strings resize: texts built", C, "            new (texts + i, AtPlacement()) AtTextUnicode;\n", ""),
    ("strings resize: list cleared", C, "        chars[i] = 0;\n", ""),
    ("strings resize: texts kept", C, "        memcpy((void*)texts, (const void*)m_texts, kept * sizeof(AtTextUnicode));\n", ""),
    ("strings resize: list kept", C, "        memcpy(chars, m_chars, kept * sizeof(unsigned short*));\n", ""),
    ("strings resize: keeps by the new", C, "const unsigned int kept = count > had ? had : count;", "const unsigned int kept = count;"),
    ("strings resize: keeps by the old", C, "const unsigned int kept = count > had ? had : count;", "const unsigned int kept = had;"),
    ("strings resize: old list", C, "        operator delete(m_chars);\n        operator delete(m_texts);\n", "        operator delete(m_texts);\n"),
    ("strings resize: old texts", C, "        operator delete(m_chars);\n        operator delete(m_texts);\n", "        operator delete(m_chars);\n"),
    ("strings resize: list", C, "    m_chars = chars;\n    m_texts = texts;\n", "    m_texts = texts;\n"),
    ("strings resize: texts", C, "    m_chars = chars;\n    m_texts = texts;\n", "    m_chars = chars;\n"),
    ("strings resize: count", C, "    m_descriptor->m_count = count;\n    return 0;", "    return 0;"),
    ("set text: room", C, "const unsigned int room = RoomFor(m_descriptor, LengthOf(text) + 1);\n    PushHeap();", "const unsigned int room = RoomFor(m_descriptor, LengthOf(text) + 2);\n    PushHeap();"),
    ("set text: block", C, "m_chars[index] = (unsigned short*)operator new(room * sizeof(unsigned short));", "m_chars[index] = (unsigned short*)operator new(room);"),
    ("set text: pop", C, "m_chars[index] = (unsigned short*)operator new(room * sizeof(unsigned short));\n    AtPopHeap();", "m_chars[index] = (unsigned short*)operator new(room * sizeof(unsigned short));"),
    ("set text: raise", C, "    if (m_chars[index] == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("set text: init", C, "    m_texts[index].Init(m_chars[index], room);\n", ""),
    ("set text: text", C, "    m_texts[index].SetNarrow(text);\n", ""),
    ("strings read: descriptor", C, "int AtDataStringArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n", "int AtDataStringArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    (void)descriptor;\n"),
    ("strings read: resize", C, "int AtDataStringArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n    const unsigned int count = m_descriptor->m_count;\n    Resize(count);\n", "int AtDataStringArray::Read(AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer) {\n    SetDescriptor(descriptor);\n    const unsigned int count = m_descriptor->m_count;\n"),
    ("strings read: set", C, "        SetText(i, text);\n", ""),
]

# Survivors of a run, or the order of one: ADR_MUTANTS names a file of names, one to a line, and only those run. The default is all of them.
import os as _os

if _os.environ.get("ADR_MUTANTS"):
    _names = set(open(_os.environ["ADR_MUTANTS"]).read().splitlines())
    M = [m for m in M if m[0] in _names]

SUITE = {"target": "oracle_adr", "subsystem": "at/adr", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": M}
