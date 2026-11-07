// See include/at/AtDatabaseLoader.h.
#include "at/AtDatabaseLoader.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

char** AtDatabaseLoader::s_textureNames;
char** AtDatabaseLoader::s_shaderNames;
char** AtDatabaseLoader::s_materialNames;
char** AtDatabaseLoader::s_meshNames;
char** AtDatabaseLoader::s_lightNames;
char** AtDatabaseLoader::s_coordSystemNames;
char** AtDatabaseLoader::s_mirrorNames;
char** AtDatabaseLoader::s_weightsNames;
char** AtDatabaseLoader::s_animationNames;
char** AtDatabaseLoader::s_matAssignmentNames;
char** AtDatabaseLoader::s_matAnimationNames;
char** AtDatabaseLoader::s_morphAnimationNames;
char** AtDatabaseLoader::s_lightAnimationNames;
char** AtDatabaseLoader::s_fontNames;
char** AtDatabaseLoader::s_imageNames;
char** AtDatabaseLoader::s_vertexProcessNames;
char** AtDatabaseLoader::s_pixelProcessNames;
void* AtDatabaseLoader::s_heap;

// 0x002B0190.
const AtIoKeyword AtDatabaseLoader::s_keywords[AtDatabaseLoader::kKeywordCount] = {
    {0x39, "TextureDatabase"}, {0x3A, "ShaderDatabase"}, {0x3B, "MaterialDatabase"}, {0x3C, "MeshDatabase"}, {0x3D, "LightDatabase"}, {0x3E, "CoordSysDatabase"},
    {0x3F, "CoordSysMirrorDatabase"}, {0x40, "WeightsDatabase"}, {0x41, "AnimationDatabase"}, {0x42, "MatAssignmentDatabase"}, {0x43, "MatAnimationDatabase"}, {0x44, "MorphAnimationDatabase"},
    {0x45, "LightAnimationDatabase"}, {0x46, "FontDatabase"}, {0x47, "ImageDatabase"}, {0x48, "VertexProcessDatabase"}, {0x49, "PixelProcessDatabase"}, {0x4A, "DatabaseLoader"},
};

namespace {

enum Word {
    kTextureDatabase = 0x39, kShaderDatabase = 0x3A, kMaterialDatabase = 0x3B, kMeshDatabase = 0x3C, kLightDatabase = 0x3D, kCoordSysDatabase = 0x3E, kCoordSysMirrorDatabase = 0x3F, kWeightsDatabase = 0x40,
    kAnimationDatabase = 0x41, kMatAssignmentDatabase = 0x42, kMatAnimationDatabase = 0x43, kMorphAnimationDatabase = 0x44, kLightAnimationDatabase = 0x45, kFontDatabase = 0x46, kImageDatabase = 0x47,
    kVertexProcessDatabase = 0x48, kPixelProcessDatabase = 0x49, kDatabaseLoader = 0x4A
};

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27 };

unsigned int LengthOf(const char* text) {
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

char* FindLast(char* text, char c) {
    char* found = 0;
    for (; *text != 0; text++) {
        if (*text == c) {
            found = text;
        }
    }
    return found;
}

void Append(char*& out, const char* text) {
    while (*text != 0) {
        *out++ = *text++;
    }
    *out = 0;
}

void ReadCount(AtIoTokenizer* tokenizer, unsigned short& count) {
    if (count > 0) {
        tokenizer->Error(kListAlreadyGiven);
    }
    count = (unsigned short)tokenizer->ExpectCount();
}

// The original has this inline at each use. The list is on the token file's heap, cleared, and each name goes on that heap too.
void ReadNames(AtIoTokenizer* tokenizer, unsigned short count, char**& names) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    names = (char**)operator new(count * sizeof(char*));
    AtPopHeap();
    if (names == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        names[i] = 0;
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    for (unsigned int i = 0; i < count; i++) {
        tokenizer->ExpectStringCopy(&names[i]);
    }
    AtPopHeap();
}

void FreeList(char**& names, unsigned short count) {
    if (names != 0) {
        AtFreeNameList(names, count);
        names = 0;
    }
}

}  // namespace

