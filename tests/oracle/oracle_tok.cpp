// Differential oracle for the recovered text tokenizer and its stream (Windows x86, local only). See oracle_jam.cpp and
// docs/verification.md. Runs original code from the owner's XBE beside recovered source, in lockstep, token by token.
//
//   oracle_tok <default.xbe> <cases dir> [--entries FILE] [--mutate] [--limit N] [--digest FILE]
//
// Cases: every <name>.bin in the directory, real files written by tools/jam.py textcases. Names starting "b_" are binary samples.
#include <windows.h>

#include <float.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "oracle_image.h"
#include "tok_common.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks, g_tokens;
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

// ---- call log: fixed storage, so logging never allocates --------------------------------------------------------------
struct Log {
    uint32_t v[1 << 14];
    int n = 0;
    void add(uint32_t a, uint32_t b = 0) { if (n + 2 <= (int)(sizeof v / 4)) { v[n++] = a; v[n++] = b; } }
} static g_log[2];
static char g_message[2][4096];
static int g_side;
static bool g_recording;
static int g_syntax_errors[2];

static void record_message(const char* m, int side) {
    if (!g_recording) return;
    strncat_s(g_message[side], m, _TRUNCATE);
    strncat_s(g_message[side], "|", _TRUNCATE);
    if (std::strstr(m, "syntax error")) g_syntax_errors[side]++;
}

static void* __cdecl o_new(unsigned n) { if (g_recording) g_log[0].add('N', n); return std::calloc(1, n); }
static void __cdecl o_delete(void* p) { if (g_recording && p) g_log[0].add('D'); std::free(p); }
static void __cdecl o_push(void* h) { if (g_recording) g_log[0].add('P', (uint32_t)(uintptr_t)h); }
static void __cdecl o_pop() { if (g_recording) g_log[0].add('p'); }
static void __cdecl o_fatal(const char* m, int a, int, int) { if (g_recording) { g_log[0].add('F', a); record_message(m, 0); } }
static void __cdecl o_raise(int code, int, int, int) { if (g_recording) g_log[0].add('R', code); }

void* operator new(std::size_t n) { if (g_recording && g_side == 1) g_log[1].add('N', (uint32_t)n); return std::calloc(1, n); }
void* operator new[](std::size_t n) { if (g_recording && g_side == 1) g_log[1].add('N', (uint32_t)n); return std::calloc(1, n); }
void operator delete(void* p) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void operator delete[](void* p) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void operator delete(void* p, std::size_t) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { if (g_recording && g_side == 1 && p) g_log[1].add('D'); std::free(p); }
void AtPushHeap(void* h) { if (g_recording) g_log[1].add('P', (uint32_t)(uintptr_t)h); }
void AtPopHeap() { if (g_recording) g_log[1].add('p'); }
void AtFatal(const char* m, int a, int, int) { if (g_recording) { g_log[1].add('F', a); record_message(m, 1); } }
void AtRaise(int code, int, int, int) { if (g_recording) g_log[1].add('R', code); }

static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

// Runs one call on one side with logging on. Logging is off in between, because the harness's own containers allocate.
struct On {
    explicit On(int side) { g_side = side; g_recording = true; }
    ~On() { g_recording = false; g_side = 0; }
};

static void compare_logs(const char* what) {
    g_recording = false;
    eq(what, g_log[0].n, g_log[1].n);
    if (g_log[0].n != g_log[1].n && g_failures < 3) {
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < g_log[side].n && i < 40; i += 2) std::printf("%c%X ", (char)g_log[side].v[i], g_log[side].v[i + 1]);
            std::printf("\n");
        }
    }
    for (int i = 0; i < g_log[0].n && i < g_log[1].n; i++) {
        if (g_log[0].v[i] != g_log[1].v[i]) {
            eq(what, g_log[0].v[i], g_log[1].v[i]);
            break;
        }
    }
    eqs(what, g_message[0], g_message[1]);
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
}

// ---- test doubles beneath both sides ----------------------------------------------------------------------------------
class Request : public AtIoFileRequest {
public:
    int side = 0;
    int status = 0;
    void Unknown0() override {}
    void Unknown1() override {}
    int Wait() override { g_log[side].add('W', status); return status; }
};

enum Mode { kNormal, kPendingIo, kOneByte, kZeroAtEnd, kFailAfterFirst, kWaitFails };

