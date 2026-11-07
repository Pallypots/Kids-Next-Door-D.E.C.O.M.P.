// Differential oracle for the .AOD and .AOE files and the Xbox sound classes (Windows x86, local only). See docs/verification.md.
//
//   oracle_aod <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Every .AOD on the disc is loaded on each side, through the original's or the recovered concrete classes, down through the recovered .AOB, .AOS and .AOE readers on the recovered side, and the original's on the other. The lists in
// archives are in the text cases; those loose on the disc are in <cases>\..\scratch\aod\loose, written by tests/oracle/aod_loose_cases.py. Beneath both sides, doubles: the bank's three slots that open the .WBA file and reach XACT, the
// emitter's SDK calls, the matrices, and the files.
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

#include "at/AtAudioDatabase.h"
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

static void set_list_heaps();

// ---- what stands beneath both sides ---------------------------------------------------------------------------------------------
static int g_load_status = AtIoFile::kPending;
static int g_create_status, g_mixbin_status;
static unsigned g_stream_next[2];
static int g_status_left[2];

static uint32_t name_hash(const char* n) {
    uint32_t h = 17;
    for (int i = 0; i < 8 && n[i]; i++) h = h * 31 + (unsigned char)n[i];
    return h;
}

// Gives the bank blocks to free, so that what Reset frees can be seen.
static int xact_load(int side, AtSoXboxXactBank* b) {
    const bool was = g_recording;
    g_recording = false;
    b->m_block1C0 = std::malloc(4);
    b->m_block1C8 = std::malloc(4);
    b->m_block1CC = std::malloc(4);
    b->m_block1D8 = std::malloc(4);
    b->m_block1DC = std::malloc(4);
    g_recording = was;
    g_log[side].add('L', name_hash(b->m_name));
    g_log[side].add('l', b->m_system != nullptr);
    return g_load_status;
}
static void xact_process(int side, AtSoXboxXactBank* b) {
    g_log[side].add('G', name_hash(b->m_name));
    b->m_step = 8;
}
static void xact_update(int side, AtSoXboxXactBank* b) { g_log[side].add('U', name_hash(b->m_name)); }

static int ds_create(int side, void* ds, int kind, void** stream) {
    g_log[side].add('s', kind);
    g_log[side].add('w', (uint32_t)(uintptr_t)ds);
    *stream = g_create_status < 0 ? nullptr : (void*)(uintptr_t)(0x1000 + 4 * g_stream_next[side]++);
    return g_create_status;
}
static int ds_mixbins(int side, void* stream, const AtSoXboxMixBins* bins) {
    g_log[side].add('b', (uint32_t)(uintptr_t)stream);
    g_log[side].add('b', bins->m_count);
    g_log[side].add('b', bins->m_bins[0].m_bin);
    g_log[side].add('b', (uint32_t)bins->m_bins[0].m_volume);
    return g_mixbin_status;
}
static void ds_stop(int side, void* stream) { g_log[side].add('t', (uint32_t)(uintptr_t)stream); g_status_left[side] = 2; }
static void ds_status(int side, void* stream, unsigned* status) {
    g_log[side].add('u', (uint32_t)(uintptr_t)stream);
    *status = g_status_left[side]-- > 0 ? 1 : 0;
}
static void ds_release(int side, void* stream) { g_log[side].add('e', (uint32_t)(uintptr_t)stream); }

int AtSoXboxXactBankLoad(AtSoXboxXactBank* b) { return xact_load(1, b); }
void AtSoXboxXactBankProcess(AtSoXboxXactBank* b) { xact_process(1, b); }
void AtSoXboxXactBankUpdate(AtSoXboxXactBank* b) { xact_update(1, b); }
int AtSoXboxCreateStream(void* d, int k, void** s) { return ds_create(1, d, k, s); }
int AtSoXboxSetMixBins(void* s, const AtSoXboxMixBins* b) { return ds_mixbins(1, s, b); }
void AtSoXboxStopStream(void* s) { ds_stop(1, s); }
void AtSoXboxGetStreamStatus(void* s, unsigned* st) { ds_status(1, s, st); }
void AtSoXboxReleaseStream(void* s) { ds_release(1, s); }

static int __fastcall o_xact_load(void* self, void*) { return xact_load(0, (AtSoXboxXactBank*)self); }
static void __fastcall o_xact_process(void* self, void*) { xact_process(0, (AtSoXboxXactBank*)self); }
static void __fastcall o_xact_update(void* self, void*) { xact_update(0, (AtSoXboxXactBank*)self); }
static int __stdcall o_ds_create(void* d, int k, void** s) { return ds_create(0, d, k, s); }
static int __stdcall o_ds_mixbins(void* s, const AtSoXboxMixBins* b) { return ds_mixbins(0, s, b); }
static void __stdcall o_ds_stop(void* s) { ds_stop(0, s); }
static void __stdcall o_ds_status(void* s, unsigned* st) { ds_status(0, s, st); }
static void __stdcall o_ds_release(void* s) { ds_release(0, s); }

// Matrices: an id, and each side's own count of them.
struct Mat { uint32_t id; };
enum { kMats = 4096 };
static Mat g_mats[2][kMats];
static unsigned g_mat_used[2];
static uint32_t mat_id(int side, const void* p) {
    if (!p) return 0xFFFFFFFF;
    const Mat* m = (const Mat*)p;
    if (m >= g_mats[side] && m < g_mats[side] + kMats) return (uint32_t)(m - g_mats[side]);
    return 0xFFFFFFFE;
}
static uint32_t bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static void __cdecl m_identity(AtMatrix* m) { g_log[g_side].add('i', mat_id(g_side, m)); }
static void __cdecl m_translation(AtMatrix* m, const AtVector3* v) {
    Log& l = g_log[g_side];
    l.add('T', mat_id(g_side, m));
    l.add('x', bits(v->x));
    l.add('x', bits(v->y));
    l.add('x', bits(v->z));
}
static void __cdecl m_rotation(AtMatrix* m, const AtQuaternion* q) {
    Log& l = g_log[g_side];
    l.add('Q', mat_id(g_side, m));
    l.add('x', bits(q->x));
    l.add('x', bits(q->y));
    l.add('x', bits(q->z));
    l.add('x', bits(q->w));
}
class TestMatrixDevice : public AtMatrixDevice {
public:
    int side = 0;
    AtMatrix* NewMatrix() override {
        g_log[side].add('n');
        return (AtMatrix*)&g_mats[side][std::min(g_mat_used[side]++, (unsigned)kMats - 1)];
    }
    AtMatrixPool* NewPool() override { return nullptr; }
    void Slot2() override {}
    void Slot3() override {}
    void FreeMatrix(AtMatrix* m) override { g_log[side].add('f', mat_id(side, m)); }
    void FreePool(AtMatrixPool*) override {}
};
static TestMatrixDevice g_matdev[2];

class TestSystem : public AtSoXboxSystem {
public:
    int side = 0;
    void Unknown0() override { g_log[side].add('z', 0); }
    void Unknown1() override { g_log[side].add('z', 1); }
    void Add(AtAudioBank*) override { g_log[side].add('z', 2); }
    void Remove(AtAudioBank*) override { g_log[side].add('z', 3); }
};
static TestSystem* g_system[2];
static AtNameTableOwner* g_own[2];
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token;
static int g_heap_database, g_heap_bank, g_heap_segment, g_heap_emitter;  // Distinct, so that using the wrong one shows.

// The disc's files, by path in capitals, on a file system that looks them up. A .WBA is never opened, its slots being doubles.
class TreeFs : public MemFs {
public:
    std::map<std::string, std::string> tree;
    std::map<std::string, std::string>* extra = nullptr;
    MemFile pool[2][8];
    int next[2] = {0, 0};
    int missing = 0;
    const std::string* find(const char* path) {
        m_path.Resolve(path);
        std::string key(m_path.m_full);
        for (char& c : key) c = c == '/' ? '\\' : (char)toupper((unsigned char)c);
        if (extra) {
            auto f = extra->find(key);
            if (f != extra->end()) return &f->second;
        }
        auto f = tree.find(key);
        return f == tree.end() ? nullptr : &f->second;
    }
    int Open(const char* path, int flags, AtIoFile** out) override {
        const bool was = g_recording;
        g_recording = false;
        const std::string* text = find(path);
        g_recording = was;
        g_log[g_side].add('O', (uint32_t)flags);
        record_message(m_path.m_full, g_side);
        if (!text) {
            *out = nullptr;
            return AtIoFile::kNotFound;
        }
        MemFile& f = pool[g_side][next[g_side]++ & 7];
        f.side = g_side;
        f.m_position = 0;
        f.reads = 0;
        f.m_flags = 2;
        f.data = (const unsigned char*)text->data();
        f.size = text->size();
        strcpy_s(f.m_path, m_path.m_full);
        *out = &f;
        return 0;
    }
    int IsFileMissing(const char* path) override {
        const bool was = g_recording;
        g_recording = false;
        const bool gone = find(path) == nullptr;
        g_recording = was;
        g_log[g_side].add('M', gone);
        if (gone) missing++;
        return gone ? 1 : 0;
    }
};
static TreeFs* g_fs;

// ---- comparing -----------------------------------------------------------------------------------------------------------------
typedef std::map<const void*, int> Ids;
static Ids g_bank_ids[2];
static int id_of(const Ids& ids, const void* p) {
    if (!p) return -1;
    auto f = ids.find(p);
    return f == ids.end() ? -2 : f->second;
}

static std::string group_name(const GmNameMapGroup* g) {
    if (!g) return "(none)";
    return std::string(g->m_name, strnlen(g->m_name, 8));
}

static void same_map(const GmNameMap* o, const GmNameMap* r) {
    eq("size", o->m_size, r->m_size);
    eq("entries", o->m_entries != nullptr, r->m_entries != nullptr);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            const GmNameMap::Entry& x = o->m_entries[i];
            const GmNameMap::Entry& y = r->m_entries[i];
            eq("slot used", x.m_value != nullptr, y.m_value != nullptr);
            if (x.m_value && y.m_value) eq("slot's name", strncmp(x.m_name, y.m_name, 8), 0);
        }
    }
    eq("in a group", o->m_group != nullptr, r->m_group != nullptr);
    eqs("group's name", group_name(o->m_group).c_str(), group_name(r->m_group).c_str());
}

