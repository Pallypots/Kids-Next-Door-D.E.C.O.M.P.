// WaterActor, LensFlareActor and SoundNode, with the coordinate system functions a sound node reaches. See include/at/AtSceneNodes.h.
#include "at/AtSceneNodes.h"

#include "at/AtAudioDatabase.h"
#include "at/AtSceneParticle.h"
#include "AtArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

AtSceneWaterPatch* AtSceneWaterActor::s_patches;
AtScenePendingSegment* AtSceneSoundNode::s_pending;

namespace {

const unsigned short kClassWater = 9;
const unsigned short kClassLensFlare = 10;
const unsigned short kClassSound = 16;
const float kDefaultRate = 60.0f;
const float kFlareRadius = 0.0001f;
const unsigned int kSoundBit = 0x800;

void NotRecovered() {
    AtFatal("Atlas Scene: this node slot is not recovered", 0, 0, 0);
}

// The original goes through its array constructor with an empty function, 0x00057E20: a patch has nothing to construct.
void ConstructPatch(void*) {
}

void CheckHeap(void* heap) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
}

// The sound owner: an owner of the same layout, at the scene owner's +0x14.
AtNameTableOwner* SoundOwner(AtNameTableOwner* owner) {
    return owner->m_soundOwner;
}

}  // namespace

// ---- coordinate systems: a world matrix given from outside ------------------------------------------------------------------

// MarkChildrenDirty (0x00032310) and the world-to-local step (0x00032DF0, AtCoordSys::LocalFromWorld) are gm/gmsound's: src/at/AtAudioVoices.cpp.

void AtCoordSys::SetWorldFrom(AtCoordSys* from, const AtVector3* scale) {
    from->UpdateWorld();
    g_atMatrix.Copy(m_world, from->m_world);
    if (scale != 0) {
        g_atMatrix.ScaleAxes(m_world, scale);
    }
    LocalFromWorld();
}

void AtCoordSys::SetWorld(const AtQuaternion* rotation, const AtVector3* position, const AtVector3* scale) {
    g_atMatrix.MakeRotation(m_world, rotation);
    g_atMatrix.SetTranslation(m_world, position);
    if (scale != 0) {
        g_atMatrix.ScaleAxes(m_world, scale);
    }
    LocalFromWorld();
}

// ---- water ---------------------------------------------------------------------------------------------------------------------

// Slots 2 and 3 of the maker, for water. The running game's class answers them; nothing here makes one.
AtSceneWaterDrawer* AtSceneParticleDrawerMaker::NewWater() {
    AtFatal("Atlas Scene: no water drawer is supplied", 0, 0, 0);
    return 0;
}

void AtSceneParticleDrawerMaker::ReleaseWater(AtSceneWaterDrawer*) {
    AtFatal("Atlas Scene: no water drawer is supplied", 0, 0, 0);
}

AtSceneWaterActor::AtSceneWaterActor() {
    m_drawer = 0;
    m_matAssignmentOffset = 0;
    m_patchCount = 0;
    m_damping = 0.0f;
    m_generation = 0.0f;
    m_class = kClassWater;
    m_rate = kDefaultRate;
}

AtSceneWaterActor::~AtSceneWaterActor() {
    AtSceneWaterActor::Clear();
}

void AtSceneWaterActor::Clear() {
    if (m_drawer != 0) {
        m_owner->m_particleDrawers->ReleaseWater(m_drawer);
    }
    m_drawer = 0;
    m_matAssignmentOffset = 0;
    m_patchCount = 0;
    m_damping = 0.0f;
    m_generation = 0.0f;
    m_class = kClassWater;
    m_rate = kDefaultRate;
}

void AtSceneWaterActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    if ((m_flags & kLoaded) != 0) {
        Clear();
    }
    AtSceneActor::Read(owner, scene, tokenizer);
    m_drawer = m_owner->m_particleDrawers->NewWater();
    m_drawer->Start(owner->m_device, m_patchCount, s_patches, m_rate, m_damping, m_generation);
    operator delete(s_patches);
}

void AtSceneWaterActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kAtWordWaterPatch: {
        m_patchCount = (unsigned int)tokenizer->ExpectCount();
        CheckHeap(AtIoTokenFile::s_heap);
        AtPushHeap(AtIoTokenFile::s_heap);
        const unsigned int count = m_patchCount;
        void* block = operator new(count * sizeof(AtSceneWaterPatch));
        if (block != 0) {
            AtConstructEach(block, sizeof(AtSceneWaterPatch), (int)count, ConstructPatch);
        }
        s_patches = (AtSceneWaterPatch*)block;
        AtPopHeap();
        if (s_patches == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        unsigned char* bytes = (unsigned char*)s_patches;
        for (unsigned int i = 0; i < m_patchCount * sizeof(AtSceneWaterPatch); i++) {
            bytes[i] = 0;
        }
        for (unsigned int i = 0; i < m_patchCount; i++) {
            tokenizer->ExpectToken(kAtWordWaterPatch);
            tokenizer->ExpectLeftCurly();
            AtSceneWaterPatch& patch = s_patches[i];
            for (int j = 0; j < 4; j++) {
                patch.m_neighbors[j] = -1;
            }
            for (int j = 0; j < 32; j++) {
                patch.m_mask[j] = -1;
            }
            for (int word = tokenizer->Next(); word != AtIoTokenizer::kRightCurly; word = tokenizer->Next()) {
                switch (word) {
                case kAtWordPositionRect:
                    for (int j = 0; j < 4; j++) {
                        patch.m_position[j] = tokenizer->ExpectReal();
                    }
                    break;
                case kAtWordTxtCoordRect:
                    for (int j = 0; j < 4; j++) {
                        patch.m_textureCoords[j] = tokenizer->ExpectReal();
                    }
                    break;
                case kAtWordNeighbors:
                    for (int j = 0; j < 4; j++) {
                        patch.m_neighbors[j] = tokenizer->ExpectInt();
                    }
                    break;
                case kAtWordMask:
                    tokenizer->ExpectLeftCurly();
                    for (int j = 0; j < 32; j++) {
                        patch.m_mask[j] = tokenizer->ExpectInt();
                    }
                    tokenizer->ExpectRightCurly();
                    break;
                default:
                    tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                    break;
                }
            }
        }
        tokenizer->ExpectRightCurly();
        break;
    }
    case kAtWordMatAssignmentOffset:
        m_matAssignmentOffset = tokenizer->ExpectInt();
        break;
    case kAtWordPlaybackRate:
        m_rate = tokenizer->ExpectReal();
        break;
    case kAtWordDampingFactor:
        m_damping = tokenizer->ExpectReal();
        break;
    case kAtWordGenerationFactor:
        m_generation = tokenizer->ExpectReal();
        break;
    default:
        AtSceneActor::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneWaterActor::Slot12() { NotRecovered(); }
void AtSceneWaterActor::Slot34() { NotRecovered(); }
void AtSceneWaterActor::Slot35() { NotRecovered(); }
void AtSceneWaterActor::Slot38() { NotRecovered(); }
void AtSceneWaterActor::Slot39() { NotRecovered(); }

// ---- lens flare ----------------------------------------------------------------------------------------------------------------

AtSceneLensFlareActor::AtSceneLensFlareActor() {
    m_unknown138 = 0;
    m_flare = 0;
    m_unknown9C = 0;
    m_sphere.radius = kFlareRadius;
    m_worldSphere.radius = kFlareRadius;
    m_class = kClassLensFlare;
}

AtSceneLensFlareActor::~AtSceneLensFlareActor() {
    AtSceneLensFlareActor::Clear();
}

void AtSceneLensFlareActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    m_sphere.radius = kFlareRadius;
    m_worldSphere.radius = kFlareRadius;
    AtSceneActor::Read(owner, scene, tokenizer);
}

