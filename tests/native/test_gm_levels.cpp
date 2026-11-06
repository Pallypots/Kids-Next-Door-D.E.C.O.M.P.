// End to end, on any host: Levels.txt read through recovered code only, from the archive to the level table.
//
//   test_gm_levels                      input written here, needs no game data
//   test_gm_levels <directory> <expected> [--print]
//
// tools/levels.py writes the directory: AppInit.JAM and an expected list made by Python's own parse of the text, so the expected values owe
// nothing to the recovered code. --print writes the inventory to stdout; it is game data, so keep it out of the repository.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#include "at/AtIoJam.h"
#include "at/AtIoMountManager.h"
#include "gm/GmLevelManager.h"
#include "gm/GmRumble.h"
#include "host_file_system.h"

static int g_fail;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static AtIoFileSystem* g_stack[4];
static AtIoContext* g_context;
static char g_heap;

static void setup_context() {
    g_context = new AtIoContext;
    g_context->m_stack = g_stack;
    g_context->m_stackSize = 4;
    AtIoContext::s_instance = g_context;
    AtIoFileRef::s_context = g_context;
    AtIoFastAlignedJamHeader::SetHeap(&g_heap);
    AtIoFastJamHeader::SetHeap(&g_heap);
    AtIoJamFileSystem::SetHeap(&g_heap);
    AtIoJamFileSystem::SetHeaderHeap(&g_heap);
    AtIoTokenFile::s_heap = &g_heap;
    AtIoStream::s_heap = &g_heap;
    GmLevelManager::SetHeap(&g_heap);
}

class TextFile : public AtIoFile {
public:
    std::string text;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        const size_t have = text.size() - m_position;
        if (n > have) n = (unsigned int)have;
        if (n) std::memcpy(out, text.data() + m_position, n);
        m_position += n;
        *done = n;
        return n ? 0 : (int)kEndOfFile;
    }
    int Write(const void*, unsigned int, unsigned int*) override { return 3; }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class TextFs : public AtIoFileSystem {
public:
    TextFile file;
    int Open(const char*, int, AtIoFile** out) override {
        file.m_position = 0;
        file.m_flags = 2;
        *out = &file;
        return 0;
    }
    int Close(AtIoFile*) override { return 0; }
    int IsDirectoryMissing(const char*) override { return 1; }
    int IsFileMissing(const char*) override { return 0; }
    int GetEntryType(const char*, int*) override { return 1; }
    int Unknown7(void*) override { return 0; }
    void* List(const char*) override { return nullptr; }
    void FreeList(void*) override {}
    int Unknown11(void*) override { return 0; }
    int Unknown12() override { return 0; }
};

// What a port supplies in place of the Xbox's device layer: host files, under one directory, for both kinds.
class HostFactory : public AtIoFileSystemFactory {
public:
    explicit HostFactory(const std::string& root) : m_rootDir(root) {}
    void Unknown0() override {}
    void Unknown1() override {}
    void Unknown2() override {}
    void Unknown3() override {}
    void Unknown4() override {}
    AtIoFileSystem* Create(int, int, int) override { return new AtIoHostFileSystem(m_rootDir); }
    void Destroy(AtIoFileSystem* fs) override { delete fs; }
    std::string m_rootDir;
};

class NoDevice : public AtIoContextDevice {
public:
    void Unknown0() override {}
    int Unknown1() override { return 0; }
};

static std::string name_of(const GmLevelManager& m, int i) {
    char text[9] = {0};
    std::memcpy(text, m.m_names.m_items[i].m_name, 8);
    return text;
}

static void run_synthetic() {
    TextFs* fs = new TextFs;
    g_context->Push(fs);
    fs->file.text =
        "// a comment\r\nLevels [3]\r\n{\r\n"
        "\tLevel \"M1\" FMVCinema\r\n\r\n"
        "\tLevel \"L1\" NormalLevel\r\n\t{\r\n\t\tNumLoadItems 12\r\n\t\tDescription \"First: One\"\r\n\t}\r\n"
        "\tLevel \"L2\" HiddenLevel\r\n\t{\r\n\t\tNumLoadItems 34\r\n\t}\r\n}\r\n";
    static GmLevelManager m;
    m.Load("Levels.txt");
    CHECK(m.m_loaded == 1);
    CHECK(m.m_levelCount == 3);
    CHECK(name_of(m, 0) == "M1");
    CHECK(name_of(m, 1) == "L1");
    CHECK(name_of(m, 2) == "L2");
    CHECK(m.m_kinds[0] == GmLevelManager::kMovie);
    CHECK(m.m_kinds[1] == GmLevelManager::kNormal);
    CHECK(m.m_kinds[2] == GmLevelManager::kHidden);
    CHECK(m.m_loadItems[1] == 12);
    CHECK(m.m_loadItems[2] == 34);
    CHECK(m.m_descriptions[0] == nullptr);
    CHECK(std::strcmp(m.m_descriptions[1], "First: One") == 0);
    CHECK(m.m_descriptions[2] == nullptr);
    g_context->Pop();
}

