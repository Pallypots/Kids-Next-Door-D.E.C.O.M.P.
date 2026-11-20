// A material, as read from the .AGM files: colours, how to shade and blend, and which textures. Original address cluster 0x00038370-0x00038F6F. The keyword names are the original's, from its table
// at 0x002B15C0; the flag names follow them. Other names are inferred. Nothing here draws: slot 0 is what a concrete material, at vftable 0x0028A204, does with this for the graphics device.
#pragma once

#include "at/AtDisplayDevice.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

struct AtColor {
    void SetWhite();                           // 0x000412B0 all 255
    void SetBlack();                           // 0x000412C0 0, 0, 0 and alpha 255; in src/at/AtImage.cpp
    void Read(AtIoTokenizer* tokenizer);       // 0x00041270 four integers
    unsigned char r, g, b, a;
};

// Four colours, one for each corner, read in order ("ColorValueRectangle"). 16 bytes. The bodies are in src/at/AtImage.cpp. The .ASD work (as AtColorRectangle, for the flat
// lists) and the image and font work (as AtColorRect) recovered it twice; folded.
struct AtColorRectangle {
    AtColorRectangle();                                // 0x00042170 all white
    void Read(AtIoTokenizer* tokenizer);               // 0x00042140 four colours of four integers
    void SetAll(const AtColor& color);                 // 0x000421A0
    AtColor m_colors[4];
};

class AtMaterialDatabase;  // include/at/AtMaterialDatabase.h
class AtMatrixDevice;      // include/at/AtCoordSys.h
void AtFatal(const char* message, int file, int line, int unused);

class AtGfxContext;     // include/at/AtGraphicsDevice.h
class AtGfxDrawLists;   // include/at/AtGraphicsDevice.h
class AtGfxBufferSet;   // include/at/AtGraphicsDevice.h
class AtGfxObject;      // include/at/AtGraphicsDevice.h

// The abstract graphics device: vftable 0x00273030, 26 slots, 0x3A8 bytes, constructor 0x00026190. It holds the name tables every database joins: for each kind, a group with no name space and,
// for most, a database of name spaces. Original address clusters 0x00025E70-0x000267FB and 0x00027EF0-0x00028464 (the add and find methods). The name is the one the loaders' work gave it; the
// class is the graphics device's base, and the only class built on it is the concrete device AtGraphicsDeviceXbox (include/at/AtGraphicsDevice.h, vftable 0x00289208): the constructor has the one
// caller 0x000F1FBF. The scene code knows it as AtSceneDevice (include/at/AtScene.h), the same class: see docs/worklog/2026-11-08-gfxdev.md.
//
// Slots 0 to 16 keep the names Slot0 to Slot16, because the harnesses' devices override them by those names. In the original, slots 5 to 10 and 13 to 25 are pure (0x00125F25); here they stop with
// a diagnostic, as a pure call does. Slots 13 to 16 and 18 to 25 are named for what the concrete device's do.
struct AtMaterialOwner : AtDisplayDevice {
    AtMaterialOwner();                 // 0x00026190 every table constructed; +0x34 and +0x39C to +0x3A4 cleared. m_animationDevice is left as it is.
    ~AtMaterialOwner();                // 0x000266F0 Reset (this class's, not a derived one's), the name space databases destroyed, then the base's
    void Slot0() override;             // slot 0, 0x00026800 the deleting destructor; the flag 1 is taken as given
    void Slot1() override;             // slot 1, 0x00026560 Reset: every table's group reset and name space database cleared, the words cleared, then the base's Reset
    virtual void Slot5();              // slot 5, pure
    virtual void Slot6();              // slot 6, pure
    virtual void Slot7();              // slot 7, pure
    virtual void Slot8();              // slot 8, pure
    virtual void Slot9();              // slot 9, pure
    virtual void Slot10();             // slot 10, pure
    virtual void Slot11() {}           // slot 11, 0x000447F0: takes two arguments in the original, does nothing
    virtual void Slot12() {}           // slot 12, 0x001CC9F0: does nothing
    virtual void* RenderTarget0();     // slot 13, pure. The concrete device's member at +0x3B0
    virtual void* RenderTarget1();     // slot 14, pure. The concrete device's member at +0x400
    virtual AtGfxContext* Context();   // slot 15, pure
    virtual AtGfxDrawLists* DrawLists();  // slot 16, pure
    virtual AtMatrixDevice* MatrixDevice();  // slot 17, pure. 0x0001A68C hands what it returns to AtCoordSysDatabase::Load. Stops with a diagnostic here; the concrete device's returns its member.
    // Slot 18, pure: the object whose slot made the concrete device, the graphics factory (at/grfactory's AtGraphicsFactory, slot 3), called at 0x00012CC2 and 0x00013245. The
    // Flash code knows that factory as AtFlashGraphicsFactory (include/at/AtFlashShape.h): the same table, not yet folded into one class.
    virtual class AtFlashGraphicsFactory* GraphicsFactory();
    virtual void* Slot19(void* unknown);  // slot 19, pure
    virtual void ReleaseObject(AtGfxObject* object);  // slot 20, pure
    virtual AtGfxBufferSet* BufferSet();  // slot 21, pure
    virtual void Slot22();             // slot 22, pure
    virtual unsigned int Width();      // slot 23, pure: the display mode's
    virtual unsigned int Height();     // slot 24, pure
    virtual void DrawScale(float* out);  // slot 25, pure: two reals, scales for flat drawn things

