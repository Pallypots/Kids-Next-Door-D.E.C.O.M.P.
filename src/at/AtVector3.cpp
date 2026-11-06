// AtVector3. Original address cluster 0x00042D80-0x0004340F.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtVector3.h"

#include "at/AtIoTokenizer.h"

extern "C" double sqrt(double value);
#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

namespace {
const float kShortest = 5.0e-6f;  // 0x00272AC8, 0x36A7C5AC. Checked against the bytes by tests/oracle/oracle_vec.cpp.
}

void AtVector3::Lerp(const AtVector3& from, const AtVector3& to, float t) {
    x = (float)(((double)to.x - from.x) * t + from.x);
    y = (float)(((double)to.y - from.y) * t + from.y);
    z = (float)(((double)to.z - from.z) * t + from.z);
}

float AtVector3::DistanceSquared(const AtVector3& other) const {
    const double dx = (double)other.x - x;
    const double dy = (double)other.y - y;
    const double dz = (double)other.z - z;
    return (float)(dz * dz + dy * dy + dx * dx);
}

void AtVector3::Read(AtIoTokenizer* tokenizer) {
    x = tokenizer->ExpectReal();
    y = tokenizer->ExpectReal();
    z = tokenizer->ExpectReal();
}

void AtVector3::Normalize() {
    const double a = x, b = y, c = z;
    const double length = sqrt(a * a + b * b + c * c);
    if (length <= kShortest) {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return;
    }
    const double scale = 1.0 / length;
    x = (float)(scale * x);
    y = (float)(scale * y);
    z = (float)(scale * z);
}

void AtVector3::SetNormalized(const AtVector3& from) {
    const double a = from.x, b = from.y, c = from.z;
    const double length = sqrt(a * a + b * b + c * c);
    if (length <= kShortest) {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return;
    }
    const double scale = 1.0 / length;
    x = (float)(scale * from.x);
    y = (float)(scale * from.y);
    z = (float)(scale * from.z);
}

float AtVector3::NormalizeReturningLength() {
    const double a = x, b = y, c = z;
    const double length = sqrt(a * a + b * b + c * c);
    if (length <= kShortest) {
        x = 1.0f;
        y = 0.0f;
        z = 0.0f;
        return 0.0f;
    }
    const double scale = 1.0 / length;
    x = (float)(scale * x);
    y = (float)(scale * y);
    z = (float)(scale * z);
    return (float)length;
}

float AtVector3::Distance(const AtVector3& other) const {
    const double dx = (double)other.x - x;
    const double dy = (double)other.y - y;
    const double dz = (double)other.z - z;
    return (float)sqrt(dz * dz + dy * dy + dx * dx);
}

void AtVector3::Rotate(const AtVector3& axis, float angle) {
    if (angle == 0.0f) {
        return;
    }
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    const float oldX = x;
    const float oldY = y;
    const double d = AtOneMinusCos(angle);
    const double dax = d * axis.x;
    const double day = d * axis.y;
    const float xy = (float)(dax * axis.y);
    const float xz = (float)(dax * axis.z);
    const float yz = (float)(day * axis.z);
    const float yy = (float)(day * axis.y);
    const float zz = (float)(d * axis.z * axis.z);
    const float sx = (float)(s * axis.x);
    const float sy = (float)(s * axis.y);
    const float sz = (float)(s * axis.z);
    x = (float)((dax * axis.x + c) * x + ((double)xz - sy) * z + ((double)sz + xy) * y);
    y = (float)(((double)sx + yz) * z + ((double)xy - sz) * oldX + ((double)yy + c) * y);
    z = (float)(((double)zz + c) * z + ((double)yz - sx) * oldY + ((double)sy + xz) * oldX);
}

void AtVector3::SetRotated(const AtVector3& axis, float angle, const AtVector3& from) {
    if (angle == 0.0f) {
        x = from.x;
        y = from.y;
        z = from.z;
        return;
    }
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    const double d = AtOneMinusCos(angle);
    const double dax = d * axis.x;
    const double day = d * axis.y;
    const float xy = (float)(dax * axis.y);
    const float xz = (float)(dax * axis.z);
    const float yz = (float)(day * axis.z);
    const float yy = (float)(day * axis.y);
    const float zz = (float)(d * axis.z * axis.z);
    const float sx = (float)(s * axis.x);
    const float sy = (float)(s * axis.y);
    const float sz = (float)(s * axis.z);
    x = (float)((dax * axis.x + c) * from.x + ((double)xz - sy) * from.z + ((double)sz + xy) * from.y);
    y = (float)(((double)xy - sz) * from.x + ((double)sx + yz) * from.z + ((double)yy + c) * from.y);
    z = (float)(((double)sy + xz) * from.x + ((double)zz + c) * from.z + ((double)yz - sx) * from.y);
}

void AtVector3::RotateZ(float angle) {
    const double a = x, b = y;
    const double s = AtSin(angle);
    const double c = AtCos(angle);
    x = (float)(c * a - s * b);
    y = (float)(c * b + s * a);
}

void AtVector3::Project(const AtVector3& direction) {
    const double a = direction.x, b = direction.y, e = direction.z;
    const double lengthSquared = a * a + b * b + e * e;
    if (lengthSquared == 0.0) {
        y = 0.0f;
        z = 0.0f;
        return;
    }
    const double scale = ((double)z * direction.z + (double)direction.y * y + (double)direction.x * x) / lengthSquared;
    x = (float)(scale * direction.x);
    y = (float)(scale * direction.y);
    z = (float)(scale * direction.z);
}

void AtVector3::SetProjected(const AtVector3& from, const AtVector3& direction) {
    const double a = direction.x, b = direction.y, e = direction.z;
    const double lengthSquared = a * a + b * b + e * e;
    if (lengthSquared == 0.0) {
        x = from.x;
        y = 0.0f;
        z = 0.0f;
        return;
    }
    const double scale = ((double)from.y * direction.y + (double)from.z * direction.z + (double)from.x * direction.x) / lengthSquared;
    x = (float)(scale * direction.x);
    y = (float)(scale * direction.y);
    z = (float)(scale * direction.z);
}

void AtVector3::Reject(const AtVector3& direction) {
    const double a = direction.x, b = direction.y, e = direction.z;
    const double lengthSquared = a * a + b * b + e * e;
    if (lengthSquared == 0.0) {
        x = 0.0f;
        return;
    }
    const double scale = ((double)direction.y * y + (double)z * direction.z + (double)direction.x * x) / lengthSquared;
    const double alongX = scale * direction.x;
    const float alongY = (float)(scale * direction.y);
    const float alongZ = (float)(scale * direction.z);
    x = (float)(x - alongX);
    y = (float)((double)y - alongY);
    z = (float)((double)z - alongZ);
}

void AtVector3::SetRejected(const AtVector3& from, const AtVector3& direction) {
    const double a = direction.x, b = direction.y, e = direction.z;
    const double lengthSquared = a * a + b * b + e * e;
    if (lengthSquared == 0.0) {
        x = 0.0f;
        y = from.y;
        z = from.z;
        return;
    }
    const double scale = ((double)from.x * direction.x + (double)from.z * direction.z + (double)from.y * direction.y) / lengthSquared;
    const double alongX = scale * direction.x;
    const float alongY = (float)(scale * direction.y);
    const float alongZ = (float)(scale * direction.z);
    x = (float)(from.x - alongX);
    y = (float)((double)from.y - alongY);
    z = (float)((double)from.z - alongZ);
}
