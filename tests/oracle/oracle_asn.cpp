// Differential oracle for scenes, .ASN files (Windows x86, local only). See docs/verification.md.
//
//   oracle_asn <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ASN file listed in index.txt, loaded through the file code on both sides; text written for each word and error path; and coordinate systems, nodes and modifier sets driven directly.
//
// The classes of node that are not recovered are doubles on both sides: the base node with a reader that takes the words those classes own and goes on to the base for the rest. On the original's side the double is its
// base class's code with the constructors redirected and a vftable of the harness's, so its array handling, its eh iterators and its vector deleters still run. So those words are consumed and not compared; everything the base
// class, the scene and the coordinate systems do with a file is. The matrix table, the graphics device and the owner's maps are the harness's.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtScene.h"
#include "at/AtText.h"
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

// The doubles' heap stack is wrapped, to refuse memory to one class's heap only.
#define AtPushHeap AtPushHeapInDoubles
#define AtPopHeap AtPopHeapInDoubles
#define AtFatal AtFatalInDoubles
#include "oracle_doubles.h"
#undef AtPushHeap
#undef AtPopHeap
#undef AtFatal

// A file ending inside a node makes both sides report the end for ever, since the report comes back here and not in the game. So after this many reports in one load, a deliberate crash ends it, at the
// same report on both sides, and the guard around the load catches it.
static int g_reports;
static void count_report() {
    if (g_expect_crash && ++g_reports > 40) *(volatile int*)nullptr = 0;
}
void AtFatal(const char* m, int a, int b, int c) { AtFatalInDoubles(m, a, b, c); count_report(); }
static void __cdecl o_fatal2(const char* m, int a, int b, int c) { o_fatal(m, a, b, c); count_report(); }

static void* g_refuse_heap;
static int g_refusing;
static void note_push(void* h) { if (h == g_refuse_heap && h) { g_fail_alloc = true; g_refusing++; } }
static void note_pop() { if (g_refusing) { g_refusing--; g_fail_alloc = false; } }
void AtPushHeap(void* h) { AtPushHeapInDoubles(h); note_push(h); }
void AtPopHeap() { AtPopHeapInDoubles(); note_pop(); }
static void __cdecl o_push2(void* h) { o_push(h); note_push(h); }
static void __cdecl o_pop2() { o_pop(); note_pop(); }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_file, g_heap_scene, g_heap_node, g_heap_db;

// ---- a running hash of everything the doubles see, since the log has a limit ------------------------------------------------
static uint64_t g_hash[2];
static void note(int side, uint32_t a, uint32_t b = 0) {
    if (!g_recording) return;
    g_log[side].add(a, b);
    g_hash[side] = (g_hash[side] ^ a) * 0x100000001B3ULL;
    g_hash[side] = (g_hash[side] ^ b) * 0x100000001B3ULL;
}

// ---- matrices ---------------------------------------------------------------------------------------------------------------
struct Mat { uint32_t v[4]; };
enum { kMats = 1 << 17 };
static Mat g_mats[2][kMats];
static unsigned g_mat_used[2];
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static uint32_t mat_id(int side, const void* p) {
    const Mat* m = (const Mat*)p;
    if (m >= g_mats[side] && m < g_mats[side] + kMats) return (uint32_t)(m - g_mats[side]);
    return 0xFFFFFF00u;
}
static int side_of(const void* p) { return (const Mat*)p >= g_mats[1] && (const Mat*)p < g_mats[1] + kMats ? 1 : 0; }
static void mix(Mat* m, uint32_t op, uint32_t a, uint32_t b, uint32_t c) {
    m->v[0] = op ^ (m->v[0] * 3);
    m->v[1] = a;
    m->v[2] = b;
    m->v[3] = c;
}
static uint32_t sum(const Mat* m) { return m->v[0] * 5 + m->v[1] * 7 + m->v[2] * 11 + m->v[3] * 13; }

static void __cdecl m_copy(AtMatrix* to, const AtMatrix* from) {
    const int s = side_of(to);
    note(s, 'c', mat_id(s, to));
    note(s, 'C', mat_id(s, from));
    *(Mat*)to = *(const Mat*)from;
}
static void __cdecl m_identity(AtMatrix* m) {
    const int s = side_of(m);
    note(s, 'i', mat_id(s, m));
    Mat* x = (Mat*)m;
    x->v[0] = 1; x->v[1] = 2; x->v[2] = 3; x->v[3] = 4;
}
static void __cdecl m_translation(AtMatrix* m, const AtVector3* p) {
    const int s = side_of(m);
    note(s, 't', mat_id(s, m));
    mix((Mat*)m, 't', bits(p->x), bits(p->y), bits(p->z));
}
static void __cdecl m_rotation(AtMatrix* m, const AtQuaternion* q) {
    const int s = side_of(m);
    note(s, 'q', mat_id(s, m));
    mix((Mat*)m, 'q' ^ bits(q->w), bits(q->x), bits(q->y), bits(q->z));
}
static void __cdecl m_lookat(AtMatrix* m, const AtVector3* a, const AtVector3* b) {
    const int s = side_of(m);
    note(s, 'l', mat_id(s, m));
    note(s, 'L', bits(b->x) ^ (bits(b->y) * 3) ^ (bits(b->z) * 5));
    mix((Mat*)m, 'l' ^ bits(b->x) ^ (bits(b->y) * 3) ^ (bits(b->z) * 5), bits(a->x), bits(a->y), bits(a->z));
}
static void __cdecl m_scale(AtMatrix* m, const AtVector3* p) {
    const int s = side_of(m);
    note(s, 's', mat_id(s, m));
    mix((Mat*)m, 's', bits(p->x), bits(p->y), bits(p->z));
}
static void __cdecl m_inverse(AtMatrix* to, const AtMatrix* from) {
    const int s = side_of(to);
    note(s, 'v', mat_id(s, to));
    note(s, 'V', mat_id(s, from));
    mix((Mat*)to, 'v', sum((const Mat*)from), 1, 2);
}
static void __cdecl m_multiply(AtMatrix* m, const AtMatrix* by) {
    const int s = side_of(m);
    note(s, 'x', mat_id(s, m));
    note(s, 'X', mat_id(s, by));
    mix((Mat*)m, 'x', sum((const Mat*)m), sum((const Mat*)by), 3);
}
static void __cdecl m_product(AtMatrix* to, const AtMatrix* a, const AtMatrix* b) {
    const int s = side_of(to);
    note(s, 'p', mat_id(s, to));
    note(s, 'P', mat_id(s, a));
    note(s, 'Q', mat_id(s, b));
    mix((Mat*)to, 'p', sum((const Mat*)a), sum((const Mat*)b), 4);
}
static void __cdecl m_transform(const AtMatrix* m, const AtVector3* from, AtVector3* to) {
    const int s = side_of(m);
    note(s, 'T', mat_id(s, m));
    const uint32_t k = sum((const Mat*)m);
    float f[3] = {from->x + (float)(k & 7), from->y + (float)((k >> 3) & 7), from->z + (float)((k >> 6) & 7)};
    to->x = f[0]; to->y = f[1]; to->z = f[2];
}

static void set_tables() {
    g_atMatrix.Copy = m_copy;
    g_atMatrix.Identity = m_identity;
    g_atMatrix.SetTranslation = m_translation;
    g_atMatrix.SetRotation = m_rotation;
    g_atMatrix.SetLookAt = m_lookat;
    g_atMatrix.SetScale = m_scale;
    g_atMatrix.SetInverse = m_inverse;
    g_atMatrix.Multiply = m_multiply;
    g_atMatrix.SetProduct = m_product;
    g_atMatrix.TransformPoint = m_transform;
    *(void**)(uintptr_t)0x002C60C8 = (void*)m_copy;
    *(void**)(uintptr_t)0x002C60CC = (void*)m_identity;
    *(void**)(uintptr_t)0x002C60D4 = (void*)m_translation;
    *(void**)(uintptr_t)0x002C60E0 = (void*)m_rotation;
    *(void**)(uintptr_t)0x002C60F4 = (void*)m_lookat;
    *(void**)(uintptr_t)0x002C6120 = (void*)m_scale;
    *(void**)(uintptr_t)0x002C61D4 = (void*)m_inverse;
    *(void**)(uintptr_t)0x002C6228 = (void*)m_multiply;
    *(void**)(uintptr_t)0x002C6234 = (void*)m_product;
    *(void**)(uintptr_t)0x002C6250 = (void*)m_transform;
}

static Mat g_spare[2];
class TestPool : public AtMatrixPool {
public:
    int side = 0;
    unsigned base = 0, capacity = 0;
    void Slot0() override {}
    void Reserve(unsigned n) override {
        note(side, 'R', n);
        base = g_mat_used[side];
        capacity = n;
        g_mat_used[side] = std::min(g_mat_used[side] + n, (unsigned)kMats);
    }
    void Clear() override { note(side, 'K', capacity); }
    unsigned Count() override { return capacity; }
    AtMatrix* At(unsigned i) override {
        note(side, 'A', i);
        if (i >= capacity || base + i >= kMats) return (AtMatrix*)&g_spare[side];
        return (AtMatrix*)&g_mats[side][base + i];
    }
};
static TestPool g_pool[2];

class TestMatrixDevice : public AtMatrixDevice {
public:
    int side = 0;
    AtMatrix* NewMatrix() override {
        note(side, 'n');
        if (g_mat_used[side] >= kMats) return (AtMatrix*)&g_spare[side];
        return (AtMatrix*)&g_mats[side][g_mat_used[side]++];
    }
    AtMatrixPool* NewPool() override { note(side, 'o'); return &g_pool[side]; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix* m) override { note(side, 'f', mat_id(side, m)); }
    void FreePool(AtMatrixPool* p) override { note(side, 'F', p == &g_pool[side]); }
};
static TestMatrixDevice g_matdev[2];

class TestDevice : public AtSceneDevice {
public:
    int side = 0;
    void Slot0() override {} void Slot1() override {} void Slot2() override {} void Slot3() override {} void Slot4() override {} void Slot5() override {}
    void Slot6() override {} void Slot7() override {} void Slot8() override {} void Slot9() override {} void Slot10() override {} void Slot11() override {}
    void Slot12() override {} void Slot13() override {} void Slot14() override {} void Slot15() override {} void Slot16() override {}
    AtMatrixDevice* MatrixDevice() override { note(side, 'd'); return &g_matdev[side]; }
};
static TestDevice g_device[2];

// ---- the classes not recovered ---------------------------------------------------------------------------------------------
static const unsigned kSizes[kAtActorKinds] = {0xA0, 0xCC, 0xFC, 0xB8, 0x13C, 0xA8, 0xAC, 0xB0, 0x70};
static const uint16_t kClasses[kAtActorKinds] = {2, 3, 8, 9, 10, 11, 12, 13, 15};
static const uint32_t kOriginalCtor[kAtActorKinds] = {0x0004B0B0, 0x0004B680, 0x0004D1D0, 0x0004E4A0, 0x0004EBE0, 0x0004D520, 0x0004E300, 0x0004DB60, 0x0004EF90};
static const uint32_t kOriginalDtor[kAtActorKinds] = {0x0004B5E0, 0x0004C040, 0x0004D390, 0x0004E6F0, 0x0004EF00, 0x0004DA30, 0x0004E180, 0x0004E100, 0};  // The last is the real 0x0003B910.
static const uint32_t kOriginalDeleter[kAtActorKinds] = {0x0003B850, 0x0003B8B0, 0x0003B920, 0x0003BAA0, 0x0003BB00, 0x0003B980, 0x0003BA40, 0x0003B9E0, 0x0003CD00};

typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnI)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

// The sound node is 0x70 bytes, so it has none of the fields past that, and keeps the base's slots.
static const uint16_t kSoundClass = 15;
static void*& raw(void* p, unsigned off) { return *(void**)((char*)p + off); }
static void* raw(const void* p, unsigned off) { return *(void* const*)((const char*)p + off); }

// Every name in the file, as joints of any jointed actor. The real class fills this from CoordSysDatabase, which is not recovered.
static GmNameMap* g_joints[2];

// What an actor does with a word of its own: takes its arguments. True if it was one. Both sides run this.
static void take_channel(AtIoTokenizer* t, int side) {
    char name[8];
    const int n = t->ExpectCount();
    for (int i = 0; i < n; i++) {
        t->Next();
        t->ExpectLeftCurly();
        int w = t->Next();
        while (w != AtIoTokenizer::kRightCurly && w != AtIoTokenizer::kEnd) {
            note(side, 'w', w);
            switch (w) {
            case 69: case 110: case 129: t->ExpectName(name); t->ExpectName(name); t->ExpectInt(); break;
            case 71: case 72: t->ExpectReal(); break;
            case 80: t->ExpectInt(); break;
            default: break;
            }
            w = t->Next();
        }
    }
    t->ExpectRightCurly();
}
static bool actor_word(void* self, AtIoTokenizer* t, int token, int side) {
    char name[8];
    switch (token) {
    case 113: case 82: t->ExpectName(name); t->ExpectName(name); break;  // Mesh, Emitter
    case 66: case 67: case 130: case 131: t->ExpectName(name); break;    // the coordinate system databases
    case 105: case 106: case 115: break;                                 // DrawingEnable, DrawingDisable, ModifierReceiver
    case 145: case 146: t->ExpectReal(); t->ExpectReal(); t->ExpectReal(); break;
    case 68: case 133: case 109: take_channel(t, side); break;
    case 111: {                                                          // Scale reaches the scale coordinate system, if there is one
        AtVector3 v;
        v.x = t->ExpectReal(); v.y = t->ExpectReal(); v.z = t->ExpectReal();
        void* cs = ((AtSceneNode*)self)->m_class == kSoundClass ? nullptr : raw(self, 0x80);
        if (cs) {
            if (side == 0) at<void(__fastcall*)(void*, void*, const AtVector3*)>(0x00032B30)(cs, nullptr, &v);
            else ((AtScaleCoordSys*)cs)->SetScale(&v);
        }
        break;
    }
    default: return false;
    }
    note(side, 'b', token);
    return true;
}