    // src/at/AtDatabaseLoader.cpp.
    void AddMeshes(GmNameMap* map, const char* ns);          // 0x00025F30 ns may be empty or null
    void AddLightAnimations(GmNameMap* map, const char* ns); // 0x00025FB0 ns may be empty or null
    void AddMorphAnimations(GmNameMap* map, const char* ns); // 0x00026070 ns may be empty or null
    void AddCoordSystems(GmNameMap* map);    // 0x00026130
    void AddMirrors(GmNameMap* map);         // 0x00026140
    void AddWeights(GmNameMap* map);         // 0x00026150
    void AddAnimations(GmNameMap* map);      // 0x00026160
    void AddVertexProcesses(GmNameMap* map); // 0x00026170
    void AddPixelProcesses(GmNameMap* map);  // 0x00026180

    void* FindShader(const char* name, const char* ns);     // 0x000383A0 ns may be empty or null
    void AddMaterials(GmNameMap* map, const char* ns);      // 0x00025EB0 ns may be empty or null
    void AddTextures(GmNameMap* map, const char* ns);       // 0x00025E70 ns may be empty or null; src/at/AtTextureDatabase.cpp
    void AddShaders(GmNameMap* map, const char* ns);        // 0x00025EF0 ns may be empty or null; defined in src/at/AtShaderDatabase.cpp
    // src/at/AtMaterialOwnerLists.cpp.
    void* FindMaterial(const char* name, const char* ns);   // 0x00027EF0 ns may be empty or null
    void AddLights(GmNameMap* map, const char* ns);         // 0x00025F70 ns may be empty or null
    void AddMatAssignments(GmNameMap* map, const char* ns); // 0x00025FF0 ns may be empty or null
    void AddMatAnimations(GmNameMap* map, const char* ns);  // 0x00026030 ns may be empty or null
    void AddFonts(GmNameMap* map, const char* ns);          // 0x000260B0 ns may be empty or null
    void AddImages(GmNameMap* map, const char* ns);         // 0x000260F0 ns may be empty or null
    void* FindFont(const char* name, const char* ns);       // 0x00049290 ns may be empty or null. src/at/AtDrawItem.cpp
    void* FindImage(const char* name, const char* ns);      // 0x00048E50 likewise

