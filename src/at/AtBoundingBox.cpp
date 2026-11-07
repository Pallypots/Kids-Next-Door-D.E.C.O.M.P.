// See include/at/AtBoundingGeometry.h. The shape of a box and all its arithmetic. Original address cluster 0x0005CDB0-0x0005ED62.
//
// The order of the sums below is the original's, and differs from one test to the next: see at/AtVector3.h.
#include "AtBoundingMath.h"

using namespace AtBoundingMath;

namespace {

// Absolute values of three column vectors: a.xy is the y of column x.
struct AbsoluteAxes {
    float xx, xy, xz, yx, yy, yz, zx, zy, zz;
    explicit AbsoluteAxes(const AtVector3* axes) {
        xx = Abs(axes[0].x);
        yx = Abs(axes[1].x);
        zx = Abs(axes[2].x);
        xy = Abs(axes[0].y);
        yy = Abs(axes[1].y);
        zy = Abs(axes[2].y);
        xz = Abs(axes[0].z);
        yz = Abs(axes[1].z);
        zz = Abs(axes[2].z);
    }
};

// |n . t|, stored on the way.
float DotAbs(const AtVector3& abs, const AtVector3& t) {
    return Abs((float)(D(abs.z) * t.z + D(abs.y) * t.y + D(abs.x) * t.x));
}
// |a * b - c * d|, stored on the way.
float CrossAbs(float a, float b, float c, float d) {
    return Abs((float)(D(a) * b - D(c) * d));
}

AtVector3 AbsOf(const AtVector3& v) {
    AtVector3 r;
    r.x = Abs(v.x);
    r.y = Abs(v.y);
    r.z = Abs(v.z);
    return r;
}

const float kNearlyOne = 5e-6f;

// An edge's cross product comes to nothing when a unit axis is nearly along one of the box's.
bool NotAlong(float absolute) {
    const double difference = D(absolute) - 1.0;
    return !(difference >= -D(kNearlyOne)) || !(difference <= D(kNearlyOne));
}

// The deepest separation so far, and its extents. The normal is the caller's to write.
struct Deepest {
    float depth;
    float* extent;
    float* otherExtent;