// The original's side. Slots 0, 4, 5 and 6 are its own code for actors, 0x00055B80, 0x00055BD0, 0x00055BE0, 0x00055C00, which touch only +0x34, +0x80 and +0x90.
static void __fastcall o_body(void* self, void*, void* owner, AtIoTokenizer* t, int token) {
    if (actor_word(self, t, token, 0)) return;
    at<void(__fastcall*)(void*, void*, void*, void*, int)>(0x0004AA10)(self, nullptr, owner, t, token);
}
static const void* g_actor_table[kAtActorKinds][32];

template <int K> static void* __fastcall o_ctor(void* self, void*) {
    at<Fn0>(0x0004A740)(self, nullptr);
    *(const void***)self = g_actor_table[K];
    *(uint16_t*)((char*)self + 8) = kClasses[K];
    if (K != kAtActorSound) {
        raw(self, 0x80) = nullptr;
        raw(self, 0x90) = nullptr;
    }
    if (kSizes[K] > 0xA0) raw(self, 0xA0) = K == kAtActorJointedModel ? g_joints[0] : nullptr;
    return self;
}
template <int K> static void install_kind() {
    const void** table = g_actor_table[K];
    for (int i = 0; i < 32; i++) table[i] = *(const void**)(uintptr_t)(0x002765A0 + i * 4);
    table[1] = (const void*)o_body;
    if (K != kAtActorSound) {
        table[0] = (const void*)(uintptr_t)0x00055B80;
        table[4] = (const void*)(uintptr_t)0x00055BD0;
        table[5] = (const void*)(uintptr_t)0x00055BE0;
        table[6] = (const void*)(uintptr_t)0x00055C00;
    }
    table[7] = (const void*)(uintptr_t)kOriginalDeleter[K];
    redirect(kOriginalCtor[K], (const void*)o_ctor<K>);
    if (kOriginalDtor[K]) redirect(kOriginalDtor[K], (const void*)(uintptr_t)0x0004AFD0);
}

// The recovered side's double.
class TestActor : public AtSceneNode {
public:
    void ReadHeader(AtScene* scene, AtIoTokenizer* t, int token) override {
        if (token == kAtWordScaleCoordSys && m_class != kSoundClass) {
            if (raw(this, 0x80)) t->Error(kAtProblemWrongUse);
            raw(this, 0x80) = scene->NextScale(t);
        } else {
            AtSceneNode::ReadHeader(scene, t, token);
        }
    }
    void ReadBody(AtNameTableOwner* owner, AtIoTokenizer* t, int token) override {
        if (actor_word(this, t, token, 1)) return;
        AtSceneNode::ReadBody(owner, t, token);
    }
    AtCoordSys* CoordSys() override {
        if (m_class == kSoundClass) return m_coordSys;
        return m_coordSys ? m_coordSys : (AtCoordSys*)raw(this, 0x80);
    }
    AtCoordSys* ParentCoordSys() override {
        if (m_class == kSoundClass) return m_coordSys;
        if (raw(this, 0x90)) return ((AtSceneNode*)raw(this, 0x90))->ParentCoordSys();
        return raw(this, 0x80) ? (AtCoordSys*)raw(this, 0x80) : m_coordSys;
    }
    void FinishHeader() override {
        if (m_class == kSoundClass) return;
        AtCoordSys* scale = (AtCoordSys*)raw(this, 0x80);
        if (scale && m_coordSys) m_coordSys->Attach(scale);
    }
};

unsigned int AtSceneActorsSize(int kind) { return kSizes[kind]; }
void AtSceneActorsConstruct(int kind, void* first, unsigned int count) {
    for (unsigned i = 0; i < count; i++) {
        char* p = (char*)first + i * kSizes[kind];
        new (p) TestActor;
        *(uint16_t*)(p + 8) = kClasses[kind];
        if (kind != kAtActorSound) {
            raw(p, 0x80) = nullptr;
            raw(p, 0x90) = nullptr;
        }
        if (kSizes[kind] > 0xA0) raw(p, 0xA0) = kind == kAtActorJointedModel ? g_joints[1] : nullptr;
    }
}
void AtSceneActorsDestroy(int kind, void* first, unsigned int count) {
    for (unsigned i = count; i > 0; i--) ((TestActor*)((char*)first + (i - 1) * kSizes[kind]))->~TestActor();
}
AtCoordSys* AtSceneActorSharedCoordSys(const AtSceneNode* actor) { return (AtCoordSys*)raw(actor, 0x90); }
GmNameMap* AtSceneActorJoints(const AtSceneNode* actor) { return (GmNameMap*)raw(actor, 0xA0); }

// ---- what the owners find ----------------------------------------------------------------------------------------------------
static void* value_of(const std::string& name, uint32_t salt) {
    uint32_t h = 0x811C9DC5 ^ salt;
    for (const char c : name) h = (h ^ (unsigned char)c) * 16777619;
    return (void*)(uintptr_t)(h | 0x1000);
}

static AtNameTableOwner* g_own[2];
static char* g_fake_jointed[2];
static char* g_fake_plain[2];
static AtCoordSys* g_fake_cs[2];
static AtCoordSys* g_joint_cs[2];
static AtMatrix* fresh_matrix(int side) { return (AtMatrix*)&g_mats[side][g_mat_used[side]++]; }

static AtCoordSys* make_cs(int side) {
    void* p = std::malloc(sizeof(AtRotTransCoordSys));
    if (side == 0) at<Fn0>(0x00026830)(p, nullptr);
    else new (p) AtRotTransCoordSys;
    AtMatrix* a = fresh_matrix(side);
    AtMatrix* b = fresh_matrix(side);
    ((AtCoordSys*)p)->m_local = a;
    ((AtCoordSys*)p)->m_world = b;
    m_identity(a);
    m_identity(b);
    ((AtCoordSys*)p)->m_dirty = 0;
    return (AtCoordSys*)p;
}
static char* make_actor(int side, int kind) {
    char* p = (char*)std::malloc(0x200);  // Room for +0xA0, which the harness sets on a model too, so that a wrong class can be attached to.
    std::memset(p, 0xCD, 0x200);
    if (side == 0) at<void*(__fastcall*)(void*, void*)>(kOriginalCtor[kind])(p, nullptr);
    else AtSceneActorsConstruct(kind, p, 1);
    return p;
}

struct Env {
    std::vector<GmNameMap*> maps[2];
    GmNameMap joints[2];
    explicit Env(const std::set<std::string>& names, const std::set<std::string>& spaces) {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            g_mat_used[side] = 0;
            g_device[side].side = side;
            g_matdev[side].side = side;
            g_pool[side].side = side;
            g_own[side] = new AtNameTableOwner;
            AtNameTableOwner& w = *g_own[side];
            w.m_unknown08 = 0; w.m_loader = 0; w.m_unknown10 = 0; w.m_unknown14 = 0;
            w.m_device = &g_device[side];
            const unsigned pool = (unsigned)spaces.size() + 8;
            w.m_nodeSpaces.Init(pool);
            w.m_modifierSpaces.Init(pool);
            w.m_geometrySpaces.Init(pool);
            g_fake_cs[side] = make_cs(side);
            g_joint_cs[side] = make_cs(side);
            g_fake_jointed[side] = make_actor(side, kAtActorJointedModel);
            g_fake_plain[side] = make_actor(side, kAtActorModel);
            g_joints[side] = &joints[side];
            joints[side].Init((unsigned)names.size() + 1);
            for (const std::string& n : names) joints[side].Insert(n.c_str(), g_joint_cs[side]);
            for (char* f : {g_fake_jointed[side], g_fake_plain[side]}) {
                ((AtSceneNode*)f)->m_coordSys = g_fake_cs[side];
                ((AtSceneNode*)f)->m_owner = g_own[side];
                raw(f, 0xA0) = &joints[side];
                ((AtSceneNode*)f)->m_flags = 0x00100000;  // Attaching to a joint clears this.
            }
            const auto nodes = [&]() {
                GmNameMap* m = new GmNameMap;
                m->Init((unsigned)names.size() + 1);
                for (const std::string& n : names) m->Insert(n.c_str(), n[0] == 'P' ? g_fake_plain[side] : g_fake_jointed[side]);
                maps[side].push_back(m);
                return m;
            };
            const auto fill = [&](uint32_t salt) {
                GmNameMap* m = new GmNameMap;
                m->Init((unsigned)names.size() + 1);
                for (const std::string& n : names) m->Insert(n.c_str(), value_of(n, salt));
                maps[side].push_back(m);
                return m;
            };
            w.m_nodes.Add(nodes());
            w.m_modifiers.Add(fill(1));
            w.m_geometry.Add(fill(2));
            for (const std::string& s : spaces) {
                w.m_nodeSpaces.Add(s.c_str(), nodes());
                w.m_modifierSpaces.Add(s.c_str(), fill(3));
                w.m_geometrySpaces.Add(s.c_str(), fill(4));
            }
        }
        g_recording = was;
    }
    ~Env() {
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            for (GmNameMap* m : maps[side]) {
                m->m_group = nullptr;
                m->Clear();
                delete m;
            }
            joints[side].Clear();
            // The owner's maps are gone; what is left of it is not used again. It is left allocated, along with the fakes, since a scene that crashed may still point at them.
            g_own[side] = nullptr;
        }
    }
};

static void gather(const std::vector<unsigned char>& d, std::set<std::string>& names, std::set<std::string>& spaces) {
    size_t i = 0;
    bool last_was_string = false;
    while (i < d.size()) {
        const unsigned char c = d[i];
        if (c == '\n') { last_was_string = false; i++; continue; }
        if (c == '/' && i + 1 < d.size() && d[i + 1] == '/') { while (i < d.size() && d[i] != '\n') i++; continue; }
        if (c == '"') {
            size_t e = i + 1;
            while (e < d.size() && d[e] != '"' && d[e] != '\n') e++;
            std::string s((const char*)d.data() + i + 1, e - i - 1);
            if (s.size() > 8) s.resize(8);
            const bool after_word = i >= 11 && !std::memcmp(d.data() + i - 10, "NameSpace", 9);
            if (!s.empty() && s[0] != 'X') {
                names.insert(s);
                if (after_word || last_was_string) spaces.insert(s);
            }
            last_was_string = true;
            i = e + 1;
            continue;
        }
        if (c != ' ' && c != '\t' && c != '\r') last_was_string = false;
        i++;
    }
}

// ---- comparing -----------------------------------------------------------------------------------------------------------------
struct Range { const char* lo; const char* hi; uint32_t id; };
static std::vector<Range> g_ranges[2];
static void add_range(int side, const void* p, size_t size, uint32_t id) { if (p) g_ranges[side].push_back({(const char*)p, (const char*)p + size, id}); }
static uint64_t canon(int side, const void* p) {
    if (!p) return 0;
    for (const Range& r : g_ranges[side]) {
        if ((const char*)p >= r.lo && (const char*)p < r.hi) return (uint64_t(r.id) << 32) | (uint32_t)((const char*)p - r.lo);
    }
    const Mat* m = (const Mat*)p;
    if (m >= g_mats[side] && m < g_mats[side] + kMats) return (uint64_t(0xEE) << 32) | (uint32_t)(m - g_mats[side]);
    return (uintptr_t)p;
}
static uint64_t canon_known(int side, const void* p) {
    const uint64_t c = canon(side, p);
    return c == (uintptr_t)p ? 0xBAD : c;
}
static void same_ptr(const char* what, const void* o, const void* r) { eq(what, canon(0, o), canon(1, r)); }

static uint32_t dword(const void* p, unsigned off) { return *(const uint32_t*)((const char*)p + off); }

static const void* g_rot_table, *g_scale_table;

static void same_cs(const AtCoordSys* o, const AtCoordSys* r, unsigned size) {
    const uint32_t ov = dword(o, 0);
    const void* rv = *(void* const*)r;
    eq("cs class", ov == 0x00273100 ? 1 : ov == 0x00274B08 ? 2 : ov == 0x00274AE8 ? 3 : 0, rv == g_rot_table ? 1 : rv == g_scale_table ? 2 : 3);
    same_ptr("cs parent", o->m_parent, r->m_parent);
    same_ptr("cs sibling", o->m_nextSibling, r->m_nextSibling);
    same_ptr("cs child", o->m_firstChild, r->m_firstChild);
    eq("cs +10", (uintptr_t)o->m_drawingLocal, (uintptr_t)r->m_drawingLocal);
    same_ptr("cs local", o->m_local, r->m_local);
    same_ptr("cs world", o->m_world, r->m_world);
    eq("cs +1C", (uintptr_t)o->m_drawingWorld, (uintptr_t)r->m_drawingWorld);
    eq("cs +20", o->m_unknown20, r->m_unknown20);
    eq("cs dirty", o->m_dirty, r->m_dirty);
    for (unsigned off = 0x28; off < size; off += 4) eq("cs scale", dword(o, off), dword(r, off));
}

static void same_node(const void* o, const void* r, unsigned size) {
    for (unsigned off = 4; off < size; off += 4) {
        switch (off) {
        case 0x30:
            eq("node's owner", raw(o, off) == g_own[0], raw(r, off) == g_own[1]);
            break;
        case 0x34: case 0x38: case 0x44: case 0x48: case 0x4C: case 0x50: case 0x54: case 0x58: case 0x5C: case 0x80: case 0xA0: case 0x90:
            // Unknown when past the end of an array, or left behind when a node was attached twice: both sides hold such a pointer, and neither reads it here.
            eq("node pointer", canon_known(0, raw(o, off)), canon_known(1, raw(r, off)));
            break;
        default:
            eq("node", dword(o, off), dword(r, off));
            break;
        }
    }
}

static void same_map(const GmNameMap* o, const GmNameMap* r) {
    eq("map's entries", o->m_entries != nullptr, r->m_entries != nullptr);
    eq("map's size", o->m_size, r->m_size);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                // A list given twice leaves the first array allocated and its names in the map; those are compared as such.
                eq("slot's value", canon_known(0, x.m_value), canon_known(1, y.m_value));
            }
        }
    }
}

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static void compare_side_effects(const char* what) {
    eq("matrices used", g_mat_used[0], g_mat_used[1]);
    if (g_mat_used[0] == g_mat_used[1]) eq("matrices", std::memcmp(g_mats[0], g_mats[1], g_mat_used[0] * sizeof(Mat)), 0);
    eq("hash", (long long)g_hash[0], (long long)g_hash[1]);
    compare_logs(what);
}

