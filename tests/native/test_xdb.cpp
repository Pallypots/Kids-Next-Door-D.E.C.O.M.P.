// Host test of the Xbox texture and material databases, on any pointer size, through written .AGT and .AGM files and the real text tokenizer, and through an .AGD and the concrete loader. Not evidence of a byte
// match. Supplies the functions of src/at/AtGrD3DTextureSeam.cpp: textures load through the real slot 12 from written .DDS files (tests/native/texload_device.h); the rest of the device is counted here.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "at/AtAudioBank.h"
#include "at/AtGrD3DDatabase.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static std::map<unsigned int, int> g_unrecovered;
static int g_loaded_materials, g_stage_releases, g_before, g_releases;
static int g_handle;

unsigned int AtGrD3DUnrecovered(unsigned int va, void*, unsigned int, unsigned int) { g_unrecovered[va]++; return 0; }
void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int) { return std::malloc(size); }
void AtGrD3DFreeContiguous(void* data) { std::free(data); }
void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char**, char**, char**) {}
void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char**, char**) {}
void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char**, char**) {}
void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char**, char**, char**, char*) {}

// A texture loads through the real slot 12 (src/at/AtGrD3DTextureLoad.cpp), from .DDS files on the tree: tests/native/texload_device.h is the device's part, and counts what it makes.
#include "texload_device.h"
// As 0x0010B7F0 might leave a material: two stage states, one holding the shared tables and entries.
void AtGrD3DLoadMaterial(AtGrD3DMaterial* material, AtMaterialOwner*) {
    g_loaded_materials++;
    material->m_stageStateCount = 2;
    material->m_stageStates[0].m_entries = new AtGrD3DStageEntry[3];
    material->m_stageStates[0].m_unknown04 = operator new(16);
    material->m_stageStates[1].m_flags = AtGrD3DMaterialStage::kShared;
    if (AtGrD3DMaterialStage::s_sharedUsers++ == 0) AtGrD3DMaterialStage::s_shared84 = operator new(8);
}
void AtGrD3DReleaseStage(void*, unsigned int) { g_stage_releases++; }
void AtGrD3DBeforeRelease(void*) { g_before++; }
void AtGrD3DRelease(void*) { g_releases++; }

