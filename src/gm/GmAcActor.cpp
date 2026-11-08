// GmAcActor, its members' classes, and the animation callback: see include/gm/GmAcActor.h. Names other than the RTTI names are inferred.
#include "gm/GmAcActor.h"

#include "at/AtSpaceGraph.h"
#include "at/AtVector3.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManifest.h"

void AtFatal(const char* message, int file, int line, int unused);
void AtRaise(int code, int file, int line, int unused);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);   // 0x00013F70 two words, or an empty name if `from` is null
void AtCopyName9(char* to, const char* from);   // 0x00013F90 always eight bytes then a terminator; null means empty
int AtNamesEqual(const char* a, const char* b); // 0x00013FC0

namespace {
typedef decltype(sizeof(0)) Size;
struct Placement {};
}  // namespace
inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

void* GmAcActor::s_heap;
GmNameMap GmAcActor::s_namedMaps;
GmEventManager* GmEventManager::s_instance;

// ---- the vector sets -----------------------------------------------------------------------------------------------------
void GmAcVectorSet::Clear() {
    m_active = 0;
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 3; k++) {
            m_vectors[i][k] = 0;
        }
    }
}

GmAcVectorSet::~GmAcVectorSet() {
    Clear();
}

// ---- the list at +0x21C --------------------------------------------------------------------------------------------------
GmLinkedListOf<GmAcLinkedItem*>::GmLinkedListOf() {
}

GmLinkedListOf<GmAcLinkedItem*>::~GmLinkedListOf() {
}

// ---- the animation callback ----------------------------------------------------------------------------------------------
AtKiAnimationCallBack::AtKiAnimationCallBack() {
    m_unknown04 = 1;
}

AtKiAnimationCallBack::~AtKiAnimationCallBack() {
}

void AtKiAnimationCallBack::Slot1(int) {}
void AtKiAnimationCallBack::Slot2(int) {}
void AtKiAnimationCallBack::OnLoop(int) {}
void AtKiAnimationCallBack::OnTrigger(const char*, int, int) {}
int AtKiAnimationCallBack::Slot5(int) { return 0; }
void AtKiAnimationCallBack::Slot6(int) {}

GmAcAnimationCallback::~GmAcAnimationCallback() {
    Reset();
}

void GmAcAnimationCallback::Reset() {
    m_actor = 0;
    m_node = 0;
    m_place = 0;
}

void GmAcAnimationCallback::OnLoop(int) {
    if (m_actor != 0) {
        m_actor->Slot49("ANIMLOOP", 0, 1);
    }
}