struct ActorField { unsigned count, array; };
static const ActorField kActorFields[kAtActorKinds] = {{0x74, 0xC0}, {0x78, 0xC4}, {0x8C, 0xD8}, {0x90, 0xDC}, {0x94, 0xE0}, {0x98, 0xE4}, {0x9C, 0xE8}, {0xA0, 0xEC}, {0xA4, 0xF0}};

static void register_scene(int side, const AtScene* s) {
    g_ranges[side].clear();
    add_range(side, g_fake_jointed[side], kSizes[kAtActorJointedModel], 1);
    add_range(side, g_fake_plain[side], kSizes[kAtActorModel], 2);
    add_range(side, g_fake_cs[side], sizeof(AtRotTransCoordSys), 3);
    add_range(side, g_joint_cs[side], sizeof(AtRotTransCoordSys), 4);
    add_range(side, g_joints[side], sizeof(GmNameMap), 40);
    add_range(side, s->m_coordSys.m_rotTrans, (size_t)s->m_coordSys.m_rotTransCount * sizeof(AtRotTransCoordSys), 5);
    add_range(side, s->m_coordSys.m_scaled, (size_t)s->m_coordSys.m_scaledCount * sizeof(AtScaleCoordSys), 6);
    add_range(side, s->m_motionStates, (size_t)s->m_motionCount * sizeof(AtSceneMotionState), 7);
    add_range(side, s->m_nodes, (size_t)s->m_nodeCount * 0x6C, 8);
    add_range(side, s->m_modifierNodes, (size_t)s->m_modifierNodeCount * 0x70, 9);
    for (int k = 0; k < kAtActorKinds; k++) add_range(side, raw(s, kActorFields[k].array), (size_t)dword(s, kActorFields[k].count) * kSizes[k], 10 + k);
}

static unsigned g_nodes_seen, g_files, g_actors_seen;

static void same_scene(const char* what, const AtScene* o, const AtScene* r) {
    g_recording = false;
    register_scene(0, o);
    register_scene(1, r);
    same_map(o, r);
    eq("scene in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("scene in the plain group", o->m_group == &g_own[0]->m_nodes, r->m_group == &g_own[1]->m_nodes);
    eqs("scene's group", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    eqs("last space", group_name(g_own[0]->m_nodeSpaces.m_last).c_str(), group_name(g_own[1]->m_nodeSpaces.m_last).c_str());
    const AtCoordSysDatabase& a = o->m_coordSys;
    const AtCoordSysDatabase& b = r->m_coordSys;
    same_map(&a, &b);
    eq("database device", a.m_device == &g_matdev[0], b.m_device == &g_matdev[1]);
    eq("database pool", a.m_pool == &g_pool[0], b.m_pool == &g_pool[1]);
    eq("rot count", a.m_rotTransCount, b.m_rotTransCount);
    eq("scale count", a.m_scaledCount, b.m_scaledCount);
    eq("rot array", a.m_rotTrans != nullptr, b.m_rotTrans != nullptr);
    eq("scale array", a.m_scaled != nullptr, b.m_scaled != nullptr);
    eq("database +2C", (uintptr_t)a.m_root, (uintptr_t)b.m_root);
    eq("extra", a.m_extra, b.m_extra);
    eq("next matrix", a.m_nextMatrix, b.m_nextMatrix);
    for (unsigned off = 0x4C; off < 0xF4; off += 4) {
        const bool pointer = off == 0x4C || off >= 0xB4;
        if (pointer) eq("scene's array", raw(o, off) != nullptr, raw(r, off) != nullptr);
        else eq("scene's count", dword(o, off), dword(r, off));
    }
    eq("scene's owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
    if (a.m_rotTrans && b.m_rotTrans && a.m_rotTransCount == b.m_rotTransCount) {
        for (unsigned i = 0; i < a.m_rotTransCount; i++) same_cs(&a.m_rotTrans[i], &b.m_rotTrans[i], sizeof(AtRotTransCoordSys));
        eq("rot array's count", dword(a.m_rotTrans, (unsigned)-4), dword(b.m_rotTrans, (unsigned)-4));
    }
    if (a.m_scaled && b.m_scaled && a.m_scaledCount == b.m_scaledCount) {
        for (unsigned i = 0; i < a.m_scaledCount; i++) same_cs(&a.m_scaled[i], &b.m_scaled[i], sizeof(AtScaleCoordSys));
        eq("scale array's count", dword(a.m_scaled, (unsigned)-4), dword(b.m_scaled, (unsigned)-4));
    }
    same_cs(g_fake_cs[0], g_fake_cs[1], sizeof(AtRotTransCoordSys));
    same_cs(g_joint_cs[0], g_joint_cs[1], sizeof(AtRotTransCoordSys));
    same_node(g_fake_jointed[0], g_fake_jointed[1], kSizes[kAtActorJointedModel]);
    same_node(g_fake_plain[0], g_fake_plain[1], kSizes[kAtActorModel]);
    if (o->m_motionStates && r->m_motionStates && o->m_motionCount == r->m_motionCount) eq("motion states", std::memcmp(o->m_motionStates, r->m_motionStates, o->m_motionCount * sizeof(AtSceneMotionState)), 0);
    const auto nodes = [&](const void* x, const void* y, unsigned count, unsigned size) {
        if (x && y) {
            eq("node array's count", dword(x, (unsigned)-4), dword(y, (unsigned)-4));
            for (unsigned i = 0; i < count; i++) {
                same_node((const char*)x + i * size, (const char*)y + i * size, size);
                g_nodes_seen++;
            }
        }
    };
    if (o->m_nodeCount == r->m_nodeCount) nodes(o->m_nodes, r->m_nodes, o->m_nodeCount, 0x6C);
    if (o->m_modifierNodeCount == r->m_modifierNodeCount) nodes(o->m_modifierNodes, r->m_modifierNodes, o->m_modifierNodeCount, 0x70);
    for (int k = 0; k < kAtActorKinds; k++) {
        if (dword(o, kActorFields[k].count) == dword(r, kActorFields[k].count)) nodes(raw(o, kActorFields[k].array), raw(r, kActorFields[k].array), dword(o, kActorFields[k].count), kSizes[k]);
    }
    compare_side_effects(what);
}

// ---- files -----------------------------------------------------------------------------------------------------------------------
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);

static AtScene* make_o(unsigned char fill) {
    void* p = std::malloc(sizeof(AtScene));
    std::memset(p, fill, sizeof(AtScene));
    { On on(0); at<Fn0>(0x0003BC70)(p, nullptr); }
    return (AtScene*)p;
}
static AtScene* make_r(unsigned char fill) {
    void* p = std::malloc(sizeof(AtScene));
    std::memset(p, fill, sizeof(AtScene));
    { On on(1); new (p) AtScene; }
    return (AtScene*)p;
}

static void load_o(AtScene* s) { g_reports = 0; On on(0); at<FnLoad>(0x0003CD60)(s, nullptr, g_own[0], "ART\\THING", 77); }
static void load_r(AtScene* s) { g_reports = 0; On on(1); s->Load(g_own[1], "ART\\THING", 77); }

static void destroy(AtScene* o, AtScene* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002E640)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~AtScene(); }
    compare_side_effects(what);
    // What was left is as a fresh one: nothing but the vftable differs, and the owner the constructor leaves.
    std::free(o);
    std::free(r);
}

