// Differential oracle for the recovered JAM directory reader (Windows x86, local only). See oracle_gm.cpp and
// docs/verification.md. Runs original code from the owner's XBE beside the recovered source on real archive headers.
//
//   oracle_jam <default.xbe> <cases dir> [--entries FILE] [--mutate]
//
// Each case is <name>.bin (an archive header, or a whole small JAM2 file) and <name>.txt, lines of
//   F \t path \t offset \t size     a real file, with the offset and size tools/jam.py parsed
//   D \t path \t 0 \t 0             a real directory
//   ? \t path \t 0 \t 0             a near miss: no expected value, the two implementations must simply agree
// so a real file is checked three ways: Python parser, original code, recovered source.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoJam.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks, g_queries;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 20) {
        std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.c_str(), what, o, r);
    }
}

// thiscall through __fastcall: ecx = this, edx ignored, stack arguments and callee cleanup are the same.
template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }
static void* slot(void* obj, int k) { return (*(void***)obj)[k]; }
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnP)(void*, void*, const void*);
typedef int(__fastcall* FnRS)(void*, void*, const char*);
typedef int(__fastcall* FnRSP)(void*, void*, const char*, void*);
typedef int(__fastcall* FnRSPP)(void*, void*, const char*, void*, void*);
typedef const unsigned char*(__fastcall* FnDir)(void*, void*, const char*, unsigned*, unsigned*);
typedef const unsigned char*(__fastcall* FnLocate)(void*, void*, const char*, unsigned*, unsigned*, int*, int*);
typedef void(__fastcall* FnTableInit)(void*, void*, const char*, unsigned, unsigned);
typedef int(__fastcall* FnFullName)(void*, void*, unsigned, unsigned, char*);
typedef int(__fastcall* FnMatches)(void*, void*, const char*, const char*, const char*);

struct OInfo { int token; unsigned unk04, size, names, exts; char label[16]; };
struct OTable { const char* names; const char* exts; int a, b; unsigned nameCount, extCount; };
struct OHeader { void* vptr; OTable table; void* fs; void* files; const unsigned char* buffer; const unsigned char* root;
                 unsigned rootFiles, rootDirs; int flag; };

// Calls to helpers and to the archive file, per side, in fixed storage so that logging never allocates. g_side is 1 only
// during a recovered call, so the harness's own allocations stay out of the log.
struct Log {
    unsigned n = 0;
    uint32_t v[8192];
    void add(uint32_t a, uint32_t b = 0) {
        if (n + 2 <= 8192) { v[n++] = a; v[n++] = b; }
    }
};
static Log g_log[2];
static int g_side = 0;  // 0 original, 1 recovered
static bool g_recording = false;

static void compare_logs(const char* what) {
    if (g_log[0].n != g_log[1].n && g_failures < 4) {
        for (int k = 0; k < 2; k++) {
            std::printf("  %s log:", k ? "recovered" : "original");
            for (unsigned i = 0; i + 1 < g_log[k].n; i += 2) std::printf(" %c%X", (char)g_log[k].v[i], g_log[k].v[i + 1]);
            std::printf("\n");
        }
    }
    eq(what, g_log[0].n, g_log[1].n);
    if (g_log[0].n == g_log[1].n) {
        for (unsigned i = 0; i < g_log[0].n; i++) {
            if (g_log[0].v[i] != g_log[1].v[i]) { eq(what, g_log[0].v[i], g_log[1].v[i]); break; }
        }
    }
    g_log[0].n = g_log[1].n = 0;
}

// Fatal messages are compared as text: a recovered message must say what the original says.
static char g_message[2][4096];  // Every message and opened path since the last check, in order. Fixed storage: no allocation.
static void record_message(const char* m, int side) { if (g_recording) strncat_s(g_message[side], m, _TRUNCATE); }

// ---- helpers the original calls, redirected at run time in the mapped copy; and the recovered side's definitions
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
    *(uint8_t*)(uintptr_t)va = 0xE9;
    *(int32_t*)(uintptr_t)(va + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

static void redirect_helpers() {
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
}

class FakeRequest : public AtIoFileRequest {
public:
    int status = 0;
    void Unknown0() override {}
    void Unknown1() override {}
    int Wait() override { g_log[g_side].add('W'); return status; }
};

// A test-double archive file. Each side gets its own, and their call logs are compared.
class FakeFile : public AtIoFile {
public:
    const std::vector<unsigned char>* data = nullptr;
    int mode = 0;          // 0 immediate, 1 pending, 2 pending that then fails, 3 read fails, 4 seek fails, 5 seek fails later
    FakeRequest request;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned offset) override {
        g_log[g_side].add('S', offset);
        m_position = offset;
        return (mode == 4 || offset == 0xFFFFFFFFu) ? 3 : 0;
    }
    int Read(void* out, unsigned size, unsigned* done, AtIoFileRequest** req) override {
        g_log[g_side].add('r', size);
        if (mode == 3) return 3;
        unsigned char* to = (unsigned char*)out;
        for (unsigned i = 0; i < size; i++) {  // Past the end reads as zero: FSTA cases hold the header only.
            to[i] = m_position + i < data->size() ? (*data)[m_position + i] : 0;
        }
        m_position += size;
        if (done) *done = size;
        if (mode == 1 || mode == 2) {
            request.status = mode == 2 ? 3 : 0;
            *req = &request;
            return kPending;
        }
        return 0;
    }
    int Write(const void*, unsigned, unsigned*) override { return kUnsupported; }
    int Flush() override { return kUnsupported; }
    int Unknown8(void* p) override { g_log[g_side].add('8', (uint32_t)(uintptr_t)p); return 0x88; }
    int Release(AtIoFileRequest*) override { g_log[g_side].add('L'); return 0x99; }
};

static void check_name_table(OHeader& o, AtIoJamNameTable& r, unsigned nameCount, unsigned extCount) {
    OTable* ot = &o.table;
    eq("table.names", (uintptr_t)ot->names, (uintptr_t)r.m_names);
    eq("table.exts", (uintptr_t)ot->exts, (uintptr_t)r.m_extensions);
    eq("table.a", ot->a, r.m_unknown08);
    eq("table.b", ot->b, r.m_unknown0C);
    eq("table.nameCount", ot->nameCount, r.m_nameCount);
    eq("table.extCount", ot->extCount, r.m_extensionCount);
    char key[16];
    for (unsigned i = 0; i < nameCount; i++) {
        std::memcpy(key, r.m_names + 8 * i, 8);
        key[8] = 0;
        eq("FindName(real)", at<FnRS>(0x00082BF0)(ot, nullptr, key), r.FindName(key));
        key[0] ^= 1;  // a name that sorts elsewhere and is almost surely absent
        eq("FindName(near)", at<FnRS>(0x00082BF0)(ot, nullptr, key), r.FindName(key));
    }
    const char* strange[] = {"", "A", "ZZZZZZZZ", "0", "~", "AAAAAAAAA"};
    for (const char* k : strange) {
        eq("FindName(strange)", at<FnRS>(0x00082BF0)(ot, nullptr, k), r.FindName(k));
        eq("FindExtension(strange)", at<FnRS>(0x00082C20)(ot, nullptr, k), r.FindExtension(k));
    }
    eq("FindExtension(null)", at<FnRS>(0x00082C20)(ot, nullptr, nullptr), r.FindExtension(nullptr));
    for (unsigned i = 0; i < extCount; i++) {
        std::memcpy(key, r.m_extensions + 4 * i, 4);
        key[4] = 0;
        eq("FindExtension(real)", at<FnRS>(0x00082C20)(ot, nullptr, key), r.FindExtension(key));
    }
    for (unsigned n = 0; n < nameCount + 2; n += 1 + nameCount / 40) {
        for (unsigned x = 0; x < extCount + 2; x++) {
            char a[32], b[32];
            std::memset(a, 0x55, sizeof a);
            std::memset(b, 0x55, sizeof b);
            eq("GetFullName", at<FnFullName>(0x00082B70)(ot, nullptr, n, x, a), r.GetFullName(n, x, b));
            eq("GetFullName bytes", std::memcmp(a, b, sizeof a), 0);
            if (n < nameCount && x < extCount) {
                char fname[9], ext[5];
                std::memcpy(fname, r.m_names + 8 * n, 8); fname[8] = 0;
                std::memcpy(ext, r.m_extensions + 4 * x, 4); ext[4] = 0;
                const char* patterns[][2] = {{fname, ext}, {"*", ext}, {fname, "*"}, {"*", "*"}, {"ZZZ", ext}, {fname, "ZZZ"}, {"", ext}};
                for (auto& p : patterns) {
                    eq("Matches", at<FnMatches>(0x00082CB0)(ot, nullptr, a, p[0], p[1]), r.Matches(a, p[0], p[1]));
                }
            }
        }
    }
    eq("Matches(no dot)", at<FnMatches>(0x00082CB0)(ot, nullptr, "NODOT", "NODOT", "*"), r.Matches("NODOT", "NODOT", "*"));
    eq("Matches(empty)", at<FnMatches>(0x00082CB0)(ot, nullptr, "", "*", "*"), r.Matches("", "*", "*"));
    // No dot: the extension is the empty string at the name's end, visible only with a pattern that is not "*".
    const char* no_dot_exts[] = {"", "NOD", "NODOT", "X"};
    for (const char* e : no_dot_exts) {
        eq("Matches(no dot, ext)", at<FnMatches>(0x00082CB0)(ot, nullptr, "NODOT", "NODOT", e), r.Matches("NODOT", "NODOT", e));
    }
}

