# Mutants of the matrix table's functions. Each text occurs exactly once in the source.
F = "AtMatrix.cpp"
C = "AtCoordSys.cpp"

_PRODUCT_B = "    const float* a = first->m;\n    float b[16];\n    for (int i = 0; i < 16; i++) {\n        b[i] = second->m[i];\n    }\n"
_POINT_HEAD = ("    // Stored after every step, and from read again each time: if from is to, it reads what it has written.\n    const float* m = matrix->m;\n"
               "    to->x = (float)((double)m[0] * from->x);\n    to->y = (float)((double)m[1] * from->x);\n")
_DIRECTION_HEAD = ("void AtMatrixTransformDirection(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {\n    const float* m = matrix->m;\n"
                   "    to->x = (float)((double)m[0] * from->x);\n    to->y = (float)((double)m[1] * from->x);\n")
_SET_T = "void AtMatrixSetTranslation(AtMatrix* m, const AtVector3* position) {\n    m->m[12] = position->x;\n    m->m[13] = position->y;\n    m->m[14] = position->z;\n"
_INVERSE_HEAD = ("    t[12] = (float)-((double)f[2] * f[14] + (double)f[1] * f[13] + (double)f[0] * f[12]);\n"
                 "    t[13] = (float)-((double)f[5] * f[13] + (double)f[14] * f[6] + (double)f[12] * f[4]);\n")

