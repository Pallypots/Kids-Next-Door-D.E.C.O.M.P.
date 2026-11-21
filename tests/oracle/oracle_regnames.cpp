// Differential oracle for the registry names (Windows x86, local only). See docs/verification.md and include/gm/GmRegistryNames.h.
//
//   oracle_regnames <default.xbe> <tokenizer cases dir> [--entries FILE] [--mutate]
//
// Cases: for every real .MNG file with InstanceRegistries, a fresh map on each side given the library's root registry and then each of the file's registries, under the file names a
// load gives them in two spellings, each looked up by its name, released in the file's order (as the manifest's Free does), and destroyed; every .ADR file on the disc, by its path,
// into one map that grows (the disc has leaf names in common, so duplicates are reported), looked up, released by thirds; written file names for each way KeyFor cuts and copies; a
// duplicate name, and releasing one of two registries under one name; releasing one the map does not hold, and the same one twice; the life cycle: made on filled memory, Init with
// and without a table, Clear through slot 1, destroyed through slot 0 with and without freeing, s_instance with two maps; no heap, and no memory.
//
// Real on both sides: the name map (GmNameMap, recovered, against the original's 0x00024BB0-0x000252A0) and the C library's strstr and strrchr. Doubles on both sides, so no evidence
// about them: the heap (new, delete, push, pop) and the fatal report, which returns here. The registries are zeroed storage with only a file name, which is all Add reads; no registry
// is loaded here, so the file names a real load gives are taken as each .MNG path plus ".ADR", and as "d:\" and the path in lower case: both cut to the same leaf.
//
// Each call that keeps eight bytes of name on its stack (Add, Release, KeyFor) is made with the stack below it filled with one byte first, the same on both sides, so that a name's
// bytes past its terminator, which reach the map's slots, are the same on both and are compared whole.
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

#include "at/AtDataRegistry.h"
#include "gm/GmRegistryNames.h"
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

static_assert(offsetof(AtDataRegistry, m_fileName) == 0x1E8, "AtDataRegistry::m_fileName");
static_assert(sizeof(GmRegistryNames) == 0x14, "GmRegistryNames");

typedef void*(__fastcall* FnCtor)(void*, void*);
typedef void(__fastcall* Fn0)(void*, void*);
typedef int(__fastcall* FnInit)(void*, void*, unsigned);
typedef void(__fastcall* FnKey)(void*, void*, char*, const void*);
typedef void(__fastcall* FnReg)(void*, void*, void*);
typedef void*(__fastcall* FnFind)(void*, void*, const char*);
typedef void*(__fastcall* FnDel)(void*, void*, unsigned);

static const uint32_t kOriginalVftable = 0x00277B60, kOriginalBaseVftable = 0x00272E60, kInstance = 0x002CD138;
static int g_heap;
static unsigned g_files, g_registries, g_adr, g_written;

// ---- the stack below a call, filled ---------------------------------------------------------------------------------------------------------------------------------------------------
// Fills the 0x2000 bytes below its own return address, which are where the next call's frame will be, the top of it included: a function with a local array of its own could not
// reach the first words below its caller, where its return address and saved registers were, and where Add and Release keep their names.
static unsigned char g_scrub = 0xCD;
static __declspec(naked) void scrub() {
    __asm {
        mov edx, edi
        lea edi, [esp - 0x2000]
        mov ecx, 0x2000
        mov al, g_scrub
        rep stosb
        mov edi, edx
        ret
    }
}

// ---- registries: storage shared by both sides, since Add only reads a file name ------------------------------------------------------------------------------------------------------
struct Registries {
    std::vector<AtDataRegistry*> all;
    std::vector<std::string*> names;
    AtDataRegistry* make(const std::string& file) {
        AtDataRegistry* r = (AtDataRegistry*)std::calloc(1, sizeof(AtDataRegistry));
        names.push_back(new std::string(file));
        r->m_fileName = (char*)names.back()->c_str();
        all.push_back(r);
        return r;
    }
    ~Registries() {
        for (AtDataRegistry* r : all) std::free(r);
        for (std::string* s : names) delete s;
    }
};

// ---- one map to a side ------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Pair {
    void* o = nullptr;
    GmRegistryNames* r = nullptr;
};

static void* raw(int fill) {
    void* p = std::malloc(sizeof(GmRegistryNames));
    std::memset(p, fill, sizeof(GmRegistryNames));
    return p;
}

