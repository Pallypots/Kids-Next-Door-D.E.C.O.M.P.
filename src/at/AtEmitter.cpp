// See include/at/AtEmitter.h.
#include "at/AtEmitter.h"

#include "at/AtText.h"

extern "C" double sqrt(double value);
#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtEmitterDatabase::s_heap;

// 0x002B1F20. ParticleRotationSpeed's place in the table is the original's, out of the order of its id.
const AtIoKeyword AtEmitterDatabase::s_keywords[AtEmitterDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "Emitter"}, {0x3B, "EmissionPoint"}, {0x3C, "EmissionLine"}, {0x3D, "EmissionRectangle"}, {0x3E, "EmissionCone"}, {0x3F, "EmissionConeInnerAngle"},
    {0x40, "MaxEmissionEventRate"}, {0x41, "EmissionEventChance"}, {0x42, "MaxParticlesPerEvent"}, {0x43, "ParticleChance"}, {0x44, "ParticleInitialSpeed"}, {0x45, "DeltaInitialSpeed"},
    {0x46, "LocalAcceleration"}, {0x47, "SquareSize"}, {0x48, "LinearSquareChange"}, {0x49, "QuadraticSquareChange"}, {0x4A, "RectWidthHeight"}, {0x4B, "LinearRectChange"}, {0x4C, "QuadraticRectChange"},
    {0x4D, "DiffuseColor"}, {0x4E, "MotionBlur"}, {0x4F, "LinearColorChange"}, {0x50, "QuadraticColorChange"}, {0x51, "MotionBlurLinearColorChange"}, {0x52, "MotionBlurQuadraticColorChange"},
    {0x53, "ParticleLifetime"}, {0x54, "DeltaParticleLifetime"}, {0x55, "EmitterLifetime"}, {0x56, "MaxParticles"}, {0x57, "MatAssignment"}, {0x58, "MatAssignmentWithNameSpace"},
    {0x59, "LocalBoundingSphere"}, {0x5A, "ParticleRadius"}, {0x5B, "LineSegIntersectTesting"}, {0x5C, "Opaque"}, {0x5D, "EulerUpdate"}, {0x5E, "EmissionConeScale"}, {0x67, "ParticleRotationSpeed"},
    {0x5F, "LinearSquareSizeKeys"}, {0x60, "QuadraticSquareSizeKeys"}, {0x61, "LinearRectSizeKeys"}, {0x62, "QuadraticRectSizeKeys"}, {0x63, "LinearColorChangeKeys"}, {0x64, "QuadraticColorChangeKeys"},
    {0x65, "LinearMotionBlurKeys"}, {0x66, "QuadraticMotionBlurKeys"},
};

