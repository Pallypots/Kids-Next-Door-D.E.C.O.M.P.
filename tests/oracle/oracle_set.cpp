// Differential oracle for the reader of App.ini (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_set <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// The cases directory is not used; App.ini is short enough to be a literal here: it is 28 bytes on the disc and is one of the inputs below.
#include <windows.h>

#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmAppSettings.h"
#include "oracle_guard.h"
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
static int g_language, g_region;

int AtPlatformLanguage() { return g_language; }
int AtPlatformGameRegion() { return g_region; }
static int __cdecl o_language() { return g_language; }
static int __cdecl o_region() { return g_region; }

class HistoryDouble : public GmHistoryLog {
public:
    int side = 0;
    int id = 0;
    void Unknown0() override {}
    void Unknown1() override {}
    void Unknown2() override {}
    void Unknown3() override {}
    void Unknown4() override {}
    void SetLength(int lines) override { g_log[side].add(id ? 'H' : 'h', (uint32_t)lines); }
};

static HistoryDouble g_logs[2][2];
static GmHistoryLogs g_histories[2];
static GmAppAudioOptions g_audio[2];
static AtIoMountManager* g_mount[2];

static const uint32_t kChannels = 0x002B93A4, kPlatformBinary = 0x002B9348, kBgMusic = 0x002B9110, kMemStats = 0x002B93A0, kSkipProfile = 0x003ADA5C, kUnlockables = 0x003ADA60, kNoDialogs = 0x003B3908, kFlashHeap = 0x003B3544;

static void reset_globals() {
    *(uint32_t*)(uintptr_t)kChannels = 0x00010003;  // The high word is another variable's: a word write must leave it.
    g_debugChannels = 3;
    *(int*)(uintptr_t)kPlatformBinary = g_usePlatformBinary = 1;
    *(int*)(uintptr_t)kBgMusic = g_backgroundMusic = 1;
    *(int*)(uintptr_t)kMemStats = g_consoleMemStats = 1;
    *(int*)(uintptr_t)kSkipProfile = g_skipProfileLoad = 0;
    *(int*)(uintptr_t)kUnlockables = g_enableAllUnlockables = 0;
    *(int*)(uintptr_t)kNoDialogs = g_noDialogs = 0;
    *(int*)(uintptr_t)kFlashHeap = g_showFlashHeapInfo = 0;
    for (GmAppAudioOptions& a : g_audio) std::memset(&a.m_noMusic, 0, sizeof(int) * 8);
}

static void same_globals(const char* what) {
    eq(what, *(uint16_t*)(uintptr_t)kChannels, g_debugChannels);
    eq("next variable untouched", *(uint16_t*)(uintptr_t)(kChannels + 2), 1);
    eq(what, *(int*)(uintptr_t)kPlatformBinary, g_usePlatformBinary);
    eq(what, *(int*)(uintptr_t)kBgMusic, g_backgroundMusic);
    eq(what, *(int*)(uintptr_t)kMemStats, g_consoleMemStats);
    eq(what, *(int*)(uintptr_t)kSkipProfile, g_skipProfileLoad);
    eq(what, *(int*)(uintptr_t)kUnlockables, g_enableAllUnlockables);
    eq(what, *(int*)(uintptr_t)kNoDialogs, g_noDialogs);
    eq(what, *(int*)(uintptr_t)kFlashHeap, g_showFlashHeapInfo);
    eq(what, std::memcmp(&g_audio[0].m_noMusic, &g_audio[1].m_noMusic, sizeof(int) * 8), 0);
}

