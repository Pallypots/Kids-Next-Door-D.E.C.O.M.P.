// Differential oracle for text strings and the game text table (Windows x86, local only). See oracle_tok.cpp and docs/verification.md.
//
//   oracle_txt <default.xbe> <cases dir> [--entries FILE] [--mutate] [--limit N]
//
// Cases: every .AIS file listed in index.txt, which tools/jam.py textcases writes; and input written here.
#include <windows.h>

#include <intrin.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtText.h"
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
static AtIoContext* g_ctx[2];
static AtIoFileSystem* g_stacks[2][4];
static int g_heap_token;

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnInitU)(void*, void*, unsigned short*, unsigned);
typedef void(__fastcall* FnInitA)(void*, void*, char*, unsigned);
typedef unsigned(__fastcall* FnU0)(void*, void*);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef void(__fastcall* FnAlloc)(void*, void*, unsigned, unsigned);
typedef void(__fastcall* FnParse)(void*, void*, void*, void*);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);

static void same_text(const char* what, AtText* o, AtText* r, const void* odata, const void* rdata, unsigned unit) {
    eq(what, o->m_capacity, r->m_capacity);
    eq(what, o->m_start, r->m_start);
    eq(what, o->m_end, r->m_end);
    if (o->m_capacity == r->m_capacity && odata && rdata) eq(what, std::memcmp(odata, rdata, o->m_capacity * unit), 0);
}

