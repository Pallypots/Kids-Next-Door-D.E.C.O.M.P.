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
    h.m_buffer = const_cast<unsigned char*>(v.data());
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
    int read_result = 0;
    bool seek_fails = false;
    int releases = 0;
    MemoryRequest request;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned offset) override { m_position = offset; return seek_fails || offset >= data.size() ? 1 : 0; }
    int Read(void* out, unsigned size, unsigned* done, AtIoFileRequest** req) override {
        if (read_result != 0 && read_result != kPending) return read_result;
        std::memcpy(out, data.data() + m_position, size);
        m_position += size;
        if (done) *done = size;
        if (req) *req = &request;
        return read_result;
    }
    int Write(const void*, unsigned, unsigned*) override { return kUnsupported; }
    int Flush() override { return kUnsupported; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { releases++; return 0; }
};

static AtIoJamFileSystem* make_fs(MemoryFile* file, const std::vector<unsigned char>& v, unsigned max_open) {
    AtIoJamFileSystem* fs = new AtIoJamFileSystem;
    fs->m_info.Init((const char*)v.data());
    fs->m_maxOpen = max_open;
    fs->m_file = file;
    fs->m_overrideFile = nullptr;
    return fs;
}

// Header loaded through vInit from a file, then files opened, read, sought and closed through OpenFile.
template <class Header>
static void test_loaded_archive(const char* token, bool jam2) {
    auto v = build(token, jam2);
    MemoryFile disc;
    disc.data = v;
    disc.data.resize(0x5000, 0);
    unsigned char* data = disc.data.data();
    for (int k = 0; k < 0x40; k++) data[0x1000 + (jam2 ? 0x20 : 0) + k] = (unsigned char)(k + 1);
    if (jam2) {
        data[0x1000] = 11;  // record: first size, second size
        data[0x1004] = 99;
    }
    Header::SetHeap(&disc);
    Header& h = *new Header;
    AtIoJamFileSystem* fs = make_fs(&disc, v, 2);
    h.vInit(fs);
    CHECK(h.m_fs == fs);
    CHECK(h.m_rootDirs == 1 && h.m_rootFiles == 2);
    CHECK(h.IsDirectoryMissing("ART") == 0);
    CHECK(h.m_table.FindName("ROOT") == 4);

    AtIoJamFile* a = h.OpenFile();
    AtIoJamFile* b = h.OpenFile();
    CHECK(a != nullptr && b != nullptr && a != b);
    CHECK(a->m_owner == &h);
    CHECK(h.OpenFile() == nullptr);  // all m_maxOpen in use

    CHECK(a->Open("NOPE.AFA", "shown", 0) == AtIoFile::kNotFound);
    CHECK(a->Open("FILEA.AFA", "shown", 0x4) == AtIoFile::kUnsupported);  // write access
    CHECK(a->m_flags == 0);
    CHECK(a->Open("FILEA.AFA", "shown", 0x21) == AtIoFile::kOk);
    CHECK(a->m_flags == AtIoFile::kOpen);                                 // the ignored bits are gone
    CHECK(!std::strcmp(a->m_path, "shown"));
    CHECK(a->m_size == 11 && a->m_position == 0);
    CHECK(a->m_base == (jam2 ? 0x1020u : 0x1000u));
    if (jam2) CHECK(a->m_secondSize == 99);

    unsigned char out[32] = {};
    unsigned done = 0;
    AtIoFileRequest* request = nullptr;
    CHECK(a->Read(out, 0, &done, &request) == AtIoFile::kOk);
    CHECK(a->Read(out, 4, &done, &request) == AtIoFile::kOk && done == 4);
    CHECK(out[0] == 1 && out[3] == 4 && a->m_position == 4);
    CHECK(a->Read(out, 32, &done, &request) == AtIoFile::kOk && done == 7);  // clipped to what is left
    CHECK(out[0] == 5 && (a->m_flags & AtIoFile::kReachedEnd));
    CHECK(a->Read(out, 1, &done, &request) == AtIoFile::kEndOfFile);
    CHECK(a->Seek(11) == AtIoFile::kEndOfFile);
    CHECK(a->Write(out, 1, &done) == AtIoFile::kUnsupported && done == 0);
    CHECK(a->Flush() == AtIoFile::kUnsupported);
    CHECK(a->Seek(2) == AtIoFile::kOk);
    CHECK(a->m_position == 2 && !(a->m_flags & AtIoFile::kReachedEnd));
    // Original behaviour: the FSTA file adds its current position (11 here) to the target, the JAM2 file does not.
    CHECK(disc.m_position == a->m_base + (jam2 ? 2u : 13u));
    CHECK(a->Read(out, 1, &done, &request) == AtIoFile::kOk);  // Read seeks for itself, so the data is right either way.
    CHECK(out[0] == 3);

    CHECK(h.CloseFile(a) == 0);
    CHECK(a->m_owner == nullptr && a->m_flags != AtIoFile::kOpen);
    CHECK(h.OpenFile() == a);        // the slot is free again
    CHECK(a->Read(out, 1, &done, &request) == AtIoFile::kNotFound);  // attached but not open
    h.vInit(fs);                     // a second vInit shuts down first
    CHECK(h.OpenFile() != nullptr);
    delete &h;
}

static void test_jam2() {
    auto v = build("JAM2", true);
    AtIoFastJamHeader& h = *new AtIoFastJamHeader;
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
    AtIoJamFileSystem& fs = *new AtIoJamFileSystem;  // Never destroyed: it does not own this file.
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
    test_fsta("FSTA", *new AtIoFastAlignedJamHeader);
    AtIoFastRawJamHeader* raw = new AtIoFastRawJamHeader;
    CHECK(raw->m_swappedFields == 1);
    test_fsta("FSTR", *raw);
    CHECK(raw->m_swappedFields == 1);  // Reset keeps the FSTR form
    test_jam2();
    test_loaded_archive<AtIoFastAlignedJamHeader>("FSTA", false);
    test_loaded_archive<AtIoFastRawJamHeader>("FSTR", false);
    test_loaded_archive<AtIoFastJamHeader>("JAM2", true);
    std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
    return g_fail ? 1 : 0;
}
