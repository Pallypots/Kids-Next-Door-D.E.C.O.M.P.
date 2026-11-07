// See include/at/AtAudioDatabase.h. The Xbox classes that a database, its lists and what they list are made of.
#include "at/AtAudioDatabase.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

namespace {

const int kPlatformSource = 3;
const int kNoResidence = 0;
const int kPending = 7;
const unsigned int kFrontCenterBin = 2;   // DSMIXBIN_FRONT_CENTER, inferred from the word Center
const unsigned int kStreamPlaying = 1;    // Inferred: the loop waits for it to stop being this.
const int kStream3D = 2;
const int kStreamCenter = 1;

void BankSlotNotRecovered() {
    AtFatal("Atlas Sound: this slot of AtSoXboxXactBank is not recovered", 0, 0, 0);
}

// new[], from a heap, as each of the original's arrays is.
template <class T> T* NewArrayFrom(void* heap, unsigned int count) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    T* first = AtNewArray<T>(count);
    AtPopHeap();
    if (first == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return first;
}

}  // namespace

// ---- The list of banks ------------------------------------------------------------------------------------------------------------

AtSoXboxBankDatabase::AtSoXboxBankDatabase() {
}

AtSoXboxBankDatabase::~AtSoXboxBankDatabase() {
}

void AtSoXboxBankDatabase::CreateBank(unsigned int index, const AtAudioBankDesc& desc) {
    AtSoXboxXactBank* bank = 0;
    if (desc.m_format == kPlatformSource && desc.m_residence == kNoResidence) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        bank = AtNewObject<AtSoXboxXactBank>();
        AtPopHeap();
        if (bank == 0) {
            AtRaise(-4, 0, 0, 0);
            m_banks[index] = 0;
            return;
        }
        bank->Init(desc);
    }
    m_banks[index] = bank;
}

void AtSoXboxBankDatabase::SetDirectory(const char*) {
}

// ---- The list of segments ---------------------------------------------------------------------------------------------------------

AtSoXboxSegment::AtSoXboxSegment() {
}

AtSoXboxSegment::~AtSoXboxSegment() {
}

void AtSoXboxSegment::OnRead() {
}

void AtSoXboxSegment::Reset() {
    AtAudioSegment::Reset();
}

AtSoXboxSegmentDatabase::AtSoXboxSegmentDatabase() {
    m_segments = 0;
}

AtSoXboxSegmentDatabase::~AtSoXboxSegmentDatabase() {
}

void AtSoXboxSegmentDatabase::Clear() {
    if (m_segments != 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            m_segments[i].Reset();
        }
        AtDeleteArray(m_segments);
    }
    m_segments = 0;
    AtAudioSegmentDatabase::Clear();
}

void AtSoXboxSegmentDatabase::OnReady() {
    if (m_count != 0) {
        m_segments = NewArrayFrom<AtSoXboxSegment>(AtAudioSegmentDatabase::s_heap, m_count);
    }
}

void AtSoXboxSegmentDatabase::Unknown3(void*) {
}

AtAudioSegment* AtSoXboxSegmentDatabase::SegmentAt(unsigned int index) {
    return &m_segments[index];
}

// ---- The list of emitters ---------------------------------------------------------------------------------------------------------

AtSoXboxEmitter::AtSoXboxEmitter() {
    m_stream = 0;
    m_unknown54 = 0;
}

AtSoXboxEmitter::~AtSoXboxEmitter() {
}

void AtSoXboxEmitter::Reset() {
    if (m_stream != 0) {
        AtSoXboxStopStream(m_stream);
        unsigned int status;
        do {
            AtSoXboxGetStreamStatus(m_stream, &status);
        } while (status == kStreamPlaying);
        AtSoXboxReleaseStream(m_stream);
    }
    m_stream = 0;
    m_unknown54 = 0;
    AtAudioEmitter::Reset();
}

