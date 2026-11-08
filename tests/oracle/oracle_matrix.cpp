// Differential oracle for the matrix table (Windows x86, local only). See docs/verification.md and docs/worklog/2026-11-08-matrix.md.
//
//   oracle_matrix <default.xbe> <text cases> [--count N] [--entries FILE] [--mutate]
//
// Runs the original's 0x000EC2B0 to fill a table of its own, and calls through each slot this code names, against the member of g_atMatrix; and the two helpers no slot holds, directly. By bits at x87
// precision 53, the main thread's. Elements only moved, and every element a function leaves alone, are compared bit for bit, NaNs too. Results of arithmetic count NaNs differing in sign or payload as
// alike, counted apart: which NaN comes out of two is not the same between x87 and SSE. Every output starts with the same random contents on both sides, so what a function must not write is compared.
// Also prints a hash of the original's results without signalling NaNs, which tests/native/test_matrix.cpp holds the x64 build to.
#include <float.h>
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"
#include "matrix_cases.h"
#include "oracle_image.h"

using namespace matrix_cases;

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static const char* g_op = "";
static int g_nan_alike;
static std::map<std::string, int> g_by_op;

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

// What the rest of the library needs to link. Nothing here reaches either.
void AtFatal(const char* message, int, int, int) { std::printf("AtFatal: %s\n", message); std::exit(3); }
void AtLibraryFree(void* data) { std::free(data); }

// The original's table, filled by its own 0x000EC2B0.
static uint32_t g_table[104];
static void* slot(uint32_t va) { return (void*)(uintptr_t)g_table[(va - 0x002C60C8) / 4]; }

// exact: moved, not computed. Otherwise any two NaNs are alike.
static void same(const float* a, const float* b, int count, bool exact, const float* exactMask = nullptr) {
    g_checks += (unsigned)count;
    bool ok = true;
    for (int i = 0; i < count; i++) {
        if (to_bits(a[i]) == to_bits(b[i])) continue;
        const bool strict = exact || (exactMask && exactMask[i] != 0);
        if (!strict && a[i] != a[i] && b[i] != b[i]) { g_nan_alike++; continue; }
        ok = false;
    }
    if (ok) return;
    g_by_op[g_op]++;
    if (g_failures++ < 16) {
        std::printf("DIFF %s: original", g_op);
        for (int i = 0; i < count; i++) std::printf(" %08X", to_bits(a[i]));
        std::printf(", recovered");
        for (int i = 0; i < count; i++) std::printf(" %08X", to_bits(b[i]));
        std::printf("\n");
    }
}

struct alignas(16) M { AtMatrix v; };
static void add(Hash& h, const AtMatrix& m) { for (float f : m.m) h.add(f); }
static void add(Hash& h, const AtVector3& v) { h.add(v.x); h.add(v.y); h.add(v.z); }

typedef void(__cdecl* FnMM)(AtMatrix*, const AtMatrix*);
typedef void(__cdecl* FnM)(AtMatrix*);
typedef void(__cdecl* FnMV)(AtMatrix*, const AtVector3*);
typedef void(__cdecl* FnMQ)(AtMatrix*, const AtQuaternion*);
typedef void(__cdecl* FnMVV)(AtMatrix*, const AtVector3*, const AtVector3*);
typedef void(__cdecl* FnMMM)(AtMatrix*, const AtMatrix*, const AtMatrix*);
typedef void(__cdecl* FnPoint)(const AtMatrix*, const AtVector3*, AtVector3*);
typedef void(__cdecl* FnMF)(AtMatrix*, float);
typedef void(__cdecl* FnGet)(const AtMatrix*, AtVector3*);
typedef void(__cdecl* FnAxes)(const AtMatrix*, AtVector3*, AtVector3*, AtVector3*);
typedef void(__cdecl* FnAxesIn)(AtMatrix*, const AtVector3*, const AtVector3*, const AtVector3*);
typedef void(__cdecl* FnMVF)(AtMatrix*, const AtVector3*, float);
typedef void(__cdecl* FnSetElement)(AtMatrix*, int, int, float);
typedef float(__cdecl* FnGetElement)(const AtMatrix*, int, int);
typedef double(__cdecl* FnPointZ)(const AtMatrix*, const AtVector3*);  // The original leaves its sum in st(0); read as a double, all 53 bits.

