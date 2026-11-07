// Differential oracle for skeletons, .AKC files (Windows x86, local only). See docs/verification.md.
//
//   oracle_akc <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AKC file listed in index.txt, loaded through the file code on both sides, loaded over itself, cleared, and loaded again; text written for each word and error path, with the report coming back, and with the
// report ending the load as it does in the game; the matrix reader and NextMatrix by themselves.
//
// Doubles, on both sides, so no evidence about them: the graphics layer's matrix device and pool, and the matrix table's SetTranslation, SetRotation, SetScale and SetUniformScale, which log and keep every value they are given
// in the matrix. The matrix reader is the original's own on its side, 0x00117080, reached through the table. The file code, tokenizer, name map, Attach and Allocate are the original's own on its side.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <new>
#include <string>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#define AtFatal AtFatalInDoubles
#include "oracle_doubles.h"
#undef AtFatal

// A report does not return in the game. Here it does, which shows what comes after it, and at a file's end would go on for ever. So after this many in one load a deliberate crash ends the load, at the same report on both sides.
// A limit of zero ends the load at its first report, as the game would.
static int g_reports;
static int g_report_limit = 40;
static void count_report() {
    if (g_expect_crash && ++g_reports > g_report_limit) *(volatile int*)nullptr = 0;
}
void AtFatal(const char* m, int a, int b, int c) { AtFatalInDoubles(m, a, b, c); count_report(); }
static void __cdecl o_fatal2(const char* m, int a, int b, int c) { o_fatal(m, a, b, c); count_report(); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap;

// ---- a running hash of everything the doubles see, since the log has a limit ------------------------------------------------
static uint64_t g_hash[2];
static void note(uint32_t a, uint32_t b = 0) {
    if (!g_recording) return;
    g_log[g_side].add(a, b);
    g_hash[g_side] = (g_hash[g_side] ^ a) * 0x100000001B3ULL;
    g_hash[g_side] = (g_hash[g_side] ^ b) * 0x100000001B3ULL;
}

// ---- matrices ---------------------------------------------------------------------------------------------------------------
struct Mat { uint32_t v[16]; };
enum { kMats = 1 << 14 };
static Mat g_mats[2][kMats];
static unsigned g_mat_used[2];
static Mat g_spare[2];
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static uint32_t mat_id(int side, const void* p) {
    const Mat* m = (const Mat*)p;
    if (m >= g_mats[side] && m < g_mats[side] + kMats) return (uint32_t)(m - g_mats[side]);
    if (m == &g_spare[side]) return 0xFFFFFF01u;
    return p ? 0xFFFFFF00u : 0xFFFFFFFFu;
}
static void keep(AtMatrix* m, uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d = 0) {
    note(op, mat_id(g_side, m));
    note(a, b);
    note(c, d);
    Mat* x = (Mat*)m;
    x->v[0] = op ^ (x->v[0] * 3);
    x->v[1] ^= a;
    x->v[2] ^= b;
    x->v[3] ^= c;
    x->v[4] ^= d;
}
static void __cdecl m_identity(AtMatrix* m) { note('i', mat_id(g_side, m)); Mat* x = (Mat*)m; std::memset(x, 0, sizeof *x); x->v[0] = 1; x->v[5] = 1; x->v[10] = 1; x->v[15] = 1; }
static void __cdecl m_translation(AtMatrix* m, const AtVector3* p) { keep(m, 't', bits(p->x), bits(p->y), bits(p->z)); }
static void __cdecl m_rotation(AtMatrix* m, const AtQuaternion* q) { keep(m, 'q', bits(q->x), bits(q->y), bits(q->z), bits(q->w)); }
static void __cdecl m_scale(AtMatrix* m, const AtVector3* p) { keep(m, 's', bits(p->x), bits(p->y), bits(p->z)); }
static void __cdecl m_uniform(AtMatrix* m, float f) { keep(m, 'u', bits(f), 0, 0); }
// The table's reader, wrapped to log which matrix each side asks for. What it reads shows in the matrix.
static void __cdecl m_read(AtMatrix* m, AtIoTokenizer* t) {
    note('d', mat_id(g_side, m));
    if (g_side == 0) at<void(__cdecl*)(AtMatrix*, AtIoTokenizer*)>(0x00117080)(m, t);
    else AtMatrixRead(m, t);
}

static void set_tables() {
    g_atMatrix.Identity = m_identity;
    g_atMatrix.SetTranslation = m_translation;
    g_atMatrix.SetRotation = m_rotation;
    g_atMatrix.SetScale = m_scale;
    g_atMatrix.SetUniformScale = m_uniform;
    g_atMatrix.Read = m_read;
    *(void**)(uintptr_t)0x002C60CC = (void*)m_identity;
    *(void**)(uintptr_t)0x002C60D4 = (void*)m_translation;
    *(void**)(uintptr_t)0x002C60E0 = (void*)m_rotation;
    *(void**)(uintptr_t)0x002C6120 = (void*)m_scale;
    *(void**)(uintptr_t)0x002C611C = (void*)m_uniform;
    *(void**)(uintptr_t)0x002C6264 = (void*)m_read;
}

static int g_count_more;  // The pool answers Count with this many more than it holds. The only way to one matrix left, since a file's are reserved in twos.

class TestPool : public AtMatrixPool {
public:
    int side = 0;
    unsigned base = 0, capacity = 0;
    void Slot0() override {}
    void Reserve(unsigned n) override {
        note('R', n);
        base = g_mat_used[side];
        capacity = n;
        const unsigned last = std::min(base + n, (unsigned)kMats);
        for (unsigned i = base; i < last; i++) std::memset(&g_mats[side][i], 0xAB, sizeof(Mat));
        g_mat_used[side] = last;
    }
    void Clear() override { note('K', capacity); }
    unsigned Count() override { note('#', capacity); return capacity + g_count_more; }
    AtMatrix* At(unsigned i) override {
        note('A', i);
        if (i >= capacity || base + i >= kMats) return (AtMatrix*)&g_spare[side];
        return (AtMatrix*)&g_mats[side][base + i];
    }
};
static TestPool g_pool[2];

class TestMatrixDevice : public AtMatrixDevice {
public:
    int side = 0;
    AtMatrix* NewMatrix() override { note('n'); return (AtMatrix*)&g_spare[side]; }
    AtMatrixPool* NewPool() override { note('o'); return &g_pool[side]; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override { note('f'); }
    void FreePool(AtMatrixPool* p) override { note('F', p == &g_pool[side]); }
};
static TestMatrixDevice g_device[2];

// ---- the database on each side ---------------------------------------------------------------------------------------------
typedef AtCoordSysDatabase Db;
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void*(__fastcall* FnNext)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

static Db* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(Db));
    std::memset(o, fill, sizeof(Db));
    at<Fn0>(0x000268F0)(o, nullptr);
    eq("the original's vftable", *(uint32_t*)o, 0x00273120);
    return (Db*)o;
}
static Db* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(Db));
    std::memset(r, fill, sizeof(Db));
    return new (r) Db;
}

