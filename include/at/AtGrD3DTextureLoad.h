// What a 2D texture does when it loads, short of the graphics device: the image file it reads and how, the format and size the texture is made in, and how each level's bytes reach the device's memory. See
// docs/worklog/2026-11-19-texload.md. Names are inferred but two phrases of the original's own messages: "pixel map" (0x002774D8, "Pixel map file not found.") and "DDS" (0x0028B034 and others).
//
// Where a texture's pixels come from, in the game: a .DDS file named for the texture, found on the file systems of the I/O context (the texture database pushes its own directory while it reads). Every
// one of the 1,611 distinct .DDS files in the archives is a DirectDraw surface file of the usual layout, compressed DXT1 or DXT3; 56 hold more than one level. A .AGT says DDSSource or PlatformSource for
// 2,297 textures, TGASource for 58, BMPSource for 4, TplSource for 1, and ProgSource for 1; there is no .XPR. Only the DDS reader, the made texture and the device-free work are recovered: the readers for the
// other sources (BMP, TGA, TPL, PNT and TM2, classes constructed at 0x00068B60, 0x00069790 and 0x0011BD20) are not, and choosing one stops.
//
// The device's own calls are the seam: src/at/AtGrD3DTextureSeam.cpp, which a test supplies. They are the Xbox Direct3D library's (0x00205xxx-0x0020Fxxx), not the game's.
#pragma once

#include "at/AtGrD3DTextures.h"

// ---- The seam: the device's calls, src/at/AtGrD3DTextureSeam.cpp or a test's -----------------------------------------------------------------------------------------------------------------------------------

// What the device says of a surface (0x1C bytes): format, kind, usage, size in bytes, multisampling, width and height.
struct AtGrD3DSurfaceDesc {
    unsigned int m_format;             // +0x00 the device's format number, as in the tables of src/at/AtGrD3DTextureLoad.cpp
    unsigned int m_type;               // +0x04
    unsigned int m_usage;              // +0x08
    unsigned int m_size;               // +0x0C
    unsigned int m_multiSample;        // +0x10 0x11 for none; otherwise a horizontal factor in bits 4 to 7 and a vertical in bits 0 to 3
    unsigned int m_width;              // +0x14
    unsigned int m_height;             // +0x18
};

// What locking a surface gives: the bytes from one row to the next, and where the first is.
struct AtGrD3DLockedRect {
    int m_pitch;                       // +0x00
    void* m_bits;                      // +0x04
};

void* AtGrD3DGetBackBuffer(int which);  // 0x00208430 -1 the front buffer, otherwise the back; a reference is taken, which the caller drops with AtGrD3DRelease
// 0x0020B160. Makes a texture: called with a depth of 1 and a kind of 3. usage is 1 for a render target, else 0. Null if it could not.
void* AtGrD3DCreateTexture(unsigned int width, unsigned int height, unsigned int depth, unsigned int levels, unsigned int usage, unsigned int format, unsigned int type);
void* AtGrD3DSurfaceLevel(void* texture, unsigned int level);           // 0x0020B220 a level's surface
void AtGrD3DDescribe(void* surface, AtGrD3DSurfaceDesc* desc);          // 0x0020AAB0
void AtGrD3DLockRect(void* surface, AtGrD3DLockedRect* locked, const void* rect, unsigned int flags);  // 0x0020AAD0 the whole surface when rect is null; flags 0x80 for reading only

// ---- What the owner holds for making textures: its classes are not recovered, only what is read here ---------------------------------------------------------------------------------------------------------

// What AtMaterialOwner::m_textureLimits (+0x34) points to: the sizes the device can make a texture of each depth in, and words of its capabilities. Slots 0 and 5 on are not known.
class AtGrD3DTextureLimits {
public:
    virtual void Slot0() = 0;
    virtual unsigned int MinWidth(unsigned int bits) = 0;   // slot 1
    virtual unsigned int MaxWidth(unsigned int bits) = 0;   // slot 2
    virtual unsigned int MinHeight(unsigned int bits) = 0;  // slot 3
    virtual unsigned int MaxHeight(unsigned int bits) = 0;  // slot 4

    // The capability word at +0x18, less what the word at +0x68 takes away: bit 2 there clears 0x01000000, bit 3 clears 0x02000000. Of the rest, 0x10000 lets a texture with a transparent colour keep an
    // indexed format, and 0x100000 lets a texture be other than square.
    // Written out where it is read in the original (0x0011CA81, 0x0011CB61); a helper here, in src/at/AtGrD3DTextureLoad.cpp.
    unsigned int Caps() const;

    char m_unknown04[0x14];
    unsigned int m_caps;               // +0x18
    char m_unknown1C[0x68 - 0x1C];
    unsigned int m_capsMask;           // +0x68
};

