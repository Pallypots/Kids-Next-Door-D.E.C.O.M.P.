// See include/at/AtMeshDatabase.h.
#include "at/AtMeshDatabase.h"

#include "at/AtText.h"

void AtFatal(const char* message, int file, int line, int unused);
extern "C" double __cdecl sqrt(double value);

// 0x002B0AA0.
const AtIoKeyword AtMeshDatabase::s_keywords[AtMeshDatabase::kKeywordCount] = {
    {0x39, "Mesh"}, {0x3A, "NameSpace"}, {0x3B, "MatAssignment"}, {0x3C, "MatAssignmentWithNameSpace"}, {0x3D, "MeshComponent"}, {0x3E, "VertexArray"},
    {0x3F, "VertexFormat"}, {0x40, "Pos2D"}, {0x41, "Pos3D"}, {0x42, "Pos4D"}, {0x43, "BlendWeight"}, {0x44, "Normal"},
    {0x45, "DiffuseColor"}, {0x46, "SpecularColor"}, {0x47, "TxtCoord"}, {0x48, "TxtCoord3D0"}, {0x49, "TxtCoord3D1"}, {0x4A, "TxtCoord3D2"},
    {0x4B, "TxtCoord3D3"}, {0x4C, "TxtCoord3D4"}, {0x4D, "TxtCoord3D5"}, {0x4E, "TxtCoord3D6"}, {0x4F, "TxtCoord3D7"}, {0x50, "TxtCoord4D0"},
    {0x51, "TxtCoord4D1"}, {0x52, "TxtCoord4D2"}, {0x53, "TxtCoord4D3"}, {0x54, "TxtCoord4D4"}, {0x55, "TxtCoord4D5"}, {0x56, "TxtCoord4D6"},
    {0x57, "TxtCoord4D7"}, {0x58, "FrequentModify"}, {0x59, "InfrequentModify"}, {0x5A, "FixedSysMem"}, {0x5B, "FixedSysMemOnly"}, {0x5C, "MorphTarget"},
    {0x5D, "Vertex"}, {0x5E, "PosTransform"}, {0x5F, "IndexedPosTransform"}, {0x60, "VertexGroup"}, {0x61, "TriangleGroup"}, {0x62, "TriangleStrip"},
    {0x63, "IndexedTriangleGroup"}, {0x64, "IndexedTriangleStrip"}, {0x65, "ZeroBasedIndexedTriangleGroup"}, {0x66, "Index"}, {0x67, "IndexArray"}, {0x68, "Index8Bit"},
    {0x69, "Index16Bit"}, {0x6A, "Index32Bit"}, {0x6B, "MathTriangle"}, {0x6C, "BspTree"}, {0x6D, "BspPlane"}, {0x6E, "SpriteArray"},
    {0x6F, "SpriteFormat"}, {0x70, "CornerColors"}, {0x71, "SquareSize"}, {0x72, "RectWidthHeight"}, {0x73, "Sprite"}, {0x74, "PivotAxis3D"},
    {0x75, "SpriteGroup"}, {0x76, "VertexLoad"}, {0x77, "DemandLoad"}, {0x78, "HullLayers"}, {0x79, "ProgShader"}, {0x7A, "SoftwareTransform"},
};