namespace {

enum Word {
    kNameSpace = 0x39, kEmitterWord = 0x3A, kEmissionPointWord = 0x3B, kEmissionLineWord = 0x3C, kEmissionRectangleWord = 0x3D, kEmissionCone = 0x3E, kEmissionConeInnerAngle = 0x3F, kMaxEmissionEventRate = 0x40,
    kEmissionEventChance = 0x41, kMaxParticlesPerEvent = 0x42, kParticleChance = 0x43, kParticleInitialSpeed = 0x44, kDeltaInitialSpeed = 0x45, kLocalAcceleration = 0x46, kSquareSize = 0x47, kLinearSquareChange = 0x48,
    kQuadraticSquareChange = 0x49, kRectWidthHeight = 0x4A, kLinearRectChange = 0x4B, kQuadraticRectChange = 0x4C, kDiffuseColor = 0x4D, kMotionBlurWord = 0x4E, kLinearColorChange = 0x4F, kQuadraticColorChange = 0x50,
    kMotionBlurLinearColorChange = 0x51, kMotionBlurQuadraticColorChange = 0x52, kParticleLifetime = 0x53, kDeltaParticleLifetime = 0x54, kEmitterLifetime = 0x55, kMaxParticles = 0x56, kMatAssignment = 0x57,
    kMatAssignmentWithNameSpace = 0x58, kLocalBoundingSphere = 0x59, kParticleRadius = 0x5A, kLineSegIntersectTestingWord = 0x5B, kOpaqueWord = 0x5C, kEulerUpdateWord = 0x5D, kEmissionConeScale = 0x5E,
    kLinearSquareSizeKeys = 0x5F, kQuadraticSquareSizeKeys = 0x60, kLinearRectSizeKeys = 0x61, kQuadraticRectSizeKeys = 0x62, kLinearColorChangeKeys = 0x63, kQuadraticColorChangeKeys = 0x64, kLinearMotionBlurKeys = 0x65,
    kQuadraticMotionBlurKeys = 0x66, kParticleRotationSpeed = 0x67
};

// The code passed to the tokenizer's Error; the word for it is inferred from where it is used.
enum Problem { kOutOfRange = 0x23 };

const float kLargestFloat = 3.40282346639e+38f;  // 0x7F7FFFFF
const float kShortestInterval = 0.004f;          // 0x00276D90, 0x3B83126F; also what Reset gives
const float kOverSteps = 1.0f / 2047.0f;         // 0x002767B8, 0x3A001002
const float kShortestAxis = 5.0e-6f;             // 0x00272AC8, 0x36A7C5AC

// 0x00035820 in the original, a method of the quaternion that the particle work owns. This stands in until that merges: replace it then.
void SetIdentity(AtQuaternion* quaternion) {
    quaternion->x = 0;
    quaternion->y = 0;
    quaternion->z = 0;
    quaternion->w = 1.0f;
}

// Of an event, or a particle, not happening, in 2047ths. The original has this inline for particles and as a method, 0x00050E90, for events.
int ChanceToInt(float chance) {
    double against = 1.0f - (double)chance;
    if (against < 0.0f) {
        against = 0.0f;
    } else if (against > 1.0f) {
        against = 1.0f;
    }
    return (int)(against * 2047.0f);
}

void ReadColor(AtQuaternion* color, AtIoTokenizer* tokenizer) {
    color->Read(tokenizer);
    color->x = (float)((double)color->x * 255.0f);
    color->y = (float)((double)color->y * 255.0f);
    color->z = (float)((double)color->z * 255.0f);
    color->w = (float)((double)color->w * 255.0f);
}

void ReadReals(float* out, int count, AtIoTokenizer* tokenizer) {
    for (int i = 0; i < count; i++) {
        out[i] = tokenizer->ExpectReal();
    }
}

// The curve through a start, a middle key and an end key, each with its time: the start plus linear times t plus quadratic times t squared. The quadratic stays in a register for the linear.
double QuadraticOf(double start, float end, float endTime, float middle, float middleTime) {
    const double difference = (double)middleTime * end - (double)middle * endTime;
    return (difference / ((double)endTime - middleTime) + start) / ((double)middleTime * endTime);
}

double LinearOf(double change, float endTime, double quadratic) {
    return (change - (double)endTime * endTime * quadratic) / endTime;
}

// The original takes fsin of the C library's acos, 0x00126600, whose result is in the x87's full 64 bits. That is the square root below, with one exception: the acos of -1 is 64-bit pi, a little over the real one, whose
// fsin is exactly -2^-64. A double's pi would give a different result there, of the wrong sign, so this does not go through one. The original's register keeps 64 bits where this keeps 53, which could show after rounding to
// float only in a case about one in 2^29: none in the oracle's cases.
double SineOfAcos(double cosine) {
    if (cosine == -1.0f) {
        return -5.42101086242752217e-20;  // -2^-64
    }
    return sqrt(1.0f - cosine * cosine);
}

}  // namespace

void AtVector2::Normalize() {
    const double length = sqrt((double)x * x + (double)y * y);
    if (length <= kShortestAxis) {
        x = 1.0f;
        y = 0;
        return;
    }
    const double inverse = 1.0f / length;
    x = (float)(inverse * x);
    y = (float)(inverse * y);
}