MUTANTS = [
    # Copies: every element, bit for bit.
    ("copy: first element only", F, "    *to = *from;\n", "    to->m[0] = from->m[0];\n"),
    ("copy words: one short", F, "    for (int i = 0; i < 16; i++) {\n        to->m[i] = from->m[i];\n", "    for (int i = 0; i < 15; i++) {\n        to->m[i] = from->m[i];\n"),
    ("identity: 5", F, "    m->m[4] = 0.0f;\n    m->m[5] = 1.0f;\n", "    m->m[4] = 0.0f;\n    m->m[5] = 0.0f;\n"),
    ("identity: 0 left", F, "void AtMatrixIdentity(AtMatrix* m) {\n    m->m[0] = 1.0f;\n", "void AtMatrixIdentity(AtMatrix* m) {\n"),
    ("translation: y from x", F, _SET_T, _SET_T.replace("m->m[13] = position->y;", "m->m[13] = position->x;")),
    ("translation: z left", F, _SET_T, _SET_T.replace("    m->m[14] = position->z;\n", "")),
    ("translation: through a register", F, _SET_T, _SET_T.replace("m->m[12] = position->x;", "m->m[12] = ThroughRegister(position->x);")),
    ("scale: z from y", F, "    m->m[9] = 0.0f;\n    m->m[10] = scale->z;\n", "    m->m[9] = 0.0f;\n    m->m[10] = scale->y;\n"),
    ("scale: 15", F, "    m->m[15] = 1.0f;\n}\n\nvoid AtMatrixSetUniformScale", "    m->m[15] = 0.0f;\n}\n\nvoid AtMatrixSetUniformScale"),
    ("uniform scale: 0 not through a register", F, "    m->m[0] = ThroughRegister(scale);\n", "    m->m[0] = scale;\n"),
    ("uniform scale: 5 through a register", F, "    m->m[5] = scale;\n", "    m->m[5] = ThroughRegister(scale);\n"),
    ("uniform scale: 15 left", F, "    m->m[15] = 1.0f;\n}\n\nvoid AtMatrixSetInverse", "}\n\nvoid AtMatrixSetInverse"),
    # Holding the value as a float instead is equivalent on this x86 harness: a float returned goes through st(0), which quiets it all the same. tests/native/test_matrix.cpp checks the quieting on x64,
    # where it would not be. So that mutant is left out.
    ("position: y from x", F, "    position->y = m->m[13];\n", "    position->y = m->m[12];\n"),
    ("axes: y's z", F, "    y->z = m->m[6];\n", "    y->z = m->m[7];\n"),
    ("axes: z's x left", F, "    z->x = m->m[8];\n", ""),

    # Rotation.
    ("rotation: 1 over", F, "    const double s = 2.0 / (x * x + y * y + z * z + w * w);", "    const double s = 1.0 / (x * x + y * y + z * z + w * w);"),
    ("rotation: sum order", F, "    const double s = 2.0 / (x * x + y * y + z * z + w * w);", "    const double s = 2.0 / (w * w + z * z + y * y + x * x);"),
    ("rotation: xs stored", F, "    const double xs = s * q->x;\n", "    const float xs = (float)(s * q->x);\n"),
    ("rotation: ys kept", F, "    const float ys = (float)(s * q->y);\n", "    const double ys = s * q->y;\n"),
    ("rotation: zz stored", F, "    const double zz = (double)zs * q->z;\n", "    const float zz = (float)((double)zs * q->z);\n"),
    ("rotation: yy kept", F, "    const float yy = (float)((double)ys * q->y);\n", "    const double yy = (double)ys * q->y;\n"),
    ("rotation: 1 sign", F, "    m->m[1] = (float)(xy - wz);\n", "    m->m[1] = (float)(xy + wz);\n"),
    ("rotation: 5 order", F, "    m->m[5] = (float)(1.0 - (zz + xx));\n", "    m->m[5] = (float)(1.0 - zz - xx);\n"),
    ("rotation: 6 wy", F, "    m->m[6] = (float)((double)yz - wx);\n", "    m->m[6] = (float)((double)yz - wy);\n"),
    ("rotation: 10 left", F, "    m->m[10] = (float)(1.0 - (yy + xx));\n", ""),

    # Inverse.
    ("inverse: 12 order", F, "    t[12] = (float)-((double)f[2] * f[14] + (double)f[1] * f[13] + (double)f[0] * f[12]);\n",
     "    t[12] = (float)-((double)f[0] * f[12] + (double)f[1] * f[13] + (double)f[2] * f[14]);\n"),
    ("inverse: 13 element", F, "(double)f[14] * f[6]", "(double)f[14] * f[7]"),
    ("inverse: 14 not negated", F, "    const double z = -((double)f[10]", "    const double z = ((double)f[10]"),
    ("inverse: in place, position read before written", F, _INVERSE_HEAD,
     "    const float p12 = (float)-((double)f[2] * f[14] + (double)f[1] * f[13] + (double)f[0] * f[12]);\n"
     "    t[13] = (float)-((double)f[5] * f[13] + (double)f[14] * f[6] + (double)f[12] * f[4]);\n    t[12] = p12;\n"),
    ("inverse: 1 not transposed", F, "    t[1] = f[4];\n", "    t[1] = f[1];\n"),
    ("inverse: 3 left", F, "    t[3] = 0.0f;\n", ""),
    ("inverse: 11 left", F, "    t[11] = 0.0f;\n", ""),
    ("inverse: 15 left", F, "    t[15] = 1.0f;\n", ""),

    # The x87 product and Multiply.
    ("product words: 0 order", F, "    to[0] = (float)((double)a[0] * b[0] + (double)a[3] * b[12] + (double)a[2] * b[8] + (double)a[1] * b[4]);",
     "    to[0] = (float)((double)a[0] * b[0] + (double)a[1] * b[4] + (double)a[2] * b[8] + (double)a[3] * b[12]);"),
    ("product words: 5 order", F, "    to[5] = (float)((double)a[6] * b[9] + (double)a[4] * b[1] + (double)a[7] * b[13] + (double)a[5] * b[5]);",
     "    to[5] = (float)((double)a[4] * b[1] + (double)a[5] * b[5] + (double)a[6] * b[9] + (double)a[7] * b[13]);"),
    ("product words: 14 element", F, "(double)a[13] * b[6] + (double)a[14] * b[10]);", "(double)a[13] * b[6] + (double)a[14] * b[11]);"),
    ("product words: 15 stored products", F, "    to[15] = (float)((double)a[15] * b[15] + (double)a[13] * b[7] + (double)a[12] * b[3] + (double)a[14] * b[11]);",
     "    to[15] = (float)(a[15] * b[15] + a[13] * b[7] + a[12] * b[3] + a[14] * b[11]);"),
    ("multiply: on the right", F, "    AtMatrixSetProductWords(m, by, &was);\n", "    AtMatrixSetProductWords(m, &was, by);\n"),
    ("multiply: no copy", F, "    AtMatrixCopyWords(&was, m);\n    AtMatrixSetProductWords(m, by, &was);\n", "    AtMatrixSetProductWords(m, by, m);\n    (void)was;\n"),
    ("multiply: by copied first", F, "    AtMatrixSetProductWords(m, by, &was);\n", "    AtMatrix first = *by;\n    AtMatrixSetProductWords(m, &first, &was);\n"),

    # The SSE product.
    ("product: sum order", F, "            row[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];",
     "            row[i * 4 + j] = a[i * 4 + 3] * b[12 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 1] * b[4 + j] + a[i * 4] * b[j];"),
    ("product: in double", F, "            row[(i - 2) * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];",
     "            row[(i - 2) * 4 + j] = (float)((double)a[i * 4] * b[j] + (double)a[i * 4 + 1] * b[4 + j] + (double)a[i * 4 + 2] * b[8 + j] + (double)a[i * 4 + 3] * b[12 + j]);"),
    ("product: b read as it goes", F, _PRODUCT_B, "    const float* a = first->m;\n    const float* b = second->m;\n"),
    ("product: all of a read first", F, _PRODUCT_B, "    float a[16];\n    for (int i = 0; i < 16; i++) {\n        a[i] = first->m[i];\n    }\n    float b[16];\n    for (int i = 0; i < 16; i++) {\n        b[i] = second->m[i];\n    }\n"),
    ("product: last row left", F, "    for (int k = 0; k < 8; k++) {\n        product->m[8 + k] = row[k];\n", "    for (int k = 0; k < 4; k++) {\n        product->m[8 + k] = row[k];\n"),

    # Transforms.
    ("point: from read once", F, _POINT_HEAD,
     "    const float* m = matrix->m;\n    const float fx = from->x;\n    to->x = (float)((double)m[0] * fx);\n    to->y = (float)((double)m[1] * fx);\n"),
    ("point: position 13 for 12", F, "    to->x = (float)((double)m[12] + to->x);\n", "    to->x = (float)((double)m[13] + to->x);\n"),
    ("point: z position left", F, "    to->z = (float)((double)m[14] + to->z);\n", ""),
    ("direction: from read once", F, _DIRECTION_HEAD,
     "void AtMatrixTransformDirection(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {\n    const float* m = matrix->m;\n    const float fx = from->x;\n"
     "    to->x = (float)((double)m[0] * fx);\n    to->y = (float)((double)m[1] * fx);\n"),
    ("direction: product rounded first", F, "    to->z = (float)((double)m[10] * from->z + to->z);\n}\n\nvoid AtMatrixGetPosition", "    to->z = m[10] * from->z + to->z;\n}\n\nvoid AtMatrixGetPosition"),

    # Look-at, and what its two variants share.
    ("look at: not normalised", F, "    forward->SetNormalized(*direction);\n", "    *forward = *direction;\n"),
    ("look at: dot order", F, "    const double along = (double)forward->y * up->y + (double)forward->x * up->x + (double)forward->z * up->z;",
     "    const double along = (double)forward->z * up->z + (double)forward->y * up->y + (double)forward->x * up->x;"),
    ("look at: product stored", F, "    above->x = (float)(up->x - forward->x * along);\n", "    above->x = (float)(up->x - (float)(forward->x * along));\n"),
    ("look at: above not normalised", F, "    above->Normalize();\n", ""),
    ("look at: side y sign", F, "    const double sideY = (double)above.z * forward.x - (double)above.x * forward.z;", "    const double sideY = (double)above.x * forward.z - (double)above.z * forward.x;"),
    ("look at: 4 from side y", F, "    m->m[4] = (float)sideX;\n", "    m->m[4] = (float)sideY;\n"),
    ("look at: 9 from x", F, "    m->m[9] = above.y;\n", "    m->m[9] = above.x;\n"),
    ("look at: 0 left", F, "    m->m[0] = forward.x;\n    m->m[4] = (float)sideX;", "    m->m[4] = (float)sideX;"),
    ("look at zy: side x sign", F, "    const double sideX = (double)forward.z * above.y - (double)forward.y * above.z;", "    const double sideX = (double)forward.y * above.z - (double)forward.z * above.y;"),
    ("look at zy: 9 from x", F, "    m->m[8] = forward.x;\n    m->m[9] = forward.y;\n", "    m->m[8] = forward.x;\n    m->m[9] = forward.x;\n"),
    ("look at zy: 2 from side y", F, "    m->m[2] = (float)sideZ;\n", "    m->m[2] = (float)sideY;\n"),
    ("look at xy: z sign", F, "    const float sideZ = (float)((double)above.y * forward.x - (double)above.x * forward.y);\n    m->m[10] = sideZ;",
     "    const float sideZ = (float)((double)above.y * forward.x + (double)above.x * forward.y);\n    m->m[10] = sideZ;"),
    ("look at xy: 9 from side x", F, "    m->m[9] = (float)sideY;\n}", "    m->m[9] = (float)sideX;\n}"),
    ("look at xy: 6 left", F, "    m->m[5] = above.y;\n    m->m[6] = above.z;\n    const float sideZ", "    m->m[5] = above.y;\n    const float sideZ"),

    # The further slots.
    ("scale 3x3: 10 from y", F, "    m->m[10] = scale->z;\n    m->m[1] = 0.0f;", "    m->m[10] = scale->y;\n    m->m[1] = 0.0f;"),
    ("scale 3x3: 9 left", F, "    m->m[8] = 0.0f;\n    m->m[9] = 0.0f;\n}\n\nvoid AtMatrixSetAxes", "    m->m[8] = 0.0f;\n}\n\nvoid AtMatrixSetAxes"),
    ("set axes: y's z", F, "    m->m[6] = y->z;\n", "    m->m[6] = y->y;\n"),
    ("make translation: 15", F, "    m->m[14] = position->z;\n    m->m[15] = 1.0f;\n}", "    m->m[14] = position->z;\n    m->m[15] = 0.0f;\n}"),
    ("make translation: 7 left", F, "    m->m[6] = 0.0f;\n    m->m[7] = 0.0f;\n    m->m[8] = 0.0f;\n    m->m[9] = 0.0f;\n    m->m[11] = 0.0f;", "    m->m[6] = 0.0f;\n    m->m[8] = 0.0f;\n    m->m[9] = 0.0f;\n    m->m[11] = 0.0f;"),
    ("outside 3x3: 13 left", F, "    m->m[12] = 0.0f;\n    m->m[13] = 0.0f;\n    m->m[14] = 0.0f;\n    m->m[15] = 1.0f;\n}\n\n}  // namespace", "    m->m[12] = 0.0f;\n    m->m[14] = 0.0f;\n    m->m[15] = 1.0f;\n}\n\n}  // namespace"),
    ("make rotation: not cleared", F, "    AtMatrixSetRotation(m, rotation);\n    ClearOutside3x3(m);", "    AtMatrixSetRotation(m, rotation);"),
    ("make from axes: not cleared", F, "    AtMatrixSetAxes(m, x, y, z);\n    ClearOutside3x3(m);", "    AtMatrixSetAxes(m, x, y, z);"),
    ("axis angle: negated", F, "void AtMatrixSetAxisAngle(AtMatrix* m, const AtVector3* axis, float angle) {\n    AtQuaternion rotation;\n    rotation.SetAxisAngle(*axis, angle);",
     "void AtMatrixSetAxisAngle(AtMatrix* m, const AtVector3* axis, float angle) {\n    AtQuaternion rotation;\n    rotation.SetAxisAngle(*axis, -angle);"),
    ("make axis angle: not cleared", F, "    AtMatrixSetRotation(m, &rotation);\n    ClearOutside3x3(m);", "    AtMatrixSetRotation(m, &rotation);"),
    ("set element: column major", F, "    m->m[row * 4 + column] = ThroughRegister(value);", "    m->m[column * 4 + row] = ThroughRegister(value);"),
    ("set element: as a word", F, "    m->m[row * 4 + column] = ThroughRegister(value);", "    m->m[row * 4 + column] = value;"),
    ("get element: column major", F, "    return m->m[row * 4 + column];", "    return m->m[column * 4 + row];"),
    ("x axis: z from 3", F, "    axis->z = m->m[2];\n", "    axis->z = m->m[3];\n"),
    ("y axis: x from 0", F, "    axis->x = m->m[4];\n", "    axis->x = m->m[0];\n"),
    ("z axis: y from 5", F, "    axis->y = m->m[9];\n", "    axis->y = m->m[5];\n"),
    ("multiply right: on the left", F, "    AtMatrixSetProductWords(m, &was, by);\n", "    AtMatrixSetProductWords(m, by, &was);\n    (void)0;\n"),
    ("multiply right: no copy", F, "    AtMatrixCopyWords(&was, m);\n    AtMatrixSetProductWords(m, &was, by);\n", "    AtMatrixSetProductWords(m, m, by);\n    (void)was;\n"),
    ("point z: rounded", F, "    return (double)m[6] * point->y + (double)m[2] * point->x + (double)m[10] * point->z + m[14];",
     "    return (float)((double)m[6] * point->y + (double)m[2] * point->x + (double)m[10] * point->z + m[14]);"),
    ("point z: order", F, "    return (double)m[6] * point->y + (double)m[2] * point->x + (double)m[10] * point->z + m[14];",
     "    return (double)m[10] * point->z + (double)m[2] * point->x + (double)m[6] * point->y + m[14];"),
    ("inverse point: x order", F, "    to->x = (float)(to->x - ((double)m[1] * m[13] + (double)m[12] * m[0] + (double)m[14] * m[2]));",
     "    to->x = (float)(to->x - ((double)m[14] * m[2] + (double)m[12] * m[0] + (double)m[1] * m[13]));"),
    ("inverse point: z element", F, "    to->z = (float)(to->z - ((double)m[9] * m[13] + (double)m[14] * m[10] + (double)m[12] * m[8]));",
     "    to->z = (float)(to->z - ((double)m[9] * m[13] + (double)m[14] * m[10] + (double)m[12] * m[9]));"),
    ("inverse point: from read once", F, "void AtMatrixInverseTransformPoint(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {\n    const float* m = matrix->m;\n    to->x = (float)((double)m[0] * from->x);\n    to->y = (float)((double)m[4] * from->x);",
     "void AtMatrixInverseTransformPoint(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {\n    const float* m = matrix->m;\n    const float fx = from->x;\n    to->x = (float)((double)m[0] * fx);\n    to->y = (float)((double)m[4] * fx);"),
    ("inverse direction: not transposed", F, "void AtMatrixInverseTransformDirection(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {\n    const float* m = matrix->m;\n    to->x = (float)((double)m[0] * from->x);\n    to->y = (float)((double)m[4] * from->x);",
     "void AtMatrixInverseTransformDirection(const AtMatrix* matrix, const AtVector3* from, AtVector3* to) {\n    const float* m = matrix->m;\n    to->x = (float)((double)m[0] * from->x);\n    to->y = (float)((double)m[1] * from->x);"),
    ("inverse direction: product rounded first", F, "    to->z = (float)((double)m[10] * from->z + to->z);\n}\n\nvoid AtMatrixScaleAxes", "    to->z = m[10] * from->z + to->z;\n}\n\nvoid AtMatrixScaleAxes"),
    ("scale axes: y by x", F, "        m->m[j] = y * m->m[j];", "        m->m[j] = x * m->m[j];"),
    ("scale axes: fourth column left", F, "    for (int j = 8; j < 12; j++) {\n        m->m[j] = z * m->m[j];", "    for (int j = 8; j < 11; j++) {\n        m->m[j] = z * m->m[j];"),
    ("transpose: not at all", F, "            m->m[i * 4 + j] = was.m[j * 4 + i];", "            m->m[i * 4 + j] = was.m[i * 4 + j];"),
    ("scale column x: 12 left", F, "    m->m[12] = scale * m->m[12];\n", ""),
    ("scale column y: 5 for 4", F, "    m->m[5] = scale * m->m[5];\n", "    m->m[5] = scale * m->m[4];\n"),
    ("invert: 12 order", F, "    m[12] = (float)-(p13 * m[1] + (double)m[14] * m[2] + p12 * m[0]);", "    m[12] = (float)-(p12 * m[0] + p13 * m[1] + (double)m[14] * m[2]);"),
    ("invert: position read again", F, "    m[13] = (float)-(p13 * m[5] + (double)m[14] * m[6] + p12 * m[4]);", "    m[13] = (float)-((double)m[13] * m[5] + (double)m[14] * m[6] + (double)m[12] * m[4]);"),
    ("invert: 14 order", F, "    m[14] = (float)-((double)m[10] * m[14] + p13 * m[9] + p12 * m[8]);", "    m[14] = (float)-(p12 * m[8] + p13 * m[9] + (double)m[10] * m[14]);"),
    ("invert: 4 as a word", F, "    m[4] = ThroughRegister(m1);", "    m[4] = m1;"),
    ("invert: 9 from 9", F, "    m[9] = m6;\n", "    m[9] = m9;\n"),
    ("translate after: by y for x", F, "        m[r] = (float)((double)m[r + 3] * by->x + m[r]);", "        m[r] = (float)((double)m[r + 3] * by->y + m[r]);"),
    ("translate after: last row left", F, "    for (int i = 0; i < 4; i++) {\n        const int r = i * 4;", "    for (int i = 0; i < 3; i++) {\n        const int r = i * 4;"),
    ("translate after: product rounded first", F, "        m[r + 2] = (float)((double)m[r + 3] * by->z + m[r + 2]);", "        m[r + 2] = m[r + 3] * by->z + m[r + 2];"),

    # The table.
    ("table: point and direction swapped", C, "AtMatrixSetProduct, AtMatrixTransformPoint, AtMatrixSetUniformScale, AtMatrixRead, AtMatrixGetPosition, AtMatrixGetAxes, AtMatrixTransformDirection,",
     "AtMatrixSetProduct, AtMatrixTransformDirection, AtMatrixSetUniformScale, AtMatrixRead, AtMatrixGetPosition, AtMatrixGetAxes, AtMatrixTransformPoint,"),
    ("table: x and z axes swapped", C, "AtMatrixGetXAxis, AtMatrixGetYAxis, AtMatrixGetZAxis,", "AtMatrixGetZAxis, AtMatrixGetYAxis, AtMatrixGetXAxis,"),
    ("table: the two inverse transforms swapped", C, "AtMatrixInverseTransformPoint, AtMatrixInverseTransformDirection,", "AtMatrixInverseTransformDirection, AtMatrixInverseTransformPoint,"),
    ("table: translation and scale swapped", C, "AtMatrixSetTranslation, AtMatrixSetRotation, AtMatrixSetLookAt, AtMatrixSetScale,", "AtMatrixSetScale, AtMatrixSetRotation, AtMatrixSetLookAt, AtMatrixSetTranslation,"),
    # Copy for CopyWords in the table would be equivalent: both move every word as it is. Not a mutant.
]

SUITE = {"target": "oracle_matrix", "subsystem": "at/matrix", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
