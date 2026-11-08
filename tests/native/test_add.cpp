// Host test of the data library and the database manager, on any pointer size. Not evidence of a byte match. The .ADR registry, the descriptor's and the value classes' other members are held by other work, so
// the seams are supplied here, by small classes of this test's own, and file loading goes through the real archive-independent text path.
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

struct Fake {
    int loaded = 0;
    std::string name;
    AtDataOwner* owner = nullptr;
    int unused = -1;
    AtDataRegistry* parent = nullptr;
    int cleared = 0;
};
std::vector<Fake*> g_registries;

class Value : public AtDataValue {
public:
    void Slot0(unsigned int) override {}
    int Read(AtDataDescriptorFields* descriptor, AtIoTokenizer* tokenizer) override {
        m_descriptor = descriptor;
        m_read = tokenizer->Next();
        m_int = tokenizer->m_int;
        m_text = tokenizer->m_string;
        return 0;
    }
    int SetDescriptor(AtDataDescriptorFields* descriptor) override {
        m_descriptor = descriptor;
        m_read = -1;
        return 0;
    }
    void Slot3() override {}
    int Slot4(unsigned int) override { return 0; }
    void* Slot5(unsigned int) override { return nullptr; }
    void Slot6(const void*, unsigned int, unsigned int) override {}
    int Equals(const void* other) override { return m_int == ((const Value*)other)->m_int; }

    int m_read = 0;
    int m_int = 0;
    std::string m_text;
    char* m_characters = nullptr;
    unsigned int m_size = 0;
    int m_resets = 0;
};

