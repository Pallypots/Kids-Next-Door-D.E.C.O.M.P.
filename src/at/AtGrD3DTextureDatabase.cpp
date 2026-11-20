// The Xbox texture database and what it holds: 2D textures, their surfaces, and the surfaces' palettes. See include/at/AtGrD3DTextures.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();
void AtCopyName8(char* to, const char* from);  // 0x00013F70

namespace {

// Each kind's array is made the same way, from the base's heap, by its count. The array is stored before the heap is popped.
template <class T> void MakeArray(T** array, unsigned int count) {
    if (AtTextureDatabase::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtTextureDatabase::s_heap);
    *array = new T[count];
    AtPopHeap();
    if (*array == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

}  // namespace

// ---- The owner's list ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DOwner::AddTextureDatabase(AtGrD3DTextureDatabase* database) {
    database->m_next = m_textureDatabases;
    m_textureDatabases = database;
}

void AtGrD3DOwner::RemoveTextureDatabase(AtGrD3DTextureDatabase* database) {
    AtGrD3DTextureDatabase** link = &m_textureDatabases;
    for (AtGrD3DTextureDatabase* at = *link; at != 0; at = *link) {
        if (at == database) {
            *link = database->m_next;
            return;
        }
        link = &at->m_next;
    }
}

// ---- Palettes -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrIPalette::AtGrIPalette() {
    m_count = 0;
    m_unknown06 = 0;
}

AtGrIPalette::~AtGrIPalette() {
}

AtGrD3DPalette::AtGrD3DPalette() {
    m_resource = 0;
    m_entries = 0;
    m_count = 0;
    m_unknown06 = 0;
}

AtGrD3DPalette::~AtGrD3DPalette() {
    Free();
}

void AtGrD3DPalette::Free() {
    if (m_entries != 0) {
        operator delete(m_entries);
    }
    if (m_resource != 0) {
        AtGrD3DRelease(m_resource);
    }
    m_resource = 0;
    m_entries = 0;
    m_count = 0;
    m_unknown06 = 0;
}

// ---- Surfaces -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrIPixelSurface::AtGrIPixelSurface() {
    m_flags = 0;
    m_colorKey[0] = 0;
    m_colorKey[1] = 0;
    m_colorKey[2] = 0;
    m_unknown20 = 0;
    m_unknown22 = 0;
    m_colorKey[3] = 0xFF;
}

AtGrIPixelSurface::~AtGrIPixelSurface() {
    m_format.Clear();
    m_flags = 0;
    m_colorKey[0] = 0;
    m_colorKey[1] = 0;
    m_colorKey[2] = 0;
    m_colorKey[3] = 0xFF;
    m_unknown20 = 0;
    m_unknown22 = 0;
}

void AtGrIPixelSurface::Reset() {
    m_format.Clear();
    m_flags = 0;
    m_colorKey[0] = 0;
    m_colorKey[1] = 0;
    m_colorKey[2] = 0;
    m_colorKey[3] = 0xFF;
    m_unknown20 = 0;
    m_unknown22 = 0;
}

AtGrIPalette* AtGrIPixelSurface::Palette() {
    return 0;
}

void AtGrIPixelSurface::SetColorKey(const unsigned char* rgba) {
    m_colorKey[0] = rgba[0];
    m_colorKey[1] = rgba[1];
    m_colorKey[2] = rgba[2];
    m_colorKey[3] = rgba[3];
    m_flags |= kColorKey;
}

void AtGrIPixelSurface::ClearColorKey() {
    m_flags &= ~kColorKey;
}

void AtGrIPixelSurface::Slot6() {
}

AtGrD3DSurface2D::AtGrD3DSurface2D() {
    m_surface = 0;
    m_unknown40 = 0;
    m_unknown44 = 0;
    m_unknown48 = 0;
    m_next = 0;
    m_unknown3C = 0;
}

