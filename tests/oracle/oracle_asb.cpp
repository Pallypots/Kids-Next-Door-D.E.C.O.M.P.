// Differential oracle for bounding geometry, .ASB (Windows x86, local only). See docs/verification.md.
//
//   oracle_asb <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .ASB file listed in index.txt, loaded through the file code on both sides and then tested against each other; text written here, through a tokenizer of the harness's whose Error can stop a reader that would otherwise never
// finish; the arithmetic on boxes and the virtual functions on random shapes, with some awkward values; the drawing, on a device of this file's; the list of hits and the small classes, directly.
//
// Stand-ins. The four functions of the seam in include/at/AtBoundingGeometry.h are the original's own code on both sides here. Nodes are this file's. Matrices are this file's arithmetic, the same on both sides and logged. The joints and
// coordinate systems are the recovered classes on both sides, with the original's own 0x000325A0 and 0x00032690 acting on them on its side.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtBoundingGeometry.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

// Past this many there is nothing more to learn, and a mutant run should not wait for the end.
static void enough() { if (g_failures > 4) { std::printf("stopping early: %d differences\n", g_failures); std::fflush(stdout); ExitProcess(1); } }

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    enough();
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 200).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    enough();
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 200).c_str(), what, o, r);
}
// Bits, except that a NaN is a NaN: which one x87 and SSE arithmetic leave of two is not this code's doing.
static void eqf(const char* what, float o, float r) {
    uint32_t a, b;
    std::memcpy(&a, &o, 4);
    std::memcpy(&b, &r, 4);
    g_checks++;
    if (o != o && r != r) return;
    enough();
    if (a != b && g_failures++ < 25) std::printf("DIFF %s: %s: original %.9g (%08X), recovered %.9g (%08X)\n", g_context.substr(0, 200).c_str(), what, o, a, r, b);
}
static void eqd(const char* what, double o, double r) {
    uint64_t a, b;
    std::memcpy(&a, &o, 8);
    std::memcpy(&b, &r, 8);
    g_checks++;
    if (o != o && r != r) return;
    enough();
    if (a != b && g_failures++ < 25) std::printf("DIFF %s: %s: original %.17g (%016llX), recovered %.17g (%016llX)\n", g_context.substr(0, 200).c_str(), what, o, a, r, b);
}
static void eqv(const char* what, const AtVector3& o, const AtVector3& r) {
    eqf(what, o.x, r.x);
    eqf(what, o.y, r.y);
    eqf(what, o.z, r.z);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

// ---- matrices: the same arithmetic on both sides, logged --------------------------------------------------------------

struct AtMatrix { float m[16]; };

// A NaN is a NaN: see eqf.
static uint32_t bits_of(float f) { uint32_t b; std::memcpy(&b, &f, 4); return f != f ? 0x7FC00000u : b; }
static void log_m(char kind, const AtMatrix* m) { if (g_recording) g_log[g_side].add(kind, bits_of(m->m[0]) ^ (bits_of(m->m[5]) * 3) ^ (bits_of(m->m[12]) * 5) ^ (bits_of(m->m[13]) * 7)); }
static void log_v(char kind, const AtVector3* v) { if (g_recording) g_log[g_side].add(kind, bits_of(v->x) ^ (bits_of(v->y) * 3) ^ (bits_of(v->z) * 5)); }

static void __cdecl m_copy(AtMatrix* to, const AtMatrix* from) { *to = *from; log_m('c', to); }
static void __cdecl m_identity(AtMatrix* m) { std::memset(m, 0, sizeof *m); m->m[0] = m->m[5] = m->m[10] = m->m[15] = 1; log_m('i', m); }
static void __cdecl m_product(AtMatrix* to, const AtMatrix* a, const AtMatrix* b) {
    AtMatrix r;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { float s = 0; for (int k = 0; k < 4; k++) s += a->m[i * 4 + k] * b->m[k * 4 + j]; r.m[i * 4 + j] = s; }
    *to = r;
    log_m('x', to);
}
static void __cdecl m_multiply(AtMatrix* m, const AtMatrix* by) { AtMatrix r = *m; const bool was = g_recording; g_recording = false; m_product(m, &r, by); g_recording = was; log_m('y', m); }
static void __cdecl m_inverse(AtMatrix* to, const AtMatrix* from) {  // Of a rotation and a translation.
    AtMatrix r;
    std::memset(&r, 0, sizeof r);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r.m[i * 4 + j] = from->m[j * 4 + i];
    for (int i = 0; i < 3; i++) r.m[12 + i] = -(from->m[12] * from->m[i * 4] + from->m[13] * from->m[i * 4 + 1] + from->m[14] * from->m[i * 4 + 2]);
    r.m[15] = 1;
    *to = r;
    log_m('v', to);
}
static void __cdecl m_point(const AtMatrix* m, const AtVector3* from, AtVector3* to) {
    log_m('p', m);
    log_v('P', from);
    const AtVector3 f = *from;
    to->x = f.x * m->m[0] + f.y * m->m[4] + f.z * m->m[8] + m->m[12];
    to->y = f.x * m->m[1] + f.y * m->m[5] + f.z * m->m[9] + m->m[13];
    to->z = f.x * m->m[2] + f.y * m->m[6] + f.z * m->m[10] + m->m[14];
}
static void __cdecl m_direction(const AtMatrix* m, const AtVector3* from, AtVector3* to) {
    log_m('d', m);
    log_v('D', from);
    const AtVector3 f = *from;
    to->x = f.x * m->m[0] + f.y * m->m[4] + f.z * m->m[8];
    to->y = f.x * m->m[1] + f.y * m->m[5] + f.z * m->m[9];
    to->z = f.x * m->m[2] + f.y * m->m[6] + f.z * m->m[10];
}
static void __cdecl m_position(const AtMatrix* m, AtVector3* p) { log_m('g', m); p->x = m->m[12]; p->y = m->m[13]; p->z = m->m[14]; }
static void __cdecl m_axes(const AtMatrix* m, AtVector3* x, AtVector3* y, AtVector3* z) {
    log_m('a', m);
    x->x = m->m[0]; x->y = m->m[1]; x->z = m->m[2];
    y->x = m->m[4]; y->y = m->m[5]; y->z = m->m[6];
    z->x = m->m[8]; z->y = m->m[9]; z->z = m->m[10];
}

static void set_matrix_table() {
    g_atMatrix.Copy = m_copy;
    g_atMatrix.Identity = m_identity;
    g_atMatrix.SetInverse = m_inverse;
    g_atMatrix.Multiply = m_multiply;
    g_atMatrix.SetProduct = m_product;
    g_atMatrix.TransformPoint = m_point;
    g_atGeometryMatrix.GetPosition = m_position;
    g_atGeometryMatrix.GetAxes = m_axes;
    g_atGeometryMatrix.TransformDirection = m_direction;
    *(void**)(uintptr_t)0x002C60C8 = (void*)m_copy;
    *(void**)(uintptr_t)0x002C60CC = (void*)m_identity;
    *(void**)(uintptr_t)0x002C61D4 = (void*)m_inverse;
    *(void**)(uintptr_t)0x002C6228 = (void*)m_multiply;
    *(void**)(uintptr_t)0x002C6234 = (void*)m_product;
    *(void**)(uintptr_t)0x002C6250 = (void*)m_point;
    *(void**)(uintptr_t)0x002C61F8 = (void*)m_position;
    *(void**)(uintptr_t)0x002C61FC = (void*)m_axes;
    *(void**)(uintptr_t)0x002C625C = (void*)m_direction;
}

// ---- the seam, as the original's own code -------------------------------------------------------------------------------

int AtSegmentQueryInsert(AtSegmentQuery* query, float key) { return at<int(__fastcall*)(void*, void*, float)>(0x000552B0)(query, nullptr, key); }
void AtSegmentQueryFinish(AtSegmentQuery* query, int kind) { at<void(__fastcall*)(void*, void*, int)>(0x00057B50)(query, nullptr, kind); }
void AtCoordSysTransformDirection(AtCoordSys* coordSys, const AtVector3* from, AtVector3* to) { at<void(__fastcall*)(void*, void*, const void*, void*)>(0x000327B0)(coordSys, nullptr, from, to); }
int AtSphereSegment(const AtSceneSphere* sphere, const AtVector3* start, const AtVector3* end, float* length, const AtVector3* direction, AtVector3* point, float* key, int unused) {
    return at<int(__fastcall*)(const void*, void*, const void*, const void*, void*, const void*, void*, void*, int)>(0x000434C0)(sphere, nullptr, start, end, length, direction, point, key, unused);
}

// ---- nodes, and a world for them -----------------------------------------------------------------------------------------

struct FakeNode {
    AtCoordSys* parent = nullptr;
    virtual void S0() {} virtual void S1() {} virtual void S2() {} virtual void S3() {} virtual void S4() {}
    virtual AtCoordSys* Parent() { g_log[g_side].add('n'); return parent; }  // slot 5
    // vptr +0, parent +4: the flags' place, which nothing here reads. Then as a node.
    unsigned short cls = 3;  // +0x08
    char pad[0xA0 - 0x0A];
    AtCoordSysDatabase* joints = nullptr;  // +0xA0
};

static uint32_t g_seed = 12345;
static uint32_t rnd() { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }
static float rnd_real(float low, float high) { return low + (high - low) * ((rnd() & 0xFFFFF) / 1048575.0f); }

static void rotation_of(float yaw, float pitch, float roll, AtVector3* axes) {
    const float a = std::cos(yaw), b = std::sin(yaw), c = std::cos(pitch), d = std::sin(pitch), e = std::cos(roll), f = std::sin(roll);
    axes[0].x = a * c; axes[0].y = b * c; axes[0].z = -d;
    axes[1].x = a * d * f - b * e; axes[1].y = b * d * f + a * e; axes[1].z = c * f;
    axes[2].x = a * d * e + b * f; axes[2].y = b * d * e - a * f; axes[2].z = c * e;
}

static const float kAwkward[] = {0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 100.0f, 1e-7f, -1e-7f, 3.0e30f, -3.0e30f, HUGE_VALF, -HUGE_VALF, std::numeric_limits<float>::quiet_NaN()};

// A world's worth of coordinate systems, made the same on both sides from the same seed.
struct World {
    AtMatrix matrices[16];
    AtRotTransCoordSys own, others, parentOfJoints, joints[3];
    AtCoordSysDatabase database;
    FakeNode node, otherNode;
    AtBoundingContact contactItems[4];
    AtBoundingContactList contacts;
    float keys[8];
    char hits[4 * AtSegmentHitList::kHitSize];
    AtSegmentHit records[4];
    AtSegmentQuery query;

    void attach(AtCoordSys* c, int i) { c->SetMatrices(&matrices[i], &matrices[i + 1]); }
    void place(AtCoordSys* c, uint32_t seed) {
        const uint32_t saved = g_seed;
        g_seed = seed;
        AtVector3 axes[3];
        rotation_of(rnd_real(-3, 3), rnd_real(-3, 3), rnd_real(-3, 3), axes);
        AtMatrix* m = c->m_local;
        std::memset(m, 0, sizeof *m);
        for (int i = 0; i < 3; i++) { m->m[i * 4] = axes[i].x; m->m[i * 4 + 1] = axes[i].y; m->m[i * 4 + 2] = axes[i].z; }
        m->m[12] = rnd_real(-50, 50); m->m[13] = rnd_real(-50, 50); m->m[14] = rnd_real(-50, 50); m->m[15] = 1;
        c->m_dirty = 0x2B;
        g_seed = saved;
    }
    World(uint32_t seed) {
        attach(&own, 0);
        attach(&others, 2);
        attach(&parentOfJoints, 4);
        for (int i = 0; i < 3; i++) attach(&joints[i], 6 + 2 * i);
        parentOfJoints.Attach(&joints[1]);
        place(&own, seed);
        place(&others, seed + 1);
        place(&parentOfJoints, seed + 2);
        for (int i = 0; i < 3; i++) place(&joints[i], seed + 3 + i);
        database.m_rotTrans = joints;
        node.joints = &database;
        otherNode.joints = &database;
        std::memset(&contacts, 0, sizeof contacts);
        contacts.m_capacity = 4;
        contacts.m_items = contactItems;
        std::memset(contactItems, 0, sizeof contactItems);
        std::memset(keys, 0, sizeof keys);
        std::memset(records, 0, sizeof records);
        query.SetStorage(4, keys, hits);
        query.m_records = records;
        query.m_current = 0;
    }
    ~World() { database.m_rotTrans = nullptr; }
    void reset_answers() {
        contacts.m_count = 0;
        std::memset(contactItems, 0, sizeof contactItems);
        std::memset(records, 0, sizeof records);
        query.m_count = 0;
        query.m_current = 0;
        query.m_flags = AtSegmentHitList::kNotOwned;
    }
};

// Where the context lives, on each side.
struct Segment { AtVector3 direction, start, end, middle; float half; };

static void set_context(int side, World& w, bool coordSys, bool otherCoordSys, bool contacts, const Segment& s) {
    AtSceneNode* node = (AtSceneNode*)&w.node;
    AtSceneNode* other = (AtSceneNode*)&w.otherNode;
    AtCoordSys* mine = coordSys ? &w.own : nullptr;
    AtCoordSys* theirs = otherCoordSys ? &w.others : nullptr;
    AtBoundingContactList* list = contacts ? &w.contacts : nullptr;
    if (side == 0) {
        *(void**)(uintptr_t)0x002C7ED8 = other;
        *(void**)(uintptr_t)0x002C7EDC = node;
        *(void**)(uintptr_t)0x002C7EE4 = theirs;
        *(void**)(uintptr_t)0x002C7EE8 = &w.query;
        *(void**)(uintptr_t)0x002C7EEC = list;
        *(float*)(uintptr_t)0x002C7EF0 = s.half;
        *(void**)(uintptr_t)0x002C7EF4 = mine;
        *(AtVector3*)(uintptr_t)0x002C7EF8 = s.direction;
        *(AtVector3*)(uintptr_t)0x002C7F04 = s.start;
        *(AtVector3*)(uintptr_t)0x002C7F40 = s.middle;
        *(AtVector3*)(uintptr_t)0x002C7F4C = s.end;
    } else {
        AtBoundingGeometry::s_otherNode = other;
        AtBoundingGeometry::s_node = node;
        AtBoundingGeometry::s_otherCoordSys = theirs;
        AtBoundingGeometry::s_query = &w.query;
        AtBoundingGeometry::s_contacts = list;
        AtBoundingGeometry::s_segmentHalfLength = s.half;
        AtBoundingGeometry::s_coordSys = mine;
        AtBoundingGeometry::s_segmentDirection = s.direction;
        AtBoundingGeometry::s_segmentStart = s.start;
        AtBoundingGeometry::s_segmentMiddle = s.middle;
        AtBoundingGeometry::s_segmentEnd = s.end;
    }
}

static int node_id(const World& w, const void* node) { return node == &w.node ? 1 : node == &w.otherNode ? 2 : node == nullptr ? 0 : 3; }

static void same_context(const World& o, const World& r) {
    eqv("relative position", *(AtVector3*)(uintptr_t)0x002C7F10, AtBoundingGeometry::s_relativePosition);
    const AtVector3* axes = (const AtVector3*)(uintptr_t)0x002C7F1C;
    for (int i = 0; i < 3; i++) eqv("relative axis", axes[i], AtBoundingGeometry::s_relativeAxes[i]);
    eq("node", node_id(o, *(void**)(uintptr_t)0x002C7EDC), node_id(r, AtBoundingGeometry::s_node));
    eq("other node", node_id(o, *(void**)(uintptr_t)0x002C7ED8), node_id(r, AtBoundingGeometry::s_otherNode));
}

static void same_coord(const AtCoordSys& a, const AtCoordSys& b) {
    eq("dirty", a.m_dirty, b.m_dirty);
    eq("generation", a.m_unknown20, b.m_unknown20);
    eq("world", std::memcmp(a.m_world, b.m_world, sizeof(AtMatrix)), 0);
    if (std::memcmp(a.m_world, b.m_world, sizeof(AtMatrix)) != 0) for (int i = 0; i < 16; i++) eqf("world's", a.m_world->m[i], b.m_world->m[i]);
}

static void same_worlds(const World& o, const World& r, const void* oa, const void* ra, const void* ob, const void* rb) {
    same_coord(o.own, r.own);
    same_coord(o.others, r.others);
    same_coord(o.parentOfJoints, r.parentOfJoints);
    for (int i = 0; i < 3; i++) same_coord(o.joints[i], r.joints[i]);
    eq("contacts", o.contacts.m_count, r.contacts.m_count);
    for (unsigned i = 0; i < o.contacts.m_count && i < r.contacts.m_count; i++) {
        eq("contact's first", o.contactItems[i].m_first == oa ? 1 : o.contactItems[i].m_first == ob ? 2 : 0, r.contactItems[i].m_first == ra ? 1 : r.contactItems[i].m_first == rb ? 2 : 0);
        eq("contact's second", o.contactItems[i].m_second == oa ? 1 : o.contactItems[i].m_second == ob ? 2 : 0, r.contactItems[i].m_second == ra ? 1 : r.contactItems[i].m_second == rb ? 2 : 0);
        eq("contact's node", node_id(o, o.contactItems[i].m_firstNode), node_id(r, r.contactItems[i].m_firstNode));
        eq("contact's other node", node_id(o, o.contactItems[i].m_secondNode), node_id(r, r.contactItems[i].m_secondNode));
    }
    eq("hits counted", o.query.m_count, r.query.m_count);
    eq("hit's place", o.query.m_current, r.query.m_current);
    for (int i = 0; i < 4; i++) {
        eq("key", std::memcmp(&o.keys[i], &r.keys[i], 4), 0);
        eq("record's geometry", o.records[i].m_geometry == oa ? 1 : o.records[i].m_geometry == nullptr ? 0 : 2, r.records[i].m_geometry == ra ? 1 : r.records[i].m_geometry == nullptr ? 0 : 2);
        eq("record's faces", o.records[i].m_faces, r.records[i].m_faces);
        eq("record's kind", o.records[i].m_unknown0C, r.records[i].m_unknown0C);
    }
}

// ---- drawing -----------------------------------------------------------------------------------------------------------

#define O(n) void Slot##n() override {}

class FakeBuffer : public AtGeometryBuffer {
public:
    O(0) O(1) O(2) O(3) O(4) O(7) O(8)
    static const int kRoom = 4096;
    char positions[kRoom * 20];
    char texCoords[kRoom * 24];
    unsigned short indices[kRoom];
    void* device_seen = nullptr;
    void Lock(const AtVertexFormat* f, unsigned int vertices, AtGeometryLocked* locked) override {
        g_log[g_side].add('L', vertices);
        g_log[g_side].add('l', f->m_attributes.m_flags);
        g_log[g_side].add('l', f->m_attributes.m_count);
        g_log[g_side].add('l', f->m_texCoordCount);
        g_log[g_side].add('l', *(uint32_t*)&f->m_attributes.m_mask0);
        g_log[g_side].add('l', *(uint32_t*)&f->m_texCoordMasks[0]);
        g_log[g_side].add('l', *(uint32_t*)&f->m_texCoordMasks[4]);
        g_log[g_side].add('l', f->m_mask4 | (f->m_mask5 << 8));
        std::memset(locked, 0xCD, sizeof *locked);
        locked->m_positions = positions;
        locked->m_positionStride = 20;
        locked->m_texCoords = texCoords;
        locked->m_texCoordStride = 24;
    }
    void Unlock(unsigned int vertices) override { g_log[g_side].add('U', vertices); }
    void LockIndices(unsigned int count, unsigned short** out) override { g_log[g_side].add('I', count); *out = indices; }
    void UnlockIndices(unsigned int count) override { g_log[g_side].add('J', count); }
    void Draw(AtGeometryDrawDevice* device, void* material, unsigned int count, void** instances) override {
        device_seen = device;
        g_log[g_side].add('W', (uint32_t)(uintptr_t)material);
        g_log[g_side].add('w', count);
        g_log[g_side].add('w', (uint32_t)(uintptr_t)*instances);
    }
};

class FakeSource : public AtGeometryBufferSource {
public:
    O(0) O(1) O(2) O(3) O(4) O(5) O(6) O(7) O(8) O(9) O(10) O(11) O(12) O(13) O(14) O(15) O(16) O(17) O(18) O(19) O(20)
    FakeBuffer* buffer = nullptr;
    AtGeometryBuffer* Buffer() override { g_log[g_side].add('B'); return buffer; }
};

class FakeDevice : public AtGeometryDrawDevice {
public:
    O(0) O(1) O(2) O(3) O(4) O(5) O(6) O(7) O(8) O(9) O(10) O(11) O(12) O(13) O(14) O(15) O(16) O(17) O(18) O(19) O(20) O(21) O(22) O(23)
    O(24) O(25) O(26) O(27) O(28) O(29) O(30) O(31) O(32) O(33) O(34) O(35) O(36) O(37) O(38) O(39) O(40) O(41) O(42) O(43) O(44) O(45) O(46) O(47)
    O(48) O(49) O(50) O(51) O(52) O(53) O(54) O(55) O(56) O(57) O(58) O(59) O(60) O(61) O(62) O(63) O(64) O(65) O(66) O(67)
    AtMatrix matrix;
    int generation = 7;
    AtMatrix* DrawingMatrix() override { g_log[g_side].add('M'); return &matrix; }
    int DrawingGeneration() override { g_log[g_side].add('G'); return generation; }
    void SetWorld(int stage, AtMatrix* world) override { g_log[g_side].add('T', stage); log_m('t', world); }
};

struct Drawing {
    FakeBuffer* buffer[2];
    FakeSource source[2];
    FakeDevice device[2];
    Drawing() {
        for (int i = 0; i < 2; i++) {
            buffer[i] = new FakeBuffer;
            source[i].buffer = buffer[i];
            device[i].m_source = &source[i];
            for (int k = 0; k < 16; k++) device[i].matrix.m[k] = k == 0 || k == 5 || k == 10 || k == 15 ? 1.0f : k == 13 ? 2.0f : 0.0f;
        }
        clear();
    }
    ~Drawing() { delete buffer[0]; delete buffer[1]; }
    void clear() {
        for (int i = 0; i < 2; i++) {
            std::memset(buffer[i]->positions, 0xCD, sizeof buffer[i]->positions);
            std::memset(buffer[i]->texCoords, 0xCD, sizeof buffer[i]->texCoords);
            std::memset(buffer[i]->indices, 0xCD, sizeof buffer[i]->indices);
            buffer[i]->device_seen = nullptr;
        }
    }
    void same(const char* what) {
        eq("positions", std::memcmp(buffer[0]->positions, buffer[1]->positions, sizeof buffer[0]->positions), 0);
        if (std::memcmp(buffer[0]->positions, buffer[1]->positions, sizeof buffer[0]->positions) != 0) {
            for (int i = 0; i < 400; i += 1) eqf("position's", ((float*)buffer[0]->positions)[i], ((float*)buffer[1]->positions)[i]);
        }
        eq("texture coordinates", std::memcmp(buffer[0]->texCoords, buffer[1]->texCoords, sizeof buffer[0]->texCoords), 0);
        eq("indices", std::memcmp(buffer[0]->indices, buffer[1]->indices, sizeof buffer[0]->indices), 0);
        if (std::memcmp(buffer[0]->indices, buffer[1]->indices, sizeof buffer[0]->indices) != 0) for (int i = 0; i < 400; i++) eq("index", buffer[0]->indices[i], buffer[1]->indices[i]);
        eq("device passed", buffer[0]->device_seen == &device[0], buffer[1]->device_seen == &device[1]);
        compare_logs(what);
    }
};

typedef void(__fastcall* FnDrawBox)(void*, void*, const void*, void*, void*);
typedef void(__fastcall* FnDrawSphere)(void*, void*, const void*, void*, unsigned, unsigned, void*);

static void run_draw() {
    g_context = "draw";
    Drawing d;
    AtBoundingBox box;
    box.m_center.x = 1.5f; box.m_center.y = -2.25f; box.m_center.z = 7.125f;
    box.m_half.x = 3.1f; box.m_half.y = 0.7f; box.m_half.z = 11.3f;
    for (void* instance : {(void*)nullptr, (void*)&g_heap_token}) {
        d.clear();
        { On on(0); at<FnDrawBox>(0x00058CB0)(&d.device[0], nullptr, &box, &g_heap_token, instance); }
        { On on(1); d.device[1].DrawBox(&box, &g_heap_token, instance); }
        d.same("box");
    }
    AtSceneSphere sphere;
    sphere.center = box.m_center;
    sphere.radius = 9.75f;
    const unsigned shapes[][2] = {{6, 6}, {1, 3}, {2, 4}, {3, 1}, {1, 1}, {0, 3}, {4, 0}, {0, 0}, {5, 7}, {12, 9}};
    for (const auto& s : shapes) {
        g_context = "draw sphere " + std::to_string(s[0]) + " " + std::to_string(s[1]);
        d.clear();
        { On on(0); at<FnDrawSphere>(0x00059170)(&d.device[0], nullptr, &sphere, &g_heap_token, s[0], s[1], &g_heap_token); }
        { On on(1); d.device[1].DrawSphere(&sphere, &g_heap_token, s[0], s[1], &g_heap_token); }
        d.same("sphere");
    }
}

static void run_formats() {
    g_context = "vertex format";
    typedef void*(__fastcall* FnCtor)(void*, void*);
    typedef void(__fastcall* FnSet3)(void*, void*, unsigned, unsigned, unsigned);
    typedef void(__fastcall* FnSet2)(void*, void*, unsigned, unsigned);
    for (uint32_t flags : {0u, 0xA080u, 0x8080u, 0x200u, 0x400u, 0x800u, 0x2000u, 0x4000u, 0x6000u, 0xFFFFFFFFu, 0x40007FFFu, 0xC000u, 0x80000000u, 0x40000000u}) {
        for (unsigned count : {0u, 1u, 3u, 4u, 5u, 100u}) {
            AtVertexAttributes a, b;
            std::memset(&a, 0xCD, sizeof a);
            std::memset(&b, 0xCD, sizeof b);
            at<FnCtor>(0x00055750)(&a, nullptr);
            new (&b) AtVertexAttributes;
            eq("attributes made", std::memcmp(&a, &b, sizeof a), 0);
            at<FnSet2>(0x00055770)(&a, nullptr, flags, count);
            b.Set(flags, count);
            eq("attributes", std::memcmp(&a, &b, sizeof a), 0);
            for (unsigned tex : {0u, 1u, 2u, 7u, 8u, 9u, 200u}) {
                AtVertexFormat x, y;
                std::memset(&x, 0xCD, sizeof x);
                std::memset(&y, 0xCD, sizeof y);
                eq("format's constructor returns this", (uintptr_t)at<FnCtor>(0x000454C0)(&x, nullptr), (uintptr_t)&x);
                new (&y) AtVertexFormat;
                eq("format made", std::memcmp(&x, &y, 0x1A), 0);
                at<FnSet3>(0x000454F0)(&x, nullptr, flags, count, tex);
                y.Set(flags, count, tex);
                eq("format", std::memcmp(&x, &y, 0x1A), 0);
                eq("format's count", x.m_texCoordCount, y.m_texCoordCount);
                at<FnSet3>(0x000454F0)(&x, nullptr, flags, count, 8);
                y.Set(flags, count, 8);
                eq("format set to eight", std::memcmp(&x, &y, 0x1A), 0);
                at<FnSet3>(0x000454F0)(&x, nullptr, flags, count, tex);
                y.Set(flags, count, tex);
                eq("format set again", std::memcmp(&x, &y, 0x1A), 0);
            }
        }
    }
}

// ---- the arithmetic on boxes ---------------------------------------------------------------------------------------------

typedef int(__fastcall* FnOverlaps)(const void*, void*, const void*, const void*, const void*);
typedef int(__fastcall* FnSphere)(const void*, void*, const void*);
typedef int(__fastcall* FnSegment)(const void*, void*, const void*, const void*, float);
typedef unsigned(__fastcall* FnClip)(const void*, void*, const void*, const void*, void*, void*, void*, void*);
typedef double(__fastcall* FnPen)(const void*, void*, const void*, const void*, const void*, void*, void*, void*);
typedef double(__fastcall* FnPenSphere)(const void*, void*, const void*, void*, void*, void*);

static float any_real(float range, int awkward) {
    if (awkward && (rnd() % awkward) == 0) return kAwkward[rnd() % (sizeof kAwkward / sizeof kAwkward[0])];
    return rnd_real(-range, range);
}
static AtVector3 any_vector(float range, int awkward) { AtVector3 v; v.x = any_real(range, awkward); v.y = any_real(range, awkward); v.z = any_real(range, awkward); return v; }

static void any_box(AtBoundingBox* b, float centers, float sizes, int awkward) {
    b->m_center = any_vector(centers, awkward);
    b->m_half.x = std::fabs(any_real(sizes, awkward));
    b->m_half.y = std::fabs(any_real(sizes, awkward));
    b->m_half.z = std::fabs(any_real(sizes, awkward));
}

// Rotations of several kinds, since an axis nearly along another is what the edge tests turn on.
static void any_rotation(AtVector3* axes, int awkward) {
    switch (rnd() % 6) {
    case 0:
        axes[0].x = 1; axes[0].y = 0; axes[0].z = 0;
        axes[1].x = 0; axes[1].y = 1; axes[1].z = 0;
        axes[2].x = 0; axes[2].y = 0; axes[2].z = 1;
        break;
    case 1:  // Turned about one axis of this box: one axis stays along it.
        rotation_of(rnd_real(-3, 3), 0, 0, axes);
        break;
    case 2:
        rotation_of(0, rnd_real(-3, 3), 0, axes);
        break;
    case 3:
        rotation_of(0, 0, rnd_real(-3, 3), axes);
        break;
    default:
        rotation_of(rnd_real(-3, 3), rnd_real(-3, 3), rnd_real(-3, 3), axes);
        break;
    }
    if (awkward && rnd() % awkward == 0) axes[rnd() % 3] = any_vector(2, 3);
}

static unsigned g_seen_overlap[2], g_seen_faces, g_seen_edge;

static void run_box_math(int count) {
    for (int round = 0; round < count; round++) {
        g_context = "box arithmetic " + std::to_string(round);
        const int awkward = round % 3 == 0 ? 6 : 0;
        const float spread = round % 5 == 0 ? 5.0f : 40.0f;
        AtBoundingBox a, b;
        any_box(&a, 10, 20, awkward);
        any_box(&b, 10, 20, awkward);
        AtVector3 axes[3];
        any_rotation(axes, awkward);
        const AtVector3 position = any_vector(spread, awkward);
        const int ro = at<FnOverlaps>(0x0005CE20)(&a, nullptr, &b, axes, &position);
        eq("overlaps", ro, a.Overlaps(&b, axes, &position));
        g_seen_overlap[ro != 0]++;

        AtVector3 no, nr;
        float eo[2], er[2];
        std::memset(&no, 0xCD, sizeof no); std::memset(&nr, 0xCD, sizeof nr);
        std::memset(eo, 0xCD, sizeof eo); std::memset(er, 0xCD, sizeof er);
        const double po = at<FnPen>(0x0005DD50)(&a, nullptr, &b, axes, &position, &no, &eo[0], &eo[1]);
        const double pr = a.Penetration(&b, axes, &position, &nr, &er[0], &er[1]);
        eqd("penetration", po, pr);
        eqv("penetration's normal", no, nr);
        eqf("penetration's extent", eo[0], er[0]);
        eqf("penetration's other extent", eo[1], er[1]);
        if (no.x != 0 && no.y != 0) g_seen_edge++;

        AtSceneSphere s;
        s.center = any_vector(spread, awkward);
        s.radius = std::fabs(any_real(30, awkward));
        if (round % 4 == 1) {  // Inside, so that each nearest face comes up.
            s.center.x = rnd_real(-a.m_half.x, a.m_half.x);
            s.center.y = rnd_real(-a.m_half.y, a.m_half.y);
            s.center.z = rnd_real(-a.m_half.z, a.m_half.z);
        } else if (round % 4 == 2) {  // Past one face or two only.
            if (rnd() & 1) s.center.x = rnd_real(-a.m_half.x, a.m_half.x);
            if (rnd() & 1) s.center.y = rnd_real(-a.m_half.y, a.m_half.y);
            if (rnd() & 1) s.center.z = rnd_real(-a.m_half.z, a.m_half.z);
        }
        eq("overlaps sphere", at<FnSphere>(0x0005D570)(&a, nullptr, &s), a.OverlapsSphere(&s));
        std::memset(&no, 0xCD, sizeof no); std::memset(&nr, 0xCD, sizeof nr);
        std::memset(eo, 0xCD, sizeof eo); std::memset(er, 0xCD, sizeof er);
        const double so = at<FnPenSphere>(0x0005EAB0)(&a, nullptr, &s, &no, &eo[0], &eo[1]);
        const double sr = a.PenetrationSphere(&s, &nr, &er[0], &er[1]);
        eqd("sphere's penetration", so, sr);
        eqv("sphere's normal", no, nr);
        eqf("sphere's extent", eo[0], er[0]);
        eqf("sphere's other extent", eo[1], er[1]);

        AtVector3 start = any_vector(spread, awkward), end = any_vector(spread, awkward);
        if (round % 4 == 3) {  // Through one axis's faces only, in either direction.
            const int axis = rnd() % 3;
            for (int k = 0; k < 3; k++) {
                if (k == axis) continue;
                (&start.x)[k] = (&a.m_center.x)[k] + rnd_real(-1, 1) * (&a.m_half.x)[k];
                (&end.x)[k] = (&a.m_center.x)[k] + rnd_real(-1, 1) * (&a.m_half.x)[k];
            }
        }
        AtVector3 d;
        d.x = end.x - start.x; d.y = end.y - start.y; d.z = end.z - start.z;
        AtVector3 m;
        m.x = (start.x + end.x) / 2; m.y = (start.y + end.y) / 2; m.z = (start.z + end.z) / 2;
        const float half = std::fabs(any_real(30, awkward));
        eq("overlaps segment", at<FnSegment>(0x0005D6E0)(&a, nullptr, &d, &m, half), a.OverlapsSegment(&d, &m, half));
        AtVector3 so1, so2, sr1, sr2;
        float to[2] = {-1, -1}, tr[2] = {-1, -1};
        const unsigned fo = at<FnClip>(0x0005D880)(&a, nullptr, &start, &end, &so1, &so2, &to[0], &to[1]);
        const unsigned fr = a.Clip(&start, &end, &sr1, &sr2, &tr[0], &tr[1]);
        eq("clip's faces", fo, fr);
        eqv("clip's start", so1, sr1);
        eqv("clip's end", so2, sr2);
        eqf("clip's t", to[0], tr[0]);
        eqf("clip's other t", to[1], tr[1]);
        g_seen_faces |= fo;
    }
}

static void run_box_edges() {
    // Exactly touching: the comparisons at their boundaries, in each axis, and the same with the separating axes of the edges.
    g_context = "box boundaries";
    for (int axis = 0; axis < 3; axis++) {
        for (const float gap : {0.0f, 1e-6f, -1e-6f}) {
            AtBoundingBox a, b;
            a.m_half.x = 2; a.m_half.y = 3; a.m_half.z = 5;
            b.m_half.x = 1; b.m_half.y = 1; b.m_half.z = 1;
            AtVector3 axes[3];
            axes[0].x = 1; axes[0].y = 0; axes[0].z = 0;
            axes[1].x = 0; axes[1].y = 1; axes[1].z = 0;
            axes[2].x = 0; axes[2].y = 0; axes[2].z = 1;
            AtVector3 position;
            position.x = position.y = position.z = 0;
            (&position.x)[axis] = (&a.m_half.x)[axis] + 1 + gap;
            eq("touching", at<FnOverlaps>(0x0005CE20)(&a, nullptr, &b, axes, &position), a.Overlaps(&b, axes, &position));
            AtSceneSphere s;
            s.center.x = s.center.y = s.center.z = 0;
            (&s.center.x)[axis] = (&a.m_half.x)[axis] + 1 + gap;
            s.radius = 1;
            eq("touching sphere", at<FnSphere>(0x0005D570)(&a, nullptr, &s), a.OverlapsSphere(&s));
            s.center.x = a.m_half.x + 0.6f + gap; s.center.y = a.m_half.y + 0.8f; s.center.z = axis == 0 ? 0.0f : a.m_half.z + 0.0f;
            eq("touching corner", at<FnSphere>(0x0005D570)(&a, nullptr, &s), a.OverlapsSphere(&s));
            // A sphere on each of the eight combinations of three axes past or short, signs both ways.
            for (int mask = 0; mask < 8; mask++) {
                for (const float sign : {1.0f, -1.0f}) {
                    s.center.x = sign * (a.m_half.x + (mask & 1 ? 0.5f + gap : -0.5f));
                    s.center.y = -sign * (a.m_half.y + (mask & 2 ? 0.5f + gap : -0.75f));
                    s.center.z = sign * (a.m_half.z + (mask & 4 ? 0.5f + gap : -0.25f));
                    for (const float radius : {0.25f, 0.5f, 0.9f, 2.0f}) {
                        s.radius = radius;
                        eq("eight ways", at<FnSphere>(0x0005D570)(&a, nullptr, &s), a.OverlapsSphere(&s));
                        AtVector3 no, nr;
                        float eo[2], er[2];
                        std::memset(eo, 0xCD, sizeof eo); std::memset(er, 0xCD, sizeof er);
                        const double so = at<FnPenSphere>(0x0005EAB0)(&a, nullptr, &s, &no, &eo[0], &eo[1]);
                        eqd("eight ways' depth", so, a.PenetrationSphere(&s, &nr, &er[0], &er[1]));
                        eqv("eight ways' normal", no, nr);
                        eqf("eight ways' extent", eo[0], er[0]);
                        eqf("eight ways' other", eo[1], er[1]);
                    }
                }
            }
            AtVector3 d, m;
            d.x = d.y = d.z = 0;
            m.x = m.y = m.z = 0;
            (&m.x)[axis] = (&a.m_half.x)[axis] + 1 + gap;
            (&d.x)[axis] = 1;
            eq("touching segment", at<FnSegment>(0x0005D6E0)(&a, nullptr, &d, &m, 1.0f), a.OverlapsSegment(&d, &m, 1.0f));
            // A segment beside the box, askew, which only the cross products can part from it.
            for (int k = 0; k < 3; k++) {
                d.x = d.y = d.z = 0.70710678f;
                (&d.x)[k] = 0;
                m.x = m.y = m.z = 0;
                (&m.x)[(k + 1) % 3] = (&a.m_half.x)[(k + 1) % 3] + 2 + gap;
                (&m.x)[(k + 2) % 3] = -(&a.m_half.x)[(k + 2) % 3] - 2;
                eq("skew segment", at<FnSegment>(0x0005D6E0)(&a, nullptr, &d, &m, 1.0f), a.OverlapsSegment(&d, &m, 1.0f));
            }
        }
    }
    // Every face, entering and leaving, by a segment along each axis; and its keep masks, with segments through two axes at once.
    g_context = "clip faces";
    AtBoundingBox a;
    a.m_center.x = 1; a.m_center.y = 2; a.m_center.z = 3;
    a.m_half.x = 2; a.m_half.y = 3; a.m_half.z = 5;
    const float stops[] = {-20, -2, 0, 1.5f, 2, 3, 4, 6, 8, 20};
    const int n = sizeof stops / sizeof stops[0];
    for (int i = 0; i < n * n * n; i++) {
        for (int j = 0; j < n * n * n; j += 7) {
            AtVector3 s, e;
            s.x = stops[i % n]; s.y = stops[(i / n) % n]; s.z = stops[i / (n * n)];
            e.x = stops[j % n]; e.y = stops[(j / n) % n]; e.z = stops[j / (n * n)];
            AtVector3 so1, so2, sr1, sr2;
            float to[2] = {-1, -1}, tr[2] = {-1, -1};
            const unsigned fo = at<FnClip>(0x0005D880)(&a, nullptr, &s, &e, &so1, &so2, &to[0], &to[1]);
            eq("faces", fo, a.Clip(&s, &e, &sr1, &sr2, &tr[0], &tr[1]));
            eqv("start", so1, sr1);
            eqv("end", so2, sr2);
            eqf("t", to[0], tr[0]);
            eqf("other t", to[1], tr[1]);
            g_seen_faces |= fo;
        }
    }
}

// Axes tilted by very little from each of the box's, either side of the point where the edge tests are skipped, with the boxes apart so that a test not skipped would win.
static unsigned g_seen_near;

static void run_near_parallel() {
    static const int kOrders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (const auto& order : kOrders) {
        for (const float tilt : {1e-4f, 1e-3f, 3e-3f, 3.3e-3f, 5e-3f, 2e-2f}) {
            for (int which = 0; which < 3; which++) {
                g_context = "near parallel";
                AtVector3 turned[3];
                rotation_of(which == 0 ? tilt : 0.0f, which == 1 ? tilt : 0.0f, which == 2 ? tilt : 0.0f, turned);
                AtVector3 axes[3];
                for (int i = 0; i < 3; i++) axes[i] = turned[order[i]];
                AtBoundingBox a, b;
                a.m_half.x = 2; a.m_half.y = 3; a.m_half.z = 5;
                b.m_half.x = 1; b.m_half.y = 4; b.m_half.z = 2;
                // An edge's axis this close to parallel lies nearly along one of the faces' axes, and its depth differs from that face's only in rounding: so a test not skipped shows only now and then, and needs many tries.
                for (int place = 0; place < 67; place++) {
                    AtVector3 position;
                    if (place < 7) {
                        position.x = place & 1 ? 40.0f : 0.25f;
                        position.y = place & 2 ? -40.0f : 0.5f;
                        position.z = place & 4 ? 40.0f : -0.75f;
                    } else {
                        position = any_vector(60, 0);
                        a.m_half.x = rnd_real(0.5f, 9); a.m_half.y = rnd_real(0.5f, 9); a.m_half.z = rnd_real(0.5f, 9);
                        b.m_half.x = rnd_real(0.5f, 9); b.m_half.y = rnd_real(0.5f, 9); b.m_half.z = rnd_real(0.5f, 9);
                    }
                    AtVector3 no, nr;
                    float eo[2], er[2];
                    std::memset(&no, 0xCD, sizeof no); std::memset(&nr, 0xCD, sizeof nr);
                    std::memset(eo, 0xCD, sizeof eo); std::memset(er, 0xCD, sizeof er);
                    const double po = at<FnPen>(0x0005DD50)(&a, nullptr, &b, axes, &position, &no, &eo[0], &eo[1]);
                    eqd("near parallel's depth", po, a.Penetration(&b, axes, &position, &nr, &er[0], &er[1]));
                    eqv("near parallel's normal", no, nr);
                    eqf("near parallel's extent", eo[0], er[0]);
                    eqf("near parallel's other", eo[1], er[1]);
                    g_seen_near++;
                }
            }
        }
    }
}

// ---- geometries, made on both sides -------------------------------------------------------------------------------------

typedef void*(__fastcall* FnCtor)(void*, void*);
static const uint32_t kBoxTable = 0x00276F08, kSphereTable = 0x00276E58, kGroupTable = 0x00276E38;

template <class T> struct Pair {
    T* o;
    T* r;
};

static Pair<AtBoxBoundingGeometry> make_box(const AtBoundingBox& shape) {
    Pair<AtBoxBoundingGeometry> p;
    p.o = (AtBoxBoundingGeometry*)std::malloc(sizeof *p.o);
    p.r = (AtBoxBoundingGeometry*)std::malloc(sizeof *p.r);
    std::memset(p.o, 0xCD, sizeof *p.o);
    std::memset(p.r, 0xCD, sizeof *p.r);
    eq("box's constructor returns this", (uintptr_t)at<FnCtor>(0x00054AA0)(p.o, nullptr), (uintptr_t)p.o);
    new (p.r) AtBoxBoundingGeometry;
    eq("box's vftable", *(uint32_t*)p.o, kBoxTable);
    eq("box made", std::memcmp((char*)p.o + 4, (char*)p.r + 4, sizeof *p.o - 4), 0);
    p.o->m_box = shape;
    p.r->m_box = shape;
    return p;
}
static Pair<AtSphereBoundingGeometry> make_sphere(const AtSceneSphere& shape) {
    Pair<AtSphereBoundingGeometry> p;
    p.o = (AtSphereBoundingGeometry*)std::malloc(sizeof *p.o);
    p.r = (AtSphereBoundingGeometry*)std::malloc(sizeof *p.r);
    std::memset(p.o, 0xCD, sizeof *p.o);
    std::memset(p.r, 0xCD, sizeof *p.r);
    eq("sphere's constructor returns this", (uintptr_t)at<FnCtor>(0x000542D0)(p.o, nullptr), (uintptr_t)p.o);
    new (p.r) AtSphereBoundingGeometry;
    eq("sphere's vftable", *(uint32_t*)p.o, kSphereTable);
    eq("sphere made", std::memcmp((char*)p.o + 4, (char*)p.r + 4, sizeof *p.o - 4), 0);
    p.o->m_sphere = shape;
    p.r->m_sphere = shape;
    return p;
}
static Pair<AtGroupBoundingGeometry> make_group() {
    Pair<AtGroupBoundingGeometry> p;
    p.o = (AtGroupBoundingGeometry*)std::malloc(sizeof *p.o);
    p.r = (AtGroupBoundingGeometry*)std::malloc(sizeof *p.r);
    std::memset(p.o, 0xCD, sizeof *p.o);
    std::memset(p.r, 0xCD, sizeof *p.r);
    eq("group's constructor returns this", (uintptr_t)at<FnCtor>(0x00054290)(p.o, nullptr), (uintptr_t)p.o);
    new (p.r) AtGroupBoundingGeometry;
    eq("group's vftable", *(uint32_t*)p.o, kGroupTable);
    eq("group made", std::memcmp((char*)p.o + 4, (char*)p.r + 4, sizeof *p.o - 4), 0);
    return p;
}

// The original's virtual functions, through its own vftables.
template <class F> static F slot_of(const void* object, int slot) { return (F)(uintptr_t)(*(uint32_t**)object)[slot]; }
typedef int(__fastcall* SlotOverlaps)(void*, void*, void*);
typedef int(__fastcall* SlotTest)(void*, void*);
typedef void(__fastcall* SlotCollect)(void*, void*);
typedef double(__fastcall* SlotPen)(void*, void*, void*, void*, void*, void*, void*, void*);
typedef void(__fastcall* SlotDraw)(void*, void*, void*, void*, void*, void*);

struct Worlds {
    World* side[2];
    explicit Worlds(uint32_t seed) { side[0] = new World(seed); side[1] = new World(seed); }
    ~Worlds() { delete side[0]; delete side[1]; }
};

static unsigned g_slot_calls[8], g_unset_normals;

// Everything a pair of geometries can do, once, with the context given.
static void exercise_pair(AtBoundingGeometry* a, AtBoundingGeometry* b, AtBoundingGeometry* c, AtBoundingGeometry* d, uint32_t seed, bool coordSys, bool otherCoordSys, bool contacts, const Segment& seg, bool parentless) {
    Worlds ws(seed);
    World& wo = *ws.side[0];
    World& wr = *ws.side[1];
    if (parentless) {
        wo.node.parent = wr.node.parent = nullptr;
    } else {
        wo.node.parent = &wo.own;
        wr.node.parent = &wr.own;
    }
    wo.otherNode.parent = &wo.others;
    wr.otherNode.parent = &wr.others;
    if (seed & 1) {  // Asks for only where a segment comes in.
        wo.query.m_flags |= 0x10;
        wr.query.m_flags |= 0x10;
    }

    set_context(0, wo, coordSys, otherCoordSys, contacts, seg);
    set_context(1, wr, coordSys, otherCoordSys, contacts, seg);
    int ro = 0, rr = 0;
    bool co = guarded([&] { On on(0); ro = slot_of<SlotOverlaps>(a, 1)(a, nullptr, b); });
    bool cr = guarded([&] { On on(1); rr = c->Overlaps(d); });
    g_recording = false;
    eq("crash in overlaps", co, cr);
    eq("overlaps", ro, rr);
    same_context(wo, wr);
    same_worlds(wo, wr, a, c, b, d);
    compare_logs("overlaps");
    g_slot_calls[1] += ro != 0;

    AtVector3 no, nr;
    float eo[2], er[2];
    double po = 0, pr = 0;
    std::memset(&no, 0xCD, sizeof no); std::memset(&nr, 0xCD, sizeof nr);
    std::memset(eo, 0xCD, sizeof eo); std::memset(er, 0xCD, sizeof er);
    AtSceneNode* node[2] = {(AtSceneNode*)&wo.node, (AtSceneNode*)&wr.node};
    AtSceneNode* other[2] = {(AtSceneNode*)&wo.otherNode, (AtSceneNode*)&wr.otherNode};
    co = guarded([&] { On on(0); po = slot_of<SlotPen>(a, 4)(a, nullptr, node[0], b, other[0], &no, &eo[0], &eo[1]); });
    cr = guarded([&] { On on(1); pr = c->Penetration(node[1], d, other[1], &nr, &er[0], &er[1]); });
    g_recording = false;
    eq("crash in penetration", co, cr);
    eqd("penetration", po, pr);
    // Two boxes where no axis beats the starting depth, by infinities or NaN: the original never writes its local normal, and moves its stack's old bytes to world space, as this does its own. Those cannot be compared.
    bool normalUnset = false;
    if (a->m_kind == AtBoundingGeometry::kBox && b->m_kind == AtBoundingGeometry::kBox) {
        AtVector3 mark;
        mark.x = mark.y = mark.z = 12345.5f;
        float e1, e2;
        ((AtBoxBoundingGeometry*)c)->m_box.Penetration(&((AtBoxBoundingGeometry*)d)->m_box, AtBoundingGeometry::s_relativeAxes, &AtBoundingGeometry::s_relativePosition, &mark, &e1, &e2);
        normalUnset = mark.x == 12345.5f && mark.y == 12345.5f && mark.z == 12345.5f;
    }
    if (normalUnset) {
        g_unset_normals++;
        g_log[0].n = g_log[1].n = 0;
    } else {
        eqv("normal", no, nr);
    }
    eqf("extent", eo[0], er[0]);
    eqf("other extent", eo[1], er[1]);
    same_context(wo, wr);
    same_worlds(wo, wr, a, c, b, d);
    compare_logs("penetration");

    AtVector3 along;
    along.x = 0.3f; along.y = -0.5f; along.z = 0.8f;
    AtVector3 alongO = along, alongR = along;
    std::memset(eo, 0xCD, sizeof eo); std::memset(er, 0xCD, sizeof er);
    co = guarded([&] { On on(0); po = slot_of<SlotPen>(a, 5)(a, nullptr, node[0], b, other[0], &alongO, &eo[0], &eo[1]); });
    cr = guarded([&] { On on(1); pr = c->PenetrationAlong(node[1], d, other[1], &alongR, &er[0], &er[1]); });
    g_recording = false;
    eq("crash along", co, cr);
    eqd("along", po, pr);
    eqf("along's extent", eo[0], er[0]);
    eqf("along's other extent", eo[1], er[1]);
    eqv("along's direction", alongO, alongR);
    same_context(wo, wr);
    same_worlds(wo, wr, a, c, b, d);
    compare_logs("along");

    // A segment. Twice in a row for the list, whose keys go in order.
    co = guarded([&] { On on(0); ro = slot_of<SlotTest>(a, 2)(a, nullptr); });
    cr = guarded([&] { On on(1); rr = c->TestSegment(); });
    g_recording = false;
    eq("crash in segment test", co, cr);
    eq("segment test", ro, rr);
    compare_logs("segment test");
    for (int i = 0; i < 2; i++) {
        co = guarded([&] { On on(0); slot_of<SlotCollect>(a, 3)(a, nullptr); });
        cr = guarded([&] { On on(1); c->CollectSegment(); });
        g_recording = false;
        eq("crash in segment collect", co, cr);
        same_worlds(wo, wr, a, c, b, d);
        compare_logs("segment collect");
    }
    g_slot_calls[3] += wo.query.m_count;

    // Drawing, down the tree, by the joint's or the node's coordinate system, or by one given.
    static Drawing* drawing = new Drawing;
    for (int given = 0; given < 2; given++) {
        drawing->clear();
        co = guarded([&] { On on(0); slot_of<SlotDraw>(a, 7)(a, nullptr, &g_heap_token, &drawing->device[0], node[0], given ? &wo.others : nullptr); });
        cr = guarded([&] { On on(1); c->DrawTree(&g_heap_token, &drawing->device[1], node[1], given ? &wr.others : nullptr); });
        g_recording = false;
        eq("crash in draw", co, cr);
        same_worlds(wo, wr, a, c, b, d);
        drawing->same("draw");
    }
}

static void run_slots(int count) {
    for (int round = 0; round < count; round++) {
        g_context = "slots " + std::to_string(round);
        const int awkward = round % 7 == 0 ? 5 : 0;
        AtBoundingBox ba, bb;
        any_box(&ba, 5, 30, awkward);
        any_box(&bb, 5, 30, awkward);
        AtSceneSphere sa, sb;
        sa.center = any_vector(5, awkward);
        sa.radius = std::fabs(any_real(30, awkward));
        sb.center = any_vector(5, awkward);
        sb.radius = std::fabs(any_real(30, awkward));
        auto box1 = make_box(ba), box2 = make_box(bb);
        auto sphere1 = make_sphere(sa), sphere2 = make_sphere(sb);
        auto group = make_group();
        auto child = make_sphere(sb);
        struct One { AtBoundingGeometry* o; AtBoundingGeometry* r; } all[] = {{box1.o, box1.r}, {box2.o, box2.r}, {sphere1.o, sphere1.r}, {sphere2.o, sphere2.r}, {group.o, group.r}};
        for (int i = 0; i < 5; i++) {
            for (int side = 0; side < 2; side++) {
                AtBoundingGeometry* g = side ? all[i].r : all[i].o;
                g->m_jointIndex = (round >> i) & 1 ? 1 + (round % 3) : 0;
            }
        }
        // Children for the drawing and for the list that wants leaves only.
        if (round % 2) {
            box1.o->m_firstChild = child.o;
            box1.r->m_firstChild = child.r;
            child.o->m_nextSibling = child.r->m_nextSibling = nullptr;
        }
        child.o->m_jointIndex = child.r->m_jointIndex = 0;
        Segment seg;
        seg.start = any_vector(40, awkward);
        seg.end = any_vector(40, awkward);
        if (round % 3 == 0) { seg.start.x = ba.m_center.x - 50; seg.start.y = ba.m_center.y; seg.start.z = ba.m_center.z; seg.end.x = ba.m_center.x + 50; seg.end.y = ba.m_center.y; seg.end.z = ba.m_center.z; }
        seg.direction.x = seg.end.x - seg.start.x; seg.direction.y = seg.end.y - seg.start.y; seg.direction.z = seg.end.z - seg.start.z;
        const float length = std::sqrt(seg.direction.x * seg.direction.x + seg.direction.y * seg.direction.y + seg.direction.z * seg.direction.z);
        if (length > 0 && length < 1e10f) { seg.direction.x /= length; seg.direction.y /= length; seg.direction.z /= length; }
        seg.middle.x = (seg.start.x + seg.end.x) / 2; seg.middle.y = (seg.start.y + seg.end.y) / 2; seg.middle.z = (seg.start.z + seg.end.z) / 2;
        seg.half = length / 2;
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                const bool coordSys = (round >> 1) & 1, otherCoordSys = (round >> 2) & 1;
                exercise_pair(all[i].o, all[j].o, all[i].r, all[j].r, 1000 + round, coordSys, otherCoordSys, (round >> 3) & 1 ? true : round % 2 == 0, seg, (round >> 4) & 1);
            }
        }
        for (void* p : {(void*)box1.o, (void*)box1.r, (void*)box2.o, (void*)box2.r, (void*)sphere1.o, (void*)sphere1.r, (void*)sphere2.o, (void*)sphere2.r, (void*)group.o, (void*)group.r, (void*)child.o, (void*)child.r}) std::free(p);
    }

    // Two spheres exactly touching. The square of the reach is exact in a double, and equal to the square of the distance only unrounded.
    g_context = "touching spheres";
    {
        const uint32_t saved = g_seed;
        for (int i = 0; i < 200; i++) {
            AtSceneSphere sa, sb;
            const float d = rnd_real(0.5f, 90.0f);
            sa.center.x = sa.center.y = sa.center.z = 0;
            sa.radius = d;
            sb.center.x = d; sb.center.y = 0; sb.center.z = 0;
            sb.radius = 0;
            auto a = make_sphere(sa), b = make_sphere(sb);
            Worlds ws(9);
            Segment seg;
            std::memset(&seg, 0, sizeof seg);
            set_context(0, *ws.side[0], false, false, false, seg);
            set_context(1, *ws.side[1], false, false, false, seg);
            eq("touching", slot_of<SlotOverlaps>(a.o, 1)(a.o, nullptr, b.o), a.r->Overlaps(b.r));
            g_log[0].n = g_log[1].n = 0;
            std::free(a.o); std::free(a.r); std::free(b.o); std::free(b.r);
        }
        g_seed = saved;
    }

    // A node that is not jointed, or has no database: the two reports, then what follows.
    g_context = "not a jointed node";
    for (int problem = 0; problem < 2; problem++) {
        AtBoundingBox ba, bb;
        any_box(&ba, 5, 30, 0);
        any_box(&bb, 5, 30, 0);
        auto a = make_box(ba), b = make_box(bb);
        a.o->m_jointIndex = a.r->m_jointIndex = 1;
        b.o->m_jointIndex = b.r->m_jointIndex = 2;
        for (int which = 0; which < 2; which++) {
            Worlds ws(77);
            for (int side = 0; side < 2; side++) {
                FakeNode& n = which ? ws.side[side]->node : ws.side[side]->otherNode;
                if (problem) n.joints = nullptr; else n.cls = 1;
            }
            Segment seg;
            std::memset(&seg, 0, sizeof seg);
            set_context(0, *ws.side[0], true, true, true, seg);
            set_context(1, *ws.side[1], true, true, true, seg);
            const bool co = guarded([&] { On on(0); slot_of<SlotOverlaps>(a.o, 1)(a.o, nullptr, b.o); });
            const bool cr = guarded([&] { On on(1); a.r->Overlaps(b.r); });
            g_recording = false;
            eq("crash", co, cr);
            compare_logs("reports");
            if (!problem) {
                // Drawing by a joint asks the node's class by the other route, and reports with a message of its own.
                static Drawing* drawing = new Drawing;
                AtSceneNode* n[2] = {(AtSceneNode*)&ws.side[0]->otherNode, (AtSceneNode*)&ws.side[1]->otherNode};
                const bool dco = guarded([&] { On on(0); slot_of<SlotDraw>(a.o, 7)(a.o, nullptr, &g_heap_token, &drawing->device[0], n[0], nullptr); });
                const bool dcr = guarded([&] { On on(1); a.r->DrawTree(&g_heap_token, &drawing->device[1], n[1], nullptr); });
                g_recording = false;
                eq("crash in draw", dco, dcr);
                drawing->same("draw, not jointed");
            }
        }
    }

    // The list full, and leaves only.
    g_context = "contact list";
    {
        AtBoundingGeometry* x = (AtBoundingGeometry*)&g_heap_token;
        typedef void(__fastcall* FnAdd)(void*, void*, void*, void*, void*, void*);
        AtSphereBoundingGeometry p, q, withChild, withChild2;
        p.m_firstChild = q.m_firstChild = nullptr;
        withChild.m_firstChild = x;
        withChild2.m_firstChild = x;
        for (const unsigned flags : {0u, 1u, 2u, 3u}) {
            for (const unsigned capacity : {0u, 1u, 3u}) {
                AtBoundingGeometry* firsts[] = {&p, &withChild, &p, &withChild};
                AtBoundingGeometry* seconds[] = {&q, &q, &withChild2, &withChild2};
                AtBoundingContact itemsO[4], itemsR[4];
                std::memset(itemsO, 0, sizeof itemsO);
                std::memset(itemsR, 0, sizeof itemsR);
                AtBoundingContactList lo = {capacity, 0, flags, itemsO}, lr = {capacity, 0, flags, itemsR};
                for (int i = 0; i < 4; i++) {
                    AtSceneNode* n1 = (AtSceneNode*)(uintptr_t)(0x1000 + i);
                    AtSceneNode* n2 = (AtSceneNode*)(uintptr_t)(0x2000 + i);
                    at<FnAdd>(0x0005CC80)(&lo, nullptr, firsts[i], seconds[i], n1, n2);
                    lr.Add(firsts[i], seconds[i], n1, n2);
                    eq("count", lo.m_count, lr.m_count);
                }
                eq("items", std::memcmp(itemsO, itemsR, sizeof itemsO), 0);
            }
        }
    }
}

