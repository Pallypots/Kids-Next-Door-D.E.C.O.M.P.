// The Flash player's display manager, its stack of contexts, sprite nodes' frames, labels, paths, properties and display lists, sprites' variables and the remote function calls: see include/at/AtFpDisplay.h.
// What is not recovered is in src/at/AtFpDisplaySeams.cpp.
#include "at/AtFpDisplay.h"

#include "at/AtCoordSys.h"
#include "at/AtEmitter.h"
#include "at/AtFlashLevel.h"
#include "at/AtText.h"

#include "AtSceneArray.h"

extern "C" {
AtSize strlen(const char* text);
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
char* strncpy(char* to, const char* from, AtSize count);
char* strchr(const char* text, int c);
double sqrt(double value);
double atan2(double y, double x);
double sin(double value);
double cos(double value);
}
#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

void AtFatal(const char* message, int a, int b, int c);
void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtFpRfcArgs::s_heap;
void* AtFpActionQueue::s_heap;
void* AtFpCharacterNode::s_heap;
void* AtFpRfcTable::s_heap;

// The table of remote functions as the original image has it: one name, whose function the game sets at start.
unsigned int AtFpRfcTable::s_count = 1;
AtFpRfcTable::Entry AtFpRfcTable::s_entries[1] = {{0, "PreloadMovie"}};

