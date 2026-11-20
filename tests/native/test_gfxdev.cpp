// Host test of the graphics device's construction and its member-returning slots, on any pointer size. Not evidence of a byte match.
//
//   test_gfxdev [<text cases directory>]
//
// The texture work's three classes are not recovered here: this test stands in for them (src/at/AtGraphicsDeviceHost.cpp's seams), counting what is made, and for the destructors that file stops in.
// With a directory (the oracle's text cases, with index.txt), it reads every .AKC listed there with the matrix device a real owner hands out: AtMaterialOwner::MatrixDevice() on the concrete device,
// as AtDatabaseLoader::LoadDatabases does.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtCoordSys.h"
#include "at/AtGraphicsDevice.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "gm/GmNameMap.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// ---- stand-ins for src/at/AtGraphicsDeviceHost.cpp ------------------------------------------------------------------------
static std::vector<std::pair<char, const void*>> g_made;
void AtGfxConstructTextureA(void* at) { g_made.push_back({'A', at}); std::memset(at, 0, 0x50); }
void AtGfxDestroyTextureA(void* at) { g_made.push_back({'a', at}); }
void AtGfxConstructTexture2D(void* at) { g_made.push_back({'T', at}); std::memset(at, 0, 0x34); }
void AtGfxDestroyTexture2D(void* at) { g_made.push_back({'t', at}); }
void AtGfxConstructTextureDatabase(void* at) { g_made.push_back({'D', at}); std::memset(at, 0, 0x58); }
void AtGfxDestroyTextureDatabase(void* at) { g_made.push_back({'d', at}); }
AtIndexArrayXbox::~AtIndexArrayXbox() {}
AtGfxBufferSet::~AtGfxBufferSet() {}
AtMeshDatabaseXbox::~AtMeshDatabaseXbox() {}
AtGfxResourceXbox::~AtGfxResourceXbox() {}

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

bool all_zero(const void* p, size_t n) {
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) {
        if (b[i]) return false;
    }
    return true;
}

class Released : public AtGfxObject {
public:
    int released = 0;
    void Slot0() override {}
    void Release() override { released++; }
};

