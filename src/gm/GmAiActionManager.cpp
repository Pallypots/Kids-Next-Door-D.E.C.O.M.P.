// The AI action manager, its actions and pattern, and the small readers it alone needs: see include/gm/GmAiActionManager.h. Names are inferred.
#include "gm/GmAiActionManager.h"

#include "gm/GmRandom.h"
#include "gm/GmAcNPC.h"
#include "gm/GmAppSettings.h"

void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);   // 0x00013F70
void AtCopyName9(char* to, const char* from);   // 0x00013F90
void AtCopyName(const char* from, char* to);    // 0x00013ED0
int AtNamesEqual(const char* a, const char* b);  // 0x00013FC0

void* GmAiActionManager::s_heap;

// ---- the small readers ---------------------------------------------------------------------------------------------------------
// AtNameLength8 (0x00014030), AtDataRegistryFindValue (0x00067CF0) and GmAcNodeFindLink (0x0014BAC0) are gm/gmplayer's AtNameLength, at/datalookup's
// AtDataRegistry::ValueAt and gm/gmplayer's GmAcNode::FindRecord: folded on integrate-batch8.

// ---- the manager ---------------------------------------------------------------------------------------------------------------
namespace {
void ClearName(char* name) {
    for (int i = 0; i < 8; i++) {
        name[i] = 0;
    }
}

void* NewFromHeap(unsigned int size) {
    if (GmAiActionManager::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmAiActionManager::s_heap);
    void* block = operator new(size);
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return block;
}
}  // namespace

void GmAiActionManager::SetHeap(void* heap) {
    s_heap = heap;
}

GmAiActionManager::GmAiActionManager() {
    m_owner = 0;
    m_defaultAction = 0;
    m_actions = 0;
    m_actionCount = 0;
    m_weights = 0;
    ClearName(m_current);
    m_pattern = 0;
    m_patternIndex = 0;
}

GmAiActionManager::~GmAiActionManager() {
    Clear();
}

void GmAiActionManager::Clear() {
    if (m_actions != 0) {
        for (unsigned int i = 0; i < m_actionCount; i++) {
            if (m_actions[i].m_modifiers != 0) {
                operator delete(m_actions[i].m_modifiers);
                m_actions[i].m_modifiers = 0;
            }
        }
        operator delete(m_actions);
    }
    if (m_weights != 0) {
        operator delete(m_weights);
    }
    m_owner = 0;
    m_defaultAction = 0;
    m_actions = 0;
    m_actionCount = 0;
    m_weights = 0;
    ClearName(m_current);
    m_pattern = 0;
    m_patternIndex = 0;
}

void GmAiActionManager::Setup(GmAcNPC* owner) {
    m_owner = owner;
    m_defaultAction = (const char*)owner->FindValueOrDefault("DefaultAIAction", 0);
    const char* pattern = (const char*)m_owner->FindValueOrDefault("DefaultAIPattern", 0);
    if (pattern != 0) {
        SetPattern(pattern);
    }
    if (m_actions == 0) {
        ReadActions();
    }
}

void GmAiActionManager::SetPattern(const char* name) {
    char path[12];
    AtCopyName9(path, name);
    AtDataComponent* value = m_owner->FindValueObject(path, 0);
    if (value != 0 && value->m_descriptor->m_type == AtDataRegistry::kShortNameArray) {
        m_pattern = (AtDataShortNameArray*)value;
        m_patternIndex = 0;
        return;
    }
    m_pattern = 0;
    m_patternIndex = 0;
}