static Pair make(int fill) {
    Pair p;
    void* a = raw(fill);
    void* b = raw(fill);
    { On on(0); p.o = at<FnCtor>(0x00071DF0)(a, nullptr); }
    { On on(1); p.r = new (b) GmRegistryNames; }
    eq("constructor returns this", p.o == a, p.r == (GmRegistryNames*)b);
    return p;
}

static void* recovered_vftable() {
    static void* v = nullptr;
    if (!v) {
        alignas(GmRegistryNames) static unsigned char space[sizeof(GmRegistryNames)];
        GmRegistryNames* const keep = GmRegistryNames::s_instance;
        GmRegistryNames* n = new (space) GmRegistryNames;
        v = *(void**)n;
        GmRegistryNames::s_instance = keep;
    }
    return v;
}
static void* recovered_base_vftable() {
    static void* v = nullptr;
    if (!v) {
        alignas(GmNameMap) static unsigned char space[sizeof(GmNameMap)];
        v = *(void**)new (space) GmNameMap;
    }
    return v;
}
static uint32_t vft_id(int side, const void* object) {
    const void* v = *(void* const*)object;
    if (side == 0) return v == (void*)(uintptr_t)kOriginalVftable ? 1 : v == (void*)(uintptr_t)kOriginalBaseVftable ? 2 : (uint32_t)(uintptr_t)v;
    return v == recovered_vftable() ? 1 : v == recovered_base_vftable() ? 2 : (uint32_t)(uintptr_t)v;
}

// The instance each side holds: 1 for the map given, 2 for the other given, 0 for none.
static uint32_t instance_id(int side, const void* mine, const void* other) {
    const void* s = side == 0 ? *(void**)(uintptr_t)kInstance : (void*)GmRegistryNames::s_instance;
    return s == nullptr ? 0 : s == mine ? 1 : (other && s == other) ? 2 : 0xEEEEEEEEu;
}

static void same(const char* what, const Pair& p, const Pair* other = nullptr) {
    compare_logs(what);
    const GmNameMap* o = (const GmNameMap*)p.o;
    const GmNameMap* r = p.r;
    eq("vftable", vft_id(0, o), vft_id(1, r));
    eq("instance", instance_id(0, p.o, other ? other->o : nullptr), instance_id(1, p.r, other ? other->r : nullptr));
    eq("size", o->m_size, r->m_size);
    eq("table", o->m_entries != nullptr, r->m_entries != nullptr);
    eq("next", (long long)(uintptr_t)o->m_next, (long long)(uintptr_t)r->m_next);
    eq("group", (long long)(uintptr_t)o->m_group, (long long)(uintptr_t)r->m_group);
    if (o->m_entries && r->m_entries && o->m_size == r->m_size) {
        for (unsigned i = 0; i < o->m_size; i++) {
            eq("slot value", (long long)(uintptr_t)o->m_entries[i].m_value, (long long)(uintptr_t)r->m_entries[i].m_value);
            g_checks++;
            if (std::memcmp(o->m_entries[i].m_name, r->m_entries[i].m_name, 8) != 0 && g_failures++ < 25)
                std::printf("DIFF %s: %s: slot %u name %016llX / %016llX\n", g_context.substr(0, 160).c_str(), what, i, _byteswap_uint64(*(const unsigned long long*)o->m_entries[i].m_name),
                            _byteswap_uint64(*(const unsigned long long*)r->m_entries[i].m_name));
        }
    }
}

