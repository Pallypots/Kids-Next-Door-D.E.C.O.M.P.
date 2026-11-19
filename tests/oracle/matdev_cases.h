// Inputs for the last five matrix-table slots and their helpers, the same on every host. Used by oracle_matdev.cpp and tests/native/test_matdev.cpp.
#pragma once
#include <cmath>
#include <cstring>

#include "matrix_cases.h"

namespace matdev_cases {
using namespace matrix_cases;

// An angle: often an ordinary one, sometimes anything value() gives.
inline float angle(Rng& rng, int kind) {
    const uint32_t k = rng.next() % 4;
    if (k == 0) return value(rng, kind);
    if (k == 1) return (float)((int)(rng.next() % 16) * 0.39269908169872414);  // multiples of pi/8, where sines and cosines are near round
    return (float)((double)(int32_t)rng.next() / 2147483648.0 * 7.0);
}

// A matrix for GetRotation: each of its four ways, ties between diagonal elements, and what matrix() gives.
inline void rotation_matrix(Rng& rng, AtMatrix* m, int kind) {
    const uint32_t k = rng.next() % 6;
    matrix(rng, m, kind);
    if (k < 3) {
        // About 180 degrees about axis k: the trace near -1, element k of the diagonal the largest.
        AtQuaternion q;
        double v[3] = {0, 0, 0};
        v[k] = 1;
        for (double& e : v) e += (double)(int32_t)rng.next() / 2147483648.0 * 0.2;
        const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        const double w = (double)(int32_t)rng.next() / 2147483648.0 * 0.1;
        q.x = (float)(v[0] / n); q.y = (float)(v[1] / n); q.z = (float)(v[2] / n); q.w = (float)w;
        AtMatrixSetRotation(m, &q);
    } else if (k == 3) {
        // Ties: two diagonal elements equal, the trace not above 0.
        m->m[0] = -value(rng, 0);
        m->m[5] = (rng.next() & 1) ? m->m[0] : -m->m[0];
        m->m[10] = (rng.next() & 1) ? m->m[0] : -1.0f;
    }
}

// One round's inputs, drawn in a fixed order.
struct Inputs {
    AtMatrix a, start, b;
    float t;
    float rows[12];
};

inline void draw(Rng& rng, int kind, Inputs* in) {
    matrix(rng, &in->a, kind);
    fill(rng, &in->start, kind);
    in->t = angle(rng, kind);
    for (float& f : in->rows) f = value(rng, kind);
    if (rng.next() % 4 == 0) {
        // Rows of three values and their negatives, so that a sum's order shows.
        AtMatrix c;
        cancelling(rng, &c);
        std::memcpy(in->rows, c.m, sizeof in->rows);
    }
    rotation_matrix(rng, &in->b, kind);
}

}  // namespace matdev_cases