// Every method through the vftable, with the same sequence on each kind and each side.
template <class T, class C> static void run_string_ops(const char* kind, uint32_t ctor, uint32_t init, uint32_t vftable, uint32_t rewind) {
    g_context = kind;
    void* mem = std::malloc(sizeof(T));
    std::memset(mem, 0xCD, sizeof(T));
    at<Fn0>(ctor)(mem, nullptr);
    T* o = (T*)mem;
    eq("vftable", *(intptr_t*)o, vftable);
    T r;
    eq("cleared", o->m_capacity + o->m_start + o->m_end + (intptr_t)o->m_data, r.m_capacity + r.m_start + r.m_end + (intptr_t)r.m_data);

    static const char* const texts[] = {"", "A", "Hello", "123", "-45", "  67x", "1.5", "-2.25", "0.1", "1e3", ".5", "abc", "99999999999", "-99999999999", "+12", "\t 8", "\n-9", "\x0b\x0c\r7", "2147483648", "3.14159"};
    for (const char* text : texts) {
        g_context = std::string(kind) + " on [" + text + "]";
        const size_t n = std::strlen(text);
        const unsigned capacity = (unsigned)n + 6;
        std::vector<C> a(capacity + 8, (C)0x7A), b(capacity + 8, (C)0x7A);
        for (size_t i = 0; i < n; i++) { a[i] = (C)text[i]; b[i] = (C)text[i]; }
        a[n] = b[n] = 0;
        const auto check = [&](const char* what) {
            same_text(what, o, &r, a.data(), b.data(), sizeof(C));
        };
        if (sizeof(C) == 2) {
            at<FnInitU>(init)(o, nullptr, (unsigned short*)a.data(), capacity);
            ((AtTextUnicode*)&r)->Init((unsigned short*)b.data(), capacity);
        } else {
            at<FnInitA>(init)(o, nullptr, (char*)a.data(), capacity);
            ((AtTextAscii*)&r)->Init((char*)b.data(), capacity);
        }
        check("Init");
        eq("ToInt", o->ToInt(), r.ToInt());
        // The original's wide float parser is swscanf, which needs C library locks that only start-up sets up, so it cannot run here and 0x0002DDC0 is not recorded.
        // The recovered one is compared with the original's narrow parser on the same text, where something parses: the same value is what it should give.
        if (*text && std::strcmp(text, "abc") != 0 && std::strcmp(text, "Hello") != 0 && std::strcmp(text, "A") != 0) {
            const float y = r.ToFloat();
            typedef double(__cdecl* FnAtof)(const char*);
            const float x = (float)at<FnAtof>(0x00126131)(text);
            eq("ToFloat", *(const uint32_t*)&x, *(const uint32_t*)&y);
            if (sizeof(C) == 1) {
                const float z = o->ToFloat();
                eq("ToFloat, narrow, through the vftable", *(const uint32_t*)&z, *(const uint32_t*)&y);
            }
        }
        for (unsigned i = 0; i < n + 1; i++) eq("CharAt", o->CharAt(i), r.CharAt(i));
        // Copies, at sizes around the length.
        for (unsigned size = 1; size < n + 4; size++) {
            char x[64], y[64];
            std::memset(x, 'q', sizeof x);
            std::memset(y, 'q', sizeof y);
            o->CopyNarrow(x, size);
            r.CopyNarrow(y, size);
            eq("CopyNarrow", std::memcmp(x, y, sizeof x), 0);
            unsigned short u[64], v[64];
            std::memset(u, 'q', sizeof u);
            std::memset(v, 'q', sizeof v);
            o->CopyWide(u, size);
            r.CopyWide(v, size);
            eq("CopyWide", std::memcmp(u, v, sizeof u), 0);
        }
        if (n) {
            o->SetChar(0, 'Z');
            r.SetChar(0, 'Z');
            check("SetChar");
            o->SetChar((unsigned)n - 1, 0x1E9);  // A kind that keeps one byte cuts it.
            r.SetChar((unsigned)n - 1, 0x1E9);
            check("SetChar high");
            unsigned short wa[64], wb[64];
            std::memset(wa, 'q', sizeof wa);
            std::memset(wb, 'q', sizeof wb);
            o->CopyWide(wa, 40);
            r.CopyWide(wb, 40);
            eq("CopyWide of a high character", std::memcmp(wa, wb, sizeof wa), 0);
        }
        o->Insert('I', 0);
        r.Insert('I', 0);
        check("Insert at the start");
        o->Insert('J', o->m_end - o->m_start);
        r.Insert('J', r.m_end - r.m_start);
        check("Insert at the end");
        o->RemoveAt(0);
        r.RemoveAt(0);
        check("RemoveAt");
        // A window that does not start at zero: every method adds m_start.
        if (o->m_end >= 2) {
            o->m_start = 1;
            r.m_start = 1;
            eq("CharAt with a start", o->CharAt(0), r.CharAt(0));
            o->SetChar(0, 'S');
            r.SetChar(0, 'S');
            check("SetChar with a start");
            o->Insert('K', 1);
            r.Insert('K', 1);
            check("Insert with a start");
            o->RemoveAt(1);
            r.RemoveAt(1);
            check("RemoveAt with a start");
            char x[64], y[64];
            std::memset(x, 'q', sizeof x);
            std::memset(y, 'q', sizeof y);
            o->CopyNarrow(x, 40);
            r.CopyNarrow(y, 40);
            eq("CopyNarrow with a start", std::memcmp(x, y, sizeof x), 0);
            eq("ComputeEnd with a start", at<FnU0>(0x000398B0)(o, nullptr), r.ComputeEnd());
            if (o->m_end >= 3) {
                o->RemoveRange(0, 0);
                r.RemoveRange(0, 0);
                check("RemoveRange with a start");
            }
            o->m_start = 0;
            r.m_start = 0;
        }
        if (o->m_end >= 3) {
            o->RemoveRange(0, 1);
            r.RemoveRange(0, 1);
            check("RemoveRange");
        }
        // From a window that starts later: Recount goes back to the beginning.
        o->m_start = r.m_start = 1;
        o->Recount();
        r.Recount();
        check("Recount");
        eq("ComputeEnd", at<FnU0>(0x000398B0)(o, nullptr), r.ComputeEnd());
       
        for (unsigned i = 0; i < capacity; i++) { a[i] = (C)'x'; b[i] = (C)'x'; }
        eq("ComputeEnd, no terminator", at<FnU0>(0x000398B0)(o, nullptr), r.ComputeEnd());
        o->m_start = r.m_start = capacity;
        eq("ComputeEnd, empty window", at<FnU0>(0x000398B0)(o, nullptr), r.ComputeEnd());
        o->m_start = r.m_start = 0;
        if (rewind) {
            a[1] = b[1] = 0;
            o->m_start = r.m_start = 1;
            at<Fn0>(rewind)(o, nullptr);
            ((AtTextUnicode*)&r)->Rewind();
            check("Rewind");
        }
    }
    at<Fn0>(0x00031BA0)(o, nullptr);
    r.ClearFields();
    eq("ClearFields", o->m_capacity + o->m_start + o->m_end, r.m_capacity + r.m_start + r.m_end);
    eq("ClearFields keeps data", o->m_data != nullptr, r.m_data != nullptr);
    std::free(mem);
}

