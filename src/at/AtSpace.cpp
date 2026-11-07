// The spaces of a graph. See include/at/AtSpaceGraph.h. In the Insert functions, `double` stands for what the original keeps in an x87 register, `float` for what it stored; the order of each sum is the original's.
#include "at/AtSpaceGraph.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

namespace {

void NotRecovered() {
    AtFatal("Atlas Space: this slot belongs to drawing or moving and is not recovered", 0, 0, 0);
}

// 0x00056DA0, in a range held elsewhere: six reals. Used by no one here except for its longest side.
struct SpaceBox {
    float m_value[6];
    void Read(AtIoTokenizer* tokenizer) {
        for (int i = 0; i < 6; i++) {
            m_value[i] = tokenizer->ExpectReal();
        }
    }
};

// Slot 9 of the classes with children, all but the distance. A sphere wholly on one side goes to that child, and the link remembers how far it is from the plane. One across both goes to both, on two items; with no item left, only to the positive.
void InsertAcross(AtSpace* positive, AtSpace* negative, AtSceneLink* link, float radius, AtSceneLinkItem* item, float distance) {
    float margin;
    if (-radius > distance) {
        margin = (float)(-(double)distance - radius);
        negative->Insert(link, radius, item);
    } else if (distance >= radius) {
        margin = (float)((double)distance - radius);
        positive->Insert(link, radius, item);
    } else {
        AtSceneLinkItem* second = link->m_set->TakeItem();
        if (second == 0) {
            positive->Insert(link, radius, item);
            return;
        }
        item->AddSecond(second);
        if (distance < 0.0f) {
            margin = -distance;
            negative->Insert(link, radius, item);
            positive->Insert(link, radius, second);
        } else {
            margin = distance;
            negative->Insert(link, radius, second);
            positive->Insert(link, radius, item);
        }
    }
    if (margin < link->m_margin) {
        link->m_margin = margin;
    }
}

}  // namespace

// ---- what the owner finds, and an item ------------------------------------------------------------------------------------------------------

// 0x00055930, which this calls, is in a range held elsewhere; it is a seam here, by the class word. Its report of a null node cannot happen on this path.
AtSceneNode* AtNameTableOwner::FindActor(const char* name, const char* ns) {
    AtSceneNode* node = FindNode(name, ns);
    if (node == 0) {
        return 0;
    }
    if (node->m_class < 1 || node->m_class > 13) {
        AtFatal("Atlas Scene: Error trying to cast a node to an illegal type", 0, 0, 0);
    }
    return node;
}

void* AtNameTableOwner::FindCellTable(const char* name, const char* ns) {
    return ns != 0 && ns[0] != 0 ? m_cellTableSpaces.Find(ns, name) : m_cellTables.Find(name);
}

AtModifier* AtNameTableOwner::FindModifier(const char* name, const char* ns) {
    return (AtModifier*)(ns != 0 && ns[0] != 0 ? m_modifierSpaces.Find(ns, name) : m_modifiers.Find(name));
}

void AtSceneLinkItem::AddSecond(AtSceneLinkItem* added) {
    added->m_node = m_node;
    added->m_next = m_next;
    m_next = added;
    added->m_head = m_head != 0 ? m_head : this;
}

// ---- the base ------------------------------------------------------------------------------------------------------

AtSpace::AtSpace() {
    m_modifier = 0;
    m_pending = 0;
}

AtSpace::~AtSpace() {
    AtSpace::Clear();
}

void AtSpace::Clear() {
    Pending* pending = m_pending;
    while (pending != 0) {
        Pending* next = pending->m_next;
        operator delete(pending);
        pending = next;
    }
    m_modifier = 0;
    m_pending = 0;
}

void AtSpace::InitFrom(const AtSpaceDesc* desc) {
    m_modifier = desc->m_modifier;
}

void AtSpace::Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftCurly();
    int word = tokenizer->Next();
    while (word != AtIoTokenizer::kRightCurly) {
        ReadWord(owner, graph, tokenizer, word);
        word = tokenizer->Next();
    }
}

void AtSpace::Slot12(void* a) {
    if (m_modifier != 0) {
        m_modifier->Slot5(a);
    }
}

