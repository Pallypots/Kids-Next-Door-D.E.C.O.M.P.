// Host test of the Xbox shader, light, font and image databases, on any pointer size, through written .AGS, .AGL, .AGF and .AGI files and the real text tokenizer, and through an .AGD and the concrete loader.
// Not evidence of a byte match. Supplies the functions of src/at/AtGrD3DItemSeam.cpp and src/at/AtGrD3DTextureSeam.cpp: what the device would do is counted here, and loading marks the items so that clearing
// has something to release.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "at/AtGrD3DDatabase.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static std::map<unsigned int, int> g_unrecovered;
static int g_loaded_fonts, g_loaded_images, g_releases;
static int g_handle;

unsigned int AtGrD3DUnrecovered(unsigned int va, void*, unsigned int, unsigned int) { g_unrecovered[va]++; return 0; }
void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int) { return std::malloc(size); }
void AtGrD3DFreeContiguous(void* data) { std::free(data); }
void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char**, char**, char**) {}
void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char**, char**) {}
void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char**, char**) {}
void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char**, char**, char**, char*) {}
#include "texload_device.h"  // a texture loads through the real slot 12, from a written .DDS file
void AtGrD3DLoadMaterial(AtGrD3DMaterial*, AtMaterialOwner*) {}
void AtGrD3DReleaseStage(void*, unsigned int) {}
void AtGrD3DBeforeRelease(void*) {}
void AtGrD3DRelease(void*) { g_releases++; }