void AtFreeNameList(char** list, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        if (list[i] != 0) {
            operator delete(list[i]);
        }
    }
    operator delete(list);
}

void AtLeafName(const char* path, char* out) {
    const char* leaf = FindLast((char*)path, '\\');
    if (leaf == 0) {
        leaf = FindLast((char*)path, '/');
    }
    if (leaf == 0) {
        leaf = FindLast((char*)path, ':');
    }
    leaf = leaf != 0 ? leaf + 1 : path;
    unsigned int length = 0;
    while (leaf[length] != 0 && leaf[length] != '.') {
        length++;
    }
    if (length >= 8) {
        for (unsigned int i = 0; i < 8; i++) {
            out[i] = leaf[i];
        }
        return;
    }
    for (unsigned int i = 0; i < length; i++) {
        out[i] = leaf[i];
    }
    out[length] = 0;
}

void AtJoinPrefix(char* out, char* name, const char* prefix) {
    char* slash = FindLast(name, '\\');
    if (slash == 0) {
        Append(out, prefix);
        Append(out, "\\");
        Append(out, name);
        return;
    }
    *slash = 0;
    Append(out, name);
    *slash = '\\';
    Append(out, "\\");
    Append(out, prefix);
    Append(out, slash);
}

void AtDatabaseLoader::SetHeap(void* heap) {
    s_heap = heap;
}

void AtDatabaseLoader::Reset() {
    m_owner = 0;
    m_textureCount = 0;
    m_materialCount = 0;
    m_shaderCount = 0;
    m_meshCount = 0;
    m_lightCount = 0;
    m_coordSystemCount = 0;
    m_mirrorCount = 0;
    m_weightsCount = 0;
    m_animationCount = 0;
    m_matAssignmentCount = 0;
    m_matAnimationCount = 0;
    m_morphAnimationCount = 0;
    m_lightAnimationCount = 0;
    m_fontCount = 0;
    m_imageCount = 0;
    m_vertexProcessCount = 0;
    m_pixelProcessCount = 0;
    m_loaderCount = 0;
    m_coordSystems = 0;
    m_mirrors = 0;
    m_weights = 0;
    m_animations = 0;
    m_lightAnimations = 0;
    m_morphAnimations = 0;
    m_matAnimations = 0;
    m_matAssignments = 0;
    s_textureNames = 0;
    s_materialNames = 0;
    s_shaderNames = 0;
    s_meshNames = 0;
    s_lightNames = 0;
    s_coordSystemNames = 0;
    s_mirrorNames = 0;
    s_weightsNames = 0;
    s_animationNames = 0;
    s_matAssignmentNames = 0;
    s_matAnimationNames = 0;
    s_morphAnimationNames = 0;
    s_lightAnimationNames = 0;
    s_fontNames = 0;
    s_imageNames = 0;
    s_vertexProcessNames = 0;
    s_pixelProcessNames = 0;
    m_loaderNames = 0;
    m_child = 0;
    m_unknown2C = 16;
    m_unknown28 = 16;
}

AtDatabaseLoader::AtDatabaseLoader() {
    Reset();
}

AtDatabaseLoader::~AtDatabaseLoader() {
    if (m_owner != 0) {
        AtDatabaseLoader::Unload();
    }
}

void AtDatabaseLoader::Unload() {
    if (m_child != 0) {
        m_child->Unload();
        delete m_child;
        m_child = 0;
    }
    if (m_matAnimations != 0) {
        delete[] m_matAnimations;
    }
    if (m_matAssignments != 0) {
        delete[] m_matAssignments;
    }
    if (m_animations != 0) {
        for (unsigned int i = 0; i < m_animationCount; i++) {
            m_owner->m_animationDevice->FreeDatabase(m_animations[i]);
        }
        operator delete(m_animations);
    }
    if (m_coordSystems != 0) {
        delete[] m_coordSystems;
    }
    m_animationMap.Clear();
    m_weightsMap.Clear();
    m_coordSystemMap.Clear();
    m_mirrorMap.Clear();
    Reset();
}