namespace {

// The heap the original pushes for each allocation: the object's own, else the class's; neither raises -2 and pushes null.
void PushOwnHeap(void* own, void* fallback) {
    void* heap = own != 0 ? own : fallback;
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
}

// The C library's _stricmp and _strnicmp in the "C" locale (0x00126F41, 0x00126EA0): A to Z folded to lower case, bytes compared unsigned.
int Lower(const char* p) {
    const int c = (unsigned char)*p;
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}
int CompareNoCase(const char* a, const char* b) {
    int ca, cb;
    do {
        ca = Lower(a++);
        cb = Lower(b++);
    } while (ca != 0 && ca == cb);
    return ca - cb;
}
int CompareNoCase(const char* a, const char* b, AtSize count) {
    if (count == 0) {
        return 0;
    }
    int ca, cb;
    do {
        ca = Lower(a++);
        cb = Lower(b++);
    } while (--count != 0 && ca != 0 && ca == cb);
    return ca - cb;
}

// The C library's _itoa in base 10 (0x00126092).
void IntToText(int value, char* out) {
    char digits[12];
    int n = 0;
    unsigned int u = value < 0 ? 0u - (unsigned int)value : (unsigned int)value;
    do {
        digits[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u != 0);
    if (value < 0) {
        *out++ = '-';
    }
    while (n > 0) {
        *out++ = digits[--n];
    }
    *out = 0;
}

// A node from its link, which is at +0x04: null for null.
AtFpCharacterNode* NodeOf(AtFlashLink* link) {
    return link != 0 ? (AtFpCharacterNode*)((char*)link - __builtin_offsetof(AtFpCharacterNode, m_link)) : 0;
}
AtFpSpriteCharacterNode* Sprite(void* node) {
    return (AtFpSpriteCharacterNode*)node;
}
AtFpSpriteCharacterNode* PlayerOf(AtFlashLevel* level) {
    return (AtFpSpriteCharacterNode*)(void*)level->m_player;
}
AtFlashSoundTable* SoundsOf(AtFlashLevel* level) {
    return (AtFlashSoundTable*)(void*)level->m_sounds;
}

// A dead node goes first in the environment's list.
void PushDead(AtFlashList& dead, AtFpCharacterNode* node) {
    AtFlashLink* link = &node->m_link;
    link->m_next = dead.m_head;
    if (dead.m_head != 0) {
        dead.m_head->m_prev = link;
    } else {
        dead.m_tail = link;
    }
    dead.m_head = link;
    link->m_prev = 0;
}

// "path:rest": the path copied into buffer (0x100 bytes) and rest after the colon; 0 with no colon. The original reports a path of 0x100 or more and copies it all the same, past its buffer; here it is cut.
const char* SplitPath(const char* text, char* buffer, const char* tooLong) {
    const char* colon = strchr(text, ':');
    if (colon == 0) {
        return 0;
    }
    AtSize length = (AtSize)(colon - text);
    if (length + 1 > 0x100) {
        AtFatal(tooLong, 0, 0, 0);
        length = 0xFF;
    }
    strncpy(buffer, text, length);
    buffer[length] = 0;
    return colon + 1;
}

// How many decimal digits the text starts with, as the original counts them.
int LeadingDigits(const char* text) {
    const signed char* c = (const signed char*)text;  // compared signed, as the original does
    int n = 0;
    if (c[0] >= '0') {
        while (c[n] <= '9') {
            n++;
            if (c[n] < '0') {
                break;
            }
        }
    }
    return n;
}

}  // namespace

// ---- lists --------------------------------------------------------------------------------------------------------------

void AtFlashList::Append(AtFlashLink* link) {
    link->m_prev = m_tail;
    if (m_tail != 0) {
        m_tail->m_next = link;
    } else {
        m_head = link;
    }
    m_tail = link;
    link->m_next = 0;
}

void AtFlashList::Remove(AtFlashLink* link) {
    if (link->m_prev != 0) {
        link->m_prev->m_next = link->m_next;
        if (link->m_prev->m_next == 0) {
            m_tail = link->m_prev;
        } else {
            link->m_next->m_prev = link->m_prev;
        }
    } else {
        m_head = link->m_next;
        if (m_head == 0) {
            m_tail = 0;
        } else {
            m_head->m_prev = 0;
        }
    }
}

// ---- a value's name -------------------------------------------------------------------------------------------------------

void AtFpActionScriptValue::SetName(const char* name) {
    if (m_name != 0) {
        operator delete(m_name);
        m_name = 0;
    }
    if (name != 0) {
        PushOwnHeap(m_heap, s_heap);
        m_name = (char*)operator new(strlen(name) + 1);
        AtPopHeap();
        if (m_name == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        strcpy(m_name, name);
    }
}

void AtFpActionScriptValue::SetUndefinedNamed(const char* name) {
    m_type = kTypeNone;
    SetName(name);
}

// ---- variables ----------------------------------------------------------------------------------------------------------

void AtFpRfcArgs::Construct() {
    new (m_undefined, AtPlacement()) AtFpActionScriptValue;
    m_values = 0;
    m_capacity = 0;
    m_count = 0;
    Undefined()->Clear();
    m_heap = 0;
}

void AtFpRfcArgs::Destruct() {
    Clear();
    Undefined()->~AtFpActionScriptValue();
}

void AtFpRfcArgs::Clear() {
    if (m_values != 0) {
        AtDeleteArray(m_values);
    }
    Undefined()->Reset();
    m_values = 0;
    m_capacity = 0;
    m_count = 0;
    Undefined()->Clear();
    m_heap = 0;
}

void AtFpRfcArgs::Init() {
    PushOwnHeap(m_heap, s_heap);
    m_values = AtNewArray<AtFpActionScriptValue>(10);
    AtPopHeap();
    if (m_values == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_capacity = 10;
    Undefined()->SetUndefinedNamed("UNDEFINED");
}

AtFpActionScriptValue* AtFpRfcArgs::Find(const char* name) {
    int low = 0;
    int high = m_count - 1;
    while (low <= high) {
        const int middle = (low + high) / 2;
        const int order = CompareNoCase(name, m_values[middle].m_name);
        if (order > 0) {
            low = middle + 1;
        } else if (order < 0) {
            high = middle - 1;
        } else {
            return &m_values[middle];
        }
    }
    return Undefined();
}

int AtFpRfcArgs::Get(const char* name, AtFpActionScriptValue* out) {
    if (m_capacity == 0) {
        out->CopyFrom(Undefined());
        return 0;
    }
    out->CopyFrom(Find(name));
    return out->m_type != AtFpActionScriptValue::kTypeNone;
}

void AtFpRfcArgs::Insert(AtFpActionScriptValue* value) {
    const char* name = value->m_name;
    int at = 0;
    if (m_count > 0) {
        int low = 0;
        int high = m_count - 1;
        do {
            const int middle = (low + high) / 2;
            const int order = CompareNoCase(name, m_values[middle].m_name);
            if (order > 0) {
                low = middle + 1;
            } else if (order < 0) {
                high = middle - 1;
            } else {
                // Found after all: replaced, not inserted.
                m_values[middle].CopyFrom(value);
                return;
            }
        } while (low < high);
        const int middle = (high + low) / 2;
        at = middle;
        if (CompareNoCase(name, m_values[middle].m_name) > 0) {
            at = middle + 1;
        }
    }
    const int count = m_count;
    const int capacity = (int)m_capacity;
    if (count + 1 < capacity) {
        // Moved up by their bytes, the text they point at with them; the slot left is cleared without freeing.
        for (int i = count - 1; i >= at; i--) {
            char* from = (char*)&m_values[i];
            char* to = (char*)&m_values[i + 1];
            for (unsigned int k = 0; k < sizeof(AtFpActionScriptValue); k++) {
                to[k] = from[k];
            }
        }
        m_values[at].Clear();
    } else {
        // One slot short of full: half again, at least 50 and at most 100 more.
        unsigned int more = (unsigned int)(capacity / 2);
        if (more < 50) {
            more = 50;
        } else if (more > 100) {
            more = 100;
        }
        m_capacity = more + capacity;
        PushOwnHeap(m_heap, s_heap);
        AtFpActionScriptValue* values = AtNewArray<AtFpActionScriptValue>(m_capacity);
        AtPopHeap();
        if (values == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        int gap = 0;
        for (int i = 0; i < count; i++) {
            if (i == at) {
                gap = 1;
            }
            m_values[i].MoveTo(&values[i + gap]);
        }
        if (m_values != 0) {
            AtDeleteArray(m_values);
        }
        m_values = values;
    }
    m_values[at].m_heap = m_heap != 0 ? m_heap : s_heap;
    m_values[at].CopyFrom(value);
    m_values[at].SetName(name);
    m_count++;
}

void AtFpRfcArgs::Add(const char* name, AtFpActionScriptValue* value) {
    AtFpActionScriptValue copy;
    copy.CopyFrom(value);
    copy.SetName(name);
    AtFpActionScriptValue* found = Find(copy.m_name);
    if (found == Undefined()) {
        Insert(&copy);
    } else {
        found->CopyFrom(&copy);
    }
}

// ---- remote function calls ---------------------------------------------------------------------------------------------

void AtFpRfcTable::Call(const char* name, AtFpRfcArgs* args) {
    if (m_first.m_function == 0 || m_first.m_name == 0 || s_count == 0) {
        return;
    }
    char local[0x100];
    char* translated = local;
    if (strlen(name) >= 0x100) {
        PushOwnHeap(0, s_heap);
        translated = (char*)operator new(strlen(name) + 1);
        AtPopHeap();
        if (translated == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    m_translate(name, translated);
    bool called = false;
    for (Registered* r = &m_first; r != 0; r = r->m_next) {
        if (CompareNoCase(r->m_name, translated) == 0) {
            if (r->m_function != 0) {
                r->m_function(args);
            }
            called = true;
            break;
        }
    }
    if (!called) {
        for (unsigned int i = 0; i < s_count; i++) {
            if (CompareNoCase(s_entries[i].m_name, translated) == 0 && s_entries[i].m_method != 0) {
                (this->*s_entries[i].m_method)(args);
                called = true;
                break;
            }
        }
    }
    // A name not found: the original formats "RFC %s not defined. (%s)\n" with the translated name and the name into a buffer of its own, and nothing reads it.
    if (translated != local) {
        operator delete(translated);
    }
}

// ---- the stack of contexts -----------------------------------------------------------------------------------------------

void AtFpDisplayMgrContextStack::Push(void* context, void* target) {
    if (m_count >= 50) {
        AtFatal("AtFpDisplayMgrContextStack::vPush Stack full", 0, 0, 0);
        return;
    }
    m_entries[m_count].m_context = context;
    m_entries[m_count].m_target = target;
    m_count++;
}

void AtFpDisplayMgrContextStack::GetCurrentContext(Entry* out) {
    if (m_count == 0 || m_count > 50) {
        AtFatal("AtFpDisplayMgrContextStack::vGetCurrentContext Invalid stack depth", 0, 0, 0);
    }
    out->m_context = m_entries[(int)m_count - 1].m_context;
    out->m_target = m_entries[(int)m_count - 1].m_target;
}

void AtFpDisplayMgrContextStack::SetTarget(void* target) {
    if (m_count == 0 || m_count > 50) {
        AtFatal("AtFpDisplayMgrContextStack::vSetTarget Invalid stack depth", 0, 0, 0);
    }
    m_entries[(int)m_count - 1].m_target = target;
}

AtFpSpriteCharacterNode* AtFpDisplayMgrContextStack::GetContext() {
    if (m_count == 0 || m_count > 50) {
        AtFatal("AtFpDisplayMgrContextStack::poGetCurrentTarget Invalid stack depth", 0, 0, 0);
        return 0;
    }
    return Sprite(m_entries[m_count - 1].m_context);
}

AtFpSpriteCharacterNode* AtFpDisplayMgrContextStack::GetTarget() {
    if (m_count == 0 || m_count > 50) {
        AtFatal("AtFpDisplayMgrContextStack::poGetCurrentTarget Invalid stack depth", 0, 0, 0);
        return 0;
    }
    return Sprite(m_entries[m_count - 1].m_target);
}

// ---- the queue of actions --------------------------------------------------------------------------------------------------

void AtFpActionQueue::Push(int script, AtFpSpriteCharacterNode* node) {
    Item* item;
    if (m_free.m_head == 0) {
        PushOwnHeap(m_heap, s_heap);
        item = (Item*)operator new(sizeof(Item));
        AtPopHeap();
        if (item == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    } else {
        item = (Item*)m_free.m_head;
        if (item != 0) {
            m_free.m_head = item->m_link.m_next;
            if (m_free.m_head != 0) {
                m_free.m_head->m_prev = 0;
            } else {
                m_free.m_tail = 0;
            }
        }
    }
    item->m_script = script;
    item->m_node = node;
    m_items.Append(&item->m_link);
}

// ---- sprites and scripts --------------------------------------------------------------------------------------------------

AtFlashFrame* AtFpSprite::Frame(unsigned int index) {
    if (m_refs == 0) {
        AtFatal("AtFpSprite::poGetFrame out of bounds frame index", 0, 0, 0);
    }
    if (index >= m_refCount) {
        return 0;
    }
    return m_refs[index].m_frameData;
}

const char* AtFpSprite::FrameName(unsigned int index) {
    if (m_refs == 0 || index >= m_refCount) {
        AtFatal("AtFpSprite::poGetFrame out of bounds frame index", 0, 0, 0);
    }
    return m_refs[index].m_name;
}

void AtFlashScriptTable::RunScript(unsigned int id) {
    AtFpScript* script = m_scripts[id & 0xFFFF];
    if (script != 0) {
        m_vm.Run(script);
    }
}

// ---- two floats --------------------------------------------------------------------------------------------------------------

void AtVector2::SetNormalized(const AtVector2& from) {
    const double length = sqrt((double)from.x * from.x + (double)from.y * from.y);
    if (length <= 5.0e-6f) {
        x = 1.0f;
        y = 0;
        return;
    }
    const double inverse = 1.0f / length;
    x = (float)(inverse * from.x);
    y = (float)(inverse * from.y);
}

float AtVector2::NormalizeLength() {
    const double length = sqrt((double)x * x + (double)y * y);
    if (length <= 5.0e-6f) {
        x = 1.0f;
        y = 0;
        return 0.0f;
    }
    const double inverse = 1.0f / length;
    x = (float)(inverse * x);
    y = (float)(inverse * y);
    return (float)length;
}

// ---- nodes ----------------------------------------------------------------------------------------------------------------

AtFpCharacterNode::AtFpCharacterNode() {
    m_kind = -1;
    m_character = -1;
    m_state = -1;
    m_ratio = -1;
    m_placedFrame = -1;
    m_depth = 0xFFFFFFFFu;
    m_initialized = 0;
    m_scripted = 0;
    m_unknown34 = 0;
    m_scripts = 0;
    m_parent = 0;
    m_matrices = 0;
    m_name = 0;
    for (int i = 0; i < 4; i++) {
        m_colorOffset[i] = 0;
    }
    for (int i = 0; i < 4; i++) {
        m_colorScale[i] = 1.0f;
    }
    m_heap = 0;
}

AtFlashLevel* AtFpCharacterNode::Level() {
    return (AtFlashLevel*)((char*)m_scripts - __builtin_offsetof(AtFlashLevel, m_scripts));
}

AtFpSpriteCharacterNode* AtFpCharacterNode::Root() {
    return Level()->m_root;
}

void AtFpCharacterNode::SetName(const char* name) {
    if (m_name != 0) {
        operator delete(m_name);
    }
    PushOwnHeap(m_heap, s_heap);
    m_name = (char*)operator new(strlen(name) + 1);
    AtPopHeap();
    if (m_name == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    strcpy(m_name, name);
}

AtFpBaseBranchCharacterNode::AtFpBaseBranchCharacterNode() {
    m_children.m_head = 0;
    m_children.m_tail = 0;
    m_branchInitialized = 0;
}

AtFpShapeCharacterNode::AtFpShapeCharacterNode() {
    m_unknown58 = 0;
    m_unknown5C = 0;
    m_unknown60 = 0;
}

AtFpTextCharacterNode::AtFpTextCharacterNode() {
    m_unknown58 = 0;
    m_unknown5C = 0;
}

AtFpSpriteCharacterNode::AtFpSpriteCharacterNode() {
    m_pending.m_head = 0;
    m_pending.m_tail = 0;
    m_spriteInitialized = 0;
    m_sprite = 0;
    m_playing = 0;
    m_heap = 0;
    m_frame = -1;
}

AtFpCharacterNode* AtFpBaseBranchCharacterNode::ChildAtDepth(unsigned int depth) {
    for (AtFpCharacterNode* child = NodeOf(m_children.m_head); child != 0; child = NodeOf(child->m_link.m_next)) {
        if (depth <= child->m_depth) {
            return depth == child->m_depth ? child : 0;
        }
    }
    return 0;
}

void AtFpBaseBranchCharacterNode::AddChild(AtFpCharacterNode* child) {
    AtFlashLink* link = child != 0 ? &child->m_link : 0;
    if (m_children.m_head == 0) {
        link->m_next = m_children.m_head;
        if (m_children.m_head != 0) {
            m_children.m_head->m_prev = link;
        } else {
            m_children.m_tail = link;
        }
        m_children.m_head = link;
        link->m_prev = 0;
        child->m_state = kAlive;
        return;
    }
    for (AtFpCharacterNode* at = NodeOf(m_children.m_head); at != 0; at = NodeOf(at->m_link.m_next)) {
        if (child->m_depth <= at->m_depth) {
            // Before the first as deep or deeper.
            AtFlashLink* before = &at->m_link;
            link->m_prev = before->m_prev;
            if (before->m_prev != 0) {
                before->m_prev->m_next = link;
            } else {
                m_children.m_head = link;
            }
            link->m_next = before;
            before->m_prev = link;
            child->m_state = kAlive;
            return;
        }
    }
    link->m_prev = m_children.m_tail;
    if (m_children.m_tail != 0) {
        m_children.m_tail->m_next = link;
        m_children.m_tail = link;
    } else {
        m_children.m_head = link;
        m_children.m_tail = link;
    }
    link->m_next = 0;
    child->m_state = kAlive;
}

void AtFpBaseBranchCharacterNode::Kill(AtFpCharacterNode* node) {
    node->m_state = kDead;
    PushDead(Level()->m_environment->m_dead, node);
}

void AtFpBaseBranchCharacterNode::RemoveAllChildren() {
    while (m_children.m_head != 0) {
        AtFlashLink* link = m_children.m_head;
        m_children.m_head = link->m_next;
        if (m_children.m_head != 0) {
            m_children.m_head->m_prev = 0;
        } else {
            m_children.m_tail = 0;
        }
        AtFpCharacterNode* child = NodeOf(link);
        switch (child->m_kind) {
        case kShape:
        case kText:
            break;
        case kSprite:
            ((AtFpBaseBranchCharacterNode*)child)->RemoveAllChildren();
            break;
        default:
            AtFatal("AtFpBaseBranchCharacterNode::vRemoveAllChildrenToDeadNodeList Unexpected Node type", 0, 0, 0);
            break;
        }
        child->m_state = kDead;
        PushDead(Level()->m_environment->m_dead, child);
    }
}

void AtFpBaseBranchCharacterNode::RemoveChild(AtFpCharacterNode* child) {
    switch (child->m_kind) {
    case kShape:
    case kText:
        break;
    case kSprite:
        ((AtFpBaseBranchCharacterNode*)child)->RemoveAllChildren();
        break;
    default:
        AtFatal("AtFpBaseBranchCharacterNode::vRemoveChildToDeadNodeList Unexpected Node type", 0, 0, 0);
        break;
    }
    AtFlashLink* link = &child->m_link;
    if (link->m_prev != 0) {
        link->m_prev->m_next = link->m_next;
        if (link->m_prev->m_next == 0) {
            m_children.m_tail = link->m_prev;
        } else {
            link->m_next->m_prev = link->m_prev;
        }
    } else {
        m_children.m_head = link->m_next;
        if (m_children.m_head != 0) {
            m_children.m_head->m_prev = 0;
        } else {
            m_children.m_tail = 0;
        }
    }
    child->m_state = kDead;
    PushDead(Level()->m_environment->m_dead, child);
}

// ---- a sprite's timeline -------------------------------------------------------------------------------------------------

int AtFpSpriteCharacterNode::FindLabel(const char* label) {
    const int count = (int)m_sprite->m_refCount;
    for (int i = 0; i < count; i++) {
        const char* name = m_sprite->FrameName((unsigned int)i);
        if (name != 0 && CompareNoCase(name, label) == 0) {
            return i;
        }
    }
    return -1;
}

void AtFpSpriteCharacterNode::ExecuteFrame(int frame) {
    AtFlashFrame* f = m_sprite->Frame((unsigned int)frame);
    if (f == 0) {
        return;
    }
    const int count = (int)f->m_tagCount;
    for (int i = 0; i < count; i++) {
        AtFlashTag* tag = f->m_tags[i];
        if (tag->m_type == 4) {
            m_scripts->RunScript((unsigned int)((AtFlashActionTag*)tag)->m_id);
        }
    }
}

void AtFpSpriteCharacterNode::GotoLabel(const char* label) {
    GotoFrame(FindLabel(label));
}

void AtFpSpriteCharacterNode::ExecuteLabel(const char* label) {
    ExecuteFrame(FindLabel(label));
}

void AtFpSpriteCharacterNode::RemoveChildrenAfter(int frame) {
    AtFpCharacterNode* child = NodeOf(m_children.m_head);
    while (child != 0) {
        if (child->m_placedFrame > frame) {
            AtFpCharacterNode* removed = child;
            child = NodeOf(child->m_link.m_next);
            RemoveChild(removed);
        } else {
            child = NodeOf(child->m_link.m_next);
        }
    }
}

void AtFpSpriteCharacterNode::FlushPending() {
    for (;;) {
        AtFlashLink* link = m_pending.m_head;
        if (link == 0) {
            return;
        }
        m_pending.m_head = link->m_next;
        if (m_pending.m_head != 0) {
            m_pending.m_head->m_prev = 0;
        } else {
            m_pending.m_tail = 0;
        }
        AtFpCharacterNode* node = NodeOf(link);
        if (node == 0) {
            return;
        }
        node->Acquire();
        node->Update();
        AddChild(node);
    }
}

AtFpCharacterNode* AtFpSpriteCharacterNode::FindPending(unsigned int depth) {
    for (AtFpCharacterNode* node = NodeOf(m_pending.m_head); node != 0; node = NodeOf(node->m_link.m_next)) {
        if (node->m_depth == depth) {
            return node;
        }
    }
    return 0;
}

AtFpCharacterNode* AtFpSpriteCharacterNode::MakeNode(int kind, unsigned int depth) {
    AtFpCharacterNode* node;
    switch (kind) {
    case kShape:
        PushOwnHeap(m_heap, s_heap);
        node = AtNewObject<AtFpShapeCharacterNode>();
        break;
    case kSprite:
        PushOwnHeap(m_heap, s_heap);
        node = AtNewObject<AtFpSpriteCharacterNode>();
        break;
    case kText:
        PushOwnHeap(m_heap, s_heap);
        node = AtNewObject<AtFpTextCharacterNode>();
        break;
    default:
        AtFatal("AtFpSpriteCharacterNode::vExecutePlaceObject2 unexpected character type", 0, 0, 0);
        return 0;
    }
    AtPopHeap();
    if (node == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    node->m_depth = depth;
    node->m_heap = m_heap != 0 ? m_heap : s_heap;
    return node;
}

void AtFpSpriteCharacterNode::ExecutePlace(AtFpPlaceObject2Tag* tag) {
    AtFpCharacterNode* node = 0;
    const unsigned int depth = tag->m_unknown28;
    switch (tag->m_unknown30) {
    case 0: {
        // A new node, unless one is at that depth already; it waits among the pending until the frames are done.
        const int kind = tag->m_unknown50;
        const int character = tag->m_unknown54;
        node = ChildAtDepth(depth);
        if (node == 0) {
            node = MakeNode(kind, depth);
            m_pending.Append(node != 0 ? &node->m_link : 0);
            node->Init(kind, character, this, m_scripts, m_frame, 0);
        }
        break;
    }
    case 1:
        node = ChildAtDepth(depth);
        if (node == 0) {
            node = FindPending(depth);
        }
        break;
    case 2: {
        node = ChildAtDepth(depth);
        if (node == 0) {
            node = FindPending(depth);
        }
        const int kindAndCharacter[2] = {tag->m_unknown50, tag->m_unknown54};
        node->Replace(kindAndCharacter, m_frame);
        break;
    }
    default:
        break;
    }
    const unsigned char flags = tag->m_unknown34;
    if ((flags & 1) != 0 && node->m_scripted == 0) {
        float transform[6];
        for (int i = 0; i < 6; i++) {
            ((unsigned int*)transform)[i] = tag->m_transform[i];
        }
        node->SetMatrix(transform);
    }
    if ((flags & 2) != 0) {
        for (int i = 0; i < 4; i++) {
            node->m_colorOffset[i] = tag->m_colorOffset[i];
        }
        for (int i = 0; i < 4; i++) {
            node->m_colorScale[i] = tag->m_colorScale[i];
        }
    }
    if ((flags & 8) != 0) {
        node->m_ratio = tag->m_unknown2A;
    }
    if ((flags & 4) != 0) {
        node->SetName(tag->m_name);
        return;
    }
    // A new or replaced sprite with no name of its own is named "instance" and the level's count.
    const int type = tag->m_unknown30;
    if ((type == 0 || type == 2) && node->m_kind == kSprite && node->m_name == 0) {
        char name[64] = "instance";
        char digits[16];
        AtFlashLevel* level = Level();
        IntToText(level->m_read, digits);
        strcat(name, digits);
        node->SetName(name);
        level->m_read++;
    }
}

void AtFpSpriteCharacterNode::GotoFrame(int target) {
    if (target < 0 || (unsigned int)target >= m_sprite->m_refCount) {
        return;
    }
    if (m_frame > target) {
        // Backwards: what was placed after it goes, and the frames run again from the first.
        RemoveChildrenAfter(target);
        m_frame = -1;
    }
    if (m_frame >= target) {
        return;
    }
    do {
        m_frame++;
        AtFlashFrame* frame = m_sprite->Frame((unsigned int)m_frame);
        const unsigned int count = frame->m_tagCount;
        for (unsigned int i = 0; i < count; i++) {
            AtFlashTag* tag = frame->m_tags[i];
            switch (tag->m_type) {
            case 1: {
                AtFpPlaceObject2Tag* place = (AtFpPlaceObject2Tag*)tag;
                AtFpCharacterNode* child = ChildAtDepth(place->m_unknown28);
                if (child == 0 || m_frame >= child->m_placedFrame) {
                    ExecutePlace(place);
                }
                break;
            }
            case 2: {
                const unsigned int depth = (unsigned int)((AtFlashRemoveTag*)tag)->m_id;
                AtFpCharacterNode* child = ChildAtDepth(depth);
                if (child != 0) {
                    if (m_frame > child->m_placedFrame) {
                        RemoveChild(ChildAtDepth(depth));
                    }
                } else {
                    // Not placed yet: taken from the pending. A depth in neither faults on its null link.
                    AtFpCharacterNode* pending = FindPending(depth);
                    m_pending.Remove(pending != 0 ? &pending->m_link : 0);
                    Kill(pending);
                }
                break;
            }
            case 3:
                if (m_frame == target) {
                    SoundsOf(Level())->Play(((AtFlashSoundTag*)tag)->m_id);
                }
                break;
            case 4:
                if (m_frame == target) {
                    Level()->m_environment->m_actions.Push(((AtFlashActionTag*)tag)->m_id, this);
                }
                break;
            default:
                break;
            }
        }
        if (m_frame >= target - 1) {
            FlushPending();
        }
    } while (m_frame < target);
}

// ---- paths and properties -------------------------------------------------------------------------------------------------

AtFpSpriteCharacterNode* AtFpSpriteCharacterNode::FindChild(const char* path, int matchSelf, int* found) {
    AtFpSpriteCharacterNode* result = 0;
    *found = 0;
    const char* rest = path;
    if (matchSelf == 0) {
        *found = 1;
    } else {
        const AtSize length = strlen(m_name);
        if (CompareNoCase(path, m_name, length) == 0) {
            const char* after = path + length;
            if (*after == 0) {
                *found = 1;
                return this;
            }
            if (*after == '/') {
                if (after[1] == 0) {
                    *found = 1;
                    return this;
                }
                *found = 1;
                rest = after + 1;
            }
        }
        if (*found == 0) {
            return 0;
        }
    }
    // The first sprite child whose name matches decides, whether or not the rest is found under it.
    int childFound = 0;
    for (AtFpCharacterNode* child = NodeOf(m_children.m_head); child != 0; child = NodeOf(child->m_link.m_next)) {
        if (childFound != 0) {
            break;
        }
        if (child->m_kind == kSprite) {
            result = ((AtFpSpriteCharacterNode*)child)->FindChild(rest, 1, &childFound);
        }
    }
    return result;
}

void AtFpSpriteCharacterNode::TargetPath(char* out, unsigned int size, unsigned int* length) {
    if (m_parent == 0) {
        return;
    }
    ((AtFpSpriteCharacterNode*)m_parent)->TargetPath(out, size, length);
    const unsigned int room = (unsigned int)strlen(m_name) + 1;
    if (*length + room + 1 >= size) {
        AtFatal("AtFpSpriteCharacterNode::vGetTargetPath Exceeded array bounds", 0, 0, 0);
    }
    out[*length] = '/';
    out[*length + 1] = 0;
    strcpy(out + *length + 1, m_name);
    *length += room;
}

void AtFpSpriteCharacterNode::GetProperty(int index, AtFpActionScriptValue* out) {
    switch (index) {
    case 0: {
        const float x = g_atMatrix.GetElement(m_matrices->At(0), 3, 0);
        out->SetReal(&x);
        break;
    }
    case 1: {
        const float y = g_atMatrix.GetElement(m_matrices->At(0), 3, 1);
        out->SetReal(&y);
        break;
    }
    case 2:
    case 3: {
        // The length of the first or second row, as a percentage.
        const int row = index - 2;
        const float a = g_atMatrix.GetElement(m_matrices->At(0), row, 0);
        const double b = g_atMatrix.GetElement(m_matrices->At(0), row, 1);
        const float scale = (float)(sqrt(b * b + (double)a * a) * 100.0f);
        out->SetReal(&scale);
        break;
    }
    case 4:
        // From 0, unlike Flash's.
        out->SetInt(&m_frame);
        break;
    case 5:
    case 12: {
        const int frames = (int)m_sprite->m_refCount;
        out->SetInt(&frames);
        break;
    }
    case 10: {
        const float a = g_atMatrix.GetElement(m_matrices->At(0), 0, 0);
        const double b = g_atMatrix.GetElement(m_matrices->At(0), 0, 1);
        const float radians = (float)atan2(b, (double)a);
        out->SetReal(&radians);
        const float degrees = (float)((double)out->Real() * 57.2957763671875f);
        out->SetReal(&degrees);
        break;
    }
    case 11:
        if (m_parent == 0) {
            char root[2] = {'/', 0};
            out->SetText(root);
        } else {
            char path[0x400];
            unsigned int length = 0;
            TargetPath(path, 0x400, &length);
            out->SetText(path);
        }
        break;
    case 13:
        if (m_parent == 0) {
            out->m_type = AtFpActionScriptValue::kTypeNone;
        } else {
            out->SetText(m_name);
        }
        break;
    default:
        out->m_type = AtFpActionScriptValue::kTypeNone;
        break;
    }
}

void AtFpSpriteCharacterNode::SetProperty(int index, AtFpActionScriptValue* value) {
    AtMatrix* m = m_matrices->At(0);
    AtVector3 position;
    AtVector2 first;   // the first row as it is to be
    AtVector2 second;  // the second row's (y, -x)
    switch (index) {
    case 0:
        g_atMatrix.SetElement(m, 3, 0, value->Real());
        m_scripted = 1;
        return;
    case 1:
        g_atMatrix.SetElement(m, 3, 1, value->Real());
        m_scripted = 1;
        return;
    case 2: {
        // X scale: the first row becomes the second's perpendicular, at the length asked for.
        g_atMatrix.GetPosition(m, &position);
        const float m10 = g_atMatrix.GetElement(m, 1, 0);
        const double m11 = g_atMatrix.GetElement(m, 1, 1);
        second.x = (float)m11;
        second.y = -m10;
        first.SetNormalized(second);
        const double scale = (double)value->Real() * 0.01f;
        first.x = (float)(first.x * scale);
        first.y = (float)(scale * first.y);
        break;
    }
    case 3: {
        // Y scale: the second row becomes the first's perpendicular.
        g_atMatrix.GetPosition(m, &position);
        first.x = g_atMatrix.GetElement(m, 0, 0);
        first.y = g_atMatrix.GetElement(m, 0, 1);
        second.SetNormalized(first);
        const double scale = (double)value->Real() * 0.01f;
        second.x = (float)(second.x * scale);
        second.y = (float)(scale * second.y);
        break;
    }
    case 10: {
        // Rotation in degrees, each row keeping its length.
        g_atMatrix.GetPosition(m, &position);
        AtVector2 a;
        a.x = g_atMatrix.GetElement(m, 0, 0);
        a.y = g_atMatrix.GetElement(m, 0, 1);
        const float lengthX = a.NormalizeLength();
        AtVector2 b;
        b.x = g_atMatrix.GetElement(m, 1, 0);
        b.y = g_atMatrix.GetElement(m, 1, 1);
        const float lengthY = b.NormalizeLength();
        const double angle = (double)value->Real() * 3.1415927f * 0.0055555556900799274f;
        const float sine = (float)sin(angle);
        const float cosine = (float)cos(angle);
        g_atMatrix.Identity(m);
        g_atMatrix.SetTranslation(m, &position);
        g_atMatrix.SetElement(m, 0, 0, (float)((double)cosine * lengthX));
        g_atMatrix.SetElement(m, 0, 1, (float)((double)sine * lengthX));
        g_atMatrix.SetElement(m, 1, 0, (float)(-(double)sine * lengthY));
        g_atMatrix.SetElement(m, 1, 1, (float)((double)cosine * lengthY));
        m_scripted = 1;
        return;
    }
    default:
        return;
    }
    g_atMatrix.Identity(m);
    g_atMatrix.SetTranslation(m, &position);
    g_atMatrix.SetElement(m, 0, 0, first.x);
    g_atMatrix.SetElement(m, 0, 1, first.y);
    g_atMatrix.SetElement(m, 1, 0, -second.y);
    g_atMatrix.SetElement(m, 1, 1, second.x);
    m_scripted = 1;
}

// ---- the display manager --------------------------------------------------------------------------------------------------

int AtFpDisplayManager::GetCurrentFrame() {
    AtFpSpriteCharacterNode* target = m_contexts.GetTarget();
    if (target == 0) {
        AtFatal("AtFpDisplayManager::nGetCurrentFrame current target is NULL", 0, 0, 0);
        return -1;
    }
    return target->m_frame;
}

void AtFpDisplayManager::GotoFrame(int frame) {
    AtFpSpriteCharacterNode* target = m_contexts.GetTarget();
    if (target == 0) {
        AtFatal("AtFpDisplayManager::vGotoFrameByNumber current target is NULL", 0, 0, 0);
        return;
    }
    target->GotoFrame(frame);
}

void AtFpDisplayManager::SetPlaying(int playing) {
    AtFpSpriteCharacterNode* target = m_contexts.GetTarget();
    if (target == 0) {
        AtFatal("AtFpDisplayManager::vGotoFrameByLabel current target is NULL", 0, 0, 0);
        return;
    }
    target->m_playing = playing;
}

void AtFpDisplayManager::StopSounds() {
    for (AtFlashLevel* level = (AtFlashLevel*)m_active.m_head; level != 0; level = (AtFlashLevel*)level->m_link.m_next) {
        SoundsOf(level)->StopAll();
    }
}

void AtFpDisplayManager::GotoFrameByLabel(const char* label) {
    AtFpSpriteCharacterNode* target = m_contexts.GetTarget();
    if (target == 0) {
        AtFatal("AtFpDisplayManager::vGotoFrameByLabel current target is NULL", 0, 0, 0);
        return;
    }
    target->GotoFrame(target->FindLabel(label));
}

AtFpSpriteCharacterNode* AtFpDisplayManager::ResolvePath(const char* path, int fromContext) {
    if (m_contexts.m_count == 0) {
        AtFatal("Cannot resolve path without a context on the stack", 0, 0, 0);
        return 0;
    }
    AtFpDisplayMgrContextStack::Entry current;
    m_contexts.GetCurrentContext(&current);
    if (path == 0 || *path == 0) {
        return fromContext != 0 ? Sprite(current.m_context)->Root() : Sprite(current.m_target);
    }
    AtFpSpriteCharacterNode* node = 0;
    int found;
    if (*path == '/') {
        path++;
        if (*path == 0) {
            node = Sprite(current.m_target)->Root()->Root();
        } else {
            node = Sprite(current.m_target)->Root()->FindChild(path, 0, &found);
        }
        if (node != 0) {
            return node;
        }
    }
    // Each level's root, by its name first: "_level0/a".
    found = 0;
    for (AtFlashLevel* level = (AtFlashLevel*)m_active.m_head; level != 0; level = (AtFlashLevel*)level->m_link.m_next) {
        if (found != 0) {
            return node;
        }
        node = PlayerOf(level)->FindChild(path, 1, &found);
    }
    if (found != 0) {
        return node;
    }
    AtFpSpriteCharacterNode* from = Sprite(fromContext != 0 ? current.m_context : current.m_target);
    if (from == 0) {
        AtFatal("AtFpDisplayManager::poResolvePath current context is NULL", 0, 0, 0);
        return 0;
    }
    if (from->m_state != AtFpCharacterNode::kAlive) {
        return 0;
    }
    return from->FindChild(path, 0, &found);
}

int AtFpDisplayManager::ExecuteScripts(int frame) {
    AtFpSpriteCharacterNode* target = m_contexts.GetTarget();
    if (target == 0) {
        AtFatal("AtFpDisplayManager::vExecuteScripts current target is NULL", 0, 0, 0);
        return 1;
    }
    m_contexts.Push(target, target);
    target->ExecuteFrame(frame);
    m_contexts.m_count--;
    AtFpSpriteCharacterNode* context = m_contexts.GetContext();
    if (context == 0 || context->m_state == AtFpCharacterNode::kDead) {
        return 1;
    }
    return 0;
}

int AtFpDisplayManager::ExecuteScripts(const char* label) {
    char buffer[0x100];
    AtFpSpriteCharacterNode* node;
    const char* frame = SplitPath(label, buffer, "AtFpDisplayManager::vGotoFrameByExpression Path Copy Buffer too small");
    if (frame != 0) {
        node = ResolvePath(buffer, 0);
    } else {
        frame = label;
        if (m_contexts.m_count == 0) {
            AtFatal("Cannot resolve path without a context on the stack", 0, 0, 0);
            return 1;
        }
        AtFpDisplayMgrContextStack::Entry current;
        m_contexts.GetCurrentContext(&current);
        node = Sprite(current.m_target);
    }
    if (node != 0 && node->m_state == AtFpCharacterNode::kAlive && frame != 0 && *frame != 0) {
        const int length = (int)strlen(frame);
        const int digits = LeadingDigits(frame);
        m_contexts.Push(node, node);
        if (digits >= length) {
            node->ExecuteFrame(AtNarrowToInt(frame) - 1);
        } else {
            node->ExecuteLabel(frame);
        }
        m_contexts.m_count--;
        node = m_contexts.GetContext();
    }
    if (node == 0 || node->m_state == AtFpCharacterNode::kDead) {
        return 1;
    }
    return 0;
}

void AtFpDisplayManager::GetProperty(const char* target, int index, AtFpActionScriptValue* out) {
    AtFpSpriteCharacterNode* node = ResolvePath(target, 0);
    if (node != 0 && node->m_state != AtFpCharacterNode::kDead) {
        node->GetProperty(index, out);
    } else {
        out->m_type = AtFpActionScriptValue::kTypeNone;
    }
}

void AtFpDisplayManager::SetProperty(const char* target, int index, AtFpActionScriptValue* value) {
    AtFpSpriteCharacterNode* node = ResolvePath(target, 0);
    if (node != 0 && node->m_state != AtFpCharacterNode::kDead) {
        node->SetProperty(index, value);
    }
}

void AtFpDisplayManager::SetTarget(const char* path) {
    AtFpDisplayMgrContextStack::Entry current;
    m_contexts.GetCurrentContext(&current);
    AtFpSpriteCharacterNode* context = Sprite(current.m_context);
    if (context == 0) {
        AtFatal("AtFpDisplayManager::vSetTarget Can't set target with an invalid context", 0, 0, 0);
        m_contexts.SetTarget(0);
        return;
    }
    AtFpSpriteCharacterNode* target;
    if (context->m_state == AtFpCharacterNode::kDead) {
        target = context->Root();
    } else {
        target = ResolvePath(path, 1);
        if (target == 0 || target->m_state == AtFpCharacterNode::kDead) {
            target = context->Root();
        }
    }
    m_contexts.SetTarget(target);
}

int AtFpDisplayManager::GetVariable(const char* name, AtFpActionScriptValue* out) {
    char buffer[0x100];
    AtFpSpriteCharacterNode* node;
    const char* variable = SplitPath(name, buffer, "AtFpDisplayManager::bGetVariable Path Copy Buffer too small");
    if (variable != 0) {
        node = ResolvePath(buffer, 0);
    } else {
        variable = name;
        if (m_contexts.m_count == 0) {
            AtFatal("Cannot resolve path without a context on the stack", 0, 0, 0);
            out->m_type = AtFpActionScriptValue::kTypeNone;
            return 0;
        }
        AtFpDisplayMgrContextStack::Entry current;
        m_contexts.GetCurrentContext(&current);
        node = Sprite(current.m_target);
    }
    if (node == 0 || node->m_state == AtFpCharacterNode::kDead) {
        out->m_type = AtFpActionScriptValue::kTypeNone;
        return 0;
    }
    return node->m_variables.Get(variable, out);
}

int AtFpDisplayManager::SetVariable(const char* name, AtFpActionScriptValue* value) {
    char buffer[0x100];
    AtFpSpriteCharacterNode* node;
    const char* variable = SplitPath(name, buffer, "AtFpDisplayManager::bSetVariable Path Copy Buffer too small");
    if (variable != 0) {
        node = ResolvePath(buffer, 0);
    } else {
        variable = name;
        if (m_contexts.m_count == 0) {
            AtFatal("Cannot resolve path without a context on the stack", 0, 0, 0);
            return 0;
        }
        AtFpDisplayMgrContextStack::Entry current;
        m_contexts.GetCurrentContext(&current);
        node = Sprite(current.m_target);
    }
    if (node != 0 && node->m_state != AtFpCharacterNode::kDead) {
        node->m_variables.Add(variable, value);
    }
    return 0;
}

void AtFpDisplayManager::GotoFrameByExpression(const char* expression) {
    char buffer[0x100];
    AtFpSpriteCharacterNode* node;
    const char* frame = SplitPath(expression, buffer, "AtFpDisplayManager::vGotoFrameByExpression Path Copy Buffer too small");
    if (frame != 0) {
        node = ResolvePath(buffer, 0);
    } else {
        frame = expression;
        if (m_contexts.m_count == 0) {
            AtFatal("Cannot resolve path without a context on the stack", 0, 0, 0);
            return;
        }
        AtFpDisplayMgrContextStack::Entry current;
        m_contexts.GetCurrentContext(&current);
        node = Sprite(current.m_target);
    }
    if (node == 0 || node->m_state != AtFpCharacterNode::kAlive || frame == 0 || *frame == 0) {
        return;
    }
    const int length = (int)strlen(frame);
    if (LeadingDigits(frame) >= length) {
        node->GotoFrame(AtNarrowToInt(frame) - 1);
    } else {
        node->GotoLabel(frame);
    }
}

AtFpRfcArgs* AtFpDisplayManager::ResolveTarget(const char* path) {
    // Not checked: a path naming nothing gives the variables of a null node.
    AtFpSpriteCharacterNode* node = ResolvePath(path, 0);
    return (AtFpRfcArgs*)((char*)node + __builtin_offsetof(AtFpSpriteCharacterNode, m_variables));
}

void AtFpDisplayManager::LoadMovie(const char* name, const char* target) {
    const int number = AtNarrowToInt(target + 6);  // past "_level"
    if (m_maxLevel < (unsigned int)number) {
        return;
    }
    char shortName[8];
    AtCopyName(name, shortName);  // copied, and not used
    if (*name == 0) {
        UnloadLevel(number);
        return;
    }
    AtFlashLevel* level = FindLoadedLevel(name);
    if (level == 0) {
        level = NewLevel(name, 0);
    } else {
        m_loaded.Remove(&level->m_link);
    }
    AttachLevel(level, number);
}
