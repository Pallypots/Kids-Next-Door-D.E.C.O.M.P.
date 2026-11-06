// AtText, AtTextAscii, AtTextUnicode, AtTextTable: the reader of game text, .AIS. Original address clusters 0x0001F560-0x0001FBBE, 0x0002DD30-0x0002E1EE, 0x000398B0-0x00039903, 0x00031BA0, 0x000140D0-0x00014247.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtText.h"
#include "AtIoJamCommon.h"

void* AtTextTable::s_heap;

namespace {

struct AtPlacement {};

enum TextToken { kAsciiString = 0x39, kAsciiChar = 0x3A, kUnicodeString = 0x3B, kUnicodeChar = 0x3C, kNameSpace = 0x3D };

// 0x002B0368.
const AtIoKeyword kKeywords[] = {
    {kAsciiString, "ASCIIString"}, {kAsciiChar, "ASCIIChar"}, {kUnicodeString, "UnicodeString"}, {kUnicodeChar, "UnicodeChar"}, {kNameSpace, "NameSpace"},
};

}  // namespace

inline void* operator new(AtSize, void* where, AtPlacement) { return where; }
inline void operator delete(void*, void*, AtPlacement) {}

const char* AtIoStatusText(int status) {
    switch (status) {
    case 0: return "no error";
    case 1: return "file or directory not found";
    case 2: return "error opening the specified file";
    case 3: return "general error";
    case 4: return "media not formatted";
    case 5: return "media format incorrect";
    case 6: return "buffer supplied was insufficient";
    case 7: return "asynchronous operation is pending";
    case 8: return "file pointer is out of bounds";
    case 9: return "access violation";
    case 10: return "write was only partially completed";
    case 11: return "memory card error (is the card attached?)";
    case 12: return "memory card is not attached";
    case 13: return "memory card file system has no save game specified";
    case 14: return "device is out of space";
    case 15: return "bad file descriptor";
    case 16: return "media not ready";
    case 17: return "media is corrupt";
    case 18: return "media is invalid";
    case 20: return "feature is unimplemented";
    case 21: return "feature is unsupported";
    default: return "<unspecified error>";
    }
}

void AtReportFileError(int status, const char* path, const char* extension) {
    // The original is variadic, at 0x00014230 and 0x000141C0. Every caller recovered so far passes "%s.%s" with these two.
    char message[0x200];
    strcpy(message, AtIoStatusText(status));
    strcat(message, ": ");
    strcat(message, path);
    strcat(message, ".");
    strcat(message, extension);
    AtFatal(message, 0, 0, 0);
}

AtText::AtText() {
    m_capacity = 0;
    m_start = 0;
    m_end = 0;
}

void AtText::ClearFields() {
    m_capacity = 0;
    m_start = 0;
    m_end = 0;
}

unsigned int AtText::ComputeEnd() {
    for (unsigned int i = 0; i < m_capacity - m_start; i++) {
        if (CharAt(i) == 0) {
            return m_start + i;
        }
    }
    return m_capacity - 1;
}

AtTextUnicode::AtTextUnicode() {
    m_data = 0;
    ClearFields();
}

void AtTextUnicode::Release() {
    m_data = 0;
    ClearFields();
}

void AtTextUnicode::Rewind() {
    m_start = 0;
    m_end = ComputeEnd();
}

void AtTextUnicode::Init(unsigned short* data, unsigned int capacity) {
    m_capacity = capacity;
    m_data = data;
    data[capacity - 1] = 0;
    m_start = 0;
    m_end = ComputeEnd();
}

unsigned int AtTextUnicode::CharAt(unsigned int index) {
    return m_data[m_start + index];
}

void AtTextUnicode::SetChar(unsigned int index, unsigned int c) {
    m_data[m_start + index] = (unsigned short)c;
}

void AtTextUnicode::RemoveRange(unsigned int first, unsigned int last) {
    unsigned short* to = m_data + m_start + first;
    const unsigned short* from = m_data + m_start + last + 1;
    for (unsigned int n = m_end - m_start - last; n != 0; n--) {
        *to++ = *from++;
    }
    m_end += first - last - 1;
}

void AtTextUnicode::RemoveAt(unsigned int index) {
    unsigned short* to = m_data + m_start + index;
    const unsigned short* from = to + 1;
    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {
        *to++ = *from++;
    }
    m_end--;
}

void AtTextUnicode::Insert(unsigned int c, unsigned int index) {
    unsigned short* at = m_data + m_start + index;
    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {
        at[n] = at[n - 1];
    }
    *at = (unsigned short)c;
    m_end++;
}