void AtDatabaseLoader::FreeNames() {
    FreeList(s_pixelProcessNames, m_pixelProcessCount);
    FreeList(s_vertexProcessNames, m_vertexProcessCount);
    FreeList(s_imageNames, m_imageCount);
    FreeList(s_fontNames, m_fontCount);
    FreeList(s_matAnimationNames, m_matAnimationCount);
    FreeList(s_morphAnimationNames, m_morphAnimationCount);
    FreeList(s_lightAnimationNames, m_lightAnimationCount);
    FreeList(s_matAssignmentNames, m_matAssignmentCount);
    FreeList(s_animationNames, m_animationCount);
    FreeList(s_weightsNames, m_weightsCount);
    FreeList(s_coordSystemNames, m_coordSystemCount);
    FreeList(s_mirrorNames, m_mirrorCount);
    FreeList(s_lightNames, m_lightCount);
    FreeList(s_meshNames, m_meshCount);
    FreeList(s_materialNames, m_materialCount);
    FreeList(s_shaderNames, m_shaderCount);
    FreeList(s_textureNames, m_textureCount);
}

void AtDatabaseLoader::MakeCoordSystems() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_coordSystems = new AtCoordSysDatabase[m_coordSystemCount];
    AtPopHeap();
    if (m_coordSystems == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_coordSystemMap.Init(m_coordSystemCount);
    m_owner->AddCoordSystems(&m_coordSystemMap);
}

void AtDatabaseLoader::MakeAnimations() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_animations = (AtAnimationDatabase**)operator new(m_animationCount * sizeof(AtAnimationDatabase*));
    AtPopHeap();
    if (m_animations == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < m_animationCount; i++) {
        m_animations[i] = m_owner->m_animationDevice->NewDatabase();
    }
    m_animationMap.Init(m_animationCount);
    m_owner->AddAnimations(&m_animationMap);
}