    bool Face(double depthHere, float mine, float theirs) {
        if (!(depthHere > depth)) {
            return false;
        }
        depth = (float)depthHere;
        *extent = mine;
        *otherExtent = theirs;
        return true;
    }
    // An edge's axis, divided through by its length. Its reciprocal comes back for the normal.
    bool Edge(float absolute, double mine, float theirs, float a, float b, float* inverse, double* depthHere) {
        const double length = sqrt(D(a) * a + D(b) * b);
        *inverse = (float)(1.0 / length);
        *depthHere = (D(absolute) - (D(theirs) + mine)) * *inverse;
        if (!(*depthHere > depth)) {
            return false;
        }
        depth = (float)*depthHere;
        *extent = (float)(D(*inverse) * mine);
        *otherExtent = (float)(D(*inverse) * theirs);
        return true;
    }
};

void SetNormal(AtVector3* normal, float x, float y, float z) {
    normal->x = x;
    normal->y = y;
    normal->z = z;
}

// One axis of Clip. keep and set for the four ways a face is crossed, in the order the code meets them.
struct ClipFlags {
    unsigned int startAtMaxKeep, startAtMaxSet, endAtMinKeep, endAtMinSet, endAtMaxKeep, endAtMaxSet, startAtMinKeep, startAtMinSet;
};

// False when the segment misses. In Z, when a backward segment leaves through the low face, the original has the two ends the other way round, and so its clipped end is measured from the end: swappedEnd.
bool ClipAxis(const AtBoundingBox* box, int axis, const AtVector3* start, const AtVector3* end, AtVector3* clippedStart, AtVector3* clippedEnd, float* tStart, float* tEnd, const ClipFlags& f, bool swappedEnd, unsigned int* flags) {
    const float center = (&box->m_center.x)[axis];
    const float half = (&box->m_half.x)[axis];
    const float from = (&start->x)[axis];
    const float to = (&end->x)[axis];
    float& s = (&clippedStart->x)[axis];
    float& e = (&clippedEnd->x)[axis];
    const float high = (float)(D(center) + half);
    if (s > e) {
        if (s >= high) {
            if (!(e < high)) {
                return false;
            }
            const float t = (float)((D(high) - from) / (D(to) - from));
            *tStart = t;
            clippedStart->Lerp(*start, *end, t);
            *flags = (*flags & f.startAtMaxKeep) | f.startAtMaxSet;
        }
        const float low = (float)(D(center) - half);
        if (!(s >= low)) {
            return false;
        }
        if (e < low) {
            const float t = (float)((D(low) - from) / (D(to) - from));
            *tEnd = t;
            if (swappedEnd) {
                clippedEnd->Lerp(*end, *start, t);
            } else {
                clippedEnd->Lerp(*start, *end, t);
            }
            *flags = (*flags & f.endAtMinKeep) | f.endAtMinSet;
        }
    } else {
        if (e >= high) {
            if (!(s < high)) {
                return false;
            }
            const float t = (float)((D(high) - from) / (D(to) - from));
            *tEnd = t;
            clippedEnd->Lerp(*start, *end, t);
            *flags = (*flags & f.endAtMaxKeep) | f.endAtMaxSet;
        }
        const float low = (float)(D(center) - half);
        if (!(e >= low)) {
            return false;
        }
        if (s < low) {
            const float t = (float)((D(low) - from) / (D(to) - from));
            *tStart = t;
            clippedStart->Lerp(*start, *end, t);
            *flags = (*flags & f.startAtMinKeep) | f.startAtMinSet;
        }
    }
    return true;
}

const unsigned int kAll = 0xFFFFFFFFu;
const ClipFlags kClipX = {kAll, 0x001, kAll, 0x200, kAll, 0x040, kAll, 0x008};
const ClipFlags kClipY = {0xFFFFFFC2u, 0x002, 0xFFFFF43Fu, 0x400, 0xFFFFF0BFu, 0x080, 0xFFFFFFD0u, 0x010};
const ClipFlags kClipZ = {0xFFFFFFC4u, 0x004, 0xFFFFF83Fu, 0x800, 0xFFFFF13Fu, 0x100, 0xFFFFFFE0u, 0x020};

}  // namespace

AtBoundingBox::AtBoundingBox() {
    m_center.x = 0.0f;
    m_center.y = 0.0f;
    m_center.z = 0.0f;
    m_half.x = 0.0f;
    m_half.y = 0.0f;
    m_half.z = 0.0f;
}

void AtBoundingBox::Read(AtIoTokenizer* tokenizer) {
    m_center.Read(tokenizer);
    m_half.x = Abs(tokenizer->ExpectReal());
    m_half.y = Abs(tokenizer->ExpectReal());
    m_half.z = Abs(tokenizer->ExpectReal());
}

