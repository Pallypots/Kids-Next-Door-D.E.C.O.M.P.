// See include/at/AtShader.h.
#include "at/AtShader.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);
extern "C" char* strcat(char* to, const char* from);

void* AtShader::s_heap;
void* AtProgShaderComponent::s_heap;

namespace {

// The words these readers tell apart. The whole list is in src/at/AtShaderDatabase.cpp.
enum Word {
    kProgShaderComponent = 0x3B, kCombineOp = 0x3C, kVertexProcess = 0x3D, kPixelProcess = 0x3E, kTextureStage = 0x3F, kTexture = 0x40, kTextureWithNameSpace = 0x41, kTxtCoordOffset = 0x42,
    kPointSampled = 0x43, kBilinear = 0x44, kAnisotropic = 0x45, kFlatCubic = 0x46, kGaussianCubic = 0x47, kMipNearest = 0x48, kMipLinear = 0x49, kWrap = 0x4A, kClamp = 0x4B, kBorder = 0x4C,
    kCoordGenNormalWithScaleAndBias = 0x4D, kCoordGenNormal = 0x4E, kCoordGenPos = 0x4F, kCoordGenReflectionWithScaleAndBias = 0x50, kCoordGenReflection = 0x51, kCoordGenToonLighting = 0x52,
    kCoordViewToWorldTransform = 0x53, kCoord2DTransform = 0x54, kCoord3DTransform = 0x55, kCoord4DTransform = 0x56, kCoordProjected = 0x57, kAdd = 0x58, kBlendAlpha = 0x59,
    kBlendDestAlpha = 0x5A, kModulate = 0x5B, kModulate2x = 0x5C, kModulateByAlpha = 0x5D, kModulateByAlpha2x = 0x5E, kModulateByAlphaAddColor = 0x5F, kSubtract = 0x60,
    kBlendInverseAlpha = 0x61, kStagedShaderComponent = 0x62, kModulateOp = 0x63, kAlphaSource = 0x64, kSelectConstant = 0x65, kSelectDiffuse = 0x66, kSelectTexture = 0x67,
    kSelectModulatedOutput = 0x68, kModulateTextureByConstant = 0x69, kModulateTextureByDiffuse = 0x6A, kModulateTextureByDiffuseAndConstant = 0x6B, kCubeNormalizationTexture = 0x6C,
    kFlipFaceCulling = 0x6D, kDisableDepthBuffer = 0x6E, kDisableDepthWrites = 0x6F, kDisableColorWrites = 0x70
};

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kBadWord = 1, kBadCount = 0x23, kAlreadyGiven = 0x27 };

typedef decltype(sizeof(0)) Size;
struct Placement {};

}  // namespace

inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

namespace {

// What new[] does in the original, written out, because the original goes on after a null from a heap with no room and a modern new[] assumes it does not. The count stands before the first element.
// The size wraps at 32 bits, as there.
const Size kCookie = sizeof(void*);

template <class T> T* NewArray(void* heap, unsigned int count) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    void* block = operator new((Size)(unsigned int)(count * (unsigned int)sizeof(T) + (unsigned int)kCookie));
    T* items = 0;
    if (block != 0) {
        *(unsigned int*)block = count;
        items = (T*)((char*)block + kCookie);
        for (unsigned int i = 0; i < count; i++) {
            new (items + i, Placement()) T;
        }
    }
    AtPopHeap();
    if (items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return items;
}

template <class T> void DeleteArray(T* items) {
    void* block = (char*)items - kCookie;
    for (unsigned int i = *(unsigned int*)block; i > 0; i--) {
        items[i - 1].~T();
    }
    operator delete(block);
}

// A name is eight bytes, not always terminated.
void ReportNotFound(const char* name, const char* what) {
    char message[0x80];
    for (int i = 0; i < 8; i++) {
        message[i] = name[i];
    }
    message[8] = 0;
    strcat(message, what);
    AtFatal(message, 0, 0, 0);
}

}  // namespace

// ---- combine op -----------------------------------------------------------------------------------------------------------

AtShaderCombineOp::AtShaderCombineOp() {
    m_op = 0;
}

void AtShaderCombineOp::Read(AtIoTokenizer* tokenizer) {
    switch (tokenizer->Next()) {
    case kAdd: m_op = 1; break;
    case kBlendAlpha: m_op = 2; break;
    case kBlendDestAlpha: m_op = 3; break;
    case kModulate: m_op = 4; break;
    case kModulate2x: m_op = 5; break;
    case kModulateByAlpha: m_op = 6; break;
    case kModulateByAlpha2x: m_op = 7; break;
    case kModulateByAlphaAddColor: m_op = 8; break;
    case kSubtract: m_op = 9; break;
    case kBlendInverseAlpha: m_op = 10; break;
    default: tokenizer->Error(AtIoTokenizer::kExpectedKeyword); break;
    }
}

