// Sound nodes (.GOO), collision sounds (.GOC) and instance sounds (.GOI): their readers and the sound manager's banks of them. See include/gm/GmSoundNodes.h and include/gm/GmAudioData.h.
// Verification status per function: config/functions.d/gmsound.toml.
#include "at/AtAudioDatabase.h"
#include "at/AtNameTableOwner.h"
#include "at/AtText.h"
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"

namespace {

const float kWaitSeconds = 0.1f;

enum NodeToken {
    kSoundNodes = 0x39, kNode = 0x3A, kActor = 0x3B, kSegment = 0x3C, kEmitter = 0x3D, kActorEmitter = 0x3E, kVelocityParm = 0x3F, kAngularVelocityParm = 0x40, kCustomParm = 0x41,
    kAnalogInputParm = 0x42, kMaxDistanceFromListener = 0x43, kPauseWhenAirborne = 0x44, kTriggeredOnly = 0x45
};

// 0x002B8FD0.
const AtIoKeyword kNodeKeywords[] = {
    {kSoundNodes, "SoundNodes"}, {kNode, "Node"}, {kActor, "Actor"}, {kSegment, "Segment"}, {kEmitter, "Emitter"}, {kActorEmitter, "ActorEmitter"}, {kVelocityParm, "VelocityParm"},
    {kAngularVelocityParm, "AngularVelocityParm"}, {kCustomParm, "CustomParm"}, {kAnalogInputParm, "AnalogInputParm"}, {kMaxDistanceFromListener, "MaxDistanceFromListener"},
    {kPauseWhenAirborne, "PauseWhenAirborne"}, {kTriggeredOnly, "TriggeredOnly"},
};

enum CollisionToken {
    kCollisionSoundNodes = 0x39, kCollisionSoundNode = 0x3A, kDedicatedEmitter = 0x3B, kDefaultSegment = 0x3C, kMaterialSegments = 0x3D, kPlayerInitiatedOnly = 0x3E, kRagdollOnly = 0x3F,
    kImpulseThresholds = 0x40
};

// 0x002B9038.
const AtIoKeyword kCollisionKeywords[] = {
    {kCollisionSoundNodes, "CollisionSoundNodes"}, {kCollisionSoundNode, "CollisionSoundNode"}, {kDedicatedEmitter, "DedicatedEmitter"}, {kDefaultSegment, "DefaultSegment"},
    {kMaterialSegments, "MaterialSegments"}, {kPlayerInitiatedOnly, "PlayerInitiatedOnly"}, {kRagdollOnly, "RagdollOnly"}, {kImpulseThresholds, "ImpulseThresholds"},
};

enum InstanceToken { kInstanceSoundNodes = 0x39, kInstanceSoundNode = 0x3A, kInstances = 0x3B };

// 0x002B9078.
const AtIoKeyword kInstanceKeywords[] = {
    {kInstanceSoundNodes, "InstanceSoundNodes"}, {kInstanceSoundNode, "InstanceSoundNode"}, {kInstances, "Instances"},
};

// The original clears a whole name, as two words.
void ClearName(char* name) {
    for (int i = 0; i < 8; i++) {
        name[i] = 0;
    }
}

void* AllocateAudio(unsigned int size) {
    if (GmAudioData::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(GmAudioData::s_heap);
    void* memory = operator new(size);
    AtPopHeap();
    return memory;
}

// What the original's new[] does to each node before the reader clears it all again: the first byte of each name, and the other fields.
void ConstructNode(GmSoundNode* node) {
    node->m_name[0] = 0;
    node->m_actor[0] = 0;
    node->m_owner = 0;
    node->m_source = 0;
    node->m_emitter = 0;
    node->m_segmentName[0] = 0;
    node->m_segment = 0;
    node->m_velocityParm[0] = 0;
    node->m_velocitySecond = 0;
    node->m_velocityFirst = 0;
    node->m_angularVelocityParm[0] = 0;
    node->m_angularVelocitySecond = 0;
    node->m_angularVelocityFirst = 0;
    node->m_customParm[0] = 0;
    node->m_customSecond = 0;
    node->m_customFirst = 0;
    node->m_unknown58 = 0;
    node->m_analogInputThird[0] = 0;
    node->m_analogInputSecond = 0;
    node->m_analogInputFirst = 0;
    node->m_analogInputParm[0] = 0;
    node->m_analogInputSecondName[0] = 0;
    node->m_range = 0;
    node->m_pauseWhenAirborne = 0;
    node->m_unknown84 = 0;
    node->m_triggeredOnly = 0;
    node->m_active = 0;
}

void ClearNode(GmSoundNode* node) {
    ClearName(node->m_name);
    ClearName(node->m_actor);
    node->m_owner = 0;
    node->m_source = 0;
    node->m_emitter = 0;
    ClearName(node->m_segmentName);
    node->m_segment = 0;
    ClearName(node->m_velocityParm);
    node->m_velocitySecond = 0;
    node->m_velocityFirst = 0;
    ClearName(node->m_angularVelocityParm);
    node->m_angularVelocitySecond = 0;
    node->m_angularVelocityFirst = 0;
    ClearName(node->m_customParm);
    node->m_customSecond = 0;
    node->m_customFirst = 0;
    node->m_unknown58 = 0;
    ClearName(node->m_analogInputParm);
    ClearName(node->m_analogInputSecondName);
    ClearName(node->m_analogInputThird);
    node->m_analogInputSecond = 0;
    node->m_analogInputFirst = 0;
    node->m_range = 0;
    node->m_pauseWhenAirborne = 0;
    node->m_unknown84 = 0;
    node->m_triggeredOnly = 0;
    node->m_active = 0;
}

// One Node block, after its name. Only a right curly or the end of the text ends it; any other word or token goes by.
void ReadNodeBody(GmSoundNode* node, AtIoTokenizer* t) {
    for (int token = t->Next(); token != AtIoTokenizer::kRightCurly && token != AtIoTokenizer::kEnd; token = t->Next()) {
        switch (token) {
        case kActor: t->ExpectName(node->m_actor); break;
        case kSegment: t->ExpectName(node->m_segmentName); break;
        case kEmitter: {
            char name[8];
            t->ExpectName(name);
            node->m_emitter = (AtAudioEmitter*)GmAudioData::s_soundOwner->m_nodes.Find(name);
            break;
        }
        case kVelocityParm:
            t->ExpectName(node->m_velocityParm);
            node->m_velocityFirst = t->ExpectReal();
            node->m_velocitySecond = t->ExpectReal();
            break;
        case kAngularVelocityParm:
            t->ExpectName(node->m_angularVelocityParm);
            node->m_angularVelocityFirst = t->ExpectReal();
            node->m_angularVelocitySecond = t->ExpectReal();
            break;
        case kCustomParm:
            t->ExpectName(node->m_customParm);
            node->m_customFirst = t->ExpectReal();
            node->m_customSecond = t->ExpectReal();
            break;
        case kAnalogInputParm:
            t->ExpectName(node->m_analogInputParm);
            t->ExpectName(node->m_analogInputSecondName);
            t->ExpectName(node->m_analogInputThird);
            node->m_analogInputFirst = t->ExpectReal();
            node->m_analogInputSecond = t->ExpectReal();
            break;
        case kPauseWhenAirborne: node->m_pauseWhenAirborne = 1; break;
        case kTriggeredOnly:
            node->m_triggeredOnly = 1;
            node->m_active = 0;
            break;
        default: break;  // ActorEmitter, and MaxDistanceFromListener, whose value then goes by as a token of its own.
        }
    }
}

}  // namespace

// ---- sound nodes ----------------------------------------------------------------------------------------------------------

GmSoundNodeBank* GmAudioData::AllocateNodeBank() {
    for (unsigned int i = 0; i < kNodeBanks; i++) {
        if (m_nodeBanks[i].m_used == 0) {
            m_nodeBanks[i].m_used = 1;
            return &m_nodeBanks[i];
        }
    }
    return 0;
}

void GmAudioData::ReleaseNodeBank(GmSoundNodeBank* bank) {
    for (unsigned int i = 0; i < kNodeBanks; i++) {
        if (&m_nodeBanks[i] == bank) {
            m_nodeBanks[i].m_used = 0;
            return;
        }
    }
}

void GmAudioData::ReadNodes(AtIoTokenizer* t, GmSoundNodeBank* bank) {
    bank->m_count = (unsigned int)t->ExpectCount();
    const unsigned int count = bank->m_count;
    GmSoundNode* nodes = (GmSoundNode*)AllocateAudio(count * sizeof(GmSoundNode));
    if (nodes != 0) {
        for (unsigned int i = 0; i < count; i++) {
            ConstructNode(&nodes[i]);
        }
    }
    bank->m_nodes = nodes;
    if (bank->m_nodes == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < bank->m_count; i++) {
        GmSoundNode* node = &bank->m_nodes[i];
        ClearNode(node);
        t->Next();  // The Node word, not tested.
        t->ExpectName(node->m_name);
        AtCopyName8(node->m_actor, node->m_name);
        t->ExpectLeftCurly();
        ReadNodeBody(node, t);
    }
}

void GmAudioData::LoadNodes(GmSoundNodeBank* bank, const char* path) {
    AtIoTokenFile file;
    const int status = file.Open(path, "GOO", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "GOO");
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kNodeKeywords, sizeof kNodeKeywords / sizeof kNodeKeywords[0], 0);
    for (int token = t->Next(); token != AtIoTokenizer::kEnd; token = t->Next()) {
        if (token == kSoundNodes) {
            ReadNodes(t, bank);
        }
    }
    // Not Close: the tokenizer is reset, and the file's destructor does the rest.
    t->Reset();
}

