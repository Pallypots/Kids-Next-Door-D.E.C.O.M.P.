// Host semantic tests for the recovered JAM directory reader, on synthetic archives: no game data is needed. These test
// the recovered source's behaviour; the comparison with the original is tests/oracle/oracle_jam.cpp.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtIoJam.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static void put16(std::vector<unsigned char>& v, unsigned x) { v.push_back(x & 255); v.push_back((x >> 8) & 255); }
static void put32(std::vector<unsigned char>& v, unsigned x) { put16(v, x & 0xFFFF); put16(v, x >> 16); }
static void set32(std::vector<unsigned char>& v, size_t at, unsigned x) {
    for (int i = 0; i < 4; i++) v[at + i] = (x >> (8 * i)) & 255;
}

// Names, sorted:  0 ART  1 FILEA  2 FILEB  3 LEVEL  4 ROOT. Extensions: 0 "", 1 AFA, 2 AFD.
// Layout:  ROOT files: FILEA.AFA, FILEB (no extension).
//          ART\   files: FILEB.AFD         subdirectory LEVEL\ files: FILEA.AFD
static std::vector<unsigned char> build(const char* token, bool jam2) {
    static const char* names[] = {"ART", "FILEA", "FILEB", "LEVEL", "ROOT"};
    static const char* exts[] = {"", "AFA", "AFD"};
    const bool swapped = !std::strcmp(token, "FSTR");
    auto word = [&](unsigned name, unsigned dirs, unsigned files) {
        return swapped ? name | files << 12 | dirs << 21 : name | dirs << 12 | files << 21;
    };
    auto file = [&](std::vector<unsigned char>& v, unsigned name, unsigned ext, unsigned off, unsigned size) {
        put16(v, name);
        v.push_back((unsigned char)ext);
        v.push_back(0);
        put32(v, off);
        if (!jam2) put32(v, size);
    };
    std::vector<unsigned char> v(token, token + 4);
    put32(v, 0x12345678);
    size_t size_at = v.size();
    put32(v, 0);
    const char label[15] = "none";
    v.insert(v.end(), label, label + 15);
    v.push_back(0);
    put16(v, 5);
    v.push_back(3);
    v.push_back(0);
    for (const char* n : names) { char b[8] = {}; std::strncpy(b, n, 8); v.insert(v.end(), b, b + 8); }
    for (const char* e : exts) { char b[4] = {}; std::strncpy(b, e, 4); v.insert(v.end(), b, b + 4); }
    put16(v, 1);  // root: one subdirectory
    put16(v, 2);  // and two files
    size_t art_entry = v.size();
    put32(v, word(0, 1, 1));
    put32(v, 0);
    file(v, 1, 1, 0x1000, 11);
    file(v, 2, 0, 0x2000, 22);
    size_t art_table = v.size();
    size_t level_entry = v.size();
    put32(v, word(3, 0, 1));
    put32(v, 0);
    file(v, 2, 2, 0x3000, 33);
    size_t level_table = v.size();
    file(v, 1, 2, 0x4000, 44);
    set32(v, art_entry + 4, (unsigned)art_table);
    set32(v, level_entry + 4, (unsigned)level_table);
    set32(v, size_at, (unsigned)v.size());
    return v;
}

template <class H>
static void prepare(H& h, const std::vector<unsigned char>& v, unsigned names, unsigned exts) {
    h.m_table.Init((const char*)v.data() + 0x20, names, exts);
    const unsigned char* root = v.data() + 0x20 + 4 * (exts + 2 * names);
    h.m_buffer = v.data();
    h.m_rootDirs = root[0] + (root[1] << 8);
    h.m_rootFiles = root[2] + (root[3] << 8);
    h.m_root = root + 4;
}

static void test_info_header() {
    auto v = build("FSTA", false);
    AtIoJamInfoHeader info;
    CHECK(info.m_token == AtIoJamInfoHeader::kNone);
    info.Init((const char*)v.data());
    CHECK(info.m_token == AtIoJamInfoHeader::kFsta);
    CHECK(info.m_unknown04 == 0x12345678);
    CHECK(info.m_headerSize == v.size());
    CHECK(info.m_nameCount == 5);
    CHECK(info.m_extensionCount == 3);
    CHECK(!std::strcmp(info.m_label, "none"));
    auto j = build("JAM2", true);
    info.Init((const char*)j.data());
    CHECK(info.m_token == AtIoJamInfoHeader::kJam2);
    info.Clear();
    CHECK(info.m_token == 0 && info.m_headerSize == 0 && info.m_label[0] == 0);
}

