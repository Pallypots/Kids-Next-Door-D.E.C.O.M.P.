// See include/at/AtFlashText.h.
#include "at/AtFlashText.h"

#include "AtArray.h"
#include "at/AtImageDatabase.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
int AtFormat(char* out, const char* format, ...);
extern "C" {
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
}

namespace {

enum Word { kNameSpace = 0x39, kText = 0x3A, kString = 0x3B, kFont2D = 0x3C, kTextFormat = 0x3D, kVariable = 0x3E };

// The report a text gives for a word it does not know; AtIoTokenizer::ErrorText has the text.
const int kBadField = 0x27;

// At 0x00271280.
const AtIoKeyword s_keywords[] = {
    {kNameSpace, "NameSpace"},
    {kText, "Text"},
    {kString, "String"},
    {kFont2D, "Font2D"},
    {kTextFormat, "TextFormat"},
    {kVariable, "Variable"},
};

void* TableHeap(AtFlashResourceTable* table) {
    return table->m_heap != 0 ? table->m_heap : AtFlashResourceTable::s_defaultHeap;
}

// The directory "Text" below the current one, pushed on the top file system's path; the path is returned for the pop.
AtIoPath* PushText() {
    AtIoPath& path = AtIoFileRef::s_context->Top()->m_path;
    char directory[0x80];
    strcpy(directory, path.Current());
    strcat(directory, "Text");
    path.Push(directory);
    return &path;
}

}  // namespace

// ---- a text ---------------------------------------------------------------------------------------------------------

AtFlashText::AtFlashText() {
    ResetLinks();
    m_read = 0;
    m_unknown20 = 0;
    m_unknown28 = 0;
    m_variable = 0;
    m_unknown34 = 0;
    m_unknown40 = 0;
    m_unknown48 = 0;
    m_format = -1;
    ((int*)m_string)[0] = 0;
    ((int*)m_string)[1] = 0;
    ((int*)m_font)[0] = 0;
    ((int*)m_font)[1] = 0;
    m_heap = 0;
}

AtFlashText::~AtFlashText() {
    Reset();
}

void AtFlashText::Reset() {
    if (m_read == 0) {
        return;
    }
    if (m_variable != 0) {
        operator delete(m_variable);
        m_variable = 0;
    }
    ResetLinks();
    ResetLinks();
    m_read = 0;
    m_unknown20 = 0;
    m_unknown28 = 0;
    m_variable = 0;
    m_unknown34 = 0;
    m_unknown40 = 0;
    m_unknown48 = 0;
    m_format = -1;
    ((int*)m_string)[0] = 0;
    ((int*)m_string)[1] = 0;
    ((int*)m_font)[0] = 0;
    ((int*)m_font)[1] = 0;
    m_heap = 0;
}

void AtFlashText::Read(AtIoTokenizer* tokenizer) {
    ReadData(tokenizer);
    m_read = 1;
}

void AtFlashText::ReadData(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectToken(kText);
    m_index = tokenizer->ExpectInt();
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        switch (token) {
        case kString:
            tokenizer->ExpectName(m_string);
            break;
        case kFont2D:
            tokenizer->ExpectName(m_font);
            break;
        case kTextFormat:
            m_format = tokenizer->ExpectInt();
            break;
        case kVariable:
            tokenizer->ExpectStringCopy(&m_variable);
            break;
        default:
            tokenizer->Error(kBadField);
            break;
        }
    }
    m_read = 1;
}

void AtFlashText::Release(AtFlashResourceTable* formats, AtFlashFontTable* fonts) {
    formats->Release((unsigned int)m_format);
    m_unknown48 = 0;
    if (m_unknown28 == 0) {
        fonts->ReleaseFont(m_font);
        m_unknown40 = 0;
    }
    m_unknown28 = 0;
    m_unknown34 = 0;
    m_unknown20 = 0;
}

// ---- the fonts --------------------------------------------------------------------------------------------------------

AtFlashFontTable::AtFlashFontTable() {
    m_loaded = 0;
    m_fonts = 0;
    m_fontLoader = 0;
    m_owner = 0;
    m_factory = 0;
    m_refs = 0;
    m_refCount = 0;
    m_heap = 0;
}

AtFlashFontTable::~AtFlashFontTable() {
    ClearFonts();
}

void AtFlashFontTable::ClearFonts() {
    if (m_loaded == 0) {
        return;
    }
    if (m_refs != 0) {
        for (unsigned int i = 0; i < m_refCount; i++) {
            if (m_refs[i].m_references > 0) {
                AtFatal("Error: Font reference exists. Unable to release.", 0, 0, 0);
            }
        }
        operator delete(m_refs);
        m_refs = 0;
    }
    if (m_fonts != 0) {
        m_factory->FreeFontDatabase(m_fonts);
        m_fonts = 0;
    }
    if (m_fontLoader != 0) {
        m_factory->FreeFontLoader(m_fontLoader);
        m_fontLoader = 0;
    }
    Clear();
    m_loaded = 0;
    m_fonts = 0;
    m_fontLoader = 0;
    m_owner = 0;
    m_factory = 0;
    m_refs = 0;
    m_refCount = 0;
    m_heap = 0;
}