// A coordinate system by where it is: kind and index, so that the two sides compare. An entry past a count is still its index, since a file can read over the end of an array.
static uint32_t cs_id(const Db* d, const void* p) {
    if (!p) return 0xFFFFFFFFu;
    const char* c = (const char*)p;
    if (d->m_rotTrans) {
        const char* first = (const char*)d->m_rotTrans;
        if (c >= first && c < first + (d->m_rotTransCount + 64) * sizeof(AtRotTransCoordSys)) return (uint32_t)((c - first) / sizeof(AtRotTransCoordSys));
    }
    if (d->m_scaled) {
        const char* first = (const char*)d->m_scaled;
        if (c >= first && c < first + (d->m_scaledCount + 64) * sizeof(AtScaleCoordSys)) return 0x10000u + (uint32_t)((c - first) / sizeof(AtScaleCoordSys));
    }
    return 0xFFFFFF00u;
}

static void same_cs(const Db* da, const Db* db, const AtCoordSys* a, const AtCoordSys* b, int side_size) {
    eq("cs parent", cs_id(da, a->m_parent), cs_id(db, b->m_parent));
    eq("cs sibling", cs_id(da, a->m_nextSibling), cs_id(db, b->m_nextSibling));
    eq("cs child", cs_id(da, a->m_firstChild), cs_id(db, b->m_firstChild));
    eq("cs local", mat_id(0, a->m_local), mat_id(1, b->m_local));
    eq("cs world", mat_id(0, a->m_world), mat_id(1, b->m_world));
    eq("cs drawing local", mat_id(0, a->m_drawingLocal), mat_id(1, b->m_drawingLocal));
    eq("cs drawing world", mat_id(0, a->m_drawingWorld), mat_id(1, b->m_drawingWorld));
    eq("cs +20", a->m_unknown20, b->m_unknown20);
    eq("cs dirty", a->m_dirty, b->m_dirty);
    if (side_size == sizeof(AtScaleCoordSys)) {
        const AtVector3& x = ((const AtScaleCoordSys*)a)->m_scale;
        const AtVector3& y = ((const AtScaleCoordSys*)b)->m_scale;
        eq("scale x", bits(x.x), bits(y.x));
        eq("scale y", bits(x.y), bits(y.y));
        eq("scale z", bits(x.z), bits(y.z));
    }
}

