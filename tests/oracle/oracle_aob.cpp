// Differential oracle for the sound bank and segment lists (Windows x86, local only). See docs/verification.md.
//
//   oracle_aob <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .AOB and .AOS listed in index.txt, each pair of a directory loaded together through the file code; text written for each word and error path; and each method alone. Each side has its own
// owner, loader, banks and segments, with the concrete classes' slots supplied by the harness on both sides: a bank made here runs the side's own Init, Load and so on.
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioBank.h"
#include "at/AtIoTokenizer.h"
#include "oracle_guard.h"
#include "oracle_image.h"

static bool g_mutate;
static int g_failures;
static unsigned long long g_checks;
static std::string g_context;

static void eq(const char* what, long long o, long long r) {
    g_checks++;
    if (o != r && g_failures++ < 25) std::printf("DIFF %s: %s: original 0x%llX, recovered 0x%llX\n", g_context.substr(0, 120).c_str(), what, o, r);
}
static void eqs(const char* what, const char* o, const char* r) {
    g_checks++;
    if (std::strcmp(o, r) != 0 && g_failures++ < 25) std::printf("DIFF %s: %s: original [%.300s], recovered [%.300s]\n", g_context.substr(0, 120).c_str(), what, o, r);
}

template <class F> static F at(uint32_t va) { return (F)(uintptr_t)va; }

#include "oracle_doubles.h"

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static AtNameTableOwner* g_own[2];

// ---- identities: pointers differ by side, so they are compared as an index in the order made ---------------------------
enum { kMaxBanks = 8192, kMaxDbs = 8, kSegPool = 1024 };
static void* g_banks[2][kMaxBanks];
static int g_bank_count[2];
static void* g_dbs[2][kMaxDbs];
static unsigned char g_segs[2][kMaxDbs][kSegPool][sizeof(AtAudioSegment)];

static long long bank_id(int side, const void* p) {
    if (!p) return -1;
    for (int i = g_bank_count[side] - 1; i >= 0; i--) {
        if (g_banks[side][i] == p) return i;
    }
    return -2;
}
static int db_slot(int side, const void* p) {
    for (int i = 0; i < kMaxDbs; i++) {
        if (g_dbs[side][i] == p) return i;
    }
    return -1;
}
static AtAudioSegment* seg_at(int side, const void* db, unsigned i) {
    return (AtAudioSegment*)g_segs[side][std::max(db_slot(side, db), 0)][std::min(i, (unsigned)kSegPool - 1)];
}

static void note_desc(int side, const AtAudioBankDesc& d) {
    Log& l = g_log[side];
    l.add('d', db_slot(side, d.m_database));
    l.add('f', d.m_format);
    l.add('r', d.m_residence);
    l.add('b', d.m_blockId);
    l.add('g', d.m_flags);
    char name[9] = {};
    std::strncpy(name, d.m_name, 8);
    for (const char c : name) l.add('n', (unsigned char)c);
}
static void note_seg(int side, AtAudioSegment* s) {
    g_log[side].add('S', (uint32_t)bank_id(side, s->m_bank));
    g_log[side].add('i', s->m_index);
    g_log[side].add('l', s->m_looping);
}

// ---- the concrete classes, on the recovered side -----------------------------------------------------------------------
class TestSegment : public AtAudioSegment {
public:
    void OnRead() override { note_seg(1, this); }
};

static float g_lengths[4];

// While set, a bank's Reset notes how many entries of this list are still there, which is the only time a list being cleared can be seen.
static AtAudioBank** g_watch[2];
static void note_watch(int side) {
    if (!g_watch[side]) return;
    int live = 0;
    for (int i = 0; i < 3; i++) live += g_watch[side][i] != nullptr;
    g_log[side].add('w', live);
}

class TestBank : public AtAudioBank {
public:
    void OnInit(const AtAudioBankDesc& d) override { g_log[1].add('I', m_state); note_desc(1, d); char n[9] = {}; std::strncpy(n, m_name, 8); g_log[1].add('m', (uint32_t)n[0]); g_log[1].add('M', m_format + (m_residence << 8) + (m_blockId << 16)); }
    void Unknown3() override { g_log[1].add('X', 3); }
    void Unknown4() override { g_log[1].add('X', 4); }
    void Unknown5() override { g_log[1].add('X', 5); }
    void Unknown6() override { g_log[1].add('X', 6); }
    void Unknown7() override { g_log[1].add('X', 7); }
    void Unknown8() override { g_log[1].add('X', 8); }
    void Unknown9() override { g_log[1].add('X', 9); }
    void Process() override { g_log[1].add('G', m_state); }
    void Reset() override { g_log[1].add('Z', m_userCount); note_watch(1); AtAudioBank::Reset(); }
};
class LengthBank : public TestBank {
public:
    float SegmentLength(int i) override { g_log[1].add('L', i); return g_lengths[i & 3]; }
    float Unknown12(int i) override { g_log[1].add('K', i); return g_lengths[(i + 1) & 3]; }
};

class TestLoader : public AtAudioBankLoader {
public:
    int side = 0;
    void Unknown0() override { g_log[side].add('x', 0); }
    void Unknown1() override { g_log[side].add('x', 1); }
    void Add(AtAudioBank* b) override { g_log[side].add('q', (uint32_t)bank_id(side, b)); g_log[side].add('s', b->m_state); }
    void Remove(AtAudioBank* b) override { g_log[side].add('Q', (uint32_t)bank_id(side, b)); g_log[side].add('s', b->m_state); }
};
static TestLoader g_loader[2];

// ---- and on the original's side: the original's own bodies, with the pure slots filled -----------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnI)(void*, void*);
typedef int(__fastcall* FnP)(void*, void*, void*);
typedef void(__fastcall* FnV1)(void*, void*, const void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef float(__fastcall* FnF)(void*, void*);
typedef float(__fastcall* FnFI)(void*, void*, int);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnReadS)(void*, void*, void*, void*);
typedef void(__fastcall* FnReadB)(void*, void*, void*, void*, const char*);
typedef void(__fastcall* FnReadSeg)(void*, void*, void*, void*, const char*, const char*);
typedef void(__fastcall* FnAdd)(void*, void*, void*, const char*);
typedef void(__fastcall* FnNames)(void*, void*, char*, char*);

static void __fastcall o_seg_read(void* self, void*) { note_seg(0, (AtAudioSegment*)self); }
static const void* g_seg_table[3] = {(const void*)(uintptr_t)0x00031C30, (const void*)o_seg_read, (const void*)(uintptr_t)0x00031BA0};

static void __fastcall o_bank_init(void* self, void*, const AtAudioBankDesc* d) {
    AtAudioBank* b = (AtAudioBank*)self;
    g_log[0].add('I', b->m_state);
    note_desc(0, *d);
    char n[9] = {};
    std::strncpy(n, b->m_name, 8);
    g_log[0].add('m', (uint32_t)n[0]);
    g_log[0].add('M', b->m_format + (b->m_residence << 8) + (b->m_blockId << 16));
}
template <int N> static void __fastcall o_bank_pure(void*, void*) { g_log[0].add('X', N); }
static void __fastcall o_bank_process(void* self, void*) { g_log[0].add('G', ((AtAudioBank*)self)->m_state); }
static void __fastcall o_bank_reset(void* self, void*) {
    g_log[0].add('Z', ((AtAudioBank*)self)->m_userCount);
    note_watch(0);
    at<Fn0>(0x00031E20)(self, nullptr);
}
static float __fastcall o_bank_length(void*, void*, int i) { g_log[0].add('L', i); return g_lengths[i & 3]; }
static float __fastcall o_bank_u12(void*, void*, int i) { g_log[0].add('K', i); return g_lengths[(i + 1) & 3]; }

#define VA(x) (const void*)(uintptr_t)(x)
static const void* g_bank_table[17] = {VA(0x00031EE0), (const void*)o_bank_init, (const void*)o_bank_reset, (const void*)o_bank_pure<3>, (const void*)o_bank_pure<4>, (const void*)o_bank_pure<5>, (const void*)o_bank_pure<6>, (const void*)o_bank_pure<7>, (const void*)o_bank_pure<8>, (const void*)o_bank_pure<9>,
                                       VA(0x00031FC0), VA(0x00031E30), VA(0x00031E30), VA(0x00031F70), (const void*)o_bank_process, VA(0x00032010), VA(0x000AB720)};
static const void* g_length_table[17] = {VA(0x00031EE0), (const void*)o_bank_init, (const void*)o_bank_reset, (const void*)o_bank_pure<3>, (const void*)o_bank_pure<4>, (const void*)o_bank_pure<5>, (const void*)o_bank_pure<6>, (const void*)o_bank_pure<7>, (const void*)o_bank_pure<8>, (const void*)o_bank_pure<9>,
                                         VA(0x00031FC0), (const void*)o_bank_length, (const void*)o_bank_u12, VA(0x00031F70), (const void*)o_bank_process, VA(0x00032010), VA(0x000AB720)};

// Banks are the harness's memory on both sides and go back through the code under test's delete.
static void* new_bank(int side, bool lengths, unsigned char fill) {
    void* p = std::malloc(sizeof(LengthBank));
    std::memset(p, fill, sizeof(LengthBank));
    const bool was = g_recording;
    g_recording = false;
    if (side == 0) {
        at<Fn0>(0x00031EC0)(p, nullptr);
        *(const void***)p = lengths ? g_length_table : g_bank_table;
    } else if (lengths) {
        new (p) LengthBank;
    } else {
        new (p) TestBank;
    }
    g_recording = was;
    if (g_bank_count[side] < kMaxBanks) g_banks[side][g_bank_count[side]++] = p;
    return p;
}

