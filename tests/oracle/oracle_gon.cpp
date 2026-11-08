// Differential oracle for the .GON sound notes (Windows x86, local only). See docs/verification.md.
//
//   oracle_gon <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: every .GON file listed in index.txt, loaded through the file code on both sides, registered, searched and freed; text written here for each word and each error; and the banks and the manifest's item
// called directly. Fatal reports do not come back in the game, so in written text the first one stops that side (guarded()). Doubles here stand for three things: the sound lists (real maps of fake segments),
// the emitters (real maps of fake emitters), and the file system. Nothing plays: the notes are compared as records.
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "at/AtAudioDatabase.h"
#include "at/AtIoTokenizer.h"
#include "at/AtNameTableOwner.h"
#include "gm/GmAudioData.h"
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

static MemFs* g_fs;
static AtIoContext g_ctx;
static AtIoFileSystem* g_ctx_stack[4];
static int g_heap_token, g_audio_heap_token;

// ---- what stops a side, and what fails an allocation --------------------------------------------------------------------
static bool g_stop_at_fatal;
static void __cdecl o_fatal_stops(const char* m, int a, int b, int c) {
    o_fatal(m, a, b, c);
    if (g_stop_at_fatal) *(volatile int*)8 = 0;
}
static void r_fatal_stops(const char* m, int a, int, int) {
    if (g_recording) { g_log[1].add('F', a); record_message(m, 1); }
    if (g_stop_at_fatal) *(volatile int*)8 = 0;
}
// The nth push of the audio heap (from 1) makes its allocation fail, and the pop that follows ends that. The first is a file's notes, the next its first Segments.
static int g_fail_push, g_pushes;
static void note_push(void* h) {
    if (h == &g_audio_heap_token && ++g_pushes == g_fail_push) g_fail_alloc = true;
}
static void __cdecl o_push_counting(void* h) { if (g_recording) { g_log[0].add('P', (uint32_t)(uintptr_t)h); note_push(h); } }
static void __cdecl o_pop_counting() { if (g_recording) { g_log[0].add('p'); g_fail_alloc = false; } }
static void r_push_counting(void* h) { if (g_recording) { g_log[1].add('P', (uint32_t)(uintptr_t)h); note_push(h); } }
static void r_pop_counting() { if (g_recording) { g_log[1].add('p'); g_fail_alloc = false; } }

// ---- the sound lists beneath both sides ---------------------------------------------------------------------------------
class TestSegments : public AtAudioSegmentDatabase {
public:
    void OnReady() override {}
    void Unknown3(void*) override {}
    AtAudioSegment* SegmentAt(unsigned int) override { return nullptr; }
};

class TestDb : public AtAudioDatabase {
public:
    TestSegments m_lists[2];
    int m_id = 0;
    void AllocateBankDatabases() override {}
    void AllocateSegmentDatabases() override {}
    void AllocateEmitterDatabases() override {}
    void AllocateNestedDatabases() override {}
    AtAudioBankDatabase* BankDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioSegmentDatabase* SegmentDatabaseAt(unsigned int index) override {
        if (g_recording) g_log[g_side].add('s', (uint32_t)(m_id * 16 + index));
        return &m_lists[index];
    }
    AtAudioEmitterDatabase* EmitterDatabaseAt(unsigned int) override { return nullptr; }
    AtAudioDatabase* NestedDatabaseAt(unsigned int) override { return nullptr; }
};
enum { kDbs = 5 };
static TestDb* g_db[kDbs];

static void reset_lists() {
    for (int i = 0; i < kDbs; i++) {
        for (TestSegments& s : g_db[i]->m_lists) {
            s.Clear();
            s.Init(64);
        }
    }
}

static void* fake_segment(unsigned i) { return (void*)(uintptr_t)(0x10000000u + i * 16); }

// Each list gets some of the names: a fifth are in none, a third of the rest are in a later list too, so the first one found matters. 3 is past the first count, and only "notcount" is there.
static void fill_lists(const std::vector<std::string>& names) {
    reset_lists();
    unsigned i = 0;
    for (const std::string& n : names) {
        i++;
        if (i % 5 == 4) continue;
        switch (i % 4) {
        case 0: g_db[0]->m_lists[0].Insert(n.c_str(), fake_segment(i)); break;
        case 1: g_db[0]->m_lists[1].Insert(n.c_str(), fake_segment(i)); break;
        case 2: g_db[1]->m_lists[0].Insert(n.c_str(), fake_segment(i)); break;
        default: g_db[2]->m_lists[1].Insert(n.c_str(), fake_segment(i)); break;
        }
        if (i % 3 == 0) g_db[2]->m_lists[0].Insert(n.c_str(), fake_segment(i + 1000));
    }
    g_db[3]->m_lists[0].Insert("notcount", fake_segment(2000));
    g_db[4]->m_lists[1].Insert("lastdb", fake_segment(2001));
    g_db[1]->m_lists[1].Insert("seconddb", fake_segment(2002));
}

// ---- managers -----------------------------------------------------------------------------------------------------------
typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* Fn0)(void*, void*);
typedef void(__fastcall* Fn1)(void*, void*, void*);
typedef void(__fastcall* FnPath)(void*, void*, void*, const char*);
typedef void*(__fastcall* FnFind1)(void*, void*, const char*);
typedef void(__fastcall* FnName)(void*, void*, const void*, char*);
typedef void*(__fastcall* FnFind2)(void*, void*, const char*, const char*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

static GmAudioData* new_manager(int side) {
    GmAudioData* m = (GmAudioData*)std::calloc(1, sizeof(GmAudioData));
    m->m_firstDatabases[0] = g_db[0];
    m->m_firstDatabases[2] = g_db[1];
    m->m_firstDatabases[3] = g_db[3];
    m->m_firstDatabaseCount = 3;
    m->m_databases[7] = g_db[2];
    m->m_databases[59] = g_db[4];
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) {
        std::memset(&m->m_noteBanks[i], 0xCD, sizeof(GmSoundNoteBank));
        if (side == 0) at<FnCtor>(0x00136CC0)(&m->m_noteBanks[i], nullptr);
        else new (&m->m_noteBanks[i]) GmSoundNoteBank;
    }
    return m;
}

