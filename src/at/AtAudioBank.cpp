// See include/at/AtAudioBank.h. Banks and the .AOB list.
#include "at/AtAudioBank.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);

void* AtAudioBank::s_heap;
void* AtAudioBankDatabase::s_heap;

namespace {

enum Word {
    kNameSpace = 0x39, kBank = 0x3A, kDiskResident = 0x3B, kSoundRamResident = 0x3C, kPrimaryRamResident = 0x3D, kComboResident = 0x3E, kMsaFormat = 0x3F, kMiaFormat = 0x40,
    kBlockID = 0x41, kDelayLoad = 0x42, kRwfFormat = 0x43, kWbaFormat = 0x44, kStreaming = 0x45, kSbaFormat = 0x46, kGmsFormat = 0x47, kGmdFormat = 0x48, kPlatformSource = 0x49,
    kStreamed = 0x4A, kStreamedStereo = 0x4B, kStreamedMemory = 0x4C
};

}  // namespace

// The original's table has GmdFormat before GmsFormat.
const AtIoKeyword AtAudioBankDatabase::s_keywords[kKeywordCount] = {
    {kNameSpace, "NameSpace"}, {kBank, "Bank"}, {kDiskResident, "DiskResident"}, {kSoundRamResident, "SoundRamResident"}, {kPrimaryRamResident, "PrimaryRamResident"},
    {kComboResident, "ComboResident"}, {kMsaFormat, "MsaFormat"}, {kMiaFormat, "MiaFormat"}, {kBlockID, "BlockID"}, {kDelayLoad, "DelayLoad"}, {kRwfFormat, "RwfFormat"},
    {kWbaFormat, "WbaFormat"}, {kStreaming, "Streaming"}, {kSbaFormat, "SbaFormat"}, {kGmdFormat, "GmdFormat"}, {kGmsFormat, "GmsFormat"}, {kPlatformSource, "PlatformSource"},
    {kStreamed, "Streamed"}, {kStreamedStereo, "StreamedStereo"}, {kStreamedMemory, "StreamedMemory"},
};

void AtNameTableOwner::AddToFirst(GmNameMap* map, const char* ns) {
    if (ns == 0) {
        m_first.Add(map);
        return;
    }
    m_firstSpaces.Add(ns, map);
}

void AtNameTableOwner::AddToSecond(GmNameMap* map, const char* ns) {
    if (ns == 0) {
        m_second.Add(map);
        return;
    }
    m_secondSpaces.Add(ns, map);
}

AtAudioBankFile::AtAudioBankFile() {
    m_fs = 0;
    m_file = 0;
}

AtAudioBankFile::~AtAudioBankFile() {
    if (m_file != 0) {
        m_fs->Close(m_file);
    }
    m_fs = 0;
    m_file = 0;
}

AtAudioBankDesc::AtAudioBankDesc() {
    m_database = 0;
    m_name[0] = 0;
    m_format = 0;
    m_residence = 0;
    m_blockId = 0xFFFF;
    m_flags = 0;
}

void AtAudioBank::SetHeap(void* heap) {
    s_heap = heap;
}

AtAudioBank::AtAudioBank() {
    ResetFields();
}

AtAudioBank::~AtAudioBank() {
}

void AtAudioBank::ResetFields() {
    m_name[0] = 0;
    m_format = 0;
    m_residence = 0;
    m_state = 0;
    m_unknown1C = 0;
    for (int i = 0; i < kMaxUsers; i++) {
        m_users[i] = 0;
    }
    m_unknown78 = 0;
    m_userCount = 0;
    m_unknown80 = 0;
    m_unknown84 = 0;
    m_unknown88 = 0;
}

void AtAudioBank::Reset() {
    ResetFields();
}

float AtAudioBank::SegmentLength(int) {
    return 0.0f;
}

float AtAudioBank::Unknown12(int) {
    return 0.0f;
}

int AtAudioBank::AddUser(AtAudioBankDatabase* user) {
    for (int i = 0; i < kMaxUsers; i++) {
        if (m_users[i] == 0) {
            m_users[i] = user;
            m_userCount++;
            return m_userCount;
        }
    }
    return m_userCount;
}

int AtAudioBank::RemoveUser(AtAudioBankDatabase* user) {
    for (int i = 0; i < kMaxUsers; i++) {
        if (m_users[i] == user) {
            m_users[i] = 0;
            m_userCount--;
            return m_userCount;
        }
    }
    return m_userCount;
}

int AtAudioBank::UserCount() const {
    return m_userCount;
}

void AtAudioBank::Init(const AtAudioBankDesc& desc) {
    AddUser(desc.m_database);
    AtCopyName8(m_name, desc.m_name);
    if (m_name[0] != 0) {
        desc.m_database->Insert(desc.m_name, this);
    }
    m_residence = desc.m_residence;
    m_format = desc.m_format;
    m_blockId = desc.m_blockId;
    OnInit(desc);
    m_state |= kInitialised;
}

AtAudioBankDatabase* AtAudioBank::FirstUser() const {
    for (int i = 0; i < kMaxUsers; i++) {
        if (m_users[i] != 0) {
            return m_users[i];
        }
    }
    return 0;
}

int AtAudioBank::Load() {
    if (!(m_state & kLoaded)) {
        m_state |= kLoadRequested;
        FirstUser()->m_owner->m_loader->Add(this);
    }
    return 0;
}