static void run_real(const std::string& dir, const std::string& expected, bool print) {
    // The same steps as start-up: 0x0008E6FF creates the file systems, 0x0008E78C mounts AppInit.
    static NoDevice contextDevice;
    *(AtIoContextDevice**)g_context = &contextDevice;
    static HostFactory factory(dir);
    static AtIoMountOwner owner;
    owner.m_factory = &factory;
    AtIoMountParams params = {};
    params.m_owner = &owner;
    params.m_heap = &g_heap;
    params.m_context = g_context;
    static AtIoMountManager mount;
    mount.Init(&params);
    mount.Mount("AppInit", "");
    CHECK(mount.m_mounted != nullptr);
    CHECK(g_context->Top() == mount.m_mounted);

    GmLevelManager m;
    m.Load("Levels.txt");
    CHECK(m.m_loaded == 1);

    std::ifstream in(expected);
    std::string line;
    int i = 0;
    while (std::getline(in, line)) {
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '\t')) f.push_back(part);
        while (f.size() < 4) f.push_back("");
        CHECK(i < m.m_levelCount);
        if (i >= m.m_levelCount) break;
        CHECK(name_of(m, i) == f[0]);
        CHECK(std::to_string(m.m_kinds[i]) == f[1]);
        if (!f[2].empty()) CHECK(std::to_string(m.m_loadItems[i]) == f[2]);
        CHECK((m.m_descriptions[i] ? std::string(m.m_descriptions[i]) : std::string()) == f[3]);
        i++;
    }
    CHECK(i == m.m_levelCount);
    std::printf("%d entries read from AppInit.JAM through recovered code, compared with Python's parse\n", m.m_levelCount);
    // Rumble effects, the same way. Python's floats are correctly rounded and the game's parser is not, so these compare within a tolerance; tests/oracle/oracle_rmb.cpp compares bits.
    {
        GmNameMap::s_heap = &g_heap;
        GmRumbleEffectArray::s_heap = &g_heap;
        GmRumbleFrameArray::s_heap = &g_heap;
        GmRumbleEffects effects;
        effects.Load("Design\\Control\\rumble");
        CHECK(effects.m_loaded == 1);
        std::ifstream rumble(expected.substr(0, expected.size() - 10) + "rumble.tsv");
        std::string row;
        unsigned effect = 0, frame = 0, frames = 0;
        GmRumbleEffect* current = nullptr;
        while (std::getline(rumble, row)) {
            std::vector<std::string> f;
            std::stringstream ss(row);
            std::string part;
            while (std::getline(ss, part, '\t')) f.push_back(part);
            if (f[0] == "E") {
                current = effects.Find(f[1].c_str());
                CHECK(current != nullptr);
                if (!current) break;
                CHECK(current == &effects.m_effects.m_items[effect]);
                CHECK(std::to_string(current->m_frames.m_count) == f[2]);
                effect++;
                frame = 0;
            } else if (current) {
                const GmRumbleFrame* k = current->Frame(frame);
                CHECK(std::abs(k->m_time - std::atof(f[1].c_str())) < 1e-5);
                CHECK(std::abs(k->m_left - std::atof(f[2].c_str())) < 1e-5);
                CHECK(std::abs(k->m_right - std::atof(f[3].c_str())) < 1e-5);
                frame++;
                frames++;
            }
        }
        CHECK(effect == effects.m_effects.m_count);
        CHECK(effect != 0);
        std::printf("%u rumble effects and %u key frames, compared with Python's parse\n", effect, frames);
    }
    if (print) {
        static const char* const kinds[] = {"normal", "hidden", "non-selectable", "movie"};
        for (int k = 0; k < m.m_levelCount; k++) {
            std::printf("%2d  %-9s %-15s %5s  %s\n", k, name_of(m, k).c_str(), kinds[m.m_kinds[k]],
                        m.m_kinds[k] == GmLevelManager::kMovie ? "" : std::to_string(m.m_loadItems[k]).c_str(), m.m_descriptions[k] ? m.m_descriptions[k] : "");
        }
    }
    std::fflush(stdout);
    std::_Exit(g_fail ? 1 : 0);  // Unloading needs level content code that is not recovered.
}

int main(int argc, char** argv) {
    setup_context();
    if (argc >= 3) {
        run_real(argv[1], argv[2], argc > 3 && !std::strcmp(argv[3], "--print"));
    }
    run_synthetic();
    std::printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
    std::fflush(stdout);
    std::_Exit(g_fail ? 1 : 0);
}
