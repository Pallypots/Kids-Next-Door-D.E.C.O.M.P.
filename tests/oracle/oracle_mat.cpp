// Differential oracle for the material (Windows x86, local only). See docs/verification.md.
//
//   oracle_mat <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Every text case holding a material is an .AGM file of the game: 'Material "name" {', often after shaders. It is tokenized once by the recovered tokenizer, which the tok suite compares with the original's over the same files, and the
// tokens are then handed to both sides through one scripted tokenizer, on which the original's own helpers run. What finds a shader is replaced on both sides, by the same answer.
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"
#include "at/AtMaterial.h"
#include "at/AtMaterialDatabase.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;
static std::string g_log[2];
static int g_side;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 100).c_str(), what, o, r);
}
static void eqs(const char* what, const std::string& o, const std::string& r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 100).c_str(), what, o.c_str(), r.c_str());
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }
static void redirect(uint32_t va, const void* to) {
    unsigned char* p = (unsigned char*)(uintptr_t)va;
    p[0] = 0xE9;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)to - (va + 5));
}

struct ScriptToken {
    int type;
    int value;
    float real;
    std::string text;
};

class Script : public AtIoTokenizer {
public:
    const std::vector<ScriptToken>* m_tokens = nullptr;
    size_t m_next = 0;
    std::string* m_log = nullptr;

    int Next() override {
        if (m_pushedBack != 0) {
            m_pushedBack = 0;
            return m_type;
        }
        if (m_next >= m_tokens->size()) {
            m_type = kRightCurly;  // Keeps a reader from running on past the script.
            return kRightCurly;
        }
        const ScriptToken& t = (*m_tokens)[m_next++];
        m_type = t.type;
        m_int = t.value;
        m_real = t.real;
        strcpy_s(m_string, sizeof m_string, t.text.c_str());
        m_stringLength = (unsigned int)t.text.size();
        return t.type;
    }
    void Error(int code) override { *m_log += "error " + std::to_string(code) + " at " + std::to_string(m_next) + "; "; }
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
    void PutRaw(const char*) {}
};

// The one concrete thing: slot 0 notes that it ran.
class Concrete : public AtMaterial {
public:
    void OnLoaded(AtMaterialOwner* owner) override { g_log[1] += "loaded " + std::to_string((uintptr_t)owner) + "; "; }
};
static void __fastcall o_loaded(void*, void*, void* owner) { g_log[0] += "loaded " + std::to_string((uintptr_t)owner) + "; "; }
static const void* g_table[5] = {(const void*)o_loaded, (const void*)(uintptr_t)0x00038F00, (const void*)(uintptr_t)0x00038C50, (const void*)(uintptr_t)0x00038370, (const void*)(uintptr_t)0x00038390};

// A shader's name decides the answer: names beginning with X are not found.
static void* answer(const char* name, const char* ns, int side) {
    // Names are eight bytes, with no terminator when full.
    const std::string shown(name, strnlen(name, 8));
    g_log[side] += "find " + shown + " in " + (ns ? std::string(ns, strnlen(ns, 8)) : std::string("(none)")) + "; ";
    if (name[0] == 'X') return nullptr;
    uint32_t h = 0x811C9DC5;
    for (const char c : shown) h = (h ^ (unsigned char)c) * 16777619;
    return (void*)(uintptr_t)(h | 0x1000);
}
static void* __fastcall o_find(void*, void*, const char* name, const char* ns) { return answer(name, ns, 0); }
void* AtMaterialOwner::FindShader(const char* name, const char* ns) { return answer(name, ns, 1); }

typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*, void*, const char*);
typedef void(__fastcall* FnCopy)(void*, void*, void*, void*, const void*);
typedef void(__fastcall* FnDescribe)(const void*, void*, void*);
typedef void(__fastcall* FnSet)(void*, void*, unsigned char, unsigned char);
typedef void(__fastcall* FnColor)(void*, void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned int);
typedef int(__fastcall* FnBlend)(void*, void*, void*, int);
typedef int(__fastcall* FnTok)(void*, void*, void*);