int AtBoundingBox::Overlaps(const AtBoundingBox* other, const AtVector3* rotation, const AtVector3* position) const {
    double relative[3];
    Relative(rotation, *position, m_center, other->m_center, relative);
    AtVector3 t;
    t.x = (float)relative[0];
    t.y = (float)relative[1];
    t.z = (float)relative[2];
    const AtVector3& h = m_half;
    const AtVector3& o = other->m_half;
    const AbsoluteAxes r(rotation);

    // This box's faces.
    if (D(r.xx) * o.x + D(r.yx) * o.y + D(r.zx) * o.z + h.x < Abs(t.x)) {
        return 0;
    }
    if (D(r.xy) * o.x + D(r.yy) * o.y + D(r.zy) * o.z + h.y < Abs(t.y)) {
        return 0;
    }
    if (D(r.xz) * o.x + D(r.yz) * o.y + D(r.zz) * o.z + h.z < Abs(t.z)) {
        return 0;
    }

    // The other's first face, and that axis crossed with this box's.
    AtVector3 n;
    n.SetNormalized(rotation[0]);
    AtVector3 a = AbsOf(n);
    if (D(a.y) * h.y + D(r.xz) * o.x + D(r.xy) * o.x + D(r.xx) * o.x + D(a.z) * h.z + D(a.x) * h.x < DotAbs(a, t)) {
        return 0;
    }
    if (D(a.z) * h.y + D(a.y) * h.z + D(r.zx) * o.y + D(r.yx) * o.z < CrossAbs(n.y, t.z, n.z, t.y)) {
        return 0;
    }
    if (D(a.z) * h.x + D(a.x) * h.z + D(r.zy) * o.y + D(r.yy) * o.z < CrossAbs(n.z, t.x, n.x, t.z)) {
        return 0;
    }
    if (D(a.x) * h.y + D(a.y) * h.x + D(r.zz) * o.y + D(r.yz) * o.z < CrossAbs(n.x, t.y, n.y, t.x)) {
        return 0;
    }

    // The second.
    n.SetNormalized(rotation[1]);
    a = AbsOf(n);
    if (D(a.y) * h.y + D(a.z) * h.z + D(a.x) * h.x + D(r.yz) * o.y + D(r.yy) * o.y + D(r.yx) * o.y < DotAbs(a, t)) {
        return 0;
    }
    if (D(a.z) * h.y + D(r.zx) * o.x + D(a.y) * h.z + D(r.xx) * o.z < CrossAbs(n.y, t.z, n.z, t.y)) {
        return 0;
    }
    if (D(r.zy) * o.x + D(a.z) * h.x + D(a.x) * h.z + D(r.xy) * o.z < CrossAbs(n.z, t.x, n.x, t.z)) {
        return 0;
    }
    if (D(a.x) * h.y + D(r.zz) * o.x + D(a.y) * h.x + D(r.xz) * o.z < CrossAbs(n.x, t.y, n.y, t.x)) {
        return 0;
    }

    // The third.
    n.SetNormalized(rotation[2]);
    a = AbsOf(n);
    if (D(a.y) * h.y + D(a.z) * h.z + D(a.x) * h.x + D(r.zz) * o.z + D(r.zy) * o.z + D(r.zx) * o.z < DotAbs(a, t)) {
        return 0;
    }
    if (D(a.z) * h.y + D(r.yx) * o.x + D(a.y) * h.z + D(r.xx) * o.y < CrossAbs(n.y, t.z, n.z, t.y)) {
        return 0;
    }
    if (D(r.yy) * o.x + D(a.z) * h.x + D(a.x) * h.z + D(r.xy) * o.y < CrossAbs(n.z, t.x, n.x, t.z)) {
        return 0;
    }
    if (D(a.x) * h.y + D(r.yz) * o.x + D(a.y) * h.x + D(r.xz) * o.y < CrossAbs(n.x, t.y, n.y, t.x)) {
        return 0;
    }
    return 1;
}

int AtBoundingBox::OverlapsSphere(const AtSceneSphere* sphere) const {
    const double dx = D(Abs(sphere->center.x)) - m_half.x;
    const float dy = (float)(D(Abs(sphere->center.y)) - m_half.y);
    const float dz = (float)(D(Abs(sphere->center.z)) - m_half.z);
    const double radius = sphere->radius;
    if (dx <= 0.0) {
        if (dy <= 0.0f) {
            if (dz <= 0.0f) {
                return 1;
            }
            return dz <= sphere->radius ? 1 : 0;
        }
        if (dz <= 0.0f) {
            return dy <= sphere->radius ? 1 : 0;
        }
        return radius * radius >= D(dz) * dz + D(dy) * dy ? 1 : 0;
    }
    if (dy <= 0.0f) {
        if (dz <= 0.0f) {
            return dx <= radius ? 1 : 0;
        }
        return radius * radius >= D(dz) * dz + dx * dx ? 1 : 0;
    }
    if (dz <= 0.0f) {
        return radius * radius >= D(dy) * dy + dx * dx ? 1 : 0;
    }
    return radius * radius >= D(dz) * dz + D(dy) * dy + dx * dx ? 1 : 0;
}

