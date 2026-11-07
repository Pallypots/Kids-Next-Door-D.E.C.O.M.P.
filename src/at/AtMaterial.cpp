// AtMaterial and AtColor. Original address clusters 0x00038370-0x00038F6F, 0x00041270-0x000412BD.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtMaterial.h"
#include "at/AtMaterialDatabase.h"

extern "C" void* memcpy(void* to, const void* from, decltype(sizeof(0)) count);

namespace {

// Keyword ids, as in the table below.
enum {
    kAmbientColor = 64, kDiffuseColor, kSpecularColor, kConstantColorWord, kColorModulateWord, kColorDecal, kAlphaModulateWord, kAlphaDecal, kGouraudShadeWord, kFlatShade, kAlphaCompareWord, kBlendWord,
    kBlendSeparateAlphaWord, kVertexAlpha, kConstantAlphaWord, kSimpleTextureShader, kShader, kShaderWithNameSpace, kAntiAliasedTransparencyWord, kRangedAntiAliasedTransparencyWord, kProjectionMask,
    kStencilOperationWord, kEmissiveWord, kSingleSidedWord, kDoubleSidedWord, kDisableLightingWord, kPowerWord, kOverBrightWord, kBlendSortBias, kDisableFoggingWord, kDisableDepthBufferWord,
    kDisableDepthWritesWord, kDisableColorWritesWord, kDepthCompareWord, kClampWord, kWrap,
    kFirstBlendMode = 100, kLastBlendMode = 110,
    kAlways = 111, kEqual, kGreater, kGreaterEqual, kLess, kLessEqual, kNever, kNotEqual,
    kNoChange = 119, kZero, kReplace, kIncrementWrap, kIncrementClamp, kDecrementWrap, kDecrementClamp, kInvert
};

// Codes of AtIoTokenizer::Error used here.
enum { kBadIndex = 0x23, kNoSuchShader = 0x25, kBadKeyword = 0x1E };

}  // namespace

const AtIoKeyword AtMaterial::s_keywords[AtMaterial::kKeywordCount] = {
    {57, "NameSpace"},
    {58, "ProgShaderTexture"},
    {59, "StagedShaderTexture"},
    {60, "ProgShaderTextureWithNameSpace"},
    {61, "StagedShaderTextureWithNameSpace"},
    {62, "Default"},
    {63, "Material"},
    {64, "AmbientColor"},
    {65, "DiffuseColor"},
    {66, "SpecularColor"},
    {67, "ConstantColor"},
    {68, "ColorModulate"},
    {69, "ColorDecal"},
    {70, "AlphaModulate"},
    {71, "AlphaDecal"},
    {72, "GouraudShade"},
    {73, "FlatShade"},
    {74, "AlphaCompare"},
    {97, "DepthCompare"},
    {75, "Blend"},
    {76, "BlendSeparateAlpha"},
    {77, "VertexAlpha"},
    {78, "ConstantAlpha"},
    {79, "SimpleTextureShader"},
    {80, "Shader"},
    {81, "ShaderWithNameSpace"},
    {86, "Emissive"},
    {87, "SingleSided"},
    {88, "DoubleSided"},
    {89, "DisableLighting"},
    {90, "Power"},
    {91, "OverBright"},
    {92, "BlendSortBias"},
    {93, "DisableFogging"},
    {94, "DisableDepthBuffer"},
    {95, "DisableDepthWrites"},
    {96, "DisableColorWrites"},
    {100, "Add"},
    {101, "BlendAlpha"},
    {102, "BlendDestAlpha"},
    {103, "Modulate"},
    {104, "Modulate2x"},
    {105, "ModulateByAlpha"},
    {106, "ModulateByAlpha2x"},
    {107, "ModulateByAlphaAddColor"},
    {108, "Subtract"},
    {109, "BlendInverseAlpha"},
    {110, "BlendZeroByAlpha"},
    {111, "Always"},
    {112, "Equal"},
    {113, "Greater"},
    {114, "GreaterEqual"},
    {115, "Less"},
    {116, "LessEqual"},
    {117, "Never"},
    {118, "NotEqual"},
    {84, "ProjectionMask"},
    {85, "StencilOperation"},
    {82, "AntiAliasedTransparency"},
    {83, "RangedAntiAliasedTransparency"},
    {119, "NoChange"},
    {120, "Zero"},
    {121, "Replace"},
    {122, "IncrementWrap"},
    {123, "IncrementClamp"},
    {124, "DecrementWrap"},
    {125, "DecrementClamp"},
    {126, "Invert"},
    {99, "Wrap"},
    {98, "Clamp"},
};

void AtColor::SetWhite() {
    r = 255;
    g = 255;
    b = 255;
    a = 255;
}