AtGrD3DSurface2D::~AtGrD3DSurface2D() {
    AtGrD3DSurface2D::Reset();
}

void AtGrD3DSurface2D::Reset() {
    if (m_next != 0) {
        m_next->Reset();
        delete m_next;
        m_next = 0;
    }
    if (m_unknown48 != 0) {
        operator delete(m_unknown48);
        m_unknown48 = 0;
    }
    if (m_unknown44 != 0) {
        operator delete(m_unknown44);
        m_unknown44 = 0;
    }
    if (m_surface != 0) {
        AtGrD3DRelease(m_surface);
    }
    m_palette.Free();
    AtGrIPixelSurface::Reset();
    m_surface = 0;
    m_unknown40 = 0;
    m_unknown44 = 0;
    m_unknown48 = 0;
    m_next = 0;
    m_unknown3C = 0;
}

AtGrIPalette* AtGrD3DSurface2D::Palette() {
    if (m_format.m_flags & AtPixelFormat::kIndex) {
        return &m_palette;
    }
    return 0;
}

int AtGrD3DSurface2D::Slot3() {
    return 0;
}

void AtGrD3DSurface2D::ReleaseChain() {
    for (AtGrD3DSurface2D* surface = this; surface != 0; surface = surface->m_next) {
        if (surface->m_surface != 0) {
            AtGrD3DRelease(surface->m_surface);
            surface->m_surface = 0;
        }
    }
}

// ---- 2D textures --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DTexture2D::AtGrD3DTexture2D() {
    m_texture = 0;
    m_surfaces = 0;
    m_owner = 0;
}

AtGrD3DTexture2D::~AtGrD3DTexture2D() {
    Release();
    AtTextureBase::Reset();
    m_texture = 0;
    m_surfaces = 0;
    m_owner = 0;
}

void AtGrD3DTexture2D::Release() {
    if (m_owner != 0) {
        for (unsigned int stage = 0; stage < 4; stage++) {
            AtGrD3DReleaseStage(m_owner->m_stageCache, stage);
        }
    }
    if (m_surfaces != 0) {
        for (unsigned int level = 0; level < m_levels; level++) {
            m_surfaces[level].ReleaseChain();
        }
    }
    if (m_texture != 0) {
        AtGrD3DBeforeRelease(m_texture);
        AtGrD3DRelease(m_texture);
        m_texture = 0;
    }
}

void AtGrD3DTexture2D::Describe(AtTextureDatabase* database, const char* name, unsigned int usage, int autoGenMipLevels, unsigned int levels, int textureStage) {
    AtTextureBase::Describe(database, name, usage, autoGenMipLevels, levels, textureStage);
    AtGrD3DTextureDatabase* textures = static_cast<AtGrD3DTextureDatabase*>(database);
    if (m_flags & (kFrontBufferSource | kBackBufferSource)) {
        m_surfaces = textures->TakeSurfaces2D(1);
    } else {
        m_surfaces = textures->TakeSurfaces2D(m_levelsAsked);
    }
}

void AtGrD3DTexture2D::Read(AtTextureDatabase* database, const char* name, AtIoTokenizer* tokenizer) {
    AtTextureBase::Read(database, name, tokenizer);
    AtGrD3DTextureDatabase* textures = static_cast<AtGrD3DTextureDatabase*>(database);
    if (m_flags & (kFrontBufferSource | kBackBufferSource)) {
        m_surfaces = textures->TakeSurfaces2D(1);
    } else {
        m_surfaces = textures->TakeSurfaces2D(m_levelsAsked);
    }
}

void AtGrD3DTexture2D::ClearPending() {
    if (m_flags & kPending) {
        if (m_surfaces != 0) {
            Release();
        }
        AtTextureBase::ClearPending();
    }
}

void AtGrD3DTexture2D::Reset() {
    Release();
    AtTextureBase::Reset();
    m_texture = 0;
    m_surfaces = 0;
    m_owner = 0;
}