static void run_base_and_status() {
    g_context = "base";
    int base[4];
    std::memset(base, 0xCD, sizeof base);
    at<Fn0>(0x000398F0)(base, nullptr);
    eq("vftable", base[0], 0x0027514C);
    eq("cleared", base[1] + base[2] + base[3], 0);
    AtTextUnicode u;
    u.m_data = (unsigned short*)&base;
    at<Fn0>(0x0002DFB0)(&u, nullptr);
    AtTextUnicode u2;
    u2.m_data = (unsigned short*)&base;
    u2.Release();
    eq("Release", (intptr_t)u.m_data, (intptr_t)u2.m_data);

    g_context = "status";
    typedef const char*(__cdecl* FnStatus)(int);
    for (int status = -1; status < 24; status++) eqs("status text", at<FnStatus>(0x000140D0)(status), AtIoStatusText(status));
    typedef void(__cdecl* FnReport)(int, const char*, const char*, const char*, const char*);
    for (int status : {0, 1, 3, 9, 20, 99}) {
        { On on(0); at<FnReport>(0x00014230)(status, "%s.%s", "PATH\\NAME", "AIS", nullptr); }
        { On on(1); AtReportFileError(status, "PATH\\NAME", "AIS"); }
        compare_logs("report");
    }
}

static AtTextTable* new_original_table() {
    void* p = std::malloc(sizeof(AtTextTable));
    std::memset(p, 0xCD, sizeof(AtTextTable));
    at<Fn0>(0x0001F6B0)(p, nullptr);
    return (AtTextTable*)p;
}

static void same_table(const char* what, const AtTextTable* o, const AtTextTable* r) {
    eq(what, o->m_ascii, r->m_ascii);
    eq(what, o->m_loaded, r->m_loaded);
    eq(what, o->m_charCount, r->m_charCount);
    eq(what, o->m_charsUsed, r->m_charsUsed);
    eq(what, o->m_stringCount, r->m_stringCount);
    eq(what, o->m_stringsUsed, r->m_stringsUsed);
    eq(what, o->m_size, r->m_size);
    eq(what, o->m_chars != nullptr, r->m_chars != nullptr);
    eq(what, o->m_strings != nullptr, r->m_strings != nullptr);
    if (!o->m_loaded || !r->m_loaded || o->m_stringsUsed != r->m_stringsUsed) return;
    const unsigned unit = o->m_ascii ? 1 : 2;
    if (o->m_charsUsed == r->m_charsUsed && o->m_chars && r->m_chars && o->m_charsUsed <= o->m_charCount) eq("characters", std::memcmp(o->m_chars, r->m_chars, o->m_charsUsed * unit), 0);
    for (unsigned i = 0; i < o->m_stringsUsed; i++) {
        AtText* a = (AtText*)((char*)o->m_strings + i * 0x14);
        AtText* b = (AtText*)((char*)r->m_strings + i * 0x14);
        eq("string vftable", *(intptr_t*)a, o->m_ascii ? 0x002748F0 : 0x002748C0);
        eq("string capacity", a->m_capacity, b->m_capacity);
        eq("string end", a->m_end, b->m_end);
    }
    if (o->m_size != r->m_size || !o->m_entries || !r->m_entries) return;
    // A name that failed to parse is empty with stack bytes after it, and the hash reads past the terminator, so its slot depends on those bytes, in the original too.
    for (unsigned i = 0; i < r->m_size; i++) {
        if (r->m_entries[i].m_value && r->m_entries[i].m_name[0] == 0) return;
    }
    for (unsigned i = 0; i < o->m_size; i++) {
        const void* a = o->m_entries[i].m_value;
        const void* b = r->m_entries[i].m_value;
        eq("slot used", a != nullptr, b != nullptr);
        if (a && b) eq("slot is the same string", (const char*)a - (const char*)o->m_strings, (const char*)b - (const char*)r->m_strings);
        if (a && b) eq("slot name", std::strncmp(o->m_entries[i].m_name, r->m_entries[i].m_name, 8), 0);
    }
}

static unsigned long long g_strings_read;

