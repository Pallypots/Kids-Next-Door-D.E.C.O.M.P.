// Differential oracle for the Flash user interface's text formats and level files, .AFX and .AFL (Windows x86, local only). See docs/verification.md.
//
//   oracle_afx <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AFX and .AFL file listed in index.txt, loaded through the file code on both sides; written text; and each reader called directly on a tokenizer of the harness's, whose Error can stop a reader
// that would otherwise never finish. Through the file code a bad word's report comes back, as it does not in the game, and the readers go on: so text sent that way always closes its braces, or both sides spin.
// The setter at 0x00058AF0 is not recovered: the recovered side uses this file's definition of it, the original its own code. Both are compared by what they store.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtFlashLevel.h"
#include "at/AtFlashTextFormat.h"
#include "at/AtHeap.h"
#include "at/AtIoTokenizer.h"
#include "at/AtMemory.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 160).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 160).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

// Here rather than in AtMemory.cpp, whose heap stack the doubles above replace.
AtHeap* AtHeapStack::s_contentHeap;

// The level's block is the first request after its file closes, so that is when memory runs out, on either side.
static bool g_fail_after_close;
class LevelFs : public MemFs {
public:
    int Close(AtIoFile* file) override {
        if (g_fail_after_close) g_fail_alloc = true;
        return MemFs::Close(file);
    }
};

void AtFlashHeapUser::SetHeap(void* heap) { m_heap = heap; }

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;

typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void(__fastcall* FnName)(void*, void*, const char*);
typedef void(__fastcall* FnCount)(void*, void*, unsigned);
typedef void(__fastcall* FnTok)(void*, void*, void*);
typedef void(__fastcall* FnLevel)(void*, void*, const char*, int);

static unsigned g_afx_files, g_afl_files, g_formats;

// ---- comparing --------------------------------------------------------------------------------------------------------

static void same_format(const AtFlashTextFormat* o, const AtFlashTextFormat* r) {
    eq("format's vftable", *(uint32_t*)o, 0x002749D0);
    eq("format's bytes", std::memcmp((const char*)o + 4, (const char*)r + 4, sizeof(AtFlashTextFormat) - 4), 0);
}

static void same_table(const char* what, const AtFlashTextFormatTable* o, const AtFlashTextFormatTable* r) {
    g_recording = false;
    eq("initialised", o->m_initialised, r->m_initialised);
    eq("first group", o->m_firstGroup != nullptr, r->m_firstGroup != nullptr);
    eq("last group", o->m_lastGroup != nullptr, r->m_lastGroup != nullptr);
    eq("name", std::memcmp(o->m_name, r->m_name, 8), 0);
    eq("items", o->m_items != nullptr, r->m_items != nullptr);
    eq("item count", o->m_itemCount, r->m_itemCount);
    eq("group count", o->m_groupCount, r->m_groupCount);
    eq("heap", (uintptr_t)o->m_heap, (uintptr_t)r->m_heap);
    eq("loaded", o->m_loaded, r->m_loaded);
    if (o->m_items && r->m_items && o->m_itemCount == r->m_itemCount) {
        for (unsigned i = 0; i < o->m_itemCount; i++) {
            eq("item held", o->m_items[i] != nullptr, r->m_items[i] != nullptr);
            if (!o->m_items[i] || !r->m_items[i]) continue;
            eq("item's place", (char*)o->m_items[i] - (char*)o->m_items[0], (char*)r->m_items[i] - (char*)r->m_items[0]);
            same_format((AtFlashTextFormat*)o->m_items[i], (AtFlashTextFormat*)r->m_items[i]);
        }
        if (o->m_itemCount && o->m_items[0] && r->m_items[0]) eq("cookie", ((unsigned*)o->m_items[0])[-1], ((unsigned*)r->m_items[0])[-1]);
    }
    compare_logs(what);
}

// ---- text formats through the file code -------------------------------------------------------------------------------