void TestDevice(AtGraphicsDeviceXbox* d) {
    AtMaterialOwner* owner = d;
    // The texture work's members, in the original's order: two of A, then A and 2D alternating as at 0x000F1FD8-0x000F210C, then the database.
    const char* order = "AAATTATATATATATATAT";
    CHECK(g_made.size() == std::strlen(order) + 1);
    for (size_t i = 0; i < g_made.size() && i < std::strlen(order); i++) CHECK(g_made[i].first == order[i]);
    if (g_made.size() == std::strlen(order) + 1) {
        CHECK(g_made.back().first == 'D' && g_made.back().second == &d->m_textureDatabase);
        CHECK(g_made[0].second == &d->m_target0 && g_made[1].second == &d->m_target1 && g_made[2].second == &d->m_textureA2ED0 && g_made[18].second == &d->m_texture32F0);
    }

    // The slots that hand back members, through the base, as the game's code calls them.
    CHECK(owner->MatrixDevice() == &d->m_matrixDevice);
    CHECK(owner->RenderTarget0() == &d->m_target0);
    CHECK(owner->RenderTarget1() == &d->m_target1);
    CHECK(owner->Context() == &d->m_context);
    CHECK(owner->DrawLists() == &d->m_drawLists);
    CHECK(owner->BufferSet() == &d->m_bufferSet);
    CHECK(owner->GraphicsFactory() == nullptr);
    int creator;
    d->m_creator = &creator;
    CHECK((void*)owner->GraphicsFactory() == (void*)&creator);
    CHECK(owner->Width() == 0 && owner->Height() == 0);
    d->m_mode.m_width = 640;
    d->m_mode.m_height = 480;
    CHECK(owner->Width() == 640 && owner->Height() == 480);
    float scale[3] = {7, 7, 7};
    owner->DrawScale(scale);
    CHECK(scale[0] == 1.0f && scale[1] == 1.0f && scale[2] == 7);
    Released r;
    owner->ReleaseObject(&r);
    CHECK(r.released == 1);

    // The bases.
    CHECK(d->m_unknown04 == 0 && d->m_noTextures == 0 && d->m_unknown2C[0] == 0x80 && d->m_unknown2C[3] == 0x80);
    CHECK(d->m_unknown34 == 0 && d->m_unknown39C == 0 && d->m_unknown3A4 == 0);
    CHECK(d->m_textures.m_first == nullptr && d->m_textureSpaces.m_pool == nullptr && d->m_unknown12CSpaces.m_size == 0);
    // Init.
    CHECK(d->m_unknown3E10 == 0x901 && d->m_unknown3E00 == 1 && d->m_unknown30598 == 2 && d->m_unknown3DF8 == 0);
    CHECK(d->m_unknown3FA4 == d->m_table3FA8);
    CHECK(d->m_sortedA.m_less == AtGfxLessAt10 && d->m_sortedB.m_less == AtGfxLessAt10);
    CHECK(all_zero(d->m_unknown614C, sizeof d->m_unknown614C) && all_zero(d->m_table5BE4, sizeof d->m_table5BE4) && all_zero(d->m_unknown30254, sizeof d->m_unknown30254));
    CHECK(d->m_contexts == nullptr && d->m_meshDatabases == nullptr);
    for (const AtMatrixObject& m : d->m_matrices) CHECK(identity(&m));
    // Members.
    CHECK(d->m_context.m_unknown2A60 == 2 && d->m_context.m_unknown2A70 == 1.0f && d->m_context.m_next == nullptr && d->m_context.m_device == nullptr);
    CHECK(d->m_context.m_shared == AtGfxContextBase::s_shared && d->m_context.m_color20[3] == 0xFF && d->m_context.m_unknown24 == 1.0f);
    CHECK(d->m_context.m_part30.m_unknown14 == 1.0f && d->m_context.m_part48.m_unknownD0 == 5 && d->m_context.m_part48.m_unknownDC == -1.0f && d->m_context.m_part48.m_unknownE0 == 1.0f);
    CHECK(all_zero(&d->m_state, sizeof d->m_state));
    CHECK(d->m_drawLists.m_block24 == nullptr && d->m_drawLists.m_unknown28 == 0);
    CHECK(d->m_bufferSet.m_indexArrays[1].m_count == 0 && d->m_bufferSet.m_indexArrays[1].m_name[0] == 0 && d->m_bufferSet.m_indexArrays[0].m_memory == nullptr);
    CHECK(d->m_meshDatabase.m_next == nullptr && d->m_meshDatabase.m_meshCount == 0);
    CHECK(d->m_pool47A8.m_count == 0 && d->m_sortedB.m_pool.m_matrices == nullptr);
    CHECK(d->m_resources[0x7F].m_unknown2E == 1 && d->m_resources[0].m_resource == nullptr);
    CHECK(d->m_lists.m_block1C == nullptr && d->m_pair13180.m_block08 == nullptr);
}

void TestLess() {
    float a[5] = {}, b[5] = {};
    const float nan = std::strtof("nan", nullptr);
    struct { float x, y; int less; } k[] = {{1, 2, 1}, {2, 1, 0}, {1, 1, 0}, {-0.0f, 0.0f, 0}, {nan, 1, 0}, {1, nan, 0}, {-1e30f, 1e30f, 1}};
    for (auto& c : k) {
        a[4] = c.x;
        b[4] = c.y;
        CHECK(AtGfxLessAt10(a, b) == c.less);
    }
}

void TestUnlink(AtGraphicsDeviceXbox* d) {
    static AtGfxContext c[3];
    d->m_contexts = &c[0];
    c[0].m_next = &c[1];
    c[1].m_next = &c[2];
    c[2].m_next = nullptr;
    d->UnlinkContext(&c[1]);
    CHECK(d->m_contexts == &c[0] && c[0].m_next == &c[2]);
    d->UnlinkContext(&c[1]);  // absent
    CHECK(d->m_contexts == &c[0] && c[0].m_next == &c[2]);
    d->UnlinkContext(&c[0]);
    CHECK(d->m_contexts == &c[2]);
    d->UnlinkContext(&c[2]);
    CHECK(d->m_contexts == nullptr);
    d->UnlinkContext(&c[2]);
    CHECK(d->m_contexts == nullptr);
    // A context's destructor leaves its device's list when +0x10 says it is in one.
    {
        AtGfxContext* e = new AtGfxContext;
        e->m_device = d;
        e->m_unknown10 = 1;
        d->m_contexts = e;
        delete e;
        CHECK(d->m_contexts == nullptr);
    }
}

