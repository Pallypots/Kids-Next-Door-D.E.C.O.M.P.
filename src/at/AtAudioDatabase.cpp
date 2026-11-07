// See include/at/AtAudioDatabase.h. The .AOD and .AOE files, and an emitter.
#include "at/AtAudioDatabase.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);
void AtCopyName8(char* to, const char* from);

void* AtAudioDatabase::s_heap;
char** AtAudioDatabase::s_bankNames;
char** AtAudioDatabase::s_segmentNames;
char** AtAudioDatabase::s_emitterNames;
char** AtAudioDatabase::s_nestedNames;
void* AtAudioEmitterDatabase::s_heap;

namespace {

enum DatabaseWord { kBankDatabase = 0x39, kSegmentDatabase = 0x3A, kEmitterDatabase = 0x3B, kDatabaseLoader = 0x3C };
enum EmitterWord { kNameSpace = 0x39, kEmitter = 0x3A, kCenter = 0x3B, kStereo = 0x3C, kEmitter3D = 0x3D, kPosition = 0x3E, kOrientation = 0x3F };
enum Problem { kRepeated = 0x27 };

const unsigned int kPending = 7;

void FreeNames(char**& names, unsigned int count) {
    if (names != 0) {
        for (unsigned int i = 0; i < count; i++) {
            if (names[i] != 0) {
                operator delete(names[i]);
            }
        }
        operator delete(names);
        names = 0;
    }
}

void EmitterSlotNotRecovered() {
    AtFatal("Atlas Sound: this slot of an emitter is not recovered", 0, 0, 0);
}

}  // namespace

const AtIoKeyword AtAudioDatabase::s_keywords[kKeywordCount] = {{kBankDatabase, "BankDatabase"}, {kSegmentDatabase, "SegmentDatabase"}, {kEmitterDatabase, "EmitterDatabase"}, {kDatabaseLoader, "DatabaseLoader"}};

const AtIoKeyword AtAudioEmitterDatabase::s_keywords[kKeywordCount] = {
    {kNameSpace, "NameSpace"}, {kEmitter, "Emitter"}, {kCenter, "Center"}, {kStereo, "Stereo"}, {kEmitter3D, "Emitter3D"}, {kPosition, "Position"}, {kOrientation, "Orientation"},
};

// ---- An .AOD file ---------------------------------------------------------------------------------------------------------------

void AtAudioDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtAudioDatabase::AtAudioDatabase() {
    m_owner = 0;
    m_bankCount = 0;
    m_segmentCount = 0;
    m_emitterCount = 0;
    m_nestedCount = 0;
}

AtAudioDatabase::~AtAudioDatabase() {
    m_owner = 0;
    m_bankCount = 0;
    m_segmentCount = 0;
    m_emitterCount = 0;
    m_nestedCount = 0;
}

void AtAudioDatabase::Clear() {
    m_owner = 0;
    m_bankCount = 0;
    m_segmentCount = 0;
    m_emitterCount = 0;
    m_nestedCount = 0;
}

// Inline four times in the original, one to a word. The strings come from the token file's heap, as the table does.
void AtAudioDatabase::ReadNames(AtIoTokenizer* tokenizer, unsigned int count, char*** names) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    *names = (char**)operator new(count * sizeof(char*));
    AtPopHeap();
    if (*names == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        (*names)[i] = 0;
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->ExpectStringCopy(&(*names)[i]);
    }
    AtPopHeap();
}

void AtAudioDatabase::Load(AtNameTableOwner* owner, const char* name, int third) {
    AtIoTokenFile file;
    if (m_owner != 0) {
        Clear();
    }
    const int status = file.Open(name, "AOD", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AOD");
    }
    AtIoPath* path = &file.m_file.m_fs->m_path;
    m_owner = owner;
    path->PushFromFile(file.m_file.m_file->m_path);
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    // Repeating a word is reported, and if that returns the new list replaces the old one, which is lost.
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kEnd; token = tokenizer->Next()) {
        switch (token) {
        case kBankDatabase:
            if (m_bankCount > 0) {
                tokenizer->Error(kRepeated);
            }
            m_bankCount = (unsigned int)tokenizer->ExpectCount();
            AllocateBankDatabases();
            ReadNames(tokenizer, m_bankCount, &s_bankNames);
            break;
        case kSegmentDatabase:
            if (m_segmentCount > 0) {
                tokenizer->Error(kRepeated);
            }
            m_segmentCount = (unsigned int)tokenizer->ExpectCount();
            AllocateSegmentDatabases();
            ReadNames(tokenizer, m_segmentCount, &s_segmentNames);
            break;
        case kEmitterDatabase:
            if (m_emitterCount > 0) {
                tokenizer->Error(kRepeated);
            }
            m_emitterCount = (unsigned int)tokenizer->ExpectCount();
            AllocateEmitterDatabases();
            ReadNames(tokenizer, m_emitterCount, &s_emitterNames);
            break;
        case kDatabaseLoader:
            if (m_nestedCount > 0) {
                tokenizer->Error(kRepeated);
            }
            m_nestedCount = (unsigned int)tokenizer->ExpectCount();
            AllocateNestedDatabases();
            ReadNames(tokenizer, m_nestedCount, &s_nestedNames);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        tokenizer->ExpectRightCurly();
    }
    file.Close();
    LoadLists(owner, third);
    path->Pop(0);
    FreeNames(s_nestedNames, m_nestedCount);
    FreeNames(s_emitterNames, m_emitterCount);
    FreeNames(s_segmentNames, m_segmentCount);
    FreeNames(s_bankNames, m_bankCount);
}