void AtSpace::AddPending(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    Pending* pending = (Pending*)operator new(sizeof(Pending));
    AtPopHeap();
    if (pending == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    pending->m_graph = graph;
    pending->m_kind = kind;
    pending->m_node = node;
    pending->m_next = m_pending;
    m_pending = pending;
}

void AtSpace::AttachPending() {
    for (Pending* pending = m_pending; pending != 0; pending = pending->m_next) {
        Attach(pending->m_graph, pending->m_kind, pending->m_node);
    }
}

void AtSpace::Enter(AtNameTableOwner* owner) {
    if (m_modifier != 0) {
        AtModifierStack& stack = owner->m_modifierStack;
        stack.Push();
        m_modifier->Apply(stack.Top(), 0);
    }
}

void AtSpace::Leave(AtNameTableOwner* owner) {
    if (m_modifier != 0) {
        owner->m_modifierStack.Pop();
    }
}

void AtSpace::ReadCommonWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) {
    char name[8];
    char ns[8];
    switch (word) {
    case kAtWordDraw: {
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* actor = owner->FindActor(name, ns);
        if (actor == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        AddPending(graph, kAtLinkDraw, actor);
        break;
    }
    case kAtWordLineSegIntersect: {
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* node = owner->FindNode(name, ns);
        if (node == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        AddPending(graph, kAtLinkLineSegIntersect, node);
        break;
    }
    case kAtWordSpaceModifier:
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_modifier = owner->FindModifier(name, ns);
        if (m_modifier == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    case kAtWordBoundingBox: {
        // Read and not kept, except by a leaf.
        SpaceBox box;
        box.Read(tokenizer);
        break;
    }
    default:
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        break;
    }
}

// ---- leaves ------------------------------------------------------------------------------------------------------

AtLeafSpace::AtLeafSpace() {
    m_cell = 0;
    m_sortAxis = 0;
    for (int i = 0; i < kAtLinkKinds; i++) {
        m_first[i] = 0;
    }
}

AtLeafSpace::~AtLeafSpace() {
    AtSpace::Clear();
    m_cell = 0;
    m_sortAxis = 0;
    for (int i = 0; i < kAtLinkKinds; i++) {
        m_first[i] = 0;
    }
}

void AtLeafSpace::Clear() {
    AtSpace::Clear();
    m_cell = 0;
    m_sortAxis = 0;
    for (int i = 0; i < kAtLinkKinds; i++) {
        m_first[i] = 0;
    }
}

void AtLeafSpace::Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) {
    AtSpace::Read(owner, graph, tokenizer);
}

void AtLeafSpace::ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) {
    char name[8];
    char ns[8];
    switch (word) {
    case kAtWordPotentiallyVisibleSet:
        // Nothing is read, so what follows is taken as words.
        break;
    case kAtWordDraw: {
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* actor = owner->FindActor(name, ns);
        if (actor == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        Attach(graph, kAtLinkDraw, actor);
        break;
    }
    case kAtWordLineSegIntersect: {
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneNode* node = owner->FindNode(name, ns);
        if (node == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        Attach(graph, kAtLinkLineSegIntersect, node);
        break;
    }
    case kAtWordBoundingBox: {
        SpaceBox box;
        box.Read(tokenizer);
        const double x = (double)box.m_value[1] - box.m_value[0];
        const float y = box.m_value[3] - box.m_value[2];
        const float z = box.m_value[5] - box.m_value[4];
        if (x > y) {
            m_sortAxis = x > z ? 0 : 2;
        } else {
            m_sortAxis = y > z ? 1 : 2;
        }
        break;
    }
    case kAtWordCell: {
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSpaceCellTable* table = (AtSpaceCellTable*)owner->FindCellTable(name, ns);
        if (table == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        tokenizer->ExpectName(name);
        m_cell = table->m_cells.Find(name);
        if (m_cell == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    }
    default:
        ReadCommonWord(owner, graph, tokenizer, word);
        break;
    }
}

void AtLeafSpace::Build(AtNameTableOwner* owner) {
    Enter(owner);
    m_state.Assign(*owner->m_modifierStack.Top());
    Leave(owner);
}

void AtLeafSpace::Attach(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node) {
    AtSceneLink* link = graph->TakeLink();
    if (link == 0) {
        return;
    }
    AtSceneLinkItem* item = graph->TakeItem();
    if (item == 0) {
        graph->ReturnLink(link);
        return;
    }
    graph->m_kind = kind;
    graph->MakeLink(node, link, item);
    item->m_list = this;
    item->m_head = item;
    if (m_first[kind] != 0) {
        m_first[kind]->m_nextInList = item;
    }
    item->m_prevInList = m_first[kind];
    m_first[kind] = item;
    item->m_nextInList = 0;
}

void AtLeafSpace::Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) {
    const unsigned int kind = link->m_set->m_kind;
    AtSceneLinkItem* after = m_first[kind];
    item->m_list = this;
    if (item->m_head == 0) {
        item->m_head = item;
    }
    if (kind >= 2) {
        m_first[kind] = item;
        item->m_nextInList = 0;
        item->m_prevInList = after;
    } else {
        // In order of the low end, along the longest side.
        switch (m_sortAxis) {
        case 0:
            item->m_low = (float)((double)link->m_center.x - radius);
            item->m_high = (float)((double)radius + link->m_center.x);
            break;
        case 1:
            item->m_low = (float)((double)link->m_center.y - radius);
            item->m_high = (float)((double)radius + link->m_center.y);
            break;
        case 2:
            item->m_low = (float)((double)link->m_center.z - radius);
            item->m_high = (float)((double)radius + link->m_center.z);
            break;
        default:
            break;
        }
        AtSceneLinkItem* before = 0;
        while (after != 0 && after->m_low < item->m_low) {
            before = after;
            after = after->m_prevInList;
        }
        item->m_nextInList = before;
        item->m_prevInList = after;
        if (before == 0) {
            m_first[kind] = item;
        } else {
            before->m_prevInList = item;
        }
    }
    if (after != 0) {
        after->m_nextInList = item;
    }
}

void AtLeafSpace::Slot6() { NotRecovered(); }
void AtLeafSpace::Slot7() { NotRecovered(); }
void AtLeafSpace::Slot10() { NotRecovered(); }
void AtLeafSpace::Slot11() { NotRecovered(); }

// ---- two children ------------------------------------------------------------------------------------------------------

AtBinarySpaceBase::AtBinarySpaceBase() {
    m_posChild = 0;
    m_negChild = 0;
}

AtBinarySpaceBase::~AtBinarySpaceBase() {}

void AtBinarySpaceBase::ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) {
    if (word == kAtWordPosChild) {
        m_posChild = graph->ReadSpaceReference(tokenizer);
    } else if (word == kAtWordNegChild) {
        m_negChild = graph->ReadSpaceReference(tokenizer);
    } else {
        ReadCommonWord(owner, graph, tokenizer, word);
    }
}

void AtBinarySpaceBase::Build(AtNameTableOwner* owner) {
    Enter(owner);
    if (m_posChild != 0) {
        m_posChild->Build(owner);
    }
    if (m_negChild != 0) {
        m_negChild->Build(owner);
    }
    Leave(owner);
}

void AtBinarySpaceBase::Clear() {
    AtSpace::Clear();
    m_posChild = 0;
    m_negChild = 0;
}

void AtBinarySpaceBase::Attach(AtSpaceGraph* graph, unsigned int kind, AtSceneNode* node) {
    if (m_posChild != 0) {
        m_posChild->Attach(graph, kind, node);
    }
    if (m_negChild != 0) {
        m_negChild->Attach(graph, kind, node);
    }
}

// ---- a plane ------------------------------------------------------------------------------------------------------

AtBinarySpace::AtBinarySpace() {
    m_plane.x = 0;
    m_plane.y = 0;
    m_plane.z = 0;
    m_plane.w = 0;
}

AtBinarySpace::~AtBinarySpace() {}

void AtBinarySpace::Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) {
    AtSpace::Read(owner, graph, tokenizer);
}

void AtBinarySpace::ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) {
    if (word == kAtWordPartition) {
        m_plane.Read(tokenizer);
    } else {
        AtBinarySpaceBase::ReadWord(owner, graph, tokenizer, word);
    }
}

void AtBinarySpace::Clear() {
    AtBinarySpaceBase::Clear();
    m_plane.x = 0;
    m_plane.y = 0;
    m_plane.z = 0;
    m_plane.w = 0;
}

void AtBinarySpace::Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) {
    const float distance = (float)((double)link->m_center.z * m_plane.z + (double)link->m_center.y * m_plane.y + (double)m_plane.x * link->m_center.x + m_plane.w);
    InsertAcross(m_posChild, m_negChild, link, radius, item, distance);
}