static void same_settings(const char* what, const GmAppSettings* o, const GmAppSettings* r) {
    // Names are cut at their terminator: an eight-byte copy takes stack bytes along after a shorter one.
    const auto name = [](const char* n, char* out) { std::memcpy(out, n, 8); out[8] = 0; };
    char a[9], b[9];
    eq(what, o->m_ready, r->m_ready);
    name(o->m_level, a); name(r->m_level, b); eqs("level", a, b);
    name(o->m_dataFolder, a); name(r->m_dataFolder, b); eqs("data folder", a, b);
    name(o->m_language, a); name(r->m_language, b); eqs("language", a, b);
    name(o->m_character, a); name(r->m_character, b); eqs("character", a, b);
    eq("width", o->m_width, r->m_width);
    eq("height", o->m_height, r->m_height);
    eq("fullscreen", o->m_fullscreen, r->m_fullscreen);
    eq("unknown20", o->m_unknown20, r->m_unknown20);
    eq("test all levels", o->m_testAllLevels, r->m_testAllLevels);
    eq("open list", o->m_openList, r->m_openList);
    eq("vertical sync", o->m_verticalSyncOff, r->m_verticalSyncOff);
    eq("dead zone", *(const uint32_t*)&o->m_controllerDeadZone, *(const uint32_t*)&r->m_controllerDeadZone);
    eq("controllers", std::memcmp(o->m_activeControllers, r->m_activeControllers, sizeof o->m_activeControllers), 0);
    eq("skip intro", o->m_skipIntro, r->m_skipIntro);
    eq("skip main menu", o->m_skipMainMenu, r->m_skipMainMenu);
    eq("debug menu", o->m_enableDebugMenu, r->m_enableDebugMenu);
    eq("skip legal", o->m_skipLegal, r->m_skipLegal);
    eq("mem stats", o->m_memStats, r->m_memStats);
}

typedef void(__fastcall* Fn0)(void*, void*);

static void new_settings(GmAppSettings*& o, GmAppSettings*& r) {
    // Dirty, so a field the constructor must set shows if it does not. What it leaves alone stays equal on both sides. Channels too.
    *(uint16_t*)(uintptr_t)kChannels = 0x1234;
    g_debugChannels = 0x1234;
    o = (GmAppSettings*)std::malloc(sizeof(GmAppSettings) + 0x40);
    std::memset(o, 0xCD, sizeof(GmAppSettings) + 0x40);
    { On on(0); at<Fn0>(0x000989D0)(o, nullptr); }
    r = (GmAppSettings*)std::malloc(sizeof(GmAppSettings) + 0x40);
    std::memset(r, 0xCD, sizeof(GmAppSettings) + 0x40);
    { On on(1); new (r) GmAppSettings; }
}

static void run_load(const std::string& name, const std::string& text) {
    g_context = name;
    reset_globals();
    GmAppSettings* o, *r;
    new_settings(o, r);
    eq("channels after construction", *(uint16_t*)(uintptr_t)kChannels, g_debugChannels);
    same_settings("constructed", o, r);
    compare_logs("constructed");
    const std::vector<unsigned char> data(text.begin(), text.end());
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    { On on(0); at<Fn0>(0x00098A50)(o, nullptr); }
    { On on(1); r->Load(); }
    same_settings("after Load", o, r);
    same_globals("after Load");
    compare_logs("Load");
    std::free(o);
    std::free(r);
}

