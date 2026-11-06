// Native test of the text tokenizer (any host). With no arguments it uses only text written here, and needs no game data.
// With a cases directory and digests from tests/oracle/oracle_tok.cpp, it also checks that this build, on this host, produces what the
// ORIGINAL code produced on every text file on the disc, floating point included:
//
//   test_at_tok <cases dir> <digests file>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "tok_common.h"

static int g_failures;
static int g_syntax_errors;
static std::string g_last_message;

void AtFatal(const char* message, int, int, int) {
    g_last_message = message;
    if (std::strstr(message, "syntax error")) g_syntax_errors++;
}
void AtRaise(int code, int, int, int) { std::printf("raise %d\n", code); g_failures++; }
void AtPushHeap(void*) {}
void AtPopHeap() {}

#define CHECK(c) do { if (!(c)) { std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); g_failures++; } } while (0)

class MemFile : public AtIoFile {
public:
    const unsigned char* data = nullptr;
    size_t size = 0;
    std::string written;
    int Close() override { return 0; }
    int Open(const char*, const char*, int) override { return 0; }
    int Clear() override { return 0; }
    int Seek(unsigned int offset) override { m_position = offset; return 0; }
    int Read(void* out, unsigned int n, unsigned int* done, AtIoFileRequest**) override {
        const size_t have = size - m_position;
        if (n > have) n = (unsigned int)have;
        if (n) std::memcpy(out, data + m_position, n);
        m_position += n;
        *done = n;
        return n ? 0 : (int)kEndOfFile;
    }
    int Write(const void* in, unsigned int n, unsigned int* done) override {
        written.append((const char*)in, n);
        m_position += n;
        *done = n;
        return 0;
    }
    int Flush() override { return 0; }
    int Unknown8(void*) override { return 0; }
    int Release(AtIoFileRequest*) override { return 0; }
};

