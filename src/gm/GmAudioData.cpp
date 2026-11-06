// GmAudioData: the readers of .GOV and .GOB. Original address cluster 0x00136DA0-0x001373A0.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "gm/GmAudioData.h"
#include "gm/GmLevelManager.h"

extern "C" {
int toupper(int c);
}

GmAudioMemory* GmAudioMemory::s_instance;
void* GmAudioData::s_heap;

namespace {

enum PresetToken { kVolumePresets = 0x39, kPreset = 0x3A, kAmbience = 0x3B, kSfx = 0x3C, kVoice = 0x3D, kMusic = 0x3E, kUi = 0x3F, kCinema = 0x40 };

// 0x002B8F90.
const AtIoKeyword kPresetKeywords[] = {
    {kVolumePresets, "VolumePresets"}, {kPreset, "Preset"}, {kAmbience, "Ambience"}, {kSfx, "SFX"}, {kVoice, "Voice"}, {kMusic, "Music"}, {kUi, "UI"}, {kCinema, "Cinema"},
};

enum BlockToken {
    kBlockDatabase = 0x39, kBlock = 0x3A, kPrimaryRam = 0x3B, kSecondaryRam = 0x3C, kSoundRam = 0x3D, kInstanceCount = 0x3E, kBlockId = 0x3F, kSize = 0x40, kPlatformSpecific = 0x41, kSection = 0x42
};

// 0x002B8F40.
const AtIoKeyword kBlockKeywords[] = {
    {kBlockDatabase, "BlockDatabase"}, {kBlock, "Block"}, {kPrimaryRam, "PrimaryRamResident"}, {kSecondaryRam, "SecondaryRamResident"}, {kSoundRam, "SoundRamResident"},
    {kInstanceCount, "InstanceCount"}, {kBlockId, "BlockID"}, {kSize, "Size"}, {kPlatformSpecific, "UsePlatformSpecific"}, {kSection, "Section"},
};

// The C library's _stricmp, here so as to need no header.
int EqualIgnoringCase(const char* a, const char* b) {
    for (;; a++, b++) {
        const int x = toupper((unsigned char)*a);
        if (x != toupper((unsigned char)*b)) {
            return 0;
        }
        if (x == 0) {
            return 1;
        }
    }
}

}  // namespace

GmVolumePreset::GmVolumePreset() {
    m_name[0] = 0;
    m_ambience = 0;
    m_sfx = 0;
    m_voice = 0;
    m_music = 0;
    m_ui = 0;
    m_cinema = 0;
    for (int i = 0; i < 10; i++) {
        m_unknown20[i] = 0;
    }
}

void GmAudioData::LoadVolumePresets(const char* path) {
    AtIoTokenFile file;
    if (file.Open(path, "GOV", 2, 0) != 0) {
        return;
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kPresetKeywords, sizeof kPresetKeywords / sizeof kPresetKeywords[0], 0);
    t->ExpectToken(kVolumePresets);
    m_presetCount = (unsigned int)t->ExpectCount();
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_presets = new GmVolumePreset[m_presetCount];
    AtPopHeap();
    if (m_presets == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_presetCount; i++) {
        t->ExpectToken(kPreset);
        char name[8];
        t->ExpectName(name);
        GmVolumePreset* preset = &m_presets[i];
        AtCopyName8(preset->m_name, name);
        preset->m_ambience = preset->m_sfx = preset->m_voice = preset->m_music = 1.0f;
        preset->m_ui = preset->m_cinema = 1.0f;
        for (int k = 0; k < 10; k++) {
            preset->m_unknown20[k] = 1.0f;
        }
        t->ExpectLeftCurly();
        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {
            switch (token) {
            case kAmbience: preset->m_ambience = t->ExpectReal(); break;
            case kSfx: preset->m_sfx = t->ExpectReal(); break;
            case kVoice: preset->m_voice = t->ExpectReal(); break;
            case kMusic: preset->m_music = t->ExpectReal(); break;
            case kUi: preset->m_ui = t->ExpectReal(); break;
            case kCinema: preset->m_cinema = t->ExpectReal(); break;
            default: break;
            }
        }
    }
    t->ExpectRightCurly();
    file.Close();
}

void GmAudioData::LoadMemoryBlocks(const char* path) {
    AtIoTokenFile file;
    if (file.Open(path, "GOB", 2, 0) != 0) {
        return;
    }
    AtIoTokenizer* t = file.m_tokenizer;
    t->SetKeywords(kBlockKeywords, sizeof kBlockKeywords / sizeof kBlockKeywords[0], 0);
    if (t->Next() == kPlatformSpecific) {
        // Sections until one for this platform; none means nothing is read at all.
        int found = 0;
        for (int token = t->Next(); token != 0; token = t->Next()) {
            if (token == kSection && EqualIgnoringCase(t->ExpectString(8u), "XBOX")) {
                found = 1;
                break;
            }
        }
        if (!found) {
            file.Close();
            return;
        }
    } else {
        t->m_pushedBack = 1;
    }
    t->ExpectToken(kBlockDatabase);
    char database[8];
    t->ExpectName(database);
    const unsigned int count = (unsigned int)t->ExpectCount();
    for (unsigned int i = 0; i < count; i++) {
        t->ExpectToken(kBlock);
        char name[8];
        t->ExpectName(name);
        int where = 2;
        int id = 0;
        int size = 0;
        unsigned int instances = 1;
        t->ExpectLeftCurly();
        for (int token = t->Next(); token != AtIoTokenizer::kRightCurly; token = t->Next()) {
            switch (token) {
            case kPrimaryRam: where = 0; break;
            case kSecondaryRam: where = 1; break;
            case kSoundRam: where = 2; break;
            case kInstanceCount: instances = (unsigned int)t->ExpectInt(); break;
            case kBlockId: id = t->ExpectInt(); break;
            case kSize: size = t->ExpectInt(); break;
            default: break;
            }
        }
        for (; instances != 0; instances--) {
            void* memory = GmAudioMemory::s_instance->Allocate(where, size, id, 0);
            if (memory != 0) {
                m_blocks[m_blockCount].m_where = where;
                m_blocks[m_blockCount].m_id = id;
                m_blocks[m_blockCount].m_size = size;
                m_blocks[m_blockCount].m_memory = memory;
                m_blockCount++;
            }
        }
    }
    file.Close();
}
