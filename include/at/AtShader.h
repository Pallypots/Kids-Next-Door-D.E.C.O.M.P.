// What a surface is drawn with, as read from the .AGS files: a shader of staged or programmable components, each with texture stages. That these are shaders is confirmed by the loader's keywords, by
// its error messages ("unable to find texture used by shader") and by AtMaterialOwner::FindShader, which searches the tables a database joins. Original address clusters 0x00037520-0x000379F1,
// 0x00044940-0x0004541B and 0x00055630-0x000556D9. Keyword names are the original's, from its table at 0x002B0CC0; class and member names are inferred. Nothing here draws: a concrete shader, at
// vftable 0x0028A718, does that for the graphics device, and it and its database are not recovered.
#pragma once

#include "at/AtMaterial.h"

class AtShaderDatabase;  // include/at/AtShaderDatabase.h

// How a component's result joins the one before: 0 none, else 1 Add to 10 BlendInverseAlpha, in keyword order.
struct AtShaderCombineOp {
    AtShaderCombineOp();                       // 0x00055630
    void Read(AtIoTokenizer* tokenizer);       // 0x00055640

    int m_op;                                  // +0x00
};

// One texture and how to sample it, read from a "{ ... }" block.
struct AtTextureStage {
    // m_flags. Three small fields and some switches.
    enum Flag {
        kFilterMask = 0x000F,        // 0 PointSampled, 1 Bilinear, 2 Anisotropic, 4 FlatCubic, 8 GaussianCubic
        kFlagClamp = 0x0010,
        kFlagBorder = 0x0020,
        kCoordGenMask = 0x01C0,      // In units of 0x40: 1 Normal, 2 NormalWithScaleAndBias, 3 Pos, 4 Reflection, 5 ReflectionWithScaleAndBias, 6 ToonLighting
        kFlagViewToWorld = 0x0200,
        kFlagTransform2D = 0x0400,
        kFlagTransform3D = 0x0800,
        kFlagTransform4D = 0x1000,
        kTransformMask = kFlagTransform2D | kFlagTransform3D | kFlagTransform4D,
        kFlagProjected = 0x2000,
        kFlagCubeNormalization = 0x4000,
        kFlagMipLinear = 0x8000
    };

    AtTextureStage();                          // 0x00044940 all zero, and the anisotropy 1
    ~AtTextureStage();                         // the same again, through Reset
    void Reset();                              // 0x00044DB0 this is the address the arrays' destructors call
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, const char* ns);  // 0x00044970

    void* m_texture;                           // +0x00
    AtColor m_border;                          // +0x04 Border
    unsigned int m_flags;                      // +0x08
    unsigned short m_coordOffset;              // +0x0C TxtCoordOffset
    unsigned short m_transform;                // +0x0E Coord2DTransform to Coord4DTransform, cut to a byte on reading
    float m_anisotropy;                        // +0x10 Anisotropic
};

// The switches a component can have.
enum AtShaderSwitch { kShaderFlipFaceCulling = 1, kShaderDisableDepthBuffer = 2, kShaderDisableDepthWrites = 4, kShaderDisableColorWrites = 8 };

// One fixed-function stage of a shader.
struct AtStagedShaderComponent {
    enum ModulateOp { kOpSelectTexture = 0, kOpSelectConstant = 1, kOpSelectDiffuse = 2, kOpTextureByConstant = 3, kOpTextureByDiffuse = 4, kOpTextureByDiffuseAndConstant = 5 };
    enum AlphaSource { kFromTexture = 0, kFromConstant = 1, kFromDiffuse = 2, kFromModulatedOutput = 3 };

    AtStagedShaderComponent();                 // 0x00044EE0
    ~AtStagedShaderComponent();                // 0x00037550
    void Clear();                              // 0x00044DE0 leaves the combine op, as the original does
    // 0x00044F10. index is passed and not used.
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, unsigned int index, const char* ns);
    void ReadModulateOp(AtIoTokenizer* tokenizer);   // 0x00044E00 SelectModulatedOutput is not one
    void ReadAlphaSource(AtIoTokenizer* tokenizer);  // 0x00044E80

    AtShaderCombineOp m_combine;               // +0x00
    AtTextureStage m_stage;                    // +0x04
    unsigned short m_switches;                 // +0x18 AtShaderSwitch
    unsigned char m_modulate;                  // +0x1A
    unsigned char m_alphaSource;               // +0x1B
};