struct Block { unsigned int count; Value* values; };
std::vector<Block> g_blocks;
int g_blocksDeleted;

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
    std::string add, adb;
    std::vector<std::string> opened;
    int Open(const char* path, int flags, AtIoFile** out) override {
        opened.push_back(path);
        const std::string& text = std::strstr(path, ".ADB") ? adb : add;
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

// ---- the seams, for this test ---------------------------------------------------------------------------------------------
AtDataRegistry* AtDataRegistryNew() {
    Fake* f = new Fake;
    g_registries.push_back(f);
    return (AtDataRegistry*)f;
}
int AtDataRegistryIsLoaded(AtDataRegistry* registry) { return ((Fake*)registry)->loaded; }
void AtDataRegistryClear(AtDataRegistry* registry) { ((Fake*)registry)->cleared++; }
void AtDataRegistryDelete(AtDataRegistry* registry) {
    Fake* f = (Fake*)registry;
    for (Fake*& g : g_registries) if (g == f) g = nullptr;
    delete f;
}
void AtDataRegistryLoad(AtDataRegistry* registry, AtDataOwner* owner, const char* name, int unused, char* space, AtDataRegistry* parent) {
    Fake* f = (Fake*)registry;
    f->loaded = 1;
    f->name = name;
    f->owner = owner;
    f->unused = unused;
    f->parent = parent;
    CHECK(space[0] == 0);
    space[0] = 'Z';
}

AtDataDescriptorFields* AtDataDescriptorArrayNew(unsigned int count) {
    AtDataDescriptorFields* d = new AtDataDescriptorFields[count];
    for (unsigned int i = 0; i < count; i++) {
        std::memset(&d[i], 0, sizeof d[i]);
        d[i].m_type = 13;
        d[i].m_count = 1;
        d[i].m_intCeiling = 0x7FFFFFFF;
    }
    return d;
}
void AtDataDescriptorArrayDelete(AtDataDescriptorFields* descriptors) {
    delete[] descriptors;
}
void AtDataDescriptorClear(AtDataDescriptorFields*) {}

AtDataValue* AtDataValueArrayNew(unsigned int, unsigned int count) {
    Value* v = new Value[count ? count : 1];
    g_blocks.push_back({count, v});
    return v;
}
void AtDataValueArrayDelete(AtDataValue* values) {
    delete[] (Value*)values;
    g_blocksDeleted++;
}
unsigned int AtDataValueSize(unsigned int) { return (unsigned int)sizeof(Value); }
void AtDataStringReset(AtDataValue* value) { ((Value*)value)->m_resets++; }
void AtDataStringBindAndRead(AtDataValue* value, AtDataDescriptorFields* descriptor, AtIoTokenizer* tokenizer, char* characters, unsigned int size) {
    value->Read(descriptor, tokenizer);
    Value* v = (Value*)value;
    v->m_characters = characters;
    v->m_size = size;
}
void AtDataStringBind(AtDataValue* value, AtDataDescriptorFields* descriptor, char* characters, unsigned int size) {
    value->SetDescriptor(descriptor);
    Value* v = (Value*)value;
    v->m_characters = characters;
    v->m_size = size;
}

static const char kAdb[] =
    "NameSpace \"sp\"\n"
    "DataComponentDescs[ 3 ] {\n"
    " DataComponentDesc { Name \"i\" Type Int Floor -1 Ceiling 9 }\n"
    " DataComponentDesc { Name \"r\" Type Real Floor 0.5 Ceiling 1.5 FormatString \"%f\" }\n"
    " DataComponentDesc { Name \"s\" Type String Ceiling 7 }\n"
    "}\n"
    "RecordLayoutDatabases[ 1 ] { RecordLayoutDatabase { Name \"Lay\" Desc \"about\" DataComponents[ 3 ] { \"i\" \"r\" \"s\" } } }\n"
    "DataRecordDatabases[ 1 ] { DataRecordDatabase { Name \"Tab\" Desc \"rows\" RecordLayout \"Lay\" DataRecords[ 2 ] { 1 2 \"a\" 3 4 \"b\" } } }\n";

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

    // A library of the shape on the disc.
    AtDataOwner* owner = new AtDataOwner;
    owner->m_databaseSpaces.Init(8);
    fs.add = "DataRegistry[ 2 ]\n{\n \"Design\\Registry\\Actor\"\n \"Design\\Registry\\NPC\"\n}\n";
    int parentToken;
    const int top = fs.m_path.m_top;
    {
        AtDataLibrary library;
        CHECK(library.m_loaded == 0);
        library.Load(owner, "Library", 7, (AtDataRegistry*)&parentToken);
        CHECK(library.m_loaded == 1);
        CHECK(library.m_owner == owner);
        CHECK(library.m_registryCount == 2);
        CHECK(library.m_databaseCount == 0);
        CHECK(AtDataLibrary::s_registryNames == nullptr);
        CHECK(fs.opened.back() == "Library.ADD");
        CHECK(fs.m_path.m_top == top);
        Fake* a = (Fake*)library.m_registries[0];
        Fake* b = (Fake*)library.m_registries[1];
        CHECK(a->name == "Design\\Registry\\Actor");
        CHECK(b->name == "Design\\Registry\\NPC");
        CHECK(a->owner == owner && a->unused == 7 && a->parent == (AtDataRegistry*)&parentToken);
        library.Clear();
        CHECK(library.m_loaded == 0 && library.m_registries == nullptr && library.m_registryCount == 0 && library.m_owner == nullptr);
    }

    // With a database.
    fs.add = "DatabaseManager[ 1 ] { \"Thing\" }\nDataRegistry[ 1 ] { \"One\" }\n";
    fs.adb = kAdb;
    g_blocks.clear();
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
        CHECK(g_blocks.size() == 3);
        AtDataRecord* r = t->RecordAt(1);
        CHECK(r->m_index == 1 && r->m_inUse == 1);
        CHECK(r->FieldAt(0) == &((Value*)t->m_columns[0])[1]);
        CHECK(((Value*)r->FieldNamed("i"))->m_int == 3);
        CHECK(r->DescriptorAt(0) == &m->m_descriptors[0]);
        CHECK(((Value*)r->FieldNamed("s"))->m_text == "b");
        CHECK(((Value*)r->FieldNamed("s"))->m_characters == t->m_characters + 8);
        CHECK(((Value*)r->FieldNamed("s"))->m_size == 8);
        CHECK(t->FindByValue(0, r->FieldAt(0)) == r);
        unsigned int index = 77;
        CHECK(t->TakeUnused(&index) == nullptr && index == 77);
        CHECK(t->Unknown7(1) == 0);
        Value* strings = (Value*)t->m_columns[2];
        library.Clear();
        (void)strings;
        CHECK(AtDatabaseManager::s_first == nullptr);
        CHECK(g_blocksDeleted == 3);
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
        CHECK(((Value*)t->m_records[3].FieldAt(0))->m_read == -1);
        unsigned int index = 77;
        CHECK(t->TakeUnused(&index) == &t->m_records[2] && index == 2);
        CHECK(t->m_records[2].m_inUse == 1);
    }

    std::printf(g_fail ? "FAILED\n" : "ok\n");
    return g_fail ? 1 : 0;
}