// Quirks kept: both nine-byte copies of the names are made and never read. A note the trigger table finds goes first to a method of the sound manager (0x000B15B0) that returns 0, and the answer is
// ignored; with no actor, after the placing, the note goes to another (0x000447F0) that does nothing. Neither call is written here. With no actor, FindNote is given a null bank name, which crashes once
// any bank is in use. The GameEvent is posted with the actor, null when there is none.
void GmAcAnimationCallback::OnTrigger(const char* trigger, int, int) {
    char name[8];
    AtCopyName8(name, trigger);
    char nameText[9];
    AtCopyName9(nameText, trigger);
    GmSoundNote* note = 0;
    if (m_actor != 0) {
        char bank[8];
        AtCopyName8(bank, m_actor->m_noteBank);
        char bankText[9];
        AtCopyName9(bankText, bank);
        if (m_actor->Slot27() == 0) {
            return;
        }
        GmAcTriggerTable* triggers = m_actor->m_triggers;
        char effect[8] = {0};
        char effectTarget[8] = {0};
        if (triggers != 0) {
            triggers->FindTrigger(name, effect, effectTarget);
        }
        if (!AtNamesEqual(effect, "")) {
            if (!AtNamesEqual(effectTarget, "")) {
                m_actor->SpawnEffect(effect, 0, effectTarget, 0, 0, 0);
            } else {
                m_actor->SpawnEffect(effect, 0, 0, 0, 0, 0);
            }
        } else if (GmVfxManagerHas(GmVfxManager::s_instance, name)) {
            m_actor->SpawnEffect(name, 0, 0, 0, 0, 0);
        }
        GmAcComponent* component = m_actor->m_component;
        bool play = false;
        if (component != 0) {
            if (component->m_audio != 0) {
                if (triggers != 0) {
                    note = triggers->FindNote(name);
                }
                if (note != 0) {
                    play = true;
                } else {
                    for (int i = 0; i < 8; i++) {
                        bank[i] = 0;
                    }
                    m_actor->GetNoteBank(bank);
                }
            }
        } else {
            for (int i = 0; i < 8; i++) {
                bank[i] = 0;
            }
            AtCopyName8(bank, m_actor->m_noteBank);
        }
        if (!play && (component == 0 || component->m_audio != 0)) {
            note = GmAudioData::s_instance->FindNoteByTrigger(name, bank);
            if (note == 0) {
                note = GmAudioData::s_instance->FindNoteByTrigger(name, "");
            }
            play = note != 0;
        }
        if (play) {
            GmAudioDataPlayNote(GmAudioData::s_instance, m_actor, note);
        }
        m_actor->Slot49(name, 0, 1);
    } else {
        if (m_node == 0 || m_place == 0) {
            return;
        }
        note = GmAudioData::s_instance->FindNote(name, 0);
        if (note == 0) {
            return;
        }
        AtVector3 position;
        if (m_node->m_coordSys != 0) {
            AtCoordSysWorldPosition(m_node->m_coordSys, &position);
        } else {
            position.x = 0;
            position.y = 0;
            position.z = 0;
        }
        AtVector3 forward;
        forward.x = 0;
        forward.y = 0;
        forward.z = 1.0f;
        AtCoordSysPlace(m_place, &forward, 0, &position, 0);
    }
    if (note != 0 && note->m_gameEvent[0] != 0) {
        GmEventManager::s_instance->Post(note->m_gameEvent, note->m_gameEventArgument, m_actor);
    }
}

// ---- GmAcActor -----------------------------------------------------------------------------------------------------------
GmAcActor::GmAcActor() {
    ResetActor();
}

GmAcActor::~GmAcActor() {
    GmAcActor::Shutdown();
}

void GmAcActor::ResetActor() {
    m_flags98 |= kActorReset;
    m_triggers = 0;
    for (int i = 0; i < 4; i++) {
        m_unknown14C[i] = 0;
    }
    m_unknown24C = -3.402823466e+38f;
    m_unknown14C[4] = 0;
    m_unknown17C = 0;
    m_unknown180 = 0;
    m_sceneActor = 0;
    m_attached = 0;
    m_unknown1FC = 0;
    m_unknown200 = 0;
    m_unknown204 = 0;
    m_unknown208 = 0;
    m_component = 0;
    m_unknown240[0] = 0;
    m_unknown240[1] = 0;
    m_unknown240[2] = 0;
    m_unknown160 = 0;
    m_unknown250 = 0;
    m_unknown254 = 0;
    for (int i = 0; i < 3; i++) {
        m_unknown258[i] = 0;
    }
    m_unknown264[0] = 0;
    m_unknown264[1] = 0;
    m_unknown26C = 0;
    m_unknown270 = 0;
    m_unknown274 = 0;
    m_unknown278 = 0;
    m_unknown27C = 0;
    m_unknown280 = 1;
    m_unknown284 = 0;
    m_unknown288[0] = 0;
    m_unknown288[1] = 0;
    m_unknown29C = 0;
    for (int i = 0; i < 3; i++) {
        m_unknown290[i] = 1.0f;
    }
    m_unknown2A0[0] = 0;
    m_unknown2A0[1] = 0;
    m_records = 0;
    m_recordCount = 0;
    m_unknown23C = 0;
    for (int i = 0; i < 5; i++) {
        m_unknown2A8[i] = 0;
    }
    for (int i = 0; i < 3; i++) {
        m_unknown2C8[i] = 0;
    }
    m_unknown2D4 = 1.0f;
    for (int i = 0; i < 3; i++) {
        m_unknown2BC[i] = 0;
    }
    m_motionCount = 0;
    m_motions = 0;
    m_unknown218 = 0;
}

