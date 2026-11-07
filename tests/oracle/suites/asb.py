# Mutants for bounding geometry. Each text occurs exactly once; the helpers widen a text by whole lines until it does, reading the sources as they stand. See docs/verification.md.
#
# Not mutated, as equivalent or not comparable:
#   * The order of the zeroes in constructors, Reset and Clear: nothing reads between them.
#   * The order Clear frees the three arrays in: the log records only that one was freed, and each is tested by being kept in turn.
#   * A second AtVector3::Normalize of a vector already a unit in PenetrationAlong on a generic direction would differ only in its last bit: it is mutated, and the bits are compared.
#   * NotAlong without its upper test alone: the absolute value of a component of a unit vector is above 1 by at most a rounding, far below the tolerance, and a NaN takes the lower test. Its lower test alone is kept.
#   * m_kind in the base's constructor is mutated all the same: a class of the harness's that adds nothing shows it.
#   * The two extents swapped where a sphere asks a box, in Penetration and PenetrationAlong: that is always a box against a sphere, whose answer writes zero to both.
#   * `<` for `<=` and the like in the separating axes of edges: they differ only on exact equality of sums of products of floats, which no chosen case reaches. Those comparisons are mutated to the opposite instead.
import os
import re
import sys

B = "AtBoundingBox.cpp"
G = "AtBoundingGeometry.cpp"
D = "AtBoundingDraw.cpp"
H = "AtBoundingMath.h"
Q = "AtSegmentQuery.cpp"
_SRC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[0]))), "src", "at")  # The runner compiles this with no __file__; it is tools/oracle_run.py.
_TEXT = {}


def _text(f):
    if f not in _TEXT:
        with open(os.path.join(_SRC, f), encoding="utf-8") as h:
            _TEXT[f] = h.read()
    return _TEXT[f]


def _at(f, start, old, new, name=None):
    """The mutant for the occurrence of old at start, widened by whole lines until its text is unique."""
    s = _text(f)
    a, b = start, start + len(old)
    while s.count(s[a:b]) != 1:
        if a > 0:
            a = s.rfind("\n", 0, a - 1) + 1
        if b < len(s):
            n = s.find("\n", b + 1)
            b = len(s) if n < 0 else n + 1
    line = s.count("\n", 0, start) + 1
    return (name or "%s:%d %s -> %s" % (f, line, old.strip()[:50], new.strip()[:50]), f, s[a:b], s[a:start] + new + s[start + len(old):b])


def one(f, old, new, nth=0):
    s = _text(f)
    start = -1
    for _ in range(nth + 1):
        start = s.index(old, start + 1)
    return _at(f, start, old, new)


def all_of(f, old, new):
    s = _text(f)
    out = []
    start = s.find(old)
    assert start >= 0, old
    while start >= 0:
        out.append(_at(f, start, old, new))
        start = s.find(old, start + 1)
    return out


def _range(f, a, b):
    s = _text(f)
    i = s.index(a)
    return i, s.index(b, i)


_NEXT = {"x": "y", "y": "z", "z": "x"}


def terms(f, a, b):
    """For each line of a function with products on half sizes: its first and its last term, on another axis."""
    i, j = _range(f, a, b)
    s = _text(f)
    out = []
    pos = i
    while pos < j:
        e = s.index("\n", pos)
        found = list(re.finditer(r"D\([\w.]+\) \* ([ho])\.([xyz])", s[pos:e]))
        if found:
            for m in {0: found[0], 1: found[-1]}.values():
                start = pos + m.start(2)
                out.append(_at(f, start, m.group(2), _NEXT[m.group(2)], "%s:%d term %s" % (f, s.count("\n", 0, start) + 1, m.group(0))))
        pos = e + 1
    return out


def flips(f, a, b, op, new):
    """Each comparison of the form on a line by itself in a function."""
    i, j = _range(f, a, b)
    s = _text(f)
    out = []
    start = s.find(op, i)
    while 0 <= start < j:
        out.append(_at(f, start, op, new))
        start = s.find(op, start + 1)
    return out


_O = "int AtBoundingBox::Overlaps("
_OS = "int AtBoundingBox::OverlapsSphere("
_SEG = "int AtBoundingBox::OverlapsSegment("
_CL = "unsigned int AtBoundingBox::Clip("
_P = "double AtBoundingBox::Penetration("
_PS = "double AtBoundingBox::PenetrationSphere("

BOX = []
BOX += [one(B, "%s = Abs(axes[%d].%s);" % (n, c, k), "%s = axes[%d].%s;" % (n, c, k)) for n, c, k in [("xx", 0, "x"), ("yx", 1, "x"), ("zx", 2, "x"), ("xy", 0, "y"), ("yy", 1, "y"), ("zy", 2, "y"), ("xz", 0, "z"), ("yz", 1, "z"), ("zz", 2, "z")]]
BOX += [one(B, "    return Abs((float)(D(abs.z) * t.z + D(abs.y) * t.y + D(abs.x) * t.x));", "    return Abs((float)(D(abs.z) * t.y + D(abs.y) * t.y + D(abs.x) * t.x));")]
BOX += [one(B, "    return Abs((float)(D(a) * b - D(c) * d));", "    return Abs((float)(D(a) * b + D(c) * d));")]
BOX += [one(B, "    r.x = Abs(v.x);", "    r.x = v.x;"), one(B, "    r.y = Abs(v.y);", "    r.y = v.y;"), one(B, "    r.z = Abs(v.z);", "    r.z = v.z;")]
BOX += [one(B, "const float kNearlyOne = 5e-6f;", "const float kNearlyOne = 5e-3f;")]
BOX += [one(B, "    return !(difference >= -D(kNearlyOne)) || !(difference <= D(kNearlyOne));", "    return !(difference <= D(kNearlyOne));")]
BOX += [
    one(B, "        if (!(depthHere > depth)) {", "        if (depthHere > depth) {"),
    one(B, "        depth = (float)depthHere;", "        ;"),
    one(B, "        *extent = mine;", "        *extent = theirs;"),
    one(B, "        *otherExtent = theirs;", "        *otherExtent = mine;"),
    one(B, "        const double length = sqrt(D(a) * a + D(b) * b);", "        const double length = sqrt(D(a) * a + D(a) * a);"),
    one(B, "        *inverse = (float)(1.0 / length);", "        *inverse = (float)length;"),
    one(B, "        *depthHere = (D(absolute) - (D(theirs) + mine)) * *inverse;", "        *depthHere = (D(absolute) - (D(theirs) + mine));"),
    one(B, "        *depthHere = (D(absolute) - (D(theirs) + mine)) * *inverse;", "        *depthHere = (D(absolute) - D(theirs)) * *inverse;"),
    one(B, "        if (!(*depthHere > depth)) {", "        if (*depthHere > depth) {"),
    one(B, "        depth = (float)*depthHere;", "        ;"),
    one(B, "        *extent = (float)(D(*inverse) * mine);", "        *extent = (float)mine;"),
    one(B, "        *otherExtent = (float)(D(*inverse) * theirs);", "        *otherExtent = theirs;"),
]
# Clip.
BOX += [
    one(B, "    if (s > e) {", "    if (s <= e) {"),
    one(B, "    const float high = (float)(D(center) + half);", "    const float high = (float)(D(center) - half);"),
]
BOX += all_of(B, "        const float low = (float)(D(center) - half);", "        const float low = (float)(D(center));")
BOX += all_of(B, "        if (s >= high) {", "        if (s > high + 1000) {") + all_of(B, "        if (e >= high) {", "        if (e > high + 1000) {")
BOX += all_of(B, "            if (!(e < high)) {", "            if (false) {") + all_of(B, "            if (!(s < high)) {", "            if (false) {")
BOX += all_of(B, "        if (!(s >= low)) {", "        if (false) {") + all_of(B, "        if (!(e >= low)) {", "        if (false) {")
BOX += all_of(B, "        if (e < low) {", "        if (e < low - 1000) {") + all_of(B, "        if (s < low) {", "        if (s < low - 1000) {")
BOX += all_of(B, "            const float t = (float)((D(high) - from) / (D(to) - from));", "            const float t = (float)((D(high) - to) / (D(to) - from));")
BOX += all_of(B, "            const float t = (float)((D(low) - from) / (D(to) - from));", "            const float t = (float)((D(low) - from) / (D(from) - to));")
BOX += all_of(B, "            *tStart = t;", "            ;") + all_of(B, "            *tEnd = t;", "            ;")
BOX += all_of(B, "clippedStart->Lerp(*start, *end, t);", "clippedStart->Lerp(*end, *start, t);")
BOX += [one(B, "                clippedEnd->Lerp(*end, *start, t);", "                clippedEnd->Lerp(*start, *end, t);"), one(B, "                clippedEnd->Lerp(*start, *end, t);", "                clippedEnd->Lerp(*end, *start, t);"), one(B, "            clippedEnd->Lerp(*start, *end, t);", "            clippedEnd->Lerp(*end, *start, t);")]
BOX += [one(B, "*flags = (*flags & f.startAtMaxKeep) | f.startAtMaxSet;", "*flags = f.startAtMaxSet;"), one(B, "*flags = (*flags & f.endAtMinKeep) | f.endAtMinSet;", "*flags = f.endAtMinSet;"), one(B, "*flags = (*flags & f.endAtMaxKeep) | f.endAtMaxSet;", "*flags = f.endAtMaxSet;"), one(B, "*flags = (*flags & f.startAtMinKeep) | f.startAtMinSet;", "*flags = f.startAtMinSet;")]
BOX += [one(B, "{kAll, 0x001, kAll, 0x200, kAll, 0x040, kAll, 0x008}", "{kAll, 0x002, kAll, 0x200, kAll, 0x040, kAll, 0x008}"), one(B, "{kAll, 0x001, kAll, 0x200, kAll, 0x040, kAll, 0x008}", "{kAll, 0x001, kAll, 0x100, kAll, 0x040, kAll, 0x008}"), one(B, "{kAll, 0x001, kAll, 0x200, kAll, 0x040, kAll, 0x008}", "{kAll, 0x001, kAll, 0x200, kAll, 0x080, kAll, 0x008}"), one(B, "{kAll, 0x001, kAll, 0x200, kAll, 0x040, kAll, 0x008}", "{kAll, 0x001, kAll, 0x200, kAll, 0x040, kAll, 0x010}")]
for k, v, w in [("0xFFFFFFC2u, 0x002", "0xFFFFFFC2u, 0x001", "kAll, 0x002"), ("0xFFFFF43Fu, 0x400", "0xFFFFF43Fu, 0x200", "kAll, 0x400"), ("0xFFFFF0BFu, 0x080", "0xFFFFF0BFu, 0x040", "kAll, 0x080"), ("0xFFFFFFD0u, 0x010", "0xFFFFFFD0u, 0x008", "kAll, 0x010"),
                ("0xFFFFFFC4u, 0x004", "0xFFFFFFC4u, 0x002", "kAll, 0x004"), ("0xFFFFF83Fu, 0x800", "0xFFFFF83Fu, 0x400", "kAll, 0x800"), ("0xFFFFF13Fu, 0x100", "0xFFFFF13Fu, 0x080", "kAll, 0x100"), ("0xFFFFFFE0u, 0x020", "0xFFFFFFE0u, 0x010", "kAll, 0x020")]:
    BOX += [one(B, k, v), one(B, k, w)]
