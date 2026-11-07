// Particle emitters, as read by the loader of ".ASE" text. Original address clusters 0x0003F580-0x0003F8D7, 0x00050830-0x000520CC with its jump table at 0x000520D0, and 0x00021EC0, 0x0002E880. The words are the original's,
// from its table at 0x002B1F20; other names are inferred. Despite the extension this is no 3D Studio file: it is the engine's own text, one Emitter[n] block of named emitters. Making and drawing particles is not recovered.
//
// Arithmetic: see at/AtVector3.h. `double` for what the original keeps in an x87 register, `float` for what it stores.
#pragma once

#include "at/AtLensFlare.h"
#include "at/AtMatAssignment.h"
#include "at/AtNameTableOwner.h"
#include "at/AtSegmentQuery.h"

// Two floats. Other callers of 0x00021EC0 are not recovered, so it may be more than this.
struct AtVector2 {
    void Normalize();          // 0x00021EC0 no longer than 5e-6: becomes (1, 0)

    float x;   // +0x00
    float y;   // +0x04
};

// 0x270 bytes. The original's array of these has a cookie of 0x10, which an alignment of 16 accounts for.
class alignas(16) AtEmitter {
public:
    enum Flags {
        kEmissionPoint = 0x1, kEmissionLine = 0x2, kEmissionRectangle = 0x4, kLinearColor = 0x8, kQuadraticColor = 0x10, kMotionBlur = 0x20, kLinearBlur = 0x40, kQuadraticBlur = 0x80, kSquareParticles = 0x100, kLinearSize = 0x200,
        kQuadraticSize = 0x400, kLineSegIntersectTesting = 0x800, kHasBoundingSphere = 0x1000, kOpaque = 0x2000, kEulerUpdate = 0x4000, kConeScale = 0x8000, kSizeKeys = 0x10000, kColorKeys = 0x20000, kBlurKeys = 0x40000
    };

    AtEmitter();               // 0x00051290
    ~AtEmitter();              // 0x00051300
    void Reset();              // 0x00050F20 its fields only: not its list or pool, and not what only a word's keys fill

    // 0x00051380. Its body, a left curly through its right curly. Over an earlier read, empties it first. Colours are given as 0 to 1, and kept times 255. Materials are looked up on the owner's object at +4.
    // Text that ends inside the body never leaves the loop, unless the report stops.
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);

    void CurveFromSizeKeys();  // 0x00050A90 one value for both width and height, from the width's keys
    void CurveFromRectKeys();  // 0x00050B50
    void CurveFromColorKeys(); // 0x00050C70
    void CurveFromBlurKeys();  // 0x00050830 a motion blur's colour keys, which start from the colour plus the blur's own start
    void SetEventChance(float chance);  // 0x00050E90 of an event not happening, in 2047ths: 1 is 0 and 0 is 2047; anything outside is held to that range
    void SetConeScaleAxes(const AtVector3& axis);  // 0x00050EF0
    void CountStreams();       // 0x000510E0 the name is a guess from what is counted
    void MakeConeMatrix();     // 0x00051170 from the direction, which it normalizes. The identity when x and y are exactly 0 after that.

    AtNameTableOwner* m_owner;  // +0x00
    AtMatAssignment m_materials;  // +0x04
    unsigned int m_flags;       // +0x0C
    AtVector3 m_emission[4];    // +0x10 the point, a line's ends, or a rectangle's corners
    AtVector3 m_coneDirection;  // +0x40 z is 1 from Reset
    float m_coneOuterAngle;     // +0x4C
    float m_coneAngleStep;      // +0x50 from the inner angle to the outer, over 2047
    float m_coneInnerAngle;     // +0x54
    float m_eventInterval;      // +0x58 1 over the rate, 0.004 at least
    int m_eventChance;          // +0x5C
    int m_maxParticlesPerEvent; // +0x60 1 from Reset
    int m_particleChance;       // +0x64
    float m_initialSpeed;       // +0x68
    float m_deltaInitialSpeed;  // +0x6C
    float m_rotationSpeed;      // +0x70
    AtVector3 m_acceleration;   // +0x74
    AtVector3 m_halfAcceleration;  // +0x80
    AtLensColor m_color;        // +0x8C white from Reset
    AtQuaternion m_colorLinear;     // +0x9C
    AtQuaternion m_colorQuadratic;  // +0xAC
    AtQuaternion m_colorEnd;    // +0xBC
    int m_unknownCC;            // +0xCC not set here
    float m_colorEndTime[4];    // +0xD0
    AtQuaternion m_colorMiddle; // +0xE0
    float m_colorMiddleTime[4]; // +0xF0
    AtQuaternion m_blur;        // +0x100
    AtQuaternion m_blurLinear;  // +0x110
    AtQuaternion m_blurQuadratic;  // +0x120
    int m_blurCount;            // +0x130
    float m_blurLength;         // +0x134
    float m_blurStep;           // +0x138 the length over the count
    AtQuaternion m_blurEnd;     // +0x13C
    int m_unknown14C;           // +0x14C not set here
    float m_blurEndTime[4];     // +0x150
    AtQuaternion m_blurMiddle;  // +0x160
    float m_blurMiddleTime[4];  // +0x170
    float m_size[2];            // +0x180 1 from Reset
    float m_sizeLinear[2];      // +0x188
    float m_sizeQuadratic[2];   // +0x190
    float m_sizeEnd[2];         // +0x198
    float m_sizeEndTime[2];     // +0x1A0
    float m_sizeMiddle[2];      // +0x1A8
    float m_sizeMiddleTime[2];  // +0x1B0
    float m_emitterLifetime;    // +0x1B8 the largest float from Reset
    float m_particleLifetime;   // +0x1BC 1 from Reset
    float m_deltaParticleLifetime;  // +0x1C0
    int m_maxParticles;         // +0x1C4 0x80 from Reset
    int m_streamCount;          // +0x1C8
    int m_unknown1CC;           // +0x1CC
    AtVector3 m_coneScaleAxisA; // +0x1D0
    AtVector3 m_coneScaleAxisB; // +0x1DC
    float m_coneScaleA;         // +0x1E8 1 from Reset
    float m_coneScaleB;         // +0x1EC 1 from Reset
    float m_coneMatrix[9];      // +0x1F0
    unsigned int m_materialCount;  // +0x214 1 from Reset
    float m_lifetimeOverMaterials; // +0x218
    AtSceneSphere m_boundingSphere;  // +0x21C the radius is the largest float from Reset
    float m_particleRadius;     // +0x22C
    AtSegmentQueryPool m_queries;  // +0x230 made only for kLineSegIntersectTesting
};