int AtAudioBank::Unload() {
    if (!(m_state & kLoaded) && m_unknown1C != 3) {
        return 7;
    }
    FirstUser()->m_owner->m_loader->Remove(this);
    m_state &= ~kLoadRequested;
    return 0;
}

void AtAudioBank::Update() {
    if (m_state & kLoadRequested) {
        Process();
    }
}

int AtAudioBank::Unknown16() {
    return 0;
}

void AtAudioBank::GetNames(char* name, char* space) {
    AtCopyName8(name, m_name);
    const AtAudioBankDatabase* user = FirstUser();
    if (user == 0 || user->m_group == 0) {
        space[0] = 0;
        return;
    }
    AtCopyName8(space, user->m_group->m_name);
}

void AtAudioBankDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtAudioBankDatabase::AtAudioBankDatabase() {
    m_owner = 0;
    m_count = 0;
    m_banks = 0;
}

AtAudioBankDatabase::~AtAudioBankDatabase() {
}

void AtAudioBankDatabase::Allocate(const Args& args) {
    m_owner = args.m_owner;
    m_count = args.m_count;
    Init(m_count);
    m_owner->AddToFirst(this, args.m_space);
    SetDirectory(args.m_directory);
    if (m_count > 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_banks = (AtAudioBank**)operator new(m_count * sizeof(AtAudioBank*));
        AtPopHeap();
        if (m_banks == 0) {
            AtRaise(-4, 0, 0, 0);
        }
    }
}

void AtAudioBankDatabase::Clear() {
    if (m_banks != 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            AtAudioBank* bank = m_banks[i];
            if (bank != 0) {
                if (bank->UserCount() == 1) {
                    bank->Reset();
                    delete bank;
                } else {
                    bank->RemoveUser(this);
                }
                m_banks[i] = 0;
            }
        }
        operator delete(m_banks);
    }
    m_owner = 0;
    m_count = 0;
    m_banks = 0;
    GmNameMap::Clear();
}

void AtAudioBankDatabase::Read(AtNameTableOwner* owner, AtIoTokenizer* tokenizer, const char* directory) {
    if (m_count > 0) {
        Clear();
    }
    Args args;
    args.m_owner = owner;
    args.m_count = 0;
    args.m_space = 0;
    args.m_directory = directory;
    char space[8];
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd) {
        if (token == kNameSpace) {
            tokenizer->ExpectName(space);
            args.m_space = space;
        } else if (token == kBank) {
            args.m_count = (unsigned int)tokenizer->ExpectCount();
            Allocate(args);
            for (unsigned int i = 0; i < m_count; i++) {
                AtAudioBankDesc desc;
                tokenizer->ExpectToken(kBank);
                desc.m_database = this;
                tokenizer->ExpectName(desc.m_name);
                tokenizer->ExpectLeftCurly();
                for (token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
                    switch (token) {
                    case kDiskResident: desc.m_residence = AtAudioBankDesc::kDisk; break;
                    case kSoundRamResident: desc.m_residence = AtAudioBankDesc::kSoundRam; break;
                    case kPrimaryRamResident: desc.m_residence = AtAudioBankDesc::kPrimaryRam; break;
                    case kMsaFormat: desc.m_format = AtAudioBankDesc::kMsa; break;
                    case kMiaFormat: desc.m_format = AtAudioBankDesc::kMia; break;
                    case kRwfFormat: desc.m_format = AtAudioBankDesc::kRwf; break;
                    case kWbaFormat: desc.m_format = AtAudioBankDesc::kWba; break;
                    case kPlatformSource: desc.m_format = AtAudioBankDesc::kWba; break;
                    case kSbaFormat: desc.m_format = AtAudioBankDesc::kSba; break;
                    case kGmsFormat: desc.m_format = AtAudioBankDesc::kGms; break;
                    case kGmdFormat: desc.m_format = AtAudioBankDesc::kGmd; break;
                    case kBlockID: desc.m_blockId = tokenizer->ExpectInt(); break;
                    case kDelayLoad: desc.m_flags |= AtAudioBankDesc::kDelayLoad; break;
                    case kStreaming: desc.m_flags |= AtAudioBankDesc::kStreaming; break;
                    // Taken and ignored: their jump table entries go straight to the next token.
                    case kStreamed:
                    case kStreamedStereo:
                    case kStreamedMemory:
                        break;
                    // Original behaviour preserved: ComboResident is in the word table and is refused here.
                    default:
                        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
                        break;
                    }
                }
                // A bank of this name in a list with no name space is shared. One in a name space is not found here, and another of that name is made.
                AtAudioBank* known = (AtAudioBank*)owner->m_first.Find(desc.m_name);
                if (known != 0) {
                    known->AddUser(this);
                    m_banks[i] = known;
                } else {
                    CreateBank(i, desc);
                }
            }
            tokenizer->ExpectRightCurly();
            return;
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        token = tokenizer->Next();
    }
}

void AtAudioBankDatabase::Load(AtNameTableOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AOB", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AOB");
    }
    // The file's own directory is current while reading. If the path does not fit, the original passes this buffer on as it stood.
    char directory[0x10C];
    AtIoPath& path = file.m_file.m_fs->m_path;
    path.PushFromFile(file.m_file.m_file->m_path);
    path.GetCurrentPath(directory, sizeof directory);
    Read(owner, file.m_tokenizer, directory);
    path.Pop(0);
    file.Close();
}
