// Host test of the .AGD helpers and readers through the real text tokenizer, on any pointer size. Not evidence of a byte match. Load is left out, for want of a file system here.
#include <cstdio>
#include <cstring>
#include <vector>

#include "at/AtDatabaseLoader.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

struct Text {
    Memory stream;
    AtIoTextTokenizer tokenizer;
    std::vector<char> bytes;
    explicit Text(const char* source) : bytes(source, source + std::strlen(source)) {
        stream.m_base = bytes.data();
        stream.m_cur = bytes.data();
        stream.m_limit = bytes.data() + bytes.size();
        stream.m_size = (unsigned int)bytes.size();
        tokenizer.SetStream(&stream);
    }
    ~Text() {
        stream.m_base = nullptr;
        stream.m_cur = nullptr;
        stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

class Keys : public AtAnimationKeys {
public:
    int m_rotations = 0;
    void Allocate(unsigned int) override {}
    void Free() override {}
    void ReadEntries(AtIoTokenizer*) override {}
    void Interpolate(unsigned int, unsigned int, float, int, int*, int*, float*) override {}
    int Find(unsigned int, unsigned int, float, int) override { return 0; }
    void Load() override {}
    unsigned short At(unsigned int) override { return 0; }
    void Slot9(unsigned int) override {} void Slot10(unsigned int) override {} void Slot11(unsigned int) override {} void Slot12(unsigned int) override {} void Slot13(unsigned int) override {}
    void Slot14() override {} void Slot15() override {} void Slot16() override {} void Slot17() override {} void Slot18() override {}
    void ReadRotationKeys(AtIoTokenizer* t) override { m_rotations += t->ExpectInt(); }
    void ReadTranslationKeys(AtIoTokenizer* t) override { t->ExpectInt(); }
    void ReadScaleKeys(AtIoTokenizer* t) override { t->ExpectInt(); }
    void ReadNoteKeys(AtIoTokenizer* t) override { t->ExpectInt(); }
    void ReadCameraKeys(AtIoTokenizer* t) override { t->ExpectInt(); }
};

class Device : public AtAnimationDevice {
public:
    Keys m_keys;
    AtAnimationKeys* NewKeys() override { return &m_keys; }
    void FreeKeys(AtAnimationKeys*) override {}
    AtAnimationDatabase* NewDatabase() override { return nullptr; }
    void FreeDatabase(AtAnimationDatabase*) override {}
};

class AnimDb : public AtAnimationDatabase {
public:
    int HasBinaryForm() override { return 0; }
    void LoadBinaryForm(AtAnimationDevice*, const char*) override {}
};

class Indices : public AtIndexArray {
public:
    std::vector<unsigned char> m_memory = std::vector<unsigned char>(64);
    unsigned int m_given = 0;
    Indices() { m_count = 0; m_flags = 0; m_loadFlags = 0; m_index = 0; }
    void Allocate(unsigned int, unsigned int flags) override { m_given = flags; }
    void Slot1() override {} void Slot2() override {}
    void Lock(void** out, int) override { *out = m_memory.data(); }
    void Unlock() override {}
    void Finish() override {}
};

class Process : public AtProcessItem {
public:
    int m_loaded = 0;
    void OnLoaded(AtMaterialOwner*) override { m_loaded++; }
    void Slot1() override {}
    void ReadPixel(AtMaterialOwner*, AtIoTokenizer* t) override { t->ExpectLeftCurly(); t->ExpectRightCurly(); }
    void Slot3() override {} void Slot4() override {}
    void ReadVertex(AtMaterialOwner*, AtIoTokenizer* t) override { t->ExpectLeftCurly(); t->ExpectRightCurly(); }
};

class VertexDb : public AtVertexProcessDatabase {
public:
    Process m_items[4];
    void Make(AtMaterialOwner*) override {}
    AtProcessItem* ItemAt(unsigned int i) override { return &m_items[i]; }
};

}  // namespace

int main() {
    char leaf[9];
    std::memset(leaf, 0, sizeof leaf);
    AtLeafName("A\\B\\NAME.AGD", leaf);
    CHECK(std::strcmp(leaf, "NAME") == 0);
    std::memset(leaf, 0, sizeof leaf);
    AtLeafName("A/LONGERTHANEIGHT", leaf);
    CHECK(std::strcmp(leaf, "LONGERTH") == 0);

    char out[32];
    char name1[] = "D\\A";
    AtJoinPrefix(out, name1, "P");
    CHECK(std::strcmp(out, "D\\P\\A") == 0);
    CHECK(std::strcmp(name1, "D\\A") == 0);
    char name2[] = "A";
    AtJoinPrefix(out, name2, "P");
    CHECK(std::strcmp(out, "P\\A") == 0);

    int heap = 0;
    AtAnimationDatabase::SetHeap(&heap);
    AtIoTokenFile::s_heap = &heap;
    GmNameMap::s_heap = &heap;
    {
        Device device;
        AnimDb db;
        Text t("KeyframeSet { RotationKey 7 } RotAnimComponent[1] { 1 2 3 4 } Animation[1] { Animation \"A\" { Rate 2.5 Length 9 BoundingSphere 1.0 2.0 3.0 4.0 } }");
        db.Read(&device, &t.tokenizer);
        CHECK(device.m_keys.m_rotations == 7);
        CHECK(db.m_rotationCount == 1);
        CHECK(db.m_rotation[0].m_unknown00 == 1 && db.m_rotation[0].m_unknown0A == 4);
        CHECK(db.m_animationCount == 1);
        CHECK(db.m_animations[0].m_rate == 2.5f);
        CHECK(db.m_animations[0].m_length == 9 && db.m_animations[0].m_loopPeriod == 10);
        CHECK(db.m_animations[0].m_flags == AtAnimation::kHasBoundingSphere);
        CHECK(db.m_animations[0].m_boundingSphere.radius == 4.0f);
        CHECK(db.Find("A") == &db.m_animations[0]);
    }
    {
        Indices list;
        Text t("{ Index8Bit Index[3] { 1 2 257 } } }");
        t.tokenizer.SetKeywords(AtMeshDatabase::s_keywords, AtMeshDatabase::kKeywordCount, nullptr);
        list.Read(&t.tokenizer, 5);
        CHECK(list.m_index == 5 && list.m_count == 3);
        CHECK(list.m_given == AtIndexArray::kIndex8Bit);
        CHECK(list.m_memory[0] == 1 && list.m_memory[2] == 1);
    }
    {
        AtMathTriangle triangle = {};
        float a[3] = {0, 0, 0}, b[3] = {1, 0, 0}, c[3] = {0, 1, 0};
        triangle.m_vertices[0] = b;
        triangle.m_vertices[1] = a;
        triangle.m_vertices[2] = c;
        AtMathTriangleSetAxis(&triangle);
        CHECK(triangle.m_axis == 0);
        triangle.m_vertices[2] = b;
        AtMathTriangleSetAxis(&triangle);
        CHECK(triangle.m_axis == 3);
    }
    {
        AtMaterialOwner owner = {};
        VertexDb db;
        Text t("VertexProcess[2] { VertexProcess \"A\" { } VertexProcess \"B\" { } }");
        db.Read(&owner, &t.tokenizer);
        CHECK(db.m_count == 2 && db.Find("B") == &db.m_items[1]);
        CHECK(db.m_items[0].m_loaded == 1);
        CHECK(owner.m_vertexProcesses.m_first == &db);
    }
    std::printf(g_fail ? "FAILED\n" : "ok\n");
    return g_fail ? 1 : 0;
}
