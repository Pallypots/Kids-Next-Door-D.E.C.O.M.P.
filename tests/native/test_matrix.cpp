// Holds the build on this host to a hash of the original's own results, printed by tests/oracle/oracle_matrix.cpp over the same inputs, for the matrix functions that compute; and checks by value what
// only moves.
#include <cstdio>
#include <cstring>

#include "at/AtCoordSys.h"
#include "matrix_cases.h"

using namespace matrix_cases;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static const int kRounds = 200000;
static const unsigned long long kOriginal = 0xE38718B84F213177ULL;  // Printed by oracle_matrix from the original, not from this build.

static void add(Hash& h, const AtMatrix& m) { for (float f : m.m) h.add(f); }
static void add(Hash& h, const AtVector3& v) { h.add(v.x); h.add(v.y); h.add(v.z); }

// The same inputs, in the same order, as oracle_matrix's first round, including what it draws in the middle of a round; and the same results hashed.
static unsigned long long Hashed() {
    Rng rng{4242};
    Hash hash;
    for (int i = 0; i < kRounds; i++) {
        AtMatrix a, b, start, r;
        matrix(rng, &a, 0);
        matrix(rng, &b, 0);
        fill(rng, &start, 0);
        AtVector3 v, w, vs;
        vector(rng, &v, 0);
        vector(rng, &w, 0);
        vector(rng, &vs, 0);
        AtQuaternion q;
        quaternion(rng, &q, 0);
        const float s = value(rng, 0);
        r = start; g_atMatrix.SetRotation(&r, &q); add(hash, r);
        r = start; g_atMatrix.SetLookAt(&r, &v, &w); add(hash, r);
        {
            AtVector3 f, across;
            f.SetNormalized(v);
            const float k = from_bits(0x3F800000u + ((rng.next() % 16) << 23));
            across.x = f.y * k;
            across.y = -f.x * k;
            across.z = w.z;
            r = start; g_atMatrix.SetLookAt(&r, &v, &across); add(hash, r);
        }
        r = start; g_atMatrix.SetInverse(&r, &a); add(hash, r);
        r = a; g_atMatrix.SetInverse(&r, &r); add(hash, r);
        r = start; AtMatrixSetProductWords(&r, &a, &b); add(hash, r);
        r = a; g_atMatrix.Multiply(&r, &b); add(hash, r);
        r = a; g_atMatrix.Multiply(&r, &r); add(hash, r);
        r = start; g_atMatrix.SetProduct(&r, &a, &b); add(hash, r);
        r = a; g_atMatrix.SetProduct(&r, &r, &b); add(hash, r);
        r = b; g_atMatrix.SetProduct(&r, &a, &r); add(hash, r);
        AtVector3 t = vs; g_atMatrix.TransformPoint(&a, &v, &t); add(hash, t);
        t = v; g_atMatrix.TransformPoint(&a, &t, &t); add(hash, t);
        t = vs; g_atMatrix.TransformDirection(&a, &v, &t); add(hash, t);
        t = v; g_atMatrix.TransformDirection(&a, &t, &t); add(hash, t);
        r = start; g_atMatrix.MakeRotation(&r, &q); add(hash, r);
        r = start; g_atMatrix.SetLookAtZY(&r, &v, &w); add(hash, r);
        r = start; g_atMatrix.SetLookAtXY(&r, &v, &w); add(hash, r);
        r = a; g_atMatrix.ScaleAxes(&r, &v); add(hash, r);
        r = a; g_atMatrix.TranslateAfter(&r, &v); add(hash, r);
        r = a; g_atMatrix.ScaleColumnX(&r, s); add(hash, r);
        r = a; g_atMatrix.ScaleColumnY(&r, s); add(hash, r);
        r = a; g_atMatrix.Invert(&r); add(hash, r);
        (void)rng.next();  // The element's row and column.
        (void)rng.next();
        r = a; g_atMatrix.MultiplyRight(&r, &b); add(hash, r);
        r = a; g_atMatrix.MultiplyRight(&r, &r); add(hash, r);
        const double z = g_atMatrix.TransformPointZ(&a, &v);
        hash.add((float)z);
        hash.add((float)(z - (double)(float)z));
        t = vs; g_atMatrix.InverseTransformPoint(&a, &v, &t); add(hash, t);
        t = v; g_atMatrix.InverseTransformPoint(&a, &t, &t); add(hash, t);
        t = vs; g_atMatrix.InverseTransformDirection(&a, &v, &t); add(hash, t);
        t = v; g_atMatrix.InverseTransformDirection(&a, &t, &t); add(hash, t);
    }
    return hash.h;
}