void AtDatabaseLoader::MakeMatAssignments() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_matAssignments = new AtMatAssignmentDatabase[m_matAssignmentCount];
    AtPopHeap();
    if (m_matAssignments == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtDatabaseLoader::MakeMatAnimations() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_matAnimations = new AtMatAnimationDatabase[m_matAnimationCount];
    AtPopHeap();
    if (m_matAnimations == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

// The seam. The original makes arrays of classes not recovered (vftables 0x00273154, 0x00273164, 0x00273274, 0x0027337C) and loads them from .AKM, .AKW, .AGO and .AGH files. No .AGD on the disc lists any.
void AtDatabaseLoader::MakeMirrors() {
    AtFatal("AtDatabaseLoader: CoordSysMirrorDatabase is not recovered (0x00019D30, .AKM)", 0, 0, 0);
}

void AtDatabaseLoader::MakeWeights() {
    AtFatal("AtDatabaseLoader: WeightsDatabase is not recovered (0x00019E70, .AKW)", 0, 0, 0);
}

void AtDatabaseLoader::MakeMorphAnimations() {
    AtFatal("AtDatabaseLoader: MorphAnimationDatabase is not recovered (0x0001A1F0, .AGO)", 0, 0, 0);
}

void AtDatabaseLoader::MakeLightAnimations() {
    AtFatal("AtDatabaseLoader: LightAnimationDatabase is not recovered (0x0001A310, .AGH)", 0, 0, 0);
}

void AtDatabaseLoader::LoadDatabases(AtMaterialOwner* owner, int mode, const char* prefix, int freeNames) {
    const unsigned int prefixRoom = prefix != 0 ? LengthOf(prefix) + 2 : 0;
    for (unsigned int i = 0; i < m_vertexProcessCount; i++) {
        VertexProcessAt(i)->Load(owner, s_vertexProcessNames[i], mode);
    }
    for (unsigned int i = 0; i < m_pixelProcessCount; i++) {
        PixelProcessAt(i)->Load(owner, s_pixelProcessNames[i], mode);
    }
    for (unsigned int i = 0; i < m_textureCount; i++) {
        TextureAt(i)->Load(owner, s_textureNames[i], mode);
    }
    for (unsigned int i = 0; i < m_shaderCount; i++) {
        ShaderAt(i)->Load(owner, s_shaderNames[i], mode);
    }
    for (unsigned int i = 0; i < m_materialCount; i++) {
        MaterialAt(i)->Load(owner, s_materialNames[i], mode);
    }
    for (unsigned int i = 0; i < m_meshCount; i++) {
        MeshAt(i)->Load(owner, s_meshNames[i], mode);
    }
    for (unsigned int i = 0; i < m_lightCount; i++) {
        LightAt(i)->Load(owner, s_lightNames[i], mode);
    }
    char leaf[8];
    for (unsigned int i = 0; i < m_coordSystemCount; i++) {
        AtCoordSysDatabase* database = &m_coordSystems[i];
        database->Load(owner->MatrixDevice(), s_coordSystemNames[i], mode);
        AtLeafName(s_coordSystemNames[i], leaf);
        m_coordSystemMap.Insert(leaf, database);
    }
    for (unsigned int i = 0; i < m_animationCount; i++) {
        AtAnimationDatabase* database = m_animations[i];
        database->Load(owner->m_animationDevice, s_animationNames[i], mode);
        AtLeafName(s_animationNames[i], leaf);
        m_animationMap.Insert(leaf, database);
    }
    for (unsigned int i = 0; i < m_matAssignmentCount; i++) {
        m_matAssignments[i].Load(owner, s_matAssignmentNames[i], mode);
    }
    for (unsigned int i = 0; i < m_fontCount; i++) {
        FontAt(i)->Load(owner, s_fontNames[i], mode);
    }
    for (unsigned int i = 0; i < m_imageCount; i++) {
        if (prefix != 0) {
            if (AtIoTokenFile::s_heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(AtIoTokenFile::s_heap);
            char* joined = (char*)operator new(LengthOf(s_imageNames[i]) + prefixRoom + 1);
            AtPopHeap();
            if (joined == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            AtJoinPrefix(joined, s_imageNames[i], prefix);
            ImageAt(i)->Load(owner, joined, mode);
            operator delete(joined);
        } else {
            ImageAt(i)->Load(owner, s_imageNames[i], mode);
        }
    }
    for (unsigned int i = 0; i < m_matAnimationCount; i++) {
        m_matAnimations[i].Load(owner, s_matAnimationNames[i], mode);
    }
    if (freeNames != 0) {
        FreeNames();
    }
    for (unsigned int i = 0; i < m_loaderCount; i++) {
        LoaderAt(i)->Load(owner, m_loaderNames[i], mode, 0, 1);
    }
    if (m_loaderNames != 0) {
        AtFreeNameList(m_loaderNames, m_loaderCount);
        m_loaderNames = 0;
    }
}

void AtDatabaseLoader::Load(AtMaterialOwner* owner, const char* name, int mode, const char* prefix, int inDirectory) {
    AtIoTokenFile file;
    if (m_owner != 0) {
        Unload();
    }
    const int status = file.Open(name, "AGD", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGD");
    }
    m_owner = owner;
    AtIoPath* path = 0;
    if (inDirectory != 0) {
        path = &file.m_file.m_fs->m_path;
        path->PushFromFile(file.m_file.m_file->m_path);
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd) {
        switch (token) {
        case kTextureDatabase:
            ReadCount(tokenizer, m_textureCount);
            MakeTextures();
            ReadNames(tokenizer, m_textureCount, s_textureNames);
            break;
        case kShaderDatabase:
            ReadCount(tokenizer, m_shaderCount);
            MakeShaders();
            ReadNames(tokenizer, m_shaderCount, s_shaderNames);
            break;
        case kMaterialDatabase:
            ReadCount(tokenizer, m_materialCount);
            MakeMaterials();
            ReadNames(tokenizer, m_materialCount, s_materialNames);
            break;
        case kMeshDatabase:
            ReadCount(tokenizer, m_meshCount);
            MakeMeshes();
            ReadNames(tokenizer, m_meshCount, s_meshNames);
            break;
        case kLightDatabase:
            ReadCount(tokenizer, m_lightCount);
            MakeLights();
            ReadNames(tokenizer, m_lightCount, s_lightNames);
            break;
        case kCoordSysDatabase:
            ReadCount(tokenizer, m_coordSystemCount);
            MakeCoordSystems();
            ReadNames(tokenizer, m_coordSystemCount, s_coordSystemNames);
            break;
        case kCoordSysMirrorDatabase:
            ReadCount(tokenizer, m_mirrorCount);
            MakeMirrors();
            ReadNames(tokenizer, m_mirrorCount, s_mirrorNames);
            break;
        case kWeightsDatabase:
            ReadCount(tokenizer, m_weightsCount);
            MakeWeights();
            ReadNames(tokenizer, m_weightsCount, s_weightsNames);
            break;
        case kAnimationDatabase:
            ReadCount(tokenizer, m_animationCount);
            MakeAnimations();
            ReadNames(tokenizer, m_animationCount, s_animationNames);
            break;
        case kMatAssignmentDatabase:
            ReadCount(tokenizer, m_matAssignmentCount);
            MakeMatAssignments();
            ReadNames(tokenizer, m_matAssignmentCount, s_matAssignmentNames);
            break;
        case kMatAnimationDatabase:
            ReadCount(tokenizer, m_matAnimationCount);
            MakeMatAnimations();
            ReadNames(tokenizer, m_matAnimationCount, s_matAnimationNames);
            break;
        case kMorphAnimationDatabase:
            ReadCount(tokenizer, m_morphAnimationCount);
            MakeMorphAnimations();
            ReadNames(tokenizer, m_morphAnimationCount, s_morphAnimationNames);
            break;
        case kLightAnimationDatabase:
            ReadCount(tokenizer, m_lightAnimationCount);
            MakeLightAnimations();
            ReadNames(tokenizer, m_lightAnimationCount, s_lightAnimationNames);
            break;
        case kFontDatabase:
            ReadCount(tokenizer, m_fontCount);
            MakeFonts();
            ReadNames(tokenizer, m_fontCount, s_fontNames);
            break;
        case kImageDatabase:
            ReadCount(tokenizer, m_imageCount);
            MakeImages();
            ReadNames(tokenizer, m_imageCount, s_imageNames);
            break;
        case kVertexProcessDatabase:
            ReadCount(tokenizer, m_vertexProcessCount);
            MakeVertexProcesses();
            ReadNames(tokenizer, m_vertexProcessCount, s_vertexProcessNames);
            break;
        case kPixelProcessDatabase:
            ReadCount(tokenizer, m_pixelProcessCount);
            MakePixelProcesses();
            ReadNames(tokenizer, m_pixelProcessCount, s_pixelProcessNames);
            break;
        case kDatabaseLoader:
            ReadCount(tokenizer, m_loaderCount);
            MakeLoaders();
            ReadNames(tokenizer, m_loaderCount, m_loaderNames);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        tokenizer->ExpectRightCurly();
        token = tokenizer->Next();
    }
    file.Close();
    LoadDatabases(owner, mode, prefix, 1);
    if (inDirectory != 0) {
        path->Pop(0);
    }
}

// ---- the owner's tables ------------------------------------------------------------------------------------------------

void AtMaterialOwner::AddMeshes(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_meshSpaces.Add(ns, map);
        return;
    }
    m_meshes.Add(map);
}

void AtMaterialOwner::AddLightAnimations(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_lightAnimationSpaces.Add(ns, map);
        return;
    }
    m_lightAnimations.Add(map);
}

void AtMaterialOwner::AddMorphAnimations(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_morphAnimationSpaces.Add(ns, map);
        return;
    }
    m_morphAnimations.Add(map);
}

void AtMaterialOwner::AddCoordSystems(GmNameMap* map) { m_coordSystems.Add(map); }
void AtMaterialOwner::AddMirrors(GmNameMap* map) { m_mirrors.Add(map); }
void AtMaterialOwner::AddWeights(GmNameMap* map) { m_weights.Add(map); }
void AtMaterialOwner::AddAnimations(GmNameMap* map) { m_animations.Add(map); }
void AtMaterialOwner::AddVertexProcesses(GmNameMap* map) { m_vertexProcesses.Add(map); }
void AtMaterialOwner::AddPixelProcesses(GmNameMap* map) { m_pixelProcesses.Add(map); }
