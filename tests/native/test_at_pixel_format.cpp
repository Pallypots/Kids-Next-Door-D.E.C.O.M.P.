// Holds the build on this host to a hash of the original's own results, printed by tests/oracle/oracle_fmt.cpp over the same inputs.
#include <cstdio>
#include <cstring>

#include "at/AtPixelFormat.h"
#include "fmt_cases.h"

using namespace vec_cases;
using namespace fmt_cases;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static const int kRounds = 300000;
static const unsigned long long kOriginal = 0xA24FA9E5908E6053ULL;

int main() {
    Rng rng{1234};
    Hash hash;
    for (int i = 0; i < kRounds; i++) {
        const AtPixelFormat f = format(rng);
        const AtPixelFormat g = related(rng, f);
        hash.add(from_bits(f.Bits0()));
        hash.add(from_bits(f.Bits1()));
        hash.add(from_bits(f.Bits2()));
        hash.add(from_bits(f.Bits3()));
        hash.add(from_bits(f.Shift0()));
        hash.add(from_bits(f.Shift1()));
        hash.add(from_bits(f.Shift2()));
        hash.add(from_bits(f.Shift3()));
        unsigned char bytes[4] = {(unsigned char)rng.next(), (unsigned char)rng.next(), (unsigned char)rng.next(), (unsigned char)rng.next()};
        const unsigned packed = f.Pack(bytes);
        hash.add(from_bits(packed));
        const unsigned pixel = (rng.next() % 2) ? packed : rng.next();
        unsigned char out[4];
        f.Unpack(pixel, out);
        uint32_t w;
        std::memcpy(&w, out, 4);
        hash.add(from_bits(w));
        const int e = f.Equals(g), l = f.SameLayout(g), a = f.AtLeast(g), a2 = g.AtLeast(f);
        hash.add(from_bits((uint32_t)(e | l << 8 | a << 16 | a2 << 24)));
    }
    std::printf("hash %016llX\n", hash.h);
    CHECK(hash.h == kOriginal);

    // What a reader can check by eye: 565.
    AtPixelFormat f;
    f.SetRgba(16, 0xF800, 0x7E0, 0x1F, 0, 0);
    const unsigned char white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(f.Pack(white) == 0xFFFF);
    unsigned char back[4];
    f.Unpack(0xFFFF, back);
    CHECK(back[0] == 0xF8 && back[1] == 0xFC && back[2] == 0xF8 && back[3] == 0);
    CHECK(f.Bits0() == 5 && f.Bits1() == 6 && f.Shift0() == 11 && f.Shift3() == 32);
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