void AtVector3::SetCrossNormalized(const AtVector3& a, const AtVector3& b) {
    x = (float)((double)b.z * a.y - (double)b.y * a.z);
    y = (float)((double)b.x * a.z - (double)a.x * b.z);
    const double wholeZ = (double)a.x * b.y - (double)b.x * a.y;
    z = (float)wholeZ;
    const double length = sqrt((double)x * x + (double)y * y + wholeZ * wholeZ);
    if (length == 0.0f) {
        x = 1.0f;
        y = 0;
        z = 0;
        return;
    }
    const double inverse = 1.0f / length;
    x = (float)(inverse * x);
    y = (float)(inverse * y);
    z = (float)(inverse * z);
}

void AtNameTableOwner::AddEmitters(GmNameMap* map, const char* ns) {
    if (ns != 0) {
        m_emitterSpaces.Add(ns, map);
        return;
    }
    m_emitters.Add(map);
}

// ---- one emitter ------------------------------------------------------------------------------------------------------

void AtEmitter::Reset() {
    m_owner = 0;
    m_flags = 0;
    for (int i = 0; i < 4; i++) {
        m_emission[i].x = 0;
        m_emission[i].y = 0;
        m_emission[i].z = 0;
    }
    m_coneDirection.x = 0;
    m_coneDirection.y = 0;
    m_coneDirection.z = 1.0f;
    m_coneOuterAngle = 0;
    m_coneAngleStep = 0;
    m_coneInnerAngle = 0;
    m_eventInterval = kShortestInterval;
    m_eventChance = 0;
    m_maxParticlesPerEvent = 1;
    m_particleChance = 0;
    m_initialSpeed = 0;
    m_deltaInitialSpeed = 0;
    m_rotationSpeed = 0;
    m_acceleration.x = 0;
    m_acceleration.y = 0;
    m_acceleration.z = 0;
    m_halfAcceleration.x = 0;
    m_halfAcceleration.y = 0;
    m_halfAcceleration.z = 0;
    m_coneScaleAxisA.x = 0;
    m_coneScaleAxisA.y = 0;
    m_coneScaleAxisA.z = 0;
    m_coneScaleAxisB.x = 0;
    m_coneScaleAxisB.y = 0;
    m_coneScaleAxisB.z = 0;
    m_coneScaleA = 1.0f;
    m_coneScaleB = 1.0f;
    m_color.SetWhite();
    SetIdentity(&m_colorLinear);
    SetIdentity(&m_colorQuadratic);
    SetIdentity(&m_blur);
    SetIdentity(&m_blurLinear);
    SetIdentity(&m_blurQuadratic);
    m_blurCount = 0;
    m_blurLength = 0;
    m_blurStep = 0;
    m_size[0] = 1.0f;
    m_size[1] = 1.0f;
    m_sizeLinear[0] = 0;
    m_sizeLinear[1] = 0;
    m_sizeQuadratic[0] = 0;
    m_sizeQuadratic[1] = 0;
    m_emitterLifetime = kLargestFloat;
    m_particleLifetime = 1.0f;
    m_deltaParticleLifetime = 0;
    m_maxParticles = 0x80;
    m_streamCount = 0;
    m_coneMatrix[0] = 1.0f;
    m_coneMatrix[1] = 0;
    m_coneMatrix[2] = 0;
    m_coneMatrix[3] = 0;
    m_coneMatrix[4] = 1.0f;
    m_coneMatrix[5] = 0;
    m_coneMatrix[6] = 0;
    m_coneMatrix[7] = 0;
    m_coneMatrix[8] = 1.0f;
    m_unknown1CC = 0;
    m_lifetimeOverMaterials = 0;
    m_materialCount = 1;
    m_boundingSphere.center.x = 0;
    m_boundingSphere.center.y = 0;
    m_boundingSphere.center.z = 0;
    m_particleRadius = 0;
    m_boundingSphere.radius = kLargestFloat;
}

AtEmitter::AtEmitter() {
    Reset();
}

AtEmitter::~AtEmitter() {
    m_materials.~AtMatAssignment();
    m_queries.Release();
    Reset();
}

