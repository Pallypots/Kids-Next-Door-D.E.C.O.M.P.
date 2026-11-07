// See include/at/AtAnimationDatabase.h.
#include "at/AtAnimationDatabase.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtAnimationDatabase::s_heap;

// 0x002B0990.
const AtIoKeyword AtAnimationDatabase::s_keywords[AtAnimationDatabase::kKeywordCount] = {
    {0x39, "KeyframeSet"}, {0x3A, "RotationKey"}, {0x3B, "TranslationKey"}, {0x3C, "ScaleKey"}, {0x3D, "NoteKey"}, {0x4C, "CameraKey"},
    {0x3E, "FrameNumber"}, {0x3F, "RotAnimComponent"}, {0x40, "TransAnimComponent"}, {0x41, "ScaleAnimComponent"}, {0x42, "NoteAnimComponent"}, {0x4D, "CameraAnimComponent"},
    {0x43, "Animation"}, {0x44, "Rate"}, {0x45, "VelocityVector"}, {0x46, "Length"}, {0x47, "LoopPeriod"}, {0x48, "LinearVelocity"},
    {0x49, "AngularVelocity"}, {0x4A, "BoundingSphere"}, {0x4B, "DemandLoad"},
};

namespace {

enum Word {
    kKeyframeSet = 0x39, kRotationKey = 0x3A, kTranslationKey = 0x3B, kScaleKey = 0x3C, kNoteKey = 0x3D, kFrameNumber = 0x3E, kRotAnimComponent = 0x3F, kTransAnimComponent = 0x40, kScaleAnimComponent = 0x41,
    kNoteAnimComponent = 0x42, kAnimationWord = 0x43, kRate = 0x44, kVelocityVector = 0x45, kLength = 0x46, kLoopPeriod = 0x47, kLinearVelocity = 0x48, kAngularVelocity = 0x49, kBoundingSphere = 0x4A,
    kCameraKey = 0x4C, kCameraAnimComponent = 0x4D
};

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kOutOfRange = 0x23 };

const int kModeBinaryAllowed = 2;
const float kLargestFloat = 3.40282346638528859e+38f;  // 0x7F7FFFFF

void ReadPair(AtIoTokenizer* tokenizer, unsigned short* pair) {
    pair[0] = (unsigned short)tokenizer->ExpectInt();
    pair[1] = (unsigned short)tokenizer->ExpectInt();
}

void ReadThree(AtIoTokenizer* tokenizer, float* three) {
    three[0] = tokenizer->ExpectReal();
    three[1] = tokenizer->ExpectReal();
    three[2] = tokenizer->ExpectReal();
}

}  // namespace

// ---- the keys ----------------------------------------------------------------------------------------------------------

void AtAnimationKeys::ReadKeys(AtIoTokenizer* tokenizer) {
    if (m_flags & kRead) {
        Clear();
    }
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kRotationKey:
            ReadRotationKeys(tokenizer);
            break;
        case kTranslationKey:
            ReadTranslationKeys(tokenizer);
            break;
        case kScaleKey:
            ReadScaleKeys(tokenizer);
            break;
        case kNoteKey:
            ReadNoteKeys(tokenizer);
            break;
        case kCameraKey:
            ReadCameraKeys(tokenizer);
            break;
        case kFrameNumber:
            Read(tokenizer);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
}

// ---- one animation -----------------------------------------------------------------------------------------------------

AtAnimation::AtAnimation() {
    Reset();
}

AtAnimation::~AtAnimation() {
    Reset();
}

void AtAnimation::Reset() {
    m_database = 0;
    m_rotation[0] = 0;
    m_rotation[1] = 0;
    m_translation[0] = 0;
    m_translation[1] = 0;
    m_scale[0] = 0;
    m_scale[1] = 0;
    m_note[0] = 0;
    m_note[1] = 0;
    m_camera[0] = 0;
    m_camera[1] = 0;
    m_rate = 0;
    m_velocity[0] = 0;
    m_velocity[1] = 0;
    m_velocity[2] = 0;
    m_angularVelocity[0] = 0;
    m_angularVelocity[1] = 0;
    m_angularVelocity[2] = 0;
    m_length = 0;
    m_loopPeriod = 0;
    m_boundingSphere.center.x = 0;
    m_boundingSphere.center.y = 0;
    m_boundingSphere.center.z = 0;
    m_boundingSphere.radius = kLargestFloat;
    m_flags = 0;
}

void AtAnimation::Read(AtAnimationDatabase* database, AtIoTokenizer* tokenizer) {
    if (m_database != 0) {
        Reset();
    }
    m_database = database;
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kRotAnimComponent:
            ReadPair(tokenizer, m_rotation);
            break;
        case kTransAnimComponent:
            ReadPair(tokenizer, m_translation);
            break;
        case kScaleAnimComponent:
            ReadPair(tokenizer, m_scale);
            break;
        case kNoteAnimComponent:
            ReadPair(tokenizer, m_note);
            break;
        case kCameraAnimComponent:
            ReadPair(tokenizer, m_camera);
            break;
        case kRate:
            m_rate = tokenizer->ExpectReal();
            break;
        case kVelocityVector:
        case kLinearVelocity:
            ReadThree(tokenizer, m_velocity);
            break;
        case kAngularVelocity:
            ReadThree(tokenizer, m_angularVelocity);
            break;
        case kLength: {
            const int length = tokenizer->ExpectInt();
            m_length = (unsigned short)length;
            if (m_loopPeriod == 0) {
                m_loopPeriod = (unsigned short)(length + 1);
            }
            break;
        }
        case kLoopPeriod:
            m_loopPeriod = (unsigned short)tokenizer->ExpectInt();
            break;
        case kBoundingSphere:
            m_boundingSphere.center.Read(tokenizer);
            m_boundingSphere.radius = tokenizer->ExpectReal();
            m_flags |= kHasBoundingSphere;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
}

