// Host test of the matrix device, its pool, and the last five matrix-table slots, on any pointer size. Not evidence of a byte match.
//
//   test_matdev [<text cases directory>]
//
// With a directory (the oracle's text cases, with index.txt), it also reads every .AKC listed there with the recovered device and the recovered matrix functions, and says what happened.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMatrixDevice.h"
#include "gm/GmNameMap.h"
#include "matdev_cases.h"

using namespace matdev_cases;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// The original's results on oracle_matdev's first round: printed by it as "hash of the original's products and rotations to quaternions".
static const unsigned long long kOriginalHash = 0x7C6CAC824FFEBFB0ULL;
static const int kRounds = 200000;

namespace {

unsigned char g_heapStorage[sizeof(AtHeap)];
AtHeap* const g_poolHeap = (AtHeap*)(void*)g_heapStorage;
int g_deviceHeap;

bool identity(const AtMatrix* m) {
    for (int i = 0; i < 16; i++) {
        if (m->m[i] != ((i % 5) == 0 ? 1.0f : 0.0f)) return false;
    }
    return true;
}

void TestDevice() {
    AtMatrixDeviceXbox::SetHeap(&g_deviceHeap);
    AtMatrixPoolXbox::SetHeap(g_poolHeap);
    AtMatrixDeviceXbox device;
    AtMatrixDevice* base = &device;
    CHECK(device.m_unknown04 == 0);
    AtMatrix* m = base->NewMatrix();
    CHECK(m != nullptr && identity(m));
    base->FreeMatrix(m);
    m = device.DoNewMatrix();
    CHECK(m != nullptr && identity(m));
    device.ReleaseMatrix(m);

    AtMatrixPool* pool = base->NewPool();
    CHECK(pool != nullptr && pool->Count() == 0);
    AtMatrixPoolXbox* xpool = (AtMatrixPoolXbox*)pool;
    CHECK(xpool->IsReserved() == 0);
    g_poolHeap->m_alignShift = 3;
    g_poolHeap->m_allocShift = 99;
    pool->Reserve(5);
    CHECK(pool->Count() == 5 && xpool->IsReserved() == 1);
    CHECK(g_poolHeap->m_allocShift == 3);  // Set back to the alignment shift, not to what it was.
    for (unsigned i = 0; i < 5; i++) {
        CHECK(identity(pool->At(i)));
        CHECK(pool->At(i) == xpool->Matrix(i));
        CHECK((char*)pool->At(i) - (char*)pool->At(0) == (long)(64 * i));
    }
    g_poolHeap->m_alignShift = 9;
    pool->Reserve(2);  // Frees the first block.
    CHECK(pool->Count() == 2 && g_poolHeap->m_allocShift == 9);
    pool->Reserve(0x80000001u);  // 64 bytes after the size wraps, and nothing constructed, as the original.
    CHECK(pool->Count() == 0x80000001u && xpool->m_matrices != nullptr);
    pool->Clear();
    CHECK(pool->Count() == 0 && xpool->IsReserved() == 0);
    pool->Reserve(1);
    base->FreePool(pool);
    base->FreePool(nullptr);
    device.FreeVertexArray(nullptr);
    device.FreeIndexArray(nullptr);
}

// The arithmetic, against the original's hash: the two products and the matrix to quaternion. The rotations take the host's sine and cosine, which on x64 are the C library's and not the x87's, so they
// are checked by structure here, and by bits only by the oracle on x86.
void TestArithmetic() {
    Rng rng{4242};
    Hash hash;
    for (int i = 0; i < kRounds; i++) {
        Inputs in;
        draw(rng, 0, &in);
        AtMatrix o = in.a;
        AtMatrixPreMultiply3x3(&o, in.rows);
        for (float f : o.m) hash.add(f);
        o = in.a;
        AtMatrixPostMultiply3x3(&o, in.rows);
        for (float f : o.m) hash.add(f);
        AtQuaternion q;
        std::memcpy(&q, in.start.m, sizeof q);
        g_atMatrix.GetRotation(&in.b, &q);
        hash.add(q.x); hash.add(q.y); hash.add(q.z); hash.add(q.w);
    }
    std::printf("hash %016llX, original's %016llX\n", (unsigned long long)hash.h, kOriginalHash);
    CHECK(hash.h == kOriginalHash);

    // Rotations by 0: exact on any host. Pre: rows 0-2 are kept, all four columns, and row 3 untouched; post: columns 0-2 kept and column 3 untouched.
    AtMatrix m, r;
    for (int i = 0; i < 16; i++) m.m[i] = (float)(i + 1) * 0.75f;
    void (*kRotations[])(AtMatrix*, float) = {g_atMatrix.PreRotateX, g_atMatrix.PreRotateY, g_atMatrix.PreRotateZ, g_atMatrix.PostRotateZ};
    for (auto rotate : kRotations) {
        r = m;
        rotate(&r, 0.0f);
        CHECK(std::memcmp(&r, &m, sizeof m) == 0);
    }
    // A quarter turn about z, by structure: the rotation's rows are (c -s 0) (s c 0) (0 0 1).
    r = m;
    g_atMatrix.PreRotateZ(&r, 1.5707963705062866f);
    CHECK(std::fabs(r.m[0] - m.m[4] * -1.0f) < 1e-5f || std::fabs(r.m[0] + m.m[4]) < 1e-5f);
    CHECK(std::fabs(r.m[4] - m.m[0]) < 1e-5f && r.m[8] == m.m[8] && r.m[12] == m.m[12]);
    r = m;
    g_atMatrix.PostRotateZ(&r, 1.5707963705062866f);
    CHECK(std::fabs(r.m[0] - m.m[1]) < 1e-5f && std::fabs(r.m[1] + m.m[0]) < 1e-5f && r.m[2] == m.m[2] && r.m[3] == m.m[3]);
    r = m;
    g_atMatrix.PreRotateX(&r, 1.5707963705062866f);
    CHECK(r.m[0] == m.m[0] && std::fabs(r.m[4] + m.m[8]) < 1e-5f && std::fabs(r.m[8] - m.m[4]) < 1e-5f);
    r = m;
    g_atMatrix.PreRotateY(&r, 1.5707963705062866f);
    CHECK(r.m[4] == m.m[4] && std::fabs(r.m[0] - m.m[8]) < 1e-5f && std::fabs(r.m[8] + m.m[0]) < 1e-5f);

    // The quaternion of each kind of rotation, through all four ways.
    const float kQuats[][4] = {{0, 0, 0, 1}, {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0.5f, 0.5f, 0.5f, 0.5f}};
    for (auto& k : kQuats) {
        AtQuaternion q = {k[0], k[1], k[2], k[3]}, back;
        AtMatrixMakeRotation(&r, &q);
        g_atMatrix.GetRotation(&r, &back);
        CHECK(std::fabs(back.x - q.x) < 1e-6f && std::fabs(back.y - q.y) < 1e-6f && std::fabs(back.z - q.z) < 1e-6f && std::fabs(back.w - q.w) < 1e-6f);
    }
}

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// Every .AKC in the oracle's cases, through the recovered device and matrix functions, as a game's load would after opening the file.
int ReadFiles(const std::string& dir) {
    int heap = 0;
    GmNameMap::s_heap = &heap;
    GmNameSpaceDb::s_heap = &heap;
    AtCoordSysDatabase::s_heap = &heap;
    AtIoStream::s_heap = &heap;
    AtIoTokenFile::s_heap = &heap;
    AtMatrixDeviceXbox device;
    FILE* index = std::fopen((dir + "/index.txt").c_str(), "r");
    if (!index) { std::printf("cannot read %s/index.txt\n", dir.c_str()); return 1; }
    unsigned files = 0, entries = 0, matrices = 0, empty = 0, nonFinite = 0, notIdentity = 0;
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AKC\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = std::fopen((dir + "/" + line).c_str(), "rb");
        if (!f) { std::printf("cannot read %s\n", line); return 1; }
        std::vector<char> text;
        char buffer[4096];
        for (size_t n; (n = std::fread(buffer, 1, sizeof buffer, f)) > 0;) text.insert(text.end(), buffer, buffer + n);
        std::fclose(f);
        Memory stream;
        stream.m_base = text.data();
        stream.m_cur = text.data();
        stream.m_limit = text.data() + text.size();
        stream.m_size = (unsigned int)text.size();
        AtIoTextTokenizer tokenizer;
        tokenizer.SetStream(&stream);
        {
            AtCoordSysDatabase db;
            db.Read(&device, &tokenizer);
            AtMatrixPoolXbox* pool = (AtMatrixPoolXbox*)db.m_pool;
            const unsigned n = db.m_rotTransCount + db.m_scaledCount;
            entries += n;
            matrices += pool ? pool->Count() : 0;
            if (n == 0) empty++;
            AtCoordSys* all[2] = {db.m_rotTrans, db.m_scaled};
            const unsigned count[2] = {db.m_rotTransCount, db.m_scaledCount};
            const size_t size[2] = {sizeof(AtRotTransCoordSys), sizeof(AtScaleCoordSys)};
            for (int kind = 0; kind < 2; kind++) {
                for (unsigned i = 0; i < count[kind]; i++) {
                    AtCoordSys* cs = (AtCoordSys*)((char*)all[kind] + i * size[kind]);
                    cs->UpdateWorld();
                    cs->UpdateInverse();
                    bool finite = true, ident = true;
                    for (int e = 0; e < 16; e++) {
                        const float v = cs->m_world->m[e];
                        if (!(v - v == 0.0f)) finite = false;
                        if (v != ((e % 5) == 0 ? 1.0f : 0.0f)) ident = false;
                    }
                    if (!finite) nonFinite++;
                    if (!ident) notIdentity++;
                }
            }
            tokenizer.SetStream(nullptr);
            stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        }
        files++;
    }
    std::fclose(index);
    std::printf(".AKC files read with the recovered device: %u files, %u coordinate systems, %u matrices from the pool, %u files with none; world matrices not finite %u, not the identity %u\n", files,
                entries, matrices, empty, nonFinite, notIdentity);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    TestDevice();
    TestArithmetic();
    if (argc > 1 && ReadFiles(argv[1])) g_fail++;
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("all passed\n");
    return 0;
}
