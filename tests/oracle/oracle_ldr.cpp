// Differential oracle for the time-sliced loader's base class (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_ldr <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// The class is abstract, so one double supplies its four slots on both sides. The original side has the original's constructor run on it, then the double's vftable put back.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "gm/GmManifestLoader.h"
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

// The clock: each read moves on by a step the test sets, on each side's own count.
static unsigned int g_now[2], g_step;
static unsigned int read_clock(int side) {
    if (g_recording) g_log[side].add('T');
    const unsigned int t = g_now[side];
    g_now[side] += g_step;
    return t;
}
static unsigned int __cdecl o_clock() { return read_clock(0); }
unsigned int AtPlatformMilliseconds() { return read_clock(1); }

static AtMemoryManager g_manager[2];

class Item : public GmManifestItem {
public:
    int side = 0;
    int id = 0;
    void Unknown0() override { if (g_recording) g_log[side].add('0', (uint32_t)id); }
    void Run() override {
        if (g_recording) {
            g_log[side].add('R', (uint32_t)id);
            g_log[side].add('t', (uint32_t)g_manager[side].m_unknown24C);
        }
    }
};

typedef void(__fastcall* FnEnqueue)(void*, void*, void*, int);

// A concrete Parse installs its table first; the tokenizer has nothing to look a word up in before that.
static const AtIoKeyword kWords[] = {{100, "alpha"}, {101, "SoundDatabaseLoaders"}, {102, "GraphicsDatabasesLoaders"}};

class Loader : public GmManifestLoader {
public:
    int side = 0;
    int read_in_parse = 0;
    int finished = 0;
    std::vector<Item>* to_queue = nullptr;
    void Parse(AtIoTokenizer* t) override {
        if (g_recording) g_log[side].add('p', t != nullptr);
        t->SetKeywords(kWords, sizeof kWords / sizeof kWords[0], nullptr);
        for (int i = 0; i < read_in_parse; i++) {
            const int token = t->Next();
            if (g_recording) g_log[side].add('r', (uint32_t)token);
        }
    }
    void AfterParse() override {
        if (g_recording) g_log[side].add('a', (uint32_t)m_state);
        if (!to_queue) return;
        for (Item& i : *to_queue) {
            if (side) Enqueue(&i, 0x55);
            else at<FnEnqueue>(0x00141FF0)(this, nullptr, &i, 0x55);
        }
    }
    void OnToken(int token, AtIoTokenizer*) override {
        if (g_recording) g_log[side].add('k', (uint32_t)token);
    }
    void Finished() override {
        finished++;
        if (g_recording) {
            g_log[side].add('f', (uint32_t)(m_head != nullptr));
            g_log[side].add('g', (uint32_t)(m_tail != nullptr));
        }
    }
};

typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnU)(void*, void*, unsigned int);
typedef void(__fastcall* FnP)(void*, void*, const void*);
typedef void*(__fastcall* FnD)(void*, void*, unsigned int);

// One pair, dirty to begin with, so a store the constructor must make shows if it does not.
struct Pair {
    Loader* o;
    Loader* r;
    std::vector<Item> items[2];
    Pair() {
        static const Loader model;
        o = (Loader*)std::malloc(sizeof(Loader));
        r = (Loader*)std::malloc(sizeof(Loader));
        std::memset(o, 0xCD, sizeof(Loader));
        std::memset(r, 0xCD, sizeof(Loader));
        { On on(0); at<Fn0>(0x00141F20)(o, nullptr); }
        { On on(1); new (r) Loader; }
        void* const vptr = *(void* const*)&model;
        *(void**)o = vptr;
        o->side = 0; o->read_in_parse = 0; o->finished = 0; o->to_queue = nullptr;
        r->side = 1;
    }
    ~Pair() {
        // Each side's file by its own destructor: an open one has a tokenizer of that side's.
        { On on(0); at<Fn0>(0x00015500)(&o->m_file, nullptr); }
        { On on(1); r->m_file.~AtIoTokenFile(); }
        compare_logs("torn down");
        std::free(o);
        std::free(r);
    }
    void queue(int count) {
        for (int s = 0; s < 2; s++) {
            items[s].resize((size_t)count);
            for (int i = 0; i < count; i++) {
                items[s][(size_t)i].side = s;
                items[s][(size_t)i].id = i;
                items[s][(size_t)i].m_next = nullptr;
            }
        }
        o->to_queue = &items[0];
        r->to_queue = &items[1];
    }
};

