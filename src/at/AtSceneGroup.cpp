// GroupActor, LodGroupActor and SwitchGroupActor. See include/at/AtSceneNodes.h.
#include "at/AtSceneNodes.h"

#include "AtArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

namespace {

const unsigned short kClassGroup = 11;
const unsigned short kClassSwitchGroup = 12;
const unsigned short kClassLodGroup = 13;

void NotRecovered() {
    AtFatal("Atlas Scene: this group slot is not recovered", 0, 0, 0);
}

// Inline in both readers: a node by name, in the owner's tables of nodes.
AtSceneNode* FindNodeInline(AtNameTableOwner* owner, const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return (AtSceneNode*)owner->m_nodeSpaces.Find(ns, name);
    }
    return (AtSceneNode*)owner->m_nodes.Find(name);
}

// Room for count pointers on the actors' heap. Reports having no heap, and no memory.
void* NewPointers(unsigned int count) {
    if (AtSceneActor::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtSceneActor::s_heap);
    void* block = operator new(count * 4);
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return block;
}

}  // namespace

// ---- the group -----------------------------------------------------------------------------------------------------------------

AtSceneGroupActor::AtSceneGroupActor() {
    m_readOwner = 0;
    m_capacity = 0;
    m_count = 0;
    m_children = 0;
    m_class = kClassGroup;
}

AtSceneGroupActor::~AtSceneGroupActor() {
    AtSceneGroupActor::Clear();
}

void AtSceneGroupActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    m_readOwner = owner;
    AtSceneActor::Read(owner, scene, tokenizer);
}

void AtSceneGroupActor::Clear() {
    if (m_children != 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            m_children[i]->StopSharingCoordSys();
        }
        operator delete(m_children);
    }
    AtSceneActor::Clear();
    m_readOwner = 0;
    m_capacity = 0;
    m_count = 0;
    m_children = 0;
}

void AtSceneGroupActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    if (token != kAtWordActor) {
        AtSceneActor::ReadBody(owner, tokenizer, token);
        return;
    }
    if (m_children == 0) {
        tokenizer->ExpectLeftBracket();
        m_capacity = (unsigned int)tokenizer->ExpectInt();
        tokenizer->ExpectRightBracket();
        m_children = (AtSceneActor**)NewPointers(m_capacity);
        if (tokenizer->Next() == AtIoTokenizer::kLeftCurly) {
            m_count = m_capacity;
        } else {
            // No list after the count: the token goes back, and nothing more is read, not even a right curly.
            tokenizer->m_pushedBack = 1;
            m_count = 0;
        }
    } else {
        m_count = (unsigned int)tokenizer->ExpectCount();
        if (m_count > m_capacity) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
    }
    // A list of none leaves its right curly unread, for the body to take as its own end.
    if (m_count == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_count; i++) {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_children[i] = AtSceneActor::AsActor(FindNodeInline(owner, name, ns));
        if (m_children[i] == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        if (m_children[i]->m_sharedFrom != 0) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
        m_children[i]->ShareCoordSys(this);
    }
    tokenizer->ExpectRightCurly();
}

void AtSceneGroupActor::Slot12() { NotRecovered(); }
void AtSceneGroupActor::Slot14() { NotRecovered(); }
void AtSceneGroupActor::Slot15() { NotRecovered(); }
void AtSceneGroupActor::Slot34() { NotRecovered(); }
void AtSceneGroupActor::Slot38() { NotRecovered(); }
void AtSceneGroupActor::Slot39() { NotRecovered(); }
void AtSceneGroupActor::Slot42() { NotRecovered(); }

// ---- the level-of-detail group ------------------------------------------------------------------------------------------------

AtSceneLodGroupActor::AtSceneLodGroupActor() {
    m_distances = 0;
    m_unknownAC = 0;
    m_class = kClassLodGroup;
}

