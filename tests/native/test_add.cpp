// Host test of the data library and the database manager, on any pointer size. Not evidence of a byte match. The registries, descriptors and values are the recovered .ADR classes; the three members of
// AtDataString no branch has recovered are supplied here, and file loading goes through the real archive-independent text path.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtDataLibrary.h"
#include "at/AtIoFileSystem.h"
#include "at/AtIoTokenizer.h"

void AtPushHeap(void*);
void AtPopHeap();

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

// What the three string seams were asked, by value.
struct Bound {
    char* characters = nullptr;
    unsigned int size = 0;
    int resets = 0;
};
std::vector<std::pair<const void*, Bound>> g_bound;
Bound& bound(const void* value) {
    for (auto& b : g_bound) if (b.first == value) return b.second;
    g_bound.push_back({value, Bound()});
    return g_bound.back().second;
}

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// A file system with one text in memory, by extension.
class File : public AtIoFile {
public:
    const char* data = nullptr;
    size_t size = 0;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        const size_t have = size - m_position;
        if (n > have) n = (unsigned int)have;
        if (n) std::memcpy(out, data + m_position, n);
        m_position += n;
        if (done) *done = n;
        return n ? 0 : (int)AtIoFile::kEndOfFile;
    }
    int Write(const void*, unsigned int, unsigned int*) override { return 0; }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class Fs : public AtIoFileSystem {
public:
    File file;
    std::string add, adb, adr;
    std::vector<std::string> opened;
    int Open(const char* path, int flags, AtIoFile** out) override {
        opened.push_back(path);
        const std::string& text = std::strstr(path, ".ADB") ? adb : std::strstr(path, ".ADR") ? adr : add;
        file.data = text.data();
        file.size = text.size();
        file.m_position = 0;
        file.m_flags = (flags & 0x21) ? 0x0C : 2;
#if defined(_MSC_VER)
        strcpy_s(file.m_path, "DESIGN\\LIBRARY.ADD");
#else
        std::strcpy(file.m_path, "DESIGN\\LIBRARY.ADD");
#endif
        *out = &file;
        return 0;
    }
    int Close(AtIoFile*) override { return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char*) override { return 0; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

}  // namespace

// ---- the seams, for this test --------------------------------------------------------------------------------------------
void AtDataStringReset(AtDataString* value) {
    value->Reset();
    bound(value).resets++;
}
void AtDataStringBindAndRead(AtDataString* value, AtDataDescriptor* descriptor, AtIoTokenizer* tokenizer, char* characters, unsigned int size) {
    value->Read(descriptor, tokenizer);
    bound(value).characters = characters;
    bound(value).size = size;
}
void AtDataStringBind(AtDataString* value, AtDataDescriptor* descriptor, char* characters, unsigned int size) {
    value->SetDescriptor(descriptor);
    bound(value).characters = characters;
    bound(value).size = size;
}

// One registry, the same text for every name.
static const char kAdr[] =
    "DataComponentTypes[ 1 ] { DataComponent { Type Int NumComponents 2 } }\n"
    "DataRegistry[ 1 ] { DataLink { Name \"Speed\" Type Int Data 5 } }\n";

static const char kAdb[] =
    "NameSpace \"sp\"\n"
    "DataComponentDescs[ 3 ] {\n"
    " DataComponentDesc { Name \"i\" Type Int Floor -1 Ceiling 9 }\n"
    " DataComponentDesc { Name \"r\" Type Real Floor 0.5 Ceiling 1.5 FormatString \"%f\" }\n"
    " DataComponentDesc { Name \"s\" Type String Ceiling 7 }\n"
    "}\n"
    "RecordLayoutDatabases[ 1 ] { RecordLayoutDatabase { Name \"Lay\" Desc \"about\" DataComponents[ 3 ] { \"i\" \"r\" \"s\" } } }\n"
    "DataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"Tab\" Desc \"rows\" RecordLayout \"Lay\" DataRecords[ 2 ] { 1 2.0 \"a\" 3 4.0 \"b\" } } }\n";

int main() {
    Fs fs;
    AtIoContext context;
    AtIoFileSystem* stack[4] = {&fs};
    context.m_stack = stack;
    context.m_stackCount = 1;
    context.m_stackSize = 4;
    AtIoFileRef::s_context = &context;
    static int heap;
    AtIoTokenFile::s_heap = &heap;
    AtIoStream::s_heap = &heap;
    GmNameMap::s_heap = &heap;
    GmNameSpaceDb::s_heap = &heap;
    AtDataLibrary::SetHeap(&heap);
    AtDatabaseManager::SetHeap(&heap);
    AtRecordLayout::SetHeap(&heap);
    AtDataRecordDatabase::SetHeap(&heap);
    AtDataRegistry::SetHeap(&heap);
    AtDataComponent::SetHeap(&heap);
    AtDataDescriptor::SetHeap(&heap);
    AtDataLink::SetHeap(&heap);
    AtDataRegistryNameSpaces::SetHeap(&heap);

    // A library of the shape on the disc.
    AtDataRegistryOwner* owner = new AtDataRegistryOwner;
    owner->m_databaseSpaces.Init(8);
    owner->m_spaces.Init(8);
    fs.add = "DataRegistry[ 2 ]\n{\n \"Design\\Registry\\Actor\"\n \"Design\\Registry\\NPC\"\n}\n";
    fs.adr = kAdr;
    AtDataRegistry* parent = new AtDataRegistry;
    {
        char space[8] = {0};
        parent->Load(owner, "Generic", 0, space, nullptr);
        CHECK(parent->m_loaded == 1);
    }
    const int top = fs.m_path.m_top;
    {
        AtDataLibrary library;
        CHECK(library.m_loaded == 0);
        library.Load(owner, "Library", 7, parent);
        CHECK(library.m_loaded == 1);
        CHECK(library.m_owner == owner);
        CHECK(library.m_registryCount == 2);
        CHECK(library.m_databaseCount == 0);
        CHECK(AtDataLibrary::s_registryNames == nullptr);
        CHECK(fs.opened[fs.opened.size() - 3] == "Library.ADD");  // then the two registries
        CHECK(fs.m_path.m_top == top);
        AtDataRegistry* a = library.m_registries[0];
        AtDataRegistry* b = library.m_registries[1];
        CHECK(fs.opened[fs.opened.size() - 2] == "Design\\Registry\\Actor.ADR");
        CHECK(fs.opened.back() == "Design\\Registry\\NPC.ADR");
        CHECK(a->m_loaded == 1 && b->m_loaded == 1);
        CHECK(a->m_owner == owner && a->m_parent == parent && b->m_parent == parent);
        CHECK(a->m_pools[AtDataRegistry::kInt].m_count == 2);
        CHECK(a->m_pools[AtDataRegistry::kInt].m_links[0].m_inUse == 1);
        CHECK(std::strcmp(a->m_pools[AtDataRegistry::kInt].m_links[0].m_name, "Speed") == 0);
        CHECK(((AtDataInt*)a->m_pools[AtDataRegistry::kInt].ElementAt(0))->m_value == 5);
        CHECK(owner->m_plain.m_first == b);
        library.Clear();
        CHECK(library.m_loaded == 0 && library.m_registries == nullptr && library.m_registryCount == 0 && library.m_owner == nullptr);
    }

    // With a database.
    fs.add = "DatabaseManager[ 1 ] { \"Thing\" }\nDataRegistry[ 1 ] { \"One\" }\n";
    fs.adb = kAdb;
    {
        AtDataLibrary library;
        library.Load(owner, "Library", 0, nullptr);
        CHECK(library.m_databaseCount == 1);
        CHECK(library.m_registryCount == 1);
        CHECK(fs.opened.back() == "Thing.ADB");
        AtDatabaseManager* m = library.m_databases[0];
        CHECK(m->m_loaded == 1);
        CHECK(std::strcmp(m->m_fileName, "DESIGN\\LIBRARY.ADD") == 0);
        CHECK(m->m_descriptorCount == 3 && m->m_layoutCount == 1 && m->m_tableCount == 1);
        CHECK(AtDatabaseManager::s_first == m);
        CHECK(m->m_group != nullptr);
        CHECK(std::strncmp(m->m_group->m_name, "sp", 8) == 0);
        CHECK(m->m_descriptors[0].m_type == 2 && m->m_descriptors[0].m_intFloor == -1 && m->m_descriptors[0].m_intCeiling == 9);
        CHECK(m->m_descriptors[1].m_type == 3 && m->m_descriptors[1].m_realFloor == 0.5f && m->m_descriptors[1].m_realCeiling == 1.5f);
        CHECK(std::strcmp(m->m_descriptors[1].m_format, "%f") == 0);
        CHECK(m->m_descriptorsByName.Find("s") == &m->m_descriptors[2]);
        AtRecordLayout* l = &m->m_layouts[0];
        CHECK(m->Find("Lay") == l);
        CHECK(l->m_count == 3 && l->FieldAt(1) == &m->m_descriptors[1]);
        CHECK(l->Find("r") == &l->m_fields[1]);
        CHECK(std::strcmp(l->m_description, "about") == 0);
        AtDataRecordDatabase* t = m->m_tables[0];
        CHECK(m->Find("Tab") == t);
        CHECK(m->FindEverywhere("Tab") == t);
        CHECK(m->FindEverywhere("None") == nullptr);
        CHECK(t->m_count == 2 && t->m_layout == l);
        CHECK(t->m_characterSize == 16 && t->m_characterUsed == 16);
        AtDataRecord* r = t->RecordAt(1);
        CHECK(r->m_index == 1 && r->m_inUse == 1);
        CHECK(r->FieldAt(0) == &((AtDataInt*)t->m_columns[0])[1]);
        CHECK(((AtDataInt*)r->FieldNamed("i"))->m_value == 3);
        CHECK(r->DescriptorAt(0) == &m->m_descriptors[0]);
        CHECK(r->FieldNamed("s")->m_descriptor == &m->m_descriptors[2]);
        CHECK(bound(r->FieldNamed("s")).characters == t->m_characters + 8);
        CHECK(bound(r->FieldNamed("s")).size == 8);
        unsigned int index = 77;
        CHECK(t->TakeUnused(&index) == nullptr && index == 77);
        CHECK(t->Unknown7(1) == 0);
        const void* firstString = t->m_columns[2];
        library.Clear();
        CHECK(AtDatabaseManager::s_first == nullptr);
        CHECK(bound(firstString).resets == 1);
    }

    // Rows no file gave, on a manager of its own.
    fs.adb = kAdb;
    {
        AtDatabaseManager manager;
        char space[8] = {0};
        manager.Load(owner, "Thing", 0, space, 2, 1);
        CHECK(std::strncmp(space, "sp", 8) == 0);
        AtDataRecordDatabase* t = manager.m_tables[0];
        CHECK(t->m_count == 4);
        CHECK(t->m_records[1].m_inUse == 1 && t->m_records[2].m_inUse == 0);
        CHECK(t->m_records[3].FieldAt(0)->m_descriptor == &manager.m_descriptors[0]);
        unsigned int index = 77;
        CHECK(t->TakeUnused(&index) == &t->m_records[2] && index == 2);
        CHECK(t->m_records[2].m_inUse == 1);
    }

    std::printf(g_fail ? "FAILED\n" : "ok\n");
    return g_fail ? 1 : 0;
}
