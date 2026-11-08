// See include/at/AtImage.h. Fonts and 2D fonts.
#include "at/AtImage.h"

#include "at/AtImageDatabase.h"

void AtFatal(const char* message, int file, int line, int unused);
void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);

void* AtGrI2DFont::s_heap;

namespace {

// The font's words: the database's table at 0x002B08E0.
enum Word {
    kWordMatAssignment = 0x3B, kWordKerning = 0x3E, kWordTransparentColor = 0x3F, kWordCharacterSet = 0x40, kWordColorValueRectangle = 0x41, kWordBmpSource = 0x42, kWordTgaSource = 0x43,
    kWordColorRectangleCharX = 0x44, kWordColorRectangleCharY = 0x45, kWordTm2Source = 0x46, kWordTplSource = 0x47, kWordDemandLoad = 0x48, kWordLeading = 0x49, kWordScale = 0x4A,
    kWordInfrequentModify = 0x4B, kWordFrequentModify = 0x4C
};

enum Problem { kOutOfRange = 0x23, kListAlreadyGiven = 0x27 };

enum { kSpace = 0x20, kNewline = 0x0A };

unsigned int LengthOf(const char* text) {
    unsigned int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

// A table of count characters, all zero, on the 2D fonts' heap: CharacterSet's and BuildCharacterSet's, each inline in the original.
AtGrI2DFontChar* NewCharacters(unsigned int count) {
    if (AtGrI2DFont::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtGrI2DFont::s_heap);
    AtGrI2DFontChar* chars = (AtGrI2DFontChar*)operator new(count * sizeof(AtGrI2DFontChar));
    AtPopHeap();
    if (chars == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    unsigned char* bytes = (unsigned char*)chars;
    for (unsigned int i = 0; i < count * sizeof(AtGrI2DFontChar); i++) {
        bytes[i] = 0;
    }
    return chars;
}

void MarkSpaces(AtGrI2DFontChar* chars, unsigned int count) {
    for (unsigned int i = 0; i < count; i++) {
        if (chars[i].m_code == kSpace) {
            chars[i].m_space = 1;
        } else if (chars[i].m_code == kNewline) {
            chars[i].m_newline = 1;
        }
    }
}

}  // namespace

// ---- every font ---------------------------------------------------------------------------------------------------------

AtFont::AtFont() {
    m_name[0] = 0;
    m_flags = 0;
    m_count = 0;
    m_unknown14 = 0;
    m_database = 0;
    m_materials = 0;
    m_kerning = 0.0f;
    m_leading = 0.0f;
    m_scaleX = 1.0f;
    m_scaleY = 1.0f;
}

AtFont::~AtFont() {
    AtFont::Free();
}

void AtFont::Free() {
    m_name[0] = 0;
    m_flags = 0;
    m_count = 0;
    m_unknown14 = 0;
    m_database = 0;
    m_materials = 0;
    m_kerning = 0.0f;
    m_leading = 0.0f;
    m_scaleX = 1.0f;
    m_scaleY = 1.0f;
}

void AtFont::ReadWord(AtMaterialOwner*, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kWordKerning:
        m_kerning = tokenizer->ExpectReal();
        break;
    case kWordLeading:
        m_leading = tokenizer->ExpectReal();
        break;
    case kWordColorValueRectangle:
        m_flags |= kColorRectangle;
        m_colorRectangle.Read(tokenizer);
        break;
    case kWordColorRectangleCharX:
        m_flags |= kColorRectangleCharX;
        break;
    case kWordColorRectangleCharY:
        m_flags |= kColorRectangleCharY;
        break;
    case kWordDemandLoad:
        m_flags |= kDemandLoad;
        break;
    case kWordScale:
        m_scaleX = tokenizer->ExpectReal();
        m_scaleY = tokenizer->ExpectReal();
        break;
    default:
        tokenizer->Error(AtIoTokenizer::kExpectedKeyword);
        break;
    }
}

void AtFont::Read(AtMaterialOwner* owner, AtFontDatabase* database, const char* name, AtIoTokenizer* tokenizer) {
    if (m_count > 0) {
        Free();
    }
    m_database = database;
    m_materials = &database->m_materials;
    AtCopyName8(m_name, name);
    tokenizer->ExpectLeftCurly();
    for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly; token = tokenizer->Next()) {
        ReadWord(owner, tokenizer, token);
    }
}

void AtFont::Allocate(AtMaterialOwner*, AtFontDatabase* database, const char* name, AtGrCharacterSource* source, unsigned int flags) {
    if (m_count > 0) {
        Free();
    }
    m_count = (unsigned int)(source->m_end - source->m_begin);
    if (m_count == 0) {
        return;
    }
    m_database = database;
    m_materials = &database->m_materials;
    m_flags = flags;
    m_kerning = 0.0f;
    m_leading = 0.0f;
    m_scaleX = 1.0f;
    m_scaleY = 1.0f;
    AtCopyName8(m_name, name);
    BuildCharacterSet(source);
}

// ---- the 2D font --------------------------------------------------------------------------------------------------------

void AtGrI2DFont::SetHeap(void* heap) {
    s_heap = heap;
}