// Quirks kept: the component is deleted but not cleared here (ResetActor does it), and it is read again after its slot 3; the scene actor likewise. "Motion" is looked up in the name map at 0x002CD124 and what
// is found is not used. The array of motions is freed but its pointer is left for ResetActor.
void GmAcActor::Shutdown() {
    m_callback.Reset();
    if (m_component != 0) {
        m_component->Slot3();
        if (m_component != 0) {
            delete m_component;
        }
    }
    if (m_attached != 0) {
        GmDynamicNodeRelease(m_attached, 0);
    }
    if (m_sceneActor != 0) {
        GmSpaceGraphOwner::s_instance->m_graph->RemoveAsKind1(m_sceneActor);
        m_sceneActor->Clear();
        if (m_sceneActor != 0) {
            delete m_sceneActor;
        }
    }
    ReleaseItems(1);
    m_vectors184.Clear();
    m_vectors1BC.Clear();
    if (m_unknown270 != 0) {
        delete m_unknown270;
        m_unknown270 = 0;
    }
    if (m_unknown218 != 0) {
        m_unknown218->Slot15();
    }
    if (m_motions != 0) {
        s_namedMaps.Find("Motion");
        for (unsigned int i = 0; i < m_motionCount; i++) {
            if (m_motions[i] != 0) {
                m_motions[i]->Slot3();
                if (m_motions[i] != 0) {
                    delete m_motions[i];
                }
                m_motions[i] = 0;
            }
        }
        operator delete(m_motions);
    }
    if (m_records != 0) {
        operator delete(m_records);
        m_records = 0;
    }
    ResetActor();
    GmAcNode::Shutdown();
}

// Each item is released and taken out of the list as the iterator passes it.
void GmAcActor::ReleaseItems(int flag) {
    GmLinkedListIteratorOf<GmAcLinkedItem*> items(&m_items);
    GmAcLinkedItem::ReleaseArgs args;
    args.m_one = 1;
    args.m_flag = flag;
    for (GmAcLinkedItem* item = (GmAcLinkedItem*)items.NextData(); item != 0; item = (GmAcLinkedItem*)items.NextData()) {
        item->Release(&args);
        item->m_owner = 0;
        static_cast<GmSocket*>(&m_items)->Remove(item);
    }
    ClearRecords();
}

void GmAcActor::ClearRecords() {
    if (m_records == 0) {
        return;
    }
    for (unsigned int i = 0; i < (m_recordCount != 0 ? *m_recordCount : 3); i++) {
        GmAcActorRecord* record = &m_records[i];
        record->m_unknown00[0] = 0;
        record->m_unknown00[1] = 0;
        record->m_unknown24 = 0;
        record->m_unknown2C[0] = 0;
        record->m_unknown2C[1] = 0;
        record->m_unknown08[0] = 0;
        record->m_unknown08[1] = 0;
        record->m_unknown08[2] = 0;
    }
}

const char* GmAcActor::Name() {
    Descriptor* descriptor = m_descriptor;
    if (descriptor == 0) {
        return 0;
    }
    if (m_unknown1FC == 0 && descriptor->m_alias != 0) {
        descriptor = descriptor->m_alias;
    }
    return descriptor->m_name;
}

void GmAcActor::SetFlag10(int on) {
    if (on) {
        m_flagsA4 |= kFlagA4_10;
    } else {
        m_flagsA4 &= ~kFlagA4_10;
    }
    SceneSetFlag10(on);
}

int GmAcActor::HasUnknown254() {
    if (m_unknown1FC != 0 && m_unknown200 != 0) {
        return 0;
    }
    return m_unknown254 != 0 && *m_unknown254 != 0 ? 1 : 0;
}

int GmAcActor::Slot27() {
    return (m_flagsA4 & kFlagA4_2) != 0 || HasUnknown254() != 0 ? 1 : 0;
}

void GmAcActor::Slot28() {
    GmAcNode::Slot28();
    if (m_attached != 0) {
        m_attached->Slot28();
    }
}

void GmAcActor::Slot29() {
    GmAcNode::Slot29();
    if (m_attached != 0) {
        m_attached->Slot29();
    }
}

void GmAcActor::Slot16() {
    GmAcNode::Slot16();
}