// ---- databases ----------------------------------------------------------------------------------------------------------
// A name beginning with N gets no bank, as in the concrete class when the format is not one it plays.
class TestBankDb : public AtAudioBankDatabase {
public:
    void CreateBank(unsigned int i, const AtAudioBankDesc& d) override {
        g_log[1].add('C', i);
        note_desc(1, d);
        if (d.m_name[0] == 'N') {
            m_banks[i] = nullptr;
            return;
        }
        TestBank* b = (TestBank*)new_bank(1, false, 0xCD);
        b->Init(d);
        m_banks[i] = b;
    }
    void SetDirectory(const char* dir) override {
        g_log[1].add('H', m_group != nullptr);
        record_message(dir, 1);
    }
};
static void __fastcall o_create(void* self, void*, unsigned i, const AtAudioBankDesc* d) {
    g_log[0].add('C', i);
    note_desc(0, *d);
    AtAudioBankDatabase* db = (AtAudioBankDatabase*)self;
    if (d->m_name[0] == 'N') {
        db->m_banks[i] = nullptr;
        return;
    }
    void* b = new_bank(0, false, 0xCD);
    at<FnV1>(0x00031F00)(b, nullptr, d);
    db->m_banks[i] = (AtAudioBank*)b;
}
static void __fastcall o_directory(void* self, void*, const char* dir) {
    g_log[0].add('H', ((GmNameMap*)self)->m_group != nullptr);
    record_message(dir, 0);
}
static const void* g_bank_db_table[4] = {VA(0x00025840), VA(0x00025790), (const void*)o_create, (const void*)o_directory};

static void make_segs(int side, const void* db, unsigned count) {
    const bool was = g_recording;
    g_recording = false;
    for (unsigned i = 0; i < std::min(count, (unsigned)kSegPool); i++) {
        void* p = seg_at(side, db, i);
        std::memset(p, 0xCD, sizeof(AtAudioSegment));
        if (side == 0) {
            at<Fn0>(0x00031C10)(p, nullptr);
            *(const void***)p = g_seg_table;
        } else {
            new (p) TestSegment;
        }
    }
    g_recording = was;
}

class TestSegDb : public AtAudioSegmentDatabase {
public:
    void OnReady() override {
        g_log[1].add('Y', m_owner == g_own[1]);
        g_log[1].add('y', m_group != nullptr);
        g_log[1].add('c', m_count);
        make_segs(1, this, m_count);
    }
    void Unknown3(void*) override { g_log[1].add('X', 33); }
    AtAudioSegment* SegmentAt(unsigned int i) override {
        g_log[1].add('A', i);
        return seg_at(1, this, i);
    }
};
static void __fastcall o_ready(void* self, void*) {
    AtAudioSegmentDatabase* db = (AtAudioSegmentDatabase*)self;
    g_log[0].add('Y', db->m_owner == g_own[0]);
    g_log[0].add('y', db->m_group != nullptr);
    g_log[0].add('c', db->m_count);
    make_segs(0, self, db->m_count);
}
static void __fastcall o_seg_u3(void*, void*, void*) { g_log[0].add('X', 33); }
static AtAudioSegment* __fastcall o_seg_at(void* self, void*, unsigned i) {
    g_log[0].add('A', i);
    return seg_at(0, self, i);
}
static const void* g_seg_db_table[5] = {VA(0x00025530), VA(0x00025310), (const void*)o_ready, (const void*)o_seg_u3, (const void*)o_seg_at};

static AtAudioBankDatabase* make_o_bank_db(int slot) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtAudioBankDatabase));
    std::memset(o, 0xCD, sizeof(AtAudioBankDatabase));
    g_dbs[0][slot] = o;
    at<Fn0>(0x00025820)(o, nullptr);
    *(const void***)o = g_bank_db_table;
    return (AtAudioBankDatabase*)o;
}
static TestBankDb* make_r_bank_db(int slot) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(TestBankDb));
    std::memset(r, 0xCD, sizeof(TestBankDb));
    g_dbs[1][slot] = r;
    return new (r) TestBankDb;
}
static AtAudioSegmentDatabase* make_o_seg_db(int slot) {
    unsigned char* o = (unsigned char*)std::malloc(sizeof(AtAudioSegmentDatabase));
    std::memset(o, 0xCD, sizeof(AtAudioSegmentDatabase));
    g_dbs[0][slot] = o;
    at<Fn0>(0x00025510)(o, nullptr);
    *(const void***)o = g_seg_db_table;
    return (AtAudioSegmentDatabase*)o;
}
static TestSegDb* make_r_seg_db(int slot) {
    unsigned char* r = (unsigned char*)std::malloc(sizeof(TestSegDb));
    std::memset(r, 0xCD, sizeof(TestSegDb));
    g_dbs[1][slot] = r;
    return new (r) TestSegDb;
}

// ---- comparing ----------------------------------------------------------------------------------------------------------
static std::string group_name(const GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

// The bytes after a terminator in a name are stack garbage on each side. Fields are compared one by one for that reason.
static void same_bank(const AtAudioBank* o, const AtAudioBank* r) {
    eq("bank's name", strncmp(o->m_name, r->m_name, 8), 0);
    eq("bank's format", o->m_format, r->m_format);
    eq("bank's residence", o->m_residence, r->m_residence);
    eq("bank's block", o->m_blockId, r->m_blockId);
    eq("bank's state", o->m_state, r->m_state);
    eq("bank's unknown 1C", o->m_unknown1C, r->m_unknown1C);
    for (int i = 0; i < AtAudioBank::kMaxUsers; i++) eq("bank's user", db_slot(0, o->m_users[i]) * (o->m_users[i] != nullptr), db_slot(1, r->m_users[i]) * (r->m_users[i] != nullptr));
    for (int i = 0; i < AtAudioBank::kMaxUsers; i++) eq("bank's user set", o->m_users[i] != nullptr, r->m_users[i] != nullptr);
    eq("bank's file", o->m_file.m_file != nullptr, r->m_file.m_file != nullptr);
    eq("bank's unknown 78", o->m_unknown78, r->m_unknown78);
    eq("bank's users", o->m_userCount, r->m_userCount);
    eq("bank's unknown 80", o->m_unknown80, r->m_unknown80);
    eq("bank's unknown 84", o->m_unknown84, r->m_unknown84);
    eq("bank's unknown 88", o->m_unknown88, r->m_unknown88);
}

static void same_map(const GmNameMap* o, const GmNameMap* r, bool segments) {
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) {
                eq("slot's name", strncmp(x.m_name, y.m_name, 8), 0);
                if (segments) eq("slot's segment", (char*)x.m_value - (char*)g_segs[0], (char*)y.m_value - (char*)g_segs[1]);
                else eq("slot's bank", bank_id(0, x.m_value), bank_id(1, y.m_value));
            }
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
}

static void same_owner() {
    AtNameTableOwner* a = g_own[0];
    AtNameTableOwner* b = g_own[1];
    for (int i = 0; i < kMaxDbs; i++) {
        if (!g_dbs[0][i] || !g_dbs[1][i]) continue;
        const GmNameMap* x = (const GmNameMap*)g_dbs[0][i];
        const GmNameMap* y = (const GmNameMap*)g_dbs[1][i];
        eq("in the first plain group", x->m_group == &a->m_first, y->m_group == &b->m_first);
        eq("in the second plain group", x->m_group == &a->m_second, y->m_group == &b->m_second);
        eq("first's head", a->m_first.m_first == x, b->m_first.m_first == y);
        eq("second's head", a->m_second.m_first == x, b->m_second.m_first == y);
    }
    eqs("last first space", group_name(a->m_firstSpaces.m_last).c_str(), group_name(b->m_firstSpaces.m_last).c_str());
    eqs("last second space", group_name(a->m_secondSpaces.m_last).c_str(), group_name(b->m_secondSpaces.m_last).c_str());
    eq("first spaces used", a->m_firstSpaces.m_free ? a->m_firstSpaces.m_free - a->m_firstSpaces.m_pool : -1, b->m_firstSpaces.m_free ? b->m_firstSpaces.m_free - b->m_firstSpaces.m_pool : -1);
    eq("second spaces used", a->m_secondSpaces.m_free ? a->m_secondSpaces.m_free - a->m_secondSpaces.m_pool : -1, b->m_secondSpaces.m_free ? b->m_secondSpaces.m_free - b->m_secondSpaces.m_pool : -1);
}

static void same_bank_db(const char* what, const AtAudioBankDatabase* o, const AtAudioBankDatabase* r) {
    g_recording = false;
    eq("db's owner", o->m_owner == g_own[0] ? 1 : o->m_owner ? 2 : 0, r->m_owner == g_own[1] ? 1 : r->m_owner ? 2 : 0);
    eq("db's count", o->m_count, r->m_count);
    eq("db's list", o->m_banks != nullptr, r->m_banks != nullptr);
    if (o->m_banks && r->m_banks && o->m_count == r->m_count) {
        for (unsigned i = 0; i < o->m_count; i++) {
            eq("listed bank", bank_id(0, o->m_banks[i]), bank_id(1, r->m_banks[i]));
            if (o->m_banks[i] && r->m_banks[i]) same_bank(o->m_banks[i], r->m_banks[i]);
        }
    }
    same_map(o, r, false);
    same_owner();
    compare_logs(what);
}

