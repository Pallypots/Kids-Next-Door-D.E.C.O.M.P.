// Differential oracle for the vector math (Windows x86, local only). See docs/verification.md.
//
//   oracle_vec <default.xbe> [--count N] [--entries FILE] [--mutate]
//
// Compares bits at x87 precision of 53 bits, which is what recovered source computes in on any host with SSE arithmetic, and what the C library's start-up sets on the game's main thread: docs/portability.md.
// Differences at 24 and 64 bits are counted and printed: a measurement of what would be at stake on a thread at another setting, not a failure. Two NaNs of different sign count as alike, and are counted apart: where only that differs is the order of
// operands, not a value. Signalling NaNs are not generated. Also prints a hash of the original's results for the functions without a sine or cosine, on finite
// inputs at 53 bits, which tests/native/test_at_vector.cpp holds the x64 build to; and measures how far the host library's sine and cosine stand from fsin and fcos.
#include <float.h>
#include <math.h>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtVector3.h"
#include "oracle_image.h"
#include "vec_cases.h"

using namespace vec_cases;

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static const char* g_op = "";
static unsigned g_mode;

#include <map>
static std::map<std::string, int> g_by_op;

static int g_nan_sign;
static int g_other_precision;

static bool alike(float a, float b) {
    const uint32_t x = to_bits(a), y = to_bits(b);
    if (x == y) return true;
    if (a != a && b != b) {
        g_nan_sign++;
        return true;
    }
    return false;
}

