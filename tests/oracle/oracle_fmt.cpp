// Differential oracle for the pixel format (Windows x86, local only). See docs/verification.md.
//
//   oracle_fmt <default.xbe> [--count N] [--entries FILE] [--mutate]
//
// Also prints a hash of the original's results, to which tests/native/test_at_pixel_format.cpp holds the x64 build. The tokenizer in Read is a double supplying a script, the same object for both sides:
// the original's own 0x00015880 runs on it, through its vftable.
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtPixelFormat.h"
#include "fmt_cases.h"
#include "oracle_guard.h"
#include "oracle_image.h"

using namespace vec_cases;
using namespace fmt_cases;

void AtFatal(const char*, int, int, int) { *(volatile int*)0 = 0; }
void AtRaise(int, int, int, int) { *(volatile int*)0 = 0; }
void AtPushHeap(void*) {}
void AtPopHeap() {}

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static const char* g_op = "";

static void eq(long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 30) std::printf("DIFF %s: original 0x%llX, recovered 0x%llX\n", g_op, o, r);
}
static void same(const unsigned char* o, const unsigned char* r) {
    g_checks++;
    if (std::memcmp(o, r, sizeof(AtPixelFormat)) == 0) return;
    if (g_failures++ < 30) {
        std::printf("DIFF %s: original", g_op);
        for (size_t i = 0; i < sizeof(AtPixelFormat); i++) std::printf(" %02X", o[i]);
        std::printf(", recovered");
        for (size_t i = 0; i < sizeof(AtPixelFormat); i++) std::printf(" %02X", r[i]);
        std::printf("\n");
    }
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnCopy)(void*, void*, const void*);
typedef void(__fastcall* FnSet)(void*, void*, unsigned short, unsigned short, unsigned, unsigned, unsigned, unsigned);
typedef void(__fastcall* FnRgba)(void*, void*, unsigned short, unsigned, unsigned, unsigned, unsigned, int);
typedef void(__fastcall* FnTwo)(void*, void*, unsigned short, unsigned, unsigned, int);
typedef void(__fastcall* FnDxt)(void*, void*, int);
typedef unsigned(__fastcall* FnU)(const void*, void*);
typedef unsigned(__fastcall* FnPack)(const void*, void*, const unsigned char*);
typedef void(__fastcall* FnUnpack)(const void*, void*, unsigned, unsigned char*);
typedef int(__fastcall* FnCmp)(const void*, void*, const void*);
typedef void(__fastcall* FnRead)(void*, void*, void*);

struct ScriptToken {
    int type;
    int value;
    const char* text;
};

// The same script on both sides, each from the start.
class Script : public AtIoTokenizer {
public:
    std::vector<ScriptToken> m_tokens;
    size_t m_next = 0;
    std::vector<int> m_errors;

    int Next() override {
        if (m_pushedBack != 0) {
            m_pushedBack = 0;
            return m_type;
        }
        if (m_next >= m_tokens.size()) {
            m_type = kEnd;
            return kEnd;
        }
        const ScriptToken& t = m_tokens[m_next++];
        m_type = t.type;
        m_int = t.value;
        if (t.text) strcpy_s(m_string, sizeof m_string, t.text);
        return t.type;
    }
    void Error(int code) override { m_errors.push_back(code); }
    void PutInt(int) override {}
    void PutReal(float) override {}
    void PutString(const char*) override {}
    void PutComment(const char*) override {}
    void PutKeyword(int) override {}
    void PutLeftBracket() override {}
    void PutRightBracket() override {}
    void PutLeftCurly() override {}
    void PutRightCurly() override {}
    void PutComma() override {}
    void PutSemicolon() override {}
    void PutColon() override {}
    void PutExclamation() override {}
    void PutPercent() override {}
    void PutCaret() override {}
    void PutAmpersand() override {}
    void PutTimes() override {}
    void PutLeftParen() override {}
    void PutRightParen() override {}
    void PutMinus() override {}
    void PutPlus() override {}
    void PutDivide() override {}
    void PutEquals() override {}
    void PutLess() override {}
    void PutGreater() override {}
    void PutOr() override {}
    void PutQuestion() override {}
    void PutAt() override {}
    void PutTilde() override {}
    void PutNewline() override {}
    void PutSpace() override {}
    void PutSpaces(unsigned int) override {}
    void PutTab() override {}
    void PutTabs(unsigned int) override {}
    void PutName(const char*) override {}
    void PutRaw(const char*) override {}
};