void AtColor::Read(AtIoTokenizer* tokenizer) {
    r = (unsigned char)tokenizer->ExpectInt();
    g = (unsigned char)tokenizer->ExpectInt();
    b = (unsigned char)tokenizer->ExpectInt();
    a = (unsigned char)tokenizer->ExpectInt();
}

AtMaterialDesc::AtMaterialDesc() {
    m_unknown00 = 0;
    m_store = 0;
    m_ambient.SetWhite();
    m_diffuse.SetWhite();
    m_specular.SetWhite();
    m_constant.SetWhite();
    m_flags = 0;
    m_alphaCompare = 0;
    m_alphaReference = 0;
    m_blend = 0;
    m_stencilFail = 0;
    m_stencilDepthFail = 0;
    m_stencilCompare = 0;
    m_stencilReference = 0;
    m_unknown44 = 0;
    m_shader = 0;
    m_stageCount = 0;
    m_stages = 0;
    m_textureCount = 0;
    m_textures = 0;
    m_projectionMask = -1;
    m_range = 3.4028234663852886e38f;  // 0x7F7FFFFF
}

AtMaterial::AtMaterial() {
    m_shader = 0;
    m_stages = 0;
    m_textures = 0;
    m_flags = 0;
    m_stageCount = 0;
    m_unknown28 = 0;
    m_range = 3.4028234663852886e38f;
    m_textureCount = 0;
    m_blend = 0;
    m_alphaCompare = 0;
    m_alphaReference = 0;
    m_depthCompare = 2;
    m_stencilFail = 0;
    m_stencilDepthFail = 0;
    m_stencilCompare = 0;
    m_stencilReference = 0;
    m_projectionMask = -1;
    m_sortBias = 0.0f;
    m_power = 0.0f;
    m_store = 0;
}

AtMaterial::~AtMaterial() {
    m_shader = 0;
    m_stages = 0;
    m_textures = 0;
    m_flags = 0;
    m_stageCount = 0;
    m_unknown28 = 0;
    m_textureCount = 0;
    m_blend = 0;
    m_alphaCompare = 0;
    m_alphaReference = 0;
    m_stencilFail = 0;
    m_stencilDepthFail = 0;
    m_stencilCompare = 0;
    m_stencilReference = 0;
    m_sortBias = 0.0f;
    m_power = 0.0f;
    m_store = 0;
    m_range = 3.4028234663852886e38f;
    m_depthCompare = 2;
    m_projectionMask = -1;
}

void AtMaterial::Reset() {
    m_shader = 0;
    m_stages = 0;
    m_textures = 0;
    m_flags = 0;
    m_stageCount = 0;
    m_unknown28 = 0;
    m_range = 3.4028234663852886e38f;
    m_textureCount = 0;
    m_blend = 0;
    m_alphaCompare = 0;
    m_alphaReference = 0;
    m_depthCompare = 2;
    m_stencilFail = 0;
    m_stencilDepthFail = 0;
    m_stencilCompare = 0;
    m_stencilReference = 0;
    m_projectionMask = -1;
    m_sortBias = 0.0f;
    m_power = 0.0f;
    m_store = 0;
}

void AtMaterial::SetAlphaCompare(unsigned char compare, unsigned char reference) {
    m_flags |= kAlphaCompare;
    m_alphaCompare = compare;
    m_alphaReference = reference;
}

void AtMaterial::ClearAlphaCompare() {
    m_flags &= ~(unsigned int)kAlphaCompare;
}

int AtMaterial::ReadBlendMode(AtIoTokenizer* tokenizer, int token) {
    if (token >= kFirstBlendMode && token <= kLastBlendMode) {
        return token - kFirstBlendMode + 1;
    }
    tokenizer->Error(kBadKeyword);
    return 0;
}

int AtMaterial::ReadStencilAction(AtIoTokenizer* tokenizer) {
    switch (tokenizer->Next()) {
    case kNoChange: return 0;
    case kZero: return 1;
    case kReplace: return 2;
    case kIncrementWrap: return 3;
    case kIncrementClamp: return 4;
    case kDecrementWrap: return 5;
    case kDecrementClamp: return 6;
    case kInvert: return 7;
    default:
        tokenizer->Error(kBadKeyword);
        return 0;
    }
}

int AtMaterial::ReadCompare(AtIoTokenizer* tokenizer) {
    switch (tokenizer->Next()) {
    case kAlways: return 0;
    case kEqual: return 5;
    case kGreater: return 3;
    case kGreaterEqual: return 4;
    case kLess: return 1;
    case kLessEqual: return 2;
    case kNever: return 7;
    case kNotEqual: return 6;
    default:
        tokenizer->Error(kBadKeyword);
        return 0;
    }
}