// 0x002B0F58. Ids are not unique across the table's sections: 0x5F is both IndexedPosTransform and Source.
const AtIoKeyword AtVertexProcessDatabase::s_keywords[AtVertexProcessDatabase::kKeywordCount] = {
    {0x3F, "VertexFormat"}, {0x40, "Pos2D"}, {0x41, "Pos3D"}, {0x42, "Pos4D"}, {0x43, "BlendWeight"}, {0x5F, "IndexedPosTransform"},
    {0x44, "Normal"}, {0x45, "DiffuseColor"}, {0x46, "SpecularColor"}, {0x47, "TxtCoord"}, {0x48, "TxtCoord3D0"}, {0x49, "TxtCoord3D1"},
    {0x4A, "TxtCoord3D2"}, {0x4B, "TxtCoord3D3"}, {0x4C, "TxtCoord3D4"}, {0x4D, "TxtCoord3D5"}, {0x4E, "TxtCoord3D6"}, {0x4F, "TxtCoord3D7"},
    {0x50, "TxtCoord4D0"}, {0x51, "TxtCoord4D1"}, {0x52, "TxtCoord4D2"}, {0x53, "TxtCoord4D3"}, {0x54, "TxtCoord4D4"}, {0x55, "TxtCoord4D5"},
    {0x56, "TxtCoord4D6"}, {0x57, "TxtCoord4D7"}, {0x58, "VertexProcess"}, {0x5A, "Constant"}, {0x5D, "MicroCode"}, {0x5E, "Binary"},
    {0x5F, "Source"}, {0x59, "Immediate"}, {0x5B, "Real4D"}, {0x5C, "UInt4D"}, {0x60, "VertexArray"}, {0x61, "Vector1D"},
    {0x62, "Vector2D"}, {0x63, "Vector3D"}, {0x64, "Vector4D"}, {0x65, "Short1D"}, {0x66, "Short2D"}, {0x67, "Short3D"},
    {0x68, "Short4D"}, {0x69, "NByte1D"}, {0x6A, "NByte2D"}, {0x6B, "NByte3D"}, {0x6C, "NByte4D"}, {0x6D, "Normal32"},
    {0x6E, "RGBA"}, {0x6F, "UByte4D"}, {0x70, "UShort2D"}, {0x71, "UShort4D"}, {0x72, "ValueZero"}, {0x73, "ValueOne"},
    {0x74, "ValueHalf"}, {0x75, "Value255"}, {0x76, "ValueInverse255"}, {0x77, "RenderContextConstants"}, {0x78, "WorldToViewTransform"}, {0x79, "ViewToWorldTransform"},
    {0x7A, "WorldToClipTransform"}, {0x7B, "ViewToClipTransform"}, {0x7C, "LocalToWorldPalette4x3"}, {0x7D, "LocalToWorldPalette4x4"}, {0x7E, "LocalToViewPalette4x3"}, {0x7F, "LocalToViewPalette4x4"},
    {0x80, "LocalToClipPalette4x3"}, {0x81, "LocalToClipPalette4x4"}, {0x82, "CurrentLocalToViewTransform"}, {0x83, "CurrentViewToLocalTransform"}, {0x84, "ActorLocalToViewTransform"}, {0x85, "ActorViewToLocalTransform"},
    {0x86, "CurrentLocalToWorldTransform"}, {0x87, "CurrentWorldToLocalTransform"}, {0x88, "ActorLocalToWorldTransform"}, {0x89, "ActorWorldToLocalTransform"}, {0x8A, "CurrentLocalToClipTransform"}, {0x8B, "ActorLocalToClipTransform"},
    {0x8C, "Texture2DTransform"}, {0x8D, "TextureTransform"}, {0x8E, "AmbientColor"}, {0x8F, "FogColor"}, {0x90, "ViewportXYWidthHeight"}, {0x91, "MaterialAmbientColor"},
    {0x92, "MaterialDiffuseColor"}, {0x93, "MaterialSpecularColor"}, {0x94, "MaterialConstantColor"}, {0x95, "LocalPointLightPosition"}, {0x96, "WorldPointLightPosition"}, {0x97, "ActorLocalPointLightPosition"},
    {0x98, "PointLightMatCombinedColor"}, {0x99, "PointLightMatCombinedSpecularColor"}, {0x9A, "PointLightRange"}, {0x9B, "PointLightAttenuation"}, {0x9C, "LocalDirectionalLightDirection"}, {0x9D, "WorldDirectionalLightDirection"},
    {0x9E, "ViewDirectionalLightDirection"}, {0x9F, "ActorLocalDirectionalLightDirection"}, {0xA0, "DirectionalLightMatCombinedColor"}, {0xA1, "DirectionalLightMatCombinedSpecularColor"}, {0xA2, "AmbientLightMatCombinedColor"}, {0xA6, "WorldCameraPosition"},
    {0xA7, "WorldCameraForward"}, {0xA8, "WorldCameraDown"}, {0xA3, "LocalCameraPosition"}, {0xA4, "LocalCameraForward"}, {0xA5, "LocalCameraDown"}, {0xA9, "VertexGroupOffset"},
    {0xAA, "FogStart"}, {0xAB, "FogEnd"}, {0xAC, "FogDensity"}, {0xAD, "PackedInverseFogRange_FogEndDivFogRange"}, {0xAE, "PositionDecompression"}, {0xAF, "CurrentCompressedLocalToViewTransform"},
    {0xB0, "CurrentCompressedLocalToWorldTransform"}, {0xB1, "CurrentCompressedLocalToClipTransform"}, {0xB2, "TxtCoordDecompression"}, {0xB3, "ValueImmediate"}, {0xB4, "MorphAmount"}, {0xB5, "SpecularPower"},
    {0xB6, "Position"}, {0xB7, "PointSize"}, {0xB8, "MatrixIndices"}, {0xB9, "Fog"}, {0xBA, "TexCoord"}, {0xBB, "Tangent"},
    {0xBC, "Binormal"},
};