struct Pair {
    GmAudioData* m[2];
    Pair() {
        m[0] = new_manager(0);
        m[1] = new_manager(1);
        *(GmAudioData**)(uintptr_t)0x003B354C = m[0];
        GmAudioData::s_instance = m[1];
    }
    // Every bank's destructor, on both sides, with what it frees compared.
    ~Pair() {
        { On on(0); for (int i = 0; i < GmAudioData::kNoteBanks; i++) at<Fn0>(0x00136740)(&m[0]->m_noteBanks[i], nullptr); }
        { On on(1); for (int i = 0; i < GmAudioData::kNoteBanks; i++) m[1]->m_noteBanks[i].~GmSoundNoteBank(); }
        compare_logs("bank destructors");
        std::free(m[0]);
        std::free(m[1]);
    }
};

static int g_notes_seen, g_choices_seen, g_scaled_seen, g_segments_found, g_segments_absent, g_emitters_seen, g_conditionals_seen, g_stops_seen, g_singletons_seen, g_vfx_seen, g_events_seen, g_lookups_found, g_crashes_seen;

static long long note_index(const GmSoundNoteBank* b, const void* p) {
    if (p == nullptr) return -1;
    const long long d = (const char*)p - (const char*)b->m_notes;
    if (b->m_notes == nullptr || d < 0 || d % sizeof(GmSoundNote) != 0 || d / (long long)sizeof(GmSoundNote) >= (long long)b->m_noteCount) return -2;
    return d / (long long)sizeof(GmSoundNote);
}

static void same_map(const char* what, const GmSoundNoteBank* bo, const GmNameMap& o, const GmSoundNoteBank* br, const GmNameMap& r) {
    eq(what, o.m_size, r.m_size);
    eq(what, o.m_entries != nullptr, r.m_entries != nullptr);
    if (!o.m_entries || !r.m_entries || o.m_size != r.m_size) return;
    for (unsigned i = 0; i < o.m_size; i++) {
        eq(what, note_index(bo, o.m_entries[i].m_value), note_index(br, r.m_entries[i].m_value));
        if (o.m_entries[i].m_value) eq(what, std::memcmp(o.m_entries[i].m_name, r.m_entries[i].m_name, 8), 0);
    }
}

static void same_bank(const char* what, const GmSoundNoteBank* o, const GmSoundNoteBank* r) {
    // Up to the terminator: with no NameSpace, both copy eight bytes from an empty literal, and what follows each literal is its own image's.
    eq(what, std::strncmp(o->m_name, r->m_name, 8), 0);
    eq(what, o->m_noteCount, r->m_noteCount);
    eq(what, o->m_notes != nullptr, r->m_notes != nullptr);
    same_map(what, o, o->m_byName, r, r->m_byName);
    same_map(what, o, o->m_byTrigger, r, r->m_byTrigger);
    same_map(what, o, o->m_byVfxTrigger, r, r->m_byVfxTrigger);
    if (!o->m_notes || !r->m_notes || o->m_noteCount != r->m_noteCount) return;
    for (unsigned i = 0; i < o->m_noteCount; i++) {
        const GmSoundNote& a = o->m_notes[i];
        const GmSoundNote& b = r->m_notes[i];
        eq("note names", std::memcmp(&a, &b, offsetof(GmSoundNote, m_emitter)), 0);
        eq("note emitter", (uintptr_t)a.m_emitter, (uintptr_t)b.m_emitter);
        eq("note conditional", (uintptr_t)a.m_conditional, (uintptr_t)b.m_conditional);
        eq("note stop", std::memcmp(a.m_stop, b.m_stop, 8), 0);
        eq("note singleton", a.m_singleton, b.m_singleton);
        eq("note choices", a.m_choiceCount, b.m_choiceCount);
        eq("note choices", a.m_choices != nullptr, b.m_choices != nullptr);
        if (!a.m_choices || !b.m_choices || a.m_choiceCount != b.m_choiceCount) continue;
        for (unsigned k = 0; k < a.m_choiceCount; k++) {
            eq("choice name", std::memcmp(a.m_choices[k].m_name, b.m_choices[k].m_name, 8), 0);
            eq("choice segment", (uintptr_t)a.m_choices[k].m_segment, (uintptr_t)b.m_choices[k].m_segment);
            eq("choice weight", *(const uint32_t*)&a.m_choices[k].m_weight, *(const uint32_t*)&b.m_choices[k].m_weight);
        }
    }
}

static void count_bank(const GmSoundNoteBank* b) {
    if (!b->m_notes) return;
    for (unsigned i = 0; i < b->m_noteCount; i++) {
        const GmSoundNote& n = b->m_notes[i];
        g_notes_seen++;
        if (n.m_emitter) g_emitters_seen++;
        if (n.m_conditional) g_conditionals_seen++;
        if (n.m_stop[0]) g_stops_seen++;
        if (n.m_singleton) g_singletons_seen++;
        if (n.m_vfxTrigger[0]) g_vfx_seen++;
        if (n.m_gameEvent[0]) g_events_seen++;
        if (n.m_choices) {
            for (unsigned k = 0; k < n.m_choiceCount; k++) {
                g_choices_seen++;
                if (n.m_choices[k].m_weight != 1.0f) g_scaled_seen++;
                if (n.m_choices[k].m_segment) g_segments_found++;
                else g_segments_absent++;
            }
        }
    }
}

