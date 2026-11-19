// Inputs for the matrix functions, the same on every host. Used by oracle_matrix.cpp and tests/native/test_matrix.cpp.
#pragma once
#include <cmath>

#include "at/AtMatrix.h"
#include "at/AtQuaternion.h"
#include "at/AtVector3.h"
#include "vec_cases.h"

namespace matrix_cases {
using namespace vec_cases;

// Signalling NaNs, which only the original's x87 loads quiet. Not hashed: a host may quiet them anywhere.
inline float signalling(Rng& r) {
    static const uint32_t kBits[] = {0x7F800001, 0xFF800001, 0x7FA00000, 0xFFBFFFFF};
    return from_bits(kBits[r.next() % 4]);
}

inline float value(Rng& r, int kind) {
    if (kind == 2 && r.next() % 8 == 0) return signalling(r);
    return pick(r, kind != 0);
}

inline void fill(Rng& r, AtMatrix* m, int kind) {
    for (int i = 0; i < 16; i++) m->m[i] = value(r, kind);
}

// A rotation and a translation, the kind SetInverse and the coordinate systems are given, from a unit-like quaternion.
inline void rigid(Rng& r, AtMatrix* m) {
    double q[4], n = 0;
    for (double& v : q) { v = (double)(int32_t)r.next() / 2147483648.0; n += v * v; }
    if (n == 0) n = 1;
    for (double& v : q) v /= std::sqrt(n);
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const float rows[12] = {(float)(1 - 2 * (y * y + z * z)), (float)(2 * (x * y + w * z)), (float)(2 * (x * z - w * y)), 0,
                            (float)(2 * (x * y - w * z)), (float)(1 - 2 * (x * x + z * z)), (float)(2 * (y * z + w * x)), 0,
                            (float)(2 * (x * z + w * y)), (float)(2 * (y * z - w * x)), (float)(1 - 2 * (x * x + y * y)), 0};
    for (int i = 0; i < 12; i++) m->m[i] = rows[i];
    for (int i = 12; i < 15; i++) m->m[i] = (float)((double)(int32_t)r.next() / 2147483648.0 * 500.0);
    m->m[15] = 1;
}

// Elements from three values and their negatives, so that products cancel exactly and the order of a sum shows: (big - big) + small is not (small - big) + big.
inline void cancelling(Rng& r, AtMatrix* m) {
    float v[3];
    for (float& e : v) e = pick(r, false);
    v[2] = from_bits((to_bits(v[2]) & 0x807FFFFFu) | 0x2F800000u);  // One small next to the others.
    for (int i = 0; i < 16; i++) {
        const uint32_t k = r.next();
        const float e = v[k % 3];
        m->m[i] = (k & 8) ? -e : e;
    }
}

// Any of the three kinds of matrix.
inline void matrix(Rng& r, AtMatrix* m, int kind) {
    const uint32_t k = r.next() % 4;
    if (k == 0) rigid(r, m);
    else if (k == 1) cancelling(r, m);
    else fill(r, m, kind);
}

inline void vector(Rng& r, AtVector3* v, int kind) { v->x = value(r, kind); v->y = value(r, kind); v->z = value(r, kind); }

inline void quaternion(Rng& r, AtQuaternion* q, int kind) {
    const uint32_t k = r.next() % 4;
    if (k == 3) {
        // Two components equal and two small: SetRotation's 1 - (zz + xx) cancels to almost nothing, so an ulp of its denominator, and the order of its sum, show.
        // analysis-local/scratch/matrix/search_rotation.py found about one in six of these does, over 4,000,000.
        q->x = from_bits(0x3F000000u | (r.next() & 0x807FFFFFu));
        q->y = (r.next() & 1) ? q->x : -q->x;
        q->z = from_bits((0x30000000u + ((r.next() % 6) << 23)) | (r.next() & 0x807FFFFFu));
        q->w = from_bits((0x30000000u + ((r.next() % 6) << 23)) | (r.next() & 0x807FFFFFu));
        return;
    }
    if (k < 2) {
        double v[4], n = 0;
        for (double& e : v) { e = (double)(int32_t)r.next() / 2147483648.0; n += e * e; }
        if (n == 0) n = 1;
        const double s = 1 / std::sqrt(n);
        q->x = (float)(v[0] * s); q->y = (float)(v[1] * s); q->z = (float)(v[2] * s); q->w = (float)(v[3] * s);
        return;
    }
    q->x = value(r, kind); q->y = value(r, kind); q->z = value(r, kind); q->w = value(r, kind);
}

}  // namespace matrix_cases