void AtMaterial::Read(AtMaterialOwner* owner, AtMaterialDatabase* store, AtIoTokenizer* tokenizer, const char* ns) {
    if (m_flags & kLoaded) {
        Reset();
    }
    m_store = store;
    tokenizer->ExpectLeftCurly();
    char name[8];
    char nameSpace[8];
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kAmbientColor:
            m_ambient.Read(tokenizer);
            break;
        case kDiffuseColor:
            m_diffuse.Read(tokenizer);
            break;
        case kSpecularColor:
            m_specular.Read(tokenizer);
            break;
        case kConstantColorWord:
            m_constant.r = (unsigned char)tokenizer->ExpectInt();
            m_constant.g = (unsigned char)tokenizer->ExpectInt();
            m_constant.b = (unsigned char)tokenizer->ExpectInt();
            m_flags |= kConstantColor;
            break;
        case kColorModulateWord:
            m_flags |= kColorModulate;
            break;
        case kColorDecal:
            m_flags &= ~(unsigned int)kColorModulate;
            break;
        case kAlphaModulateWord:
            m_flags |= kAlphaModulate;
            break;
        case kAlphaDecal:
            m_flags &= ~(unsigned int)kAlphaModulate;
            break;
        case kClampWord:
            m_flags |= kClamp;
            break;
        case kWrap:
            m_flags &= ~(unsigned int)kClamp;
            break;
        case kGouraudShadeWord:
            m_flags |= kGouraudShade;
            break;
        case kFlatShade:
            m_flags &= ~(unsigned int)kGouraudShade;
            break;
        case kAlphaCompareWord: {
            const int compare = ReadCompare(tokenizer);
            m_alphaCompare = (unsigned char)compare;
            if (m_alphaCompare != 0 && m_alphaCompare != 7) {
                m_alphaReference = (unsigned char)tokenizer->ExpectInt();
            }
            m_flags |= kAlphaCompare;
            break;
        }
        case kDepthCompareWord:
            m_depthCompare = (unsigned char)ReadCompare(tokenizer);
            m_flags |= kDepthCompare;
            break;
        case kBlendWord: {
            const int next = tokenizer->Next();
            m_blend = (unsigned char)ReadBlendMode(tokenizer, next);
            m_flags |= kBlend;
            break;
        }
        case kBlendSeparateAlphaWord: {
            const int next = tokenizer->Next();
            m_blendAlpha = (unsigned char)ReadBlendMode(tokenizer, next);
            m_flags |= kBlendSeparateAlpha;
            break;
        }
        case kVertexAlpha:
            m_flags &= ~(unsigned int)kConstantAlpha;
            break;
        case kConstantAlphaWord:
            m_constant.a = (unsigned char)tokenizer->ExpectInt();
            m_flags |= kConstantAlpha;
            break;
        case kSimpleTextureShader: {
            m_shader = 0;
            const unsigned int first = (unsigned int)tokenizer->ExpectInt();
            if (first + 1 > store->m_textureCapacity) {
                tokenizer->Error(kBadIndex);
            }
            m_textureCount = 1;
            m_textures = store->m_textures + first;
            break;
        }
        case kShader:
        case kShaderWithNameSpace: {
            tokenizer->ExpectName(name);
            const char* look = ns;
            if (token == kShaderWithNameSpace) {
                tokenizer->ExpectName(nameSpace);
                look = nameSpace;
            }
            m_shader = owner->FindShader(name, look);
            if (m_shader == 0) {
                tokenizer->Error(kNoSuchShader);
            }
            const unsigned int firstStage = (unsigned int)tokenizer->ExpectInt();
            m_stageCount = (unsigned char)tokenizer->ExpectInt();
            if (m_stageCount + firstStage > store->m_stageCapacity) {
                tokenizer->Error(kBadIndex);
            }
            if (m_stageCount != 0) {
                m_stages = store->m_stages + firstStage;
            }
            const unsigned int firstTexture = (unsigned int)tokenizer->ExpectInt();
            m_textureCount = (unsigned char)tokenizer->ExpectInt();
            if (m_textureCount + firstTexture > store->m_textureCapacity) {
                tokenizer->Error(kBadIndex);
            }
            if (m_textureCount != 0) {
                m_textures = store->m_textures + firstTexture;
            }
            break;
        }
        case kProjectionMask:
            m_projectionMask = tokenizer->ExpectInt();
            break;
        case kAntiAliasedTransparencyWord:
            m_flags |= kAntiAliasedTransparency;
            break;
        case kRangedAntiAliasedTransparencyWord:
            m_flags |= kRangedAntiAliasedTransparency;
            m_range = tokenizer->ExpectReal();
            break;
        case kStencilOperationWord:
            m_stencilFail = (unsigned char)ReadStencilAction(tokenizer);
            m_stencilDepthFail = (unsigned char)ReadStencilAction(tokenizer);
            m_stencilCompare = (unsigned char)ReadCompare(tokenizer);
            m_stencilReference = (unsigned char)tokenizer->ExpectInt();
            m_flags |= kStencilOperation;
            break;
        case kEmissiveWord:
            m_flags |= kEmissive;
            break;
        case kSingleSidedWord:
            m_flags = (m_flags & ~(unsigned int)kDoubleSided) | kSingleSided;
            break;
        case kDoubleSidedWord:
            m_flags = (m_flags & ~(unsigned int)kSingleSided) | kDoubleSided;
            break;
        case kDisableLightingWord:
            m_flags |= kDisableLighting;
            break;
        case kPowerWord:
            m_power = tokenizer->ExpectReal();
            m_flags |= kPower;
            break;
        case kOverBrightWord:
            m_flags |= kOverBright;
            break;
        case kBlendSortBias:
            m_sortBias = tokenizer->ExpectReal();
            break;
        case kDisableFoggingWord:
            m_flags |= kDisableFogging;
            break;
        case kDisableColorWritesWord:
            m_flags |= kDisableColorWrites;
            break;
        case kDisableDepthBufferWord:
            m_flags |= kDisableDepthBuffer;
            break;
        case kDisableDepthWritesWord:
            m_flags |= kDisableDepthWrites;
            break;
        default:
            tokenizer->Error(kBadKeyword);
            break;
        }
    }
    if (owner->m_noTextures < 0) {
        m_shader = 0;
        m_textureCount = 0;
        m_stageCount = 0;
        m_textures = 0;
        m_stages = 0;
    }
    // Blending by adding, or by the inverse of alpha, turns fog off unless the material says so already.
    if (!(m_flags & kDisableFogging) && (m_flags & kBlend) && (m_blend == 1 || m_blend == 9)) {
        m_flags |= kDisableFogging;
    }
    if (!(m_flags & kBlendSeparateAlpha)) {
        m_blendAlpha = m_blend;
    }
    OnLoaded(owner);
    m_flags |= kLoaded;
}