static void same_seg_db(const char* what, const AtAudioSegmentDatabase* o, const AtAudioSegmentDatabase* r, int slot) {
    g_recording = false;
    eq("db's owner", o->m_owner == g_own[0] ? 1 : o->m_owner ? 2 : 0, r->m_owner == g_own[1] ? 1 : r->m_owner ? 2 : 0);
    eq("db's count", o->m_count, r->m_count);
    if (o->m_count == r->m_count) {
        for (unsigned i = 0; i < std::min(o->m_count, (unsigned)kSegPool); i++) {
            const AtAudioSegment* x = (const AtAudioSegment*)g_segs[0][slot][i];
            const AtAudioSegment* y = (const AtAudioSegment*)g_segs[1][slot][i];
            eq("segment's bank", bank_id(0, x->m_bank), bank_id(1, y->m_bank));
            eq("segment's index", x->m_index, y->m_index);
            eq("segment's loop", x->m_looping, y->m_looping);
        }
    }
    same_map(o, r, true);
    same_owner();
    compare_logs(what);
}

// ---- the owner ----------------------------------------------------------------------------------------------------------
struct Env {
    Env() {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            g_own[side] = new AtNameTableOwner;
            AtNameTableOwner& w = *g_own[side];
            g_loader[side].side = side;
            w.m_loader = &g_loader[side];
            w.m_firstSpaces.Init(8);
            w.m_secondSpaces.Init(8);
            g_bank_count[side] = 0;
            std::memset(g_banks[side], 0, sizeof g_banks[side]);
            std::memset(g_dbs[side], 0, sizeof g_dbs[side]);
        }
        g_fill = 0xCD;
        g_recording = was;
    }
    ~Env() {
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            delete g_own[side];
            g_own[side] = nullptr;
        }
    }
};

static void set_text(const std::string& text, const char* path) {
    g_fs->files[0].data = g_fs->files[1].data = (const unsigned char*)text.data();
    g_fs->files[0].size = g_fs->files[1].size = text.size();
    for (int side = 0; side < 2; side++) {
        strcpy_s(g_fs->files[side].m_path, path);
    }
}

// ---- through the file code ----------------------------------------------------------------------------------------------
static unsigned g_files_aob, g_files_aos, g_banks_seen, g_segments_seen;

typedef void (*Thunk)(void*);
static void load_bank(AtAudioBankDatabase* o, TestBankDb* r, const char* what) {
    // A second read when the first read a count of zero: Init clears through the slot what was just set, and the owner is then null, on both sides.
    g_fs->m_path.m_top = -1;
    const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00025A90)(o, nullptr, g_own[0], "AUDIO\\THING", 77); });
    g_recording = false;
    const int otop = g_fs->m_path.m_top;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], "AUDIO\\THING", 77); });
    g_recording = false;
    eq("directory stack afterwards", otop, g_fs->m_path.m_top);
    g_fs->m_path.m_top = -1;
    eq("crash on reading again", oc, rc);
    same_bank_db(what, o, r);
}
static void load_seg(AtAudioSegmentDatabase* o, TestSegDb* r, const char* what, int slot) {
    const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00025640)(o, nullptr, g_own[0], "AUDIO\\THING", 77); });
    g_recording = false;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], "AUDIO\\THING", 77); });
    g_recording = false;
    eq("crash on reading again", oc, rc);
    same_seg_db(what, o, r, slot);
}
static void clear_bank_db(AtAudioBankDatabase* o, TestBankDb* r, const char* what) {
    { On on(0); at<Fn0>(0x00025790)(o, nullptr); }
    { On on(1); r->Clear(); }
    same_bank_db(what, o, r);
}
static void destroy_bank_db(AtAudioBankDatabase* o, TestBankDb* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00025840)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~TestBankDb(); }
    compare_logs(what);
    std::free(o);
    std::free(r);
}
static void destroy_seg_db(AtAudioSegmentDatabase* o, TestSegDb* r, const char* what) {
    { On on(0); eq("returns this", (uintptr_t)at<FnDel>(0x00025530)(o, nullptr, 0), (uintptr_t)o); }
    { On on(1); r->~TestSegDb(); }
    compare_logs(what);
    std::free(o);
    std::free(r);
}

// The bank file, then a second list over the same banks, then the segment file, and each let go of in turn. Either text may be absent.
static void run_pair(const std::string* aob, const std::string* aos, const std::string& context) {
    g_context = context;
    Env env;
    AtAudioBankDatabase* o = make_o_bank_db(0);
    TestBankDb* r = make_r_bank_db(0);
    AtAudioBankDatabase* o2 = make_o_bank_db(1);
    TestBankDb* r2 = make_r_bank_db(1);
    AtAudioSegmentDatabase* so = make_o_seg_db(2);
    TestSegDb* sr = make_r_seg_db(2);
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtAudioBankDatabase) - 4), 0);
    eq("constructed", std::memcmp((char*)so + 4, (char*)sr + 4, sizeof(AtAudioSegmentDatabase) - 4), 0);
    if (aob) {
        set_text(*aob, "AUDIO\\DIR\\THING.AOB");
        load_bank(o, r, "bank file");
        g_banks_seen += r->m_count;
        load_bank(o2, r2, "bank file, again in a second list");
    }
    if (aos) {
        set_text(*aos, "AUDIO\\DIR\\THING.AOS");
        load_seg(so, sr, "segment file", 2);
        g_segments_seen += sr->m_count;
        load_seg(so, sr, "segment file over itself", 2);
        // What each segment says.
        for (unsigned i = 0; i < std::min(sr->m_count, (unsigned)kSegPool); i++) {
            void* a = g_segs[0][2][i];
            AtAudioSegment* b = (AtAudioSegment*)g_segs[1][2][i];
            eq("length", *(int32_t*)&(const float&)at<FnF>(0x00031BB0)(a, nullptr), *(int32_t*)&(const float&)b->Length());
            eq("slot 12", *(int32_t*)&(const float&)at<FnF>(0x00031BF0)(a, nullptr), *(int32_t*)&(const float&)b->Unknown31BF0());
        }
        { On on(0); at<Fn0>(0x00025310)(so, nullptr); at<Fn0>(0x00025310)(so, nullptr); }
        { On on(1); sr->Clear(); sr->Clear(); }
        eq("cleared owner", so->m_owner != nullptr, sr->m_owner != nullptr);
        eq("cleared count", so->m_count, sr->m_count);
        eq("cleared entries", so->m_entries != nullptr, sr->m_entries != nullptr);
        compare_logs("segments cleared");
    }
    if (aob) {
        // The first list reads again, over itself: it lets go of what the second also holds, and finds it again.
        set_text(*aob, "AUDIO\\DIR\\THING.AOB");
        load_bank(o, r, "bank file over itself");
        clear_bank_db(o, r, "first list cleared");
        clear_bank_db(o2, r2, "second list cleared, the banks destroyed");
        clear_bank_db(o2, r2, "cleared with nothing there");
    }
    destroy_seg_db(so, sr, "segments destroyed");
    destroy_bank_db(o, r, "first destroyed");
    destroy_bank_db(o2, r2, "second destroyed");
}

