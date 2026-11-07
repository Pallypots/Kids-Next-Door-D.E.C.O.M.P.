// Host test of the .AOD and .AOE files through the real text tokenizer and the real lists, on any pointer size. Not evidence of a byte match. Supplies the functions of src/at/AtSoXboxHost.cpp.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>

#include "at/AtAudioDatabase.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static int g_loads, g_processes, g_updates, g_creates[4], g_mixes, g_stops, g_releases, g_status_calls;
static int g_load_status = 7;
static int g_stream;

int AtSoXboxXactBankLoad(AtSoXboxXactBank*) { g_loads++; return g_load_status; }
void AtSoXboxXactBankProcess(AtSoXboxXactBank*) { g_processes++; }
void AtSoXboxXactBankUpdate(AtSoXboxXactBank*) { g_updates++; }
int AtSoXboxCreateStream(void*, int kind, void** stream) { g_creates[kind & 3]++; *stream = &g_stream; return 0; }
int AtSoXboxSetMixBins(void*, const AtSoXboxMixBins* bins) { g_mixes++; CHECK(bins->m_count == 1 && bins->m_bins[0].m_bin == 2); return 0; }
void AtSoXboxStopStream(void*) { g_stops++; }
void AtSoXboxGetStreamStatus(void*, unsigned int* status) { *status = g_status_calls++ % 2 == 0 ? 1 : 0; }
void AtSoXboxReleaseStream(void*) { g_releases++; }

namespace {

float g_position[3];
int g_identity;
void Identity(AtMatrix*) { g_identity++; }
void Translation(AtMatrix*, const AtVector3* v) { g_position[0] = v->x; g_position[1] = v->y; g_position[2] = v->z; }
void Rotation(AtMatrix*, const AtQuaternion*) {}

struct Mat { int unused; };
class Matrices : public AtMatrixDevice {
public:
    Mat m_mats[64];
    int m_used = 0;
    int m_freed = 0;
    AtMatrix* NewMatrix() override { return (AtMatrix*)&m_mats[m_used++]; }
    AtMatrixPool* NewPool() override { return nullptr; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix*) override { m_freed++; }
    void FreePool(AtMatrixPool*) override {}
};

class System : public AtSoXboxSystem {
public:
    void Unknown0() override {}
    void Unknown1() override {}
    void Add(AtAudioBank*) override {}
    void Remove(AtAudioBank*) override {}
};

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

int g_heap_token;

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
    g_atMatrix.Identity = Identity;
    g_atMatrix.SetTranslation = Translation;
    g_atMatrix.SetRotation = Rotation;

    tree.m_files["AUDIO\\G.AOD"] =
        "// a comment\nBankDatabase[2]\n{\n \"A\"\n \"SUB\\B\"\n}\nSegmentDatabase[1]\n{\n \"A\"\n}\nEmitterDatabase[1]\n{\n \"E\"\n}\n";
    tree.m_files["AUDIO\\A.AOB"] = "Bank[2]\n{\n Bank \"One\"\n {\n PlatformSource\n }\n Bank \"Two\"\n {\n PlatformSource DelayLoad\n }\n}\n";
    tree.m_files["AUDIO\\SUB\\B.AOB"] = "Bank[2]\n{\n Bank \"One\"\n {\n PlatformSource\n }\n Bank \"Disk\"\n {\n PlatformSource DiskResident\n }\n}\n";
    tree.m_files["AUDIO\\A.AOS"] = "Segment[2]\n{\n Segment \"S1\"\n {\n Bank \"Two\" \"\" 3\n }\n Segment \"S2\"\n {\n Bank \"Gone\" \"\" 4\n Looping\n }\n}\n";
    tree.m_files["AUDIO\\E.AOE"] =
        "Emitter[4]\n{\n Emitter \"sfx\"\n {\n }\n Emitter \"voice\"\n {\n Center\n }\n Emitter \"actor1\"\n {\n Emitter3D\n Position 1.0 2.5 -3.0\n }\n Emitter \"stereo\"\n {\n Stereo\n Orientation 0.0 0.0 0.0 1.0\n }\n}\n";

    Matrices matrices;
    System system;
    system.m_directSound = &g_stream;
    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_unknown08 = &matrices;
    owner->m_loader = &system;

    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::SetHeap(&g_heap_token);
    AtSoXboxDatabase::SetHeap(&g_heap_token);
    AtAudioEmitterDatabase::SetHeap(&g_heap_token);
    AtAudioSegmentDatabase::SetHeap(&g_heap_token);
    AtAudioBankDatabase::SetHeap(&g_heap_token);

    AtSoXboxDatabase database;
    database.Load(owner, "AUDIO\\G", 0);

