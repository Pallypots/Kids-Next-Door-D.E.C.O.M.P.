// See include/at/AtFlashShape.h.
#include "at/AtFlashShape.h"

#include "AtSceneArray.h"
#include "at/AtDatabaseLoader.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName(const char* from, char* to);
void AtFatal(const char* message, int a, int b, int c);
extern "C" {
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
}

namespace {

enum Word { kNameSpace = 0x39, kGraphicsDatabaseLoader = 0x3A, kShape = 0x3B, kRectangle = 0x3C, kMeshAssignment = 0x3D };

// At 0x002710FC.
const AtIoKeyword s_keywords[] = {
    {kNameSpace, "NameSpace"},
    {kGraphicsDatabaseLoader, "GraphicsDatabaseLoader"},
    {kShape, "Shape"},
    {kRectangle, "Rectangle"},
    {kMeshAssignment, "MeshAssignment"},
};

void* TableHeap(AtFlashResourceTable* table) {
    return table->m_heap != 0 ? table->m_heap : AtFlashResourceTable::s_defaultHeap;
}

}  // namespace

// ---- a shape --------------------------------------------------------------------------------------------------------

AtFlashShape::AtFlashShape() {
    ResetLinks();
    m_mesh = -1;
    m_unknown30 = 0;
    m_heap = 0;
}

AtFlashShape::~AtFlashShape() {
    ResetLinks();
    ResetLinks();
    m_mesh = -1;
    m_unknown30 = 0;
    m_heap = 0;
}

void AtFlashShape::Read(AtIoTokenizer* tokenizer) {
    ReadData(tokenizer);
}

void AtFlashShape::ReadData(AtIoTokenizer* tokenizer) {
    char* meshName = 0;
    tokenizer->ExpectToken(kShape);
    m_index = tokenizer->ExpectInt();
    tokenizer->ExpectLeftCurly();
    tokenizer->ExpectToken(kRectangle);
    m_rectangle[0] = tokenizer->ExpectReal();
    m_rectangle[1] = tokenizer->ExpectReal();
    m_rectangle[2] = tokenizer->ExpectReal();
    m_rectangle[3] = tokenizer->ExpectReal();
    tokenizer->ExpectToken(kMeshAssignment);
    m_mesh = tokenizer->ExpectInt();
    tokenizer->ExpectStringCopy(&meshName);
    operator delete(meshName);
    tokenizer->ExpectRightCurly();
}

// ---- the shapes ------------------------------------------------------------------------------------------------------

AtFlashShapeTable::AtFlashShapeTable() {
    m_loaded = 0;
    m_owner = 0;
    m_loader = 0;
    m_textures = 0;
    m_textureCount = 0;
}

AtFlashShapeTable::~AtFlashShapeTable() {
    ClearShapes();
}

void AtFlashShapeTable::Allocate(unsigned int count) {
    AtFlashResourceTable::Allocate(count);
    void* heap = TableHeap(this);
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    AtFlashShape* shapes = AtNewArray<AtFlashShape>(count);
    AtPopHeap();
    if (shapes == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        m_items[i] = &shapes[i];
        shapes[i].m_heap = TableHeap(this);
    }
}

void AtFlashShapeTable::Release(unsigned int) {
    AtFatal("AtFlashShapeTable::Release (0x00012F70) is not recovered", 0, 0, 0);
}

void AtFlashShapeTable::ClearShapes() {
    if (m_loaded == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_groupCount; i++) {
        RecycleGroup(1);
    }
    if (m_textures != 0) {
        for (unsigned int i = 0; i < m_textureCount; i++) {
            if (m_textures[i].m_references > 0) {
                AtFatal("Error: Texture reference exists. Unable to release.", 0, 0, 0);
            }
        }
        operator delete(m_textures);
        m_textures = 0;
    }
    if (m_items != 0 && m_items[0] != 0) {
        AtDeleteArray(static_cast<AtFlashShape*>(m_items[0]));
        for (unsigned int i = 0; i < m_itemCount; i++) {
            m_items[i] = 0;
        }
    }
    if (m_loader != 0) {
        m_loader->Unload();
        m_owner->GraphicsFactory()->FreeLoader(m_loader);
    }
    Clear();
    m_loaded = 0;
    m_owner = 0;
    m_loader = 0;
    m_textures = 0;
    m_textureCount = 0;
}

char* AtFlashShapeTable::ReadFile(const char* name) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AFH", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AFH");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);
    tokenizer->ExpectToken(kNameSpace);
    char* space;
    tokenizer->ExpectStringCopy(&space);
    AtCopyName(space, m_name);
    operator delete(space);
    tokenizer->ExpectToken(kGraphicsDatabaseLoader);
    char* loaderName;
    tokenizer->ExpectStringCopy(&loaderName);
    tokenizer->ExpectToken(kShape);
    tokenizer->ExpectLeftBracket();
    m_itemCount = (unsigned int)tokenizer->ExpectInt();
    tokenizer->ExpectRightBracket();
    tokenizer->ExpectLeftCurly();
    if (m_itemCount > 0) {
        Allocate(m_itemCount);
        for (unsigned int i = 0; i < m_itemCount; i++) {
            m_items[i]->Read(tokenizer);
        }
    }
    tokenizer->ExpectRightCurly();
    file.Close();
    return loaderName;
}

void AtFlashShapeTable::Load(const char* name, int mode, AtFlashApplication* application) {
    if (m_loaded != 0) {
        ClearShapes();
    }
    Init();
    AtIoPath& path = AtIoFileRef::s_context->Top()->m_path;
    char directory[0x80];
    strcpy(directory, path.Current());
    strcat(directory, "Shapes");
    path.Push(directory);
    char* loaderName = ReadFile(name);
    if (m_itemCount > 0) {
        m_owner = application->m_graphics->m_owner;
        m_loader = m_owner->GraphicsFactory()->MakeLoader();
        m_loader->Load(m_owner, loaderName, mode, 0, 1);
        const unsigned int textures = m_loader->m_textureCount;
        if (textures > 0) {
            if (textures != 1) {
                AtFatal("Unexpected number of texture databases!", 0, 0, 0);
            }
            m_textureCount = m_loader->TextureAt(0)->CountUsedTextures();
            void* heap = TableHeap(this);
            if (heap == 0) {
                AtRaise(-2, 0, 0, 0);
            }
            AtPushHeap(heap);
            m_textures = (TextureRef*)operator new(m_textureCount * sizeof(TextureRef));
            AtPopHeap();
            if (m_textures == 0) {
                AtRaise(-4, 0, 0, 0);
            }
            for (unsigned int i = 0; i < m_textureCount; i++) {
                m_textures[i].m_texture = 0;
                m_textures[i].m_references = 0;
            }
        }
    }
    if (loaderName != 0) {
        operator delete(loaderName);
    }
    path.Pop(0);
    m_loaded = 1;
}