static void test_name_table() {
    auto v = build("FSTA", false);
    AtIoJamNameTable t;
    t.Init((const char*)v.data() + 0x20, 5, 3);
    CHECK(t.FindName("ART") == 0);
    CHECK(t.FindName("ROOT") == 4);
    CHECK(t.FindName("NOPE") == -1);
    CHECK(t.FindName("") == -1);
    CHECK(t.FindExtension("AFD") == 2);
    CHECK(t.FindExtension("ZZZ") == -1);
    CHECK(t.FindExtension("") == 0);
    CHECK(t.FindExtension(nullptr) == 0);
    char out[32];
    CHECK(t.GetFullName(1, 2, out) == 1 && !std::strcmp(out, "FILEA.AFD"));
    CHECK(t.GetFullName(5, 0, out) == 0 && out[0] == 0);
    CHECK(t.GetFullName(0, 3, out) == 0);
    CHECK(t.Matches("FILEA.AFD", "FILEA", "AFD") == 1);
    CHECK(t.Matches("FILEA.AFD", "*", "AFD") == 1);
    CHECK(t.Matches("FILEA.AFD", "FILEA", "*") == 1);
    CHECK(t.Matches("FILEA.AFD", "FILEB", "AFD") == 0);
    CHECK(t.Matches("FILEA.AFD", "FILEA", "AFA") == 0);
    CHECK(t.Matches("", "*", "*") == 0);
}

// A large sorted table, so that the binary phase of Search runs. Every present entry must be found.
static void test_search_large() {
    std::vector<char> table(8 * 1000, 0);
    for (int i = 0; i < 1000; i++) std::snprintf(&table[8 * i], 8, "N%04d", i);
    AtIoJamNameTable t;
    t.Init(table.data(), 1000, 0);
    for (int i = 0; i < 1000; i++) {
        char key[9] = {};
        std::memcpy(key, &table[8 * i], 8);
        int got = t.FindName(key);
        CHECK(got == i);
    }
    CHECK(t.FindName("ZZZZ") == -1);
    CHECK(t.FindName("A") == -1);
    CHECK(t.FindName("N0500X") == -1);  // between two present names
}

template <class H>
static void test_directories(H& h) {
    unsigned d = 99, f = 99;
    CHECK(h.FindDirectory("", &d, &f) == h.m_root && d == 1 && f == 2);
    CHECK(h.FindDirectory(nullptr, &d, &f) == h.m_root);
    CHECK(h.FindDirectory("ART", &d, &f) != nullptr && d == 1 && f == 1);
    CHECK(h.FindDirectory("art", &d, &f) != nullptr);  // folded to upper case
    CHECK(h.FindDirectory("ART\\LEVEL", &d, &f) != nullptr && d == 0 && f == 1);
    CHECK(h.FindDirectory("LEVEL", &d, &f) == nullptr);   // a real name, but not in the root
    CHECK(h.FindDirectory("NOPE", &d, &f) == nullptr);    // not a name at all
    CHECK(h.FindDirectory("ART\\NOPE", &d, &f) == nullptr);
    CHECK(h.IsDirectoryMissing("ART") == 0);
    CHECK(h.IsDirectoryMissing("NOPE") == 1);
    // Original behaviour: `found` describes only the last component. FILEA is a name but no directory here, so the walk
    // goes on, from where the failed scan stopped.
    d = f = 99;
    CHECK(h.FindDirectory("FILEA\\ART", &d, &f) == nullptr);
}

static void test_fsta(const char* token, AtIoFastAlignedJamHeader& h) {
    auto v = build(token, false);
    prepare(h, v, 5, 3);
    test_directories(h);
    unsigned size = 0;
    CHECK(h.FindFile("FILEA.AFA", &size) == 0x1000 && size == 11);
    CHECK(h.FindFile("filea.afa", &size) == 0x1000);
    CHECK(h.FindFile("FILEB", &size) == 0x2000 && size == 22);  // no extension: index 0
    CHECK(h.FindFile("ART\\FILEB.AFD", &size) == 0x3000 && size == 33);
    CHECK(h.FindFile("ART\\LEVEL\\FILEA.AFD", &size) == 0x4000 && size == 44);
    CHECK(h.FindFile("art\\level\\filea.afd", &size) == 0x4000);
    size = 77;
    CHECK(h.FindFile("FILEA.AFD", &size) == -1 && size == 77);  // name and extension exist, the pair does not
    CHECK(h.FindFile("FILEA.ZZZ", &size) == -1);
    CHECK(h.FindFile("NOPE.AFA", &size) == -1);
    CHECK(h.FindFile("NOPE\\FILEA.AFA", &size) == -1);
    CHECK(h.IsFileMissing("FILEA.AFA") == 0);
    CHECK(h.IsFileMissing("NOPE") == 1);
    int type = 0;
    CHECK(h.GetEntryType("FILEA.AFA", &type) == 0 && type == AtIoJamHeader::kEntryIsFile);
    type = 0;
    CHECK(h.GetEntryType("NOPE", &type) == 1 && type == 0);
    h.Reset();
    CHECK(h.m_root == nullptr && h.m_rootDirs == 0);
}

