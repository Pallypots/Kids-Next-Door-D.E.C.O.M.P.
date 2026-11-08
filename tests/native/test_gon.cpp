// Host test of the .GON sound notes through the real text tokenizer and real maps, on any pointer size. Not evidence of a byte match.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "at/AtAudioDatabase.h"
#include "at/AtIoTokenizer.h"
#include "at/AtNameTableOwner.h"
#include "gm/GmAudioData.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

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

class Segments : public AtAudioSegmentDatabase {
public:
    void OnReady() override {}
    void Unknown3(void*) override {}
    AtAudioSegment* SegmentAt(unsigned int) override { return nullptr; }
};

class Database : public AtAudioDatabase {
public:
    Segments m_list;
    void AllocateBankDatabases() override {}
    void AllocateSegmentDatabases() override {}
    void AllocateEmitterDatabases() override {}
    void AllocateNestedDatabases() override {}
    AtAudioBankDatabase* BankDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int) override { return &m_list; }
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioDatabase* NestedDatabaseAt(unsigned int) override { return nullptr; }
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
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    GmAudioData::s_heap = &g_heap_token;

    tree.m_files["AUDIO\\PC.GON"] =
        "SoundNotes[3]\n{\n Note \"step\"\n {\n  AnimationTrigger \"foot\"\n  ActorEmitter\n  Segments [2]\n  {\n   \"step1\" 1.0\n   \"step2\" 3.0\n  }\n }\n"
        " Note \"say\"\n {\n  AnimationTrigger \"talk\"\n  VFXTrigger \"sparkle\"\n  GameEvent \"evt\" \"arg\"\n  GlobalEmitter \"voice\"\n  Conditional \"voice\"\n  Stop \"step\"\n  Singleton\n  Segments [1]\n  {\n   \"gone\"\n  }\n }\n"
        " Note \"quiet\"\n {\n }\n}\n";
    tree.m_files["AUDIO\\NONE.GON"] = "NameSpace \"X\"\nSoundNotes[0]\n{\n}\n";

    AtNameTableOwner* owner = new AtNameTableOwner;
    int emitter;
    GmNameMap emitters;
    emitters.Init(4);
    emitters.Insert("voice", &emitter);
    owner->m_nodes.Add(&emitters);
    GmAudioData::s_soundOwner = owner;

    int segment1, segment2;
    Database database;
    database.m_segmentCount = 1;
    database.m_list.Init(4);
    database.m_list.Insert("step1", &segment1);
    database.m_list.Insert("step2", &segment2);

    GmAudioData* audio = new GmAudioData;
    std::memset(audio->m_databases, 0, sizeof audio->m_databases);
    std::memset(audio->m_firstDatabases, 0, sizeof audio->m_firstDatabases);
    std::memset(audio->m_noteBankUsed, 0, sizeof audio->m_noteBankUsed);
    audio->m_firstDatabaseCount = 0;
    audio->m_databases[5] = &database;
    GmAudioData::s_instance = audio;

    // A file of no notes leaves its bank unused.
    GmSoundNoteBank* bank = audio->AllocateNoteBank();
    CHECK(bank == &audio->m_noteBanks[0]);
    audio->LoadNotes(bank, "AUDIO\\NONE");
    CHECK(bank->m_notes == nullptr);
    CHECK(bank->m_noteCount == 0);
    audio->ReleaseNoteBank(bank);
    CHECK(audio->m_noteBankUsed[0] == 0);

    // The manifest's item: a bank, and a load.
    GmSoundNoteItem item;
    GmSoundNoteItem::Request request;
    std::strcpy(request.m_path, "AUDIO\\PC");
    item.Configure(&request);
    CHECK(std::strcmp(item.m_extension, "GON") == 0);
    CHECK(std::strcmp(item.m_path, "AUDIO\\PC") == 0);
    item.Run();
    CHECK(item.m_bank == &audio->m_noteBanks[0]);
    CHECK(audio->m_noteBankUsed[0] == 1);
    bank = item.m_bank;
    CHECK(bank->m_noteCount == 3);
    const GmSoundNote* notes = bank->m_notes;
    CHECK(std::strcmp(notes[0].m_name, "step") == 0);
    CHECK(std::strcmp(notes[0].m_trigger, "foot") == 0);
    CHECK(notes[0].m_emitter == nullptr);
    CHECK(notes[0].m_choiceCount == 2);
    CHECK(std::strcmp(notes[0].m_choices[1].m_name, "step2") == 0);
    CHECK(std::fabs(notes[0].m_choices[0].m_weight - 0.25f) < 1e-6f);
    CHECK(std::fabs(notes[0].m_choices[1].m_weight - 0.75f) < 1e-6f);
    CHECK(std::strcmp(notes[1].m_vfxTrigger, "sparkle") == 0);
    CHECK(std::strcmp(notes[1].m_gameEvent, "evt") == 0);
    CHECK(std::strcmp(notes[1].m_gameEventArgument, "arg") == 0);
    CHECK((void*)notes[1].m_emitter == &emitter);
    CHECK((void*)notes[1].m_conditional == &emitter);
    CHECK(std::strcmp(notes[1].m_stop, "step") == 0);
    CHECK(notes[1].m_singleton == 1);
    CHECK(notes[1].m_choices[0].m_weight == 1.0f);
    CHECK(notes[2].m_choiceCount == 0);
    CHECK(notes[2].m_choices == nullptr);

    // Before registering, nothing is found.
    CHECK(audio->FindNote("step", "") == nullptr);
    CHECK(notes[0].m_choices[0].m_segment == nullptr);
    audio->RegisterNotes(bank);
    CHECK((void*)notes[0].m_choices[0].m_segment == &segment1);
    CHECK((void*)notes[0].m_choices[1].m_segment == &segment2);
    CHECK(notes[1].m_choices[0].m_segment == nullptr);
    CHECK(audio->FindNote("step", "") == &notes[0]);
    CHECK(audio->FindNote("foot", "") == nullptr);
    CHECK(audio->FindNoteByTrigger("foot", "") == &notes[0]);
    CHECK(audio->FindNoteByTrigger("talk", "") == &notes[1]);
    CHECK(audio->FindNoteByTrigger("quiet", "") == nullptr);
    CHECK(audio->FindNote("step", "other") == nullptr);
    CHECK(audio->FindSegment("step2") == (AtAudioSegment*)&segment2);
    char name[9] = {0};
    audio->GetSegmentName((AtAudioSegment*)&segment2, name);
    CHECK(std::strcmp(name, "step2") == 0);
    CHECK(bank->m_byVfxTrigger.Find("sparkle") == &notes[1]);

    // Giving it back.
    item.Unknown0();
    CHECK(item.m_bank == nullptr);
    CHECK(item.m_extension[0] == 0);
    CHECK(bank->m_notes == nullptr);
    CHECK(audio->m_noteBankUsed[0] == 0);
    CHECK(audio->FindNote("step", "") == nullptr);

    // Sixty, and no more.
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) CHECK(audio->AllocateNoteBank() == &audio->m_noteBanks[i]);
    CHECK(audio->AllocateNoteBank() == nullptr);
    item.Configure(&request);
    item.Run();
    CHECK(item.m_bank == nullptr);

    if (g_fail == 0) std::printf("test_gon: all passed\n");
    return g_fail ? 1 : 0;
}