static void same_bank(const AtSoXboxXactBank* o, const AtSoXboxXactBank* r) {
    eq("bank's name", strncmp(o->m_name, r->m_name, 8), 0);
    eq("bank's format", o->m_format, r->m_format);
    eq("bank's residence", o->m_residence, r->m_residence);
    eq("bank's block", o->m_blockId, r->m_blockId);
    eq("bank's state", o->m_state, r->m_state);
    eq("bank's unknown 1C", o->m_unknown1C, r->m_unknown1C);
    eq("bank's users", o->m_userCount, r->m_userCount);
    eq("bank's system", o->m_system == g_system[0], r->m_system == g_system[1]);
    eq("bank's names", (unsigned char)o->m_names.m_name[0], (unsigned char)r->m_names.m_name[0]);
    eq("bank's +1A4", o->m_names.m_unknown10C, r->m_names.m_unknown10C);
    eq("bank's +1A8", o->m_names.m_unknown110, r->m_names.m_unknown110);
    eq("bank's +1AC", o->m_names.m_unknown114, r->m_names.m_unknown114);
    eq("bank's +1B0", o->m_unknown1B0, r->m_unknown1B0);
    eq("bank's +1B4", o->m_unknown1B4, r->m_unknown1B4);
    eq("bank's header", o->m_header != nullptr, r->m_header != nullptr);
    eq("bank's +1BC", o->m_unknown1BC, r->m_unknown1BC);
    eq("bank's file", o->m_fileHandle, r->m_fileHandle);
    eq("bank's status", o->m_status, r->m_status);
    eq("bank's step", o->m_step, r->m_step);
    eq("bank's +1E0", o->m_unknown1E0, r->m_unknown1E0);
    eq("bank's +1E4", o->m_unknown1E4, r->m_unknown1E4);
    eq("bank's +1E8", o->m_unknown1E8, r->m_unknown1E8);
    eq("bank's +1EC", o->m_unknown1EC, r->m_unknown1EC);
    eq("bank's +1F0", o->m_unknown1F0, r->m_unknown1F0);
    eq("bank's +1F4", o->m_unknown1F4, r->m_unknown1F4);
    eq("bank's +1F8", o->m_unknown1F8, r->m_unknown1F8);
    eq("bank's +1C0", o->m_block1C0 != nullptr, r->m_block1C0 != nullptr);
    eq("bank's +1C8", o->m_block1C8 != nullptr, r->m_block1C8 != nullptr);
    eq("bank's +1CC", o->m_block1CC != nullptr, r->m_block1CC != nullptr);
    eq("bank's +1D8", o->m_block1D8 != nullptr, r->m_block1D8 != nullptr);
    eq("bank's +1DC", o->m_block1DC != nullptr, r->m_block1DC != nullptr);
}

static unsigned g_banks_seen, g_segments_seen, g_emitters_seen, g_bank_dbs_seen, g_segment_dbs_seen, g_emitter_dbs_seen, g_aods_seen, g_with_segment_bank;

static void same_bank_db(const AtSoXboxBankDatabase* o, const AtSoXboxBankDatabase* r) {
    eq("list's owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
    eq("list's count", o->m_count, r->m_count);
    eq("list's banks", o->m_banks != nullptr, r->m_banks != nullptr);
    same_map(o, r);
    if (!o->m_banks || !r->m_banks || o->m_count != r->m_count) return;
    g_bank_dbs_seen++;
    for (unsigned i = 0; i < o->m_count; i++) {
        const void* a = o->m_banks[i];
        const void* b = r->m_banks[i];
        eq("bank there", a != nullptr, b != nullptr);
        if (!a || !b) continue;
        const bool fresh = g_bank_ids[0].find(a) == g_bank_ids[0].end();
        eq("bank seen before", fresh, g_bank_ids[1].find(b) == g_bank_ids[1].end());
        if (fresh) {
            const int id = (int)g_bank_ids[0].size();
            g_bank_ids[0][a] = id;
            g_bank_ids[1][b] = id;
            same_bank((const AtSoXboxXactBank*)a, (const AtSoXboxXactBank*)b);
            g_banks_seen++;
        } else {
            eq("the same bank", id_of(g_bank_ids[0], a), id_of(g_bank_ids[1], b));
        }
    }
}

static void same_segment_db(const AtSoXboxSegmentDatabase* o, const AtSoXboxSegmentDatabase* r) {
    eq("list's owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
    eq("list's count", o->m_count, r->m_count);
    eq("list's segments", o->m_segments != nullptr, r->m_segments != nullptr);
    same_map(o, r);
    if (!o->m_segments || !r->m_segments || o->m_count != r->m_count) return;
    g_segment_dbs_seen++;
    for (unsigned i = 0; i < o->m_count; i++) {
        const AtSoXboxSegment& x = o->m_segments[i];
        const AtSoXboxSegment& y = r->m_segments[i];
        eq("segment's bank", id_of(g_bank_ids[0], x.m_bank), id_of(g_bank_ids[1], y.m_bank));
        eq("segment's index", x.m_index, y.m_index);
        eq("segment's loop", x.m_looping, y.m_looping);
        if (x.m_bank) g_with_segment_bank++;
        g_segments_seen++;
    }
}

static void same_emitter(const AtSoXboxEmitter& x, const AtSoXboxEmitter& y) {
    eq("emitter's parent", x.m_parent != nullptr, y.m_parent != nullptr);
    eq("emitter's sibling", x.m_nextSibling != nullptr, y.m_nextSibling != nullptr);
    eq("emitter's child", x.m_firstChild != nullptr, y.m_firstChild != nullptr);
    eq("emitter's +10", x.m_unknown10, y.m_unknown10);
    eq("emitter's local", mat_id(0, x.m_local), mat_id(1, y.m_local));
    eq("emitter's world", mat_id(0, x.m_world), mat_id(1, y.m_world));
    eq("emitter's +1C", x.m_unknown1C, y.m_unknown1C);
    eq("emitter's +20", x.m_unknown20, y.m_unknown20);
    eq("emitter's dirty", x.m_dirty, y.m_dirty);
    eq("emitter's owner", x.m_owner == g_own[0], y.m_owner == g_own[1]);
    eq("emitter's kind", x.m_kind, y.m_kind);
    eq("emitter's +30", x.m_unknown30, y.m_unknown30);
    eq("emitter's +34", x.m_unknown34, y.m_unknown34);
    eq("emitter's +38", x.m_unknown38, y.m_unknown38);
    eq("emitter's +3C", x.m_unknown3C, y.m_unknown3C);
    eq("emitter's +40", x.m_firstVoice != nullptr, y.m_firstVoice != nullptr);
    eq("emitter's +44", x.m_lastVoice != nullptr, y.m_lastVoice != nullptr);
    eq("emitter's stream", (uintptr_t)x.m_stream, (uintptr_t)y.m_stream);
    eq("emitter's +54", x.m_unknown54, y.m_unknown54);
}

static void same_emitter_db(const AtSoXboxEmitterDatabase* o, const AtSoXboxEmitterDatabase* r) {
    eq("list's owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
    eq("list's count", o->m_count, r->m_count);
    eq("list's emitters", o->m_emitters != nullptr, r->m_emitters != nullptr);
    same_map(o, r);
    if (!o->m_emitters || !r->m_emitters || o->m_count != r->m_count) return;
    g_emitter_dbs_seen++;
    for (unsigned i = 0; i < o->m_count; i++) {
        same_emitter(o->m_emitters[i], r->m_emitters[i]);
        g_emitters_seen++;
    }
    // Each entry's value, in the map's own slots.
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            if (o->m_entries[i].m_value && r->m_entries[i].m_value) eq("slot's emitter", (const AtSoXboxEmitter*)o->m_entries[i].m_value - o->m_emitters, (const AtSoXboxEmitter*)r->m_entries[i].m_value - r->m_emitters);
        }
    }
}

// Maps in a group, as (list of what kind, index in the database), so that a map in the wrong table or list shows.
static std::string where_is(const GmNameMap* map, const AtSoXboxDatabase* d) {
    char text[64];
    if (d->m_bankDatabases) for (unsigned i = 0; i < d->m_bankCount; i++) if (map == &d->m_bankDatabases[i]) { sprintf_s(text, "b%u", i); return text; }
    if (d->m_segmentDatabases) for (unsigned i = 0; i < d->m_segmentCount; i++) if (map == &d->m_segmentDatabases[i]) { sprintf_s(text, "s%u", i); return text; }
    if (d->m_emitterDatabases) for (unsigned i = 0; i < d->m_emitterCount; i++) if (map == &d->m_emitterDatabases[i]) { sprintf_s(text, "e%u", i); return text; }
    return "?";
}
static std::string walk(const GmNameMapGroup& g, const AtSoXboxDatabase* d) {
    std::string out;
    for (const GmNameMap* m = g.m_first; m; m = m->m_next) out += where_is(m, d) + ",";
    return out;
}
static std::string walk_spaces(const GmNameSpaceDb& spaces, const AtSoXboxDatabase* d) {
    std::string out;
    if (!spaces.m_pool) return out;
    for (unsigned i = 0; i < spaces.m_poolSize; i++) {
        const GmNameMapGroup& g = spaces.m_pool[i];
        if (g.m_first) out += group_name(&g) + ":" + walk(g, d) + ";";
    }
    return out;
}
static void same_owner(const AtSoXboxDatabase* o, const AtSoXboxDatabase* r) {
    AtNameTableOwner* a = g_own[0];
    AtNameTableOwner* b = g_own[1];
    eqs("first table", walk(a->m_first, o).c_str(), walk(b->m_first, r).c_str());
    eqs("second table", walk(a->m_second, o).c_str(), walk(b->m_second, r).c_str());
    eqs("third table", walk(a->m_nodes, o).c_str(), walk(b->m_nodes, r).c_str());
    eqs("first spaces", walk_spaces(a->m_firstSpaces, o).c_str(), walk_spaces(b->m_firstSpaces, r).c_str());
    eqs("second spaces", walk_spaces(a->m_secondSpaces, o).c_str(), walk_spaces(b->m_secondSpaces, r).c_str());
    eqs("third spaces", walk_spaces(a->m_nodeSpaces, o).c_str(), walk_spaces(b->m_nodeSpaces, r).c_str());
}

static void same_db(const AtSoXboxDatabase* o, const AtSoXboxDatabase* r, int depth = 0) {
    eq("database's owner", o->m_owner == g_own[0], r->m_owner == g_own[1]);
    eq("bank count", o->m_bankCount, r->m_bankCount);
    eq("segment count", o->m_segmentCount, r->m_segmentCount);
    eq("emitter count", o->m_emitterCount, r->m_emitterCount);
    eq("nested count", o->m_nestedCount, r->m_nestedCount);
    eq("bank lists", o->m_bankDatabases != nullptr, r->m_bankDatabases != nullptr);
    eq("segment lists", o->m_segmentDatabases != nullptr, r->m_segmentDatabases != nullptr);
    eq("emitter lists", o->m_emitterDatabases != nullptr, r->m_emitterDatabases != nullptr);
    eq("nested", o->m_nestedDatabases != nullptr, r->m_nestedDatabases != nullptr);
    // Order matters: segments refer to banks already seen.
    if (o->m_bankDatabases && r->m_bankDatabases && o->m_bankCount == r->m_bankCount) for (unsigned i = 0; i < o->m_bankCount; i++) same_bank_db(&o->m_bankDatabases[i], &r->m_bankDatabases[i]);
    if (o->m_segmentDatabases && r->m_segmentDatabases && o->m_segmentCount == r->m_segmentCount) for (unsigned i = 0; i < o->m_segmentCount; i++) same_segment_db(&o->m_segmentDatabases[i], &r->m_segmentDatabases[i]);
    if (o->m_emitterDatabases && r->m_emitterDatabases && o->m_emitterCount == r->m_emitterCount) for (unsigned i = 0; i < o->m_emitterCount; i++) same_emitter_db(&o->m_emitterDatabases[i], &r->m_emitterDatabases[i]);
    if (o->m_nestedDatabases && r->m_nestedDatabases && o->m_nestedCount == r->m_nestedCount) for (unsigned i = 0; i < o->m_nestedCount; i++) same_db(&o->m_nestedDatabases[i], &r->m_nestedDatabases[i], depth + 1);
    if (depth == 0) same_owner(o, r);
}

static void same_statics(const char* what) {
    eq(what, *(void**)(uintptr_t)0x002C6280 != nullptr, AtAudioDatabase::s_bankNames != nullptr);
    eq(what, *(void**)(uintptr_t)0x002C6284 != nullptr, AtAudioDatabase::s_segmentNames != nullptr);
    eq(what, *(void**)(uintptr_t)0x002C6288 != nullptr, AtAudioDatabase::s_emitterNames != nullptr);
    eq(what, *(void**)(uintptr_t)0x002C628C != nullptr, AtAudioDatabase::s_nestedNames != nullptr);
}

// ---- running ---------------------------------------------------------------------------------------------------------------------
typedef void(__fastcall* Fn0)(void*, void*);
typedef void*(__fastcall* FnP0)(void*, void*);
typedef int(__fastcall* FnI)(void*, void*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);
typedef void*(__fastcall* FnAt)(void*, void*, unsigned);
typedef void(__fastcall* FnLoad)(void*, void*, void*, const char*, int);
typedef void(__fastcall* FnRead)(void*, void*, void*, void*);
typedef void(__fastcall* FnEmitterRead)(void*, void*, void*, void*, const char*, const char*);
typedef void(__fastcall* FnCreate)(void*, void*, unsigned, const AtAudioBankDesc*);
typedef void(__fastcall* FnEmitterInit)(void*, void*, const AtAudioEmitterDesc*);
typedef void(__fastcall* FnV)(void*, void*, const AtAudioBankDesc*);
typedef void(__cdecl* FnSet)(void*);

static void set_list_heaps() {
    *(void**)(uintptr_t)0x002C627C = AtAudioDatabase::s_heap = &g_heap_database;
    *(void**)(uintptr_t)0x002C6338 = AtAudioBankDatabase::s_heap = &g_heap_bank;
    *(void**)(uintptr_t)0x002C6334 = AtAudioSegmentDatabase::s_heap = &g_heap_segment;
    *(void**)(uintptr_t)0x002C6330 = AtAudioEmitterDatabase::s_heap = &g_heap_emitter;
}

struct Env {
    Env() {
        const bool was = g_recording;
        g_recording = false;
        for (int side = 0; side < 2; side++) {
            g_own[side] = new AtNameTableOwner;
            AtNameTableOwner& w = *g_own[side];
            g_system[side] = new TestSystem;
            g_system[side]->side = side;
            g_system[side]->m_directSound = (void*)(uintptr_t)0xD500;
            w.m_loader = g_system[side];
            g_matdev[side].side = side;
            w.m_unknown08 = &g_matdev[side];
            w.m_firstSpaces.Init(8);
            w.m_secondSpaces.Init(8);
            w.m_nodeSpaces.Init(8);
            g_mat_used[side] = 0;
            g_stream_next[side] = 0;
            g_bank_ids[side].clear();
        }
        *(void**)(uintptr_t)0x002C6280 = *(void**)(uintptr_t)0x002C6284 = *(void**)(uintptr_t)0x002C6288 = *(void**)(uintptr_t)0x002C628C = nullptr;
        AtAudioDatabase::s_bankNames = AtAudioDatabase::s_segmentNames = AtAudioDatabase::s_emitterNames = AtAudioDatabase::s_nestedNames = nullptr;
        g_fs->m_path.m_top = -1;
        g_fill = 0xCD;
        g_load_status = AtIoFile::kPending;
        g_create_status = g_mixbin_status = 0;
        g_recording = was;
    }
    ~Env() {
        g_recording = false;
        // What a run leaves behind is left: a group's maps may be gone or half gone, and these are only tables.
        g_fs->m_path.m_top = -1;
        g_fill = 0;
    }
};

static void* raw(size_t n) {
    void* p = std::malloc(n);
    std::memset(p, 0xCD, n);
    return p;
}

static AtSoXboxDatabase* make_o() {
    void* o = raw(sizeof(AtSoXboxDatabase));
    g_recording = false;
    at<Fn0>(0x0010E870)(o, nullptr);
    return (AtSoXboxDatabase*)o;
}
static AtSoXboxDatabase* make_r() {
    void* r = raw(sizeof(AtSoXboxDatabase));
    g_recording = false;
    return new (r) AtSoXboxDatabase;
}

// Everything observable after a step. The owner's maps stay pointed at the lists, so a Clear comes after this, not before.
static void after(const char* what, const AtSoXboxDatabase* o, const AtSoXboxDatabase* r) {
    g_recording = false;
    same_db(o, r);
    same_statics(what);
    compare_logs(what);
}

static bool load_both(AtSoXboxDatabase* o, AtSoXboxDatabase* r, const char* name, int third, const char* what) {
    g_fs->m_path.m_top = -1;
    const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00018AC0)(o, nullptr, g_own[0], name, third); });
    g_recording = false;
    const int otop = g_fs->m_path.m_top;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], name, third); });
    g_recording = false;
    eq("directory stack afterwards", otop, g_fs->m_path.m_top);
    eq("crashed", oc, rc);
    g_fs->m_path.m_top = -1;
    if (oc || rc) {
        compare_logs(what);
        return false;
    }
    after(what, o, r);
    return true;
}

