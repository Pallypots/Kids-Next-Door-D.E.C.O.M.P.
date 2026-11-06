// Inputs for the vector math, the same on every host, and a hash of results: so the x64 build can be held to what the original gives. Used by oracle_vec.cpp and tests/native/test_at_vector.cpp.
#pragma once
#include <cstdint>
#include <cstring>

namespace vec_cases {

// A small generator of our own: std::mt19937's distributions are not the same across libraries, and this needs only raw words.
struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

inline float from_bits(uint32_t b) {
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
inline uint32_t to_bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

// Finite values, of the kinds that tell code apart. withSpecials adds infinities and NaNs, which are host-dependent in their NaN's bits and so left out of what is hashed.
inline float pick(Rng& r, bool withSpecials) {
    static const uint32_t kEdge[] = {0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x36A7C5AC, 0x36A7C5AB, 0x36A7C5AD, 0x358637BD, 0x00000001, 0x80000001, 0x00800000, 0x7F7FFFFF, 0xFF7FFFFF,
                                     0x3F000000, 0x40490FDB, 0x3FC90FDB, 0x1E3CE508, 0x5F000000, 0x2F800000, 0x3F7FFFFF};
    static const uint32_t kSpecial[] = {0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00000};
    const uint32_t k = r.next() % 16;
    if (withSpecials && k == 0) return from_bits(kSpecial[r.next() % 4]);
    if (k < 3) return from_bits(kEdge[r.next() % (sizeof kEdge / sizeof kEdge[0])]);
    if (k < 8) return (float)((double)(int32_t)r.next() / 2147483648.0);              // -1 to 1
    if (k < 13) return (float)((double)(int32_t)r.next() / 2147483648.0 * 1000.0);    // -1000 to 1000
    for (;;) {
        const uint32_t b = r.next();
        if ((b & 0x7F800000) != 0x7F800000) return from_bits(b);                      // Any finite bits.
    }
}

struct Hash {
    uint64_t h = 0xCBF29CE484222325ULL;
    void add(float f) {
        uint32_t b = to_bits(f);
        // Every NaN alike: which one results is not the same between x87 and SSE.
        if ((b & 0x7F800000) == 0x7F800000 && (b & 0x007FFFFF) != 0) b = 0x7FC00000;
        for (int i = 0; i < 4; i++) {
            h ^= (b >> (i * 8)) & 0xFF;
            h *= 0x100000001B3ULL;
        }
    }
};

}  // namespace vec_cases