void GmAudioData::FreeNodes(GmSoundNodeBank* bank) {
    if (bank->m_nodes != 0) {
        for (unsigned int i = 0; i < bank->m_count; i++) {
            GmSoundNode* node = &bank->m_nodes[i];
            if (node->m_emitter == 0) {
                continue;
            }
            node->m_emitter->StopAll();
            while (node->m_emitter->m_firstVoice != 0) {
                s_soundOwner->m_loader->Wait(kWaitSeconds);
            }
            if (node->m_emitter->m_kind == 1) {
                ReleaseVoiceSlot(node->m_emitter);
            }
            node->m_emitter = 0;
        }
        operator delete(bank->m_nodes);
        bank->m_nodes = 0;
    }
    bank->m_count = 0;
}

void GmAudioData::RegisterNodes(GmSoundNodeBank* bank) {
    for (unsigned int i = 0; i < bank->m_count; i++) {
        GmSoundNode* node = &bank->m_nodes[i];
        node->m_segment = FindSegment(node->m_segmentName);
        if (node->m_segment != 0) {
            node->m_range = node->m_segment->Unknown31BF0();
        }
    }
}

// ---- instance sounds ------------------------------------------------------------------------------------------------------

GmInstanceSoundBank* GmAudioData::AllocateInstanceBank(unsigned int count) {
    for (unsigned int i = 0; i < kSoundBanks; i++) {
        GmInstanceSoundBank* bank = &m_instanceBanks[i];
        if (bank->m_used != 0) {
            continue;
        }
        GmInstanceSound* sounds = (GmInstanceSound*)AllocateAudio(count * sizeof(GmInstanceSound));
        if (sounds != 0) {
            for (unsigned int k = 0; k < count; k++) {
                sounds[k].m_name[0] = 0;
                sounds[k].m_collision = 0;
                sounds[k].m_instanceCount = 0;
                sounds[k].m_instances = 0;
                sounds[k].m_unknown14 = 0;
            }
        }
        bank->m_sounds = sounds;
        if (bank->m_sounds == 0) {
            AtRaise(-4, 0, 0, 0);
            if (bank->m_sounds == 0) {
                return bank;  // Neither used nor counted, and handed back regardless.
            }
        }
        bank->m_used = 1;
        bank->m_count = count;
        return bank;
    }
    return 0;
}