static void same(const AtVector3& o, const AtVector3& r, const AtVector3* in1 = nullptr, const AtVector3* in2 = nullptr, float t = 0) {
    g_checks += 3;
    if (alike(o.x, r.x) && alike(o.y, r.y) && alike(o.z, r.z)) return;
    g_by_op[std::string(g_op) + " pc " + std::to_string(g_mode)]++;
    if (g_mode != 53) {
        g_other_precision++;
        return;
    }
    if (g_failures++ < 12) {
        std::printf("DIFF %s pc %u: original %08X %08X %08X, recovered %08X %08X %08X", g_op, g_mode, to_bits(o.x), to_bits(o.y), to_bits(o.z), to_bits(r.x), to_bits(r.y), to_bits(r.z));
        if (in1) std::printf(" in %08X %08X %08X", to_bits(in1->x), to_bits(in1->y), to_bits(in1->z));
        if (in2) std::printf(" and %08X %08X %08X", to_bits(in2->x), to_bits(in2->y), to_bits(in2->z));
        std::printf(" t %08X\n", to_bits(t));
    }
}
static void same_f(float o, float r) {
    g_checks++;
    if (alike(o, r)) return;
    g_by_op[std::string(g_op) + " pc " + std::to_string(g_mode)]++;
    if (g_mode != 53) {
        g_other_precision++;
        return;
    }
    if (g_failures++ < 12) std::printf("DIFF %s pc %u: returned original %08X, recovered %08X\n", g_op, g_mode, to_bits(o), to_bits(r));
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

typedef void(__fastcall* FnLerp)(void*, void*, const void*, const void*, float);
typedef float(__fastcall* FnF1)(const void*, void*, const void*);
typedef void(__fastcall* FnV0)(void*, void*);
typedef void(__fastcall* FnV1)(void*, void*, const void*);
typedef void(__fastcall* FnV2)(void*, void*, const void*, const void*);
typedef float(__fastcall* FnF0)(void*, void*);
typedef void(__fastcall* FnRot)(void*, void*, const void*, float);
typedef void(__fastcall* FnSetRot)(void*, void*, const void*, float, const void*);
typedef void(__fastcall* FnRotZ)(void*, void*, float);
typedef void(__fastcall* FnRead)(void*, void*, void*);

// Reals handed out in order, on each side.
static std::vector<float> g_reals;
static size_t g_next_real;
static float __fastcall o_expect_real(void*, void*) { return g_reals[g_next_real++]; }

class RealsOnly : public AtIoTokenizer {
public:
    int Next() override {
        m_real = g_reals[g_next_real++];
        return kReal;
    }
    void PutInt(int) override {}
    void PutReal(float) override {}
    void PutString(const char*) override {}
    void PutComment(const char*) override {}
    void PutKeyword(int) override {}
    void PutLeftBracket() override {}
    void PutRightBracket() override {}
    void PutLeftCurly() override {}
    void PutRightCurly() override {}
    void PutComma() override {}
    void PutSemicolon() override {}
    void PutColon() override {}
    void PutExclamation() override {}
    void PutPercent() override {}
    void PutCaret() override {}
    void PutAmpersand() override {}
    void PutTimes() override {}
    void PutLeftParen() override {}
    void PutRightParen() override {}
    void PutMinus() override {}
    void PutPlus() override {}
    void PutDivide() override {}
    void PutEquals() override {}
    void PutLess() override {}
    void PutGreater() override {}
    void PutOr() override {}
    void PutQuestion() override {}
    void PutAt() override {}
    void PutTilde() override {}
    void PutNewline() override {}
    void PutSpace() override {}
    void PutSpaces(unsigned int) override {}
    void PutTab() override {}
    void PutTabs(unsigned int) override {}
    void PutName(const char*) override {}
    void PutRaw(const char*) override {}
};

static AtVector3 vec(Rng& r, bool specials) {
    AtVector3 v;
    v.x = pick(r, specials);
    v.y = pick(r, specials);
    v.z = pick(r, specials);
    return v;
}

// A unit axis, as callers pass; the tests with any vector as axis are separate.
static AtVector3 unit(Rng& r) {
    AtVector3 v = vec(r, false);
    const double l = sqrt((double)v.x * v.x + (double)v.y * v.y + (double)v.z * v.z);
    if (!(l > 0) || l > 1e30 || l < 1e-30) { v.x = 0; v.y = 1; v.z = 0; return v; }
    v.x = (float)(v.x / l);
    v.y = (float)(v.y / l);
    v.z = (float)(v.z / l);
    return v;
}

static void round_of(unsigned seed, int count, bool specials, Hash* hash) {
    Rng rng{seed};
    for (int i = 0; i < count; i++) {
        const AtVector3 a = vec(rng, specials), b = vec(rng, specials), c = vec(rng, specials);
        const float t = pick(rng, specials);
        AtVector3 o = a, r = a;
        g_op = "lerp";
        at<FnLerp>(0x00042D80)(&o, nullptr, &b, &c, t);
        r.Lerp(b, c, t);
        same(o, r, &b, &c, t);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }
        g_op = "distance squared";
        const float d2o = at<FnF1>(0x00042DC0)(&a, nullptr, &b), d2r = a.DistanceSquared(b);
        same_f(d2o, d2r);
        if (hash) hash->add(d2o);
        g_op = "distance";
        const float d1o = at<FnF1>(0x00042F60)(&a, nullptr, &b), d1r = a.Distance(b);
        same_f(d1o, d1r);
        if (hash) hash->add(d1o);
        g_op = "normalize";
        o = a; r = a;
        at<FnV0>(0x00042E20)(&o, nullptr);
        r.Normalize();
        same(o, r, &a);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }
        g_op = "set normalized";
        o = c; r = c;
        at<FnV1>(0x00042E80)(&o, nullptr, &a);
        r.SetNormalized(a);
        same(o, r, &a);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }
        g_op = "normalize with length";
        o = a; r = a;
        const float lo = at<FnF0>(0x00042EF0)(&o, nullptr), lr = r.NormalizeReturningLength();
        same(o, r, &a);
        same_f(lo, lr);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); hash->add(lo); }
        const AtVector3 d = (rng.next() % 8 == 0) ? AtVector3{0, 0, 0} : b;
        g_op = "project";
        o = a; r = a;
        at<FnV1>(0x000431E0)(&o, nullptr, &d);
        r.Project(d);
        same(o, r, &a, &d);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }
        g_op = "set projected";
        o = c; r = c;
        at<FnV2>(0x00043250)(&o, nullptr, &a, &d);
        r.SetProjected(a, d);
        same(o, r, &a, &d);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }
        g_op = "reject";
        o = a; r = a;
        at<FnV1>(0x000432D0)(&o, nullptr, &d);
        r.Reject(d);
        same(o, r, &a, &d);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }
        g_op = "set rejected";
        o = c; r = c;
        at<FnV2>(0x00043370)(&o, nullptr, &a, &d);
        r.SetRejected(a, d);
        same(o, r, &a, &d);
        if (hash) { hash->add(o.x); hash->add(o.y); hash->add(o.z); }

        // Rotations. Not hashed: the sine and cosine are the host's on x64.
        const AtVector3 axis = (rng.next() % 4 == 0) ? b : unit(rng);
        const float angle = (rng.next() % 16 == 0) ? 0.0f : (rng.next() % 4 == 0) ? t : (float)((double)(int32_t)rng.next() / 2147483648.0 * 7.0);
        g_op = "rotate";
        o = a; r = a;
        at<FnRot>(0x00042F90)(&o, nullptr, &axis, angle);
        r.Rotate(axis, angle);
        same(o, r, &a, &axis, angle);
        g_op = "set rotated";
        o = c; r = c;
        at<FnSetRot>(0x000430A0)(&o, nullptr, &axis, angle, &a);
        r.SetRotated(axis, angle, a);
        same(o, r, &a, &axis, angle);
        g_op = "rotate z";
        o = a; r = a;
        at<FnRotZ>(0x000431B0)(&o, nullptr, angle);
        r.RotateZ(angle);
        same(o, r, &a, nullptr, angle);
    }
}