// Run through the real file code, where an error comes back from the report. Any text left open inside an entry would go round for ever on both sides; those are in kOpenTexts, on a tokenizer that stops it.
static const char* const kBankTexts[] = {
    "",
    "// nothing but a comment\n",
    "Bank[0]\n{\n}\n",
    "Bank[1]\n{\n Bank \"A\"\n {\n }\n}\n",
    "Bank[2]\n{\n Bank \"A\"\n {\n DiskResident\n }\n Bank \"B\"\n {\n SoundRamResident\n }\n}\n",
    "Bank[1]\n{\n Bank \"C\"\n {\n PrimaryRamResident\n }\n}\n",
    "Bank[1]\n{\n Bank \"C\"\n {\n ComboResident\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n MiaFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n MiaFormat\n MsaFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n RwfFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n WbaFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n SbaFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n GmsFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n GmdFormat\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n PlatformSource\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n PlatformSource Streamed\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n SbaFormat StreamedStereo\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n SbaFormat StreamedMemory\n }\n}\n",
    "Bank[1]\n{\n Bank \"F\"\n {\n SbaFormat Streamed\n }\n}\n",
    "Bank[1]\n{\n Bank \"G\"\n {\n BlockID 12\n }\n}\n",
    "Bank[1]\n{\n Bank \"G\"\n {\n BlockID 1.5\n }\n}\n",
    "Bank[1]\n{\n Bank \"G\"\n {\n BlockID \"x\"\n }\n}\n",
    "Bank[1]\n{\n Bank \"H\"\n {\n DelayLoad\n }\n}\n",
    "Bank[1]\n{\n Bank \"H\"\n {\n Streaming\n }\n}\n",
    "Bank[1]\n{\n Bank \"H\"\n {\n Streaming DelayLoad\n }\n}\n",
    "Bank[1]\n{\n Bank \"I\"\n {\n DiskResident MsaFormat BlockID 3 DelayLoad Streaming\n }\n}\n",
    "Bank[1]\n{\n Bank \"NoBank\"\n {\n }\n}\n",
    "Bank[3]\n{\n Bank \"A\"\n {\n }\n Bank \"NoBank\"\n {\n }\n Bank \"C\"\n {\n }\n}\n",
    "Bank[2]\n{\n Bank \"Dup\"\n {\n DiskResident\n }\n Bank \"Dup\"\n {\n SoundRamResident\n }\n}\n",
    "Bank[1]\n{\n Bank \"LongerThanEight\"\n {\n }\n}\n",
    "Bank[1]\n{\n Bank \"\"\n {\n }\n}\n",
    "Bank[1]\n{\n Other \"A\"\n {\n }\n}\n",
    "Bank[1]\n{\n Bank \"A\"\n {\n 5\n }\n}\n",
    "Bank[1]\n{\n Bank \"A\"\n {\n \"str\"\n Bank\n NameSpace\n }\n}\n",
    "Bank[1]\n{\n Bank \"A\"\n {\n }\n",
    "Bank[1]\n{\n Bank \"A\"\n {\n }\n}\nBank[1]\n{\n Bank \"Ignored\"\n {\n }\n}\n",
    "Bank[1]\n{\n Bank \"A\"\n {\n }\n}\n5\n",
    "Bank\n{\n}\n",
    "Bank[0\n{\n}\n",
    "NameSpace \"Space\"\nBank[1]\n{\n Bank \"A\"\n {\n }\n}\n",
    "NameSpace \"Space\"\nBank[2]\n{\n Bank \"Dup\"\n {\n }\n Bank \"Dup\"\n {\n }\n}\n",
    "NameSpace \"\"\nBank[1]\n{\n Bank \"A\"\n {\n }\n}\n",
    "NameSpace \"LongerThanEight\"\nBank[1]\n{\n Bank \"A\"\n {\n }\n}\n",
    "NameSpace \"One\"\nNameSpace \"Two\"\nBank[1]\n{\n Bank \"A\"\n {\n }\n}\n",
    "NameSpace \"Space\"\n",
    "NameSpace 5\nBank[0]\n{\n}\n",
    "5\nBank[1]\n{\n Bank \"A\"\n {\n }\n}\n",
    "DiskResident\n",
    "\"Name\"\n",
    "{\n}\n",
    "Segment[1]\n",
};
static const char* const kSegTexts[] = {
    "",
    "// nothing but a comment\n",
    "Segment[0]\n{\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" \"\" 2\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"XGone\" \"\" 2\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" \"Space\" 2\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" \"Other\" 2\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" \"\" 2\n Bank \"C\" \"\" 3\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" \"\" 2\n Bank \"XGone\" \"\" 3\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Looping\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Looping Looping Bank \"A\" \"\" -1\n }\n}\n",
    "Segment[2]\n{\n Segment \"S\"\n {\n Bank \"A\" \"\" 0\n Looping\n }\n Segment \"T\"\n {\n Bank \"A\" \"\" 1\n }\n}\n",
    "Segment[2]\n{\n Segment \"Dup\"\n {\n }\n Segment \"Dup\"\n {\n }\n}\n",
    "Segment[1]\n{\n Segment \"LongerThanEight\"\n {\n }\n}\n",
    "Segment[1]\n{\n Other \"S\"\n {\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n 5\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n \"str\" Segment NameSpace\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank 5 \"\" 2\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" 5 2\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n Bank \"A\" \"\" 2.5\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n }\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n }\n}\nSegment[1]\n{\n Segment \"Ignored\"\n {\n }\n}\n",
    "Segment[1]\n{\n Segment \"S\"\n {\n }\n}\n5\n",
    "Segment\n{\n}\n",
    "Segment[0\n{\n}\n",
    "NameSpace \"Space\"\nSegment[1]\n{\n Segment \"S\"\n {\n }\n}\n",
    "NameSpace \"\"\nSegment[1]\n{\n Segment \"S\"\n {\n }\n}\n",
    "NameSpace \"LongerThanEight\"\nSegment[1]\n{\n Segment \"S\"\n {\n }\n}\n",
    "NameSpace \"One\"\nNameSpace \"Two\"\nSegment[1]\n{\n Segment \"S\"\n {\n }\n}\n",
    "NameSpace \"Space\"\n",
    "5\nSegment[1]\n{\n Segment \"S\"\n {\n }\n}\n",
    "Bank\n",
    "Looping\n",
    "\"Name\"\n",
    "{\n}\n",
    "Bank[1]\n",
};

// Banks these segments can name: A and C with no name space, and A in one.
static const char kBanksForSegments[] = "Bank[3]\n{\n Bank \"A\"\n {\n }\n Bank \"C\"\n {\n }\n Bank \"D\"\n {\n }\n}\n";
static const char kSpacedBanks[] = "NameSpace \"Space\"\nBank[1]\n{\n Bank \"A\"\n {\n }\n}\n";

static void run_written() {
    for (const char* t : kBankTexts) {
        const std::string text = t;
        run_pair(&text, nullptr, "bank text [" + text + "]");
    }
    const std::string banks = kBanksForSegments, spaced = kSpacedBanks;
    for (const char* t : kSegTexts) {
        const std::string text = t;
        run_pair(&banks, &text, "segment text [" + text + "]");
        run_pair(nullptr, &text, "segment text, no banks [" + text + "]");
        // Banks in a name space and in none: which is found.
        g_context = "segment text, spaced banks [" + text + "]";
        Env env;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        AtAudioBankDatabase* o2 = make_o_bank_db(1);
        TestBankDb* r2 = make_r_bank_db(1);
        AtAudioSegmentDatabase* so = make_o_seg_db(2);
        TestSegDb* sr = make_r_seg_db(2);
        set_text(banks, "AUDIO\\THING.AOB");
        load_bank(o, r, "plain banks");
        set_text(spaced, "AUDIO\\THING.AOB");
        load_bank(o2, r2, "spaced banks");
        set_text(text, "AUDIO\\THING.AOS");
        load_seg(so, sr, "segments", 2);
        destroy_seg_db(so, sr, "segments destroyed");
        clear_bank_db(o, r, "plain cleared");
        clear_bank_db(o2, r2, "spaced cleared");
        destroy_bank_db(o, r, "plain destroyed");
        destroy_bank_db(o2, r2, "spaced destroyed");
    }
    // Where the file is: a root, no directory at all, and a path with forward slashes.
    for (const char* root : {"", "D:\\DATA"}) {
        for (const char* path : {"THING.AOB", "AUDIO/SUB/THING.AOB", "AUDIO\\THING.AOB", "\\THING.AOB"}) {
            g_context = std::string("directory ") + root + " " + path;
            Env env;
            g_fs->m_path.SetRoot(root);
            AtAudioBankDatabase* o = make_o_bank_db(0);
            TestBankDb* r = make_r_bank_db(0);
            const std::string text = kBanksForSegments;
            set_text(text, path);
            load_bank(o, r, "bank file");
            eq("stack restored", g_fs->m_path.m_top, -1);
            clear_bank_db(o, r, "cleared");
            destroy_bank_db(o, r, "destroyed");
            g_fs->m_path.SetRoot("");
        }
    }
}

// ---- on a tokenizer of the harness's ------------------------------------------------------------------------------------
class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};

// Error returns, as the file's does here. Text left open inside an entry would go round for ever, on both sides, so this one stops it with a crash that guarded() catches.
class DirectTokenizer : public AtIoTextTokenizer {
public:
    int errors = 0;
    void Error(int code) override {
        g_log[g_side].add('E', code);
        if (++errors > 40) *(volatile int*)nullptr = 1;
    }
};
struct Direct {
    MemoryStream stream;
    DirectTokenizer tokenizer;
    std::string text;
    Direct(const char* t, const AtIoKeyword* words, unsigned count) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
        tokenizer.SetKeywords(words, count, nullptr);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

// A deliberately stopped run leaves things half done, so what is left is not destroyed: one fresh environment each.
static void run_open_texts() {
    static const char* const banks[] = {
        "Bank[1]\n{\n Bank \"A\"\n {\n",
        "Bank[1]\n{\n Bank \"A\"\n",
        "Bank[2]\n{\n Bank \"A\"\n {\n }\n",
        "Bank[1]\n{\n",
        "Bank[1]\n",
    };
    static const char* const segs[] = {
        "Segment[1]\n{\n Segment \"S\"\n {\n",
        "Segment[1]\n{\n Segment \"S\"\n",
        "Segment[2]\n{\n Segment \"S\"\n {\n }\n",
        "Segment[1]\n{\n",
        "Segment[1]\n",
    };
    char dir[] = "SOMEWHERE";
    for (const char* text : banks) {
        g_context = std::string("open bank text [") + text + "]";
        Env env;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        Direct da(text, AtAudioBankDatabase::s_keywords, AtAudioBankDatabase::kKeywordCount), db(text, AtAudioBankDatabase::s_keywords, AtAudioBankDatabase::kKeywordCount);
        const bool oc = guarded([&] { On on(0); at<FnReadB>(0x00025860)(o, nullptr, g_own[0], &da.tokenizer, dir); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer, dir); });
        g_recording = false;
        eq("stopped", oc, rc);
        compare_logs("open text");
    }
    for (const char* text : segs) {
        g_context = std::string("open segment text [") + text + "]";
        Env env;
        AtAudioSegmentDatabase* o = make_o_seg_db(2);
        TestSegDb* r = make_r_seg_db(2);
        Direct da(text, AtAudioSegmentDatabase::s_keywords, AtAudioSegmentDatabase::kKeywordCount), db(text, AtAudioSegmentDatabase::s_keywords, AtAudioSegmentDatabase::kKeywordCount);
        const bool oc = guarded([&] { On on(0); at<FnReadS>(0x00025550)(o, nullptr, g_own[0], &da.tokenizer); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer); });
        g_recording = false;
        eq("stopped", oc, rc);
        compare_logs("open text");
    }
}