    // The lists, the banks, and what is left of the names.
    CHECK(database.m_owner == owner);
    CHECK(database.m_bankCount == 2);
    CHECK(database.m_segmentCount == 1);
    CHECK(database.m_emitterCount == 1);
    CHECK(database.m_nestedCount == 0);
    CHECK(AtAudioDatabase::s_bankNames == nullptr);
    CHECK(AtAudioDatabase::s_segmentNames == nullptr);
    CHECK(AtAudioDatabase::s_emitterNames == nullptr);
    CHECK(tree.m_path.m_top == -1);

    AtAudioBank* one = (AtAudioBank*)owner->m_first.Find("One");
    AtAudioBank* two = (AtAudioBank*)owner->m_first.Find("Two");
    CHECK(one != nullptr);
    CHECK(two != nullptr);
    CHECK(owner->m_first.Find("Disk") == nullptr);  // A residence given makes no bank.
    CHECK(one && one->UserCount() == 2);            // Shared by the second list.
    CHECK(two && two->UserCount() == 1);
    CHECK(((AtSoXboxXactBank*)one)->m_system == &system);
    CHECK(((AtSoXboxXactBank*)two)->m_status == 3);
    CHECK(g_loads == 1);      // Two has DelayLoad, and One is made only once.
    CHECK(g_processes == 1);  // Load said pending.

    AtAudioSegment* s1 = (AtAudioSegment*)owner->m_second.Find("S1");
    AtAudioSegment* s2 = (AtAudioSegment*)owner->m_second.Find("S2");
    CHECK(s1 && s1->m_bank == two && s1->m_index == 3 && s1->m_looping == 0);
    CHECK(s2 && s2->m_bank == nullptr && s2->m_looping == 1);

    // The table the game finds emitters in.
    AtSoXboxEmitter* sfx = (AtSoXboxEmitter*)owner->m_nodes.Find("sfx");
    AtSoXboxEmitter* voice = (AtSoXboxEmitter*)owner->m_nodes.Find("voice");
    AtSoXboxEmitter* actor = (AtSoXboxEmitter*)owner->m_nodes.Find("actor1");
    AtSoXboxEmitter* stereo = (AtSoXboxEmitter*)owner->m_nodes.Find("stereo");
    CHECK(sfx && sfx->m_kind == AtAudioEmitterDesc::kPlain && sfx->m_stream == nullptr);
    CHECK(voice && voice->m_kind == AtAudioEmitterDesc::kCenter && voice->m_stream == &g_stream);
    CHECK(actor && actor->m_kind == AtAudioEmitterDesc::kEmitter3D && actor->m_stream == &g_stream);
    CHECK(stereo && stereo->m_kind == AtAudioEmitterDesc::kStereo && stereo->m_stream == nullptr);
    CHECK(sfx && sfx->m_owner == owner && sfx->m_dirty == AtCoordSys::kAllLocal);
    CHECK(g_creates[1] == 1 && g_creates[2] == 1 && g_mixes == 1);
    CHECK(matrices.m_used == 8);
    CHECK(g_identity == 8);

    // Over itself: everything goes, and comes back.
    g_load_status = 0;
    database.Load(owner, "AUDIO\\G", 0);
    CHECK(matrices.m_freed == 8);
    CHECK(g_updates == 2);
    CHECK(g_stops == 2 && g_releases == 2);
    CHECK(g_loads == 2);
    CHECK(g_processes == 1);  // Load said done.
    CHECK(owner->m_nodes.Find("voice") != nullptr);

    database.Clear();
    CHECK(database.m_owner == nullptr);
    CHECK(database.m_bankCount == 0);
    CHECK(database.m_bankDatabases == nullptr);
    database.Clear();  // Twice is fine.

    // Each index slot.
    AtSoXboxDatabase second;
    second.m_bankCount = 2;
    second.m_segmentCount = 2;
    second.m_emitterCount = 2;
    second.m_nestedCount = 1;
    second.AllocateBankDatabases();
    second.AllocateSegmentDatabases();
    second.AllocateEmitterDatabases();
    second.AllocateNestedDatabases();
    CHECK(second.BankDatabaseAt(1) == &second.m_bankDatabases[1]);
    CHECK(second.BankDatabaseAt(2) == nullptr);
    CHECK(second.SegmentDatabaseAt(1) == &second.m_segmentDatabases[1]);
    CHECK(second.EmitterDatabaseAt(1) == &second.m_emitterDatabases[1]);
    CHECK(second.NestedDatabaseAt(0) == &second.m_nestedDatabases[0]);

    if (g_fail == 0) std::printf("OK\n");
    return g_fail ? 1 : 0;
}