void AtEmitter::CurveFromSizeKeys() {
    if ((m_flags & kLinearSize) != 0) {
        const double linear = ((double)m_sizeEnd[0] - m_size[0]) * (1.0f / (double)m_sizeEndTime[0]);
        m_sizeLinear[0] = (float)linear;
        m_sizeLinear[1] = (float)linear;
    } else if ((m_flags & kQuadraticSize) != 0) {
        const double quadratic = QuadraticOf(m_size[0], m_sizeEnd[0], m_sizeEndTime[0], m_sizeMiddle[0], m_sizeMiddleTime[0]);
        const double change = (double)m_sizeEnd[0] - m_size[0];
        // By the inverse here, unlike in every other curve: not always the same bits.
        const double linear = (change - (double)m_sizeEndTime[0] * m_sizeEndTime[0] * quadratic) * (1.0f / (double)m_sizeEndTime[0]);
        m_sizeQuadratic[0] = (float)quadratic;
        m_sizeLinear[0] = (float)linear;
        m_sizeLinear[1] = (float)linear;
        m_sizeQuadratic[1] = (float)quadratic;
    }
}

void AtEmitter::CurveFromRectKeys() {
    if ((m_flags & kLinearSize) != 0) {
        m_sizeLinear[0] = (float)(((double)m_sizeEnd[0] - m_size[0]) / m_sizeEndTime[0]);
        m_sizeLinear[1] = (float)(((double)m_sizeEnd[1] - m_size[1]) / m_sizeEndTime[1]);
    } else if ((m_flags & kQuadraticSize) != 0) {
        for (int i = 0; i < 2; i++) {
            const double quadratic = QuadraticOf(m_size[i], m_sizeEnd[i], m_sizeEndTime[i], m_sizeMiddle[i], m_sizeMiddleTime[i]);
            m_sizeQuadratic[i] = (float)quadratic;
            m_sizeLinear[i] = (float)LinearOf((double)m_sizeEnd[i] - m_size[i], m_sizeEndTime[i], quadratic);
        }
    }
}

void AtEmitter::CurveFromColorKeys() {
    const float* start = &m_color.x;
    const float* end = &m_colorEnd.x;
    const float* middle = &m_colorMiddle.x;
    float* linear = &m_colorLinear.x;
    float* quadratic = &m_colorQuadratic.x;
    for (int i = 0; i < 4; i++) {
        if ((m_flags & kQuadraticColor) != 0) {
            const double curve = QuadraticOf(start[i], end[i], m_colorEndTime[i], middle[i], m_colorMiddleTime[i]);
            quadratic[i] = (float)curve;
            linear[i] = (float)LinearOf((double)end[i] - start[i], m_colorEndTime[i], curve);
        } else {
            linear[i] = (float)(((double)end[i] - start[i]) / m_colorEndTime[i]);
        }
    }
}

void AtEmitter::CurveFromBlurKeys() {
    // The original stores three of the four sums, and keeps the second, y's, in a register all the way.
    const double start[4] = {(float)((double)m_color.x + m_blur.x), (double)m_color.y + m_blur.y, (float)((double)m_color.z + m_blur.z), (float)((double)m_color.w + m_blur.w)};
    const float* end = &m_blurEnd.x;
    const float* middle = &m_blurMiddle.x;
    float* linear = &m_blurLinear.x;
    float* quadratic = &m_blurQuadratic.x;
    for (int i = 0; i < 4; i++) {
        if ((m_flags & kQuadraticBlur) != 0) {
            const double curve = QuadraticOf(start[i], end[i], m_blurEndTime[i], middle[i], m_blurMiddleTime[i]);
            quadratic[i] = (float)curve;
            linear[i] = (float)LinearOf((double)end[i] - start[i], m_blurEndTime[i], curve);
        } else {
            linear[i] = (float)(((double)end[i] - start[i]) / m_blurEndTime[i]);
        }
    }
}

void AtEmitter::SetEventChance(float chance) {
    m_eventChance = ChanceToInt(chance);
}

