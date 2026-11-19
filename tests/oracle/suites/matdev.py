# Mutants of the matrix device, its pool, and the last five matrix-table slots. Each text occurs exactly once in its file.
D = "AtMatrixDevice.cpp"
X = "AtMatrixAxis.cpp"
C = "AtCoordSys.cpp"

_CLEAR = "void AtMatrixPoolXbox::Clear() {\n    if (m_matrices != 0) {\n        operator delete(m_matrices);\n    }\n    m_matrices = 0;\n    m_count = 0;\n"
_DTOR = "AtMatrixPoolXbox::~AtMatrixPoolXbox() {\n    if (m_matrices != 0) {\n        operator delete(m_matrices);\n    }\n    m_matrices = 0;\n    m_count = 0;\n"
_NEW_MATRIX = "AtMatrix* AtMatrixDeviceXbox::DoNewMatrix() {\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n"
_NEW_POOL = "AtMatrixPool* AtMatrixDeviceXbox::DoNewPool() {\n    if (s_heap == 0) {\n        AtRaise(-2, 0, 0, 0);\n    }\n"
_PRE_Z = ("void AtMatrixPreRotateZ(AtMatrix* m, float angle) {\n    float r[12];\n    r[2] = 0.0f;\n    r[6] = 0.0f;\n    r[8] = 0.0f;\n    r[9] = 0.0f;\n    r[10] = 1.0f;\n"
          "    const double s = AtSin(angle);\n    const double c = AtCos(angle);\n    r[0] = (float)c;\n    r[1] = (float)-s;\n    r[4] = (float)s;\n    r[5] = (float)c;\n")
_POST_Z_TAIL = "    r[0] = (float)c;\n    r[1] = (float)-s;\n    r[4] = (float)s;\n    r[5] = (float)c;\n    AtMatrixPostMultiply3x3(m, r);\n"


def _item(name, kind):
    return "void AtMatrixDeviceXbox::%s(AtMatrixDeviceItem%s* item) {\n    if (item != 0) {\n        item->Destroy(1);\n" % (name, kind)