template <class R>
static void check_directory(OHeader& o, R& r, uint32_t find_dir, const std::string& path) {
    // An empty path makes IsDirectoryMissing write through its null arguments, in both implementations.
    unsigned od = 0x11111, of = 0x22222, rd = 0x11111, rf = 0x22222;
    const unsigned char* op = at<FnDir>(find_dir)(&o, nullptr, path.c_str(), &od, &of);
    const unsigned char* rp = r.FindDirectory(path.c_str(), &rd, &rf);
    eq("FindDirectory", (uintptr_t)op, (uintptr_t)rp);
    eq("FindDirectory.dirs", od, rd);
    eq("FindDirectory.files", of, rf);
    if (!path.empty()) {
        eq("IsDirectoryMissing", ((FnRS)slot(&o, 6))(&o, nullptr, path.c_str()), r.IsDirectoryMissing(path.c_str()));
    }
}

static void run_fsta(const std::string& name, const std::vector<unsigned char>& data, const std::vector<std::vector<std::string>>& queries, bool raw) {
    static OHeader o;
    OInfo oi;
    AtIoJamInfoHeader ri;
    at<Fn0>(0x0007FE40)(&oi, nullptr);
    at<FnP>(0x0007FE80)(&oi, nullptr, data.data());
    ri.Init((const char*)data.data());
    g_context = name;
    AtIoFastAlignedJamHeader* r = raw ? new AtIoFastRawJamHeader : new AtIoFastAlignedJamHeader;
    at<Fn0>(raw ? 0x0007D190 : 0x0007F650)(&o, nullptr);
    eq("ctor flag", o.flag, r->m_swappedFields);
    eq("ctor table.a", o.table.a, r->m_table.m_unknown08);

    at<FnTableInit>(0x00082C70)(&o.table, nullptr, (const char*)data.data() + 0x20, oi.names, oi.exts);
    r->m_table.Init((const char*)data.data() + 0x20, ri.m_nameCount, ri.m_extensionCount);
    const unsigned char* root = data.data() + 0x20 + 4 * (oi.exts + 2 * oi.names);  // 0x0007F8B9
    o.buffer = data.data();
    o.root = root + 4;
    o.rootDirs = root[0] + (root[1] << 8);
    o.rootFiles = root[2] + (root[3] << 8);
    r->m_buffer = const_cast<unsigned char*>(data.data());
    r->m_root = root + 4;
    r->m_rootDirs = o.rootDirs;
    r->m_rootFiles = o.rootFiles;

    check_name_table(o, r->m_table, oi.names, oi.exts);
    check_directory(o, *r, 0x0007F220, "");
    for (const auto& q : queries) {
        g_context = name + ": " + q[1];
        g_queries++;
        const char* path = q[1].c_str();
        unsigned os = 0x33333, rs = 0x33333;
        const int oo = ((int(__fastcall*)(void*, void*, const char*, unsigned*))(uintptr_t)0x0007F4B0)(&o, nullptr, path, &os);
        const int ro = g_mutate && g_queries % 97 == 0 ? -1 : r->FindFile(path, &rs);
        eq("FindFile.offset", oo, ro);
        eq("FindFile.size", os, rs);
        eq("IsFileMissing", ((FnRS)slot(&o, 7))(&o, nullptr, path), r->IsFileMissing(path));
        int ot = 0x44, rt = 0x44;
        eq("GetEntryType", ((FnRSP)slot(&o, 8))(&o, nullptr, path, &ot), r->GetEntryType(path, &rt));
        eq("GetEntryType.type", ot, rt);
        check_directory(o, *r, 0x0007F220, q[1]);
        if (q[0] == "F") {  // tools/jam.py, independently
            eq("parser offset vs original", std::atoll(q[2].c_str()), (unsigned)oo);
            eq("parser size vs original", std::atoll(q[3].c_str()), os);
        } else if (q[0] == "D") {
            eq("parser directory vs original", 1, at<FnDir>(0x0007F220)(&o, nullptr, path, &os, &os) != nullptr);
        }
    }
    // Reset, through the vftable.
    ((Fn0)slot(&o, 10))(&o, nullptr);
    r->Reset();
    eq("Reset flag", o.flag, r->m_swappedFields);
    eq("Reset root", (uintptr_t)o.root, (uintptr_t)r->m_root);
    eq("Reset rootDirs", o.rootDirs, r->m_rootDirs);
    eq("Reset rootFiles", o.rootFiles, r->m_rootFiles);
    eq("info.token", oi.token, ri.m_token);
    eq("info.unk04", oi.unk04, ri.m_unknown04);
    eq("info.size", oi.size, ri.m_headerSize);
    eq("info.names", oi.names, ri.m_nameCount);
    eq("info.exts", oi.exts, ri.m_extensionCount);
    eq("info.label", std::memcmp(oi.label, ri.m_label, 16), 0);
    // A second Init on a used object clears first. Then Clear itself.
    at<FnP>(0x0007FE80)(&oi, nullptr, data.data());
    ri.Init((const char*)data.data());
    eq("info.label(again)", std::memcmp(oi.label, ri.m_label, 16), 0);
    at<Fn0>(0x0007FE60)(&oi, nullptr);
    ri.Clear();
    eq("info.token(clear)", oi.token, ri.m_token);
    eq("info.label(clear)", std::memcmp(oi.label, ri.m_label, 16), 0);
    // A label using all 15 bytes, and a stale 16th byte: only Init's clearing touches it.
    {
        std::vector<unsigned char> crafted(data.begin(), data.begin() + 0x20);
        std::memcpy(crafted.data() + 0x0C, "ABCDEFGHIJKLMNO", 15);
        for (int stale_token = 0; stale_token < 2; stale_token++) {
            oi.token = ri.m_token = stale_token;
            oi.label[15] = ri.m_label[15] = 0x7F;
            at<FnP>(0x0007FE80)(&oi, nullptr, crafted.data());
            ri.Init((const char*)crafted.data());
            eq("info.label(full)", std::memcmp(oi.label, ri.m_label, 16), 0);
            eq("info.label[15]", oi.label[15], ri.m_label[15]);
        }
    }
    // The name table's destructor and re-Init, starting from values that differ from what they store.
    auto dirty = [&]() {
        o.table.a = r->m_table.m_unknown08 = 5;
        o.table.b = r->m_table.m_unknown0C = 6;
        o.table.nameCount = r->m_table.m_nameCount = 7;
        o.table.extCount = r->m_table.m_extensionCount = 8;
        o.table.names = r->m_table.m_names = (const char*)data.data();
        o.table.exts = r->m_table.m_extensions = (const char*)data.data();
    };
    auto same = [&](const char* what) {
        eq(what, o.table.a, r->m_table.m_unknown08);
        eq(what, o.table.b, r->m_table.m_unknown0C);
        eq(what, o.table.nameCount, r->m_table.m_nameCount);
        eq(what, o.table.extCount, r->m_table.m_extensionCount);
        eq(what, (uintptr_t)o.table.names, (uintptr_t)r->m_table.m_names);
        eq(what, (uintptr_t)o.table.exts, (uintptr_t)r->m_table.m_extensions);
    };
    dirty();
    at<FnTableInit>(0x00082C70)(&o.table, nullptr, (const char*)data.data() + 0x20, 1, 1);
    r->m_table.Init((const char*)data.data() + 0x20, 1, 1);
    same("table re-Init");
    dirty();
    at<Fn0>(0x00082C50)(&o.table, nullptr);
    r->m_table.~AtIoJamNameTable();
    same("~table");
    new (&r->m_table) AtIoJamNameTable;
    at<Fn0>(0x00082B50)(&o.table, nullptr);
    eq("table ctor nameCount", o.table.nameCount, r->m_table.m_nameCount);
    OHeader base;
    at<Fn0>(0x00082D90)(&base, nullptr);
    eq("base vptr", (uintptr_t)base.vptr, 0x00279A5C);
}

