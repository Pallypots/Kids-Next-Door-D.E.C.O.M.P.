// Host test of the texture database through the real text tokenizer, on any pointer size. Not evidence of a byte match. The file code is left to test_gm_levels and the jam tests: only Read and the
// methods around it run here.
#include <cstdio>
#include <cstring>

#include "at/AtIoTokenizer.h"
#include "at/AtTextureDatabase.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class Item : public AtTextureItem {
public:
    Item() {
        m_flags = 0;
        m_levels = 0;
    }
    void Slot0() override {}
    void Slot1() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    // Only what these tests need of the base's reader: its braces, and the words the database cares about.
    void Read(AtTextureDatabase*, const char*, AtIoTokenizer* tokenizer) override {
        m_read++;
        m_flags = 0;
        m_levels = 1;
        tokenizer->ExpectLeftCurly();
        for (int token = tokenizer->Next(); token != AtIoTokenizer::kRightCurly && token != AtIoTokenizer::kEnd; token = tokenizer->Next()) {
            if (token == 0x55) m_flags |= kDemandLoad;
            if (token == 0x42) m_levels = (unsigned char)tokenizer->ExpectInt();
        }
    }
    void OnLoaded(AtMaterialOwner*) override { m_loaded++; }
    int m_read = 0;
    int m_loaded = 0;
};

class Database : public AtTextureDatabase {
public:
    void OnReady(AtMaterialOwner*) override { m_ready++; }
    void MakeSurfaces2D() override { m_made2D++; }
    void MakeSurfaces3D() override { m_made3D++; }
    void MakeTextures2D() override {}
    void MakeTextures3D() override {}
    void MakeCubeMaps() override {}
    void MakeMore() override { m_more = m_nextMore; }
    void* Surface2DAt(unsigned int) override { return nullptr; }
    void* Surface3DAt(unsigned int) override { return nullptr; }
    AtTextureItem* Texture2DAt(unsigned int index) override { return &m_2D[index]; }
    AtTextureItem* Texture3DAt(unsigned int index) override { return &m_3D[index]; }
    AtTextureItem* CubeMapAt(unsigned int index) override { return &m_cube[index]; }
    Item m_2D[4];
    Item m_3D[4];
    Item m_cube[4];
    AtTextureDatabase* m_nextMore = nullptr;
    int m_ready = 0;
    int m_made2D = 0;
    int m_made3D = 0;
};

int g_heap;
int g_known;

void read(Database& db, AtMaterialOwner& owner, const char* source) {
    static char text[1024];
    const unsigned int size = (unsigned int)std::strlen(source);
    std::memcpy(text, source, size);
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
    AtIoTokenFile::s_heap = &g_heap;
    AtTextureDatabase::SetHeap(&g_heap);
    CHECK(AtTextureDatabase::s_heap == &g_heap);

    AtMaterialOwner owner = {};
    owner.m_textureSpaces.Init(4);
    GmNameMap known;
    known.Init(3);
    known.Insert("Known", &g_known);
    owner.m_textures.Add(&known);

    {
        Database db;
        read(db, owner,
             "Surface2D[3]\nSurface3D[1]\nTexture[5]\n"
             "Texture2D[3]\n{\n  Texture2D \"First\"\n  {\n    PlatformSource\n    MipMap 3\n  }\n  Texture2D \"Known\"\n  {\n  }\n  Texture2D \"Later\"\n  {\n    DemandLoad\n  }\n}\n"
             "Texture3D[1]\n{\n  Texture3D \"Volume\"\n  {\n  }\n}\n"
             "CubeMapTexture[1]\n{\n  CubeMapTexture \"Cube\"\n  {\n    DDSSource\n  }\n}\n");
        CHECK(db.m_ready == 1);
        CHECK(db.m_surface2DCount == 3 && db.m_made2D == 1);
        CHECK(db.m_surface3DCount == 1 && db.m_made3D == 1);
        CHECK(db.m_texture2DCount == 3 && db.m_texture3DCount == 1 && db.m_cubeMapCount == 1);
        CHECK(db.m_2D[0].m_read == 1 && db.m_2D[1].m_read == 1 && db.m_2D[2].m_read == 1);
        CHECK(db.m_2D[0].m_loaded == 1);
        CHECK(db.m_2D[1].m_loaded == 0);  // The owner already knows this name.
        CHECK(db.m_2D[2].m_loaded == 0);  // On demand.
        CHECK(db.m_3D[0].m_loaded == 1 && db.m_cube[0].m_loaded == 1);
        CHECK(AtTextureDatabase::s_demandLoadCount == 1);
        CHECK(db.Find("First") == &db.m_2D[0]);
        CHECK(db.Find("Known") == nullptr);
        CHECK(db.Find("Later") == &db.m_2D[2]);
        CHECK(db.Find("Volume") == &db.m_3D[0]);
        CHECK(db.Find("Cube") == &db.m_cube[0]);
        CHECK(owner.m_textures.Find("Cube") == &db.m_cube[0]);
        CHECK(owner.m_textures.Find("Known") == &g_known);
        CHECK(db.CountUsedTextures() == 3);
        CHECK(db.m_2D[0].m_levels == 3);

        // Reading again clears first, because surfaces are counted.
        read(db, owner, "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"Second\"\n {\n }\n}\n");
        CHECK(db.m_surface2DCount == 1 && db.m_surface3DCount == 0 && db.m_texture3DCount == 0);
        CHECK(db.Find("First") == nullptr);
        CHECK(db.Find("Second") == &db.m_2D[0]);
        CHECK(AtTextureDatabase::s_demandLoadCount == 0);

        db.Clear();
        CHECK(db.m_surface2DCount == 0 && db.m_texture2DCount == 0 && db.m_entries == nullptr);
    }
    CHECK(owner.m_textures.Find("Second") == nullptr);  // Destroying it took it out of the group.

    {
        // In a name space, and a name given twice.
        Database db;
        read(db, owner, "NameSpace \"Level\"\nSurface2D[2]\nTexture[2]\nTexture2D[2]\n{\n Texture2D \"Twice\"\n {\n }\n Texture2D \"Twice\"\n {\n }\n}\n");
        CHECK(owner.m_textureSpaces.Find("Level", "Twice") == &db.m_2D[0]);
        CHECK(owner.m_textures.Find("Twice") == nullptr);
        CHECK(db.m_2D[0].m_loaded == 1 && db.m_2D[1].m_loaded == 0);
    }

    {
        // Allocate, and the chain.
        Database a, b;
        a.Allocate(&owner, 1, 0, 2, 0, 0, nullptr);
        CHECK(a.m_ready == 1 && a.m_made2D == 1 && a.m_made3D == 0 && a.m_texture2DCount == 2);
        CHECK(a.m_group == &owner.m_textures);
        b.m_texture2DCount = 2;
        b.m_2D[0].m_levels = 1;
        b.m_2D[1].m_levels = 0;
        a.m_nextMore = &b;
        for (Item& item : a.m_2D) item.m_levels = 2;
        CHECK(a.CountUsedTextures() == 2);
        CHECK(a.FindFreeTexture() == &b.m_2D[1]);
        CHECK(a.m_more == &b);
        CHECK(a.CountUsedTextures() == 3);
    }

    {
        // Nothing to push until there is both a file system and a path.
        Database db;
        db.PushDirectory();
        db.PopDirectory();
        CHECK(db.m_fileSystem == nullptr);
    }

    if (g_fail == 0) std::printf("ok\n");
    return g_fail != 0;
}