// ---- coordinate systems, for this ---------------------------------------------------------------------------------------

static void run_coord() {
    typedef int(__fastcall* FnDrawing)(void*, void*, void*, int, int);
    typedef void(__fastcall* FnInverse)(void*, void*, const void*, void*);
    for (int seed = 0; seed < 40; seed++) {
        g_context = "drawing transform " + std::to_string(seed);
        Worlds ws(500 + seed);
        World& a = *ws.side[0];
        World& b = *ws.side[1];
        AtMatrix base;
        std::memset(&base, 0, sizeof base);
        base.m[0] = base.m[5] = base.m[10] = base.m[15] = 1;
        base.m[12] = (float)seed;
        for (const int generation : {1, 1, 2, 2}) {
            for (const int multiply : {1, 0}) {
                for (const int dirty : {0, 2}) {
                    a.joints[1].m_dirty = b.joints[1].m_dirty = (a.joints[1].m_dirty & ~2u) | dirty;
                    a.parentOfJoints.m_dirty = b.parentOfJoints.m_dirty = (a.parentOfJoints.m_dirty & ~2u) | (dirty && seed & 1 ? 2 : 0);
                    int ro, rr;
                    { On on(0); ro = at<FnDrawing>(0x00032690)(&a.joints[1], nullptr, &base, generation, multiply); }
                    { On on(1); rr = b.joints[1].UpdateDrawing(&base, generation, multiply); }
                    eq("drawing's result", ro, rr);
                    same_coord(a.joints[1], b.joints[1]);
                    same_coord(a.parentOfJoints, b.parentOfJoints);
                    compare_logs("drawing");
                }
            }
        }
        // Up to date in every way but its own generation, with its parent having this one already.
        a.parentOfJoints.m_dirty = b.parentOfJoints.m_dirty = 0;
        a.parentOfJoints.m_unknown20 = b.parentOfJoints.m_unknown20 = 5;
        a.joints[1].m_dirty = b.joints[1].m_dirty = 0;
        a.joints[1].m_unknown20 = b.joints[1].m_unknown20 = 4;
        {
            int ro, rr;
            { On on(0); ro = at<FnDrawing>(0x00032690)(&a.joints[1], nullptr, &base, 5, 1); }
            { On on(1); rr = b.joints[1].UpdateDrawing(&base, 5, 1); }
            eq("own generation behind", ro, rr);
            same_coord(a.joints[1], b.joints[1]);
            compare_logs("own generation behind");
        }
        // Children told, in a list of more than one.
        a.joints[0].m_parent = nullptr; b.joints[0].m_parent = nullptr;
        a.own.Attach(&a.joints[0]); b.own.Attach(&b.joints[0]);
        a.own.Attach(&a.joints[2]); b.own.Attach(&b.joints[2]);
        a.joints[0].m_dirty = b.joints[0].m_dirty = 0;
        a.joints[2].m_dirty = b.joints[2].m_dirty = 0;
        { On on(0); at<FnDrawing>(0x00032690)(&a.own, nullptr, &base, 9, 1); }
        { On on(1); b.own.UpdateDrawing(&base, 9, 1); }
        same_coord(a.joints[0], b.joints[0]);
        same_coord(a.joints[2], b.joints[2]);
        same_coord(a.own, b.own);
        compare_logs("children");
        AtVector3 from, to1, to2;
        from.x = 1.5f; from.y = -2.5f; from.z = 0.25f;
        { On on(0); at<FnInverse>(0x00032810)(&a.joints[1], nullptr, &from, &to1); }
        { On on(1); b.joints[1].TransformDirectionInverse(&from, &to2); }
        eqv("direction", to1, to2);
        same_coord(a.joints[1], b.joints[1]);
        compare_logs("direction");
    }
    // The base's own.
    g_context = "base";
    unsigned char x[sizeof(AtGroupBoundingGeometry)];
    std::memset(x, 0xCD, sizeof x);
    eq("base's constructor returns this", (uintptr_t)at<FnCtor>(0x00053A10)(x, nullptr), (uintptr_t)x);
    eq("base's vftable", *(uint32_t*)x, 0x00276D98);
    class Plain : public AtBoundingGeometry {
    public:
        void ReadWord(AtIoTokenizer*, int) override {}
        int Overlaps(AtBoundingGeometry*) override { return 0; }
        int TestSegment() override { return 0; }
        void CollectSegment() override {}
        double Penetration(AtSceneNode*, AtBoundingGeometry*, AtSceneNode*, AtVector3*, float*, float*) override { return 0; }
        double PenetrationAlong(AtSceneNode*, AtBoundingGeometry*, AtSceneNode*, const AtVector3*, float*, float*) override { return 0; }
    };
    const Plain y;
    eq("base made", std::memcmp(x + 4, (const char*)&y + 4, sizeof x - 4), 0);
    AtSceneNode* n1 = (AtSceneNode*)&g_heap_token;
    AtSceneNode* n2 = (AtSceneNode*)&g_fill;
    *(void**)(uintptr_t)0x002C7EDC = n1;
    *(void**)(uintptr_t)0x002C7ED8 = n2;
    AtBoundingGeometry::s_node = n1;
    AtBoundingGeometry::s_otherNode = n2;
    at<void(__cdecl*)()>(0x000539F0)();
    AtBoundingGeometry::SwapNodes();
    eq("swapped", *(uint32_t*)(uintptr_t)0x002C7EDC, (uint32_t)(uintptr_t)AtBoundingGeometry::s_node);
    eq("swapped other", *(uint32_t*)(uintptr_t)0x002C7ED8, (uint32_t)(uintptr_t)AtBoundingGeometry::s_otherNode);
    eq("really", (uint32_t)(uintptr_t)AtBoundingGeometry::s_node, (uint32_t)(uintptr_t)n2);
}