// 0x002B0E90.
const AtIoKeyword AtPixelProcessDatabase::s_keywords[AtPixelProcessDatabase::kKeywordCount] = {
    {0x39, "PixelProcess"}, {0x3A, "Constant"}, {0x3B, "MicroCode"}, {0x3C, "Binary"}, {0x3D, "Source"}, {0x3E, "ValueZero"},
    {0x3F, "ValueOne"}, {0x40, "Value255"}, {0x41, "ValueInverse255"}, {0x42, "RenderContextConstants"}, {0x43, "MaterialAmbientColor"}, {0x44, "MaterialDiffuseColor"},
    {0x45, "MaterialSpecularColor"}, {0x46, "MaterialConstantColor"}, {0x47, "LightAmbientColor"}, {0x48, "LightDiffuseColor"}, {0x49, "LightSpecularColor"}, {0x4A, "AmbientColor"},
    {0x4B, "FogColor"}, {0x4C, "Immediate"}, {0x4D, "Real4D"}, {0x4E, "UInt4D"}, {0x4F, "ValueImmediate"},
};

namespace {

enum MeshWord {
    kMeshWord = 0x39, kNameSpace = 0x3A, kVertexArrayWord = 0x3E, kFrequentModifyWord = 0x58, kInfrequentModifyWord = 0x59, kFixedSysMemWord = 0x5A, kFixedSysMemOnlyWord = 0x5B, kIndexWord = 0x66, kIndexArrayWord = 0x67,
    kIndex8BitWord = 0x68, kIndex16BitWord = 0x69, kIndex32BitWord = 0x6A, kDemandLoadWord = 0x77
};
enum ProcessWord { kPixelProcessWord = 0x39, kVertexProcessWord = 0x58 };

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kOutOfRange = 0x23 };

const int kModeBinaryAllowed = 2;

}  // namespace

// ---- triangles ---------------------------------------------------------------------------------------------------------

void AtMathTriangleNormal(const AtMathTriangle* triangle, float* out) {
    const float* v0 = triangle->m_vertices[0];
    const float* v1 = triangle->m_vertices[1];
    const float* v2 = triangle->m_vertices[2];
    // x87: what the original keeps in registers is double here; its b0 and b1 were stored, so they are float.
    const double a0 = (double)v0[0] - v1[0];
    const double a1 = (double)v0[1] - v1[1];
    const double a2 = (double)v0[2] - v1[2];
    volatile float b0 = v2[0] - v1[0];
    volatile float b1 = v2[1] - v1[1];
    const double b2 = (double)v2[2] - v1[2];
    out[0] = (float)(b2 * a1 - b1 * a2);
    out[1] = (float)(a2 * b0 - b2 * a0);
    out[2] = (float)(b1 * a0 - a1 * b0);
}

void AtMathTriangleSetAxis(AtMathTriangle* triangle) {
    float normal[3];
    AtMathTriangleNormal(triangle, normal);
    volatile float x = normal[0] < 0 ? -normal[0] : normal[0];
    volatile float y = normal[1] < 0 ? -normal[1] : normal[1];
    volatile float z = normal[2] < 0 ? -normal[2] : normal[2];
    const float fx = x, fy = y, fz = z;
    const double length = sqrt((double)fz * fz + (double)fy * fy + (double)fx * fx);
    if (length == 0.0) {
        triangle->m_axis = 3;
    } else if (fx > fy) {
        triangle->m_axis = fz > fx ? 0 : 1;
    } else {
        triangle->m_axis = fz > fy ? 0 : 2;
    }
}

// ---- index arrays ------------------------------------------------------------------------------------------------------

void AtIndexArray::ReadIndices(AtIoTokenizer* tokenizer, unsigned int flags) {
    Allocate(m_count, flags);
    void* memory;
    Lock(&memory, 8);
    if (m_flags & kIndex8Bit) {
        unsigned char* out = (unsigned char*)memory;
        for (unsigned int i = 0; i < m_count; i++) {
            out[i] = (unsigned char)tokenizer->ExpectInt();
        }
    } else if (m_flags & kIndex32Bit) {
        int* out = (int*)memory;
        for (unsigned int i = 0; i < m_count; i++) {
            out[i] = tokenizer->ExpectInt();
        }
    } else {
        unsigned short* out = (unsigned short*)memory;
        for (unsigned int i = 0; i < m_count; i++) {
            out[i] = (unsigned short)tokenizer->ExpectInt();
        }
    }
    Unlock();
}