void AtEmitter::SetConeScaleAxes(const AtVector3& axis) {
    m_coneScaleAxisB.SetCrossNormalized(m_coneDirection, axis);
    m_coneScaleAxisA.SetCrossNormalized(m_coneScaleAxisB, m_coneDirection);
}

void AtEmitter::CountStreams() {
    const int perEvent = m_maxParticlesPerEvent;
    m_streamCount = perEvent + 1;
    if (m_deltaInitialSpeed != 0.0f) {
        m_streamCount += perEvent;
    }
    if (m_coneOuterAngle != 0.0f) {
        m_streamCount += perEvent * 2;
    }
    if (m_deltaParticleLifetime != 0.0f) {
        m_streamCount += perEvent * 2;
    }
    if ((m_flags & kEmissionLine) != 0) {
        m_streamCount += perEvent;
    } else if ((m_flags & kEmissionRectangle) != 0) {
        m_streamCount += perEvent * 3;
    }
}

void AtEmitter::MakeConeMatrix() {
    m_coneDirection.Normalize();
    if (m_coneDirection.x == 0.0f && m_coneDirection.y == 0.0f) {
        m_coneMatrix[0] = 1.0f;
        m_coneMatrix[1] = 0;
        m_coneMatrix[2] = 0;
        m_coneMatrix[3] = 0;
        m_coneMatrix[4] = 1.0f;
        m_coneMatrix[5] = 0;
        m_coneMatrix[6] = 0;
        m_coneMatrix[7] = 0;
        m_coneMatrix[8] = 1.0f;
        return;
    }
    AtVector2 flat;
    flat.x = m_coneDirection.x;
    flat.y = m_coneDirection.y;
    flat.Normalize();
    const float sineAround = (float)SineOfAcos(flat.y);
    const float cosineUp = m_coneDirection.z;
    const double sineUp = SineOfAcos(cosineUp);
    m_coneMatrix[0] = flat.y;
    m_coneMatrix[1] = -sineAround;
    m_coneMatrix[2] = 0;
    m_coneMatrix[3] = (float)((double)cosineUp * sineAround);
    m_coneMatrix[4] = (float)((double)cosineUp * flat.y);
    m_coneMatrix[5] = (float)-sineUp;
    m_coneMatrix[6] = (float)((double)sineAround * sineUp);
    m_coneMatrix[7] = (float)(sineUp * flat.y);
    m_coneMatrix[8] = cosineUp;
}