static void run_table_text(const std::string& name, const std::string& text, bool reload) {
    g_context = name;
    const std::vector<unsigned char> data(text.begin(), text.end());
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    AtTextTable* o = new_original_table();
    AtTextTable* r = new AtTextTable;
    for (int round = 0; round < (reload ? 2 : 1); round++) {
        { On on(0); at<FnLoad>(0x0001FA70)(o, nullptr, g_ctx[0], "P\\N", 0); }
        { On on(1); r->Load(g_ctx[1], "P\\N", 0); }
        same_table("after Load", o, r);
        compare_logs("Load");
        eq("in a namespace", o->m_group != nullptr, r->m_group != nullptr);
        if (o->m_group && r->m_group) {
            eq("default group", o->m_group == &g_ctx[0]->m_defaultGroup, r->m_group == &g_ctx[1]->m_defaultGroup);
            eq("namespace name", std::memcmp(o->m_group, r->m_group, 1), 0);
            if (o->m_group != &g_ctx[0]->m_defaultGroup) eq("namespace name", std::strncmp((const char*)o->m_group, (const char*)r->m_group, 8), 0);
        }
        for (unsigned i = 0; i < o->m_stringsUsed; i++) {
            typedef void*(__fastcall* FnStr)(void*, void*, unsigned);
            eq("StringAt", (char*)at<FnStr>(0x0001F680)(o, nullptr, i) - (char*)o->m_strings, (char*)r->StringAt(i) - (char*)r->m_strings);
        }
    }
    { On on(0); at<Fn0>(0x0001FB70)(o, nullptr); }
    { On on(1); r->~AtTextTable(); }
    compare_logs("destructor");
    eq("vftable", *(intptr_t*)o, 0x00272E60);
    std::free(o);
    std::free(r);
}

static void run_table_synthetic() {
    static const char* const texts[] = {
        "{ }",
        "NameSpace \"N\" { }",
        "UnicodeChar [ 3 ] UnicodeString [ 1 ] { \"A\" 0x41 0x42 0x00 }",
        "UNICODEChar [ 3 ] UNICODEString [ 1 ] { \"A\" 65 66 0 }",
        "UnicodeChar [ 9 ] UnicodeString [ 2 ] { \"A\" 0x41 0x00 \"B\" 0x1234 0x10041 0x10000 }",
        "UnicodeString [ 2 ] UnicodeChar [ 9 ] { \"A\" 0x41 0x00 \"B\" 0x42 0x00 }",
        "NameSpace \"TEXTNAME\" UnicodeChar [ 4 ] UnicodeString [ 1 ] { \"ABCDEFGH\" 0x41 0x00 }",
        "ASCIIChar [ 6 ] ASCIIString [ 1 ] { \"A\" \"abcde\" }",
        "ASCIIString [ 2 ] ASCIIChar [ 20 ] { \"A\" \"abc\" \"B\" \"\" }",
        "NameSpace \"S\" ASCIIChar [ 20 ] ASCIIString [ 2 ] { \"A\" \"a\\nb\\tc\\\"d\\\\e\" \"B\" \"x\" }",
        "ASCIIChar [ 20 ] UnicodeChar [ 20 ] UnicodeString [ 1 ] { \"A\" 0x41 0x00 }",
        "UnicodeChar [ 20 ] ASCIIChar [ 20 ] ASCIIString [ 1 ] { \"A\" \"x\" }",
        "ASCIIString [ 1 ] ASCIIString [ 5 ] ASCIIChar [ 9 ] { \"A\" \"x\" }",
        "ASCIIString [ 0 ] ASCIIChar [ 9 ] { }",
        "other 1 2 ASCIIChar [ 9 ] other ASCIIString [ 1 ] { \"A\" \"x\" }",
        "ASCIIString [ 0 ] {",
        "ASCIIChar 9 ] ASCIIString [ 0 ] { }",
        "ASCIIChar [ x ] ASCIIString [ 0 ] { }",
        "ASCIIChar [ 9 ASCIIString [ 0 ] { }",
        "UnicodeChar 9 ] { }",
        "UnicodeChar [ x ] { }",
        "UnicodeChar [ 9 { }",
        "UnicodeString 1 ] UnicodeChar [ 9 ] { \"A\" 0x41 0x00 }",
        "UnicodeString [ x ] UnicodeChar [ 9 ] { }",
        "UnicodeString [ 1 UnicodeChar [ 9 ] { \"A\" 0x41 0x00 }",
        "ASCIIChar [ 9 ] ASCIIString [ 1 ] { 5 \"x\" }",
        "ASCIIChar [ 9 ] ASCIIString [ 1 ] { \"A\" 5 }",
        "ASCIIChar [ 9 ] ASCIIString [ 1 ] { \"123456789\" \"x\" }",
        "UnicodeChar [ 9 ] UnicodeString [ 1 ] { 5 0x41 0x00 }",
        "ASCIIChar [ 40 ] ASCIIString [ 2 ] { \"D\" \"x\" \"D\" \"y\" }",
        "UnicodeChar [ 40 ] UnicodeString [ 2 ] { \"D\" 0x41 0x00 \"D\" 0x42 0x00 }",
    };
    // The end of the file before an opening brace is a loop with no end, in both. A malformed header can consume its brace, so each input gets spare ones; and a wide string read past
    // the end of the file keeps the last value, so the spare zeros stop that too.
    for (const char* t : texts) run_table_text(std::string("text [") + t + "]", std::string(t) + " { { { { 0 0 0 0 0 0", false);
    run_table_text("reload ascii", "ASCIIChar [ 9 ] ASCIIString [ 1 ] { \"A\" \"x\" }", true);
    run_table_text("reload unicode", "UnicodeChar [ 9 ] UnicodeString [ 1 ] { \"A\" 0x41 0x00 }", true);
}

