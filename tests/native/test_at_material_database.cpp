// Host test of the material database through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstring>

#include "at/AtIoTokenizer.h"
#include "at/AtMaterialDatabase.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class Concrete : public AtMaterial {
public:
    void OnLoaded(AtMaterialOwner*) override { m_loaded++; }
    int m_loaded = 0;
};

class Database : public AtMaterialDatabase {
public:
    void OnReady(AtMaterialOwner*) override { m_ready++; }
    void Slot3() override {}
    AtMaterial* MaterialAt(unsigned int index) override { return &m_items[index]; }
    Concrete m_items[4];
    int m_ready = 0;
};

int g_heap;
int g_a, g_b, g_c;

void read(Database& db, AtMaterialOwner& owner, char* text, unsigned int size) {
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + size;
    stream.m_size = size;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);
    db.Read(&owner, &tokenizer);
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    tokenizer.SetStream(nullptr);
}

}  // namespace

int main() {
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;
    AtMaterialDatabase::SetHeap(&g_heap);

    AtMaterialOwner owner = {};
    owner.m_materialSpaces.Init(4);
    GmNameMap textures;
    textures.Init(5);
    textures.Insert("wood", &g_a);
    textures.Insert("stone", &g_b);
    owner.m_textures.Add(&textures);
    GmNameMap shaders;
    shaders.Init(3);
    shaders.Insert("Found", &g_c);
    owner.m_shaders.Add(&shaders);

    {
        static char text[] =
            "StagedShaderTexture[2]\n{\n  \"wood\"\n  Default\n}\n"
            "ProgShaderTexture[1]\n{\n  \"stone\"\n}\n"
            "Material[2]\n{\n"
            "  Material \"first\"\n  {\n    SimpleTextureShader 0\n  }\n"
            "  Material \"second\"\n  {\n    Shader \"Found\" 0 1 1 1\n  }\n"
            "}\n";
        Database db;
        read(db, owner, text, sizeof text - 1);
        CHECK(db.m_count == 2);
        CHECK(db.m_ready == 1);
        CHECK(db.m_textureCapacity == 2 && db.m_textures[0] == &g_a && db.m_textures[1] == nullptr);
        CHECK(db.m_stageCapacity == 1 && db.m_stages[0] == &g_b);
        CHECK(db.Find("first") == &db.m_items[0]);
        CHECK(db.Find("second") == &db.m_items[1]);
        CHECK(db.Find("third") == nullptr);
        CHECK(db.m_items[0].m_loaded == 1 && db.m_items[1].m_loaded == 1);
        CHECK(db.m_items[1].m_shader == &g_c);
        CHECK(owner.m_materials.Find("first") == &db.m_items[0]);

        // Only loaded materials are copied, and names are not.
        db.m_items[1].m_flags &= ~(unsigned int)AtMaterial::kLoaded;
        Database copy;
        copy.CopyFrom(&owner, db, nullptr);
        CHECK(copy.m_count == 2 && copy.m_ready == 1);
        CHECK(copy.m_textureCapacity == 2 && copy.m_stageCapacity == 1);
        CHECK(copy.m_items[0].m_flags & AtMaterial::kLoaded);
        CHECK(!(copy.m_items[1].m_flags & AtMaterial::kLoaded));
        CHECK(copy.Find("first") == nullptr);

        // Cleared by a new allocation, and under a name space this time.
        copy.Allocate(&owner, 3, 0, 1, "Space");
        CHECK(copy.m_count == 1 && copy.m_stageCapacity == 3 && copy.m_textureCapacity == 0 && copy.m_textures == nullptr);
        CHECK(copy.m_stages[0] == nullptr && copy.m_stages[2] == nullptr);
        copy.Insert("named", &copy.m_items[0]);
        CHECK(owner.m_materialSpaces.Find("Space", "named") == &copy.m_items[0]);
        copy.Clear();
        CHECK(copy.m_count == 0 && copy.m_stages == nullptr && copy.m_entries == nullptr);
    }
    CHECK(owner.m_materials.m_first == nullptr);

    if (g_fail == 0) std::printf("ok\n");
    return g_fail != 0;
}
