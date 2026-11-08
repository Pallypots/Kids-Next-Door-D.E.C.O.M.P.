// The Xbox database loader: what each kind of an .AGD's lists is made as. And the platform factory's two functions that make a loader and a sound database. See include/at/AtGrD3DDatabase.h. Names inferred.
#include "at/AtGrD3DDatabase.h"

#include "at/AtAudioDatabase.h"

void AtRaise(int code, int a, int b, int c);
void AtPushHeap(void* heap);
void AtPopHeap();

void* AtXboxFactory::s_soundHeap;
void* AtXboxFactory::s_loaderHeap;

namespace {

// Each kind's array is made the same way, from the loader's heap, by its count.
template <class T> void MakeArray(T** array, unsigned int count) {
    if (AtDatabaseLoader::s_heap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(AtDatabaseLoader::s_heap);
    *array = new T[count];
    AtPopHeap();
    if (*array == 0) {
        AtRaise(-4, 0, 0, 0);
    }
}

}  // namespace

AtGrD3DDatabaseLoader::AtGrD3DDatabaseLoader()
    : m_textures(0), m_shaders(0), m_materials(0), m_meshes(0), m_lights(0), m_fonts(0), m_images(0), m_vertexProcesses(0), m_pixelProcesses(0), m_loaders(0) {
}

AtGrD3DDatabaseLoader::~AtGrD3DDatabaseLoader() {
    Unload();
}

void AtGrD3DDatabaseLoader::MakeTextures() {
    MakeArray(&m_textures, m_textureCount);
}

void AtGrD3DDatabaseLoader::MakeMaterials() {
    MakeArray(&m_materials, m_materialCount);
}

void AtGrD3DDatabaseLoader::MakeShaders() {
    MakeArray(&m_shaders, m_shaderCount);
}

void AtGrD3DDatabaseLoader::MakeMeshes() {
    MakeArray(&m_meshes, m_meshCount);
}

void AtGrD3DDatabaseLoader::MakeLights() {
    MakeArray(&m_lights, m_lightCount);
}

void AtGrD3DDatabaseLoader::Slot5() {
}

void AtGrD3DDatabaseLoader::MakeFonts() {
    MakeArray(&m_fonts, m_fontCount);
}

void AtGrD3DDatabaseLoader::MakeImages() {
    MakeArray(&m_images, m_imageCount);
}

void AtGrD3DDatabaseLoader::MakeVertexProcesses() {
    MakeArray(&m_vertexProcesses, m_vertexProcessCount);
}

void AtGrD3DDatabaseLoader::MakePixelProcesses() {
    MakeArray(&m_pixelProcesses, m_pixelProcessCount);
}

void AtGrD3DDatabaseLoader::MakeLoaders() {
    MakeArray(&m_loaders, m_loaderCount);
}

void AtGrD3DDatabaseLoader::Unload() {
    if (m_loaders != 0) {
        delete[] m_loaders;
    }
    if (m_pixelProcesses != 0) {
        delete[] m_pixelProcesses;
    }
    if (m_vertexProcesses != 0) {
        delete[] m_vertexProcesses;
    }
    if (m_images != 0) {
        delete[] m_images;
    }
    if (m_fonts != 0) {
        delete[] m_fonts;
    }
    if (m_lights != 0) {
        delete[] m_lights;
    }
    if (m_meshes != 0) {
        delete[] m_meshes;
    }
    if (m_materials != 0) {
        delete[] m_materials;
    }
    if (m_shaders != 0) {
        delete[] m_shaders;
    }
    if (m_textures != 0) {
        delete[] m_textures;
    }
    AtDatabaseLoader::Unload();
    m_textures = 0;
    m_shaders = 0;
    m_materials = 0;
    m_meshes = 0;
    m_lights = 0;
    m_fonts = 0;
    m_images = 0;
    m_vertexProcesses = 0;
    m_pixelProcesses = 0;
    m_loaders = 0;
}

AtTextureDatabase* AtGrD3DDatabaseLoader::TextureAt(unsigned int index) {
    return &m_textures[index];
}

AtMaterialDatabase* AtGrD3DDatabaseLoader::MaterialAt(unsigned int index) {
    return &m_materials[index];
}

AtShaderDatabase* AtGrD3DDatabaseLoader::ShaderAt(unsigned int index) {
    return &m_shaders[index];
}

AtMeshDatabase* AtGrD3DDatabaseLoader::MeshAt(unsigned int index) {
    return &m_meshes[index];
}

AtLightDatabase* AtGrD3DDatabaseLoader::LightAt(unsigned int index) {
    return &m_lights[index];
}

AtFontDatabase* AtGrD3DDatabaseLoader::FontAt(unsigned int index) {
    return &m_fonts[index];
}

AtImageDatabase* AtGrD3DDatabaseLoader::ImageAt(unsigned int index) {
    return &m_images[index];
}

AtVertexProcessDatabase* AtGrD3DDatabaseLoader::VertexProcessAt(unsigned int index) {
    return &m_vertexProcesses[index];
}

AtPixelProcessDatabase* AtGrD3DDatabaseLoader::PixelProcessAt(unsigned int index) {
    return &m_pixelProcesses[index];
}

AtDatabaseLoader* AtGrD3DDatabaseLoader::LoaderAt(unsigned int index) {
    return &m_loaders[index];
}

// ---- The factory --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

AtSoXboxDatabase* AtXboxFactory::NewSoundDatabase() {
    if (s_soundHeap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_soundHeap);
    AtSoXboxDatabase* database = new AtSoXboxDatabase;
    AtPopHeap();
    if (database == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return database;
}

AtGrD3DDatabaseLoader* AtXboxFactory::NewDatabaseLoader() {
    if (s_loaderHeap == 0) {
        AtRaise(-2, 0, 0, 0);
    }
    AtPushHeap(s_loaderHeap);
    AtGrD3DDatabaseLoader* loader = new AtGrD3DDatabaseLoader;
    AtPopHeap();
    if (loader == 0) {
        AtRaise(-4, 0, 0, 0);
    }
    return loader;
}