// Links are compared as positions, each side having its own items.
static long long place(const std::vector<Item>& items, const GmManifestItem* p) {
    if (!p) return -1;
    for (size_t i = 0; i < items.size(); i++) if (&items[i] == p) return (long long)i;
    return -2;
}

static void same(const char* what, Pair& p) {
    eq(what, place(p.items[0], p.o->m_head), place(p.items[1], p.r->m_head));
    eq(what, place(p.items[0], p.o->m_tail), place(p.items[1], p.r->m_tail));
    eq(what, p.o->m_done, p.r->m_done);
    eq(what, p.o->m_state, p.r->m_state);
    eq(what, p.o->m_tag, p.r->m_tag);
    eq(what, p.o->finished, p.r->finished);
    eq(what, p.o->m_name[0], p.r->m_name[0]);
    eq(what, std::memcmp(p.o->m_name, p.r->m_name, 8), 0);
    eqs(what, p.o->m_path, p.r->m_path);
    eq(what, p.o->m_file.m_tokenizer != nullptr, p.r->m_file.m_tokenizer != nullptr);
    eq("manager", g_manager[0].m_unknown24C, g_manager[1].m_unknown24C);
    for (size_t i = 0; i < p.items[0].size() && i < p.items[1].size(); i++) {
        eq("item link", place(p.items[0], p.items[0][i].m_next), place(p.items[1], p.items[1][i].m_next));
    }
}

static void clock_start(unsigned int step) {
    g_now[0] = g_now[1] = 1000;
    g_step = step;
}

static void set_file(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
}

static void configure(Pair& p, const char* name, const char* path, int tag) {
    GmManifestRequest req;
    std::memset(&req, 0, sizeof req);
    std::strncpy(req.m_name, name, 8);
    req.m_path = path;
    req.m_tag = tag;
    at<FnP>(0x00141E10)(p.o, nullptr, &req);
    p.r->Configure(&req);
}

static int update(Pair& p, unsigned int budget, const char* what) {
    int a, b;
    { On on(0); a = at<FnU>(0x00142040)(p.o, nullptr, budget); }
    { On on(1); b = p.r->Update(budget); }
    eq(what, a, b);
    same(what, p);
    compare_logs(what);
    return b;
}