// What SetInverse moves or sets, compared bit for bit; it computes only the position, 12 to 14.
static const float kInverseMoved[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1};
// What SetRotation and SetLookAt leave as it was: all but the 3x3.
static const float kOutside3x3[16] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1};
// What ScaleAxes leaves: the position row. TranslateAfter: the fourth column. ScaleColumnX and Y: all but their column.
static const float kRow3[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
static const float kColumn3[16] = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
static const float kNotColumn0[16] = {0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1};
static const float kNotColumn1[16] = {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1};

// A double by its bits; any two NaNs alike, counted.
static void same_double(double a, double b) {
    g_checks++;
    uint64_t x, y;
    std::memcpy(&x, &a, 8);
    std::memcpy(&y, &b, 8);
    if (x == y) return;
    if (a != a && b != b) { g_nan_alike++; return; }
    g_by_op[g_op]++;
    if (g_failures++ < 16) std::printf("DIFF %s: original %016llX, recovered %016llX\n", g_op, (unsigned long long)x, (unsigned long long)y);
}

static void round_of(unsigned seed, int count, int kind, Hash* hash) {
    Rng rng{seed};
    for (int i = 0; i < count; i++) {
        M a, b, start, o, r;
        matrix(rng, &a.v, kind);
        matrix(rng, &b.v, kind);
        fill(rng, &start.v, kind);
        AtVector3 v, w, vs;
        vector(rng, &v, kind);
        vector(rng, &w, kind);
        vector(rng, &vs, kind);
        AtQuaternion q;
        quaternion(rng, &q, kind);
        const float s = value(rng, kind);

        g_op = "copy";
        o = start; r = start;
        ((FnMM)slot(0x002C60C8))(&o.v, &a.v);
        g_atMatrix.Copy(&r.v, &a.v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "copy words";
        o = start; r = start;
        at<FnMM>(0x00114FB0)(&o.v, &a.v);
        AtMatrixCopyWords(&r.v, &a.v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "identity";
        o = start; r = start;
        ((FnM)slot(0x002C60CC))(&o.v);
        g_atMatrix.Identity(&r.v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "translation";
        o = start; r = start;
        ((FnMV)slot(0x002C60D4))(&o.v, &v);
        g_atMatrix.SetTranslation(&r.v, &v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "scale";
        o = start; r = start;
        ((FnMV)slot(0x002C6120))(&o.v, &v);
        g_atMatrix.SetScale(&r.v, &v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "uniform scale";
        o = start; r = start;
        ((FnMF)slot(0x002C611C))(&o.v, s);
        g_atMatrix.SetUniformScale(&r.v, s);
        same(o.v.m, r.v.m, 16, true);  // Element 0 is quieted on both sides alike: x87 and SSE keep the payload.
        g_op = "position";
        AtVector3 po = vs, pr = vs;
        ((FnGet)slot(0x002C61F8))(&a.v, &po);
        g_atMatrix.GetPosition(&a.v, &pr);
        same(&po.x, &pr.x, 3, true);
        g_op = "axes";
        AtVector3 ax[3] = {vs, w, v}, ar[3] = {vs, w, v};
        ((FnAxes)slot(0x002C61FC))(&a.v, &ax[0], &ax[1], &ax[2]);
        g_atMatrix.GetAxes(&a.v, &ar[0], &ar[1], &ar[2]);
        same(&ax[0].x, &ar[0].x, 9, true);

        g_op = "rotation";
        o = start; r = start;
        ((FnMQ)slot(0x002C60E0))(&o.v, &q);
        g_atMatrix.SetRotation(&r.v, &q);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        if (hash) add(*hash, o.v);
        g_op = "look at";
        o = start; r = start;
        ((FnMVV)slot(0x002C60F4))(&o.v, &v, &w);
        g_atMatrix.SetLookAt(&r.v, &v, &w);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        if (hash) add(*hash, o.v);
        g_op = "look at, up across";
        {
            AtVector3 f, across;
            f.SetNormalized(v);  // As the original's 0x00042E80 gives it: the vec suite holds the two equal.
            const float k = from_bits(0x3F800000u + ((rng.next() % 16) << 23));
            across.x = f.y * k;
            across.y = -f.x * k;
            across.z = w.z;
            o = start; r = start;
            ((FnMVV)slot(0x002C60F4))(&o.v, &v, &across);
            g_atMatrix.SetLookAt(&r.v, &v, &across);
            same(o.v.m, r.v.m, 16, false, kOutside3x3);
            if (hash) add(*hash, o.v);
        }
        g_op = "inverse";
        o = start; r = start;
        ((FnMM)slot(0x002C61D4))(&o.v, &a.v);
        g_atMatrix.SetInverse(&r.v, &a.v);
        same(o.v.m, r.v.m, 16, false, kInverseMoved);
        if (hash) add(*hash, o.v);
        g_op = "inverse in place";
        o = a; r = a;
        ((FnMM)slot(0x002C61D4))(&o.v, &o.v);
        g_atMatrix.SetInverse(&r.v, &r.v);
        same(o.v.m, r.v.m, 16, false, kInverseMoved);
        if (hash) add(*hash, o.v);
        g_op = "product words";
        o = start; r = start;
        at<FnMMM>(0x001166A0)(&o.v, &a.v, &b.v);
        AtMatrixSetProductWords(&r.v, &a.v, &b.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "multiply";
        o = a; r = a;
        ((FnMM)slot(0x002C6228))(&o.v, &b.v);
        g_atMatrix.Multiply(&r.v, &b.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "multiply by itself";
        o = a; r = a;
        ((FnMM)slot(0x002C6228))(&o.v, &o.v);
        g_atMatrix.Multiply(&r.v, &r.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "product";
        o = start; r = start;
        ((FnMMM)slot(0x002C6234))(&o.v, &a.v, &b.v);
        g_atMatrix.SetProduct(&r.v, &a.v, &b.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "product into first";
        o = a; r = a;
        ((FnMMM)slot(0x002C6234))(&o.v, &o.v, &b.v);
        g_atMatrix.SetProduct(&r.v, &r.v, &b.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "product into second";
        o = b; r = b;
        ((FnMMM)slot(0x002C6234))(&o.v, &a.v, &o.v);
        g_atMatrix.SetProduct(&r.v, &a.v, &r.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "product, rows overlapping";  // to two rows into a: the first two rows written are a's last two, which are read after.
        {
            M pair[2], pr[2];
            pair[0] = a; pair[1] = start; pr[0] = a; pr[1] = start;
            AtMatrix* to_o = (AtMatrix*)&pair[0].v.m[8];
            AtMatrix* to_r = (AtMatrix*)&pr[0].v.m[8];
            ((FnMMM)slot(0x002C6234))(to_o, &pair[0].v, &b.v);
            g_atMatrix.SetProduct(to_r, &pr[0].v, &b.v);
            same(pair[0].v.m, pr[0].v.m, 32, false);
        }
        g_op = "point";
        AtVector3 to = vs, tr = vs;
        ((FnPoint)slot(0x002C6250))(&a.v, &v, &to);
        g_atMatrix.TransformPoint(&a.v, &v, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
        g_op = "point in place";
        to = v; tr = v;
        ((FnPoint)slot(0x002C6250))(&a.v, &to, &to);
        g_atMatrix.TransformPoint(&a.v, &tr, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
        g_op = "direction";
        to = vs; tr = vs;
        ((FnPoint)slot(0x002C625C))(&a.v, &v, &to);
        g_atMatrix.TransformDirection(&a.v, &v, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
        g_op = "direction in place";
        to = v; tr = v;
        ((FnPoint)slot(0x002C625C))(&a.v, &to, &to);
        g_atMatrix.TransformDirection(&a.v, &tr, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);

        // ---- The further slots.
        g_op = "scale 3x3";
        o = start; r = start;
        ((FnMV)slot(0x002C60D8))(&o.v, &v);
        g_atMatrix.SetScale3x3(&r.v, &v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "set axes";
        o = start; r = start;
        ((FnAxesIn)slot(0x002C60E4))(&o.v, &v, &w, &vs);
        g_atMatrix.SetAxes(&r.v, &v, &w, &vs);
        same(o.v.m, r.v.m, 16, true);
        g_op = "make translation";
        o = start; r = start;
        ((FnMV)slot(0x002C6100))(&o.v, &v);
        g_atMatrix.MakeTranslation(&r.v, &v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "make rotation";
        o = start; r = start;
        ((FnMQ)slot(0x002C6108))(&o.v, &q);
        g_atMatrix.MakeRotation(&r.v, &q);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        if (hash) add(*hash, o.v);
        g_op = "make from axes";
        o = start; r = start;
        ((FnAxesIn)slot(0x002C610C))(&o.v, &v, &w, &vs);
        g_atMatrix.MakeFromAxes(&r.v, &v, &w, &vs);
        same(o.v.m, r.v.m, 16, true);
        g_op = "set axis angle";
        o = start; r = start;
        ((FnMVF)slot(0x002C60DC))(&o.v, &v, s);
        g_atMatrix.SetAxisAngle(&r.v, &v, s);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        g_op = "make axis angle";
        o = start; r = start;
        ((FnMVF)slot(0x002C6104))(&o.v, &v, s);
        g_atMatrix.MakeAxisAngle(&r.v, &v, s);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        g_op = "look at zy";
        o = start; r = start;
        ((FnMVV)slot(0x002C60F8))(&o.v, &v, &w);
        g_atMatrix.SetLookAtZY(&r.v, &v, &w);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        if (hash) add(*hash, o.v);
        g_op = "look at xy";
        o = start; r = start;
        ((FnMVV)slot(0x002C60FC))(&o.v, &v, &w);
        g_atMatrix.SetLookAtXY(&r.v, &v, &w);
        same(o.v.m, r.v.m, 16, false, kOutside3x3);
        if (hash) add(*hash, o.v);
        g_op = "scale axes";
        o = a; r = a;
        ((FnMV)slot(0x002C6168))(&o.v, &v);
        g_atMatrix.ScaleAxes(&r.v, &v);
        same(o.v.m, r.v.m, 16, false, kRow3);
        if (hash) add(*hash, o.v);
        g_op = "translate after";
        o = a; r = a;
        ((FnMV)slot(0x002C6184))(&o.v, &v);
        g_atMatrix.TranslateAfter(&r.v, &v);
        same(o.v.m, r.v.m, 16, false, kColumn3);
        if (hash) add(*hash, o.v);
        g_op = "scale column x";
        o = a; r = a;
        ((FnMF)slot(0x002C61A4))(&o.v, s);
        g_atMatrix.ScaleColumnX(&r.v, s);
        same(o.v.m, r.v.m, 16, false, kNotColumn0);
        if (hash) add(*hash, o.v);
        g_op = "scale column y";
        o = a; r = a;
        ((FnMF)slot(0x002C61A8))(&o.v, s);
        g_atMatrix.ScaleColumnY(&r.v, s);
        same(o.v.m, r.v.m, 16, false, kNotColumn1);
        if (hash) add(*hash, o.v);
        g_op = "invert";
        o = a; r = a;
        ((FnM)slot(0x002C61D0))(&o.v);
        g_atMatrix.Invert(&r.v);
        same(o.v.m, r.v.m, 16, false, kInverseMoved);
        if (hash) add(*hash, o.v);
        g_op = "transpose";
        o = a; r = a;
        ((FnM)slot(0x002C61E0))(&o.v);
        g_atMatrix.Transpose(&r.v);
        same(o.v.m, r.v.m, 16, true);
        g_op = "set element";
        {
            const int row = (int)(rng.next() % 4), column = (int)(rng.next() % 4);
            o = start; r = start;
            ((FnSetElement)slot(0x002C61E8))(&o.v, row, column, s);
            g_atMatrix.SetElement(&r.v, row, column, s);
            same(o.v.m, r.v.m, 16, true);
            g_op = "get element";
            const float eo = ((FnGetElement)slot(0x002C61EC))(&a.v, row, column);
            const float er = g_atMatrix.GetElement(&a.v, row, column);
            same(&eo, &er, 1, true);
        }
        g_op = "x axis";
        po = vs; pr = vs;
        ((FnGet)slot(0x002C620C))(&a.v, &po);
        g_atMatrix.GetXAxis(&a.v, &pr);
        same(&po.x, &pr.x, 3, true);
        g_op = "y axis";
        po = vs; pr = vs;
        ((FnGet)slot(0x002C6210))(&a.v, &po);
        g_atMatrix.GetYAxis(&a.v, &pr);
        same(&po.x, &pr.x, 3, true);
        g_op = "z axis";
        po = vs; pr = vs;
        ((FnGet)slot(0x002C6214))(&a.v, &po);
        g_atMatrix.GetZAxis(&a.v, &pr);
        same(&po.x, &pr.x, 3, true);
        g_op = "multiply right";
        o = a; r = a;
        ((FnMM)slot(0x002C622C))(&o.v, &b.v);
        g_atMatrix.MultiplyRight(&r.v, &b.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "multiply right by itself";
        o = a; r = a;
        ((FnMM)slot(0x002C622C))(&o.v, &o.v);
        g_atMatrix.MultiplyRight(&r.v, &r.v);
        same(o.v.m, r.v.m, 16, false);
        if (hash) add(*hash, o.v);
        g_op = "point z";
        {
            const double zo = ((FnPointZ)slot(0x002C6254))(&a.v, &v);
            const double zr = g_atMatrix.TransformPointZ(&a.v, &v);
            same_double(zo, zr);
            if (hash) hash->add((float)zo), hash->add((float)(zo - (double)(float)zo));
        }
        g_op = "inverse point";
        to = vs; tr = vs;
        ((FnPoint)slot(0x002C6258))(&a.v, &v, &to);
        g_atMatrix.InverseTransformPoint(&a.v, &v, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
        g_op = "inverse point in place";
        to = v; tr = v;
        ((FnPoint)slot(0x002C6258))(&a.v, &to, &to);
        g_atMatrix.InverseTransformPoint(&a.v, &tr, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
        g_op = "inverse direction";
        to = vs; tr = vs;
        ((FnPoint)slot(0x002C6260))(&a.v, &v, &to);
        g_atMatrix.InverseTransformDirection(&a.v, &v, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
        g_op = "inverse direction in place";
        to = v; tr = v;
        ((FnPoint)slot(0x002C6260))(&a.v, &to, &to);
        g_atMatrix.InverseTransformDirection(&a.v, &tr, &tr);
        same(&to.x, &tr.x, 3, false);
        if (hash) add(*hash, to);
    }
}


// ---- real .AKC files ------------------------------------------------------------------------------------------------------
// Each file read by the host's AtCoordSysDatabase::Read twice, once with g_atMatrix holding the original's own functions from its filled table and once the recovered ones, then every entry's world matrix
// updated, a point transformed and the inverse updated. Everything but the matrix functions is the same host code on both sides, so this compares those functions on the values real files give them, in the
// order the coordinate systems call them. The device and its pool are this harness's: plain 16-byte aligned matrices.
enum { kFileMats = 8192 };
static M g_file_mats[2][kFileMats];
static M g_file_spare[2];
static int g_file_side;
static int g_file_heap;

class FilePool : public AtMatrixPool {
public:
    unsigned count = 0;
    void Slot0() override {}
    void Reserve(unsigned n) override {
        count = n < kFileMats ? n : kFileMats;
        for (unsigned i = 0; i < count; i++) for (float& f : g_file_mats[g_file_side][i].v.m) f = from_bits(0x7FBADBAD);
    }
    void Clear() override {}
    unsigned Count() override { return count; }
    AtMatrix* At(unsigned i) override { return i < count ? &g_file_mats[g_file_side][i].v : &g_file_spare[g_file_side].v; }
};
static FilePool g_file_pool;

class FileDevice : public AtMatrixDevice {
public:
    AtMatrix* NewMatrix() override { return &g_file_spare[g_file_side].v; }
    AtMatrixPool* NewPool() override { return &g_file_pool; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override {}
    void FreePool(AtMatrixPool*) override {}
};
static FileDevice g_file_device;

class FileStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

static AtMatrixTable original_table() {
    AtMatrixTable t = g_atMatrix;
#define SLOT(member, va) t.member = (decltype(t.member))slot(va)
    SLOT(Copy, 0x002C60C8); SLOT(Identity, 0x002C60CC); SLOT(SetTranslation, 0x002C60D4); SLOT(SetRotation, 0x002C60E0); SLOT(SetLookAt, 0x002C60F4); SLOT(SetScale, 0x002C6120);
    SLOT(SetInverse, 0x002C61D4); SLOT(Multiply, 0x002C6228); SLOT(SetProduct, 0x002C6234); SLOT(TransformPoint, 0x002C6250); SLOT(SetUniformScale, 0x002C611C); SLOT(Read, 0x002C6264);
    SLOT(GetPosition, 0x002C61F8); SLOT(GetAxes, 0x002C61FC); SLOT(TransformDirection, 0x002C625C);
#undef SLOT
    return t;
}

struct FileTotals { unsigned files, entries, matrices, drawing; };

static void run_akc_file(const std::vector<unsigned char>& data, const char* name, FileTotals* totals) {
    g_op = name;
    const AtMatrixTable mine = g_atMatrix;
    const AtMatrixTable theirs = original_table();
    AtVector3 points[2][64 * 1024 / 16];
    unsigned npoints[2] = {0, 0}, counts[2] = {0, 0}, entries = 0, drawing = 0;
    for (int side = 0; side < 2; side++) {
        g_file_side = side;
        g_atMatrix = side == 0 ? theirs : mine;
        std::string text((const char*)data.data(), data.size());
        FileStream stream;
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        AtIoTextTokenizer tokenizer;
        tokenizer.SetStream(&stream);
        {
            AtCoordSysDatabase db;
            db.Read(&g_file_device, &tokenizer);
            counts[side] = g_file_pool.count;
            entries = db.m_rotTransCount + db.m_scaledCount;
            drawing = g_file_pool.count / 2 - entries;
            AtCoordSys* all[2] = {db.m_rotTrans, db.m_scaled};
            const unsigned n[2] = {db.m_rotTransCount, db.m_scaledCount};
            const unsigned size[2] = {sizeof(AtRotTransCoordSys), sizeof(AtScaleCoordSys)};
            for (int kind = 0; kind < 2; kind++) {
                for (unsigned i = 0; i < n[kind]; i++) {
                    AtCoordSys* cs = (AtCoordSys*)((char*)all[kind] + i * size[kind]);
                    cs->UpdateWorld();
                    const AtVector3 p = {1.5f, -2.25f, 0.125f};
                    if (npoints[side] < sizeof points[0] / sizeof points[0][0]) cs->Transform(&p, &points[side][npoints[side]++]);
                }
            }
            for (int kind = 0; kind < 2; kind++) {
                for (unsigned i = 0; i < n[kind]; i++) {
                    AtCoordSys* cs = (AtCoordSys*)((char*)all[kind] + i * size[kind]);
                    cs->UpdateInverse();
                }
            }
            tokenizer.SetStream(nullptr);
            stream.m_base = stream.m_cur = stream.m_limit = nullptr;
            // Compared before the database goes: the pool is the harness's and stays.
        }
    }
    g_atMatrix = mine;
    g_checks++;
    if (counts[0] != counts[1]) { std::printf("DIFF %s: %u matrices against %u\n", name, counts[0], counts[1]); g_failures++; return; }
    for (unsigned i = 0; i < counts[0]; i++) same(g_file_mats[0][i].v.m, g_file_mats[1][i].v.m, 16, false);
    if (npoints[0] == npoints[1]) same(&points[0][0].x, &points[1][0].x, (int)npoints[0] * 3, false);
    totals->files++;
    totals->entries += entries;
    totals->matrices += counts[0];
    totals->drawing += drawing;
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
    if (pos.size() < 2) { std::printf("usage: oracle_matrix <default.xbe> <text cases> [--count N] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    if (*(uint32_t*)(uintptr_t)0x00274B64 != 0x40000000 || *(uint32_t*)(uintptr_t)0x00272AC4 != 0x3F800000 || *(uint32_t*)(uintptr_t)0x00272AC8 != 0x36A7C5AC) {
        std::printf("constants are not as read\n");
        return 2;
    }
    at<void(__cdecl*)()>(0x00125BC7)();
    // The original's filler, on a table of our own. Each slot named here must hold the function it was read to hold, and g_atMatrix the recovered one.
    std::memset(g_table, 0xCC, sizeof g_table);
    at<void(__cdecl*)(void*)>(0x000EC2B0)(g_table);
    struct { uint32_t slot, target; void* mine; } kSlots[] = {
        {0x002C60C8, 0x00105440, (void*)g_atMatrix.Copy}, {0x002C60CC, 0x00115020, (void*)g_atMatrix.Identity}, {0x002C60D4, 0x001150A0, (void*)g_atMatrix.SetTranslation},
        {0x002C60E0, 0x001150F0, (void*)g_atMatrix.SetRotation}, {0x002C60F4, 0x00117780, (void*)g_atMatrix.SetLookAt}, {0x002C611C, 0x00115320, (void*)g_atMatrix.SetUniformScale},
        {0x002C6120, 0x00115370, (void*)g_atMatrix.SetScale}, {0x002C61D4, 0x00116150, (void*)g_atMatrix.SetInverse}, {0x002C61F8, 0x00116540, (void*)g_atMatrix.GetPosition},
        {0x002C61FC, 0x00116560, (void*)g_atMatrix.GetAxes}, {0x002C6228, 0x00116A20, (void*)g_atMatrix.Multiply}, {0x002C6234, 0x00105470, (void*)g_atMatrix.SetProduct},
        {0x002C6250, 0x00116E30, (void*)g_atMatrix.TransformPoint}, {0x002C625C, 0x00116FA0, (void*)g_atMatrix.TransformDirection}, {0x002C6264, 0x00117080, (void*)g_atMatrix.Read},
        {0x002C60D8, 0x001150C0, (void*)g_atMatrix.SetScale3x3}, {0x002C60DC, 0x00117690, (void*)g_atMatrix.SetAxisAngle}, {0x002C60E4, 0x001151F0, (void*)g_atMatrix.SetAxes},
        {0x002C60F8, 0x00117870, (void*)g_atMatrix.SetLookAtZY}, {0x002C60FC, 0x00117950, (void*)g_atMatrix.SetLookAtXY}, {0x002C6100, 0x00115240, (void*)g_atMatrix.MakeTranslation},
        {0x002C6104, 0x00117A40, (void*)g_atMatrix.MakeAxisAngle}, {0x002C6108, 0x00115290, (void*)g_atMatrix.MakeRotation}, {0x002C610C, 0x001152C0, (void*)g_atMatrix.MakeFromAxes},
        {0x002C6168, 0x00105780, (void*)g_atMatrix.ScaleAxes}, {0x002C6184, 0x001157B0, (void*)g_atMatrix.TranslateAfter}, {0x002C61A4, 0x00115B00, (void*)g_atMatrix.ScaleColumnX},
        {0x002C61A8, 0x00115B30, (void*)g_atMatrix.ScaleColumnY}, {0x002C61D0, 0x001160C0, (void*)g_atMatrix.Invert}, {0x002C61E0, 0x001056A0, (void*)g_atMatrix.Transpose},
        {0x002C61E8, 0x00116390, (void*)g_atMatrix.SetElement}, {0x002C61EC, 0x001163B0, (void*)g_atMatrix.GetElement}, {0x002C620C, 0x00116640, (void*)g_atMatrix.GetXAxis},
        {0x002C6210, 0x00116660, (void*)g_atMatrix.GetYAxis}, {0x002C6214, 0x00116680, (void*)g_atMatrix.GetZAxis}, {0x002C622C, 0x00116A50, (void*)g_atMatrix.MultiplyRight},
        {0x002C6254, 0x00116EB0, (void*)g_atMatrix.TransformPointZ}, {0x002C6258, 0x00116EE0, (void*)g_atMatrix.InverseTransformPoint},
        {0x002C6260, 0x00117010, (void*)g_atMatrix.InverseTransformDirection}};
    for (auto& k : kSlots) {
        g_checks++;
        if (!k.mine) { std::printf("DIFF slot %08X has no member\n", k.slot); g_failures++; }
        if (g_table[(k.slot - 0x002C60C8) / 4] != k.target) { std::printf("DIFF slot %08X holds %08X\n", k.slot, g_table[(k.slot - 0x002C60C8) / 4]); g_failures++; }
    }
    g_checks++;
    if ((void*)g_atMatrix.Read != (void*)AtMatrixRead) { std::printf("DIFF Read is not AtMatrixRead\n"); g_failures++; }
    if (entries && !arm_entries(entries)) return 2;
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.
    unsigned int control;
    _controlfp_s(&control, _PC_53, _MCW_PC);
    Hash hash;
    round_of(4242, count, 0, &hash);
    std::printf("hash of the original's results, %d rounds: %016llX\n", count, hash.h);
    round_of(777, count / 4, 1, nullptr);   // Infinities and NaNs.
    round_of(999, count / 4, 2, nullptr);   // Signalling NaNs too.

    GmNameMap::s_heap = &g_file_heap;
    GmNameSpaceDb::s_heap = &g_file_heap;
    AtCoordSysDatabase::s_heap = &g_file_heap;
    AtIoStream::s_heap = &g_file_heap;
    AtIoTokenFile::s_heap = &g_file_heap;
    FileTotals totals = {};
    const std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AKC\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty() && std::fread(data.data(), 1, data.size(), f) != data.size()) { std::fclose(f); return 2; }
        std::fclose(f);
        run_akc_file(data, line, &totals);
    }
    std::fclose(index);
    std::printf(".AKC files with the original's matrix functions and the recovered ones: %u files, %u coordinate systems, %u matrices, %u drawing transforms\n", totals.files, totals.entries, totals.matrices, totals.drawing);
    std::printf("NaNs of arithmetic differing only in sign or payload: %d\n", g_nan_alike);
    for (const auto& k : g_by_op) std::printf("  %s: %d\n", k.first.c_str(), k.second);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
