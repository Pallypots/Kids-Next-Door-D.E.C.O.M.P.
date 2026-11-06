// Differential oracle for the volume preset and memory block readers (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_aud <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// Cases: every .GOV and .GOB file listed in index.txt, which tools/jam.py textcases writes; and input written here.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmAudioData.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 20) std::printf("DIFF %s: %s: original [%.200s], recovered [%.200s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

// Logs every request, and refuses the fourth of each run, so a null result is seen.
class Memory : public GmAudioMemory {
public:
    int side = 0;
    int calls = 0;
    int fail_every = 0;
    void Slot0() override {}
    void Slot1() override {}
    void Slot2() override {}
    void Slot3() override {}
    void Slot4() override {}
    void Slot5() override {}
    void Slot6() override {}
    void Slot7() override {}
    void Slot8() override {}
    void Slot9() override {}
    void Slot10() override {}
    void Slot11() override {}
    void Slot12() override {}
    void* Allocate(int where, int size, int id, int unused) override {
        g_log[side].add('A', (uint32_t)where);
        g_log[side].add('z', (uint32_t)size);
        g_log[side].add('i', (uint32_t)id);
        g_log[side].add('u', (uint32_t)unused);
        calls++;
        if (fail_every && calls % fail_every == 0) return nullptr;
        return (void*)(uintptr_t)(0x1000 + calls);
    }
};
static Memory g_memory[2];

typedef void(__fastcall* FnS)(void*, void*, const char*);

static void same(const char* what, const GmAudioData* o, const GmAudioData* r) {
    eq(what, o->m_presetCount, r->m_presetCount);
    eq(what, o->m_blockCount, r->m_blockCount);
    eq(what, o->m_presets != nullptr, r->m_presets != nullptr);
    if (o->m_presetCount == r->m_presetCount && o->m_presets && r->m_presets) {
        for (unsigned i = 0; i < o->m_presetCount; i++) {
            char a[9] = {0}, b[9] = {0};
            std::memcpy(a, o->m_presets[i].m_name, 8);
            std::memcpy(b, r->m_presets[i].m_name, 8);
            eqs("preset name", a, b);
            eq("preset floats", std::memcmp(&o->m_presets[i].m_ambience, &r->m_presets[i].m_ambience, 0x40), 0);
        }
    }
    if (o->m_blockCount == r->m_blockCount) {
        for (int i = 0; i < o->m_blockCount && i < 100; i++) eq("block", std::memcmp(&o->m_blocks[i], &r->m_blocks[i], sizeof(GmAudioBlock)), 0);
    }
}

static int g_presets_seen, g_blocks_seen;