BOX += [one(B, "kClipZ, true, &flags", "kClipZ, false, &flags"), one(B, "kClipY, false, &flags", "kClipY, true, &flags"), one(B, "    *clippedStart = *start;\n", ""), one(B, "    *clippedEnd = *end;\n", "")]
BOX += [one(B, "tEnd, kClipX, false, &flags)) {\n        return 0;", "tEnd, kClipX, false, &flags)) {\n        return flags;"), one(B, "tEnd, kClipY, false, &flags)) {\n        return 0;", "tEnd, kClipY, false, &flags)) {\n        return flags;"), one(B, "tEnd, kClipZ, true, &flags)) {\n        return 0;", "tEnd, kClipZ, true, &flags)) {\n        return flags;")]
# Read.
BOX += [one(B, "    m_center.Read(tokenizer);\n", ""), one(B, "    m_half.x = Abs(tokenizer->ExpectReal());", "    m_half.x = tokenizer->ExpectReal();"), one(B, "    m_half.y = Abs(tokenizer->ExpectReal());", "    m_half.y = tokenizer->ExpectReal();"), one(B, "    m_half.z = Abs(tokenizer->ExpectReal());", "    m_half.z = tokenizer->ExpectReal();")]
# Overlaps and Penetration, term by term.
BOX += terms(B, _O, _OS) + terms(B, _P, _PS)
BOX += flips(B, _O, _OS, " < Abs(", " > Abs(") + flips(B, _O, _OS, " < CrossAbs(", " > CrossAbs(") + flips(B, _O, _OS, " < DotAbs(", " > DotAbs(")
for old, new in [("CrossAbs(n.y, t.z, n.z, t.y)", "CrossAbs(n.y, t.y, n.z, t.z)"), ("CrossAbs(n.z, t.x, n.x, t.z)", "CrossAbs(n.z, t.z, n.x, t.x)"), ("CrossAbs(n.x, t.y, n.y, t.x)", "CrossAbs(n.x, t.x, n.y, t.y)")]:
    BOX += all_of(B, old, new)
BOX += all_of(B, "DotAbs(a, t)", "DotAbs(n, t)")
BOX += all_of(B, "n.SetNormalized(rotation[0]);", "n.SetNormalized(rotation[1]);") + all_of(B, "n.SetNormalized(rotation[1]);", "n.SetNormalized(rotation[2]);") + all_of(B, "n.SetNormalized(rotation[2]);", "n.SetNormalized(rotation[0]);")
BOX += all_of(B, "    a = AbsOf(n);\n", "") + all_of(B, "AtVector3 a = AbsOf(n);", "AtVector3 a = n;")
BOX += all_of(B, "        *normal = n;\n", "")
BOX += all_of(B, "if (NotAlong(a.x)) {", "if (true) {") + all_of(B, "if (NotAlong(a.y)) {", "if (true) {") + all_of(B, "if (NotAlong(a.z)) {", "if (true) {")
BOX += all_of(B, "SetNormal(normal, 0.0f, (float)-(D(inverse) * n.z), (float)(D(inverse) * n.y));", "SetNormal(normal, 0.0f, (float)(D(inverse) * n.z), (float)(D(inverse) * n.y));")
BOX += all_of(B, "SetNormal(normal, (float)(D(inverse) * n.z), 0.0f, (float)-(D(inverse) * n.x));", "SetNormal(normal, (float)(D(inverse) * n.z), 0.0f, (float)(D(inverse) * n.x));")
BOX += all_of(B, "SetNormal(normal, (float)-(D(inverse) * n.y), (float)(D(inverse) * n.x), 0.0f);", "SetNormal(normal, (float)(D(inverse) * n.y), (float)(D(inverse) * n.x), 0.0f);")
BOX += [one(B, "SetNormal(normal, 1.0f, 0.0f, 0.0f);", "SetNormal(normal, 0.0f, 1.0f, 0.0f);"), one(B, "SetNormal(normal, 0.0f, 1.0f, 0.0f);", "SetNormal(normal, 1.0f, 0.0f, 0.0f);"), one(B, "SetNormal(normal, 0.0f, 0.0f, 1.0f);", "SetNormal(normal, 0.0f, 1.0f, 0.0f);")]
BOX += [one(B, "(reach + h.x), h.x, (float)reach)", "(reach + h.x), h.y, (float)reach)"), one(B, "(reach + h.y), h.y, (float)reach)", "(reach + h.y), h.z, (float)reach)"), one(B, "(reach + h.z), h.z, (float)reach)", "(reach + h.z), h.x, (float)reach)")]
BOX += [one(B, "(reach + h.x), h.x", "(reach + h.y), h.x"), one(B, "(reach + mine), (float)mine, o.x)", "(reach + mine), (float)mine, o.y)"), one(B, "(reach + mine), (float)mine, o.y)", "(reach + mine), (float)mine, o.z)"), one(B, "(reach + mine), (float)mine, o.z)", "(reach + mine), (float)mine, o.x)")]
BOX += [one(B, "    Deepest best = {-3.4028234663852886e+38f,", "    Deepest best = {0.0f,")]
BOX += [one(B, "            return depth;  // Only here", "            return (float)depth;  // Only here")]
BOX += [one(B, "    t.x = (float)relative[0];", "    t.x = (float)relative[1];", 0), one(B, "    t.x = (float)relative[0];", "    t.x = (float)relative[1];", 1), one(B, "    t.z = (float)relative[2];", "    t.z = (float)relative[0];", 0), one(B, "    t.z = (float)relative[2];", "    t.z = (float)relative[0];", 1)]
# The sphere and the segment.
BOX += [one(B, "    const double dx = D(Abs(sphere->center.x)) - m_half.x;", "    const double dx = D(sphere->center.x) - m_half.x;"), one(B, "    const float dy = (float)(D(Abs(sphere->center.y)) - m_half.y);", "    const float dy = (float)(D(sphere->center.y) - m_half.y);"), one(B, "    const float dz = (float)(D(Abs(sphere->center.z)) - m_half.z);", "    const float dz = (float)(D(sphere->center.z) - m_half.z);")]
BOX += [one(B, "    if (dx <= 0.0) {\n        if (dy", "    if (dx > 0.0) {\n        if (dy"), one(B, "    if (dx <= 0.0) {\n        if (dy", "    if (!(dx > 0.0)) {\n        if (dy"), one(B, "            return dz <= sphere->radius ? 1 : 0;", "            return dz < sphere->radius ? 1 : 0;"), one(B, "            return dy <= sphere->radius ? 1 : 0;", "            return dy < sphere->radius ? 1 : 0;"), one(B, "            return dx <= radius ? 1 : 0;", "            return dx < radius ? 1 : 0;")]
BOX += [one(B, "        return radius * radius >= D(dz) * dz + D(dy) * dy ? 1 : 0;", "        return radius * radius >= D(dz) * dz ? 1 : 0;"), one(B, "        return radius * radius >= D(dz) * dz + dx * dx ? 1 : 0;", "        return radius * radius >= dx * dx ? 1 : 0;"), one(B, "        return radius * radius >= D(dy) * dy + dx * dx ? 1 : 0;", "        return radius * radius >= D(dy) * dy ? 1 : 0;"), one(B, "    return radius * radius >= D(dz) * dz + D(dy) * dy + dx * dx ? 1 : 0;", "    return radius * radius >= D(dz) * dz + D(dy) * dy ? 1 : 0;")]
BOX += [one(B, "    if (dy <= 0.0f) {\n        if (dz <= 0.0f) {\n            return dx", "    if (dy > 0.0f) {\n        if (dz <= 0.0f) {\n            return dx"), one(B, "    if (dz <= 0.0f) {\n        return radius * radius >= D(dy)", "    if (dz > 0.0f) {\n        return radius * radius >= D(dy)"), one(B, "        if (dy <= 0.0f) {\n            if (dz <= 0.0f) {\n                return 1;", "        if (dy > 0.0f) {\n            if (dz <= 0.0f) {\n                return 1;"), one(B, "            if (dz <= 0.0f) {\n                return 1;", "            if (dz > 0.0f) {\n                return 1;")]
for a, b in [("d.x = (float)(D(middle->x) - m_center.x);", "d.x = (float)(D(middle->x));"), ("d.y = (float)(D(middle->y) - m_center.y);", "d.y = (float)(D(middle->y));"), ("d.z = (float)(D(middle->z) - m_center.z);", "d.z = (float)(D(middle->z));"),
             ("const float ax = Abs(direction->x);", "const float ax = direction->x;"), ("const float ay = Abs(direction->y);", "const float ay = direction->y;"), ("const float az = Abs(direction->z);", "const float az = direction->z;"),
             ("if (D(ax) * halfLength + m_half.x < Abs(d.x)) {", "if (D(ax) + m_half.x < Abs(d.x)) {"), ("if (D(ay) * halfLength + m_half.y < Abs(d.y)) {", "if (D(ay) * halfLength < Abs(d.y)) {"), ("if (D(az) * halfLength + m_half.z < Abs(d.z)) {", "if (D(az) * halfLength + m_half.z < d.z) {"),
             ("float reach = (float)(D(az) * m_half.y + D(ay) * m_half.z);", "float reach = (float)(D(ay) * m_half.y + D(az) * m_half.z);"), ("reach = (float)(D(ax) * m_half.z + D(az) * m_half.x);", "reach = (float)(D(az) * m_half.z + D(ax) * m_half.x);"), ("reach = (float)(D(ax) * m_half.y + D(ay) * m_half.x);", "reach = (float)(D(ay) * m_half.y + D(ax) * m_half.x);"),
             ("if (CrossAbs(d.y, direction->z, d.z, direction->y) > reach) {", "if (CrossAbs(d.y, direction->y, d.z, direction->z) > reach) {"), ("if (CrossAbs(d.z, direction->x, d.x, direction->z) > reach) {", "if (CrossAbs(d.z, direction->z, d.x, direction->x) > reach) {"), ("if (CrossAbs(d.x, direction->y, d.y, direction->x) > reach) {", "if (CrossAbs(d.x, direction->x, d.y, direction->y) > reach) {")]:
    BOX.append(one(B, a, b))