void AtEmitter::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_owner != 0) {
        m_materials.~AtMatAssignment();
        m_queries.Release();
        Reset();
    }
    m_owner = owner;
    AtMaterialOwner* materials = (AtMaterialOwner*)owner->m_device;  // As for a lens flare.
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kEmissionPointWord:
            m_emission[0].Read(tokenizer);
            m_flags = (m_flags & ~(unsigned int)(kEmissionLine | kEmissionRectangle)) | kEmissionPoint;
            break;
        case kEmissionLineWord:
            m_emission[0].Read(tokenizer);
            m_emission[1].Read(tokenizer);
            m_flags = (m_flags & ~(unsigned int)(kEmissionPoint | kEmissionRectangle)) | kEmissionLine;
            break;
        case kEmissionRectangleWord:
            m_emission[0].Read(tokenizer);
            m_emission[1].Read(tokenizer);
            m_emission[2].Read(tokenizer);
            m_emission[3].Read(tokenizer);
            m_flags = (m_flags & ~(unsigned int)(kEmissionPoint | kEmissionLine)) | kEmissionRectangle;
            break;
        case kEmissionCone:
            m_coneDirection.Read(tokenizer);
            m_coneOuterAngle = tokenizer->ExpectReal();
            m_coneAngleStep = (float)(((double)m_coneOuterAngle - m_coneInnerAngle) * kOverSteps);
            break;
        case kEmissionConeInnerAngle:
            m_coneInnerAngle = tokenizer->ExpectReal();
            m_coneAngleStep = (float)(((double)m_coneOuterAngle - m_coneInnerAngle) * kOverSteps);
            break;
        case kEmissionConeScale:
            m_coneScaleAxisA.Read(tokenizer);
            m_coneScaleA = tokenizer->ExpectReal();
            m_coneScaleB = tokenizer->ExpectReal();
            SetConeScaleAxes(m_coneScaleAxisA);
            m_flags |= kConeScale;
            break;
        case kMaxEmissionEventRate: {
            const float rate = tokenizer->ExpectReal();
            if (rate <= 0.0f) {
                tokenizer->Error(kOutOfRange);
            }
            const double interval = 1.0f / (double)rate;
            m_eventInterval = (float)interval;
            if (interval < kShortestInterval) {
                m_eventInterval = kShortestInterval;
            }
            break;
        }
        case kEmissionEventChance:
            SetEventChance(tokenizer->ExpectReal());
            break;
        case kMaxParticlesPerEvent:
            m_maxParticlesPerEvent = tokenizer->ExpectInt();
            break;
        case kParticleChance:
            m_particleChance = ChanceToInt(tokenizer->ExpectReal());
            break;
        case kParticleInitialSpeed:
            m_initialSpeed = tokenizer->ExpectReal();
            break;
        case kDeltaInitialSpeed:
            m_deltaInitialSpeed = tokenizer->ExpectReal();
            break;
        case kParticleRotationSpeed:
            m_rotationSpeed = tokenizer->ExpectReal();
            break;
        case kLocalAcceleration:
            m_acceleration.Read(tokenizer);
            m_halfAcceleration.x = (float)((double)m_acceleration.x * 0.5f);
            m_halfAcceleration.y = (float)((double)m_acceleration.y * 0.5f);
            m_halfAcceleration.z = (float)((double)m_acceleration.z * 0.5f);
            break;
        case kSquareSize: {
            const float size = tokenizer->ExpectReal();
            m_size[0] = size;
            m_size[1] = size;
            m_flags |= kSquareParticles;
            break;
        }
        case kLinearSquareChange: {
            const float linear = tokenizer->ExpectReal();
            m_sizeLinear[0] = linear;
            m_sizeLinear[1] = linear;
            m_flags = (m_flags & ~(unsigned int)kQuadraticSize) | kLinearSize;
            m_sizeQuadratic[0] = 0;
            m_sizeQuadratic[1] = 0;
            break;
        }
        case kQuadraticSquareChange: {
            const float linear = tokenizer->ExpectReal();
            m_sizeLinear[0] = linear;
            m_sizeLinear[1] = linear;
            const float quadratic = tokenizer->ExpectReal();
            m_sizeQuadratic[0] = quadratic;
            m_sizeQuadratic[1] = quadratic;
            m_flags = (m_flags & ~(unsigned int)kLinearSize) | kQuadraticSize;
            break;
        }
        case kLinearSquareSizeKeys:
        case kQuadraticSquareSizeKeys: {
            const float size = tokenizer->ExpectReal();
            m_size[0] = size;
            m_size[1] = size;
            const float end = tokenizer->ExpectReal();
            m_sizeEnd[0] = end;
            m_sizeEnd[1] = end;
            const float endTime = tokenizer->ExpectReal();
            m_sizeEndTime[0] = endTime;
            m_sizeEndTime[1] = endTime;
            if (token == kLinearSquareSizeKeys) {
                m_flags = (m_flags & ~(unsigned int)kQuadraticSize) | kSizeKeys | kSquareParticles | kLinearSize;
            } else {
                const float middle = tokenizer->ExpectReal();
                m_sizeMiddle[0] = middle;
                m_sizeMiddle[1] = middle;
                const float middleTime = tokenizer->ExpectReal();
                m_sizeMiddleTime[0] = middleTime;
                m_sizeMiddleTime[1] = middleTime;
                m_flags = (m_flags & ~(unsigned int)kLinearSize) | kSizeKeys | kSquareParticles | kQuadraticSize;
            }
            CurveFromSizeKeys();
            break;
        }
        case kRectWidthHeight:
            ReadReals(m_size, 2, tokenizer);
            m_flags &= ~(unsigned int)kSquareParticles;
            break;
        case kLinearRectChange:
            ReadReals(m_sizeLinear, 2, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kQuadraticSize) | kLinearSize;
            m_sizeQuadratic[0] = 0;
            m_sizeQuadratic[1] = 0;
            break;
        case kQuadraticRectChange:
            ReadReals(m_sizeLinear, 2, tokenizer);
            ReadReals(m_sizeQuadratic, 2, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kLinearSize) | kQuadraticSize;
            break;
        case kLinearRectSizeKeys:
            ReadReals(m_size, 2, tokenizer);
            ReadReals(m_sizeEnd, 2, tokenizer);
            ReadReals(m_sizeEndTime, 2, tokenizer);
            m_flags = (m_flags & ~(unsigned int)(kSquareParticles | kQuadraticSize)) | kSizeKeys | kLinearSize;
            CurveFromRectKeys();
            break;
        case kQuadraticRectSizeKeys:
            ReadReals(m_size, 2, tokenizer);
            ReadReals(m_sizeEnd, 2, tokenizer);
            ReadReals(m_sizeEndTime, 2, tokenizer);
            ReadReals(m_sizeMiddle, 2, tokenizer);
            ReadReals(m_sizeMiddleTime, 2, tokenizer);
            m_flags = (m_flags & ~(unsigned int)(kSquareParticles | kLinearSize)) | kSizeKeys | kQuadraticSize;
            CurveFromRectKeys();
            break;
        case kDiffuseColor:
            ReadColor(&m_color, tokenizer);
            break;
        case kLinearColorChange:
            ReadColor(&m_colorLinear, tokenizer);
            m_colorQuadratic.x = 0;
            m_colorQuadratic.y = 0;
            m_colorQuadratic.z = 0;
            m_colorQuadratic.w = 0;
            m_flags = (m_flags & ~(unsigned int)kQuadraticColor) | kLinearColor;
            break;
        case kQuadraticColorChange:
            ReadColor(&m_colorLinear, tokenizer);
            ReadColor(&m_colorQuadratic, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kLinearColor) | kQuadraticColor;
            break;
        case kLinearColorChangeKeys:
            ReadColor(&m_color, tokenizer);
            ReadColor(&m_colorEnd, tokenizer);
            ReadReals(m_colorEndTime, 4, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kQuadraticColor) | kColorKeys | kLinearColor;
            CurveFromColorKeys();
            break;
        case kQuadraticColorChangeKeys:
            ReadColor(&m_color, tokenizer);
            ReadColor(&m_colorEnd, tokenizer);
            ReadReals(m_colorEndTime, 4, tokenizer);
            ReadColor(&m_colorMiddle, tokenizer);
            ReadReals(m_colorMiddleTime, 4, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kLinearColor) | kColorKeys | kQuadraticColor;
            CurveFromColorKeys();
            break;
        case kMotionBlurWord:
            m_blurCount = tokenizer->ExpectInt();
            if ((unsigned int)m_blurCount < 1) {
                tokenizer->Error(kOutOfRange);
            }
            m_flags |= kMotionBlur;
            m_blurLength = tokenizer->ExpectReal();
            m_blurStep = (float)((double)m_blurLength / (double)m_blurCount);
            ReadColor(&m_blur, tokenizer);
            break;
        case kMotionBlurLinearColorChange:
            ReadColor(&m_blurLinear, tokenizer);
            m_blurQuadratic.x = 0;
            m_blurQuadratic.y = 0;
            m_blurQuadratic.z = 0;
            m_blurQuadratic.w = 0;
            m_flags = (m_flags & ~(unsigned int)kQuadraticBlur) | kLinearBlur;
            break;
        case kMotionBlurQuadraticColorChange:
            ReadColor(&m_blurLinear, tokenizer);
            ReadColor(&m_blurQuadratic, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kLinearBlur) | kQuadraticBlur;
            break;
        case kLinearMotionBlurKeys:
            ReadColor(&m_blur, tokenizer);
            ReadColor(&m_blurEnd, tokenizer);
            ReadReals(m_blurEndTime, 4, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kQuadraticBlur) | kBlurKeys | kLinearBlur;
            CurveFromBlurKeys();
            break;
        case kQuadraticMotionBlurKeys:
            ReadColor(&m_blur, tokenizer);
            ReadColor(&m_blurEnd, tokenizer);
            ReadReals(m_blurEndTime, 4, tokenizer);
            ReadColor(&m_blurMiddle, tokenizer);
            ReadReals(m_blurMiddleTime, 4, tokenizer);
            m_flags = (m_flags & ~(unsigned int)kLinearBlur) | kBlurKeys | kQuadraticBlur;
            CurveFromBlurKeys();
            break;
        case kParticleLifetime:
            m_particleLifetime = tokenizer->ExpectReal();
            m_lifetimeOverMaterials = (float)((double)m_particleLifetime / (double)(int)m_materialCount);
            break;
        case kDeltaParticleLifetime:
            m_deltaParticleLifetime = tokenizer->ExpectReal();
            break;
        case kEmitterLifetime:
            m_emitterLifetime = tokenizer->ExpectReal();
            break;
        case kMaxParticles:
            m_maxParticles = tokenizer->ExpectInt();
            break;
        case kMatAssignment:
        case kMatAssignmentWithNameSpace:
            if (token == kMatAssignment) {
                m_materials.Read(materials, tokenizer, 0);
            } else {
                m_materials.ReadWithNameSpaces(materials, tokenizer);
            }
            m_materialCount = m_materials.m_count;
            if (m_materialCount < 1) {
                tokenizer->Error(kOutOfRange);
            }
            m_lifetimeOverMaterials = (float)((double)m_particleLifetime / (double)(int)m_materialCount);
            break;
        case kLocalBoundingSphere:
            m_boundingSphere.center.Read(tokenizer);
            m_boundingSphere.radius = tokenizer->ExpectReal();
            m_flags |= kHasBoundingSphere;
            break;
        case kParticleRadius:
            m_particleRadius = tokenizer->ExpectReal();
            break;
        case kLineSegIntersectTestingWord:
            m_flags |= kLineSegIntersectTesting;
            break;
        case kOpaqueWord:
            m_flags |= kOpaque;
            break;
        case kEulerUpdateWord:
            m_flags |= kEulerUpdate;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
    if ((m_flags & kLineSegIntersectTesting) != 0) {
        m_queries.Init((unsigned int)m_maxParticles, 1);
    }
    CountStreams();
    MakeConeMatrix();
}