// What AtMaterialOwner::FormatChooser (slot 16) returns: turns the format a texture asks for into one the device makes, in place. The other slots are not known.
class AtGrD3DFormatChooser {
public:
    virtual void Slot0() = 0;
    virtual void Slot1() = 0;
    virtual void Slot2() = 0;
    virtual void Slot3() = 0;
    virtual void Slot4() = 0;
    virtual void Slot5() = 0;
    virtual void Slot6() = 0;
    virtual void Slot7() = 0;
    virtual void Slot8() = 0;
    virtual void ChooseRenderTarget(const AtPixelFormat* asked, AtPixelFormat* chosen) = 0;  // slot 9
    virtual void Slot10() = 0;
    virtual void Slot11() = 0;
    virtual void ChooseTexture(const AtPixelFormat* asked, AtPixelFormat* chosen) = 0;       // slot 12
};

// ---- Formats and making the texture -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// The device's format numbers and what each is. Two tables: swizzled formats, then linear and compressed ones.
struct AtGrD3DFormatEntry {
    unsigned int m_format;             // +0x00
    unsigned int m_flags;              // +0x04 as AtPixelFormat's
    unsigned int m_bits;               // +0x08
    unsigned int m_mask[4];            // +0x0C
};
extern const AtGrD3DFormatEntry g_atGrSwizzledFormats[28];  // 0x0028A930
extern const AtGrD3DFormatEntry g_atGrLinearFormats[26];    // 0x0028AC40

// 0x001197D0. What a device format number is. A swizzled format keeps AtPixelFormat::kSwizz only if swizzled is not 0. 0, and the format cleared, for a number in neither table.
int AtGrD3DFormatFromDevice(unsigned int format, AtPixelFormat* out, int swizzled);
// 0x00119A60. 0 when the number is a linear or compressed format, 1 otherwise.
int AtGrD3DIsNotLinear(unsigned int format);
// 0x00119B90. The device's number for a format, -1 if none: a swizzled format must match an entry whole; a linear one matches whole, or failing that by its bits alone.
unsigned int AtGrD3DFormatToDevice(const AtPixelFormat* format);

// 0x0011C9F0. The format a texture is made in: swizzled unless it is to change often or is compressed, given an alpha channel if it has a transparent colour and none, then as the owner's chooser says.
void AtGrD3DChooseFormat(AtMaterialOwner* owner, unsigned int usage, int transparent, AtPixelFormat* format);
// 0x0011CAF0. Each side up to a power of two, square unless the device allows otherwise, within the device's limits for the format's depth.
void AtGrD3DFitSize(AtMaterialOwner* owner, unsigned int usage, unsigned int* width, unsigned int* height, const AtPixelFormat* format);
// 0x0011CD60. The device's texture; a failure stops with "Atlas Graphics: create mip map failed".
void AtGrD3DMakeTexture(AtMaterialOwner* owner, unsigned int usage, unsigned int levels, unsigned int width, unsigned int height, unsigned int format, void** texture);
// 0x0011CDD0. Chooses the format and size, fits the levels to them (one level unless swizzled or compressed, no smaller than the device makes), and makes the texture. *levels is what was made.
void AtGrD3DCreateTexture2D(AtMaterialOwner* owner, unsigned int usage, int transparent, unsigned int* levels, unsigned int width, unsigned int height, const AtPixelFormat* format, void** texture);
// 0x0011CFF0. Locks a surface whole. flags bit 0 asks for reading only.
void AtGrD3DLockSurface(void* surface, unsigned int flags, void** bits, int* pitch);

// ---- Image files ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Vftable 0x0027748C. An image file being read: its size and format, and the file. 0x540 bytes; a reader of one kind of file adds to it. "Pixel map" is the original's phrase.
class AtGrIPixelMap {
public:
    AtGrIPixelMap();                       // 0x00065C40
    virtual void ReadHeader() = 0;         // slot 0: the file is open
    virtual ~AtGrIPixelMap();              // slot 1; 0x000660E0 deleting, 0x00065F10 complete
    virtual int Open(AtIoFileSystem* fs, const char* path);  // slot 2, 0x00065D40 freed first if it holds a format; 0, and the header read, if the file opened
    virtual void Parse(const unsigned char* header) = 0;     // slot 3
    virtual void Free();                   // slot 4, 0x00065D80 the format, the file, the sizes; the palette's count
    virtual void Slot5(unsigned int a, unsigned int b, unsigned int c) = 0;  // not known; three arguments
    virtual unsigned int FileSize() = 0;   // slot 6
    virtual void ReadFile(void* buffer) = 0;  // slot 7 the whole file
    virtual const char* Extension() = 0;   // slot 8 added to a name with no dot

    void Fail(const char* message);        // 0x00065B30 "name: message", which stops
    void FailIo(int status);               // 0x00065C20 the status's meaning
    int TryLoad(const char* name);         // 0x00065C70 1 if no file system has the file; else what Open returns
    void Load(const char* name);           // 0x00065F80 a file that cannot be found stops: "name: Unable to find name pixel map file."

    static void SetHeap(void* heap);       // 0x00065B00