template <class FO, class FR> static bool both(const char* what, FO fo, FR fr) {
    const bool oc = guarded([&] { On on(0); fo(); });
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    const int pushes = g_pushes;
    g_pushes = 0;
    const bool rc = guarded([&] { On on(1); fr(); });
    g_recording = false;
    g_side = 0;
    g_fail_alloc = false;
    eq(what, pushes, g_pushes);
    g_pushes = 0;
    eq(what, oc, rc);
    if (oc) g_crashes_seen++;
    if ((oc || rc) && std::getenv("GON_SHOW_STOPS")) std::printf("stop: %s: %s\n", g_context.c_str(), what);
    compare_logs(what);
    return oc || rc;
}

static std::vector<std::string> strings_of(const std::vector<unsigned char>& data) {
    std::vector<std::string> out;
    std::string now;
    bool in = false;
    for (unsigned char c : data) {
        if (c == '"') {
            if (in && !now.empty() && now.size() <= 8) {
                bool have = false;
                for (const std::string& s : out) have = have || s == now;
                if (!have) out.push_back(now);
            }
            now.clear();
            in = !in;
        } else if (in) {
            now += (char)c;
        }
    }
    return out;
}

static void set_file(const std::vector<unsigned char>& data) {
    g_fs->files[0].data = g_fs->files[1].data = data.data();
    g_fs->files[0].size = g_fs->files[1].size = data.size();
}

// Load, register, every search, the name of every segment, and free, on bank `index`.
static void exercise(Pair& p, int index, const std::vector<std::string>& names) {
    GmSoundNoteBank* bo = &p.m[0]->m_noteBanks[index];
    GmSoundNoteBank* br = &p.m[1]->m_noteBanks[index];
    bool crashed = both("register", [&] { at<Fn1>(0x00139550)(p.m[0], nullptr, bo); }, [&] { p.m[1]->RegisterNotes(br); });
    same_bank("after register", bo, br);
    count_bank(br);
    if (crashed) return;
    for (const std::string& n : names) {
        char name[9] = {0};
        std::strncpy(name, n.c_str(), 8);
        for (const char* bank : {"", "Space", "other"}) {
            void* a = nullptr;
            void* b = nullptr;
            both("find note", [&] { a = at<FnFind2>(0x00137EB0)(p.m[0], nullptr, name, bank); }, [&] { b = p.m[1]->FindNote(name, bank); });
            eq("find note", note_index(bo, a), note_index(br, b));
            if (b) g_lookups_found++;
            both("find by trigger", [&] { a = at<FnFind2>(0x00137F00)(p.m[0], nullptr, name, bank); }, [&] { b = p.m[1]->FindNoteByTrigger(name, bank); });
            eq("find by trigger", note_index(bo, a), note_index(br, b));
            if (b) g_lookups_found++;
        }
        void* a = nullptr;
        void* b = nullptr;
        both("find segment", [&] { a = at<FnFind1>(0x00137D20)(p.m[0], nullptr, name); }, [&] { b = p.m[1]->FindSegment(name); });
        eq("find segment", (uintptr_t)a, (uintptr_t)b);
        if (b) {
            char oo[16], rr[16];
            std::memset(oo, 0xCD, sizeof oo);
            std::memset(rr, 0xCD, sizeof rr);
            both("segment name", [&] { at<FnName>(0x00137DE0)(p.m[0], nullptr, a, oo); }, [&] { p.m[1]->GetSegmentName((AtAudioSegment*)b, rr); });
            eq("segment name", std::memcmp(oo, rr, sizeof oo), 0);
        }
    }
    both("free", [&] { at<Fn1>(0x00136990)(p.m[0], nullptr, bo); }, [&] { p.m[1]->FreeNotes(br); });
    same_bank("after free", bo, br);
    // A second free finds nothing to do.
    both("free again", [&] { at<Fn1>(0x00136990)(p.m[0], nullptr, bo); }, [&] { p.m[1]->FreeNotes(br); });
}

static void run_text(const std::string& name, const std::vector<unsigned char>& data, bool stop) {
    g_context = name;
    set_file(data);
    const std::vector<std::string> names = strings_of(data);
    fill_lists(names);
    Pair p;
    g_stop_at_fatal = stop;
    const bool crashed = both("load", [&] { at<FnPath>(0x00139090)(p.m[0], nullptr, &p.m[0]->m_noteBanks[3], "AUDIO\\FILE"); }, [&] { p.m[1]->LoadNotes(&p.m[1]->m_noteBanks[3], "AUDIO\\FILE"); });
    same_bank("after load", &p.m[0]->m_noteBanks[3], &p.m[1]->m_noteBanks[3]);
    if (!crashed) exercise(p, 3, names);
    else count_bank(&p.m[1]->m_noteBanks[3]);
    g_stop_at_fatal = false;
    if (crashed) {
        // What was loaded before the stop is left to leak: the destructors of Pair free only the maps.
        for (int s = 0; s < 2; s++) p.m[s]->m_noteBanks[3].m_notes = nullptr;
    }
}

static std::vector<unsigned char> bytes(const std::string& s) { return std::vector<unsigned char>(s.begin(), s.end()); }

// ---- the item -----------------------------------------------------------------------------------------------------------
static void same_item(const char* what, Pair& p, const GmSoundNoteItem* o, const GmSoundNoteItem* r) {
    eq(what, *(const int*)o->m_extension, *(const int*)r->m_extension);
    eqs(what, o->m_path, r->m_path);
    eq(what, (uintptr_t)o->m_next, (uintptr_t)r->m_next);
    eq(what, o->m_bank ? o->m_bank - p.m[0]->m_noteBanks : -1, r->m_bank ? r->m_bank - p.m[1]->m_noteBanks : -1);
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) eq(what, p.m[0]->m_noteBankUsed[i], p.m[1]->m_noteBankUsed[i]);
}

static const char kItemText[] = "SoundNotes[2]{Note \"a\"{AnimationTrigger \"ta\" Segments[1]{\"s1\"}} Note \"b\"{Stop \"a\" Segments[2]{\"s1\" 1.0 \"s2\" 3.0}}}";

