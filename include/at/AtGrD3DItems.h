// The Xbox graphics layer's shader, light, font and image databases, and the items they hold: what the concrete loader makes for an .AGD's ShaderDatabase, LightDatabase, FontDatabase and ImageDatabase lists.
// All names are inferred. See docs/worklog/2026-11-19-xshade.md.
//
// What is recovered: the shader and light databases and their items whole (vftables 0x0028A720, 0x0028A718, 0x0028A764, 0x00288B88, and the base light's 0x00274C78); the font and image databases but for
// one or two slots each; and of the concrete 2D font and image what reading,
// emptying, clearing and destroying need. What is not, and stops: what a font or an image does with the graphics device when it has been read (slot 9 of the font, 0x00123850; slot 10 of the image,
// 0x00124CD0), a seam in src/at/AtGrD3DItemSeam.cpp that a test supplies; and in src/at/AtGrD3DUnrecovered.cpp, the font database's binary form (slot 5, 0x00111630), the slots of either database that hand
// out the first free item (their base type is an int and they return a pointer), and the items' slots that draw or make textures.
//
// A concrete font or image holds a texture database and a 2D surface of its own, at the original's offsets: what its slot 9 or 10 presumably loads the picture into. Clearing and destroying reach them.
#pragma once

#include "at/AtGrD3DTextures.h"
#include "at/AtImageDatabase.h"
#include "at/AtLight.h"
#include "at/AtShaderDatabase.h"

struct AtGrD3DOwner;
class AtGrD3DFont2D;
class AtGrD3DImage;

// ---- The seam: src/at/AtGrD3DItemSeam.cpp, or a test's -------------------------------------------------------------------------------------------------------------------------------------------------------

void AtGrD3DLoadFont2D(AtGrD3DFont2D* font, AtMaterialOwner* owner);  // 0x00123850 slot 9: what the font does with the device once read
void AtGrD3DLoadImage(AtGrD3DImage* image, AtMaterialOwner* owner);   // 0x00124CD0 slot 10: the image's pixels and texture

// ---- Shaders --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028A718: slot 0 0x00110390 vector deleting, slot 1 the base's Clear. 0x38 bytes. Nothing of its own: everything a shader is, is read and drawn through the base and the device.
class AtGrD3DShader : public AtShader {
public:
    AtGrD3DShader();                          // 0x00110230
    ~AtGrD3DShader() override;                // 0x00110250 a jump to the base's
};

// Vftable 0x0028A720, 0x1C bytes. Shaders are made from AtShaderDatabase::s_heap by the count. Not on any owner's list.
class AtGrD3DShaderDatabase : public AtShaderDatabase {
public:
    AtGrD3DShaderDatabase();                  // 0x00110260
    ~AtGrD3DShaderDatabase() override;        // slot 0; 0x001124A0 vector deleting, 0x001103F0 complete
    void Clear() override;                    // slot 1, 0x00110280 the array, then the base's
    void OnReady() override;                  // slot 2, 0x001102D0 m_count shaders
    AtShader* ShaderAt(unsigned int index) override;  // slot 3, 0x00110220 not bounds checked

    AtGrD3DShader* m_shaders;                 // +0x18
};

// ---- Lights ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x00288B88: slot 0 0x001112F0 vector deleting, slot 1 the base's Reset. 0x50 bytes. Nothing of its own.
class AtGrD3DLight : public AtLight {
public:
    AtGrD3DLight();                           // 0x000EB7D0
    ~AtGrD3DLight() override;                 // 0x000EBFD0 a jump to the base's
};

// Vftable 0x0028A764, 0x20 bytes. Lights are made from AtLightDatabase::s_heap by the count. Not on any owner's list.
class AtGrD3DLightDatabase : public AtLightDatabase {
public:
    AtGrD3DLightDatabase();                   // 0x001111B0
    ~AtGrD3DLightDatabase() override;         // slot 0; 0x00112700 vector deleting, 0x00111350 complete
    void Clear() override;                    // slot 1, 0x001111D0 the array, everything to 0, then the base's
    void MakeLights(AtMaterialOwner* owner) override;  // slot 2, 0x00111210 m_count lights; keeps the owner, after the array, even when there is none
    AtLight* LightAt(unsigned int index) override;     // slot 3, 0x00111190 not bounds checked