// One stage of a shader that runs programs. None of the disc's text cases uses it.
struct AtProgShaderComponent {
    enum { kMaxStages = 8 };

    AtProgShaderComponent();                   // 0x00045030
    ~AtProgShaderComponent();                  // 0x00037530
    void Clear();                              // 0x00045370 leaves the combine op and the stages
    void Read(AtMaterialOwner* owner, AtIoTokenizer* tokenizer, const char* ns);  // 0x00045070
    unsigned int CountTextures() const;        // 0x000453B0 stages with a texture, of all eight

    static void SetHeap(void* heap);           // 0x00045020

    AtShaderCombineOp m_combine;               // +0x00
    unsigned short m_switches;                 // +0x04 AtShaderSwitch
    unsigned char m_stageCount;                // +0x06
    unsigned char m_vertexProcessCount;        // +0x07
    AtTextureStage m_stages[kMaxStages];       // +0x08
    void** m_vertexProcesses;                  // +0xA8
    void* m_pixelProcess;                      // +0xAC

    static void* s_heap;                       // 0x002C7C44
};

// Vftable 0x00274D6C.
class AtShader {
public:
    AtShader();                                // 0x00037560
    virtual ~AtShader();                       // slot 0; 0x000379D0 deleting, 0x00037730 complete
    virtual void Clear();                      // slot 1, 0x000375A0

    // 0x00037740. database is passed and not used. Ends by noting the stages' transforms, of staged components only.
    void Read(AtMaterialOwner* owner, AtShaderDatabase* database, AtIoTokenizer* tokenizer, const char* ns);
    void NoteProgTransforms();                 // 0x00037680 called from the graphics layer
    void NoteStagedTransforms();               // 0x000376F0 called from the graphics layer; Read has the same inline

    static void SetHeap(void* heap);           // 0x00037520

    unsigned int m_progCount;                  // +0x04
    AtProgShaderComponent* m_progs;            // +0x08 a count stands before the first
    unsigned int m_stagedCount;                // +0x0C
    AtStagedShaderComponent* m_stageds;        // +0x10 a count stands before the first
    unsigned int m_transforms[8];              // +0x14 not bounds checked: a ninth goes over m_transformCount, as in the original
    unsigned int m_transformCount;             // +0x34

    static void* s_heap;                       // 0x002C63EC
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtTextureStage, m_flags) == 0x08, "AtTextureStage::m_flags");
static_assert(offsetof(AtTextureStage, m_anisotropy) == 0x10, "AtTextureStage::m_anisotropy");
static_assert(sizeof(AtTextureStage) == 0x14, "AtTextureStage");
static_assert(offsetof(AtStagedShaderComponent, m_stage) == 0x04, "AtStagedShaderComponent::m_stage");
static_assert(offsetof(AtStagedShaderComponent, m_alphaSource) == 0x1B, "AtStagedShaderComponent::m_alphaSource");
static_assert(sizeof(AtStagedShaderComponent) == 0x1C, "AtStagedShaderComponent");
static_assert(offsetof(AtProgShaderComponent, m_stages) == 0x08, "AtProgShaderComponent::m_stages");
static_assert(offsetof(AtProgShaderComponent, m_vertexProcesses) == 0xA8, "AtProgShaderComponent::m_vertexProcesses");
static_assert(sizeof(AtProgShaderComponent) == 0xB0, "AtProgShaderComponent");
static_assert(offsetof(AtShader, m_stageds) == 0x10, "AtShader::m_stageds");
static_assert(offsetof(AtShader, m_transforms) == 0x14, "AtShader::m_transforms");
static_assert(offsetof(AtShader, m_transformCount) == 0x34, "AtShader::m_transformCount");
static_assert(sizeof(AtShader) == 0x38, "AtShader");
#pragma pop_macro("offsetof")
#endif
