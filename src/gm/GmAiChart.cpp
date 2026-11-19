// The behaviour chart's three commands that drive the AI action manager: see include/gm/GmAiActionManager.h. Names other than the commands' own are inferred.
#include "gm/GmAiActionManager.h"

#include "gm/GmAcNPC.h"

void AtCopyName8(char* to, const char* from);   // 0x00013F70
void AtCopyName(const char* from, char* to);    // 0x00013ED0
int AtNamesEqual(const char* a, const char* b);  // 0x00013FC0

namespace {
// The character's slot 49 answers an int in the original (0x00148A00 answers 0, or what the actor's state machine answers), and the chart tests it here. include/gm/GmAcActor.h declares the slot
// void, and changing that would change every class that overrides it, so the answer is read through a member pointer of the right type. A class whose override is void answers what the register
// holds; every override recovered so far is a jump to the actor's, which is not recovered.
int SendEvent(GmAcActor* actor, const char* event, int b, int c) {
    typedef int (GmAcActor::*IntSlot)(const char*, int, int);
    void (GmAcActor::*slot)(const char*, int, int) = &GmAcActor::Slot49;
    const IntSlot answering = reinterpret_cast<IntSlot>(slot);
    return (actor->*answering)(event, b, c);
}

GmAcNPC* CharacterOf(GmChartContext* context) {
    GmChartFrame* frame = context->m_frames[context->m_current];
    return frame != 0 ? frame->m_actor : 0;
}
}  // namespace

// The chart's state stays as it is when the choice is the action already running and the character answers RESUME; else the character is told the choice. 1 when there is a choice.
int GmChartChooseAIAction(GmChartContext* context, void*, void**) {
    char last[8] = {0};
    char next[8] = {0};
    GmAcNPC* character = CharacterOf(context);
    GmAiActionManager* manager = character->m_aiActions;
    if (manager != 0) {
        AtCopyName8(last, manager->m_current);
        manager->Choose(next);
        if (!AtNamesEqual(next, last) || SendEvent(character, "RESUME", 0, 1) == 0) {
            SendEvent(character, next, 0, 1);
        }
    }
    return AtNameLength8(next) > 0 ? 1 : 0;
}

// Always 1.
int GmChartStoreAIAction(GmChartContext* context, void* a2, void** args) {
    char argument[0x100];
    char name[8];
    GmChartTextArgument(argument, 9, context, a2, args, 1, 0);
    AtCopyName(argument, name);
    GmAcNPC* character = CharacterOf(context);
    GmAiActionManager* manager = character->m_aiActions;
    if (manager != 0) {
        manager->Store(name);
    }
    return 1;
}

int GmChartHasAIAction(GmChartContext* context, void* a2, void** args) {
    char argument[0x100];
    char name[8];
    GmChartTextArgument(argument, 9, context, a2, args, 1, 0);
    AtCopyName(argument, name);
    GmAcNPC* character = CharacterOf(context);
    GmAiActionManager* manager = character->m_aiActions;
    int enabled = 0;
    if (manager != 0) {
        enabled = manager->IsEnabled(name);
    }
    return enabled != 0 ? 1 : 0;
}