    AtGrD3DLight* m_lights;                   // +0x18
    AtMaterialOwner* m_owner;                 // +0x1C
};

// ---- Fonts ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028B970, 0x10C bytes. Slots 4, 11, 12 and 13 draw or make textures and are not recovered: they stop. Slot 3 is the base's, which stops too.
class AtGrD3DFont2D : public AtGrI2DFont {
public:
    enum Flags { kBorrowedChars = 0x200 };    // in m_flags: the character table is not the font's, and emptying forgets it rather than freeing it. What sets it is not recovered.

    AtGrD3DFont2D();                          // 0x001236B0
    ~AtGrD3DFont2D() override;                // slot 5; 0x001114E0 vector deleting, 0x00123770 complete. As Free, then the members and the base.
    void Slot4() override;                    // 0x00124340
    void Free() override;                     // slot 8, 0x00123710 its surface reset, its textures cleared, a borrowed table forgotten, then the 2D font's Free
    void OnLoaded(AtMaterialOwner* owner) override;  // slot 9, 0x00123850: the seam
    void FreeData() override;                 // slot 10, 0x001238A0 when it owns its data: surface, textures, then the 2D font's
    void Slot11() override;                   // 0x00123750 a texture's first surface, by index (one argument in the original)
    void Slot12() override;                   // 0x00123C40
    int PassName(AtGrDevice* device, const AtGrI2DFont* font, int value) override;  // slot 13, 0x001238D0

    AtGrD3DTextureDatabase m_textures;        // +0x64
    AtGrD3DSurface2D m_surface;               // +0xBC
};

// Vftable 0x0028A774, 0x3C bytes. Fonts are made from AtFontDatabase::s_heap by the count.
class AtGrD3DFontDatabase : public AtFontDatabase {
public:
    AtGrD3DFontDatabase();                    // 0x001115A0
    ~AtGrD3DFontDatabase() override;          // slot 0; 0x00112830 vector deleting, 0x001119B0 complete
    void Clear() override;                    // slot 1, 0x001115D0 the binary block, off the owner's list, the array, everything to 0 (m_next too), then the base's
    void OnReady(AtMaterialOwner* owner) override;  // slot 2, 0x00111400 keeps the owner and goes on its list
    void MakeFonts2D() override;              // slot 3, 0x00111420 m_count fonts
    int Slot4() override;                     // 0x001A2B40 returns 1, folded: it has a binary form
    void Slot5(int a, int b) override;        // 0x00111630 the binary form, .XGF, which nothing asks for: stops
    AtGrI2DFont* Font2DAt(unsigned int index) override;  // slot 6, 0x001113E0 not bounds checked
    int Slot7() override;                     // 0x00111980 the first font with no characters, a pointer: stops, see above

    AtGrD3DOwner* m_owner;                    // +0x28
    AtGrD3DFont2D* m_fonts;                   // +0x2C
    void* m_binary;                           // +0x30 freed by Clear; set by the binary form only
    unsigned int m_unknown34;                 // +0x34 cleared only
    AtGrD3DFontDatabase* m_next;              // +0x38 on the owner's list
};

// ---- Images ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0028B9B8, 0xF0 bytes. Slots 0 to 3, 5 and 14 look up materials, draw or make textures and are not recovered: they stop.
class AtGrD3DImage : public AtImage {
public:
    AtGrD3DImage();                           // 0x00124580
    ~AtGrD3DImage() override;                 // slot 6; 0x00111B00 vector deleting, 0x00124660 complete. As Free, then the members and the base.
    void Slot0() override;                    // 0x001247D0 the image's material, or one of two of the device's own
    void Slot1() override;                    // 0x00124EB0
    void Slot2() override;                    // 0x00124630 slot 3 with its six arguments
    void Slot3() override;                    // 0x00124800
    int PassName(AtGrDevice* device, const AtImage* image, int value) override;  // slot 5, 0x001246F0
    void Free() override;                     // slot 9, 0x001245E0 its surface reset, its textures cleared, then the image's Free
    void OnLoaded(AtMaterialOwner* owner) override;  // slot 10, 0x00124CD0: the seam
    void FreeData() override;                 // slot 11, 0x00124730 when it owns its data: textures, surface (the font's order reversed), then the image's
    void Slot14() override;                   // 0x00124610 a texture's first surface, by index (one argument in the original)