static void same_db(const char* what, const Db* o, const Db* r) {
    g_recording = false;
    eq("device", o->m_device == &g_device[0], r->m_device == &g_device[1]);
    eq("has a device", o->m_device != nullptr, r->m_device != nullptr);
    eq("pool", o->m_pool == &g_pool[0], r->m_pool == &g_pool[1]);
    eq("rot count", o->m_rotTransCount, r->m_rotTransCount);
    eq("scale count", o->m_scaledCount, r->m_scaledCount);
    eq("rot array", o->m_rotTrans != nullptr, r->m_rotTrans != nullptr);
    eq("scale array", o->m_scaled != nullptr, r->m_scaled != nullptr);
    eq("root", cs_id(o, o->m_root), cs_id(r, r->m_root));
    eq("extra", o->m_extra, r->m_extra);
    eq("next matrix", o->m_nextMatrix, r->m_nextMatrix);
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's entry", cs_id(o, x.m_value), cs_id(r, y.m_value));
            }
        }
    }
    if (o->m_rotTrans && r->m_rotTrans && o->m_rotTransCount == r->m_rotTransCount) {
        for (unsigned i = 0; i < o->m_rotTransCount; i++) same_cs(o, r, &o->m_rotTrans[i], &r->m_rotTrans[i], sizeof(AtRotTransCoordSys));
    }
    if (o->m_scaled && r->m_scaled && o->m_scaledCount == r->m_scaledCount) {
        for (unsigned i = 0; i < o->m_scaledCount; i++) same_cs(o, r, &o->m_scaled[i], &r->m_scaled[i], sizeof(AtScaleCoordSys));
    }
    // Every word of every matrix handed out so far. Those the pool reserved start from one pattern, so one not written stands out.
    eq("matrices used", g_mat_used[0], g_mat_used[1]);
    if (g_mat_used[0] == g_mat_used[1]) {
        for (unsigned i = 0; i < g_mat_used[0]; i++) {
            for (int k = 0; k < 16; k++) eq("matrix", g_mats[0][i].v[k], g_mats[1][i].v[k]);
        }
    }
    eq("calls", (long long)g_hash[0], (long long)g_hash[1]);
    compare_logs(what);
}

static const char* const kFileName = "ART\\CAST\\THING";

// A crash storing a real read into a null matrix leaves that real on the x87 stack, and the guard does not restore the x87. Eight of them and every later load would read NaNs.
template <class F> static bool guarded_clean(F f) {
    const bool crashed = guarded(f);
    g_recording = false;
    g_side = 0;
    _fpreset();
    return crashed;
}

static void load_both(Db* o, Db* r, const char* what) {
    g_reports = 0;
    const bool oc = guarded_clean([&] { On on(0); at<FnLoad>(0x00026DB0)(o, nullptr, &g_device[0], kFileName, 77); });
    g_reports = 0;
    const bool rc = guarded_clean([&] { On on(1); r->Load(&g_device[1], kFileName, 77); });
    eq("crashed", oc, rc);
    same_db(what, o, r);
}

// Clearing what a file left. A file can leave a cycle, or entries past an array, so it may crash; both must, and alike.
static void clear_both(Db* o, Db* r) {
    const bool oc = guarded([&] { On on(0); at<Fn0>(0x000271F0)(o, nullptr); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); r->Clear(); });
    g_recording = false;
    g_side = 0;
    eq("clear crashed", oc, rc);
    same_db("clear", o, r);
}