struct Both {
    unsigned char o[sizeof(AtMaterial)];
    unsigned char r[sizeof(AtMaterial)];
    AtMaterial* O() { return (AtMaterial*)o; }
    AtMaterial* R() { return (AtMaterial*)r; }
    void make(unsigned char fill) {
        std::memset(o, fill, sizeof o);
        std::memset(r, fill, sizeof r);
        at<Fn0>(0x00038EB0)(o, nullptr);
        new (r) Concrete;
        *(const void***)o = g_table;
    }
    void same(const char* what) {
        g_checks++;
        for (size_t i = 4; i < sizeof o; i++) {  // All but the vftable.
            if (o[i] != r[i]) {
                if (g_failures++ < 25) std::printf("DIFF %s: %s: +0x%X: original 0x%02X, recovered 0x%02X\n", g_context.substr(0, 100).c_str(), what, (unsigned)i, o[i], r[i]);
                return;
            }
        }
    }
};

// The fields of a database that a material uses. The oracle copies these by value, so it holds them as a plain layout rather than a class with a destructor.
struct AtMaterialStore {
    char m_unknown00[0x18];
    unsigned int m_stageCapacity;
    unsigned int m_stageUsed;
    void** m_stages;
    unsigned int m_textureCapacity;
    unsigned int m_textureUsed;
    void** m_textures;
};
static_assert(offsetof(AtMaterialDatabase, m_stages) == offsetof(AtMaterialStore, m_stages), "layout");
static_assert(offsetof(AtMaterialDatabase, m_textures) == offsetof(AtMaterialStore, m_textures), "layout");
static AtMaterialDatabase* as_db(AtMaterialStore* s) { return (AtMaterialDatabase*)s; }

static AtMaterialOwner g_owner[2];
static void* g_stagesArray[64];
static void* g_texturesArray[64];
static AtMaterialStore g_store;

static void run_read(const std::vector<ScriptToken>& tokens, unsigned char fill, const char* ns) {
    Both m;
    m.make(fill);
    g_log[0].clear();
    g_log[1].clear();
    Script a, b;
    a.m_tokens = &tokens;
    b.m_tokens = &tokens;
    a.m_log = &g_log[0];
    b.m_log = &g_log[1];
    at<FnRead>(0x00038590)(m.o, nullptr, &g_owner[0], &g_store, &a, ns);
    m.R()->Read(&g_owner[0], as_db(&g_store), &b, ns);
    m.same("read");
    eqs("log", g_log[0], g_log[1]);
    eq("taken", (long long)a.m_next, (long long)b.m_next);
    // Read again on the same object: it first resets.
    a.m_next = b.m_next = 0;
    a.m_pushedBack = b.m_pushedBack = 0;
    at<FnRead>(0x00038590)(m.o, nullptr, &g_owner[0], &g_store, &a, ns);
    m.R()->Read(&g_owner[0], as_db(&g_store), &b, ns);
    m.same("read again");
    eqs("log", g_log[0], g_log[1]);
    // Describing it.
    unsigned char da[sizeof(AtMaterialDesc)], db[sizeof(AtMaterialDesc)];
    std::memset(da, 0xAB, sizeof da);
    std::memset(db, 0xAB, sizeof db);
    at<FnDescribe>(0x00038D60)(m.o, nullptr, da);
    m.R()->Describe((AtMaterialDesc*)db);
    eq("described", std::memcmp(da, db, sizeof da), 0);
    // Copying it, into a store of each side's own.
    static void* stages[2][256];
    static void* textures[2][256];
    AtMaterialStore sa = g_store, sb = g_store;
    std::memset(stages, 0, sizeof stages);
    std::memset(textures, 0, sizeof textures);
    sa.m_stages = stages[0]; sb.m_stages = stages[1];
    sa.m_textures = textures[0]; sb.m_textures = textures[1];
    sa.m_stageUsed = sb.m_stageUsed = 3;
    sa.m_textureUsed = sb.m_textureUsed = 5;
    Both c;
    c.make(0x77);
    at<FnCopy>(0x00038AD0)(c.o, nullptr, &g_owner[0], &sa, m.o);
    c.R()->Copy(&g_owner[0], as_db(&sb), *m.R());
    eq("copied flags", c.O()->m_flags, c.R()->m_flags);
    eq("stages used", sa.m_stageUsed, sb.m_stageUsed);
    eq("textures used", sa.m_textureUsed, sb.m_textureUsed);
    eq("stages", std::memcmp(stages[0], stages[1], sizeof stages[0]), 0);
    eq("textures", std::memcmp(textures[0], textures[1], sizeof textures[0]), 0);
    eq("stage index", (char*)c.O()->m_stages - (char*)stages[0], c.R()->m_stages ? (char*)c.R()->m_stages - (char*)stages[1] : (char*)c.O()->m_stages - (char*)stages[0]);
    eq("texture index", (char*)c.O()->m_textures - (char*)textures[0], c.R()->m_textures ? (char*)c.R()->m_textures - (char*)textures[1] : (char*)c.O()->m_textures - (char*)textures[0]);
    // All but the store and array fields, which differ by side.
    c.O()->m_store = c.R()->m_store = nullptr;
    c.O()->m_stages = c.R()->m_stages = nullptr;
    c.O()->m_textures = c.R()->m_textures = nullptr;
    c.same("copied");
    eqs("log", g_log[0], g_log[1]);
    // Resetting.
    at<Fn0>(0x00038C50)(c.o, nullptr);
    c.R()->Reset();
    c.same("reset");
    // The deleting destructor without the delete.
    eq("returns this", (uintptr_t)at<FnDel>(0x00038F00)(c.o, nullptr, 0), (uintptr_t)c.o);
    c.R()->~AtMaterial();
    c.same("destroyed");
}