class MemoryRequest : public AtIoFileRequest {
public:
    int status = 0;
    void Unknown0() override {}
    void Unknown1() override {}
    int Wait() override { return status; }
};

class MemoryFile : public AtIoFile {
public:
    std::vector<unsigned char> data;
    unsigned position = 0;
    int read_result = 0;
    bool seek_fails = false;
    int releases = 0;
    MemoryRequest request;
    void Unknown0() override {}
    void Unknown1() override {}
    void Unknown2() override {}
    void Unknown3() override {}
    int Seek(unsigned offset) override { position = offset; return seek_fails || offset >= data.size() ? 1 : 0; }
    int Read(void* out, unsigned size, int, AtIoFileRequest** req) override {
        if (read_result != 0 && read_result != kPending) return read_result;
        std::memcpy(out, data.data() + position, size);
        *req = &request;
        return read_result;
    }
    void Unknown6() override {}
    void Unknown7() override {}
    void Unknown8() override {}
    void Release(AtIoFileRequest*) override { releases++; }
};

static void test_jam2() {
    auto v = build("JAM2", true);
    AtIoFastJamHeader& h = *new AtIoFastJamHeader;  // The destructor is not recovered yet.
    prepare(h, v, 5, 3);
    test_directories(h);
    CHECK(h.FindRecord("FILEA.AFA") == 0x1000);
    CHECK(h.FindRecord("art\\level\\filea.afd") == 0x4000);
    CHECK(h.FindRecord("NOPE") == -1);
    CHECK(h.IsFileMissing("FILEB") == 0);
    CHECK(h.IsFileMissing("NOPE") == 1);
    unsigned offset = 9;
    CHECK(h.GetRecordOffset("FILEB", &offset) == 1 && offset == 0x2000);
    CHECK(h.GetRecordOffset("NOPE", &offset) == 0 && offset == 0);

    MemoryFile file;
    file.data.assign(0x5000, 0);
    file.data[0x1000] = 0xAA;      // the first size field: the game reads the second
    file.data[0x1004] = 0x34;
    file.data[0x1005] = 0x12;
    static AtIoJamFileSystem fs;
    fs.m_file = &file;
    fs.m_overrideFile = nullptr;
    h.m_fs = &fs;
    unsigned size = 0;
    CHECK(h.GetFileInfo("FILEA.AFA", &offset, &size) == 1 && offset == 0x1000 && size == 0x1234);
    file.read_result = AtIoFile::kPending;   // asynchronous, completes
    size = 0;
    CHECK(h.GetFileInfo("FILEA.AFA", &offset, &size) == 1 && size == 0x1234 && file.releases == 1);
    file.request.status = 5;                 // asynchronous, fails
    CHECK(h.GetFileInfo("FILEA.AFA", &offset, &size) == 0 && file.releases == 2);
    file.request.status = 0;
    file.read_result = 5;                    // fails at once
    CHECK(h.GetFileInfo("FILEA.AFA", &offset, &size) == 0);
    file.read_result = 0;
    file.seek_fails = true;
    CHECK(h.GetFileInfo("FILEA.AFA", &offset, &size) == 0);
    file.seek_fails = false;
    CHECK(h.GetFileInfo("NOPE", &offset, &size) == 0 && offset == 0xFFFFFFFFu);  // seeks to -1, which must fail
    MemoryFile other;                        // an override file takes precedence
    other.data.assign(0x5000, 0);
    other.data[0x1004] = 0x77;
    fs.m_overrideFile = &other;
    CHECK(h.GetFileInfo("FILEA.AFA", &offset, &size) == 1 && size == 0x77);
}

int main() {
    test_info_header();
    test_name_table();
    test_search_large();
    test_fsta("FSTA", *new AtIoFastAlignedJamHeader);  // Leaked on purpose: the destructor is not recovered yet.
    AtIoFastRawJamHeader* raw = new AtIoFastRawJamHeader;
    CHECK(raw->m_swappedFields == 1);
    test_fsta("FSTR", *raw);
    CHECK(raw->m_swappedFields == 1);  // Reset keeps the FSTR form
    test_jam2();
    std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
    return g_fail ? 1 : 0;
}