// Parse on a tokenizer the caller keeps: what it leaves on it shows. And a failed open, where both report and then dereference null.
static volatile bool g_expect_crash, g_resumed;
static volatile int g_crashes;
static CONTEXT g_resume;
static uint32_t g_resume_seh;

static __declspec(noinline) void load_expecting_crash(AtTextTable* t, int side) {
    g_resumed = false;
    g_resume_seh = __readfsdword(0);
    RtlCaptureContext(&g_resume);
    if (!g_resumed) {
        g_expect_crash = true;
        g_side = side;
        g_recording = true;
        if (side) t->Load(g_ctx[1], "MISSING", 0);
        else at<FnLoad>(0x0001FA70)(t, nullptr, g_ctx[0], "MISSING", 0);
        g_expect_crash = false;
    }
    g_recording = false;
    g_side = 0;
}

static void run_parse_direct() {
    g_context = "parse, direct";
    static const unsigned char text[] = "ASCIIChar [ 9 ] ASCIIString [ 1 ] { \"A\" \"x\" }";
    g_fs->files[0].data = g_fs->files[1].data = text;
    g_fs->files[0].size = g_fs->files[1].size = sizeof text - 1;
    AtIoTokenFile ofile_store;
    void* ofile = std::calloc(1, sizeof(AtIoTokenFile));
    at<Fn0>(0x00015460)(ofile, nullptr);
    typedef int(__fastcall* FnOpen4)(void*, void*, const char*, const char*, int, void*);
    { On on(0); at<FnOpen4>(0x00015560)(ofile, nullptr, "F", "AIS", 2, nullptr); }
    { On on(1); ofile_store.Open("F", "AIS", 2, nullptr); }
    compare_logs("open");
    AtTextTable* o = new_original_table();
    AtTextTable* r = new AtTextTable;
    AtIoTokenizer* ot = ((AtIoTokenFile*)ofile)->m_tokenizer;
    AtIoTokenizer* rt = ofile_store.m_tokenizer;
    ot->m_escapes = rt->m_escapes = 7;
    { On on(0); at<FnParse>(0x0001F810)(o, nullptr, g_ctx[0], ot); }
    { On on(1); r->Parse(g_ctx[1], rt); }
    compare_logs("Parse");
    eq("escapes off", ot->m_escapes, rt->m_escapes);
    eq("escapes off", rt->m_escapes, 0);
    same_table("after Parse", o, r);
    { On on(0); at<Fn0>(0x0001FB70)(o, nullptr); }
    { On on(1); r->~AtTextTable(); }
    { On on(0); at<Fn0>(0x00015500)(ofile, nullptr); }
    { On on(1); ofile_store.Close(); }
    g_log[0].n = g_log[1].n = 0;
    g_message[0][0] = g_message[1][0] = 0;
    std::free(o);
    std::free(ofile);
    std::free(r);

    g_context = "failed open";
    for (int present = 0; present < 2; present++) {
        g_fs->has = present != 0;
        g_fs->open_status = present ? 9 : 0;
        AtTextTable* ot2 = new_original_table();
        AtTextTable* rt2 = new AtTextTable;
        const int before = g_crashes;
        load_expecting_crash(ot2, 0);
        load_expecting_crash(rt2, 1);
        eq("both stop", g_crashes - before, 2);
        compare_logs("failed open");
        g_fs->has = true;
        g_fs->open_status = 0;
    }
}