# The sphere's depth.
for a, b in [("    float dx = (float)(D(absX) - m_half.x);", "    float dx = (float)(D(absX));"), ("    float dy = (float)(D(absY) - m_half.y);", "    float dy = (float)(D(absY));"), ("    const double dzHeld = D(absZ) - m_half.z;", "    const double dzHeld = D(absZ);"),
             ("    const float absZ = Abs(sphere->center.z);", "    const float absZ = sphere->center.z;"), ("    const float absY = Abs(sphere->center.y);", "    const float absY = sphere->center.y;"), ("    const float absX = Abs(sphere->center.x);", "    const float absX = sphere->center.x;"),
             ("    *otherExtent = 0.0f;\n", ""), ("    *extent = 0.0f;\n", ""),
             ("                dx = -dx;\n", ""), ("                dy = -dy;\n", ""), ("                dz = (float)-dzHeld;\n", ""),
             ("                normal->x = 0.0f;\n", ""), ("                normal->y = 0.0f;\n", ""), ("                normal->z = 0.0f;\n", ""),
             ("                if (dx < dy && !(dz < dx)) {", "                if (dx < dy) {"), ("                if (dx < dy && !(dz < dx)) {", "                if (!(dz < dx)) {"), ("                } else if (dz < dy) {  // When", "                } else if (dx < dy) {  // When"),
             ("normal->x = (float)(-D(dx) - radius);", "normal->x = (float)(-D(dx) + radius);"), ("normal->z = (float)(-D(dz) - radius);", "normal->z = (float)(-D(dz) + radius);"), ("normal->y = (float)(-D(dy) - radius);", "normal->y = (float)(-D(dy) + radius);"),
             ("            surface = dzHeld - radius;", "            surface = dzHeld;"), ("SetNormal(normal, 0.0f, 0.0f, (float)surface);", "SetNormal(normal, 0.0f, (float)surface, 0.0f);"),
             ("            surface = D(dy) - radius;", "            surface = D(dy);"), ("SetNormal(normal, 0.0f, (float)surface, 0.0f);", "SetNormal(normal, (float)surface, 0.0f, 0.0f);"),
             ("            surface = D(dx) - radius;", "            surface = D(dx);"), ("SetNormal(normal, (float)surface, 0.0f, 0.0f);", "SetNormal(normal, 0.0f, 0.0f, (float)surface);"),
             ("SetNormal(normal, 0.0f, dy, dz);", "SetNormal(normal, 0.0f, dz, dy);"), ("SetNormal(normal, dx, 0.0f, dz);", "SetNormal(normal, dz, 0.0f, dx);"), ("SetNormal(normal, dx, dy, 0.0f);", "SetNormal(normal, dy, dx, 0.0f);"),
             ("surface = sqrt(D(dz) * dz + D(dy) * dy) - radius;", "surface = sqrt(D(dz) * dz + D(dy) * dy);"), ("surface = sqrt(D(dz) * dz + D(dx) * dx) - radius;", "surface = sqrt(D(dz) * dz + D(dx) * dx);"), ("surface = sqrt(D(dy) * dy + D(dx) * dx) - radius;", "surface = sqrt(D(dy) * dy + D(dx) * dx);"), ("surface = sqrt(D(dz) * dz + D(dy) * dy + D(dx) * dx) - radius;", "surface = sqrt(D(dz) * dz + D(dy) * dy + D(dx) * dx);"),
             ("    normal->x = (float)(surface * normal->x);\n", ""), ("    normal->y = (float)(surface * normal->y);\n", ""), ("    normal->z = (float)(surface * normal->z);\n", ""),
             ("    if (sphere->center.x < absX) {", "    if (false) {"), ("    if (sphere->center.y < absY) {", "    if (false) {"), ("    if (sphere->center.z < absZ) {", "    if (false) {"),
             ("    return outsideBelow ? -length : length;", "    return length;"), ("    const double length = NormalizeHeld(normal);", "    const double length = (float)NormalizeHeld(normal);"),
             ("            d.x = dx;", "            d.x = dy;"), ("            d.y = dy;", "            d.y = dz;"), ("            d.z = dz;", "            d.z = dx;")]:
    BOX.append(one(B, a, b))
BOX += all_of(B, "normal->Normalize();\n", ";\n")
BOX += all_of(B, "    outsideBelow = surface < 0.0;\n", "")
BOX += [one(B, "    if (dx <= 0.0f) {\n        if (dy <= 0.0f) {\n            if (dzHeld", "    if (dx > 0.0f) {\n        if (dy <= 0.0f) {\n            if (dzHeld"), one(B, "        if (dy <= 0.0f) {\n            if (dzHeld <= 0.0) {\n                // Inside", "        if (dy > 0.0f) {\n            if (dzHeld <= 0.0) {\n                // Inside"), one(B, "            if (dzHeld <= 0.0) {\n                // Inside", "            if (dzHeld > 0.0) {\n                // Inside"), one(B, "        } else if (dzHeld <= 0.0) {", "        } else if (dzHeld > 0.0) {"), one(B, "    } else if (dy <= 0.0f) {", "    } else if (dy > 0.0f) {"), one(B, "        if (dzHeld <= 0.0) {\n            surface = D(dx)", "        if (dzHeld > 0.0) {\n            surface = D(dx)"), one(B, "        if (dzHeld <= 0.0) {\n            SetNormal(normal, dx, dy", "        if (dzHeld > 0.0) {\n            SetNormal(normal, dx, dy")]

