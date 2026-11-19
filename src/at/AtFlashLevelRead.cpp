// See include/at/AtFlashLevel.h: the .AFD reader.
#include "at/AtFlashLevel.h"

#include "at/AtText.h"

void AtCopyName8(char* to, const char* from);
void AtCopyName9(char* to, const char* from);
void AtFatal(const char* message, int a, int b, int c);

namespace {

enum Word { kLoader = 0x39, kLevel = 0x3A, kScript = 0x3B, kSprite = 0x3C, kShape = 0x3D, kText = 0x3E, kSound = 0x3F };

// At 0x00270E14.
const AtIoKeyword s_keywords[] = {
    {kLoader, "Loader"},
    {kLevel, "Level"},
    {kScript, "Script"},
    {kSprite, "Sprite"},
    {kShape, "Shape"},
    {kText, "Text"},
    {kSound, "Sound"},
};

}  // namespace

// ---- the level ------------------------------------------------------------------------------------------------------

void AtFlashLevel::Reset() {
    m_fontSourceLevel = -1;
    m_unknown340 = -1;
    m_unknown008 = 0;
    m_unknown31C = 0;
    m_environment = 0;
    m_unknown324 = 0;
    m_read = 0;
    m_heapSize = 0;
    m_block = 0;
    m_heapInUse = 0;
    // The original names an empty string at 0x00270F0B, and the copy takes eight bytes: the next seven are "%s.%s" and its terminator, which sit after it in the image.
    static const char kNoName[8] = {0, '%', 's', '.', '%', 's', 0, 0};
    AtCopyName8(m_name, kNoName);
    m_heap = 0;
}

void AtFlashLevel::ReadDefinition(const char* name, int unused) {
    AtIoTokenFile file;
    const int mode = m_environment->m_platformSpecificFiles != 0 ? 2 : 0;
    char path[9];
    AtCopyName9(path, name);
    const int status = file.Open(path, "AFD", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "AFD");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kLoader:
            tokenizer->ExpectLeftCurly();
            break;
        case kLevel: {
            char level[8];
            tokenizer->ExpectName(level);
            Load(level, unused);
            m_read = 1;
            break;
        }
        case kScript: {
            AtFlashApplication* application = m_environment->m_application;
            m_scripts.Load(tokenizer->ExpectString(), mode, application);
            break;
        }
        case kShape: {
            AtFlashApplication* application = m_environment->m_application;
            m_shapes.Load(tokenizer->ExpectString(), mode, application);
            break;
        }
        case kSprite:
            m_sprites.Load(tokenizer->ExpectString(), mode);
            break;
        case kText: {
            AtMaterialOwner* owner = m_environment->m_application->m_graphics->m_owner;
            const int ownFonts = m_fontSourceLevel == -1 ? 1 : 0;
            m_texts.Load(tokenizer->ExpectString(), mode, owner, ownFonts);
            break;
        }
        case kSound:
            tokenizer->ExpectString();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    file.Close();
}