    char m_name[0x10];                     // +0x04 only ever emptied here
    unsigned int m_width;                  // +0x14
    unsigned int m_height;                 // +0x18
    unsigned int m_pitch;                  // +0x1C bytes to a row
    AtPixelFormat m_format;                // +0x20
    AtIoFileRef m_file;                    // +0x34
    unsigned int m_paletteCount;           // +0x3C
    unsigned int m_palette[0x100];         // +0x40
    unsigned char m_remap[0x100];          // +0x440 the last two bytes at +0x53E and +0x53F; filled by 0x00065DB0, not recovered

    static void* s_heap;                   // 0x002CB090 for names in messages, and the DDS reader's file
};

// Vftable 0x0028AFF0. A .DDS file: a DirectDraw surface's header of 0x80 bytes, then each level's bytes. 0x5DC bytes.
class AtGrDdsPixelMap : public AtGrIPixelMap {
public:
    enum Kind { kPlain = 0, kVolume = 1, kCube = 2 };

    AtGrDdsPixelMap();                     // 0x0011B2C0
    ~AtGrDdsPixelMap() override;           // slot 1; 0x0011BD50 deleting; the complete one is 0x000EEFA0, a jump to the base's
    void ReadHeader() override;            // slot 0, 0x0011B970
    void Parse(const unsigned char* header) override;  // slot 3, 0x0011B310 bad magic or a bad header stops; the sizes, format and levels
    void Free() override;                  // slot 4, 0x0011B910 the file's bytes, the base's, then all of its own
    void Slot5(unsigned int a, unsigned int b, unsigned int c) override;  // 0x001ACD70 empty, folded
    unsigned int FileSize() override;      // slot 6, 0x0011B250
    void ReadFile(void* buffer) override;  // slot 7, 0x0011B260 the header kept, then the rest
    const char* Extension() override;      // slot 8, 0x0011B2B0 "Dds"

    // 0x0011B9C0. A level of the file, and a face of a cube, into a surface. The file is read whole the first time. Unless the surface is a render target, the bytes are copied in, row by row if the
    // device's rows are longer, when the surface has the file's format and the level's size; otherwise a compressed format stops, and the conversion of others is not recovered. owner is not used here.
    void CopyLevel(AtMaterialOwner* owner, AtGrD3DSurface2D* surface, unsigned int level, unsigned int face);

    unsigned int m_fileSize;               // +0x540
    unsigned char* m_buffer;               // +0x544 the whole file, from the base's heap
    const unsigned char* m_data;           // +0x548 the first level's first byte
    unsigned int m_depth;                  // +0x54C
    unsigned int m_levelSize;              // +0x550 the first level's bytes
    unsigned int m_levels;                 // +0x554 at least one
    unsigned int m_kind;                   // +0x558
    unsigned char m_header[0x80];          // +0x55C

    static AtGrDdsPixelMap s_map;          // 0x003B13E8 the one every DDS texture loads through; constructed at 0x00201FA0, destroyed at 0x00203F00
};

// 0x00119A90. A DDS file's pixel format (from its +0x4C: size, flags, four characters, bits and four masks) as AtPixelFormat. A four-character code other than DXT1 to DXT5 stops, as does a format neither
// RGB nor luminance.
void AtGrDdsFormat(const unsigned int* ddsFormat, AtPixelFormat* out);

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
#pragma push_macro("offsetof")
#undef offsetof
#define offsetof(t, m) __builtin_offsetof(t, m)
static_assert(sizeof(AtGrD3DSurfaceDesc) == 0x1C, "AtGrD3DSurfaceDesc");
static_assert(offsetof(AtGrD3DTextureLimits, m_caps) == 0x18, "AtGrD3DTextureLimits::m_caps");
static_assert(offsetof(AtGrD3DTextureLimits, m_capsMask) == 0x68, "AtGrD3DTextureLimits::m_capsMask");
static_assert(sizeof(AtGrD3DFormatEntry) == 0x1C, "AtGrD3DFormatEntry");
static_assert(offsetof(AtGrIPixelMap, m_width) == 0x14, "AtGrIPixelMap::m_width");
static_assert(offsetof(AtGrIPixelMap, m_format) == 0x20, "AtGrIPixelMap::m_format");
static_assert(offsetof(AtGrIPixelMap, m_file) == 0x34, "AtGrIPixelMap::m_file");
static_assert(offsetof(AtGrIPixelMap, m_paletteCount) == 0x3C, "AtGrIPixelMap::m_paletteCount");
static_assert(sizeof(AtGrIPixelMap) == 0x540, "AtGrIPixelMap");
static_assert(offsetof(AtGrDdsPixelMap, m_levels) == 0x554, "AtGrDdsPixelMap::m_levels");
static_assert(offsetof(AtGrDdsPixelMap, m_header) == 0x55C, "AtGrDdsPixelMap::m_header");
static_assert(sizeof(AtGrDdsPixelMap) == 0x5DC, "AtGrDdsPixelMap");
#pragma pop_macro("offsetof")
#endif