MATH = [
    (one(H, "    u.bits &= 0x7FFFFFFFu;\n", "")),
    one(H, "    const double toX = D(position.x) - center.x;", "    const double toX = D(position.x);"),
    one(H, "    const double toY = D(position.y) - center.y;", "    const double toY = D(position.y);"),
    one(H, "    const float toZ = (float)(D(position.z) - center.z);", "    const float toZ = (float)(D(position.z));"),
    one(H, "    const double fromX0 = D(axes[0].x) * point.x;", "    const double fromX0 = D(axes[0].y) * point.x;"),
    one(H, "    const double fromX1 = D(axes[0].y) * point.x;", "    const double fromX1 = D(axes[0].z) * point.x;"),
    one(H, "    const float fromX2 = (float)(D(axes[0].z) * point.x);", "    const float fromX2 = (float)(D(axes[0].x) * point.x);"),
    one(H, "    const float sum0 = (float)(D(axes[1].x) * point.y + fromX0);", "    const float sum0 = (float)(D(axes[1].x) * point.y);"),
    one(H, "    const double sum1 = D(axes[1].y) * point.y + fromX1;", "    const double sum1 = D(axes[1].y) * point.y;"),
    one(H, "    const double sum2 = D(axes[1].z) * point.y + fromX2;", "    const double sum2 = D(axes[1].z) * point.y;"),
    one(H, "    const float total0 = (float)(D(axes[2].x) * point.z + sum0);", "    const float total0 = (float)(sum0);"),
    one(H, "    const float total1 = (float)(D(axes[2].y) * point.z + sum1);", "    const float total1 = (float)(sum1);"),
    one(H, "    const float total2 = (float)(D(axes[2].z) * point.z + sum2);", "    const float total2 = (float)(sum2);"),
    one(H, "    out[0] = (float)(toX + total0);", "    out[0] = (float)(total0);"),
    one(H, "    out[1] = toY + total1;", "    out[1] = total1;"),
    one(H, "    out[2] = D(toZ) + total2;", "    out[2] = total2;"),
    one(H, "    if (length <= D(5e-6f)) {", "    if (length <= D(5e-3f)) {"),
    one(H, "        v->x = 1.0f;\n", "        v->x = 0.0f;\n"),
    one(H, "        return 0.0;\n", "        return length;\n"),
    one(H, "    v->x = (float)(scale * v->x);\n", ""),
    one(H, "    v->y = (float)(scale * v->y);\n", ""),
    one(H, "    v->z = (float)(scale * v->z);\n", ""),
    one(H, "    return length;\n", "    return (float)length;\n"),
    one(H, "    return dz * dz + dy * dy + dx * dx;", "    return (float)(dz * dz + dy * dy + dx * dx);"),
    one(H, "    return dz * dz + dy * dy + dx * dx;", "    return dz * dz + dy * dy;"),
]

