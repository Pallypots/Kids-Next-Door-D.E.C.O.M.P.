// The AI action manager that enemies use, its actions and pattern, and the three behaviour-chart commands that drive it. Original address clusters 0x0017CDD0-0x0017D6C4 (the manager),
// 0x0016C0C0-0x0016C263 (the commands), and two small readers: 0x0014BAC0 (GmAcNode's) and 0x00067CF0 (AtDataRegistry's). Also 0x00014030, an engine text routine.
//
// Names. Every class, member and method name here is inferred, and says so by being descriptive. The record words (AIActions, DefaultAIAction, DefaultAIPattern, Repeatable, NextAction) and the
// chart commands' names (bNPCChooseAIAction, vNPCStoreAIAction, bNPCHasAIAction, registered by 0x0016FD60 through 0x000754D0) are the executable's own.
//
// What it does. An NPC whose record says CreateAiActionManager makes one (GmAcNPC's slot 15, include/gm/GmAcNPC.h) and keeps it at +0x304. Its actions are the record's AIActions list, a short name
// array; each action is a record of that name under the list, whose own value is its weight, with a Repeatable boolean, a NextAction short name, and any number of real children: modifiers, each a
// condition name the NPC answers (its slot 60, TestCondition, include/gm/GmAcNPC.h) and a factor in [0, 1] the weight is multiplied by when the condition holds. The behaviour chart asks it for the
// next action (bNPCChooseAIAction): the pattern's current entry if a pattern is set; else the action the last one names as its next; else a weighted random pick over the enabled actions, not
// repeating the last unless it is Repeatable (and, if that leaves nothing, allowing repeats); else DefaultAIAction. The chart then tells the NPC (its slot 49), and reports back which action ran
// (vNPCStoreAIAction), which moves the pattern on when it is the pattern's entry. Script events elsewhere enable and disable actions and set the pattern (0x00174610, 0x00174650, 0x00174690: not
// recovered, see docs/worklog/2026-11-19-gmai.md).
#pragma once

class AtDataComponent;
class AtDataLink;
class AtDataRegistry;
class AtDataShortNameArray;
class GmAcActor;
class GmAcNode;
class GmAcNPC;

// ---- small readers of classes declared elsewhere ---------------------------------------------------------------------------------
// 0x00014030, an engine text routine with no owner yet: the length of an eight-byte name, 0 to 8. Recovered in src/gm/GmAiActionManager.cpp for this work; it belongs with the text routines.
int AtNameLength8(const char* name);

// 0x00067CF0, a method of AtDataRegistry (thiscall, three arguments): the value of the link the registry's search finds, or null. A free function so as not to change include/at/AtDataRegistry.h.
AtDataComponent* AtDataRegistryFindValue(AtDataRegistry* registry, AtDataLink* under, const char* path, int parents);

// 0x0014BAC0, a method of GmAcNode (thiscall, two arguments; the second is not read): the link named `name` under the node's record, searched with parents, or null for an empty name. Like
// GmAcNode::FindValueObject (0x0014CB00) but the link, not its value; neither the registry nor the record is tested. A free function so as not to change include/gm/GmAcActor.h.
AtDataLink* GmAcNodeFindLink(GmAcNode* node, const char* name, int unused);

// ---- the manager -------------------------------------------------------------------------------------------------------------------
// 0x0C bytes. One of an action's modifiers.
struct GmAiActionModifier {
    char m_condition[8];                // +0x00 the child record's name: a condition the NPC's TestCondition answers
    float m_factor;                     // +0x08 the child's real, clamped to [0, 1]; a NaN is kept
};

// 0x24 bytes. One action.
struct GmAiAction {
    char m_name[8];                     // +0x00 empty when the list's name was empty or its record was missing or not a real
    float m_weight;                     // +0x08 the record's own real; 0 when negative or NaN
    GmAiActionModifier* m_modifiers;    // +0x0C one per real child of the record
    unsigned int m_modifierCount;       // +0x10
    int m_enabled;                      // +0x14 1 when read; the script events set and clear it
    int m_repeatable;                   // +0x18 1 unless the record says Repeatable with another value
    char m_next[8];                     // +0x1C NextAction: the action to choose after this one, or empty
};

// 0x24 bytes, no vftable: GmAcNPC deletes it by calling the destructor directly, then freeing.
class GmAiActionManager {
public:
    GmAiActionManager();                // 0x0017D420 every word 0
    ~GmAiActionManager();               // 0x0017D6C0, a jump to Clear
    void Clear();                       // 0x0017D450 frees each action's modifiers, the actions and the weights; then every word 0

    // 0x0017D4E0. Keeps the owner; DefaultAIAction and DefaultAIPattern by GmAcNode::FindValueOrDefault (the defaults record too); the pattern through SetPattern; and the actions, when there are
    // none yet. Called once, by GmAcNPC's slot 15.
    void Setup(GmAcNPC* owner);