void GmAiActionManager::ReadActions() {
    AtDataRegistry* registry = (AtDataRegistry*)m_owner->m_unknown10;
    AtDataLink* list = m_owner->FindRecord("AIActions", 0);
    AtDataShortNameArray* names = list != 0 ? (AtDataShortNameArray*)list->m_value : 0;
    if (registry == 0 || names == 0 || names->m_descriptor->m_type != AtDataRegistry::kShortNameArray) {
        return;
    }
    m_actionCount = names->m_descriptor->m_count;
    m_actions = (GmAiAction*)NewFromHeap(m_actionCount * sizeof(GmAiAction));
    m_weights = (float*)NewFromHeap(m_actionCount * sizeof(float));
    for (unsigned int i = 0; i < m_actionCount; i++) {
        GmAiAction* action = &m_actions[i];
        ClearName(action->m_name);
        action->m_enabled = 1;
        action->m_repeatable = 1;
        action->m_weight = 0;
        action->m_modifierCount = 0;
        action->m_modifiers = 0;
        ClearName(action->m_next);
    }

    for (unsigned int used = 0; used < m_actionCount; used++) {
        GmAiAction* action = &m_actions[used];
        char name[8];
        AtCopyName8(name, names->m_data[used]);
        if (AtNamesEqual(name, "")) {
            continue;
        }
        unsigned int earlier = 0;
        while (earlier < used && !AtNamesEqual(m_actions[earlier].m_name, name)) {
            earlier++;
        }
        if (earlier < used) {
            // The same slot and the same list entry again, with one action fewer: so the list ends here.
            used--;
            m_actionCount--;
            continue;
        }

        char path[9];
        AtCopyName9(path, name);
        AtDataLink* record = registry->FindPath(list, path, 1);
        if (record == 0) {
            continue;
        }
        AtDataComponent* weight = record->m_value;
        if (weight == 0 || weight->m_descriptor->m_type != AtDataRegistry::kReal) {
            continue;
        }
        AtCopyName8(action->m_name, name);
        // Kept as written, not as a maximum: -0 stays -0 and a NaN becomes 0, as in the original's x87 test.
        const float value = *(float*)weight->ElementAt(0);
        action->m_weight = value;
        if (!(value >= 0.0f)) {
            action->m_weight = 0.0f;
        }

        AtDataComponent* repeatable = registry->ValueAt(record, "Repeatable", 0);
        if (repeatable != 0 && repeatable->m_descriptor->m_type == AtDataRegistry::kBoolean) {
            action->m_repeatable = *(int*)repeatable->ElementAt(0);
        }
        AtDataComponent* next = registry->ValueAt(record, "NextAction", 0);
        if (next != 0 && next->m_descriptor->m_type == AtDataRegistry::kShortName) {
            AtCopyName8(action->m_next, (const char*)next->ElementAt(0));
        }

        for (AtDataLink* child = record->m_firstChild; child != 0; child = child->m_sibling) {
            if (child->m_value->m_descriptor->m_type == AtDataRegistry::kReal) {
                action->m_modifierCount++;
            }
        }
        action->m_modifiers = (GmAiActionModifier*)NewFromHeap(action->m_modifierCount * sizeof(GmAiActionModifier));
        GmAiActionModifier* modifier = action->m_modifiers;
        for (AtDataLink* child = record->m_firstChild; child != 0; child = child->m_sibling) {
            AtDataComponent* value = child->m_value;
            if (value->m_descriptor->m_type != AtDataRegistry::kReal) {
                continue;
            }
            AtCopyName(child->m_name, modifier->m_condition);
            // Stored, then clamped in place, as the original does: -0 stays -0 and a NaN stays.
            modifier->m_factor = *(float*)value->ElementAt(0);
            if (modifier->m_factor > 1.0f) {
                modifier->m_factor = 1.0f;
            }
            if (modifier->m_factor < 0.0f) {
                modifier->m_factor = 0.0f;
            }
            modifier++;
        }
    }
}

GmAiAction* GmAiActionManager::Find(const char* name, int) {
    for (unsigned int i = 0; i < m_actionCount; i++) {
        if (AtNamesEqual(m_actions[i].m_name, name)) {
            return &m_actions[i];
        }
    }
    return 0;
}

int GmAiActionManager::IsEnabled(const char* name) {
    GmAiAction* action = Find(name, 0);
    if (action != 0 && action->m_enabled != 0) {
        return 1;
    }
    return 0;
}

void GmAiActionManager::Enable(const char* name) {
    GmAiAction* action = Find(name, 1);
    if (action != 0) {
        action->m_enabled = 1;
    }
}

void GmAiActionManager::Disable(const char* name) {
    GmAiAction* action = Find(name, 1);
    if (action != 0) {
        action->m_enabled = 0;
    }
}

float GmAiActionManager::Weight(unsigned int index, int allowRepeat) {
    GmAiAction* action = &m_actions[index];
    if (action->m_enabled == 0) {
        return 0.0f;
    }
    if (allowRepeat == 0 && action->m_repeatable == 0 && AtNamesEqual(m_current, action->m_name)) {
        return 0.0f;
    }
    float weight = action->m_weight;
    for (unsigned int i = 0; i < action->m_modifierCount; i++) {
        GmAiActionModifier* modifier = &action->m_modifiers[i];
        if (m_owner->TestCondition(modifier->m_condition)) {
            weight = weight * modifier->m_factor;
        }
    }
    return weight;
}

void GmAiActionManager::Choose(char* out) {
    AtCopyName8(out, m_defaultAction);
    if (m_pattern != 0) {
        if (m_patternIndex >= m_pattern->m_descriptor->m_count) {
            m_patternIndex = 0;
        }
        if (m_pattern->m_descriptor->m_count != 0) {
            AtCopyName8(out, m_pattern->m_data[m_patternIndex]);
        }
        return;
    }

    ClearName(out);
    GmAiAction* last = Find(m_current, 0);
    if (last != 0) {
        AtCopyName8(out, last->m_next);
    }
    if (AtNameLength(out) > 0) {
        return;
    }

    AtCopyName8(out, m_defaultAction);
    float total = 0.0f;
    for (unsigned int i = 0; i < m_actionCount; i++) {
        const float weight = Weight(i, 0);
        m_weights[i] = weight;
        total = total + weight;
    }
    if (total <= 0.0f) {
        total = 0.0f;
        for (unsigned int i = 0; i < m_actionCount; i++) {
            const float weight = Weight(i, 1);
            m_weights[i] = weight;
            total = total + weight;
        }
    }
    const float target = (float)(GmHistoryLogs::s_instance->m_random->NextUnit() * total);
    double sum = 0.0;
    for (unsigned int i = 0; i < m_actionCount; i++) {
        sum += m_weights[i];
        if (target < sum) {
            AtCopyName8(out, m_actions[i].m_name);
            return;
        }
    }
}

void GmAiActionManager::Store(const char* name) {
    AtCopyName8(m_current, name);
    if (m_pattern == 0) {
        return;
    }
    if (m_patternIndex >= m_pattern->m_descriptor->m_count) {
        m_patternIndex = 0;
        return;
    }
    char entry[8];
    AtCopyName8(entry, m_pattern->m_data[m_patternIndex]);
    if (AtNamesEqual(name, entry)) {
        m_patternIndex = (m_patternIndex + 1) % m_pattern->m_descriptor->m_count;
    }
}
