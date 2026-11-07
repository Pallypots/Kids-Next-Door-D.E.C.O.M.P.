// ParticleSystemActor. See include/at/AtSceneParticle.h. `double` stands for what the original keeps in an x87 register; the order of each sum is the original's.
#include "at/AtSceneParticle.h"

#include "at/AtRandom.h"
#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void* AtSceneParticleSystemActor::s_heap;
void* AtSceneParticleLayer::s_heap;

namespace {

const unsigned short kClassParticleSystem = 8;

void NotRecovered() {
    AtFatal("Atlas Scene: this particle system slot is not recovered", 0, 0, 0);
}

}  // namespace

// ---- the owner's table of emitters -----------------------------------------------------------------------------------------------

void* AtNameTableOwner::FindEmitter(const char* name, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        return m_emitterSpaces.Find(ns, name);
    }
    return m_emitters.Find(name);
}

// ---- a particle and a layer -------------------------------------------------------------------------------------------------------

AtSceneParticle::AtSceneParticle() {
    for (unsigned int i = 0; i < 0x10; i++) {
        m_words[i] = 0;
    }
}

void AtSceneParticleLayer::SetHeap(void* heap) {
    s_heap = heap;
}

void AtQuaternion::SetIdentity() {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    w = 1.0f;
}

AtSceneParticleLayer::AtSceneParticleLayer() {
    m_drawer = 0;
    m_emitter = 0;
    m_particles = 0;
    m_unknown0C = 0;
    m_unknown10 = 0;
    m_count = 0;
    m_unknown18 = 0;
    m_color.SetIdentity();
    m_unknown2C = 0;
    m_unknown30 = 0;
    m_unknown34 = 0;
}

AtSceneParticleLayer::~AtSceneParticleLayer() {
    Clear();
}

void AtSceneParticleLayer::Clear() {
    if (m_drawer != 0) {
        m_drawer->m_owner->m_particleDrawers->Release(m_drawer);
    }
    if (m_particles != 0) {
        operator delete(m_particles);
    }
    m_drawer = 0;
    m_emitter = 0;
    m_particles = 0;
    m_unknown0C = 0;
    m_unknown10 = 0;
    m_count = 0;
    m_unknown18 = 0;
    m_color.SetIdentity();
    m_unknown2C = 0;
    m_unknown30 = 0;
    m_unknown34 = 0;
}

void AtSceneParticleLayer::Start(AtNameTableOwner* owner, AtEmitter* emitter, const AtQuaternion* color) {
    if (m_emitter != 0) {
        Clear();
    }
    m_emitter = emitter;
    if (color != 0) {
        m_color = *color;
    } else {
        m_color.x = 0.0f;
        m_color.y = 0.0f;
        m_color.z = 0.0f;
        m_color.w = 0.0f;
    }
    m_count = emitter->m_maxParticles + 1;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    const unsigned int count = m_count;
    AtSceneParticle* particles = (AtSceneParticle*)operator new(count * sizeof(AtSceneParticle));
    if (particles != 0) {
        for (int i = (int)count - 1; i >= 0; i--) {
            new (&particles[count - 1 - i], AtPlacement()) AtSceneParticle;
        }
    }
    m_particles = particles;
    AtPopHeap();
    if (m_particles == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_drawer = owner->m_particleDrawers->New();
    m_drawer->Start(owner, emitter);
    m_unknown0C = m_count - 1;
    m_unknown10 = m_count - 1;
    m_unknown18 = 0;
}

// ---- the actor --------------------------------------------------------------------------------------------------------------------

void AtSceneParticleSystemActor::SetHeap(void* heap) {
    s_heap = heap;
}

AtSceneParticleSystemActor::AtSceneParticleSystemActor() {
    m_class = kClassParticleSystem;
    Reset();
}

AtSceneParticleSystemActor::~AtSceneParticleSystemActor() {
    if (m_layers != 0) {
        AtDeleteArray(m_layers);
    }
    AtSceneActor::Clear();
    Reset();
}

void AtSceneParticleSystemActor::Reset() {
    m_emitter = 0;
    m_seed = 0;
    m_state = 0;
    m_unknownA4 = 0;
    m_layerCount = 0;
    m_layers = 0;
    m_unknownB0 = 1.0f;
    m_unknownB4 = 0.0f;
    m_unknownBC = 0;
    m_unknownC0 = 0;
    m_unknownC4 = 0;
    m_unknownB8 = 0;
    m_worldInitialVelocity.x = 0.0f;
    m_worldInitialVelocity.y = 0.0f;
    m_worldInitialVelocity.z = 0.0f;
    m_worldAcceleration.x = 0.0f;
    m_worldAcceleration.y = 0.0f;
    m_worldAcceleration.z = 0.0f;
    m_halfWorldAcceleration.x = 0.0f;
    m_halfWorldAcceleration.y = 0.0f;
    m_halfWorldAcceleration.z = 0.0f;
    m_unknownEC = 0;
    m_unknownF0 = 0;
    m_unknownF8 = 0;
}

void AtSceneParticleSystemActor::ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kAtWordEmitter: {
        char name[8];
        char ns[8];
        tokenizer->ExpectName(name);
        tokenizer->ExpectName(ns);
        m_emitter = (AtEmitter*)owner->FindEmitter(name, ns);
        if (m_emitter == 0) {
            tokenizer->Error(kAtProblemNotFound);
        }
        break;
    }
    case kAtWordWorldAdditiveInitialVelocity:
        m_worldInitialVelocity.Read(tokenizer);
        break;
    case kAtWordWorldAdditiveAcceleration:
        m_worldAcceleration.Read(tokenizer);
        m_halfWorldAcceleration.x = m_worldAcceleration.x * 0.5f;
        m_halfWorldAcceleration.y = m_worldAcceleration.y * 0.5f;
        m_halfWorldAcceleration.z = m_worldAcceleration.z * 0.5f;
        break;
    case kAtWordUpdateAtDraw:
        m_flags |= kUpdateAtDraw;
        break;
    case kAtWordPlayParticles:
        m_state = kPlaying;
        break;
    case kAtWordPauseParticles:
        m_flags |= kPaused;
        m_state = kPausedState;
        break;
    default:
        AtSceneActor::ReadBody(owner, tokenizer, token);
        break;
    }
}