void AtFlashFontTable::Load(const char* name, int mode, AtMaterialOwner* owner) {
    if (m_loaded != 0) {
        ClearFonts();
    }
    Init();
    AtIoFileSystem* fs = AtIoFileRef::s_context->Top();
    AtIoPath& path = fs->m_path;
    char directory[0x80];
    strcpy(directory, path.Current());
    strcat(directory, "Text");
    path.Push(directory);
    char file[0x10];
    const char extension[4] = {'A', 'G', 'F', 0};
    AtFormat(file, "%s.%s", name, extension);
    if (fs->IsFileMissing(file) == 0) {
        m_owner = owner;
        m_factory = owner->GraphicsFactory();
        m_fontLoader = m_factory->MakeFontLoader();
        m_fontLoader->Load(m_owner, name, mode);
        m_fonts = m_factory->MakeFontDatabase();
        m_fonts->Load(m_owner, name, 0);
        m_refCount = m_fonts->m_count;
        void* heap = TableHeap(this);
        if (heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(heap);
        m_refs = (FontRef*)operator new(m_refCount * sizeof(FontRef));
        AtPopHeap();
        if (m_refs == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < m_refCount; i++) {
            m_refs[i].m_font = 0;
            m_refs[i].m_references = 0;
        }
    }
    path.Pop(0);
    m_loaded = 1;
}

int AtFlashFontTable::ReleaseFont(const char* name) {
    AtFont* font = (AtFont*)m_fonts->Find(name);
    if (font == 0) {
        return 0;
    }
    if ((font->m_flags & AtFont::kDemandLoad) != 0) {
        unsigned int found = 0;
        for (unsigned int i = 0; i < m_refCount; i++) {
            if (m_refs[i].m_font == font) {
                found = i;
                break;
            }
        }
        m_refs[found].m_references--;
        if (m_refs[found].m_references == 0) {
            font->FreeData();
            m_refs[found].m_font = 0;
        }
    }
    return 1;
}

// ---- the formats' load --------------------------------------------------------------------------------------------------

void AtFlashTextFormatTable::Load(const char* name, int) {
    if (m_loaded != 0) {
        ReleaseFormats();
    }
    Init();
    AtIoPath* path = PushText();
    Read(name);
    path->Pop(0);
    m_loaded = 1;
}

// ---- the texts ---------------------------------------------------------------------------------------------------------

AtFlashTextTable::AtFlashTextTable() {
    m_loaded = 0;
    m_heap = 0;
}

AtFlashTextTable::~AtFlashTextTable() {
    ClearTexts();
}

void AtFlashTextTable::Allocate(unsigned int count) {
    AtFlashResourceTable::Allocate(count);
    void* heap = TableHeap(this);
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    AtFlashText* texts = AtNewArray<AtFlashText>(count);
    AtPopHeap();
    if (texts == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        m_items[i] = &texts[i];
        texts[i].m_heap = TableHeap(this);
    }
}

void AtFlashTextTable::Release(unsigned int index) {
    AtFlashText* text = (AtFlashText*)m_items[index];
    if (text->m_references == 1) {
        text->Release(&m_formats, &m_fonts);
    }
    AtFlashResourceTable::Release(index);
}

void AtFlashTextTable::ClearTexts() {
    if (m_loaded == 0) {
        return;
    }
    for (unsigned int i = 0; i < m_groupCount; i++) {
        RecycleGroup(1);
    }
    m_strings.Clear();
    m_fonts.ClearFonts();
    m_formats.ReleaseFormats();
    if (m_items != 0 && m_items[0] != 0) {
        AtDeleteArray(static_cast<AtFlashText*>(m_items[0]));
        for (unsigned int i = 0; i < m_itemCount; i++) {
            m_items[i] = 0;
        }
    }
    Clear();
    m_loaded = 0;
    m_heap = 0;
}

void AtFlashTextTable::ReadFile(const char* name) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AFT", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AFT");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);
    tokenizer->ExpectToken(kNameSpace);
    char* space;
    tokenizer->ExpectStringCopy(&space);
    AtCopyName(space, m_name);
    operator delete(space);
    tokenizer->ExpectToken(kText);
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
}

void AtFlashTextTable::Load(const char* name, int mode, AtMaterialOwner* owner, int) {
    if (m_loaded != 0) {
        ClearTexts();
    }
    Init();
    m_formats.m_heap = TableHeap(this);
    m_formats.Load(name, mode);
    m_fonts.m_heap = TableHeap(this);
    m_fonts.Load("Font", mode, owner);
    AtIoPath* path = PushText();
    ReadFile(name);
    m_strings.Load(AtIoFileRef::s_context, name, mode);
    path->Pop(0);
    m_loaded = 1;
}