class MemFs : public AtIoFileSystem {
public:
    MemFile file;
    int Open(const char*, int flags, AtIoFile** out) override {
        file.m_position = 0;
        file.m_flags = (flags & 0x21) ? 0x0C : 2;
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

static MemFs* g_fs;

static std::string real_text(float v) {
    char out[64];
    AtFormatReal(v, out);
    return out;
}

static void test_reading() {
    static const AtIoKeyword words[] = {{60, "Levels"}, {61, "Level"}};
    static const char text[] = "// comment\r\nLevels [2]\r\n{\r\n\tLEVEL \"FMV01\" unknown -0x10 0.5 -1.5 .25\r\n}\r\n";
    g_fs->file.data = (const unsigned char*)text;
    g_fs->file.size = sizeof text - 1;
    AtIoTokenFile f;
    CHECK(f.Open("LEVELS", "TXT", 2, nullptr) == 0);
    AtIoTokenizer* t = f.m_tokenizer;
    t->SetKeywords(words, 2, nullptr);
    CHECK(t->Next() == 60);
    CHECK(t->ExpectCount() == 2);
    CHECK(t->m_stream->m_line == 3);
    CHECK(t->Next() == 61);
    CHECK(std::strcmp(t->ExpectString(), "FMV01") == 0);
    CHECK(t->Next() == AtIoTokenizer::kError);
    CHECK(std::strcmp(t->GetIdentifier(), "UNKNOWN") == 0);
    CHECK(t->ExpectInt() == -16);
    CHECK(t->ExpectReal() == 0.5f);
    CHECK(t->ExpectReal() == -1.5f);
    CHECK(t->ExpectReal() == 0.25f);
    t->ExpectRightCurly();
    CHECK(t->Next() == 0);
    g_last_message.clear();
    t->Error(4);
    CHECK(g_last_message == "LEVELS.TXT: line 6 - expected real");
    f.Close();
}

static void test_writing() {
    static const AtIoKeyword words[] = {{60, "Alpha"}};
    g_fs->file.written.clear();
    AtIoTokenFile f;
    CHECK(f.Open("OUT.TXT", 0x21, nullptr) == 0);
    AtIoTokenizer* t = f.m_tokenizer;
    t->SetKeywords(words, 1, nullptr);
    t->PutKeyword(60);
    t->PutSpace();
    t->PutLeftBracket();
    t->PutInt(-7);
    t->PutRightBracket();
    t->PutNewline();
    t->PutTabs(2);
    t->PutString("text");
    t->PutReal(0.5f);
    t->PutName(nullptr);
    t->PutComment("note");
    f.Close();
    CHECK(g_fs->file.written == "Alpha [-7]\r\n\t\t\"text\"0.500000\"\"// note");
}

// Values checked against the original in tests/oracle/oracle_tok.cpp: its C library's rounding, which a modern printf does not have.
static void test_real_format() {
    CHECK(real_text(123456.7890625f) == "123456.789063");
    CHECK(real_text(-0.0f) == "0.000000");
    CHECK(real_text(0.9999995f) == "0.999999" || real_text(0.9999995f) == "1.000000");
    CHECK(real_text(999999.94f) == "999999.937500");
    CHECK(real_text(1.0e-7f) == "0.000000");
    CHECK(real_text(-1.0e-7f) == "-0.000000");
    CHECK(real_text(0.5f) == "0.500000");
    CHECK(real_text(16777216.0f) == "16777216.000000");
}

// The original adds digit * scale, with scale itself a rounded float, so it is often not the nearest float to the text: 100.001 is off by
// 0x189 units. These values are what the original's code gives (the same list is asserted against it in tests/oracle/oracle_tok.cpp).
static void test_fractions() {
    static const char text[] = "0.1 0.7 3.14159 100.001 123456.789 0.3333333333333333333";
    g_fs->file.data = (const unsigned char*)text;
    g_fs->file.size = sizeof text - 1;
    AtIoTokenFile f;
    f.Open("N", "TXT", 2, nullptr);
    AtIoTokenizer* t = f.m_tokenizer;
    t->SetKeywords(nullptr, 0, nullptr);
    static const unsigned int bits[] = {0x3DCCCCCD, 0x3F333333, 0x40490FCE, 0x42C80083, 0x47F12065, 0x3EAAAAAB};
    for (unsigned int expected : bits) {
        CHECK(t->Next() == AtIoTokenizer::kReal);
        unsigned int got;
        std::memcpy(&got, &t->m_real, 4);
        if (got != expected) std::printf("  real bits %08X, expected %08X\n", got, expected);
        CHECK(got == expected);
    }
    f.Close();
}

static bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize((size_t)n);
    const size_t got = n ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == (size_t)n;
}

static void test_digests(const char* dir, const char* digests) {
    FILE* in = std::fopen(digests, "r");
    if (!in) { std::printf("cannot read %s\n", digests); g_failures++; return; }
    char name[128];
    unsigned long long hash, tokens, total = 0;
    int files = 0;
    while (std::fscanf(in, "%127s %llx %llu", name, &hash, &tokens) == 3) {
        std::vector<unsigned char> data;
        if (!read_file(std::string(dir) + "/" + name, data)) { std::printf("cannot read %s\n", name); g_failures++; continue; }
        KeywordSet words;
        words.build(data);
        g_fs->file.data = data.data();
        g_fs->file.size = data.size();
        AtIoTokenFile f;
        f.Open("X\\NAME", "ABC", 2, nullptr);
        AtIoTextTokenizer* t = (AtIoTextTokenizer*)f.m_tokenizer;
        t->SetKeywords(words.table.data(), (unsigned int)words.table.size(), nullptr);
        Digest d;
        g_syntax_errors = 0;
        for (int i = 0; i < 2000000; i++) {
            const int r = t->Next();
            if (g_syntax_errors) break;
            d.token(r, *t);
            if (r == 0) break;
        }
        f.Close();
        if (d.h != hash || d.tokens != tokens) {
            if (g_failures < 20) std::printf("DIFFERENT from the original: %s (%llu tokens, %llu expected)\n", name, d.tokens, tokens);
            g_failures++;
        }
        files++;
        total += tokens;
    }
    std::fclose(in);
    std::printf("%d files, %llu tokens compared with the original's digests\n", files, total);
    if (!files) g_failures++;
}

int main(int argc, char** argv) {
    g_fs = new MemFs;
    static AtIoFileSystem* stack[1] = {g_fs};
    static AtIoContext ctx;
    ctx.m_stackCount = 1;
    ctx.m_stackSize = 1;
    ctx.m_stack = stack;
    AtIoFileRef::s_context = &ctx;
    static int heap;
    AtIoTokenFile::s_heap = &heap;
    AtIoStream::s_heap = &heap;
    test_reading();
    test_writing();
    test_real_format();
    test_fractions();
    if (argc == 3) test_digests(argv[1], argv[2]);
    std::printf(g_failures ? "FAILED: %d\n" : "ok\n", g_failures);
    return g_failures ? 1 : 0;
}
