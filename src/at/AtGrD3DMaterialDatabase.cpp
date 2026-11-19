// The Xbox material database, its materials and their stage states. See include/at/AtGrD3DTextures.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);  // 0x00013F70

void* AtGrD3DMaterialDatabase::s_heap;
unsigned int AtGrD3DMaterialStage::s_sharedUsers;
AtGrD3DStageEntry* AtGrD3DMaterialStage::s_sharedEntries;
unsigned int AtGrD3DMaterialStage::s_shared7C;
unsigned int AtGrD3DMaterialStage::s_shared80;
void* AtGrD3DMaterialStage::s_shared84;
void* AtGrD3DMaterialStage::s_shared88;
unsigned int AtGrD3DMaterialStage::s_shared8C;

// ---- The owner's list ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DOwner::AddMaterialDatabase(AtGrD3DMaterialDatabase* database) {
    database->m_next = m_materialDatabases;
    m_materialDatabases = database;
}

void AtGrD3DOwner::RemoveMaterialDatabase(AtGrD3DMaterialDatabase* database) {
    AtGrD3DMaterialDatabase** link = &m_materialDatabases;
    for (AtGrD3DMaterialDatabase* at = *link; at != 0; at = *link) {
        if (at == database) {
            *link = database->m_next;
            return;
        }
        link = &at->m_next;
    }
}

// ---- Stage states -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DStageEntry::~AtGrD3DStageEntry() {
    m_unknown00 = 0;
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_unknown0E = 0;
    m_unknown0C = 0;
    m_unknown10[3] = 0;
    m_unknown10[2] = 0;
    m_unknown10[1] = 0;
    m_unknown10[0] = 0;
}

AtGrD3DMaterialStage::AtGrD3DMaterialStage() {
    m_unknown16 = 0;
    m_unknown18 = 0;
    m_unknown1A = 0;
    m_entries = 0;
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_flags = 0;
    m_unknown1C = 0x8006;
    m_unknown1E = 1;
    m_unknown20 = 0;
    m_unknown22 = 0x8006;
    m_unknown24 = 1;
    m_unknown26 = 0;
    m_unknown0C = 0;
    m_unknown10 = 0;
}

AtGrD3DMaterialStage::~AtGrD3DMaterialStage() {
    Reset();
}

void AtGrD3DMaterialStage::Reset() {
    if (m_unknown04 != 0) {
        operator delete(m_unknown04);
    }
    if (m_unknown08 != 0) {
        operator delete(m_unknown08);
    }
    if (m_entries != 0) {
        delete[] m_entries;
    }
    if (m_flags & kShared) {
        if (--s_sharedUsers == 0) {
            FreeShared();
        }
    }
    m_unknown16 = 0;
    m_unknown18 = 0;
    m_unknown1A = 0;
    m_entries = 0;
    m_unknown04 = 0;
    m_unknown08 = 0;
    m_flags = 0;
    m_unknown20 = 0;
    m_unknown26 = 0;
    m_unknown0C = 0;
    m_unknown10 = 0;
    m_unknown1E = 1;
    m_unknown24 = 1;
    m_unknown1C = 0x8006;
    m_unknown22 = 0x8006;
}

void AtGrD3DMaterialStage::FreeShared() {
    if (s_shared88 != 0) {
        operator delete(s_shared88);
    }
    if (s_shared84 != 0) {
        operator delete(s_shared84);
    }
    if (s_sharedEntries != 0) {
        delete[] s_sharedEntries;
    }
    s_sharedEntries = 0;
    s_shared7C = 0;
    s_shared80 = 0;
    s_shared84 = 0;
    s_shared8C = 0;
    s_shared88 = 0;
}

// ---- Materials ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DMaterial::AtGrD3DMaterial() {
    m_stageStateCount = 0;
    m_unknown188 = 0;
}

AtGrD3DMaterial::~AtGrD3DMaterial() {
}

void AtGrD3DMaterial::OnLoaded(AtMaterialOwner* owner) {
    AtGrD3DLoadMaterial(this, owner);
}

void AtGrD3DMaterial::ReleaseStages(AtMaterialOwner*) {
    for (unsigned int i = 0; i < m_stageStateCount; i++) {
        m_stageStates[i].Reset();
    }
    m_stageStateCount = 0;
}

void AtGrD3DMaterial::Release(AtMaterialOwner* owner) {
    ReleaseStages(owner);
    AtMaterial::Reset();
}

// ---- The database -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DMaterialDatabase::SetHeap(void* heap) {
    s_heap = heap;
}

AtGrD3DMaterialDatabase::AtGrD3DMaterialDatabase() {
    m_materials = 0;
    m_owner = 0;
    m_next = 0;
}

AtGrD3DMaterialDatabase::~AtGrD3DMaterialDatabase() {
    AtGrD3DMaterialDatabase::Clear();
}

void AtGrD3DMaterialDatabase::Clear() {
    if (m_owner != 0) {
        m_owner->RemoveMaterialDatabase(this);
        for (unsigned int i = 0; i < m_count; i++) {
            m_materials[i].Release(m_owner);
        }
        m_owner = 0;
    }
    if (m_materials != 0) {
        delete[] m_materials;
        m_materials = 0;
    }
    AtMaterialDatabase::Clear();
}

void AtGrD3DMaterialDatabase::OnReady(AtMaterialOwner* owner) {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_materials = new AtGrD3DMaterial[m_count];
    AtPopHeap();
    if (m_materials == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    m_owner->AddMaterialDatabase(this);
}

void AtGrD3DMaterialDatabase::Slot3() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_more = new AtGrD3DMaterialDatabase;
    AtPopHeap();
    if (m_more == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    // Eight bytes, unterminated when the name is eight long, as AtCopyName8 copies it; a missing group leaves only the first byte set.
    char ns[8];
    if (m_group == 0) {
        ns[0] = 0;
    } else {
        AtCopyName8(ns, m_group->m_name);
    }
    m_more->Allocate(m_owner, m_stageCapacity, m_textureCapacity, m_count, ns);
}

AtMaterial* AtGrD3DMaterialDatabase::MaterialAt(unsigned int index) {
    if (index < m_count) {
        return &m_materials[index];
    }
    return m_more->MaterialAt(index - m_count);
}