static void same_bytes(const char* what, const void* a, const void* b, size_t n) {
    eq(what, std::memcmp(a, b, n), 0);
}

static void run_keywords() {
    g_context = "keywords";
    const AtIoKeyword* o = (const AtIoKeyword*)(uintptr_t)0x002B0518;
    for (unsigned i = 0; i < AtAudioBankDatabase::kKeywordCount; i++) {
        eq("bank word id", o[i].m_id, AtAudioBankDatabase::s_keywords[i].m_id);
        eqs("bank word", o[i].m_name, AtAudioBankDatabase::s_keywords[i].m_name);
    }
    o = (const AtIoKeyword*)(uintptr_t)0x002B04E4;
    for (unsigned i = 0; i < AtAudioSegmentDatabase::kKeywordCount; i++) {
        eq("segment word id", o[i].m_id, AtAudioSegmentDatabase::s_keywords[i].m_id);
        eqs("segment word", o[i].m_name, AtAudioSegmentDatabase::s_keywords[i].m_name);
    }
}

static void run_setters() {
    g_context = "heaps";
    for (void* heap : {(void*)&g_heap_token, (void*)nullptr, (void*)&g_heap_token}) {
        at<void(__cdecl*)(void*)>(0x000254F0)(heap);
        AtAudioSegmentDatabase::SetHeap(heap);
        eq("segment db heap", (uintptr_t)*(void**)(uintptr_t)0x002C6334, (uintptr_t)AtAudioSegmentDatabase::s_heap);
        at<void(__cdecl*)(void*)>(0x000256E0)(heap);
        AtAudioBankDatabase::SetHeap(heap);
        eq("bank db heap", (uintptr_t)*(void**)(uintptr_t)0x002C6338, (uintptr_t)AtAudioBankDatabase::s_heap);
        at<void(__cdecl*)(void*)>(0x00031D90)(heap);
        AtAudioBank::SetHeap(heap);
        eq("bank heap", (uintptr_t)*(void**)(uintptr_t)0x002C63D4, (uintptr_t)AtAudioBank::s_heap);
    }
}

static void run_small_objects() {
    g_context = "desc";
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x00, (unsigned char)0xFF}) {
        unsigned char a[sizeof(AtAudioBankDesc)], b[sizeof(AtAudioBankDesc)];
        std::memset(a, fill, sizeof a);
        std::memset(b, fill, sizeof b);
        eq("returns this", (uintptr_t)at<void*(__fastcall*)(void*, void*)>(0x00031D70)(a, nullptr), (uintptr_t)a);
        new (b) AtAudioBankDesc;
        same_bytes("desc", a, b, sizeof a);
    }

    g_context = "segment";
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x00}) {
        unsigned char a[sizeof(TestSegment)], b[sizeof(TestSegment)];
        std::memset(a, fill, sizeof a);
        std::memset(b, fill, sizeof b);
        eq("returns this", (uintptr_t)at<void*(__fastcall*)(void*, void*)>(0x00031C10)(a, nullptr), (uintptr_t)a);
        TestSegment* r = new (b) TestSegment;
        eq("segment's vftable", *(uint32_t*)a, 0x00274A84);
        same_bytes("constructed segment", a + 4, b + 4, sizeof a - 4);
        // Reset on dirty fields.
        std::memset(a + 4, 0x5A, sizeof a - 4);
        std::memset(b + 4, 0x5A, sizeof b - 4);
        at<Fn0>(0x00031BA0)(a, nullptr);
        r->AtAudioSegment::Reset();
        same_bytes("reset segment", a + 4, b + 4, sizeof a - 4);
        // The complete destructor, and the deleting one with the delete off, leave the fields.
        std::memset(a + 4, 0x5A, sizeof a - 4);
        std::memset(b + 4, 0x5A, sizeof b - 4);
        at<Fn0>(0x00031B90)(a, nullptr);
        eq("after destructor", *(uint32_t*)a, 0x00274A84);
        same_bytes("destroyed segment", a + 4, b + 4, sizeof a - 4);
        *(void**)a = nullptr;
        eq("deleting destructor returns this", (uintptr_t)at<FnDel>(0x00031C30)(a, nullptr, 0), (uintptr_t)a);
        eq("deleting destructor's vftable", *(uint32_t*)a, 0x00274A84);
        same_bytes("destroyed segment", a + 4, b + 4, sizeof a - 4);
    }
    // The delete.
    {
        void* a = std::malloc(sizeof(TestSegment));
        { On on(0); at<Fn0>(0x00031C10)(a, nullptr); *(const void***)a = g_seg_table; at<FnDel>(0x00031C30)(a, nullptr, 1); }
        void* b = std::malloc(sizeof(TestSegment));
        { On on(1); TestSegment* r = new (b) TestSegment; delete r; }
        compare_logs("segment deleted");
    }
}

// Segment length with each kind of bank, value and loop.
static void run_lengths() {
    g_context = "lengths";
    Env env;
    static const float values[][4] = {{0.0f, 1.5f, -2.0f, 3.0f}, {-0.0f, 0.25f, 0.0f, -0.0f}};
    unsigned nan_bits = 0x7FC00001, inf_bits = 0x7F800000, neg_bits = 0xBF800000;
    for (int v = 0; v < 4; v++) {
        for (int i = 0; i < 4; i++) {
            g_lengths[i] = v < 2 ? values[v][i] : v == 2 ? *(float*)&nan_bits : (i & 1) ? *(float*)&inf_bits : *(float*)&neg_bits;
        }
        void* ob = new_bank(0, true, 0xCD);
        void* rb = new_bank(1, true, 0xCD);
        void* obase = new_bank(0, false, 0xCD);
        void* rbase = new_bank(1, false, 0xCD);
        for (int kind = 0; kind < 3; kind++) {
            for (const int index : {0, 1, 2, 3, 7, -1}) {
                for (const int looping : {0, 1, 2, -1}) {
                    unsigned char a[sizeof(TestSegment)], b[sizeof(TestSegment)];
                    at<Fn0>(0x00031C10)(a, nullptr);
                    TestSegment* r = new (b) TestSegment;
                    AtAudioSegment* x = (AtAudioSegment*)a;
                    x->m_bank = kind == 0 ? nullptr : kind == 1 ? (AtAudioBank*)ob : (AtAudioBank*)obase;
                    r->m_bank = kind == 0 ? nullptr : kind == 1 ? (AtAudioBank*)rb : (AtAudioBank*)rbase;
                    x->m_index = r->m_index = index;
                    x->m_looping = r->m_looping = looping;
                    float f1, f2, g1, g2;
                    { On on(0); f1 = at<FnF>(0x00031BB0)(a, nullptr); g1 = at<FnF>(0x00031BF0)(a, nullptr); }
                    { On on(1); f2 = r->Length(); g2 = r->Unknown31BF0(); }
                    eq("length", *(int32_t*)&f1, *(int32_t*)&f2);
                    eq("slot 12", *(int32_t*)&g1, *(int32_t*)&g2);
                    compare_logs("length");
                }
            }
        }
    }
    // The base's own answers, called directly.
    void* ob = new_bank(0, false, 0xCD);
    void* rb = new_bank(1, false, 0xCD);
    const float a = at<FnFI>(0x00031E30)(ob, nullptr, 5);
    const float b = ((AtAudioBank*)rb)->AtAudioBank::SegmentLength(5);
    const float c = ((AtAudioBank*)rb)->AtAudioBank::Unknown12(5);
    eq("slot 11", *(int32_t*)&a, *(int32_t*)&b);
    eq("slot 12", *(int32_t*)&a, *(int32_t*)&c);
    eq("slot 16", at<FnI>(0x000AB720)(ob, nullptr), ((AtAudioBank*)rb)->AtAudioBank::Unknown16());
}

// A user: a list with the right owner and a chosen group.
struct FakeUser {
    AtAudioBankDatabase* db[2];
    FakeUser(GmNameMapGroup* a, GmNameMapGroup* b) {
        for (int side = 0; side < 2; side++) {
            db[side] = (AtAudioBankDatabase*)std::calloc(1, sizeof(AtAudioBankDatabase));
            db[side]->m_owner = g_own[side];
            db[side]->m_group = side == 0 ? a : b;
        }
    }
    ~FakeUser() { std::free(db[0]); std::free(db[1]); }
};

static void same_bank_bytes(const char* what, const void* o, const void* r) {
    g_recording = false;
    eq(what, std::memcmp((const char*)o + 4, (const char*)r + 4, offsetof(AtAudioBank, m_users) - 4), 0);
    eq(what, std::memcmp((const char*)o + offsetof(AtAudioBank, m_file), (const char*)r + offsetof(AtAudioBank, m_file), sizeof(AtAudioBank) - offsetof(AtAudioBank, m_file)), 0);
    compare_logs(what);
}

