// See include/at/AtSceneDatabaseLoader.h.
#include "at/AtSceneDatabaseLoader.h"

#include "at/AtDatabaseLoader.h"
#include "at/AtIoTokenizer.h"
#include "AtArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

char** AtSceneDatabaseLoader::s_nodeNames;
char** AtSceneDatabaseLoader::s_modifierNames;
char** AtSceneDatabaseLoader::s_emitterNames;
char** AtSceneDatabaseLoader::s_lensFlareNames;
char** AtSceneDatabaseLoader::s_cellGraphNames;
char** AtSceneDatabaseLoader::s_spaceGraphNames;
char** AtSceneDatabaseLoader::s_geometryNames;
char** AtSceneDatabaseLoader::s_stringNames;
char** AtSceneDatabaseLoader::s_imageDrawNames;
char** AtSceneDatabaseLoader::s_textDrawNames;
void* AtSceneDatabaseLoader::s_heap;

// 0x002B13A0.
const AtIoKeyword AtSceneDatabaseLoader::s_keywords[AtSceneDatabaseLoader::kKeywordCount] = {
    {0x39, "NodeDatabase"}, {0x3A, "ModifierDatabase"}, {0x3B, "SpaceGraph"}, {0x3C, "BoundingGeometryDatabase"}, {0x3D, "DatabaseLoader"}, {0x3E, "EmitterDatabase"}, {0x3F, "LensFlareDatabase"},
    {0x40, "CellGraphDatabase"}, {0x41, "StringDatabase"}, {0x42, "ImageDrawDatabase"}, {0x43, "TextDrawDatabase"},
};