GEO = []
for a, b in [
    ('{0x3A, "BoxBoundingGeometry"}', '{0x3B, "BoxBoundingGeometry"}'), ('{0x46, "GroupBoundingGeometry"}', '{0x45, "GroupBoundingGeometry"}'), ('{0x44, "Parent"}', '{0x44, "Parent2"}'),
    ("    return &((AtSceneJointedModelActor*)node)->m_joints.m_database->m_rotTrans[index - 1];", "    return &((AtSceneJointedModelActor*)node)->m_joints.m_database->m_rotTrans[index];"),
    ("    if (node->m_class != 3) {", "    if (false) {"), ("    if (((AtSceneJointedModelActor*)node)->m_joints.m_database == 0) {", "    if (false) {"),
    ("    g_atMatrix.GetPosition(m, position);\n", ""), ("    g_atMatrix.GetAxes(m, &axes[0], &axes[1], &axes[2]);", "    g_atMatrix.GetAxes(m, &axes[1], &axes[0], &axes[2]);"),
    ("    if (AtBoundingGeometry::s_contacts != 0) {", "    if (false) {"), ("->Add(mine, other, AtBoundingGeometry::s_node, AtBoundingGeometry::s_otherNode);", "->Add(other, mine, AtBoundingGeometry::s_node, AtBoundingGeometry::s_otherNode);"), ("->Add(mine, other, AtBoundingGeometry::s_node, AtBoundingGeometry::s_otherNode);", "->Add(mine, other, AtBoundingGeometry::s_otherNode, AtBoundingGeometry::s_node);"),
    ("            AtCoordSysTransformDirection(coordSys, vector, normal);", "            *normal = *vector;"), ("            *normal = *vector;", "            AtCoordSysTransformDirection(AtBoundingGeometry::s_coordSys, vector, normal);"), ("        AtCoordSysTransformDirection(JointOf(node, jointIndex), vector, normal);", "        *normal = *vector;"),
    ("    if (jointIndex == 0) {\n        AtCoordSys* coordSys = node->ParentCoordSys();\n        if (coordSys != 0) {\n            AtCoordSysTransformDirection", "    if (jointIndex != 0) {\n        AtCoordSys* coordSys = node->ParentCoordSys();\n        if (coordSys != 0) {\n            AtCoordSysTransformDirection"),
    ("    if (jointIndex == 0) {\n        AtCoordSys* coordSys = node->ParentCoordSys();\n        if (coordSys != 0) {\n            coordSys->TransformDirectionInverse", "    if (jointIndex != 0) {\n        AtCoordSys* coordSys = node->ParentCoordSys();\n        if (coordSys != 0) {\n            coordSys->TransformDirectionInverse"),
    ("    normal->Normalize();\n", ""), ("D(to.z) * vector->z + D(to.y) * vector->y + D(to.x) * vector->x < 0.0", "D(to.z) * vector->z + D(to.y) * vector->y + D(to.x) * vector->x > 0.0"), ("D(to.z) * vector->z + D(to.y) * vector->y + D(to.x) * vector->x < 0.0", "D(to.z) * vector->z + D(to.y) * vector->y < 0.0"),
    ("            coordSys->TransformDirectionInverse(direction, local);", "            *local = *direction;"), ("            *local = *direction;", "            ;"), ("        JointOf(node, jointIndex)->TransformDirectionInverse(direction, local);", "        *local = *direction;"), ("    local->Normalize();\n", ""),
    ("    if (AtBoundingGeometryDatabase::s_heap == 0) {", "    if (false) {"), ("    AtPushHeap(AtBoundingGeometryDatabase::s_heap);\n", ""), ("    AtPopHeap();\n    if (first == 0) {", "    if (first == 0) {"), ("    if (first == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (false) {\n        AtRaise(-4, 0, 0, 0);"), ("        for (int left = (int)count - 1; left >= 0; left--) {", "        for (int left = (int)count - 2; left >= 0; left--) {"), ("            each++;\n", ""),
    ("    tokenizer->ExpectLeftBracket();\n", ""), ("    tokenizer->ExpectRightBracket();\n", ""),
    # AtCoordSys.
    ("(m_dirty & 2) == 0 && generation == m_unknown20 && multiply != 0", "generation == m_unknown20 && multiply != 0"), ("(m_dirty & 2) == 0 && generation == m_unknown20 && multiply != 0", "(m_dirty & 2) == 0 && multiply != 0"), ("(m_dirty & 2) == 0 && generation == m_unknown20 && multiply != 0", "(m_dirty & 2) == 0 && generation == m_unknown20"),
    ("            g_atMatrix.SetProduct(m_world, m_local, base);", "            g_atMatrix.SetProduct(m_world, base, m_local);"), ("            g_atMatrix.Copy(m_world, base);", "            g_atMatrix.Copy(m_world, m_local);"), ("        m_unknown20 = generation;\n    } else {", "    } else {"),
    ("        m_parent->UpdateDrawing(base, generation, multiply);\n", ""), ("        if ((m_dirty & 2) == 0 && generation == m_unknown20) {", "        if (generation == m_unknown20) {"), ("        if ((m_dirty & 2) == 0 && generation == m_unknown20) {", "        if ((m_dirty & 2) == 0) {"), ("        m_unknown20 = generation;\n        if (multiply", "        if (multiply"),
    ("            g_atMatrix.SetProduct(m_world, m_local, m_parent->m_world);", "            g_atMatrix.SetProduct(m_world, m_parent->m_world, m_local);"), ("            g_atMatrix.Copy(m_world, m_parent->m_world);", "            g_atMatrix.Copy(m_world, m_local);"),
    ("        child->m_dirty |= 2;\n", "        ;\n"), ("    m_dirty = (m_dirty & ~2u) | 0x29;", "    m_dirty = (m_dirty & ~2u) | 0x2B;"), ("    m_dirty = (m_dirty & ~2u) | 0x29;", "    m_dirty = m_dirty | 0x29;"), ("    return 2;\n", "    return 1;\n"),
    ("    UpdateInverse();\n", ""), ("    g_atMatrix.TransformDirection(m_world, from, to);", "    g_atMatrix.TransformDirection(m_local, from, to);"),
    ("        m_geometrySpaces.Add(ns, map);", "        m_geometry.Add(map);"), ("        m_geometry.Add(map);", "        m_geometrySpaces.Add(\"\", map);"),
    # Contacts.
    ("    if (m_count >= m_capacity) {", "    if (false) {"), ("    if ((m_flags & kLeavesOnly) != 0 && (first->m_firstChild != 0 || second->m_firstChild != 0)) {", "    if ((first->m_firstChild != 0 || second->m_firstChild != 0)) {"), ("    if ((m_flags & kLeavesOnly) != 0 && (first->m_firstChild != 0 || second->m_firstChild != 0)) {", "    if ((m_flags & kLeavesOnly) != 0 && (first->m_firstChild != 0)) {"), ("    if ((m_flags & kLeavesOnly) != 0 && (first->m_firstChild != 0 || second->m_firstChild != 0)) {", "    if ((m_flags & kLeavesOnly) != 0 && (second->m_firstChild != 0)) {"),
    ("    m_items[m_count].m_first = first;", "    m_items[m_count].m_first = second;"), ("    m_items[m_count].m_second = second;", "    m_items[m_count].m_second = first;"), ("    m_items[m_count].m_firstNode = firstNode;", "    m_items[m_count].m_firstNode = secondNode;"), ("    m_items[m_count].m_secondNode = secondNode;", "    m_items[m_count].m_secondNode = firstNode;"), ("    m_count++;\n", ""),
    # The base.
    ("    m_kind = -1;", "    m_kind = 0;"), ("    m_jointIndex = 0;\n", ""), ("    m_unknown0C = 0;\n", ""), ("    m_parent = 0;\n", ""), ("    m_nextSibling = 0;\n", ""), ("    m_firstChild = 0;\n", ""), ("    m_database = 0;\n", ""),
    ("    Draw(material, device, node, coordSys);\n", ""), ("        child->DrawTree(material, device, node, coordSys);", "        child->Draw(material, device, node, coordSys);"), ("child = child->m_nextSibling) {\n        child->DrawTree", "child = 0) {\n        child->DrawTree"),
    ("    s_node = s_otherNode;\n", ""), ("    s_otherNode = node;\n", ""),
    ("    tokenizer->ExpectLeftCurly();\n", ""), ("            m_jointIndex = tokenizer->ExpectInt();", "            tokenizer->ExpectInt();"), ("            AtBoundingGeometry* parent = (AtBoundingGeometry*)database->Find(name);", "            AtBoundingGeometry* parent = (AtBoundingGeometry*)database->Find(\"A\");"), ("            if (parent == 0) {", "            if (false) {"), ("            if (m_parent != parent) {", "            if (parent != 0) {"), ("                m_parent = parent;\n", ""), ("                m_nextSibling = parent->m_firstChild;\n", ""), ("                parent->m_firstChild = this;\n", ""), ("            ReadWord(tokenizer, token);\n", ""), ("    m_database = database;\n", ""),
    ("    if (coordSys == 0) {\n        if (m_jointIndex == 0) {", "    if (true) {\n        if (m_jointIndex == 0) {"), ("        if (m_jointIndex == 0) {\n            coordSys = node->ParentCoordSys();", "        if (m_jointIndex != 0) {\n            coordSys = node->ParentCoordSys();"), ("            coordSys = JointOf(AtSceneNode::AsJointed(node), m_jointIndex);", "            coordSys = JointOf(node, m_jointIndex);"),
    ("    coordSys->UpdateDrawing(device->DrawingMatrix(), generation, 1);", "    coordSys->UpdateDrawing(device->DrawingMatrix(), generation, 0);"), ("    coordSys->UpdateDrawing(device->DrawingMatrix(), generation, 1);", "    coordSys->UpdateDrawing(device->DrawingMatrix(), 0, 1);"), ("    device->SetWorld(0, coordSys->m_world);", "    device->SetWorld(1, coordSys->m_world);"), ("    device->SetWorld(0, coordSys->m_world);", "    device->SetWorld(0, coordSys->m_local);"),
    # RelateTo.
    ("    if (other->m_jointIndex == 0) {", "    if (m_jointIndex == 0) {"), ("        AtCoordSys* coordSys = s_otherCoordSys;", "        AtCoordSys* coordSys = s_coordSys;"), ("        if (coordSys == 0) {\n            position.x", "        if (false) {\n            position.x"),
    ("            axes[0].x = 1.0f;", "            axes[0].x = 0.0f;"), ("            axes[1].y = 1.0f;", "            axes[1].y = 0.0f;"), ("            axes[2].z = 1.0f;", "            axes[2].z = 0.0f;"), ("            position.y = 0.0f;", "            position.y = 1.0f;"), ("            axes[1].x = 0.0f;", "            axes[1].x = 1.0f;"), ("            axes[2].y = 0.0f;", "            axes[2].y = 1.0f;"),
    ("            coordSys->UpdateWorld();\n", ""), ("        AtCoordSys* joint = CheckedJointOf(s_otherNode, other->m_jointIndex);", "        AtCoordSys* joint = CheckedJointOf(s_node, other->m_jointIndex);"), ("        AtCoordSys* joint = CheckedJointOf(s_otherNode, other->m_jointIndex);", "        AtCoordSys* joint = CheckedJointOf(s_otherNode, m_jointIndex);"), ("        joint->UpdateWorld();\n", ""), ("        ReadMatrixAxes(joint->m_world, &position, axes);", "        ReadMatrixAxes(joint->m_local, &position, axes);"),
    ("    if (m_jointIndex == 0) {\n        mine = s_coordSys;", "    if (m_jointIndex != 0) {\n        mine = s_coordSys;"), ("        mine = s_coordSys;", "        mine = s_otherCoordSys;"), ("        if (mine == 0) {", "        if (false) {"), ("            s_relativePosition = position;\n", ""), ("            s_relativeAxes[0] = axes[0];\n", ""), ("            s_relativeAxes[1] = axes[1];\n", ""), ("            s_relativeAxes[2] = axes[2];\n", ""),
    ("        mine = CheckedJointOf(s_node, m_jointIndex);", "        mine = CheckedJointOf(s_otherNode, m_jointIndex);"), ("    mine->UpdateInverse();\n", ""), ("    g_atMatrix.TransformPoint(mine->m_world, &position, &s_relativePosition);", "    g_atMatrix.TransformDirection(mine->m_world, &position, &s_relativePosition);"), ("&axes[0], &s_relativeAxes[0]);", "&axes[1], &s_relativeAxes[0]);"), ("&axes[1], &s_relativeAxes[1]);", "&axes[2], &s_relativeAxes[1]);"), ("&axes[2], &s_relativeAxes[2]);", "&axes[0], &s_relativeAxes[2]);"),
    # Kinds and the group.
    ("    m_kind = kGroup;", "    m_kind = 3;"), ("    m_kind = kSphere;", "    m_kind = kBox;"), ("    m_kind = kBox;\n}", "    m_kind = kSphere;\n}"),
    ("void AtGroupBoundingGeometry::ReadWord(AtIoTokenizer* tokenizer, int) {\n    tokenizer->Error(kUnexpectedWord);", "void AtGroupBoundingGeometry::ReadWord(AtIoTokenizer* tokenizer, int) {\n    tokenizer->Error(kAtProblemWrongUse);"),
    ("int AtGroupBoundingGeometry::Overlaps(AtBoundingGeometry*) {\n    return 1;", "int AtGroupBoundingGeometry::Overlaps(AtBoundingGeometry*) {\n    return 0;"), ("int AtGroupBoundingGeometry::TestSegment() {\n    return 1;", "int AtGroupBoundingGeometry::TestSegment() {\n    return 0;"),
    ("AtSceneNode*, AtVector3*, float*, float*) {\n    return 0.0;", "AtSceneNode*, AtVector3*, float*, float*) {\n    return 1.0;"), ("AtSceneNode*, const AtVector3*, float*, float*) {\n    return 0.0;", "AtSceneNode*, const AtVector3*, float*, float*) {\n    return 1.0;"),
    # The sphere's words.
    ("    if (token != kSphereShape) {", "    if (false) {"), ("    if (token != kSphereShape) {\n        tokenizer->Error(kUnexpectedWord);\n        return;", "    if (token != kSphereShape) {\n        tokenizer->Error(kUnexpectedWord);"), ("    m_sphere.center.Read(tokenizer);\n", ""), ("    m_sphere.radius = Abs(tokenizer->ExpectReal());", "    m_sphere.radius = tokenizer->ExpectReal();"),
    # The sphere's overlap.
    ("        center.x = (float)(relative[0] + m_sphere.center.x);", "        center.x = (float)(relative[0]);"), ("        center.y = (float)(relative[1] + m_sphere.center.y);", "        center.y = (float)(relative[1]);"), ("        center.z = (float)(relative[2] + m_sphere.center.z);", "        center.z = (float)(relative[2]);"),
    ("        const float reach = (float)(D(theirs.radius) + m_sphere.radius);", "        const float reach = (float)(D(theirs.radius));"), ("        if (!(D(reach) * reach >= DistanceSquaredHeld(center, m_sphere.center))) {", "        if (D(reach) * reach >= DistanceSquaredHeld(center, m_sphere.center)) {"),
    ("        return Found(this, other);\n", "        return 1;\n"),
    ("        const int result = other->Overlaps(this);", "        const int result = 0;"), ("        SwapNodes();\n        const int result", "        const int result"), ("        const int result = other->Overlaps(this);\n        SwapNodes();", "        const int result = other->Overlaps(this);"),
    ("    return other->m_kind == kGroup ? 1 : 0;\n}\n\nint AtSphereBoundingGeometry::TestSegment", "    return 0;\n}\n\nint AtSphereBoundingGeometry::TestSegment"), ("    return other->m_kind == kGroup ? 1 : 0;\n}\n\nint AtBoxBoundingGeometry::TestSegment", "    return 0;\n}\n\nint AtBoxBoundingGeometry::TestSegment"),
    ("    if (other->m_kind == kSphere) {\n        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;\n        RelateTo(other);\n        double relative[3];", "    if (false) {\n        const AtSceneSphere& theirs = ((AtSphereBoundingGeometry*)other)->m_sphere;\n        RelateTo(other);\n        double relative[3];"),
    # The sphere's segment.
    ("AtSphereSegment(&m_sphere, &s_segmentStart, &s_segmentEnd, &length, &s_segmentDirection, &point, &key, 0) != 0 && AtSegmentQueryInsert(s_query, key) != 0) {", "AtSphereSegment(&m_sphere, &s_segmentStart, &s_segmentEnd, &length, &s_segmentDirection, &point, &key, 0) != 0) {"),
    ("        s_query->m_records[s_query->m_current].m_geometry = this;\n        AtSegmentQueryFinish(s_query, 3);", "        AtSegmentQueryFinish(s_query, 3);"), ("        AtSegmentQueryFinish(s_query, 3);", "        AtSegmentQueryFinish(s_query, 2);"),
    # The sphere's penetration.
    ("        const float depth = (float)(NormalizeHeld(&between) - m_sphere.radius - theirs.radius);", "        const float depth = (float)(NormalizeHeld(&between) - m_sphere.radius);"), ("        const float depth = (float)(NormalizeHeld(&between) - m_sphere.radius - theirs.radius);", "        const float depth = (float)(NormalizeHeld(&between) - theirs.radius);"),
    ("        NormalToWorld(m_jointIndex, node, &between, normal);\n        return depth;", "        return depth;"),
    ("        const float depth = (float)other->Penetration(otherNode, this, node, normal, otherExtent, extent);", "        const float depth = (float)other->Penetration(node, this, otherNode, normal, otherExtent, extent);"),
    ("        const float depth = (float)other->Penetration(otherNode, this, node, normal, otherExtent, extent);\n        SwapNodes();\n        normal->x = -normal->x;", "        const float depth = (float)other->Penetration(otherNode, this, node, normal, otherExtent, extent);\n        normal->x = -normal->x;"),
    ("        SwapNodes();\n        normal->x = -normal->x;\n        normal->y = -normal->y;\n        normal->z = -normal->z;\n        return depth;", "        SwapNodes();\n        return depth;"),
    ("        SwapNodes();\n        const float depth = (float)other->Penetration", "        const float depth = (float)other->Penetration"),
    ("        DirectionFromWorld(m_jointIndex, node, direction, &local);\n        const AtVector3 between", "        local = *direction;\n        const AtVector3 between"),
    ("        local.Normalize();  // Again.\n", ""),
    ("        const double depth = D(between.z) * local.z + D(between.y) * local.y + D(local.x) * between.x - m_sphere.radius - theirs.radius;", "        const double depth = D(between.z) * local.z + D(between.y) * local.y + D(local.x) * between.x - m_sphere.radius;"),
    ("        const double depth = D(between.z) * local.z + D(between.y) * local.y + D(local.x) * between.x - m_sphere.radius - theirs.radius;", "        const double depth = D(between.z) * local.z + D(between.y) * local.y - m_sphere.radius - theirs.radius;"),
    ("        const double depth = D(between.z) * local.z + D(between.y) * local.y + D(local.x) * between.x - m_sphere.radius - theirs.radius;", "        const double depth = (float)(D(between.z) * local.z + D(between.y) * local.y + D(local.x) * between.x - m_sphere.radius - theirs.radius);"),
    ("        reversed.x = -direction->x;", "        reversed.x = direction->x;"), ("        reversed.y = -direction->y;", "        reversed.y = direction->y;"), ("        reversed.z = -direction->z;", "        reversed.z = direction->z;"),
    ("        const float depth = (float)other->PenetrationAlong(otherNode, this, node, &reversed, otherExtent, extent);", "        const double depth = other->PenetrationAlong(otherNode, this, node, &reversed, otherExtent, extent);"),
    ("    device->DrawSphere(&m_sphere, material, 6, 6, 0);", "    device->DrawSphere(&m_sphere, material, 6, 5, 0);"), ("    device->DrawBox(&m_box, material, 0);", "    device->DrawBox(&m_box, 0, 0);"),
    # The box's words.
    ("    if (token == kBoxShape) {\n        m_box.Read(tokenizer);\n        return;", "    if (token == kBoxShape) {\n        m_box.Read(tokenizer);"), ("    if (token != kBoxMinMax) {", "    if (false) {"), ("    if (token != kBoxMinMax) {\n        tokenizer->Error(kUnexpectedWord);\n        return;", "    if (token != kBoxMinMax) {\n        tokenizer->Error(kUnexpectedWord);"),
    ("    m_box.m_half.x = (float)(D(high.x) - low.x);", "    m_box.m_half.x = (float)(D(high.x));"), ("    m_box.m_half.y = (float)(D(high.y) - low.y);", "    m_box.m_half.y = (float)(D(high.y));"), ("    m_box.m_half.z = (float)(D(high.z) - low.z);", "    m_box.m_half.z = (float)(D(high.z));"),
    ("    m_box.m_half.x = (float)(D(m_box.m_half.x) * 0.5f);\n", ""), ("    m_box.m_half.y = (float)(D(m_box.m_half.y) * 0.5f);\n", ""), ("    m_box.m_half.z = (float)(D(m_box.m_half.z) * 0.5f);\n", ""),
    ("    m_box.m_center.x = (float)(D(low.x) + m_box.m_half.x);", "    m_box.m_center.x = (float)(D(low.x));"), ("    m_box.m_center.y = (float)(D(low.y) + m_box.m_half.y);", "    m_box.m_center.y = (float)(D(low.y));"), ("    m_box.m_center.z = (float)(D(low.z) + m_box.m_half.z);", "    m_box.m_center.z = (float)(D(low.z));"),
    ("    low.Read(tokenizer);\n    high.Read(tokenizer);", "    high.Read(tokenizer);\n    low.Read(tokenizer);"),
    # The box's overlaps.
    ("s_relativeAxes, &s_relativePosition) != 0 ? Found(this, other) : 0;", "s_relativeAxes, &s_relativePosition) != 0 ? 1 : 0;"), ("        return m_box.OverlapsSphere(&sphere) != 0 ? Found(this, other) : 0;", "        return m_box.OverlapsSphere(&sphere) != 0 ? 1 : 0;"),
    ("    return m_box.OverlapsSegment(&s_segmentDirection, &s_segmentMiddle, s_segmentHalfLength);", "    return m_box.OverlapsSegment(&s_segmentDirection, &s_segmentStart, s_segmentHalfLength);"),
    # The box's collection.
    ("    if ((faces & 0x3F) != 0 && AtSegmentQueryInsert(s_query, tStart) != 0) {", "    if ((faces & 0xFFF) != 0 && AtSegmentQueryInsert(s_query, tStart) != 0) {"), ("    if ((faces & 0x3F) != 0 && AtSegmentQueryInsert(s_query, tStart) != 0) {", "    if ((faces & 0x3F) != 0 && AtSegmentQueryInsert(s_query, tEnd) != 0) {"),
    ("    if ((faces & 0xFC0) != 0 && (s_query->m_flags & 0x10) == 0 && AtSegmentQueryInsert(s_query, tEnd) != 0) {", "    if ((s_query->m_flags & 0x10) == 0 && (faces & 0xFC0) != 0 && false) {"), ("(faces & 0xFC0) != 0 && (s_query->m_flags & 0x10) == 0 && AtSegmentQueryInsert", "(faces & 0xFC0) != 0 && AtSegmentQueryInsert"), ("(faces & 0xFC0) != 0 && (s_query->m_flags & 0x10) == 0 && AtSegmentQueryInsert(s_query, tEnd)", "(faces & 0xFC0) != 0 && (s_query->m_flags & 0x10) == 0 && AtSegmentQueryInsert(s_query, tStart)"),
    # The database.
    ("    AtBoundingGeometryDatabase::Clear();\n", ""), ("    if (m_groups != 0) {\n        operator delete(m_groups);\n    }\n", ""), ("    if (m_spheres != 0) {\n        operator delete(m_spheres);\n    }\n", ""), ("    if (m_boxes != 0) {\n        operator delete(m_boxes);\n    }\n", ""), ("    GmNameMap::Clear();\n", ""),
    ('    const int status = file.Open(name, "ASB", 2, 0);', '    const int status = file.Open(name, "ASN", 2, 0);'), ('file.Open(name, "ASB", 2, 0);', 'file.Open(name, "ASB", 1, 0);'), ("    if (status != 0) {\n        AtReportFileError", "    if (false) {\n        AtReportFileError"), ('AtReportFileError(status, name, "ASB");', 'AtReportFileError(status, name, "ASN");'), ("    Read(owner, file.m_tokenizer);\n", ""),
    ("    if (m_entries != 0) {\n        Clear();\n    }\n    tokenizer->SetKeywords", "    tokenizer->SetKeywords"), ("    tokenizer->SetKeywords(s_keywords, kKeywordCount, 0);", "    tokenizer->SetKeywords(s_keywords, kKeywordCount - 1, 0);"),
    ("    if (token == kNameSpace) {", "    if (false) {"), ("        ns = space;\n", ""), ("        tokenizer->ExpectName(space);\n        ns = space;\n        token = tokenizer->Next();", "        tokenizer->ExpectName(space);\n        ns = space;"),
    ("        if (token == kEndOfFile) {", "        if (false) {"), ("            Init(m_groupCount + m_sphereCount + m_boxCount);\n            owner->AddGeometry(this, ns);\n            return;", "            owner->AddGeometry(this, ns);\n            return;"), ("            Init(m_groupCount + m_sphereCount + m_boxCount);\n            owner->AddGeometry(this, ns);\n            return;", "            Init(m_groupCount + m_sphereCount + m_boxCount);\n            return;"), ("            Init(m_groupCount + m_sphereCount + m_boxCount);", "            Init(m_groupCount + m_sphereCount);"),
    ("        if (token == kBoxWord) {\n            if (m_boxCount != 0) {", "        if (token == kBoxWord) {\n            if (false) {"), ("        } else if (token == kSphereWord) {\n            if (m_sphereCount != 0) {", "        } else if (token == kSphereWord) {\n            if (false) {"), ("        } else if (token == kGroupWord) {\n            if (m_groupCount != 0) {", "        } else if (token == kGroupWord) {\n            if (false) {"),
    ("ReadSize(tokenizer, &m_boxCount)", "ReadSize(tokenizer, &m_groupCount)"), ("ReadSize(tokenizer, &m_sphereCount)", "ReadSize(tokenizer, &m_boxCount)"), ("ReadSize(tokenizer, &m_groupCount)", "ReadSize(tokenizer, &m_sphereCount)"), ("m_spheres = NewGeometries<AtSphereBoundingGeometry>", "m_groups = (AtGroupBoundingGeometry*)NewGeometries<AtSphereBoundingGeometry>"),
    ("            m_boxes = NewGeometries<AtBoxBoundingGeometry>(ReadSize(tokenizer, &m_boxCount));\n        } else if", "            NewGeometries<AtBoxBoundingGeometry>(ReadSize(tokenizer, &m_boxCount));\n        } else if"),
    ("    const unsigned int total = m_groupCount + m_sphereCount + m_boxCount;", "    const unsigned int total = m_groupCount + m_sphereCount;"), ("    Init(total);\n", ""), ("    Init(total);\n    owner->AddGeometry(this, ns);\n", "    Init(total);\n"), ("    if (listed > total) {", "    if (listed >= total) {"), ("    if (listed > total) {", "    if (false) {"), ("        tokenizer->Error(kAtProblemIsEmpty);", "        tokenizer->Error(kUnexpectedWord);"),
    ("            if (boxes >= m_boxCount) {", "            if (false) {"), ("            if (spheres >= m_sphereCount) {", "            if (false) {"), ("            if (groups >= m_groupCount) {", "            if (false) {"), ("            geometry = m_boxes + boxes;", "            geometry = m_boxes;"), ("            geometry = m_spheres + spheres;", "            geometry = m_spheres;"), ("            geometry = m_groups + groups;", "            geometry = m_groups;"),
    ("            boxes++;\n", ""), ("            spheres++;\n", ""), ("            groups++;\n", ""), ("            continue;\n", "            geometry = m_boxes;\n"), ("        tokenizer->ExpectName(name);\n        Insert(name, geometry);", "        tokenizer->ExpectName(name);"), ("        geometry->Read(this, tokenizer);\n", ""), ("    tokenizer->ExpectRightCurly();\n", ""),
]:
    GEO.append(one(G, a, b))
