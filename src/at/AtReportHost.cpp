// The host's C library for the fatal report: vsprintf, which the original reaches at 0x00125ECE. Not recovered code; nothing here is recorded.
#include <stdarg.h>
#include <stdio.h>

#include "at/AtFatal.h"

int AtFormatV(char* out, const char* format, AtVaList arguments) {
    return vsprintf(out, format, arguments);
}
