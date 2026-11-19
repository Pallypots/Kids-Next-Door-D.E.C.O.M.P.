// Host test of playing sound notes, the voice slots, the .GOO, .GOC and .GOI readers and the game's random numbers, through the real text tokenizer and real maps, on any pointer size.
// Not evidence of a byte match. The emitters are doubles that record what they are asked to play: nothing makes a sound here.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioDatabase.h"
#include "at/AtIoTokenizer.h"
#include "at/AtNameTableOwner.h"
#include "gm/GmApp.h"
#include "gm/GmAppSettings.h"
#include "gm/GmAudioData.h"
#include "gm/GmRandom.h"

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

// What it was asked to play, and how often it was asked to stop.
class Emitter : public AtAudioEmitter {
public:
    std::vector<AtAudioSegment*> m_played;
    int m_updates = 0, m_stops = 0;
    void InitPlatform() override {}
    void Play(AtAudioSegment* segment, AtAudioVoice**) override { m_played.push_back(segment); }
    void Unknown21() override { m_updates++; }
    void StopAll() override { m_stops++; }
};

class Segment : public AtAudioSegment {
public:
    void OnRead() override {}
};

int g_heap_token;

}  // namespace

int main() {
    // The twister, from its default seed: values from a model of the original's code (analysis-local/scratch/gmsound/mt.py).
    GmRandom* random = new GmRandom;
    CHECK(random->m_index == GmRandom::kCount + 1);
    CHECK(random->SeedDefault() == 0);
    CHECK(random->m_seed == 4357);
    CHECK(random->Next() == 0xD13C8AF5u);
    CHECK(random->Next() == 0xFFC27482u);
    CHECK(random->NextBelow(0) == 0);
    CHECK(random->NextBelow(10000) == 0x82A6958Bu % 10000);
    CHECK(random->SeedDefaultReturningSeed() == 4357);
    CHECK(std::fabs(random->NextUnit() - 0.5877f) < 1e-6f);

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

    tree.m_files["AUDIO\\LOC.GOO"] =
        "SoundNodes[2]\n{\n Node \"clock\"\n {\n  Segment \"tick\"\n  MaxDistanceFromListener 50.0\n  MinDistanceFromListener 1.0\n  CustomParm \"velocity\" 0.5 4.0\n"
        "  AnalogInputParm \"a\" \"b\" \"c\" 1.5 2.5\n  PauseWhenAirborne\n  ActorEmitter\n }\n Node \"door\" { Actor \"dooract\" Emitter \"sfx\" TriggeredOnly }\n}\n";
    tree.m_files["AUDIO\\HIT.GOC"] =
        "CollisionSoundNodes[1]\n{\n CollisionSoundNode \"crate\"\n {\n  DedicatedEmitter\n  DefaultSegment \"soft\" \"mid\" \"hard\"\n  MaterialSegments[2] { \"wood\" \"thud\" \"metal\" \"clang\" }\n"
        "  RagdollOnly\n  ImpulseThresholds 1.0 2.0 3.0\n }\n}\n";
    tree.m_files["AUDIO\\INST.GOI"] = "InstanceSoundNodes[1]\n{\n InstanceSoundNode \"crate\" { Instances[2] { \"crate01\" \"crate02\" } }\n}\n";

    Emitter sfx, actors[3];
    GmNameMap emitters;
    emitters.Init(8);
    emitters.Insert("sfx", &sfx);
    emitters.Insert("actor1", &actors[0]);
    emitters.Insert("actor2", &actors[1]);
    emitters.Insert("actor3", &actors[2]);
    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_nodes.Add(&emitters);
    GmAudioData::s_soundOwner = owner;

    Segment tick, tock;
    Database database;
    database.m_segmentCount = 1;
    database.m_list.Init(4);
    database.m_list.Insert("tick", &tick);
    database.m_list.Insert("tock", &tock);

    GmAudioData* audio = new GmAudioData;
    std::memset(audio, 0, sizeof *audio);
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) new (&audio->m_noteBanks[i]) GmSoundNoteBank;
    for (int i = 0; i < GmAudioData::kVoiceSlots; i++) audio->m_voiceSlots[i].m_state = GmSoundVoiceSlot::kFree;
    audio->m_databases[5] = &database;
    audio->m_impulseThresholds[0] = 9.0f;
    GmAudioData::s_instance = audio;

    App* app = (App*)std::calloc(1, sizeof(App));  // only its audio words are read
    App::s_gmApp = app;
    GmHistoryLogs logs = {};
    logs.m_random = random;
    GmHistoryLogs::s_instance = &logs;

    // The emitters by name, and the voice slots they make.
    audio->FindEmitters();
    CHECK(audio->m_sfx == &sfx);
    CHECK(audio->m_ui == nullptr);
    CHECK(audio->m_voiceSlots[0].m_emitter == &actors[0]);
    CHECK(audio->m_voiceSlots[2].m_emitter == &actors[2]);
    CHECK(audio->m_voiceSlots[3].m_emitter == nullptr);
    // Round from the last taken, which is never itself taken.
    CHECK(audio->TakeVoiceSlot() == &actors[1]);
    CHECK(audio->m_voiceSlots[0].m_state == GmSoundVoiceSlot::kFree);
    CHECK(audio->m_voiceSlots[1].m_state == GmSoundVoiceSlot::kNode);
    CHECK(actors[1].m_updates == 1);
    CHECK(audio->TakeVoiceSlot() == &actors[2]);
    CHECK(audio->m_lastVoiceSlot == 2);
    audio->ReleaseVoiceSlot(&actors[1]);
    CHECK(actors[1].m_stops == 1);
    CHECK(audio->m_voiceSlots[1].m_state == GmSoundVoiceSlot::kFree);
    audio->ReleaseVoiceSlot(&actors[2]);

    // Nodes: a bank, a load, the segments found.
    GmSoundNodeBank* nodes = audio->AllocateNodeBank();
    CHECK(nodes == &audio->m_nodeBanks[0]);
    audio->LoadNodes(nodes, "AUDIO\\LOC");
    CHECK(nodes->m_count == 2);
    const GmSoundNode* clock = &nodes->m_nodes[0];
    CHECK(std::strcmp(clock->m_name, "clock") == 0);
    CHECK(std::strcmp(clock->m_actor, "clock") == 0);
    CHECK(std::strcmp(clock->m_segmentName, "tick") == 0);
    CHECK(std::strcmp(clock->m_customParm, "velocity") == 0);
    CHECK(clock->m_customFirst == 0.5f && clock->m_customSecond == 4.0f);
    CHECK(std::strcmp(clock->m_analogInputParm, "a") == 0 && std::strcmp(clock->m_analogInputSecondName, "b") == 0 && std::strcmp(clock->m_analogInputThird, "c") == 0);
    CHECK(clock->m_analogInputFirst == 1.5f && clock->m_analogInputSecond == 2.5f);
    CHECK(clock->m_pauseWhenAirborne == 1);
    CHECK(clock->m_range == 0);
    const GmSoundNode* door = &nodes->m_nodes[1];
    CHECK(std::strcmp(door->m_actor, "dooract") == 0);
    CHECK(door->m_emitter == &sfx);
    CHECK(door->m_triggeredOnly == 1);
    audio->RegisterNodes(nodes);
    CHECK(clock->m_segment == &tick);
    CHECK(door->m_segment == nullptr);
    // A dynamic node is a node of the first bank with no owner.
    AtAudioEmitter* doorEmitter = door->m_emitter;
    audio->FreeNodes(nodes);
    CHECK(nodes->m_nodes == nullptr && nodes->m_count == 0);
    CHECK(((Emitter*)doorEmitter)->m_stops == 1);
    audio->ReleaseNodeBank(nodes);
    CHECK(nodes->m_used == 0);

    // Collision and instance sounds.
    GmCollisionSoundBank* collisions = audio->LoadCollisionSounds("AUDIO\\HIT");
    CHECK(collisions == &audio->m_collisionBanks[0]);
    CHECK(collisions->m_count == 1);
    const GmCollisionSound* crate = &collisions->m_sounds[0];
    CHECK(std::strcmp(crate->m_name, "crate") == 0);
    CHECK(crate->m_emitter == &actors[0]);  // round from slot 2, past the empty ones
    CHECK(std::strcmp(crate->m_defaultSegments[2], "hard") == 0);
    CHECK(crate->m_materialCount == 2);
    CHECK(std::strcmp(crate->m_materials[1].m_segment, "clang") == 0);
    CHECK(crate->m_ragdollOnly == 1 && crate->m_playerInitiatedOnly == 0);
    CHECK(crate->m_impulseThresholds[0] == 1.0f && crate->m_impulseThresholds[2] == 3.0f);
    GmInstanceSoundBank* instances = audio->LoadInstanceSounds("AUDIO\\INST");
    CHECK(instances == &audio->m_instanceBanks[0]);
    CHECK(instances->m_count == 1);
    CHECK(instances->m_sounds[0].m_instanceCount == 2);
    CHECK(std::strcmp(instances->m_sounds[0].m_instances[1], "crate02") == 0);
    audio->RegisterInstanceSounds(instances);
    CHECK(instances->m_sounds[0].m_collision == crate);
    audio->FreeInstanceSounds(instances);
    CHECK(instances->m_used == 0 && instances->m_sounds == nullptr);
    audio->FreeCollisionSounds(collisions);
    CHECK(collisions->m_used == 0 && collisions->m_sounds == nullptr);

    // A note on a global emitter, chosen by weight: with the default seed the first pick is 0.5877.
    GmSoundChoice choices[3];
    choices[0].m_segment = &tick;
    choices[0].m_weight = 0.5f;
    choices[1].m_segment = &tock;
    choices[1].m_weight = 0.5f;
    choices[2].m_weight = 0.0f;
    GmSoundNote note;
    note.m_emitter = &sfx;
    note.m_choiceCount = 2;
    note.m_choices = choices;
    random->SeedDefault();
    sfx.m_played.clear();
    audio->PlayNote(nullptr, &note);
    CHECK(sfx.m_played.size() == 1 && sfx.m_played[0] == &tock);
    // NoAudio: nothing.
    app->m_noAudio = 1;
    audio->PlayNote(nullptr, &note);
    CHECK(sfx.m_played.size() == 1);
    app->m_noAudio = 0;
    // By name.
    audio->PlayByName("sfx", "tick");
    CHECK(sfx.m_played.size() == 2 && sfx.m_played[1] == &tick);

    if (g_fail == 0) std::printf("test_gmsound: all passed\n");
    return g_fail ? 1 : 0;
}