static void run_inputs() {
    static const char* const keys[] = {
        "Level \"ABC\"", "Level \"12345678\"", "Level \"123456789\"", "Level 5", "Level \"\"",
        "Resolution 100 200", "Resolution -1 0", "Resolution x 2", "Resolution 1 x", "Resolution 1.5 2",
        "Fullscreen", "Maximized", "NTSCScreen", "PALScreen", "PS2Screen", "XBScreen", "XB480pScreen", "XB720pScreen", "XB1080iScreen",
        "Fullscreen PALScreen", "PALScreen Fullscreen", "Fullscreen XBScreen", "Fullscreen NTSCScreen", "Fullscreen XB720pScreen", "Fullscreen XB1080iScreen", "Fullscreen PS2Screen",
        "TestAllLevels", "OpenList", "VerticalSyncOff",
        "ControllerDeadZone 0", "ControllerDeadZone 50", "ControllerDeadZone 99", "ControllerDeadZone 100", "ControllerDeadZone -1", "ControllerDeadZone x",
        "TraceHistory 0", "TraceHistory 999", "TraceHistory 1000", "TraceHistory -1", "TraceHistory x",
        "ErrorHistory 0", "ErrorHistory 999", "ErrorHistory 1000", "ErrorHistory -1", "ErrorHistory x",
        "add_dio \"framework\"", "add_dio \"ai\"", "add_dio \"audio\"", "add_dio \"io\"", "add_dio \"render\"", "add_dio \"sim\"", "add_dio \"ui\"", "add_dio \"controller\"", "add_dio \"tool\"",
        "add_dio \"debugoutput\"", "add_dio \"event\"", "add_dio \"innerloop\"", "add_dio \"FRAMEWORK\"", "add_dio \"Sim\"", "add_dio \"nothing\"", "add_dio \"\"", "add_dio 5",
        "add_dio \"framework\" add_dio \"sim\" add_dio \"innerloop\"", "add_dio \"zframework\"", "add_dio \"frameworkz\"",
        "no_dio \"framework\"", "no_dio \"ai\"", "no_dio \"audio\"", "no_dio \"io\"", "no_dio \"render\"", "no_dio \"sim\"", "no_dio \"ui\"", "no_dio \"controller\"", "no_dio \"tool\"",
        "no_dio \"debugoutput\"", "no_dio \"event\"", "no_dio \"innerloop\"", "no_dio \"nothing\"", "no_dio 5",
        "add_dio \"framework\" no_dio \"framework\"", "add_dio \"audio\" add_dio \"io\" no_dio \"audio\"",
        "DontUsePlatformBinary", "BgMusicDisabled", "Language \"FRENCH\"", "Language \"ABCDEFGH\"", "Language \"ABCDEFGHI\"", "Language 5", "Page", "Profile", "Yellow", "Red",
        "ActivateController 0", "ActivateController 1", "ActivateController 3", "ActivateController x",
        "SetCharacter 1 \"12345678\"", "SetCharacter 2 \"ABCDEFGH\"", "SetCharacter 0 \"NUMBUH\"", "SetCharacter 0 \"12345678\"", "SetCharacter 0 \"123456789\"", "SetCharacter 0 5", "SetCharacter x \"A\"",
        "SkipIntro", "DisableConsoleMemStats", "SkipMainMenu", "EnableDebugMenu", "SkipLegal", "SkipProfileLoadCreate",
        "NoMusic", "NoAmbientAudio", "NoAudio", "DisplayBanks", "DisplaySounds", "DisplayMusic", "AudioTest",
        "AudioMode 0", "AudioMode 1", "AudioMode 2", "AudioMode 3", "AudioMode -1", "AudioMode x",
        "NoDialogs", "ShowFlashHeapInfo", "EnableAllUnlockables", "unknown 1 2 \"x\"", "noaudio", "SKIPLEGAL",
        "DataFolder \"DataXbox\"", "DataFolder \"12345678\"", "DataFolder \"123456789\"", "DataFolder 5",
        "SkipIntro SkipLegal NoAudio Level \"X\" Resolution 3 4 add_dio \"ui\"",
    };
    for (const char* k : keys) run_load(std::string("key [") + k + "]", std::string("{ ") + k + " }");
    // The file on the disc, as it is there.
    run_load("app.ini", "{\r\n\tDataFolder \"DataXbox\"\r\n}");
    run_load("no opening brace", "SkipIntro } ");
    run_load("empty block", "{ }");
    run_load("comment", "// c\r\n{ // d\r\n SkipIntro // e\r\n}");
    // Long strings for Level: a local buffer of 0x80 is cut before the length check.
    run_load("long level", "{ Level \"" + std::string(0x100, 'L') + "\" }");
    run_load("level at the buffer", "{ Level \"" + std::string(0x7F, 'L') + "\" }");
}