namespace {

class File : public AtIoFile {
public:
    const std::string* m_text = nullptr;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        const size_t have = m_text->size() - m_position;
        if (n > have) n = (unsigned)have;
        if (n) std::memcpy(out, m_text->data() + m_position, n);
        m_position += n;
        if (done) *done = n;
        return n ? 0 : (int)kEndOfFile;
    }
    int Write(const void*, unsigned int, unsigned int*) override { return kUnsupported; }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class Tree : public AtIoFileSystem {
public:
    std::map<std::string, std::string> m_files;
    File m_file;
    const std::string* Find(const char* path) {
        m_path.Resolve(path);
        auto f = m_files.find(m_path.m_full);
        return f == m_files.end() ? nullptr : &f->second;
    }
    int Open(const char* path, int flags, AtIoFile** out) override {
        const std::string* text = Find(path);
        if (!text) return AtIoFile::kNotFound;
        m_file.m_text = text;
        m_file.m_position = 0;
        m_file.m_size = (unsigned)text->size();
        m_file.m_flags = (flags & 0x21) ? 0x0C : 2;
        std::strcpy(m_file.m_path, m_path.m_full);
        *out = &m_file;
        return 0;
    }
    int Close(AtIoFile*) override { return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char* path) override { return Find(path) ? 0 : 1; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

struct Owner : public AtGrD3DOwner {
    TexloadChooser m_chooserObject;
    AtGfxDrawLists* DrawLists() override { return &m_chooserObject; }
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
};

}  // namespace

int main() {
    Tree tree;
    tree.m_path.m_top = -1;
    AtIoContext context;
    AtIoFileSystem* stack[4] = {&tree};
    context.m_stackCount = 1;
    context.m_stackSize = 4;
    context.m_stack = stack;
    AtIoFileRef::s_context = &context;
    AtAudioBankFile::s_context = &context;  // loader's AtIoBinaryFile, folded into afd's AtAudioBankFile in batch 5

    static int token;
    AtIoTokenFile::s_heap = &token;
    AtDatabaseLoader::s_heap = &token;
    AtXboxFactory::s_loaderHeap = &token;
    GmNameMap::s_heap = &token;
    GmNameSpaceDb::s_heap = &token;
    AtIoStream::s_heap = &token;
    AtTextureDatabase::s_heap = &token;
    AtMaterialDatabase::s_heap = &token;
    AtGrD3DMaterialDatabase::SetHeap(&token);
    AtGrIPixelMap::SetHeap(&token);

    Owner* owner = new Owner;
    owner->m_textureSpaces.Init(8);
    owner->m_materialSpaces.Init(8);
    owner->m_shaderSpaces.Init(8);
    owner->m_textureDatabases = nullptr;
    owner->m_materialDatabases = nullptr;
    TexloadLimits limits;
    owner->m_textureLimits = &limits;
    owner->m_noTextures = 0;
    tree.m_files["MODEL\\ROCK.Dds"] = TexloadDds(32, 32, 3);
    tree.m_files["MODEL\\SKY.Dds"] = TexloadDds(16, 16, 0);

    // Textures: three levels, one, and a back buffer source, which takes one surface whatever its levels.
    tree.m_files["MODEL\\T.AGT"] =
        "Surface2D[5]\nTexture[3]\nTexture2D[3]\n{\n"
        " Texture2D \"ROCK\" { DDSSource MipMap 3 }\n"
        " Texture2D \"SKY\" { PlatformSource }\n"
        " Texture2D \"BACK\" { BackBufferSource MipMap 2 }\n}\n";
    AtGrD3DTextureDatabase* textures = new AtGrD3DTextureDatabase;
    textures->Load(owner, "MODEL\\T", 2);
    CHECK(textures->m_surface2DCount == 5 && textures->m_texture2DCount == 3);
    CHECK(textures->m_owner == owner && owner->m_textureDatabases == textures);
    AtGrD3DTexture2D* rock = static_cast<AtGrD3DTexture2D*>(textures->Texture2DAt(0));
    AtGrD3DTexture2D* sky = static_cast<AtGrD3DTexture2D*>(textures->Texture2DAt(1));
    AtGrD3DTexture2D* back = static_cast<AtGrD3DTexture2D*>(textures->Texture2DAt(2));
    CHECK(rock->m_surfaces == &textures->m_surfaces2D[0] && sky->m_surfaces == &textures->m_surfaces2D[3] && back->m_surfaces == &textures->m_surfaces2D[4]);
    CHECK(textures->m_surfaces2DTaken == 5);
    CHECK(textures->Find("SKY") == sky && rock->m_database == textures);
    CHECK(g_textures_made == 3 && rock->m_owner == owner && rock->SurfaceAt(2) == &textures->m_surfaces2D[2]);
    CHECK(textures->Surface2DAt(1) == &textures->m_surfaces2D[1]);
    CHECK(textures->m_surfaces2D[0].Palette() == nullptr);
    textures->m_surfaces2D[0].m_format.m_flags = AtPixelFormat::kIndex;
    CHECK(rock->PaletteOf() == &textures->m_surfaces2D[0].m_palette);
    unsigned char key[4] = {1, 2, 3, 4};
    textures->m_surfaces2D[1].SetColorKey(key);
    CHECK(textures->m_surfaces2D[1].m_colorKey[3] == 4 && (textures->m_surfaces2D[1].m_flags & AtGrIPixelSurface::kColorKey));
    textures->m_surfaces2D[1].ClearColorKey();
    CHECK(textures->m_surfaces2D[1].m_flags == AtGrD3DSurface2D::kLoaded);  // ROCK's second level, loaded
    // A chained surface, owned by the first, freed with it.
    textures->m_surfaces2D[0].m_next = new AtGrD3DSurface2D;
    textures->m_surfaces2D[0].m_next->m_surface = &g_handle;
    textures->m_surfaces2D[0].m_palette.m_entries = (unsigned int*)operator new(16);

    // More of the same, on demand.
    textures->MakeMore();
    AtGrD3DTextureDatabase* more = static_cast<AtGrD3DTextureDatabase*>(textures->m_more);
    CHECK(more != nullptr && more->m_texture2DCount == 3 && more->m_surface2DCount == 5 && more->m_owner == owner);
    CHECK(owner->m_textureDatabases == more && more->m_next == textures);
    delete more;
    CHECK(owner->m_textureDatabases == textures);

    // Clearing: each texture lets the four stages go, its levels' surfaces and its texture; then the surfaces themselves.
    g_releases = 0;  // loading BACK dropped the back buffer's reference once
    textures->Clear();
    CHECK(g_stage_releases == 12 && g_before == 3);
    CHECK(g_releases == 3 + 3 + 1 + 1);  // three textures; ROCK's three levels and the surface chained to its first, SKY's one; BACK's surface was never given one. Destroying the surfaces finds nothing left
    CHECK(owner->m_textureDatabases == nullptr && textures->m_owner == nullptr && textures->m_surfaces2D == nullptr);
    delete textures;

    // Materials.
    tree.m_files["MODEL\\M.AGM"] = "Material[2]\n{\n Material \"STONE\" { DiffuseColor 1 2 3 4 }\n Material \"WOOD\" { }\n}\n";
    AtGrD3DMaterialDatabase* materials = new AtGrD3DMaterialDatabase;
    materials->Load(owner, "MODEL\\M", 2);
    CHECK(materials->m_count == 2 && materials->m_owner == owner && owner->m_materialDatabases == materials);
    CHECK(g_loaded_materials == 2);
    AtGrD3DMaterial* stone = static_cast<AtGrD3DMaterial*>(materials->MaterialAt(0));
    CHECK(stone == &materials->m_materials[0] && materials->Find("WOOD") == &materials->m_materials[1]);
    CHECK(stone->m_diffuse.r == 1 && stone->m_diffuse.a == 4 && stone->m_stageStateCount == 2);
    CHECK(AtGrD3DMaterialStage::s_sharedUsers == 2 && AtGrD3DMaterialStage::s_shared84 != nullptr);
    materials->Slot3();
    AtGrD3DMaterialDatabase* moreMaterials = static_cast<AtGrD3DMaterialDatabase*>(materials->m_more);
    CHECK(moreMaterials != nullptr && moreMaterials->m_count == 2 && owner->m_materialDatabases == moreMaterials);
    CHECK(materials->MaterialAt(3) == &moreMaterials->m_materials[1]);
    delete moreMaterials;
    materials->Clear();
    CHECK(AtGrD3DMaterialStage::s_sharedUsers == 0 && AtGrD3DMaterialStage::s_shared84 == nullptr);
    CHECK(owner->m_materialDatabases == nullptr && materials->m_materials == nullptr && materials->m_count == 0);
    delete materials;

    // From an .AGD, through the concrete loader.
    tree.m_files["MODEL\\THING.AGD"] = "TextureDatabase[1]\n{\n \"T\"\n}\nMaterialDatabase[1]\n{\n \"M\"\n}\n";
    g_textures_made = g_loaded_materials = 0;
    AtXboxFactory factory;
    AtGrD3DDatabaseLoader* loader = factory.NewDatabaseLoader();
    loader->Load(owner, "MODEL\\THING", 2, 0, 1);
    CHECK(loader->m_textures != nullptr && loader->m_textures[0].m_texture2DCount == 3 && g_textures_made == 3);
    CHECK(loader->m_materials != nullptr && loader->m_materials[0].m_count == 2 && g_loaded_materials == 2);
    CHECK(owner->m_textureDatabases == loader->m_textures && owner->m_materialDatabases == loader->m_materials);
    loader->Unload();
    CHECK(owner->m_textureDatabases == nullptr && owner->m_materialDatabases == nullptr);
    delete loader;

    // A 3D list stops at its constructor.
    tree.m_files["MODEL\\V.AGT"] = "Texture[1]\nTexture3D[1]\n{\n Texture3D \"V\" { }\n}\n";
    g_unrecovered.clear();
    AtGrD3DTextureDatabase* volume = new AtGrD3DTextureDatabase;
    volume->Load(owner, "MODEL\\V", 2);
    CHECK(g_unrecovered[0x001214A0] == 1);
    delete volume;

    delete owner;
    if (g_fail == 0) std::printf("test_xdb: all passed\n");
    return g_fail != 0;
}