static std::vector<unsigned char> read_file(const std::string& p) {
    FILE* f = nullptr;
    fopen_s(&f, p.c_str(), "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    std::vector<unsigned char> d((size_t)std::ftell(f));
    std::fseek(f, 0, SEEK_SET);
    if (!d.empty()) std::fread(d.data(), 1, d.size(), f);
    std::fclose(f);
    return d;
}

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

static std::vector<ScriptToken> tokenize(std::vector<unsigned char>& d) {
    std::vector<ScriptToken> out;
    d.push_back(0);
    MemoryStream stream;
    stream.m_base = (char*)d.data();
    stream.m_cur = stream.m_base;
    stream.m_limit = stream.m_base + d.size() - 1;
    stream.m_size = (unsigned int)d.size() - 1;
    AtIoTextTokenizer t;
    t.SetStream(&stream);
    t.SetKeywords(AtMaterial::s_keywords, AtMaterial::kKeywordCount, nullptr);
    for (;;) {
        const int k = t.Next();
        if (k == AtIoTokenizer::kEnd) break;
        ScriptToken s;
        s.type = k;
        s.value = t.m_int;
        s.real = t.m_real;
        s.text = t.m_string;
        out.push_back(s);
    }
    stream.m_base = nullptr;
    stream.m_cur = nullptr;
    stream.m_limit = nullptr;
    t.SetStream(nullptr);
    return out;
}

enum { kMaterialWord = 63 };

static unsigned g_files, g_materials;
static unsigned g_used[140];

static void run_file(const std::string& path) {
    std::vector<unsigned char> d = read_file(path);
    static const char kMark[] = "Material \"";
    if (std::search(d.begin(), d.end(), kMark, kMark + sizeof kMark - 1) == d.end()) return;
    g_context = path;
    const std::vector<ScriptToken> tokens = tokenize(d);
    g_files++;
    for (size_t i = 0; i + 2 < tokens.size(); i++) {
        if (tokens[i].type != kMaterialWord || tokens[i + 1].type != AtIoTokenizer::kString || tokens[i + 2].type != AtIoTokenizer::kLeftCurly) continue;
        size_t e = i + 2;
        while (e < tokens.size() && tokens[e].type != AtIoTokenizer::kRightCurly) {
            if (tokens[e].type >= 0 && tokens[e].type < 140) g_used[tokens[e].type]++;
            e++;
        }
        if (e >= tokens.size()) break;
        const std::vector<ScriptToken> one(tokens.begin() + (long)i + 2, tokens.begin() + (long)e + 1);
        g_materials++;
        g_context = path + " #" + std::to_string(g_materials);
        run_read(one, 0xCD, "Space");
        i = e;
    }
}

static ScriptToken word(int id) { return {id, 0, 0, ""}; }
static ScriptToken number(int v) { return {AtIoTokenizer::kInt, v, 0, ""}; }
static ScriptToken real(float v) { return {AtIoTokenizer::kReal, 0, v, ""}; }
static ScriptToken name(const char* s) { return {AtIoTokenizer::kString, 0, 0, s}; }

static void run_written() {
    const ScriptToken open = word(AtIoTokenizer::kLeftCurly), close = word(AtIoTokenizer::kRightCurly);
    const auto with = [&](std::vector<ScriptToken> body, const char* what) {
        std::vector<ScriptToken> s = {open};
        s.insert(s.end(), body.begin(), body.end());
        s.push_back(close);
        g_context = what;
        run_read(s, 0xCD, "Space");
        g_owner[0].m_noTextures = -1;
        run_read(s, 0x33, nullptr);
        g_owner[0].m_noTextures = 0;
    };
    // Every keyword by itself, followed by what it reads, and each of the words those read.
    const std::vector<ScriptToken> four = {number(1), number(2), number(3), number(4)};
    for (int id = 57; id <= 127; id++) with({word(id), number(7), number(8), number(9), number(10)}, ("keyword " + std::to_string(id)).c_str());
    for (int id = 97; id <= 128; id++) {
        with({word(75), word(id)}, ("blend " + std::to_string(id)).c_str());
        with({word(76), word(id)}, ("blend alpha " + std::to_string(id)).c_str());
    }
    for (int id = 75; id <= 76; id++) with({word(75), word(100), word(76), word(id == 75 ? 108 : 109)}, "both blends");
    for (int blend = 99; blend <= 111; blend++) {
        with({word(75), word(blend), word(93)}, "blend then no fog");
        with({word(93), word(75), word(blend)}, "no fog then blend");
    }
    for (int id = 110; id <= 119; id++) {
        with({word(74), word(id), number(0x80)}, ("alpha compare " + std::to_string(id)).c_str());
        with({word(97), word(id)}, ("depth compare " + std::to_string(id)).c_str());
    }
    for (int a = 118; a <= 127; a += 1) {
        with({word(85), word(a), word(127 - (a - 118)), word(111 + (a - 118) % 8), number(9)}, ("stencil " + std::to_string(a)).c_str());
    }
    with({word(67), number(1), number(2), number(3), word(78), number(4), word(77)}, "constants");
    with({word(64), number(1), number(2), number(3), number(4), word(65), number(5), number(6), number(7), number(8), word(66), number(9), number(10), number(11), number(12)}, "colours");
    with({word(68), word(69), word(70), word(71), word(72), word(73), word(98), word(99), word(87), word(88), word(87)}, "opposites");
    with({word(88), word(87), word(88)}, "double last");
    with({word(87), word(88), word(87)}, "single last");
    with({word(80), name("Shade"), number(1), number(2), number(3), number(4), word(79), number(9)}, "shader then simple");
    with({word(82), word(83), real(1.5f), word(84), number(-3), word(86), word(89), word(90), real(2.5f), word(91), word(92), real(-0.5f), word(94), word(95), word(96)}, "the rest");
    // Textures: in bounds, on the edge, past it; a shader found and not; with and without a name space; no stages.
    for (const int first : {0, 1, 63, 64, -1}) with({word(79), number(first)}, "simple shader");
    for (const char* shader : {"Shade", "XMissing"}) {
        for (const int stage : {0, 1, 62, 64}) {
            for (const int count : {0, 1, 2, 255, 300}) {
                with({word(80), name(shader), number(stage), number(count), number(stage), number(count)}, "shader");
                with({word(81), name(shader), name("Other"), number(stage), number(count), number(stage), number(count)}, "shader in a space");
            }
        }
    }
    // Names of nine characters and more are reported by the original's own helper.
    with({word(80), name("LongerThanEight"), number(0), number(0), number(0), number(0)}, "long name");
    // The wrong kind of thing where a number or a real should be.
    with({word(90), number(5), word(67), real(1.0f), name("x"), word(99)}, "wrong kinds");
    // Not a left curly first.
    {
        const std::vector<ScriptToken> s = {word(68), close};
        g_context = "no left curly";
        run_read(s, 0xCD, "Space");
    }
}

// Over something already there: what a second read or copy has to clear, a blend mode without its flag, and fields no text sets.
static void run_over() {
    const ScriptToken open = word(AtIoTokenizer::kLeftCurly), close = word(AtIoTokenizer::kRightCurly);
    const std::vector<ScriptToken> full = {open, word(90), real(2.5f), word(92), real(1.5f), word(84), number(5), word(80), name("Shade"), number(1), number(2), number(3), number(4), word(75), word(101), word(76), word(104), word(74), word(113), number(9), word(85), word(120), word(121), word(112), number(6), close};
    const std::vector<ScriptToken> bare = {open, close};
    const auto read = [&](Both& m, const std::vector<ScriptToken>& tokens) {
        Script a, b;
        a.m_tokens = &tokens;
        b.m_tokens = &tokens;
        a.m_log = &g_log[0];
        b.m_log = &g_log[1];
        at<FnRead>(0x00038590)(m.o, nullptr, &g_owner[0], &g_store, &a, "Space");
        m.R()->Read(&g_owner[0], as_db(&g_store), &b, "Space");
    };
    g_log[0].clear();
    g_log[1].clear();
    {
        g_context = "read over a loaded one";
        Both m;
        m.make(0xCD);
        read(m, full);
        m.same("full");
        read(m, bare);
        m.same("then bare");
        eq("cleared", m.R()->m_power == 0.0f && m.R()->m_shader == nullptr && m.R()->m_blend == 0, 1);
    }
    for (const unsigned char mode : {(unsigned char)1, (unsigned char)9, (unsigned char)2}) {
        g_context = "a blend mode with no flag";
        Both m;
        m.make(0xCD);
        m.O()->m_blend = m.R()->m_blend = mode;
        read(m, bare);
        m.same("fog");
    }
    {
        g_context = "copy over a loaded one";
        Both from, to;
        from.make(0xCD);
        read(from, bare);
        to.make(0xCD);
        read(to, full);
        from.O()->m_unknown28 = from.R()->m_unknown28 = 0x1234;
        AtMaterialStore sa = g_store, sb = g_store;
        at<FnCopy>(0x00038AD0)(to.o, nullptr, &g_owner[0], &sa, from.o);
        to.R()->Copy(&g_owner[0], as_db(&sb), *from.R());
        eq("store", to.O()->m_store == as_db(&sa), to.R()->m_store == as_db(&sb));
        to.O()->m_store = to.R()->m_store = nullptr;  // One store for each side.
        to.same("over a loaded one");
        eq("cleared", to.R()->m_power == 0.0f && to.R()->m_sortBias == 0.0f, 1);
        eq("carried", to.R()->m_unknown28, 0x1234);
    }
    {
        g_context = "counts with no arrays, arrays with no counts";
        static void* one[2][16];
        for (int how = 0; how < 2; how++) {
            Both from, to;
            from.make(0xCD);
            read(from, bare);
            to.make(0xCD);
            for (AtMaterial* f : {from.O(), from.R()}) {
                f->m_stageCount = how == 0 ? 3 : 0;
                f->m_textureCount = how == 0 ? 3 : 0;
                f->m_stages = how == 0 ? nullptr : g_stagesArray;
                f->m_textures = how == 0 ? nullptr : g_texturesArray;
            }
            AtMaterialStore sa = g_store, sb = g_store;
            sa.m_stages = one[0]; sa.m_textures = one[0];
            sb.m_stages = one[1]; sb.m_textures = one[1];
            sa.m_stageUsed = sb.m_stageUsed = 1;
            sa.m_textureUsed = sb.m_textureUsed = 2;
            at<FnCopy>(0x00038AD0)(to.o, nullptr, &g_owner[0], &sa, from.o);
            to.R()->Copy(&g_owner[0], as_db(&sb), *from.R());
            eq("stages used", sa.m_stageUsed, sb.m_stageUsed);
            eq("textures used", sa.m_textureUsed, sb.m_textureUsed);
            eq("stage array set", to.O()->m_stages == nullptr, to.R()->m_stages == nullptr);
            eq("texture array set", to.O()->m_textures == nullptr, to.R()->m_textures == nullptr);
        }
    }
}

static void run_small() {
    g_context = "small";
    // Descriptions, on dirty memory; the colour alone.
    unsigned char a[sizeof(AtMaterialDesc)], b[sizeof(AtMaterialDesc)];
    std::memset(a, 0xCD, sizeof a);
    std::memset(b, 0xCD, sizeof b);
    at<Fn0>(0x00038E40)(a, nullptr);
    new (b) AtMaterialDesc;
    eq("description", std::memcmp(a, b, sizeof a), 0);
    unsigned char ca[4] = {1, 2, 3, 4}, cb[4] = {1, 2, 3, 4};
    at<Fn0>(0x000412B0)(ca, nullptr);
    ((AtColor*)cb)->SetWhite();
    eq("colour", std::memcmp(ca, cb, 4), 0);
    const std::vector<ScriptToken> numbers = {number(1), number(300), number(-1), number(4)};
    Script s1, s2;
    std::string l1, l2;
    s1.m_tokens = &numbers; s2.m_tokens = &numbers;
    s1.m_log = &l1; s2.m_log = &l2;
    at<FnColor>(0x00041270)(ca, nullptr, &s1);
    ((AtColor*)cb)->Read(&s2);
    eq("colour read", std::memcmp(ca, cb, 4), 0);
    // Slots 3 and 4, on their own.
    Both m;
    m.make(0xCD);
    at<FnSet>(0x00038370)(m.o, nullptr, 3, 200);
    m.R()->SetAlphaCompare(3, 200);
    m.same("set compare");
    at<Fn0>(0x00038390)(m.o, nullptr);
    m.R()->ClearAlphaCompare();
    m.same("clear compare");
    // The three word readers on their own.
    for (int id = 90; id <= 130; id++) {
        const std::vector<ScriptToken> one = {word(id)};
        Script t1, t2;
        l1.clear(); l2.clear();
        t1.m_tokens = &one; t2.m_tokens = &one;
        t1.m_log = &l1; t2.m_log = &l2;
        eq("blend", at<FnBlend>(0x000381A0)(m.o, nullptr, &t1, id), m.R()->ReadBlendMode(&t2, id));
        eqs("blend log", l1, l2);
        l1.clear(); l2.clear();
        t1.m_next = t2.m_next = 0;
        eq("stencil", at<FnTok>(0x00038250)(m.o, nullptr, &t1), m.R()->ReadStencilAction(&t2));
        eqs("stencil log", l1, l2);
        l1.clear(); l2.clear();
        t1.m_next = t2.m_next = 0;
        eq("compare", at<FnTok>(0x000382E0)(m.o, nullptr, &t1), m.R()->ReadCompare(&t2));
        eqs("compare log", l1, l2);
    }
    // The table, against the original's.
    g_context = "keywords";
    for (int i = 0; i < AtMaterial::kKeywordCount; i++) {
        const uint32_t* const row = (const uint32_t*)(uintptr_t)(0x002B15C0 + 8u * (unsigned)i);
        eq("id", (long long)row[0], AtMaterial::s_keywords[i].m_id);
        eqs("name", (const char*)(uintptr_t)row[1], AtMaterial::s_keywords[i].m_name);
    }
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* entries = nullptr;
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() < 2) { std::printf("usage: oracle_mat <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    at<void(__cdecl*)()>(0x00125BC7)();
    redirect(0x000383A0, (const void*)o_find);
    if (entries && !arm_entries(entries)) return 2;
    if (g_mutate) g_failures++, std::printf("DIFF deliberate\n");  // Positive control of the harness.

    std::memset(&g_store, 0, sizeof g_store);
    g_store.m_stageCapacity = 64;
    g_store.m_textureCapacity = 64;
    g_store.m_stages = g_stagesArray;
    g_store.m_textures = g_texturesArray;

    run_small();
    run_written();
    run_over();
    WIN32_FIND_DATAA found;
    const HANDLE list = FindFirstFileA((std::string(pos[1]) + "\\*").c_str(), &found);
    if (list == INVALID_HANDLE_VALUE) { std::printf("cannot list %s\n", pos[1]); return 2; }
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) run_file(std::string(pos[1]) + "\\" + found.cFileName);
    } while (FindNextFileA(list, &found));
    FindClose(list);
    std::printf("words used by the game's materials:");
    for (int i = 57; i < 140; i++) if (g_used[i]) std::printf(" %d:%u", i, g_used[i]);
    std::printf("\n");
    report_entries();
    std::printf("%s: %u files, %u materials; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files, g_materials, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
