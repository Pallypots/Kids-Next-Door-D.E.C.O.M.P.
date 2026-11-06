// What a texture's pixels are: flags, bits per pixel, and a mask for each of four channels. Original address cluster 0x000423F0-0x00042D75.
// The flag names are the original's own, from the table at 0x002B2228. "Pixel format" and the method names are inferred. The channels are named by their order in the byte form Pack and Unpack use.
#pragma once

class AtIoTokenizer;

class AtPixelFormat {
public:
    enum Flags {
        kRgba = 0x1, kLum = 0x2, kIndex = 0x4, kBump = 0x8, kDepth = 0x10, kFloat = 0x20,
        kDxt1 = 0x40, kDxt2 = 0x80, kDxt3 = 0x100, kDxt4 = 0x200, kDxt5 = 0x400,
        kSwizz = 0x800, kTiled = 0x1000, kLumd = 0x2000
    };

    AtPixelFormat();                     // 0x00042C90 all zero
    void Clear();                        // 0x00042D60
    void Copy(const AtPixelFormat& other);  // 0x000423F0
    void Set(unsigned short flags, unsigned short bits, unsigned int mask0, unsigned int mask1, unsigned int mask2, unsigned int mask3);  // 0x00042420
    // Each of these sets every field. "swizzled" adds kSwizz.
    void SetRgba(unsigned short bits, unsigned int mask0, unsigned int mask1, unsigned int mask2, unsigned int mask3, int swizzled);  // 0x00042450
    void SetIndex(unsigned short bits, unsigned int mask0, unsigned int mask3, int swizzled);   // 0x00042490
    void SetLum(unsigned short bits, unsigned int mask0, unsigned int mask3, int swizzled);     // 0x000424D0
    void SetDepth(unsigned short bits, unsigned int mask0, unsigned int mask1, int swizzled);   // 0x00042510
    void SetDxt1(int swizzled);          // 0x00042550 4 bits
    void SetDxt2(int swizzled);          // 0x00042580 8 bits
    void SetDxt3(int swizzled);          // 0x000425B0 8 bits
    void SetDxt4(int swizzled);          // 0x000425E0 8 bits
    void SetDxt5(int swizzled);          // 0x00042610 8 bits

    // The run of set bits from the lowest set bit; bits above a gap are not counted.
    unsigned int Bits0() const;          // 0x000426A0
    unsigned int Bits1() const;          // 0x000426D0
    unsigned int Bits2() const;          // 0x00042640
    unsigned int Bits3() const;          // 0x00042670
    // The position of the lowest set bit, 32 for an empty mask.
    unsigned int Shift0() const;         // 0x00042700
    unsigned int Shift1() const;         // 0x00042720
    unsigned int Shift2() const;         // 0x00042740
    unsigned int Shift3() const;         // 0x00042760

    unsigned int Pack(const unsigned char* bytes) const;           // 0x00042780 four bytes to a pixel, truncating
    void Unpack(unsigned int pixel, unsigned char* bytes) const;   // 0x00042920 a pixel to four bytes; low bits stay 0
    int Equals(const AtPixelFormat& other) const;                  // 0x00042AC0
    int SameLayout(const AtPixelFormat& other) const;              // 0x00042B10 flags, bits, and each channel's width; not where they lie
    int AtLeast(const AtPixelFormat& other) const;                 // 0x00042BD0 flags alike, nothing narrower
    void Read(AtIoTokenizer* tokenizer);                           // 0x00042CB0 flags are set from a number, or ORed in from names, inverted: see the source

    unsigned short m_flags;  // +0x00
    unsigned short m_bits;   // +0x02
    unsigned int m_mask0;    // +0x04
    unsigned int m_mask1;    // +0x08
    unsigned int m_mask2;    // +0x0C
    unsigned int m_mask3;    // +0x10
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(AtPixelFormat) == 0x14, "AtPixelFormat");
#endif