static void run_bank_alone() {
    g_context = "bank";
    Env env;
    // Constructing: a field it leaves is compared by the fill.
    for (const unsigned char fill : {(unsigned char)0xCD, (unsigned char)0x00, (unsigned char)0x77}) {
        void* o = new_bank(0, false, fill);
        void* r = new_bank(1, false, fill);
        eq("vftable", *(uint32_t*)o != 0, 1);
        same_bytes("constructed bank", (char*)o + 4, (char*)r + 4, sizeof(AtAudioBank) - 4);
        // And the original's own table, before the harness's.
        unsigned char plain[sizeof(AtAudioBank)];
        at<Fn0>(0x00031EC0)(plain, nullptr);
        eq("original's vftable", *(uint32_t*)plain, 0x00274A90);
        // Reset and the bare fields' reset, on dirty fields.
        std::memset((char*)o + 4, 0x5A, sizeof(AtAudioBank) - 4);
        std::memset((char*)r + 4, 0x5A, sizeof(AtAudioBank) - 4);
        at<Fn0>(0x00031E20)(o, nullptr);
        ((AtAudioBank*)r)->AtAudioBank::Reset();
        same_bytes("reset bank", (char*)o + 4, (char*)r + 4, sizeof(AtAudioBank) - 4);
        std::memset((char*)o + 4, 0x5A, sizeof(AtAudioBank) - 4);
        std::memset((char*)r + 4, 0x5A, sizeof(AtAudioBank) - 4);
        at<Fn0>(0x00031DB0)(o, nullptr);
        ((AtAudioBank*)r)->ResetFields();
        same_bytes("reset fields", (char*)o + 4, (char*)r + 4, sizeof(AtAudioBank) - 4);
    }

    // Users: filling up, past full, and out of order. Pointers are the same small numbers on both sides.
    {
        AtAudioBank* o = (AtAudioBank*)new_bank(0, false, 0xCD);
        AtAudioBank* r = (AtAudioBank*)new_bank(1, false, 0xCD);
        for (uintptr_t i = 1; i <= 24; i++) {
            eq("add", at<FnP>(0x00031E40)(o, nullptr, (void*)i), r->AddUser((AtAudioBankDatabase*)i));
            eq("count", at<FnI>(0x00031EB0)(o, nullptr), r->UserCount());
            same_bytes("users", o->m_users, r->m_users, sizeof o->m_users);
            eq("count field", o->m_userCount, r->m_userCount);
        }
        for (const uintptr_t i : {(uintptr_t)7, (uintptr_t)1, (uintptr_t)20, (uintptr_t)21, (uintptr_t)7, (uintptr_t)99}) {
            eq("remove", at<FnP>(0x00031E70)(o, nullptr, (void*)i), r->RemoveUser((AtAudioBankDatabase*)i));
            same_bytes("users", o->m_users, r->m_users, sizeof o->m_users);
            eq("count field", o->m_userCount, r->m_userCount);
        }
        // A gap is filled first.
        eq("add to a gap", at<FnP>(0x00031E40)(o, nullptr, (void*)55), r->AddUser((AtAudioBankDatabase*)55));
        same_bytes("users", o->m_users, r->m_users, sizeof o->m_users);
        // Removing null finds a free slot, and counts it.
        eq("remove null", at<FnP>(0x00031E70)(o, nullptr, nullptr), r->RemoveUser(nullptr));
        same_bytes("users", o->m_users, r->m_users, sizeof o->m_users);
        eq("count field", o->m_userCount, r->m_userCount);
    }

    // Init, with the desc's own values, a name and none, a full table of users.
    {
        for (const char* name : {"Thing", "", "EightChr"}) {
            for (const int full : {0, 1}) {
                AtAudioBankDatabase* odb = make_o_bank_db(0);
                TestBankDb* rdb = make_r_bank_db(0);
                ((GmNameMap*)odb)->Init(8);
                ((GmNameMap*)rdb)->Init(8);
                AtAudioBank* o = (AtAudioBank*)new_bank(0, false, 0xCD);
                AtAudioBank* r = (AtAudioBank*)new_bank(1, false, 0xCD);
                if (full) {
                    for (uintptr_t i = 1; i <= 20; i++) {
                        o->AddUser((AtAudioBankDatabase*)i);
                        r->AddUser((AtAudioBankDatabase*)i);
                    }
                }
                AtAudioBankDesc a, b;
                a.m_database = odb;
                b.m_database = rdb;
                std::memset(a.m_name, 0, 8);
                std::memset(b.m_name, 0, 8);
                std::memcpy(a.m_name, name, std::strlen(name));
                std::memcpy(b.m_name, name, std::strlen(name));
                a.m_format = b.m_format = 4;
                a.m_residence = b.m_residence = 2;
                a.m_blockId = b.m_blockId = 123;
                a.m_flags = b.m_flags = 3;
                o->m_state = r->m_state = 0x40;
                { On on(0); at<FnV1>(0x00031F00)(o, nullptr, &a); }
                { On on(1); r->Init(b); }
                g_recording = false;
                same_bank_bytes("init", o, r);
                eq("user added", o->m_userCount, r->m_userCount);
                for (int i = 0; i < 20; i++) {
                    eq("user", o->m_users[i] == odb, r->m_users[i] == rdb);
                }
                same_map(odb, rdb, false);
                compare_logs("init");
                o->m_userCount = r->m_userCount = 0;
                ((GmNameMap*)odb)->GmNameMap::Clear();
                ((GmNameMap*)rdb)->GmNameMap::Clear();
                std::free(odb);
                std::free(rdb);
                g_dbs[0][0] = g_dbs[1][0] = nullptr;
            }
        }
    }

    // Load, Unload and Update: each state, each place for the first user, and no user at all.
    GmNameMapGroup groupA, groupB;
    std::memcpy(groupA.m_name, "Group\0\0\0", 8);
    std::memcpy(groupB.m_name, "Group\0\0\0", 8);
    FakeUser user(&groupA, &groupB);
    FakeUser ungrouped(nullptr, nullptr);
    for (int state = 0; state < 16; state++) {
        for (const int u1c : {0, 3, 5}) {
            for (const int place : {-1, 0, 7, 19}) {
                g_context = "bank state " + std::to_string(state) + " " + std::to_string(u1c) + " " + std::to_string(place);
                AtAudioBank* o = (AtAudioBank*)new_bank(0, false, 0xCD);
                AtAudioBank* r = (AtAudioBank*)new_bank(1, false, 0xCD);
                const auto arrange = [&] {
                    o->m_state = r->m_state = state;
                    o->m_unknown1C = r->m_unknown1C = u1c;
                    std::memset(o->m_users, 0, sizeof o->m_users);
                    std::memset(r->m_users, 0, sizeof r->m_users);
                    if (place >= 0) {
                        o->m_users[place] = user.db[0];
                        r->m_users[place] = user.db[1];
                        // A later user with another owner must not be the one asked.
                        if (place < 19) {
                            o->m_users[19] = ungrouped.db[1];
                            r->m_users[19] = ungrouped.db[0];
                        }
                    }
                };
                int ov = -1, rv = -1;
                arrange();
                bool oc = guarded([&] { On on(0); ov = at<FnI>(0x00031F70)(o, nullptr); });
                g_recording = false;
                bool rc = guarded([&] { On on(1); rv = r->AtAudioBank::Load(); });
                g_recording = false;
                eq("load crashes", oc, rc);
                eq("load", ov, rv);
                same_bank_bytes("load", o, r);
                arrange();
                ov = rv = -1;
                oc = guarded([&] { On on(0); ov = at<FnI>(0x00031FC0)(o, nullptr); });
                g_recording = false;
                rc = guarded([&] { On on(1); rv = r->AtAudioBank::Unload(); });
                g_recording = false;
                eq("unload crashes", oc, rc);
                eq("unload", ov, rv);
                same_bank_bytes("unload", o, r);
                arrange();
                { On on(0); at<Fn0>(0x00032010)(o, nullptr); }
                { On on(1); r->AtAudioBank::Update(); }
                same_bank_bytes("update", o, r);
            }
        }
    }

    // Names: a user with a group, without, none, and a user not first.
    g_context = "names";
    for (int kind = 0; kind < 4; kind++) {
        AtAudioBank* o = (AtAudioBank*)new_bank(0, false, 0xCD);
        AtAudioBank* r = (AtAudioBank*)new_bank(1, false, 0xCD);
        std::memcpy(o->m_name, "BankName", 8);
        std::memcpy(r->m_name, "BankName", 8);
        if (kind == 1) { o->m_users[3] = user.db[0]; r->m_users[3] = user.db[1]; }
        if (kind == 2) { o->m_users[0] = ungrouped.db[0]; r->m_users[0] = ungrouped.db[1]; }
        if (kind == 3) { o->m_users[2] = ungrouped.db[0]; r->m_users[2] = ungrouped.db[1]; o->m_users[9] = user.db[0]; r->m_users[9] = user.db[1]; }
        char a1[9], a2[9], b1[9], b2[9];
        std::memset(a1, 0x41, 9); std::memset(a2, 0x41, 9); std::memset(b1, 0x41, 9); std::memset(b2, 0x41, 9);
        at<FnNames>(0x00032020)(o, nullptr, a1, a2);
        r->GetNames(b1, b2);
        same_bytes("own name", a1, b1, 9);
        same_bytes("space", a2, b2, 9);
    }

    // Destroying: with a file open, none, and a file with no file system, which crashes where the shape's other destructor would not.
    g_context = "bank destroyed";
    for (int kind = 0; kind < 3; kind++) {
        for (const unsigned flags : {0u, 1u}) {
            AtAudioBank* o = (AtAudioBank*)new_bank(0, false, 0xCD);
            AtAudioBank* r = (AtAudioBank*)new_bank(1, false, 0xCD);
            static int token;
            const auto arrange = [&](AtAudioBank* b) {
                b->m_file.m_fs = kind == 1 ? g_fs : nullptr;
                b->m_file.m_file = kind == 0 ? nullptr : (AtIoFile*)&token;
            };
            arrange(o);
            arrange(r);
            void* ov = nullptr;
            bool oc = guarded([&] { On on(0); ov = at<FnDel>(0x00031EE0)(o, nullptr, flags); });
            g_recording = false;
            bool rc = guarded([&] {
                On on(1);
                if (flags) {
                    delete r;
                } else {
                    r->~AtAudioBank();
                }
            });
            g_recording = false;
            eq("destroying crashes", oc, rc);
            if (!oc) eq("returns this", (uintptr_t)ov, (uintptr_t)o);
            compare_logs("bank destroyed");
            if (!flags && !oc) {
                eq("vftable afterwards", *(uint32_t*)o, 0x00274A90);
                eq("file afterwards", o->m_file.m_fs != nullptr || o->m_file.m_file != nullptr, r->m_file.m_fs != nullptr || r->m_file.m_file != nullptr);
            }
        }
    }
}

