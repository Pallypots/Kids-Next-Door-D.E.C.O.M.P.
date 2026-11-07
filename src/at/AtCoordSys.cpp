// See include/at/AtCoordSys.h.
#include "at/AtCoordSys.h"

#include "at/AtIoTokenizer.h"

#include "AtSceneArray.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtFatal(const char* message, int a, int b, int c);

void* AtCoordSysDatabase::s_heap;

namespace {

void NoCopy(AtMatrix*, const AtMatrix*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoIdentity(AtMatrix*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoTranslation(AtMatrix*, const AtVector3*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoRotation(AtMatrix*, const AtQuaternion*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoLookAt(AtMatrix*, const AtVector3*, const AtVector3*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoScale(AtMatrix*, const AtVector3*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoInverse(AtMatrix*, const AtMatrix*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoMultiply(AtMatrix*, const AtMatrix*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoProduct(AtMatrix*, const AtMatrix*, const AtMatrix*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoTransform(const AtMatrix*, const AtVector3*, AtVector3*) { AtFatal("Atlas: no matrix table", 0, 0, 0); }
void NoUniformScale(AtMatrix*, float) { AtFatal("Atlas: no matrix table", 0, 0, 0); }

}  // namespace

AtMatrixTable g_atMatrix = {NoCopy, NoIdentity, NoTranslation, NoRotation, NoLookAt, NoScale, NoInverse, NoMultiply, NoProduct, NoTransform, NoUniformScale, AtMatrixRead};

void AtQuaternion::Read(AtIoTokenizer* tokenizer) {
    x = tokenizer->ExpectReal();
    y = tokenizer->ExpectReal();
    z = tokenizer->ExpectReal();
    w = tokenizer->ExpectReal();
}

// ---- AtCoordSys ---------------------------------------------------------------------------------------------------------

AtCoordSys::AtCoordSys() {
    m_parent = 0;
    m_nextSibling = 0;
    m_firstChild = 0;
    m_drawingLocal = 0;
    m_drawingWorld = 0;
    m_local = 0;
    m_world = 0;
    m_dirty = 0;
    m_unknown20 = -1;
}

AtCoordSys::~AtCoordSys() {
    AtCoordSys::Reset();
}

float AtCoordSys::ScaleFactor() {
    return 1.0f;
}

void AtCoordSys::Invalidate() {
    m_dirty &= ~(unsigned int)kScaled;
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->Invalidate();
    }
}

void AtCoordSys::Reset() {
    if (m_parent != 0) {
        m_parent->Detach(this);
    }
    AtCoordSys* child = m_firstChild;
    while (child != 0) {
        AtCoordSys* next = child->m_nextSibling;
        Detach(child);
        child = next;
    }
    m_parent = 0;
    m_nextSibling = 0;
    m_firstChild = 0;
    m_drawingLocal = 0;
    m_drawingWorld = 0;
    m_local = 0;
    m_world = 0;
    m_dirty = 0;
    m_unknown20 = -1;
}

void AtCoordSys::Attach(AtCoordSys* child) {
    if (child->m_parent == this) {
        return;
    }
    if (child->m_parent != 0) {
        child->m_parent->Detach(child);
    }
    child->m_parent = this;
    child->m_nextSibling = m_firstChild;
    m_firstChild = child;
    child->m_dirty = kAllLocal;
}

void AtCoordSys::Detach(AtCoordSys* child) {
    AtCoordSys** link = &m_firstChild;
    while (*link != 0) {
        if (*link == child) {
            *link = child->m_nextSibling;
            child->m_nextSibling = 0;
            child->m_parent = 0;
            child->m_dirty = kAllLocal;
            return;
        }
        link = &(*link)->m_nextSibling;
    }
}

int AtCoordSys::UpdateInverse() {
    if (m_parent != 0) {
        m_parent->UpdateInverse();
    }
    if ((m_dirty & kInverse) == 0) {
        return 0;
    }
    g_atMatrix.SetInverse(m_world, m_local);
    if (m_parent != 0) {
        g_atMatrix.Multiply(m_world, m_parent->m_world);
    }
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->m_dirty |= kInverse;
    }
    m_dirty = (m_dirty & ~(unsigned int)kInverse) | 0x0B;
    return kInverse;
}

void AtCoordSys::Visit(void* with) {
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->Visit(with);
    }
}

void AtCoordSys::MarkDirty(unsigned int mask) {
    if ((m_dirty & mask) != 0) {
        return;
    }
    m_dirty |= mask;
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->MarkDirty(mask);
    }
}

void AtCoordSys::SetMatrices(AtMatrix* local, AtMatrix* world) {
    if (m_local != 0) {
        Reset();
    }
    m_local = local;
    g_atMatrix.Identity(local);
    m_world = world;
    g_atMatrix.Identity(world);
    m_dirty = kAllLocal;
}

int AtCoordSys::UpdateWorld() {
    if (m_parent != 0) {
        m_parent->UpdateWorld();
    }
    if ((m_dirty & kWorld) == 0) {
        return 0;
    }
    if (m_parent != 0) {
        g_atMatrix.SetProduct(m_world, m_local, m_parent->m_world);
    } else {
        g_atMatrix.Copy(m_world, m_local);
    }
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->m_dirty |= kWorld;
    }
    m_dirty = (m_dirty & ~(unsigned int)kWorld) | 0x2A;
    return kWorld;
}

void AtCoordSys::Transform(const AtVector3* from, AtVector3* to) {
    UpdateWorld();
    g_atMatrix.TransformPoint(m_world, from, to);
}

void AtCoordSys::CopyLocal(const AtCoordSys* from) {
    g_atMatrix.Copy(m_local, from->m_local);
    m_dirty |= kAllLocal;
}

void AtCoordSys::SetRotation(const AtQuaternion* rotation) {
    g_atMatrix.SetRotation(m_local, rotation);
    m_dirty |= kAllLocal;
}

void AtCoordSys::SetPosition(const AtVector3* position) {
    g_atMatrix.SetTranslation(m_local, position);
    m_dirty |= kAllLocal;
}

void AtCoordSys::SetLookAt(const AtVector3* direction, const AtVector3* up) {
    g_atMatrix.SetLookAt(m_local, direction, up);
    m_dirty |= kAllLocal;
}

// ---- the two kinds ------------------------------------------------------------------------------------------------------

AtRotTransCoordSys::AtRotTransCoordSys() {}

AtRotTransCoordSys::~AtRotTransCoordSys() {}

AtScaleCoordSys::AtScaleCoordSys() {
    m_scale.x = 1.0f;
    m_scale.y = 1.0f;
    m_scale.z = 1.0f;
}

AtScaleCoordSys::~AtScaleCoordSys() {}

void AtScaleCoordSys::Invalidate() {}

void AtScaleCoordSys::Reset() {
    m_scale.x = 1.0f;
    m_scale.y = 1.0f;
    m_scale.z = 1.0f;
    AtCoordSys::Reset();
}

void AtScaleCoordSys::Attach(AtCoordSys* child) {
    AtCoordSys::Attach(child);
    MarkDirty(kScaled);
}

void AtScaleCoordSys::Detach(AtCoordSys* child) {
    AtCoordSys::Detach(child);
    child->Invalidate();
}

void AtScaleCoordSys::SetScale(const AtVector3* scale) {
    m_scale = *scale;
    g_atMatrix.SetScale(m_local, scale);
    m_dirty |= kAllLocal;
}

int AtScaleCoordSys::UpdateInverse() {
    if (m_parent != 0) {
        m_parent->UpdateInverse();
    }
    if ((m_dirty & kInverse) == 0) {
        return 0;
    }
    AtVector3 inverse;
    inverse.x = 1.0f / m_scale.x;
    inverse.y = 1.0f / m_scale.y;
    inverse.z = 1.0f / m_scale.z;
    g_atMatrix.SetScale(m_world, &inverse);
    if (m_parent != 0) {
        g_atMatrix.Multiply(m_world, m_parent->m_world);
    }
    for (AtCoordSys* child = m_firstChild; child != 0; child = child->m_nextSibling) {
        child->m_dirty |= kInverse;
    }
    m_dirty = (m_dirty & ~(unsigned int)kInverse) | 0x0B;
    return kInverse;
}

// ---- the database -------------------------------------------------------------------------------------------------------

void AtCoordSysDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtCoordSysDatabase::AtCoordSysDatabase() {
    m_device = 0;
    m_pool = 0;
    m_rotTransCount = 0;
    m_scaledCount = 0;
    m_rotTrans = 0;
    m_scaled = 0;
    m_root = 0;
    m_extra = 0;
    m_nextMatrix = 0;
}

AtCoordSysDatabase::~AtCoordSysDatabase() {
    AtCoordSysDatabase::Clear();
}

void AtCoordSysDatabase::Clear() {
    GmNameMap::Clear();
    if (m_device != 0 && m_pool != 0) {
        m_pool->Clear();
        m_device->FreePool(m_pool);
    }
    if (m_scaled != 0) {
        AtDeleteArray(m_scaled);
    }
    if (m_rotTrans != 0) {
        AtDeleteArray(m_rotTrans);
    }
    m_device = 0;
    m_pool = 0;
    m_rotTransCount = 0;
    m_scaledCount = 0;
    m_rotTrans = 0;
    m_scaled = 0;
    m_root = 0;
    m_extra = 0;
    m_nextMatrix = 0;
}

void AtCoordSysDatabase::Allocate(AtMatrixDevice* device, unsigned int rotTrans, unsigned int scaled, unsigned int extra) {
    if (m_entries != 0) {
        Clear();
    }
    m_rotTransCount = rotTrans;
    m_scaledCount = scaled;
    m_device = device;
    Init(rotTrans + scaled);
    m_pool = device->NewPool();
    m_extra = extra;
    m_pool->Reserve((m_rotTransCount + m_scaledCount + extra) * 2);
    m_nextMatrix = 0;
    if (m_rotTransCount > 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_rotTrans = AtNewArray<AtRotTransCoordSys>(m_rotTransCount);
        AtPopHeap();
        if (m_rotTrans == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < m_rotTransCount; i++) {
            AtMatrix* world = m_pool->At(m_nextMatrix + 1);
            AtMatrix* local = m_pool->At(m_nextMatrix);
            m_rotTrans[i].SetMatrices(local, world);
            m_nextMatrix += 2;
        }
    }
    if (m_scaledCount > 0) {
        if (s_heap == 0) {
            AtRaise(-2, 0, 0, 0);
        }
        AtPushHeap(s_heap);
        m_scaled = AtNewArray<AtScaleCoordSys>(m_scaledCount);
        AtPopHeap();
        if (m_scaled == 0) {
            AtRaise(-4, 0, 0, 0);
        }
        for (unsigned int i = 0; i < m_scaledCount; i++) {
            AtMatrix* world = m_pool->At(m_nextMatrix + 1);
            AtMatrix* local = m_pool->At(m_nextMatrix);
            m_scaled[i].SetMatrices(local, world);
            m_nextMatrix += 2;
        }
    }
}
