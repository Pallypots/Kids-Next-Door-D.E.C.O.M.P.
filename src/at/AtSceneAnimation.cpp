// What actors animate with: the channels, the set of them and room to blend, and the copy of a database of coordinate systems. See include/at/AtSceneActor.h.
#include "at/AtSceneActor.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtSceneAnimBlend::s_heap;
void* AtSceneAnimSet::s_heap;

namespace {

void CheckHeap(void* heap) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
}

void ConstructMatChannel(void* where) {
    new (where, AtPlacement()) AtSceneMatChannel;
}

// Elements of the blend have nothing to construct. The original still goes through its loop with an empty function, 0x00057E20.
void ConstructNothing(void*) {}

template <class T> T* NewBlendArray(unsigned int count) {
    CheckHeap(AtSceneAnimBlend::s_heap);
    AtPushHeap(AtSceneAnimBlend::s_heap);
    void* block = operator new(count * sizeof(T));
    if (block != 0) {
        AtConstructEach(block, sizeof(T), (int)count, ConstructNothing);
    }
    AtPopHeap();
    if (block == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return (T*)block;
}

}  // namespace

// ---- playback ----------------------------------------------------------------------------------------------------------------

void AtScenePlayback::Rewind() {
    m_flags = kWasRewound;
    m_time = 0;
}

AtSceneMatPlayback::AtSceneMatPlayback() {
    m_animation = 0;
}

AtSceneMatPlayback::~AtSceneMatPlayback() {
    m_flags = 0;
    m_time = 0;
    m_rate = 0;
    m_listener = 0;
    m_animation = 0;
}

void AtSceneMatPlayback::SetAnimation(AtSceneMatAnimation* animation) {
    m_animation = animation;
    m_rate = animation->m_rate;
    Rewind();
}

AtSceneMatChannel::AtSceneMatChannel() {
    m_options = 0;
    m_assignmentOffset = 0;
}

AtSceneMatChannel::~AtSceneMatChannel() {
    m_options = 0;
    m_assignmentOffset = 0;
}

