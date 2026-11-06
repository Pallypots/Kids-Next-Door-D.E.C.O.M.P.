// Not recovered code: the host's C library, for AtFormatReal in AtIoTokenizer.cpp. Kept apart because the matching build has no headers.
#include <stdio.h>

// "d.dddddddddddddddde+xx": 17 significant digits, which is what the original's C library works from.
void AtFormatScientific(double value, char* out) {
    sprintf(out, "%.16e", value);
}