AtGrI2DFont::AtGrI2DFont() {
    m_unknown44[0] = 0;
    m_unknown44[1] = 0;
    m_unknown44[2] = 0;
    m_unknown44[3] = 0;
    m_unknown44[4] = 0;
    m_format = 0;
    m_chars = 0;
    m_material = 0xFFFFFFFF;
}

AtGrI2DFont::~AtGrI2DFont() {
    AtGrI2DFont::Free();
}

void AtGrI2DFont::Free() {
    if (m_chars != 0) {
        operator delete(m_chars);
    }
    m_unknown44[0] = 0;
    m_unknown44[1] = 0;
    m_unknown44[2] = 0;
    m_unknown44[3] = 0;
    m_unknown44[4] = 0;
    m_format = 0;
    m_chars = 0;
    m_material = 0xFFFFFFFF;
}

void AtGrI2DFont::FreeData() {
    if ((m_flags & kOwnsData) != 0) {
        if (m_chars != 0) {
            operator delete(m_chars);
        }
        m_flags &= ~kOwnsData;
    }
}

void AtGrI2DFont::Slot3() {
    AtFatal("Atlas Graphics: slot 3 of AtGrI2DFont, 0x000FF510, is not recovered", 0, 0, 0);
}

void AtGrI2DFont::BuildCharacterSet(AtGrCharacterSource* source) {
    m_chars = NewCharacters(m_count);
    for (unsigned int i = 0; i < m_count; i++) {
        m_chars[i].m_code = source->At(i);
        if (m_chars[i].m_code == kSpace) {
            m_chars[i].m_space = 1;
        } else if (m_chars[i].m_code == kNewline) {
            m_chars[i].m_newline = 1;
        }
    }
}

int AtGrI2DFont::FindCharacter(unsigned short code) {
    int low = 0;
    int high = (int)m_count - 1;
    int middle = (int)(m_count >> 1);
    if (high >= 0) {
        do {
            const unsigned short here = m_chars[middle].m_code;
            if (here == code) {
                if (low <= high) {
                    return middle;
                }
                break;
            }
            if (here > code) {
                high = middle - 1;
            } else {
                low = middle + 1;
            }
            middle = (high + low) >> 1;
        } while (low <= high);
    }
    return code == kNewline ? -1 : 0;
}

void AtGrI2DFont::ReadWord(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, int token) {
    switch (token) {
    case kWordMatAssignment:
        m_material = (unsigned int)tokenizer->ExpectInt();
        if (m_material >= m_materials->m_count) {
            tokenizer->Error(kOutOfRange);
        }
        break;
    case kWordTransparentColor:
        m_transparent.r = (unsigned char)tokenizer->ExpectInt();
        m_transparent.g = (unsigned char)tokenizer->ExpectInt();
        m_transparent.b = (unsigned char)tokenizer->ExpectInt();
        m_flags |= kTransparent;
        break;
    case kWordCharacterSet: {
        // A second set is reported, and the first is left to leak.
        if (m_chars != 0) {
            tokenizer->Error(kListAlreadyGiven);
        }
        m_count = (unsigned int)tokenizer->ExpectCount();
        m_chars = NewCharacters(m_count);
        // Integers are one character each, strings one for each byte, sign extended. A string is not held to the count: it writes past the table if it is longer than what is left.
        for (unsigned int i = 0; i < m_count;) {
            const int next = tokenizer->Next();
            if (next == AtIoTokenizer::kString) {
                const unsigned int length = LengthOf(tokenizer->m_string);
                for (unsigned int k = 0; k < length; k++) {
                    m_chars[i + k].m_code = (unsigned short)(short)(signed char)tokenizer->m_string[k];
                }
                i += length;
            } else if (next == AtIoTokenizer::kInt) {
                m_chars[i].m_code = (unsigned short)tokenizer->m_int;
                i++;
            } else {
                tokenizer->Error(AtIoTokenizer::kExpectedString);
            }
        }
        tokenizer->ExpectRightCurly();
        MarkSpaces(m_chars, m_count);
        break;
    }
    case kWordBmpSource:
        m_format = kBmpSource;
        break;
    case kWordTgaSource:
        m_format = kTgaSource;
        break;
    case kWordTm2Source:
        m_format = kTm2Source;
        break;
    case kWordTplSource:
        m_format = kTplSource;
        break;
    case kWordInfrequentModify:
        m_format |= kInfrequentModify;
        break;
    case kWordFrequentModify:
        m_format |= kFrequentModify;
        break;
    default:
        AtFont::ReadWord(owner, tokenizer, token);
        break;
    }
}

int AtGrI2DFont::PassName(AtGrDevice* device, const AtGrI2DFont* font, int value) {
    return device->NameSink()->TakeName(font->m_name, value);
}

void AtGrI2DFont::SetTransparentColor(const AtColor* color) {
    m_transparent.r = color->r;
    m_transparent.g = color->g;
    m_transparent.b = color->b;
    m_transparent.a = color->a;
    m_flags |= kTransparent;
}

void AtGrI2DFont::ClearTransparentColor() {
    m_flags &= ~kTransparent;
}