// Vftable 0x0027602C. Concrete. 0x1C bytes.
class AtEmitterDatabase : public GmNameMap {
public:
    enum { kKeywordCount = 47 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B1F20

    AtEmitterDatabase();                    // 0x0003F590
    ~AtEmitterDatabase() override;          // slot 0; 0x0002E880 vector deleting, 0x0003F850 complete
    void Clear() override;                  // slot 1, 0x0003F800

    void Load(AtNameTableOwner* owner, const char* name, int unused);  // 0x0003F760 name + ".ASE"
    // 0x0003F5B0. As for lens flares, the word before the count is taken on trust: whichever it is, no report.
    void Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer);

    static void SetHeap(void* heap);        // 0x0003F580

    unsigned int m_count;    // +0x14
    AtEmitter* m_emitters;   // +0x18

    static void* s_heap;     // 0x002C6414
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtVector2) == 8, "AtVector2");
static_assert(offsetof(AtEmitter, m_flags) == 0x0C, "AtEmitter::m_flags");
static_assert(offsetof(AtEmitter, m_coneDirection) == 0x40, "AtEmitter::m_coneDirection");
static_assert(offsetof(AtEmitter, m_color) == 0x8C, "AtEmitter::m_color");
static_assert(offsetof(AtEmitter, m_blur) == 0x100, "AtEmitter::m_blur");
static_assert(offsetof(AtEmitter, m_blurLinear) == 0x110, "AtEmitter::m_blurLinear");
static_assert(offsetof(AtEmitter, m_blurQuadratic) == 0x120, "AtEmitter::m_blurQuadratic");
static_assert(offsetof(AtEmitter, m_blurCount) == 0x130, "AtEmitter::m_blurCount");
static_assert(offsetof(AtEmitter, m_blurLength) == 0x134, "AtEmitter::m_blurLength");
static_assert(offsetof(AtEmitter, m_blurStep) == 0x138, "AtEmitter::m_blurStep");
static_assert(offsetof(AtEmitter, m_maxParticles) == 0x1C4, "AtEmitter::m_maxParticles");
static_assert(offsetof(AtEmitter, m_boundingSphere) == 0x21C, "AtEmitter::m_boundingSphere");
static_assert(offsetof(AtEmitter, m_size) == 0x180, "AtEmitter::m_size");
static_assert(offsetof(AtEmitter, m_emitterLifetime) == 0x1B8, "AtEmitter::m_emitterLifetime");
static_assert(offsetof(AtEmitter, m_coneMatrix) == 0x1F0, "AtEmitter::m_coneMatrix");
static_assert(offsetof(AtEmitter, m_materialCount) == 0x214, "AtEmitter::m_materialCount");
static_assert(offsetof(AtEmitter, m_queries) == 0x230, "AtEmitter::m_queries");
static_assert(sizeof(AtEmitter) == 0x270, "AtEmitter");
static_assert(sizeof(AtEmitterDatabase) == 0x1C, "AtEmitterDatabase");
#pragma pop_macro("offsetof")
#endif
