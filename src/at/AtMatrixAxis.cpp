// The rotations about one axis, their two products, and the matrix to quaternion: the last five slots of the matrix table that some code in the game reads. See include/at/AtMatrix.h.
// Verification status per function: config/functions.d/matdev.toml.
//
// `double` stands for what the original keeps in an x87 register and `float` for what it stores, as in src/at/AtMatrix.cpp. Products of two floats are exact in a double; each sum rounds as the
// original's does at precision 53.
#include "at/AtMatrix.h"

#include "at/AtQuaternion.h"
#include "at/AtVector3.h"

extern "C" double sqrt(double value);

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

void AtMatrixPreMultiply3x3(AtMatrix* matrix, const float* r) {
    float* m = matrix->m;
    // Every element of rows 0-2 is held before the first is written.
    const float m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3];
    const float m4 = m[4], m5 = m[5], m6 = m[6], m7 = m[7];
    const float m8 = m[8], m9 = m[9], m10 = m[10], m11 = m[11];
    m[0] = (float)((double)m8 * r[2] + (double)m0 * r[0] + (double)m4 * r[1]);
    m[1] = (float)((double)m9 * r[2] + (double)m1 * r[0] + (double)m5 * r[1]);
    m[2] = (float)((double)m10 * r[2] + (double)m2 * r[0] + (double)m6 * r[1]);
    m[3] = (float)((double)m11 * r[2] + (double)m3 * r[0] + (double)m7 * r[1]);
    m[4] = (float)((double)m8 * r[6] + (double)m0 * r[4] + (double)m4 * r[5]);
    m[5] = (float)((double)m9 * r[6] + (double)m1 * r[4] + (double)m5 * r[5]);
    m[6] = (float)((double)m10 * r[6] + (double)m2 * r[4] + (double)m6 * r[5]);
    m[7] = (float)((double)m11 * r[6] + (double)m3 * r[4] + (double)m7 * r[5]);
    m[8] = (float)((double)m0 * r[8] + (double)m4 * r[9] + (double)m8 * r[10]);
    m[9] = (float)((double)m1 * r[8] + (double)m5 * r[9] + (double)m9 * r[10]);
    m[10] = (float)((double)m2 * r[8] + (double)m6 * r[9] + (double)m10 * r[10]);
    m[11] = (float)((double)m3 * r[8] + (double)m7 * r[9] + (double)m11 * r[10]);
}

void AtMatrixPostMultiply3x3(AtMatrix* matrix, const float* r) {
    float* m = matrix->m;
    for (int row = 0; row < 16; row += 4) {
        // Each row's three are held before the first is written.
        const float a = m[row], b = m[row + 1], c = m[row + 2];
        m[row] = (float)((double)c * r[8] + (double)a * r[0] + (double)b * r[4]);
        m[row + 1] = (float)((double)c * r[9] + (double)a * r[1] + (double)b * r[5]);
        m[row + 2] = (float)((double)a * r[2] + (double)b * r[6] + (double)c * r[10]);
    }
}

// Three rows of four, the fourth of each never written: the original's is stack it does not set, and neither product reads it.
void AtMatrixPreRotateX(AtMatrix* m, float angle) {
    float r[12];
    r[0] = 1.0f;
    r[1] = 0.0f;
    r[2] = 0.0f;
    r[4] = 0.0f;
    r[8] = 0.0f;
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    r[5] = (float)c;
    r[6] = (float)-s;
    r[9] = (float)s;
    r[10] = (float)c;
    AtMatrixPreMultiply3x3(m, r);
}

void AtMatrixPreRotateY(AtMatrix* m, float angle) {
    float r[12];
    r[1] = 0.0f;
    r[4] = 0.0f;
    r[5] = 1.0f;
    r[6] = 0.0f;
    r[9] = 0.0f;
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    r[0] = (float)c;
    r[2] = (float)s;
    r[8] = (float)-s;
    r[10] = (float)c;
    AtMatrixPreMultiply3x3(m, r);
}

void AtMatrixPreRotateZ(AtMatrix* m, float angle) {
    float r[12];
    r[2] = 0.0f;
    r[6] = 0.0f;
    r[8] = 0.0f;
    r[9] = 0.0f;
    r[10] = 1.0f;
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    r[0] = (float)c;
    r[1] = (float)-s;
    r[4] = (float)s;
    r[5] = (float)c;
    AtMatrixPreMultiply3x3(m, r);
}

void AtMatrixPostRotateZ(AtMatrix* m, float angle) {
    float r[12];
    r[2] = 0.0f;
    r[6] = 0.0f;
    r[8] = 0.0f;
    r[9] = 0.0f;
    r[10] = 1.0f;
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    r[0] = (float)c;
    r[1] = (float)-s;
    r[4] = (float)s;
    r[5] = (float)c;
    AtMatrixPostMultiply3x3(m, r);
}

void AtMatrixGetRotation(const AtMatrix* matrix, AtQuaternion* q) {
    const float* m = matrix->m;
    const double yz = (double)m[5] + m[10];
    const float yzStored = (float)yz;  // The original spills it, and the x branch reads the spill.
    const double trace = yz + m[0];
    if (trace > 0.0) {
        const double root = sqrt(trace + 1.0);  // 0x00272AC4 1.0f
        q->w = (float)(0.5 * root);             // 0x0027463C 0.5f
        const double k = 0.5 / root;
        q->x = (float)(((double)m[9] - m[6]) * k);
        q->y = (float)(((double)m[2] - m[8]) * k);
        q->z = (float)(((double)m[4] - m[1]) * k);
        return;
    }
    int axis = 0;
    if (m[5] > m[0]) {
        axis = 1;
    }
    if (m[10] > m[axis * 5]) {
        axis = 2;
    }
    switch (axis) {
    case 0: {
        const double root = sqrt((double)m[0] - yzStored + 1.0);
        q->x = (float)(0.5 * root);
        const double k = 0.5 / root;
        q->w = (float)(((double)m[9] - m[6]) * k);
        q->y = (float)(((double)m[4] + m[1]) * k);
        q->z = (float)(((double)m[8] + m[2]) * k);
        break;
    }
    case 1: {
        const double root = sqrt((double)m[5] - ((double)m[10] + m[0]) + 1.0);
        q->y = (float)(0.5 * root);
        const double k = 0.5 / root;
        q->w = (float)(((double)m[2] - m[8]) * k);
        q->z = (float)(((double)m[9] + m[6]) * k);
        q->x = (float)(((double)m[4] + m[1]) * k);
        break;
    }
    case 2: {
        const double root = sqrt((double)m[10] - ((double)m[5] + m[0]) + 1.0);
        q->z = (float)(0.5 * root);
        const double k = 0.5 / root;
        q->w = (float)(((double)m[4] - m[1]) * k);
        q->x = (float)(((double)m[8] + m[2]) * k);
        q->y = (float)(((double)m[9] + m[6]) * k);
        break;
    }
    }
}
