// See include/at/AtAudioBank.h. Segments and the .AOS list.
#include "at/AtAudioBank.h"

#include "at/AtText.h"

void AtCopyName8(char* to, const char* from);

void* AtAudioSegmentDatabase::s_heap;

namespace {

enum Word { kNameSpace = 0x39, kSegment = 0x3A, kBank = 0x3B, kLooping = 0x3C };

const unsigned int kLargestFloatBits = 0x7F7FFFFF;  // 0x00274A00

}  // namespace

const AtIoKeyword AtAudioSegmentDatabase::s_keywords[kKeywordCount] = {{kNameSpace, "NameSpace"}, {kSegment, "Segment"}, {kBank, "Bank"}, {kLooping, "Looping"}};

AtAudioSegment::AtAudioSegment() {
    m_bank = 0;
    m_index = 0;
    m_looping = 0;
}

AtAudioSegment::~AtAudioSegment() {
}

void AtAudioSegment::Reset() {
    m_bank = 0;
    m_index = 0;
    m_looping = 0;
}

float AtAudioSegment::Length() {
    float length = 0.0f;
    if (m_bank != 0) {
        length = m_bank->SegmentLength(m_index);
        if (length != 0.0f) {
            return length;
        }
    }
    if (m_looping == 1) {
        return *(const float*)&kLargestFloatBits;
    }
    return length;
}

float AtAudioSegment::Unknown31BF0() {
    if (m_bank == 0) {
        return 0.0f;
    }
    return m_bank->Unknown12(m_index);
}

void AtAudioSegment::Read(AtAudioSegmentDatabase* database, AtIoTokenizer* tokenizer, const char* name, const char*) {
    char ownName[8];
    AtCopyName8(ownName, name);
    AtAudioBank* bank = 0;
    int index = 0;
    int looping = 0;
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        if (token == kBank) {
            char bankName[8];
            char space[8];
            tokenizer->ExpectName(bankName);
            tokenizer->ExpectName(space);
            bank = (AtAudioBank*)database->m_owner->FindInFirst(bankName, space);
            index = tokenizer->ExpectInt();
        } else if (token == kLooping) {
            looping = 1;
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
    }
    if (database != 0) {
        database->Insert(ownName, this);
    }
    m_bank = bank;
    m_index = index;
    m_looping = looping;
    OnRead();
}

void AtAudioSegmentDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtAudioSegmentDatabase::AtAudioSegmentDatabase() {
    m_owner = 0;
    m_count = 0;
}

AtAudioSegmentDatabase::~AtAudioSegmentDatabase() {
}

void AtAudioSegmentDatabase::Clear() {
    m_owner = 0;
    m_count = 0;
    GmNameMap::Clear();
}

void AtAudioSegmentDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    char space[8];
    space[0] = 0;
    const char* ns = 0;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd) {
        if (token == kNameSpace) {
            tokenizer->ExpectName(space);
            ns = space;
        } else if (token == kSegment) {
            const unsigned int count = (unsigned int)tokenizer->ExpectCount();
            m_owner = owner;
            m_count = count;
            Init(count);
            m_owner->AddToSecond(this, ns);
            OnReady();
            for (unsigned int i = 0; i < m_count; i++) {
                char name[8];
                tokenizer->ExpectToken(kSegment);
                tokenizer->ExpectName(name);
                SegmentAt(i)->Read(this, tokenizer, name, space);
            }
            tokenizer->ExpectRightCurly();
            return;
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        token = tokenizer->Next();
    }
}

void AtAudioSegmentDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AOS", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AOS");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}