static bool clear_both(AtSoXboxDatabase* o, AtSoXboxDatabase* r, const char* what) {
    const bool oc = guarded([&] { On on(0); (*(*(Fn0**)o + 5))(o, nullptr); });
    const bool rc = guarded([&] { On on(1); r->Clear(); });
    g_recording = false;
    eq("crashed clearing", oc, rc);
    if (oc || rc) {
        compare_logs(what);
        return false;
    }
    g_bank_ids[0].clear();
    g_bank_ids[1].clear();
    after(what, o, r);
    return true;
}

static void destroy_both(AtSoXboxDatabase* o, AtSoXboxDatabase* r, const char* what) {
    void* ov = nullptr;
    const bool oc = guarded([&] { On on(0); ov = at<FnDel>(0x0010EC20)(o, nullptr, 0); });
    const bool rc = guarded([&] { On on(1); r->~AtSoXboxDatabase(); });
    g_recording = false;
    eq("crashed destroying", oc, rc);
    if (!oc) eq("returns this", (uintptr_t)ov, (uintptr_t)o);
    compare_logs(what);
}

// Load, load over itself, clear, clear, load and destroy.
static void run_database(const char* name, const std::string& context, int load_status = AtIoFile::kPending) {
    g_context = context;
    Env env;
    g_load_status = load_status;
    AtSoXboxDatabase* o = make_o();
    AtSoXboxDatabase* r = make_r();
    eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtSoXboxDatabase) - 4), 0);
    if (!load_both(o, r, name, 77, "loaded")) return;
    g_aods_seen++;
    g_bank_ids[0].clear();
    g_bank_ids[1].clear();
    if (!load_both(o, r, name, 78, "loaded over itself")) return;
    if (!clear_both(o, r, "cleared")) return;
    if (!clear_both(o, r, "cleared again")) return;
    if (!load_both(o, r, name, 0, "loaded again")) return;
    destroy_both(o, r, "destroyed");
    std::free(o);
    std::free(r);
}

// ---- written text -------------------------------------------------------------------------------------------------------------------
static std::map<std::string, std::string> g_written;

