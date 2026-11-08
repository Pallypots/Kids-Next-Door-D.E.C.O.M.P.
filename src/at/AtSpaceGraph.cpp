// A space graph: reading an .ASG file and managing its links. See include/at/AtSpaceGraph.h. The five removal functions are in src/at/AtSceneNode.cpp, where they were found first.
#include "at/AtSpaceGraph.h"

#include "at/AtHeap.h"
#include "at/AtText.h"

#include "AtArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtSpaceGraph::s_heap;
int AtSpaceGraph::s_itemsPeak;

const AtIoKeyword AtSpaceGraph::s_keywords[AtSpaceGraph::kKeywordCount] = {
    {57, "BinarySpace"}, {58, "XBinarySpace"}, {59, "YBinarySpace"}, {60, "ZBinarySpace"}, {61, "LeafSpace"}, {62, "NodeLink"}, {63, "NodeIncidence"}, {64, "PotentiallyVisibleSet"},
    {65, "Partition"}, {66, "Draw"}, {67, "Collide"}, {68, "Modifier"}, {69, "SpaceGraph"}, {70, "Root"}, {71, "BoundingBox"}, {72, "PosChild"},
    {73, "NegChild"}, {74, "LineSegIntersect"}, {75, "Cell"}};

namespace {

void AlignHeap() {
    AtHeap* heap = (AtHeap*)AtSpaceGraph::s_heap;
    heap->m_allocShift = heap->m_alignShift >= 5 ? heap->m_alignShift : 5;
}

void RestoreHeapAlignment() {
    AtHeap* heap = (AtHeap*)AtSpaceGraph::s_heap;
    heap->m_allocShift = heap->m_alignShift;
}

template <class T> T* NewSpaces(unsigned int count) {
    AtPushHeap(AtSpaceGraph::s_heap);
    T* list = AtNewArray<T>(count);
    AtPopHeap();
    if (list == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return list;
}

template <class T> T* NewPool(unsigned int count) {
    AtPushHeap(AtSpaceGraph::s_heap);
    T* pool = (T*)operator new(count * sizeof(T));
    if (pool != 0) {
        for (unsigned int i = 0; i < count; i++) {
            new (pool + i, AtPlacement()) T;
        }
    }
    AtPopHeap();
    if (pool == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return pool;
}

template <class T> void FreeSpaces(T* list) {
    if (list != 0) {
        AtDeleteArray(list);
    }
}

// Each of a class's list, every element a word and a body.
template <class T> void ReadSpaceList(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word, unsigned int given, unsigned int have, T* list) {
    if (given > have) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    for (unsigned int i = 0; i < given; i++) {
        tokenizer->ExpectToken(word);
        list[i].Read(owner, graph, tokenizer);
    }
}

}  // namespace

void AtSpaceGraph::SetHeap(void* heap) {
    s_heap = heap;
}

AtSpaceGraph::AtSpaceGraph() {
    s_itemsInUse = 0;
    s_itemsPeak = 0;
    m_linkCount = 0;
    m_links = 0;
    m_freeLinks = 0;
    m_itemCount = 0;
    m_items = 0;
    m_freeItems = 0;
    m_binaryCount = 0;
    m_binary = 0;
    m_xCount = 0;
    m_x = 0;
    m_yCount = 0;
    m_y = 0;
    m_zCount = 0;
    m_z = 0;
    m_leafCount = 0;
    m_leaves = 0;
    m_root = 0;
    m_hooksBefore = 0;
    m_hooksAfter = 0;
    for (int i = 0; i < kAtLinkKinds; i++) {
        m_dirty[i] = 0;
    }
}

AtSpaceGraph::~AtSpaceGraph() {
    Clear();
}

void AtSpaceGraph::Clear() {
    AtSceneLink* const end = m_links + m_linkCount;
    for (AtSceneLink* link = m_links; link != end; link++) {
        if (link->m_set != 0 && link->m_items != 0) {
            AtSceneNode* node = link->m_items->m_node;
            for (m_kind = 0; m_kind < kAtLinkKinds; m_kind++) {
                RemoveLink(node);
            }
        }
    }
    FreeSpaces(m_leaves);
    FreeSpaces(m_z);
    FreeSpaces(m_y);
    FreeSpaces(m_x);
    FreeSpaces(m_binary);
    if (m_items != 0) {
        operator delete(m_items);
    }
    if (m_links != 0) {
        operator delete(m_links);
    }
    s_itemsInUse = 0;
    s_itemsPeak = 0;
    m_linkCount = 0;
    m_links = 0;
    m_freeLinks = 0;
    m_itemCount = 0;
    m_items = 0;
    m_freeItems = 0;
    m_binaryCount = 0;
    m_binary = 0;
    m_xCount = 0;
    m_x = 0;
    m_yCount = 0;
    m_y = 0;
    m_zCount = 0;
    m_z = 0;
    m_leafCount = 0;
    m_leaves = 0;
    m_root = 0;
    m_hooksBefore = 0;
    m_hooksAfter = 0;
    for (int i = 0; i < kAtLinkKinds; i++) {
        m_dirty[i] = 0;
    }
}

void AtSpaceGraph::ReadCounts(AtIoTokenizer* tokenizer) {
    unsigned int* count = 0;
    for (int word = tokenizer->Next(); word != kAtWordRoot; word = tokenizer->Next()) {
        switch (word) {
        case kAtWordBinarySpace:
            count = &m_binaryCount;
            break;
        case kAtWordXBinarySpace:
            count = &m_xCount;
            break;
        case kAtWordYBinarySpace:
            count = &m_yCount;
            break;
        case kAtWordZBinarySpace:
            count = &m_zCount;
            break;
        case kAtWordLeafSpace:
            count = &m_leafCount;
            break;
        case kAtWordNodeLink:
            count = &m_linkCount;
            break;
        case kAtWordNodeIncidence:
            count = &m_itemCount;
            break;
        default:
            // Then the count goes to the last word's. If there was none, this crashes in the original too.
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        tokenizer->ExpectLeftBracket();
        *count = (unsigned int)tokenizer->ExpectInt();
        tokenizer->ExpectRightBracket();
    }
}

void AtSpaceGraph::Allocate() {
    if (m_linkCount == 0) {
        m_linkCount = 1000;
    }
    if (m_itemCount <= m_linkCount) {
        m_itemCount = m_linkCount * 3;
    }

    AlignHeap();
    m_links = NewPool<AtSceneLink>(m_linkCount);
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    m_items = NewPool<AtSceneLinkItem>(m_itemCount);
    RestoreHeapAlignment();

    AtSceneLink* const lastLink = m_links + m_linkCount - 1;
    AtSceneLink* link = m_links;
    m_freeLinks = link;
    while (link < lastLink) {
        link->m_set = 0;
        link->m_next = link + 1;
        link++;
    }
    link->m_next = 0;

    AtSceneLinkItem* const lastItem = m_items + m_itemCount - 1;
    AtSceneLinkItem* item = m_items;
    m_freeItems = item;
    while (item < lastItem) {
        item->m_next = item + 1;
        item++;
    }
    item->m_next = 0;

    s_itemsInUse = 0;
    s_itemsPeak = 0;

    if (m_binaryCount != 0) {
        AlignHeap();
        m_binary = NewSpaces<AtBinarySpace>(m_binaryCount);
        RestoreHeapAlignment();
    }
    if (m_xCount != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        m_x = NewSpaces<AtXBinarySpace>(m_xCount);
    }
    if (m_yCount != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        m_y = NewSpaces<AtYBinarySpace>(m_yCount);
    }
    if (m_zCount != 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        m_z = NewSpaces<AtZBinarySpace>(m_zCount);
    }
    if (m_leafCount != 0) {
        AlignHeap();
        m_leaves = NewSpaces<AtLeafSpace>(m_leafCount);
        RestoreHeapAlignment();
    }
}

AtSpace* AtSpaceGraph::ReadSpaceReference(AtIoTokenizer* tokenizer) {
    const int word = tokenizer->Next();
    const unsigned int index = (unsigned int)tokenizer->ExpectInt();
    switch (word) {
    case kAtWordBinarySpace:
        if (index < m_binaryCount) {
            return &m_binary[index];
        }
        break;
    case kAtWordXBinarySpace:
        if (index < m_xCount) {
            return &m_x[index];
        }
        break;
    case kAtWordYBinarySpace:
        if (index < m_yCount) {
            return &m_y[index];
        }
        break;
    case kAtWordZBinarySpace:
        if (index < m_zCount) {
            return &m_z[index];
        }
        break;
    case kAtWordLeafSpace:
        if (index < m_leafCount) {
            return &m_leaves[index];
        }
        break;
    default:
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        break;
    }
    tokenizer->Error(kAtProblemIsEmpty);
    return 0;
}

void AtSpaceGraph::ReadSpaces(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    for (int word = tokenizer->Next(); word != AtIoTokenizer::kRightCurly; word = tokenizer->Next()) {
        const unsigned int given = (unsigned int)tokenizer->ExpectCount();
        switch (word) {
        case kAtWordBinarySpace:
            ReadSpaceList(owner, this, tokenizer, kAtWordBinarySpace, given, m_binaryCount, m_binary);
            break;
        case kAtWordXBinarySpace:
            ReadSpaceList(owner, this, tokenizer, kAtWordXBinarySpace, given, m_xCount, m_x);
            break;
        case kAtWordYBinarySpace:
            ReadSpaceList(owner, this, tokenizer, kAtWordYBinarySpace, given, m_yCount, m_y);
            break;
        case kAtWordZBinarySpace:
            ReadSpaceList(owner, this, tokenizer, kAtWordZBinarySpace, given, m_zCount, m_z);
            break;
        case kAtWordLeafSpace:
            ReadSpaceList(owner, this, tokenizer, kAtWordLeafSpace, given, m_leafCount, m_leaves);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        tokenizer->ExpectRightCurly();
    }
}

void AtSpaceGraph::Build(AtNameTableOwner* owner) {
    owner->m_modifierStack.Init(owner->m_stackDepth, owner->m_stackSlots, 1);
    m_root->Build(owner);
    owner->m_modifierStack.Free();
}

void AtSpaceGraph::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_root != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    tokenizer->ExpectToken(kAtWordSpaceGraph);
    tokenizer->ExpectLeftCurly();
    ReadCounts(tokenizer);
    if (m_leafCount < 1) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    Allocate();
    m_root = ReadSpaceReference(tokenizer);
    ReadSpaces(owner, tokenizer);
    // Leaves take what they are told as they read it. The rest had to wait for their children to exist.
    for (unsigned int i = 0; i < m_binaryCount; i++) {
        m_binary[i].AttachPending();
    }
    for (unsigned int i = 0; i < m_xCount; i++) {
        m_x[i].AttachPending();
    }
    for (unsigned int i = 0; i < m_yCount; i++) {
        m_y[i].AttachPending();
    }
    for (unsigned int i = 0; i < m_zCount; i++) {
        m_z[i].AttachPending();
    }
    Build(owner);
}

void AtSpaceGraph::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    if (m_root != 0) {
        Clear();
    }
    const int status = file.Open(name, "ASG", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASG");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

// ---- links ------------------------------------------------------------------------------------------------------

AtSceneLink* AtSpaceGraph::TakeLink() {
    AtSceneLink* link = m_freeLinks;
    if (link == 0) {
        return 0;
    }
    link->m_set = this;
    m_freeLinks = link->m_next;
    return link;
}

void AtSpaceGraph::ReturnLink(AtSceneLink* link) {
    link->m_set = 0;
    link->m_next = m_freeLinks;
    m_freeLinks = link;
}

AtSceneLinkItem* AtSpaceGraph::TakeItem() {
    s_itemsInUse++;
    if (s_itemsInUse > s_itemsPeak) {
        s_itemsPeak = s_itemsInUse;
    }
    AtSceneLinkItem* item = m_freeItems;
    if (item == 0) {
        return 0;
    }
    m_freeItems = item->m_next;
    return item;
}

void AtSpaceGraph::MakeLink(AtSceneNode* node, AtSceneLink* link, AtSceneLinkItem* item) {
    if ((node->m_flags & AtSceneNode::kMoved) != 0) {
        node->FinishBody();
    }
    link->m_set = this;
    link->m_center = node->m_worldSphere.center;
    link->m_margin = 4294967296.0f;
    link->m_next = node->m_links[m_kind];
    node->m_links[m_kind] = link;
    link->m_items = item;
    link->m_nextDirty = 0;
    item->m_node = node;
    item->m_next = 0;
    item->m_head = 0;
}

void AtSpaceGraph::AddNode(AtSceneNode* node) {
    for (AtSceneLink* link = node->m_links[m_kind]; link != 0; link = link->m_next) {
        if (link->m_set == this) {
            return;
        }
    }
    AtSceneLink* link = TakeLink();
    if (link == 0) {
        return;
    }
    AtSceneLinkItem* item = TakeItem();
    if (item == 0) {
        ReturnLink(link);
        return;
    }
    MakeLink(node, link, item);
    if ((node->m_flags & AtSceneNode::kMoved) != 0) {
        node->FinishBody();
    }
    m_root->Insert(link, node->m_worldSphere.radius, item);
    const double margin = link->m_margin;
    link->m_margin = (float)(margin * margin);
}

void AtSpaceGraph::AddAsKind0(AtSceneNode* node) {
    m_kind = 0;
    RemoveDirty(node);
    AddNode(node);
}

void AtSpaceGraph::AddAsKind1(AtSceneNode* node) {
    m_kind = 1;
    RemoveDirty(node);
    AddNode(node);
}

void AtSpaceGraph::AddAsKind2(AtSceneNode* node) {
    m_kind = 2;
    AddNode(node);
}

void AtSpaceGraph::AddAsKind3(AtSceneNode* node) {
    m_kind = 3;
    RemoveDirty(node);
    AddNode(node);
}

void AtSpaceGraph::RemoveAsKind2(AtSceneNode* node) {
    m_kind = 2;
    RemoveDirty(node);
    RemoveLink(node);
}

// ---- hooks ------------------------------------------------------------------------------------------------------

namespace {

void AddHook(AtSpaceGraphHook** first, AtSpaceGraphHook* hook) {
    AtSpaceGraphHook* at = *first;
    if (at == 0) {
        *first = hook;
        return;
    }
    AtSpaceGraphHook* last = 0;
    while (at != 0) {
        if (at == hook) {
            return;
        }
        last = at;
        at = at->m_next;
    }
    last->m_next = hook;
}

void RemoveHook(AtSpaceGraphHook** first, AtSpaceGraphHook* hook) {
    AtSpaceGraphHook* before = *first;
    for (AtSpaceGraphHook* at = *first; at != 0; at = at->m_next) {
        if (at == hook) {
            if (at == *first) {
                *first = hook->m_next;
            } else {
                before->m_next = hook->m_next;
            }
            hook->m_next = 0;
            return;
        }
        before = at;
    }
}

}  // namespace

void AtSpaceGraph::AddHookBefore(AtSpaceGraphHook* hook) { AddHook(&m_hooksBefore, hook); }
void AtSpaceGraph::RemoveHookBefore(AtSpaceGraphHook* hook) { RemoveHook(&m_hooksBefore, hook); }
void AtSpaceGraph::AddHookAfter(AtSpaceGraphHook* hook) { AddHook(&m_hooksAfter, hook); }
void AtSpaceGraph::RemoveHookAfter(AtSpaceGraphHook* hook) { RemoveHook(&m_hooksAfter, hook); }
