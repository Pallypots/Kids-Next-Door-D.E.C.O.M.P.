// See include/at/AtMaterialDatabase.h.
#include "at/AtMaterialDatabase.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtMaterialDatabase::s_heap;

namespace {

enum Word {
    kNameSpace = 57, kProgShaderTexture = 58, kStagedShaderTexture = 59, kProgShaderTextureWithNameSpace = 60, kStagedShaderTextureWithNameSpace = 61,
    kDefault = 62, kMaterial = 63
};

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kListIsEmpty = 0x23, kTextureNotFound = 0x25 };

// The original has this inline at each use. The heap is not cleared, and Read does not clear the lists.
void** NewList(unsigned int count) {
    if (AtMaterialDatabase::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtMaterialDatabase::s_heap);
    void** list = (void**)operator new(count * sizeof(void*));
    AtPopHeap();
    if (list == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return list;
}

void** NewClearedList(unsigned int count) {
    void** list = NewList(count);
    for (unsigned int i = 0; i < count; i++) {
        list[i] = 0;
    }
    return list;
}

}  // namespace

void AtMaterialDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtMaterialDatabase::AtMaterialDatabase() {
    m_count = 0;
    m_stageCapacity = 0;
    m_stageUsed = 0;
    m_stages = 0;
    m_textureCapacity = 0;
    m_textureUsed = 0;
    m_textures = 0;
    m_unknown30 = 0;
}

AtMaterialDatabase::~AtMaterialDatabase() {
    if (m_stages != 0) {
        operator delete(m_stages);
    }
    if (m_textures != 0) {
        operator delete(m_textures);
    }
    GmNameMap::Clear();
    m_count = 0;
    m_stageCapacity = 0;
    m_stageUsed = 0;
    m_stages = 0;
    m_textureCapacity = 0;
    m_textureUsed = 0;
    m_textures = 0;
    m_unknown30 = 0;
}

void AtMaterialDatabase::Clear() {
    if (m_stages != 0) {
        operator delete(m_stages);
    }
    if (m_textures != 0) {
        operator delete(m_textures);
    }
    GmNameMap::Clear();
    m_count = 0;
    m_stageCapacity = 0;
    m_stageUsed = 0;
    m_stages = 0;
    m_textureCapacity = 0;
    m_textureUsed = 0;
    m_textures = 0;
    m_unknown30 = 0;
}

void AtMaterialDatabase::Allocate(AtMaterialOwner* owner, unsigned int stages, unsigned int textures, unsigned int count, const char* ns) {
    if (m_count > 0) {
        Clear();
    }
    m_stageCapacity = stages;
    m_textureCapacity = textures;
    m_count = count;
    Init(count);
    if (m_stageCapacity != 0) {
        m_stages = NewClearedList(m_stageCapacity);
    }
    if (m_textureCapacity != 0) {
        m_textures = NewClearedList(m_textureCapacity);
    }
    OnReady(owner);
    owner->AddMaterials(this, ns);
}

void AtMaterialDatabase::CopyFrom(AtMaterialOwner* owner, AtMaterialDatabase& from, const char* ns) {
    if (m_count > 0) {
        Clear();
    }
    m_stageCapacity = from.m_stageCapacity;
    m_textureCapacity = from.m_textureCapacity;
    m_count = from.m_count;
    Init(m_count);
    if (m_stageCapacity != 0) {
        m_stages = NewClearedList(m_stageCapacity);
    }
    if (m_textureCapacity != 0) {
        m_textures = NewClearedList(m_textureCapacity);
    }
    OnReady(owner);
    for (unsigned int i = 0; i < m_count; i++) {
        AtMaterial* material = from.MaterialAt(i);
        if (material->m_flags & AtMaterial::kLoaded) {
            MaterialAt(i)->Copy(owner, this, *material);
        }
    }
    owner->AddMaterials(this, ns);
}

void AtMaterialDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGM", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGM");
    }
    Read(owner, file.m_tokenizer);
    file.Close();
}

void AtMaterialDatabase::ReadTextures(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, void** list, unsigned int count, const char* ns, int withSpaces) {
    for (unsigned int i = 0; i < count; i++) {
        const int token = tokenizer->Next();
        if (token == AtIoTokenizer::kString) {
            char name[8];
            tokenizer->CopyName(name);
            void* texture;
            if (withSpaces != 0) {
                char space[8];
                tokenizer->ExpectName(space);
                texture = space[0] != 0 ? owner->m_textureSpaces.Find(space, name) : owner->m_textures.Find(name);
            } else {
                texture = ns != 0 && ns[0] != 0 ? owner->m_textureSpaces.Find(ns, name) : owner->m_textures.Find(name);
            }
            list[i] = texture;
            if (texture == 0) {
                tokenizer->Error(kTextureNotFound);
            }
        } else if (token == kDefault) {
            list[i] = 0;
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedString);
        }
    }
    tokenizer->ExpectRightCurly();
}

void AtMaterialDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(AtMaterial::s_keywords, AtMaterial::kKeywordCount, 0);
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }
    while (token != AtIoTokenizer::kEnd) {
        switch (token) {
        case kProgShaderTexture:
        case kProgShaderTextureWithNameSpace:
            if (m_stageCapacity > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_stageCapacity = (unsigned int)tokenizer->ExpectCount();
            if (m_stageCapacity < 1) {
                tokenizer->Error(kListIsEmpty);
            }
            m_stages = NewList(m_stageCapacity);
            ReadTextures(owner, tokenizer, m_stages, m_stageCapacity, ns, token == kProgShaderTextureWithNameSpace);
            break;
        case kStagedShaderTexture:
        case kStagedShaderTextureWithNameSpace:
            if (m_textureCapacity > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_textureCapacity = (unsigned int)tokenizer->ExpectCount();
            if (m_textureCapacity < 1) {
                tokenizer->Error(kListIsEmpty);
            }
            m_textures = NewList(m_textureCapacity);
            ReadTextures(owner, tokenizer, m_textures, m_textureCapacity, ns, token == kStagedShaderTextureWithNameSpace);
            break;
        case kMaterial:
            m_count = (unsigned int)tokenizer->ExpectCount();
            Init(m_count);
            owner->AddMaterials(this, ns);
            OnReady(owner);
            for (unsigned int i = 0; i < m_count; i++) {
                tokenizer->ExpectToken(kMaterial);
                char name[8];
                tokenizer->ExpectName(name);
                AtMaterial* material = MaterialAt(i);
                Insert(name, material);
                material->Read(owner, this, tokenizer, ns);
            }
            tokenizer->ExpectRightCurly();
            break;
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }
}