static void init(Pair& p, unsigned count) {
    int a, b;
    { On on(0); a = at<FnInit>(0x00071E20)(p.o, nullptr, count); }
    { On on(1); b = p.r->Init(count); }
    eq("init returns", a, b);
}
// Fills the stack below, then calls fn(self, arg) as a __thiscall with one argument from that same depth, so that nothing the compiler places between the fill and the call
// lands in the callee's frame. (A scrub() called first left a return address of the harness in the original Add's name, from a push the compiler placed before it.)
static __declspec(naked) void __cdecl scrubbed_call(const void* fn, void* self, void* arg) {
    __asm {
        mov edx, edi
        lea edi, [esp - 0x2000]
        mov ecx, 0x2000
        mov al, g_scrub
        rep stosb
        mov edi, edx
        mov eax, [esp + 4]
        mov ecx, [esp + 8]
        push dword ptr [esp + 12]
        call eax
        ret
    }
}
static void __fastcall r_add(GmRegistryNames* names, void*, AtDataRegistry* registry) { names->Add(registry); }
static void __fastcall r_release(GmRegistryNames* names, void*, AtDataRegistry* registry) { names->Release(registry); }
static void add(Pair& p, AtDataRegistry* registry) {
    { On on(0); scrubbed_call((const void*)(uintptr_t)0x00071F20, p.o, registry); }
    { On on(1); scrubbed_call((const void*)r_add, p.r, registry); }
}
static void release(Pair& p, AtDataRegistry* registry) {
    { On on(0); scrubbed_call((const void*)(uintptr_t)0x00071DC0, p.o, registry); }
    { On on(1); scrubbed_call((const void*)r_release, p.r, registry); }
}
static void find(Pair& p, const char* name) {
    char key[8];
    std::memset(key, 0, sizeof key);
    std::strncpy(key, name, 8);
    void* a;
    void* b;
    { On on(0); a = at<FnFind>(0x00025150)(p.o, nullptr, key); }
    { On on(1); b = p.r->Find(key); }
    eq("find", (long long)(uintptr_t)a, (long long)(uintptr_t)b);
}
static void key_of(Pair& p, AtDataRegistry* registry, char* out_o, char* out_r) {
    { On on(0); scrub(); at<FnKey>(0x00071E50)(p.o, nullptr, out_o, registry); }
    { On on(1); scrub(); p.r->KeyFor(out_r, registry); }
}
// What a registry is filed under, as the original makes it: for looking it up again.
static std::string key_text(Pair& p, AtDataRegistry* registry) {
    char k[9] = {0};
    { On on(0); at<FnKey>(0x00071E50)(p.o, nullptr, k, registry); }
    compare_logs("key text");
    return std::string(k, strnlen(k, 8));
}
static void slot0(Pair& p, unsigned flags) {
    void* a;
    void* b;
    { On on(0); a = at<FnDel>((*(uint32_t**)p.o)[0])(p.o, nullptr, flags); }
    { On on(1); b = flags ? (delete (GmNameMap*)p.r, (void*)p.r) : (p.r->~GmRegistryNames(), (void*)p.r); }
    eq("slot 0 returns", a == p.o, b == p.r);
}
// Destroyed and freed through slot 0, then the instance compared.
static void destroy(Pair& p, const char* what) {
    void* const o = p.o;
    { On on(0); at<FnDel>((*(uint32_t**)o)[0])(o, nullptr, 1); }
    { On on(1); delete (GmNameMap*)p.r; }
    compare_logs(what);
    eq("instance after", *(void**)(uintptr_t)kInstance == nullptr, GmRegistryNames::s_instance == nullptr);
}

// ---- real files ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static std::string read_file(const std::string& path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") || !f) return std::string();
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    std::fclose(f);
    return s;
}

// The quoted paths of a manifest's InstanceRegistries, comments removed.
static std::vector<std::string> registry_paths(const std::string& text) {
    std::string t;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') i++;
        }
        if (i < text.size()) t += text[i];
    }
    std::vector<std::string> out;
    const size_t w = t.find("InstanceRegistries");
    if (w == std::string::npos) return out;
    const size_t open = t.find('{', w), close = t.find('}', w);
    if (open == std::string::npos || close == std::string::npos) return out;
    for (size_t i = open; i < close; i++) {
        if (t[i] == '"') {
            const size_t e = t.find('"', i + 1);
            out.push_back(t.substr(i + 1, e - i - 1));
            i = e;
        }
    }
    return out;
}

static std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s;
}

static void run_manifest(const std::string& name, const std::vector<std::string>& paths, int spelling) {
    g_context = name + (spelling ? " (d:\\, lower case)" : "");
    Registries regs;
    std::vector<AtDataRegistry*> level;
    AtDataRegistry* library = regs.make("Library");
    for (const std::string& p : paths) level.push_back(regs.make(spelling ? "d:\\" + lower(p) + ".adr" : p + ".ADR"));
    g_fill = 0xCD;
    Pair m = make(0xCD);
    same("made", m);
    init(m, 20);
    same("init", m);
    add(m, library);
    same("library", m);
    for (AtDataRegistry* r : level) {
        add(m, r);
        same("add", m);
        g_registries += spelling == 0;
    }
    find(m, "Library");
    for (AtDataRegistry* r : level) find(m, key_text(m, r).c_str());
    find(m, "Instance");
    // As the manifest's Free does, in the file's order.
    for (AtDataRegistry* r : level) {
        release(m, r);
        same("release", m);
        find(m, key_text(m, r).c_str());
    }
    find(m, "Library");
    destroy(m, "destroyed");
}