AtSceneLodGroupActor::~AtSceneLodGroupActor() {
    AtSceneLodGroupActor::Clear();
}

void AtSceneLodGroupActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneGroupActor::Read(owner, scene, tokenizer);
}

void AtSceneLodGroupActor::Clear() {
    if (m_distances != 0) {
        operator delete(m_distances);
    }
    AtSceneGroupActor::Clear();
    m_distances = 0;
    m_unknownAC = 0;
}

void AtSceneLodGroupActor::Insert(AtSceneActor* child, float distanceSquared) {
    unsigned int at = 0;
    // Past every one nearer, and past any distance that does not compare.
    while (at < m_count && !(distanceSquared <= m_distances[at])) {
        at++;
    }
    for (unsigned int i = m_count; i > at; i--) {
        m_children[i] = m_children[i - 1];
        m_distances[i] = m_distances[i - 1];
    }
    m_children[at] = child;
    m_distances[at] = distanceSquared;
    child->ShareCoordSys(this);
    m_count++;
}

void AtSceneLodGroupActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    if (token != kAtWordLod) {
        AtSceneActor::ReadBody(owner, tokenizer, token);
        return;
    }
    unsigned int listed;
    if (m_children == 0) {
        tokenizer->ExpectLeftBracket();
        m_capacity = (unsigned int)tokenizer->ExpectInt();
        tokenizer->ExpectRightBracket();
        m_children = (AtSceneActor**)NewPointers(m_capacity);
        m_distances = (float*)NewPointers(m_capacity);
        if (tokenizer->Next() != AtIoTokenizer::kLeftCurly) {
            tokenizer->m_pushedBack = 1;
            return;
        }
        listed = m_capacity;
    } else {
        listed = (unsigned int)tokenizer->ExpectCount();
        if (listed > m_capacity) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
    }
    if (listed == 0) {
        return;
    }
    for (unsigned int i = 0; i < listed; i++) {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtSceneActor* child = AtSceneActor::AsActor(FindNodeInline(owner, name, ns));
        if (child == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        if (child->m_sharedFrom != 0) {
            tokenizer->Error(kAtProblemIsEmpty);
        }
        const float distance = tokenizer->ExpectReal();
        Insert(child, distance * distance);
    }
    tokenizer->ExpectRightCurly();
}

void AtSceneLodGroupActor::Slot14() { NotRecovered(); }
void AtSceneLodGroupActor::Slot15() { NotRecovered(); }
void AtSceneLodGroupActor::Slot38() { NotRecovered(); }
void AtSceneLodGroupActor::Slot39() { NotRecovered(); }
void AtSceneLodGroupActor::Slot42() { NotRecovered(); }

// ---- the switch group ---------------------------------------------------------------------------------------------------------

AtSceneSwitchGroupActor::AtSceneSwitchGroupActor() {
    m_class = kClassSwitchGroup;
    m_switch = 0;
}

AtSceneSwitchGroupActor::~AtSceneSwitchGroupActor() {
    AtSceneGroupActor::Clear();
}

void AtSceneSwitchGroupActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneGroupActor::Read(owner, scene, tokenizer);
}

void AtSceneSwitchGroupActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    if (token == kAtWordSwitch) {
        m_switch = tokenizer->ExpectInt();
        return;
    }
    AtSceneGroupActor::ReadBody(owner, tokenizer, token);
}

void AtSceneSwitchGroupActor::Slot12() { NotRecovered(); }
void AtSceneSwitchGroupActor::Slot14() { NotRecovered(); }
void AtSceneSwitchGroupActor::Slot15() { NotRecovered(); }
void AtSceneSwitchGroupActor::Slot34() { NotRecovered(); }
void AtSceneSwitchGroupActor::Slot38() { NotRecovered(); }
void AtSceneSwitchGroupActor::Slot39() { NotRecovered(); }
void AtSceneSwitchGroupActor::Slot42() { NotRecovered(); }