// As 0x00123850 and 0x00124CD0 might leave an item: a surface of its own, and its own texture database on the owner's list.
void AtGrD3DLoadFont2D(AtGrD3DFont2D* font, AtMaterialOwner* owner) {
    g_loaded_fonts++;
    font->m_surface.m_surface = &g_handle;
    font->m_textures.OnReady(owner);
}
void AtGrD3DLoadImage(AtGrD3DImage* image, AtMaterialOwner* owner) {
    g_loaded_images++;
    image->m_surface.m_surface = &g_handle;
    image->m_textures.OnReady(owner);
}

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
    AtGrD3DFormatChooser* FormatChooser() override { return &m_chooserObject; }
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
    AtAudioBankFile::s_context = &context;

    static int token;
    AtIoTokenFile::s_heap = &token;
    AtDatabaseLoader::s_heap = &token;
    AtXboxFactory::s_loaderHeap = &token;
    GmNameMap::s_heap = &token;
    GmNameSpaceDb::s_heap = &token;
    AtIoStream::s_heap = &token;
    AtTextureDatabase::s_heap = &token;
    AtMaterialDatabase::s_heap = &token;
    AtMatAssignment::s_heap = &token;
    AtShader::SetHeap(&token);
    AtProgShaderComponent::SetHeap(&token);
    AtGrI2DFont::SetHeap(&token);
    AtShaderDatabase::SetHeap(&token);
    AtLightDatabase::SetHeap(&token);
    AtFontDatabase::SetHeap(&token);
    AtImageDatabase::SetHeap(&token);
    AtGrD3DMaterialDatabase::SetHeap(&token);
    AtGrIPixelMap::SetHeap(&token);

    Owner* owner = new Owner;
    TexloadLimits limits;
    owner->m_textureLimits = &limits;
    owner->m_noTextures = 0;
    tree.m_files["MODEL\\ROCK.Dds"] = TexloadDds(16, 16, 0);
    owner->m_textureSpaces.Init(8);
    owner->m_materialSpaces.Init(8);
    owner->m_shaderSpaces.Init(8);
    owner->m_lightSpaces.Init(8);
    owner->m_matAssignmentSpaces.Init(8);
    owner->m_fontSpaces.Init(8);
    owner->m_imageSpaces.Init(8);
    owner->m_textureDatabases = nullptr;
    owner->m_materialDatabases = nullptr;
    owner->m_fontDatabases = nullptr;
    owner->m_imageDatabases = nullptr;

    // A texture for a shader to find.
    tree.m_files["MODEL\\T.AGT"] = "Surface2D[1]\nTexture[1]\nTexture2D[1]\n{\n Texture2D \"ROCK\" { DDSSource }\n}\n";
    AtGrD3DTextureDatabase* textures = new AtGrD3DTextureDatabase;
    textures->Load(owner, "MODEL\\T", 2);
    CHECK(g_textures_made == 1);

    // Shaders: one with a staged component finding the texture, one empty.
    tree.m_files["MODEL\\S.AGS"] =
        "Shader[2]\n{\n Shader \"WET\"\n {\n  StagedShaderComponent[1]\n  {\n   StagedShaderComponent\n   {\n    TextureStage\n    {\n     Texture \"ROCK\"\n     Bilinear\n    }\n"
        "    ModulateOp ModulateTextureByDiffuse\n   }\n  }\n }\n Shader \"DRY\"\n {\n }\n}\n";
    AtGrD3DShaderDatabase* shaders = new AtGrD3DShaderDatabase;
    shaders->Load(owner, "MODEL\\S", 2);
    CHECK(shaders->m_count == 2 && shaders->m_shaders != nullptr);
    CHECK(shaders->ShaderAt(1) == &shaders->m_shaders[1] && shaders->Find("WET") == &shaders->m_shaders[0]);
    CHECK(shaders->m_shaders[0].m_stagedCount == 1 && shaders->m_shaders[0].m_stageds[0].m_stage.m_texture == textures->Texture2DAt(0));
    CHECK(owner->FindShader("DRY", nullptr) == &shaders->m_shaders[1]);
    shaders->Clear();
    CHECK(shaders->m_shaders == nullptr && shaders->m_count == 0);
    delete shaders;

    // Lights: as new until read, then the words given.
    tree.m_files["MODEL\\L.AGL"] = "Light[2]\n{\n Light \"SUN\"\n {\n  DiffuseColor 0.5 0.25 1.0 1.0\n  Range 20.0\n }\n Light \"MOON\"\n {\n }\n}\n";
    AtGrD3DLightDatabase* lights = new AtGrD3DLightDatabase;
    lights->Load(owner, "MODEL\\L", 2);
    CHECK(lights->m_count == 2 && lights->m_owner == owner);
    AtLight* sun = lights->LightAt(0);
    AtLight* moon = lights->LightAt(1);
    CHECK(lights->Find("MOON") == moon && sun->m_owner == owner);
    CHECK(sun->m_diffuse.y == 0.25f && sun->m_range == 20.0f && sun->m_ambient.x == 1.0f);
    CHECK(moon->m_range == 1000.0f && moon->m_attenuation[0] == 1.0f && moon->m_attenuation[1] == 0.0f);
    sun->Reset();
    CHECK(sun->m_owner == nullptr && sun->m_range == 1000.0f && sun->m_diffuse.y == 1.0f);
    lights->Clear();
    CHECK(lights->m_lights == nullptr && lights->m_owner == nullptr);
    delete lights;

    // Fonts: one loaded, one on demand.
    tree.m_files["UI\\F.AGF"] = "Font[2]\nFont2D[2]\n{\n Font2D \"BIG\"\n {\n  TgaSource\n  CharacterSet[2]\n  {\n   \"A\" \"B\"\n  }\n }\n Font2D \"SMALL\"\n {\n  DemandLoad\n }\n}\n";
    AtGrD3DFontDatabase* fonts = new AtGrD3DFontDatabase;
    fonts->Load(owner, "UI\\F", 2);
    CHECK(fonts->m_count == 2 && fonts->m_owner == owner && owner->m_fontDatabases == fonts);
    CHECK(g_loaded_fonts == 1 && fonts->Font2DAt(1) == &fonts->m_fonts[1] && fonts->Find("BIG") == &fonts->m_fonts[0]);
    CHECK(fonts->m_fonts[0].m_count == 2 && fonts->m_fonts[0].m_chars != nullptr && fonts->m_fonts[0].m_chars[1].m_code == 'B');
    CHECK(owner->m_textureDatabases == &fonts->m_fonts[0].m_textures && fonts->m_fonts[0].m_textures.m_next == textures);
    CHECK(fonts->Slot4() == 1);
    // Emptying a font with a borrowed table forgets it.
    AtGrI2DFontChar borrowed[1];
    fonts->m_fonts[1].m_chars = borrowed;
    fonts->m_fonts[1].m_flags |= AtGrD3DFont2D::kBorrowedChars;
    fonts->m_fonts[1].Free();
    CHECK(fonts->m_fonts[1].m_chars == nullptr && (fonts->m_fonts[1].m_flags & AtGrD3DFont2D::kBorrowedChars));  // the 2D font empties only its own fields: the flags stay
    // Clearing: off the owner's list, each font's surface released and its textures off the list too.
    g_releases = 0;
    fonts->Clear();
    CHECK(g_releases == 1 && owner->m_fontDatabases == nullptr && owner->m_textureDatabases == textures && fonts->m_fonts == nullptr && fonts->m_next == nullptr);
    delete fonts;

    // Images: two loaded, one on demand, which keeps the directory.
    tree.m_files["UI\\I.AGI"] = "MatAssignment[1]\n{\n \"alpha\"\n}\nImage[3]\n{\n Image \"A\" { TgaSource }\n Image \"B\" { DemandLoad }\n Image \"C\" { DdsSource }\n}\n";
    AtGrD3DImageDatabase* images = new AtGrD3DImageDatabase;
    images->Load(owner, "UI\\I", 2);
    CHECK(images->m_count == 3 && images->m_owner == owner && owner->m_imageDatabases == images);
    CHECK(g_loaded_images == 2 && images->ImageAt(2) == &images->m_images[2] && images->Find("B") == &images->m_images[1]);
    CHECK(images->m_directory != nullptr && (images->m_images[1].m_flags & AtImage::kDemandLoad));
    CHECK(owner->m_textureDatabases == &images->m_images[2].m_textures);
    images->m_images[0].m_data = operator new(4);
    images->m_images[0].m_flags |= AtImage::kOwnsData;
    images->m_images[0].FreeData();
    CHECK(images->m_images[0].m_data == nullptr && owner->m_textureDatabases == &images->m_images[2].m_textures && images->m_images[0].m_surface.m_surface == nullptr);
    g_releases = 0;
    images->Clear();
    CHECK(g_releases == 1 && owner->m_imageDatabases == nullptr && owner->m_textureDatabases == textures && images->m_images == nullptr);
    delete images;

    // Two font databases on the owner's list, the first cleared.
    AtGrD3DFontDatabase* a = new AtGrD3DFontDatabase;
    AtGrD3DFontDatabase* b = new AtGrD3DFontDatabase;
    a->OnReady(owner);
    b->OnReady(owner);
    CHECK(owner->m_fontDatabases == b && b->m_next == a);
    b->Clear();
    CHECK(owner->m_fontDatabases == a && b->m_next == nullptr);
    delete a;
    delete b;
    CHECK(owner->m_fontDatabases == nullptr);

    // From an .AGD, through the concrete loader: every list of this work, read and loaded.
    tree.m_files["MODEL\\THING.AGD"] =
        "TextureDatabase[1]\n{\n \"T\"\n}\nShaderDatabase[1]\n{\n \"S\"\n}\nLightDatabase[1]\n{\n \"L\"\n}\nFontDatabase[1]\n{\n \"F\"\n}\nImageDatabase[1]\n{\n \"I\"\n}\n";
    tree.m_files["MODEL\\F.AGF"] = tree.m_files["UI\\F.AGF"];
    tree.m_files["MODEL\\I.AGI"] = tree.m_files["UI\\I.AGI"];
    g_loaded_fonts = g_loaded_images = 0;
    AtXboxFactory factory;
    AtGrD3DDatabaseLoader* loader = factory.NewDatabaseLoader();
    loader->Load(owner, "MODEL\\THING", 2, 0, 1);
    CHECK(loader->m_shaders != nullptr && loader->m_shaders[0].m_count == 2);
    CHECK(loader->m_lights != nullptr && loader->m_lights[0].m_count == 2);
    CHECK(loader->m_fonts != nullptr && loader->m_fonts[0].m_count == 2 && g_loaded_fonts == 1);
    CHECK(loader->m_images != nullptr && loader->m_images[0].m_count == 3 && g_loaded_images == 2);
    CHECK(owner->m_fontDatabases == loader->m_fonts && owner->m_imageDatabases == loader->m_images);
    loader->Unload();
    CHECK(owner->m_fontDatabases == nullptr && owner->m_imageDatabases == nullptr && owner->m_textureDatabases == textures);
    delete loader;

    // What stops: the binary form, and the slots that hand out a first free item.
    g_unrecovered.clear();
    AtGrD3DFontDatabase* c = new AtGrD3DFontDatabase;
    c->Slot5(1, 2);
    c->Slot7();
    CHECK(g_unrecovered[0x00111630] == 1 && g_unrecovered[0x00111980] == 1);
    delete c;

    textures->Clear();
    delete textures;
    delete owner;
    if (g_fail == 0) std::printf("test_xshade: all passed\n");
    return g_fail != 0;
}