static unsigned g_files, g_rot_total, g_scale_total, g_drawing_total, g_roots;
static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, bool stats = false) {
    g_context = context;
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    g_mat_used[0] = g_mat_used[1] = 0;
    g_hash[0] = g_hash[1] = 0;
    Db* o = make_o(fill);
    Db* r = make_r(fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(Db) - 4), 0);
    load_both(o, r, "load");
    if (stats) {
        g_rot_total += r->m_rotTransCount;
        g_scale_total += r->m_scaledCount;
        g_drawing_total += r->m_extra;
        g_roots += r->m_root != nullptr;
    }
    load_both(o, r, "load again");
    clear_both(o, r);
    load_both(o, r, "load after clear");
    const bool oc = guarded([&] { On on(0); at<FnDel>(0x00019CD0)(o, nullptr, 0); });
    g_recording = false;
    g_side = 0;
    const bool rc = guarded([&] { On on(1); r->~Db(); });
    g_recording = false;
    g_side = 0;
    eq("destroy crashed", oc, rc);
    compare_logs("destroy");
    eq("calls", (long long)g_hash[0], (long long)g_hash[1]);
    std::free(o);
    std::free(r);
}

// ---- written text -----------------------------------------------------------------------------------------------------------
#define M16 "1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5 10.5 11.5 12.5 13.5 14.5 15.5 16.5"
#define POS "Position 1.5 -2.5 3.25\n"
#define ORI "Orientation 0.1 0.2 0.3 0.4\n"
#define RT(name, body) " RotTransCoordSys \"" name "\"\n {\n " body " }\n"
#define SC(name, body) " ScaleCoordSys \"" name "\"\n {\n " body " }\n"
#define R1 "RotTransCoordSys[1]\nCoordSys\n{\n"
#define R2 "RotTransCoordSys[2]\nCoordSys\n{\n"
#define R3 "RotTransCoordSys[3]\nCoordSys\n{\n"
#define S1 "ScaleCoordSys[1]\nCoordSys\n{\n"
#define S2 "ScaleCoordSys[2]\nCoordSys\n{\n"
#define R2S2 "RotTransCoordSys[2]\nScaleCoordSys[2]\nCoordSys\n{\n"
#define R1D1 "RotTransCoordSys[1]\nLocalDrawingTransform[1]\nCoordSys\n{\n"
#define R2D2 "RotTransCoordSys[2]\nLocalDrawingTransform[2]\nCoordSys\n{\n"
#define S1D1 "ScaleCoordSys[1]\nLocalDrawingTransform[1]\nCoordSys\n{\n"