void AtAudioDatabase::LoadLists(AtNameTableOwner* owner, int third) {
    for (unsigned int i = 0; i < m_bankCount; i++) {
        BankDatabaseAt(i)->Load(owner, s_bankNames[i], third);
    }
    for (unsigned int i = 0; i < m_segmentCount; i++) {
        SegmentDatabaseAt(i)->Load(owner, s_segmentNames[i], third);
    }
    for (unsigned int i = 0; i < m_emitterCount; i++) {
        EmitterDatabaseAt(i)->Load(owner, s_emitterNames[i], third);
    }
    for (unsigned int i = 0; i < m_nestedCount; i++) {
        NestedDatabaseAt(i)->Load(owner, s_nestedNames[i], third);
    }
}

// ---- An .AOE file ---------------------------------------------------------------------------------------------------------------

void AtAudioEmitterDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtAudioEmitterDatabase::AtAudioEmitterDatabase() {
    m_owner = 0;
    m_count = 0;
}

AtAudioEmitterDatabase::~AtAudioEmitterDatabase() {
}

void AtAudioEmitterDatabase::Clear() {
    m_owner = 0;
    m_count = 0;
    GmNameMap::Clear();
}

void AtAudioEmitterDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer) {
    char space[8];
    space[0] = 0;
    const char* group = 0;
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kEnd; token = tokenizer->Next()) {
        if (token == kNameSpace) {
            tokenizer->ExpectName(space);
            group = space;
        } else if (token == kEmitter) {
            const unsigned int count = (unsigned int)tokenizer->ExpectCount();
            m_owner = owner;
            m_count = count;
            Init(count);
            m_owner->AddToThird(this, group);  // Through the member: if a count of zero was read before, Init has just cleared it, and this crashes in the original too.
            OnReady();
            for (unsigned int i = 0; i < m_count; i++) {
                char name[8];
                tokenizer->ExpectToken(kEmitter);
                tokenizer->ExpectName(name);
                EmitterAt(i)->Read(this, tokenizer, name, space);
            }
            tokenizer->ExpectRightCurly();
            return;
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
    }
}

void AtAudioEmitterDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AOE", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AOE");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

// ---- An emitter -----------------------------------------------------------------------------------------------------------------

AtAudioEmitter::AtAudioEmitter() {
    m_owner = 0;
    m_kind = 0;
    m_unknown30 = 0;
    m_firstVoice = 0;
    m_lastVoice = 0;
    m_unknown34 = 0;
    m_unknown38 = 0;
    m_unknown3C = 0;
}

AtAudioEmitter::~AtAudioEmitter() {
}

void AtAudioEmitter::Reset() {
    if (m_owner != 0) {
        if (m_world != 0) {
            m_owner->m_unknown08->FreeMatrix(m_world);
        }
        if (m_local != 0) {
            m_owner->m_unknown08->FreeMatrix(m_local);
        }
    }
    m_owner = 0;
    m_kind = 0;
    m_unknown30 = 0;
    m_firstVoice = 0;
    m_lastVoice = 0;
    m_unknown34 = 0;
    m_unknown38 = 0;
    m_unknown3C = 0;
    AtCoordSys::Reset();
}

void AtAudioEmitter::Init(const AtAudioEmitterDesc& desc) {
    m_owner = 0;
    m_kind = 0;
    m_unknown30 = 0;
    m_firstVoice = 0;
    m_lastVoice = 0;
    m_unknown34 = 0;
    m_unknown38 = 0;
    m_unknown3C = 0;
    desc.m_database->Insert(desc.m_name, this);
    m_owner = desc.m_database->m_owner;
    AtMatrix* world = m_owner->m_unknown08->NewMatrix();
    AtMatrix* local = m_owner->m_unknown08->NewMatrix();
    SetMatrices(local, world);
    SetPosition(&desc.m_position);
    SetRotation(&desc.m_rotation);
    m_kind = desc.m_kind;
    InitPlatform();
}

void AtAudioEmitter::Read(AtAudioEmitterDatabase* database, AtIoTokenizer* tokenizer, const char* name, const char* space) {
    AtAudioEmitterDesc desc;
    desc.m_kind = AtAudioEmitterDesc::kPlain;
    desc.m_unknown18 = 0;
    desc.m_position.x = 0.0f;
    desc.m_position.y = 0.0f;
    desc.m_position.z = 0.0f;
    desc.m_rotation.x = 0.0f;
    desc.m_rotation.y = 0.0f;
    desc.m_rotation.z = 0.0f;
    desc.m_rotation.w = 1.0f;
    desc.m_database = database;
    AtCopyName8(desc.m_name, name);
    AtCopyName8(desc.m_space, space);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kCenter:
            desc.m_kind = AtAudioEmitterDesc::kCenter;
            break;
        case kStereo:
            desc.m_kind = AtAudioEmitterDesc::kStereo;
            break;
        case kEmitter3D:
            desc.m_kind = AtAudioEmitterDesc::kEmitter3D;
            break;
        case kPosition:
            desc.m_position.Read(tokenizer);
            break;
        case kOrientation:
            desc.m_rotation.Read(tokenizer);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
    Init(desc);
}

void AtAudioEmitter::Unknown8() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown12() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown13() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown14() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown15() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown16() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown17() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown18() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown19() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown20() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown21() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown22() { EmitterSlotNotRecovered(); }
void AtAudioEmitter::Unknown23() { EmitterSlotNotRecovered(); }