static void set_written() {
    g_written.clear();
    g_written["W\\A.AOB"] = "Bank[2]\n{\n Bank \"One\"\n {\n PlatformSource\n }\n Bank \"Two\"\n {\n PlatformSource DelayLoad\n }\n}\n";
    g_written["W\\B.AOB"] = "Bank[3]\n{\n Bank \"Disk\"\n {\n PlatformSource DiskResident\n }\n Bank \"Sba\"\n {\n SbaFormat\n }\n Bank \"One\"\n {\n PlatformSource BlockID 9\n }\n}\n";
    g_written["W\\S.AOB"] = "NameSpace \"Sp\"\nBank[1]\n{\n Bank \"InSpace\"\n {\n PlatformSource\n }\n}\n";
    g_written["W\\A.AOS"] = "Segment[2]\n{\n Segment \"S1\"\n {\n Bank \"One\" \"\" 3\n }\n Segment \"S2\"\n {\n Bank \"Two\" \"\" 4\n Looping\n }\n}\n";
    g_written["W\\S.AOS"] = "NameSpace \"Sp\"\nSegment[1]\n{\n Segment \"S3\"\n {\n Bank \"InSpace\" \"Sp\" 5\n }\n}\n";
    g_written["W\\E.AOE"] =
        "Emitter[5]\n{\n Emitter \"plain\"\n {\n }\n Emitter \"three\"\n {\n Emitter3D\n Position 1.5 -2.25 3.0\n }\n Emitter \"center\"\n {\n Center\n }\n Emitter \"stereo\"\n {\n Stereo\n Orientation 0.5 0.25 -0.75 0.125\n }\n Emitter \"all\"\n {\n Center Stereo Emitter3D Position 7.5 8.5 9.5 Orientation 1.5 2.5 3.5 4.5\n }\n}\n";
    g_written["W\\SP.AOE"] = "NameSpace \"Sp\"\nEmitter[1]\n{\n Emitter \"ns\"\n {\n Emitter3D\n }\n}\n";
    g_written["W\\EMPTY.AOE"] = "NameSpace \"\"\nEmitter[1]\n{\n Emitter \"ns\"\n {\n }\n}\n";
    g_written["W\\LONG.AOE"] = "NameSpace \"LongerThanEight\"\nEmitter[2]\n{\n Emitter \"LongerThanEight\"\n {\n }\n Emitter \"LongerThanNine\"\n {\n }\n}\n";
    g_written["W\\ZERO.AOE"] = "Emitter[0]\n{\n}\n";
    g_written["W\\NONE.AOE"] = "// nothing but a comment\n";
    g_written["W\\WORD.AOE"] = "5\nCenter\nEmitter[1]\n{\n Emitter \"x\"\n {\n 5 \"s\" NameSpace Emitter\n }\n}\n";
    g_written["W\\WRONG.AOE"] = "Emitter[2]\n{\n Center \"x\"\n {\n }\n NameSpace \"y\"\n {\n }\n}\n";
    g_written["W\\TWICE.AOE"] = "Emitter[1]\n{\n Emitter \"a\"\n {\n }\n}\nEmitter[1]\n{\n Emitter \"ignored\"\n {\n }\n}\n";
    g_written["W\\DUP.AOE"] = "Emitter[2]\n{\n Emitter \"d\"\n {\n }\n Emitter \"d\"\n {\n Emitter3D\n }\n}\n";
    g_written["W\\REAL.AOE"] = "Emitter[1]\n{\n Emitter \"r\"\n {\n Position \"x\" 1 2\n Orientation 1 2\n Center\n }\n}\n";
    g_written["W\\N1.AOD"] = "BankDatabase[1]\n{\n \"S\"\n}\nSegmentDatabase[1]\n{\n \"S\"\n}\n";
    g_written["W\\N2.AOD"] = "EmitterDatabase[1]\n{\n \"E\"\n}\n";
    g_written["W\\SUB\\C.AOB"] = "Bank[1]\n{\n Bank \"Sub\"\n {\n PlatformSource\n }\n}\n";
    g_written["W\\SUB\\N3.AOD"] = "BankDatabase[1]\n{\n \"C\"\n}\n";
}

static const char* const kDatabaseTexts[] = {
    "",
    "// nothing but a comment\n",
    "BankDatabase[1]\n{\n \"A\"\n}\n",
    "BankDatabase[2]\n{\n \"A\"\n \"B\"\n}\n",
    "BankDatabase[2]\n{\n \"A\"\n \"A\"\n}\n",
    "BankDatabase[0]\n{\n}\n",
    "BankDatabase[1]\n{\n \"S\"\n}\nSegmentDatabase[1]\n{\n \"S\"\n}\n",
    "BankDatabase[1]\n{\n \"A\"\n}\nSegmentDatabase[1]\n{\n \"A\"\n}\n",
    "SegmentDatabase[1]\n{\n \"A\"\n}\nBankDatabase[1]\n{\n \"A\"\n}\n",
    "SegmentDatabase[1]\n{\n \"A\"\n}\n",
    "SegmentDatabase[0]\n{\n}\n",
    "EmitterDatabase[1]\n{\n \"E\"\n}\n",
    "EmitterDatabase[2]\n{\n \"E\"\n \"SP\"\n}\n",
    "EmitterDatabase[0]\n{\n}\n",
    "EmitterDatabase[1]\n{\n \"EMPTY\"\n}\n",
    "EmitterDatabase[1]\n{\n \"LONG\"\n}\n",
    "EmitterDatabase[1]\n{\n \"ZERO\"\n}\n",
    "EmitterDatabase[1]\n{\n \"NONE\"\n}\n",
    "EmitterDatabase[1]\n{\n \"WORD\"\n}\n",
    "EmitterDatabase[1]\n{\n \"WRONG\"\n}\n",
    "EmitterDatabase[1]\n{\n \"TWICE\"\n}\n",
    "EmitterDatabase[1]\n{\n \"DUP\"\n}\n",
    "EmitterDatabase[1]\n{\n \"REAL\"\n}\n",
    "BankDatabase[1]\n{\n \"A\"\n}\nSegmentDatabase[1]\n{\n \"A\"\n}\nEmitterDatabase[1]\n{\n \"E\"\n}\n",
    "DatabaseLoader[0]\n{\n}\n",
    "DatabaseLoader[1]\n{\n \"N1\"\n}\n",
    "DatabaseLoader[1]\n{\n \"N2\"\n}\n",
    "DatabaseLoader[2]\n{\n \"N1\"\n \"N2\"\n}\n",
    "BankDatabase[1]\n{\n \"A\"\n}\nDatabaseLoader[1]\n{\n \"N2\"\n}\n",
    "DatabaseLoader[1]\n{\n \"SUB\\\\N3\"\n}\n",
    "DatabaseLoader[1]\n{\n \"SUB\\N3\"\n}\n",
    "BankDatabase[1]\n{\n \"SUB\\C\"\n}\n",
    "BankDatabase[1]\n{\n \"A\"\n}\nBankDatabase[1]\n{\n \"B\"\n}\n",
    "SegmentDatabase[1]\n{\n \"A\"\n}\nSegmentDatabase[1]\n{\n \"S\"\n}\n",
    "EmitterDatabase[1]\n{\n \"E\"\n}\nEmitterDatabase[1]\n{\n \"SP\"\n}\n",
    "DatabaseLoader[1]\n{\n \"N1\"\n}\nDatabaseLoader[1]\n{\n \"N2\"\n}\n",
    "BankDatabase[0]\n{\n}\nBankDatabase[1]\n{\n \"A\"\n}\n",
    "BankDatabase[1]\n{\n \"Gone\"\n}\n",
    "SegmentDatabase[1]\n{\n \"Gone\"\n}\n",
    "EmitterDatabase[1]\n{\n \"Gone\"\n}\n",
    "DatabaseLoader[1]\n{\n \"Gone\"\n}\n",
    "5\n",
    "\"text\"\n",
    "Other[1]\n{\n}\n",
    "Bank[1]\n{\n \"A\"\n}\n",
    "BankDatabase\n{\n \"A\"\n}\n",
    "BankDatabase[1\n{\n \"A\"\n}\n",
    "BankDatabase[1]\n{\n 5\n}\n",
    "BankDatabase[2]\n{\n \"A\"\n}\n",
    "BankDatabase[1]\n{\n \"A\"\n \"B\"\n}\n",
    "BankDatabase[1]\n{\n \"A\"\n",
    "BankDatabase[1]\n{\n \"A\"\n}\n5\nSegmentDatabase[1]\n{\n \"A\"\n}\n",
};

static void run_written() {
    set_written();
    g_fs->extra = &g_written;
    char tag[32];
    int n = 0;
    for (const char* text : kDatabaseTexts) {
        g_written["W\\MAIN.AOD"] = text;
        sprintf_s(tag, "written %d: ", n++);
        for (int status : {(int)AtIoFile::kPending, 0}) {
            run_database("W\\MAIN", std::string(tag) + text, status);
        }
    }
    // The file itself is not there.
    g_written.erase("W\\MAIN.AOD");
    run_database("W\\MAIN", "no such .AOD");
    set_written();
    // The SDK refusing.
    for (int create : {0, -1}) {
        for (int mix : {0, -1}) {
            g_written["W\\MAIN.AOD"] = "EmitterDatabase[1]\n{\n \"E\"\n}\n";
            g_context = "SDK refusing";
            Env env;
            g_create_status = create;
            g_mixbin_status = mix;
            AtSoXboxDatabase* o = make_o();
            AtSoXboxDatabase* r = make_r();
            if (load_both(o, r, "W\\MAIN", 1, "refused") && clear_both(o, r, "refused, cleared")) destroy_both(o, r, "refused, destroyed");
            std::free(o);
            std::free(r);
        }
    }
    g_fs->extra = nullptr;
}

// Each .AOE alone, on a list of its own, and read again over itself.
static void run_emitter_lists() {
    set_written();
    g_fs->extra = &g_written;
    for (const char* name : {"W\\E", "W\\SP", "W\\EMPTY", "W\\LONG", "W\\ZERO", "W\\NONE", "W\\WORD", "W\\WRONG", "W\\TWICE", "W\\DUP", "W\\REAL", "W\\GONE"}) {
        g_context = std::string("emitter list ") + name;
        Env env;
        void* mo = raw(sizeof(AtSoXboxEmitterDatabase));
        void* mr = raw(sizeof(AtSoXboxEmitterDatabase));
        at<Fn0>(0x0010F320)(mo, nullptr);
        AtSoXboxEmitterDatabase* o = (AtSoXboxEmitterDatabase*)mo;
        AtSoXboxEmitterDatabase* r = new (mr) AtSoXboxEmitterDatabase;
        eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtSoXboxEmitterDatabase) - 4), 0);
        for (int pass = 0; pass < 2; pass++) {
            const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00025450)(o, nullptr, g_own[0], name, 9); });
            const bool rc = guarded([&] { On on(1); r->Load(g_own[1], name, 9); });
            g_recording = false;
            eq("crashed", oc, rc);
            if (oc || rc) break;
            same_emitter_db(o, r);
            eqs("third spaces", group_name(g_own[0]->m_nodeSpaces.m_last).c_str(), group_name(g_own[1]->m_nodeSpaces.m_last).c_str());
            eq("third table has it", g_own[0]->m_nodes.m_first == o, g_own[1]->m_nodes.m_first == r);
            compare_logs("emitter list");
        }
        // Looking an emitter up, as the game does.
        const char* look = "plain";
        if (g_own[0]->m_nodes.m_first && g_own[1]->m_nodes.m_first && o->m_entries && r->m_entries) {
            void* fo = g_own[0]->m_nodes.Find(look);
            void* fr = g_own[1]->m_nodes.Find(look);
            eq("found", fo != nullptr, fr != nullptr);
            if (fo && fr) eq("found at", (char*)fo - (char*)o->m_emitters, (char*)fr - (char*)r->m_emitters);
        }
        if (g_own[0]->m_nodes.m_first == o) {
            { On on(0); at<Fn0>(0x0010F170)(o, nullptr); }
            { On on(1); r->Clear(); }
            same_emitter_db(o, r);
            compare_logs("emitter list cleared");
        }
    }
    g_fs->extra = nullptr;
}

