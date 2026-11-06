// Differential oracle for the reader of Levels.txt (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_lvl <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// Cases: the real file, found through index.txt in the directory tools/jam.py textcases writes; and input written here.
#include <windows.h>

#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmLevelManager.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 20) std::printf("DIFF %s: %s: original [%s], recovered [%s]\n", g_context.c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

static const char kPending[] = "GmLevelManager::ReleaseLevelData (0x0013E530) is not recovered";

// Stands in for the original's 0x0013E530: same log as the recovered side's diagnostic, so both show when it is reached.
static void __fastcall o_release(void*, void*) {
    g_log[0].add('F', 0);
    record_message(kPending, 0);
}

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnS)(void*, void*, const char*);
typedef void(__fastcall* FnU)(void*, void*, unsigned);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__cdecl* FnSetHeap)(void*);
typedef void(__cdecl* FnCopy8)(char*, const char*);

static GmLevelManager* new_original() {
    // The original constructor also builds content classes that are not recovered, so this builds only what is under test.
    void* p = std::calloc(1, sizeof(GmLevelManager));
    *(uint32_t*)p = 0x0028DAB4;
    at<Fn0>(0x0013D950)(p, nullptr);
    return (GmLevelManager*)p;
}

static void same(const char* what, const GmLevelManager* o, const GmLevelManager* r) {
    eq(what, o->m_currentIndex, r->m_currentIndex);
    eq(what, std::memcmp(o->m_currentName, r->m_currentName, 9), 0);
    eq(what, o->m_previousIndex, r->m_previousIndex);
    eq(what, o->m_levelCount, r->m_levelCount);
    eq(what, o->m_loaded, r->m_loaded);
    eq(what, o->m_unknown020, r->m_unknown020);
    eq(what, std::memcmp(o->m_startPoint, r->m_startPoint, 6), 0);
    eq(what, std::memcmp(o->m_unknown02C, r->m_unknown02C, sizeof o->m_unknown02C), 0);
    eq(what, o->m_unknown05C, r->m_unknown05C);
    eq(what, o->m_unknown060, r->m_unknown060);
    eq(what, o->m_names.m_count, r->m_names.m_count);
    eq(what, o->m_kinds != nullptr, r->m_kinds != nullptr);
    eq(what, o->m_loadItems != nullptr, r->m_loadItems != nullptr);
}

static void same_table(const char* what, const GmLevelManager* o, const GmLevelManager* r) {
    same(what, o, r);
    if (o->m_levelCount != r->m_levelCount || !o->m_loaded || !r->m_loaded) return;
    const int names = (int)(o->m_names.m_count < r->m_names.m_count ? o->m_names.m_count : r->m_names.m_count);
    for (int i = 0; i < o->m_levelCount && i < names; i++) {
        char a[9] = {0}, b[9] = {0};
        std::memcpy(a, o->m_names.m_items[i].m_name, 8);
        std::memcpy(b, r->m_names.m_items[i].m_name, 8);
        eqs("name", a, b);
        eq("kind", o->m_kinds[i], r->m_kinds[i]);
        eq("load items", o->m_loadItems[i], r->m_loadItems[i]);
        eqs("description", o->m_descriptions[i] ? o->m_descriptions[i] : "(none)", r->m_descriptions[i] ? r->m_descriptions[i] : "(none)");
    }
}

