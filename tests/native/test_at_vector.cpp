// Holds the build on this host to what the original gives. The hash is of the original's own results, printed by tests/oracle/oracle_vec.cpp at x87 precision 53, over the same inputs, for the functions
// without a sine or cosine. So on x64 this is evidence for those functions that SSE double arithmetic gives the original's bits. The rotations are checked only for what must hold.
#include <cmath>
#include <cstdio>

#include "at/AtVector3.h"
#include "vec_cases.h"

using namespace vec_cases;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// Must be the value oracle_vec prints for this count.
static const int kRounds = 200000;
static const unsigned long long kOriginal = 0x7B61BE23F1D29A03ULL;

static AtVector3 vec(Rng& r) {
    AtVector3 v;
    v.x = pick(r, false);
    v.y = pick(r, false);
    v.z = pick(r, false);
    return v;
}

static unsigned long long Hashed() {
    Rng rng{53u * 77u + 1u};
    Hash hash;
    for (int i = 0; i < kRounds; i++) {
        const AtVector3 a = vec(rng), b = vec(rng), c = vec(rng);
        const float t = pick(rng, false);
        AtVector3 r = a;
        r.Lerp(b, c, t);
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        hash.add(a.DistanceSquared(b));
        hash.add(a.Distance(b));
        r = a;
        r.Normalize();
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        r = c;
        r.SetNormalized(a);
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        r = a;
        const float length = r.NormalizeReturningLength();
        hash.add(r.x); hash.add(r.y); hash.add(r.z); hash.add(length);
        const AtVector3 d = (rng.next() % 8 == 0) ? AtVector3{0, 0, 0} : b;
        r = a;
        r.Project(d);
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        r = c;
        r.SetProjected(a, d);
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        r = a;
        r.Reject(d);
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        r = c;
        r.SetRejected(a, d);
        hash.add(r.x); hash.add(r.y); hash.add(r.z);
        // What the oracle draws next for its rotations, so that the stream stays in step.
        if (rng.next() % 4 != 0) {
            const AtVector3 skipped = vec(rng);
            (void)skipped;
        }
        if (rng.next() % 16 != 0) {
            if (rng.next() % 4 != 0) rng.next();
        }
    }
    return hash.h;
}

int main() {
    const unsigned long long h = Hashed();
    std::printf("hash %016llX\n", h);
    CHECK(h == kOriginal);

    AtVector3 v = {1, 0, 0};
    const AtVector3 z = {0, 0, 1};
    // These two turn the opposite way to RotateZ, as the original does.
    v.Rotate(z, 1.5707963267948966f);
    CHECK(std::fabs(v.x) < 1e-6f && std::fabs(v.y + 1) < 1e-6f && v.z == 0);
    AtVector3 w;
    const AtVector3 from = {1, 0, 0};
    w.SetRotated(z, 1.5707963267948966f, from);
    CHECK(std::fabs(w.x) < 1e-6f && std::fabs(w.y + 1) < 1e-6f && w.z == 0);
    w = from;
    w.RotateZ(1.5707963267948966f);
    CHECK(std::fabs(w.x) < 1e-6f && std::fabs(w.y - 1) < 1e-6f);
    std::printf(g_fail ? "FAILED: %d\n" : "ok\n", g_fail);
    return g_fail ? 1 : 0;
}