void AtMaterial::Copy(AtMaterialOwner* owner, AtMaterialDatabase* store, const AtMaterial& from) {
    if (m_flags & kLoaded) {
        Reset();
    }
    m_store = store;
    m_flags = from.m_flags;
    m_ambient = from.m_ambient;
    m_diffuse = from.m_diffuse;
    m_specular = from.m_specular;
    m_constant = from.m_constant;
    m_shader = from.m_shader;
    m_projectionMask = from.m_projectionMask;
    m_unknown28 = from.m_unknown28;
    m_range = from.m_range;
    m_stageCount = from.m_stageCount;
    m_textureCount = from.m_textureCount;
    m_blend = from.m_blend;
    m_blendAlpha = from.m_blendAlpha;
    m_alphaCompare = from.m_alphaCompare;
    m_alphaReference = from.m_alphaReference;
    m_depthCompare = from.m_depthCompare;
    m_stencilFail = from.m_stencilFail;
    m_stencilDepthFail = from.m_stencilDepthFail;
    m_stencilCompare = from.m_stencilCompare;
    m_stencilReference = from.m_stencilReference;
    if (from.m_stages != 0) {
        const unsigned int first = store->m_stageUsed;
        store->m_stageUsed = first + m_stageCount;
        m_stages = store->m_stages + first;
        memcpy(m_stages, from.m_stages, m_stageCount * sizeof(void*));
    }
    if (from.m_textures != 0) {
        const unsigned int first = store->m_textureUsed;
        store->m_textureUsed = first + m_textureCount;
        m_textures = store->m_textures + first;
        memcpy(m_textures, from.m_textures, m_textureCount * sizeof(void*));
    }
    OnLoaded(owner);
    m_flags |= kLoaded;
}

void AtMaterial::Describe(AtMaterialDesc* out) const {
    out->m_ambient = m_ambient;
    out->m_diffuse = m_diffuse;
    out->m_specular = m_specular;
    out->m_constant = m_constant;
    out->m_flags = m_flags & ~(unsigned int)kLoaded;
    out->m_alphaCompare = m_alphaCompare;
    out->m_alphaReference = m_alphaReference;
    out->m_depthCompare = m_depthCompare;
    out->m_blend = m_blend;
    out->m_stencilFail = m_stencilFail;
    out->m_stencilDepthFail = m_stencilDepthFail;
    out->m_stencilCompare = m_stencilCompare;
    out->m_stencilReference = m_stencilReference;
    out->m_projectionMask = m_projectionMask;
    out->m_unknown44 = m_unknown28;
    out->m_range = m_range;
    out->m_shader = m_shader;
    out->m_stageCount = m_stageCount;
    out->m_stages = m_stages;
    out->m_textureCount = m_textureCount;
    out->m_textures = m_textures;
    out->m_store = m_store;
}