static void run_parts() {
    g_context = "constructor and Reset";
    {
        Pair p;
        compare_logs("constructed");
        same("constructed", p);
        // Dirty what Reset must clear, with what it must leave.
        for (Loader* l : {p.o, p.r}) {
            std::memset((char*)l + 4, 0x77, 0x18);
            l->m_tag = 0x77777777;
            std::memset(l->m_path, 0x77, sizeof l->m_path - 1);
            l->m_path[sizeof l->m_path - 1] = 0;
        }
        at<Fn0>(0x00141E50)(p.o, nullptr);
        p.r->Reset();
        same("reset", p);
        eq("reset second byte of name", p.o->m_name[1], p.r->m_name[1]);
        eq("reset second byte of path", p.o->m_path[1], p.r->m_path[1]);
    }
    g_context = "Configure";
    {
        for (const char* name : {"", "A", "ABCDEFG", "ABCDEFGH"}) {
            for (const char* path : {"", "X", "DESIGN\\LOCATION\\ASTICKY\\LOADER"}) {
                Pair p;
                configure(p, name, path, 0x12345);
                same("configure", p);
            }
        }
    }
    g_context = "Enqueue";
    {
        Pair p;
        p.queue(4);
        // A stale link on the first item stays, on purpose; on a later one it is cleared.
        p.items[0][0].m_next = &p.items[0][3];
        p.items[1][0].m_next = &p.items[1][3];
        p.items[0][2].m_next = &p.items[0][3];
        p.items[1][2].m_next = &p.items[1][3];
        for (size_t i = 0; i < 3; i++) {
            at<FnEnqueue>(0x00141FF0)(p.o, nullptr, &p.items[0][i], 7);
            p.r->Enqueue(&p.items[1][i], 7);
            same("enqueue", p);
        }
    }
    g_context = "Enqueue: an empty queue is told by its head";
    {
        Pair p;
        p.queue(2);
        p.o->m_tail = &p.items[0][1];
        p.r->m_tail = &p.items[1][1];
        at<FnEnqueue>(0x00141FF0)(p.o, nullptr, &p.items[0][0], 0);
        p.r->Enqueue(&p.items[1][0], 0);
        same("stale tail", p);
    }
    g_context = "RunNext";
    {
        Pair p;
        g_manager[0].m_unknown24C = g_manager[1].m_unknown24C = 0x99;
        configure(p, "N", "P", 0x42);
        // Nothing queued: the field is swapped and restored, and nothing finishes.
        { On on(0); at<Fn0>(0x00141F80)(p.o, nullptr); }
        { On on(1); p.r->RunNext(); }
        same("empty", p);
        compare_logs("empty");
        p.queue(3);
        for (size_t i = 0; i < 3; i++) {
            at<FnEnqueue>(0x00141FF0)(p.o, nullptr, &p.items[0][i], 0);
            p.r->Enqueue(&p.items[1][i], 0);
        }
        for (int i = 0; i < 4; i++) {
            { On on(0); at<Fn0>(0x00141F80)(p.o, nullptr); }
            { On on(1); p.r->RunNext(); }
            same("run", p);
            compare_logs("run");
            eq("restored", g_manager[1].m_unknown24C, 0x99);
        }
    }
    g_context = "ReadTokens";
    {
        for (const char* text : {"", "alpha", "\"s\" 5 { } 2.5 [ ]", "alpha alpha 1 alpha"}) {
            Pair p;
            const std::string s = text;
            const std::vector<unsigned char> data(s.begin(), s.end());
            set_file(data);
            configure(p, "", "P", 0);
            { On on(0); at<Fn0>(0x00141EA0)(p.o, nullptr); }
            { On on(1); p.r->Open(); }
            same("opened", p);
            compare_logs("opened");
            for (Loader* l : {p.o, p.r}) l->m_file.m_tokenizer->SetKeywords(kWords, sizeof kWords / sizeof kWords[0], nullptr);
            { On on(0); at<FnP>(0x00141E70)(p.o, nullptr, p.o->m_file.m_tokenizer); }
            { On on(1); p.r->ReadTokens(p.r->m_file.m_tokenizer); }
            compare_logs("tokens");
        }
    }
    g_context = "destructors";
    for (unsigned int flag : {0u, 1u}) {
        for (int open : {0, 1}) {
            static const std::vector<unsigned char> data = {'a'};
            set_file(data);
            Loader* o = (Loader*)::operator new(sizeof(Loader));
            Loader* r = (Loader*)::operator new(sizeof(Loader));
            at<Fn0>(0x00141F20)(o, nullptr);
            new (r) Loader;
            std::strcpy(o->m_path, "P");
            std::strcpy(r->m_path, "P");
            if (open) {
                { On on(0); at<Fn0>(0x00141EA0)(o, nullptr); }
                { On on(1); r->Open(); }
                compare_logs("opened");
            }
            // Everything the destructors clear, set.
            for (Loader* l : {o, r}) {
                l->m_head = l->m_tail = (GmManifestItem*)(uintptr_t)4;
                l->m_done = 1;
                l->m_state = 2;
                l->m_tag = 5;
                l->m_name[0] = 'N';
            }
            void* after;
            { On on(0); after = at<FnD>(0x00142020)(o, nullptr, flag); }
            eq("returns this", (long long)(uintptr_t)after, (long long)(uintptr_t)o);
            if (!flag) {
            }
            {
                On on(1);
                if (flag) delete r;
                else r->~Loader();
            }
            compare_logs("destructor");
            if (!flag) {
                // Still ours to look at, for the fields they clear.
                eq("head", (long long)(uintptr_t)o->m_head, (long long)(uintptr_t)r->m_head);
                eq("tail", (long long)(uintptr_t)o->m_tail, (long long)(uintptr_t)r->m_tail);
                eq("done", o->m_done, r->m_done);
                eq("state", o->m_state, r->m_state);
                eq("tag", o->m_tag, r->m_tag);
                eq("name", o->m_name[0], r->m_name[0]);
                eq("path", o->m_path[0], r->m_path[0]);
                eq("the original clears them", o->m_state | o->m_tag | o->m_done, 0);
                ::operator delete(o);
                ::operator delete(r);
            }
        }
    }
}

