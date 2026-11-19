// Host test of the Xbox database loader, .XKA and .XGG readers, on any pointer size, through written files and the real text tokenizer. Not evidence of a byte match. Supplies the functions of src/at/AtGrD3DSeam.cpp:
// what the graphics layer would do is counted here, and the items' readers step over their records by the sizes the original's do.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "at/AtGrD3DDatabase.h"
#include "at/AtAudioDatabase.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static std::map<unsigned int, int> g_unrecovered;
static int g_contiguous_allocs, g_contiguous_frees;
static int g_vertex_reads, g_index_reads, g_stream_reads, g_mesh_reads;
static std::vector<char*> g_vertex_steps;

unsigned int AtGrD3DUnrecovered(unsigned int va, void*, unsigned int, unsigned int) { g_unrecovered[va]++; return 0; }
void* AtGrD3DAllocateContiguous(unsigned int size, unsigned int) { g_contiguous_allocs++; return std::malloc(size); }
void AtGrD3DFreeContiguous(void* data) { g_contiguous_frees++; std::free(data); }
void AtGrD3DReadVertexArray(AtGrD3DVertexArray*, char** records, char** kept, char**) {
    g_vertex_reads++;
    g_vertex_steps.push_back(*kept);
    *records += 0x3C;
}
void AtGrD3DReadIndexArray(AtGrD3DIndexArray*, char** records, char**) { g_index_reads++; *records += 8; }
void AtGrD3DReadVertexStream(AtGrD3DVertexStream*, AtMaterialOwner*, char** records, char**) { g_stream_reads++; *records += 0x48; }
void AtGrD3DReadMesh(AtGrD3DMesh*, AtMaterialOwner*, AtGrD3DMeshDatabase*, char**, char**, char**, char** records, char**, char**, char* name) {
    g_mesh_reads++;
    std::strcpy(name, *records + 0x0C);
    *records += 0x18;
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
    int m_opens = 0;
    const std::string* Find(const char* path) {
        m_path.Resolve(path);
        auto f = m_files.find(m_path.m_full);
        return f == m_files.end() ? nullptr : &f->second;
    }
    int Open(const char* path, int flags, AtIoFile** out) override {
        const std::string* text = Find(path);
        if (!text) return AtIoFile::kNotFound;
        m_opens++;
        m_file.m_text = text;
        m_file.m_position = 0;
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
    AtMatrixDevice* MatrixDevice() override { return nullptr; }
};

// Little-endian words into a string, whatever the host.
void Put(std::string& s, size_t at, unsigned int v) {
    if (s.size() < at + 4) s.resize(at + 4);
    for (int i = 0; i < 4; i++) s[at + i] = (char)((v >> (8 * i)) & 0xFF);
}
void PutName(std::string& s, size_t at, const char* name) {
    if (s.size() < at + 8) s.resize(at + 8);
    std::memcpy(&s[at], name, std::strlen(name));
}

// An .XKA of two animations, three frame numbers, four rotation keys and one translation key.
std::string WriteXka() {
    std::string head(0x10, '\0');
    std::string table(0x78, '\0');
    Put(table, 0x00, 0xD0);          // scratch
    Put(table, 0x04, 2);             // animations
    Put(table, 0x08, 0x00);          // names
    Put(table, 0x0C, 0x10);          // records
    Put(table, 0x10, 0x1000);        // key flags
    Put(table, 0x14, 0x3F800000);
    Put(table, 0x18, 7);
    Put(table, 0x1C, 0x60);          // kept
    Put(table, 0x20, 2); Put(table, 0x24, 0x00);   // rotation components
    Put(table, 0x28, 1); Put(table, 0x2C, 0x18);   // translation
    Put(table, 0x48, 3); Put(table, 0x4C, 0x24);   // frames
    Put(table, 0x50, 4); Put(table, 0x54, 0x30);   // rotation keys
    Put(table, 0x58, 1); Put(table, 0x5C, 0x50);   // translation keys
    std::string scratch(0xD0, '\0');
    PutName(scratch, 0, "RUN");
    PutName(scratch, 8, "WALKFAST");
    for (int a = 0; a < 2; a++) {
        const size_t r = 0x10 + a * 0x60;
        Put(scratch, r + 0x00, a == 0 ? 1u : 0u);    // flags
        Put(scratch, r + 0x04, 0x40200000);           // rate 2.5
        Put(scratch, r + 0x20, 0x00010000 + a);       // a run cut to 16 bits
        Put(scratch, r + 0x48, 30 + a);               // length
        Put(scratch, r + 0x4C, 31 + a);               // loop
        Put(scratch, r + 0x5C, 0x40800000);           // radius 4
    }
    std::string kept(0x60, '\0');
    Put(kept, 0x24, 0x00020001);
    return head + table + scratch + kept;
}

// An .XGG of two meshes, two vertex arrays, one index array and one stream, under the name space NS.
std::string WriteXgg() {
    std::string head(0x10, '\0');
    std::string table(0x58, '\0');
    PutName(table, 0x00, "NS");
    Put(table, 0x08, 0x100);                       // scratch
    Put(table, 0x0C, 0xF0); Put(table, 0x10, 2);   // material references, meshes
    Put(table, 0x14, 0xC0);
    Put(table, 0x18, 2); Put(table, 0x1C, 0x00);   // vertex arrays
    Put(table, 0x20, 1); Put(table, 0x24, 0x78);   // index arrays
    Put(table, 0x28, 1); Put(table, 0x2C, 0x80);   // streams
    Put(table, 0x30, 0x40);                        // kept
    Put(table, 0x40, 0x10);                        // vertex data in kept
    Put(table, 0x4C, 0x20);                        // contiguous
    std::string scratch(0x100, '\0');
    PutName(scratch, 0xC0 + 0x0C, "MESHA");
    PutName(scratch, 0xD8 + 0x0C, "MESHB");
    std::string kept(0x40, 'k');
    std::string contiguous(0x20, 'c');
    return head + table + scratch + kept + contiguous;
}

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
    static AtHeap meshHeap;
    meshHeap.m_alignShift = 2;
    meshHeap.m_allocShift = 2;
    AtIoTokenFile::s_heap = &token;
    AtAnimationDatabase::s_heap = &token;
    AtDatabaseLoader::s_heap = &token;
    AtGrD3DAnimationDevice::s_heap = &token;
    AtGrD3DMeshDatabase::s_heap = &meshHeap;
    AtXboxFactory::s_loaderHeap = &token;
    AtXboxFactory::s_soundHeap = &token;
    GmNameMap::s_heap = &token;
    AtCoordSysDatabase::s_heap = &token;
    AtIoStream::s_heap = &token;
    AtTextureDatabase::s_heap = &token;
    GmNameSpaceDb::s_heap = &token;
    AtMaterialDatabase::s_heap = &token;

    tree.m_files["MODEL\\RUN.XKA"] = WriteXka();
    tree.m_files["MODEL\\BOX.XGG"] = WriteXgg();

    Owner* owner = new Owner;
    owner->m_meshSpaces.Init(8);
    owner->m_textureSpaces.Init(8);
    AtGrD3DAnimationDevice device;
    owner->m_animationDevice = &device;

    // An animation database, by itself.
    {
        AtAnimationDatabase* db = device.NewDatabase();
        CHECK(db->HasBinaryForm() == 1);
        db->Load(&device, "MODEL\\RUN", 2);
        CHECK(db->m_animationCount == 2);
        AtGrD3DAnimationDatabase* xb = static_cast<AtGrD3DAnimationDatabase*>(db);
        CHECK(db->Find("RUN") == &db->m_animations[0]);
        CHECK(db->Find("WALKFAST") == &db->m_animations[1]);
        CHECK(db->m_animations[0].m_flags == 1 && db->m_animations[1].m_flags == 0);
        CHECK(db->m_animations[0].m_rate == 2.5f);
        CHECK(db->m_animations[1].m_rotation[0] == 1);
        CHECK(db->m_animations[0].m_length == 30 && db->m_animations[1].m_loopPeriod == 32);
        CHECK(db->m_animations[1].m_boundingSphere.radius == 4.0f);
        CHECK(db->m_animations[0].m_database == db);
        CHECK(db->m_rotationCount == 2 && db->m_translationCount == 1);
        CHECK((char*)db->m_rotation == xb->m_data && (char*)db->m_translation == xb->m_data + 0x18);
        AtGrD3DAnimationKeys* keys = static_cast<AtGrD3DAnimationKeys*>(db->m_keys);
        CHECK(keys != nullptr);
        CHECK(keys->m_count == 3);
        CHECK((char*)keys->m_frames == xb->m_data + 0x24 && keys->m_frames[0] == 1 && keys->m_frames[1] == 2);
        CHECK(keys->m_rotationKeyCount == 4 && keys->m_rotationKeys == xb->m_data + 0x30);
        CHECK(keys->m_translationKeyCount == 1 && keys->m_translationKeys == xb->m_data + 0x50);
        CHECK((keys->m_flags & 0x1400) == 0x1400 && (keys->m_flags & 0xA8) == 0xA8);
        CHECK(keys->m_unknown40 == 0x3F800000 && keys->m_unknown44 == 7);
        db->Clear();
        CHECK(xb->m_data == nullptr && db->m_keys == nullptr && db->m_animations == nullptr);
        device.FreeDatabase(db);
    }

    // A mesh database, by itself.
    {
        AtGrD3DMeshDatabase* db = new AtGrD3DMeshDatabase;
        db->Load(owner, "MODEL\\BOX", 2);
        CHECK(g_vertex_reads == 2 && g_index_reads == 1 && g_stream_reads == 1 && g_mesh_reads == 2);
        CHECK(g_vertex_steps.size() == 2 && g_vertex_steps[0] == db->m_kept + 0x10 && g_vertex_steps[1] == db->m_kept + 0x10);
        CHECK(db->m_meshCount == 2 && db->m_vertexArrayCount == 2 && db->m_indexArrayCount == 1 && db->m_streamCount == 1);
        CHECK(db->Find("MESHA") == &db->m_meshes[0] && db->Find("MESHB") == &db->m_meshes[1]);
        CHECK(db->m_group != nullptr && std::strncmp(db->m_group->m_name, "NS", 8) == 0);
        CHECK(db->m_keptSize == 0x40 && db->m_kept[0] == 'k' && db->m_kept[0x3F] == 'k');
        CHECK(db->m_contiguousSize == 0x20 && ((char*)db->m_contiguous)[0x1F] == 'c' && g_contiguous_allocs == 1);
        CHECK(meshHeap.m_allocShift == 2);
        CHECK(owner->m_meshDatabases == db && db->m_owner == owner);
        CHECK(db->m_vertexArrays[1].m_owner == owner && db->m_indexArrays[0].m_owner == owner);
        CHECK(g_unrecovered[0x00122460] == 2 && g_unrecovered[0x000EC890] == 2 && g_unrecovered[0x000F8010] == 1 && g_unrecovered[0x000EECA0] == 1);
        db->Slot8();
        AtGrD3DMeshDatabase* more = static_cast<AtGrD3DMeshDatabase*>(db->m_more);
        CHECK(more != nullptr && more->m_meshCount == 2 && more->m_owner == owner);
        CHECK(owner->m_meshDatabases == more && more->m_next == db);
        delete more;
        CHECK(owner->m_meshDatabases == db);
        db->Clear();
        CHECK(owner->m_meshDatabases == nullptr && db->m_owner == nullptr);
        CHECK(g_contiguous_frees == 1);
        CHECK(g_unrecovered[0x00123390] == 4 && g_unrecovered[0x000ED260] == 2 && g_unrecovered[0x000F84B0] == 1 && g_unrecovered[0x000EEF00] == 1);
        delete db;
    }

    // From an .AGD, through the factory's loader.
    {
        tree.m_files["MODEL\\THING.AGD"] =
            "TextureDatabase[1]\n{\n \"T\"\n}\nMeshDatabase[1]\n{\n \"BOX\"\n}\nAnimationDatabase[1]\n{\n \"RUN\"\n}";
        tree.m_files["MODEL\\T.AGT"] = "";
        g_unrecovered.clear();
        g_mesh_reads = 0;
        AtXboxFactory factory;
        AtGrD3DDatabaseLoader* loader = factory.NewDatabaseLoader();
        CHECK(loader != nullptr && loader->m_textures == nullptr && loader->m_loaders == nullptr);
        loader->Load(owner, "MODEL\\THING", 2, 0, 1);
        CHECK(loader->m_textureCount == 1 && loader->m_meshCount == 1 && loader->m_animationCount == 1);
        CHECK(loader->m_textures != nullptr && loader->TextureAt(0) == loader->m_textures);
        CHECK(loader->m_textures[0].m_owner == owner && owner->m_textureDatabases == loader->m_textures);  // the recovered texture database, include/at/AtGrD3DTextures.h
        CHECK(loader->m_meshes != nullptr && loader->MeshAt(0) == loader->m_meshes);
        CHECK(loader->m_meshes[0].m_meshCount == 2 && g_mesh_reads == 2);
        CHECK(loader->m_animations != nullptr && loader->m_animations[0]->m_animationCount == 2);
        CHECK(loader->m_animations[0]->Find("WALKFAST") != nullptr);
        CHECK(owner->m_meshDatabases == loader->m_meshes);
        loader->Unload();
        CHECK(loader->m_meshes == nullptr && loader->m_textures == nullptr && owner->m_meshDatabases == nullptr);
        CHECK(owner->m_textureDatabases == nullptr);
        delete loader;
    }

    // The sound database's factory.
    {
        AtXboxFactory factory;
        AtSoXboxDatabase* sound = factory.NewSoundDatabase();
        CHECK(sound != nullptr);
        delete sound;
    }

    delete owner;
    if (g_fail == 0) std::printf("test_loader: all passed\n");
    return g_fail != 0;
}