static void run_segment_lists() {
    set_written();
    g_fs->extra = &g_written;
    for (const char* name : {"W\\A", "W\\S"}) {
        g_context = std::string("segment list ") + name;
        Env env;
        void* mo = raw(sizeof(AtSoXboxSegmentDatabase));
        void* mr = raw(sizeof(AtSoXboxSegmentDatabase));
        at<Fn0>(0x0010EF50)(mo, nullptr);
        AtSoXboxSegmentDatabase* o = (AtSoXboxSegmentDatabase*)mo;
        AtSoXboxSegmentDatabase* r = new (mr) AtSoXboxSegmentDatabase;
        eq("constructed", std::memcmp((char*)o + 4, (char*)r + 4, sizeof(AtSoXboxSegmentDatabase) - 4), 0);
        for (int pass = 0; pass < 2; pass++) {
            { On on(0); at<FnLoad>(0x00025640)(o, nullptr, g_own[0], name, 9); }
            { On on(1); r->Load(g_own[1], name, 9); }
            same_segment_db(o, r);
            compare_logs("segment list");
        }
        { On on(0); at<Fn0>(0x0010EED0)(o, nullptr); }
        { On on(1); r->Clear(); }
        same_segment_db(o, r);
        compare_logs("segment list cleared");
    }
    g_fs->extra = nullptr;
}

// Error returns, as the file's does here. Text left open inside an emitter would go round for ever, on both sides, so this one stops it with a crash that guarded catches.
class MemoryStream : public AtIoStream {
public:
    int Refill() override { return 0; }
};
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
    explicit Direct(const char* t) : text(t) {
        stream.m_base = (char*)text.data();
        stream.m_cur = stream.m_base;
        stream.m_limit = stream.m_base + text.size();
        stream.m_size = (unsigned)text.size();
        tokenizer.SetStream(&stream);
    }
    ~Direct() {
        stream.m_base = stream.m_cur = stream.m_limit = nullptr;
        tokenizer.SetStream(nullptr);
    }
};

static void run_open_texts() {
    static const char* const texts[] = {
        "Emitter[1]\n{\n Emitter \"A\"\n {\n",
        "Emitter[1]\n{\n Emitter \"A\"\n",
        "Emitter[2]\n{\n Emitter \"A\"\n {\n }\n",
        "Emitter[1]\n{\n",
        "Emitter[1]\n",
        "Emitter\n",
    };
    for (const char* text : texts) {
        g_context = std::string("open: ") + text;
        Env env;
        void* mo = raw(sizeof(AtSoXboxEmitterDatabase));
        void* mr = raw(sizeof(AtSoXboxEmitterDatabase));
        at<Fn0>(0x0010F320)(mo, nullptr);
        AtSoXboxEmitterDatabase* r = new (mr) AtSoXboxEmitterDatabase;
        Direct da(text);
        Direct db(text);
        const bool oc = guarded([&] { On on(0); at<FnRead>(0x00025360)(mo, nullptr, g_own[0], &da.tokenizer); });
        const bool rc = guarded([&] { On on(1); r->Read(g_own[1], &db.tokenizer); });
        g_recording = false;
        eq("crashed", oc, rc);
        compare_logs("open text");
    }
}

// A database whose bank slot then takes away the memory or the heap, for what follows it in Load.
static int g_fail_mode;
static void arm_failure() {
    if (g_fail_mode == 1) g_fail_alloc = true;
    if (g_fail_mode == 2) {
        *(void**)(uintptr_t)0x002C60B0 = nullptr;
        AtIoTokenFile::s_heap = nullptr;
    }
}
static void __fastcall o_slot0_failing(void*, void*) {
    g_log[0].add('a');
    arm_failure();
}
class FailingDb : public AtSoXboxDatabase {
public:
    void AllocateBankDatabases() override {
        g_log[1].add('a');
        arm_failure();
    }
};

static void run_counts_without_owner() {
    g_context = "counts without an owner";
    set_written();
    g_fs->extra = &g_written;
    g_written["W\\MAIN.AOD"] = "";
    Env env;
    AtSoXboxDatabase* o = make_o();
    AtSoXboxDatabase* r = make_r();
    o->m_bankCount = r->m_bankCount = 1;
    const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00018AC0)(o, nullptr, g_own[0], "W\\MAIN", 1); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    const bool rc = guarded([&] { On on(1); r->Load(g_own[1], "W\\MAIN", 1); });
    g_recording = false;
    g_fs->m_path.m_top = -1;
    eq("crashed", oc, rc);
    eq("crashed, as it should", oc, true);
    compare_logs("counts without an owner");
    g_fs->extra = nullptr;
}

// Loading two nested databases from a file crashes, in both (the first frees the static list of names under the second), so to see them cleared in order they are loaded one at a time.
static void run_two_nested() {
    g_context = "two nested";
    set_written();
    g_fs->extra = &g_written;
    Env env;
    AtSoXboxDatabase* o = make_o();
    AtSoXboxDatabase* r = make_r();
    o->m_nestedCount = r->m_nestedCount = 2;
    o->m_owner = g_own[0];
    r->m_owner = g_own[1];
    { On on(0); at<Fn0>(0x0010EB60)(o, nullptr); }
    { On on(1); r->AllocateNestedDatabases(); }
    compare_logs("two nested made");
    const char* names[2] = {"W\\N1", "W\\N2"};
    for (int i = 0; i < 2; i++) {
        g_fs->m_path.m_top = -1;
        { On on(0); at<FnLoad>(0x00018AC0)(&o->m_nestedDatabases[i], nullptr, g_own[0], names[i], 1); }
        g_recording = false;
        g_fs->m_path.m_top = -1;
        { On on(1); r->m_nestedDatabases[i].Load(g_own[1], names[i], 1); }
        g_recording = false;
        compare_logs("nested loaded");
    }
    g_fs->m_path.m_top = -1;
    { On on(0); at<Fn0>(0x0010E890)(o, nullptr); }
    { On on(1); r->Clear(); }
    g_bank_ids[0].clear();
    g_bank_ids[1].clear();
    after("two nested cleared", o, r);
    g_fs->extra = nullptr;
}

// Without their heap or their memory.
static void run_memory() {
    g_context = "memory";
    set_written();
    g_fs->extra = &g_written;

    // The list of names cannot be had. With a count of one the original writes through null.
    for (int mode : {1, 2})
    for (const char* text : {"BankDatabase[1]\n{\n \"A\"\n}\n", "BankDatabase[0]\n{\n}\n"}) {
        g_written["W\\MAIN.AOD"] = text;
        Env env;
        g_context = std::string("names: ") + text;
        const void* table[10];
        void* mo = raw(sizeof(AtSoXboxDatabase));
        at<Fn0>(0x0010E870)(mo, nullptr);
        std::memcpy(table, *(const void***)mo, sizeof table);
        table[0] = (const void*)o_slot0_failing;
        *(const void***)mo = table;
        void* mr = raw(sizeof(FailingDb));
        FailingDb* r = new (mr) FailingDb;
        g_fail_mode = mode;
        const bool oc = guarded([&] { On on(0); at<FnLoad>(0x00018AC0)(mo, nullptr, g_own[0], "W\\MAIN", 1); });
        g_fail_alloc = false;
        g_recording = false;
        g_fs->m_path.m_top = -1;
        *(void**)(uintptr_t)0x002C60B0 = &g_heap_token;
        AtIoTokenFile::s_heap = &g_heap_token;
        const bool rc = guarded([&] { On on(1); r->Load(g_own[1], "W\\MAIN", 1); });
        g_fail_alloc = false;
        g_recording = false;
        g_fs->m_path.m_top = -1;
        *(void**)(uintptr_t)0x002C60B0 = &g_heap_token;
        AtIoTokenFile::s_heap = &g_heap_token;
        g_fail_mode = 0;
        eq("crashed", oc, rc);
        compare_logs("names without memory");
    }

    // Each slot that makes an array, then a bank, without memory and without a heap.
    static const uint32_t kHeaps[] = {0x002C627C, 0x002C6338, 0x002C6334, 0x002C6330};
    void** recovered_heap[] = {&AtAudioDatabase::s_heap, &AtAudioBankDatabase::s_heap, &AtAudioSegmentDatabase::s_heap, &AtAudioEmitterDatabase::s_heap};
    for (int fail = 0; fail < 3; fail++) {
        g_context = fail == 0 ? "heaps" : fail == 1 ? "no memory" : "no heaps";
        Env env;
        g_fail_alloc = fail == 1;
        if (fail == 2) {
            for (uint32_t va : kHeaps) *(void**)(uintptr_t)va = nullptr;
            for (void** h : recovered_heap) *h = nullptr;
        }
        AtSoXboxDatabase* o = make_o();
        AtSoXboxDatabase* r = make_r();
        for (AtSoXboxDatabase* d : {o, r}) {
            d->m_owner = nullptr;
            d->m_bankCount = d->m_segmentCount = d->m_emitterCount = d->m_nestedCount = 1;
        }
        // The slots, by position.
        for (int slot = 0; slot < 4; slot++) {
            const bool oc = guarded([&] { On on(0); (*(*(Fn0**)o + slot))(o, nullptr); });
            const bool rc = guarded([&] {
                On on(1);
                if (slot == 0) r->AllocateBankDatabases();
                else if (slot == 1) r->AllocateSegmentDatabases();
                else if (slot == 2) r->AllocateEmitterDatabases();
                else r->AllocateNestedDatabases();
            });
            g_recording = false;
            eq("slot crashed", oc, rc);
            eq("slot's array", *((void**)o + 6 + slot) != nullptr, *((void**)r + 6 + slot) != nullptr);
            compare_logs("a database's array");
        }
        // The lists' own arrays.
        void* eo = raw(sizeof(AtSoXboxEmitterDatabase));
        void* er = raw(sizeof(AtSoXboxEmitterDatabase));
        void* so = raw(sizeof(AtSoXboxSegmentDatabase));
        void* sr = raw(sizeof(AtSoXboxSegmentDatabase));
        void* bo = raw(sizeof(AtSoXboxBankDatabase));
        void* br = raw(sizeof(AtSoXboxBankDatabase));
        const bool was = g_fail_alloc;
        g_fail_alloc = false;
        at<Fn0>(0x0010F320)(eo, nullptr);
        at<Fn0>(0x0010EF50)(so, nullptr);
        at<Fn0>(0x000E9F30)(bo, nullptr);
        AtSoXboxEmitterDatabase* er2 = new (er) AtSoXboxEmitterDatabase;
        AtSoXboxSegmentDatabase* sr2 = new (sr) AtSoXboxSegmentDatabase;
        AtSoXboxBankDatabase* br2 = new (br) AtSoXboxBankDatabase;
        g_fail_alloc = was;
        ((AtSoXboxEmitterDatabase*)eo)->m_count = er2->m_count = 2;
        ((AtSoXboxSegmentDatabase*)so)->m_count = sr2->m_count = 2;
        { On on(0); at<Fn0>(0x0010F1E0)(eo, nullptr); at<Fn0>(0x0010EDA0)(so, nullptr); }
        { On on(1); er2->OnReady(); sr2->OnReady(); }
        eq("emitters", ((AtSoXboxEmitterDatabase*)eo)->m_emitters != nullptr, er2->m_emitters != nullptr);
        eq("segments", ((AtSoXboxSegmentDatabase*)so)->m_segments != nullptr, sr2->m_segments != nullptr);
        compare_logs("a list's array");
        // A count of zero makes nothing.
        ((AtSoXboxEmitterDatabase*)eo)->m_count = er2->m_count = 0;
        ((AtSoXboxSegmentDatabase*)so)->m_count = sr2->m_count = 0;
        { On on(0); at<Fn0>(0x0010F1E0)(eo, nullptr); at<Fn0>(0x0010EDA0)(so, nullptr); }
        { On on(1); er2->OnReady(); sr2->OnReady(); }
        compare_logs("a list's array, none");
        // A bank.
        AtAudioBankDesc desc;
        desc.m_name[0] = 'B';
        desc.m_name[1] = 0;
        desc.m_format = 3;
        desc.m_residence = 0;
        void* olist[1] = {(void*)1};
        void* rlist[1] = {(void*)1};
        ((AtSoXboxBankDatabase*)bo)->m_banks = (AtAudioBank**)olist;
        br2->m_banks = (AtAudioBank**)rlist;
        ((AtSoXboxBankDatabase*)bo)->m_owner = g_own[0];
        br2->m_owner = g_own[1];
        AtAudioBankDesc d0 = desc, d1 = desc;
        d0.m_database = (AtAudioBankDatabase*)bo;
        d1.m_database = br2;
        const bool oc = guarded([&] { On on(0); at<FnCreate>(0x0010EC80)(bo, nullptr, 0, &d0); });
        const bool rc = guarded([&] { On on(1); br2->CreateBank(0, d1); });
        g_recording = false;
        eq("bank crashed", oc, rc);
        eq("bank", olist[0] != nullptr, rlist[0] != nullptr);
        compare_logs("a bank");
        g_fail_alloc = false;
        set_list_heaps();
    }
    g_fs->extra = nullptr;
}

