// Host test of the sound bank and segment lists through the real text tokenizer, on any pointer size. Not evidence of a byte match.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "at/AtAudioBank.h"
#include "at/AtIoTokenizer.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class Tok {
public:
    explicit Tok(const char* text) : m_text(text) {
        const unsigned size = (unsigned)std::strlen(text);
        m_stream.m_base = (char*)m_text;
        m_stream.m_cur = m_stream.m_base;
        m_stream.m_limit = m_stream.m_base + size;
        m_stream.m_size = size;
        m_tokenizer.SetStream(&m_stream);
    }
    ~Tok() {
        m_stream.m_base = nullptr;
        m_stream.m_cur = nullptr;
        m_stream.m_limit = nullptr;
        m_tokenizer.SetStream(nullptr);
    }
    const char* m_text;
    Memory m_stream;
    AtIoTextTokenizer m_tokenizer;
};

int g_inits, g_processes, g_deletes;
float g_length;

class Bank : public AtAudioBank {
public:
    ~Bank() override { g_deletes++; }
    void OnInit(const AtAudioBankDesc& desc) override { g_inits++; m_flagsSeen = desc.m_flags; }
    void Unknown3() override {}
    void Unknown4() override {}
    void Unknown5() override {}
    void Unknown6() override {}
    void Unknown7() override {}
    void Unknown8() override {}
    void Unknown9() override {}
    void Process() override { g_processes++; }
    float SegmentLength(int) override { return g_length; }
    int m_flagsSeen = 0;
};

class BankDb : public AtAudioBankDatabase {
public:
    void CreateBank(unsigned int index, const AtAudioBankDesc& desc) override {
        Bank* bank = new Bank;
        bank->Init(desc);
        m_banks[index] = bank;
    }
    void SetDirectory(const char* directory) override { std::strcpy(m_directory, directory); }
    char m_directory[64] = {};
};

class Segment : public AtAudioSegment {
public:
    void OnRead() override { m_reads++; }
    int m_reads = 0;
};

class SegmentDb : public AtAudioSegmentDatabase {
public:
    void OnReady() override { m_ready++; }
    void Unknown3(void*) override {}
    AtAudioSegment* SegmentAt(unsigned int index) override { return &m_items[index]; }
    Segment m_items[4];
    int m_ready = 0;
};

class Loader : public AtAudioBankLoader {
public:
    void Unknown0() override {}
    void Unknown1() override {}
    void Add(AtAudioBank* bank) override { m_added = bank; }
    void Remove(AtAudioBank* bank) override { m_removed = bank; }
    AtAudioBank* m_added = nullptr;
    AtAudioBank* m_removed = nullptr;
};

int g_heap;

}  // namespace