void AtSoXboxEmitter::InitPlatform() {
    void* directSound = static_cast<AtSoXboxSystem*>(m_owner->m_loader)->m_directSound;
    if (m_kind == AtAudioEmitterDesc::kEmitter3D) {
        if (AtSoXboxCreateStream(directSound, kStream3D, &m_stream) < 0) {
            AtFatal("Atlas Sound XBOX: AtSoXboxEmitter::vInitPlatformSpecific() Unable to create sound source.", 0, 0, 0);
        }
    } else if (m_kind == AtAudioEmitterDesc::kCenter) {
        if (AtSoXboxCreateStream(directSound, kStreamCenter, &m_stream) < 0) {
            AtFatal("Atlas Sound XBOX: AtSoXboxEmitter::vInitPlatformSpecific() Unable to create sound source.", 0, 0, 0);
            return;
        }
        AtSoXboxMixBin bin;
        bin.m_bin = kFrontCenterBin;
        bin.m_volume = 0;
        AtSoXboxMixBins bins;
        bins.m_count = 1;
        bins.m_bins = &bin;
        if (AtSoXboxSetMixBins(m_stream, &bins) < 0) {
            AtFatal("Atlas Sound XBOX: AtSoXboxEmitter::vInitPlatformSpecific() Unable to set mixbins for sound source.", 0, 0, 0);
        }
    } else {
        m_stream = 0;
    }
}

AtSoXboxEmitterDatabase::AtSoXboxEmitterDatabase() {
    m_emitters = 0;
}

AtSoXboxEmitterDatabase::~AtSoXboxEmitterDatabase() {
}

void AtSoXboxEmitterDatabase::Clear() {
    if (m_emitters != 0) {
        for (unsigned int i = 0; i < m_count; i++) {
            m_emitters[i].Reset();
        }
        AtDeleteArray(m_emitters);
    }
    m_emitters = 0;
    AtAudioEmitterDatabase::Clear();
}

void AtSoXboxEmitterDatabase::OnReady() {
    if (m_count != 0) {
        m_emitters = NewArrayFrom<AtSoXboxEmitter>(AtAudioEmitterDatabase::s_heap, m_count);
    }
}

AtAudioEmitter* AtSoXboxEmitterDatabase::EmitterAt(unsigned int index) {
    return &m_emitters[index];
}

// ---- A database -------------------------------------------------------------------------------------------------------------------

AtSoXboxDatabase::AtSoXboxDatabase() {
    m_bankDatabases = 0;
    m_segmentDatabases = 0;
    m_emitterDatabases = 0;
    m_nestedDatabases = 0;
}

AtSoXboxDatabase::~AtSoXboxDatabase() {
    AtSoXboxDatabase::Clear();
}

void AtSoXboxDatabase::AllocateBankDatabases() {
    if (m_bankCount > 0) {
        m_bankDatabases = NewArrayFrom<AtSoXboxBankDatabase>(s_heap, m_bankCount);
    }
}

void AtSoXboxDatabase::AllocateSegmentDatabases() {
    m_segmentDatabases = NewArrayFrom<AtSoXboxSegmentDatabase>(s_heap, m_segmentCount);
}

void AtSoXboxDatabase::AllocateEmitterDatabases() {
    m_emitterDatabases = NewArrayFrom<AtSoXboxEmitterDatabase>(s_heap, m_emitterCount);
}

void AtSoXboxDatabase::AllocateNestedDatabases() {
    m_nestedDatabases = NewArrayFrom<AtSoXboxDatabase>(s_heap, m_nestedCount);
}

void AtSoXboxDatabase::Clear() {
    if (m_emitterDatabases != 0) {
        for (unsigned int i = 0; i < m_emitterCount; i++) {
            m_emitterDatabases[i].Clear();
        }
        AtDeleteArray(m_emitterDatabases);
    }
    if (m_segmentDatabases != 0) {
        for (unsigned int i = 0; i < m_segmentCount; i++) {
            m_segmentDatabases[i].Clear();
        }
        AtDeleteArray(m_segmentDatabases);
    }
    if (m_bankDatabases != 0) {
        for (unsigned int i = 0; i < m_bankCount; i++) {
            m_bankDatabases[i].Clear();
        }
        AtDeleteArray(m_bankDatabases);
    }
    if (m_nestedDatabases != 0) {
        for (unsigned int i = 0; i < m_nestedCount; i++) {
            m_nestedDatabases[i].Clear();
        }
        AtDeleteArray(m_nestedDatabases);
    }
    m_bankDatabases = 0;
    m_segmentDatabases = 0;
    m_emitterDatabases = 0;
    m_nestedDatabases = 0;
    AtAudioDatabase::Clear();
}

