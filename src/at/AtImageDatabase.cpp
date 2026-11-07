// See include/at/AtImageDatabase.h.
#include "at/AtImageDatabase.h"

#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

unsigned int AtImageDatabase::s_demandLoadCount;
void* AtImageDatabase::s_heap;
unsigned int AtFontDatabase::s_demandLoadCount;
void* AtFontDatabase::s_heap;

// 0x002B0838. Ids from 0x3C on are the items' words.
const AtIoKeyword AtImageDatabase::s_keywords[AtImageDatabase::kKeywordCount] = {
    {0x39, "Image"}, {0x3A, "NameSpace"}, {0x3B, "MatAssignment"}, {0x3C, "TransparentColor"}, {0x3D, "ColorValueRectangle"}, {0x3E, "BmpSource"},
    {0x3F, "TgaSource"}, {0x40, "DdsSource"}, {0x41, "DemandLoad"}, {0x42, "Tm2Source"}, {0x43, "TplSource"}, {0x44, "PntSource"},
    {0x46, "PlatformSource"}, {0x45, "MpegMacroBlocks"}, {0x47, "FlipTextureHorizontally"}, {0x48, "FlipTextureVertically"}, {0x49, "InfrequentModify"}, {0x4A, "FrequentModify"},
};

// 0x002B08E0. Ids from 0x3E on are the items' words.
const AtIoKeyword AtFontDatabase::s_keywords[AtFontDatabase::kKeywordCount] = {
    {0x39, "Font"}, {0x3A, "NameSpace"}, {0x3B, "MatAssignment"}, {0x3C, "Font2D"}, {0x3D, "Font3D"}, {0x3E, "Kerning"}, {0x49, "Leading"},
    {0x4A, "Scale"}, {0x3F, "TransparentColor"}, {0x40, "CharacterSet"}, {0x41, "ColorValueRectangle"}, {0x42, "BmpSource"}, {0x43, "TgaSource"},
    {0x46, "Tm2Source"}, {0x47, "TplSource"}, {0x44, "ColorRectangleCharX"}, {0x45, "ColorRectangleCharY"}, {0x48, "DemandLoad"}, {0x4B, "InfrequentModify"}, {0x4C, "FrequentModify"},
};

namespace {

enum ImageWord { kImage = 0x39, kImageNameSpace = 0x3A, kImageMaterials = 0x3B };
enum FontWord { kFont = 0x39, kFontNameSpace = 0x3A, kFontMaterials = 0x3B, kFont2D = 0x3C, kFont3D = 0x3D };

// Codes passed to the tokenizer's Error; the words for them are inferred from where they are used.
enum Problem { kListAlreadyGiven = 0x27, kOutOfRange = 0x23 };

// A copy of a path on a heap, as the end of both Loads; the original has it inline.
char* KeepPath(const char* path, void* heap) {
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    unsigned int length = 0;
    while (path[length] != 0) {
        length++;
    }
    char* copy = (char*)operator new(length + 1);
    AtPopHeap();
    if (copy == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i <= length; i++) {
        copy[i] = path[i];
    }
    return copy;
}

unsigned int ReadBracketedInt(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectLeftBracket();
    return (unsigned int)tokenizer->ExpectInt();
}

}  // namespace

// ---- images -----------------------------------------------------------------------------------------------------------

void AtImageDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtImageDatabase::AtImageDatabase() {
    m_count = 0;
    m_fileSystem = 0;
    m_directory = 0;
}

int AtImageDatabase::Slot5() {
    return 0;
}

void AtImageDatabase::Clear() {
    m_materials.~AtMatAssignment();
    if (m_directory != 0) {
        operator delete(m_directory);
    }
    GmNameMap::Clear();
    m_count = 0;
    m_fileSystem = 0;
    m_directory = 0;
}

AtImageDatabase::~AtImageDatabase() {
    AtImageDatabase::Clear();
}

void AtImageDatabase::PushDirectory() {
    if (m_fileSystem != 0 && m_directory != 0) {
        m_fileSystem->m_path.PushFromFile(m_directory);
    }
}

void AtImageDatabase::PopDirectory() {
    if (m_fileSystem != 0 && m_directory != 0) {
        m_fileSystem->m_path.Pop(0);
    }
}

void AtImageDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kImageNameSpace) {
        tokenizer->ExpectName(space);
        token = tokenizer->Next();
        ns = space;
    }
    s_demandLoadCount = 0;
    do {
        if (token == kImage) {
            if (m_count > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_count = ReadBracketedInt(tokenizer);
            if (m_count == 0) {
                tokenizer->Error(kOutOfRange);
            }
            tokenizer->ExpectRightBracket();
            Init(m_count);
            owner->AddImages(this, ns);
            OnReady(owner);
            tokenizer->ExpectLeftCurly();
            MakeItems();
            for (unsigned int i = 0; i < m_count; i++) {
                tokenizer->ExpectToken(kImage);
                char name[8];
                tokenizer->ExpectName(name);
                AtImageItem* item = ImageAt(i);
                Insert(name, item);
                item->Read(owner, this, name, tokenizer);
            }
            tokenizer->ExpectRightCurly();
        } else if (token == kImageMaterials) {
            m_materials.Read(owner, tokenizer, ns);
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
        token = tokenizer->Next();
    } while (token != AtIoTokenizer::kEnd);
    for (unsigned int i = 0; i < m_count; i++) {
        AtImageItem* item = ImageAt(i);
        if ((item->m_flags & AtImageItem::kDemandLoad) != 0) {
            s_demandLoadCount++;
        } else {
            item->OnLoaded(owner);
        }
    }
}

void AtImageDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGI", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGI");
    }
    m_fileSystem = file.m_file.m_fs;
    m_fileSystem->m_path.PushFromFile(file.m_file.m_file->m_path);
    Read(owner, file.m_tokenizer);
    if (s_demandLoadCount > 0) {
        m_directory = KeepPath(file.m_file.m_file->m_path, s_heap);
    }
    m_fileSystem->m_path.Pop(0);
    file.Close();
}

// ---- fonts ------------------------------------------------------------------------------------------------------------

void AtFontDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtFontDatabase::AtFontDatabase() {
    m_count = 0;
    m_fileSystem = 0;
    m_directory = 0;
}

int AtFontDatabase::Slot4() {
    return 0;
}

void AtFontDatabase::Slot5(int, int) {
}

int AtFontDatabase::Slot7() {
    return 0;
}

void AtFontDatabase::Clear() {
    m_materials.~AtMatAssignment();
    if (m_directory != 0) {
        operator delete(m_directory);
    }
    m_count = 0;
    m_fileSystem = 0;
    m_directory = 0;
    GmNameMap::Clear();
}

AtFontDatabase::~AtFontDatabase() {
    AtFontDatabase::Clear();
}

void AtFontDatabase::PushDirectory() {
    if (m_fileSystem != 0 && m_directory != 0) {
        m_fileSystem->m_path.PushFromFile(m_directory);
    }
}

void AtFontDatabase::PopDirectory() {
    if (m_fileSystem != 0 && m_directory != 0) {
        m_fileSystem->m_path.Pop(0);
    }
}

void AtFontDatabase::Allocate(AtMaterialOwner* owner, unsigned int count, const char* ns) {
    if (m_count > 0) {
        Clear();
    }
    m_count = count;
    Init(count);
    MakeFonts2D();
    owner->AddFonts(this, ns);
}

void AtFontDatabase::Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Clear();
    }
    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);
    int token = tokenizer->Next();
    const char* ns = 0;
    char space[8];
    if (token == kFontNameSpace) {
        tokenizer->ExpectName(space);
        tokenizer->ExpectToken(kFont);
        ns = space;
    } else if (token != kFont) {
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
    }
    const unsigned int total = ReadBracketedInt(tokenizer);
    tokenizer->ExpectRightBracket();
    Init(total);
    owner->AddFonts(this, ns);
    OnReady(owner);
    s_demandLoadCount = 0;
    for (token = tokenizer->Next(); token != AtIoTokenizer::kEnd; token = tokenizer->Next()) {
        if (token == kFontMaterials) {
            m_materials.Read(owner, tokenizer, ns);
        } else if (token == kFont2D) {
            if (m_count > 0) {
                tokenizer->Error(kListAlreadyGiven);
            }
            m_count = ReadBracketedInt(tokenizer);
            if (m_count == 0 || m_count > total) {
                tokenizer->Error(kOutOfRange);
            }
            tokenizer->ExpectRightBracket();
            tokenizer->ExpectLeftCurly();
            MakeFonts2D();
            for (unsigned int i = 0; i < m_count; i++) {
                tokenizer->ExpectToken(kFont2D);
                char name[8];
                tokenizer->ExpectName(name);
                AtFontItem* item = Font2DAt(i);
                Insert(name, item);
                item->Read(owner, this, name, tokenizer);
            }
            tokenizer->ExpectRightCurly();
        } else if (token == kFont3D) {
            AtFatal("Atlas Graphics - 3D fonts not supported", 0, 0, 0);
        } else {
            tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        }
    }
    for (unsigned int i = 0; i < m_count; i++) {
        AtFontItem* item = Font2DAt(i);
        if ((item->m_flags & AtFontItem::kDemandLoad) != 0) {
            s_demandLoadCount++;
        } else {
            item->OnLoaded(owner);
        }
    }
}

void AtFontDatabase::Load(AtMaterialOwner* owner, const char* name, int) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AGF", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AGF");
    }
    m_fileSystem = file.m_file.m_fs;
    m_fileSystem->m_path.PushFromFile(file.m_file.m_file->m_path);
    Read(owner, file.m_tokenizer);
    if (s_demandLoadCount > 0) {
        m_directory = KeepPath(file.m_file.m_file->m_path, s_heap);
    }
    m_fileSystem->m_path.Pop(0);
    file.Close();
}
