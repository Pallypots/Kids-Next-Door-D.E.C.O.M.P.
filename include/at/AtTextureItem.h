// What every texture is, before the graphics layer's classes add to it: its name, how it is used, where its pixels come from, and the reader of the body of a Texture2D, Texture3D or CubeMapTexture in an .AGT file.
// Original address clusters 0x00030FF0-0x00031100, 0x00031120-0x000314C4 and 0x00031530. Words of the format are the original's own, in the table at 0x002B0230; all else is inferred.
//
// The base has eleven virtual slots, vftable 0x00274A04. The concrete classes (vftables 0x00289700, 0x0028B88C and 0x0028B8DC) are the graphics layer's and are not recovered.
#pragma once

#include "at/AtPixelFormat.h"

class AtIoTokenizer;
struct AtMaterialOwner;
class AtTextureDatabase;
class AtVector3;

// What the word ProgSource gives: a texture that is made rather than read, as in "ProgSource 128 128 32 1 16711680 65280 255 -16777216". 0x1C bytes from the token file's heap.
struct AtProgTextureSource {
    int m_width;              // +0x00 inferred from the order and values on the one real file
    int m_height;             // +0x04
    AtPixelFormat m_format;   // +0x08
};

class AtTextureBase {
public:
    // In m_usage. The source is a field of three bits, set whole by each word.
    enum Usage {
        kRenderTarget = 0x001, kDepthStencil = 0x002, kFrequentModify = 0x010, kInfrequentModify = 0x020, kFlipVertically = 0x040, kMpegMacroBlocks = 0x100,
        kSourceMask = 0xE00,
        kSourceBmp = 0x000, kSourceTga = 0x200, kSourceDds = 0x400, kSourceTm2 = 0x600, kSourceTpl = 0x800, kSourcePnt = 0xA00, kSourceProg = 0xC00
    };
    // In m_flags. PlatformSource is DDSSource with kPlatformSource as well. kPending is set by a concrete class, only cleared here.
    enum Flags {
        kAutoGenMipLevels = 0x004, kTransparentColor = 0x008, kDiffuseLightMap = 0x010, kPlatformSource = 0x040,
        kFrontBufferSource = 0x100, kBackBufferSource = 0x200, kDemandLoad = 0x400, kPending = 0x800
    };

    AtTextureBase();                          // 0x00031070
    virtual ~AtTextureBase();                 // slot 0; 0x00031530 deleting, 0x00031290 complete. Both as Reset: neither frees the ProgSource.
    // Slot 1, 0x00030FF0: as a file would, with no file. usage replaces m_usage; flags are only added to. Levels below one are one.
    virtual void Describe(AtTextureDatabase* database, const char* name, unsigned int usage, int autoGenMipLevels, unsigned int levels, int textureStage);
    // Slot 2, 0x000312D0. A left curly through its right curly. name is the eight bytes given in the file, not always terminated. A ProgSource read earlier is lost, not freed.
    virtual void Read(AtTextureDatabase* database, const char* name, AtIoTokenizer* tokenizer);
    virtual void ClearPending();              // slot 3, 0x00031100
    virtual void Reset();                     // slot 4, 0x000310C0 as new, but for the bytes of the name after the first; does not free the ProgSource
    virtual void SetTransparentColor(const unsigned char* rgba);  // slot 5, 0x00031260
    virtual void ClearTransparentColor();     // slot 6, 0x00031050 only the flag
    virtual void SetLightDirection(const AtVector3* direction);  // slot 7, 0x000311F0 normalized, each axis to a byte from -1 at 0 to 1 at 255
    virtual int Unknown8();                   // slot 8, 0x000AB720 0, folded; the concrete classes have their own
    virtual int Unknown9();                   // slot 9, 0x000AB720 as above
    virtual void ClearModifyUsage(int unused);  // slot 10, 0x00031060 not a render target, and neither kind of modify

    void FreeProgSource();                    // 0x00031040 the concrete 2D texture's business, when it has loaded: the pointer stays
    void ReadProgSource(AtIoTokenizer* tokenizer);  // 0x00031120

    char m_name[8];                 // +0x04
    unsigned char m_transparent[4]; // +0x0C red, green, blue, alpha; the word TransparentColor gives the first three
    unsigned char m_lightDirection[4];  // +0x10 x, y, z, and the fourth at 0x7F always
    unsigned int m_usage;           // +0x14
    unsigned short m_flags;         // +0x18
    char m_unknown1A[2];
    unsigned char m_textureStage;   // +0x1C
    unsigned char m_levels;         // +0x1D the word MipMap's value, and 1 at the start of Read; 0 from new, which is what marks an item unused
    unsigned char m_levelsAsked;    // +0x1E always the same as m_levels here; the concrete classes size their surfaces by this
    unsigned char m_unknown1F;      // +0x1F 0 from new; no use here
    AtProgTextureSource* m_progSource;  // +0x20
    AtTextureDatabase* m_database;  // +0x24
};

// The same object as the database sees it. These two slots are found only in the concrete classes, and which class of the original first declares them is not known.
class AtTextureItem : public AtTextureBase {
public:
    virtual void Slot11() = 0;
    virtual void OnLoaded(AtMaterialOwner* owner) = 0;  // slot 12, 0x000FCAD0 in the concrete 2D texture
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(offsetof(AtTextureBase, m_name) == 0x04, "AtTextureBase::m_name");
static_assert(offsetof(AtTextureBase, m_usage) == 0x14, "AtTextureBase::m_usage");
static_assert(offsetof(AtTextureBase, m_flags) == 0x18, "AtTextureBase::m_flags");
static_assert(offsetof(AtTextureBase, m_levels) == 0x1D, "AtTextureBase::m_levels");
static_assert(offsetof(AtTextureBase, m_progSource) == 0x20, "AtTextureBase::m_progSource");
static_assert(sizeof(AtTextureBase) == 0x28, "AtTextureBase");
static_assert(sizeof(AtTextureItem) == 0x28, "AtTextureItem");
static_assert(sizeof(AtProgTextureSource) == 0x1C, "AtProgTextureSource");
#pragma pop_macro("offsetof")
#endif