# The list of hits, in the file of the class it now belongs to.
for a, b in [
    ("    m_first0 = &m_vectors0[0];", "    m_first0 = &m_vectors0[1];"), ("    m_second0 = &m_vectors0[1];", "    m_second0 = &m_vectors0[0];"), ("    m_first2 = &m_vectors2[0];", "    m_first2 = &m_vectors2[1];"), ("    m_second2 = &m_vectors2[1];", "    m_second2 = &m_vectors2[0];"), ("    m_vectors0[0].x = 0.0f;\n", ""), ("    m_vectors0[1].z = 0.0f;\n", ""), ("    m_vectors1[0].x = 0.0f;\n", ""), ("    m_vectors1[1].z = 0.0f;\n", ""), ("    m_vectors2[0].x = 0.0f;\n", ""), ("    m_vectors2[1].z = 0.0f;\n", ""),
    ("    m_unknown60[0] = 0;\n", ""), ("    m_unknown60[3] = 0;\n", ""), ("    m_unknown60[4] = 0;\n", ""), ("    m_unknown60[5] = 0;\n", ""), ("    m_flags = 0;\n", ""), ("    m_capacity = 0;\n", ""), ("    m_keys = 0;\n", ""), ("    m_hits = 0;\n", ""), ("    m_count = 0;\n", ""), ("    m_unknown8C = 0;\n", ""),
    ("AtSegmentHitList::~AtSegmentHitList() {\n    if ((m_flags & kNotOwned) == 0) {", "AtSegmentHitList::~AtSegmentHitList() {\n    if (true) {"), ("void AtSegmentHitList::Clear() {\n    if ((m_flags & kNotOwned) == 0) {", "void AtSegmentHitList::Clear() {\n    if (true) {"),
    ("    if (m_count < m_capacity) {", "    if (true) {"), ("    if (m_capacity != 0) {\n        Clear();\n    }\n    m_hits = hits;", "    m_hits = hits;"), ("    m_hits = hits;\n", ""), ("    m_keys = keys;\n", ""), ("    m_capacity = capacity;\n    m_keys = keys;", "    m_keys = keys;"), ("    m_flags |= kNotOwned;\n", ""),
    ("    if (m_capacity != 0) {\n        Clear();\n    }\n    m_capacity = capacity;\n    if (s_heap", "    m_capacity = capacity;\n    if (s_heap"),
    ("    m_keys = operator new(m_capacity * kKeySize);", "    m_keys = operator new(m_capacity * 4);"), ("    m_hits = operator new(m_capacity * kHitSize);", "    m_hits = operator new(m_capacity * 4);"), ("        keyBytes[i] = 0;\n", "        ;\n"), ("    for (unsigned int i = 0; i < m_capacity * kKeySize; i++) {", "    for (unsigned int i = 0; i < m_capacity; i++) {"),
    ("    if (m_keys == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (false) {\n        AtRaise(-4, 0, 0, 0);"), ("    if (m_hits == 0) {\n        AtRaise(-4, 0, 0, 0);", "    if (false) {\n        AtRaise(-4, 0, 0, 0);"),
]:
    GEO.append(one(Q, a, b))
