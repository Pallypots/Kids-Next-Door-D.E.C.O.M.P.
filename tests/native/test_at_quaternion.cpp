// Holds the build on this host to a hash of the original's own results, printed by tests/oracle/oracle_qua.cpp over the same inputs, for what uses no sine, cosine or arc cosine.
// The rest is checked only for what must hold.
#include <cmath>
#include <cstdio>

#include "at/AtQuaternion.h"
#include "qua_cases.h"

using namespace vec_cases;
using namespace qua_cases;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static const int kRounds = 200000;
static const unsigned long long kOriginal = 0xA021B09021D51782ULL;

static void add(Hash& h, const AtQuaternion& q) { h.add(q.x); h.add(q.y); h.add(q.z); h.add(q.w); }
static void add(Hash& h, const AtVector3& v) { h.add(v.x); h.add(v.y); h.add(v.z); }

static unsigned long long Hashed() {
    Rng rng{4242};
    Hash hash;
    for (int i = 0; i < kRounds; i++) {
        const AtQuaternion a = (rng.next() % 2) ? near_unit(rng) : any(rng, false);
        const AtQuaternion b = (rng.next() % 2) ? near_unit(rng) : any(rng, false);
        const AtQuaternion start = any(rng, false);
        const float t = (rng.next() % 2) ? (float)((rng.next() & 0xFFFF) / 65536.0) : pick(rng, false);
        AtQuaternion r = start;
        r.SetProduct(a, b);
        add(hash, r);
        r = start;
        r.SetLerp(a, b, t);
        add(hash, r);
        r = start;
        r.SetReflection(a, b);
        add(hash, r);
        r = a;
        r.Multiply(b);
        add(hash, r);
        r = (rng.next() % 16 == 0) ? AtQuaternion{0, 0, 0, 0} : a;
        r.Normalize();
        add(hash, r);
        AtVector3 v = any3(rng, false);
        a.Rotate(&v);
        add(hash, v);
        AtVector3 ra, rb, rc;
        rows_of(near_unit(rng), &ra, &rb, &rc);
        r = start;
        r.SetFromRows(ra, rb, rc);
        add(hash, r);
        const AtVector3 fa = any3(rng, false), fb = any3(rng, false), fc = any3(rng, false);
        r = start;
        r.SetFromRows(fa, fb, fc);
        add(hash, r);
        const AtVector3 p = any3(rng, false), q = any3(rng, false);
        v.SetCross(p, q);
        add(hash, v);
        v.SetDifference(p, q);
        add(hash, v);
        // What the oracle draws next, so that the stream stays in step.
        if (rng.next() % 4 != 0) {}
        any3(rng, false);
        if (rng.next() % 2) rng.next();
    }
    return hash.h;
}

int main() {
    const unsigned long long h = Hashed();
    std::printf("hash %016llX\n", h);
    CHECK(h == kOriginal);

    AtVector3 axis = {0, 0, 1};
    AtQuaternion q;
    q.SetAxisAngle(axis, 1.5707963267948966f);
    CHECK(std::fabs(q.w - 0.70710678f) < 1e-6f && std::fabs(q.z - 0.70710678f) < 1e-6f);
    AtQuaternion from = {0, 0, 0, 1}, s;
    s.SetSlerp(from, q, 0.5f);
    CHECK(std::fabs(s.w - 0.92387953f) < 1e-5f && std::fabs(s.z - 0.38268343f) < 1e-5f);
    s.SetSlerp(from, from, 0.5f);  // Small angle: the lerp.
    CHECK(s.w == 1.0f && s.x == 0.0f);
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
