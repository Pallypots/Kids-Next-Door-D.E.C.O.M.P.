// A material, as read from the .AGM files: colours, how to shade and blend, and which textures. Original address cluster 0x00038370-0x00038F6F. The keyword names are the original's, from its table
// at 0x002B15C0; the flag names follow them. Other names are inferred. Nothing here draws: slot 0 is what a concrete material, at vftable 0x0028A204, does with this for the graphics device.
#pragma once

#include "at/AtIoTokenizer.h"

struct AtColor {
    void SetWhite();                           // 0x000412B0 all 255
    void Read(AtIoTokenizer* tokenizer);       // 0x00041270 four integers
    unsigned char r, g, b, a;
};

// What holds the arrays a material's textures live in, known here by these fields. Vftable 0x00275124; not recovered.
struct AtMaterialStore {
    char m_unknown00[0x18];
    unsigned int m_stageCapacity;    // +0x18
    unsigned int m_stageUsed;        // +0x1C
    void** m_stages;                 // +0x20
    unsigned int m_textureCapacity;  // +0x24
    unsigned int m_textureUsed;      // +0x28
    void** m_textures;               // +0x2C
};

// What materials load for, known here by one flag and by what finds a shader. Not recovered.
struct AtMaterialOwner {
    char m_unknown00[0x30];
    signed char m_noTextures;        // +0x30 negative: a material loaded for it keeps no shader or textures
};

// Not recovered: 0x000383A0, which looks a name up in two tables of the owner. platform or a port supplies it. ns may be empty or null.
void* AtFindShader(AtMaterialOwner* owner, const char* name, const char* ns);

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
    void Read(AtMaterialOwner* owner, AtMaterialStore* store, AtIoTokenizer* tokenizer, const char* ns);
    // 0x00038AD0. Takes room for its arrays from store, copying them. Power and the sort bias are not copied.
    void Copy(AtMaterialOwner* owner, AtMaterialStore* store, const AtMaterial& from);
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
    AtMaterialStore* m_store;                  // +0x44
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
static_assert(offsetof(AtMaterialStore, m_stages) == 0x20, "AtMaterialStore::m_stages");
#pragma pop_macro("offsetof")
#endif