AtGrD3DSurface2D* AtGrD3DTexture2D::SurfaceAt(unsigned int level) {
    return &m_surfaces[level];
}

// ---- The database -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DTextureDatabase::AtGrD3DTextureDatabase() {
    m_next = 0;
    m_surfaces2D = 0;
    m_surfaces3D = 0;
    m_textures2D = 0;
    m_textures3D = 0;
    m_cubeMaps = 0;
    m_owner = 0;
    m_surfaces2DTaken = 0;
    m_unknown50 = 0;
}

AtGrD3DTextureDatabase::~AtGrD3DTextureDatabase() {
    AtGrD3DTextureDatabase::Clear();
}

void AtGrD3DTextureDatabase::Clear() {
    if (m_owner != 0) {
        m_owner->RemoveTextureDatabase(this);
    }
    if (m_textures2D != 0) {
        delete[] m_textures2D;
    }
    if (m_textures3D != 0) {
        delete[] m_textures3D;
    }
    if (m_cubeMaps != 0) {
        delete[] m_cubeMaps;
    }
    if (m_surfaces2D != 0) {
        delete[] m_surfaces2D;
    }
    if (m_surfaces3D != 0) {
        delete[] m_surfaces3D;
    }
    AtTextureDatabase::Clear();
    m_surfaces2D = 0;
    m_surfaces3D = 0;
    m_textures2D = 0;
    m_textures3D = 0;
    m_cubeMaps = 0;
    m_owner = 0;
    m_surfaces2DTaken = 0;
    m_unknown50 = 0;
}

void AtGrD3DTextureDatabase::OnReady(AtMaterialOwner* owner) {
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    m_owner->AddTextureDatabase(this);
}

void AtGrD3DTextureDatabase::MakeSurfaces2D() {
    MakeArray(&m_surfaces2D, m_surface2DCount);
}

void AtGrD3DTextureDatabase::MakeSurfaces3D() {
    MakeArray(&m_surfaces3D, m_surface3DCount);
}

void AtGrD3DTextureDatabase::MakeTextures2D() {
    MakeArray(&m_textures2D, m_texture2DCount);
}

void AtGrD3DTextureDatabase::MakeTextures3D() {
    MakeArray(&m_textures3D, m_texture3DCount);
}

void AtGrD3DTextureDatabase::MakeCubeMaps() {
    MakeArray(&m_cubeMaps, m_cubeMapCount);
}

void AtGrD3DTextureDatabase::MakeMore() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_more = new AtGrD3DTextureDatabase;
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
    m_more->Allocate(m_owner, m_surface2DCount, m_surface3DCount, m_texture2DCount, m_texture3DCount, m_cubeMapCount, ns);
}

void* AtGrD3DTextureDatabase::Surface2DAt(unsigned int index) {
    return &m_surfaces2D[index];
}

void* AtGrD3DTextureDatabase::Surface3DAt(unsigned int index) {
    return &m_surfaces3D[index];
}

AtTextureItem* AtGrD3DTextureDatabase::Texture2DAt(unsigned int index) {
    return &m_textures2D[index];
}

AtTextureItem* AtGrD3DTextureDatabase::Texture3DAt(unsigned int index) {
    return &m_textures3D[index];
}

AtTextureItem* AtGrD3DTextureDatabase::CubeMapAt(unsigned int index) {
    return &m_cubeMaps[index];
}

AtGrD3DSurface2D* AtGrD3DTextureDatabase::TakeSurfaces2D(unsigned int count) {
    if (m_surfaces2DTaken + count > m_surface2DCount) {
        // The original copies its message to its frame first.
        char message[] = "Atlas Graphics: Texture database ran out of 2D surfaces for textures\n";
        AtFatal(message, 0, 0, 0);
    }
    AtGrD3DSurface2D* taken = &m_surfaces2D[m_surfaces2DTaken];
    m_surfaces2DTaken += count;
    return taken;
}