static AtFlashTextFormatTable* make_o(unsigned char fill) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtFlashTextFormatTable));
    std::memset(o, fill, sizeof(AtFlashTextFormatTable));
    eq("constructor returns this", (uintptr_t)at<FnCtor>(0x00020280)(o, nullptr), (uintptr_t)o);
    eq("table's vftable", *(uint32_t*)o, 0x0027294C);
    return (AtFlashTextFormatTable*)o;
}
static AtFlashTextFormatTable* make_r(unsigned char fill) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(AtFlashTextFormatTable));
    std::memset(r, fill, sizeof(AtFlashTextFormatTable));
    return new (r) AtFlashTextFormatTable;
}

static void run_afx(const std::string& data, const std::string& context, unsigned char fill) {
    g_context = context;
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = fill;
    AtFlashTextFormatTable* o = make_o(fill);
    AtFlashTextFormatTable* r = make_r(fill);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtFlashTextFormatTable) - 4), 0);
    { On on(0); at<FnName>(0x00020310)(o, nullptr, "TEXT\\THING"); }
    { On on(1); r->Read("TEXT\\THING"); }
    same_table("read", o, r);
    if (fill == 0xCD) g_formats += r->m_itemCount;  // Each file runs on two fills, and counts once.
    // As 0x00020570 leaves it.
    o->m_loaded = r->m_loaded = 1;
    o->m_initialised = r->m_initialised = 1;
    { On on(0); at<Fn0>(0x000202A0)(o, nullptr); at<Fn0>(0x000202A0)(o, nullptr); }
    { On on(1); r->ReleaseFormats(); r->ReleaseFormats(); }
    same_table("released", o, r);
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00020620)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~AtFlashTextFormatTable(); }
    compare_logs("destroyed");
    std::free(o);
    std::free(r);
}

// ---- a level ----------------------------------------------------------------------------------------------------------

static void same_level(const char* what, const unsigned char* o, const unsigned char* r) {
    g_recording = false;
    const AtFlashLevel* a = (const AtFlashLevel*)o;
    const AtFlashLevel* b = (const AtFlashLevel*)r;
    eq("block", a->m_block != nullptr, b->m_block != nullptr);
    std::vector<unsigned char> x(o, o + sizeof(AtFlashLevel)), y(r, r + sizeof(AtFlashLevel));
    std::memset(x.data() + offsetof(AtFlashLevel, m_block), 0, 4);
    std::memset(y.data() + offsetof(AtFlashLevel, m_block), 0, 4);
    for (size_t i = 0; i + 4 <= x.size(); i += 4) {
        if (*(uint32_t*)&x[i] != *(uint32_t*)&y[i]) eq(("level word " + std::to_string(i)).c_str(), *(uint32_t*)&x[i], *(uint32_t*)&y[i]);
    }
    g_checks++;
    compare_logs(what);
}

static void run_afl(const std::string& data, const std::string& context, const char* name, void* heap, void* content) {
    g_context = context;
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
    g_fill = 0xCD;
    static unsigned char o[sizeof(AtFlashLevel)], r[sizeof(AtFlashLevel)];
    std::memset(o, 0xCD, sizeof o);
    std::memset(r, 0xCD, sizeof r);
    ((AtFlashLevel*)o)->m_heap = heap;
    ((AtFlashLevel*)r)->m_heap = heap;
    *(void**)(uintptr_t)0x002C60A8 = content;
    AtHeapStack::s_contentHeap = (AtHeap*)content;
    { On on(0); at<FnLevel>(0x000114A0)(o, nullptr, name, 77); }
    { On on(1); ((AtFlashLevel*)r)->Load(name, 77); }
    same_level("level", o, r);
    std::free(((AtFlashLevel*)o)->m_block);
    std::free(((AtFlashLevel*)r)->m_block);
}

// ---- the readers directly --------------------------------------------------------------------------------------------

class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

class DirectTokenizer : public AtIoTextTokenizer {
public:
    int m_errors = 0;
    void Error(int code) override {
        g_log[g_side].add('E', code);
        if (++m_errors > 12) *(volatile int*)8 = 0;  // Does not come back, as in the game. guarded() takes it from here.
    }
};

static const AtIoKeyword* const kImageKeywords = (const AtIoKeyword*)(uintptr_t)0x002727B0;

struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    explicit Direct(const std::string& t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(kImageKeywords, 21, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void run_format_text(const std::string& text) {
    for (const int entry : {0, 1}) {
        g_context = std::string("format [") + text + "]";
        static unsigned char o[sizeof(AtFlashTextFormat)], r[sizeof(AtFlashTextFormat)];
        std::memset(o, 0x33, sizeof o);
        std::memset(r, 0x33, sizeof r);
        at<FnCtor>(0x000304A0)(o, nullptr);
        new (r) AtFlashTextFormat;
        same_format((AtFlashTextFormat*)o, (AtFlashTextFormat*)r);
        // Values to be seen overwritten, and to be seen kept.
        std::memset(o + 0x20, 0x55, 0x48);
        std::memset(r + 0x20, 0x55, 0x48);
        Direct da(text), db(text);
        const bool oc = guarded([&] { On on(0); at<FnTok>(entry ? 0x000304F0 : 0x00030270)(o, nullptr, &da.tokenizer); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); if (entry) ((AtFlashTextFormat*)r)->Read(&db.tokenizer); else ((AtFlashTextFormat*)r)->ReadFields(&db.tokenizer); });
        g_recording = false;
        g_side = 0;
        eq("crash", oc, rc);
        same_format((AtFlashTextFormat*)o, (AtFlashTextFormat*)r);
        compare_logs("format read");
        eq("left at", da.stream.m_cur - da.stream.m_base, db.stream.m_cur - db.stream.m_base);
        { On on(0); at<Fn0>(0x00030510)(o, nullptr); }
        { On on(1); ((AtFlashTextFormat*)r)->~AtFlashTextFormat(); }
        eq("destroyed format", std::memcmp(o + 4, r + 4, sizeof(AtFlashTextFormat) - 4), 0);
        compare_logs("format destroyed");
    }
}

static void run_formats() {
    static const char* const bodies[] = {
        "",
        "BoundsRectangle 1.5 2.5 3.5 4.5",
        "BoundsRectangle 1.5 2.5 3.5",
        "BoundsRectangle \"a\" 2.5 3.5 4.5",
        "WordWrap",
        "MultiLine",
        "Password",
        "ReadOnly",
        "NoSelect",
        "Border",
        "FontHeight 9.25",
        "FontHeight \"a\"",
        "TextColor 1 2 3 4",
        "TextColor 511 2 3 4",
        "TextColor 1 2 3",
        "TextMaxLength 77",
        "TextMaxLength -2",
        "Alignment AlignmentLeft",
        "Alignment AlignmentRight",
        "Alignment AlignmentCenter",
        "Alignment AlignmentJustify",
        "Alignment Border",
        "Alignment 7",
        "LeftMargin 1.25",
        "RightMargin 2.25",
        "Indent 3.25",
        "Leading 4.25",
        "NameSpace",
        "TextFormat",
        "AlignmentLeft",
        "Bogus",
        "7",
        "BoundsRectangle 1.5 2.5 3.5 4.5 WordWrap MultiLine Password ReadOnly NoSelect Border FontHeight 1.5 TextColor 9 8 7 6 TextMaxLength 5 Alignment AlignmentCenter LeftMargin 1.5 RightMargin 2.5 Indent 3.5 Leading 4.5",
    };
    for (const char* body : bodies) run_format_text(std::string("TextFormat 3\n{\n") + body + "\n}\n");
    for (const char* text : {"", "TextFormat", "TextFormat 3", "TextFormat 3 {", "TextFormat 3 { Border", "TextFormat \"a\" { }", "TextFormat 3 Border }", "Border 3 { }"}) run_format_text(text);

    // A bare format: the base's own.
    g_context = "resource";
    {
        unsigned char o[sizeof(AtFlashResource)], r[sizeof(AtFlashResource)];
        std::memset(o, 0x33, sizeof o);
        std::memset(r, 0x33, sizeof r);
        eq("resource ctor returns this", (uintptr_t)at<FnCtor>(0x00023420)(o, nullptr), (uintptr_t)o);
        new (r) AtFlashResource;
        eq("resource's vftable", *(uint32_t*)o, 0x00272D28);
        eq("constructed resource", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
        std::memset(o + 12, 0x77, 16);
        std::memset(r + 12, 0x77, 16);
        at<Fn0>(0x00023400)(o, nullptr);
        ((AtFlashResource*)r)->ResetLinks();
        eq("reset resource", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
        std::memset(o + 12, 0x77, 16);
        std::memset(r + 12, 0x77, 16);
        at<Fn0>(0x00023440)(o, nullptr);
        ((AtFlashResource*)r)->~AtFlashResource();
        eq("destroyed resource", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
    }
    // Destroying a format never read leaves the fields be; Reset clears what is set.
    g_context = "format unread";
    {
        unsigned char o[sizeof(AtFlashTextFormat)], r[sizeof(AtFlashTextFormat)];
        std::memset(o, 0x33, sizeof o);
        std::memset(r, 0x33, sizeof r);
        at<FnCtor>(0x000304A0)(o, nullptr);
        new (r) AtFlashTextFormat;
        std::memset(o + 0x20, 0x55, 0x48);
        std::memset(r + 0x20, 0x55, 0x48);
        at<Fn0>(0x00030510)(o, nullptr);
        ((AtFlashTextFormat*)r)->~AtFlashTextFormat();
        eq("destroyed unread", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
        std::memset(o + 0x0C, 0x55, 0x5C);
        std::memset(r + 0x0C, 0x55, 0x5C);
        at<Fn0>(0x00030440)(o, nullptr);
        ((AtFlashTextFormat*)r)->Reset();
        eq("reset", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
    }
}

// ---- the table's base, and the rest ------------------------------------------------------------------------------------

static void run_tables() {
    g_context = "base table";
    g_fill = 0xCD;
    int heap_a, heap_b;
    for (void* own : {(void*)nullptr, (void*)&heap_a}) {
        for (void* fallback : {(void*)&heap_b, (void*)nullptr}) {
            *(void**)(uintptr_t)0x002C6270 = fallback;
            AtFlashResourceTable::s_defaultHeap = fallback;
            unsigned char o[sizeof(AtFlashResourceTable)], r[sizeof(AtFlashResourceTable)];
            std::memset(o, 0x33, sizeof o);
            std::memset(r, 0x33, sizeof r);
            at<FnCtor>(0x000172A0)(o, nullptr);
            new (r) AtFlashResourceTable;
            AtFlashResourceTable* a = (AtFlashResourceTable*)o;
            AtFlashResourceTable* b = (AtFlashResourceTable*)r;
            eq("base's vftable", *(uint32_t*)o, 0x00271F78);
            eq("constructed base", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
            a->m_heap = b->m_heap = own;
            { On on(0); at<FnCount>(0x00016FB0)(a, nullptr, 3); }
            { On on(1); b->AtFlashResourceTable::Allocate(3); }
            eq("count", a->m_itemCount, b->m_itemCount);
            eq("list", a->m_items != nullptr, b->m_items != nullptr);
            eq("cleared list", std::memcmp(a->m_items, b->m_items, 12), 0);
            compare_logs("allocated");
            AtFlashResource ra, rb;
            a->m_items[1] = &ra;
            b->m_items[1] = &rb;
            ra.m_references = rb.m_references = 5;
            at<FnCount>(0x00017040)(a, nullptr, 1);
            b->AtFlashResourceTable::Release(1);
            eq("references", ra.m_references, rb.m_references);
            // Groups as 0x000171B0 makes them, on both sides, for Clear to free. Its count is set for it.
            a->m_groupCount = b->m_groupCount = 0;
            at<FnCount>(0x000171B0)(a, nullptr, 3);
            at<FnCount>(0x000171B0)(b, nullptr, 3);
            eq("groups made", a->m_groupCount, b->m_groupCount);
            g_log[0].n = g_log[1].n = 0;
            { On on(0); at<Fn0>(0x000172E0)(a, nullptr); }
            { On on(1); b->Clear(); }
            compare_logs("not initialised");
            eq("left alone", a->m_groupCount, b->m_groupCount);
            a->m_initialised = b->m_initialised = 1;
            { On on(0); at<Fn0>(0x000172E0)(a, nullptr); }
            { On on(1); b->Clear(); }
            compare_logs("cleared");
            eq("cleared base", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
            // A count higher than the groups there are: the extra turns find none.
            a->m_groupCount = b->m_groupCount = 0;
            at<FnCount>(0x000171B0)(a, nullptr, 2);
            at<FnCount>(0x000171B0)(b, nullptr, 2);
            g_log[0].n = g_log[1].n = 0;
            a->m_initialised = b->m_initialised = 1;
            a->m_groupCount = b->m_groupCount = 4;
            { On on(0); at<Fn0>(0x000172E0)(a, nullptr); }
            { On on(1); b->Clear(); }
            compare_logs("cleared, a high count");
            eq("cleared base, a high count", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
            // A count lower: the rest are lost, and the table forgets them.
            a->m_groupCount = b->m_groupCount = 0;
            at<FnCount>(0x000171B0)(a, nullptr, 3);
            at<FnCount>(0x000171B0)(b, nullptr, 3);
            g_log[0].n = g_log[1].n = 0;
            a->m_initialised = b->m_initialised = 1;
            a->m_groupCount = b->m_groupCount = 1;
            { On on(0); at<Fn0>(0x000172E0)(a, nullptr); }
            { On on(1); b->Clear(); }
            compare_logs("cleared, a low count");
            eq("cleared base, a low count", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
            a->m_initialised = b->m_initialised = 1;
            { On on(0); at<Fn0>(0x000172E0)(a, nullptr); }
            { On on(1); b->Clear(); }
            compare_logs("cleared with no list");
            a->m_initialised = b->m_initialised = 1;
            { On on(0); at<Fn0>(0x00017360)(a, nullptr); }
            { On on(1); b->~AtFlashResourceTable(); }
            compare_logs("destroyed");
            eq("destroyed base", std::memcmp(o + 4, r + 4, sizeof o - 4), 0);
        }
    }
    *(void**)(uintptr_t)0x002C6270 = &g_heap_token;
    AtFlashResourceTable::s_defaultHeap = &g_heap_token;

    // Destroyed while loaded: the formats go too. And released with the base not initialised: the base's Clear does nothing, so what the release does itself shows.
    {
        g_context = "table destroyed loaded";
        AtFlashTextFormatTable* o = make_o(0xCD);
        AtFlashTextFormatTable* r = make_r(0xCD);
        { On on(0); at<FnCount>(0x00020430)(o, nullptr, 2); }
        { On on(1); r->Allocate(2); }
        same_table("allocated", o, r);
        o->m_loaded = r->m_loaded = 1;
        o->m_initialised = r->m_initialised = 1;
        { On on(0); at<Fn0>(0x00020520)(o, nullptr); }
        { On on(1); r->~AtFlashTextFormatTable(); }
        compare_logs("destroyed loaded");
        std::free(o);
        std::free(r);

        g_context = "table released, base not initialised";
        o = make_o(0xCD);
        r = make_r(0xCD);
        { On on(0); at<FnCount>(0x00020430)(o, nullptr, 2); }
        { On on(1); r->Allocate(2); }
        same_table("allocated", o, r);
        o->m_loaded = r->m_loaded = 1;
        o->m_heap = r->m_heap = &heap_a;
        { On on(0); at<Fn0>(0x000202A0)(o, nullptr); }
        { On on(1); r->ReleaseFormats(); }
        same_table("released", o, r);
        std::free(o);
        std::free(r);
    }

    // Deleting forms, with and without freeing.
    {
        void* p = std::malloc(sizeof(AtFlashResourceTable));
        at<FnCtor>(0x000172A0)(p, nullptr);
        void* q = new AtFlashResourceTable;
        { On on(0); eq("base deleting", (uintptr_t)at<FnDel>(0x000173A0)(p, nullptr, 1), (uintptr_t)p); }
        { On on(1); delete (AtFlashResourceTable*)q; }
        compare_logs("base deleted");
        void* s = std::malloc(sizeof(AtFlashResourceTable));
        at<FnCtor>(0x000172A0)(s, nullptr);
        { On on(0); eq("base deleting, no free", (uintptr_t)at<FnDel>(0x000173A0)(s, nullptr, 0), (uintptr_t)s); }
        compare_logs("base not freed");
        std::free(s);
        void* t = std::malloc(sizeof(AtFlashTextFormatTable));
        at<FnCtor>(0x00020280)(t, nullptr);
        void* u = new AtFlashTextFormatTable;
        { On on(0); eq("table deleting", (uintptr_t)at<FnDel>(0x00020620)(t, nullptr, 1), (uintptr_t)t); }
        { On on(1); delete (AtFlashTextFormatTable*)u; }
        compare_logs("table deleted");
    }

    // Allocate on the table, on each heap; and with no room.
    for (void* own : {(void*)nullptr, (void*)&heap_a}) {
        for (void* fallback : {(void*)&heap_b, (void*)nullptr}) {
            *(void**)(uintptr_t)0x002C6270 = fallback;
            AtFlashResourceTable::s_defaultHeap = fallback;
            g_context = "table allocate";
            g_fill = 0xCD;
            AtFlashTextFormatTable* o = make_o(0xCD);
            AtFlashTextFormatTable* r = make_r(0xCD);
            o->m_heap = r->m_heap = own;
            o->m_initialised = r->m_initialised = 1;
            o->m_loaded = r->m_loaded = 1;
            { On on(0); at<FnCount>(0x00020430)(o, nullptr, 3); }
            { On on(1); r->Allocate(3); }
            same_table("allocated", o, r);
            { On on(0); at<Fn0>(0x000202A0)(o, nullptr); }
            { On on(1); r->ReleaseFormats(); }
            same_table("released", o, r);
            // The list with no first format, and no list at all: nothing to destroy.
            o->m_loaded = r->m_loaded = 1;
            o->m_initialised = r->m_initialised = 1;
            { On on(0); at<Fn0>(0x000202A0)(o, nullptr); }
            { On on(1); r->ReleaseFormats(); }
            same_table("released with nothing", o, r);
            o->m_initialised = r->m_initialised = 1;
            o->m_loaded = r->m_loaded = 1;
            o->m_heap = r->m_heap = own;
            { On on(0); at<FnCount>(0x00016FB0)(o, nullptr, 2); }
            { On on(1); r->AtFlashResourceTable::Allocate(2); }
            same_table("empty list", o, r);
            { On on(0); at<Fn0>(0x000202A0)(o, nullptr); }
            { On on(1); r->ReleaseFormats(); }
            same_table("released an empty list", o, r);
            // Not loaded: left.
            o->m_heap = r->m_heap = &heap_a;
            { On on(0); at<Fn0>(0x000202A0)(o, nullptr); }
            { On on(1); r->ReleaseFormats(); }
            same_table("not loaded", o, r);
            std::free(o);
            std::free(r);
        }
    }
    *(void**)(uintptr_t)0x002C6270 = &g_heap_token;
    AtFlashResourceTable::s_defaultHeap = &g_heap_token;

    // No room: a raise, then what follows with null.
    g_context = "no room";
    {
        AtFlashTextFormatTable* o = make_o(0xCD);
        AtFlashTextFormatTable* r = make_r(0xCD);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnCount>(0x00020430)(o, nullptr, 2); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Allocate(2); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room");
        std::free(o);
        std::free(r);
    }
    // The list has room and the formats do not.
    {
        AtFlashTextFormatTable* o = make_o(0xCD);
        AtFlashTextFormatTable* r = make_r(0xCD);
        { On on(0); at<FnCount>(0x00016FB0)(o, nullptr, 2); }
        { On on(1); r->AtFlashResourceTable::Allocate(2); }
        g_log[0].n = g_log[1].n = 0;
        void** ol = (void**)o->m_items;
        void** rl = (void**)r->m_items;
        (void)ol; (void)rl;
        // 0x00020430 asks for the list first, so the second request cannot fail alone without counting: a count of none asks for nothing of the first kind.
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnCount>(0x00020430)(o, nullptr, 0); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); r->Allocate(0); });
        g_recording = false;
        g_side = 0;
        g_fail_alloc = false;
        eq("crash", oc, rc);
        compare_logs("no room for formats");
        std::free(o);
        std::free(r);
    }
}

static std::string every(const char* alignment) {
    return std::string("TextFormat 1\n{\nBoundsRectangle 1.5 2.5 3.5 4.5\nWordWrap\nMultiLine\nPassword\nReadOnly\nNoSelect\nBorder\nFontHeight 1.5\nTextColor 9 8 7 6\nTextMaxLength 5\nAlignment ") + alignment + "\nLeftMargin 1.5\nRightMargin 2.5\nIndent 3.5\nLeading 4.5\n}\n";
}

static void run_written_afx() {
    const std::string one = "TextFormat 0\n{\nBoundsRectangle 1.5 2.5 3.5 4.5\nReadOnly\nFontHeight 20.0\nTextColor 1 2 3 4\nAlignment AlignmentRight\nLeading 2.5\n}\n";
    const std::vector<std::string> texts = {
        "NameSpace \"A\"\nTextFormat[ 0 ]\n{\n}\n",
        "NameSpace \"LongerThanEight\"\nTextFormat[ 0 ]\n{\n}\n",
        "NameSpace \"\"\nTextFormat[ 0 ]\n{\n}\n",
        "NameSpace \"A\"\nTextFormat[ 1 ]\n{\n" + one + "}\n",
        "NameSpace \"A\"\nTextFormat[ 2 ]\n{\n" + one + one + "}\n",
        // Every word, through the reader's own table: the real files use only some.
        "NameSpace \"A\"\nTextFormat[ 4 ]\n{\n" + every("AlignmentLeft") + every("AlignmentRight") + every("AlignmentCenter") + every("AlignmentJustify") + "}\n",
        "NameSpace \"A\"\nTextFormat[ 1 ]\n{\n" + one + one + "}\n",
        "TextFormat[ 0 ]\n{\n}\n",
        "NameSpace TextFormat[ 0 ]\n{\n}\n",
        "NameSpace \"A\"\n[ 0 ]\n{\n}\n",
        "NameSpace \"A\"\nTextFormat 0 ]\n{\n}\n",
        "NameSpace \"A\"\nTextFormat[ 0 \n{\n}\n",
        "NameSpace \"A\"\nTextFormat[ \"a\" ]\n{\n}\n",
        "NameSpace \"A\"\nTextFormat[ 0 ]\n}\n",
        "NameSpace \"A\"\nTextFormat[ 0 ]\n{\n",
        "",
    };
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x33}) {
        for (const std::string& t : texts) run_afx(t, "afx [" + t + "]", fill);
    }
}

static void run_written_afl() {
    int heap_a, heap_b;
    const std::vector<std::string> texts = {
        "Level\n{\n}\n",
        "}\n",
        "Level\n{\nHeapSize 4096\n}\n",
        "Level\n{\nHeapSize 0\nFontSourceLevel 0\n}\n",
        "Level\n{\nFontSourceLevel Self\n}\n",
        "Level\n{\nFontSourceLevel All\n}\n",
        "Level\n{\nFontSourceLevel 7\n}\n",
        "Level\n{\nFontSourceLevel -3\n}\n",
        "Level\n{\nMaxVariables 7 FontSourceLevel Level\n}\n",
        "Level\n{\nMaxVariables 9 FontSourceLevel \"a\"\n}\n",
        "Level\n{\nFontSourceLevel 2.5\n}\n",
        "Level\n{\nMaxVariableValueLength 33\n}\n",
        "Level\n{\nMaxVariables 44\n}\n",
        "Level\n{\nMaxVariableValueLength 33\nMaxVariables 44\nHeapSize 55\nFontSourceLevel All\n}\n",
        "Level\n{\nHeapSize \"a\"\n}\n",
        "Level\n{\nMaxVariables \"a\"\n}\n",
        "Level\n{\nMaxVariableValueLength \"a\"\n}\n",
        "Level\n{\nSelf\n}\n",
        "Level\n{\nAll\n}\n",
        "Level\n{\nBogus\n}\n",
        "Level\n{\n7\n}\n",
        "Bogus\n}\n",
        "Level\nLevel\n{\n}\n",
        "Level\nHeapSize 3\n}\n",
    };
    for (const std::string& t : texts) {
        run_afl(t, "afl [" + t + "]", "LEVEL", nullptr, &heap_b);
        run_afl(t, "afl, own heap [" + t + "]", "LEVEL", &heap_a, &heap_b);
    }
    // Names, as the original copies them, cut to eight.
    run_afl(texts[0], "afl name long", "LONGERTHANEIGHT", nullptr, &heap_b);
    run_afl(texts[0], "afl name exact", "EIGHTLET", nullptr, &heap_b);
    run_afl(texts[0], "afl name short", "A", nullptr, &heap_b);
    // No heap at all: the raise, and nothing to hand out.
    run_afl(texts[0], "afl no heap", "LEVEL", nullptr, nullptr);
    // No room.
    {
        g_context = "afl no room";
        g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)texts[0].data();
        g_fs->files[0].size = g_fs->files[1].size = texts[0].size();
        static unsigned char o[sizeof(AtFlashLevel)], r[sizeof(AtFlashLevel)];
        std::memset(o, 0xCD, sizeof o);
        std::memset(r, 0xCD, sizeof r);
        ((AtFlashLevel*)o)->m_heap = ((AtFlashLevel*)r)->m_heap = nullptr;
        *(void**)(uintptr_t)0x002C60A8 = &heap_b;
        AtHeapStack::s_contentHeap = (AtHeap*)&heap_b;
        g_fail_after_close = true;
        { On on(0); at<FnLevel>(0x000114A0)(o, nullptr, "LEVEL", 77); }
        g_fail_alloc = false;
        { On on(1); ((AtFlashLevel*)r)->Load("LEVEL", 77); }
        g_fail_after_close = false;
        g_fail_alloc = false;
        same_level("no room", o, r);
    }
    // Files that do not open: the report, then the reader on no tokenizer.
    for (int status : {1, 2, 5}) {
        g_context = "afl no file";
        g_fs->open_status = status;
        static unsigned char o[sizeof(AtFlashLevel)], r[sizeof(AtFlashLevel)];
        std::memset(o, 0xCD, sizeof o);
        std::memset(r, 0xCD, sizeof r);
        // A name over eight: the report gives the copy, cut.
        const bool oc = guarded([&] { On on(0); at<FnLevel>(0x000114A0)(o, nullptr, "LONGERTHANEIGHT", 77); });
        g_recording = false;
        g_side = 0;
        const bool rc = guarded([&] { On on(1); ((AtFlashLevel*)r)->Load("LONGERTHANEIGHT", 77); });
        g_recording = false;
        g_side = 0;
        eq("crash", oc, rc);
        compare_logs("no file");
        AtFlashTextFormatTable* a = make_o(0xCD);
        AtFlashTextFormatTable* b = make_r(0xCD);
        const bool xc = guarded([&] { On on(0); at<FnName>(0x00020310)(a, nullptr, "THING"); });
        g_recording = false;
        g_side = 0;
        const bool yc = guarded([&] { On on(1); b->Read("THING"); });
        g_recording = false;
        g_side = 0;
        eq("crash", xc, yc);
        compare_logs("afx no file");
        g_fs->open_status = 0;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, g_context.c_str());
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
    std::vector<const char*> pos;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--entries") && i + 1 < argc) entries = argv[++i];
        else if (!std::strcmp(argv[i], "--mutate")) g_mutate = true;
        else pos.push_back(argv[i]);
    }
    if (pos.size() != 2) { std::printf("usage: oracle_afx <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new LevelFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6270u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    AtFlashResourceTable::s_defaultHeap = &g_heap_token;

    g_heap_pad = 0x1000;
    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_formats();
    run_tables();
    run_written_afx();
    run_written_afl();

    g_formats = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    int heap_b;
    while (std::fgets(line, sizeof line, index)) {
        const bool afx = std::strstr(line, ".AFX\t") != nullptr, afl = std::strstr(line, ".AFL\t") != nullptr;
        if (!afx && !afl) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::string data((size_t)std::ftell(f), '\0');
        std::fseek(f, 0, SEEK_SET);
        if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
        std::fclose(f);
        if (afx) {
            run_afx(data, line, 0xCD);
            run_afx(data, line, 0x33);
            g_afx_files++;
        } else {
            run_afl(data, line, "CODIALOG", nullptr, &heap_b);
            g_afl_files++;
        }
    }
    std::fclose(index);
    report_entries();
    std::printf("%s: %u .AFX files, %u text formats; %u .AFL files; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_afx_files, g_formats, g_afl_files, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