// ---- texture stage --------------------------------------------------------------------------------------------------------

AtTextureStage::AtTextureStage() {
    Reset();
}

AtTextureStage::~AtTextureStage() {
    Reset();
}

void AtTextureStage::Reset() {
    m_texture = 0;
    m_flags = 0;
    m_coordOffset = 0;
    m_transform = 0;
    m_border.r = 0;
    m_border.g = 0;
    m_border.b = 0;
    m_border.a = 0;
    m_anisotropy = 1.0f;
}

// Each field is set as the compiler left it, by clearing only those of its bits the new value lacks. So the last word of a group wins, with this exception: kFlagViewToWorld is not among the bits a
// transform word clears, so CoordViewToWorldTransform before Coord2DTransform leaves both.
void AtTextureStage::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, const char* ns) {
    tokenizer->ExpectLeftCurly();
    m_flags |= kFlagMipLinear;
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        char name[8];
        switch (token) {
        case kTexture:
            if (m_flags & kFlagCubeNormalization) {
                tokenizer->Error(kAlreadyGiven);
            }
            tokenizer->ExpectName(name);
            m_texture = ns != 0 && ns[0] != 0 ? owner->m_textureSpaces.Find(ns, name) : owner->m_textures.Find(name);
            if (m_texture == 0) {
                ReportNotFound(name, ": unable to find texture used by shader");
            }
            break;
        case kTextureWithNameSpace: {
            if (m_flags & kFlagCubeNormalization) {
                tokenizer->Error(kAlreadyGiven);
            }
            char space[8];
            tokenizer->ExpectName(name);
            tokenizer->ExpectName(space);
            m_texture = space[0] != 0 ? owner->m_textureSpaces.Find(space, name) : owner->m_textures.Find(name);
            if (m_texture == 0) {
                ReportNotFound(name, ": unable to find texture used by shader");
            }
            break;
        }
        case kTxtCoordOffset:
            m_coordOffset = (unsigned short)tokenizer->ExpectInt();
            break;
        case kPointSampled:
            m_flags &= ~0xFu;
            break;
        case kBilinear:
            m_flags = (m_flags & ~0xEu) | 1;
            break;
        case kAnisotropic:
            m_flags = (m_flags & ~0xDu) | 2;
            m_anisotropy = tokenizer->ExpectReal();
            break;
        case kFlatCubic:
            m_flags = (m_flags & ~0xBu) | 4;
            break;
        case kGaussianCubic:
            m_flags = (m_flags & ~0x7u) | 8;
            break;
        case kMipNearest:
            m_flags &= ~(unsigned int)kFlagMipLinear;
            break;
        case kMipLinear:
            m_flags |= kFlagMipLinear;
            break;
        case kWrap:
            m_flags &= ~(unsigned int)(kFlagClamp | kFlagBorder);
            break;
        case kClamp:
            m_flags = (m_flags & ~(unsigned int)kFlagBorder) | kFlagClamp;
            break;
        case kBorder:
            m_flags = (m_flags & ~(unsigned int)kFlagClamp) | kFlagBorder;
            m_border.Read(tokenizer);
            break;
        case kCoordGenNormal:
            m_flags = (m_flags & ~0x180u) | 0x40;
            break;
        case kCoordGenNormalWithScaleAndBias:
            m_flags = (m_flags & ~0x140u) | 0x80;
            break;
        case kCoordGenPos:
            m_flags = (m_flags & ~0x100u) | 0xC0;
            break;
        case kCoordGenReflection:
            m_flags = (m_flags & ~0xC0u) | 0x100;
            break;
        case kCoordGenReflectionWithScaleAndBias:
            m_flags = (m_flags & ~0x80u) | 0x140;
            break;
        case kCoordGenToonLighting:
            m_flags = (m_flags & ~0x40u) | 0x180;
            break;
        case kCoordViewToWorldTransform:
            m_flags = (m_flags & ~(unsigned int)kTransformMask) | kFlagViewToWorld;
            break;
        case kCoord2DTransform:
            m_flags = (m_flags & ~(unsigned int)(kFlagTransform3D | kFlagTransform4D)) | kFlagTransform2D;
            m_transform = (unsigned char)tokenizer->ExpectInt();
            break;
        case kCoord3DTransform:
            m_flags = (m_flags & ~(unsigned int)(kFlagTransform2D | kFlagTransform4D)) | kFlagTransform3D;
            m_transform = (unsigned char)tokenizer->ExpectInt();
            break;
        case kCoord4DTransform:
            m_flags = (m_flags & ~(unsigned int)(kFlagTransform2D | kFlagTransform3D)) | kFlagTransform4D;
            m_transform = (unsigned char)tokenizer->ExpectInt();
            break;
        case kCoordProjected:
            m_flags |= kFlagProjected;
            break;
        case kCubeNormalizationTexture:
            if (m_texture != 0) {
                tokenizer->Error(kAlreadyGiven);
            }
            m_flags |= kFlagCubeNormalization;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
    }
}