static void run_jam2(const std::string& name, const std::vector<unsigned char>& data, const std::vector<std::vector<std::string>>& queries) {
    static OHeader o;
    OInfo oi;
    at<Fn0>(0x0007FE40)(&oi, nullptr);
    at<FnP>(0x0007FE80)(&oi, nullptr, data.data());
    g_context = name;
    AtIoFastJamHeader* r = new AtIoFastJamHeader;
    at<Fn0>(0x00080390)(&o, nullptr);
    at<FnTableInit>(0x00082C70)(&o.table, nullptr, (const char*)data.data() + 0x20, oi.names, oi.exts);
    r->m_table.Init((const char*)data.data() + 0x20, oi.names, oi.exts);
    const unsigned char* root = data.data() + 0x20 + 4 * (oi.exts + 2 * oi.names);
    o.buffer = data.data();
    o.root = root + 4;
    o.rootDirs = root[0] + (root[1] << 8);
    o.rootFiles = root[2] + (root[3] << 8);
    r->m_buffer = const_cast<unsigned char*>(data.data());
    r->m_root = root + 4;
    r->m_rootDirs = o.rootDirs;
    r->m_rootFiles = o.rootFiles;
    check_name_table(o, r->m_table, oi.names, oi.exts);
    check_directory(o, *r, 0x000800A0, "");

    static FakeFile primary[2], overriding[2];
    static AtIoJamFileSystem* fside_ptrs[2] = {new AtIoJamFileSystem, new AtIoJamFileSystem};  // Never destroyed.
    #define fsides(k) (*fside_ptrs[k])
    for (int k = 0; k < 2; k++) {
        primary[k].data = overriding[k].data = &data;
        fsides(k).m_file = &primary[k];
        fsides(k).m_overrideFile = nullptr;
    }
    o.fs = &fsides(0);
    r->m_fs = &fsides(1);
    int round = 0;
    for (const auto& q : queries) {
        g_context = name + ": " + q[1];
        g_queries++;
        const char* path = q[1].c_str();
        const int oo = ((FnRS)(uintptr_t)0x00080AE0)(&o, nullptr, path);
        const int ro = g_mutate && g_queries % 97 == 0 ? -1 : r->FindRecord(path);
        eq("FindRecord", oo, ro);
        unsigned od = 7, of = 7, rd = 7, rf = 7;
        int on = 7, oe = 7, rn = 7, re = 7;
        eq("Locate", (uintptr_t)at<FnLocate>(0x00080280)(&o, nullptr, path, &od, &of, &on, &oe),
           (uintptr_t)r->Locate(path, &rd, &rf, &rn, &re));
        eq("Locate.dirs", od, rd);
        eq("Locate.files", of, rf);
        eq("Locate.name", on, rn);
        eq("Locate.ext", oe, re);
        eq("IsFileMissing", ((FnRS)slot(&o, 7))(&o, nullptr, path), r->IsFileMissing(path));
        int ot = 0x44, rt = 0x44;
        eq("GetEntryType", ((FnRSP)slot(&o, 8))(&o, nullptr, path, &ot), r->GetEntryType(path, &rt));
        eq("GetEntryType.type", ot, rt);
        unsigned oa = 7, ra = 7;
        eq("GetRecordOffset", ((FnRSP)(uintptr_t)0x00080C90)(&o, nullptr, path, &oa), r->GetRecordOffset(path, &ra));
        eq("GetRecordOffset.out", oa, ra);
        check_directory(o, *r, 0x000800A0, q[1]);
        // GetFileInfo through each I/O outcome, on a rotating basis. Both sides use the same fake, so call counts compare.
        if (oo != -1 || round % 16 == 0) {
            const int mode = round % 5;
            const bool use_override = round % 7 == 3;
            unsigned ooff = 9, osize = 9, roff = 9, rsize = 9;
            for (int k = 0; k < 2; k++) {
                fsides(k).m_overrideFile = use_override ? &overriding[k] : nullptr;
                primary[k].mode = overriding[k].mode = mode;
            }
            g_side = 0;
            const int ores = ((FnRSPP)(uintptr_t)0x00080BA0)(&o, nullptr, path, &ooff, &osize);
            g_side = 1;
            const int rres = r->GetFileInfo(path, &roff, &rsize);
            g_side = 0;
            eq("GetFileInfo", ores, rres);
            eq("GetFileInfo.offset", ooff, roff);
            eq("GetFileInfo.size", osize, rsize);
            compare_logs("GetFileInfo calls");
            if (q[0] == "F" && mode == 0) {  // tools/jam.py read the record's size independently.
                eq("parser size vs original", std::atoll(q[3].c_str()), osize);
            }
            for (int k = 0; k < 2; k++) primary[k].mode = overriding[k].mode = 0;
            round++;
        }
        if (q[0] == "F") {
            eq("parser offset vs original", std::atoll(q[2].c_str()), (unsigned)oo);
        }
    }
}

struct OFile {
    void* vptr;
    unsigned position, size, sizeCopy;
    int flags;
    char path[0x80];
    int u94, u98;
    void* owner;
    unsigned base, second;
};

typedef void*(__fastcall* FnP1)(void*, void*, int);
typedef void(__fastcall* FnFs)(void*, void*, void*);
typedef int(__fastcall* FnSeek)(void*, void*, unsigned);
typedef int(__fastcall* FnOpen)(void*, void*, const char*, const char*, int);
typedef int(__fastcall* FnRead)(void*, void*, void*, unsigned, unsigned*, void**);
typedef int(__fastcall* FnWrite)(void*, void*, const void*, unsigned, unsigned*);
typedef void*(__fastcall* FnOpenFile)(void*, void*);
typedef int(__fastcall* FnClose)(void*, void*, void*);
typedef int(__fastcall* FnPtr)(void*, void*, void*);

static void same_file(const char* what, const OFile& o, const AtIoJamFile& r, const void* ohdr, const void* rhdr) {
    eq(what, o.position, r.m_position);
    eq(what, o.size, r.m_size);
    eq(what, o.sizeCopy, r.m_sizeCopy);
    eq(what, o.flags, r.m_flags);
    eq(what, std::memcmp(o.path, r.m_path, sizeof o.path), 0);
    eq(what, o.u94, r.m_unknown94);
    eq(what, o.u98, r.m_unknown98);
    eq(what, o.base, r.m_base);
    eq(what, o.second, r.m_secondSize);
    eq(what, o.owner == ohdr, (const void*)r.m_owner == rhdr);
    eq(what, o.owner == nullptr, r.m_owner == nullptr);
}