static void run_item() {
    const std::vector<unsigned char> data = bytes(kItemText);
    set_file(data);
    fill_lists(strings_of(data));
    Pair p;
    alignas(16) unsigned char mo[sizeof(GmSoundNoteItem)], mr[sizeof(GmSoundNoteItem)];
    std::memset(mo, 0xCD, sizeof mo);
    std::memset(mr, 0xCD, sizeof mr);
    GmSoundNoteItem* o = (GmSoundNoteItem*)mo;
    GmSoundNoteItem* r = (GmSoundNoteItem*)mr;
    g_context = "item: constructor";
    both("item ctor", [&] { at<FnCtor>(0x001B3640)(o, nullptr); }, [&] { new (r) GmSoundNoteItem; });
    eq("item vftable", *(uint32_t*)o, 0x002950AC);
    same_item("after ctor", p, o, r);

    GmSoundNoteItem::Request request;
    std::memset(&request, 0, sizeof request);
    std::strcpy(request.m_path, "AUDIO\\FILE");
    typedef void(__fastcall* FnConfigure)(void*, void*, const void*);
    g_context = "item: configure";
    both("item configure", [&] { at<FnConfigure>(0x001B3540)(o, nullptr, &request); }, [&] { r->Configure(&request); });
    same_item("after configure", p, o, r);

    g_context = "item: run";
    both("item run", [&] { at<Fn0>(0x001B35D0)(o, nullptr); }, [&] { r->Run(); });
    same_item("after run", p, o, r);
    if (o->m_bank && r->m_bank) same_bank("item bank", o->m_bank, r->m_bank);

    g_context = "item: release";
    both("item release", [&] { at<Fn0>(0x001B3580)(o, nullptr); }, [&] { r->Unknown0(); });
    same_item("after release", p, o, r);
    both("item release again", [&] { at<Fn0>(0x001B3580)(o, nullptr); }, [&] { r->Unknown0(); });
    same_item("after release again", p, o, r);

    // Configuring gives back what it had; so does the destructor.
    std::strcpy(request.m_path, "AUDIO\\FILE");
    g_context = "item: configure over a bank";
    both("item configure", [&] { at<FnConfigure>(0x001B3540)(o, nullptr, &request); }, [&] { r->Configure(&request); });
    both("item run", [&] { at<Fn0>(0x001B35D0)(o, nullptr); }, [&] { r->Run(); });
    same_item("run again", p, o, r);
    both("item configure", [&] { at<FnConfigure>(0x001B3540)(o, nullptr, &request); }, [&] { r->Configure(&request); });
    same_item("configure over a bank", p, o, r);
    both("item run", [&] { at<Fn0>(0x001B35D0)(o, nullptr); }, [&] { r->Run(); });
    g_context = "item: destructor";
    both("item dtor", [&] { at<Fn0>(0x001B3680)(o, nullptr); }, [&] { r->~GmSoundNoteItem(); });
    eq("item vftable after", *(uint32_t*)o, 0x002950AC);
    eq("item bank", (uintptr_t)o->m_bank, (uintptr_t)r->m_bank);
    eq("item extension", *(const int*)o->m_extension, *(const int*)r->m_extension);
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) eq("used", p.m[0]->m_noteBankUsed[i], p.m[1]->m_noteBankUsed[i]);

    // All sixty in use: no load.
    g_context = "item: no bank";
    both("item ctor", [&] { at<FnCtor>(0x001B3640)(o, nullptr); }, [&] { new (r) GmSoundNoteItem; });
    both("item configure", [&] { at<FnConfigure>(0x001B3540)(o, nullptr, &request); }, [&] { r->Configure(&request); });
    for (int s = 0; s < 2; s++) for (int i = 0; i < GmAudioData::kNoteBanks; i++) p.m[s]->m_noteBankUsed[i] = 1;
    both("item run", [&] { at<Fn0>(0x001B35D0)(o, nullptr); }, [&] { r->Run(); });
    same_item("no bank", p, o, r);
    for (int s = 0; s < 2; s++) for (int i = 0; i < GmAudioData::kNoteBanks; i++) p.m[s]->m_noteBankUsed[i] = 0;

    // The deleting destructor: flag 0 keeps the memory, flag 1 frees it. Slot 2 of each side's table.
    typedef void*(__fastcall* FnSlot)(void*, void*, unsigned);
    for (unsigned flags : {0u, 1u}) {
        g_context = "item: deleting, flags " + std::to_string(flags);
        void* ao = std::malloc(sizeof(GmSoundNoteItem));
        void* ar = std::malloc(sizeof(GmSoundNoteItem));
        at<FnCtor>(0x001B3640)(ao, nullptr);
        new (ar) GmSoundNoteItem;
        void* ro = nullptr;
        void* rr = nullptr;
        both("item deleting", [&] { ro = (*(FnSlot**)ao)[2](ao, nullptr, flags); }, [&] { rr = (*(FnSlot**)ar)[2](ar, nullptr, flags); });
        eq("item deleting returns", ro == ao, rr == ar);
        if (!(flags & 1)) { std::free(ao); std::free(ar); }
    }
    // Arrays, with their count in front, as the parser at 0x00142E90 makes them; flag 2 walks them, and 3 frees the block too. `new[]` here is what makes the host compiler emit that form.
    for (unsigned flags : {2u, 3u}) {
        g_context = "item: deleting an array, flags " + std::to_string(flags);
        int* bo = (int*)std::malloc(4 + 3 * sizeof(GmSoundNoteItem));
        *bo = 3;
        GmSoundNoteItem* ao = (GmSoundNoteItem*)(bo + 1);
        for (int i = 0; i < 3; i++) at<FnCtor>(0x001B3640)(&ao[i], nullptr);
        GmSoundNoteItem* ar = new GmSoundNoteItem[3];
        void* ro = nullptr;
        void* rr = nullptr;
        both("array deleting", [&] { ro = (*(FnSlot**)ao)[2](ao, nullptr, flags); }, [&] { rr = (*(FnSlot**)ar)[2](ar, nullptr, flags); });
        eq("array deleting returns", ro == bo, rr == ((int*)ar) - 1);
        if (!(flags & 1)) { std::free(bo); operator delete[](((int*)ar) - 1); }
    }
}