void AtBinarySpace::Plane(float* out) const {
    out[0] = m_plane.x;
    out[1] = m_plane.y;
    out[2] = m_plane.z;
    out[3] = m_plane.w;
}

void AtBinarySpace::Slot6() { NotRecovered(); }
void AtBinarySpace::Slot7() { NotRecovered(); }
void AtBinarySpace::Slot10() { NotRecovered(); }
void AtBinarySpace::Slot11() { NotRecovered(); }

// ---- along an axis ------------------------------------------------------------------------------------------------------

AtAxisBinarySpace::AtAxisBinarySpace() {
    m_distance = 0;
}

AtAxisBinarySpace::~AtAxisBinarySpace() {}

void AtAxisBinarySpace::Read(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer) {
    AtSpace::Read(owner, graph, tokenizer);
}

void AtAxisBinarySpace::ReadWord(AtNameTableOwner* owner, AtSpaceGraph* graph, AtIoTokenizer* tokenizer, int word) {
    if (word == kAtWordPartition) {
        m_distance = tokenizer->ExpectReal();
    } else {
        AtBinarySpaceBase::ReadWord(owner, graph, tokenizer, word);
    }
}

void AtAxisBinarySpace::Clear() {
    AtBinarySpaceBase::Clear();
    m_distance = 0;
}