GmInstanceSoundBank* GmAudioData::LoadInstanceSounds(const char* path) {
    AtIoTokenFile file;
    const int status = file.Open(path, "GOI", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "GOI");
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kInstanceKeywords, sizeof kInstanceKeywords / sizeof kInstanceKeywords[0], 0);
    t->ExpectToken(kInstanceSoundNodes);
    const unsigned int count = (unsigned int)t->ExpectCount();
    GmInstanceSoundBank* bank = AllocateInstanceBank(count);
    if (bank == 0) {
        file.Close();
        return 0;
    }
    for (unsigned int i = 0; i < count; i++) {
        GmInstanceSound* sound = &bank->m_sounds[i];
        t->ExpectToken(kInstanceSoundNode);
        t->ExpectName(sound->m_name);
        sound->m_collision = 0;
        sound->m_instanceCount = 0;
        sound->m_instances = 0;
        sound->m_unknown14 = 0;
        t->ExpectLeftCurly();
        // Only a right curly ends this: at the end of the text the original spins, if nothing has stopped it first.
        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {
            if (token != kInstances) {
                continue;
            }
            sound->m_instanceCount = (unsigned int)t->ExpectCount();
            sound->m_instances = (char(*)[8])AllocateAudio(sound->m_instanceCount * 8);
            if (sound->m_instances == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            sound->m_unknown14 = (void**)AllocateAudio(sound->m_instanceCount * 4);
            if (sound->m_unknown14 == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int k = 0; k < sound->m_instanceCount; k++) {
                t->ExpectName(sound->m_instances[k]);
            }
            t->ExpectRightCurly();
        }
    }
    t->ExpectRightCurly();
    file.Close();
    return bank;
}