static void run_owner() {
    g_context = "owner";
    Env env;
    GmNameMap a, b, c, d;
    typedef void(__fastcall* Fn)(void*, void*, void*, const char*);
    struct Which { uint32_t va; void (AtNameTableOwner::*r)(GmNameMap*, const char*); GmNameMapGroup AtNameTableOwner::*plain; GmNameSpaceDb AtNameTableOwner::*spaces; };
    const Which whichs[] = {{0x00031A30, &AtNameTableOwner::AddToFirst, &AtNameTableOwner::m_first, &AtNameTableOwner::m_firstSpaces}, {0x00031A60, &AtNameTableOwner::AddToSecond, &AtNameTableOwner::m_second, &AtNameTableOwner::m_secondSpaces}};
    for (const Which& w : whichs) {
        for (const char* ns : {(const char*)nullptr, "", "Space", "Fresh"}) {
            at<Fn>(w.va)(g_own[0], nullptr, &a, ns);
            (g_own[1]->*w.r)(&b, ns);
            eq("in the plain group", a.m_group == &(g_own[0]->*w.plain), b.m_group == &(g_own[1]->*w.plain));
            eqs("group's name", group_name(a.m_group).c_str(), group_name(b.m_group).c_str());
            eq("head", (g_own[0]->*w.plain).m_first == &a, (g_own[1]->*w.plain).m_first == &b);
            eq("space's pool", (g_own[0]->*w.spaces).m_free ? (g_own[0]->*w.spaces).m_free - (g_own[0]->*w.spaces).m_pool : -1, (g_own[1]->*w.spaces).m_free ? (g_own[1]->*w.spaces).m_free - (g_own[1]->*w.spaces).m_pool : -1);
            at<Fn>(w.va)(g_own[0], nullptr, &c, ns);
            (g_own[1]->*w.r)(&d, ns);
            eq("second's next", c.m_next == &a, d.m_next == &b);
        }
    }
}

// ---- memory ------------------------------------------------------------------------------------------------------------
static void run_memory() {
    char dir[] = "SOMEWHERE";
    // No room: the raise, and then the list is null. A count of zero asks for nothing. The name map's own request comes first and fails too.
    for (const char* text : {"Bank[0]\n{\n}\n", "Bank[1]\n{\n Bank \"A\"\n {\n }\n}\n"}) {
        g_context = std::string("no memory: ") + text;
        Env env;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        Direct da(text, AtAudioBankDatabase::s_keywords, AtAudioBankDatabase::kKeywordCount), db(text, AtAudioBankDatabase::s_keywords, AtAudioBankDatabase::kKeywordCount);
        g_fail_alloc = true;
        const bool oc = guarded([&] { On on(0); at<FnReadB>(0x00025860)(o, nullptr, g_own[0], &da.tokenizer, dir); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer, dir); });
        g_recording = false;
        g_fail_alloc = false;
        eq("crash on no list", oc, rc);
        compare_logs("no memory");
    }
    // No heap: the raise, and it goes on with a null heap.
    g_context = "no heap";
    {
        Env env;
        *(void**)(uintptr_t)0x002C6338 = nullptr;
        AtAudioBankDatabase::s_heap = nullptr;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        const std::string text = kBanksForSegments;
        set_text(text, "AUDIO\\THING.AOB");
        load_bank(o, r, "no heap");
        clear_bank_db(o, r, "cleared");
        destroy_bank_db(o, r, "destroyed");
        *(void**)(uintptr_t)0x002C6338 = &g_heap_token;
        AtAudioBankDatabase::s_heap = &g_heap_token;
    }
    // A file that will not open: the report, then a crash on what is not there, on both sides. In the game the report does not come back.
    for (const int status : {1, 2, 5}) {
        g_context = "no file";
        Env env;
        g_fs->open_status = status;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        AtAudioSegmentDatabase* so = make_o_seg_db(2);
        TestSegDb* sr = make_r_seg_db(2);
        bool oc = guarded([&] { On on(0); at<FnLoad>(0x00025A90)(o, nullptr, g_own[0], "AUDIO\\THING", 77); });
        g_recording = false;
        bool rc = guarded([&] { On on(1); r->Load(g_own[1], "AUDIO\\THING", 77); });
        g_recording = false;
        eq("bank file crash", oc, rc);
        compare_logs("bank file will not open");
        oc = guarded([&] { On on(0); at<FnLoad>(0x00025640)(so, nullptr, g_own[0], "AUDIO\\THING", 77); });
        g_recording = false;
        rc = guarded([&] { On on(1); sr->Load(g_own[1], "AUDIO\\THING", 77); });
        g_recording = false;
        eq("segment file crash", oc, rc);
        compare_logs("segment file will not open");
        g_fs->open_status = 0;
        g_fs->m_path.m_top = -1;
    }
}