void GmAcActor::Slot38(int a) {
    if (m_component != 0 && m_component->m_audio != 0) {
        m_component->m_audio->Slot38(a);
    }
}

void GmAcActor::Slot41(int a) {
    Slot40(a);
}

int GmAcActor::Slot44(int) {
    return 1;
}

void GmAcActor::StartCollision(int a, int) {
    if (m_component != 0) {
        char name[8];
        AtCopyName8(name, "STRTCOL");
        m_component->SendEvent(a, name);
    }
}

void GmAcActor::EndCollision(int a) {
    if (m_component != 0) {
        char name[8];
        AtCopyName8(name, "ENDCOL");
        m_component->SendEvent(a, name);
    }
}

void GmAcActor::ForgetComponent() {
    m_component = 0;
}

int GmAcActor::Unknown278() {
    return m_unknown278 != 0 ? *m_unknown278 : 0;
}

int GmAcActor::Unknown274() {
    return m_unknown274 != 0 ? *m_unknown274 : 0;
}

float GmAcActor::Unknown27C() {
    if (m_unknown27C != 0) {
        return (float)*m_unknown27C;
    }
    return 0.0f;
}

unsigned int GmAcActor::Unknown17C() {
    return m_unknown17C;
}

void GmAcActor::Slot54(int* out) {
    out[0] = 0;
    out[1] = 0;
}

int GmAcActor::Slot55() {
    return 0;
}

int GmAcActor::Slot56() {
    if (m_unknown1FC != 0 && Slot27() == 0 && m_unknown200 != 0) {
        return 1;
    }
    return 0;
}

int GmAcActor::Slot57() {
    return m_unknown1FC != 0 && m_unknown204 != 0 ? 1 : 0;
}

void GmAcActor::SetUnknown284(GmAcActor* actor) {
    m_unknown284 = actor == this ? 0 : actor;
}

int GmAcActor::Slot59() {
    return 0;
}

void GmAcActor::SetHeap(void* heap) {
    s_heap = heap;
}

// The two factories. An array comes from new[], with the count in the word ahead of the first element; this compiler, like the original's, checks new[]'s block for null before constructing.
static GmAcActor* CreateActors(int count) {
    if (count <= 1) {
        if (GmAcActor::s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(GmAcActor::s_heap);
        void* block = operator new(sizeof(GmAcActor));
        GmAcActor* actor = block != 0 ? new (block, Placement()) GmAcActor : 0;
        AtPopHeap();
        if (actor == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        return actor;
    }
    if (GmAcActor::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmAcActor::s_heap);
    GmAcActor* first = new GmAcActor[count];
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}

GmAcActor* GmAcActor::CreateRotator(int count) {
    return CreateActors(count);
}

GmAcActor* GmAcActor::CreateNode(int count) {
    return CreateActors(count);
}

// Slots not recovered. Each stops with a diagnostic naming its original.
void GmAcActor::Slot4() { AtFatal("Gm actor: GmAcActor slot 4, 0x0014AC20, is not recovered", 0, 0, 0); }
void GmAcActor::Slot10() { AtFatal("Gm actor: GmAcActor slot 10, 0x001494C0, is not recovered", 0, 0, 0); }
void GmAcActor::Slot15() { AtFatal("Gm actor: GmAcActor slot 15, 0x0014AE90, is not recovered", 0, 0, 0); }
void GmAcActor::Slot17(int) { AtFatal("Gm actor: GmAcActor slot 17, 0x00149080, is not recovered", 0, 0, 0); }
void GmAcActor::Slot20(int) { AtFatal("Gm actor: GmAcActor slot 20, 0x0014B340, is not recovered", 0, 0, 0); }
void GmAcActor::Slot40(int) { AtFatal("Gm actor: GmAcActor slot 40, 0x001484B0, is not recovered", 0, 0, 0); }
void GmAcActor::Slot47(int, int, int) { AtFatal("Gm actor: GmAcActor slot 47, 0x001494D0, is not recovered", 0, 0, 0); }
void GmAcActor::Slot49(const char*, int, int) { AtFatal("Gm actor: GmAcActor slot 49, 0x00148A00, is not recovered", 0, 0, 0); }
