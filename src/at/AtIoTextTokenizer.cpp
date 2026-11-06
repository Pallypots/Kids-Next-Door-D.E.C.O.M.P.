// AtIoTextTokenizer. Original address cluster 0x000224C0-0x000232D9, with tables at 0x000232DC-0x000233F3 and 0x00272B00.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtIoTokenizer.h"
#include "AtIoJamCommon.h"

namespace {

// Token of each character from '!' to '~'. Values above the Token enum select a lexer state and are replaced before Next returns,
// with one exception: see kAfterMinus.
enum { kStartZero = 41, kStartMinus = 42, kStartIdentifier = 57 };
const unsigned char kCharTokens[0x7F - 0x21] = {
    12, 2, 1, 1, 13, 15, 1, 17, 18, 16, 20, 9, 42, 3, 21,   // ! " # $ % & ' ( ) * + , - . /
    41, 4, 4, 4, 4, 4, 4, 4, 4, 4,                          // 0-9
    11, 10, 23, 22, 24, 26, 27,                             // : ; < = > ? @
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,     // A-Z
    7, 1, 8, 14, 1, 1,                                      // [ \ ] ^ _ `
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
    57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,     // a-z
    5, 25, 6, 28                                            // { | } ~
};

enum State { kStart, kAfterZero, kDecimal, kHex, kFraction, kAfterMinus, kIdentifier, kAfterSlash, kInString, kAfterBackslash };

// Original behaviour preserved: no length check on the upper-case copy of the name.
unsigned int HashOfName(const char* name) {
    char upper[0x40];
    char* to = upper;
    for (; *name != 0; name++) {
        *to++ = (char)toupper((signed char)*name);
    }
    *to = 0;
    return AtIoTextTokenizer::HashOf(upper);
}

void Insert(AtIoTextTokenizer* t, const AtIoKeyword* keyword) {
    unsigned int h = HashOfName(keyword->m_name);
    while (t->m_hash[h] != 0) {
        h++;
        if (h >= AtIoTextTokenizer::kHashSize) {
            h = 0;
        }
    }
    t->m_hash[h] = keyword;
}

void PutText(AtIoStream* stream, const char* text) {
    for (; *text != 0; text++) {
        stream->Put(*text);
    }
}

}  // namespace

unsigned int AtIoTextTokenizer::HashOf(const char* upper) {
    unsigned int sum = 0;
    unsigned int shift = 0;
    for (; *upper != 0; upper++) {
        sum += (unsigned int)((int)(signed char)*upper - 0x30) << (shift & 0x1F);
        shift += 6;
    }
    return sum % kHashSize;
}

AtIoTextTokenizer::AtIoTextTokenizer() {
    m_identifier[0] = 0;
    m_keywords = 0;
    m_keywordCount = 0;
}

AtIoTextTokenizer::~AtIoTextTokenizer() {
}

void AtIoTextTokenizer::SetKeywords(const AtIoKeyword* table, unsigned int count, AtIoTokenizer* inherit) {
    m_keywords = table;
    m_keywordCount = count;
    for (unsigned int i = 0; i < kHashSize; i++) {
        m_hash[i] = 0;
    }
    unsigned int inherited = 0;
    if (inherit != 0) {
        const AtIoTextTokenizer* from = (const AtIoTextTokenizer*)inherit;
        inherited = from->m_keywordCount;
        for (unsigned int i = 0; i < kHashSize; i++) {
            if (from->m_hash[i] != 0) {
                Insert(this, from->m_hash[i]);
            }
        }
    }
    for (const AtIoKeyword* k = m_keywords; k < m_keywords + m_keywordCount; k++) {
        Insert(this, k);
    }
    m_keywordCount += inherited;
}