// ---- staged component -----------------------------------------------------------------------------------------------------

AtStagedShaderComponent::AtStagedShaderComponent() {
    m_modulate = kOpSelectTexture;
    m_switches = 0;
    m_alphaSource = kFromModulatedOutput;
}

AtStagedShaderComponent::~AtStagedShaderComponent() {
}

void AtStagedShaderComponent::Clear() {
    m_stage.Reset();
    m_modulate = kOpSelectTexture;
    m_alphaSource = kFromModulatedOutput;
    m_switches = 0;
}

void AtStagedShaderComponent::ReadModulateOp(AtIoTokenizer* tokenizer) {
    switch (tokenizer->Next()) {
    case kSelectConstant: m_modulate = kOpSelectConstant; break;
    case kSelectDiffuse: m_modulate = kOpSelectDiffuse; break;
    case kSelectTexture: m_modulate = kOpSelectTexture; break;
    case kModulateTextureByConstant: m_modulate = kOpTextureByConstant; break;
    case kModulateTextureByDiffuse: m_modulate = kOpTextureByDiffuse; break;
    case kModulateTextureByDiffuseAndConstant: m_modulate = kOpTextureByDiffuseAndConstant; break;
    default: tokenizer->Error(AtIoTokenizer::kExpectedKeyword); break;
    }
}

void AtStagedShaderComponent::ReadAlphaSource(AtIoTokenizer* tokenizer) {
    switch (tokenizer->Next()) {
    case kSelectConstant: m_alphaSource = kFromConstant; break;
    case kSelectDiffuse: m_alphaSource = kFromDiffuse; break;
    case kSelectTexture: m_alphaSource = kFromTexture; break;
    case kSelectModulatedOutput: m_alphaSource = kFromModulatedOutput; break;
    default: tokenizer->Error(AtIoTokenizer::kExpectedKeyword); break;
    }
}

void AtStagedShaderComponent::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, unsigned int, const char* ns) {
    tokenizer->ExpectToken(kStagedShaderComponent);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kCombineOp: m_combine.Read(tokenizer); break;
        case kTextureStage: m_stage.Read(owner, tokenizer, ns); break;
        case kModulateOp: ReadModulateOp(tokenizer); break;
        case kAlphaSource: ReadAlphaSource(tokenizer); break;
        case kFlipFaceCulling: m_switches |= kShaderFlipFaceCulling; break;
        case kDisableDepthBuffer: m_switches |= kShaderDisableDepthBuffer; break;
        case kDisableDepthWrites: m_switches |= kShaderDisableDepthWrites; break;
        case kDisableColorWrites: m_switches |= kShaderDisableColorWrites; break;
        default: tokenizer->Error(kBadWord); break;
        }
    }
}

// ---- programmable component -----------------------------------------------------------------------------------------------

void AtProgShaderComponent::SetHeap(void* heap) {
    s_heap = heap;
}

AtProgShaderComponent::AtProgShaderComponent() {
    m_switches = 0;
    m_stageCount = 0;
    m_vertexProcessCount = 0;
    m_vertexProcesses = 0;
    m_pixelProcess = 0;
}

AtProgShaderComponent::~AtProgShaderComponent() {
}

void AtProgShaderComponent::Clear() {
    if (m_vertexProcesses != 0) {
        operator delete(m_vertexProcesses);
    }
    m_switches = 0;
    m_stageCount = 0;
    m_vertexProcessCount = 0;
    m_vertexProcesses = 0;
    m_pixelProcess = 0;
}

unsigned int AtProgShaderComponent::CountTextures() const {
    unsigned int count = 0;
    for (int i = 0; i < kMaxStages; i++) {
        if (m_stages[i].m_texture != 0) {
            count++;
        }
    }
    return count;
}