static void run_adr(const std::vector<std::string>& disc) {
    for (int spelling = 0; spelling < 2; spelling++) {
        g_context = spelling ? "every .ADR path, lower case" : "every .ADR path";
        Registries regs;
        std::vector<AtDataRegistry*> all;
        for (const std::string& p : disc) all.push_back(regs.make(spelling ? lower(p) : p));
        g_fill = 0xCD;
        Pair m = make(0xCD);
        init(m, 20);
        for (AtDataRegistry* r : all) {
            add(m, r);
            same("add", m);
        }
        for (AtDataRegistry* r : all) find(m, key_text(m, r).c_str());
        for (int third = 0; third < 3; third++) {
            for (size_t i = third; i < all.size(); i += 3) {
                release(m, all[i]);
                same("release", m);
            }
            for (AtDataRegistry* r : all) find(m, key_text(m, r).c_str());
        }
        // The complete destructor alone, then the memory freed by hand.
        void* const o = m.o;
        { On on(0); at<Fn0>(0x00071F50)(o, nullptr); }
        { On on(1); m.r->~GmRegistryNames(); }
        same("destroyed", m);
        std::free(m.o);
        std::free(m.r);
    }
}

// ---- written cases ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static const char* const kWritten[] = {
    "Design\\Registry\\N_Dental.ADR", "D:\\DESIGN\\REGISTRY\\PAD.ADR", "design\\registry\\aimer.adr", "Generic", "Library.b", "X\\Y.B", "Abcdefghij.ADR", "Abcdefgh.ADR",
    "Abcdefg.ADR", "A.ADR", "Dir.adr\\Inner\\Leaf.ADR", "One.Ab\\Two.a", "One.A\\Two.ADR", "P.B\\Q.b", "P.b\\Q.B", "R.B\\S", "Dir\\", "\\", "", ".ADR", "a.a.a", "Leaf.A.b",
    "Under\\Score_9.Bin", "Mixed\\CaSe.aDR", "No\\Extension\\Here", "Trailing.", "Two\\\\Separators.ADR", "Dots.x.y.ADR", "x.ADR\\y.adr", "Deep\\er\\and\\deeper\\N_KNIGHT.ADR",
    "Tab\tName.ADR", "Space Name.ADR", "\xC9t\xE9.ADR", "Exactly8\\Abcdefgh", "Nine\\Abcdefghi", "Seven\\Abcdefg",
};

