// See include/at/AtShaderDatabase.h.
#include "at/AtShaderDatabase.h"

#include "at/AtText.h"

void* AtShaderDatabase::s_heap;

namespace {
enum Word { kNameSpace = 0x39, kShader = 0x3A };
}

// In the original's order, which has each WithScaleAndBias word after the word it extends and its id before.
const AtIoKeyword AtShaderDatabase::s_keywords[AtShaderDatabase::kKeywordCount] = {
    {kNameSpace, "NameSpace"},
    {kShader, "Shader"},
    {0x3B, "ProgShaderComponent"},
    {0x3C, "CombineOp"},
    {0x3D, "VertexProcess"},
    {0x3E, "PixelProcess"},
    {0x3F, "TextureStage"},
    {0x40, "Texture"},
    {0x41, "TextureWithNameSpace"},
    {0x42, "TxtCoordOffset"},
    {0x43, "PointSampled"},
    {0x44, "Bilinear"},
    {0x45, "Anisotropic"},
    {0x46, "FlatCubic"},
    {0x47, "GaussianCubic"},
    {0x48, "MipNearest"},
    {0x49, "MipLinear"},
    {0x4A, "Wrap"},
    {0x4B, "Clamp"},
    {0x4C, "Border"},
    {0x4E, "CoordGenCameraSpaceNormal"},
    {0x4D, "CoordGenCameraSpaceNormalWithScaleAndBias"},
    {0x4F, "CoordGenCameraSpacePos"},
    {0x51, "CoordGenCameraSpaceReflection"},
    {0x50, "CoordGenCameraSpaceReflectionWithScaleAndBias"},
    {0x52, "CoordGenCameraSpaceToonLighting"},
    {0x53, "CoordViewToWorldTransform"},
    {0x54, "Coord2DTransform"},
    {0x55, "Coord3DTransform"},
    {0x56, "Coord4DTransform"},
    {0x57, "CoordProjected"},
    {0x58, "Add"},
    {0x59, "BlendAlpha"},
    {0x5A, "BlendDestAlpha"},
    {0x5B, "Modulate"},
    {0x5C, "Modulate2x"},
    {0x5D, "ModulateByAlpha"},
    {0x5E, "ModulateByAlpha2x"},
    {0x5F, "ModulateByAlphaAddColor"},
    {0x60, "Subtract"},
    {0x61, "BlendInverseAlpha"},
    {0x62, "StagedShaderComponent"},
    {0x63, "ModulateOp"},
    {0x64, "AlphaSource"},
    {0x65, "SelectConstant"},
    {0x66, "SelectDiffuse"},
    {0x67, "SelectTexture"},
    {0x68, "SelectModulatedOutput"},
    {0x69, "ModulateTextureByConstant"},
    {0x6A, "ModulateTextureByDiffuse"},
    {0x6B, "ModulateTextureByDiffuseAndConstant"},
    {0x6C, "CubeNormalizationTexture"},
    {0x6D, "FlipFaceCulling"},
    {0x6E, "DisableDepthBuffer"},
    {0x6F, "DisableDepthWrites"},
    {0x70, "DisableColorWrites"},
};

void AtShaderDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtShaderDatabase::AtShaderDatabase() {
    m_count = 0;
}

// The original clears the map twice, here and in Clear. The second finds nothing to do.
AtShaderDatabase::~AtShaderDatabase() {
    GmNameMap::Clear();
    m_count = 0;
    GmNameMap::Clear();
}

void AtShaderDatabase::Clear() {
    GmNameMap::Clear();
    m_count = 0;
    GmNameMap::Clear();
}

void AtShaderDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    const int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        tokenizer->ExpectToken(kShader);
        ns = space;
    } else if (token != kShader) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    m_count = tokenizer->ExpectCount();
    Init(m_count);
    owner->AddShaders(this, ns);
    OnReady();
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kShader);
        char name[8];
        tokenizer->ExpectName(name);
        AtShader* shader = ShaderAt(i);
        Insert(name, shader);
        shader->Read(owner, this, tokenizer, ns);
    }
    tokenizer->ExpectRightCurly();
}

void AtShaderDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGS", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGS");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

void AtMaterialOwner::AddShaders(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_shaderSpaces.Add(ns, map);
        return;
    }
    m_shaders.Add(map);
}
