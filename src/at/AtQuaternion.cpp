// AtQuaternion. Original address cluster 0x00033570-0x00033BBF.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtQuaternion.h"

extern "C" double sqrt(double value);
#ifdef _MSC_VER
#pragma intrinsic(sqrt)
#endif

namespace {
const float kSmallAngle = 0.01f;  // 0x002719C8, 0x3C23D70A: checked against the bytes by tests/oracle/oracle_qua.cpp.

inline double D(float v) {
    return v;
}
}  // namespace

void AtQuaternion::SetProduct(const AtQuaternion& a, const AtQuaternion& b) {
    x = (float)(D(a.x) * b.w + D(b.z) * a.y + D(b.x) * a.w - D(b.y) * a.z);
    y = (float)(D(b.y) * a.w + D(b.x) * a.z + D(a.y) * b.w - D(a.x) * b.z);
    z = (float)(D(a.x) * b.y + D(b.z) * a.w + D(a.z) * b.w - D(b.x) * a.y);
    w = (float)(D(a.w) * b.w - (D(a.x) * b.x + D(b.z) * a.z + D(b.y) * a.y));
}

void AtQuaternion::SetLerp(const AtQuaternion& from, const AtQuaternion& to, float t) {
    const double dot = D(to.w) * from.w + D(to.z) * from.z + D(from.y) * to.y + D(from.x) * to.x;
    if (dot > 0.0) {
        x = (float)((D(to.x) - from.x) * t + from.x);
        y = (float)((D(to.y) - from.y) * t + from.y);
        z = (float)((D(to.z) - from.z) * t + from.z);
        w = (float)((D(to.w) - from.w) * t + from.w);
        return;
    }
    x = (float)(from.x - (D(from.x) + to.x) * t);
    y = (float)(from.y - (D(from.y) + to.y) * t);
    z = (float)(from.z - (D(to.z) + from.z) * t);
    w = (float)(from.w - (D(to.w) + from.w) * t);
}

void AtQuaternion::SetFromRows(const AtVector3& row0, const AtVector3& row1, const AtVector3& row2) {
    const double sum12 = D(row2.z) + row1.y;
    const float storedSum12 = (float)sum12;  // The original stores it, and later reads the stored one.
    const double trace = sum12 + row0.x;
    if (trace > 0.0) {
        const double root = sqrt(trace + 1.0);
        w = (float)(0.5f * root);
        const double scale = 0.5f / root;
        x = (float)((D(row2.y) - row1.z) * scale);
        y = (float)((D(row0.z) - row2.x) * scale);
        z = (float)((D(row1.x) - row0.y) * scale);
        return;
    }
    int largest = 0;
    if (row1.y > row0.x) {
        largest = 1;
        if (row2.z > row1.y) {
            largest = 2;
        }
    } else if (row2.z > row0.x) {
        largest = 2;
    }
    if (largest == 2) {
        const double root = sqrt(row2.z - (D(row0.x) + row1.y) + 1.0);
        z = (float)(0.5f * root);
        const double scale = 0.5f / root;
        w = (float)((D(row1.x) - row0.y) * scale);
        x = (float)((D(row0.z) + row2.x) * scale);
        y = (float)((D(row1.z) + row2.y) * scale);
    } else if (largest == 1) {
        const double root = sqrt(row1.y - (D(row0.x) + row2.z) + 1.0);
        y = (float)(0.5f * root);
        const double scale = 0.5f / root;
        w = (float)((D(row0.z) - row2.x) * scale);
        z = (float)((D(row1.z) + row2.y) * scale);
        x = (float)((D(row0.y) + row1.x) * scale);
    } else {
        const double root = sqrt(D(row0.x) - storedSum12 + 1.0);
        x = (float)(0.5f * root);
        const double scale = 0.5f / root;
        w = (float)((D(row2.y) - row1.z) * scale);
        y = (float)((D(row0.y) + row1.x) * scale);
        z = (float)((D(row0.z) + row2.x) * scale);
    }
}

void AtQuaternion::SetReflection(const AtQuaternion& normal, const AtQuaternion& value) {
    const double scale = -((D(normal.x) * value.x + D(value.z) * normal.z + D(value.y) * normal.y) * 2.0);
    x = (float)-(scale * normal.x + value.x);
    y = (float)-(scale * normal.y + value.y);
    z = (float)-(scale * normal.z + value.z);
    w = value.w;
}

void AtQuaternion::Multiply(const AtQuaternion& other) {
    const double a = x, b = y, c = z, d = w;
    x = (float)(c * other.y + d * other.x + a * other.w - b * other.z);
    y = (float)(d * other.y + a * other.z + b * other.w - c * other.x);
    z = (float)(d * other.z + b * other.x + c * other.w - a * other.y);
    w = (float)(d * other.w - (b * other.y + c * other.z + a * other.x));
}

void AtQuaternion::Normalize() {
    const double a = x, b = y, c = z, d = w;
    const double lengthSquared = a * a + b * b + c * c + d * d;
    if (lengthSquared == 0.0) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 1.0f;
        return;
    }
    const double scale = 1.0 / sqrt(lengthSquared);
    x = (float)(scale * x);
    y = (float)(scale * y);
    z = (float)(scale * z);
    w = (float)(scale * w);
}

void AtQuaternion::SetSlerp(const AtQuaternion& from, const AtQuaternion& to, float t) {
    const double dot = D(to.w) * from.w + D(to.z) * from.z + D(from.x) * to.x + D(to.y) * from.y;
    float cosine = (float)dot;
    AtQuaternion target;
    if (dot < 0.0) {
        cosine = -cosine;
        target.x = -to.x;
        target.y = -to.y;
        target.z = -to.z;
        target.w = -to.w;
    } else {
        target = to;
    }
    double k0;
    double k1;
    if (1.0 - D(cosine) > kSmallAngle) {
        const double exactAngle = AtAcos(cosine);
        const float angle = (float)exactAngle;  // The original stores this, then takes the sine of what is still in the register.
        const float scale = (float)(1.0 / AtSin(exactAngle));
        k0 = AtSin((1.0 - D(t)) * angle) * scale;
        k1 = AtSin(D(angle) * t) * scale;
    } else {
        k0 = 1.0 - D(t);
        k1 = t;
    }
    x = (float)(k0 * from.x + k1 * target.x);
    y = (float)(k0 * from.y + k1 * target.y);
    z = (float)(k1 * target.z + k0 * from.z);
    w = (float)(k1 * target.w + k0 * from.w);
}

void AtQuaternion::Rotate(AtVector3* vector) const {
    AtQuaternion pure;
    pure.x = vector->x;
    pure.y = vector->y;
    pure.z = vector->z;
    pure.w = 0.0f;
    AtQuaternion result;
    result.SetProduct(pure, *this);
    pure.x = -x;
    pure.y = -y;
    pure.z = -z;
    pure.w = w;
    result.Multiply(pure);
    vector->x = result.x;
    vector->y = result.y;
    vector->z = result.z;
}

void AtQuaternion::SetAxisAngle(const AtVector3& axis, float angle) {
    const double half = D(angle) * 0.5f;
    const double s = AtSin(half);
    w = (float)AtCos(half);
    x = (float)(s * axis.x);
    y = (float)(s * axis.y);
    z = (float)(s * axis.z);
}