int main() {
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::SetHeap(&g_heap);
    AtAudioBankDatabase::SetHeap(&g_heap);
    AtIoStream::s_heap = &g_heap;

    AtNameTableOwner* owner = new AtNameTableOwner;
    Loader loader;
    owner->m_loader = &loader;
    owner->m_firstSpaces.Init(4);
    owner->m_secondSpaces.Init(4);

    // Every word on one bank.
    BankDb banks;
    {
        Tok t(
            "// a comment\n"
            "Bank[3]\n{\n"
            " Bank \"Music\"\n {\n  PrimaryRamResident GmsFormat BlockID 12 DelayLoad Streaming\n }\n"
            " Bank \"Voice\"\n {\n  PlatformSource StreamedStereo\n }\n"
            " Bank \"Music\"\n {\n  SbaFormat\n }\n"
            "}\n"
            "Bank[1]\n{\n Bank \"NotRead\"\n {\n }\n}\n");
        banks.Read(owner, &t.m_tokenizer, "AUDIO\\HERE");
    }
    CHECK(std::strcmp(banks.m_directory, "AUDIO\\HERE") == 0);
    CHECK(banks.m_count == 3);
    CHECK(g_inits == 2);
    Bank* music = (Bank*)banks.m_banks[0];
    Bank* voice = (Bank*)banks.m_banks[1];
    CHECK(banks.m_banks[2] == music);  // A name already known is shared.
    CHECK(music->UserCount() == 2);
    CHECK(voice->UserCount() == 1);
    CHECK(std::strncmp(music->m_name, "Music", 8) == 0);
    CHECK(music->m_format == AtAudioBankDesc::kGms);
    CHECK(music->m_residence == AtAudioBankDesc::kPrimaryRam);
    CHECK(music->m_blockId == 12);
    CHECK(music->m_flagsSeen == (AtAudioBankDesc::kDelayLoad | AtAudioBankDesc::kStreaming));
    CHECK(music->m_state == AtAudioBank::kInitialised);
    CHECK(voice->m_format == AtAudioBankDesc::kWba);
    CHECK(voice->m_blockId == 0xFFFF);
    CHECK(banks.Find("Voice") == voice);
    CHECK(banks.Find("NotRead") == nullptr);
    CHECK(owner->m_first.m_first == &banks);
    CHECK(owner->FindInFirst("Voice", "") == voice);
    CHECK(owner->FindInFirst("Voice", nullptr) == voice);

    // Going through the loader, by way of the first user's owner.
    CHECK(voice->Load() == 0);
    CHECK(loader.m_added == voice);
    CHECK(voice->m_state == (AtAudioBank::kInitialised | AtAudioBank::kLoadRequested));
    voice->Update();
    CHECK(g_processes == 1);
    CHECK(voice->Unload() == 7);
    CHECK(loader.m_removed == nullptr);
    voice->m_state |= AtAudioBank::kLoaded;
    CHECK(voice->Unload() == 0);
    CHECK(loader.m_removed == voice);
    CHECK(voice->m_state == (AtAudioBank::kInitialised | AtAudioBank::kLoaded));
    voice->Update();
    CHECK(g_processes == 1);
    char name[9] = {}, space[9] = {'x'};
    voice->GetNames(name, space);
    CHECK(std::strcmp(name, "Voice") == 0);
    CHECK(space[0] == 0);

    // Users, in full.
    Bank* full = new Bank;
    for (int i = 0; i < 24; i++) {
        CHECK(full->AddUser(&banks) == (i < 20 ? i + 1 : 20));
    }
    CHECK(full->RemoveUser(nullptr) == 20);
    CHECK(full->RemoveUser(&banks) == 19);
    delete full;
    CHECK(g_deletes == 1);

    // Segments on those banks.
    SegmentDb segments;
    {
        Tok t(
            "NameSpace \"Sounds\"\n"
            "Segment[3]\n{\n"
            " Segment \"one\"\n {\n  Bank \"Voice\" \"\" 7\n }\n"
            " Segment \"two\"\n {\n  Looping\n  Bank \"Music\" \"\" 1\n  Bank \"Gone\" \"\" 2\n }\n"
            " Segment \"three\"\n {\n  Bank \"Music\" \"\" 3\n  Looping\n }\n"
            "}\n");
        segments.Read(owner, &t.m_tokenizer);
    }
    CHECK(segments.m_ready == 1);
    CHECK(segments.m_count == 3);
    CHECK(segments.m_items[0].m_bank == voice);
    CHECK(segments.m_items[0].m_index == 7);
    CHECK(segments.m_items[0].m_looping == 0);
    CHECK(segments.m_items[0].m_reads == 1);
    CHECK(segments.m_items[1].m_bank == nullptr);  // The later Bank replaces the earlier one, even when not found.
    CHECK(segments.m_items[1].m_index == 2);
    CHECK(segments.m_items[1].m_looping == 1);
    CHECK(segments.Find("three") == &segments.m_items[2]);
    CHECK(segments.m_group != nullptr);
    CHECK(owner->m_secondSpaces.Find("Sounds", "two") == &segments.m_items[1]);

    // Lengths, and the loop.
    g_length = 2.5f;
    CHECK(segments.m_items[0].Length() == 2.5f);
    CHECK(segments.m_items[2].Length() == 2.5f);
    g_length = 0.0f;
    CHECK(segments.m_items[0].Length() == 0.0f);
    CHECK(segments.m_items[2].Length() > 3.4e38f);
    CHECK(segments.m_items[1].Length() > 3.4e38f);
    CHECK(segments.m_items[1].Unknown31BF0() == 0.0f);
    segments.m_items[1].m_looping = 0;
    CHECK(segments.m_items[1].Length() == 0.0f);
    segments.m_items[0].Reset();
    CHECK(segments.m_items[0].m_bank == nullptr);
    segments.Clear();
    CHECK(segments.m_count == 0);
    CHECK(segments.m_owner == nullptr);
    CHECK(segments.m_entries == nullptr);

    // Let go of: Music is listed twice with two users, so the first entry takes one user off and the third destroys it. Voice has one user. Two deletes.
    const int before = g_deletes;
    banks.Clear();
    CHECK(g_deletes == before + 2);
    CHECK(banks.m_banks == nullptr);
    CHECK(banks.m_count == 0);
    CHECK(banks.m_entries == nullptr);

    if (g_fail) {
        std::printf("%d failures\n", g_fail);
        return 1;
    }
    std::printf("test_aob: all checks passed\n");
    return 0;
}