// ---- each thing a database and a segment do on their own -------------------------------------------------------------------
static void run_direct() {
    char dir[] = "SOMEWHERE";
    // Allocate from its own arguments, and with a directory not the one Read would pass.
    g_context = "allocate";
    for (const unsigned count : {0u, 1u, 3u}) {
        for (const char* ns : {(const char*)nullptr, "", "Space"}) {
            Env env;
            AtAudioBankDatabase* o = make_o_bank_db(0);
            TestBankDb* r = make_r_bank_db(0);
            AtAudioBankDatabase::Args a = {g_own[0], count, ns, dir}, b = {g_own[1], count, ns, dir};
            { On on(0); at<FnV1>(0x00025700)(o, nullptr, &a); }
            { On on(1); r->Allocate(b); }
            g_recording = false;
            eq("list filled from the heap's fill", o->m_banks && r->m_banks ? std::memcmp(o->m_banks, r->m_banks, count * 4) : 0, 0);
            eq("list", o->m_banks != nullptr, r->m_banks != nullptr);
            eq("count", o->m_count, r->m_count);
            same_map(o, r, false);
            same_owner();
            compare_logs("allocate");
            if (o->m_banks) std::memset(o->m_banks, 0, count * 4);
            if (r->m_banks) std::memset(r->m_banks, 0, count * 4);
            clear_bank_db(o, r, "cleared");
            destroy_bank_db(o, r, "destroyed");
        }
    }

    // A deleting destructor with the delete, on each list, and a clear of a list with a live table.
    g_context = "deleting";
    {
        Env env;
        for (const int kind : {0, 1}) {
            void* o = kind ? (void*)make_o_seg_db(2) : (void*)make_o_bank_db(0);
            void* r = kind ? (void*)make_r_seg_db(2) : (void*)make_r_bank_db(0);
            ((GmNameMap*)o)->Init(4);
            ((GmNameMap*)r)->Init(4);
            { On on(0); eq("returns this", (uintptr_t)at<FnDel>(kind ? 0x00025530 : 0x00025840)(o, nullptr, 1), (uintptr_t)o); }
            { On on(1); delete (GmNameMap*)r; }
            compare_logs("deleted");
        }
        // The complete destructors alone, with a table, in a group.
        for (const int kind : {0, 1}) {
            void* o = kind ? (void*)make_o_seg_db(2) : (void*)make_o_bank_db(0);
            void* r = kind ? (void*)make_r_seg_db(2) : (void*)make_r_bank_db(0);
            ((GmNameMap*)o)->Init(4);
            ((GmNameMap*)r)->Init(4);
            g_own[0]->m_first.Add((GmNameMap*)o);
            g_own[1]->m_first.Add((GmNameMap*)r);
            { On on(0); at<Fn0>(kind ? 0x00025500 : 0x000256F0)(o, nullptr); }
            { On on(1); ((GmNameMap*)r)->~GmNameMap(); }
            eq("left its group", g_own[0]->m_first.m_first == nullptr, g_own[1]->m_first.m_first == nullptr);
            compare_logs("destroyed");
            std::free(o);
            std::free(r);
        }
        g_dbs[0][0] = g_dbs[1][0] = g_dbs[0][2] = g_dbs[1][2] = nullptr;
    }

    // Clear of a bank list holding banks shared and alone: listed by hand, with holes and a count that disagrees.
    g_context = "clear by hand";
    for (const int users : {1, 2, 3, 0}) {
        Env env;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        o->m_count = r->m_count = 3;
        o->m_owner = g_own[0];
        r->m_owner = g_own[1];
        o->m_banks = (AtAudioBank**)std::calloc(3, 4);
        r->m_banks = (AtAudioBank**)std::calloc(3, 4);
        AtAudioBank* ob = (AtAudioBank*)new_bank(0, false, 0xCD);
        AtAudioBank* rb = (AtAudioBank*)new_bank(1, false, 0xCD);
        ob->m_userCount = rb->m_userCount = users;
        ob->m_users[1] = o;
        rb->m_users[1] = r;
        ob->m_users[4] = (AtAudioBankDatabase*)(uintptr_t)4;
        rb->m_users[4] = (AtAudioBankDatabase*)(uintptr_t)4;
        o->m_banks[1] = ob;
        r->m_banks[1] = rb;
        { On on(0); at<Fn0>(0x00025790)(o, nullptr); }
        { On on(1); r->Clear(); }
        same_bank_db("clear by hand", o, r);
        if (users != 1) same_bank(ob, rb);
        destroy_bank_db(o, r, "destroyed");
    }
    // Two banks to destroy: the second's Reset sees the first's entry gone. And a count with no list, which must not be walked.
    g_context = "clear by hand, two";
    {
        Env env;
        AtAudioBankDatabase* o = make_o_bank_db(0);
        TestBankDb* r = make_r_bank_db(0);
        o->m_count = r->m_count = 3;
        o->m_banks = (AtAudioBank**)std::calloc(3, 4);
        r->m_banks = (AtAudioBank**)std::calloc(3, 4);
        for (const int i : {0, 2}) {
            AtAudioBank* ob = (AtAudioBank*)new_bank(0, false, 0xCD);
            AtAudioBank* rb = (AtAudioBank*)new_bank(1, false, 0xCD);
            ob->m_userCount = rb->m_userCount = 1;
            o->m_banks[i] = ob;
            r->m_banks[i] = rb;
        }
        g_watch[0] = o->m_banks;
        g_watch[1] = r->m_banks;
        { On on(0); at<Fn0>(0x00025790)(o, nullptr); }
        { On on(1); r->Clear(); }
        g_watch[0] = g_watch[1] = nullptr;
        same_bank_db("two banks", o, r);
        o->m_count = r->m_count = 3;
        const bool oc = guarded([&] { On on(0); at<Fn0>(0x00025790)(o, nullptr); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Clear(); });
        g_recording = false;
        eq("a count with no list: crash", oc, rc);
        eq("a count with no list: count", o->m_count, r->m_count);
        compare_logs("a count with no list");
        destroy_bank_db(o, r, "destroyed");
    }

    // One segment read alone: a null database, a name of each length, and a fourth argument that does nothing.
    g_context = "segment read";
    for (const char* text : {"{ }", "{ Looping }", "{ 5 }", "5 }", "{ Looping 5 Looping }", "}"}) {
        for (const char* name : {(const char*)"S", (const char*)"EightChr", (const char*)"", (const char*)nullptr}) {
            Env env;
            unsigned char a[sizeof(TestSegment)], b[sizeof(TestSegment)];
            std::memset(a, 0xCD, sizeof a);
            std::memset(b, 0xCD, sizeof b);
            at<Fn0>(0x00031C10)(a, nullptr);
            *(const void***)a = g_seg_table;
            TestSegment* r = new (b) TestSegment;
            Direct da(text, AtAudioSegmentDatabase::s_keywords, AtAudioSegmentDatabase::kKeywordCount), db(text, AtAudioSegmentDatabase::s_keywords, AtAudioSegmentDatabase::kKeywordCount);
            const bool oc = guarded([&] { On on(0); at<FnReadSeg>(0x00031C50)(a, nullptr, nullptr, &da.tokenizer, name, "ignored"); });
            g_recording = false;
            const bool rc = guarded([&] { On on(1); r->Read(nullptr, &db.tokenizer, name, "ignored"); });
            g_recording = false;
            eq("crash", oc, rc);
            eq("bank", ((AtAudioSegment*)a)->m_bank != nullptr, r->m_bank != nullptr);
            eq("index", ((AtAudioSegment*)a)->m_index, r->m_index);
            eq("looping", ((AtAudioSegment*)a)->m_looping, r->m_looping);
            compare_logs("segment read");
        }
    }
    // A Bank word with no database: a crash, on both.
    g_context = "segment read, no database";
    {
        Env env;
        unsigned char a[sizeof(TestSegment)], b[sizeof(TestSegment)];
        at<Fn0>(0x00031C10)(a, nullptr);
        *(const void***)a = g_seg_table;
        TestSegment* r = new (b) TestSegment;
        const char* text = "{ Bank \"A\" \"\" 1 }";
        Direct da(text, AtAudioSegmentDatabase::s_keywords, AtAudioSegmentDatabase::kKeywordCount), db(text, AtAudioSegmentDatabase::s_keywords, AtAudioSegmentDatabase::kKeywordCount);
        const bool oc = guarded([&] { On on(0); at<FnReadSeg>(0x00031C50)(a, nullptr, nullptr, &da.tokenizer, "S", nullptr); });
        g_recording = false;
        const bool rc = guarded([&] { On on(1); r->Read(nullptr, &db.tokenizer, "S", nullptr); });
        g_recording = false;
        eq("crash", oc, rc);
        eq("did crash", oc, 1);
        compare_logs("segment read, no database");
    }
    // Reading over what is there, with a count already set: the slot is what clears it.
    g_context = "read over";
    {
        Env env;
        AtAudioSegmentDatabase* o = make_o_seg_db(2);
        TestSegDb* r = make_r_seg_db(2);
        const std::string text = "Segment[1]\n{\n Segment \"S\"\n {\n }\n}\n";
        set_text(text, "AUDIO\\THING.AOS");
        load_seg(o, r, "first", 2);
        const std::string empty = "";
        set_text(empty, "AUDIO\\THING.AOS");
        load_seg(o, r, "over it, with nothing", 2);
        eq("cleared by reading", r->m_count, 0);
        destroy_seg_db(o, r, "destroyed");
    }
}

// ---- real files ---------------------------------------------------------------------------------------------------------
static std::string read_case(const std::string& dir, const char* file) {
    FILE* f = nullptr;
    if (fopen_s(&f, (dir + "\\" + file).c_str(), "rb") || !f) {
        std::printf("cannot read %s\n", file);
        std::exit(2);
    }
    std::fseek(f, 0, SEEK_END);
    std::string data((size_t)std::ftell(f), 0);
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    return data;
}

static void run_real(const std::string& dir) {
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) {
        std::printf("cannot read index.txt\n");
        std::exit(2);
    }
    // By path without its extension, so that a pair is loaded together. Identical files appear once in the index, so some of either have no partner here.
    struct Pair { std::string aob, aos; };
    std::map<std::string, Pair> pairs;
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        const char* tab1 = std::strchr(line, '\t');
        const char* tab2 = tab1 ? std::strchr(tab1 + 1, '\t') : nullptr;
        const char* tab3 = tab2 ? std::strchr(tab2 + 1, '\t') : nullptr;
        if (!tab3) continue;
        const std::string file(line, tab1 - line);
        const std::string path(tab2 + 1, tab3 - tab2 - 1);
        if (path.size() < 4) continue;
        std::string ext = path.substr(path.size() - 4);
        for (char& c : ext) c = (char)toupper((unsigned char)c);
        std::string stem = path.substr(0, path.size() - 4);
        for (char& c : stem) c = (char)toupper((unsigned char)c);
        if (ext == ".AOB") pairs[stem].aob = file;
        if (ext == ".AOS") pairs[stem].aos = file;
    }
    std::fclose(index);
    for (const auto& p : pairs) {
        const bool a = !p.second.aob.empty(), s = !p.second.aos.empty();
        if (!a && !s) continue;
        const std::string aob = a ? read_case(dir, p.second.aob.c_str()) : std::string();
        const std::string aos = s ? read_case(dir, p.second.aos.c_str()) : std::string();
        run_pair(a ? &aob : nullptr, s ? &aos : nullptr, p.first);
        g_files_aob += a;
        g_files_aos += s;
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        if (guard_resume(e)) return EXCEPTION_CONTINUE_EXECUTION;
        char text[300];
        const int len = sprintf_s(text, "CRASH 0x%08X at 0x%p ecx %X edi %X esi %X eax %X during [%.200s]\n", (unsigned)c, e->ExceptionRecord->ExceptionAddress, e->ContextRecord->Ecx, e->ContextRecord->Edi, e->ContextRecord->Esi, e->ContextRecord->Eax, g_context.c_str());
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
    if (pos.size() != 2) { std::printf("usage: oracle_aob <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_fs->files[0].m_path[0] = g_fs->files[1].m_path[0] = 0;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u, 0x002C6338u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    AtAudioBankDatabase::s_heap = &g_heap_token;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_keywords();
    run_setters();
    run_small_objects();
    run_owner();
    run_lengths();
    run_bank_alone();
    run_written();
    run_open_texts();
    run_direct();
    run_memory();

    g_banks_seen = g_segments_seen = 0;
    run_real(pos[1]);
    report_entries();
    std::printf("%s: %u .AOB and %u .AOS files, %u banks, %u segments; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files_aob, g_files_aos, g_banks_seen, g_segments_seen, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