void AtTextUnicode::Recount() {
    m_start = 0;
    const unsigned short* p = m_data;
    while (*p != 0) {
        p++;
    }
    m_end = (unsigned int)(p - m_data);
}

void AtTextUnicode::CopyNarrow(char* out, unsigned int size) {
    const unsigned short* from = m_data + m_start;
    unsigned int n = m_end - m_start;
    if (size <= n) {
        n = size - 1;
    }
    out[n] = 0;
    while (n != 0) {
        n--;
        out[n] = (char)from[n];
    }
}

void AtTextUnicode::CopyWide(unsigned short* out, unsigned int size) {
    const unsigned short* from = m_data + m_start;
    unsigned int n = m_end - m_start;
    if (size <= n) {
        n = size - 1;
    }
    out[n] = 0;
    while (n != 0) {
        n--;
        out[n] = from[n];
    }
}

int AtTextUnicode::ToInt() {
    return AtWideToInt(m_data);
}

float AtTextUnicode::ToFloat() {
    return AtWideToFloat(m_data);
}

AtTextAscii::AtTextAscii() {
    m_data = 0;
    ClearFields();
}

void AtTextAscii::Init(char* data, unsigned int capacity) {
    m_capacity = capacity;
    m_data = data;
    data[capacity - 1] = 0;
    m_start = 0;
    m_end = ComputeEnd();
}

unsigned int AtTextAscii::CharAt(unsigned int index) {
    return (unsigned char)m_data[m_start + index];
}

void AtTextAscii::SetChar(unsigned int index, unsigned int c) {
    m_data[m_start + index] = (char)c;
}

void AtTextAscii::RemoveRange(unsigned int first, unsigned int last) {
    char* to = m_data + m_start + first;
    const char* from = m_data + m_start + last + 1;
    for (unsigned int n = m_end - m_start - last; n != 0; n--) {
        *to++ = *from++;
    }
    m_end += first - last - 1;
}

void AtTextAscii::RemoveAt(unsigned int index) {
    char* to = m_data + m_start + index;
    const char* from = to + 1;
    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {
        *to++ = *from++;
    }
    m_end--;
}

void AtTextAscii::Insert(unsigned int c, unsigned int index) {
    char* at = m_data + m_start + index;
    for (unsigned int n = m_end - m_start - index + 1; n != 0; n--) {
        at[n] = at[n - 1];
    }
    *at = (char)c;
    m_end++;
}

void AtTextAscii::Recount() {
    m_start = 0;
    m_end = (unsigned int)strlen(m_data);
}

void AtTextAscii::CopyNarrow(char* out, unsigned int size) {
    const char* from = m_data + m_start;
    unsigned int n = m_end - m_start;
    if (size <= n) {
        n = size - 1;
    }
    out[n] = 0;
    while ((int)n > 0) {
        n--;
        out[n] = from[n];
    }
}

void AtTextAscii::CopyWide(unsigned short* out, unsigned int size) {
    const char* from = m_data + m_start;
    unsigned int n = m_end - m_start;
    if (size <= n) {
        n = size - 1;
    }
    out[n] = 0;
    while (n != 0) {
        n--;
        out[n] = (unsigned char)from[n];
    }
}

int AtTextAscii::ToInt() {
    return AtNarrowToInt(m_data);
}

float AtTextAscii::ToFloat() {
    return AtNarrowToFloat(m_data);
}

AtTextTable::AtTextTable() {
    m_ascii = 0;
    m_loaded = 0;
    m_charCount = 0;
    m_charsUsed = 0;
    m_stringCount = 0;
    m_stringsUsed = 0;
    m_strings = 0;
    m_chars = 0;
}

AtTextTable::~AtTextTable() {
    AtTextTable::Clear();
}

void AtTextTable::Clear() {
    GmNameMap::Clear();
    if (m_loaded != 0) {
        if (m_strings != 0) {
            operator delete(m_strings);
            m_strings = 0;
        }
        if (m_chars != 0) {
            operator delete(m_chars);
            m_chars = 0;
        }
        m_ascii = 0;
        m_loaded = 0;
        m_charCount = 0;
        m_charsUsed = 0;
        m_stringCount = 0;
        m_stringsUsed = 0;
        m_strings = 0;
        m_chars = 0;
    }
}

AtText* AtTextTable::StringAt(unsigned int index) {
    // The original has two branches here, for the two kinds, that do the same thing.
    return m_ascii != 0 ? (AtText*)((AtTextAscii*)m_strings + index) : (AtText*)((AtTextUnicode*)m_strings + index);
}