int AtBoundingBox::OverlapsSegment(const AtVector3* direction, const AtVector3* middle, float halfLength) const {
    AtVector3 d;
    d.x = (float)(D(middle->x) - m_center.x);
    d.y = (float)(D(middle->y) - m_center.y);
    d.z = (float)(D(middle->z) - m_center.z);
    const float ax = Abs(direction->x);
    if (D(ax) * halfLength + m_half.x < Abs(d.x)) {
        return 0;
    }
    const float ay = Abs(direction->y);
    if (D(ay) * halfLength + m_half.y < Abs(d.y)) {
        return 0;
    }
    const float az = Abs(direction->z);
    if (D(az) * halfLength + m_half.z < Abs(d.z)) {
        return 0;
    }
    float reach = (float)(D(az) * m_half.y + D(ay) * m_half.z);
    if (CrossAbs(d.y, direction->z, d.z, direction->y) > reach) {
        return 0;
    }
    reach = (float)(D(ax) * m_half.z + D(az) * m_half.x);
    if (CrossAbs(d.z, direction->x, d.x, direction->z) > reach) {
        return 0;
    }
    reach = (float)(D(ax) * m_half.y + D(ay) * m_half.x);
    if (CrossAbs(d.x, direction->y, d.y, direction->x) > reach) {
        return 0;
    }
    return 1;
}

unsigned int AtBoundingBox::Clip(const AtVector3* start, const AtVector3* end, AtVector3* clippedStart, AtVector3* clippedEnd, float* tStart, float* tEnd) const {
    *clippedStart = *start;
    *clippedEnd = *end;
    unsigned int flags = 0;
    if (!ClipAxis(this, 0, start, end, clippedStart, clippedEnd, tStart, tEnd, kClipX, false, &flags)) {
        return 0;
    }
    if (!ClipAxis(this, 1, start, end, clippedStart, clippedEnd, tStart, tEnd, kClipY, false, &flags)) {
        return 0;
    }
    if (!ClipAxis(this, 2, start, end, clippedStart, clippedEnd, tStart, tEnd, kClipZ, true, &flags)) {
        return 0;
    }
    return flags;
}

