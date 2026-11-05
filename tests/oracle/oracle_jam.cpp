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

// One test-double file, shared by both implementations, so each must make the same calls in the same order.
class FakeRequest : public AtIoFileRequest {
public:
    int status = 0;
    void Unknown0() override {}
    void Unknown1() override {}
    int Wait() override { waits++; return status; }
    int waits = 0;
};

class FakeFile : public AtIoFile {
public:
    const std::vector<unsigned char>* data = nullptr;
    unsigned position = 0;
    int mode = 0;          // 0 immediate, 1 pending, 2 pending that then fails, 3 read fails, 4 seek fails
    int seeks = 0, reads = 0, releases = 0;
    FakeRequest request;
    void Unknown0() override {}
    void Unknown1() override {}
    void Unknown2() override {}
    void Unknown3() override {}
    int Seek(unsigned offset) override {
        seeks++;
        position = offset;
        return (mode == 4 || offset >= data->size()) ? 3 : 0;
    }
    int Read(void* out, unsigned size, int, AtIoFileRequest** req) override {
        reads++;
        if (mode == 3) return 3;
        std::memcpy(out, data->data() + position, size);
        if (mode == 1 || mode == 2) {
            request.status = mode == 2 ? 3 : 0;
            *req = &request;
            return kPending;
        }
        return 0;
    }
    void Unknown6() override {}
    void Unknown7() override {}
    void Unknown8() override {}
    void Release(AtIoFileRequest*) override { releases++; }
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
    r->m_buffer = data.data();
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
    r->m_buffer = data.data();
    r->m_root = root + 4;
    r->m_rootDirs = o.rootDirs;
    r->m_rootFiles = o.rootFiles;
    check_name_table(o, r->m_table, oi.names, oi.exts);
    check_directory(o, *r, 0x000800A0, "");

    static FakeFile primary, overriding;
    primary.data = overriding.data = &data;
    static AtIoJamFileSystem fs;
    fs.m_file = &primary;
    fs.m_overrideFile = nullptr;
    o.fs = &fs;
    r->m_fs = &fs;
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
            FakeFile& file = (round % 7 == 3) ? overriding : primary;
            fs.m_overrideFile = (round % 7 == 3) ? &overriding : nullptr;
            file.mode = mode;
            int s0 = file.seeks, r0 = file.reads, l0 = file.releases, w0 = file.request.waits;
            unsigned ooff = 9, osize = 9, roff = 9, rsize = 9;
            const int ores = ((FnRSPP)(uintptr_t)0x00080BA0)(&o, nullptr, path, &ooff, &osize);
            const int sd = file.seeks - s0, rdn = file.reads - r0, ld = file.releases - l0, wd = file.request.waits - w0;
            s0 = file.seeks; r0 = file.reads; l0 = file.releases; w0 = file.request.waits;
            const int rres = r->GetFileInfo(path, &roff, &rsize);
            eq("GetFileInfo", ores, rres);
            eq("GetFileInfo.offset", ooff, roff);
            eq("GetFileInfo.size", osize, rsize);
            eq("GetFileInfo.seeks", sd, file.seeks - s0);
            eq("GetFileInfo.reads", rdn, file.reads - r0);
            eq("GetFileInfo.releases", ld, file.releases - l0);
            eq("GetFileInfo.waits", wd, file.request.waits - w0);
            if (q[0] == "F" && mode == 0) {  // tools/jam.py read the record's size independently.
                eq("parser size vs original", std::atoll(q[3].c_str()), osize);
            }
            file.mode = 0;
            round++;
        }
        if (q[0] == "F") {
            eq("parser offset vs original", std::atoll(q[2].c_str()), (unsigned)oo);
        }
    }
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

int main(int argc, char** argv) {
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_jam <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    if (entries && !arm_entries(entries)) return 2;
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
        cases++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    report_entries();
    std::printf("%s: %d archives, %llu paths, %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", cases, g_queries, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
