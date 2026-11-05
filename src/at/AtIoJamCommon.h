// Shared by the AtIoJam* sources. CRT functions are declared rather than included: the matching build has no headers, and
// these prototypes agree with every C library we target.
#pragma once

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned int AtSize;  // Original-era MSVC has no decltype; 32-bit only.
#else
typedef decltype(sizeof(0)) AtSize;
#endif

extern "C" {
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strcpy(char* to, const char* from);
char* strcat(char* to, const char* from);
char* strncpy(char* to, const char* from, AtSize n);
char* strncat(char* to, const char* from, AtSize n);
int strncmp(const char* a, const char* b, AtSize n);
AtSize strlen(const char* s);
int toupper(int c);
}

inline unsigned int AtReadU16(const unsigned char* p) {
    return (p[1] << 8) + p[0];
}

inline unsigned int AtReadU32(const unsigned char* p) {
    return (((((p[3] << 8) + p[2]) << 8) + p[1]) << 8) + p[0];
}