// ---- the list of hits -----------------------------------------------------------------------------------------------------

typedef void(__fastcall* FnP1)(void*, void*, unsigned);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnStorage)(void*, void*, unsigned, void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

static long long inner(const AtSegmentHitList* l, const void* p) { return (const char*)p - (const char*)l; }

static void same_list(const AtSegmentHitList* a, const AtSegmentHitList* b, bool keysToo) {
    eq("flags", a->m_flags, b->m_flags);
    eq("vectors 0", std::memcmp(a->m_vectors0, b->m_vectors0, sizeof a->m_vectors0), 0);
    eq("vectors 1", std::memcmp(a->m_vectors1, b->m_vectors1, sizeof a->m_vectors1), 0);
    eq("vectors 2", std::memcmp(a->m_vectors2, b->m_vectors2, sizeof a->m_vectors2), 0);
    eq("first 0", inner(a, a->m_first0), inner(b, b->m_first0));
    eq("second 0", inner(a, a->m_second0), inner(b, b->m_second0));
    eq("first 2", inner(a, a->m_first2), inner(b, b->m_first2));
    eq("second 2", inner(a, a->m_second2), inner(b, b->m_second2));
    eq("unknown", std::memcmp(a->m_unknown60, b->m_unknown60, sizeof a->m_unknown60), 0);
    eq("capacity", a->m_capacity, b->m_capacity);
    eq("current", a->m_current, b->m_current);
    eq("count", a->m_count, b->m_count);
    eq("8C", a->m_unknown8C, b->m_unknown8C);
    eq("keys held", a->m_keys != nullptr, b->m_keys != nullptr);
    eq("hits held", a->m_hits != nullptr, b->m_hits != nullptr);
    if (keysToo && a->m_keys && b->m_keys && a->m_capacity == b->m_capacity) eq("keys", std::memcmp(a->m_keys, b->m_keys, a->m_capacity * 8), 0);
}

static void disturb(AtSegmentHitList* l) {
    std::memset(l->m_vectors0, 0x11, sizeof l->m_vectors0);
    std::memset(l->m_vectors1, 0x22, sizeof l->m_vectors1);
    std::memset(l->m_vectors2, 0x33, sizeof l->m_vectors2);
    std::memset(l->m_unknown60, 0x44, sizeof l->m_unknown60);
    l->m_first0 = l->m_second0 = l->m_first2 = l->m_second2 = nullptr;
    l->m_count = 2;
    l->m_current = 3;
    l->m_unknown8C = 5;
}

static void run_hit_list() {
    g_context = "hit list";
    g_fill = 0xCD;
    unsigned char ob[sizeof(AtSegmentHitList)], rb[sizeof(AtSegmentHitList)];
    AtSegmentHitList* a = (AtSegmentHitList*)ob;
    AtSegmentHitList* b = (AtSegmentHitList*)rb;
    const auto make = [&] {
        std::memset(ob, 0xCD, sizeof ob);
        std::memset(rb, 0xCD, sizeof rb);
        at<FnCtor>(0x000555A0)(ob, nullptr);
        new (rb) AtSegmentHitList;
        eq("vftable", *(uint32_t*)ob, 0x00276F28);
        same_list(a, b, false);
    };
    make();
    for (const unsigned n : {3u, 5u, 1u}) {
        { On on(0); at<FnP1>(0x000553A0)(a, nullptr, n); }
        { On on(1); b->Allocate(n); }
        same_list(a, b, true);
        compare_logs("allocated");
        for (unsigned i = 0; i < n + 2; i++) {
            at<Fn0>(0x00055340)(a, nullptr);
            b->CountOne();
            eq("counted", a->m_count, b->m_count);
        }
    }
    { On on(0); at<Fn0>(0x000555B0)(a, nullptr); }
    { On on(1); b->Clear(); }
    same_list(a, b, false);
    compare_logs("cleared");
    disturb(a); disturb(b);
    { On on(0); at<Fn0>(0x000555B0)(a, nullptr); }
    { On on(1); b->Clear(); }
    same_list(a, b, false);
    compare_logs("cleared, disturbed");
    int k1[8], k2[8];
    { On on(0); at<FnP1>(0x000553A0)(a, nullptr, 3); }
    { On on(1); b->Allocate(3); }
    compare_logs("owned");
    disturb(a); disturb(b);
    { On on(0); at<FnStorage>(0x00055360)(a, nullptr, 4, k1, k2); }
    { On on(1); b->SetStorage(4, k1, k2); }
    same_list(a, b, false);
    compare_logs("storage after owned");
    { On on(0); at<Fn0>(0x000555B0)(a, nullptr); }
    { On on(1); b->Clear(); }
    compare_logs("clear");
    { On on(0); at<FnStorage>(0x00055360)(a, nullptr, 4, k1, k2); }
    { On on(1); b->SetStorage(4, k1, k2); }
    eq("storage keys", (uintptr_t)a->m_keys, (uintptr_t)k1);
    eq("storage keys", (uintptr_t)b->m_keys, (uintptr_t)k1);
    eq("storage hits", (uintptr_t)a->m_hits, (uintptr_t)k2);
    eq("storage hits", (uintptr_t)b->m_hits, (uintptr_t)k2);
    same_list(a, b, false);
    compare_logs("storage");
    { On on(0); at<FnStorage>(0x00055360)(a, nullptr, 2, k2, k1); }
    { On on(1); b->SetStorage(2, k2, k1); }
    same_list(a, b, false);
    compare_logs("storage twice");
    { On on(0); at<FnP1>(0x000553A0)(a, nullptr, 2); }
    { On on(1); b->Allocate(2); }
    same_list(a, b, true);
    compare_logs("owned after not owned");
    { On on(0); at<Fn0>(0x000555F0)(a, nullptr); }
    { On on(1); b->~AtSegmentHitList(); }
    same_list(a, b, false);
    compare_logs("destroyed, owned");
    make();
    { On on(0); at<FnStorage>(0x00055360)(a, nullptr, 2, k2, k1); }
    { On on(1); b->SetStorage(2, k2, k1); }
    { On on(0); at<Fn0>(0x000555B0)(a, nullptr); }
    { On on(1); b->Clear(); }
    compare_logs("clear, not owned");
    { On on(0); at<FnStorage>(0x00055360)(a, nullptr, 2, k2, k1); }
    { On on(1); b->SetStorage(2, k2, k1); }
    { On on(0); at<Fn0>(0x000555F0)(a, nullptr); }
    { On on(1); b->~AtSegmentHitList(); }
    compare_logs("destroyed, not owned");
    // The deleting form, each way.
    {
        unsigned char* p = (unsigned char*)std::malloc(sizeof(AtSegmentHitList));
        at<FnCtor>(0x000555A0)(p, nullptr);
        AtSegmentHitList* q = new AtSegmentHitList;
        { On on(0); eq("deleting returns this", (uintptr_t)at<FnDel>(0x00057440)(p, nullptr, 1), (uintptr_t)p); }
        { On on(1); delete q; }
        compare_logs("deleted");
        make();
        { On on(0); eq("not freed returns this", (uintptr_t)at<FnDel>(0x00057440)(a, nullptr, 0), (uintptr_t)a); }
        { On on(1); b->~AtSegmentHitList(); }
        compare_logs("not freed");
    }
    // As an array: the count ahead, destroyed last first; no recovered twin, the compiler's own array delete being the other side.
    {
        unsigned char* block = (unsigned char*)std::malloc(4 + 2 * sizeof(AtSegmentHitList));
        *(uint32_t*)block = 2;
        for (int i = 0; i < 2; i++) at<FnCtor>(0x000555A0)(block + 4 + i * sizeof(AtSegmentHitList), nullptr);
        at<FnP1>(0x000553A0)(block + 4, nullptr, 1);
        AtSegmentHitList* many = new AtSegmentHitList[2];
        many[0].Allocate(1);
        g_log[0].n = g_log[1].n = 0;
        { On on(0); eq("array returns the block", (uintptr_t)at<FnDel>(0x00057440)(block + 4, nullptr, 3), (uintptr_t)block); }
        { On on(1); delete[] many; }
        compare_logs("array deleted");
    }
    // No room, no heap.
    make();
    g_fail_alloc = true;
    bool oc = guarded([&] { On on(0); at<FnP1>(0x000553A0)(a, nullptr, 2); });
    g_recording = false;
    bool rc = guarded([&] { On on(1); b->Allocate(2); });
    g_recording = false;
    g_fail_alloc = false;
    eq("crash on no room", oc, rc);
    compare_logs("no room");
    make();
    // For none: nothing to clear, so both raises come.
    g_fail_alloc = true;
    oc = guarded([&] { On on(0); at<FnP1>(0x000553A0)(a, nullptr, 0); });
    g_recording = false;
    rc = guarded([&] { On on(1); b->Allocate(0); });
    g_recording = false;
    g_fail_alloc = false;
    eq("crash on no room for none", oc, rc);
    compare_logs("no room for none");
    make();
    at<void(__cdecl*)(void*)>(0x000552A0)(nullptr);
    AtSegmentHitList::SetHeap(nullptr);
    { On on(0); at<FnP1>(0x000553A0)(a, nullptr, 2); }
    { On on(1); b->Allocate(2); }
    compare_logs("no heap");
    at<void(__cdecl*)(void*)>(0x000552A0)(&g_heap_token);
    AtSegmentHitList::SetHeap(&g_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002C7F58, (uintptr_t)AtSegmentHitList::s_heap);
}

// ---- files and text -------------------------------------------------------------------------------------------------------

typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*);