static void run_written() {
    // KeyFor alone, each name, with two fills of the stack: the bytes past a short name's terminator are as they were.
    for (const unsigned char fill : {0xCD, 0x00, 0x5A}) {
        g_scrub = fill;
        g_fill = 0xCD;
        Pair m = make(0xCD);
        Registries regs;
        for (const char* w : kWritten) {
            g_context = std::string("key [") + w + "] fill " + std::to_string(fill);
            AtDataRegistry* r = regs.make(w);
            for (const unsigned char before : {0x5A, 0x00}) {
                char a[12], b[12];
                std::memset(a, before, sizeof a);
                std::memset(b, before, sizeof b);
                key_of(m, r, a, b);
                compare_logs("key");
                for (int i = 0; i < 12; i++) eq("key byte", (unsigned char)a[i], (unsigned char)b[i]);
            }
            g_written++;
        }
        // A 255-character path, the longest the stack's copy holds.
        {
            g_context = "key of 255 characters";
            std::string longest(240, 'p');
            longest += "\\LongLeafName.ADR";
            longest.resize(255, 'q');
            AtDataRegistry* r = regs.make(longest);
            char a[12], b[12];
            std::memset(a, 0x5A, sizeof a);
            std::memset(b, 0x5A, sizeof b);
            key_of(m, r, a, b);
            for (int i = 0; i < 12; i++) eq("key byte", (unsigned char)a[i], (unsigned char)b[i]);
        }
        // All into one map, then out again, backwards.
        g_context = std::string("written names in one map, fill ") + std::to_string(fill);
        init(m, 5);
        same("init", m);
        std::vector<AtDataRegistry*> added;
        for (size_t i = 0; i < sizeof kWritten / sizeof kWritten[0]; i++) {
            if (!*kWritten[i] || !std::strcmp(kWritten[i], "Dir\\") || !std::strcmp(kWritten[i], "\\") || !std::strcmp(kWritten[i], ".ADR")) continue;  // empty names: below
            added.push_back(regs.all[i]);
            add(m, regs.all[i]);
            same("add", m);
        }
        for (AtDataRegistry* r : added) find(m, key_text(m, r).c_str());
        for (size_t i = added.size(); i-- > 0;) {
            release(m, added[i]);
            same("release", m);
        }
        destroy(m, "destroyed");
    }
    g_scrub = 0xCD;

    // Duplicates: one registry twice, and two registries under one name; then each of the two released, which removes the first the probe meets.
    for (int order = 0; order < 2; order++) {
        g_context = std::string("duplicates, releasing the ") + (order ? "second" : "first");
        Registries regs;
        AtDataRegistry* a = regs.make("Design\\Registry\\Hazard.ADR");
        AtDataRegistry* b = regs.make("Design\\Location\\Lcave1\\Hazard.ADR");
        AtDataRegistry* c = regs.make("Design\\Registry\\Pad.ADR");
        g_fill = 0xCD;
        Pair m = make(0xCD);
        init(m, 3);
        add(m, a);
        same("first", m);
        add(m, b);
        same("same name", m);
        add(m, c);
        same("another", m);
        add(m, c);
        same("again", m);
        find(m, "Hazard");
        release(m, order ? b : a);
        same("one released", m);
        find(m, "Hazard");
        release(m, order ? a : b);
        same("other released", m);
        find(m, "Hazard");
        find(m, "Pad");
        release(m, c);
        same("pad once", m);
        find(m, "Pad");
        destroy(m, "destroyed");
    }

    // Releasing what is not there: an empty name, which the map's Remove looks for, matching a free slot that has never held a name and moving the run after it; the same registry
    // twice; one with an empty name of its own; each under two fills of the stack.
    for (const unsigned char fill : {0xCD, 0x00, 0x41}) {
        g_scrub = fill;
        g_context = std::string("releasing what is not there, fill ") + std::to_string(fill);
        Registries regs;
        AtDataRegistry* a = regs.make("Design\\Registry\\Aimer.ADR");
        AtDataRegistry* b = regs.make("Design\\Registry\\Rotator.ADR");
        AtDataRegistry* c = regs.make("Design\\Registry\\Vehicle.ADR");
        AtDataRegistry* empty = regs.make("Design\\Registry\\");
        AtDataRegistry* never = regs.make("Design\\Registry\\Never.ADR");
        g_fill = 0xCD;
        Pair m = make(0xCD);
        init(m, 3);
        release(m, never);
        same("from an empty map", m);
        add(m, a);
        add(m, b);
        add(m, c);
        same("three", m);
        release(m, never);
        same("never added", m);
        release(m, b);
        same("released", m);
        release(m, b);
        same("released twice", m);
        add(m, empty);
        same("an empty name added", m);
        find(m, "Aimer");
        find(m, "Vehicle");
        release(m, empty);
        same("an empty name released", m);
        release(m, a);
        release(m, c);
        same("all released", m);
        destroy(m, "destroyed");
    }
    g_scrub = 0xCD;
}