// Dirty storage on both sides: what a function does not set must stay.
struct Pair {
    unsigned char o[sizeof(AtPixelFormat)];
    unsigned char r[sizeof(AtPixelFormat)];
    Pair() {
        std::memset(o, 0xCD, sizeof o);
        std::memset(r, 0xCD, sizeof r);
    }
    AtPixelFormat* R() { return (AtPixelFormat*)r; }
};

static void run_setters(Rng& rng) {
    const unsigned short flags = (unsigned short)rng.next(), bits = (unsigned short)rng.next();
    const unsigned a = mask(rng), b = mask(rng), c = mask(rng), d = mask(rng);
    const int swizz = (rng.next() % 3 == 0) ? 0 : (rng.next() % 2) ? 1 : (int)rng.next();
    {
        Pair p;
        g_op = "constructor";
        at<Fn0>(0x00042C90)(p.o, nullptr);
        new (p.r) AtPixelFormat;
        same(p.o, p.r);
    }
    {
        Pair p;
        g_op = "clear";
        at<Fn0>(0x00042D60)(p.o, nullptr);
        p.R()->Clear();
        same(p.o, p.r);
    }
    {
        Pair p, q;
        g_op = "set";
        at<FnSet>(0x00042420)(p.o, nullptr, flags, bits, a, b, c, d);
        p.R()->Set(flags, bits, a, b, c, d);
        same(p.o, p.r);
        g_op = "copy";
        at<FnCopy>(0x000423F0)(q.o, nullptr, p.o);
        q.R()->Copy(*p.R());
        same(q.o, q.r);
    }
    {
        Pair p;
        g_op = "rgba";
        at<FnRgba>(0x00042450)(p.o, nullptr, bits, a, b, c, d, swizz);
        p.R()->SetRgba(bits, a, b, c, d, swizz);
        same(p.o, p.r);
    }
    {
        Pair p;
        g_op = "index";
        at<FnTwo>(0x00042490)(p.o, nullptr, bits, a, b, swizz);
        p.R()->SetIndex(bits, a, b, swizz);
        same(p.o, p.r);
    }
    {
        Pair p;
        g_op = "lum";
        at<FnTwo>(0x000424D0)(p.o, nullptr, bits, a, b, swizz);
        p.R()->SetLum(bits, a, b, swizz);
        same(p.o, p.r);
    }
    {
        Pair p;
        g_op = "depth";
        at<FnTwo>(0x00042510)(p.o, nullptr, bits, a, b, swizz);
        p.R()->SetDepth(bits, a, b, swizz);
        same(p.o, p.r);
    }
    typedef void (AtPixelFormat::*Dxt)(int);
    static const struct { uint32_t va; Dxt fn; const char* name; } kDxt[] = {{0x00042550, &AtPixelFormat::SetDxt1, "dxt1"}, {0x00042580, &AtPixelFormat::SetDxt2, "dxt2"}, {0x000425B0, &AtPixelFormat::SetDxt3, "dxt3"}, {0x000425E0, &AtPixelFormat::SetDxt4, "dxt4"}, {0x00042610, &AtPixelFormat::SetDxt5, "dxt5"}};
    for (const auto& k : kDxt) {
        Pair p;
        g_op = k.name;
        at<FnDxt>(k.va)(p.o, nullptr, swizz);
        (p.R()->*k.fn)(swizz);
        same(p.o, p.r);
    }
}