    int m_unknown34;                   // +0x34 cleared by the constructor and Reset
    class AtAnimationDevice* m_animationDevice;  // +0x38 include/at/AtAnimationDatabase.h; the constructor leaves it
    GmNameMapGroup m_textures;         // +0x3C
    GmNameSpaceDb m_textureSpaces;     // +0x54
    GmNameMapGroup m_materials;        // +0x78
    GmNameSpaceDb m_materialSpaces;    // +0x90
    GmNameMapGroup m_shaders;          // +0xB4
    GmNameSpaceDb m_shaderSpaces;      // +0xCC
    GmNameMapGroup m_meshes;           // +0xF0
    GmNameSpaceDb m_meshSpaces;        // +0x108
    GmNameMapGroup m_unknown12C;       // +0x12C a table whose add and find methods are not known
    GmNameSpaceDb m_unknown12CSpaces;  // +0x144
    GmNameMapGroup m_lights;               // +0x168
    GmNameSpaceDb m_lightSpaces;           // +0x180
    GmNameMapGroup m_matAssignments;       // +0x1A4
    GmNameSpaceDb m_matAssignmentSpaces;   // +0x1BC
    GmNameMapGroup m_matAnimations;        // +0x1E0
    GmNameSpaceDb m_matAnimationSpaces;    // +0x1F8
    GmNameMapGroup m_morphAnimations;      // +0x21C
    GmNameSpaceDb m_morphAnimationSpaces;  // +0x234
    GmNameMapGroup m_fonts;                // +0x258
    GmNameSpaceDb m_fontSpaces;            // +0x270
    GmNameMapGroup m_images;               // +0x294
    GmNameSpaceDb m_imageSpaces;           // +0x2AC
    GmNameMapGroup m_lightAnimations;      // +0x2D0
    GmNameSpaceDb m_lightAnimationSpaces;  // +0x2E8
    GmNameMapGroup m_coordSystems;         // +0x30C
    GmNameMapGroup m_mirrors;              // +0x324
    GmNameMapGroup m_weights;              // +0x33C
    GmNameMapGroup m_animations;           // +0x354
    GmNameMapGroup m_vertexProcesses;  // +0x36C searched by name by a shader; what fills it is not recovered
    GmNameMapGroup m_pixelProcesses;   // +0x384 searched by name by a shader; what fills it is not recovered
    int m_unknown39C;                  // +0x39C cleared by the constructor and Reset
    int m_unknown3A0;                  // +0x3A0 cleared likewise. The concrete device's slot 10 walks a list from here, by each item's +8, calling its slot 5
    int m_unknown3A4;                  // +0x3A4 cleared likewise
};

// What a material hands on to the device layer. Filled by Describe.
struct AtMaterialDesc {
    AtMaterialDesc();                          // 0x00038E40
    int m_unknown00;                           // +0x00
    void* m_store;                             // +0x04
    AtColor m_ambient;                         // +0x08
    AtColor m_diffuse;                         // +0x0C
    AtColor m_specular;                        // +0x10
    AtColor m_constant;                        // +0x14
    unsigned int m_flags;                      // +0x18
    int m_alphaCompare;                        // +0x1C
    int m_alphaReference;                      // +0x20
    int m_depthCompare;                        // +0x24
    int m_blend;                               // +0x28
    int m_unknown2C;                           // +0x2C not set by Describe
    int m_stencilFail;                         // +0x30
    int m_stencilDepthFail;                    // +0x34
    int m_stencilCompare;                      // +0x38
    int m_stencilReference;                    // +0x3C
    int m_projectionMask;                      // +0x40
    int m_unknown44;                           // +0x44
    float m_range;                             // +0x48
    void* m_shader;                            // +0x4C
    int m_stageCount;                          // +0x50
    void** m_stages;                           // +0x54
    int m_textureCount;                        // +0x58
    void** m_textures;                         // +0x5C
};

// Vftable 0x00274E20.
class AtMaterial {
public:
    enum Flags {
        kLoaded = 0x1, kGouraudShade = 0x2, kColorModulate = 0x4, kAlphaModulate = 0x8, kConstantColor = 0x10, kConstantAlpha = 0x20, kAlphaCompare = 0x40, kBlend = 0x80,
        kBlendSeparateAlpha = 0x100, kAntiAliasedTransparency = 0x200, kRangedAntiAliasedTransparency = 0x400, kStencilOperation = 0x800, kEmissive = 0x1000, kSingleSided = 0x2000,
        kDoubleSided = 0x4000, kDisableLighting = 0x8000, kPower = 0x10000, kOverBright = 0x20000, kDisableFogging = 0x80000, kDisableDepthBuffer = 0x100000, kDisableDepthWrites = 0x200000,
        kDisableColorWrites = 0x400000, kDepthCompare = 0x800000, kClamp = 0x1000000
    };
    enum { kKeywordCount = 70 };
    static const AtIoKeyword s_keywords[kKeywordCount];  // 0x002B15C0: for the whole file, the database's keywords among them