    unsigned int m_unknown3C[3];              // +0x3C not set by the constructor
    AtGrD3DTextureDatabase m_textures;        // +0x48
    AtGrD3DSurface2D m_surface;               // +0xA0
};

// Vftable 0x0028A794, 0x34 bytes. Images are made from AtImageDatabase::s_heap by the count.
class AtGrD3DImageDatabase : public AtImageDatabase {
public:
    AtGrD3DImageDatabase();                   // 0x00111BC0
    ~AtGrD3DImageDatabase() override;         // slot 0; 0x00112960 vector deleting, 0x00111C60 complete
    void Clear() override;                    // slot 1, 0x00111BE0 the array, off the owner's list (the font's order reversed), everything to 0 (m_next too), then the base's
    void MakeItems() override;                // slot 2, 0x00111A40 m_count images
    void OnReady(AtMaterialOwner* owner) override;  // slot 3, 0x00111A20 keeps the owner and goes on its list
    AtImage* ImageAt(unsigned int index) override;  // slot 4, 0x00111A00 not bounds checked
    int Slot5() override;                     // 0x00111C30 the first image not read, a pointer: stops, see above

    AtGrD3DOwner* m_owner;                    // +0x28
    AtGrD3DImage* m_images;                   // +0x2C
    AtGrD3DImageDatabase* m_next;             // +0x30 on the owner's list
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtGrD3DShader) == 0x38, "AtGrD3DShader");
static_assert(offsetof(AtGrD3DShaderDatabase, m_shaders) == 0x18, "AtGrD3DShaderDatabase::m_shaders");
static_assert(sizeof(AtGrD3DShaderDatabase) == 0x1C, "AtGrD3DShaderDatabase");
static_assert(sizeof(AtGrD3DLight) == 0x50, "AtGrD3DLight");
static_assert(offsetof(AtGrD3DLightDatabase, m_owner) == 0x1C, "AtGrD3DLightDatabase::m_owner");
static_assert(sizeof(AtGrD3DLightDatabase) == 0x20, "AtGrD3DLightDatabase");
static_assert(offsetof(AtGrD3DFont2D, m_textures) == 0x64, "AtGrD3DFont2D::m_textures");
static_assert(offsetof(AtGrD3DFont2D, m_surface) == 0xBC, "AtGrD3DFont2D::m_surface");
static_assert(sizeof(AtGrD3DFont2D) == 0x10C, "AtGrD3DFont2D");
static_assert(offsetof(AtGrD3DFontDatabase, m_owner) == 0x28, "AtGrD3DFontDatabase::m_owner");
static_assert(offsetof(AtGrD3DFontDatabase, m_next) == 0x38, "AtGrD3DFontDatabase::m_next");
static_assert(sizeof(AtGrD3DFontDatabase) == 0x3C, "AtGrD3DFontDatabase");
static_assert(offsetof(AtGrD3DImage, m_textures) == 0x48, "AtGrD3DImage::m_textures");
static_assert(offsetof(AtGrD3DImage, m_surface) == 0xA0, "AtGrD3DImage::m_surface");
static_assert(sizeof(AtGrD3DImage) == 0xF0, "AtGrD3DImage");
static_assert(offsetof(AtGrD3DImageDatabase, m_owner) == 0x28, "AtGrD3DImageDatabase::m_owner");
static_assert(offsetof(AtGrD3DImageDatabase, m_next) == 0x30, "AtGrD3DImageDatabase::m_next");
static_assert(sizeof(AtGrD3DImageDatabase) == 0x34, "AtGrD3DImageDatabase");
#pragma pop_macro("offsetof")
#endif