static const char* const kTexts[] = {
    // Heads.
    "", "// a comment\n", "CoordSys\n{\n}\n", "RotTransCoordSys[1]\n", "ScaleCoordSys[1]\n", "LocalDrawingTransform[1]\n", "LocalDrawingTransform[1]\nCoordSys\n{\n}\n",
    "RotTransCoordSys[0]\nCoordSys\n{\n}\n", "ScaleCoordSys[0]\nCoordSys\n{\n}\n", "RotTransCoordSys[0]\nScaleCoordSys[1]\nCoordSys\n{\n}\n", "RotTransCoordSys[1]\nScaleCoordSys[0]\nCoordSys\n{\n}\n",
    "RotTransCoordSys[3]\nRotTransCoordSys[1]\nCoordSys\n{\n" RT("a", "") "}\n", "ScaleCoordSys[3]\nScaleCoordSys[1]\nCoordSys\n{\n" SC("a", "") "}\n", "RotTransCoordSys[1]\nLocalDrawingTransform[5]\nLocalDrawingTransform[1]\nCoordSys\n{\n" RT("a", "") "}\n",
    "ScaleCoordSys[1]\nRotTransCoordSys[1]\nCoordSys\n{\n" RT("a", "") SC("b", "") "}\n", "LocalDrawingTransform[1]\nRotTransCoordSys[1]\nCoordSys\n{\n" RT("a", "") "}\n",
    "LocalDrawingTransform[3]\nScaleCoordSys[2]\nRotTransCoordSys[4]\n", "RotTransCoordSys[40]\nScaleCoordSys[30]\nLocalDrawingTransform[70]\nCoordSys\n{\n}\n",
    "Position\nRotTransCoordSys[1]\nCoordSys\n{\n}\n", "5\nRotTransCoordSys[1]\n", "1.5\n", "\"text\"\nRotTransCoordSys[1]\n", "{\n", "}\n", "Parent\n", "Nonsense\nRotTransCoordSys[1]\n",
    "RotTransCoordSys\n", "RotTransCoordSys 1]\n", "RotTransCoordSys[1\n", "RotTransCoordSys[\"a\"]\n", "RotTransCoordSys[1.5]\n",
    "ScaleCoordSys\n", "ScaleCoordSys 1]\n", "ScaleCoordSys[1\n", "ScaleCoordSys[\"a\"]\n",
    "LocalDrawingTransform\n", "LocalDrawingTransform 1]\n", "LocalDrawingTransform[1\n", "LocalDrawingTransform[\"a\"]\n",
    // What comes after the word CoordSys.
    "RotTransCoordSys[1]\nCoordSys\n", "RotTransCoordSys[1]\nCoordSys\n}\n", "RotTransCoordSys[1]\nCoordSys 5\n}\n", "RotTransCoordSys[1]\nCoordSys\n{\n", R1 "}\nRotTransCoordSys[1]\n", R1 "}\n", R2 "}\n",
    // Entries at the level of the list.
    R1 "Position\n}\n", R1 "5\n}\n", R1 "1.5\n}\n", R1 "\"text\"\n}\n", R1 "{\n}\n", R1 "CoordSys\n}\n", R1 "Parent\n}\n", R1 "LocalDrawingTransform\n}\n", R1 "Nonsense\n}\n",
    // Names.
    R1 RT("LongerThanEight", "") "}\n", R1 RT("Exactly8", "") "}\n", R1 RT("", "") "}\n",
    R2 RT("a", "") RT("a", "") "}\n", R2S2 RT("a", "") SC("a", "") RT("b", "") SC("b", "") "}\n",
    // Bodies, with and without curlies, and cut short.
    R1 " RotTransCoordSys \"a\"\n}\n", S1 " ScaleCoordSys \"a\"\n}\n", R1 " RotTransCoordSys \"a\"\n {\n", S1 " ScaleCoordSys \"a\"\n {\n", R1 " RotTransCoordSys \"a\"\n {\n Position 1.5\n", S1 " ScaleCoordSys \"a\"\n {\n Scale 1.5\n", S1 " ScaleCoordSys \"a\"\n {\n UniformScale\n",
    // The words of the rotation.
    R1 RT("a", POS) "}\n", R1 RT("a", ORI) "}\n", R1 RT("a", POS ORI) "}\n", R1 RT("a", ORI POS) "}\n", R1 RT("a", POS "Position 7.5 8.5 9.5\n") "}\n", R1 RT("a", ORI "Orientation 0.5 0.6 0.7 0.8\n") "}\n",
    R2 RT("a", POS ORI) RT("b", "Position -1.5 -2.5 -3.5\nOrientation -0.1 -0.2 -0.3 -0.4\n") "}\n",
    R1 RT("a", "Position 1 2 3\n") "}\n", R1 RT("a", "Orientation 1 0 0 0\n") "}\n", R1 RT("a", "Position \"x\" 2.5 3.5\n") "}\n", R1 RT("a", "Orientation \"x\" 2.5 3.5 4.5\n") "}\n",
    // Words of the other kind, and of nothing, in each.
    R1 RT("a", "Scale 1.5 2.5 3.5\n") "}\n", R1 RT("a", "UniformScale 1.5\n") "}\n", R1 RT("a", "CoordSys\n") "}\n", R1 RT("a", "RotTransCoordSys\n") "}\n", R1 RT("a", "5\n") "}\n", R1 RT("a", "{\n") "}\n", R1 RT("a", "Nonsense\n") "}\n",
    S1 SC("a", POS) "}\n", S1 SC("a", ORI) "}\n", S1 SC("a", "CoordSys\n") "}\n", S1 SC("a", "ScaleCoordSys\n") "}\n", S1 SC("a", "1.5\n") "}\n", S1 SC("a", "[\n") "}\n", S1 SC("a", "Nonsense\n") "}\n",
    // The words of the scale.
    S1 SC("a", "Scale 1.5 2.5 0.5\n") "}\n", S1 SC("a", "UniformScale 2.5\n") "}\n", S1 SC("a", "Scale 1.5 2.5 0.5\nUniformScale 7.5\n") "}\n", S1 SC("a", "UniformScale 7.5\nScale 1.5 2.5 0.5\n") "}\n", S1 SC("a", "UniformScale 1.5\nUniformScale 2.5\n") "}\n",
    S1 SC("a", "Scale 1 2 3\n") "}\n", S1 SC("a", "UniformScale 2\n") "}\n", S1 SC("a", "UniformScale \"x\"\n") "}\n",
    S2 SC("a", "Scale 1.5 2.5 3.5\n") SC("b", "UniformScale 0.25\n") "}\n",
    // Parents.
    R2 RT("a", POS) RT("b", "Parent \"a\"\n") "}\n", R3 RT("a", "") RT("b", "Parent \"a\"\n") RT("c", "Parent \"a\"\n") "}\n", R3 RT("a", "") RT("b", "Parent \"a\"\n") RT("c", "Parent \"b\"\n") "}\n",
    R1 RT("a", "Parent \"a\"\n") "}\n", R3 RT("a", "") RT("b", "") RT("c", "Parent \"a\"\nParent \"b\"\n") "}\n", R2 RT("a", "") RT("b", "Parent \"a\"\nParent \"a\"\n") "}\n",
    R2S2 RT("a", "") SC("b", "Parent \"a\"\n") RT("c", "Parent \"b\"\n") SC("d", "Parent \"c\"\n") "}\n", R2S2 SC("a", "") SC("b", "Parent \"a\"\n") RT("c", "Parent \"a\"\n") RT("d", "Parent \"b\"\n") "}\n",
    R2 RT("a", "Parent \"b\"\n") RT("b", "") "}\n", R1 RT("a", "Parent \"nobody\"\n") "}\n", S1 SC("a", "Parent \"nobody\"\n") "}\n",
    R2 RT("LongerThanEight", "") RT("b", "Parent \"LongerThanEight\"\n") "}\n", R2 RT("Exactly8", "") RT("b", "Parent \"Exactly8\"\n") "}\n",
    // The root: the last with no parent, of either kind, when the entry ends.
    R3 RT("a", "") RT("b", "") RT("c", "Parent \"a\"\n") "}\n", R3 RT("a", "") RT("b", "Parent \"a\"\n") RT("c", "") "}\n", R2S2 RT("a", "") SC("b", "") RT("c", "") SC("d", "Parent \"a\"\n") "}\n", R2S2 SC("a", "") RT("b", "") SC("c", "") RT("d", "Parent \"a\"\n") "}\n", R1 "}\n",
    // More and fewer than the counts.
    R1 RT("a", POS) RT("b", POS) "}\n", S1 SC("a", "UniformScale 1.5\n") SC("b", "UniformScale 2.5\n") "}\n", R2 RT("a", "") "}\n", S2 SC("a", "") "}\n", "RotTransCoordSys[1]\nCoordSys\n{\n" RT("a", "") SC("b", "") "}\n", "ScaleCoordSys[1]\nCoordSys\n{\n" SC("a", "") RT("b", "") "}\n",
    // Drawing transforms.
    R1D1 RT("a", "LocalDrawingTransform " M16 "\n") "}\n", S1D1 SC("a", "LocalDrawingTransform " M16 "\n") "}\n", R2D2 RT("a", "LocalDrawingTransform " M16 "\n") RT("b", "LocalDrawingTransform 16.5 15.5 14.5 13.5 12.5 11.5 10.5 9.5 8.5 7.5 6.5 5.5 4.5 3.5 2.5 1.5\n") "}\n",
    R2D2 RT("a", "") RT("b", "LocalDrawingTransform " M16 "\n") "}\n", R2D2 RT("a", "LocalDrawingTransform " M16 "\nLocalDrawingTransform 1.25 2.25 3.25 4.25 5.25 6.25 7.25 8.25 9.25 10.25 11.25 12.25 13.25 14.25 15.25 16.25\n") "}\n",
    R1 RT("a", "LocalDrawingTransform " M16 "\n") "}\n", S1 SC("a", "LocalDrawingTransform " M16 "\n") "}\n", R1D1 RT("a", "LocalDrawingTransform 1.5 2.5\n") "}\n", R1D1 RT("a", "LocalDrawingTransform\n") "}\n", R1D1 RT("a", "LocalDrawingTransform 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16\n") "}\n",
    R1D1 " RotTransCoordSys \"a\"\n {\n LocalDrawingTransform 1.5 2.5", R1D1 RT("a", "LocalDrawingTransform \"x\" 1.5 2.5 3.5 4.5 5.5 6.5 7.5 8.5 9.5 10.5 11.5 12.5 13.5 14.5 15.5\n") "}\n",
};