static void run_parts() {
    g_context = "language";
    for (int region : {0, 1, 2}) {
        for (int language = 0; language < 10; language++) {
            g_language = language;
            g_region = region;
            g_context = "language " + std::to_string(language) + " region " + std::to_string(region);
            GmAppSettings* o, *r;
            new_settings(o, r);
            same_settings("constructed", o, r);
            compare_logs("constructed");
            std::memset(o->m_language, 'q', 8);
            std::memset(r->m_language, 'q', 8);
            at<Fn0>(0x00098900)(o, nullptr);
            r->SetDefaultLanguage();
            same_settings("default language", o, r);
            std::free(o);
            std::free(r);
        }
    }
    g_language = 1;
    g_region = 1;

    g_context = "channels, directly";
    typedef void(__fastcall* FnChan)(void*, void*, const char*, int);
    typedef void(__cdecl* FnSet)(unsigned);
    for (const char* n : {"framework", "ai", "audio", "io", "render", "sim", "ui", "controller", "tool", "debugoutput", "event", "innerloop", "Tool", "x", ""}) {
        for (int value : {1, 0, 2}) {
            *(uint32_t*)(uintptr_t)kChannels = 0x0001AAAA;
            g_debugChannels = 0xAAAA;
            at<FnChan>(0x000981C0)(nullptr, nullptr, n, value);
            GmAppSettings::SetChannel(n, value);
            eq("channels", *(uint16_t*)(uintptr_t)kChannels, g_debugChannels);
            *(uint32_t*)(uintptr_t)kChannels = 0x00015555;
            g_debugChannels = 0x5555;
            at<FnChan>(0x000981C0)(nullptr, nullptr, n, value);
            GmAppSettings::SetChannel(n, value);
            eq("channels", *(uint16_t*)(uintptr_t)kChannels, g_debugChannels);
        }
    }
    at<FnChan>(0x000981C0)(nullptr, nullptr, nullptr, 1);
    GmAppSettings::SetChannel(nullptr, 1);
    eq("no name", *(uint16_t*)(uintptr_t)kChannels, g_debugChannels);
    at<FnSet>(0x0015DA00)(0x1234);
    GmSetDebugChannels(0x1234);
    eq("set", *(uint16_t*)(uintptr_t)kChannels, g_debugChannels);
    eq("next variable untouched", *(uint16_t*)(uintptr_t)(kChannels + 2), 1);

    g_context = "history";
    typedef void(__fastcall* FnHist)(void*, void*, int);
    for (int present = 0; present < 2; present++) {
        GmHistoryLogs a = {}, b = {};
        if (present) {
            a.m_errors = &g_logs[0][0];
            a.m_traces = &g_logs[0][1];
            b.m_errors = &g_logs[1][0];
            b.m_traces = &g_logs[1][1];
        }
        { On on(0); at<FnHist>(0x0006C2D0)(&a, nullptr, 7); at<FnHist>(0x0006C2F0)(&a, nullptr, 8); }
        { On on(1); b.SetTraceHistory(7); b.SetErrorHistory(8); }
        compare_logs("history");
    }
}

// ---- no App.ini, and one that will not open ------------------------------------------------------------------------------

static __declspec(noinline) void load_expecting_crash(GmAppSettings* s, int side) {
    guarded([&]() {
        g_side = side;
        g_recording = true;
        if (side) s->Load();
        else at<Fn0>(0x00098A50)(s, nullptr);
    });
    g_recording = false;
    g_side = 0;
}

static void run_missing() {
    g_context = "no file";
    GmAppSettings* o, *r;
    new_settings(o, r);
    compare_logs("constructed");
    g_fs->has = false;
    { On on(0); at<Fn0>(0x00098A50)(o, nullptr); }
    { On on(1); r->Load(); }
    same_settings("no file", o, r);
    compare_logs("no file");
    g_fs->has = true;
    g_context = "file will not open";
    g_fs->open_status = 9;
    const int before = g_crashes;
    load_expecting_crash(o, 0);
    load_expecting_crash(r, 1);
    eq("both stop", g_crashes - before, 2);
    compare_logs("will not open");
    g_fs->open_status = 0;
}

// The original's 128-byte buffer, which nothing writes, lies directly under the return address. So fill it from here, just before the call, to choose what strlen sees.
static __declspec(noinline) void original_command_line(GmAppSettings* self, unsigned int count) {
    __asm {
        mov ecx, count
        lea edi, [esp - 0x84]
        mov al, 0x41
        rep stosb
        mov byte ptr [edi], 0
        mov ecx, self
        xor edx, edx
        mov eax, 0x00098410
        call eax
    }
}

static __declspec(noinline) void command_expecting_crash(GmAppSettings* s, int side, unsigned int count) {
    guarded([&]() {
        g_side = side;
        g_recording = true;
        if (side) s->ReadCommandLine();
        else original_command_line(s, count);
    });
    g_recording = false;
    g_side = 0;
}