// Small things alone.
class BareDb : public AtAudioDatabase {
public:
    void AllocateBankDatabases() override {}
    void AllocateSegmentDatabases() override {}
    void AllocateEmitterDatabases() override {}
    void AllocateNestedDatabases() override {}
    AtAudioBankDatabase* BankDatabaseAt(unsigned) override { return nullptr; }
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned) override { return nullptr; }
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned) override { return nullptr; }
    AtAudioDatabase* NestedDatabaseAt(unsigned) override { return nullptr; }
};
class BareEmitterDb : public AtAudioEmitterDatabase {
public:
    void OnReady() override {}
    AtAudioEmitter* EmitterAt(unsigned) override { return nullptr; }
};
class BareEmitter : public AtAudioEmitter {
public:
    void InitPlatform() override {}
};

static void run_small_objects() {
    g_context = "small";
    Env env;
    // Setters.
    int token = 0;
    at<FnSet>(0x00018A90)(&token);
    AtAudioDatabase::SetHeap(&token);
    eq("database heap", *(void**)(uintptr_t)0x002C627C == &token, AtAudioDatabase::s_heap == &token);
    at<FnSet>(0x00018A90)(&g_heap_database);
    AtAudioDatabase::SetHeap(&g_heap_database);
    at<FnSet>(0x000252F0)(&token);
    AtAudioEmitterDatabase::SetHeap(&token);
    eq("emitter heap", *(void**)(uintptr_t)0x002C6330 == &token, AtAudioEmitterDatabase::s_heap == &token);
    at<FnSet>(0x000252F0)(&g_heap_emitter);
    AtAudioEmitterDatabase::SetHeap(&g_heap_emitter);

    // The tables' words, as text and as ids.
    for (int i = 0; i < AtAudioDatabase::kKeywordCount; i++) {
        const AtIoKeyword& k = ((const AtIoKeyword*)(uintptr_t)0x002B0140)[i];
        eq("database word", k.m_id, AtAudioDatabase::s_keywords[i].m_id);
        eqs("database word", k.m_name, AtAudioDatabase::s_keywords[i].m_name);
    }
    for (int i = 0; i < AtAudioEmitterDatabase::kKeywordCount; i++) {
        const AtIoKeyword& k = ((const AtIoKeyword*)(uintptr_t)0x002B049C)[i];
        eq("emitter word", k.m_id, AtAudioEmitterDatabase::s_keywords[i].m_id);
        eqs("emitter word", k.m_name, AtAudioEmitterDatabase::s_keywords[i].m_name);
    }

    // The base classes' deleting destructors, each flag.
    for (unsigned flags : {0u, 1u}) {
        const size_t n = sizeof(BareDb);
        void* po = raw(n);
        void* pr = raw(n);
        at<Fn0>(0x00018AA0)(po, nullptr);
        BareDb* rb = new (pr) BareDb;
        eq("base constructed", std::memcmp((char*)po + 4, (char*)pr + 4, n - 4), 0);
        for (void* p : {po, pr}) ((AtAudioDatabase*)p)->m_owner = g_own[0];
        for (void* p : {po, pr}) ((AtAudioDatabase*)p)->m_bankCount = ((AtAudioDatabase*)p)->m_segmentCount = ((AtAudioDatabase*)p)->m_emitterCount = ((AtAudioDatabase*)p)->m_nestedCount = 5;
        { On on(0); (void)at<FnDel>(0x00019150)(po, nullptr, flags); }
        { On on(1); rb->~BareDb(); if (flags & 1) g_log[1].add('D'); }
        if (flags & 1) { compare_logs("base deleted"); }
        else { eq("base destroyed", std::memcmp((char*)po + 4, (char*)pr + 4, n - 4), 0); compare_logs("base destroyed"); std::free(po); }
        std::free(pr);
    }
    // Clear, on the base.
    {
        void* po = raw(sizeof(BareDb));
        void* pr = raw(sizeof(BareDb));
        at<Fn0>(0x00018AA0)(po, nullptr);
        BareDb* rb = new (pr) BareDb;
        for (void* p : {po, pr}) { ((AtAudioDatabase*)p)->m_owner = g_own[0]; ((AtAudioDatabase*)p)->m_bankCount = ((AtAudioDatabase*)p)->m_segmentCount = ((AtAudioDatabase*)p)->m_emitterCount = ((AtAudioDatabase*)p)->m_nestedCount = 5; }
        at<Fn0>(0x00019110)(po, nullptr);
        rb->AtAudioDatabase::Clear();
        eq("base cleared", std::memcmp((char*)po + 4, (char*)pr + 4, sizeof(BareDb) - 4), 0);
        std::free(po);
        std::free(pr);
    }
    // The emitter list's base.
    for (unsigned flags : {0u, 1u}) {
        void* po = raw(sizeof(BareEmitterDb));
        void* pr = raw(sizeof(BareEmitterDb));
        at<Fn0>(0x00025320)(po, nullptr);
        BareEmitterDb* rb = new (pr) BareEmitterDb;
        eq("emitter list base", std::memcmp((char*)po + 4, (char*)pr + 4, sizeof(BareEmitterDb) - 4), 0);
        { On on(0); (void)at<FnDel>(0x00025340)(po, nullptr, flags); }
        { On on(1); rb->~BareEmitterDb(); if (flags & 1) g_log[1].add('D'); }
        compare_logs("emitter list base deleted");
        if (!(flags & 1)) std::free(po);
        std::free(pr);
    }
    // An emitter alone, base and Xbox, and its deleting destructors.
    for (unsigned flags : {0u, 1u}) {
        void* po = raw(sizeof(AtSoXboxEmitter));
        void* pr = raw(sizeof(AtSoXboxEmitter));
        at<Fn0>(0x0011F1B0)(po, nullptr);
        BareEmitter* rb = new (pr) BareEmitter;
        eq("emitter base", std::memcmp((char*)po + 4, (char*)pr + 4, 0x48 - 4), 0);
        { On on(0); (void)at<FnDel>(0x0011EC50)(po, nullptr, flags); }
        { On on(1); rb->~BareEmitter(); if (flags & 1) g_log[1].add('D'); }
        compare_logs("emitter base deleted");
        if (!(flags & 1)) std::free(po);
        std::free(pr);

        void* xo = raw(sizeof(AtSoXboxEmitter));
        void* xr = raw(sizeof(AtSoXboxEmitter));
        at<Fn0>(0x0010EF80)(xo, nullptr);
        AtSoXboxEmitter* xb = new (xr) AtSoXboxEmitter;
        eq("emitter stream", ((AtSoXboxEmitter*)xo)->m_stream != nullptr, xb->m_stream != nullptr);
        eq("emitter +54", ((AtSoXboxEmitter*)xo)->m_unknown54, xb->m_unknown54);
        { On on(0); (void)at<FnDel>(0x0010F2B0)(xo, nullptr, flags); }
        { On on(1); if (flags & 1) delete xb; else xb->~AtSoXboxEmitter(); }
        compare_logs("emitter deleted");
        if (!(flags & 1)) std::free(xo);
        if (!(flags & 1)) std::free(xr);

        void* so = raw(sizeof(AtSoXboxSegment));
        void* sr = raw(sizeof(AtSoXboxSegment));
        at<Fn0>(0x00120650)(so, nullptr);
        AtSoXboxSegment* sb = new (sr) AtSoXboxSegment;
        eq("segment", std::memcmp((char*)so + 4, (char*)sr + 4, sizeof(AtSoXboxSegment) - 4), 0);
        ((AtSoXboxSegment*)so)->m_index = sb->m_index = 3;
        at<Fn0>(0x00120680)(so, nullptr);
        sb->Reset();
        eq("segment reset", std::memcmp((char*)so + 4, (char*)sr + 4, sizeof(AtSoXboxSegment) - 4), 0);
        at<Fn0>(0x001CC9F0)(so, nullptr);
        sb->OnRead();
        eq("segment read", std::memcmp((char*)so + 4, (char*)sr + 4, sizeof(AtSoXboxSegment) - 4), 0);
        { On on(0); (void)at<FnDel>(0x0010EE70)(so, nullptr, flags); }
        { On on(1); if (flags & 1) delete sb; else sb->~AtSoXboxSegment(); }
        compare_logs("segment deleted");
        if (!(flags & 1)) std::free(so);
        if (!(flags & 1)) std::free(sr);

        // Each list's and the database's own deleting destructors, not as an array.
        void* a0 = raw(sizeof(AtSoXboxBankDatabase));
        void* a1 = raw(sizeof(AtSoXboxBankDatabase));
        at<Fn0>(0x000E9F30)(a0, nullptr);
        AtSoXboxBankDatabase* a2 = new (a1) AtSoXboxBankDatabase;
        eq("bank list", ((AtSoXboxBankDatabase*)a0)->m_banks != nullptr, a2->m_banks != nullptr);
        { On on(0); (void)at<FnDel>(0x0010EAB0)(a0, nullptr, flags); }
        { On on(1); if (flags & 1) delete a2; else a2->~AtSoXboxBankDatabase(); }
        compare_logs("bank list deleted");
        if (!(flags & 1)) std::free(a0);
        if (!(flags & 1)) std::free(a1);
        void* b0 = raw(sizeof(AtSoXboxSegmentDatabase));
        void* b1 = raw(sizeof(AtSoXboxSegmentDatabase));
        at<Fn0>(0x0010EF50)(b0, nullptr);
        AtSoXboxSegmentDatabase* b2 = new (b1) AtSoXboxSegmentDatabase;
        { On on(0); (void)at<FnDel>(0x0010E6A0)(b0, nullptr, flags); }
        { On on(1); if (flags & 1) delete b2; else b2->~AtSoXboxSegmentDatabase(); }
        compare_logs("segment list deleted");
        if (!(flags & 1)) std::free(b0);
        if (!(flags & 1)) std::free(b1);
        void* c0 = raw(sizeof(AtSoXboxEmitterDatabase));
        void* c1 = raw(sizeof(AtSoXboxEmitterDatabase));
        at<Fn0>(0x0010F320)(c0, nullptr);
        AtSoXboxEmitterDatabase* c2 = new (c1) AtSoXboxEmitterDatabase;
        { On on(0); (void)at<FnDel>(0x0010E7C0)(c0, nullptr, flags); }
        { On on(1); if (flags & 1) delete c2; else c2->~AtSoXboxEmitterDatabase(); }
        compare_logs("emitter list deleted");
        if (!(flags & 1)) std::free(c0);
        if (!(flags & 1)) std::free(c1);
        AtSoXboxDatabase* d0 = make_o();
        AtSoXboxDatabase* d1 = make_r();
        { On on(0); (void)at<FnDel>(0x0010EC20)(d0, nullptr, flags); }
        { On on(1); if (flags & 1) delete d1; else d1->~AtSoXboxDatabase(); }
        compare_logs("database deleted");
        if (!(flags & 1)) std::free(d0);
        if (!(flags & 1)) std::free(d1);
    }
    // The bank, and its base, alone.
    for (unsigned flags : {0u, 1u}) {
        void* po = raw(sizeof(AtSoXboxXactBank));
        void* pr = raw(sizeof(AtSoXboxXactBank));
        at<Fn0>(0x00120550)(po, nullptr);
        AtSoXboxXactBank* rb = new (pr) AtSoXboxXactBank;
        same_bank((AtSoXboxXactBank*)po, rb);
        eq("slot 16", at<FnI>(0x0011F370)(po, nullptr), rb->Unknown16());
        ((AtSoXboxXactBank*)po)->m_unknown1F8 = rb->m_unknown1F8 = 21;
        eq("slot 16 set", at<FnI>(0x0011F370)(po, nullptr), rb->Unknown16());
        { On on(0); (void)at<FnDel>(0x0011F380)(po, nullptr, flags); }
        { On on(1); if (flags & 1) delete rb; else rb->~AtSoXboxXactBank(); }
        compare_logs("bank deleted");
        if (!(flags & 1)) std::free(po);
        if (!(flags & 1)) std::free(pr);
        void* qo = raw(sizeof(AtSoBank) + 8);
        at<Fn0>(0x00125070)(qo, nullptr);
        { On on(0); (void)at<FnDel>(0x001250A0)(qo, nullptr, flags); }
        g_log[1].n = g_log[0].n = 0;
        if (!(flags & 1)) std::free(qo);
    }
    // A database's four slots for an index, within and past the end.
    {
        AtSoXboxDatabase* o = make_o();
        AtSoXboxDatabase* r = make_r();
        for (AtSoXboxDatabase* d : {o, r}) {
            d->m_bankCount = 2;
            d->m_bankDatabases = (AtSoXboxBankDatabase*)(uintptr_t)0x10000;
            d->m_segmentDatabases = (AtSoXboxSegmentDatabase*)(uintptr_t)0x20000;
            d->m_emitterDatabases = (AtSoXboxEmitterDatabase*)(uintptr_t)0x30000;
            d->m_nestedDatabases = (AtSoXboxDatabase*)(uintptr_t)0x40000;
        }
        for (unsigned i : {0u, 1u, 2u, 3u}) {
            eq("bank list at", (uintptr_t)at<FnAt>(0x0010E820)(o, nullptr, i), (uintptr_t)r->BankDatabaseAt(i));
            eq("segment list at", (uintptr_t)at<FnAt>(0x0010E840)(o, nullptr, i), (uintptr_t)r->SegmentDatabaseAt(i));
            eq("emitter list at", (uintptr_t)at<FnAt>(0x0010E850)(o, nullptr, i), (uintptr_t)r->EmitterDatabaseAt(i));
            eq("nested at", (uintptr_t)at<FnAt>(0x0010E860)(o, nullptr, i), (uintptr_t)r->NestedDatabaseAt(i));
        }
        std::free(o);
        std::free(r);
    }
    // Each list's slot for an index, and the unused slots of the list classes.
    {
        void* eo = raw(sizeof(AtSoXboxEmitterDatabase));
        void* er = raw(sizeof(AtSoXboxEmitterDatabase));
        void* so = raw(sizeof(AtSoXboxSegmentDatabase));
        void* sr = raw(sizeof(AtSoXboxSegmentDatabase));
        at<Fn0>(0x0010F320)(eo, nullptr);
        at<Fn0>(0x0010EF50)(so, nullptr);
        AtSoXboxEmitterDatabase* e2 = new (er) AtSoXboxEmitterDatabase;
        AtSoXboxSegmentDatabase* s2 = new (sr) AtSoXboxSegmentDatabase;
        ((AtSoXboxEmitterDatabase*)eo)->m_emitters = e2->m_emitters = (AtSoXboxEmitter*)(uintptr_t)0x10000;
        ((AtSoXboxSegmentDatabase*)so)->m_segments = s2->m_segments = (AtSoXboxSegment*)(uintptr_t)0x10000;
        for (unsigned i : {0u, 1u, 5u}) {
            eq("emitter at", (uintptr_t)at<FnAt>(0x0010F310)(eo, nullptr, i), (uintptr_t)e2->EmitterAt(i));
            eq("segment at", (uintptr_t)at<FnAt>(0x0010EF40)(so, nullptr, i), (uintptr_t)s2->SegmentAt(i));
        }
    }
    compare_logs("small");
}