static void run_queries(Rng& rng, Hash& hash) {
    const AtPixelFormat f = format(rng);
    const AtPixelFormat g = related(rng, f);
    typedef unsigned (AtPixelFormat::*Q)() const;
    static const struct { uint32_t va; Q fn; const char* name; } kQ[] = {{0x000426A0, &AtPixelFormat::Bits0, "bits0"}, {0x000426D0, &AtPixelFormat::Bits1, "bits1"}, {0x00042640, &AtPixelFormat::Bits2, "bits2"}, {0x00042670, &AtPixelFormat::Bits3, "bits3"},
                                                                  {0x00042700, &AtPixelFormat::Shift0, "shift0"}, {0x00042720, &AtPixelFormat::Shift1, "shift1"}, {0x00042740, &AtPixelFormat::Shift2, "shift2"}, {0x00042760, &AtPixelFormat::Shift3, "shift3"}};
    for (const auto& k : kQ) {
        g_op = k.name;
        const unsigned o = at<FnU>(k.va)(&f, nullptr);
        eq(o, (f.*k.fn)());
        hash.add(from_bits(o));
    }
    unsigned char bytes[4] = {(unsigned char)rng.next(), (unsigned char)rng.next(), (unsigned char)rng.next(), (unsigned char)rng.next()};
    g_op = "pack";
    const unsigned packed = at<FnPack>(0x00042780)(&f, nullptr, bytes);
    eq(packed, f.Pack(bytes));
    hash.add(from_bits(packed));
    g_op = "unpack";
    const unsigned pixel = (rng.next() % 2) ? packed : rng.next();
    unsigned char uo[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA}, ur[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    at<FnUnpack>(0x00042920)(&f, nullptr, pixel, uo);
    f.Unpack(pixel, ur);
    eq(std::memcmp(uo, ur, 6), 0);
    uint32_t w;
    std::memcpy(&w, uo, 4);
    hash.add(from_bits(w));
    g_op = "equals";
    const int e = at<FnCmp>(0x00042AC0)(&f, nullptr, &g);
    eq(e, f.Equals(g));
    g_op = "same layout";
    const int l = at<FnCmp>(0x00042B10)(&f, nullptr, &g);
    eq(l, f.SameLayout(g));
    g_op = "at least";
    const int a = at<FnCmp>(0x00042BD0)(&f, nullptr, &g);
    eq(a, f.AtLeast(g));
    const int a2 = at<FnCmp>(0x00042BD0)(&g, nullptr, &f);
    eq(a2, g.AtLeast(f));
    hash.add(from_bits((uint32_t)(e | l << 8 | a << 16 | a2 << 24)));
}

static void run_read() {
    typedef AtIoTokenizer T;
    static const char* const kNames[] = {"RGBA", "LUM", "INDEX", "BUMP", "DEPTH", "FLOAT", "DXT1", "DXT2", "DXT3", "DXT4", "DXT5", "SWIZZ", "TILED", "LUMD", "RGBA_SWIZZ", "rgba", "", "LUMDXT1", "NOTHING"};
    std::vector<std::vector<ScriptToken>> scripts;
    const std::vector<ScriptToken> tail = {{T::kInt, 0x11, nullptr}, {T::kInt, 0x22, nullptr}, {T::kInt, 0x33, nullptr}, {T::kInt, (int)0xFF000000, nullptr}};
    const auto with = [&](std::vector<ScriptToken> s) {
        s.insert(s.end(), tail.begin(), tail.end());
        scripts.push_back(s);
    };
    // Flags as a number, wider than the field.
    with({{T::kInt, 32, nullptr}, {T::kInt, 0x12345, nullptr}});
    // One name, two, each of them; then a token that is neither, which is put back and so is the next read.
    for (const char* n : kNames) with({{T::kInt, 16, nullptr}, {T::kString, 0, n}});
    with({{T::kInt, 16, nullptr}, {T::kString, 0, "DXT1"}, {T::kString, 0, "SWIZZ"}, {T::kString, 0, "TILED"}});
    // Nothing named: the flags stay.
    with({{T::kInt, 16, nullptr}});
    scripts.push_back({{T::kInt, 16, nullptr}, {T::kReal, 0, nullptr}, {T::kInt, 1, nullptr}});
    // Bits not a number: reported, and the old value is used.
    with({{T::kString, 0, "X"}, {T::kInt, 1, nullptr}});
    // Running out.
    scripts.push_back({});
    scripts.push_back({{T::kInt, 16, nullptr}, {T::kString, 0, "RGBA"}});
    for (const auto& script : scripts) {
        for (const unsigned short start : {(unsigned short)0, (unsigned short)0x8001}) {
            Pair p;
            ((AtPixelFormat*)p.o)->m_flags = start;
            p.R()->m_flags = start;
            Script a, b;
            a.m_tokens = script;
            b.m_tokens = script;
            a.m_int = b.m_int = 0x5A5A;
            g_op = "read";
            at<FnRead>(0x00042CB0)(p.o, nullptr, &a);
            p.R()->Read(&b);
            same(p.o, p.r);
            eq((long long)a.m_next, (long long)b.m_next);
            eq(a.m_pushedBack, b.m_pushedBack);
            eq((long long)a.m_errors.size(), (long long)b.m_errors.size());
            for (size_t i = 0; i < a.m_errors.size() && i < b.m_errors.size(); i++) eq(a.m_errors[i], b.m_errors[i]);
        }
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    int count = 300000;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--count") && i + 1 < argc) count = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.empty()) { std::printf("usage: oracle_fmt <default.xbe> [--count N] [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    if (entries && !arm_entries(entries)) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.

    Rng setters{99};
    for (int i = 0; i < count / 10; i++) run_setters(setters);
    run_read();
    Rng queries{1234};
    Hash hash;
    for (int i = 0; i < count; i++) run_queries(queries, hash);
    std::printf("hash of the original's results, %d rounds: %016llX\n", count, hash.h);
    report_entries();
    std::printf("%s: %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
