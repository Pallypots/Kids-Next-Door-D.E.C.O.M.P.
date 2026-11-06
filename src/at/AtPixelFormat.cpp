// AtPixelFormat. Original address cluster 0x000423F0-0x00042D75.
// Verification status per function: config/functions.toml, docs/PROGRESS.md.
#include "at/AtPixelFormat.h"

#include "at/AtIoTokenizer.h"

extern "C" int toupper(int c);

namespace {

// The C library's _stricmp, here so as to need no header.
int EqualIgnoringCase(const char* a, const char* b) {
    for (;; a++, b++) {
        const int x = toupper((unsigned char)*a);
        if (x != toupper((unsigned char)*b)) {
            return 0;
        }
        if (x == 0) {
            return 1;
        }
    }
}

// 0x002B2228.
struct FlagName {
    const char* m_name;
    unsigned short m_flag;
};
const FlagName kFlagNames[] = {
    {"RGBA", AtPixelFormat::kRgba},   {"LUM", AtPixelFormat::kLum},     {"INDEX", AtPixelFormat::kIndex}, {"BUMP", AtPixelFormat::kBump},   {"DEPTH", AtPixelFormat::kDepth},
    {"FLOAT", AtPixelFormat::kFloat}, {"DXT1", AtPixelFormat::kDxt1},   {"DXT2", AtPixelFormat::kDxt2},   {"DXT3", AtPixelFormat::kDxt3},   {"DXT4", AtPixelFormat::kDxt4},
    {"DXT5", AtPixelFormat::kDxt5},   {"SWIZZ", AtPixelFormat::kSwizz}, {"TILED", AtPixelFormat::kTiled}, {"LUMD", AtPixelFormat::kLumd},   {0, 0},
};

inline unsigned int RunLength(unsigned int mask) {
    if (mask == 0) {
        return 0;
    }
    while ((mask & 1) == 0) {
        mask >>= 1;
    }
    unsigned int count = 0;
    while ((mask & 1) != 0) {
        mask >>= 1;
        count++;
    }
    return count;
}

inline unsigned int LowestBit(unsigned int mask) {
    if (mask == 0) {
        return 32;
    }
    unsigned int position = 0;
    while ((mask & 1) == 0) {
        mask >>= 1;
        position++;
    }
    return position;
}

// A shift by the x86's rule: only the low five bits of the count. The original reaches counts of 32 and beyond, and an unbounded C++ shift by those is undefined.
inline unsigned int ShiftRight(unsigned int value, unsigned int count) {
    return value >> (count & 31);
}
inline unsigned int ShiftLeft(unsigned int value, unsigned int count) {
    return value << (count & 31);
}

}  // namespace

AtPixelFormat::AtPixelFormat() {
    m_flags = 0;
    m_bits = 0;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
}

void AtPixelFormat::Clear() {
    m_flags = 0;
    m_bits = 0;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
}

void AtPixelFormat::Copy(const AtPixelFormat& other) {
    m_flags = other.m_flags;
    m_bits = other.m_bits;
    m_mask0 = other.m_mask0;
    m_mask1 = other.m_mask1;
    m_mask2 = other.m_mask2;
    m_mask3 = other.m_mask3;
}

void AtPixelFormat::Set(unsigned short flags, unsigned short bits, unsigned int mask0, unsigned int mask1, unsigned int mask2, unsigned int mask3) {
    m_flags = flags;
    m_bits = bits;
    m_mask0 = mask0;
    m_mask1 = mask1;
    m_mask2 = mask2;
    m_mask3 = mask3;
}

void AtPixelFormat::SetRgba(unsigned short bits, unsigned int mask0, unsigned int mask1, unsigned int mask2, unsigned int mask3, int swizzled) {
    m_bits = bits;
    m_mask0 = mask0;
    m_mask1 = mask1;
    m_mask2 = mask2;
    m_mask3 = mask3;
    m_flags = kRgba;
    if (swizzled != 0) {
        m_flags = kRgba | kSwizz;
    }
}

void AtPixelFormat::SetIndex(unsigned short bits, unsigned int mask0, unsigned int mask3, int swizzled) {
    m_bits = bits;
    m_mask0 = mask0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = mask3;
    m_flags = kIndex;
    if (swizzled != 0) {
        m_flags = kIndex | kSwizz;
    }
}

void AtPixelFormat::SetLum(unsigned short bits, unsigned int mask0, unsigned int mask3, int swizzled) {
    m_bits = bits;
    m_mask0 = mask0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = mask3;
    m_flags = kLum;
    if (swizzled != 0) {
        m_flags = kLum | kSwizz;
    }
}

void AtPixelFormat::SetDepth(unsigned short bits, unsigned int mask0, unsigned int mask1, int swizzled) {
    m_bits = bits;
    m_mask0 = mask0;
    m_mask1 = mask1;
    m_mask2 = 0;
    m_mask3 = 0;
    m_flags = kDepth;
    if (swizzled != 0) {
        m_flags = kDepth | kSwizz;
    }
}

void AtPixelFormat::SetDxt1(int swizzled) {
    m_flags = kDxt1;
    m_bits = 4;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
    if (swizzled != 0) {
        m_flags = kDxt1 | kSwizz;
    }
}