static void set_file(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fs->files[0].mode = g_fs->files[1].mode = kNormal;
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static void load_both(GmLevelManager* o, GmLevelManager* r, const std::vector<unsigned char>& data) {
    set_file(data);
    { On on(0); o->Load("Levels.txt"); }
    { On on(1); r->Load("Levels.txt"); }
    same_table("after Load", o, r);
    compare_logs("Load");
}

static void unload_both(GmLevelManager* o, GmLevelManager* r) {
    { On on(0); o->Unload(); }
    { On on(1); r->Unload(); }
    same("after Unload", o, r);
    compare_logs("Unload");
}

static void run_text(const std::string& name, const std::string& text) {
    g_context = name;
    const std::vector<unsigned char> data = bytes(text);
    GmLevelManager* o = new_original();
    GmLevelManager* r = new GmLevelManager;
    load_both(o, r, data);
    unload_both(o, r);
    std::free(o);
    r->m_names.Resize(0);
    delete r;
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
}

static void run_synthetic() {
    static const char* const texts[] = {
        "",
        "// nothing",
        "Levels [0] { }",
        "Levels [1] { Level \"A\" NormalLevel }",
        "Levels [1] { Level \"A\" HiddenLevel }",
        "Levels [1] { Level \"A\" NonSelectableLevel }",
        "Levels [1] { Level \"A\" FMVCinema }",
        "Levels [1] { Level \"A\" DesignerCinema }",
        "Levels [1] { Level \"A\" ConversationCinema }",
        "Levels [1] { Level \"A\" ToolCinema }",
        "Levels [1] { Level \"A\" Type }",
        "Levels [1] { Level \"A\" }",
        "Levels [1] { Level \"A\" 5 }",
        "Levels [2] { Level \"A\" Level \"B\" }",
        "Levels [2] { Level \"A\" NormalLevel Level \"B\" FMVCinema }",
        "Levels [2] { Level \"A\" NormalLevel { } Level \"B\" HiddenLevel { NumLoadItems 7 } }",
        "Levels [1] { Level \"A\" NormalLevel { Description \"D\" } }",
        "Levels [1] { Level \"A\" NormalLevel { NumLoadItems 7 Description \"D\" } }",
        "Levels [1] { Level \"A\" NormalLevel { Description \"D\" NumLoadItems 7 } }",
        "Levels [1] { Level \"A\" NormalLevel { NumLoadItems 7 NumLoadItems 8 } }",
        "Levels [1] { Level \"A\" NormalLevel { 1 2.5 \"s\" unknown [ ] Type NormalLevel NumLoadItems 9 } }",
        "Levels [1] { Level \"A\" { NumLoadItems 3 } }",
        "Levels [1] { Level \"12345678\" NormalLevel }",
        "Levels [1] { Level \"123456789\" NormalLevel }",
        "Levels [1] { Level 5 NormalLevel }",
        "Levels [1] { NormalLevel \"A\" NormalLevel }",
        "Levels [1] { Level \"A\" NormalLevel { NumLoadItems \"x\" } }",
        "Levels [1] { Level \"A\" NormalLevel { Description 5 } }",
        "Levels [1] { Level \"A\" NormalLevel ,",
        "Levels [1] { Level \"A\" NormalLevel } trailing 1 2 3",
        "Levels 1 { }",
        "Levels [ 1 { }",
        "Levels [ x ] { }",
        "Levels [ 1 ] x",
        "before \"x\" 5 Levels [1] { Level \"A\" NormalLevel } after",
        "levels [1] { level \"a\" normallevel { numloaditems 2 description \"d\" } }",
    };
    for (const char* t : texts) run_text(std::string("text [") + t + "]", t);
    // Two tables in one file: the name array grows or shrinks, keeping names. The first run's tables are lost, as in the original.
    run_text("grow", "Levels [1] { Level \"A\" NormalLevel } Levels [3] { Level \"B\" HiddenLevel Level \"C\" NormalLevel Level \"D\" FMVCinema }");
    run_text("shrink", "Levels [3] { Level \"A\" NormalLevel Level \"B\" NormalLevel Level \"C\" NormalLevel } Levels [1] { Level \"D\" HiddenLevel }");
    run_text("same size", "Levels [2] { Level \"A\" NormalLevel Level \"B\" NormalLevel } Levels [2] { Level \"C\" NormalLevel Level \"D\" NormalLevel }");
    run_text("to nothing", "Levels [2] { Level \"A\" NormalLevel Level \"B\" NormalLevel } Levels [0] { }");
    // A count larger than the entries: errors for each missing one. Reading past the end is safe here because no block is open.
    run_text("count too large", "Levels [3] { Level \"A\" NormalLevel }");
    run_text("count too small", "Levels [1] { Level \"A\" NormalLevel Level \"B\" NormalLevel }");
}

static void run_lifecycle() {
    const std::vector<unsigned char> a = bytes("Levels [2] { Level \"A\" NormalLevel { Description \"first\" } Level \"B\" FMVCinema }");
    const std::vector<unsigned char> b = bytes("Levels [3] { Level \"C\" HiddenLevel { NumLoadItems 3 Description \"second\" } Level \"D\" FMVCinema Level \"E\" NormalLevel }");
    g_context = "load twice";
    GmLevelManager* o = new_original();
    GmLevelManager* r = new GmLevelManager;
    same("fresh", o, r);
    unload_both(o, r);  // Nothing loaded: only the reset.
    load_both(o, r, a);
    load_both(o, r, b);  // Loading over a table unloads it first, through the vftable.
    load_both(o, r, a);
    unload_both(o, r);
    load_both(o, r, b);

    g_context = "reset";
    o->m_currentIndex = r->m_currentIndex = 4;
    std::memset(o->m_currentName, 'n', 9);
    std::memset(r->m_currentName, 'n', 9);
    o->m_previousIndex = r->m_previousIndex = 5;
    o->m_unknown020 = r->m_unknown020 = 6;
    std::memset(o->m_startPoint, 's', 8);
    std::memset(r->m_startPoint, 's', 8);
    for (int i = 0; i < 12; i++) o->m_unknown02C[i] = r->m_unknown02C[i] = 7;
    o->m_unknown05C = r->m_unknown05C = 8;
    o->m_unknown060 = r->m_unknown060 = 9;
    int* okinds = o->m_kinds;
    int* oitems = o->m_loadItems;
    int* rkinds = r->m_kinds;
    int* ritems = r->m_loadItems;
    at<Fn0>(0x0013D950)(o, nullptr);
    r->Reset();
    same("after Reset", o, r);
    eq("kinds cleared", (intptr_t)o->m_kinds, 0);
    eq("items cleared", (intptr_t)o->m_loadItems, 0);
    // Only the fields Reset touches changed: m_descriptions and the name array stay.
    eq("descriptions kept", o->m_descriptions != nullptr, r->m_descriptions != nullptr);
    eq("names kept", o->m_names.m_items != nullptr, r->m_names.m_items != nullptr);
    std::free(okinds);
    std::free(oitems);
    std::free(rkinds);
    std::free(ritems);

    g_context = "no heap";
    GmLevelManager::SetHeap(nullptr);
    at<FnSetHeap>(0x0013D8E0)(nullptr);
    {
        GmLevelManager* o2 = new_original();
        GmLevelManager* r2 = new GmLevelManager;
        load_both(o2, r2, a);
        unload_both(o2, r2);
        std::free(o2);
        std::free(r2);  // Not destroyed: its names are compared below as a leak on both sides, and this is a test process.
    }
    g_context = "set heap";
    at<FnSetHeap>(0x0013D8E0)(&g_heap_token);
    eq("heap", *(intptr_t*)(uintptr_t)0x003B356C, (intptr_t)&g_heap_token);
    GmLevelManager::SetHeap(&g_heap_token);
    eq("heap", (intptr_t)GmLevelManager::s_heap, (intptr_t)&g_heap_token);
}

static void run_names() {
    g_context = "names";
    // One element, each way out. The array form is exercised by Resize.
    void* op = std::calloc(1, sizeof(GmLevelName));
    at<Fn0>(0x001950E0)(op, nullptr);
    eq("vftable", *(intptr_t*)op, 0x00292F9C);
    GmLevelName* o = (GmLevelName*)op;
    GmLevelName* r = new GmLevelName;
    at<FnS>(0x001950C0)(o, nullptr, "ABCDEFGH");
    r->SetName("ABCDEFGH");
    eq("name", std::memcmp(o->m_name, r->m_name, 8), 0);
    at<FnS>(0x001950C0)(o, nullptr, nullptr);
    r->SetName(nullptr);
    eq("null name", o->m_name[0], r->m_name[0]);
    eq("rest kept", std::memcmp(o->m_name + 1, r->m_name + 1, 7), 0);
    GmLevelName other;
    other.SetName("OTHER123");
    at<FnS>(0x001950A0)(o, nullptr, (const char*)&other);
    *r = other;
    eq("assign", std::memcmp(o->m_name, r->m_name, 8), 0);
    { On on(0); at<FnDel>(0x0013D8F0)(o, nullptr, 1); }
    { On on(1); delete r; }
    compare_logs("single delete");

    void* keep = std::calloc(1, sizeof(GmLevelName));
    at<Fn0>(0x001950E0)(keep, nullptr);
    at<FnDel>(0x0013D8F0)(keep, nullptr, 0);
    eq("base vftable after destruction", *(intptr_t*)keep, 0x0027855C);
    std::free(keep);
    void* keep2 = std::calloc(1, sizeof(GmLevelName));
    at<Fn0>(0x001950E0)(keep2, nullptr);
    at<Fn0>(0x00195100)(keep2, nullptr);
    eq("destructor", *(intptr_t*)keep2, 0x0027855C);
    std::free(keep2);

    char oa[12], ra[12];
    std::memset(oa, 'z', sizeof oa);
    std::memset(ra, 'z', sizeof ra);
    at<FnCopy8>(0x00013F70)(oa, "12345678");
    AtCopyName8(ra, "12345678");
    eq("copy", std::memcmp(oa, ra, sizeof oa), 0);
    at<FnCopy8>(0x00013F70)(oa, nullptr);
    AtCopyName8(ra, nullptr);
    eq("copy null", std::memcmp(oa, ra, sizeof oa), 0);

    // The array on its own: same size is a no-op, zero frees, and a null array has nothing to keep.
    g_context = "array";
    *(void**)(uintptr_t)0x003B3574 = &g_heap_token;
    GmLevelNameArray::s_heap = &g_heap_token;
    GmLevelNameArray oarr = {nullptr, 0}, rarr = {nullptr, 0};
    const auto resize = [&](unsigned n) {
        { On on(0); at<FnU>(0x0013DFE0)(&oarr, nullptr, n); }
        { On on(1); rarr.Resize(n); }
        eq("count", oarr.m_count, rarr.m_count);
        eq("set", oarr.m_items != nullptr, rarr.m_items != nullptr);
        compare_logs("Resize");
    };
    resize(0);
    resize(2);
    oarr.m_items[0].SetName("KEEP1");
    rarr.m_items[0].SetName("KEEP1");
    oarr.m_items[1].SetName("KEEP2");
    rarr.m_items[1].SetName("KEEP2");
    resize(2);
    resize(3);
    eqs("kept", oarr.m_items[1].m_name, rarr.m_items[1].m_name);
    resize(1);
    eqs("kept", oarr.m_items[0].m_name, rarr.m_items[0].m_name);
    resize(0);
    eq("freed", (intptr_t)oarr.m_items, 0);
    *(void**)(uintptr_t)0x003B3574 = nullptr;
    GmLevelNameArray::s_heap = nullptr;
    resize(2);  // No heap: raises, then the redirected allocator carries on.
    resize(0);
    *(void**)(uintptr_t)0x003B3574 = &g_heap_token;
    GmLevelNameArray::s_heap = &g_heap_token;
}

// ---- both implementations dereference null if the error routine returns after a failed open ------------------------------

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_INT_DIVIDE_BY_ZERO) return EXCEPTION_CONTINUE_SEARCH;
    if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
    char text[256];
    const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
    DWORD ignored;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
    ExitProcess(3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static __declspec(noinline) void load_expecting_crash(GmLevelManager* m, int side) {
    guarded([&]() {
        g_side = side;
        g_recording = true;
        m->Load("MISSING.TXT");
    });
    g_recording = false;
    g_side = 0;
}

static void run_open_failure() {
    for (int present = 0; present < 2; present++) {
        g_context = present ? "open fails" : "no file system has it";
        g_fs->has = present != 0;
        g_fs->open_status = present ? 9 : 0;
        GmLevelManager* o = new_original();
        GmLevelManager* r = new GmLevelManager;
        const int before = g_crashes;
        load_expecting_crash(o, 0);
        load_expecting_crash(r, 1);
        eq("both stop", g_crashes - before, 2);
        compare_logs("failed open");
        g_fs->has = true;
        g_fs->open_status = 0;
    }
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
    if (pos.size() != 2) { std::printf("usage: oracle_lvl <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x0013E530, (const void*)o_release);
    at<void(__cdecl*)()>(0x00125BC7)();  // The C library's floating-point formatters, as in oracle_tok.cpp.
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    *(void**)(uintptr_t)0x002C60B0 = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x002C63CC = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x003B356C = &g_heap_token;
    GmLevelManager::s_heap = &g_heap_token;

    g_fill = 0xCD;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_names();
    run_lifecycle();
    run_open_failure();
    run_synthetic();

    // The real file.
    std::string dir = pos[1], level_file;
    {
        FILE* index = nullptr;
        if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
        char line[1024];
        while (std::fgets(line, sizeof line, index)) {
            if (!std::strstr(line, "\tLEVELS.TXT\t")) continue;
            *std::strchr(line, '\t') = 0;
            level_file = line;
            break;
        }
        std::fclose(index);
    }
    if (level_file.empty()) { std::printf("Levels.txt not listed\n"); return 2; }
    std::vector<unsigned char> real;
    {
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + level_file).c_str(), "rb") || !f) { std::printf("cannot read Levels.txt\n"); return 2; }
        std::fseek(f, 0, SEEK_END);
        real.resize((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(real.data(), 1, real.size(), f);
        std::fclose(f);
    }
    g_context = "Levels.txt";
    GmLevelManager* o = new_original();
    GmLevelManager* r = new GmLevelManager;
    load_both(o, r, real);
    const int levels = o->m_levelCount;
    int described = 0;
    for (int i = 0; i < levels; i++) described += o->m_descriptions[i] != nullptr;
    unload_both(o, r);

    report_entries();
    std::printf("%s: Levels.txt with %d entries, %d described; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", levels, described, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