    AtMaterial();                              // 0x00038EB0 colours are left as they are
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;   // slot 0
    virtual ~AtMaterial();                     // slot 1; 0x00038F00 deleting. The complete form is inlined there.
    virtual void Reset();                      // slot 2, 0x00038C50 colours and the blend for alpha are left as they are
    virtual void SetAlphaCompare(unsigned char compare, unsigned char reference);  // slot 3, 0x00038370
    virtual void ClearAlphaCompare();          // slot 4, 0x00038390

    // 0x00038590. The token after the name, a left curly, through its right curly. ns goes to the shader lookup. Errors go to the tokenizer, and reading goes on.
    void Read(AtMaterialOwner* owner, AtMaterialDatabase* store, AtIoTokenizer* tokenizer, const char* ns);
    // 0x00038AD0. Takes room for its arrays from store, copying them. Power and the sort bias are not copied.
    void Copy(AtMaterialOwner* owner, AtMaterialDatabase* store, const AtMaterial& from);
    void Describe(AtMaterialDesc* out) const;  // 0x00038D60

    int ReadBlendMode(AtIoTokenizer* tokenizer, int token);   // 0x000381A0
    int ReadStencilAction(AtIoTokenizer* tokenizer);          // 0x00038250
    int ReadCompare(AtIoTokenizer* tokenizer);                // 0x000382E0