static void run_allocate() {
    g_context = "allocate";
    for (int kind = 0; kind < 2; kind++) {
        for (unsigned strings : {0u, 1u, 7u}) {
            for (int heap = 1; heap >= 0; heap--) {
                *(void**)(uintptr_t)0x002C62F8 = heap ? &g_heap_token : nullptr;
                AtTextTable::s_heap = heap ? &g_heap_token : nullptr;
                AtTextTable* o = new_original_table();
                AtTextTable* r = new AtTextTable;
                o->m_ascii = r->m_ascii = kind ? 1 : 0;  // What the call must overwrite.
                { On on(0); at<FnAlloc>(kind ? 0x0001F560 : 0x0001F6E0)(o, nullptr, strings, 40); }
                { On on(1); if (kind) r->AllocateUnicode(strings, 40); else r->AllocateAscii(strings, 40); }
                same_table("allocated", o, r);
                compare_logs("allocate");
                for (unsigned i = 0; i < strings; i++) eq("string vftable", *(intptr_t*)((char*)o->m_strings + i * 0x14), *(intptr_t*)((char*)o->m_strings));
                { On on(0); o->Clear(); }
                { On on(1); r->Clear(); }
                same_table("cleared", o, r);
                compare_logs("clear");
                eq("chars null", (intptr_t)o->m_chars, (intptr_t)r->m_chars);
                eq("strings null", (intptr_t)o->m_strings, (intptr_t)r->m_strings);
                // A second clear does nothing more.
                { On on(0); o->Clear(); }
                { On on(1); r->Clear(); }
                compare_logs("clear again");
                std::free(o);
                r->~AtTextTable();
                std::free(r);
                g_log[0].n = g_log[1].n = 0;
            }
        }
    }
    *(void**)(uintptr_t)0x002C62F8 = &g_heap_token;
    AtTextTable::s_heap = &g_heap_token;
    // The constructor, on dirty memory on both sides.
    g_context = "table constructor";
    {
        AtTextTable* o = new_original_table();
        void* rm = std::malloc(sizeof(AtTextTable));
        std::memset(rm, 0xCD, sizeof(AtTextTable));
        AtTextTable* r = new (rm) AtTextTable;
        eq("vftable", *(intptr_t*)o, 0x002726B4);
        eq("zero", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtTextTable) - 4), 0);
        // Clear ignores the blocks when nothing is loaded.
        o->m_strings = r->m_strings = (void*)(uintptr_t)4;
        o->m_chars = r->m_chars = (void*)(uintptr_t)4;
        { On on(0); o->Clear(); }
        { On on(1); r->Clear(); }
        compare_logs("Clear when not loaded");
        eq("kept", (intptr_t)o->m_strings, (intptr_t)r->m_strings);
        std::free(o);
        std::free(rm);
    }
    // The vector deleting form, single and array.
    g_context = "deleting";
    void* one = std::calloc(1, sizeof(AtTextTable));
    at<Fn0>(0x0001F6B0)(one, nullptr);
    { On on(0); at<FnDel>(0x0002E510)(one, nullptr, 1); }
    AtTextTable* r1 = new AtTextTable;
    { On on(1); delete r1; }
    compare_logs("single");
    unsigned char* block = (unsigned char*)std::calloc(1, 4 + 2 * sizeof(AtTextTable));
    *(unsigned*)block = 2;
    at<Fn0>(0x0001F6B0)(block + 4, nullptr);
    at<Fn0>(0x0001F6B0)(block + 4 + sizeof(AtTextTable), nullptr);
    { On on(0); at<FnDel>(0x0002E510)(block + 4, nullptr, 3); }
    AtTextTable* r2 = new AtTextTable[2];
    { On on(1); delete[] r2; }
    compare_logs("array");
}