void GmAudioData::FreeInstanceSounds(GmInstanceSoundBank* bank) {
    if (bank->m_sounds != 0) {
        for (unsigned int i = 0; i < bank->m_count; i++) {
            GmInstanceSound* sound = &bank->m_sounds[i];
            if (sound->m_instances != 0) {
                operator delete(sound->m_instances);
            }
            if (sound->m_unknown14 != 0) {
                operator delete(sound->m_unknown14);
            }
            sound->m_name[0] = 0;
            sound->m_collision = 0;
            sound->m_instanceCount = 0;
            sound->m_instances = 0;
            sound->m_unknown14 = 0;
        }
        operator delete(bank->m_sounds);
    }
    bank->m_used = 0;
    bank->m_count = 0;
    bank->m_sounds = 0;
}

void GmAudioData::RegisterInstanceSounds(GmInstanceSoundBank* bank) {
    for (unsigned int i = 0; i < bank->m_count; i++) {
        GmInstanceSound* sound = &bank->m_sounds[i];
        for (unsigned int b = 0; b < kSoundBanks; b++) {
            GmCollisionSoundBank* collisions = &m_collisionBanks[b];
            if (collisions->m_used != 1) {
                continue;
            }
            for (unsigned int k = 0; k < collisions->m_count; k++) {
                if (AtNamesEqual(sound->m_name, collisions->m_sounds[k].m_name) == 1) {
                    sound->m_collision = &collisions->m_sounds[k];
                }
            }
        }
    }
}

// ---- collision sounds -----------------------------------------------------------------------------------------------------