// An emitter and a bank with everything already set, which a list never gives: what Init, Reset and the bank's Reset clear can only be seen here.
static void run_emitter_alone() {
    g_context = "emitter alone";
    set_written();
    for (int shape = 0; shape < 6; shape++) {
        Env env;
        void* eo = raw(sizeof(AtSoXboxEmitterDatabase));
        void* er = raw(sizeof(AtSoXboxEmitterDatabase));
        at<Fn0>(0x0010F320)(eo, nullptr);
        AtSoXboxEmitterDatabase* dr = new (er) AtSoXboxEmitterDatabase;
        AtSoXboxEmitterDatabase* d[2] = {(AtSoXboxEmitterDatabase*)eo, dr};
        d[0]->m_owner = g_own[0];
        d[1]->m_owner = g_own[1];
        d[0]->GmNameMap::Init(4);
        d[1]->GmNameMap::Init(4);
        void* mo = raw(sizeof(AtSoXboxEmitter));
        void* mr = raw(sizeof(AtSoXboxEmitter));
        at<Fn0>(0x0010EF80)(mo, nullptr);
        AtSoXboxEmitter* r = new (mr) AtSoXboxEmitter;
        AtSoXboxEmitter* e[2] = {(AtSoXboxEmitter*)mo, r};
        for (int side = 0; side < 2; side++) {
            AtSoXboxEmitter* x = e[side];
            x->m_owner = (shape & 1) ? g_own[side] : nullptr;
            x->m_kind = 9;
            x->m_unknown30 = 30;
            x->m_unknown34 = 34;
            x->m_unknown38 = 38;
            x->m_unknown3C = 60;
            x->m_firstVoice = (void*)(uintptr_t)4;
            x->m_lastVoice = (void*)(uintptr_t)8;
            x->m_stream = (shape & 2) ? (void*)(uintptr_t)0x88 : nullptr;
            x->m_unknown54 = 54;
            x->m_local = (shape == 3) ? nullptr : (AtMatrix*)&g_mats[side][1];
            x->m_world = (shape == 5) ? nullptr : (AtMatrix*)&g_mats[side][2];
            x->m_unknown10 = 10;
            x->m_unknown1C = 11;
            x->m_unknown20 = 12;
            x->m_dirty = 0x40;
        }
        compare_logs("emitter set up");
        { On on(0); at<Fn0>(0x0010F070)(e[0], nullptr); }
        { On on(1); e[1]->Reset(); }
        same_emitter(*e[0], *e[1]);
        compare_logs("emitter reset");
        // Dirty again, then Init.
        AtAudioEmitterDesc desc[2];
        for (int side = 0; side < 2; side++) {
            AtSoXboxEmitter* x = e[side];
            x->m_owner = g_own[side];
            x->m_kind = 9;
            x->m_unknown30 = 30;
            x->m_unknown34 = 34;
            x->m_unknown38 = 38;
            x->m_unknown3C = 60;
            x->m_firstVoice = (void*)(uintptr_t)4;
            x->m_lastVoice = (void*)(uintptr_t)8;
            x->m_stream = (void*)(uintptr_t)0x88;
            desc[side].m_database = d[side];
            std::memset(desc[side].m_name, 0, 8);
            desc[side].m_name[0] = 'n';
            desc[side].m_kind = shape;
            desc[side].m_position.x = 1.5f;
            desc[side].m_position.y = 2.5f;
            desc[side].m_position.z = 3.5f;
            desc[side].m_rotation.x = 4.5f;
            desc[side].m_rotation.y = 5.5f;
            desc[side].m_rotation.z = 6.5f;
            desc[side].m_rotation.w = 7.5f;
        }
        { On on(0); (*(*(FnEmitterInit**)e[0] + 9))(e[0], nullptr, &desc[0]); }
        { On on(1); e[1]->Init(desc[1]); }
        same_emitter(*e[0], *e[1]);
        same_map(d[0], d[1]);
        compare_logs("emitter initialised");
    }
}