void AtIoTextTokenizer::LookUpIdentifier() {
    if (m_keywords == 0) {
        Error(kInvalidKeyword);
    }
    unsigned int h = HashOf(m_identifier);
    while (m_hash[h] != 0) {
        const char* a = m_identifier;
        const char* b = m_hash[h]->m_name;
        for (;;) {
            if ((int)(signed char)*a != toupper((signed char)*b)) {
                break;
            }
            if (*a == 0) {
                m_type = m_hash[h]->m_id;
                return;
            }
            a++;
            b++;
        }
        h++;
        if (h >= kHashSize) {
            h = 0;
        }
    }
    m_type = kError;
}

int AtIoTextTokenizer::Next() {
    if (m_pushedBack != 0) {
        const int type = m_type;
        m_pushedBack = 0;
        return type;
    }
    int state = kStart;
    int negative = 0;
    unsigned int length = 0;
    float scale = 0.1f;
    m_type = 0;
    if (!m_stream->SkipWhitespace()) {
        return 0;
    }
    for (;;) {
        AtIoStream* s = m_stream;
        if (s->m_cur >= s->m_limit && !s->Refill()) {
            break;
        }
        const unsigned int c = (unsigned char)*s->m_cur++;
        switch (state) {
        case kStart:
            if (c < 0x21 || c > 0x7E) {
                // Past this the original indexes its table out of bounds and takes what it finds as the token. This stops instead.
                Error(kSyntaxError);
                m_type = kError;
                return kError;
            }
            m_type = kCharTokens[c - 0x21];
            switch (m_type) {
            case kError:
                Error(kSyntaxError);
                break;
            case kString:
                state = kInString;
                m_stringLength = 0;
                break;
            case kReal:
                state = kFraction;
                m_real = 0;
                break;
            case kInt:
                state = kDecimal;
                m_int = (int)(c - '0');
                break;
            case kDivide:
                state = kAfterSlash;
                break;
            case kStartZero:
                state = kAfterZero;
                m_int = 0;
                break;
            case kStartMinus:
                state = kAfterMinus;
                m_int = 0;
                negative = 1;
                break;
            case kStartIdentifier:
                m_identifier[0] = (char)toupper((int)c);
                state = kIdentifier;
                length = 1;
                break;
            default:
                return m_type;
            }
            break;
        case kAfterZero:
            if (c == 'x' || c == 'X') {
                state = kHex;
                break;
            }
            // fall through
        case kDecimal:
            if (c >= '0' && c <= '9') {
                m_int = (int)(c - '0' + (unsigned int)m_int * 10);
            } else if (c == '.') {
                m_real = (float)m_int;
                state = kFraction;
            } else {
                s->m_cur--;
                if (negative) {
                    m_int = (int)(0u - (unsigned int)m_int);
                }
                m_type = kInt;
                return kInt;
            }
            break;
        case kHex:
            if (c >= '0' && c <= '9') {
                m_int = (int)(((unsigned int)m_int << 4) + c - '0');
            } else {
                const unsigned int upper = (unsigned int)toupper((int)c);
                if (upper >= 'A' && upper <= 'F') {
                    m_int = (int)(((unsigned int)m_int << 4) + upper - 0x37);
                } else {
                    s->m_cur--;
                    if (negative) {
                        m_int = (int)(0u - (unsigned int)m_int);
                    }
                    m_type = kInt;
                    return kInt;
                }
            }
            break;
        case kFraction:
            if (c >= '0' && c <= '9') {
                // The original evaluates these on the x87 and rounds only when it stores. Doubles keep that on any host:
                // see docs/portability.md.
                m_real = (float)((double)(int)(c - '0') * (double)scale + (double)m_real);
                scale = (float)((double)scale * (double)0.1f);
            } else {
                if (c == '.') {
                    Error(kSyntaxError);
                }
                s->m_cur--;
                if (negative) {
                    m_real = -m_real;
                }
                m_type = kReal;
                return kReal;
            }
            break;
        case kAfterMinus:
            if (c >= '1' && c <= '9') {
                m_int = (int)(c - '0');
                state = kDecimal;
            } else if (c == '.') {
                state = kFraction;
                m_real = 0;
            } else if (c == '0') {
                state = kAfterZero;
                m_int = 0;
            } else {
                s->m_cur--;
                return kMinus;  // m_type stays kStartMinus, as in the original.
            }
            break;
        case kIdentifier: {
            const unsigned int upper = (unsigned int)toupper((int)c);
            if ((upper >= 'A' && upper <= 'Z') || (upper >= '0' && upper <= '9') || upper == '_') {
                if (length >= 0x3F) {
                    Error(kInvalidKeyword);
                }
                m_identifier[length] = (char)upper;
                length++;
            } else {
                m_identifier[length] = 0;
                s->m_cur--;
                LookUpIdentifier();
                return m_type;
            }
            break;
        }
        case kAfterSlash:
            if (c == '/') {
                if (!m_stream->SkipLine() || !m_stream->SkipWhitespace()) {
                    return 0;
                }
                state = kStart;
            } else {
                s->m_cur--;
                return m_type;
            }
            break;
        case kInString:
            if (c == '"') {
                m_string[m_stringLength] = 0;
                return kString;
            }
            if (c == '\\' && m_escapes != 0) {
                state = kAfterBackslash;
                break;
            }
            if (m_stringLength >= 0xFF) {
                Error(kInvalidString);
            }
            m_string[m_stringLength] = (char)c;
            m_stringLength++;
            break;
        case kAfterBackslash:
            if (m_stringLength >= 0xFF) {
                Error(kInvalidString);
            }
            switch (c) {
            case 'n': case 'N': m_string[m_stringLength] = '\n'; break;
            case 't': case 'T': m_string[m_stringLength] = '\t'; break;
            case '"': m_string[m_stringLength] = '"'; break;
            case '\\': m_string[m_stringLength] = '\\'; break;
            default: Error(kInvalidString); break;
            }
            m_stringLength++;
            state = kInString;
            break;
        }
    }
    // End of the data.
    switch (state) {
    case kAfterZero:
        m_type = kInt;
        return kInt;
    case kDecimal:
    case kHex:
        if (negative) {
            m_int = (int)(0u - (unsigned int)m_int);
        }
        m_type = kInt;
        return kInt;
    case kFraction:
        if (negative) {
            m_real = -m_real;
        }
        m_type = kReal;
        return kReal;
    case kAfterMinus:
    case kAfterSlash:
        Error(kSyntaxError);
        return kError;
    case kIdentifier:
        m_identifier[length] = 0;
        LookUpIdentifier();
        return m_type;
    case kInString:
    case kAfterBackslash:
        Error(kInvalidString);
        return kError;
    default:
        return 0;
    }
}