void AtPixelFormat::SetDxt2(int swizzled) {
    m_flags = kDxt2;
    m_bits = 8;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
    if (swizzled != 0) {
        m_flags = kDxt2 | kSwizz;
    }
}

void AtPixelFormat::SetDxt3(int swizzled) {
    m_flags = kDxt3;
    m_bits = 8;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
    if (swizzled != 0) {
        m_flags = kDxt3 | kSwizz;
    }
}

void AtPixelFormat::SetDxt4(int swizzled) {
    m_flags = kDxt4;
    m_bits = 8;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
    if (swizzled != 0) {
        m_flags = kDxt4 | kSwizz;
    }
}

void AtPixelFormat::SetDxt5(int swizzled) {
    m_flags = kDxt5;
    m_bits = 8;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
    if (swizzled != 0) {
        m_flags = kDxt5 | kSwizz;
    }
}

unsigned int AtPixelFormat::Bits0() const { return RunLength(m_mask0); }
unsigned int AtPixelFormat::Bits1() const { return RunLength(m_mask1); }
unsigned int AtPixelFormat::Bits2() const { return RunLength(m_mask2); }
unsigned int AtPixelFormat::Bits3() const { return RunLength(m_mask3); }

unsigned int AtPixelFormat::Shift0() const { return LowestBit(m_mask0); }
unsigned int AtPixelFormat::Shift1() const { return LowestBit(m_mask1); }
unsigned int AtPixelFormat::Shift2() const { return LowestBit(m_mask2); }
unsigned int AtPixelFormat::Shift3() const { return LowestBit(m_mask3); }

unsigned int AtPixelFormat::Pack(const unsigned char* bytes) const {
    const unsigned int v0 = ShiftRight(bytes[0], 8 - RunLength(m_mask0));
    const unsigned int v1 = ShiftRight(bytes[1], 8 - RunLength(m_mask1));
    const unsigned int v2 = ShiftRight(bytes[2], 8 - RunLength(m_mask2));
    const unsigned int v3 = ShiftRight(bytes[3], 8 - RunLength(m_mask3));
    return ShiftLeft(v0, LowestBit(m_mask0)) | ShiftLeft(v1, LowestBit(m_mask1)) | ShiftLeft(v2, LowestBit(m_mask2)) | ShiftLeft(v3, LowestBit(m_mask3));
}

void AtPixelFormat::Unpack(unsigned int pixel, unsigned char* bytes) const {
    const unsigned int v0 = ShiftRight(pixel & m_mask0, LowestBit(m_mask0));
    const unsigned int v1 = ShiftRight(pixel & m_mask1, LowestBit(m_mask1));
    const unsigned int v2 = ShiftRight(pixel & m_mask2, LowestBit(m_mask2));
    const unsigned int v3 = ShiftRight(pixel & m_mask3, LowestBit(m_mask3));
    bytes[0] = (unsigned char)ShiftLeft(v0, 8 - RunLength(m_mask0));
    bytes[1] = (unsigned char)ShiftLeft(v1, 8 - RunLength(m_mask1));
    bytes[2] = (unsigned char)ShiftLeft(v2, 8 - RunLength(m_mask2));
    bytes[3] = (unsigned char)ShiftLeft(v3, 8 - RunLength(m_mask3));
}

int AtPixelFormat::Equals(const AtPixelFormat& other) const {
    if (m_flags == other.m_flags && m_bits == other.m_bits && m_mask0 == other.m_mask0 && m_mask1 == other.m_mask1 && m_mask2 == other.m_mask2 && m_mask3 == other.m_mask3) {
        return 1;
    }
    return 0;
}

int AtPixelFormat::SameLayout(const AtPixelFormat& other) const {
    if (m_flags == other.m_flags && m_bits == other.m_bits && Bits0() == other.Bits0() && Bits1() == other.Bits1() && Bits2() == other.Bits2() && Bits3() == other.Bits3()) {
        return 1;
    }
    return 0;
}

int AtPixelFormat::AtLeast(const AtPixelFormat& other) const {
    if (m_flags != other.m_flags || m_bits < other.m_bits) {
        return 0;
    }
    if (Bits0() < other.Bits0()) {
        return 0;
    }
    if (Bits1() < other.Bits1() || Bits2() < other.Bits2() || Bits3() < other.Bits3()) {
        return 0;
    }
    return 1;
}

void AtPixelFormat::Read(AtIoTokenizer* tokenizer) {
    m_bits = (unsigned short)tokenizer->ExpectInt();
    int token = tokenizer->Next();
    if (token == AtIoTokenizer::kInt) {
        m_flags = (unsigned short)tokenizer->m_int;
    } else {
        while (token == AtIoTokenizer::kString) {
            // As in the original: a flag is set when its name is NOT the token's, so "RGBA" sets every flag but kRgba. Probably a slip for a test of equality.
            for (const FlagName* entry = kFlagNames; entry->m_name != 0; entry++) {
                if (!EqualIgnoringCase(tokenizer->m_string, entry->m_name)) {
                    m_flags |= entry->m_flag;
                }
            }
            token = tokenizer->Next();
        }
        tokenizer->m_pushedBack = 1;
    }
    m_mask0 = tokenizer->ExpectInt();
    m_mask1 = tokenizer->ExpectInt();
    m_mask2 = tokenizer->ExpectInt();
    m_mask3 = tokenizer->ExpectInt();
}