// Each side has its own writable copy, and room after each word, in lower case, for an uppercasing that runs past the end.
struct Words {
    enum { kRoom = 200 };  // More than the longest count tried.
    std::vector<std::vector<char>> text;
    std::vector<char*> argv;
    explicit Words(const std::vector<const char*>& words) {
        for (const char* w : words) {
            std::vector<char> t(std::strlen(w) + 1 + kRoom, 'q');
            std::memcpy(t.data(), w, std::strlen(w) + 1);
            text.push_back(t);
        }
        for (auto& t : text) argv.push_back(t.data());
        argv.push_back(nullptr);
    }
};

static void set_words(Words& o, Words& r, size_t count) {
    *(unsigned int*)(uintptr_t)0x002BF308 = (unsigned int)count;
    *(char***)(uintptr_t)0x002BF300 = o.argv.data();
    GmAppSettings::s_argc = (unsigned int)count;
    GmAppSettings::s_argv = r.argv.data();
}

static void run_command(const std::vector<const char*>& words, unsigned int count = 0) {
    g_context = "command line [" + std::to_string(count) + "]";
    for (const char* w : words) { g_context += ' '; g_context += w; }
    reset_globals();
    GmAppSettings* o, *r;
    new_settings(o, r);
    compare_logs("constructed");
    Words wo(words), wr(words);
    set_words(wo, wr, words.size());
    GmAppSettings::s_levelUppercaseCount = count;
    { On on(0); original_command_line(o, count); }
    { On on(1); r->ReadCommandLine(); }
    same_settings("after command line", o, r);
    same_globals("after command line");
    compare_logs("command line");
    for (size_t i = 0; i < words.size(); i++) eq("word after", std::memcmp(wo.text[i].data(), wr.text[i].data(), wo.text[i].size()), 0);
    GmAppSettings::s_levelUppercaseCount = 0;
    std::free(o);
    std::free(r);
}

static void run_command_crash(const std::vector<const char*>& words) {
    g_context = "command line stops:";
    for (const char* w : words) { g_context += ' '; g_context += w; }
    reset_globals();
    GmAppSettings* o, *r;
    new_settings(o, r);
    compare_logs("constructed");
    Words wo(words), wr(words);
    set_words(wo, wr, words.size());
    const int before = g_crashes;
    command_expecting_crash(o, 0, 0);
    command_expecting_crash(r, 1, 0);
    eq("both stop", g_crashes - before, 2);
    same_settings("after stopping", o, r);
    compare_logs("stopping");
    std::free(o);
    std::free(r);
}

