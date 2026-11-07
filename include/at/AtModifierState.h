// What is in force at one point of a space graph: for each kind of modifier, the modifiers that apply there. A graph works it out when it loads, walking down from its root with a stack of these (0x000468A0), and each leaf keeps
// a copy. Names are inferred. How the renderer uses it is not recovered.
//
// Some of this sits in ranges other work holds: AtModifierSlot's functions at 0x0005B900-0x0005BA9F, and AtModifierState's Set, CopyFrom, destructor and SetHeap at 0x00053480-0x00053591. They are written in src/at/AtModifierState.cpp
// so that a graph runs on a real build. They are not recorded in config/, and are compared by the at/asm oracle through the code that calls them. Whoever recovers those ranges should fold theirs and these into one.
#pragma once

class AtModifier;

// 0x40 bytes: the modifiers of one kind, in order of priority.
struct AtModifierSlot {
    enum { kMaxEntries = 7 };
    struct Entry {
        AtModifier* m_modifier;
        int m_data;            // what was passed to Apply. CopyFrom does not copy it, and leaves what the new slot had
    };

    AtModifierSlot();                                   // 0x0005B900 seam. Clears all but the last word
    void Insert(AtModifier* modifier, int data);        // 0x0005BA20 seam. Before the first of lower priority; when full the last falls off. A new lowest is dropped when full

    unsigned int m_count;      // +0x00
    Entry m_entries[kMaxEntries];  // +0x04
    int m_unused;              // +0x3C never written
};

// 8 bytes: one slot to a kind.
class AtModifierState {
public:
    AtModifierState();                                  // 0x00018810 the same code as AtIoFileRef's, folded
    ~AtModifierState();                                 // 0x00053570 the same code as AtMatAssignment's, folded, where it is recorded
    void Init(unsigned int slots, int fromFileHeap);    // 0x000535A0
    void Assign(const AtModifierState& from);           // 0x00053720 a new array of the same size, then CopyFrom
    void CopyFrom(const AtModifierState& from);         // 0x000534D0 seam. Takes `from`'s entries for as many slots as this has
    void Set(AtModifier* modifier);                     // 0x00053520 seam. The slot of its kind, unless what holds that has a higher priority

    static void SetHeap(void* heap);                    // 0x00053480 seam

    unsigned int m_count;      // +0x00
    AtModifierSlot* m_slots;   // +0x04

    static void* s_heap;       // 0x002C7EB8 an AtHeap
};

// 0xC bytes, in the owner at +0x210. No constructor of its own is seen; the owner's, not recovered, must clear it.
struct AtModifierStack {
    void Init(unsigned int depth, unsigned int slots, int fromFileHeap);  // 0x000468A0 a second call frees the first. The second element is the bottom: the first is never used
    void Free();                                        // 0x00046860
    void Push();                                        // 0x00046830 a copy of the top. There is no check against the depth
    void Pop();                                         // 0x00046850
    AtModifierState* Top() { return &m_states[m_used]; }

    static void SetHeap(void* heap);                    // 0x00046820

    unsigned int m_depth;      // +0x00
    unsigned int m_used;       // +0x04
    AtModifierState* m_states;  // +0x08

    static void* s_heap;       // 0x002C7E04
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(AtModifierSlot) == 0x40, "AtModifierSlot");
static_assert(sizeof(AtModifierState) == 8, "AtModifierState");
static_assert(sizeof(AtModifierStack) == 0xC, "AtModifierStack");
#endif