struct Owners {
    AtNameTableOwner* side[2];
    Owners() {
        for (int i = 0; i < 2; i++) {
            side[i] = new AtNameTableOwner;
            side[i]->m_geometrySpaces.Init(8);
        }
    }
    ~Owners() {
        g_recording = false;
        delete side[0];
        delete side[1];
    }
};

static std::string group_name(GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static unsigned g_seen_boxes, g_seen_spheres, g_seen_groups;

// Where a pointer leads in a database: a kind and an index. Past the array's count is fine, for a list longer than its count.
static long long geometry_id(const AtBoundingGeometryDatabase* d, const void* p) {
    if (!p) return -1;
    const char* c = (const char*)p;
    if (d->m_boxes && c >= (const char*)d->m_boxes && c < (const char*)(d->m_boxes + 64)) return 0x1000 + (c - (const char*)d->m_boxes);
    if (d->m_spheres && c >= (const char*)d->m_spheres && c < (const char*)(d->m_spheres + 64)) return 0x2000 + (c - (const char*)d->m_spheres);
    if (d->m_groups && c >= (const char*)d->m_groups && c < (const char*)(d->m_groups + 64)) return 0x3000 + (c - (const char*)d->m_groups);
    return -2;
}

static void same_geometry(const AtBoundingGeometryDatabase* o, const AtBoundingGeometryDatabase* r, const AtBoundingGeometry* a, const AtBoundingGeometry* b) {
    eq("kind", a->m_kind, b->m_kind);
    eq("joint", a->m_jointIndex, b->m_jointIndex);
    eq("+0C", a->m_unknown0C, b->m_unknown0C);
    eq("parent", geometry_id(o, a->m_parent), geometry_id(r, b->m_parent));
    eq("sibling", geometry_id(o, a->m_nextSibling), geometry_id(r, b->m_nextSibling));
    eq("child", geometry_id(o, a->m_firstChild), geometry_id(r, b->m_firstChild));
    eq("database", a->m_database == o, b->m_database == r);
}

static void same_db(const char* what, const AtBoundingGeometryDatabase* o, const AtBoundingGeometryDatabase* r, AtNameTableOwner* oo, AtNameTableOwner* ro) {
    g_recording = false;
    eq("box count", o->m_boxCount, r->m_boxCount);
    eq("sphere count", o->m_sphereCount, r->m_sphereCount);
    eq("group count", o->m_groupCount, r->m_groupCount);
    eq("boxes", o->m_boxes != nullptr, r->m_boxes != nullptr);
    eq("spheres", o->m_spheres != nullptr, r->m_spheres != nullptr);
    eq("groups", o->m_groups != nullptr, r->m_groups != nullptr);
    eq("size", o->m_size, r->m_size);
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eq("in the plain group", o->m_group == &oo->m_geometry, r->m_group == &ro->m_geometry);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
    if (o->m_boxes && r->m_boxes && o->m_boxCount == r->m_boxCount && o->m_boxCount < 4096) {
        for (unsigned i = 0; i < o->m_boxCount; i++) {
            eq("box's vftable", *(uint32_t*)&o->m_boxes[i], kBoxTable);
            same_geometry(o, r, &o->m_boxes[i], &r->m_boxes[i]);
            eqv("box's centre", o->m_boxes[i].m_box.m_center, r->m_boxes[i].m_box.m_center);
            eqv("box's size", o->m_boxes[i].m_box.m_half, r->m_boxes[i].m_box.m_half);
        }
    }
    if (o->m_spheres && r->m_spheres && o->m_sphereCount == r->m_sphereCount && o->m_sphereCount < 4096) {
        for (unsigned i = 0; i < o->m_sphereCount; i++) {
            eq("sphere's vftable", *(uint32_t*)&o->m_spheres[i], kSphereTable);
            same_geometry(o, r, &o->m_spheres[i], &r->m_spheres[i]);
            eqv("sphere's centre", o->m_spheres[i].m_sphere.center, r->m_spheres[i].m_sphere.center);
            eqf("sphere's radius", o->m_spheres[i].m_sphere.radius, r->m_spheres[i].m_sphere.radius);
        }
    }
    if (o->m_groups && r->m_groups && o->m_groupCount == r->m_groupCount && o->m_groupCount < 4096) {
        for (unsigned i = 0; i < o->m_groupCount; i++) {
            eq("group's vftable", *(uint32_t*)&o->m_groups[i], kGroupTable);
            same_geometry(o, r, &o->m_groups[i], &r->m_groups[i]);
        }
    }
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", std::strncmp(x.m_name, y.m_name, 8), 0);
                eq("slot's geometry", geometry_id(o, x.m_value), geometry_id(r, y.m_value));
            }
        }
    }
    compare_logs(what);
}