static void run_commands() {
    static const char* const flags[] = {
        "fullscreen", "testalllevels", "openlist", "maximized", "verticalsyncoff", "page", "skipintro", "disableconsolememstats", "nomusic", "noambientaudio",
        "noaudio", "displaybanks", "displaysounds", "displaymusic", "audiotest", "skipmainmenu", "mainmenu", "unknownoption", "",
    };
    for (const char* f : flags) {
        run_command({"game", f});
        // What follows must be read as an option, not eaten as a value.
        run_command({"game", f, "fullscreen", "skipmainmenu"});
    }
    run_command({"game"});
    run_command({});
    run_command({"fullscreen"});  // The first word is the program's.
    run_command({"game", "FULLSCREEN", "SkipMainMenu", "NoMuSiC"});
    run_command({"game", "fullscreens"});
    run_command({"game", "fullscree"});
    run_command({"game", "\xE9"});

    // The level: only with a capital L, and eight letters at most.
    for (const char* name : {"LEVEL", "L", "LABCDEFG", "LABCDEFGH", "lower", "ALMOND", "Lmixed", "", "fullscreen"}) {
        run_command({"game", "level", name});
        run_command({"game", "LEVEL", name, "skipmainmenu"});
    }
    // What the original's buffer does, with the count chosen from outside: the first letters only, and on past the end of the word.
    for (unsigned int count : {1u, 2u, 3u, 7u, 8u, 9u, 30u, 127u}) {
        run_command({"game", "level", "lower", "skipmainmenu"}, count);
        run_command({"game", "level", "LOWER"}, count);
        run_command({"game", "level", "l!z{a`"}, count);
    }

    for (const char* v : {"0", "1", "50", "99", "100", "-1", "abc", "", " 7", "+7", "7x", "2147483647", "2147483648", "4294967346"}) {
        run_command({"game", "controllerdeadzone", v});
        run_command({"game", "controllerdeadzone", v, "fullscreen"});
        run_command({"game", "audiomode", v});
        run_command({"game", "audiomode", v, "fullscreen"});
    }
    run_command({"game", "audiomode", "2"});
    run_command({"game", "audiomode", "3"});
    run_command({"game", "audiomode", "1", "audiomode", "9"});
    run_command({"game", "audiomode", "1", "audiomode", "0"});  // Zero is a value, not the default showing through.
    run_command({"game", "controllerdeadzone", "5", "controllerdeadzone", "0"});
    // A value that is itself an option's name is still only a value.
    run_command({"game", "controllerdeadzone", "fullscreen"});
    run_command({"game", "audiomode", "fullscreen"});
    run_command({"game", "add_dio", "fullscreen"});
    run_command({"game", "no_dio", "fullscreen"});
    run_command({"game", "level", "fullscreen"});

    static const char* const channels[] = {"framework", "ai", "audio", "io", "render", "sim", "ui", "controller", "tool", "debugoutput", "event", "innerloop", "AUDIO", "other", ""};
    for (const char* c : channels) {
        run_command({"game", "add_dio", c, "fullscreen"});
        run_command({"game", "no_dio", c, "fullscreen"});
        run_command({"game", "add_dio", c, "no_dio", c});
        run_command({"game", "no_dio", "ai", "no_dio", c, "add_dio", c});
    }
    // At the end of the line these two see a null and do nothing; the rest read it.
    run_command({"game", "add_dio"});
    run_command({"game", "no_dio"});
    run_command_crash({"game", "level"});
    run_command_crash({"game", "controllerdeadzone"});
    run_command_crash({"game", "audiomode"});
    // Everything at once, in both orders, and twice over App.ini's values.
    run_command({"game", "level", "LXY", "fullscreen", "testalllevels", "openlist", "maximized", "controllerdeadzone", "12", "verticalsyncoff", "add_dio", "sim", "no_dio", "ai", "page", "skipintro",
                 "disableconsolememstats", "nomusic", "noambientaudio", "noaudio", "displaybanks", "displaysounds", "displaymusic", "audiomode", "2", "audiotest", "skipmainmenu"});
    // A number too large for the signed conversion keeps its low bits, here as in the original.
    run_command({"game", "controllerdeadzone", "99999999999"});
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (g_expect_crash) {
            g_recording = false;  // Here, not after the resume: the harness's own allocations must not reach the log.
            g_side = 0;
        }
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
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
    if (pos.empty()) { std::printf("usage: oracle_set <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x001B7B84, (const void*)o_language);
    redirect(0x001B7B12, (const void*)o_region);
    at<void(__cdecl*)()>(0x00125BC7)();  // The C library's floating-point formatters, as in oracle_tok.cpp.
    if (entries && !arm_entries(entries)) return 2;

    g_language = 1;
    g_region = 1;
    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    for (int k = 0; k < 2; k++) {
        for (int i = 0; i < 2; i++) {
            g_logs[k][i].side = k;
            g_logs[k][i].id = i;
        }
        g_histories[k].m_errors = &g_logs[k][0];
        g_histories[k].m_traces = &g_logs[k][1];
    }
    *(GmHistoryLogs**)(uintptr_t)0x002CB0A8 = &g_histories[0];
    GmHistoryLogs::s_instance = &g_histories[1];
    *(GmAppAudioOptions**)(uintptr_t)0x003B360C = &g_audio[0];
    GmAppAudioOptions::s_instance = &g_audio[1];
    AtIoMountManager* mount = (AtIoMountManager*)std::calloc(1, sizeof(AtIoMountManager));
    mount->m_device = g_fs;
    *(AtIoMountManager**)(uintptr_t)0x002E26E0 = mount;
    AtIoMountManager::s_instance = mount;
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_parts();
    run_missing();
    run_inputs();
    run_commands();
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
