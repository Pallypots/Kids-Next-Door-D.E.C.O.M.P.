// Not recovered code: the original's C library, as the action script machine's values use it, for src/at/AtFlashScriptVM.cpp. Kept apart from the recovered code.
//
// The original formats a real with its library's sprintf and "%g" (0x00125559). A modern library differs from it in two ways that show: it writes a two-digit exponent where the original writes three
// ("1e+010"), and it rounds the exact value half to even where the original works from 17 significant digits and rounds those half up (the same as AtFormatReal in src/at/AtIoTokenizer.cpp). Its
// special values are "1.#INF", "-1.#INF", "-1.#IND" (the default NaN) and "1.#QNAN". So this writes the original's form itself. tests/oracle/oracle_asvm.cpp compares it with the original's.

void AtFormatScientific(double value, char* out);  // src/at/AtIoFormatHost.cpp: "d.dddddddddddddddde+xx", 17 significant digits

namespace {
void Copy(char* to, const char* from) {
    while ((*to++ = *from++) != 0) {
    }
}
}  // namespace

void AtFormatGeneral(char* out, double value) {
    const unsigned char* bytes = (const unsigned char*)&value;
    const unsigned int high = (unsigned int)bytes[4] | ((unsigned int)bytes[5] << 8) | ((unsigned int)bytes[6] << 16) | ((unsigned int)bytes[7] << 24);
    const unsigned int low = (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8) | ((unsigned int)bytes[2] << 16) | ((unsigned int)bytes[3] << 24);
    const int negative = (int)(high >> 31);
    if ((high & 0x7FF00000) == 0x7FF00000) {
        const unsigned int mantissaHigh = high & 0x000FFFFF;
        if (mantissaHigh == 0 && low == 0) {
            Copy(out, negative ? "-1.#INF" : "1.#INF");
        } else if (negative && mantissaHigh == 0x00080000 && low == 0) {
            Copy(out, "-1.#IND");
        } else if (mantissaHigh & 0x00080000) {
            Copy(out, negative ? "-1.#QNAN" : "1.#QNAN");
        } else {
            Copy(out, negative ? "-1.#SNAN" : "1.#SNAN");
        }
        return;
    }
    char* to = out;
    if (negative) {
        *to++ = '-';
    }
    const double magnitude = negative ? -value : value;
    if (magnitude == 0) {
        Copy(to, "0");
        return;
    }
    char scientific[40];
    AtFormatScientific(magnitude, scientific);
    // Six significant digits, rounded half up from the seventeen.
    char digits[7];
    for (int i = 0; i < 6; i++) {
        digits[i] = scientific[i == 0 ? 0 : i + 1];
    }
    int exponent = 0;
    const char* e = scientific + 19;
    const int negativeExponent = *e == '-';
    for (e++; *e != 0; e++) {
        exponent = exponent * 10 + (*e - '0');
    }
    if (negativeExponent) {
        exponent = -exponent;
    }
    if (scientific[7] >= '5') {
        int i = 5;
        while (i >= 0 && digits[i] == '9') {
            digits[i] = '0';
            i--;
        }
        if (i < 0) {
            digits[0] = '1';
            exponent++;
        } else {
            digits[i]++;
        }
    }
    int count = 6;  // Trailing zeros go.
    while (count > 1 && digits[count - 1] == '0') {
        count--;
    }
    if (exponent < -4 || exponent >= 6) {
        *to++ = digits[0];
        if (count > 1) {
            *to++ = '.';
            for (int i = 1; i < count; i++) {
                *to++ = digits[i];
            }
        }
        *to++ = 'e';
        *to++ = exponent < 0 ? '-' : '+';
        const int a = exponent < 0 ? -exponent : exponent;
        *to++ = (char)('0' + a / 100);
        *to++ = (char)('0' + a / 10 % 10);
        *to++ = (char)('0' + a % 10);
        *to = 0;
        return;
    }
    if (exponent < 0) {
        *to++ = '0';
        *to++ = '.';
        for (int i = 0; i < -exponent - 1; i++) {
            *to++ = '0';
        }
        for (int i = 0; i < count; i++) {
            *to++ = digits[i];
        }
    } else {
        for (int i = 0; i <= exponent; i++) {
            *to++ = i < count ? digits[i] : '0';
        }
        if (count > exponent + 1) {
            *to++ = '.';
            for (int i = exponent + 1; i < count; i++) {
                *to++ = digits[i];
            }
        }
    }
    *to = 0;
}