GmCollisionSoundBank* GmAudioData::LoadCollisionSounds(const char* path) {
    AtIoTokenFile file;
    const int status = file.Open(path, "GOC", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "GOC");
    }
    GmCollisionSoundBank* bank = 0;
    for (unsigned int i = 0; i < kSoundBanks; i++) {
        if (m_collisionBanks[i].m_used == 0) {
            m_collisionBanks[i].m_used = 1;
            bank = &m_collisionBanks[i];
            break;
        }
    }
    if (bank == 0) {
        file.Close();
        return 0;
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kCollisionKeywords, sizeof kCollisionKeywords / sizeof kCollisionKeywords[0], 0);
    // Each CollisionSoundNodes block replaces what an earlier one read, which leaks. Its right curly goes by as a token of this loop.
    for (int token = t->Next(); token != AtIoTokenizer::kEnd; token = t->Next()) {
        if (token != kCollisionSoundNodes) {
            continue;
        }
        bank->m_count = (unsigned int)t->ExpectCount();
        const unsigned int count = bank->m_count;
        GmCollisionSound* sounds = (GmCollisionSound*)AllocateAudio(count * sizeof(GmCollisionSound));
        if (sounds != 0) {
            for (unsigned int i = 0; i < count; i++) {
                sounds[i].m_name[0] = 0;
                sounds[i].m_emitter = 0;
                sounds[i].m_materialCount = 0;
                sounds[i].m_materials = 0;
                sounds[i].m_playerInitiatedOnly = 0;
                sounds[i].m_ragdollOnly = 0;
                for (int k = 0; k < 3; k++) {
                    sounds[i].m_defaultSegments[k][0] = 0;
                    sounds[i].m_impulseThresholds[k] = 0;
                }
            }
        }
        bank->m_sounds = sounds;
        if (bank->m_sounds == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < bank->m_count; i++) {
            GmCollisionSound* sound = &bank->m_sounds[i];
            for (int k = 0; k < 3; k++) {
                ClearName(sound->m_defaultSegments[k]);
                sound->m_impulseThresholds[k] = m_impulseThresholds[k];
            }
            ClearName(sound->m_name);
            sound->m_emitter = 0;
            sound->m_materialCount = 0;
            sound->m_playerInitiatedOnly = 0;
            sound->m_ragdollOnly = 0;
            sound->m_materials = 0;
            t->Next();  // The CollisionSoundNode word, not tested.
            t->ExpectName(sound->m_name);
            t->ExpectLeftCurly();
            for (int word = t->Next(); word != AtIoTokenizer::kRightCurly && word != AtIoTokenizer::kEnd; word = t->Next()) {
                switch (word) {
                case kDedicatedEmitter: sound->m_emitter = TakeVoiceSlot(); break;
                case kDefaultSegment:
                    for (int k = 0; k < 3; k++) {
                        t->ExpectName(sound->m_defaultSegments[k]);
                    }
                    break;
                case kMaterialSegments:
                    sound->m_materialCount = (unsigned int)t->ExpectCount();
                    if (sound->m_materialCount != 0) {
                        const unsigned int materials = sound->m_materialCount;
                        GmCollisionMaterial* list = (GmCollisionMaterial*)AllocateAudio(materials * sizeof(GmCollisionMaterial));
                        if (list != 0) {
                            for (unsigned int k = 0; k < materials; k++) {
                                ClearName(list[k].m_material);
                                ClearName(list[k].m_segment);
                            }
                        }
                        sound->m_materials = list;
                        if (sound->m_materials == 0) {
                            AtRaise(-4, 0, 0, 0);
                        }
                        for (unsigned int k = 0; k < sound->m_materialCount; k++) {
                            t->ExpectName(sound->m_materials[k].m_material);
                            t->ExpectName(sound->m_materials[k].m_segment);
                        }
                    }
                    t->ExpectRightCurly();
                    break;
                case kPlayerInitiatedOnly: sound->m_playerInitiatedOnly = 1; break;
                case kRagdollOnly: sound->m_ragdollOnly = 1; break;
                case kImpulseThresholds:
                    for (int k = 0; k < 3; k++) {
                        sound->m_impulseThresholds[k] = t->ExpectReal();
                    }
                    break;
                default: break;
                }
            }
        }
    }
    file.Close();
    return bank;
}

void GmAudioData::FreeCollisionSounds(GmCollisionSoundBank* bank) {
    if (bank->m_sounds != 0) {
        for (unsigned int i = 0; i < bank->m_count; i++) {
            GmCollisionSound* sound = &bank->m_sounds[i];
            for (int k = 0; k < 3; k++) {
                ClearName(sound->m_defaultSegments[k]);
            }
            ClearName(sound->m_name);
            ReleaseVoiceSlot(sound->m_emitter);
            sound->m_emitter = 0;
            sound->m_playerInitiatedOnly = 0;
            sound->m_ragdollOnly = 0;
            sound->m_materialCount = 0;
            if (sound->m_materials != 0) {
                operator delete(sound->m_materials);  // Not cleared after: the whole array goes next.
            }
        }
        operator delete(bank->m_sounds);
        bank->m_sounds = 0;
    }
    bank->m_used = 0;
    bank->m_count = 0;
}