    // 0x0017CEA0. Reads the record's AIActions: nothing without the owner's registry or a short name array there. The actions and the weights from the heap below (-2 raised with no heap, -4 with
    // no memory). Quirks kept: an empty name is skipped but its slot is kept, blank and enabled; the first name that repeats an action already read ends the list there (the same slot and list entry are tried again with the count one lower, until the loop stops); a name
    // whose record is missing, has no value or is not a real stays a blank, enabled slot;
    // a modifier is any child whose value is a real, so a child with no value is a crash, and the weight of a record is its own value, which must be a real.
    void ReadActions();

    GmAiAction* Find(const char* name, int unused);   // 0x0017CDE0 by name; the second argument is not read
    int IsEnabled(const char* name);    // 0x0017CE30 1 for an enabled action of that name
    void Enable(const char* name);      // 0x0017CE60 nothing for an unknown name
    void Disable(const char* name);     // 0x0017CE80

    // 0x0017D290. An action's weight now: 0 when disabled, or when it was the last action, is not Repeatable and repeats are not allowed; else its weight times the factor of each modifier
    // whose condition the owner answers true. Floats throughout, stored after each step.
    float Weight(unsigned int index, int allowRepeat);

    // 0x0017D530. The next action's name into `out` (eight bytes), as described at the top. The random pick: each weight is kept in m_weights, the random from the game's generator (GmHistoryLogs'
    // m_random, include/at/AtRandomGenerator.h) as Unit() inlined, times the total, against the running sum in the x87 register; the first action whose running sum passes it. Quirk kept: with a
    // pattern, an empty pattern leaves DefaultAIAction and nothing else is tried.
    void Choose(char* out);

    // 0x0017D340. The action the chart says ran becomes the last one; with a pattern, the pattern moves on when it is the current entry (wrapping), and an index past the end goes back to 0.
    void Store(const char* name);

    // 0x0017D3C0. The pattern: the short name array of that name under the owner's record (GmAcNode::FindValueObject), or none; the index back to 0 either way. The name is copied to nine bytes first.
    void SetPattern(const char* name);

    static void SetHeap(void* heap);    // 0x0017CDD0

    GmAcNPC* m_owner;                   // +0x00
    const char* m_defaultAction;        // +0x04 DefaultAIAction's name, where the record holds it; may be null
    GmAiAction* m_actions;              // +0x08
    unsigned int m_actionCount;         // +0x0C
    float* m_weights;                   // +0x10 one per action, the weights of the last random pick
    char m_current[8];                  // +0x14 the last action the chart stored
    AtDataShortNameArray* m_pattern;    // +0x1C
    unsigned int m_patternIndex;        // +0x20

    static void* s_heap;                // 0x003B3C88 where the actions and the weights come from
};

// ---- the behaviour chart's commands ----------------------------------------------------------------------------------------------
// What a command is handed. Only what these commands read: one frame per character the chart runs (+0x18), which is current (+0x9C), and each frame's character (+0x64). Every command of the
// chart (about 300, registered by 0x0016FD60) reads its character so; their other words are not established.
struct GmChartFrame {
    char m_unknown00[0x64];
    GmAcNPC* m_actor;                   // +0x64 read as an NPC here, with no test of its class
};
struct GmChartContext {
    char m_unknown00[0x18];
    GmChartFrame* m_frames[0x21];       // +0x18
    int m_current;                      // +0x9C
};

// 0x001659F0, the chart's reader of a command's text argument `index` into `out` (at most size - 1 characters), through the chart's variables and the character's record. Used by about forty
// commands; it reaches the chart's interpreter (0x0007AFD0, 0x00075410). Not recovered: stops with a diagnostic, leaving `out` empty.
void GmChartTextArgument(char* out, unsigned int size, GmChartContext* context, void* a2, void** args, int index, int flag);

// The commands, cdecl, with the chart's three arguments. Each reads the current frame's character and its manager (+0x304); a null frame is a crash in each, as in the original.
int GmChartChooseAIAction(GmChartContext* context, void* a2, void** args);  // 0x0016C0C0 "bNPCChooseAIAction"
int GmChartStoreAIAction(GmChartContext* context, void* a2, void** args);   // 0x0016C180 "vNPCStoreAIAction"
int GmChartHasAIAction(GmChartContext* context, void* a2, void** args);     // 0x0016C1F0 "bNPCHasAIAction"

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(GmAiActionModifier) == 0x0C, "GmAiActionModifier");
static_assert(offsetof(GmAiAction, m_modifierCount) == 0x10, "GmAiAction::m_modifierCount");
static_assert(offsetof(GmAiAction, m_next) == 0x1C, "GmAiAction::m_next");
static_assert(sizeof(GmAiAction) == 0x24, "GmAiAction");
static_assert(offsetof(GmAiActionManager, m_current) == 0x14, "GmAiActionManager::m_current");
static_assert(offsetof(GmAiActionManager, m_pattern) == 0x1C, "GmAiActionManager::m_pattern");
static_assert(sizeof(GmAiActionManager) == 0x24, "GmAiActionManager");
static_assert(offsetof(GmChartFrame, m_actor) == 0x64, "GmChartFrame::m_actor");
static_assert(offsetof(GmChartContext, m_current) == 0x9C, "GmChartContext::m_current");
#pragma pop_macro("offsetof")
#endif