void AtSceneParticleSystemActor::Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) {
    if (m_flags & kLoaded) {
        Clear();
    }
    m_state = kPlaying;
    AtSceneActor::Read(owner, scene, tokenizer);
    StartLayers(owner);
}

void AtSceneParticleSystemActor::StartLayers(AtNameTableOwner* owner) {
    if (m_emitter == 0) {
        AtFatal("Atlas Scene: Particle system actor missing actor", 0, 0, 0);
    }
    m_layerCount = m_emitter->m_blurCount + 1;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_layers = AtNewArray<AtSceneParticleLayer>(m_layerCount);
    AtPopHeap();
    if (m_layers == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_layers[0].Start(owner, m_emitter, 0);

    // The first layer has no colour. Each echo has the colour at its time, a sum of three terms.
    float time = 0.0f;
    for (unsigned int i = 1; i < m_layerCount; i++) {
        const AtEmitter* emitter = m_emitter;
        time = time + emitter->m_blurStep;
        AtQuaternion color = emitter->m_blur;
        color.x = (float)((double)time * emitter->m_blurLinear.x + color.x);
        color.y = (float)((double)time * emitter->m_blurLinear.y + color.y);
        color.z = (float)((double)time * emitter->m_blurLinear.z + color.z);
        color.w = (float)((double)time * emitter->m_blurLinear.w + color.w);
        const double squared = (double)time * time;
        color.x = (float)(squared * emitter->m_blurQuadratic.x + color.x);
        color.y = (float)(squared * emitter->m_blurQuadratic.y + color.y);
        color.z = (float)(squared * emitter->m_blurQuadratic.z + color.z);
        color.w = (float)(squared * emitter->m_blurQuadratic.w + color.w);
        m_layers[i].Start(owner, m_emitter, &color);
    }

    m_seed = AtNextRandomWord();
    m_sphere.center.x = m_emitter->m_boundingSphere.center.x;
    m_sphere.center.y = m_emitter->m_boundingSphere.center.y;
    m_sphere.center.z = m_emitter->m_boundingSphere.center.z;
    m_sphere.radius = m_emitter->m_boundingSphere.radius;
    m_worldSphere.center.x = m_sphere.center.x;
    m_worldSphere.center.y = m_sphere.center.y;
    m_worldSphere.center.z = m_sphere.center.z;
    m_worldSphere.radius = m_emitter->m_boundingSphere.radius;
    OnMoved();
    m_unknownA4 = 0;
    m_unknownB0 = 1.0f;
}

void AtSceneParticleSystemActor::Clear() {
    if (m_layers != 0) {
        AtDeleteArray(m_layers);
    }
    AtSceneActor::Clear();
    Reset();
}

void AtSceneParticleSystemActor::Slot12() { NotRecovered(); }
void AtSceneParticleSystemActor::Slot34() { NotRecovered(); }
void AtSceneParticleSystemActor::Slot35() { NotRecovered(); }
void AtSceneParticleSystemActor::Slot38() { NotRecovered(); }
void AtSceneParticleSystemActor::Slot39() { NotRecovered(); }