static void run_file(const std::string& name, const std::vector<unsigned char>& data, bool presets) {
    g_context = name;
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    for (int fail : {0, 3}) {
        for (Memory& m : g_memory) { m.calls = 0; m.fail_every = fail; }
        GmAudioData* o = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
        GmAudioData* r = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
        { On on(0); at<FnS>(presets ? 0x001370E0 : 0x00136E70)(o, nullptr, "AUDIO\\FILE"); }
        { On on(1); if (presets) r->LoadVolumePresets("AUDIO\\FILE"); else r->LoadMemoryBlocks("AUDIO\\FILE"); }
        same("after Load", o, r);
        compare_logs("Load");
        g_presets_seen += (int)r->m_presetCount;
        g_blocks_seen += r->m_blockCount;
        std::free(o);
        std::free(r);
    }
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static void run_synthetic() {
    static const char* const presets[] = {
        "VolumePresets [0] { }",
        "VolumePresets [1] { Preset \"A\" { } }",
        "VolumePresets [1] { Preset \"A\" { Ambience 0.5 } }",
        "VolumePresets [1] { Preset \"A\" { SFX 0.5 } }",
        "VolumePresets [1] { Preset \"A\" { Voice 0.5 } }",
        "VolumePresets [1] { Preset \"A\" { Music 0.5 } }",
        "VolumePresets [1] { Preset \"A\" { UI 0.5 } }",
        "VolumePresets [1] { Preset \"A\" { Cinema 0.5 } }",
        "VolumePresets [1] { Preset \"A\" { Cinema 0.1 UI 0.2 Music 0.3 Voice 0.4 SFX 0.6 Ambience 0.7 } }",
        "VolumePresets [1] { Preset \"A\" { other 1 2 Music 0.3 } }",
        "VolumePresets [1] { Preset \"A\" { Music 1 } }",
        "VolumePresets [1] { Preset \"12345678\" { } }",
        "VolumePresets [1] { Preset \"123456789\" { } }",
        "VolumePresets [1] { Preset 5 { } }",
        "VolumePresets [1] { Other \"A\" { } }",
        "VolumePresets [1] { Preset \"A\" Music 0.3 } }",
        "VolumePresets [2] { Preset \"A\" { Music 0.3 } Preset \"B\" { Voice 0.4 } }",
        "VolumePresets [1] { Preset \"A\" { } ]",
        "Other [1] { Preset \"A\" { } }",
        "volumepresets [1] { preset \"a\" { music 0.25 } }",
    };
    for (const char* t : presets) run_file(std::string("presets [") + t + "]", bytes(t), true);
    const std::string body = "BlockDatabase \"Game\" [2] { Block \"A\" { PrimaryRamResident InstanceCount 2 BlockID 7 Size 100 } Block \"B\" { SecondaryRamResident BlockID 8 Size 200 } }";
    const std::string text[] = {
        body,
        "UsePlatformSpecific Section \"XBOX\" " + body,
        "UsePlatformSpecific Section \"xbox\" " + body,
        "UsePlatformSpecific Section \"PSX2\" BlockDatabase \"G\" [1] { Block \"A\" { } } Section \"XBOX\" " + body,
        "UsePlatformSpecific Section \"PSX2\" " + body,
        "UsePlatformSpecific",
        "UsePlatformSpecific Section \"XBOXXBOXX\" " + body,
        "UsePlatformSpecific Section 5 Section \"XBOX\" " + body,
        "UsePlatformSpecific other Section \"XBOX\" " + body,
        "BlockDatabase \"G\" [0] { }",
        "BlockDatabase \"G\" [1] { Block \"A\" { } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { SoundRamResident } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { PrimaryRamResident SoundRamResident } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { SoundRamResident SecondaryRamResident } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { InstanceCount 0 } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { InstanceCount 7 Size 5 } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { other 1 Size 5 } }",
        "BlockDatabase \"G\" [1] { Block \"A\" { Size x } }",
        "BlockDatabase \"G\" [1] { Block 5 { } }",
        "BlockDatabase 5 [1] { Block \"A\" { } }",
        "BlockDatabase \"G\" 1 { Block \"A\" { } } } {",
        "Other \"G\" [1] { Block \"A\" { } }",
        "BlockDatabase \"G\" [1] { Other \"A\" { } }",
        "BlockDatabase \"G\" [1] { Block \"A\" Size 5 } }",
    };
    for (const std::string& t : text) run_file("blocks [" + t + "]", bytes(t), false);
    // Files that will not open: nothing at all.
    g_context = "no file";
    g_fs->has = false;
    GmAudioData* o = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
    GmAudioData* r = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
    { On on(0); at<FnS>(0x001370E0)(o, nullptr, "GONE"); at<FnS>(0x00136E70)(o, nullptr, "GONE"); }
    { On on(1); r->LoadVolumePresets("GONE"); r->LoadMemoryBlocks("GONE"); }
    compare_logs("no file");
    same("no file", o, r);
    g_fs->has = true;
    // No heap.
    g_context = "no heap";
    *(void**)(uintptr_t)0x003B3548 = nullptr;
    GmAudioData::s_heap = nullptr;
    run_file("no heap", bytes("VolumePresets [1] { Preset \"A\" { } }"), true);
    *(void**)(uintptr_t)0x003B3548 = &g_heap_token;
    GmAudioData::s_heap = &g_heap_token;
    // The element constructor, on dirty memory.
    g_context = "preset constructor";
    unsigned char raw[sizeof(GmVolumePreset)];
    std::memset(raw, 0xCD, sizeof raw);
    at<void(__fastcall*)(void*, void*)>(0x00136DA0)(raw, nullptr);
    unsigned char raw2[sizeof(GmVolumePreset)];
    std::memset(raw2, 0xCD, sizeof raw2);
    new (raw2) GmVolumePreset;
    eq("whole preset", std::memcmp(raw, raw2, sizeof raw), 0);
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_aud <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x003B3548u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmAudioData::s_heap = &g_heap_token;
    g_memory[0].side = 0;
    g_memory[1].side = 1;
    *(GmAudioMemory**)(uintptr_t)0x003B355C = &g_memory[0];
    GmAudioMemory::s_instance = &g_memory[1];
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_synthetic();

    g_presets_seen = g_blocks_seen = 0;
    int files = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        const bool presets = std::strstr(line, ".GOV\t") != nullptr;
        if (!presets && !std::strstr(line, ".GOB\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_file(line, data, presets);
        files++;
    }
    std::fclose(index);
    if (files == 0) { std::printf("no files listed\n"); g_failures++; }
    report_entries();
    std::printf("%s: %d files, %d presets, %d blocks (each file read twice); %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_presets_seen, g_blocks_seen, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