// ---- the database -----------------------------------------------------------------------------------------------------

void AtEmitterDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtEmitterDatabase::AtEmitterDatabase() {
    m_count = 0;
    m_emitters = 0;
}

void AtEmitterDatabase::Clear() {
    if (m_emitters != 0) {
        delete[] m_emitters;
    }
    GmNameMap::Clear();
    m_count = 0;
    m_emitters = 0;
}

AtEmitterDatabase::~AtEmitterDatabase() {
    if (m_emitters != 0) {
        delete[] m_emitters;
    }
    GmNameMap::Clear();
    m_count = 0;
    m_emitters = 0;
}

void AtEmitterDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_emitters != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    const char* ns = 0;
    char space[8];
    if (tokenizer->Next() == kNameSpace) {
        tokenizer->ExpectName(space);
        tokenizer->Next();
        ns = space;
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (m_count < 1) {
        tokenizer->Error(kOutOfRange);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_emitters = new AtEmitter[m_count];
    AtPopHeap();
    if (m_emitters == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(m_count);
    owner->AddEmitters(this, ns);
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kEmitterWord);
        char name[8];
        tokenizer->ExpectName(name);
        Insert(name, &m_emitters[i]);
        m_emitters[i].Read(owner, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

void AtEmitterDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "ASE", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASE");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