// One full lifecycle under one archive-file behaviour: vInit (twice, so the second shuts down first), files opened until
// none are left, reads, seeks, close, reopen, destruction through the deleting destructor.
template <class R>
static void run_lifecycle(const std::string& name, const std::vector<unsigned char>& data,
                          const std::vector<std::vector<std::string>>& queries, uint32_t ctor, uint32_t heap_var, uint32_t heap_setter,
                          void* heap, int init_mode, bool null_heap) {
    g_context = name + " lifecycle";
    static FakeFile device[2], other[2];
    static AtIoJamFileSystem* fs_ptr = new AtIoJamFileSystem[2];  // Never destroyed: it does not own these files.
    AtIoJamFileSystem* fs = fs_ptr;
    for (int k = 0; k < 2; k++) {
        device[k].data = other[k].data = &data;
        device[k].mode = other[k].mode = init_mode;
        fs[k].m_maxOpen = 3;
        fs[k].m_file = &device[k];
        fs[k].m_overrideFile = nullptr;
        fs[k].m_info.Init((const char*)data.data());
    }
    ((void(__cdecl*)(void*))(uintptr_t)heap_setter)(null_heap ? nullptr : heap);
    eq("heap variable", *(uintptr_t*)(uintptr_t)heap_var, (uintptr_t)(null_heap ? nullptr : heap));
    R::SetHeap(null_heap ? nullptr : heap);

    OHeader* o = (OHeader*)std::calloc(1, sizeof(OHeader));
    at<Fn0>(ctor)(o, nullptr);
    R* r = new R;
    g_recording = true;
    g_log[0].n = g_log[1].n = 0;
    for (int pass = 0; pass < 2; pass++) {
        g_side = 0;
        ((FnFs)slot(o, 1))(o, nullptr, &fs[0]);
        g_side = 1;
        r->vInit(&fs[1]);
        g_side = 0;
        compare_logs("vInit calls");
        eq("vInit message text", (long long)std::strcmp(g_message[0], g_message[1]), 0);
        g_message[0][0] = g_message[1][0] = 0;
        eq("vInit fs", o->fs == &fs[0], r->m_fs == &fs[1]);
        eq("vInit buffer bytes", std::memcmp(o->buffer, r->m_buffer, fs[0].m_info.m_headerSize), 0);
        eq("vInit root", o->root - o->buffer, r->m_root - r->m_buffer);
        eq("vInit rootDirs", o->rootDirs, r->m_rootDirs);
        eq("vInit rootFiles", o->rootFiles, r->m_rootFiles);
        eq("vInit names", (const unsigned char*)o->table.names - o->buffer, (const unsigned char*)r->m_table.m_names - r->m_buffer);
        eq("vInit exts", (const unsigned char*)o->table.exts - o->buffer, (const unsigned char*)r->m_table.m_extensions - r->m_buffer);
        eq("vInit file array cookie", ((int*)o->files)[-1], ((int*)r->m_files)[-1]);
    }
    // Shutdown called directly: its callers overwrite these fields at once, so only this shows what it leaves behind.
    {
        const uint32_t shutdown = sizeof(R) == 0x34 ? 0x00080040 : 0x0007F1A0;
        g_side = 0;
        at<Fn0>(shutdown)(o, nullptr);
        g_side = 1;
        r->Shutdown();
        g_side = 0;
        compare_logs("Shutdown calls");
        eq("Shutdown fs", o->fs == nullptr, r->m_fs == nullptr);
        eq("Shutdown files", o->files == nullptr, r->m_files == nullptr);
        eq("Shutdown buffer", o->buffer == nullptr, r->m_buffer == nullptr);
        eq("Shutdown root", o->root == nullptr, r->m_root == nullptr);
        eq("Shutdown rootDirs", o->rootDirs, r->m_rootDirs);
        eq("Shutdown rootFiles", o->rootFiles, r->m_rootFiles);
        eq("Shutdown names", o->table.names == nullptr, r->m_table.m_names == nullptr);
        eq("Shutdown nameCount", o->table.nameCount, r->m_table.m_nameCount);
        g_side = 0;
        ((FnFs)slot(o, 1))(o, nullptr, &fs[0]);
        g_side = 1;
        r->vInit(&fs[1]);
        g_side = 0;
        compare_logs("vInit after Shutdown");
        g_message[0][0] = g_message[1][0] = 0;
    }
    for (int k = 0; k < 2; k++) device[k].mode = 0;

    // Fill every slot, then run file operations on the first with each behaviour.
    OFile* of[4] = {};
    AtIoJamFile* rf[4] = {};
    for (int i = 0; i < 4; i++) {
        of[i] = (OFile*)((FnOpenFile)slot(o, 2))(o, nullptr);
        rf[i] = r->OpenFile();
        eq("OpenFile null", of[i] == nullptr, rf[i] == nullptr);
        if (of[i] && rf[i]) {
            eq("OpenFile index", ((char*)of[i] - (char*)o->files) / 0xA8, rf[i] - r->m_files);
            same_file("OpenFile state", *of[i], *rf[i], o, r);
        }
    }
    static const int kFlags[] = {0, 0x21, 1, 0x20, 4, 8, 0x40};
    unsigned step = 0;
    for (const auto& q : queries) {
        if (q[0] != "?" && (step++ % 17) != 0 && queries.size() > 80) continue;
        g_context = name + " file: " + q[1];
        const int flags = kFlags[step % 7];
        const int behaviour = step % 11 < 7 ? 0 : step % 11 - 6;
        const bool use_override = step % 13 == 5;
        for (int k = 0; k < 2; k++) {
            device[k].mode = other[k].mode = behaviour > 4 ? 0 : behaviour;
            fs[k].m_overrideFile = use_override ? &other[k] : nullptr;
        }
        g_side = 0;
        const int ostatus = ((FnOpen)slot(of[0], 2))(of[0], nullptr, q[1].c_str(), "shown name", flags);
        g_side = 1;
        const int rstatus = rf[0]->Open(q[1].c_str(), "shown name", flags);
        g_side = 0;
        eq("Open", ostatus, rstatus);
        same_file("Open state", *of[0], *rf[0], o, r);
        compare_logs("Open calls");
        for (int k = 0; k < 2; k++) device[k].mode = other[k].mode = behaviour;
        const unsigned size = rf[0]->m_size;
        const unsigned reads[] = {0, 1, 16, size / 2 + 1, size, size + 5, 1};
        for (unsigned n : reads) {
            if (n > 1 << 20) continue;
            std::vector<unsigned char> ob(n + 1, 0x5A), rb(n + 1, 0x5A);
            unsigned odone = 0x77, rdone = 0x77;
            void* oreq = nullptr;
            AtIoFileRequest* rreq = nullptr;
            const bool with_done = n != 16;
            g_side = 0;
            const int ores = ((FnRead)slot(of[0], 5))(of[0], nullptr, ob.data(), n, with_done ? &odone : nullptr, &oreq);
            g_side = 1;
            const int rres = rf[0]->Read(rb.data(), n, with_done ? &rdone : nullptr, &rreq);
            g_side = 0;
            eq("Read", ores, rres);
            eq("Read.done", odone, rdone);
            eq("Read.request", oreq != nullptr, rreq != nullptr);
            eq("Read bytes", std::memcmp(ob.data(), rb.data(), n + 1), 0);
            same_file("Read state", *of[0], *rf[0], o, r);
            compare_logs("Read calls");
            if (oreq && rreq) {
                g_side = 0;
                const int a = ((FnPtr)slot(of[0], 8))(of[0], nullptr, oreq);
                const int b = ((FnPtr)slot(of[0], 9))(of[0], nullptr, oreq);
                g_side = 1;
                const int c = rf[0]->Unknown8(rreq);
                const int d = rf[0]->Release(rreq);
                g_side = 0;
                eq("Unknown8", a, c);
                eq("Release", b, d);
                // The request's address differs by side only through the fake it came from; drop that one field.
                if (g_log[0].n == g_log[1].n && g_log[0].n >= 4) g_log[0].v[g_log[0].n - 3] = g_log[1].v[g_log[1].n - 3];
                compare_logs("forward calls");
            }
            eq("Device position", (long long)fs[0].File()->m_position, (long long)fs[1].File()->m_position);
        }
        const unsigned seeks[] = {0, size / 2, size ? size - 1 : 0, size, size + 1};
        for (unsigned t : seeks) {
            g_side = 0;
            const int ores = ((FnSeek)slot(of[0], 4))(of[0], nullptr, t);
            g_side = 1;
            const int rres = rf[0]->Seek(t);
            g_side = 0;
            eq("Seek", ores, rres);
            same_file("Seek state", *of[0], *rf[0], o, r);
            compare_logs("Seek calls");
            eq("Device position after Seek", (long long)fs[0].File()->m_position, (long long)fs[1].File()->m_position);
        }
        unsigned odone = 0x66, rdone = 0x66;
        eq("Write", ((FnWrite)slot(of[0], 6))(of[0], nullptr, "x", 1, &odone), rf[0]->Write("x", 1, &rdone));
        eq("Write.done", odone, rdone);
        eq("Flush", ((FnOpenFile)slot(of[0], 7))(of[0], nullptr) != nullptr, rf[0]->Flush() != 0);
        if (step % 3 == 2) {  // Not before Close: Clear zeroes the flags, which would hide what Close does to them.
            ((Fn0)slot(of[0], 3))(of[0], nullptr);
            rf[0]->Clear();
            same_file("Clear state", *of[0], *rf[0], o, r);
        }
        if (step % 3 == 0) {  // Close through the header, then take the slot again.
            eq("CloseFile", ((FnClose)slot(o, 3))(o, nullptr, of[0]), r->CloseFile(rf[0]));
            same_file("Close state", *of[0], *rf[0], o, r);
            eq("Close again", ((FnOpenFile)slot(of[0], 1))(of[0], nullptr) != nullptr, rf[0]->Close() != 0);
            of[0] = (OFile*)((FnOpenFile)slot(o, 2))(o, nullptr);
            rf[0] = r->OpenFile();
            eq("OpenFile after close", of[0] != nullptr, rf[0] != nullptr);
            same_file("reopen state", *of[0], *rf[0], o, r);
        } else if (step % 3 == 1) {  // Attach on an owned file closes it first.
            at<FnPtr>(0x00082830)(of[0], nullptr, o);
            rf[0]->Attach(r);
            same_file("Attach state", *of[0], *rf[0], o, r);
        }
        compare_logs("after file ops");
        for (int k = 0; k < 2; k++) { fs[k].m_overrideFile = nullptr; device[k].mode = 0; other[k].mode = 0; }
    }
    // Reads on a file that is attached but not open, and with no owner.
    g_context = name + " unopened";
    ((FnClose)slot(o, 3))(o, nullptr, of[1]);
    r->CloseFile(rf[1]);
    unsigned d1 = 1, d2 = 1;
    char byte[2];
    eq("Read(no owner)", ((FnRead)slot(of[1], 5))(of[1], nullptr, byte, 1, &d1, nullptr), rf[1]->Read(byte, 1, &d2, nullptr));
    eq("Read(no owner).done", d1, d2);
    compare_logs("before destruction");
    // Destruction through slot 0, with a file left open. Frees through the redirected helper.
    g_side = 0;
    ((FnP1)slot(o, 0))(o, nullptr, 1);
    g_side = 1;
    delete r;
    g_side = 0;
    compare_logs("destruction calls");
    g_recording = false;
}

