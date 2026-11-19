// The matrix device and its pool. See include/at/AtMatrixDevice.h.
// Verification status per function: config/functions.d/matdev.toml.
#include "at/AtMatrixDevice.h"

#include "at/AtHeap.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int file, int line, int unused);

namespace {

typedef decltype(sizeof(0)) Size;
struct Placement {};

}  // namespace

inline void* operator new(Size, void* where, Placement) { return where; }
inline void operator delete(void*, void*, Placement) {}

AtHeap* AtMatrixPoolXbox::s_heap;
void* AtMatrixDeviceXbox::s_heap;

// ---- the matrix ---------------------------------------------------------------------------------------------------------

AtMatrixObject::AtMatrixObject() {
    m[0] = 1.0f;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;
    m[4] = 0.0f;
    m[5] = 1.0f;
    m[6] = 0.0f;
    m[7] = 0.0f;
    m[8] = 0.0f;
    m[9] = 0.0f;
    m[10] = 1.0f;
    m[11] = 0.0f;
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
}

// ---- the pool -----------------------------------------------------------------------------------------------------------

AtMatrixPool::~AtMatrixPool() {
}

void AtMatrixPoolXbox::SetHeap(AtHeap* heap) {
    s_heap = heap;
}

AtMatrixPoolXbox::AtMatrixPoolXbox() {
    m_matrices = 0;
    m_count = 0;
}

AtMatrixPoolXbox::~AtMatrixPoolXbox() {
    if (m_matrices != 0) {
        operator delete(m_matrices);
    }
    m_matrices = 0;
    m_count = 0;
}

void AtMatrixPoolXbox::Slot0() {
    this->~AtMatrixPoolXbox();
    operator delete(this);
}

void AtMatrixPoolXbox::Reserve(unsigned int count) {
    if (IsReserved()) {
        Clear();
    }
    m_count = count;
    if (s_heap->m_alignShift < 6) {
        s_heap->m_allocShift = 6;
    } else {
        s_heap->m_allocShift = s_heap->m_alignShift;
    }
    AtPushHeap(s_heap);
    AtMatrixObject* block = (AtMatrixObject*)operator new(m_count << 6);
    if (block != 0) {
        // The original's loop: from count - 1 down, as a signed number.
        AtMatrixObject* each = block;
        for (int left = (int)m_count - 1; left >= 0; left--) {
            new (each, Placement()) AtMatrixObject;
            each++;
        }
    }
    m_matrices = block;
    AtPopHeap();
    if (m_matrices == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    s_heap->m_allocShift = s_heap->m_alignShift;
}

void AtMatrixPoolXbox::Clear() {
    if (m_matrices != 0) {
        operator delete(m_matrices);
    }
    m_matrices = 0;
    m_count = 0;
}

unsigned int AtMatrixPoolXbox::Count() {
    return m_count;
}

AtMatrix* AtMatrixPoolXbox::At(unsigned int index) {
    return &m_matrices[index];
}

int AtMatrixPoolXbox::IsReserved() {
    return m_matrices != 0;
}

AtMatrix* AtMatrixPoolXbox::Matrix(unsigned int index) {
    return &m_matrices[index];
}

// ---- the device ---------------------------------------------------------------------------------------------------------

AtMatrixDevice::AtMatrixDevice() {
    m_unknown04 = 0;
}

AtMatrixDeviceXbox::AtMatrixDeviceXbox() {
}

void AtMatrixDeviceXbox::SetHeap(void* heap) {
    s_heap = heap;
}

AtMatrix* AtMatrixDeviceXbox::NewMatrix() {
    return DoNewMatrix();
}

AtMatrixPool* AtMatrixDeviceXbox::NewPool() {
    return DoNewPool();
}

void AtMatrixDeviceXbox::Slot2() {
    NewVertexArray();
}

void AtMatrixDeviceXbox::Slot3() {
    NewIndexArray();
}

void AtMatrixDeviceXbox::FreeMatrix(AtMatrix* matrix) {
    operator delete(matrix);
}

void AtMatrixDeviceXbox::FreePool(AtMatrixPool* pool) {
    if (pool != 0) {
        pool->Slot0();
    }
}

void AtMatrixDeviceXbox::FreeVertexArray(AtVertexArray* array) {
    if (array != 0) {
        array->Destroy(1);
    }
}

void AtMatrixDeviceXbox::FreeIndexArray(AtIndexArray* array) {
    if (array != 0) {
        array->Destroy(1);
    }
}

AtMatrix* AtMatrixDeviceXbox::DoNewMatrix() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* block = operator new(sizeof(AtMatrixObject));
    AtMatrixObject* matrix = block != 0 ? new (block, Placement()) AtMatrixObject : 0;
    AtPopHeap();
    if (matrix == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return matrix;
}

AtMatrixPool* AtMatrixDeviceXbox::DoNewPool() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    void* block = operator new(sizeof(AtMatrixPoolXbox));
    AtMatrixPoolXbox* pool = block != 0 ? new (block, Placement()) AtMatrixPoolXbox : 0;
    AtPopHeap();
    if (pool == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return pool;
}

AtVertexArray* AtMatrixDeviceXbox::NewVertexArray() {
    AtFatal("AtMatrixDeviceXbox: slot 10, the graphics layer's vertex array (vftable 0x00288CF8), is not recovered", 0, 0, 0);
    return 0;
}

AtIndexArray* AtMatrixDeviceXbox::NewIndexArray() {
    AtFatal("AtMatrixDeviceXbox: slot 11, the graphics layer's index array (vftable 0x00289358), is not recovered", 0, 0, 0);
    return 0;
}

void AtMatrixDeviceXbox::ReleaseMatrix(AtMatrix* matrix) {
    operator delete(matrix);
}

void AtMatrixDeviceXbox::ReleasePool(AtMatrixPool* pool) {
    if (pool != 0) {
        pool->Slot0();
    }
}

void AtMatrixDeviceXbox::ReleaseVertexArray(AtVertexArray* array) {
    if (array != 0) {
        array->Destroy(1);
    }
}

void AtMatrixDeviceXbox::ReleaseIndexArray(AtIndexArray* array) {
    if (array != 0) {
        array->Destroy(1);
    }
}
