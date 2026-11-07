// See include/at/AtMatAnimation.h.
#include "at/AtMatAnimation.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);
extern "C" double __cdecl floor(double value);

void* AtGrFrameNumberSet::s_heap;
void* AtMatAnimationDatabase::s_heap;

// 0x002B06D8.
const AtIoKeyword AtMatAnimationDatabase::s_keywords[AtMatAnimationDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "FrameNumber"}, {0x3B, "MaterialKey"}, {0x3C, "MatAnimation"},
    {0x3D, "Keyframes"}, {0x3E, "Rate"}, {0x3F, "Length"}, {0x40, "LoopPeriod"},
};

namespace {

enum Word { kNameSpace = 0x39, kFrameNumber = 0x3A, kMaterialKey = 0x3B, kMatAnimationWord = 0x3C, kKeyframes = 0x3D, kRate = 0x3E, kLength = 0x3F, kLoopPeriod = 0x40, kExternalSet = 0x4B };

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kOutOfRange = 0x23 };

const float kDefaultRate = 30.0f;

// A call the compiler cannot bind to the pure function at compile time: in the base's destructor this is the pure call, as in the original.
void FreeThroughTable(AtGrKeySet* set) {
    set->Free();
}

}  // namespace

// ---- the set's base ---------------------------------------------------------------------------------------------------

AtGrKeySet::AtGrKeySet() {
    m_flags = 0;
    m_name[0] = 0;
    m_count = 0;
}

// The original makes a call through its own vftable here if a count remains, which is the pure call. Its one derived class has left none.
AtGrKeySet::~AtGrKeySet() {
    if (m_count != 0) {
        FreeThroughTable(this);
    }
    m_flags = 0;
    m_name[0] = 0;
    m_count = 0;
}

void AtGrKeySet::Clear() {
    if (m_count != 0) {
        Free();
    }
    m_flags = 0;
    m_name[0] = 0;
    m_count = 0;
}

void AtGrKeySet::SetExternalName(const char* name) {
    AtCopyName(name, m_name);
    m_flags |= kNamed;
}