// ---- banks --------------------------------------------------------------------------------------------------------------
static void run_banks() {
    g_context = "banks";
    fill_lists({});
    Pair p;
    // The element constructors, on dirty memory.
    unsigned char a[sizeof(GmSoundNote)], b[sizeof(GmSoundNote)];
    std::memset(a, 0xCD, sizeof a);
    std::memset(b, 0xCD, sizeof b);
    at<FnCtor>(0x00136D50)(a, nullptr);
    new (b) GmSoundNote;
    eq("note constructor", std::memcmp(a, b, sizeof a), 0);
    unsigned char c[sizeof(GmSoundChoice)], d[sizeof(GmSoundChoice)];
    std::memset(c, 0xCD, sizeof c);
    std::memset(d, 0xCD, sizeof d);
    at<FnCtor>(0x00136D30)(c, nullptr);
    new (d) GmSoundChoice;
    eq("choice constructor", std::memcmp(c, d, sizeof c), 0);
    // Their name, map and pointer fields, dirty beneath, are as the original leaves them.
    same_bank("new bank", &p.m[0]->m_noteBanks[0], &p.m[1]->m_noteBanks[0]);
    eq("new bank name", std::memcmp(p.m[0]->m_noteBanks[1].m_name, p.m[1]->m_noteBanks[1].m_name, 8), 0);
    eq("new bank count", p.m[0]->m_noteBanks[1].m_noteCount, p.m[1]->m_noteBanks[1].m_noteCount);

    // Allocating them all, one more, a freed one again, and one not of the manager.
    typedef void*(__fastcall* FnAlloc)(void*, void*);
    for (int i = 0; i <= GmAudioData::kNoteBanks; i++) {
        void* x = at<FnAlloc>(0x00136930)(p.m[0], nullptr);
        GmSoundNoteBank* y = p.m[1]->AllocateNoteBank();
        eq("allocate", x ? (GmSoundNoteBank*)x - p.m[0]->m_noteBanks : -1, y ? y - p.m[1]->m_noteBanks : -1);
    }
    for (int i : {0, 17, 59}) {
        at<Fn1>(0x00136960)(p.m[0], nullptr, &p.m[0]->m_noteBanks[i]);
        p.m[1]->ReleaseNoteBank(&p.m[1]->m_noteBanks[i]);
    }
    int outside;
    at<Fn1>(0x00136960)(p.m[0], nullptr, &outside);
    p.m[1]->ReleaseNoteBank((GmSoundNoteBank*)&outside);
    for (int i = 0; i < GmAudioData::kNoteBanks; i++) eq("used", p.m[0]->m_noteBankUsed[i], p.m[1]->m_noteBankUsed[i]);
    // A bank left as a load that ran out of memory for its notes leaves it: a count and maps, and no notes. Freeing it does nothing.
    g_context = "banks: free with no notes";
    for (int s = 0; s < 2; s++) {
        GmSoundNoteBank& b = p.m[s]->m_noteBanks[10];
        b.m_noteCount = 2;
        b.m_byName.Init(2);
        b.m_byTrigger.Init(2);
    }
    both("free with no notes", [&] { at<Fn1>(0x00136990)(p.m[0], nullptr, &p.m[0]->m_noteBanks[10]); }, [&] { p.m[1]->FreeNotes(&p.m[1]->m_noteBanks[10]); });
    same_bank("free with no notes", &p.m[0]->m_noteBanks[10], &p.m[1]->m_noteBanks[10]);
    for (int i = 0; i <= 3; i++) {
        void* x = at<FnAlloc>(0x00136930)(p.m[0], nullptr);
        GmSoundNoteBank* y = p.m[1]->AllocateNoteBank();
        eq("allocate again", x ? (GmSoundNoteBank*)x - p.m[0]->m_noteBanks : -1, y ? y - p.m[1]->m_noteBanks : -1);
    }
}

