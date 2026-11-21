// ParticleSystemActor, vftable 0x002767C0, what it starts from an emitter when it loads, and how it lets go. Original address clusters 0x0004C180, 0x0004C370-0x0004C83F, 0x0004D1D0-0x0004D40F, 0x0003B920, 0x0005AAC0, 0x0005AC90-0x0005AD0F, 0x0005B2D0-0x0005B407.
// The class and word names come from the keyword table at 0x002B1990 and the original's messages; all other names are inferred from what the code does.
//
// Only loading and clearing are recovered. Updating and drawing, slots 12, 34, 35, 38 and 39, stop with a diagnostic.
#pragma once

#include "at/AtEmitter.h"
#include "at/AtQuaternion.h"
#include "at/AtSceneActor.h"

class AtSceneParticleDrawer;
class AtSceneWaterDrawer;  // include/at/AtSceneNodes.h

// What makes and takes back the drawers: the word at the head of the owner, set from 0x00040770's first argument. In the game a class of the running game's; nothing here makes one. A real build must supply it, as it must the owner's tables.
class AtSceneParticleDrawerMaker {
public:
    virtual AtSceneParticleDrawer* New() = 0;                     // slot 0
    virtual void Release(AtSceneParticleDrawer* drawer) = 0;      // slot 1
    // Slots 2 and 3, the same for a water actor (0x0004E4F0, 0x0004E570). Not pure here, so that a maker for particles alone still builds: these stop with a diagnostic, in src/at/AtSceneEffectNodes.cpp.
    virtual AtSceneWaterDrawer* NewWater();
    virtual void ReleaseWater(AtSceneWaterDrawer* drawer);
};

// The base has vftable 0x00277314. Only what is called in loading and clearing is declared, and only this much is established: Start is slot 1, and a drawer's second word leads back to its maker, in 0x0005ACC0.
class AtSceneParticleDrawer {
public:
    virtual void Slot0() = 0;
    virtual void Start(AtNameTableOwner* owner, AtEmitter* emitter) = 0;  // slot 1

    AtNameTableOwner* m_owner;         // +0x04 where the maker is found again. Slot 1 of the real classes must set it
};

// One particle: 0x40 bytes, all cleared. What the words are is for the update, not recovered.
struct AtSceneParticle {
    AtSceneParticle();                 // 0x0005FF30
    unsigned int m_words[0x10];        // +0x00
};

// The particles of one drawing of a system. A system has one, and one more for each echo of MotionBlur. 0x38 bytes.
class AtSceneParticleLayer {
public:
    AtSceneParticleLayer();            // 0x0005AC90
    ~AtSceneParticleLayer();           // 0x0005B2D0 only Clear
    void Clear();                      // 0x0005ACC0 does not ask whether it has anything but a drawer and particles
    void Start(AtNameTableOwner* owner, AtEmitter* emitter, const AtQuaternion* color);  // 0x0005B2E0 over any it had. No colour: all four are zero

    static void SetHeap(void* heap);   // 0x0005AAC0

    AtSceneParticleDrawer* m_drawer;   // +0x00
    AtEmitter* m_emitter; // +0x04
    AtSceneParticle* m_particles;      // +0x08
    unsigned int m_unknown0C;          // +0x0C the last index, to begin with
    unsigned int m_unknown10;          // +0x10 likewise
    unsigned int m_count;              // +0x14 MaxParticles and one more
    unsigned int m_unknown18;          // +0x18
    AtQuaternion m_color;              // +0x1C
    unsigned int m_unknown2C;          // +0x2C
    unsigned int m_unknown30;          // +0x30
    unsigned int m_unknown34;          // +0x34

    static void* s_heap;               // 0x002C9C10
};

// Vftable 0x002767C0. Size 0xFC.
class AtSceneParticleSystemActor : public AtSceneActor {
public:
    enum Flags { kUpdateAtDraw = 0x10000, kPaused = 0x20000 };
    enum State { kPlaying = 1, kPausedState = 8 };

    AtSceneParticleSystemActor();                  // 0x0004D1D0
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, int token) override;  // slot 1, 0x0004C710
    ~AtSceneParticleSystemActor() override;        // slot 7, 0x0003B920 deleting, for arrays too; 0x0004D390 complete
    void Read(AtNameTableOwner* owner, AtScene* scene, AtIoTokenizer* tokenizer) override;  // slot 10, 0x0004D1F0
    void Clear() override;                         // slot 11, 0x0004D310
    void Slot12() override;                        // slot 12, 0x0004D350 not recovered
    void Slot34() override;                        // slot 34, 0x0004D180 not recovered
    void Slot35() override;                        // slot 35, 0x0004D190 not recovered
    void Slot38() override;                        // slot 38, 0x0004D060 not recovered
    void Slot39() override;                        // slot 39, 0x0004D0A0 not recovered

    void Reset();                                  // 0x0004C670 all but +0xF4
    void StartLayers(AtNameTableOwner* owner);     // 0x0004C3B0 reports having no emitter, and then reads through the null

    static void SetHeap(void* heap);               // 0x0004C180

    AtEmitter* m_emitter; // +0x98 Emitter
    unsigned int m_seed;               // +0x9C a word from the random table
    unsigned int m_state;              // +0xA0 PlayParticles, PauseParticles
    unsigned int m_unknownA4;          // +0xA4
    unsigned int m_layerCount;         // +0xA8
    AtSceneParticleLayer* m_layers;    // +0xAC
    float m_unknownB0;                 // +0xB0 1 once reset
    float m_unknownB4;                 // +0xB4
    unsigned int m_unknownB8;          // +0xB8
    unsigned int m_unknownBC;          // +0xBC
    unsigned int m_unknownC0;          // +0xC0
    unsigned int m_unknownC4;          // +0xC4
    AtVector3 m_worldInitialVelocity;  // +0xC8 WorldAdditiveInitialVelocity
    AtVector3 m_worldAcceleration;     // +0xD4 WorldAdditiveAcceleration
    AtVector3 m_halfWorldAcceleration; // +0xE0 half of it
    unsigned int m_unknownEC;          // +0xEC
    unsigned int m_unknownF0;          // +0xF0
    unsigned int m_unknownF4;          // +0xF4 not set by the constructor, nor by clearing
    unsigned int m_unknownF8;          // +0xF8

    static void* s_heap;               // 0x002C7E10
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtSceneParticleDrawer, m_owner) == 0x04, "AtSceneParticleDrawer::m_owner");
static_assert(sizeof(AtSceneParticle) == 0x40, "AtSceneParticle");
static_assert(offsetof(AtSceneParticleLayer, m_color) == 0x1C, "AtSceneParticleLayer::m_color");
static_assert(sizeof(AtSceneParticleLayer) == 0x38, "AtSceneParticleLayer");
static_assert(offsetof(AtSceneParticleSystemActor, m_emitter) == 0x98, "AtSceneParticleSystemActor::m_emitter");
static_assert(offsetof(AtSceneParticleSystemActor, m_layers) == 0xAC, "AtSceneParticleSystemActor::m_layers");
static_assert(offsetof(AtSceneParticleSystemActor, m_worldInitialVelocity) == 0xC8, "AtSceneParticleSystemActor::m_worldInitialVelocity");
static_assert(offsetof(AtSceneParticleSystemActor, m_halfWorldAcceleration) == 0xE0, "AtSceneParticleSystemActor::m_halfWorldAcceleration");
static_assert(sizeof(AtSceneParticleSystemActor) == 0xFC, "AtSceneParticleSystemActor");
#pragma pop_macro("offsetof")
#endif