    AtColor m_ambient;                         // +0x04
    AtColor m_diffuse;                         // +0x08
    AtColor m_constant;                        // +0x0C its fourth byte is ConstantAlpha
    AtColor m_specular;                        // +0x10
    float m_power;                             // +0x14
    void* m_shader;                            // +0x18
    void** m_stages;                           // +0x1C
    void** m_textures;                         // +0x20
    int m_projectionMask;                      // +0x24
    int m_unknown28;                           // +0x28
    float m_range;                             // +0x2C
    unsigned int m_flags;                      // +0x30
    unsigned char m_textureCount;              // +0x34
    unsigned char m_depthCompare;              // +0x35
    unsigned char m_stageCount;                // +0x36
    unsigned char m_unknown37;                 // +0x37 nothing reads or sets it
    unsigned char m_blend;                     // +0x38
    unsigned char m_blendAlpha;                // +0x39
    unsigned char m_alphaCompare;              // +0x3A
    unsigned char m_alphaReference;            // +0x3B
    unsigned char m_stencilFail;               // +0x3C
    unsigned char m_stencilDepthFail;          // +0x3D
    unsigned char m_stencilCompare;            // +0x3E
    unsigned char m_stencilReference;          // +0x3F
    float m_sortBias;                          // +0x40
    AtMaterialDatabase* m_store;                  // +0x44
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtMaterial, m_flags) == 0x30, "AtMaterial::m_flags");
static_assert(offsetof(AtMaterial, m_blend) == 0x38, "AtMaterial::m_blend");
static_assert(offsetof(AtMaterial, m_store) == 0x44, "AtMaterial::m_store");
static_assert(sizeof(AtMaterial) == 0x48, "AtMaterial");
static_assert(offsetof(AtMaterialDesc, m_textures) == 0x5C, "AtMaterialDesc::m_textures");
static_assert(offsetof(AtMaterialOwner, m_noTextures) == 0x30, "AtMaterialOwner::m_noTextures");
static_assert(offsetof(AtMaterialOwner, m_textures) == 0x3C, "AtMaterialOwner::m_textures");
static_assert(offsetof(AtMaterialOwner, m_textureSpaces) == 0x54, "AtMaterialOwner::m_textureSpaces");
static_assert(offsetof(AtMaterialOwner, m_materials) == 0x78, "AtMaterialOwner::m_materials");
static_assert(offsetof(AtMaterialOwner, m_materialSpaces) == 0x90, "AtMaterialOwner::m_materialSpaces");
static_assert(offsetof(AtMaterialOwner, m_shaders) == 0xB4, "AtMaterialOwner::m_shaders");
static_assert(offsetof(AtMaterialOwner, m_shaderSpaces) == 0xCC, "AtMaterialOwner::m_shaderSpaces");
static_assert(offsetof(AtMaterialOwner, m_lights) == 0x168, "AtMaterialOwner::m_lights");
static_assert(offsetof(AtMaterialOwner, m_lightSpaces) == 0x180, "AtMaterialOwner::m_lightSpaces");
static_assert(offsetof(AtMaterialOwner, m_matAssignments) == 0x1A4, "AtMaterialOwner::m_matAssignments");
static_assert(offsetof(AtMaterialOwner, m_matAssignmentSpaces) == 0x1BC, "AtMaterialOwner::m_matAssignmentSpaces");
static_assert(offsetof(AtMaterialOwner, m_matAnimations) == 0x1E0, "AtMaterialOwner::m_matAnimations");
static_assert(offsetof(AtMaterialOwner, m_matAnimationSpaces) == 0x1F8, "AtMaterialOwner::m_matAnimationSpaces");
static_assert(offsetof(AtMaterialOwner, m_fonts) == 0x258, "AtMaterialOwner::m_fonts");
static_assert(offsetof(AtMaterialOwner, m_fontSpaces) == 0x270, "AtMaterialOwner::m_fontSpaces");
static_assert(offsetof(AtMaterialOwner, m_images) == 0x294, "AtMaterialOwner::m_images");
static_assert(offsetof(AtMaterialOwner, m_imageSpaces) == 0x2AC, "AtMaterialOwner::m_imageSpaces");
static_assert(offsetof(AtMaterialOwner, m_animationDevice) == 0x38, "AtMaterialOwner::m_animationDevice");
static_assert(offsetof(AtMaterialOwner, m_meshes) == 0xF0, "AtMaterialOwner::m_meshes");
static_assert(offsetof(AtMaterialOwner, m_meshSpaces) == 0x108, "AtMaterialOwner::m_meshSpaces");
static_assert(offsetof(AtMaterialOwner, m_morphAnimations) == 0x21C, "AtMaterialOwner::m_morphAnimations");
static_assert(offsetof(AtMaterialOwner, m_morphAnimationSpaces) == 0x234, "AtMaterialOwner::m_morphAnimationSpaces");
static_assert(offsetof(AtMaterialOwner, m_lightAnimations) == 0x2D0, "AtMaterialOwner::m_lightAnimations");
static_assert(offsetof(AtMaterialOwner, m_lightAnimationSpaces) == 0x2E8, "AtMaterialOwner::m_lightAnimationSpaces");
static_assert(offsetof(AtMaterialOwner, m_coordSystems) == 0x30C, "AtMaterialOwner::m_coordSystems");
static_assert(offsetof(AtMaterialOwner, m_mirrors) == 0x324, "AtMaterialOwner::m_mirrors");
static_assert(offsetof(AtMaterialOwner, m_weights) == 0x33C, "AtMaterialOwner::m_weights");
static_assert(offsetof(AtMaterialOwner, m_animations) == 0x354, "AtMaterialOwner::m_animations");
static_assert(offsetof(AtMaterialOwner, m_vertexProcesses) == 0x36C, "AtMaterialOwner::m_vertexProcesses");
static_assert(offsetof(AtMaterialOwner, m_pixelProcesses) == 0x384, "AtMaterialOwner::m_pixelProcesses");
static_assert(offsetof(AtMaterialOwner, m_unknown34) == 0x34, "AtMaterialOwner::m_unknown34");
static_assert(offsetof(AtMaterialOwner, m_unknown39C) == 0x39C, "AtMaterialOwner::m_unknown39C");
static_assert(sizeof(AtMaterialOwner) == 0x3A8, "AtMaterialOwner");
#pragma pop_macro("offsetof")
#endif
