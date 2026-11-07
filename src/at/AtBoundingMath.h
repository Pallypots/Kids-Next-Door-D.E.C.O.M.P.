// Private to the bounding geometry's sources.
#pragma once

#include "at/AtBoundingGeometry.h"

extern "C" double sqrt(double value);
#pragma intrinsic(sqrt)

namespace AtBoundingMath {

inline double D(float value) { return value; }

// fabs through a store: the sign bit alone, so that -0 becomes 0, and a NaN keeps its bits.
inline float Abs(float value) {
    union { float f; unsigned int bits; } u;
    u.f = value;
    u.bits &= 0x7FFFFFFFu;
    return u.f;
}

// A point of other's, in the frame of a shape at center, given other's rotation and position relative to that shape. The original writes this out in each function, always the same way. The first of the three has been stored, and the
// others not yet: a function that goes on to add to them does so unrounded.
inline void Relative(const AtVector3* axes, const AtVector3& position, const AtVector3& center, const AtVector3& point, double* out) {
    const double toX = D(position.x) - center.x;
    const double toY = D(position.y) - center.y;
    const float toZ = (float)(D(position.z) - center.z);
    const double fromX0 = D(axes[0].x) * point.x;
    const double fromX1 = D(axes[0].y) * point.x;
    const float fromX2 = (float)(D(axes[0].z) * point.x);
    const float sum0 = (float)(D(axes[1].x) * point.y + fromX0);
    const double sum1 = D(axes[1].y) * point.y + fromX1;
    const double sum2 = D(axes[1].z) * point.y + fromX2;
    const float total0 = (float)(D(axes[2].x) * point.z + sum0);
    const float total1 = (float)(D(axes[2].y) * point.z + sum1);
    const float total2 = (float)(D(axes[2].z) * point.z + sum2);
    out[0] = (float)(toX + total0);
    out[1] = toY + total1;
    out[2] = D(toZ) + total2;
}

// 0x00042EF0 and 0x00042DC0 leave their results in st0, unrounded, and these functions use them so. AtVector3's own declare float, and would round: docs in the worklog.
inline double NormalizeHeld(AtVector3* v) {
    const double a = v->x, b = v->y, c = v->z;
    const double length = sqrt(a * a + b * b + c * c);
    if (length <= D(5e-6f)) {
        v->x = 1.0f;
        v->y = 0.0f;
        v->z = 0.0f;
        return 0.0;
    }
    const double scale = 1.0 / length;
    v->x = (float)(scale * v->x);
    v->y = (float)(scale * v->y);
    v->z = (float)(scale * v->z);
    return length;
}

inline double DistanceSquaredHeld(const AtVector3& a, const AtVector3& b) {
    const double dx = D(b.x) - a.x;
    const double dy = D(b.y) - a.y;
    const double dz = D(b.z) - a.z;
    return dz * dz + dy * dy + dx * dx;
}

}  // namespace AtBoundingMath