static void run_bank_alone() {
    g_context = "bank alone";
    for (int delay = 0; delay < 2; delay++) {
        for (int status : {(int)AtIoFile::kPending, 0, 3}) {
            Env env;
            g_load_status = status;
            void* po = raw(sizeof(AtSoXboxXactBank));
            void* pr = raw(sizeof(AtSoXboxXactBank));
            at<Fn0>(0x00120550)(po, nullptr);
            AtSoXboxXactBank* r = new (pr) AtSoXboxXactBank;
            AtSoXboxXactBank* b[2] = {(AtSoXboxXactBank*)po, r};
            void* lo = raw(sizeof(AtSoXboxBankDatabase));
            void* lr = raw(sizeof(AtSoXboxBankDatabase));
            at<Fn0>(0x000E9F30)(lo, nullptr);
            AtSoXboxBankDatabase* list = new (lr) AtSoXboxBankDatabase;
            ((AtSoXboxBankDatabase*)lo)->m_owner = g_own[0];
            list->m_owner = g_own[1];
            AtAudioBankDesc d0, d1;
            d0.m_database = (AtAudioBankDatabase*)lo;
            d1.m_database = list;
            d0.m_flags = d1.m_flags = delay ? AtAudioBankDesc::kDelayLoad : AtAudioBankDesc::kStreaming;
            { On on(0); at<FnV>(0x0011F460)(b[0], nullptr, &d0); }
            { On on(1); r->OnInit(d1); }
            same_bank(b[0], b[1]);
            compare_logs("bank's OnInit");
            for (int side = 0; side < 2; side++) {
                AtSoXboxXactBank* x = b[side];
                x->m_users[0] = (AtAudioBankDatabase*)(side == 0 ? lo : lr);
                x->m_userCount = 1;
                x->m_names.m_name[0] = 'x';
                x->m_names.m_unknown10C = x->m_names.m_unknown110 = x->m_names.m_unknown114 = 5;
                x->m_unknown1B0 = x->m_unknown1B4 = x->m_unknown1BC = x->m_unknown1E0 = x->m_unknown1E4 = x->m_unknown1E8 = x->m_unknown1EC = x->m_unknown1F0 = x->m_unknown1F4 = x->m_unknown1F8 = 6;
                x->m_header = (void*)(uintptr_t)4;
                x->m_fileHandle = 7;
                x->m_status = 0;
                x->m_step = 8;
                if (!x->m_block1C0) x->m_block1C0 = std::malloc(4);
                if (!x->m_block1C8) x->m_block1C8 = std::malloc(4);
                if (!x->m_block1CC) x->m_block1CC = std::malloc(4);
                if (!x->m_block1D8) x->m_block1D8 = std::malloc(4);
                if (!x->m_block1DC) x->m_block1DC = std::malloc(4);
            }
            { On on(0); at<Fn0>(0x001200D0)(b[0], nullptr); }
            { On on(1); r->Reset(); }
            same_bank(b[0], b[1]);
            compare_logs("bank's Reset");
        }
    }
    // Not PlatformSource, or with a residence: no bank. Both, with a stale entry.
    Env env;
    void* lo = raw(sizeof(AtSoXboxBankDatabase));
    void* lr = raw(sizeof(AtSoXboxBankDatabase));
    at<Fn0>(0x000E9F30)(lo, nullptr);
    AtSoXboxBankDatabase* list = new (lr) AtSoXboxBankDatabase;
    void* s0[1];
    void* s1[1];
    ((AtSoXboxBankDatabase*)lo)->m_banks = (AtAudioBank**)s0;
    list->m_banks = (AtAudioBank**)s1;
    for (int format = 0; format < 7; format++) {
        for (int residence = 0; residence < 4; residence++) {
            if (format == 3 && residence == 0) continue;
            AtAudioBankDesc d;
            d.m_format = format;
            d.m_residence = residence;
            s0[0] = s1[0] = (void*)(uintptr_t)1;
            { On on(0); at<FnCreate>(0x0010EC80)(lo, nullptr, 0, &d); }
            { On on(1); list->CreateBank(0, d); }
            eq("no bank", (uintptr_t)s0[0], (uintptr_t)s1[0]);
            compare_logs("no bank");
        }
    }
}

// ---- the disc's files ---------------------------------------------------------------------------------------------------------------
static std::string read_case(const std::string& dir, const std::string& file) {
    FILE* f = nullptr;
    if (fopen_s(&f, (dir + "\\" + file).c_str(), "rb") || !f) {
        std::printf("cannot read %s\n", file.c_str());
        std::exit(2);
    }
    std::fseek(f, 0, SEEK_END);
    std::string data((size_t)std::ftell(f), 0);
    std::fseek(f, 0, SEEK_SET);
    if (!data.empty()) std::fread(&data[0], 1, data.size(), f);
    std::fclose(f);
    return data;
}

static unsigned g_files_aod, g_files_aoe, g_files_aob, g_files_aos, g_loose;

static void read_index(const std::string& dir, std::vector<std::string>& aods) {
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) {
        std::printf("cannot read %s\\index.txt: the loose lists are written by tests/oracle/aod_loose_cases.py\n", dir.c_str());
        std::exit(2);
    }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        const char* tab1 = std::strchr(line, '\t');
        const char* tab2 = tab1 ? std::strchr(tab1 + 1, '\t') : nullptr;
        const char* tab3 = tab2 ? std::strchr(tab2 + 1, '\t') : nullptr;
        if (!tab3) continue;
        std::string path(tab2 + 1, tab3 - tab2 - 1);
        if (path.size() < 4) continue;
        for (char& c : path) c = (char)toupper((unsigned char)c);
        const std::string ext = path.substr(path.size() - 4);
        if (ext != ".AOD" && ext != ".AOB" && ext != ".AOS" && ext != ".AOE") continue;
        g_fs->tree[path] = read_case(dir, std::string(line, tab1 - line));
        if (ext == ".AOD") { aods.push_back(path.substr(0, path.size() - 4)); g_files_aod++; }
        if (ext == ".AOE") g_files_aoe++;
        if (ext == ".AOB") g_files_aob++;
        if (ext == ".AOS") g_files_aos++;
    }
    std::fclose(index);
}

static void run_real(const std::string& dir) {
    std::vector<std::string> aods;
    read_index(dir, aods);
    const unsigned before = g_files_aod + g_files_aob + g_files_aos + g_files_aoe;
    read_index(dir + "\\..\\scratch\\aod\\loose", aods);
    g_loose = g_files_aod + g_files_aob + g_files_aos + g_files_aoe - before;
    g_banks_seen = g_segments_seen = g_emitters_seen = g_bank_dbs_seen = g_segment_dbs_seen = g_emitter_dbs_seen = g_aods_seen = g_with_segment_bank = 0;
    g_fs->missing = 0;
    for (const std::string& p : aods) {
        run_database(p.c_str(), p);
    }
    eq("names not found on the disc", g_fs->missing, 0);
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
    if (pos.size() != 2) { std::printf("usage: oracle_aod <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    redirect(0x0011F4B0, (const void*)o_xact_load);
    redirect(0x0011F2A0, (const void*)o_xact_process);
    redirect(0x0011FF80, (const void*)o_xact_update);
    redirect(0x00241DB6, (const void*)o_ds_create);
    redirect(0x002422BA, (const void*)o_ds_mixbins);
    redirect(0x002422BF, (const void*)o_ds_stop);
    redirect(0x002422C4, (const void*)o_ds_status);
    redirect(0x0024228C, (const void*)o_ds_release);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_atMatrix.Identity = m_identity;
    g_atMatrix.SetTranslation = m_translation;
    g_atMatrix.SetRotation = m_rotation;
    *(void**)(uintptr_t)0x002C60CC = (void*)m_identity;
    *(void**)(uintptr_t)0x002C60D4 = (void*)m_translation;
    *(void**)(uintptr_t)0x002C60E0 = (void*)m_rotation;

    g_fs = new TreeFs;
    g_fs->m_path.m_top = -1;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    set_list_heaps();
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_small_objects();
    run_written();
    run_emitter_lists();
    run_open_texts();
    run_counts_without_owner();
    run_two_nested();
    run_memory();
    run_segment_lists();
    run_emitter_alone();
    run_bank_alone();
    run_real(pos[1]);
    report_entries();
    std::printf("%s: %u .AOD, %u .AOB, %u .AOS, %u .AOE files (%u of them loose on the disc); loaded through them, last pass %u lists of banks, %u of segments, %u of emitters, %u banks, %u segments (%u with a bank), %u emitters; %llu comparisons, %d differences\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files_aod, g_files_aob, g_files_aos, g_files_aoe, g_loose, g_bank_dbs_seen, g_segment_dbs_seen, g_emitter_dbs_seen, g_banks_seen, g_segments_seen, g_with_segment_bank, g_emitters_seen, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
