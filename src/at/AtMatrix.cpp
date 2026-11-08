// The matrix functions of the table g_atMatrix. See include/at/AtMatrix.h.
// Verification status per function: config/functions.d/matrix.toml.
#include "at/AtMatrix.h"

#include "at/AtQuaternion.h"
#include "at/AtVector3.h"

// The original rounds a product before it adds it, or keeps both in a register and rounds the sum: never one rounding for both. A compiler allowed to fuse them would give other bits.
#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#endif

namespace {

// What the original passes through an x87 register and stores, where the rest is moved as words: loading quiets a signalling NaN. volatile, or a compiler drops the round trip as doing nothing.
float ThroughRegister(float value) {
    volatile double held = value;
    return (float)held;
}

}  // namespace

void AtMatrixCopy(AtMatrix* to, const AtMatrix* from) {
    *to = *from;
}

void AtMatrixCopyWords(AtMatrix* to, const AtMatrix* from) {
    for (int i = 0; i < 16; i++) {
        to->m[i] = from->m[i];
    }
}

void AtMatrixIdentity(AtMatrix* m) {
    m->m[0] = 1.0f;
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[4] = 0.0f;
    m->m[5] = 1.0f;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->m[8] = 0.0f;
    m->m[9] = 0.0f;
    m->m[10] = 1.0f;
    m->m[11] = 0.0f;
    m->m[12] = 0.0f;
    m->m[13] = 0.0f;
    m->m[14] = 0.0f;
    m->m[15] = 1.0f;
}

void AtMatrixSetTranslation(AtMatrix* m, const AtVector3* position) {
    m->m[12] = position->x;
    m->m[13] = position->y;
    m->m[14] = position->z;
}

void AtMatrixSetRotation(AtMatrix* m, const AtQuaternion* q) {
    const double x = q->x, y = q->y, z = q->z, w = q->w;
    const double s = 2.0 / (x * x + y * y + z * z + w * w);  // 0x00274B64 2.0f
    // Which of these the original kept in a register and which it stored is its own choice, and the rounding follows it.
    const double xs = s * q->x;
    const float ys = (float)(s * q->y);
    const float zs = (float)(s * q->z);
    const float wx = (float)(xs * q->w);
    const float wy = (float)((double)ys * q->w);
    const float wz = (float)((double)zs * q->w);
    const double xx = xs * q->x;
    const double xy = (double)ys * q->x;
    const double xz = (double)zs * q->x;
    const float yy = (float)((double)ys * q->y);
    const float yz = (float)((double)zs * q->y);
    const double zz = (double)zs * q->z;
    m->m[0] = (float)(1.0 - (yy + zz));  // 0x00272AC4 1.0f
    m->m[1] = (float)(xy - wz);
    m->m[2] = (float)(xz + wy);
    m->m[4] = (float)(xy + wz);
    m->m[5] = (float)(1.0 - (zz + xx));
    m->m[6] = (float)((double)yz - wx);
    m->m[8] = (float)(xz - wy);
    m->m[9] = (float)((double)yz + wx);
    m->m[10] = (float)(1.0 - (yy + xx));
}

void AtMatrixSetScale(AtMatrix* m, const AtVector3* scale) {
    m->m[0] = scale->x;
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[4] = 0.0f;
    m->m[5] = scale->y;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->m[8] = 0.0f;
    m->m[9] = 0.0f;
    m->m[10] = scale->z;
    m->m[11] = 0.0f;
    m->m[12] = 0.0f;
    m->m[13] = 0.0f;
    m->m[14] = 0.0f;
    m->m[15] = 1.0f;
}

void AtMatrixSetUniformScale(AtMatrix* m, float scale) {
    m->m[0] = ThroughRegister(scale);
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[4] = 0.0f;
    m->m[5] = scale;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->m[8] = 0.0f;
    m->m[9] = 0.0f;
    m->m[10] = scale;
    m->m[11] = 0.0f;
    m->m[12] = 0.0f;
    m->m[13] = 0.0f;
    m->m[14] = 0.0f;
    m->m[15] = 1.0f;
}