void AtSceneLensFlareActor::Clear() {
    // Through the query's own slot, as the original calls it.
    AtSegmentHitList* query = &m_query;
    query->Clear();
    AtSceneActor::Clear();
    m_unknown138 = 0;
    m_flare = 0;
    m_unknown9C = 0;
    m_sphere.radius = kFlareRadius;
    m_worldSphere.radius = kFlareRadius;
}

void AtSceneLensFlareActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kAtWordLensFlare: {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        if (ns != 0 && ns[0] != 0) {
            m_flare = (AtLensFlare*)owner->m_lensFlareSpaces.Find(ns, name);
        } else {
            m_flare = (AtLensFlare*)owner->m_lensFlares.Find(name);
        }
        if (m_flare == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    }
    case kAtWordLineOfSightTestEnable:
        m_flags |= kLineOfSightTest;
        break;
    case kAtWordLineOfSightTestDisable:
        m_flags &= ~(unsigned int)kLineOfSightTest;
        break;
    default:
        AtSceneActor::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneLensFlareActor::Slot34() { NotRecovered(); }
void AtSceneLensFlareActor::Slot35() { NotRecovered(); }
void AtSceneLensFlareActor::Slot38() { NotRecovered(); }
void AtSceneLensFlareActor::Slot39() { NotRecovered(); }

// ---- sound ---------------------------------------------------------------------------------------------------------------------

AtSceneSoundNode::AtSceneSoundNode() {
    m_class = kClassSound;
    m_emitter = 0;
}

void AtSceneSoundNode::Clear() {
    AtSceneNode::Clear();
    m_emitter = 0;
}

void AtSceneSoundNode::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    char name[8];
    char ns[8];
    switch (token) {
    case kAtWordEmitter: {
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtNameTableOwner* sounds = SoundOwner(owner);
        if (sounds != 0) {
            if (ns != 0 && ns[0] != 0) {
                m_emitter = (AtAudioEmitter*)sounds->m_nodeSpaces.Find(ns, name);
            } else {
                m_emitter = (AtAudioEmitter*)sounds->m_nodes.Find(name);
            }
            if (m_emitter == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
        }
        break;
    }
    case kAtWordPlaySegment: {
        AtAudioSegment* segment = 0;
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        AtNameTableOwner* sounds = SoundOwner(owner);
        if (sounds != 0) {
            if (ns != 0 && ns[0] != 0) {
                segment = (AtAudioSegment*)sounds->m_secondSpaces.Find(ns, name);
            } else {
                segment = (AtAudioSegment*)sounds->m_second.Find(name);
            }
            if (segment == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
        }
        CheckHeap(AtIoTokenFile::s_heap);
        AtPushHeap(AtIoTokenFile::s_heap);
        AtScenePendingSegment* pending = (AtScenePendingSegment*)operator new(sizeof(AtScenePendingSegment));
        AtPopHeap();
        if (pending == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        pending->m_segment = segment;
        pending->m_start = tokenizer->ExpectInt();
        pending->m_next = s_pending;
        s_pending = pending;
        break;
    }
    default:
        AtSceneNode::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneSoundNode::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    AtSceneNode::Read(owner, scene, tokenizer);
    AtScenePendingSegment* pending = s_pending;
    while (pending != 0) {
        AtScenePendingSegment* next = pending->m_next;
        if (m_emitter != 0) {
            AtCoordSys* emitter = m_emitter;
            if (m_coordSys != 0) {
                emitter->SetWorldFrom(m_coordSys, 0);
            } else {
                const AtVector3 origin = {0.0f, 0.0f, 0.0f};
                const AtQuaternion identity = {0.0f, 0.0f, 0.0f, 1.0f};
                emitter->SetWorld(&identity, &origin, 0);
            }
            const AtVector3 still = {0.0f, 0.0f, 0.0f};
            AtSceneSoundSetVelocity(m_emitter, &still);
            AtSceneSoundPlay(m_emitter, pending->m_segment, 0);
        }
        operator delete(pending);
        pending = next;
    }
    s_pending = 0;
    m_flags |= kSoundBit;
}

void AtSceneSoundNode::Slot12() { NotRecovered(); }