// Three files in three banks of one manager: which is found, and what the bank name filters. Bank 0 is unused but named, and its map empty: a search must pass over it, not into it.
static void run_many() {
    g_context = "many banks";
    const std::string files[3] = {
        "NameSpace \"Other\" SoundNotes[2]{Note \"n\"{AnimationTrigger \"t\" Segments[1]{\"s1\"}} Note \"only1\"{AnimationTrigger \"t1\"}}",
        "NameSpace \"Space\" SoundNotes[2]{Note \"n\"{AnimationTrigger \"t\" VFXTrigger \"v\" Segments[1]{\"s2\"}} Note \"only2\"{AnimationTrigger \"t2\"}}",
        "SoundNotes[2]{Note \"n\"{AnimationTrigger \"t\"} Note \"only3\"{AnimationTrigger \"t3\"}}",
    };
    fill_lists({"s1", "s2"});
    Pair p;
    for (int s = 0; s < 2; s++) std::memcpy(p.m[s]->m_noteBanks[0].m_name, "Space\0\0", 8);
    const int where[3] = {1, 4, 59};
    std::vector<std::vector<unsigned char>> data;
    for (const std::string& f : files) data.push_back(bytes(f));
    for (int i = 0; i < 3; i++) {
        set_file(data[i]);
        both("load", [&] { at<FnPath>(0x00139090)(p.m[0], nullptr, &p.m[0]->m_noteBanks[where[i]], "F"); }, [&] { p.m[1]->LoadNotes(&p.m[1]->m_noteBanks[where[i]], "F"); });
        both("register", [&] { at<Fn1>(0x00139550)(p.m[0], nullptr, &p.m[0]->m_noteBanks[where[i]]); }, [&] { p.m[1]->RegisterNotes(&p.m[1]->m_noteBanks[where[i]]); });
        same_bank("many", &p.m[0]->m_noteBanks[where[i]], &p.m[1]->m_noteBanks[where[i]]);
    }
    const auto place = [&](int side, void* n) -> long long {
        if (!n) return -1;
        for (int i : where) {
            const GmSoundNoteBank& b = p.m[side]->m_noteBanks[i];
            if (n >= (void*)b.m_notes && n < (void*)(b.m_notes + b.m_noteCount)) return i * 100 + ((GmSoundNote*)n - b.m_notes);
        }
        return -2;
    };
    for (const char* name : {"n", "t", "only1", "only2", "only3", "t1", "t2", "t3", "gone"}) {
        for (const char* bank : {"", "Other", "Space", "Spac", "Absent"}) {
            void* a = nullptr;
            void* b = nullptr;
            both("many find", [&] { a = at<FnFind2>(0x00137EB0)(p.m[0], nullptr, name, bank); }, [&] { b = p.m[1]->FindNote(name, bank); });
            eq("many find", place(0, a), place(1, b));
            if (b) g_lookups_found++;
            both("many trigger", [&] { a = at<FnFind2>(0x00137F00)(p.m[0], nullptr, name, bank); }, [&] { b = p.m[1]->FindNoteByTrigger(name, bank); });
            eq("many trigger", place(0, a), place(1, b));
            if (b) g_lookups_found++;
        }
    }
    // A null bank name, as 0x00160237 passes: with a bank in use, a crash on both sides.
    g_context = "many banks: null bank name";
    both("null bank name", [&] { at<FnFind2>(0x00137EB0)(p.m[0], nullptr, "n", nullptr); }, [&] { p.m[1]->FindNote("n", nullptr); });
    both("null bank name", [&] { at<FnFind2>(0x00137F00)(p.m[0], nullptr, "t", nullptr); }, [&] { p.m[1]->FindNoteByTrigger("t", nullptr); });
    for (int i : where) {
        both("free", [&] { at<Fn1>(0x00136990)(p.m[0], nullptr, &p.m[0]->m_noteBanks[i]); }, [&] { p.m[1]->FreeNotes(&p.m[1]->m_noteBanks[i]); });
        same_bank("many freed", &p.m[0]->m_noteBanks[i], &p.m[1]->m_noteBanks[i]);
    }
    // With none in use, a null bank name is never read.
    g_context = "many banks: none in use";
    void* a = nullptr;
    void* b = nullptr;
    both("none in use", [&] { a = at<FnFind2>(0x00137EB0)(p.m[0], nullptr, "n", nullptr); }, [&] { b = p.m[1]->FindNote("n", nullptr); });
    eq("none in use", (uintptr_t)a, (uintptr_t)b);
}

// FindSegment and GetSegmentName across the lists, directly.
static void run_lists() {
    g_context = "lists";
    fill_lists({"first", "second", "third", "absent4", "fifth", "sixth", "seventh", "eighth", "ninth"});
    Pair p;
    for (const char* name : {"first", "second", "third", "absent4", "fifth", "sixth", "seventh", "eighth", "ninth", "notcount", "lastdb", "seconddb", "gone"}) {
        g_context = std::string("lists: ") + name;
        void* a = nullptr;
        void* b = nullptr;
        both("find segment", [&] { a = at<FnFind1>(0x00137D20)(p.m[0], nullptr, name); }, [&] { b = p.m[1]->FindSegment(name); });
        eq("find segment", (uintptr_t)a, (uintptr_t)b);
        if (b) g_segments_found++;
        else g_segments_absent++;
    }
    for (unsigned i : {1u, 2u, 3u, 5u, 6u, 7u, 1003u, 1006u, 2000u, 2001u, 2002u, 4u, 9999u}) {
        g_context = "lists: name of " + std::to_string(i);
        char oo[16], rr[16];
        std::memset(oo, 0xCD, sizeof oo);
        std::memset(rr, 0xCD, sizeof rr);
        both("segment name", [&] { at<FnName>(0x00137DE0)(p.m[0], nullptr, fake_segment(i), oo); }, [&] { p.m[1]->GetSegmentName((AtAudioSegment*)fake_segment(i), rr); });
        eq("segment name", std::memcmp(oo, rr, sizeof oo), 0);
    }
    // A later first database, a database at the last slot, and no database at all.
    g_context = "lists: emptied";
    for (int s = 0; s < 2; s++) {
        p.m[s]->m_firstDatabaseCount = 0;
        std::memset(p.m[s]->m_databases, 0, sizeof p.m[s]->m_databases);
    }
    void* a = nullptr;
    void* b = nullptr;
    both("no lists", [&] { a = at<FnFind1>(0x00137D20)(p.m[0], nullptr, "first"); }, [&] { b = p.m[1]->FindSegment("first"); });
    eq("no lists", (uintptr_t)a, (uintptr_t)b);
    // With no list at all, only the first store writes the name.
    char oo[16], rr[16];
    std::memset(oo, 0xCD, sizeof oo);
    std::memset(rr, 0xCD, sizeof rr);
    both("no lists: name", [&] { at<FnName>(0x00137DE0)(p.m[0], nullptr, fake_segment(1), oo); }, [&] { p.m[1]->GetSegmentName((AtAudioSegment*)fake_segment(1), rr); });
    eq("no lists: name", std::memcmp(oo, rr, sizeof oo), 0);
}