static void run_context() {
    g_context = "context";
    GmNameMap* m1[2] = {new GmNameMap, new GmNameMap};
    for (const char* space : {(const char*)nullptr, "SPACE"}) {
        m1[0]->Init(3);
        m1[1]->Init(3);
        { On on(0); at<FnAdd>(0x00019190)(g_ctx[0], nullptr, m1[0], space); }
        { On on(1); g_ctx[1]->AddMap(m1[1], space); }
        compare_logs("AddMap");
        eq("default group", m1[0]->m_group == &g_ctx[0]->m_defaultGroup, m1[1]->m_group == &g_ctx[1]->m_defaultGroup);
        eq("in a namespace", std::memcmp(m1[0]->m_group, m1[1]->m_group, 8), 0);
        eq("in a group", m1[0]->m_group != nullptr, m1[1]->m_group != nullptr);
    }
    m1[0]->Clear();
    m1[1]->Clear();
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (g_expect_crash) {
            g_expect_crash = false;
            g_resumed = true;
            g_crashes = g_crashes + 1;
            __writefsdword(0, g_resume_seh);
            *e->ContextRecord = g_resume;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p address 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, (void*)e->ExceptionRecord->ExceptionInformation[1], g_context.c_str());
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
    int limit = -1;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--limit") && i + 1 < argc) limit = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_txt <default.xbe> <cases dir> [--entries FILE] [--mutate] [--limit N]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();  // The C library's floating-point formatters, as in oracle_tok.cpp.
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    for (int k = 0; k < 2; k++) {
        g_ctx[k] = new AtIoContext;
        g_ctx[k]->m_stack = g_stacks[k];
        g_ctx[k]->m_stackSize = 4;
        g_ctx[k]->m_stackCount = 1;
        g_stacks[k][0] = g_fs;
    }
    *(AtIoContext**)(uintptr_t)0x002C6278 = g_ctx[0];
    AtIoFileRef::s_context = g_ctx[1];
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C62F8u, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    AtTextTable::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    for (int k = 0; k < 2; k++) {
        typedef void(__fastcall* FnInit)(void*, void*, unsigned);
        if (k == 0) at<FnInit>(0x00018060)(&g_ctx[0]->m_spaces, nullptr, 64);
        else g_ctx[1]->m_spaces.Init(64);
    }
    g_fill = 0xCD;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_base_and_status();
    run_string_ops<AtTextUnicode, unsigned short>("unicode", 0x0002DD90, 0x0002DDE0, 0x002748C0, 0x0002DD70);
    run_string_ops<AtTextAscii, unsigned char>("ascii", 0x0002E000, 0x0002E040, 0x002748F0, 0);
    run_allocate();
    run_context();
    run_table_synthetic();
    run_parse_direct();

    int files = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".AIS\t")) continue;
        if (limit >= 0 && files >= limit) break;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        AtTextTable* o = new_original_table();
        AtTextTable* r = new AtTextTable;
        g_context = line;
        g_fs->files[0].data = g_fs->files[1].data = data.data();
        g_fs->files[0].size = g_fs->files[1].size = data.size();
        { On on(0); at<FnLoad>(0x0001FA70)(o, nullptr, g_ctx[0], "GameText", 0); }
        { On on(1); r->Load(g_ctx[1], "GameText", 0); }
        same_table("after Load", o, r);
        compare_logs("Load");
        g_strings_read += r->m_stringsUsed;
        // Each string, through both sides' lookups.
        for (unsigned i = 0; i < r->m_size; i++) {
            const char* name = r->m_entries[i].m_name;
            if (!r->m_entries[i].m_value) continue;
            char q[16] = {0};
            std::memcpy(q, name, 8);
            AtText* a = (AtText*)at<void* (__fastcall*)(void*, void*, const char*)>(0x00025150)(o, nullptr, q);
            AtText* b = (AtText*)r->Find(q);
            eq("found", a != nullptr, b != nullptr);
            if (a && b) {
                char x[600], y[600];
                a->CopyNarrow(x, sizeof x);
                b->CopyNarrow(y, sizeof y);
                eqs("text", x, y);
            }
        }
        { On on(0); at<Fn0>(0x0001FB70)(o, nullptr); }
        { On on(1); r->~AtTextTable(); }
        compare_logs("destructor");
        std::free(o);
        std::free(r);
        files++;
        if (g_failures > 20) break;
    }
    std::fclose(index);
    if (limit < 0 && files == 0) { std::printf("no .AIS files listed\n"); g_failures++; }
    report_entries();
    std::printf("%s: %d .AIS files, %llu strings, %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_strings_read, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