void AtIoTextTokenizer::PutInt(int value) {
    char text[0x20];
    m_int = value;
    m_type = kInt;
    AtFormatInt(value, text);
    for (const char* p = text; *p != 0 && p < text + 0x1F; p++) {
        m_stream->Put(*p);
    }
}

void AtIoTextTokenizer::PutReal(float value) {
    char text[0x20];
    m_real = value;
    m_type = kReal;
    AtFormatReal(value, text);
    for (const char* p = text; *p != 0 && p < text + 0x1F; p++) {
        m_stream->Put(*p);
    }
}

void AtIoTextTokenizer::PutString(const char* text) {
    strncpy(m_string, text, 0xFF);
    m_string[0xFF] = 0;
    m_type = kString;
    m_stream->Put('"');
    PutText(m_stream, m_string);
    m_stream->Put('"');
}

void AtIoTextTokenizer::PutComment(const char* text) {
    m_stream->Put('/');
    m_stream->Put('/');
    m_stream->Put(' ');
    strncpy(m_string, text, 0xFF);
    m_string[0xFF] = 0;
    PutText(m_stream, m_string);
}

void AtIoTextTokenizer::PutKeyword(int id) {
    if (m_keywords == 0) {
        Error(kInvalidKeyword);
    }
    for (unsigned int i = 0; i < m_keywordCount; i++) {
        if (m_keywords[i].m_id == id) {
            PutText(m_stream, m_keywords[i].m_name);
            m_type = id;
            return;
        }
    }
    Error(kInvalidKeyword);
}