AtXBinarySpace::AtXBinarySpace() {}
AtXBinarySpace::~AtXBinarySpace() {}

void AtXBinarySpace::Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) {
    InsertAcross(m_posChild, m_negChild, link, radius, item, (float)((double)link->m_center.x - m_distance));
}

void AtXBinarySpace::Plane(float* out) const {
    out[0] = 1.0f;
    out[1] = 0;
    out[2] = 0;
    out[3] = -m_distance;
}

void AtXBinarySpace::Slot6() { NotRecovered(); }
void AtXBinarySpace::Slot7() { NotRecovered(); }
void AtXBinarySpace::Slot10() { NotRecovered(); }
void AtXBinarySpace::Slot11() { NotRecovered(); }

AtYBinarySpace::AtYBinarySpace() {}
AtYBinarySpace::~AtYBinarySpace() {}

void AtYBinarySpace::Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) {
    InsertAcross(m_posChild, m_negChild, link, radius, item, (float)((double)link->m_center.y - m_distance));
}

void AtYBinarySpace::Plane(float* out) const {
    out[0] = 0;
    out[1] = 1.0f;
    out[2] = 0;
    out[3] = -m_distance;
}

void AtYBinarySpace::Slot6() { NotRecovered(); }
void AtYBinarySpace::Slot7() { NotRecovered(); }
void AtYBinarySpace::Slot10() { NotRecovered(); }
void AtYBinarySpace::Slot11() { NotRecovered(); }

AtZBinarySpace::AtZBinarySpace() {}
AtZBinarySpace::~AtZBinarySpace() {}

void AtZBinarySpace::Insert(AtSceneLink* link, float radius, AtSceneLinkItem* item) {
    InsertAcross(m_posChild, m_negChild, link, radius, item, (float)((double)link->m_center.z - m_distance));
}

void AtZBinarySpace::Plane(float* out) const {
    out[0] = 0;
    out[1] = 0;
    out[2] = 1.0f;
    out[3] = -m_distance;
}

void AtZBinarySpace::Slot6() { NotRecovered(); }
void AtZBinarySpace::Slot7() { NotRecovered(); }
void AtZBinarySpace::Slot10() { NotRecovered(); }
void AtZBinarySpace::Slot11() { NotRecovered(); }
