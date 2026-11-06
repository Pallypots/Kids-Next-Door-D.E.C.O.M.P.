// End-to-end: open a real archive through the recovered layers on host files, and read files out of it.
//
//   test_at_e2e <directory> <manifest>
//
// tools/jam.py e2e writes the manifest independently, by slicing each archive with Python and computing CRC-32s:
//   A \t archive name
//   F \t path in archive \t size \t crc32
// With no arguments this uses a synthetic archive and needs no game data. The source under test is recovered; the host file
// system beneath it is port code (platform/).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "at/AtIoJam.h"
#include "host_file_system.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static uint32_t crc32(const unsigned char* p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static AtIoFileSystem* g_stack[4];
static AtIoContext* g_context;

static void setup(const std::string& root) {
    static char heap_token;
    AtIoFastAlignedJamHeader::SetHeap(&heap_token);
    AtIoFastJamHeader::SetHeap(&heap_token);
    AtIoJamFileSystem::SetHeap(&heap_token);
    AtIoJamFileSystem::SetHeaderHeap(&heap_token);
    g_context = new AtIoContext;
    std::memset(static_cast<void*>(g_context), 0, sizeof *g_context);
    g_context->m_stack = g_stack;
    g_context->m_stackSize = 4;
    AtIoContext::s_instance = g_context;
    g_context->Push(new AtIoHostFileSystem(root));
}

static std::vector<unsigned char> read_all(AtIoFile* file, unsigned size) {
    std::vector<unsigned char> out(size);
    unsigned got = 0;
    while (got < size) {  // Deliberately in pieces, so positions and clipping are exercised.
        unsigned n = size - got < 4099 ? size - got : 4099;
        unsigned done = 0;
        AtIoFileRequest* request = nullptr;
        const int status = file->Read(out.data() + got, n, &done, &request);
        if (status != AtIoFile::kOk) { CHECK(status == AtIoFile::kOk); break; }
        CHECK(done == n);
        got += n;
    }
    CHECK(file->m_position == size);
    return out;
}

static int run_manifest(const char* manifest) {
    std::ifstream in(manifest);
    if (!in) { std::printf("cannot read %s\n", manifest); return 2; }
    std::string line;
    AtIoJamFileSystem* fs = nullptr;
    unsigned archives = 0, files = 0;
    unsigned long long bytes = 0;
    while (std::getline(in, line)) {
        std::istringstream parts(line);
        std::string kind, path;
        std::getline(parts, kind, '\t');
        std::getline(parts, path, '\t');
        if (kind == "A") {
            if (fs) { g_context->Pop(); delete fs; }
            fs = AtIoJamFileSystem::Create(path.c_str(), 4, 0);
            CHECK(fs->m_info.m_token != 0);
            g_context->Push(fs);  // Mounted: the context now finds files through the archive first.
            archives++;
            continue;
        }
        unsigned size = 0, want = 0;
        parts >> size >> want;
        AtIoFileSystem* found = nullptr;
        CHECK(g_context->FindFile(path.c_str(), &found) == 0);
        CHECK(found == fs);
        AtIoFile* file = nullptr;
        const int status = fs->Open(path.c_str(), 0, &file);
        CHECK(status == 0);
        if (status != 0) continue;
        CHECK(file->m_size == size);
        const std::vector<unsigned char> data = read_all(file, size);
        CHECK(crc32(data.data(), data.size()) == want);
        bytes += size;
        files++;
        fs->Close(file);
    }
    if (fs) { g_context->Pop(); delete fs; }
    std::printf("read through recovered source: %u archives, %u files, %llu bytes, all CRC-32s compared\n", archives, files, bytes);
    return 0;
}

static void put16(std::vector<unsigned char>& v, unsigned x) { v.push_back(x & 255); v.push_back((x >> 8) & 255); }
static void put32(std::vector<unsigned char>& v, unsigned x) { put16(v, x & 0xFFFF); put16(v, x >> 16); }

static void run_synthetic() {
    // One directory-less FSTA archive holding HELLO.TXT.
    const char body[] = "hello from a synthetic archive";
    std::vector<unsigned char> v;
    const char token[] = "FSTA";
    v.insert(v.end(), token, token + 4);
    put32(v, 0);
    put32(v, 0x20 + 8 + 8 + 4 + 12);  // header size: names, extensions, root counts, one file
    for (int i = 0; i < 16; i++) v.push_back(0);
    put16(v, 1);
    v.push_back(2);
    v.push_back(0);
    const char name[8] = "HELLO";
    v.insert(v.end(), name, name + 8);
    for (int i = 0; i < 4; i++) v.push_back(0);
    const char ext[4] = "TXT";
    v.insert(v.end(), ext, ext + 4);
    put16(v, 0);
    put16(v, 1);
    put16(v, 0);
    v.push_back(1);
    v.push_back(0);
    const unsigned offset = (unsigned)v.size() + 8;
    put32(v, offset);
    put32(v, (unsigned)sizeof body);
    v.insert(v.end(), body, body + sizeof body);
    const char* dir = ".";
    const char* archive = "knd_test_synthetic.jam";
    FILE* f = std::fopen(archive, "wb");
    CHECK(f != nullptr);
    std::fwrite(v.data(), 1, v.size(), f);
    std::fclose(f);

    setup(dir);
    AtIoJamFileSystem* fs = AtIoJamFileSystem::Create(archive, 2, 0);
    CHECK(fs->m_info.m_token == AtIoJamInfoHeader::kFsta);
    CHECK(fs->m_archiveSize == v.size());
    CHECK(fs->m_kind == 5 && fs->m_maxOpen == 2);
    g_context->Push(fs);
    AtIoFileSystem* found = nullptr;
    CHECK(g_context->FindFile("hello.txt", &found) == 0 && found == fs);
    CHECK(g_context->FindFile("nope.txt", &found) == 1 && found == nullptr);
    AtIoFile* file = nullptr;
    CHECK(fs->Open("hello.txt", 0, &file) == 0);
    CHECK(fs->m_openFiles == 1 && fs->m_peakOpenFiles == 1);
    const std::vector<unsigned char> data = read_all(file, (unsigned)sizeof body);
    CHECK(!std::memcmp(data.data(), body, sizeof body));
    AtIoFileStat stat;
    CHECK(fs->GetFileInfo("hello.txt", &stat) == 0 && stat.m_size == sizeof body && stat.m_offset == offset);
    CHECK(fs->GetFileInfo("nope.txt", &stat) == 1);
    int type = 0;
    CHECK(fs->GetEntryType("hello.txt", &type) == 0 && type == AtIoJamHeader::kEntryIsFile);
    CHECK(fs->IsFileMissing("nope.txt") == 1);
    CHECK(fs->Close(file) == 0);
    CHECK(fs->m_openFiles == 0 && fs->m_peakOpenFiles == 1);
    AtIoFile* missing = reinterpret_cast<AtIoFile*>(&v);
    CHECK(fs->Open("nope.txt", 0, &missing) == AtIoFile::kNotFound && missing == nullptr);
    CHECK(g_context->Top() == fs);
    g_context->Pop();
    delete fs;
    std::remove(archive);
    std::printf("synthetic archive read through recovered source\n");
}

int main(int argc, char** argv) {
    if (argc == 3) {
        setup(argv[1]);
        const int rc = run_manifest(argv[2]);
        std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
        return rc ? rc : (g_fail ? 1 : 0);
    }
    run_synthetic();
    std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
    return g_fail ? 1 : 0;
}
