// See include/at/AtTextureItem.h.
#include "at/AtTextureItem.h"

#include "at/AtIoTokenizer.h"
#include "at/AtVector3.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);

namespace {

// The words of an item's body. Their spellings are in the database's table, 0x002B0230.
enum Word {
    kBmpSource = 0x40, kTgaSource = 0x41, kMipMap = 0x42, kAutoGenMipLevelsWord = 0x43, kRenderTargetWord = 0x44, kDepthStencilWord = 0x45,
    kFrequentModifyWord = 0x46, kInfrequentModifyWord = 0x47, kFlipVerticallyWord = 0x48, kTextureStage = 0x49, kDdsSource = 0x4A, kLightDirection = 0x4B,
    kTransparentColorWord = 0x4C, kDiffuseLightMapWord = 0x4D, kTm2Source = 0x4E, kTplSource = 0x4F, kPntSource = 0x50, kMpegMacroBlocksWord = 0x51,
    kPlatformSourceWord = 0x52, kFrontBufferSourceWord = 0x53, kBackBufferSourceWord = 0x54, kDemandLoadWord = 0x55, kProgSource = 0x56
};

unsigned int WithSource(unsigned int usage, unsigned int source) {
    return (usage & ~(unsigned int)AtTextureBase::kSourceMask) | source;
}

// x87: the sum and the product stay in a register, and the conversion truncates.
unsigned char AxisToByte(float axis) {
    return (unsigned char)(int)(((double)axis + 1.0) * 127.5);
}

}  // namespace

AtTextureBase::AtTextureBase() {
    AtTextureBase::Reset();
}

AtTextureBase::~AtTextureBase() {
    AtTextureBase::Reset();
}

void AtTextureBase::Reset() {
    m_name[0] = 0;
    m_usage = 0;
    m_flags = 0;
    m_levelsAsked = 0;
    m_levels = 0;
    m_textureStage = 0;
    m_unknown1F = 0;
    m_transparent[0] = 0;
    m_transparent[1] = 0;
    m_transparent[2] = 0;
    m_transparent[3] = 0xFF;
    m_lightDirection[0] = 0x7F;
    m_lightDirection[1] = 0x7F;
    m_lightDirection[2] = 0;
    m_lightDirection[3] = 0x7F;
    m_database = 0;
    m_progSource = 0;
}

void AtTextureBase::Describe(AtTextureDatabase* database, const char* name, unsigned int usage, int autoGenMipLevels, unsigned int levels, int textureStage) {
    m_database = database;
    AtCopyName8(m_name, name);
    m_usage = usage;
    if (autoGenMipLevels != 0) {
        m_flags |= kAutoGenMipLevels;
    }
    if (levels < 1) {
        levels = 1;
    }
    m_levels = (unsigned char)levels;
    m_levelsAsked = (unsigned char)levels;
    m_textureStage = (unsigned char)textureStage;
}

void AtTextureBase::Read(AtTextureDatabase* database, const char* name, AtIoTokenizer* tokenizer) {
    m_database = database;
    AtCopyName8(m_name, name);
    tokenizer->ExpectLeftCurly();
    m_usage = 0;
    m_flags = 0;
    m_levels = 1;
    m_levelsAsked = 1;
    m_textureStage = 0;
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kBmpSource:
            m_usage = WithSource(m_usage, kSourceBmp);
            break;
        case kTgaSource:
            m_usage = WithSource(m_usage, kSourceTga);
            break;
        case kDdsSource:
            m_usage = WithSource(m_usage, kSourceDds);
            break;
        case kTm2Source:
            m_usage = WithSource(m_usage, kSourceTm2);
            break;
        case kTplSource:
            m_usage = WithSource(m_usage, kSourceTpl);
            break;
        case kPntSource:
            m_usage = WithSource(m_usage, kSourcePnt);
            break;
        case kPlatformSourceWord:
            m_flags |= kPlatformSource;
            m_usage = WithSource(m_usage, kSourceDds);
            break;
        case kProgSource:
            m_usage = WithSource(m_usage, kSourceProg);
            ReadProgSource(tokenizer);
            break;
        case kMipMap: {
            const unsigned char levels = (unsigned char)tokenizer->ExpectInt();
            m_levels = levels;
            m_levelsAsked = levels;
            if (levels < 1) {
                m_levels = 1;
                m_levelsAsked = 1;
            }
            break;
        }
        case kAutoGenMipLevelsWord:
            m_flags |= kAutoGenMipLevels;
            break;
        case kMpegMacroBlocksWord:
            m_usage |= kMpegMacroBlocks;
            break;
        case kRenderTargetWord:
            m_usage |= kRenderTarget;
            break;
        case kDepthStencilWord:
            m_usage |= kDepthStencil;
            break;
        case kFrequentModifyWord:
            m_usage |= kFrequentModify;
            break;
        case kInfrequentModifyWord:
            m_usage |= kInfrequentModify;
            break;
        case kFlipVerticallyWord:
            m_usage |= kFlipVertically;
            break;
        case kTextureStage:
            m_textureStage = (unsigned char)tokenizer->ExpectInt();
            break;
        case kLightDirection: {
            AtVector3 direction;
            direction.Read(tokenizer);
            SetLightDirection(&direction);
            break;
        }
        case kTransparentColorWord:
            m_transparent[0] = (unsigned char)tokenizer->ExpectInt();
            m_transparent[1] = (unsigned char)tokenizer->ExpectInt();
            m_transparent[2] = (unsigned char)tokenizer->ExpectInt();
            m_flags |= kTransparentColor;
            break;
        case kDiffuseLightMapWord:
            m_flags |= kDiffuseLightMap;
            break;
        case kFrontBufferSourceWord:
            m_flags |= kFrontBufferSource;
            break;
        case kBackBufferSourceWord:
            m_flags |= kBackBufferSource;
            break;
        case kDemandLoadWord:
            m_flags |= kDemandLoad;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

void AtTextureBase::ClearPending() {
    if (m_flags & kPending) {
        m_flags &= ~kPending;
    }
}

void AtTextureBase::SetTransparentColor(const unsigned char* rgba) {
    m_transparent[0] = rgba[0];
    m_transparent[1] = rgba[1];
    m_transparent[2] = rgba[2];
    m_transparent[3] = rgba[3];
    m_flags |= kTransparentColor;
}

void AtTextureBase::ClearTransparentColor() {
    m_flags &= ~kTransparentColor;
}

void AtTextureBase::SetLightDirection(const AtVector3* direction) {
    AtVector3 unit;
    unit.SetNormalized(*direction);
    m_lightDirection[0] = AxisToByte(unit.x);
    m_lightDirection[1] = AxisToByte(unit.y);
    m_lightDirection[2] = AxisToByte(unit.z);
}

int AtTextureBase::Unknown8() {
    return 0;
}

int AtTextureBase::Unknown9() {
    return 0;
}

void AtTextureBase::ClearModifyUsage(int) {
    m_usage &= ~(unsigned int)(kRenderTarget | kFrequentModify | kInfrequentModify);
}

void AtTextureBase::FreeProgSource() {
    operator delete(m_progSource);
}

void AtTextureBase::ReadProgSource(AtIoTokenizer* tokenizer) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    m_progSource = AtNewObject<AtProgTextureSource>();
    AtPopHeap();
    if (m_progSource == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_progSource->m_width = tokenizer->ExpectInt();
    m_progSource->m_height = tokenizer->ExpectInt();
    m_progSource->m_format.Read(tokenizer);
}
