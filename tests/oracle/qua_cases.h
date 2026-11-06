// Inputs for the quaternion tests, the same on every host. Used by oracle_qua.cpp and tests/native/test_at_quaternion.cpp.
#pragma once

#include "at/AtQuaternion.h"
#include "vec_cases.h"

namespace qua_cases {

using vec_cases::Rng;
using vec_cases::pick;

inline AtQuaternion any(Rng& r, bool specials) {
    AtQuaternion q;
    q.x = pick(r, specials);
    q.y = pick(r, specials);
    q.z = pick(r, specials);
    q.w = pick(r, specials);
    return q;
}

inline AtVector3 any3(Rng& r, bool specials) {
    AtVector3 v;
    v.x = pick(r, specials);
    v.y = pick(r, specials);
    v.z = pick(r, specials);
    return v;
}

// Near a unit one, as the game's are.
inline AtQuaternion near_unit(Rng& r) {
    AtQuaternion q;
    q.x = (float)((double)(int32_t)r.next() / 2147483648.0);
    q.y = (float)((double)(int32_t)r.next() / 2147483648.0);
    q.z = (float)((double)(int32_t)r.next() / 2147483648.0);
    q.w = (float)((double)(int32_t)r.next() / 2147483648.0);
    q.Normalize();
    return q;
}

// A rotation matrix from a quaternion, by the textbook. Its largest diagonal entry, or none, decides the branch taken.
inline void rows_of(const AtQuaternion& q, AtVector3* a, AtVector3* b, AtVector3* c) {
    const double x = q.x, y = q.y, z = q.z, w = q.w;
    a->x = (float)(1 - 2 * (y * y + z * z));
    a->y = (float)(2 * (x * y + z * w));
    a->z = (float)(2 * (x * z - y * w));
    b->x = (float)(2 * (x * y - z * w));
    b->y = (float)(1 - 2 * (x * x + z * z));
    b->z = (float)(2 * (y * z + x * w));
    c->x = (float)(2 * (x * z + y * w));
    c->y = (float)(2 * (y * z - x * w));
    c->z = (float)(1 - 2 * (x * x + y * y));
}

}  // namespace qua_cases