static AtBoundingGeometryDatabase* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtBoundingGeometryDatabase));
    std::memset(o, fill, sizeof(AtBoundingGeometryDatabase));
    eq("constructor returns this", (uintptr_t)at<FnCtor>(0x00040A70)(o, nullptr), (uintptr_t)o);
    eq("database's vftable", *(uint32_t*)o, 0x002761F8);
    return (AtBoundingGeometryDatabase*)o;
}
static AtBoundingGeometryDatabase* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(AtBoundingGeometryDatabase));
    std::memset(r, fill, sizeof(AtBoundingGeometryDatabase));
    return new (r) AtBoundingGeometryDatabase;
}
static void destroy(AtBoundingGeometryDatabase* o, AtBoundingGeometryDatabase* r, const char* what, int mode) {
    if (mode == 0) {
        { On on(0); at<Fn0>(0x00041090)(o, nullptr); }
        { On on(1); r->~AtBoundingGeometryDatabase(); }
        compare_logs(what);
        std::free(o);
        std::free(r);
    } else {
        // The deleting form: the two fills of its flag.
        { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x0002ECC0)(o, nullptr, 0), (uintptr_t)o); }
        { On on(1); r->~AtBoundingGeometryDatabase(); }
        compare_logs(what);
        std::free(o);
        std::free(r);
    }
}

