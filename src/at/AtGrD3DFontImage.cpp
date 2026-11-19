// The Xbox font and image databases, their 2D fonts and images, and the owner's lists of them. See include/at/AtGrD3DItems.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

// ---- The owner's lists --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DOwner::AddFontDatabase(AtGrD3DFontDatabase* database) {
    database->m_next = m_fontDatabases;
    m_fontDatabases = database;
}

void AtGrD3DOwner::RemoveFontDatabase(AtGrD3DFontDatabase* database) {
    AtGrD3DFontDatabase** link = &m_fontDatabases;
    for (AtGrD3DFontDatabase* at = *link; at != 0; at = *link) {
        if (at == database) {
            *link = database->m_next;
            return;
        }
        link = &at->m_next;
    }
}

void AtGrD3DOwner::AddImageDatabase(AtGrD3DImageDatabase* database) {
    database->m_next = m_imageDatabases;
    m_imageDatabases = database;
}

void AtGrD3DOwner::RemoveImageDatabase(AtGrD3DImageDatabase* database) {
    AtGrD3DImageDatabase** link = &m_imageDatabases;
    for (AtGrD3DImageDatabase* at = *link; at != 0; at = *link) {
        if (at == database) {
            *link = database->m_next;
            return;
        }
        link = &at->m_next;
    }
}

// ---- 2D fonts -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DFont2D::AtGrD3DFont2D() {
}

AtGrD3DFont2D::~AtGrD3DFont2D() {
    AtGrD3DFont2D::Free();
}

void AtGrD3DFont2D::Free() {
    m_surface.Reset();
    m_textures.Clear();
    if (m_flags & kBorrowedChars) {
        m_chars = 0;
    }
    AtGrI2DFont::Free();
}

void AtGrD3DFont2D::OnLoaded(AtMaterialOwner* owner) {
    AtGrD3DLoadFont2D(this, owner);
}

void AtGrD3DFont2D::FreeData() {
    if (m_flags & kOwnsData) {
        m_surface.Reset();
        m_textures.Clear();
        AtGrI2DFont::FreeData();
    }
}

// ---- The font database --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DFontDatabase::AtGrD3DFontDatabase() {
    m_owner = 0;
    m_fonts = 0;
    m_next = 0;
    m_binary = 0;
    m_unknown34 = 0;
}

AtGrD3DFontDatabase::~AtGrD3DFontDatabase() {
    AtGrD3DFontDatabase::Clear();
}

void AtGrD3DFontDatabase::Clear() {
    if (m_binary != 0) {
        operator delete(m_binary);
    }
    if (m_owner != 0) {
        m_owner->RemoveFontDatabase(this);
    }
    if (m_fonts != 0) {
        delete[] m_fonts;
    }
    m_owner = 0;
    m_fonts = 0;
    m_next = 0;
    m_binary = 0;
    m_unknown34 = 0;
    AtFontDatabase::Clear();
}

void AtGrD3DFontDatabase::OnReady(AtMaterialOwner* owner) {
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    m_owner->AddFontDatabase(this);
}

void AtGrD3DFontDatabase::MakeFonts2D() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_fonts = new AtGrD3DFont2D[m_count];
    AtPopHeap();
    if (m_fonts == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

int AtGrD3DFontDatabase::Slot4() {
    return 1;
}

AtGrI2DFont* AtGrD3DFontDatabase::Font2DAt(unsigned int index) {
    return &m_fonts[index];
}

// ---- Images -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DImage::AtGrD3DImage() {
}

AtGrD3DImage::~AtGrD3DImage() {
    AtGrD3DImage::Free();
}

void AtGrD3DImage::Free() {
    m_surface.Reset();
    m_textures.Clear();
    AtImage::Free();
}

void AtGrD3DImage::OnLoaded(AtMaterialOwner* owner) {
    AtGrD3DLoadImage(this, owner);
}

void AtGrD3DImage::FreeData() {
    if (m_flags & kOwnsData) {
        m_textures.Clear();
        m_surface.Reset();
        AtImage::FreeData();
    }
}

// ---- The image database -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtGrD3DImageDatabase::AtGrD3DImageDatabase() {
    m_owner = 0;
    m_images = 0;
    m_next = 0;
}

AtGrD3DImageDatabase::~AtGrD3DImageDatabase() {
    AtGrD3DImageDatabase::Clear();
}

void AtGrD3DImageDatabase::Clear() {
    if (m_images != 0) {
        delete[] m_images;
    }
    if (m_owner != 0) {
        m_owner->RemoveImageDatabase(this);
    }
    m_owner = 0;
    m_images = 0;
    m_next = 0;
    AtImageDatabase::Clear();
}

void AtGrD3DImageDatabase::MakeItems() {
    if (s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_heap);
    m_images = new AtGrD3DImage[m_count];
    AtPopHeap();
    if (m_images == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

void AtGrD3DImageDatabase::OnReady(AtMaterialOwner* owner) {
    m_owner = static_cast<AtGrD3DOwner*>(owner);
    m_owner->AddImageDatabase(this);
}

AtImage* AtGrD3DImageDatabase::ImageAt(unsigned int index) {
    return &m_images[index];
}