void AtMatrixSetInverse(AtMatrix* to, const AtMatrix* from) {
    const float* f = from->m;
    float* t = to->m;
    t[12] = (float)-((double)f[2] * f[14] + (double)f[1] * f[13] + (double)f[0] * f[12]);
    t[13] = (float)-((double)f[5] * f[13] + (double)f[14] * f[6] + (double)f[12] * f[4]);
    const double z = -((double)f[10] * f[14] + (double)f[8] * f[12] + (double)f[13] * f[9]);
    t[15] = 1.0f;
    t[14] = (float)z;
    t[0] = f[0];
    t[1] = f[4];
    t[2] = f[8];
    t[3] = 0.0f;
    t[4] = f[1];
    t[5] = f[5];
    t[6] = f[9];
    t[7] = 0.0f;
    t[8] = f[2];
    t[9] = f[6];
    t[10] = f[10];
    t[11] = 0.0f;
}

void AtMatrixSetProductWords(AtMatrix* product, const AtMatrix* first, const AtMatrix* second) {
    // Each sum in the order the original adds it, which is not the same from one element to the next. Written from its instructions by a script, not by hand.
    float* to = product->m;
    const float* a = first->m;
    const float* b = second->m;
    to[0] = (float)((double)a[0] * b[0] + (double)a[3] * b[12] + (double)a[2] * b[8] + (double)a[1] * b[4]);
    to[1] = (float)((double)a[3] * b[13] + (double)a[2] * b[9] + (double)a[1] * b[5] + (double)a[0] * b[1]);
    to[2] = (float)((double)a[1] * b[6] + (double)a[3] * b[14] + (double)a[0] * b[2] + (double)a[2] * b[10]);
    to[3] = (float)((double)a[3] * b[15] + (double)a[2] * b[11] + (double)a[1] * b[7] + (double)a[0] * b[3]);
    to[4] = (float)((double)a[7] * b[12] + (double)a[6] * b[8] + (double)a[5] * b[4] + (double)a[4] * b[0]);
    to[5] = (float)((double)a[6] * b[9] + (double)a[4] * b[1] + (double)a[7] * b[13] + (double)a[5] * b[5]);
    to[6] = (float)((double)a[7] * b[14] + (double)a[5] * b[6] + (double)a[6] * b[10] + (double)a[4] * b[2]);
    to[7] = (float)((double)a[6] * b[11] + (double)a[4] * b[3] + (double)a[7] * b[15] + (double)a[5] * b[7]);
    to[8] = (float)((double)a[11] * b[12] + (double)a[8] * b[0] + (double)a[10] * b[8] + (double)a[9] * b[4]);
    to[9] = (float)((double)a[11] * b[13] + (double)a[10] * b[9] + (double)a[9] * b[5] + (double)a[8] * b[1]);
    to[10] = (float)((double)a[11] * b[14] + (double)a[9] * b[6] + (double)a[8] * b[2] + (double)a[10] * b[10]);
    to[11] = (float)((double)a[11] * b[15] + (double)a[10] * b[11] + (double)a[9] * b[7] + (double)a[8] * b[3]);
    to[12] = (float)((double)a[13] * b[4] + (double)a[15] * b[12] + (double)a[14] * b[8] + (double)a[12] * b[0]);
    to[13] = (float)((double)a[15] * b[13] + (double)a[13] * b[5] + (double)a[12] * b[1] + (double)a[14] * b[9]);
    to[14] = (float)((double)a[12] * b[2] + (double)a[15] * b[14] + (double)a[13] * b[6] + (double)a[14] * b[10]);
    to[15] = (float)((double)a[15] * b[15] + (double)a[13] * b[7] + (double)a[12] * b[3] + (double)a[14] * b[11]);
}

void AtMatrixMultiply(AtMatrix* m, const AtMatrix* by) {
    AtMatrix was;
    AtMatrixCopyWords(&was, m);
    AtMatrixSetProductWords(m, by, &was);
}

void AtMatrixSetProduct(AtMatrix* product, const AtMatrix* first, const AtMatrix* second) {
    // All of b is read first, then the first two rows of a, and those two rows written before the last two of a are read.
    const float* a = first->m;
    float b[16];
    for (int i = 0; i < 16; i++) {
        b[i] = second->m[i];
    }
    float row[8];
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 4; j++) {
            row[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
        }
    }
    for (int k = 0; k < 8; k++) {
        product->m[k] = row[k];
    }
    for (int i = 2; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            row[(i - 2) * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
        }
    }
    for (int k = 0; k < 8; k++) {
        product->m[8 + k] = row[k];
    }
}