void AtProgShaderComponent::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, const char* ns) {
    tokenizer->ExpectToken(kProgShaderComponent);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kCombineOp:
            m_combine.Read(tokenizer);
            break;
        case kVertexProcess:
            if (m_vertexProcessCount > 0) {
                tokenizer->Error(kAlreadyGiven);
            }
            m_vertexProcessCount = (unsigned char)tokenizer->ExpectCount();
            if (m_vertexProcessCount < 1) {
                tokenizer->Error(kBadCount);
            }
            if (s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(s_heap);
            m_vertexProcesses = (void**)operator new(m_vertexProcessCount * sizeof(void*));
            AtPopHeap();
            if (m_vertexProcesses == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int i = 0; i < m_vertexProcessCount; i++) {
                char name[8];
                tokenizer->ExpectName(name);
                m_vertexProcesses[i] = owner->m_vertexProcesses.Find(name);
                if (m_vertexProcesses[i] == 0) {
                    ReportNotFound(name, ": Atlas Graphics is unable to find vertex process used by shader");
                }
            }
            tokenizer->ExpectRightCurly();
            break;
        case kPixelProcess: {
            char name[8];
            tokenizer->ExpectName(name);
            m_pixelProcess = owner->m_pixelProcesses.Find(name);
            if (m_pixelProcess == 0) {
                ReportNotFound(name, ": Atlas Graphics is unable to find pixel process used by shader");
            }
            break;
        }
        case kTextureStage:
            // Over eight is reported and, if the report comes back, read anyway, over what follows.
            m_stageCount = (unsigned char)tokenizer->ExpectCount();
            if (m_stageCount > kMaxStages) {
                tokenizer->Error(kBadCount);
            }
            for (unsigned int i = 0; i < m_stageCount; i++) {
                tokenizer->ExpectToken(kTextureStage);
                m_stages[i].Read(owner, tokenizer, ns);
            }
            tokenizer->ExpectRightCurly();
            break;
        case kFlipFaceCulling: m_switches |= kShaderFlipFaceCulling; break;
        case kDisableDepthBuffer: m_switches |= kShaderDisableDepthBuffer; break;
        case kDisableDepthWrites: m_switches |= kShaderDisableDepthWrites; break;
        case kDisableColorWrites: m_switches |= kShaderDisableColorWrites; break;
        default:
            tokenizer->Error(kBadWord);
            break;
        }
    }
}

// ---- shader ---------------------------------------------------------------------------------------------------------------

void AtShader::SetHeap(void* heap) {
    s_heap = heap;
}

AtShader::AtShader() {
    m_progCount = 0;
    m_progs = 0;
    m_stagedCount = 0;
    m_stageds = 0;
    m_transformCount = 0;
    for (int i = 0; i < 8; i++) {
        m_transforms[i] = 0;
    }
}

AtShader::~AtShader() {
    AtShader::Clear();
}

void AtShader::Clear() {
    if (m_progs != 0) {
        for (unsigned int i = 0; i < m_progCount; i++) {
            m_progs[i].Clear();
        }
        DeleteArray(m_progs);
    }
    if (m_stageds != 0) {
        for (unsigned int i = 0; i < m_stagedCount; i++) {
            m_stageds[i].Clear();
        }
        DeleteArray(m_stageds);
    }
    m_progCount = 0;
    m_progs = 0;
    m_stagedCount = 0;
    m_stageds = 0;
    m_transformCount = 0;
    for (int i = 0; i < 8; i++) {
        m_transforms[i] = 0;
    }
}

void AtShader::NoteProgTransforms() {
    m_transformCount = 0;
    for (unsigned int i = 0; i < m_progCount; i++) {
        const AtProgShaderComponent& prog = m_progs[i];
        for (unsigned int j = 0; j < prog.m_stageCount; j++) {
            if (prog.m_stages[j].m_flags & AtTextureStage::kTransformMask) {
                m_transforms[m_transformCount] = prog.m_stages[j].m_transform;
                m_transformCount++;
            }
        }
    }
}

void AtShader::NoteStagedTransforms() {
    m_transformCount = 0;
    for (unsigned int i = 0; i < m_stagedCount; i++) {
        if (m_stageds[i].m_stage.m_flags & AtTextureStage::kTransformMask) {
            m_transforms[m_transformCount] = m_stageds[i].m_stage.m_transform;
            m_transformCount++;
        }
    }
}

void AtShader::Read(AtMaterialOwner* owner, AtShaderDatabase*, AtIoTokenizer* tokenizer, const char* ns) {
    if (m_progs != 0 || m_stageds != 0) {
        Clear();
    }
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        if (token == kProgShaderComponent) {
            if (m_progs != 0) {
                tokenizer->Error(kAlreadyGiven);
            }
            m_progCount = tokenizer->ExpectCount();
            m_progs = NewArray<AtProgShaderComponent>(s_heap, m_progCount);
            for (unsigned int i = 0; i < m_progCount; i++) {
                m_progs[i].Read(owner, tokenizer, ns);
            }
            tokenizer->ExpectRightCurly();
        } else if (token == kStagedShaderComponent) {
            if (m_stageds != 0) {
                tokenizer->Error(kAlreadyGiven);
            }
            m_stagedCount = tokenizer->ExpectCount();
            m_stageds = NewArray<AtStagedShaderComponent>(s_heap, m_stagedCount);
            for (unsigned int i = 0; i < m_stagedCount; i++) {
                m_stageds[i].Read(owner, tokenizer, i, ns);
            }
            tokenizer->ExpectRightCurly();
        } else {
            tokenizer->Error(kBadWord);
        }
    }
    NoteStagedTransforms();
}