// Base-class pieces that nothing above reaches on their own.
static void run_file_base() {
    g_context = "file base";
    char raw[0xA8];
    std::memset(raw, 0x5A, sizeof raw);
    at<Fn0>(0x00025B80)(raw, nullptr);
    FakeFile r;
    eq("AtIoFile vptr", *(uintptr_t*)raw, 0x00273004);
    eq("AtIoFile position", ((OFile*)raw)->position, r.m_position);
    eq("AtIoFile flags", ((OFile*)raw)->flags, r.m_flags);
    eq("AtIoFile u94", ((OFile*)raw)->u94, r.m_unknown94);
    eq("AtIoFile path untouched", (unsigned char)((OFile*)raw)->path[0], 0x5A);
    ((OFile*)raw)->position = ((OFile*)raw)->size = 7;
    r.m_position = r.m_size = 7;
    eq("ResetFile", ((FnOpenFile)(uintptr_t)0x00025BC0)(raw, nullptr) != nullptr, r.ResetFile() != 0);
    eq("ResetFile.position", ((OFile*)raw)->position, r.m_position);
    eq("ResetFile.size", ((OFile*)raw)->size, r.m_size);
    ((OFile*)raw)->flags = r.m_flags = 7;
    ((OFile*)raw)->sizeCopy = r.m_sizeCopy = 8;
    ((OFile*)raw)->u94 = r.m_unknown94 = 9;
    ((OFile*)raw)->u98 = r.m_unknown98 = 10;
    ((FnOpenFile)(uintptr_t)0x00025BC0)(raw, nullptr);
    r.ResetFile();
    eq("ResetFile.flags", ((OFile*)raw)->flags, r.m_flags);
    eq("ResetFile.sizeCopy", ((OFile*)raw)->sizeCopy, r.m_sizeCopy);
    eq("ResetFile.u94", ((OFile*)raw)->u94, r.m_unknown94);
    eq("ResetFile.u98", ((OFile*)raw)->u98, r.m_unknown98);
    at<Fn0>(0x00025BB0)(raw, nullptr);
    eq("~AtIoFile vptr", *(uintptr_t*)raw, 0x00273004);
}

// A file destructor's stores land in memory about to be released, so they are visible only by reading it afterwards.
template <class R>
static void run_file_destructor(uint32_t ctor, uint32_t dtor) {
    g_context = "file destructor";
    static unsigned char oraw[sizeof(OFile)], rraw[sizeof(OFile)];
    int owner_token = 0;
    for (int attached = 0; attached < 2; attached++) {
        std::memset(oraw, 0x5A, sizeof oraw);
        std::memset(rraw, 0x5A, sizeof rraw);
        at<Fn0>(ctor)(oraw, nullptr);
        R* r = new (rraw) R;
        OFile* o = (OFile*)oraw;
        o->owner = attached ? &owner_token : nullptr;
        r->m_owner = attached ? reinterpret_cast<AtIoJamDirectoryHeader*>(&owner_token) : nullptr;
        o->position = r->m_position = 1;
        o->size = r->m_size = 2;
        o->sizeCopy = r->m_sizeCopy = 3;
        o->flags = r->m_flags = 4;
        o->base = r->m_base = 5;
        o->second = r->m_secondSize = 6;
        std::memset(o->path, 'p', sizeof o->path);
        std::memset(r->m_path, 'p', sizeof r->m_path);
        at<Fn0>(dtor)(oraw, nullptr);
        r->~R();
        const OFile* after = (const OFile*)rraw;
        eq("dtor owner", o->owner == nullptr, after->owner == nullptr);
        eq("dtor position", o->position, after->position);
        eq("dtor size", o->size, after->size);
        eq("dtor sizeCopy", o->sizeCopy, after->sizeCopy);
        eq("dtor flags", o->flags, after->flags);
        eq("dtor base", o->base, after->base);
        eq("dtor second", o->second, after->second);
        eq("dtor path", std::memcmp(o->path, after->path, sizeof o->path), 0);
        eq("dtor vptr", (uintptr_t)o->vptr, 0x00273004);
    }
}

// ---- the file system layer. On x86 the recovered classes share the original's layout and calling convention, so an original
// object can be read through recovered types and driven through its own vftable with ordinary virtual calls.
static AtIoPath* orig_path(void* mem) {
    at<Fn0>(0x00018340)(mem, nullptr);
    return (AtIoPath*)mem;
}

static void same_path(const char* what, const AtIoPath& o, const AtIoPath& r) {
    eq(what, std::memcmp(&o, &r, sizeof(AtIoPath)), 0);
}

static void run_path() {
    g_context = "AtIoPath";
    static unsigned char omem[sizeof(AtIoPath)];
    std::memset(omem, 0x5A, sizeof omem);
    AtIoPath* o = orig_path(omem);
    static AtIoPath r;
    same_path("ctor", *o, r);
    typedef void(__fastcall* FnS)(void*, void*, const char*);
    typedef int(__fastcall* FnJoin)(void*, void*, char*, unsigned, const char*);
    typedef int(__fastcall* FnCur)(void*, void*, char*, unsigned);
    static char too_long[0x100];
    std::memset(too_long, 'L', sizeof too_long - 1);  // For SetRoot only, which bounds its copy: the others do not check.
    static const char* const names[] = {"", "ART", "ART\\", "A\\B", "a/b/c.txt", "DIR\\FILE.AFA", "x", "LONGERNAMEHERE\\", "/", "\\"};
    const int kNames = sizeof names / sizeof names[0];
    uint32_t rng = 12345;
    auto rnd = [&](uint32_t n) { rng = rng * 1103515245u + 12345u; return ((rng >> 16) & 0x7FFF) % n; };
    for (int step = 0; step < 20000; step++) {
        const char* name = names[rnd(kNames)];
        char a[0x200], b[0x200];
        std::memset(a, 0x33, sizeof a);
        std::memset(b, 0x33, sizeof b);
        const unsigned size = rnd(4) == 0 ? rnd(12) : 0x100;
        switch (rnd(11)) {
            case 0: at<FnS>(0x00018390)(o, nullptr, name); r.SetRoot(name); break;
            case 1: case 2: at<FnS>(0x00018630)(o, nullptr, name); r.Push(name); break;
            case 3: at<FnS>(0x000186B0)(o, nullptr, name); r.PushFromFile(name); break;
            case 4:
                if (o->m_top >= 0) {
                    const char* in = rnd(2) ? name : nullptr;
                    at<FnS>(0x00018720)(o, nullptr, in);
                    r.Pop(in);
                }
                break;
            case 5: at<FnS>(0x00018410)(o, nullptr, name); r.Resolve(name); break;
            case 6:
                eq("Join", at<FnJoin>(0x00018540)(o, nullptr, a, size, name), r.Join(b, size, name));
                eq("Join out", std::memcmp(a, b, sizeof a), 0);
                break;
            case 7:
                eq("GetCurrentPath", at<FnCur>(0x000184A0)(o, nullptr, a, size), r.GetCurrentPath(b, size));
                eq("GetCurrentPath out", std::memcmp(a, b, sizeof a), 0);
                break;
            case 8:
                eq("Current null", ((FnOpenFile)(uintptr_t)0x00018610)(o, nullptr) == nullptr, r.Current() == nullptr);
                break;
            case 9:
                if (rnd(50) == 0) { at<Fn0>(0x000187B0)(o, nullptr); r.Init(); }
                break;
            case 10:
                if (rnd(100) == 0) { at<Fn0>(0x00018760)(o, nullptr); r.Clear(); }
                if (rnd(100) == 0) { o->m_top = r.m_top = -1; }  // A state only a real Pop past the bottom reaches.
                break;
        }
    }
    same_path("after sequence", *o, r);
    at<FnS>(0x00018390)(o, nullptr, too_long);
    r.SetRoot(too_long);
    same_path("SetRoot at its limit", *o, r);
}