GEO += all_of(G, "RelateTo(other);\n", "\n")
GEO += all_of(G, "        sphere.radius = theirs.radius;\n", "")
GEO += all_of(G, "        sphere.center = StoredRelative(m_box.m_center, theirs.center);", "        sphere.center = theirs.center;")
GEO += all_of(G, "    SetDrawTransform(device, node, coordSys);\n", "")
GEO += all_of(G, "    NormalToWorld(m_jointIndex, node, &local, normal);\n", "")
GEO += all_of(G, "        s_query->m_records[s_query->m_current].m_geometry = this;\n        s_query->m_records[s_query->m_current].m_faces = faces;", "        s_query->m_records[s_query->m_current].m_faces = faces;")
GEO += all_of(G, "        s_query->m_records[s_query->m_current].m_faces = faces;\n", "")
GEO += all_of(G, "        AtSegmentQueryFinish(s_query, 2);", "        AtSegmentQueryFinish(s_query, 3);")
GEO += all_of(G, "        *extent = (float)(D(m_sphere.radius) + m_sphere.radius);", "        *extent = m_sphere.radius;")
GEO += all_of(G, "        *otherExtent = (float)(D(theirs.radius) + theirs.radius);", "        *otherExtent = theirs.radius;")
GEO += all_of(G, "    float length = (float)(D(s_segmentHalfLength) + s_segmentHalfLength);", "    float length = s_segmentHalfLength;")
GEO += all_of(G, "        DirectionFromWorld(m_jointIndex, node, direction, &local);\n        return m_box", "        local = *direction;\n        return m_box")