void AtMatrixTransformPoint(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {
    // Stored after every step, and from read again each time: if from is to, it reads what it has written.
    const float* m = matrix->m;
    to->x = (float)((double)m[0] * from->x);
    to->y = (float)((double)m[1] * from->x);
    to->z = (float)((double)m[2] * from->x);
    to->x = (float)((double)m[4] * from->y + to->x);
    to->y = (float)((double)m[5] * from->y + to->y);
    to->z = (float)((double)m[6] * from->y + to->z);
    to->x = (float)((double)m[8] * from->z + to->x);
    to->y = (float)((double)m[9] * from->z + to->y);
    to->z = (float)((double)m[10] * from->z + to->z);
    to->x = (float)((double)m[12] + to->x);
    to->y = (float)((double)m[13] + to->y);
    to->z = (float)((double)m[14] + to->z);
}

void AtMatrixTransformDirection(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {
    const float* m = matrix->m;
    to->x = (float)((double)m[0] * from->x);
    to->y = (float)((double)m[1] * from->x);
    to->z = (float)((double)m[2] * from->x);
    to->x = (float)((double)m[4] * from->y + to->x);
    to->y = (float)((double)m[5] * from->y + to->y);
    to->z = (float)((double)m[6] * from->y + to->z);
    to->x = (float)((double)m[8] * from->z + to->x);
    to->y = (float)((double)m[9] * from->z + to->y);
    to->z = (float)((double)m[10] * from->z + to->z);
}

void AtMatrixGetPosition(const AtMatrix* m, AtVector3* position) {
    position->x = m->m[12];
    position->y = m->m[13];
    position->z = m->m[14];
}

void AtMatrixGetAxes(const AtMatrix* m, AtVector3* x, AtVector3* y, AtVector3* z) {
    x->x = m->m[0];
    x->y = m->m[1];
    x->z = m->m[2];
    y->x = m->m[4];
    y->y = m->m[5];
    y->z = m->m[6];
    z->x = m->m[8];
    z->y = m->m[9];
    z->z = m->m[10];
}

namespace {

// What the three look-at functions share: the direction normalised, and the up with its part along the direction taken away, normalised. Each of the original's has its own copy of this code.
void LookAxes(const AtVector3* direction, const AtVector3* up, AtVector3* forward, AtVector3* above) {
    forward->SetNormalized(*direction);
    const double along = (double)forward->y * up->y + (double)forward->x * up->x + (double)forward->z * up->z;
    above->x = (float)(up->x - forward->x * along);
    above->y = (float)(up->y - forward->y * along);
    above->z = (float)(up->z - forward->z * along);
    above->Normalize();
}

}  // namespace

void AtMatrixSetLookAt(AtMatrix* m, const AtVector3* direction, const AtVector3* up) {
    AtVector3 forward, above;
    LookAxes(direction, up, &forward, &above);
    const double sideX = (double)above.y * forward.z - (double)above.z * forward.y;
    m->m[1] = forward.y;
    m->m[2] = forward.z;
    m->m[8] = above.x;
    m->m[10] = above.z;
    const double sideY = (double)above.z * forward.x - (double)above.x * forward.z;
    const float sideZ = (float)((double)above.x * forward.y - (double)above.y * forward.x);
    m->m[6] = sideZ;
    m->m[9] = above.y;
    m->m[0] = forward.x;
    m->m[4] = (float)sideX;
    m->m[5] = (float)sideY;
}

// ---- Further slots the game calls ----------------------------------------------------------------------------------------

void AtMatrixSetScale3x3(AtMatrix* m, const AtVector3* scale) {
    m->m[0] = scale->x;
    m->m[5] = scale->y;
    m->m[10] = scale->z;
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[4] = 0.0f;
    m->m[6] = 0.0f;
    m->m[8] = 0.0f;
    m->m[9] = 0.0f;
}

void AtMatrixSetAxes(AtMatrix* m, const AtVector3* x, const AtVector3* y, const AtVector3* z) {
    m->m[0] = x->x;
    m->m[1] = x->y;
    m->m[2] = x->z;
    m->m[4] = y->x;
    m->m[5] = y->y;
    m->m[6] = y->z;
    m->m[8] = z->x;
    m->m[9] = z->y;
    m->m[10] = z->z;
}

void AtMatrixMakeTranslation(AtMatrix* m, const AtVector3* position) {
    m->m[1] = 0.0f;
    m->m[2] = 0.0f;
    m->m[3] = 0.0f;
    m->m[4] = 0.0f;
    m->m[6] = 0.0f;
    m->m[7] = 0.0f;
    m->m[8] = 0.0f;
    m->m[9] = 0.0f;
    m->m[11] = 0.0f;
    m->m[0] = 1.0f;
    m->m[5] = 1.0f;
    m->m[10] = 1.0f;
    m->m[12] = position->x;
    m->m[13] = position->y;
    m->m[14] = position->z;
    m->m[15] = 1.0f;
}

namespace {

// What the Make functions add to a 3x3: no fourth column, no position.
void ClearOutside3x3(AtMatrix* m) {
    m->m[3] = 0.0f;
    m->m[7] = 0.0f;
    m->m[11] = 0.0f;
    m->m[12] = 0.0f;
    m->m[13] = 0.0f;
    m->m[14] = 0.0f;
    m->m[15] = 1.0f;
}

}  // namespace

void AtMatrixMakeRotation(AtMatrix* m, const AtQuaternion* rotation) {
    AtMatrixSetRotation(m, rotation);
    ClearOutside3x3(m);
}

void AtMatrixMakeFromAxes(AtMatrix* m, const AtVector3* x, const AtVector3* y, const AtVector3* z) {
    AtMatrixSetAxes(m, x, y, z);
    ClearOutside3x3(m);
}

void AtMatrixSetAxisAngle(AtMatrix* m, const AtVector3* axis, float angle) {
    AtQuaternion rotation;
    rotation.SetAxisAngle(*axis, angle);
    AtMatrixSetRotation(m, &rotation);
}

void AtMatrixMakeAxisAngle(AtMatrix* m, const AtVector3* axis, float angle) {
    AtQuaternion rotation;
    rotation.SetAxisAngle(*axis, angle);
    AtMatrixSetRotation(m, &rotation);
    ClearOutside3x3(m);
}

void AtMatrixSetElement(AtMatrix* m, int row, int column, float value) {
    m->m[row * 4 + column] = ThroughRegister(value);
}

float AtMatrixGetElement(const AtMatrix* m, int row, int column) {
    return m->m[row * 4 + column];
}

void AtMatrixGetXAxis(const AtMatrix* m, AtVector3* axis) {
    axis->x = m->m[0];
    axis->y = m->m[1];
    axis->z = m->m[2];
}

void AtMatrixGetYAxis(const AtMatrix* m, AtVector3* axis) {
    axis->x = m->m[4];
    axis->y = m->m[5];
    axis->z = m->m[6];
}

void AtMatrixGetZAxis(const AtMatrix* m, AtVector3* axis) {
    axis->x = m->m[8];
    axis->y = m->m[9];
    axis->z = m->m[10];
}

void AtMatrixMultiplyRight(AtMatrix* m, const AtMatrix* by) {
    AtMatrix was;
    AtMatrixCopyWords(&was, m);
    AtMatrixSetProductWords(m, &was, by);
}

double AtMatrixTransformPointZ(const AtMatrix* matrix, const AtVector3* point) {
    const float* m = matrix->m;
    return (double)m[6] * point->y + (double)m[2] * point->x + (double)m[10] * point->z + m[14];
}

void AtMatrixInverseTransformPoint(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {
    const float* m = matrix->m;
    to->x = (float)((double)m[0] * from->x);
    to->y = (float)((double)m[4] * from->x);
    to->z = (float)((double)m[8] * from->x);
    to->x = (float)((double)m[1] * from->y + to->x);
    to->y = (float)((double)m[5] * from->y + to->y);
    to->z = (float)((double)m[9] * from->y + to->z);
    to->x = (float)((double)m[2] * from->z + to->x);
    to->y = (float)((double)m[6] * from->z + to->y);
    to->z = (float)((double)m[10] * from->z + to->z);
    to->x = (float)(to->x - ((double)m[1] * m[13] + (double)m[12] * m[0] + (double)m[14] * m[2]));
    to->y = (float)(to->y - ((double)m[5] * m[13] + (double)m[14] * m[6] + (double)m[12] * m[4]));
    to->z = (float)(to->z - ((double)m[9] * m[13] + (double)m[14] * m[10] + (double)m[12] * m[8]));
}

void AtMatrixInverseTransformDirection(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {
    const float* m = matrix->m;
    to->x = (float)((double)m[0] * from->x);
    to->y = (float)((double)m[4] * from->x);
    to->z = (float)((double)m[8] * from->x);
    to->x = (float)((double)m[1] * from->y + to->x);
    to->y = (float)((double)m[5] * from->y + to->y);
    to->z = (float)((double)m[9] * from->y + to->z);
    to->x = (float)((double)m[2] * from->z + to->x);
    to->y = (float)((double)m[6] * from->z + to->y);
    to->z = (float)((double)m[10] * from->z + to->z);
}

void AtMatrixScaleAxes(AtMatrix* m, const AtVector3* scale) {
    // SSE: each of the first three rows, all four of its elements, by one of scale's.
    const float x = scale->x, y = scale->y, z = scale->z;
    for (int j = 0; j < 4; j++) {
        m->m[j] = x * m->m[j];
    }
    for (int j = 4; j < 8; j++) {
        m->m[j] = y * m->m[j];
    }
    for (int j = 8; j < 12; j++) {
        m->m[j] = z * m->m[j];
    }
}

void AtMatrixTranspose(AtMatrix* m) {
    const AtMatrix was = *m;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m->m[i * 4 + j] = was.m[j * 4 + i];
        }
    }
}

void AtMatrixScaleColumnX(AtMatrix* m, float scale) {
    m->m[0] = scale * m->m[0];
    m->m[4] = scale * m->m[4];
    m->m[8] = scale * m->m[8];
    m->m[12] = scale * m->m[12];
}

void AtMatrixScaleColumnY(AtMatrix* m, float scale) {
    m->m[1] = scale * m->m[1];
    m->m[5] = scale * m->m[5];
    m->m[9] = scale * m->m[9];
    m->m[13] = scale * m->m[13];
}

void AtMatrixInvert(AtMatrix* matrix) {
    // As SetInverse, in place, keeping the position's first two in registers; the fourth column is left. The two moved through registers are quieted, the others moved as words.
    float* m = matrix->m;
    const double p12 = m[12], p13 = m[13];
    m[12] = (float)-(p13 * m[1] + (double)m[14] * m[2] + p12 * m[0]);
    m[13] = (float)-(p13 * m[5] + (double)m[14] * m[6] + p12 * m[4]);
    m[14] = (float)-((double)m[10] * m[14] + p13 * m[9] + p12 * m[8]);
    const float m1 = m[1], m2 = m[2], m4 = m[4], m6 = m[6], m8 = m[8], m9 = m[9];
    m[1] = m4;
    m[2] = m8;
    m[4] = ThroughRegister(m1);
    m[6] = m9;
    m[8] = ThroughRegister(m2);
    m[9] = m6;
}

void AtMatrixTranslateAfter(AtMatrix* matrix, const AtVector3* by) {
    // Each row gains by times its fourth element: a translation applied after this matrix.
    float* m = matrix->m;
    for (int i = 0; i < 4; i++) {
        const int r = i * 4;
        m[r] = (float)((double)m[r + 3] * by->x + m[r]);
        m[r + 1] = (float)((double)m[r + 3] * by->y + m[r + 1]);
        m[r + 2] = (float)((double)m[r + 3] * by->z + m[r + 2]);
    }
}

void AtMatrixSetLookAtZY(AtMatrix* m, const AtVector3* direction, const AtVector3* up) {
    AtVector3 forward, above;
    LookAxes(direction, up, &forward, &above);
    const double sideX = (double)forward.z * above.y - (double)forward.y * above.z;
    m->m[4] = above.x;
    m->m[5] = above.y;
    m->m[6] = above.z;
    m->m[8] = forward.x;
    m->m[9] = forward.y;
    const double sideY = (double)forward.x * above.z - (double)forward.z * above.x;
    m->m[10] = forward.z;
    const double sideZ = (double)forward.y * above.x - (double)forward.x * above.y;
    m->m[0] = (float)sideX;
    m->m[1] = (float)sideY;
    m->m[2] = (float)sideZ;
}

void AtMatrixSetLookAtXY(AtMatrix* m, const AtVector3* direction, const AtVector3* up) {
    AtVector3 forward, above;
    LookAxes(direction, up, &forward, &above);
    const double sideX = (double)above.z * forward.y - (double)above.y * forward.z;
    m->m[1] = forward.y;
    m->m[2] = forward.z;
    const double sideY = (double)above.x * forward.z - (double)above.z * forward.x;
    m->m[4] = above.x;
    m->m[5] = above.y;
    m->m[6] = above.z;
    const float sideZ = (float)((double)above.y * forward.x - (double)above.x * forward.y);
    m->m[10] = sideZ;
    m->m[0] = forward.x;
    m->m[8] = (float)sideX;
    m->m[9] = (float)sideY;
}
