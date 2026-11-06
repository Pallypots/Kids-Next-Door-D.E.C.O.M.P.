// Differential oracle for the quaternion and two further vector functions (Windows x86, local only). See docs/verification.md.
//
//   oracle_qua <default.xbe> [--count N] [--entries FILE] [--mutate]
//
// By bits at x87 precision 53, the main thread's. NaNs differing only in sign count as alike and are counted apart. Also prints a hash of the original's results for what uses no sine, cosine or
// arc cosine, which tests/native/test_at_quaternion.cpp holds the x64 build to.
#include <float.h>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "at/AtQuaternion.h"
#include "oracle_image.h"
#include "qua_cases.h"

using namespace vec_cases;
using namespace qua_cases;

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static const char* g_op = "";
static int g_nan_sign;
static std::map<std::string, int> g_by_op;

static bool alike(float a, float b) {
    if (to_bits(a) == to_bits(b)) return true;
    if (a != a && b != b) {
        g_nan_sign++;
        return true;
    }
    return false;
}

static void same(const void* o, const void* r, int count, const void* in1 = nullptr, const void* in2 = nullptr) {
    const float* const a = (const float*)o;
    const float* const b = (const float*)r;
    g_checks += (unsigned)count;
    bool ok = true;
    for (int i = 0; i < count; i++) ok = alike(a[i], b[i]) && ok;
    if (ok) return;
    g_by_op[g_op]++;
    if (g_failures++ < 16) {
        std::printf("DIFF %s: original", g_op);
        for (int i = 0; i < count; i++) std::printf(" %08X", to_bits(a[i]));
        std::printf(", recovered");
        for (int i = 0; i < count; i++) std::printf(" %08X", to_bits(b[i]));
        if (in1) { const float* v = (const float*)in1; std::printf(" in %08X %08X %08X %08X", to_bits(v[0]), to_bits(v[1]), to_bits(v[2]), to_bits(v[3])); }
        if (in2) { const float* v = (const float*)in2; std::printf(" and %08X %08X %08X %08X", to_bits(v[0]), to_bits(v[1]), to_bits(v[2]), to_bits(v[3])); }
        std::printf("\n");
    }
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

typedef void(__fastcall* FnV0)(void*, void*);
typedef void(__fastcall* FnV1)(void*, void*, const void*);
typedef void(__fastcall* FnV2)(void*, void*, const void*, const void*);
typedef void(__fastcall* FnV3)(void*, void*, const void*, const void*, const void*);
typedef void(__fastcall* FnLerp)(void*, void*, const void*, const void*, float);
typedef void(__fastcall* FnAxis)(void*, void*, const void*, float);

static void add(Hash& h, const AtQuaternion& q) { h.add(q.x); h.add(q.y); h.add(q.z); h.add(q.w); }
static void add(Hash& h, const AtVector3& v) { h.add(v.x); h.add(v.y); h.add(v.z); }

static void from_rows(const AtVector3& a, const AtVector3& b, const AtVector3& c, const AtQuaternion& start, Hash* hash) {
    AtQuaternion o = start, r = start;
    g_op = "from rows";
    at<FnV3>(0x000336F0)(&o, nullptr, &a, &b, &c);
    r.SetFromRows(a, b, c);
    same(&o, &r, 4);
    if (hash) add(*hash, o);
}

static void round_of(unsigned seed, int count, bool specials, Hash* hash) {
    Rng rng{seed};
    for (int i = 0; i < count; i++) {
        const AtQuaternion a = (rng.next() % 2) ? near_unit(rng) : any(rng, specials);
        const AtQuaternion b = (rng.next() % 2) ? near_unit(rng) : any(rng, specials);
        const AtQuaternion start = any(rng, false);
        const float t = (rng.next() % 2) ? (float)((rng.next() & 0xFFFF) / 65536.0) : pick(rng, specials);
        AtQuaternion o = start, r = start;
        g_op = "product";
        at<FnV2>(0x00033570)(&o, nullptr, &a, &b);
        r.SetProduct(a, b);
        same(&o, &r, 4, &a, &b);
        if (hash) add(*hash, o);
        g_op = "lerp";
        o = start; r = start;
        at<FnLerp>(0x00033600)(&o, nullptr, &a, &b, t);
        r.SetLerp(a, b, t);
        same(&o, &r, 4, &a, &b);
        if (hash) add(*hash, o);
        g_op = "reflection";
        o = start; r = start;
        at<FnV2>(0x00033870)(&o, nullptr, &a, &b);
        r.SetReflection(a, b);
        same(&o, &r, 4, &a, &b);
        if (hash) add(*hash, o);
        g_op = "multiply";
        o = a; r = a;
        at<FnV1>(0x000338C0)(&o, nullptr, &b);
        r.Multiply(b);
        same(&o, &r, 4, &a, &b);
        if (hash) add(*hash, o);
        g_op = "normalize";
        o = (rng.next() % 16 == 0) ? AtQuaternion{0, 0, 0, 0} : a;
        r = o;
        const AtQuaternion before = o;
        at<FnV0>(0x00033950)(&o, nullptr);
        r.Normalize();
        same(&o, &r, 4, &before);
        if (hash) add(*hash, o);
        AtVector3 v = any3(rng, specials), vo = v, vr = v;
        g_op = "rotate vector";
        at<FnV1>(0x00033B00)((void*)&a, nullptr, &vo);
        a.Rotate(&vr);
        same(&vo, &vr, 3, &a);
        if (hash) add(*hash, vo);
        // Rows of a rotation, every branch in turn; and any rows at all.
        AtVector3 ra, rb, rc;
        rows_of(near_unit(rng), &ra, &rb, &rc);
        from_rows(ra, rb, rc, start, hash);
        const AtVector3 fa = any3(rng, specials), fb = any3(rng, specials), fc = any3(rng, specials);
        from_rows(fa, fb, fc, start, hash);
        const AtVector3 p = any3(rng, specials), q = any3(rng, specials);
        g_op = "cross";
        at<FnV2>(0x00033C00)(&vo, nullptr, &p, &q);
        vr.SetCross(p, q);
        same(&vo, &vr, 3);
        if (hash) add(*hash, vo);
        g_op = "difference";
        at<FnV2>(0x00033C40)(&vo, nullptr, &p, &q);
        vr.SetDifference(p, q);
        same(&vo, &vr, 3);
        if (hash) add(*hash, vo);

        // With a sine: not hashed.
        g_op = "slerp";
        o = start; r = start;
        const AtQuaternion c = (rng.next() % 4 == 0) ? a : b;
        at<FnLerp>(0x000339D0)(&o, nullptr, &a, &c, t);
        r.SetSlerp(a, c, t);
        same(&o, &r, 4, &a, &c);
        g_op = "axis angle";
        o = start; r = start;
        const AtVector3 axis = any3(rng, specials);
        const float angle = (rng.next() % 2) ? (float)((double)(int32_t)rng.next() / 2147483648.0 * 7.0) : t;
        at<FnAxis>(0x00033B90)(&o, nullptr, &axis, angle);
        r.SetAxisAngle(axis, angle);
        same(&o, &r, 4, &axis);
    }
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
    if (pos.empty()) { std::printf("usage: oracle_qua <default.xbe> [--count N] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    if (*(uint32_t*)(uintptr_t)0x002719C8 != 0x3C23D70A || *(uint32_t*)(uintptr_t)0x0027463C != 0x3F000000) { std::printf("constants are not as read\n"); return 2; }
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.
    unsigned int control;
    _controlfp_s(&control, _PC_53, _MCW_PC);
    Hash hash;
    round_of(4242, count, false, &hash);
    std::printf("hash of the original's results, %d rounds: %016llX\n", count, hash.h);
    round_of(777, count / 4, true, nullptr);
    std::printf("NaNs differing only in sign: %d\n", g_nan_sign);
    for (const auto& k : g_by_op) std::printf("  %s: %d\n", k.first.c_str(), k.second);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