class FakeDevice : public AtIoFileSystem {
public:
    int side = 0;
    FakeFile* file = nullptr;
    const char* archive = "";
    int open_status = 0;
    int stat_status = 0;
    unsigned stat_time = 0x1234;
    int Open(const char* path, int flags, AtIoFile** out) override {
        g_log[side].add('O', (uint32_t)flags);
        record_message(path, side);
        file->m_position = 0;  // A real device opens at the start.
        *out = open_status ? nullptr : file;
        return open_status;
    }
    int Close(AtIoFile*) override { g_log[side].add('C'); return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char* path) override { g_log[side].add('M'); return std::strcmp(path, archive) != 0; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    int GetFileInfo(const char* path, AtIoFileStat* stat) override {
        g_log[side].add('I');
        strncpy_s(stat->m_name, path, _TRUNCATE);
        stat->m_time = stat_time;
        stat->m_offset = 0;
        stat->m_size = 0xABCDE;
        return stat_status;
    }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

static void same_fs(const char* what, const AtIoJamFileSystem& o, const AtIoJamFileSystem& r) {
    same_path(what, o.m_path, r.m_path);
    eq(what, o.m_kind, r.m_kind);
    eq(what, o.m_openFiles, r.m_openFiles);
    eq(what, o.m_maxOpen, r.m_maxOpen);
    eq(what, o.m_peakOpenFiles, r.m_peakOpenFiles);
    eq(what, (intptr_t)o.m_unknown6B4, (intptr_t)r.m_unknown6B4);
    eq(what, o.m_unknown6B8, r.m_unknown6B8);
    eq(what, o.m_info.m_token, r.m_info.m_token);
    eq(what, o.m_info.m_headerSize, r.m_info.m_headerSize);
    eq(what, o.m_header == nullptr, r.m_header == nullptr);
    eq(what, o.m_file == nullptr, r.m_file == nullptr);
    eq(what, o.m_device == nullptr, r.m_device == nullptr);
    eq(what, std::memcmp(o.m_archiveName, r.m_archiveName, sizeof o.m_archiveName), 0);
    eq(what, o.m_archiveTime, r.m_archiveTime);
    eq(what, o.m_archiveSize, r.m_archiveSize);
    eq(what, o.m_overrideFile == nullptr, r.m_overrideFile == nullptr);
    eq(what, o.m_unknown810, r.m_unknown810);
}

static int g_hook_calls;
static void __cdecl open_hook() { g_hook_calls++; }

static void run_file_system(const std::string& name, const std::vector<unsigned char>& data,
                            const std::vector<std::vector<std::string>>& queries) {
    g_context = name + " file system";
    static FakeFile file[2];
    static FakeDevice device[2];
    static AtIoContext context[2];
    static AtIoFileSystem* stack[2][4];
    static int heap_token;
    const char* archive = "Jamfiles\\Test.JAM";
    for (int k = 0; k < 2; k++) {
        file[k].data = &data;
        file[k].m_position = 0;
        file[k].mode = 0;
        device[k].side = k;
        device[k].file = &file[k];
        device[k].archive = archive;
        device[k].m_unknown6B8 = 0x6B8;
        context[k].m_stack = stack[k];
        context[k].m_stackSize = 4;
        context[k].m_stackCount = 0;
        stack[k][0] = &device[k];
        context[k].m_stackCount = 1;
    }
    *(AtIoContext**)(uintptr_t)0x002C62F0 = &context[0];
    AtIoContext::s_instance = &context[1];
    ((void(__cdecl*)(void*))(uintptr_t)0x0007DAD0)(&heap_token);
    ((void(__cdecl*)(void*))(uintptr_t)0x0007D100)(&heap_token);
    ((void(__cdecl*)(void*))(uintptr_t)0x0007F110)(&heap_token);
    ((void(__cdecl*)(void*))(uintptr_t)0x0007FFD0)(&heap_token);
    AtIoJamFileSystem::SetHeap(&heap_token);
    AtIoJamFileSystem::SetHeaderHeap(&heap_token);
    AtIoFastAlignedJamHeader::SetHeap(&heap_token);
    AtIoFastJamHeader::SetHeap(&heap_token);

    g_recording = true;
    g_log[0].n = g_log[1].n = 0;
    typedef AtIoJamFileSystem*(__stdcall* FnCreate)(const char*, unsigned, int);
    typedef void(__fastcall* FnFsInit)(void*, void*, unsigned, const char*, int);
    g_side = 0;
    AtIoJamFileSystem* o = ((FnCreate)(uintptr_t)0x0007DB10)(archive, 3, 2);
    g_side = 1;
    AtIoJamFileSystem* r = AtIoJamFileSystem::Create(archive, 3, 2);
    g_side = 0;
    compare_logs("Create calls");
    eq("Create open path", std::strcmp(g_message[0], g_message[1]), 0);
    g_message[0][0] = g_message[1][0] = 0;
    same_fs("Create state", *o, *r);
    eq("device", o->m_device == &device[0], r->m_device == &device[1]);

    // Every behaviour of the archive file through vInit again, which shuts down first.
    for (int mode = 0; mode < 3; mode++) {
        for (int k = 0; k < 2; k++) file[k].mode = mode;
        g_side = 0;
        at<FnFsInit>(0x0007D770)(o, nullptr, 3, archive, mode);
        g_side = 1;
        r->vInit(3, archive, mode);
        g_side = 0;
        for (int k = 0; k < 2; k++) file[k].mode = 0;
        compare_logs("vInit calls");
        eq("vInit message", std::strcmp(g_message[0], g_message[1]), 0);
        g_message[0][0] = g_message[1][0] = 0;
        same_fs("vInit state", *o, *r);
    }
    // A zero nMaxOpen reports, then carries on.
    for (int k = 0; k < 2; k++) device[k].stat_time = 0x77;
    g_side = 0;
    at<FnFsInit>(0x0007D770)(o, nullptr, 0, archive, 0);
    g_side = 1;
    r->vInit(0, archive, 0);
    g_side = 0;
    compare_logs("vInit(0) calls");
    eq("vInit(0) message", std::strcmp(g_message[0], g_message[1]), 0);
    g_message[0][0] = g_message[1][0] = 0;
    g_side = 0;
    at<FnFsInit>(0x0007D770)(o, nullptr, 3, archive, 0);
    g_side = 1;
    r->vInit(3, archive, 0);
    g_side = 0;
    compare_logs("vInit(3) calls");
    same_fs("vInit(3) state", *o, *r);

    *(void(__cdecl**)())(uintptr_t)0x002C62F4 = open_hook;
    AtIoContext::s_openHook = open_hook;
    unsigned step = 0;
    AtIoFile* held_o[2] = {};
    AtIoFile* held_r[2] = {};
    for (const auto& q : queries) {
        if (q[0] != "?" && (step++ % 23) != 0 && queries.size() > 80) continue;
        step++;
        g_context = name + " fs: " + q[1];
        const char* path = q[1].c_str();
        eq("IsFileMissing", o->IsFileMissing(path), r->IsFileMissing(path));
        same_path("path after IsFileMissing", o->m_path, r->m_path);
        if (*path) eq("IsDirectoryMissing", o->IsDirectoryMissing(path), r->IsDirectoryMissing(path));
        int ot = 0x44, rt = 0x44;
        eq("GetEntryType", o->GetEntryType(path, &ot), r->GetEntryType(path, &rt));
        eq("GetEntryType.type", ot, rt);
        static AtIoFileStat os, rs;
        std::memset(&os, 0x5A, sizeof os);
        std::memset(&rs, 0x5A, sizeof rs);
        g_side = 0;
        const int oi = o->GetFileInfo(path, &os);
        g_side = 1;
        const int ri = r->GetFileInfo(path, &rs);
        g_side = 0;
        eq("GetFileInfo", oi, ri);
        eq("GetFileInfo stat", std::memcmp(&os, &rs, sizeof os), 0);
        compare_logs("GetFileInfo calls");
        AtIoFile* of = reinterpret_cast<AtIoFile*>(&os);
        AtIoFile* rf = reinterpret_cast<AtIoFile*>(&os);
        const int before = g_hook_calls;
        g_side = 0;
        const int oo = o->Open(path, 0, &of);
        const int mid = g_hook_calls;
        g_side = 1;
        const int ro = r->Open(path, 0, &rf);
        g_side = 0;
        eq("Open", oo, ro);
        eq("Open out", of == nullptr, rf == nullptr);
        eq("Open hook", mid - before, g_hook_calls - mid);
        same_fs("Open state", *o, *r);
        compare_logs("Open calls");
        if (oo == 0 && ro == 0) {
            eq("Open path", std::memcmp(of->m_path, rf->m_path, sizeof of->m_path), 0);
            eq("Open size", of->m_size, rf->m_size);
            g_side = 0;
            const int oc = o->Close(of);
            g_side = 1;
            const int rc = r->Close(rf);
            g_side = 0;
            eq("Close", oc, rc);
            same_fs("Close state", *o, *r);
            compare_logs("Close calls");
        }
        if (step % 41 == 0) {  // The current directory and root take part in every path.
            o->m_path.Push("ART");
            r->m_path.Push("ART");
            eq("relative IsFileMissing", o->IsFileMissing(path), r->IsFileMissing(path));
            same_path("relative path", o->m_path, r->m_path);
            o->m_path.Pop(nullptr);
            r->m_path.Pop(nullptr);
            o->SetRoot("ignored");
            r->SetRoot("ignored");
            same_path("SetRoot is empty", o->m_path, r->m_path);
        }
    }
    *(void(__cdecl**)())(uintptr_t)0x002C62F4 = nullptr;
    AtIoContext::s_openHook = nullptr;
    (void)held_o; (void)held_r;

    // A non-empty root: the full path has it and the relative path does not, and the file's display name is the relative one.
    for (const auto& q : queries) {
        const size_t split = q[1].find('\\');
        if (q[0] != "F" || split == std::string::npos) continue;
        const std::string root = q[1].substr(0, split);
        const std::string rest = q[1].substr(split + 1);
        g_context = name + " fs under a root: " + q[1];
        o->m_path.SetRoot(root.c_str());
        r->m_path.SetRoot(root.c_str());
        AtIoFile* of = nullptr;
        AtIoFile* rf = nullptr;
        g_side = 0;
        const int oo = o->Open(rest.c_str(), 0, &of);
        g_side = 1;
        const int ro = r->Open(rest.c_str(), 0, &rf);
        g_side = 0;
        eq("Open under a root", oo, ro);
        if (oo == 0 && ro == 0) {
            eq("display name", std::memcmp(of->m_path, rf->m_path, sizeof of->m_path), 0);
            eq("display name is relative", std::strcmp(of->m_path, rest.c_str()), 0);
            g_side = 0;
            o->Close(of);
            g_side = 1;
            r->Close(rf);
            g_side = 0;
        }
        compare_logs("under a root");
        o->m_path.SetRoot("");
        r->m_path.SetRoot("");
        break;
    }

    // Slots with fixed answers, a path too long for the relative buffer's limit is out of scope: both overflow their stack.
    g_context = name + " fs slots";
    int dummy = 0;
    eq("Unknown7", o->Unknown7(&dummy), r->Unknown7(&dummy));
    eq("Unknown11", o->Unknown11(&dummy), r->Unknown11(&dummy));
    eq("Unknown12", o->Unknown12(), r->Unknown12());
    eq("Unknown14", o->Unknown14(&dummy), r->Unknown14(&dummy));
    AtIoFile* oout = reinterpret_cast<AtIoFile*>(&dummy);
    AtIoFile* rout = reinterpret_cast<AtIoFile*>(&dummy);
    eq("OpenWithCallback", o->OpenWithCallback("x", 0, nullptr, &oout), r->OpenWithCallback("x", 0, nullptr, &rout));
    eq("OpenWithCallback out", oout == nullptr, rout == nullptr);
    // Files until none are left, then a failure.
    AtIoFile* first_o = nullptr;
    AtIoFile* first_r = nullptr;
    for (const auto& q : queries) {
        if (q[0] != "F") continue;
        for (int i = 0; i < 5; i++) {
            AtIoFile* of = nullptr;
            AtIoFile* rf = nullptr;
            g_side = 0;
            const int oa = o->Open(q[1].c_str(), 0, &of);
            g_side = 1;
            const int ra = r->Open(q[1].c_str(), 0, &rf);
            g_side = 0;
            eq("Open(all)", oa, ra);
            same_fs("Open(all) state", *o, *r);
            if (i == 0) { first_o = of; first_r = rf; }
        }
        break;
    }
    compare_logs("Open(all) calls");
    if (first_o && first_r) {
        g_side = 0;
        const int oc = o->Close(first_o);
        g_side = 1;
        const int rc = r->Close(first_r);
        g_side = 0;
        eq("Close(one)", oc, rc);
        same_fs("Close(one) state", *o, *r);
    }

    // Shutdown directly, an Open with no archive, a second start, and destruction with files still open.
    g_side = 0;
    at<Fn0>(0x0007D270)(o, nullptr);
    g_side = 1;
    r->Shutdown();
    g_side = 0;
    compare_logs("Shutdown calls");
    same_fs("Shutdown state", *o, *r);
    AtIoFile* none_o = nullptr;
    AtIoFile* none_r = nullptr;
    eq("Open(no archive)", o->Open("x", 0, &none_o), r->Open("x", 0, &none_r));
    g_side = 0;
    at<FnFsInit>(0x0007D770)(o, nullptr, 3, archive, 0);
    g_side = 1;
    r->vInit(3, archive, 0);
    g_side = 0;
    compare_logs("restart calls");
    same_fs("restart state", *o, *r);
    g_side = 0;
    delete o;  // Slot 0 of the original's own vftable.
    g_side = 1;
    delete r;
    g_side = 0;
    compare_logs("destruction calls");
    g_recording = false;
    g_message[0][0] = g_message[1][0] = 0;
}

// Context stack: the only part of AtIoContext recovered so far.
static void run_context() {
    g_context = "AtIoContext";
    static AtIoContext o, r;
    static AtIoFileSystem* so[3];
    static AtIoFileSystem* sr[3];
    static FakeDevice a, b, a2;
    a.archive = "A";
    b.archive = "B";
    a2.archive = "A";  // Holds the same file as `a`: which one is found depends on the search order.
    o.m_stack = so; r.m_stack = sr;
    o.m_stackSize = r.m_stackSize = 3;
    o.m_stackCount = r.m_stackCount = 0;
    typedef void(__fastcall* FnPush)(void*, void*, void*);
    typedef void*(__fastcall* FnTop)(void*, void*);
    typedef int(__fastcall* FnFind)(void*, void*, const char*, AtIoFileSystem**);
    const auto same = [&]() {
        eq("count", o.m_stackCount, r.m_stackCount);
        eq("stack", std::memcmp(so, sr, sizeof so), 0);
        eq("Top", (uintptr_t)((FnTop)(uintptr_t)0x000192E0)(&o, nullptr), (uintptr_t)r.Top());
    };
    const auto find = [&](const char* path) {
        AtIoFileSystem* fo = reinterpret_cast<AtIoFileSystem*>(&o);
        AtIoFileSystem* fr = reinterpret_cast<AtIoFileSystem*>(&o);
        eq("FindFile", ((FnFind)(uintptr_t)0x00019300)(&o, nullptr, path, &fo), r.FindFile(path, &fr));
        eq("FindFile.out", (uintptr_t)fo, (uintptr_t)fr);
        eq("FindFile(null)", ((FnFind)(uintptr_t)0x00019300)(&o, nullptr, path, nullptr), r.FindFile(path, nullptr));
    };
    find("A");
    same();
    ((Fn0)(uintptr_t)0x000192C0)(&o, nullptr); r.Pop();  // Pop on empty
    same();
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &a); r.Push(&a);
    same();
    find("A"); find("B");
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &b); r.Push(&b);
    find("A"); find("B"); find("C");  // Searched from the top down.
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &a); r.Push(&a);
    same();
    g_recording = true;
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &b); r.Push(&b);  // Full: reports.
    g_recording = false;
    g_log[0].n = g_log[1].n = 0;
    eq("full message", std::strcmp(g_message[0], g_message[1]), 0);
    g_message[0][0] = g_message[1][0] = 0;
    same();
    for (int i = 0; i < 4; i++) { ((Fn0)(uintptr_t)0x000192C0)(&o, nullptr); r.Pop(); same(); }
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &a); r.Push(&a);
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &a2); r.Push(&a2);
    find("A");
    ((FnPush)(uintptr_t)0x00019290)(&o, nullptr, &b); r.Push(&b);
    find("A"); find("B");
    AtIoFileSystem* top = nullptr;
    r.FindFile("A", &top);
    eq("top of the stack wins", (uintptr_t)top, (uintptr_t)&a2);
    g_log[0].n = g_log[1].n = 0;  // These devices log on one side only: nothing to compare, and nothing may leak into a later check.
}