// Sums whose order shows. A sum's order changes a double by its last bit, which reaches a float only when the result is next to a midpoint between two floats: for random inputs, about once in
// 2^29. These are built to land there: one large term, one about 2^-12 of it, whose square is about 2^-24 of the first's and so lands on a float midpoint, and one about 2^-26 of it to move in the double's last bits.
static float scaled(Rng& r, float large, int power, unsigned spreadBits) {
    // large * 2^power * (1 + a little), the little being of at most spreadBits, with a random sign.
    const double little = (double)(r.next() & ((1u << spreadBits) - 1)) / 8388608.0;
    const double v = large * ldexp(1.0 + little, power);
    return (float)((r.next() & 1) ? v : -v);
}

static AtVector3 near_midpoint(Rng& r) {
    const float large = (r.next() % 2) ? 1.0f : (float)ldexp(1.0 + (double)(r.next() & 0x3F) / 8388608.0, (int)(r.next() % 9) * 2 - 8);
    const float parts[3] = {(r.next() & 1) ? large : -large, scaled(r, large, -12, 1 + r.next() % 3), scaled(r, large, -26 + (int)(r.next() % 3), 8 + r.next() % 16)};
    const unsigned first = r.next() % 3, step = 1 + r.next() % 2;
    AtVector3 v;
    float* const out[3] = {&v.x, &v.y, &v.z};
    for (unsigned i = 0; i < 3; i++) *out[(first + i * step) % 3] = parts[i];
    return v;
}

// A direction with a length squared near a power of 2, and with all three terms: so a dot product over it stays near what it was.
static AtVector3 near_axes(Rng& r) {
    static const float kSmall[] = {1.0f / 4096, 1.0f / 8192, 1.0f / 16384, 1.0f / 32768};
    AtVector3 d;
    float* const out[3] = {&d.x, &d.y, &d.z};
    const unsigned a = r.next() % 3;
    *out[a] = 1.0f;
    *out[(a + 1) % 3] = (r.next() & 1) ? 1.0f : 0.0f;
    *out[(a + 2) % 3] = kSmall[r.next() % 4];
    return d;
}

static void round_of_midpoints(unsigned seed, int count) {
    Rng rng{seed};
    const AtVector3 zero = {0, 0, 0};
    for (int i = 0; i < count; i++) {
        const AtVector3 a = near_midpoint(rng);
        AtVector3 o = a, r = a;
        g_op = "distance squared, midpoint";
        same_f(at<FnF1>(0x00042DC0)(&zero, nullptr, &a), zero.DistanceSquared(a));
        g_op = "distance, midpoint";
        same_f(at<FnF1>(0x00042F60)(&zero, nullptr, &a), zero.Distance(a));
        g_op = "normalize, midpoint";
        at<FnV0>(0x00042E20)(&o, nullptr);
        r.Normalize();
        same(o, r, &a);
        const AtVector3 d = near_axes(rng);
        g_op = "project, midpoint";
        o = a; r = a;
        at<FnV1>(0x000431E0)(&o, nullptr, &d);
        r.Project(d);
        same(o, r, &a, &d);
        g_op = "set projected, midpoint";
        at<FnV2>(0x00043250)(&o, nullptr, &a, &d);
        r.SetProjected(a, d);
        same(o, r, &a, &d);
        // A small angle over a vector of this kind: the terms of a rotation are then of these same sizes.
        const float angle = (float)ldexp(1.0 + (double)(rng.next() & 0xFFFF) / 65536.0, -(int)(rng.next() % 12) - 1);
        const AtVector3 axis = unit(rng);
        g_op = "rotate, midpoint";
        o = a; r = a;
        at<FnRot>(0x00042F90)(&o, nullptr, &axis, angle);
        r.Rotate(axis, angle);
        same(o, r, &a, &axis, angle);
        g_op = "set rotated, midpoint";
        at<FnSetRot>(0x000430A0)(&o, nullptr, &axis, angle, &a);
        r.SetRotated(axis, angle, a);
        same(o, r, &a, &axis, angle);
    }
}