// A name that is not a string: past the report, the name is what the stack held, so a load that goes on cannot be compared. These stop at the report, as the game does.
static const char* const kTextsToTheReport[] = {
    R1 " RotTransCoordSys 5\n {\n }\n}\n", R1 " RotTransCoordSys\n {\n }\n}\n", S1 " ScaleCoordSys 5\n {\n }\n}\n", S1 " ScaleCoordSys\n {\n }\n}\n", R1 RT("a", "Parent 5\n") "}\n", R1 RT("a", "Parent\n") "}\n", S1 SC("a", "Parent 5\n") "}\n",
};

static void run_written() {
    g_report_limit = 0;
    for (const char* t : kTextsToTheReport) run_text(bytes(t), std::string("to the report [") + t + "]", 0xCD);
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const int limit : {40, 0}) {
            g_report_limit = limit;
            for (const char* t : kTexts) run_text(bytes(t), std::string("text [") + t + "] limit " + std::to_string(limit), fill);
        }
    }
    g_report_limit = 40;
    // One matrix left when two are wanted: the second report, with no crash after it. Past what the pool holds, both sides get the spare.
    for (const int more : {1, 3}) {
        g_count_more = more;
        for (const char* t : {R1 RT("a", "LocalDrawingTransform " M16 "\n") "}\n", S1 SC("a", "LocalDrawingTransform " M16 "\n") "}\n"}) {
            run_text(bytes(t), std::string("spare ") + std::to_string(more) + " " + t, 0xCD);
        }
    }
    g_count_more = 0;
}