// ---- the life cycle -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void run_life() {
    for (const int fill : {0xCD, 0x00}) {
        g_context = std::string("life cycle, fill ") + std::to_string(fill);
        g_fill = fill;
        Pair m = make(fill);
        same("made", m);
        // Init with no table, with one (slot 1 clears it first), and with 0.
        init(m, 20);
        same("init", m);
        Registries regs;
        AtDataRegistry* a = regs.make("Design\\Registry\\Animated.ADR");
        add(m, a);
        init(m, 20);
        same("init again", m);
        find(m, "Animated");
        add(m, a);
        init(m, 0);
        same("init 0", m);
        init(m, 40);
        same("init 40", m);
        add(m, a);
        // Clear through slot 1.
        { On on(0); at<Fn0>((*(uint32_t**)m.o)[1])(m.o, nullptr); }
        { On on(1); ((GmNameMap*)m.r)->Clear(); }
        same("slot 1", m);
        { On on(0); at<Fn0>((*(uint32_t**)m.o)[1])(m.o, nullptr); }
        { On on(1); ((GmNameMap*)m.r)->Clear(); }
        same("slot 1 on nothing", m);
        // A second map takes the instance; the first's destructor clears it anyway.
        Pair second = make(fill);
        same("second made", m, &second);
        init(second, 4);
        add(second, a);
        init(m, 4);
        slot0(m, 0);
        same("first destroyed, not freed", m, &second);
        eq("instance after the first", *(void**)(uintptr_t)kInstance == nullptr, GmRegistryNames::s_instance == nullptr);
        std::free(m.o);
        std::free(m.r);
        slot0(second, 1);
        compare_logs("second destroyed and freed");
        eq("instance after the second", *(void**)(uintptr_t)kInstance == nullptr, GmRegistryNames::s_instance == nullptr);
    }

    // No heap for the table: reported, and made anyway from what the push gives.
    for (int mode = 0; mode < 2; mode++) {
        g_context = mode ? "init with no memory" : "init with no heap";
        g_fill = 0xCD;
        Pair m = make(0xCD);
        if (mode == 0) {
            *(void**)(uintptr_t)0x002C632C = nullptr;
            GmNameMap::s_heap = nullptr;
        }
        g_fail_alloc = mode == 1;
        init(m, 7);
        g_fail_alloc = false;
        *(void**)(uintptr_t)0x002C632C = &g_heap;
        GmNameMap::s_heap = &g_heap;
        same("init", m);
        destroy(m, "destroyed");
    }
}

static LONG CALLBACK on_crash(EXCEPTION_POINTERS* e) {
    const DWORD c = e->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_INT_DIVIDE_BY_ZERO || c == EXCEPTION_STACK_OVERFLOW) {
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
    if (pos.size() != 2) { std::printf("usage: oracle_regnames <default.xbe> <cases dir> [--entries FILE] [--mutate]\n"); return 2; }
    if (!map_image(pos[0])) return 2;
    redirect(0x000111F0, (const void*)o_new);
    redirect(0x00011210, (const void*)o_delete);
    redirect(0x00011240, (const void*)o_push);
    redirect(0x00011260, (const void*)o_pop);
    redirect(0x00011070, (const void*)o_fatal);
    redirect(0x00011170, (const void*)o_raise);
    at<void(__cdecl*)()>(0x00125BC7)();
    if (entries && !arm_entries(entries)) return 2;
    *(void**)(uintptr_t)0x002C632C = &g_heap;
    GmNameMap::s_heap = &g_heap;
    recovered_vftable();
    recovered_base_vftable();
    *(void**)(uintptr_t)kInstance = nullptr;
    GmRegistryNames::s_instance = nullptr;

    if (g_mutate) eq("deliberate", 1, 2);  // Positive control of the harness.
    run_life();
    run_written();

    std::string dir = pos[1];
    FILE* index = nullptr;
    if (fopen_s(&index, (dir + "\\index.txt").c_str(), "r") || !index) { std::printf("cannot read index.txt\n"); return 2; }
    char line[1024];
    std::vector<std::string> disc;
    while (std::fgets(line, sizeof line, index)) {
        char* tab1 = std::strchr(line, '\t');
        char* tab2 = tab1 ? std::strchr(tab1 + 1, '\t') : nullptr;
        char* tab3 = tab2 ? std::strchr(tab2 + 1, '\t') : nullptr;
        if (!tab3) continue;
        *tab1 = *tab3 = 0;
        const std::string leaf = line, path = tab2 + 1;
        if (path.size() > 4 && path.compare(path.size() - 4, 4, ".ADR") == 0) {
            disc.push_back(path);
            g_adr++;
        }
        if (path.size() > 4 && path.compare(path.size() - 4, 4, ".MNG") == 0) {
            const std::string text = read_file(dir + "\\" + leaf);
            if (text.empty()) { std::printf("cannot read %s\n", leaf.c_str()); return 2; }
            const std::vector<std::string> paths = registry_paths(text);
            if (paths.empty()) continue;
            run_manifest(path, paths, 0);
            run_manifest(path, paths, 1);
            g_files++;
        }
    }
    std::fclose(index);
    run_adr(disc);
    report_entries();
    std::printf("%s: %u files, %u registries, %u .ADR paths, %u written names; %llu comparisons, %d differences\n", g_failures ? "DIFFERENT" : "NO DIFFERENCE OBSERVED", g_files,
                g_registries, g_adr, g_written, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
