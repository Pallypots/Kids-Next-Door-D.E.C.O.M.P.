// AtIoTokenizer, the base of the text tokenizer, and AtIoTokenFile. Original address clusters 0x00015460-0x00016078,
// 0x00013ED0, 0x00013F90, 0x000206F0, 0x00020710. Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtIoTokenizer.h"
#include "AtIoJamCommon.h"

void* AtIoTokenFile::s_heap;

void AtFormatScientific(double value, char* out);  // AtIoFormatHost.cpp

void AtCopyName(const char* from, char* to) {
    to[0] = 0;
    for (int i = 0; i < 8; i++) {
        to[i] = from[i];
        if (from[i] == 0) {
            break;
        }
    }
}

void AtCopyName9(char* to, const char* from) {
    static const char empty[8] = {0};
    if (from == 0) {
        from = empty;
    }
    for (int i = 0; i < 8; i++) {  // Always eight bytes, past a shorter name's terminator: the original copies two words.
        to[i] = from[i];
    }
    to[8] = 0;
}

void AtFormatInt(int value, char* out) {
    // The original calls the C library's itoa with base 10.
    unsigned int magnitude = (unsigned int)value;
    if (value < 0) {
        *out++ = '-';
        magnitude = 0u - magnitude;
    }
    char reversed[12];
    int count = 0;
    do {
        reversed[count++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    while (count > 0) {
        *out++ = reversed[--count];
    }
    *out = 0;
}

// The original calls the C library's sprintf with "%f". That library works from 17 significant digits and rounds those half up, where a
// modern one rounds the exact value half to even (123456.7890625 gives ...789063 and ...789062). This does what the original's does, on
// any host: 0x00125559, tested against it in tests/oracle/oracle_tok.cpp.
void AtFormatReal(float value, char* out) {
    const double v = value;
    if (v != v) {
        unsigned int bits = 0;
        const unsigned char* bytes = (const unsigned char*)&value;
        bits = ((unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8) | ((unsigned int)bytes[2] << 16) | ((unsigned int)bytes[3] << 24));
        strcpy(out, (bits >> 31) ? "-1.#QNAN0" : "1.#QNAN0");
        return;
    }
    char* to = out;
    if (v < 0) {
        *to++ = '-';
    }
    const double magnitude = v < 0 ? -v : v;
    if (magnitude > 1.7976931348623157e308) {
        strcpy(to, "1.#INF00");
        return;
    }
    if (magnitude == 0) {
        strcpy(to, "0.000000");  // No sign on a negative zero: the comparison above does not see one, as the original's does not.
        return;
    }
    // d.dddddddddddddddde+xx
    char scientific[40];
    AtFormatScientific(magnitude, scientific);
    char digits[19];
    digits[0] = '0';  // Room for a carry out of the first digit.
    digits[1] = scientific[0];
    for (int i = 0; i < 16; i++) {
        digits[2 + i] = scientific[2 + i];
    }
    int exponent = 0;
    const char* e = scientific + 19;  // Past the 'e'.
    const int negativeExponent = *e == '-';
    for (e++; *e != 0; e++) {
        exponent = exponent * 10 + (*e - '0');
    }
    if (negativeExponent) {
        exponent = -exponent;
    }
    exponent++;  // digits[1] now has weight 10^(exponent - 1), and a carry moves it up by one.
    const int keep = exponent + 6;  // Digits kept from digits[1]: everything down to the sixth decimal.
    if (keep >= 0 && keep < 17 && digits[1 + keep] >= '5') {
        int i = keep;
        while (digits[i] == '9') {
            digits[i] = '0';
            i--;
        }
        digits[i]++;
    }
    const char* from = digits + 1;
    int count = keep < 0 ? 0 : (keep < 17 ? keep : 17);
    if (keep >= 0 && digits[0] != '0') {  // The carry made a new first digit.
        from = digits;
        exponent++;
        count++;
    }
    if (exponent <= 0) {
        *to++ = '0';
    }
    int position = 0;
    for (int i = 0; i < exponent; i++) {
        *to++ = position < count ? from[position] : '0';
        position++;
    }
    *to++ = '.';
    for (int i = 0; i < 6; i++) {
        if (exponent + i < 0) {
            *to++ = '0';
        } else {
            *to++ = position < count ? from[position] : '0';
            position++;
        }
    }
    *to = 0;
}

AtIoTokenizer::AtIoTokenizer() {
    Reset();
}

AtIoTokenizer::~AtIoTokenizer() {
    Reset();
}

void AtIoTokenizer::Reset() {
    m_stream = 0;
    m_pushedBack = 0;
    m_type = 0;
    m_int = 0;
    m_real = 0;
    m_string[0] = 0;
    m_stringLength = 0;
    m_fileName = 0;
    m_extension[0] = 0;
    m_escapes = 0;
}

void AtIoTokenizer::SetKeywords(const AtIoKeyword*, unsigned int, AtIoTokenizer*) {
}

const char* AtIoTokenizer::GetIdentifier() {
    return "";
}

int AtIoTokenizer::ExpectInt() {
    if (Next() != kInt) {
        Error(kExpectedInt);
    }
    return m_int;
}

float AtIoTokenizer::ExpectReal() {
    if (Next() != kReal) {
        Error(kExpectedReal);
    }
    return m_real;
}

const char* AtIoTokenizer::ExpectString() {
    if (Next() != kString) {
        Error(kExpectedString);
    }
    return m_string;
}

const char* AtIoTokenizer::ExpectString(unsigned int maxLength) {
    if (Next() != kString) {
        Error(kExpectedString);
    }
    if (m_stringLength > maxLength) {
        Error(kInvalidString);
    }
    return m_string;
}

void AtIoTokenizer::ExpectStringCopy(char** out) {
    if (Next() != kString) {
        Error(kExpectedString);
    }
    const unsigned int size = (unsigned int)strlen(m_string) + 1;
    *out = (char*)operator new(size);
    if (*out == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    for (unsigned int i = 0; i < size; i++) {
        (*out)[i] = m_string[i];
    }
}

void AtIoTokenizer::ExpectName(char* out) {
    if (Next() != kString) {
        Error(kExpectedString);
    }
    if (m_stringLength > 8) {
        Error(kInvalidString);
    }
    AtCopyName(m_string, out);
}

void AtIoTokenizer::ExpectToken(int token) {
    if (Next() != token) {
        Error(kExpectedKeyword);
    }
}

void AtIoTokenizer::ExpectLeftBracket() {
    if (Next() != kLeftBracket) {
        Error(kExpectedLeftBracket);
    }
}

void AtIoTokenizer::ExpectRightBracket() {
    if (Next() != kRightBracket) {
        Error(kExpectedRightBracket);
    }
}

void AtIoTokenizer::ExpectLeftCurly() {
    if (Next() != kLeftCurly) {
        Error(kExpectedLeftCurly);
    }
}

void AtIoTokenizer::ExpectRightCurly() {
    if (Next() != kRightCurly) {
        Error(kExpectedRightCurly);
    }
}

int AtIoTokenizer::ExpectCount() {
    if (Next() != kLeftBracket) {
        Error(kExpectedLeftBracket);
    }
    if (Next() != kInt) {
        Error(kExpectedInt);
    }
    if (Next() != kRightBracket) {
        Error(kExpectedRightBracket);
    }
    if (Next() != kLeftCurly) {
        Error(kExpectedLeftCurly);
    }
    return m_int;
}

void AtIoTokenizer::CopyName(char* out) {
    for (unsigned int i = 0; i < 8; i++) {
        out[i] = m_string[i];
        if (m_string[i] == 0) {
            break;
        }
    }
}

const char* AtIoTokenizer::ErrorText(int code) {
    // Spellings as in the executable.
    switch (code) {
    case 0: return "syntax error";
    case 1: return "parse error";
    case 2: return "numeric too long";
    case 3: return "expected string";
    case 4: return "expected real";
    case 5: return "expected int";
    case 6: return "expected left curly";
    case 7: return "expected right curly";
    case 8: return "expected left bracket";
    case 9: return "expected right bracket";
    case 10: return "expected comma";
    case 11: return "expected semi-colon";
    case 12: return "expected colon";
    case 13: return "expected exclamation";
    case 14: return "expected percent";
    case 15: return "expected caret";
    case 16: return "expected ampersand";
    case 17: return "expected times";
    case 18: return "expected left parenthesis";
    case 19: return "expected righ parenthesis";
    case 20: return "expected minus";
    case 21: return "expected plus";
    case 22: return "expected divide";
    case 23: return "expected equals";
    case 24: return "expected less than";
    case 25: return "expected greater than";
    case 26: return "expected or sign";
    case 27: return "expected question mark";
    case 28: return "expected at sign";
    case 29: return "expected at tilde";
    case 30: return "expected keyword";
    case 31: return "expected string or left curly";
    case 32: return "expected EOF";
    case 33: return "invalid keyword";
    case 34: return "invalid string";
    case 35: return "invalid value";
    case 36: return "duplicate name";
    case 37: return "unknown name";
    case 38: return "unsupported";
    case 39: return "unexpected keyword";
    default: return "unknown error";
    }
}

// "NAME.EXT: line N - message". With no file name or no stream the original appends to an uninitialised buffer; this starts it empty.
static void BuildMessage(AtIoTokenizer* t, int code, char* message) {
    message[0] = 0;
    if (t->m_fileName != 0 && t->m_stream != 0) {
        strncpy(message, t->m_fileName, 0x3F);
        message[0x3F] = 0;
        strncat(message + strlen(message), t->m_extension, 4);
        strcat(message, ": line ");
        AtFormatInt(t->m_stream->m_line, message + strlen(message));
        strcat(message, " - ");
    }
    strcat(message, t->ErrorText(code));
}

void AtIoTokenizer::Report(int code) {
    char message[0x80];
    BuildMessage(this, code, message);
    strcat(message, "\n");  // The result goes nowhere in this build: presumably a debug print compiled out.
}

void AtIoTokenizer::Error(int code) {
    char message[0x80];
    BuildMessage(this, code, message);
    AtFatal(message, 0, 0, 0);
}

void AtIoTokenizer::SetStream(AtIoStream* stream) {
    if (m_stream != 0) {
        Reset();
    }
    m_stream = stream;
}

AtIoTokenFile::AtIoTokenFile() {
    m_tokenizer = 0;
    m_name = 0;
    m_ownsTokenizer = 0;
}

AtIoTokenFile::~AtIoTokenFile() {
    Close();
}

void AtIoTokenFile::Close() {
    if (m_ownsTokenizer != 0 && m_tokenizer != 0) {
        delete m_tokenizer;
    }
    if (m_name != 0) {
        operator delete(m_name);
    }
    m_stream.Detach();
    m_file.Close();
    m_tokenizer = 0;
    m_name = 0;
    m_ownsTokenizer = 0;
}

// What both Open overloads do once the file is open.
static void Attached(AtIoTokenFile* f, const char* path, AtIoTokenizer* tokenizer) {
    f->m_stream.Attach(&f->m_file, 0x400);
    if (tokenizer == 0) {
        if (AtIoTokenFile::s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(AtIoTokenFile::s_heap);
        tokenizer = new AtIoTextTokenizer;
        AtPopHeap();
        if (tokenizer == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        f->m_ownsTokenizer = 1;
    } else {
        f->m_ownsTokenizer = 0;
    }
    tokenizer->SetStream(&f->m_stream);
    f->m_tokenizer = tokenizer;
    if (AtIoTokenFile::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtIoTokenFile::s_heap);
    f->m_name = (char*)operator new(strlen(path) + 1);
    AtPopHeap();
    if (f->m_name == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    strcpy(f->m_name, path);
    f->m_tokenizer->m_fileName = f->m_name;
}

int AtIoTokenFile::Open(const char* path, const char* ext, int flags, AtIoTokenizer* tokenizer) {
    if (m_tokenizer != 0) {
        Close();
    }
    const int status = m_file.OpenWithExtension(path, ext, flags);
    if (status != 0) {
        return status;
    }
    Attached(this, path, tokenizer);
    if (ext == 0) {
        m_tokenizer->m_extension[0] = 0;
    } else {
        m_tokenizer->m_extension[0] = '.';
        strncpy(m_tokenizer->m_extension + 1, ext, 3);
    }
    return 0;
}

int AtIoTokenFile::Open(const char* path, int flags, AtIoTokenizer* tokenizer) {
    if (m_tokenizer != 0) {
        Close();
    }
    const int status = m_file.Open(path, flags);
    if (status != 0) {
        return status;
    }
    Attached(this, path, tokenizer);
    return 0;
}
