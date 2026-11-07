// A quaternion, x y z w. Original address cluster 0x00033570-0x00033BBF. All names are inferred; what a function does is read from its arithmetic.
// `double` in source stands for what the original keeps in an x87 register, as in at/AtVector3.h. The order of each sum is the original's.
#pragma once

#include "at/AtVector3.h"

class AtQuaternion {
public:
    void SetProduct(const AtQuaternion& a, const AtQuaternion& b);  // 0x00033570 writes as it goes, so neither may be this
    // 0x00033600. Between two, by the shorter way round, unnormalised. Exactly opposite or at a dot of 0 counts as the longer way.
    void SetLerp(const AtQuaternion& from, const AtQuaternion& to, float t);
    // 0x000336F0. From the rows of a rotation matrix.
    void SetFromRows(const AtVector3& row0, const AtVector3& row1, const AtVector3& row2);
    void SetReflection(const AtQuaternion& normal, const AtQuaternion& value);   // 0x00033870 w comes from value
    void Multiply(const AtQuaternion& other);                       // 0x000338C0
    void Normalize();                                               // 0x00033950 a zero one becomes (0, 0, 0, 1)
    void SetSlerp(const AtQuaternion& from, const AtQuaternion& to, float t);    // 0x000339D0 lerps when the angle is small
    void Rotate(AtVector3* vector) const;                           // 0x00033B00 (v * q) * conjugate(q)
    void SetAxisAngle(const AtVector3& axis, float angle);          // 0x00033B90
    void Read(AtIoTokenizer* tokenizer);                            // 0x00033BC0 in src/at/AtCoordSys.cpp

    float x;   // +0x00
    float y;   // +0x04
    float z;   // +0x08
    float w;   // +0x0C
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(AtQuaternion) == 16, "AtQuaternion");
#endif