// Base file system: constructor, default slots, destructor, on inspected memory.
static void run_file_system_base() {
    g_context = "AtIoFileSystem";
    static unsigned char mem[sizeof(AtIoFileSystem)];
    std::memset(mem, 0x5A, sizeof mem);
    at<Fn0>(0x0001F4D0)(mem, nullptr);
    static FakeDevice r;
    AtIoFileSystem* o = reinterpret_cast<AtIoFileSystem*>(mem);
    eq("vptr", *(uintptr_t*)mem, 0x00272644);
    eq("kind", o->m_kind, r.m_kind);
    eq("maxOpen", o->m_maxOpen, r.m_maxOpen);
    same_path("ctor path", o->m_path, r.m_path);
    AtIoFile* a = reinterpret_cast<AtIoFile*>(mem);
    AtIoFile* b = reinterpret_cast<AtIoFile*>(mem);
    eq("OpenWithCallback", o->OpenWithCallback("x", 0, nullptr, &a), r.AtIoFileSystem::OpenWithCallback("x", 0, nullptr, &b));
    eq("OpenWithCallback.out", a == nullptr, b == nullptr);
    static AtIoFileStat os, rs;
    std::memset(&os, 0x5A, sizeof os);
    std::memset(&rs, 0x5A, sizeof rs);
    eq("GetFileInfo", o->GetFileInfo("x", &os), r.AtIoFileSystem::GetFileInfo("x", &rs));
    eq("GetFileInfo stat", std::memcmp(&os, &rs, sizeof os), 0);
    o->SetRoot("ROOT");
    r.SetRoot("ROOT");
    same_path("SetRoot", o->m_path, r.m_path);
    eq("Unknown14", o->Unknown14(nullptr), r.Unknown14(nullptr));
    at<Fn0>(0x0001F460)(mem, nullptr);
    r.InitPath();
    same_path("InitPath", o->m_path, r.m_path);
    at<Fn0>(0x0001F460)(mem, nullptr);
    r.InitPath();
    same_path("InitPath twice", o->m_path, r.m_path);
    o->m_kind = r.m_kind = 3;
    o->m_openFiles = r.m_openFiles = 4;
    o->m_maxOpen = r.m_maxOpen = 5;
    o->m_peakOpenFiles = r.m_peakOpenFiles = 6;
    o->m_unknown6B4 = r.m_unknown6B4 = (void*)(uintptr_t)7;
    o->m_unknown6B8 = r.m_unknown6B8 = 8;
    at<Fn0>(0x0001F510)(mem, nullptr);
    r.ResetFileSystem();
    same_path("Reset path", o->m_path, r.m_path);
    eq("Reset kind", o->m_kind, r.m_kind);
    eq("Reset open", o->m_openFiles, r.m_openFiles);
    eq("Reset max", o->m_maxOpen, r.m_maxOpen);
    eq("Reset peak", o->m_peakOpenFiles, r.m_peakOpenFiles);
    eq("Reset 6B4", (intptr_t)o->m_unknown6B4, (intptr_t)r.m_unknown6B4);
    eq("Reset 6B8", o->m_unknown6B8, r.m_unknown6B8);
    at<Fn0>(0x0001F450)(mem, nullptr);
    eq("~vptr", *(uintptr_t*)mem, 0x00272644);
}

static bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.assign(n, 0);
    bool ok = std::fread(out.data(), 1, n, f) == (size_t)n;
    std::fclose(f);
    return ok;
}

// A crash in either implementation is a result, so say where it happened.
static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        std::printf("CRASH in %s: side %d, at 0x%08X, %s 0x%08X\n", g_context.c_str(), g_side, (unsigned)(uintptr_t)e->ExceptionRecord->ExceptionAddress,
                    e->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading", (unsigned)e->ExceptionRecord->ExceptionInformation[1]);
        std::fflush(stdout);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv) {
    AddVectoredExceptionHandler(0, on_crash);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_jam <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect_helpers();
    if (entries && !arm_entries(entries)) return 2;
    run_file_base();
    run_path();
    run_context();
    run_file_system_base();
    run_file_destructor<AtIoFastAlignedJamFile>(0x000827E0, 0x00082A20);
    run_file_destructor<AtIoFastJamFile>(0x00082DA0, 0x00083090);
    int heap_token = 0;
    WIN32_FIND_DATAA fd;
    std::string dir = pos[1];
    HANDLE h = FindFirstFileA((dir + "\\*.bin").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { std::printf("no cases in %s\n", pos[1]); return 2; }
    int cases = 0;
    do {
        std::string base = fd.cFileName;
        base.resize(base.size() - 4);
        std::vector<unsigned char> data;
        if (!read_file(dir + "\\" + base + ".bin", data)) { std::printf("cannot read %s\n", base.c_str()); return 2; }
        std::vector<std::vector<std::string>> queries;
        FILE* q = std::fopen((dir + "\\" + base + ".txt").c_str(), "r");
        if (!q) { std::printf("cannot read %s.txt\n", base.c_str()); return 2; }
        char line[1024];
        while (std::fgets(line, sizeof line, q)) {
            std::vector<std::string> parts;
            char* ctx = nullptr;
            line[std::strcspn(line, "\r\n")] = 0;
            for (char* t = strtok_s(line, "\t", &ctx); t; t = strtok_s(nullptr, "\t", &ctx)) parts.push_back(t);
            if (parts.size() == 4) queries.push_back(parts);
        }
        std::fclose(q);
        const bool jam2 = std::memcmp(data.data(), "JAM2", 4) == 0;
        const bool raw = std::memcmp(data.data(), "FSTR", 4) == 0;
        if (jam2) run_jam2(base, data, queries);
        else run_fsta(base, data, queries, raw);
        run_file_system(base, data, queries);
        // Lifecycles: the first with each archive-file behaviour, plus a heap that was never set.
        for (int mode = 0; mode <= 4; mode++) {
            for (int null_heap = 0; null_heap < (mode == 0 ? 2 : 1); null_heap++) {
                if (data.size() > (1u << 21) && mode != 0) continue;
                if (jam2) {
                    run_lifecycle<AtIoFastJamHeader>(base, data, queries, 0x00080390, 0x00301B60, 0x0007FFD0, &heap_token, mode, null_heap);
                } else if (raw) {
                    run_lifecycle<AtIoFastRawJamHeader>(base, data, queries, 0x0007D190, 0x00301B5C, 0x0007F110, &heap_token, mode, null_heap);
                } else {
                    run_lifecycle<AtIoFastAlignedJamHeader>(base, data, queries, 0x0007F650, 0x00301B5C, 0x0007F110, &heap_token, mode, null_heap);
                }
            }
        }
        cases++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    report_entries();
    std::printf("%s: %d archives, %llu paths, %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", cases, g_queries, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
