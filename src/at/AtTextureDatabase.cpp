// See include/at/AtTextureDatabase.h.
#include "at/AtTextureDatabase.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

unsigned int AtTextureDatabase::s_demandLoadCount;
void* AtTextureDatabase::s_heap;

// 0x002B0230. Ids from 0x40 on are the items' words.
const AtIoKeyword AtTextureDatabase::s_keywords[AtTextureDatabase::kKeywordCount] = {
    {0x39, "NameSpace"}, {0x3A, "Surface2D"}, {0x3B, "Surface3D"}, {0x3C, "Texture"}, {0x3D, "Texture2D"},
    {0x3E, "Texture3D"}, {0x3F, "CubeMapTexture"}, {0x40, "BmpSource"}, {0x41, "TgaSource"}, {0x42, "MipMap"},
    {0x43, "AutoGenMipLevels"}, {0x44, "RenderTarget"}, {0x45, "DepthStencil"}, {0x46, "FrequentModify"}, {0x47, "InfrequentModify"},
    {0x48, "FlipVertically"}, {0x49, "TextureStage"}, {0x4A, "DDSSource"}, {0x52, "PlatformSource"}, {0x4B, "LightDirection"},
    {0x4C, "TransparentColor"}, {0x4D, "DiffuseLightMap"}, {0x4E, "Tm2Source"}, {0x4F, "TplSource"}, {0x50, "PntSource"},
    {0x51, "MpegMacroBlocks"}, {0x53, "FrontBufferSource"}, {0x54, "BackBufferSource"}, {0x56, "ProgSource"}, {0x55, "DemandLoad"},
};

namespace {

enum Word { kNameSpace = 0x39, kSurface2D = 0x3A, kSurface3D = 0x3B, kTexture = 0x3C, kTexture2D = 0x3D, kTexture3D = 0x3E, kCubeMapTexture = 0x3F };

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kListIsEmpty = 0x23 };

enum { kNameSize = 8, kKinds = 3 };

// "[ n ]". An empty list is reported, and read on with.
unsigned int ReadCountInBrackets(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftBracket();
    const unsigned int count = (unsigned int)tokenizer->ExpectInt();
    if (count < 1) {
        tokenizer->Error(kListIsEmpty);
    }
    tokenizer->ExpectRightBracket();
    return count;
}

// The names of one list while it reads. They are on the token file's heap, not the database's, and are not cleared.
char* NewNames(unsigned int count) {
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    char* names = (char*)operator new(count * kNameSize);
    AtPopHeap();
    if (names == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return names;
}

AtTextureItem* ItemAt(AtTextureDatabase* database, int kind, unsigned int index) {
    if (kind == 0) {
        return database->Texture2DAt(index);
    }
    if (kind == 1) {
        return database->Texture3DAt(index);
    }
    return database->CubeMapAt(index);
}

}  // namespace

void AtMaterialOwner::AddTextures(GmNameMap* map, const char* ns) {
    if (ns != 0 && ns[0] != 0) {
        m_textureSpaces.Add(ns, map);
        return;
    }
    m_textures.Add(map);
}

void AtTextureDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtTextureDatabase::AtTextureDatabase() {
    m_surface2DCount = 0;
    m_surface3DCount = 0;
    m_texture2DCount = 0;
    m_texture3DCount = 0;
    m_cubeMapCount = 0;
    m_fileSystem = 0;
    m_directory = 0;
    m_more = 0;
}

AtTextureDatabase::~AtTextureDatabase() {
}

void AtTextureDatabase::Clear() {
    GmNameMap::Clear();
    if (m_directory != 0) {
        operator delete(m_directory);
    }
    m_surface2DCount = 0;
    m_surface3DCount = 0;
    m_texture2DCount = 0;
    m_texture3DCount = 0;
    m_cubeMapCount = 0;
    m_fileSystem = 0;
    m_directory = 0;
    m_more = 0;
}

void AtTextureDatabase::Allocate(AtMaterialOwner* owner, unsigned int surfaces2D, unsigned int surfaces3D, unsigned int textures2D, unsigned int textures3D, unsigned int cubeMaps, const char* ns) {
    if (m_surface2DCount + m_surface3DCount != 0) {
        Clear();
    }
    m_surface2DCount = surfaces2D;
    m_surface3DCount = surfaces3D;
    m_texture2DCount = textures2D;
    m_texture3DCount = textures3D;
    m_cubeMapCount = cubeMaps;
    // If a table is left from before, Init's Clear undoes the five stores above, and nothing below is made: as in the original.
    Init(textures2D + textures3D + cubeMaps);
    OnReady(owner);
    if (m_surface2DCount != 0) {
        MakeSurfaces2D();
    }
    if (m_surface3DCount != 0) {
        MakeSurfaces3D();
    }
    if (m_texture2DCount != 0) {
        MakeTextures2D();
    }
    if (m_texture3DCount != 0) {
        MakeTextures3D();
    }
    if (m_cubeMapCount != 0) {
        MakeCubeMaps();
    }
    owner->AddTextures(this, ns);
}