MUTANTS = [
    # The matrix every maker constructs.
    ("matrix: 5 is 0", D, "    m[5] = 1.0f;\n", "    m[5] = 0.0f;\n"),
    ("matrix: 15 left", D, "    m[15] = 1.0f;\n", ""),
    ("matrix: 3 left", D, "    m[3] = 0.0f;\n", ""),

    # The pool.
    ("pool: count not cleared by the constructor", D, "AtMatrixPoolXbox::AtMatrixPoolXbox() {\n    m_matrices = 0;\n    m_count = 0;\n", "AtMatrixPoolXbox::AtMatrixPoolXbox() {\n    m_matrices = 0;\n"),
    ("pool: block not cleared by the constructor", D, "AtMatrixPoolXbox::AtMatrixPoolXbox() {\n    m_matrices = 0;\n", "AtMatrixPoolXbox::AtMatrixPoolXbox() {\n"),
    ("destructor: block not freed", D, _DTOR, _DTOR.replace("        operator delete(m_matrices);\n", "")),
    ("destructor: count kept", D, _DTOR, _DTOR.replace("    m_count = 0;\n", "")),
    ("deleting destructor: itself not freed", D, "    this->~AtMatrixPoolXbox();\n    operator delete(this);\n", "    this->~AtMatrixPoolXbox();\n"),
    ("deleting destructor: block not freed", D, "    this->~AtMatrixPoolXbox();\n    operator delete(this);\n", "    operator delete(this);\n"),
    ("reserve: no clear first", D, "    if (IsReserved()) {\n        Clear();\n    }\n", ""),
    ("reserve: shift 5", D, "    if (s_heap->m_alignShift < 6) {\n        s_heap->m_allocShift = 6;", "    if (s_heap->m_alignShift < 6) {\n        s_heap->m_allocShift = 5;"),
    ("reserve: below 5", D, "    if (s_heap->m_alignShift < 6) {\n", "    if (s_heap->m_alignShift < 5) {\n"),
    ("reserve: larger alignment not taken", D, "    } else {\n        s_heap->m_allocShift = s_heap->m_alignShift;\n    }\n", "    }\n"),
    ("reserve: no push", D, "    AtPushHeap(s_heap);\n    AtMatrixObject* block", "    AtMatrixObject* block"),
    ("reserve: no pop", D, "    m_matrices = block;\n    AtPopHeap();\n", "    m_matrices = block;\n"),
    ("reserve: 32 bytes each", D, "operator new(m_count << 6)", "operator new(m_count << 5)"),
    ("reserve: counted unsigned", D, "for (int left = (int)m_count - 1; left >= 0; left--)", "for (unsigned int left = m_count; left > 0; left--)"),
    ("reserve: one short", D, "(int)m_count - 1; left >= 0", "(int)m_count - 1; left > 0"),
    ("reserve: no report", D, "    if (m_matrices == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n    s_heap->m_allocShift", "    s_heap->m_allocShift"),
    ("reserve: shift not set back", D, "    s_heap->m_allocShift = s_heap->m_alignShift;\n}\n\nvoid AtMatrixPoolXbox::Clear", "}\n\nvoid AtMatrixPoolXbox::Clear"),
    ("reserve: count dropped on failure", D, "    m_matrices = block;\n", "    m_matrices = block;\n    if (block == 0) {\n        m_count = 0;\n    }\n"),
    ("reserve: count kept from before", D, "    m_count = count;\n", ""),
    ("clear: count kept", D, _CLEAR, _CLEAR.replace("    m_count = 0;\n", "")),
    ("clear: block not freed", D, _CLEAR, _CLEAR.replace("        operator delete(m_matrices);\n", "")),
    ("count: one more", D, "    return m_count;\n", "    return m_count + 1;\n"),
    ("at: next one", D, "AtMatrix* AtMatrixPoolXbox::At(unsigned int index) {\n    return &m_matrices[index];", "AtMatrix* AtMatrixPoolXbox::At(unsigned int index) {\n    return &m_matrices[index + 1];"),
    ("matrix: next one", D, "AtMatrix* AtMatrixPoolXbox::Matrix(unsigned int index) {\n    return &m_matrices[index];", "AtMatrix* AtMatrixPoolXbox::Matrix(unsigned int index) {\n    return &m_matrices[index + 1];"),
    ("is reserved: inverted", D, "    return m_matrices != 0;\n", "    return m_matrices == 0;\n"),
    ("pool heap: not stored", D, "void AtMatrixPoolXbox::SetHeap(AtHeap* heap) {\n    s_heap = heap;\n", "void AtMatrixPoolXbox::SetHeap(AtHeap* heap) {\n"),

    # The device.
    ("base: member not cleared", D, "    m_unknown04 = 0;\n", "    m_unknown04 = 1;\n"),
    ("device heap: not stored", D, "void AtMatrixDeviceXbox::SetHeap(void* heap) {\n    s_heap = heap;\n", "void AtMatrixDeviceXbox::SetHeap(void* heap) {\n"),
    ("slot 2: to slot 11", D, "void AtMatrixDeviceXbox::Slot2() {\n    NewItemA();", "void AtMatrixDeviceXbox::Slot2() {\n    NewItemB();"),
    ("slot 3: to slot 10", D, "void AtMatrixDeviceXbox::Slot3() {\n    NewItemB();", "void AtMatrixDeviceXbox::Slot3() {\n    NewItemA();"),
    ("free matrix: not freed", D, "void AtMatrixDeviceXbox::FreeMatrix(AtMatrix* matrix) {\n    operator delete(matrix);\n", "void AtMatrixDeviceXbox::FreeMatrix(AtMatrix* matrix) {\n"),
    ("release matrix: not freed", D, "void AtMatrixDeviceXbox::ReleaseMatrix(AtMatrix* matrix) {\n    operator delete(matrix);\n", "void AtMatrixDeviceXbox::ReleaseMatrix(AtMatrix* matrix) {\n"),
    ("free pool: cleared, not deleted", D, "void AtMatrixDeviceXbox::FreePool(AtMatrixPool* pool) {\n    if (pool != 0) {\n        pool->Slot0();", "void AtMatrixDeviceXbox::FreePool(AtMatrixPool* pool) {\n    if (pool != 0) {\n        pool->Clear();"),
    ("release pool: cleared, not deleted", D, "void AtMatrixDeviceXbox::ReleasePool(AtMatrixPool* pool) {\n    if (pool != 0) {\n        pool->Slot0();", "void AtMatrixDeviceXbox::ReleasePool(AtMatrixPool* pool) {\n    if (pool != 0) {\n        pool->Clear();"),
    ("free item A: flag 0", D, _item("FreeItemA", "A"), _item("FreeItemA", "A").replace("Destroy(1)", "Destroy(0)")),
    ("free item B: flag 0", D, _item("FreeItemB", "B"), _item("FreeItemB", "B").replace("Destroy(1)", "Destroy(0)")),
    ("release item A: flag 0", D, _item("ReleaseItemA", "A"), _item("ReleaseItemA", "A").replace("Destroy(1)", "Destroy(0)")),
    ("release item B: flag 0", D, _item("ReleaseItemB", "B"), _item("ReleaseItemB", "B").replace("Destroy(1)", "Destroy(0)")),
    ("new matrix: null heap not reported", D, _NEW_MATRIX, "AtMatrix* AtMatrixDeviceXbox::DoNewMatrix() {\n"),
    ("new matrix: not constructed", D, "AtMatrixObject* matrix = block != 0 ? new (block, Placement()) AtMatrixObject : 0;", "AtMatrixObject* matrix = (AtMatrixObject*)block;"),
    ("new matrix: failure not reported", D, "    if (matrix == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("new matrix: 48 bytes", D, "    void* block = operator new(sizeof(AtMatrixObject));\n", "    void* block = operator new(48);\n"),
    ("new pool: null heap not reported", D, _NEW_POOL, "AtMatrixPool* AtMatrixDeviceXbox::DoNewPool() {\n"),
    ("new pool: no pop", D, "    AtPopHeap();\n    if (pool == 0) {\n", "    if (pool == 0) {\n"),
    ("new pool: failure not reported", D, "    if (pool == 0) {\n        AtRaise(-4, 0, 0, 0);\n    }\n", ""),
    ("new pool: heap not pushed", D, "    AtPushHeap(s_heap);\n    void* block = operator new(sizeof(AtMatrixPoolXbox));\n", "    void* block = operator new(sizeof(AtMatrixPoolXbox));\n"),

    # The products.
    ("pre: row 0's order", X, "    m[0] = (float)((double)m8 * r[2] + (double)m0 * r[0] + (double)m4 * r[1]);\n", "    m[0] = (float)((double)m0 * r[0] + (double)m4 * r[1] + (double)m8 * r[2]);\n"),
    ("pre: row 1's order", X, "    m[5] = (float)((double)m9 * r[6] + (double)m1 * r[4] + (double)m5 * r[5]);\n", "    m[5] = (float)((double)m1 * r[4] + (double)m5 * r[5] + (double)m9 * r[6]);\n"),
    ("pre: row 2's order", X, "    m[10] = (float)((double)m2 * r[8] + (double)m6 * r[9] + (double)m10 * r[10]);\n", "    m[10] = (float)((double)m10 * r[10] + (double)m2 * r[8] + (double)m6 * r[9]);\n"),
    ("pre: wrong element of r", X, "    m[6] = (float)((double)m10 * r[6] + (double)m2 * r[4] + (double)m6 * r[5]);\n", "    m[6] = (float)((double)m10 * r[6] + (double)m2 * r[4] + (double)m6 * r[4]);\n"),
    ("pre: fourth column left", X, "    m[7] = (float)((double)m11 * r[6] + (double)m3 * r[4] + (double)m7 * r[5]);\n", ""),
    ("pre: row 0 rounded to float before the third", X, "    m[1] = (float)((double)m9 * r[2] + (double)m1 * r[0] + (double)m5 * r[1]);\n", "    m[1] = (float)((double)(float)((double)m9 * r[2] + (double)m1 * r[0]) + (double)m5 * r[1]);\n"),
    ("pre: an element read after it was written", X, "    m[4] = (float)((double)m8 * r[6] + (double)m0 * r[4] + (double)m4 * r[5]);\n", "    m[4] = (float)((double)m8 * r[6] + (double)m[0] * r[4] + (double)m4 * r[5]);\n"),
    ("pre: row 2's fourth column", X, "    m[11] = (float)((double)m3 * r[8] + (double)m7 * r[9] + (double)m11 * r[10]);\n", "    m[11] = (float)((double)m3 * r[8] + (double)m7 * r[9] + (double)m11 * r[11]);\n"),
    ("post: column 0's order", X, "        m[row] = (float)((double)c * r[8] + (double)a * r[0] + (double)b * r[4]);\n", "        m[row] = (float)((double)a * r[0] + (double)b * r[4] + (double)c * r[8]);\n"),
    ("post: column 1's order", X, "        m[row + 1] = (float)((double)c * r[9] + (double)a * r[1] + (double)b * r[5]);\n", "        m[row + 1] = (float)((double)c * r[9] + ((double)a * r[1] + (double)b * r[5]));\n"),
    ("post: column 2's order", X, "        m[row + 2] = (float)((double)a * r[2] + (double)b * r[6] + (double)c * r[10]);\n", "        m[row + 2] = (float)((double)c * r[10] + (double)a * r[2] + (double)b * r[6]);\n"),
    ("post: position row left", X, "    for (int row = 0; row < 16; row += 4) {\n", "    for (int row = 0; row < 12; row += 4) {\n"),
    ("post: an element read after it was written", X, "+ (double)a * r[1] + (double)b * r[5]", "+ (double)m[row] * r[1] + (double)b * r[5]"),
    ("post: wrong element of r", X, "(double)a * r[2] + (double)b * r[6]", "(double)a * r[2] + (double)b * r[2]"),

    # The rotations.
    ("x: 0 not 1", X, "    r[0] = 1.0f;\n    r[1] = 0.0f;\n", "    r[0] = 0.5f;\n    r[1] = 0.0f;\n"),
    ("x: sine for cosine", X, "    r[5] = (float)c;\n    r[6] = (float)-s;\n", "    r[5] = (float)s;\n    r[6] = (float)-s;\n"),
    ("x: signs", X, "    r[6] = (float)-s;\n    r[9] = (float)s;\n", "    r[6] = (float)s;\n    r[9] = (float)-s;\n"),
    ("x: 8 not 0", X, "    r[4] = 0.0f;\n    r[8] = 0.0f;\n    const double s", "    r[4] = 0.0f;\n    r[8] = 1.0f;\n    const double s"),
    ("y: signs", X, "    r[2] = (float)s;\n    r[8] = (float)-s;\n", "    r[2] = (float)-s;\n    r[8] = (float)s;\n"),
    ("y: 5 not 1", X, "    r[5] = 1.0f;\n", "    r[5] = 0.0f;\n"),
    ("y: post multiplied", X, "    r[10] = (float)c;\n    AtMatrixPreMultiply3x3(m, r);\n}\n\nvoid AtMatrixPreRotateZ", "    r[10] = (float)c;\n    AtMatrixPostMultiply3x3(m, r);\n}\n\nvoid AtMatrixPreRotateZ"),
    ("pre z: signs", X, _PRE_Z, _PRE_Z.replace("    r[1] = (float)-s;\n    r[4] = (float)s;\n", "    r[1] = (float)s;\n    r[4] = (float)-s;\n")),
    ("pre z: 10 not 1", X, _PRE_Z, _PRE_Z.replace("    r[10] = 1.0f;\n", "    r[10] = 0.0f;\n")),
    ("pre z: cosine at 5 from the sine", X, _PRE_Z, _PRE_Z.replace("    r[5] = (float)c;\n", "    r[5] = (float)s;\n")),
    ("pre z: cosine through a float", X, _PRE_Z, _PRE_Z.replace("    const double c = AtCos(angle);\n", "    const double c = (float)AtCos(angle) * 1.0000001f;\n")),
    ("post z: pre multiplied", X, _POST_Z_TAIL, _POST_Z_TAIL.replace("AtMatrixPostMultiply3x3", "AtMatrixPreMultiply3x3")),
    ("post z: signs", X, _POST_Z_TAIL, _POST_Z_TAIL.replace("    r[1] = (float)-s;\n    r[4] = (float)s;\n", "    r[1] = (float)s;\n    r[4] = (float)-s;\n")),
    ("table: x and y swapped", C, "AtMatrixPreRotateX, AtMatrixPreRotateY,", "AtMatrixPreRotateY, AtMatrixPreRotateX,"),
    ("table: get rotation missing", C, "AtMatrixPostRotateZ, AtMatrixGetRotation};", "AtMatrixPostRotateZ, 0};"),

    # The matrix to quaternion.
    ("rotation: trace of 0 taken", X, "    if (trace > 0.0) {\n", "    if (trace >= 0.0) {\n"),
    ("rotation: trace summed in another order", X, "    const double trace = yz + m[0];\n", "    const double trace = ((double)m[0] + m[5]) + m[10];\n"),
    ("rotation: x branch from the register", X, "sqrt((double)m[0] - yzStored + 1.0)", "sqrt((double)m[0] - yz + 1.0)"),
    ("rotation: y branch summed in another order", X, "sqrt((double)m[5] - ((double)m[10] + m[0]) + 1.0)", "sqrt((double)m[5] - (double)m[10] - m[0] + 1.0)"),
    ("rotation: z branch summed in another order", X, "sqrt((double)m[10] - ((double)m[5] + m[0]) + 1.0)", "sqrt((double)m[10] - (double)m[5] - m[0] + 1.0)"),
    ("rotation: tie of y taken", X, "    if (m[5] > m[0]) {\n", "    if (m[5] >= m[0]) {\n"),
    ("rotation: tie of z taken", X, "    if (m[10] > m[axis * 5]) {\n", "    if (m[10] >= m[axis * 5]) {\n"),
    ("rotation: z against x always", X, "    if (m[10] > m[axis * 5]) {\n", "    if (m[10] > m[0]) {\n"),
    ("rotation: x's sign", X, "        q->x = (float)(((double)m[9] - m[6]) * k);\n", "        q->x = (float)(((double)m[6] - m[9]) * k);\n"),
    ("rotation: k stored", X, "        const double k = 0.5 / root;\n        q->x = (float)(((double)m[9] - m[6]) * k);\n", "        const float k = (float)(0.5 / root);\n        q->x = (float)(((double)m[9] - m[6]) * k);\n"),
    ("rotation: y and z written in the other order", X, "        q->y = (float)(((double)m[2] - m[8]) * k);\n        q->z = (float)(((double)m[4] - m[1]) * k);\n",
     "        q->z = (float)(((double)m[4] - m[1]) * k);\n        q->y = (float)(((double)m[2] - m[8]) * k);\n"),
    ("rotation: half not taken", X, "        q->w = (float)(0.5 * root);             // 0x0027463C 0.5f\n", "        q->w = (float)root;\n"),
    ("rotation x branch: w's sign", X, "        q->w = (float)(((double)m[9] - m[6]) * k);\n", "        q->w = (float)(((double)m[6] - m[9]) * k);\n"),
    ("rotation x branch: z from 8 and 2 differenced", X, "        q->z = (float)(((double)m[8] + m[2]) * k);\n", "        q->z = (float)(((double)m[8] - m[2]) * k);\n"),
    ("rotation y branch: z's sum", X, "        q->z = (float)(((double)m[9] + m[6]) * k);\n", "        q->z = (float)(((double)m[9] - m[6]) * k);\n"),
    ("rotation y branch: w", X, "        q->w = (float)(((double)m[2] - m[8]) * k);\n", "        q->w = (float)(((double)m[8] - m[2]) * k);\n"),
    ("rotation z branch: w's sign", X, "        q->w = (float)(((double)m[4] - m[1]) * k);\n", "        q->w = (float)(((double)m[4] + m[1]) * k);\n"),
    ("rotation z branch: y", X, "        q->y = (float)(((double)m[9] + m[6]) * k);\n", "        q->y = (float)(((double)m[9] + m[5]) * k);\n"),
]

SUITE = {"target": "oracle_matdev", "subsystem": "at/matdev", "src": "at", "var": "KND_AT_SRC", "cases": "tok", "mutants": MUTANTS}