void AtTextTable::AllocateAscii(unsigned int strings, unsigned int chars) {
    GmNameMap::Init(strings);
    m_charCount = chars;
    m_stringCount = strings;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_chars = operator new(m_charCount);
    AtPopHeap();
    if (m_chars == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* block = operator new(strings * sizeof(AtTextAscii));
    if (block != 0) {
        for (unsigned int i = 0; i < strings; i++) {
            new ((AtTextAscii*)block + i, AtPlacement()) AtTextAscii;
        }
    }
    m_strings = block;
    AtPopHeap();
    if (m_strings == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_ascii = 1;
    m_loaded = 1;
}

void AtTextTable::AllocateUnicode(unsigned int strings, unsigned int chars) {
    GmNameMap::Init(strings);
    m_charCount = chars;
    m_stringCount = strings;
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_chars = operator new(m_charCount * 2);
    AtPopHeap();
    if (m_chars == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* block = operator new(strings * sizeof(AtTextUnicode));
    if (block != 0) {
        for (unsigned int i = 0; i < strings; i++) {
            new ((AtTextUnicode*)block + i, AtPlacement()) AtTextUnicode;
        }
    }
    m_strings = block;
    AtPopHeap();
    if (m_strings == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_ascii = 0;
    m_loaded = 1;
}

void AtTextTable::Parse(AtIoContext* context, AtIoTokenizer* t) {
    unsigned int strings = 0;
    unsigned int chars = 0;
    if (m_loaded != 0) {
        Clear();
    }
    t->SetKeywords(kKeywords, sizeof kKeywords / sizeof kKeywords[0], 0);
    t->m_escapes = 1;
    int token = t->Next();
    char space[8];
    const char* spaceName = 0;
    if (token == kNameSpace) {
        t->ExpectName(space);
        spaceName = space;
        token = t->Next();
    }
    if (token != AtIoTokenizer::kLeftCurly) {
        do {
            switch (token) {
            case kAsciiString:
            case kUnicodeString:
                if (strings == 0) {
                    t->ExpectLeftBracket();
                    strings = (unsigned int)t->ExpectInt();
                    t->ExpectRightBracket();
                }
                break;
            case kAsciiChar:
                t->ExpectLeftBracket();
                chars = (unsigned int)t->ExpectInt();
                t->ExpectRightBracket();
                m_ascii = 1;
                break;
            case kUnicodeChar:
                t->ExpectLeftBracket();
                chars = (unsigned int)t->ExpectInt();
                t->ExpectRightBracket();
                m_ascii = 0;
                break;
            default:
                break;
            }
            token = t->Next();
        } while (token != AtIoTokenizer::kLeftCurly);
        if (strings != 0 && chars == 0) {
            AtFatal("If you have strings you must declare the amount of characters stored in the string database with either ASCIIChar [x] or UnicodeChar [x]", 0, 0, 0);
        }
    }
    if (m_ascii != 0) {
        AllocateAscii(strings, chars);
        for (; strings != 0; strings--) {
            char name[8];
            t->ExpectName(name);
            const char* text = t->ExpectString();
            const unsigned int length = t->m_stringLength;
            char* to = (char*)m_chars + m_charsUsed;
            strncpy(to, text, length);
            AtTextAscii* item = (AtTextAscii*)m_strings + m_stringsUsed;
            item->Init(to, length + 1);
            GmNameMap::Insert(name, item);
            m_charsUsed += length + 1;
            m_stringsUsed++;
        }
    } else {
        AllocateUnicode(strings, chars);
        for (; strings != 0; strings--) {
            char name[8];
            t->ExpectName(name);
            unsigned int length = 0;
            for (int value = t->ExpectInt(); (unsigned short)value != 0; value = t->ExpectInt()) {
                ((unsigned short*)m_chars)[m_charsUsed] = (unsigned short)value;
                m_charsUsed++;
                length++;
            }
            AtTextUnicode* item = (AtTextUnicode*)m_strings + m_stringsUsed;
            item->Init((unsigned short*)m_chars + (m_charsUsed - length), length + 1);
            GmNameMap::Insert(name, item);
            m_charsUsed++;
            m_stringsUsed++;
        }
    }
    context->AddMap(this, spaceName);
    t->m_escapes = 0;
}

void AtTextTable::Load(AtIoContext* context, const char* path, int unused) {
    (void)unused;
    AtIoTokenFile file;
    const int status = file.Open(path, "AIS", 2, 0);
    if (status != 0) {
        AtReportFileError(status, path, "AIS");
    }
    Parse(context, file.m_tokenizer);
    file.Close();
}
