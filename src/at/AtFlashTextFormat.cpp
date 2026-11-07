// See include/at/AtFlashTextFormat.h.
#include "at/AtFlashTextFormat.h"

#include "AtSceneArray.h"
#include "at/AtText.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);
void AtFatal(const char* message, int a, int b, int c);

void* AtFlashResourceTable::s_defaultHeap;

namespace {

enum Word {
    kNameSpace = 0x39, kTextFormat = 0x3A, kBoundsRectangle = 0x3B, kWordWrap = 0x3C, kMultiLine = 0x3D, kPassword = 0x3E, kReadOnly = 0x3F, kNoSelect = 0x40, kBorder = 0x41, kFontHeight = 0x42,
    kTextColor = 0x43, kTextMaxLength = 0x44, kAlignment = 0x45, kLeftMargin = 0x46, kRightMargin = 0x47, kIndent = 0x48, kLeading = 0x49, kWordAlignmentLeft = 0x4A, kWordAlignmentRight = 0x4B,
    kWordAlignmentCenter = 0x4C, kWordAlignmentJustify = 0x4D
};

// The report the fields' reader gives for a word it does not know; AtIoTokenizer::ErrorText has the text.
const int kBadField = 0x27;

// At 0x002727B0.
const AtIoKeyword s_keywords[] = {
    {kNameSpace, "NameSpace"},
    {kTextFormat, "TextFormat"},
    {kBoundsRectangle, "BoundsRectangle"},
    {kWordWrap, "WordWrap"},
    {kMultiLine, "MultiLine"},
    {kPassword, "Password"},
    {kReadOnly, "ReadOnly"},
    {kNoSelect, "NoSelect"},
    {kBorder, "Border"},
    {kFontHeight, "FontHeight"},
    {kTextColor, "TextColor"},
    {kTextMaxLength, "TextMaxLength"},
    {kAlignment, "Alignment"},
    {kLeftMargin, "LeftMargin"},
    {kRightMargin, "RightMargin"},
    {kIndent, "Indent"},
    {kLeading, "Leading"},
    {kWordAlignmentLeft, "AlignmentLeft"},
    {kWordAlignmentRight, "AlignmentRight"},
    {kWordAlignmentCenter, "AlignmentCenter"},
    {kWordAlignmentJustify, "AlignmentJustify"},
};

void* HeapOf(AtFlashResourceTable* table) {
    return table->m_heap != 0 ? table->m_heap : AtFlashResourceTable::s_defaultHeap;
}

}  // namespace

// ---- a resource -----------------------------------------------------------------------------------------------------

AtFlashResource::AtFlashResource() {
    m_index = -1;
    m_references = 0;
    m_unknown14 = 0;
    m_table = 0;
}

AtFlashResource::~AtFlashResource() {
    m_index = -1;
    m_references = 0;
    m_unknown14 = 0;
    m_table = 0;
}

void AtFlashResource::Read(AtIoTokenizer*) {
}

void AtFlashResource::ResetLinks() {
    m_index = -1;
    m_references = 0;
    m_unknown14 = 0;
    m_table = 0;
}

// ---- a text format ----------------------------------------------------------------------------------------------------

AtFlashTextFormat::AtFlashTextFormat() {
    Reset();
}

AtFlashTextFormat::~AtFlashTextFormat() {
    if (m_read != 0) {
        Reset();
    }
}

void AtFlashTextFormat::Reset() {
    ResetLinks();
    m_heap = 0;
    m_read = 0;
    m_wordWrap = 0;
    m_multiLine = 0;
    m_password = 0;
    m_readOnly = 0;
    m_noSelect = 0;
    m_border = 0;
    m_fontHeight = 12.0f;
    m_textMaxLength = -1;
    m_alignment = kAlignmentLeft;
    m_leftMargin = 0.0f;
    m_rightMargin = 0.0f;
    m_indent = 0.0f;
    m_leading = 0.0f;
    m_bounds[0] = 0.0f;
    m_bounds[1] = 0.0f;
    m_bounds[2] = 0.0f;
    m_bounds[3] = 0.0f;
    m_color[0] = 255;
    m_color[1] = 255;
    m_color[2] = 255;
    m_color[3] = 255;
}

void AtFlashTextFormat::Read(AtIoTokenizer* tokenizer) {
    ReadFields(tokenizer);
    m_read = 1;
}