void AtSceneMatChannel::Read(AtSceneDevice* device, AtIoTokenizer* tokenizer) {
    int rateGiven = 0;
    float rate = 0;
    tokenizer->ExpectToken(kAtWordMatAnimationChannel);
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kAtWordPlaybackRate:
            rate = tokenizer->ExpectReal();
            rateGiven = 1;
            break;
        case kAtWordMatAssignmentOffset:
            m_assignmentOffset = tokenizer->ExpectInt();
            break;
        case kAtWordPlayMatAnimation: {
            char name[8];
            char ns[8];
            tokenizer->ExpectName(name);
            tokenizer->ExpectName(ns);
            AtSceneMatAnimation* animation = (AtSceneMatAnimation*)(ns[0] != 0 ? device->m_matAnimationSpaces.Find(ns, name) : device->m_matAnimations.Find(name));
            if (animation == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            SetAnimation(animation);
            m_flags |= kStarted;
            if (m_listener != 0) {
                m_listener->OnStarted(this);
            }
            m_options |= 1;
            m_flags |= kPlaying;
            const unsigned int start = (unsigned int)tokenizer->ExpectInt();
            if ((m_options & 1) != 0) {
                m_time = (float)(int)(start % m_animation->m_frames);
            }
            break;
        }
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    if (rateGiven != 0) {
        m_rate = rate;
    }
}

AtSceneAnimPlayback::AtSceneAnimPlayback() {
    m_animation = 0;
    m_unknown14 = 0;
    m_translationScale.x = 1.0f;
    m_translationScale.y = 1.0f;
    m_translationScale.z = 1.0f;
    m_speed = 1.0f;
}

void AtSceneAnimPlayback::SetAnimation(AtSceneAnimation* animation) {
    m_animation = animation;
    m_rate = animation->m_rate;
    m_speed = 1.0f;
    Rewind();
}

AtSceneAnimChannel::AtSceneAnimChannel() {
    m_weights = 0;
    m_options = 0;
    m_mirrors = 0;
    m_unknown38 = 0;
    m_weightMultiplier = 1.0f;
}

void AtSceneAnimChannel::Read(AtSceneDevice* device, AtIoTokenizer* tokenizer, int word) {
    int rateGiven = 0;
    float rate = 0;
    char name[8];
    tokenizer->ExpectToken(word);
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kAtWordPlayAnimation: {
            tokenizer->ExpectName(name);
            GmNameMap* animations = (GmNameMap*)device->m_animationDatabases.Find(name);
            if (animations == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            tokenizer->ExpectName(name);
            AtSceneAnimation* animation = (AtSceneAnimation*)animations->Find(name);
            if (animation == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            SetAnimation(animation);
            m_flags |= kStarted;
            if (m_listener != 0) {
                m_listener->OnStarted(this);
            }
            m_options |= kPlaying;
            m_flags |= AtScenePlayback::kPlaying;
            const unsigned int start = (unsigned int)tokenizer->ExpectInt();
            if ((m_options & kPlaying) != 0) {
                m_time = (float)(int)(start % m_animation->m_frames);
            }
            break;
        }
        case kAtWordWeights: {
            tokenizer->ExpectName(name);
            GmNameMap* weights = (GmNameMap*)device->m_weightDatabases.Find(name);
            if (weights == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            tokenizer->ExpectName(name);
            m_weights = weights->Find(name);
            if (m_weights == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            break;
        }
        case kAtWordWeightMultiplier:
            m_weightMultiplier = tokenizer->ExpectReal();
            break;
        case kAtWordLinearVelocity:
            m_options |= kLinearVelocity;
            break;
        case kAtWordAngularVelocity:
            m_options |= kAngularVelocity;
            break;
        case kAtWordPlaybackRate:
            rate = tokenizer->ExpectReal();
            rateGiven = 1;
            break;
        case kAtWordCoordSysMirrorDatabase:
            tokenizer->ExpectName(name);
            m_mirrors = device->m_mirrorDatabases.Find(name);
            if (m_mirrors == 0) {
                tokenizer->Error(kAtProblemNotFound);
            }
            break;
        case kAtWordCoordSysMirroringEnable:
            m_options |= kMirroring;
            break;
        case kAtWordCoordSysMirroringDisable:
            m_options &= ~(unsigned int)kMirroring;
            break;
        case kAtWordScaleTranslations: {
            AtVector3 scale;
            scale.Read(tokenizer);
            m_flags |= kHasTranslationScale;
            m_translationScale = scale;
            break;
        }
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    if (rateGiven != 0) {
        m_rate = rate;
        if (m_animation != 0) {
            m_speed = rate / m_animation->m_rate;
        }
    }
}

// ---- room to blend -----------------------------------------------------------------------------------------------------------

void AtSceneAnimBlend::SetHeap(void* heap) {
    s_heap = heap;
}

AtSceneAnimBlend::AtSceneAnimBlend() {
    m_rotations = 0;
    m_translations = 0;
    m_scales = 0;
    m_rotTransCount = 0;
    m_scaleCount = 0;
    m_unknown14 = 0;
}

void AtSceneAnimBlend::Clear() {
    if (m_rotations != 0) {
        operator delete(m_rotations);
    }
    if (m_translations != 0) {
        operator delete(m_translations);
    }
    if (m_scales != 0) {
        operator delete(m_scales);
    }
    m_rotations = 0;
    m_translations = 0;
    m_scales = 0;
    m_rotTransCount = 0;
    m_scaleCount = 0;
    m_unknown14 = 0;
}

void AtSceneAnimBlend::Reset() {
    for (unsigned int i = 0; i < m_rotTransCount; i++) {
        m_rotations[i].value[0] = 0;
    }
    for (unsigned int i = 0; i < m_rotTransCount; i++) {
        m_translations[i].value[0] = 0;
    }
    for (unsigned int i = 0; i < m_scaleCount; i++) {
        m_scales[i].value[0] = 0;
    }
}

void AtSceneAnimBlend::Allocate(const AtCoordSysDatabase* database) {
    if (m_rotTransCount > 0 || m_scaleCount > 0) {
        Clear();
    }
    m_rotTransCount = database->m_rotTransCount;
    m_scaleCount = database->m_scaledCount;
    if (m_rotTransCount > 0) {
        m_rotations = NewBlendArray<Rotation>(m_rotTransCount);
        m_translations = NewBlendArray<Vector>(m_rotTransCount);
    }
    if (m_scaleCount > 0) {
        m_scales = NewBlendArray<Vector>(m_scaleCount);
    }
    Reset();
}

// ---- the set -----------------------------------------------------------------------------------------------------------------

void AtSceneAnimSet::SetHeap(void* heap) {
    s_heap = heap;
}

AtSceneAnimSet::AtSceneAnimSet() {
    m_database = 0;
    m_channelCount = 0;
    m_channels = 0;
    m_unknown24 = 0;
}

AtSceneAnimSet::~AtSceneAnimSet() {
    m_blend.Clear();
    if (m_channels != 0) {
        operator delete(m_channels);
    }
    m_database = 0;
    m_channelCount = 0;
    m_channels = 0;
    m_unknown24 = 0;
}

void AtSceneAnimSet::Clear() {
    m_blend.Clear();
    if (m_channels != 0) {
        operator delete(m_channels);
    }
    m_database = 0;
    m_channelCount = 0;
    m_channels = 0;
    m_unknown24 = 0;
}

void AtSceneAnimSet::SetDatabase(AtCoordSysDatabase* database) {
    m_database = database;
}

void AtSceneAnimSet::PrepareBlend(int always) {
    if (always == 0 && m_channelCount <= 1) {
        return;
    }
    if (m_database != 0) {
        m_blend.Allocate(m_database);
    }
}

void AtSceneAnimSet::ReadDatabase(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_database != 0) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    char name[8];
    tokenizer->ExpectName(name);
    m_database = (AtCoordSysDatabase*)owner->m_device->m_coordSysDatabases.Find(name);
    if (m_database == 0) {
        tokenizer->Error(kAtProblemNotFound);
    }
}

void AtSceneAnimSet::ReadChannels(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int word) {
    if (m_channelCount > 0) {
        tokenizer->Error(kAtProblemWrongUse);
    }
    m_channelCount = (unsigned int)tokenizer->ExpectCount();
    if (m_channelCount < 1) {
        tokenizer->Error(kAtProblemIsEmpty);
    }
    CheckHeap(s_heap);
    AtPushHeap(s_heap);
    const unsigned int count = m_channelCount;
    AtSceneAnimChannel* channels = (AtSceneAnimChannel*)operator new(count * sizeof(AtSceneAnimChannel));
    if (channels != 0) {
        for (int i = 0; i < (int)count; i++) {
            new (channels + i, AtPlacement()) AtSceneAnimChannel;
        }
    }
    m_channels = channels;
    AtPopHeap();
    if (m_channels == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_channelCount; i++) {
        m_channels[i].Read(owner->m_device, tokenizer, word);
    }
    tokenizer->ExpectRightCurly();
}

// ---- copying a database ------------------------------------------------------------------------------------------------------

void AtCoordSys::CopyFromDatabase(AtCoordSysDatabase* database, AtCoordSysDatabase* from, const AtCoordSys* source) {
    g_atMatrix.Copy(m_local, source->m_local);
    const AtCoordSys* parent = source->m_parent;
    if (parent == 0) {
        return;
    }
    const AtRotTransCoordSys* rotTrans = from->m_rotTrans;
    if (parent >= rotTrans && parent < rotTrans + from->m_rotTransCount) {
        database->m_rotTrans[(const AtRotTransCoordSys*)parent - rotTrans].Attach(this);
        return;
    }
    const AtScaleCoordSys* scaled = from->m_scaled;
    if (parent >= scaled && parent < scaled + from->m_scaledCount) {
        database->m_scaled[(const AtScaleCoordSys*)parent - scaled].Attach(this);
    }
}

void AtRotTransCoordSys::CopyFromDatabase(AtCoordSysDatabase* database, AtCoordSysDatabase* from, unsigned int index) {
    AtCoordSys::CopyFromDatabase(database, from, &from->m_rotTrans[index]);
}

void AtScaleCoordSys::CopyFromDatabase(AtCoordSysDatabase* database, AtCoordSysDatabase* from, unsigned int index) {
    AtCoordSys::CopyFromDatabase(database, from, &from->m_scaled[index]);
}

// The part both kinds share: matrices, the name, and what a second matrix is for when the source has one.
void AtCoordSysDatabase::TakeMatricesFor(AtCoordSys* mine, AtCoordSysDatabase* from, const AtCoordSys* source) {
    AtMatrix* world = m_pool->At(m_nextMatrix + 1);
    AtMatrix* local = m_pool->At(m_nextMatrix);
    mine->SetMatrices(local, world);
    char name[8];
    from->GetNameOf(source, name);
    if (name[0] != 0) {
        Insert(name, mine);
    }
}

void AtCoordSysDatabase::TakeDrawingMatricesFor(AtCoordSys* mine, const AtCoordSys* source) {
    m_nextMatrix += 2;
    if (source->m_unknown10 != 0) {
        AtMatrix* first = m_pool->At(m_nextMatrix++);
        g_atMatrix.Copy(first, source->m_unknown10);
        AtMatrix* second = m_pool->At(m_nextMatrix++);
        mine->m_unknown10 = first;
        mine->m_unknown1C = second;
    }
}

void AtCoordSysDatabase::CopyFrom(AtMatrixDevice* device, AtCoordSysDatabase* from) {
    if (m_entries != 0) {
        Clear();
    }
    m_device = device;
    m_rotTransCount = from->m_rotTransCount;
    m_scaledCount = from->m_scaledCount;
    m_extra = from->m_extra;
    Init(m_rotTransCount + m_scaledCount);
    m_pool = device->NewPool();
    m_pool->Reserve((m_rotTransCount + m_scaledCount + m_extra) * 2);
    m_nextMatrix = 0;
    if (m_rotTransCount > 0) {
        CheckHeap(s_heap);
        AtPushHeap(s_heap);
        m_rotTrans = AtNewArray<AtRotTransCoordSys>(m_rotTransCount);
        AtPopHeap();
        if (m_rotTrans == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    if (m_scaledCount > 0) {
        CheckHeap(s_heap);
        AtPushHeap(s_heap);
        m_scaled = AtNewArray<AtScaleCoordSys>(m_scaledCount);
        AtPopHeap();
        if (m_scaled == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
    for (unsigned int i = 0; i < m_rotTransCount; i++) {
        const AtRotTransCoordSys* source = &from->m_rotTrans[i];
        TakeMatricesFor(&m_rotTrans[i], from, source);
        m_rotTrans[i].CopyFromDatabase(this, from, i);
        if (source == from->m_unknown2C) {
            m_unknown2C = &m_rotTrans[i];
        }
        TakeDrawingMatricesFor(&m_rotTrans[i], source);
    }
    for (unsigned int i = 0; i < m_scaledCount; i++) {
        const AtScaleCoordSys* source = &from->m_scaled[i];
        TakeMatricesFor(&m_scaled[i], from, source);
        m_scaled[i].CopyFromDatabase(this, from, i);
        if (source == from->m_unknown2C) {
            m_unknown2C = &m_scaled[i];
        }
        TakeDrawingMatricesFor(&m_scaled[i], source);
    }
}