// The whole machine over one file, with a clock that gives each call the budget or less.
static void run_machine(const std::string& name, const std::vector<unsigned char>& data, unsigned int step, unsigned int budget, int items, int read_in_parse, int calls_limit) {
    g_context = name + " step " + std::to_string(step) + " budget " + std::to_string(budget);
    Pair p;
    compare_logs("constructed");
    set_file(data);
    clock_start(step);
    g_manager[0].m_unknown24C = g_manager[1].m_unknown24C = 0x31;
    configure(p, "ABC", "DESIGN\\FILE", 0x77);
    p.o->read_in_parse = p.r->read_in_parse = read_in_parse;
    if (items >= 0) p.queue(items);
    for (int i = 0; i < calls_limit; i++) {
        if (update(p, budget, "update")) break;
    }
    // Once more: done stays done.
    update(p, budget, "after");
    eq("clock", g_now[0], g_now[1]);
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

static void run_states() {
    const std::vector<unsigned char> text = bytes("alpha \"beta\" 5 { 2.5 }");
    for (unsigned int step : {0u, 1u, 7u, 30u, 31u, 100u}) {
        for (unsigned int budget : {1u, 2u, 30u}) {
            if (step == 0 && budget == 0) continue;
            for (int items : {-1, 0, 1, 3}) {
                run_machine("text", text, step, budget, items, 0, 20);
                run_machine("text", text, step, budget, items, 2, 20);
            }
        }
    }
    // A budget of none does nothing, however often.
    run_machine("none", text, 5, 0, 1, 0, 4);
    run_machine("empty file", bytes(""), 1, 2, 1, 0, 20);

    // Will not open, by each way.
    {
        g_context = "missing";
        g_fs->has = false;
        run_machine("missing", text, 1, 2, 1, 0, 6);
        g_fs->has = true;
        g_fs->open_status = 9;
        run_machine("status", text, 1, 2, 1, 0, 6);
        g_fs->open_status = 0;
    }
    // A name of none, and a long one: the unused name built on opening must not matter.
    for (const char* name : {"", "ABCDEFGH"}) {
        g_context = std::string("name ") + name;
        Pair p;
        set_file(text);
        configure(p, name, "P", 0);
        { On on(0); at<Fn0>(0x00141EA0)(p.o, nullptr); }
        { On on(1); p.r->Open(); }
        same("open", p);
        compare_logs("open");
    }
    // States reached by setting them: no tokenizer, and numbers outside the machine.
    {
        g_context = "no tokenizer";
        Pair p;
        p.o->m_state = p.r->m_state = 1;
        clock_start(1);
        eq("no tokenizer: fails, then the budget allows one more look", update(p, 2, "no tokenizer"), 1);
        eq("state", p.r->m_state, 4);
    }
    // The same, with a file open, so the close can be seen.
    {
        g_context = "no tokenizer, file open";
        Pair p;
        set_file(text);
        configure(p, "", "P", 0);
        { On on(0); at<Fn0>(0x00141EA0)(p.o, nullptr); }
        { On on(1); p.r->Open(); }
        compare_logs("opened");
        AtIoTokenizer* const kept[2] = {p.o->m_file.m_tokenizer, p.r->m_file.m_tokenizer};
        p.o->m_file.m_tokenizer = p.r->m_file.m_tokenizer = nullptr;
        p.o->m_file.m_ownsTokenizer = p.r->m_file.m_ownsTokenizer = 0;
        clock_start(100);
        update(p, 2, "closed");
        (void)kept;  // Left unfreed: the harness does not depend on it, and it may be neither side's to free by then.
    }
    for (int state : {3, 4, 5, 100, -1}) {
        g_context = "state " + std::to_string(state);
        Pair p;
        p.o->m_state = p.r->m_state = state;
        clock_start(1);
        update(p, 3, "state");
    }
    // Running with the done flag already set: it is worked out afresh.
    {
        g_context = "done flag";
        Pair p;
        p.queue(2);
        for (size_t i = 0; i < 2; i++) {
            at<FnEnqueue>(0x00141FF0)(p.o, nullptr, &p.items[0][i], 0);
            p.r->Enqueue(&p.items[1][i], 0);
        }
        p.o->m_state = p.r->m_state = 2;
        p.o->m_done = p.r->m_done = 1;
        clock_start(5);
        update(p, 1, "first");
        update(p, 1, "second");
    }
    // A clock that wraps: the budget is a difference.
    {
        g_context = "wrap";
        Pair p;
        set_file(text);
        configure(p, "", "P", 0);
        g_now[0] = g_now[1] = 0xFFFFFFFEu;
        g_step = 1;
        update(p, 5, "wrap");
    }
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
    if (pos.size() != 2) { std::printf("usage: oracle_ldr <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x00020640, (const void*)o_clock);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

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
    *(AtMemoryManager**)(uintptr_t)0x002C60AC = &g_manager[0];
    AtMemoryManager::s_instance = &g_manager[1];
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_parts();
    run_states();

    int files = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".MNG\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_machine(line, data, 0, 30, 3, 0, 3);
        run_machine(line, data, 40, 30, 3, 1, 12);
        files++;
    }
    std::fclose(index);
    if (files == 0) { std::printf("no files listed\n"); g_failures++; }
    report_entries();
    std::printf("%s: %d files; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