void AtFlashTextFormat::ReadFields(AtIoTokenizer* tokenizer) {
    tokenizer->ExpectToken(kTextFormat);
    m_index = tokenizer->ExpectInt();
    tokenizer->ExpectLeftCurly();
    int token = tokenizer->Next();
    while (token != AtIoTokenizer::kRightCurly) {
        switch (token) {
        case kBoundsRectangle:
            m_bounds[0] = tokenizer->ExpectReal();
            m_bounds[1] = tokenizer->ExpectReal();
            m_bounds[2] = tokenizer->ExpectReal();
            m_bounds[3] = tokenizer->ExpectReal();
            break;
        case kWordWrap:
            m_wordWrap = 1;
            break;
        case kMultiLine:
            m_multiLine = 1;
            break;
        case kPassword:
            m_password = 1;
            break;
        case kReadOnly:
            m_readOnly = 1;
            break;
        case kNoSelect:
            m_noSelect = 1;
            break;
        case kBorder:
            m_border = 1;
            break;
        case kFontHeight:
            m_fontHeight = tokenizer->ExpectReal();
            break;
        case kTextColor:
            m_color[0] = (unsigned char)tokenizer->ExpectInt();
            m_color[1] = (unsigned char)tokenizer->ExpectInt();
            m_color[2] = (unsigned char)tokenizer->ExpectInt();
            m_color[3] = (unsigned char)tokenizer->ExpectInt();
            break;
        case kTextMaxLength:
            m_textMaxLength = tokenizer->ExpectInt();
            break;
        case kAlignment: {
            const int alignment = tokenizer->Next();
            if (alignment == kWordAlignmentLeft) {
                m_alignment = kAlignmentLeft;
            } else if (alignment == kWordAlignmentRight) {
                m_alignment = kAlignmentRight;
            } else if (alignment == kWordAlignmentCenter) {
                m_alignment = kAlignmentCenter;
            } else if (alignment == kWordAlignmentJustify) {
                m_alignment = kAlignmentJustify;
            } else {
                tokenizer->Error(kBadField);
            }
            break;
        }
        case kLeftMargin:
            m_leftMargin = tokenizer->ExpectReal();
            break;
        case kRightMargin:
            m_rightMargin = tokenizer->ExpectReal();
            break;
        case kIndent:
            m_indent = tokenizer->ExpectReal();
            break;
        case kLeading:
            m_leading = tokenizer->ExpectReal();
            break;
        default:
            tokenizer->Error(kBadField);
            break;
        }
        token = tokenizer->Next();
    }
}

// ---- the table's base -------------------------------------------------------------------------------------------------

AtFlashResourceTable::AtFlashResourceTable() {
    m_initialised = 0;
    m_firstGroup = 0;
    m_lastGroup = 0;
    m_items = 0;
    m_itemCount = 0;
    m_groupCount = 0;
    AtCopyName8(m_name, "NotInitd");
    m_heap = 0;
}

AtFlashResourceTable::~AtFlashResourceTable() {
    Clear();
}

void AtFlashResourceTable::Clear() {
    if (m_initialised == 0) {
        return;
    }
    if (m_items != 0) {
        operator delete(m_items);
    }
    for (unsigned int i = 0; i < m_groupCount; i++) {
        Group* group = m_firstGroup;
        if (group != 0) {
            m_firstGroup = group->m_next;
            if (m_firstGroup != 0) {
                m_firstGroup->m_prev = 0;
            } else {
                m_lastGroup = 0;
            }
            group->m_first = 0;
            group->m_last = 0;
            operator delete(group);
        }
    }
    m_initialised = 0;
    m_items = 0;
    m_itemCount = 0;
    m_groupCount = 0;
    m_firstGroup = 0;
    m_lastGroup = 0;
    AtCopyName8(m_name, "NotInitd");
    m_heap = 0;
}

void AtFlashResourceTable::Allocate(unsigned int count) {
    m_itemCount = count;
    void* heap = HeapOf(this);
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    m_items = (AtFlashResource**)operator new(count * sizeof(AtFlashResource*));
    AtPopHeap();
    if (m_items == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        m_items[i] = 0;
    }
}

void AtFlashResourceTable::Release(unsigned int index) {
    m_items[index]->m_references--;
}

void AtFlashResourceTable::Slot3(int) {
    AtFatal("AtFlashResourceTable::Slot3 (0x00017290) is not recovered", 0, 0, 0);
}

// ---- the table of text formats --------------------------------------------------------------------------------------------

AtFlashTextFormatTable::AtFlashTextFormatTable() {
    m_loaded = 0;
    m_heap = 0;
}

AtFlashTextFormatTable::~AtFlashTextFormatTable() {
    ReleaseFormats();
}

void AtFlashTextFormatTable::Allocate(unsigned int count) {
    AtFlashResourceTable::Allocate(count);
    void* heap = HeapOf(this);
    if (heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(heap);
    AtFlashTextFormat* formats = AtNewArray<AtFlashTextFormat>(count);
    AtPopHeap();
    if (formats == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < count; i++) {
        m_items[i] = &formats[i];
        formats[i].m_heap = HeapOf(this);
    }
}

void AtFlashTextFormatTable::ReleaseFormats() {
    if (m_loaded == 0) {
        return;
    }
    if (m_items != 0 && m_items[0] != 0) {
        AtDeleteArray(static_cast<AtFlashTextFormat*>(m_items[0]));
        for (unsigned int i = 0; i < m_itemCount; i++) {
            m_items[i] = 0;
        }
    }
    Clear();
    m_loaded = 0;
    m_heap = 0;
}

void AtFlashTextFormatTable::Read(const char* name) {
    AtIoTokenFile file;
    const int status = file.Open(name, "AFX", 2, 0);
    if (status != 0) {
        AtReportFileError(status, name, "AFX");
    }
    AtIoTokenizer* tokenizer = file.m_tokenizer;
    tokenizer->SetKeywords(s_keywords, sizeof(s_keywords) / sizeof(s_keywords[0]), 0);
    tokenizer->ExpectToken(kNameSpace);
    char* space;
    tokenizer->ExpectStringCopy(&space);
    AtCopyName(space, m_name);
    operator delete(space);
    tokenizer->ExpectToken(kTextFormat);
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