double AtBoundingBox::Penetration(const AtBoundingBox* other, const AtVector3* rotation, const AtVector3* position, AtVector3* normal, float* extent, float* otherExtent) const {
    double relative[3];
    Relative(rotation, *position, m_center, other->m_center, relative);
    AtVector3 t;
    t.x = (float)relative[0];
    t.y = (float)relative[1];
    t.z = (float)relative[2];
    const AtVector3& h = m_half;
    const AtVector3& o = other->m_half;
    const AbsoluteAxes r(rotation);
    Deepest best = {-3.4028234663852886e+38f, extent, otherExtent};
    double reach, depth, mine;
    float theirs, inverse;

    // This box's faces.
    reach = D(r.zx) * o.z + D(r.xx) * o.x + D(r.yx) * o.y;
    if (best.Face(D(Abs(t.x)) - (reach + h.x), h.x, (float)reach)) {
        SetNormal(normal, 1.0f, 0.0f, 0.0f);
    }
    reach = D(r.zy) * o.z + D(r.xy) * o.x + D(r.yy) * o.y;
    if (best.Face(D(Abs(t.y)) - (reach + h.y), h.y, (float)reach)) {
        SetNormal(normal, 0.0f, 1.0f, 0.0f);
    }
    reach = D(r.xz) * o.x + D(r.yz) * o.y + D(r.zz) * o.z;
    if (best.Face(D(Abs(t.z)) - (reach + h.z), h.z, (float)reach)) {
        SetNormal(normal, 0.0f, 0.0f, 1.0f);
    }

    // The other's first face, and that axis crossed with this box's.
    AtVector3 n;
    n.SetNormalized(rotation[0]);
    AtVector3 a = AbsOf(n);
    mine = D(a.y) * h.y + D(a.z) * h.z + D(a.x) * h.x;
    reach = D(r.xz) * o.x + D(r.xy) * o.x + D(r.xx) * o.x;
    if (best.Face(D(DotAbs(a, t)) - (reach + mine), (float)mine, o.x)) {
        *normal = n;
    }
    if (NotAlong(a.x)) {
        mine = D(a.z) * h.y + D(a.y) * h.z;
        theirs = (float)(D(r.yx) * o.z + D(r.zx) * o.y);
        if (best.Edge(CrossAbs(n.y, t.z, n.z, t.y), mine, theirs, n.z, n.y, &inverse, &depth)) {
            SetNormal(normal, 0.0f, (float)-(D(inverse) * n.z), (float)(D(inverse) * n.y));
        }
    }
    if (NotAlong(a.y)) {
        mine = D(a.x) * h.z + D(a.z) * h.x;
        theirs = (float)(D(r.yy) * o.z + D(r.zy) * o.y);
        if (best.Edge(CrossAbs(n.z, t.x, n.x, t.z), mine, theirs, n.z, n.x, &inverse, &depth)) {
            SetNormal(normal, (float)(D(inverse) * n.z), 0.0f, (float)-(D(inverse) * n.x));
        }
    }
    if (NotAlong(a.z)) {
        mine = D(a.x) * h.y + D(a.y) * h.x;
        theirs = (float)(D(r.yz) * o.z + D(r.zz) * o.y);
        if (best.Edge(CrossAbs(n.x, t.y, n.y, t.x), mine, theirs, n.y, n.x, &inverse, &depth)) {
            SetNormal(normal, (float)-(D(inverse) * n.y), (float)(D(inverse) * n.x), 0.0f);
        }
    }

    // The second.
    n.SetNormalized(rotation[1]);
    a = AbsOf(n);
    mine = D(a.y) * h.y + D(a.z) * h.z + D(a.x) * h.x;
    reach = D(r.yz) * o.y + D(r.yy) * o.y + D(r.yx) * o.y;
    if (best.Face(D(DotAbs(a, t)) - (reach + mine), (float)mine, o.y)) {
        *normal = n;
    }
    if (NotAlong(a.x)) {
        mine = D(a.z) * h.y + D(a.y) * h.z;
        theirs = (float)(D(r.xx) * o.z + D(r.zx) * o.x);
        if (best.Edge(CrossAbs(n.y, t.z, n.z, t.y), mine, theirs, n.z, n.y, &inverse, &depth)) {
            SetNormal(normal, 0.0f, (float)-(D(inverse) * n.z), (float)(D(inverse) * n.y));
        }
    }
    if (NotAlong(a.y)) {
        mine = D(a.x) * h.z + D(a.z) * h.x;
        theirs = (float)(D(r.xy) * o.z + D(r.zy) * o.x);
        if (best.Edge(CrossAbs(n.z, t.x, n.x, t.z), mine, theirs, n.z, n.x, &inverse, &depth)) {
            SetNormal(normal, (float)(D(inverse) * n.z), 0.0f, (float)-(D(inverse) * n.x));
        }
    }
    if (NotAlong(a.z)) {
        mine = D(a.x) * h.y + D(a.y) * h.x;
        theirs = (float)(D(r.xz) * o.z + D(r.zz) * o.x);
        if (best.Edge(CrossAbs(n.x, t.y, n.y, t.x), mine, theirs, n.y, n.x, &inverse, &depth)) {
            SetNormal(normal, (float)-(D(inverse) * n.y), (float)(D(inverse) * n.x), 0.0f);
        }
    }

    // The third.
    n.SetNormalized(rotation[2]);
    a = AbsOf(n);
    mine = D(a.y) * h.y + D(a.z) * h.z + D(a.x) * h.x;
    reach = D(r.zy) * o.z + D(r.zx) * o.z + D(r.zz) * o.z;
    if (best.Face(D(DotAbs(a, t)) - (reach + mine), (float)mine, o.z)) {
        *normal = n;
    }
    if (NotAlong(a.x)) {
        mine = D(a.z) * h.y + D(a.y) * h.z;
        theirs = (float)(D(r.yx) * o.x + D(r.xx) * o.y);
        if (best.Edge(CrossAbs(n.y, t.z, n.z, t.y), mine, theirs, n.z, n.y, &inverse, &depth)) {
            SetNormal(normal, 0.0f, (float)-(D(inverse) * n.z), (float)(D(inverse) * n.y));
        }
    }
    if (NotAlong(a.y)) {
        mine = D(a.x) * h.z + D(a.z) * h.x;
        theirs = (float)(D(r.yy) * o.x + D(r.xy) * o.y);
        if (best.Edge(CrossAbs(n.z, t.x, n.x, t.z), mine, theirs, n.z, n.x, &inverse, &depth)) {
            SetNormal(normal, (float)(D(inverse) * n.z), 0.0f, (float)-(D(inverse) * n.x));
        }
    }
    if (NotAlong(a.z)) {
        mine = D(a.x) * h.y + D(a.y) * h.x;
        theirs = (float)(D(r.yz) * o.x + D(r.xz) * o.y);
        if (best.Edge(CrossAbs(n.x, t.y, n.y, t.x), mine, theirs, n.y, n.x, &inverse, &depth)) {
            SetNormal(normal, (float)-(D(inverse) * n.y), (float)(D(inverse) * n.x), 0.0f);
            return depth;  // Only here does the result go back as it stood in the register, not as it was stored.
        }
    }
    return best.depth;
}