// ---- entries by themselves --------------------------------------------------------------------------------------------------
class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};
class DirectTokenizer : public AtIoTextTokenizer {
public:
    void Error(int code) override { if (g_recording) g_log[g_side].add('E', code); count_report(); }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const char* t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void run_tables() {
    g_context = "keywords";
    const AtIoKeyword* table = (const AtIoKeyword*)(uintptr_t)0x002B05F8;
    for (int i = 0; i < AtCoordSysDatabase::kKeywordCount; i++) {
        eq("keyword's id", table[i].m_id, AtCoordSysDatabase::s_keywords[i].m_id);
        eqs("keyword's word", table[i].m_name, AtCoordSysDatabase::s_keywords[i].m_name);
    }
}

static void run_matrix() {
    g_context = "matrix reader";
    // On a real matrix table's slot: the read itself, each float in its place, and what is left of the text afterwards.
    for (const char* text : {M16, M16 " 99.5", "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16", "-1.5e3 0.0 1e-3 2.5", "\"a\" 1.5", ""}) {
        Mat a, b;
        std::memset(&a, 0xAB, sizeof a);
        std::memset(&b, 0xAB, sizeof b);
        Direct da(text), db(text);
        da.tokenizer.SetKeywords(AtCoordSysDatabase::s_keywords, AtCoordSysDatabase::kKeywordCount, nullptr);
        db.tokenizer.SetKeywords(AtCoordSysDatabase::s_keywords, AtCoordSysDatabase::kKeywordCount, nullptr);
        { On on(0); at<void(__cdecl*)(AtMatrix*, AtIoTokenizer*)>(0x00117080)((AtMatrix*)&a, &da.tokenizer); }
        { On on(1); AtMatrixRead((AtMatrix*)&b, &db.tokenizer); }
        for (int k = 0; k < 16; k++) eq("cell", a.v[k], b.v[k]);
        eq("position after", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
        compare_logs("matrix reader");
    }
}

static void run_next() {
    g_context = "next matrix";
    g_fill = 0xCD;
    for (const unsigned rot : {1u, 2u}) {
        for (const unsigned drawing : {0u, 1u, 2u}) {
            for (const int more : {0, 1}) {
                g_mat_used[0] = g_mat_used[1] = 0;
                Db* o = make_o(0xCD);
                Db* r = make_r(0xCD);
                g_count_more = more;
                { On on(0); at<void(__fastcall*)(void*, void*, void*, unsigned, unsigned, unsigned)>(0x00026920)(o, nullptr, &g_device[0], rot, 0, drawing); }
                { On on(1); r->Allocate(&g_device[1], rot, 0, drawing); }
                for (int i = 0; i < 8; i++) {
                    void* a;
                    void* b;
                    { On on(0); a = at<FnNext>(0x000268C0)(o, nullptr); }
                    { On on(1); b = r->NextMatrix(); }
                    eq("next", mat_id(0, a), mat_id(1, b));
                    eq("next index", o->m_nextMatrix, r->m_nextMatrix);
                }
                same_db("next matrix", o, r);
                g_count_more = 0;
                { On on(0); at<Fn0>(0x000271F0)(o, nullptr); }
                { On on(1); r->Clear(); }
                compare_logs("next matrix: clear");
                std::free(o);
                std::free(r);
            }
        }
    }
}

// An entry's reader by itself, with its dirty bits not what they are from Allocate. In a file every entry is new, so all the bits the words set are already set, and only this shows that they set them, and add to what is there.
static void run_entries() {
    g_context = "entries by themselves";
    g_fill = 0xCD;
    typedef void(__fastcall* FnEntry)(void*, void*, void*, void*);
    static const char* const rotations[] = {"\"a\" { Position 1.5 2.5 3.5 }", "\"a\" { Orientation 0.1 0.2 0.3 0.4 }", "\"a\" { }"};
    static const char* const scales[] = {"\"a\" { Scale 1.5 2.5 3.5 }", "\"a\" { UniformScale 2.5 }", "\"a\" { }"};
    for (const unsigned dirty : {0u, 0x10u, 0x40u, 0x2Bu, 0xFFFFFFFFu}) {
        for (int kind = 0; kind < 2; kind++) {
            for (const char* text : (kind ? scales : rotations)) {
                g_context = std::string("entry by itself [") + text + "] dirty " + std::to_string(dirty);
                g_mat_used[0] = g_mat_used[1] = 0;
                g_hash[0] = g_hash[1] = 0;
                Db* o = make_o(0xCD);
                Db* r = make_r(0xCD);
                { On on(0); at<void(__fastcall*)(void*, void*, void*, unsigned, unsigned, unsigned)>(0x00026920)(o, nullptr, &g_device[0], 1, 1, 0); }
                { On on(1); r->Allocate(&g_device[1], 1, 1, 0); }
                o->m_rotTrans[0].m_dirty = r->m_rotTrans[0].m_dirty = dirty;
                o->m_scaled[0].m_dirty = r->m_scaled[0].m_dirty = dirty;
                Direct da(text), db(text);
                da.tokenizer.SetKeywords(AtCoordSysDatabase::s_keywords, AtCoordSysDatabase::kKeywordCount, nullptr);
                db.tokenizer.SetKeywords(AtCoordSysDatabase::s_keywords, AtCoordSysDatabase::kKeywordCount, nullptr);
                if (kind == 0) {
                    { On on(0); at<FnEntry>(0x00032D40)(&o->m_rotTrans[0], nullptr, &da.tokenizer, o); }
                    { On on(1); r->m_rotTrans[0].Read(&db.tokenizer, r); }
                } else {
                    { On on(0); at<FnEntry>(0x00032C70)(&o->m_scaled[0], nullptr, &da.tokenizer, o); }
                    { On on(1); r->m_scaled[0].Read(&db.tokenizer, r); }
                }
                same_db("entry by itself", o, r);
                { On on(0); at<Fn0>(0x000271F0)(o, nullptr); }
                { On on(1); r->Clear(); }
                compare_logs("entry by itself: clear");
                std::free(o);
                std::free(r);
            }
        }
    }
}

// A file that will not open: the report, and then a null tokenizer. In the game the report does not come back.
static void run_no_file() {
    g_context = "no file";
    static const std::string none;
    const auto data = bytes(none);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = 0;
    for (const int status : {1, 2, 5}) {
        g_fs->open_status = status;
        g_mat_used[0] = g_mat_used[1] = 0;
        Db* o = make_o(0xCD);
        Db* r = make_r(0xCD);
        const int before = g_crashes;
        load_both(o, r, "no file");
        eq("this crashed, on both", g_crashes - before, 2);
        g_fs->open_status = 0;
        std::free(o);
        std::free(r);
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_akc <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal2);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    set_tables();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6344u}) *(void**)(uintptr_t)va = &g_heap;
    AtIoTokenFile::s_heap = &g_heap;
    AtIoStream::s_heap = &g_heap;
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtCoordSysDatabase::s_heap = &g_heap;
    for (int side = 0; side < 2; side++) {
        g_pool[side].side = side;
        g_device[side].side = side;
        std::strcpy(g_fs->files[side].m_path, "ART\\CAST\\THING.AKC");
    }
    g_heap_pad = 0x1000;  // Room for the entries a file reads past the end of an array.

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_tables();
    run_matrix();
    run_next();
    run_entries();
    run_no_file();
    run_written();

    g_rot_total = g_scale_total = g_drawing_total = g_roots = 0;
    std::string dir = pos[1];
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
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_text(data, line, 0xCD, true);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u rotations, %u scales, %u drawing transforms, %u with a root; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_rot_total, g_scale_total, g_drawing_total, g_roots, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