// ---- the database ------------------------------------------------------------------------------------------------------

AtAnimationDatabase::AtAnimationDatabase() {
    m_device = 0;
    m_keys = 0;
    m_rotation = 0;
    m_translation = 0;
    m_scale = 0;
    m_note = 0;
    m_camera = 0;
    m_rotationCount = 0;
    m_translationCount = 0;
    m_scaleCount = 0;
    m_noteCount = 0;
    m_cameraCount = 0;
    m_animations = 0;
    m_animationCount = 0;
}

void AtAnimationDatabase::Clear() {
    GmNameMap::Clear();
    if (m_keys != 0) {
        m_keys->Clear();
        m_device->FreeKeys(m_keys);
    }
    if (m_rotation != 0) {
        operator delete(m_rotation);
    }
    if (m_translation != 0) {
        operator delete(m_translation);
    }
    if (m_scale != 0) {
        operator delete(m_scale);
    }
    if (m_note != 0) {
        operator delete(m_note);
    }
    if (m_camera != 0) {
        operator delete(m_camera);
    }
    if (m_animations != 0) {
        delete[] m_animations;
    }
    m_device = 0;
    m_keys = 0;
    m_rotation = 0;
    m_translation = 0;
    m_scale = 0;
    m_note = 0;
    m_camera = 0;
    m_rotationCount = 0;
    m_translationCount = 0;
    m_scaleCount = 0;
    m_noteCount = 0;
    m_cameraCount = 0;
    m_animations = 0;
    m_animationCount = 0;
}

AtAnimationDatabase::~AtAnimationDatabase() {
    AtAnimationDatabase::Clear();
}

void AtAnimationDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

void AtAnimationDatabase::ReadComponents(AtIoTokenizer* tokenizer, AtAnimationComponent** list, unsigned int* count) {
    if (*list != 0) {
        tokenizer->Error(kListAlreadyGiven);
    }
    *count = (unsigned int)tokenizer->ExpectCount();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    *list = (AtAnimationComponent*)operator new(*count * sizeof(AtAnimationComponent));
    AtPopHeap();
    if (*list == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < *count; i++) {
        AtAnimationComponent& entry = (*list)[i];
        entry.m_unknown00 = tokenizer->ExpectInt();
        entry.m_unknown04 = tokenizer->ExpectInt();
        entry.m_unknown08 = (unsigned short)tokenizer->ExpectInt();
        if (entry.m_unknown08 == 0) {
            tokenizer->Error(kOutOfRange);
        }
        entry.m_unknown0A = (unsigned short)tokenizer->ExpectInt();
    }
    tokenizer->ExpectRightCurly();
}

void AtAnimationDatabase::ReadAnimations(AtIoTokenizer* tokenizer) {
    if (m_animations != 0) {
        tokenizer->Error(kListAlreadyGiven);
    }
    m_animationCount = (unsigned int)tokenizer->ExpectCount();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_animations = new AtAnimation[m_animationCount];
    AtPopHeap();
    if (m_animations == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    Init(m_animationCount);
    for (unsigned int i = 0; i < m_animationCount; i++) {
        tokenizer->ExpectToken(kAnimationWord);
        char name[8];
        tokenizer->ExpectName(name);
        Insert(name, &m_animations[i]);
        m_animations[i].Read(this, tokenizer);
    }
    tokenizer->ExpectRightCurly();
}

void AtAnimationDatabase::Read(AtAnimationDevice* device, AtIoTokenizer* tokenizer) {
    if (m_keys != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    m_device = device;
    m_keys = device->NewKeys();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd) {
        switch (token) {
        case kKeyframeSet:
            m_keys->ReadKeys(tokenizer);
            break;
        case kRotAnimComponent:
            ReadComponents(tokenizer, &m_rotation, &m_rotationCount);
            break;
        case kTransAnimComponent:
            ReadComponents(tokenizer, &m_translation, &m_translationCount);
            break;
        case kScaleAnimComponent:
            ReadComponents(tokenizer, &m_scale, &m_scaleCount);
            break;
        case kNoteAnimComponent:
            ReadComponents(tokenizer, &m_note, &m_noteCount);
            break;
        case kCameraAnimComponent:
            ReadComponents(tokenizer, &m_camera, &m_cameraCount);
            break;
        case kAnimationWord:
            ReadAnimations(tokenizer);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    m_keys->Load();
}

void AtAnimationDatabase::Load(AtAnimationDevice* device, const char* name, int mode) {
    if (mode == kModeBinaryAllowed && HasBinaryForm() != 0) {
        LoadBinaryForm(device, name);
        return;
    }
    AtIoTokenFile file;
    const int status = file.Open(name, "AKA", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AKA");
    }
    Read(device, file.m_tokenizer);
    file.Close();
}
