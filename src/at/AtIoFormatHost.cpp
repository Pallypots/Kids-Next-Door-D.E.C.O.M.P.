// Not recovered code: the host's C library, for src/at. Kept apart because the matching build has no headers.
#include <stdio.h>
#include <stdlib.h>

// "d.dddddddddddddddde+xx": 17 significant digits, which is what the original's C library works from.
void AtFormatScientific(double value, char* out) {
    sprintf(out, "%.16e", value);
}

namespace {
// The original's atoi and _wtoi wrap on overflow, and a modern library's may saturate, so these do it themselves.
template <class C> int ToIntWrapping(const C* text) {
    while (*text == ' ' || (*text >= 9 && *text <= 13)) {
        text++;
    }
    const C sign = *text;
    if (*text == '-' || *text == '+') {
        text++;
    }
    unsigned int value = 0;
    for (; *text >= '0' && *text <= '9'; text++) {
        value = value * 10 + (unsigned int)(*text - '0');
    }
    return (int)(sign == '-' ? 0u - value : value);
}
}  // namespace

int AtNarrowToInt(const char* text) {
    return ToIntWrapping(text);
}

float AtNarrowToFloat(const char* text) {
    return (float)atof(text);
}

namespace {
// Characters past the buffer are ignored: nothing the game parses this way is that long. Non-ASCII stops a number anyway.
void Narrow(const unsigned short* text, char* out, unsigned int size) {
    unsigned int i = 0;
    for (; i + 1 < size && text[i] != 0; i++) {
        out[i] = text[i] < 0x80 ? (char)text[i] : '?';
    }
    out[i] = 0;
}
}  // namespace

int AtWideToInt(const unsigned short* text) {
    return ToIntWrapping(text);
}

float AtWideToFloat(const unsigned short* text) {
    // Original behaviour preserved: when nothing parses, the result is whatever the local held. Here it is zero.
    char narrow[64];
    Narrow(text, narrow, sizeof narrow);
    float value = 0;
    sscanf(narrow, "%f", &value);
    return value;
}
