// Playing a sound note, and the voice slots, emitters and dynamic nodes it plays through. See include/gm/GmAudioData.h and include/gm/GmSoundNodes.h.
// Verification status per function: config/functions.d/gmsound.toml. What actually makes a sound is the emitter's slots, which reach the bank and XACT: those are not recovered, and stop with
// a diagnostic in a real build (src/at/AtAudioDatabase.cpp).
#include "at/AtAudioDatabase.h"
#include "at/AtCoordSys.h"
#include "at/AtNameTableOwner.h"
#include "at/AtScene.h"
#include "gm/GmAppSettings.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"
#include "gm/GmRandom.h"

int AtFormat(char* out, const char* format, ...);  // Stands in for the library's sprintf, 0x00125559: src/at/AtIoFormatHost.cpp

AtCoordSys* GmAudioData::s_listener;

GmAppAudioOptions* GmAppAudioOptions::Get() {
    return s_instance;
}

void AtSceneNode::GetWorldSphere(AtSceneSphere* out) {
    if ((m_flags & kMoved) != 0) {
        FinishBody();
    }
    out->center = m_worldSphere.center;
    out->radius = m_worldSphere.radius;
}

namespace {

const float kWaitSeconds = 0.1f;

// An emitter is placed by an axis and an angle of zero about it, and a position.
void PlaceEmitter(AtAudioEmitter* emitter, const AtVector3* position) {
    AtVector3 axis;
    axis.x = 1.0f;
    axis.y = 0;
    axis.z = 0;
    emitter->SetWorldTransform(&axis, 0, position, 0);
}

}  // namespace

// ---- the forty voice slots ------------------------------------------------------------------------------------------------

void GmAudioData::FindEmitters() {
    AtNameTableOwner* owner = s_soundOwner;
    m_ui = (AtAudioEmitter*)owner->m_nodes.Find("ui");
    m_sfx = (AtAudioEmitter*)s_soundOwner->m_nodes.Find("sfx");
    m_voice = (AtAudioEmitter*)s_soundOwner->m_nodes.Find("voice");
    m_music = (AtAudioEmitter*)s_soundOwner->m_nodes.Find("music");
    m_ambience = (AtAudioEmitter*)s_soundOwner->m_nodes.Find("ambience");
    char name[12];
    for (unsigned int i = 0; i < 5; i++) {
        AtFormat(name, "collsn%d", i + 1);
        m_collisionEmitters[i] = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(name);
    }
    for (unsigned int i = 0; i < kVoiceSlots; i++) {
        AtFormat(name, "actor%d", i + 1);
        m_voiceSlots[i].m_emitter = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(name);
    }
}

AtAudioEmitter* GmAudioData::TakeVoiceSlot() {
    for (unsigned int i = (m_lastVoiceSlot + 1) % kVoiceSlots; i != m_lastVoiceSlot; i = (i + 1) % kVoiceSlots) {
        GmSoundVoiceSlot* slot = &m_voiceSlots[i];
        if (slot->m_state == GmSoundVoiceSlot::kFree && slot->m_emitter != 0 && slot->m_emitter->m_firstVoice == 0) {
            slot->m_state = GmSoundVoiceSlot::kNode;
            slot->m_emitter->ResumeAll();
            m_lastVoiceSlot = i;
            return slot->m_emitter;
        }
    }
    return 0;
}

void GmAudioData::ReleaseVoiceSlot(AtAudioEmitter* emitter) {
    for (unsigned int i = 0; i < kVoiceSlots; i++) {
        if (emitter != 0 && emitter == m_voiceSlots[i].m_emitter) {
            emitter->StopAll();
            m_voiceSlots[i].m_state = GmSoundVoiceSlot::kFree;
            return;
        }
    }
}

// ---- playing --------------------------------------------------------------------------------------------------------------

void GmAudioData::PlayOnEmitter(AtAudioEmitter* emitter, AtAudioSegment* segment) {
    if (emitter == 0 || segment == 0) {
        return;
    }
    if (GmAppAudioOptions::Get() != 0 && GmAppAudioOptions::Get()->m_noAudio != 0) {
        return;
    }
    emitter->Play(segment, 0);
}