double AtBoundingBox::PenetrationSphere(const AtSceneSphere* sphere, AtVector3* normal, float* extent, float* otherExtent) const {
    const float absZ = Abs(sphere->center.z);
    const float absY = Abs(sphere->center.y);
    const float absX = Abs(sphere->center.x);
    float dx = (float)(D(absX) - m_half.x);
    float dy = (float)(D(absY) - m_half.y);
    const double dzHeld = D(absZ) - m_half.z;
    float dz = (float)dzHeld;
    *otherExtent = 0.0f;
    *extent = 0.0f;
    const double radius = sphere->radius;
    bool outsideBelow = false;
    double surface;
    if (dx <= 0.0f) {
        if (dy <= 0.0f) {
            if (dzHeld <= 0.0) {
                // Inside: out through the nearest face.
                dx = -dx;
                dy = -dy;
                dz = (float)-dzHeld;
                normal->x = 0.0f;
                normal->y = 0.0f;
                normal->z = 0.0f;
                if (dx < dy && !(dz < dx)) {
                    normal->x = (float)(-D(dx) - radius);
                } else if (dz < dy) {  // When dx < dy, to be here dz < dx as well, and so this holds.
                    normal->z = (float)(-D(dz) - radius);
                } else {
                    normal->y = (float)(-D(dy) - radius);
                }
                goto signs;
            }
            surface = dzHeld - radius;
            SetNormal(normal, 0.0f, 0.0f, (float)surface);
        } else if (dzHeld <= 0.0) {
            surface = D(dy) - radius;
            SetNormal(normal, 0.0f, (float)surface, 0.0f);
        } else {
            SetNormal(normal, 0.0f, dy, dz);
            normal->Normalize();
            surface = sqrt(D(dz) * dz + D(dy) * dy) - radius;
            goto scale;
        }
    } else if (dy <= 0.0f) {
        if (dzHeld <= 0.0) {
            surface = D(dx) - radius;
            SetNormal(normal, (float)surface, 0.0f, 0.0f);
        } else {
            SetNormal(normal, dx, 0.0f, dz);
            normal->Normalize();
            surface = sqrt(D(dz) * dz + D(dx) * dx) - radius;
            goto scale;
        }
    } else {
        if (dzHeld <= 0.0) {
            SetNormal(normal, dx, dy, 0.0f);
            normal->Normalize();
            surface = sqrt(D(dy) * dy + D(dx) * dx) - radius;
        } else {
            AtVector3 d;
            d.x = dx;
            d.y = dy;
            d.z = dz;
            normal->SetNormalized(d);
            surface = sqrt(D(dz) * dz + D(dy) * dy + D(dx) * dx) - radius;
        }
        goto scale;
    }
    outsideBelow = surface < 0.0;
    goto signs;
scale:
    normal->x = (float)(surface * normal->x);
    normal->y = (float)(surface * normal->y);
    normal->z = (float)(surface * normal->z);
    outsideBelow = surface < 0.0;
signs:
    if (sphere->center.x < absX) {
        normal->x = -normal->x;
    }
    if (sphere->center.y < absY) {
        normal->y = -normal->y;
    }
    if (sphere->center.z < absZ) {
        normal->z = -normal->z;
    }
    const double length = NormalizeHeld(normal);
    return outsideBelow ? -length : length;
}