void AtIndexArray::Read(AtIoTokenizer* tokenizer, unsigned int index) {
    m_index = index;
    tokenizer->ExpectLeftCurly();
    m_flags = 0;
    int token = tokenizer->Next();
    while (token != kIndexWord) {
        switch (token) {
        case kFrequentModifyWord:
            m_flags = (unsigned short)((m_flags & 0xFFF9) | 1);
            break;
        case kInfrequentModifyWord:
            m_flags = (unsigned short)((m_flags & 0xFFFE) | 2);
            break;
        case kFixedSysMemWord:
            m_flags = (unsigned short)((m_flags & 0xFFFE) | 4);
            break;
        case kFixedSysMemOnlyWord:
            m_flags = (unsigned short)((m_flags & 0xFFF8) | 8);
            break;
        case kIndex8BitWord:
            m_flags |= kIndex8Bit;
            break;
        case kIndex16BitWord:
            m_flags &= 0xFFCF;
            break;
        case kIndex32BitWord:
            m_flags |= kIndex32Bit;
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
    m_count = (unsigned int)tokenizer->ExpectCount();
    if (tokenizer->Next() == kDemandLoadWord) {
        m_loadFlags |= kDemandLoad;
        tokenizer->ExpectName(m_name);
    } else {
        tokenizer->m_pushedBack = 1;
        ReadIndices(tokenizer, m_flags);
    }
    tokenizer->ExpectRightCurly();
    tokenizer->ExpectRightCurly();
}

// ---- a mesh ------------------------------------------------------------------------------------------------------------

void AtMesh::Finish() {
    for (unsigned int i = 0; i < m_ownVertexArrays; i++) {
        if (OwnVertexArrayAt(i) != 0) {
            OwnVertexArrayAt(i)->Finish();
        }
    }
    for (unsigned int i = 0; i < m_ownIndexArrays; i++) {
        if (OwnIndexArrayAt(i) != 0) {
            OwnIndexArrayAt(i)->Finish();
        }
    }
    for (unsigned int i = 0; i < m_triangleCount; i++) {
        const unsigned int which = m_triangles[i].m_array;
        AtVertexArray* array;
        if (m_ownVertexArrays != 0) {
            if (which >= m_ownVertexArrays) {
                AtFatal("Atlas Graphics: vertex array reference out of bounds", 0, 0, 0);
            }
            array = VertexArrayAt(which);
        } else {
            array = m_database->VertexArrayAt(which);
        }
        if (array->m_flags & AtVertexArray::kPositionsInMemory) {
            m_triangles[i].m_vertices[0] = array->PositionOf(m_triangles[i].m_vertices[0]);
            m_triangles[i].m_vertices[1] = array->PositionOf(m_triangles[i].m_vertices[1]);
            m_triangles[i].m_vertices[2] = array->PositionOf(m_triangles[i].m_vertices[2]);
            AtMathTriangleSetAxis(&m_triangles[i]);
        }
    }
}

// ---- the database ------------------------------------------------------------------------------------------------------

AtMeshDatabase::AtMeshDatabase() {
    m_meshCount = 0;
    m_unknown18 = 0;
    m_vertexArrayCount = 0;
    m_indexArrayCount = 0;
}

void AtMeshDatabase::Clear() {
    GmNameMap::Clear();
    m_meshCount = 0;
    m_unknown18 = 0;
    m_vertexArrayCount = 0;
    m_indexArrayCount = 0;
}

void AtMeshDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_meshCount > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }
    while (token == kVertexArrayWord || token == kIndexArrayWord) {
        if (token == kVertexArrayWord) {
            if (m_vertexArrayCount != 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_vertexArrayCount = (unsigned int)tokenizer->ExpectCount();
            MakeVertexArrays(owner);
            for (unsigned int i = 0; i < m_vertexArrayCount; i++) {
                tokenizer->ExpectToken(kVertexArrayWord);
                VertexArrayAt(i)->Read(owner, tokenizer, i);
            }
        } else {
            if (m_indexArrayCount != 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_indexArrayCount = (unsigned int)tokenizer->ExpectCount();
            MakeIndexArrays(owner);
            for (unsigned int i = 0; i < m_indexArrayCount; i++) {
                tokenizer->ExpectToken(kIndexArrayWord);
                IndexArrayAt(i)->Read(tokenizer, i);
            }
        }
        tokenizer->ExpectRightCurly();
        token = tokenizer->Next();
    }
    if (token != kMeshWord) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    m_meshCount = (unsigned int)tokenizer->ExpectCount();
    if (m_meshCount < 1) {
        tokenizer->Error(kOutOfRange);
    }
    Init(m_meshCount);
    owner->AddMeshes(this, ns);
    MakeMeshes(owner);
    for (unsigned int i = 0; i < m_meshCount; i++) {
        tokenizer->ExpectToken(kMeshWord);
        char name[8];
        tokenizer->ExpectName(name);
        AtMesh* mesh = MeshAt(i);
        Insert(name, mesh);
        mesh->Read(owner, this, tokenizer, ns);
    }
    tokenizer->ExpectRightCurly();
}

void AtMeshDatabase::Finish() {
    for (unsigned int i = 0; i < m_vertexArrayCount; i++) {
        VertexArrayAt(i)->Finish();
    }
    for (unsigned int i = 0; i < m_indexArrayCount; i++) {
        IndexArrayAt(i)->Finish();
    }
    for (unsigned int i = 0; i < m_meshCount; i++) {
        MeshAt(i)->Finish();
    }
}

void AtMeshDatabase::Load(AtMaterialOwner* owner, const char* name, int mode) {
    if (mode == kModeBinaryAllowed && HasBinaryForm() != 0) {
        LoadBinaryForm(owner, name);
        return;
    }
    AtIoTokenFile file;
    const int status = file.Open(name, "AGG", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGG");
    }
    AtIoPath& path = file.m_file.m_fs->m_path;
    path.PushFromFile(file.m_file.m_file->m_path);
    Read(owner, file.m_tokenizer);
    Finish();
    path.Pop(0);
    file.Close();
}

// ---- vertex processes --------------------------------------------------------------------------------------------------

AtVertexProcessDatabase::AtVertexProcessDatabase() {
    m_count = 0;
}

AtVertexProcessDatabase::~AtVertexProcessDatabase() {
    m_count = 0;
}

void AtVertexProcessDatabase::Clear() {
    m_count = 0;
}

void AtVertexProcessDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    tokenizer->ExpectToken(kVertexProcessWord);
    m_count = (unsigned int)tokenizer->ExpectCount();
    Init(m_count);
    owner->AddVertexProcesses(this);
    Make(owner);
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kVertexProcessWord);
        char name[8];
        tokenizer->ExpectName(name);
        AtProcessItem* item = ItemAt(i);
        Insert(name, item);
        item->ReadVertex(owner, tokenizer);
    }
    tokenizer->ExpectRightCurly();
    for (unsigned int i = 0; i < m_count; i++) {
        ItemAt(i)->OnLoaded(owner);
    }
}

void AtVertexProcessDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGV", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGV");
    }
    AtIoPath& path = file.m_file.m_fs->m_path;
    path.PushFromFile(file.m_file.m_file->m_path);
    Read(owner, file.m_tokenizer);
    path.Pop(0);
    file.Close();
}

// ---- pixel processes ---------------------------------------------------------------------------------------------------

AtPixelProcessDatabase::AtPixelProcessDatabase() {
    m_count = 0;
}

AtPixelProcessDatabase::~AtPixelProcessDatabase() {
    m_count = 0;
    GmNameMap::Clear();
}

void AtPixelProcessDatabase::Clear() {
    m_count = 0;
    GmNameMap::Clear();
}

void AtPixelProcessDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    tokenizer->ExpectToken(kPixelProcessWord);
    m_count = (unsigned int)tokenizer->ExpectCount();
    Init(m_count);
    owner->AddPixelProcesses(this);
    Make(owner);
    for (unsigned int i = 0; i < m_count; i++) {
        tokenizer->ExpectToken(kPixelProcessWord);
        char name[8];
        tokenizer->ExpectName(name);
        AtProcessItem* item = ItemAt(i);
        Insert(name, item);
        item->ReadPixel(owner, tokenizer);
    }
    tokenizer->ExpectRightCurly();
    for (unsigned int i = 0; i < m_count; i++) {
        ItemAt(i)->OnLoaded(owner);
    }
}

void AtPixelProcessDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGP", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGP");
    }
    AtIoPath& path = file.m_file.m_fs->m_path;
    path.PushFromFile(file.m_file.m_file->m_path);
    Read(owner, file.m_tokenizer);
    path.Pop(0);
    file.Close();
}
