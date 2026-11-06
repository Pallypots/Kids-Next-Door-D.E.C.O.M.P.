// Not recovered code: the sine and cosine for the recovered math. The original uses the x87's fsin and fcos, whose results no standard function promises to reproduce.
// On x86 this uses those same instructions; elsewhere the host's library. How far the two can differ is measured by tests/oracle/oracle_vec.cpp: see docs/portability.md.
#include <math.h>

#include "at/AtVector3.h"

#if defined(_M_IX86) && defined(_MSC_VER)
double AtSin(double angle) {
    __asm {
        fld angle
        fsin
        fstp angle
    }
    return angle;
}

double AtCos(double angle) {
    __asm {
        fld angle
        fcos
        fstp angle
    }
    return angle;
}

double AtOneMinusCos(double angle) {
    __asm {
        fld1
        fld angle
        fcos
        fsubp st(1), st
        fstp angle
    }
    return angle;
}
#else
double AtSin(double angle) { return sin(angle); }
double AtCos(double angle) { return cos(angle); }
// More exact than the original near 0, where 1 - cos in a double would lose nearly everything.
double AtOneMinusCos(double angle) {
    const double half = sin(angle / 2);
    return 2 * half * half;
}
#endif