void TestFreeing() {
    {
        AtGfxDrawLists lists;
        lists.m_block0C = operator new(4);
        lists.m_block24 = operator new(4);
        lists.m_unknown04 = 3;
        lists.Clear();
        CHECK(lists.m_block0C == nullptr && lists.m_block24 == nullptr && lists.m_unknown04 == 0);
    }
    {
        AtGfxIndexedList list;
        list.m_block00 = operator new(4);
        list.m_block14 = operator new(4);
        list.m_unknown20 = 5;
        list.Clear();
        CHECK(list.m_block00 == nullptr && list.m_block14 == nullptr && list.m_unknown20 == 0);
    }
    {
        AtGfxStateCache* cache = new AtGfxStateCache;
        cache->m_blockA58 = operator new(4);
        cache->m_cachedA04 = 1;
        delete cache;
    }
    {
        // An owner of its own: Reset clears its tables' name space pools, and its destructor.
        int heap;
        GmNameSpaceDb::s_heap = &heap;
        GmNameMap::s_heap = &heap;
        AtMaterialOwner* owner = new AtMaterialOwner;
        owner->m_textureSpaces.Init(3);
        owner->m_unknown12CSpaces.Init(2);
        owner->m_unknown34 = 9;
        owner->m_noTextures = -1;
        owner->m_mode.m_width = 5;
        owner->Slot1();
        CHECK(owner->m_textureSpaces.m_pool == nullptr && owner->m_unknown12CSpaces.m_pool == nullptr && owner->m_unknown34 == 0 && owner->m_noTextures == 0 && owner->m_mode.m_width == 0);
        owner->m_shaderSpaces.Init(2);
        owner->Slot0();  // the deleting destructor
    }
}

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// Every .AKC in the oracle's cases, with the matrix device a real owner hands out.
int ReadFiles(const std::string& dir, AtMaterialOwner* owner) {
    int heap = 0;
    GmNameMap::s_heap = &heap;
    GmNameSpaceDb::s_heap = &heap;
    AtCoordSysDatabase::s_heap = &heap;
    AtIoStream::s_heap = &heap;
    AtIoTokenFile::s_heap = &heap;
    AtMatrixDevice* device = owner->MatrixDevice();
    FILE* index = std::fopen((dir + "/index.txt").c_str(), "r");
    if (!index) { std::printf("cannot read %s/index.txt\n", dir.c_str()); return 1; }
    unsigned files = 0, entries = 0, matrices = 0, nonFinite = 0;
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
            db.Read(device, &tokenizer);
            AtMatrixPoolXbox* pool = (AtMatrixPoolXbox*)db.m_pool;
            entries += db.m_rotTransCount + db.m_scaledCount;
            matrices += pool ? pool->Count() : 0;
            AtCoordSys* all[2] = {db.m_rotTrans, db.m_scaled};
            const unsigned count[2] = {db.m_rotTransCount, db.m_scaledCount};
            const size_t size[2] = {sizeof(AtRotTransCoordSys), sizeof(AtScaleCoordSys)};
            for (int kind = 0; kind < 2; kind++) {
                for (unsigned i = 0; i < count[kind]; i++) {
                    AtCoordSys* cs = (AtCoordSys*)((char*)all[kind] + i * size[kind]);
                    cs->UpdateWorld();
                    cs->UpdateInverse();
                    for (int e = 0; e < 16; e++) {
                        const float v = cs->m_world->m[e];
                        if (!(v - v == 0.0f)) { nonFinite++; break; }
                    }
                }
            }
            tokenizer.SetStream(nullptr);
            stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        }
        files++;
    }
    std::fclose(index);
    std::printf(".AKC files read with the matrix device of the concrete graphics device, through AtMaterialOwner::MatrixDevice(): %u files, %u coordinate systems, %u matrices; world matrices not finite %u\n",
                files, entries, matrices, nonFinite);
    return files == 0;
}

}  // namespace

int main(int argc, char** argv) {
    AtMatrixDeviceXbox::SetHeap(&g_deviceHeap);
    AtMatrixPoolXbox::SetHeap(g_poolHeap);
    AtGraphicsDeviceXbox* d = new AtGraphicsDeviceXbox;  // Never destroyed: its destructor's first step, Release, is not recovered.
    TestDevice(d);
    TestLess();
    TestUnlink(d);
    TestFreeing();
    if (argc > 1 && ReadFiles(argv[1], d)) g_fail++;
    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("all passed\n");
    return 0;
}