void AtIoTextTokenizer::PutRaw(const char* text) {
    PutText(m_stream, text);
}

void AtIoTextTokenizer::PutLeftBracket() { m_type = kLeftBracket; m_stream->Put('['); }
void AtIoTextTokenizer::PutRightBracket() { m_type = kRightBracket; m_stream->Put(']'); }
void AtIoTextTokenizer::PutLeftCurly() { m_type = kLeftCurly; m_stream->Put('{'); }
void AtIoTextTokenizer::PutRightCurly() { m_type = kRightCurly; m_stream->Put('}'); }
void AtIoTextTokenizer::PutComma() { m_type = kComma; m_stream->Put(','); }
void AtIoTextTokenizer::PutSemicolon() { m_type = kSemicolon; m_stream->Put(';'); }
void AtIoTextTokenizer::PutColon() { m_type = kColon; m_stream->Put(':'); }
void AtIoTextTokenizer::PutExclamation() { m_type = kExclamation; m_stream->Put('!'); }
void AtIoTextTokenizer::PutPercent() { m_type = kPercent; m_stream->Put('%'); }
void AtIoTextTokenizer::PutCaret() { m_type = kCaret; m_stream->Put('^'); }
void AtIoTextTokenizer::PutAmpersand() { m_type = kAmpersand; m_stream->Put('&'); }
void AtIoTextTokenizer::PutTimes() { m_type = kTimes; m_stream->Put('*'); }
void AtIoTextTokenizer::PutLeftParen() { m_type = kLeftParen; m_stream->Put('('); }
void AtIoTextTokenizer::PutRightParen() { m_type = kRightParen; m_stream->Put(')'); }
void AtIoTextTokenizer::PutMinus() { m_type = kMinus; m_stream->Put('-'); }
void AtIoTextTokenizer::PutPlus() { m_type = kPlus; m_stream->Put('+'); }
void AtIoTextTokenizer::PutDivide() { m_type = kDivide; m_stream->Put('/'); }
void AtIoTextTokenizer::PutEquals() { m_type = kEquals; m_stream->Put('='); }
void AtIoTextTokenizer::PutLess() { m_type = kLess; m_stream->Put('<'); }
void AtIoTextTokenizer::PutGreater() { m_type = kGreater; m_stream->Put('>'); }
void AtIoTextTokenizer::PutOr() { m_type = kOr; m_stream->Put('|'); }
void AtIoTextTokenizer::PutQuestion() { m_type = kQuestion; m_stream->Put('?'); }
void AtIoTextTokenizer::PutAt() { m_type = kAt; m_stream->Put('@'); }
void AtIoTextTokenizer::PutTilde() { m_type = kTilde; m_stream->Put('~'); }

void AtIoTextTokenizer::PutNewline() {
    m_stream->Put('\r');
    m_stream->Put('\n');
}

void AtIoTextTokenizer::PutSpace() {
    m_stream->Put(' ');
}

void AtIoTextTokenizer::PutSpaces(unsigned int count) {
    for (; count != 0; count--) {
        m_stream->Put(' ');
    }
}

void AtIoTextTokenizer::PutTab() {
    m_stream->Put('\t');
}

void AtIoTextTokenizer::PutTabs(unsigned int count) {
    for (; count != 0; count--) {
        m_stream->Put('\t');
    }
}

void AtIoTextTokenizer::PutName(const char* name) {
    char text[9];
    AtCopyName9(text, name);
    PutString(text);
}

const char* AtIoTextTokenizer::GetIdentifier() {
    return m_identifier;
}