static void run_text(const std::vector<unsigned char>& data, const std::string& context, unsigned char fill, bool crashes_allowed) {
    g_context = context;
    if (getenv("KND_TRACE")) std::printf("text [%s]\n", context.c_str());
    std::set<std::string> names, spaces;
    gather(data, names, spaces);
    spaces.insert("Space");
    Env env(names, spaces);
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    AtScene* o = make_o(fill);
    AtScene* r = make_r(fill);
    {
        unsigned char a[sizeof(AtScene)], b[sizeof(AtScene)];
        std::memcpy(a, o, sizeof a);
        std::memcpy(b, r, sizeof b);
        for (const unsigned off : {0u, 0x14u}) { std::memset(a + off, 0, 4); std::memset(b + off, 0, 4); }
        eq("constructed", std::memcmp(a, b, sizeof a), 0);
    }
    bool oc = guarded([&] { load_o(o); });
    g_recording = false;
    bool rc = guarded([&] { load_r(r); });
    g_recording = false;
    eq("crash", oc, rc);
    if (!crashes_allowed && oc) std::printf("unexpected crash on %s\n", context.c_str());
    if (oc || rc) {
        // After a crash the two are comparable only as far as it: what was logged and said. The scene stays allocated.
        compare_logs("crash");
        g_hash[0] = g_hash[1] = 0;
        return;
    }
    same_scene("load", o, r);
    for (int k = 0; k < kAtActorKinds; k++) g_actors_seen += dword(r, kActorFields[k].count);
    // Over what is there.
    oc = guarded([&] { load_o(o); });
    g_recording = false;
    rc = guarded([&] { load_r(r); });
    g_recording = false;
    eq("crash on the second load", oc, rc);
    if (oc || rc) { compare_logs("crash on the second load"); g_hash[0] = g_hash[1] = 0; return; }
    same_scene("load again", o, r);
    { On on(0); at<Fn0>(0x0003D6F0)(o, nullptr); at<Fn0>(0x0003D6F0)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_scene("clear", o, r);
    destroy(o, r, "destroy");
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static const char* const kArrays[] = {"Node", "ModifierNode", "ModelActor", "JointedModelActor", "PhysicsJointedModelActor", "ParticleSystemActor", "WaterActor", "LensFlareActor", "GroupActor", "SwitchGroupActor", "LodGroupActor", "SoundNode"};
static const char kHead[] = "RotTransCoordSys[6]\nScaleCoordSys[2]\nMotionState[3]\nNode[9]\n";

// One node with these lines inside it.
static std::string one(const std::string& cls, const std::string& header, const std::string& body) {
    return std::string(kHead) + cls + "[1]\n{\n " + cls + " \"Self\" " + header + "\n {\n" + body + "\n }\n}\n";
}

static void run_written() {
    static const char* const heads[] = {
        "Node[1]\n", "RotTransCoordSys[1]\nNode[1]\n", "ScaleCoordSys[1]\nNode[1]\n", "MotionState[1]\nNode[1]\n", "RotTransCoordSys[2]\nScaleCoordSys[3]\nMotionState[4]\nNode[5]\n",
        "MotionState[2]\nScaleCoordSys[1]\nRotTransCoordSys[1]\nNode[1]\n", "RotTransCoordSys[1]\nRotTransCoordSys[3]\nNode[1]\n", "ScaleCoordSys[1]\nScaleCoordSys[3]\nNode[1]\n", "MotionState[1]\nMotionState[3]\nNode[1]\n",
        "RotTransCoordSys[0]\nScaleCoordSys[0]\nMotionState[0]\nNode[1]\n", "Node[0]\n", "Mesh\nNode[1]\n", "5\nNode[1]\n", "\"text\"\nNode[1]\n", "NameSpace \"Space\"\nNode[1]\n", "NameSpace \"\"\nNode[1]\n",
        "NameSpace \"LongerThanEight\"\nNode[1]\n", "NameSpace \"Space\"\nRotTransCoordSys[2]\nNode[2]\n", "RotTransCoordSys[1\nNode[1]\n", "RotTransCoordSys 1]\nNode[1]\n", "Node 1]\n", "Node[1\n", "Node[x]\n", "RotTransCoordSys[x]\nNode[1]\n",
    };
    for (const char* h : heads) run_text(bytes(h), h, 0xCD, true);

    for (const char* word : kArrays) {
        const std::string w = word;
        for (const std::string& text : {
                 std::string(kHead) + w + "[0]\n{\n}\n",
                 std::string(kHead) + w + "[1]\n{\n " + w + " \"a\" RotTransCoordSys\n {\n }\n}\n",
                 std::string(kHead) + w + "[2]\n{\n " + w + " \"a\" RotTransCoordSys MotionState\n {\n }\n " + w + " \"LongerThanEight\"\n {\n }\n}\n",
                 std::string(kHead) + w + "[1]\n{\n " + w + " \"a\"\n {\n }\n}\n" + w + "[1]\n{\n " + w + " \"b\"\n {\n }\n}\n",
                 std::string(kHead) + w + "[1]\n{\n " + w + " \"a\"\n {\n }\n}\n" + w + "[0]\n{\n}\n",
                 std::string(kHead) + w + "[0]\n{\n}\n" + w + "[1]\n{\n " + w + " \"b\"\n {\n }\n}\n",
                 std::string(kHead) + w + "[2]\n{\n " + w + " \"a\"\n {\n }\n}\nNode[0]\n{\n}\n",
                 std::string(kHead) + w + "[1]\n{\n Other \"a\"\n {\n }\n}\n",
                 std::string(kHead) + w + "[1]\n{\n " + w + " \"a\"\n {\n }\n",
                 std::string(kHead) + w + " 1]\n{\n}\n",
             }) {
            run_text(bytes(text), text, 0xCD, true);
        }
    }
    {
        const std::string text = std::string(kHead) + "Mesh\n5\n";
        run_text(bytes(text), text, 0xCD, true);
        const std::string everything = std::string("RotTransCoordSys[24]\nScaleCoordSys[2]\nMotionState[3]\nNode[30]\n");
        std::string all = everything;
        for (const char* word : kArrays) {
            const std::string w = word;
            if (w == "PhysicsJointedModelActor") continue;
            all += w + "[2]\n{\n " + w + " \"" + w.substr(0, 3) + "1\" RotTransCoordSys\n {\n Position 1 2 3\n }\n " + w + " \"" + w.substr(0, 3) + "2\" RotTransCoordSys\n {\n }\n}\n";
        }
        run_text(bytes(all), "every class, in a file", 0xCD, false);
    }

    static const char* const headers[] = {
        "", "RotTransCoordSys", "MotionState", "RotTransCoordSys MotionState", "MotionState RotTransCoordSys", "RotTransCoordSys RotTransCoordSys", "MotionState MotionState", "Mesh", "5", "\"text\"",
        "RotTransCoordSys RotTransCoordSys RotTransCoordSys RotTransCoordSys RotTransCoordSys RotTransCoordSys", "MotionState MotionState MotionState",
    };
    for (const char* cls : {"Node", "ModifierNode", "ModelActor"}) {
        for (const char* h : headers) { const std::string t = one(cls, h, ""); run_text(bytes(t), t, 0xCD, true); }
    }
    {
        // Past the end of each, and with none.
        for (const std::string& t : {
                 std::string("RotTransCoordSys[1]\nNode[3]\nNode[2]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n Node \"b\" RotTransCoordSys\n {\n }\n}\n"),
                 std::string("Node[1]\nNode[1]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n}\n"),
                 std::string("MotionState[1]\nNode[3]\nNode[2]\n{\n Node \"a\" MotionState\n {\n }\n Node \"b\" MotionState\n {\n }\n}\n"),
                 std::string("Node[1]\nNode[1]\n{\n Node \"a\" MotionState\n {\n }\n}\n"),
                 std::string("ScaleCoordSys[1]\nNode[3]\nModelActor[2]\n{\n ModelActor \"a\" ScaleCoordSys\n {\n }\n ModelActor \"b\" ScaleCoordSys\n {\n }\n}\n"),
                 std::string("Node[1]\nModelActor[1]\n{\n ModelActor \"a\" ScaleCoordSys\n {\n }\n}\n"),
             }) {
            run_text(bytes(t), t, 0xCD, true);
        }
        const std::string scaled = std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\" RotTransCoordSys ScaleCoordSys\n {\n Scale 1.5 2.5 3.5\n BoundingSphere 1.5 2.5 3.5 4.5\n }\n ModelActor \"b\" ScaleCoordSys\n {\n Scale 2.5 2.5 2.5\n }\n}\n";
        run_text(bytes(scaled), scaled, 0xCD, false);
    }

    static const char* const bodies[] = {
        "", "Mesh \"a\" \"b\"", "5", "Other",
        "CollisionEnable", "CollisionDisable", "CollisionEnable\nCollisionDisable", "LineSegIntersectEnable", "LineSegIntersectDisable", "LineSegIntersectEnable\nLineSegIntersectDisable",
        "BoundingSphere 1.5 -2.5 3.25 4.75", "BoundingSphere 1 2 3 x", "ActiveDistance 3.5", "ActiveDistance 0", "BoundingGeometryMatAssignment 3", "BoundingSphereMatAssignment 4", "BoundingGeometryMatAssignment x",
        "EnableBoundingGeometryDraw", "DisableBoundingGeometryDraw", "EnableNodeBoundingSphereDraw", "DisableNodeBoundingSphereDraw",
        "Position 1.5 2.5 3.5", "Quaternion 0.1 0.2 0.3 0.9", "OrthoNormalize 1.5 2.5 3.5 4.5 5.5 6.5", "LookAt 1.5 2.5 3.5 7.25 9.75 11.5 0.5 1.5 0.25", "LinearVelocity 1.5 2.5 3.5", "AngularVelocity 4.5 5.5 6.5", "Position 1 2 3",
        "BoundingGeometry \"Geo\" \"\"", "BoundingGeometry \"Geo\" \"Space\"", "BoundingGeometry \"XGone\" \"\"", "BoundingGeometry \"Geo\" \"XNone\"",
        "Attach \"Target\" \"\" \"Joint\"", "Attach \"Target\" \"Space\" \"Joint\"", "Attach \"XGone\" \"\" \"Joint\"", "Attach \"PlainOne\" \"\" \"Joint\"", "Attach \"Target\" \"\" \"XJoint\"", "Attach \"Target\" \"\" \"Joint\"\nAttach \"Target\" \"\" \"Joint\"",
        "AttachBase \"Target\" \"\"", "AttachBase \"Target\" \"Space\"", "AttachBase \"XGone\" \"\"", "AttachBase \"Target\" \"\"\nAttachBase \"Target\" \"\"", "AttachBase \"Target\" \"\"\nAttachBase \"PlainOne\" \"\"", "AttachBase \"Target\" \"\"\nPosition 1.5 2.5 3.5",
        "AttachNodeToThisJoint \"Target\" \"\" \"Joint\"", "AttachNodeToThisJoint \"XGone\" \"\" \"Joint\"", "AttachNodeToThisBase \"Target\" \"\"", "AttachNodeToThisBase \"XGone\" \"\"",
        // Not run: attaching a node to itself, whose coordinate system then recurses for ever on both sides, past what the guard can catch.
    };
    for (const char* cls : {"Node", "ModifierNode", "ModelActor", "JointedModelActor"}) {
        for (const char* h : {"RotTransCoordSys MotionState", ""}) {
            for (const char* b : bodies) { const std::string t = one(cls, h, b); run_text(bytes(t), t, 0xCD, true); }
        }
    }
    // Between nodes of one file, in both orders of who names whom.
    for (const std::string& t : {
             std::string(kHead) + "Node[3]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n Node \"b\" RotTransCoordSys\n {\n AttachBase \"a\" \"\"\n }\n Node \"c\" RotTransCoordSys\n {\n AttachBase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "Node[3]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n Node \"b\" RotTransCoordSys\n {\n AttachBase \"a\" \"\"\n }\n Node \"c\" RotTransCoordSys\n {\n AttachBase \"b\" \"\"\n }\n}\n",
             std::string(kHead) + "Node[2]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n Node \"b\" RotTransCoordSys\n {\n AttachNodeToThisBase \"a\" \"\"\n }\n}\n",
             std::string(kHead) + "JointedModelActor[2]\n{\n JointedModelActor \"a\" RotTransCoordSys\n {\n }\n JointedModelActor \"b\" RotTransCoordSys\n {\n AttachNodeToThisJoint \"Target\" \"\" \"Joint\"\n }\n}\n",
             std::string("NameSpace \"Space\"\n") + kHead + "Node[2]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n Node \"b\" RotTransCoordSys\n {\n AttachBase \"a\" \"Space\"\n }\n}\n",
             std::string(kHead) + "ModelActor[2]\n{\n ModelActor \"a\" RotTransCoordSys ScaleCoordSys\n {\n }\n ModelActor \"b\" RotTransCoordSys\n {\n AttachBase \"a\" \"\"\n }\n}\n",
         }) {
        run_text(bytes(t), t, 0xCD, false);
    }
    // The modifier node's words.
    for (const char* b : {"Modifier \"Mod\" \"\"", "Modifier \"Mod\" \"Space\"", "Modifier \"XGone\" \"\"", "Modifier \"Mod\" \"XNone\"", "ModificationEnable", "ModificationDisable", "ModificationEnable\nModificationDisable", "ModificationEnable\nPosition 1 2 3"}) {
        const std::string t = one("ModifierNode", "RotTransCoordSys", b);
        run_text(bytes(t), t, 0xCD, true);
        const std::string u = one("Node", "RotTransCoordSys", b);
        run_text(bytes(u), u, 0xCD, true);
    }
    // Under another fill, so that nothing rests on what the heap held.
    {
        const std::string t = one("Node", "RotTransCoordSys MotionState", "Position 1.5 2.5 3.5\nCollisionEnable\nLinearVelocity 1.5 2.5 3.5");
        run_text(bytes(t), t, 0x5A, false);
        run_text(bytes(t), t, 0, false);
    }
}

// ---- no heap, no memory, no file -----------------------------------------------------------------------------------------------------
static void set_heaps(void* scene, void* node, void* db) {
    *(void**)(uintptr_t)0x002C640C = scene;
    AtScene::s_heap = scene;
    *(void**)(uintptr_t)0x002C7E0C = node;
    AtSceneNode::s_heap = node;
    *(void**)(uintptr_t)0x002C6344 = db;
    AtCoordSysDatabase::s_heap = db;
}

static void run_shortages() {
    std::string text = std::string(kHead);
    for (const char* word : kArrays) {
        const std::string w = word;
        if (w == "PhysicsJointedModelActor") continue;
        text += w + "[0]\n{\n}\n";
    }
    const std::string counted = std::string(kHead) + "Node[1]\n{\n Node \"a\" RotTransCoordSys\n {\n }\n}\nModifierNode[1]\n{\n ModifierNode \"b\"\n {\n }\n}\nModelActor[1]\n{\n ModelActor \"c\"\n {\n }\n}\n";

    set_heaps(nullptr, nullptr, nullptr);
    g_refuse_heap = nullptr;
    run_text(bytes(text), "no heap for anything", 0xCD, true);
    run_text(bytes(counted), "no heap for anything, with nodes", 0xCD, true);
    set_heaps(&g_heap_scene, &g_heap_node, nullptr);
    run_text(bytes(text), "no heap for the database", 0xCD, true);
    set_heaps(nullptr, &g_heap_node, &g_heap_db);
    run_text(bytes(text), "no heap for the scene", 0xCD, true);
    set_heaps(&g_heap_scene, &g_heap_node, &g_heap_db);

    g_refuse_heap = &g_heap_scene;
    run_text(bytes(text), "no memory for the scene", 0xCD, true);
    run_text(bytes(counted), "no memory for the scene, with nodes", 0xCD, true);
    g_refuse_heap = &g_heap_db;
    run_text(bytes("RotTransCoordSys[1]\nNode[1]\n"), "no memory for rot", 0xCD, true);
    run_text(bytes("ScaleCoordSys[1]\nNode[1]\n"), "no memory for scale", 0xCD, true);
    g_refuse_heap = nullptr;
    g_fail_alloc = false;
    g_refusing = 0;

    // A file that will not open: the report, and then the reader on no tokenizer. In the game the report does not come back.
    for (int status : {1, 2, 5}) {
        g_fs->open_status = status;
        run_text(bytes(""), "no file", 0xCD, true);
        g_fs->open_status = 0;
    }
}

// ---- driving the classes directly ----------------------------------------------------------------------------------------------------
typedef void(__fastcall* FnP)(void*, void*, const void*);
typedef void(__fastcall* FnPP)(void*, void*, const void*, const void*);
typedef void(__fastcall* FnU)(void*, void*, unsigned);

static void same_tree(AtCoordSys** o, AtCoordSys** r, unsigned n, const unsigned* sizes) {
    g_recording = false;
    for (unsigned i = 0; i < n; i++) same_cs(o[i], r[i], sizes[i]);
    compare_side_effects("coordinate systems");
}

static void run_coordsys(unsigned seed, unsigned ops) {
    g_context = "coordinate systems, seed " + std::to_string(seed);
    std::mt19937 rng(seed);
    enum { kN = 12 };
    AtCoordSys* o[kN];
    AtCoordSys* r[kN];
    unsigned sizes[kN];
    g_mat_used[0] = g_mat_used[1] = 0;
    g_ranges[0].clear();
    g_ranges[1].clear();
    for (unsigned i = 0; i < kN; i++) {
        const unsigned kind = i % 3;
        sizes[i] = kind == 1 ? 0x34 : 0x28;
        void* a = std::malloc(0x34);
        void* b = std::malloc(0x34);
        std::memset(a, 0xCD, 0x34);
        std::memset(b, 0xCD, 0x34);
        {
            On on(0);
            if (kind == 0) at<Fn0>(0x00026830)(a, nullptr);
            else if (kind == 1) at<Fn0>(0x00032AE0)(a, nullptr);
            else at<Fn0>(0x00032460)(a, nullptr);
        }
        {
            On on(1);
            if (kind == 0) new (b) AtRotTransCoordSys;
            else if (kind == 1) new (b) AtScaleCoordSys;
            else new (b) AtCoordSys;
        }
        o[i] = (AtCoordSys*)a;
        r[i] = (AtCoordSys*)b;
        add_range(0, a, 0x34, 100 + i);
        add_range(1, b, 0x34, 100 + i);
    }
    same_tree(o, r, kN, sizes);  // As constructed.
    const auto matrices = [&](unsigned i) {
        AtMatrix* a0 = fresh_matrix(0); AtMatrix* b0 = fresh_matrix(0);
        AtMatrix* a1 = fresh_matrix(1); AtMatrix* b1 = fresh_matrix(1);
        { On on(0); at<FnPP>(0x000324A0)(o[i], nullptr, a0, b0); }
        { On on(1); r[i]->SetMatrices(a1, b1); }
    };
    for (unsigned i = 0; i < kN; i++) matrices(i);
    same_tree(o, r, kN, sizes);
    for (unsigned n = 0; n < ops; n++) {
        const unsigned i = rng() % kN, j = rng() % kN;
        const unsigned op = rng() % 18;
        AtVector3 v, w;
        v.x = (float)(int)(rng() % 100) / 7.0f; v.y = (float)(int)(rng() % 100) / 3.0f + 1.0f; v.z = (float)(rng() % 9 + 1);
        w.x = v.y; w.y = v.z; w.z = v.x;
        AtQuaternion q;
        q.x = v.x; q.y = v.y; q.z = v.z; q.w = w.x;
        AtVector3 out[2];
        out[0] = v; out[1] = v;
        int ret[2] = {0, 0};
        float scale[2] = {0, 0};
        for (int side = 0; side < 2; side++) {
            AtCoordSys** s = side == 0 ? o : r;
            On on(side);
            switch (op) {
            case 0: case 1: case 2: if (i < j) s[i]->Attach(s[j]); break;  // Virtual, so the original's own through its vftable. A parent has the lower index: a cycle recurses for ever, on both sides.
            case 3: s[i]->Detach(s[j]); break;
            case 4: s[i]->Invalidate(); break;
            case 5: ret[side] = s[i]->UpdateInverse(); break;
            case 6: s[i]->Visit(nullptr); break;
            case 7: scale[side] = s[i]->ScaleFactor(); break;
            case 8:
                if (side == 0) at<FnU>(0x000322D0)(s[i], nullptr, 1u << (j % 6));
                else s[i]->MarkDirty(1u << (j % 6));
                break;
            case 9:
                if (side == 0) ret[0] = at<FnI>(0x000325A0)(s[i], nullptr);
                else ret[1] = s[i]->UpdateWorld();
                break;
            case 10:
                if (side == 0) at<FnPP>(0x00032780)(s[i], nullptr, &v, &out[0]);
                else s[i]->Transform(&v, &out[1]);
                break;
            case 11:
                if (side == 0) at<FnP>(0x00032910)(s[i], nullptr, s[j]);
                else s[i]->CopyLocal(s[j]);
                break;
            case 12:
                if (side == 0) at<FnP>(0x000272E0)(s[i], nullptr, &q);
                else s[i]->SetRotation(&q);
                break;
            case 13:
                if (side == 0) at<FnP>(0x00027310)(s[i], nullptr, &v);
                else s[i]->SetPosition(&v);
                break;
            case 14:
                if (side == 0) at<FnPP>(0x00049960)(s[i], nullptr, &v, &w);
                else s[i]->SetLookAt(&v, &w);
                break;
            case 15:
                if (i % 3 == 1) {
                    if (side == 0) at<FnP>(0x00032B30)(s[i], nullptr, &v);
                    else ((AtScaleCoordSys*)s[i])->SetScale(&v);
                }
                break;
            case 16:
                // Everything a reset clears, made to show, including the fields only a reset writes.
                if (n % 40 == 0) { s[i]->m_drawingLocal = (AtMatrix*)5; s[i]->m_drawingWorld = (AtMatrix*)6; s[i]->m_unknown20 = 7; s[i]->Reset(); }
                break;
            }
        }
        eq("returned", ret[0], ret[1]);
        eq("scale factor", bits(scale[0]), bits(scale[1]));
        eq("transformed", std::memcmp(&out[0], &out[1], sizeof(AtVector3)), 0);
        if (op == 16 && n % 40 == 0) { same_tree(o, r, kN, sizes); matrices(i); }
        if (op == 17) matrices(i);  // Over what it has: a reset first.
        same_tree(o, r, kN, sizes);
    }
    // Attach to what it is already attached to, and detach what is not attached.
    for (unsigned i = 1; i < kN; i++) {
        for (int side = 0; side < 2; side++) {
            AtCoordSys** s = side == 0 ? o : r;
            On on(side);
            s[0]->Attach(s[i]);
            s[0]->Attach(s[i]);
        }
        same_tree(o, r, kN, sizes);
    }
    // Destroying a parent lets go of its children, through the deleting destructors.
    for (unsigned i = 0; i < kN; i++) {
        { On on(0); delete o[i]; }
        { On on(1); delete r[i]; }
        g_recording = false;
        g_ranges[0].erase(std::remove_if(g_ranges[0].begin(), g_ranges[0].end(), [&](const Range& x) { return x.id == 100 + i; }), g_ranges[0].end());
        g_ranges[1].erase(std::remove_if(g_ranges[1].begin(), g_ranges[1].end(), [&](const Range& x) { return x.id == 100 + i; }), g_ranges[1].end());
        for (unsigned k = i + 1; k < kN; k++) same_cs(o[k], r[k], sizes[k]);
        compare_side_effects("destroyed");
    }
}

static void run_small() {
    g_context = "small";
    g_mat_used[0] = g_mat_used[1] = 0;
    g_ranges[0].clear();
    g_ranges[1].clear();

    // In-place destructors of the coordinate systems.
    {
        unsigned char a[0x34], b[0x34];
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        { On on(0); at<Fn0>(0x00026830)(a, nullptr); at<Fn0>(0x00026850)(a, nullptr); }
        { On on(1); new (b) AtRotTransCoordSys; ((AtRotTransCoordSys*)b)->~AtRotTransCoordSys(); }
        eq("destroyed", std::memcmp(a + 4, b + 4, sizeof a - 4), 0);
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        { On on(0); at<Fn0>(0x00032460)(a, nullptr); at<Fn0>(0x00032490)(a, nullptr); }
        { On on(1); new (b) AtCoordSys; ((AtCoordSys*)b)->~AtCoordSys(); }
        eq("destroyed", std::memcmp(a + 4, b + 4, sizeof a - 4), 0);
        // A scale's reset.
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        AtVector3 v; v.x = 2; v.y = 3; v.z = 4;
        AtMatrix* m0 = fresh_matrix(0); AtMatrix* m1 = fresh_matrix(1);
        { On on(0); at<Fn0>(0x00032AE0)(a, nullptr); ((AtCoordSys*)a)->m_local = m0; at<FnP>(0x00032B30)(a, nullptr, &v); ((AtCoordSys*)a)->Reset(); }
        { On on(1); new (b) AtScaleCoordSys; ((AtCoordSys*)b)->m_local = m1; ((AtScaleCoordSys*)b)->SetScale(&v); ((AtCoordSys*)b)->Reset(); }
        eq("reset scale", std::memcmp(a + 4, b + 4, sizeof a - 4), 0);
        compare_side_effects("in place");
    }
    // The array deleters, with a count and on one element, by way of the vftable; and the scalar ones.
    {
        for (int scaled = 0; scaled < 2; scaled++) {
            const unsigned size = scaled ? 0x34 : 0x28;
            for (unsigned flags : {0u, 1u, 2u, 3u}) {
                for (int side = 0; side < 2; side++) {
                    On on(side);
                    const unsigned count = 3;
                    // The scalar forms free the object itself, the array forms the word before it.
                    const unsigned ahead = flags & 2 ? 4 : 0;
                    uint32_t* block = (uint32_t*)std::malloc(ahead + count * size + 64);
                    std::memset(block, 0xCD, ahead + count * size + 64);
                    if (ahead) *block = count;
                    void* first = (char*)block + ahead;
                    for (unsigned i = 0; i < count; i++) {
                        void* e = (char*)first + i * size;
                        if (side == 0) at<Fn0>(scaled ? 0x00032AE0 : 0x00026830)(e, nullptr);
                        else if (scaled) new (e) AtScaleCoordSys;
                        else new (e) AtRotTransCoordSys;
                    }
                    if (side == 0) {
                        at<FnDel>(scaled ? 0x00026BB0 : 0x00026B50)(first, nullptr, flags);
                    } else {
                        // Modern compilers do not emit the array path unless new[] is used, so the recovered side's is what the database does: each element, last first.
                        if (flags & 2) {
                            for (unsigned i = count; i > 0; i--) {
                                if (scaled) ((AtScaleCoordSys*)((char*)first + (i - 1) * size))->~AtScaleCoordSys();
                                else ((AtRotTransCoordSys*)((char*)first + (i - 1) * size))->~AtRotTransCoordSys();
                            }
                            if (flags & 1) { operator delete(block); block = nullptr; }
                        } else if (flags & 1) {
                            if (scaled) { ((AtScaleCoordSys*)first)->~AtScaleCoordSys(); }
                            else { ((AtRotTransCoordSys*)first)->~AtRotTransCoordSys(); }
                            operator delete(first);
                        } else {
                            if (scaled) ((AtScaleCoordSys*)first)->~AtScaleCoordSys();
                            else ((AtRotTransCoordSys*)first)->~AtRotTransCoordSys();
                        }
                    }
                }
                compare_side_effects("deleters");
            }
        }
    }
    // AtConstructEach.
    {
        static int calls[2];
        static char* seen[2];
        struct Each {
            static void __fastcall o(void* p, void*) { calls[0]++; g_log[0].add('E', (uint32_t)((char*)p - seen[0])); }
            static void r(void* p) { calls[1]++; g_log[1].add('E', (uint32_t)((char*)p - seen[1])); }
        };
        char block[64];
        seen[0] = seen[1] = block;
        for (int count : {-1, 0, 1, 2, 5}) {
            for (unsigned size : {4u, 7u, 0x30u}) {
                if (count > 0 && count * size > sizeof block) continue;
                at<void(__stdcall*)(void*, unsigned, int, void*)>(0x00011000)(block, size, count, (void*)Each::o);
                AtConstructEach(block, size, count, Each::r);
                eq("constructed each", calls[0], calls[1]);
                compare_logs("each");
            }
        }
    }
    // Spheres, motion states.
    {
        unsigned char a[0x30], b[0x30], c[0x30], d[0x30];
        for (unsigned i = 0; i < sizeof c; i++) c[i] = (unsigned char)(i * 7 + 1);
        std::memset(a, 0xCD, sizeof a);
        std::memset(b, 0xCD, sizeof b);
        std::memset(d, 0xCD, sizeof d);
        at<void*(__fastcall*)(void*, void*)>(0x0017AED0)(a, nullptr);
        new (b) AtSceneSphere;
        eq("sphere", std::memcmp(a, b, sizeof a), 0);
        at<FnP>(0x00039F80)(a, nullptr, c);
        ((AtSceneSphere*)b)->Set(*(AtSceneSphere*)c);
        eq("sphere set", std::memcmp(a, b, sizeof a), 0);
        eq("sphere set all", std::memcmp(a, c, sizeof(AtSceneSphere)), 0);
        at<void*(__fastcall*)(void*, void*)>(0x0004F5A0)(a, nullptr);
        new (b) AtSceneMotionState;
        eq("motion", std::memcmp(a, b, sizeof a), 0);
        at<FnP>(0x0004F5D0)(a, nullptr, c);
        ((AtSceneMotionState*)b)->Set(*(AtSceneMotionState*)c);
        eq("motion set", std::memcmp(a, b, sizeof a), 0);
        eq("motion set all", std::memcmp(a, c, sizeof a), 0);
        (void)d;
    }
    // The keyword table is the original's.
    {
        for (unsigned i = 0; i < AtScene::kKeywordCount; i++) {
            const uint32_t* entry = (const uint32_t*)(uintptr_t)(0x002B1990 + i * 8);
            eq("keyword id", (int)entry[0], AtScene::s_keywords[i].m_id);
            eqs("keyword", (const char*)(uintptr_t)entry[1], AtScene::s_keywords[i].m_name);
        }
    }
    // The heap setters.
    {
        set_heaps(nullptr, nullptr, nullptr);
        at<void(__cdecl*)(void*)>(0x0003B790)(&g_heap_scene);
        AtScene::SetHeap(&g_heap_scene);
        eq("scene heap", (uintptr_t)*(void**)(uintptr_t)0x002C640C, (uintptr_t)AtScene::s_heap);
        at<void(__cdecl*)(void*)>(0x000497F0)(&g_heap_node);
        AtSceneNode::SetHeap(&g_heap_node);
        eq("node heap", (uintptr_t)*(void**)(uintptr_t)0x002C7E0C, (uintptr_t)AtSceneNode::s_heap);
        at<void(__cdecl*)(void*)>(0x00026820)(&g_heap_db);
        AtCoordSysDatabase::SetHeap(&g_heap_db);
        eq("database heap", (uintptr_t)*(void**)(uintptr_t)0x002C6344, (uintptr_t)AtCoordSysDatabase::s_heap);
    }
}

static void run_owner() {
    g_context = "the owner";
    Env env({"A", "B", "PlainOne"}, {"Space"});
    g_ranges[0].clear();
    g_ranges[1].clear();
    add_range(0, g_fake_jointed[0], 0xCC, 1);
    add_range(1, g_fake_jointed[1], 0xCC, 1);
    add_range(0, g_fake_plain[0], 0xA0, 2);
    add_range(1, g_fake_plain[1], 0xA0, 2);
    typedef void*(__fastcall* FnF)(void*, void*, const char*, const char*);
    for (const char* ns : {(const char*)nullptr, "", "Space", "XNone"}) {
        for (const char* name : {"A", "PlainOne", "XGone"}) {
            void* a; void* b;
            { On on(0); a = at<FnF>(0x00045680)(g_own[0], nullptr, name, ns); }
            { On on(1); b = g_own[1]->FindNode(name, ns); }
            same_ptr("found node", a, b);
            { On on(0); a = at<FnF>(0x000499B0)(g_own[0], nullptr, name, ns); }
            { On on(1); b = g_own[1]->FindBoundingGeometry(name, ns); }
            eq("found geometry", (uintptr_t)a, (uintptr_t)b);
            compare_logs("found");
        }
    }
    // Adding, under each form of name space.
    for (const char* ns : {(const char*)nullptr, "", "Space", "Another"}) {
        GmNameMap a, b;
        a.Init(1);
        b.Init(1);
        { On on(0); at<void(__fastcall*)(void*, void*, void*, const char*)>(0x00031A90)(g_own[0], nullptr, &a, ns); }
        { On on(1); g_own[1]->AddScene(&b, ns); }
        eq("added plainly", a.m_group == &g_own[0]->m_nodes, b.m_group == &g_own[1]->m_nodes);
        eq("added", a.m_group != nullptr, b.m_group != nullptr);
        eqs("added to", group_name(a.m_group).c_str(), group_name(b.m_group).c_str());
        compare_logs("added");
        a.Clear();
        b.Clear();
    }
    // AsJointed returns what it was given.
    for (char** f : {g_fake_jointed, g_fake_plain}) {
        void* a; void* b;
        { On on(0); a = at<void*(__cdecl*)(void*)>(0x000559B0)(f[0]); }
        { On on(1); b = AtSceneNode::AsJointed((AtSceneNode*)f[1]); }
        same_ptr("as jointed", a, b);
        compare_logs("as jointed");
    }
    { On on(0); eq("as jointed, null", (uintptr_t)guarded([&] { at<void*(__cdecl*)(void*)>(0x000559B0)(nullptr); }), 1); }
    { On on(1); eq("as jointed, null", (uintptr_t)guarded([&] { AtSceneNode::AsJointed(nullptr); }), 1); }
    g_recording = false;
    compare_logs("as jointed, null");
}

// A coordinate system database on its own, with extra matrices, over itself, with nothing.
static void run_database() {
    g_context = "database";
    std::set<std::string> none;
    Env env(none, none);
    static const unsigned shapes[][3] = {{1, 0, 0}, {0, 1, 0}, {2, 3, 0}, {2, 3, 4}, {0, 0, 0}, {0, 0, 5}, {5, 0, 1}, {0, 5, 1}};
    void* a = std::malloc(sizeof(AtCoordSysDatabase));
    void* b = std::malloc(sizeof(AtCoordSysDatabase));
    std::memset(a, 0xCD, sizeof(AtCoordSysDatabase));
    std::memset(b, 0xCD, sizeof(AtCoordSysDatabase));
    { On on(0); at<Fn0>(0x000268F0)(a, nullptr); }
    { On on(1); new (b) AtCoordSysDatabase; }
    AtCoordSysDatabase* o = (AtCoordSysDatabase*)a;
    AtCoordSysDatabase* r = (AtCoordSysDatabase*)b;
    typedef void(__fastcall* FnA)(void*, void*, void*, unsigned, unsigned, unsigned);
    const auto same = [&](const char* what) {
        g_recording = false;
        g_ranges[0].clear();
        g_ranges[1].clear();
        add_range(0, o->m_rotTrans, (size_t)o->m_rotTransCount * 0x28, 5);
        add_range(1, r->m_rotTrans, (size_t)r->m_rotTransCount * 0x28, 5);
        add_range(0, o->m_scaled, (size_t)o->m_scaledCount * 0x34, 6);
        add_range(1, r->m_scaled, (size_t)r->m_scaledCount * 0x34, 6);
        same_map(o, r);
        eq("device", o->m_device == &g_matdev[0], r->m_device == &g_matdev[1]);
        eq("pool", o->m_pool == &g_pool[0], r->m_pool == &g_pool[1]);
        eq("rot count", o->m_rotTransCount, r->m_rotTransCount);
        eq("scale count", o->m_scaledCount, r->m_scaledCount);
        eq("+2C", (uintptr_t)o->m_root, (uintptr_t)r->m_root);
        eq("extra", o->m_extra, r->m_extra);
        eq("next", o->m_nextMatrix, r->m_nextMatrix);
        eq("rot", o->m_rotTrans != nullptr, r->m_rotTrans != nullptr);
        eq("scale", o->m_scaled != nullptr, r->m_scaled != nullptr);
        if (o->m_rotTrans && r->m_rotTrans) for (unsigned i = 0; i < o->m_rotTransCount; i++) same_cs(&o->m_rotTrans[i], &r->m_rotTrans[i], 0x28);
        if (o->m_scaled && r->m_scaled) for (unsigned i = 0; i < o->m_scaledCount; i++) same_cs(&o->m_scaled[i], &r->m_scaled[i], 0x34);
        compare_side_effects(what);
    };
    same("constructed");
    for (const auto& s : shapes) {
        { On on(0); at<FnA>(0x00026920)(o, nullptr, &g_matdev[0], s[0], s[1], s[2]); }
        { On on(1); r->Allocate(&g_matdev[1], s[0], s[1], s[2]); }
        same("allocate");
    }
    // Arrays whose count is 0 are only freed.
    {
        { On on(0); at<FnA>(0x00026920)(o, nullptr, &g_matdev[0], 2, 2, 0); }
        { On on(1); r->Allocate(&g_matdev[1], 2, 2, 0); }
        same("allocate");
        // The elements are left as they were, without destroying; only the count differs.
        ((uint32_t*)o->m_rotTrans)[-1] = 0;
        ((size_t*)r->m_rotTrans)[-1] = 0;
        ((uint32_t*)o->m_scaled)[-1] = 0;
        ((size_t*)r->m_scaled)[-1] = 0;
        { On on(0); at<Fn0>(0x000271F0)(o, nullptr); at<Fn0>(0x000271F0)(o, nullptr); }
        { On on(1); r->Clear(); r->Clear(); }
        same("clear with no counts");
    }
    // A device with a pool that is cleared, and one without a pool.
    {
        { On on(0); at<FnA>(0x00026920)(o, nullptr, &g_matdev[0], 1, 1, 1); }
        { On on(1); r->Allocate(&g_matdev[1], 1, 1, 1); }
        o->m_pool = nullptr;
        r->m_pool = nullptr;
        { On on(0); at<Fn0>(0x000271F0)(o, nullptr); }
        { On on(1); r->Clear(); }
        same("clear with no pool");
        { On on(0); at<FnA>(0x00026920)(o, nullptr, &g_matdev[0], 1, 1, 1); }
        { On on(1); r->Allocate(&g_matdev[1], 1, 1, 1); }
        o->m_device = nullptr;
        r->m_device = nullptr;
        { On on(0); at<Fn0>(0x000271F0)(o, nullptr); }
        { On on(1); r->Clear(); }
        same("clear with no device");
    }
    { On on(0); at<FnA>(0x00026920)(o, nullptr, &g_matdev[0], 2, 1, 1); }
    { On on(1); r->Allocate(&g_matdev[1], 2, 1, 1); }
    same("allocate");
    // The complete destructor, in place; then the deleting one, on a fresh database.
    { On on(0); at<Fn0>(0x00027280)(o, nullptr); }
    { On on(1); r->~AtCoordSysDatabase(); }
    compare_side_effects("destroyed");
    std::memset(a, 0xCD, sizeof(AtCoordSysDatabase));
    std::memset(b, 0xCD, sizeof(AtCoordSysDatabase));
    { On on(0); at<Fn0>(0x000268F0)(a, nullptr); at<FnA>(0x00026920)(o, nullptr, &g_matdev[0], 1, 1, 0); eq("returns this", (uintptr_t)at<FnDel>(0x00019CD0)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); new (b) AtCoordSysDatabase; r->Allocate(&g_matdev[1], 1, 1, 0); r->~AtCoordSysDatabase(); }
    compare_side_effects("deleted");
    std::free(a);
    std::free(b);
}

// Nodes on their own: copies, starting from a description, membership of modifier sets.
struct SetWorld {
    AtSceneModifierSet sets[2];
    AtSceneDirtyOwner lists[2];
    AtSceneLink links[8];
    AtSceneLinkItem items[8];
};
static SetWorld g_world[2];

static void build_world(int side, AtSceneNode* node, unsigned seed) {
    std::mt19937 rng(seed);
    SetWorld& w = g_world[side];
    std::memset(&w, 0, sizeof w);
    for (unsigned i = 0; i < 8; i++) {
        AtSceneLink& l = w.links[i];
        AtSceneLinkItem& it = w.items[i];
        it.m_node = node;
        it.m_list = &w.lists[i % 2];
        l.m_set = &w.sets[rng() % 2];
        l.m_items = &it;
    }
    // Lists by kind: 0 to 3, each with some links; items in a chain within their link, and in a list they can leave.
    for (unsigned kind = 0; kind < 4; kind++) {
        const unsigned a = kind * 2, b = kind * 2 + 1;
        w.links[a].m_next = rng() % 2 ? &w.links[b] : nullptr;
        node->m_links[kind] = rng() % 4 ? &w.links[a] : nullptr;
        // Dirty lists hold a link only when the node's flag says so.
        if (rng() % 2) {
            w.sets[kind % 2].m_dirty[kind] = &w.links[a];
            w.links[a].m_nextDirty = rng() % 2 ? &w.links[b] : nullptr;
            node->m_flags |= AtSceneNode::kInDirtyList0 << kind;
        }
        w.items[a].m_prevInList = rng() % 2 ? &w.items[b] : nullptr;
        w.items[b].m_nextInList = w.items[a].m_prevInList ? &w.items[a] : nullptr;
        w.items[a].m_nextInList = nullptr;
        w.lists[kind % 2].m_first[kind] = &w.items[a];
    }
    AtSceneModifierSet::s_itemsInUse = 100;
    *(int*)(uintptr_t)0x002C63F4 = 100;
}

static uint64_t world_ptr(int side, const void* p) {
    if (!p) return 0;
    const char* c = (const char*)p;
    const char* base = (const char*)&g_world[side];
    if (c >= base && c < base + sizeof(SetWorld)) return 0x10000 + (c - base);
    return canon(side, p);
}

static void same_world() {
    // Every word of the two, pointers by where they lead.
    const uint32_t* a = (const uint32_t*)&g_world[0];
    const uint32_t* b = (const uint32_t*)&g_world[1];
    for (unsigned i = 0; i < sizeof(SetWorld) / 4; i++) eq("set world", world_ptr(0, (const void*)(uintptr_t)a[i]), world_ptr(1, (const void*)(uintptr_t)b[i]));
    eq("items in use", *(int*)(uintptr_t)0x002C63F4, AtSceneModifierSet::s_itemsInUse);
}

static void run_nodes() {
    g_context = "nodes";
    std::set<std::string> none;
    Env env(none, none);
    const unsigned size = 0xCC;
    const auto make = [&](int side, int cls) {
        char* p = (char*)std::malloc(size);
        std::memset(p, 0xCD, size);
        On on(side);
        if (cls == 0) {
            if (side == 0) at<Fn0>(0x0004A740)(p, nullptr);
            else new (p) AtSceneNode;
        } else if (cls == 14) {
            if (side == 0) at<Fn0>(0x0004F3A0)(p, nullptr);
            else new (p) AtSceneModifierNode;
        } else {
            if (side == 0) at<void*(__fastcall*)(void*, void*)>(kOriginalCtor[kAtActorJointedModel])(p, nullptr);
            else AtSceneActorsConstruct(kAtActorJointedModel, p, 1);
        }
        return p;
    };
    const auto same = [&](const char* what, char** o, char** r, unsigned n) {
        g_recording = false;
        g_ranges[0].clear();
        g_ranges[1].clear();
        for (unsigned i = 0; i < n; i++) { add_range(0, o[i], size, 200 + i); add_range(1, r[i], size, 200 + i); }
        add_range(0, &g_world[0], sizeof(SetWorld), 500);
        add_range(1, &g_world[1], sizeof(SetWorld), 500);
        // What each points at, wherever it came from, by which node it belongs to.
        for (unsigned i = 0; i < n; i++) {
            add_range(0, ((AtSceneNode*)o[i])->m_coordSys, 0x28, 220 + i);
            add_range(1, ((AtSceneNode*)r[i])->m_coordSys, 0x28, 220 + i);
            add_range(0, ((AtSceneNode*)o[i])->m_motion, 0x30, 240 + i);
            add_range(1, ((AtSceneNode*)r[i])->m_motion, 0x30, 240 + i);
        }
        for (unsigned i = 0; i < n; i++) same_node(o[i], r[i], size);
        same_world();
        compare_side_effects(what);
    };

    // Set up a source by hand: every field a copy takes, its own coordinate system and motion state.
    for (unsigned variant = 0; variant < 12; variant++) {
        const int cls = variant % 3 == 0 ? 0 : variant % 3 == 1 ? 14 : 3;
        char* src[2] = {make(0, cls), make(1, cls)};
        char* dst[2] = {make(0, 0), make(1, 0)};
        AtCoordSys* cs[2] = {make_cs(0), make_cs(1)};
        AtSceneMotionState motion;
        for (unsigned i = 0; i < 12; i++) ((float*)&motion)[i] = (float)i * 1.5f + (float)variant;
        for (int side = 0; side < 2; side++) {
            AtSceneNode* s = (AtSceneNode*)src[side];
            s->m_flags = 0x7FFFF & ~1u & ~4u;
            s->m_sphere.center.x = 1; s->m_sphere.center.y = 2; s->m_sphere.center.z = 3; s->m_sphere.radius = 4;
            s->m_worldSphere.center.x = 5; s->m_worldSphere.center.y = 6; s->m_worldSphere.center.z = 7; s->m_worldSphere.radius = 8;
            s->m_activeDistanceSquared = 9;
            s->m_boundingGeometry = (void*)(uintptr_t)0x1234;
            s->m_unknown40 = 0x77;
            s->m_coordSys = variant & 4 ? nullptr : cs[side];
            s->m_motion = variant & 8 ? nullptr : &motion;
            s->m_owner = g_own[side];
            raw(s, 0x90) = variant & 1 ? (void*)(uintptr_t)0x4321 : nullptr;
            // The two ends of the classes that can share a coordinate system. Each needs a source with one, and with +0x90 set: an odd variant without the bit for 4.
            if (variant == 9) s->m_class = 0xD;
            if (variant == 11) s->m_class = 1;
        }
        for (int twice = 0; twice < 2; twice++) {
            typedef void(__fastcall* FnCopy)(void*, void*, void*, const void*);
            { On on(0); ((AtSceneNode*)dst[0])->CopyFrom(g_own[0], (AtSceneNode*)src[0]); }
            { On on(1); ((AtSceneNode*)dst[1])->CopyFrom(g_own[1], (AtSceneNode*)src[1]); }
            (void)sizeof(FnCopy);
            char* o[2] = {src[0], dst[0]};
            char* r[2] = {src[1], dst[1]};
            same("copy", o, r, 2);
            const AtCoordSys* oc = ((AtSceneNode*)dst[0])->m_coordSys;
            const AtCoordSys* rc = ((AtSceneNode*)dst[1])->m_coordSys;
            eq("copy's coordinate system", oc != nullptr, rc != nullptr);
            if (oc && rc) { add_range(0, oc, 0x28, 300); add_range(1, rc, 0x28, 300); same_cs(oc, rc, 0x28); }
            const AtSceneMotionState* om = ((AtSceneNode*)dst[0])->m_motion;
            const AtSceneMotionState* rm = ((AtSceneNode*)dst[1])->m_motion;
            eq("copy's motion", om != nullptr, rm != nullptr);
            if (om && rm) eq("copy's motion state", std::memcmp(om, rm, sizeof *om), 0);
            compare_side_effects("copy");
        }
        { On on(0); ((AtSceneNode*)dst[0])->Clear(); }
        { On on(1); ((AtSceneNode*)dst[1])->Clear(); }
        char* o[1] = {dst[0]};
        char* r[1] = {dst[1]};
        same("cleared copy", o, r, 1);
    }
    // No memory and no heap for a copy.
    for (int how = 0; how < 2; how++) {
        char* src[2] = {make(0, 0), make(1, 0)};
        char* dst[2] = {make(0, 0), make(1, 0)};
        AtCoordSys* cs[2] = {make_cs(0), make_cs(1)};
        AtSceneMotionState motion;
        for (int side = 0; side < 2; side++) {
            ((AtSceneNode*)src[side])->m_motion = &motion;
            ((AtSceneNode*)src[side])->m_coordSys = how == 0 ? cs[side] : nullptr;
        }
        g_refuse_heap = &g_heap_node;
        const bool a = guarded([&] { On on(0); ((AtSceneNode*)dst[0])->CopyFrom(g_own[0], (AtSceneNode*)src[0]); });
        g_recording = false;
        g_fail_alloc = false; g_refusing = 0;
        const bool b = guarded([&] { On on(1); ((AtSceneNode*)dst[1])->CopyFrom(g_own[1], (AtSceneNode*)src[1]); });
        g_recording = false;
        g_fail_alloc = false; g_refusing = 0;
        g_refuse_heap = nullptr;
        eq("crash in a copy with no memory", a, b);
        compare_logs("no memory for a copy");
        g_hash[0] = g_hash[1] = 0;

        set_heaps(&g_heap_scene, nullptr, &g_heap_db);
        char* dst2[2] = {make(0, 0), make(1, 0)};
        { On on(0); ((AtSceneNode*)dst2[0])->CopyFrom(g_own[0], (AtSceneNode*)src[0]); }
        { On on(1); ((AtSceneNode*)dst2[1])->CopyFrom(g_own[1], (AtSceneNode*)src[1]); }
        set_heaps(&g_heap_scene, &g_heap_node, &g_heap_db);
        compare_side_effects("no heap for a copy");
    }
    // From a description.
    for (unsigned variant = 0; variant < 8; variant++) {
        char* dst[2] = {make(0, 0), make(1, 0)};
        AtCoordSys* cs[2] = {make_cs(0), make_cs(1)};
        AtSceneMotionState motion[2];
        for (int twice = 0; twice < 2; twice++) {
            for (int side = 0; side < 2; side++) {
                AtSceneNodeDesc d;
                std::memset(&d, 0, sizeof d);
                d.m_collision = variant & 1;
                d.m_sphere.center.x = 1; d.m_sphere.center.y = 2; d.m_sphere.center.z = 3; d.m_sphere.radius = 4;
                d.m_activeDistance = 5;
                d.m_coordSys = variant & 2 && !twice ? cs[side] : nullptr;  // The second time with neither: what the first left goes only if a reload clears it.
                d.m_motion = variant & 4 && !twice ? &motion[side] : nullptr;
                On on(side);
                ((AtSceneNode*)dst[side])->InitFrom(g_own[side], &d);
            }
            add_range(0, cs[0], 0x28, 301);
            add_range(1, cs[1], 0x28, 301);
            add_range(0, &motion[0], sizeof motion[0], 302);
            add_range(1, &motion[1], sizeof motion[1], 302);
            g_recording = false;
            for (unsigned off = 4; off < 0x6C; off += 4) {
                if (off == 0x30) eq("owner", raw(dst[0], off) == g_own[0], raw(dst[1], off) == g_own[1]);
                else if (off == 0x34 || off == 0x38) same_ptr("from a description", raw(dst[0], off), raw(dst[1], off));
                else eq("from a description", dword(dst[0], off), dword(dst[1], off));
            }
            compare_side_effects("from a description");
            // Clear would not free these: they are not owned.
        }
    }
    // Modifier sets, in every shape: being moved, then cleared, for both classes.
    for (unsigned seed = 0; seed < 64; seed++) {
        for (const int cls : {0, 14}) {
            for (int side = 0; side < 2; side++) {
                g_ranges[side].clear();
            }
            char* n[2] = {make(0, cls), make(1, cls)};
            char* child[2] = {make(0, 0), make(1, 0)};
            char* parent[2] = {make(0, 0), make(1, 0)};
            for (int side = 0; side < 2; side++) {
                build_world(side, (AtSceneNode*)n[side], seed);
                AtSceneNode* x = (AtSceneNode*)n[side];
                if (seed & 1) { x->m_firstChild = (AtSceneNode*)child[side]; ((AtSceneNode*)child[side])->m_parent = x; }
                if (seed & 2) { x->m_parent = (AtSceneNode*)parent[side]; ((AtSceneNode*)parent[side])->m_firstChild = x; }
                if (seed & 4) x->m_flags |= AtSceneNode::kMoved;
                if (cls == 14) raw(n[side], 0x6C) = (void*)(uintptr_t)0x5151;
            }
            char* o[3] = {n[0], child[0], parent[0]};
            char* r[3] = {n[1], child[1], parent[1]};
            same("built", o, r, 3);
            { On on(0); ((AtSceneNode*)n[0])->OnMoved(); }
            { On on(1); ((AtSceneNode*)n[1])->OnMoved(); }
            same("moved", o, r, 3);
            { On on(0); ((AtSceneNode*)n[0])->Clear(); }
            { On on(1); ((AtSceneNode*)n[1])->Clear(); }
            same("cleared", o, r, 3);
            std::memset(g_world, 0, sizeof g_world);
        }
    }
    // A link from another set first, so the search goes on; and a node that belongs to neither: both ways through RemoveLink and RemoveDirty.
    for (unsigned kind : {0u, 1u, 3u}) {
        for (unsigned shape = 0; shape < 5; shape++) {
            char* n[2] = {make(0, 0), make(1, 0)};
            char* other[2] = {make(0, 0), make(1, 0)};
            for (int side = 0; side < 2; side++) {
                SetWorld& w = g_world[side];
                std::memset(&w, 0, sizeof w);
                AtSceneNode* x = (AtSceneNode*)n[side];
                for (unsigned i = 0; i < 8; i++) {
                    w.items[i].m_list = &w.lists[0];
                    w.items[i].m_node = i % 2 ? (AtSceneNode*)other[side] : x;
                    w.links[i].m_items = &w.items[i];
                    w.links[i].m_set = &w.sets[0];
                }
                w.links[0].m_set = &w.sets[1];  // the first of the node's is another set's
                x->m_links[kind] = &w.links[0];
                w.links[0].m_next = &w.links[2];
                w.items[2].m_next = &w.items[4];  // two items in the link
                w.sets[0].m_freeLinks = &w.links[6];
                w.sets[0].m_freeItems = &w.items[6];
                if (shape == 1) x->m_links[kind] = &w.links[0], w.links[0].m_next = nullptr;  // none of this set
                if (shape == 2) x->m_links[kind] = nullptr;
                // Dirty: another node's link first.
                if (shape == 3) { w.sets[0].m_dirty[kind] = &w.links[1]; w.links[1].m_nextDirty = &w.links[2]; x->m_flags = AtSceneNode::kInDirtyList0 << kind; }
                if (shape == 4) { w.sets[0].m_dirty[kind] = &w.links[1]; x->m_flags = 0xFFF0; }
                AtSceneModifierSet::s_itemsInUse = 100;
                *(int*)(uintptr_t)0x002C63F4 = 100;
            }
            char* o[2] = {n[0], other[0]};
            char* r[2] = {n[1], other[1]};
            static const uint32_t kOriginal[4] = {0x0003A370, 0x0003A310, 0, 0x0003A340};
            { On on(0); at<FnP>(kOriginal[kind])(&g_world[0].sets[0], nullptr, n[0]); }
            {
                On on(1);
                if (kind == 0) g_world[1].sets[0].RemoveAsKind0((AtSceneNode*)n[1]);
                else if (kind == 1) g_world[1].sets[0].RemoveAsKind1((AtSceneNode*)n[1]);
                else g_world[1].sets[0].RemoveAsKind3((AtSceneNode*)n[1]);
            }
            same("removed", o, r, 2);
        }
    }
    // The node deleting forms: one element, an array of two by way of the vftable, for both classes.
    for (const int cls : {0, 14}) {
        for (unsigned flags : {0u, 1u}) {
            char* n[2] = {make(0, cls), make(1, cls)};
            { On on(0); at<FnDel>(cls ? 0x0003CD00 : 0x0003B7F0)(n[0], nullptr, flags & ~1u); }
            { On on(1); if (cls) ((AtSceneModifierNode*)n[1])->~AtSceneModifierNode(); else ((AtSceneNode*)n[1])->~AtSceneNode(); }
            g_recording = false;
            for (unsigned off = 4; off < 0x6C; off += 4) eq("destroyed node", dword(n[0], off), dword(n[1], off));
            compare_side_effects("destroyed node");
        }
    }
    // Complete destructors by address, and the modifier node's own, and the sphere of a node with a parent coordinate system.
    {
        char* n[2] = {make(0, 14), make(1, 14)};
        { On on(0); at<Fn0>(0x0003B910)(n[0], nullptr); }
        { On on(1); ((AtSceneModifierNode*)n[1])->~AtSceneModifierNode(); }
        compare_side_effects("modifier node destroyed");
        char* m[2] = {make(0, 0), make(1, 0)};
        { On on(0); at<Fn0>(0x0004AFD0)(m[0], nullptr); }
        { On on(1); ((AtSceneNode*)m[1])->~AtSceneNode(); }
        compare_side_effects("node destroyed");
    }
    // ReleaseCoordSys with a coordinate system it does not own stays; Reset leaves the class word and +0x68.
    {
        char* n[2] = {make(0, 14), make(1, 14)};
        AtCoordSys* cs[2] = {make_cs(0), make_cs(1)};
        for (int side = 0; side < 2; side++) {
            ((AtSceneNode*)n[side])->m_coordSys = cs[side];
            ((AtSceneNode*)n[side])->m_unknown68 = 5;
            raw(n[side], 0x6C) = (void*)(uintptr_t)9;
        }
        { On on(0); at<Fn0>(0x00049FE0)(n[0], nullptr); }
        { On on(1); ((AtSceneNode*)n[1])->ReleaseCoordSys(); }
        g_recording = false;
        eq("released, not owned", raw(n[0], 0x34) == nullptr, raw(n[1], 0x34) == nullptr);
        eq("released, not owned", dword(n[0], 4), dword(n[1], 4));
        { On on(0); at<Fn0>(0x00049F80)(n[0], nullptr); }
        { On on(1); ((AtSceneNode*)n[1])->Reset(); }
        g_recording = false;
        for (unsigned off = 4; off < 0x70; off += 4) eq("released", dword(n[0], off), dword(n[1], off));
        compare_side_effects("released");
    }
    // The next of each, in bounds and past them.
    {
        AtScene* o = make_o(0xCD);
        AtScene* r = make_r(0xCD);
        typedef void*(__fastcall* FnN)(void*, void*, void*);
        // Each side's reports go through its own tokenizer's class.
        static unsigned char original_tokenizer[sizeof(AtIoTextTokenizer)];
        at<Fn0>(0x00022500)(original_tokenizer, nullptr);
        AtIoTextTokenizer recovered_tokenizer;
        static char store[0x100];
        for (AtScene* s : {o, r}) {
            s->m_rotTransCount = 2;
            s->m_scaleCount = 2;
            s->m_motionCount = 2;
            s->m_coordSys.m_rotTrans = (AtRotTransCoordSys*)store;
            s->m_coordSys.m_scaled = (AtScaleCoordSys*)store;
            s->m_motionStates = (AtSceneMotionState*)store;
        }
        for (int i = 0; i < 3; i++) {
            { On on(0); eq("next rot", (uintptr_t)at<FnN>(0x0003D8C0)(o, nullptr, original_tokenizer), (uintptr_t)(store + i * 0x28)); }
            { On on(1); eq("next rot", (uintptr_t)r->NextRotTrans(&recovered_tokenizer), (uintptr_t)(store + i * 0x28)); }
            { On on(0); eq("next scale", (uintptr_t)at<FnN>(0x0003D8F0)(o, nullptr, original_tokenizer), (uintptr_t)(store + i * 0x34)); }
            { On on(1); eq("next scale", (uintptr_t)r->NextScale(&recovered_tokenizer), (uintptr_t)(store + i * 0x34)); }
            { On on(0); eq("next motion", (uintptr_t)at<FnN>(0x0003BC40)(o, nullptr, original_tokenizer), (uintptr_t)(store + i * 0x30)); }
            { On on(1); eq("next motion", (uintptr_t)r->NextMotionState(&recovered_tokenizer), (uintptr_t)(store + i * 0x30)); }
            eq("used", o->m_rotTransUsed, r->m_rotTransUsed);
            eq("used", o->m_scaleUsed, r->m_scaleUsed);
            eq("used", o->m_motionUsed, r->m_motionUsed);
            // With no file name the original's report starts from an uninitialised buffer (docs/verification.md, tokenizer), so here the log says it was reported, not the text.
            g_message[0][0] = g_message[1][0] = 0;
            compare_logs("next");
        }
    }
}

// What only shows when these are called by themselves. Both sides read through the recovered tokenizer, which the tok suite compares; nothing here reports through it.
class TextStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};
struct Reader {
    std::string text;
    TextStream stream;
    AtIoTextTokenizer tokenizer;
    explicit Reader(const std::string& t) : text(t) {
        stream.m_base = &text[0];
        stream.m_cur = &text[0];
        stream.m_limit = &text[0] + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(AtScene::s_keywords, AtScene::kKeywordCount, nullptr);
    }
    ~Reader() {
        stream.m_base = nullptr;
        stream.m_cur = nullptr;
        stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

class SpyParent : public AtCoordSys {
public:
    int side = 0;
    void Detach(AtCoordSys* child) override { note(side, 'z', (uint32_t)(uintptr_t)(child->m_drawingLocal)); }
};

static void run_alone() {
    g_context = "alone";
    std::set<std::string> names = {"Base", "Other", "Target"};
    std::set<std::string> spaces;
    Env env(names, spaces);
    const auto make = [&](int side, int cls) {
        char* p = (char*)std::malloc(0xCC);
        std::memset(p, 0xCD, 0xCC);
        On on(side);
        if (cls == 14) {
            if (side == 0) at<Fn0>(0x0004F3A0)(p, nullptr);
            else new (p) AtSceneModifierNode;
        } else {
            if (side == 0) at<Fn0>(0x0004A740)(p, nullptr);
            else new (p) AtSceneNode;
        }
        return p;
    };
    const auto same = [&](const char* what, char** o, char** r, unsigned n) {
        g_recording = false;
        g_ranges[0].clear();
        g_ranges[1].clear();
        for (unsigned i = 0; i < n; i++) { add_range(0, o[i], 0xCC, 600 + i); add_range(1, r[i], 0xCC, 600 + i); }
        for (unsigned i = 0; i < n; i++) same_node(o[i], r[i], 0x6C);
        compare_side_effects(what);
    };

    // Children: the first, the middle and the last of three, and one that is not there.
    for (unsigned which = 0; which < 4; which++) {
        char* a[4] = {make(0, 0), make(0, 0), make(0, 0), make(0, 0)};
        char* b[4] = {make(1, 0), make(1, 0), make(1, 0), make(1, 0)};
        for (char** x : {a, b}) {
            AtSceneNode* parent = (AtSceneNode*)x[0];
            parent->m_firstChild = (AtSceneNode*)x[1];
            ((AtSceneNode*)x[1])->m_nextChild = (AtSceneNode*)x[2];
            ((AtSceneNode*)x[2])->m_nextChild = (AtSceneNode*)x[3];
            for (int i = 1; i < 4; i++) ((AtSceneNode*)x[i])->m_parent = parent;
        }
        char* gone[2] = {make(0, 0), make(1, 0)};
        { On on(0); at<FnP>(0x00049920)(a[0], nullptr, which < 3 ? a[which + 1] : gone[0]); }
        { On on(1); ((AtSceneNode*)b[0])->RemoveChild((AtSceneNode*)(which < 3 ? b[which + 1] : gone[1])); }
        same("remove a child", a, b, 4);
    }

    // A list of items, leaving from each place in it.
    for (unsigned which = 0; which < 3; which++) {
        AtSceneDirtyOwner list[2];
        AtSceneLinkItem items[2][3];
        std::memset(list, 0, sizeof list);
        std::memset(items, 0, sizeof items);
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < 3; i++) {
                items[side][i].m_list = &list[side];
                items[side][i].m_prevInList = i > 0 ? &items[side][i - 1] : nullptr;
                items[side][i].m_nextInList = i < 2 ? &items[side][i + 1] : nullptr;
            }
            for (int k = 0; k < 4; k++) list[side].m_first[k] = &items[side][2];
        }
        at<void(__fastcall*)(void*, void*, unsigned)>(0x000467F0)(&items[0][which], nullptr, 2);
        items[1][which].Unlink(2);
        for (int i = 0; i < 3; i++) {
            eq("item's previous", items[0][i].m_prevInList ? items[0][i].m_prevInList - items[0] : -1, items[1][i].m_prevInList ? items[1][i].m_prevInList - items[1] : -1);
            eq("item's next", items[0][i].m_nextInList ? items[0][i].m_nextInList - items[0] : -1, items[1][i].m_nextInList ? items[1][i].m_nextInList - items[1] : -1);
        }
        for (int k = 0; k < 4; k++) eq("list's head", list[0].m_first[k] ? list[0].m_first[k] - items[0] : -1, list[1].m_first[k] ? list[1].m_first[k] - items[1] : -1);
    }

    // The sphere at the end of a node, with and without a coordinate system, when its fields differ.
    for (int with_cs = 0; with_cs < 2; with_cs++) {
        char* a[1] = {make(0, 0)};
        char* b[1] = {make(1, 0)};
        AtCoordSys* cs[2] = {make_cs(0), make_cs(1)};
        for (int side = 0; side < 2; side++) {
            AtSceneNode* x = (AtSceneNode*)(side ? b[0] : a[0]);
            x->m_sphere.center.x = 1.5f; x->m_sphere.center.y = 2.5f; x->m_sphere.center.z = 3.5f; x->m_sphere.radius = 4.5f;
            x->m_worldSphere.center.x = 9; x->m_worldSphere.center.y = 9; x->m_worldSphere.center.z = 9; x->m_worldSphere.radius = 9;
            x->m_flags = AtSceneNode::kMoved;
            x->m_coordSys = with_cs ? cs[side] : nullptr;
        }
        { On on(0); at<Fn0>(0x00049F30)(a[0], nullptr); }
        { On on(1); ((AtSceneNode*)b[0])->FinishBody(); }
        g_recording = false;
        g_ranges[0].clear();
        g_ranges[1].clear();
        add_range(0, cs[0], 0x28, 610);
        add_range(1, cs[1], 0x28, 610);
        same_node(a[0], b[0], 0x6C);
        compare_side_effects("finish");
    }

    // One word by itself: nothing after BoundingSphere to hide what it wrote.
    {
        char* a[1] = {make(0, 0)};
        char* b[1] = {make(1, 0)};
        Reader ra("1.5 2.5 3.5 4.5"), rb("1.5 2.5 3.5 4.5");
        { On on(0); at<void(__fastcall*)(void*, void*, void*, void*, int)>(0x0004AA10)(a[0], nullptr, g_own[0], &ra.tokenizer, kAtWordBoundingSphere); }
        { On on(1); ((AtSceneNode*)b[0])->ReadBody(g_own[1], &rb.tokenizer, kAtWordBoundingSphere); }
        same("sphere alone", a, b, 1);
    }

    // Read twice with no scene in between: what the first gave goes, if a reload clears it. The scene only answers for the coordinate system.
    {
        AtScene* so = make_o(0xCD);
        AtScene* sr = make_r(0xCD);
        AtRotTransCoordSys rot[2];
        for (int side = 0; side < 2; side++) {
            AtScene* sc = side ? sr : so;
            sc->m_coordSys.m_rotTrans = &rot[side];
            sc->m_rotTransCount = 1;
            rot[side].SetMatrices(fresh_matrix(side), fresh_matrix(side));
        }
        g_ranges[0].clear();
        g_ranges[1].clear();
        char* a[1] = {make(0, 0)};
        char* b[1] = {make(1, 0)};
        for (const char* text : {"RotTransCoordSys { CollisionEnable }", "{ }"}) {
            Reader ra(text), rb(text);
            { On on(0); at<void(__fastcall*)(void*, void*, void*, void*, void*)>(0x000499F0)(a[0], nullptr, g_own[0], so, &ra.tokenizer); }
            { On on(1); ((AtSceneNode*)b[0])->Read(g_own[1], sr, &rb.tokenizer); }
            add_range(0, &rot[0], 0x28, 611);
            add_range(1, &rot[1], 0x28, 611);
            same_node(a[0], b[0], 0x6C);
            compare_side_effects("read twice");
        }
        for (AtScene* sc : {so, sr}) { sc->m_coordSys.m_rotTrans = nullptr; sc->m_rotTransCount = 0; }
    }

    // A release of what it owns, compared before anything resets it.
    {
        char* src[2] = {make(0, 0), make(1, 0)};
        char* a[1] = {make(0, 0)};
        char* b[1] = {make(1, 0)};
        AtCoordSys* cs[2] = {make_cs(0), make_cs(1)};
        ((AtSceneNode*)src[0])->m_coordSys = cs[0];
        ((AtSceneNode*)src[1])->m_coordSys = cs[1];
        { On on(0); ((AtSceneNode*)a[0])->CopyFrom(g_own[0], (AtSceneNode*)src[0]); at<Fn0>(0x00049FE0)(a[0], nullptr); }
        { On on(1); ((AtSceneNode*)b[0])->CopyFrom(g_own[1], (AtSceneNode*)src[1]); ((AtSceneNode*)b[0])->ReleaseCoordSys(); }
        same("released", a, b, 1);
    }

    // A scene read directly sets its owner; Load sets it too, and hides that.
    {
        AtScene* so = make_o(0xCD);
        AtScene* sr = make_r(0xCD);
        Reader ra("Node[1]\n"), rb("Node[1]\n");
        { On on(0); at<void(__fastcall*)(void*, void*, void*, void*)>(0x0003BCD0)(so, nullptr, g_own[0], &ra.tokenizer); }
        { On on(1); sr->Read(g_own[1], &rb.tokenizer); }
        same_scene("read directly", so, sr);
    }

    // A database with stale fields and no names yet: a Clear does not run, so only Allocate can set these.
    {
        void* a = std::malloc(sizeof(AtCoordSysDatabase));
        void* b = std::malloc(sizeof(AtCoordSysDatabase));
        std::memset(a, 0xCD, sizeof(AtCoordSysDatabase));
        std::memset(b, 0xCD, sizeof(AtCoordSysDatabase));
        { On on(0); at<Fn0>(0x000268F0)(a, nullptr); }
        { On on(1); new (b) AtCoordSysDatabase; }
        AtCoordSysDatabase* o = (AtCoordSysDatabase*)a;
        AtCoordSysDatabase* r = (AtCoordSysDatabase*)b;
        o->m_nextMatrix = 1;
        r->m_nextMatrix = 1;
        { On on(0); at<void(__fastcall*)(void*, void*, void*, unsigned, unsigned, unsigned)>(0x00026920)(o, nullptr, &g_matdev[0], 1, 1, 0); }
        { On on(1); r->Allocate(&g_matdev[1], 1, 1, 0); }
        eq("next matrix", o->m_nextMatrix, r->m_nextMatrix);
        g_recording = false;
        compare_side_effects("stale database");

        // Which array is destroyed first: each element's parent says so as it lets go.
        static SpyParent spy[2];
        spy[0].side = 0;
        spy[1].side = 1;
        for (AtCoordSysDatabase* d : {o, r}) {
            const int side = d == r ? 1 : 0;
            d->m_rotTrans[0].m_drawingLocal = (AtMatrix*)1;
            d->m_scaled[0].m_drawingLocal = (AtMatrix*)2;
            d->m_rotTrans[0].m_parent = &spy[side];
            d->m_scaled[0].m_parent = &spy[side];
        }
        { On on(0); at<Fn0>(0x000271F0)(o, nullptr); }
        { On on(1); r->Clear(); }
        compare_side_effects("order of destroying the database");
    }

    // A scene destroyed while it holds everything, without a Clear first.
    {
        const std::string text = std::string(kHead) + "Node[1]\n{\n Node \"Base\" RotTransCoordSys\n {\n }\n}\nModelActor[1]\n{\n ModelActor \"Other\" RotTransCoordSys MotionState\n {\n }\n}\n";
        const std::vector<unsigned char> data = bytes(text);
        g_fs->files[0].data = g_fs->files[1].data = data.data();
        g_fs->files[0].size = g_fs->files[1].size = data.size();
        AtScene* so = make_o(0xCD);
        AtScene* sr = make_r(0xCD);
        load_o(so);
        load_r(sr);
        same_scene("loaded, to destroy", so, sr);
        destroy(so, sr, "destroyed while loaded");
    }

    // The order the scene destroys its arrays in: each array's first node lets go of its link in a set, and the set's free list ends up in that order.
    {
        std::string text = "RotTransCoordSys[1]\nNode[30]\n";
        for (const char* word : kArrays) {
            const std::string w = word;
            if (w == "PhysicsJointedModelActor") continue;
            text += w + "[1]\n{\n " + w + " \"" + w.substr(0, 4) + "\"\n {\n }\n}\n";
        }
        const std::vector<unsigned char> data = bytes(text);
        g_fs->files[0].data = g_fs->files[1].data = data.data();
        g_fs->files[0].size = g_fs->files[1].size = data.size();
        AtScene* so = make_o(0xCD);
        AtScene* sr = make_r(0xCD);
        load_o(so);
        load_r(sr);
        same_scene("loaded, to clear", so, sr);
        static AtSceneModifierSet sets[2];
        static AtSceneDirtyOwner lists[2];
        static AtSceneLink links[2][16];
        static AtSceneLinkItem items[2][16];
        std::memset(sets, 0, sizeof sets);
        std::memset(lists, 0, sizeof lists);
        std::memset(links, 0, sizeof links);
        std::memset(items, 0, sizeof items);
        for (int side = 0; side < 2; side++) {
            AtScene* sc = side ? sr : so;
            unsigned n = 0;
            const auto give = [&](void* node) {
                AtSceneLink& l = links[side][n];
                AtSceneLinkItem& it = items[side][n];
                it.m_list = &lists[side];
                it.m_node = (AtSceneNode*)node;
                l.m_set = &sets[side];
                l.m_items = &it;
                ((AtSceneNode*)node)->m_links[1] = &l;
                n++;
            };
            give(sc->m_nodes);
            give(sc->m_modifierNodes);
            for (int k = 0; k < kAtActorKinds; k++) give(raw(sc, kActorFields[k].array));
        }
        { On on(0); at<Fn0>(0x0003D6F0)(so, nullptr); }
        { On on(1); sr->Clear(); }
        compare_side_effects("cleared in order");
        AtSceneLink* x = sets[0].m_freeLinks;
        AtSceneLink* y = sets[1].m_freeLinks;
        unsigned count = 0;
        while (x && y) {
            eq("order of clearing", x - links[0], y - links[1]);
            x = x->m_next;
            y = y->m_next;
            count++;
        }
        eq("all let go", count, 2 + kAtActorKinds);
        eq("order of clearing, end", x != nullptr, y != nullptr);
    }
    g_hash[0] = g_hash[1] = 0;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (g_expect_crash && getenv("KND_TRACE")) std::printf("crash at 0x%p reading 0x%p [%.100s]\n", e->ExceptionRecord->ExceptionAddress, (void*)e->ExceptionRecord->ExceptionInformation[1], g_context.c_str());
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
    if (pos.size() != 2) { std::printf("usage: oracle_asn <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push2);
    redirect(0x00011260, (const void*)o_pop2);
    redirect(0x00011070, (const void*)o_fatal2);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    install_kind<0>(); install_kind<1>(); install_kind<2>(); install_kind<3>(); install_kind<4>(); install_kind<5>(); install_kind<6>(); install_kind<7>(); install_kind<8>();
    set_tables();
    if (entries && !arm_entries(entries)) return 2;

    {
        AtRotTransCoordSys a;
        AtScaleCoordSys b;
        g_rot_table = *(void**)&a;
        g_scale_table = *(void**)&b;
    }
    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_file;
    AtIoTokenFile::s_heap = &g_heap_file;
    AtIoStream::s_heap = &g_heap_file;
    GmNameMap::s_heap = &g_heap_file;
    GmNameSpaceDb::s_heap = &g_heap_file;
    set_heaps(&g_heap_scene, &g_heap_node, &g_heap_db);

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_small(); std::printf("run_small done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_owner(); std::printf("run_owner done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_database(); std::printf("run_database done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_alone(); std::printf("run_alone done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_nodes(); std::printf("run_nodes done: %llu comparisons, %d differences\n", g_checks, g_failures);
    for (unsigned seed = 1; seed <= 20; seed++) run_coordsys(seed, 2000);
    std::printf("run_coordsys done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_written(); std::printf("run_written done: %llu comparisons, %d differences\n", g_checks, g_failures);
    run_shortages(); std::printf("run_shortages done: %llu comparisons, %d differences\n", g_checks, g_failures);

    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ASN\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_text(data, line, 0xCD, false);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u files, %u nodes seen, %u actors; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_nodes_seen, g_actors_seen, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