static void note_counts(const AtBoundingGeometryDatabase* r) {
    g_seen_boxes += r->m_boxCount;
    g_seen_spheres += r->m_sphereCount;
    g_seen_groups += r->m_groupCount;
}

static unsigned g_files, g_file_pairs;

// The geometries of a real file, against each other, in a world.
static void exercise_file(AtBoundingGeometryDatabase* o, AtBoundingGeometryDatabase* r, uint32_t seed) {
    std::vector<std::pair<AtBoundingGeometry*, AtBoundingGeometry*>> all;
    for (unsigned i = 0; i < o->m_boxCount && i < 3; i++) all.push_back({&o->m_boxes[i], &r->m_boxes[i]});
    for (unsigned i = 0; i < o->m_sphereCount && i < 2; i++) all.push_back({&o->m_spheres[i], &r->m_spheres[i]});
    for (unsigned i = 0; i < o->m_groupCount && i < 1; i++) all.push_back({&o->m_groups[i], &r->m_groups[i]});
    Segment seg;
    seg.start.x = -300; seg.start.y = 10; seg.start.z = 20;
    seg.end.x = 300; seg.end.y = -10; seg.end.z = 40;
    seg.direction.x = 0.99; seg.direction.y = -0.03f; seg.direction.z = 0.03f;
    seg.middle.x = 0; seg.middle.y = 0; seg.middle.z = 30;
    seg.half = 300;
    for (auto& a : all) {
        for (auto& b : all) {
            exercise_pair(a.first, b.first, a.second, b.second, seed, true, true, true, seg, false);
            g_file_pairs++;
        }
    }
}