void GmAudioData::PlayByName(const char* emitterName, const char* segmentName) {
    AtAudioEmitter* emitter = (AtAudioEmitter*)s_soundOwner->m_nodes.Find(emitterName);
    AtAudioSegment* segment = FindSegment(segmentName);
    if (emitter == 0 || segment == 0) {
        return;
    }
    if (GmAppAudioOptions::Get() != 0 && GmAppAudioOptions::Get()->m_noAudio != 0) {
        return;
    }
    emitter->Play(segment, 0);
}

void GmAudioData::PlayAtActor(GmActor* actor, AtAudioSegment* segment) {
    // Unlike PlayOnEmitter, no test for there being an App object.
    if (GmAppAudioOptions::Get()->m_noAudio != 0) {
        return;
    }
    unsigned int i = 0;
    while (m_voiceSlots[i].m_state != GmSoundVoiceSlot::kFree || m_voiceSlots[i].m_emitter == 0) {
        if (++i >= kVoiceSlots) {
            return;
        }
    }
    GmSoundVoiceSlot* slot = &m_voiceSlots[i];
    slot->m_state = GmSoundVoiceSlot::kActor;
    if (actor != 0) {
        slot->m_actor = actor;
        AtVector3 position;
        actor->GetMotion(&position, 0, 0, 0, 0);
        PlaceEmitter(slot->m_emitter, &position);
    }
    slot->m_emitter->ResumeAll();
    slot->m_emitter->Play(segment, 0);
}

void GmAudioData::PlayNote(GmActor* actor, GmSoundNote* note) {
    if (!AtNamesEqual(note->m_stop, "")) {
        char stop[8];
        AtCopyName8(stop, note->m_stop);
        GmSoundNote* other = FindNote(stop, "");
        if (other != 0 && other->m_choiceCount != 0 && other->m_choices[0].m_segment != 0) {
            AtAudioSegment* segment = other->m_choices[0].m_segment;
            if (note->m_emitter != 0) {
                note->m_emitter->StopSegment(segment);
            } else if (actor != 0 && actor->m_audio != 0 && actor->m_audio->m_soundNodes != 0) {
                actor->m_audio->m_soundNodes->StopSegment(segment, 0);
            }
        }
    }
    if (note->m_conditional != 0 && note->m_conditional->m_firstVoice != 0) {
        return;
    }
    if (note->m_choiceCount == 0) {
        return;
    }
    // A pick in ten-thousandths, and the running sum of the weights in a register. The sum takes the next weight before the count is tested, so with weights that do not reach the pick it
    // reads one past the last, and chooses it: that element is past the end of the array.
    const int step = (int)(GmHistoryLogs::s_instance->m_random->Next() % 10000);
    const float pick = (float)(step * (double)0.0001f);
    GmSoundChoice* choices = note->m_choices;
    unsigned int chosen = 0;
    double sum = choices[0].m_weight;
    while (!(sum > pick)) {  // as the original's test, which goes on for an unordered sum
        sum += choices[chosen + 1].m_weight;
        if (++chosen >= note->m_choiceCount) {
            break;
        }
    }
    if (note->m_emitter != 0) {
        if (note->m_singleton != 0 && note->m_emitter->IsPlaying(choices[chosen].m_segment) != 0) {
            return;
        }
        PlayOnEmitter(note->m_emitter, note->m_choices[chosen].m_segment);
        return;
    }
    if (actor == 0 || choices[chosen].m_segment == 0) {
        return;
    }
    AtVector3 position;
    position.x = 0;
    position.y = 0;
    position.z = 0;
    if (actor->m_node != 0) {
        actor->m_node->GetWorldPosition(&position);
        if (actor->m_audio != 0 && actor->m_audio->m_soundNodes != 0) {
            actor->m_audio->m_soundNodes->Play(note->m_choices[chosen].m_segment, 0, &position, note->m_singleton);
            return;
        }
    }
    PlayAtActor(actor, note->m_choices[chosen].m_segment);
}

// ---- dynamic nodes --------------------------------------------------------------------------------------------------------