class MemFile : public AtIoFile {
public:
    int side = 0;
    const unsigned char* data = nullptr;
    size_t size = 0;
    Mode mode = kNormal;
    std::string* written = nullptr;
    int reads = 0;
    Request request;
    int Close() override { g_log[side].add('c'); return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { g_log[side].add('S', offset); m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest** req) override {
        g_log[side].add('r', n);
        reads++;
        if (mode == kFailAfterFirst && reads > 1) return AtIoFile::kIoError;
        size_t have = size - m_position;
        if (n > have) n = (unsigned)have;
        if (mode == kOneByte && n > 1) n = 1;
        if (n) std::memcpy(out, data + m_position, n);
        m_position += n;
        *done = n;
        const int status = (n || mode == kZeroAtEnd) ? 0 : (int)AtIoFile::kEndOfFile;
        if (mode == kPendingIo || mode == kWaitFails) {
            request.side = side;
            request.status = mode == kWaitFails ? (int)AtIoFile::kIoError : status;
            *req = &request;
            return AtIoFile::kPending;
        }
        return status;
    }
    int Write(const void* in, unsigned int n, unsigned int* done) override {
        g_log[side].add('w', n);
        // Nothing is logged about content here: bytes are compared at the end, as one string.
        if (written) {
            const bool was = g_recording;  // The string grows on the harness's heap, not the code under test's.
            g_recording = false;
            written->append((const char*)in, n);
            g_recording = was;
        }
        m_position += n;
        if (mode == kPendingIo) {
            request.side = side;
            request.status = 0;
            *done = (unsigned int)(uintptr_t)&request;
            return AtIoFile::kPending;
        }
        *done = n;
        return 0;
    }
    int Flush() override { g_log[side].add('f'); return 5; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { g_log[side].add('e'); return 0; }
};

class MemFs : public AtIoFileSystem {
public:
    MemFile files[2];
    bool has = true;
    int open_status = 0;
    int Open(const char* path, int flags, AtIoFile** out) override {
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(path, g_side);
        MemFile& f = files[g_side];
        f.side = g_side;
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = (flags & 0x21) ? 0x0C : 2;
        *out = open_status ? nullptr : &f;
        return open_status;
    }
    int Close(AtIoFile*) override { g_log[g_side].add('C'); return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char*) override { g_log[g_side].add('M'); return has ? 0 : 1; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

static void set_heaps(void* tokens, void* stream) {
    *(void**)(uintptr_t)0x002C60B0 = tokens;
    AtIoTokenFile::s_heap = tokens;
    *(void**)(uintptr_t)0x002C63CC = stream;
    AtIoStream::s_heap = stream;
}

typedef void(__fastcall* Ctor)(void*, void*);
typedef int(__fastcall* FnOpen4)(void*, void*, const char*, const char*, int, void*);
typedef int(__fastcall* FnOpen3)(void*, void*, const char*, int, void*);
typedef void(__fastcall* Fn0)(void*, void*);

static AtIoTokenFile* new_original_file() {
    void* p = std::calloc(1, sizeof(AtIoTokenFile));
    at<Ctor>(0x00015460)(p, nullptr);
    return (AtIoTokenFile*)p;
}

static void delete_original_file(AtIoTokenFile* f) {
    at<Fn0>(0x00015500)(f, nullptr);
    std::free(f);
}

static void same_stream(const char* what, const AtIoStream* o, const AtIoStream* r) {
    eq(what, o->m_size, r->m_size);
    eq(what, o->m_cur - o->m_base, r->m_cur - r->m_base);
    eq(what, o->m_limit - o->m_base, r->m_limit - r->m_base);
    eq(what, o->m_line, r->m_line);
}

static void same_tokens(const char* what, const AtIoTextTokenizer* o, const AtIoTextTokenizer* r) {
    eq(what, o->m_type, r->m_type);
    eq(what, o->m_int, r->m_int);
    eq(what, *(const uint32_t*)&o->m_real, *(const uint32_t*)&r->m_real);
    eq(what, o->m_stringLength, r->m_stringLength);
    eq(what, o->m_pushedBack, r->m_pushedBack);
    eqs(what, o->m_string, r->m_string);
    eqs(what, o->m_identifier, r->m_identifier);
    if (o->m_stream && r->m_stream) same_stream(what, o->m_stream, r->m_stream);
}

// ---- a whole file, token by token -------------------------------------------------------------------------------------
struct Options {
    Mode mode = kNormal;
    int escapes = 0;
    bool stop_at_syntax_error = true;
    const AtIoKeyword* table = nullptr;
    unsigned count = 0;
    Digest* digest = nullptr;
    bool original_only = false;
    int pushed_back_at = -1;
};

static void lex_both(const std::string& name, const std::vector<unsigned char>& data, const Options& opt) {
    g_context = name;
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fs->files[0].mode = g_fs->files[1].mode = opt.mode;
    AtIoTokenFile* of = new_original_file();
    AtIoTokenFile* rf = new AtIoTokenFile;
    int os, rs;
    { On on(0); os = at<FnOpen4>(0x00015560)(of, nullptr, "X\\NAME", "ABC", 2, nullptr); }
    { On on(1); rs = rf->Open("X\\NAME", "ABC", 2, nullptr); }
    eq("Open", os, rs);
    compare_logs("Open");
    AtIoTextTokenizer* ot = (AtIoTextTokenizer*)of->m_tokenizer;
    AtIoTextTokenizer* rt = (AtIoTextTokenizer*)rf->m_tokenizer;
    { On on(0); ot->SetKeywords(opt.table, opt.count, nullptr); }
    { On on(1); rt->SetKeywords(opt.table, opt.count, nullptr); }
    eq("keyword count", ot->m_keywordCount, rt->m_keywordCount);
    for (int i = 0; i < AtIoTextTokenizer::kHashSize; i++) eq("hash", (intptr_t)ot->m_hash[i], (intptr_t)rt->m_hash[i]);
    ot->m_escapes = rt->m_escapes = opt.escapes;
    g_syntax_errors[0] = g_syntax_errors[1] = 0;
    for (int token = 0; token < 2000000; token++) {
        if (token == opt.pushed_back_at) ot->m_pushedBack = rt->m_pushedBack = 1;
        int a, b;
        { On on(0); a = ot->Next(); }
        { On on(1); b = rt->Next(); }
        g_tokens++;
        if (opt.stop_at_syntax_error && (g_syntax_errors[0] || g_syntax_errors[1])) {
            // After reporting, the original can index its token table out of bounds and take what it finds as the token: the recovered
            // code stops there on purpose. So only the first message can be compared, and this is the last token of this input.
            eq("both report a syntax error", g_syntax_errors[0] > 0, g_syntax_errors[1] > 0);
            char* a_end = std::strchr(g_message[0], '|');
            char* b_end = std::strchr(g_message[1], '|');
            if (a_end) *a_end = 0;
            if (b_end) *b_end = 0;
            eqs("first message", g_message[0], g_message[1]);
            g_log[0].n = g_log[1].n = 0;
            g_message[0][0] = g_message[1][0] = 0;
            break;
        }
        eq("Next", a, b);
        same_tokens("after Next", ot, rt);
        compare_logs("Next");
        if (opt.digest) opt.digest->token(a, *ot);
        if (a == 0) break;
        if (g_failures > 20) break;
    }
    { On on(0); delete_original_file(of); }
    { On on(1); rf->~AtIoTokenFile(); } ::operator delete(rf);
    compare_logs("close");
}

// The original alone, for a digest under a given x87 precision. Nothing to compare against but another digest.
static void digest_original(const std::vector<unsigned char>& data, const AtIoKeyword* table, unsigned count, Digest& digest) {
    g_fs->files[0].data = data.data();
    g_fs->files[0].size = data.size();
    g_fs->files[0].mode = kNormal;
    AtIoTokenFile* of = new_original_file();
    at<FnOpen4>(0x00015560)(of, nullptr, "X\\NAME", "ABC", 2, nullptr);
    AtIoTextTokenizer* ot = (AtIoTextTokenizer*)of->m_tokenizer;
    ot->SetKeywords(table, count, nullptr);
    g_recording = true;
    g_syntax_errors[0] = 0;
    for (int token = 0; token < 2000000; token++) {
        const int a = ot->Next();
        if (g_syntax_errors[0]) break;
        digest.token(a, *ot);
        if (a == 0) break;
    }
    g_recording = false;
    delete_original_file(of);
    g_log[0].n = 0;
    g_message[0][0] = 0;
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

// ---- input written to reach every state ------------------------------------------------------------------------------
static void run_synthetic() {
    static const AtIoKeyword words[] = {{60, "RumbleEffects"}, {61, "Effect"}, {62, "A1_B"}, {63, "VALUE"}, {64, "levels"}, {65, "Z"}};
    const unsigned n = sizeof words / sizeof words[0];
    const std::string long_id(0x3E, 'q');
    const std::string longer_id(0x3F, 'q');
    const std::string max_string = "\"" + std::string(0xFE, 's') + "\"";
    const std::string over_string = "\"" + std::string(0xFF, 's') + "\"";
    const std::string past_id(0x40, 'q');
    const std::string past_string = "\"" + std::string(0x100, 's') + "\"";
    const std::string past_escape = "\"" + std::string(0xFF, 's') + "\\n\"";
    static const char* const texts[] = {
        "", " ", "\r\n\t  ", "// only a comment", "// comment\r\n", "//\n//\n\n  //x\n 5",
        "0", "00", "007", "1", "-1", "-0", "-00", "12345", "-12345", "2147483647", "2147483648", "-2147483648", "4294967296", "99999999999999",
        "0x", "0X", "0x0", "0xFF", "0Xff", "0xabcDEF", "0x12G", "-0x10", "0xFFFFFFFF", "0x100000000", "0x1.5",
        "0.", ".", ".5", "-.5", "1.", "1.5", "-1.5", "0.1", "0.25", "3.14159", "100.001", "123456789.125", "16777217.0", "2147483648.5", "1.5.2", "1..",
        "0.1234567890123456789012345678901234567890", "1.00000000000000000000000000000000000000000000000001", "-99999.99999",
        "-", "- 1", "-x", "-\"a\"", "--1", "-{", "/", "/x", "/ /", "/5", "a/b", "1/2",
        "!", "%", "^", "&", "*", "(", ")", "+", "=", "<", ">", "|", "?", "@", "~", "[", "]", "{", "}", ",", ";", ":",
        "[3]{}", "A[ 2 ]\r\n{\r\n\t\"x\" 1 2.5\r\n}", "{{{}}}", ",,;;::",
        "#x", "$5", "'a'", "\\y", "_z", "`1", "#\"s\"", "##5",
        "\"\"", "\"abc\"", "\"a b\tc\"", "\"line\nbreak\"", "\"unterminated", "\"", "\"a\\", "\"a\\\"b\"", "\"a\\n\\N\\t\\T\\\\\\\"\"", "\"\\q\"",
        "\"\\", "\"//not comment\"",
        "RumbleEffects", "rumbleeffects", "RUMBLEEFFECTS", "effect", "a1_b", "A1_b ", "value[", "LEVELS", "z", "Z9", "unknown", "x_1", "a-b", "a.b", "a\"b\"",
        "Effect Effect", "effect;value", "1a", "0a", "1.5a", "0x1z", "\"s\"t",
    };
    for (int escapes = 0; escapes < 2; escapes++) {
        for (int table = 0; table < 2; table++) {
            for (const char* t : texts) {
                Options o;
                o.escapes = escapes;
                o.table = table ? words : nullptr;
                o.count = table ? n : 0;
                o.stop_at_syntax_error = false;
                std::string name = "synthetic [";
                name += t;
                name += "]";
                lex_both(name, bytes(t), o);
            }
        }
    }
    for (const std::string& t : {long_id, longer_id, past_id, max_string, over_string, past_string, past_escape}) {
        Options o;
        o.escapes = 1;
        o.table = words;
        o.count = n;
        o.stop_at_syntax_error = false;
        lex_both("long " + t.substr(0, 4), bytes(t), o);
    }
    // Tokens across the buffer boundary, and every read behaviour.
    std::string across(0x3FE, ' ');
    across += "VALUE 0x12 -1.25 \"str\" // c\r\n 7";
    std::string across_comment = std::string(0x3F0, ' ') + "// a comment past the end of the buffer" + std::string(0x500, 'c') + "\n8 \"s\"";
    for (int mode = kNormal; mode <= kWaitFails; mode++) {
        for (const std::string* t : {&across, &across_comment}) {
            Options o;
            o.mode = (Mode)mode;
            o.table = words;
            o.count = n;
            o.stop_at_syntax_error = false;
            lex_both("mode " + std::to_string(mode), bytes(*t), o);
        }
    }
    // A token handed back.
    Options back;
    back.table = words;
    back.count = n;
    back.pushed_back_at = 2;
    lex_both("pushed back", bytes("1 2 3 4"), back);
    // Random soup: anything printable, with blanks and newlines.
    static const char soup[] = "0123456789abzABZ_xX.-/\\\"'{}[]()!%^&*+=<>|?@~,;:# \t\r\n";
    uint32_t state = 0x1234567;
    for (int round = 0; round < 1500; round++) {
        std::string s;
        const int len = 1 + round % 200;
        for (int i = 0; i < len; i++) {
            state = state * 1664525u + 1013904223u;
            s += soup[(state >> 16) % (sizeof soup - 1)];
        }
        Options o;
        o.escapes = round & 1;
        o.table = (round & 2) ? words : nullptr;
        o.count = (round & 2) ? n : 0;
        lex_both("soup " + std::to_string(round), bytes(s), o);
    }
}

// ---- the expectation helpers ------------------------------------------------------------------------------------------
typedef int(__fastcall* FnI)(void*, void*);
typedef float(__fastcall* FnF)(void*, void*);
typedef const char*(__fastcall* FnS)(void*, void*);
typedef const char*(__fastcall* FnSU)(void*, void*, unsigned);
typedef void(__fastcall* FnP)(void*, void*, void*);
typedef void(__fastcall* FnTok)(void*, void*, int);

static void run_expect() {
    static const char* const inputs[] = {"7", "2.5", "\"str\"", "[", "]", "{", "}", ",", "\"12345678\"", "\"123456789\"", "\"abcd\"", "\"abcde\"", "\"\"", "x", "", "[ 5 ] {", "[ x", "5 ] {", "[ 5 5 {", "[ 5 ] ]"};
    for (const char* in : inputs) {
        const std::vector<unsigned char> data = bytes(in);
        for (int which = 0; which < 14; which++) {
            g_context = std::string("expect ") + std::to_string(which) + " on " + in;
            g_fs->files[0].data = g_fs->files[1].data = data.data();
            g_fs->files[0].size = g_fs->files[1].size = data.size();
            g_fs->files[0].mode = g_fs->files[1].mode = kNormal;
            AtIoTokenFile* of = new_original_file();
            AtIoTokenFile* rf = new AtIoTokenFile;
            { On on(0); at<FnOpen4>(0x00015560)(of, nullptr, "F", "ABC", 2, nullptr); }
            { On on(1); rf->Open("F", "ABC", 2, nullptr); }
            AtIoTokenizer* o = of->m_tokenizer;
            AtIoTokenizer* r = rf->m_tokenizer;
            long long a = 0, b = 0;
            char oname[16], rname[16];
            std::memset(oname, 'z', sizeof oname);
            std::memset(rname, 'z', sizeof rname);
            char* ocopy = nullptr;
            char* rcopy = nullptr;
            const auto run = [&](int side) {
                AtIoTokenizer* t = side ? r : o;
                long long& out = side ? b : a;
                char* name = side ? rname : oname;
                On on(side);
                switch (which) {
                case 0: out = side ? t->ExpectInt() : at<FnI>(0x00015880)(t, nullptr); break;
                case 1: { float f = side ? t->ExpectReal() : at<FnF>(0x000158A0)(t, nullptr); out = *(uint32_t*)&f; break; }
                case 2: out = side ? (intptr_t)t->ExpectString() - (intptr_t)t : (intptr_t)at<FnS>(0x000158C0)(t, nullptr) - (intptr_t)t; break;
                case 3: out = side ? (intptr_t)t->ExpectString(4u) - (intptr_t)t : (intptr_t)at<FnSU>(0x000158E0)(t, nullptr, 4) - (intptr_t)t; break;
                case 4: if (side) t->ExpectStringCopy(&rcopy); else at<FnP>(0x00015920)(t, nullptr, &ocopy); break;
                case 5: if (side) t->ExpectName(name); else at<FnP>(0x00015990)(t, nullptr, name); break;
                case 6: if (side) t->ExpectToken(AtIoTokenizer::kInt); else at<FnTok>(0x000159D0)(t, nullptr, AtIoTokenizer::kInt); break;
                case 7: if (side) t->ExpectLeftBracket(); else at<Fn0>(0x000159F0)(t, nullptr); break;
                case 8: if (side) t->ExpectRightBracket(); else at<Fn0>(0x00015A10)(t, nullptr); break;
                case 9: if (side) t->ExpectLeftCurly(); else at<Fn0>(0x00015A30)(t, nullptr); break;
                case 10: if (side) t->ExpectRightCurly(); else at<Fn0>(0x00015A50)(t, nullptr); break;
                case 11: out = side ? t->ExpectCount() : at<FnI>(0x00015A70)(t, nullptr); break;
                case 12: t->Next(); if (side) t->CopyName(name); else at<FnP>(0x00015AD0)(t, nullptr, name); break;
                case 13: t->Report(5); t->Report(99); break;
                }
            };
            run(0);
            run(1);
            eq("result", a, b);
            eq("name bytes", std::memcmp(oname, rname, sizeof oname), 0);
            if (ocopy || rcopy) {
                eqs("copy", ocopy ? ocopy : "", rcopy ? rcopy : "");
                std::free(ocopy);
                std::free(rcopy);
            }
            same_tokens("after", (AtIoTextTokenizer*)o, (AtIoTextTokenizer*)r);
            compare_logs("expect");
            { On on(0); delete_original_file(of); }
            { On on(1); rf->~AtIoTokenFile(); } ::operator delete(rf);
            compare_logs("expect close");
        }
    }
    // A file name longer than the message keeps, and an extension of each length: the message is the only thing that shows them.
    {
        static const unsigned char nothing[] = "x";
        g_fs->files[0].data = g_fs->files[1].data = nothing;
        g_fs->files[0].size = g_fs->files[1].size = 1;
        const std::string longer(0x70, 'N');
        for (const char* ext : {"", "A", "AB", "ABC", "ABCD"}) {
            g_context = std::string("message, extension ") + ext;
            AtIoTokenFile* of = new_original_file();
            AtIoTokenFile* rf = new AtIoTokenFile;
            { On on(0); at<FnOpen4>(0x00015560)(of, nullptr, longer.c_str(), ext, 2, nullptr); }
            { On on(1); rf->Open(longer.c_str(), ext, 2, nullptr); }
            compare_logs("open");
            eq("extension", std::memcmp(of->m_tokenizer->m_extension, rf->m_tokenizer->m_extension, 4), 0);
            { On on(0); of->m_tokenizer->Error(4); of->m_tokenizer->Error(99); }
            { On on(1); rf->m_tokenizer->Error(4); rf->m_tokenizer->Error(99); }
            compare_logs("message");
            { On on(0); delete_original_file(of); }
            { On on(1); rf->~AtIoTokenFile(); } ::operator delete(rf);
            compare_logs("close");
        }
    }
    // Error text, directly, and through a message with no file name and no stream.
    AtIoTextTokenizer rt;
    typedef const char*(__fastcall* FnErr)(void*, void*, int);
    for (int code = -2; code < 45; code++) {
        g_context = "error text " + std::to_string(code);
        eqs("text", at<FnErr>(0x00015B00)(&rt, nullptr, code), rt.ErrorText(code));
    }
}

// ---- writing ----------------------------------------------------------------------------------------------------------
enum { kPutSteps = 57 };

static void put_step(AtIoTokenizer* t, int step, const char* long_text) {
    switch (step) {
    case 0: t->PutInt(0); break;
    case 1: t->PutInt(-5); break;
    case 2: t->PutInt(2147483647); break;
    case 3: t->PutInt((int)0x80000000); break;
    case 4: t->PutReal(0.0f); break;
    case 5: t->PutReal(1.5f); break;
    case 6: t->PutReal(-2.25f); break;
    case 7: t->PutReal(0.1f); break;
    case 8: t->PutReal(123456.789f); break;
    case 9: t->PutString(""); break;
    case 10: t->PutString("a string"); break;
    case 11: t->PutComment("a comment"); break;
    case 12: t->PutComment(""); break;
    case 13: t->PutKeyword(60); break;
    case 14: t->PutKeyword(61); break;
    case 15: t->PutKeyword(999); break;
    case 16: t->PutLeftBracket(); break;
    case 17: t->PutRightBracket(); break;
    case 18: t->PutLeftCurly(); break;
    case 19: t->PutRightCurly(); break;
    case 20: t->PutComma(); break;
    case 21: t->PutSemicolon(); break;
    case 22: t->PutColon(); break;
    case 23: t->PutExclamation(); break;
    case 24: t->PutPercent(); break;
    case 25: t->PutCaret(); break;
    case 26: t->PutAmpersand(); break;
    case 27: t->PutTimes(); break;
    case 28: t->PutLeftParen(); break;
    case 29: t->PutRightParen(); break;
    case 30: t->PutMinus(); break;
    case 31: t->PutPlus(); break;
    case 32: t->PutDivide(); break;
    case 33: t->PutEquals(); break;
    case 34: t->PutLess(); break;
    case 35: t->PutGreater(); break;
    case 36: t->PutOr(); break;
    case 37: t->PutQuestion(); break;
    case 38: t->PutAt(); break;
    case 39: t->PutTilde(); break;
    case 40: t->PutNewline(); break;
    case 41: t->PutSpace(); break;
    case 42: t->PutSpaces(0); break;
    case 43: t->PutSpaces(3); break;
    case 44: t->PutTab(); break;
    case 45: t->PutTabs(0); break;
    case 46: t->PutTabs(2); break;
    case 47: t->PutName("NAME"); break;
    case 48: t->PutName("EIGHTCHR"); break;
    case 49: t->PutName(nullptr); break;
    case 50: t->PutRaw("raw text"); break;
    case 51: t->PutRaw(""); break;
    case 52: t->PutString(long_text); break;
    case 53: t->PutComment(long_text); break;
    case 54: t->PutKeyword(60); break;
    case 55: t->PutInt(7); break;
    case 56: t->PutReal(2.5f); break;
    }
}

static void dirty_put(AtIoTokenizer* t) {
    t->m_type = 0x7777;
    t->m_int = 0x7777;
    t->m_real = 77.0f;
    std::strcpy(t->m_string, "stale");
}

static void run_write() {
    static const AtIoKeyword words[] = {{60, "Alpha"}, {61, "Beta"}};
    const std::string long_text(0x180, 'L');
    for (int mode : {(int)kNormal, (int)kPendingIo}) {
        for (int shift = 0; shift < 2; shift++) {
            g_context = "write mode " + std::to_string(mode) + " shift " + std::to_string(shift);
            std::string out[2];
            g_fs->files[0].written = &out[0];
            g_fs->files[1].written = &out[1];
            g_fs->files[0].mode = g_fs->files[1].mode = (Mode)mode;
            AtIoTokenFile* of = new_original_file();
            AtIoTokenFile* rf = new AtIoTokenFile;
            int os, rs;
            { On on(0); os = at<FnOpen3>(0x00015700)(of, nullptr, "OUT.TXT", 0x21, nullptr); }
            { On on(1); rs = rf->Open("OUT.TXT", 0x21, nullptr); }
            eq("Open for writing", os, rs);
            compare_logs("Open for writing");
            AtIoTokenizer* ot = of->m_tokenizer;
            AtIoTokenizer* rt = rf->m_tokenizer;
            for (int round = 0; round < 12; round++) {  // Enough to fill the 0x400 buffer several times.
                { On on(0); ot->SetKeywords(words, 2, nullptr); }
                { On on(1); rt->SetKeywords(words, 2, nullptr); }
                for (int step = 0; step < kPutSteps; step++) {
                    dirty_put(ot);
                    dirty_put(rt);
                    { On on(0); put_step(ot, step, long_text.c_str()); }
                    { On on(1); put_step(rt, step, long_text.c_str()); }
                    g_context = "write step " + std::to_string(step) + " mode " + std::to_string(mode);
                    same_tokens("after writing", (AtIoTextTokenizer*)ot, (AtIoTextTokenizer*)rt);
                    compare_logs("writing");
                }
                if (shift) {  // Someone else moved the file: the next flush must seek back.
                    g_fs->files[0].m_position += 3;
                    g_fs->files[1].m_position += 3;
                }
                eq("file position", of->m_stream.m_filePosition, rf->m_stream.m_filePosition);
            }
            { On on(0); delete_original_file(of); }
            { On on(1); rf->~AtIoTokenFile(); } ::operator delete(rf);
            compare_logs("close after writing");
            eq("bytes written", out[0].size(), out[1].size());
            eq("written content", out[0] == out[1], 1);
            if (out[0] != out[1]) {
                size_t k = 0;
                while (k < out[0].size() && k < out[1].size() && out[0][k] == out[1][k]) k++;
                std::printf("first difference at %u of %u/%u:\n[%s]\n[%s]\n", (unsigned)k, (unsigned)out[0].size(), (unsigned)out[1].size(),
                            out[0].substr(k > 30 ? k - 30 : 0, 80).c_str(), out[1].substr(k > 30 ? k - 30 : 0, 80).c_str());
            }
            g_fs->files[0].written = g_fs->files[1].written = nullptr;
            g_fs->files[0].mode = g_fs->files[1].mode = kNormal;
        }
    }
}

// ---- file reference, stream, lifecycle --------------------------------------------------------------------------------
typedef int(__fastcall* FnRefOpen)(void*, void*, const char*, int);
typedef int(__fastcall* FnRefOpenExt)(void*, void*, const char*, const char*, int);
typedef int(__fastcall* FnRefOpenOn)(void*, void*, void*, const char*, int);

static void run_ref() {
    static const char* const paths[] = {"A", "A.B", "DIR\\A", "DIR\\A.B", "D.E\\A", "D.E\\A.B", "A\\", "A.B\\C\\D", ".X", "\\", "P\\D.E\\A", "P\\Q\\R.S"};
    for (const char* p : paths) {
        for (int present = 0; present < 2; present++) {
            for (int status = 0; status < 2; status++) {
                g_context = std::string("ref ") + p;
                g_fs->has = present != 0;
                g_fs->open_status = status ? 9 : 0;
                AtIoFileRef o, r;
                int a, b;
                { On on(0); a = at<FnRefOpenExt>(0x000189D0)(&o, nullptr, p, "EXT", 2); }
                { On on(1); b = r.OpenWithExtension(p, "EXT", 2); }
                eq("OpenWithExtension", a, b);
                compare_logs("OpenWithExtension");
                eq("fs set", o.m_fs != nullptr, r.m_fs != nullptr);
                eq("file set", o.m_file != nullptr, r.m_file != nullptr);
                // A second open over the first closes it.
                for (int flags : {0x21, 0x20, 0x01, 0x02, 0x0C}) {
                    { On on(0); a = at<FnRefOpen>(0x00018940)(&o, nullptr, p, flags); }
                    { On on(1); b = r.Open(p, flags); }
                    eq("Open with flags", a, b);
                    compare_logs("Open with flags");
                }
                { On on(0); a = at<FnRefOpenOn>(0x000188F0)(&o, nullptr, g_fs, p, 2); }
                { On on(1); b = r.OpenOn(g_fs, p, 2); }
                eq("OpenOn", a, b);
                compare_logs("OpenOn");
                if (o.m_file && r.m_file) {
                    typedef int(__fastcall * FnFlush)(void*, void*);
                    g_fs->files[0].side = 0;
                    g_fs->files[1].side = 1;
                    { On on(0); a = at<FnFlush>(0x000188E0)(&o, nullptr); }
                    { On on(1); b = r.Flush(); }
                    eq("Flush", a, b);
                }
                { On on(0); at<Fn0>(0x00018820)(&o, nullptr); }
                { On on(1); r.Close(); }
                compare_logs("Close");
                eq("cleared", (intptr_t)o.m_fs + (intptr_t)o.m_file, (intptr_t)r.m_fs + (intptr_t)r.m_file);
                o.m_fs = nullptr;
                o.m_file = nullptr;
            }
        }
    }
    g_fs->has = true;
    g_fs->open_status = 0;
}

typedef void(__fastcall* FnAlloc)(void*, void*, unsigned);
typedef void(__cdecl* FnSetHeap)(void*);
typedef int(__fastcall* FnI0)(void*, void*);
typedef void(__fastcall* FnPut)(void*, void*, char);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

// A plain stream over memory, with a text put into it by writing: the only way the base class's Refill is reached.
static void run_stream() {
    g_context = "plain stream";
    at<FnSetHeap>(0x00031590)(&g_heap_token);
    eq("heap set", *(intptr_t*)(uintptr_t)0x002C63CC, (intptr_t)&g_heap_token);
    AtIoStream::SetHeap(&g_heap_token);
    void* op = std::calloc(1, sizeof(AtIoStream));
    at<Ctor>(0x000316B0)(op, nullptr);
    AtIoStream* o = (AtIoStream*)op;
    AtIoStream* r = new AtIoStream;
    { On on(0); at<FnAlloc>(0x00031740)(o, nullptr, 0x350); }
    { On on(1); r->Allocate(0x350); }
    compare_logs("Allocate");
    same_stream("after Allocate", o, r);
    std::string text;
    uint32_t state = 77;
    for (int i = 0; i < 0x350; i++) {
        state = state * 1664525u + 1013904223u;
        static const char mix[] = "ab \t\r\n\n/x";
        text += mix[(state >> 16) % (sizeof mix - 1)];
    }
    for (char c : text) {
        { On on(0); at<FnPut>(0x00031630)(o, nullptr, c); }
        { On on(1); r->Put(c); }
    }
    same_stream("after Put", o, r);
    eq("content", std::memcmp(o->m_base, r->m_base, 0x350), 0);
    compare_logs("Put");
    // Full: writing one more refills at the end of the data, which fails with a message.
    { On on(0); at<FnPut>(0x00031630)(o, nullptr, 'q'); }
    { On on(1); r->Put('q'); }
    compare_logs("Put when full");
    o->m_limit = o->m_base + o->m_size;
    r->m_limit = r->m_base + r->m_size;
    o->m_cur = o->m_limit;
    r->m_cur = r->m_limit;
    { On on(0); at<FnPut>(0x00031630)(o, nullptr, 'q'); }
    { On on(1); r->Put('q'); }
    compare_logs("Put with nothing to refill");
    same_stream("after Put when full", o, r);
    // Rewind, then walk through it with Refill's 0x100 windows, in three ways.
    for (int how = 0; how < 3; how++) {
        o->m_cur = o->m_limit = o->m_base;
        r->m_cur = r->m_limit = r->m_base;
        for (int step = 0; step < 2000; step++) {
            int a, b;
            if (how == 0) {
                a = o->Refill();
                b = r->Refill();
                eq("Refill", a, b);
            } else if (how == 1) {
                a = at<FnI0>(0x000315A0)(o, nullptr);
                b = r->SkipLine();
                eq("SkipLine", a, b);
            } else {
                a = at<FnI0>(0x000315E0)(o, nullptr);
                b = r->SkipWhitespace();
                eq("SkipWhitespace", a, b);
                if (a && b) { o->m_cur++; r->m_cur++; }
            }
            same_stream("walking", o, r);
            if (!a || !b) break;
        }
    }
    // Allocate again over an allocation, then the three ways out.
    { On on(0); at<FnAlloc>(0x00031740)(o, nullptr, 0x20); }
    { On on(1); r->Allocate(0x20); }
    compare_logs("Allocate again");
    { On on(0); at<Fn0>(0x000316D0)(o, nullptr); }
    { On on(1); r->Free(); }
    compare_logs("Free");
    same_stream("after Free", o, r);
    eq("base cleared", (intptr_t)o->m_base, (intptr_t)r->m_base);
    { On on(0); at<FnAlloc>(0x00031740)(o, nullptr, 0x20); }
    { On on(1); r->Allocate(0x20); }
    { On on(0); at<Fn0>(0x00031700)(o, nullptr); }
    eq("vftable after destruction", *(intptr_t*)o, 0x00274A54);
    eq("cleared", (intptr_t)o->m_allocated, 0);
    { On on(1); r->Free(); }
    compare_logs("destruction");
    { On on(0); at<FnAlloc>(0x00031740)(o, nullptr, 0x20); }
    { On on(1); r->Allocate(0x20); }
    { On on(0); at<FnDel>(0x000317E0)(o, nullptr, 1); }
    { On on(1); delete r; }
    compare_logs("deleting destructor");
    // The allocation helper's failures: a heap never set, on a stream of each kind.
    AtIoStream::SetHeap(nullptr);
    *(void**)(uintptr_t)0x002C63CC = nullptr;
    void* op2 = std::calloc(1, sizeof(AtIoStream));
    at<Ctor>(0x000316B0)(op2, nullptr);
    AtIoStream r2;
    { On on(0); at<FnAlloc>(0x00031740)(op2, nullptr, 0x10); }
    { On on(1); r2.Allocate(0x10); }
    compare_logs("no heap");
    same_stream("no heap", (AtIoStream*)op2, &r2);
    std::free(((AtIoStream*)op2)->m_allocated);
    std::free(op2);
    AtIoStream::SetHeap(&g_heap_token);
    *(void**)(uintptr_t)0x002C63CC = &g_heap_token;
}

static void run_file_stream() {
    g_context = "file stream";
    static const unsigned char data[] = "some content for the stream, some content";
    g_fs->files[0].data = g_fs->files[1].data = data;
    g_fs->files[0].size = g_fs->files[1].size = sizeof data - 1;
    g_fs->files[0].mode = g_fs->files[1].mode = kNormal;
    void* op = std::calloc(1, sizeof(AtIoFileStream));
    at<Ctor>(0x000222A0)(op, nullptr);
    AtIoFileStream* o = (AtIoFileStream*)op;
    AtIoFileStream* r = new AtIoFileStream;
    eq("vftable", *(intptr_t*)o, 0x00272AF4);
    AtIoFileRef oref, rref;
    { On on(0); oref.m_fs = g_fs; g_fs->Open("F", 2, &oref.m_file); }
    { On on(1); rref.OpenOn(g_fs, "F", 2); }
    compare_logs("open");
    typedef void(__fastcall* FnAttach)(void*, void*, void*, unsigned);
    // Attached twice: the second time detaches first.
    for (int i = 0; i < 2; i++) {
        { On on(0); at<FnAttach>(0x00022450)(o, nullptr, &oref, 0x10); }
        { On on(1); r->Attach(&rref, 0x10); }
        compare_logs("Attach");
        same_stream("after Attach", o, r);
        eq("position after Attach", o->m_filePosition, r->m_filePosition);
        { On on(0); o->Refill(); o->Refill(); }
        { On on(1); r->Refill(); r->Refill(); }
        compare_logs("Refill");
        same_stream("after Refill", o, r);
        eq("position", o->m_filePosition, r->m_filePosition);
    }
    { On on(0); at<Fn0>(0x000222C0)(o, nullptr); }
    { On on(1); r->Detach(); }
    compare_logs("Detach");
    eq("detached", (intptr_t)o->m_ref, (intptr_t)r->m_ref);
    { On on(0); at<Fn0>(0x000222C0)(o, nullptr); }
    { On on(1); r->Detach(); }
    compare_logs("Detach again");
    { On on(0); at<FnAttach>(0x00022450)(o, nullptr, &oref, 0x10); }
    { On on(1); r->Attach(&rref, 0x10); }
    { On on(0); at<FnDel>(0x000224A0)(o, nullptr, 1); }
    { On on(1); delete r; }
    compare_logs("deleting destructor");
    { On on(0); g_fs->Close(oref.m_file); }
    { On on(1); rref.Close(); }
    oref.m_file = nullptr;
    oref.m_fs = nullptr;
    compare_logs("close");
}

// The values tests/native/test_at_tok.cpp expects, from the original itself.
static void run_known_reals() {
    static const unsigned char text[] = "0.1 0.7 3.14159 100.001 123456.789 0.3333333333333333333";
    static const unsigned bits[] = {0x3DCCCCCD, 0x3F333333, 0x40490FCE, 0x42C80083, 0x47F12065, 0x3EAAAAAB};
    g_context = "known reals";
    g_fs->files[0].data = text;
    g_fs->files[0].size = sizeof text - 1;
    g_fs->files[0].mode = kNormal;
    AtIoTokenFile* of = new_original_file();
    at<FnOpen4>(0x00015560)(of, nullptr, "N", "TXT", 2, nullptr);
    of->m_tokenizer->SetKeywords(nullptr, 0, nullptr);
    for (unsigned expected : bits) {
        eq("token", of->m_tokenizer->Next(), AtIoTokenizer::kReal);
        eq("bits", *(uint32_t*)&of->m_tokenizer->m_real, expected);
    }
    delete_original_file(of);
    g_log[0].n = 0;
    g_message[0][0] = 0;
}

typedef void(__cdecl* FnCopyName)(const char*, char*);
typedef void(__cdecl* FnCopyName9)(char*, const char*);
typedef void(__cdecl* FnFormatInt)(int, char*);
typedef void(__cdecl* FnFormatReal)(float, char*);

static void run_helpers() {
    g_context = "helpers";
    static const char* const names[] = {"", "A", "1234567", "12345678", "123456789", "a much longer string than eight bytes"};
    for (const char* n : names) {
        char oa[16], ra[16];
        std::memset(oa, 'z', sizeof oa);
        std::memset(ra, 'z', sizeof ra);
        at<FnCopyName>(0x00013ED0)(n, oa);
        AtCopyName(n, ra);
        eq("CopyName", std::memcmp(oa, ra, sizeof oa), 0);
        char n16[32] = {0};  // Long enough for the unconditional eight bytes.
        strncpy_s(n16, n, _TRUNCATE);
        std::memset(oa, 'z', sizeof oa);
        std::memset(ra, 'z', sizeof ra);
        at<FnCopyName9>(0x00013F90)(oa, n16);
        AtCopyName9(ra, n16);
        eq("CopyName9", std::memcmp(oa, ra, sizeof oa), 0);
    }
    char oa[16], ra[16];
    std::memset(oa, 'z', sizeof oa);
    std::memset(ra, 'z', sizeof ra);
    at<FnCopyName9>(0x00013F90)(oa, nullptr);
    AtCopyName9(ra, nullptr);
    eqs("CopyName9 null", oa, ra);
    eq("terminator", oa[8], ra[8]);
    eq("past", oa[9], ra[9]);
    for (int v : {0, 1, -1, 10, -10, 123456789, -123456789, 2147483647, (int)0x80000000}) {
        char ob[64], rb[64];
        at<FnFormatInt>(0x000206F0)(v, ob);
        AtFormatInt(v, rb);
        eqs("FormatInt", ob, rb);
    }
    for (float v : {0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 0.1f, 3.14159f, 1000000.0f, 1.0e-7f, -1.0e-7f, 16777216.0f, 1.0e15f, -0.25f, 0.0000005f, 2.5f, 3.5f, 1.0000005f}) {
        char ob[64], rb[64];
        at<FnFormatReal>(0x00020710)(v, ob);
        AtFormatReal(v, rb);
        eqs("FormatReal", ob, rb);
    }
}

// Every kind of float: all exponents the 0x20-byte output buffer of PutReal could hold, ties, and values one bit away from them.
static void run_format_real() {
    g_context = "FormatReal";
    const auto one = [](float v) {
        char ob[128], rb[128];
        at<FnFormatReal>(0x00020710)(v, ob);
        AtFormatReal(v, rb);
        if (std::strcmp(ob, rb) != 0) g_context = "FormatReal of " + std::to_string(*(const uint32_t*)&v);
        eqs("FormatReal", ob, rb);
    };
    uint32_t state = 0x9E3779B9;
    for (int i = 0; i < 300000; i++) {
        state = state * 1664525u + 1013904223u;
        uint32_t bits = state ^ (state >> 15);
        float f;
        std::memcpy(&f, &bits, 4);
        one(f);
    }
    // Exact ties at the sixth decimal, from small numerators over 2^k.
    for (int k = 7; k < 24; k++) {
        for (int n = 1; n < 2000; n++) one((float)n / (float)(1 << k));
    }
    // Carries, and values around them.
    for (float v : {0.9999995f, 0.99999949f, 9.9999995f, 99.999992f, 999999.94f, 999999.9f, 9999999.0f, 0.0000015f, 0.0000005f, 0.00000049f, 1e-30f, 1e-45f, 1e38f,
                    -0.9999995f, -1e-30f, -1e38f, 3.4028235e38f, 1.17549435e-38f}) {
        one(v);
    }
    uint32_t inf = 0x7F800000, minf = 0xFF800000, qnan = 0x7FC00000, snan = 0x7F800001;
    for (uint32_t bits : {inf, minf, qnan, snan}) {
        float f;
        std::memcpy(&f, &bits, 4);
        one(f);
    }
    g_context = "helpers";
}

static void run_lifecycle() {
    static const unsigned char data[] = "1 2 3";
    g_fs->files[0].data = g_fs->files[1].data = data;
    g_fs->files[0].size = g_fs->files[1].size = sizeof data - 1;
    g_fs->files[0].mode = g_fs->files[1].mode = kNormal;
    static const AtIoKeyword first[] = {{70, "ONE"}, {71, "TWO"}};
    static const AtIoKeyword second[] = {{72, "THREE"}, {73, "ONE"}};
    // A tokenizer of the caller's, then reopening over it, then keywords inherited from another.
    g_context = "caller's tokenizer";
    {
        void* otp = std::calloc(1, sizeof(AtIoTextTokenizer));
        at<Ctor>(0x00022500)(otp, nullptr);
        AtIoTextTokenizer* ot = (AtIoTextTokenizer*)otp;
        eq("vftable", *(intptr_t*)ot, 0x00272C78);
        AtIoTextTokenizer* rt = new AtIoTextTokenizer;
        AtIoTokenFile* of = new_original_file();
        AtIoTokenFile* rf = new AtIoTokenFile;
        for (int i = 0; i < 2; i++) {
            int a, b;
            { On on(0); a = at<FnOpen4>(0x00015560)(of, nullptr, "NAME.TXT", "ABC", 2, ot); }
            { On on(1); b = rf->Open("NAME.TXT", "ABC", 2, rt); }
            eq("Open", a, b);
            compare_logs("Open");
            eq("not owned", of->m_ownsTokenizer, rf->m_ownsTokenizer);
            eq("extension", std::memcmp(ot->m_extension, rt->m_extension, 4), 0);
            eqs("file name", ot->m_fileName, rt->m_fileName);
            { On on(0); ot->Next(); }
            { On on(1); rt->Next(); }
            same_tokens("Next", ot, rt);
            compare_logs("Next");
        }
        // No extension, on an existing tokenizer.
        { On on(0); at<FnOpen4>(0x00015560)(of, nullptr, "NAME.TXT", nullptr, 2, ot); }
        { On on(1); rf->Open("NAME.TXT", nullptr, 2, rt); }
        compare_logs("Open without extension");
        eq("empty extension", ot->m_extension[0], rt->m_extension[0]);
        // Ownership set by hand must be overwritten, and an extension left over must be cleared when there is none to set.
        { On on(0); at<Fn0>(0x000154B0)(of, nullptr); }
        { On on(1); rf->Close(); }
        compare_logs("Close before ownership");
        of->m_ownsTokenizer = rf->m_ownsTokenizer = 1;
        ot->m_stream = rt->m_stream = nullptr;
        ot->m_extension[0] = rt->m_extension[0] = 'Q';
        { On on(0); at<FnOpen4>(0x00015560)(of, nullptr, "NAME.TXT", nullptr, 2, ot); }
        { On on(1); rf->Open("NAME.TXT", nullptr, 2, rt); }
        compare_logs("Open over stale state");
        eq("ownership overwritten", of->m_ownsTokenizer, rf->m_ownsTokenizer);
        eq("extension cleared", ot->m_extension[0], rt->m_extension[0]);
        // The overload with no extension, opening again, and a failed open keeps what the first left.
        { On on(0); at<FnOpen3>(0x00015700)(of, nullptr, "OTHER", 2, ot); }
        { On on(1); rf->Open("OTHER", 2, rt); }
        compare_logs("Open, second overload");
        eqs("name", of->m_name, rf->m_name);
        for (int missing = 0; missing < 2; missing++) {
            int a, b;
            g_fs->has = missing == 0;
            g_fs->open_status = missing ? 0 : 9;
            { On on(0); a = at<FnOpen3>(0x00015700)(of, nullptr, "GONE", 2, nullptr); }
            { On on(1); b = rf->Open("GONE", 2, nullptr); }
            eq("failed Open", a, b);
            compare_logs("failed Open");
            { On on(0); a = at<FnOpen4>(0x00015560)(of, nullptr, "GONE", "ABC", 2, nullptr); }
            { On on(1); b = rf->Open("GONE", "ABC", 2, nullptr); }
            eq("failed Open with extension", a, b);
            compare_logs("failed Open with extension");
            g_fs->has = true;
            g_fs->open_status = 0;
        }
        { On on(0); of->m_tokenizer = ot; at<Fn0>(0x000154B0)(of, nullptr); }
        { On on(1); rf->m_tokenizer = rt; rf->Close(); }
        compare_logs("Close");
        eq("closed", (intptr_t)of->m_name, (intptr_t)rf->m_name);
        // A stream already attached: SetStream resets first. Reset is virtual, and clears everything.
        typedef void(__fastcall* FnSet)(void*, void*, void*);
        AtIoStream dummy[2];
        ot->m_int = rt->m_int = 5;
        at<FnSet>(0x00015D50)(ot, nullptr, &dummy[0]);
        rt->SetStream(&dummy[1]);
        ot->m_int = rt->m_int = 5;
        eq("set", (intptr_t)ot->m_stream, (intptr_t)&dummy[0]);
        at<FnSet>(0x00015D50)(ot, nullptr, &dummy[0]);
        rt->SetStream(&dummy[1]);
        eq("reset", ot->m_int, rt->m_int);
        ot->m_stream = nullptr;
        rt->m_stream = nullptr;
        // Keywords inherited, and lookups through them.
        void* other_o = std::calloc(1, sizeof(AtIoTextTokenizer));
        at<Ctor>(0x00022500)(other_o, nullptr);
        AtIoTextTokenizer* oo = (AtIoTextTokenizer*)other_o;
        AtIoTextTokenizer* or2 = new AtIoTextTokenizer;
        oo->SetKeywords(first, 2, nullptr);
        or2->SetKeywords(first, 2, nullptr);
        ot->SetKeywords(second, 2, oo);
        rt->SetKeywords(second, 2, or2);
        eq("inherited count", ot->m_keywordCount, rt->m_keywordCount);
        for (int i = 0; i < AtIoTextTokenizer::kHashSize; i++) eq("inherited hash", (intptr_t)ot->m_hash[i], (intptr_t)rt->m_hash[i]);
        eqs("identifier slot", ot->GetIdentifier(), rt->GetIdentifier());
        eq("identifier is member", ot->GetIdentifier() - (const char*)ot, rt->GetIdentifier() - (const char*)rt);
        // A full table with a colliding run: probing wraps past the end. These names hash to the last slots.
        std::vector<std::string> names;
        std::vector<AtIoKeyword> table;
        for (int i = 0; names.size() < 6 && i < 200000; i++) {
            std::string w = "K" + std::to_string(i);
            if (at<unsigned(*)(void)>(0x00011000) == nullptr) break;
            unsigned sum = 0, shift = 0;
            for (char c : w) { sum += (unsigned)((int)c - 0x30) << (shift & 0x1F); shift += 6; }
            if (sum % 0x14B == 0x14A) names.push_back(w);
        }
        table.reserve(names.size());
        for (size_t k = 0; k < names.size(); k++) table.push_back({(int)(200 + k), names[k].c_str()});
        ot->SetKeywords(table.data(), (unsigned)table.size(), nullptr);
        rt->SetKeywords(table.data(), (unsigned)table.size(), nullptr);
        for (int i = 0; i < AtIoTextTokenizer::kHashSize; i++) eq("wrapped hash", (intptr_t)ot->m_hash[i], (intptr_t)rt->m_hash[i]);
        std::vector<unsigned char> text;
        for (const std::string& n : names) { text.insert(text.end(), n.begin(), n.end()); text.push_back(' '); }
        Options o;
        o.table = table.data();
        o.count = (unsigned)table.size();
        lex_both("wrapping lookups", text, o);
        ot->~AtIoTextTokenizer();
        std::free(otp);
        delete rt;
        oo->~AtIoTextTokenizer();
        std::free(other_o);
        delete or2;
        at<Fn0>(0x00015500)(of, nullptr);
        std::free(of);
        delete rf;
        compare_logs("end");
    }
    // Base class parts, on their own.
    g_context = "base tokenizer";
    {
        void* bp = std::calloc(1, sizeof(AtIoTextTokenizer));
        at<Ctor>(0x00015D10)(bp, nullptr);
        AtIoTokenizer* b = (AtIoTokenizer*)bp;
        eq("base vftable", *(intptr_t*)b, 0x00271D58);
        AtIoTextTokenizer r;
        eqs("base identifier", b->GetIdentifier(), r.AtIoTokenizer::GetIdentifier());
        b->SetKeywords(first, 2, nullptr);  // Does nothing.
        r.AtIoTokenizer::SetKeywords(first, 2, nullptr);
        eq("keywords ignored", (intptr_t)r.m_keywords, 0);
        const auto dirty = [](AtIoTokenizer* t) {
            t->m_stream = (AtIoStream*)(uintptr_t)4;
            t->m_pushedBack = 1;
            t->m_type = 2;
            t->m_int = 3;
            t->m_real = 4.0f;
            std::memset(t->m_string, 'q', sizeof t->m_string - 1);
            t->m_stringLength = 5;
            t->m_escapes = 6;
            t->m_fileName = "file";
            std::memset(t->m_extension, 'e', 4);
        };
        const auto check = [](const char* what, const AtIoTokenizer* o, const AtIoTokenizer* r) {
            eq(what, (intptr_t)o->m_stream, (intptr_t)r->m_stream);
            eq(what, o->m_pushedBack, r->m_pushedBack);
            eq(what, o->m_type, r->m_type);
            eq(what, o->m_int, r->m_int);
            eq(what, *(const uint32_t*)&o->m_real, *(const uint32_t*)&r->m_real);
            eq(what, o->m_string[0], r->m_string[0]);
            eq(what, o->m_stringLength, r->m_stringLength);
            eq(what, o->m_escapes, r->m_escapes);
            eq(what, (intptr_t)o->m_fileName, (intptr_t)r->m_fileName);
            eq(what, o->m_extension[0], r->m_extension[0]);
        };
        dirty(b);
        dirty(&r);
        at<Fn0>(0x00015D80)(b, nullptr);
        r.AtIoTokenizer::Reset();
        check("Reset", b, &r);
        dirty(b);
        dirty(&r);
        b->Reset();
        r.Reset();
        check("Reset through the vftable", b, &r);
        b->m_int = 9;
        at<Fn0>(0x00015FF0)(b, nullptr);
        eq("destructor clears", b->m_int, 0);
        eq("destructor vftable", *(intptr_t*)b, 0x00271D58);
        b->m_int = 9;
        at<Fn0>(0x00022DE0)(b, nullptr);
        eq("derived destructor is the base's", b->m_int, 0);
        // Deleting forms, one of each.
        void* del = std::calloc(1, sizeof(AtIoTextTokenizer));
        at<Ctor>(0x00015D10)(del, nullptr);
        at<FnDel>(0x00016030)(del, nullptr, 1);
        void* keep = std::calloc(1, sizeof(AtIoTextTokenizer));
        at<Ctor>(0x00015D10)(keep, nullptr);
        at<FnDel>(0x00016030)(keep, nullptr, 0);
        std::free(keep);
        std::free(bp);
    }
    // A message with no file name and no stream. The original appends to an uninitialised buffer: only its end can be compared.
    g_context = "message without a file";
    {
        void* tp = std::calloc(1, sizeof(AtIoTextTokenizer));
        at<Ctor>(0x00022500)(tp, nullptr);
        AtIoTextTokenizer* ot = (AtIoTextTokenizer*)tp;
        AtIoTextTokenizer* rt = new AtIoTextTokenizer;
        { On on(0); ot->Error(5); }
        { On on(1); rt->Error(5); }
        const char* tail = "expected int|";
        const size_t a = std::strlen(g_message[0]), b = std::strlen(g_message[1]), t = std::strlen(tail);
        eq("tail", a >= t && !std::strcmp(g_message[0] + a - t, tail), b >= t && !std::strcmp(g_message[1] + b - t, tail));
        eqs("recovered is the bare text", g_message[1], tail);
        g_message[0][0] = g_message[1][0] = 0;
        g_log[0].n = g_log[1].n = 0;
        { On on(0); ot->Report(5); }
        { On on(1); rt->Report(5); }
        compare_logs("Report");
        std::free(tp);
        delete rt;
    }
    // No heap: both raise, and carry on with the redirected allocator.
    g_context = "no heap";
    {
        set_heaps(nullptr, nullptr);
        AtIoTokenFile* of = new_original_file();
        AtIoTokenFile* rf = new AtIoTokenFile;
        { On on(0); at<FnOpen4>(0x00015560)(of, nullptr, "N", "ABC", 2, nullptr); }
        { On on(1); rf->Open("N", "ABC", 2, nullptr); }
        compare_logs("no heap, first overload");
        { On on(0); at<FnOpen3>(0x00015700)(of, nullptr, "N", 2, nullptr); }
        { On on(1); rf->Open("N", 2, nullptr); }
        compare_logs("no heap, second overload");
        { On on(0); delete_original_file(of); }
        { On on(1); rf->~AtIoTokenFile(); } ::operator delete(rf);
        compare_logs("no heap, close");
        set_heaps(&g_heap_token, &g_heap_token);
    }
}

static bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize((size_t)n);
    const size_t got = n ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == (size_t)n;
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        char text[256];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
        DWORD ignored;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)len, &ignored, nullptr);
        std::fflush(stdout);
        ExitProcess(3);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    const char* digest_path = nullptr;
    int limit = -1;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--digest") && i + 1 < argc) digest_path = argv[++i];
        else if (!std::strcmp(argv[i], "--limit") && i + 1 < argc) limit = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_tok <default.xbe> <cases dir> [--entries FILE] [--mutate] [--limit N] [--digest FILE]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    // The C library's start-up installs its floating-point formatters here; nothing else runs that start-up in this process.
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    set_heaps(&g_heap_token, &g_heap_token);

    if (g_mutate) {
        // Positive control of the harness: the comparison must be able to fail.
        eq("deliberate", 1, 2);
    }
    g_context = "helpers"; run_helpers(); run_format_real();
    g_context = "stream"; run_stream();
    g_context = "file_stream"; run_file_stream();
    run_known_reals(); run_known_reals(); run_known_reals(); g_context = "ref"; run_ref();
    g_context = "expect"; run_expect();
    g_context = "write"; run_write();
    g_context = "lifecycle"; run_lifecycle();
    g_context = "synthetic"; run_synthetic();

    FILE* digests = nullptr;
    if (digest_path && fopen_s(&digests, digest_path, "w")) { std::printf("cannot write %s\n", digest_path); return 2; }
    int cases = 0, binary = 0, precision_differences = 0;
    unsigned long long bytes_read = 0;
    WIN32_FIND_DATAA fd;
    std::string dir = pos[1];
    HANDLE h = FindFirstFileA((dir + "\\*.bin").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { std::printf("no cases in %s\n", pos[1]); return 2; }
    do {
        if (limit >= 0 && cases >= limit) break;
        std::string name = fd.cFileName;
        std::vector<unsigned char> data;
        if (!read_file(dir + "\\" + name, data)) { std::printf("cannot read %s\n", name.c_str()); return 2; }
        KeywordSet words;
        words.build(data);
        Options o;
        Digest lockstep;
        o.table = words.table.data();
        o.count = (unsigned)words.table.size();
        o.digest = &lockstep;
        lex_both(name, data, o);
        // The same file under 64-bit x87 precision: the Xbox's setting is not established, and the original must not care.
        unsigned int cw;
        Digest p53, p64;
        _controlfp_s(&cw, _PC_53, _MCW_PC);
        digest_original(data, o.table, o.count, p53);
        _controlfp_s(&cw, _PC_64, _MCW_PC);
        digest_original(data, o.table, o.count, p64);
        _controlfp_s(&cw, _PC_53, _MCW_PC);
        if (p53.h != p64.h) {
            precision_differences++;
            std::printf("x87 precision changes the result of the original on %s\n", name.c_str());
        }
        if (digests) std::fprintf(digests, "%s %016llX %llu\n", name.c_str(), lockstep.h, lockstep.tokens);
        cases++;
        bytes_read += data.size();
        if (name[0] == 'b' && name[1] == '_') binary++;
        if (g_failures > 20) break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (digests) std::fclose(digests);
    // The one format whose keyword table is already identified: 0x00074800 passes 0x002B2940, six entries, for the rumble file.
    int rumble_keywords = 0;
    FILE* index = nullptr;
    if (limit < 0 && !fopen_s(&index, (dir + "\\index.txt").c_str(), "r") && index) {
        char line[1024];
        while (std::fgets(line, sizeof line, index)) {
            if (!std::strstr(line, "RUMBLE.RMB")) continue;
            std::vector<unsigned char> data;
            *std::strchr(line, '\t') = 0;
            if (!read_file(dir + "\\" + line, data)) { std::printf("cannot read rumble file\n"); return 2; }
            Options o;
            o.table = (const AtIoKeyword*)(uintptr_t)0x002B2940;
            o.count = 6;
            lex_both("RUMBLE.RMB with the game's table", data, o);
            // And once more, only to count what the table recognised.
            AtIoTokenFile f;
            g_fs->files[0].data = g_fs->files[1].data = data.data();
            g_fs->files[0].size = g_fs->files[1].size = data.size();
            f.Open("R", "RMB", 2, nullptr);
            f.m_tokenizer->SetKeywords(o.table, o.count, nullptr);
            for (int t = f.m_tokenizer->Next(); t != 0; t = f.m_tokenizer->Next()) rumble_keywords += t >= 0x39;
            g_log[0].n = g_log[1].n = 0;
            g_message[0][0] = g_message[1][0] = 0;
        }
        std::fclose(index);
        if (!rumble_keywords) { std::printf("RUMBLE.RMB: no keyword recognised\n"); g_failures++; }
    }
    report_entries();
    std::printf("%s: %d files (%d binary samples), %llu bytes, %llu tokens, %llu comparisons, %d differences, %d x87 precision differences, %d rumble keywords\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", cases, binary, bytes_read, g_tokens, g_checks, g_failures, precision_differences, rumble_keywords);
    return g_failures ? 1 : 0;
}