void AtGrKeySet::Read(AtIoTokenizer* tokenizer) {
    if ((m_flags & kRead) != 0) {
        Clear();
    }
    if (m_count > 0) {
        tokenizer->Error(kListAlreadyGiven);
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (tokenizer->Next() == kExternalSet) {
        m_flags |= kNamed;
        tokenizer->ExpectName(m_name);
    } else {
        tokenizer->m_pushedBack = 1;
        Allocate(m_count);
        ReadEntries(tokenizer);
        m_flags |= kHasData;
    }
    tokenizer->ExpectRightCurly();
    m_flags |= kRead;
}

void AtGrKeySet::Reserve(unsigned int count) {
    if ((m_flags & kRead) != 0) {
        Clear();
    }
    m_count = count;
    if (count != 0) {
        Allocate(count);
    }
    m_flags |= kRead | kHasData;
}

// ---- frame numbers ----------------------------------------------------------------------------------------------------

void AtGrFrameNumberSet::SetHeap(void* heap) {
    s_heap = heap;
}

AtGrFrameNumberSet::AtGrFrameNumberSet() {
    m_frames = 0;
}

AtGrFrameNumberSet::~AtGrFrameNumberSet() {
    AtGrKeySet::Clear();
    m_frames = 0;
}

void AtGrFrameNumberSet::Allocate(unsigned int count) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_frames = (unsigned short*)operator new(count * sizeof(unsigned short));
    AtPopHeap();
    if (m_frames == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtGrFrameNumberSet::Free() {
    operator delete(m_frames);
}

void AtGrFrameNumberSet::ReadEntries(AtIoTokenizer* tokenizer) {
    for (unsigned int i = 0; i < m_count; i++) {
        m_frames[i] = (unsigned short)tokenizer->ExpectInt();
    }
}

void AtGrFrameNumberSet::Clear() {
    AtGrKeySet::Clear();
    m_frames = 0;
}

unsigned short AtGrFrameNumberSet::At(unsigned int index) {
    return m_frames[index];
}

void AtGrFrameNumberSet::Load() {
    if ((m_flags & kNamed) != 0) {
        AtFatal("AtGrFrameNumberSet::vLoad - frame numbers cannot be demand loaded.\n", 0, 0, 0);
    }
}

// `double` stands for what the original holds in an x87 register; the order of each sum is the original's.
void AtGrFrameNumberSet::Interpolate(unsigned int first, unsigned int count, float time, int length, int* from, int* to, float* t) {
    const int frame = (int)floor(time);
    const unsigned short* run = m_frames + first;
    const unsigned short* last = run + count - 1;
    const int firstFrame = *run;
    if (frame < firstFrame) {
        // Before the first: back across the loop, from the last.
        const int sinceLast = length - *last;
        const double along = (double)sinceLast + time;
        const int span = sinceLast + firstFrame;
        *from = (int)count - 1;
        *to = 0;
        *t = (float)(along / (double)span);
        return;
    }
    const int lastFrame = *last;
    double start;
    int span;
    if (frame >= lastFrame) {
        start = (double)lastFrame;
        span = firstFrame - lastFrame + length;
        *from = (int)count - 1;
        *to = 0;
    } else {
        const unsigned short* low = run;
        const unsigned short* high = last;
        while (low + 1 != high) {
            const unsigned short* middle = low + (high - low) / 2;
            if ((int)*middle > frame) {
                high = middle;
            } else {
                low = middle;
            }
        }
        start = (double)*low;
        span = (int)*high - (int)*low;
        *from = (int)(low - run);
        *to = (int)(high - run);
    }
    *t = (float)(((double)time - start) / (double)span);
}

int AtGrFrameNumberSet::Find(unsigned int first, unsigned int count, float time, int) {
    const int frame = (int)floor(time);
    const unsigned short* run = m_frames + first;
    const unsigned short* last = run + count - 1;
    if (frame < (int)*run || frame >= (int)*last) {
        return (int)count - 1;
    }
    const unsigned short* low = run;
    const unsigned short* high = last;
    while (low + 1 != high) {
        const unsigned short* middle = low + (high - low) / 2;
        if ((int)*middle > frame) {
            high = middle;
        } else {
            low = middle;
        }
    }
    return (int)(low - run);
}

// ---- one run ----------------------------------------------------------------------------------------------------------

AtMatAnimation::AtMatAnimation() {
    m_database = 0;
    m_rate = kDefaultRate;
    m_length = 0;
    m_loopPeriod = 0;
    m_firstFrame = 0;
    m_firstKey = 0;
    m_keyCount = 0;
}

AtMatAnimation::~AtMatAnimation() {
    m_database = 0;
    m_rate = kDefaultRate;
    m_length = 0;
    m_loopPeriod = 0;
    m_firstFrame = 0;
    m_firstKey = 0;
    m_keyCount = 0;
}

void AtMatAnimation::Apply(AtMatAssignment* list, unsigned int slot, float time) {
    const int key = m_database->m_frameNumbers.Find(m_firstFrame, m_keyCount, time, (int)m_loopPeriod);
    list->m_items[slot] = m_database->m_keys[m_firstKey + key];
}

void AtMatAnimation::Read(AtMaterialOwner*, AtMatAnimationDatabase* database, AtIoTokenizer* tokenizer) {
    if (m_database != 0) {
        m_database = 0;
        m_rate = kDefaultRate;
        m_length = 0;
        m_loopPeriod = 0;
        m_firstFrame = 0;
        m_firstKey = 0;
        m_keyCount = 0;
    }
    m_database = database;
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kKeyframes:
            m_firstFrame = (unsigned int)tokenizer->ExpectInt();
            if (m_firstFrame >= database->m_frameNumbers.m_count) {
                tokenizer->Error(kOutOfRange);
            }
            m_firstKey = (unsigned int)tokenizer->ExpectInt();
            if (m_firstKey >= database->m_keyCount) {
                tokenizer->Error(kOutOfRange);
            }
            m_keyCount = (unsigned int)tokenizer->ExpectInt();
            if (m_firstKey + m_keyCount > database->m_keyCount || m_firstFrame + m_keyCount > database->m_frameNumbers.m_count) {
                tokenizer->Error(kOutOfRange);
            }
            break;
        case kRate:
            m_rate = tokenizer->ExpectReal();
            break;
        case kLength:
            m_length = (unsigned int)tokenizer->ExpectInt();
            if (m_loopPeriod == 0) {
                m_loopPeriod = m_length + 1;
            }
            break;
        case kLoopPeriod:
            m_loopPeriod = (unsigned int)tokenizer->ExpectInt();
            if (m_loopPeriod < m_length) {
                tokenizer->Error(kOutOfRange);
            }
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

// ---- the database -----------------------------------------------------------------------------------------------------

void AtMatAnimationDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtMatAnimationDatabase::AtMatAnimationDatabase() {
    m_keyCount = 0;
    m_keys = 0;
    m_animationCount = 0;
    m_animations = 0;
}

void AtMatAnimationDatabase::Clear() {
    m_frameNumbers.Clear();
    if (m_keys != 0) {
        operator delete(m_keys);
    }
    if (m_animations != 0) {
        delete[] m_animations;
    }
    GmNameMap::Clear();
    m_keyCount = 0;
    m_keys = 0;
    m_animationCount = 0;
    m_animations = 0;
}

AtMatAnimationDatabase::~AtMatAnimationDatabase() {
    AtMatAnimationDatabase::Clear();
}

void AtMatAnimationDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_keys != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        token = tokenizer->Next();
        ns = space;
    }
    do {
        switch (token) {
        case kFrameNumber:
            m_frameNumbers.Read(tokenizer);
            break;
        case kMaterialKey:
            if (m_keyCount != 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_keyCount = (unsigned int)tokenizer->ExpectCount();
            if (m_keyCount < 1) {
                tokenizer->Error(kOutOfRange);
            }
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_keys = (void**)operator new(m_keyCount * sizeof(void*));
            AtPopHeap();
            if (m_keys == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int i = 0; i < m_keyCount; i++) {
                char name[8];
                char keySpace[8];
                tokenizer->ExpectName(name);
                tokenizer->ExpectName(keySpace);
                m_keys[i] = owner->FindMaterial(name, keySpace);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kMatAnimationWord:
            if (m_animationCount != 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_animationCount = (unsigned int)tokenizer->ExpectCount();
            if (m_animationCount < 1) {
                tokenizer->Error(kOutOfRange);
            }
            Init(m_animationCount);
            owner->AddMatAnimations(this, ns);
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_animations = new AtMatAnimation[m_animationCount];
            AtPopHeap();
            if (m_animations == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int i = 0; i < m_animationCount; i++) {
                tokenizer->ExpectToken(kMatAnimationWord);
                char name[8];
                tokenizer->ExpectName(name);
                Insert(name, &m_animations[i]);
                m_animations[i].Read(owner, this, tokenizer);
            }
            tokenizer->ExpectRightCurly();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    } while (token != AtIoTokenizer::kEnd);
}

void AtMatAnimationDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGA", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGA");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