unsigned int AtTextureDatabase::CountUsedTextures() {
    unsigned int found = 0;
    for (unsigned int i = 0; i < m_texture2DCount; i++) {
        if (Texture2DAt(i)->m_levels != 0) {
            found++;
        }
    }
    if (m_more != 0) {
        return found + m_more->CountUsedTextures();
    }
    return found;
}

AtTextureItem* AtTextureDatabase::FindFreeTexture() {
    AtTextureDatabase* database = this;
    for (;;) {
        for (unsigned int i = 0; i < database->m_texture2DCount; i++) {
            AtTextureItem* item = database->Texture2DAt(i);
            if (item->m_levels == 0) {
                return item;
            }
        }
        if (database->m_more == 0) {
            database->MakeMore();
        }
        database = database->m_more;
    }
}

void AtTextureDatabase::PushDirectory() {
    if (m_fileSystem != 0 && m_directory != 0) {
        m_fileSystem->m_path.PushFromFile(m_directory);
    }
}

void AtTextureDatabase::PopDirectory() {
    if (m_fileSystem != 0 && m_directory != 0) {
        m_fileSystem->m_path.Pop(0);
    }
}

void AtTextureDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGT", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGT");
    }
    m_fileSystem = file.m_file.m_fs;
    m_fileSystem->m_path.PushFromFile(file.m_file.m_file->m_path);
    Read(owner, file.m_tokenizer);
    if (s_demandLoadCount > 0) {
        const char* path = file.m_file.m_file->m_path;
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        unsigned int length = 0;
        while (path[length] != 0) {
            length++;
        }
        m_directory = (char*)operator new(length + 1);
        AtPopHeap();
        if (m_directory == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i <= length; i++) {
            m_directory[i] = path[i];
        }
    }
    m_fileSystem->m_path.Pop(0);
    file.Close();
}

void AtTextureDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_surface2DCount + m_surface3DCount != 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[kNameSize];
    if (token == kNameSpace) {
        tokenizer->ExpectName(space);
        ns = space;
        token = tokenizer->Next();
    }
    OnReady(owner);
    s_demandLoadCount = 0;

    // The value given by the word Texture. A list is reported as given twice only when this is set as well.
    unsigned int nameCount = 0;
    // A list given twice leaves its first names unfreed, and only its last names are looked at below.
    char* names[kKinds] = {0, 0, 0};
    unsigned int* const counts[kKinds] = {&m_texture2DCount, &m_texture3DCount, &m_cubeMapCount};
    while (token != AtIoTokenizer::kEnd) {
        switch (token) {
        case kSurface2D:
            if (m_surface2DCount > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_surface2DCount = ReadCountInBrackets(tokenizer);
            MakeSurfaces2D();
            break;
        case kSurface3D:
            if (m_surface3DCount > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_surface3DCount = ReadCountInBrackets(tokenizer);
            MakeSurfaces3D();
            break;
        case kTexture:
            if (m_entries != 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            nameCount = ReadCountInBrackets(tokenizer);
            Init(nameCount);
            owner->AddTextures(this, ns);
            break;
        case kTexture2D:
        case kTexture3D:
        case kCubeMapTexture: {
            const int kind = token - kTexture2D;
            if (*counts[kind] > 0 && nameCount > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            *counts[kind] = (unsigned int)tokenizer->ExpectCount();
            if (*counts[kind] < 1) {
                tokenizer->Error(kListIsEmpty);
            }
            if (kind == 0) {
                MakeTextures2D();
            } else if (kind == 1) {
                MakeTextures3D();
            } else {
                MakeCubeMaps();
            }
            names[kind] = NewNames(*counts[kind]);
            char* name = names[kind];
            for (unsigned int i = 0; i < *counts[kind]; i++, name += kNameSize) {
                tokenizer->ExpectToken(token);
                tokenizer->ExpectName(name);
                AtTextureItem* item = ItemAt(this, kind, i);
                item->Read(this, name, tokenizer);
                // The owner holds this database too, from the word Texture, so a name given twice in a file is already known the second time.
                void* known = ns != 0 && ns[0] != 0 ? owner->m_textureSpaces.Find(ns, name) : owner->m_textures.Find(name);
                if (known != 0) {
                    name[0] = 0;
                } else {
                    Insert(name, item);
                }
            }
            tokenizer->ExpectRightCurly();
            break;
        }
        default:
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
            break;
        }
        token = tokenizer->Next();
    }

    for (int kind = 0; kind < kKinds; kind++) {
        for (unsigned int i = 0; i < *counts[kind]; i++) {
            if (names[kind][i * kNameSize] != 0) {
                AtTextureItem* item = ItemAt(this, kind, i);
                if (item->m_flags & AtTextureItem::kDemandLoad) {
                    s_demandLoadCount++;
                } else {
                    item->OnLoaded(owner);
                }
            }
        }
    }
    for (int kind = 0; kind < kKinds; kind++) {
        if (names[kind] != 0) {
            operator delete(names[kind]);
        }
    }
}