static bool Bits(const AtMatrix& m, const uint32_t (&want)[16]) {
    for (int i = 0; i < 16; i++) if (to_bits(m.m[i]) != want[i]) return false;
    return true;
}

int main() {
    const unsigned long long h = Hashed();
    std::printf("hash of this build's results, %d rounds: %016llX\n", kRounds, h);
    CHECK(h == kOriginal);

    AtMatrix m;
    for (int i = 0; i < 16; i++) m.m[i] = from_bits(0x7F800001u + i);  // Signalling NaNs: moved, they stay as they are.
    AtMatrix c;
    g_atMatrix.Copy(&c, &m);
    CHECK(std::memcmp(&c, &m, sizeof m) == 0);
    AtMatrixCopyWords(&c, &m);
    CHECK(std::memcmp(&c, &m, sizeof m) == 0);
    g_atMatrix.Identity(&c);
    static const uint32_t kIdentity[16] = {0x3F800000, 0, 0, 0, 0, 0x3F800000, 0, 0, 0, 0, 0x3F800000, 0, 0, 0, 0, 0x3F800000};
    CHECK(Bits(c, kIdentity));
    g_atMatrix.SetUniformScale(&c, from_bits(0x7F800001));
    CHECK(to_bits(c.m[0]) == 0x7FC00001);  // Quieted, as the x87 load did.
    CHECK(to_bits(c.m[5]) == 0x7F800001 && to_bits(c.m[10]) == 0x7F800001);
    const AtVector3 s = {from_bits(0x7F800002), 2.0f, -0.0f};
    g_atMatrix.SetScale(&c, &s);
    static const uint32_t kScale[16] = {0x7F800002, 0, 0, 0, 0, 0x40000000, 0, 0, 0, 0, 0x80000000, 0, 0, 0, 0, 0x3F800000};
    CHECK(Bits(c, kScale));
    g_atMatrix.SetTranslation(&c, &s);
    CHECK(to_bits(c.m[12]) == 0x7F800002 && c.m[13] == 2.0f && to_bits(c.m[14]) == 0x80000000 && c.m[15] == 1.0f);
    AtVector3 p, x, y, z;
    g_atMatrix.GetPosition(&m, &p);
    CHECK(to_bits(p.x) == 0x7F80000D && to_bits(p.z) == 0x7F80000F);
    g_atMatrix.GetAxes(&m, &x, &y, &z);
    CHECK(to_bits(x.x) == 0x7F800001 && to_bits(y.y) == 0x7F800006 && to_bits(z.z) == 0x7F80000B);
    g_atMatrix.SetElement(&c, 2, 3, from_bits(0x7F800003));
    CHECK(to_bits(c.m[11]) == 0x7FC00003);  // Through a register.
    CHECK(to_bits(g_atMatrix.GetElement(&m, 3, 1)) == 0x7F80000E || to_bits(g_atMatrix.GetElement(&m, 3, 1)) == 0x7FC0000E);  // A host may quiet a returned float, as the original's st(0) does.
    c = m;
    g_atMatrix.Transpose(&c);
    CHECK(to_bits(c.m[1]) == 0x7F800005 && to_bits(c.m[4]) == 0x7F800002 && to_bits(c.m[14]) == 0x7F80000C && to_bits(c.m[15]) == 0x7F800010);
    c = m;
    g_atMatrix.Invert(&c);
    CHECK(to_bits(c.m[4]) == 0x7FC00002 && to_bits(c.m[8]) == 0x7FC00003 && to_bits(c.m[1]) == 0x7F800005 && to_bits(c.m[3]) == 0x7F800004);  // 1 and 2 through registers, quieted; the rest as words.
    std::printf("%s\n", g_fail ? "FAILED" : "passed");
    return g_fail ? 1 : 0;
}