static void read_round() {
    RealsOnly tok;
    g_reals = {1.5f, -2.25f, 1e10f, 7.0f, 8.0f, 9.0f};
    AtVector3 o = {0, 0, 0}, r = {0, 0, 0};
    g_op = "read";
    for (int i = 0; i < 2; i++) {
        const size_t first = (size_t)i * 3;
        g_next_real = first;
        at<FnRead>(0x00042DF0)(&o, nullptr, &tok);
        const size_t taken = g_next_real - first;
        g_next_real = first;
        r.Read(&tok);
        g_checks++;
        if (taken != g_next_real - first && g_failures++ < 30) std::printf("DIFF read took %u, recovered %u\n", (unsigned)taken, (unsigned)(g_next_real - first));
        same(o, r);
    }
}

// How far the host library's sine and cosine stand from the x87's, seen in single precision, which is what a rotation ends in. Measurement, not a pass or fail.
static void measure_trig() {
    Rng rng{1234};
    const int n = 2000000;
    int sinDiff = 0, cosDiff = 0, sinBeyond = 0;
    double worst = 0;
    for (int i = 0; i < n; i++) {
        const float a = (float)((double)(int32_t)rng.next() / 2147483648.0 * 7.0);
        const double fs = AtSin(a), fc = AtCos(a), ls = sin((double)a), lc = cos((double)a);
        if ((float)fs != (float)ls) sinDiff++;
        if ((float)fc != (float)lc) cosDiff++;
        const double e = fabs(fs - ls);
        if (e > worst) worst = e;
    }
    // fsin leaves a value of 2^63 or more as it is.
    if (AtSin(1e19f) == (double)1e19f) sinBeyond = 1;
    std::printf("trig: of %d angles within +-7, sine differs from the library's as a float in %d, cosine in %d, largest double difference %.3g; fsin returns its argument at 1e19: %s\n", n, sinDiff, cosDiff, worst, sinBeyond ? "yes" : "no");
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    int count = 200000;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--count") && i + 1 < argc) count = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.empty()) { std::printf("usage: oracle_vec <default.xbe> [--count N] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    if (*(uint32_t*)(uintptr_t)0x00272AC8 != 0x36A7C5AC || *(uint32_t*)(uintptr_t)0x00272AC4 != 0x3F800000 || *(uint32_t*)(uintptr_t)0x002725F0 != 0) { std::printf("constants are not as read\n"); return 2; }
    redirect(0x000158A0, (const void*)o_expect_real);
    if (entries && !arm_entries(entries)) return 2;
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.

    unsigned int control;
    for (unsigned pc : {53u, 24u, 64u}) {
        g_mode = pc;
        _controlfp_s(&control, pc == 53 ? _PC_53 : pc == 24 ? _PC_24 : _PC_64, _MCW_PC);
        Hash hash;
        round_of(pc * 77u + 1u, count, false, pc == 53 ? &hash : nullptr);
        round_of(pc * 91u + 1u, count / 4, true, nullptr);
        read_round();
        if (pc == 53) round_of_midpoints(4321u, count * 5);
        if (pc == 53) std::printf("hash of the original's results, 53 bits, %d rounds: %016llX\n", count, hash.h);
    }
    _controlfp_s(&control, _PC_53, _MCW_PC);
    measure_trig();
    std::printf("NaNs differing only in sign: %d. Differences at other precisions, which do not fail: %d\n", g_nan_sign, g_other_precision);
    for (const auto& k : g_by_op) std::printf("  %s: %d\n", k.first.c_str(), k.second);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
