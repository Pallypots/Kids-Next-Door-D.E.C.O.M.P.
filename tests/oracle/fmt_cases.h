// Formats for the pixel format tests, the same on every host. Used by oracle_fmt.cpp and tests/native/test_at_pixel_format.cpp.
#pragma once
#include <cstdint>

#include "at/AtPixelFormat.h"
#include "vec_cases.h"

namespace fmt_cases {

using vec_cases::Rng;

inline uint32_t mask(Rng& r) {
    static const uint32_t kKnown[] = {0, 0xFF, 0xFF00, 0xFF0000, 0xFF000000, 0xF800, 0x7E0, 0x1F, 0x7C00, 0x3E0, 0x8000, 0xF00, 0xF0, 0xF, 0xF000, 0xFFFF, 0xFFFF0000, 0xFFFFFF, 0xFFFFFFFF, 1, 0x80000000, 0x101, 0xF0F0, 0x80000001};
    const uint32_t k = r.next() % 8;
    if (k < 4) return kKnown[r.next() % (sizeof kKnown / sizeof kKnown[0])];
    if (k < 7) {
        const uint32_t width = 1 + r.next() % 12, at = r.next() % 32;
        return (uint32_t)((((uint64_t)1 << width) - 1) << at);
    }
    return r.next();
}

inline AtPixelFormat format(Rng& r) {
    AtPixelFormat f;
    f.m_flags = (r.next() % 4 == 0) ? (unsigned short)r.next() : (unsigned short)(1u << (r.next() % 14));
    f.m_bits = (r.next() % 4 == 0) ? (unsigned short)r.next() : (unsigned short)(4u * (1 + r.next() % 8));
    f.m_mask0 = mask(r);
    f.m_mask1 = mask(r);
    f.m_mask2 = mask(r);
    f.m_mask3 = mask(r);
    return f;
}

// Another, with something in common: widths the same or changed by one, positions moved, one field different.
inline AtPixelFormat related(Rng& r, const AtPixelFormat& f) {
    AtPixelFormat g = f;
    switch (r.next() % 8) {
    case 0: break;
    case 1: g.m_flags ^= (unsigned short)(1u << (r.next() % 16)); break;
    case 2: g.m_bits = (unsigned short)(g.m_bits + (int)(r.next() % 3) - 1); break;
    case 3: g.m_mask0 <<= 1; break;
    case 4: g.m_mask1 >>= 1; break;
    case 5: g.m_mask2 = (g.m_mask2 << 1) | 1; break;
    case 6: g.m_mask3 = g.m_mask3 >> (r.next() % 3); break;
    default: g.m_mask0 = 0; break;
    }
    return g;
}

}  // namespace fmt_cases