static void run_file(const std::string& data, const std::string& context, unsigned char fill, bool exercise) {
    g_context = context;
    Owners own;
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    AtBoundingGeometryDatabase* o = make_o(fill);
    AtBoundingGeometryDatabase* r = make_r(fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtBoundingGeometryDatabase) - 4), 0);
    for (int pass = 0; pass < 2; pass++) {
        { On on(0); at<FnLoad>(0x00040F90)(o, nullptr, own.side[0], "ART\\THING", 77); }
        { On on(1); r->Load(own.side[1], "ART\\THING", 77); }
        same_db("load", o, r, own.side[0], own.side[1]);
    }
    if (fill == 0xCD) note_counts(r);
    if (exercise) exercise_file(o, r, g_files);
    { On on(0); at<Fn0>(0x00041030)(o, nullptr); at<Fn0>(0x00041030)(o, nullptr); }
    { On on(1); r->Clear(); r->Clear(); }
    same_db("clear", o, r, own.side[0], own.side[1]);
    g_recording = false;
    GmNameSpaceDb::Remove(o);
    GmNameSpaceDb::Remove(r);
    destroy(o, r, "destroy", fill == 0xCD ? 0 : 1);
}

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class DirectTokenizer : public AtIoTextTokenizer {
public:
    int m_errors = 0;
    void Error(int code) override {
        g_log[g_side].add('E', code);
        if (++m_errors > 12) *(volatile int*)8 = 0;  // Does not come back, as in the game. guarded() takes it from here.
    }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const std::string& t) : text(t) {
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

static void run_text(const std::string& text, unsigned char fill) {
    g_context = "text [" + text + "]";
    Owners own;
    g_fill = fill;
    AtBoundingGeometryDatabase* o = make_o(fill);
    AtBoundingGeometryDatabase* r = make_r(fill);
    bool crashed = false;
    for (int pass = 0; pass < 2 && !crashed; pass++) {
        Direct da(text), db(text);
        const bool oc = guarded([&] { On on(0); at<FnRead>(0x00040AA0)(o, nullptr, own.side[0], &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Read(own.side[1], &db.tokenizer); });
        g_recording = false;
        g_side = 0;
        eq("crash", oc, rc);
        crashed = oc || rc;
        same_db("read", o, r, own.side[0], own.side[1]);
        eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
    }
    if (crashed) return;  // What was half made stays: a crash here has no way back in the game.
    note_counts(r);
    GmNameSpaceDb::Remove(o);
    GmNameSpaceDb::Remove(r);
    destroy(o, r, "destroy", 0);
}

static std::string one_box(const std::string& body) { return "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n{\nBoxBoundingGeometry \"A\"\n{\n" + body + "\n}\n}\n"; }
static std::string one_sphere(const std::string& body) { return "SphereBoundingGeometry[1]\nBoundingGeometry[1]\n{\nSphereBoundingGeometry \"A\"\n{\n" + body + "\n}\n}\n"; }
static std::string one_group(const std::string& body) { return "GroupBoundingGeometry[1]\nBoundingGeometry[1]\n{\nGroupBoundingGeometry \"A\"\n{\n" + body + "\n}\n}\n"; }

static void run_written() {
    std::vector<std::string> texts = {
        "",
        "NameSpace \"Space\"",
        "NameSpace \"Space\"\nBoxBoundingGeometry[2]",
        "BoxBoundingGeometry[2]\nSphereBoundingGeometry[1]\nGroupBoundingGeometry[3]",
        "BoundingGeometry[0]\n{\n}\n",
        "NameSpace \"Space\"\nBoundingGeometry[0]\n{\n}\n",
        "NameSpace \"\"\nBoundingGeometry[0]\n{\n}\n",
        "NameSpace \"TooLongForEightBytes\"\nGroupBoundingGeometry[1]\nBoundingGeometry[1]\n{\nGroupBoundingGeometry \"LongerThanEightBytes\"\n{\n}\n}\n",
        "BoxBoundingGeometry[0]\nBoundingGeometry[0]\n{\n}\n",
        "SphereBoundingGeometry[0]\nBoundingGeometry[0]\n{\n}\n",
        "GroupBoundingGeometry[0]\nBoundingGeometry[0]\n{\n}\n",
        "GroupBoundingGeometry[1]\nBoxBoundingGeometry[1]\nSphereBoundingGeometry[1]\nBoundingGeometry[3]\n{\nGroupBoundingGeometry \"G\"\n{\n}\nBoxBoundingGeometry \"B\"\n{\nParent \"G\"\nRotTransCoordSys 2\nBox 1.5 2.5 3.5 4.5 5.5 6.5\n}\nSphereBoundingGeometry \"S\"\n{\nParent \"G\"\nSphere 1.25 2.25 3.25 4.25\n}\n}\n",
        // A list given twice, of each kind.
        "BoxBoundingGeometry[1]\nBoxBoundingGeometry[2]\nBoundingGeometry[0]\n{\n}\n",
        "SphereBoundingGeometry[1]\nSphereBoundingGeometry[2]\nBoundingGeometry[0]\n{\n}\n",
        "GroupBoundingGeometry[1]\nGroupBoundingGeometry[2]\nBoundingGeometry[0]\n{\n}\n",
        // Words not of this place, and the words nobody reads.
        "ConeBoundingGeometry[1]\nBoundingGeometry[0]\n{\n}\n",
        "CylinderBoundingGeometry[1]\nBoundingGeometry[0]\n{\n}\n",
        "Box\nBoundingGeometry[0]\n{\n}\n",
        "Bogus\nBoundingGeometry[0]\n{\n}\n",
        "7\nBoundingGeometry[0]\n{\n}\n",
        "BoxBoundingGeometry 1]\nBoundingGeometry[0]\n{\n}\n",
        "BoxBoundingGeometry[1\nBoundingGeometry[0]\n{\n}\n",
        // In the list: too many of a kind, more than all, words that are not kinds.
        "BoundingGeometry[1]\n{\n}\n",
        "GroupBoundingGeometry[1]\nBoundingGeometry[2]\n{\nGroupBoundingGeometry \"A\"\n{\n}\n}\n",
        "GroupBoundingGeometry[1]\nBoundingGeometry[1]\n{\nConeBoundingGeometry\n}\n",
        "GroupBoundingGeometry[1]\nBoundingGeometry[1]\n{\nBogus\n}\n",
        "GroupBoundingGeometry[2]\nBoundingGeometry[1]\n{\nGroupBoundingGeometry \"A\"\n{\n}\nGroupBoundingGeometry \"B\"\n{\n}\n}\n",
        "GroupBoundingGeometry[1]\nBoxBoundingGeometry[1]\nBoundingGeometry[2]\n{\nGroupBoundingGeometry \"A\"\n{\n}\nGroupBoundingGeometry \"B\"\n{\n}\n}\n",
        "GroupBoundingGeometry[1]\nBoxBoundingGeometry[1]\nBoundingGeometry[2]\n{\nBoxBoundingGeometry \"A\"\n{\n}\nBoxBoundingGeometry \"B\"\n{\n}\n}\n",
        "GroupBoundingGeometry[1]\nSphereBoundingGeometry[1]\nBoundingGeometry[2]\n{\nSphereBoundingGeometry \"A\"\n{\n}\nSphereBoundingGeometry \"B\"\n{\n}\n}\n",
        "GroupBoundingGeometry[2]\nBoundingGeometry[2]\n{\nGroupBoundingGeometry \"A\"\n{\n}\nGroupBoundingGeometry \"A\"\n{\n}\n}\n",
        "BoxBoundingGeometry[2]\nSphereBoundingGeometry[2]\nGroupBoundingGeometry[2]\nBoundingGeometry[6]\n{\nBoxBoundingGeometry \"B0\"\n{\n}\nSphereBoundingGeometry \"S0\"\n{\n}\nGroupBoundingGeometry \"G0\"\n{\n}\nBoxBoundingGeometry \"B1\"\n{\nParent \"G0\"\n}\nSphereBoundingGeometry \"S1\"\n{\nParent \"B0\"\n}\nGroupBoundingGeometry \"G1\"\n{\nParent \"S0\"\n}\n}\n",
        // Parents.
        "GroupBoundingGeometry[3]\nBoundingGeometry[3]\n{\nGroupBoundingGeometry \"A\"\n{\n}\nGroupBoundingGeometry \"B\"\n{\nParent \"A\"\n}\nGroupBoundingGeometry \"C\"\n{\nParent \"A\"\n}\n}\n",
        "GroupBoundingGeometry[3]\nBoundingGeometry[3]\n{\nGroupBoundingGeometry \"A\"\n{\n}\nGroupBoundingGeometry \"B\"\n{\n}\nGroupBoundingGeometry \"C\"\n{\nParent \"A\"\nParent \"B\"\nParent \"B\"\n}\n}\n",
        one_group("Parent \"A\""),
        one_group("Parent \"Nobody\""),
        one_group("RotTransCoordSys 3"),
        one_group("RotTransCoordSys"),
        one_group("RotTransCoordSys 1.5"),
        one_group("Box 1 2 3 4 5 6"),
        one_group("Sphere 1 2 3 4"),
        one_group("Bogus"),
        one_group("7"),
        one_box(""),
        one_box("Box 1.5 -2.5 3.5 -4.5 5.5 -6.5"),
        one_box("Box -1.5 2.5 -3.5 4.5 -5.5 6.5 RotTransCoordSys 2"),
        one_box("Box 1 2 3 4 5 6"),
        one_box("Box 1.5 2.5"),
        one_box("BoxMinMax -1.5 -2.5 -3.5 4.25 5.25 6.25"),
        one_box("BoxMinMax 4.25 5.25 6.25 -1.5 -2.5 -3.5"),
        one_box("BoxMinMax 0.1 0.2 0.3 0.7 0.9 1.1"),
        one_box("BoxMinMax 1.5"),
        one_box("Box 1.5 2.5 3.5 4.5 5.5 6.5 BoxMinMax 0.5 0.5 0.5 1.5 2.5 3.5"),
        one_box("Sphere 1.5 2.5 3.5 4.5"),
        one_box("Cone"),
        one_box("Cylinder"),
        one_box("Bogus"),
        one_sphere(""),
        one_sphere("Sphere 1.5 -2.5 3.5 4.5"),
        one_sphere("Sphere 1.5 2.5 3.5 -4.5"),
        one_sphere("Sphere 1.5 -2.5 3.5 -4.5 RotTransCoordSys 1"),
        one_sphere("Sphere 1 2 3 4"),
        one_sphere("Sphere 1.5"),
        one_sphere("Box 1.5 2.5 3.5 4.5 5.5 6.5"),
        one_sphere("BoxMinMax 1.5 2.5 3.5 4.5 5.5 6.5"),
        one_sphere("Bogus"),
        // Cut short.
        "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n{\nBoxBoundingGeometry \"A\"\n{\n",
        "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n{\nBoxBoundingGeometry \"A\"\n",
        "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n{\n",
        "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n",
        "BoxBoundingGeometry[1]\nBoundingGeometry",
        "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n{\nBoxBoundingGeometry \"A\"\n{\n}\nExtra",
        "BoxBoundingGeometry[1]\nBoundingGeometry[1]\n{\nBoxBoundingGeometry \"A\"\n{\n}\n}\nExtra",
        // Not here: a name that is not a string. ExpectName then leaves the name's stack bytes as they were, which differ between the sides.
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const std::string& t : texts) run_text(t, fill);
    }
}

// Memory runs out, and no heap: a raise, then what the reader does with null.
static void run_no_room() {
    for (const std::string& text : {std::string("BoxBoundingGeometry[1]\nBoundingGeometry[0]\n{\n}\n"), std::string("SphereBoundingGeometry[1]\nBoundingGeometry[0]\n{\n}\n"), std::string("GroupBoundingGeometry[1]\nBoundingGeometry[0]\n{\n}\n"), one_box("Box 1.5 2.5 3.5 4.5 5.5 6.5")}) {
        g_context = "no room [" + text + "]";
        Owners own;
        AtBoundingGeometryDatabase* o = make_o(0xCD);
        AtBoundingGeometryDatabase* r = make_r(0xCD);
        Direct da(text), db(text);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnRead>(0x00040AA0)(o, nullptr, own.side[0], &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Read(own.side[1], &db.tokenizer); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room");
    }
    at<void(__cdecl*)(void*)>(0x00040A60)(nullptr);
    AtBoundingGeometryDatabase::SetHeap(nullptr);
    run_text("BoxBoundingGeometry[1]\nSphereBoundingGeometry[1]\nGroupBoundingGeometry[1]\nBoundingGeometry[0]\n{\n}\n", 0xCD);
    at<void(__cdecl*)(void*)>(0x00040A60)(&g_heap_token);
    AtBoundingGeometryDatabase::SetHeap(&g_heap_token);
    eq("heap", (uintptr_t)*(void**)(uintptr_t)0x002C6428, (uintptr_t)AtBoundingGeometryDatabase::s_heap);
}

static void run_no_file() {
    g_context = "no file";
    Owners own;
    for (int status : {1, 2, 5}) {
        g_fs->open_status = status;
        AtBoundingGeometryDatabase* o = make_o(0xCD);
        AtBoundingGeometryDatabase* r = make_r(0xCD);
        const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00040F90)(o, nullptr, own.side[0], "ART\\THING", 77); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Load(own.side[1], "ART\\THING", 77); });
        g_recording = false;
        eq("crash on no tokenizer", oc, rc);
        compare_logs("no file");
        g_fs->open_status = 0;
    }
}