AtAudioBankDatabase* AtSoXboxDatabase::BankDatabaseAt(unsigned int index) {
    if (index >= m_bankCount) {
        return 0;
    }
    return &m_bankDatabases[index];
}

AtAudioSegmentDatabase* AtSoXboxDatabase::SegmentDatabaseAt(unsigned int index) {
    return &m_segmentDatabases[index];
}

AtAudioEmitterDatabase* AtSoXboxDatabase::EmitterDatabaseAt(unsigned int index) {
    return &m_emitterDatabases[index];
}

AtAudioDatabase* AtSoXboxDatabase::NestedDatabaseAt(unsigned int index) {
    return &m_nestedDatabases[index];
}

// ---- A bank -----------------------------------------------------------------------------------------------------------------------

AtSoBank::AtSoBank() {
}

AtSoBank::~AtSoBank() {
}

AtSoXboxXactBank::Names::Names() {
    m_name[0] = 0;
    m_unknown10C = 0;
    m_unknown110 = 0;
    m_unknown114 = 0;
}

AtSoXboxXactBank::AtSoXboxXactBank() {
    ClearFields();
}

AtSoXboxXactBank::~AtSoXboxXactBank() {
}

void AtSoXboxXactBank::ClearFields() {
    const Names empty;
    m_names = empty;
    m_unknown1B0 = 0;
    m_unknown1B4 = 0;
    m_header = 0;
    m_block1C8 = 0;
    m_block1CC = 0;
    m_status = 3;
    m_step = 0;
    m_block1D8 = 0;
    m_block1DC = 0;
    m_unknown1F4 = 0;
    m_unknown1E8 = 0;
    m_unknown1EC = 0;
    m_unknown1F0 = 0;
    m_unknown1E0 = 0;
    m_unknown1E4 = 0;
    m_unknown1BC = 0;
    m_block1C0 = 0;
    m_fileHandle = -1;
    m_unknown1F8 = 0;
}

void AtSoXboxXactBank::OnInit(const AtAudioBankDesc& desc) {
    AtAudioBankDesc unused;  // Made and not used, in the original too.
    (void)unused;
    m_system = desc.m_database->m_owner->m_loader;
    if ((desc.m_flags & AtAudioBankDesc::kDelayLoad) == 0) {
        if (Load() == kPending) {
            Process();
        }
    }
}

void AtSoXboxXactBank::Reset() {
    Update();
    operator delete(m_block1D8);
    operator delete(m_block1DC);
    operator delete(m_block1CC);
    operator delete(m_block1C8);
    operator delete(m_block1C0);
    ClearFields();
    AtAudioBank::Reset();
}

int AtSoXboxXactBank::Unknown16() {
    return m_unknown1F8;
}

void AtSoXboxXactBank::Unknown3() { BankSlotNotRecovered(); }
void AtSoXboxXactBank::Unknown4() { BankSlotNotRecovered(); }
void AtSoXboxXactBank::Unknown5() { BankSlotNotRecovered(); }
void AtSoXboxXactBank::Unknown6() { BankSlotNotRecovered(); }
void AtSoXboxXactBank::Unknown7() { BankSlotNotRecovered(); }
void AtSoXboxXactBank::Unknown8() { BankSlotNotRecovered(); }
void AtSoXboxXactBank::Unknown9() { BankSlotNotRecovered(); }
int AtSoXboxXactBank::Unload() { BankSlotNotRecovered(); return 3; }
float AtSoXboxXactBank::SegmentLength(int) { BankSlotNotRecovered(); return 0.0f; }
float AtSoXboxXactBank::Unknown12(int) { BankSlotNotRecovered(); return 0.0f; }
int AtSoXboxXactBank::Load() { return AtSoXboxXactBankLoad(this); }
void AtSoXboxXactBank::Process() { AtSoXboxXactBankProcess(this); }
void AtSoXboxXactBank::Update() { AtSoXboxXactBankUpdate(this); }