namespace {

enum Word {
    kNodeDatabase = 0x39, kModifierDatabase = 0x3A, kSpaceGraph = 0x3B, kBoundingGeometryDatabase = 0x3C, kDatabaseLoader = 0x3D, kEmitterDatabase = 0x3E, kLensFlareDatabase = 0x3F,
    kCellGraphDatabase = 0x40, kStringDatabase = 0x41, kImageDrawDatabase = 0x42, kTextDrawDatabase = 0x43
};

enum Problem { kListAlreadyGiven = 0x27 };

void ReadCount(AtIoTokenizer* tokenizer, unsigned short& count) {
    if (count > 0) {
        tokenizer->Error(kListAlreadyGiven);
    }
    count = (unsigned short)tokenizer->ExpectCount();
}

// The original has this inline at each use, as the .AGD list has. The list is on the token file's heap, cleared, and each name goes on that heap too.
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

// The original has this inline at each use.
void FreeList(char**& names, unsigned short count) {
    if (names != 0) {
        for (unsigned int i = 0; i < count; i++) {
            if (names[i] != 0) {
                operator delete(names[i]);
            }
        }
        operator delete(names);
        names = 0;
    }
}

// Each Make is the original's own function, and all alike but for the type.
template <class T> T* MakeList(unsigned int count) {
    if (AtSceneDatabaseLoader::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtSceneDatabaseLoader::s_heap);
    T* list = AtNewArray<T>(count);
    AtPopHeap();
    if (list == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return list;
}

template <class T> void DeleteList(T* list) {
    if (list != 0) {
        AtDeleteArray(list);
    }
}

}  // namespace

void AtSceneDatabaseLoader::SetHeap(void* heap) {
    s_heap = heap;
}

void AtSceneDatabaseLoader::MakeImageDraws() {
    m_imageDraws = MakeList<AtImageDrawDatabase>(m_imageDrawCount);
}

void AtSceneDatabaseLoader::MakeTextDraws() {
    m_textDraws = MakeList<AtTextDrawDatabase>(m_textDrawCount);
}

void AtSceneDatabaseLoader::MakeStrings() {
    m_strings = MakeList<AtTextTable>(m_stringCount);
}

void AtSceneDatabaseLoader::MakeNodes() {
    m_nodes = MakeList<AtScene>(m_nodeCount);
    m_nodesByName.Init(m_nodeCount);
}

void AtSceneDatabaseLoader::MakeModifiers() {
    m_modifiers = MakeList<AtModifierDatabase>(m_modifierCount);
}

void AtSceneDatabaseLoader::MakeEmitters() {
    m_emitters = MakeList<AtEmitterDatabase>(m_emitterCount);
}

void AtSceneDatabaseLoader::MakeLensFlares() {
    m_lensFlares = MakeList<AtLensFlareDatabase>(m_lensFlareCount);
}

void AtSceneDatabaseLoader::MakeCellGraphs() {
    m_cellGraphs = MakeList<AtCellGraphDatabase>(m_cellGraphCount);
}

void AtSceneDatabaseLoader::MakeSpaceGraphs() {
    m_spaceGraphs = MakeList<AtSpaceGraph>(m_spaceGraphCount);
    m_spaceGraphsByName.Init(m_spaceGraphCount);
    m_owner->AddSpaceGraphs(&m_spaceGraphsByName);
}

void AtSceneDatabaseLoader::MakeGeometry() {
    m_geometry = MakeList<AtBoundingGeometryDatabase>(m_geometryCount);
}

void AtSceneDatabaseLoader::MakeLoaders() {
    m_loaders = MakeList<AtSceneDatabaseLoader>(m_loaderCount);
}

AtScene* AtSceneDatabaseLoader::NodeDatabaseAt(unsigned int index) {
    return m_nodes + index;
}

AtSpaceGraph* AtSceneDatabaseLoader::SpaceGraphAt(unsigned int index) {
    return m_spaceGraphs + index;
}

AtSceneDatabaseLoader* AtSceneDatabaseLoader::LoaderAt(unsigned int index) {
    return m_loaders + index;
}

void AtSceneDatabaseLoader::Zero() {
    m_owner = 0;
    m_nodes = 0;
    m_modifiers = 0;
    m_emitters = 0;
    m_lensFlares = 0;
    m_cellGraphs = 0;
    m_spaceGraphs = 0;
    m_loaders = 0;
    m_geometry = 0;
    m_strings = 0;
    m_imageDraws = 0;
    m_textDraws = 0;
    m_loaderNames = 0;
    m_nodeCount = 0;
    m_modifierCount = 0;
    m_emitterCount = 0;
    m_lensFlareCount = 0;
    m_spaceGraphCount = 0;
    m_loaderCount = 0;
    m_geometryCount = 0;
    m_stringCount = 0;
    m_cellGraphCount = 0;
    m_imageDrawCount = 0;
    m_textDrawCount = 0;
}

AtSceneDatabaseLoader::AtSceneDatabaseLoader() {
    Zero();
}

AtSceneDatabaseLoader::~AtSceneDatabaseLoader() {
    if (m_owner != 0) {
        AtSceneDatabaseLoader::Clear();
    }
}

void AtSceneDatabaseLoader::Clear() {
    DeleteList(m_geometry);
    DeleteList(m_loaders);
    DeleteList(m_cellGraphs);
    DeleteList(m_emitters);
    DeleteList(m_lensFlares);
    DeleteList(m_modifiers);
    DeleteList(m_nodes);
    DeleteList(m_strings);
    DeleteList(m_imageDraws);
    DeleteList(m_textDraws);
    DeleteList(m_spaceGraphs);
    m_spaceGraphsByName.Clear();
    m_nodesByName.Clear();
    Zero();
}

void AtSceneDatabaseLoader::LoadDatabases(AtNameTableOwner* owner, int mode, int freeNames) {
    for (unsigned int i = 0; i < m_stringCount; i++) {
        // The owner's word at +0x0C is an AtIoContext on this path; a sound owner's is its bank loader.
        m_strings[i].Load((AtIoContext*)owner->m_loader, s_stringNames[i], mode);
    }
    for (unsigned int i = 0; i < m_textDrawCount; i++) {
        m_textDraws[i].Load(owner, s_textDrawNames[i], mode);
    }
    for (unsigned int i = 0; i < m_imageDrawCount; i++) {
        m_imageDraws[i].Load(owner, s_imageDrawNames[i], mode);
    }
    for (unsigned int i = 0; i < m_geometryCount; i++) {
        m_geometry[i].Load(owner, s_geometryNames[i], mode);
    }
    for (unsigned int i = 0; i < m_modifierCount; i++) {
        m_modifiers[i].Load(owner, s_modifierNames[i], mode);
    }
    for (unsigned int i = 0; i < m_emitterCount; i++) {
        m_emitters[i].Load(owner, s_emitterNames[i], mode);
    }
    for (unsigned int i = 0; i < m_lensFlareCount; i++) {
        m_lensFlares[i].Load(owner, s_lensFlareNames[i], mode);
    }
    for (unsigned int i = 0; i < m_nodeCount; i++) {
        char leaf[8];
        m_nodes[i].Load(owner, s_nodeNames[i], mode);
        AtLeafName(s_nodeNames[i], leaf);
        m_nodesByName.Insert(leaf, m_nodes + i);
    }
    for (unsigned int i = 0; i < m_cellGraphCount; i++) {
        m_cellGraphs[i].Load(owner, s_cellGraphNames[i], mode);
    }
    for (unsigned int i = 0; i < m_spaceGraphCount; i++) {
        char leaf[8];
        m_spaceGraphs[i].Load(owner, s_spaceGraphNames[i], mode);
        AtLeafName(s_spaceGraphNames[i], leaf);
        m_spaceGraphsByName.Insert(leaf, m_spaceGraphs + i);
    }
    if (freeNames != 0) {
        FreeList(s_cellGraphNames, m_cellGraphCount);
        FreeList(s_emitterNames, m_emitterCount);
        FreeList(s_lensFlareNames, m_lensFlareCount);
        FreeList(s_geometryNames, m_geometryCount);
        FreeList(s_spaceGraphNames, m_spaceGraphCount);
        FreeList(s_modifierNames, m_modifierCount);
        FreeList(s_nodeNames, m_nodeCount);
        FreeList(s_stringNames, m_stringCount);
        FreeList(s_imageDrawNames, m_imageDrawCount);
        FreeList(s_textDrawNames, m_textDrawCount);
    }
    for (unsigned int i = 0; i < m_loaderCount; i++) {
        m_loaders[i].Load(owner, m_loaderNames[i], mode);
    }
}

void AtSceneDatabaseLoader::Load(AtNameTableOwner* owner, const char* name, int mode) {
    AtIoTokenFile file;
    if (m_owner != 0) {
        Clear();
    }
    const int status = file.Open(name, "ASD", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "ASD");
    }
    AtIoFileSystem* fs = file.m_file.m_fs;
    m_owner = owner;
    fs->m_path.PushFromFile(file.m_file.m_file->m_path);
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kEnd) {
        switch (token) {
        case kNodeDatabase:
            ReadCount(tokenizer, m_nodeCount);
            MakeNodes();
            ReadNames(tokenizer, m_nodeCount, s_nodeNames);
            break;
        case kModifierDatabase:
            ReadCount(tokenizer, m_modifierCount);
            MakeModifiers();
            ReadNames(tokenizer, m_modifierCount, s_modifierNames);
            break;
        case kSpaceGraph:
            ReadCount(tokenizer, m_spaceGraphCount);
            MakeSpaceGraphs();
            ReadNames(tokenizer, m_spaceGraphCount, s_spaceGraphNames);
            break;
        case kBoundingGeometryDatabase:
            ReadCount(tokenizer, m_geometryCount);
            MakeGeometry();
            ReadNames(tokenizer, m_geometryCount, s_geometryNames);
            break;
        case kDatabaseLoader:
            ReadCount(tokenizer, m_loaderCount);
            MakeLoaders();
            ReadNames(tokenizer, m_loaderCount, m_loaderNames);
            break;
        case kEmitterDatabase:
            ReadCount(tokenizer, m_emitterCount);
            MakeEmitters();
            ReadNames(tokenizer, m_emitterCount, s_emitterNames);
            break;
        case kLensFlareDatabase:
            ReadCount(tokenizer, m_lensFlareCount);
            MakeLensFlares();
            ReadNames(tokenizer, m_lensFlareCount, s_lensFlareNames);
            break;
        case kCellGraphDatabase:
            ReadCount(tokenizer, m_cellGraphCount);
            MakeCellGraphs();
            ReadNames(tokenizer, m_cellGraphCount, s_cellGraphNames);
            break;
        case kStringDatabase:
            ReadCount(tokenizer, m_stringCount);
            MakeStrings();
            ReadNames(tokenizer, m_stringCount, s_stringNames);
            break;
        case kImageDrawDatabase:
            ReadCount(tokenizer, m_imageDrawCount);
            MakeImageDraws();
            ReadNames(tokenizer, m_imageDrawCount, s_imageDrawNames);
            break;
        case kTextDrawDatabase:
            ReadCount(tokenizer, m_textDrawCount);
            MakeTextDraws();
            ReadNames(tokenizer, m_textDrawCount, s_textDrawNames);
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        tokenizer->ExpectRightCurly();
        token = tokenizer->Next();
    }
    file.Close();
    LoadDatabases(owner, mode, 1);
    fs->m_path.Pop(0);
    FreeList(m_loaderNames, m_loaderCount);
}