DRAW = []
for a, b in [
    ("(float)(D(box->m_center.x) + D(sx) * box->m_half.x);", "(float)(D(box->m_center.x) + box->m_half.x);"), ("(float)(D(box->m_center.y) + D(sy) * box->m_half.y);", "(float)(D(box->m_center.y) + box->m_half.y);"), ("(float)(D(box->m_center.z) + D(sz) * box->m_half.z);", "(float)(D(box->m_center.z) + box->m_half.z);"),
    ("        m_mask1 = 0;\n", "        m_mask1 = 1;\n"), ("        flags &= 0xFFFFFDFFu;\n", ""), ("        m_mask1 = 0x1F;\n", "        m_mask1 = 0;\n"), ("        if (count > 4) {", "        if (count > 5) {"), ("        flags |= 0x200;\n", ""), ("    m_count = count;\n", ""), ("    m_flags = flags;\n", ""), ("    m_mask0 = 0x1F;", "    m_mask0 = 0;"), ("(((flags >> 10) & 1) << 3)", "(((flags >> 10) & 1) << 2)"), ("    m_mask3 = (flags & 0x800) != 0 ? 0x1F : 0;", "    m_mask3 = 0;"),
    ("        flags &= 0x40007FFFu;\n", ""), ("        if (texCoords > 8) {", "        if (texCoords > 9) {"), ("        flags |= 0x8000;\n", ""), ("    m_attributes.Set(flags, count);", "    m_attributes.Set(flags, 0);"), ("    m_texCoordCount = texCoords;\n", ""), ("    m_mask4 = (m_attributes.m_flags & 0x2000) != 0 ? 0x1E : 0;", "    m_mask4 = 0;"), ("    m_mask5 = (m_attributes.m_flags & 0x4000) != 0 ? 0x1E : 0;", "    m_mask5 = 0;"), ("        m_texCoordMasks[i] = 0x1F;", "        m_texCoordMasks[i] = 0;"), ("    for (; i < 8; i++) {\n        m_texCoordMasks[i] = 0;\n    }\n", ""),
    ("    format.Set(0xA080, 0, 1);", "    format.Set(0x8080, 0, 1);"), ("    buffer->Lock(&format, 8, &locked);", "    buffer->Lock(&format, 7, &locked);"), ("    buffer->LockIndices(36, &indices);", "    buffer->LockIndices(35, &indices);"), ("{1, -1, 1}, {-1, 1, 1}", "{-1, 1, 1}, {1, -1, 1}"), ("{1, -1, -1}, {-1, 1, -1}", "{-1, 1, -1}, {1, -1, -1}"), ("{{1, 0}, {1, 1}, {0, 0}", "{{1, 1}, {1, 0}, {0, 0}"), ("        uv[0] = kTexCoords[i][0];\n", ""), ("        uv[1] = kTexCoords[i][1];\n", ""), ("{0, 1, 2, 1, 3, 2,", "{0, 2, 1, 1, 3, 2,"), ("6, 7, 4, 7, 5, 4}", "6, 7, 4, 7, 4, 5}"), ("    buffer->UnlockIndices(36);", "    buffer->UnlockIndices(35);"), ("    buffer->Unlock(8);", "    buffer->Unlock(7);"),
    ("    const unsigned int vertices = stacks * slices + 2;", "    const unsigned int vertices = stacks * slices + 1;"), ("    unsigned int indexCount = slices * 6;", "    unsigned int indexCount = slices * 3;"), ("    if (stacks > 1) {", "    if (stacks > 2) {"), ("        indexCount += (stacks - 1) * slices * 6;", "        indexCount += stacks * slices * 6;"), ("    format.Set(0x8080, 0, 1);", "    format.Set(0xA080, 0, 1);"),
    ("    top->x = sphere->center.x;", "    top->x = 0;"), ("    top->y = sphere->center.y;", "    top->y = 0;"), ("    top->z = (float)(D(sphere->center.z) + radius);", "    top->z = (float)(D(sphere->center.z));"), ("    unsigned int written = 1;", "    unsigned int written = 0;"), ("(float)(D(kPi) / FromUnsigned(stacks + 1));", "(float)(D(kPi) / FromUnsigned(stacks + 2));"),
    ("D(kHalfPi) - FromUnsigned(stack + 1) * step;", "D(kHalfPi) - FromUnsigned(stack) * step;"), ("const float kHalfPi = 1.57079637f;", "const float kHalfPi = 1.5707f;"), ("const float kTwoPi = 6.28318548f;", "const float kTwoPi = 6.283f;"), ("const float kPi = 3.14159274f;", "const float kPi = 3.1415f;"),
    ("            const float sinLatitude = (float)AtSin(latitude);", "            const float sinLatitude = (float)AtCos(latitude);"), ("            const double cosLatitude = AtCos(latitude);", "            const double cosLatitude = (float)AtCos(latitude);"), ("const double angle = FromUnsigned(slice) * kTwoPi / sliceCount;", "const double angle = FromUnsigned(slice + 1) * kTwoPi / sliceCount;"),
    ("to->y = (float)(AtSin(angle) * cosLatitude * radius + sphere->center.y);", "to->y = (float)(AtSin(stored) * cosLatitude * radius + sphere->center.y);"), ("to->y = (float)(AtSin(angle) * cosLatitude * radius + sphere->center.y);", "to->y = (float)(AtSin(angle) * cosLatitude * radius);"), ("to->z = (float)(D(sinLatitude) * radius + sphere->center.z);", "to->z = (float)(D(sinLatitude) * radius);"), ("to->x = (float)(AtCos(stored) * cosLatitude * radius + sphere->center.x);", "to->x = (float)(AtCos(angle) * cosLatitude * radius + sphere->center.x);"), ("to->x = (float)(AtCos(stored) * cosLatitude * radius + sphere->center.x);", "to->x = (float)(AtCos(stored) * radius + sphere->center.x);"),
    ("            written += slices;\n", ""), ("        ringStart = written;\n", ""),
    ("            if (stack == 0) {", "            if (stack == 1) {"), ("index[1] = (unsigned short)(slice == slices - 1 ? 1 : slice + 2);", "index[1] = (unsigned short)(slice + 2);"), ("                index[2] = (unsigned short)(slice + 1);", "                index[2] = (unsigned short)(slice);"), ("                index[0] = 0;", "                index[0] = 1;"),
    ("                const unsigned int previous = ringStart - slices + slice;", "                const unsigned int previous = ringStart + slice;"), ("                if (slice == slices - 1) {", "                if (slice == slices) {"), ("                    index[0] = (unsigned short)(ringStart - slices);", "                    index[0] = (unsigned short)(ringStart);"), ("                    index[2] = (unsigned short)(ringStart - 1);", "                    index[2] = (unsigned short)(ringStart);"), ("                    index[3] = (unsigned short)(ringStart - 1);", "                    index[3] = (unsigned short)(ringStart);"),
    ("                    index[0] = (unsigned short)(previous + 1);", "                    index[0] = (unsigned short)(previous);"), ("                    index[1] = (unsigned short)(ringStart + slice + 1);", "                    index[1] = (unsigned short)(ringStart + slice);"), ("                    index[2] = (unsigned short)previous;", "                    index[2] = (unsigned short)ringStart;"), ("                    index[3] = (unsigned short)previous;", "                    index[3] = (unsigned short)ringStart;"), ("                    index[4] = (unsigned short)(ringStart + slice + 1);", "                    index[4] = (unsigned short)(ringStart + slice);"), ("                index[5] = (unsigned short)(ringStart + slice);", "                index[5] = (unsigned short)(ringStart);"),
    ("    bottom->z = (float)(D(sphere->center.z) - radius);", "    bottom->z = (float)(D(sphere->center.z));"), ("    bottom->y = sphere->center.y;", "    bottom->y = 0;"), ("    bottom->x = sphere->center.x;", "    bottom->x = 0;"), ("    const unsigned int last = ringStart - slices;", "    const unsigned int last = ringStart;"), ("        index[0] = (unsigned short)written;", "        index[0] = (unsigned short)last;"), ("        index[1] = (unsigned short)(last + slice);", "        index[1] = (unsigned short)(last);"), ("index[2] = (unsigned short)(slice == slices - 1 ? last : last + slice + 1);", "index[2] = (unsigned short)(last + slice + 1);"),
    ("    buffer->UnlockIndices(indexCount);", "    buffer->UnlockIndices(0);"), ("    buffer->Unlock(vertices);", "    buffer->Unlock(0);"), ("    buffer->LockIndices(indexCount, &index);", "    buffer->LockIndices(0, &index);"),
]:
    DRAW.append(one(D, a, b))
DRAW += all_of(D, "    buffer->Draw(this, material, 1, &instance);", "    buffer->Draw(this, 0, 1, &instance);")
DRAW += all_of(D, "    AtGeometryBuffer* buffer = m_source->Buffer();", "    AtGeometryBuffer* buffer = (m_source->Buffer(), m_source->Buffer());")

MUTANTS = BOX + MATH + GEO + DRAW

# Each text must occur exactly once; the runner checks.
SUITE = {"target": "oracle_asb", "subsystem": "at/asb", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