static void run_owner() {
    g_context = "owner";
    typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);
    Owners own;
    GmNameMap a, b, c, d;
    for (const char* ns : {(const char*)nullptr, "Space", ""}) {
        at<FnAdd>(0x000400B0)(own.side[0], nullptr, &a, ns);
        own.side[1]->AddGeometry(&b, ns);
        eq("in the plain group", a.m_group == &own.side[0]->m_geometry, b.m_group == &own.side[1]->m_geometry);
        eqs("group", group_name(a.m_group).c_str(), group_name(b.m_group).c_str());
    }
    GmNameSpaceDb::Remove(&a);
    GmNameSpaceDb::Remove(&b);
}

static void run_tables() {
    g_context = "tables";
    const AtIoKeyword* image = (const AtIoKeyword*)(uintptr_t)0x002B21A0;
    for (int i = 0; i < AtBoundingGeometryDatabase::kKeywordCount; i++) {
        eq("keyword id", image[i].m_id, AtBoundingGeometryDatabase::s_keywords[i].m_id);
        eqs("keyword", image[i].m_name, AtBoundingGeometryDatabase::s_keywords[i].m_name);
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
    if (pos.size() != 2) { std::printf("usage: oracle_asb <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    set_matrix_table();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6428u, 0x002C7F58u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtBoundingGeometryDatabase::s_heap = &g_heap_token;
    AtSegmentHitList::s_heap = &g_heap_token;

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_tables();
    run_owner();
    run_formats();
    run_draw();
    run_hit_list();
    run_coord();
    run_box_edges();
    run_near_parallel();
    run_box_math(4000);
    run_slots(64);
    run_written();
    run_no_room();
    run_no_file();

    g_seen_boxes = g_seen_spheres = g_seen_groups = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".ASB\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::string data((size_t)std::ftell(f), '\0');
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
        std::fclose(f);
        run_file(data, line, 0xCD, true);
        run_file(data, line, 0x33, false);
        g_files++;
    }
    std::fclose(index);
    report_entries();
    std::printf("seen: %u overlaps and %u misses, faces 0x%X, %u edge normals, %u real pairs, %u contacts, %u hits, %u normals not compared\n", g_seen_overlap[1], g_seen_overlap[0], g_seen_faces, g_seen_edge, g_file_pairs, g_slot_calls[1], g_slot_calls[3], g_unset_normals);
    std::printf("%s: %u files, %u boxes, %u spheres, %u groups, each file on two fills and read twice on each; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_seen_boxes, g_seen_spheres, g_seen_groups, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