static void run_synthetic() {
    // Each word, with text shown to reach a compared field: see the counts in the summary line.
    static const char* const good[] = {
        "SoundNotes[0]{}",
        "SoundNotes[0]{Note}",
        "NameSpace \"Space\" SoundNotes[0]{}",
        "NameSpace \"Space\" SoundNotes[1]{Note \"a\"{}}",
        "NameSpace \"12345678\" SoundNotes[1]{Note \"a\"{}}",
        "SoundNotes[1]{Note \"a\"{}}",
        "SoundNotes[1]{Note \"12345678\"{}}",
        "SoundNotes[1]{Note \"a\"{AnimationTrigger \"t\"}}",
        "SoundNotes[1]{Note \"a\"{AnimationTrigger \"12345678\"}}",
        "SoundNotes[1]{Note \"a\"{VFXTrigger \"v\"}}",
        "SoundNotes[1]{Note \"a\"{GameEvent \"e\"}}",
        "SoundNotes[1]{Note \"a\"{GameEvent \"e\" \"arg\"}}",
        "SoundNotes[1]{Note \"a\"{GameEvent \"e\" \"arg12345678\"}}",
        "SoundNotes[1]{Note \"a\"{GameEvent \"e\" Singleton}}",
        "SoundNotes[1]{Note \"a\"{GameEvent \"e\" 5 Singleton}}",
        "SoundNotes[1]{Note \"a\"{GameEvent \"e\" \"arg\" GameEvent \"f\" Singleton}}",
        "SoundNotes[1]{Note \"a\"{ActorEmitter}}",
        "SoundNotes[1]{Note \"a\"{GlobalEmitter \"sfx\"}}",
        "SoundNotes[1]{Note \"a\"{GlobalEmitter \"gone\"}}",
        "SoundNotes[1]{Note \"a\"{GlobalEmitter \"sfx\" ActorEmitter}}",
        "SoundNotes[1]{Note \"a\"{Conditional \"voice\"}}",
        "SoundNotes[1]{Note \"a\"{Conditional \"gone\"}}",
        "SoundNotes[1]{Note \"a\"{GlobalEmitter \"sfx\" Conditional \"voice\"}}",
        "SoundNotes[1]{Note \"a\"{Stop \"b\"}}",
        "SoundNotes[1]{Note \"a\"{Singleton}}",
        "SoundNotes[1]{Note \"a\"{Segments[0]{}}}",
        "SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\"}}}",
        "SoundNotes[1]{Note \"a\"{Segments[2]{\"s1\" \"s2\"}}}",
        "SoundNotes[1]{Note \"a\"{Segments[2]{\"s1\" 1.0 \"s2\" 3.0}}}",
        "SoundNotes[1]{Note \"a\"{Segments[3]{\"s1\" 0.1 \"s2\" 0.2 \"s3\" 0.7}}}",
        "SoundNotes[1]{Note \"a\"{Segments[3]{\"s1\" 3.3 \"s2\" 2.7 \"s3\" 0.9}}}",
        "SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\" 0.0}}}",
        "SoundNotes[1]{Note \"a\"{Segments[2]{\"s1\" -2.0 \"s2\" 1.0}}}",
        "SoundNotes[1]{Note \"a\"{Segments[2]{\"s1\" -1.0 \"s2\" 1.0}}}",
        "SoundNotes[1]{Note \"a\"{Segments[2]{\"s1\" 0.5 \"s2\"}}}",
        "SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\" 2.0} Segments[1]{\"s2\" 4.0}}}",
        "SoundNotes[1]{Note \"a\"{other 1 2.5 \"text\" , ; Singleton}}",
        "SoundNotes[2]{Note \"a\"{Singleton} Note \"b\"{Stop \"a\"}}",
        "SoundNotes[3]{Note \"a\"{AnimationTrigger \"a\" Segments[1]{\"a\"}} Note \"b\"{AnimationTrigger \"t\" VFXTrigger \"v\" Segments[2]{\"b\" 0.25 \"c\" 0.75}} Note \"c\"{GlobalEmitter \"voice\" Singleton}}",
        "soundnotes[1]{note \"a\"{animationtrigger \"t\" segments[1]{\"s1\" 2.0}}}",
        "SoundNotes[1]{Note \"a\"{}} trailing words are not read",
    };
    for (const char* t : good) run_text(std::string("good [") + t + "]", bytes(t), true);
    // Each of these reaches a report, and in the game a report does not come back.
    static const char* const bad[] = {
        "",
        "NameSpace \"123456789\" SoundNotes[1]{Note \"a\"{}}",
        "Other[1]{Note \"a\"{}}",
        "NameSpace 5 SoundNotes[0]{}",
        "NameSpace \"Space\"",
        "SoundNotes",
        "SoundNotes[",
        "SoundNotes 1 {}",
        "SoundNotes[x]{}",
        "SoundNotes[1 {}",
        "SoundNotes[1] Note \"a\"{}}",
        "SoundNotes[1]{Other \"a\"{}}",
        "SoundNotes[1]{Note 5 {}}",
        "SoundNotes[1]{Note \"a\" Singleton}}",
        "SoundNotes[1]{Note \"a\"{}",
        "SoundNotes[1]{Note \"a\"{} Note \"b\"{}}",
        "SoundNotes[2]{Note \"a\"{}}",
        "SoundNotes[1]{Note \"a\"{AnimationTrigger 5}}",
        "SoundNotes[1]{Note \"a\"{VFXTrigger 5}}",
        "SoundNotes[1]{Note \"a\"{GameEvent 5}}",
        "SoundNotes[1]{Note \"a\"{GlobalEmitter 5}}",
        "SoundNotes[1]{Note \"a\"{Conditional 5}}",
        "SoundNotes[1]{Note \"a\"{Stop 5}}",
        "SoundNotes[1]{Note \"a\"{Segments}}",
        "SoundNotes[1]{Note \"a\"{Segments 1 {}}}",
        "SoundNotes[1]{Note \"a\"{Segments[1]{5}}}",
        "SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\" 2}}}",
        "SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\" 1.0 \"s2\"}}}",
        "SoundNotes[1]{Note \"a\"{Segments[2]{\"s1\"}}}",
        "SoundNotes[2]{Note \"a\"{AnimationTrigger \"t\" Segments[1]{\"s1\"}} Note \"a\"{AnimationTrigger \"t\"}}",
    };
    for (const char* t : bad) run_text(std::string("bad [") + t + "]", bytes(t), true);
    // A name twice: when the report comes back, as it does not in the game, the second goes in anyway.
    run_text("twice", bytes("SoundNotes[2]{Note \"a\"{AnimationTrigger \"t\" VFXTrigger \"v\"} Note \"a\"{AnimationTrigger \"t\" VFXTrigger \"v\"}}"), false);

    // A file that will not open: reported, and then, if that comes back, a null tokenizer.
    {
        g_context = "no file";
        g_fs->has = false;
        std::vector<unsigned char> none;
        set_file(none);
        run_text("no file, stopped", none, true);
        run_text("no file, report comes back", none, false);
        g_fs->has = true;
    }
    // No heap: raised, and then, as nothing stops it, the same allocations with a null heap.
    {
        *(void**)(uintptr_t)0x003B3548 = nullptr;
        GmAudioData::s_heap = nullptr;
        run_text("no heap", bytes("SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\"}}}"), true);
        *(void**)(uintptr_t)0x003B3548 = &g_audio_heap_token;
        GmAudioData::s_heap = &g_audio_heap_token;
    }
    // Memory refused: raised, and then a crash.
    for (int which : {1, 2}) {
        g_fail_push = which;
        run_text("no memory for " + std::to_string(which), bytes("SoundNotes[1]{Note \"a\"{Segments[1]{\"s1\"}}}"), true);
        g_fail_push = 0;
    }
    run_banks();
    run_lists();
    run_many();
    run_item();
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
    if (pos.size() != 2) { std::printf("usage: oracle_gon <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push_counting);
    redirect(0x00011260, (const void*)o_pop_counting);
    redirect(0x00011070, (const void*)o_fatal_stops);
    redirect(0x00011170, (const void*)o_raise);
    // The host's own, so that both sides stop alike and count alike. /SECTION:.text,ERW and no incremental link make each address the function itself.
    redirect((uint32_t)(uintptr_t)(void (*)(const char*, int, int, int))AtFatal, (const void*)r_fatal_stops);
    redirect((uint32_t)(uintptr_t)(void (*)(void*))AtPushHeap, (const void*)r_push_counting);
    redirect((uint32_t)(uintptr_t)(void (*)())AtPopHeap, (const void*)r_pop_counting);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;

    g_fs = new MemFs;
    g_ctx.m_stackCount = 1;
    g_ctx.m_stackSize = 4;
    g_ctx.m_stack = g_ctx_stack;
    g_ctx_stack[0] = g_fs;
    *(AtIoContext**)(uintptr_t)0x002C6278 = &g_ctx;
    AtIoFileRef::s_context = &g_ctx;
    for (uint32_t va : {0x002C60B0u, 0x002C63CCu, 0x002C632Cu, 0x002C6274u}) *(void**)(uintptr_t)va = &g_heap_token;
    AtIoTokenFile::s_heap = &g_heap_token;
    AtIoStream::s_heap = &g_heap_token;
    GmNameMap::s_heap = &g_heap_token;
    GmNameSpaceDb::s_heap = &g_heap_token;
    *(void**)(uintptr_t)0x003B3548 = &g_audio_heap_token;
    GmAudioData::s_heap = &g_audio_heap_token;
    g_fill = 0xCD;

    for (int i = 0; i < kDbs; i++) {
        g_db[i] = new TestDb;
        g_db[i]->m_id = i;
        g_db[i]->m_segmentCount = 2;
        for (TestSegments& s : g_db[i]->m_lists) s.Init(64);
    }
    // The emitters, in one real table, the same one for both sides.
    AtNameTableOwner* owner = (AtNameTableOwner*)std::calloc(1, sizeof(AtNameTableOwner));
    new (&owner->m_nodes) GmNameMapGroup;
    GmNameMap* emitters = new GmNameMap;
    emitters->Init(8);
    emitters->Insert("sfx", (void*)(uintptr_t)0x20000010);
    emitters->Insert("voice", (void*)(uintptr_t)0x20000020);
    owner->m_nodes.Add(emitters);
    *(AtNameTableOwner**)(uintptr_t)0x003B3558 = owner;
    GmAudioData::s_soundOwner = owner;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_synthetic();
    const int synthetic_failures = g_failures;

    int files = 0;
    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    while (std::fgets(line, sizeof line, index)) {
        if (!std::strstr(line, ".GON\t")) continue;
        *std::strchr(line, '\t') = 0;
        FILE* f = nullptr;
        if (fopen_s(&f, (dir + "\\" + line).c_str(), "rb") || !f) { std::printf("cannot read %s\n", line); return 2; }
        std::fseek(f, 0, SEEK_END);
        std::vector<unsigned char> data((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(data.data(), 1, data.size(), f);
        std::fclose(f);
        run_text(line, data, false);
        files++;
    }
    std::fclose(index);
    if (files == 0) { std::printf("no files listed\n"); g_failures++; }
    report_entries();
    std::printf("%s: %d files, %d notes, %d choices (%d with a scaled weight), %d segments found, %d absent, %d emitters, %d conditionals, %d stops, %d singletons, %d VFX triggers, %d events, %d lookups found, %d stops at a crash; %llu comparisons, %d differences (%d in written cases)\n",
                g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", files, g_notes_seen, g_choices_seen, g_scaled_seen, g_segments_found, g_segments_absent, g_emitters_seen, g_conditionals_seen, g_stops_seen, g_singletons_seen, g_vfx_seen, g_events_seen, g_lookups_found, g_crashes_seen, g_checks, g_failures, synthetic_failures);
    return g_failures ? 1 : 0;
}