GmSoundNode* GmAudioData::TakeDynamicNode(GmActor* actor, float) {
    GmSoundNodeBank* pool = &m_nodeBanks[0];
    for (unsigned int i = 0; i < pool->m_count; i++) {
        if (pool->m_nodes[i].m_owner == 0) {
            pool->m_nodes[i].m_owner = actor;
            pool->m_nodes[i].m_source = actor->m_soundSource;
            pool->m_nodes[i].m_range = 0;
            return &pool->m_nodes[i];
        }
    }
    return 0;
}

void GmAudioData::PlayNode(GmSoundNode* node, AtAudioSegment* segment) {
    AtSceneSphere sphere;
    if (node == 0 || segment == 0) {
        return;
    }
    if (segment->Unknown31BF0() > node->m_range) {
        node->m_range = segment->Unknown31BF0();
    }
    bool near = false;
    if (node->m_source != 0) {
        node->m_source->GetWorldSphere(&sphere);
        AtVector3 listener;
        s_listener->GetWorldPosition(&listener);
        const double distance = listener.Distance(sphere.center);
        near = distance <= (double)s_soundOwner->m_unknown14 * node->m_range;
    }
    if (!near) {
        // Out of range, or nowhere: a looping segment is kept for when the node comes in range. Anything else is dropped.
        if (segment->Length() == 3.4028234663852886e+38f) {
            node->m_segment = segment;
            node->m_active = 1;
        }
        return;
    }
    if (node->m_emitter == 0) {
        node->m_emitter = TakeVoiceSlot();
    }
    if (node->m_emitter == 0) {
        return;
    }
    PlaceEmitter(node->m_emitter, &sphere.center);
    node->m_emitter->Play(segment, 0);
    node->m_segment = 0;
    node->m_active = 1;
}

void GmActorSoundNodes::StopSegment(AtAudioSegment* segment, int index) {
    if (index < 0 || index >= m_count || m_nodes[index] == 0 || m_nodes[index]->m_emitter == 0) {
        return;
    }
    m_nodes[index]->m_emitter->StopSegment(segment);
}

void GmActorSoundNodes::Play(AtAudioSegment* segment, int index, const AtVector3*, int singleton) {
    if (segment == 0 || index < 0 || index >= m_count) {
        return;
    }
    if (m_nodes[index] == 0) {
        m_nodes[index] = m_manager->TakeDynamicNode(m_actor, m_actor->Unknown52());
    }
    if (m_nodes[index] == 0) {
        return;
    }
    if (singleton != 0 && m_nodes[index]->m_emitter != 0 && m_nodes[index]->m_emitter->IsPlaying(segment) != 0) {
        return;
    }
    if (GmAppAudioOptions::Get()->m_noAudio != 0) {
        return;
    }
    m_manager->PlayNode(m_nodes[index], segment);
}

// ---- stopping -------------------------------------------------------------------------------------------------------------

void GmAudioData::StopBankVoices(AtAudioEmitter* emitter, AtAudioBank* bank) {
    if (emitter == 0 || bank == 0) {
        return;
    }
    for (;;) {
        int stopped = 0;
        for (AtAudioVoice* voice = emitter->FirstVoice(); voice != 0; voice = emitter->NextVoice(voice)) {
            if (voice->m_segment->m_bank == bank) {
                emitter->StopVoice(voice);
                stopped = 1;
            }
        }
        if (stopped == 0) {
            return;
        }
        s_soundOwner->m_loader->Wait(kWaitSeconds);
    }
}

void GmAudioData::RemoveDatabase(AtAudioDatabase* database) {
    for (unsigned int i = 0; i < database->m_bankCount; i++) {
        AtAudioBankDatabase* list = database->BankDatabaseAt(i);
        if (list == 0) {
            continue;
        }
        for (unsigned int k = 0; k < list->m_count; k++) {
            AtAudioBank* bank = list->m_banks[k];
            if (bank == 0) {
                continue;
            }
            AtAudioEmitter** named = &m_ambience;
            for (int e = 0; e < 5; e++) {
                StopBankVoices(named[e], bank);
            }
            for (unsigned int e = 0; e < kVoiceSlots; e++) {
                StopBankVoices(m_voiceSlots[e].m_emitter, bank);
            }
        }
    }
    for (unsigned int i = 0; i < kDatabases; i++) {
        if (m_databases[i] == database) {
            m_databases[i] = 0;
            return;
        }
    }
}
